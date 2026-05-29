

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HitReactParams.h"
#include "HitReactProfile.generated.h"


UCLASS(Blueprintable, BlueprintType)
class PROCHITREACT_API UHitReactProfile : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = HitReact, meta = (MultiLine = "true"))
	FString Description;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = HitReact)
	FHitReactPhysicsStateParams BlendParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HitReact, meta = (UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1", Delta = "0.05", ForceUnits = "%"))
	float MaxBlendWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bones, meta = (UIMin = "0", ClampMin = "0", UIMax = "12", Delta = "1.0", ForceUnits = "x"))
	float BoneBlendRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = HitReact, meta = (UIMin = "0", ClampMin = "0", UIMax = "1", Delta = "0.01", ForceUnits = "s"))
	float Cooldown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = HitReact)
	EHitReactMaxBlendHandling MaxBlendHandling;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = HitReact, meta = (UIMin = "1", ClampMin = "1", EditCondition = "MaxBlendHandling != EHitReactMaxBlendHandling::Disabled", EditConditionHides))
	int32 MaxActiveBlends;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	TArray<FHitReactSubsequentImpulse> SubsequentImpulseScalars;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Bones)
	TMap<FName, FName> RemapSimulatedBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Bones)
	TMap<FName, FHitReactBoneOverride> BoneOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Physics)
	FName PhysicalAnimProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Physics)
	FName ConstraintProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Performance, meta = (DisplayName = "LOD Threshold", ClampMin = "-1", UIMin = "-1"))
	int32 LODThreshold;

public:
	UHitReactProfile()
		: MaxBlendWeight(0.4f)
		, BoneBlendRate(10.f)
		, Cooldown(0.05f)
		, MaxBlendHandling(EHitReactMaxBlendHandling::Disabled)
		, MaxActiveBlends(50)
		, SubsequentImpulseScalars({
			{ 0.1f, 0.35f },
			{ 0.25f, 0.5f },
			{ 0.35f, 0.7f },
			{ 0.5f, 0.9f } })
			, PhysicalAnimProfile(NAME_None)
		, ConstraintProfile(NAME_None)
		, LODThreshold(-1)
	{
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

