// Copyright 2022 wevet works All Rights Reserved.


#include "Component/WvFactionComponent.h"
#include "Velours.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvFactionComponent)

UWvFactionComponent::UWvFactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UWvFactionComponent::BeginPlay()
{
	Super::BeginPlay();
	Super::SetComponentTickEnabled(false);

	if (GetOwnerRole() == ROLE_Authority)
	{
		if (!BaseFactionTag.IsValid())
		{
			BaseFactionTag = TAG_Faction_Neutral;
			MARK_PROPERTY_DIRTY_FROM_NAME(UWvFactionComponent, BaseFactionTag, this);
		}

		RefreshEffectiveFactionFromOwner();
	}
}

void UWvFactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UWvFactionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UWvFactionComponent, EffectiveFactionTag, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UWvFactionComponent, FactionOwnerActor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UWvFactionComponent, BaseFactionTag, Params);
}

FGameplayTag UWvFactionComponent::GetBaseFactionTag() const
{
	return BaseFactionTag.IsValid() ? BaseFactionTag : TAG_Faction_Neutral;
}

FGameplayTag UWvFactionComponent::GetEffectiveFactionTag() const
{
	return EffectiveFactionTag.IsValid() ? EffectiveFactionTag : GetBaseFactionTag();
}

void UWvFactionComponent::SetBaseFactionTag(FGameplayTag NewFaction)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!NewFaction.IsValid())
	{
		NewFaction = TAG_Faction_Neutral;
	}

	if (BaseFactionTag == NewFaction)
	{
		return;
	}

	const FGameplayTag OldFaction = BaseFactionTag;
	BaseFactionTag = NewFaction;

	MARK_PROPERTY_DIRTY_FROM_NAME(UWvFactionComponent, BaseFactionTag, this);

	// EffectiveFaction ‚ª–¢Ý’èA‚Ü‚½‚ÍŒÃ‚¢ BaseFaction ‚É’Ç]‚µ‚Ä‚¢‚½ê‡‚Íˆê‚ÉXV‚·‚é
	if (!EffectiveFactionTag.IsValid() || EffectiveFactionTag == OldFaction)
	{
		SetEffectiveFactionTag(BaseFactionTag);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[%s] Owner=%s OldBase=%s NewBase=%s"),
		*FString(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*OldFaction.ToString(),
		*BaseFactionTag.ToString());
}

void UWvFactionComponent::SetEffectiveFactionTag(FGameplayTag NewFaction)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!NewFaction.IsValid())
	{
		NewFaction = GetBaseFactionTag();
	}


	if (EffectiveFactionTag == NewFaction)
	{
		return;
	}

	const FGameplayTag OldFaction = EffectiveFactionTag;
	EffectiveFactionTag = NewFaction;

	MARK_PROPERTY_DIRTY_FROM_NAME(UWvFactionComponent, EffectiveFactionTag, this);

	UE_LOG(LogTemp, Verbose, TEXT("[%s] Owner=%s OldEffective=%s NewEffective=%s"),
		*FString(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*OldFaction.ToString(),
		*EffectiveFactionTag.ToString());
}

void UWvFactionComponent::SetFactionOwnerActor(AActor* NewOwnerActor)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (FactionOwnerActor == NewOwnerActor)
	{
		return;
	}

	FactionOwnerActor = NewOwnerActor;

	MARK_PROPERTY_DIRTY_FROM_NAME(UWvFactionComponent, FactionOwnerActor, this);

	RefreshEffectiveFactionFromOwner();
}

void UWvFactionComponent::RefreshEffectiveFactionFromOwner()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (IsValid(FactionOwnerActor))
	{
		if (const UWvFactionComponent* OwnerFactionComponent = FactionOwnerActor->FindComponentByClass<UWvFactionComponent>())
		{
			const FGameplayTag OwnerEffectiveFaction = OwnerFactionComponent->GetEffectiveFactionTag();
			if (OwnerEffectiveFaction.IsValid())
			{
				SetEffectiveFactionTag(OwnerEffectiveFaction);
				return;
			}
		}
	}

	SetEffectiveFactionTag(GetBaseFactionTag());
}

void UWvFactionComponent::OnRep_EffectiveFactionTag()
{
	UE_LOG(LogTemp, Verbose, TEXT("[%s] Owner=%s EffectiveFaction=%s"),
		*FString(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*EffectiveFactionTag.ToString());

	// If UI, debug, or AI cache updates are needed, broadcast a delegate here
}

void UWvFactionComponent::OnRep_BaseFactionTag()
{
	UE_LOG(LogTemp, Verbose, TEXT("[%s] Owner=%s BaseFaction=%s"),
		*FString(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*BaseFactionTag.ToString());

	// If UI, debug, or AI cache updates are needed, broadcast a delegate here
}

void UWvFactionComponent::OnRep_FactionOwnerActor()
{
	UE_LOG(LogTemp, Verbose, TEXT("[%s] Owner=%s FactionOwner=%s"),
		*FString(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(FactionOwnerActor));

	// If UI, debug, or AI cache updates are needed, broadcast a delegate here
}

