// Copyright 2022 wevet works All Rights Reserved.


#include "WvEngineSubsystem.h"
#include "AbilitySystemGlobals.h"
#include "WvAbilitySystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvEngineSubsystem)

void UWvEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UAbilitySystemGlobals::Get().InitGlobalData();
	UAbilitySystemGlobals::Get().GetGameplayCueManager();
}

