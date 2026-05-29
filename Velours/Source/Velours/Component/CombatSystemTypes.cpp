// Copyright 2022 wevet works All Rights Reserved.

#include "CombatSystemTypes.h"


DEFINE_LOG_CATEGORY(LogWvCombatSystem)


#pragma region ComboChainSystem
FComboChainSystem::FComboChainSystem()
{
	Count = 0;
	CurTimer = 0.f;
	Timer = 0.f;
	Speed = 1.0f;
	K_InitDuration = 0.7f;
	bIsPlaying = false;
}

void FComboChainSystem::Begin()
{
	Count = 0;
	CurTimer = 0.f;

	bIsPlaying = true;
	Push();
	UE_LOG(LogWvCombatSystem, Log, TEXT("[%s]"), *FString(__FUNCTION__));
}

void FComboChainSystem::Push()
{
	if (!bIsPlaying)
	{
		UE_LOG(LogWvCombatSystem, Error, TEXT("not valid chain system => [%s]"), *FString(__FUNCTION__));
		return;
	}

	++Count;

	Timer = FMath::Max(K_InitDuration / Count, 0.15f);
	CurTimer = 0.f;

	UE_LOG(LogWvCombatSystem, Log, TEXT("[%s], Timer => %.3f, Count => %d"), *FString(__FUNCTION__), Timer, Count);
}

void FComboChainSystem::Update(const float DeltaTime)
{
	if (!bIsPlaying)
	{
		return;
	}

	CurTimer += DeltaTime * FMath::Max(Speed, 0.f);

	if (CurTimer >= Timer)
	{
		End();
	}
}

bool FComboChainSystem::IsPlaying() const
{
	return bIsPlaying;
}

float FComboChainSystem::GetProgressValue() const
{
	if (Timer <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp((Timer - CurTimer) / Timer, 0.f, 1.0f);
}

void FComboChainSystem::SetSpeed(const float NewSpeed)
{
	Speed = FMath::Max(NewSpeed, 0.f);
}

void FComboChainSystem::End()
{
	bIsPlaying = false;
	Count = 0;
	CurTimer = 0.f;
	UE_LOG(LogWvCombatSystem, Log, TEXT("[%s]"), *FString(__FUNCTION__));
}

#pragma endregion

