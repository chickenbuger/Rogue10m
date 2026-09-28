// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterAnimationComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "TimerManager.h"

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
	if (const URogue10mCharacterMotionDataAsset* Data = GetMotionData())
	{
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
	bMovementInputActive = false;
	bSprintActive = false;
	bNextFootstepRight = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FootstepTimerHandle);
	}
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

	const float Interval = bSprintActive ? Data->RunFootstepInterval : Data->WalkFootstepInterval;
	World->GetTimerManager().SetTimer(
		FootstepTimerHandle, this, &URogue10mCharacterAnimationComponent::SpawnFootstepEffect,
		FMath::Max(0.05f, Interval), true, FMath::Max(0.05f, Interval));
}

void URogue10mCharacterAnimationComponent::SpawnFootstepEffect()
{
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
