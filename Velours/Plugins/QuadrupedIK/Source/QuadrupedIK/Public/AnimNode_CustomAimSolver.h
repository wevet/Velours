// Copyright 2022 wevet works All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CustomIKData.h"
#include "AnimNode_CustomIKControlBase.h"
#include "AnimNode_CustomAimSolver.generated.h"

class FPrimitiveDrawInterface;
class USkeletalMeshComponent;


USTRUCT()
struct QUADRUPEDIK_API FAnimNode_CustomAimSolver : public FAnimNode_CustomIKControlBase
{
	GENERATED_BODY()



public:

	UPROPERTY(EditAnywhere, Category = CoreInputData, meta = (DisplayName = "Start Bone (Eg:- Head)", PinHiddenByDefault))
	FBoneReference EndSplineBone;

	UPROPERTY(EditAnywhere, Category = CoreInputData, meta = (DisplayName = "End Bone", PinHiddenByDefault))
	FBoneReference StartSplineBone;

	UPROPERTY(EditAnywhere, Category = CoreInputData, meta = (PinShownByDefault))
	FTransform LookAtLocation{ FTransform::Identity };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CoreInputData, meta = (DisplayName = "Hands Input (optional)", PinHiddenByDefault))
	TArray<FCustomBone_ArmsData> AimingHandLimbs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (DisplayName = "Reach instead of aiming ? (Only for arms)", PinHiddenByDefault))
	bool bIsReachInstead = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (DisplayName = "Hand aiming/reaching use the override target transforms ?", PinHiddenByDefault))
	bool bIsUseSeparateTargets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	bool bIsNsewPoleMethod = false;


#pragma region Debug
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	FTransform DebugLookAtTransform{ FTransform::Identity };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category = Debug)
	TArray<FTransform> DebugHandTransforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bIsHeadDrawGizmos{false};
#pragma endregion


	UPROPERTY(Transient)
	TArray<FTransform> ElbowBoneTransformArray = TArray<FTransform>();


	UPROPERTY(Transient)
	TArray<FTransform> HandDefaultTransformArray = TArray<FTransform>();
	FTransform HeadOrigTransform{ FTransform::Identity };

	bool bIsFocusDebugtarget = true;

	TArray<FBoneTransform> HeadTransforms;

protected:



#pragma region ReachingAndSeparateAim
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (DisplayName = "Overrided Arm Aim Target Array", PinHiddenByDefault))
	FCustomBone_Overrided_Location_Data ArmTargetLocationOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	bool bIsOverrideHandRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (DisplayName = "If multi-arm aiming", PinHiddenByDefault))
	bool bIsAggregateHandBody = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (DisplayName = "If separate reaching mode", PinHiddenByDefault))
	bool bIsLetArmTwistWithHand = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	EArmTwistIKType ArmTwistAxis = EArmTwistIKType::PoseAxisTwist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	int32 PoleVectorIndex = 2;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	bool bIsEnableHandInterpolation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	bool bIsIgnoreElbowModification = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	bool bIsIgnoreSeparateHandSolving = false;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (EditCondition = "bIsEnableHandInterpolation", PinHiddenByDefault))
	float HandInterpolationSpeed = 10.0f;

	/// <summary>
	/// if -1 to automation handjoint
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReachingAndSeparateAim, meta = (PinHiddenByDefault))
	int32 MainArmIndex = INDEX_NONE;
#pragma endregion


#pragma region ClampSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Max Body Lookat Clamp", PinHiddenByDefault))
	float LookAtRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Inner Body Lookat Threshold", PinHiddenByDefault))
	FRotator InnerBodyClamp = FRotator(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Head Max Clamp", PinHiddenByDefault))
	float LookAtClamp = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Limbs Max Clamp", PinHiddenByDefault))
	float LimbsClamp = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Max Vertical Angle Range (Degrees)", PinHiddenByDefault))
	FVector2D VerticalRangeAngles = FVector2D(-75, 75);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ClampSettings, meta = (DisplayName = "Max Horizontal Angle Range (Degrees)", PinHiddenByDefault))
	FVector2D HorizontalRangeAngles = FVector2D(-90, 90);
#pragma endregion


#pragma region MultiplierSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float DownwardDipMultiplier = -0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float InvertedDipMultiplier = -0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float VerticalDipTreshold = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float SideMoveMultiplier = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float SideDownMultiplier = -0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MultiplierSettings, meta = (PinHiddenByDefault))
	float UpRotClamp = 0.5f;
#pragma endregion


#pragma region CurveInputSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurveInputSettings, meta = (DisplayName = "Bone Clamp Curve (0 = End Bone, 1 = Head Bone)", PinHiddenByDefault))
	FRuntimeFloatCurve LookBendingCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurveInputSettings, meta = (DisplayName = "Body Rotation Multiplier Curve (0 = End Bone, 1 = Head Bone)", PinHiddenByDefault))
	FRuntimeFloatCurve LookMultiplierCurve;
#pragma endregion


#pragma region BasicSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsUseNaturalMethod = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsHeadUseSeparateClamp = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsOverrideHeadRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsHeadAccurate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinShownByDefault))
	bool bIsEnableSolver = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	EIKInterpLocationType InterpLocationType = EIKInterpLocationType::DivisiveLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsEnableInterpolation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, EditCondition = "bIsEnableInterpolation"))
	float InterpolationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character UpVector"))
	FVector CharacterDirectionVectorCS = FVector(0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character ForwardDirection"))
	FVector ForwardDirectionVector = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (DisplayName = "Use Reference Forward Axis Logic", PinHiddenByDefault))
	bool bIsUseReferenceForwardAxis = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (DisplayName = "Reference Forward Axis", PinHiddenByDefault))
	FVector ReferenceConstantForwardAxis = FVector(0, 1, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (DisplayName = "Use Middle joint Axis", PinHiddenByDefault))
	bool bUseSpecificIntermediateAxis = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (DisplayName = "Middle joint Axis", PinHiddenByDefault))
	FAimBoneAxisSetting IntermediateLookAtAxis;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (DisplayName = "Work outside gameplay (For Sequencer)", PinShownByDefault))
	bool bIsWorkOutsidePIE = false;
#pragma endregion


#pragma region TailTerrainSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TailTerrainSettings, meta = (PinHiddenByDefault))
	bool bIsAdaptiveTerrainTail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TailTerrainSettings, meta = (PinHiddenByDefault, EditCondition = "bIsAdaptiveTerrainTail"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TailTerrainSettings, meta = (PinHiddenByDefault, EditCondition = "bIsAdaptiveTerrainTail"))
	float TraceUpHeight = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TailTerrainSettings, meta = (PinHiddenByDefault, EditCondition = "bIsAdaptiveTerrainTail"))
	float TraceDownHeight = 250.0f;
#pragma endregion




public:
	FAnimNode_CustomAimSolver();
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	virtual void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const;

#if WITH_EDITOR
	void ResizeDebugLocations(const int32 NewSize);
#endif


protected:
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	

private:


	void ApplyLineTrace(
		const FAnimationUpdateContext& Context,
		const FVector& StartPoint,
		const FVector& EndPoint,
		FHitResult& OutHitResult,
		const FLinearColor& DebugColor,
		const bool bIsDebugMode);


	const FBoneTransform LookAt_Processor(
		const FBoneContainer& RequiredBones,
		FComponentSpacePoseContext& Output,
		FCSPose<FCompactPose>& MeshBases,
		const FVector& OffsetVector,
		const FName& BoneName,
		const int32 InIndex,
		const float LookAtClampParam);


	void FABRIK_BodySystem(FCSPose<FCompactPose>& MeshBases, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms);

	FVector AnimLocationLerp(const FVector& InStartPosition, const FVector& InEndPosition, const float InDeltaSeconds) const;


private:


	bool bIsUpArmTwistTechnique = false;
	FTransform MainHandDefaultTransform{ FTransform::Identity };
	FTransform MainHandNewTransform{ FTransform::Identity };

	int32 NumValidSpines = 0;

	float ComponentScale = 1.0f;
	float HitResultHeight = 0.0f;
	bool bIsAtleastOneHit = false;

	UPROPERTY(Transient)
	FHitResult AimHitResult;

	UPROPERTY(Transient)
	TArray<AActor*> IgnoreActors;


	float HeadActualAlpha = 0.0f;
	float HandToggleAlpha = 0.0f;

	FVector LerpedLookatLocation = FVector::ZeroVector;

	FVector RefConstantForwardTemp = FVector(0, 1, 0);

	FTransform SavedLookAtTransform{ FTransform::Identity };
	FRotator LimbRotationOffset{FRotator::ZeroRotator};

	bool bIsArmsEnable{false};


	float MaxRangeLimitLerp = 0.0f;
	float CachedDeltaSeconds = 0.f;

	float SmoothFactor = 10.0f;
	bool bIsEveryFootDontHaveChild = false;

	bool bSolveShouldFail = false;
	bool bIsDebugHandsInitialized = false;


	UPROPERTY(Transient)
	TArray<float> LastShoulderAngles = TArray<float>();

	TArray<FBoneReference> HandBoneArray;
	TArray<FBoneReference> ElbowBoneArray;
	TArray<FBoneReference> ShoulderBoneArray;
	TArray<FBoneReference> ActualShoulderBoneArray;
	TArray<FArmSolverWorkArea> ArmSolverWorkArea;


	UPROPERTY(Transient)
	TArray<FVector> TraceStartList = TArray<FVector>();

	UPROPERTY(Transient)
	TArray<FVector> TraceEndList = TArray<FVector>();

	TArray<FBoneTransform> RestBoneTransforms;
	TArray<FBoneTransform> AnimatedBoneTransforms;
	TArray<FBoneTransform> Original_AnimatedBoneTransforms;
	TArray<FCompactPoseBoneIndex> CombinedIndices;


	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> owning_skel{ nullptr };



};

