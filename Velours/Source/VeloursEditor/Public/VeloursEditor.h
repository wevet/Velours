// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface to this module
 */
class IVeloursEditorPlugin : public IModuleInterface
{

public:

	static inline IVeloursEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IVeloursEditorPlugin>("VeloursEditorPlugin");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VeloursEditorPlugin");
	}
};