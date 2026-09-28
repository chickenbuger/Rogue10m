// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Rogue10mFistAnimInstance.generated.h"

class UAnimSequence;

/** Fighting styles share the same input, damage window and bounded arm reach. */
UENUM(BlueprintType)
enum class ERogue10mBrawlerStance : uint8
{
	CompactBoxing,
	LongGuard,
	RootedMartial
};

/** First-person spatial adaptation of the existing Manny punch montages. */
UCLASS(Transient, Blueprintable)
class ROGUE10M_API URogue10mFistAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URogue10mFistAnimInstance();
	virtual void NativeInitializeAnimation() override;
	virtual void NativePostEvaluateAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose")
	TSoftObjectPtr<UAnimSequence> FistAnimation;

	/** Retargeted Boxing.fbx supplies the live left jab; the right jab mirrors the whole pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Source Boxing")
	TSoftObjectPtr<UAnimSequence> SourceBoxingAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Source Boxing")
	bool bEnableSourceBoxingBasicAttacks = true;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Source Boxing")
	float GetSourceBoxingWeight() const { return SourceBoxingWeight; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Source Boxing")
	float GetSourceBoxingTime() const { return SourceBoxingTime; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Source Boxing")
	bool IsSourceBoxingMirrored() const { return bSourceBoxingMirrored; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Source Boxing")
	bool IsSourceBoxingActive() const { return SourceBoxingWeight > KINDA_SMALL_NUMBER; }

	/** Component space: lateral distance, forward distance, height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose")
	FVector GuardHandTarget = FVector(20.0f, 43.0f, 148.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose")
	FVector ExtendedHandTarget = FVector(10.0f, 64.0f, 148.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose", meta=(ClampMin="10.0"))
	float SourcePunchTravel = 32.0f;

	/** Thumb points up with a slight inward lean in the relaxed vertical-fist guard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Orientation", meta=(ClampMin="-60", ClampMax="90", Units="deg"))
	float GuardThumbInwardDegrees = 15.0f;

	/** A small additional pronation follows the original montage's forward hand travel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Orientation", meta=(ClampMin="-60", ClampMax="90", Units="deg"))
	float HitThumbInwardDegrees = 35.0f;

	/** Anatomical MCP/PIP/DIP bends for the four fingers, evaluated once into the cached hand pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Clench")
	FVector FingerCurlDegrees = FVector(100.0f, 100.0f, 75.0f);

	/** Bounded first-person body/arc adaptation; zero restores the stationary-body presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Body", meta=(ClampMin="0", ClampMax="1"))
	float BodyMotionStrength = 0.65f;

	/** Final camera-local values already include the source montage blend weight. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	FRotator GetBodyMotionRotation() const { return BodyMotionRotation; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	FVector GetBodyMotionOffset() const { return BodyMotionOffset; }

	/** Activity gate only; consumers must not multiply the returned motion values again. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	float GetBodyMotionWeight() const { return BodyMotionWeight; }


	/** Full-body unarmed rests in the source Idle pose; legacy arms and equipped knuckles retain their guard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Ready")
	bool bEnableRelaxedIdle = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Ready", meta=(ClampMin="0.08", ClampMax="0.3", Units="s"))
	float CombatReadyRaiseSeconds = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Ready", meta=(ClampMin="0", ClampMax="3", Units="s"))
	float CombatReadyHoldSeconds = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Ready", meta=(ClampMin="0.2", ClampMax="1", Units="s"))
	float CombatReadyLowerSeconds = 0.45f;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Basic Brawler|Ready")
	float GetCombatReadyWeight() const { return CombatReadyWeight; }

	/** Boxing.fbx supplies a stepping left straight reference; other styles are authored adaptations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Stance")
	ERogue10mBrawlerStance BasicCombatStance = ERogue10mBrawlerStance::CompactBoxing;

	/** Full-body unarmed only; zero preserves the prior pose and all legacy arms/knuckle paths. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Stance", meta=(ClampMin="0", ClampMax="1"))
	float BasicStanceStrength = 1.0f;

	/** Bounded pose-space lead-foot step, independent of capsule movement and attack reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Stance", meta=(ClampMin="0", ClampMax="10", Units="cm"))
	float BasicLeadFootStep = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Stance", meta=(ClampMin="0", ClampMax="20", Units="deg"))
	float BasicSupportFootPivot = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Stance", meta=(ClampMin="0", ClampMax="3", Units="cm"))
	float BasicSupportHeelLift = 2.0f;

	/** Component-space fist endpoints for the default unarmed class (lateral, forward, height). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicJabTarget = FVector(7.0f, 64.0f, 153.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicStraightTarget = FVector(3.0f, 70.0f, 154.0f);

	/** Advance the shoulder with the torso rather than stretching the arm past its reach limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="0", ClampMax="12", Units="cm"))
	float BasicStraightBodyAdvance = 7.0f;

	/** Withdraw the opposite guard outside the hook's screen-space follow-through. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicHookGuardOffset = FVector(6.0f, -7.0f, 1.5f);

	/** Signed lateral coordinates: negative crosses the centreline for the right hook. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicHookTarget = FVector(-7.0f, 55.0f, 153.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicHookLoadedTarget = FVector(34.0f, 37.0f, 151.0f);

	/** Outer control point of the horizontal sweep; the fist accelerates into the centreline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicHookArcControl = FVector(44.0f, 68.0f, 154.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="0", ClampMax="35", Units="deg"))
	float BasicHookBodyYaw = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="-30", ClampMax="60", Units="deg"))
	float BasicHookThumbInwardDegrees = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicUppercutTarget = FVector(14.0f, 57.0f, 164.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler")
	FVector BasicChargeHandOffset = FVector(2.0f, 2.0f, -7.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="0", ClampMax="30", Units="deg"))
	float BasicChargeBodyYaw = 18.0f;

	/** Fraction of attack playback reserved for accelerating the fist into its existing hit time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Timing", meta=(ClampMin="0.08", ClampMax="0.22"))
	float BasicStrikeAccelerationFraction = 0.14f;

	/** Recovery bends the elbow before returning the fist along a separate arc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Timing", meta=(ClampMin="0.20", ClampMax="0.45"))
	float BasicRecoveryFraction = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Timing", meta=(ClampMin="0", ClampMax="0.08"))
	float BasicBodyLeadFraction = 0.04f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler|Timing", meta=(ClampMin="0", ClampMax="0.08"))
	float BasicHeadLagFraction = 0.035f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="0", ClampMax="10", Units="cm"))
	float BasicRecoveryArc = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Basic Brawler", meta=(ClampMin="0", ClampMax="1"))
	float JumpPresentationStrength = 1.0f;

	/** Grounded travel drives the same upper-body gait used by the full-body head camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Locomotion", meta=(ClampMin="0", ClampMax="1"))
	float LocomotionMotionStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Locomotion", meta=(ClampMin="100", ClampMax="1200", Units="cm/s"))
	float LocomotionReferenceSpeed = 600.0f;

	/** Distance of a complete left/right gait cycle; phase follows distance, not elapsed idle time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Locomotion", meta=(ClampMin="150", ClampMax="600", Units="cm"))
	float LocomotionStrideLength = 340.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Fist Pose|Locomotion", meta=(ClampMin="0", ClampMax="4", Units="cm"))
	float LocomotionVerticalAmplitude = 1.8f;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Locomotion")
	float GetLocomotionPresentationWeight() const { return LocomotionPresentationWeight; }

	/** Measured evaluated head delta before the bounded camera gain, in camera-local coordinates. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	FRotator GetHeadPoseDeltaRotation() const { return HeadPoseDeltaRotation; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	FVector GetHeadPoseDeltaOffset() const { return HeadPoseDeltaOffset; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Fist Pose|Body")
	float GetJumpPresentationWeight() const { return JumpPresentationWeight; }

	/** Unhidden reference head origin in mesh component space for the calibrated eye pivot. */
	FVector GetHeadPoseReferenceLocation() const { return HeadPoseReferenceLocation; }

	// Strong references keep the immutable sequence data alive while the proxy evaluates it.
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LoadedIdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LoadedFistAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LoadedSourceBoxingAnimation;

private:
	float SourceBoxingWeight = 0.0f;
	float SourceBoxingTime = 0.0f;
	bool bSourceBoxingMirrored = false;
	float CombatReadyWeight = 0.0f;
	FVector BodyMotionOffset = FVector::ZeroVector;
	FRotator BodyMotionRotation = FRotator::ZeroRotator;
	float BodyMotionWeight = 0.0f;
	FRotator HeadPoseDeltaRotation = FRotator::ZeroRotator;
	FVector HeadPoseDeltaOffset = FVector::ZeroVector;
	FVector HeadPoseReferenceLocation = FVector::ZeroVector;
	float JumpPresentationWeight = 0.0f;
	float LocomotionPresentationWeight = 0.0f;
};
