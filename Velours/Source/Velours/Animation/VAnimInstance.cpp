// Copyright 2022 wevet works All Rights Reserved.


#include "Animation/VAnimInstance.h"


#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetAnimationLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedAnimLayer.h"
#include "Animation/BlendSpace.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VAnimInstance)

DEFINE_LOG_CATEGORY(LogVAnimation)

UVAnimInstance::UVAnimInstance()
{
}

void UVAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UVAnimInstance::NativeUninitializeAnimation()
{
	Super::NativeUninitializeAnimation();
}

void UVAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
}

void UVAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
}

void UVAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}


FVector UVAnimInstance::GetPredictionStopLocation(const FVector& CurrentLocation) const
{
	if (!TargetActor.IsValid())
	{
		return CurrentLocation;
	}

	const UMoverComponent* MoverComponent = TargetActor->FindComponentByClass<UMoverComponent>();
	if (MoverComponent == nullptr)
	{
		return CurrentLocation;
	}

	const UCommonLegacyMovementSettings* MovementSettings = MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	if (MovementSettings == nullptr)
	{
		return CurrentLocation;
	}

	const FVector UpDirection = MoverComponent->GetUpDirection().GetSafeNormal();
	if (UpDirection.IsNearlyZero())
	{
		return CurrentLocation;
	}

	// 重力方向の速度を除外し、地面に沿った速度だけを使用
	FVector PredictedVelocity = FVector::VectorPlaneProject(MoverComponent->GetVelocity(), UpDirection);
	if (PredictedVelocity.SizeSquared() <= FMath::Square(1.0f))
	{
		return CurrentLocation;
	}

	const float BrakingDeceleration = FMath::Max(0.0f, MovementSettings->Deceleration);
	const float BaseFriction = MovementSettings->bUseSeparateBrakingFriction ? MovementSettings->BrakingFriction : MovementSettings->GroundFriction;
	const float BrakingFriction = FMath::Max(0.0f, BaseFriction * MovementSettings->BrakingFrictionFactor);

	if (BrakingDeceleration <= KINDA_SMALL_NUMBER && BrakingFriction <= KINDA_SMALL_NUMBER)
	{
		return CurrentLocation;
	}

	FVector PredictedLocation = CurrentLocation;

	// 描画フレームレートに依存させない固定ステップ
	constexpr float SimulationDeltaTime = 1.0f / 60.0f;
	constexpr float MaxPredictionTime = 5.0f;
	constexpr float StopSpeed = 1.0f;
	const int32 MaxSimulationSteps = FMath::CeilToInt(MaxPredictionTime / SimulationDeltaTime);

	for (int32 Step = 0; Step < MaxSimulationSteps; ++Step)
	{
		const float CurrentSpeed = PredictedVelocity.Size();
		if (CurrentSpeed <= StopSpeed)
		{
			break;
		}

		const FVector VelocityDirection = PredictedVelocity / CurrentSpeed;

		// 摩擦は現在速度に比例し、Decelerationは一定値として加算
		const float FrictionDeceleration = CurrentSpeed * BrakingFriction;
		const float TotalDeceleration = FrictionDeceleration + BrakingDeceleration;
		const float NewSpeed = FMath::Max(0.0f, CurrentSpeed - TotalDeceleration * SimulationDeltaTime);
		const FVector NewVelocity = VelocityDirection * NewSpeed;

		// フレーム先頭と末尾の平均速度で移動距離を積算
		PredictedLocation += (PredictedVelocity + NewVelocity) * (0.5f * SimulationDeltaTime);
		PredictedVelocity = NewVelocity;
	}

	return PredictedLocation;
}


#pragma region Utils
const TArray<UAnimInstance*> UVAnimInstance::GetAllAnimInstances()
{
	TArray<UAnimInstance*> Instances;
	Instances.Add(this);

	if (const IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
		const TArray<FStructProperty*>& LinkedAnimLayerNodeProperties = AnimBlueprintClass->GetLinkedAnimLayerNodeProperties();
		for (const FStructProperty* LayerNodeProperty : LinkedAnimLayerNodeProperties)
		{
			const FAnimNode_LinkedAnimLayer* Layer = LayerNodeProperty->ContainerPtrToValuePtr<FAnimNode_LinkedAnimLayer>(this);
			UAnimInstance* TargetInstance = Layer->GetTargetInstance<UAnimInstance>();
			if (IsValid(TargetInstance))
			{
				Instances.AddUnique(TargetInstance);
			}
		}

		const TArray<FStructProperty*>& LinkedAnimGraphNodeProperties = AnimBlueprintClass->GetLinkedAnimGraphNodeProperties();
		for (const FStructProperty* LinkedAnimGraphNodeProperty : LinkedAnimGraphNodeProperties)
		{
			const FAnimNode_LinkedAnimGraph* LinkedAnimGraph = LinkedAnimGraphNodeProperty->ContainerPtrToValuePtr<FAnimNode_LinkedAnimGraph>(this);
			UAnimInstance* TargetInstance = LinkedAnimGraph->GetTargetInstance<UAnimInstance>();
			if (IsValid(TargetInstance))
			{
				Instances.AddUnique(TargetInstance);
			}
		}
	}
	return Instances;
}

const TMap<FName, FAnimGroupInstance>& UVAnimInstance::GetSyncGroupMapRead() const
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetSyncGroupMapRead();
}

const TArray<FAnimTickRecord>& UVAnimInstance::GetUngroupedActivePlayersRead()
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetUngroupedActivePlayersRead();
}

void UVAnimInstance::DrawRelevantAnimation()
{
	if (!bOwnerPlayerController)
	{
		return;
	}

	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	const UWorld* World = Pawn->GetWorld();

	// check sync group
	{
		const TMap<FName, FAnimGroupInstance>& SyncGroupMap = GetSyncGroupMapRead();
		const TArray<FAnimTickRecord>& UngroupedActivePlayers = GetUngroupedActivePlayersRead();

		// Sync Groups and Sequences
		const FString SynGroupsHeading = FString::Printf(TEXT("SyncGroups: %i"), SyncGroupMap.Num());
		UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*SynGroupsHeading), true, false, FColor::Green, 0.0f);

		for (const TTuple<FName, FAnimGroupInstance>& SyncGroupPair : SyncGroupMap)
		{
			const FAnimGroupInstance& SyncGroup = SyncGroupPair.Value;
			const FString GroupLabel = FString::Printf(TEXT("Group %s - Players %i"), *SyncGroupPair.Key.ToString(), SyncGroup.ActivePlayers.Num());
			UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*GroupLabel), true, false, FColor::Green, 0.0f);

			if (SyncGroup.ActivePlayers.Num() > 0)
			{
				check(SyncGroup.GroupLeaderIndex != -1);
				constexpr bool bFullBlendSpaceDisplay = true;
				RenderAnimTickRecords(SyncGroup.ActivePlayers, SyncGroup.GroupLeaderIndex, FColor::White, FColor::Green, FColor::Black, bFullBlendSpaceDisplay);
			}
		}
		const FString UngroupedHeading = FString::Printf(TEXT("Ungrouped: %i"), UngroupedActivePlayers.Num());
		UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*UngroupedHeading), true, false, FColor::Green, 0.0f);

		constexpr int HighlightIndex = -1;
		constexpr bool bFullBlendSpaceDisplay = true;
		RenderAnimTickRecords(UngroupedActivePlayers, HighlightIndex, FColor::White, FColor::Green, FColor::Black, bFullBlendSpaceDisplay);
	}

	// montage & anim notify entry
	{
		const TArray<UAnimInstance*> AnimInstances = GetAllAnimInstances();
		for (UAnimInstance* AnimInstance : AnimInstances)
		{
			if (!IsValid(AnimInstance))
			{
				continue;
			}

			const FString ABPHeading = FString::Printf(TEXT("ABP Name: %s"), *AnimInstance->GetName());
			UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*ABPHeading), true, false, FColor::Yellow, 0.0f);

			const int32 MontageInstancesCount = AnimInstance->MontageInstances.Num();
			const FString MontagesHeading = FString::Printf(TEXT("Montages: %i"), MontageInstancesCount);
			UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*MontagesHeading), true, false, FColor::Blue, 0.0f);
			for (FAnimMontageInstance* MontageInstance : AnimInstance->MontageInstances)
			{
				FColor ActiveColor = MontageInstance->IsActive() ? FColor::Green : FColor::Black;

				if (MontageInstance && MontageInstance->Montage)
				{
					const FString MontageEntry = FString::Printf(TEXT("%s, CurrSec: %s, NextSec: %s, Weight: %.3f"),
						*MontageInstance->Montage->GetName(),
						*MontageInstance->GetCurrentSection().ToString(),
						*MontageInstance->GetNextSection().ToString(),
						MontageInstance->GetWeight());

					UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*MontageEntry), true, false, ActiveColor, 0.0f);
				}
			}

			const int32 ActiveNotifiesCount = ActiveAnimNotifyState.Num();
			const FString AnimNotifyHeading = FString::Printf(TEXT("Active Notify States: %i"), ActiveNotifiesCount);
			UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*AnimNotifyHeading), true, false, FColor::Blue, 0.0f);
			for (const FAnimNotifyEvent& NotifyState : AnimInstance->ActiveAnimNotifyState)
			{
				const FString NotifyEntry = FString::Printf(TEXT("%s Dur:%.3f"), *NotifyState.NotifyName.ToString(), NotifyState.GetDuration());
				UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*NotifyEntry), true, false, FColor::Green, 0.0f);
			}
		}

	}

}

void UVAnimInstance::RenderAnimTickRecords(
	const TArray<FAnimTickRecord>& Records,
	const int32 HighlightIndex, 
	FColor TextColor, 
	FColor HighlightColor, 
	FColor InInactiveColor, 
	bool bFullBlendSpaceDisplay) const
{

	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	const UWorld* World = Pawn->GetWorld();
	for (int32 PlayerIndex = 0; PlayerIndex < Records.Num(); ++PlayerIndex)
	{
		const FAnimTickRecord& Player = Records[PlayerIndex];
		FString PlayerEntry = FString::Printf(TEXT("%i) %s"), PlayerIndex, *Player.SourceAsset->GetName());

		float Progress = -1.f;
		// See if we have access to SequenceLength
		if (UAnimSequenceBase* AnimSeqBase = Cast<UAnimSequenceBase>(Player.SourceAsset))
		{
			if (Player.TimeAccumulator != nullptr)
			{
				Progress = *Player.TimeAccumulator / AnimSeqBase->GetPlayLength();
			}
		}

		if (Progress == -1.f)
		{
			PlayerEntry += FString::Printf(TEXT(" P(%.2f)"), Player.TimeAccumulator != nullptr ? *Player.TimeAccumulator : 0.f);
		}
		UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*PlayerEntry), true, false, HighlightColor, 0.0f);

		if (Progress >= 0.f)
		{
			UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*FString::Printf(TEXT("%.3f"), Progress)), true, false, HighlightColor, 0.0f);
		}

		if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(Player.SourceAsset))
		{
			if (bFullBlendSpaceDisplay && Player.BlendSpace.BlendSampleDataCache && Player.BlendSpace.BlendSampleDataCache->Num() > 0)
			{
				TArray<FBlendSampleData> SampleData = *Player.BlendSpace.BlendSampleDataCache;
				SampleData.Sort([](const FBlendSampleData& L, const FBlendSampleData& R) { return L.SampleDataIndex < R.SampleDataIndex; });

				const FVector BlendSpacePosition(Player.BlendSpace.BlendSpacePositionX, Player.BlendSpace.BlendSpacePositionY, 0.f);
				const FString BlendSpaceHeader = FString::Printf(TEXT("Blendspace Input (%s)"), *BlendSpacePosition.ToString());
				UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*BlendSpaceHeader), true, false, HighlightColor, 0.0f);

				const TArray<FBlendSample>& BlendSamples = BlendSpace->GetBlendSamples();

				int32 WeightedSampleIndex = 0;

				for (int32 SampleIndex = 0; SampleIndex < BlendSamples.Num(); ++SampleIndex)
				{
					const FBlendSample& BlendSample = BlendSamples[SampleIndex];

					float Weight = 0.f;
					for (; WeightedSampleIndex < SampleData.Num(); ++WeightedSampleIndex)
					{
						FBlendSampleData& WeightedSample = SampleData[WeightedSampleIndex];
						if (WeightedSample.SampleDataIndex == SampleIndex)
						{
							Weight += WeightedSample.GetClampedWeight();
						}
						else if (WeightedSample.SampleDataIndex > SampleIndex)
						{
							break;
						}
					}

					const FString SampleEntry = FString::Printf(TEXT("%s"), *BlendSample.Animation->GetName());

					const FColor CurColor = (Weight > 0.f) ? HighlightColor : InInactiveColor;
					UKismetSystemLibrary::PrintString(World, TCHAR_TO_ANSI(*SampleEntry), true, false, CurColor, 0.0f);
				}
			}
		}

	}
}

#pragma endregion

