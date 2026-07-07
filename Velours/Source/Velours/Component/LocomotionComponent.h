// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/CharacterSystemTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "LocomotionComponent.generated.h"

class UHitTargetComponent;
class ABasePawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLocomotionOverlayChangeDelegate, const ELSOverlayState, PrevOverlay, const ELSOverlayState, CurrentOverlay);


UCLASS(BlueprintType)
class VELOURS_API ULocomotionStateDataAsset : public UDataAsset
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FGameplayTag> MovementModeTagMap;


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag AimingTag;

public:
	FGameplayTag FindMovementModeTag(const FName& MovementModeName) const;
};



UCLASS( ClassGroup=(Movement), meta=(BlueprintSpawnableComponent) )
class VELOURS_API ULocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULocomotionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;


public:	
	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void SetStanceMode(const ELSStance NewLSStance);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void SetRotationMode(const ELSRotationMode NewLSRotationMode);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void SetOverlayState(const ELSOverlayState NewLSOverlayState);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void SetGaitMode(const ELSGait NewGait);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void SetLookAimTarget(const bool NewLookAtAimOffset, AActor* NewLookAtTarget);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void UpdateLookAimTargetComponent(UHitTargetComponent* NewLookAtTargetComponent);

	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	ELSOverlayState GetOverlayState() const { return LocomotionEssencialVariables.OverlayState; }

	UPROPERTY(BlueprintAssignable)
	FLocomotionOverlayChangeDelegate OnOverlayChangeDelegate;


	const FLocomotionEssencialVariables& GetLocomotionEssencialVariables() { return LocomotionEssencialVariables; }

	void OnMovementModeChanged(const FName& PreviousModeName, const FName& NewModeName, FGameplayTag& OutPrevMovementModeTag, FGameplayTag& OutNextMovementModeTag);

	void SetAiming(const bool bIsNewAiming);

protected:
	UPROPERTY(Transient)
	FLocomotionEssencialVariables LocomotionEssencialVariables;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|Config")
	TObjectPtr<class ULocomotionStateDataAsset> LocomotionStateDA;

private:
	void UpdateLocomotionState(const float DeltaTime);
	FVector ChooseTargetPosition() const;

	TWeakObjectPtr<class ABasePawn> Character;
};
