// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/CharacterSystemTypes.h"
#include "Engine/DataAsset.h"
#include "WeaknessComponent.generated.h"

UCLASS(BlueprintType)
class VELOURS_API UCharacterWeaknessDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FCharacterWeaknessContainer> WeaknessContainer;

	FCharacterWeaknessData FindCharacterWeaknessData(const EAttackWeaponState InWeaponState, const FName HitBoneName) const;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UWeaknessComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWeaknessComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UCharacterWeaknessDataAsset* WeaknessDA;

public:
	FCharacterWeaknessData FindCharacterWeaknessData(const EAttackWeaponState InWeaponState, const FName HitBoneName) const;
};
