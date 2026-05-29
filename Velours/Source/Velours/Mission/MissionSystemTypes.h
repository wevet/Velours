// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Engine/DataAsset.h"
#include "Logging/LogMacros.h"
#include "MissionSystemTypes.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogMission, Log, All)

UENUM(BlueprintType)
enum class EMissionState : uint8
{
	None		UMETA(DisplayName = "None"),
	Available	UMETA(DisplayName = "Available"),
	Progress	UMETA(DisplayName = "Progress"),
	Interrupted	UMETA(DisplayName = "Interrupted"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed"),
};

USTRUCT(BlueprintType)
struct VELOURS_API FMissionPhase
{
	GENERATED_BODY()

public:
	FMissionPhase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PhaseId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsFinalPhase = false;

	bool IsValid() const;
};


USTRUCT(BlueprintType)
struct VELOURS_API FMissionBaseData
{
	GENERATED_BODY()

public:
	FMissionBaseData();

	// Mission unique id
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MissionId = INDEX_NONE;

	// Parent mission id.
	// INDEX_NONE means this mission is a main mission.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ParentMissionId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	// Other missions that must be completed before accepting this mission.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<int32> RequiredCompletedMissionIds;

	// Phase definitions.
	// Runtime completion status should not be stored here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionPhase> MissionPhases;

	bool IsValid() const;

	bool IsSubMission() const;

	const FMissionPhase* FindPhaseById(const int32 PhaseId) const;

	const FMissionPhase* GetFirstPhase() const;

	const FMissionPhase* GetNextIncompletePhase(const TArray<int32>& CompletedPhaseIds) const;
};

/*
* player save data
*/
USTRUCT(BlueprintType)
struct VELOURS_API FMissionSaveData
{
	GENERATED_BODY()

public:
	FMissionSaveData();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 MissionId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EMissionState State = EMissionState::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CurrentPhaseId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<int32> CompletedPhaseIds;

	bool IsValid() const;

	bool HasCompletedPhase(const int32 PhaseId) const;

	bool IsProgress() const;

	bool IsCompleted() const;

	bool IsInterrupted() const;
};


/*
* Mission UI struct
*/
USTRUCT(BlueprintType)
struct VELOURS_API FMissionViewData
{
	GENERATED_BODY()

public:
	FMissionViewData();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 MissionId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EMissionState State = EMissionState::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CurrentPhaseId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText CurrentPhaseDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<int32> CompletedPhaseIds;
};

UCLASS(BlueprintType)
class VELOURS_API UMissionGameDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Key should be same as FMissionBaseData::MissionId.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<int32, FMissionBaseData> MissionGameMap;

public:
	const FMissionBaseData* FindMission(const int32 MissionId) const;

	UFUNCTION(BlueprintCallable)
	bool FindMissionData(const int32 MissionId, FMissionBaseData& OutMissionData) const;

	UFUNCTION(BlueprintCallable)
	bool HasMission(const int32 MissionId) const;

	UFUNCTION(BlueprintCallable)
	TArray<int32> GetMissionIds() const;

	UFUNCTION(BlueprintCallable)
	TArray<FMissionBaseData> GetAllMissionData() const;
};


