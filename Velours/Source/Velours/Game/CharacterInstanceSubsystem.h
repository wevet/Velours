// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CharacterInstanceSubsystem.generated.h"


class ABasePawn;
class UWvSkeletalMeshComponent;

/**
 * 
 */
UCLASS()
class VELOURS_API UCharacterInstanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	

public:
	UCharacterInstanceSubsystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ~FTickableGameObject
	virtual TStatId GetStatId() const;
	virtual bool IsTickable() const;
	virtual void Tick(float DeltaTime) override;
	// ~FTickableGameObject

public:
	static UCharacterInstanceSubsystem* Get();

	UFUNCTION(BlueprintCallable, Category = CharacterInstanceSubsystem)
	void FreezeAlCharacters(bool bFindWorldActorIterator = false);

	UFUNCTION(BlueprintCallable, Category = CharacterInstanceSubsystem)
	void UnFreezeAlCharacters(bool bFindWorldActorIterator = false);

	UFUNCTION(BlueprintCallable, Category = CharacterInstanceSubsystem)
	void DoForceKill(bool bFindWorldActorIterator = false);

	UFUNCTION(BlueprintCallable, Category = CharacterInstanceSubsystem)
	void DoForceKillIgnorePlayer(bool bFindWorldActorIterator = false);

	void AssignAICharacter(ABasePawn* NewCharacter);
	void RemoveAICharacter(ABasePawn* InCharacter);
	void GeneratorSpawnedFinish();

	bool IsInEnemyAgent(const ABasePawn* Other) const;
	bool IsInFriendAgent(const ABasePawn* Other) const;
	bool IsInNeutralAgent(const ABasePawn* Other) const;

	bool IsInBattleAny() const;

	TArray<ABasePawn*> GetLeaderAgent() const;

	TArray<ABasePawn*> GetPOIActors() const;

	void StartCinematicCharacter(ABasePawn* InCharacter);
	void StopCinematicCharacter(ABasePawn* InCharacter);

	TArray<ABasePawn*> GetIgnorePlayerArray() const;

private:
	UPROPERTY()
	TArray<ABasePawn*> Characters;

	static UCharacterInstanceSubsystem* Instance;

	void UpdateCharacterInWorld();
	void WorldCharacterIterator(TArray<class ABasePawn*>& OutCharacterArray);

	TArray<UWvSkeletalMeshComponent*> GetSkelMeshComponents(const ABasePawn* InCharacter) const;

	bool bIsTickable = false;
};
