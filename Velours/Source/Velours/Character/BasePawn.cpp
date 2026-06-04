// Copyright 2022 wevet works All Rights Reserved.


#include "BasePawn.h"
#include "Misc/WvCommonUtils.h"
#include "Component/WvSkeletalMeshComponent.h"
#include "Component/InventoryComponent.h"
#include "Component/CombatComponent.h"
#include "Component/StatusComponent.h"
#include "Component/WeaknessComponent.h"

#include "Mission/MinimapMarkerComponent.h"

#include "Game/WvGameInstance.h"
#include "Item/BulletHoldWeaponActor.h"
#include "GameExtension.h"
#include "Game/CharacterInstanceSubsystem.h"
#include "Level/FieldInstanceSubsystem.h"

#include "Ability/WvInheritanceAttributeSet.h"
#include "WvAbilitySystemBlueprintFunctionLibrary.h"
#include "Velours.h"

// built in
#include "Components/LODSyncComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "MotionWarpingComponent.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Net/UnrealNetwork.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectBaseUtility.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Prediction.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Runtime/Launch/Resources/Version.h"
#include "IAnimationBudgetAllocator.h"
#include "SignificanceManager.h"
#include "Algo/Transform.h"
#include "ChooserFunctionLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Net/Core/PushModel/PushModel.h"

// mover
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/MovementMixer.h"

DEFINE_LOG_CATEGORY(LogBaseCharacter)

#include UE_INLINE_GENERATED_CPP_BY_NAME(BasePawn)


FName ABasePawn::MeshComponentName(TEXT("CharacterMesh0"));
FName ABasePawn::CapsuleComponentName(TEXT("CollisionCylinder"));

namespace CharacterHelper
{
	const bool HasTag(const UAbilitySystemComponent* ASC, const FGameplayTag& TagToCheck)
	{
		return ASC && ASC->HasMatchingGameplayTag(TagToCheck);
	}
}


ABasePawn::ABasePawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	SetNetCullDistanceSquared(900000000.0f);
	SetReplicateMovement(true);

	AbilitySystemComponentClass = UWvAbilitySystemComponent::StaticClass();
	bReplicates = true;

	// motion warping
	MotionWarpingComponent = ObjectInitializer.CreateDefaultSubobject<UMotionWarpingComponent>(this, TEXT("MotionWarpingComponent"));
	MotionWarpingComponent->bSearchForWindowsInAnimsWithinMontages = false;

	// managed item, weapon class
	ItemInventoryComponent = ObjectInitializer.CreateDefaultSubobject<UInventoryComponent>(this, TEXT("InventoryComponent"));
	ItemInventoryComponent->bAutoActivate = 1;

	// managed combat system
	CombatComponent = ObjectInitializer.CreateDefaultSubobject<UCombatComponent>(this, TEXT("CombatComponent"));
	CombatComponent->bAutoActivate = 1;

	// managed character health and more
	StatusComponent = ObjectInitializer.CreateDefaultSubobject<UStatusComponent>(this, TEXT("StatusComponent"));
	StatusComponent->bAutoActivate = 1;

	// managed character combat weakness
	WeaknessComponent = ObjectInitializer.CreateDefaultSubobject<UWeaknessComponent>(this, TEXT("WeaknessComponent"));
	WeaknessComponent->bAutoActivate = 1;

	// noise emitter
	PawnNoiseEmitterComponent = ObjectInitializer.CreateDefaultSubobject<UPawnNoiseEmitterComponent>(this, TEXT("PawnNoiseEmitterComponent"));
	PawnNoiseEmitterComponent->bAutoActivate = 1;

	MinimapMarkerComponent = ObjectInitializer.CreateDefaultSubobject<UMinimapMarkerComponent>(this, TEXT("MinimapMarkerComponent"));
	MinimapMarkerComponent->bAutoActivate = 1;


	MyTeamID = FGenericTeamId(0);
	CharacterTag = FGameplayTag::RequestGameplayTag(TAG_Character_Default.GetTag().GetTagName());


}

void ABasePawn::BeginPlay()
{
	Super::BeginPlay();

}


void ABasePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABasePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WEVET_COMMENT("CharacterInstanceSubsystem API")
	UCharacterInstanceSubsystem::Get()->RemoveAICharacter(this);

	//FTimerManager& TM = GetWorld()->GetTimerManager();
	//TM.ClearTimer(Ragdoll_TimerHandle);

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityFailedCallbacks.Remove(AbilityFailedDelegateHandle);
	}

	Super::EndPlay(EndPlayReason);
}


void ABasePawn::OnConstruction(const FTransform& Transform)
{
}

void ABasePawn::PreInitializeComponents()
{
	UE_LOG(LogTemp, Warning, TEXT("PreInitializeComponents: %s Class=%s"),
		*GetName(),
		*GetClass()->GetName());

	Super::PreInitializeComponents();
}

void ABasePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UCharacterMoverComponent* CharacterMover = FindComponentByClass<UCharacterMoverComponent>();

	if (CharacterMover && !CharacterMover->MovementMixer)
	{
		CharacterMover->MovementMixer = NewObject<UMovementMixer>(CharacterMover, TEXT("RuntimeMovementMixer"));
		UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] Created MovementMixer for %s"), *GetName(), *CharacterMover->GetName());
	}


	if (HasAuthority() && AbilitySystemCreationPolicy == EAbilitySystemCreationPolicy::Always)
	{
		RequestAbilitySystemWarmup(EAbilitySystemLoadReason::Always);
	}
}

void ABasePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ABasePawn, ReplicatedAbilitySystemComponent, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(ABasePawn, AbilitySystemLoadState, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ABasePawn, LastAbilitySystemLoadReason, Params);

	DOREPLIFETIME(ABasePawn, MyTeamID);
	DOREPLIFETIME(ABasePawn, ReplicatedAcceleration);
}


void ABasePawn::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);


	if (AbilitySystemCreationPolicy == EAbilitySystemCreationPolicy::Never)
	{
		return;
	}

	if (!ReplicatedAbilitySystemComponent && AbilitySystemComponent)
	{
		ReplicatedAbilitySystemComponent = AbilitySystemComponent;
		MARK_PROPERTY_DIRTY_FROM_NAME(ABasePawn, ReplicatedAbilitySystemComponent, this);
	}
}


#if WITH_EDITOR
void ABasePawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
}
#endif


const FWvAbilitySystemAvatarData& ABasePawn::GetAbilitySystemData()
{
	return AbilitySystemData;
}

void ABasePawn::InitAbilitySystemComponentByData(UWvAbilitySystemComponentBase* ASC)
{
	IWvAbilitySystemAvatarInterface::InitAbilitySystemComponentByData(ASC);

	// Read DataTable of locomotion system
	const FCustomWvAbilitySystemAvatarData& Data = GetCustomWvAbilitySystemData();

	TArray<TSoftObjectPtr<UDataTable>> AbilityTables;
	AbilityTables.Add(Data.LocomotionAbilityTable);
	AbilityTables.Add(Data.FieldAbilityTable);
	AbilityTables += Data.FunctionAbilityTables;

	for (int32 Index = 0; Index < AbilityTables.Num(); Index++)
	{
		TSoftObjectPtr<UDataTable> SoftAbilityTable = AbilityTables[Index];

		if (SoftAbilityTable.IsNull())
		{
			continue;
		}

		const FSoftObjectPath TablePath = SoftAbilityTable.ToSoftObjectPath();
		const FString TablePathString = TablePath.ToString();

		UDataTable* AbilityTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *TablePathString));
		if (!AbilityTable)
		{
			continue;
		}

		TArray<FWvAbilityRow*> Rows;
		AbilityTable->GetAllRows(SoftAbilityTable.GetAssetName(), Rows);

		for (int32 JIndex = 0; JIndex < Rows.Num(); ++JIndex)
		{
			const FWvAbilityRow* AbilityRow = Rows[JIndex];
			AbilitySystemComponent->AddRegisterAbilityDA(AbilityRow->AbilityData);
		}
	}

	AbilitySystemComponent->GiveAllRegisterAbility();
	UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : %s"), *FString(__FUNCTION__), *GetNameSafe(this));

}

UBehaviorTree* ABasePawn::GetBehaviorTree() const
{
	return nullptr;
}

UWvHitReactDataAsset* ABasePawn::GetHitReactDataAsset() const
{
	return nullptr;
}

FName ABasePawn::GetAvatarName() const
{
	return FName();
}

FGenericTeamId ABasePawn::GetGenericTeamId() const
{
	return FGenericTeamId();
}

void ABasePawn::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
}

FGameplayTag ABasePawn::GetAvatarTag() const
{
	return CharacterTag;
}

USceneComponent* ABasePawn::GetOverlapBaseComponent()
{
	return nullptr;
}

FOnTeamIndexChangedDelegate* ABasePawn::GetOnTeamIndexChangedDelegate()
{
	return nullptr;
}

bool ABasePawn::IsDead() const
{
	return false;
}

bool ABasePawn::IsTargetable() const
{
	return false;
}

bool ABasePawn::IsInBattled() const
{
	return false;
}

void ABasePawn::OnSendAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage)
{
}

void ABasePawn::OnSendWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage)
{
}

void ABasePawn::OnSendKillTarget(AActor* Actor, const float Damage)
{
}

void ABasePawn::OnReceiveAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage)
{
}

void ABasePawn::OnReceiveWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage)
{
}

void ABasePawn::OnReceiveKillTarget(AActor* Actor, const float Damage)
{
}

void ABasePawn::OnReceiveHitReact(FGameplayEffectContextHandle& Context, const bool IsInDead, const float Damage)
{
}

void ABasePawn::Freeze()
{
}

void ABasePawn::UnFreeze()
{
}

bool ABasePawn::IsFreezing() const
{
	return false;
}

bool ABasePawn::IsSprintingMovement() const
{
	return false;
}

void ABasePawn::DoAttack()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemHot(EAbilitySystemLoadReason::AttackStart);
	if (!ASC)
	{
		return;
	}

	if (ASC->HasMatchingGameplayTag(TAG_Character_ActionMelee_Forbid))
	{
		UE_LOG(LogBaseCharacter, Warning, TEXT("has tag TAG_Character_StateMelee_Forbid => %s"), *FString(__FUNCTION__));
		return;
	}

	const auto Weapon = ItemInventoryComponent->GetEquipWeapon();
	if (Weapon)
	{
		if (Weapon->IsAvailable())
		{
			const FGameplayTag TriggerTag = Weapon->GetPluralInputTriggerTag();
			ASC->TryActivateAbilityByTag(TriggerTag);
		}
	}
}

void ABasePawn::DoResumeAttack()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::AbilityActivation);
	if (!ASC)
	{
		return;
	}

	ASC->RemoveGameplayTag(TAG_Character_ActionMelee_Forbid, 1);
}

void ABasePawn::DoStopAttack()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::AbilityActivation);
	if (!ASC)
	{
		return;
	}

	ASC->AddGameplayTag(TAG_Character_ActionMelee_Forbid, 1);
}

void ABasePawn::DoBulletAttack()
{
}

void ABasePawn::DoThrowAttack()
{
}

void ABasePawn::DoStartCinematic()
{
}

void ABasePawn::DoStopCinematic()
{
}

bool ABasePawn::IsCinematic() const
{
	return false;
}

void ABasePawn::DoKill(const bool bIsForceKill)
{

}

bool ABasePawn::IsLeader() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(TAG_Character_AI_Leader);
	}
	return false;
}

void ABasePawn::SetAIActionState(const EAIActionState NewAIActionState)
{
}

EAIActionState ABasePawn::GetAIActionState() const
{
	return EAIActionState();
}


// AI Perception
// https://blog.gamedev.tv/ai-sight-perception-to-custom-points/
bool ABasePawn::CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor, const bool* bWasVisible, int32* UserData) const
{
	return false;
}


void ABasePawn::NotifyControllerChanged()
{
	const FGenericTeamId OldTeamId = GetGenericTeamId();

	Super::NotifyControllerChanged();

	// Update our team ID based on the controller
	if (HasAuthority() && (Controller != nullptr))
	{
		if (IWvAbilityTargetInterface* ControllerWithTeam = Cast<IWvAbilityTargetInterface>(Controller))
		{
			MyTeamID = ControllerWithTeam->GetGenericTeamId();
			ConditionalBroadcastTeamChanged(this, OldTeamId, MyTeamID);
		}
	}
}

UMotionWarpingComponent* ABasePawn::GetMotionWarpingComponent() const
{
	return MotionWarpingComponent;
}

UWvSkeletalMeshComponent* ABasePawn::GetWvSkeletalMeshComponent() const
{
	return nullptr;
}

UCombatComponent* ABasePawn::GetCombatComponent() const
{
	return CombatComponent;
}

UInventoryComponent* ABasePawn::GetInventoryComponent() const
{
	return ItemInventoryComponent;
}

UWeaknessComponent* ABasePawn::GetWeaknessComponent() const
{
	return WeaknessComponent;
}

UMinimapMarkerComponent* ABasePawn::GetMinimapMarkerComponent() const
{
	return MinimapMarkerComponent;
}


void ABasePawn::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void ABasePawn::OnRep_ReplicatedAcceleration()
{
	//
}

const FCustomWvAbilitySystemAvatarData& ABasePawn::GetCustomWvAbilitySystemData()
{
	return AbilitySystemData;
}

float ABasePawn::GetSkillToWidget() const
{
	return StatusComponent->GetSkillToWidget();
}

float ABasePawn::GetHealthToWidget() const
{
	return StatusComponent->GetHealthToWidget();
}

bool ABasePawn::IsBotCharacter() const
{
	return UWvCommonUtils::IsBot(GetController());
}

void ABasePawn::BeginDrive()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::Interact);
	if (!ASC)
	{
		return;
	}


}

void ABasePawn::EndDrive()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::Interact);
	if (!ASC)
	{
		return;
	}
}

bool ABasePawn::IsVehicleDriving() const
{
	const UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TAG_Vehicle_State_Drive);
}

void ABasePawn::SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale)
{
	AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
}

float ABasePawn::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}

#pragma region Melee
bool ABasePawn::IsMeleePlaying() const
{
	if (GetWvAbilitySystemComponent())
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(TAG_Character_StateMelee);
	}
	return false;
}


#pragma endregion


#pragma region Abilities
UWvAbilitySystemComponent* ABasePawn::RequestAbilitySystemWarmup(EAbilitySystemLoadReason Reason)
{
	if (AbilitySystemCreationPolicy == EAbilitySystemCreationPolicy::Never)
	{
		return nullptr;
	}

	// Clients never create or change the ASC load state.
	// They only use the ASC replicated from the server.
	if (!HasAuthority())
	{
		return GetWvAbilitySystemComponent();
	}

	if (AbilitySystemLoadState == EAbilitySystemLoadState::Hot)
	{
		return AbilitySystemComponent;
	}

	UWvAbilitySystemComponent* ASC = EnsureAbilitySystemComponentCreated();

	if (ASC)
	{
		SetAbilitySystemLoadState(EAbilitySystemLoadState::Warm, Reason);
	}

	return ASC;
}

UWvAbilitySystemComponent* ABasePawn::RequestAbilitySystemHot(EAbilitySystemLoadReason Reason)
{
	if (AbilitySystemCreationPolicy == EAbilitySystemCreationPolicy::Never)
	{
		return nullptr;
	}

	if (!HasAuthority())
	{
		return GetWvAbilitySystemComponent();
	}

	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(Reason);

	if (ASC)
	{
		SetAbilitySystemLoadState(EAbilitySystemLoadState::Hot, Reason);
	}

	return ASC;
}


UWvAbilitySystemComponent* ABasePawn::EnsureAbilitySystemComponentCreated()
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	if (!HasAuthority())
	{
		return nullptr;
	}

	if (AbilitySystemCreationPolicy == EAbilitySystemCreationPolicy::Never)
	{
		return nullptr;
	}

	if (!AbilitySystemComponentClass)
	{
		return nullptr;
	}

	CreateAbilitySystemComponent();
	InitializeAbilitySystemComponent();
	ForceNetUpdate();
	return AbilitySystemComponent;
}

UAbilitySystemComponent* ABasePawn::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	if (!HasAuthority())
	{
		return nullptr;
	}

	if (AbilitySystemCreationPolicy != EAbilitySystemCreationPolicy::Lazy)
	{
		return nullptr;
	}

	ABasePawn* MutableThis = const_cast<ABasePawn*>(this);
	return MutableThis->EnsureAbilitySystemComponentCreated();
}

UWvAbilitySystemComponent* ABasePawn::GetWvAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABasePawn::CreateAbilitySystemComponent()
{
	if (AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent = NewObject<UWvAbilitySystemComponent>(this, AbilitySystemComponentClass, TEXT("AbilitySystemComponent"));

	check(AbilitySystemComponent);

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AbilitySystemComponent->RegisterComponent();

	UE_LOG(LogBaseCharacter, Log, TEXT("[%s] Created for %s"), *FString(__FUNCTION__), *GetNameSafe(this));
}

void ABasePawn::InitializeAbilitySystemComponent()
{
	if (!AbilitySystemComponent || bAbilitySystemInitialized)
	{
		return;
	}

	bAbilitySystemInitialized = true;

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (GetLocalRole() == ROLE_Authority)
	{
		AbilitySystemComponent->AddStartupGameplayAbilities();
	}

	// input component setup
	PostAbilitySystemInitialize();

	OnAbilitySystemAvailable.Broadcast(AbilitySystemComponent);
	AbilityFailedDelegateHandle = AbilitySystemComponent->AbilityFailedCallbacks.AddUObject(this, &ThisClass::OnAbilityFailed_Callback);
}

/// <summary>
/// client
/// </summary>
void ABasePawn::OnRep_ReplicatedAbilitySystemComponent()
{
	AbilitySystemComponent = ReplicatedAbilitySystemComponent;

	if (AbilitySystemComponent)
	{
		InitializeAbilitySystemComponent();

		// Pending attribute replication ‚ðŽg‚¤‚È‚ç‚±‚±‚Å“K—p
		// ApplyPendingAttributesFromReplication();

		UE_LOG(LogBaseCharacter, Log, TEXT("[%s]"), *FString(__FUNCTION__));
	}
}


void ABasePawn::SetAbilitySystemLoadState(EAbilitySystemLoadState NewState, EAbilitySystemLoadReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AbilitySystemLoadState == NewState && LastAbilitySystemLoadReason == Reason)
	{
		return;
	}

	const EAbilitySystemLoadState OldState = AbilitySystemLoadState;

	AbilitySystemLoadState = NewState;
	LastAbilitySystemLoadReason = Reason;

	MARK_PROPERTY_DIRTY_FROM_NAME(ABasePawn, AbilitySystemLoadState, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ABasePawn, LastAbilitySystemLoadReason, this);

	UE_LOG(
		LogBaseCharacter,
		Log,
		TEXT("ASC LoadState changed. Actor=%s Old=%d New=%d Reason=%d"),
		*GetNameSafe(this),
		static_cast<uint8>(OldState),
		static_cast<uint8>(NewState),
		static_cast<uint8>(Reason)
	);
}

void ABasePawn::OnRep_AbilitySystemLoadState(EAbilitySystemLoadState OldState)
{
	UE_LOG(
		LogBaseCharacter,
		Log,
		TEXT("ASC LoadState replicated. Actor=%s Old=%d New=%d Reason=%d"),
		*GetNameSafe(this),
		static_cast<uint8>(OldState),
		static_cast<uint8>(AbilitySystemLoadState),
		static_cast<uint8>(LastAbilitySystemLoadReason)
	);

	// Notifications to the UI/Debug display and lightweight components are fine here.
	// However, do not create a new ASC object on the client side.
}

void ABasePawn::OnAbilityFailed_Callback(const UGameplayAbility* Ability, const FGameplayTagContainer& GameplayTags)
{
}

void ABasePawn::RequestAbilitySystemCooldown(EAbilitySystemLoadReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AbilitySystemLoadState == EAbilitySystemLoadState::Hot)
	{
		SetAbilitySystemLoadState(EAbilitySystemLoadState::Warm, Reason);
	}
}

EAbilitySystemLoadState ABasePawn::GetAbilitySystemLoadState() const
{
	return AbilitySystemLoadState; 
}

EAbilitySystemLoadReason ABasePawn::GetLastAbilitySystemLoadReason() const
{
	return LastAbilitySystemLoadReason; 
}
#pragma endregion

