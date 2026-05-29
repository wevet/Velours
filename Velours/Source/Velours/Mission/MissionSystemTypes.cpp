// Copyright 2022 wevet works All Rights Reserved.

#include "MissionSystemTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Algo/AllOf.h"

DEFINE_LOG_CATEGORY(LogMission)

#include UE_INLINE_GENERATED_CPP_BY_NAME(MissionSystemTypes)

FMissionPhase::FMissionPhase()
{
	PhaseId = INDEX_NONE;
	Description = FText::GetEmpty();
	bIsFinalPhase = false;
}

bool FMissionPhase::IsValid() const
{
	return PhaseId != INDEX_NONE;
}

FMissionBaseData::FMissionBaseData()
{
	MissionId = INDEX_NONE;
	ParentMissionId = INDEX_NONE;
	Title = FText::GetEmpty();
	Description = FText::GetEmpty();
}

bool FMissionBaseData::IsValid() const
{
	return MissionId != INDEX_NONE;
}

bool FMissionBaseData::IsSubMission() const
{
	return ParentMissionId != INDEX_NONE;
}

const FMissionPhase* FMissionBaseData::FindPhaseById(const int32 PhaseId) const
{
	return MissionPhases.FindByPredicate([PhaseId](const FMissionPhase& Item)
	{
		return Item.PhaseId == PhaseId;
	});
}

const FMissionPhase* FMissionBaseData::GetFirstPhase() const
{
	if (MissionPhases.Num() <= 0)
	{
		return nullptr;
	}

	return &MissionPhases[0];
}

const FMissionPhase* FMissionBaseData::GetNextIncompletePhase(const TArray<int32>& CompletedPhaseIds) const
{
	for (const FMissionPhase& Phase : MissionPhases)
	{
		if (!CompletedPhaseIds.Contains(Phase.PhaseId))
		{
			return &Phase;
		}
	}

	return nullptr;
}

FMissionSaveData::FMissionSaveData()
{
	MissionId = INDEX_NONE;
	State = EMissionState::None;
	CurrentPhaseId = INDEX_NONE;
	CompletedPhaseIds.Reset();
}

bool FMissionSaveData::IsValid() const
{
	return MissionId != INDEX_NONE;
}

bool FMissionSaveData::HasCompletedPhase(const int32 PhaseId) const
{
	return CompletedPhaseIds.Contains(PhaseId);
}

bool FMissionSaveData::IsProgress() const
{
	return State == EMissionState::Progress;
}

bool FMissionSaveData::IsCompleted() const
{
	return State == EMissionState::Completed;
}

bool FMissionSaveData::IsInterrupted() const
{
	return State == EMissionState::Interrupted;
}


FMissionViewData::FMissionViewData()
{
	MissionId = INDEX_NONE;
	Title = FText::GetEmpty();
	Description = FText::GetEmpty();
	State = EMissionState::None;
	CurrentPhaseId = INDEX_NONE;
	CurrentPhaseDescription = FText::GetEmpty();
	CompletedPhaseIds.Reset();
}


const FMissionBaseData* UMissionGameDataAsset::FindMission(const int32 MissionId) const
{
	return MissionGameMap.Find(MissionId);
}

bool UMissionGameDataAsset::FindMissionData(const int32 MissionId, FMissionBaseData& OutMissionData) const
{
	const FMissionBaseData* FoundMission = FindMission(MissionId);
	if (!FoundMission)
	{
		return false;
	}

	OutMissionData = *FoundMission;
	return true;
}

bool UMissionGameDataAsset::HasMission(const int32 MissionId) const
{
	return MissionGameMap.Contains(MissionId);
}

TArray<int32> UMissionGameDataAsset::GetMissionIds() const
{
	TArray<int32> Result;
	MissionGameMap.GetKeys(Result);
	return Result;
}

TArray<FMissionBaseData> UMissionGameDataAsset::GetAllMissionData() const
{
	TArray<FMissionBaseData> Result;
	MissionGameMap.GenerateValueArray(Result);
	return Result;
}

