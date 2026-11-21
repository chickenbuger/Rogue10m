// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mFighterCombatStanceAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mViewAlignedUpperBodyProxy.h"
#include "Rogue10mWeightedFirstPersonSettings.h"

URogue10mFighterCombatStanceAnimInstance::URogue10mFighterCombatStanceAnimInstance()
{
	FighterCombatStanceSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle.A_Fighter_CombatIdle")));
	GreatSwordIdleSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/A_GreatSword_Idle.A_GreatSword_Idle")));
	GreatSwordLocomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/BS_GreatSword_Ground.BS_GreatSword_Ground")));
	LearnedRollAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Common/Evade/A_CommonShoulderRoll.A_CommonShoulderRoll")));
	GreatSwordCapstoneBrace = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/GreatSword20261110/A_GreatSword_CapstoneChargeBrace.A_GreatSword_CapstoneChargeBrace")));
	SecondJumpPoseAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Common/Airborne/A_CommonSecondJumpImpulse.A_CommonSecondJumpImpulse")));
	NonlethalFrontReaction = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/FighterStance/A_PlayerHitRecoil_Front.A_PlayerHitRecoil_Front")));
	NonlethalRearReaction = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/FighterStance/A_PlayerHitRecoil_Rear.A_PlayerHitRecoil_Rear")));
}

void URogue10mFighterCombatStanceAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedFighterIdle = FighterCombatStanceSequence.LoadSynchronous();
	CachedGreatSwordIdle = GreatSwordIdleSequence.LoadSynchronous();
	CachedUnarmedLocomotion = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference.BS_Unarmed_PalmReference"));
	CachedGreatSwordLocomotion = GreatSwordLocomotion.LoadSynchronous();
	CachedLearnedRoll = LearnedRollAnimation.LoadSynchronous();
	CachedCapstoneBrace = GreatSwordCapstoneBrace.LoadSynchronous();
	CachedNonlethalFront = NonlethalFrontReaction.LoadSynchronous();
	CachedNonlethalRear = NonlethalRearReaction.LoadSynchronous();
	bLearnedRollPoseApplied = false;
	CachedSecondJumpPose = SecondJumpPoseAnimation.LoadSynchronous();
	bSecondJumpPoseApplied = false;
	FighterCombatStanceAnimation = CachedFighterIdle;
	GroundLocomotionBlendSpace = CachedUnarmedLocomotion;
	FighterCombatBlendProgress = FighterCombatStanceAlpha = 0.0f;
	MovingAttackUpperBodyAlpha = 0.0f;
	WeightedHandPitch = FVector2D::ZeroVector;
	bWeightedHandMotionApplied = false;
	UpperBodyAimPitch = UpperBodyAimYaw = 0.0f;
	bMouseDrivenHeadApplied = false;
	MouseDrivenHeadWorldAim = FQuat::Identity;
	MouseDrivenHeadSourceRotation = FQuat::Identity;
	GroundLocomotionPlayRate = 1.0f;
	bUseForwardRunLanding = bWasAirborneOnFirstJump = false;
	GroundLocomotionStartPosition = ForwardRunLandingElapsed = 0.0f;
	CachedDodgeMontage.Reset();
	CachedDodgeMontagePath.Reset();
	bDodgeMontageCacheAttempted = false;
	if (const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(TryGetPawnOwner()))
	{
		CacheDodgeMontage(Character->GetCharacterAnimationComponent());
	}
}

FAnimInstanceProxy* URogue10mFighterCombatStanceAnimInstance::CreateAnimInstanceProxy()
{
	return new FRogue10mViewAlignedUpperBodyProxy(this);
}

void URogue10mFighterCombatStanceAnimInstance::SetGroundedDefeatStartPose(FPoseSnapshot&& Pose)
{
	GetProxyOnGameThread<FRogue10mViewAlignedUpperBodyProxy>().SetGroundedDefeatStartPose(MoveTemp(Pose));
}

bool URogue10mFighterCombatStanceAnimInstance::IsViewAimDodgeActive() const
{
	const auto* Character = Cast<ARogue10mCharacter>(TryGetPawnOwner());
	if (Character && Character->IsRolling()) { return true; }
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (!Instance || !Instance->Montage) { continue; }
		const bool bDodge = Instance->Montage == CachedDodgeMontage.Get()
			|| (CachedDodgeMontagePath.IsValid() && FSoftObjectPath(Instance->Montage.Get()) == CachedDodgeMontagePath);
		if (bDodge && (Instance->IsActive() || Instance->GetWeight() > UE_SMALL_NUMBER)) { return true; }
	}
	return false;
}

void URogue10mFighterCombatStanceAnimInstance::CacheDodgeMontage(const URogue10mCharacterAnimationComponent* Animation)
{
	// Initialization may precede the pawn owner; the first update with an owner retries once.
	// Never synchronously load motion data on every animation update.
	bDodgeMontageCacheAttempted = true;
	const URogue10mCharacterMotionDataAsset* MotionData = Animation ? Animation->GetMotionData() : nullptr;
	CachedDodgeMontage = MotionData ? MotionData->DodgeMontage.Get() : nullptr;
	CachedDodgeMontagePath = FSoftObjectPath(CachedDodgeMontage.Get());
}

void URogue10mFighterCombatStanceAnimInstance::UpdateForwardRunLanding(const ARogue10mCharacter* Character,
	const UCharacterMovementComponent* Movement, bool bCharacterContextValid, float GroundSpeed, float DeltaSeconds)
{
	const bool bArrivingFromFirstJump = bWasAirborneOnFirstJump;
	bWasAirborneOnFirstJump = bCharacterContextValid && Movement->IsFalling() && Character->JumpCurrentCount == 1;
	if (!bUseForwardRunLanding && !bArrivingFromFirstJump)
	{
		return;
	}

	// At these speeds the existing BlendSpace selects its authored forward Jog endpoint.
	constexpr float SeedGateTime = 0.25f;
	constexpr float HandoffSettledTime = 0.50f;
	// The outgoing L/R interval enters Walk / Run at the second Jog stride.
	constexpr float SeedSecondsInJog = 0.767617f;
	constexpr float ForwardJogDuration = 53.0f / 30.0f;
	const bool bWithinSpeed = GroundSpeed >= ForwardRunLandingSpeedRange.X && GroundSpeed <= ForwardRunLandingSpeedRange.Y;
	const bool bGroundedForwardRun = bCharacterContextValid && bWithinSpeed && Movement->IsMovingOnGround()
		&& Character->IsThirdPersonGameplayActive() && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
		&& !Character->IsDodging() && !Character->IsSprinting() && !IsAnyMontagePlaying()
		&& Movement->GetCurrentAcceleration().SizeSquared2D() > UE_SMALL_NUMBER
		&& FVector::DotProduct(Movement->Velocity.GetSafeNormal2D(), Character->GetActorForwardVector().GetSafeNormal2D())
			>= FMath::Cos(FMath::DegreesToRadians(ForwardRunLandingMaxYaw));
	ForwardRunLandingElapsed = bUseForwardRunLanding ? ForwardRunLandingElapsed + FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	bUseForwardRunLanding = bGroundedForwardRun && ForwardRunLandingElapsed < HandoffSettledTime;
	GroundLocomotionStartPosition = bUseForwardRunLanding && ForwardRunLandingElapsed >= SeedGateTime
		? SeedSecondsInJog / ForwardJogDuration : 0.0f;
}

void URogue10mFighterCombatStanceAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(TryGetPawnOwner());
	URogue10mCharacterAnimationComponent* Animation = Character ? Character->GetCharacterAnimationComponent() : nullptr;
	if (Character && !bDodgeMontageCacheAttempted)
	{
		CacheDodgeMontage(Animation);
	}
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const ARogue10mPlayerController* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	const bool bCharacterContextValid = Character && Movement && Character->GetController()
		&& !Character->IsDead() && !(Controller && Controller->IsAnyBlockingWindowVisible());
	const float GroundSpeed = Movement ? Movement->Velocity.Size2D() : 0.0f;
	UpdateForwardRunLanding(Character, Movement, bCharacterContextValid, GroundSpeed, DeltaSeconds);
	GroundLocomotionPlayRate = 1.0f;
	if (bCharacterContextValid && Movement->IsMovingOnGround() && !IsViewAimDodgeActive()
		&& FMath::IsFinite(GroundSpeed) && GroundSpeed > UE_SMALL_NUMBER)
	{
		// BlendSpace coordinates already blend Walk (300 cm/s) into Jog (600 cm/s).
		// Preserve its weighted stride inside that range; only speed above its
		// authored maximum needs extra cadence. Scaling slow walking again would
		// count the idle/walk stride blend twice and introduce foot sliding.
		const float RunReference = FMath::Max(1.0f, LocomotionRunReferenceSpeed);
		const float MaximumRate = FMath::Clamp(LocomotionMaximumPlayRate, 1.0f, 5.0f);
		GroundLocomotionPlayRate = FMath::Clamp(GroundSpeed / RunReference, 1.0f, MaximumRate);
	}
	const URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	const URogue10mAttackSkillData* AcceptedSkill = Combat ? Combat->GetLastAcceptedAttackSkill() : nullptr;
	const UAnimMontage* AcceptedMontage = AcceptedSkill ? AcceptedSkill->AttackMontage.Get() : nullptr;
	const UAnimMontage* ActiveMontage = GetCurrentActiveMontage();
	const FAnimMontageInstance* AcceptedInstance = AcceptedMontage ? GetInstanceForMontage(AcceptedMontage) : nullptr;
	bool bDodgePoseContributes = Character && Character->IsRolling();
	bool bAnyMontageContributes = false;
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (Instance && Instance->Montage && (Instance->IsActive() || Instance->GetWeight() > UE_SMALL_NUMBER))
		{
			bAnyMontageContributes = true;
		}
		if (Instance && Instance->Montage && Instance->GetWeight() > UE_SMALL_NUMBER)
		{
			// A soft-only configuration can be collected between uses. Recover the
			// non-owning cache from the playing montage's stable path without a load.
			const bool bDodge = Instance->Montage == CachedDodgeMontage.Get()
				|| (!CachedDodgeMontage.IsValid() && CachedDodgeMontagePath.IsValid()
					&& FSoftObjectPath(Instance->Montage.Get()) == CachedDodgeMontagePath);
			if (bDodge)
			{
				CachedDodgeMontage = Instance->Montage.Get();
				bDodgePoseContributes = true;
				break;
			}
		}
	}
	// A blending-out attack still contributes to the slot after it stops being the active montage.
	// A different active montage, such as Dodge, must always retain its authored full-body pose.
	const bool bAcceptedMontageContributes = AcceptedMontage && (ActiveMontage == AcceptedMontage
		|| (!ActiveMontage && AcceptedInstance && AcceptedInstance->GetWeight() > UE_SMALL_NUMBER));
	const bool bGroundAttack = AcceptedSkill && AcceptedSkill->InputSlot != ERogue10mAttackInputSlot::JumpPrimary
		&& AcceptedSkill->InputSlot != ERogue10mAttackInputSlot::JumpSpecial;
	const bool bUpperBodyEligible = bCharacterContextValid && Movement->IsMovingOnGround()
		&& bGroundAttack && bAcceptedMontageContributes && !bDodgePoseContributes;
	if (!bUpperBodyEligible)
	{
		MovingAttackUpperBodyAlpha = 0.0f;
	}
	else if (GroundSpeed > FMath::Max(0.0f, MovingAttackGroundSpeedThreshold))
	{
		MovingAttackUpperBodyAlpha = 1.0f;
	}
	else
	{
		const float ReturnStep = FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, MovingAttackReturnToFullBodySeconds);
		MovingAttackUpperBodyAlpha = FMath::Max(0.0f, MovingAttackUpperBodyAlpha - ReturnStep);
	}

	const ERogue10mWeaponType Weapon = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	const bool bGreatSword = Weapon == ERogue10mWeaponType::GreatSword;
	UAnimSequence* DesiredStance = bGreatSword && CachedGreatSwordIdle ? CachedGreatSwordIdle.Get() : CachedFighterIdle.Get();
	// Replacing an already weighted sequence swaps the entire guard in a single frame.
	// Let the outgoing guard reach the locomotion pose before selecting the next sequence.
	const bool bLowerOutgoingGuard = Character && Character->IsThirdPersonGameplayActive() && !Character->IsDead()
		&& Movement && Movement->IsMovingOnGround() && !Character->IsDodging() && !bAnyMontageContributes
		&& FighterCombatStanceAnimation && DesiredStance != FighterCombatStanceAnimation.Get()
		&& FighterCombatBlendProgress > UE_SMALL_NUMBER;
	if (!bLowerOutgoingGuard)
	{
		FighterCombatStanceAnimation = DesiredStance;
	}
	UBlendSpace* NextLocomotion = bGreatSword && CachedGreatSwordLocomotion ? CachedGreatSwordLocomotion.Get() : CachedUnarmedLocomotion.Get();
	const bool bCanBlendWeaponLocomotion = Character && Movement && GetSkelMeshComponent() == Character->GetAnimationPlaybackMesh()
		&& Character->IsThirdPersonGameplayActive() && !Character->IsDead() && Movement->IsMovingOnGround()
		&& !Character->IsDodging() && !bDodgePoseContributes && !bAnyMontageContributes && !bUseForwardRunLanding;
	const bool bChangeMovingLocomotion = GroundLocomotionBlendSpace && NextLocomotion && GroundLocomotionBlendSpace.Get() != NextLocomotion
		&& GroundSpeed > FMath::Max(0.0f, FighterCombatStationarySpeed);
	GetProxyOnGameThread<FRogue10mViewAlignedUpperBodyProxy>().UpdateWeaponLocomotionBlend(bCanBlendWeaponLocomotion, bChangeMovingLocomotion, DeltaSeconds);
	GroundLocomotionBlendSpace = NextLocomotion;
	const bool bFighterWeapon = bGreatSword || Weapon == ERogue10mWeaponType::Unarmed || Weapon == ERogue10mWeaponType::Knuckle;
	const bool bContextValid = Character && Animation && Movement && Character->GetController()
		&& !Character->IsDead() && bFighterWeapon && !(Controller && Controller->IsAnyBlockingWindowVisible());
	if (!bContextValid)
	{
		if (Animation) { Animation->ResetFighterCombatStance(); }
		if (!Character)
		{
			FighterCombatBlendProgress = FighterCombatStanceAlpha = 0.0f;
			return;
		}
	}

	// Ease the sword guard into ordinary third-person travel. Attacks still need the
	// unmixed locomotion pose under their lower body, as do jumps and evasions.
	const bool bLeavesStandingPose = Movement && (!Movement->IsMovingOnGround()
		|| GroundSpeed > FMath::Max(0.0f, FighterCombatStationarySpeed) || (Animation && Animation->HasMovementInput()));
	const bool bBlendSwordGuardIntoMovement = bLeavesStandingPose && bContextValid && bGreatSword && Character->IsThirdPersonGameplayActive()
		&& Movement->IsMovingOnGround() && !Character->IsDodging() && !bDodgePoseContributes
		&& !bAnyMontageContributes && !(Combat && Combat->IsWeaponChangeBlocked());
	if (bLeavesStandingPose && !bBlendSwordGuardIntoMovement && !bLowerOutgoingGuard)
	{
		FighterCombatBlendProgress = FighterCombatStanceAlpha = 0.0f;
		return;
	}
	const bool bStanding = bContextValid && Movement->IsMovingOnGround() && !Character->IsSprinting()
		&& !Animation->HasMovementInput() && GroundSpeed <= FMath::Max(0.0f, FighterCombatStationarySpeed);
	const bool bRaise = !bLowerOutgoingGuard && FighterCombatStanceAnimation && bStanding
		&& (bGreatSword || Animation->IsFighterCombatStanceRequested());
	const auto* Appearance = Character ? Character->GetAppearanceCameraComponent() : nullptr;
	const bool bWeightedGuard = bContextValid && !bAnyMontageContributes && Appearance && Appearance->IsWeightedFirstPersonActive();
	const FRogue10mWeightedFirstPersonSettings WeightedSettings = bWeightedGuard
		? Appearance->GetWeightedFirstPersonSettings() : FRogue10mWeightedFirstPersonSettings();
	const float RaiseSeconds = bWeightedGuard
		? FMath::Lerp(FighterCombatRaiseSeconds, WeightedSettings.GuardRaiseSeconds, WeightedSettings.HandMotionScale) : FighterCombatRaiseSeconds;
	const float LowerSeconds = bWeightedGuard
		? FMath::Lerp(FighterCombatLowerSeconds, WeightedSettings.GuardLowerSeconds, WeightedSettings.HandMotionScale) : FighterCombatLowerSeconds;
	const float FastLowerSeconds = bBlendSwordGuardIntoMovement || bLowerOutgoingGuard
		? FMath::Min(LowerSeconds, FMath::Max(0.01f, GreatSwordMoveGuardLowerSeconds)) : LowerSeconds;
	const float Duration = bRaise ? RaiseSeconds : FastLowerSeconds;
	const float Step = FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, Duration);
	FighterCombatBlendProgress = FMath::Clamp(FighterCombatBlendProgress + (bRaise ? Step : -Step), 0.0f, 1.0f);
	FighterCombatStanceAlpha = FMath::SmoothStep(0.0f, 1.0f, FighterCombatBlendProgress);
}
