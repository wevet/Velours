// Copyright 2022 wevet works All Rights Reserved.

#include "CustomIKData.h"
#include "QuadrupedIK.h"

#include "Animation/AnimInstanceProxy.h"
#include "Engine/SkeletalMeshSocket.h"


FCustomBoneSocketTarget::FCustomBoneSocketTarget(FName InName/* = NAME_None*/, bool bInUseSocket/* = false*/)
{
	bUseSocket = bInUseSocket;

	if (bUseSocket)
	{
		SocketReference.SocketName = InName;
	}
	else
	{
		BoneReference.BoneName = InName;
	}
}

void FCustomBoneSocketTarget::Initialize(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	if (bUseSocket)
	{
		SocketReference.InitializeSocketInfo(InAnimInstanceProxy);
	}
}

void FCustomSocketReference::InitializeSocketInfo(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	CachedSocketMeshBoneIndex = INDEX_NONE;
	CachedSocketCompactBoneIndex = FCompactPoseBoneIndex(INDEX_NONE);

	if (SocketName != NAME_None)
	{
		const USkeletalMeshComponent* OwnerMeshComponent = InAnimInstanceProxy->GetSkelMeshComponent();
		if (OwnerMeshComponent && OwnerMeshComponent->DoesSocketExist(SocketName))
		{
			USkeletalMeshSocket const* const Socket = OwnerMeshComponent->GetSocketByName(SocketName);
			if (Socket)
			{
				CachedSocketLocalTransform = Socket->GetSocketLocalTransform();
				// cache mesh bone index, so that we know this is valid information to follow
				CachedSocketMeshBoneIndex = OwnerMeshComponent->GetBoneIndex(Socket->BoneName);
				ensureMsgf(CachedSocketMeshBoneIndex != INDEX_NONE, TEXT("%s : socket has invalid bone."), *SocketName.ToString());
			}
		}
		else
		{
			// @todo : move to graph node warning
			UE_LOG(LogAnimation, Warning, TEXT("%s: socket doesn't exist"), *SocketName.ToString());
		}
	}
}

void FCustomSocketReference::InitialzeCompactBoneIndex(const FBoneContainer& RequiredBones)
{
	if (CachedSocketMeshBoneIndex != INDEX_NONE)
	{
		const FMeshPoseBoneIndex MeshBoneIndex(CachedSocketMeshBoneIndex);
		//auto A = RequiredBones.GetSkeletonPoseIndexFromMeshPoseIndex();
		const FSkeletonPoseBoneIndex BoneIndex = RequiredBones.GetSkeletonPoseIndexFromMeshPoseIndex(MeshBoneIndex);
		//const int32 SocketBoneSkeletonIndex = RequiredBones.GetPoseToSkeletonBoneIndexArray()[CachedSocketMeshBoneIndex];
		CachedSocketCompactBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(BoneIndex.GetInt());
	}
}

void FCustomBoneSocketTarget::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	if (bUseSocket)
	{
		SocketReference.InitialzeCompactBoneIndex(RequiredBones);
		BoneReference.InvalidateCachedBoneIndex();
	}
	else
	{
		BoneReference.Initialize(RequiredBones);
		SocketReference.InvalidateCachedBoneIndex();
	}
}


bool FCustomBoneSocketTarget::HasValidSetup() const
{
	if (bUseSocket)
	{
		return SocketReference.HasValidSetup();
	}
	return BoneReference.BoneIndex != INDEX_NONE;
}


bool FCustomBoneSocketTarget::HasTargetSetup() const
{
	if (bUseSocket)
	{
		return (SocketReference.SocketName != NAME_None);
	}
	return (BoneReference.BoneName != NAME_None);
}


FName FCustomBoneSocketTarget::GetTargetSetup() const
{
	if (bUseSocket)
	{
		return (SocketReference.SocketName);
	}
	return (BoneReference.BoneName);
}

bool FCustomBoneSocketTarget::IsValidToEvaluate(const FBoneContainer& RequiredBones) const
{
	if (bUseSocket)
	{
		return SocketReference.IsValidToEvaluate();
	}
	return BoneReference.IsValidToEvaluate(RequiredBones);
}


FCompactPoseBoneIndex FCustomBoneSocketTarget::GetCompactPoseBoneIndex() const
{
	if (bUseSocket)
	{
		return SocketReference.GetCachedSocketCompactBoneIndex();
	}
	return BoneReference.CachedCompactPoseIndex;
}


#pragma region ToePathInfo
void FPredictionToePathInfo::Reset()
{
	IsPathValid = false;
	IsPathStarted = false;
	ToeFloorState = EPredictionToeFloorState::None;
}


void FPredictionToePathInfo::Update(const USkeletalMeshComponent* InSkMeshComp,
	const FVector& InRightToeCSPos,
	const FVector& InLeftToeCSPos,
	const EPredictionMotionFoot& InFoot,
	const FName& InToeName)
{
	CurToeCSPos = InFoot == EPredictionMotionFoot::Right ? InRightToeCSPos : InLeftToeCSPos;

	if (CurToeCSPos.IsNearlyZero())
	{
		Reset();
		return;
	}

	CurToePos = InSkMeshComp->GetComponentTransform().ToMatrixWithScale().TransformPosition(CurToeCSPos);

	// @NOTE
	// ‘Š‘ÎÀ•W‚É‚µ‚È‚¢‚Ælost‚·‚é‚Ì‚ÅCSÀ•W”äŠr
	EPredictionToeFloorState LocalToeFloorState = CurToeCSPos.Z < ToeContactFloorHeight ? EPredictionToeFloorState::Contacting : EPredictionToeFloorState::Leaving;

	if (IsContacting() && LocalToeFloorState == EPredictionToeFloorState::Leaving)
	{
		LeaveFloorPos = CurToePos;
		LocalToeFloorState = EPredictionToeFloorState::LeaveStart;
	}

	if (IsLeaving() && LocalToeFloorState == EPredictionToeFloorState::Contacting)
	{
		ContactFloorPos = CurToePos;
		LocalToeFloorState = EPredictionToeFloorState::ContactStart;
	}

	ToeFloorState = LocalToeFloorState;
	SetupPath(InToeName);
}

void FPredictionToePathInfo::SetupPath(const FName& InToeName)
{
	if (IsLeaveStart())
	{
		IsPathStarted = true;
	}

	if (IsContacStart())
	{
		FVector ToePathTranslation = ContactFloorPos - LeaveFloorPos;
		float TranslationSizeSquared = ToePathTranslation.SizeSquared();
		if (100.f * 100.f <= TranslationSizeSquared && TranslationSizeSquared <= 2000.f * 2000.f) // magic num
		{
			IsPathValid = true;
			PathTranslation = FVector(ToePathTranslation.X, ToePathTranslation.Y, 0.f);
			UE_LOG(LogQuadrupedIK, Verbose, TEXT("%s Path: %s PathSize: %f"), *InToeName.ToString(), *PathTranslation.ToString(), PathTranslation.Size2D());
		}
	}

}

bool FPredictionToePathInfo::IsInvalidState() const
{
	return ToeFloorState == EPredictionToeFloorState::None;
}

bool FPredictionToePathInfo::IsContacting() const
{
	return ToeFloorState == EPredictionToeFloorState::ContactStart || ToeFloorState == EPredictionToeFloorState::Contacting;
}

bool FPredictionToePathInfo::IsLeaving() const
{
	return ToeFloorState == EPredictionToeFloorState::LeaveStart || ToeFloorState == EPredictionToeFloorState::Leaving;
}

bool FPredictionToePathInfo::IsLeaveStart() const
{
	return ToeFloorState == EPredictionToeFloorState::LeaveStart;
}

bool FPredictionToePathInfo::IsContacStart() const
{
	return ToeFloorState == EPredictionToeFloorState::ContactStart;
}

void FPredictionToePathInfo::SetToeContactFloorHeight(float InHeight)
{
	ToeContactFloorHeight = InHeight;
}

void FPredictionToePathInfo::SetDefaultPathDistance(float InDist)
{
	DefaultPathDistance = InDist;
}
#pragma endregion



void FArmSolverWorkArea::Initialize(const FBoneContainer& RequiredBones, const FCustomBone_ArmsData& Settings)
{
	ResultClavicle = FBoneTransform(Settings.ClavicleBone.GetCompactPoseIndex(RequiredBones), FTransform::Identity);
	ResultShoulder = FBoneTransform(Settings.ShoulderBone.GetCompactPoseIndex(RequiredBones), FTransform::Identity);
	ResultElbow = FBoneTransform(Settings.ElbowBone.GetCompactPoseIndex(RequiredBones), FTransform::Identity);
	ResultHand = FBoneTransform(Settings.HandBone.GetCompactPoseIndex(RequiredBones), FTransform::Identity);

	OrigClavicle.BoneIndex = ResultClavicle.BoneIndex;
	OrigShoulder.BoneIndex = ResultShoulder.BoneIndex;
	OrigElbow.BoneIndex = ResultElbow.BoneIndex;
	OrigHand.BoneIndex = ResultHand.BoneIndex;

	bHasClavicle = (ResultClavicle.BoneIndex != INDEX_NONE);
	bIsInitialized = false;
}


FAxis FAimBoneAxisSetting::ToFAxis() const
{
	FAxis NewAxis;
	NewAxis.Axis = Axis.GetSafeNormal();
	NewAxis.bInLocalSpace = bInLocalSpace;
	return NewAxis;
}

