// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mFirstPersonPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "Misc/MemStack.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mFistAnimInstance.h"

URogue10mFirstPersonPresentationComponent::URogue10mFirstPersonPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GuardAnimationClass = URogue10mFistAnimInstance::StaticClass();
	GuardFullBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	GuardArmsMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Rogue10m/Character/SK_FirstPersonArms.SK_FirstPersonArms")));
}

void URogue10mFirstPersonPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshPresentation();
}

void URogue10mFirstPersonPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestorePresentation();
	Super::EndPlay(EndPlayReason);
}

void URogue10mFirstPersonPresentationComponent::ApplyInitialFullBodyViewPitchOnce(APlayerController& PlayerController)
{
	if (bInitialLocalViewHandled || !HasBegunPlay() || !PlayerController.IsLocalController()) { return; }
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	if (!Character || PlayerController.GetPawn() != Character || !Character->IsLocallyControlled()) { return; }

	// Consume this pawn's first local view opportunity even when disabled or starting in
	// another presentation. A later equipment/UI/refresh event must never replace user aim.
	bInitialLocalViewHandled = true;
	const bool bAppearanceView = Character->GetAppearanceCameraComponent() && Character->GetAppearanceCameraComponent()->IsAppearanceCameraActive();
 if (!bEnableInitialFullBodyViewPitch || (!IsFullBodyPresentationActive() && !bAppearanceView) || Character->IsDead()
		|| !FMath::IsFinite(InitialFullBodyViewPitch)) { return; }
	FRotator ViewRotation = PlayerController.GetControlRotation();
	if (ViewRotation.ContainsNaN() || !FMath::IsNearlyZero(FRotator::NormalizeAxis(ViewRotation.Pitch), 0.01)) { return; }
	ViewRotation.Pitch = FMath::Clamp(InitialFullBodyViewPitch, -60.0f, 0.0f);
	PlayerController.SetControlRotation(ViewRotation);
}

void URogue10mFirstPersonPresentationComponent::NotifyConfirmedBrawlerHit(float Strength, float Side)
{
 ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
 if (!bPresentationActive || !bEnableFistPresentation || !Character || Character->IsDead()
  || !Character->IsLocallyControlled() || Character->GetEquippedWeaponType() != ERogue10mWeaponType::Unarmed
  || !FMath::IsFinite(Strength) || !FMath::IsFinite(Side) || !FMath::IsFinite(HitFeedbackScale)
  || !FMath::IsFinite(CameraMotionScale) || CameraMotionScale <= 0.0f || HitFeedbackScale <= 0.0f || Strength <= 0.0f)
 { HitFeedbackAge = 0.16f; HitFeedbackStrength = HitFeedbackWeight = 0.0f; return; }
 HitFeedbackStrength = FMath::Max(HitFeedbackWeight, FMath::Clamp(Strength, 0.0f, 1.0f));
 HitFeedbackAge = 0.0f;
 HitFeedbackSide = FMath::Clamp(Side, -1.0f, 1.0f);
 ++ConfirmedHitFeedbackCount;
}

void URogue10mFirstPersonPresentationComponent::ResetCameraMotion()
{
	HitFeedbackAge = 0.16f;
	HitFeedbackStrength = HitFeedbackWeight = 0.0f;
	HeadReferenceSequence.Reset();
	HeadReferenceMesh.Reset();
	SequenceReferenceHead = FTransform::Identity;
	SmoothedHeadCameraRotation = FQuat::Identity;
	HeadYawSource.Reset();
	PreviousHeadSourceYaw = UnwrappedHeadSourceYaw = 0.0;
	PreviousHeadSourceTime = -1.0f;
	bHasHeadSourceYaw = false;
	AppliedCameraMotionOffset = FVector::ZeroVector;
	AppliedCameraMotionRotation = FRotator::ZeroRotator;
	CameraMotionOffsetVelocity = FVector::ZeroVector;
	CameraMotionRotationVelocity = FVector::ZeroVector;
}

namespace
{
	bool EvaluateSequenceHead(const UAnimSequence& Sequence, const FBoneContainer& Bones, float Time, FTransform& OutHead)
	{
		if (!Bones.IsValid() || !FMath::IsFinite(Time) || Sequence.IsValidAdditive()) { return false; }
		const int32 MeshHead = Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("head"));
		if (MeshHead == INDEX_NONE) { return false; }
		const FCompactPoseBoneIndex Head = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshHead));
		if (Head == INDEX_NONE) { return false; }

		// Evaluate the same compressed, retargeted local pose as the mesh, before
		// rendering collapses the hidden neck. Socket transforms are unsuitable here.
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&Bones);
		Pose.ResetToRefPose();
		FBlendedCurve Curve;
		Curve.InitFrom(Bones);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData Data(Pose, Curve, Attributes);
		Sequence.GetAnimationPose(Data, FAnimExtractContext(FMath::Clamp(Time, 0.0f, Sequence.GetPlayLength()), false));
		FCSPose<FCompactPose> ComponentPose;
		ComponentPose.InitPose(Pose);
		OutHead = ComponentPose.GetComponentSpaceTransform(Head);
		return !OutHead.ContainsNaN() && OutHead.GetRotation().IsNormalized();
	}
}

bool URogue10mFirstPersonPresentationComponent::ReadSingleNodeHeadMotion(
	USkeletalMeshComponent* Mesh, FVector& OutOffset, FRotator& OutRotation)
{
	UAnimSingleNodeInstance* SingleNode = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
	UAnimSequence* Sequence = SingleNode ? Cast<UAnimSequence>(SingleNode->GetCurrentAsset()) : nullptr;
	USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Sequence || !MeshAsset || Sequence->GetSkeleton() != MeshAsset->GetSkeleton()) { return false; }
	const FBoneContainer& Bones = SingleNode->GetRequiredBones();
	if (HeadReferenceSequence.Get() != Sequence || HeadReferenceMesh.Get() != MeshAsset)
	{
		if (!EvaluateSequenceHead(*Sequence, Bones, 0.0f, SequenceReferenceHead)) { return false; }
		HeadReferenceSequence = Sequence;
		HeadReferenceMesh = MeshAsset;
	}
	FTransform CurrentHead;
	if (!EvaluateSequenceHead(*Sequence, Bones, SingleNode->GetCurrentTime(), CurrentHead)) { return false; }
	const FQuat MeshToBody = Mesh->GetRelativeRotation().Quaternion();
	OutOffset = MeshToBody.RotateVector(CurrentHead.GetLocation() - SequenceReferenceHead.GetLocation());
	const FQuat HeadDelta = CurrentHead.GetRotation() * SequenceReferenceHead.GetRotation().Inverse();
	OutRotation = (MeshToBody * HeadDelta * MeshToBody.Inverse()).Rotator().GetNormalized();
	return !OutOffset.ContainsNaN() && !OutRotation.ContainsNaN();
}

void URogue10mFirstPersonPresentationComponent::ApplyHeadMotionToView(float DeltaTime, FMinimalViewInfo& InOutView)
{
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	const ERogue10mWeaponType Weapon = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	if (!bEnableFistPresentation || !Character || Character->IsDead() || !Character->IsLocallyControlled()
		|| (Weapon != ERogue10mWeaponType::Unarmed && Weapon != ERogue10mWeaponType::Knuckle)
		|| !FMath::IsFinite(DeltaTime) || !FMath::IsFinite(CameraMotionScale) || CameraMotionScale <= 0.0f
		|| !FMath::IsFinite(HeadCameraTranslationScale) || !FMath::IsFinite(MaxHeadCameraOffset)
		|| !FMath::IsFinite(HeadCameraFollowSpeed) || HeadCameraRotationScale.ContainsNaN()
		|| MaxHeadCameraRotation.ContainsNaN() || SmoothedHeadCameraRotation.ContainsNaN()
		|| AppliedCameraMotionOffset.ContainsNaN() || InOutView.Location.ContainsNaN() || InOutView.Rotation.ContainsNaN())
	{
		ResetCameraMotion();
		return;
	}

	USkeletalMeshComponent* Mesh = Character->GetFirstPersonMesh();
	const URogue10mFistAnimInstance* Fist = Mesh ? Cast<URogue10mFistAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	FVector SourceOffset = FVector::ZeroVector;
	FRotator SourceRotation = FRotator::ZeroRotator;
	FVector ReferenceHeadCS = FVector::ZeroVector;
	bool bHasReferenceHead = false;
	const UObject* SourceIdentity = Fist;
	float SourceTime = -1.0f;
	if (Fist)
	{
		// Raw pose includes gait/jump and body rotation even while montage weight is zero.
		SourceOffset = Fist->GetHeadPoseDeltaOffset();
		SourceRotation = Fist->GetHeadPoseDeltaRotation();
		ReferenceHeadCS = Fist->GetHeadPoseReferenceLocation();
		bHasReferenceHead = !ReferenceHeadCS.ContainsNaN() && !ReferenceHeadCS.IsNearlyZero();
		HeadReferenceSequence.Reset();
		HeadReferenceMesh.Reset();
	}
	else
	{
		bHasReferenceHead = ReadSingleNodeHeadMotion(Mesh, SourceOffset, SourceRotation);
		ReferenceHeadCS = SequenceReferenceHead.GetLocation();
		if (bHasReferenceHead)
		{
			SourceIdentity = HeadReferenceSequence.Get();
			SourceTime = Mesh->GetSingleNodeInstance()->GetCurrentTime();
		}
	}
	if (SourceOffset.ContainsNaN() || SourceRotation.ContainsNaN()) { ResetCameraMotion(); return; }

	const float Scale = FMath::Clamp(CameraMotionScale, 0.0f, 1.0f);
	const float OffsetLimit = FMath::Clamp(MaxHeadCameraOffset, 0.0f, 80.0f) * Scale;
	const FRotator Limits(
		FMath::Clamp(FMath::Abs(MaxHeadCameraRotation.Pitch), 0.0, 85.0) * Scale,
		FMath::Clamp(FMath::Abs(MaxHeadCameraRotation.Yaw), 0.0, 180.0) * Scale,
		FMath::Clamp(FMath::Abs(MaxHeadCameraRotation.Roll), 0.0, 60.0) * Scale);
	// Unwrap before applying an intensity or a limit. Scaling normalized Euler
	// yaw would turn +179/-179 into +89.5/-89.5 at half strength and whip the view.
	// A new sequence/native source or a backward seek starts a new reference turn,
	// but leaves the filtered orientation intact so recovery is still smooth.
	const double SourceYaw = FRotator::NormalizeAxis(SourceRotation.Yaw);
	const bool bSourceRewound = SourceTime >= 0.0f && PreviousHeadSourceTime >= 0.0f
		&& SourceTime + KINDA_SMALL_NUMBER < PreviousHeadSourceTime;
	if (!bHasHeadSourceYaw || HeadYawSource.Get() != SourceIdentity || bSourceRewound)
	{
		UnwrappedHeadSourceYaw = SourceYaw;
		bHasHeadSourceYaw = true;
		HeadYawSource = SourceIdentity;
	}
	else
	{
		UnwrappedHeadSourceYaw += FMath::FindDeltaAngleDegrees(PreviousHeadSourceYaw, SourceYaw);
	}
	PreviousHeadSourceYaw = SourceYaw;
	PreviousHeadSourceTime = SourceTime;
	const double ScaledYaw = UnwrappedHeadSourceYaw * FMath::Clamp(HeadCameraRotationScale.Yaw, 0.0, 1.0) * Scale;
	// 180 denotes an unrestricted orientation, allowing a full 360-degree turn.
	// A smaller comfort limit holds the accumulated turn on that side rather than
	// jumping to the opposite side of the interval at the Euler wrap boundary.
	const double TargetYaw = Limits.Yaw >= 180.0 - KINDA_SMALL_NUMBER
		? ScaledYaw : FMath::Clamp(ScaledYaw, -Limits.Yaw, Limits.Yaw);
	const FRotator TargetRotation(
		FMath::Clamp(FRotator::NormalizeAxis(SourceRotation.Pitch)
			* FMath::Clamp(HeadCameraRotationScale.Pitch, 0.0, 1.0) * Scale, -Limits.Pitch, Limits.Pitch),
		TargetYaw,
		FMath::Clamp(FRotator::NormalizeAxis(SourceRotation.Roll)
			* FMath::Clamp(HeadCameraRotationScale.Roll, 0.0, 1.0) * Scale, -Limits.Roll, Limits.Roll));
	FVector EyePivotOffset = FVector::ZeroVector;
	if (bHasReferenceHead && Mesh)
	{
		const UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
		if (Camera)
		{
			// Keep the calibrated eye-to-head distance when turning. Without this
			// lever correction a 180-degree head turn would leave the eye in front
			// of the chest, looking into the inside of the body.
			const FVector ReferenceHeadBody = Mesh->GetRelativeTransform().TransformPosition(ReferenceHeadCS);
			const FVector EyeLever = (Camera->GetRelativeLocation() - ReferenceHeadBody).GetClampedToMaxSize(50.0);
			EyePivotOffset = TargetRotation.Quaternion().RotateVector(EyeLever) - EyeLever;
		}
	}
	const FVector TargetOffset = ((SourceOffset * Scale + EyePivotOffset)
		* FMath::Clamp(HeadCameraTranslationScale, 0.0f, 1.0f)).GetClampedToMaxSize(OffsetLimit);
	const float FrameTime = FMath::Clamp(DeltaTime, 0.0f, 0.1f);
	const float Speed = FMath::Clamp(HeadCameraFollowSpeed, 0.0f, 60.0f);
	const float Alpha = FrameTime <= 0.0f ? 0.0f : (Speed > KINDA_SMALL_NUMBER ? 1.0f - FMath::Exp(-Speed * FrameTime) : 1.0f);
	AppliedCameraMotionOffset = FMath::Lerp(AppliedCameraMotionOffset, TargetOffset, Alpha).GetClampedToMaxSize(OffsetLimit);
	// Quaternion interpolation stays continuous across +180/-180 during a turn.
	SmoothedHeadCameraRotation = FQuat::Slerp(SmoothedHeadCameraRotation, TargetRotation.Quaternion(), Alpha).GetNormalized();
	AppliedCameraMotionRotation = SmoothedHeadCameraRotation.Rotator().GetNormalized();
	CameraMotionOffsetVelocity = CameraMotionRotationVelocity = FVector::ZeroVector;

	FVector FinalOffset = AppliedCameraMotionOffset;
	FQuat FinalRotation = SmoothedHeadCameraRotation;
	if (Weapon == ERogue10mWeaponType::Unarmed && FMath::IsFinite(HitFeedbackScale) && HitFeedbackScale > 0.0f)
	{
		HitFeedbackAge = FMath::Min(0.16f, HitFeedbackAge + FMath::Max(0.0f, DeltaTime));
		HitFeedbackWeight = HitFeedbackStrength * FMath::Square(1.0f - HitFeedbackAge / 0.16f);
		const float Weight = HitFeedbackWeight * FMath::Clamp(HitFeedbackScale, 0.0f, 1.0f) * Scale;
		FinalOffset = (FinalOffset + FVector(-0.65f, 0.12f * HitFeedbackSide, 0.12f) * Weight).GetClampedToMaxSize(OffsetLimit);
		FinalRotation = FinalRotation * (FRotator(-0.7f, 0.25f * HitFeedbackSide, 0.3f * HitFeedbackSide) * Weight).Quaternion();
	}
	else { HitFeedbackAge = 0.16f; HitFeedbackStrength = HitFeedbackWeight = 0.0f; }

	// Head displacement is body-local, not view-local: looking down cannot rotate a
	// vertical step into a forward lunge. Actor movement already supplies jump height.
	// Rotate the view in that same body frame, keeping user look and gameplay aim intact.
	const FQuat BodyRotation = Character->GetActorQuat();
	InOutView.Location += BodyRotation.RotateVector(FinalOffset);
	InOutView.Rotation = (BodyRotation * FinalRotation * BodyRotation.Inverse()
		* InOutView.Rotation.Quaternion()).Rotator();
}

void URogue10mFirstPersonPresentationComponent::ApplyBodyMotionToView(float DeltaTime, FMinimalViewInfo& InOutView)
{
	const bool bUseHeadCamera = IsFullBodyPresentationActive() && bEnableHeadCameraFollow;
	if (bUseHeadCamera != bUsingHeadCameraFollow)
	{
		ResetCameraMotion();
		bUsingHeadCameraFollow = bUseHeadCamera;
	}
	if (bUseHeadCamera)
	{
		ApplyHeadMotionToView(DeltaTime, InOutView);
		return;
	}

	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	const ERogue10mWeaponType Weapon = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	if (!FMath::IsFinite(CameraMotionScale) || !FMath::IsFinite(MaxCameraMotionOffset)
		|| MaxCameraMotionRotation.ContainsNaN() || !FMath::IsFinite(CameraMotionInterpolationSpeed)
		|| (Weapon == ERogue10mWeaponType::Unarmed && !FMath::IsFinite(BasicCameraMotionResponse))
		|| !FMath::IsFinite(DeltaTime)
		|| AppliedCameraMotionOffset.ContainsNaN() || AppliedCameraMotionRotation.ContainsNaN()
		|| CameraMotionOffsetVelocity.ContainsNaN() || CameraMotionRotationVelocity.ContainsNaN())
	{
		ResetCameraMotion();
		return;
	}
	const float Scale = FMath::Clamp(CameraMotionScale, 0.0f, 1.0f);
	if (!bPresentationActive || !bEnableFistPresentation || !Character || Character->IsDead() || !Character->IsLocallyControlled()
		|| Scale <= KINDA_SMALL_NUMBER
		|| (Weapon != ERogue10mWeaponType::Unarmed && Weapon != ERogue10mWeaponType::Knuckle))
	{
		ResetCameraMotion();
		return;
	}
	const USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
	const URogue10mFistAnimInstance* Fist = Arms ? Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()) : nullptr;
	const bool bBasicMotion = Weapon == ERogue10mWeaponType::Unarmed;
	if (!Fist || !FMath::IsFinite(Fist->GetBodyMotionWeight())
		|| (!bBasicMotion && Fist->GetBodyMotionWeight() <= KINDA_SMALL_NUMBER))
	{
		ResetCameraMotion();
		return;
	}

	const bool bSourceActive = Fist->GetBodyMotionWeight() > KINDA_SMALL_NUMBER;
	const FVector SourceOffset = bSourceActive ? Fist->GetBodyMotionOffset() : FVector::ZeroVector;
	const FRotator SourceRotation = bSourceActive ? Fist->GetBodyMotionRotation() : FRotator::ZeroRotator;
	if (SourceOffset.ContainsNaN() || SourceRotation.ContainsNaN())
	{
		ResetCameraMotion();
		return;
	}
	const float OffsetLimit = FMath::Clamp(MaxCameraMotionOffset, 0.0f, 3.0f) * Scale;
	const FRotator RotationLimit(
		FMath::Clamp(FMath::Abs(MaxCameraMotionRotation.Pitch), 0.0, 3.0) * Scale,
		FMath::Clamp(FMath::Abs(MaxCameraMotionRotation.Yaw), 0.0, 3.0) * Scale,
		FMath::Clamp(FMath::Abs(MaxCameraMotionRotation.Roll), 0.0, 2.0) * Scale);
	const auto ClampRotation = [&](const FRotator& Rotation)
	{
		const FRotator Normalized = Rotation.GetNormalized();
		return FRotator(
			FMath::Clamp(Normalized.Pitch, -RotationLimit.Pitch, RotationLimit.Pitch),
			FMath::Clamp(Normalized.Yaw, -RotationLimit.Yaw, RotationLimit.Yaw),
			FMath::Clamp(Normalized.Roll, -RotationLimit.Roll, RotationLimit.Roll));
	};

	// The animation snapshot already contains the actual slot blend weight.
	// Weight is only an activity gate; multiplying it again would attenuate the signal twice.
	const FVector TargetOffset = (SourceOffset * Scale).GetClampedToMaxSize(OffsetLimit);
	const FRotator TargetRotation = ClampRotation(SourceRotation * Scale);
	if (bBasicMotion)
	{
		// Exact critically damped integration for a held target: keep velocity through
		// a changing head pose instead of restarting a first-order ease every frame.
		// Clamp a hitch to 100 ms so a resumed view cannot jump across the whole response.
		const double FrameTime = FMath::Clamp(static_cast<double>(DeltaTime), 0.0, 0.1);
		const double Frequency = FMath::Clamp(static_cast<double>(BasicCameraMotionResponse), 1.0, 60.0);
		const double Decay = FMath::Exp(-Frequency * FrameTime);
		const auto FollowAxis = [&](double Position, double Target, double& Velocity)
		{
			const double Error = Position - Target;
			const double Transient = Velocity + Frequency * Error;
			Velocity = (Velocity - Frequency * Transient * FrameTime) * Decay;
			return Target + (Error + Transient * FrameTime) * Decay;
		};
		AppliedCameraMotionOffset = FVector(
			FollowAxis(AppliedCameraMotionOffset.X, TargetOffset.X, CameraMotionOffsetVelocity.X),
			FollowAxis(AppliedCameraMotionOffset.Y, TargetOffset.Y, CameraMotionOffsetVelocity.Y),
			FollowAxis(AppliedCameraMotionOffset.Z, TargetOffset.Z, CameraMotionOffsetVelocity.Z));
		if (AppliedCameraMotionOffset.SizeSquared() > FMath::Square(OffsetLimit))
		{
			AppliedCameraMotionOffset = AppliedCameraMotionOffset.GetClampedToMaxSize(OffsetLimit);
			const FVector Normal = AppliedCameraMotionOffset.GetSafeNormal();
			CameraMotionOffsetVelocity -= Normal * FMath::Max(0.0, FVector::DotProduct(CameraMotionOffsetVelocity, Normal));
		}
		if (OffsetLimit <= KINDA_SMALL_NUMBER) { CameraMotionOffsetVelocity = FVector::ZeroVector; }
		const auto FollowRotationAxis = [&](double Position, double Target, double Limit, double& Velocity)
		{
			// Values are local and bounded below three degrees, so this is also the shortest arc.
			const double Next = FollowAxis(Position, Target, Velocity);
			const double Bounded = FMath::Clamp(Next, -Limit, Limit);
			if ((Next > Limit && Velocity > 0.0) || (Next < -Limit && Velocity < 0.0) || Limit <= KINDA_SMALL_NUMBER)
			{
				Velocity = 0.0;
			}
			return Bounded;
		};
		AppliedCameraMotionRotation = FRotator(
			FollowRotationAxis(AppliedCameraMotionRotation.Pitch, TargetRotation.Pitch, RotationLimit.Pitch, CameraMotionRotationVelocity.X),
			FollowRotationAxis(AppliedCameraMotionRotation.Yaw, TargetRotation.Yaw, RotationLimit.Yaw, CameraMotionRotationVelocity.Y),
			FollowRotationAxis(AppliedCameraMotionRotation.Roll, TargetRotation.Roll, RotationLimit.Roll, CameraMotionRotationVelocity.Z));
	}
	else
	{
		// Preserve the existing equipped-knuckle response.
		CameraMotionOffsetVelocity = CameraMotionRotationVelocity = FVector::ZeroVector;
		const float Speed = FMath::Clamp(CameraMotionInterpolationSpeed, 0.0f, 60.0f);
		const float FrameTime = FMath::Max(0.0f, DeltaTime);
		const float Alpha = Speed > KINDA_SMALL_NUMBER ? 1.0f - FMath::Exp(-Speed * FrameTime) : 1.0f;
		AppliedCameraMotionOffset = FMath::Lerp(AppliedCameraMotionOffset, TargetOffset, Alpha).GetClampedToMaxSize(OffsetLimit);
		AppliedCameraMotionRotation = ClampRotation(FRotator(
			FMath::Lerp(AppliedCameraMotionRotation.Pitch, TargetRotation.Pitch, static_cast<double>(Alpha)),
			FMath::Lerp(AppliedCameraMotionRotation.Yaw, TargetRotation.Yaw, static_cast<double>(Alpha)),
			FMath::Lerp(AppliedCameraMotionRotation.Roll, TargetRotation.Roll, static_cast<double>(Alpha))));
	}

	// Keep the head-follow filter state separate from the short confirmed-hit pulse.
	FVector FinalOffset = AppliedCameraMotionOffset;
	FRotator FinalRotation = AppliedCameraMotionRotation;
	if (bBasicMotion && FMath::IsFinite(HitFeedbackScale) && HitFeedbackScale > 0.0f)
	{
		HitFeedbackAge = FMath::Min(0.16f, HitFeedbackAge + FMath::Max(0.0f, DeltaTime));
		HitFeedbackWeight = HitFeedbackStrength * FMath::Square(1.0f - HitFeedbackAge / 0.16f);
		const float Weight = HitFeedbackWeight * FMath::Clamp(HitFeedbackScale, 0.0f, 1.0f) * Scale;
		FinalOffset = (FinalOffset + FVector(-0.65f, 0.12f * HitFeedbackSide, 0.12f) * Weight).GetClampedToMaxSize(OffsetLimit);
		FinalRotation = ClampRotation(FinalRotation + FRotator(-0.7f, 0.25f * HitFeedbackSide, 0.3f * HitFeedbackSide) * Weight);
	}
	else { HitFeedbackAge = 0.16f; HitFeedbackStrength = HitFeedbackWeight = 0.0f; }

	// Compose in the final view's local space. Gameplay traces continue to use the untouched
	// CameraComponent and ControlRotation, while FOV, projection and post processing remain intact.
	const FQuat BaseRotation = InOutView.Rotation.Quaternion();
	InOutView.Location += BaseRotation.RotateVector(FinalOffset);
	InOutView.Rotation = (BaseRotation * FinalRotation.Quaternion()).Rotator();
}

void URogue10mFirstPersonPresentationComponent::RefreshPresentation()
{
	// Possession can happen before components have begun play. BeginPlay applies that deferred state.
	if (!HasBegunPlay())
	{
		return;
	}
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	if (Character && Character->GetAppearanceCameraComponent())
 {
  auto* Appearance = Character->GetAppearanceCameraComponent();
  if (Appearance->bEnableAppearanceCamera) { RestorePresentation(); }
  Appearance->RefreshAppearanceCamera();
  if (Appearance->bEnableAppearanceCamera) { return; }
 }
 const ERogue10mWeaponType Weapon = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	const bool bShouldActivate = bEnableFistPresentation && Character && Character->IsLocallyControlled()
		&& (Weapon == ERogue10mWeaponType::Unarmed || Weapon == ERogue10mWeaponType::Knuckle);
	if (!bShouldActivate)
	{
		RestorePresentation();
		return;
	}
	const bool bBasicPresentation = Weapon == ERogue10mWeaponType::Unarmed;
	if (bPresentationActive)
	{
		if (bActiveBasicPresentation == bBasicPresentation && bActiveFullBodyPresentation == bShowFullBody) { return; }
		RestorePresentation();
	}
	UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
	USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
	UClass* AnimationClass = GuardAnimationClass.LoadSynchronous();
	if (!Camera || !Arms || !Arms->GetSkeletalMeshAsset() || !AnimationClass)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("First-person fist presentation assets are missing on %s"), *GetNameSafe(Character));
		return;
	}

	USkeletalMesh* PresentationMesh = bShowFullBody ? GuardFullBodyMesh.LoadSynchronous() : GuardArmsMesh.LoadSynchronous();
	USkeletalMesh* OriginalMesh = Arms->GetSkeletalMeshAsset();
	// Matching skeleton assets alone are insufficient: presentation reads the original
	// head/spine hierarchy and uses the reference pose to construct the arm IK chains.
	bool bCompatibleMesh = PresentationMesh && PresentationMesh->GetSkeleton() == OriginalMesh->GetSkeleton();
	if (bCompatibleMesh)
	{
		const FReferenceSkeleton& SourceSkeleton = OriginalMesh->GetRefSkeleton();
		const FReferenceSkeleton& ArmsSkeleton = PresentationMesh->GetRefSkeleton();
		bCompatibleMesh = SourceSkeleton.GetNum() == ArmsSkeleton.GetNum();
		for (int32 Index = 0; bCompatibleMesh && Index < SourceSkeleton.GetNum(); ++Index)
		{
			bCompatibleMesh = SourceSkeleton.GetBoneName(Index) == ArmsSkeleton.GetBoneName(Index)
				&& SourceSkeleton.GetParentIndex(Index) == ArmsSkeleton.GetParentIndex(Index)
				&& SourceSkeleton.GetRefBonePose()[Index].Equals(ArmsSkeleton.GetRefBonePose()[Index]);
		}
	}
	if (!bCompatibleMesh)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("First-person %s mesh is missing or incompatible with %s; retaining configured mesh"), bShowFullBody ? TEXT("full-body") : TEXT("arms-only"), *GetNameSafe(OriginalMesh));
	}
	PreviousMeshAsset = OriginalMesh;
	PreviousMaterialOverrides = Arms->OverrideMaterials;
	PreviousHiddenBones.Reset();
	const TArray<uint8>& BoneVisibility = Arms->GetBoneVisibilityStates();
	for (int32 Index = 0; Index < BoneVisibility.Num(); ++Index)
	{
		if (BoneVisibility[Index] == BVS_ExplicitlyHidden)
		{
			PreviousHiddenBones.Add(Arms->GetBoneName(Index));
		}
	}
	PreviousSingleNodeAsset = nullptr;
	if (UAnimSingleNodeInstance* SingleNode = Arms->GetSingleNodeInstance())
	{
		PreviousSingleNodeAsset = SingleNode->GetCurrentAsset();
		PreviousSingleNodePosition = SingleNode->GetCurrentTime();
		PreviousSingleNodePlayRate = SingleNode->GetPlayRate();
		bPreviousSingleNodePlaying = SingleNode->IsPlaying();
		bPreviousSingleNodeLooping = SingleNode->IsLooping();
	}
	PreviousPrimitiveType = static_cast<uint8>(Arms->FirstPersonPrimitiveType);
	bPreviousOnlyOwnerSee = Arms->bOnlyOwnerSee;
	bPreviousOwnerNoSee = Arms->bOwnerNoSee;
	PreviousBodyMesh = Character->GetMesh();
	if (PreviousBodyMesh.IsValid() && PreviousBodyMesh.Get() != Arms)
	{
		bPreviousBodyOwnerNoSee = PreviousBodyMesh->bOwnerNoSee;
		PreviousBodyMesh->SetOwnerNoSee(true);
	}

	PreviousCameraParent = Camera->GetAttachParent();
	PreviousMeshParent = Arms->GetAttachParent();
	PreviousCameraSocket = Camera->GetAttachSocketName();
	PreviousMeshSocket = Arms->GetAttachSocketName();
	PreviousCameraTransform = Camera->GetRelativeTransform();
	PreviousMeshTransform = Arms->GetRelativeTransform();
	PreviousWorldFOV = Camera->FieldOfView;
	PreviousArmsFOV = Camera->FirstPersonFieldOfView;
	PreviousFirstPersonScale = Camera->FirstPersonScale;
	bPreviousFirstPersonFOV = Camera->bEnableFirstPersonFieldOfView;
	bPreviousFirstPersonScale = Camera->bEnableFirstPersonScale;
	PreviousAnimationClass = Arms->GetAnimClass();
	PreviousAnimationMode = static_cast<uint8>(Arms->GetAnimationMode());
	bPreviousVisible = Arms->IsVisible();
	bPreviousHidden = Arms->bHiddenInGame;

	// Detach the camera first to avoid a cycle when the old camera is attached to the head.
	Camera->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Camera->SetRelativeLocationAndRotation(bShowFullBody ? FullBodyCameraOffset : GuardCameraOffset, FRotator::ZeroRotator);
	Camera->SetFieldOfView(GuardWorldFieldOfView);
	// Manny's first-person material branch fades nearby body surfaces and blends
	// viewmodel scale by depth. A complete body must use world projection so its
	// torso and legs stay continuous and its feet retain their real ground position.
	Camera->bEnableFirstPersonFieldOfView = !bShowFullBody;
	Camera->bEnableFirstPersonScale = !bShowFullBody;
	Camera->FirstPersonFieldOfView = GuardArmsFieldOfView;
	Camera->FirstPersonScale = GuardFirstPersonScale;

	// A complete body follows capsule yaw, not camera pitch. Otherwise looking down
	// rotates the feet away from the view and makes the body appear to float.
	USceneComponent* MeshParent = bShowFullBody ? static_cast<USceneComponent*>(Character->GetCapsuleComponent()) : Camera;
	Arms->AttachToComponent(MeshParent, FAttachmentTransformRules::KeepRelativeTransform);
	Arms->SetRelativeLocationAndRotation(bShowFullBody ? FullBodyMeshOffset : GuardMeshOffset, GuardMeshRotation);
	Arms->SetRelativeScale3D(FVector::OneVector);
	bMeshWasReplaced = bCompatibleMesh && PresentationMesh != OriginalMesh;
	if (bMeshWasReplaced)
	{
		Arms->SetSkeletalMeshAsset(PresentationMesh);
	}
	Arms->SetOnlyOwnerSee(true);
	Arms->SetOwnerNoSee(false);
	Arms->SetFirstPersonPrimitiveType(bShowFullBody ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::FirstPerson);
	Arms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Arms->SetAnimInstanceClass(AnimationClass);
	Arms->SetVisibility(true, false);
	Arms->SetHiddenInGame(false, false);
	// Hide the complete neck/head skin, keeping the proven first-person silhouette.
	// The animation proxy measures its real head pose before renderer bone hiding is applied.
	// Never hide pelvis/spine: those bones also parent the arms.
	if (bShowFullBody)
	{
		// A configured mesh can already carry explicit thigh/torso masks. Preserve them
		// for deactivation, but reveal all geometry before hiding only the head.
		const TArray<uint8> Visibility = Arms->GetBoneVisibilityStates();
		for (int32 Index = 0; Index < Visibility.Num(); ++Index)
		{
			if (Visibility[Index] == BVS_ExplicitlyHidden) { Arms->UnHideBoneByName(Arms->GetBoneName(Index)); }
		}
	}
	const TArray<FName> BonesToHide = bShowFullBody
		? TArray<FName>{ TEXT("neck_01") }
		: TArray<FName>{ TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r") };
	for (const FName Bone : BonesToHide)
	{
		if (Arms->GetBoneIndex(Bone) != INDEX_NONE && !Arms->IsBoneHiddenByName(Bone))
		{
			Arms->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			NewlyHiddenBones.Add(Bone);
		}
	}
	ResetCameraMotion();
	bActiveBasicPresentation = bBasicPresentation;
	bActiveFullBodyPresentation = bShowFullBody;
	bPresentationActive = true;
	UE_LOG(LogRogue10m, Log, TEXT("First-person fist guard enabled: %s mode=%s mesh=%s"), *GetNameSafe(Character),
		bShowFullBody ? TEXT("full-body") : TEXT("arms-only"), *GetNameSafe(Arms->GetSkeletalMeshAsset()));
}

void URogue10mFirstPersonPresentationComponent::RestorePresentation()
{
	ResetCameraMotion();
	if (!bPresentationActive)
	{
		return;
	}
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
	USkeletalMeshComponent* Arms = Character ? Character->GetFirstPersonMesh() : nullptr;
	if (Arms)
	{
		for (const FName Bone : NewlyHiddenBones)
		{
			Arms->UnHideBoneByName(Bone);
		}
		if (bMeshWasReplaced)
		{
			Arms->SetSkeletalMeshAsset(PreviousMeshAsset);
		}
		{
			// Mesh initialization can rebuild bone visibility; restore only explicit masks,
			// letting the engine reconstruct the inherited hidden-by-parent states.
			const TArray<uint8> CurrentVisibility = Arms->GetBoneVisibilityStates();
			for (int32 Index = 0; Index < CurrentVisibility.Num(); ++Index)
			{
				if (CurrentVisibility[Index] == BVS_ExplicitlyHidden)
				{
					Arms->UnHideBoneByName(Arms->GetBoneName(Index));
				}
			}
			for (const FName Bone : PreviousHiddenBones)
			{
				Arms->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			}
		}
		Arms->OverrideMaterials = PreviousMaterialOverrides;
		Arms->SetOnlyOwnerSee(bPreviousOnlyOwnerSee);
		Arms->SetOwnerNoSee(bPreviousOwnerNoSee);
		Arms->SetFirstPersonPrimitiveType(static_cast<EFirstPersonPrimitiveType>(PreviousPrimitiveType));
		Arms->MarkRenderStateDirty();
		// Restore the mesh first; the saved camera parent may be this mesh.
		if (PreviousMeshParent.IsValid())
		{
			Arms->AttachToComponent(PreviousMeshParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, PreviousMeshSocket);
		}
		else
		{
			Arms->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}
		Arms->SetRelativeTransform(PreviousMeshTransform);
		Arms->SetAnimInstanceClass(PreviousAnimationClass);
		Arms->SetAnimationMode(static_cast<EAnimationMode::Type>(PreviousAnimationMode));
		if (static_cast<EAnimationMode::Type>(PreviousAnimationMode) == EAnimationMode::AnimationSingleNode)
		{
			if (UAnimSingleNodeInstance* SingleNode = Arms->GetSingleNodeInstance())
			{
				SingleNode->SetAnimationAsset(PreviousSingleNodeAsset, bPreviousSingleNodeLooping, PreviousSingleNodePlayRate);
				SingleNode->SetPosition(PreviousSingleNodePosition, false);
				SingleNode->SetPlaying(bPreviousSingleNodePlaying);
			}
		}
		Arms->SetVisibility(bPreviousVisible, false);
		Arms->SetHiddenInGame(bPreviousHidden, false);
	}
	if (Camera)
	{
		if (PreviousCameraParent.IsValid())
		{
			Camera->AttachToComponent(PreviousCameraParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, PreviousCameraSocket);
		}
		else
		{
			Camera->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}
		Camera->SetRelativeTransform(PreviousCameraTransform);
		Camera->SetFieldOfView(PreviousWorldFOV);
		Camera->FirstPersonFieldOfView = PreviousArmsFOV;
		Camera->FirstPersonScale = PreviousFirstPersonScale;
		Camera->bEnableFirstPersonFieldOfView = bPreviousFirstPersonFOV;
		Camera->bEnableFirstPersonScale = bPreviousFirstPersonScale;
	}
	if (PreviousBodyMesh.IsValid() && PreviousBodyMesh.Get() != Arms)
	{
		PreviousBodyMesh->SetOwnerNoSee(bPreviousBodyOwnerNoSee);
	}
	PreviousBodyMesh.Reset();
	PreviousMeshAsset = nullptr;
	PreviousMaterialOverrides.Reset();
	PreviousSingleNodeAsset = nullptr;
	PreviousHiddenBones.Reset();
	bMeshWasReplaced = false;
	NewlyHiddenBones.Reset();
	PreviousAnimationClass = nullptr;
	bPresentationActive = false;
	bActiveFullBodyPresentation = false;
}
