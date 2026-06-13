// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NativeGameplayTags.h"
#include "WvPlayerReputationComponent.generated.h"

USTRUCT(BlueprintType)
struct FWvFactionReputationEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Reputation = 0.0f;
};

USTRUCT(BlueprintType)
struct FWvRegionReputationEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag RegionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Reputation = 0.0f;
};

USTRUCT(BlueprintType)
struct FWvPlayerReputationData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Honor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GlobalReputation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WantedLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWvFactionReputationEntry> FactionReputations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWvRegionReputationEntry> RegionReputations;
};

/// <summary>
/// Possesses a long-standing reputation and hero status as a player
/// </summary>
UCLASS( ClassGroup=(Relationship), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UWvPlayerReputationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWvPlayerReputationComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;


public:
	const FWvPlayerReputationData& GetReputationData() const
	{
		return ReputationData;
	}

	float GetFactionReputation(FGameplayTag FactionTag) const;
	void SetFactionReputation(FGameplayTag FactionTag, float NewValue);

	void ApplyFactionReputationDelta(FGameplayTag FactionTag, float Delta, FName Reason);

	void MarkReputationDataDirty();

	void CopyReputationFrom(const UWvPlayerReputationComponent* SourceComponent);


protected:
	UPROPERTY(ReplicatedUsing = OnRep_ReputationData, BlueprintReadOnly, Category = "Reputation")
	FWvPlayerReputationData ReputationData;


	UFUNCTION()
	void OnRep_ReputationData();

		
};
