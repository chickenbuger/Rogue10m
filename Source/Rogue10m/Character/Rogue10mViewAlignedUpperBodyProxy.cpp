#include "Rogue10mViewAlignedUpperBodyProxy.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#endif
#include "Rogue10mCharacter.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mWeightedFirstPersonSettings.h"

namespace
{
constexpr float GroundedDefeatBlendSeconds = .22f;
constexpr float EarthSplitterFadeRate = 10.0f;
constexpr float EarthSplitterMovingLowerBodyFadeRate = 15.0f;
constexpr double EarthSplitterClearanceFadeMargin = 39.0;

#if WITH_EDITOR
TAutoConsoleVariable<int32> CVarGreatSwordCapstoneBrace(TEXT("a.Rogue10m.GreatSwordCapstoneBrace"), 1,
	TEXT("Enable the visual-only capstone brace for editor pose comparisons."));
TAutoConsoleVariable<int32> CVarMovingWeaponLocomotion(TEXT("a.Rogue10m.MovingWeaponLocomotion"), 1,
	TEXT("Enable the grounded pose handoff when equipping or removing a sword while moving."));
TAutoConsoleVariable<int32> CVarEarthSplitterPose(TEXT("a.Rogue10m.EarthSplitterPose"), 1,
	TEXT("Enable the EarthSplitter two-palm pose for editor comparisons."));
#endif

constexpr float WeaponLocomotionUpperSeconds = 0.22f;
constexpr float WeaponLocomotionLowerSeconds = 0.16f;

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

bool IsEarthSplitterBladeSpaceClear(const ARogue10mCharacter& Character, const UCharacterMovementComponent& Movement, double SideMargin)
{
	const UWorld* World = Character.GetWorld();
	const auto& Floor = Movement.CurrentFloor;
	if (!World || !Floor.IsWalkableFloor() || Floor.HitResult.ImpactNormal.Z < .9995)
	{
		return false;
	}
	// Reserve the low blade's forward corridor on the current floor. This check is cosmetic;
	// the existing attack still traces its own arc and can hit enemies inside the corridor.
	const FVector Forward = Character.GetActorForwardVector().GetSafeNormal2D();
	const FVector Actor = Character.GetActorLocation();
	const FVector Origin(Actor.X, Actor.Y, Floor.HitResult.ImpactPoint.Z + 68.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EarthSplitterBladeClearance), false, &Character);
	return !World->SweepTestByObjectType(Origin + Forward * 43.0, Origin + Forward * 175.0, Forward.ToOrientationQuat(),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(FVector(9.0, 48.0 + SideMargin, 66.0)), Params);
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
	bApplyRollPose = false;
	bLearnedRollPoseEvaluated = false;
	RollSequence = nullptr;
	RollSequenceLength = 0.0f;
	CapstoneBraceSequence = nullptr;
	CapstoneBraceTime = CapstoneBraceBlend = CapstoneBraceWeight = 0.0f;
	EarthSplitterSequence = nullptr;
	EarthSplitterTime = EarthSplitterWeight = EarthSplitterLowerBodyWeight = 0.0f;
	EarthSplitterMontageAtChargeStart = INDEX_NONE;
	bEarthSplitterChargeContinuous = false;
	bEarthSplitterHoldObserved = false;
	SecondJumpSequence = nullptr;
	SecondJumpAge = -1.0f;
	SecondJumpInterruptionWeight = 1.0f;
	bSecondJumpPoseEvaluated = false;
	UpdateWeaponLocomotionBlend(false, false, 0.0f);
	NonlethalHitSequence = nullptr;
	NonlethalHitAge = NonlethalHitWeight = 0.0f;
	GroundedDefeatStartPose.Reset();
	GroundedDefeatBlendElapsed = 0.0f;
	SmoothedHeadResidual = SmoothedNeckResidual = FQuat::Identity;
	HeadAimInMesh = HeadWorldAim = FQuat::Identity;
	HeadSourceMeshRotation = FQuat::Identity;
	EvaluatedHeadWorldAim = EvaluatedHeadSourceRotation = FQuat::Identity;
}

void FRogue10mViewAlignedUpperBodyProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (GroundedDefeatStartPose.bIsValid && (GroundedDefeatBlendElapsed += FMath::Max(0.0f, DeltaSeconds)) >= GroundedDefeatBlendSeconds)
	{
		GroundedDefeatStartPose.Reset();
	}
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
	CaptureCapstoneBrace(bContextValid ? Character : nullptr, Instance, DeltaSeconds);
	CaptureNonlethalHit(bContextValid ? Character : nullptr, Instance);
	RollSequence = nullptr;
	SecondJumpSequence = nullptr;
	FVector RollDirection;
	bApplyRollPose = Character && SourceMesh && Character->GetRollPresentation(RollProgress, RollDirection);
	if (bContextValid && !bApplyRollPose)
	{
		CaptureSecondJumpPose(*Character, *Instance, *SourceMesh, DeltaSeconds);
	}
	else
	{
		SecondJumpAge = -1.0f;
		SecondJumpInterruptionWeight = 1.0f;
	}
	if (bApplyRollPose)
	{
		const FQuat MeshRotation = SourceMesh->GetComponentQuat();
		const FVector ActorForward = Character->GetActorForwardVector().GetSafeNormal2D();
		const FVector Turn = FVector::CrossProduct(ActorForward, RollDirection);
		RollYawRadians = static_cast<float>(FMath::Atan2(Turn.Z, FVector::DotProduct(ActorForward, RollDirection)));
		RollRight = MeshRotation.UnrotateVector(FVector::CrossProduct(FVector::UpVector, ActorForward));
		RollUp = MeshRotation.UnrotateVector(FVector::UpVector);
		const UAnimSequence* Sequence = Instance->CachedLearnedRoll;
		const USkeletalMesh* Mesh = SourceMesh->GetSkeletalMeshAsset();
		if (Sequence && Mesh && Sequence->GetSkeleton() == Mesh->GetSkeleton()
			&& !Sequence->IsValidAdditive() && !Sequence->HasRootMotion())
		{
			const float Length = Sequence->GetPlayLength();
			if (FMath::IsFinite(Length) && Length > UE_SMALL_NUMBER)
			{
				RollSequence = Sequence;
				RollSequenceLength = Length;
			}
		}
		AimPitch = AimYaw = Instance->UpperBodyAimPitch = Instance->UpperBodyAimYaw = 0.0f;
		return;
	}
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
		// The head turns with horizontal look. The camera and upper-body aim use control pitch independently.
		const float HeadYaw = FRotator::NormalizeAxis(Control.Yaw - Actor.Yaw);
		HeadWorldAim = FRotator(0.0f, HeadYaw, 0.0f).Quaternion();
		HeadSourceMeshRotation = MeshRotation;
		HeadAimInMesh = (MeshRotation.Inverse() * HeadWorldAim * MeshRotation).GetNormalized();
		bApplyMouseHead = true;
	}
	else
	{
		SmoothedHeadResidual = SmoothedNeckResidual = FQuat::Identity;
	}
	// Dodge retains its authored body pose while the head still responds to horizontal look.
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
	// The third-person camera and combat query consume vertical look independently of the pose.
	const float PitchLimit = Character->IsThirdPersonGameplayActive()
		? 0.0f : FMath::Clamp(Instance->UpperBodyAimMaxPitch, 0.0f, 85.0f);
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
	CaptureEarthSplitterPose(Character, Instance, GetDeltaSeconds());
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
	bLearnedRollPoseEvaluated = false;
	bSecondJumpPoseEvaluated = false;
	if (GroundedDefeatStartPose.bIsValid)
	{
		BlendGroundedDefeatStartPose(Output.Pose);
		return true;
	}
	if (bApplyRollPose)
	{
		if (!ApplyAuthoredRoll(Output)) { ApplyRollPose(Output.Pose); }
		return true;
	}
	ApplyWeaponLocomotionBlend(Output.Pose);
	if (CapstoneBraceWeight > UE_SMALL_NUMBER) { ApplyCapstoneBrace(Output); }
	if (EarthSplitterSequence && EarthSplitterWeight > UE_SMALL_NUMBER) { ApplyEarthSplitterPose(Output); }
	if (SecondJumpSequence) { ApplySecondJumpPose(Output); }
	if (NonlethalHitSequence) { ApplyNonlethalHit(Output); }
	if (bApplyAim) { ApplyViewAim(Output.Pose); }
	if (bApplyWeightedHandMotion) { ApplyWeightedHandMotion(Output.Pose); }
	if (bApplyMouseHead) { ApplyMouseDrivenHead(Output.Pose); }
	return true;
}

void FRogue10mViewAlignedUpperBodyProxy::UpdateWeaponLocomotionBlend(bool bEligible, bool bChangedWhileMoving, float DeltaSeconds)
{
#if WITH_EDITOR
	bEligible = bEligible && CVarMovingWeaponLocomotion.GetValueOnGameThread() != 0;
#endif
	bRecordGroundPose = bEligible;
	if (!bEligible)
	{
		LastGroundPose.Reset();
		WeaponLocomotionOffsets.Reset();
		bCaptureOutgoingGroundPose = false;
		return;
	}
	if (bChangedWhileMoving)
	{
		bCaptureOutgoingGroundPose = true;
		WeaponLocomotionElapsed = 0.0f;
	}
	else if (!bCaptureOutgoingGroundPose && !WeaponLocomotionOffsets.IsEmpty())
	{
		WeaponLocomotionElapsed += FMath::Max(0.0f, DeltaSeconds);
		if (WeaponLocomotionElapsed >= WeaponLocomotionUpperSeconds) { WeaponLocomotionOffsets.Reset(); }
	}
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyWeaponLocomotionBlend(FCompactPose& Pose)
{
	if (!bRecordGroundPose) { return; }
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (GroundPoseBoneSerial != Bones.GetSerialNumber() || LastGroundPose.Num() != Pose.GetNumBones())
	{
		LastGroundPose.Reset();
		WeaponLocomotionOffsets.Reset();
		GroundPoseBoneSerial = Bones.GetSerialNumber();
	}
	if (bCaptureOutgoingGroundPose || !WeaponLocomotionOffsets.IsEmpty())
	{
		if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	}
	if (bCaptureOutgoingGroundPose)
	{
		bCaptureOutgoingGroundPose = false;
		WeaponLocomotionOffsets.SetNum(LastGroundPose.Num());
		if (!LastGroundPose.IsEmpty())
		{
			for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
			{
				if (Bone == Root) { continue; }
				FTransform Outgoing = LastGroundPose[Bone.GetInt()];
				if (Root != INDEX_NONE && Bones.GetParentBoneIndex(Bone) == Root)
				{
					// Express the old visible position under the new root without modifying the live root.
					Outgoing = (Outgoing * LastGroundPose[Root.GetInt()]).GetRelativeTransform(Pose[Root]);
				}
				WeaponLocomotionOffsets[Bone.GetInt()] = Outgoing.GetRelativeTransform(Pose[Bone]);
			}
		}
	}
	if (!WeaponLocomotionOffsets.IsEmpty())
	{
		const float OldUpper = 1.0f - FMath::SmoothStep(0.0f, WeaponLocomotionUpperSeconds, WeaponLocomotionElapsed);
		const float OldLower = 1.0f - FMath::SmoothStep(0.0f, WeaponLocomotionLowerSeconds, WeaponLocomotionElapsed);
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			if (Bone == Root) { continue; }
			const bool bUpper = Spine[0] != INDEX_NONE && (Bone == Spine[0] || Bones.BoneIsChildOf(Bone, Spine[0]));
			FTransform Offset = FTransform::Identity;
			Offset.BlendWith(WeaponLocomotionOffsets[Bone.GetInt()], bUpper ? OldUpper : OldLower);
			// Let the destination stride advance with its own velocity while the presentation offset decays.
			Pose[Bone] = Offset * Pose[Bone];
		}
	}
	// Capture the evaluated graph before camera aim and layer-specific corrections.
	LastGroundPose = Pose.GetBones();
}

void FRogue10mViewAlignedUpperBodyProxy::PostEvaluate(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::PostEvaluate(InAnimInstance);
	if (auto* Instance = Cast<URogue10mFighterCombatStanceAnimInstance>(InAnimInstance))
	{
		// PostEvaluate is on the game thread. No UObject is read or written by the pose worker.
		Instance->bMouseDrivenHeadApplied = bMouseHeadEvaluated;
		Instance->bWeightedHandMotionApplied = bWeightedHandMotionEvaluated;
		Instance->bLearnedRollPoseApplied = bLearnedRollPoseEvaluated;
		Instance->bSecondJumpPoseApplied = bSecondJumpPoseEvaluated;
		Instance->WeightedHandPitch = bWeightedHandMotionEvaluated
			? EvaluatedWeightedHandPitch : FVector2D::ZeroVector;
		// A later PreUpdate can prepare another aim without evaluating a new pose.
		// Publish only the basis actually consumed when the head pose was written.
		Instance->MouseDrivenHeadWorldAim = bMouseHeadEvaluated ? EvaluatedHeadWorldAim : FQuat::Identity;
		Instance->MouseDrivenHeadSourceRotation = bMouseHeadEvaluated ? EvaluatedHeadSourceRotation : FQuat::Identity;
	}
}

void FRogue10mViewAlignedUpperBodyProxy::SetGroundedDefeatStartPose(FPoseSnapshot&& Pose)
{
	GroundedDefeatStartPose = MoveTemp(Pose);
	GroundedDefeatBlendElapsed = 0.0f;
}

void FRogue10mViewAlignedUpperBodyProxy::BlendGroundedDefeatStartPose(FCompactPose& Pose) const
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const FReferenceSkeleton& ReferenceSkeleton = Bones.GetReferenceSkeleton();
	const float Weight = 1.0f - FMath::SmoothStep(0.0f, 1.0f, GroundedDefeatBlendElapsed / GroundedDefeatBlendSeconds);
	for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
	{
		const int32 Index = Bones.MakeMeshPoseIndex(Bone).GetInt();
		if (GroundedDefeatStartPose.LocalTransforms.IsValidIndex(Index) && GroundedDefeatStartPose.BoneNames.IsValidIndex(Index)
			&& ReferenceSkeleton.GetBoneName(Index) == GroundedDefeatStartPose.BoneNames[Index])
		{
			Pose[Bone].BlendWith(GroundedDefeatStartPose.LocalTransforms[Index], Weight);
		}
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
	Spine[0] = Find(TEXT("spine_01")); Spine[1] = Find(TEXT("spine_02")); Spine[2] = Find(TEXT("spine_03")); Spine[3] = Find(TEXT("spine_04"));
	UpperArm[0] = Find(TEXT("upperarm_l")); UpperArm[1] = Find(TEXT("upperarm_r"));
	Neck = Find(TEXT("neck_01")); Head = Find(TEXT("head"));
	Hand[0] = Find(TEXT("hand_l")); Hand[1] = Find(TEXT("hand_r"));
	IKHand[0] = Find(TEXT("ik_hand_gun")); IKHand[1] = Find(TEXT("ik_hand_l")); IKHand[2] = Find(TEXT("ik_hand_r"));
	Root = Find(TEXT("root")); IKHandRoot = Find(TEXT("ik_hand_root"));
	Pelvis = Find(TEXT("pelvis"));
	Clavicle[0] = Find(TEXT("clavicle_l")); Clavicle[1] = Find(TEXT("clavicle_r"));
	Forearm[0] = Find(TEXT("lowerarm_l")); Forearm[1] = Find(TEXT("lowerarm_r"));
	Thigh[0] = Find(TEXT("thigh_l")); Thigh[1] = Find(TEXT("thigh_r"));
	Calf[0] = Find(TEXT("calf_l")); Calf[1] = Find(TEXT("calf_r"));
	Foot[0] = Find(TEXT("foot_l")); Foot[1] = Find(TEXT("foot_r"));
	Ball[0] = Find(TEXT("ball_l")); Ball[1] = Find(TEXT("ball_r"));
	ReferenceHeadRotation = Head != INDEX_NONE ? ReferenceComponentRotation(Bones, Head) : FQuat::Identity;
	ReferenceNeckRotation = Neck != INDEX_NONE ? ReferenceComponentRotation(Bones, Neck) : FQuat::Identity;
	BoneCacheSerial = Bones.GetSerialNumber();
	bBoneCacheValid = true;
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureCapstoneBrace(const ARogue10mCharacter* Character,
	const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds)
{
	CapstoneBraceSequence = nullptr;
	CapstoneBraceWeight = 0.0f;
#if WITH_EDITOR
	if (CVarGreatSwordCapstoneBrace.GetValueOnGameThread() == 0)
	{
		CapstoneBraceBlend = CapstoneBraceTime = 0.0f;
		return;
	}
#endif
	const auto* Sequence = Instance ? Instance->CachedCapstoneBrace.Get() : nullptr;
	const auto* Mesh = Instance ? Instance->GetSkelMeshComponent() : nullptr;
	const auto* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement || !Movement->IsMovingOnGround() || Character->IsDodging()
		|| !Character->IsThirdPersonGameplayActive() || Character->GetEquippedWeaponType() != ERogue10mWeaponType::GreatSword
		|| Mesh != Character->GetAnimationPlaybackMesh() || !MeshAsset || !Sequence || !Sequence->IsValidAdditive()
		|| Sequence->GetAdditiveAnimType() != AAT_LocalSpaceBase || Sequence->GetSkeleton() != MeshAsset->GetSkeleton())
	{
		CapstoneBraceBlend = CapstoneBraceTime = 0.0f;
		return;
	}

	const auto* Combat = Character->GetCombatComponent();
	const auto* CharacterAnimation = Character->FindComponentByClass<URogue10mCharacterAnimationComponent>();
	const bool bStationary = (!CharacterAnimation || !CharacterAnimation->HasMovementInput())
		&& Character->GetVelocity().SizeSquared2D() <= FMath::Square(FMath::Max(0.0f, Instance->FighterCombatStationarySpeed));
	const TOptional<float> Hold = bStationary && Combat ? Combat->GetGreatSwordCapstoneHoldSeconds() : TOptional<float>();
	if (Hold.IsSet()) { CapstoneBraceTime = FMath::Clamp(Hold.GetValue(), 0.0f, Sequence->GetPlayLength()); }
	CapstoneBraceBlend = FMath::FInterpConstantTo(CapstoneBraceBlend, Hold.IsSet() ? 1.0f : 0.0f,
		FMath::Max(0.0f, DeltaSeconds), Hold.IsSet() ? 10.0f : 8.0f);
	// The existing release slot progressively replaces the held posture during its own blend-in.
	const float Slot = FMath::Clamp(Instance->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")), 0.0f, 1.0f);
	CapstoneBraceWeight = CapstoneBraceBlend * (1.0f - Slot) * FMath::Clamp(Instance->GreatSwordCapstoneBraceStrength, 0.0f, 1.0f);
	CapstoneBraceSequence = Sequence;
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyCapstoneBrace(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	if (!Output.Pose.IsValidIndex(Spine[0])) { return; }

	FPoseContext Brace(Output, true);
	Brace.ResetToAdditiveIdentity();
	FAnimationPoseData BraceData(Brace);
	CapstoneBraceSequence->GetAnimationPose(BraceData, FAnimExtractContext(CapstoneBraceTime, false));
	const FTransform IdentityDelta(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	for (const FCompactPoseBoneIndex Bone : Brace.Pose.ForEachBoneIndex())
	{
		FCompactPoseBoneIndex Ancestor = Bone;
		while (Ancestor != INDEX_NONE && Ancestor != Spine[0])
		{
			Ancestor = Brace.Pose.GetParentBoneIndex(Ancestor);
		}
		if (Ancestor == INDEX_NONE) { Brace.Pose[Bone] = IdentityDelta; }
		else if (Brace.Pose[Bone].ContainsNaN()) { return; }
	}
	FAnimationPoseData BaseData(Output);
	FAnimationRuntime::AccumulateAdditivePose(BaseData, BraceData, CapstoneBraceWeight, AAT_LocalSpaceBase);
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureEarthSplitterPose(const ARogue10mCharacter* Character,
	const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds)
{
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	const TOptional<float> Hold = Combat ? Combat->GetGreatSwordEarthSplitterHoldSeconds() : TOptional<float>();
	const bool bNewHold = Hold.IsSet() && !bEarthSplitterHoldObserved;
	bEarthSplitterHoldObserved = Hold.IsSet();
	const auto* Source = Instance ? Instance->GetSkelMeshComponent() : nullptr;
	const auto* Mesh = Source ? Source->GetSkeletalMeshAsset() : nullptr;
	const auto Appearance = Character ? Character->GetCharacterAppearance() : FRogue10mCharacterAppearance();
	const bool bFemaleRefit = Appearance.Gender == ERogue10mCharacterGender::Female
		&& (Appearance.Race == ERogue10mCharacterRace::Human || Appearance.Race == ERogue10mCharacterRace::Orc);
	const bool bHumanMale = Appearance.Race == ERogue10mCharacterRace::Human && Appearance.Gender == ERogue10mCharacterGender::Male;
	const auto* Charge = Instance ? (bFemaleRefit ? Instance->CachedEarthSplitterFemaleCharge.Get() : Instance->CachedEarthSplitterCharge.Get()) : nullptr;
	const auto* Release = Instance ? (bFemaleRefit ? Instance->CachedEarthSplitterFemaleRelease.Get() : Instance->CachedEarthSplitterRelease.Get()) : nullptr;
	const auto* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	bool bPresentationEnabled = true;
#if WITH_EDITOR
	bPresentationEnabled = CVarEarthSplitterPose.GetValueOnGameThread() != 0;
#endif
	// The displayed retarget changes the blade axis because the mount uses both palms.
	// Only use the authored pairs on profiles calibrated against that visible grip.
	if (!bPresentationEnabled || (!bHumanMale && !bFemaleRefit) || !Combat || !Controller || Controller->IsAnyBlockingWindowVisible() || Character->IsDead()
		|| !Character->IsThirdPersonGameplayActive() || Character->IsDodging() || Character->GetEquippedWeaponType() != ERogue10mWeaponType::GreatSword
		|| Source != Character->GetAnimationPlaybackMesh() || !Mesh || !Charge || !Release || !Movement || !Movement->IsMovingOnGround()
		|| Charge->GetSkeleton() != Mesh->GetSkeleton() || Release->GetSkeleton() != Mesh->GetSkeleton())
	{
		EarthSplitterSequence = nullptr;
		EarthSplitterWeight = EarthSplitterLowerBodyWeight = 0.0f;
		bEarthSplitterChargeContinuous = false;
		return;
	}
	if (!Hold.IsSet() && EarthSplitterWeight <= UE_SMALL_NUMBER) { return; }
	const auto* Animation = Character->GetCharacterAnimationComponent();
	const bool bStationary = (!Animation || !Animation->HasMovementInput())
		&& Character->GetVelocity().SizeSquared2D() <= FMath::Square(FMath::Max(0.0f, Instance->FighterCombatStationarySpeed));
	if (!IsEarthSplitterBladeSpaceClear(*Character, *Movement, 0.0))
	{
		EarthSplitterSequence = nullptr;
		EarthSplitterWeight = EarthSplitterLowerBodyWeight = 0.0f;
		bEarthSplitterChargeContinuous = false;
		return;
	}

	// A wall being approached by a turn gets a wider warning corridor to release the pose before
	// it reaches the inner clearance. An immediate obstruction still takes precedence over blending.
	const bool bCanUseLowPose = bStationary && IsEarthSplitterBladeSpaceClear(*Character, *Movement, EarthSplitterClearanceFadeMargin);
	const UAnimSequence* Desired = nullptr;
	if (Hold.IsSet())
	{
		const bool bWasContinuous = bEarthSplitterChargeContinuous;
		// A cancelled brace can only restart with a new charge.
		bEarthSplitterChargeContinuous = bCanUseLowPose && (bWasContinuous || (bNewHold && Hold.GetValue() <= .10f));
		if (bEarthSplitterChargeContinuous)
		{
			if (!bWasContinuous)
			{
				const auto* Bound = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial);
				const auto* BoundMontage = Bound ? Bound->AttackMontage.Get() : nullptr;
				const auto* Existing = BoundMontage ? Instance->GetActiveInstanceForMontage(BoundMontage) : nullptr;
				EarthSplitterMontageAtChargeStart = Existing ? Existing->GetInstanceID() : INDEX_NONE;
			}
			Desired = Charge;
			EarthSplitterTime = FMath::Min(Hold.GetValue(), Charge->GetPlayLength());
		}
	}
	else if (bCanUseLowPose && bEarthSplitterChargeContinuous)
	{
		const auto* Accepted = Combat->GetLastAcceptedAttackSkill();
		static const FSoftObjectPath EarthSplitter(TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_EarthSplitter.DA_Hybrid_GreatSword_EarthSplitter"));
		const UAnimMontage* Montage = Accepted && FSoftObjectPath(Accepted) == EarthSplitter ? Accepted->AttackMontage.Get() : nullptr;
		const FAnimMontageInstance* Playback = Montage ? Instance->GetActiveInstanceForMontage(Montage) : nullptr;
		// A newly accepted attack instance proves release even if a frame hitch skipped late hold poses.
		if (Playback && Playback->GetInstanceID() != EarthSplitterMontageAtChargeStart
			&& FMath::IsNearlyEqual(Montage->GetPlayLength(), Release->GetPlayLength(), .001f))
		{
			Desired = Release;
			EarthSplitterTime = FMath::Clamp(Playback->GetPosition(), 0.0f, Release->GetPlayLength());
		}
		else { bEarthSplitterChargeContinuous = false; }
	}
	else { bEarthSplitterChargeContinuous = false; }

	const float FadeTarget = Desired ? 1.0f : 0.0f;
	const float Delta = FMath::Max(0.0f, DeltaSeconds);
	EarthSplitterWeight = FMath::FInterpConstantTo(EarthSplitterWeight, FadeTarget, Delta, EarthSplitterFadeRate);
	// Let the live legs take over sooner during locomotion. Never restore a discarded brace
	// when the player stops again with the same charge still held.
	EarthSplitterLowerBodyWeight = FMath::FInterpConstantTo(EarthSplitterLowerBodyWeight, FadeTarget, Delta,
		bStationary ? EarthSplitterFadeRate : EarthSplitterMovingLowerBodyFadeRate);
	if (Desired) { EarthSplitterSequence = Desired; }
	if (EarthSplitterWeight <= UE_SMALL_NUMBER) { EarthSplitterSequence = nullptr; }
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyEarthSplitterPose(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	FPoseContext Strike(Output);
	FAnimationPoseData StrikeData(Strike);
	EarthSplitterSequence->GetAnimationPose(StrikeData, FAnimExtractContext(EarthSplitterTime, false));
	for (const FCompactPoseBoneIndex Bone : Strike.Pose.ForEachBoneIndex())
	{
		if (Bone == Root || Strike.Pose[Bone].ContainsNaN()) { continue; }
		const bool bUsesLowerBodyWeight = EarthSplitterLowerBodyWeight < EarthSplitterWeight
			&& (!Strike.Pose.IsValidIndex(Spine[0]) || (Bone != Spine[0] && !Bones.BoneIsChildOf(Bone, Spine[0])));
		const float Weight = bUsesLowerBodyWeight ? EarthSplitterLowerBodyWeight : EarthSplitterWeight;
		if (Weight <= UE_SMALL_NUMBER) { continue; }
		FTransform Authored = Strike.Pose[Bone];
		if (Root != INDEX_NONE && Bones.GetParentBoneIndex(Bone) == Root)
		{
			// The live graph can carry a translated root during the charged stance. Keep its root
			// while placing the authored children in the same component space as their Blender rig.
			Authored = (Authored * Strike.Pose[Root]).GetRelativeTransform(Output.Pose[Root]);
		}
		Output.Pose[Bone].BlendWith(Authored, Weight);
	}
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureSecondJumpPose(const ARogue10mCharacter& Character,
	const URogue10mFighterCombatStanceAnimInstance& Instance, const USkeletalMeshComponent& Mesh, float DeltaSeconds)
{
	const auto* Animation = Character.GetCharacterAnimationComponent();
	const auto* Movement = Character.GetCharacterMovement();
	const UAnimSequence* Sequence = Instance.CachedSecondJumpPose;
	const USkeletalMesh* SkeletalMesh = Mesh.GetSkeletalMeshAsset();
	const float Age = Animation ? Animation->GetSecondJumpPresentationAge() : -1.0f;
	if (!Character.IsThirdPersonGameplayActive() || Character.IsDodging() || !Movement || !Movement->IsFalling()
		|| !Sequence || !SkeletalMesh || Sequence->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| Sequence->HasRootMotion() || Sequence->IsValidAdditive()
		|| !FMath::IsFinite(Age) || Age < 0.0f || Age >= Sequence->GetPlayLength())
	{
		SecondJumpAge = -1.0f;
		SecondJumpInterruptionWeight = 1.0f;
		return;
	}

	const auto* Combat = Character.GetCombatComponent();
	const bool bAttack = Instance.IsAnyMontagePlaying() || (Combat && Combat->IsWeaponChangeBlocked());
	if (SecondJumpAge < 0.0f || Age < SecondJumpAge)
	{
		SecondJumpInterruptionWeight = bAttack ? 0.0f : 1.0f;
	}
	else if (bAttack || SecondJumpInterruptionWeight < 1.0f)
	{
		// Yield a pulse already in progress over three 60 Hz frames. Do not resume
		// that pulse if the attack ends before its short authored window expires.
		SecondJumpInterruptionWeight = FMath::Max(0.0f,
			SecondJumpInterruptionWeight - 20.0f * FMath::Clamp(DeltaSeconds, 0.0f, 0.05f));
	}
	SecondJumpAge = Age;
	if (SecondJumpInterruptionWeight > UE_SMALL_NUMBER) { SecondJumpSequence = Sequence; }
}

void FRogue10mViewAlignedUpperBodyProxy::ApplySecondJumpPose(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }

	FPoseContext Neutral(Output);
	Neutral.ResetToRefPose();
	FAnimationPoseData NeutralData(Neutral);
	SecondJumpSequence->GetAnimationPose(NeutralData, FAnimExtractContext(0.0, false));
	FPoseContext Authored(Output);
	Authored.ResetToRefPose();
	FAnimationPoseData AuthoredData(Authored);
	SecondJumpSequence->GetAnimationPose(AuthoredData, FAnimExtractContext(SecondJumpAge, false));

	// Transfer only local leg rotations relative to the clip's first frame.
	// The live fall graph owns the pelvis, root, timing and the upper body.
	for (const FCompactPoseBoneIndex Bone : { Thigh[0], Thigh[1], Calf[0], Calf[1], Foot[0], Foot[1], Ball[0], Ball[1] })
	{
		if (!Output.Pose.IsValidIndex(Bone) || Neutral.Pose[Bone].ContainsNaN() || Authored.Pose[Bone].ContainsNaN()) { continue; }
		const FQuat Delta = (Neutral.Pose[Bone].GetRotation().Inverse() * Authored.Pose[Bone].GetRotation()).GetNormalized();
		const FQuat Applied = FQuat::Slerp(FQuat::Identity, Delta, SecondJumpInterruptionWeight);
		if (Applied.AngularDistance(FQuat::Identity) <= UE_KINDA_SMALL_NUMBER) { continue; }
		Output.Pose[Bone].SetRotation((Output.Pose[Bone].GetRotation() * Applied).GetNormalized());
		bSecondJumpPoseEvaluated = true;
	}
}

void FRogue10mViewAlignedUpperBodyProxy::CaptureNonlethalHit(const ARogue10mCharacter* Character,
	const URogue10mFighterCombatStanceAnimInstance* Instance)
{
	NonlethalHitSequence = nullptr;
	const auto* Source = Instance ? Instance->GetSkelMeshComponent() : nullptr;
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Animation = Character ? Character->GetCharacterAnimationComponent() : nullptr;
	if (!Animation || !Movement || !Movement->IsMovingOnGround() || Character->IsDodging()
		|| !Character->IsThirdPersonGameplayActive() || !Source || Source != Character->GetAnimationPlaybackMesh()) { return; }
	bool bFromRear = false;
	if (!Animation->GetNonlethalHitPresentation(NonlethalHitAge, bFromRear)) { return; }

	const auto* Sequence = bFromRear ? Instance->CachedNonlethalRear.Get() : Instance->CachedNonlethalFront.Get();
	const auto* Mesh = Source->GetSkeletalMeshAsset();
	NonlethalHitWeight = FMath::Clamp(Instance->NonlethalReactionStrength, 0.0f, 1.0f);
	if (Sequence && Mesh && Sequence->GetSkeleton() == Mesh->GetSkeleton()
		&& Sequence->IsValidAdditive() && Sequence->GetAdditiveAnimType() == AAT_LocalSpaceBase
		&& NonlethalHitAge <= Sequence->GetPlayLength() && NonlethalHitWeight > UE_SMALL_NUMBER)
	{
		NonlethalHitSequence = Sequence;
	}
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyNonlethalHit(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	FPoseContext Reaction(Output, true);
	Reaction.ResetToAdditiveIdentity();
	FAnimationPoseData ReactionData(Reaction);
	NonlethalHitSequence->GetAnimationPose(ReactionData, FAnimExtractContext(NonlethalHitAge, false));
	// Apply the Blender-authored spine rotations only. Hands inherit the torso movement;
	// the base pose owns roots, leg contacts, hand articulation, curves and attributes.
	for (const FCompactPoseBoneIndex Bone : Spine)
	{
		if (!Output.Pose.IsValidIndex(Bone)) { continue; }
		const FQuat Delta = Reaction.Pose[Bone].GetRotation();
		if (Delta.ContainsNaN()) { continue; }
		const FQuat Applied = FQuat::Slerp(FQuat::Identity, Delta, NonlethalHitWeight);
		Output.Pose[Bone].SetRotation((Applied * Output.Pose[Bone].GetRotation()).GetNormalized());
	}
}

bool FRogue10mViewAlignedUpperBodyProxy::ApplyAuthoredRoll(FPoseContext& Output)
{
	if (!RollSequence) { return false; }
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	if (!Output.Pose.IsValidIndex(Root) || !Output.Pose.IsValidIndex(Pelvis)) { return false; }

	// Blend the takeoff and recovery into the current graph at either dodge duration.
	const float Weight = FMath::SmoothStep(0.0f, 0.14f, RollProgress)
		* (1.0f - FMath::SmoothStep(0.86f, 1.0f, RollProgress));
	if (Weight <= UE_SMALL_NUMBER) { return true; }

	FPoseContext Authored(Output);
	Authored.ResetToRefPose();
	FAnimationPoseData PoseData(Authored);
	RollSequence->GetAnimationPose(PoseData, FAnimExtractContext(RollProgress * RollSequenceLength, false));
	for (const FCompactPoseBoneIndex Bone : Authored.Pose.ForEachBoneIndex())
	{
		if (Authored.Pose[Bone].ContainsNaN()) { return false; }
	}

	// Rotate the body and the root-level IK chains around the graph root. The graph
	// root itself and its extracted motion remain owned by the existing movement path.
	// Spread a side or back turn across takeoff and recovery, keeping the turn
	// direction settled while the shoulder carries the body.
	const float TurnWeight = FMath::SmoothStep(0.0f, 0.34f, RollProgress)
		* (1.0f - FMath::SmoothStep(0.67f, 1.0f, RollProgress));
	const FQuat RootRotation = Output.Pose[Root].GetRotation();
	const FQuat DirectionInRoot = RootRotation.Inverse() * FQuat(RollUp, RollYawRadians * TurnWeight) * RootRotation;
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		if (Bone == Root) { continue; }
		FTransform Blended;
		Blended.Blend(Output.Pose[Bone], Authored.Pose[Bone], Weight);
		if (Output.Pose.GetParentBoneIndex(Bone) == Root)
		{
			Blended.SetTranslation(DirectionInRoot.RotateVector(Blended.GetTranslation()));
			Blended.SetRotation(DirectionInRoot * Blended.GetRotation());
		}
		Blended.NormalizeRotation();
		Output.Pose[Bone] = Blended;
	}
	bLearnedRollPoseEvaluated = true;
	return true;
}

void FRogue10mViewAlignedUpperBodyProxy::ApplyRollPose(FCompactPose& Pose)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (!bBoneCacheValid || BoneCacheSerial != Bones.GetSerialNumber()) { RebuildBoneCache(Bones); }
	if (!Pose.IsValidIndex(Pelvis)) { return; }

	const float Tuck = FMath::Clamp(FMath::Sin(UE_PI * RollProgress) * 1.3f, 0.0f, 1.0f);
	const float Turn = RollYawRadians * Tuck;
	const float ForwardRotation = 2.0f * UE_PI * FMath::SmoothStep(0.10f, 0.91f, RollProgress);
	const auto Rotation = [Right = RollRight, Tuck](float Degrees)
	{
		return FQuat(Right, FMath::DegreesToRadians(Degrees * Tuck));
	};
	const auto TuckBone = [&Pose, &Bones, Tuck, &Rotation](FCompactPoseBoneIndex Bone, float Degrees)
	{
		if (!Pose.IsValidIndex(Bone)) { return; }
		Pose[Bone].SetRotation(FQuat::Slerp(Pose[Bone].GetRotation(), Bones.GetRefPoseTransform(Bone).GetRotation(), Tuck));
		RotateBone(Pose, Bone, Rotation(Degrees));
	};

	// Reuse the project's locomotion skeleton and evaluated pose at both ends.
	// The roll is posed around the pelvis and has no asset root motion.
	for (FCompactPoseBoneIndex Bone : Spine) { TuckBone(Bone, 12.0f); }
	TuckBone(Neck, 18.0f);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TuckBone(Clavicle[Index], -15.0f);
		TuckBone(UpperArm[Index], -62.0f);
		TuckBone(Forearm[Index], 48.0f);
		TuckBone(Thigh[Index], -76.0f);
		TuckBone(Calf[Index], 112.0f);
	}
	const FQuat Parent = ParentRotation(Pose, Pelvis);
	Pose[Pelvis].AddToTranslation(Parent.UnrotateVector(-RollUp * (39.0f * Tuck)));
	RotateBone(Pose, Pelvis, FQuat(RollUp, Turn) * FQuat(RollRight, ForwardRotation));
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
		// Share the horizontal turn across the neck and head.
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
