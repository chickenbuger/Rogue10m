// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.generated.h"

struct FMinimalViewInfo;
class APlayerController;
class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class USceneComponent;

/** Local first-person body presentation; combat montages and damage timing remain on their existing path. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mFirstPersonPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mFirstPersonPresentationComponent();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|First Person")
	void RefreshPresentation();

	UFUNCTION(BlueprintPure, Category="Rogue10m|First Person")
	bool IsFistPresentationActive() const { return bPresentationActive; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|First Person")
	bool IsFullBodyPresentationActive() const { return bPresentationActive && bActiveFullBodyPresentation; }

	/** Called before the owning controller's first rotation update, after GameMode spawn rotation settles. */
	void ApplyInitialFullBodyViewPitchOnce(APlayerController& PlayerController);
	bool HasHandledInitialFullBodyViewPitch() const { return bInitialLocalViewHandled; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body|Initial View")
	bool bEnableInitialFullBodyViewPitch = true;

	/** Replaces only a neutral spawn pitch once; explicit spawn aim and later mouse input are preserved. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body|Initial View", meta=(ClampMin="-60", ClampMax="0", Units="deg"))
	float InitialFullBodyViewPitch = -30.0f;

	/** Adds only to a final view result; never moves the gameplay camera component. */
	void ApplyBodyMotionToView(float DeltaTime, FMinimalViewInfo& InOutView);
	void ResetCameraMotion();
	void NotifyConfirmedBrawlerHit(float Strength, float Side);
	int32 GetConfirmedHitFeedbackCount() const { return ConfirmedHitFeedbackCount; }
	float GetHitFeedbackWeight() const { return HitFeedbackWeight; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Hit Feedback", meta=(ClampMin="0", ClampMax="1"))
	float HitFeedbackScale = 1.0f;

	UFUNCTION(BlueprintPure, Category="Rogue10m|First Person|Camera Motion")
	FVector GetAppliedCameraMotionOffset() const { return AppliedCameraMotionOffset; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|First Person|Camera Motion")
	FRotator GetAppliedCameraMotionRotation() const { return AppliedCameraMotionRotation; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Camera Motion", meta=(ClampMin="0", ClampMax="1"))
	float CameraMotionScale = 1.0f;

	/** Maximum translation length in centimetres after scaling and filtering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Camera Motion", meta=(ClampMin="0", ClampMax="3", Units="cm"))
	float MaxCameraMotionOffset = 2.0f;

	/** Per-axis local pitch, yaw and roll limits in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Camera Motion")
	FRotator MaxCameraMotionRotation = FRotator(2.0f, 2.0f, 1.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Camera Motion", meta=(ClampMin="0", ClampMax="60"))
	float CameraMotionInterpolationSpeed = 18.0f;


	/** Full-body view follows animation before hidden-neck rendering and mouse aim are applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera")
	bool bEnableHeadCameraFollow = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera", meta=(ClampMin="0", ClampMax="1"))
	float HeadCameraTranslationScale = 1.0f;

	/** Yaw remains complete so a rear-facing animated head can look behind the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera")
	FRotator HeadCameraRotationScale = FRotator(0.75f, 1.0f, 0.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera", meta=(ClampMin="0", ClampMax="80", Units="cm"))
	float MaxHeadCameraOffset = 65.0f;

	/** Comfort limits for tilt. Yaw 180 permits complete turns; lower values limit accumulated turning until the source changes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera")
	FRotator MaxHeadCameraRotation = FRotator(55.0f, 180.0f, 25.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Head Camera", meta=(ClampMin="0", ClampMax="60"))
	float HeadCameraFollowSpeed = 24.0f;

	/** Natural frequency of the basic brawler's critically damped head-follow response. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Camera Motion", meta=(ClampMin="1", ClampMax="60"))
	float BasicCameraMotionResponse = 32.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	bool bEnableFistPresentation = true;

	/** Show one complete owner-visible body; false retains the camera-relative arms option. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body")
	bool bShowFullBody = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body")
	TSoftObjectPtr<USkeletalMesh> GuardFullBodyMesh;

	/** Full-body eye position, moved above and ahead of the shoulder surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body")
	FVector FullBodyCameraOffset = FVector(20.0f, 0.0f, 76.0f);

	/** Component-space fist/elbow adjustment, keeping reserve reach for punches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body")
	FVector FullBodyHandTargetOffset = FVector(0.0f, 12.0f, 18.0f);

	/** Retract only the basic unarmed guard; keep strike endpoints forward for visible extension. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body", meta=(ClampMin="0", ClampMax="25", Units="cm"))
	float FullBodyGuardPullback = 14.0f;

	/** Bring the basic unarmed guard inward; equipped knuckles retain their original guard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body", meta=(ClampMin="0", ClampMax="15", Units="cm"))
	float FullBodyGuardInward = 8.0f;

	/** Capsule-relative mesh origin. Looking down rotates the view, never the legs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body")
	FVector FullBodyMeshOffset = FVector(-5.0f, 0.0f, -94.0f);

	/** Limit procedural upper-body aiming without tipping the pelvis or feet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body", meta=(ClampMin="0", ClampMax="85", Units="deg"))
	float FullBodyAimPitchLimit = 75.0f;

	/** The remaining pitch is applied at each shoulder after the existing fist IK. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person|Full Body", meta=(ClampMin="0", ClampMax="0.5"))
	float FullBodySpinePitchShare = 0.0f;

	/** Camera position relative to the capsule, independent of animated head motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	FVector GuardCameraOffset = FVector(0.0f, 0.0f, 64.0f);

	/** Manny origin is at the feet; keep the two hands low and the centre of the screen clear. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	FVector GuardMeshOffset = FVector(-5.0f, 0.0f, -158.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	FRotator GuardMeshRotation = FRotator(0.0f, -90.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person", meta=(ClampMin="60", ClampMax="110"))
	float GuardWorldFieldOfView = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person", meta=(ClampMin="50", ClampMax="110"))
	float GuardArmsFieldOfView = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person", meta=(ClampMin="0.1", ClampMax="1.0"))
	float GuardFirstPersonScale = 0.6f;

	/** Same skeleton and DefaultSlot as the gameplay punch montages, without CopyPose from a retargeted body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	TSoftClassPtr<UAnimInstance> GuardAnimationClass;

	/** Arms geometry with the original full skeleton, so the head can still drive camera motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person")
	TSoftObjectPtr<USkeletalMesh> GuardArmsMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bInitialLocalViewHandled = false;
	void RestorePresentation();
	void ApplyHeadMotionToView(float DeltaTime, FMinimalViewInfo& InOutView);
	bool ReadSingleNodeHeadMotion(USkeletalMeshComponent* Mesh, FVector& OutOffset, FRotator& OutRotation);

	TWeakObjectPtr<UAnimSequence> HeadReferenceSequence;
	TWeakObjectPtr<USkeletalMesh> HeadReferenceMesh;
	FTransform SequenceReferenceHead = FTransform::Identity;
	FQuat SmoothedHeadCameraRotation = FQuat::Identity;
	TWeakObjectPtr<const UObject> HeadYawSource;
	double PreviousHeadSourceYaw = 0.0;
	double UnwrappedHeadSourceYaw = 0.0;
	float PreviousHeadSourceTime = -1.0f;
	bool bHasHeadSourceYaw = false;
	bool bUsingHeadCameraFollow = false;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> PreviousAnimationClass;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> PreviousMeshAsset;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> PreviousMaterialOverrides;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> PreviousSingleNodeAsset;

	TWeakObjectPtr<USceneComponent> PreviousCameraParent;
	TWeakObjectPtr<USceneComponent> PreviousMeshParent;
	FTransform PreviousCameraTransform;
	FTransform PreviousMeshTransform;
	FName PreviousCameraSocket;
	FName PreviousMeshSocket;
	TArray<FName> NewlyHiddenBones;
	TArray<FName> PreviousHiddenBones;
	TWeakObjectPtr<USkeletalMeshComponent> PreviousBodyMesh;
	float PreviousSingleNodePosition = 0.0f;
	float PreviousSingleNodePlayRate = 1.0f;
	uint8 PreviousPrimitiveType = 0;
	bool bPreviousSingleNodeLooping = false;
	bool bPreviousSingleNodePlaying = false;
	bool bPreviousOnlyOwnerSee = false;
	bool bPreviousOwnerNoSee = false;
	bool bPreviousBodyOwnerNoSee = false;
	bool bMeshWasReplaced = false;
	float PreviousWorldFOV = 90.0f;
	float PreviousArmsFOV = 70.0f;
	float PreviousFirstPersonScale = 0.6f;
	uint8 PreviousAnimationMode = 0;
	bool bPreviousFirstPersonFOV = false;
	bool bPreviousFirstPersonScale = false;
	bool bPreviousVisible = true;
	bool bPreviousHidden = false;
	bool bPresentationActive = false;
	bool bActiveBasicPresentation = false;
	bool bActiveFullBodyPresentation = false;
	float HitFeedbackAge = 0.16f;
	float HitFeedbackStrength = 0.0f;
	float HitFeedbackSide = 1.0f;
	float HitFeedbackWeight = 0.0f;
	int32 ConfirmedHitFeedbackCount = 0;
	FVector AppliedCameraMotionOffset = FVector::ZeroVector;
	FRotator AppliedCameraMotionRotation = FRotator::ZeroRotator;
	FVector CameraMotionOffsetVelocity = FVector::ZeroVector;
	FVector CameraMotionRotationVelocity = FVector::ZeroVector;
};
