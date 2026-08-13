// Copyright 2022 wevet works All Rights Reserved.

#include "AnimNode_CacheToePosForFootIK.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"


void FAnimNode_CacheToePosForFootIK::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" Target: %s)"), *LeftToe.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_CacheToePosForFootIK::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);
	FinalWeight = Context.GetFinalBlendWeight();
}

void FAnimNode_CacheToePosForFootIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
	check(OutBoneTransforms.Num() == 0);

	// The method for applying transforms is the same as for FMatrix or FTransform.
	// Apply scaling first, followed by rotation and translation.
	// If you want to apply translation first, you'll need two nodes: the first node handles translation, and the second node handles rotation.
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	if (PredictionFootIKComponent)
	{
		FCompactPoseBoneIndex RightToeCompactPoseBone = RightToe.GetCompactPoseIndex(BoneContainer);
		FCompactPoseBoneIndex LeftToeCompactPoseBone = LeftToe.GetCompactPoseIndex(BoneContainer);
		FTransform RightBoneTM = Output.Pose.GetComponentSpaceTransform(RightToeCompactPoseBone);
		FTransform LeftBoneTM = Output.Pose.GetComponentSpaceTransform(LeftToeCompactPoseBone);

		PredictionFootIKComponent->SetToeCSPos(RightBoneTM.GetLocation(), LeftBoneTM.GetLocation(), FinalWeight);
	}
}

bool FAnimNode_CacheToePosForFootIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return LeftToe.IsValidToEvaluate(RequiredBones) && RightToe.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_CacheToePosForFootIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)
	LeftToe.Initialize(RequiredBones);
	RightToe.Initialize(RequiredBones);
}

void FAnimNode_CacheToePosForFootIK::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	Super::Initialize_AnyThread(Context);


	auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();

	if (SK && SK->GetOwner())
	{
		PredictionFootIKComponent = SK->GetOwner()->FindComponentByClass<UPredictionFootIKComponent>();
	}


}

