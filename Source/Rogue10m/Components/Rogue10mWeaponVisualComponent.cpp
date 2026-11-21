#include "Rogue10mWeaponVisualComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#endif
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Rogue10mCharacter.h"
#include "Rogue10m.h"
#include "Animation/AnimMontage.h"
#include "Animation/PoseSnapshot.h"
#include "BoneContainer.h"
#include "Math/RotationMatrix.h"
#include "Rogue10mWeaponRetargetAnimInstance.h"

namespace
{
#if WITH_EDITOR
TAutoConsoleVariable<bool> CVarRollSwordFloorCorrection(TEXT("a.Rogue10m.RollSwordFloorCorrection"), true,
 TEXT("Compare the stock learned-roll weapon presentation in isolated editor animation QA."));
TAutoConsoleVariable<bool> CVarShortRollSwordEntry(TEXT("a.Rogue10m.ShortRollSwordEntry"), false,
 TEXT("Compare the earlier end of the two-hand to carry blend in isolated editor animation QA."));
#endif
FQuat KeepRollBladeAboveFloor(const UStaticMeshComponent& Sword, const FVector& FloorPoint, const FVector& FloorNormal,
 const FTransform& Body, const FTransform& Hand, const FVector& Palm, const FQuat& Desired, const FVector& Blade)
{
 const UStaticMesh* Mesh = Sword.GetStaticMesh();
 if (!Mesh) { return Desired; }

 const FVector FlatBlade = FVector::VectorPlaneProject(Blade, FloorNormal).GetSafeNormal();
 if (FlatBlade.IsNearlyZero()) { return Desired; }

 const FBox LocalBounds = Mesh->GetBoundingBox();
 const FVector Scale = (Hand * Body).GetScale3D() * Sword.GetRelativeScale3D();
 const double PalmHeight = FVector::DotProduct(Body.TransformPosition(Palm) - FloorPoint, FloorNormal);
 const auto Clearance = [&](const FQuat& Rotation)
 {
  const FVector NormalInSword = (Body.GetRotation() * Rotation).UnrotateVector(FloorNormal) * Scale;
  return PalmHeight + FVector::DotProduct(LocalBounds.GetCenter(), NormalInSword)
   - FVector::DotProduct(LocalBounds.GetExtent(), NormalInSword.GetAbs());
 };
 constexpr double ClearanceCm = 0.5;
 const double DesiredClearance = Clearance(Desired);
 if (DesiredClearance >= ClearanceCm) { return Desired; }

 // Use the same carry heading along the floor when the existing grip would cross it.
 const FQuat Safe = Body.GetRotation().Inverse()
  * FRotationMatrix::MakeFromZY(FlatBlade + FloorNormal * 0.05, FloorNormal).ToQuat();
 const double SafeClearance = Clearance(Safe);
 if (SafeClearance < ClearanceCm) { return SafeClearance > DesiredClearance ? Safe : Desired; }

 double Lower = 0.0;
 double Upper = 1.0;
 for (int32 Step = 0; Step < 8; ++Step)
 {
  const double Middle = (Lower + Upper) * 0.5;
  if (Clearance(FQuat::Slerp(Desired, Safe, Middle)) >= ClearanceCm) { Upper = Middle; }
  else { Lower = Middle; }
 }
 return FQuat::Slerp(Desired, Safe, Upper).GetNormalized();
}
}

bool URogue10mWeaponVisualComponent::GetRollSwordFloorPlane(const ARogue10mCharacter& Character,
 FVector& FloorPoint, FVector& FloorNormal)
{
 const auto* Movement = Character.GetCharacterMovement();
 const auto* Capsule = Character.GetCapsuleComponent();
 if (!Movement || !Capsule || !Movement->IsMovingOnGround() || !Movement->CurrentFloor.IsWalkableFloor()) { return false; }

 const FHitResult& Hit = Movement->CurrentFloor.HitResult;
 FloorPoint = Hit.ImpactPoint;
 FloorNormal = Hit.ImpactNormal.GetSafeNormal();
 const FVector CapsuleBottom = Capsule->GetComponentLocation() - FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
 const double FloorDistance = FVector::DotProduct(CapsuleBottom - FloorPoint, FloorNormal);
 // The previous platform can still be reported while the capsule crosses below its top.
 return !FloorNormal.IsNearlyZero() && FloorDistance >= -2.0 && FloorDistance <= Movement->MaxStepHeight + 3.0;
}

#if WITH_EDITOR
bool URogue10mWeaponVisualComponent::IsRollSwordFloorCorrectionEnabled()
{
 return CVarRollSwordFloorCorrection.GetValueOnGameThread();
}
#endif

void URogue10mWeaponVisualComponent::SwitchWeaponRetarget(USkeletalMeshComponent& Body, UClass* NewClass, bool bThirdPerson)
{
 if (bThirdPerson && NewClass->IsChildOf<URogue10mWeaponRetargetAnimInstance>() && Body.GetBoneTransformRevisionNumber() > 0)
 {
  if (const auto* OldAnim = Cast<URogue10mWeaponRetargetAnimInstance>(Body.GetAnimInstance()))
  {
   const auto& Bones = OldAnim->GetRequiredBones().GetBoneIndicesArray();
   FPoseSnapshot PreviousPose;
   if (!Bones.IsEmpty()) { Body.SnapshotPose(PreviousPose); }
   if (PreviousPose.bIsValid)
   {
    PendingRetargetBones.Init(false, PreviousPose.LocalTransforms.Num());
    for (const FBoneIndexType Bone : Bones)
    {
     if (PendingRetargetBones.IsValidIndex(Bone)) { PendingRetargetBones[Bone] = true; }
    }
    PendingRetargetPose = MoveTemp(PreviousPose.LocalTransforms);
    PendingRetargetBody = &Body;
    PendingRetargetClass = NewClass;
   }
  }
 }
 // InitAnim can immediately evaluate the new Blueprint at time zero.
 Body.SetAnimInstanceClass(NewClass);
 PendingRetargetBody.Reset();
 PendingRetargetClass.Reset();
 PendingRetargetPose.Reset();
 PendingRetargetBones.Reset();
}

bool URogue10mWeaponVisualComponent::ConsumeEquipmentRetargetPose(UAnimInstance& Instance,
 TArray<FTransform>& OutPose, TBitArray<>& OutBones)
{
 if (Instance.GetSkelMeshComponent() != PendingRetargetBody.Get() || Instance.GetClass() != PendingRetargetClass.Get()
     || PendingRetargetPose.IsEmpty()) { return false; }
 OutPose = MoveTemp(PendingRetargetPose);
 OutBones = MoveTemp(PendingRetargetBones);
 return true;
}

bool URogue10mWeaponVisualComponent::BuildSwordPalmGrip(const FVector& RightPalm, const FVector& LeftPalm, const FVector& FingerAxis, FTransform& OutGrip)
{
 if (RightPalm.ContainsNaN() || LeftPalm.ContainsNaN() || FingerAxis.ContainsNaN()) { return false; }
 FVector Z=RightPalm-LeftPalm;
 if (!FMath::IsFinite(Z.SizeSquared()) || Z.SizeSquared()<1.0 || !Z.Normalize()) { return false; }
 // Preserve the authored static grip roll: projected finger is Y, not blade-face X.
 FVector Y=FingerAxis-Z*FVector::DotProduct(FingerAxis,Z);
 if (!FMath::IsFinite(Y.SizeSquared()) || Y.SizeSquared()<1.e-6 || !Y.Normalize()) { return false; }
 const FVector X=FVector::CrossProduct(Y,Z);
 const FQuat Rotation=FRotationMatrix::MakeFromZX(Z,X).ToQuat();
 if (Rotation.ContainsNaN()) { return false; }
 OutGrip=FTransform(Rotation,RightPalm);
 return true;
}

void URogue10mWeaponVisualComponent::UnbindSwordGrip()
{
 if (GripBody.IsValid() && GripDelegate.IsValid()) { GripBody->UnregisterOnBoneTransformsFinalizedDelegate(GripDelegate); }
 GripDelegate.Reset(); GripBody.Reset(); bOwnGrip=false;
 bHasTwoHandSwordRotation = false;
 bHasRollSwordStart = false;
}

void URogue10mWeaponVisualComponent::UpdateSwordGrip()
{
 auto* Character=Cast<ARogue10mCharacter>(GetOwner());
 USkeletalMeshComponent* Body=GripBody.Get();
 if (!bOwnGrip || !Body || !Character || !RightWeaponMesh) { return; }
 if (Character->GetMesh()!=Body || Character->GetEquippedWeaponType()!=ERogue10mWeaponType::GreatSword)
 { UnbindSwordGrip(); return; }
 // A different system owns any external relative-transform or attachment change.
 if (RightWeaponMesh->GetAttachParent()!=Body || RightWeaponMesh->GetAttachSocketName()!=TEXT("hand_r")
     || !RightWeaponMesh->GetRelativeTransform().Equals(LastGrip,.0001))
 { UnbindSwordGrip(); return; }
 FTransform Relative=GreatSwordGrip;
 if (Character->IsThirdPersonGameplayActive())
 {
  bool Valid=true;
  for (const FName Bone : {FName(TEXT("hand_r")),FName(TEXT("hand_l")),FName(TEXT("middle_01_r")),FName(TEXT("middle_01_l"))})
  { Valid=Valid && Body->GetBoneIndex(Bone)!=INDEX_NONE; }
  if (Valid)
  {
   const FTransform Hand=Body->GetSocketTransform(TEXT("hand_r"),RTS_Component);
   const FVector RH=Hand.GetLocation(), LH=Body->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation();
   const FVector RF=Body->GetSocketTransform(TEXT("middle_01_r"),RTS_Component).GetLocation();
   const FVector LF=Body->GetSocketTransform(TEXT("middle_01_l"),RTS_Component).GetLocation();
   FTransform Grip;
   if (!Hand.ContainsNaN() && BuildSwordPalmGrip((RH+RF)*.5,(LH+LF)*.5,RF-RH,Grip))
   {
    const FQuat ComponentRotation = Body->GetComponentQuat();
    float RollProgress = 0.0f;
    FVector RollDirection;
    if (Character->GetRollPresentation(RollProgress, RollDirection))
    {
     // Keep the hilt in the right palm while the support hand separates from the grip.
     // Carry the blade on the rolling side with its flat face nearly parallel to the floor.
     const FVector RollSide = FVector::CrossProduct(FVector::UpVector, RollDirection);
     const FVector Blade = (RollDirection * 0.4 + RollSide * 0.9 + FVector::UpVector * 0.05).GetSafeNormal();
     const FQuat Carry = FRotationMatrix::MakeFromZY(ComponentRotation.UnrotateVector(Blade),
      ComponentRotation.UnrotateVector(FVector::UpVector)).ToQuat();
     const float CarryWeight = FMath::SmoothStep(0.03f, 0.27f, RollProgress)
      * (1.0f - FMath::SmoothStep(0.76f, 0.99f, RollProgress));
     FQuat Preferred = FQuat::Slerp(Grip.GetRotation(), Carry, CarryWeight).GetNormalized();
     bool bCorrectRollGrip = true;
#if WITH_EDITOR
     bCorrectRollGrip = IsRollSwordFloorCorrectionEnabled();
#endif
     if (bCorrectRollGrip)
     {
      if (!bHasRollSwordStart || RollProgress < LastSwordRollProgress)
      {
       RollSwordStartWorldRotation = bHasTwoHandSwordRotation ? LastTwoHandSwordWorldRotation : ComponentRotation * Grip.GetRotation();
       bHasRollSwordStart = true;
      }
      LastSwordRollProgress = RollProgress;
      // The lead hand separates early. Blend from the last complete two-hand orientation instead of the changing palm line.
      const FQuat Start = ComponentRotation.Inverse() * RollSwordStartWorldRotation;
      float CarryEntryEnd = 0.35f;
#if WITH_EDITOR
      if (CVarShortRollSwordEntry.GetValueOnGameThread()) { CarryEntryEnd = 0.27f; }
#endif
      const FQuat InCarry = FQuat::Slerp(Start, Carry, FMath::SmoothStep(0.03f, CarryEntryEnd, RollProgress));
      Preferred = FQuat::Slerp(InCarry, Grip.GetRotation(), FMath::SmoothStep(0.68f, 0.99f, RollProgress)).GetNormalized();
     }
     FVector FloorPoint, FloorNormal;
     Grip.SetRotation(bCorrectRollGrip && GetRollSwordFloorPlane(*Character, FloorPoint, FloorNormal)
      ? KeepRollBladeAboveFloor(*RightWeaponMesh, FloorPoint, FloorNormal, Body->GetComponentTransform(), Hand,
        Grip.GetLocation(), Preferred, Blade) : Preferred);
    }
    else
    {
     LastTwoHandSwordWorldRotation = ComponentRotation * Grip.GetRotation();
     bHasTwoHandSwordRotation = true;
     bHasRollSwordStart = false;
    }
    const FTransform Candidate=Grip.GetRelativeTransform(Hand);
    if (!Candidate.ContainsNaN()) { Relative=Candidate; }
   }
  }
 }
 // Preserve scale and touch only presentation location/rotation. FP retains its static grip.
 RightWeaponMesh->SetRelativeLocationAndRotation(Relative.GetLocation(),Relative.GetRotation());
 LastGrip=RightWeaponMesh->GetRelativeTransform();
}

bool URogue10mWeaponVisualComponent::BuildStaffUprightGrip(const FTransform& HandWorld, const FTransform& AuthoredGrip,
 const FVector& FacingForward, float ActionWeight, FTransform& OutGrip)
{
 if (!HandWorld.IsValid() || !AuthoredGrip.IsValid() || !FMath::IsFinite(ActionWeight)
     || FacingForward.ContainsNaN() || AuthoredGrip.GetScale3D().GetMin()<=0.) { return false; }
 FVector Forward=FVector::VectorPlaneProject(FacingForward,FVector::UpVector);
 if (!Forward.Normalize()) { return false; }
 // A stable actor-facing basis avoids the ambiguous 180-degree shortest-arc case at inverted idle.
 const FQuat Upright=FRotationMatrix::MakeFromYX(FVector::UpVector,Forward).ToQuat();
 const FQuat Authored=(HandWorld.GetRotation()*AuthoredGrip.GetRotation()).GetNormalized();
 const float Weight=FMath::Clamp(ActionWeight,0.f,1.f);
 const FQuat WorldRotation=FQuat::Slerp(Upright,Authored,Weight).GetNormalized();
 OutGrip=AuthoredGrip;
 OutGrip.SetRotation(Weight>=1.f?AuthoredGrip.GetRotation():(HandWorld.GetRotation().Inverse()*WorldRotation).GetNormalized());
 return OutGrip.IsValid();
}

void URogue10mWeaponVisualComponent::UnbindStaffGrip()
{
 if (StaffGripBody.IsValid() && StaffGripDelegate.IsValid()) { StaffGripBody->UnregisterOnBoneTransformsFinalizedDelegate(StaffGripDelegate); }
 StaffGripDelegate.Reset(); StaffGripBody.Reset(); StaffGripAsset.Reset(); bOwnStaffGrip=false;
}

void URogue10mWeaponVisualComponent::UpdateStaffGrip()
{
 auto* Character=Cast<ARogue10mCharacter>(GetOwner());
 auto* Body=StaffGripBody.Get();
 if (!bOwnStaffGrip || !Body || !Character || !RightWeaponMesh) { return; }
 if (Character->GetMesh()!=Body || Character->GetEquippedWeaponType()!=ERogue10mWeaponType::Staff
     || RightWeaponMesh->GetAttachParent()!=Body || RightWeaponMesh->GetAttachSocketName()!=StaffAttachSocket
     || RightWeaponMesh->GetStaticMesh()!=StaffGripAsset.Get()
     || !RightWeaponMesh->GetRelativeTransform().Equals(LastStaffGrip,.0001))
 { UnbindStaffGrip(); return; }
 if (!bStaffUprightIdle || !Character->IsThirdPersonGameplayActive())
 { RightWeaponMesh->SetRelativeTransform(StaffGrip); UnbindStaffGrip(); return; }
 const FTransform Hand=Body->GetSocketTransform(StaffAttachSocket,RTS_World);
 // Slot weight includes blend-out after Montage_IsPlaying becomes false. No reset/pop at montage end.
 auto* Source=Character->GetAnimationPlaybackMesh();
 auto* Animation=Source?Source->GetAnimInstance():nullptr;
 const float ActionWeight=Animation?FMath::Clamp(Animation->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")),0.f,1.f):0.f;
 FTransform Relative;
 if (!BuildStaffUprightGrip(Hand,StaffGrip,Character->GetActorForwardVector(),ActionWeight,Relative)) { return; }
 RightWeaponMesh->SetRelativeLocationAndRotation(Relative.GetLocation(),Relative.GetRotation());
 LastStaffGrip=RightWeaponMesh->GetRelativeTransform();
}

URogue10mWeaponVisualComponent::URogue10mWeaponVisualComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
 // Owned Wizard Staff01 derivative, locally verified in the HumanMale presentation fixture.
 StaffMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Weapons/Staff20261110/SM_BattleWizardStaff01.SM_BattleWizardStaff01")));
 StaffGrip = FTransform(FQuat(.28596543351450643,.5380152286970158,.6652892006679809,.431455286211118),
  FVector(-5.897337436676025,-.12185341864824561,2.099235296249381),FVector(.7902026362595275));
 bStaffUprightIdle = true;
 GreatSwordMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Weapons/Starter20261110/SM_StarterGreatSword.SM_StarterGreatSword")));
 KnuckleMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Weapons/Starter20261110/SM_StarterKnuckle.SM_StarterKnuckle")));
 GreatSwordBodyAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/ABP_Human_GreatSword.ABP_Human_GreatSword_C")));
 GreatSwordBodySkeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/StylizedCharacter/Meshes/Character/Human/Male/SKEL_Hu_M.SKEL_Hu_M")));
 // Grip-local axes are documented alongside the authored meshes; UE centimeters.
 GreatSwordGrip = FTransform(FQuat(0.308477411, -0.189303508, 0.598270401, 0.714897473), FVector(-5.897337437, -0.121853419, 2.099235296));
 RightKnuckleGrip = FTransform(FRotator::ZeroRotator, FVector::ZeroVector);
 LeftKnuckleGrip = FTransform(FRotator(0, 0, 180), FVector::ZeroVector);
}
void URogue10mWeaponVisualComponent::BeginPlay()
{
 Super::BeginPlay();
 RefreshWeaponVisuals();
}
void URogue10mWeaponVisualComponent::RefreshWeaponVisuals()
{
 ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
 USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
 UnbindSwordGrip();
 UnbindStaffGrip();
 if (!Body || (!Character->HasActorBegunPlay() && !Character->IsActorBeginningPlay())) { return; }
 const ERogue10mWeaponType Weapon = Character->GetEquippedWeaponType();
 const bool bSword = Weapon == ERogue10mWeaponType::GreatSword;
 const bool bKnuckle = Weapon == ERogue10mWeaponType::Knuckle;
 const bool bStaff = Weapon == ERogue10mWeaponType::Staff;
 const bool bStaffGripValid = StaffGrip.IsValid() && StaffGrip.GetScale3D().GetMin() > 0.0;
 RefreshBodyRetarget(Body, bSword);
 UStaticMesh* Asset = bSword ? GreatSwordMesh.LoadSynchronous() : bKnuckle ? KnuckleMesh.LoadSynchronous() : bStaff ? StaffMesh.LoadSynchronous() : nullptr;
 auto Apply = [&](TObjectPtr<UStaticMeshComponent>& Part, const TCHAR* Name, FName Bone, bool bShow, const FTransform& Grip)
 {
  const bool bValid = bShow && Asset && !Bone.IsNone() && Body->DoesSocketExist(Bone) && (!bStaff || bStaffGripValid);
  if (!Part && bValid)
  {
   Part = NewObject<UStaticMeshComponent>(Character, Name, RF_Transient);
   Character->AddInstanceComponent(Part);
   Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Part->SetGenerateOverlapEvents(false);
   Part->SetCanEverAffectNavigation(false);
   Part->SetCastShadow(true);
   Part->SetReceivesDecals(false);
   Part->SetOwnerNoSee(false);
   Part->SetOnlyOwnerSee(false);
   Part->SetupAttachment(Body, Bone);
   Part->RegisterComponent();
  }
  if (!Part) { return; }
  if (bValid)
  {
   Part->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
   Part->SetStaticMesh(Asset);
   Part->SetRelativeTransform(Grip);
  }
  Part->SetVisibility(bValid, false);
  Part->SetHiddenInGame(!bValid, false);
 };
 // Finger-base reference offsets adapt to Manny and all rendered appearance proportions.
 auto FingerGrip = [&](FName Finger, FName Hand, const FTransform& Adjustment)
 {
  FTransform Result=Adjustment;
  if (const USkeletalMesh* Mesh=Body->GetSkeletalMeshAsset())
  {
   const FReferenceSkeleton& Ref=Mesh->GetRefSkeleton();const int32 H=Ref.FindBoneIndex(Hand);int32 I=Ref.FindBoneIndex(Finger);
   if (H!=INDEX_NONE && I!=INDEX_NONE)
   {
    FTransform Local=Ref.GetRefBonePose()[I];I=Ref.GetParentIndex(I);
    while(I!=INDEX_NONE && I!=H){Local*=Ref.GetRefBonePose()[I];I=Ref.GetParentIndex(I);}
    if(I==H){Result.AddToTranslation(Local.GetTranslation());}
   }
  }
  return Result;
 };
 const FTransform RightFingerGrip=FingerGrip(TEXT("middle_01_r"),TEXT("hand_r"),RightKnuckleGrip);
 const FTransform LeftFingerGrip=FingerGrip(TEXT("middle_01_l"),TEXT("hand_l"),LeftKnuckleGrip);
 const FName RightSocket = bStaff ? StaffAttachSocket : FName(TEXT("hand_r"));
 const FTransform RightGrip = bSword ? GreatSwordGrip : bStaff ? StaffGrip : RightFingerGrip;
 Apply(RightWeaponMesh, TEXT("Right Weapon Visual"), RightSocket, bSword || bKnuckle || bStaff, RightGrip);
 Apply(LeftWeaponMesh, TEXT("Left Weapon Visual"), TEXT("hand_l"), bKnuckle, LeftFingerGrip);
 if (bSword && RightWeaponMesh && RightWeaponMesh->IsVisible())
 {
  GripBody=Body; LastGrip=RightWeaponMesh->GetRelativeTransform(); bOwnGrip=true;
  GripDelegate=Body->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&URogue10mWeaponVisualComponent::UpdateSwordGrip));
  UpdateSwordGrip();
 }
 if (bStaff && bStaffUprightIdle && Character->IsThirdPersonGameplayActive() && RightWeaponMesh && RightWeaponMesh->IsVisible())
 {
  StaffGripBody=Body; StaffGripAsset=RightWeaponMesh->GetStaticMesh(); LastStaffGrip=RightWeaponMesh->GetRelativeTransform(); bOwnStaffGrip=true;
  StaffGripDelegate=Body->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&URogue10mWeaponVisualComponent::UpdateStaffGrip));
  UpdateStaffGrip();
 }
 if ((bSword || bKnuckle || bStaff) && !Asset)
 {
  UE_LOG(LogRogue10m, Warning, TEXT("Weapon presentation mesh is missing for weapon %d"), static_cast<int32>(Weapon));
 }
 if (bStaff && Asset && (StaffAttachSocket.IsNone() || !Body->DoesSocketExist(StaffAttachSocket)))
 {
  UE_LOG(LogRogue10m, Warning, TEXT("Staff presentation socket is missing: %s"), *StaffAttachSocket.ToString());
 }
 if (bStaff && Asset && !bStaffGripValid)
 {
  UE_LOG(LogRogue10m, Warning, TEXT("Staff presentation grip is invalid; finite transform and positive scale required"));
 }
}
void URogue10mWeaponVisualComponent::RefreshBodyRetarget(USkeletalMeshComponent* Body, bool bGreatSword)
{
 if (!Body) { return; }
 USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
 UClass* Current = Body->GetAnimClass();
 const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
 const bool bThirdPerson = Character && Character->IsThirdPersonGameplayActive();
 // Restore only our own change. A profile can replace the mesh/class independently.
 if (AppliedSwordBodyAnimClass && (Current != AppliedSwordBodyAnimClass || Body->GetAnimationMode() != EAnimationMode::AnimationBlueprint))
 {
  PreviousBodyAnimClass = nullptr; PreviousBodyMesh = nullptr; AppliedSwordBodyAnimClass = nullptr;
 }
 const bool bCompatible = bGreatSword && Mesh && Mesh->GetSkeleton() == GreatSwordBodySkeleton.LoadSynchronous();
 if (bCompatible && !AppliedSwordBodyAnimClass && Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Current)
 {
  if (UClass* SwordClass = GreatSwordBodyAnimClass.LoadSynchronous())
  {
   if (SwordClass != Current)
   {
    SwitchWeaponRetarget(*Body, SwordClass, bThirdPerson);
    if (Body->GetAnimClass() == SwordClass)
    {
     PreviousBodyAnimClass = Current; PreviousBodyMesh = Mesh; AppliedSwordBodyAnimClass = SwordClass;
    }
   }
  }
 }
 else if (!bCompatible && AppliedSwordBodyAnimClass)
 {
  if (Current == AppliedSwordBodyAnimClass && Mesh && PreviousBodyMesh && Mesh->GetSkeleton() == PreviousBodyMesh->GetSkeleton() && PreviousBodyAnimClass)
  {
   SwitchWeaponRetarget(*Body, PreviousBodyAnimClass, bThirdPerson);
   // UE can reject a switch during parallel animation evaluation. Keep ownership for a later refresh.
   if (Body->GetAnimClass() != PreviousBodyAnimClass) { return; }
  }
  PreviousBodyAnimClass = nullptr; PreviousBodyMesh = nullptr; AppliedSwordBodyAnimClass = nullptr;
 }
}
void URogue10mWeaponVisualComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 UnbindSwordGrip();
 UnbindStaffGrip();
 if (RightWeaponMesh) { RightWeaponMesh->DestroyComponent(); RightWeaponMesh = nullptr; }
 if (LeftWeaponMesh) { LeftWeaponMesh->DestroyComponent(); LeftWeaponMesh = nullptr; }
 Super::EndPlay(Reason);
}
void URogue10mWeaponVisualComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
 UnbindSwordGrip();
 UnbindStaffGrip();
 Super::OnComponentDestroyed(bDestroyingHierarchy);
}
