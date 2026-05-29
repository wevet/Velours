// Copyright 2022 wevet works All Rights Reserved.


#include "Game/WvGameInstance.h"
#include "Engine.h"
#include "Velours.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGameSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvGameInstance)

FStreamableManager UWvGameInstance::StreamableManager;

const FString UWvGameInstance::PlayerSlotName = TEXT("PlayerSaveSlot");

void UWvGameInstance::Init()
{
	Super::Init();
	Load();
}

void UWvGameInstance::Shutdown()
{
	Save();

	Super::Shutdown();
}

bool UWvGameInstance::Save()
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(SaveGame, PlayerSlotName, SaveSlotID);
}

bool UWvGameInstance::Load()
{
	CurrentSaveGame = nullptr;

	if (UGameplayStatics::DoesSaveGameExist(PlayerSlotName, SaveSlotID))
	{
		CurrentSaveGame = Cast<UWvSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerSlotName, SaveSlotID));
	}
	else
	{
		CurrentSaveGame = Cast<UWvSaveGame>(UGameplayStatics::CreateSaveGameObject(UWvSaveGame::StaticClass()));
	}
	return IsValid(CurrentSaveGame);
}


UWvGameInstance* UWvGameInstance::GetGameInstance()
{
	UWvGameInstance* Instance = nullptr;
	if (GEngine != nullptr)
	{
		FWorldContext* Context = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
		Instance = Cast<UWvGameInstance>(Context->OwningGameInstance);
	}
	return Instance;
}

FStreamableManager& UWvGameInstance::GetStreamableManager()
{
	return StreamableManager;
}

UWvSaveGame* UWvGameInstance::GetOrCreateWvSaveGame()
{
	if (IsValid(CurrentSaveGame))
	{
		return CurrentSaveGame;
	}

	if (UGameplayStatics::DoesSaveGameExist(PlayerSlotName, SaveSlotID))
	{
		CurrentSaveGame = Cast<UWvSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerSlotName, SaveSlotID));
	}
	else
	{
		CurrentSaveGame = Cast<UWvSaveGame>(UGameplayStatics::CreateSaveGameObject(UWvSaveGame::StaticClass()));
	}
	return CurrentSaveGame;
}


const bool UWvGameInstance::IsGameClear()
{
	UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	if (IsValid(WvSaveGame))
	{
		return WvSaveGame->IsGameClear();
	}
	return false;
}

#pragma region Mission
const UMissionGameDataAsset* UWvGameInstance::GetMasterMissionGameDataAsset() const
{
	return MasterMissionGameDataAsset;
}

const FMissionBaseData* UWvGameInstance::FindMissionDefinitionPtr(const int32 MissionId) const
{
	if (!IsValid(MasterMissionGameDataAsset))
	{
		return nullptr;
	}

	return MasterMissionGameDataAsset->FindMission(MissionId);
}

bool UWvGameInstance::FindMissionDefinition(const int32 MissionId, FMissionBaseData& OutMissionData) const
{
	const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId);
	if (!MissionDef)
	{
		return false;
	}

	OutMissionData = *MissionDef;
	return true;
}

TArray<FMissionBaseData> UWvGameInstance::FindMissionDefinitionsByIds(const TArray<int32>& MissionIds) const
{
	TArray<FMissionBaseData> Result;

	for (const int32 MissionId : MissionIds)
	{
		if (const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId))
		{
			Result.Add(*MissionDef);
		}
	}

	return Result;
}

bool UWvGameInstance::TryAcceptMissionById(const int32 MissionId)
{
	const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId);
	if (!MissionDef)
	{
		return false;
	}

	return TryAcceptMissionFromData(*MissionDef);
}

bool UWvGameInstance::CanAcceptMissionById(const int32 MissionId) const
{
	const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId);
	if (!MissionDef)
	{
		return false;
	}

	return CanAcceptMissionFromData(*MissionDef);
}

TArray<FMissionBaseData> UWvGameInstance::GetAcceptableMissionsByIds(const TArray<int32>& MissionIds) const
{
	TArray<FMissionBaseData> Result;

	for (const int32 MissionId : MissionIds)
	{
		const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId);
		if (!MissionDef)
		{
			continue;
		}

		if (CanAcceptMissionFromData(*MissionDef))
		{
			Result.Add(*MissionDef);
		}
	}

	return Result;
}

bool UWvGameInstance::TryAcceptMissionFromData(const FMissionBaseData& MissionDef)
{
	if (!CanAcceptMissionFromData(MissionDef))
	{
		return false;
	}

	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->AcceptMission(MissionDef))
	{
		return false;
	}

	return Save();
}

bool UWvGameInstance::CanAcceptMissionFromData(const FMissionBaseData& MissionDef) const
{
	if (!MissionDef.IsValid())
	{
		return false;
	}

	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (SaveGame->HasMission(MissionDef.MissionId))
	{
		return false;
	}

	if (MissionDef.ParentMissionId != INDEX_NONE)
	{
		if (!SaveGame->HasCompletedMission(MissionDef.ParentMissionId))
		{
			return false;
		}
	}

	for (const int32 RequiredMissionId : MissionDef.RequiredCompletedMissionIds)
	{
		if (!SaveGame->HasCompletedMission(RequiredMissionId))
		{
			return false;
		}
	}

	return true;
}

bool UWvGameInstance::BeginMission(const int32 MissionId)
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->BeginMission(MissionId))
	{
		return false;
	}

	return Save();
}

bool UWvGameInstance::InterruptMission(const int32 MissionId)
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->InterruptMission(MissionId))
	{
		return false;
	}

	return Save();
}

void UWvGameInstance::InterruptCurrentMissions()
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return;
	}

	SaveGame->InterruptCurrentMissions();
	Save();
}

bool UWvGameInstance::CompleteMission(const int32 MissionId)
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->CompleteMission(MissionId))
	{
		return false;
	}

	return Save();
}

bool UWvGameInstance::CompleteMissionPhaseById(const int32 MissionId, const int32 PhaseId)
{
	const FMissionBaseData* MissionDef = FindMissionDefinitionPtr(MissionId);
	if (!MissionDef)
	{
		return false;
	}

	return CompleteMissionPhaseFromData(*MissionDef, PhaseId);
}

bool UWvGameInstance::CompleteMissionPhaseFromData(const FMissionBaseData& MissionDef, const int32 PhaseId)
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->CompleteMissionPhase(MissionDef, PhaseId))
	{
		return false;
	}

	return Save();
}

bool UWvGameInstance::SetCurrentMissionPhase(const int32 MissionId, const int32 PhaseId)
{
	UWvSaveGame* SaveGame = GetOrCreateWvSaveGame();
	if (!IsValid(SaveGame))
	{
		return false;
	}

	if (!SaveGame->SetCurrentPhase(MissionId, PhaseId))
	{
		return false;
	}

	return Save();
}

bool UWvGameInstance::HasMission(const int32 MissionId) const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	return IsValid(SaveGame) && SaveGame->HasMission(MissionId);
}

bool UWvGameInstance::HasProgressMission(const int32 MissionId) const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	return IsValid(SaveGame) && SaveGame->HasProgressMission(MissionId);
}

bool UWvGameInstance::HasCompletedMission(const int32 MissionId) const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	return IsValid(SaveGame) && SaveGame->HasCompletedMission(MissionId);
}

bool UWvGameInstance::HasInterruptedMission(const int32 MissionId) const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	return IsValid(SaveGame) && SaveGame->HasInterruptedMission(MissionId);
}

bool UWvGameInstance::LoadMissionSaveData(
	const int32 MissionId,
	FMissionSaveData& OutSaveData) const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return false;
	}

	return SaveGame->LoadMissionSaveData(MissionId, OutSaveData);
}

TArray<FMissionSaveData> UWvGameInstance::GetProgressMissions() const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return TArray<FMissionSaveData>();
	}

	return SaveGame->GetProgressMissions();
}

TArray<FMissionSaveData> UWvGameInstance::GetCompletedMissions() const
{
	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return TArray<FMissionSaveData>();
	}

	return SaveGame->GetCompletedMissions();
}

#pragma endregion


#pragma region MissionUI
TArray<FMissionViewData> UWvGameInstance::GetProgressMissionViewData() const
{
	TArray<FMissionViewData> Result;

	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return Result;
	}

	const TArray<FMissionSaveData> ProgressMissions = SaveGame->GetProgressMissions();

	for (const FMissionSaveData& SaveData : ProgressMissions)
	{
		FMissionViewData ViewData;
		if (BuildMissionViewData(SaveData, ViewData))
		{
			Result.Add(ViewData);
		}
	}

	return Result;
}

TArray<FMissionViewData> UWvGameInstance::GetCompletedMissionViewData() const
{
	TArray<FMissionViewData> Result;

	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return Result;
	}

	const TArray<FMissionSaveData> CompletedMissions = SaveGame->GetCompletedMissions();

	for (const FMissionSaveData& SaveData : CompletedMissions)
	{
		FMissionViewData ViewData;
		if (BuildMissionViewData(SaveData, ViewData))
		{
			Result.Add(ViewData);
		}
	}

	return Result;
}

TArray<FMissionViewData> UWvGameInstance::GetMissionViewDataByState(const EMissionState MissionState) const
{
	TArray<FMissionViewData> Result;

	const UWvSaveGame* SaveGame = CurrentSaveGame;
	if (!IsValid(SaveGame))
	{
		return Result;
	}

	for (const TPair<int32, FMissionSaveData>& Pair : SaveGame->MissionSaveMap)
	{
		const FMissionSaveData& SaveData = Pair.Value;

		if (SaveData.State != MissionState)
		{
			continue;
		}

		FMissionViewData ViewData;
		if (BuildMissionViewData(SaveData, ViewData))
		{
			Result.Add(ViewData);
		}
	}

	return Result;
}

bool UWvGameInstance::BuildMissionViewData(const FMissionSaveData& SaveData, FMissionViewData& OutViewData) const
{
	if (!SaveData.IsValid())
	{
		return false;
	}

	if (!IsValid(MasterMissionGameDataAsset))
	{
		return false;
	}

	const FMissionBaseData* MissionDef = MasterMissionGameDataAsset->FindMission(SaveData.MissionId);

	if (!MissionDef)
	{
		return false;
	}

	OutViewData.MissionId = SaveData.MissionId;
	OutViewData.Title = MissionDef->Title;
	OutViewData.Description = MissionDef->Description;
	OutViewData.State = SaveData.State;
	OutViewData.CurrentPhaseId = SaveData.CurrentPhaseId;
	OutViewData.CompletedPhaseIds = SaveData.CompletedPhaseIds;

	if (const FMissionPhase* CurrentPhase = MissionDef->FindPhaseById(SaveData.CurrentPhaseId))
	{
		OutViewData.CurrentPhaseDescription = CurrentPhase->Description;
	}
	else
	{
		OutViewData.CurrentPhaseDescription = FText::GetEmpty();
	}

	return true;
}
#pragma endregion


void UWvGameInstance::SetGameClear()
{
	UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	WvSaveGame->SetGameClear();
	UGameplayStatics::SaveGameToSlot(WvSaveGame, PlayerSlotName, SaveSlotID);
}

void UWvGameInstance::SetHour(const int32 InHour)
{
	UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	WvSaveGame->SetHour(InHour);
	UGameplayStatics::SaveGameToSlot(WvSaveGame, PlayerSlotName, SaveSlotID);
}

void UWvGameInstance::IncrementMoney(const int32 AddMoney)
{
	UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	WvSaveGame->IncrementMoney(AddMoney);
	UGameplayStatics::SaveGameToSlot(WvSaveGame, PlayerSlotName, SaveSlotID);
}

void UWvGameInstance::DecrementMoney(const int32 InMoney)
{
	UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	WvSaveGame->DecrementMoney(InMoney);
	UGameplayStatics::SaveGameToSlot(WvSaveGame, PlayerSlotName, SaveSlotID);
}

const int32 UWvGameInstance::GetHour()
{
	const UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	return WvSaveGame->GetHour();
}

const int32 UWvGameInstance::GetMoney()
{
	const UWvSaveGame* WvSaveGame = GetOrCreateWvSaveGame();
	return WvSaveGame->GetMoney();
}

void UWvGameInstance::SetSaveSlotID(const int32 NewSaveSlotID)
{
	SaveSlotID = NewSaveSlotID;
}

int32 UWvGameInstance::GetSaveSlotID()
{
	return SaveSlotID;
}


