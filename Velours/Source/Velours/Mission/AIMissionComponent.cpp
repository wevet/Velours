// Copyright 2022 wevet works All Rights Reserved.


#include "Mission/AIMissionComponent.h"
#include "Velours.h"
//#include "GameExtension.h"
#include "Misc/WvCommonUtils.h"
//#include "Character/WvPlayerController.h"
#include "Mission/MissionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AIMissionComponent)


UAIMissionComponent::UAIMissionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAIMissionComponent::BeginPlay()
{
	Super::BeginPlay();
	Super::SetComponentTickEnabled(false);
}

void UAIMissionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

UMissionComponent* UAIMissionComponent::GetPlayerMissionComponent() const
{
	//const AWvPlayerController* PC = Cast<AWvPlayerController>(Game::ControllerExtension::GetPlayer(GetWorld()));

	//if (!IsValid(PC))
	//{
	//	return nullptr;
	//}

	//return PC->GetMissionComponent();

	return nullptr;
}

void UAIMissionComponent::SetSendMissionData(const int32 InSendMissionId)
{
	SelectedMissionId = InSendMissionId;

	if (!OfferMissionIds.Contains(InSendMissionId))
	{
		OfferMissionIds.Add(InSendMissionId);
	}

	UE_LOG(LogMission, Log, TEXT("Assign Mission Id: [%d], Function: [%s]"), InSendMissionId, *FString(__FUNCTION__));
}

bool UAIMissionComponent::GetAllowRegisterMission() const
{
	return bAllowRegisterMission;
}

void UAIMissionComponent::SetAllowRegisterMission(const bool bNewAllowRegisterMission)
{
	bAllowRegisterMission = bNewAllowRegisterMission;
}

bool UAIMissionComponent::HasMissionData() const
{
	return OfferMissionIds.Num() > 0;
}

bool UAIMissionComponent::HasMission(const int32 MissionId) const
{
	return OfferMissionIds.Contains(MissionId);
}

bool UAIMissionComponent::HasAcceptableMission() const
{
	const UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return false;
	}

	const TArray<FMissionBaseData> AcceptableMissions = MissionComponent->GetAcceptableMissionsByIds(OfferMissionIds);

	return AcceptableMissions.Num() > 0;
}

bool UAIMissionComponent::CanOfferMission(const int32 MissionId) const
{
	if (!OfferMissionIds.Contains(MissionId))
	{
		return false;
	}

	const UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return false;
	}

	return MissionComponent->CanAcceptMissionById(MissionId);
}

TArray<FMissionBaseData> UAIMissionComponent::GetAcceptableMissions() const
{
	const UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return TArray<FMissionBaseData>();
	}

	return MissionComponent->GetAcceptableMissionsByIds(OfferMissionIds);
}

const TArray<int32>& UAIMissionComponent::GetOfferMissionIds() const
{
	return OfferMissionIds;
}

void UAIMissionComponent::RegisterMission()
{
	if (!bAllowRegisterMission)
	{
		UE_LOG(LogMission, Warning, TEXT("This owner does not allow mission registration. Owner: [%s], Function: [%s]"), *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
		return;
	}

	if (!HasMissionData())
	{
		UE_LOG(LogMission, Warning, TEXT("This owner has no mission ids. Owner: [%s], Function: [%s]"), *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
		return;
	}

	UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return;
	}

	if (HasMissionAllComplete())
	{
		MissionAllCompleteDelegate.Broadcast(true);
		return;
	}

	if (SelectedMissionId != INDEX_NONE)
	{
		RegisterMission(SelectedMissionId);
		return;
	}

	const TArray<FMissionBaseData> AcceptableMissions = MissionComponent->GetAcceptableMissionsByIds(OfferMissionIds);

	if (AcceptableMissions.Num() <= 0)
	{
		MissionAllCompleteDelegate.Broadcast(true);
		return;
	}

	const FMissionBaseData& MissionDef = AcceptableMissions[0];

	const bool bAccepted = MissionComponent->TryAcceptMissionById(MissionDef.MissionId);

	if (bAccepted)
	{
		RegisterMissionDelegate.Broadcast(MissionDef.MissionId);

		UE_LOG(LogMission, Log, TEXT("Mission Registered. MissionId: [%d], Function: [%s]"), MissionDef.MissionId, *FString(__FUNCTION__));
	}
}

void UAIMissionComponent::RegisterMission(const int32 MissionId)
{
	if (!bAllowRegisterMission)
	{
		UE_LOG(LogMission, Warning, TEXT("This owner does not allow mission registration. Owner: [%s], Function: [%s]"), *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
		return;
	}

	if (!OfferMissionIds.Contains(MissionId))
	{
		UE_LOG(LogMission, Warning, TEXT("This owner does not offer mission. MissionId: [%d], Owner: [%s], Function: [%s]"), MissionId, *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
		return;
	}

	UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return;
	}

	if (!MissionComponent->CanAcceptMissionById(MissionId))
	{
		UE_LOG(LogMission, Log, TEXT("Mission cannot be accepted. MissionId: [%d], Function: [%s]"), MissionId, *FString(__FUNCTION__));
		return;
	}

	const bool bAccepted = MissionComponent->TryAcceptMissionById(MissionId);

	if (bAccepted)
	{
		RegisterMissionDelegate.Broadcast(MissionId);
		UE_LOG(LogMission, Log, TEXT("Mission Registered. MissionId: [%d], Function: [%s]"), MissionId, *FString(__FUNCTION__));
	}
}

bool UAIMissionComponent::HasMissionAllComplete() const
{
	if (OfferMissionIds.Num() <= 0)
	{
		return false;
	}

	const UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return false;
	}

	for (const int32 MissionId : OfferMissionIds)
	{
		if (!MissionComponent->HasCompleteMission(MissionId))
		{
			return false;
		}
	}

	return true;
}

bool UAIMissionComponent::HasMissionComplete(const int32 MissionId) const
{
	const UMissionComponent* MissionComponent = GetPlayerMissionComponent();
	if (!IsValid(MissionComponent))
	{
		return false;
	}

	return MissionComponent->HasCompleteMission(MissionId);
}


