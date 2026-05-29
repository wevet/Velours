// Copyright 2022 wevet works All Rights Reserved.

#include "AnimNode_CustomAimSolver.h"
#include "QuadrupedIKLibrary.h"

#include "Animation/AnimInstanceProxy.h"
#include "DrawDebugHelpers.h"
#include "AnimationRuntime.h"
#include "AnimationCoreLibrary.h"
#include "Algo/Reverse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"



#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_CustomAimSolver)

DECLARE_CYCLE_STAT(TEXT("CustomAimSolver Eval"), STAT_CustomAimSolver_EvalSKelControl, STATGROUP_Anim);

namespace AimHelper
{

	FRotator GetHandYaw(
		const bool bIsHandArm,
		FCustomBone_ArmsData& HandData,
		const FTransform& Body_Transform,
		const FTransform& OrigArmTransform,
		const FTransform& OrigHandTransform,
		const FTransform& CurArmTransform,
		const FTransform& CurHandTransform,
		const FTransform& UnmodifiedHandTransform,
		const FVector& Up_Vector_CS)
	{
		FTransform Composed_Rot = FTransform::Identity;
		FTransform Arm_Forward_Point = CurHandTransform;
		Arm_Forward_Point.SetLocation(CurHandTransform.GetLocation() +
			(CurHandTransform.GetLocation() - CurArmTransform.GetLocation()).GetSafeNormal() * 1000);

		const FTransform Inverse_Point_Hand = CurHandTransform.Inverse() * UnmodifiedHandTransform;
		Arm_Forward_Point = Arm_Forward_Point * Inverse_Point_Hand;
		const FVector Arm_Hand_Dir = (CurHandTransform.GetLocation() - CurArmTransform.GetLocation()).GetSafeNormal();
		const FVector Hand_Point_Dir = (Arm_Forward_Point.GetLocation() - CurHandTransform.GetLocation()).GetSafeNormal();
		const FVector Arm_Hand_Cross_Dir = FVector::CrossProduct(Arm_Hand_Dir, Up_Vector_CS);

		const float DiffAngle_Forward = UKismetMathLibrary::DegAcos(FVector::DotProduct(Hand_Point_Dir, Arm_Hand_Dir));
		const float DiffAngle_Sideward = UKismetMathLibrary::DegAcos(FVector::DotProduct(Hand_Point_Dir, Up_Vector_CS));
		const float DirectionSign = FMath::Sign(FVector::CrossProduct(Hand_Point_Dir, Arm_Hand_Cross_Dir).Z);

		Composed_Rot.SetLocation(CurArmTransform.GetLocation());
		FTransform Current_Reference_Rot = FTransform::Identity;

		{
			Current_Reference_Rot.SetRotation(
				(CurHandTransform.GetLocation() - CurArmTransform.GetLocation()).GetSafeNormal().ToOrientationQuat());
		}

		FTransform Orig_Reference_Rot = FTransform::Identity;

		{
			Orig_Reference_Rot.SetRotation(
				(OrigHandTransform.GetLocation() - OrigArmTransform.GetLocation()).GetSafeNormal().ToOrientationQuat());
		}

		float Forward_Alpha = FMath::Clamp(DiffAngle_Forward, 90, 180);
		Forward_Alpha = UKismetMathLibrary::NormalizeToRange(Forward_Alpha, 90, 180);
		Forward_Alpha = Forward_Alpha > 0 ? 1.0f : 0.0f;

		FTransform OffsetTransformMod = FTransform::Identity;
		OffsetTransformMod.SetRotation(FRotator(0.0f, 0.0f, -180).Quaternion());

		FTransform Orig_Hand_Rel = OrigHandTransform * Orig_Reference_Rot.Inverse();
		FTransform Orig_Arm_Rel = OrigArmTransform * Orig_Reference_Rot.Inverse();

		FTransform Current_Hand_Rel = CurHandTransform * Current_Reference_Rot.Inverse();
		FTransform Current_Arm_Rel = CurArmTransform * Current_Reference_Rot.Inverse();

		Composed_Rot.SetRotation((Orig_Hand_Rel.Inverse() * Orig_Arm_Rel).GetRotation());
		Composed_Rot.SetRotation((Current_Hand_Rel.Inverse() * Current_Arm_Rel).GetRotation().Inverse() * Composed_Rot.GetRotation());

		float Composed_Roll = Composed_Rot.Rotator().Roll;
		float Angular_Offset1 = FMath::Clamp(DiffAngle_Sideward, 0.0f, 90.0f);
		Angular_Offset1 = UKismetMathLibrary::NormalizeToRange(Angular_Offset1, 0.0f, 90.0f);

		float Angular_Offset2 = FMath::Clamp(DiffAngle_Sideward, 90.0f, 180.0f);
		Angular_Offset2 = 1 - UKismetMathLibrary::NormalizeToRange(Angular_Offset2, 90.0f, 180.0f);
		float SideMultiplier = FMath::Min(Angular_Offset1, Angular_Offset2);

		if (DiffAngle_Forward < 90.0f)
		{
			//	Composed_Roll = Composed_Roll;
		}
		else
		{
			Composed_Roll = -Composed_Roll + HandData.TwistOffsetReverse;
		}

		if (bIsHandArm)
		{
			if ((DiffAngle_Forward < 85.0f || DiffAngle_Forward > 115.0f))
			{
				HandData.LastHandRotation.Roll = Composed_Roll;
			}
			else
			{
				return FRotator(0.0f, 0.0f, HandData.LastHandRotation.Roll);
			}
		}
		return FRotator(0.0f, 0.0f, Composed_Roll);
	}


	FTransform SetArmYaw(
		const bool InvertTwist,
		const bool bIsRightHand,
		const float Roll,
		const FTransform& BodyTransform,
		const FTransform& OriginalArmTransform,
		const FTransform& OriginalHandTransform,
		const FTransform& CurrentArmTransform,
		const FTransform& CurrentHandTransform)
	{

		FTransform ComposedRot = CurrentArmTransform;
		FTransform ReferenceRot = FTransform::Identity;
		ReferenceRot.SetRotation((CurrentHandTransform.GetLocation() - CurrentArmTransform.GetLocation()).GetSafeNormal().ToOrientationQuat());
		ComposedRot.SetRotation(ReferenceRot.GetRotation().Inverse() * ComposedRot.GetRotation());

		int32 InvTwist = 1;

		if (InvertTwist)
		{
			InvTwist = -1;
		}
		ComposedRot.SetRotation(FRotator(0.0f, 0.0f, Roll * InvTwist).Quaternion() * ComposedRot.GetRotation());
		ComposedRot.SetRotation(ReferenceRot.GetRotation() * ComposedRot.GetRotation());
		return ComposedRot;
	}


	void Solve_Modified_TwoBoneIK(
		const FVector& RootPos,
		const FVector& JointPos,
		const FVector& EndPos,
		const FVector& JointTarget,
		const FVector& Effector,
		const FVector& ThighEffector,
		FVector& OutJointPos,
		FVector& OutEndPos,
		float UpperLimbLength,
		float LowerLimbLength,
		bool bAllowStretching,
		float StartStretchRatio,
		float MaxStretchScale)
	{
		// This is our reach goal.
		FVector DesiredPos = Effector;
		FVector DesiredDelta = DesiredPos - RootPos;
		float DesiredLength = DesiredDelta.Size();

		// Find lengths of upper and lower limb in the ref skeleton.
		// Use actual sizes instead of ref skeleton, so we take into account translation and scaling from other bone controllers.
		float MaxLimbLength = LowerLimbLength + UpperLimbLength;

		// Check to handle case where DesiredPos is the same as RootPos.
		FVector	DesiredDir;
		if (DesiredLength < KINDA_SMALL_NUMBER)
		{
			DesiredLength = KINDA_SMALL_NUMBER;
			DesiredDir = FVector(1.0f, 0.0f, 0.0f);
		}
		else
		{
			DesiredDir = DesiredDelta.GetSafeNormal();
		}

		// Get joint target (used for defining plane that joint should be in).
		FVector JointTargetDelta = JointTarget - RootPos;
		const float JointTargetLengthSqr = JointTargetDelta.SizeSquared();

		// Same check as above, to cover case when JointTarget position is the same as RootPos.
		FVector JointPlaneNormal, JointBendDir;
		if (JointTargetLengthSqr < FMath::Square(KINDA_SMALL_NUMBER))
		{
			JointBendDir = FVector(0, 1, 0);
			JointPlaneNormal = FVector(0, 0, 1);
		}
		else
		{
			JointPlaneNormal = DesiredDir ^ JointTargetDelta;
			// If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
			// we have to just pick 2 random vector perp to DesiredDir and each other.
			if (JointPlaneNormal.SizeSquared() < FMath::Square(KINDA_SMALL_NUMBER))
			{
				DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
			}
			else
			{
				JointPlaneNormal.Normalize();
				// Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
				// This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
				JointBendDir = JointTargetDelta - ((JointTargetDelta | DesiredDir) * DesiredDir);
				JointBendDir.Normalize();
			}
		}

		if (bAllowStretching)
		{
			const float ScaleRange = MaxStretchScale - StartStretchRatio;
			if (ScaleRange > KINDA_SMALL_NUMBER && MaxLimbLength > KINDA_SMALL_NUMBER)
			{
				const float ReachRatio = DesiredLength / MaxLimbLength;
				const float ScalingFactor = (MaxStretchScale - 1.f) * FMath::Clamp((ReachRatio - StartStretchRatio) / ScaleRange, 0.f, 1.f);
				if (ScalingFactor > KINDA_SMALL_NUMBER)
				{
					LowerLimbLength *= (1.f + ScalingFactor);
					UpperLimbLength *= (1.f + ScalingFactor);
					MaxLimbLength *= (1.f + ScalingFactor);
				}
			}
		}

		OutEndPos = DesiredPos;
		OutJointPos = JointPos;

		// If we are trying to reach a goal beyond the length of the limb, clamp it to something solvable and extend limb fully.
		if (DesiredLength >= MaxLimbLength)
		{
			OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
			OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
		}
		else
		{
			// So we have a triangle we know the side lengths of. We can work out the angle between DesiredDir and the direction of the upper limb
			// using the sin rule:
			const float TwoAB = 2.f * UpperLimbLength * DesiredLength;

			const float CosAngle = (TwoAB != 0.f) ? ((UpperLimbLength * UpperLimbLength) + (DesiredLength * DesiredLength) - (LowerLimbLength * LowerLimbLength)) / TwoAB : 0.f;

			// If CosAngle is less than 0, the upper arm actually points the opposite way to DesiredDir, so we handle that.
			const bool bReverseUpperBone = (CosAngle < 0.f);

			// Angle between upper limb and DesiredDir
			// ACos clamps internally so we dont need to worry about out-of-range values here.
			const float Angle = FMath::Acos(CosAngle);

			// Now we calculate the distance of the joint from the root -> effector line.
			// This forms a right-angle triangle, with the upper limb as the hypotenuse.
			const float JointLineDist = UpperLimbLength * FMath::Sin(Angle);

			// And the final side of that triangle - distance along DesiredDir of perpendicular.
			// ProjJointDistSqr can't be neg, because JointLineDist must be <= UpperLimbLength because appSin(Angle) is <= 1.
			const float ProjJointDistSqr = (UpperLimbLength * UpperLimbLength) - (JointLineDist * JointLineDist);
			// although this shouldn't be ever negative, sometimes Xbox release produces -0.f, causing ProjJointDist to be NaN
			// so now I branch it. 						
			float ProjJointDist = (ProjJointDistSqr > 0.f) ? FMath::Sqrt(ProjJointDistSqr) : 0.f;
			if (bReverseUpperBone)
			{
				ProjJointDist *= -1.f;
			}
			// So now we can work out where to put the joint!
			OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
		}
	}


	void Solve_Modified_TwoBoneIK_3(
		FTransform& InOutRootTransform,
		FTransform& InOutJointTransform,
		FTransform& InOutEndTransform,
		const FVector& JointTarget,
		const FVector& Effector,
		const FVector& ThighEffector,
		float UpperLimbLength,
		float LowerLimbLength,
		bool bAllowStretching,
		float StartStretchRatio,
		float MaxStretchScale)
	{

		const FVector RootPos = InOutRootTransform.GetLocation();
		const FVector JointPos = InOutJointTransform.GetLocation();
		const FVector EndPos = InOutEndTransform.GetLocation();
		const FTransform Const_RootPos = InOutRootTransform;

		// IK solver
		FVector OutJointPos, OutEndPos;
		AimHelper::Solve_Modified_TwoBoneIK(
			RootPos,
			JointPos,
			EndPos,
			JointTarget,
			Effector,
			ThighEffector,
			OutJointPos,
			OutEndPos,
			UpperLimbLength,
			LowerLimbLength,
			bAllowStretching,
			StartStretchRatio,
			MaxStretchScale);

		// Update transform for upper bone.
		{
			// Get difference in direction for old and new joint orientations
			const FVector OldDir = (JointPos - RootPos).GetSafeNormal();
			const FVector NewDir = (OutJointPos - RootPos).GetSafeNormal();
			const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
			InOutRootTransform.SetTranslation(RootPos);
		}

		// update transform for middle bone
		{
			// Get difference in direction for old and new joint orientations
			const FVector OldDir = (RootPos - JointPos).GetSafeNormal();
			const FVector NewDir = (RootPos - OutJointPos).GetSafeNormal();
			const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
			InOutJointTransform.SetRotation(DeltaRotation * InOutJointTransform.GetRotation());
			InOutJointTransform.SetTranslation(OutJointPos);
		}

		// Update transform for end bone.
		// currently not doing anything to rotation
		// keeping input rotation
		// Set correct location for end bone.
		{
			const FVector OldDir = (EndPos - JointPos).GetSafeNormal();
			const FVector NewDir = (OutEndPos - OutJointPos).GetSafeNormal();
			const FQuat DeltaRotation = FQuat::FindBetweenNormals(OldDir, NewDir);
			InOutEndTransform.SetRotation(DeltaRotation * InOutEndTransform.GetRotation());
			InOutEndTransform.SetTranslation(OutEndPos);
		}
	}


	void Solve_Modified_TwoBoneIK_4(
		FTransform& InOutRootTransform,
		FTransform& InOutJointTransform,
		FTransform& InOutEndTransform,
		const FVector& JointTarget,
		const FVector& Effector,
		const FVector& ThighEffector,
		bool bAllowStretching,
		float StartStretchRatio,
		float MaxStretchScale)
	{
		const float LowerLimbLength = (InOutEndTransform.GetLocation() - InOutJointTransform.GetLocation()).Size();
		const float UpperLimbLength = (InOutJointTransform.GetLocation() - InOutRootTransform.GetLocation()).Size();
		AimHelper::Solve_Modified_TwoBoneIK_3(
			InOutRootTransform,
			InOutJointTransform,
			InOutEndTransform,
			JointTarget,
			Effector,
			ThighEffector,
			UpperLimbLength,
			LowerLimbLength,
			bAllowStretching,
			StartStretchRatio,
			MaxStretchScale);

	}


	void Solve_Modified_Direct_TwoBoneIK(
		const FVector& RootPos,
		const FVector& JointPos,
		const FVector& EndPos,
		const FVector& JointTarget,
		const FVector& Effector,
		FVector& OutJointPos,
		FVector& OutEndPos,
		float UpperLimbLength,
		float LowerLimbLength,
		bool bAllowStretching,
		float StartStretchRatio,
		float MaxStretchScale)
	{
		// This is our reach goal.
		FVector DesiredPos = Effector;
		FVector DesiredDelta = DesiredPos - RootPos;
		float DesiredLength = DesiredDelta.Size();

		// Find lengths of upper and lower limb in the ref skeleton.
		// Use actual sizes instead of ref skeleton, so we take into account translation and scaling from other bone controllers.
		float MaxLimbLength = LowerLimbLength + UpperLimbLength;

		// Check to handle case where DesiredPos is the same as RootPos.
		FVector	DesiredDir;
		if (DesiredLength < KINDA_SMALL_NUMBER)
		{
			DesiredLength = KINDA_SMALL_NUMBER;
			DesiredDir = FVector(1, 0, 0);
		}
		else
		{
			DesiredDir = DesiredDelta.GetSafeNormal();
		}

		// Get joint target (used for defining plane that joint should be in).
		FVector JointTargetDelta = JointTarget - RootPos;
		const float JointTargetLengthSqr = JointTargetDelta.SizeSquared();

		// Same check as above, to cover case when JointTarget position is the same as RootPos.
		FVector JointPlaneNormal, JointBendDir;
		if (JointTargetLengthSqr < FMath::Square(KINDA_SMALL_NUMBER))
		{
			JointBendDir = FVector(0, 1, 0);
			JointPlaneNormal = FVector(0, 0, 1);
		}
		else
		{
			JointPlaneNormal = DesiredDir ^ JointTargetDelta;
			// If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
			// we have to just pick 2 random vector perp to DesiredDir and each other.
			if (JointPlaneNormal.SizeSquared() < FMath::Square(KINDA_SMALL_NUMBER))
			{
				DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
			}
			else
			{
				JointPlaneNormal.Normalize();

				// Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
				// This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
				JointBendDir = JointTargetDelta - ((JointTargetDelta | DesiredDir) * DesiredDir);
				JointBendDir.Normalize();
			}
		}

		//UE_LOG(LogAnimationCore, Log, TEXT("UpperLimb : %0.2f, LowerLimb : %0.2f, MaxLimb : %0.2f"), UpperLimbLength, LowerLimbLength, MaxLimbLength);

		if (bAllowStretching)
		{
			const float ScaleRange = MaxStretchScale - StartStretchRatio;
			if (ScaleRange > KINDA_SMALL_NUMBER && MaxLimbLength > KINDA_SMALL_NUMBER)
			{
				const float ReachRatio = DesiredLength / MaxLimbLength;
				const float ScalingFactor = (MaxStretchScale - 1.f) * FMath::Clamp((ReachRatio - StartStretchRatio) / ScaleRange, 0.f, 1.f);
				if (ScalingFactor > KINDA_SMALL_NUMBER)
				{
					LowerLimbLength *= (1.f + ScalingFactor);
					UpperLimbLength *= (1.f + ScalingFactor);
					MaxLimbLength *= (1.f + ScalingFactor);
				}
			}
		}
		OutEndPos = DesiredPos;
		OutJointPos = JointPos;

		// If we are trying to reach a goal beyond the length of the limb, clamp it to something solvable and extend limb fully.
		if (DesiredLength >= MaxLimbLength)
		{
			OutEndPos = RootPos + (MaxLimbLength * DesiredDir);
			OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
		}
		else
		{
			// So we have a triangle we know the side lengths of. We can work out the angle between DesiredDir and the direction of the upper limb
			// using the sin rule:
			const float TwoAB = 2.f * UpperLimbLength * DesiredLength;
			const float CosAngle = (TwoAB != 0.f) ? ((UpperLimbLength * UpperLimbLength) + (DesiredLength * DesiredLength) - (LowerLimbLength * LowerLimbLength)) / TwoAB : 0.f;
			// If CosAngle is less than 0, the upper arm actually points the opposite way to DesiredDir, so we handle that.
			const bool bReverseUpperBone = (CosAngle < 0.f);
			// Angle between upper limb and DesiredDir
			// ACos clamps internally so we dont need to worry about out-of-range values here.
			const float Angle = FMath::Acos(CosAngle);
			// Now we calculate the distance of the joint from the root -> effector line.
			// This forms a right-angle triangle, with the upper limb as the hypotenuse.
			const float JointLineDist = UpperLimbLength * FMath::Sin(Angle);
			// And the final side of that triangle - distance along DesiredDir of perpendicular.
			// ProjJointDistSqr can't be neg, because JointLineDist must be <= UpperLimbLength because appSin(Angle) is <= 1.
			const float ProjJointDistSqr = (UpperLimbLength * UpperLimbLength) - (JointLineDist * JointLineDist);
			// although this shouldn't be ever negative, sometimes Xbox release produces -0.f, causing ProjJointDist to be NaN
			// so now I branch it. 						
			float ProjJointDist = (ProjJointDistSqr > 0.f) ? FMath::Sqrt(ProjJointDistSqr) : 0.f;
			if (bReverseUpperBone)
			{
				ProjJointDist *= -1.f;
			}
			// So now we can work out where to put the joint!
			OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
		}
	}


	void Solve_Modified_Direct_TwoBoneIK_3(
		const FTransform& ComponentTransform,
		const float WristRotation,
		FTransform& InOutRootTransform,
		FTransform& InOutJointTransform,
		FTransform& InOutEndTransform,
		const FVector& JointTarget,
		const FVector& Effector,
		float UpperLimbLength,
		float LowerLimbLength,
		bool bAllowStretching,
		float StartStretchRatio,
		float MaxStretchScale,
		const bool bIsUpArmTwistTech)
	{
		FVector OutJointPos, OutEndPos;
		FVector RootPos = InOutRootTransform.GetLocation();
		FVector JointPos = InOutJointTransform.GetLocation();
		FVector EndPos = InOutEndTransform.GetLocation();

		// IK solver
		Solve_Modified_Direct_TwoBoneIK(
			RootPos,
			JointPos,
			EndPos,
			JointTarget,
			Effector,
			OutJointPos,
			OutEndPos,
			UpperLimbLength,
			LowerLimbLength,
			bAllowStretching,
			StartStretchRatio,
			MaxStretchScale);

		// update transform for middle bone
		{
			// Get difference in direction for old and new joint orientations
			const FVector OldDir = (EndPos - JointPos).GetSafeNormal();
			const FVector NewDir = (OutEndPos - OutJointPos).GetSafeNormal();

			const FRotator Rot_Ref_01 = UQuadrupedIKLibrary::CustomLookRotation((EndPos - JointPos).GetSafeNormal(), FVector::UpVector);
			const FRotator Rot_Ref_02 = UQuadrupedIKLibrary::CustomLookRotation((OutEndPos - OutJointPos).GetSafeNormal(), FVector::UpVector);

			const FQuat Delta1 = Rot_Ref_02.Quaternion() * Rot_Ref_01.Quaternion().Inverse();
			const FQuat FBNormals = FQuat::FindBetweenNormals(OldDir, NewDir);
			FQuat DeltaRotation;

			if (bIsUpArmTwistTech)
			{
				DeltaRotation = Delta1.GetNormalized();
			}
			else
			{
				DeltaRotation = FBNormals.GetNormalized();
			}

			// Rotate our Joint quaternion by this delta rotation
			InOutJointTransform.SetRotation(DeltaRotation * InOutJointTransform.GetRotation());
			// And put joint where it should be.
			InOutJointTransform.SetTranslation(OutJointPos);
		}

		// Update transform for upper bone.
		{
			const FVector OldDir = (JointPos - RootPos).GetSafeNormal();
			const FVector NewDir = (OutJointPos - RootPos).GetSafeNormal();

			const FRotator Rot_Ref_01 = UQuadrupedIKLibrary::CustomLookRotation((JointPos - RootPos).GetSafeNormal(), FVector::UpVector);
			const FRotator Rot_Ref_02 = UQuadrupedIKLibrary::CustomLookRotation((OutJointPos - RootPos).GetSafeNormal(), FVector::UpVector);
			const FQuat Delta1 = Rot_Ref_02.Quaternion() * Rot_Ref_01.Quaternion().Inverse();
			const FQuat FBNormals = FQuat::FindBetweenNormals(OldDir, NewDir);

			FQuat DeltaRotation;

			if (bIsUpArmTwistTech)
			{
				DeltaRotation = Delta1.GetNormalized();
			}
			else
			{
				DeltaRotation = FBNormals.GetNormalized();
			}

			InOutRootTransform.SetRotation(DeltaRotation * InOutRootTransform.GetRotation());
			InOutRootTransform.SetTranslation(RootPos);
		}

		InOutEndTransform.SetTranslation(OutEndPos);
	}


	void Solve_Modified_Direct_TwoBoneIK_4(
		const FTransform ComponentTransform,
		const float WristRotation,
		FTransform& InOutRootTransform,
		FTransform& InOutJointTransform,
		FTransform& InOutEndTransform,
		const FVector& JointTarget,
		const FVector& Effector,
		const bool bAllowStretching,
		const float StartStretchRatio,
		const float MaxStretchScale,
		const bool bIsUpArmTwistTech)
	{
		const float LowerLimbLength = (InOutEndTransform.GetLocation() - InOutJointTransform.GetLocation()).Size();
		const float UpperLimbLength = (InOutJointTransform.GetLocation() - InOutRootTransform.GetLocation()).Size();
		Solve_Modified_Direct_TwoBoneIK_3(
			ComponentTransform,
			WristRotation,
			InOutRootTransform,
			InOutJointTransform,
			InOutEndTransform,
			JointTarget,
			Effector,
			UpperLimbLength,
			LowerLimbLength,
			bAllowStretching,
			StartStretchRatio,
			MaxStretchScale,
			bIsUpArmTwistTech);
	}



	FTransform LookAt_Processor_Helper(
		FTransform& ComponentBoneTransform,
		const FVector& HeadLocation,
		const FVector& OffsetVector,
		const FAxis& LookAtAxis,
		const float LookatClamp,
		const FRotator& InnerBodyClamp,
		bool bIsUseNaturalMethod/* = true*/,
		float UpRotationClamp/* = 1*/,
		float Intensity/* = 1*/)
	{
		const FVector TargetLocationInComponentSpace = OffsetVector;
		const FVector LookAtVector = LookAtAxis.GetTransformedAxis(ComponentBoneTransform).GetSafeNormal();
		const FVector TargetDir = (TargetLocationInComponentSpace - HeadLocation).GetSafeNormal();
		FVector HorizontalTargetDir = TargetDir;
		FVector OppositeTargetDir = (HeadLocation - TargetLocationInComponentSpace).GetSafeNormal();
		OppositeTargetDir.Y = 0;

		const float AimClampInRadians = FMath::DegreesToRadians(FMath::Min(LookatClamp, 180.f));
		const float DiffAngle = FMath::Acos(FVector::DotProduct(LookAtVector, OppositeTargetDir));

		FVector SelectedVector = HorizontalTargetDir;

		if (DiffAngle > AimClampInRadians)
		{
			//check(DiffAngle > 0.f);
			FVector DeltaTarget = OppositeTargetDir - LookAtVector;
			DeltaTarget *= (AimClampInRadians / DiffAngle);
			OppositeTargetDir = LookAtVector + DeltaTarget;
			OppositeTargetDir.Normalize();
		}

		const float AimClampInRadiansForward = FMath::DegreesToRadians(FMath::Min(LookatClamp, 180.f));
		const float DiffAngleForward = FMath::Acos(FVector::DotProduct(LookAtVector, HorizontalTargetDir));

		if (DiffAngleForward > AimClampInRadiansForward)
		{
			//check(DiffAngleForward > 0.f);
			FVector DeltaTargetForward = HorizontalTargetDir - LookAtVector;
			DeltaTargetForward *= (AimClampInRadiansForward / DiffAngleForward);
			HorizontalTargetDir = LookAtVector + DeltaTargetForward;
			HorizontalTargetDir.Normalize();
		}

		SelectedVector = HorizontalTargetDir;
		SelectedVector.Z = SelectedVector.Z * UpRotationClamp;

		FQuat NormalizedDelta = FQuat::FindBetweenNormals(LookAtVector, SelectedVector);

		if (!bIsUseNaturalMethod)
		{
			const FRotator RotRef1 = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, SelectedVector * 100);
			const FRotator RotRef2 = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, LookAtVector * 100);
			NormalizedDelta = RotRef1.Quaternion() * RotRef2.Quaternion().Inverse();
		}

		const FQuat NormalizedDeltaRef = FQuat::FindBetweenNormals(LookAtVector, LookAtVector);
		FVector RotEuler = NormalizedDelta.Euler();
		RotEuler.X = FMath::ClampAngle(RotEuler.X, -LookatClamp, LookatClamp);
		RotEuler.Y = FMath::ClampAngle(RotEuler.Y, -LookatClamp, LookatClamp);
		RotEuler.Z = FMath::ClampAngle(RotEuler.Z, -LookatClamp, LookatClamp);
		NormalizedDelta = FQuat::MakeFromEuler(RotEuler);
		NormalizedDelta = FQuat::Slerp(NormalizedDeltaRef, NormalizedDelta, Intensity);
		FTransform WSRotationTransform = ComponentBoneTransform;
		FRotator NormalizedDeltaRot = NormalizedDelta.Rotator();
		FRotator InnerBodyClampAbs = InnerBodyClamp;
		InnerBodyClampAbs.Pitch = FMath::Abs(InnerBodyClampAbs.Pitch);
		InnerBodyClampAbs.Yaw = FMath::Abs(InnerBodyClampAbs.Yaw);
		InnerBodyClampAbs.Roll = FMath::Abs(InnerBodyClampAbs.Roll);

		if (InnerBodyClampAbs.Pitch > 0)
		{
			NormalizedDeltaRot.Pitch = FMath::ClampAngle(NormalizedDeltaRot.Pitch, InnerBodyClampAbs.Pitch, -InnerBodyClampAbs.Pitch);
			if (NormalizedDeltaRot.Pitch > 0)
			{
				NormalizedDeltaRot.Pitch = NormalizedDeltaRot.Pitch - InnerBodyClampAbs.Pitch;
			}
			else
			{
				NormalizedDeltaRot.Pitch = NormalizedDeltaRot.Pitch + InnerBodyClampAbs.Pitch;
			}
		}

		if (InnerBodyClampAbs.Roll > 0)
		{
			NormalizedDeltaRot.Roll = FMath::ClampAngle(NormalizedDeltaRot.Roll, InnerBodyClampAbs.Roll, -InnerBodyClampAbs.Roll);
			if (NormalizedDeltaRot.Roll > 0)
			{
				NormalizedDeltaRot.Roll = NormalizedDeltaRot.Roll - InnerBodyClampAbs.Roll;
			}
			else
			{
				NormalizedDeltaRot.Roll = NormalizedDeltaRot.Roll + InnerBodyClampAbs.Roll;
			}
		}

		if (InnerBodyClampAbs.Yaw > 0)
		{
			NormalizedDeltaRot.Yaw = FMath::ClampAngle(NormalizedDeltaRot.Yaw, InnerBodyClampAbs.Yaw, -InnerBodyClampAbs.Yaw);
			if (NormalizedDeltaRot.Yaw > 0)
			{
				NormalizedDeltaRot.Yaw = NormalizedDeltaRot.Yaw - InnerBodyClampAbs.Yaw;
			}
			else
			{
				NormalizedDeltaRot.Yaw = NormalizedDeltaRot.Yaw + InnerBodyClampAbs.Yaw;
			}
		}
		WSRotationTransform.SetRotation(NormalizedDeltaRot.Quaternion());
		ComponentBoneTransform.SetRotation(WSRotationTransform.GetRotation() * ComponentBoneTransform.GetRotation());
		return ComponentBoneTransform;
	}

	/// <summary>
	/// Head Fabrik IK
	/// </summary>
	void Evaluate_ConsecutiveBoneRotations(
		FComponentSpacePoseContext& Output,
		FRuntimeFloatCurve& LookBendingCurve,
		const FBoneReference& RootBoneInput,
		const FBoneReference& TipBoneInput,
		const float LookatRadius,
		const FRotator& InnerBodyClamp,
		const FTransform& EffectorTransform,
		const FAxis& LookAtAxis,
		const FAxis& IntermediateLookAtAxis,
		const bool bUseIntermediateAxis,
		const float LookatClamp,
		const float VerticalDipTreshold,
		const float DownwardDipMultiplier,
		const float InvertedDipMultiplier,
		const float SideMultiplier,
		const float SideDownMultiplier,
		const bool bIsAlterPelvis,
		const FTransform PelvisLocTarget,
		FRuntimeFloatCurve& BendingMultiplierCurve,
		const float UpRotClamp,
		const bool bIsUseNaturalRotation,
		const bool bIsSeparateHeadClamp,
		const float HeadClampValue,
		const FTransform& HeadTransf,
		const bool bIsHeadRotOverride,
		TArray<FBoneTransform>& OutBoneTransforms)
	{
		const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

		FTransform CSEffectorTransform = EffectorTransform;
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(Output.AnimInstanceProxy->GetComponentTransform(),
			Output.Pose,
			CSEffectorTransform, RootBoneInput.GetCompactPoseIndex(BoneContainer),
			EBoneControlSpace::BCS_WorldSpace);

		FVector CSEffectorLocation = CSEffectorTransform.GetLocation();

		// Gather all bone indices between root and tip.
		TArray<FCompactPoseBoneIndex> BoneIndices;

		{
			const FCompactPoseBoneIndex RootIndex = RootBoneInput.GetCompactPoseIndex(BoneContainer);
			FCompactPoseBoneIndex BoneIndex = TipBoneInput.GetCompactPoseIndex(BoneContainer);
			do
			{
				BoneIndices.Insert(BoneIndex, 0);
				BoneIndex = Output.Pose.GetPose().GetParentBoneIndex(BoneIndex);
			} while (BoneIndex != RootIndex);
			BoneIndices.Insert(BoneIndex, 0);
		}

		const int32 NumTransforms = BoneIndices.Num();
		//OutBoneTransforms.AddUninitialized(NumTransforms);

		// Maximum length of skeleton segment at full extension
		float MaximumReach = 0;
		// Gather chain links. These are non zero length bones.
		TArray<FCCDIK_Modified_ChainLink> Chain;
		Chain.Reserve(NumTransforms);

		// Start with Root Bone
		{
			const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
			const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(RootBoneIndex);
			const FTransform& BoneLocalTransform = Output.Pose.GetComponentSpaceTransform(RootBoneIndex);
			OutBoneTransforms[0] = FBoneTransform(RootBoneIndex, BoneCSTransform);
			Chain.Add(FCCDIK_Modified_ChainLink(BoneCSTransform.GetLocation(), BoneLocalTransform.GetLocation(), BoneCSTransform.GetRotation(), 0.f, RootBoneIndex, 0));
		}

		// Go through remaining transforms
		for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
		{
			const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];
			const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);
			FTransform BoneLocalTransform = Output.Pose.GetLocalSpaceTransform(BoneIndex);
			const FVector BoneCSPosition = BoneCSTransform.GetLocation();
			OutBoneTransforms[TransformIndex] = FBoneTransform(BoneIndex, BoneCSTransform);
			// Calculate the combined length of this segment of skeleton
			const float BoneLength = FVector::Dist(BoneCSPosition, OutBoneTransforms[TransformIndex - 1].Transform.GetLocation());
			if (!FMath::IsNearlyZero(BoneLength))
			{
				Chain.Add(FCCDIK_Modified_ChainLink(BoneCSPosition, BoneLocalTransform.GetLocation(), BoneCSTransform.GetRotation(), BoneLength, BoneIndex, TransformIndex));
				MaximumReach += BoneLength;
			}
			else
			{
				// Mark this transform as a zero length child of the last link.
				// It will inherit position and delta rotation from parent link.
				FCCDIK_Modified_ChainLink& ParentLink = Chain[Chain.Num() - 1];
				ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
			}
		}

		const int32 NumChainLinks = Chain.Num();
		FTransform HeadLocation = Output.Pose.GetComponentSpaceTransform(BoneIndices[BoneIndices.Num() - 1]);
		const FTransform PelvisLocation = Output.Pose.GetComponentSpaceTransform(BoneIndices[0]);

		float UpLength = 0;
		UpLength = -(CSEffectorLocation.Z - Chain[NumChainLinks - 1].Position.Z);
		UpLength = FMath::Clamp(UpLength, -(FMath::Abs(Chain[NumChainLinks - 1].Position.Z * 0.5f)), 0);
		const FVector DiffPelvis = (PelvisLocation.GetLocation() - PelvisLocTarget.GetLocation());
		FTransform PelvisRefFullTransform;

		for (int32 LinkIndex = 0; LinkIndex < NumChainLinks; LinkIndex++)
		{
			const FCCDIK_Modified_ChainLink& ChainLink = Chain[LinkIndex];
			float NewValue = LookBendingCurve.GetRichCurve()->Eval((float)LinkIndex / (float)NumChainLinks);
			float MultiplierValue = BendingMultiplierCurve.GetRichCurve()->Eval((float)LinkIndex / (float)NumChainLinks);
			FVector TempTargetLoc = CSEffectorLocation;

			FAxis ActiveAxis = LookAtAxis;
			if (bUseIntermediateAxis && LinkIndex > 0 && LinkIndex < (NumChainLinks - 1))
			{
				ActiveAxis = IntermediateLookAtAxis;
			}

			if (LinkIndex == NumChainLinks - 1)
			{
				TempTargetLoc = HeadTransf.GetLocation();
			}

			if (LinkIndex == 0)
			{
				PelvisRefFullTransform = LookAt_Processor_Helper(
					OutBoneTransforms[0].Transform,
					HeadLocation.GetLocation(),
					TempTargetLoc, 
					LookAtAxis,
					80, 
					InnerBodyClamp,
					bIsUseNaturalRotation, 
					1.0f, 
					1.0f);
			}

			const FRotator OriginalRot = OutBoneTransforms[ChainLink.TransformIndex].Transform.Rotator();
			float LookAtModifiedClamp = LookatClamp * NewValue;
			float UpLookRatio = UpRotClamp;
			FRotator InnerBodyClampVal = InnerBodyClamp;

			if (bIsSeparateHeadClamp)
			{
				if (LinkIndex == NumChainLinks - 1)
				{
					LookAtModifiedClamp = HeadClampValue;
					UpLookRatio = 1;
					InnerBodyClampVal = FRotator(0, 0, 0);
					MultiplierValue = 1;
				}
			}

			if (LinkIndex == (NumChainLinks - 1) && bIsHeadRotOverride)
			{
				OutBoneTransforms[ChainLink.TransformIndex].Transform.SetRotation(CSEffectorTransform.GetRotation() *
					OutBoneTransforms[ChainLink.TransformIndex].Transform.GetRotation());
			}
			else
			{
				OutBoneTransforms[ChainLink.TransformIndex].Transform = LookAt_Processor_Helper(
					OutBoneTransforms[ChainLink.TransformIndex].Transform,
					HeadLocation.GetLocation(),
					TempTargetLoc,
					ActiveAxis,
					LookAtModifiedClamp,
					InnerBodyClampVal,
					bIsUseNaturalRotation,
					UpLookRatio,
					MultiplierValue);
			}


			if (bIsAlterPelvis)
			{
				if (LinkIndex == 0)
				{
					OutBoneTransforms[ChainLink.TransformIndex].Transform.SetLocation(PelvisLocTarget.GetLocation());
					OutBoneTransforms[ChainLink.TransformIndex].Transform.SetRotation(OriginalRot.Quaternion() * PelvisLocTarget.GetRotation());
					TempTargetLoc += DiffPelvis;
				}
			}

			FTransform OldParent = FTransform();
			FTransform CurrentParent = FTransform();

			if (ChainLink.TransformIndex > 0)
			{
				CurrentParent = OutBoneTransforms[ChainLink.TransformIndex - 1].Transform;
				const FCompactPoseBoneIndex& Parent_BoneIndex = BoneIndices[ChainLink.TransformIndex - 1];
				OldParent = Output.Pose.GetComponentSpaceTransform(Parent_BoneIndex);
			}

			//Get Delta result
			FTransform ParentDifference = OldParent.Inverse() * CurrentParent;
			OutBoneTransforms[ChainLink.TransformIndex].Transform.SetLocation(
				(OutBoneTransforms[ChainLink.TransformIndex].Transform * ParentDifference).GetLocation());
		}

		const FRotator InvertedPelvisRot = FRotator(PelvisRefFullTransform.GetRotation() * PelvisLocation.GetRotation().Inverse());
		const float Side_Angle = -InvertedPelvisRot.Yaw;
		float VerticalAngle = FMath::Abs(InvertedPelvisRot.Roll);
		VerticalAngle = FMath::Clamp(VerticalAngle - VerticalDipTreshold, 0, 1000);

		float VerticalDirectionVal = 1;

		if ((NumChainLinks - 1) > 0)
		{
			VerticalDirectionVal = CSEffectorLocation.Z > OutBoneTransforms[NumChainLinks - 1].Transform.GetLocation().Z ? 1 : -1;
		}

		const float SideDownVal = FMath::Abs(Side_Angle);
		const FVector RightDir = FVector::CrossProduct(LookAtAxis.Axis, FVector::UpVector);

		for (int32 LinkIndex = 0; LinkIndex < NumChainLinks; LinkIndex++)
		{
			const FCCDIK_Modified_ChainLink& ChainLink = Chain[LinkIndex];

			if (VerticalDirectionVal == 1)
			{
				OutBoneTransforms[ChainLink.TransformIndex].Transform.AddToTranslation(FVector(0, 0, 1) * DownwardDipMultiplier * VerticalAngle);
			}
			else
			{
				OutBoneTransforms[ChainLink.TransformIndex].Transform.AddToTranslation(FVector(0, 0, 1) * InvertedDipMultiplier * VerticalAngle);
			}
			OutBoneTransforms[ChainLink.TransformIndex].Transform.AddToTranslation(RightDir * SideMultiplier * Side_Angle);
			OutBoneTransforms[ChainLink.TransformIndex].Transform.AddToTranslation(FVector(0, 0, 1) * SideDownMultiplier * SideDownVal);
		}
	}


	/// <summary>
	/// Modify HandIK
	/// </summary>
	void Evaluate_TwoBoneIK_Direct_Modified(
		FComponentSpacePoseContext& Output,
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FBoneReference& HandBone,
		const FBoneReference& ElbowBone,
		const FBoneReference& ShoulderBone,
		const FTransform& ThighTransform,
		const FTransform& Shoulder,
		const FTransform& Knee,
		const FTransform& Hand,
		const FVector& JointLocation,
		const FVector& KneePoleOffset,
		const FTransform& TransformOffset,
		const FTransform& CommonSpineModifiedTransform,
		const FRotator& LimbRotationOffset,
		FCustomBone_ArmsData& HandData,
		const float HandClampValue,
		const FTransform& ExtraHandOffset,
		const FVector& ElbowPoleOffset,
		const bool bIsOverrideHandRotation,
		const FTransform& KneeTransformDefault,
		const FVector& LookAtAxis,
		const FVector& ReferenceConstantForwardAxis,
		float& LastShoulderAngle,
		const bool bIsUseNSEWPoles,
		const bool bIsUseUpArmTwist,
		const FVector UpVectorVal,
		const bool bIsSeparateArmsLogicUsed,
		const bool bIsReachMode,
		FArmSolverWorkArea& OutArmSolverWorkArea)
	{

		const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
		const FCompactPoseBoneIndex& CachedUpperLimbIndex = OutArmSolverWorkArea.ResultShoulder.BoneIndex; 
		const FCompactPoseBoneIndex& CachedLowerLimbIndex = OutArmSolverWorkArea.ResultElbow.BoneIndex; 
		const FCompactPoseBoneIndex& CachedFeetPoseIndex = OutArmSolverWorkArea.ResultHand.BoneIndex; 

		FCompactPoseBoneIndex CachedClavicleIndex = FCompactPoseBoneIndex(0);

		FTransform ClavicleOffset = FTransform::Identity;
		FTransform ClavicleTransform = FTransform::Identity;

		bool bIsShoulderTwistAvailable = false;

		if (OutArmSolverWorkArea.bHasClavicle && bIsSeparateArmsLogicUsed)
		{
			bIsShoulderTwistAvailable = true;
			CachedClavicleIndex = OutArmSolverWorkArea.ResultClavicle.BoneIndex; 

			ClavicleTransform = Output.Pose.GetComponentSpaceTransform(CachedClavicleIndex) * TransformOffset;
			const FTransform OrigClavicleTransform = ClavicleTransform;
			const FVector ClavicleDirection = (ClavicleTransform.GetLocation() - Shoulder.GetLocation()).GetSafeNormal();
			const FVector ClavicleEffectorDirection = (ClavicleTransform.GetLocation() - ThighTransform.GetLocation()).GetSafeNormal();
			FRotator ClavicleRotDifference = FQuat::FindBetweenVectors(ClavicleDirection, ClavicleEffectorDirection).Rotator();

			ClavicleRotDifference.Pitch = FMath::ClampAngle(ClavicleRotDifference.Pitch,
				FMath::Abs(HandData.InnerClavicle_VLimit.Y),
				-FMath::Abs(HandData.InnerClavicle_VLimit.X));

			ClavicleRotDifference.Yaw = FMath::ClampAngle(ClavicleRotDifference.Yaw,
				FMath::Abs(HandData.InnerClavicle_HLimit.X),
				-FMath::Abs(HandData.InnerClavicle_HLimit.Y));

			if (ClavicleRotDifference.Pitch > 0)
			{
				ClavicleRotDifference.Pitch = ClavicleRotDifference.Pitch - FMath::Abs(HandData.InnerClavicle_VLimit.Y);
			}
			else
			{
				ClavicleRotDifference.Pitch = ClavicleRotDifference.Pitch + FMath::Abs(HandData.InnerClavicle_VLimit.X);
			}

			if (ClavicleRotDifference.Yaw > 0)
			{
				ClavicleRotDifference.Yaw = ClavicleRotDifference.Yaw - FMath::Abs(HandData.InnerClavicle_HLimit.X);
			}
			else
			{
				ClavicleRotDifference.Yaw = ClavicleRotDifference.Yaw + FMath::Abs(HandData.InnerClavicle_HLimit.Y);
			}

			ClavicleRotDifference.Roll = 0;
			ClavicleRotDifference.Pitch = FMath::ClampAngle(ClavicleRotDifference.Pitch,
				-FMath::Abs(HandData.OuterClavicle_VLimit.X), FMath::Abs(HandData.OuterClavicle_VLimit.Y));
			ClavicleRotDifference.Yaw = FMath::ClampAngle(ClavicleRotDifference.Yaw,
				-FMath::Abs(HandData.OuterClavicle_HLimit.X), FMath::Abs(HandData.OuterClavicle_HLimit.Y));

			if (ClavicleRotDifference.Pitch != -FMath::Abs(HandData.OuterClavicle_VLimit.X) &&
				ClavicleRotDifference.Pitch != FMath::Abs(HandData.OuterClavicle_VLimit.Y))
			{
				HandData.LastClavicleRotation.Pitch = ClavicleRotDifference.Pitch;
			}

			if (ClavicleRotDifference.Yaw != -FMath::Abs(HandData.OuterClavicle_HLimit.X) &&
				ClavicleRotDifference.Yaw != FMath::Abs(HandData.OuterClavicle_HLimit.Y))
			{
				HandData.LastClavicleRotation.Yaw = ClavicleRotDifference.Yaw;
			}

			HandData.LastClavicleRotation.Roll = ClavicleRotDifference.Roll;
			ClavicleTransform.SetRotation(HandData.LastClavicleRotation.Quaternion() * ClavicleTransform.GetRotation());
			ClavicleOffset = Output.Pose.GetComponentSpaceTransform(CachedClavicleIndex).Inverse() * ClavicleTransform;
	
			OutArmSolverWorkArea.ResultClavicle.Transform = ClavicleTransform;
			OutArmSolverWorkArea.OrigClavicle.Transform = OrigClavicleTransform;
		}

		// Get Local Space transforms for our bones. We do this first in case they already are local.
		// As right after we get them in component space. (And that does the auto conversion).
		// We might save one transform by doing local first...
		const FTransform& EndBoneLocalTransform = Output.Pose.GetLocalSpaceTransform(CachedFeetPoseIndex);
		const FTransform& LowerLimbLocalTransform = Output.Pose.GetLocalSpaceTransform(CachedLowerLimbIndex);
		const FTransform& UpperLimbLocalTransform = Output.Pose.GetLocalSpaceTransform(CachedUpperLimbIndex);

		// Now get those in component space...
		FTransform LowerLimbCSTransform = Knee;
		FTransform UpperLimbCSTransform = Shoulder;
		FTransform EndBoneCSTransform = Hand;

		FTransform OrigLowerLimbCSTransform = Knee;
		FTransform OrigUpperLimbCSTransform = Shoulder;
		FTransform OrigEndBoneCSTransform = Hand;

		if (OutArmSolverWorkArea.bHasClavicle && bIsSeparateArmsLogicUsed)
		{
			LowerLimbCSTransform = Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex) * ClavicleOffset;
			UpperLimbCSTransform = Output.Pose.GetComponentSpaceTransform(CachedUpperLimbIndex) * ClavicleOffset;
			EndBoneCSTransform = Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex) * ClavicleOffset;

			OrigLowerLimbCSTransform = Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex) * ClavicleOffset;
			OrigUpperLimbCSTransform = Output.Pose.GetComponentSpaceTransform(CachedUpperLimbIndex) * ClavicleOffset;
			OrigEndBoneCSTransform = Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex) * ClavicleOffset;
		}

		FTransform OriginalElbowTransform = Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex);
		const FTransform EndBoneCSTransform_Const = Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex);
		FTransform EndBoneCSTransform_Always = EndBoneCSTransform_Const;
		EndBoneCSTransform.SetRotation(EndBoneCSTransform_Const.GetRotation());

		const FVector& RootPos = UpperLimbCSTransform.GetTranslation();
		const FVector& InitialJointPos = LowerLimbCSTransform.GetTranslation();
		const FVector& InitialEndPos = EndBoneCSTransform.GetTranslation();

		FTransform EffectorTransform = EndBoneCSTransform;

		FTransform PoleOrigTransform = FTransform::Identity;
		PoleOrigTransform.SetLocation(ElbowPoleOffset);

		FTransform PoleRefTransform = FTransform::Identity;
		PoleRefTransform.SetLocation(UpperLimbCSTransform.GetLocation());

		{
			const FQuat PoleRotDiff = FQuat::FindBetweenVectors((ThighTransform.GetLocation() - UpperLimbCSTransform.GetLocation()),
				(EffectorTransform.GetLocation() - UpperLimbCSTransform.GetLocation()));
			PoleRefTransform.SetRotation(PoleRotDiff);
		}

		// Get joint target (used for defining plane that joint should be in).
		FTransform JointTargetTransform = LowerLimbCSTransform;

		FQuat Forward_Rotation_Difference = FQuat::FindBetweenNormals(LookAtAxis, ReferenceConstantForwardAxis);
		FTransform FRP_Knee_Transform = FTransform::Identity;
		FRP_Knee_Transform.SetRotation(Forward_Rotation_Difference);
		FTransform Pole_Transform = FTransform::Identity;
		Pole_Transform.SetLocation(ElbowPoleOffset);

		FTransform Second_Pole_Transform = FTransform::Identity;
		Second_Pole_Transform.SetLocation(KneePoleOffset);

		FVector CS_Forward = (
			(
				(UpperLimbCSTransform.GetLocation() + EndBoneCSTransform.GetLocation() + LowerLimbCSTransform.GetLocation()) / 3) -
			(LowerLimbCSTransform.GetLocation() + Pole_Transform.GetLocation())).GetSafeNormal();

		if (bIsReachMode)
		{
			if (bIsUseNSEWPoles)
			{
				FVector forward_dir = LookAtAxis;
				FVector right_dir = FVector::CrossProduct(LookAtAxis, UpVectorVal);
				FVector Hand_Shoulder_dir = (ThighTransform.GetLocation() - UpperLimbCSTransform.GetLocation()).GetSafeNormal();
				float NS_Alpha = UKismetMathLibrary::DegAcos(FVector::DotProduct(forward_dir, Hand_Shoulder_dir)) / 180;
				float EW_Alpha = UKismetMathLibrary::DegAcos(FVector::DotProduct(right_dir, Hand_Shoulder_dir)) / 180;
				FVector NS_Aggregated_Pole;
				FVector EW_Aggregated_Pole;
				FVector Total_Aggregated_Pole;

				if (HandData.bIsRightHand)
				{
					NS_Aggregated_Pole = FMath::Lerp(HandData.NorthPoleOffset * 10, HandData.SouthPoleOffset * 10, EW_Alpha);
					EW_Aggregated_Pole = FMath::Lerp(HandData.EastPoleOffset * 10, HandData.WestPoleOffset * 10, NS_Alpha);
					Total_Aggregated_Pole = (NS_Aggregated_Pole + EW_Aggregated_Pole) / 2;
				}
				else
				{
					NS_Aggregated_Pole = FMath::Lerp(HandData.SouthPoleOffset * 10, HandData.NorthPoleOffset * 10, EW_Alpha);
					EW_Aggregated_Pole = FMath::Lerp(HandData.WestPoleOffset * 10, HandData.EastPoleOffset * 10, NS_Alpha);
					Total_Aggregated_Pole = (NS_Aggregated_Pole + EW_Aggregated_Pole) / 2;
				}
				JointTargetTransform.SetLocation(Total_Aggregated_Pole);
			}
			else
			{
				JointTargetTransform.SetLocation(OriginalElbowTransform.GetLocation() + Pole_Transform.GetLocation() * 10);
			}
		}
		else
		{
			JointTargetTransform.SetLocation(JointTargetTransform.GetLocation() + CS_Forward * -1000);
		}

		FVector	JointTargetPos = JointTargetTransform.GetLocation();
		// IK solver
		UpperLimbCSTransform.SetLocation(RootPos);
		LowerLimbCSTransform.SetLocation(InitialJointPos);
		EndBoneCSTransform.SetLocation(InitialEndPos);

		// This is our reach goal.
		FVector DesiredPos = ThighTransform.GetLocation();
		FVector Far_Target = CommonSpineModifiedTransform.GetLocation();
		FVector DesiredThighPos = SkeletalMeshComponent->GetComponentToWorld().InverseTransformPosition(ThighTransform.GetLocation());
		float WristOffset = 90;

		if (HandData.bIsRightHand)
		{
			WristOffset = -90;
		}

		AimHelper::Solve_Modified_Direct_TwoBoneIK_4(
			SkeletalMeshComponent->GetComponentTransform(),
			WristOffset,
			UpperLimbCSTransform,
			LowerLimbCSTransform,
			EndBoneCSTransform,
			JointTargetPos, DesiredPos,
			false, 1.0f, 1.0f, bIsUseUpArmTwist);


		FRotator InputArmRot = FRotator::ZeroRotator;
		float RollAlpha = 1.0f;
		float RollAbsolute = 0.0f;
		float RollLimit = 60.0f;

		// Update transform for end bone.
		{
			if (bIsOverrideHandRotation)
			{
				FQuat Default_Rot = (EndBoneLocalTransform * LowerLimbCSTransform).GetRotation();
				FTransform Unmodifed_Hand = (EndBoneLocalTransform * LowerLimbCSTransform);
				EndBoneCSTransform.SetRotation(ThighTransform.GetRotation() * EndBoneCSTransform.GetRotation());

				InputArmRot = GetHandYaw(
					true,
					HandData,
					SkeletalMeshComponent->GetComponentToWorld(),
					Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex),
					Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex),
					LowerLimbCSTransform,
					EndBoneCSTransform,
					Unmodifed_Hand,
					UpVectorVal);
			}
			else
			{
				if (ExtraHandOffset.Equals(FTransform::Identity))
				{
					EndBoneCSTransform.SetRotation((EndBoneLocalTransform * LowerLimbCSTransform).GetRotation());
				}
				else
				{
					EndBoneCSTransform.SetRotation(ExtraHandOffset.GetRotation() * EndBoneCSTransform.GetRotation());
				}
			}

			if (!HandData.bIsAccurateHandRotation)
			{
				OutArmSolverWorkArea.ResultHand.Transform = EndBoneCSTransform;
				OutArmSolverWorkArea.OrigHand.Transform = OrigEndBoneCSTransform;

			}
			else
			{
				FAxis AxisInstance;
				AxisInstance.bInLocalSpace = !HandData.bIsRelativeAxis;
				AxisInstance.Axis = HandData.LocalDirectionAxis;
				EndBoneCSTransform_Always.SetLocation(EndBoneCSTransform.GetLocation());

				FTransform EndBoneCSTransformAccurate = LookAt_Processor_Helper(
					EndBoneCSTransform_Always,
					EndBoneCSTransform_Always.GetLocation(),
					Far_Target, AxisInstance,
					HandClampValue,
					FRotator::ZeroRotator, true, 1.0f, 1.0f);

				OutArmSolverWorkArea.ResultHand.Transform = EndBoneCSTransformAccurate;
				OutArmSolverWorkArea.OrigHand.Transform = OrigEndBoneCSTransform;

			}
		}

		FTransform BS_EndBoneCSTransform = EndBoneCSTransform_Const.Inverse() * EndBoneCSTransform;
		FTransform BS_LowerLimbTransform = LowerLimbCSTransform;
		FTransform BS_UpperLimbTransform = UpperLimbCSTransform;
		FVector Arm_Direction = (LowerLimbCSTransform.GetLocation() - EndBoneCSTransform.GetLocation()).GetSafeNormal();
		FTransform Reference_Parent = FTransform::Identity;
		Reference_Parent.SetRotation(Arm_Direction.ToOrientationQuat());

		BS_LowerLimbTransform = SetArmYaw(
			HandData.bIsInvertLowerTwist,
			HandData.bIsRightHand,
			InputArmRot.Roll,
			SkeletalMeshComponent->GetComponentToWorld(),
			Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex),
			Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex),
			LowerLimbCSTransform,
			EndBoneCSTransform);

		float ForeArmAngle = UKismetMathLibrary::RadiansToDegrees((BS_LowerLimbTransform.GetRotation().Inverse() * LowerLimbCSTransform.GetRotation()).GetAngle());
		ForeArmAngle = FMath::UnwindDegrees(ForeArmAngle);
		RollLimit = 70.f;
		ForeArmAngle = FMath::ClampAngle(ForeArmAngle, -RollLimit, RollLimit);

		if (ForeArmAngle <= RollLimit)
		{
			RollAlpha = 1.0f;
		}
		else
		{
			RollAlpha = 0.0f;
		}

		float StartClamp = 25.0f;
		float EndClamp = -25.0f;

		InputArmRot.Roll = FMath::UnwindDegrees(InputArmRot.Roll);

		FVector2D ForearmLimitAbs = HandData.ForeArmAngleLimit;
		ForearmLimitAbs.X = FMath::Clamp(FMath::Abs(ForearmLimitAbs.X), 0.01f, 179.9f);
		ForearmLimitAbs.Y = FMath::Clamp(FMath::Abs(ForearmLimitAbs.Y), 0.01f, 179.9f);

		if (ForearmLimitAbs.X > 0 && ForearmLimitAbs.Y > 0)
		{
			InputArmRot.Roll = FMath::ClampAngle(InputArmRot.Roll, -ForearmLimitAbs.X, ForearmLimitAbs.Y);
			if (InputArmRot.Roll != -ForearmLimitAbs.X && InputArmRot.Roll != ForearmLimitAbs.Y)
			{
				HandData.LastForarmAngle = InputArmRot.Roll;
			}
		}
		else
		{
			HandData.LastForarmAngle = InputArmRot.Roll;
		}

		BS_LowerLimbTransform = SetArmYaw(
			HandData.bIsInvertLowerTwist,
			HandData.bIsRightHand,
			HandData.LastForarmAngle,
			SkeletalMeshComponent->GetComponentToWorld(),
			Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex),
			Output.Pose.GetComponentSpaceTransform(CachedFeetPoseIndex),
			LowerLimbCSTransform,
			EndBoneCSTransform);

		const FVector ShoulderArmDir = (UpperLimbCSTransform.GetLocation() - LowerLimbCSTransform.GetLocation()).GetSafeNormal();
		const FVector ArmHandDir = (LowerLimbCSTransform.GetLocation() - EndBoneCSTransform.GetLocation()).GetSafeNormal();
		float ArmAngle = (1 - 1.5f * (UKismetMathLibrary::DegAcos(FVector::DotProduct(ArmHandDir, ShoulderArmDir)) / 180.0f));

		ArmAngle = FMath::Clamp(ArmAngle, 0.0f, 1.0f);

		FRotator ForArmInputRot = GetHandYaw(
			false,
			HandData,
			SkeletalMeshComponent->GetComponentToWorld(),
			Output.Pose.GetComponentSpaceTransform(CachedUpperLimbIndex),
			Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex),
			BS_UpperLimbTransform,
			BS_LowerLimbTransform,
			BS_LowerLimbTransform,
			UpVectorVal);

		FVector2D LocalShoulderInnerRange = HandData.ShoulderInnerRange;
		LocalShoulderInnerRange.X = FMath::Abs(LocalShoulderInnerRange.X);
		LocalShoulderInnerRange.Y = FMath::Abs(LocalShoulderInnerRange.Y);

		if (LocalShoulderInnerRange.X > 0 && LocalShoulderInnerRange.Y > 0)
		{
			ForArmInputRot.Roll = FMath::ClampAngle(ForArmInputRot.Roll, LocalShoulderInnerRange.X, -LocalShoulderInnerRange.Y);

			if (ForArmInputRot.Roll > 0)
			{
				ForArmInputRot.Roll = ForArmInputRot.Roll - LocalShoulderInnerRange.X;
			}
			else
			{
				ForArmInputRot.Roll = ForArmInputRot.Roll + LocalShoulderInnerRange.Y;
			}
		}

		FVector2D ShoulderOuterClamps = HandData.ShoulderOuterRange;
		ShoulderOuterClamps.X = FMath::Clamp(FMath::Abs(ShoulderOuterClamps.X), 0.01f, 179.9f);
		ShoulderOuterClamps.Y = FMath::Clamp(FMath::Abs(ShoulderOuterClamps.Y), 0.01f, 179.9f);
		ForArmInputRot.Roll = FMath::ClampAngle(ForArmInputRot.Roll, -ShoulderOuterClamps.X, ShoulderOuterClamps.Y);
		if (ForArmInputRot.Roll != -ShoulderOuterClamps.X && ForArmInputRot.Roll != ShoulderOuterClamps.Y)
		{
			LastShoulderAngle = ForArmInputRot.Roll;
		}

		{

			BS_UpperLimbTransform = AimHelper::SetArmYaw(
				HandData.bIsInvertUpperTwist,
				HandData.bIsRightHand,
				LastShoulderAngle * ArmAngle,
				SkeletalMeshComponent->GetComponentToWorld(),
				Output.Pose.GetComponentSpaceTransform(CachedUpperLimbIndex),
				Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex),
				BS_UpperLimbTransform,
				BS_LowerLimbTransform);

			OutArmSolverWorkArea.ResultShoulder.Transform = BS_UpperLimbTransform;
			OutArmSolverWorkArea.OrigShoulder.Transform = OrigUpperLimbCSTransform;

		}

		OutArmSolverWorkArea.ResultElbow.Transform = BS_LowerLimbTransform;
		OutArmSolverWorkArea.OrigElbow.Transform = OrigLowerLimbCSTransform;
	}

}

FAnimNode_CustomAimSolver::FAnimNode_CustomAimSolver()
{
	LerpedLookatLocation = FVector::ZeroVector;
	FRichCurve* Look_Bending_CurveData = LookBendingCurve.GetRichCurve();
	Look_Bending_CurveData->AddKey(0.f, 0.025f);
	Look_Bending_CurveData->AddKey(1.f, 1.f);

	FRichCurve* Look_Multiplier_CurveData = LookMultiplierCurve.GetRichCurve();
	Look_Multiplier_CurveData->AddKey(0.f, 1.0f);
	Look_Multiplier_CurveData->AddKey(1.f, 1.0f);


#if WITH_EDITOR
	if (DebugHandTransforms.IsEmpty())
	{
		ResizeDebugLocations(2);
	}
	DebugLookAtTransform.SetLocation(FVector(0, 200, 100));
#endif

}


void FAnimNode_CustomAimSolver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (Context.AnimInstanceProxy)
	{
		owning_skel = Context.AnimInstanceProxy->GetSkelMeshComponent();
		IgnoreActors.Add(Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner());

	}
}


void FAnimNode_CustomAimSolver::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const
{

#if WITH_EDITORONLY_DATA
	if (PreviewSkelMeshComp && PreviewSkelMeshComp->GetWorld())
	{
		for (int32 Index = 0; Index < TraceStartList.Num(); Index++)
		{
			DrawDebugLine(PreviewSkelMeshComp->GetWorld(), TraceStartList[Index], TraceEndList[Index], FColor::Red, false, 0.1f);
		}
	}
#endif

}

void FAnimNode_CustomAimSolver::UpdateInternal(const FAnimationUpdateContext& Context)
{
	TraceStartList.Empty();
	TraceEndList.Empty();

	const float MinValue = 0.25f;
	const float MaxValue = 100.0f;
	InterpolationSpeed = FMath::Clamp(InterpolationSpeed, MinValue, MaxValue);

	CachedDeltaSeconds = Context.GetDeltaTime();

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const bool bIsTurnOnSolver = (bIsAdaptiveTerrainTail) ? bIsEnableSolver && AimHitResult.bBlockingHit : bIsEnableSolver;

	const float AlphaVal = (bIsTurnOnSolver) ? 1.0f : 0.f;

	const float Factor = (1.0f - FMath::Exp(-SmoothFactor * CachedDeltaSeconds));
	HeadActualAlpha = FMath::FInterpTo(HeadActualAlpha, AlphaVal, Factor, InterpolationSpeed);


	ComponentScale = ComponentToWorld.GetScale3D().Z;
	FVector TTS_Ref_Pos = (DebugLookAtTransform * ComponentToWorld).GetLocation();
	FVector TTS_Ref_Down = TTS_Ref_Pos;

	if (bIsAdaptiveTerrainTail)
	{
		ApplyLineTrace(
			Context,
			TTS_Ref_Pos + FVector(0, 0, TraceUpHeight) * ComponentScale,
			TTS_Ref_Down + FVector(0, 0, -TraceDownHeight) * ComponentScale,
			AimHitResult, FLinearColor::Blue, true);
	}

	FTransform ZeroedHit = DebugLookAtTransform;
	ZeroedHit.SetLocation(FVector(DebugLookAtTransform.GetLocation().X, DebugLookAtTransform.GetLocation().Y, 0));
	ZeroedHit.SetLocation((ZeroedHit * ComponentToWorld).GetLocation());
	HitResultHeight = (TTS_Ref_Pos - ZeroedHit.GetLocation()).Size();
}


void FAnimNode_CustomAimSolver::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_CustomAimSolver_EvalSKelControl);

	check(OutBoneTransforms.Num() == 0);

	FABRIK_BodySystem(Output.Pose, Output, OutBoneTransforms);
	
	for (int32 i = 0; i < HeadTransforms.Num(); i++)
	{
		OutBoneTransforms.Add(HeadTransforms[i]);
	}

	for (int32 i = 0; i < ArmSolverWorkArea.Num(); i++)
	{
		FArmSolverWorkArea& WorkData = ArmSolverWorkArea[i];

		if (!WorkData.bIsInitialized)
		{
			WorkData.CurrentClavicle = WorkData.ResultClavicle.Transform;
			WorkData.CurrentShoulder = WorkData.ResultShoulder.Transform;
			WorkData.CurrentElbow = WorkData.ResultElbow.Transform;
			WorkData.CurrentHand = WorkData.ResultHand.Transform;
			WorkData.bIsInitialized = true;
		}

		const float Factor = (1.0f - FMath::Exp(-SmoothFactor * CachedDeltaSeconds));

		if (bIsEnableHandInterpolation)
		{
			WorkData.CurrentHand.SetLocation(FMath::VInterpTo(WorkData.CurrentHand.GetLocation(),
				WorkData.ResultHand.Transform.GetLocation(), Factor, HandInterpolationSpeed));
			WorkData.CurrentHand.SetRotation(FMath::QInterpTo(WorkData.CurrentHand.GetRotation(),
				WorkData.ResultHand.Transform.GetRotation(), Factor, HandInterpolationSpeed));

			if (WorkData.bHasClavicle)
			{
				WorkData.CurrentClavicle.SetLocation(FMath::VInterpTo(WorkData.CurrentClavicle.GetLocation(),
					WorkData.ResultClavicle.Transform.GetLocation(), Factor, HandInterpolationSpeed));
				WorkData.CurrentClavicle.SetRotation(FMath::QInterpTo(WorkData.CurrentClavicle.GetRotation(),
					WorkData.ResultClavicle.Transform.GetRotation(), Factor, HandInterpolationSpeed));
			}

			WorkData.CurrentShoulder.SetLocation(FMath::VInterpTo(WorkData.CurrentShoulder.GetLocation(),
				WorkData.ResultShoulder.Transform.GetLocation(), Factor, HandInterpolationSpeed));
			WorkData.CurrentShoulder.SetRotation(FMath::QInterpTo(WorkData.CurrentShoulder.GetRotation(),
				WorkData.ResultShoulder.Transform.GetRotation(), Factor, HandInterpolationSpeed));

			WorkData.CurrentElbow.SetLocation(FMath::VInterpTo(WorkData.CurrentElbow.GetLocation(),
				WorkData.ResultElbow.Transform.GetLocation(), Factor, HandInterpolationSpeed));
			WorkData.CurrentElbow.SetRotation(FMath::QInterpTo(WorkData.CurrentElbow.GetRotation(),
				WorkData.ResultElbow.Transform.GetRotation(), Factor, HandInterpolationSpeed));
		}
		else
		{
			WorkData.CurrentClavicle = WorkData.ResultClavicle.Transform;
			WorkData.CurrentShoulder = WorkData.ResultShoulder.Transform;
			WorkData.CurrentElbow = WorkData.ResultElbow.Transform;
			WorkData.CurrentHand = WorkData.ResultHand.Transform;
		}

		if (WorkData.bHasClavicle)
		{
			OutBoneTransforms.Add(FBoneTransform(WorkData.ResultClavicle.BoneIndex, WorkData.CurrentClavicle));
		}

		OutBoneTransforms.Add(FBoneTransform(WorkData.ResultShoulder.BoneIndex, WorkData.CurrentShoulder));
		OutBoneTransforms.Add(FBoneTransform(WorkData.ResultElbow.BoneIndex, WorkData.CurrentElbow));
		OutBoneTransforms.Add(FBoneTransform(WorkData.ResultHand.BoneIndex, WorkData.CurrentHand));
	}

	
	for (int32 i = 0; i < OutBoneTransforms.Num(); i++)
	{
		const FTransform& AnimBoneTransform = Output.Pose.GetComponentSpaceTransform(OutBoneTransforms[i].BoneIndex);

		OutBoneTransforms[i].Transform = UKismetMathLibrary::TLerp(
			AnimBoneTransform,
			OutBoneTransforms[i].Transform, HeadActualAlpha);
	}

}

bool FAnimNode_CustomAimSolver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{

	if (!bIsEnableSolver)
	{
		return false;
	}

	for (int32 i = 0; i < AimingHandLimbs.Num(); ++i)
	{
		const FCustomBone_ArmsData& ArmData = AimingHandLimbs[i];

		// @NOTE
		// dont must need to setting ClavicleBone
		// !ArmData.ClavicleBone.IsValidToEvaluate(RequiredBones) || 
		if (!ArmData.ShoulderBone.IsValidToEvaluate(RequiredBones) ||
			!ArmData.ElbowBone.IsValidToEvaluate(RequiredBones) || 
			!ArmData.HandBone.IsValidToEvaluate(RequiredBones))
		{
			return false;
		}
	}


	return (EndSplineBone.IsValidToEvaluate(RequiredBones) && 
		StartSplineBone.IsValidToEvaluate(RequiredBones) && 
		RequiredBones.IsValid() && 
		(RequiredBones.BoneIsChildOf(EndSplineBone.BoneIndex, StartSplineBone.BoneIndex)));
}

void FAnimNode_CustomAimSolver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{

	EndSplineBone.Initialize(RequiredBones);
	StartSplineBone.Initialize(RequiredBones);

	CombinedIndices.Empty();

	HandBoneArray.Empty();
	ElbowBoneArray.Empty();
	ShoulderBoneArray.Empty();
	ActualShoulderBoneArray.Empty();

	LastShoulderAngles.Empty();
	ElbowBoneTransformArray.Empty();
	HandDefaultTransformArray.Empty();

	HeadTransforms.Empty();

	ArmSolverWorkArea.Empty();

#if WITH_EDITOR
	if (DebugHandTransforms.Num() < AimingHandLimbs.Num())
	{
		ResizeDebugLocations(AimingHandLimbs.Num());
	}
#endif


	bIsArmsEnable = (!AimingHandLimbs.IsEmpty());

	for (int32 i = 0; i < AimingHandLimbs.Num(); ++i)
	{
		FCustomBone_ArmsData& ArmData = AimingHandLimbs[i];
		ArmData.ClavicleBone.Initialize(RequiredBones);
		ArmData.ShoulderBone.Initialize(RequiredBones);
		ArmData.ElbowBone.Initialize(RequiredBones);
		ArmData.HandBone.Initialize(RequiredBones);

		HandBoneArray.Add(ArmData.HandBone);
		ElbowBoneArray.Add(ArmData.ElbowBone);
		ShoulderBoneArray.Add(ArmData.ShoulderBone);
		ActualShoulderBoneArray.Add(ArmData.ShoulderBone);
	}

	// Gather all bone indices between root and tip.
	TArray<FCompactPoseBoneIndex> BoneIndices;

	{

		const FCompactPoseBoneIndex RootIndex = StartSplineBone.GetCompactPoseIndex(RequiredBones);
		FCompactPoseBoneIndex BoneIndex = EndSplineBone.GetCompactPoseIndex(RequiredBones);
		do
		{
			BoneIndices.Insert(BoneIndex, 0);
			BoneIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		} while (BoneIndex != RootIndex);
		BoneIndices.Insert(BoneIndex, 0);
	}

	const int32 NumHeadBones = BoneIndices.Num();
	HeadTransforms.SetNumUninitialized(NumHeadBones);
	for (int32 i = 0; i < NumHeadBones; ++i)
	{
		HeadTransforms[i] = FBoneTransform(BoneIndices[i], FTransform::Identity);
	}

	const int32 NumArms = AimingHandLimbs.Num();
	ElbowBoneTransformArray.SetNumUninitialized(NumArms);
	HandDefaultTransformArray.SetNumUninitialized(NumArms);
	LastShoulderAngles.SetNumZeroed(NumArms);

	ArmSolverWorkArea.SetNumUninitialized(NumArms);

	for (int32 i = 0; i < NumArms; ++i)
	{
		ArmSolverWorkArea[i].Initialize(RequiredBones, AimingHandLimbs[i]);
	}

	bIsDebugHandsInitialized = true;
}


void FAnimNode_CustomAimSolver::FABRIK_BodySystem(FCSPose<FCompactPose>& MeshBases, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{

	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const auto SK = Output.AnimInstanceProxy->GetSkelMeshComponent();

	RefConstantForwardTemp = bIsUseReferenceForwardAxis ? ReferenceConstantForwardAxis : ForwardDirectionVector;

	FTransform LookAtLocation_Temp = LookAtLocation;
	FTransform Body_LookTarget = LookAtLocation_Temp;
	FVector ArmAveragePosition = FVector::ZeroVector;

	if (bIsAggregateHandBody && bIsUseSeparateTargets)
	{
		for (int32 ArmIdx = 0; ArmIdx < ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num(); ArmIdx++)
		{
			FVector Arm_Position;

			if ((SK->GetWorld()->IsGameWorld() || bIsWorkOutsidePIE))
			{
				Arm_Position = ArmTargetLocationOverrides.ArmTargetLocationOverrides[ArmIdx].OverrideArmTransform.GetLocation();
			}
			else
			{
				if (DebugHandTransforms.IsValidIndex(ArmIdx))
				{
					Arm_Position = DebugHandTransforms[ArmIdx].GetLocation();
				}
			}
			ArmAveragePosition += Arm_Position;
		}

		if (ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num() > 0)
		{
			ArmAveragePosition = ArmAveragePosition / ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num();
		}
	}

	for (int32 ArmIdx = 0; ArmIdx < ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num(); ArmIdx++)
	{
		if ((SK->GetWorld()->IsGameWorld() || bIsWorkOutsidePIE))
		{
			if (DebugHandTransforms.Num() > ArmIdx)
			{
				DebugHandTransforms[ArmIdx] = ArmTargetLocationOverrides.ArmTargetLocationOverrides[ArmIdx].OverrideArmTransform;
			}
		}
	}

	if (!(SK->GetWorld()->IsGameWorld() || bIsWorkOutsidePIE))
	{
		LookAtLocation_Temp = DebugLookAtTransform;
		Body_LookTarget = DebugLookAtTransform;
	}

	if (!bIsEnableSolver)
	{
		return;
	}

	FAxis LookatAxis_Temp;
	LookatAxis_Temp.bInLocalSpace = false;
	LookatAxis_Temp.Axis = ForwardDirectionVector.GetSafeNormal();
	FAxis IntermediateAxis = IntermediateLookAtAxis.ToFAxis();


	if (bIsAdaptiveTerrainTail)
	{
		if (AimHitResult.bBlockingHit)
		{
			LookAtLocation_Temp.SetLocation(AimHitResult.ImpactPoint + FVector(0, 0, HitResultHeight));
		}
		else
		{
			FVector TTS_Ref_Pos = (DebugLookAtTransform * ComponentToWorld).GetLocation();
			LookAtLocation_Temp.SetLocation(TTS_Ref_Pos);
		}
	}
	HeadOrigTransform = MeshBases.GetComponentSpaceTransform(EndSplineBone.CachedCompactPoseIndex);

	if (LerpedLookatLocation.IsNearlyZero())
	{
		LerpedLookatLocation = LookAtLocation_Temp.GetLocation();
	}
	LerpedLookatLocation = AnimLocationLerp(LerpedLookatLocation, LookAtLocation_Temp.GetLocation(), CachedDeltaSeconds);

	LookAtLocation_Temp.SetLocation(LerpedLookatLocation);

	LookAtLocation_Temp.SetLocation(
		UQuadrupedIKLibrary::ClampRotateVector(
			LookAtLocation_Temp.GetLocation(),
			ComponentToWorld.TransformVector(ForwardDirectionVector),
			ComponentToWorld.TransformPosition(HeadOrigTransform.GetLocation()),
			VerticalRangeAngles.X, VerticalRangeAngles.Y,
			HorizontalRangeAngles.X, HorizontalRangeAngles.Y, bIsUseNaturalMethod));


	FTransform LookHeadTransform = LookAtLocation_Temp;
	FTransform LookTargetTransform = LookAtLocation_Temp;

	if (bIsAggregateHandBody && bIsUseSeparateTargets)
	{
		const FVector Head_WS = ComponentToWorld.TransformPosition(HeadOrigTransform.GetLocation());
		const FVector ClampedTargetDir = (LookAtLocation_Temp.GetLocation() - Head_WS).GetSafeNormal();
		const float HeadArmDist = (Head_WS - ArmAveragePosition).Size() * 2;
		LookAtLocation_Temp.SetLocation(Head_WS + ClampedTargetDir * HeadArmDist);
		LookTargetTransform.SetLocation((LookAtLocation_Temp.GetLocation() + ArmAveragePosition) / 2);
		LookAtLocation_Temp.SetLocation((LookAtLocation_Temp.GetLocation() + ArmAveragePosition) / 2);
	}

	FAnimationRuntime::ConvertBoneSpaceTransformToCS(
		ComponentToWorld,
		MeshBases,
		LookTargetTransform,
		EndSplineBone.GetCompactPoseIndex(RequiredBones),
		EBoneControlSpace::BCS_WorldSpace);

	FAnimationRuntime::ConvertBoneSpaceTransformToCS(
		ComponentToWorld,
		MeshBases,
		LookHeadTransform,
		EndSplineBone.GetCompactPoseIndex(RequiredBones),
		EBoneControlSpace::BCS_WorldSpace);

	bIsNsewPoleMethod = false;
	bIsUpArmTwistTechnique = (ArmTwistAxis == EArmTwistIKType::UpAxisTwist) ? true : false;


	AimHelper::Evaluate_ConsecutiveBoneRotations(
		Output,
		LookBendingCurve,
		StartSplineBone,
		EndSplineBone,
		LookAtRadius,
		InnerBodyClamp,
		LookAtLocation_Temp,
		LookatAxis_Temp,
		IntermediateAxis,
		bUseSpecificIntermediateAxis,
		LookAtRadius,
		VerticalDipTreshold,
		DownwardDipMultiplier,
		InvertedDipMultiplier,
		SideMoveMultiplier,
		SideDownMultiplier,
		false,
		FTransform::Identity,
		LookMultiplierCurve,
		UpRotClamp,
		bIsUseNaturalMethod,
		bIsHeadUseSeparateClamp,
		LookAtClamp,
		LookHeadTransform,
		bIsOverrideHeadRotation,
		HeadTransforms);

	if (bIsHeadAccurate)
	{
		const int32 LastIndex = (HeadTransforms.Num() - 1);
		if (bIsOverrideHeadRotation)
		{
			HeadTransforms[LastIndex].Transform.SetRotation(
				(ComponentToWorld.GetRotation().Inverse() *
					LookAtLocation_Temp.GetRotation()) * HeadOrigTransform.GetRotation());
		}
		else
		{
			HeadTransforms[LastIndex].Transform.SetRotation(
				LookAt_Processor(
					RequiredBones,
					Output,
					MeshBases,
					LookHeadTransform.GetLocation(),
					EndSplineBone.BoneName,
					LastIndex, LookAtClamp).Transform.GetRotation());
		}
	}

	TArray<FCompactPoseBoneIndex> HeadTransformPoses = TArray<FCompactPoseBoneIndex>();

	for (int32 Index = 0; Index < HeadTransforms.Num(); Index++)
	{
		HeadTransformPoses.Add(HeadTransforms[Index].BoneIndex);
	}

	if (!bIsIgnoreSeparateHandSolving)
	{
		if (!bIsReachInstead)
		{
			if (MainArmIndex > -1 && MainArmIndex < HandBoneArray.Num())
			{
				if (ActualShoulderBoneArray[MainArmIndex].IsValidToEvaluate() &&
					ElbowBoneArray[MainArmIndex].IsValidToEvaluate() &&
					HandBoneArray[MainArmIndex].IsValidToEvaluate())
				{
					FCompactPoseBoneIndex ConnectorIndex = FCompactPoseBoneIndex(0);
					FTransform CommonSpineModifiedTransform = FTransform::Identity;

					for (int32 Index = 0; Index < HeadTransforms.Num(); Index++)
					{
						const FCompactPoseBoneIndex& FirstBone = MeshBases.GetPose().GetParentBoneIndex(ActualShoulderBoneArray[MainArmIndex].CachedCompactPoseIndex);
						const FCompactPoseBoneIndex& SecondBone = MeshBases.GetPose().GetParentBoneIndex(FirstBone);

						if (HeadTransforms[Index].BoneIndex == FirstBone)
						{
							ConnectorIndex = HeadTransforms[Index].BoneIndex;
							CommonSpineModifiedTransform = HeadTransforms[Index].Transform;
						}

						if (HeadTransforms[Index].BoneIndex == SecondBone)
						{
							ConnectorIndex = HeadTransforms[Index].BoneIndex;
							CommonSpineModifiedTransform = HeadTransforms[Index].Transform;
						}
					}

					if (ConnectorIndex.GetInt() > 0)
					{
						const FVector Arm_LookTarget = LookAtLocation_Temp.GetLocation();
						FTransform  Common_Spine_Transform = MeshBases.GetComponentSpaceTransform(ConnectorIndex);
						FVector Target_CS_Position = ComponentToWorld.InverseTransformPosition(Arm_LookTarget);

						Target_CS_Position += AimingHandLimbs[MainArmIndex].ArmAimingOffset;
						const FTransform Inv_Common_Spine = Common_Spine_Transform.Inverse() * CommonSpineModifiedTransform;
						FTransform Hand_Transform_Default = MeshBases.GetComponentSpaceTransform(HandBoneArray[MainArmIndex].CachedCompactPoseIndex);

						const FTransform& Shoulder_Transform_Default = MeshBases.GetComponentSpaceTransform(ActualShoulderBoneArray[MainArmIndex].CachedCompactPoseIndex);

						const FTransform& Elbow_Transform_Default = MeshBases.GetComponentSpaceTransform(ElbowBoneArray[MainArmIndex].CachedCompactPoseIndex);

						FTransform Hand_Transform = MeshBases.GetComponentSpaceTransform(
							HandBoneArray[MainArmIndex].CachedCompactPoseIndex) * Inv_Common_Spine;

						const FTransform Shoulder_Transform = MeshBases.GetComponentSpaceTransform(
							ActualShoulderBoneArray[MainArmIndex].CachedCompactPoseIndex) * Inv_Common_Spine;

						FTransform Shoulder_Offseted_Transform = Shoulder_Transform_Default;
						Shoulder_Offseted_Transform.SetLocation(Shoulder_Transform.GetLocation());

						const float Individual_Leg_Clamp = LimbsClamp;

						FTransform Rotated_Shoulder = AimHelper::LookAt_Processor_Helper(
							Shoulder_Offseted_Transform,
							Common_Spine_Transform.GetLocation(),
							Target_CS_Position,
							LookatAxis_Temp,
							Individual_Leg_Clamp,
							FRotator::ZeroRotator,
							bIsUseNaturalMethod,
							1.0f, 1.0f);


						const FTransform Inv_Shoulder_Value = Shoulder_Transform_Default.Inverse() * Rotated_Shoulder;
						FTransform Shoulder_Transform_Output = Shoulder_Transform_Default * Inv_Shoulder_Value;
						FTransform Elbow_Transform_Output = Elbow_Transform_Default * Inv_Shoulder_Value;
						FTransform Hand_Transform_Output = Hand_Transform_Default * Inv_Shoulder_Value;

						const FVector Arm_Vector = (Hand_Transform_Output.GetLocation() - Shoulder_Transform_Output.GetLocation());
						const float Arm_Length = Arm_Vector.Size();
						float Target_Arm_Length = (CommonSpineModifiedTransform.GetLocation() - Target_CS_Position).Size();

						Target_Arm_Length = FMath::Clamp(Target_Arm_Length, 1, Arm_Length);
						Hand_Transform.SetLocation(Shoulder_Transform_Output.GetLocation() + Arm_Vector.GetSafeNormal() * Target_Arm_Length);

						Common_Spine_Transform.SetLocation(Target_CS_Position);

						if (!bIsIgnoreElbowModification)
						{
							FTransform Main_Relative_Transform = (MainHandDefaultTransform.Inverse() * MainHandNewTransform);
							FTransform Offseted_Hand_Transform = Hand_Transform_Default * Main_Relative_Transform;

							if (MainArmIndex < 0)
							{
								Main_Relative_Transform = FTransform::Identity;
								Offseted_Hand_Transform = Hand_Transform;
							}

							FTransform Hand_Spine_Relation = Hand_Transform_Default * Inv_Common_Spine;

							if (bIsReachInstead)
							{
								Offseted_Hand_Transform.SetLocation(Target_CS_Position);

								Offseted_Hand_Transform.SetLocation(UQuadrupedIKLibrary::ClampRotateVector(
									ComponentToWorld.TransformPosition(Offseted_Hand_Transform.GetLocation()),
									ComponentToWorld.TransformVector(ForwardDirectionVector),
									ComponentToWorld.TransformPosition(Shoulder_Transform_Output.GetLocation()),
									AimingHandLimbs[MainArmIndex].MaxArm_VAngle.X,
									AimingHandLimbs[MainArmIndex].MaxArm_VAngle.Y,
									AimingHandLimbs[MainArmIndex].MaxArm_HAngle.X,
									AimingHandLimbs[MainArmIndex].MaxArm_HAngle.Y,
									bIsUseNaturalMethod));


								Offseted_Hand_Transform.SetLocation(
									ComponentToWorld.InverseTransformPosition(Offseted_Hand_Transform.GetLocation()));

								Shoulder_Transform_Output = Shoulder_Transform_Default * Inv_Common_Spine;
								Elbow_Transform_Output = Elbow_Transform_Default * Inv_Common_Spine;
								Hand_Transform_Output = Hand_Spine_Relation;

								FVector Point_Thigh_Dir = (Offseted_Hand_Transform.GetLocation() - Shoulder_Transform_Output.GetLocation());
								const float Point_Thigh_Size = Point_Thigh_Dir.Size();
								const float Effector_Thigh_Size = (Hand_Transform_Output.GetLocation() - Shoulder_Transform_Output.GetLocation()).Size();
								Point_Thigh_Dir.Normalize();

								Offseted_Hand_Transform.SetLocation(
									Shoulder_Transform_Output.GetLocation() + Point_Thigh_Dir *
									FMath::Clamp(
										Point_Thigh_Size,
										Effector_Thigh_Size * FMath::Abs(AimingHandLimbs[MainArmIndex].MinimumExtension),
										Effector_Thigh_Size * FMath::Abs(AimingHandLimbs[MainArmIndex].MaximumExtension)));
							}
							else
							{
								Offseted_Hand_Transform.SetLocation(Shoulder_Transform_Output.GetLocation() + (Arm_Vector.GetSafeNormal() *
									FMath::Clamp(
										Arm_Length,
										Arm_Length * FMath::Abs(AimingHandLimbs[MainArmIndex].MinimumExtension),
										Arm_Length * FMath::Abs(AimingHandLimbs[MainArmIndex].MaximumExtension))));
							}

							if (ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num() > MainArmIndex)
							{
								Offseted_Hand_Transform = UKismetMathLibrary::TLerp(Hand_Spine_Relation,
									Offseted_Hand_Transform,
									ArmTargetLocationOverrides.ArmTargetLocationOverrides[MainArmIndex].ArmAlpha);

								if (bIsOverrideHandRotation)
								{
									const FQuat Offseted_Rotation_Value = ComponentToWorld.GetRotation().Inverse() *
										DebugHandTransforms[MainArmIndex].GetRotation();
									Offseted_Hand_Transform.SetRotation(Offseted_Rotation_Value);
								}
							}

							float MainArmLSA = 0;
							AimHelper::Evaluate_TwoBoneIK_Direct_Modified(
								Output,
								SK,
								HandBoneArray[MainArmIndex],
								ElbowBoneArray[MainArmIndex],
								ActualShoulderBoneArray[MainArmIndex],
								Hand_Transform,
								Shoulder_Transform_Output,
								Elbow_Transform_Output,
								Hand_Transform_Output,
								FVector::ZeroVector,
								FVector::ZeroVector,
								Inv_Common_Spine,
								Common_Spine_Transform,
								LimbRotationOffset,
								AimingHandLimbs[MainArmIndex],
								Individual_Leg_Clamp,
								FTransform::Identity,
								AimingHandLimbs[MainArmIndex].ElbowPoleOffset,
								bIsOverrideHandRotation,
								Elbow_Transform_Default,
								ForwardDirectionVector,
								RefConstantForwardTemp,
								MainArmLSA,
								bIsNsewPoleMethod,
								bIsUpArmTwistTechnique,
								CharacterDirectionVectorCS,
								bIsUseSeparateTargets,
								bIsReachInstead,
								ArmSolverWorkArea[MainArmIndex]);

							const int32 PVIdx = PoleVectorIndex;
							MainHandNewTransform = ArmSolverWorkArea[MainArmIndex].ResultElbow.Transform;
							//MainHandDefaultTransform = Hand_Transform_Default;
							MainHandDefaultTransform = Elbow_Transform_Default;
						}
						else
						{

							ArmSolverWorkArea[MainArmIndex].ResultShoulder.Transform = Rotated_Shoulder;
						}
					}
				}
			}
		}

		for (int32 LIndex = 0; LIndex < AimingHandLimbs.Num(); LIndex++)
		{
			ActualShoulderBoneArray[LIndex] = ShoulderBoneArray[LIndex];
			const FBoneReference Original_BoneRef = ActualShoulderBoneArray[LIndex];
			FCompactPoseBoneIndex CurParent = ActualShoulderBoneArray[LIndex].CachedCompactPoseIndex;
			FCompactPoseBoneIndex LastSavedBone = CurParent;

			bool Found_Limbs = false;
			constexpr int32 MAX = 5;

			for (int32 Index = 0; Index < MAX; Index++)
			{
				if (ActualShoulderBoneArray[LIndex].BoneIndex > 0)
				{
					LastSavedBone = ActualShoulderBoneArray[LIndex].GetCompactPoseIndex(RequiredBones);

					if (LastSavedBone.GetInt() > INDEX_NONE)
					{
						FCompactPoseBoneIndex Ref_Parent = MeshBases.GetPose().GetParentBoneIndex(LastSavedBone);
						if (HeadTransformPoses.Contains(Ref_Parent))
						{
							break;
						}

						Found_Limbs = true;

						if (!bIsReachInstead && !bIsUseSeparateTargets)
						{
							if (!ActualShoulderBoneArray[LIndex].IsValidToEvaluate())
							{
								ActualShoulderBoneArray[LIndex] = FBoneReference(SK->GetBoneName(CurParent.GetInt()));
								ActualShoulderBoneArray[LIndex].Initialize(RequiredBones);

								UE_LOG(LogTemp, Error, TEXT("[%s] : ActualShoulderBoneArray[LIndex] not initialized"), *FString(__FUNCTION__));
							}
						}

						if (LIndex < ActualShoulderBoneArray.Num() && LIndex > -1)
						{
							if (ActualShoulderBoneArray[LIndex].GetCompactPoseIndex(RequiredBones).GetInt() > -1)
							{
								CurParent = MeshBases.GetPose().GetParentBoneIndex(
									ActualShoulderBoneArray[LIndex].GetCompactPoseIndex(RequiredBones));
							}
						}
					}

				}
				else
				{
					break;
				}
			}


		}

		for (int32 LIndex = 0; LIndex < AimingHandLimbs.Num(); LIndex++)
		{
			if ((ActualShoulderBoneArray[LIndex].IsValidToEvaluate() && ElbowBoneArray[LIndex].IsValidToEvaluate() && HandBoneArray[LIndex].IsValidToEvaluate()) &&
				(LIndex != MainArmIndex || MainArmIndex < 0 || bIsReachInstead))
			{

				if (ElbowBoneTransformArray.Num() > LIndex)
				{
					ElbowBoneTransformArray[LIndex] = MeshBases.GetComponentSpaceTransform(ElbowBoneArray[LIndex].CachedCompactPoseIndex);
				}

				if (HandDefaultTransformArray.Num() > LIndex)
				{
					HandDefaultTransformArray[LIndex] = MeshBases.GetComponentSpaceTransform(HandBoneArray[LIndex].CachedCompactPoseIndex);
				}

				FCompactPoseBoneIndex Connector_Index = FCompactPoseBoneIndex(0);
				FTransform Common_Spine_Modified_Transform = FTransform::Identity;
				FVector Arm_LookTarget = LookAtLocation_Temp.GetLocation();

				if (bIsUseSeparateTargets)
				{
					if (ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num() > LIndex)
					{
						if (DebugHandTransforms.Num() > LIndex)
						{
							Arm_LookTarget = DebugHandTransforms[LIndex].GetLocation();
						}
					}
				}

				for (int32 BodyIndex = 0; BodyIndex < HeadTransforms.Num(); BodyIndex++)
				{
					const FCompactPoseBoneIndex& FirstBoneIdx = MeshBases.GetPose().GetParentBoneIndex(ActualShoulderBoneArray[LIndex].CachedCompactPoseIndex);
					const FCompactPoseBoneIndex& SecondBoneIdx = MeshBases.GetPose().GetParentBoneIndex(FirstBoneIdx);

					if (HeadTransforms[BodyIndex].BoneIndex == FirstBoneIdx)
					{
						Connector_Index = HeadTransforms[BodyIndex].BoneIndex;
						Common_Spine_Modified_Transform = HeadTransforms[BodyIndex].Transform;
					}


					if (HeadTransforms[BodyIndex].BoneIndex == SecondBoneIdx)
					{
						Connector_Index = HeadTransforms[BodyIndex].BoneIndex;
						Common_Spine_Modified_Transform = HeadTransforms[BodyIndex].Transform;
					}
				}

				if (!(Connector_Index.GetInt() > 0))
				{
					continue;
				}

				FTransform Common_Spine_Transform = MeshBases.GetComponentSpaceTransform(Connector_Index);
				FVector Target_CS_Position = ComponentToWorld.InverseTransformPosition(Arm_LookTarget);

				if (AimingHandLimbs[LIndex].bIsOverrideLimits)
				{
					Target_CS_Position = (UQuadrupedIKLibrary::ClampRotateVector(
						Arm_LookTarget,
						ComponentToWorld.TransformVector(ForwardDirectionVector),
						ComponentToWorld.TransformPosition(HeadOrigTransform.GetLocation()),
						AimingHandLimbs[LIndex].MaxArm_VAngle.X,
						AimingHandLimbs[LIndex].MaxArm_VAngle.Y,
						AimingHandLimbs[LIndex].MaxArm_HAngle.X,
						AimingHandLimbs[LIndex].MaxArm_HAngle.Y,
						bIsUseNaturalMethod));
				}
				else
				{
					Target_CS_Position = (UQuadrupedIKLibrary::ClampRotateVector(
						Arm_LookTarget,
						ComponentToWorld.TransformVector(ForwardDirectionVector),
						ComponentToWorld.TransformPosition(HeadOrigTransform.GetLocation()),
						VerticalRangeAngles.X,
						VerticalRangeAngles.Y,
						HorizontalRangeAngles.X,
						HorizontalRangeAngles.Y,
						bIsUseNaturalMethod));
				}

				Target_CS_Position = ComponentToWorld.InverseTransformPosition(Target_CS_Position);
				Target_CS_Position += AimingHandLimbs[LIndex].ArmAimingOffset;

				const FTransform Inv_Common_Spine = Common_Spine_Transform.Inverse() * Common_Spine_Modified_Transform;
				const FTransform Hand_Transform_Default = MeshBases.GetComponentSpaceTransform(
					HandBoneArray[LIndex].CachedCompactPoseIndex);

				const FTransform Shoulder_Transform_Default = MeshBases.GetComponentSpaceTransform(
					ActualShoulderBoneArray[LIndex].CachedCompactPoseIndex);

				const FTransform Knee_Transform_Default = MeshBases.GetComponentSpaceTransform(
					ElbowBoneArray[LIndex].CachedCompactPoseIndex);

				FTransform Hand_Transform = MeshBases.GetComponentSpaceTransform(
					HandBoneArray[LIndex].CachedCompactPoseIndex) * Inv_Common_Spine;

				FTransform Shoulder_Transform = MeshBases.GetComponentSpaceTransform(
					ActualShoulderBoneArray[LIndex].CachedCompactPoseIndex) * Inv_Common_Spine;

				FTransform Shoulder_Offseted_Transform = Shoulder_Transform_Default;
				Shoulder_Offseted_Transform.SetLocation(Shoulder_Transform.GetLocation());
				const float Individual_Leg_Clamp = LimbsClamp;
				const FVector X_Diff = Shoulder_Transform.GetLocation() - Common_Spine_Transform.GetLocation();

				FTransform Rotated_Shoulder = AimHelper::LookAt_Processor_Helper(
					Shoulder_Offseted_Transform,
					Common_Spine_Transform.GetLocation(),
					Target_CS_Position,
					LookatAxis_Temp,
					Individual_Leg_Clamp,
					FRotator::ZeroRotator,
					bIsUseNaturalMethod, 1.0f, 1.0f);

				const FTransform Inv_Shoulder_Value = Shoulder_Transform_Default.Inverse() * Rotated_Shoulder;
				FTransform Shoulder_Transform_Output = Shoulder_Transform_Default * Inv_Shoulder_Value;
				FTransform Knee_Transform_Output = Knee_Transform_Default * Inv_Shoulder_Value;
				FTransform Hand_Transform_Output = Hand_Transform_Default * Inv_Shoulder_Value;

				const FVector Arm_Vector = (Hand_Transform_Output.GetLocation() - Shoulder_Transform_Output.GetLocation());
				const float Arm_Length = Arm_Vector.Size();
				float Target_Arm_Length = (Common_Spine_Modified_Transform.GetLocation() - Target_CS_Position).Size();

				Target_Arm_Length = FMath::Clamp(Target_Arm_Length, 1, Arm_Length);
				Hand_Transform.SetLocation(Shoulder_Transform_Output.GetLocation() + Arm_Vector.GetSafeNormal() * Target_Arm_Length);

				Common_Spine_Transform.SetLocation(Target_CS_Position);

				if (!bIsIgnoreElbowModification)
				{
					FTransform Main_Relative_Transform = (MainHandDefaultTransform.Inverse() * MainHandNewTransform);
					FTransform Offseted_Hand_Transform = Hand_Transform_Default * Main_Relative_Transform;

					if (MainArmIndex < 0 || bIsReachInstead)
					{
						Main_Relative_Transform = FTransform::Identity;
						Offseted_Hand_Transform = Hand_Transform;
					}

					FTransform Hand_Spine_Relation = Hand_Transform_Default * Inv_Common_Spine;
					FTransform Elbow_Pole_Transform = FTransform::Identity;

					if (bIsReachInstead)
					{
						Offseted_Hand_Transform.SetLocation(Target_CS_Position);
						Shoulder_Transform_Output = Shoulder_Transform_Default * Inv_Common_Spine;
						Knee_Transform_Output = Knee_Transform_Default * Inv_Common_Spine;
						Hand_Transform_Output = Hand_Spine_Relation;

						FVector Point_Thigh_Dir = (Offseted_Hand_Transform.GetLocation() - Shoulder_Transform_Output.GetLocation());
						const float Point_Thigh_Size = Point_Thigh_Dir.Size();
						const float Effector_Thigh_Size = (Hand_Transform_Output.GetLocation() - Shoulder_Transform_Output.GetLocation()).Size();
						Point_Thigh_Dir.Normalize();

						const float ClampVal = FMath::Clamp(
							Point_Thigh_Size,
							Effector_Thigh_Size * FMath::Abs(AimingHandLimbs[LIndex].MinimumExtension),
							Effector_Thigh_Size * FMath::Abs(AimingHandLimbs[LIndex].MaximumExtension));

						Offseted_Hand_Transform.SetLocation(Shoulder_Transform_Output.GetLocation() + Point_Thigh_Dir * ClampVal);

						Elbow_Pole_Transform.SetLocation(AimingHandLimbs[LIndex].ElbowPoleOffset);
						Elbow_Pole_Transform = Elbow_Pole_Transform * Inv_Shoulder_Value;
					}
					else
					{
						const float ClampVal = FMath::Clamp(
							Arm_Length,
							Arm_Length * FMath::Abs(AimingHandLimbs[LIndex].MinimumExtension),
							Arm_Length * FMath::Abs(AimingHandLimbs[LIndex].MaximumExtension));

						const FVector Arm_Hand_Vector = (Offseted_Hand_Transform.GetLocation() - Shoulder_Transform_Output.GetLocation()).GetSafeNormal();
						Offseted_Hand_Transform.SetLocation(Shoulder_Transform_Output.GetLocation() + (Arm_Hand_Vector * ClampVal));
					}


					if (ArmTargetLocationOverrides.ArmTargetLocationOverrides.Num() > LIndex)
					{
						Offseted_Hand_Transform = UKismetMathLibrary::TLerp(Hand_Spine_Relation, Offseted_Hand_Transform,
							ArmTargetLocationOverrides.ArmTargetLocationOverrides[LIndex].ArmAlpha);

						if (bIsOverrideHandRotation)
						{
							const FQuat Offseted_Rotation_Value = ComponentToWorld.GetRotation().Inverse() * DebugHandTransforms[LIndex].GetRotation();
							Offseted_Hand_Transform.SetRotation(Offseted_Rotation_Value);
						}
					}
					else
					{
						if (bIsOverrideHandRotation)
						{
							const FQuat Offseted_Rotation_Value = ComponentToWorld.GetRotation().Inverse() * LookAtLocation.GetRotation();
							Offseted_Hand_Transform.SetRotation(Offseted_Rotation_Value);
						}
					}

					AimHelper::Evaluate_TwoBoneIK_Direct_Modified(
						Output,
						SK,
						HandBoneArray[LIndex],
						ElbowBoneArray[LIndex],
						ActualShoulderBoneArray[LIndex],
						Offseted_Hand_Transform,
						Shoulder_Transform_Output,
						Knee_Transform_Output,
						Hand_Transform_Output,
						FVector::ZeroVector,
						FVector::ZeroVector,
						Inv_Common_Spine,
						Common_Spine_Transform,
						LimbRotationOffset,
						AimingHandLimbs[LIndex],
						Individual_Leg_Clamp,
						Main_Relative_Transform,
						Elbow_Pole_Transform.GetLocation(),
						bIsOverrideHandRotation,
						Knee_Transform_Default,
						ForwardDirectionVector,
						RefConstantForwardTemp,
						LastShoulderAngles[LIndex],
						bIsNsewPoleMethod,
						bIsUpArmTwistTechnique,
						CharacterDirectionVectorCS,
						bIsUseSeparateTargets,
						bIsReachInstead,
						ArmSolverWorkArea[LIndex]);
				}
				else
				{
					ArmSolverWorkArea[LIndex].ResultShoulder.Transform = Rotated_Shoulder;
				}


			}
		}
	}
}


FVector FAnimNode_CustomAimSolver::AnimLocationLerp(const FVector& InStartPosition, const FVector& InEndPosition, const float InDeltaSeconds) const
{
	if (!bIsEnableInterpolation)
	{
		return InEndPosition;
	}

	FVector Local_StartPosition = InStartPosition;
	FVector Local_EndPosition = InEndPosition;

	if (bIsAdaptiveTerrainTail)
	{
		Local_StartPosition.X = Local_EndPosition.X;
		Local_StartPosition.Y = Local_EndPosition.Y;
	}

	const FVector Diff = (Local_StartPosition - Local_EndPosition) / FMath::Clamp(100 - (InterpolationSpeed * InDeltaSeconds * 12), 1, 100);
	FVector Output = FVector::ZeroVector;

	if (InterpLocationType == EIKInterpLocationType::DivisiveLocation)
	{
		Output = (Local_StartPosition - Diff);
	}
	else
	{
		Output = FMath::VInterpTo(Local_StartPosition, Local_EndPosition, InDeltaSeconds, InterpolationSpeed);
	}
	return Output;
}

const FBoneTransform FAnimNode_CustomAimSolver::LookAt_Processor(
	const FBoneContainer& RequiredBones, 
	FComponentSpacePoseContext& Output,
	FCSPose<FCompactPose>& MeshBases, 
	const FVector& OffsetVector, 
	const FName& BoneName, 
	const int32 InIndex, 
	const float LookAtClampParam)
{
	const FCompactPoseBoneIndex ModifyBoneIndex = EndSplineBone.GetCompactPoseIndex(RequiredBones);
	FTransform ComponentBoneTransform = MeshBases.GetComponentSpaceTransform(ModifyBoneIndex);

	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();

	if (!HeadTransforms.IsEmpty())
	{
		ComponentBoneTransform.SetLocation(HeadTransforms[InIndex].Transform.GetLocation());
	}

	const FVector TargetLocationInComponentSpace = OffsetVector;
	FAxis LookatAxisTemp;
	LookatAxisTemp.bInLocalSpace = false;
	LookatAxisTemp.Axis = ForwardDirectionVector.GetSafeNormal();

	const FVector LookAtVector = LookatAxisTemp.GetTransformedAxis(ComponentBoneTransform).GetSafeNormal();
	FVector TargetDir = (TargetLocationInComponentSpace - ComponentBoneTransform.GetLocation()).GetSafeNormal();
	const float AimClampInRadians = FMath::DegreesToRadians(FMath::Min(LookAtClampParam, 180.f));
	const float DiffAngle = FMath::Acos(FVector::DotProduct(LookAtVector, TargetDir));

	if (DiffAngle > AimClampInRadians)
	{
		check(DiffAngle > 0.f);
		FVector DeltaTarget = TargetDir - LookAtVector;
		DeltaTarget *= (AimClampInRadians / DiffAngle);
		TargetDir = LookAtVector + DeltaTarget;
		TargetDir.Normalize();
	}

	FQuat NormalizedDelta = FQuat::FindBetweenNormals(LookAtVector, TargetDir);
	if (!bIsUseNaturalMethod)
	{
		const FRotator Rot_Ref_01 = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, TargetDir);
		const FRotator Rot_Ref_02 = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, LookAtVector);
		NormalizedDelta = Rot_Ref_01.Quaternion() * Rot_Ref_02.Quaternion().Inverse();
	}

	FTransform RotationTransform_WS = ComponentBoneTransform;
	RotationTransform_WS.SetRotation(NormalizedDelta);

	FAnimationRuntime::ConvertCSTransformToBoneSpace(
		ComponentToWorld, MeshBases, 
		RotationTransform_WS, ModifyBoneIndex, EBoneControlSpace::BCS_WorldSpace);

	FAnimationRuntime::ConvertBoneSpaceTransformToCS(
		ComponentToWorld, MeshBases, 
		RotationTransform_WS, ModifyBoneIndex, EBoneControlSpace::BCS_WorldSpace);

	ComponentBoneTransform.SetRotation(RotationTransform_WS.GetRotation() * ComponentBoneTransform.GetRotation());

	return (FBoneTransform(ModifyBoneIndex, ComponentBoneTransform));
}



#if WITH_EDITOR
void FAnimNode_CustomAimSolver::ResizeDebugLocations(const int32 NewSize)
{
	if (NewSize == 0)
	{
		DebugHandTransforms.Reset();
	}
	else if (DebugHandTransforms.Num() != NewSize)
	{
		const int32 StartIndex = DebugHandTransforms.Num();
		DebugHandTransforms.SetNum(NewSize);

		int32 PairFinishCount = 1;

		for (int32 Index = StartIndex; Index < DebugHandTransforms.Num(); ++Index)
		{
			DebugHandTransforms[Index] = FTransform::Identity;

			const bool bIsEven = (Index % 2 == 0);

			if (bIsEven)
			{
				++PairFinishCount;
			}

			if (bIsEven)
			{
				DebugHandTransforms[Index].SetLocation(FVector(50.0f * PairFinishCount, 75.0f, 75.0f));
			}
			else
			{
				DebugHandTransforms[Index].SetLocation(FVector(-50.0f * PairFinishCount, 75.0f, 75.0f));
			}
		}
	}
}
#endif


void FAnimNode_CustomAimSolver::ApplyLineTrace(const FAnimationUpdateContext& Context, const FVector& StartPoint, const FVector& EndPoint,
	FHitResult& OutHitResult, const FLinearColor& DebugColor, const bool bIsDebugMode)
{

	const auto SK = Context.AnimInstanceProxy->GetSkelMeshComponent();

	UKismetSystemLibrary::LineTraceSingle(SK, StartPoint, EndPoint,
		TraceChannel, true, IgnoreActors,
		EDrawDebugTrace::None, OutHitResult, true, DebugColor);

	if (bIsDebugMode)
	{
		TraceStartList.Add(StartPoint);
		TraceEndList.Add(EndPoint);
	}
}


