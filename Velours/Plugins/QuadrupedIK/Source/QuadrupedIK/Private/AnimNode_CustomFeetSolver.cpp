// Copyright 2022 wevet works All Rights Reserved.

#include "AnimNode_CustomFeetSolver.h"
#include "PredictionAnimInstance.h"
#include "QuadrupedIKLibrary.h"
#include "QuadrupedIK.h"


#include "Animation/AnimInstanceProxy.h"
#include "DrawDebugHelpers.h"
#include "AnimationRuntime.h"
#include "AnimationCoreLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
//#include "Kismet/KismetMathLibrary.h"
#include "CollisionQueryParams.h"
#include "Animation/InputScaleBias.h"
#include "FABRIK.h"
#include "Algo/Reverse.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"


DECLARE_CYCLE_STAT(TEXT("CustomFeetSolver Eval"), STAT_CustomFeetSolver_Eval, STATGROUP_Anim);

#define ITERATION_COUNTER 50
#define DEV_UPDATE 1

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_CustomFeetSolver)


namespace FeetSolverHelper
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
}

FAnimNode_CustomFeetSolver::FAnimNode_CustomFeetSolver()
{
	bIsInitialized = false;

	FRichCurve* InterpolationVelocityCurveData = InterpolationVelocityCurve.GetRichCurve();
	InterpolationVelocityCurveData->AddKey(0.0f, 1.0f);
	InterpolationVelocityCurveData->AddKey(1500.f, 10.0f);

	FRichCurve* ComplexSimpleFootVelocityCurveData = ComplexSimpleFootVelocityCurve.GetRichCurve();
	ComplexSimpleFootVelocityCurveData->AddKey(0.0f, 1.00f);
	ComplexSimpleFootVelocityCurveData->AddKey(100.f, 0.f);

	FRichCurve* FingerAlphaVelocityCurveData = FingerVelocityCurve.GetRichCurve();
	FingerAlphaVelocityCurveData->AddKey(0.0f, 1.0f);
	FingerAlphaVelocityCurveData->AddKey(100.f, 0.0f);
}

void FAnimNode_CustomFeetSolver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);
	if (Context.AnimInstanceProxy)
	{
		owning_skel = Context.AnimInstanceProxy->GetSkelMeshComponent();
		IgnoreActors.Add(Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner());
	}
	BlendRefPose.Initialize(Context);

	PreviousMovementDirection = FVector::ZeroVector;
	DirectionChangeAlpha = 0.0f;
	DirectionChangeSmoothing = 1.0f;

	// 前フレームの足位置配列を初期化
	PreviousFeetLocations.Empty();

	for (int32 Index = 0; Index < SolverInputData.FeetBones.Num(); ++Index)
	{
		TArray<FVector> FootLocArray;

		for (int32 JIdx = 0; JIdx < SolverInputData.FeetBones[Index].FingerBoneArray.Num(); ++JIdx)
		{
			FootLocArray.Add(FVector::ZeroVector);
		}
		PreviousFeetLocations.Add(FootLocArray);
	}
}


void FAnimNode_CustomFeetSolver::GetResetedPoseInfo(FCSPose<FCompactPose>& MeshBases)
{
	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();

	const int32 NumFeet = SolverInputData.FeetBones.Num();

	// knee trans
	for (int32 i = 0; i < NumFeet; i++)
	{
		const FBoneReference& KneeBoneRef = KneeBoneRefArray[i];

		if (KneeBoneRef.IsValidToEvaluate() && KneeAnimatedTransformArray.IsValidIndex(i))
		{
			KneeAnimatedTransformArray[i] = MeshBases.GetComponentSpaceTransform(KneeBoneRef.CachedCompactPoseIndex);
		}
	}

}


void FAnimNode_CustomFeetSolver::CalculateFeetRotation(FComponentSpacePoseContext& Output, TArray<TArray<FTransform>> FeetRotationArray)
{
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector UpWS = ComponentToWorld.GetUnitAxis(EAxis::Z);

	check(SpineFeetPair.Num() == SpineHitPairs.Num());


	for (int32 i = 0; i < SpineHitPairs.Num(); i++)
	{
		for (int32 j = 0; j < SpineHitPairs[i].FeetHitArray.Num(); j++)
		{

			const FHitResult& HitC = SpineHitPairs[i].FeetHitArray[j];
			const FHitResult& HitF = SpineHitPairs[i].FeetFrontHitArray[j];
			const FHitResult& HitL = SpineHitPairs[i].FeetLeftHitArray[j];
			const FHitResult& HitR = SpineHitPairs[i].FeetRightHitArray[j];

			const bool bHitC = HitC.bBlockingHit;
			const bool bHitF = HitF.bBlockingHit;
			const bool bHitL = HitL.bBlockingHit;
			const bool bHitR = HitR.bBlockingHit;

			const int32 HitCount = (int32)bHitC + (int32)bHitF + (int32)bHitL + (int32)bHitR;


			FVector SurfaceNormal = UpWS;

			if (HitCount <= 1)
			{
				// 1hit
				if (bHitC)     
					SurfaceNormal = HitC.ImpactNormal.GetSafeNormal();
				else if (bHitF)
					SurfaceNormal = HitF.ImpactNormal.GetSafeNormal();
				else if (bHitL)
					SurfaceNormal = HitL.ImpactNormal.GetSafeNormal();
				else if (bHitR)
					SurfaceNormal = HitR.ImpactNormal.GetSafeNormal();
			}
			else if (HitCount < 4)
			{
				// 2hit / 3hit
				if (bHitC && bHitF)
				{
					const FVector ForwardVec = (HitF.ImpactPoint - HitC.ImpactPoint).GetSafeNormal();
					FVector RightAxis = FVector::ZeroVector;

					if (bHitL && bHitR)
					{
						RightAxis = (HitR.ImpactPoint - HitL.ImpactPoint).GetSafeNormal();
					}
					else
					{
						// center/front しかない時は キャラ右方向を補助軸に使う
						const FVector OwnerUp = ComponentToWorld.TransformVectorNoScale(CharacterDirectionVectorCS).GetSafeNormal();
						const FVector OwnerFwd = ComponentToWorld.TransformVectorNoScale(CharacterForwardDirectionVector_CS).GetSafeNormal();
						RightAxis = FVector::CrossProduct(OwnerUp, OwnerFwd).GetSafeNormal();
					}

					FVector PlaneNormal = FVector::CrossProduct(ForwardVec, RightAxis).GetSafeNormal();
					if ((PlaneNormal | UpWS) < 0.0f)
					{
						PlaneNormal *= -1.0f;
					}

					SurfaceNormal = FMath::Lerp(HitC.ImpactNormal.GetSafeNormal(), PlaneNormal, 0.45f).GetSafeNormal();
				}
				else if (bHitL && bHitR)
				{
					const FVector RightVec = (HitR.ImpactPoint - HitL.ImpactPoint).GetSafeNormal();
					const FVector OwnerFwd = ComponentToWorld.TransformVectorNoScale(CharacterForwardDirectionVector_CS).GetSafeNormal();

					FVector PlaneNormal = FVector::CrossProduct(OwnerFwd, RightVec).GetSafeNormal();
					if ((PlaneNormal | UpWS) < 0.0f)
					{
						PlaneNormal *= -1.0f;
					}

					SurfaceNormal = PlaneNormal;
				}
				else
				{
					// 2hit だけど組み合わせが弱いときは center 優先
					if (bHitC)     
						SurfaceNormal = HitC.ImpactNormal.GetSafeNormal();
					else if (bHitF)
						SurfaceNormal = HitF.ImpactNormal.GetSafeNormal();
					else if (bHitL)
						SurfaceNormal = HitL.ImpactNormal.GetSafeNormal();
					else if (bHitR)
						SurfaceNormal = HitR.ImpactNormal.GetSafeNormal();
				}
			}
			else
			{
				// 4hit
				FVector ForwardVec = (HitF.ImpactPoint - HitC.ImpactPoint).GetSafeNormal();
				FVector RightVec = (HitR.ImpactPoint - HitL.ImpactPoint).GetSafeNormal();

				FVector PlaneNormal = FVector::CrossProduct(ForwardVec, RightVec).GetSafeNormal();
				if ((PlaneNormal | UpWS) < 0.0f)
				{
					PlaneNormal *= -1.0f;
				}

				const float Align = bHitC
					? FMath::Clamp(FVector::DotProduct(HitC.ImpactNormal.GetSafeNormal(), PlaneNormal), 0.0f, 1.0f)
					: 1.0f;

				const float SurfaceWeight = FMath::Lerp(0.25f, 0.7f, Align);

				SurfaceNormal = FMath::Lerp(
					HitC.ImpactNormal.GetSafeNormal(),
					PlaneNormal,
					SurfaceWeight).GetSafeNormal();
			}

			// 2. ボーン自身の向きを基準にする
			// キャラクターのForward(1,0,0)ではなく、現在のアニメーションにおけるつま先の向きを使用
			FCompactPoseBoneIndex BoneIdx = SpineFeetPair[i].FeetArray[j].CachedCompactPoseIndex;
			FTransform BoneWSTransform = Output.Pose.GetComponentSpaceTransform(BoneIdx) * ComponentToWorld;

			// ボーンの主軸（通常はX）を取得
			FVector BoneForwardWS = BoneWSTransform.GetUnitAxis(EAxis::X);
			const float FeetLimit = FMath::Abs(SolverInputData.FeetBones[SpineFeetPair[i].OrderIndexArray[j]].FeetRotationLimit);

			const FRotator FinalTargetRotation = RotationFromImpactNormal(
				i, 
				j, 
				false,
				Output,
				SurfaceNormal,
				Output.Pose.GetComponentSpaceTransform(BoneIdx),
				FeetLimit);

			FTransform& FingerTrans = FeetModofyTransformArray[i][j];

			const FQuat& InterpolatedQuat = AnimationQuatSlerp(
				HitC.bBlockingHit, 
				FingerTrans.GetRotation(), 
				FinalTargetRotation.Quaternion(), 
				FormatRotationLerp);

			FingerTrans.SetRotation(InterpolatedQuat);
			FeetModifiedNormalArray[i][j] = SurfaceNormal;

			if (bIsCalcFingerJoints)
			{
				for (int32 f = 0; f < SpineHitPairs[i].FingerHitArray[j].Num(); f++)
				{
					const FBoneReference& FingerBoneRef = SpineFeetPair[i].FingerArray[j][f];
					const FCompactPoseBoneIndex& ModifyBoneIndexLocalFinger = FingerBoneRef.GetCompactPoseIndex(Output.Pose.GetPose().GetBoneContainer());
					const FTransform BoneTransformFinger = Output.Pose.GetComponentSpaceTransform(ModifyBoneIndexLocalFinger);
					const FHitResult& OrigFingerHit = SpineHitPairs[i].OriginalFingerHitArray[j][f];
					const FHitResult& FingerHit = SpineHitPairs[i].FingerHitArray[j][f];

					FVector NormalFingerImpact = UpWS;
					const bool bHasForwardFingerHits = FingerHit.bBlockingHit && OrigFingerHit.bBlockingHit;
					const bool bHasSideFeetHits = HitR.bBlockingHit && HitL.bBlockingHit;

					if (bHasForwardFingerHits && bHasSideFeetHits)
					{
						FVector ForwardImpact = (OrigFingerHit.ImpactPoint - FingerHit.ImpactPoint).GetSafeNormal();
						const int32 FingerSpineIndex = SpineFeetPair[i].OrderIndexArray[j];
						const auto& FingerConfig = SolverInputData.FeetBones[FingerSpineIndex].FingerBoneArray[f];

						if (!FingerConfig.Is_Finger_Backward)
						{
							ForwardImpact = (OrigFingerHit.ImpactPoint - FingerHit.ImpactPoint).GetSafeNormal();
						}
						else
						{
							ForwardImpact = (FingerHit.ImpactPoint - OrigFingerHit.ImpactPoint).GetSafeNormal();
						}

						const FVector RightCross = (HitR.ImpactPoint - HitL.ImpactPoint).GetSafeNormal();
						FVector ImpactForwardFinal = FVector::CrossProduct(ForwardImpact, RightCross).GetSafeNormal();

						ImpactForwardFinal = ComponentToWorld.InverseTransformVectorNoScale(ImpactForwardFinal);
						NormalFingerImpact = ImpactForwardFinal;
						NormalFingerImpact = ComponentToWorld.TransformVectorNoScale(NormalFingerImpact);
					}
					else if (FingerHit.bBlockingHit)
					{
						NormalFingerImpact = FingerHit.ImpactNormal.GetSafeNormal();
					}
					else if (HitC.bBlockingHit)
					{
						NormalFingerImpact = HitC.ImpactNormal.GetSafeNormal();
					}
					else
					{
						NormalFingerImpact = FeetModifiedNormalArray[i][j].GetSafeNormal();
					}

					FTransform& FeetFingerTransform = FeetFingerTransformArray[i][j][f];

					const FRotator& RotatedFingerRotation = RotationFromImpactNormal(
						i, 
						j, 
						true, 
						Output,
						NormalFingerImpact, 
						BoneTransformFinger, 
						FeetLimit);

					const bool bHasAnyToeContact = FingerHit.bBlockingHit || OrigFingerHit.bBlockingHit || HitC.bBlockingHit;

					const FQuat& Res = AnimationQuatSlerp(
						bHasAnyToeContact,
						FeetFingerTransform.GetRotation(),
						RotatedFingerRotation.Quaternion(),
						FormatRotationLerp);

					FeetFingerTransform.SetRotation(Res);
				}
			}


		}

	}
}

FRotator FAnimNode_CustomFeetSolver::RotationFromImpactNormal(
	const int32 SpineIndex,
	const int32 FeetIndex,
	const bool bIsFinger,
	FComponentSpacePoseContext& Output,
	const FVector& NormalImpactInput,
	const FTransform& OriginalBoneTransform,
	const float FeetLimit) const
{

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const auto SK = Output.AnimInstanceProxy->GetSkelMeshComponent();
	const AActor* Owner = SK->GetOwner();

	const FVector& ImpactNormal = ComponentToWorld.InverseTransformVectorNoScale(NormalImpactInput);
	const float ImpactNormal_Pitch = ((180.0) / UE_DOUBLE_PI * FMath::Atan2(ImpactNormal.X, ImpactNormal.Z) * -1.0f);
	const float ImpactNormal_Roll = ((180.0) / UE_DOUBLE_PI * FMath::Atan2(ImpactNormal.Y, ImpactNormal.Z) * 1.0f);
	FRotator NormalRotation = FRotator(ImpactNormal_Pitch, 0.0f, ImpactNormal_Roll);

	const FVector& UnitNormalImpact = CharacterDirectionVectorCS;
	const float L_Pitch = (180.0) / UE_DOUBLE_PI * FMath::Atan2(UnitNormalImpact.X, UnitNormalImpact.Z) * -1.0f;
	const float L_Roll = (180.0) / UE_DOUBLE_PI * FMath::Atan2(UnitNormalImpact.Y, UnitNormalImpact.Z) * 1.0f;
	const FRotator& UnitNormalRotation = FRotator(L_Pitch, 0.0f, L_Roll);

	if (!bEnablePitch)
	{
		NormalRotation.Pitch = 0.0f;
	}

	if (!bEnableRoll)
	{
		NormalRotation.Roll = 0.0f;
	}

	NormalRotation.Pitch = FMath::Clamp(NormalRotation.Pitch, -FeetLimit, FeetLimit);
	NormalRotation.Roll = FMath::Clamp(NormalRotation.Roll, -FeetLimit, FeetLimit);
	NormalRotation.Yaw = FMath::Clamp(NormalRotation.Yaw, -FeetLimit, FeetLimit);
	NormalRotation = NormalRotation - UnitNormalRotation;
	FTransform TestTransform1 = OriginalBoneTransform;

	if (!bIsFinger)
	{
		const int32 Idx = SpineFeetPair[SpineIndex].OrderIndexArray[FeetIndex];
		const FRotator& OffsetLocalRot = SolverInputData.FeetBones[Idx].FeetRotationOffset;
		const FQuat BoneInput(OffsetLocalRot);
		FQuat Forward_Rotation_Difference = FQuat::FindBetweenNormals(CharacterForwardDirectionVector_CS, PolesForwardDirectionVector_CS);
		TestTransform1.SetRotation(BoneInput * TestTransform1.GetRotation());
		TestTransform1.SetRotation(Forward_Rotation_Difference * TestTransform1.GetRotation());
	}
	NormalRotation = FRotator(NormalRotation.Quaternion() * (TestTransform1.Rotator()).Quaternion());

	return NormalRotation;

}


void FAnimNode_CustomFeetSolver::CalculateFeetHeight(FComponentSpacePoseContext& Output)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector UpCS = CharacterDirectionVectorCS.GetSafeNormal();

	FeetRootHeights.Empty();
	FeetRootHeights.SetNum(SpineFeetPair.Num());

	for (int32 i = 0; i < SpineFeetPair.Num(); ++i)
	{
		FeetRootHeights[i].SetNum(SpineFeetPair[i].FeetArray.Num());

		for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); ++j)
		{
			const FTransform BoneTraceTransform = Output.Pose.GetComponentSpaceTransform(SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer));
			const auto Scale = ComponentToWorld.GetScale3D() * ComponentScale;
			const int32 Idx = SpineFeetPair[i].OrderIndexArray[j];

			float Height = 0.0f;
			if (SolverInputData.FeetBones.IsValidIndex(Idx))
			{
				const float Diff = (BoneTraceTransform.GetLocation().Z - FVector(0.0f, 0.0f, 0.0f).Z);
				const float HeightOffset = (SolverInputData.FeetBones[Idx].FeetHeight) * Scale.Z;
				Height = (FMath::Abs(Diff)) + HeightOffset;
			}

			const FCompactPoseBoneIndex FootIndex = SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer);
			const FTransform FootCS = Output.Pose.GetComponentSpaceTransform(FootIndex);

			if (bIsSpiderMode && SpineFeetPair[i].ToeArray.IsValidIndex(j))
			{
				const FBoneReference& ToeRef = SpineFeetPair[i].ToeArray[j];
				if (ToeRef.IsValidToEvaluate())
				{
					const FTransform ToeCS = Output.Pose.GetComponentSpaceTransform(ToeRef.GetCompactPoseIndex(BoneContainer));
					const FVector DeltaCS = ToeCS.GetLocation() - FootCS.GetLocation();
					Height = FMath::Max(Height, FMath::Abs(FVector::DotProduct(DeltaCS, UpCS)) * ComponentToWorld.GetScale3D().Z);
				}
			}
			else if (bIsCalcFingerJoints && SpineFeetPair[i].FingerArray.IsValidIndex(j))
			{
				for (int32 f = 0; f < SpineFeetPair[i].FingerArray[j].Num(); ++f)
				{
					const FBoneReference& FingerRef = SpineFeetPair[i].FingerArray[j][f];
					if (!FingerRef.IsValidToEvaluate())
					{
						continue;
					}

					const FTransform FingerCS = Output.Pose.GetComponentSpaceTransform(FingerRef.GetCompactPoseIndex(BoneContainer));
					const FVector DeltaCS = FingerCS.GetLocation() - FootCS.GetLocation();
					Height = FMath::Max(Height, FMath::Abs(FVector::DotProduct(DeltaCS, UpCS)) * ComponentToWorld.GetScale3D().Z);
				}
			}

			FeetRootHeights[i][j] = Height;
		}
	}
}

FVector FAnimNode_CustomFeetSolver::ClampRotateVector(
	const FVector& InputPosition,
	const FVector& ForwardVectorDir,
	const FVector& Origin,
	const float MinClampDegrees,
	const float MaxClampDegrees,
	const float HClampMin,
	const float HClampMax) const
{
	const float Magnitude = (Origin - InputPosition).Size();
	const FVector Rot1 = (ForwardVectorDir).GetSafeNormal();
	const FVector Rot2 = (InputPosition - Origin).GetSafeNormal();
	const FVector Rot3 = Rot2;
	const float Degrees = ((180.0) / UE_DOUBLE_PI * FMath::Acos(FVector::DotProduct(Rot1, Rot2)));

	const FVector AngleCrossResult = FVector::CrossProduct(Rot2, Rot1);
	const float Dir = FVector::DotProduct(AngleCrossResult, FVector::CrossProduct(FVector::UpVector, Rot1));
	const float AlphaDirVertical = (Dir / 2) + 0.5f;

	const FVector AngleCrossResultHorizontal = FVector::CrossProduct(Rot2, Rot1);
	const float DirHorizontal = FVector::DotProduct(AngleCrossResultHorizontal, FVector::UpVector);
	const float AlphaDirHorizontal = (DirHorizontal / 2) + 0.5f;

	const float HorizontalDegreePriority = (FMath::Lerp(FMath::Abs(HClampMin), FMath::Abs(HClampMax),
		FMath::Clamp(AlphaDirHorizontal, 0.0f, 1.0f)));

	const float VerticalDegreePriority = (FMath::Lerp(FMath::Abs(MinClampDegrees), FMath::Abs(MaxClampDegrees),
		FMath::Clamp(AlphaDirVertical, 0.0f, 1.0f)));

	const float SelectedClampValue = FMath::Lerp(VerticalDegreePriority, HorizontalDegreePriority,
		FMath::Clamp(FMath::Abs(DirHorizontal), 0.0f, 1.0f));

	float CurAlpha = (SelectedClampValue / (FMath::Max(SelectedClampValue, Degrees)));
	CurAlpha = FMath::Clamp(CurAlpha, 0.0f, 1.0f);
	const FVector OutputRot = FMath::Lerp(Rot1, Rot2, CurAlpha);
	return (Origin + (OutputRot.GetSafeNormal() * Magnitude));
}


void FAnimNode_CustomFeetSolver::UpdateInternal(const FAnimationUpdateContext& Context)
{
	CachedDeltaSeconds = Context.GetDeltaTime();

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();

	const AActor* Owner = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner();

	const float DeltaSeconds = CachedDeltaSeconds;
	ComponentScale = ComponentToWorld.GetScale3D().Z * VirtualScale;

	FVector CurrentMovementDirection = FVector::ZeroVector;
	FRotator CurrentCharacterRotation = FRotator::ZeroRotator;

	if (Owner)
	{
		CharacterMovementSpeed = Owner->GetVelocity().Size2D();
		CurrentMovementDirection = Owner->GetVelocity().GetSafeNormal();
		CurrentCharacterRotation = Owner->GetActorRotation();
	}

	// 方向転換の検出
	if (!PreviousMovementDirection.IsNearlyZero() && !CurrentMovementDirection.IsNearlyZero())
	{
		float DirectionDot = FVector::DotProduct(PreviousMovementDirection, CurrentMovementDirection);
		// 90度以上の方向転換を検出 (Dot < 0)
		if (DirectionDot < 0.7f) // 約45度以上の変化
		{
			DirectionChangeAlpha = FMath::Max(DirectionChangeAlpha, 1.0f);
		}
	}

	if (!PreviousCharacterRotation.IsZero())
	{
		FRotator RotationDelta = (CurrentCharacterRotation - PreviousCharacterRotation).GetNormalized();
		CharacterRotationLag = FMath::Abs(RotationDelta.Yaw) * Context.GetDeltaTime();
	}

	// 方向転換アルファを徐々に減衰
	DirectionChangeAlpha = FMath::Max(0.0f, DirectionChangeAlpha - Context.GetDeltaTime() * 2.0f);
	DirectionChangeSmoothing = FMath::Clamp(1.0f - DirectionChangeAlpha, 0.3f, 1.0f);
	PreviousMovementDirection = CurrentMovementDirection;

	PreviousCharacterRotation = CurrentCharacterRotation;
	CachedCharacterRotation = CurrentCharacterRotation;


	const float ExtraMultiplier = InterpolationVelocityCurve.GetRichCurve()->Eval(CharacterMovementSpeed);
	FormatLocationLerp = FMath::Clamp(DeltaSeconds * 15.0f * LocationLerpSpeed * ExtraMultiplier, 0.0f, 1.0f);
	FormatRotationLerp = FMath::Clamp((1 - FMath::Exp(-10 * DeltaSeconds)) * FeetRotationSpeed * ExtraMultiplier, 0.0f, 1.0f);


	TraceStartList.Empty();
	TraceEndList.Empty();
	TraceLinearColor.Empty();

	if (UPredictionAnimInstance* AnimInst = Cast<UPredictionAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		//LineTraceUpperHeight = AnimInst->GetReactFootIKUpTraceHeight();
		//LineTraceDownwardHeight = AnimInst->GetReactFootIKDownTraceHeight();
	}


	if (SpineHitPairs.IsEmpty() || SpineFeetPair.IsEmpty() || !bEnableSolver)
	{
		return;
	}

	// Component Up
	// fix wv pro
	const FVector& UpVector = ComponentToWorld.TransformVectorNoScale(CharacterDirectionVectorCS);
	const FVector& ForwardVector = ComponentToWorld.TransformVectorNoScale(CharacterForwardDirectionVector_CS);
	const FVector& RightVector = FVector::CrossProduct(ForwardVector, UpVector);


	for (int32 i = 0; i < SpineHitPairs.Num(); i++)
	{
		for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
		{

			FVector BaseFootLocation = SpineTransformPairs[i].AssociatedFootArray[j].GetLocation();
			const FVector& Origin = BaseFootLocation;
			const int32 OrderIndex = SpineFeetPair[i].OrderIndexArray[j];
			const auto& FootConfig = SolverInputData.FeetBones[OrderIndex];
			const float FrontTracePointSpacing = FMath::Max(FootConfig.FrontTracePointSpacing, 1.0f);
			const float SideTraceSpacing = FMath::Max(FootConfig.SideTracesSpacing, 1.0);
			const float UpHit = LineTraceUpperHeight * ComponentScale;
			const float DownHit = LineTraceDownwardHeight * ComponentScale;
			const FVector& FootForward = Origin + (ForwardVector * FrontTracePointSpacing * ComponentScale);
			const FVector& FootRight = Origin - (RightVector * SideTraceSpacing * ComponentScale);
			const FVector& FootLeft = Origin + (RightVector * SideTraceSpacing * ComponentScale);


			if (bIsTraceOptimization)
			{
				ApplyMultiPointTraceBulk(Context, BaseFootLocation, FootForward, FootLeft, FootRight, SideTraceSpacing, UpHit, DownHit, i, j);
			}
			else
			{
				const FVector BaseTraceStart = BaseFootLocation + UpVector * UpHit;
				const FVector FrontTraceStart = FootForward + UpVector * UpHit;
				const FVector LeftTraceStart = FootLeft + UpVector * UpHit;
				const FVector RightTraceStart = FootRight + UpVector * UpHit;

				const FVector BaseTraceEnd = BaseFootLocation - UpVector * DownHit;
				const FVector FrontTraceEnd = FootForward - UpVector * DownHit;
				const FVector LeftTraceEnd = FootLeft - UpVector * DownHit;
				const FVector RightTraceEnd = FootRight - UpVector * DownHit;

				ApplyLineTrace(Context, BaseTraceStart, BaseTraceEnd, SpineHitPairs[i].FeetHitArray[j], FLinearColor::Green, true);
				ApplyLineTrace(Context, FrontTraceStart, FrontTraceEnd, SpineHitPairs[i].FeetFrontHitArray[j], FLinearColor::Blue, true);
				ApplyLineTrace(Context, LeftTraceStart, LeftTraceEnd, SpineHitPairs[i].FeetLeftHitArray[j], FLinearColor::Yellow, true);
				ApplyLineTrace(Context, RightTraceStart, RightTraceEnd, SpineHitPairs[i].FeetRightHitArray[j], FLinearColor::White, true);
			}

			FeetTipLocations[i][j] = ComponentToWorld.InverseTransformPosition(FootForward);

			// modify finger ik
			for (int32 f = 0; f < SpineTransformPairs[i].AssociatedFingerArray[j].Num(); f++)
			{
				const int32 Idx = SpineFeetPair[i].OrderIndexArray[j];
				const FCustomBone_FingerData& FingerData = SolverInputData.FeetBones[Idx].FingerBoneArray[f];
				const FVector& FingerLocationCS = SpineTransformPairs[i].AssociatedFingerArray[j][f].GetLocation();
				const FTransform& FeetFingerTransformCS = FeetFingerTransformArray[i][j][f];
				const FVector& FingerLocationWS = ComponentToWorld.TransformPosition(FeetFingerTransformCS.GetLocation());
				const FVector& OrigFingerLocationWS = ComponentToWorld.TransformPosition(FeetFingerTransformCS.GetLocation());
				const float FingerScale = FingerData.TraceScale;

				ApplyLineTrace(Context,
					FingerLocationWS + UpVector * UpHit * FingerScale,
					FingerLocationWS - UpVector * DownHit * FingerScale,
					SpineHitPairs[i].FingerHitArray[j][f], FLinearColor::Yellow, true);

				ApplyLineTrace(Context,
					OrigFingerLocationWS + UpVector * UpHit * FingerScale,
					OrigFingerLocationWS - UpVector * DownHit * FingerScale,
					SpineHitPairs[i].OriginalFingerHitArray[j][f], FLinearColor::Yellow, false);
			}



		}
	}

}

void FAnimNode_CustomFeetSolver::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_CustomFeetSolver_Eval);
	check(OutBoneTransforms.Num() == 0);


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

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	TRACE_CPUPROFILER_EVENT_SCOPE(CustomFeetSolver_EvaluateSkeletalControl_AnyThread);

	PrepareAnimatedPoseInfo_AnyThread(Output);
	GetResetedPoseInfo(Output.Pose);

	for (int32 i = 0; i < SpineFeetPair.Num(); i++)
	{
		const FCompactPoseBoneIndex SpineBoneIndex = SpineFeetPair[i].SpineBoneRef.GetCompactPoseIndex(BoneContainer);
		const FTransform SpineCS = Output.Pose.GetComponentSpaceTransform(SpineBoneIndex);
		SpineAnimatedTransformPairs[i].SpineInvolved = (SpineCS)*ComponentToWorld;
		SpineAnimatedTransformPairs[i].SpineInvolved.SetRotation(ComponentToWorld.GetRotation() * SpineCS.GetRotation());

		for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
		{
			if (SpineAnimatedTransformPairs.IsValidIndex(i))
			{
				const FCompactPoseBoneIndex FootBoneIndex = SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer);
				const FTransform FootCS = Output.Pose.GetComponentSpaceTransform(FootBoneIndex);
				SpineAnimatedTransformPairs[i].AssociatedFootArray[j] = FootCS * ComponentToWorld;
			}
		}

	}

	TArray<TArray<FTransform>> FeetRotationArray;
	BuildLegRotationArray(Output, FeetRotationArray);

	BlendRefPose.EvaluateComponentSpace(Output);
	CalculateFeetHeight(Output);
	EvaluateComponentSpaceInternal(Output);
	CalculateFeetRotation(Output, FeetRotationArray);

	ComponentPose.EvaluateComponentSpace(Output);

	for (int32 i = 0; i < SpineHitPairs.Num(); ++i)
	{
		if (!SpineFeetPair.IsValidIndex(i))
		{
			continue;
		}

		for (int32 j = 0; j < SpineHitPairs[i].FeetHitArray.Num(); ++j)
		{
			if (!SpineFeetPair[i].FeetArray.IsValidIndex(j))
			{
				continue;
			}
			ApplyLegFull(Output, SpineFeetPair[i].FeetArray[j], i, j, Output, OutBoneTransforms);
		}
	}

	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}


void FAnimNode_CustomFeetSolver::ApplyLegFull(
	const FComponentSpacePoseContext& Output,
	const FBoneReference& BoneRef,
	const int32 FeetIndex,
	const int32 HitIndex, 
	FComponentSpacePoseContext& MeshBasesSaved, 
	TArray<FBoneTransform>& OutBoneTransforms)
{

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	switch (IKType)
	{
	case EIKType::OneBoneIk:
		ApplySingleBoneIK(BoneContainer, BoneRef, FeetIndex, HitIndex, MeshBasesSaved, OutBoneTransforms);
		break;
	case EIKType::TwoBoneIk:
		ApplyTwoBoneIK(BoneContainer, BoneRef, FeetIndex, HitIndex, MeshBasesSaved, OutBoneTransforms);
		break;
	case EIKType::Fabrik:
		ApplyFabrikIK(BoneContainer, BoneRef, FeetIndex, HitIndex, MeshBasesSaved, OutBoneTransforms);
		break;
	}

}


/// <summary>
/// Basic IK
/// </summary>
void FAnimNode_CustomFeetSolver::ApplyTwoBoneIK(
	const FBoneContainer& RequiredBones,
	const FBoneReference& IKFootBone, 
	const int32 FeetIndex, 
	const int32 HitIndex, 
	FComponentSpacePoseContext& MeshBasesSaved, 
	TArray<FBoneTransform>& OutBoneTransforms)
{
	enum EBoneControlSpace EffectorLocationSpace = EBoneControlSpace::BCS_WorldSpace;
	enum EBoneControlSpace JointTargetLocationSpace = EBoneControlSpace::BCS_ComponentSpace;

	bool bInvalidLimb = false;
	const FCompactPoseBoneIndex IKBoneCompactPoseIndex = SpineFeetPair[FeetIndex].FeetArray[HitIndex].CachedCompactPoseIndex;

	const FTransform& ComponentToWorld = MeshBasesSaved.AnimInstanceProxy->GetComponentTransform();
	const AActor* Owner = MeshBasesSaved.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner();
	const FVector UpWS = ComponentToWorld.GetUnitAxis(EAxis::Z);

	const FVector CharacterDirectionVector = ComponentToWorld.TransformVectorNoScale(UpWS);
	const FCompactPoseBoneIndex UpperLimbIndex = SpineFeetPair[FeetIndex].ThighArray[HitIndex].CachedCompactPoseIndex;
	const FCompactPoseBoneIndex LowerLimbIndex = SpineFeetPair[FeetIndex].KneeArray[HitIndex].CachedCompactPoseIndex;

	if (LowerLimbIndex == INDEX_NONE || UpperLimbIndex == INDEX_NONE)
	{
		bInvalidLimb = true;
	}

	const bool bInBoneSpace = (EffectorLocationSpace == BCS_ParentBoneSpace) || (EffectorLocationSpace == BCS_BoneSpace);
	const int32 EffectorBoneIndex = bInBoneSpace ? (RequiredBones).GetPoseBoneIndexForBoneName("") : INDEX_NONE;
	const FCompactPoseBoneIndex EffectorSpaceBoneIndex = (RequiredBones).MakeCompactPoseIndex(FMeshPoseBoneIndex(EffectorBoneIndex));

	// If we walked past the root, this controlled is invalid, so return no affected bones.
	if (bInvalidLimb)
	{
		return;
	}

	const FTransform EndBoneLocalTransform = MeshBasesSaved.Pose.GetLocalSpaceTransform(IKBoneCompactPoseIndex);

	FTransform LowerLimbCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(LowerLimbIndex);
	FTransform UpperLimbCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	FTransform EndBoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
	FTransform LowerLimbCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(LowerLimbIndex);
	FTransform UpperLimbCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	FTransform EndBoneCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);

	FTransform& Joint_MainTarget = FeetModofyTransformArray[FeetIndex][HitIndex];

	// don't remove it ! fixed
	if (!Owner->GetWorld()->IsGameWorld())
	{
		Joint_MainTarget = EndBoneCSTransformX;
	}

	

	FTransform RootBoneCSTransform = FTransform::Identity;
	float FeetRootHeight = 0.0f;

	// Get current position of root of limb.
	// All position are in Component space.
	const FVector RootPos = UpperLimbCSTransform.GetTranslation();
	const FVector InitialJointPos = LowerLimbCSTransform.GetTranslation();
	const FVector InitialEndPos = EndBoneCSTransform.GetTranslation();
	const FTransform OrigEndBoneCSTransform = EndBoneCSTransform;
	FVector EffectorLocationPoint = ComponentToWorld.InverseTransformPosition(SpineAnimatedTransformPairs[FeetIndex].AssociatedFootArray[HitIndex].GetLocation());

	const float DeltaSeconds = CachedDeltaSeconds;

	float& IKAlpha = FootAlphaArray[FeetIndex][HitIndex];
	const FHitResult& HitData = SpineHitPairs[FeetIndex].FeetHitArray[HitIndex];
	const int32 SelectOrderIndex = SpineFeetPair[FeetIndex].OrderIndexArray[HitIndex];

	if (bEnableSolver)
	{
		FTransform EndBoneWorldTransform = EndBoneCSTransform;
		FAnimationRuntime::ConvertCSTransformToBoneSpace(
			ComponentToWorld,
			MeshBasesSaved.Pose,
			EndBoneWorldTransform,
			EffectorSpaceBoneIndex,
			EffectorLocationSpace);


		{
			FVector TempImpactRef = EndBoneWorldTransform.GetLocation();

			// --- Fallback計算の追加 ---
			// アニメーション本来の足の位置（ワールド空間）を取得
			const FVector AnimatedFootWS = SpineAnimatedTransformPairs[FeetIndex].AssociatedFootArray[HitIndex].GetLocation();
			// 足の高さ（FeetRootHeight）を引いて「アニメーション上の理想的な接地位置」を算出
			const FVector AnimatedGroundPosWS = AnimatedFootWS - (UpWS * FeetRootHeights[FeetIndex][HitIndex]);

			if (HitData.bBlockingHit)
			{
				TempImpactRef.Z = HitData.ImpactPoint.Z;
			}
			else
			{
				TempImpactRef = AnimatedGroundPosWS;
			}

			const FVector StartPos = FVector(TempImpactRef.X, TempImpactRef.Y, FeetImpactPointArray[FeetIndex][HitIndex].Z);
			FeetImpactPointArray[FeetIndex][HitIndex] = AnimationLocationLerp(
				HitData.bBlockingHit,
				StartPos,
				TempImpactRef,
				FormatLocationLerp);

			const FVector& LocalImpactPoint = FeetImpactPointArray[FeetIndex][HitIndex];
			const FVector CachedImactPoint = LocalImpactPoint;

			const float MaxAngle = 90.0f;
			EffectorLocationPoint = (LocalImpactPoint + UpWS * FeetRootHeights[FeetIndex][HitIndex]);
			const float FeetLimit = FMath::Clamp(FMath::Abs(SolverInputData.FeetBones[SelectOrderIndex].FeetRotationLimit), 1.0f, MaxAngle);

			float LimitAlphaValue = (FeetLimit / MaxAngle);
			LimitAlphaValue = LimitAlphaValue * ComplexSimpleFootVelocityCurve.GetRichCurve()->Eval(CharacterMovementSpeed);

			if (bEnableComplexRotationMethod)
			{
				const FQuat Post_Rotated_Normal = (FQuat::FindBetweenNormals(CharacterDirectionVector,
					FMath::Lerp(CharacterDirectionVector, FeetModifiedNormalArray[FeetIndex][HitIndex], LimitAlphaValue)));

				FTransform Origin_Transform = FTransform::Identity;
				Origin_Transform.SetLocation(LocalImpactPoint);

				FTransform New_Transform = FTransform::Identity;
				New_Transform.SetLocation(LocalImpactPoint);
				New_Transform.SetRotation(Post_Rotated_Normal);

				const FTransform Diff_Transform = Origin_Transform.Inverse() * New_Transform;
				FTransform Modified_Feet_Transform = FTransform::Identity;
				Modified_Feet_Transform.SetLocation(EffectorLocationPoint);
				Modified_Feet_Transform = Modified_Feet_Transform * Diff_Transform;
				EffectorLocationPoint = Modified_Feet_Transform.GetLocation();
			}

			EffectorLocationPoint = ComponentToWorld.InverseTransformPosition(EffectorLocationPoint);
			FVector Effector_Thigh_Dir = (OrigEndBoneCSTransform.GetLocation() - UpperLimbCSTransform.GetLocation());
			FVector Point_Thigh_Dir = (EffectorLocationPoint - UpperLimbCSTransform.GetLocation());
			const float Effector_Thigh_Size = Effector_Thigh_Dir.Size();
			const float Point_Thigh_Size = (EffectorLocationPoint - UpperLimbCSTransform.GetLocation()).Size();
			Effector_Thigh_Dir.Normalize();
			Point_Thigh_Dir.Normalize();

			FVector Formatted_Effector_Point = EffectorLocationPoint;

			if (bEnableFootLiftLimit)
			{
				const FCustomBone_FootData& FootBoneData = SolverInputData.FeetBones[SelectOrderIndex];

				Formatted_Effector_Point = UpperLimbCSTransform.GetLocation() + Point_Thigh_Dir *
					FMath::Clamp(Point_Thigh_Size,
						Effector_Thigh_Size *
						FMath::Abs(FootBoneData.MinFeetExtension),
						Effector_Thigh_Size *
						FMath::Abs(FootBoneData.MaxFeetExtension));

				const float Foot_Lift_Height = (Formatted_Effector_Point - OrigEndBoneCSTransform.GetLocation()).Size();

				if (FootBoneData.MaxFeetLift > 0.0f)
				{
					Formatted_Effector_Point = OrigEndBoneCSTransform.GetLocation() + CharacterDirectionVectorCS * FMath::Clamp(Foot_Lift_Height, 0.0f, FootBoneData.MaxFeetLift);
				}
			}

			const float TempMaxLimbRadius = FMath::Abs(MaxLegIKAngle);

			Formatted_Effector_Point = ClampRotateVector(
				Formatted_Effector_Point,
				-CharacterDirectionVectorCS,
				UpperLimbCSTransform.GetLocation(),
				-TempMaxLimbRadius, 
				TempMaxLimbRadius,
				-TempMaxLimbRadius,
				TempMaxLimbRadius);

			EffectorLocationPoint = Formatted_Effector_Point;

			if (bInterpolateOnly_Z)
			{
				FVector X_Y_Loc = EndBoneCSTransform.GetLocation();
				EffectorLocationPoint.X = X_Y_Loc.X;
				EffectorLocationPoint.Y = X_Y_Loc.Y;
			}
		}


		const FQuat& Rotated_Difference = OrigEndBoneCSTransform.GetRotation() * Joint_MainTarget.GetRotation().Inverse();

		EffectorLocationPoint.Z += FMath::Max(
			FMath::Abs(FRotator(Rotated_Difference).Roll),
			FMath::Abs(FRotator(Rotated_Difference).Pitch)) *
			SolverInputData.FeetBones[SelectOrderIndex].FeetSlopeOffsetMultiplier *
			ComponentScale;

		Joint_MainTarget.SetLocation(EffectorLocationPoint);

		const FCustomBone_FootData& FootData = SolverInputData.FeetBones[SelectOrderIndex];
		const float DisableIKCurveValue = MeshBasesSaved.Curve.Get(FootData.DisableCurveName);
		const float K_Alpha = (HitData.bBlockingHit) ? FMath::Clamp(FootData.FeetAlpha - DisableIKCurveValue, 0.0f, 1.f) : 0.0f;
		IKAlpha = FMath::FInterpTo(IKAlpha, K_Alpha, CachedDeltaSeconds, WeightAlphaInterpSpeed);
	}

	FTransform EffectorTransform(ComponentToWorld.TransformPosition(Joint_MainTarget.GetLocation()));
	FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentToWorld, MeshBasesSaved.Pose, EffectorTransform, EffectorSpaceBoneIndex, EffectorLocationSpace);

	const FVector DesiredPos = EffectorTransform.GetTranslation();
	const FVector DesiredDelta = DesiredPos - RootPos;
	float DesiredLength = DesiredDelta.Size();
	FVector	DesiredDir;
	if (DesiredLength < KINDA_SMALL_NUMBER)
	{
		DesiredLength = KINDA_SMALL_NUMBER;
		DesiredDir = FVector::ForwardVector;
	}
	else
	{
		DesiredDir = DesiredDelta / DesiredLength;
	}

	FTransform BendingDirectionTransform = LowerLimbCSTransform;

	FVector Foward_CS = ComponentToWorld.InverseTransformVector(ComponentToWorld.GetUnitAxis(EAxis::Y));
	FVector UpperLimb_WS = ComponentToWorld.TransformPosition(UpperLimbCSTransform.GetLocation());
	FVector EndLimb_WS = ComponentToWorld.TransformPosition(EndBoneCSTransform.GetLocation());
	FVector LowerLimb_WS = ComponentToWorld.TransformPosition(LowerLimbCSTransform.GetLocation());
	const FQuat ForwardRotationDiff = FQuat::FindBetweenNormals(CharacterForwardDirectionVector_CS, PolesForwardDirectionVector_CS);

	FTransform Knee_Transform = FTransform::Identity;
	Knee_Transform.SetRotation(ForwardRotationDiff);

	FTransform Pole_Transform = FTransform::Identity;
	Pole_Transform.SetLocation(SolverInputData.FeetBones[SelectOrderIndex].KneeDirectionOffset);
	Pole_Transform = Pole_Transform * Knee_Transform;

	if (SolverInputData.FeetBones.IsValidIndex(SelectOrderIndex))
	{
		const FVector TwoBoneJointLocation = ((UpperLimbCSTransform.GetLocation() + EndBoneCSTransform.GetLocation() + LowerLimbCSTransform.GetLocation()) / 3);
		Foward_CS = (TwoBoneJointLocation - (LowerLimbCSTransform.GetLocation() + Pole_Transform.GetLocation())).GetSafeNormal();
		BendingDirectionTransform.SetLocation(BendingDirectionTransform.GetLocation() + Foward_CS * -100);

		FootKneeOffsetArray[FeetIndex][HitIndex] = ComponentToWorld.TransformPosition(
			LowerLimbCSTransform.GetLocation() + SolverInputData.FeetBones[SelectOrderIndex].KneeDirectionOffset);

	}


	FTransform JointTargetTransform(BendingDirectionTransform);
	FCompactPoseBoneIndex JointTargetSpaceBoneIndex(INDEX_NONE);
	FVector	JointTargetPos = JointTargetTransform.GetTranslation();
	FVector JointTargetDelta = JointTargetPos - RootPos;
	const float JointTargetLengthSqr = JointTargetDelta.SizeSquared();

	FVector JointPlaneNormal, JointBendDir;
	if (JointTargetLengthSqr < FMath::Square(KINDA_SMALL_NUMBER))
	{
		JointBendDir = FVector::RightVector;
		JointPlaneNormal = CharacterDirectionVector;
	}
	else
	{
		JointPlaneNormal = DesiredDir ^ JointTargetDelta;
		if (JointPlaneNormal.SizeSquared() < FMath::Square(KINDA_SMALL_NUMBER))
		{
			DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
		}
		else
		{
			JointPlaneNormal.Normalize();

			// 曲げ方向 = JointTargetDelta を DesiredDir に直交な平面へ射影（＝成分落とし）
			// 明示的に float 化
			const float Dot = static_cast<float>((JointTargetDelta | DesiredDir));
			JointBendDir = JointTargetDelta - (Dot * DesiredDir);
			JointBendDir.Normalize();
		}
	}

	const float LowerLimbLength = (InitialEndPos - InitialJointPos).Size();
	const float UpperLimbLength = (InitialJointPos - RootPos).Size();
	const float MaxLimbLength = LowerLimbLength + UpperLimbLength;
	FVector OutEndPos = DesiredPos;
	FVector OutJointPos = InitialJointPos;

	if (DesiredLength > MaxLimbLength)
	{
		OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
		OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
	}
	else
	{
		const float TwoAB = 2.0f * UpperLimbLength * DesiredLength;
		const float CosAngle = (TwoAB != 0.0f) ? ((UpperLimbLength * UpperLimbLength) + (DesiredLength * DesiredLength) - (LowerLimbLength * LowerLimbLength)) / TwoAB : 0.0f;
		const bool bReverseUpperBone = (CosAngle < 0.f);

		if ((CosAngle > 1.f) || (CosAngle < -1.f))
		{
			if (UpperLimbLength > LowerLimbLength)
			{
				OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
				OutEndPos = OutJointPos - (LowerLimbLength * DesiredDir);
			}
			else
			{
				OutJointPos = RootPos - (UpperLimbLength * DesiredDir);
				OutEndPos = OutJointPos + (LowerLimbLength * DesiredDir);
			}
		}
		else
		{
			const float Angle = FMath::Acos(CosAngle);
			const float JointLineDist = UpperLimbLength * FMath::Sin(Angle);
			const float ProjJointDistSqr = (UpperLimbLength * UpperLimbLength) - (JointLineDist * JointLineDist);
			float ProjJointDist = (ProjJointDistSqr > 0.0f) ? FMath::Sqrt(ProjJointDistSqr) : 0.0f;

			if (bReverseUpperBone)
			{
				ProjJointDist *= -1.f;
			}
			OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
		}
	}

	{
		// Update transform for upper bone.
		const FVector OldDir = (InitialJointPos - RootPos).GetSafeNormal();
		const FVector NewDir = (OutJointPos - RootPos).GetSafeNormal();
		const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
		UpperLimbCSTransform.SetRotation(DeltaRotation * UpperLimbCSTransform.GetRotation());
		UpperLimbCSTransform.SetTranslation(RootPos);
	}

	{
		// Update transform for lower bone.
		const FVector OldDir = (InitialEndPos - InitialJointPos).GetSafeNormal();
		const FVector NewDir = (OutEndPos - OutJointPos).GetSafeNormal();
		const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
		LowerLimbCSTransform.SetRotation(DeltaRotation * LowerLimbCSTransform.GetRotation());
		LowerLimbCSTransform.SetTranslation(OutJointPos);
	}

	{

		EndBoneCSTransform.SetTranslation(OutEndPos);

		if (HitData.bBlockingHit)
		{
			const FTransform FeetCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
			const FTransform Feet_Saved_Transform = FTransform(Joint_MainTarget.Rotator());
			if (bShouldRotateFeet)
			{
				EndBoneCSTransform.SetRotation(Joint_MainTarget.GetRotation());
			}

		}
		else
		{
			if (!bIgnoreLerping)
			{
				const float Factor = (1.0f - FMath::Exp(-SmoothFactor * CachedDeltaSeconds));
				auto Result = FQuat::Slerp(Joint_MainTarget.GetRotation(), EndBoneCSTransform.GetRotation(), Factor);
				Joint_MainTarget.SetRotation(Result);
			}
			else
			{
				Joint_MainTarget.SetRotation(EndBoneCSTransform.GetRotation());
			}

			if (bShouldRotateFeet)
			{
				EndBoneCSTransform.SetRotation(Joint_MainTarget.GetRotation());
			}
		}

	}

	// if disable preview mesh
	const float AlphaTemp = (!Owner->GetWorld()->IsGameWorld()) ? 0.0f : IKAlpha;

	OutBoneTransforms.Add(FBoneTransform(UpperLimbIndex, FeetSolverHelper::TLerp(UpperLimbCSTransformX, UpperLimbCSTransform, AlphaTemp)));
	OutBoneTransforms.Add(FBoneTransform(LowerLimbIndex, FeetSolverHelper::TLerp(LowerLimbCSTransformX, LowerLimbCSTransform, AlphaTemp)));
	OutBoneTransforms.Add(FBoneTransform(IKBoneCompactPoseIndex, FeetSolverHelper::TLerp(EndBoneCSTransformX, EndBoneCSTransform, AlphaTemp)));


	if (bIsCalcFingerJoints)
	{
		for (int32 f = 0; f < SpineHitPairs[FeetIndex].FingerHitArray[HitIndex].Num(); f++)
		{
			const FBoneReference& PoseRef = SpineFeetPair[FeetIndex].FingerArray[HitIndex][f];
			const FCompactPoseBoneIndex FingerBoneIndex = PoseRef.GetCompactPoseIndex(RequiredBones);
			const FTransform FingerBoneIndexCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(FingerBoneIndex);
			const FTransform FootInverse = OrigEndBoneCSTransform.Inverse() * FeetSolverHelper::TLerp(EndBoneCSTransformX, EndBoneCSTransform, AlphaTemp);
			const FTransform FootDiff = (FingerBoneIndexCS * FootInverse);
			FeetFingerTransformArray[FeetIndex][HitIndex][f].SetLocation(FootDiff.GetLocation());

			const FTransform Result = FeetFingerTransformArray[FeetIndex][HitIndex][f];
			const FTransform FingerBlendCS = FeetSolverHelper::TLerp(FootDiff, Result, AlphaTemp);
			OutBoneTransforms.Add(FBoneTransform(SpineFeetPair[FeetIndex].FingerArray[HitIndex][f].CachedCompactPoseIndex, FingerBlendCS));
		}
	}



}


/// <summary>
/// 単純な手先の位置合わせ
/// </summary>
void FAnimNode_CustomFeetSolver::ApplySingleBoneIK(
	const FBoneContainer& RequiredBones, 
	const FBoneReference& IKFootBone, 
	const int32 FeetIndex,
	const int32 HitIndex,
	FComponentSpacePoseContext& MeshBasesSaved, 
	TArray<FBoneTransform>& OutBoneTransforms)
{

	enum EBoneControlSpace EffectorLocationSpace = EBoneControlSpace::BCS_WorldSpace;
	enum EBoneControlSpace JointTargetLocationSpace = EBoneControlSpace::BCS_ComponentSpace;

	// Get indices of the lower and upper limb bones and check validity.
	FCompactPoseBoneIndex IKBoneCompactPoseIndex = IKFootBone.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex UpperLimbIndex = (RequiredBones).GetParentBoneIndex(IKBoneCompactPoseIndex);
	const FTransform& ComponentToWorld = MeshBasesSaved.AnimInstanceProxy->GetComponentTransform();

	const AActor* Owner = MeshBasesSaved.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner();

	const bool bInvalidLimb = (UpperLimbIndex == INDEX_NONE);

	const FVector UpVector = CharacterDirectionVectorCS;
	FVector CharacterDirectionVector = ComponentToWorld.TransformVectorNoScale(UpVector);

	const bool bInBoneSpace = (EffectorLocationSpace == BCS_ParentBoneSpace) || (EffectorLocationSpace == BCS_BoneSpace);
	const int32 EffectorBoneIndex = bInBoneSpace ? (RequiredBones).GetPoseBoneIndexForBoneName("") : INDEX_NONE;
	const FCompactPoseBoneIndex EffectorSpaceBoneIndex = (RequiredBones).MakeCompactPoseIndex(FMeshPoseBoneIndex(EffectorBoneIndex));

	// If we walked past the root, this controlled is invalid, so return no affected bones.
	if (bInvalidLimb)
	{
		return;
	}

	const FTransform EndBoneLocalTransform = MeshBasesSaved.Pose.GetLocalSpaceTransform(IKBoneCompactPoseIndex);
	FTransform UpperLimbCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	FTransform EndBoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
	FTransform RootBoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0));

	// Get current position of root of limb.
	// All position are in Component space.
	const FVector RootPos = UpperLimbCSTransform.GetTranslation();
	const FVector InitialEndPos = EndBoneCSTransform.GetTranslation();
	FVector EffectorLocation_Point;

	const float DeltaSeconds = CachedDeltaSeconds;
	const float DX = (1.0f - FMath::Exp(-10.0f * DeltaSeconds));

	float& IKAlpha = FootAlphaArray[FeetIndex][HitIndex];

	FTransform& Joint_MainTarget = FeetModofyTransformArray[FeetIndex][HitIndex];
	const FHitResult& HitData = SpineHitPairs[FeetIndex].FeetHitArray[HitIndex];

	if (HitData.bBlockingHit && bEnableSolver)
	{
		FTransform EndBoneTransformWS = EndBoneCSTransform;
		FAnimationRuntime::ConvertCSTransformToBoneSpace(
			ComponentToWorld,
			MeshBasesSaved.Pose,
			EndBoneTransformWS,
			EffectorSpaceBoneIndex,
			EffectorLocationSpace);

		EffectorLocation_Point = (HitData.ImpactPoint + CharacterDirectionVector * FeetRootHeights[FeetIndex][HitIndex]);
		EffectorLocation_Point = ComponentToWorld.InverseTransformPosition(EffectorLocation_Point);

		if (bInterpolateOnly_Z)
		{
			FVector X_Y_Loc = EndBoneCSTransform.GetLocation();
			EffectorLocation_Point.X = X_Y_Loc.X;
			EffectorLocation_Point.Y = X_Y_Loc.Y;
		}

		FeetModofyTransformArray[FeetIndex][HitIndex].SetLocation(EffectorLocation_Point);
		FCustomBone_FootData& FootData = SolverInputData.FeetBones[SpineFeetPair[FeetIndex].OrderIndexArray[HitIndex]];

		const float DisableIKCurveValue = MeshBasesSaved.Curve.Get(FootData.DisableCurveName);
		const float K_Alpha = (HitData.bBlockingHit) ? FMath::Clamp(FootData.FeetAlpha - DisableIKCurveValue, 0.0f, 1.f) : 0.0f;
		IKAlpha = FMath::FInterpTo(IKAlpha, K_Alpha, CachedDeltaSeconds, WeightAlphaInterpSpeed);
	}
	else
	{
		FTransform EndBoneTransformWS = EndBoneCSTransform;
		FeetModofyTransformArray[FeetIndex][HitIndex].SetLocation(EndBoneTransformWS.GetLocation());

		FAnimationRuntime::ConvertCSTransformToBoneSpace(
			ComponentToWorld,
			MeshBasesSaved.Pose,
			EndBoneTransformWS,
			EffectorSpaceBoneIndex,
			EffectorLocationSpace);

		EffectorLocation_Point = EndBoneTransformWS.GetLocation();
		IKAlpha = FMath::FInterpTo(IKAlpha, 0.0f, CachedDeltaSeconds, WeightAlphaInterpSpeed);
	}

	FTransform EffectorTransform(ComponentToWorld.TransformPosition(FeetModofyTransformArray[FeetIndex][HitIndex].GetLocation()));

	FAnimationRuntime::ConvertBoneSpaceTransformToCS(
		ComponentToWorld,
		MeshBasesSaved.Pose,
		EffectorTransform,
		EffectorSpaceBoneIndex,
		EffectorLocationSpace);

	FVector DesiredPos = EffectorTransform.GetTranslation();
	const FVector DesiredDelta = DesiredPos - RootPos;
	float DesiredLength = DesiredDelta.Size();

	FVector	DesiredDir;
	if (DesiredLength < KINDA_SMALL_NUMBER)
	{
		DesiredLength = KINDA_SMALL_NUMBER;
		DesiredDir = FVector::ForwardVector;
	}
	else
	{
		DesiredDir = DesiredDelta / DesiredLength;
	}

	const float UpperLimbLength = (InitialEndPos - RootPos).Size();
	const float MaxLimbLength = UpperLimbLength;
	FVector OutEndPos = DesiredPos;

	if (DesiredLength > MaxLimbLength)
	{
		OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
	}
	else
	{
		OutEndPos = EffectorTransform.GetLocation();
	}

	{
		const FVector OldDir = (InitialEndPos - RootPos).GetSafeNormal();
		const FVector NewDir = (OutEndPos - RootPos).GetSafeNormal();
		const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
		UpperLimbCSTransform.SetRotation(DeltaRotation * UpperLimbCSTransform.GetRotation());
		UpperLimbCSTransform.SetTranslation(RootPos);
	}

	{
		EndBoneCSTransform.SetTranslation(OutEndPos);

		if (HitData.bBlockingHit)
		{
			EndBoneCSTransform.SetRotation(FeetModofyTransformArray[FeetIndex][HitIndex].GetRotation());
		}
		else
		{
			const FTransform UpperLimbCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
			const FTransform EndBoneCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
			FeetModofyTransformArray[FeetIndex][HitIndex].SetRotation(EndBoneCSTransform.Rotator().Quaternion());
			EndBoneCSTransform = EndBoneCSTransformX;
			UpperLimbCSTransform = UpperLimbCSTransformX;
			EndBoneCSTransform.SetRotation(FeetModofyTransformArray[FeetIndex][HitIndex].GetRotation());
		}
	}


	const float AlphaTemp = (!Owner->GetWorld()->IsGameWorld()) ? 0.0f : IKAlpha;
	const FTransform UpperLimbCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	const FTransform EndBoneCSTransformX = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);
	OutBoneTransforms.Add(FBoneTransform(UpperLimbIndex, FeetSolverHelper::TLerp(UpperLimbCSTransformX, UpperLimbCSTransform, AlphaTemp)));
	OutBoneTransforms.Add(FBoneTransform(IKBoneCompactPoseIndex, FeetSolverHelper::TLerp(EndBoneCSTransformX, EndBoneCSTransform, AlphaTemp)));
}



/// <summary>
/// MultiLegIK
/// </summary>
void FAnimNode_CustomFeetSolver::ApplyFabrikIK(
	const FBoneContainer& RequiredBones,
	const FBoneReference& IKFootBone,
	const int32 FeetIndex,
	const int32 HitIndex,
	FComponentSpacePoseContext& MeshBasesSaved,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	if (!SpineFeetPair.IsValidIndex(FeetIndex))
	{
		return;
	}

	if (!SpineFeetPair[FeetIndex].ThighArray.IsValidIndex(HitIndex) ||
		!SpineFeetPair[FeetIndex].KneeArray.IsValidIndex(HitIndex) || !SpineFeetPair[FeetIndex].FeetArray.IsValidIndex(HitIndex))
	{
		UE_LOG(LogQuadrupedIK, Error, TEXT("[%s] : thigh, knee, foot, joint not valid Index"), *FString(__FUNCTION__));
		return;
	}

	const EBoneControlSpace EffectorLocationSpace = EBoneControlSpace::BCS_WorldSpace;
	const EBoneControlSpace JointTargetLocationSpace = EBoneControlSpace::BCS_ComponentSpace;

	const FTransform& ComponentToWorld = MeshBasesSaved.AnimInstanceProxy->GetComponentTransform();
	const AActor* Owner = MeshBasesSaved.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner();
	const USkeletalMeshComponent* SK = MeshBasesSaved.AnimInstanceProxy->GetSkelMeshComponent();

	FTransform FootCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(SpineFeetPair[FeetIndex].FeetArray[HitIndex].CachedCompactPoseIndex);
	FVector EffectorLocation_Point = FootCSTransform.GetLocation();

	FCompactPoseBoneIndex IKBoneCompactPoseIndex = IKFootBone.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex UpperLimbIndex = (RequiredBones).GetParentBoneIndex(IKBoneCompactPoseIndex);
	const FTransform EndBoneLocalTransform = MeshBasesSaved.Pose.GetLocalSpaceTransform(IKBoneCompactPoseIndex);
	FTransform UpperLimbCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	FTransform EndBoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(IKBoneCompactPoseIndex);

	const FVector UpWS = ComponentToWorld.GetUnitAxis(EAxis::Z);

	const bool bInBoneSpace = (EffectorLocationSpace == BCS_ParentBoneSpace) || (EffectorLocationSpace == BCS_BoneSpace);
	const int32 EffectorBoneIndex = bInBoneSpace ? (RequiredBones).GetPoseBoneIndexForBoneName("") : INDEX_NONE;
	const FCompactPoseBoneIndex EffectorSpaceBoneIndex = (RequiredBones).MakeCompactPoseIndex(FMeshPoseBoneIndex(EffectorBoneIndex));

	// setting boneIndices
	TArray<FCompactPoseBoneIndex> BoneIndices;
	BoneIndices.Add(SpineFeetPair[FeetIndex].ThighArray[HitIndex].CachedCompactPoseIndex);
	BoneIndices.Add(SpineFeetPair[FeetIndex].KneeArray[HitIndex].CachedCompactPoseIndex);
	BoneIndices.Add(SpineFeetPair[FeetIndex].FeetArray[HitIndex].CachedCompactPoseIndex);

	const FTransform FootBoneCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(SpineFeetPair[FeetIndex].FeetArray[HitIndex].CachedCompactPoseIndex);

	float FeetRootHeight = FeetRootHeights[FeetIndex][HitIndex];

	if (bIsSpiderMode && (SpineFeetPair[FeetIndex].ToeArray.IsValidIndex(HitIndex)))
	{
		const FBoneReference& ToeBoneRef = SpineFeetPair[FeetIndex].ToeArray[HitIndex];
		if (ToeBoneRef.IsValidToEvaluate())
		{
			const FTransform ToeBoneCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(ToeBoneRef.CachedCompactPoseIndex);
			const FVector DeltaCS = ToeBoneCS.GetLocation() - FootBoneCS.GetLocation();
			const float ToeHeight = FMath::Abs(FVector::DotProduct(DeltaCS, FVector::UpVector)) * ComponentScale;
			FeetRootHeight = FMath::Max(FeetRootHeight, ToeHeight);
		}
	}
	else if (bIsCalcFingerJoints && SpineFeetPair[FeetIndex].FingerArray.IsValidIndex(HitIndex))
	{
		for (int32 J = 0; J < SpineFeetPair[FeetIndex].FingerArray[HitIndex].Num(); J++)
		{
			const FBoneReference& FingerBoneRef = SpineFeetPair[FeetIndex].FingerArray[HitIndex][J];
			if (!FingerBoneRef.IsValidToEvaluate())
			{
				continue;
			}

			const FTransform FingerBoneCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(FingerBoneRef.CachedCompactPoseIndex);
			const FVector FingerDeltaCS = FingerBoneCS.GetLocation() - FootBoneCS.GetLocation();
			const float FingerHeight = FMath::Abs(FVector::DotProduct(FingerDeltaCS, FVector::UpVector)) * ComponentScale;
			FeetRootHeight = FMath::Max(FeetRootHeight, FingerHeight);
		}
	}
	else
	{
		UE_LOG(LogQuadrupedIK, Warning, TEXT("[%s] : finger array not valid Index : %d"), *FString(__FUNCTION__), HitIndex);
	}


	BoneIndices.Sort();

	const float Factor = (1.0f - FMath::Exp(-SmoothFactor * CachedDeltaSeconds));
	const FHitResult& TraceHit = SpineHitPairs[FeetIndex].FeetHitArray[HitIndex];

	float& FootAlpha = FootAlphaArray[FeetIndex][HitIndex];

	if (bEnableSolver && TraceHit.bBlockingHit)
	{
		FTransform EndBoneTransform_W = EndBoneCSTransform;
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentToWorld, MeshBasesSaved.Pose, EndBoneTransform_W, EffectorSpaceBoneIndex, EffectorLocationSpace);

		FVector CurrentHitPoint = TraceHit.ImpactPoint;



		if (bIsSpiderMode && SpineHitPairs[FeetIndex].ToeHitArray.IsValidIndex(HitIndex))
		{
			const FHitResult& ToeHit = SpineHitPairs[FeetIndex].ToeHitArray[HitIndex];
			if (ToeHit.bBlockingHit)
			{
				CurrentHitPoint = ToeHit.ImpactPoint;
			}
		}

		EffectorLocation_Point = (CurrentHitPoint + UpWS * FeetRootHeight);
		EffectorLocation_Point = ComponentToWorld.InverseTransformPosition(EffectorLocation_Point);

		// spider mode では effector を少し引き戻す
		if (bIsSpiderMode)
		{
			const FVector HipCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(SpineFeetPair[FeetIndex].ThighArray[HitIndex].CachedCompactPoseIndex).GetLocation();
			const FVector KneeCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(SpineFeetPair[FeetIndex].KneeArray[HitIndex].CachedCompactPoseIndex).GetLocation();
			const FVector FootCS = MeshBasesSaved.Pose.GetComponentSpaceTransform(SpineFeetPair[FeetIndex].FeetArray[HitIndex].CachedCompactPoseIndex).GetLocation();

			const float UpperLen = FVector::Dist(HipCS, KneeCS);
			const float LowerLen = FVector::Dist(KneeCS, FootCS);

			const float TotalLen = UpperLen + LowerLen;

			FVector HipToEff = EffectorLocation_Point - HipCS;
			const float Dist = HipToEff.Size();

			const float PrevHeight = EffectorLocation_Point.Z;

			if (Dist > KINDA_SMALL_NUMBER)
			{
				const float MaxReachWithSlack = TotalLen * SpiderReachRatio;
				if (Dist > MaxReachWithSlack)
				{
					HipToEff = HipToEff.GetSafeNormal() * MaxReachWithSlack;
					EffectorLocation_Point = HipCS + HipToEff;

					EffectorLocation_Point.Z = PrevHeight;
				}
			}
		}


		if (bInterpolateOnly_Z)
		{
			const FVector& X_Y_Loc = EndBoneCSTransform.GetLocation();
			EffectorLocation_Point.X = X_Y_Loc.X;
			EffectorLocation_Point.Y = X_Y_Loc.Y;
		}

		FCustomBone_FootData& FootData = SolverInputData.FeetBones[SpineFeetPair[FeetIndex].OrderIndexArray[HitIndex]];
		const float Value = MeshBasesSaved.Curve.Get(FootData.DisableCurveName);
		const float K_Alpha = FMath::Clamp(1.0f - Value, 0.0f, 1.f);
		FootAlpha = FMath::FInterpTo(FootAlpha, K_Alpha, Factor, WeightAlphaInterpSpeed);
	}
	else
	{
		FTransform EndBoneWorldTransform = EndBoneCSTransform;
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentToWorld, MeshBasesSaved.Pose, EndBoneWorldTransform, EffectorSpaceBoneIndex, EffectorLocationSpace);

		EffectorLocation_Point = EndBoneWorldTransform.GetLocation();
		FootAlpha = FMath::FInterpTo(FootAlpha, 0.0f, Factor, WeightAlphaInterpSpeed);
	}


	// preview editorではAlphaは無効
	const float AlphaTemp = (IsValid(Owner) && !Owner->GetWorld()->IsGameWorld()) ? 0.0f : FootAlpha;

	// fix effector transform
	FootCSTransform.SetLocation(EffectorLocation_Point);

	double MaximumReach = 0;
	const int32 NumTransforms = BoneIndices.Num();

	TArray<FTransform> LocalBoneTransforms;
	TArray<FTransform> InitialBoneTransforms;
	LocalBoneTransforms.AddUninitialized(NumTransforms);
	InitialBoneTransforms.AddUninitialized(NumTransforms);

	TArray<FFABRIKChainLink> Chain;
	Chain.Reserve(NumTransforms);


	// thighBone setting
	{
		const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
		const FTransform& BoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(RootBoneIndex);

		LocalBoneTransforms[0] = BoneCSTransform;
		InitialBoneTransforms[0] = BoneCSTransform;
		Chain.Add(FFABRIKChainLink(BoneCSTransform.GetLocation(), 0.f, RootBoneIndex, 0));
	}

	// Go through remaining transforms
	for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
	{
		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];

		const FTransform& BoneCSTransform = MeshBasesSaved.Pose.GetComponentSpaceTransform(BoneIndex);
		const FVector& BoneCSPosition = BoneCSTransform.GetLocation();

		LocalBoneTransforms[TransformIndex] = BoneCSTransform;
		InitialBoneTransforms[TransformIndex] = BoneCSTransform;

		// Calculate the combined length of this segment of skeleton
		double const BoneLength = FVector::Dist(BoneCSPosition, LocalBoneTransforms[TransformIndex - 1].GetLocation());

		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(FFABRIKChainLink(BoneCSPosition, BoneLength, BoneIndex, TransformIndex));
			MaximumReach += BoneLength;
		}
		else
		{
			FFABRIKChainLink& ParentLink = Chain[Chain.Num() - 1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}


	const int32 NumChainLinks = Chain.Num();
	const bool bBoneLocationUpdated = AnimationCore::SolveFabrik(Chain, FootCSTransform.GetLocation(), MaximumReach, FabrikPrecision, FabrikIterations);


	if (bBoneLocationUpdated)
	{
		// First step: update bone transform positions from chain links.
		for (int32 LIndex = 0; LIndex < NumChainLinks; LIndex++)
		{
			const FFABRIKChainLink& ChainLink = Chain[LIndex];
			LocalBoneTransforms[ChainLink.TransformIndex].SetTranslation(ChainLink.Position);

			const int32 NumChildren = ChainLink.ChildZeroLengthTransformIndices.Num();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				LocalBoneTransforms[ChainLink.ChildZeroLengthTransformIndices[ChildIndex]].SetTranslation(ChainLink.Position);
			}
		}


		// 
		if (bIsSpiderMode && LocalBoneTransforms.Num() >= 3)
		{
			FTransform& ThighTM = LocalBoneTransforms[0];
			FTransform& KneeTM = LocalBoneTransforms[1];
			FTransform& FootTM = LocalBoneTransforms[2];

			const FVector ThighPos = ThighTM.GetLocation();
			const FVector FootPos = FootTM.GetLocation();
			const FVector LegDir = (FootPos - ThighPos).GetSafeNormal();

			// Component空間の下方向
			FVector KneeBendDir = ComponentToWorld.InverseTransformVectorNoScale(-ComponentToWorld.GetUnitAxis(EAxis::Z)).GetSafeNormal();

			// 足設定の KneeDirectionOffset を使えるなら優先する
			const int32 SelectIndex = SpineFeetPair[FeetIndex].OrderIndexArray[HitIndex];
			if (SolverInputData.FeetBones.IsValidIndex(SelectIndex))
			{
				const FCustomBone_FootData& FeetData = SolverInputData.FeetBones[SelectIndex];

				FVector DesiredKneeDir = FeetData.KneeDirectionOffset;

				const FQuat ForwardRotDiff = FQuat::FindBetweenNormals(CharacterForwardDirectionVector_CS, PolesForwardDirectionVector_CS);

				DesiredKneeDir = ForwardRotDiff.RotateVector(DesiredKneeDir).GetSafeNormal();

				if (!DesiredKneeDir.IsNearlyZero())
				{
					KneeBendDir = DesiredKneeDir;
				}
			}

			// 脚の軸に沿う成分を除去して、純粋な曲げ方向だけ残す
			KneeBendDir = FVector::VectorPlaneProject(KneeBendDir, LegDir).GetSafeNormal();

			if (KneeBendDir.IsNearlyZero())
			{
				KneeBendDir = KneeBendBaseDir.GetSafeNormal();
			}

			const float UpperLen = FVector::Dist(InitialBoneTransforms[0].GetLocation(), InitialBoneTransforms[1].GetLocation());
			const float LowerLen = FVector::Dist(InitialBoneTransforms[1].GetLocation(), InitialBoneTransforms[2].GetLocation());

			// M字の強さ
			const float TotalLen = UpperLen + LowerLen;
			const float RawKneeDropDist = TotalLen * KneeDropIntensity;
			const float KneeDropDist = FMath::Clamp(RawKneeDropDist, TotalLen * 0.15f, TotalLen * 0.55f);

			const FVector MidPos = (ThighPos + FootPos) * 0.5f;
			const FVector TargetKneePos = MidPos + KneeBendDir * KneeDropDist;

			FVector NewKneePos = FMath::VInterpTo(KneeTM.GetLocation(), TargetKneePos, CachedDeltaSeconds, KneeDropInterpSpeed);

			// 長さ保持: thigh-knee と knee-foot を元の長さへ寄せる
			NewKneePos = ThighPos + (NewKneePos - ThighPos).GetSafeNormal() * UpperLen;

			const FVector KneeToFoot = FootPos - NewKneePos;
			if (!KneeToFoot.IsNearlyZero())
			{
				const FVector NewFootPos = NewKneePos + KneeToFoot.GetSafeNormal() * LowerLen;
				FootTM.SetLocation(NewFootPos);
			}

			KneeTM.SetLocation(NewKneePos);
		}


		// FABRIK algorithm - re-orientation of bone local axes after translation calculation
		for (int32 LIndex = 0; LIndex < NumChainLinks - 1; LIndex++)
		{
			const FFABRIKChainLink& CurrentLink = Chain[LIndex];
			const FFABRIKChainLink& ChildLink = Chain[LIndex + 1];

			// Calculate pre-translation vector between this bone and child
			const FVector& OldDir = (GetCurrentLocation(MeshBasesSaved.Pose, FCompactPoseBoneIndex(ChildLink.BoneIndex)) -
				GetCurrentLocation(MeshBasesSaved.Pose, FCompactPoseBoneIndex(CurrentLink.BoneIndex))).GetUnsafeNormal();

			// Get vector from the post-translation bone to it's child

			// 膝を落とした後の位置関係で回転
			const FVector NewDir = (LocalBoneTransforms[ChildLink.TransformIndex].GetLocation() - LocalBoneTransforms[CurrentLink.TransformIndex].GetLocation()).GetSafeNormal();

			const FVector& RotationAxis = FVector::CrossProduct(OldDir, NewDir).GetSafeNormal();
			const float SafeDot = FMath::Clamp(FVector::DotProduct(OldDir, NewDir), -1.0f, 1.0f);
			double const RotationAngle = FMath::Acos(SafeDot);
			const FQuat& DeltaRotation = FQuat(RotationAxis, RotationAngle);

			// We're going to multiply it, in order to not have to re-normalize the final quaternion, it has to be a unit quaternion.
			checkSlow(DeltaRotation.IsNormalized());

			// Calculate absolute rotation and set it
			FTransform& CurrentBoneTransform = LocalBoneTransforms[CurrentLink.TransformIndex];
			CurrentBoneTransform.SetRotation(DeltaRotation * CurrentBoneTransform.GetRotation());
			CurrentBoneTransform.NormalizeRotation();

			// Update zero length children if any
			const int32 NumChildren = CurrentLink.ChildZeroLengthTransformIndices.Num();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				FTransform& ChildBoneTransform = LocalBoneTransforms[CurrentLink.ChildZeroLengthTransformIndices[ChildIndex]];
				ChildBoneTransform.SetRotation(DeltaRotation * ChildBoneTransform.GetRotation());
				ChildBoneTransform.NormalizeRotation();
			}
		}
	}

	const int32 TipBoneTransformIndex = LocalBoneTransforms.Num() - 1;

	switch (EffectorRotationSource)
	{
	case EFabrikBoneRotationSource::KeepLocalSpaceRotation:
		LocalBoneTransforms[TipBoneTransformIndex] = MeshBasesSaved.Pose.GetLocalSpaceTransform(BoneIndices[TipBoneTransformIndex]) * LocalBoneTransforms[TipBoneTransformIndex - 1];
		break;
	case EFabrikBoneRotationSource::CopyFromTarget:
		LocalBoneTransforms[TipBoneTransformIndex].SetRotation(FootCSTransform.GetRotation());
		break;
	case EFabrikBoneRotationSource::KeepComponentSpaceRotation:
		// Don't change the orientation at all
		break;
	default:
		break;
	}

	for (int32 Idx = 0; Idx < NumTransforms; Idx++)
	{
		FTransform FinalTransform;
		if (AlphaTemp >= 1.0f)
		{
			FinalTransform = LocalBoneTransforms[Idx];
		}
		else if (AlphaTemp <= 0.0f)
		{
			FinalTransform = InitialBoneTransforms[Idx];
		}
		else
		{
			FinalTransform.Blend(InitialBoneTransforms[Idx], LocalBoneTransforms[Idx], AlphaTemp);
		}

		OutBoneTransforms.Add(FBoneTransform(BoneIndices[Idx], FinalTransform));
	}

}


void FAnimNode_CustomFeetSolver::PrepareAnimatedPoseInfo_AnyThread(FComponentSpacePoseContext& Output)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();


	if (!SpineFeetPair.IsEmpty() && !SpineTransformPairs.IsEmpty() && !Output.Pose.GetPose().ContainsNaN())
	{
		for (int32 i = 0; i < SpineFeetPair.Num(); i++)
		{
			const FCompactPoseBoneIndex FeetBoneIndex = SpineFeetPair[i].SpineBoneRef.GetCompactPoseIndex(BoneContainer);
			const FTransform FeetBoneIndexCS = Output.Pose.GetComponentSpaceTransform(FeetBoneIndex);
			SpineTransformPairs[i].SpineInvolved = FeetBoneIndexCS * ComponentToWorld;

			for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
			{
				const FCompactPoseBoneIndex ChildFeetBoneIndex = SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer);
				const FTransform ChildFeetBoneIndexCS = Output.Pose.GetComponentSpaceTransform(ChildFeetBoneIndex);
				SpineTransformPairs[i].AssociatedFootArray[j] = ChildFeetBoneIndexCS * ComponentToWorld;

				for (int32 f = 0; f < SpineTransformPairs[i].AssociatedFingerArray[j].Num(); f++)
				{
					const FBoneReference& FingerBoneRef = SpineFeetPair[i].FingerArray[j][f];
					const FCompactPoseBoneIndex FingerBoneIndex = FingerBoneRef.GetCompactPoseIndex(BoneContainer);

					const FTransform FingerBoneIndexCS = Output.Pose.GetComponentSpaceTransform(FingerBoneIndex);
					SpineTransformPairs[i].AssociatedFingerArray[j][f] = FingerBoneIndexCS * ComponentToWorld;
				}
			}

		}
	}


}

bool FAnimNode_CustomFeetSolver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	bool bIsFeetBone = true;

	for (int32 i = 0; i < SolverBoneData.FeetBones.Num(); i++)
	{
		if (!SolverBoneData.FeetBones[i].IsValidToEvaluate(RequiredBones) || 
			!SolverBoneData.KneeBones[i].IsValidToEvaluate(RequiredBones) ||
			!SolverBoneData.ThighBones[i].IsValidToEvaluate(RequiredBones))
		{
			bIsFeetBone = false;
		}

	}

	return (RequiredBones.IsValid() && 
		!bSolveShouldFail &&
		bIsFeetBone &&
		SolverBoneData.SpineBone.IsValidToEvaluate(RequiredBones) &&
		SolverBoneData.Pelvis.IsValidToEvaluate(RequiredBones) &&
		RequiredBones.BoneIsChildOf(SolverBoneData.SpineBone.BoneIndex, SolverBoneData.Pelvis.BoneIndex));
}

/// <summary>
/// initialize
/// </summary>
/// <param name="RequiredBones"></param>
void FAnimNode_CustomFeetSolver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	bSolveShouldFail = false;
	SolverBoneData.SpineBone = FBoneReference(SolverInputData.ChestBoneName);
	SolverBoneData.SpineBone.Initialize(RequiredBones);
	SolverBoneData.Pelvis = FBoneReference(SolverInputData.PelvisBoneName);
	SolverBoneData.Pelvis.Initialize(RequiredBones);


	if (!RequiredBones.BoneIsChildOf(SolverBoneData.SpineBone.BoneIndex, SolverBoneData.Pelvis.BoneIndex))
	{
		bSolveShouldFail = true;
	}

	if (!bSolveShouldFail)
	{
		FootBoneRefArray.Empty();
		FeetRootHeights.Empty();
		FeetTipLocations.Empty();
		FeetWidthSpacing.Empty();
		FeetFingerHeights.Empty();
		TotalSpineBoneArray.Empty();
		SpineFeetPair.Empty();


		SpineTransformPairs.Empty();
		SpineAnimatedTransformPairs.Empty();
		SpineHitPairs.Empty();
		FeetModofyTransformArray.Empty();
		FeetModifiedNormalArray.Empty();
		FeetImpactPointArray.Empty();

		KneeAnimatedTransformArray.Empty();
		KneeBoneRefArray.Empty();

		bSolveShouldFail = false;
		TotalSpineBoneArray = BoneArrayMachine_Spine(RequiredBones, 0, SolverInputData.ChestBoneName, SolverInputData.PelvisBoneName, false);

		Algo::Reverse(TotalSpineBoneArray);

		for (int32 i = 0; i < SolverInputData.FeetBones.Num(); i++)
		{
			for (int32 j = 0; j < SolverInputData.FeetBones.Num(); j++)
			{
				if (i != j)
				{
					if (SolverInputData.FeetBones[i].FeetBoneName == SolverInputData.FeetBones[j].FeetBoneName)
					{
						bSolveShouldFail = true;
					}
				}
			}
			BoneArrayMachine_Feet(RequiredBones, i, SolverInputData.FeetBones[i], SolverInputData.PelvisBoneName, true);
		}


		if (SolverInputData.PelvisBoneName == SolverInputData.ChestBoneName)
		{
			bSolveShouldFail = true;
		}

		for (int32 Index = SpineFeetPair.Num() - 1; Index >= 0; --Index)
		{
			if (SpineFeetPair[Index].FeetArray.Num() == 0)
			{
				SpineFeetPair.RemoveAt(Index);
			}
		}
		SpineFeetPair.Shrink();


		SpineFeetPair = SwapSpinePairs(SpineFeetPair);

		for (int32 i = 0; i < SpineFeetPair.Num(); i++)
		{
			SpineFeetPair[i].FingerArray.Empty();
			SpineFeetPair[i].FingerChainNumArray.Empty();
			SpineFeetPair[i].FingerArray.SetNum(SpineFeetPair[i].FeetArray.Num());
			SpineFeetPair[i].FingerChainNumArray.SetNum(SpineFeetPair[i].FeetArray.Num());

			for (int32 j = 0; j < SpineFeetPair[i].FeetArray.Num(); j++)
			{
				const auto FootData = SolverInputData.FeetBones[SpineFeetPair[i].OrderIndexArray[j]];
				for (int32 FingerIndex = 0; FingerIndex < FootData.FingerBoneArray.Num(); FingerIndex++)
				{
					const FName FingerName = FootData.FingerBoneArray[FingerIndex].FingerBoneName;
					FBoneReference FingerBoneRef = FBoneReference(FingerName);
					FingerBoneRef.Initialize(RequiredBones);
					SpineFeetPair[i].FingerArray[j].Add(FingerBoneRef);

					if (!FingerBoneRef.IsValidToEvaluate())
					{
						bSolveShouldFail = true;
					}

					const int32 ChainIndex = FootData.FingerBoneArray[FingerIndex].ChainNumber;
					SpineFeetPair[i].FingerChainNumArray[j].Add(ChainIndex);
				}
			}
		}


		SpineTransformPairs.AddDefaulted(SpineFeetPair.Num());
		SpineAnimatedTransformPairs.AddDefaulted(SpineFeetPair.Num());
		KneeAnimatedTransformArray.AddDefaulted(SolverInputData.FeetBones.Num());
		FeetModofyTransformArray.AddDefaulted(SpineFeetPair.Num());
		FeetModifiedNormalArray.AddDefaulted(SpineFeetPair.Num());
		FeetImpactPointArray.AddDefaulted(SpineFeetPair.Num());
		FeetFingerTransformArray.AddDefaulted(SpineFeetPair.Num());
		FootKneeOffsetArray.AddDefaulted(SpineFeetPair.Num());
		FootAlphaArray.AddDefaulted(SpineFeetPair.Num());
		SpineHitPairs.AddDefaulted(SpineFeetPair.Num());


		FeetRootHeights.AddDefaulted(SpineFeetPair.Num());
		FeetFingerHeights.AddDefaulted(SpineFeetPair.Num());
		FeetTipLocations.AddDefaulted(SpineFeetPair.Num());
		FeetWidthSpacing.AddDefaulted(SpineFeetPair.Num());

		const int32 FeetBonesNum = SolverInputData.FeetBones.Num();

		// cache
		KneeBoneRefArray.AddDefaulted(FeetBonesNum);
		FootBoneRefArray.AddDefaulted(FeetBonesNum);

		for (int32 i = 0; i < SpineFeetPair.Num(); i++)
		{
			const FQuadrupedBone_SpineFeetPair& FeetPair = SpineFeetPair[i];
			SpineHitPairs[i].FeetHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineHitPairs[i].FeetFrontHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineHitPairs[i].FeetBackHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineHitPairs[i].FeetLeftHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineHitPairs[i].FeetRightHitArray.AddDefaulted(FeetPair.FeetArray.Num());

			SpineHitPairs[i].FingerHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineHitPairs[i].OriginalFingerHitArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineTransformPairs[i].AssociatedFootArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineTransformPairs[i].AssociatedFingerArray.AddDefaulted(FeetPair.FeetArray.Num());


			for (int32 hitx = 0; hitx < SpineHitPairs[i].FingerHitArray.Num(); hitx++)
			{
				SpineHitPairs[i].FingerHitArray[hitx].AddDefaulted(FeetPair.FingerArray[hitx].Num());
			}

			for (int32 hitx = 0; hitx < SpineHitPairs[i].OriginalFingerHitArray.Num(); hitx++)
			{
				SpineHitPairs[i].OriginalFingerHitArray[hitx].AddDefaulted(FeetPair.FingerArray[hitx].Num());
			}

			for (int32 hitx = 0; hitx < SpineTransformPairs[i].AssociatedFingerArray.Num(); ++hitx)
			{
				SpineTransformPairs[i].AssociatedFingerArray[hitx].AddDefaulted(FeetPair.FingerArray[hitx].Num());
			}

			SpineAnimatedTransformPairs[i].AssociatedFootArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineAnimatedTransformPairs[i].AssociatedKneeArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineAnimatedTransformPairs[i].AssociatedToeArray.AddDefaulted(FeetPair.FeetArray.Num());
			SpineAnimatedTransformPairs[i].AssociatedToeArray.AddDefaulted(FeetPair.FeetArray.Num());

			FeetModofyTransformArray[i].AddDefaulted(FeetPair.FeetArray.Num());
			FeetModifiedNormalArray[i].AddDefaulted(FeetPair.FeetArray.Num());
			FeetImpactPointArray[i].AddDefaulted(FeetPair.FeetArray.Num());
			FeetFingerTransformArray[i].AddDefaulted(FeetPair.FeetArray.Num());

			for (int32 hitx = 0; hitx < FeetPair.FingerArray.Num(); ++hitx)
			{
				FeetFingerTransformArray[i][hitx].AddDefaulted(FeetPair.FingerArray[hitx].Num());
			}

			FootKneeOffsetArray[i].AddDefaulted(FeetPair.FeetArray.Num());
			FootAlphaArray[i].AddDefaulted(FeetPair.FeetArray.Num());

			for (int32 JIndex = 0; JIndex < FeetPair.FeetArray.Num(); JIndex++)
			{
				FeetRootHeights[i].Add(0.0f);
				FeetWidthSpacing[i].Add(0.0f);
				FeetTipLocations[i].Add(FVector::ZeroVector);

				const int32 FingerSize = SolverInputData.FeetBones[FeetPair.OrderIndexArray[JIndex]].FingerBoneArray.Num();
				FeetFingerHeights[i].AddDefaulted(FingerSize);
				FootAlphaArray[i][JIndex] = 1.0f;
			}
		}


		for (int32 Idx = 0; Idx < FeetBonesNum; Idx++)
		{
			KneeBoneRefArray[Idx] = FBoneReference(SolverInputData.FeetBones[Idx].KneeBoneName);
			KneeBoneRefArray[Idx].Initialize(RequiredBones);
		}

		for (int32 Idx = 0; Idx < FeetBonesNum; Idx++)
		{
			FootBoneRefArray[Idx] = FBoneReference(SolverInputData.FeetBones[Idx].FeetBoneName);
			FootBoneRefArray[Idx].Initialize(RequiredBones);
		}

		bool bIsSwapped = false;

		do
		{
			bIsSwapped = false;
			for (int32 Index = 1; Index < FootBoneRefArray.Num(); Index++)
			{
				if (FootBoneRefArray[Index - 1].BoneIndex < FootBoneRefArray[Index].BoneIndex)
				{
					FBoneReference BoneRef = FootBoneRefArray[Index - 1];
					FootBoneRefArray[Index - 1] = FootBoneRefArray[Index];
					FootBoneRefArray[Index] = BoneRef;
					bIsSwapped = true;
				}
			}
		} while (bIsSwapped);

		FootAlphaArray.AddDefaulted(FootBoneRefArray.Num());

		if (SolverInputData.ChestBoneName == SolverInputData.PelvisBoneName)
		{
			bSolveShouldFail = true;
		}

		if (SpineFeetPair.Num() > 0)
		{
			if (SpineFeetPair[0].SpineBoneRef.BoneIndex > SpineFeetPair[SpineFeetPair.Num() - 1].SpineBoneRef.BoneIndex)
			{
				bSolveShouldFail = true;
			}
		}

		SolverBoneData.FeetBones.Empty();
		SolverBoneData.KneeBones.Empty();
		SolverBoneData.ThighBones.Empty();

		for (int32 Index = 0; Index < SolverInputData.FeetBones.Num(); Index++)
		{
			SolverBoneData.FeetBones.Add(FBoneReference(SolverInputData.FeetBones[Index].FeetBoneName));
			SolverBoneData.FeetBones[Index].Initialize(RequiredBones);

			SolverBoneData.KneeBones.Add(FBoneReference(SolverInputData.FeetBones[Index].KneeBoneName));
			SolverBoneData.KneeBones[Index].Initialize(RequiredBones);
			SolverBoneData.ThighBones.Add(FBoneReference(SolverInputData.FeetBones[Index].ThighBoneName));
			SolverBoneData.ThighBones[Index].Initialize(RequiredBones);

		}
		bIsInitialized = true;
	}
}


#pragma region Misc
TArray<FQuadrupedBone_SpineFeetPair> FAnimNode_CustomFeetSolver::SwapSpinePairs(TArray<FQuadrupedBone_SpineFeetPair>& OutSpineFeetArray)
{

	for (int32 Index = 1; Index < OutSpineFeetArray.Num(); Index++)
	{
		FQuadrupedBone_SpineFeetPair& SpinePair = OutSpineFeetArray[Index];
		const int32 NumFeet = SpinePair.FeetArray.Num();

		if (NumFeet <= 1)
		{
			continue;
		}

		// ソート用のインデックス配列を生成
		TArray<int32> SortedIndices;
		SortedIndices.SetNumUninitialized(NumFeet);
		for (int32 Idx = 0; Idx < NumFeet; Idx++)
		{
			SortedIndices[Idx] = Idx;
		}

		// BoneIndexの降順でインデックスをソート
		Algo::SortBy(SortedIndices, [&SpinePair](int32 Idx)
			{
				return SpinePair.FeetArray[Idx].BoneIndex;
			}, TGreater<>());

		// ソートが必要かチェック（既にソート済みならスキップ）
		bool bAlreadySorted = true;
		for (int32 Idx = 0; Idx < NumFeet; Idx++)
		{
			if (SortedIndices[Idx] != Idx)
			{
				bAlreadySorted = false;
				break;
			}
		}
		if (bAlreadySorted)
		{
			continue;
		}

		auto ReorderArray = [&SortedIndices](auto& SourceArray)
			{
				using ElementType = typename TRemoveReference<decltype(SourceArray)>::Type::ElementType;
				TArray<ElementType> TempArray;
				TempArray.Reserve(SortedIndices.Num());

				Algo::Transform(SortedIndices, TempArray, [&SourceArray](int32 Idx)
					{
						return SourceArray[Idx];
					});

				return TempArray;
			};

		SpinePair.FeetArray = ReorderArray(SpinePair.FeetArray);
		SpinePair.KneeArray = ReorderArray(SpinePair.KneeArray);
		SpinePair.ThighArray = ReorderArray(SpinePair.ThighArray);
		SpinePair.OrderIndexArray = ReorderArray(SpinePair.OrderIndexArray);

	}

	return OutSpineFeetArray;

}

FVector FAnimNode_CustomFeetSolver::AnimationLocationLerp(const bool bIsHit, const FVector& StartPosition, const FVector& EndPosition, const float DeltaSeconds) const
{
	if (bIgnoreLerping || !bIsHit || bWarpDetected)
	{
		return EndPosition;
	}

	constexpr float Max = 6.0f;
	const FVector Diff = (StartPosition - EndPosition) / FMath::Clamp(Max - LocationLerpSpeed, 1.0f, Max);
	if (LocationInterpType == EIKInterpLocationType::DivisiveLocation)
	{
		return StartPosition - Diff;
	}

	const float InterpSpeed = FMath::Abs(LocationLerpSpeed);
	return FMath::VInterpTo(StartPosition, EndPosition, DeltaSeconds, InterpSpeed);
}

FQuat FAnimNode_CustomFeetSolver::AnimationQuatSlerp(const bool bIsHit, const FQuat& StartRotation, const FQuat& EndRotation, const float DeltaSeconds) const
{
	if (bIgnoreLerping || !bIsHit || bWarpDetected)
	{
		return EndRotation;
	}

	const float Speed = FMath::Max(0.0f, FormatRotationLerp);
	const float Local_Alpha = FMath::Clamp(Speed * DeltaSeconds, 0.0f, 1.0f);

	if (RotationInterpType == EIKInterpRotationType::DivisiveRotation)
	{
		// Start -> End への一回分の割合(1/divisor) で進める
		const float Divisor = FMath::Clamp(FeetRotationSpeed, 1.0f, 10.0f);
		const float DivAlpha = FMath::Clamp(1.0f / Divisor, 0.0f, 1.0f);
		return FQuat::Slerp(StartRotation, EndRotation, DivAlpha).GetNormalized();
	}

	// 通常は速度*DeltaSeconds を alpha として Slerp
	return FQuat::Slerp(StartRotation, EndRotation, Local_Alpha).GetNormalized();
}

TArray<FName> FAnimNode_CustomFeetSolver::BoneArrayMachine_Feet(const FBoneContainer& RequiredBones, const int32 Index, const FCustomBone_FootData& FootData, const FName& EndBoneName, const bool bWasFootBone)
{
	TArray<FName> SpineBoneArray;
	SpineBoneArray.Add(FootData.FeetBoneName);

	if (!bWasFootBone)
	{
		FQuadrupedBone_SpineFeetPair Instance;
		Instance.SpineBoneRef = FBoneReference(FootData.FeetBoneName);
		Instance.SpineBoneRef.Initialize(RequiredBones);
		SpineFeetPair.Add(Instance);
	}

	const FReferenceSkeleton& RefSkel = RequiredBones.GetReferenceSkeleton();

	bool bWasFinish = false;
	int32 IterationCount = 0;
	constexpr int32 MaxIterationCount = ITERATION_COUNTER;

	do
	{
		if (bWasFootBone)
		{
			if (SpineBoneArray.IsEmpty())
			{
				break;
			}


			if (CheckLoopExist(
				RequiredBones, Index, FootData,
				FootData.FeetBoneName, FootData.KneeBoneName, FootData.ThighBoneName,
				SpineBoneArray.Last(), TotalSpineBoneArray))
			{
				return SpineBoneArray;
			}
		}

		IterationCount++;
		const FName CurBoneName = SpineBoneArray[IterationCount - 1];
		const int32 CurBoneIndex = RefSkel.FindBoneIndex(CurBoneName);
		if (CurBoneIndex == INDEX_NONE)
		{
			break;
		}

		const int32 ParentIndex = RefSkel.GetParentIndex(CurBoneIndex);

		if (ParentIndex != INDEX_NONE)
		{
			const FName ParentName = RefSkel.GetBoneName(ParentIndex);
			SpineBoneArray.Add(ParentName);
		}

		if (!bWasFootBone)
		{
			FQuadrupedBone_SpineFeetPair Instance;
			Instance.SpineBoneRef = FBoneReference(SpineBoneArray.Last());
			Instance.SpineBoneRef.Initialize(RequiredBones);
			SpineFeetPair.Add(Instance);
		}

		if (!bWasFootBone && SpineBoneArray.Last() == EndBoneName)
		{
			return SpineBoneArray;
		}

	} while (IterationCount < MaxIterationCount && !bWasFinish);

	return SpineBoneArray;
}

TArray<FName> FAnimNode_CustomFeetSolver::BoneArrayMachine_Spine(const FBoneContainer& RequiredBones, const int32 Index, const FName& StartBoneName, const FName& EndBoneName, const bool bWasFootBone)
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

	bool bWasFinish = false;
	int32 IterationCount = 0;
	constexpr int32 MaxIterationCount = ITERATION_COUNTER;

	do
	{
		if (bWasFootBone)
		{
			if (CheckLoopExist(RequiredBones, Index, SolverInputData.FeetBones[Index],
				StartBoneName, NAME_None, NAME_None, SpineBoneArray.Last(), TotalSpineBoneArray))
			{
				return SpineBoneArray;
			}
		}

		IterationCount++;
		if (!SpineBoneArray.IsValidIndex(IterationCount - 1))
		{
			continue;
		}

		const FName CurBoneName = SpineBoneArray[IterationCount - 1];
		const int32 CurBoneIndex = RefSkel.FindBoneIndex(CurBoneName);
		if (CurBoneIndex == INDEX_NONE)
		{
			break;
		}

		const int32 ParentIndex = RefSkel.GetParentIndex(CurBoneIndex);

		if (ParentIndex != INDEX_NONE)
		{
			const FName ParentName = RefSkel.GetBoneName(ParentIndex);
			SpineBoneArray.Add(ParentName);
		}

		if (!bWasFootBone)
		{
			FQuadrupedBone_SpineFeetPair Instance;
			Instance.SpineBoneRef = FBoneReference(SpineBoneArray.Last());
			Instance.SpineBoneRef.Initialize(RequiredBones);
			SpineFeetPair.Add(Instance);
		}

		if (!bWasFootBone && SpineBoneArray.Last() == EndBoneName)
		{
			return SpineBoneArray;
		}

	} while (IterationCount < MaxIterationCount && !bWasFinish);
	return SpineBoneArray;
}


bool FAnimNode_CustomFeetSolver::CheckLoopExist(
	const FBoneContainer& RequiredBones,
	const int32 OrderIndex,
	const FCustomBone_FootData& InFootData,
	const FName& StartBone,
	const FName& KneeBone,
	const FName& ThighBone,
	const FName& InputBone,
	const TArray<FName>& TotalSpineBones)
{

	for (int32 Index = 0; Index < TotalSpineBones.Num(); Index++)
	{

		const FName& CurBoneName = TotalSpineBones[Index];
		if (InputBone == CurBoneName)
		{
			if (SpineFeetPair.Num() > Index)
			{
				FQuadrupedBone_SpineFeetPair Instance = FQuadrupedBone_SpineFeetPair();
				Instance.SpineBoneRef = FBoneReference(CurBoneName);
				Instance.SpineBoneRef.Initialize(RequiredBones);

				FBoneReference FootBoneInstance = FBoneReference(StartBone);
				FootBoneInstance.Initialize(RequiredBones);
				Instance.FeetArray.Add(FootBoneInstance);

				SpineFeetPair[Index].SpineBoneRef = Instance.SpineBoneRef;
				SpineFeetPair[Index].FeetArray.Add(FootBoneInstance);
				SpineFeetPair[Index].FeetRotationOffsetArray.Add(InFootData.FeetRotationOffset);
				SpineFeetPair[Index].KneeDirectionOffsetArray.Add(InFootData.KneeDirectionOffset);
				SpineFeetPair[Index].OrderIndexArray.Add(OrderIndex);
				SpineFeetPair[Index].FeetTraceOffsetArray.Add(InFootData.FeetTraceOffset);
				SpineFeetPair[Index].FeetHeightArray.Add(InFootData.FeetHeight);
				SpineFeetPair[Index].FeetRotationLimitArray.Add(InFootData.FeetRotationLimit);

				FBoneReference KneeBoneInstance = FBoneReference(KneeBone);
				KneeBoneInstance.Initialize(RequiredBones);
				SpineFeetPair[Index].KneeArray.Add(KneeBoneInstance);

				FBoneReference ThighBoneInstance = FBoneReference(ThighBone);
				ThighBoneInstance.Initialize(RequiredBones);
				SpineFeetPair[Index].ThighArray.Add(ThighBoneInstance);

				return true;
			}
		}
	}
	return false;
}

#pragma endregion


void FAnimNode_CustomFeetSolver::ApplyLineTrace(
	const FAnimationUpdateContext& Context, 
	const FVector& StartLocation, 
	const FVector& EndLocation, 
	FHitResult& OutHitResult, 
	const FLinearColor& DebugColor, 
	const bool bRenderTrace)
{
	

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const UWorld* World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();
	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();

	const FVector& OwnerDirectionVector = ComponentToWorld.TransformVectorNoScale(CharacterDirectionVectorCS);
	//const FVector& OwnerDirectionVector = ComponentToWorld.GetUnitAxis(EAxis::Z);

	const float CurOwnerScale = (SK->GetOwner()) ? ComponentToWorld.GetScale3D().Z * VirtualScale : 1.0f;

	const float SelectedRadius = TraceRadiusValue;
	const float ScaledTraceRadius = SelectedRadius * CurOwnerScale;

	const EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::None;

	switch (RaycastTraceType)
	{
	case EIKRaycastType::LineTrace:
	{
		UKismetSystemLibrary::LineTraceSingle(SK, StartLocation, EndLocation, TraceChannel, true, IgnoreActors, DebugTrace, OutHitResult, true, DebugColor);
	}
	break;

	case EIKRaycastType::SphereTrace:
	{
		const FVector& Offset = OwnerDirectionVector * ScaledTraceRadius;
		const FVector& Start = StartLocation + Offset;
		const FVector& End = EndLocation + Offset;
		UKismetSystemLibrary::SphereTraceSingle(SK, Start, End, ScaledTraceRadius, TraceChannel, true, IgnoreActors, DebugTrace, OutHitResult, true, DebugColor);
	}
	break;

	case EIKRaycastType::BoxTrace:
	{
		const float TraceHalfLength = (StartLocation - EndLocation).Size() * 0.5f;
		const FVector BoxHalfSize = FVector(ScaledTraceRadius, ScaledTraceRadius, TraceHalfLength);
		const FVector& Center = (StartLocation + EndLocation) * 0.5f;
		const FRotator& TraceRotation = FRotationMatrix::MakeFromZ(OwnerDirectionVector).Rotator();
		UKismetSystemLibrary::BoxTraceSingle(SK, Center, Center, BoxHalfSize, TraceRotation, TraceChannel, true, IgnoreActors, DebugTrace, OutHitResult, true, DebugColor);
	}
	break;
	}

	if (bRenderTrace)
	{
		TraceStartList.Add(StartLocation);
		TraceEndList.Add(EndLocation);
		TraceLinearColor.Add(DebugColor.ToFColor(true));
	}

#if ENABLE_DRAW_DEBUG
	if (bDisplayLineTrace)
	{
		TWeakObjectPtr<UWorld> Editor_World = GEngine->GetWorldFromContextObject(SK, EGetWorldErrorMode::LogAndReturnNull);
		const float LocalScale = CurOwnerScale;

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


void FAnimNode_CustomFeetSolver::ApplyMultiPointTraceBulk(
	const FAnimationUpdateContext& Context,
	const FVector& CenterOrigin,
	const FVector& FrontTarget,
	const FVector& MidLocationLeft,
	const FVector& MidLocationRight,
	const float SideSpacing, 
	const float StartScale, 
	const float EndScale, 
	const int32 SIndex, 
	const int32 FIndex)
{

	TRACE_CPUPROFILER_EVENT_SCOPE(CustomFeetSolver_ApplyMultiPointTraceBulk);

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();

	const FVector OwnerUp = ComponentToWorld.GetUnitAxis(EAxis::Z);
	//const FVector OwnerUp = ComponentToWorld.TransformVectorNoScale(CharacterDirectionVectorCS).GetSafeNormal();

	// --- 1. BoxExtent の修正 ---
	const float FootLength = FVector::Dist(CenterOrigin, FrontTarget);
	const float ScaledRadius = TraceRadiusValue * ComponentScale;
	const float HalfWidth = SideSpacing * ComponentScale;

	// X: 前後の長さ, Y: 左右の幅, Z: スウィープの厚み（薄くして精度を出す）
	const FVector BoxExtent(FootLength * 0.5f + ScaledRadius, HalfWidth + ScaledRadius, 5.0f * ComponentScale);

	const FVector CenterOfBox = (CenterOrigin + FrontTarget) * 0.5f;
	const FVector TraceStart = CenterOfBox + OwnerUp * StartScale;
	const FVector TraceEnd = CenterOfBox - OwnerUp * EndScale;

	TArray<FHitResult> HitResults;
	const EDrawDebugTrace::Type DebugTrace = bDisplayLineTrace ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	UKismetSystemLibrary::BoxTraceMulti(SK, TraceStart, TraceEnd, BoxExtent,
		FRotator(ComponentToWorld.GetRotation()), TraceChannel, true, IgnoreActors,
		DebugTrace, HitResults, true);

	// SearchBest の修正（TargetWS を基準に計算）
	auto SearchBest = [&](const FVector& TargetWS, FHitResult& OutHit)
		{
			float MinScore = 1000000.0f;
			const float NormalDotThreshold = 0.35f;

			for (const FHitResult& Hit : HitResults)
			{
				if (!Hit.GetComponent() || !Hit.GetComponent()->IsPhysicsCollisionEnabled())
				{
					continue;
				}

				if (Hit.GetActor() && Hit.GetActor()->IsA(ACharacter::StaticClass()))
				{
					continue;
				}

				const float NormalDot = FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), OwnerUp);
				if (NormalDot < NormalDotThreshold)
				{
					continue;
				}

				const float DistSqXY = FVector::DistSquaredXY(Hit.ImpactPoint, TargetWS);
				const float DistSqZ = FMath::Square(Hit.ImpactPoint.Z - TargetWS.Z);
				const float CombinedScore = DistSqXY * 8.0f + DistSqZ * 0.1f;

				if (CombinedScore < MinScore)
				{
					MinScore = CombinedScore;
					OutHit = Hit;
				}
			}
		};

	auto RefineHitByLineTrace = [&](const FVector& TargetWS, FHitResult& InOutHit)
		{
			if (!InOutHit.bBlockingHit)
			{
				return;
			}

			FHitResult RefinedHit;
			const FVector Start = TargetWS + OwnerUp * (TraceRadiusValue * ComponentScale + 10.0f);
			const FVector End = TargetWS - OwnerUp * (TraceRadiusValue * ComponentScale + 20.0f);
			UKismetSystemLibrary::LineTraceSingle(SK, Start, End, TraceChannel, true, IgnoreActors, EDrawDebugTrace::None, RefinedHit, true);

			if (RefinedHit.bBlockingHit)
			{
				InOutHit = RefinedHit;
			}
		};

	SearchBest(CenterOrigin, SpineHitPairs[SIndex].FeetHitArray[FIndex]);
	SearchBest(FrontTarget, SpineHitPairs[SIndex].FeetFrontHitArray[FIndex]);
	SearchBest(MidLocationLeft, SpineHitPairs[SIndex].FeetLeftHitArray[FIndex]);
	SearchBest(MidLocationRight, SpineHitPairs[SIndex].FeetRightHitArray[FIndex]);

	RefineHitByLineTrace(CenterOrigin, SpineHitPairs[SIndex].FeetHitArray[FIndex]);
	RefineHitByLineTrace(FrontTarget, SpineHitPairs[SIndex].FeetFrontHitArray[FIndex]);
	RefineHitByLineTrace(MidLocationLeft, SpineHitPairs[SIndex].FeetLeftHitArray[FIndex]);
	RefineHitByLineTrace(MidLocationRight, SpineHitPairs[SIndex].FeetRightHitArray[FIndex]);

	//TraceStartList.Add(TraceStart);
	//TraceEndList.Add(TraceEnd);
	//TraceRadiusList.Add(HalfWidth + ScaledRadius);


	if (bIsCalcFingerJoints)
	{
		for (int32 f = 0; f < SpineHitPairs[SIndex].FingerHitArray[FIndex].Num(); f++)
		{
			FVector FingerLoc = ComponentToWorld.TransformPosition(FeetFingerTransformArray[SIndex][FIndex][f].GetLocation());
			SearchBest(FingerLoc, SpineHitPairs[SIndex].FingerHitArray[FIndex][f]);
			RefineHitByLineTrace(FingerLoc, SpineHitPairs[SIndex].FingerHitArray[FIndex][f]);
		}
	}

}


void FAnimNode_CustomFeetSolver::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const
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

void FAnimNode_CustomFeetSolver::BuildLegRotationArray(FComponentSpacePoseContext& Output, TArray<TArray<FTransform>>& OutFeetRotationArray)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	for (int32 i = 0; i < SpineHitPairs.Num(); i++)
	{
		OutFeetRotationArray.AddDefaulted();

		for (int32 j = 0; j < SpineHitPairs[i].FeetHitArray.Num(); ++j)
		{
			if (!SpineFeetPair[i].FeetArray.IsValidIndex(j))
			{
				OutFeetRotationArray[i].Add(FTransform::Identity);
				continue;
			}

			const FCompactPoseBoneIndex FootBoneIndex = SpineFeetPair[i].FeetArray[j].GetCompactPoseIndex(BoneContainer);
			const FTransform FootCS = Output.Pose.GetComponentSpaceTransform(FootBoneIndex);
			OutFeetRotationArray[i].Add(FootCS);
		}
	}
}
