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
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"

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
 ReferenceHeadLocation = Head.GetLocation();
 SmoothedHeadPositionOffset = FVector::ZeroVector;
 HeadPositionFrame = MAX_uint64; bHeadPositionInitialized = false;
 bEvaluatedPoseMouseHead = false;
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
  // Retargeted appearance can still hold the previous pose after its source updates.
  // Pair its actual finalized pose with the source basis used by that evaluation.
  HeadPoseFinalizedHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
   FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &URogue10mAppearanceCameraComponent::CaptureEvaluatedHeadBasis));
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

void URogue10mAppearanceCameraComponent::CaptureEvaluatedHeadBasis()
{
 // Bone finalization callbacks run on the game thread, after source PostEvaluate.
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 USkeletalMeshComponent* Source = Character ? Character->GetAnimationPlaybackMesh() : nullptr;
 const auto* Animation = Source ? Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()) : nullptr;
 EvaluatedPoseSourceMesh = Source;
 bEvaluatedPoseMouseHead = bApplied && Animation && Animation->bMouseDrivenHeadApplied;
 EvaluatedPoseWorldAim = bEvaluatedPoseMouseHead ? Animation->MouseDrivenHeadWorldAim : FQuat::Identity;
 EvaluatedPoseSourceRotation = bEvaluatedPoseMouseHead ? Animation->MouseDrivenHeadSourceRotation : FQuat::Identity;
}
bool URogue10mAppearanceCameraComponent::IsBrawlerViewStabilized() const
{
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 if (!Character || !bStabilizeBrawlerHeadMotion) { return false; }
 const ERogue10mWeaponType Weapon = Character->GetEquippedWeaponType();
 return Weapon == ERogue10mWeaponType::Unarmed || Weapon == ERogue10mWeaponType::Knuckle;
}

FRogue10mHeadMotionSettings URogue10mAppearanceCameraComponent::GetHeadMotionSettings() const
{
 FRogue10mHeadMotionSettings Settings;
 auto Safe = [](float Value, float Fallback, float Maximum)
 {
  return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, Maximum) : Fallback;
 };
 Settings.RotationWeight = Safe(HeadAnimationRotationWeight, 0.15f, 1.0f);
 Settings.InterpolationSpeed = Safe(HeadMotionInterpolationSpeed, 18.0f, 60.0f);
 Settings.NeckAimWeight = Safe(MouseHeadNeckAimWeight, 0.35f, 1.0f);
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
 const URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
 const URogue10mAttackSkillData* Skill = Combat ? Combat->GetLastAcceptedAttackSkill() : nullptr;
 const UAnimMontage* Attack = Skill ? Skill->AttackMontage.Get() : nullptr;
 const USkeletalMeshComponent* Source = Character ? Character->GetAnimationPlaybackMesh() : nullptr;
 const UAnimInstance* Animation = Source ? Source->GetAnimInstance() : nullptr;
 const FAnimMontageInstance* AttackInstance = Animation && Attack ? Animation->GetInstanceForMontage(Attack) : nullptr;
 const bool bAttackContributes = Attack && Animation && (Animation->GetCurrentActiveMontage() == Attack
  || (AttackInstance && AttackInstance->GetWeight() > UE_SMALL_NUMBER));
 const bool bCharging = Combat && Combat->IsAttackChargeHeld();
 const auto* NativeAnimation = Cast<URogue10mFighterCombatStanceAnimInstance>(Animation);
 const bool bDodgeContributes = NativeAnimation && NativeAnimation->IsViewAimDodgeActive();
 const bool bAirborne = Movement && Movement->IsFalling();
 const bool bRunning = Character && Movement && Character->IsSprinting() && Movement->Velocity.Size2D() > 10.0f;
 if (bAttackContributes || bCharging || bDodgeContributes || bAirborne)
 {
  Settings.MaxRotationDegrees = Safe(AttackingHeadMaxRotationDegrees, 3.0f, 3.0f);
  Settings.MaxPositionOffsetCm = Safe(AttackingHeadMaxPositionOffset, 1.5f, 5.0f);
 }
 else if (bRunning)
 {
  Settings.MaxRotationDegrees = Safe(RunningHeadMaxRotationDegrees, 4.0f, 4.0f);
  Settings.MaxPositionOffsetCm = Safe(RunningHeadMaxPositionOffset, 2.0f, 5.0f);
 }
 else
 {
  Settings.MaxRotationDegrees = Safe(WalkingHeadMaxRotationDegrees, 2.0f, 2.0f);
  Settings.MaxPositionOffsetCm = Safe(WalkingHeadMaxPositionOffset, 1.0f, 5.0f);
 }
 return Settings;
}

bool URogue10mAppearanceCameraComponent::ComposeCameraTransform(const FTransform& AnimatedHead,
 const FQuat& WorldHeadMotion, const FQuat& ActorRotation, const FQuat& ControlRotation,
 const FVector& Offset, float MotionWeight, float MaxMotionDegrees, FTransform& OutCamera)
{
 if (AnimatedHead.ContainsNaN() || WorldHeadMotion.ContainsNaN() || ActorRotation.ContainsNaN()
  || ControlRotation.ContainsNaN() || Offset.ContainsNaN() || !FMath::IsFinite(MotionWeight)
  || !FMath::IsFinite(MaxMotionDegrees) || WorldHeadMotion.SizeSquared() < UE_SMALL_NUMBER
  || ActorRotation.SizeSquared() < UE_SMALL_NUMBER || ControlRotation.SizeSquared() < UE_SMALL_NUMBER) { return false; }
 const FQuat RawMotion = WorldHeadMotion.GetNormalized();
 const double Angle = RawMotion.AngularDistance(FQuat::Identity);
 const double Limit = FMath::DegreesToRadians(FMath::Clamp(MaxMotionDegrees, 0.0f, 180.0f));
 const double Weight = Angle > UE_SMALL_NUMBER
  ? FMath::Min(static_cast<double>(FMath::Clamp(MotionWeight, 0.0f, 1.0f)), Limit / Angle)
  : 0.0;
 const FQuat ViewMotion = FQuat::Slerp(FQuat::Identity, RawMotion, Weight).GetNormalized();
 // Keep the actual animated head position. Only the eye's orientation and its small
 // offset are stabilized; no body pose, root movement or mouse input is filtered.
 const FQuat HeadForward = (ViewMotion * ActorRotation.GetNormalized()).GetNormalized();
 OutCamera = FTransform((ViewMotion * ControlRotation.GetNormalized()).GetNormalized(),
  AnimatedHead.GetLocation() + HeadForward.RotateVector(Offset));
 return !OutCamera.ContainsNaN();
}

bool URogue10mAppearanceCameraComponent::ReadCameraTransform(FTransform& OutCamera) const
{
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 FTransform Head;
 FQuat Motion;
 if (!bApplied || !Character || !ReadAnimatedHead(Head, Motion)) { return false; }
 const USkeletalMeshComponent* Source = Character->GetAnimationPlaybackMesh();
 const auto* SourceAnimation = Source ? Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()) : nullptr;
 // For a direct source body, ReadAnimatedHead has already waited for that very pose.
 // A separate appearance uses only its finalized-pose cache, never a newer source pose.
 const bool bDirectSourceBody = Source && Source == AppliedMesh.Get();
 const USkeletalMeshComponent* PoseSource = bDirectSourceBody ? Source : EvaluatedPoseSourceMesh.Get();
 const bool bPoseMouseHead = bDirectSourceBody
  ? SourceAnimation && SourceAnimation->bMouseDrivenHeadApplied : bEvaluatedPoseMouseHead;
 const FQuat PoseWorldAim = bDirectSourceBody && SourceAnimation ? SourceAnimation->MouseDrivenHeadWorldAim : EvaluatedPoseWorldAim;
 const FQuat PoseSourceRotation = bDirectSourceBody && SourceAnimation ? SourceAnimation->MouseDrivenHeadSourceRotation : EvaluatedPoseSourceRotation;
 const bool bMouseHead = PoseSource && bPoseMouseHead && !PoseWorldAim.ContainsNaN() && !PoseSourceRotation.ContainsNaN()
  && PoseWorldAim.SizeSquared() > UE_SMALL_NUMBER && PoseSourceRotation.SizeSquared() > UE_SMALL_NUMBER;
 if (bMouseHead)
 {
  // Local bone poses can remain unchanged while actor/component rotation changes.
  // Transport the evaluated aim to that same current component space before removal.
  const FQuat Transport = (PoseSource->GetComponentQuat() * PoseSourceRotation.Inverse()).GetNormalized();
  const FQuat CurrentPoseAim = (Transport * PoseWorldAim * Transport.Inverse()).GetNormalized();
  Motion = (Motion * CurrentPoseAim.Inverse()).GetNormalized();
 }
 if (bStabilizeAllHeadMotion)
 {
  const FRogue10mHeadMotionSettings Settings = GetHeadMotionSettings();
  const USkeletalMeshComponent* Mesh = AppliedMesh.Get();
  if (!Mesh) { return false; }
  const FTransform MeshWorld = Mesh->GetComponentTransform();
  const FVector RawOffset = MeshWorld.InverseTransformPosition(Head.GetLocation()) - ReferenceHeadLocation;
  const FVector DesiredOffset = RawOffset.GetClampedToMaxSize(Settings.MaxPositionOffsetCm);
  if (HeadPositionFrame != GFrameCounter)
  {
   const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
   const float Follow = Settings.InterpolationSpeed > 0.0f && FMath::IsFinite(Delta)
    ? 1.0f - FMath::Exp(-Settings.InterpolationSpeed * FMath::Max(Delta, 0.0f)) : 1.0f;
   SmoothedHeadPositionOffset = bHeadPositionInitialized
    ? FMath::Lerp(SmoothedHeadPositionOffset, DesiredOffset, Follow) : DesiredOffset;
   bHeadPositionInitialized = true; HeadPositionFrame = GFrameCounter;
  }
  SmoothedHeadPositionOffset = SmoothedHeadPositionOffset.GetClampedToMaxSize(Settings.MaxPositionOffsetCm);
  // The mesh/capsule world travel is preserved, including jumps. Only local head motion
  // is bounded around the neutral head anchor; skeletal translations remain untouched.
  Head.SetLocation(MeshWorld.TransformPosition(ReferenceHeadLocation + SmoothedHeadPositionOffset));
  if (!ComposeCameraTransform(Head, Motion, Character->GetActorQuat(),
   Character->GetControlRotation().Quaternion(), EyeOffset,
   bMouseHead ? 1.0f : Settings.RotationWeight, Settings.MaxRotationDegrees, OutCamera)) { return false; }
  // The eye offset follows the mouse-driven head, too. Capsule travel and current
  // mouse rotation are never interpolated by the local animation-position filter.
  OutCamera.SetLocation(Head.GetLocation() + OutCamera.GetRotation().RotateVector(EyeOffset));
  return !OutCamera.ContainsNaN();
 }
 const bool bStabilize = IsBrawlerViewStabilized();
 return ComposeCameraTransform(Head, Motion, Character->GetActorQuat(),
  Character->GetControlRotation().Quaternion(), EyeOffset,
  bMouseHead ? 1.0f : (bStabilize ? BrawlerHeadRotationWeight : 1.0f),
  bStabilize ? BrawlerHeadMaxRotationDegrees : 180.0f, OutCamera);
}

void URogue10mAppearanceCameraComponent::UpdateCameraFromAppearance()
{
 RefreshAppearanceCamera();
 auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 FTransform View;
 if (!Character || !ReadCameraTransform(View)) { return; }
 Character->GetFirstPersonCameraComponent()->SetWorldLocationAndRotation(View.GetLocation(), View.GetRotation());
}
void URogue10mAppearanceCameraComponent::Restore()
{
 if (USkeletalMeshComponent* Mesh = AppliedMesh.Get(); Mesh && HeadPoseFinalizedHandle.IsValid())
 {
  Mesh->UnregisterOnBoneTransformsFinalizedDelegate(HeadPoseFinalizedHandle);
 }
 HeadPoseFinalizedHandle.Reset(); EvaluatedPoseSourceMesh.Reset(); bEvaluatedPoseMouseHead = false;
 EvaluatedPoseWorldAim = EvaluatedPoseSourceRotation = FQuat::Identity;
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
 SmoothedHeadPositionOffset = FVector::ZeroVector; HeadPositionFrame = MAX_uint64; bHeadPositionInitialized = false;
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
