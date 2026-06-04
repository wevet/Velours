// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/VBaseHuman.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class UHitTargetComponent;
class UQTEActionComponent;
class AWvPlayerController;

/**
 * 
 */
UCLASS()
class VELOURS_API APlayerCharacter : public AVBaseHuman, public IWvKeyableTargetInterface
{
	GENERATED_BODY()
	
public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void NotifyControllerChanged() override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;



protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UQTEActionComponent> QTEActionComponent;


public:
	FORCEINLINE class UQTEActionComponent* GetQTEActionComponent() const { return QTEActionComponent; }

#pragma region IWvKeyableTargetInterface
	virtual bool IsInputKeyDisable() const override;
	virtual void SetKeyInputDisable() override;
	virtual void SetKeyInputEnable() override;
#pragma endregion

	bool IsQTEActionPlaying() const;

	void NotifyRegisterMission(const int32 MissionIndex);

protected:
	virtual void PostAbilitySystemInitialize() override;

private:
	void TryNotifyControllerAbilitySystemInitialized();

	void HandleStanceMode();
	void HandleTargetLock();
	void HandleRotationMode();


	void HandleJump(const bool bIsPress);
	void HandleSprinting(const bool bIsPress);
	void HandleWalking(const bool bIsPress);
	void HandleMeleeAction(const bool bIsPress);
	void HandleDriveAction(const bool bIsPress);
	void HandleAliveAction(const bool bIsPress);
	void HandleHoldAimAction(const bool bIsPress);
	void HandleFinisherAction(const FGameplayTag Tag, const bool bIsPress);
	bool HasFinisherAction(const FGameplayTag Tag) const;
	void HandleQTEAction(const bool bIsPress);


	void DoBulletAttack_Callback();
	void Clear_BulletTimer();
	FTimerHandle Bullet_TimerHandle;

	UFUNCTION()
	void GameplayTagTrigger_Callback(const FGameplayTag Tag, const bool bIsPress);

	UFUNCTION()
	void OnPluralInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress);

	UFUNCTION()
	void OnHoldingInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress);

	UFUNCTION()
	void OnDoubleClickInputEventTrigger_Callback(const FGameplayTag Tag, const bool bIsPress);

	UFUNCTION()
	void OverlayStateChange_Callback(const ELSOverlayState PrevOverlay, const ELSOverlayState CurrentOverlay);

	UFUNCTION()
	void OnTargetLockedOn_Callback(AActor* LookOnTarget, UHitTargetComponent* TargetComponent);

	UFUNCTION()
	void OnTargetLockedOff_Callback(AActor* LookOnTarget, UHitTargetComponent* TargetComponent);

	UFUNCTION()
	void OnQTEBegin_Callback();

	UFUNCTION()
	void OnQTEEnd_Callback(const bool bIsSuccess);

};
