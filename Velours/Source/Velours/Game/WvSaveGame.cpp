// Copyright 2022 wevet works All Rights Reserved.


#include "Game/WvSaveGame.h"
#include "Engine.h"



UWvSaveGame::UWvSaveGame() : Super()
{
	Hour = 0;
}

bool UWvSaveGame::HasMission(const int32 MissionId) const
{
	return MissionSaveMap.Contains(MissionId);
}

bool UWvSaveGame::HasProgressMission(const int32 MissionId) const
{
	const FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	return SaveData && SaveData->State == EMissionState::Progress;
}

bool UWvSaveGame::HasCompletedMission(const int32 MissionId) const
{
	const FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	return SaveData && SaveData->State == EMissionState::Completed;
}

bool UWvSaveGame::HasInterruptedMission(const int32 MissionId) const
{
	const FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	return SaveData && SaveData->State == EMissionState::Interrupted;
}

const FMissionSaveData* UWvSaveGame::FindMissionSaveData(const int32 MissionId) const
{
	return MissionSaveMap.Find(MissionId);
}

FMissionSaveData* UWvSaveGame::FindMissionSaveDataMutable(const int32 MissionId)
{
	return MissionSaveMap.Find(MissionId);
}

bool UWvSaveGame::AcceptMission(const FMissionBaseData& MissionDef)
{
	if (!MissionDef.IsValid())
	{
		return false;
	}

	if (HasMission(MissionDef.MissionId))
	{
		return false;
	}

	FMissionSaveData NewSaveData;
	NewSaveData.MissionId = MissionDef.MissionId;
	NewSaveData.State = EMissionState::Progress;
	NewSaveData.CompletedPhaseIds.Reset();

	if (const FMissionPhase* FirstPhase = MissionDef.GetFirstPhase())
	{
		NewSaveData.CurrentPhaseId = FirstPhase->PhaseId;
	}
	else
	{
		NewSaveData.CurrentPhaseId = INDEX_NONE;
	}

	MissionSaveMap.Add(MissionDef.MissionId, NewSaveData);
	return true;
}

bool UWvSaveGame::BeginMission(const int32 MissionId)
{
	FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	if (!SaveData)
	{
		return false;
	}

	if (SaveData->State == EMissionState::Completed)
	{
		return false;
	}

	SaveData->State = EMissionState::Progress;
	return true;
}

bool UWvSaveGame::InterruptMission(const int32 MissionId)
{
	FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	if (!SaveData)
	{
		return false;
	}

	if (SaveData->State != EMissionState::Progress)
	{
		return false;
	}

	SaveData->State = EMissionState::Interrupted;
	return true;
}

void UWvSaveGame::InterruptCurrentMissions()
{
	for (TPair<int32, FMissionSaveData>& Pair : MissionSaveMap)
	{
		FMissionSaveData& SaveData = Pair.Value;

		if (SaveData.State == EMissionState::Progress)
		{
			SaveData.State = EMissionState::Interrupted;
		}
	}
}

bool UWvSaveGame::CompleteMission(const int32 MissionId)
{
	FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	if (!SaveData)
	{
		return false;
	}

	SaveData->State = EMissionState::Completed;
	SaveData->CurrentPhaseId = INDEX_NONE;
	return true;
}

bool UWvSaveGame::CompleteMissionPhase(const FMissionBaseData& MissionDef, const int32 PhaseId)
{
	if (!MissionDef.IsValid())
	{
		return false;
	}

	FMissionSaveData* SaveData = MissionSaveMap.Find(MissionDef.MissionId);
	if (!SaveData)
	{
		return false;
	}

	if (SaveData->State != EMissionState::Progress &&
		SaveData->State != EMissionState::Interrupted)
	{
		return false;
	}

	const FMissionPhase* TargetPhase = MissionDef.FindPhaseById(PhaseId);
	if (!TargetPhase)
	{
		return false;
	}

	SaveData->CompletedPhaseIds.AddUnique(PhaseId);

	const FMissionPhase* NextPhase = MissionDef.GetNextIncompletePhase(SaveData->CompletedPhaseIds);
	if (NextPhase)
	{
		SaveData->CurrentPhaseId = NextPhase->PhaseId;
		SaveData->State = EMissionState::Progress;
		return true;
	}

	SaveData->CurrentPhaseId = INDEX_NONE;
	SaveData->State = EMissionState::Completed;
	return true;
}

bool UWvSaveGame::SetCurrentPhase(const int32 MissionId, const int32 PhaseId)
{
	FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	if (!SaveData)
	{
		return false;
	}

	SaveData->CurrentPhaseId = PhaseId;
	return true;
}

bool UWvSaveGame::LoadMissionSaveData(const int32 MissionId, FMissionSaveData& OutSaveData) const
{
	const FMissionSaveData* SaveData = MissionSaveMap.Find(MissionId);
	if (!SaveData)
	{
		return false;
	}

	OutSaveData = *SaveData;
	return true;
}

TArray<FMissionSaveData> UWvSaveGame::GetProgressMissions() const
{
	TArray<FMissionSaveData> Result;

	for (const TPair<int32, FMissionSaveData>& Pair : MissionSaveMap)
	{
		if (Pair.Value.State == EMissionState::Progress || Pair.Value.State == EMissionState::Interrupted)
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

TArray<FMissionSaveData> UWvSaveGame::GetCompletedMissions() const
{
	TArray<FMissionSaveData> Result;

	for (const TPair<int32, FMissionSaveData>& Pair : MissionSaveMap)
	{
		if (Pair.Value.State == EMissionState::Completed)
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}


#pragma region Misc
void UWvSaveGame::SetGameClear()
{
	if (!bIsGameCompleted)
	{
		bIsGameCompleted = true;
	}
}

bool UWvSaveGame::IsGameClear() const
{
	return bIsGameCompleted;
}

void UWvSaveGame::SetHour(const int32 InHour)
{
	Hour = InHour;
}

void UWvSaveGame::IncrementMoney(const int32 AddMoney)
{
	auto Value = Money + AddMoney;
	SetMoney(Value);
}

void UWvSaveGame::DecrementMoney(const int32 InMoney)
{
	auto Value = Money - InMoney;
	Value = FMath::Clamp(Value, 0, INT32_MAX);
	SetMoney(Value);
}

void UWvSaveGame::SetMoney(const int32 InMoney)
{
	Money = InMoney;
}
#pragma endregion


