// Copyright 2022 wevet works All Rights Reserved.


#include "Ability/WvAbility_BasicAction.h"
#include "Velours.h"
#include "Character/BasePawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvAbility_BasicAction)


UWvAbility_BasicAction::UWvAbility_BasicAction(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	bRetriggerInstancedAbility = true;
}



void UWvAbility_BasicAction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Character = GetBaseCharacter();
	if (!Character.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s : Character is null.]"), *FString(__FUNCTION__));
		CancelAbility(Handle, ActorInfo, ActivationInfo, false);
		return;
	}


	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsValid(BasicActionMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s : BasicActionMontage is null.]"), *FString(__FUNCTION__));
		CancelAbility(Handle, ActorInfo, ActivationInfo, false);
		return;
	}

	if (MontageTask)
	{
		MontageTask->OnCompleted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->EndTask();
	}


	MontageTask = UWvAT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this,
		FName("BasicAction"),
		BasicActionMontage,
		FGameplayTagContainer(),
		1.0, 
		0.f,
		FName("Default"), 
		true, 
		1.0f);

	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnPlayMontageCompleted_Event);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnPlayMontageCompleted_Event);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnPlayMontageCompleted_Event);
	MontageTask->ReadyForActivation();

}


void UWvAbility_BasicAction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UWvAbility_BasicAction::OnPlayMontageCompleted_Event(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (Character.IsValid())
	{
	}
	Character.Reset();
	K2_EndAbility();
}



