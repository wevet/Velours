// Copyright 2022 wevet works All Rights Reserved.

#include "WvAT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Character/BasePawn.h"
#include "Animation/AnimInstance.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvAT_PlayMontageAndWaitForEvent)


UWvAT_PlayMontageAndWaitForEvent::UWvAT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Rate = 1.0f;
	StartTimeSeconds = 0.0f;
	//bStopWhenAbilityEnds = true;
}

UWvAbilitySystemComponent* UWvAT_PlayMontageAndWaitForEvent::GetTargetAbilitySystemComponent()
{
	return Cast<UWvAbilitySystemComponent>(AbilitySystemComponent);
}

void UWvAT_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale	
			ABasePawn* Character = Cast<ABasePawn>(GetAvatarActor());

			if (Character && (Character->GetLocalRole() == ROLE_Authority 
				|| (Character->GetLocalRole() == ROLE_AutonomousProxy 
					&& Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}

		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}


void UWvAT_PlayMontageAndWaitForEvent::OnMontageBlendedIn(UAnimMontage* Montage)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendIn.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}


void UWvAT_PlayMontageAndWaitForEvent::OnAbilityCancelled()
{
	StopPlayingMontage();
	ClearCurrentMoverMontageAbility();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData Payload;
		Payload.OptionalObject = MontageToPlay;
		OnCancelled.Broadcast(FGameplayTag(), Payload);
	}

	EndTask();
}


void UWvAT_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	EndTask();
}

void UWvAT_PlayMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;
		EventReceived.Broadcast(EventTag, TempData);
	}
}

UWvAT_PlayMontageAndWaitForEvent* UWvAT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName, 
	UAnimMontage* MontageToPlay, 
	FGameplayTagContainer EventTags, 
	float Rate, 
	float StartTimeSeconds,
	FName StartSection, 
	bool bStopWhenAbilityEnds,
	float AnimRootMotionTranslationScale, 
	float StartingPosition)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UWvAT_PlayMontageAndWaitForEvent* Instance = NewAbilityTask<UWvAT_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	Instance->MontageToPlay = MontageToPlay;
	Instance->EventTags = EventTags;
	Instance->Rate = Rate;
	Instance->StartTimeSeconds = StartTimeSeconds;
	Instance->StartSection = StartSection;
	Instance->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	Instance->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	Instance->StartingPosition = StartingPosition;
	return Instance;
}



void UWvAT_PlayMontageAndWaitForEvent::Activate()
{
	if (!IsValid(Ability) || !MontageToPlay)
	{
		OnAbilityCancelled();
		return;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		OnAbilityCancelled();
		return;
	}

	UMoverComponent* MoverComponent = AvatarActor ? AvatarActor->FindComponentByClass<UMoverComponent>() : nullptr;

	if (!MoverComponent)
	{
		ABILITY_LOG(Warning, TEXT("MoverComponent not found."));
		OnAbilityCancelled();
		return;
	}

	UWvAbilitySystemComponent* ASC = GetTargetAbilitySystemComponent();
	if (!ASC)
	{
		ABILITY_LOG(Warning, TEXT("Invalid AbilitySystemComponent."));
		OnAbilityCancelled();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

	// Bind to event callback
	EventHandle = ASC->AddGameplayEventTagContainerDelegate(EventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UWvAT_PlayMontageAndWaitForEvent::OnGameplayEvent));

	float PlayRate = Rate;
	//const float Result = ASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, PlayRate, StartSection, StartTimeSeconds);
	MoverMontageProxy = UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
		MoverComponent,
		MontageToPlay,
		PlayRate,
		StartingPosition,
		StartSection);

	if (!MoverMontageProxy)
	{
		ABILITY_LOG(Warning, TEXT("Failed to create PlayMoverMontageCallbackProxy."));
		OnAbilityCancelled();
		return;
	}

	ASC->SetCurrentMoverMontageAbility(Ability, MontageToPlay);

	CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UWvAT_PlayMontageAndWaitForEvent::OnAbilityCancelled);

	MoverMontageProxy->OnCompleted.AddDynamic(this, &UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageCompleted);
	MoverMontageProxy->OnBlendOut.AddDynamic(this, &UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageBlendOut);
	MoverMontageProxy->OnInterrupted.AddDynamic(this, &UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageInterrupted);
	//MoverMontageProxy->OnCancelled.AddDynamic(this, &UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageCancelled);

	ASC->AbilityMontageBeginDelegate.Broadcast(Ability, MontageToPlay);

	SetWaitingOnAvatar();
}

void UWvAT_PlayMontageAndWaitForEvent::ClearCurrentMoverMontageAbility()
{
	if (UWvAbilitySystemComponent* ASC = GetTargetAbilitySystemComponent())
	{
		ASC->ClearCurrentMoverMontageAbility(Ability, MontageToPlay);
	}
}

void UWvAT_PlayMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());
	OnAbilityCancelled();
	Super::ExternalCancel();
}

void UWvAT_PlayMontageAndWaitForEvent::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}
	Super::OnDestroy(AbilityEnded);

}

bool UWvAT_PlayMontageAndWaitForEvent::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (!AnimInstance || !MontageToPlay)
	{
		return false;
	}

	if (!AnimInstance->Montage_IsPlaying(MontageToPlay))
	{
		return false;
	}

	AnimInstance->Montage_Stop(MontageToPlay->BlendOut.GetBlendTime(), MontageToPlay);
	return true;
}


FString UWvAT_PlayMontageAndWaitForEvent::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

		if (IsValid(AnimInstance))
		{
			if (MontageToPlay && AnimInstance->Montage_IsPlaying(MontageToPlay))
			{
				PlayingMontage = MontageToPlay;
			}
			else
			{
				PlayingMontage = AnimInstance->GetCurrentActiveMontage();
			}
		}
	}
	return FString::Printf(TEXT("PlayMontageAndWaitForEvent. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}


void UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageCompleted(FName NotifyName)
{
	ClearCurrentMoverMontageAbility();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
	}

	EndTask();
}

void UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageBlendOut(FName NotifyName)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageInterrupted(FName NotifyName)
{
	ClearCurrentMoverMontageAbility();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
	}

	EndTask();
}

void UWvAT_PlayMontageAndWaitForEvent::OnMoverMontageCancelled(FName NotifyName)
{
	ClearCurrentMoverMontageAbility();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData Payload;
		Payload.OptionalObject = MontageToPlay;
		OnCancelled.Broadcast(FGameplayTag(), Payload);
	}

	EndTask();
}


