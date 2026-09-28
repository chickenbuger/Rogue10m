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
 /** Eye offset in the neutral character's forward/right/up axes, rotated by the animated head. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera", meta=(Units="cm"))
 FVector EyeOffset = FVector(8.0f, 0.0f, 4.0f);
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera")
 bool bHideHeadForOwner = true;
 /** Draw the full appearance pose in shadows while the owner camera hides the head. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Appearance Camera|Shadow")
 bool bEnableFullBodyShadow = true;
 bool ReadAnimatedHead(FTransform& OutWorldHead, FQuat& OutWorldMotion) const;
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
 bool CacheHeadReference(USkeletalMeshComponent& Mesh);
 TWeakObjectPtr<USkeletalMeshComponent> AppliedMesh;
 TWeakObjectPtr<USkeletalMesh> ReferenceMesh;
 TWeakObjectPtr<USceneComponent> SavedCameraParent;
 FName SavedCameraSocket;
 FName CachedHeadBone;
 FName HiddenHeadBone;
 FTransform SavedCameraTransform;
 FQuat ReferenceHeadRotation = FQuat::Identity;
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
