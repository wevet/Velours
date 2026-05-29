

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProcHitReact, Log, All)


class IProcHitReactModule : public IModuleInterface
{
public:

	static inline IProcHitReactModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IProcHitReactModule>("ProcHitReactModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ProcHitReactModule");
	}

};
