// Copyright 2022 wevet works All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomIKData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
//#include "CommonAnimTypes.h"
#include "QuadrupedIKLibrary.generated.h"

struct FAxis;
class USkeletalMeshComponent;

/**
 * 
 */
UCLASS()
class QUADRUPEDIK_API UQuadrupedIKLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static FRotator CustomLookRotation(const FVector& LookAt, const FVector& UpDirection);

	const static FName GetChildBone(const FName& BoneName, const USkeletalMeshComponent* SkeletalMeshComponent);

	const FVector SmoothApproach(
		const float InDeltaTimeSeconds,
		const FVector& PastPosition,
		const FVector& PastTargetPosition,
		const FVector& TargetPosition,
		const float Speed);

	const static FVector RotateAroundPoint(const FVector& InputPoint, const FVector& ForwardVector, const FVector& Origin, const float Angle);


	const static FVector ClampRotateVector(
		const FVector& InputPosition,
		const FVector& ForwardDirection,
		const FVector& OriginLocation,
		const float MinClampDegrees,
		const float MaxClampDegrees,
		const float H_ClampMin,
		const float H_ClampMax,
		const bool bIsUseNaturalRotations);


	static FORCEINLINE float RateToAlpha(const float RatePerSec, const float DeltaSeconds)
	{
		if (RatePerSec <= 0.f || DeltaSeconds <= 0.f)
		{
			return 0.f;

		}
		// 1 - exp(-Rate * dt) : FPS‚É‹­‚¢
		return 1.f - FMath::Exp(-RatePerSec * DeltaSeconds);
	}

	static const bool DoesContainsNaN(const TArray<FBoneTransform>& BoneTransforms);


	static void DrawDebugLineTraceSingle(const UWorld* World, const FVector& Start, const FVector& End, bool bHit, const FHitResult& OutHit, FLinearColor TraceColor, FLinearColor TraceHitColor);
	static void DrawDebugSweptSphere(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority);
	static void DrawDebugSphereTraceSingle(const UWorld* World, const FVector& Start, const FVector& End, float Radius, bool bHit, const FHitResult& OutHit, FLinearColor TraceColor, FLinearColor TraceHitColor);
	static void DrawDebugSweptBox(const UWorld* InWorld, FVector const& Start, FVector const& End, FRotator const& Orientation, FVector const& HalfSize, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority);
	static void DrawDebugBoxTraceSingle(const UWorld* World, const FVector& Start, const FVector& End, const FVector HalfSize, const FQuat Orientation, bool bHit, const FHitResult& OutHit, FLinearColor TraceColor, FLinearColor TraceHitColor);


	static void GetSimpleHitResult(TArray<FHitResult>& HitResults, const float NormalDotThreshold, FHitResult& OutHitResult);
};
