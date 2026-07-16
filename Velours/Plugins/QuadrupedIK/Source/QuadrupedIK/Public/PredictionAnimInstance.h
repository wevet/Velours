// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PredictionFootIKComponent.h"
#include "PredictionAnimInstance.generated.h"

class UCharacterMoverComponent;
class UCapsuleComponent;
struct FFloatSpringState;


USTRUCT()
struct FToeRuntimeInfo
{
	GENERATED_BODY()

	FVector PrevCSPos = FVector::ZeroVector;
	FVector CurCSPos = FVector::ZeroVector;
	FVector VelocityCS = FVector::ZeroVector;

	FVector PrevWSPos = FVector::ZeroVector;
	FVector CurWSPos = FVector::ZeroVector;
	FVector VelocityWS = FVector::ZeroVector;
	FVector RelativeVelocityWS = FVector::ZeroVector;

	bool bInitialized = false;
};

USTRUCT()
struct FToePredictionDebugData
{
	GENERATED_BODY()

	FVector RawToeWSPos = FVector::ZeroVector;
	FVector LeaveFloorPos = FVector::ZeroVector;
	FVector PredictionOrigin = FVector::ZeroVector;
	FVector RawPredictionEndPos = FVector::ZeroVector;
	FVector FinalPathEndPos = FVector::ZeroVector;
	FVector LastPathStartPos = FVector::ZeroVector;

	FVector ToeVelocityWS = FVector::ZeroVector;
	FVector RelativeToeVelocityWS = FVector::ZeroVector;
	FVector OwnerVelocityWS = FVector::ZeroVector;

	bool bUsedContact = false;
	bool bUsedPastPath = false;
	bool bUsedToeVelocity = false;
	bool bPathValid = false;
	bool bUsedCurve = false;
	bool bUsedDefaultDistance = false;

};

USTRUCT()
struct QUADRUPEDIK_API FIKBaseAnimInstanceProxy : public FAnimInstanceProxy
{

public:
	GENERATED_BODY()

	struct FIKDebugData
	{
		FVector CenterOfMass{ FVector::ZeroVector };
		float Radius{ 20.0f };
		bool bValid = false;
	};

	mutable FIKDebugData IKDebugData;

	FIKBaseAnimInstanceProxy() {}

	FIKBaseAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance)
	{
		IKDebugData.bValid = false;
		IKDebugData.Radius = 0.0f;
		IKDebugData.CenterOfMass = FVector::ZeroVector;
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;
};

/**
 *
 */
UCLASS()
class QUADRUPEDIK_API UPredictionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPredictionAnimInstance();
	virtual ~UPredictionAnimInstance() {}

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUninitializeAnimation() override;
	virtual void NativeBeginPlay() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	static float INVALID_TOE_DISTANCE;
	static float DEFAULT_TOE_HEIGHT_LIMIT;


public:
	virtual void InitializeBoneOffset(const int32 BoneIndex);
	virtual void SetBoneLocationOffset(const int32 BoneIndex, const FVector& Location);
	virtual FVector GetBoneLocationOffset(const int32 BoneIndex) const;
	virtual void SetBoneRotationOffset(const int32 BoneIndex, const FRotator& Rotation);
	virtual FRotator GetBoneRotationOffset(const int32 BoneIndex) const;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool EnableFootIK() const;

	float GetPelvisFinalOffset() const;

protected:
	virtual bool ForceDisableFootIK() { return false; }

private:
	bool TickPredictiveFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, bool BlockPredictive, bool AbnormalMove);

	void TickReactFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float InMinHitZ);

	void TickDisableFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float Weight);

	/// <summary>
	/// Predictive Step
	/// 	0. prepare something
	///		1. predictive toe end position
	///		2. setup toe path
	///		3. get pelvis height offset
	/// 	4. complete
	/// </summary>
private:
	void Step0_Prepare(float DeltaSeconds);

	bool Step1_PredictiveToeEndPos(
		FVector& OutToeEndPos,
		const FPredictionToePathInfo& InPastPath,
		const float& InCurToeCurveValue,
		const FName& InToeName);

	void Step2_TraceToePath(
		TArray<FVector>& OutToePath,
		float& OutToeHeightLimit,
		const FVector& InToeStartPos,
		const FVector& InToeCurPos,
		FVector InToeEndPos,
		const FName& InToeName,
		const float& DeltaSeconds);

	void Step3_CalcMeshPosZ(
		float& OutTargetMeshPosZ,
		const float& InRightEndDist,
		const float& InLeftEndDist,
		bool InIsRightContacting,
		bool InIsLeftContacting,
		const FVector& InRightEndPos,
		const FVector& InLeftEndPos,
		const float& DeltaSeconds);

	void Step4_Completed();

private:
	void CurveSampling();

	void ToePosSampling(float DeltaSeconds);

	void UpdateToeRuntimeInfo(FToeRuntimeInfo& Info, const FVector& NewCSPos, const FTransform& ComponentToWorld, float DeltaSeconds);

	bool IsToeVelocityPredictable(const FName& InToeName) const;

	void CalcToeEndPosByToeVelocity(
		FVector& OutToeEndPos,
		const FPredictionToePathInfo& InPastPath,
		const FName& InToeName);

	void CalcToeEndPosByPastPath(
		FVector& OutToeEndPos,
		const FPredictionToePathInfo& InPastPath);

	void CalcToeEndPosByDefaultDistance(FVector& OutToeEndPos, const FPredictionToePathInfo& InPastPath);

	void CalcToeEndPosByCurve(FVector& OutToeEndPos, const float& InCurToeCurveValue);

	void CheckEndPosByTrace(bool& OutEndPosChanged, FVector& OutToeEndPos, const FVector& InLastToeEndPos);

	void LineTracePath2_Old(bool& OutEndPosValid, TArray<FVector>& OutToePath, const FVector& InToeStartPos, const FVector& InToeEndPos);
	void LineTracePath2(bool& OutEndPosValid, TArray<FVector>& OutToePath, const FVector& InToeStartPos, const FVector& InToeEndPos);

	FVector GetToePredictivePos(const float& InMeshPosZ, const TArray<FVector>& InToePath, const FName& InToeName);

	void GetToeHeightLimitByPathCurve(float& OutHeightLimit, const FVector& InToeCurPos, const TArray<FVector>& InToePath);


	bool GetFlatHeightFromPath(
		float& OutHeight,
		const FVector& InCurrentPos,
		const TArray<FVector>& InPath) const;

	bool GetPelvisHeightFromPath(
		float& OutHeight,
		const FVector& InCurrentPos,
		const TArray<FVector>& InPath,
		float TransitionHalfWidth) const;

	void CalcPelvisOffset2(
		float& OutTargetMeshPosZ,
		FVector& OutFootStartPos,
		const FVector& InFootEndPos,
		const FVector& InMappedPos,
		const float& DeltaSeconds,
		EPredictionMotionFoot InLstMotionFoot,
		EPredictionMotionFoot InCurMotionFoot);

	void TraceForTwoFoots(
		float DeltaSeconds,
		float& OutMinHitZ,
		float& OutRightFootHeight,
		float& OutLeftFootHeight,
		FVector& OutRightHitNor,
		FVector& OutLeftHitNor);

	bool RefineToeEndPosByTerrain(
		FVector& InOutToeEndPos,
		const FVector& InPredictionDirection) const;


private:
	void DebugDrawToePath(
		const TArray<FVector>& InToePath,
		const FVector& InToePos,
		const FVector& InToePredictivePos,
		FLinearColor InColor);

	void DebugDrawPelvisPath();


	void DebugDrawToePredictionDetailed(
		const FToePredictionDebugData& DebugData,
		const TArray<FVector>& ToePath,
		const FLinearColor& PathColor,
		const FString& Prefix) const;

public:
	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Debug")
	uint8 bDrawDebug : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForToe : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForPelvis : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForReactFootIK : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	bool bIsArrowPredictionFunction{false};

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	uint8 bEnableCurvePredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	uint8 bEnablePastPathPredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	uint8 bEnableToeVelocityPredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	uint8 bEnableDefaultDistancePredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float DefaultToeFirstPathDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float ReactFootIKUpTraceHeight = 40.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float ReactFootIKDownTraceHeight = 100.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float ToeWidth = 5.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float AbnormalMoveCosAngle = 0.71f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float AbnormalMoveTimeLimit = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float TeleportedDistanceThreshold = 100.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|HeightThreshold")
	float ToeHeightThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|HeightThreshold")
	float PelvisHeightUpThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|HeightThreshold")
	float PelvisHeightDownThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|HeightThreshold")
	float ReactFootIKHeightThreshold = 60.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|EndPosChanged")
	float EndPosChangedDistanceSquareThreshold = 100.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|EndPosChanged")
	float EndPosChangedHeightThreshold = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float TraceIntervalLength = 30.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float InvalidToeEndDist = 10.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float MaxSlopeToePathAlpha = 0.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float MaxSlopeToePathDownZ = 20.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float LeaveHysteresisThreshold{ 5.0f };

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	FVector2D StrideRatioRange{0.5f, 5.0f};

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|InterpSpeed")
	float EndPosZInterpSpeed = 15.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|InterpSpeed")
	float MeshPosZInterpSpeedWhenDisableFootIK = 10.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|InterpSpeed")
	float MeshPosZInterpSpeedWhenReactFootIK = 10.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|InterpSpeed")
	float FootIKHeightOffsetInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|InterpSpeed")
	float PelvisInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Joint")
	FName RightToeName;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Joint")
	FName LeftToeName;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Joint")
	FName RightFootName;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Joint")
	FName LeftFootName;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float ToeLeaveFloorOffset{4.0f};

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	FVector TargetFootEndPosOffset = FVector(0.0f, 0.0f, 1.5f);

	/** LineTracePath2 to stepup params */
	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float PredictionMaxStepUp = 30.0f;

	/** LineTracePath2 to stepup params */
	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|Config")
	float PredictionMaxStepDown = 60.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float ToeVelocityPredictionTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float MinToeVelocityForPrediction = 20.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float MinToePredictDistance = 15.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float MaxToePredictDistance = 90.f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float ToeVelocityDirWeight = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	float OwnerVelocityDirWeight = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK | Pelvis")
	float PelvisTransitionHalfWidth = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK | Pelvis")
	float PelvisFlatHeightTolerance = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK | Pelvis")
	float PelvisMaxUpSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK | Pelvis")
	float PelvisMaxDownSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK | Pelvis")
	float PelvisBaseFollowSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float PastPathDirectionWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float ToeVelocityDirectionWeight = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float OwnerVelocityDirectionWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float MinStrideScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float MaxStrideScale = 1.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Runtime Prediction")
	float ReferenceMoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Terrain Prediction")
	float LandingProbeSideOffset = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Terrain Prediction")
	float LandingProbeForwardOffset = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Terrain Prediction")
	float LandingProbeTraceHeight = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prediction Foot IK|Config")
	float PathStartChangedDistance = 8.f;

	FToePredictionDebugData RightToeDebugData;
	FToePredictionDebugData LeftToeDebugData;

	FVector LastValidMoveDirection = FVector::ZeroVector;


public:
	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float FootIKWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float PelvisFinalOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	bool FootIKByHeightLimit = false;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	bool FootIKByHeightOffset = false;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|PredictiveFootIK")
	float RightToeHeightLimit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|PredictiveFootIK")
	float LeftToeHeightLimit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|ReactFootIK")
	float RightFootHeightOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|ReactFootIK")
	float LeftFootHeightOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|ReactFootIK")
	FVector RightFootHitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter|ReactFootIK")
	FVector LeftFootHitNormal = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<class UCharacterMoverComponent> CharacterMoverComponent;

	UPROPERTY(Transient)
	TObjectPtr<class UCapsuleComponent> CapsuleComponent;

	UPROPERTY(Transient)
	TObjectPtr<class UCharacterMovementComponent> CharacterMovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FootIK")
	TObjectPtr<class UPredictionFootIKComponent> PredictionFootIKComponent;

private:
	TArray<AActor*> IgnoreActors;

	float CurRightToeCurveValue = 0.f;
	float CurLeftToeCurveValue = 0.f;
	float CurMoveSpeedCurveValue = 0.f;
	FVector RightToeCSPos;
	FVector LeftToeCSPos;
	bool ValidPredictiveWeight = false;
	bool bIsTraceComplex{true};

private:
	FPredictionToePathInfo RightToePathInfo;
	FPredictionToePathInfo LeftToePathInfo;

	TArray<FVector> RightToePath;
	TArray<FVector> LeftToePath;

	EPredictionMotionFoot PelvisSourceFoot = EPredictionMotionFoot::None;

private:
	FVector MotionFootStartPos_MapByRootPos{ FVector::ZeroVector };
	FVector MotionFootStartPos_MapByToePos{ FVector::ZeroVector };
	FVector MotionFootEndPos{ FVector::ZeroVector };
	EPredictionMotionFoot CurMotionFoot = EPredictionMotionFoot::None;

private:
	float CharacterMaxStepHeight = 0.f;
	float CharacterRadius = 0.f;
	float CharacterWalkableFloorZ = 0.f;

	FVector LstCharacterBottomLocation = FVector::ZeroVector;
	FVector CurCharacterBottomLocation = FVector::ZeroVector;

	float CurMeshWorldPosZ = 0.f;
	float CurRightFootWorldPosZ = 0.f;
	float CurLeftFootWorldPosZ = 0.f;

	float WeightOfDisableFootIK = 0.f;
	float AbnormalMoveTime = 0.f;

	FToeRuntimeInfo RightToeRuntimeInfo;
	FToeRuntimeInfo LeftToeRuntimeInfo;



protected:
	UPROPERTY()
	TMap<int32, FVector> OffsetLocations;

	UPROPERTY()
	TMap<int32, FRotator> OffsetRotations;


private:

	bool bIsPawnTypeMover{ false };
	bool ShouldRunPredictive() const;
	FVector GetOwnerVelocity() const;
	bool IsHitWalkableForPrediction(const FHitResult& Hit) const;

	// スプリング減衰後の、平滑化済みメッシュ(pelvis)ワールドZ。
	// 「実際に出力する値」を保持し続けるバッファで、毎フレームここを目標へ寄せていく。
	float SmoothedMeshWorldPosZ = 0.f;


};
