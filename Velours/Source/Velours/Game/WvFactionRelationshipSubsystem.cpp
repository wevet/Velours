// Copyright 2022 wevet works All Rights Reserved.


#include "Game/WvFactionRelationshipSubsystem.h"
#include "Component/WvFactionComponent.h"
#include "Component/WvRelationshipComponent.h"
#include "GameFramework/Actor.h"
#include "Velours.h"
#include "GameFramework/Controller.h"
#include "Character/BasePlayerState.h"
#include "Component/WvPlayerReputationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvFactionRelationshipSubsystem)

EWvFactionAttitude UWvFactionRelationshipSubsystem::GetAttitude(const AActor* SourceActor, const AActor* TargetActor) const
{
	if (!IsValid(SourceActor) || !IsValid(TargetActor))
	{
		return EWvFactionAttitude::Neutral;
	}

	if (SourceActor == TargetActor)
	{
		return EWvFactionAttitude::Friendly;
	}

	const UWvFactionComponent* SourceFactionComp = GetFactionComponent(SourceActor);
	const UWvFactionComponent* TargetFactionComp = GetFactionComponent(TargetActor);

	if (!SourceFactionComp || !TargetFactionComp)
	{
		return EWvFactionAttitude::Neutral;
	}

	const FGameplayTag SourceFaction = SourceFactionComp->GetEffectiveFactionTag();
	const FGameplayTag TargetFaction = TargetFactionComp->GetEffectiveFactionTag();

	EWvFactionAttitude Attitude = GetBaseFactionAttitude(SourceFaction, TargetFaction);

	Attitude = ApplyPlayerReputation(Attitude, SourceActor, TargetActor);
	Attitude = ApplyRelationshipMemory(Attitude, SourceActor, TargetActor);

	return Attitude;
}

bool UWvFactionRelationshipSubsystem::IsHostile(const AActor* SourceActor, const AActor* TargetActor) const
{
	return GetAttitude(SourceActor, TargetActor) == EWvFactionAttitude::Hostile;
}

bool UWvFactionRelationshipSubsystem::IsFriendly(const AActor* SourceActor, const AActor* TargetActor) const
{
	return GetAttitude(SourceActor, TargetActor) == EWvFactionAttitude::Friendly;
}

bool UWvFactionRelationshipSubsystem::ShouldWarmupAbilitySystem(const AActor* SourceActor, const AActor* TargetActor) const
{
	const EWvFactionAttitude Attitude = GetAttitude(SourceActor, TargetActor);

	return Attitude == EWvFactionAttitude::Suspicious ||
		Attitude == EWvFactionAttitude::Fear ||
		Attitude == EWvFactionAttitude::Hostile;
}

bool UWvFactionRelationshipSubsystem::ShouldHotAbilitySystem(const AActor* SourceActor, const AActor* TargetActor) const
{
	return GetAttitude(SourceActor, TargetActor) == EWvFactionAttitude::Hostile;
}

EWvFactionAttitude UWvFactionRelationshipSubsystem::GetBaseFactionAttitude(FGameplayTag SourceFaction, FGameplayTag TargetFaction) const
{
	if (!SourceFaction.IsValid() || !TargetFaction.IsValid())
	{
		return EWvFactionAttitude::Neutral;
	}

	if (SourceFaction == TargetFaction)
	{
		return EWvFactionAttitude::Friendly;
	}

	for (const FWvFactionRelationshipRule& Rule : RelationshipRules)
	{
		const bool bSourceMatches = SourceFaction == Rule.SourceFaction || SourceFaction.MatchesTag(Rule.SourceFaction);
		const bool bTargetMatches = TargetFaction == Rule.TargetFaction || TargetFaction.MatchesTag(Rule.TargetFaction);

		if (bSourceMatches && bTargetMatches)
		{
			return Rule.Attitude;
		}
	}

	return EWvFactionAttitude::Neutral;
}

/// <summary>
/// SourceActor Ç™ NPC / Police / Civilian Ç≈ÅATargetActor Ç™ Player ÇÃÇ∆Ç´Ç…ï‚ê≥
/// </summary>
/// <param name="CurrentAttitude"></param>
/// <param name="SourceActor"></param>
/// <param name="TargetActor"></param>
/// <returns></returns>
EWvFactionAttitude UWvFactionRelationshipSubsystem::ApplyPlayerReputation(
	EWvFactionAttitude CurrentAttitude,
	const AActor* SourceActor,
	const AActor* TargetActor) const
{
	if (!IsValid(SourceActor) || !IsValid(TargetActor))
	{
		return CurrentAttitude;
	}

	if (!IsPlayerControlledActor(TargetActor))
	{
		return CurrentAttitude;
	}

	const UWvPlayerReputationComponent* ReputationComponent = GetPlayerReputationComponentFromActor(TargetActor);

	if (!ReputationComponent)
	{
		return CurrentAttitude;
	}

	const UWvFactionComponent* SourceFactionComponent = GetFactionComponent(SourceActor);
	if (!SourceFactionComponent)
	{
		return CurrentAttitude;
	}

	const FGameplayTag SourceFaction = SourceFactionComponent->GetEffectiveFactionTag();
	const FWvPlayerReputationData& ReputationData = ReputationComponent->GetReputationData();

	EWvFactionAttitude ResultAttitude = CurrentAttitude;

	// Police ÇÕ WantedLevel Çç≈óDêÊ
	if (SourceFaction.MatchesTag(TAG_Faction_Police))
	{
		if (ReputationData.WantedLevel > 0)
		{
			return EWvFactionAttitude::Hostile;
		}

		if (ReputationData.Honor <= -80.0f)
		{
			ResultAttitude = ChooseStrongerAttitude(ResultAttitude, EWvFactionAttitude::Suspicious);
		}

		return ResultAttitude;
	}

	// Civilian ÇÕ Honor ÇÃâeãøÇéÛÇØÇÈ
	if (SourceFaction.MatchesTag(TAG_Faction_Civilian))
	{
		if (ReputationData.Honor >= 70.0f)
		{
			if (ResultAttitude == EWvFactionAttitude::Neutral)
			{
				ResultAttitude = EWvFactionAttitude::Friendly;
			}
		}
		else if (ReputationData.Honor <= -70.0f)
		{
			ResultAttitude = ChooseStrongerAttitude(ResultAttitude, EWvFactionAttitude::Suspicious);
		}

		return ResultAttitude;
	}

	// Gang ÇÕ FactionReputation Çå©ÇÈ
	if (SourceFaction.MatchesTag(TAG_Faction_Gang))
	{
		const float FactionReputation = ReputationComponent->GetFactionReputation(SourceFaction);

		if (FactionReputation >= 60.0f)
		{
			if (ResultAttitude == EWvFactionAttitude::Neutral)
			{
				ResultAttitude = EWvFactionAttitude::Friendly;
			}
		}
		else if (FactionReputation <= -60.0f)
		{
			ResultAttitude = EWvFactionAttitude::Hostile;
		}

		return ResultAttitude;
	}

	return ResultAttitude;
}

bool UWvFactionRelationshipSubsystem::IsPlayerControlledActor(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn)
	{
		return false;
	}

	return Pawn->IsPlayerControlled();
}

EWvFactionAttitude UWvFactionRelationshipSubsystem::ApplyRelationshipMemory(EWvFactionAttitude CurrentAttitude, const AActor* SourceActor, const AActor* TargetActor) const
{
	const UWvRelationshipComponent* RelationshipComp = GetRelationshipComponent(SourceActor);
	if (!RelationshipComp)
	{
		return CurrentAttitude;
	}

	const EWvFactionAttitude MemoryAttitude = RelationshipComp->GetMemoryAttitudeFor(const_cast<AActor*>(TargetActor));

	return ChooseStrongerAttitude(CurrentAttitude, MemoryAttitude);
}

EWvFactionAttitude UWvFactionRelationshipSubsystem::ChooseStrongerAttitude(EWvFactionAttitude A, EWvFactionAttitude B) const
{
	auto Score = [](EWvFactionAttitude Attitude) -> int32
		{
			switch (Attitude)
			{
			case EWvFactionAttitude::Ignore:
				return 0;
			case EWvFactionAttitude::Neutral:
				return 1;
			case EWvFactionAttitude::Friendly:
				return 2;
			case EWvFactionAttitude::Suspicious:
				return 3;
			case EWvFactionAttitude::Fear:
				return 4;
			case EWvFactionAttitude::Hostile:
				return 5;
			default:
				return 1;
			}
		};

	return Score(B) > Score(A) ? B : A;
}

const UWvFactionComponent* UWvFactionRelationshipSubsystem::GetFactionComponent(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UWvFactionComponent>();
}

const UWvRelationshipComponent* UWvFactionRelationshipSubsystem::GetRelationshipComponent(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	const UWvRelationshipComponent* RelationshipComp = Actor->FindComponentByClass<UWvRelationshipComponent>();

	if (!RelationshipComp || !RelationshipComp->bRelationshipEnabled)
	{
		return nullptr;
	}

	return RelationshipComp;
}

const UWvPlayerReputationComponent* UWvFactionRelationshipSubsystem::GetPlayerReputationComponentFromActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn)
	{
		return Actor->FindComponentByClass<UWvPlayerReputationComponent>();
	}

	const ABasePlayerState* BasePlayerState = Pawn->GetPlayerState<ABasePlayerState>();
	if (BasePlayerState)
	{
		return BasePlayerState->GetPlayerReputationComponent();
	}

	const AController* Controller = Pawn->GetController();
	if (Controller)
	{
		const ABasePlayerState* ControllerPlayerState = Controller->GetPlayerState<ABasePlayerState>();
		if (ControllerPlayerState)
		{
			return ControllerPlayerState->GetPlayerReputationComponent();
		}
	}

	return nullptr;
}

