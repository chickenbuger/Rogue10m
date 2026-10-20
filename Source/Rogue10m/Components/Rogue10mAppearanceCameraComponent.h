// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mAppearanceCameraComponent.generated.h"

class URogue10mAppearanceShadowComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USceneComponent;
struct FMinimalViewInfo;

/** One game-thread policy shared by the evaluated head and its first-person camera. */
struct FRogue10mHeadMotionSettings
{
 float RotationWeight = 0.15f;
 float MaxRotationDegrees = 2.0f;
 float MaxPositionOffsetCm = 1.0f;
 float InterpolationSpeed = 18.0f;
 float NeckAimWeight = 0.35f;
};

/** Uses the visible character's evaluated pose, without a separate owner-only animation rig. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mAppearanceCameraComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URogue10mAppearanceCameraComponent();
 UFUNCTION(BlueprintCallable, Category="Rogue10m|Appearance Camera")
 void RefreshAppearanceCamera();
 /** Called before CalcCamera and before a gameplay camera trace. Never evaluates or changes the animation pose. */
 void UpdateCameraFromAppearance();
 UFUNCTION(BlueprintPure, Category="Rogue10m|Appearance Camera")
 bool IsAppearanceCameraActive() const { return bApplied; }
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera")
 bool bEnableAppearanceCamera = true;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera")
 FName HeadBone = TEXT("head");
 /** Eye offset in head forward/right/up axes. Stable view rotates it with current mouse aim. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera", meta=(Units="cm"))
 FVector EyeOffset = FVector(8.0f, 0.0f, -3.0f);
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera")
 bool bHideHeadForOwner = true;
 /** Draw the full appearance pose in shadows while the owner camera hides the head. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Shadow")
 bool bEnableFullBodyShadow = true;
 bool ReadAnimatedHead(FTransform& OutWorldHead, FQuat& OutWorldMotion) const;
 /** Initial local view only; later refreshes never overwrite mouse aim. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera", meta=(ClampMin="-60", ClampMax="0", Units="deg"))
 float InitialViewPitch = -20.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Brawler Stability")
 bool bStabilizeBrawlerHeadMotion = true;
 /** Legacy fallback when all-weapon head stability is disabled; never weights mouse input. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Brawler Stability", meta=(ClampMin="0", ClampMax="1"))
 float BrawlerHeadRotationWeight = 0.15f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Brawler Stability", meta=(ClampMin="0", ClampMax="180", Units="deg"))
 float BrawlerHeadMaxRotationDegrees = 8.0f;
 bool IsBrawlerViewStabilized() const;
 /** Attack pose contribution, including blend-out, takes precedence over locomotion. */
 FRogue10mHeadMotionSettings GetHeadMotionSettings() const;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability")
 bool bStabilizeAllHeadMotion = true;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="1"))
 float HeadAnimationRotationWeight = 0.15f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="2", Units="deg"))
 float WalkingHeadMaxRotationDegrees = 2.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="4", Units="deg"))
 float RunningHeadMaxRotationDegrees = 4.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="3", Units="deg"))
 float AttackingHeadMaxRotationDegrees = 3.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="5", Units="cm"))
 float WalkingHeadMaxPositionOffset = 1.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="5", Units="cm"))
 float RunningHeadMaxPositionOffset = 2.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="5", Units="cm"))
 float AttackingHeadMaxPositionOffset = 1.5f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="60"))
 float HeadMotionInterpolationSpeed = 18.0f;
 /** Share mouse turning with the neck while the final head still follows the full view angle. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Head Stability", meta=(ClampMin="0", ClampMax="1"))
 float MouseHeadNeckAimWeight = 0.35f;
 bool ReadCameraTransform(FTransform& OutCamera) const;
 /** Stateless composition: repeated camera/trace calls cannot integrate motion twice. */
 static bool ComposeCameraTransform(const FTransform& AnimatedHead, const FQuat& WorldHeadMotion,
  const FQuat& ActorRotation, const FQuat& ControlRotation, const FVector& Offset,
  float MotionWeight, float MaxMotionDegrees, FTransform& OutCamera);
 /** Inspection changes only the rendered POV; the head camera remains the combat trace source. */
 UFUNCTION(BlueprintCallable, Category="Rogue10m|Appearance Camera|Inspection")
 void ToggleThirdPersonInspection();
 UFUNCTION(BlueprintPure, Category="Rogue10m|Appearance Camera|Inspection")
 bool IsThirdPersonInspectionActive() const { return bApplied && bThirdPersonInspection; }
 bool ApplyInspectionView(FMinimalViewInfo& OutPOV);
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Inspection", meta=(ClampMin="0", ClampMax="1000", Units="cm"))
 float InspectionDistance = 300.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Inspection", meta=(ClampMin="-200", ClampMax="200", Units="cm"))
 float InspectionPivotHeight = 0.0f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Inspection", meta=(ClampMin="1", ClampMax="50", Units="cm"))
 float InspectionCollisionRadius = 12.0f;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 UPROPERTY(Transient)
 TObjectPtr<URogue10mAppearanceShadowComponent> ShadowComponent;
 void Restore();
 void RefreshInspectionParts();
 void RestoreInspectionParts();
 void CaptureEvaluatedHeadBasis();
 bool CacheHeadReference(USkeletalMeshComponent& Mesh);
 TWeakObjectPtr<USkeletalMeshComponent> AppliedMesh;
 TWeakObjectPtr<USkeletalMesh> ReferenceMesh;
 TWeakObjectPtr<USceneComponent> SavedCameraParent;
 FName SavedCameraSocket;
 FName CachedHeadBone;
 FName HiddenHeadBone;
 FTransform SavedCameraTransform;
 FQuat ReferenceHeadRotation = FQuat::Identity;
 FVector ReferenceHeadLocation = FVector::ZeroVector;
 FDelegateHandle HeadPoseFinalizedHandle;
 TWeakObjectPtr<USkeletalMeshComponent> EvaluatedPoseSourceMesh;
 FQuat EvaluatedPoseWorldAim = FQuat::Identity;
 FQuat EvaluatedPoseSourceRotation = FQuat::Identity;
 bool bEvaluatedPoseMouseHead = false;
 // Queries may happen more than once for traces and rendering. Only one update per frame.
 mutable FVector SmoothedHeadPositionOffset = FVector::ZeroVector;
 mutable uint64 HeadPositionFrame = MAX_uint64;
 mutable bool bHeadPositionInitialized = false;
 int32 HeadIndex = INDEX_NONE;
 uint8 SavedPrimitiveType = 0;
 uint8 SavedVisibilityTickOption = 0;
 TMap<TWeakObjectPtr<USkeletalMeshComponent>, bool> SavedPartOwnerNoSee;
 bool bThirdPersonInspection = false;
 bool bApplied = false;
 bool bHidHead = false;
 bool bSavedBodyVisible = true;
 bool bSavedBodyHidden = false;
 bool bSavedBodyOwnerNoSee = false;
 bool bSavedBodyOnlyOwnerSee = false;
 bool bSavedCameraControl = true;
 bool bSavedCameraAbsoluteLocation = false;
 bool bSavedCameraAbsoluteRotation = false;
 bool bSavedCameraAbsoluteScale = false;
 bool bSavedCameraFirstPersonFOV = false;
 bool bSavedCameraFirstPersonScale = false;
 bool bSavedArmsVisible = true;
 bool bSavedArmsHidden = false;
 bool bSavedArmsTick = true;
 bool bSavedArmsPause = false;
};
