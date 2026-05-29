// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/WvSaveGame.h"
#include "Mission/MissionSystemTypes.h"
#include "MissionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReceiveRegisterMissionDelegate, int32, InSendMissionIndex);

UCLASS( ClassGroup=(Mission), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UMissionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMissionComponent(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;


public:
	UPROPERTY(BlueprintAssignable)
	FReceiveRegisterMissionDelegate RegisterMissionDelegate;

	UPROPERTY(BlueprintAssignable)
	FReceiveRegisterMissionDelegate BeginMissionDelegate;


public:
	bool TryAcceptMissionById(const int32 MissionId);

	bool CanAcceptMissionById(const int32 MissionId) const;

	TArray<FMissionBaseData> GetAcceptableMissionsByIds(const TArray<int32>& MissionIds) const;

	bool FindMissionDefinition(const int32 MissionId, FMissionBaseData& OutMissionData) const;

	TArray<FMissionBaseData> FindMissionDefinitionsByIds(const TArray<int32>& MissionIds) const;

	bool BeginMission(const int32 MissionId);

	bool CompleteMission(const int32 MissionId);

	bool InterruptMission(const int32 MissionId);

	void InterruptCurrentMissions();

	bool CompleteMissionPhaseById(const int32 MissionId, const int32 PhaseId);

	bool SetCurrentMissionPhase(const int32 MissionId, const int32 PhaseId);

	bool HasMission(const int32 MissionId) const;

	bool HasProgressMission(const int32 MissionId) const;

	bool HasCompleteMission(const int32 MissionId) const;

	bool HasInterruptedMission(const int32 MissionId) const;

	bool LoadMissionSaveData(const int32 MissionId, FMissionSaveData& OutSaveData) const;

	TArray<FMissionSaveData> GetProgressMissions() const;

	TArray<FMissionSaveData> GetCompletedMissions() const;

	TArray<FMissionViewData> GetProgressMissionViewData() const;
	TArray<FMissionViewData> GetCompletedMissionViewData() const;
	TArray<FMissionViewData> GetMissionViewDataByState(const EMissionState MissionState) const;

};

