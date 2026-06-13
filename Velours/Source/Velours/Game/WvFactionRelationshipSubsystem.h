// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Component/WvRelationshipComponent.h"
#include "WvFactionRelationshipSubsystem.generated.h"

class UWvFactionComponent;
class UWvPlayerReputationComponent;

USTRUCT(BlueprintType)
struct FWvFactionRelationshipRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SourceFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag TargetFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWvFactionAttitude Attitude = EWvFactionAttitude::Neutral;
};

/**
 * Determine the relationship between any two actors, Actor A and Actor B
 */
UCLASS(ClassGroup = (Relationship))
class VELOURS_API UWvFactionRelationshipSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Faction")
	EWvFactionAttitude GetAttitude(const AActor* SourceActor, const AActor* TargetActor) const;

	UFUNCTION(BlueprintCallable, Category = "Faction")
	bool IsHostile(const AActor* SourceActor, const AActor* TargetActor) const;

	UFUNCTION(BlueprintCallable, Category = "Faction")
	bool IsFriendly(const AActor* SourceActor, const AActor* TargetActor) const;

	UFUNCTION(BlueprintCallable, Category = "Faction")
	bool ShouldWarmupAbilitySystem(const AActor* SourceActor, const AActor* TargetActor) const;

	UFUNCTION(BlueprintCallable, Category = "Faction")
	bool ShouldHotAbilitySystem(const AActor* SourceActor, const AActor* TargetActor) const;

	const UWvPlayerReputationComponent* GetPlayerReputationComponentFromActor(const AActor* Actor) const;

protected:
	EWvFactionAttitude GetBaseFactionAttitude(FGameplayTag SourceFaction, FGameplayTag TargetFaction) const;

	EWvFactionAttitude ApplyPlayerReputation(EWvFactionAttitude CurrentAttitude, const AActor* SourceActor, const AActor* TargetActor) const;

	EWvFactionAttitude ApplyRelationshipMemory(EWvFactionAttitude CurrentAttitude, const AActor* SourceActor, const AActor* TargetActor) const;

	EWvFactionAttitude ChooseStrongerAttitude(EWvFactionAttitude A, EWvFactionAttitude B) const;

	const UWvFactionComponent* GetFactionComponent(const AActor* Actor) const;
	const UWvRelationshipComponent* GetRelationshipComponent(const AActor* Actor) const;

	bool IsPlayerControlledActor(const AActor* Actor) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Faction")
	TArray<FWvFactionRelationshipRule> RelationshipRules;

};
