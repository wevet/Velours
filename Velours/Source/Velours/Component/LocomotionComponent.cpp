// Copyright 2022 wevet works All Rights Reserved.


#include "Component/LocomotionComponent.h"

ULocomotionComponent::ULocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULocomotionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void ULocomotionComponent::SetGaitMode(const ELSGait NewGait)
{
	LocomotionEssencialVariables.LSGait = NewGait;
}


