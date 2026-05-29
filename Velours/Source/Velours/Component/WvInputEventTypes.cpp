// Copyright 2022 wevet works All Rights Reserved.


#include "WvInputEventTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WvInputEventTypes)

TArray<int32> FWvInputEvent::GetBindingIndexs() const
{
	return BindingIndexs; 
}

TArray<FInputActionKeyMapping> FWvInputEvent::GetActionKeyMappings() const
{
	return ActionKeyMappings; 
}

FString FWvInputEvent::GetExtend() const
{
	return Extend; 
}

FString FWvInputEvent::GetEventTagNameWithExtend() const
{
	return EventTag.ToString() + GetExtend(); 
}

bool FWvInputEvent::GetIsUseExtend() const
{
	return IsUseExtend; 
}

void FWvInputEvent::AddBindingIndex(const int32 BindingIndex)
{
	BindingIndexs.Add(BindingIndex);
}

void FWvInputEvent::AddInputActionKeyMapping(FInputActionKeyMapping& InputActionKeyMapping)
{
	ActionKeyMappings.Add(InputActionKeyMapping);
}

void FWvInputEvent::SetAttachExtendToEventTag(const FString InExtend)
{
	Extend = InExtend;
	IsUseExtend = true;
}


#pragma region HoldAction
void UWvInputEventCallbackInfo::OnPressed(const UWorld* World)
{
	bCallbackResult = false;

	StartTime = World->GetTimeSeconds();

	FTimerManager& TM = World->GetTimerManager();
	if (TM.IsTimerActive(HoldActionTH))
		TM.ClearTimer(HoldActionTH);

	TM.SetTimer(HoldActionTH, this, &UWvInputEventCallbackInfo::UpdateHold, 0.016f, true);
}

void UWvInputEventCallbackInfo::OnReleased()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldActionTH);
	}
}

void UWvInputEventCallbackInfo::UpdateHold()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if ((CurrentTime - StartTime) >= HoldTimer)
	{
		if (!bCallbackResult)
		{
			bCallbackResult = true;
			OnHoldingCallback.Broadcast(EventTag, true);
			// 成功したらタイマーを止めても良い（連打発動防止）
			OnReleased();
		}
	}
}
#pragma endregion

#define DOUBLE_CLICK_COUNT 2

const bool UWvInputEventCallbackInfo::OnDoubleClickPressed()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();

	if (ClickCount <= 0)
	{
		LastPressedTimeSeconds = CurrentTime;
		ClickCount++;
		return false;
	}

	const float CLICK_INTERVAL = 0.33f;
	if ((CurrentTime - LastPressedTimeSeconds) < CLICK_INTERVAL)
	{
		ClickCount++;
		if (ClickCount >= DOUBLE_CLICK_COUNT)
		{
			OnDoubleClickCallback.Broadcast(EventTag, true);
			OnDoubleClickEnded();
			return true;
		}
	}
	else
	{
		LastPressedTimeSeconds = CurrentTime;
		ClickCount = 1;
	}
	return false;
}

void UWvInputEventCallbackInfo::OnDoubleClickReleased()
{

}

void UWvInputEventCallbackInfo::OnDoubleClickEnded()
{
	bDoubleClickStarted = false;
	ClickCount = 0;
}

