// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterAnimationComponent.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "TimerManager.h"

namespace Rogue10mHeadViewMotion
{
// Exact source paths verified by the evaluated pose audit. These originals are
// retained for reference; active Data Assets use separate forward-facing clips.
static bool IsForbiddenSource(const UAnimSequenceBase* Sequence)
{
	const FString Path = Sequence->GetPathName();
	for (const TCHAR* Forbidden : {
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash"),
		TEXT("/Game/Rogue10m/Animation/Combat/FighterSkills/A_Fighter_Overlord_PowerPunch.A_Fighter_Overlord_PowerPunch"),
		TEXT("/Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny.A_Martelo2_Manny"),
		TEXT("/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_StraightKick_Manny.A_CC_StraightKick_Manny"),
		TEXT("/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_LowRightThaiKick_Manny.A_CC_LowRightThaiKick_Manny"),
		TEXT("/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_FrontEvade_Manny.A_CC_FrontEvade_Manny"),
		TEXT("/Game/Rogue10m/Animation/Preview/CloseCombat/A_CC_BackEvade_Manny.A_CC_BackEvade_Manny") })
	{
		if (Path == Forbidden) { return true; }
	}
	return false;
}

static bool IsSafeSequence(const UAnimSequenceBase* Sequence, TSet<const UAnimSequenceBase*>& Visiting, int32 Depth)
{
	if (!Sequence) { return true; }
	if (Depth > 8 || Visiting.Contains(Sequence) || IsForbiddenSource(Sequence)) { return false; }
	Visiting.Add(Sequence);
	bool bSafe = true;
	if (const UAnimComposite* Composite = Cast<UAnimComposite>(Sequence))
	{
		for (const FAnimSegment& Segment : Composite->AnimationTrack.AnimSegments)
		{
			if (!IsSafeSequence(Segment.GetAnimReference(), Visiting, Depth + 1)) { bSafe = false; break; }
		}
	}
	else if (const UAnimMontage* Montage = Cast<UAnimMontage>(Sequence))
	{
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (!IsSafeSequence(Segment.GetAnimReference(), Visiting, Depth + 1)) { bSafe = false; break; }
			}
			if (!bSafe) { break; }
		}
	}
	Visiting.Remove(Sequence);
	return bSafe;
}
}

bool URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(const UAnimMontage* Montage)
{
	TSet<const UAnimSequenceBase*> Visiting;
	return Rogue10mHeadViewMotion::IsSafeSequence(Montage, Visiting, 0);
}

URogue10mCharacterAnimationComponent::URogue10mCharacterAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MotionData = TSoftObjectPtr<URogue10mCharacterMotionDataAsset>(FSoftObjectPath(TEXT(
		"/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion.DA_CommonCharacterMotion")));
}

void URogue10mCharacterAnimationComponent::SetMovementState(
	bool bHasMovementInput, bool bSprinting)
{
	if (bMovementInputActive == bHasMovementInput && bSprintActive == bSprinting)
	{
		return;
	}

	bMovementInputActive = bHasMovementInput;
	bSprintActive = bSprinting;
	RefreshFootstepTimer();
}

void URogue10mCharacterAnimationComponent::NotifyJump(int32 JumpCount)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FootstepTimerHandle);
	}

	const URogue10mCharacterMotionDataAsset* Data = GetMotionData();
	if (Data)
	{
		const bool bDoubleJump = JumpCount > 1;
		SpawnGroundEffect(
			bDoubleJump ? Data->DoubleJumpEffect : Data->JumpEffect,
			bDoubleJump ? Data->DoubleJumpEffectScale : Data->JumpEffectScale,
			bDoubleJump ? Data->DoubleJumpEffectEmissionDuration : Data->JumpEffectEmissionDuration);
	}
}

void URogue10mCharacterAnimationComponent::NotifyLanded()
{
	const URogue10mCharacterMotionDataAsset* Data = GetMotionData();
	if (Data)
	{
		SpawnGroundEffect(
			Data->LandEffect, Data->LandEffectScale, Data->LandEffectEmissionDuration);
	}
	RefreshFootstepTimer();
}

void URogue10mCharacterAnimationComponent::NotifyDodge()
{
	RequestFighterCombatStance();
	if (const URogue10mCharacterMotionDataAsset* Data = GetMotionData())
	{
		if (Data->DodgeMontage)
		{
			RequestFighterCombatStance(Data->DodgeMontage->GetPlayLength());
		}
		if (ARogue10mCharacter* Character = GetOwnerCharacter())
		{
			Character->PlayCommonMontage(Data->DodgeMontage, 1.0f);
		}
		SpawnGroundEffect(
			Data->DodgeEffect, Data->DodgeEffectScale, Data->DodgeEffectEmissionDuration);
	}
}

void URogue10mCharacterAnimationComponent::StopMotionEffects()
{
	ResetFighterCombatStance();
	bMovementInputActive = false;
	bSprintActive = false;
	bNextFootstepRight = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FootstepTimerHandle);
	}
}

void URogue10mCharacterAnimationComponent::RequestFighterCombatStance(float BusySeconds)
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || Character->IsDead())
	{
		return;
	}
	const ERogue10mWeaponType Weapon = Character->GetEquippedWeaponType();
	if (Weapon != ERogue10mWeaponType::Unarmed && Weapon != ERogue10mWeaponType::Knuckle)
	{
		return;
	}
	FighterCombatReadyUntil = FMath::Max(FighterCombatReadyUntil,
		World->GetTimeSeconds() + FMath::Max(0.0f, BusySeconds) + FMath::Max(0.0f, FighterCombatHoldSeconds));
}

void URogue10mCharacterAnimationComponent::SetFighterCombatChargeHeld(bool bHeld)
{
	if (bFighterCombatChargeHeld == bHeld)
	{
		return;
	}
	bFighterCombatChargeHeld = bHeld;
	RequestFighterCombatStance();
}

void URogue10mCharacterAnimationComponent::ResetFighterCombatStance()
{
	bFighterCombatChargeHeld = false;
	FighterCombatReadyUntil = -1.0f;
}

bool URogue10mCharacterAnimationComponent::IsFighterCombatStanceRequested() const
{
	const UWorld* World = GetWorld();
	return World && (bFighterCombatChargeHeld || World->GetTimeSeconds() < FighterCombatReadyUntil);
}

const URogue10mCharacterMotionDataAsset* URogue10mCharacterAnimationComponent::GetMotionData() const
{
	return MotionData.LoadSynchronous();
}

void URogue10mCharacterAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMotionEffects();
	Super::EndPlay(EndPlayReason);
}

ARogue10mCharacter* URogue10mCharacterAnimationComponent::GetOwnerCharacter() const
{
	return Cast<ARogue10mCharacter>(GetOwner());
}

void URogue10mCharacterAnimationComponent::RefreshFootstepTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FootstepTimerHandle);
	const URogue10mCharacterMotionDataAsset* Data = GetMotionData();
	if (!Data || !Data->bEnableMotionEffects || !bMovementInputActive)
	{
		return;
	}

	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const USkeletalMeshComponent* Mesh = Character ? Character->GetAnimationPlaybackMesh() : nullptr;
	const URogue10mFighterCombatStanceAnimInstance* Instance = Mesh
		? Cast<URogue10mFighterCombatStanceAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	const float Cadence = Instance && FMath::IsFinite(Instance->GroundLocomotionPlayRate)
		? FMath::Clamp(Instance->GroundLocomotionPlayRate, 1.0f, 5.0f) : 1.0f;
	const float Interval = (bSprintActive ? Data->RunFootstepInterval : Data->WalkFootstepInterval) / Cadence;
	World->GetTimerManager().SetTimer(
		FootstepTimerHandle, this, &URogue10mCharacterAnimationComponent::SpawnFootstepEffect,
		FMath::Max(0.05f, Interval), false);
}

void URogue10mCharacterAnimationComponent::SpawnFootstepEffect()
{
	// One-shot rescheduling observes the same current animation cadence after a
	// move-speed/stat change, without adding a component Tick or a polling timer.
	RefreshFootstepTimer();
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const URogue10mCharacterMotionDataAsset* Data = GetMotionData();
	if (!Character || Character->IsDead() || !Movement || Movement->IsFalling() || !Data
		|| Movement->Velocity.Size2D() < Data->MinimumFootstepSpeed)
	{
		return;
	}

	const float SideSign = bNextFootstepRight ? 1.0f : -1.0f;
	bNextFootstepRight = !bNextFootstepRight;
	const FVector FootstepOffset(
		-Data->FootstepRearOffset, SideSign * Data->FootstepLateralOffset, 0.0f);
	SpawnGroundEffect(
		bSprintActive ? Data->RunFootstepEffect : Data->WalkFootstepEffect,
		bSprintActive ? Data->RunEffectScale : Data->WalkEffectScale,
		bSprintActive ? Data->RunEffectEmissionDuration : Data->WalkEffectEmissionDuration,
		FootstepOffset);
}

void URogue10mCharacterAnimationComponent::SpawnGroundEffect(
	UNiagaraSystem* Effect, float UniformScale, float EmissionDuration,
	const FVector& LocalOffset) const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	const URogue10mCharacterMotionDataAsset* Data = GetMotionData();
	if (!Character || !Capsule || !Effect || !Data || !Data->bEnableMotionEffects)
	{
		return;
	}

	const FVector Location = Character->GetActorLocation()
		- FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight()
		+ Character->GetActorTransform().TransformVectorNoScale(
			Data->GroundEffectOffset + LocalOffset);
	const FVector EffectScale(FMath::Max(0.01f, UniformScale));
	UNiagaraComponent* SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Character, Effect, Location, Character->GetActorRotation(), EffectScale,
		true, true, ENCPoolMethod::AutoRelease, true);
	if (!SpawnedEffect)
	{
		return;
	}

	SpawnedEffect->SetCustomTimeDilation(FMath::Max(0.1f, Data->MotionEffectTimeDilation));
	if (UWorld* World = GetWorld())
	{
		FTimerHandle DeactivateTimerHandle;
		World->GetTimerManager().SetTimer(
			DeactivateTimerHandle,
			FTimerDelegate::CreateWeakLambda(SpawnedEffect, [SpawnedEffect]()
			{
				SpawnedEffect->Deactivate();
			}),
			FMath::Max(0.01f, EmissionDuration), false);
	}
}
