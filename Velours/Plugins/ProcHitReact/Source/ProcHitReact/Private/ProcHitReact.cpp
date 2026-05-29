
#include "ProcHitReact.h"

#define LOCTEXT_NAMESPACE "FProcHitReactModule"

DEFINE_LOG_CATEGORY(LogProcHitReact)


class FProcHitReactModule : public IProcHitReactModule
{
	virtual void StartupModule() override
	{
		UE_LOG(LogProcHitReact, Log, TEXT("ProcHitReact Plugin : StartupModule"));
	}

	virtual bool IsGameModule() const override
	{
		return true;
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogProcHitReact, Log, TEXT("ProcHitReact Plugin : ShutdownModule"));
	}
};

IMPLEMENT_MODULE(FProcHitReactModule, ProcHitReact)


#undef LOCTEXT_NAMESPACE

