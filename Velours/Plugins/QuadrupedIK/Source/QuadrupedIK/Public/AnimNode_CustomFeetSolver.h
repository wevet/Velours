// Copyright 2022 wevet works All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CustomIKData.h"
#include "AnimNode_CustomIKControlBase.h"
#include "AnimNode_CustomFeetSolver.generated.h"


class USkeletalMeshComponent;
class UPredictionAnimInstance;


USTRUCT()
struct QUADRUPEDIK_API FAnimNode_CustomFeetSolver : public FAnimNode_CustomIKControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputData, meta = (PinHiddenByDefault))
	FCustomIKData_MultiInput SolverInputData;

	FQuadlupedBoneStruct SolverBoneData;


#pragma region BasicSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings)
	EIKType IKType = EIKType::TwoBoneIk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinShownByDefault))
	bool bEnableSolver = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bEnablePitch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bEnableRoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bShouldRotateFeet = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bUseFourPointFeets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bEnableFootLiftLimit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	bool bIsCalcFingerJoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault))
	float MaxLegIKAngle = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character UpVector"))
	FVector CharacterDirectionVectorCS = FVector(0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Character ForwardDirection"))
	FVector CharacterForwardDirectionVector_CS = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (PinHiddenByDefault, DisplayName = "Pole Vector"))
	FVector PolesForwardDirectionVector_CS = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasicSettings, meta = (EditCondition = "IKType == EIKType::FabrikIk"))
	EFabrikBoneRotationSource EffectorRotationSource{ EFabrikBoneRotationSource::CopyFromTarget };

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve FingerVelocityCurve;
#pragma endregion


#pragma region TraceSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings)
	EIKRaycastType RaycastTraceType = EIKRaycastType::LineTrace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings)
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TraceSettings, meta = (PinHiddenByDefault))
	bool bIsTraceOptimization{false};

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
#pragma endregion


#pragma region FabrikSettings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk"))
	int32 FabrikIterations = 10;

	/*
	* spider joint fabrik option
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk"))
	bool bIsSpiderMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk"))
	float FabrikPrecision = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk && bIsSpiderMode"))
	float KneeDropIntensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk && bIsSpiderMode"))
	float KneeDropInterpSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk && bIsSpiderMode"))
	float SpiderReachRatio = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FabrikSettings, meta = (PinHiddenByDefault, EditCondition = "IKType == EIKType::FabrikIk && bIsSpiderMode"))
	FVector KneeBendBaseDir = FVector(0.f, 0.35f, -0.94f);
#pragma endregion


#pragma region InterpSetting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings)
	EIKInterpLocationType LocationInterpType = EIKInterpLocationType::LegacyLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings)
	EIKInterpRotationType RotationInterpType = EIKInterpRotationType::LegacyRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings)
	FComponentSpacePoseLink BlendRefPose;

	/*
	* modify location  only ZAxis
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	bool bInterpolateOnly_Z = true;

	/*
	* if true disable smooth interp location and rotation
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	bool bIgnoreLerping = false;

	/*
	* Adopts a rotation system that moves the Fetto by rotating it to closely follow the terrain
	* When disabled, only the legs rotate. This may create a slight gap.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	bool bEnableComplexRotationMethod = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	float WeightAlphaInterpSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	float LocationLerpSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InterpSettings, meta = (PinHiddenByDefault))
	float FeetRotationSpeed = 6.0f;

	/*
	* Moving Speed Angle Limit Curve
	*/
	UPROPERTY(EditAnywhere, Category = InterpSettings, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve ComplexSimpleFootVelocityCurve;

	UPROPERTY(EditAnywhere, Category = InterpSettings, meta = (PinHiddenByDefault))
	FRuntimeFloatCurve InterpolationVelocityCurve;
#pragma endregion



	TArray<FQuadlupedBoneHitPairs> SpineHitPairs;

	UPROPERTY(Transient)
	TArray<FTransform> KneeAnimatedTransformArray;

	// for editmode
	TArray<TArray<FVector>> FeetTipLocations;
	// for editmode
	TArray<TArray<float>> FeetWidthSpacing;

	TArray<TArray<float>> FeetRootHeights;
	TArray<TArray<TArray<float>>> FeetFingerHeights;

	TArray<TArray<FVector>> FootKneeOffsetArray;
	TArray<FQuadrupedBone_SpineFeetPair> SpineFeetPair;
	TArray<FQuadlupedBoneSpineFeetPair_WS> SpineTransformPairs;
	TArray<TArray<TArray<FTransform>>> FeetFingerTransformArray;

	TArray<FBoneReference> KneeBoneRefArray;


public:
	FAnimNode_CustomFeetSolver();
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	virtual void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const;

protected:
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;


private:
	void ApplyLegFull(
		const FComponentSpacePoseContext& Output,
		const FBoneReference& BoneRef, 
		const int32 FeetIndex,
		const int32 HitIndex,
		FComponentSpacePoseContext& MeshBasesSaved, 
		TArray<FBoneTransform>& OutBoneTransforms);

	void ApplyTwoBoneIK(
		const FBoneContainer& RequiredBones, 
		const FBoneReference& IKFootBone, 
		const int32 FeetIndex,
		const int32 HitIndex,
		FComponentSpacePoseContext& MeshBasesSaved,
		TArray<FBoneTransform>& OutBoneTransforms);

	void ApplySingleBoneIK(
		const FBoneContainer& RequiredBones, 
		const FBoneReference& IKFootBone, 
		const int32 FeetIndex,
		const int32 HitIndex,
		FComponentSpacePoseContext& MeshBasesSaved, 
		TArray<FBoneTransform>& OutBoneTransforms);

	void ApplyFabrikIK(
		const FBoneContainer& RequiredBones,
		const FBoneReference& IKFootBone,
		const int32 FeetIndex,
		const int32 HitIndex,
		FComponentSpacePoseContext& MeshBasesSaved,
		TArray<FBoneTransform>& OutBoneTransforms);



	FVector ClampRotateVector(
		const FVector& InputPosition,
		const FVector& ForwardVectorDir,
		const FVector& Origin,
		const float MinClampDegrees,
		const float MaxClampDegrees, 
		const float HClampMin,
		const float HClampMax) const;

	
	TArray<FName> BoneArrayMachine_Spine(
		const FBoneContainer& RequiredBones,
		const int32 Index, 
		const FName& StartBoneName,
		const FName& EndBoneName,
		const bool bWasFootBone);

	TArray<FName> BoneArrayMachine_Feet(
		const FBoneContainer& RequiredBones,
		const int32 Index, 
		const FCustomBone_FootData& FootData,
		const FName& EndBoneName,
		const bool bWasFootBone);

	bool CheckLoopExist(
		const FBoneContainer& RequiredBones,
		const int32 OrderIndex,
		const FCustomBone_FootData& InFootData,
		const FName& StartBone,
		const FName& KneeBone,
		const FName& ThighBone,
		const FName& InputBone,
		const TArray<FName>& TotalSpineBones);

	FVector AnimationLocationLerp(const bool bIsHit, const FVector& StartPosition, const FVector& EndPosition, const float DeltaSeconds) const;
	FQuat AnimationQuatSlerp(const bool bIsHit, const FQuat& StartRotation, const FQuat& EndRotation, const float DeltaSeconds) const;

	FRotator RotationFromImpactNormal(
		const int32 SpineIndex,
		const int32 FeetIndex,
		const bool bIsFinger,
		FComponentSpacePoseContext& Output,
		const FVector& NormalImpactInput,
		const FTransform& OriginalBoneTransform,
		const float FeetLimit) const;


	void ApplyLineTrace(
		const FAnimationUpdateContext& Context,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& OutHitResult,
		const FLinearColor& DebugColor,
		const bool bRenderTrace);

	void ApplyMultiPointTraceBulk(
		const FAnimationUpdateContext& Context,
		const FVector& CenterOrigin, 
		const FVector& FrontTarget, 
		const FVector& MidLocationLeft,
		const FVector& MidLocationRight,
		const float SideSpacing,
		const float StartScale, 
		const float EndScale, 
		const int32 SIndex,
		const int32 FIndex);


	void PrepareAnimatedPoseInfo_AnyThread(FComponentSpacePoseContext& Output);


	TArray<FQuadrupedBone_SpineFeetPair> SwapSpinePairs(TArray<FQuadrupedBone_SpineFeetPair>& OutSpineFeetArray);

	void CalculateFeetHeight(FComponentSpacePoseContext& Output);
	void CalculateFeetRotation(FComponentSpacePoseContext& Output, TArray<TArray<FTransform>> FeetRotationArray);
	void GetResetedPoseInfo(FCSPose<FCompactPose>& MeshBases);

	void BuildLegRotationArray(FComponentSpacePoseContext& Output, TArray<TArray<FTransform>>& OutFeetRotationArray);


private:
	float CachedDeltaSeconds = 0.f;
	float ComponentScale{ 1.0f };
	float FormatLocationLerp{ 0.0f };
	float FormatRotationLerp{ 0.0f };
	float CharacterMovementSpeed{ 0.0f };
	bool bHasAtleastHit = false;
	bool bSolveShouldFail = false;
	bool bIsInitialized = false;
	bool bFirstTimeSetup = true;

	bool bWarpDetected{ false };

	float SmoothFactor = 10.0f;
	float DirectionChangeAlpha{0.f};
	float DirectionChangeSmoothing{ 0.f };

	FVector LastOwnerLocation{ FVector::ZeroVector };
	FVector PreviousMovementDirection{FVector::ZeroVector};
	// 前フレームの足位置を保存
	TArray<TArray<FVector>> PreviousFeetLocations; 

	FRotator CachedCharacterRotation{ FRotator::ZeroRotator };
	FRotator PreviousCharacterRotation{ FRotator::ZeroRotator };

	// キャラクター回転の遅延量
	float CharacterRotationLag{ 0.f }; 


	UPROPERTY(Transient)
	TArray<FVector> TraceStartList;

	UPROPERTY(Transient)
	TArray<FVector> TraceEndList;

	UPROPERTY(Transient)
	TArray<FColor> TraceLinearColor;

	UPROPERTY(Transient)
	TArray<AActor*> IgnoreActors;

	TArray<FBoneReference> FootBoneRefArray;
	TArray<TArray<float>> FootAlphaArray;
	TArray<TArray<FTransform>> FeetModofyTransformArray;
	TArray<TArray<FVector>> FeetModifiedNormalArray;
	TArray<TArray<FVector>> FeetImpactPointArray;


	UPROPERTY(Transient)
	TArray<FName> TotalSpineBoneArray;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> owning_skel = nullptr;
	
	TArray<FQuadlupedBoneSpineFeetPair_WS> SpineAnimatedTransformPairs;


};

