// Copyright 2022 wevet works All Rights Reserved.

#include "AnimNode_CustomSpineSolver.h"
#include "QuadrupedIK.h"
#include "QuadrupedIKLibrary.h"
#include "PredictionAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "DrawDebugHelpers.h"
#include "AnimationRuntime.h"
#include "AnimationCoreLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
//#include "Kismet/KismetMathLibrary.h"
#include "CollisionQueryParams.h"
#include "Curves/CurveFloat.h"
#include "Algo/Reverse.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define ITERATION_COUNTER 50

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_CustomSpineSolver)

DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver Eval"), STAT_QuadrupedSpineSolver_Eval, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver EvalSKelControl"), STAT_QuadrupedSpineSolver_EvalSKelControl, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver Fabrik"), STAT_QuadrupedSpineSolver_Fabrik, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver LineTraceControll"), STAT_QuadrupedSpineSolver_LineTraceControll, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver TailImpactRotation"), STAT_QuadrupedSpineSolver_TailImpactRotation, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("QuadrupedSpineSolver ImpactRotation"), STAT_QuadrupedSpineSolver_ImpactRotation, STATGROUP_Anim);

//using namespace PreditctionDebug;

namespace SpineSolverHelper
{
	FTransform TLerp(const FTransform& A, const FTransform& B, float Alpha)
	{
		FTransform NA = A;
		FTransform NB = B;

		NA.NormalizeRotation();
		NB.NormalizeRotation();

		FTransform Result;
		Result.Blend(NA, NB, FMath::Clamp(Alpha, 0.0f, 1.0f));
		return Result;
	}

	const bool DoesContainsNaN(const TArray<FBoneTransform>& BoneTransforms)
	{
		for (int32 Index = 0; Index < BoneTransforms.Num(); ++Index)
		{
			if (BoneTransforms[Index].Transform.ContainsNaN())
			{
				return true;
			}
		}
		return false;
	}

	FRotator CustomLookRotation(const FVector& LookAt, const FVector& UpDirection)
	{
		FVector Forward = LookAt;
		FVector Up = UpDirection;
		Forward = Forward.GetSafeNormal();
		Up = Up - (Forward * FVector::DotProduct(Up, Forward));
		Up = Up.GetSafeNormal();

		FVector Position = Forward.GetSafeNormal();
		FVector Position2 = FVector::CrossProduct(Up, Position);
		FVector Position3 = FVector::CrossProduct(Position, Position2);
		const float M00 = Position2.X;
		const float M01 = Position2.Y;
		const float M02 = Position2.Z;
		const float M10 = Position3.X;
		const float M11 = Position3.Y;
		const float M12 = Position3.Z;
		const float M20 = Position.X;
		const float M21 = Position.Y;
		const float M22 = Position.Z;
		const float Num8 = (M00 + M11) + M22;
		FQuat Quaternion = FQuat();
		if (Num8 > 0.0f)
		{
			float Num = (float)FMath::Sqrt(Num8 + 1.0f);
			Quaternion.W = Num * 0.5f;
			Num = 0.5f / Num;
			Quaternion.X = (M12 - M21) * Num;
			Quaternion.Y = (M20 - M02) * Num;
			Quaternion.Z = (M01 - M10) * Num;
			return FRotator(Quaternion);
		}
		if ((M00 >= M11) && (M00 >= M22))
		{
			const float Num7 = (float)FMath::Sqrt(((1.0f + M00) - M11) - M22);
			const float Num4 = 0.5f / Num7;
			Quaternion.X = 0.5f * Num7;
			Quaternion.Y = (M01 + M10) * Num4;
			Quaternion.Z = (M02 + M20) * Num4;
			Quaternion.W = (M12 - M21) * Num4;
			return FRotator(Quaternion);
		}
		if (M11 > M22)
		{
			const float Num6 = (float)FMath::Sqrt(((1.0f + M11) - M00) - M22);
			const float Num3 = 0.5f / Num6;
			Quaternion.X = (M10 + M01) * Num3;
			Quaternion.Y = 0.5f * Num6;
			Quaternion.Z = (M21 + M12) * Num3;
			Quaternion.W = (M20 - M02) * Num3;
			return FRotator(Quaternion);
		}
		const float Num5 = (float)FMath::Sqrt(((1.0f + M22) - M00) - M11);
		const float Num2 = 0.5f / Num5;
		Quaternion.X = (M20 + M02) * Num2;
		Quaternion.Y = (M21 + M12) * Num2;
		Quaternion.Z = 0.5f * Num5;
		Quaternion.W = (M01 - M10) * Num2;
		return FRotator(Quaternion);
	}

	bool IsFiniteVector(const FVector& InVector)
	{
		return FMath::IsFinite(InVector.X) &&
			FMath::IsFinite(InVector.Y) &&
			FMath::IsFinite(InVector.Z);
	}
}

namespace QuadrupedSpineTraceGroup
{
	static constexpr int32 Between = 0;
	static constexpr int32 Foot = 1;
	static constexpr int32 ParentBase = 2;
	static constexpr int32 ParentFront = 3;
	static constexpr int32 ParentBack = 4;
	static constexpr int32 ParentLeft = 5;
	static constexpr int32 ParentRight = 6;
}

FAnimNode_CustomSpineSolver::FAnimNode_CustomSpineSolver()
{
	FRichCurve* PelvisHeightMultiplierCurveData = PelvisHeightMultiplierCurve.GetRichCurve();
	PelvisHeightMultiplierCurveData->AddKey(0.f, 1.f);
	PelvisHeightMultiplierCurveData->AddKey(600.f, 1.0f);
	
	FRichCurve* ChestHeightMultiplierCurveData = ChestHeightMultiplierCurve.GetRichCurve();
	ChestHeightMultiplierCurveData->AddKey(0.f, 1.0f);
	ChestHeightMultiplierCurveData->AddKey(600.f, 1.0f);

	FRichCurve* InterpolationMultiplierCurveData = InterpolationMultiplierCurve.GetRichCurve();
	InterpolationMultiplierCurveData->AddKey(0.f, 10.0f);
	InterpolationMultiplierCurveData->AddKey(1500.f, 10.0f);
}

void FAnimNode_CustomSpineSolver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (Context.AnimInstanceProxy)
	{
		owning_skel = Context.AnimInstanceProxy->GetSkelMeshComponent();
		IgnoreActors.Add(Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner());
	}

	if (!TraceSharedState.IsValid())
	{
		TraceSharedState = MakeShared<FQuadrupedIKTraceSharedState, ESPMode::ThreadSafe>();
	}

	bEffectorInitialized = false;
}

void FAnimNode_CustomSpineSolver::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const
{

#if WITH_EDITORONLY_DATA
	if (bIsTraceOptimization)
	{
		return;
	}

	if (bDisplayLineTrace && PreviewSkelMeshComp && PreviewSkelMeshComp->GetWorld())
	{
		const UWorld* World = PreviewSkelMeshComp->GetWorld();

		for (int32 Index = 0; Index < TraceStartList.Num(); Index++)
		{
			const float Owner_Scale = PreviewSkelMeshComp && PreviewSkelMeshComp->GetOwner() ? PreviewSkelMeshComp->GetComponentToWorld().GetScale3D().Z * VirtualScale : 1.0f;

			FVector Diff = (TraceStartList[Index] - TraceEndList[Index]);
			Diff.X = 0.0f;
			Diff.Y = 0.0f;
			const FVector Offset = FVector(0.0f, 0.0f, Diff.Z * 0.5f);

			switch (RaycastTraceType)
			{
			case EIKRaycastType::LineTrace:
				DrawDebugLine(World, TraceStartList[Index], TraceEndList[Index], TraceLinearColor[Index], false, 0.1f);
				break;

			case EIKRaycastType::SphereTrace:
				DrawDebugCapsule(
					World, TraceStartList[Index] - Offset,
					Diff.Size() * 0.5f + (TraceRadiusValue * Owner_Scale),
					TraceRadiusValue * Owner_Scale, FRotator(0, 0, 0).Quaternion(), TraceLinearColor[Index], false, 0.1f);
				break;

			case EIKRaycastType::BoxTrace:
				DrawDebugBox(
					World, TraceStartList[Index] - Offset,
					FVector(TraceRadiusValue * Owner_Scale, TraceRadiusValue * Owner_Scale, Diff.Size() * 0.5f), TraceLinearColor[Index], false, 0.1f);
				break;
			}
		}

	}
#endif

}

void FAnimNode_CustomSpineSolver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	bSolveShouldFail = false;
	bFeetIsEmpty = true;
	bWasSingleSpine = false;
	bInitializeAnimationArray = false;


	SolverBoneData.SpineBone = FBoneReference(SolverInputData.ChestBoneName);
	SolverBoneData.SpineBone.Initialize(RequiredBones);
	SolverBoneData.Pelvis = FBoneReference(SolverInputData.PelvisBoneName);
	SolverBoneData.Pelvis.Initialize(RequiredBones);

	StabilizationTailBoneRef.Initialize(RequiredBones);
	StabilizationHeadBoneRef.Initialize(RequiredBones);

	const FReferenceSkeleton& RefSkel = RequiredBones.GetReferenceSkeleton();
	RootBoneRef = FBoneReference(RefSkel.GetBoneName(0));
	RootBoneRef.Initialize(RequiredBones);


	if (!SolverBoneData.SpineBone.IsValidToEvaluate(RequiredBones) || !SolverBoneData.Pelvis.IsValidToEvaluate(RequiredBones))
	{
		bSolveShouldFail = true;
		UE_LOG(LogQuadrupedIK, Error, TEXT("[%s] Invalid pelvis or spine bone reference."), *FString(__FUNCTION__));
		return;
	}


	if (!RequiredBones.BoneIsChildOf(SolverBoneData.SpineBone.BoneIndex, SolverBoneData.Pelvis.BoneIndex))
	{
		bSolveShouldFail = true;
		UE_LOG(LogQuadrupedIK, Error, TEXT("[%s] Spine bone '%s' is not a child of pelvis bone '%s'."), *FString(__FUNCTION__), *SolverBoneData.SpineBone.BoneName.ToString(), *SolverBoneData.Pelvis.BoneName.ToString());
		return;
	}

	SpineFeetPair.Empty();
	TotalSpineNameArray.Empty();

	SpineTransformPairArray.Empty();
	SpineAnimTransformPairArray.Empty();
	SpineHitPairs.Empty();
	SpineRotationDiffArray.Empty();
	TotalSpineHeights.Empty();
	TotalSpineAlphaArray.Empty();
	SpineHitBetweenArray.Empty();
	SpineHitEdgeArray.Empty();
	SpineBetweenTransformArray.Empty();
	SpineBetweenHeightArray.Empty();
	CombinedIndiceArray.Empty();

	SolvedBoneTransformArray.Empty();
	SourcePoseBoneTransformArray.Empty();
	ReferencePoseBoneTransformArray.Empty();


	TotalSpineNameArray = BoneArrayMachine(RequiredBones, 0, SolverInputData.ChestBoneName, SolverInputData.PelvisBoneName, "", false);
	Algo::Reverse(TotalSpineNameArray);
	bSolveShouldFail = false;


	{
		TSet<FName> UniqueFeetNames;
		for (const FCustomBone_FootData& FootData : SolverInputData.FeetBones)
		{
			if (UniqueFeetNames.Contains(FootData.FeetBoneName))
			{
				bSolveShouldFail = true;
				UE_LOG(LogQuadrupedIK, Error, TEXT("[%s] Duplicate feet bone name detected: '%s'."), *FString(__FUNCTION__), *FootData.FeetBoneName.ToString());
			}
			else
			{
				UniqueFeetNames.Add(FootData.FeetBoneName);
			}
		}
	}


	for (int32 Index = 0; Index < SolverInputData.FeetBones.Num(); Index++)
	{
		const FCustomBone_FootData& FeetBoneData = SolverInputData.FeetBones[Index];
		BoneArrayMachine(RequiredBones, Index, FeetBoneData.FeetBoneName, SolverBoneData.Pelvis.BoneName, FeetBoneData.ThighBoneName, true);
	}

	SpineIndiceArray.Empty();
	const TArray<FQuadrupedBone_SpineFeetPair> K_FeetPair = SpineFeetPair;

	TotalSpineAlphaArray.AddDefaulted(K_FeetPair.Num());


	// @TODO
	// leg, hand jointの枝がない場合は削除する
	for (int32 Index = SpineFeetPair.Num() - 1; Index >= 0; --Index)
	{
		if (SpineFeetPair[Index].FeetArray.Num() == 0)
		{
			SpineFeetPair.RemoveAt(Index);
		}
	}
	SpineFeetPair.Shrink();


	if (SpineFeetPair.Num() == 1)
	{
		FQuadrupedBone_SpineFeetPair Instance;
		Instance.SpineBoneRef = FBoneReference(TotalSpineNameArray[TotalSpineNameArray.Num() - 1]);
		Instance.SpineBoneRef.Initialize(RequiredBones);
		SpineFeetPair.Add(Instance);
		bWasSingleSpine = true;
		bool bIsSwapped = false;

		do
		{
			bIsSwapped = false;
			for (int32 j = 0; j < SpineFeetPair.Num(); j++)
			{
				for (int32 i = 1; i < SpineFeetPair[j].FeetArray.Num(); i++)
				{
					if (SpineFeetPair[j].FeetArray[i - 1].BoneIndex < SpineFeetPair[j].FeetArray[i].BoneIndex)
					{
						FBoneReference BoneRef = SpineFeetPair[j].FeetArray[i - 1];
						SpineFeetPair[j].FeetArray[i - 1] = SpineFeetPair[j].FeetArray[i];
						SpineFeetPair[j].FeetArray[i] = BoneRef;
						bIsSwapped = true;
					}
				}
			}
		} while (bIsSwapped);
	}
	else
	{
		bWasSingleSpine = false;
		SpineFeetPair = Swap_SpineFeetPairArray(SpineFeetPair);
	}

	if (SpineFeetPair.IsEmpty())
	{
		if ((bSpineSnakeBone || SolverBoneData.FeetBones.IsEmpty()) &&
			SolverBoneData.Pelvis.IsValidToEvaluate() && SolverBoneData.SpineBone.IsValidToEvaluate())
		{
			SpineFeetPair.AddDefaulted(2);
			SpineFeetPair[0].SpineBoneRef = SolverBoneData.Pelvis;

			if (SpineFeetPair.Num() > 1)
			{
				SpineFeetPair[SpineFeetPair.Num() - 1].SpineBoneRef = SolverBoneData.SpineBone;
			}
		}

	}


	TotalSpineHeights.AddDefaulted(SpineFeetPair.Num());
	SpineHitPairs.AddDefaulted(SpineFeetPair.Num());
	SpineTransformPairArray.AddDefaulted(SpineFeetPair.Num());
	SpineAnimTransformPairArray.AddDefaulted(SpineFeetPair.Num());
	SpineRotationDiffArray.AddDefaulted(SpineFeetPair.Num());

	for (int32 Index = 0; Index < SpineFeetPair.Num(); ++Index)
	{
		FQuadrupedBone_SpineFeetPair& Pair = SpineFeetPair[Index];
		const int32 FeetNum = Pair.FeetArray.Num();

		Pair.KneeArray.SetNum(FeetNum);
		Pair.ThighArray.SetNum(FeetNum);

		// Non-snake creature with odd number of feet on one segment => invalid
		if ((FeetNum % 2) != 0)
		{
			if (!bSpineSnakeBone && SolverBoneData.FeetBones.Num() != 0)
			{
				bSolveShouldFail = true;
				UE_LOG(LogQuadrupedIK, Warning, TEXT("[%s] Odd number of feet detected on spine segment %d."), *FString(__FUNCTION__), Index);
			}
		}

		for (int32 FootIdx = 0; FootIdx < FeetNum; ++FootIdx)
		{
			const FBoneReference& FootRef = Pair.FeetArray[FootIdx];
			const int32 FootBoneIndex = FootRef.BoneIndex;

			if (FootBoneIndex == INDEX_NONE)
			{
				continue;
			}

			// Knee = parent of foot
			const int32 KneeBoneIndex = RefSkel.GetParentIndex(FootBoneIndex);
			if (KneeBoneIndex != INDEX_NONE)
			{
				FBoneReference KneeRef(RefSkel.GetBoneName(KneeBoneIndex));
				KneeRef.Initialize(RequiredBones);
				Pair.KneeArray[FootIdx] = KneeRef;

				// Thigh = parent of knee (fallback only)
				if (!Pair.ThighArray[FootIdx].IsValidToEvaluate(RequiredBones))
				{
					const int32 ThighBoneIndex = RefSkel.GetParentIndex(KneeBoneIndex);
					if (ThighBoneIndex != INDEX_NONE)
					{
						FBoneReference ThighRef(RefSkel.GetBoneName(ThighBoneIndex));
						ThighRef.Initialize(RequiredBones);
						Pair.ThighArray[FootIdx] = ThighRef;
					}
				}
			}
		}

		Pair.FeetHeightArray.AddDefaulted(FeetNum);
		SpineHitPairs[Index].FeetHitArray.AddDefaulted(FeetNum);
		SpineHitPairs[Index].FeetHitPointArray.AddDefaulted(FeetNum);
		SpineTransformPairArray[Index].AssociatedFootArray.AddDefaulted(FeetNum);
		SpineTransformPairArray[Index].AssociatedKneeArray.AddDefaulted(FeetNum);
		SpineTransformPairArray[Index].AssociatedToeArray.AddDefaulted(FeetNum);
		SpineAnimTransformPairArray[Index].AssociatedFootArray.AddDefaulted(FeetNum);
		SpineAnimTransformPairArray[Index].AssociatedKneeArray.AddDefaulted(FeetNum);
		SpineAnimTransformPairArray[Index].AssociatedToeArray.AddDefaulted(FeetNum);


		if (FeetNum > 0)
		{
			bFeetIsEmpty = false;
		}
	}

	bool bIsBipedJoint = false;
	if (SpineFeetPair.Num() >= 2)
	{
		bool bHasChestLegs = false;
		for (int32 i = 1; i < SpineFeetPair.Num(); ++i)
		{
			if (SpineFeetPair[i].FeetArray.Num() > 0)
			{
				bHasChestLegs = true;
				break;
			}
		}

		bIsBipedJoint = (SpineFeetPair[0].FeetArray.Num() > 0 && !bHasChestLegs);
	}


	if (bSpineSnakeBone)
	{
		FabrikType = EFabrikType::Snake;
	}
	else if (!bIsBipedJoint)
	{
		FabrikType = EFabrikType::Animal;
	}
	else
	{
		FabrikType = EFabrikType::Humanoid;
	}


	// Build spine indices
	for (const FName& SpineBoneName : TotalSpineNameArray)
	{
		FBoneReference BoneRef(SpineBoneName);
		BoneRef.Initialize(RequiredBones);

		if (!BoneRef.IsValidToEvaluate(RequiredBones))
		{
			continue;
		}

		const FCompactPoseBoneIndex CompactIndex = BoneRef.GetCompactPoseIndex(RequiredBones);
		if (!CompactIndex.IsValid())
		{
			continue;
		}

		if (SpineFeetPair.Num() > 0)
		{
			if (BoneRef.BoneIndex >= SpineFeetPair[0].SpineBoneRef.BoneIndex && BoneRef.BoneIndex <= SpineFeetPair.Last().SpineBoneRef.BoneIndex)
			{
				SpineIndiceArray.Add(CompactIndex);
			}
			else
			{
				ExtraSpineIndiceArray.Add(CompactIndex);
			}
		}
		else
		{
			SpineIndiceArray.Add(CompactIndex);
		}
	}



	if (SpineFeetPair.Num() > 1 && !bSolveShouldFail)
	{
		TArray<FCompactPoseBoneIndex> BoneIndices;

		const FCompactPoseBoneIndex RootIndex = SpineFeetPair[0].SpineBoneRef.GetCompactPoseIndex(RequiredBones);
		FCompactPoseBoneIndex BoneIndex = SpineFeetPair[SpineFeetPair.Num() - 1].SpineBoneRef.GetCompactPoseIndex(RequiredBones);

		BoneIndices.Reserve(RequiredBones.GetNumBones());

		// Tip→Root で積んで、最後に反転して Root→Tip に並べる
		for (int32 Step = 0; Step < RequiredBones.GetNumBones(); ++Step)
		{
			BoneIndices.Add(BoneIndex);
			if (BoneIndex == RootIndex)
			{
				Algo::Reverse(BoneIndices); // Root→Tip に並べ替え
				break;
			}

			const FCompactPoseBoneIndex Parent = RequiredBones.GetParentBoneIndex(BoneIndex);
			if (!Parent.IsValid())
			{
				// Rootに辿れない（別ツリー等）→失敗
				BoneIndices.Reset();
				break;
			}
			BoneIndex = Parent;
		}

		if (BoneIndices.Num() == 0 || BoneIndices[0] != RootIndex)
		{
			bSolveShouldFail = true;
			UE_LOG(LogTemp, Error, TEXT("[%s] : BoneIndices is Empty"), *FString(__FUNCTION__));
		}
		else
		{
			CombinedIndiceArray = MoveTemp(BoneIndices);

			{
				const int32 Between = FMath::Max(CombinedIndiceArray.Num() - 2, 0);
				SpineBetweenTransformArray.AddDefaulted(Between);
				SpineBetweenOffsetTransformArray.AddDefaulted(Between);
				SpineBetweenHeightArray.AddDefaulted(Between);
				SnakeSpinePositionArray.AddDefaulted(Between);
			}

			TotalSpineAlphaArray.AddDefaulted(CombinedIndiceArray.Num());
		}

	}

	SourcePoseBoneTransformArray.AddDefaulted(CombinedIndiceArray.Num());
	SolvedBoneTransformArray.AddDefaulted(CombinedIndiceArray.Num());
	ReferencePoseBoneTransformArray.AddDefaulted(CombinedIndiceArray.Num());
	//PrevResolvedSpineTransforms.AddDefaulted(CombinedIndiceArray.Num());

	for (int32 SIndex = 0; SIndex < SnakeSpinePositionArray.Num(); SIndex++)
	{
		SnakeSpinePositionArray[SIndex] = FVector::ZeroVector;
	}

	SpineHitBetweenArray.AddDefaulted(SpineBetweenTransformArray.Num());
	SpinePointBetweenArray.AddDefaulted(SpineBetweenTransformArray.Num());

	SolverBoneData.FeetBones.Empty();
	for (const FCustomBone_FootData& FootData : SolverInputData.FeetBones)
	{
		FBoneReference FootRef(FootData.FeetBoneName);
		FootRef.Initialize(RequiredBones);
		SolverBoneData.FeetBones.Add(FootRef);

		if (FootRef.IsValidToEvaluate(RequiredBones))
		{
			bFeetIsEmpty = false;
		}
	}
}

void FAnimNode_CustomSpineSolver::UpdateInternal(const FAnimationUpdateContext& Context)
{
	CachedDeltaSeconds = Context.GetDeltaTime();

	const bool bIsLODEnabled = IsLODEnabled(Context.AnimInstanceProxy);

	const bool bIsValid = bIsLODEnabled &&
		 bEnableSolver &&
		FAnimWeight::IsRelevant(ActualAlpha) &&
		IsValidToEvaluate(Context.AnimInstanceProxy->GetSkeleton(), Context.AnimInstanceProxy->GetRequiredBones());

	bRequireSnap = (PreviousAlpha <= 0.f && ActualAlpha > 0.f);
	PreviousAlpha = ActualAlpha;

	if (!bEnableSolver)
	{
		ResetTraceResults();
		return;
	}


	TraceStartList.Empty();
	TraceEndList.Empty();
	TraceLinearColor.Empty();

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();
	const AActor* Owner = SK->GetOwner();

	ComponentScale = ComponentToWorld.GetScale3D().Z * VirtualScale;

	if (!FMath::IsFinite(ComponentScale) || FMath::IsNearlyZero(ComponentScale))
	{
		ResetTraceResults();
		return;
	}

	const float TraceUpperHeight = (LineTraceUpperHeight * ComponentScale);
	const float TraceDownwardHeight = (LineTraceDownwardHeight * ComponentScale);

	const FVector UpVector = ComponentToWorld.TransformVector(CharacterDirectionVectorCS);
	const FVector ForwardDir = ComponentToWorld.TransformVector(ForwardDirectionVector).GetSafeNormal();
	const FVector RightDir = FVector::CrossProduct(UpVector, ForwardDir).GetSafeNormal();

	//const float MaxSpeed = 100.0f;
	//ShiftSpeed = FMath::Clamp(ShiftSpeed, 0.0f, MaxSpeed);
	//ComponentScale = ComponentToWorld.GetScale3D().Z * VirtualScale;

	if (IsValid(Owner))
	{
		CharacterSpeed = Owner->GetVelocity().Size2D();
	}

	MaxFormatedHeight = PelvisHeightMultiplierCurve.GetRichCurve()->Eval(CharacterSpeed) * MaxDipHeight;
	MaxFormatedDipHeightChest = ChestHeightMultiplierCurve.GetRichCurve()->Eval(CharacterSpeed) * MaxDipHeightChest;

	if (bIsUseManualLocationLerpSpeed)
	{
		FormatLocationLerp = FMath::GetMappedRangeValueClamped(PawnSpeedRange, LocationLerpSpeedRange, CharacterSpeed);
		//FormatLocationLerp = LocationLerpSpeed;
		//UE_LOG(LogQuadrupedIK, Log, TEXT("FormatLocationLerp : %.3f"), FormatLocationLerp);
	}
	else
	{
		FormatLocationLerp = LocationLerpSpeed * InterpolationMultiplierCurve.GetRichCurve()->Eval(CharacterSpeed);
	}

	FormatRotationLerp = RotationLerpSpeed * InterpolationMultiplierCurve.GetRichCurve()->Eval(CharacterSpeed);


	if (UPredictionAnimInstance* AnimInst = Cast<UPredictionAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		//PelvisBaseOffset = AnimInst->GetPelvisFinalOffset();
	}

	if (bSpineSnakeBone)
	{
		if (!SpinePointBetweenArray.IsEmpty() && !SpineHitBetweenArray.IsEmpty() && !SpineBetweenTransformArray.IsEmpty())
		{
			for (int32 i = 0; i < SpineHitBetweenArray.Num(); i++)
			{
				const FVector OriginPoint = SpineBetweenTransformArray[i];
				const FVector OriginUpper = OriginPoint + UpVector * TraceUpperHeight;
				const FVector OriginDown = OriginPoint - UpVector * TraceDownwardHeight;

				ApplyLineTrace(Context, OriginPoint, OriginUpper, OriginDown, SpineHitBetweenArray[i], FLinearColor::Green, true);
				CalcParentHitResult(Context, SpineHitBetweenArray[i], OriginPoint, SpinePointBetweenArray[i]);
			}
		}
	}


	for (int32 i = 0; i < SpineHitPairs.Num(); i++)
	{

		FQuadlupedBoneHitPairs& HitPair = SpineHitPairs[i];

		FVector FeetCenterPoint = FVector::ZeroVector;
		for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
		{
			const TArray<FTransform>& AssociatedFootTransform = SpineAnimTransformPairArray[i].AssociatedFootArray;
			const FVector FeetOrigin = AssociatedFootTransform[j].GetLocation();
			FeetCenterPoint += FeetOrigin;

			// feet hit
			ApplyLineTrace(Context, FeetOrigin,
				FeetOrigin + UpVector * TraceUpperHeight,
				FeetOrigin - UpVector * TraceDownwardHeight,
				SpineHitPairs[i].FeetHitArray[j], FLinearColor::Blue, false);
			CalcParentHitResult(Context, HitPair.FeetHitArray[j], FeetOrigin, HitPair.FeetHitPointArray[j]);
		}

		FeetCenterPoint /= SpineAnimTransformPairArray[i].AssociatedFootArray.Num();
		if (bSpineSnakeBone || SolverBoneData.FeetBones.IsEmpty())
		{
			FeetCenterPoint = SpineAnimTransformPairArray[i].SpineInvolved.GetLocation();
		}

		const FVector SpiralFrontUpper = FeetCenterPoint + UpVector * TraceUpperHeight + (ForwardDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralBackUpper = FeetCenterPoint + UpVector * TraceUpperHeight - (ForwardDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralLeftUpper = FeetCenterPoint + UpVector * TraceUpperHeight + (RightDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralRightUpper = FeetCenterPoint + UpVector * TraceUpperHeight - (RightDir * ComponentScale * VirtualLegWidth);

		const FVector SpiralFrontDown = FeetCenterPoint - UpVector * TraceDownwardHeight + (ForwardDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralBackDown = FeetCenterPoint - UpVector * TraceDownwardHeight - (ForwardDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralLeftDown = FeetCenterPoint - UpVector * TraceDownwardHeight + (RightDir * ComponentScale * VirtualLegWidth);
		const FVector SpiralRightDown = FeetCenterPoint - UpVector * TraceDownwardHeight - (RightDir * ComponentScale * VirtualLegWidth);

		const FVector OriginUpper = FeetCenterPoint + UpVector * TraceUpperHeight;
		const FVector OriginDown = FeetCenterPoint - UpVector * TraceDownwardHeight;

		if (bIsTraceOptimization)
		{
			ApplyMultiPointTraceBulk(Context, FeetCenterPoint, OriginUpper, OriginDown, ForwardDir, RightDir, HitPair);
		}
		else
		{

#if false
			// base
			ApplyLineTrace(Context, FeetCenterPoint, OriginUpper, OriginDown, HitPair.ParentSpineHit, FLinearColor::Green, true);
			// front
			ApplyLineTrace(Context, FeetCenterPoint, SpiralFrontUpper, SpiralFrontDown, HitPair.ParentFrontHit, FLinearColor::Yellow, true);
			// back
			ApplyLineTrace(Context, FeetCenterPoint, SpiralBackUpper, SpiralBackDown, HitPair.ParentBackHit, FLinearColor::Black, true);
			// left
			ApplyLineTrace(Context, FeetCenterPoint, SpiralLeftUpper, SpiralLeftDown, HitPair.ParentLeftHit, FLinearColor::Blue, true);
			// right
			ApplyLineTrace(Context, FeetCenterPoint, SpiralRightUpper, SpiralRightDown, HitPair.ParentRightHit, FLinearColor::Red, true);
#endif

			ApplyLineTraceCached(
				Context, 
				MakeTraceKey(QuadrupedSpineTraceGroup::ParentBase, i, 0), 
				FeetCenterPoint, OriginUpper, OriginDown, HitPair.ParentSpineHit, FLinearColor::Green, true);

			ApplyLineTraceCached(
				Context,
				MakeTraceKey(QuadrupedSpineTraceGroup::ParentFront, i, 0),
				FeetCenterPoint, SpiralFrontUpper, SpiralFrontDown, HitPair.ParentFrontHit, FLinearColor::Yellow, true);

			ApplyLineTraceCached(
				Context,
				MakeTraceKey(QuadrupedSpineTraceGroup::ParentBack, i, 0),
				FeetCenterPoint, SpiralBackUpper, SpiralBackDown, HitPair.ParentBackHit, FLinearColor::Black, true);

			ApplyLineTraceCached(
				Context,
				MakeTraceKey(QuadrupedSpineTraceGroup::ParentLeft, i, 0),
				FeetCenterPoint, SpiralLeftUpper, SpiralLeftDown, HitPair.ParentLeftHit, FLinearColor::Blue, true);

			ApplyLineTraceCached(
				Context,
				MakeTraceKey(QuadrupedSpineTraceGroup::ParentRight, i, 0),
				FeetCenterPoint, SpiralRightUpper, SpiralRightDown, HitPair.ParentRightHit, FLinearColor::Red, true);
		}

		CalcParentHitResult(Context, HitPair.ParentSpineHit, FeetCenterPoint, HitPair.ParentSpinePoint);
		CalcParentHitResult(Context, HitPair.ParentLeftHit, SpiralLeftUpper, HitPair.ParentLeftPoint);
		CalcParentHitResult(Context, HitPair.ParentRightHit, SpiralRightUpper, HitPair.ParentRightPoint);
		CalcParentHitResult(Context, HitPair.ParentFrontHit, SpiralFrontUpper, HitPair.ParentFrontPoint);
		CalcParentHitResult(Context, HitPair.ParentBackHit, SpiralBackUpper, HitPair.ParentBackPoint);


	}

}

void FAnimNode_CustomSpineSolver::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_QuadrupedSpineSolver_Eval);
	check(OutBoneTransforms.Num() == 0);


	// only update effector transform
	if (bRequireSnap)
	{
		//PrepareAnimatedPoseInfo_AnyThread(Output);
		//InitializeEffectorTransform(Output.Pose);
		//const int32 NumTransforms = CombinedIndiceArray.Num();
		//for (int32 i = 0; i < NumTransforms; ++i)
		//{
		//	const FCompactPoseBoneIndex BoneIndex = CombinedIndiceArray[i];
		//	const FTransform CurrentTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);

		//	if (SourcePoseBoneTransformArray.IsValidIndex(i))
		//	{
		//		SourcePoseBoneTransformArray[i] = FBoneTransform(BoneIndex, CurrentTransform);
		//	}

		//	if (ReferencePoseBoneTransformArray.IsValidIndex(i))
		//	{
		//		ReferencePoseBoneTransformArray[i] = FBoneTransform(BoneIndex, CurrentTransform);
		//	}

		//	if (SolvedBoneTransformArray.IsValidIndex(i))
		//	{
		//		SolvedBoneTransformArray[i] = FBoneTransform(BoneIndex, CurrentTransform);
		//	}

		//	if (TotalSpineAlphaArray.IsValidIndex(i))
		//	{
		//		TotalSpineAlphaArray[i] = 0.0f;
		//	}
		//}
		//bRequireSnap = false;
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());
		return;
	}

	const auto Scale = Output.AnimInstanceProxy->GetActorTransform().GetScale3D();
	const bool bBothBoneValid = IsValidToEvaluate(Output.AnimInstanceProxy->GetSkeleton(), Output.AnimInstanceProxy->GetRequiredBones());

	const bool bIsValid = bEnableSolver &&
		!Scale.IsNearlyZero() &&
		!SpineFeetPair.IsEmpty() &&
		!SpineHitPairs.IsEmpty() &&
		FAnimWeight::IsRelevant(ActualAlpha) &&
		bBothBoneValid &&
		!Output.ContainsNaN();

	if (!bIsValid)
	{
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(CustomSpineSolver_EvaluateSkeletalControl_AnyThread);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector& ComponentLocation = ComponentToWorld.GetLocation();
	const float MinThreshold = -MinFeetDistance * ComponentScale;
	const float MaxThreshold = MaxFeetDistance * ComponentScale;

	auto IsValidHit = [&](const FHitResult& Hit)
		{
			return Hit.bBlockingHit && (Hit.ImpactPoint.Z - ComponentLocation.Z) > MinThreshold;
		};

	auto IsValidAbsHit = [&](const FHitResult& Hit)
		{
			return Hit.bBlockingHit && (FMath::Abs(Hit.ImpactPoint.Z - ComponentLocation.Z)) < MaxThreshold;
		};

	//bAtleastOneHit = false;

	// SpineSnakeBone
	if (bSpineSnakeBone)
	{
		for (int32 Index = 0; Index < SpineHitBetweenArray.Num(); Index++)
		{
			if (IsValidAbsHit(SpineHitBetweenArray[Index]))
			{
				bAtleastOneHit = true;
			}
		}
	}

	// check true
	for (int32 KIndex = 0; KIndex < SpineHitPairs.Num(); KIndex++)
	{
		const FQuadlupedBoneHitPairs& Pair = SpineHitPairs[KIndex];

		if (IsValidHit(Pair.ParentSpineHit))
		{
			bAtleastOneHit = true;
			break;
		}
	}

	//for (int32 i = 0; i < SpineHitPairs.Num(); i++)
	//{
	//	const FQuadlupedBoneHitPairs& Pair = SpineHitPairs[i];
	//	for (int32 j = 0; j < SpineHitPairs[i].FeetHitArray.Num(); j++)
	//	{
	//		const bool bDistanceOver = (FMath::Abs(Pair.FeetHitArray[j].ImpactPoint.Z - (ComponentLocation.Z)) > MaxThreshold);
	//		if ((bDistanceOver && Pair.FeetHitArray[j].bBlockingHit) || !Pair.FeetHitArray[j].bBlockingHit)
	//		{
	//			bAtleastOneHit = false;
	//		}
	//	}
	//}


	if (FabrikType == EFabrikType::Animal)
	{
		auto GetSupportWidth = [&](const FQuadlupedBoneHitPairs& Pair) -> float
			{
				if (Pair.ParentLeftHit.bBlockingHit && Pair.ParentRightHit.bBlockingHit)
				{
					const FVector L = ComponentToWorld.InverseTransformPosition(Pair.ParentLeftPoint);
					const FVector R = ComponentToWorld.InverseTransformPosition(Pair.ParentRightPoint);
					return FVector::Dist2D(L, R);
				}

				// left/right が無い場合は feet から代用
				if (Pair.FeetHitPointArray.Num() >= 2)
				{
					float MaxWidth = 0.0f;
					for (int32 i = 0; i < Pair.FeetHitPointArray.Num(); ++i)
					{
						for (int32 j = i + 1; j < Pair.FeetHitPointArray.Num(); ++j)
						{
							const FVector A = ComponentToWorld.InverseTransformPosition(Pair.FeetHitPointArray[i]);
							const FVector B = ComponentToWorld.InverseTransformPosition(Pair.FeetHitPointArray[j]);
							MaxWidth = FMath::Max(MaxWidth, FVector::Dist2D(A, B));
						}
					}
					return MaxWidth;
				}

				return 0.0f;
			};

		auto AddHitIfBlocking = [&](TArray<FVector>& OutPointsCS, const FHitResult& Hit)
			{
				if (Hit.bBlockingHit)
				{
					OutPointsCS.Add(ComponentToWorld.InverseTransformPosition(Hit.ImpactPoint));
				}
			};

		auto GatherCandidateHitPointsCS = [&](TArray<FVector>& OutPointsCS)
			{
				for (const FQuadlupedBoneHitPairs& Pair : SpineHitPairs)
				{
					AddHitIfBlocking(OutPointsCS, Pair.ParentSpineHit);
					AddHitIfBlocking(OutPointsCS, Pair.ParentFrontHit);
					AddHitIfBlocking(OutPointsCS, Pair.ParentBackHit);
					AddHitIfBlocking(OutPointsCS, Pair.ParentLeftHit);
					AddHitIfBlocking(OutPointsCS, Pair.ParentRightHit);

					for (const FHitResult& FootHit : Pair.FeetHitArray)
					{
						AddHitIfBlocking(OutPointsCS, FootHit);
					}
				}

				for (const FHitResult& BetweenHit : SpineHitBetweenArray)
				{
					AddHitIfBlocking(OutPointsCS, BetweenHit);
				}
			};

		const int32 LastIndex = (SpineHitPairs.Num() - 1);
		const auto& PelvisPair = SpineHitPairs[0];
		const auto& ChestPair = SpineHitPairs[LastIndex];

		if (bAtleastOneHit)
		{
			const FVector P1 = ComponentToWorld.InverseTransformPosition(PelvisPair.ParentSpineHit.ImpactPoint);
			const FVector P2 = ComponentToWorld.InverseTransformPosition(ChestPair.ParentSpineHit.ImpactPoint);

			const FVector ImpactDelta = (P2 - P1);
			const float ImpactHorizontal = FMath::Max(FVector2D(ImpactDelta.X, ImpactDelta.Y).Size(), 1e-3f);
			const float ImpactVertical = FMath::Abs(ImpactDelta.Z);
			const float ImpactDistance = ImpactDelta.Size();
			const float SpineLengthRef = ImpactDistance;

			const float MaxAnimalSpineSlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(NormalDotThreshold, -1.0f, 1.0f)));
			const float ImpactSlopeUp = FMath::RadiansToDegrees(FMath::Atan2(ImpactDelta.Z, ImpactHorizontal));

			const float CurMaxAnimalSpineGapCm = MaxAnimalSpineGapCm * ComponentScale;
			const float CurMaxAnimalVerticalGapCm = MaxAnimalVerticalGapCm * ComponentScale;

			const float PelvisWidth = GetSupportWidth(PelvisPair);
			const float ChestWidth = GetSupportWidth(ChestPair);

			const float MinRequiredWidth = SpineLengthRef * MinSupportWidthRatioToSpineLength;
			const bool bPelvisWidthValid = (PelvisWidth >= MinRequiredWidth);
			const bool bChestWidthValid = (ChestWidth >= MinRequiredWidth);
			const bool bWidthInvalid = bEnableSupportWidthValidator && (!bPelvisWidthValid || !bChestWidthValid);

			TArray<FVector> CandidateHitPointsCS;
			GatherCandidateHitPointsCS(CandidateHitPointsCS);

			float MaxProjectedVerticalResidual = 0.0f;
			if (CandidateHitPointsCS.Num() > 0)
			{
				const float SpineLenSq = FMath::Max(ImpactDelta.SizeSquared(), KINDA_SMALL_NUMBER);

				for (const FVector& CandidateCS : CandidateHitPointsCS)
				{
					const FVector FromPelvis = CandidateCS - P1;
					const float AlphaOnSpine = FMath::Clamp(FVector::DotProduct(FromPelvis, ImpactDelta) / SpineLenSq, 0.0f, 1.0f);
					const FVector ProjectedPointOnSpine = P1 + ImpactDelta * AlphaOnSpine;
					const float VerticalResidual = FMath::Abs(CandidateCS.Z - ProjectedPointOnSpine.Z);
					MaxProjectedVerticalResidual = FMath::Max(MaxProjectedVerticalResidual, VerticalResidual);
				}
			}

			const bool bOver = (ImpactSlopeUp > MaxAnimalSpineSlopeAngle)
				|| (ImpactDistance > CurMaxAnimalSpineGapCm)
				|| (ImpactVertical > CurMaxAnimalVerticalGapCm)
				|| (MaxProjectedVerticalResidual > CurMaxAnimalVerticalGapCm)
				|| bWidthInvalid;

			if (bOver)
			{
				const bool bContinuousSlope = IsContinuousSlope(Output, MidSlopeResidualRelaxedThreshold);

				if (!bContinuousSlope)
				{
					if (bDisplayLineTrace)
					{
						if (bWidthInvalid)
						{
							UE_LOG(LogQuadrupedIK, Warning,
								TEXT("[%s] : SupportWidth invalid. PelvisWidth=%.2f ChestWidth=%.2f Required=%.2f SpineLen=%.2f"),
								*FString(__FUNCTION__),
								PelvisWidth,
								ChestWidth,
								MinRequiredWidth,
								SpineLengthRef);
						}

						if (MaxProjectedVerticalResidual > CurMaxAnimalVerticalGapCm)
						{
							UE_LOG(LogQuadrupedIK, Warning,
								TEXT("[%s] : Animal support residual invalid. MaxResidual=%.2f Limit=%.2f Samples=%d"),
								*FString(__FUNCTION__),
								MaxProjectedVerticalResidual,
								CurMaxAnimalVerticalGapCm,
								CandidateHitPointsCS.Num());
						}
					}

					bAtleastOneHit = false;
				}
			}
		}

	}


	if (UPredictionAnimInstance* AnimInst = Cast<UPredictionAnimInstance>(Output.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		//if (!AnimInst->IsValidGrounded())
		{
			//bAtleastOneHit = false;
		}
	}

	if (!CombinedIndiceArray.IsEmpty() && !SpineBetweenTransformArray.IsEmpty())
	{
		for (int32 i = 1; i < CombinedIndiceArray.Num() - 1; i++)
		{
			const int32 PrevIndex = (i - 1);
			const FCompactPoseBoneIndex PoseIndex = CombinedIndiceArray[i];
			const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(PoseIndex);
			SpineBetweenTransformArray[PrevIndex] = ((BoneCSTransform)*ComponentToWorld).GetLocation();

			FTransform RootTraceTransform = FTransform::Identity;
			FAnimationRuntime::ConvertCSTransformToBoneSpace(
				ComponentToWorld,
				Output.Pose,
				RootTraceTransform,
				PoseIndex,
				EBoneControlSpace::BCS_WorldSpace);

			const float ChestDistance = FMath::Abs(SpineBetweenTransformArray[PrevIndex].Z - RootTraceTransform.GetLocation().Z);
			SpineBetweenHeightArray[PrevIndex] = ChestDistance;
		}
	}

	if (!SpineAnimTransformPairArray.IsEmpty() && !SpineFeetPair.IsEmpty())
	{
		for (int32 i = 0; i < SpineFeetPair.Num(); i++)
		{
			const FQuadrupedBone_SpineFeetPair& BoneRef = SpineFeetPair[i];

			const FCompactPoseBoneIndex SpineBoneIndex = BoneRef.SpineBoneRef.GetCompactPoseIndex(BoneContainer);
			const FTransform SpineBoneIndexCS = Output.Pose.GetComponentSpaceTransform(SpineBoneIndex);

			SpineAnimTransformPairArray[i].SpineInvolved = (SpineBoneIndexCS)*ComponentToWorld;
			SpineAnimTransformPairArray[i].SpineInvolved.SetRotation(ComponentToWorld.GetRotation() * SpineBoneIndexCS.GetRotation());

			FQuadlupedBoneSpineFeetPair_WS& TransformPairWS = SpineAnimTransformPairArray[i];

			for (int32 j = 0; j < SpineAnimTransformPairArray[i].AssociatedFootArray.Num(); j++)
			{
				if (BoneRef.FeetArray.IsValidIndex(j))
				{
					const FCompactPoseBoneIndex& FeetBoneIndex = BoneRef.FeetArray[j].GetCompactPoseIndex(BoneContainer);
					const FTransform& FeetBoneIndexCS = Output.Pose.GetComponentSpaceTransform(FeetBoneIndex);
					TransformPairWS.AssociatedFootArray[j] = (FeetBoneIndexCS)*ComponentToWorld;
					TransformPairWS.AssociatedFootArray[j].AddToTranslation(ComponentToWorld.TransformVectorNoScale(BoneRef.FeetTraceOffsetArray[j]));
				}
			}

			for (int32 j = 0; j < SpineAnimTransformPairArray[i].AssociatedKneeArray.Num(); j++)
			{
				if (BoneRef.KneeArray.IsValidIndex(j))
				{
					const FCompactPoseBoneIndex& KneeBoneIndex = BoneRef.KneeArray[j].GetCompactPoseIndex(BoneContainer);
					const FTransform& KneeBoneIndexCS = Output.Pose.GetComponentSpaceTransform(KneeBoneIndex);
					TransformPairWS.AssociatedKneeArray[j] = (KneeBoneIndexCS)*ComponentToWorld;
				}
			}

			const FTransform& BoneTraceTransform = Output.Pose.GetComponentSpaceTransform(BoneRef.SpineBoneRef.GetCompactPoseIndex(BoneContainer));
			const FCompactPoseBoneIndex RootIdx = RootBoneRef.GetCompactPoseIndex(BoneContainer);
			const FTransform RootCST = Output.Pose.GetComponentSpaceTransform(RootIdx);
			const float ChestDistance = FMath::Abs(BoneTraceTransform.GetLocation().Z - RootCST.GetLocation().Z);
			TotalSpineHeights[i] = ChestDistance;

		}
	}

	PrepareAnimatedPoseInfo_AnyThread(Output);
	InitializeEffectorTransform(Output.Pose);

	SolveSpineIK(Output, Output.Pose, OutBoneTransforms);
	ComponentPose.EvaluateComponentSpace(Output);
	ResolveSpineIK(Output, Output.Pose, OutBoneTransforms);

	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
	bRequireSnap = false;
}

void FAnimNode_CustomSpineSolver::PrepareAnimatedPoseInfo_AnyThread(FComponentSpacePoseContext& Output)
{
	LineTraceInitialized = false;

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	for (int32 i = 0; i < SpineFeetPair.Num(); i++)
	{
		const FCompactPoseBoneIndex SpineBoneIndex = SpineFeetPair[i].SpineBoneRef.GetCompactPoseIndex(BoneContainer);
		const FTransform SpineBoneIndexCS = Output.Pose.GetComponentSpaceTransform(SpineBoneIndex);
		const FVector SpineBoneIndexWS = ComponentToWorld.TransformPosition(SpineBoneIndexCS.GetLocation());
		SpineTransformPairArray[i].SpineInvolved = FTransform(SpineBoneIndexWS);

		for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
		{
			if (SpineTransformPairArray[i].AssociatedFootArray.IsValidIndex(j))
			{
				const FCompactPoseBoneIndex FeetBoneIndex = SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer);
				const FTransform FeetBoneIndexCS = Output.Pose.GetComponentSpaceTransform(FeetBoneIndex);
				SpineTransformPairArray[i].AssociatedFootArray[j] = FTransform(ComponentToWorld.TransformPosition(FeetBoneIndexCS.GetLocation()));
			}
		}

		for (int32 j = 0; j < SpineFeetPair[i].KneeArray.Num(); j++)
		{
			if (SpineTransformPairArray[i].AssociatedKneeArray.IsValidIndex(j))
			{
				const FCompactPoseBoneIndex KneeBoneIndex = SpineFeetPair[i].KneeArray[j].GetCompactPoseIndex(BoneContainer);
				const FTransform KneeBoneIndexCS = Output.Pose.GetComponentSpaceTransform(KneeBoneIndex);
				SpineTransformPairArray[i].AssociatedKneeArray[j] = FTransform(ComponentToWorld.TransformPosition(KneeBoneIndexCS.GetLocation()));
			}
		}
	}
	LineTraceInitialized = true;


	// spine
	const int32 NumTransforms = CombinedIndiceArray.Num();

	for (int32 i = 0; i < NumTransforms; i++)
	{
		ReferencePoseBoneTransformArray[i] = (FBoneTransform(CombinedIndiceArray[i], Output.Pose.GetComponentSpaceTransform(CombinedIndiceArray[i])));
		if ((i < NumTransforms - 1) && !bInitializeAnimationArray)
		{
			SolvedBoneTransformArray[i] = ReferencePoseBoneTransformArray[i];
		}
	}

	bInitializeAnimationArray = true;
}

bool FAnimNode_CustomSpineSolver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	bool bHasResult = true;


	if (!bSpineSnakeBone || !SolverBoneData.FeetBones.IsEmpty())
	{
		for (const FBoneReference& FootBoneRef : SolverBoneData.FeetBones)
		{
			if (!FootBoneRef.IsValidToEvaluate(RequiredBones))
			{
				bHasResult = false;
			}
		}
	}

	if (!RootBoneRef.IsValidToEvaluate(RequiredBones))
	{
		return false;
	}

	return (RequiredBones.IsValid() &&
		!bSolveShouldFail &&
		bHasResult &&
		SolverBoneData.SpineBone.IsValidToEvaluate(RequiredBones) &&
		SolverBoneData.Pelvis.IsValidToEvaluate(RequiredBones) &&
		RequiredBones.BoneIsChildOf(SolverBoneData.SpineBone.BoneIndex, SolverBoneData.Pelvis.BoneIndex));
}

void FAnimNode_CustomSpineSolver::ApplyLineTrace(
	const FAnimationUpdateContext& Context,
	const FVector& Origin,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& OutHitResult,
	const FLinearColor& DebugColor,
	const bool bDrawLine)
{

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const UWorld* World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();
	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();
	const float OwnerScale = ComponentScale;

	const EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::None;

	TArray<FHitResult> HitResults;

	switch (RaycastTraceType)
	{
		case EIKRaycastType::LineTrace:
		{
			UKismetSystemLibrary::LineTraceMulti(World, StartLocation, EndLocation, Trace_Channel, true, IgnoreActors, DebugTrace, HitResults, true, DebugColor);
		}
		break;
		case EIKRaycastType::SphereTrace:
		{
			const float CapsuleSize = (TraceRadiusValue * OwnerScale);
			UKismetSystemLibrary::SphereTraceMulti(World, StartLocation, EndLocation, CapsuleSize, Trace_Channel, true, IgnoreActors, DebugTrace, HitResults, true, DebugColor);
		}
		break;
		case EIKRaycastType::BoxTrace:
		{
			const FVector Extent = FVector(1, 1, 0) * TraceRadiusValue * OwnerScale;
			UKismetSystemLibrary::BoxTraceMulti(World, StartLocation, EndLocation, Extent, FRotator(0, 0, 0), Trace_Channel, true, IgnoreActors, DebugTrace, HitResults, true, DebugColor);
		}
		break;
	}

	if (bDrawLine)
	{
		TraceStartList.Add(StartLocation);
		TraceEndList.Add(EndLocation);
		TraceLinearColor.Add(DebugColor.ToFColor(true));
	}

	UQuadrupedIKLibrary::GetSimpleHitResult(HitResults, NormalDotThreshold, OutHitResult);


#if ENABLE_DRAW_DEBUG
	if (bDisplayLineTrace && bDrawLine)
	{
		TWeakObjectPtr<UWorld> Editor_World = GEngine->GetWorldFromContextObject(SK, EGetWorldErrorMode::LogAndReturnNull);
		const float LocalScale = TraceRadiusValue * OwnerScale;

		switch (RaycastTraceType)
		{
		case EIKRaycastType::LineTrace:
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[=]() {
					if (Editor_World.IsValid())
					{
						UQuadrupedIKLibrary::DrawDebugLineTraceSingle(Editor_World.Get(), StartLocation, EndLocation, OutHitResult.bBlockingHit, OutHitResult, FLinearColor::Red, DebugColor);
					}
				},
				TStatId(), nullptr, ENamedThreads::GameThread
			);
		}
		break;
		case EIKRaycastType::SphereTrace:
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[=]() {
					if (Editor_World.IsValid())
					{
						UQuadrupedIKLibrary::DrawDebugSphereTraceSingle(Editor_World.Get(), StartLocation, EndLocation, LocalScale, OutHitResult.bBlockingHit, OutHitResult, FLinearColor::Red, DebugColor);
					}
				},
				TStatId(), nullptr, ENamedThreads::GameThread
			);

		}
		break;
		case EIKRaycastType::BoxTrace:
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[=]() {
					if (Editor_World.IsValid())
					{
						const FVector& Extent = FVector(1, 1, 0) * LocalScale;
						UQuadrupedIKLibrary::DrawDebugBoxTraceSingle(Editor_World.Get(), StartLocation, EndLocation, Extent, FQuat::Identity, OutHitResult.bBlockingHit, OutHitResult, FLinearColor::Red, DebugColor);
					}
				},
				TStatId(), nullptr, ENamedThreads::GameThread
			);

		}
		break;
		}

	}
#endif


}


void FAnimNode_CustomSpineSolver::ApplyMultiPointTraceBulk(
	const FAnimationUpdateContext& Context,
	const FVector& Origin,
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FVector& ForwardDir,
	const FVector& RightDir,
	FQuadlupedBoneHitPairs& OutHitPair)
{

	TRACE_CPUPROFILER_EVENT_SCOPE(CustomSpineSolver_ApplyMultiPointTraceBulk);

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const UWorld* World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();
	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();
	const float OwnerScale = ComponentScale;

	const EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::None;

	TArray<FHitResult> HitResults;

	const float LocalVirtualLegWidth = ComponentScale * VirtualLegWidth;
	const float LocalTraceRadius = TraceRadiusValue * OwnerScale;

	const FVector OwnerUp = ComponentToWorld.GetUnitAxis(EAxis::Z);
	//const FVector OwnerUp = ComponentToWorld.TransformVectorNoScale(CharacterDirectionVectorCS).GetSafeNormal();

	const FVector Extent(LocalVirtualLegWidth + LocalTraceRadius, LocalVirtualLegWidth + LocalTraceRadius, 5.0f * ComponentScale);

	UKismetSystemLibrary::BoxTraceMulti(World, StartLocation, EndLocation, Extent, 
		FRotator(ComponentToWorld.GetRotation()),  Trace_Channel, true,
		IgnoreActors, DebugTrace, HitResults, true, FLinearColor::Green);

	TraceStartList.Add(StartLocation);
	TraceEndList.Add(EndLocation);
	TraceLinearColor.Add(FLinearColor::Green.ToFColor(true));

	const FVector BaseOrigin = Origin;
	const FVector FrontOrigin = Origin + ForwardDir * LocalVirtualLegWidth;
	const FVector BackOrigin = Origin - ForwardDir * LocalVirtualLegWidth;
	const FVector LeftOrigin = Origin + RightDir * LocalVirtualLegWidth;
	const FVector RightOrigin = Origin - RightDir * LocalVirtualLegWidth;

	const FVector UpVector = ComponentToWorld.GetUnitAxis(EAxis::Z);


	auto SelectClosestHitToOrigin = [&](const FVector& TargetOrigin, FHitResult& OutHitResult)
		{
			OutHitResult.Init();

			float MinScore = TNumericLimits<float>::Max();

			for (const FHitResult& HitResult : HitResults)
			{
				if (!HitResult.bBlockingHit)
				{
					continue;
				}

				if (!HitResult.GetComponent() || !HitResult.GetComponent()->IsPhysicsCollisionEnabled())
				{
					continue;
				}

				if (HitResult.GetActor() && HitResult.GetActor()->IsA(ACharacter::StaticClass()))
				{
					continue;
				}

				const float NormalDot = FVector::DotProduct(HitResult.ImpactNormal.GetSafeNormal(), OwnerUp);

				if (NormalDot < NormalDotThreshold)
				{
					continue;
				}

				const float DistSqXY = FVector::DistSquaredXY(HitResult.ImpactPoint, TargetOrigin);
				const float DistSqZ = FMath::Square(HitResult.ImpactPoint.Z - TargetOrigin.Z);
				const float CombinedScore = DistSqXY * 8.0f + DistSqZ * 0.1f;

				if (CombinedScore < MinScore)
				{
					MinScore = CombinedScore;
					OutHitResult = HitResult;
				}
			}
		};


	auto RefineHitByLineTrace = [&](const FVector& TargetOrigin, FHitResult& InOutHit)
		{
			if (!InOutHit.bBlockingHit)
			{
				return;
			}

			FHitResult RefinedHit;
			const FVector Start = TargetOrigin + OwnerUp * (TraceRadiusValue * ComponentScale + 10.0f);
			const FVector End = TargetOrigin - OwnerUp * (TraceRadiusValue * ComponentScale + 20.0f);
			UKismetSystemLibrary::LineTraceSingle(SK, Start, End, Trace_Channel, true, IgnoreActors, EDrawDebugTrace::None, RefinedHit, true);

			if (RefinedHit.bBlockingHit)
			{
				InOutHit = RefinedHit;
			}
		};

	SelectClosestHitToOrigin(BaseOrigin, OutHitPair.ParentSpineHit);
	SelectClosestHitToOrigin(FrontOrigin, OutHitPair.ParentFrontHit);
	SelectClosestHitToOrigin(BackOrigin, OutHitPair.ParentBackHit);
	SelectClosestHitToOrigin(LeftOrigin, OutHitPair.ParentLeftHit);
	SelectClosestHitToOrigin(RightOrigin, OutHitPair.ParentRightHit);

	if (bIsRefineTraceEnable)
	{
		RefineHitByLineTrace(BaseOrigin, OutHitPair.ParentSpineHit);
		RefineHitByLineTrace(FrontOrigin, OutHitPair.ParentFrontHit);
		RefineHitByLineTrace(BackOrigin, OutHitPair.ParentBackHit);
		RefineHitByLineTrace(LeftOrigin, OutHitPair.ParentLeftHit);
		RefineHitByLineTrace(RightOrigin, OutHitPair.ParentRightHit);

	}

#if ENABLE_DRAW_DEBUG
	if (bDisplayLineTrace)
	{
		TWeakObjectPtr<UWorld> Editor_World = GEngine->GetWorldFromContextObject(SK, EGetWorldErrorMode::LogAndReturnNull);
		const FVector LocalExtent = Extent;

		const FHitResult DebugSpineHit = OutHitPair.ParentSpineHit;
		const FHitResult DebugFrontHit = OutHitPair.ParentFrontHit;
		const FHitResult DebugBackHit = OutHitPair.ParentBackHit;
		const FHitResult DebugLeftHit = OutHitPair.ParentLeftHit;
		const FHitResult DebugRightHit = OutHitPair.ParentRightHit;

		FFunctionGraphTask::CreateAndDispatchWhenReady(
			[=]()
			{
				if (!Editor_World.IsValid())
				{
					return;
				}

				UWorld* DebugWorld = Editor_World.Get();

				UQuadrupedIKLibrary::DrawDebugBoxTraceSingle(DebugWorld, StartLocation, EndLocation, LocalExtent, FQuat::Identity,
					DebugSpineHit.bBlockingHit, DebugSpineHit, FLinearColor::Red, FLinearColor::Green);

				auto DrawPickedHit = [&](const FHitResult& HitResult, const FColor& Color)
					{
						if (!HitResult.bBlockingHit)
						{
							return;
						}

						DrawDebugSphere(DebugWorld, HitResult.ImpactPoint, 8.0f, 12, Color, false, 0.0f, 0, 1.5f);
						DrawDebugLine(DebugWorld, HitResult.ImpactPoint, HitResult.ImpactPoint + HitResult.ImpactNormal * 20.0f, Color, false, 0.0f, 0, 1.5f);
					};

				DrawPickedHit(DebugSpineHit, FColor::Green);
				DrawPickedHit(DebugFrontHit, FColor::Yellow);
				DrawPickedHit(DebugBackHit, FColor::Black);
				DrawPickedHit(DebugLeftHit, FColor::Blue);
				DrawPickedHit(DebugRightHit, FColor::Red);
			},
			TStatId(),
			nullptr,
			ENamedThreads::GameThread);
	}
#endif

}


void FAnimNode_CustomSpineSolver::SolveSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_QuadrupedSpineSolver_Fabrik);

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	UpdateHandleHeightBoneSpineProcessor();

	const int32 LastIdx = (SpineHitPairs.Num() - 1);

	if (!bSpineSnakeBone)
	{
		ImpactRotation(Output, 0, RootEffectorTransform, MeshBases);
		ImpactRotation(Output, LastIdx, ChestEffectorTransform, MeshBases);
	}
	else
	{
		SnakeImpactRotation(Output, 0, RootEffectorTransform, MeshBases);
		SnakeImpactRotation(Output, LastIdx, ChestEffectorTransform, MeshBases);
	}


	SolveBetweenSpineIK(Output, MeshBases);

	// fabrik output
	FQuadrupedBoneSpineOutput BoneSpineOutput;

	if (!bSpineSnakeBone)
	{
		if (!bWasSingleSpine)
		{
			if (bSolveFromChest)
			{
				// chest higher spine bone
				BoneSpineOutput = BoneSpineProcessor_Direct(Output, ChestEffectorTransform, MeshBases);
			}
			else
			{
				// spine bone higher chest
				BoneSpineOutput = BoneSpineProcessor(Output, RootEffectorTransform, MeshBases);
			}
		}
		else
		{
			BoneSpineOutput = BoneSpineProcessor_Direct(Output, ChestEffectorTransform, MeshBases);
		}
	}
	else
	{
		BoneSpineOutput = BoneSpineProcessor_Snake(Output, ChestEffectorTransform, MeshBases);
	}

	BoneSpineOutput.SpineFirstEffectorTransform = ChestEffectorTransform;
	BoneSpineOutput.PelvisEffectorTransform = RootEffectorTransform;
	BoneSpineOutput = BoneSpineProcessor_Transform(BoneSpineOutput, Output, MeshBases);

	for (int32 LinkIndex = 0; LinkIndex < BoneSpineOutput.NumChainLinks; LinkIndex++)
	{
		const FSpineBoneChainLink& CurrentLink = BoneSpineOutput.BoneChainArray[LinkIndex];
		const int32 ThisIdx = CurrentLink.TransformIndex;

		if (!BoneSpineOutput.TempTransforms.IsValidIndex(ThisIdx))
		{
			continue;
		}

		const FTransform& CurrentBoneTransform = BoneSpineOutput.TempTransforms[ThisIdx].Transform;

		if (SolvedBoneTransformArray.IsValidIndex(ThisIdx))
		{
			SolvedBoneTransformArray[ThisIdx].Transform = CurrentBoneTransform;
		}
		if (SourcePoseBoneTransformArray.IsValidIndex(ThisIdx))
		{
			SourcePoseBoneTransformArray[ThisIdx].Transform = MeshBases.GetComponentSpaceTransform(CurrentLink.BoneIndex);
		}

		const int32 NumChildren = CurrentLink.ChildZeroLengthTransformIndices.Num();
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const int32 ChildIdx = CurrentLink.ChildZeroLengthTransformIndices[ChildIndex];

			if (!BoneSpineOutput.TempTransforms.IsValidIndex(ChildIdx))
			{
				continue;
			}

			if (SolvedBoneTransformArray.IsValidIndex(ChildIdx))
			{
				FTransform& ChildBoneTransform = BoneSpineOutput.TempTransforms[ChildIdx].Transform;
				ChildBoneTransform.NormalizeRotation();
				SolvedBoneTransformArray[ChildIdx].Transform = CurrentBoneTransform;
			}

			if (SourcePoseBoneTransformArray.IsValidIndex(ChildIdx))
			{
				SourcePoseBoneTransformArray[ChildIdx].Transform = MeshBases.GetComponentSpaceTransform(CurrentLink.BoneIndex);
			}
		}

	}
}

void FAnimNode_CustomSpineSolver::UpdateHandleHeightBoneSpineProcessor()
{

	const int32 LastIdx = (SpineHitPairs.Num() - 1);

	// @TODO
	// 動く床対応 単純なZ成分が動く場合は再度検証しないといけないかも
	bool bMovingFloor = false;
	for (int32 i = 0; i < SpineHitPairs.Num(); ++i)
	{
		if (IsMovingBase(SpineHitPairs[i].ParentSpineHit))
		{
			bMovingFloor = true;
			break;
		}
	}


	const FVector& PelvisCS = RootEffectorTransform.GetLocation();
	const FVector& ChestCS = ChestEffectorTransform.GetLocation();
	const FVector PelvisToChest = (ChestCS - PelvisCS);
	const float Horizontal = FMath::Max(FVector2D(PelvisToChest.X, PelvisToChest.Y).Size(), 1e-3f);
	SlopeUp01 = FMath::RadiansToDegrees(FMath::Atan2(PelvisToChest.Z, Horizontal));


	// +: pelvis higher, -: chest higher
	const float Diff = PelvisCS.Z - ChestCS.Z;
	const float AbsDiff = FMath::Abs(Diff);

	// ex: 5.0f
	const float SwitchOn = PlaneFloorHeightThreshold;

	// ex: 2.5f
	const float SwitchOff = PlaneFloorHeightThreshold * 0.5f;

	if (bMovingFloor)
	{
		// 1cm
		const float MovingSwitch = 1.0f * ComponentScale;
		if (!bSolveFromChest)
		{
			if (Diff < -MovingSwitch)
			{
				bSolveFromChest = true;
			}
		}
		else
		{
			if (Diff > MovingSwitch)
			{
				bSolveFromChest = false;
			}
		}
	}
	else
	{
		// 平面の場合は高さに閾値を設ける
		if (!bSolveFromChest)
		{
			// clearly chest higher
			if (Diff < -SwitchOn)
			{
				bSolveFromChest = true;
			}
		}
		else
		{
			// clearly pelvis higher, or almost flat
			if (Diff > SwitchOn || AbsDiff < SwitchOff)
			{
				bSolveFromChest = false;
			}
		}
	}


	const float PelvisDirAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(-PlaneFloorHeightThreshold, PlaneFloorHeightThreshold),
		FVector2D(0.0f, 1.0f),
		PelvisSlopeDirection);

	const float PelvisTargetAlpha = FMath::Lerp(PelvisDownSlopeStabilizationAlpha, PelvisUpSlopeStabilizationAlpha, PelvisDirAlpha);
	PelvisSlopeStabAlpha = FMath::FInterpTo(PelvisSlopeStabAlpha, PelvisTargetAlpha, CachedDeltaSeconds, FormatShiftSpeed);

	const float ChestDirAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(-PlaneFloorHeightThreshold, PlaneFloorHeightThreshold),
		FVector2D(0.0f, 1.0f),
		ChestSlopeDirection);

	const float ChestTargetAlpha = FMath::Lerp(ChestUpSlopeStabilizationAlpha, ChestDownslopeStabilizationAlpha, ChestDirAlpha);
	ChestSlopeStabAlpha = FMath::FInterpTo(ChestSlopeStabAlpha, ChestTargetAlpha, CachedDeltaSeconds, FormatShiftSpeed);


}

void FAnimNode_CustomSpineSolver::SolveBetweenSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases)
{
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	for (int32 i = 0; i < CombinedIndiceArray.Num(); i++)
	{

		const bool bIsMiddleJoint = (i > 0 && i < CombinedIndiceArray.Num() - 1);
		if (!bIsMiddleJoint)
		{
			continue;
		}

		const FTransform& BoneTraceTransform = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]);
		const FVector LerpLocation = ComponentToWorld.TransformPosition(BoneTraceTransform.GetLocation());
		FTransform Result = FTransform(LerpLocation);

		FVector& CurPos = SpineBetweenOffsetTransformArray[i - 1];
		const FHitResult& Hit = SpineHitBetweenArray[i - 1];

		if (Hit.bBlockingHit)
		{
			const float PrevHeight = SpineBetweenHeightArray[i - 1];
			const FVector PointPos = SpinePointBetweenArray[i - 1];
			float TargetZ = PointPos.Z + PrevHeight;
			const FVector TargetPos = FVector(LerpLocation.X, LerpLocation.Y, TargetZ);
			Result.SetLocation(TargetPos);
		}

		CurPos = ComponentToWorld.InverseTransformPosition(Result.GetLocation());
	}
}

void FAnimNode_CustomSpineSolver::ResolveSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const auto SK = Output.AnimInstanceProxy->GetSkelMeshComponent();
	const AActor* Owner = SK->GetOwner();

	const int32 NumTransforms = CombinedIndiceArray.Num();
	OutBoneTransforms.Empty();
	FTransform PelvisDiffTransform = FTransform::Identity;

	constexpr float RestBoneScaleThes = 200.0f;
	constexpr float AnimBoneScaleThes = 10000.0f;

	if (Owner->GetWorld()->IsGameWorld())
	{
		for (int32 i = 0; i < ExtraSpineIndiceArray.Num(); i++)
		{
			if (ExtraSpineIndiceArray[i].GetInt() < SpineIndiceArray[0].GetInt())
			{
				const FTransform UpdatedTransform = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[0]);
				const FQuat QuatDiff = (SolvedBoneTransformArray[0].Transform.Rotator().Quaternion() * UpdatedTransform.Rotator().Quaternion().Inverse()).GetNormalized();

				const FVector DeltaPositionDiff = UpdatedTransform.GetLocation() - SolvedBoneTransformArray[0].Transform.GetLocation();
				FTransform OriginalTransform = MeshBases.GetComponentSpaceTransform(ExtraSpineIndiceArray[i]);
				OriginalTransform.SetLocation(OriginalTransform.GetLocation() - DeltaPositionDiff);
				OriginalTransform.SetRotation((QuatDiff * OriginalTransform.Rotator().Quaternion()));

				OutBoneTransforms.Add(FBoneTransform(ExtraSpineIndiceArray[i], OriginalTransform));
			}
		}


		FTransform StabilizationPelvis = FTransform::Identity;
		FTransform StabilizationPelvisAdd = FTransform::Identity;
		FTransform StabilizationChest = FTransform::Identity;

		const int32 TipReduction = (bIgnoreEndPoints) ? IgnoreTipPointIndex : 0;
		const float Factor = (1.0f - FMath::Exp(-SmoothFactor * CachedDeltaSeconds));
		const float LocalVal = (bAtleastOneHit && bEnableSolver) ? 1.0f : 0.f;


		for (int32 i = 0; i < NumTransforms - 0; i++)
		{
			if (i < (NumTransforms - TipReduction))
			{
				float& SpineAlpha = TotalSpineAlphaArray[i];
				SpineAlpha = FMath::FInterpTo(SpineAlpha, LocalVal, Factor, FormatShiftSpeed);
				SpineAlpha = FMath::Clamp(SpineAlpha, 0.0f, 1.0f);

				FTransform UpdatedTransform = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]);

				const float DistFromRest = (ReferencePoseBoneTransformArray[i].Transform.GetLocation() - UpdatedTransform.GetLocation()).Size();
				const float DistFromAnim = (SolvedBoneTransformArray[i].Transform.GetLocation() - UpdatedTransform.GetLocation()).Size();
				const bool bIsValidDist = ((DistFromRest < RestBoneScaleThes * ComponentScale && DistFromAnim < AnimBoneScaleThes * ComponentScale));


				if (!ReferencePoseBoneTransformArray[i].Transform.ContainsNaN() && !SolvedBoneTransformArray[i].Transform.ContainsNaN())
				{
					const FRotator AnimRot = SolvedBoneTransformArray[i].Transform.Rotator();
					const FRotator RestRot = ReferencePoseBoneTransformArray[i].Transform.Rotator();
					const FQuat QuatDiff = (UpdatedTransform.Rotator().Quaternion() * RestRot.Quaternion().Inverse()).GetNormalized();
					const FQuat QuatTarget = (QuatDiff * AnimRot.Quaternion());
					const FVector DeltaPositionDiff = UpdatedTransform.GetLocation() - ReferencePoseBoneTransformArray[i].Transform.GetLocation();
					const FVector OrigPos = SolvedBoneTransformArray[i].Transform.GetLocation();
					const FVector Target = DeltaPositionDiff + OrigPos;
					UpdatedTransform.SetRotation(QuatTarget);
					UpdatedTransform.SetLocation(Target);
				}

				if (bWasSingleSpine)
				{
					if (i == 0)
					{
						const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
						OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));

						if (bStabilizePelvisLegs)
						{
							if (SpineFeetPair[0].SpineBoneRef.CachedCompactPoseIndex == CombinedIndiceArray[i])
							{
								StabilizationPelvis = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]).Inverse() * OutBoneTransforms[OutBoneTransforms.Num() - 1].Transform;
							}
						}
					}
				}
				else
				{
					if (bOnlyRootSolve)
					{
						if (i == 0)
						{
							const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
							OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));
						}
					}
					else if (SolverComplexityType == ESolverComplexityType::Simple && !bSpineSnakeBone)
					{
						if (i == 0)
						{
							PelvisDiffTransform = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]).Inverse() * UpdatedTransform;
							const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
							OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));
						}

						if (i == 1 && i != NumTransforms - 1)
						{
							const FVector ChestLocationRef = SolvedBoneTransformArray[NumTransforms - 1].Transform.GetLocation();
							const FVector ChestLocationOrig = SourcePoseBoneTransformArray[NumTransforms - 1].Transform.GetLocation();
							const FVector BodyToChestLook = (UpdatedTransform.GetLocation() - ChestLocationRef).GetSafeNormal();
							const FVector BodyToChestOrigLook = ((SourcePoseBoneTransformArray[i].Transform.GetLocation()) - ChestLocationOrig).GetSafeNormal();
							const FQuat RotateDiff = FQuat::FindBetweenNormals(BodyToChestOrigLook, BodyToChestLook);
							UpdatedTransform.SetLocation((MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]) * PelvisDiffTransform).GetLocation());
							UpdatedTransform.SetRotation(RotateDiff * SourcePoseBoneTransformArray[i].Transform.GetRotation());

							const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
							OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));
						}

						if (i > 0 && i == NumTransforms - 1 && !bIgnoreChestSolve)
						{
							const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
							OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));
						}
					}
					else
					{
						// snake bone or legacy fabrik
						const FTransform& Blend = SpineSolverHelper::TLerp(MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]), UpdatedTransform, SpineAlpha);
						OutBoneTransforms.Add(FBoneTransform(CombinedIndiceArray[i], Blend));
					}

					const FBoneTransform PrevBoneTransform = OutBoneTransforms[OutBoneTransforms.Num() - 1];
					if (bStabilizePelvisLegs)
					{
						if (SpineFeetPair[0].SpineBoneRef.CachedCompactPoseIndex == CombinedIndiceArray[i])
						{
							StabilizationPelvis = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]).Inverse() * PrevBoneTransform.Transform;
						}
					}

					if (bStabilizeChestLegs || bStabilizePelvisLegs)
					{
						if (i == 1 && i != NumTransforms - 1)
						{
							StabilizationPelvisAdd = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]).Inverse() * PrevBoneTransform.Transform;
						}
						if (SpineFeetPair[SpineFeetPair.Num() - 1].SpineBoneRef.CachedCompactPoseIndex == CombinedIndiceArray[i])
						{
							StabilizationChest = MeshBases.GetComponentSpaceTransform(CombinedIndiceArray[i]).Inverse() * PrevBoneTransform.Transform;
						}
					}

				}
			}
		}

		const bool bStabilizedEnable = ((bStabilizePelvisLegs || bStabilizeChestLegs) && !OutBoneTransforms.IsEmpty());

		if (bStabilizedEnable)
		{
			for (int32 i = 0; i < SpineFeetPair.Num(); i++)
			{
				for (int32 j = 0; j < SpineFeetPair[i].ThighArray.Num(); j++)
				{
					if (SpineFeetPair[i].ThighArray[j].IsValidToEvaluate())
					{
						if (i == 0 && bStabilizePelvisLegs)
						{
							FTransform ThighTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex) * StabilizationPelvis;

							const FQuat ThighOrigRotation = ThighTransform.GetRotation();

							ThighTransform.SetRotation(FQuat::Slerp(
								ThighOrigRotation,
								MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex).GetRotation(),
								PelvisSlopeStabAlpha));

							OutBoneTransforms.Add(FBoneTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex, ThighTransform));
						}

						if (i > 0 && bStabilizeChestLegs)
						{
							FTransform ThighTransform;

							if (bOnlyRootSolve)
							{
								ThighTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex) * StabilizationPelvis;
							}
							else
							{
								if (bIgnoreChestSolve)
								{
									ThighTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex) * StabilizationPelvisAdd;
								}
								else
								{
									ThighTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex) * StabilizationChest;
								}
							}

							const FQuat ThighOriginalRotation = ThighTransform.GetRotation();

							ThighTransform.SetRotation(FQuat::Slerp(
								ThighOriginalRotation,
								MeshBases.GetComponentSpaceTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex).GetRotation(),
								ChestSlopeStabAlpha));

							OutBoneTransforms.Add(FBoneTransform(SpineFeetPair[i].ThighArray[j].CachedCompactPoseIndex, ThighTransform));
						}

					}
				}
			}

			if (StabilizationTailBoneRef.IsValidToEvaluate() && bStabilizePelvisLegs)
			{
				const FTransform OrigTail = MeshBases.GetComponentSpaceTransform(StabilizationTailBoneRef.CachedCompactPoseIndex);
				FTransform TailTransform = OrigTail * StabilizationPelvis;
				TailTransform.SetRotation(FQuat::Slerp(TailTransform.GetRotation(), OrigTail.GetRotation(), PelvisSlopeStabAlpha));
				OutBoneTransforms.Add(FBoneTransform(StabilizationTailBoneRef.CachedCompactPoseIndex, TailTransform));
			}

			if (StabilizationHeadBoneRef.IsValidToEvaluate() && bStabilizeChestLegs)
			{
				const FTransform OrigHead = MeshBases.GetComponentSpaceTransform(StabilizationHeadBoneRef.CachedCompactPoseIndex);
				FTransform HeadTransform;

				if (bStabilizeChestLegs)
				{
					if (bOnlyRootSolve)
					{
						HeadTransform = MeshBases.GetComponentSpaceTransform(StabilizationHeadBoneRef.CachedCompactPoseIndex) * StabilizationPelvis;
					}
					else
					{
						if (bIgnoreChestSolve)
						{
							HeadTransform = MeshBases.GetComponentSpaceTransform(StabilizationHeadBoneRef.CachedCompactPoseIndex) * StabilizationPelvisAdd;
						}
						else
						{
							HeadTransform = MeshBases.GetComponentSpaceTransform(StabilizationHeadBoneRef.CachedCompactPoseIndex) * StabilizationChest;
						}
					}
				}


				if (bHeadSlopeStabilization)
				{
					const FVector HeadUp_WS = ComponentToWorld.TransformVector(CharacterDirectionVectorCS).GetSafeNormal();
					const FVector HeadPos_WS = ComponentToWorld.TransformPosition(HeadTransform.GetLocation());
					const FVector HeadFwd_WS = ComponentToWorld.TransformVectorNoScale(HeadTransform.GetUnitAxis(EAxis::X)).GetSafeNormal();

					const float NoseForwardCm = NoseForwardOffset * ComponentScale;
					const FVector NosePos_WS = HeadPos_WS + HeadFwd_WS * NoseForwardCm;

					const float RadiusCm = NoseTraceRadius * ComponentScale;
					const float TraceLenCm = NoseTraceLength * ComponentScale;
					const FVector TraceEnd_WS = NosePos_WS + HeadFwd_WS * TraceLenCm;

					FHitResult NoseHit;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(HeadNoseTrace), false);
					Params.AddIgnoredActor(SK->GetOwner());

					ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(Trace_Channel);

					const bool bNoseHit = SK->GetWorld()->SweepSingleByChannel(NoseHit, NosePos_WS, TraceEnd_WS, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(RadiusCm), Params);

					FHitResult JawHit;
					const float JawCheckLen = RadiusCm * 2.0f;
					const FVector JawTraceEnd = NosePos_WS - (HeadUp_WS * JawCheckLen);
					const bool bJawHit = SK->GetWorld()->LineTraceSingleByChannel(JawHit, NosePos_WS, JawTraceEnd, CollisionChannel, Params);

					const int32 LastIdx = SpineHitPairs.Num() - 1;
					const FHitResult& ChestHit = SpineHitPairs[LastIdx].ParentSpineHit;

					// ignore up floor
					const FVector NoseN = NoseHit.ImpactNormal.GetSafeNormal();
					const float UpDot = FVector::DotProduct(NoseN, HeadUp_WS);
					const bool bAcceptNoseHit = bNoseHit && NoseHit.bBlockingHit && UpDot > 0.2f;

					// 上向き法線だけ有効にする
					const bool bValidNoseHit = bAcceptNoseHit;//bNoseHit && NoseHit.bBlockingHit;
					const bool bValidChestHit = ChestHit.bBlockingHit;

					const FVector ChestFallbackNormal_WS = bValidChestHit ? ChestHit.ImpactNormal.GetSafeNormal() : FVector::UpVector;
					const FVector NoseNormal_WS = bValidNoseHit ? NoseHit.ImpactNormal.GetSafeNormal() : ChestFallbackNormal_WS;

					HeadSurfaceBlendAlpha = FMath::FInterpTo(HeadSurfaceBlendAlpha, bValidNoseHit ? 1.0f : 0.0f, CachedDeltaSeconds, HeadSurfaceSwitchSpeed);

					FVector TargetFloorN_WS = FMath::Lerp(ChestFallbackNormal_WS, NoseNormal_WS, HeadSurfaceBlendAlpha).GetSafeNormal();

					if (TargetFloorN_WS.IsNearlyZero())
					{
						TargetFloorN_WS = FVector::UpVector;
					}

					// 法線の履歴付き平滑化
					SmoothedHeadNormal = FMath::VInterpTo(SmoothedHeadNormal, TargetFloorN_WS, CachedDeltaSeconds, FormatLocationLerp).GetSafeNormal();

					float TargetPushAmt = 0.0f;
					FVector TargetPushNormal_WS = SmoothedHeadNormal;

					if (bValidNoseHit)
					{
						TargetPushAmt = NoseHit.bStartPenetrating ? NoseHit.PenetrationDepth : FMath::Max(0.0f, RadiusCm - NoseHit.Distance);

						FVector SafePushNormal_WS = NoseHit.ImpactNormal.GetSafeNormal();

						if (FVector::DotProduct(SafePushNormal_WS, HeadFwd_WS) > 0.0f)
						{
							SafePushNormal_WS = FVector::VectorPlaneProject(SafePushNormal_WS, HeadFwd_WS).GetSafeNormal();
						}

						if (SafePushNormal_WS.IsNearlyZero())
						{
							SafePushNormal_WS = (HeadPos_WS - NoseHit.ImpactPoint).GetSafeNormal();
						}

						if (!bJawHit)
						{
							TargetPushAmt *= 0.5f;
						}

						TargetPushNormal_WS = SafePushNormal_WS;
					}

					// push方向の履歴付き平滑化
					SmoothedHeadPushNormal_WS = FMath::VInterpTo(SmoothedHeadPushNormal_WS, TargetPushNormal_WS, CachedDeltaSeconds, HeadPushNormalInterpSpeed).GetSafeNormal();

					const FVector FloorN_CS = ComponentToWorld.InverseTransformVectorNoScale(SmoothedHeadNormal).GetSafeNormal();

					const float KeepYaw = HeadTransform.GetRotation().Rotator().Yaw;
					FVector Heading_CS = FRotationMatrix(FRotator(0.f, KeepYaw, 0.f)).GetUnitAxis(EAxis::X);

					FVector Fwd_CS = Heading_CS;
					const float Nz = FloorN_CS.Z;
					if (FMath::Abs(Nz) > 1e-3f)
					{
						Fwd_CS.Z = -((Fwd_CS.X * FloorN_CS.X) + (Fwd_CS.Y * FloorN_CS.Y)) / FloorN_CS.Z;
						Fwd_CS = Fwd_CS.GetSafeNormal();
					}
					else
					{
						Fwd_CS = FVector::VectorPlaneProject(Heading_CS, FloorN_CS).GetSafeNormal();
					}

					FRotator TargetRot = FRotationMatrix::MakeFromXZ(Fwd_CS, FloorN_CS).Rotator().GetNormalized();
					TargetRot.Pitch = FMath::Clamp(TargetRot.Pitch, HeadPitchLimitRange.X, HeadPitchLimitRange.Y);
					TargetRot.Roll = FMath::Clamp(TargetRot.Roll, HeadRollLimitRange.X, HeadRollLimitRange.Y);

					// yawは維持、pitch/rollだけ安定化
					TargetRot.Yaw = HeadTransform.GetRotation().Rotator().Yaw;

					// dead zone: 微小揺れは無視
					if (FMath::Abs(FMath::FindDeltaAngleDegrees(PrevHeadTargetRot.Pitch, TargetRot.Pitch)) < HeadRotDeadZoneDeg)
					{
						TargetRot.Pitch = PrevHeadTargetRot.Pitch;
					}
					if (FMath::Abs(FMath::FindDeltaAngleDegrees(PrevHeadTargetRot.Roll, TargetRot.Roll)) < HeadRotDeadZoneDeg)
					{
						TargetRot.Roll = PrevHeadTargetRot.Roll;
					}

					PrevHeadTargetRot = FMath::RInterpTo(PrevHeadTargetRot, TargetRot, CachedDeltaSeconds, HeadPitchRollInterpSpeed).GetNormalized();

					const FQuat SmoothedRot = FQuat(PrevHeadTargetRot);
					const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FloorN_CS, FVector::UpVector), -1.f, 1.f)));
					const float SlopeAlpha = FMath::GetMappedRangeValueClamped(HeadSlopeAngleRange, FVector2D(0.f, 1.f), SlopeDeg);
					const FQuat NewQ = FQuat::Slerp(HeadTransform.GetRotation(), SmoothedRot, SlopeAlpha).GetNormalized();
					HeadTransform.SetRotation(NewQ);
					CurrentHeadPushAlpha = FMath::FInterpTo(CurrentHeadPushAlpha, TargetPushAmt, CachedDeltaSeconds, FormatLocationLerp);

					const bool bIsAllowUpdateTranslation = false;

					if (CurrentHeadPushAlpha > 0.1f && bIsAllowUpdateTranslation)
					{
						const FVector PushWS = SmoothedHeadPushNormal_WS * (CurrentHeadPushAlpha + 1.2f);
						const FVector HeadPushCS = ComponentToWorld.InverseTransformVectorNoScale(PushWS);
						HeadTransform.AddToTranslation(HeadPushCS);

						const FVector PropagatedPushCS = HeadPushCS * 0.2f;
						const FCompactPoseBoneIndex TargetChestIdx = SpineFeetPair.Last().SpineBoneRef.CachedCompactPoseIndex;
						for (FBoneTransform& BT : OutBoneTransforms)
						{
							if (BT.BoneIndex == TargetChestIdx)
							{
								BT.Transform.AddToTranslation(PropagatedPushCS);
								break;
							}
						}

					}


					if (bDisplayLineTrace)
					{
						UWorld* World = SK->GetWorld();
						FColor DebugColor = bValidNoseHit ? FColor::Green : FColor::Red;
						DrawDebugCapsule(World,
							(NosePos_WS + TraceEnd_WS) * 0.5f,
							(TraceLenCm * 0.5f) + RadiusCm,
							RadiusCm, FRotationMatrix::MakeFromZ(HeadFwd_WS).ToQuat(),
							DebugColor, false, -1.0f, 0, 0.5f);

						if (bValidNoseHit)
						{
							DrawDebugPoint(World, NoseHit.ImpactPoint, 12.0f, FColor::Yellow, false);
							DrawDebugLine(World, NoseHit.ImpactPoint, NoseHit.ImpactPoint + NoseHit.ImpactNormal * 20.0f, FColor::Blue, false);
						}
					}
				}
				else
				{
					HeadTransform.SetRotation(FQuat::Slerp(HeadTransform.GetRotation(), OrigHead.GetRotation(), ChestSlopeStabAlpha));
				}

				OutBoneTransforms.Add(FBoneTransform(StabilizationHeadBoneRef.CachedCompactPoseIndex, HeadTransform));
			}
		}
	}
}

void FAnimNode_CustomSpineSolver::InitializeEffectorTransform(FCSPose<FCompactPose>& MeshBases)
{
	if (!bEffectorInitialized)
	{
		const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();

		PelvisIdx = SpineFeetPair[0].SpineBoneRef.GetCompactPoseIndex(BoneContainer);
		ChestIdx = SpineFeetPair.Last().SpineBoneRef.GetCompactPoseIndex(BoneContainer);

		if (PelvisIdx != INDEX_NONE)
		{
			RootEffectorTransform.SetLocation(MeshBases.GetComponentSpaceTransform(PelvisIdx).GetLocation());
			RootEffectorTransform.SetRotation(MeshBases.GetComponentSpaceTransform(PelvisIdx).GetRotation());
		}

		if (ChestIdx != INDEX_NONE)
		{
			ChestEffectorTransform.SetLocation(MeshBases.GetComponentSpaceTransform(ChestIdx).GetLocation());
			ChestEffectorTransform.SetRotation(MeshBases.GetComponentSpaceTransform(ChestIdx).GetRotation());
		}

		
		bEffectorInitialized = true;
	}

}

/// <summary>
/// Snake IK function
/// </summary>
void FAnimNode_CustomSpineSolver::SnakeImpactRotation(const FComponentSpacePoseContext& Output, const int32 PointIndex, FTransform& OutputTransform, FCSPose<FCompactPose>& MeshBases)
{
	SCOPE_CYCLE_COUNTER(STAT_QuadrupedSpineSolver_TailImpactRotation);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const auto SK = Output.AnimInstanceProxy->GetSkelMeshComponent();

	auto& HitPair = SpineHitPairs[PointIndex];

	FVector ParentSpinePointCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentSpinePoint);
	FVector PointingToTransformCS = ParentSpinePointCS + (CharacterDirectionVectorCS * TotalSpineHeights[PointIndex]);

	const FTransform& BoneCSTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[PointIndex].SpineBoneRef.GetCompactPoseIndex(BoneContainer));
	const FRotator& BoneRotator = FRotator(BoneCSTransform.GetRotation());
	const FVector& BoneCSLocation = BoneCSTransform.GetLocation();

	FRotator FinalRotator = FRotator(0.0f, BoneRotator.Yaw, 0.0f);
	float ChestForwardRotationIntensity_INPUT = ChestForwardRotationIntensity;
	float PelvisForwardRotationIntensity_INPUT = PelvisForwardRotationIntensity;

	float PelvisSideRotationIntensity_INPUT = PelvisSidewardRotationIntensity;
	float ChestSideRotationIntensity_INPUT = ChestSidewardRotationIntensity;

	if (OutputTransform.GetLocation().IsNearlyZero())
	{
		OutputTransform.SetLocation(BoneCSLocation);
		return;
	}

	if (bFlipForwardAndRight)
	{
		const float PelvisSwapValue = PelvisForwardRotationIntensity_INPUT;
		PelvisForwardRotationIntensity_INPUT = PelvisSideRotationIntensity_INPUT;
		PelvisSideRotationIntensity_INPUT = PelvisSwapValue;
		const float ChestSwapValue = ChestForwardRotationIntensity_INPUT;
		ChestForwardRotationIntensity_INPUT = ChestSideRotationIntensity_INPUT;
		ChestSideRotationIntensity_INPUT = ChestSwapValue;
	}

	auto GetLocalZ = [&](const FVector& WSLoc)
		{
			return ComponentToWorld.InverseTransformPosition(WSLoc).Z;
		};

	auto ClampValue = [](float Val, const FVector2D& Range)
		{
			return FMath::Clamp(Val, Range.X, Range.Y);
		};

	FRotator PositionBaseRotation = FinalRotator;
	const FVector FeetMidPoint = ComponentToWorld.InverseTransformPosition(HitPair.ParentSpinePoint);

	const FVector CrossDot = FVector::CrossProduct(CharacterDirectionVectorCS, ForwardDirectionVector);
	const FVector CSUpVector = ComponentToWorld.InverseTransformVector((SK->GetUpVector()));
	const FVector CSForward = ComponentToWorld.InverseTransformVector((SK->GetForwardVector()));

	const bool bIsPelvis = (PointIndex == 0);
	const bool bIsLastJoint = (PointIndex == SpineTransformPairArray.Num() - 1);

	FVector ForwardDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;
	float ForwardIntensity = 0.0f;
	float RightIntensity = 0.0f;

	const bool bIsWritableParamters = (bAtleastOneHit && HitPair.ParentSpineHit.bBlockingHit && bEnableSolver);

	if (bIsPelvis)
	{
		if (HitPair.ParentFrontHit.bBlockingHit && HitPair.ParentBackHit.bBlockingHit)
		{
			ForwardIntensity = (GetLocalZ(HitPair.ParentFrontPoint) - GetLocalZ(HitPair.ParentBackPoint)) * PelvisForwardRotationIntensity_INPUT;
		}
		else
		{
			ForwardIntensity = 0.0f;
		}

		float Direction = 1.0f;
		if (HitPair.FeetHitArray.Num() > 0)
		{
			const FVector& FootPos = SpineTransformPairArray[PointIndex].AssociatedFootArray[0].GetLocation();
			const float FootX = ComponentToWorld.InverseTransformPosition(FootPos).X;
			Direction = (FootX > 0.0f) ? 1.0f : -1.0f;
		}

		if (HitPair.ParentLeftHit.bBlockingHit && HitPair.ParentRightHit.bBlockingHit)
		{
			RightIntensity = (GetLocalZ(HitPair.ParentLeftPoint) - GetLocalZ(HitPair.ParentRightPoint)) * PelvisSideRotationIntensity_INPUT * 0.5f;
		}
		else
		{
			RightIntensity = 0.0f;
		}

		ForwardDirection = (CSForward) * ForwardIntensity;
		RightDirection = ComponentToWorld.TransformVectorNoScale(CrossDot) * RightIntensity;

		FVector RelativePosition = (ComponentToWorld.TransformPosition(PointingToTransformCS) - (ComponentToWorld.TransformPosition(FeetMidPoint) + ForwardDirection)).GetSafeNormal();
		PositionBaseRotation = SpineSolverHelper::CustomLookRotation(RelativePosition, CSUpVector);
		PositionBaseRotation.Yaw = FinalRotator.Yaw;

		const FVector RollRelativePos = (PointingToTransformCS - (FeetMidPoint + RightDirection)).GetSafeNormal();
		FRotator RollLookRotation = SpineSolverHelper::CustomLookRotation(RollRelativePos, CSUpVector);

		PositionBaseRotation.Roll = RollLookRotation.Roll;
		PositionBaseRotation.Pitch = RightIntensity * -1;
		PositionBaseRotation.Roll = ForwardIntensity * -1;
		SpineRotationDiffArray[PointIndex].Yaw = PositionBaseRotation.Yaw;

		if (bAtleastOneHit)
		{
			auto& TargetDiff = SpineRotationDiffArray[PointIndex];
			TargetDiff.Pitch = ClampValue(PositionBaseRotation.Pitch, PitchRange);
			TargetDiff.Roll = ClampValue(PositionBaseRotation.Roll, RollRange);
		}
	}
	else if (bIsLastJoint)
	{

		if (HitPair.ParentFrontHit.bBlockingHit && HitPair.ParentBackHit.bBlockingHit)
		{
			ForwardIntensity = (GetLocalZ(HitPair.ParentFrontPoint) - GetLocalZ(HitPair.ParentBackPoint)) * ChestForwardRotationIntensity_INPUT;
		}
		else
		{
			ForwardIntensity = 0.0f;
		}

		float Direction = 1;
		if (HitPair.FeetHitArray.Num() > 0)
		{
			const FVector& FootPos = SpineTransformPairArray[PointIndex].AssociatedFootArray[0].GetLocation();
			const float FootX = ComponentToWorld.InverseTransformPosition(FootPos).X;
			Direction = (FootX > 0.0f) ? 1.0f : -1.0f;
		}

		if (HitPair.FeetHitArray.Num() > 2 || HitPair.FeetHitArray.Num() == 0)
		{
			if (HitPair.ParentLeftHit.bBlockingHit && HitPair.ParentRightHit.bBlockingHit)
			{
				RightIntensity = (GetLocalZ(HitPair.ParentLeftPoint) - GetLocalZ(HitPair.ParentRightPoint)) * ChestSideRotationIntensity_INPUT * 0.5f;
			}
			else
			{
				RightIntensity = 0;
			}
		}
		else
		{
			if (HitPair.FeetHitArray.Num() > 1)
			{
				RightIntensity = (GetLocalZ(HitPair.FeetHitPointArray[0]) - GetLocalZ(HitPair.FeetHitPointArray[1])) * Direction * ChestSideRotationIntensity_INPUT * -1 * 0.5f;
			}
		}

		ForwardDirection = (CSForward) * ForwardIntensity;
		RightDirection = ComponentToWorld.TransformVectorNoScale(CrossDot) * RightIntensity;

		const FVector RelativePosition = (PointingToTransformCS - (FeetMidPoint + ForwardDirection)).GetSafeNormal();
		PositionBaseRotation = SpineSolverHelper::CustomLookRotation(RelativePosition, CSUpVector);
		PositionBaseRotation.Yaw = FinalRotator.Yaw;
		PositionBaseRotation.Pitch = RightIntensity * -1;
		PositionBaseRotation.Roll = ForwardIntensity * -1;
		SpineRotationDiffArray[PointIndex].Yaw = PositionBaseRotation.Yaw;

		if (bAtleastOneHit)
		{
			auto& TargetDiff = SpineRotationDiffArray[PointIndex];
			TargetDiff.Pitch = ClampValue(PositionBaseRotation.Pitch, PitchRange);
			TargetDiff.Roll = ClampValue(PositionBaseRotation.Roll, RollRange);
		}
	}

	const bool bIsValidJoint = (SpineHitPairs.Num() > PointIndex - 1);

	if (bIsValidJoint && bIsWritableParamters)
	{
		const FRotator TargetRotator = SpineRotationDiffArray[PointIndex].GetNormalized();
		const FQuat BoneCSRot = ComponentToWorld.InverseTransformRotation(TargetRotator.Quaternion());
		OutputTransform.SetRotation(BoneCSRot.GetNormalized());

		FVector Z_Offset = BoneCSLocation - CharacterDirectionVectorCS * (TotalSpineHeights[PointIndex]);
		Z_Offset = ComponentToWorld.InverseTransformPosition(HitPair.ParentSpinePoint);
		Z_Offset += CharacterDirectionVectorCS * (PelvisBaseOffset);
		const FVector ModifyCSLocation = ComponentToWorld.TransformPosition(Z_Offset);
		const FVector ResultPosition = ModifyCSLocation + CharacterDirectionVectorCS * TotalSpineHeights[PointIndex] * ComponentScale;
		const auto ModifyBoneCSPos = ComponentToWorld.InverseTransformPosition(ResultPosition);
		OutputTransform.SetLocation(ModifyBoneCSPos);
	}
	else
	{
		OutputTransform.SetLocation(BoneCSLocation);
		OutputTransform.SetRotation(BoneCSTransform.GetRotation().GetNormalized());
	}

}

/// <summary>
/// Animal Humanoid Fabrik
/// </summary>
void FAnimNode_CustomSpineSolver::ImpactRotation(const FComponentSpacePoseContext& Output, const int32 PointIndex,  FTransform& OutputTransform, FCSPose<FCompactPose>& MeshBases)
{
	SCOPE_CYCLE_COUNTER(STAT_QuadrupedSpineSolver_ImpactRotation);

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	const bool bIsPelvisJoint = (PointIndex == 0);
	const bool bIsChestJoint = (PointIndex == SpineFeetPair.Num() - 1);

	const FTransform BoneCSTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[PointIndex].SpineBoneRef.GetCompactPoseIndex(BoneContainer));
	const FRotator CurrentYawOnly(0.0f, BoneCSTransform.Rotator().Yaw, 0.0f);

	const FVector BaseCS = BoneCSTransform.GetLocation();
	const FRotator BaseCSRot = BoneCSTransform.GetRotation().Rotator();
	const FSpineSupportData& Support = BuildSpineSupportData(Output, MeshBases, PointIndex);

	if (!(bAtleastOneHit && Support.bHasParentHit && bEnableSolver))
	{
		ApplySpineTarget(Output, OutputTransform, BaseCS, BaseCSRot, FormatLocationLerp, FormatRotationLerp, true, true);
		return;
	}

	FVector TargetLocationCS = ComputeSpineTargetLocationCS(
		Output,
		Support,
		PointIndex,
		bIsPelvisJoint);

	// X/Y は元のアニメーション位置を維持
	TargetLocationCS.X = BaseCS.X;
	TargetLocationCS.Y = BaseCS.Y;

	FRotator TargetRotation = ComputeSpineTargetRotation(
		Output,
		Support,
		PointIndex,
		bIsPelvisJoint,
		CurrentYawOnly);
	// keep yaw
	TargetRotation.Yaw = BaseCSRot.Yaw;


	ApplySpineTarget(Output, OutputTransform, TargetLocationCS, TargetRotation, FormatLocationLerp, FormatRotationLerp, true, true);

}


/// <summary>
/// pelvis up
/// </summary>
const FQuadrupedBoneSpineOutput FAnimNode_CustomSpineSolver::BoneSpineProcessor(
	const FComponentSpacePoseContext& Output, 
	FTransform& EffectorTransform, 
	FCSPose<FCompactPose>& MeshBases)
{
	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	FQuadrupedBoneSpineOutput BoneSpineOutput;
	FTransform CSEffectorTransform = EffectorTransform;

	FVector CSEffectorLocation = CSEffectorTransform.GetLocation();
	const TArray<FCompactPoseBoneIndex> BoneIndices = SpineIndiceArray;
	BoneSpineOutput.BoneIndiceArray = BoneIndices;
	const int32 NumTransforms = BoneIndices.Num();
	BoneSpineOutput.TempTransforms.AddUninitialized(NumTransforms);

	TArray<FSpineBoneChainLink> Chain;
	Chain.Reserve(NumTransforms);
	FTransform RootTraceTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[0].SpineBoneRef.GetCompactPoseIndex(BoneContainer));
	FVector LerpLocation = RootTraceTransform.GetLocation();

	const FCompactPoseBoneIndex RootIdx = RootBoneRef.GetCompactPoseIndex(BoneContainer);
	const FTransform RootTransformCS = MeshBases.GetComponentSpaceTransform(RootIdx);
	const float PelvisDistance = FMath::Abs(LerpLocation.Z - RootTransformCS.GetLocation().Z);

	const FCompactPoseBoneIndex& TipBoneIndex = BoneIndices[BoneIndices.Num() - 1];
	const FTransform& BoneCSTransform_Local = MeshBases.GetComponentSpaceTransform(TipBoneIndex);
	FTransform Offset_Transform_Local = BoneCSTransform_Local;

	Offset_Transform_Local.SetLocation(ChestEffectorTransform.GetLocation());

	BoneSpineOutput.TempTransforms[BoneIndices.Num() - 1] = FBoneTransform(TipBoneIndex, Offset_Transform_Local);
	Chain.Add(FSpineBoneChainLink(Offset_Transform_Local.GetLocation(), 0.f, TipBoneIndex, (BoneIndices.Num() - 1) * 1));
	BoneSpineOutput.PelvisEffectorTransform = RootEffectorTransform;
	const FVector RelativeDiff = (Chain[0].Position - MeshBases.GetComponentSpaceTransform(TipBoneIndex).GetLocation());

	float BoneTotalLength = 0;

	for (int32 TransformIndex = NumTransforms - 2; TransformIndex > -1; TransformIndex--)
	{
		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];
		const FTransform& SelectBoneCSTransform = MeshBases.GetComponentSpaceTransform(BoneIndex);
		FTransform Offseted_Transform_TIndex = SelectBoneCSTransform;
		Offseted_Transform_TIndex.SetLocation(SelectBoneCSTransform.GetLocation() + RelativeDiff);
		FVector const BoneCSPosition = Offseted_Transform_TIndex.GetLocation();
		BoneSpineOutput.TempTransforms[TransformIndex] = FBoneTransform(BoneIndex, Offseted_Transform_TIndex);

		const float BoneLength = FVector::Dist(BoneCSPosition, BoneSpineOutput.TempTransforms[TransformIndex + 1].Transform.GetLocation());
		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(FSpineBoneChainLink(BoneCSPosition, BoneLength, BoneIndex, TransformIndex));
			BoneTotalLength += BoneLength;
		}
		else
		{
			FSpineBoneChainLink& ParentLink = Chain[Chain.Num() - 1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}

	const float PelvisChestDistance = FVector::Dist(Chain[Chain.Num() - 1].Position, Chain[0].Position);

	const float MaximumReach = (SolverComplexityType == ESolverComplexityType::Simple) ? PelvisChestDistance : BoneTotalLength;

	const float MaxRangeLimit = FMath::Clamp((CSEffectorLocation - Chain[0].Position).Size() / MaximumReach, MinExtensionRatio, MaxExtensionRatio);
	MaxRangeLimitLerp = FMath::FInterpTo(MaxRangeLimitLerp, MaxRangeLimit, CachedDeltaSeconds, ExtensionSwitchSpeed);
	CSEffectorLocation = Chain[0].Position + (CSEffectorLocation - Chain[0].Position).GetSafeNormal() * (MaximumReach * MaxReachIntensity);

	bool bBoneLocationUpdated = false;
	BoneSpineOutput.bIsMoved = false;
	const float RootToTargetDistSq = FVector::DistSquared(Chain[0].Position, CSEffectorLocation);
	const int32 NumChainLinks = Chain.Num();
	BoneSpineOutput.NumChainLinks = NumChainLinks;

	const int32 CustomIteration = (SolverComplexityType == ESolverComplexityType::Simple) ? MinIterations : MaxIterations;

	const int32 TipBoneLinkIndex = NumChainLinks - 1;
	float Slop = FVector::Dist(Chain[TipBoneLinkIndex].Position, CSEffectorLocation);
	if (Slop > Precision)
	{
		Chain[TipBoneLinkIndex].Position = CSEffectorLocation;
		int32 IterationCount = 0;
		while ((Slop > Precision) && (IterationCount++ < CustomIteration))
		{
			for (int32 LinkIndex = TipBoneLinkIndex - 1; LinkIndex > 0; LinkIndex--)
			{
				FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];
				FSpineBoneChainLink const& ChildLink = Chain[LinkIndex + 1];
				CurrentLink.Position = (ChildLink.Position + (CurrentLink.Position - ChildLink.Position).GetUnsafeNormal() * ChildLink.Length);
			}

			for (int32 LinkIndex = 1; LinkIndex < TipBoneLinkIndex; LinkIndex++)
			{
				FSpineBoneChainLink const& ParentLink = Chain[LinkIndex - 1];
				FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];
				CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
			}

			Slop = FMath::Abs(Chain[TipBoneLinkIndex].Length - FVector::Dist(Chain[TipBoneLinkIndex - 1].Position, CSEffectorLocation));
		}

		{
			FSpineBoneChainLink const& ParentLink = Chain[TipBoneLinkIndex - 1];
			FSpineBoneChainLink& CurrentLink = Chain[TipBoneLinkIndex];
			CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
		}

		bBoneLocationUpdated = true;
		BoneSpineOutput.bIsMoved = true;
	}

	BoneSpineOutput.BoneChainArray = Chain;
	return BoneSpineOutput;
}


/// <summary>
/// chest up
/// </summary>
const FQuadrupedBoneSpineOutput FAnimNode_CustomSpineSolver::BoneSpineProcessor_Direct(
	const FComponentSpacePoseContext& Output, 
	FTransform& EffectorTransform, 
	FCSPose<FCompactPose>& MeshBases)
{
	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	FQuadrupedBoneSpineOutput SpineOutput;
	FTransform CSEffectorTransform = EffectorTransform;

	FVector CSEffectorLocation = CSEffectorTransform.GetLocation();
	const TArray<FCompactPoseBoneIndex> BoneIndices = SpineIndiceArray;
	SpineOutput.BoneIndiceArray = BoneIndices;

	float MaximumReach = 0.0f;
	const int32 NumTransforms = BoneIndices.Num();
	SpineOutput.TempTransforms.AddUninitialized(NumTransforms);
	TArray<FSpineBoneChainLink> Chain;
	Chain.Reserve(NumTransforms);
	FTransform RootTraceTransform = MeshBases.GetComponentSpaceTransform(SpineFeetPair[0].SpineBoneRef.GetCompactPoseIndex(BoneContainer));
	const FVector LerpLocation = RootTraceTransform.GetLocation();

	const FCompactPoseBoneIndex RootIdx = RootBoneRef.GetCompactPoseIndex(BoneContainer);
	const FTransform RootTransformCS = MeshBases.GetComponentSpaceTransform(RootIdx);

	const float PelvisDistance = FMath::Abs(LerpLocation.Z - RootTransformCS.GetLocation().Z);
	FVector RootDiff = FVector::ZeroVector;
	FVector RootPosition_CS = FVector::ZeroVector;
	float OriginalHeightValue = 0.0f;
	float TerrainHeightValue = 0.0f;

	// Start with Root Bone
	{
		const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
		const FTransform& BoneCSTransform = MeshBases.GetComponentSpaceTransform(RootBoneIndex);
		FVector Bone_CSPosition = BoneCSTransform.GetLocation();
		FVector BoneWorldRootPosition = FVector(BoneCSTransform.GetLocation().X, BoneCSTransform.GetLocation().Y, 0);

		OriginalHeightValue = (Bone_CSPosition - BoneWorldRootPosition).Size();
		TerrainHeightValue = (Bone_CSPosition - SpineHitPairs[0].ParentSpinePoint).Size();
		RootPosition_CS = BoneCSTransform.GetLocation();
		SpineOutput.TempTransforms[0] = FBoneTransform(RootBoneIndex, BoneCSTransform);
		Chain.Add(FSpineBoneChainLink(RootEffectorTransform.GetLocation(), 0.f, RootBoneIndex, 0));
		SpineOutput.PelvisEffectorTransform = RootEffectorTransform;
	}

	const float FabrikHeightOffet = (TerrainHeightValue - OriginalHeightValue);

	// starting from spine_01 to effector point , loop around ...
	for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
	{
		const float t = float(TransformIndex) / float(NumTransforms - 1);
		const float w = t * t;

		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];
		const FTransform& BoneCSTransform = MeshBases.GetComponentSpaceTransform(BoneIndex);
		const FVector BonePosition_CS = BoneCSTransform.GetLocation();
		FTransform ParentTransform = MeshBases.GetComponentSpaceTransform(BoneIndices[TransformIndex - 1]);
		ParentTransform = MeshBases.GetComponentSpaceTransform(BoneIndices[TransformIndex - 1]);
		const FTransform& ParentCSTransform = ParentTransform;
		SpineOutput.TempTransforms[TransformIndex] = FBoneTransform(BoneIndex, BoneCSTransform);

		/*
		* Calculate total distance from current bone to parent bone
		*/
		const float BoneLength = FVector::Dist(BonePosition_CS, ParentCSTransform.GetLocation());

		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(FSpineBoneChainLink(BonePosition_CS - FVector(0, 0, FabrikHeightOffet), BoneLength, BoneIndex, TransformIndex));
			MaximumReach += BoneLength;
		}
		else
		{
			FSpineBoneChainLink& ParentLink = Chain[Chain.Num() - 1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}

	const float PelvicChestDistance = FVector::Dist(Chain[Chain.Num() - 1].Position, Chain[0].Position);
	const float MaximumReachTemp = (SolverComplexityType == ESolverComplexityType::Simple) ? PelvicChestDistance : MaximumReach;

	const float MaxRangeLimit = FMath::Clamp((CSEffectorLocation - Chain[0].Position).Size() / MaximumReachTemp, MinExtensionRatio, MaxExtensionRatio);
	MaxRangeLimitLerp = FMath::FInterpTo(MaxRangeLimitLerp, MaxRangeLimit, CachedDeltaSeconds, ExtensionSwitchSpeed);
	CSEffectorLocation = Chain[0].Position + (CSEffectorLocation - Chain[0].Position).GetSafeNormal() * MaximumReachTemp * MaxRangeLimitLerp;

	bool bBoneLocationUpdated = false;
	SpineOutput.bIsMoved = false;
	const float RootToTargetDistSq = FVector::DistSquared(Chain[0].Position, CSEffectorLocation);
	const int32 NumChainLinks = Chain.Num();
	SpineOutput.NumChainLinks = NumChainLinks;

	const int32 CustomIteration = (SolverComplexityType == ESolverComplexityType::Simple) ? MinIterations : MaxIterations;

	if (NumChainLinks > 1)
	{
		const int32 TipBoneLinkIndex = NumChainLinks - 1;
		float Slop = FVector::Dist(Chain[TipBoneLinkIndex].Position, CSEffectorLocation);
		if (Slop > Precision && (TipBoneLinkIndex > 0))
		{
			Chain[TipBoneLinkIndex].Position = CSEffectorLocation;

			int32 IterationCount = 0;
			while ((Slop > Precision) && (IterationCount++ < CustomIteration))
			{
				for (int32 LinkIndex = TipBoneLinkIndex - 1; LinkIndex > 0; LinkIndex--)
				{
					FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];
					FSpineBoneChainLink const& ChildLink = Chain[LinkIndex + 1];
					CurrentLink.Position = (ChildLink.Position + (CurrentLink.Position - ChildLink.Position).GetUnsafeNormal() * ChildLink.Length);
				}

				for (int32 LinkIndex = 1; LinkIndex < TipBoneLinkIndex; LinkIndex++)
				{
					FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];
					FSpineBoneChainLink const& ParentLink = Chain[LinkIndex - 1];
					CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
				}

				Slop = FMath::Abs(Chain[TipBoneLinkIndex].Length - FVector::Dist(Chain[TipBoneLinkIndex - 1].Position, CSEffectorLocation));
			}

			{
				FSpineBoneChainLink& CurrentLink = Chain[TipBoneLinkIndex];
				FSpineBoneChainLink const& ParentLink = Chain[TipBoneLinkIndex - 1];
				CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
			}
			bBoneLocationUpdated = true;
			SpineOutput.bIsMoved = true;
		}
	}
	SpineOutput.BoneChainArray = Chain;
	return SpineOutput;
}


const FQuadrupedBoneSpineOutput FAnimNode_CustomSpineSolver::BoneSpineProcessor_Snake(
	const FComponentSpacePoseContext& Output, 
	const FTransform& EffectorTransform, 
	FCSPose<FCompactPose>& MeshBases)
{

	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	FQuadrupedBoneSpineOutput BoneSpineOutput = FQuadrupedBoneSpineOutput();
	FTransform CSEffectorTransform = EffectorTransform;

	FVector CSEffectorLocation = CSEffectorTransform.GetLocation();
	const TArray<FCompactPoseBoneIndex> BoneIndices = SpineIndiceArray;
	BoneSpineOutput.BoneIndiceArray = BoneIndices;

	float MaximumReach = 0.0f;
	const int32 NumTransforms = BoneIndices.Num();
	BoneSpineOutput.TempTransforms.AddUninitialized(NumTransforms);

	TArray<FSpineBoneChainLink> Chain;
	Chain.Reserve(NumTransforms);
	FTransform PelvisTransformCS = MeshBases.GetComponentSpaceTransform(SpineFeetPair[0].SpineBoneRef.GetCompactPoseIndex(BoneContainer));

	const FCompactPoseBoneIndex RootIdx = RootBoneRef.GetCompactPoseIndex(BoneContainer);
	const FTransform RootTransformCS = MeshBases.GetComponentSpaceTransform(RootIdx);

	FVector LerpLocation = PelvisTransformCS.GetLocation();
	const float PelvisDistance = FMath::Abs(LerpLocation.Z - RootTransformCS.GetLocation().Z);
	FVector RootDiff = FVector::ZeroVector;
	FVector RootPosition_CS = FVector::ZeroVector;

	// Start with Root Bone
	{
		const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
		const FTransform& BoneCSTransform = MeshBases.GetComponentSpaceTransform(RootBoneIndex);
		RootPosition_CS = BoneCSTransform.GetLocation();
		BoneSpineOutput.TempTransforms[0] = FBoneTransform(RootBoneIndex, BoneCSTransform);
		{
			Chain.Add(FSpineBoneChainLink(RootEffectorTransform.GetLocation(), 0.f, RootBoneIndex, 0.0f));
			BoneSpineOutput.PelvisEffectorTransform = RootEffectorTransform;
		}
	}

	// starting from spine_01 to effector point , loop around ...
	for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
	{
		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];
		const FTransform& BoneCSTransform = MeshBases.GetComponentSpaceTransform(BoneIndex);
		FVector const BoneCSPosition = BoneCSTransform.GetLocation();
		FTransform ParentTrans = MeshBases.GetComponentSpaceTransform(BoneIndices[TransformIndex - 1]);
		ParentTrans = MeshBases.GetComponentSpaceTransform(BoneIndices[TransformIndex - 1]);
		const FTransform& ParentCSTransform = ParentTrans;
		BoneSpineOutput.TempTransforms[TransformIndex] = FBoneTransform(BoneIndex, BoneCSTransform);

		const float BoneLength = FVector::Dist(BoneCSPosition, ParentCSTransform.GetLocation());
		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(FSpineBoneChainLink(BoneCSPosition, BoneLength, BoneIndex, TransformIndex));
			MaximumReach += BoneLength;
		}
		else
		{
			FSpineBoneChainLink& ParentLink = Chain[Chain.Num() - 1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}


	CSEffectorLocation = Chain[0].Position + (CSEffectorLocation - Chain[0].Position).GetSafeNormal() * (MaximumReach * MaxReachIntensity);

	bool bBoneLocationUpdated = false;
	BoneSpineOutput.bIsMoved = false;
	const float RootToTargetDistSq = FVector::DistSquared(Chain[0].Position, CSEffectorLocation);
	const int32 NumChainLinks = Chain.Num();
	BoneSpineOutput.NumChainLinks = NumChainLinks;

	const int32 CustomIteration = MaxIterations;

	if (NumChainLinks > 1)
	{
		const int32 TipBoneLinkIndex = NumChainLinks - 1;
		float Slop = FVector::Dist(Chain[TipBoneLinkIndex].Position, CSEffectorLocation);

		if (Slop > Precision && TipBoneLinkIndex > 0)
		{
			Chain[TipBoneLinkIndex].Position = CSEffectorLocation;

			int32 IterationCount = 0;
			while ((Slop > Precision) && (IterationCount++ < CustomIteration))
			{
				// bw
				for (int32 LinkIndex = TipBoneLinkIndex - 1; LinkIndex > 0; LinkIndex--)
				{
					FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];
					FSpineBoneChainLink& ChildLink = Chain[LinkIndex + 1];

					if (bAtleastOneHit && bEnableSolver)
					{
						const TArray<FVector>& Data = SpineBetweenOffsetTransformArray;
						if (Data.Num() <= 0)
						{
							continue;
						}

						const FVector OldDir = (CurrentLink.Position - ChildLink.Position).GetSafeNormal();
						const FVector ToTarget = (Data[LinkIndex - 1] - ChildLink.Position);

						FVector GuideDir = ToTarget.GetSafeNormal();
						if (GuideDir.IsNearlyZero())
						{
							GuideDir = OldDir; // フォールバック
						}

						CurrentLink.Position = ChildLink.Position + GuideDir * ChildLink.Length;
					}
					else
					{
						CurrentLink.Position = (ChildLink.Position + (CurrentLink.Position - ChildLink.Position).GetUnsafeNormal() * ChildLink.Length);
					}
				}

				// fw
				for (int32 LinkIndex = 1; LinkIndex < TipBoneLinkIndex; LinkIndex++)
				{
					FSpineBoneChainLink& ParentLink = Chain[LinkIndex - 1];
					FSpineBoneChainLink& CurrentLink = Chain[LinkIndex];

					if (bAtleastOneHit && bEnableSolver)
					{
						const TArray<FVector>& Data = SpineBetweenOffsetTransformArray;
						if (Data.Num() <= 0)
						{
							continue;
						}

						const FVector OldDir = (CurrentLink.Position - ParentLink.Position).GetSafeNormal();
						const FVector ToTarget = (Data[LinkIndex - 1] - ParentLink.Position);

						FVector GuideDir = ToTarget.GetSafeNormal();
						if (GuideDir.IsNearlyZero())
						{
							GuideDir = OldDir; // フォールバック
						}

						CurrentLink.Position = ParentLink.Position + GuideDir * CurrentLink.Length;
					}
					else
					{
						CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
					}
				}

				Slop = FMath::Abs(Chain[TipBoneLinkIndex].Length - FVector::Dist(Chain[TipBoneLinkIndex - 1].Position, CSEffectorLocation));
			}

			// Place tip bone based on how close we got to target.
			{
				FSpineBoneChainLink const& ParentLink = Chain[TipBoneLinkIndex - 1];
				FSpineBoneChainLink& CurrentLink = Chain[TipBoneLinkIndex];
				CurrentLink.Position = (ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length);
			}
			bBoneLocationUpdated = true;
			BoneSpineOutput.bIsMoved = true;
		}
	}

	for (int32 LinkIndex = 0; LinkIndex < Chain.Num(); LinkIndex++)
	{
		if (!SnakeSpinePositionArray.IsValidIndex(LinkIndex))
		{
			continue;
		}
		SnakeSpinePositionArray[LinkIndex] = Chain[LinkIndex].Position;
	}

	BoneSpineOutput.BoneChainArray = Chain;
	return BoneSpineOutput;
}


const FQuadrupedBoneSpineOutput FAnimNode_CustomSpineSolver::BoneSpineProcessor_Transform(
	FQuadrupedBoneSpineOutput& BoneSpine, 
	const FComponentSpacePoseContext& Output, 
	FCSPose<FCompactPose>& MeshBases)
{

	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();

	for (int32 LinkIndex = 0; LinkIndex < BoneSpine.NumChainLinks; LinkIndex++)
	{
		FSpineBoneChainLink const& ChainLink = BoneSpine.BoneChainArray[LinkIndex];
		const FCompactPoseBoneIndex& ModifyBoneIndex = BoneSpine.BoneIndiceArray[ChainLink.TransformIndex];
		FTransform ComponentBoneTransform;
		ComponentBoneTransform = MeshBases.GetComponentSpaceTransform(ModifyBoneIndex);
		FTransform ChainTransform = FTransform::Identity;

		if (bSpineSnakeBone)
		{
			if (!SnakeSpinePositionArray.IsValidIndex(LinkIndex))
			{
				continue;
			}
			ChainTransform.SetLocation(SnakeSpinePositionArray[LinkIndex]);
		}
		else
		{
			ChainTransform.SetLocation(ChainLink.Position);
		}

		BoneSpine.TempTransforms[ChainLink.TransformIndex].Transform.SetTranslation(ChainTransform.GetLocation());
		const int32 NumChildren = ChainLink.ChildZeroLengthTransformIndices.Num();
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			BoneSpine.TempTransforms[ChainLink.ChildZeroLengthTransformIndices[ChildIndex]].Transform.SetTranslation(ChainLink.Position);
		}

	}

	FRotator InitialRotatorDelta = FRotator::ZeroRotator;
	bool bIsChainSwapped = false;

	do
	{
		bIsChainSwapped = false;
		for (int32 Index = 1; Index < BoneSpine.BoneChainArray.Num(); Index++)
		{
			if (BoneSpine.BoneChainArray[Index - 1].BoneIndex > BoneSpine.BoneChainArray[Index].BoneIndex)
			{
				FSpineBoneChainLink BoneChainLink = BoneSpine.BoneChainArray[Index - 1];
				BoneSpine.BoneChainArray[Index - 1] = BoneSpine.BoneChainArray[Index];
				BoneSpine.BoneChainArray[Index] = BoneChainLink;
				bIsChainSwapped = true;
			}
		}
	} while (bIsChainSwapped);

	// FABRIK - 並進計算後の骨局所軸の再方向付け
	for (int32 LinkIndex = 0; LinkIndex < BoneSpine.NumChainLinks - 1; LinkIndex++)
	{
		const FSpineBoneChainLink& CurrentLink = BoneSpine.BoneChainArray[LinkIndex];
		const FSpineBoneChainLink& ChildLink = BoneSpine.BoneChainArray[LinkIndex + 1];

		const FVector OldDir = (GetCurrentLocation(MeshBases, ChildLink.BoneIndex) - GetCurrentLocation(MeshBases, CurrentLink.BoneIndex)).GetUnsafeNormal();
		const FVector NewDir = (ChildLink.Position - CurrentLink.Position).GetUnsafeNormal();
		const FVector RotationAxis = FVector::CrossProduct(OldDir, NewDir).GetSafeNormal();
		const float RotationAngle = FMath::Acos(FVector::DotProduct(OldDir, NewDir));
		const FQuat DeltaRotation = FQuat(RotationAxis, RotationAngle);
		checkSlow(DeltaRotation.IsNormalized());

		FTransform& CurrentBoneTransform = BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform;
		const FTransform ConstBoneTransform = BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform;

		if (LinkIndex == 0 && !bFeetIsEmpty)
		{
			const FRotator DirectionRotator = BoneRelativeConversion(CurrentLink.BoneIndex, BoneSpine.PelvisEffectorTransform.Rotator(), BoneContainer, MeshBases);
			BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform.SetRotation(DirectionRotator.Quaternion());
		}
		else
		{
			CurrentBoneTransform.SetRotation(FQuat::Slerp(CurrentBoneTransform.GetRotation(), (DeltaRotation * 1.0f) * CurrentBoneTransform.GetRotation(), RotationPowerBetween));
			BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform = CurrentBoneTransform;
		}

		const int32 NumChildren = CurrentLink.ChildZeroLengthTransformIndices.Num();
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			FTransform& ChildBoneTransform = BoneSpine.TempTransforms[CurrentLink.ChildZeroLengthTransformIndices[ChildIndex]].Transform;
			ChildBoneTransform.NormalizeRotation();
		}
	}

	if (BoneSpine.BoneChainArray.Num() > 0)
	{
		FSpineBoneChainLink const& CurrentLink = BoneSpine.BoneChainArray[BoneSpine.NumChainLinks - 1];

		if ((BoneSpine.NumChainLinks - 2) > 0)
		{
			FSpineBoneChainLink const& ChildLink = BoneSpine.BoneChainArray[BoneSpine.NumChainLinks - 2];
		}

		const FRotator DirectionRotator = BoneRelativeConversion(CurrentLink.BoneIndex, BoneSpine.SpineFirstEffectorTransform.Rotator(), BoneContainer, MeshBases);

		FQuat FabrikRotation = BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform.GetRotation();
		const FQuat& TargetQuat = DirectionRotator.Quaternion();
		const FQuat& FinalQuat = FQuat::Slerp(FabrikRotation, TargetQuat, RotationPowerBetween);
		BoneSpine.TempTransforms[CurrentLink.TransformIndex].Transform.SetRotation(FinalQuat);
	}

	return BoneSpine;
}


#pragma region Misc
TArray<FQuadrupedBone_SpineFeetPair> FAnimNode_CustomSpineSolver::Swap_SpineFeetPairArray(TArray<FQuadrupedBone_SpineFeetPair>& OutSpineFeetPair)
{
	bool bHasResult = false;

	do
	{
		bHasResult = false;
		for (int32 JIndex = 1; JIndex < OutSpineFeetPair.Num(); JIndex++)
		{
			for (int32 Index = 1; Index < OutSpineFeetPair[JIndex].FeetArray.Num(); Index++)
			{
				if (OutSpineFeetPair[JIndex].FeetArray[Index - 1].BoneIndex < OutSpineFeetPair[JIndex].FeetArray[Index].BoneIndex)
				{
					FBoneReference Instance = OutSpineFeetPair[JIndex].FeetArray[Index - 1];
					OutSpineFeetPair[JIndex].FeetArray[Index - 1] = OutSpineFeetPair[JIndex].FeetArray[Index];
					OutSpineFeetPair[JIndex].FeetArray[Index] = Instance;
					bHasResult = true;
				}
			}
		}
	} while (bHasResult);
	return OutSpineFeetPair;
}

const TArray<FName> FAnimNode_CustomSpineSolver::BoneArrayMachine(
	const FBoneContainer& RequiredBones,
	const int32 Index,
	const FName& StartBoneName,
	const FName& EndBoneName,
	const FName& ThighBoneName,
	const bool bWasFootBone)
{
	TArray<FName> SpineBoneArray;
	SpineBoneArray.Add(StartBoneName);


	if (!bWasFootBone)
	{
		FQuadrupedBone_SpineFeetPair Instance;
		Instance.SpineBoneRef = FBoneReference(StartBoneName);
		Instance.SpineBoneRef.Initialize(RequiredBones);
		SpineFeetPair.Add(Instance);
	}

	const FReferenceSkeleton& RefSkel = RequiredBones.GetReferenceSkeleton();

	bool bHasFinish = false;
	int32 IterationCount = 0;
	constexpr int32 MaxIterationCount = ITERATION_COUNTER;

	// 親方向に登っていく
	do
	{
		if (bWasFootBone)
		{
			if (CheckLoopExist(
				RequiredBones,
				SolverInputData.FeetBones[Index].FeetTraceOffset,
				SolverInputData.FeetBones[Index].FeetHeight,
				StartBoneName,
				SpineBoneArray.Last(),
				ThighBoneName,
				TotalSpineNameArray))
			{
				return SpineBoneArray;
			}
		}


		IterationCount++;

		SpineBoneArray.Add(owning_skel->GetParentBone(SpineBoneArray[IterationCount - 1]));

		if (!bWasFootBone)
		{
			FQuadrupedBone_SpineFeetPair Instance;
			Instance.SpineBoneRef = FBoneReference(SpineBoneArray[SpineBoneArray.Num() - 1]);
			Instance.SpineBoneRef.Initialize(RequiredBones);
			SpineFeetPair.Add(Instance);
		}

		if (SpineBoneArray[SpineBoneArray.Num() - 1] == EndBoneName && !bWasFootBone)
		{
			return SpineBoneArray;
		}

	} while (IterationCount < MaxIterationCount && !bHasFinish);

	return SpineBoneArray;
}


const bool FAnimNode_CustomSpineSolver::CheckLoopExist(
	const FBoneContainer& RequiredBones,
	const FVector& FeetTraceOffset,
	const float FeetHeight,
	const FName& StartBoneName,
	const FName& InputBoneName,
	const FName& ThighBoneName,
	TArray<FName>& OutTotalSpineBoneArray)
{
	for (int32 Index = 0; Index < OutTotalSpineBoneArray.Num(); Index++)
	{
		const FName& CurBoneName = OutTotalSpineBoneArray[Index];

		if (InputBoneName == CurBoneName)
		{
			FQuadrupedBone_SpineFeetPair Instance;
			Instance.SpineBoneRef = FBoneReference(OutTotalSpineBoneArray[Index]);
			Instance.SpineBoneRef.Initialize(RequiredBones);

			FBoneReference FootBoneRef = FBoneReference(StartBoneName);
			FootBoneRef.Initialize(RequiredBones);
			Instance.FeetArray.Add(FootBoneRef);

			SpineFeetPair[Index].SpineBoneRef = Instance.SpineBoneRef;
			SpineFeetPair[Index].FeetArray.Add(FootBoneRef);
			SpineFeetPair[Index].FeetHeightArray.Add(FeetHeight);
			SpineFeetPair[Index].FeetTraceOffsetArray.Add(FeetTraceOffset);

			if (!ThighBoneName.IsNone())
			{
				FBoneReference ThighBoneRef = FBoneReference(ThighBoneName);
				ThighBoneRef.Initialize(RequiredBones);
				SpineFeetPair[Index].ThighArray.Add(ThighBoneRef);
			}
			return true;
		}
	}
	return false;
}

FRotator FAnimNode_CustomSpineSolver::BoneRelativeConversion(const FCompactPoseBoneIndex& ModifyBoneIndex, const FRotator& TargetRotation, const FBoneContainer& BoneContainer, FCSPose<FCompactPose>& MeshBases) const
{
	const FTransform& NewBoneTransform = MeshBases.GetComponentSpaceTransform(ModifyBoneIndex);
	FRotator Rotation = TargetRotation;
	Rotation.Yaw = 0.0f;
	return FRotator(Rotation.Quaternion() * NewBoneTransform.Rotator().Quaternion());
}

float FAnimNode_CustomSpineSolver::CalcInterpSpeed(const FVector& CurPos, const FVector& TargetPos) const
{
	const float Distance = FVector::Dist(CurPos, TargetPos);
	const float DynamicAlpha = FMath::Clamp(Distance / TeleportThreshold, 0.1f, 1.0f);
	const float FinalSpeed = FormatLocationLerp * DynamicAlpha;
	return FinalSpeed;
}

float FAnimNode_CustomSpineSolver::CalcInterpRotationSpeed(const FQuat& CurRot, const FQuat& TargetRot) const
{
	FRotator Cur = CurRot.Rotator();
	FRotator Tgt = TargetRot.Rotator();

	float PitchDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(Cur.Pitch, Tgt.Pitch));
	float RollDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(Cur.Roll, Tgt.Roll));

	float MaxError = FMath::Max(PitchDiff, RollDiff);

	const FVector2D DeadZone = FVector2D(0.5f, 5.0f);
	float RotationErrorAlpha = FMath::GetMappedRangeValueClamped(
		DeadZone,
		FVector2D(0.1f, 1.0f),
		MaxError
	);

	return FormatRotationLerp * RotationErrorAlpha;
}

bool FAnimNode_CustomSpineSolver::IsMovingBase(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}
	if (UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		const FVector V = Comp->GetComponentVelocity();
		const float LinearSpeed = V.Size();

		const FVector PhysAngV = Comp->GetPhysicsAngularVelocityInDegrees();
		const float PhysAngularSpeed = PhysAngV.Size();

		const FRotator CurRot = Comp->GetComponentRotation();

		if (bDisplayLineTrace)
		{
			//UE_LOG(LogQuadrupedIK, Log, TEXT("BaseMoveCheck -> Vel: %.3f, P_AngV: %.3f, Rot: %s"), LinearSpeed, PhysAngularSpeed, *CurRot.ToString());
		}

		const float RotVelThresholdDegPerSec = 0.f;
		const bool bIsMoving = (LinearSpeed > VelThresholdCmPerSec) || (PhysAngularSpeed > RotVelThresholdDegPerSec);
		return bIsMoving;
	}
	return false;
}
#pragma endregion


void FAnimNode_CustomSpineSolver::CalcParentHitResult(const FAnimationUpdateContext& Context, const FHitResult& InHitResult, const FVector RelativePos, FVector& OutHitPoint)
{
	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();

	if (InHitResult.bBlockingHit)
	{
		const float LocalFormatLocationLerp = FormatLocationLerp * 0.5f;

		//OutHitPoint = InHitResult.ImpactPoint;
		FVector SpiralPoint = OutHitPoint;
		SpiralPoint = ComponentToWorld.InverseTransformPosition(SpiralPoint);
		const FVector ImpactPointInverse = ComponentToWorld.InverseTransformPosition(InHitResult.ImpactPoint);

		SpiralPoint.X = ImpactPointInverse.X;
		SpiralPoint.Y = ImpactPointInverse.Y;
		SpiralPoint = ComponentToWorld.TransformPosition(SpiralPoint);
		OutHitPoint = SpiralPoint;

		OutHitPoint = FMath::VInterpTo(
			OutHitPoint,
			InHitResult.ImpactPoint,
			CachedDeltaSeconds, LocalFormatLocationLerp);
	}
	else
	{
		FVector SpiralPoint = RelativePos;
		SpiralPoint = ComponentToWorld.InverseTransformPosition(SpiralPoint);
		//SpiralPoint.Z = 0.0f;
		SpiralPoint = ComponentToWorld.TransformPosition(SpiralPoint);
		OutHitPoint = SpiralPoint;
	}
}

FSpineSupportData FAnimNode_CustomSpineSolver::BuildSpineSupportData(const FComponentSpacePoseContext& Output, const FCSPose<FCompactPose>& MeshBases, const int32 PointIndex) const
{
	FSpineSupportData Data;

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FQuadlupedBoneHitPairs& HitPair = SpineHitPairs[PointIndex];

	Data.ParentSpineHit = HitPair.ParentSpineHit;
	Data.ParentSpineCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentSpinePoint);
	Data.FrontCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentFrontPoint);
	Data.BackCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentBackPoint);
	Data.LeftCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentLeftPoint);
	Data.RightCS = ComponentToWorld.InverseTransformPosition(HitPair.ParentRightPoint);

	Data.bHasParentHit = HitPair.ParentSpineHit.bBlockingHit;
	Data.bHasFrontBack = HitPair.ParentFrontHit.bBlockingHit && HitPair.ParentBackHit.bBlockingHit;
	Data.bHasLeftRight = HitPair.ParentLeftHit.bBlockingHit && HitPair.ParentRightHit.bBlockingHit;

	Data.ForwardCrossZ = Data.BackCS.Z - Data.FrontCS.Z;

	Data.LowestFootCS = FVector(0, 0, BIG_NUMBER);
	Data.HighestFootCS = FVector(0, 0, -BIG_NUMBER);

	for (const FVector& FootHitWS : HitPair.FeetHitPointArray)
	{
		const FVector FootCS = ComponentToWorld.InverseTransformPosition(FootHitWS);
		Data.FootHitCount++;

		if (FootCS.Z < Data.LowestFootCS.Z)
		{
			Data.LowestFootCS = FootCS;
		}

		if (FootCS.Z > Data.HighestFootCS.Z)
		{
			Data.HighestFootCS = FootCS;
		}
	}

	if (Data.FootHitCount == 0)
	{
		Data.LowestFootCS = Data.ParentSpineCS;
		Data.HighestFootCS = Data.ParentSpineCS;
	}

	Data.LowestFootZ = Data.LowestFootCS.Z;
	Data.HighestFootZ = Data.HighestFootCS.Z;

	const int32 OppositeIndex = (PointIndex == 0) ? SpineHitPairs.Num() - 1 : 0;
	Data.OppositeSpineCS = ComponentToWorld.InverseTransformPosition(SpineHitPairs[OppositeIndex].ParentSpinePoint);
	Data.OppositeCrossZ = Data.OppositeSpineCS.Z - Data.ParentSpineCS.Z;

	return Data;
}


FVector FAnimNode_CustomSpineSolver::ComputeSpineTargetLocationCS(const FComponentSpacePoseContext& Output, const FSpineSupportData& Support, const int32 PointIndex, const bool bIsPelvisJoint) const
{
	const float AdaptiveGravity = bIsPelvisJoint
		? (bIsCrouchMode ? CrouchPelvisAdaptiveGravity : PelvisAdaptiveGravity)
		: (bIsCrouchMode ? CrouchChestAdaptiveGravity : ChestAdaptiveGravity);

	const float GravityAlpha = FMath::Clamp((AdaptiveGravity * 0.5f) + 0.5f, 0.0f, 1.0f);

	// まず「地面基準Z」を1つだけ作る
	const float BaseGroundZ = FMath::Lerp(Support.LowestFootZ, Support.HighestFootZ, GravityAlpha);

	float DipOffsetZ = 0.0f;
	if (Support.FootHitCount == 2)
	{
		const float HeightDelta = FMath::Abs(Support.HighestFootZ - Support.LowestFootZ);
		if (bIsPelvisJoint)
		{
			DipOffsetZ = HeightDelta * DipMultiplier;
		}
		else
		{
			DipOffsetZ = HeightDelta * ChestSideDipMultiplier;
		}
	}

	float BaseOffset = 0.0f;
	if (bIsCrouchMode)
	{
		BaseOffset = bIsPelvisJoint ? CrouchedPelvisBaseOffset : CrouchedChestBaseOffset;
	}
	else
	{
		BaseOffset = bIsPelvisJoint ? PelvisBaseOffset : ChestBaseOffset;
	}

	const float SpineHeight = TotalSpineHeights[PointIndex];
	float TargetZ = BaseGroundZ + SpineHeight + BaseOffset - DipOffsetZ;

	const float MaxDip = bIsPelvisJoint ? MaxFormatedHeight : MaxFormatedDipHeightChest;
	const float OriginZ = Support.ParentSpineCS.Z + SpineHeight + BaseOffset;
	TargetZ = FMath::Clamp(TargetZ, OriginZ - MaxDip, OriginZ + MaxDip);

	return FVector(Support.ParentSpineCS.X, Support.ParentSpineCS.Y, TargetZ);
}


/// <summary>
/// ImpactNormalをCSに変換し、FMath::Atan2 を用いて傾斜角を求める
/// </summary>
FRotator FAnimNode_CustomSpineSolver::ComputeSpineTargetRotation(
	const FComponentSpacePoseContext& Output,
	const FSpineSupportData& Support, 
	const int32 PointIndex,
	const bool bIsPelvisJoint,
	const FRotator& CurrentYawOnly) const
{
	FRotator Target = CurrentYawOnly;

	if (!Support.bHasParentHit)
	{
		return Target.GetNormalized();
	}

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	const FVector GroundNormalWS = Support.ParentSpineHit.ImpactNormal.GetSafeNormal();

	FVector GroundNormalCS = ComponentToWorld.InverseTransformVectorNoScale(GroundNormalWS).GetSafeNormal();

	if (GroundNormalCS.IsNearlyZero())
	{
		return Target.GetNormalized();
	}

	const FVector UpCS = CharacterDirectionVectorCS.GetSafeNormal();
	const FVector ForwardCS = ComponentToWorld.TransformVector(ForwardDirectionVector).GetSafeNormal();

	if (ForwardCS.IsNearlyZero())
	{
		return Target.GetNormalized();
	}

	const FVector RightCS = FVector::CrossProduct(UpCS, ForwardCS).GetSafeNormal();

	const float NormalUp = FMath::Max(KINDA_SMALL_NUMBER, FVector::DotProduct(GroundNormalCS, UpCS)); 

	// forward 方向の傾き。坂の上り下り。
	const float ForwardSlopeRad = FMath::Atan2(FVector::DotProduct(GroundNormalCS, ForwardCS), NormalUp);

	// side 方向の傾き。左右傾き。
	const float SideSlopeRad = FMath::Atan2(FVector::DotProduct(GroundNormalCS, RightCS), NormalUp);

	float ForwardSlopeDeg = FMath::RadiansToDegrees(ForwardSlopeRad);
	float SideSlopeDeg = FMath::RadiansToDegrees(SideSlopeRad);

	const float ForwardIntensity = bIsPelvisJoint ? PelvisForwardRotationIntensity : ChestForwardRotationIntensity;
	const float SideIntensity = bIsPelvisJoint ? PelvisSidewardRotationIntensity : ChestSidewardRotationIntensity;

	ForwardSlopeDeg *= ForwardIntensity;
	SideSlopeDeg *= SideIntensity;

	Target.Roll = FMath::Clamp(-ForwardSlopeDeg, PitchRange.X, PitchRange.Y);
	Target.Pitch = FMath::Clamp(SideSlopeDeg, RollRange.X, RollRange.Y);

	return Target.GetNormalized();
}


void FAnimNode_CustomSpineSolver::ApplySpineTarget(
	const FComponentSpacePoseContext& Output,
	FTransform& OutputTransform, 
	const FVector& TargetLocationCS,
	const FRotator& TargetRotation,
	const float LocationInterpSpeed,
	const float RotationInterpSpeed,
	const bool bEnableLocationLerp,
	const bool bEnableRotationLerp) const
{

	FVector NewLocation = TargetLocationCS;

	if (bEnableLocationLerp && !bIgnoreLerping)
	{
		const FVector CurLocation = OutputTransform.GetLocation();
		const float Speed = CalcInterpSpeed(CurLocation, NewLocation);
		NewLocation.Z = FMath::FInterpTo(CurLocation.Z, TargetLocationCS.Z, CachedDeltaSeconds, LocationInterpSpeed);
	}

	OutputTransform.SetLocation(NewLocation);

	if (bEnableRotationLerp && !bIgnoreLerping)
	{
		const float Speed = CalcInterpRotationSpeed(FQuat(OutputTransform.Rotator()), FQuat(TargetRotation));
		const FRotator NewRot = FMath::RInterpTo(OutputTransform.Rotator(), TargetRotation, CachedDeltaSeconds, Speed);
		OutputTransform.SetRotation(NewRot.Quaternion().GetNormalized());
	}
	else
	{
		OutputTransform.SetRotation(TargetRotation.Quaternion().GetNormalized());
	}
}


float FAnimNode_CustomSpineSolver::EvalMidSlopeResidualCS(const FComponentSpacePoseContext& Output) const
{
	if (SpineHitPairs.Num() < 2)
	{
		return 0.0f;
	}

	const int32 MidNum = FMath::Min3(SpinePointBetweenArray.Num(), SpineHitBetweenArray.Num(), SpineBetweenTransformArray.Num());

	if (MidNum <= 0)
	{
		return 0.0f;
	}

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	const FVector PelvisCS = ComponentToWorld.InverseTransformPosition(SpineHitPairs[0].ParentSpinePoint);
	const FVector ChestCS = ComponentToWorld.InverseTransformPosition(SpineHitPairs.Last().ParentSpinePoint);

	float ResidualSum = 0.0f;
	int32 ValidCount = 0;

	for (int32 i = 0; i < MidNum; ++i)
	{
		const FHitResult& MidHit = SpineHitBetweenArray[i];
		if (!MidHit.bBlockingHit)
		{
			continue;
		}

		const float T = float(i + 1) / float(MidNum + 1);
		const float ExpectedZ = FMath::Lerp(PelvisCS.Z, ChestCS.Z, T);

		const FVector MidCS = ComponentToWorld.InverseTransformPosition(SpinePointBetweenArray[i]);
		ResidualSum += FMath::Abs(MidCS.Z - ExpectedZ);
		++ValidCount;
	}

	if (ValidCount <= 0)
	{
		return 0.0f;
	}

	return ResidualSum / float(ValidCount);
}

bool FAnimNode_CustomSpineSolver::IsContinuousSlope(const FComponentSpacePoseContext& Output, float Threshold) const
{
	if (!bEnableMidSlopeContinuityGuard)
	{
		return true;
	}

	return EvalMidSlopeResidualCS(Output) <= Threshold;
}

void FAnimNode_CustomSpineSolver::ResetTraceResults()
{
	bAtleastOneHit = false;

	for (FQuadlupedBoneHitPairs& HitPair : SpineHitPairs)
	{
		HitPair.ParentSpineHit.Init();
		HitPair.ParentFrontHit.Init();
		HitPair.ParentBackHit.Init();
		HitPair.ParentLeftHit.Init();
		HitPair.ParentRightHit.Init();
		HitPair.ParentSpinePoint = FVector::ZeroVector;
		HitPair.ParentFrontPoint = FVector::ZeroVector;
		HitPair.ParentBackPoint = FVector::ZeroVector;
		HitPair.ParentLeftPoint = FVector::ZeroVector;
		HitPair.ParentRightPoint = FVector::ZeroVector;

		for (FHitResult& FootHit : HitPair.FeetHitArray) 
		{ 
			FootHit.Init();
		}
		for (FVector& FootPoint : HitPair.FeetHitPointArray)
		{
			FootPoint = FVector::ZeroVector;
		}
	}

	for (FHitResult& Hit : SpineHitBetweenArray)
	{
		Hit.Init();
	}
	for (FVector& Point : SpinePointBetweenArray)
	{
		Point = FVector::ZeroVector;
	}
	for (FVector& Offset : SpineBetweenOffsetTransformArray)
	{

		Offset = FVector::ZeroVector; 
	}

	//PelvisSlopeStabAlpha = 0.0f;
	//ChestSlopeStabAlpha = 0.0f;
	//PelvisBaseOffset = 0.0f;

	//bRequireSnap = true;
}


void FAnimNode_CustomSpineSolver::ApplyLineTraceCached(
	const FAnimationUpdateContext& Context,
	const FQuadrupedIKTraceKey& Key,
	const FVector& Origin,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& OutHitResult,
	const FLinearColor& DebugColor,
	const bool bDrawLine)
{
	OutHitResult.Init();

	if (TraceSharedState.IsValid())
	{
		TraceSharedState->TryGetCachedHit(Key, OutHitResult);
	}

	const float Radius = TraceRadiusValue * ComponentScale;
	if (!IsValidTraceInput(StartLocation, EndLocation, Radius))
	{
		return;
	}

	if (!TraceSharedState.IsValid())
	{
		return;
	}

	const bool bShouldDispatch = TraceSharedState->MarkPendingIfNeeded(Key);
	if (bShouldDispatch)
	{
		RequestTraceOnGameThread(
			Context,
			Key,
			StartLocation,
			EndLocation,
			DebugColor);
	}

	if (bDrawLine)
	{
		TraceStartList.Add(StartLocation);
		TraceEndList.Add(EndLocation);
		TraceLinearColor.Add(DebugColor.ToFColor(true));
	}
}

FQuadrupedIKTraceKey FAnimNode_CustomSpineSolver::MakeTraceKey(
	int32 TraceGroup,
	int32 TraceIndex,
	int32 TraceSubIndex) const
{
	FQuadrupedIKTraceKey Key;
	Key.A = TraceGroup;
	Key.B = TraceIndex;
	Key.C = TraceSubIndex;
	return Key;
}


void FAnimNode_CustomSpineSolver::RequestTraceOnGameThread(
	const FAnimationUpdateContext& Context,
	const FQuadrupedIKTraceKey& Key,
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FLinearColor& DebugColor)
{
	USkeletalMeshComponent* SK = Context.AnimInstanceProxy ? Context.AnimInstanceProxy->GetSkelMeshComponent() : nullptr;

	if (!SK || !TraceSharedState.IsValid())
	{
		return;
	}

	TWeakObjectPtr<USkeletalMeshComponent> WeakSK(SK);
	TSharedPtr<FQuadrupedIKTraceSharedState, ESPMode::ThreadSafe> SharedState = TraceSharedState;

	const EIKRaycastType LocalRaycastTraceType = RaycastTraceType;
	const TEnumAsByte<ETraceTypeQuery> LocalTraceChannel = Trace_Channel;
	const TArray<AActor*> LocalIgnoreActors = IgnoreActors;
	const float LocalNormalDotThreshold = NormalDotThreshold;
	const float LocalRadius = TraceRadiusValue * ComponentScale;
	const float LocalComponentScale = ComponentScale;

	FFunctionGraphTask::CreateAndDispatchWhenReady(
		[
			WeakSK,
			SharedState,
			Key,
			StartLocation,
			EndLocation,
			DebugColor,
			LocalRaycastTraceType,
			LocalTraceChannel,
			LocalIgnoreActors,
			LocalNormalDotThreshold,
			LocalRadius,
			LocalComponentScale
		]()
	{
		if (!WeakSK.IsValid() || !SharedState.IsValid())
		{
			return;
		}

		USkeletalMeshComponent* SKPtr = WeakSK.Get();
		UWorld* World = SKPtr ? SKPtr->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}

		if (!SpineSolverHelper::IsFiniteVector(StartLocation) || !SpineSolverHelper::IsFiniteVector(EndLocation))
		{
			return;
		}

		TArray<FHitResult> HitResults;
		const EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::None;

		switch (LocalRaycastTraceType)
		{
		case EIKRaycastType::LineTrace:
		{
			UKismetSystemLibrary::LineTraceMulti(
				World,
				StartLocation,
				EndLocation,
				LocalTraceChannel,
				true,
				LocalIgnoreActors,
				DebugTrace,
				HitResults,
				true,
				DebugColor);
		}
		break;

		case EIKRaycastType::SphereTrace:
		{
			const float SafeRadius = FMath::Max(LocalRadius, 1.0f);

			UKismetSystemLibrary::SphereTraceMulti(
				World,
				StartLocation,
				EndLocation,
				SafeRadius,
				LocalTraceChannel,
				true,
				LocalIgnoreActors,
				DebugTrace,
				HitResults,
				true,
				DebugColor);
		}
		break;

		case EIKRaycastType::BoxTrace:
		{
			const float SafeRadius = FMath::Max(LocalRadius, 1.0f);
			const FVector Extent(SafeRadius, SafeRadius, FMath::Max(1.0f, SafeRadius * 0.25f));

			UKismetSystemLibrary::BoxTraceMulti(
				World,
				StartLocation,
				EndLocation,
				Extent,
				FRotator::ZeroRotator,
				LocalTraceChannel,
				true,
				LocalIgnoreActors,
				DebugTrace,
				HitResults,
				true,
				DebugColor);
		}
		break;
		}

		FHitResult BestHit;
		UQuadrupedIKLibrary::GetSimpleHitResult(
			HitResults,
			LocalNormalDotThreshold,
			BestHit);

		SharedState->CompleteTrace(Key, BestHit);
	},
		TStatId(),
		nullptr,
		ENamedThreads::GameThread);
}


bool FAnimNode_CustomSpineSolver::IsValidTraceInput(
	const FVector& StartLocation,
	const FVector& EndLocation,
	float Radius) const
{
	if (!SpineSolverHelper::IsFiniteVector(StartLocation) || !SpineSolverHelper::IsFiniteVector(EndLocation))
	{
		return false;
	}

	if (StartLocation.Equals(EndLocation, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	if (!FMath::IsFinite(Radius) || Radius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return true;
}

