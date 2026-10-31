// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mOwnerArmsComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
class UPoseableMeshComponent;

/** Project-owned arms geometry derived from an unchanged full appearance asset. */
USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mOwnerArmsVariant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TSoftObjectPtr<USkeletalMesh> ArmsMesh;
};

/** Owner-only geometry follower. Never owns animation, gameplay, body visibility or a camera. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mOwnerArmsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mOwnerArmsComponent();

	/** Body must belong to this locally controlled character. False leaves the proxy hidden. */
	bool Refresh(USkeletalMeshComponent* Body, bool bEnable);
	virtual void Deactivate() override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Owner Arms")
	USkinnedMeshComponent* GetOwnerArmsMesh() const;

	/** Called after the actual appearance camera is composed. Does not query or change any camera. */
	bool UpdatePresentation(const FTransform& View);
	bool IsViewFramingActive() const { return bFramingActive; }
	bool HasCapturedNeutralPose() const { return bNeutralCaptured; }

	/** Prototype, separate from the legacy leader-pose path. Product owner-view remains opt-in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	bool bEnableViewFraming = false;
	/** Camera-space center of the upper-arm cut roots, in cm. Keep behind/below the visible frustum. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector ShoulderAnchorView = FVector(5.0, 0.0, -40.0);
	/** Camera-space ready wrist targets. These are prototype tuning, not gameplay reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector LeftReadyHandView = FVector(45.0, -22.0, -24.0);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector RightReadyHandView = FVector(45.0, 22.0, -24.0);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector LeftElbowHintView = FVector(10.0, -48.0, -60.0);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector RightElbowHintView = FVector(10.0, 48.0, -60.0);
	/** Adds evaluated, de-aimed wrist displacement to the ready target; never changes montage time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing", meta=(ClampMin="0", ClampMax="1"))
	float AuthoredMotionScale = 0.65f;
	/** Camera-forward hand-goal range in cm; sorted and limited to [10,150] at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector2D HandGoalDepthRangeCm = FVector2D(25.0, 90.0);
	/** Absolute lateral goal range; left stays negative and right positive. Limited to [0,80] cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector2D HandGoalSideRangeCm = FVector2D(12.0, 42.0);
	/** Camera-up goal range in cm; sorted and limited to [-100,0] to retain a lower-screen stance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing")
	FVector2D HandGoalHeightRangeCm = FVector2D(-38.0, -14.0);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms|Framing", meta=(ClampMin="0.05", ClampMax="1"))
	float NeutralCaptureDelaySeconds = 0.15f;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Owner Arms")
	bool IsOwnerArmsActive() const { return IsActive() && OwnerArmsMesh != nullptr && AppliedBody.IsValid(); }

	/** Explicit soft references retain the derivatives for cooking and future appearance variants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TArray<FRogue10mOwnerArmsVariant> AssetVariants;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	USkeletalMesh* ResolveArmsMesh(USkeletalMesh& BodyAsset);
	bool HasMatchingReferencePose(const USkeletalMesh& BodyAsset, const USkeletalMesh& ArmsAsset) const;
	bool EnsureProxy();
	bool EnsureFramedProxy();
	void ShowLeaderFallback();
	void ResetFramingBasis(USkeletalMeshComponent& Body);
	void SyncMaterials(const USkeletalMeshComponent& Body);
	void WarnOnce(const FString& Key, const FString& Reason);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OwnerArmsMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> FramedArmsMesh;

	TWeakObjectPtr<USkeletalMeshComponent> FramingBasisBody;
	TWeakObjectPtr<USkeletalMesh> FramingBasisAsset;
	FVector NeutralHandInTorso[2];
	FQuat ReferenceTorsoInActor = FQuat::Identity;
	FTransform LastFramingView, LastFramingBody;
	FVector LastShoulderAnchor, LastReady[2], LastElbow[2];
	float LastMotionScale = -1.f;
	FVector2D LastGoalDepthRange, LastGoalSideRange, LastGoalHeightRange;
	double IdleCaptureStart = -1.;
	uint32 LastFramingRevision = MAX_uint32;
	int32 TorsoBone = INDEX_NONE, UpperBone[2]{INDEX_NONE,INDEX_NONE}, LowerBone[2]{INDEX_NONE,INDEX_NONE}, HandBone[2]{INDEX_NONE,INDEX_NONE};
	bool bNeutralCaptured = false, bFramingActive = false, bFramingChainValid = false;
	TWeakObjectPtr<USkeletalMeshComponent> AppliedBody;
	TWeakObjectPtr<USkeletalMesh> ResolvedBodyAsset;
	TWeakObjectPtr<USkeletalMesh> ResolvedArmsAsset;
	FSoftObjectPath ResolvedArmsPath;
	TSet<FString> ReportedWarnings;
	bool bResolutionAttempted = false;
};
