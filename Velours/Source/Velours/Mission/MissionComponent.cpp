// Copyright 2022 wevet works All Rights Reserved.

#include "Mission/MissionComponent.h"
#include "Game/WvGameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MissionComponent)

UMissionComponent::UMissionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMissionComponent::BeginPlay()
{
	Super::BeginPlay();
	Super::SetComponentTickEnabled(false);
}

void UMissionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}


bool UMissionComponent::TryAcceptMissionById(const int32 MissionId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	const bool bAccepted = GameInstance->TryAcceptMissionById(MissionId);

	if (bAccepted)
	{
		RegisterMissionDelegate.Broadcast(MissionId);
		BeginMissionDelegate.Broadcast(MissionId);
	}

	return bAccepted;
}

bool UMissionComponent::CanAcceptMissionById(const int32 MissionId) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->CanAcceptMissionById(MissionId);
}

bool UMissionComponent::FindMissionDefinition(const int32 MissionId, FMissionBaseData& OutMissionData) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->FindMissionDefinition(MissionId, OutMissionData);
}

TArray<FMissionBaseData> UMissionComponent::GetAcceptableMissionsByIds(const TArray<int32>& MissionIds) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionBaseData>();
	}

	return GameInstance->GetAcceptableMissionsByIds(MissionIds);
}

TArray<FMissionBaseData> UMissionComponent::FindMissionDefinitionsByIds(const TArray<int32>& MissionIds) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionBaseData>();
	}

	return GameInstance->FindMissionDefinitionsByIds(MissionIds);
}

bool UMissionComponent::BeginMission(const int32 MissionId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	const bool bResult = GameInstance->BeginMission(MissionId);

	if (bResult)
	{
		BeginMissionDelegate.Broadcast(MissionId);
	}

	return bResult;
}

bool UMissionComponent::CompleteMission(const int32 MissionId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->CompleteMission(MissionId);
}

bool UMissionComponent::InterruptMission(const int32 MissionId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->InterruptMission(MissionId);
}

void UMissionComponent::InterruptCurrentMissions()
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->InterruptCurrentMissions();
}

bool UMissionComponent::CompleteMissionPhaseById(const int32 MissionId, const int32 PhaseId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->CompleteMissionPhaseById(MissionId, PhaseId);
}

bool UMissionComponent::SetCurrentMissionPhase(const int32 MissionId, const int32 PhaseId)
{
	UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->SetCurrentMissionPhase(MissionId, PhaseId);
}

bool UMissionComponent::HasMission(const int32 MissionId) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	return IsValid(GameInstance) && GameInstance->HasMission(MissionId);
}

bool UMissionComponent::HasProgressMission(const int32 MissionId) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	return IsValid(GameInstance) && GameInstance->HasProgressMission(MissionId);
}

bool UMissionComponent::HasCompleteMission(const int32 MissionId) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	return IsValid(GameInstance) && GameInstance->HasCompletedMission(MissionId);
}

bool UMissionComponent::HasInterruptedMission(const int32 MissionId) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	return IsValid(GameInstance) && GameInstance->HasInterruptedMission(MissionId);
}

bool UMissionComponent::LoadMissionSaveData(const int32 MissionId, FMissionSaveData& OutSaveData) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	return GameInstance->LoadMissionSaveData(MissionId, OutSaveData);
}

TArray<FMissionSaveData> UMissionComponent::GetProgressMissions() const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionSaveData>();
	}

	return GameInstance->GetProgressMissions();
}

TArray<FMissionSaveData> UMissionComponent::GetCompletedMissions() const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionSaveData>();
	}

	return GameInstance->GetCompletedMissions();
}

TArray<FMissionViewData> UMissionComponent::GetProgressMissionViewData() const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionViewData>();
	}

	return GameInstance->GetProgressMissionViewData();
}

TArray<FMissionViewData> UMissionComponent::GetCompletedMissionViewData() const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionViewData>();
	}

	return GameInstance->GetCompletedMissionViewData();
}

TArray<FMissionViewData> UMissionComponent::GetMissionViewDataByState(const EMissionState MissionState) const
{
	const UWvGameInstance* GameInstance = UWvGameInstance::GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return TArray<FMissionViewData>();
	}

	return GameInstance->GetMissionViewDataByState(MissionState);
}


