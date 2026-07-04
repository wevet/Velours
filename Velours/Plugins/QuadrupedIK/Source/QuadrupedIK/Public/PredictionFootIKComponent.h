// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomIKData.h"
#include "PredictionFootIKComponent.generated.h"

class UAnimInstance;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class QUADRUPEDIK_API UPredictionFootIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPredictionFootIKComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void SetCurveValue(EPredictionGait InGait, float InWeight, FName InCurveName, float InCurveValue);
	void SetToeCSPos(const FVector& InRightToeCSPos, const FVector& InLeftToeCSPos, const float& InWeight);
	void GetCurveValues(float& OutLeftCurveValue, float& OutRightCurveValue, float& OutMoveSpeedCurveValue, bool& OutIsSwitchGait);
	void GetToeCSPos(FVector& OutRightToeCSPos, FVector& OutLeftToeCSPos, bool& ValidWeight);
	void ClearCurveValues();
	void ClearToeCSPos();

	void ChangeSpeedCurveValue(EPredictionGait InGait, float InWeight, float InCurveValue);

	void UpdateAnimInstance(UAnimInstance* NewAnimInstance);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RightFootCurveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LeftFootCurveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName MoveSpeedCurveName;

private:
	EPredictionGait CurGait = EPredictionGait::Walk;
	TArray<FFootGaitCurveInfo> GaitCurveArray;

	float ToeWeight = 0.f;
	FVector RightToeCSPos{ FVector::ZeroVector };
	FVector LeftToeCSPos{ FVector::ZeroVector };

	UPROPERTY()
	TObjectPtr<class UAnimInstance> AnimInstance;
};


