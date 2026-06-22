// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
//#include "Curves/CurveFloat.h"
//#include "Components/PrimitiveComponent.h"
#include "Tasks/Task.h"
#include "Logging/LogMacros.h"
//#include "Templates/UniqueFunction.h"

#include "AISystemTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWvAI, Log, All)

class AWvAIController;

/*
* AIPerception Sight CancelEvent Interval
*/
#define SIGHT_AGE 20.0f

/*
* AIPerception Hear CancelEvent Interval
*/
#define HEAR_AGE 30.0f

/*
* AIPerception Follow CancelEvent Interval
*/
#define FOLLOW_AGE -1.0

/*
* AIPerception Friend CancelEvent Interval
*/
#define FRIEND_AGE 30.0


UENUM()
enum class ETaskType : uint8
{
	None,
	Sight, 
	Hear, 
	Communication,
	Follow,
};

UENUM(BlueprintType)
enum class EAICombatAdvantage : uint8
{
	Unknown,
	Winning,
	Even,
	Losing,
	CriticalDisadvantage,
};

UENUM(BlueprintType)
enum class EAIIndividualCombatIntent : uint8
{
	None,
	PressureAttack,
	KeepDistance, 
	Defensive,
	Retreat,
	Flee, 
	CallHelp,
};

UENUM(BlueprintType)
enum class EAIGroupCombatRole : uint8
{
	None,

	Attacker,   // 実際に攻撃する
	Pressure,   // 圧をかける
	Flanker,    // 横/背後へ回る
	Support,    // 味方支援
	Observer,   // 様子見、包囲維持
	Retreater,  // 一時後退
};


USTRUCT(BlueprintType)
struct FAIIndividualCombatScore
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float SelfPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float TargetThreat = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float Confidence = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float HealthRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float StaminaRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float TargetHealthRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float DistanceToTarget = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float RecentDamagePressure = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float TargetMomentum = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	float WeaponMatchup = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	bool bHasAttackReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat|Individual")
	bool bHasEscapeRoute = false;

	bool IsValidScore() const
	{
		return HealthRatio > 0.f;
	}
};

USTRUCT(BlueprintType)
struct FAIIndividualCombatDecision
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAICombatAdvantage Advantage = EAICombatAdvantage::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAIIndividualCombatIntent Intent = EAIIndividualCombatIntent::None;

	// 最終的な判断値
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Confidence = 0.f;

	// 攻撃頻度。高いほど攻める
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackDesire = 0.f;

	// 距離を取りたい度合い
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float KeepDistanceDesire = 0.f;

	// 逃走したい度合い
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FleeDesire = 0.f;

	// 援軍要請したい度合い
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CallHelpDesire = 0.f;

	// 推奨距離。BTのMoveTo/距離制御に使う
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DesiredCombatDistance = 250.f;

	// コンボ継続率補正。勝っている時は高く、不利なら低い
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ComboContinueRate = 0.5f;

	// 後退/逃走先の計算が必要か
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNeedsEscapePoint = false;

	// 援軍要請を試みるか
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShouldCallHelp = false;

	// 完全逃走か。一時後退ではなく戦闘離脱
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShouldFlee = false;
};


USTRUCT(BlueprintType)
struct FAIIndividualCombatDecisionConfig
{
	GENERATED_BODY()

public:
	// Confidence >= この値なら勝てそう
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WinningThreshold = 0.25f;

	// Confidence <= この値なら負けそう
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LosingThreshold = -0.25f;

	// Confidence <= この値ならかなり不利
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CriticalDisadvantageThreshold = -0.55f;

	// HPがこの値以下なら逃走判断を強める
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LowHealthThreshold = 0.35f;

	// HPがこの値以下ならかなり危険
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CriticalHealthThreshold = 0.18f;

	// 互角時の理想距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EvenDesiredDistance = 280.f;

	// 有利時の理想距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WinningDesiredDistance = 180.f;

	// 不利時の理想距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LosingDesiredDistance = 450.f;

	// かなり不利時の逃走距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FleeDesiredDistance = 900.f;

	// 臆病さ。高いほど逃げやすい
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Cowardice = 0.0f;

	// 攻撃性。高いほど攻めやすい
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Aggression = 0.5f;

	// 援軍要請しやすさ
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HelpSeeking = 0.5f;
};


USTRUCT(BlueprintType)
struct VELOURS_API FBlackboardKeyConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName TargetKeyName = TEXT("Target");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName LeaderKeyName = TEXT("Leader");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName FriendKeyName = TEXT("Friend");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName SearchNodeHolderKeyName = TEXT("SearchNodeHolder");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName PatrolLocationKeyName = TEXT("PatrolLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName PredictionKeyName = TEXT("PredictionLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName CoverPointKeyName = TEXT("CoverLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName FollowLocationKeyName = TEXT("FollowLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName FriendLocationKeyName = TEXT("FriendLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName DestinationKeyName = TEXT("DestinationLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName IsDeadKeyName = TEXT("IsDead");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName IsCloseCombat = TEXT("IsCloseCombat");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard")
	FName AIActionStateKeyName = TEXT("AIActionState");


	// 個体AIの最終的な戦闘意図。
	// PressureAttack / KeepDistance / Defensive / Retreat / Flee / CallHelp など。
	// BehaviorTree側ではこの値を見て、攻撃・距離維持・後退・逃走などへ分岐する。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName CombatIntentKeyName = TEXT("CombatIntent");

	// 個体AIが現在の戦闘をどう評価しているか。
	// Winning / Even / Losing / CriticalDisadvantage など。
	// CombatIntentよりも粗い優劣判定として使う。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName CombatAdvantageKeyName = TEXT("CombatAdvantage");

	// 自分とターゲットの相対評価値。
	// -1.0に近いほど不利、0.0付近で互角、1.0に近いほど有利。
	// BT DecoratorやDebug表示で使いやすい。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName CombatConfidenceKeyName = TEXT("CombatConfidence");

	// AIが現在保ちたい理想戦闘距離。
	// 有利なら短く、不利なら長くなる。
	// MoveTo / EQS / 距離制御Taskで使用する。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName DesiredCombatDistanceKeyName = TEXT("DesiredCombatDistance");

	// 完全逃走を試みるべきか。
	// Retreatとは違い、戦闘継続ではなく戦闘離脱寄りの判断。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName ShouldFleeKeyName = TEXT("ShouldFlee");

	// 援軍要請を試みるべきか。
	// 単体で不利、かつHelpSeekingが高い場合などにtrueになる。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName ShouldCallHelpKeyName = TEXT("ShouldCallHelp");

	// 後退または逃走時の移動先。
	// 敵と逆方向だけでなく、NavMesh上・視線が切れる地点を優先して選ぶ。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName FleeLocationKeyName = TEXT("FleeLocation");

	// 攻撃したい度合い。
	// 高いほど攻撃Taskや近接コンボへ入りやすくする。
	// 直接BT分岐に使っても、攻撃頻度補正に使ってもよい。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName AttackDesireKeyName = TEXT("AttackDesire");

	// 距離を取りたい度合い。
	// 互角や不利な状態で高くなる。
	// CircleMove / BackStep / KeepRange系の判断に使う。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName KeepDistanceDesireKeyName = TEXT("KeepDistanceDesire");

	// 逃げたい度合い。
	// HP低下、Confidence低下、Cowardiceが高い場合に上がる。
	// ShouldFleeよりも連続的な評価値として使う。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName FleeDesireKeyName = TEXT("FleeDesire");

	// 援軍を呼びたい度合い。
	// HelpSeekingや不利状況によって上がる。
	// Voice / Signal / Group連携の発火条件に使う。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName CallHelpDesireKeyName = TEXT("CallHelpDesire");

	// 近接コンボを継続する確率補正。
	// 有利なら高く、不利なら低くなる。
	// NotifyCloseCombatBeginやCombo継続判断で参照できる。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Blackboard|Combat")
	FName ComboContinueRateKeyName = TEXT("ComboContinueRate");
};


struct VELOURS_API FAIPerceptionTask
{

private:
	ETaskType TaskType{ ETaskType::None};
	bool bIsNeedTimer{false};
	
	TUniqueFunction<void()> FinishDelegate;
	TFuture<void> TaskFuture;

	TWeakObjectPtr<AWvAIController> WeakOwner;
	void Cancel_Internal();

	static FString TaskTypeToString(ETaskType Type);

	TAtomic<uint32> Generation{ 0 };
	TAtomic<bool> bCancelTask{ false };
	TAtomic<bool> bIsTaskPlaying{ false };
	TAtomic<float> Timer{ 0.f };


public:
	FAIPerceptionTask()
	{
	}

	void Begin(const ETaskType InTaskType, const float InTimer, TUniqueFunction<void(void)> InFinishDelegate);
	void End();

	void Abort(const bool bIsForce);
	void AddLength(const float AddTimer);
	bool IsRunning() const;
};


struct FAILeaderTask
{

private:
	bool bIsValid{false};

public:
	FAILeaderTask();

	void OnEnable(const bool bIsEnable);

	void Notify();

	bool IsValid() const;
};


/// <summary>
/// close combat setting params
/// etc. knife action or punch action
/// </summary>
struct FAICloseCombatData
{

public:
	FAICloseCombatData();
	void Initialize(const int32 InComboTypeIndex, const int32 InMaxComboCount);

	void Deinitialize();
	bool IsOverAttack() const;

	void ComboSeedBegin(TUniqueFunction<void(void)> InFinishDelegate);
	void ComboSeedUpdate(const float DeltaTime);
	void ComboSeedEnd();

	void ComboAbort();

	bool IsPlaying() const { return bIsPlaying; }
	int32 GetComboTypeIndex() const { return ComboTypeIndex; }

private:
	int32 AttackComboCount = INDEX_NONE;
	int32 CurAttackComboCount = INDEX_NONE;

	static TArray<float> BaseRandomSeeds;
	TArray<float> ModifySeeds;

	TArray<float> IntervalSeeds;
	float CurIntervalSeeds = 0.f;
	float CurInterval;

	bool bIsComboCheckEnded;
	bool bIsPlaying;
	float CurSeeds;

	TUniqueFunction<void(void)> FinishDelegate;

	void Internal_Update();

	int32 ComboTypeIndex = INDEX_NONE;
};


#define K_FRIENDLY_COOLDOWN_TIMER 180.0f

struct FFriendlyParams
{
public:
	void Begin();
	bool IsRunning() const;
	void ClearTask();

	void Reset();
	void AddCache(AActor* Actor);
	void RemoveCache();
	bool HasCache(AActor* Actor) const;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> FriendyCacheActors;

	UE::Tasks::FTask TaskInstance;
	bool bIsFriendlyCoolDownPlaying = false;
};


