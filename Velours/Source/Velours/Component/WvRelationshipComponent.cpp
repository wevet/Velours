// Copyright 2022 wevet works All Rights Reserved.


#include "Component/WvRelationshipComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvRelationshipComponent)

UWvRelationshipComponent::UWvRelationshipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWvRelationshipComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWvRelationshipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

