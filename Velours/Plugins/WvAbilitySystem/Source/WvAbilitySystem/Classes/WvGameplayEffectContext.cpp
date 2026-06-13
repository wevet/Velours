// Copyright 2020 wevet works All Rights Reserved.


#include "WvGameplayEffectContext.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvGameplayEffectContext)

void FWvGameplayEffectContext::AddInstigator(class AActor* InInstigator, class AActor* InEffectCauser)
{
	Super::AddInstigator(InInstigator, InEffectCauser);
	const UGameplayAbility* AbilityIns = GetAbilityInstance_NotReplicated();
	if (!AbilityIns)
	{
		const UGameplayAbility* Ability = GetAbility();
		UAbilitySystemComponent* InstigatorAsc = GetOriginalInstigatorAbilitySystemComponent();
		if (Ability && InstigatorAsc)
		{
			FGameplayAbilitySpec* Spec = InstigatorAsc->FindAbilitySpecFromClass(Ability->GetClass());
			if (Spec)
			{
				AbilityInstanceNotReplicated = Spec->GetPrimaryInstance();
			}
		}
	}
}

bool FWvGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = true;

	Super::NetSerialize(Ar, Map, bSuccess);

	if (Ar.IsLoading())
	{
		TargetDataHandle.Clear();
	}

	bool bTargetDataSuccess = true;
	TargetDataHandle.NetSerialize(Ar, Map, bTargetDataSuccess);

	Ar << EffectDataAsset;
	Ar << EffectGroupIdx;

	bOutSuccess = bSuccess && bTargetDataSuccess;
	return true;
}

FWvGameplayEffectContext* FWvGameplayEffectContext::Duplicate() const
{
	FWvGameplayEffectContext* NewContext = new FWvGameplayEffectContext();
	*NewContext = *this;
	NewContext->AddActors(Actors);
	if (GetHitResult())
	{
		// Does a deep copy of the hit result
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

void FWvGameplayEffectContext::SetEffectDataAsset(UWvAbilityEffectDataAsset* DataAsset, int32 Index/* = 0*/)
{
	EffectDataAsset = DataAsset;
	EffectGroupIdx = Index;
}

void FWvGameplayEffectContext::AddTargetDataHandle(const FGameplayAbilityTargetDataHandle& InTargetDataHandle)
{
	TargetDataHandle.Append(InTargetDataHandle);
}

int32 FWvGameplayEffectContext::GetEffectGroupIdx()
{
	return EffectGroupIdx; 
}

UWvAbilityEffectDataAsset* FWvGameplayEffectContext::GetAbilityEffectDataAsset()
{
	return EffectDataAsset; 
}

FGameplayAbilityTargetDataHandle FWvGameplayEffectContext::GetTargetDataHandle()
{
	return TargetDataHandle; 
}

void FWvGameplayEffectContext::AddTargetDataHandle(FGameplayAbilityTargetData* AbilityTargetData)
{
	FGameplayAbilityTargetDataHandle Handle;
	Handle.Add(AbilityTargetData);
	//TargetDataHandle.Add(AbilityTargetData);
	AddTargetDataHandle(Handle);
}

