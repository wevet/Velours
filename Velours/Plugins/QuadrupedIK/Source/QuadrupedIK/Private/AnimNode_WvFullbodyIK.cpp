// Copyright 2022 wevet works All Rights Reserved.


#include "AnimNode_WvFullbodyIK.h"
#include "PredictionAnimInstance.h"
#include "QuadrupedIK.h"

#include "Engine/Engine.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "SceneManagement.h"
#include "DrawDebugHelpers.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_WvFullbodyIK)


DECLARE_CYCLE_STAT(TEXT("FullbodyIK Eval"), STAT_FullbodyIK_Eval, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixInverse"), STAT_FullbodyIK_MatrixInverse, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixTranspose"), STAT_FullbodyIK_MatrixTranspose, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixMultiply"), STAT_FullbodyIK_MatrixMultiply, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixDet"), STAT_FullbodyIK_MatrixDet, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK Loop"), STAT_FullbodyIK_Loop, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK CulcEta"), STAT_FullbodyIK_CulcEta, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK CalcJacobian"), STAT_FullbodyIK_CalcJacobian, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK SolveSolver"), STAT_FullbodyIK_SolveSolver, STATGROUP_Anim);


static const int32 AXIS_COUNT = 3;

using namespace WvFBIK;



#pragma region Helper
static FORCEINLINE void GetSinCosD(float Degree, float& OutSin, float& OutCos)
{
	const float Rad = FMath::DegreesToRadians(Degree);
	FMath::SinCos(&OutSin, &OutCos, Rad);
}

static FORCEINLINE FMatrix RotX(float Roll)
{
	float s, c;
	GetSinCosD(Roll, s, c);
	return FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, c, -s, 0),
		FPlane(0, s, c, 0),
		FPlane(0, 0, 0, 1)
	);
}


static FORCEINLINE FMatrix RotY(float Pitch)
{
	float s, c;
	GetSinCosD(Pitch, s, c);
	return FMatrix(
		FPlane(c, 0, s, 0),
		FPlane(0, 1, 0, 0),
		FPlane(-s, 0, c, 0),
		FPlane(0, 0, 0, 1)
	);
}


static FORCEINLINE FMatrix RotZ(float Yaw)
{
	float s, c;
	GetSinCosD(Yaw, s, c);
	return FMatrix(
		FPlane(c, s, 0, 0),
		FPlane(-s, c, 0, 0),
		FPlane(0, 0, 1, 0),
		FPlane(0, 0, 0, 1)
	);
}


static FORCEINLINE FMatrix DiffX(float Roll)
{
	float s, c;
	GetSinCosD(Roll, s, c);
	return FMatrix(
		FPlane(0, 0, 0, 0),
		FPlane(0, -s, -c, 0),
		FPlane(0, c, -s, 0),
		FPlane(0, 0, 0, 0)
	);
}


static FORCEINLINE FMatrix DiffY(float Pitch)
{
	float s, c;
	GetSinCosD(Pitch, s, c);
	return FMatrix(
		FPlane(-s, 0, c, 0),
		FPlane(0, 0, 0, 0),
		FPlane(-c, 0, -s, 0),
		FPlane(0, 0, 0, 0)
	);
}


static FORCEINLINE FMatrix DiffZ(float Yaw)
{
	float s, c;
	GetSinCosD(Yaw, s, c);
	return FMatrix(
		FPlane(-s, c, 0, 0),
		FPlane(-c, -s, 0, 0),
		FPlane(0, 0, 0, 0),
		FPlane(0, 0, 0, 0)
	);
}


static FORCEINLINE float MatrixInverse3(float* DstMatrix, const float* SrcMatrix)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixInverse);


	// 要素をローカル変数に展開（レジスタ化を促進し、ポインタ計算を削減）
	const float m00 = SrcMatrix[0], m01 = SrcMatrix[1], m02 = SrcMatrix[2];
	const float m10 = SrcMatrix[3], m11 = SrcMatrix[4], m12 = SrcMatrix[5];
	const float m20 = SrcMatrix[6], m21 = SrcMatrix[7], m22 = SrcMatrix[8];

	// 余因子の計算を先に行い、行列式(Det)を算出
	const float c00 = m11 * m22 - m12 * m21;
	const float c01 = m02 * m21 - m01 * m22;
	const float c02 = m01 * m12 - m02 * m11;

	const float Det = m00 * c00 + m10 * c01 + m20 * c02;

	// ゼロ除算回避
	if (FMath::IsNearlyZero(Det))
	{
		return 0.0f;
	}

	// 除算を一度の逆数計算に集約（除算は乗算の数倍〜十数倍重いため）
	const float InvDet = 1.0f / Det;

	// 結果を書き込み
	DstMatrix[0] = c00 * InvDet;
	DstMatrix[1] = c01 * InvDet;
	DstMatrix[2] = c02 * InvDet;

	DstMatrix[3] = (m12 * m20 - m10 * m22) * InvDet;
	DstMatrix[4] = (m00 * m22 - m02 * m20) * InvDet;
	DstMatrix[5] = (m02 * m10 - m00 * m12) * InvDet;

	DstMatrix[6] = (m10 * m21 - m11 * m20) * InvDet;
	DstMatrix[7] = (m01 * m20 - m00 * m21) * InvDet;
	DstMatrix[8] = (m00 * m11 - m01 * m10) * InvDet;

	return Det;
}


static FORCEINLINE void MatrixTranspose(float* DstMatrix, const float* SrcMatrix, const int32 Row, const int32 Col)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixTranspose);

	for (int32 i = 0; i < Row; ++i)
	{
		for (int32 j = 0; j < Col; ++j)
		{
			DstMatrix[j * Row + i] = SrcMatrix[i * Col + j];
		}
	}
}


static FORCEINLINE void MatrixMultiply(
	float* DstMatrix, 
	const float* SrcMatrix1, 
	const int32 Row1, 
	const int32 Col1, 
	const float* SrcMatrix2, 
	const int32 Row2, 
	const int32 Col2)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixMultiply);

	check(Col1 == Row2);


	// 全体をゼロクリア（一括処理の方が速い）
	FMemory::Memzero(DstMatrix, sizeof(float) * Row1 * Col2);

	for (int32 i = 0; i < Row1; ++i)
	{
		for (int32 k = 0; k < Col1; ++k)
		{
			// SrcMatrix1[i * Col1 + k] は k ループの間一定なのでレジスタに保持
			const float V1 = SrcMatrix1[i * Col1 + k];
			const float* V2Ptr = &SrcMatrix2[k * Col2];
			float* OutPtr = &DstMatrix[i * Col2];

			for (int32 j = 0; j < Col2; ++j)
			{
				// 連続するメモリへの加算。コンパイラが SIMD 化しやすい。
				OutPtr[j] += V1 * V2Ptr[j];
			}
		}
	}
}


static FORCEINLINE float GetMappedRangeEaseInClamped(
	const float& InRangeMin,
	const float& InRangeMax,
	const float& OutRangeMin,
	const float& OutRangeMax,
	const float& Exp,
	const float& Value)
{
	float Pct = FMath::Clamp((Value - InRangeMin) / (InRangeMax - InRangeMin), 0.f, 1.f);
	return FMath::InterpEaseIn(OutRangeMin, OutRangeMax, Pct, Exp);
}

#pragma endregion



FAnimNode_WvFullbodyIK::FAnimNode_WvFullbodyIK() : Super(),
	Setting(nullptr),
	DebugShowCenterOfMassRadius(0.f),
	bDebugShowEffectiveCount(false)
{
}


void FAnimNode_WvFullbodyIK::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (!Setting)
	{
		return;
	}

	if (Context.AnimInstanceProxy)
	{
		owning_skel = Context.AnimInstanceProxy->GetSkelMeshComponent();
		PredictionAnimInstance = Cast<UPredictionAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	}

	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	const FReferenceSkeleton& RefSkel = BoneContainer.GetReferenceSkeleton();

	for (const FName& IkEndBoneName : IkEndBoneNames)
	{
		FName BoneName = IkEndBoneName;

		while (true)
		{
			int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);

			if (BoneIndex == INDEX_NONE || SolverInternals.Contains(BoneIndex))
			{
				break;
			}

			SolverInternals.Add(BoneIndex, FSolverInternal());
			BoneIndices.Add(BoneIndex);

			const int32 ParentBoneIndex = RefSkel.GetParentIndex(BoneIndex);
			FFullbodyIKSolver Solver = GetSolver(BoneName);

			FSolverInternal& SolverInternal = SolverInternals[BoneIndex];
			SolverInternal.BoneIndex = BoneIndex;
			SolverInternal.ParentBoneIndex = ParentBoneIndex;
			SolverInternal.BoneIndicesIndex = -1;
			SolverInternal.bTranslation = Solver.bTranslation;
			SolverInternal.bLimited = Solver.bLimited;
			SolverInternal.Mass = Solver.Mass;
			SolverInternal.X = Solver.X;
			SolverInternal.Y = Solver.Y;
			SolverInternal.Z = Solver.Z;

			if (ParentBoneIndex >= 0)
			{
				if (!SolverTree.Contains(ParentBoneIndex))
				{
					SolverTree.Add(ParentBoneIndex, TArray<int32>());
				}
				SolverTree[ParentBoneIndex].Add(BoneIndex);
				BoneName = RefSkel.GetBoneName(ParentBoneIndex);
			}
			else
			{
				break;
			}

		}
	}

	BoneIndices.Sort();
	BoneCount = BoneIndices.Num();
	BoneAxisCount = BoneCount * AXIS_COUNT;

	for (int32 Idx = 0; Idx < BoneCount; ++Idx)
	{
		const int32& BoneIndex = BoneIndices[Idx];
		SolverInternals[BoneIndex].BoneIndicesIndex = Idx;
	}


	// 1. 基本となる次元を定義
	const int32 DimN = BoneAxisCount; // 自由度 (ボーン数 * 3)
	const int32 DimA = AXIS_COUNT;    // エフェクタの次元 (3)
	const int32 DimNxA = DimN * DimA; // N x 3 行列
	const int32 DimAxN = DimA * DimN; // 3 x N 行列
	const int32 DimAxA = DimA * DimA; // 3 x 3 行列
	const int32 DimNxN = DimN * DimN; // N x N 行列

	// 2. 必要な全要素数を合算
	// ※計算ロジックで使用するすべてのバッファをここにリストアップ
	int32 TotalSize = 0;
	TotalSize += DimNxA; // J
	TotalSize += DimAxN; // Jt
	TotalSize += DimAxA; // JtJ
	TotalSize += DimAxA; // JtJi
	TotalSize += DimAxN; // Jp
	TotalSize += DimN;   // W0 (対角成分のみならDimN、フルならDimNxN)
	TotalSize += DimNxN; // Wi
	TotalSize += DimAxN; // JtWi
	TotalSize += DimAxA; // JtWiJ
	TotalSize += DimAxA; // JtWiJi
	TotalSize += DimAxN; // JtWiJiJt
	TotalSize += DimAxN; // Jwp
	TotalSize += DimN;   // Rt1
	TotalSize += DimN;   // Eta
	TotalSize += DimA;   // EtaJ
	TotalSize += DimN;   // EtaJJp
	TotalSize += DimN;   // Rt2

	// 3. 一括確保
	SolverWorkingBuffer.SetNumUninitialized(TotalSize);
	float* BasePtr = SolverWorkingBuffer.GetData();

	// 4. ポインタの切り分け（オフセット管理）
	int32 Offset = 0;
	auto Assign = [&](float*& OutPtr, int32 Size)
	{
		OutPtr = BasePtr + Offset;
		Offset += Size;
	};

	Assign(Matrices.J, DimNxA);
	Assign(Matrices.Jt, DimAxN);
	Assign(Matrices.JtJ, DimAxA);
	Assign(Matrices.JtJi, DimAxA);
	Assign(Matrices.Jp, DimAxN);
	Assign(Matrices.W0, DimN);
	Assign(Matrices.Wi, DimNxN);
	Assign(Matrices.JtWi, DimAxN);
	Assign(Matrices.JtWiJ, DimAxA);
	Assign(Matrices.JtWiJi, DimAxA);
	Assign(Matrices.JtWiJiJt, DimAxN);
	Assign(Matrices.Jwp, DimAxN);
	Assign(Matrices.Rt1, DimN);
	Assign(Matrices.Eta, DimN);
	Assign(Matrices.EtaJ, DimA);
	Assign(Matrices.EtaJJp, DimN);
	Assign(Matrices.Rt2, DimN);


	// 1. バッファ全体のゼロクリア
	FMemory::Memzero(SolverWorkingBuffer.GetData(), SolverWorkingBuffer.Num() * sizeof(float));

	// 2. 加重行列（W0）の準備
	// 各ボーンの剛性（Weight）をバッファの特定位置にコピー
	for (int32 Idx = 0; Idx < BoneCount; ++Idx)
	{
		int32 BoneIndex = BoneIndices[Idx];
		// Matrices.W0 は SolverWorkingBuffer 内の特定のオフセット
		Matrices.W0[Idx * AXIS_COUNT + 0] = SolverInternals[BoneIndex].X.Weight;
		Matrices.W0[Idx * AXIS_COUNT + 1] = SolverInternals[BoneIndex].Y.Weight;
		Matrices.W0[Idx * AXIS_COUNT + 2] = SolverInternals[BoneIndex].Z.Weight;
	}



}


void FAnimNode_WvFullbodyIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Context, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_Eval);
	check(OutBoneTransforms.Num() == 0);


	if (!IsValid(Setting) || !IsValid(PredictionAnimInstance))
	{
		return;
	}

	if (EffectorContainer.Effectors.IsEmpty())
	{
		return;
	}

	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	const FReferenceSkeleton& RefSkel = BoneContainer.GetReferenceSkeleton();

	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	const UWorld* World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();


	TArray<FEffectorInternal> EffectorInternals;
	for (const FFBIKEffector& Effector : EffectorContainer.Effectors)
	{
		if (Effector.EffectorBoneName == NAME_None || Effector.RootBoneName == NAME_None)
		{
			continue;
		}

		FEffectorInternal EffectorInternal;
		EffectorInternal.EffectorType = Effector.EffectorType;
		EffectorInternal.EffectorBoneIndex = RefSkel.FindBoneIndex(Effector.EffectorBoneName);
		EffectorInternal.RootBoneIndex = RefSkel.FindBoneIndex(Effector.RootBoneName);

		if (EffectorInternal.EffectorBoneIndex == INDEX_NONE || EffectorInternal.RootBoneIndex == INDEX_NONE)
		{
			continue;
		}

		int32 BoneIndex = EffectorInternal.EffectorBoneIndex;

		if (!SolverInternals.Contains(BoneIndex))
		{
			continue;
		}

		EffectorInternal.ParentBoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;
		EffectorInternal.Location = Effector.Location;
		EffectorInternal.Rotation = Effector.Rotation;

		bool bValidation = true;
		while (true)
		{
			int32 ParentBoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;

			if (ParentBoneIndex == INDEX_NONE)
			{
				bValidation = false;
				break;
			}
			else if (ParentBoneIndex == EffectorInternal.RootBoneIndex)
			{
				break;
			}

			BoneIndex = ParentBoneIndex;
		}
		if (!bValidation)
		{
			continue;
		}

		EffectorInternals.Add(EffectorInternal);
	}

	int32 EffectorCount = EffectorInternals.Num();
	if (EffectorCount <= 0)
	{
		return;
	}

	for (int32 BoneIndex : BoneIndices)
	{
		PredictionAnimInstance->InitializeBoneOffset(BoneIndex);

		// 初期Transformを保存
		auto CompactPoseBoneIndex = FCompactPoseBoneIndex(BoneIndex);
		auto& SolverInternal = SolverInternals[BoneIndex];
		SolverInternal.LocalTransform = Context.Pose.GetLocalSpaceTransform(CompactPoseBoneIndex);
		SolverInternal.ComponentTransform = Context.Pose.GetComponentSpaceTransform(CompactPoseBoneIndex);
		SolverInternal.InitLocalTransform = SolverInternal.LocalTransform;
		SolverInternal.InitComponentTransform = SolverInternal.ComponentTransform;
	}

	FMemory::Memzero(Matrices.Rt1, BoneAxisCount * sizeof(float));

	SolveSolver(FEffectorInternal(), 0, FTransform::Identity, EIKUpdateMode::Initial, FTransform::Identity, ComponentToWorld, nullptr, nullptr);

	// 重心の更新
	UpdateCenterOfMass();

	int32 StepLoopCount = 0;
	int32 EffectiveCount = 0;
	while (Setting->StepLoopCountMax > 0 && Setting->EffectiveCountMax > 0 && StepLoopCount < Setting->StepLoopCountMax)
	{
		SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_Loop);

		for (int32 EffectorIndex = 0; EffectorIndex < EffectorCount; ++EffectorIndex)
		{
			const FEffectorInternal& Effector = EffectorInternals[EffectorIndex];
			float EffectorStep[AXIS_COUNT];
			float EtaStep = 0.f;

			FMemory::Memzero(EffectorStep, sizeof(float) * AXIS_COUNT);

			const int32 DisplacementCount = BoneAxisCount;

			switch (Effector.EffectorType)
			{
				case EFullbodyIkEffectorType::KeepLocation:
				{
					const FVector EndSolverLocation = GetWorldSpaceBoneLocation(Context, Effector.EffectorBoneIndex);
					const FVector DeltaLocation = Effector.Location - EndSolverLocation;
					const float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						const float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}
				}
				break;

				case EFullbodyIkEffectorType::KeepRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					SolveSolver(Effector, 0, FTransform::Identity, EIKUpdateMode::KeepRotation, FTransform::Identity, ComponentToWorld, nullptr, nullptr);
				}
				break;

				case EFullbodyIkEffectorType::KeepLocationAndRotation:
				{
					const FSolverInternal& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];

					// Transform更新
					SolveSolver(Effector, 0, FTransform::Identity, EIKUpdateMode::KeepRotation, FTransform::Identity, ComponentToWorld, nullptr, nullptr);

					FVector EndSolverLocation = GetWorldSpaceBoneLocation(Context, Effector.EffectorBoneIndex);
					FVector DeltaLocation = Effector.Location - EndSolverLocation;
					const float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}
				}
				break;

				case EFullbodyIkEffectorType::FollowOriginalLocation:
				{
					const FSolverInternal& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * ComponentToWorld;

					const FVector& TargetWorldPos = ComponentToWorld.TransformPosition(SolverInternal.InitComponentTransform.GetLocation());
					const FVector& FinalTargetPos = TargetWorldPos + ComponentToWorld.TransformVector(Effector.Location);
					const FVector& EndSolverLocation = GetWorldSpaceBoneLocation(Context, Effector.EffectorBoneIndex);
					const FVector& DeltaLocation = FinalTargetPos - EndSolverLocation;
					const float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}
				}
				break;

				case EFullbodyIkEffectorType::FollowOriginalRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * ComponentToWorld;

					// Transform更新
					SolveSolver(Effector, 0, FTransform::Identity, EIKUpdateMode::FollowRotation, InitWorldTransform, ComponentToWorld, nullptr, nullptr);
				}
				break;

				case EFullbodyIkEffectorType::FollowOriginalLocationAndRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * ComponentToWorld;

					// Transform更新
					SolveSolver(Effector, 0, FTransform::Identity, EIKUpdateMode::FollowRotation, InitWorldTransform, ComponentToWorld, nullptr, nullptr);

					const FVector& TargetWorldPos = ComponentToWorld.TransformPosition(SolverInternal.InitComponentTransform.GetLocation());
					const FVector& FinalTargetPos = TargetWorldPos + ComponentToWorld.TransformVector(Effector.Location);
					const FVector& EndSolverLocation = GetWorldSpaceBoneLocation(Context, Effector.EffectorBoneIndex);
					const FVector& DeltaLocation = FinalTargetPos - EndSolverLocation;
					const float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}
				}
				break;
			}

			if (EtaStep <= 0.f)
			{
				continue;
			}


			// calc jacobian function
			EtaStep /= Setting->StepSize;

			auto J = FBuffer(Matrices.J, DisplacementCount, AXIS_COUNT);
			auto Jt = FBuffer(Matrices.Jt, AXIS_COUNT, DisplacementCount);
			auto JtJ = FBuffer(Matrices.JtJ, AXIS_COUNT, AXIS_COUNT);
			auto JtJi = FBuffer(Matrices.JtJi, AXIS_COUNT, AXIS_COUNT);
			auto Jp = FBuffer(Matrices.Jp, AXIS_COUNT, DisplacementCount);
			auto W0 = FBuffer(Matrices.W0, BoneAxisCount);
			auto Wi = FBuffer(Matrices.Wi, DisplacementCount, DisplacementCount);
			auto JtWi = FBuffer(Matrices.JtWi, AXIS_COUNT, DisplacementCount);
			auto JtWiJ = FBuffer(Matrices.JtWiJ, AXIS_COUNT, AXIS_COUNT);
			auto JtWiJi = FBuffer(Matrices.JtWiJi, AXIS_COUNT, AXIS_COUNT);
			auto JtWiJiJt = FBuffer(Matrices.JtWiJiJt, AXIS_COUNT, DisplacementCount);
			auto Jwp = FBuffer(Matrices.Jwp, AXIS_COUNT, DisplacementCount);
			auto Rt1 = FBuffer(Matrices.Rt1, BoneAxisCount);
			auto Eta = FBuffer(Matrices.Eta, BoneAxisCount);
			auto EtaJ = FBuffer(Matrices.EtaJ, AXIS_COUNT);
			auto EtaJJp = FBuffer(Matrices.EtaJJp, BoneAxisCount);
			auto Rt2 = FBuffer(Matrices.Rt2, BoneAxisCount);

			// ヤコビアン J
			J.Reset();
			switch (Effector.EffectorType)
			{
				case EFullbodyIkEffectorType::KeepLocation:
				case EFullbodyIkEffectorType::KeepLocationAndRotation:
				case EFullbodyIkEffectorType::FollowOriginalLocation:
				case EFullbodyIkEffectorType::FollowOriginalLocationAndRotation:
				{
					CalcJacobian(Context, Effector, J.Ptr());
				}
				break;
			}

			// J^T
			Jt.Reset();
			MatrixTranspose(Jt.Ptr(), J.Ptr(), DisplacementCount, AXIS_COUNT);

			// J^T * J
			JtJ.Reset();
			MatrixMultiply(JtJ.Ptr(), Jt.Ptr(), AXIS_COUNT, DisplacementCount, J.Ptr(), DisplacementCount, AXIS_COUNT);

			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				JtJ.Ref(i, i) += Setting->JtJInverseBias;
			}

			// (J^T * J)^-1
			JtJi.Reset();
			float DetJtJ = MatrixInverse3(JtJi.Ptr(), JtJ.Ptr());
			if (DetJtJ == 0)
			{
				continue;
			}

			// ヤコビアン擬似逆行列 (J^T * J)^-1 * J^T
			Jp.Reset();
			MatrixMultiply(Jp.Ptr(), JtJi.Ptr(), AXIS_COUNT, AXIS_COUNT, Jt.Ptr(), AXIS_COUNT, DisplacementCount);

			// W^-1
			Wi.Reset();
			for (int32 i = 0; i < DisplacementCount; ++i)
			{
				Wi.Ref(i, i) = 1.0f / W0.Ref(i % BoneAxisCount);
			}

			// J^T * W^-1
			JtWi.Reset();
			MatrixMultiply(JtWi.Ptr(), Jt.Ptr(), AXIS_COUNT, DisplacementCount, Wi.Ptr(), DisplacementCount, DisplacementCount);
			
			// J^T * W^-1 * J
			JtWiJ.Reset();
			MatrixMultiply(JtWiJ.Ptr(), JtWi.Ptr(), AXIS_COUNT, DisplacementCount, J.Ptr(), DisplacementCount, AXIS_COUNT);
			
			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				JtWiJ.Ref(i, i) += Setting->JtJInverseBias;
			}

			// (J^T * W^-1 * J)^-1
			JtWiJi.Reset();
			float DetJtWiJ = MatrixInverse3(JtWiJi.Ptr(), JtWiJ.Ptr());
			if (DetJtWiJ == 0)
			{
				continue;
			}

			// (J^T * W^-1 * J)^-1 * J^T
			JtWiJiJt.Reset();
			MatrixMultiply(JtWiJiJt.Ptr(), JtWiJi.Ptr(), AXIS_COUNT, AXIS_COUNT, Jt.Ptr(), AXIS_COUNT, DisplacementCount);
			
			// ヤコビアン加重逆行列 (J^T * W^-1 * J)^-1 * J^T * W^-1
			Jwp.Reset();
			MatrixMultiply(Jwp.Ptr(), JtWiJiJt.Ptr(), AXIS_COUNT, DisplacementCount, Wi.Ptr(), DisplacementCount, DisplacementCount);
			
			// 関節角度ベクトル1 目標エフェクタ変位 * ヤコビアン加重逆行列
			Rt1.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				Rt1.Ref(i) += EffectorStep[0] * Jwp.Ref(0, i) + EffectorStep[1] * Jwp.Ref(1, i) + EffectorStep[2] * Jwp.Ref(2, i);
			}

			// 冗長変数 Eta
			Eta.Reset();
			if (EtaStep > 0.f)
			{
				for (int32 i = 0; i < BoneCount; ++i)
				{
					SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_CulcEta);
					int32 BoneIndex = BoneIndices[i];
					auto& SolverInternal = SolverInternals[BoneIndex];

					if (SolverInternal.bTranslation)
					{
						FVector CurrentLocation = GetLocalSpaceBoneLocation(BoneIndex);
						FVector InputLocation = SolverInternal.InitLocalTransform.GetLocation();
						auto CalcEta = [&](int32 Axis, float CurrentPosition, float InputPosition, const FFullbodyIKSolverAxis& SolverAxis)
						{
							CurrentPosition += Rt1.Ref(i * AXIS_COUNT + Axis);
							float DeltaPosition = CurrentPosition - InputPosition;

							const auto L_EtaStep = (Setting->EtaSize * SolverAxis.EtaBias * EtaStep);

							if (!FMath::IsNearlyZero(DeltaPosition))
							{
								Eta.Ref(i * AXIS_COUNT + Axis) = GetMappedRangeEaseInClamped(0, 90.0f, 0, L_EtaStep, 
									1.f, FMath::Abs(DeltaPosition)) * (DeltaPosition > 0 ? -1 : 1);
							}
						};
						CalcEta(0, CurrentLocation.X, InputLocation.X, SolverInternal.X);
						CalcEta(1, CurrentLocation.Y, InputLocation.Y, SolverInternal.Y);
						CalcEta(2, CurrentLocation.Z, InputLocation.Z, SolverInternal.Z);
					}
					else
					{
						FRotator CurrentRotation = GetLocalSpaceBoneRotation(BoneIndex).Rotator();
						FRotator InputRotation = SolverInternal.InitLocalTransform.Rotator();
						auto CalcEta = [&](int32 Axis, float CurrentAngle, float InputAngle, const FFullbodyIKSolverAxis& SolverAxis)
						{
							CurrentAngle += FMath::RadiansToDegrees(Rt1.Ref(i * AXIS_COUNT + Axis));
							float DeltaAngle = FRotator::NormalizeAxis(CurrentAngle - InputAngle);

							const auto L_EtaStep = (Setting->EtaSize * SolverAxis.EtaBias * EtaStep);

							if (!FMath::IsNearlyZero(DeltaAngle))
							{
								Eta.Ref(i * AXIS_COUNT + Axis) = GetMappedRangeEaseInClamped(0, 90.0f, 0, L_EtaStep,
									1.f, FMath::Abs(DeltaAngle)) * (DeltaAngle > 0 ? -1 : 1);
							}
						};
						CalcEta(0, CurrentRotation.Roll, InputRotation.Roll, SolverInternal.X);
						CalcEta(1, CurrentRotation.Pitch, InputRotation.Pitch, SolverInternal.Y);
						CalcEta(2, CurrentRotation.Yaw, InputRotation.Yaw, SolverInternal.Z);
					}
				}
			}

			// Eta * J
			EtaJ.Reset();
			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				for (int32 j = 0; j < BoneAxisCount; ++j)
				{
					EtaJ.Ref(i) += Eta.Ref(j) * J.Ref(j, i);
				}
			}

			// Eta * J * J^+
			EtaJJp.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				for (int32 j = 0; j < AXIS_COUNT; ++j)
				{
					EtaJJp.Ref(i) += EtaJ.Ref(j) * Jp.Ref(j, i);
				}
			}

			// 冗長項 Eta - Eta * J * J^+
			Rt2.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				Rt2.Ref(i) = Eta.Ref(i) - EtaJJp.Ref(i);
			}


			// Transform更新
			SolveSolver(Effector, 0, FTransform::Identity, EIKUpdateMode::KeepBoth, FTransform::Identity, ComponentToWorld, &Rt1, &Rt2);

			++EffectiveCount;

			if (EffectiveCount >= Setting->EffectiveCountMax)
			{
				break;
			}
		}

		// 重心の更新
		UpdateCenterOfMass();
		++StepLoopCount;
		if (EffectiveCount >= Setting->EffectiveCountMax)
		{
			break;
		}
	}

	for (int32 BoneIndex : BoneIndices)
	{
		OutBoneTransforms.Add(FBoneTransform(FCompactPoseBoneIndex(BoneIndex), SolverInternals[BoneIndex].ComponentTransform));
	}



	for (const FName& BoneName : DebugDumpBoneNames)
	{
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (SolverInternals.Contains(BoneIndex))
		{
			UE_LOG(LogQuadrupedIK, Log, TEXT("%s (%d) -----------------"), *BoneName.ToString(), BoneIndex);
			UE_LOG(LogQuadrupedIK, Log, TEXT("ComponentSpace : %s"), *SolverInternals[BoneIndex].ComponentTransform.ToString());
			UE_LOG(LogQuadrupedIK, Log, TEXT("LocalSpace : %s"), *SolverInternals[BoneIndex].LocalTransform.ToString());
			UE_LOG(LogQuadrupedIK, Log, TEXT("Rotator : %s"), *SolverInternals[BoneIndex].LocalTransform.Rotator().ToString());
			UE_LOG(LogQuadrupedIK, Log, TEXT(""));
		}
	}

	if (bDebugShowEffectiveCount)
	{
		FAnimInstanceProxy* RawProxyPtr = Context.AnimInstanceProxy;
		if (RawProxyPtr)
		{
			FIKBaseAnimInstanceProxy* Proxy = static_cast<FIKBaseAnimInstanceProxy*>(RawProxyPtr);
			if (Proxy)
			{
				Proxy->IKDebugData.CenterOfMass = ComponentToWorld.TransformPosition(CenterOfMass);
				Proxy->IKDebugData.Radius = DebugShowCenterOfMassRadius;
				Proxy->IKDebugData.bValid = true;
			}
		}

		//UE_LOG(LogQuadrupedIK, Log, TEXT("[%s] : Effective Count : %d"), *FString(__FUNCTION__), EffectiveCount);
	}

}



void FAnimNode_WvFullbodyIK::CalcJacobian(FComponentSpacePoseContext& Context, const WvFBIK::FEffectorInternal& EffectorInternal, float* Jacobian)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_CalcJacobian);

	int32 BoneIndex = EffectorInternal.EffectorBoneIndex;
	const auto EndSolverLocation = GetWorldSpaceBoneLocation(Context, BoneIndex);
	BoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;

	while (true)
	{
		const auto& SolverInternal = SolverInternals[BoneIndex];
		int32 ParentBoneIndex = SolverInternal.ParentBoneIndex;

		FQuat ParentWorldRotation = FQuat::Identity;
		if (ParentBoneIndex != INDEX_NONE)
		{
			ParentWorldRotation = GetWorldSpaceBoneRotation(Context, ParentBoneIndex);
		}

		if (SolverInternal.bTranslation)
		{
			FVector DVec[AXIS_COUNT];
			DVec[0] = FVector(1, 0, 0);
			DVec[1] = FVector(0, 1, 0);
			DVec[2] = FVector(0, 0, 1);

			for (int32 Axis = 0; Axis < AXIS_COUNT; ++Axis)
			{
				int32 JacobianIndex1 = SolverInternal.BoneIndicesIndex * AXIS_COUNT;
				int32 JacobianIndex2 = Axis;
				FVector DVec3 = ParentWorldRotation.RotateVector(DVec[Axis]);
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 0] = DVec3.X;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 1] = DVec3.Y;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 2] = DVec3.Z;
			}
		}
		else
		{
			FVector DeltaLocation = EndSolverLocation - GetWorldSpaceBoneLocation(Context, BoneIndex);
			FVector UnrotateDeltaLocation = GetWorldSpaceBoneRotation(Context, BoneIndex).UnrotateVector(DeltaLocation);
			FRotator BoneRotation = GetLocalSpaceBoneRotation(BoneIndex).Rotator();
			FMatrix DMat[AXIS_COUNT];

			DMat[0] = DiffX(BoneRotation.Roll) * RotY(BoneRotation.Pitch) * RotZ(BoneRotation.Yaw);
			DMat[1] = RotX(BoneRotation.Roll) * DiffY(BoneRotation.Pitch) * RotZ(BoneRotation.Yaw);
			DMat[2] = RotX(BoneRotation.Roll) * RotY(BoneRotation.Pitch) * DiffZ(BoneRotation.Yaw);

			for (int32 Axis = 0; Axis < AXIS_COUNT; ++Axis)
			{
				int32 JacobianIndex1 = SolverInternal.BoneIndicesIndex * AXIS_COUNT;
				int32 JacobianIndex2 = Axis;
				FVector4 DVec4 = DMat[Axis].TransformVector(UnrotateDeltaLocation);
				FVector DVec3 = ParentWorldRotation.RotateVector(FVector(DVec4));
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 0] = DVec3.X;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 1] = DVec3.Y;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 2] = DVec3.Z;
			}
		}

		if (BoneIndex == EffectorInternal.RootBoneIndex || ParentBoneIndex == INDEX_NONE)
		{
			break;
		}

		BoneIndex = ParentBoneIndex;
	}
}


void FAnimNode_WvFullbodyIK::SolveSolver(
	const WvFBIK::FEffectorInternal& Effector,
	const int32 BoneIndex,
	const FTransform& ParentComponentTransform,
	const EIKUpdateMode IKUpdateMode, 
	const FTransform& InitWorldTransform,
	const FTransform& ComponentToWorld, 
	WvFBIK::FBuffer* Rt1,
	WvFBIK::FBuffer* Rt2)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_SolveSolver);

	FSolverInternal& SolverInternal = SolverInternals[BoneIndex];

	if (SolverInternal.bTranslation)
	{
		FVector SavedOffsetLocation = PredictionAnimInstance->GetBoneLocationOffset(BoneIndex);
		FVector CurrentOffsetLocation = SolverInternal.LocalTransform.GetLocation();

		switch (IKUpdateMode)
		{
		case EIKUpdateMode::Initial:
			CurrentOffsetLocation += SavedOffsetLocation;
			break;
		case EIKUpdateMode::KeepLocation:
			break;
		case EIKUpdateMode::FollowLocation:
			break;
		case EIKUpdateMode::KeepBoth:
		{
			const int32 BoneIndicesIndex = SolverInternal.BoneIndicesIndex;

			const FVector DeltaLocation = FVector(
				Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 0) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 0),	// X
				Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 1) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 1),	// Y
				Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 2) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 2)	// Z
			);
			SavedOffsetLocation += DeltaLocation;
			CurrentOffsetLocation += DeltaLocation;
		}
			break;
		}


		if (SolverInternal.bLimited)
		{
			if (CurrentOffsetLocation.X < SolverInternal.X.LimitMin)
			{
				const float Offset = SolverInternal.X.LimitMin - CurrentOffsetLocation.X;
				CurrentOffsetLocation.X += Offset;
				SavedOffsetLocation.X += Offset;
			}
			else if (CurrentOffsetLocation.X > SolverInternal.X.LimitMax)
			{
				const float Offset = SolverInternal.X.LimitMax - CurrentOffsetLocation.X;
				CurrentOffsetLocation.X += Offset;
				SavedOffsetLocation.X += Offset;
			}

			if (CurrentOffsetLocation.Y < SolverInternal.Y.LimitMin)
			{
				const float Offset = SolverInternal.Y.LimitMin - CurrentOffsetLocation.Y;
				CurrentOffsetLocation.Y += Offset;
				SavedOffsetLocation.Y += Offset;
			}
			else if (CurrentOffsetLocation.Y > SolverInternal.Y.LimitMax)
			{
				const float Offset = SolverInternal.Y.LimitMax - CurrentOffsetLocation.Y;
				CurrentOffsetLocation.Y += Offset;
				SavedOffsetLocation.Y += Offset;
			}

			if (CurrentOffsetLocation.Z < SolverInternal.Z.LimitMin)
			{
				const float Offset = SolverInternal.Z.LimitMin - CurrentOffsetLocation.Z;
				CurrentOffsetLocation.Z += Offset;
				SavedOffsetLocation.Z += Offset;
			}
			else if (CurrentOffsetLocation.Z > SolverInternal.Z.LimitMax)
			{
				const float Offset = SolverInternal.Z.LimitMax - CurrentOffsetLocation.Z;
				CurrentOffsetLocation.Z += Offset;
				SavedOffsetLocation.Z += Offset;
			}
		}

		if (SavedOffsetLocation.ContainsNaN())
		{
			SavedOffsetLocation = PredictionAnimInstance->GetBoneLocationOffset(BoneIndex);
		}
		if (CurrentOffsetLocation.ContainsNaN())
		{
			CurrentOffsetLocation = SolverInternal.LocalTransform.GetLocation();
		}

		PredictionAnimInstance->SetBoneLocationOffset(BoneIndex, SavedOffsetLocation);
		SolverInternal.LocalTransform.SetLocation(CurrentOffsetLocation);
	}
	else
	{
		FRotator SavedOffsetRotation = PredictionAnimInstance->GetBoneRotationOffset(BoneIndex);
		FRotator CurrentOffsetRotation = SolverInternal.LocalTransform.Rotator();


		switch (IKUpdateMode)
		{
		case EIKUpdateMode::Initial:
			CurrentOffsetRotation += SavedOffsetRotation;
			break;
		case EIKUpdateMode::KeepRotation:
		{
			if (BoneIndex == Effector.EffectorBoneIndex)
			{
				const FTransform EffectorWorldTransform = FTransform(Effector.Rotation);
				const FTransform EffectorComponentTransform = EffectorWorldTransform * ComponentToWorld.Inverse();
				const FTransform EffectorLocalTransform = EffectorComponentTransform * 
					SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();
				const FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
				const FRotator DeltaLocalRotation = EffectorLocalRotation - CurrentOffsetRotation;
				SavedOffsetRotation += DeltaLocalRotation;
				CurrentOffsetRotation += DeltaLocalRotation;
			}
		}
			break;
		case EIKUpdateMode::FollowRotation:
		{
			if (BoneIndex == Effector.EffectorBoneIndex)
			{
				const FTransform EffectorWorldTransform = FTransform(InitWorldTransform.Rotator());
				const FTransform EffectorComponentTransform = EffectorWorldTransform * ComponentToWorld.Inverse();
				const FTransform EffectorLocalTransform = EffectorComponentTransform *
					SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();
				const FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
				const FRotator DeltaLocalRotation = EffectorLocalRotation + Effector.Rotation - CurrentOffsetRotation;
				SavedOffsetRotation += DeltaLocalRotation;
				CurrentOffsetRotation += DeltaLocalRotation;
			}

		}
			break;
		case EIKUpdateMode::KeepBoth:
		{
			const int32 BoneIndicesIndex = SolverInternal.BoneIndicesIndex;

			FRotator DeltaRotation = FRotator(
				FMath::RadiansToDegrees(Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 1) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 1)),	// Pitch
				FMath::RadiansToDegrees(Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 2) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 2)),	// Yaw
				FMath::RadiansToDegrees(Rt1->Ref(BoneIndicesIndex * AXIS_COUNT + 0) + Rt2->Ref(BoneIndicesIndex * AXIS_COUNT + 0))	// Roll
			);

			// ContainsNaN() で非数を除去し、各成分が現実的な範囲（例: 360度以内）かチェック
			bool bIsInvalid = DeltaRotation.ContainsNaN();
			if (!bIsInvalid)
			{
				// 1ステップで180度以上回るような計算結果は、行列が特異点に近く、数値的に破綻している
				if (FMath::Abs(DeltaRotation.Pitch) > 180.f ||
					FMath::Abs(DeltaRotation.Yaw) > 180.f ||
					FMath::Abs(DeltaRotation.Roll) > 180.f)
				{
					bIsInvalid = true;
				}
			}

			if (bIsInvalid)
			{
				DeltaRotation = FRotator::ZeroRotator;
			}

			SavedOffsetRotation += DeltaRotation;
			CurrentOffsetRotation += DeltaRotation;

			SavedOffsetRotation.Normalize();
			CurrentOffsetRotation.Normalize();
		}
			break;
		}


		if (SolverInternal.bLimited)
		{
			if (CurrentOffsetRotation.Roll < SolverInternal.X.LimitMin)
			{
				const float Offset = SolverInternal.X.LimitMin - CurrentOffsetRotation.Roll;
				CurrentOffsetRotation.Roll += Offset;
				SavedOffsetRotation.Roll += Offset;
			}
			else if (CurrentOffsetRotation.Roll > SolverInternal.X.LimitMax)
			{
				const float Offset = SolverInternal.X.LimitMax - CurrentOffsetRotation.Roll;
				CurrentOffsetRotation.Roll += Offset;
				SavedOffsetRotation.Roll += Offset;
			}

			if (CurrentOffsetRotation.Pitch < SolverInternal.Y.LimitMin)
			{
				const float Offset = SolverInternal.Y.LimitMin - CurrentOffsetRotation.Pitch;
				CurrentOffsetRotation.Pitch += Offset;
				SavedOffsetRotation.Pitch += Offset;
			}
			else if (CurrentOffsetRotation.Pitch > SolverInternal.Y.LimitMax)
			{
				const float Offset = SolverInternal.Y.LimitMax - CurrentOffsetRotation.Pitch;
				CurrentOffsetRotation.Pitch += Offset;
				SavedOffsetRotation.Pitch += Offset;
			}

			if (CurrentOffsetRotation.Yaw < SolverInternal.Z.LimitMin)
			{
				const float Offset = SolverInternal.Z.LimitMin - CurrentOffsetRotation.Yaw;
				CurrentOffsetRotation.Yaw += Offset;
				SavedOffsetRotation.Yaw += Offset;
			}
			else if (CurrentOffsetRotation.Yaw > SolverInternal.Z.LimitMax)
			{
				const float Offset = SolverInternal.Z.LimitMax - CurrentOffsetRotation.Yaw;
				CurrentOffsetRotation.Yaw += Offset;
				SavedOffsetRotation.Yaw += Offset;
			}
		}

		if (CurrentOffsetRotation.ContainsNaN())
		{
			CurrentOffsetRotation = SolverInternal.LocalTransform.Rotator();
		}

		if (SavedOffsetRotation.ContainsNaN())
		{
			SavedOffsetRotation = PredictionAnimInstance->GetBoneRotationOffset(BoneIndex);
		}

		FQuat NewQuat(CurrentOffsetRotation);
		if (!NewQuat.IsNormalized() || NewQuat.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			NewQuat = FQuat::Identity;
		}

		FQuat NewSavedQuat(SavedOffsetRotation);
		if (!NewSavedQuat.IsNormalized() || NewSavedQuat.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			NewSavedQuat = FQuat::Identity;
		}

		PredictionAnimInstance->SetBoneRotationOffset(BoneIndex, FRotator(NewSavedQuat));
		SolverInternal.LocalTransform.SetRotation(NewQuat);
	}

	SolverInternal.ComponentTransform = SolverInternal.LocalTransform * ParentComponentTransform;

	if (SolverTree.Contains(BoneIndex))
	{
		for (auto ChildBoneIndex : SolverTree[BoneIndex])
		{
			SolveSolver(Effector, ChildBoneIndex, SolverInternal.ComponentTransform, 
				IKUpdateMode, InitWorldTransform, ComponentToWorld, Rt1, Rt2);
		}
	}
}


void FAnimNode_WvFullbodyIK::UpdateCenterOfMass()
{

	CenterOfMass = FVector::ZeroVector;
	float MassSum = 0.f;

	for (const int32& BoneIndex : BoneIndices)
	{
		const auto& SolverInternal = SolverInternals[BoneIndex];
		const int32& ParentBoneIndex = SolverInternal.ParentBoneIndex;

		if (ParentBoneIndex == INDEX_NONE)
		{
			continue;
		}
		const auto& ParentSolverInternal = SolverInternals[ParentBoneIndex];
		CenterOfMass += (SolverInternal.ComponentTransform.GetLocation() + ParentSolverInternal.ComponentTransform.GetLocation()) * 0.5f * SolverInternal.Mass;
		MassSum += SolverInternal.Mass;
	}
	CenterOfMass /= MassSum;
}


void FAnimNode_WvFullbodyIK::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);
}

bool FAnimNode_WvFullbodyIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return true;
}

void FAnimNode_WvFullbodyIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
}


#pragma region Misc

FFullbodyIKSolver FAnimNode_WvFullbodyIK::GetSolver(const FName& BoneName) const
{
	check(Setting);

	for (auto& Solver : Setting->Solvers)
	{
		if (Solver.BoneName == BoneName)
		{
			return Solver;
		}
	}

	// 見つからなければデフォルト
	FFullbodyIKSolver Solver;
	Solver.BoneName = BoneName;

	return Solver;
}

FTransform FAnimNode_WvFullbodyIK::GetWorldSpaceBoneTransform(FComponentSpacePoseContext& Context, const int32& BoneIndex) const
{
	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= ComponentToWorld;
	return Transform;
}


FVector FAnimNode_WvFullbodyIK::GetWorldSpaceBoneLocation(FComponentSpacePoseContext& Context, const int32& BoneIndex) const
{
	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= ComponentToWorld;
	return Transform.GetLocation();
}


FQuat FAnimNode_WvFullbodyIK::GetWorldSpaceBoneRotation(FComponentSpacePoseContext& Context, const int32& BoneIndex) const
{
	const FTransform& ComponentToWorld = Context.AnimInstanceProxy->GetComponentTransform();
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= ComponentToWorld;
	return Transform.GetRotation();
}


FTransform FAnimNode_WvFullbodyIK::GetLocalSpaceBoneTransform(const int32& BoneIndex) const
{
	return SolverInternals[BoneIndex].LocalTransform;
}


FVector FAnimNode_WvFullbodyIK::GetLocalSpaceBoneLocation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].LocalTransform;
	return Transform.GetLocation();
}


FQuat FAnimNode_WvFullbodyIK::GetLocalSpaceBoneRotation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].LocalTransform;
	return Transform.GetRotation();
}
#pragma endregion


#if WITH_EDITOR
void FAnimNode_WvFullbodyIK::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const
{
}
#endif

