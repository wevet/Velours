// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "WvAbilityType.h"
#include "WvGameplayCue_HitImpact.generated.h"

class ABasePawn;

/**
 * 
 */
UCLASS()
class VELOURS_API UWvGameplayCue_HitImpact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Asset")
	UWvCueConfigDataAssest* CueConfigDataAssest;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Asset")
	bool IsDebug{ true };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Asset")
	float DebugTime{ 5.f };

public:
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;


private:
	void HandleAttack(ABasePawn* BeHitActor, ABasePawn* Attacker, const FGameplayCueParameters& Parameters, FOnceApplyEffect& OnceApplyEffect, const FHitResult* HitResult);
	void HandleBeHit(ABasePawn* BeHitActor, ABasePawn* Attacker, const FGameplayCueParameters& Parameters, FOnceApplyEffect& OnceApplyEffect, const FHitResult* HitResult);
	void GetHitImpactParticleLocation(ABasePawn* BeHitActor, ABasePawn* Attacker, const FHitResult* HitResult, FName FixedAttachBoneName, FVector LocationOffset, FVector& OutLocation);
	void GetHitImpactParticleRotation(ABasePawn* BeHitActor, ABasePawn* Attacker, const FGameplayCueParameters& Parameters, const FHitResult* HitResult, EParticleRotationMode RotationMode, bool isOnlyYaw, FRotator RotatorOffset, FRotator& OutRotation);
};
