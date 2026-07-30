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

};
