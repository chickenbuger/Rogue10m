// View aim is evaluated in the native proxy so the existing AnimBP is evaluated once, unchanged.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/PoseSnapshot.h"
#include "BoneIndices.h"

struct FBoneContainer;
struct FCompactPose;
class ARogue10mCharacter;
class UAnimSequence;
class USkeletalMeshComponent;
class URogue10mFighterCombatStanceAnimInstance;

class FRogue10mViewAlignedUpperBodyProxy final : public FAnimInstanceProxy
{
public:
	explicit FRogue10mViewAlignedUpperBodyProxy(UAnimInstance* Instance);
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override;
	virtual void PostEvaluate(UAnimInstance* InAnimInstance) override;
	void SetGroundedDefeatStartPose(FPoseSnapshot&& Pose);
	void UpdateWeaponLocomotionBlend(bool bEligible, bool bChangedWhileMoving, float DeltaSeconds);

private:
	void RebuildBoneCache(const FBoneContainer& Bones);
	void ApplyWeaponLocomotionBlend(FCompactPose& Pose);
	void ApplyViewAim(FCompactPose& Pose);
	void ApplyWeightedHandMotion(FCompactPose& Pose);
	void SyncHandIKHelpers(FCompactPose& Pose);
	void ApplyMouseDrivenHead(FCompactPose& Pose);
	bool ApplyAuthoredRoll(FPoseContext& Output);
	void ApplyRollPose(FCompactPose& Pose);
	void CaptureCapstoneBrace(const ARogue10mCharacter* Character, const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds);
	void ApplyCapstoneBrace(FPoseContext& Output);
	void CaptureEarthSplitterPose(const ARogue10mCharacter* Character, const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds);
	void ApplyEarthSplitterPose(FPoseContext& Output);
	void ApplySecondJumpPose(FPoseContext& Output);
	void CaptureSecondJumpPose(const ARogue10mCharacter& Character, const URogue10mFighterCombatStanceAnimInstance& Instance,
		const USkeletalMeshComponent& Mesh, float DeltaSeconds);
	void CaptureNonlethalHit(const ARogue10mCharacter* Character, const URogue10mFighterCombatStanceAnimInstance* Instance);
	void ApplyNonlethalHit(FPoseContext& Output);
	void BlendGroundedDefeatStartPose(FCompactPose& Pose) const;
	void CaptureHeadMotionSettings(const ARogue10mCharacter* Character, float DeltaSeconds);
	void CaptureWeightedHandMotion(const ARogue10mCharacter* Character, const URogue10mFighterCombatStanceAnimInstance* Instance, float DeltaSeconds);

	// Only plain values copied on the game thread are read during parallel evaluation.
	FQuat ActorInMesh = FQuat::Identity;
	FQuat HeadAimInMesh = FQuat::Identity;
	FQuat HeadWorldAim = FQuat::Identity;
	FQuat HeadSourceMeshRotation = FQuat::Identity;
	FQuat EvaluatedHeadWorldAim = FQuat::Identity;
	FQuat EvaluatedHeadSourceRotation = FQuat::Identity;
	FQuat ReferenceHeadRotation = FQuat::Identity;
	FQuat ReferenceNeckRotation = FQuat::Identity;
	FQuat SmoothedHeadResidual = FQuat::Identity;
	FQuat SmoothedNeckResidual = FQuat::Identity;
	float HeadRotationWeight = 0.15f;
	float HeadRotationLimitDegrees = 2.0f;
	float HeadResidualFollow = 1.0f;
	float HeadNeckAimWeight = 0.35f;
	float AimPitch = 0.0f;
	float AimYaw = 0.0f;
	// Keep the opt-out response current even while the optional slower arm aim is selected.
	float BaselineAimPitch = 0.0f;
	float BaselineAimYaw = 0.0f;
	float WeightedArmPitchDegrees[2] = { 0.0f, 0.0f };
	FVector2D EvaluatedWeightedHandPitch = FVector2D::ZeroVector;
	int32 SkippedWeightedAttackInstanceId = INDEX_NONE;
	float TorsoWeight = 0.25f;
	// The animation instance owns the sequence while the worker evaluates this snapshot.
	const UAnimSequence* RollSequence = nullptr;
	float RollSequenceLength = 0.0f;
	const UAnimSequence* CapstoneBraceSequence = nullptr;
	float CapstoneBraceTime = 0.0f;
	float CapstoneBraceBlend = 0.0f;
	float CapstoneBraceWeight = 0.0f;
	const UAnimSequence* EarthSplitterSequence = nullptr;
	float EarthSplitterTime = 0.0f;
	float EarthSplitterWeight = 0.0f;
	float EarthSplitterLowerBodyWeight = 0.0f;
	int32 EarthSplitterMontageAtChargeStart = INDEX_NONE;
	bool bEarthSplitterChargeContinuous = false;
	bool bEarthSplitterHoldObserved = false;
	bool bUseDwarfEarthSplitterGrip = false;
	const UAnimSequence* NonlethalHitSequence = nullptr;
	float NonlethalHitAge = 0.0f;
	float NonlethalHitWeight = 0.0f;
	FPoseSnapshot GroundedDefeatStartPose;
	float GroundedDefeatBlendElapsed = 0.0f;
	float RollProgress = 0.0f;
	float RollYawRadians = 0.0f;
	FVector RollRight = FVector::RightVector;
	FVector RollUp = FVector::UpVector;
	const UAnimSequence* SecondJumpSequence = nullptr;
	float SecondJumpAge = -1.0f;
	float SecondJumpInterruptionWeight = 1.0f;
	TArray<FTransform> LastGroundPose;
	TArray<FTransform> WeaponLocomotionOffsets;
	float WeaponLocomotionElapsed = 0.0f;
	uint16 GroundPoseBoneSerial = 0;
	bool bRecordGroundPose = false;
	bool bCaptureOutgoingGroundPose = false;
	uint16 BoneCacheSerial = 0;
	bool bBoneCacheValid = false;
	bool bApplyAim = false;
	bool bApplyWeightedHandMotion = false;
	bool bWeightedHandContextValid = false;
	bool bUseWeightedAimFollow = false;
	bool bWeightedHandMotionEvaluated = false;
	bool bApplyMouseHead = false;
	bool bMouseHeadEvaluated = false;
	bool bApplyRollPose = false;
	bool bLearnedRollPoseEvaluated = false;
	bool bSecondJumpPoseEvaluated = false;
	FCompactPoseBoneIndex Pelvis = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Clavicle[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Forearm[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Thigh[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Calf[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Foot[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Ball[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Spine[4] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE),
		FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex UpperArm[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Hand[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex IKHand[3] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Root = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex IKHandRoot = FCompactPoseBoneIndex(INDEX_NONE);
};
