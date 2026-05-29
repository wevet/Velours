// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Mission/MissionSystemTypes.h"
#include "AIMissionComponent.generated.h"

class UMissionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSendRegisterMissionDelegate, int32, InSendMissionIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMissionAllCompleteDelegate, bool, bMissionAllCompleteCutScene);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UAIMissionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIMissionComponent(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

protected:
	// NPC / AI / Character / Scenario side mission definitions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<UMissionGameDataAsset> SendMissionGameDataAsset;

	// This NPC can offer these mission ids.
	// Mission definitions are resolved from MasterMissionGameDataAsset in GameInstance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<int32> OfferMissionIds;

	// Optional selected mission.
	// If INDEX_NONE, RegisterMission() will try the first acceptable mission from OfferMissionIds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 SelectedMissionId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bAllowRegisterMission = true;

public:
	UPROPERTY(BlueprintAssignable)
	FSendRegisterMissionDelegate RegisterMissionDelegate;

	UPROPERTY(BlueprintAssignable)
	FMissionAllCompleteDelegate MissionAllCompleteDelegate;

public:
	void RegisterMission();

	void RegisterMission(const int32 MissionId);

	void SetAllowRegisterMission(const bool bNewAllowRegisterMission);

	bool GetAllowRegisterMission() const;

	void SetSendMissionData(const int32 InSendMissionId);

	bool HasMissionData() const;

	bool HasMission(const int32 MissionId) const;

	bool HasAcceptableMission() const;

	bool CanOfferMission(const int32 MissionId) const;

	bool HasMissionAllComplete() const;

	bool HasMissionComplete(const int32 MissionId) const;

	TArray<FMissionBaseData> GetAcceptableMissions() const;

	const TArray<int32>& GetOfferMissionIds() const;

private:
	UMissionComponent* GetPlayerMissionComponent() const;
};

