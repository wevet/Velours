// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/CharacterSystemTypes.h"
#include "LocomotionComponent.generated.h"


UCLASS( ClassGroup=(Movement), meta=(BlueprintSpawnableComponent) )
class VELOURS_API ULocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULocomotionComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	void SetGaitMode(const ELSGait NewGait);

protected:
	UPROPERTY(Transient)
	FLocomotionEssencialVariables LocomotionEssencialVariables;


};
