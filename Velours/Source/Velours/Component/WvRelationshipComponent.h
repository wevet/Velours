// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WvRelationshipComponent.generated.h"


UENUM(BlueprintType)
enum class EWvFactionAttitude : uint8
{
	Ignore     UMETA(DisplayName = "Ignore"),
	Friendly   UMETA(DisplayName = "Friendly"),
	Neutral    UMETA(DisplayName = "Neutral"),
	Suspicious UMETA(DisplayName = "Suspicious"),
	Fear       UMETA(DisplayName = "Fear"),
	Hostile    UMETA(DisplayName = "Hostile"),
};

USTRUCT(BlueprintType)
struct FWvRelationshipMemory
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Trust = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Fear = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Suspicion = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Anger = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LastUpdateTime = 0.0f;

	bool IsValid() const
	{
		return ::IsValid(TargetActor);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWvRelationshipMemoryChangedDelegate, AActor*, TargetActor, const FWvRelationshipMemory&, Memory);

/// <summary>
///  Actor: Possesses short-term emotions and memories for each individual
/// </summary>
UCLASS( ClassGroup=(Relationship), meta=(BlueprintSpawnableComponent) )
class VELOURS_API UWvRelationshipComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWvRelationshipComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	bool bRelationshipEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	bool bAutoDecay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship", meta = (ClampMin = "0.0"))
	float DecayInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship", meta = (ClampMin = "0.0"))
	float TrustDecayPerSecond = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship", meta = (ClampMin = "0.0"))
	float FearDecayPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship", meta = (ClampMin = "0.0"))
	float SuspicionDecayPerSecond = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship", meta = (ClampMin = "0.0"))
	float AngerDecayPerSecond = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	float HostileAngerThreshold = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	float FearThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	float SuspiciousThreshold = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relationship")
	float FriendlyTrustThreshold = 60.0f;

	UPROPERTY(BlueprintAssignable, Category = "Relationship")
	FWvRelationshipMemoryChangedDelegate OnRelationshipMemoryChanged;

public:
	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void SetRelationshipEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void NotifyThreatenedBy(AActor* InstigatorActor, float Amount);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void NotifyDamagedBy(AActor* InstigatorActor, float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void NotifyHelpedBy(AActor* InstigatorActor, float Amount);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void NotifySuspiciousOf(AActor* TargetActor, float Amount);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void ClearMemoryFor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Relationship")
	void ClearAllMemories();

	UFUNCTION(BlueprintPure, Category = "Relationship")
	bool HasMemoryFor(AActor* TargetActor) const;

	const FWvRelationshipMemory* FindMemory(AActor* TargetActor) const;

	UFUNCTION(BlueprintPure, Category = "Relationship")
	EWvFactionAttitude GetMemoryAttitudeFor(AActor* TargetActor) const;

protected:
	FWvRelationshipMemory* FindOrAddMemory(AActor* TargetActor);

	void ClampMemory(FWvRelationshipMemory& Memory) const;
	void BroadcastMemoryChanged(const FWvRelationshipMemory& Memory);
	void DecayRelationshipMemory(float DeltaTime);

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Relationship")
	TArray<FWvRelationshipMemory> RelationshipMemories;

private:
	float DecayTimeAccumulator = 0.0f;
		
};
