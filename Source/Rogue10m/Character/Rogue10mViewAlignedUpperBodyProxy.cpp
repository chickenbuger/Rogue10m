#include "Rogue10mViewAlignedUpperBodyProxy.h"

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimMontage.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mWeightedFirstPersonSettings.h"

namespace
{
FQuat ComponentRotation(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
{
	FQuat Rotation = Pose[Bone].GetRotation();
	for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent != INDEX_NONE; Parent = Pose.GetParentBoneIndex(Parent))
	{
		Rotation = Pose[Parent].GetRotation() * Rotation;
	}
	return Rotation.GetNormalized();
}

FTransform BoneComponentTransform(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
{
	FTransform Transform = Pose[Bone];
	for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent != INDEX_NONE; Parent = Pose.GetParentBoneIndex(Parent))
	{
		Transform *= Pose[Parent];
	}
	return Transform;
}

FQuat ParentRotation(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
{
	const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
	return Parent == INDEX_NONE ? FQuat::Identity : ComponentRotation(Pose, Parent);
}

void RotateBone(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& Delta)
{
	if (!Pose.IsValidIndex(Bone)) { return; }
	const FQuat Parent = ParentRotation(Pose, Bone);
	Pose[Bone].SetRotation((Parent.Inverse() * Delta * Parent * Pose[Bone].GetRotation()).GetNormalized());
}

void RestoreComponentRotation(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& Original)
{
	if (Pose.IsValidIndex(Bone))
	{
		Pose[Bone].SetRotation((ParentRotation(Pose, Bone).Inverse() * Original).GetNormalized());
	}
}

FQuat ReferenceComponentRotation(const FBoneContainer& Bones, FCompactPoseBoneIndex Bone)
{
	FQuat Rotation = Bones.GetRefPoseTransform(Bone).GetRotation();
	for (FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone); Parent != INDEX_NONE; Parent = Bones.GetParentBoneIndex(Parent))
	{
		Rotation = Bones.GetRefPoseTransform(Parent).GetRotation() * Rotation;
	}
	return Rotation.GetNormalized();
}

FQuat BoundedResidual(const FQuat& Rotation, float Weight, float LimitDegrees)
{
	const FQuat Normal = Rotation.GetNormalized();
	const double Angle = Normal.AngularDistance(FQuat::Identity);
	const double Limit = FMath::DegreesToRadians(static_cast<double>(LimitDegrees));
	const double Fraction = Angle > UE_SMALL_NUMBER ? FMath::Min(static_cast<double>(Weight), Limit / Angle) : 0.0;
	return FQuat::Slerp(FQuat::Identity, Normal, Fraction).GetNormalized();
}
}

FRogue10mViewAlignedUpperBodyProxy::FRogue10mViewAlignedUpperBodyProxy(UAnimInstance* Instance)
	: FAnimInstanceProxy(Instance)
{
}

void FRogue10mViewAlignedUpperBodyProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	AimPitch = AimYaw = 0.0f;
	BaselineAimPitch = BaselineAimYaw = 0.0f;
	WeightedArmPitchDegrees[0] = WeightedArmPitchDegrees[1] = 0.0f;
	EvaluatedWeightedHandPitch = FVector2D::ZeroVector;
	SkippedWeightedAttackInstanceId = INDEX_NONE;
	bApplyWeightedHandMotion = false;
	bWeightedHandContextValid = bUseWeightedAimFollow = bWeightedHandMotionEvaluated = false;
	bApplyAim = bApplyMouseHead = bMouseHeadEvaluated = bBoneCacheValid = false;
	SmoothedHeadResidual = SmoothedNeckResidual = FQuat::Identity;
	HeadAimInMesh = HeadWorldAim = FQuat::Identity;
	HeadSourceMeshRotation = FQuat::Identity;
	EvaluatedHeadWorldAim = EvaluatedHeadSourceRotation = FQuat::Identity;
}

void FRogue10mViewAlignedUpperBodyProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	auto* Instance = Cast<URogue10mFighterCombatStanceAnimInstance>(InAnimInstance);
	const auto* Character = Instance ? Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner()) : nullptr;
	const AController* Controller = Character ? Character->GetController() : nullptr;
	const auto* PlayerController = Cast<ARogue10mPlayerController>(Controller);
	const USkeletalMeshComponent* SourceMesh = Instance ? Instance->GetSkelMeshComponent() : nullptr;
	const bool bContextValid = Instance && Character && Controller && SourceMesh
		&& !Character->IsDead() && !(PlayerController && PlayerController->IsAnyBlockingWindowVisible());
	bApplyMouseHead = false;
	bApplyAim = false;
	bApplyWeightedHandMotion = false;
	bWeightedHandContextValid = false;
	bUseWeightedAimFollow = false;
	WeightedArmPitchDegrees[0] = WeightedArmPitchDegrees[1] = 0.0f;
	if (!bContextValid)
	{
		AimPitch = AimYaw = 0.0f;
		BaselineAimPitch = BaselineAimYaw = 0.0f;
		SmoothedHeadResidual = SmoothedNeckResidual = FQuat::Identity;
		if (Instance) { Instance->UpperBodyAimPitch = Instance->UpperBodyAimYaw = 0.0f; }
		return;
	}

	const FRotator Control = Controller->GetControlRotation();
	const FRotator Actor = Character->GetActorRotation();
	const FQuat MeshRotation = SourceMesh->GetComponentQuat();
	if (Control.ContainsNaN() || Actor.ContainsNaN() || MeshRotation.ContainsNaN()
		|| !FMath::IsFinite(DeltaSeconds))
	{
		AimPitch = AimYaw = Instance->UpperBodyAimPitch = Instance->UpperBodyAimYaw = 0.0f;
		BaselineAimPitch = BaselineAimYaw = 0.0f;
		return;
	}
	ActorInMesh = (MeshRotation.Inverse() * Actor.Quaternion()).GetNormalized();
	bWeightedHandContextValid = true;
	CaptureWeightedHandMotion(Character, Instance, DeltaSeconds);
	if (Instance->bEnableMouseDrivenHead)
	{
		CaptureHeadMotionSettings(Character, DeltaSeconds);
		HeadWorldAim = (Control.Quaternion() * Actor.Quaternion().Inverse()).GetNormalized();
		HeadSourceMeshRotation = MeshRotation;
		HeadAimInMesh = (MeshRotation.Inverse() * HeadWorldAim * MeshRotation).GetNormalized();
		bApplyMouseHead = true;
	}
	else
	{
		SmoothedHeadResidual = SmoothedNeckResidual = FQuat::Identity;
	}
	// Dodge may retain its authored body pose, but the head always remains mouse driven.
	if (!Instance->bEnableViewAlignedUpperBody || Instance->IsViewAimDodgeActive())
	{
		AimPitch = AimYaw = Instance->UpperBodyAimPitch = Instance->UpperBodyAimYaw = 0.0f;
		BaselineAimPitch = BaselineAimYaw = 0.0f;
		return;
	}
	if (!FMath::IsFinite(Instance->UpperBodyAimMaxPitch) || !FMath::IsFinite(Instance->UpperBodyAimMaxYaw)
		|| !FMath::IsFinite(Instance->UpperBodyAimTorsoWeight) || !FMath::IsFinite(Instance->UpperBodyAimInterpolationSpeed))
	{
		AimPitch = AimYaw = Instance->UpperBodyAimPitch = Instance->UpperBodyAimYaw = 0.0f;
		BaselineAimPitch = BaselineAimYaw = 0.0f;
		return;
	}
	const float PitchLimit = FMath::Clamp(Instance->UpperBodyAimMaxPitch, 0.0f, 85.0f);
	const float YawLimit = FMath::Clamp(Instance->UpperBodyAimMaxYaw, 0.0f, 80.0f);
	const float DesiredPitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch - Actor.Pitch), -PitchLimit, PitchLimit);
	const float DesiredYaw = FMath::Clamp(FRotator::NormalizeAxis(Control.Yaw - Actor.Yaw), -YawLimit, YawLimit);
	const float BaselineSpeed = FMath::Max(0.0f, Instance->UpperBodyAimInterpolationSpeed);
	const float BaselineFollow = BaselineSpeed > 0.0f ? 1.0f - FMath::Exp(-BaselineSpeed * FMath::Max(0.0f, DeltaSeconds)) : 1.0f;
	BaselineAimPitch = FMath::Clamp(FMath::Lerp(BaselineAimPitch, DesiredPitch, BaselineFollow), -PitchLimit, PitchLimit);
	BaselineAimYaw = FMath::Clamp(FMath::Lerp(BaselineAimYaw, DesiredYaw, BaselineFollow), -YawLimit, YawLimit);
	const auto* Appearance = Character->GetAppearanceCameraComponent();
	const bool bWeightedAim = bUseWeightedAimFollow && Appearance && Appearance->IsWeightedFirstPersonActive();
	const FRogue10mWeightedFirstPersonSettings WeightedSettings = bWeightedAim
		? Appearance->GetWeightedFirstPersonSettings() : FRogue10mWeightedFirstPersonSettings();
	const float Speed = bWeightedAim
		? FMath::Lerp(BaselineSpeed, WeightedSettings.UpperBodyFollowSpeed, WeightedSettings.HandMotionScale) : BaselineSpeed;
	const float Follow = Speed > 0.0f ? 1.0f - FMath::Exp(-Speed * FMath::Max(0.0f, DeltaSeconds)) : 1.0f;
	AimPitch = bWeightedAim ? FMath::Clamp(FMath::Lerp(AimPitch, DesiredPitch, Follow), -PitchLimit, PitchLimit) : BaselineAimPitch;
	AimYaw = bWeightedAim ? FMath::Clamp(FMath::Lerp(AimYaw, DesiredYaw, Follow), -YawLimit, YawLimit) : BaselineAimYaw;
	TorsoWeight = FMath::Clamp(Instance->UpperBodyAimTorsoWeight, 0.0f, 1.0f);
	Instance->UpperBodyAimPitch = AimPitch;
	Instance->UpperBodyAimYaw = AimYaw;
	bApplyAim = !FMath::IsNearlyZero(AimPitch, UE_SMALL_NUMBER) || !FMath::IsNearlyZero(AimYaw, UE_SMALL_NUMBER)
		|| !FMath::IsNearlyZero(BaselineAimPitch, UE_SMALL_NUMBER) || !FMath::IsNearlyZero(BaselineAimYaw, UE_SMALL_NUMBER);
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureWeightedHandMotion(
	const ARogue10mCharacter* Character, const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds)
{
	// Game-thread snapshot only. No setting, pawn, montage or CVar is read by the pose worker.
	bApplyWeightedHandMotion = false;
	bUseWeightedAimFollow = false;
	WeightedArmPitchDegrees[0] = WeightedArmPitchDegrees[1] = 0.0f;
	const auto* Appearance = Character ? Character->GetAppearanceCameraComponent() : nullptr;
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!bWeightedHandContextValid || !Instance || !Appearance || !Appearance->IsWeightedFirstPersonActive() || !Movement
		|| !Movement->IsMovingOnGround() || Character->IsDead() || Instance->IsViewAimDodgeActive()
		|| Instance->GetSkelMeshComponent() != Character->GetAnimationPlaybackMesh()
		|| (Controller && Controller->IsAnyBlockingWindowVisible()) || (Combat && Combat->IsAttackChargeHeld())) { return; }
	const FRogue10mWeightedFirstPersonSettings Settings = Appearance->GetWeightedFirstPersonSettings();
	if (Settings.HandMotionScale <= UE_SMALL_NUMBER) { return; }
	const URogue10mAttackSkillData* AcceptedSkill = Combat ? Combat->GetLastAcceptedAttackSkill() : nullptr;
	const UAnimMontage* AcceptedMontage = AcceptedSkill ? AcceptedSkill->AttackMontage.Get() : nullptr;
	const FAnimMontageInstance* AttackInstance = nullptr;
	int32 ContributingMontages = 0;
	for (const FAnimMontageInstance* Montage : Instance->MontageInstances)
	{
		if (Montage && Montage->Montage && (Montage->IsActive() || Montage->GetWeight() > UE_SMALL_NUMBER))
		{
			++ContributingMontages;
			if (Montage->Montage == AcceptedMontage) { AttackInstance = Montage; }
		}
	}
	if (ContributingMontages > 0)
	{
		if (AttackInstance && SkippedWeightedAttackInstanceId == AttackInstance->GetInstanceID()) { return; }
		const USkeletalMeshComponent* SourceMesh = Instance->GetSkelMeshComponent();
		const USkeletalMeshComponent* VisibleMesh = Character->GetMesh();
		// The contact budget covers regular source updates and at most one retarget frame.
		// Sparse/paused evaluation or a hitch cannot safely reuse a prior nonzero arm pose.
		constexpr float MaximumSupportedDeltaSeconds = 1.0f / 20.0f;
		if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || DeltaSeconds > MaximumSupportedDeltaSeconds
			|| !SourceMesh || !VisibleMesh || SourceMesh->bEnableUpdateRateOptimizations || VisibleMesh->bEnableUpdateRateOptimizations
			|| SourceMesh->bPauseAnims || VisibleMesh->bPauseAnims
			|| !SourceMesh->IsComponentTickEnabled() || !VisibleMesh->IsComponentTickEnabled()
			|| SourceMesh->GetComponentTickInterval() > 0.0f || VisibleMesh->GetComponentTickInterval() > 0.0f)
		{
			if (AttackInstance) { SkippedWeightedAttackInstanceId = AttackInstance->GetInstanceID(); }
			return;
		}
		// Only a single, forward-playing ground melee contact has a proven montage-time
		// contract. Other attacks, hit reactions and blend-out overlaps keep their original pose.
		if (ContributingMontages != 1 || !AcceptedSkill || !AcceptedMontage || !AttackInstance
			|| Instance->GetCurrentActiveMontage() != AcceptedMontage || !AttackInstance->IsActive() || !AttackInstance->IsPlaying()
			|| !FMath::IsFinite(AttackInstance->GetPlayRate()) || AttackInstance->GetPlayRate() <= 0.0f
			|| !FMath::IsNearlyEqual(Instance->GetSkelMeshComponent()->GlobalAnimRateScale, 1.0f)
			|| !FMath::IsNearlyEqual(AcceptedMontage->RateScale, 1.0f)
			|| AcceptedMontage->TimeStretchCurve.IsValid() || AcceptedMontage->HasRootMotion()
			|| AcceptedMontage->CompositeSections.Num() > 1
			|| (AcceptedMontage->CompositeSections.Num() == 1 && !AcceptedMontage->CompositeSections[0].NextSectionName.IsNone())
			|| AcceptedSkill->HitMode != ERogue10mAttackHitMode::Single
			|| AcceptedSkill->AttackShape == ERogue10mAttackShape::Projectile
			|| AcceptedSkill->InputSlot == ERogue10mAttackInputSlot::JumpPrimary
			|| AcceptedSkill->InputSlot == ERogue10mAttackInputSlot::JumpSpecial) { return; }
		const float Length = AcceptedMontage->GetPlayLength();
		const float Contact = AcceptedSkill->HitStartDelaySeconds;
		const float Position = AttackInstance->GetPosition();
		if (!FMath::IsFinite(Length) || !FMath::IsFinite(Contact) || !FMath::IsFinite(Position)
			|| Contact <= 0.0f || Contact >= Length || Position < 0.0f || Position > Length) { return; }
		// Combat divides this montage-time contact by the exact rate supplied to Montage_Play.
		// Reserve two supported frame intervals before/after contact, scaled into montage
		// seconds. The maximum-interval floor protects a short frame followed by a bounded
		// longer frame, including the previous source sample consumed by retargeting.
		const float DeltaClip = DeltaSeconds * AttackInstance->GetPlayRate();
		const float SupportedDeltaClip = MaximumSupportedDeltaSeconds * AttackInstance->GetPlayRate();
		if (!FMath::IsFinite(DeltaClip) || !FMath::IsFinite(SupportedDeltaClip)) { return; }
		const float ContactMargin = FMath::Max(1.0f / 60.0f, 2.0f * FMath::Max(DeltaClip, SupportedDeltaClip));
		float Pitch = 0.0f;
		if (Position < Contact - ContactMargin)
		{
			const float Phase = FMath::Clamp(Position / (Contact - ContactMargin), 0.0f, 1.0f);
			Pitch = -FMath::Min(Settings.AttackWindupDegrees, 3.0f) * FMath::Sin(PI * Phase);
		}
		else if (Position > Contact + ContactMargin && Length > Contact + ContactMargin)
		{
			const float Phase = FMath::Clamp((Position - Contact - ContactMargin) / (Length - Contact - ContactMargin), 0.0f, 1.0f);
			Pitch = FMath::Min(Settings.AttackRecoveryDegrees, 2.0f) * FMath::Sin(PI * Phase);
		}
		WeightedArmPitchDegrees[0] = WeightedArmPitchDegrees[1] = Pitch * Settings.HandMotionScale;
		bApplyWeightedHandMotion = !FMath::IsNearlyZero(Pitch * Settings.HandMotionScale, UE_SMALL_NUMBER);
		return;
	}
	if (const auto* Basic = Character->GetBasicBrawlerComponent())
	{
		const FRogue10mBasicBrawlerSnapshot Snapshot = Basic->GetSnapshot();
		if (Snapshot.bActive && Snapshot.Phase != ERogue10mBasicBrawlerPhase::Idle) { return; }
	}
	const float GroundSpeed = Movement->Velocity.Size2D();
	const float Age = Character->GetGameTimeSinceCreation();
	if (!FMath::IsFinite(GroundSpeed) || !FMath::IsFinite(Age) || !FMath::IsFinite(Instance->FighterCombatStanceAlpha)) { return; }
	const float IdleWeight = 1.0f - FMath::Clamp(Instance->FighterCombatStanceAlpha, 0.0f, 1.0f);
	const float Drop = -Settings.IdleArmDropDegrees * IdleWeight;
	const float MovingWeight = FMath::Clamp(GroundSpeed / 300.0f, 0.0f, 1.0f);
	const float Sway = Settings.MovementHandSwayDegrees * MovingWeight * FMath::Sin(Age * 7.0f);
	WeightedArmPitchDegrees[0] = (Drop + Sway) * Settings.HandMotionScale;
	WeightedArmPitchDegrees[1] = (Drop - Sway) * Settings.HandMotionScale;
	bUseWeightedAimFollow = true;
	bApplyWeightedHandMotion = !FMath::IsNearlyZero(WeightedArmPitchDegrees[0], UE_SMALL_NUMBER)
		|| !FMath::IsNearlyZero(WeightedArmPitchDegrees[1], UE_SMALL_NUMBER);
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureHeadMotionSettings(const ARogue10mCharacter* Character, float DeltaSeconds)
{
	const URogue10mAppearanceCameraComponent* Camera = Character ? Character->GetAppearanceCameraComponent() : nullptr;
	const FRogue10mHeadMotionSettings Settings = Camera ? Camera->GetHeadMotionSettings() : FRogue10mHeadMotionSettings();
	HeadRotationWeight = Settings.RotationWeight;
	HeadRotationLimitDegrees = Settings.MaxRotationDegrees;
	HeadNeckAimWeight = Settings.NeckAimWeight;
	HeadResidualFollow = Settings.InterpolationSpeed > 0.0f
		? 1.0f - FMath::Exp(-Settings.InterpolationSpeed * FMath::Max(0.0f, DeltaSeconds)) : 1.0f;
}

void FRogue10mViewAlignedUpperBodyProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);
	auto* Instance = Cast<URogue10mFighterCombatStanceAnimInstance>(InAnimInstance);
	const auto* Character = Instance ? Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner()) : nullptr;
	// Montage advance can start/end a contributing pose after PreUpdate. Recheck before evaluation.
	CaptureWeightedHandMotion(Character, Instance, GetDeltaSeconds());
	if (!bUseWeightedAimFollow)
	{
		// Restore the continuously tracked baseline immediately when disabled or an action starts.
		AimPitch = BaselineAimPitch; AimYaw = BaselineAimYaw;
		bApplyAim = bApplyAim && (!FMath::IsNearlyZero(AimPitch, UE_SMALL_NUMBER) || !FMath::IsNearlyZero(AimYaw, UE_SMALL_NUMBER));
		if (Instance) { Instance->UpperBodyAimPitch = AimPitch; Instance->UpperBodyAimYaw = AimYaw; }
	}
	if (!bApplyMouseHead) { return; }
	if (!Instance || !Instance->bEnableMouseDrivenHead || !Character)
	{
		bApplyMouseHead = false;
		return;
	}
	// PreUpdate runs before Montage_Advance. Its last attack blend-out frame can
	// become locomotion by evaluation, so capture the strict budget after advance.
	// This engine hook is on the game thread; aim/basis remain their paired snapshot.
	CaptureHeadMotionSettings(Character, GetDeltaSeconds());
}

bool FRogue10mViewAlignedUpperBodyProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	// Linked layers and cached graph branches use the engine's ordinary path. Only the
	// final main graph is aimed, after its slot / moving upper-body choice / FootIK.
	if (!InRootNode || InRootNode != GetRootNode()) { return false; }
	EvaluateAnimationNode_WithRoot(Output, InRootNode);
	bMouseHeadEvaluated = false;
	bWeightedHandMotionEvaluated = false;
	if (bApplyAim) { ApplyViewAim(Output.Pose); }
	if (bApplyWeightedHandMotion) { ApplyWeightedHandMotion(Output.Pose); }
	if (bApplyMouseHead) { ApplyMouseDrivenHead(Output.Pose); }
	return true;
}

void FRogue10mViewAlignedUpperBodyProxy::PostEvaluate(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::PostEvaluate(InAnimInstance);
	if (auto* Instance = Cast<URogue10mFighterCombatStanceAnimInstance>(InAnimInstance))
	{
		// PostEvaluate is on the game thread. No UObject is read or written by the pose worker.
		Instance->bMouseDrivenHeadApplied = bMouseHeadEvaluated;
		Instance->bWeightedHandMotionApplied = bWeightedHandMotionEvaluated;
		Instance->WeightedHandPitch = bWeightedHandMotionEvaluated
			? EvaluatedWeightedHandPitch : FVector2D::ZeroVector;
		// A later PreUpdate can prepare another aim without evaluating a new pose.
		// Publish only the basis actually consumed when the head pose was written.
		Instance->MouseDrivenHeadWorldAim = bMouseHeadEvaluated ? EvaluatedHeadWorldAim : FQuat::Identity;
		Instance->MouseDrivenHeadSourceRotation = bMouseHeadEvaluated ? EvaluatedHeadSourceRotation : FQuat::Identity;
	}
}

void FRogue10mViewAlignedUpperBodyProxy::RebuildBoneCache(const FBoneContainer& Bones)
{
	auto Find = [&Bones](const TCHAR* Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
			: Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	Spine[0] = Find(TEXT("spine_01")); Spine[1] = Find(TEXT("spine_02")); Spine[2] = Find(TEXT("spine_03"));
	UpperArm[0] = Find(TEXT("upperarm_l")); UpperArm[1] = Find(TEXT("upperarm_r"));
	Neck = Find(TEXT("neck_01")); Head = Find(TEXT("head"));
	Hand[0] = Find(TEXT("hand_l")); Hand[1] = Find(TEXT("hand_r"));
	IKHand[0] = Find(TEXT("ik_hand_gun")); IKHand[1] = Find(TEXT("ik_hand_l")); IKHand[2] = Find(TEXT("ik_hand_r"));
	Root = Find(TEXT("root")); IKHandRoot = Find(TEXT("ik_hand_root"));
	ReferenceHeadRotation = Head != INDEX_NONE ? ReferenceComponentRotation(Bones, Head) : FQuat::Identity;
	ReferenceNeckRotation = Neck != INDEX_NONE ? ReferenceComponentRotation(Bones, Neck) : FQuat::Identity;
	BoneCacheSerial = Bones.GetSerialNumber();
	bBoneCacheValid = true;
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyMouseDrivenHead(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	if (!Pose.IsValidIndex(Head)) { return; }
	const FQuat RawHead = (ComponentRotation(Pose, Head) * ReferenceHeadRotation.Inverse()).GetNormalized();
	const FQuat DesiredHead = BoundedResidual(RawHead, HeadRotationWeight, HeadRotationLimitDegrees);
	SmoothedHeadResidual = BoundedResidual(FQuat::Slerp(SmoothedHeadResidual, DesiredHead, HeadResidualFollow), 1.0f, HeadRotationLimitDegrees);
	if (Pose.IsValidIndex(Neck))
	{
		const FQuat RawNeck = (ComponentRotation(Pose, Neck) * ReferenceNeckRotation.Inverse()).GetNormalized();
		const FQuat DesiredNeck = BoundedResidual(RawNeck, HeadRotationWeight, HeadRotationLimitDegrees);
		SmoothedNeckResidual = BoundedResidual(FQuat::Slerp(SmoothedNeckResidual, DesiredNeck, HeadResidualFollow), 1.0f, HeadRotationLimitDegrees);
		// Sharing the mouse aim avoids placing the entire vertical bend on the head joint.
		const FQuat NeckAim = FQuat::Slerp(FQuat::Identity, HeadAimInMesh, HeadNeckAimWeight);
		RestoreComponentRotation(Pose, Neck, NeckAim * SmoothedNeckResidual * ReferenceNeckRotation);
	}
	// Replace the authored head orientation rather than adding mouse aim to a spinning head.
	// Translation, scale and the entire lower-body hierarchy retain the evaluated animation.
	RestoreComponentRotation(Pose, Head, HeadAimInMesh * SmoothedHeadResidual * ReferenceHeadRotation);
	EvaluatedHeadWorldAim = HeadWorldAim;
	EvaluatedHeadSourceRotation = HeadSourceMeshRotation;
	bMouseHeadEvaluated = true;
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyViewAim(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	// This source rig is Manny. At an LOD with an incomplete aiming chain, preserve
	// the authored pose rather than bending a partial chain or losing head compensation.
	if (!Pose.IsValidIndex(Spine[0]) || !Pose.IsValidIndex(Spine[1]) || !Pose.IsValidIndex(Spine[2])
		|| !Pose.IsValidIndex(UpperArm[0]) || !Pose.IsValidIndex(UpperArm[1]) || !Pose.IsValidIndex(Head)) { return; }
	const FQuat OriginalHead = ComponentRotation(Pose, Head);
	const FQuat OriginalNeck = Pose.IsValidIndex(Neck) ? ComponentRotation(Pose, Neck) : FQuat::Identity;
	const FQuat Aim = (ActorInMesh * FRotator(AimPitch, AimYaw, 0.0f).Quaternion() * ActorInMesh.Inverse()).GetNormalized();
	// Optional follow changes only the shoulder response; the spine keeps the original aim.
	const FQuat BaselineAim = (ActorInMesh * FRotator(BaselineAimPitch, BaselineAimYaw, 0.0f).Quaternion() * ActorInMesh.Inverse()).GetNormalized();
	const float SpineWeights[3] = { 0.20f, 0.32f, 0.48f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		RotateBone(Pose, Spine[Index], FQuat::Slerp(FQuat::Identity, BaselineAim, TorsoWeight * SpineWeights[Index]));
	}
	const FQuat ShoulderAim = FQuat::Slerp(FQuat::Identity, Aim, 1.0f - TorsoWeight);
	RotateBone(Pose, UpperArm[0], ShoulderAim);
	RotateBone(Pose, UpperArm[1], ShoulderAim);
	RestoreComponentRotation(Pose, Neck, OriginalNeck);
	RestoreComponentRotation(Pose, Head, OriginalHead);
	SyncHandIKHelpers(Pose);
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyWeightedHandMotion(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	if (!Pose.IsValidIndex(UpperArm[0]) || !Pose.IsValidIndex(UpperArm[1])
		|| !Pose.IsValidIndex(Hand[0]) || !Pose.IsValidIndex(Hand[1])
		|| !Pose.IsValidIndex(Root) || !Pose.IsValidIndex(IKHandRoot)) { return; }
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Pose.IsValidIndex(IKHand[Index])) { return; }
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(IKHand[Index]); Parent != INDEX_NONE; Parent = Pose.GetParentBoneIndex(Parent))
		{
			if (Parent != Root && Parent != IKHandRoot && Parent != IKHand[0]) { return; }
		}
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FQuat Pitch = (ActorInMesh * FRotator(WeightedArmPitchDegrees[Index], 0.0f, 0.0f).Quaternion() * ActorInMesh.Inverse()).GetNormalized();
		RotateBone(Pose, UpperArm[Index], Pitch);
	}
	// Render bones keep local translation/scale; head, spine, root and feet are untouched.
	SyncHandIKHelpers(Pose);
	EvaluatedWeightedHandPitch = FVector2D(WeightedArmPitchDegrees[0], WeightedArmPitchDegrees[1]);
	bWeightedHandMotionEvaluated = true;
}

void FRogue10mViewAlignedUpperBodyProxy::SyncHandIKHelpers(FCompactPose& Pose)
{
	// IK helpers live under root, outside the render arm hierarchy. Keep them on
	// the evaluated FK hands so retargeting never pulls the aimed wrists back.
	// Only helper T/R follow the hands; every render bone retains local T/S.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FCompactPoseBoneIndex Target = Hand[Index == 1 ? 0 : 1];
		const FCompactPoseBoneIndex IK = IKHand[Index];
		if (!Pose.IsValidIndex(Target) || !Pose.IsValidIndex(IK)) { continue; }
		// Never write a helper unexpectedly parented inside the render body. Manny's
		// IK chain contains only root / ik_hand_root / ik_hand_gun ancestors.
		bool bSafeHelper = true;
		for (FCompactPoseBoneIndex Ancestor = Pose.GetParentBoneIndex(IK); Ancestor != INDEX_NONE; Ancestor = Pose.GetParentBoneIndex(Ancestor))
		{
			if (Ancestor != Root && Ancestor != IKHandRoot && Ancestor != IKHand[0]) { bSafeHelper = false; break; }
		}
		if (!bSafeHelper) { continue; }
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(IK);
		const FTransform ParentTransform = Parent == INDEX_NONE ? FTransform::Identity : BoneComponentTransform(Pose, Parent);
		const FTransform Relative = BoneComponentTransform(Pose, Target).GetRelativeTransform(ParentTransform);
		Pose[IK].SetTranslation(Relative.GetTranslation());
		Pose[IK].SetRotation(Relative.GetRotation().GetNormalized());
	}
}
