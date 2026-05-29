// Copyright 2022 wevet works All Rights Reserved.


#include "BasePawn.h"
#include "Misc/WvCommonUtils.h"
#include "Component/CharacterMovementHelperComponent.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(BasePawn)

ABasePawn::ABasePawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	//bUseControllerRotationPitch = false;
	//bUseControllerRotationYaw = false;
	//bUseControllerRotationRoll = false;
	//AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	SetNetCullDistanceSquared(900000000.0f);
	SetReplicateMovement(true);

	// motion warping
	MotionWarpingComponent = ObjectInitializer.CreateDefaultSubobject<UMotionWarpingComponent>(this, TEXT("MotionWarpingComponent"));
	MotionWarpingComponent->bSearchForWindowsInAnimsWithinMontages = false;

	// asc
	WvAbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UWvAbilitySystemComponent>(this, TEXT("WvAbilitySystemComponent"));
	WvAbilitySystemComponent->bAutoActivate = 1;

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

	//CharacterMovementHelperComponent = ObjectInitializer.CreateDefaultSubobject<UCharacterMovementHelperComponent>(this, TEXT("CharacterMovementHelperComponent"));
	//CharacterMovementHelperComponent->bAutoActivate = 1;

	MinimapMarkerComponent = ObjectInitializer.CreateDefaultSubobject<UMinimapMarkerComponent>(this, TEXT("MinimapMarkerComponent"));
	MinimapMarkerComponent->bAutoActivate = 1;


	MyTeamID = FGenericTeamId(0);
	CharacterTag = FGameplayTag::RequestGameplayTag(TAG_Character_Default.GetTag().GetTagName());


}

void ABasePawn::BeginPlay()
{
	Super::BeginPlay();

	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Component: %s / Class: %s"),
			*GetName(),
			*Comp->GetName(),
			*Comp->GetClass()->GetName());
	}
}


void ABasePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABasePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}


void ABasePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
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

	UE_LOG(LogTemp, Warning, TEXT("PostInitializeComponents: %s Class=%s"),
		*GetName(),
		*GetClass()->GetName());

	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Component: %s / %s"),
			*Comp->GetName(),
			*Comp->GetClass()->GetName());
	}
}

void ABasePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
}

void ABasePawn::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
}

#if WITH_EDITOR
void ABasePawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
}
#endif

UAbilitySystemComponent* ABasePawn::GetAbilitySystemComponent() const
{
	return nullptr;
}

const FWvAbilitySystemAvatarData& ABasePawn::GetAbilitySystemData()
{
	return AbilitySystemData;
}

void ABasePawn::InitAbilitySystemComponentByData(UWvAbilitySystemComponentBase* ASC)
{
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
}

void ABasePawn::DoResumeAttack()
{
}

void ABasePawn::DoStopAttack()
{
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
}

UWvAbilitySystemComponent* ABasePawn::GetWvAbilitySystemComponent() const
{
	return WvAbilitySystemComponent;
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
