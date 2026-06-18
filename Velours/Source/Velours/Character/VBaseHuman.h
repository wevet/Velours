// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BasePawn.h"
#include "VBaseHuman.generated.h"

class UCapsuleComponent;
class UWvSkeletalMeshComponent;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif


public:
	virtual class UWvSkeletalMeshComponent* GetWvSkeletalMeshComponent() const override;

protected:
	virtual void BeginPlay() override;


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWvSkeletalMeshComponent> SkeletalMeshComponent;
};
