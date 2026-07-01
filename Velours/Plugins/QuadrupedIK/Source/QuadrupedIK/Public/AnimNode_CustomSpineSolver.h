// Copyright 2022 wevet works All Rights Reserved.

#pragma once
#include "CustomIKData.h"
#include "Animation/InputScaleBias.h"
#include "Animation/AnimNodeBase.h"
#include "CustomIKData.h"
#include "AnimNode_CustomIKControlBase.h"

#include "Async/TaskGraphInterfaces.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

#include "AnimNode_CustomSpineSolver.generated.h"


struct FQuadrupedIKTraceKey
{
	int32 A = 0;
	int32 B = 0;
	int32 C = 0;

	friend uint32 GetTypeHash(const FQuadrupedIKTraceKey& Key)
	{
		return HashCombine(
			HashCombine(::GetTypeHash(Key.A), ::GetTypeHash(Key.B)),
			::GetTypeHash(Key.C));
	}

	bool operator==(const FQuadrupedIKTraceKey& Other) const
	{
		return A == Other.A && B == Other.B && C == Other.C;
	}
};

struct FQuadrupedIKTraceSharedState
{
	mutable FCriticalSection Mutex;

	TMap<FQuadrupedIKTraceKey, FHitResult> CachedHits;
	TSet<FQuadrupedIKTraceKey> PendingKeys;

	bool TryGetCachedHit(const FQuadrupedIKTraceKey& Key, FHitResult& OutHit) const
	{
		FScopeLock Lock(&Mutex);

		if (const FHitResult* Found = CachedHits.Find(Key))
		{
			OutHit = *Found;
			return true;
		}

		return false;
	}

	bool MarkPendingIfNeeded(const FQuadrupedIKTraceKey& Key)
	{
		FScopeLock Lock(&Mutex);

		if (PendingKeys.Contains(Key))
		{
			return false;
		}

		PendingKeys.Add(Key);
		return true;
	}

	void CompleteTrace(const FQuadrupedIKTraceKey& Key, const FHitResult& Hit)
	{
		FScopeLock Lock(&Mutex);

		CachedHits.Add(Key, Hit);
		PendingKeys.Remove(Key);
	}

	void Clear()
	{
		FScopeLock Lock(&Mutex);

		CachedHits.Reset();
		PendingKeys.Reset();
	}
};

class FPrimitiveDrawInterface;
class USkeletalMeshComponent;
class UPredictionAnimInstance;


USTRUCT(BlueprintInternalUseOnly)
struct QUADRUPEDIK_API FAnimNode_CustomSpineSolver : public FAnimNode_CustomIKControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputData, meta = (PinHiddenByDefault))
	FCustomIKData_MultiInput SolverInputData;

	FQuadlupedBoneStruct SolverBoneData;

public:


#pragma region Solver
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	float Precision = 0.5f;

	/*
	*	Joint Pitch Angle Thredhold
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver, meta = (PinHiddenByDefault))
	FVector2D PitchRange{ -15.0f, 15.0f, };

	/*
	*	Joint Roll Angle Thredhold
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver, meta = (PinHiddenByDefault))
	FVector2D RollRange{ -1.0f, 1.0f, };


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	int32 MaxIterations = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	int32 MinIterations = 5;
#pragma endregion


#pragma region BasicSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinShownByDefault))
	bool bEnableSolver{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsCrouchMode{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIgnoreLerping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsUseManualLocationLerpSpeed{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bFlipForwardAndRight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, EditCondition = "bIsUseManualLocationLerpSpeed"))
	FVector2D LocationLerpSpeedRange = FVector2D(15.f, 6.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, EditCondition = "bIsUseManualLocationLerpSpeed"))
	FVector2D PawnSpeedRange = FVector2D(0.f, 700.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, EditCondition = "!bIsUseManualLocationLerpSpeed"))
	float LocationLerpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float RotationLerpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float ShiftSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character UpVector"))
	FVector CharacterDirectionVectorCS = FVector(0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character ForwardDirection"))
	FVector ForwardDirectionVector = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float PlaneFloorHeightThreshold{ 6.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float VelThresholdCmPerSec{ 0.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float TeleportThreshold{ 500.0f };

#pragma endregion


#pragma region TraceSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings)
	TEnumAsByte<ETraceTypeQuery> Trace_Channel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings)
	EIKRaycastType RaycastTraceType = EIKRaycastType::LineTrace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	bool bIsTraceOptimization{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault, EditCondition = "bIsTraceOptimization"))
	bool bIsRefineTraceEnable{false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	bool bDisplayLineTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float TraceRadiusValue = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float VirtualScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float LineTraceDownwardHeight = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float LineTraceUpperHeight = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float MaxFeetDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float MinFeetDistance = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	float VirtualLegWidth = 25.0f;
#pragma endregion


#pragma region VaridateSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VaridateSettings, meta = (PinHiddenByDefault))
	bool bEnableSupportWidthValidator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VaridateSettings, meta = (PinHiddenByDefault))
	float MaxAnimalSpineGapCm = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VaridateSettings, meta = (PinHiddenByDefault))
	float MaxAnimalVerticalGapCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VaridateSettings, meta = (UIMin = 0.5f, UIMax = 0.9f, ClampMin = 0.5f, ClampMax = 0.9f, PinHiddenByDefault))
	float NormalDotThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VaridateSettings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", PinHiddenByDefault))
	float MinSupportWidthRatioToSpineLength = 0.18f;
#pragma endregion


#pragma region TerrainGuard
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TerrainGuard, meta = (PinHiddenByDefault))
	bool bEnableMidSlopeContinuityGuard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TerrainGuard, meta = (PinHiddenByDefault, EditCondition = "bEnableMidSlopeContinuityGuard"))
	float MidSlopeResidualThreshold = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TerrainGuard, meta = (PinHiddenByDefault, EditCondition = "bEnableMidSlopeContinuityGuard"))
	float MidSlopeResidualRelaxedThreshold = 10.0f;
#pragma endregion


#pragma region SnakeSetting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SnakeSetting, meta = (PinHiddenByDefault))
	bool bSpineSnakeBone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SnakeSetting, meta = (PinHiddenByDefault))
	bool bIgnoreEndPoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SnakeSetting, meta = (PinHiddenByDefault, EditCondition = "bIgnoreEndPoints"))
	int32 IgnoreTipPointIndex{ 2 };

#pragma endregion


#pragma region Stabilization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (PinHiddenByDefault))
	bool bStabilizePelvisLegs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bStabilizePelvisLegs", PinHiddenByDefault))
	float PelvisUpSlopeStabilizationAlpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bStabilizePelvisLegs", PinHiddenByDefault))
	float PelvisDownSlopeStabilizationAlpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (PinHiddenByDefault))
	bool bStabilizeChestLegs = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bStabilizeChestLegs", PinHiddenByDefault))
	float ChestUpSlopeStabilizationAlpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Stabilization, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bStabilizeChestLegs", PinHiddenByDefault))
	float ChestDownslopeStabilizationAlpha = 0.5f;

	UPROPERTY(EditAnywhere, Category = Stabilization)
	FBoneReference StabilizationHeadBoneRef;

	UPROPERTY(EditAnywhere, Category = Stabilization)
	FBoneReference StabilizationTailBoneRef;
#pragma endregion


#pragma region HeadDetection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (PinHiddenByDefault))
	bool bHeadSlopeStabilization = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	FVector2D HeadPitchLimitRange = FVector2D(-15.0f, 15.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	FVector2D HeadRollLimitRange = FVector2D(-1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	FVector2D HeadSlopeAngleRange = FVector2D(0.f, 65.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float NoseTraceRadius = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float NoseForwardOffset{ 12.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float NoseTraceLength{ 8.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float HeadSurfaceSwitchSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float HeadPushNormalInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float HeadRotDeadZoneDeg = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HeadDetection, meta = (EditCondition = "bHeadSlopeStabilization", PinHiddenByDefault))
	float HeadPitchRollInterpSpeed = 8.0f;

	float HeadSurfaceBlendAlpha = 0.0f;
	FVector SmoothedHeadPushNormal_WS = FVector::UpVector;
	FRotator PrevHeadTargetRot = FRotator::ZeroRotator;

#pragma endregion


#pragma region PelvisControl
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float DipMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0", PinHiddenByDefault))
	float PelvisAdaptiveGravity = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0", PinHiddenByDefault))
	float CrouchPelvisAdaptiveGravity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float MaxDipHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float PelvisForwardRotationIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float PelvisSidewardRotationIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float PelvisBaseOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PelvisControl, meta = (PinHiddenByDefault))
	float CrouchedPelvisBaseOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = PelvisControl, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve PelvisHeightMultiplierCurve;
#pragma endregion


#pragma region ChestControl
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float ChestSideDipMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0", PinHiddenByDefault))
	float ChestAdaptiveGravity = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0", PinHiddenByDefault))
	float CrouchChestAdaptiveGravity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float MaxDipHeightChest = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float ChestForwardRotationIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float ChestSidewardRotationIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float ChestBaseOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ChestControl, meta = (PinHiddenByDefault))
	float CrouchedChestBaseOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = ChestControl, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve ChestHeightMultiplierCurve;
#pragma endregion


#pragma region AdvancedSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	ESolverComplexityType SolverComplexityType = ESolverComplexityType::Complex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	bool bRotateAroundTranslate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	bool bReverseFabrik = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	bool bOnlyRootSolve = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	bool bIgnoreChestSolve = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	float RotationPowerBetween = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	float MaxExtensionRatio = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	float MinExtensionRatio = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	float ExtensionSwitchSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	float MaxReachIntensity{ 1.0f };

	UPROPERTY(EditAnywhere, Category = AdvancedSettings, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve InterpolationMultiplierCurve;

#pragma endregion


public:
	FAnimNode_CustomSpineSolver();
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	virtual void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const;

protected:
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;


private:
	void PrepareAnimatedPoseInfo_AnyThread(FComponentSpacePoseContext& Output);

	void SnakeImpactRotation(const FComponentSpacePoseContext& Output, const int32 PointIndex, FTransform& OutputTransform, FCSPose<FCompactPose>& MeshBases);
	void ImpactRotation(const FComponentSpacePoseContext& Output, const int32 PointIndex, FTransform& OutputTransform, FCSPose<FCompactPose>& MeshBases);

	const TArray<FName> BoneArrayMachine(
		const FBoneContainer& RequiredBones,
		const int32 Index,
		const FName& StartBoneName,
		const FName& EndBoneName,
		const FName& ThighBoneName,
		const bool bWasFootBone);


	const bool CheckLoopExist(
		const FBoneContainer& RequiredBones,
		const FVector& FeetTraceOffset,
		const float FeetHeight,
		const FName& StartBoneName,
		const FName& InputBoneName,
		const FName& ThighBoneName,
		TArray<FName>& OutTotalSpineBoneArray);


	void ApplyLineTrace(
		const FAnimationUpdateContext& Context,
		const FVector& Origin,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& OutHitResult,
		const FLinearColor& DebugColor,
		const bool bDrawLine);

	void ApplyLineTraceCached(
		const FAnimationUpdateContext& Context,
		const FQuadrupedIKTraceKey& Key,
		const FVector& Origin,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& OutHitResult,
		const FLinearColor& DebugColor,
		const bool bDrawLine);

	void ApplyMultiPointTraceBulk(
		const FAnimationUpdateContext& Context,
		const FVector& Origin,
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FVector& ForwardDir,
		const FVector& RightDir,
		FQuadlupedBoneHitPairs& OutHitPair);

	TArray<FQuadrupedBone_SpineFeetPair> Swap_SpineFeetPairArray(TArray<FQuadrupedBone_SpineFeetPair>& OutSpineFeetPair);

	const FQuadrupedBoneSpineOutput BoneSpineProcessor(const FComponentSpacePoseContext& Output, FTransform& EffectorTransform, FCSPose<FCompactPose>& MeshBases);
	const FQuadrupedBoneSpineOutput BoneSpineProcessor_Direct(const FComponentSpacePoseContext& Output, FTransform& EffectorTransform, FCSPose<FCompactPose>& MeshBases);
	const FQuadrupedBoneSpineOutput BoneSpineProcessor_Snake(const FComponentSpacePoseContext& Output, const FTransform& EffectorTransform, FCSPose<FCompactPose>& MeshBases);
	const FQuadrupedBoneSpineOutput BoneSpineProcessor_Transform(FQuadrupedBoneSpineOutput& BoneSpine, const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases);

	FRotator BoneRelativeConversion(const FCompactPoseBoneIndex& ModifyBoneIndex, const FRotator& TargetRotation, const FBoneContainer& BoneContainer, FCSPose<FCompactPose>& MeshBases) const;
	
	void SolveSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms);
	void SolveBetweenSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases);

	void ResolveSpineIK(const FComponentSpacePoseContext& Output, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms);

	void CalcParentHitResult(const FAnimationUpdateContext& Context, const FHitResult& InHitResult, const FVector RelativePos, FVector& OutHitPoint);

	FSpineSupportData BuildSpineSupportData(
		const FComponentSpacePoseContext& Output,
		const FCSPose<FCompactPose>& MeshBases,
		const int32 PointIndex) const;

	FVector ComputeSpineTargetLocationCS(
		const FComponentSpacePoseContext& Output,
		const FSpineSupportData& Support,
		const int32 PointIndex,
		const bool bIsPelvisJoint) const;

	FRotator ComputeSpineTargetRotation(
		const FComponentSpacePoseContext& Output,
		const FSpineSupportData& Support,
		const int32 PointIndex,
		const bool bIsPelvisJoint,
		const FRotator& CurrentYawOnly) const;

	void ApplySpineTarget(
		const FComponentSpacePoseContext& Output,
		FTransform& OutputTransform,
		const FVector& TargetLocationCS,
		const FRotator& TargetRotation,
		const float LocationInterpSpeed,
		const float RotationInterpSpeed,
		const bool bEnableLocationLerp,
		const bool bEnableRotationLerp) const;

	void UpdateHandleHeightBoneSpineProcessor();
	float CalcInterpSpeed(const FVector& CurPos, const FVector& TargetPos) const;
	float CalcInterpRotationSpeed(const FQuat& CurRot, const FQuat& TargetRot) const;
	bool IsMovingBase(const FHitResult& Hit) const;

	void InitializeEffectorTransform(FCSPose<FCompactPose>& MeshBases);
	float EvalMidSlopeResidualCS(const FComponentSpacePoseContext& Output) const;
	bool IsContinuousSlope(const FComponentSpacePoseContext& Output, float Threshold) const;
	void ResetTraceResults();

protected:

	float SlopeUp01{ 0.f };
	float MaxFormatedHeight = 0.0f;
	float MaxFormatedDipHeightChest = 0.0f;
	float SmoothFactor = 10.0f;

	EFabrikType FabrikType{ EFabrikType::Humanoid };

	float FormatLocationLerp = 10.0f;
	float FormatRotationLerp = 10.0f;
	float FormatShiftSpeed = 50.0f;
	float ComponentScale = 1.0f;

	float MaxRangeLimitLerp = 1.05f;
	float PelvisSlopeStabAlpha = 1.0f;
	float ChestSlopeStabAlpha = 1.0f;

	float SpineMedianResult = 10.0f;
	float CharacterSpeed = 0.0f;

	bool bInitializeAnimationArray = false;
	bool bAtleastOneHit = false;
	bool bFeetIsEmpty = true;
	bool bWasSingleSpine = false;
	bool bSolveShouldFail = false;
	bool bEveryFootDontHaveChild = false;
	bool LineTraceInitialized = false;

	float PelvisSlopeDirection = 0.0f;
	float ChestSlopeDirection = 0.0f;

	/// <summary>
	/// if true chest up . false pelvis up
	/// </summary>
	bool bSolveFromChest = false;


	TArray<FQuadlupedBoneHitPairs> SpineHitPairs;

	UPROPERTY(Transient)
	TArray<FHitResult> SpineHitBetweenArray;

	UPROPERTY(Transient)
	TArray<FVector> SpinePointBetweenArray;

	UPROPERTY(Transient)
	TArray<FName> TotalSpineNameArray;

	UPROPERTY(Transient)
	TArray<FHitResult> SpineHitEdgeArray;

	TArray<FQuadrupedBone_SpineFeetPair> SpineFeetPair;
	TArray<FCompactPoseBoneIndex> SpineIndiceArray;
	TArray<FCompactPoseBoneIndex> ExtraSpineIndiceArray;
	TArray<FQuadlupedBoneSpineFeetPair_WS> SpineTransformPairArray;
	TArray<FQuadlupedBoneSpineFeetPair_WS> SpineAnimTransformPairArray;


	UPROPERTY(Transient)
	TArray<FVector> SpineBetweenTransformArray;

	UPROPERTY(Transient)
	TArray<FVector> SpineBetweenOffsetTransformArray;

	UPROPERTY(Transient)
	TArray<FVector> SnakeSpinePositionArray;

	UPROPERTY(Transient)
	TArray<FRotator> SpineRotationDiffArray;


	TArray<FBoneTransform> ReferencePoseBoneTransformArray;
	TArray<FBoneTransform> SolvedBoneTransformArray;
	TArray<FBoneTransform> SourcePoseBoneTransformArray;
	TArray<FBoneTransform> FinalBoneTransformArray;
	TArray<FCompactPoseBoneIndex> CombinedIndiceArray;


	FBoneReference RootBoneRef;

	UPROPERTY(Transient)
	TArray<float> TotalSpineHeights;

	UPROPERTY(Transient)
	TArray<float> TotalSpineAlphaArray;

	UPROPERTY(Transient)
	TArray<float> SpineBetweenHeightArray;

	UPROPERTY(Transient)
	TArray<FColor> TraceLinearColor;

	UPROPERTY(Transient)
	TArray<FVector> TraceStartList;

	UPROPERTY(Transient)
	TArray<FVector> TraceEndList;

	UPROPERTY(Transient)
	TArray<AActor*> IgnoreActors;

	FTransform ChestEffectorTransform = FTransform::Identity;
	FTransform RootEffectorTransform = FTransform::Identity;
	FCompactPoseBoneIndex PelvisIdx = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex ChestIdx = FCompactPoseBoneIndex(INDEX_NONE);
	FVector PrevRootCSLocation{ FVector::ZeroVector };
	FVector LastOwnerLocation{ FVector::ZeroVector };

	bool bWarpDetected{ false };
	bool bIsStationary{ false };

	float CachedDeltaSeconds = 0.f;
	float ChestDirSign{ 1.0f };

	float PreviousAlpha = 0.f;
	bool bRequireSnap = false;
	bool bEffectorInitialized{ false };

	float SmoothStraightAlpha{ 0.f };

	// head detection
	float CurrentHeadPushAlpha{ 0.f };
	FVector SmoothedHeadNormal{ FVector::ZeroVector };

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> owning_skel = nullptr;


private:
	TSharedPtr<FQuadrupedIKTraceSharedState, ESPMode::ThreadSafe> TraceSharedState;

	uint32 TraceFrameCounter = 0;

	FQuadrupedIKTraceKey MakeTraceKey(
		int32 TraceGroup,
		int32 TraceIndex,
		int32 TraceSubIndex) const;

	void RequestTraceOnGameThread(
		const FAnimationUpdateContext& Context,
		const FQuadrupedIKTraceKey& Key,
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FLinearColor& DebugColor);

	bool IsValidTraceInput(
		const FVector& StartLocation,
		const FVector& EndLocation,
		float Radius) const;

};

