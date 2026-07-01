// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PredictionFootIKComponent.h"
#include "PredictionAnimInstance.generated.h"

class UCharacterMoverComponent;
class UCapsuleComponent;


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

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool EnableFootIK() const;

	virtual void NativeInitializeAnimation() override;
	virtual void NativeBeginPlay() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	static float INVALID_TOE_DISTANCE;
	static float DEFAULT_TOE_HEIGHT_LIMIT;

private:
	bool TickPredictiveFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, bool BlockPredictive, bool AbnormalMove);

	void TickReactFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float InMinHitZ);

	void TickDisableFootIK(float DeltaSeconds, float& OutTargetMeshPosZ, float Weight, bool EnableInterp);

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
	bool Step1_PredictiveToeEndPos(FVector& OutToeEndPos, const FPredictionToePathInfo& InPastPath, const float& InCurToeCurveValue, const FName& InToeName);
	void Step2_TraceToePath(TArray<FVector>& OutToePath, float& OutToeHeightLimit, const FVector& InToeStartPos, const FVector& InToeCurPos, FVector InToeEndPos, const FName& InToeName, const float& DeltaSeconds);
	void Step3_CalcMeshPosZ(float& OutTargetMeshPosZ, const float& InRightEndDist, const float& InLeftEndDist, const FVector& InRightToePos, const FVector& InLeftToePos, const FVector& InRightEndPos, const FVector& InLeftEndPos, const float& DeltaSeconds);
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


	void CalcToeEndPosByCurve(FVector& OutToeEndPos, const float& InCurToeCurveValue);
	void CalcToeEndPosByDefaultDistance(FVector& OutToeEndPos, const FPredictionToePathInfo& InPastPath);
	void CheckEndPosByTrace(bool& OutEndPosChanged, FVector& OutToeEndPos, const FVector& InLastToeEndPos);
	void LineTracePath2(bool& OutEndPosValid, TArray<FVector>& OutToePath, const FVector& InToeStartPos, const FVector& InToeEndPos);
	void GetToeHeightLimitByPathCurve(float& OutHeightLimit, const FVector& InToeCurPos, const TArray<FVector>& InToePath);
	void CalcPelvisOffset2(float& OutTargetMeshPosZ, FVector& OutFootStartPos, const FVector& InFootEndPos, const FVector& InMappedPos, float dt, EPredictionMotionFoot InLstMotionFoot, EPredictionMotionFoot InCurMotionFoot);
	void TraceForTwoFoots(float DeltaSeconds, float& OutMinHitZ, float& OutRightFootHeight, float& OutLeftFootHeight, FVector& OutRightHitNor, FVector& OutLeftHitNor);

private:
	void DebugDrawToePath(const TArray<FVector>& InToePath, const FVector& InToePos, FLinearColor InColor);
	void DebugDrawPelvisPath();

public:
	UPROPERTY(EditAnywhere, Category = "Debug")
	uint8 bDrawDebug : 1;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForToe : 1;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForPelvis : 1;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug"))
	uint8 bDrawDebugForTrace : 1;

	UPROPERTY(EditAnywhere, Category = "Config")
	uint8 bEnableCurvePredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Config")
	uint8 bEnableDefaultDistancePredictive : 1;

	UPROPERTY(EditAnywhere, Category = "Config")
	float DefaultToeFirstPathDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ReactFootIKUpTraceHeight = 40.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ReactFootIKDownTraceHeight = 100.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ToeWidth = 5.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float AbnormalMoveCosAngle = 0.71f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float AbnormalMoveTimeLimit = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float TeleportedDistanceThreshold = 100.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float TraceIntervalLength = 30.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float InvalidToeEndDist = 10.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ToeHeightThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float PelvisHeightThreshold = 70.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ReactFootIKHeightThreshold = 60.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float EndPosChangedDistanceSquareThreshold = 100.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float EndPosChangedHeightThreshold = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float EndPosZInterpSpeed = 15.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float MeshPosZInterpSpeedWhenDisableFootIK = 10.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float MeshPosZInterpSpeedWhenReactFootIK = 10.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float FootIKHeightOffsetInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float ToeLeaveFloorOffset = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Config")
	FName RightToeName;

	UPROPERTY(EditAnywhere, Category = "Config")
	FName LeftToeName;

	UPROPERTY(EditAnywhere, Category = "Config")
	FName RightFootName;

	UPROPERTY(EditAnywhere, Category = "Config")
	FName LeftFootName;

	UPROPERTY(EditAnywhere, Category = "Config")
	FVector TarFootOffset = FVector(0.f, 0.f, 1.5f);

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	bool bEnableToeVelocityPredictive = true;

	UPROPERTY(EditAnywhere, Category = "Prediction Foot IK|No Curve")
	bool bEnablePastPathPredictive = true;

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

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraceSettings")
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

public:
	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float PelvisFinalOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	bool FootIKByHeightLimit = false;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	bool FootIKByHeightOffset = false;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float RightToeHeightLimit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float LeftToeHeightLimit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float RightFootHeightOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	float LeftFootHeightOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
	FVector RightFootHitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "To Rig Parameter")
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
	float CurRightToeCurveValue = 0.f;
	float CurLeftToeCurveValue = 0.f;
	float CurMoveSpeedCurveValue = 0.f;
	FVector RightToeCSPos;
	FVector LeftToeCSPos;
	bool ValidPredictiveWeight = false;

private:
	FPredictionToePathInfo RightToePathInfo;
	FPredictionToePathInfo LeftToePathInfo;

	TArray<FVector> RightToePath;
	TArray<FVector> LeftToePath;

private:
	FVector MotionFootStartPos_MapByRootPos;
	FVector MotionFootStartPos_MapByToePos;
	FVector MotionFootEndPos;
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

public:
	virtual void InitializeBoneOffset(const int32 BoneIndex);
	virtual void SetBoneLocationOffset(const int32 BoneIndex, const FVector& Location);
	virtual FVector GetBoneLocationOffset(const int32 BoneIndex) const;
	virtual void SetBoneRotationOffset(const int32 BoneIndex, const FRotator& Rotation);
	virtual FRotator GetBoneRotationOffset(const int32 BoneIndex) const;

private:
	bool ShouldUseRelativeLocationToMovementBase() const;
	bool ShouldUseRelativeLocationToHitBase(const FHitResult& Hit) const;
	UObject* GetMovementBaseObjectForPrediction() const;

	TArray<AActor*> IgnoreActors;
	bool bIsPawnTypeMover{ false };
	bool ShouldRunPredictive() const;
	FVector GetOwnerVelocity() const;
	bool IsHitWalkableForPrediction(const FHitResult& Hit) const;

};
