// Copyright 2022 wevet works All Rights Reserved.


#include "Misc/ABPFunctionLibrary.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ABPFunctionLibrary)

UAnimInstance* UABPFunctionLibrary::GetLinkedAnimLayerInstance(USkeletalMeshComponent* Mesh, TSubclassOf<UAnimInstance> LinkedAnimInstanceClass)
{
	if (Mesh == nullptr || LinkedAnimInstanceClass == nullptr)
	{
		return nullptr;
	}

	UAnimInstance* RootAnimInstance = Mesh->GetAnimInstance();
	if (RootAnimInstance == nullptr)
	{
		return nullptr;
	}

	return RootAnimInstance->GetLinkedAnimLayerInstanceByClass(LinkedAnimInstanceClass, true);
}

UAnimInstance* UABPFunctionLibrary::GetLinkedAnimGraphInstanceByTag(USkeletalMeshComponent* Mesh, FName Tag)
{
	if (Mesh == nullptr || Tag.IsNone())
	{
		return nullptr;
	}

	UAnimInstance* RootAnimInstance = Mesh->GetAnimInstance();
	if (RootAnimInstance == nullptr)
	{
		return nullptr;
	}

	return RootAnimInstance->GetLinkedAnimGraphInstanceByTag(Tag);

}
