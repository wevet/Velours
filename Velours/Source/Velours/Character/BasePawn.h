// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// plugin 
#include "Interface/WvAbilitySystemAvatarInterface.h"
#include "Interface/WvAbilityTargetInterface.h"
#include "CharacterSystemTypes.h"

// project
#include "Ability/WvAbilitySystemComponent.h"
#include "Ability/WvAbilityType.h"
//#include "BaseCharacterTypes.h"
#include "Mission/MissionSystemTypes.h"
//#include "Significance/SignificanceInterface.h"
//#include "Component/WvCharacterMovementTypes.h"


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
class ULocomotionComponent;
class UInventoryComponent;
class UCombatComponent;
class UStatusComponent;
class UWeaknessComponent;
class UStaticMeshComponent;
class ULODSyncComponent;
class UWvFactionComponent;
class UWvRelationshipComponent;
class UMinimapMarkerComponent;
class UChooserTable;
class UBehaviorTree;

DECLARE_LOG_CATEGORY_EXTERN(LogBaseCharacter, All, All)

namespace CharacterDebug
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	extern TAutoConsoleVariable<int32> CVarDebugCharacterStatus;
	extern TAutoConsoleVariable<int32> CVarDebugCombatSystem;

#endif
}

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbilitySystemAvailable, UWvAbilitySystemComponent*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActionStateChangeDelegate, EAIActionState, NewAIActionState, EAIActionState, PrevAIActionState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBoolOneParamDelegate, bool, bEnable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOverlayChangeDelegate, const ELSOverlayState, CurrentOverlay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAsyncLoadCompleteDelegate);

UCLASS(Abstract)
class VELOURS_API ABasePawn : public APawn, 
	public IAbilitySystemInterface,
	public IAISightTargetInterface,
	public IWvAbilitySystemAvatarInterface,
	public IWvAbilityTargetInterface,
	public IWvAIActionStateInterface, 
	public IWvCinematicTargetInterface
{
	GENERATED_BODY()

public:
	ABasePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	virtual FGameplayTag GetAvatarTag() const override;
#pragma endregion


#pragma region IWvAbilityTargetInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;

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
	virtual bool IsMeleeAttacking() const override;

	virtual void DoBulletAttack() override;
	virtual void DoThrowAttack() override;

	virtual void DoKill(const bool bIsForceKill) override;
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


#pragma region IWvCinematicTargetInterface
	virtual void DoStartCinematic() override;
	virtual void DoStopCinematic() override;
	virtual bool IsCinematic() const override;
#pragma endregion


	//~APawn interface
	virtual void NotifyControllerChanged() override;
	//~End of APawn interface


	FOnAbilitySystemAvailable OnAbilitySystemAvailable;

	UPROPERTY(BlueprintAssignable)
	FOnTeamHandleAttackDelegate OnTeamHandleAttackDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTeamWeaknessHandleAttackDelegate OnTeamWeaknessHandleAttackDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTeamHandleAttackDelegate OnTeamHandleReceiveDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTeamWeaknessHandleAttackDelegate OnTeamWeaknessHandleReceiveDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTeamHandleKillDelegate OnTeamHandleSendKillDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTeamHandleKillDelegate OnTeamHandleReceiveKillDelegate;

	UPROPERTY()
	FOnTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY(BlueprintAssignable)
	FActionStateChangeDelegate ActionStateChangeDelegate;

	UPROPERTY(BlueprintAssignable)
	FAsyncLoadCompleteDelegate AsyncLoadCompleteDelegate;

	UPROPERTY(BlueprintAssignable)
	FAsyncLoadCompleteDelegate AsyncMeshesLoadCompleteDelegate;

	UPROPERTY(BlueprintAssignable)
	FBoolOneParamDelegate AimingChangeDelegate;

	UPROPERTY(BlueprintAssignable)
	FBoolOneParamDelegate OnSkillEnableDelegate;

	UPROPERTY(BlueprintAssignable)
	FBoolOneParamDelegate OnJumpChangeDelegate;

	UPROPERTY(BlueprintAssignable)
	FOverlayChangeDelegate OverlayChangeDelegate;

public:
	static FName MeshComponentName;
	static FName CapsuleComponentName;

	static FName ClimbSyncPoint;
	static FName BackwardInputSyncPoint;


public:
#pragma region Components
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

	UFUNCTION(BlueprintCallable, Category = Components)
	class UMinimapMarkerComponent* GetMinimapMarkerComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class ULocomotionComponent* GetLocomotionComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UWvFactionComponent* GetFactionComponent() const;

	UFUNCTION(BlueprintCallable, Category = Components)
	class UWvRelationshipComponent* GetRelationshipComponent() const;
#pragma endregion

	UFUNCTION(BlueprintCallable, Category = Overlay)
	FTransform GetPivotOverlayTansform() const;



	float GetSkillToWidget() const;
	float GetHealthToWidget() const;
	bool IsHealthHalf() const;
	bool IsBotCharacter() const;
	bool IsLeader() const;
	bool IsTargetLock() const;

	UFUNCTION(BlueprintCallable, Category = Action)
	const bool OverlayStateChange(const ELSOverlayState CurrentOverlay);

	UFUNCTION(BlueprintCallable, Category = "BaseCharacter|Shape")
	void SetGenderType(const EGenderType InGenderType);

	UFUNCTION(BlueprintCallable, Category = "BaseCharacter|Shape")
	EGenderType GetGenderType() const;

	UFUNCTION(BlueprintCallable, Category = "BaseCharacter|Shape")
	void SetBodyShapeType(const EBodyShapeType InBodyShapeType);

	UFUNCTION(BlueprintCallable, Category = "BaseCharacter|Shape")
	EBodyShapeType GetBodyShapeType() const;

	UFUNCTION(BlueprintCallable, Category = "BaseCharacter|Shape")
	FCharacterInfo GetCharacterInfo() const;


#pragma region NearlestAction
	const TArray<AActor*> FindNearestTargets(const float Distance, const float AngleThreshold);
	AActor* FindNearestTarget(const float Distance, const float AngleThreshold, bool bTargetCheckBattled = true);


	void CalcurateNearlestTarget(const float SyncPointWeight);
	void ResetNearlestTarget();
	void FindNearestTarget(AActor* Target, const float SyncPointWeight);
	void FindNearestTarget(const FVector TargetPosition, const float SyncPointWeight);

	void FindNearestTarget(const FAttackMotionWarpingData& AttackMotionWarpingData);

#pragma endregion

#pragma region VehicleAction
	void BeginDrive();
	void EndDrive();
	bool IsVehicleDriving() const;
#pragma endregion

	void SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale);
	float GetAnimRootMotionTranslationScale() const;


	int32 GetCombatAnimationIndex() const;
	int32 CloseCombatMaxComboCount(const int32 Index) const;
	UAnimMontage* GetCloseCombatAnimMontage(const int32 Index, const FGameplayTag Tag) const;
	float CalcurateBodyShapePlayRate() const;
	void CalculateBackwardInputRotation();


#pragma region AsyncLoad
	virtual void RequestAsyncLoad();
	virtual void RequestComponentsAsyncLoad();
#pragma endregion


protected:
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class ULocomotionComponent> LocomotionComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWvFactionComponent> WvFactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Component, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWvRelationshipComponent> WvRelationshipComponent;
#pragma endregion

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Config")
	FGameplayTag CharacterTag;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FWvReplicatedAcceleration ReplicatedAcceleration;

	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;

	UPROPERTY(Transient)
	TObjectPtr<class UWvAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Abilities")
	FCustomWvAbilitySystemAvatarData AbilitySystemData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Abilities")
	TSubclassOf<class UWvAbilitySystemComponent> AbilitySystemComponentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Abilities")
	EAbilitySystemCreationPolicy AbilitySystemCreationPolicy = EAbilitySystemCreationPolicy::Lazy;

	UPROPERTY(ReplicatedUsing = OnRep_AbilitySystemLoadState, VisibleInstanceOnly, BlueprintReadOnly, Category = "BaseCharacter|Abilities")
	EAbilitySystemLoadState AbilitySystemLoadState = EAbilitySystemLoadState::Cold;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "BaseCharacter|Abilities")
	EAbilitySystemLoadReason LastAbilitySystemLoadReason = EAbilitySystemLoadReason::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Config")
	FFinisherConfig FinisherConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Overlay")
	ELSOverlayState SelectableOverlayState;

#pragma region DA
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Load")
	bool bIsAllowAsyncLoadComponentAssets = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Load")
	TMap<FGameplayTag, TSoftObjectPtr<UDataAsset>> GameDataAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BaseCharacter|Load")
	UAIActionStateDataAsset* AIActionStateDA;

	//TObjectPtr<class UChooserTable> OverlayAnimationTable{ nullptr };
	//TObjectPtr<class UChooserTable> FoleyAssetTable{ nullptr };
#pragma endregion



protected:
	UFUNCTION()
	void OnRep_MyTeamID(FGenericTeamId OldTeamID);

	UFUNCTION()
	void OnRep_ReplicatedAcceleration();

	UFUNCTION()
	void OnRep_AbilitySystemLoadState(EAbilitySystemLoadState OldState);

	UFUNCTION()
	void OnAbilityFailed_Callback(const UGameplayAbility* Ability, const FGameplayTagContainer& GameplayTags);


	void SetAbilitySystemLoadState(EAbilitySystemLoadState NewState, EAbilitySystemLoadReason Reason);

public:
	const FCustomWvAbilitySystemAvatarData& GetCustomWvAbilitySystemData();

	UWvAbilitySystemComponent* RequestAbilitySystemWarmup(EAbilitySystemLoadReason Reason);
	UWvAbilitySystemComponent* RequestAbilitySystemHot(EAbilitySystemLoadReason Reason);
	EAbilitySystemLoadState GetAbilitySystemLoadState() const;
	EAbilitySystemLoadReason GetLastAbilitySystemLoadReason() const;

	void RequestAbilitySystemCooldown(EAbilitySystemLoadReason Reason);

protected:
	virtual void PostAbilitySystemInitialize() {};

private:
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAbilitySystemComponent)
	TObjectPtr<UWvAbilitySystemComponent> ReplicatedAbilitySystemComponent = nullptr;

	struct FPendingAttributeReplication
	{
		FPendingAttributeReplication()
		{
		}

		FPendingAttributeReplication(const FGameplayAttribute& InAttribute, const FGameplayAttributeData& InNewValue)
		{
			Attribute = InAttribute;
			NewValue = InNewValue;
		}

		FGameplayAttribute Attribute;
		FGameplayAttributeData NewValue;
	};

	UPROPERTY(Transient)
	bool bAbilitySystemInitialized = false;

	TArray<struct FPendingAttributeReplication> PendingAttributeReplications;
	FDelegateHandle AbilityFailedDelegateHandle;

	UWvAbilitySystemComponent* EnsureAbilitySystemComponentCreated();
	void CreateAbilitySystemComponent();
	void InitializeAbilitySystemComponent();

	UFUNCTION()
	void OnRep_ReplicatedAbilitySystemComponent();

	/** Scale to apply to root motion translation on this Character */
	UPROPERTY(Replicated)
	float AnimRootMotionTranslationScale{1.0f};



#pragma region AsyncLoad

	virtual void OnAsyncLoadCompleteHandler();
	virtual void OnSyncLoadCompleteHandler();

	template<typename T>
	T* OnAsyncLoadDataAsset(const FGameplayTag Tag);

	template<typename T>
	T* OnSyncLoadDataAsset(const FGameplayTag Tag);

	UPROPERTY()
	TObjectPtr<UCloseCombatAnimationDataAsset> CloseCombatDA;

	UPROPERTY()
	TObjectPtr<UFinisherDataAsset> FinisherSenderDA;

	UPROPERTY()
	TObjectPtr<UFinisherDataAsset> TakeDownActionDA;

	UPROPERTY()
	TObjectPtr<UWvHitReactDataAsset> HitReactionDA;

	UPROPERTY()
	TObjectPtr<UCharacterVFXDataAsset> CharacterVFXDA;

	TSharedPtr<FStreamableHandle> AsyncLoadStreamer;

#pragma endregion
};
