// Copyright 2022 wevet works All Rights Reserved.


#include "Component/WvPlayerReputationComponent.h"
#include "Net/UnrealNetwork.h"
#if WITH_PUSH_MODEL 
#include "Net/Core/PushModel/PushModel.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvPlayerReputationComponent)

UWvPlayerReputationComponent::UWvPlayerReputationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWvPlayerReputationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWvPlayerReputationComponent::OnRep_ReputationData()
{
	// do something
}

void UWvPlayerReputationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

float UWvPlayerReputationComponent::GetFactionReputation(FGameplayTag FactionTag) const
{
	if (!FactionTag.IsValid())
	{
		return 0.0f;
	}

	for (const FWvFactionReputationEntry& Entry : ReputationData.FactionReputations)
	{
		if (Entry.FactionTag == FactionTag)
		{
			return Entry.Reputation;
		}
	}

	// Faction.Gang.Red ‚ª‚È‚¯‚ê‚Î Faction.Gang ‚ðŒ©‚é
	for (const FWvFactionReputationEntry& Entry : ReputationData.FactionReputations)
	{
		if (FactionTag.MatchesTag(Entry.FactionTag))
		{
			return Entry.Reputation;
		}
	}

	return 0.0f;
}

void UWvPlayerReputationComponent::ApplyFactionReputationDelta(FGameplayTag FactionTag, float Delta, FName Reason)
{
	const float CurrentValue = GetFactionReputation(FactionTag);
	SetFactionReputation(FactionTag, CurrentValue + Delta);
}

void UWvPlayerReputationComponent::MarkReputationDataDirty()
{
#if WITH_PUSH_MODEL
	MARK_PROPERTY_DIRTY_FROM_NAME(UWvPlayerReputationComponent, ReputationData, this);
#endif
}

void UWvPlayerReputationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#if WITH_PUSH_MODEL
	FDoRepLifetimeParams params;
	params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UWvPlayerReputationComponent, ReputationData, params);
#else
	DOREPLIFETIME(UHistoriaBlogActorComponent, MyValue);
#endif

}

void UWvPlayerReputationComponent::SetFactionReputation(FGameplayTag FactionTag, float NewValue)
{
	if (!GetOwner() || GetOwner()->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (!FactionTag.IsValid())
	{
		return;
	}

	NewValue = FMath::Clamp(NewValue, -100.0f, 100.0f);

	for (FWvFactionReputationEntry& Entry : ReputationData.FactionReputations)
	{
		if (Entry.FactionTag == FactionTag)
		{
			if (FMath::IsNearlyEqual(Entry.Reputation, NewValue))
			{
				return;
			}

			Entry.Reputation = NewValue;
			MarkReputationDataDirty();
			return;
		}
	}

	FWvFactionReputationEntry& NewEntry = ReputationData.FactionReputations.AddDefaulted_GetRef();
	NewEntry.FactionTag = FactionTag;
	NewEntry.Reputation = NewValue;

	MarkReputationDataDirty();
}

void UWvPlayerReputationComponent::CopyReputationFrom(const UWvPlayerReputationComponent* SourceComponent)
{
	if (!GetOwner() || GetOwner()->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (!SourceComponent)
	{
		return;
	}

	ReputationData = SourceComponent->GetReputationData();
	MarkReputationDataDirty();
}

