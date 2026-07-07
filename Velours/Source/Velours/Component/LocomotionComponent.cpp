// Copyright 2022 wevet works All Rights Reserved.


#include "Component/LocomotionComponent.h"
#include "Component/HitTargetComponent.h"
#include "Character/BasePawn.h"

#include "Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocomotionComponent)


FGameplayTag ULocomotionStateDataAsset::FindMovementModeTag(const FName& MovementModeName) const
{
	return MovementModeTagMap.FindRef(MovementModeName);
}



ULocomotionComponent::ULocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULocomotionComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ABasePawn>(GetOwner());
}

void ULocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateLocomotionState(DeltaTime);
}

void ULocomotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Character.Reset();
	Super::EndPlay(EndPlayReason);
}

void ULocomotionComponent::SetStanceMode(const ELSStance NewLSStance)
{
	LocomotionEssencialVariables.LSStance = NewLSStance;
}

void ULocomotionComponent::SetRotationMode(const ELSRotationMode NewLSRotationMode)
{
	LocomotionEssencialVariables.LSRotationMode = NewLSRotationMode;
}

void ULocomotionComponent::SetOverlayState(const ELSOverlayState NewLSOverlayState)
{
	if (LocomotionEssencialVariables.OverlayState == NewLSOverlayState)
	{
		return;
	}

	const ELSOverlayState PrevOverlay = LocomotionEssencialVariables.OverlayState;
	LocomotionEssencialVariables.OverlayState = NewLSOverlayState;
	if (OnOverlayChangeDelegate.IsBound())
	{
		OnOverlayChangeDelegate.Broadcast(PrevOverlay, NewLSOverlayState);
	}
}

void ULocomotionComponent::SetGaitMode(const ELSGait NewGait)
{
	LocomotionEssencialVariables.LSGait = NewGait;
}

void ULocomotionComponent::SetLookAimTarget(const bool NewLookAtAimOffset, AActor* NewLookAtTarget)
{
	LocomotionEssencialVariables.bLookAtAimOffset = NewLookAtAimOffset && IsValid(NewLookAtTarget);
	LocomotionEssencialVariables.LookAtTarget = NewLookAtTarget;
	LocomotionEssencialVariables.LookAtTargetComponent = (NewLookAtAimOffset && IsValid(NewLookAtTarget)) ?
		NewLookAtTarget->FindComponentByClass<UHitTargetComponent>() : nullptr;
}

void ULocomotionComponent::UpdateLookAimTargetComponent(UHitTargetComponent* NewLookAtTargetComponent)
{
	if (LocomotionEssencialVariables.bLookAtAimOffset)
	{
		LocomotionEssencialVariables.LookAtTargetComponent = NewLookAtTargetComponent;
	}

}


void ULocomotionComponent::UpdateLocomotionState(const float DeltaTime)
{
	if (!Character.IsValid())
	{
		return;
	}

	const float PrevAimYaw = LocomotionEssencialVariables.LookingRotation.Yaw;
	const FRotator CurrentLockingRotation = LocomotionEssencialVariables.LookingRotation;
	if (LocomotionEssencialVariables.bLookAtAimOffset)
	{
		const FVector Start = Character->GetActorLocation();
		const FVector Target = ChooseTargetPosition();
		LocomotionEssencialVariables.LookingRotation = UKismetMathLibrary::FindLookAtRotation(Start, Target);
	}
	else
	{
		LocomotionEssencialVariables.LookingRotation = Character->GetControlRotation();
	}


	LocomotionEssencialVariables.MovementInput = Character->GetLastMovementInputVector();
}

FVector ULocomotionComponent::ChooseTargetPosition() const
{
	// 1st target Component
	// 2nd Owner ActorLocation
	if (LocomotionEssencialVariables.LookAtTargetComponent.IsValid())
	{
		auto Comp = LocomotionEssencialVariables.LookAtTargetComponent.Get();
		return Comp->GetComponentLocation();
	}
	return LocomotionEssencialVariables.LookAtTarget.IsValid() ? LocomotionEssencialVariables.LookAtTarget->GetActorLocation() : FVector::ZeroVector;
}

void ULocomotionComponent::OnMovementModeChanged(
	const FName& PreviousModeName,
	const FName& NewModeName,
	FGameplayTag& OutPrevMovementModeTag,
	FGameplayTag& OutNextMovementModeTag)
{

	if (LocomotionStateDA)
	{
		OutPrevMovementModeTag = LocomotionStateDA->FindMovementModeTag(PreviousModeName);
		OutNextMovementModeTag = LocomotionStateDA->FindMovementModeTag(NewModeName);

		UE_LOG(LogBaseCharacter, Log, TEXT("[%s] : PreviousModeName => %s, NewModeName => %s"), 
			*FString(__FUNCTION__), 
			*PreviousModeName.ToString(),
			*NewModeName.ToString());

	}
}

void ULocomotionComponent::SetAiming(const bool bIsNewAiming)
{
	const bool bIsAiming = LocomotionEssencialVariables.bAiming;
	if (bIsAiming != bIsNewAiming)
	{
		LocomotionEssencialVariables.bAiming = bIsNewAiming;
		//
	}

}

