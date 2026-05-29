// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Engine/DataAsset.h"
#include "Mission/MissionSystemTypes.h"
#include "WvSaveGame.generated.h"


/**
 * player save data
 * wrapped mission data
 */
UCLASS()
class VELOURS_API UWvSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UWvSaveGame();

	// Player runtime mission state.
	// Key is MissionId.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<int32, FMissionSaveData> MissionSaveMap;

public:
#pragma region MissionSave
	bool HasMission(const int32 MissionId) const;

	bool HasProgressMission(const int32 MissionId) const;

	bool HasCompletedMission(const int32 MissionId) const;

	bool HasInterruptedMission(const int32 MissionId) const;

	const FMissionSaveData* FindMissionSaveData(const int32 MissionId) const;

	FMissionSaveData* FindMissionSaveDataMutable(const int32 MissionId);

	bool AcceptMission(const FMissionBaseData& MissionDef);

	bool BeginMission(const int32 MissionId);

	bool InterruptMission(const int32 MissionId);

	void InterruptCurrentMissions();

	bool CompleteMission(const int32 MissionId);

	bool CompleteMissionPhase(const FMissionBaseData& MissionDef, const int32 PhaseId);

	bool SetCurrentPhase(const int32 MissionId, const int32 PhaseId);

	bool LoadMissionSaveData(const int32 MissionId, FMissionSaveData& OutSaveData) const;

	TArray<FMissionSaveData> GetProgressMissions() const;

	TArray<FMissionSaveData> GetCompletedMissions() const;
#pragma endregion

	void SetGameClear();
	bool IsGameClear() const;
	void SetHour(const int32 InHour);
	int32 GetHour() const { return Hour; }

	void IncrementMoney(const int32 AddMoney);
	void DecrementMoney(const int32 InMoney);

	int32 GetMoney() const { return Money; }

protected:
	// player name
	UPROPERTY(VisibleAnywhere, Category = SaveGame)
	FName Name;

	// game“àŽžŠÔ
	UPROPERTY(VisibleAnywhere, Category = SaveGame)
	int32 Hour;

	UPROPERTY(VisibleAnywhere, Category = SaveGame)
	int32 Money;

	// is game cleard ?
	UPROPERTY(VisibleAnywhere, Category = SaveGame)
	bool bIsGameCompleted = false;

	UPROPERTY(VisibleAnywhere, Category = SaveGame)
	TArray<FMissionBaseData> MissionArray;

private:
	// key mission id
	// value phase id
	UPROPERTY()
	TMap<int32, int32> MissionDataMap;


	void SetMoney(const int32 InMoney);

};


