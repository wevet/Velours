// Copyright 2022 wevet works All Rights Reserved.


#include "Game/CharacterInstanceSubsystem.h"
//#include "Character/WvAIController.h"
#include "Component/WvSkeletalMeshComponent.h"
#include "Mission/MinimapMarkerComponent.h"
#include "Character/BasePawn.h"
#include "Misc/WvCommonUtils.h"
#include "Velours.h"
#include "GameExtension.h"

#include "Engine/World.h"
#include "EngineUtils.h"

// plugin
#include "IAnimationBudgetAllocator.h"
#include "SignificanceManager.h"


using namespace Game;


#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterInstanceSubsystem)


UCharacterInstanceSubsystem* UCharacterInstanceSubsystem::Instance = nullptr;

UCharacterInstanceSubsystem::UCharacterInstanceSubsystem()
{
	UCharacterInstanceSubsystem::Instance = this;
}

UCharacterInstanceSubsystem* UCharacterInstanceSubsystem::Get()
{
	return Instance;
}

void UCharacterInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

	bIsTickable = true;
}

void UCharacterInstanceSubsystem::Deinitialize()
{

	Characters.Reset(0);
}

TStatId UCharacterInstanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCharacterInstanceSubsystem, STATGROUP_Tickables);
}

bool UCharacterInstanceSubsystem::IsTickable() const
{
	return bIsTickable;
}

void UCharacterInstanceSubsystem::Tick(float DeltaTime)
{

}

void UCharacterInstanceSubsystem::WorldCharacterIterator(TArray<class ABasePawn*>& OutCharacterArray)
{
	ArrayExtension::WorldActorIterator<ABasePawn>(GetWorld(), OutCharacterArray);

	OutCharacterArray.RemoveAll([](ABasePawn* Character)
	{
		return !IsValid(Character);
	});
}

void UCharacterInstanceSubsystem::FreezeAlCharacters(bool bFindWorldActorIterator/* = false*/)
{
	if (bFindWorldActorIterator)
	{
		UpdateCharacterInWorld();
	}

	for (ABasePawn* Character : Characters)
	{
		if (IsValid(Character))
		{
			Character->Freeze();
		}
	}
}

void UCharacterInstanceSubsystem::UnFreezeAlCharacters(bool bFindWorldActorIterator/* = false*/)
{
	if (bFindWorldActorIterator)
	{
		UpdateCharacterInWorld();
	}

	for (ABasePawn* Character : Characters)
	{
		if (IsValid(Character))
		{
			Character->UnFreeze();
		}
	}
}

void UCharacterInstanceSubsystem::DoForceKill(bool bFindWorldActorIterator/* = false*/)
{
	if (bFindWorldActorIterator)
	{
		UpdateCharacterInWorld();
	}

	for (ABasePawn* Character : Characters)
	{
		if (IsValid(Character) && !Character->IsDead())
		{
			Character->DoKill(true);
		}
	}
}

void UCharacterInstanceSubsystem::DoForceKillIgnorePlayer(bool bFindWorldActorIterator/* = false*/)
{
	if (bFindWorldActorIterator)
	{
		UpdateCharacterInWorld();
	}

	for (ABasePawn* Character : Characters)
	{
		if (IsValid(Character) && Character->IsBotCharacter())
		{
			Character->DoKill(true);
		}
	}
}

void UCharacterInstanceSubsystem::AssignAICharacter(ABasePawn* NewCharacter)
{
	if (IsValid(NewCharacter))
	{
		if (!Characters.Contains(NewCharacter))
		{
			Characters.Add(NewCharacter);
		}
	}


}

void UCharacterInstanceSubsystem::RemoveAICharacter(ABasePawn* InCharacter)
{
	if (IsValid(InCharacter))
	{
		if (Characters.Contains(InCharacter))
		{
			Characters.Remove(InCharacter);
		}
	}

}

TArray<UWvSkeletalMeshComponent*> UCharacterInstanceSubsystem::GetSkelMeshComponents(const ABasePawn* InCharacter) const
{
	auto Components = ComponentExtension::GetComponentsArray<UWvSkeletalMeshComponent>(InCharacter);
	Components.RemoveAll([](UWvSkeletalMeshComponent* SkelMesh)
	{
		return SkelMesh == nullptr;
	});

	return Components;
}

bool UCharacterInstanceSubsystem::IsInEnemyAgent(const ABasePawn* Other) const
{
	//if (AWvAIController* AICtrl = Cast<AWvAIController>(Other->GetController()))
	//{
	//	return AICtrl->IsInEnemyAgent(*Other);
	//}
	return false;
}

bool UCharacterInstanceSubsystem::IsInFriendAgent(const ABasePawn* Other) const
{
	//if (AWvAIController* AICtrl = Cast<AWvAIController>(Other->GetController()))
	//{
	//	return AICtrl->IsInFriendAgent(*Other);
	//}
	return false;
}

bool UCharacterInstanceSubsystem::IsInNeutralAgent(const ABasePawn* Other) const
{
	//if (AWvAIController* AICtrl = Cast<AWvAIController>(Other->GetController()))
	//{
	//	return AICtrl->IsInNeutralAgent(*Other);
	//}
	return true;
}

bool UCharacterInstanceSubsystem::IsInBattleAny() const
{
	for (const ABasePawn* Character : Characters)
	{
		if (Character->IsInBattled())
		{
			return true;
		}
	}
	return false;
}

TArray<ABasePawn*> UCharacterInstanceSubsystem::GetLeaderAgent() const
{
	TArray<ABasePawn*> Result;
	for (auto Character : Characters)
	{
		if (IsValid(Character) && Character->IsLeader())
		{
			Result.Add(Character);
		}
	}
	return Result;
}

TArray<ABasePawn*> UCharacterInstanceSubsystem::GetIgnorePlayerArray() const
{
	TArray<ABasePawn*> Result;
	for (auto Character : Characters)
	{
		if (IsValid(Character) && Character->IsBotCharacter())
		{
			Result.Add(Character);
		}
	}
	return Result;
}

void UCharacterInstanceSubsystem::GeneratorSpawnedFinish()
{
}

void UCharacterInstanceSubsystem::UpdateCharacterInWorld()
{
	TArray<ABasePawn*> Array;
	WorldCharacterIterator(Array);
	Characters += Array;

	Characters.RemoveAll([](ABasePawn* Character)
	{
		return IsValid(Character) == false;
	});
}

void UCharacterInstanceSubsystem::StartCinematicCharacter(ABasePawn* InCharacter)
{
	RemoveAICharacter(InCharacter);

	if (IsValid(InCharacter))
	{
		InCharacter->DoStartCinematic();
	}
}

void UCharacterInstanceSubsystem::StopCinematicCharacter(ABasePawn* InCharacter)
{
	AssignAICharacter(InCharacter);

	if (IsValid(InCharacter))
	{
		InCharacter->DoStopCinematic();
	}
}


TArray<ABasePawn*> UCharacterInstanceSubsystem::GetPOIActors() const
{

	TArray<ABasePawn*> Filtered;
	ArrayExtension::FilterArray(Characters, Filtered, [](const ABasePawn* Char)
	{
		if (const auto* Marker = Char->GetMinimapMarkerComponent())
		{
			return Marker->IsVisibleMakerTag();
		}
		return false;
	});

	return Filtered;
}



