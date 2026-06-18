// Copyright 2022 wevet works All Rights Reserved.


#include "BasePawn.h"
#include "Misc/WvCommonUtils.h"
#include "Component/WvSkeletalMeshComponent.h"
#include "Component/InventoryComponent.h"
#include "Component/CombatComponent.h"
#include "Component/StatusComponent.h"
#include "Component/WeaknessComponent.h"
#include "Component/LocomotionComponent.h"
#include "Component/WvFactionComponent.h"
#include "Component/WvRelationshipComponent.h"

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

namespace CharacterDebug
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	TAutoConsoleVariable<int32> CVarDebugCharacterStatus(TEXT("wv.CharacterStatus.Debug"), 0, TEXT("CharacterStatus Debug .\n") TEXT("<=0: off\n") TEXT("  1: on\n"), ECVF_Default);
	TAutoConsoleVariable<int32> CVarDebugCombatSystem(TEXT("wv.CombatSystem.Debug"), 0, TEXT("CombatSystem Debug .\n") TEXT("<=0: off\n") TEXT("  1: on\n"), ECVF_Default);
#endif
}


#include UE_INLINE_GENERATED_CPP_BY_NAME(BasePawn)


FName ABasePawn::MeshComponentName(TEXT("CharacterMesh0"));
FName ABasePawn::CapsuleComponentName(TEXT("CollisionCylinder"));
FName ABasePawn::ClimbSyncPoint = FName(TEXT("ClimbSyncPoint"));
FName ABasePawn::BackwardInputSyncPoint = FName(TEXT("BackwardInputSyncPoint"));


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

	// minimap
	MinimapMarkerComponent = ObjectInitializer.CreateDefaultSubobject<UMinimapMarkerComponent>(this, TEXT("MinimapMarkerComponent"));
	MinimapMarkerComponent->bAutoActivate = 1;

	// locomotion
	LocomotionComponent = ObjectInitializer.CreateDefaultSubobject<ULocomotionComponent>(this, TEXT("LocomotionComponent"));
	LocomotionComponent->bAutoActivate = 1;

	WvFactionComponent = ObjectInitializer.CreateDefaultSubobject<UWvFactionComponent>(this, TEXT("WvFactionComponent"));
	WvFactionComponent->bAutoActivate = 1;

	WvRelationshipComponent = ObjectInitializer.CreateDefaultSubobject<UWvRelationshipComponent>(this, TEXT("WvRelationshipComponent"));
	WvRelationshipComponent->bAutoActivate = 1;


	MyTeamID = FGenericTeamId(0);
	CharacterTag = FGameplayTag::RequestGameplayTag(TAG_Character_Default.GetTag().GetTagName());


}

void ABasePawn::BeginPlay()
{
	Super::BeginPlay();

	RequestAsyncLoad();

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



#pragma region IWvAbilitySystemAvatarInterface
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

FGameplayTag ABasePawn::GetAvatarTag() const
{
	return CharacterTag;
}
#pragma endregion

FGenericTeamId ABasePawn::GetGenericTeamId() const
{
	return MyTeamID;
}

void ABasePawn::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (!GetController())
	{
		if (HasAuthority())
		{
			const FGenericTeamId OldTeamID = MyTeamID;
			MyTeamID = NewTeamID;
			ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
		}
		else
		{
			UE_LOG(LogBaseCharacter, Error, TEXT("You can't set the team ID on a character (%s) except on the authority"), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogBaseCharacter, Error, TEXT("You can't set the team ID on a possessed character (%s); it's driven by the associated controller"), *GetNameSafe(this));
	}
}


USceneComponent* ABasePawn::GetOverlapBaseComponent()
{
	return nullptr;
}

FOnTeamIndexChangedDelegate* ABasePawn::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
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

bool ABasePawn::IsMeleeAttacking() const
{
	if (GetWvAbilitySystemComponent())
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(TAG_Character_StateMelee);
	}
	return false;
}

void ABasePawn::DoBulletAttack()
{
}

void ABasePawn::DoThrowAttack()
{
}

void ABasePawn::DoKill(const bool bIsForceKill)
{

}


#pragma region IWvCinematicTargetInterface
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
#pragma endregion


#pragma region IWvAIActionStateInterface
void ABasePawn::SetAIActionState(const EAIActionState NewAIActionState)
{
}

EAIActionState ABasePawn::GetAIActionState() const
{
	return EAIActionState();
}
#pragma endregion


#pragma region IAISightTargetInterface
// AI Perception
// https://blog.gamedev.tv/ai-sight-perception-to-custom-points/
bool ABasePawn::CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor, const bool* bWasVisible, int32* UserData) const
{
	if (!IsValid(GetWvSkeletalMeshComponent()))
	{
		return false;
	}

	static const FName NAME_AILineOfSight = FName(TEXT("TestPawnLineOfSight"));

	const auto SK = GetWvSkeletalMeshComponent();

	FHitResult HitResult;
	const TArray<USkeletalMeshSocket*> Sockets = SK->GetSkeletalMeshAsset()->GetActiveSocketList();

	FCollisionObjectQueryParams ObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic) | ECC_TO_BITFIELD(ECC_Pawn));
	FCollisionQueryParams QueryParams(NAME_AILineOfSight, true, IgnoreActor);

	for (int32 Index = 0; Index < Sockets.Num(); ++Index)
	{
		const FName& SocketName = Sockets[Index]->SocketName;
		if (SocketName.IsNone())
		{
			continue;
		}

		const FVector SocketLocation = SK->GetSocketLocation(SocketName);
		const bool bHitSocket = GetWorld()->LineTraceSingleByObjectType(
			HitResult, 
			ObserverLocation, 
			SocketLocation,
			ObjectQueryParams,
			QueryParams);

		NumberOfLoSChecksPerformed++;

		if (bHitSocket || (HitResult.GetActor() && HitResult.GetActor()->IsOwnedBy(this)))
		{
			OutSeenLocation = SocketLocation;
			OutSightStrength = 1;
			return true;
		}
	}

	HitResult.Reset();
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(
		HitResult, 
		ObserverLocation, 
		GetActorLocation(),
		ObjectQueryParams,
		QueryParams);

	NumberOfLoSChecksPerformed++;

	if (bHit || (HitResult.GetActor() && HitResult.GetActor()->IsOwnedBy(this)))
	{
		OutSeenLocation = GetActorLocation();
		OutSightStrength = 1;
		return true;
	}

	OutSightStrength = 0;
	return false;
}
#pragma endregion


void ABasePawn::NotifyControllerChanged()
{
	const FGenericTeamId OldTeamId = GetGenericTeamId();

	Super::NotifyControllerChanged();

	// Update our team ID based on the controller
	if (HasAuthority() && Controller != nullptr)
	{
		if (IWvAbilityTargetInterface* ControllerWithTeam = Cast<IWvAbilityTargetInterface>(Controller))
		{
			MyTeamID = ControllerWithTeam->GetGenericTeamId();
			ConditionalBroadcastTeamChanged(this, OldTeamId, MyTeamID);
		}

		if (WvFactionComponent)
		{
			WvFactionComponent->SetFactionOwnerActor(Controller);
		}
	}
	else if (HasAuthority() && WvFactionComponent)
	{
		WvFactionComponent->SetFactionOwnerActor(nullptr);
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

ULocomotionComponent* ABasePawn::GetLocomotionComponent() const
{
	return LocomotionComponent;
}

UWvFactionComponent* ABasePawn::GetFactionComponent() const
{
	return WvFactionComponent;
}

UWvRelationshipComponent* ABasePawn::GetRelationshipComponent() const
{
	return WvRelationshipComponent;
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

bool ABasePawn::IsHealthHalf() const
{
	return StatusComponent->IsHealthHalf();
}

bool ABasePawn::IsLeader() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(TAG_Character_AI_Leader);
	}
	return false;
}

bool ABasePawn::IsTargetLock() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(TAG_Character_TargetLocking);
	}
	return false;
}

bool ABasePawn::IsBotCharacter() const
{
	return UWvCommonUtils::IsBot(GetController());
}

#pragma region NearlestAction
/// <summary>
/// call to melee ability
/// </summary>
/// <param name="SyncPointWeight"></param>
void ABasePawn::CalcurateNearlestTarget(const float SyncPointWeight)
{
	const auto& LocomotionEssencialVariables = LocomotionComponent->GetLocomotionEssencialVariables();
	if (LocomotionEssencialVariables.LookAtTarget.IsValid())
	{
		FindNearestTarget(LocomotionEssencialVariables.LookAtTarget.Get(), SyncPointWeight);
	}
}

void ABasePawn::ResetNearlestTarget()
{
	MotionWarpingComponent->RemoveWarpTarget(NEARLEST_TARGET_SYNC_POINT);
}

void ABasePawn::FindNearestTarget(AActor* Target, const float SyncPointWeight)
{
	const FVector To = Target->GetActorLocation();
	FindNearestTarget(To, SyncPointWeight);
}

void ABasePawn::FindNearestTarget(const FVector TargetPosition, const float SyncPointWeight)
{
	const FVector From = GetActorLocation();
	const FVector To = TargetPosition;
	const float Weight = SyncPointWeight;
	ResetNearlestTarget();
	const FRotator TargetLookAt = UKismetMathLibrary::FindLookAtRotation(From, To);

	const FRotator Rotation = UKismetMathLibrary::RLerp(GetActorRotation(), TargetLookAt, Weight, true);
	FMotionWarpingTarget WarpingTarget;
	WarpingTarget.Name = NEARLEST_TARGET_SYNC_POINT;
	WarpingTarget.Location = FMath::Lerp(From, To, Weight);
	WarpingTarget.Rotation = FRotator(0.f, Rotation.Yaw, 0.f);
	MotionWarpingComponent->AddOrUpdateWarpTarget(WarpingTarget);

}

void ABasePawn::FindNearestTarget(const FAttackMotionWarpingData& AttackMotionWarpingData)
{
	auto Target = FindNearestTarget(AttackMotionWarpingData.NearlestDistance, AttackMotionWarpingData.AngleThreshold, false);
	if (!Target)
	{
		UE_LOG(LogBaseCharacter, Warning, TEXT("not found FindNearlestTarget => %s"), *FString(__FUNCTION__));
		return;
	}

	FindNearestTarget(Target, AttackMotionWarpingData.TargetSyncPointWeight);

}

const TArray<AActor*> ABasePawn::FindNearestTargets(const float Distance, const float AngleThreshold)
{
	// WorldDynamic and Pawn object type
	static const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { EObjectTypeQuery::ObjectTypeQuery2, EObjectTypeQuery::ObjectTypeQuery3 };

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	TArray<AActor*> HitTargets;
	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), GetActorLocation(), Distance, ObjectTypes, ABasePawn::StaticClass(), IgnoreActors, HitTargets);

	// Get the target with the smallest angle difference from the camera forward vector
	TArray<AActor*> FilterTargets;
	for (int32 Index = 0; Index < HitTargets.Num(); ++Index)
	{
		AActor* Target = HitTargets[Index];
		if (!IsValid(Target))
		{
			continue;
		}

		const FVector NormalizePos = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		const FVector Forward = GetActorForwardVector();
		const float Angle = UKismetMathLibrary::DegAcos(FVector::DotProduct(Forward, NormalizePos));
		const bool bIsTargetInView = (FMath::Abs(Angle) < AngleThreshold);
		if (bIsTargetInView)
		{
			FilterTargets.Add(Target);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			const FVector From = GetActorLocation();
			const FVector To = Target->GetActorLocation();
			DrawDebugSphere(GetWorld(), From, 20.f, 12, FColor::Blue, false, 2);
			DrawDebugSphere(GetWorld(), To, 20.f, 12, FColor::Blue, false, 2);
			DrawDebugDirectionalArrow(GetWorld(), From, To, 20.f, FColor::Red, false, 2);
#endif

		}
	}
	return FilterTargets;
}

/// <summary>
/// Find Target, for example HoldUp. Finisher. KnockOut.
/// </summary>
AActor* ABasePawn::FindNearestTarget(const float Distance, const float AngleThreshold, bool bTargetCheckBattled/* = true*/)
{
	TArray<AActor*> HitTargets = FindNearestTargets(Distance, AngleThreshold);

	if (HitTargets.Num() <= 0)
	{
		return nullptr;
	}

	HitTargets.RemoveAll([](AActor* Actor)
		{
			return Actor == nullptr;
		});

	// Sort by distance
	UWvCommonUtils::OrderByDistance(this, HitTargets, true);


	// Get the target with the smallest angle difference from the camera forward vector
	float ClosestDotToCenter = 0.f;
	ABasePawn* NearlestTarget = nullptr;

	for (int32 Index = 0; Index < HitTargets.Num(); ++Index)
	{
		if (ABasePawn* Target = Cast<ABasePawn>(HitTargets[Index]))
		{
			if (bTargetCheckBattled && Target->IsInBattled() || Target->IsDead())
			{
				continue;
			}

			const FVector NormalizePos = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			const FVector Forward = GetActorForwardVector();
			const float Dot = FVector::DotProduct(Forward, NormalizePos);
			if (Dot > ClosestDotToCenter)
			{
				ClosestDotToCenter = Dot;
				NearlestTarget = Target;
			}
		}
	}
	return NearlestTarget;
}


#pragma endregion


#pragma region Vehicle
void ABasePawn::BeginDrive()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::Interact);
	if (!ASC)
	{
		return;
	}

	GetCombatComponent()->VisibilityCurrentWeapon(true);
	ASC->AddGameplayTag(TAG_Vehicle_State_Drive, 1);
}

void ABasePawn::EndDrive()
{
	UWvAbilitySystemComponent* ASC = RequestAbilitySystemWarmup(EAbilitySystemLoadReason::Interact);
	if (!ASC)
	{
		return;
	}

	GetCombatComponent()->VisibilityCurrentWeapon(false);
	ASC->RemoveGameplayTag(TAG_Vehicle_State_Drive, 1);
}

bool ABasePawn::IsVehicleDriving() const
{
	const UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TAG_Vehicle_State_Drive);
}
#pragma endregion


/// <summary>
/// Animation overlay change from chooser table
/// </summary>
/// <param name="CurrentOverlay"></param>
const bool ABasePawn::OverlayStateChange(const ELSOverlayState CurrentOverlay)
{
	constexpr uint8 ELSOverlayState_Min = static_cast<uint8>(ELSOverlayState::None);
	constexpr uint8 ELSOverlayState_Max = static_cast<uint8>(ELSOverlayState::Mass);


	uint8 Raw = static_cast<uint8>(CurrentOverlay);
	Raw = FMath::Clamp(Raw, ELSOverlayState_Min, ELSOverlayState_Max);
	const ELSOverlayState ClampedOverlay = static_cast<ELSOverlayState>(Raw);

	if (SelectableOverlayState == ClampedOverlay)
	{
		return false;
	}

	bool bIsOverlayChange = false;
	const ELSOverlayState PrevOverlay = SelectableOverlayState;
	SelectableOverlayState = ClampedOverlay;

	OverlayChangeDelegate.Broadcast(ClampedOverlay);
	bIsOverlayChange = true;

#if false
	if (const UClass* OverlayAnimClass = UWvCommonUtils::FindClassInChooserTable(this, OverlayAnimationTable))
	{
		if (OverlayAnimClass->IsChildOf(UAnimInstance::StaticClass()))
		{
			TSubclassOf<UAnimInstance> Subclass = const_cast<UClass*>(OverlayAnimClass);
			GetWvSkeletalMeshComponent()->LinkAnimClassLayers(Subclass);
			OverlayChangeDelegate.Broadcast(ClampedOverlay);
			bIsOverlayChange = true;
		}
		else
		{
			UE_LOG(LogBaseCharacter, Warning, TEXT("OverlayAnimClass not class UAnimInstance: [%s]"), *FString(__FUNCTION__));
		}
	}
	else
	{
		const FString CategoryName = *FString::Format(TEXT("{0}"), { *GETENUMSTRING("/Script/Redemption.ELSOverlayState", SelectableOverlayState) });
		UE_LOG(LogBaseCharacter, Warning, TEXT("OverlayAnimClass not found:[%s] FindClassInChooserTable: [%s]"), *CategoryName, *FString(__FUNCTION__));
	}
#endif


	if (!bIsOverlayChange)
	{
		SelectableOverlayState = PrevOverlay;
		UE_LOG(LogBaseCharacter, Error, TEXT("Overlay Change Failed, function: [%s]"), *FString(__FUNCTION__));
	}

	return bIsOverlayChange;
}


void ABasePawn::SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale)
{
	AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
}

float ABasePawn::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}

/// <summary>
/// ref to Widget Overlay
/// </summary>
FTransform ABasePawn::GetPivotOverlayTansform() const
{
	if (!IsValid(GetWvSkeletalMeshComponent()))
	{
		return FTransform::Identity;
	}

	auto RootPos = GetWvSkeletalMeshComponent()->GetSocketLocation(TEXT("root"));
	auto HeadPos = GetWvSkeletalMeshComponent()->GetSocketLocation(TEXT("head"));
	TArray<FVector> Points({ RootPos, HeadPos, });
	auto AveragePoint = UKismetMathLibrary::GetVectorArrayAverage(Points);
	return FTransform(GetActorRotation(), AveragePoint, FVector::OneVector);
}


#pragma region Shape
void ABasePawn::SetGenderType(const EGenderType InGenderType)
{
	StatusComponent->SetGenderType(InGenderType);
}

EGenderType ABasePawn::GetGenderType() const
{
	return StatusComponent->GetGenderType();
}

void ABasePawn::SetBodyShapeType(const EBodyShapeType InBodyShapeType)
{
	if (IsValid(StatusComponent))
	{
		StatusComponent->SetBodyShapeType(InBodyShapeType);
	}
}

EBodyShapeType ABasePawn::GetBodyShapeType() const
{
	if (IsValid(StatusComponent))
	{
		return StatusComponent->GetBodyShapeType();
	}
	return EBodyShapeType::Normal;
}

FCharacterInfo ABasePawn::GetCharacterInfo() const
{
	return StatusComponent->GetCharacterInfo();
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


#pragma region AsyncLoad
void ABasePawn::RequestAsyncLoad()
{
	OnSyncLoadCompleteHandler();

	FStreamableManager& StreamableManager = UWvGameInstance::GetStreamableManager();

	TArray<FSoftObjectPath> TempPaths;
	for (TPair<FGameplayTag, TSoftObjectPtr<UDataAsset>>Pair : GameDataAssets)
	{
		if (Pair.Value.IsNull())
		{
			continue;
		}
		TempPaths.Add(Pair.Value.ToSoftObjectPath());
	}


	AsyncLoadStreamer = StreamableManager.RequestAsyncLoad(TempPaths, [this]
	{
		this->OnAsyncLoadCompleteHandler();
	},
	FStreamableManager::AsyncLoadHighPriority);

	RequestComponentsAsyncLoad();
}

void ABasePawn::RequestComponentsAsyncLoad()
{
	if (!bIsAllowAsyncLoadComponentAssets)
	{
		return;
	}

	/*
	* InventoryComponent
	* ClimbingComponent
	* LadderComponent
	* WvCharacterMovementComponent
	* QTEActionComponent (player only)
	*/

	auto Components = Game::ComponentExtension::GetComponentsArray<UActorComponent>(this);
	for (UActorComponent* ActComp : Components)
	{
		if (IAsyncComponentInterface* Interface = Cast<IAsyncComponentInterface>(ActComp))
		{
			Interface->RequestAsyncLoad();
		}
	}
}

void ABasePawn::OnAsyncLoadCompleteHandler()
{
	TakeDownActionDA = OnAsyncLoadDataAsset<UFinisherDataAsset>(TAG_Game_Asset_FinisherReceiver);
	CloseCombatDA = OnAsyncLoadDataAsset<UCloseCombatAnimationDataAsset>(TAG_Game_Asset_CloseCombat);
	HitReactionDA = OnAsyncLoadDataAsset<UWvHitReactDataAsset>(TAG_Game_Asset_HitReaction);

	// player only load
	CharacterVFXDA = OnAsyncLoadDataAsset<UCharacterVFXDataAsset>(TAG_Game_Asset_CharacterVFX);


	if (AsyncLoadStreamer.IsValid() && AsyncLoadStreamer->IsActive())
	{
		AsyncLoadStreamer->CancelHandle();
	}

	AsyncLoadStreamer.Reset();
	AsyncLoadCompleteDelegate.Broadcast();
}

void ABasePawn::OnSyncLoadCompleteHandler()
{

}

template<typename T>
T* ABasePawn::OnAsyncLoadDataAsset(const FGameplayTag Tag)
{
	if (auto SoftPtr = GameDataAssets.Find(Tag))
	{
		if (!SoftPtr->IsValid())
		{
			UE_LOG(LogBaseCharacter, Warning, TEXT("DataAsset for tag %s not yet loaded."), *Tag.ToString());
			return nullptr;
		}

		UObject* Obj = (*SoftPtr).Get();
		if (!IsValid(Obj))
		{
			UE_LOG(LogBaseCharacter, Error, TEXT("Loaded asset is invalid for tag %s."), *Tag.ToString());
			return nullptr;
		}

		T* DataAsset = Cast<T>(Obj);
		if (!DataAsset)
		{
			UE_LOG(LogBaseCharacter, Error, TEXT("Failed to cast DataAsset for tag %s to desired type."), *Tag.ToString());
		}
		return DataAsset;
	}
	return nullptr;

}

template<typename T>
T* ABasePawn::OnSyncLoadDataAsset(const FGameplayTag Tag)
{
	T* DataAsset = nullptr;

	if (auto Ptr = GameDataAssets.Find(Tag))
	{
		auto Obj = UKismetSystemLibrary::LoadAsset_Blocking(Ptr);
		DataAsset = Cast<T>(Obj);
	}

	if (IsValid(DataAsset))
	{
		UE_LOG(LogBaseCharacter, Warning, TEXT("%s Asset Load Complete %s => [%s]"), *GetNameSafe(this), *GetNameSafe(DataAsset), *FString(__FUNCTION__));
	}
	else
	{
		UE_LOG(LogBaseCharacter, Error, TEXT("%s Asset Load Fail %s => [%s]"), *GetNameSafe(this), *GetNameSafe(DataAsset), *FString(__FUNCTION__));
	}

	return DataAsset;
}
#pragma endregion


#pragma region CloseCombat
int32 ABasePawn::GetCombatAnimationIndex() const
{
	if (IsValid(CloseCombatDA))
	{
		auto BodyShape = GetBodyShapeType();
		return CloseCombatDA->GetCombatAnimationIndex(BodyShape);
	}
	return INDEX_NONE;
}

int32 ABasePawn::CloseCombatMaxComboCount(const int32 Index) const
{
	if (IsValid(CloseCombatDA))
	{
		auto BodyShape = GetBodyShapeType();
		return CloseCombatDA->CloseCombatMaxComboCount(BodyShape, Index);
	}
	return INDEX_NONE;
}

UAnimMontage* ABasePawn::GetCloseCombatAnimMontage(const int32 Index, const FGameplayTag Tag) const
{
	if (IsValid(CloseCombatDA))
	{
		auto BodyShape = GetBodyShapeType();
		return CloseCombatDA->GetAnimMontage(BodyShape, Index, Tag);
	}
	return nullptr;
}

float ABasePawn::CalcurateBodyShapePlayRate() const
{
	if (IsValid(CloseCombatDA))
	{
		auto BodyShape = GetBodyShapeType();
		return CloseCombatDA->CalcurateBodyShapePlayRate(BodyShape);
	}
	return 1.0f;
}

void ABasePawn::CalculateBackwardInputRotation()
{
	MotionWarpingComponent->RemoveWarpTarget(ABasePawn::BackwardInputSyncPoint);

	const auto& LocomotionEssencialVariables = LocomotionComponent->GetLocomotionEssencialVariables();

	const FVector Input = LocomotionEssencialVariables.MovementInput;
	const FRotator TargetRotation = FRotationMatrix::MakeFromX(Input).Rotator();


	MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		ABasePawn::BackwardInputSyncPoint,
		FVector::ZeroVector,
		TargetRotation);

}
#pragma endregion


