// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mFighterCombatStanceAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mViewAlignedUpperBodyProxy.h"

URogue10mFighterCombatStanceAnimInstance::URogue10mFighterCombatStanceAnimInstance()
{
	FighterCombatStanceSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle.A_Fighter_CombatIdle")));
}

void URogue10mFighterCombatStanceAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	FighterCombatStanceAnimation = FighterCombatStanceSequence.LoadSynchronous();
	FighterCombatBlendProgress = FighterCombatStanceAlpha = 0.0f;
	MovingAttackUpperBodyAlpha = 0.0f;
	UpperBodyAimPitch = UpperBodyAimYaw = 0.0f;
	bMouseDrivenHeadApplied = false;
	MouseDrivenHeadWorldAim = FQuat::Identity;
	MouseDrivenHeadSourceRotation = FQuat::Identity;
	GroundLocomotionPlayRate = 1.0f;
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

bool URogue10mFighterCombatStanceAnimInstance::IsViewAimDodgeActive() const
{
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
	bool bDodgePoseContributes = false;
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
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
	const bool bFighterWeapon = Weapon == ERogue10mWeaponType::Unarmed || Weapon == ERogue10mWeaponType::Knuckle;
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

	// The cached locomotion pose is also the lower-body source during moving attacks.
	// Do not leave it mixed with a stationary guard while movement or a jump has already begun.
	if (Movement && (!Movement->IsMovingOnGround() || GroundSpeed > FMath::Max(0.0f, FighterCombatStationarySpeed)
		|| (Animation && Animation->HasMovementInput())))
	{
		FighterCombatBlendProgress = FighterCombatStanceAlpha = 0.0f;
		return;
	}
	const bool bStanding = bContextValid && Movement->IsMovingOnGround() && !Character->IsSprinting()
		&& !Animation->HasMovementInput() && GroundSpeed <= FMath::Max(0.0f, FighterCombatStationarySpeed);
	const bool bRaise = FighterCombatStanceAnimation && bStanding && Animation->IsFighterCombatStanceRequested();
	const float Duration = bRaise ? FighterCombatRaiseSeconds : FighterCombatLowerSeconds;
	const float Step = FMath::Max(0.0f, DeltaSeconds) / FMath::Max(0.01f, Duration);
	FighterCombatBlendProgress = FMath::Clamp(FighterCombatBlendProgress + (bRaise ? Step : -Step), 0.0f, 1.0f);
	FighterCombatStanceAlpha = FMath::SmoothStep(0.0f, 1.0f, FighterCombatBlendProgress);
}
