// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WvWidgetInterface.generated.h"

class ABasePawn;

/**
 * 
 */
UINTERFACE(BlueprintType)
class UWvWidgetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class VELOURS_API IWvWidgetInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WvWidgetInterface")
	void Initialize(ABasePawn* NewCharacter);

	
};
