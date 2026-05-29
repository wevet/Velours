// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterMovementHelperComponent.generated.h"

class ABasePawn;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UCharacterMovementHelperComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCharacterMovementHelperComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;


private:
	TWeakObjectPtr<ABasePawn> Character;
	
};
