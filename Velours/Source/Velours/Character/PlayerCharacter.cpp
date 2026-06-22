// Copyright 2022 wevet works All Rights Reserved.


#include "Character/PlayerCharacter.h"
#include "Velours.h"
#include "WvPlayerController.h"
#include "Component/WvSpringArmComponent.h"
#include "Component/InventoryComponent.h"
#include "Component/HitTargetComponent.h"
#include "Component/WvSkeletalMeshComponent.h"
#include "Component/QTEActionComponent.h"
#include "Mission/MinimapMarkerComponent.h"
#include "GameExtension.h"
#include "Ability/WvAbilitySystemComponent.h"
#include "Component/WvFactionComponent.h"
#include "Component/WvRelationshipComponent.h"

#include "Item/BulletHoldWeaponActor.h"

// built in
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GroomComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerCharacter)

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	MyTeamID = FGenericTeamId(1);
	CharacterTag = FGameplayTag::RequestGameplayTag(TAG_Character_Player.GetTag().GetTagName());

	QTEActionComponent = CreateDefaultSubobject<UQTEActionComponent>(TEXT("QTEActionComponent"));
	QTEActionComponent->bAutoActivate = 1;


#if WITH_EDITORONLY_DATA
	// https://forums.unrealengine.com/t/world-partion-current-pawn-vanishes-when-reaching-cell-loading-range-limit/255655/7
	bIsSpatiallyLoaded = false;
#endif

	if (WvRelationshipComponent)
	{
		WvRelationshipComponent->bRelationshipEnabled = false;
	}

	// set mini map tag
	MinimapMarkerComponent->MiniMapMakerTag = TAG_Game_Minimap_Player;
	MinimapMarkerComponent->SetVisibleMakerTag(false);

	AbilitySystemCreationPolicy = EAbilitySystemCreationPolicy::Always;
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{

	check(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{

	}
}

#if WITH_EDITOR
void APlayerCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
}
#endif

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	QTEActionComponent->AddTickPrerequisiteActor(this);

	CameraBoom = FindComponentByClass<UWvSpringArmComponent>();

	{
		auto Components = GetComponentsByTag(UCameraComponent::StaticClass(), TEXT("TPS Camera"));
		for (auto Comp : Components)
		{
			if (UCameraComponent* CamComp = Cast<UCameraComponent>(Comp))
			{
				TPSCamera = CamComp;
			}

		}
	}
	
	

	QTEActionComponent->QTEBeginDelegate.AddUniqueDynamic(this, &ThisClass::OnQTEBegin_Callback);
	QTEActionComponent->QTEEndDelegate.AddUniqueDynamic(this, &ThisClass::OnQTEEnd_Callback);

}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AWvPlayerController* PC = Cast<AWvPlayerController>(Controller))
	{
		PC->OnInputEventGameplayTagTrigger_Game.AddUniqueDynamic(this, &ThisClass::GameplayTagTrigger_Callback);
		PC->OnPluralInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnPluralInputEventTrigger_Callback);
		PC->OnHoldingInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnHoldingInputEventTrigger_Callback);
		PC->OnDoubleClickInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnDoubleClickInputEventTrigger_Callback);


		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			FModifyContextOptions Options;
			//Subsystem->AddMappingContext(DefaultMappingContext, 0, Options);
		}
	}
}

void APlayerCharacter::UnPossessed()
{
	if (AWvPlayerController* PC = Cast<AWvPlayerController>(Controller))
	{
		PC->OnInputEventGameplayTagTrigger_Game.AddUniqueDynamic(this, &ThisClass::GameplayTagTrigger_Callback);
		PC->OnPluralInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnPluralInputEventTrigger_Callback);
		PC->OnHoldingInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnHoldingInputEventTrigger_Callback);
		PC->OnDoubleClickInputEventTrigger.AddUniqueDynamic(this, &ThisClass::OnDoubleClickInputEventTrigger_Callback);


		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			FModifyContextOptions Options;
			//Subsystem->RemoveMappingContext(DefaultMappingContext, Options);
		}
	}

	Super::UnPossessed();
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	QTEActionComponent->QTEBeginDelegate.RemoveDynamic(this, &ThisClass::OnQTEBegin_Callback);
	QTEActionComponent->QTEEndDelegate.RemoveDynamic(this, &ThisClass::OnQTEEnd_Callback);

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::NotifyControllerChanged()
{
	TryNotifyControllerAbilitySystemInitialized();
}

void APlayerCharacter::PostAbilitySystemInitialize()
{
	TryNotifyControllerAbilitySystemInitialized();
	//UE_LOG(LogBaseCharacter, Log, TEXT("[%s]"), *FString(__FUNCTION__));
}

void APlayerCharacter::TryNotifyControllerAbilitySystemInitialized()
{
	if (AWvPlayerController* PC = Cast<AWvPlayerController>(Controller))
	{
		UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : Success CtrlName => %s"), *FString(__FUNCTION__), *GetNameSafe(PC));
		PC->PostAbilitySystemInitialize(AbilitySystemComponent);

		P_Controller = PC;
	}
	else
	{
		if (IsValid(Controller))
		{
			UE_LOG(LogBaseCharacter, Error, TEXT("[%s] : Failed CtrlName => %s"), *FString(__FUNCTION__), *GetNameSafe(Controller));
		}
	}

}

bool APlayerCharacter::GetOverlayMenuOpen() const
{
	if (IsValid(P_Controller))
	{
		return P_Controller->GetOverlayMenuOpen();
	}
	return false;
}


#pragma region IWvKeyableTargetInterface
void APlayerCharacter::SetKeyInputEnable()
{
	UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	if (ASC)
	{
		ASC->RemoveGameplayTag(TAG_Game_Input_Disable, 1);
	}

}

void APlayerCharacter::SetKeyInputDisable()
{
	UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	if (ASC)
	{
		ASC->AddGameplayTag(TAG_Game_Input_Disable, 1);
	}
}

bool APlayerCharacter::IsInputKeyDisable() const
{
	UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	if (ASC)
	{
		return ASC->HasMatchingGameplayTag(TAG_Game_Input_Disable);
	}
	return false;
}
#pragma endregion


void APlayerCharacter::HandleJump(const bool bIsPress)
{
	//const auto CMC = GetWvCharacterMovementComponent();

	if (bIsPress)
	{
	}
	else
	{
	}
}

void APlayerCharacter::HandleSprinting(const bool bIsPress)
{
	if (bIsPress)
	{
	}
	else
	{
	}
}

void APlayerCharacter::HandleWalking(const bool bIsPress)
{
	if (bIsPress)
	{
	}
}

void APlayerCharacter::HandleMeleeAction(const bool bIsPress)
{
	auto Inventory = GetInventoryComponent();

	if (bIsPress)
	{
		if (Super::IsMeleeAttacking() && !Inventory->CanAimingWeapon())
		{
			//const auto Tag = TAG_Character_StateMelee.GetTag().GetTagName();
			//UE_LOG(LogTemp, Warning, TEXT("Returns nothing as tags are added. %s => [%s]"), *Tag.ToString(), *FString(__FUNCTION__));
			return;
		}

		if (Inventory->CanAimingWeapon())
		{
			DoBulletAttack();
			return;
		}

		Super::DoAttack();
	}
	else
	{
		if (Inventory->CanAimingWeapon())
		{
			Clear_BulletTimer();
		}
	}
}

void APlayerCharacter::HandleDriveAction(const bool bIsPress)
{
	if (bIsPress)
	{
	}
}

void APlayerCharacter::HandleAliveAction(const bool bIsPress)
{
	if (bIsPress)
	{
	}
}

void APlayerCharacter::HandleHoldAimAction(const bool bIsPress)
{
	if (bIsPress)
	{
	}
	else
	{
	}
}

void APlayerCharacter::HandleQTEAction(const bool bIsPress)
{
	if (IsQTEActionPlaying())
	{
		if (bIsPress)
		{
			QTEActionComponent->InputPress();
		}

	}

}

bool APlayerCharacter::IsQTEActionPlaying() const
{
	const UWvAbilitySystemComponent* ASC = GetWvAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TAG_Character_Action_QTE);
}


void APlayerCharacter::HandleFinisherAction(const FGameplayTag Tag, const bool bIsPress)
{
	if (bIsPress)
	{
		const bool bIsEquipBulletWeapon = GetInventoryComponent()->CanAimingWeapon();
		if (bIsEquipBulletWeapon)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot be used if equipped with a pistol or rifle. => %s"), *FString(__FUNCTION__));
			return;
		}

		//BuildFinisherAbility(Tag);
	}
}

void APlayerCharacter::HandleStanceMode()
{
}

void APlayerCharacter::HandleTargetLock()
{
}

void APlayerCharacter::HandleRotationMode()
{
}

bool APlayerCharacter::HasFinisherAction(const FGameplayTag Tag) const
{
	FGameplayTagContainer Container;
	Container.AddTag(TAG_Weapon_Finisher);
	Container.AddTag(TAG_Weapon_HoldUp);
	Container.AddTag(TAG_Weapon_KnockOut);
	return Container.HasTag(Tag);
}

void APlayerCharacter::DoBulletAttack_Callback()
{
	const ABulletHoldWeaponActor* BulletWeapon = Cast<ABulletHoldWeaponActor>(GetInventoryComponent()->GetEquipWeapon());
	if (!BulletWeapon->IsAvailable())
	{
		Clear_BulletTimer();
		return;
	}
	//Super::DoBulletAttack();
}

void APlayerCharacter::Clear_BulletTimer()
{
	FTimerManager& TM = GetWorld()->GetTimerManager();
	if (TM.IsTimerActive(Bullet_TimerHandle))
	{
		TM.ClearTimer(Bullet_TimerHandle);
	}

}


void APlayerCharacter::GameplayTagTrigger_Callback(const FGameplayTag Tag, const bool bIsPress)
{
	if (IsInputKeyDisable())
	{
		return;
	}

	if (Tag == TAG_Character_ActionJump)
	{
		HandleJump(bIsPress);
	}
	else if (Tag == TAG_Character_ActionDash)
	{
		HandleSprinting(bIsPress);
	}
	else if (Tag == TAG_Character_ActionWalk)
	{
		HandleWalking(bIsPress);
	}
	else if (Tag == TAG_Character_ActionDrive)
	{
		HandleDriveAction(bIsPress);
	}
	else if (Tag == TAG_Character_Player_Melee)
	{
		HandleMeleeAction(bIsPress);
	}
	else if (Tag == TAG_Character_StateAlive_Trigger)
	{
		HandleAliveAction(bIsPress);
	}
	else if (Tag == TAG_Character_ActionAimChange)
	{
		HandleHoldAimAction(bIsPress);
	}

	if (bIsPress)
	{
		UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : TagName => %s"), *FString(__FUNCTION__), *Tag.ToString());
	}

}

void APlayerCharacter::OnPluralInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress)
{
	if (IsInputKeyDisable())
	{
		return;
	}

	if (Tag == TAG_Character_ActionCrouch)
	{
		HandleStanceMode();
	}
	else if (Tag == TAG_Character_TargetLock)
	{
		HandleTargetLock();
	}
	else if (Tag == TAG_Character_ActionStrafeChange)
	{
		HandleRotationMode();
	}
	else if (Tag == TAG_Character_Action_QTE_Pressed)
	{
		HandleQTEAction(bIsPress);
	}
	else if (HasFinisherAction(Tag) && !Super::IsVehicleDriving())
	{
		HandleFinisherAction(Tag, bIsPress);
	}

	if (bIsPress)
	{
		UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : TagName => %s"), *FString(__FUNCTION__), *Tag.ToString());
	}
}

void APlayerCharacter::OnHoldingInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress)
{
	if (IsInputKeyDisable())
	{
		return;
	}

	UE_LOG(LogBaseCharacter, Warning, TEXT("[%s]"), *FString(__FUNCTION__));
}

void APlayerCharacter::OnDoubleClickInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress)
{
	if (IsInputKeyDisable())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s]"), *FString(__FUNCTION__));
}

// callback
void APlayerCharacter::OverlayStateChange_Callback(const ELSOverlayState PrevOverlay, const ELSOverlayState CurrentOverlay)
{
	if (!IsValid(ItemInventoryComponent))
	{
		return;
	}

	bool bCanAttack = false;
	const auto WeaponType = ItemInventoryComponent->ConvertWeaponState(CurrentOverlay, bCanAttack);
	const bool bResult = ItemInventoryComponent->ChangeAttackWeapon(WeaponType);

	if (bResult)
	{
		//Super::OverlayStateChange(CurrentOverlay);
	}

}

void APlayerCharacter::OnTargetLockedOn_Callback(AActor* LookOnTarget, UHitTargetComponent* TargetComponent)
{
}

void APlayerCharacter::OnTargetLockedOff_Callback(AActor* LookOnTarget, UHitTargetComponent* TargetComponent)
{
}

void APlayerCharacter::OnQTEBegin_Callback()
{

}

void APlayerCharacter::OnQTEEnd_Callback(const bool bIsSuccess)
{


}

void APlayerCharacter::NotifyRegisterMission(const int32 MissionIndex)
{
}


