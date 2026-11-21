// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mWeaponRetargetAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "BoneContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mWeaponVisualComponent.h"

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
		if (PreviousLocalPose.IsEmpty()) { return; }
		const auto* Character = Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner());
		const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		Elapsed += FMath::Max(0.0f, DeltaSeconds);
		if (Elapsed >= Duration || !Movement || !Movement->IsMovingOnGround() || Character->IsDodging() || Character->IsDead())
		{
			PreviousLocalPose.Reset();
			return;
		}
		// Preserve current foot motion if an equip takes place during a stride.
		OldLegWeight = FMath::GetMappedRangeValueClamped(FVector2f(10.0f, 120.0f), FVector2f(1.0f, 0.0f), Movement->Velocity.Size2D());
	}

	bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* Root) override
	{
		if (!Root || Root != GetRootNode()) { return false; }
		EvaluateAnimationNode_WithRoot(Output, Root);
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
	TArray<FTransform> PreviousLocalPose;
	TBitArray<> PreviousBoneMask;
	float Duration = 0.0f;
	float Elapsed = 0.0f;
	float OldLegWeight = 1.0f;
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
