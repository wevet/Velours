// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Game/WvSaveGame.h"
#include "WvGameInstance.generated.h"

/**
 *
 */
UCLASS()
class VELOURS_API UWvGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	bool Save();
	bool Load();


	static UWvGameInstance* GetGameInstance();
	static FStreamableManager& GetStreamableManager();

	UWvSaveGame* GetOrCreateWvSaveGame();

#pragma region MissionDefinition
	const UMissionGameDataAsset* GetMasterMissionGameDataAsset() const;
	const FMissionBaseData* FindMissionDefinitionPtr(const int32 MissionId) const;
	bool FindMissionDefinition(const int32 MissionId, FMissionBaseData& OutMissionData) const;
	TArray<FMissionBaseData> FindMissionDefinitionsByIds(const TArray<int32>& MissionIds) const;

	bool TryAcceptMissionById(const int32 MissionId);
	bool CanAcceptMissionById(const int32 MissionId) const;
	TArray<FMissionBaseData> GetAcceptableMissionsByIds(const TArray<int32>& MissionIds) const;

	bool TryAcceptMissionFromData(const FMissionBaseData& MissionDef);
	bool CanAcceptMissionFromData(const FMissionBaseData& MissionDef) const;
	bool BeginMission(const int32 MissionId);
	bool InterruptMission(const int32 MissionId);
	void InterruptCurrentMissions();
	bool CompleteMission(const int32 MissionId);

	bool CompleteMissionPhaseById(const int32 MissionId, const int32 PhaseId);
	bool CompleteMissionPhaseFromData(const FMissionBaseData& MissionDef, const int32 PhaseId);
	bool SetCurrentMissionPhase(const int32 MissionId, const int32 PhaseId);

	bool HasMission(const int32 MissionId) const;

	bool HasProgressMission(const int32 MissionId) const;

	bool HasCompletedMission(const int32 MissionId) const;

	bool HasInterruptedMission(const int32 MissionId) const;

	bool LoadMissionSaveData(const int32 MissionId, FMissionSaveData& OutSaveData) const;

	TArray<FMissionSaveData> GetProgressMissions() const;

	TArray<FMissionSaveData> GetCompletedMissions() const;
#pragma endregion

#pragma region MissionUI
	TArray<FMissionViewData> GetProgressMissionViewData() const;
	TArray<FMissionViewData> GetCompletedMissionViewData() const;
	TArray<FMissionViewData> GetMissionViewDataByState(const EMissionState MissionState) const;
	bool BuildMissionViewData(const FMissionSaveData& SaveData, FMissionViewData& OutViewData) const;
#pragma endregion


	void SetGameClear();
	const bool IsGameClear();

	void SetHour(const int32 InHour);
	const int32 GetHour();

	void IncrementMoney(const int32 AddMoney);
	void DecrementMoney(const int32 InMoney);
	const int32 GetMoney();

	void SetSaveSlotID(const int32 NewSaveSlotID);
	int32 GetSaveSlotID();

private:
	static FStreamableManager StreamableManager;

	UPROPERTY()
	int32 SaveSlotID = 0;

	static const FString PlayerSlotName;

	UPROPERTY()
	TObjectPtr<class UWvSaveGame> CurrentSaveGame = nullptr;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Mission")
	TObjectPtr<UMissionGameDataAsset> MasterMissionGameDataAsset;
};
