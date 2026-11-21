// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mWeaponRetargetAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "TwoBoneIK.h"

namespace
{
class FWeaponRetargetProxy final : public FAnimInstanceProxy
{
public:
	explicit FWeaponRetargetProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	void SetPreviousPose(TArray<FTransform>&& Pose, TBitArray<>&& Bones, float Seconds)
	{
		PreviousLocalPose = MoveTemp(Pose);
		PreviousBoneMask = MoveTemp(Bones);
		Duration = Seconds;
		Elapsed = 0.0f;
		OldLegWeight = 1.0f;
	}

	void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto* Character = Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner());
		const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		CaptureRollFloor(Instance, Character);
		if (PreviousLocalPose.IsEmpty()) { return; }
		Elapsed += FMath::Max(0.0f, DeltaSeconds);
		if (Elapsed >= Duration || !Movement || !Movement->IsMovingOnGround() || Character->IsDodging() || Character->IsDead())
		{
			PreviousLocalPose.Reset();
			return;
		}
		// The first evaluation can happen before the new graph has live foot motion.
		// Preserve the last complete pose and ease the legs back into the live stride.
		const float StanceWeight = FMath::GetMappedRangeValueClamped(FVector2f(10.0f, 120.0f), FVector2f(1.0f, 0.0f), Movement->Velocity.Size2D());
		const float LegBlend = 1.0f - FMath::Exp(-Elapsed / FMath::Min(Duration, 0.055f));
		OldLegWeight = FMath::Lerp(1.0f, StanceWeight, LegBlend);
	}

	bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* Root) override
	{
		if (!Root || Root != GetRootNode()) { return false; }
		EvaluateAnimationNode_WithRoot(Output, Root);
		SolveRollPalmSupport(Output.Pose);
		if (PreviousLocalPose.IsEmpty()) { return true; }

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const auto Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(TEXT("spine_01"))));
		const float Weight = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Elapsed / Duration);
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
			if (!PreviousBoneMask.IsValidIndex(MeshIndex) || !PreviousBoneMask[MeshIndex]) { continue; }
			const bool bUpperBody = Spine != INDEX_NONE && (Bone == Spine || Bones.BoneIsChildOf(Bone, Spine));
			Output.Pose[Bone].BlendWith(PreviousLocalPose[MeshIndex], Weight * (bUpperBody ? 1.0f : OldLegWeight));
		}
		return true;
	}

private:
	void CaptureRollFloor(const UAnimInstance* Instance, const ARogue10mCharacter* Character)
	{
		bHasRollFloor = false;
#if WITH_EDITOR
		if (!URogue10mWeaponVisualComponent::IsRollSwordFloorCorrectionEnabled()) { return; }
#endif
		const auto* Mesh = Instance->GetSkelMeshComponent();
		float Progress = 0.0f;
		FVector Direction;
		if (!Mesh || !Character || !Character->IsThirdPersonGameplayActive()
			|| Character->GetEquippedWeaponType() != ERogue10mWeaponType::GreatSword
			|| !Character->GetRollPresentation(Progress, Direction)
			|| !URogue10mWeaponVisualComponent::GetRollSwordFloorPlane(*Character, RollFloorPoint, RollFloorNormal)) { return; }

		RollMeshToWorld = Mesh->GetComponentTransform();
		bHasRollFloor = true;
	}

	void SolveRollPalmSupport(FCompactPose& Pose) const
	{
		if (!bHasRollFloor) { return; }
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const auto FindBone = [&Bones](FName Name)
		{
			return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(Name)));
		};
		const FCompactPoseBoneIndex UpperIndex = FindBone(TEXT("upperarm_r"));
		const FCompactPoseBoneIndex LowerIndex = FindBone(TEXT("lowerarm_r"));
		const FCompactPoseBoneIndex HandIndex = FindBone(TEXT("hand_r"));
		const FCompactPoseBoneIndex FingerIndex = FindBone(TEXT("middle_01_r"));
		if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE || FingerIndex == INDEX_NONE) { return; }

		FCSPose<FCompactPose> ComponentPose;
		ComponentPose.InitPose(Pose);
		FTransform Upper = ComponentPose.GetComponentSpaceTransform(UpperIndex);
		FTransform Lower = ComponentPose.GetComponentSpaceTransform(LowerIndex);
		FTransform Hand = ComponentPose.GetComponentSpaceTransform(HandIndex);
		const FVector Palm = (Hand.GetLocation() + ComponentPose.GetComponentSpaceTransform(FingerIndex).GetLocation()) * 0.5;
		const double Height = FVector::DotProduct(RollMeshToWorld.TransformPosition(Palm) - RollFloorPoint, RollFloorNormal);
		// The near-horizontal GreatSword extends 5.62 cm below its grip at the authored carry angle.
		constexpr double CarryPalmHeightCm = 6.25;
		if (Height >= CarryPalmHeightCm) { return; }

		const FVector Shoulder = Upper.GetLocation();
		const FVector ArmAxis = (Hand.GetLocation() - Shoulder).GetSafeNormal();
		const FVector ElbowPole = FVector::VectorPlaneProject(Lower.GetLocation() - Shoulder, ArmAxis);
		const FVector Target = Hand.GetLocation()
			+ RollMeshToWorld.InverseTransformVector(RollFloorNormal * (CarryPalmHeightCm - Height));
		const FVector TargetAxis = (Target - Shoulder).GetSafeNormal();
		if (ArmAxis.IsNearlyZero() || ElbowPole.IsNearlyZero() || TargetAxis.IsNearlyZero()) { return; }

		// Carry the current bend plane to the raised wrist target, preserving both segment lengths and wrist orientation.
		const FVector Pole = Shoulder + FQuat::FindBetweenNormals(ArmAxis, TargetAxis).RotateVector(ElbowPole);
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1.0, 1.0);
		const FBoneTransform Adjustments[] = { {UpperIndex, Upper}, {LowerIndex, Lower}, {HandIndex, Hand} };
		ComponentPose.LocalBlendCSBoneTransforms(Adjustments, 1.0f);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(ComponentPose, Pose);
		Pose.NormalizeRotations();
	}

	TArray<FTransform> PreviousLocalPose;
	TBitArray<> PreviousBoneMask;
	float Duration = 0.0f;
	float Elapsed = 0.0f;
	float OldLegWeight = 1.0f;
	FTransform RollMeshToWorld;
	FVector RollFloorPoint = FVector::ZeroVector;
	FVector RollFloorNormal = FVector::UpVector;
	bool bHasRollFloor = false;
};
}

void URogue10mWeaponRetargetAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	auto* Character = Cast<ARogue10mCharacter>(TryGetPawnOwner());
	auto* Visual = Character ? Character->FindComponentByClass<URogue10mWeaponVisualComponent>() : nullptr;
	TArray<FTransform> PreviousPose;
	TBitArray<> PreviousBones;
	if (Visual && EquipmentBlendSeconds > UE_SMALL_NUMBER)
	{
		Visual->ConsumeEquipmentRetargetPose(*this, PreviousPose, PreviousBones);
	}
	GetProxyOnGameThread<FWeaponRetargetProxy>().SetPreviousPose(MoveTemp(PreviousPose), MoveTemp(PreviousBones), EquipmentBlendSeconds);
}

FAnimInstanceProxy* URogue10mWeaponRetargetAnimInstance::CreateAnimInstanceProxy()
{
	return new FWeaponRetargetProxy(this);
}

void URogue10mWeaponRetargetAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}
