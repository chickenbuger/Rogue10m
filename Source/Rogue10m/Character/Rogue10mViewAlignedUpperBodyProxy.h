// View aim is evaluated in the native proxy so the existing AnimBP is evaluated once, unchanged.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "BoneIndices.h"

struct FBoneContainer;
struct FCompactPose;
class ARogue10mCharacter;

class FRogue10mViewAlignedUpperBodyProxy final : public FAnimInstanceProxy
{
public:
	explicit FRogue10mViewAlignedUpperBodyProxy(UAnimInstance* Instance);
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override;
	virtual void PostEvaluate(UAnimInstance* InAnimInstance) override;

private:
	void RebuildBoneCache(const FBoneContainer& Bones);
	void ApplyViewAim(FCompactPose& Pose);
	void ApplyMouseDrivenHead(FCompactPose& Pose);
	void CaptureHeadMotionSettings(const ARogue10mCharacter* Character, float DeltaSeconds);

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
	float TorsoWeight = 0.25f;
	uint16 BoneCacheSerial = 0;
	bool bBoneCacheValid = false;
	bool bApplyAim = false;
	bool bApplyMouseHead = false;
	bool bMouseHeadEvaluated = false;
	FCompactPoseBoneIndex Spine[3] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex UpperArm[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Hand[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex IKHand[3] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Root = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex IKHandRoot = FCompactPoseBoneIndex(INDEX_NONE);
};
