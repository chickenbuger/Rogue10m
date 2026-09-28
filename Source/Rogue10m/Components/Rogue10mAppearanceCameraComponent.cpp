// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAppearanceShadowComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFirstPersonPresentationComponent.h"

URogue10mAppearanceCameraComponent::URogue10mAppearanceCameraComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
}
void URogue10mAppearanceCameraComponent::BeginPlay()
{
 Super::BeginPlay();
 if (auto* Character = Cast<ARogue10mCharacter>(GetOwner()))
 {
  if (auto* Presentation = Character->GetFirstPersonPresentationComponent()) { Presentation->RefreshPresentation(); }
 }
}
void URogue10mAppearanceCameraComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 Restore();
 Super::EndPlay(Reason);
}
bool URogue10mAppearanceCameraComponent::CacheHeadReference(USkeletalMeshComponent& Mesh)
{
 USkeletalMesh* Asset = Mesh.GetSkeletalMeshAsset();
 if (ReferenceMesh.Get() == Asset && CachedHeadBone == HeadBone && HeadIndex != INDEX_NONE) { return true; }
 if (bHidHead && AppliedMesh.IsValid()) { AppliedMesh->UnHideBoneByName(HiddenHeadBone); bHidHead = false; }
 ReferenceMesh = Asset;
 CachedHeadBone = HeadBone;
 HeadIndex = Asset ? Asset->GetRefSkeleton().FindBoneIndex(HeadBone) : INDEX_NONE;
 if (HeadIndex == INDEX_NONE) { return false; }
 const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
 const TArray<FTransform>& Pose = Skeleton.GetRefBonePose();
 FTransform Head = Pose[HeadIndex];
 for (int32 Parent = Skeleton.GetParentIndex(HeadIndex); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent)) { Head *= Pose[Parent]; }
 ReferenceHeadRotation = Head.GetRotation().GetNormalized();
 return !Head.ContainsNaN();
}
void URogue10mAppearanceCameraComponent::RefreshAppearanceCamera()
{
 auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
 UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
 const APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
 if (!bEnableAppearanceCamera || !Character || !Character->IsLocallyControlled() || Character->IsDead() || !Controller
  || Controller->GetViewTarget() != Character || !Mesh || !Camera)
 {
  Restore();
  return;
 }
 if (bApplied && AppliedMesh.Get() != Mesh) { Restore(); }
 if (!CacheHeadReference(*Mesh)) { Restore(); return; }
 if (!bApplied)
 {
  AppliedMesh = Mesh;
  bSavedBodyVisible = Mesh->IsVisible(); bSavedBodyHidden = Mesh->bHiddenInGame;
  bSavedBodyOwnerNoSee = Mesh->bOwnerNoSee; bSavedBodyOnlyOwnerSee = Mesh->bOnlyOwnerSee;
  SavedPrimitiveType = static_cast<uint8>(Mesh->FirstPersonPrimitiveType);
  SavedVisibilityTickOption = static_cast<uint8>(Mesh->VisibilityBasedAnimTickOption);
  SavedCameraParent = Camera->GetAttachParent(); SavedCameraSocket = Camera->GetAttachSocketName();
  SavedCameraTransform = Camera->GetRelativeTransform();
  bSavedCameraAbsoluteLocation = Camera->IsUsingAbsoluteLocation();
  bSavedCameraAbsoluteRotation = Camera->IsUsingAbsoluteRotation();
  bSavedCameraAbsoluteScale = Camera->IsUsingAbsoluteScale();
  bSavedCameraControl = Camera->bUsePawnControlRotation;
  bSavedCameraFirstPersonFOV = Camera->bEnableFirstPersonFieldOfView;
  bSavedCameraFirstPersonScale = Camera->bEnableFirstPersonScale;
  if (auto* Arms = Character->GetFirstPersonMesh())
  {
   bSavedArmsVisible = Arms->IsVisible(); bSavedArmsHidden = Arms->bHiddenInGame;
   bSavedArmsTick = Arms->IsComponentTickEnabled(); bSavedArmsPause = Arms->bPauseAnims;
  }
  bApplied = true;
 }
 Mesh->SetOwnerNoSee(false); Mesh->SetOnlyOwnerSee(false);
 Mesh->SetVisibility(true, false); Mesh->SetHiddenInGame(false, false);
 Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
 // Hidden bones must still be evaluated; only rendering hides the head, never the camera source pose.
 Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 const bool bShouldHideHead = bHideHeadForOwner && !bThirdPersonInspection;
 if (bShouldHideHead && !Mesh->IsBoneHiddenByName(HeadBone))
 {
  Mesh->HideBoneByName(HeadBone, EPhysBodyOp::PBO_None); HiddenHeadBone = HeadBone; bHidHead = true;
 }
 else if (!bShouldHideHead && bHidHead) { Mesh->UnHideBoneByName(HiddenHeadBone); bHidHead = false; }
 if (auto* Arms = Character->GetFirstPersonMesh())
 {
  Arms->SetVisibility(false, false); Arms->SetHiddenInGame(true, false);
  Arms->bPauseAnims = true; Arms->SetComponentTickEnabled(false);
 }
 if (Camera->GetAttachParent() != Mesh || Camera->GetAttachSocketName() != HeadBone)
 {
  Camera->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, HeadBone);
 }
 // Hidden-head rendering collapses the socket scale. The unmodified local pose below supplies
 // the same animated head transform while keeping a real head attachment in the hierarchy.
 Camera->SetAbsolute(true, true, true);
 Camera->SetWorldScale3D(FVector::OneVector);
 Camera->bUsePawnControlRotation = false;
 Camera->bEnableFirstPersonFieldOfView = false; Camera->bEnableFirstPersonScale = false;
 RefreshInspectionParts();
 const bool bNeedsFullShadow = bShouldHideHead && bEnableFullBodyShadow;
 if (bNeedsFullShadow && !ShadowComponent)
 {
  ShadowComponent = NewObject<URogue10mAppearanceShadowComponent>(Character, NAME_None, RF_Transient);
  Character->AddInstanceComponent(ShadowComponent);
  ShadowComponent->RegisterComponent();
 }
 if (ShadowComponent) { ShadowComponent->RefreshShadow(Mesh, bNeedsFullShadow); }
}
bool URogue10mAppearanceCameraComponent::ReadAnimatedHead(FTransform& OutWorldHead, FQuat& OutWorldMotion) const
{
 USkeletalMeshComponent* Mesh = AppliedMesh.Get();
 USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
 if (!bApplied || !Asset || Asset != ReferenceMesh.Get() || HeadIndex == INDEX_NONE) { return false; }
 // This accessor waits for any parallel evaluation. BoneSpaceTransforms precede visibility scaling.
 const TArrayView<const FTransform> Pose = Mesh->GetBoneSpaceTransformsView();
 if (!Pose.IsValidIndex(HeadIndex)) { return false; }
 const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
 FTransform Head = Pose[HeadIndex];
 for (int32 Parent = Skeleton.GetParentIndex(HeadIndex); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
 {
  if (!Pose.IsValidIndex(Parent)) { return false; }
  Head *= Pose[Parent];
 }
 if (Head.ContainsNaN() || Mesh->GetComponentTransform().ContainsNaN()) { return false; }
 const FQuat MeshWorld = Mesh->GetComponentQuat();
 const FQuat Delta = (Head.GetRotation() * ReferenceHeadRotation.Inverse()).GetNormalized();
 OutWorldMotion = (MeshWorld * Delta * MeshWorld.Inverse()).GetNormalized();
 OutWorldHead = Head * Mesh->GetComponentTransform();
 return !OutWorldMotion.ContainsNaN() && !OutWorldHead.ContainsNaN();
}
void URogue10mAppearanceCameraComponent::UpdateCameraFromAppearance()
{
 RefreshAppearanceCamera();
 auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 if (!bApplied || !Character || EyeOffset.ContainsNaN()) { return; }
 FTransform Head;
 FQuat Motion;
 if (!ReadAnimatedHead(Head, Motion)) { return; }
 const FQuat Control = Character->GetControlRotation().Quaternion();
 if (Control.ContainsNaN()) { return; }
 const FQuat HeadForward = (Motion * Character->GetActorQuat()).GetNormalized();
 Character->GetFirstPersonCameraComponent()->SetWorldLocationAndRotation(
  Head.GetLocation() + HeadForward.RotateVector(EyeOffset), (Motion * Control).GetNormalized());
}
void URogue10mAppearanceCameraComponent::Restore()
{
 if (ShadowComponent) { ShadowComponent->RestoreShadow(); }
 RestoreInspectionParts();
 bThirdPersonInspection = false;
 if (!bApplied) { return; }
 auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 if (auto* Mesh = AppliedMesh.Get())
 {
  if (bHidHead) { Mesh->UnHideBoneByName(HiddenHeadBone); }
  Mesh->SetOwnerNoSee(bSavedBodyOwnerNoSee); Mesh->SetOnlyOwnerSee(bSavedBodyOnlyOwnerSee);
  Mesh->SetVisibility(bSavedBodyVisible, false); Mesh->SetHiddenInGame(bSavedBodyHidden, false);
  Mesh->SetFirstPersonPrimitiveType(static_cast<EFirstPersonPrimitiveType>(SavedPrimitiveType));
  Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(SavedVisibilityTickOption);
 }
 if (Character)
 {
  if (auto* Camera = Character->GetFirstPersonCameraComponent())
  {
   Camera->SetAbsolute(bSavedCameraAbsoluteLocation, bSavedCameraAbsoluteRotation, bSavedCameraAbsoluteScale);
   if (SavedCameraParent.IsValid()) { Camera->AttachToComponent(SavedCameraParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, SavedCameraSocket); }
   else { Camera->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform); }
   Camera->SetRelativeTransform(SavedCameraTransform);
   Camera->bUsePawnControlRotation = bSavedCameraControl;
   Camera->bEnableFirstPersonFieldOfView = bSavedCameraFirstPersonFOV;
   Camera->bEnableFirstPersonScale = bSavedCameraFirstPersonScale;
  }
  if (auto* Arms = Character->GetFirstPersonMesh())
  {
   Arms->SetVisibility(bSavedArmsVisible, false); Arms->SetHiddenInGame(bSavedArmsHidden, false);
   Arms->bPauseAnims = bSavedArmsPause; Arms->SetComponentTickEnabled(bSavedArmsTick);
  }
 }
 AppliedMesh.Reset(); ReferenceMesh.Reset(); HeadIndex = INDEX_NONE; bApplied = false; bHidHead = false;
}

void URogue10mAppearanceCameraComponent::ToggleThirdPersonInspection()
{
 RefreshAppearanceCamera();
 if (!bApplied) { return; }
 bThirdPersonInspection = !bThirdPersonInspection;
 RefreshAppearanceCamera();
}

void URogue10mAppearanceCameraComponent::RefreshInspectionParts()
{
 if (!IsThirdPersonInspectionActive()) { RestoreInspectionParts(); return; }
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 USkeletalMeshComponent* Body = AppliedMesh.Get();
 if (!Character || !Body) { return; }
 // Only direct body cosmetics are changed. The disabled First Person Mesh is never revealed.
 for (USceneComponent* Child : Body->GetAttachChildren())
 {
  auto* Part = Cast<USkeletalMeshComponent>(Child);
  if (!Part || Part == Character->GetFirstPersonMesh()) { continue; }
  const TWeakObjectPtr<USkeletalMeshComponent> Key(Part);
  if (!SavedPartOwnerNoSee.Contains(Key)) { SavedPartOwnerNoSee.Add(Key, Part->bOwnerNoSee); }
  Part->SetOwnerNoSee(false);
 }
}

void URogue10mAppearanceCameraComponent::RestoreInspectionParts()
{
 for (const auto& Entry : SavedPartOwnerNoSee)
 {
  if (USkeletalMeshComponent* Part = Entry.Key.Get()) { Part->SetOwnerNoSee(Entry.Value); }
 }
 SavedPartOwnerNoSee.Reset();
}

bool URogue10mAppearanceCameraComponent::ApplyInspectionView(FMinimalViewInfo& OutPOV)
{
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 UWorld* World = GetWorld();
 if (!IsThirdPersonInspectionActive() || !Character || !World) { return false; }
 const FRotator Rotation = Character->GetControlRotation();
 if (Rotation.ContainsNaN()) { return false; }
 const float Distance = FMath::IsFinite(InspectionDistance) ? FMath::Clamp(InspectionDistance, 0.0f, 1000.0f) : 300.0f;
 const float Height = FMath::IsFinite(InspectionPivotHeight) ? FMath::Clamp(InspectionPivotHeight, -200.0f, 200.0f) : 0.0f;
 const float Radius = FMath::IsFinite(InspectionCollisionRadius) ? FMath::Clamp(InspectionCollisionRadius, 1.0f, 50.0f) : 12.0f;
 const FVector Pivot = Character->GetCapsuleComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, Height);
 const FVector Desired = Pivot - Rotation.Vector() * Distance;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(AppearanceInspectionCamera), false, Character);
 FHitResult Hit;
 const bool bBlocked = World->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity,
  ECC_Camera, FCollisionShape::MakeSphere(Radius), Query);
 OutPOV.Location = bBlocked ? (Hit.bStartPenetrating ? Pivot : Hit.Location) : Desired;
 OutPOV.Rotation = Rotation;
 return true;
}