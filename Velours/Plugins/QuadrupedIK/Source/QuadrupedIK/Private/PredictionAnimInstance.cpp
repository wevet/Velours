// Copyright 2022 wevet works All Rights Reserved.

#include "PredictionAnimInstance.h"
#include "QuadrupedIK.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// mover
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/FloorQueryUtils.h"

float UPredictionAnimInstance::INVALID_TOE_DISTANCE = -9999.f;
float UPredictionAnimInstance::DEFAULT_TOE_HEIGHT_LIMIT = -999.f;



#include UE_INLINE_GENERATED_CPP_BY_NAME(PredictionAnimInstance)

namespace PreditctionDebug
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	TAutoConsoleVariable<int32> CVarDebugFootIKPredictive(
		TEXT("wv.DebugFootIKPredictive"),
		0,
		TEXT("Debug FootIKPredictive\n") TEXT("<=0: Debug off\n") TEXT(">=1: Debug on\n"),
		ECVF_Default);

#endif
}


// log LogQuadrupedIK Verbose

void FIKBaseAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	Super::Initialize(InAnimInstance);
}

bool FIKBaseAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	return Super::Evaluate(Output);
}

void FIKBaseAnimInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	FAnimInstanceProxy::PostUpdate(InAnimInstance);

	if (IKDebugData.bValid)
	{
		if (UWorld* World = InAnimInstance->GetWorld())
		{
			DrawDebugSphere(World, IKDebugData.CenterOfMass, IKDebugData.Radius, 16, FColor::Red, false, 0.0f);
		}

		IKDebugData.bValid = false;
	}
}


UPredictionAnimInstance::UPredictionAnimInstance()
{
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;

	bDrawDebug = false;
	bDrawDebugForToe = false;
	bDrawDebugForPelvis = false;
	bDrawDebugForReactFootIK = false;

	bEnableCurvePredictive = false;
	bEnableToeVelocityPredictive = true;
	bEnablePastPathPredictive = true;
	bEnableDefaultDistancePredictive = true;

	RightToeName = FName(TEXT("ball_r"));
	LeftToeName = FName(TEXT("ball_l"));
	RightFootName = FName(TEXT("foot_r"));
	LeftFootName = FName(TEXT("foot_l"));
}

FAnimInstanceProxy* UPredictionAnimInstance::CreateAnimInstanceProxy()
{
	return new FIKBaseAnimInstanceProxy(this);
}

bool UPredictionAnimInstance::EnableFootIK_Implementation() const
{
	return true;
}

void UPredictionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		return;
	}

	bDrawDebugForToe = bDrawDebug & bDrawDebugForToe;
	bDrawDebugForPelvis = bDrawDebug & bDrawDebugForPelvis;
	bDrawDebugForReactFootIK = bDrawDebug & bDrawDebugForReactFootIK;

	IgnoreActors.Add(OwnerPawn);

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);

	if (Character)
	{
		CharacterMovementComponent = Character->GetCharacterMovement();
		CapsuleComponent = Character->GetCapsuleComponent();
	}

	CharacterMoverComponent = OwnerPawn->FindComponentByClass<UCharacterMoverComponent>();

	if (!CapsuleComponent)
	{
		CapsuleComponent = OwnerPawn->FindComponentByClass<UCapsuleComponent>();
	}

	bIsPawnTypeMover = IsValid(CharacterMoverComponent);
	PredictionFootIKComponent = TryGetPawnOwner()->FindComponentByClass<UPredictionFootIKComponent>();

	const FVector RightInitialToePos = GetOwningComponent()->GetSkinnedAsset()->GetComposedRefPoseMatrix(RightToeName).GetOrigin();
	const FVector LeftInitialToePos = GetOwningComponent()->GetSkinnedAsset()->GetComposedRefPoseMatrix(LeftToeName).GetOrigin();
	RightToePathInfo.SetToeContactFloorHeight(RightInitialToePos.Z + ToeLeaveFloorOffset);
	LeftToePathInfo.SetToeContactFloorHeight(LeftInitialToePos.Z + ToeLeaveFloorOffset);

}

void UPredictionAnimInstance::NativeUninitializeAnimation()
{

	Super::NativeUninitializeAnimation();
}


void UPredictionAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();


}

void UPredictionAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
}

void UPredictionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	APawn* Pawn = TryGetPawnOwner();
	if (!IsValid(Pawn))
	{
		return;
	}

	if (!bIsArrowPredictionFunction)
	{
		return;
	}

	if (IsValid(PredictionFootIKComponent))
	{
		LstCharacterBottomLocation = CurCharacterBottomLocation;
		CurCharacterBottomLocation = TryGetPawnOwner()->GetActorLocation() - FVector(0.f, 0.f, CapsuleComponent->GetScaledCapsuleHalfHeight());

		const FVector CurrentMoveDirection = (CurCharacterBottomLocation - LstCharacterBottomLocation).GetSafeNormal2D();


		//bool AbnormalMove = UKismetMathLibrary::Dot_VectorVector(
		//	(CurCharacterBottomLocation - LstCharacterBottomLocation).GetSafeNormal2D(),
		//	TryGetPawnOwner()->GetActorForwardVector()) < AbnormalMoveCosAngle;

		//AbnormalMove ? AbnormalMoveTime += DeltaSeconds : AbnormalMoveTime = 0.f;

		bool bAbnormalMove = false;

		if (!CurrentMoveDirection.IsNearlyZero())
		{
			if (!LastValidMoveDirection.IsNearlyZero())
			{
				const float MoveDirectionDot = FVector::DotProduct(CurrentMoveDirection, LastValidMoveDirection);
				bAbnormalMove = MoveDirectionDot < AbnormalMoveCosAngle;
			}

			LastValidMoveDirection = CurrentMoveDirection;
		}

		bAbnormalMove ? AbnormalMoveTime += DeltaSeconds : AbnormalMoveTime = 0.f;

		const bool FinalAbnormalMove = AbnormalMoveTime >= AbnormalMoveTimeLimit;

		float Dist2DSquared = UKismetMathLibrary::Vector_Distance2DSquared(CurCharacterBottomLocation, LstCharacterBottomLocation);
		const bool JustTeleported = Dist2DSquared > TeleportedDistanceThreshold * TeleportedDistanceThreshold;

		WeightOfDisableFootIK = JustTeleported ? 1.f : WeightOfDisableFootIK;
		const bool ValidDisableFootIKTick = WeightOfDisableFootIK > 0.f;

		if (ValidDisableFootIKTick)
		{
			//UE_LOG(LogQuadrupedIK, Verbose, TEXT("-------------DisableFootIK---------CurWeight:%f"), WeightOfDisableFootIK);
			TickDisableFootIK(DeltaSeconds, CurMeshWorldPosZ, WeightOfDisableFootIK);
		}

		const bool IsForceDisable = ForceDisableFootIK();
		const bool IsEnable = EnableFootIK() && !IsForceDisable;
		const bool ValidPredictiveFootIKTick = TickPredictiveFootIK(
			DeltaSeconds,
			CurMeshWorldPosZ,
			JustTeleported || IsForceDisable,
			FinalAbnormalMove);

		const bool ValidReactFootIKTick = !JustTeleported && IsEnable;
		const bool ValidFootIKTick = ValidPredictiveFootIKTick || ValidReactFootIKTick;

		float MinHitZ = 0.f;
		if (ValidFootIKTick)
		{
			FootIKByHeightOffset = true;
			TraceForTwoFoots(
				DeltaSeconds, 
				MinHitZ, 
				CurRightFootWorldPosZ, 
				CurLeftFootWorldPosZ,
				RightFootHitNormal,
				LeftFootHitNormal);

			RightFootHeightOffset = CurRightFootWorldPosZ - CurCharacterBottomLocation.Z;
			LeftFootHeightOffset = CurLeftFootWorldPosZ - CurCharacterBottomLocation.Z;
			WeightOfDisableFootIK = 0.f;
		}
		else
		{
			FootIKByHeightOffset = false;
			CurRightFootWorldPosZ = CurCharacterBottomLocation.Z;
			CurLeftFootWorldPosZ = CurCharacterBottomLocation.Z;
			WeightOfDisableFootIK = FMath::FInterpTo(WeightOfDisableFootIK, 1.f, DeltaSeconds, MeshPosZInterpSpeedWhenDisableFootIK);
		}

		if (ValidReactFootIKTick && !ValidPredictiveFootIKTick && !ValidDisableFootIKTick)
		{
			TickReactFootIK(DeltaSeconds, CurMeshWorldPosZ, MinHitZ);
		}

		float RawPelvisOffset = CurMeshWorldPosZ - CurCharacterBottomLocation.Z;
		RawPelvisOffset = FMath::Clamp(RawPelvisOffset, -1.f * PelvisHeightDownThreshold, PelvisHeightUpThreshold);
		PelvisFinalOffset = FMath::FInterpTo(PelvisFinalOffset, RawPelvisOffset, DeltaSeconds, PelvisInterpSpeed);
	}
	else
	{
		PredictionFootIKComponent = TryGetPawnOwner()->FindComponentByClass<UPredictionFootIKComponent>();

		WeightOfDisableFootIK = 1.f;
		PelvisFinalOffset = 0.f;
	}

	FootIKWeight = FMath::Clamp(1.f - WeightOfDisableFootIK, 0.f, 1.f);
}


bool UPredictionAnimInstance::TickPredictiveFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, bool BlockPredictive, bool AbnormalMove)
{
	Step0_Prepare(DeltaSeconds);

	const FTransform& ComponentToWorld = GetOwningComponent()->GetComponentToWorld();

	if (!BlockPredictive && ShouldRunPredictive())
	{
		bool IsTotalPathStart = RightToePathInfo.IsPathStarted || LeftToePathInfo.IsPathStarted;
		const float Dist = !IsTotalPathStart ? DefaultToeFirstPathDistance : DefaultToeFirstPathDistance * 2.f;

		const float LegLength = CapsuleComponent->GetScaledCapsuleHalfHeight();

		RightToePathInfo.SetStrideRatio(StrideRatioRange);
		LeftToePathInfo.SetStrideRatio(StrideRatioRange);

		// tick contact state and path
		RightToePathInfo.Update(
			GetOwningComponent(), 
			RightToeCSPos,
			LeftToeCSPos,
			EPredictionMotionFoot::Right, 
			RightToeName, 
			LegLength,
			LeaveHysteresisThreshold);

		LeftToePathInfo.Update(
			GetOwningComponent(), 
			RightToeCSPos, 
			LeftToeCSPos, 
			EPredictionMotionFoot::Left,
			LeftToeName, 
			LegLength,
			LeaveHysteresisThreshold);

		if (RightToePathInfo.IsLeaveStart())
		{
			RightToePathInfo.SetDefaultPathDistance(Dist);
		}

		if (LeftToePathInfo.IsLeaveStart())
		{
			LeftToePathInfo.SetDefaultPathDistance(Dist);
		}


		// r toe contact pos predictive, and compare with last pos
		FVector RightToeEndPos = FVector::ZeroVector;
		const bool IsValidForRightPredictive = Step1_PredictiveToeEndPos(
			RightToeEndPos,
			RightToePathInfo,
			CurRightToeCurveValue,
			RightToeName);

		// l toe contact pos predictive, and compare with last pos
		FVector LeftToeEndPos = FVector::ZeroVector;
		const bool IsValidForLeftPredictive = Step1_PredictiveToeEndPos(
			LeftToeEndPos,
			LeftToePathInfo,
			CurLeftToeCurveValue,
			LeftToeName);

		if (IsValidForRightPredictive)
		{
			Step2_TraceToePath(
				RightToePath,
				RightToeHeightLimit, 
				RightToePathInfo.LeaveFloorPos, 
				RightToePathInfo.CurToePos, 
				RightToeEndPos,
				RightToeName, 
				DeltaSeconds);
		}
		else
		{
			RightToePath.Empty();
			RightToeHeightLimit = DEFAULT_TOE_HEIGHT_LIMIT;
		}

		if (IsValidForLeftPredictive)
		{
			Step2_TraceToePath(
				LeftToePath, 
				LeftToeHeightLimit, 
				LeftToePathInfo.LeaveFloorPos,
				LeftToePathInfo.CurToePos, 
				LeftToeEndPos, 
				LeftToeName, 
				DeltaSeconds);
		}
		else
		{
			LeftToePath.Empty();
			LeftToeHeightLimit = DEFAULT_TOE_HEIGHT_LIMIT;
		}


		// to end pos path is walkable
		if (!AbnormalMove && (RightToePath.Num() > 1 || LeftToePath.Num() > 1))
		{
			UE_LOG(LogQuadrupedIK, VeryVerbose, TEXT("ToePath Num R: %d L: %d"), RightToePath.Num(), LeftToePath.Num());

			FVector MoveDirection = GetOwnerVelocity().GetSafeNormal2D();

			if (MoveDirection.IsNearlyZero())
			{
				MoveDirection = TryGetPawnOwner()->GetActorForwardVector().GetSafeNormal2D();
			}


			FVector RightToePredictivePos = FVector::ZeroVector;
			float RightToeEndDistance = INVALID_TOE_DISTANCE;
			if (RightToePath.Num() > 1)
			{
				RightToePredictivePos = GetToePredictivePos(OutTargetMeshPosZ, RightToePath, RightToeName);
				//RightToeEndDistance = ComponentToWorld.InverseTransformPositionNoScale(RightToePredictivePos).Y;

				RightToeEndDistance = FVector::DotProduct(RightToePredictivePos - CurCharacterBottomLocation, MoveDirection);
			}

			FVector LeftToePredictivePos = FVector::ZeroVector;
			float LeftToeEndDistance = INVALID_TOE_DISTANCE;
			if (LeftToePath.Num() > 1)
			{
				LeftToePredictivePos = GetToePredictivePos(OutTargetMeshPosZ, LeftToePath, LeftToeName);
				//LeftToeEndDistance = ComponentToWorld.InverseTransformPositionNoScale(LeftToePredictivePos).Y;

				LeftToeEndDistance = FVector::DotProduct(LeftToePredictivePos - CurCharacterBottomLocation, MoveDirection);
			}

			if (bDrawDebugForToe)
			{
				DebugDrawToePredictionDetailed(
					RightToeDebugData,
					RightToePath,
					FLinearColor::Yellow, 
					TEXT("Right"));

				DebugDrawToePredictionDetailed(
					LeftToeDebugData,
					LeftToePath,
					FLinearColor::Green,
					TEXT("Left"));
			}

			Step3_CalcMeshPosZ(
				OutTargetMeshPosZ,
				RightToeEndDistance,
				LeftToeEndDistance,
				RightToePathInfo.IsContacting(),
				LeftToePathInfo.IsContacting(),
				RightToePredictivePos,
				LeftToePredictivePos,
				DeltaSeconds);


		}
		else
		{
			CurMotionFoot = EPredictionMotionFoot::None;

			UE_LOG(LogQuadrupedIK, VeryVerbose, TEXT("ToePredictivePos unwalkable"));
		}
	}
	else
	{
		RightToePathInfo.Reset();
		LeftToePathInfo.Reset();

		CurMotionFoot = EPredictionMotionFoot::None;
	}

	Step4_Completed();
	return CurMotionFoot != EPredictionMotionFoot::None;
}


void UPredictionAnimInstance::TickReactFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float InMinHitZ)
{
	OutTargetMeshPosZ = UKismetMathLibrary::FInterpTo(OutTargetMeshPosZ, InMinHitZ, DeltaSeconds, MeshPosZInterpSpeedWhenReactFootIK);
}

void UPredictionAnimInstance::TickDisableFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float Weight)
{
	if (Weight < 1.f)
	{
		OutTargetMeshPosZ = UKismetMathLibrary::MapRangeClamped(Weight, 0.f, 1.f, OutTargetMeshPosZ, CurCharacterBottomLocation.Z);
	}
	else
	{
		OutTargetMeshPosZ = CurCharacterBottomLocation.Z;
	}
}


void UPredictionAnimInstance::Step0_Prepare(float DeltaSeconds)
{
	RightToeDebugData = FToePredictionDebugData();
	LeftToeDebugData = FToePredictionDebugData();

	CurveSampling();
	ToePosSampling(DeltaSeconds);
}


bool UPredictionAnimInstance::Step1_PredictiveToeEndPos(
	FVector& OutToeEndPos, 
	const FPredictionToePathInfo& InPastPath, 
	const float& InCurToeCurveValue,
	const FName& InToeName)
{
	bool ValidPredictive = false;

	FToePredictionDebugData& DebugData = InToeName == RightToeName
		? RightToeDebugData
		: LeftToeDebugData;

	DebugData.LeaveFloorPos = InPastPath.LeaveFloorPos;

	if (InPastPath.IsPathStarted)
	{
		if (InPastPath.IsContacting())
		{
			ValidPredictive = true;
			DebugData.PredictionOrigin = InPastPath.CurToePos;
			OutToeEndPos = InPastPath.CurToePos;
			DebugData.bUsedContact = true;
		}
		else if (bEnablePastPathPredictive && InPastPath.IsPathValid)
		{
			ValidPredictive = true;
			DebugData.PredictionOrigin = InPastPath.LeaveFloorPos;
			CalcToeEndPosByPastPath(OutToeEndPos, InPastPath);
			DebugData.bUsedPastPath = true;
		}
		else if (bEnableToeVelocityPredictive && IsToeVelocityPredictable(InToeName))
		{
			ValidPredictive = true;

			const FToeRuntimeInfo& ToeInfo = InToeName == RightToeName
				? RightToeRuntimeInfo
				: LeftToeRuntimeInfo;

			DebugData.PredictionOrigin = ToeInfo.CurWSPos;
			CalcToeEndPosByToeVelocity(OutToeEndPos, InPastPath, InToeName);
			DebugData.bUsedToeVelocity = true;
		}
		else if (bEnableDefaultDistancePredictive)
		{
			ValidPredictive = true;

			DebugData.PredictionOrigin = InPastPath.LeaveFloorPos;
			CalcToeEndPosByDefaultDistance(OutToeEndPos, InPastPath);
			DebugData.bUsedDefaultDistance = true;
		}
		else if (bEnableCurvePredictive && InCurToeCurveValue > 0.f)
		{
			ValidPredictive = true;
			DebugData.PredictionOrigin = CurCharacterBottomLocation;
			CalcToeEndPosByCurve(OutToeEndPos, InCurToeCurveValue);
			DebugData.bUsedCurve = true;
		}
	}

	if (ValidPredictive)
	{
		DebugData.RawPredictionEndPos = OutToeEndPos;
	}

	return ValidPredictive;
}


void UPredictionAnimInstance::Step2_TraceToePath(
	TArray<FVector>& OutToePath, 
	float& OutToeHeightLimit, 
	const FVector& InToeStartPos,
	const FVector& InToeCurPos,
	FVector InToeEndPos,
	const FName& InToeName,
	const float& DeltaSeconds)
{
	bool bEndPosChanged = false;
	FVector LastToeEndPos = OutToePath.Num() > 1 ? OutToePath[OutToePath.Num() - 1] : FVector::ZeroVector;
	FVector CurToeEndPos = InToeEndPos;
	CheckEndPosByTrace(bEndPosChanged, CurToeEndPos, LastToeEndPos);


	FToePredictionDebugData& DebugData = InToeName == RightToeName 
		? RightToeDebugData
		: LeftToeDebugData;

	DebugData.LastPathStartPos = InToeCurPos;

	const bool bStartPosChanged = OutToePath.IsEmpty() || 
		FVector::DistSquared2D(InToeCurPos, OutToePath[0]) > FMath::Square(PathStartChangedDistance);

	const bool bNeedRebuild = OutToePath.IsEmpty() || bEndPosChanged || bStartPosChanged;

	// will cause cur toe endpos not same with trace end pos.
	if (bNeedRebuild)
	{
		TArray<FVector> ToePath;
		bool ValidEndPos = true;
		LineTracePath2(ValidEndPos, ToePath, InToeCurPos, CurToeEndPos);

		UE_LOG(LogQuadrupedIK, Verbose, TEXT("%s End Pos Changed, Valid %d"), *InToeName.ToString(), ValidEndPos);

		if (ValidEndPos)
		{
			OutToePath = ToePath;

			//InOutLastPathStartPos = InToeCurPos;

			DebugData.bPathValid = true;

			if (!OutToePath.IsEmpty())
			{
				DebugData.FinalPathEndPos = OutToePath.Last();
			}
		}
		else
		{
			OutToePath.Empty();
			DebugData.bPathValid = false;
		}
	}

	// 再生成しなかったフレームも更新する
	DebugData.bPathValid = OutToePath.Num() > 1;

	if (DebugData.bPathValid)
	{
		DebugData.FinalPathEndPos = OutToePath.Last();
	}

}

void UPredictionAnimInstance::Step3_CalcMeshPosZ(
	float& OutTargetMeshPosZ,
	const float& InRightEndDist,
	const float& InLeftEndDist,
	bool InIsRightContacting,
	bool InIsLeftContacting,
	const FVector& InRightEndPos,
	const FVector& InLeftEndPos,
	const float& DeltaSeconds)
{
	const EPredictionMotionFoot LstMotionFoot = CurMotionFoot;

	bool bUseRightToe = true;

	if (InRightEndDist > 0.f && InLeftEndDist > 0.f)
	{
		if (InIsRightContacting != InIsLeftContacting)
		{
			bUseRightToe = InIsLeftContacting;
		}
		else
		{
			bUseRightToe = InRightEndDist < InLeftEndDist;
		}
	}
	else if (InRightEndDist < 0.f && InLeftEndDist < 0.f)
	{
		bUseRightToe = InRightEndDist > InLeftEndDist;
	}
	else
	{
		bUseRightToe = InRightEndDist > InLeftEndDist;
	}

	float TargetEndDist = INVALID_TOE_DISTANCE;

	if (bUseRightToe)
	{
		CurMotionFoot = EPredictionMotionFoot::Right;
		TargetEndDist = InRightEndDist;
	}
	else
	{
		CurMotionFoot = EPredictionMotionFoot::Left;
		TargetEndDist = InLeftEndDist;
	}

	if (LstMotionFoot != CurMotionFoot && TargetEndDist < InvalidToeEndDist)
	{
		CurMotionFoot = LstMotionFoot;
	}

	// 先に現在フレームの終点を更新
	if (CurMotionFoot != EPredictionMotionFoot::None)
	{
		FVector TargetFootEndPos = CurMotionFoot == EPredictionMotionFoot::Right
			? InRightEndPos
			: InLeftEndPos;

		TargetFootEndPos += TargetFootEndPosOffset;

		if (
			LstMotionFoot == CurMotionFoot 
			&& !MotionFootEndPos.IsNearlyZero() 
			&& FVector::DistSquared(MotionFootEndPos, TargetFootEndPos) > 1.f)
		{
			MotionFootEndPos = FMath::VInterpTo(MotionFootEndPos,
				TargetFootEndPos, DeltaSeconds, EndPosZInterpSpeed);
		}
		else
		{
			MotionFootEndPos = TargetFootEndPos;
		}
	}

	// 更新済み終点でPelvis計算
	CalcPelvisOffset2(
		OutTargetMeshPosZ,
		MotionFootStartPos_MapByRootPos,
		MotionFootEndPos,
		CurCharacterBottomLocation,
		DeltaSeconds,
		LstMotionFoot,
		CurMotionFoot);


}


void UPredictionAnimInstance::Step4_Completed()
{
	PredictionFootIKComponent->ClearCurveValues();
	PredictionFootIKComponent->ClearToeCSPos();
}

void UPredictionAnimInstance::CurveSampling()
{
	bool SwitchGait = false;
	PredictionFootIKComponent->GetCurveValues(CurLeftToeCurveValue, CurRightToeCurveValue, CurMoveSpeedCurveValue, SwitchGait);
}


void UPredictionAnimInstance::ToePosSampling(float DeltaSeconds)
{
	PredictionFootIKComponent->GetToeCSPos(RightToeCSPos, LeftToeCSPos, ValidPredictiveWeight);
	//RightToeCSPos.X = 0.f;
	//LeftToeCSPos.X = 0.f;

	const FTransform& ComponentToWorld = GetOwningComponent()->GetComponentToWorld();

	UpdateToeRuntimeInfo(RightToeRuntimeInfo, RightToeCSPos, ComponentToWorld, DeltaSeconds);
	UpdateToeRuntimeInfo(LeftToeRuntimeInfo, LeftToeCSPos, ComponentToWorld, DeltaSeconds);


	RightToeDebugData.RawToeWSPos = RightToeRuntimeInfo.CurWSPos;
	RightToeDebugData.ToeVelocityWS = RightToeRuntimeInfo.VelocityWS;
	RightToeDebugData.RelativeToeVelocityWS = RightToeRuntimeInfo.RelativeVelocityWS;

	LeftToeDebugData.RawToeWSPos = LeftToeRuntimeInfo.CurWSPos;
	LeftToeDebugData.ToeVelocityWS = LeftToeRuntimeInfo.VelocityWS;
	LeftToeDebugData.RelativeToeVelocityWS = LeftToeRuntimeInfo.RelativeVelocityWS;

	const FVector OwnerVelocity = GetOwnerVelocity();
	RightToeDebugData.OwnerVelocityWS = OwnerVelocity;
	LeftToeDebugData.OwnerVelocityWS = OwnerVelocity;
}


void UPredictionAnimInstance::UpdateToeRuntimeInfo(
	FToeRuntimeInfo& Info,
	const FVector& NewCSPos,
	const FTransform& ComponentToWorld,
	float DeltaSeconds)
{
	Info.CurCSPos = NewCSPos;
	Info.CurWSPos = ComponentToWorld.TransformPositionNoScale(NewCSPos);

	if (Info.bInitialized && DeltaSeconds > SMALL_NUMBER)
	{
		Info.VelocityCS = (Info.CurCSPos - Info.PrevCSPos) / DeltaSeconds;
		Info.VelocityWS = (Info.CurWSPos - Info.PrevWSPos) / DeltaSeconds;

		// キャラ移動ぶんを抜いた、足単体の相対速度
		Info.RelativeVelocityWS = Info.VelocityWS - GetOwnerVelocity();
	}
	else
	{
		Info.VelocityCS = FVector::ZeroVector;
		Info.VelocityWS = FVector::ZeroVector;
		Info.RelativeVelocityWS = FVector::ZeroVector;
		Info.bInitialized = true;
	}

	Info.PrevCSPos = Info.CurCSPos;
	Info.PrevWSPos = Info.CurWSPos;
}


bool UPredictionAnimInstance::IsToeVelocityPredictable(const FName& InToeName) const
{
	const FToeRuntimeInfo& ToeInfo = InToeName == RightToeName ? RightToeRuntimeInfo : LeftToeRuntimeInfo;
	return ToeInfo.bInitialized && ToeInfo.RelativeVelocityWS.Size2D() > MinToeVelocityForPrediction;
}

void UPredictionAnimInstance::CalcToeEndPosByToeVelocity(
	FVector& OutToeEndPos,
	const FPredictionToePathInfo& InPastPath,
	const FName& InToeName)
{
	const FToeRuntimeInfo& ToeInfo = InToeName == RightToeName ? RightToeRuntimeInfo : LeftToeRuntimeInfo;

	FVector ToeVelocity = ToeInfo.RelativeVelocityWS;
	ToeVelocity.Z = 0.f;

	FVector ToeDir = ToeVelocity.GetSafeNormal2D();
	FVector MoveDir = GetOwnerVelocity().GetSafeNormal2D();

	FVector PredictDir = ToeDir * ToeVelocityDirWeight + MoveDir * OwnerVelocityDirWeight;

	PredictDir = PredictDir.GetSafeNormal2D();

	if (PredictDir.IsNearlyZero())
	{
		PredictDir = MoveDir;
	}

	if (PredictDir.IsNearlyZero())
	{
		OutToeEndPos = InPastPath.LeaveFloorPos;
		return;
	}

	const float RawPredictDistance = ToeVelocity.Size2D() * ToeVelocityPredictionTime;

	const float PredictDistance = FMath::Clamp(
		RawPredictDistance,
		MinToePredictDistance,
		MaxToePredictDistance);

	OutToeEndPos = ToeInfo.CurWSPos + PredictDir * PredictDistance;
}


bool UPredictionAnimInstance::RefineToeEndPosByTerrain(
	FVector& InOutToeEndPos,
	const FVector& InPredictionDirection) const
{
	if (!GetWorld())
	{
		return false;
	}

	const FVector Forward = InPredictionDirection.GetSafeNormal2D();

	if (Forward.IsNearlyZero())
	{
		return false;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	const FVector Candidates[] =
	{
		InOutToeEndPos,
		InOutToeEndPos + Forward * LandingProbeForwardOffset,
		InOutToeEndPos - Forward * LandingProbeForwardOffset,
		InOutToeEndPos + Right * LandingProbeSideOffset,
		InOutToeEndPos - Right * LandingProbeSideOffset
	};

	bool bFoundValidCandidate = false;
	float BestScore = TNumericLimits<float>::Max();
	FVector BestPosition = InOutToeEndPos;

	for (const FVector& Candidate : Candidates)
	{
		const FVector TraceOffset =	FVector::UpVector * LandingProbeTraceHeight;

		FHitResult Hit;

		UKismetSystemLibrary::LineTraceSingle(
			GetWorld(),
			Candidate + TraceOffset,
			Candidate - TraceOffset,
			TraceChannel,
			false,
			IgnoreActors,
			EDrawDebugTrace::None,
			Hit,
			true);

		if (!Hit.IsValidBlockingHit())
		{
			continue;
		}

		if (!IsHitWalkableForPrediction(Hit))
		{
			continue;
		}

		const float HorizontalError = FVector::DistSquared2D(Candidate, InOutToeEndPos);
		const float HeightError = FMath::Square(Hit.Location.Z - CurCharacterBottomLocation.Z);

		const float Score = HorizontalError + HeightError * 0.25f;

		if (Score < BestScore)
		{
			BestScore = Score;
			BestPosition = Hit.Location;
			bFoundValidCandidate = true;
		}
	}

	if (bFoundValidCandidate)
	{
		InOutToeEndPos = BestPosition;
	}

	return bFoundValidCandidate;
}

void UPredictionAnimInstance::CalcToeEndPosByPastPath(FVector& OutToeEndPos, const FPredictionToePathInfo& InPastPath)
{

	OutToeEndPos = InPastPath.LeaveFloorPos + InPastPath.PathTranslation;
}


void UPredictionAnimInstance::CalcToeEndPosByCurve(FVector& OutToeEndPos, const float& InCurToeCurveValue)
{

	FVector MoveVelocity = GetOwnerVelocity();
	if (CurMoveSpeedCurveValue > SMALL_NUMBER)
	{
		MoveVelocity = MoveVelocity.GetSafeNormal() * CurMoveSpeedCurveValue;
	}
	OutToeEndPos = CurCharacterBottomLocation + MoveVelocity * InCurToeCurveValue;
}

void UPredictionAnimInstance::CalcToeEndPosByDefaultDistance(FVector& OutToeEndPos, const FPredictionToePathInfo& InPastPath)
{
	FVector MoveVelocity = GetOwnerVelocity();
	OutToeEndPos = InPastPath.LeaveFloorPos + MoveVelocity.GetSafeNormal() * DefaultToeFirstPathDistance;
}


void UPredictionAnimInstance::CheckEndPosByTrace(
	bool& OutEndPosChanged, 
	FVector& OutToeEndPos, 
	const FVector& InLastToeEndPos)
{
	FVector LocalToeTracePos = OutToeEndPos;
	FVector LocalToeHitPos = LocalToeTracePos;

	OutEndPosChanged = FVector::DistSquared2D(LocalToeTracePos, InLastToeEndPos) > EndPosChangedDistanceSquareThreshold;

	UObject* MovementBase = TryGetPawnOwner()->GetMovementBaseObject();
	FMovementBaseInterfaceData MovementBaseInterfaceData(MovementBase);
	if (!OutEndPosChanged && MovementBaseUtility::UseRelativeLocation(&MovementBaseInterfaceData))
	{
		FVector TraceHeight = { 0, 0, CapsuleComponent->GetScaledCapsuleHalfHeight() * 3.f };
		FHitResult Hit;

		// If FootEnd 2D Pos Not Changed, Use Last FootEndPos To Check Hight.
		UKismetSystemLibrary::LineTraceSingle(
			GetWorld(),
			LocalToeTracePos + TraceHeight,
			LocalToeTracePos - TraceHeight,
			TraceChannel,
			false,
			IgnoreActors,
			EDrawDebugTrace::None,
			Hit,
			true);

		if (!Hit.IsValidBlockingHit())
		{
			UKismetSystemLibrary::BoxTraceSingle(
				GetWorld(),
				LocalToeTracePos + TraceHeight,
				LocalToeTracePos - TraceHeight,
				FVector(ToeWidth, ToeWidth, 0.f),
				FRotator::ZeroRotator,
				TraceChannel,
				false,
				IgnoreActors,
				EDrawDebugTrace::None,
				Hit,
				true);
		}

		if (Hit.bBlockingHit)
		{
			LocalToeHitPos = { Hit.Location.X, Hit.Location.Y, Hit.Location.Z };
			OutEndPosChanged = FMath::Abs(LocalToeHitPos.Z - InLastToeEndPos.Z) > EndPosChangedHeightThreshold;
		}
		else
		{
			OutEndPosChanged = true;
		}

	}

	if (OutEndPosChanged)
	{
		OutToeEndPos = LocalToeHitPos;
	}
}


void UPredictionAnimInstance::LineTracePath2(
	bool& OutEndPosValid,
	TArray<FVector>& OutToePath,
	const FVector& InToeStartPos,
	const FVector& InToeEndPos)
{
	OutToePath.Reset();
	OutEndPosValid = false;

	if (!GetWorld() || !CapsuleComponent)
	{
		return;
	}

	const FVector Start(
		InToeStartPos.X,
		InToeStartPos.Y,
		CurCharacterBottomLocation.Z);

	const FVector End = InToeEndPos;

	const FVector Forward = (End - Start).GetSafeNormal2D();
	const float PathLength = FVector::Dist2D(Start, End);

	if (Forward.IsNearlyZero() || PathLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float SafeTraceInterval =
		FMath::Max(TraceIntervalLength, 1.f);

	const FVector TraceHeight(
		0.f,
		0.f,
		CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.f);

	FVector TracePos = Start;
	FVector ValidHitPos = Start;

	FHitResult OldHit;
	bool bHasOldValidHit = false;

	bool bTracePathEnded = false;
	bool bReachedRequestedEnd = false;

	int32 Index = 0;

	// 念のため無限ループ防止
	const int32 MaxTraceCount =
		FMath::CeilToInt(PathLength / SafeTraceInterval) + 2;

	while (!bTracePathEnded && Index < MaxTraceCount)
	{
		++Index;

		if (Index == 1)
		{
			TracePos = Start;
		}
		else
		{
			const float DistanceFromStart =
				Index * SafeTraceInterval;

			if (DistanceFromStart >= PathLength)
			{
				TracePos = FVector(
					End.X,
					End.Y,
					ValidHitPos.Z);

				bReachedRequestedEnd = true;
			}
			else
			{
				/*
				 * XYは一定間隔で前進させる。
				 * Zは直前の有効な地面高さを基準にする。
				 */
				TracePos =
					ValidHitPos +
					Forward * SafeTraceInterval;
			}
		}

		FHitResult Hit;

		UKismetSystemLibrary::LineTraceSingle(
			GetWorld(),
			TracePos + TraceHeight,
			TracePos - TraceHeight,
			TraceChannel,
			false,
			IgnoreActors,
			EDrawDebugTrace::None,
			Hit,
			true);

		if (!Hit.IsValidBlockingHit())
		{
			UKismetSystemLibrary::BoxTraceSingle(
				GetWorld(),
				TracePos + TraceHeight,
				TracePos - TraceHeight,
				FVector(ToeWidth, ToeWidth, 1.f),
				FRotator::ZeroRotator,
				TraceChannel,
				false,
				IgnoreActors,
				EDrawDebugTrace::None,
				Hit,
				true);
		}

		const bool bValidHit =
			Hit.IsValidBlockingHit();

		if (!bValidHit)
		{
			/*
			 * 穴・崖・Trace失敗。
			 * 今回の位置を有効な地面として採用しない。
			 * 最後の有効地点を終点として終了する。
			 */
			UE_LOG(
				LogQuadrupedIK,
				Verbose,
				TEXT("[%s] Invalid ground hit. Index:%d TracePos:%s"),
				*FString(__FUNCTION__),
				Index,
				*TracePos.ToString());

			bTracePathEnded = true;
			break;
		}

		const bool bCurrentWalkable =
			IsHitWalkableForPrediction(Hit);

		if (!bCurrentWalkable)
		{
			/*
			 * 壁や急斜面を地面として採用しない。
			 */
			UE_LOG(
				LogQuadrupedIK,
				Verbose,
				TEXT("[%s] Unwalkable hit. Index:%d Normal:%s"),
				*FString(__FUNCTION__),
				Index,
				*Hit.ImpactNormal.ToString());

			bTracePathEnded = true;
			break;
		}

		if (!bHasOldValidHit)
		{
			/*
			 * 最初のTrace。
			 * 比較対象となるOldHitがまだないため、
			 * キャラクター底面との高さ差だけを確認する。
			 */
			const float InitialGroundDeltaZ = Hit.Location.Z - CurCharacterBottomLocation.Z;

			const bool bInitialTooHigh = InitialGroundDeltaZ > PredictionMaxStepUp;

			const bool bInitialTooLow = InitialGroundDeltaZ < -PredictionMaxStepDown;

			if (bInitialTooHigh || bInitialTooLow)
			{
				UE_LOG(
					LogQuadrupedIK,
					Verbose,
					TEXT("[%s] Initial ground delta rejected. " "DeltaZ:%.2f UpLimit:%.2f DownLimit:%.2f"),
					*FString(__FUNCTION__),
					InitialGroundDeltaZ,
					PredictionMaxStepUp,
					PredictionMaxStepDown);

				bTracePathEnded = true;
				break;
			}

			ValidHitPos = Hit.Location;
			OutToePath.Add(ValidHitPos);

			OldHit = Hit;
			bHasOldValidHit = true;
		}
		else
		{
			/*
			 * 前回の地面と今回の地面の高さ差。
			 *
			 * 正数：上り
			 * 負数：下り
			 */
			const float GroundDeltaZ = Hit.Location.Z - OldHit.Location.Z;
			const bool bTooHighStep = GroundDeltaZ > PredictionMaxStepUp;
			const bool bTooDeepStep = GroundDeltaZ < -PredictionMaxStepDown;

			if (bTooHighStep)
			{
				UE_LOG(
					LogQuadrupedIK,
					Verbose,
					TEXT("[%s] Step up rejected. " "Index:%d DeltaZ:%.2f Limit:%.2f"),
					*FString(__FUNCTION__),
					Index,
					GroundDeltaZ,
					PredictionMaxStepUp);

				bTracePathEnded = true;
				break;
			}

			if (bTooDeepStep)
			{
				UE_LOG(
					LogQuadrupedIK,
					Verbose,
					TEXT("[%s] Step down rejected. " "Index:%d DeltaZ:%.2f Limit:%.2f"),
					*FString(__FUNCTION__),
					Index,
					GroundDeltaZ,
					PredictionMaxStepDown);

				bTracePathEnded = true;
				break;
			}

			/*
			 * 高さ差もWalkableも問題ないので、
			 * 今回のHitをパスとして採用。
			 */
			ValidHitPos = Hit.Location;
			OutToePath.Add(ValidHitPos);

			OldHit = Hit;
		}

		if (bReachedRequestedEnd)
		{
			bTracePathEnded = true;
		}
	}

	/*
	 * 2点以上あれば、少なくとも地形パスとして線分を作れる。
	 */
	OutEndPosValid = OutToePath.Num() >= 2 && bReachedRequestedEnd;
}


FVector UPredictionAnimInstance::GetToePredictivePos(
	const float& InMeshPosZ,
	const TArray<FVector>& InToePath,
	const FName& InToeName)
{

	const int32 Num = InToePath.Num();
	if (Num > 1)
	{
		FVector StartPos3D = InToePath[0];
		FVector StartPos2D = FVector(StartPos3D.X, StartPos3D.Y, 0.f);
		FVector EndPos3D = InToePath[Num - 1];
		FVector EndPos2D = FVector(EndPos3D.X, EndPos3D.Y, 0.f);

		FVector CurPos2D = FVector(CurCharacterBottomLocation.X, CurCharacterBottomLocation.Y, 0.f);

		const FVector Traslation2D = EndPos2D - StartPos2D;
		const float Traslation2DSize = Traslation2D.Size();
		const float ProjectLength = UKismetMathLibrary::Dot_VectorVector(Traslation2D, CurPos2D - StartPos2D) / Traslation2DSize;

		int32 ProjectPathIndex = INDEX_NONE;

		for (int32 i = 1; i < Num; ++i)
		{
			if (ProjectLength * ProjectLength <= (InToePath[i] - StartPos2D).SizeSquared2D())
			{
				ProjectPathIndex = i;
				break;
			}
		}

		int32 SlopePathIndex = INDEX_NONE;

		// recalculate end3d pos z by max slope of path
		if (ProjectPathIndex >= 1 && ProjectPathIndex < Num - 1)
		{
			FVector ProjectPathPos = StartPos3D;
			FVector ProjectPathDir = EndPos3D - ProjectPathPos;
			FVector ProjectPathNor = ProjectPathDir.GetSafeNormal();
			FVector MaxSlopePathPos = ProjectPathPos;
			FVector MaxSlopePathNor = ProjectPathNor;

			for (int32 i = ProjectPathIndex + 1; i < Num; ++i)
			{
				FVector NearestPathPos = InToePath[i] - FVector(0.f, 0.f, MaxSlopeToePathDownZ);
				FVector NearestPathPosNor = (NearestPathPos - ProjectPathPos).GetSafeNormal();

				if (NearestPathPosNor.Z > MaxSlopePathNor.Z)
				{
					SlopePathIndex = i;
					MaxSlopePathPos = NearestPathPos;
					MaxSlopePathNor = NearestPathPosNor;
				}
			}

			// slope angle > to end angle
			if (MaxSlopePathNor.Z > ProjectPathNor.Z)
			{
				ProjectPathDir.Z = MaxSlopePathNor.Z * (ProjectPathDir.X / MaxSlopePathNor.X);
				const FVector EndPos3DAdjustMaxSlope = ProjectPathPos + ProjectPathDir;
				const float SlopeAlpha = MaxSlopePathNor.Z - ProjectPathNor.Z;
				const float SlopeZ = FMath::Lerp(EndPos3DAdjustMaxSlope.Z, EndPos3D.Z, FMath::Clamp(MaxSlopeToePathAlpha - SlopeAlpha, 0.f, 1.f));
				EndPos3D = FVector(EndPos3DAdjustMaxSlope.X, EndPos3DAdjustMaxSlope.Y, SlopeZ);
			}

			UE_LOG(LogQuadrupedIK, Verbose, TEXT("CurIndex: %d SlopeIndex: %d MaxSlopeNorZ: %f ProjNorZ: %f |%s"),
				ProjectPathIndex, 
				SlopePathIndex, 
				MaxSlopePathNor.Z,
				ProjectPathNor.Z,
				*InToeName.ToString());
		}

		return EndPos3D;
	}

	return FVector::ZeroVector;
}


void UPredictionAnimInstance::GetToeHeightLimitByPathCurve(
	float& OutHeightLimit, 
	const FVector& InToeCurPos,
	const TArray<FVector>& InToePath)
{
	int32 Num = InToePath.Num();
	if (Num > 1)
	{
		FVector StartPos2D = FVector(InToePath[0].X, InToePath[0].Y, 0.f);
		FVector EndPos2D = FVector(InToePath[Num - 1].X, InToePath[Num - 1].Y, 0.f);
		FVector CurPos2D = FVector(InToeCurPos.X, InToeCurPos.Y, 0.f);

		FVector Traslation2D = EndPos2D - StartPos2D;
		float Traslation2DSize = Traslation2D.Size();
		float ProjectLength = UKismetMathLibrary::Dot_VectorVector(Traslation2D, CurPos2D - StartPos2D) / Traslation2DSize;

		for (int32 i = 1; i < Num; ++i)
		{
			if (ProjectLength * ProjectLength <= (InToePath[i] - StartPos2D).SizeSquared2D())
			{
				float BeforeSize = (InToePath[i - 1] - StartPos2D).Size2D();
				float AfterSize = (InToePath[i] - StartPos2D).Size2D();
				UE_LOG(LogQuadrupedIK, VeryVerbose, TEXT("ToeHeight Index i: %d"), i);
				OutHeightLimit = UKismetMathLibrary::MapRangeClamped(
					ProjectLength - BeforeSize, 
					0.f, 
					AfterSize - BeforeSize, 
					InToePath[i - 1].Z, InToePath[i].Z);
				return;
			}
		}

		OutHeightLimit = InToePath[Num - 1].Z;
	}
}


void UPredictionAnimInstance::CalcPelvisOffset2(
	float& OutTargetMeshPosZ,
	FVector& OutFootStartPos,
	const FVector& InFootEndPos,
	const FVector& InMappedPos,
	const float& DeltaSeconds,
	EPredictionMotionFoot InLstMotionFoot,
	EPredictionMotionFoot InCurMotionFoot)
{
	if (InCurMotionFoot != InLstMotionFoot)
	{
		OutFootStartPos = CurCharacterBottomLocation;
		OutFootStartPos.Z = OutTargetMeshPosZ;
	}

	if (InLstMotionFoot == EPredictionMotionFoot::None && InCurMotionFoot != EPredictionMotionFoot::None)
	{
		OutTargetMeshPosZ = CurCharacterBottomLocation.Z;
		OutFootStartPos = CurCharacterBottomLocation;
		return;
	}

	if (InLstMotionFoot == EPredictionMotionFoot::None)
	{
		return;
	}

	const FVector Start2D(
		OutFootStartPos.X,
		OutFootStartPos.Y,
		0.f);

	const FVector End2D(
		InFootEndPos.X,
		InFootEndPos.Y,
		0.f);

	const FVector Current2D(
		InMappedPos.X,
		InMappedPos.Y,
		0.f);

	const FVector Path2D = End2D - Start2D;
	const float PathLengthSq = Path2D.SizeSquared2D();

	if (PathLengthSq <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float RawAlpha = FVector::DotProduct(Current2D - Start2D, Path2D) / PathLengthSq;

	const float ClampedAlpha = FMath::Clamp( RawAlpha, 0.f, 1.f);
	const float TargetMeshWorldZ = FMath::Lerp(OutFootStartPos.Z, InFootEndPos.Z, ClampedAlpha);

	// World Z側では補間しない
	OutTargetMeshPosZ = TargetMeshWorldZ;

	if (bDrawDebugForPelvis)
	{
		DrawDebugSphere(
			GetWorld(),
			OutFootStartPos,
			8.f,
			12,
			FColor::Blue,
			false,
			0.f);

		DrawDebugSphere(
			GetWorld(),
			InFootEndPos,
			8.f,
			12,
			FColor::Red,
			false,
			0.f);

		DrawDebugSphere(
			GetWorld(),
			InMappedPos,
			8.f,
			12,
			FColor::Green,
			false,
			0.f);

		DrawDebugLine(
			GetWorld(),
			OutFootStartPos,
			InFootEndPos,
			FColor::Cyan,
			false,
			0.f,
			0,
			2.f);

		//DrawDebugString(
		//	GetWorld(),
		//	InMappedPos + FVector(0.f, 0.f, 40.f),
		//	FString::Printf(
		//		TEXT(
		//			"RawAlpha: %.2f\n"
		//			"ClampedAlpha: %.2f\n"
		//			"StartZ: %.2f\n"
		//			"EndZ: %.2f\n"
		//			"TargetZ: %.2f"),
		//		RawAlpha,
		//		ClampedAlpha,
		//		OutFootStartPos.Z,
		//		InFootEndPos.Z,
		//		TargetMeshWorldZ),
		//	nullptr,
		//	FColor::White,
		//	0.f,
		//	true);
	}
}

void UPredictionAnimInstance::TraceForTwoFoots(
	float DeltaSeconds,
	float& OutMinHitZ,
	float& OutRightFootHeight,
	float& OutLeftFootHeight,
	FVector& OutRightHitNor,
	FVector& OutLeftHitNor)
{
	FVector RightFootPos = GetOwningComponent()->GetBoneLocation(RightFootName, EBoneSpaces::WorldSpace);
	FVector LeftFootPos = GetOwningComponent()->GetBoneLocation(LeftFootName, EBoneSpaces::WorldSpace);

	RightFootPos.Z = CurCharacterBottomLocation.Z;
	LeftFootPos.Z = CurCharacterBottomLocation.Z;

	bool ValidRightHit = false;
	bool ValidbLeftHit = false;

	float RightFootHitZ = 0.f;
	float LeftFootHitZ = 0.f;

	FVector TraceUp = FVector(0.f, 0.f, ReactFootIKUpTraceHeight);
	FVector TraceDown = FVector(0.f, 0.f, ReactFootIKDownTraceHeight);

	const EDrawDebugTrace::Type TraceType = EDrawDebugTrace::None;

	FHitResult RightHit;
	UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		RightFootPos + TraceUp,
		RightFootPos - TraceDown,
		TraceChannel,
		false,
		IgnoreActors,
		TraceType,
		RightHit,
		true);

	if (!RightHit.IsValidBlockingHit())
	{
		UKismetSystemLibrary::BoxTraceSingle(
			GetWorld(),
			RightFootPos + TraceUp,
			RightFootPos - TraceDown,
			FVector(ToeWidth, ToeWidth, 0.f),
			FRotator::ZeroRotator,
			TraceChannel,
			false,
			IgnoreActors,
			TraceType,
			RightHit,
			true);
	}

	ValidRightHit = RightHit.IsValidBlockingHit();
	RightFootHitZ = ValidRightHit ? RightHit.Location.Z : CurCharacterBottomLocation.Z;

	FHitResult LeftHit;
	UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		LeftFootPos + TraceUp,
		LeftFootPos - TraceDown,
		TraceChannel,
		false,
		IgnoreActors,
		TraceType,
		LeftHit,
		true);

	if (!LeftHit.IsValidBlockingHit())
	{
		UKismetSystemLibrary::BoxTraceSingle(
			GetWorld(),
			LeftFootPos + TraceUp,
			LeftFootPos - TraceDown,
			FVector(ToeWidth, ToeWidth, 0.f),
			FRotator::ZeroRotator,
			TraceChannel,
			false,
			IgnoreActors,
			TraceType,
			LeftHit,
			true);
	}

	ValidbLeftHit = LeftHit.IsValidBlockingHit();

	LeftFootHitZ = ValidbLeftHit ? LeftHit.Location.Z : CurCharacterBottomLocation.Z;

	float TargetRightFootHeightOffset = 0.f;
	float TargetLeftFootHeightOffset = 0.f;
	float MinHitZ = FMath::Min(RightFootHitZ, LeftFootHitZ);
	if (ValidRightHit && ValidbLeftHit && (MinHitZ - CurCharacterBottomLocation.Z) > -ReactFootIKHeightThreshold) // magic num
	{
		TargetRightFootHeightOffset = RightHit.Location.Z;
		TargetLeftFootHeightOffset = LeftHit.Location.Z;
		OutRightHitNor = RightHit.ImpactNormal;
		OutLeftHitNor = LeftHit.ImpactNormal;
		OutMinHitZ = MinHitZ;
	}
	else
	{
		TargetRightFootHeightOffset = CurCharacterBottomLocation.Z;
		TargetLeftFootHeightOffset = CurCharacterBottomLocation.Z;
		OutRightHitNor = FVector::UpVector;
		OutLeftHitNor = FVector::UpVector;
		OutMinHitZ = CurCharacterBottomLocation.Z;
	}

	OutRightFootHeight = FMath::FInterpTo(OutRightFootHeight, TargetRightFootHeightOffset, DeltaSeconds, FootIKHeightOffsetInterpSpeed);
	OutLeftFootHeight = FMath::FInterpTo(OutLeftFootHeight, TargetLeftFootHeightOffset, DeltaSeconds, FootIKHeightOffsetInterpSpeed);

	OutRightHitNor = GetOwningComponent()->GetComponentToWorld().InverseTransformVector(OutRightHitNor);
	OutLeftHitNor = GetOwningComponent()->GetComponentToWorld().InverseTransformVector(OutLeftHitNor);
}


float UPredictionAnimInstance::GetPelvisFinalOffset() const
{
	return PelvisFinalOffset;
}


/// <summary>
/// 青点：現在の生Toe World位置
/// マゼンタ点：LeaveFloorPos
/// 白点：予測起点
/// 白線：現在Toe → 予測起点
/// シアン点：Trace前の予測終点
/// シアン線：予測起点 → 予測終点
/// 黄線：ToeのWorld速度
/// 緑線：Owner速度を引いたToe相対速度
/// 赤線：Owner移動速度
/// オレンジ点：Path生成に渡した開始位置
/// オレンジ線：生Toe → Path開始位置
/// 黄色Path：右足の地面Path
/// 緑色Path：左足の地面Path
/// 赤点：Trace後の最終Path終点
/// 白Box：Character Bottom
/// </summary>
/// <param name="DebugData"></param>
/// <param name="ToePath"></param>
/// <param name="PathColor"></param>
/// <param name="Prefix"></param>
void UPredictionAnimInstance::DebugDrawToePredictionDetailed(
	const FToePredictionDebugData& DebugData,
	const TArray<FVector>& ToePath,
	const FLinearColor& PathColor,
	const FString& Prefix) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 生Toe位置
	UKismetSystemLibrary::DrawDebugPoint(
		World,
		DebugData.RawToeWSPos,
		14.f,
		FLinearColor::Blue);

	// LeaveFloorPos
	UKismetSystemLibrary::DrawDebugPoint(
		World,
		DebugData.LeaveFloorPos,
		12.f,
		FLinearColor(FColor::Magenta));

	// Prediction Origin
	UKismetSystemLibrary::DrawDebugPoint(
		World,
		DebugData.PredictionOrigin,
		10.f,
		FLinearColor::White);

	// Trace前の予測終点
	UKismetSystemLibrary::DrawDebugPoint(
		World,
		DebugData.RawPredictionEndPos,
		14.f,
		FLinearColor(FColor::Cyan));

	// Path最終位置
	if (DebugData.bPathValid)
	{
		UKismetSystemLibrary::DrawDebugPoint(
			World,
			DebugData.FinalPathEndPos,
			16.f,
			FLinearColor::Red);
	}

	// 現在Toe → Prediction Origin
	UKismetSystemLibrary::DrawDebugLine(
		World,
		DebugData.RawToeWSPos,
		DebugData.PredictionOrigin,
		FLinearColor::White,
		0.f,
		1.f);

	// Prediction Origin → Raw Prediction End
	UKismetSystemLibrary::DrawDebugLine(
		World,
		DebugData.PredictionOrigin,
		DebugData.RawPredictionEndPos,
		FLinearColor(FColor::Cyan), 0.f, 3.f);

	// Toeの生速度
	const FVector ToeVelocityEnd = DebugData.RawToeWSPos + DebugData.ToeVelocityWS * 0.1f;

	UKismetSystemLibrary::DrawDebugLine(
		World,
		DebugData.RawToeWSPos,
		ToeVelocityEnd,
		FLinearColor::Yellow,
		0.f, 2.f);

	// Toe相対速度
	const FVector RelativeVelocityEnd = DebugData.RawToeWSPos + DebugData.RelativeToeVelocityWS * 0.1f;

	UKismetSystemLibrary::DrawDebugLine(
		World,
		DebugData.RawToeWSPos,
		RelativeVelocityEnd,
		FLinearColor::Green,
		0.f,
		3.f);

	// Owner速度
	const FVector OwnerVelocityEnd = CurCharacterBottomLocation + DebugData.OwnerVelocityWS * 0.1f;

	UKismetSystemLibrary::DrawDebugLine(
		World,
		CurCharacterBottomLocation,
		OwnerVelocityEnd,
		FLinearColor::Red,
		0.f,
		2.f);

	// ToePathを各区間で描画
	for (int32 Index = 0; Index < ToePath.Num(); ++Index)
	{
		UKismetSystemLibrary::DrawDebugPoint(
			World,
			ToePath[Index],
			6.f,
			PathColor);

		if (Index > 0)
		{
			UKismetSystemLibrary::DrawDebugLine(
				World,
				ToePath[Index - 1],
				ToePath[Index],
				PathColor,
				0.f,
				2.f);
		}
	}

	// Character Bottom
	UKismetSystemLibrary::DrawDebugBox(
		World,
		CurCharacterBottomLocation,
		FVector(5.f),
		FLinearColor::White,
		FRotator::ZeroRotator,
		0.f,
		1.f);

	UKismetSystemLibrary::DrawDebugPoint(
		World,
		DebugData.LastPathStartPos,
		11.f,
		FLinearColor(FColor::Orange));

	UKismetSystemLibrary::DrawDebugLine(
		World,
		DebugData.RawToeWSPos,
		DebugData.LastPathStartPos,
		FLinearColor(FColor::Orange),
		0.f,
		2.f);

	FString PredictionMode = TEXT("None");

	if (DebugData.bUsedContact)
	{
		PredictionMode = TEXT("Contact");
	}
	else if (DebugData.bUsedPastPath)
	{
		PredictionMode = TEXT("PastPath");
	}
	else if (DebugData.bUsedToeVelocity)
	{
		PredictionMode = TEXT("ToeVelocity");
	}
	else if (DebugData.bUsedDefaultDistance)
	{
		PredictionMode = TEXT("DefaultDistance");
	}

	const FString DebugText = FString::Printf(
		TEXT(
			"%s Mode:%s\n"
			"Toe:%s\n"
			"Origin:%s\n"
			"Pred:%s\n"
			"ToeVel:%.1f RelVel:%.1f OwnerVel:%.1f\n"
			"PathNum:%d Valid:%d"),
		*Prefix,
		*PredictionMode,
		*DebugData.RawToeWSPos.ToCompactString(),
		*DebugData.PredictionOrigin.ToCompactString(),
		*DebugData.RawPredictionEndPos.ToCompactString(),
		DebugData.ToeVelocityWS.Size2D(),
		DebugData.RelativeToeVelocityWS.Size2D(),
		DebugData.OwnerVelocityWS.Size2D(),
		ToePath.Num(),
		DebugData.bPathValid ? 1 : 0);

	DrawDebugString(
		World,
		DebugData.RawToeWSPos +
		FVector(0.f, 0.f, 25.f),
		DebugText,
		nullptr,
		FColor::White,
		0.f,
		true);
}


bool UPredictionAnimInstance::ShouldRunPredictive() const
{
	if (!ValidPredictiveWeight)
	{
		return false;
	}

	// Anim側の移動速度カーブがあるなら、これを最優先にする
	if (CurMoveSpeedCurveValue > SMALL_NUMBER)
	{
		return true;
	}

	const FVector Velocity = GetOwnerVelocity();
	return !Velocity.IsNearlyZero();
}

FVector UPredictionAnimInstance::GetOwnerVelocity() const
{
	if (CharacterMoverComponent)
	{
		return CharacterMoverComponent->GetVelocity();
	}

	if (CharacterMovementComponent)
	{
		return CharacterMovementComponent->Velocity;
	}

	if (const APawn* OwnerPawn = TryGetPawnOwner())
	{
		return OwnerPawn->GetVelocity();
	}

	return FVector::ZeroVector;
}

bool UPredictionAnimInstance::IsHitWalkableForPrediction(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	if (CharacterMovementComponent)
	{
		return CharacterMovementComponent->IsWalkable(Hit);
	}

	if (CharacterMoverComponent)
	{
		const FVector UpDirection = CharacterMoverComponent->GetUpDirection();

		return UFloorQueryUtils::IsHitSurfaceWalkable(
			Hit,
			UpDirection,
			CharacterWalkableFloorZ);
	}

	return FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) >= CharacterWalkableFloorZ;
}


#pragma region FBIK
void UPredictionAnimInstance::InitializeBoneOffset(const int32 BoneIndex)
{
	if (!OffsetLocations.Contains(BoneIndex))
	{
		OffsetLocations.Add(BoneIndex, FVector::ZeroVector);
	}
	if (!OffsetRotations.Contains(BoneIndex))
	{
		OffsetRotations.Add(BoneIndex, FRotator::ZeroRotator);
	}
}

void UPredictionAnimInstance::SetBoneLocationOffset(const int32 BoneIndex, const FVector& Location)
{
	OffsetLocations[BoneIndex] = Location;
}

FVector UPredictionAnimInstance::GetBoneLocationOffset(const int32 BoneIndex) const
{
	return OffsetLocations[BoneIndex];
}

void UPredictionAnimInstance::SetBoneRotationOffset(const int32 BoneIndex, const FRotator& Rotation)
{
	OffsetRotations[BoneIndex] = Rotation;
}

FRotator UPredictionAnimInstance::GetBoneRotationOffset(const int32 BoneIndex) const
{
	return OffsetRotations[BoneIndex];
}
#pragma endregion

