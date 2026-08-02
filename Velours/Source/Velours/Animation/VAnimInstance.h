// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationAsset.h"
//#include "Character/BaseCharacter.h"
#include "GameplayEffectTypes.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "Logging/LogMacros.h"

#include "VAnimInstance.generated.h"


USTRUCT(BlueprintType)
struct VELOURS_API FCharacterOverlayInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BasePose_N = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BasePose_CLF = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Spine_Add = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Head_Add = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_L_Add = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_R_Add = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Hand_L = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Hand_R = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_L_LS = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_R_LS = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_L_MS = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Arm_R_MS = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Enable_HandIK_L = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Enable_HandIK_R = 0.f;

	void ChooseStanceMode(const bool bIsStanding);
	void ModifyAnimCurveValue(const UAnimInstance* AnimInstance);

	void ShowDebugLog();
};


DECLARE_LOG_CATEGORY_EXTERN(LogVAnimation, All, All)
/**
 * 
 */
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType, meta = (BlueprintThreadSafe))
class VELOURS_API UVAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	UVAnimInstance();
	virtual ~UVAnimInstance() {}

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUninitializeAnimation() override;
	virtual void NativeBeginPlay() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;


	FVector GetPredictionStopLocation(const FVector& CurrentLocation) const;

protected:
	TWeakObjectPtr<AActor> TargetActor;

private:

	bool bOwnerPlayerController = false;
	const TArray<UAnimInstance*> GetAllAnimInstances();
	const TMap<FName, FAnimGroupInstance>& GetSyncGroupMapRead() const;
	const TArray<FAnimTickRecord>& GetUngroupedActivePlayersRead();
	void DrawRelevantAnimation();

	void RenderAnimTickRecords(
		const TArray<FAnimTickRecord>& Records, 
		const int32 HighlightIndex, 
		FColor TextColor,
		FColor HighlightColor,
		FColor InInactiveColor,
		bool bFullBlendSpaceDisplay) const;


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VAnimInstance|OverlayPose")
	FCharacterOverlayInfo CharacterOverlayInfo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VAnimInstance|OverlayPose")
	bool bIsDebugLogOverlay{false};
};


