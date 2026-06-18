// Copyright 2022 wevet works All Rights Reserved.


#include "Character/VBaseHuman.h"
#include "Component/WvSkeletalMeshComponent.h"

#include "Components/CapsuleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VBaseHuman)

AVBaseHuman::AVBaseHuman(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	CapsuleComponent = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, ABasePawn::CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(30.0f, 86.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = false;

	CapsuleComponent->SetLineThickness(0.5f);
	RootComponent = CapsuleComponent;


	SkeletalMeshComponent = ObjectInitializer.CreateDefaultSubobject<UWvSkeletalMeshComponent>(this, ABasePawn::MeshComponentName);
	SkeletalMeshComponent->SetupAttachment(CapsuleComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);


}

void AVBaseHuman::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVBaseHuman::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

#if WITH_EDITOR
void AVBaseHuman::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
}
#endif

void AVBaseHuman::BeginPlay()
{
	Super::BeginPlay();
}

void AVBaseHuman::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

UWvSkeletalMeshComponent* AVBaseHuman::GetWvSkeletalMeshComponent() const
{
	return SkeletalMeshComponent;
}


