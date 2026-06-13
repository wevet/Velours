// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Logging/LogMacros.h"
#include "RedemptionGameMode.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWvGameMode, All, All)

UCLASS(minimalapi)
class ARedemptionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARedemptionGameMode();

	virtual void StartPlay() override;

	void EnableCustomLensFlare();

	void DisableCustomLensFlare();
};



