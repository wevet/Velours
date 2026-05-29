// Copyright 2022 wevet works All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FullbodyIKSetting.generated.h"


UENUM(BlueprintType)
enum class EFullbodyIkEffectorType : uint8
{
	KeepLocation,
	KeepRotation,
	KeepLocationAndRotation,
	FollowOriginalLocation,
	FollowOriginalRotation,
	FollowOriginalLocationAndRotation,
};




USTRUCT(BlueprintType)
struct QUADRUPEDIK_API FFullbodyIKSolverAxis
{
	GENERATED_BODY()

public:
	FFullbodyIKSolverAxis() : Weight(1.f),
		LimitMin(-180.f),
		LimitMax(180.f),
		EtaBias(1.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	float Weight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	float LimitMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	float LimitMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	float EtaBias;
};


USTRUCT(BlueprintType)
struct QUADRUPEDIK_API FFullbodyIKSolver
{
	GENERATED_BODY()

public:
	FFullbodyIKSolver() : BoneName(NAME_None),
		bTranslation(false),
		bLimited(false),
		Mass(1.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	bool bTranslation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	bool bLimited;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	float Mass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	FFullbodyIKSolverAxis X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	FFullbodyIKSolverAxis Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IK)
	FFullbodyIKSolverAxis Z;
};



UCLASS(BlueprintType)
class QUADRUPEDIK_API UFullbodyIKSettingDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFullbodyIKSettingDataAsset() : ConvergenceDistance(0.1f),
		StepSize(10.f),
		StepLoopCountMax(10),
		EffectiveCountMax(10),
		EtaSize(0.f),
		JtJInverseBias(0.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FFullbodyIKSolver> Solvers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ConvergenceDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StepSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StepLoopCountMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EffectiveCountMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EtaSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float JtJInverseBias;
};



USTRUCT()
struct FSolverInternal
{
public:
	GENERATED_BODY()

	FSolverInternal() : BoneIndex(INDEX_NONE),
		ParentBoneIndex(INDEX_NONE),
		BoneIndicesIndex(INDEX_NONE),
		LocalTransform(FTransform::Identity),
		ComponentTransform(FTransform::Identity),
		InitLocalTransform(FTransform::Identity),
		InitComponentTransform(FTransform::Identity),
		bTranslation(false),
		bLimited(false),
		Mass(1.f)
	{
	}

	int32 BoneIndex;
	int32 ParentBoneIndex;
	int32 BoneIndicesIndex;
	FTransform LocalTransform;
	FTransform ComponentTransform;
	FTransform InitLocalTransform;
	FTransform InitComponentTransform;
	bool bTranslation;
	bool bLimited;
	float Mass;
	FFullbodyIKSolverAxis X;
	FFullbodyIKSolverAxis Y;
	FFullbodyIKSolverAxis Z;
};