// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BasePawn.h"
#include "VBaseHuman.generated.h"

/**
 * 
 */
UCLASS()
class VELOURS_API AVBaseHuman : public ABasePawn
{
	GENERATED_BODY()
	

public:
	AVBaseHuman(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaTime) override;

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;
};
