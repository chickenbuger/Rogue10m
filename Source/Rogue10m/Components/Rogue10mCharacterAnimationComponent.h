// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mCharacterAnimationComponent.generated.h"

class ARogue10mCharacter;
class URogue10mCharacterMotionDataAsset;
class UAnimMontage;

/** 이동 이벤트를 공통 모션과 Niagara Cue로 변환합니다. 상시 Tick을 사용하지 않습니다. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mCharacterAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mCharacterAnimationComponent();

	void SetMovementState(bool bHasMovementInput, bool bSprinting);
	void NotifyJump(int32 JumpCount);
	void NotifyLanded();
	void NotifyDodge();
	void StopMotionEffects();
	/** Accepted attacks, charging and dodges keep the fighter's guard ready briefly. */
	void RequestFighterCombatStance(float BusySeconds = 0.0f);
	void SetFighterCombatChargeHeld(bool bHeld);
	void ResetFighterCombatStance();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Animation|Fighter Stance")
	bool IsFighterCombatStanceRequested() const;

	bool HasMovementInput() const { return bMovementInputActive; }
	bool IsSprinting() const { return bSprintActive; }
	bool IsFighterCombatChargeHeld() const { return bFighterCombatChargeHeld; }

	/** Rejects the measured turning/rolling sources, including composite segments, before gameplay or playback starts. */
	static bool IsHeadViewSafeAttackMontage(const UAnimMontage* Montage);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Fighter Stance", meta=(ClampMin="0.0", Units="s"))
	float FighterCombatHoldSeconds = 3.0f;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Animation")
	const URogue10mCharacterMotionDataAsset* GetMotionData() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ARogue10mCharacter* GetOwnerCharacter() const;
	void RefreshFootstepTimer();
	void SpawnFootstepEffect();
	void SpawnGroundEffect(
		class UNiagaraSystem* Effect, float UniformScale, float EmissionDuration,
		const FVector& LocalOffset = FVector::ZeroVector) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<URogue10mCharacterMotionDataAsset> MotionData;

	FTimerHandle FootstepTimerHandle;
	bool bMovementInputActive = false;
	bool bSprintActive = false;
	bool bNextFootstepRight = false;
	bool bFighterCombatChargeHeld = false;
	float FighterCombatReadyUntil = -1.0f;
};
