// Copyright 2022 wevet works All Rights Reserved.


#include "Component/StatusComponent.h"
#include "Character/BasePawn.h"
#include "Ability/WvAbilitySystemComponent.h"
#include "WvAbilityAttributeSet.h"
#include "Ability/WvInheritanceAttributeSet.h"
#include "WvAbilitySystemBlueprintFunctionLibrary.h"
#include "Game/CharacterInstanceSubsystem.h"
#include "Velours.h"
#include "Misc/WvCommonUtils.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusComponent)

UStatusComponent::UStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	Super::SetComponentTickEnabled(false);

	Character = Cast<ABasePawn>(GetOwner());


	if (Character.IsValid())
	{
		Character->OnTeamHandleAttackDelegate.AddUniqueDynamic(this, &ThisClass::OnSendAbilityAttack);
		Character->OnTeamWeaknessHandleAttackDelegate.AddUniqueDynamic(this, &ThisClass::OnSendWeaknessAttack);
		Character->OnTeamHandleReceiveDelegate.AddUniqueDynamic(this, &ThisClass::OnReceiveAbilityAttack);
		Character->OnTeamWeaknessHandleReceiveDelegate.AddUniqueDynamic(this, &ThisClass::OnReceiveWeaknessAttack);
		Character->OnTeamHandleReceiveKillDelegate.AddUniqueDynamic(this, &ThisClass::OnReceiveKillTarget);

		Character->OnAbilitySystemAvailable.AddUObject(this, &UStatusComponent::HandleAbilitySystemAvailable);

		TryBindExistingAbilitySystem();
	}


	if (IWvAbilitySystemAvatarInterface* Avatar = Cast<IWvAbilitySystemAvatarInterface>(GetOwner()))
	{
		CharacterInfo.Name = Avatar->GetAvatarTag().GetTagName();
	}
}

void UStatusComponent::HandleAbilitySystemAvailable(UWvAbilitySystemComponent* InASC)
{
	TryBindExistingAbilitySystem();
}

void UStatusComponent::TryBindExistingAbilitySystem()
{
	if (ASC.IsValid())
	{
		return;
	}

	if (!Character.IsValid())
	{
		Character = Cast<ABasePawn>(GetOwner());
	}

	if (!Character.IsValid())
	{
		return;
	}

	ASC = Character->GetWvAbilitySystemComponent();

	if (!ASC.IsValid())
	{
		return;
	}

	AAS = ASC->GetStatusAttributeSet(UWvAbilityAttributeSet::StaticClass());

	HPChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetHPAttribute()).AddUObject(this, &UStatusComponent::HealthChange_Callback);
	DamageChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetDamageAttribute()).AddUObject(this, &UStatusComponent::DamageChange_Callback);
	SkillChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetSkillAttribute()).AddUObject(this, &UStatusComponent::SkillChange_Callback);
	VigilanceChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetVigilanceAttribute()).AddUObject(this, &UStatusComponent::Vigilance_Callback);
}

void UStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	if (Character.IsValid())
	{
		Character->OnAbilitySystemAvailable.RemoveAll(this);

		Character->OnTeamHandleAttackDelegate.RemoveDynamic(this, &ThisClass::OnSendAbilityAttack);
		Character->OnTeamWeaknessHandleAttackDelegate.RemoveDynamic(this, &ThisClass::OnSendWeaknessAttack);
		Character->OnTeamHandleReceiveDelegate.RemoveDynamic(this, &ThisClass::OnReceiveAbilityAttack);
		Character->OnTeamWeaknessHandleReceiveDelegate.RemoveDynamic(this, &ThisClass::OnReceiveWeaknessAttack);
		Character->OnTeamHandleReceiveKillDelegate.RemoveDynamic(this, &ThisClass::OnReceiveKillTarget);
	}

	if (ASC.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetHPAttribute()).Remove(HPChangeDelegateHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetDamageAttribute()).Remove(DamageChangeDelegateHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetSkillAttribute()).Remove(SkillChangeDelegateHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UWvAbilityAttributeSet::GetVigilanceAttribute()).Remove(VigilanceChangeDelegateHandle);
	}

	ASC.Reset();
	AAS.Reset();
	Character.Reset();
	Super::EndPlay(EndPlayReason);
}

void UStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UStatusComponent::HealthChange_Callback(const FOnAttributeChangeData& Data)
{
	if (AAS.IsValid())
	{
		auto Health = AAS->GetHP();
		auto MaxHealth = AAS->GetHPMax();
		//UE_LOG(LogTemp, Log, TEXT("Health => %.3f, MaxHealth => %.3f, Owner => %s, function => %s"), Health, MaxHealth, *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
	}
}

void UStatusComponent::DamageChange_Callback(const FOnAttributeChangeData& Data)
{
	//UE_LOG(LogTemp, Log, TEXT("Damage => %.3f, Owner => %s, function => %s"), Data.NewValue, *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
}

void UStatusComponent::SkillChange_Callback(const FOnAttributeChangeData& Data)
{
	if (!AAS.IsValid())
	{
		return;
	}

	if (UWvCommonUtils::IsBotPawn(Character.Get()))
	{
		auto Skill = AAS->GetSkill();
		auto MaxSkill = AAS->GetSkillMax();
		UE_LOG(LogTemp, Log, TEXT("Skill => %.3f, MaxSkill => %.3f, Owner => %s, function => %s"), Skill, MaxSkill, *GetNameSafe(GetOwner()), *FString(__FUNCTION__));
	}


	if (IsMaxSkll())
	{
		//Character->SkillEnableAction(true);
	}
	else
	{

	}

}

void UStatusComponent::Vigilance_Callback(const FOnAttributeChangeData& Data)
{
	if (!AAS.IsValid())
	{
		return;
	}

	if (IsMaxVigilance())
	{

	}
	UE_LOG(LogTemp, Log, TEXT("[%s]"), *FString(__FUNCTION__));
}

const bool UStatusComponent::SetFullSkill()
{
	if (!AAS.IsValid() || !Character.IsValid())
	{
		return false;
	}

	UWvAbilitySystemBlueprintFunctionLibrary::FullSkill(Character.Get());
	AAS->SetSkill(AAS->GetSkillMax());
	return true;
}

float UStatusComponent::GetHealthToWidget() const
{
	if (AAS.IsValid())
	{
		return AAS->GetHP() / AAS->GetHPMax();
	}
	return 0.f;
}

float UStatusComponent::GetSkillToWidget() const
{
	if (AAS.IsValid())
	{
		return AAS->GetSkill() / AAS->GetSkillMax();
	}
	return 0.f;
}

bool UStatusComponent::IsHealthHalf() const
{
	if (AAS.IsValid())
	{
		return (AAS->GetHP() / AAS->GetHPMax()) < 0.5f;
	}
	return false;
}

bool UStatusComponent::IsMaxSkll() const
{
	if (AAS.IsValid())
	{
		return (AAS->GetSkill() >= AAS->GetSkillMax());
	}
	return false;
}

bool UStatusComponent::IsMaxVigilance() const
{
	if (AAS.IsValid())
	{
		return (AAS->GetVigilance() >= AAS->GetVigilanceMax());
	}
	return false;
}

UWvInheritanceAttributeSet* UStatusComponent::GetInheritanceAttributeSet() const
{
	if (AAS.IsValid())
	{
		return Cast<UWvInheritanceAttributeSet>(AAS);
	}
	return nullptr;
}

float UStatusComponent::GetKillDamage() const
{
	if (AAS.IsValid())
	{
		return AAS->GetHPMax();
	}
	return 0.f;
}

/// <summary>
/// Œx‰ú’l
/// </summary>
/// <returns></returns>
float UStatusComponent::GetVigilance() const
{
	if (AAS.IsValid())
	{
		return AAS->GetVigilance();
	}
	return 0.f;
}

void UStatusComponent::DoAlive()
{
	if (AAS.IsValid())
	{
		const float HPMax = AAS->GetHPMax();
		AAS->SetHP(AAS->GetHPMax());
		UWvAbilitySystemBlueprintFunctionLibrary::RecoverHP(Character.Get(), HPMax);
	}
}

void UStatusComponent::DoKill()
{
	if (AAS.IsValid())
	{

	}

	//const float HPCurrent = AAS->GetHP();
	UWvAbilitySystemBlueprintFunctionLibrary::KillMySelf(Character.Get());
}

/// <summary>
/// get currenthealth
/// x => min
/// y => current
/// z => max
/// </summary>
/// <param name="OutHealth"></param>
void UStatusComponent::GetCharacterHealth(FVector& OutHealth)
{
	if (AAS.IsValid())
	{
		OutHealth = FVector(0.f, AAS->GetHP(), AAS->GetHPMax());
	}
}


#pragma region CharacterInfo
void UStatusComponent::SetGenderType(const EGenderType InGenderType)
{
	CharacterInfo.GenderType = InGenderType;
}

EGenderType UStatusComponent::GetGenderType() const
{
	return CharacterInfo.GenderType;
}

void UStatusComponent::SetBodyShapeType(const EBodyShapeType InBodyShapeType)
{
	CharacterInfo.BodyShapeType = InBodyShapeType;
}

EBodyShapeType UStatusComponent::GetBodyShapeType() const
{
	return CharacterInfo.BodyShapeType;
}

const FCharacterInfo& UStatusComponent::GetCharacterInfo()
{
	return CharacterInfo;
}
#pragma endregion


void UStatusComponent::OnSendAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage)
{
	// if owner actor attack other actor attack
}

void UStatusComponent::OnSendWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage)
{
	if (AAS.IsValid())
	{
		auto Skill = AAS->GetSkill();
	}
}

void UStatusComponent::OnReceiveAbilityAttack(AActor* Actor, const FWvBattleDamageAttackSourceInfo& SourceInfo, const float Damage)
{
	//const auto WeaponState = (EAttackWeaponState)SourceInfo.WeaponID;
	//const FString CategoryName = *FString::Format(TEXT("WeaponState => {0}"), { *GETENUMSTRING("/Script/Redemption.EAttackWeaponState", WeaponState) });
	//UE_LOG(LogTemp, Warning, TEXT("Weapon %s function => %s"), *CategoryName, *FString(__FUNCTION__));
}

void UStatusComponent::OnReceiveWeaknessAttack(AActor* Actor, const FName& WeaknessName, const float Damage)
{
}

void UStatusComponent::OnReceiveKillTarget(AActor* Actor, const float Damage)
{
	if (UWvCommonUtils::IsBotPawn(Character.Get()))
	{
		UCharacterInstanceSubsystem::Get()->RemoveAICharacter(Character.Get());
	}
}



