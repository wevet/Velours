// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// plugin 
#include "Interface/WvAbilitySystemAvatarInterface.h"
#include "Interface/WvAbilityTargetInterface.h"
#include "CharacterSystemTypes.h"

// builtin
#include "AbilitySystemInterface.h"
#include "Perception/AISightTargetInterface.h"
#include "HAL/Platform.h"
#include "UObject/UObjectGlobals.h"
#include "Logging/LogMacros.h"
#include "GameFramework/Pawn.h"
#include "BasePawn.generated.h"

class UMotionWarpingComponent;
class UPawnNoiseEmitterComponent;

class UCharacterMovementHelperComponent;
class UInventoryComponent;
class UCombatComponent;
class UStatusComponent;
class UWeaknessComponent;
class UStaticMeshComponent;
class ULODSyncComponent;
//class USignificanceComponent;
class UMinimapMarkerComponent;
class UChooserTable;
class UBehaviorTree;

UCLASS()
class VELOURS_API ABasePawn : public APawn, 
	public IAbilitySystemInterface,
	public IAISightTargetInterface,
	public IWvAbilitySystemAvatarInterface,
	public IWvAbilityTargetInterface,
	public IWvAIActionStateInterface
{
	GENERATED_BODY()

public:
	ABasePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;

public:
#pragma region IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
#pragma endregion

#pragma region IWvAbilitySystemAvatarInterface
	virtual const FWvAbilitySystemAvatarData& GetAbilitySystemData() override;
	virtual void InitAbilitySystemComponentByData(class UWvAbilitySystemComponentBase* ASC) override;
	virtual UBehaviorTree* GetBehaviorTree() const override;
	virtual UWvHitReactDataAsset* GetHitReactDataAsset() const override;
	virtual FName GetAvatarName() const override;
#pragma endregion

#pragma region IWvAbilityTargetInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;

	virtual FGameplayTag GetAvatarTag() const override;
	virtual USceneComponent* GetOverlapBaseComponent() override;
	virtual FOnTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;

	virtual bool IsDead() const override;
	virtual bool IsTargetable() const override;
	virtual bool IsInBattled() const override;

	virtual void OnSendAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage) override;
	virtual void OnSendWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage) override;
	virtual void OnSendKillTarget(AActor* Actor, const float Damage) override;

	virtual void OnReceiveAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage) override;
	virtual void OnReceiveWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage) override;
	virtual void OnReceiveKillTarget(AActor* Actor, const float Damage) override;
	virtual void OnReceiveHitReact(FGameplayEffectContextHandle& Context, const bool IsInDead, const float Damage) override;

	virtual void Freeze() override;
	virtual void UnFreeze() override;
	virtual bool IsFreezing() const override;
	virtual bool IsSprintingMovement() const override;

	virtual void DoAttack() override;
	virtual void DoResumeAttack() override;
	virtual void DoStopAttack() override;

	virtual void DoBulletAttack() override;
	virtual void DoThrowAttack() override;

	virtual void DoStartCinematic() override;
	virtual void DoStopCinematic() override;
	virtual bool IsCinematic() const override;
#pragma endregion

#pragma region IWvAIActionStateInterface
	virtual void SetAIActionState(const EAIActionState NewAIActionState) override;
	virtual EAIActionState GetAIActionState() const override;
#pragma endregion

#pragma region IAISightTargetInterface
	/**
	* The method needs to check whether the implementer is visible from given observer's location.
	* @param ObserverLocation	The location of the observer
	* @param OutSeenLocation	The first visible target location
	* @param OutSightStrengh	The sight strength for how well the target is seen
	* @param IgnoreActor		The actor to ignore when doing test
	* @param bWasVisible		If available, it is the previous visibility state
	* @param UserData			If available, it is a data passed between visibility tests for the users to store whatever they want
	* @return	True if visible from the observer's location
	*/
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor = nullptr, const bool* bWasVisible = nullptr, int32* UserData = nullptr) const;
#pragma endregion


	//~APawn interface
	virtual void NotifyControllerChanged() override;
	//~End of APawn interface


public:
	virtual class UWvSkeletalMeshComponent* GetWvSkeletalMeshComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UWvAbilitySystemComponent* GetWvAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UMotionWarpingComponent* GetMotionWarpingComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UCombatComponent* GetCombatComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UWeaknessComponent* GetWeaknessComponent() const;

	class UMinimapMarkerComponent* GetMinimapMarkerComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWvAbilitySystemComponent> WvAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInventoryComponent> ItemInventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStatusComponent> StatusComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWeaknessComponent> WeaknessComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UPawnNoiseEmitterComponent> PawnNoiseEmitterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UMinimapMarkerComponent> MinimapMarkerComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Config")
	FWvAbilitySystemAvatarData AbilitySystemData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Config")
	FGameplayTag CharacterTag;


	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FWvReplicatedAcceleration ReplicatedAcceleration;

	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;


protected:
	UFUNCTION()
	void OnRep_MyTeamID(FGenericTeamId OldTeamID);

	UFUNCTION()
	void OnRep_ReplicatedAcceleration();

};
