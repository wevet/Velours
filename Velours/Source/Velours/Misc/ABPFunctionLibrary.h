// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ABPFunctionLibrary.generated.h"


class USkeletalMeshComponent;
class UAnimInstance;

/**
 * 
 */
UCLASS()
class VELOURS_API UABPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Layer")
	static UAnimInstance* GetLinkedAnimLayerInstance(USkeletalMeshComponent* Mesh, TSubclassOf<UAnimInstance> LinkedAnimInstanceClass);

	UFUNCTION(BlueprintPure, Category = "Animation|Linked Layer")
	static UAnimInstance* GetLinkedAnimGraphInstanceByTag(USkeletalMeshComponent* Mesh, FName Tag);
};
