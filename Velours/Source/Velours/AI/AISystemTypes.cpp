// Copyright 2022 wevet works All Rights Reserved.

#include "AISystemTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

using namespace UE::Tasks;

DEFINE_LOG_CATEGORY(LogWvAI)


#pragma region PerceptionTask
bool FAIPerceptionTask::IsRunning() const
{
	return bIsTaskPlaying;
}

void FAIPerceptionTask::Abort(const bool bIsForce)
{
	++Generation;

	if (!bIsTaskPlaying.Load())
	{
		FinishDelegate.Reset();
		return;
	}

	bCancelTask.Store(true);
	bIsTaskPlaying.Store(false);

	FinishDelegate.Reset();

	if (bIsForce && TaskFuture.IsValid())
	{
		TaskFuture.Wait();
		TaskFuture = {};
	}

	TaskType = ETaskType::None;
	Timer.Store(0.f);

}

void FAIPerceptionTask::Begin(const ETaskType InTaskType, const float InTimer, TUniqueFunction<void(void)> InFinishDelegate)
{
	Abort(false);

	TaskType = InTaskType;
	Timer.Store(FMath::Max(InTimer, 0.f));
	FinishDelegate = MoveTemp(InFinishDelegate);

	bIsTaskPlaying.Store(true);
	bCancelTask.Store(false);

	const uint32 LocalGeneration = ++Generation;

	if (Timer.Load() <= 0.f)
	{
		AsyncTask(ENamedThreads::GameThread, [this, LocalGeneration]
			{
				if (LocalGeneration == Generation.Load())
				{
					End();
				}
			});
		return;
	}



	TaskFuture = Async(EAsyncExecution::ThreadPool, [this, LocalGeneration]()
	{
		float Elapsed = 0.f;
		const float Tick = 0.05f;

		UE_LOG(LogWvAI, Log, TEXT("[%s] Thread started"), *TaskTypeToString(TaskType));
		while (Elapsed < Timer.Load() && !bCancelTask.Load())
		{
			FPlatformProcess::Sleep(Tick);
			Elapsed += Tick;
		}
		UE_LOG(LogWvAI, Log, TEXT("[%s] Thread woke up"), *TaskTypeToString(TaskType));

		if (!bCancelTask.Load())
		{
			AsyncTask(ENamedThreads::GameThread, [this, LocalGeneration]()
			{
				if (LocalGeneration != Generation.Load())
				{
					return;
				}

				UE_LOG(LogWvAI, Log, TEXT("[%s] About to call End()"), *TaskTypeToString(TaskType));
				this->End();
			});
		}
	});

}

void FAIPerceptionTask::Cancel_Internal()
{
	bCancelTask = true;
	if (TaskFuture.IsValid())
	{
		TaskFuture.Wait();
		TaskFuture = {};
	}
}

FString FAIPerceptionTask::TaskTypeToString(ETaskType Type)
{
	if (const UEnum* EnumPtr = StaticEnum<ETaskType>())
	{
		return EnumPtr->GetNameStringByValue(static_cast<int64>(Type));
	}
	return TEXT("Unknown");
}

void FAIPerceptionTask::End()
{
	if (!bIsTaskPlaying.Load())
	{
		return;
	}

	bIsTaskPlaying.Store(false);
	bCancelTask.Store(true);
	TaskFuture = {};

	TUniqueFunction<void()> LocalDelegate = MoveTemp(FinishDelegate);
	FinishDelegate.Reset();

	if (LocalDelegate)
	{
		LocalDelegate();
	}

	UE_LOG(LogWvAI, Log, TEXT("[%s] : TaskEnd:[%s]"), *FString(__FUNCTION__), *TaskTypeToString(TaskType));

	TaskType = ETaskType::None;
	Timer.Store(0.f);
}

void FAIPerceptionTask::AddLength(const float AddTimer)
{
	if (AddTimer <= 0.f)
	{
		return;
	}

	const float NewTimer = Timer.Load() + AddTimer;
	Timer.Store(NewTimer);
	UE_LOG(LogWvAI, Log, TEXT("Modify Timer => %.3f, function => [%s]"), NewTimer, *FString(__FUNCTION__));
}
#pragma endregion


#pragma region LeaderTask
/// <summary>
/// leader task wip
/// </summary>
FAILeaderTask::FAILeaderTask()
{

}

void FAILeaderTask::OnEnable(const bool bIsEnable)
{
	bIsValid = bIsEnable;
}

bool FAILeaderTask::IsValid() const
{
	return bIsValid;
}

void FAILeaderTask::Notify()
{
	FTaskEvent Event{ TEXT("Event") };

	// TaskEventをLaunchで引数の最後に渡す
	Launch(TEXT("Task Event"), []
	{
		UE_LOG(LogWvAI, Log, TEXT("TaskEvent Completed => %s"), *FString(__FUNCTION__));
	},
	Event);

	// イベントとして登録されているタスクをトリガーして実行する
	Event.Trigger();

	// タスクAを起動
	FTask TaskA = Launch(TEXT("Task Prereqs TaskA"), []
	{
		FPlatformProcess::Sleep(1.0f);
		UE_LOG(LogWvAI, Log, TEXT("TaskA End"));
	});

	// タスクBとタスクCはタスクAが完了するまでは起動しない
	FTask TaskB = Launch(TEXT("Task Prereqs TaskB"), [] 
	{
		FPlatformProcess::Sleep(0.2f);
		UE_LOG(LogWvAI, Log, TEXT("TaskB End"));
	}, 
	TaskA);

	FTask TaskC = Launch(TEXT("Task Prereqs TaskC"), []
	{
		FPlatformProcess::Sleep(0.5f);
		UE_LOG(LogWvAI, Log, TEXT("TaskC End"));
	}, 
	TaskA);

	// タスクDはタスクBとタスクCが完了するまでは起動しない
	FTask TaskD = Launch(TEXT("Task Prereqs TaskD"), []
	{
		UE_LOG(LogWvAI, Log, TEXT("TaskD End"));
	}, 
	Prerequisites(TaskB, TaskC));

	TaskD.Wait();
	UE_LOG(LogWvAI, Log, TEXT("Task Prerequisites End"));
}
#pragma endregion


#pragma region CloseCombat
TArray<float>  FAICloseCombatData::BaseRandomSeeds = { 80.0f, 60.0f, 30.0f, 10.0f, 8.0f, 5.0f, 2.0f };

FAICloseCombatData::FAICloseCombatData()
{
	bIsComboCheckEnded = false;
	bIsPlaying = false;
	CurSeeds = 0.f;
}

void FAICloseCombatData::Initialize(const int32 InComboTypeIndex, const int32 InMaxComboCount)
{
	bIsPlaying = true;
	CurAttackComboCount = 0;
	AttackComboCount = FMath::Clamp(FMath::RandRange(0, InMaxComboCount), 0, BaseRandomSeeds.Num() - 1);
	ComboTypeIndex = InComboTypeIndex;

	const FVector2D SeedsRange { 0.1f, 0.3f};

	ModifySeeds.Empty();
	IntervalSeeds.Empty();

	for (const float Seed : BaseRandomSeeds)
	{
		const float ModifySeed = FMath::FRandRange(0.f, Seed);
		ModifySeeds.Add(ModifySeed);

		const float IntervalSeed = FMath::FRandRange(SeedsRange.X, SeedsRange.Y);
		IntervalSeeds.Add(IntervalSeed);
	}

	UE_LOG(LogWvAI, Verbose, TEXT("[%s]"), *FString(__FUNCTION__));
	UE_LOG(LogWvAI, Verbose, TEXT("AttackComboCount => %d"), AttackComboCount);
}

void FAICloseCombatData::Deinitialize()
{
	bIsPlaying = false;
	UE_LOG(LogWvAI, Verbose, TEXT("[%s]"), *FString(__FUNCTION__));
}

bool FAICloseCombatData::IsOverAttack() const
{
	return CurAttackComboCount >= AttackComboCount;
}

void FAICloseCombatData::ComboSeedBegin(TUniqueFunction<void(void)> InFinishDelegate)
{
	if (IsOverAttack())
	{
		return;
	}

	FinishDelegate = MoveTemp(InFinishDelegate);
	CurInterval = 0.f;
	bIsComboCheckEnded = false;

	const int32 Index = FMath::Clamp(CurAttackComboCount, 0, ModifySeeds.Num() - 1);
	CurSeeds = ModifySeeds[Index];
	CurIntervalSeeds = IntervalSeeds[Index];
	UE_LOG(LogWvAI, Verbose, TEXT("[%s]"), *FString(__FUNCTION__));
}

void FAICloseCombatData::ComboSeedUpdate(const float DeltaTime)
{
	if (IsOverAttack() || bIsComboCheckEnded)
	{
		// combo is full
		return;
	}
	
	if (CurInterval >= CurIntervalSeeds)
	{
		bIsComboCheckEnded = CurSeeds >= FMath::FRandRange(0.f, 100.0f);

		if (bIsComboCheckEnded)
		{
			if (FinishDelegate)
			{
				FinishDelegate();

				FinishDelegate.Reset();
				this->Internal_Update();
			}
		}
		else
		{
			//
		}
	}
	else
	{
		CurInterval += DeltaTime;
	}
}

void FAICloseCombatData::Internal_Update()
{
	if (!IsOverAttack())
	{
		++CurAttackComboCount;
	}

	CurInterval = 0.f;
	UE_LOG(LogWvAI, Verbose, TEXT("[%s] => %d/%d"), *FString(__FUNCTION__), CurAttackComboCount, AttackComboCount);
}

void FAICloseCombatData::ComboSeedEnd()
{
	if (!bIsComboCheckEnded)
	{
		//UE_LOG(LogWvAI, Log, TEXT("[%s]"), *FString(__FUNCTION__));
		Deinitialize();
	}

}

void FAICloseCombatData::ComboAbort()
{
	Deinitialize();
}
#pragma endregion


#pragma region FriendlyCoolDown
void FFriendlyParams::Begin()
{
	bIsFriendlyCoolDownPlaying = true;
	TaskInstance = Launch(UE_SOURCE_LOCATION, [this]
	{
		FPlatformProcess::Sleep(K_FRIENDLY_COOLDOWN_TIMER);
		bIsFriendlyCoolDownPlaying = false;
		UE_LOG(LogWvAI, Log, TEXT("FriendlyParams finish => %s"), *FString(__FUNCTION__));
	},
	ETaskPriority::Default, EExtendedTaskPriority::None);
}

bool FFriendlyParams::IsRunning() const
{
	return bIsFriendlyCoolDownPlaying;
}

void FFriendlyParams::ClearTask()
{
	if (TaskInstance.IsValid())
	{
		//
	}
}

void FFriendlyParams::Reset()
{
	FriendyCacheActors.Reset();
}

void FFriendlyParams::AddCache(AActor* Actor)
{
	if (!FriendyCacheActors.Contains(Actor))
	{
		FriendyCacheActors.Add(Actor);
	}
}

void FFriendlyParams::RemoveCache()
{
	constexpr int32 CacheMaxCount = 3;
	if (FriendyCacheActors.Num() >= CacheMaxCount)
	{
		// always first remove element
		FriendyCacheActors.RemoveAt(0);
	}
}

bool FFriendlyParams::HasCache(AActor* Actor) const
{
	return FriendyCacheActors.Contains(Actor);
}
#pragma endregion


