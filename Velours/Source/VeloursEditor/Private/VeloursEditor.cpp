// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VeloursEditor.h"

/**
 * Implements the VeloursEditor module.
 */
class FVeloursEditorModule : public IVeloursEditorPlugin
{
public:

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_GAME_MODULE(FVeloursEditorModule, VeloursEditor);

