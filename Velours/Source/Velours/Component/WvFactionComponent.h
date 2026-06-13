// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
//#include "GenericTeamAgentInterface.h"
#include "WvFactionComponent.generated.h"

/// <summary>
/// Belongs to the Actor category
/// </summary>
UCLASS( ClassGroup=(Relationship), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UWvFactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWvFactionComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_BaseFactionTag, EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FGameplayTag BaseFactionTag;

	UPROPERTY(ReplicatedUsing = OnRep_EffectiveFactionTag, VisibleInstanceOnly, BlueprintReadOnly, Category = "Faction")
	FGameplayTag EffectiveFactionTag;

	UPROPERTY(ReplicatedUsing = OnRep_FactionOwnerActor, VisibleInstanceOnly, BlueprintReadOnly, Category = "Faction")
	TObjectPtr<AActor> FactionOwnerActor = nullptr;


	UFUNCTION()
	void OnRep_BaseFactionTag();

	UFUNCTION()
	void OnRep_EffectiveFactionTag();

	UFUNCTION()
	void OnRep_FactionOwnerActor();
		
public:

	FGameplayTag GetBaseFactionTag() const;
	FGameplayTag GetEffectiveFactionTag() const;

	UFUNCTION(BlueprintCallable, Category = "Faction")
	void SetBaseFactionTag(FGameplayTag NewFaction);

	UFUNCTION(BlueprintCallable, Category = "Faction")
	void SetEffectiveFactionTag(FGameplayTag NewFaction);

	UFUNCTION(BlueprintCallable, Category = "Faction")
	void SetFactionOwnerActor(AActor* NewOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "Faction")
	void RefreshEffectiveFactionFromOwner();

};
