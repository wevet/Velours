// Copyright 2022 wevet works All Rights Reserved.


#include "Component/WvRelationshipComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvRelationshipComponent)

UWvRelationshipComponent::UWvRelationshipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UWvRelationshipComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(bRelationshipEnabled && bAutoDecay);
}


void UWvRelationshipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRelationshipEnabled || !bAutoDecay)
	{
		return;
	}

	DecayTimeAccumulator += DeltaTime;
	if (DecayTimeAccumulator < DecayInterval)
	{
		return;
	}

	const float DecayDeltaTime = DecayTimeAccumulator;
	DecayTimeAccumulator = 0.0f;

	DecayRelationshipMemory(DecayDeltaTime);
}

void UWvRelationshipComponent::SetRelationshipEnabled(bool bEnabled)
{
	bRelationshipEnabled = bEnabled;
	SetComponentTickEnabled(bRelationshipEnabled && bAutoDecay);
}

FWvRelationshipMemory* UWvRelationshipComponent::FindOrAddMemory(AActor* TargetActor)
{
	if (!bRelationshipEnabled || !IsValid(TargetActor) || TargetActor == GetOwner())
	{
		return nullptr;
	}

	for (FWvRelationshipMemory& Memory : RelationshipMemories)
	{
		if (Memory.TargetActor == TargetActor)
		{
			return &Memory;
		}
	}

	FWvRelationshipMemory& NewMemory = RelationshipMemories.AddDefaulted_GetRef();
	NewMemory.TargetActor = TargetActor;
	NewMemory.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	return &NewMemory;
}

const FWvRelationshipMemory* UWvRelationshipComponent::FindMemory(AActor* TargetActor) const
{
	if (!bRelationshipEnabled || !IsValid(TargetActor))
	{
		return nullptr;
	}

	for (const FWvRelationshipMemory& Memory : RelationshipMemories)
	{
		if (Memory.TargetActor == TargetActor)
		{
			return &Memory;
		}
	}

	return nullptr;
}

bool UWvRelationshipComponent::HasMemoryFor(AActor* TargetActor) const
{
	return FindMemory(TargetActor) != nullptr;
}

void UWvRelationshipComponent::NotifyThreatenedBy(AActor* InstigatorActor, float Amount)
{
	FWvRelationshipMemory* Memory = FindOrAddMemory(InstigatorActor);
	if (!Memory)
	{
		return;
	}

	Memory->Fear += Amount;
	Memory->Suspicion += Amount * 0.5f;
	Memory->Trust -= Amount * 0.25f;
	Memory->LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ClampMemory(*Memory);
	BroadcastMemoryChanged(*Memory);
}

void UWvRelationshipComponent::NotifyDamagedBy(AActor* InstigatorActor, float DamageAmount)
{
	FWvRelationshipMemory* Memory = FindOrAddMemory(InstigatorActor);
	if (!Memory)
	{
		return;
	}

	Memory->Anger += DamageAmount;
	Memory->Fear += DamageAmount * 0.25f;
	Memory->Trust -= DamageAmount * 0.5f;
	Memory->LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ClampMemory(*Memory);
	BroadcastMemoryChanged(*Memory);
}

void UWvRelationshipComponent::NotifyHelpedBy(AActor* InstigatorActor, float Amount)
{
	FWvRelationshipMemory* Memory = FindOrAddMemory(InstigatorActor);
	if (!Memory)
	{
		return;
	}

	Memory->Trust += Amount;
	Memory->Fear -= Amount * 0.25f;
	Memory->Suspicion -= Amount * 0.25f;
	Memory->Anger -= Amount * 0.25f;
	Memory->LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ClampMemory(*Memory);
	BroadcastMemoryChanged(*Memory);
}

void UWvRelationshipComponent::NotifySuspiciousOf(AActor* TargetActor, float Amount)
{
	FWvRelationshipMemory* Memory = FindOrAddMemory(TargetActor);
	if (!Memory)
	{
		return;
	}

	Memory->Suspicion += Amount;
	Memory->Trust -= Amount * 0.1f;
	Memory->LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ClampMemory(*Memory);
	BroadcastMemoryChanged(*Memory);
}

void UWvRelationshipComponent::ClearMemoryFor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	RelationshipMemories.RemoveAll([TargetActor](const FWvRelationshipMemory& Memory)
	{
		return Memory.TargetActor == TargetActor;
	});
}

void UWvRelationshipComponent::ClearAllMemories()
{
	RelationshipMemories.Reset();
}

EWvFactionAttitude UWvRelationshipComponent::GetMemoryAttitudeFor(AActor* TargetActor) const
{
	const FWvRelationshipMemory* Memory = FindMemory(TargetActor);
	if (!Memory)
	{
		return EWvFactionAttitude::Neutral;
	}

	if (Memory->Anger >= HostileAngerThreshold)
	{
		return EWvFactionAttitude::Hostile;
	}

	if (Memory->Fear >= FearThreshold)
	{
		return EWvFactionAttitude::Fear;
	}

	if (Memory->Suspicion >= SuspiciousThreshold)
	{
		return EWvFactionAttitude::Suspicious;
	}

	if (Memory->Trust >= FriendlyTrustThreshold)
	{
		return EWvFactionAttitude::Friendly;
	}

	return EWvFactionAttitude::Neutral;
}

void UWvRelationshipComponent::ClampMemory(FWvRelationshipMemory& Memory) const
{
	Memory.Trust = FMath::Clamp(Memory.Trust, -100.0f, 100.0f);
	Memory.Fear = FMath::Clamp(Memory.Fear, 0.0f, 100.0f);
	Memory.Suspicion = FMath::Clamp(Memory.Suspicion, 0.0f, 100.0f);
	Memory.Anger = FMath::Clamp(Memory.Anger, 0.0f, 100.0f);
}

void UWvRelationshipComponent::BroadcastMemoryChanged(const FWvRelationshipMemory& Memory)
{
	OnRelationshipMemoryChanged.Broadcast(Memory.TargetActor, Memory);
}

void UWvRelationshipComponent::DecayRelationshipMemory(float DeltaTime)
{
	for (int32 Index = RelationshipMemories.Num() - 1; Index >= 0; --Index)
	{
		FWvRelationshipMemory& Memory = RelationshipMemories[Index];

		if (!Memory.IsValid())
		{
			RelationshipMemories.RemoveAtSwap(Index);
			continue;
		}

		Memory.Trust = FMath::FInterpConstantTo(Memory.Trust, 0.0f, DeltaTime, TrustDecayPerSecond);
		Memory.Fear = FMath::FInterpConstantTo(Memory.Fear, 0.0f, DeltaTime, FearDecayPerSecond);
		Memory.Suspicion = FMath::FInterpConstantTo(Memory.Suspicion, 0.0f, DeltaTime, SuspicionDecayPerSecond);
		Memory.Anger = FMath::FInterpConstantTo(Memory.Anger, 0.0f, DeltaTime, AngerDecayPerSecond);

		ClampMemory(Memory);

		const bool bIsNearlyEmpty = FMath::IsNearlyZero(Memory.Trust, 0.1f) && 
			FMath::IsNearlyZero(Memory.Fear, 0.1f) && 
			FMath::IsNearlyZero(Memory.Suspicion, 0.1f) && 
			FMath::IsNearlyZero(Memory.Anger, 0.1f);

		if (bIsNearlyEmpty)
		{
			RelationshipMemories.RemoveAtSwap(Index);
		}
		else
		{
			BroadcastMemoryChanged(Memory);
		}
	}
}


