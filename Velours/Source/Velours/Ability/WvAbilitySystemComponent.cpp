// Copyright 2022 wevet works All Rights Reserved.

#include "WvAbilitySystemComponent.h"
#include "AnimNotify/WvAnimNotifyState.h"
#include "WvGameplayTargetData.h"
#include "WvGameplayEffectContext.h"
#include "WvAbilitySystemBlueprintFunctionLibrary.h"

// builtin
#include "AbilitySystemGlobals.h"
#include "Character/BasePawn.h"
#include "Character/WvPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvAbilitySystemComponent)

UWvAbilitySystemComponent::UWvAbilitySystemComponent() : Super()
{
	//
}


int32 UWvAbilitySystemComponent::GetDefaultAbilityLevel() const
{
	int32 Level = 1;
	if (ABasePawn* OwningCharacter = Cast<ABasePawn>(GetAvatarCharacter()))
	{
		//Level = OwningCharacter->GetParameterComponent() ? OwningCharacter->GetParameterComponent()->GetLevel() : Level;
	}
	return Level;
}


void UWvAbilitySystemComponent::AbilityNotifyBegin(class UWvAnimNotifyState* Notify, UWvGameplayAbility* DebugAbility)
{
	FAnimatingAbilityNotify& AnimNotify = AnimatingAbilityNotifys.AddDefaulted_GetRef();

	UWvGameplayAbility* NotifyAbility = DebugAbility;

	if (!NotifyAbility)
	{
		NotifyAbility = Cast<UWvGameplayAbility>(GetAnimatingAbility());
	}

	if (!NotifyAbility)
	{
		NotifyAbility = Cast<UWvGameplayAbility>(GetCurrentMoverMontageAbility());
	}

	AnimNotify.Ability = NotifyAbility;
	AnimNotify.Notify = Notify;
}

void UWvAbilitySystemComponent::AbilityNotifyEnd(class UWvAnimNotifyState* Notify)
{
	AnimatingAbilityNotifys.RemoveAll([&](const FAnimatingAbilityNotify& N) 
	{
		if (N.Notify == Notify)
		{
			return true;
		}
		return false;
	});
}

APawn* UWvAbilitySystemComponent::GetAvatarCharacter() const
{
	AActor* Avatar = GetAvatarActor();
	if (Avatar)
	{
		ABasePawn* Character = Cast<ABasePawn>(Avatar);
		return Character;
	}
	return nullptr;
}

const bool UWvAbilitySystemComponent::TryActivateAbilityByClassPressing(TSubclassOf<UGameplayAbility> InAbilityToActivate, bool bAllowRemoteActivation)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromClass(InAbilityToActivate);
	bool bIsPressing = true;
	if (GetAvatarCharacter())
	{
		if (AWvPlayerController* PC = Cast<AWvPlayerController>(GetAvatarCharacter()->GetController()))
		{
			FGameplayTag TriggerTag;
			for (FGameplayAbilitySpec& ActiveSpec : ActivatableAbilities.Items)
			{
				UWvAbilityDataAsset* AbilityData = CastChecked<UWvAbilityDataAsset>(ActiveSpec.SourceObject);

				if (ActiveSpec.Handle == Spec->Handle)
				{
					TriggerTag = AbilityData->ActiveTriggerTag;
					break;
				}
			}

			if (TriggerTag != FGameplayTag::EmptyTag)
			{
				bIsPressing = PC->GetInputEventComponent()->InputKeyDownControl(TriggerTag);
			}
		}
	}

	Spec->InputPressed = bIsPressing;

	if (Spec->IsActive())
	{
		const auto InstActivationInfo = Spec->Ability->GetCurrentActivationInfo();
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec->Handle, InstActivationInfo.GetActivationPredictionKey());
	}

	const bool bIsSucceed = TryActivateAbilityByClass(InAbilityToActivate, bAllowRemoteActivation);
	return bIsSucceed;
}


void UWvAbilitySystemComponent::OnTagUpdated(const FGameplayTag& Tag, bool TagExists)
{
	Super::OnTagUpdated(Tag, TagExists);
	AbilityTagUpdateDelegate.Broadcast(Tag, TagExists);
}


void UWvAbilitySystemComponent::SetCurrentMoverMontageAbility(
	UGameplayAbility* InAbility,
	UAnimMontage* InMontage)
{
	CurrentMoverMontageAbility = InAbility;
	CurrentMoverMontage = InMontage;
}

void UWvAbilitySystemComponent::ClearCurrentMoverMontageAbility(
	UGameplayAbility* InAbility,
	UAnimMontage* InMontage)
{
	if (CurrentMoverMontageAbility.Get() == InAbility && CurrentMoverMontage == InMontage)
	{
		CurrentMoverMontageAbility.Reset();
		CurrentMoverMontage = nullptr;
	}
}

UGameplayAbility* UWvAbilitySystemComponent::GetCurrentMoverMontageAbility() const
{
	return CurrentMoverMontageAbility.Get();
}

UAnimMontage* UWvAbilitySystemComponent::GetCurrentMoverMontage() const
{
	return CurrentMoverMontage;
}



