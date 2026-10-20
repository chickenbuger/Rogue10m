// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mFighterCombatStanceAnimInstance.generated.h"

class UAnimMontage;
class UAnimSequence;
class URogue10mCharacterAnimationComponent;
class FRogue10mViewAlignedUpperBodyProxy;

/** Common Manny locomotion remains in the AnimBP; moving ground attacks retain its lower-body pose. */
UCLASS(Blueprintable)
class ROGUE10M_API URogue10mFighterCombatStanceAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URogue10mFighterCombatStanceAnimInstance();
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** Read-only GT state used by the shared head-motion budget. */
	bool IsViewAimDodgeActive() const;

	/** The final head orientation follows the mouse separately from the authored body motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Head Aim")
	bool bEnableMouseDrivenHead = true;

	/** True only after the evaluated source pose received the captured mouse aim. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Head Aim")
	bool bMouseDrivenHeadApplied = false;

	/** Exact evaluated world-space aim delta; camera removes this same snapshot before adding current mouse aim. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Head Aim")
	FQuat MouseDrivenHeadWorldAim = FQuat::Identity;

	/** Source component basis captured with that pose; camera transports aim when the component moves later. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Head Aim")
	FQuat MouseDrivenHeadSourceRotation = FQuat::Identity;

	/** Feeds only the grounded Walk / Run BlendSpace player, never montage or jump time. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Locomotion Speed")
	float GroundLocomotionPlayRate = 1.0f;

	/** Verified Jog samples in BS_Unarmed_PalmReference were authored at 600 cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Locomotion Speed", meta=(ClampMin="1", Units="cm/s"))
	float LocomotionRunReferenceSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Locomotion Speed", meta=(ClampMin="1", ClampMax="5"))
	float LocomotionMaximumPlayRate = 3.0f;

	/** Final upper-body aim is applied after the existing locomotion / montage graph. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|View Aim")
	bool bEnableViewAlignedUpperBody = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|View Aim")
	float UpperBodyAimPitch = 0.0f;

	/** Residual controller yaw relative to the character; normally zero while facing the view. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|View Aim")
	float UpperBodyAimYaw = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|View Aim", meta=(ClampMin="0", ClampMax="85", Units="deg"))
	float UpperBodyAimMaxPitch = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|View Aim", meta=(ClampMin="0", ClampMax="80", Units="deg"))
	float UpperBodyAimMaxYaw = 45.0f;

	/** Small torso bend plus shoulder rotation keeps the head camera from following a large spine bend. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|View Aim", meta=(ClampMin="0", ClampMax="1"))
	float UpperBodyAimTorsoWeight = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|View Aim", meta=(ClampMin="0"))
	float UpperBodyAimInterpolationSpeed = 18.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Fighter Stance")
	float FighterCombatStanceAlpha = 0.0f;

	/** Selects the upper-body composite over the original full-body DefaultSlot pose. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Moving Attack")
	float MovingAttackUpperBodyAlpha = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Moving Attack", meta=(ClampMin="0.0", Units="cm/s"))
	float MovingAttackGroundSpeedThreshold = 10.0f;

	/** Movement immediately preserves locomotion; stopping returns to the authored full-body attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Moving Attack", meta=(ClampMin="0.01", Units="s"))
	float MovingAttackReturnToFullBodySeconds = 0.12f;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Rogue10m|Animation|Fighter Stance")
	TObjectPtr<UAnimSequence> FighterCombatStanceAnimation;

	/** Fixed provenance of the sequence player configured in ABP_Common_Unarmed. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Fighter Stance")
	TSoftObjectPtr<UAnimSequence> FighterCombatStanceSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Fighter Stance", meta=(ClampMin="0.01", Units="s"))
	float FighterCombatRaiseSeconds = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Fighter Stance", meta=(ClampMin="0.01", Units="s"))
	float FighterCombatLowerSeconds = 0.45f;

	/** A full-body standing guard yields to locomotion as soon as the player moves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Fighter Stance", meta=(ClampMin="0.0", Units="cm/s"))
	float FighterCombatStationarySpeed = 10.0f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	friend class FRogue10mViewAlignedUpperBodyProxy;
	void CacheDodgeMontage(const URogue10mCharacterAnimationComponent* Animation);

	TWeakObjectPtr<UAnimMontage> CachedDodgeMontage;
	FSoftObjectPath CachedDodgeMontagePath;
	bool bDodgeMontageCacheAttempted = false;
	float FighterCombatBlendProgress = 0.0f;
};
