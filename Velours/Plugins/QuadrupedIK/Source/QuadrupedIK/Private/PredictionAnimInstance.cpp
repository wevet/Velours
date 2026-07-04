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
	bEnableDefaultDistancePredictive = true;

	RootMotionMode = ERootMotionMode::RootMotionFromEverything;


	bDrawDebug = false;
	bDrawDebugForToe = false;
	bDrawDebugForPelvis = false;
	bDrawDebugForReactFootIK = false;

	bEnableCurvePredictive = false;
	bEnablePastPathPredictive = false;
	bEnableDefaultDistancePredictive = false;

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

		bool AbnormalMove = UKismetMathLibrary::Dot_VectorVector(
			(CurCharacterBottomLocation - LstCharacterBottomLocation).GetSafeNormal2D(),
			TryGetPawnOwner()->GetActorForwardVector()) < AbnormalMoveCosAngle;

		AbnormalMove ? AbnormalMoveTime += DeltaSeconds : AbnormalMoveTime = 0.f;

		const bool FinalAbnormalMove = AbnormalMoveTime >= AbnormalMoveTimeLimit;

		float Dist2DSquared = UKismetMathLibrary::Vector_Distance2DSquared(CurCharacterBottomLocation, LstCharacterBottomLocation);
		const bool JustTeleported = Dist2DSquared > TeleportedDistanceThreshold * TeleportedDistanceThreshold;

		WeightOfDisableFootIK = JustTeleported ? 1.f : WeightOfDisableFootIK;
		const bool ValidDisableFootIKTick = WeightOfDisableFootIK > 0.f;

		if (ValidDisableFootIKTick)
		{
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("-------------DisableFootIK---------CurWeight:%f"), WeightOfDisableFootIK);
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
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("-------------ReactFootIK---------CurMeshWorldPosZ:%.2f"), CurMeshWorldPosZ);
		}

		PelvisFinalOffset = CurMeshWorldPosZ - CurCharacterBottomLocation.Z;
		PelvisFinalOffset = FMath::Clamp(PelvisFinalOffset, -1.f * PelvisHeightDownThreshold, PelvisHeightUpThreshold);
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
		float Dist = !IsTotalPathStart ? DefaultToeFirstPathDistance : DefaultToeFirstPathDistance * 2.f;

		// tick contact state and path
		RightToePathInfo.Update(GetOwningComponent(), RightToeCSPos, LeftToeCSPos, EPredictionMotionFoot::Right, RightToeName);
		LeftToePathInfo.Update(GetOwningComponent(), RightToeCSPos, LeftToeCSPos, EPredictionMotionFoot::Left, LeftToeName);

		if (RightToePathInfo.IsLeaveStart())
		{
			RightToePathInfo.SetDefaultPathDistance(Dist);
		}

		if (LeftToePathInfo.IsLeaveStart())
		{
			LeftToePathInfo.SetDefaultPathDistance(Dist);
		}


		// r toe contact pos predictive, and compare with last pos
		FVector RightToeEndPos;
		const bool IsValidForRightPredictive = Step1_PredictiveToeEndPos(
			RightToeEndPos,
			RightToePathInfo,
			CurRightToeCurveValue,
			RightToeName);

		// l toe contact pos predictive, and compare with last pos
		FVector LeftToeEndPos;
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

			FVector RightToePredictivePos = FVector::ZeroVector;
			float RightToeEndDistance = INVALID_TOE_DISTANCE;
			if (RightToePath.Num() > 1)
			{
				RightToePredictivePos = GetToePredictivePos(OutTargetMeshPosZ, RightToePath, RightToeName);
				RightToeEndDistance = ComponentToWorld.InverseTransformPositionNoScale(RightToePredictivePos).Y;
			}

			FVector LeftToePredictivePos = FVector::ZeroVector;
			float LeftToeEndDistance = INVALID_TOE_DISTANCE;
			if (LeftToePath.Num() > 1)
			{
				LeftToePredictivePos = GetToePredictivePos(OutTargetMeshPosZ, LeftToePath, LeftToeName);
				LeftToeEndDistance = ComponentToWorld.InverseTransformPositionNoScale(LeftToePredictivePos).Y;
			}

			if (bDrawDebugForToe)
			{
				if (RightToePath.Num() > 1)
				{
					DebugDrawToePath(RightToePath, RightToePathInfo.CurToePos, RightToePredictivePos, FLinearColor::Yellow);
				}

				if (LeftToePath.Num() > 1)
				{
					DebugDrawToePath(LeftToePath, LeftToePathInfo.CurToePos, LeftToePredictivePos, FLinearColor::Green);
				}
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

			if (bDrawDebugForPelvis)
			{
				DebugDrawPelvisPath();
			}
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

	if (InPastPath.IsPathStarted)
	{
		if (InPastPath.IsContacting())
		{
			ValidPredictive = true;
			OutToeEndPos = InPastPath.CurToePos;
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("Predictive by contact |%s"), *InToeName.ToString());
		}
		else if (bEnableCurvePredictive && InCurToeCurveValue > 0.f)
		{
			ValidPredictive = true;
			CalcToeEndPosByCurve(OutToeEndPos, InCurToeCurveValue);
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("Predictive by curve |%s"), *InToeName.ToString());
		}
		else if (bEnablePastPathPredictive && InPastPath.IsPathValid)
		{
			ValidPredictive = true;
			CalcToeEndPosByPastPath(OutToeEndPos, InPastPath);
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("Predictive by past path |%s"), *InToeName.ToString());
		}
		else if (bEnableDefaultDistancePredictive)
		{
			ValidPredictive = true;
			CalcToeEndPosByDefaultDistance(OutToeEndPos, InPastPath);
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("Predictive by default distance |%s"), *InToeName.ToString());
		}
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
	bool EndPosChanged = false;
	FVector LastToeEndPos = OutToePath.Num() > 1 ? OutToePath[OutToePath.Num() - 1] : FVector::ZeroVector;
	FVector CurToeEndPos = InToeEndPos;
	CheckEndPosByTrace(EndPosChanged, CurToeEndPos, LastToeEndPos);

	// will cause cur toe endpos not same with trace end pos.
	if (EndPosChanged || OutToePath.IsEmpty())
	{
		TArray<FVector> ToePath;
		bool ValidEndPos = true;
		LineTracePath2(ValidEndPos, ToePath, InToeCurPos, CurToeEndPos);

		UE_LOG(LogQuadrupedIK, Verbose, TEXT("%s End Pos Changed, Valid %d"), *InToeName.ToString(), ValidEndPos);

		if (ValidEndPos)
		{
			OutToePath = ToePath;
		}
		else
		{
			OutToePath.Empty();
		}
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
	// switch motion foot
	EPredictionMotionFoot LstMotionFoot = CurMotionFoot;
	FName MotionToeName;

	// maybe double dist all < 0.f
	bool UseRightToe = true;
	if (InRightEndDist > 0.f && InLeftEndDist > 0.f)
	{
		if (InIsRightContacting != InIsLeftContacting)
		{
			UseRightToe = InIsLeftContacting;
		}
		else
		{
			UseRightToe = InRightEndDist < InLeftEndDist;
		}
	}
	else if (InRightEndDist < 0.f && InLeftEndDist < 0.f)
	{
		UseRightToe = InRightEndDist > InLeftEndDist;
	}
	else
	{
		UseRightToe = InRightEndDist > InLeftEndDist;
	}

	float TargetEndDist = INVALID_TOE_DISTANCE;
	if (UseRightToe)
	{
		CurMotionFoot = EPredictionMotionFoot::Right;
		TargetEndDist = InRightEndDist;
	}
	else
	{
		CurMotionFoot = EPredictionMotionFoot::Left;
		TargetEndDist = InLeftEndDist;
	}

	// ignore invalid switch
	if (LstMotionFoot != CurMotionFoot && TargetEndDist < InvalidToeEndDist)
	{
		CurMotionFoot = LstMotionFoot;
	}

	FName LstMotionToeName = LstMotionFoot == EPredictionMotionFoot::None ? FName(TEXT("None")) : LstMotionFoot == EPredictionMotionFoot::Right ? RightToeName : LeftToeName;

	UE_LOG(LogQuadrupedIK, Verbose, TEXT("<<<<<<<<< LstMotionToeName: %s CurRightEndDist: %f CurLeftEndDist: %f"), *LstMotionToeName.ToString(), InRightEndDist, InLeftEndDist);

	/*
	FVector CurToePos = CurMotionFoot == EPredictionMotionFoot::Right ? InRightToePos : InLeftToePos;
	CalcPelvisOffset2(PelvisFinalOffset_MapByToePos,
		MotionFootStartPos_MapByToePos, MotionFootEndPos, CurToePos,
		DeltaSeconds, LstMotionFoot, CurMotionFoot);

	UE_LOG(LogQuadrupedIK, Verbose, TEXT("AAAAAAAAAAA: MapByToe"));
	*/

	CalcPelvisOffset2(
		OutTargetMeshPosZ,
		MotionFootStartPos_MapByRootPos,
		MotionFootEndPos,
		CurCharacterBottomLocation,
		DeltaSeconds,
		LstMotionFoot,
		CurMotionFoot);


	if (CurMotionFoot != EPredictionMotionFoot::None)
	{
		FVector LstFootEndPos = MotionFootEndPos;
		FVector TarFootEndPos = CurMotionFoot == EPredictionMotionFoot::Right ? InRightEndPos : InLeftEndPos;
		TarFootEndPos += FVector(0.f, 0.f, 1.5f); // magic num

		FVector CurFootEndPos = TarFootEndPos;

		if (LstMotionFoot == CurMotionFoot && FVector::DistSquared(LstFootEndPos, TarFootEndPos) > 1.f)
		{
			CurFootEndPos = FMath::VInterpTo(LstFootEndPos, TarFootEndPos, DeltaSeconds, EndPosZInterpSpeed);
		}

		MotionFootEndPos = CurFootEndPos;
	}
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
	RightToeCSPos.X = 0.f;
	LeftToeCSPos.X = 0.f;

	const FTransform& ComponentToWorld = GetOwningComponent()->GetComponentToWorld();

	//UpdateToeRuntimeInfo(RightToeRuntimeInfo, RightToeCSPos, ComponentToWorld, DeltaSeconds);
	//UpdateToeRuntimeInfo(LeftToeRuntimeInfo, LeftToeCSPos, ComponentToWorld, DeltaSeconds);
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
	const FVector MoveVelocity = GetOwnerVelocity();
	OutToeEndPos = InPastPath.LeaveFloorPos + MoveVelocity.GetSafeNormal() * DefaultToeFirstPathDistance;
}

void UPredictionAnimInstance::CheckEndPosByTrace(bool& OutEndPosChanged, FVector& OutToeEndPos, const FVector& InLastToeEndPos)
{
	FVector LocalToeTracePos = OutToeEndPos;
	FVector LocalToeHitPos = LocalToeTracePos;

	OutEndPosChanged = FVector::DistSquared2D(LocalToeTracePos, InLastToeEndPos) > EndPosChangedDistanceSquareThreshold;


	if (!OutEndPosChanged)
	{
		FVector TraceHeight = { 0, 0, CapsuleComponent->GetScaledCapsuleHalfHeight() * 3.f };
		//TArray<AActor*> IgnoreActors;
		FHitResult Hit;

		// If FootEnd 2D Pos Not Changed, Use Last FootEndPos To Check Hight.
		UKismetSystemLibrary::LineTraceSingle(GetWorld(),
			LocalToeTracePos + TraceHeight,
			LocalToeTracePos - TraceHeight,
			TraceChannel,
			false,
			IgnoreActors,
			EDrawDebugTrace::None,
			Hit,
			true);

		const bool bUseRelative = ShouldUseRelativeLocationToMovementBase() ||
			ShouldUseRelativeLocationToHitBase(Hit);

		if (bUseRelative)
		{
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
	OutToePath.Empty();

	FVector Start = FVector(InToeStartPos.X, InToeStartPos.Y, CurCharacterBottomLocation.Z);
	FVector End = InToeEndPos;

	FVector Forward = (End - Start).GetSafeNormal2D();
	float PathLength = (End - Start).Size2D();

	FVector TracePos = Start;
	FVector ValidHitPos = TracePos;

	FVector TraceHeight = { 0, 0, CapsuleComponent->GetScaledCapsuleHalfHeight() * 2 };

	FHitResult OldHit;
	//TArray<AActor*> IgnoreActors({ Character, });

	int32 Index = 0;
	bool TracePathEnded = false;
	while (!TracePathEnded)
	{
		Index++;
		if (Index == 1)
		{
			TracePos = Start;
		}
		else if (Index * TraceIntervalLength > PathLength)
		{
			TracePos = { End.X, End.Y, ValidHitPos.Z };
			TracePathEnded = true;
		}
		else
		{
			TracePos = ValidHitPos + (Forward * TraceIntervalLength);
		}

		FHitResult Hit;

		// Start from last pos, end to cur pos, build a slope line for trace.
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

		bool ValidHit = Hit.IsValidBlockingHit();

		float HitOffsetZ = FMath::Abs(TracePos.Z - Hit.Location.Z);

		// first trace
		if (Index == 1)
		{
			if (!ValidHit)
			{
				ValidHitPos = TracePos;
			}
			else if (HitOffsetZ > CharacterMaxStepHeight * FMath::Max(1.f, TraceIntervalLength / 10.f))
			{
				ValidHitPos = TracePos;
			}
			else
			{
				ValidHitPos = Hit.Location;
			}
		}
		else
		{
			if (!ValidHit)
			{
				//FVector ValidHeight = { 0, 0, CharacterMaxStepHeight * 2 };
				ValidHitPos = TracePos; //Hit.bBlockingHit ? TracePos : ValidHitPos -= ValidHeight;
				ValidHitPos = FVector(End.X, End.Y, ValidHitPos.Z);
				TracePathEnded = true;
				UE_LOG(LogQuadrupedIK, Verbose, TEXT("trace path error:invalit hit"));
			}
			else if (!IsHitWalkableForPrediction(OldHit) &&
				!IsHitWalkableForPrediction(Hit) &&
				HitOffsetZ > CharacterMaxStepHeight)
			{
				ValidHitPos = TracePos;
				ValidHitPos = FVector(End.X, End.Y, ValidHitPos.Z);
				TracePathEnded = true;
				UE_LOG(LogQuadrupedIK, Verbose, TEXT("trace path error:un walkable"));
			}
			else if (HitOffsetZ > CharacterMaxStepHeight * FMath::Max(1.f, TraceIntervalLength / 10.f))
			{
				ValidHitPos = TracePos;
				ValidHitPos = FVector(End.X, End.Y, ValidHitPos.Z);
				TracePathEnded = true;
				UE_LOG(LogQuadrupedIK, Verbose, TEXT("trace path error:too height"));
			}
			else
			{
				ValidHitPos = Hit.Location;
			}
		}
		OutToePath.Add(ValidHitPos);
		OldHit = Hit;
	}
	OutEndPosValid = true;
}



FVector UPredictionAnimInstance::GetToePredictivePos(const float& InMeshPosZ, const TArray<FVector>& InToePath, const FName& InToeName)
{
	int32 Num = InToePath.Num();
	if (Num > 1)
	{
		FVector StartPos3D = InToePath[0];
		FVector StartPos2D = FVector(StartPos3D.X, StartPos3D.Y, 0.f);
		FVector EndPos3D = InToePath[Num - 1];
		FVector EndPos2D = FVector(EndPos3D.X, EndPos3D.Y, 0.f);

		FVector CurPos2D = FVector(CurCharacterBottomLocation.X, CurCharacterBottomLocation.Y, 0.f);

		FVector Traslation2D = EndPos2D - StartPos2D;
		float Traslation2DSize = Traslation2D.Size();
		float ProjectLength = UKismetMathLibrary::Dot_VectorVector(Traslation2D, CurPos2D - StartPos2D) / Traslation2DSize;

		int32 ProjectPathIndex = -1;

		for (int32 i = 1; i < Num; ++i)
		{
			if (ProjectLength * ProjectLength <= (InToePath[i] - StartPos2D).SizeSquared2D())
			{
				ProjectPathIndex = i;
				break;
			}
		}

		int32 SlopePathIndex = -1;

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
				FVector EndPos3DAdjustMaxSlope = ProjectPathPos + ProjectPathDir;

				float SlopeAlpha = MaxSlopePathNor.Z - ProjectPathNor.Z;
				float SlopeZ = FMath::Lerp(EndPos3DAdjustMaxSlope.Z, EndPos3D.Z, FMath::Clamp(MaxSlopeToePathAlpha - SlopeAlpha, 0.f, 1.f));

				EndPos3D = FVector(EndPos3DAdjustMaxSlope.X, EndPos3DAdjustMaxSlope.Y, SlopeZ);
			}

			UE_LOG(LogQuadrupedIK, Verbose, TEXT("CurIndex: %d SlopeIndex: %d MaxSlopeNorZ: %f ProjNorZ: %f |%s"),
				ProjectPathIndex, SlopePathIndex, MaxSlopePathNor.Z, ProjectPathNor.Z, *InToeName.ToString());
		}

		return EndPos3D;
	}

	return FVector::ZeroVector;
}


void UPredictionAnimInstance::GetToeHeightLimitByPathCurve(float& OutHeightLimit, const FVector& InToeCurPos, const TArray<FVector>& InToePath)
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
				OutHeightLimit = UKismetMathLibrary::MapRangeClamped(ProjectLength - BeforeSize, 0.f, AfterSize - BeforeSize, InToePath[i - 1].Z, InToePath[i].Z);
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
	float dt,
	EPredictionMotionFoot InLstMotionFoot, 
	EPredictionMotionFoot InCurMotionFoot)
{
	if (InLstMotionFoot != EPredictionMotionFoot::None)
	{
		FVector Start2D = FVector(OutFootStartPos.X, OutFootStartPos.Y, 0.f);
		FVector End2D = FVector(InFootEndPos.X, InFootEndPos.Y, 0.f);
		FVector Traslation2D = End2D - Start2D;
		float Traslation2DSize = Traslation2D.Size();
		float ProjectLength = UKismetMathLibrary::Dot_VectorVector(Traslation2D, FVector(InMappedPos.X, InMappedPos.Y, 0.f) - Start2D) / Traslation2DSize;
		UE_LOG(LogQuadrupedIK, Verbose, TEXT("FootStart Height: %f FootEnd Height: %f Offset: %f Mapped: %f"), OutFootStartPos.Z, InFootEndPos.Z, InFootEndPos.Z - OutFootStartPos.Z, ProjectLength / Traslation2DSize);
		const float ClampedPct = FMath::Clamp(ProjectLength / Traslation2DSize, 0.f, 2.f);
		OutTargetMeshPosZ = FMath::GetRangeValue(FVector2D(OutFootStartPos.Z, InFootEndPos.Z), ClampedPct);
		UE_LOG(LogQuadrupedIK, Verbose, TEXT("Final Z: %f"), OutTargetMeshPosZ);
	}

	if (InCurMotionFoot != InLstMotionFoot)
	{
		UE_LOG(LogQuadrupedIK, Verbose, TEXT("SwitchFoot IsStartFoot: %d Final Z: %f"), InLstMotionFoot == EPredictionMotionFoot::None, OutTargetMeshPosZ);
		OutFootStartPos = CurCharacterBottomLocation;
		OutFootStartPos.Z = OutTargetMeshPosZ;
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
	const FTransform& ComponentToWorld = GetOwningComponent()->GetComponentToWorld();

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

	auto TraceType = bDrawDebugForReactFootIK ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

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

	ValidbLeftHit = LeftHit.IsValidBlockingHit();
	LeftFootHitZ = ValidbLeftHit ? LeftHit.Location.Z : CurCharacterBottomLocation.Z;

	float TargetRightFootHeightOffset = 0.f;
	float TargetLeftFootHeightOffset = 0.f;
	float MinHitZ = FMath::Min(RightFootHitZ, LeftFootHitZ);
	if (ValidRightHit && ValidbLeftHit && (MinHitZ - CurCharacterBottomLocation.Z) > -ReactFootIKHeightThreshold)
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
		OutRightHitNor = FVector(0.f, 0.f, 1.f);
		OutLeftHitNor = FVector(0.f, 0.f, 1.f);
		OutMinHitZ = CurCharacterBottomLocation.Z;
	}

	OutRightFootHeight = UKismetMathLibrary::FInterpTo(OutRightFootHeight, TargetRightFootHeightOffset, DeltaSeconds, FootIKHeightOffsetInterpSpeed);
	OutLeftFootHeight = UKismetMathLibrary::FInterpTo(OutLeftFootHeight, TargetLeftFootHeightOffset, DeltaSeconds, FootIKHeightOffsetInterpSpeed);

	OutRightHitNor = ComponentToWorld.InverseTransformVector(OutRightHitNor);
	OutLeftHitNor = ComponentToWorld.InverseTransformVector(OutLeftHitNor);
}



void UPredictionAnimInstance::DebugDrawToePath(
	const TArray<FVector>& InToePath,
	const FVector& InToePos,
	const FVector& InToePredictivePos,
	FLinearColor InColor)
{
	if (InToePath.Num() == 0)
	{
		return;
	}

	int32 Num = InToePath.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		UKismetSystemLibrary::DrawDebugPoint(GetWorld(), InToePath[i], 5.0f, InColor);
	}

	UKismetSystemLibrary::DrawDebugLine(GetWorld(), InToePath[0], InToePath[Num - 1], InColor, 0.f, 2.f);

	UKismetSystemLibrary::DrawDebugPoint(GetWorld(), InToePredictivePos, 15.f, InColor);

#if false
	TArray<AActor*> IgnoreActors;
	FHitResult Hit;

	UKismetSystemLibrary::LineTraceSingle(
		GetWorld(),
		InToePos,
		InToePos - FVector(0.f, 0.f, 100.f),
		TraceChannel,
		false,
		IgnoreActors,
		EDrawDebugTrace::ForOneFrame,
		Hit,
		true,
		InColor,
		FLinearColor::Red);
#endif

}


void UPredictionAnimInstance::DebugDrawPelvisPath()
{
	UKismetSystemLibrary::DrawDebugLine(GetWorld(), MotionFootStartPos_MapByRootPos, MotionFootEndPos, FLinearColor::Green, 0.f, 2.f);
	UKismetSystemLibrary::DrawDebugBox(GetWorld(), MotionFootEndPos, FVector(5.f, 5.f, 5.f), FLinearColor::Black, FRotator::ZeroRotator, 0.f, 1.f);
	UKismetSystemLibrary::DrawDebugBox(GetWorld(), CurCharacterBottomLocation, FVector(5.f, 5.f, 5.f), FLinearColor::White, FRotator::ZeroRotator, 0.f, 1.f);
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


bool UPredictionAnimInstance::ShouldUseRelativeLocationToMovementBase() const
{
	UObject* MovementBaseObject = GetMovementBaseObjectForPrediction();
	if (!MovementBaseObject)
	{
		return false;
	}

	FMovementBaseInterfaceData BaseData(MovementBaseObject);
	return BaseData.IsValid() && MovementBaseUtility::UseRelativeLocation(&BaseData);
}

bool UPredictionAnimInstance::ShouldUseRelativeLocationToHitBase(const FHitResult& Hit) const
{
	UObject* HitObject = Hit.GetComponent();
	if (!HitObject)
	{
		return false;
	}

	FMovementBaseInterfaceData BaseData(HitObject);
	return BaseData.IsValid() && MovementBaseUtility::UseRelativeLocation(&BaseData);
}

UObject* UPredictionAnimInstance::GetMovementBaseObjectForPrediction() const
{
	if (CharacterMovementComponent)
	{
		return CharacterMovementComponent->GetMovementBaseObject();
	}

	// Moverは現時点ではAnimInstanceからMovementBaseを直接取らない
	// 必要になったらMover/Locomotion側で「現在Base」を公開してここに接続
	return nullptr;
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

