// Copyright 2022 wevet works All Rights Reserved.

#include "WvPlayerController.h"
#include "PlayerCharacter.h"
#include "Vehicle/WvWheeledVehiclePawn.h"
#include "Vehicle/VehicleUIController.h"
#include "Component/QTEActionComponent.h"
#include "UI/UMGManager.h"
#include "UI/MinimapUIController.h"
#include "Game/WvGameInstance.h"
#include "Game/CharacterInstanceSubsystem.h"
#include "Game/MinimapCaptureActor.h"

#include "Engine/World.h"
#include "BasePlayerState.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(WvPlayerController)


AWvPlayerController::AWvPlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WvInputEventComponent = ObjectInitializer.CreateDefaultSubobject<UWvInputEventComponent>(this, TEXT("WvInputEventComponent"));
	WvInputEventComponent->bAutoActivate = 1;

	MissionComponent = ObjectInitializer.CreateDefaultSubobject<UMissionComponent>(this, TEXT("MissionComponent"));
	MissionComponent->bAutoActivate = 1;

	bCanPossessWithoutAuthority = true;
}

void AWvPlayerController::BeginPlay()
{
	Super::BeginPlay();

	MissionComponent->BeginMissionDelegate.AddUniqueDynamic(this, &ThisClass::BeginMission_Callback);
	MissionComponent->RegisterMissionDelegate.AddUniqueDynamic(this, &ThisClass::RegisterMission_Callback);
}

void AWvPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Manager = nullptr;
	MinimapCaptureActor = nullptr;
	UMGManager, VehicleUIController = nullptr;

	MissionComponent->BeginMissionDelegate.RemoveDynamic(this, &ThisClass::BeginMission_Callback);
	MissionComponent->RegisterMissionDelegate.AddUniqueDynamic(this, &ThisClass::RegisterMission_Callback);

	Super::EndPlay(EndPlayReason);
}

void AWvPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsValid(InPawn))
	{
		return;
	}

	OverrideSquadID = FMath::Clamp(OverrideSquadID, 1, 255);
	auto PS = Cast<ABasePlayerState>(PlayerState);
	if (PS)
	{
		PS->SetSquadID(OverrideSquadID);
		PS->SetGenericTeamId(FGenericTeamId(OverrideSquadID));
	}

	if (IWvAbilityTargetInterface* TeamAgent = Cast<IWvAbilityTargetInterface>(InPawn))
	{
		TeamAgent->SetGenericTeamId(FGenericTeamId(OverrideSquadID));
	}

	if (!IsValid(Manager))
	{
		Manager = Cast<AWvPlayerCameraManager>(PlayerCameraManager);
	}


	if (InPawn->IsA(APlayerCharacter::StaticClass()))
	{
		PC = Cast<APlayerCharacter>(InPawn);
		OnDefaultPossess(InPawn);
	}
	else if (InPawn->IsA(AWvWheeledVehiclePawn::StaticClass()))
	{
		VPC = Cast<AWvWheeledVehiclePawn>(InPawn);
		OnVehilcePossess(InPawn);
	}

#if false
	if (!IsValid(MinimapCaptureActor))
	{
		AsyncLoadMinimapActor(InPawn);
	}
	else
	{
		MinimapCaptureActor->Initializer(InPawn);
		AddMinimapHiddenActor(InPawn);
	}
#endif

}

void AWvPlayerController::OnUnPossess()
{
	if (VPC)
	{
		OnVehicleUnPossess();
	}
	if (PC)
	{
		OnDefaultUnPossess();
	}
	PC = nullptr;
	VPC = nullptr;
	Super::OnUnPossess();
}

void AWvPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MinimapCaptureActor)
	{
		MinimapCaptureActor->UpdateCapture(DeltaTime);
	}
}


void AWvPlayerController::PostAbilitySystemInitialize(UAbilitySystemComponent* ASC)
{
	WvInputEventComponent->PostASCInitialize(ASC);

	UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : created InputEventComponent"), *FString(__FUNCTION__));
}

bool AWvPlayerController::IsPawnInputKeyDisable() const
{
	const APawn* CurrentPawn = GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return false;
	}

	const IWvKeyableTargetInterface* Interface = Cast<IWvKeyableTargetInterface>(GetPawn());
	if (Interface)
	{
		return Interface->IsInputKeyDisable();
	}
	return false;
}

void AWvPlayerController::SetPawnKeyInput(const bool bIsDisable)
{
	const APawn* CurrentPawn = GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return;
	}

	IWvKeyableTargetInterface* Interface = Cast<IWvKeyableTargetInterface>(GetPawn());
	if (Interface)
	{
		if (bIsDisable)
		{
			Interface->SetKeyInputDisable();
		}
		else
		{
			Interface->SetKeyInputEnable();
		}

	}
}

void AWvPlayerController::InitInputSystem()
{
	Super::InitInputSystem();
	WvInputEventComponent->BindInputEvent(InputComponent.Get());
}

bool AWvPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	WvInputEventComponent->InputKey(Params);
	return Super::InputKey(Params);
}

void AWvPlayerController::SetInputModeType(const EWvInputMode NewInputMode)
{
	WvInputEventComponent->SetInputModeType(NewInputMode);
}

EWvInputMode AWvPlayerController::GetInputModeType() const
{
	return WvInputEventComponent->GetInputModeType();
}


#pragma region IWvAbilityTargetInterface
void AWvPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogTemp, Error, TEXT("You can't set the team ID on a player controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

FGenericTeamId AWvPlayerController::GetGenericTeamId() const
{
	if (const IWvAbilityTargetInterface* TeamAgent = Cast<IWvAbilityTargetInterface>(PlayerState))
	{
		return TeamAgent->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

FOnTeamIndexChangedDelegate* AWvPlayerController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AWvPlayerController::OnReceiveKillTarget(AActor* Actor, const float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("Pawn is neutralized and AIPerception is stopped. => %s, Pawn => %s"), *FString(__FUNCTION__), *GetNameSafe(GetPawn()));
}

void AWvPlayerController::OnSendKillTarget(AActor* Actor, const float Damage)
{
}

void AWvPlayerController::Freeze()
{
}

void AWvPlayerController::UnFreeze()
{
}

bool AWvPlayerController::IsInBattled() const
{
	return UCharacterInstanceSubsystem::Get()->IsInBattleAny();
}
#pragma endregion


#pragma region Minimap
void AWvPlayerController::AsyncLoadMinimapActor(APawn* InPawn)
{

	if (!MinimapCaptureActorTemplate.IsNull())
	{

		FStreamableManager& StreamableManager = UWvGameInstance::GetStreamableManager();
		const FSoftObjectPath ObjectPath = MinimapCaptureActorTemplate.ToSoftObjectPath();
		MinimapStreamableHandle = StreamableManager.RequestAsyncLoad(ObjectPath, [this, InPawn]
		{
			this->OnMinimapLoadComplete(InPawn);
		});
	}
}

void AWvPlayerController::OnMinimapLoadComplete(APawn* InPawn)
{
	if (!MinimapStreamableHandle.Get())
	{
		return;
	}

	UObject* LoadedObj = MinimapStreamableHandle.Get()->GetLoadedAsset();
	if (LoadedObj)
	{
		UClass* ObjectClass = Cast<UClass>(LoadedObj);
		if (ObjectClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FTransform Transform = InPawn->GetActorTransform();

			MinimapCaptureActor = GetWorld()->SpawnActor<AMinimapCaptureActor>(ObjectClass, Transform, SpawnParams);
			if (MinimapCaptureActor)
			{
				MinimapCaptureActor->Initializer(InPawn);
				AddMinimapHiddenActor(InPawn);
				MinimapStreamableHandle.Reset();
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("cast failed ObjectClass => [%s]"), *FString(__FUNCTION__));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("not valid LoadedObj => [%s]"), *FString(__FUNCTION__));
	}
}

void AWvPlayerController::AddMinimapHiddenActor(AActor* NewHiddenActor)
{
	if (MinimapCaptureActor)
	{
		MinimapCaptureActor->AddMinimapHiddenActor(NewHiddenActor);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("nullptr MinimapCaptureActor async loading... => [%s]"), *FString(__FUNCTION__));
	}
}

void AWvPlayerController::RemoveMinimapHiddenActor(AActor* NewHiddenActor)
{
	if (MinimapCaptureActor)
	{
		MinimapCaptureActor->RemoveMinimapHiddenActor(NewHiddenActor);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("nullptr MinimapCaptureActor async loading... => [%s]"), *FString(__FUNCTION__));
	}
}
#pragma endregion


#pragma region Possess
/// <summary>
/// async main ui load
/// </summary>
/// <param name="InPawn"></param>
void AWvPlayerController::OnDefaultPossess(APawn* InPawn)
{
	if (IsValid(UMGManager))
	{
		UMGManager->VisibleCombatUI(true);
		BP_DefaultPossess(InPawn);
		return;
	}

	// @NOTE
	// MinimapはUMGManagerの子供になるのでUMGManagerが初期化済みの場合は非同期処理を行う必要がない。
	if (!UMGManagerTemplate.IsNull())
	{
		FStreamableManager& StreamableManager = UWvGameInstance::GetStreamableManager();
		const FSoftObjectPath ObjectPath = UMGManagerTemplate.ToSoftObjectPath();
		MainUIStreamableHandle = StreamableManager.RequestAsyncLoad(ObjectPath, [this, InPawn]
		{
			this->OnMainUILoadComplete(InPawn);
		});
	}

}

void AWvPlayerController::OnMainUILoadComplete(APawn* InPawn)
{
	if (MainUIStreamableHandle.Get())
	{
		UObject* LoadedObj = MainUIStreamableHandle.Get()->GetLoadedAsset();
		if (LoadedObj)
		{
			UClass* WidgetClass = Cast<UClass>(LoadedObj);
			if (WidgetClass)
			{
				UMGManager = Cast<UUMGManager>(CreateWidget<UUserWidget>(this, WidgetClass));
				if (IsValid(UMGManager))
				{
					UMGManager->Initializer(PC);
					UMGManager->AddToViewport((int32)EUMGLayerType::Default);

					BP_DefaultPossess(InPawn);
					MainUIStreamableHandle.Reset();
				}
			}
		}
	}

}

void AWvPlayerController::OnDefaultUnPossess()
{
	if (UMGManager)
	{
		UMGManager->VisibleCombatUI(false);
	}
	BP_DefaultUnPossess();
}

/// <summary>
/// async vehicle ui load
/// </summary>
/// <param name="InPawn"></param>
void AWvPlayerController::OnVehilcePossess(APawn* InPawn)
{
	if (!VehicleUIControllerTemplate.IsNull())
	{
		FStreamableManager& StreamableManager = UWvGameInstance::GetStreamableManager();
		const FSoftObjectPath ObjectPath = VehicleUIControllerTemplate.ToSoftObjectPath();
		VehicleUIStreamableHandle = StreamableManager.RequestAsyncLoad(ObjectPath, [this, InPawn]
		{
			this->OnVehilceUILoadComplete(InPawn);
		});
	}

}

void AWvPlayerController::OnVehilceUILoadComplete(APawn* InPawn)
{
	if (VehicleUIStreamableHandle.Get())
	{
		UObject* LoadedObj = VehicleUIStreamableHandle.Get()->GetLoadedAsset();
		if (LoadedObj)
		{
			UClass* WidgetClass = Cast<UClass>(LoadedObj);
			if (WidgetClass)
			{
				VehicleUIController = Cast<UVehicleUIController>(CreateWidget<UUserWidget>(this, WidgetClass));
				if (IsValid(VehicleUIController))
				{
					VehicleUIController->Initializer(VPC);
					VehicleUIController->AddToViewport((int32)EUMGLayerType::Vehicle);

					BP_VehilcePossess(InPawn);
					VehicleUIStreamableHandle.Reset();
				}
			}
		}
	}

}

void AWvPlayerController::OnVehicleUnPossess()
{
	if (VehicleUIController)
	{
		VehicleUIController->RemoveFromParent();
	}
	VehicleUIController = nullptr;
	BP_VehicleUnPossess();
}
#pragma endregion


class UWvInputEventComponent* AWvPlayerController::GetInputEventComponent() const
{
	return WvInputEventComponent;
}

class UMissionComponent* AWvPlayerController::GetMissionComponent() const
{
	return MissionComponent;
}

FVector AWvPlayerController::GetCameraForwardVector() const
{
	return Manager ? Manager->GetTransformComponent()->GetForwardVector() : FVector::ZeroVector;
}

void AWvPlayerController::RegisterMission_Callback(const int32 MissionIndex)
{
	if (IsValid(PC))
	{
		PC->NotifyRegisterMission(MissionIndex);
	}
}

void AWvPlayerController::BeginMission_Callback(const int32 MissionIndex)
{
	if (IsValid(PC))
	{
		PC->NotifyRegisterMission(MissionIndex);
	}
}

void AWvPlayerController::BeginQTEWithSetParameters(const float InTimer, const float InCount)
{
	if (IsValid(PC))
	{
		PC->GetQTEActionComponent()->SetParameters(InTimer, InCount);
		BeginQTE();
	}
}

void AWvPlayerController::BeginQTE()
{
	if (IsValid(PC))
	{
		EQTEType QTEType = EQTEType::None;
		if (PC->IsInBattled())
		{
			QTEType = EQTEType::Battle;
		}

		PC->GetQTEActionComponent()->Begin(QTEType);
	}
}

void AWvPlayerController::EndQTE()
{
	if (IsValid(PC))
	{
		PC->GetQTEActionComponent()->End();
	}
}

void AWvPlayerController::SetOverlayMenuOpen(const bool bIsNewOverlayMenuOpen)
{
	bIsOverlayMenuOpen = bIsNewOverlayMenuOpen;
}

bool AWvPlayerController::GetOverlayMenuOpen() const
{
	return bIsOverlayMenuOpen; 
}


