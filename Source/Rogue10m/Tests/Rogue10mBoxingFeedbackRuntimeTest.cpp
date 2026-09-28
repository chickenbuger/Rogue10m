// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mHitFeedbackComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include <limits>

namespace Rogue10mBoxingFeedbackTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
 void Start(UWorld* W)
 {
  World=W;
  FTimerHandle H;
  W->GetTimerManager().SetTimer(H,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.0f,false);
 }
private:
 void Check(bool OK,const TCHAR* Message)
 { if (!OK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("BOXING_FEEDBACK FAIL frame=%d %s"),FrameIndex,Message); } }
 void Prepare()
 {
  PC=World->GetFirstPlayerController();
  Character=PC.IsValid()?Cast<ARogue10mCharacter>(PC->GetPawn()):nullptr;
  if (!Character.IsValid()) { Check(false,TEXT("character_missing")); Finish(); return; }
  PC->SetIgnoreLookInput(true); bOwnLookIgnore=true;
  PC->SetControlRotation(FRotator(0,PC->GetControlRotation().Yaw,0));
  Character->GetCharacterMovement()->StopMovementImmediately();
  Character->SetCanBeDamaged(false);
  Character->GetRogueAttributeSet()->RestoreVitals();
  Special=Character->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
  if (!Special.IsValid()) { Check(false,TEXT("special_missing")); Finish(); return; }
  // In-memory multi-target fixture; no package saves and restored on all exits.
  OldDamage=Special->Damage; OldWidth=Special->BoxHalfWidth; OldRange=Special->AttackRange; OldMaxTargets=Special->MaxTargetsPerHit;
  Special->BoxHalfWidth=180; Special->AttackRange=250; Special->MaxTargetsPerHit=8;
  for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
  {
   It->ClearAITarget(); if (auto* AI=It->GetController()) { AI->UnPossess(); }
   It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
   if (auto* R=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { R->ConfigureRegeneration(0,0,0); }
   if (Targets.Num()<2)
   {
    Targets.Add(*It); It->GetRogueAttributeSet()->SetMaxHealth(2000); It->GetRogueAttributeSet()->SetHealth(2000);
    It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    It->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*1500.0f);
    MeshOrigins.Add(It->GetMesh()->GetRelativeTransform());
   }
   else { It->SetActorEnableCollision(false); }
  }
  if (Targets.Num()!=2) { Check(false,TEXT("two_target_fixture_missing")); Finish(); return; }
  GuardCamera=Character->GetFirstPersonCameraComponent()->GetRelativeTransform(); GuardControl=PC->GetControlRotation();
  OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bFixed=true;
  FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if (W==Self->World.Get()) {Self->Frame();}});
 }
 void Frame()
 {
  auto* Presentation=Character->GetFirstPersonPresentationComponent();
  auto* Combat=Character->GetCombatComponent();
  if (FrameIndex==20 || FrameIndex==70 || FrameIndex==120) { Combat->HandleAttackPressed(false); }
  if (FrameIndex==23 || FrameIndex==73 || FrameIndex==123) { Combat->HandleAttackReleased(false); }
  if (FrameIndex==60)
  {
   Check(Presentation->GetConfirmedHitFeedbackCount()==0,TEXT("miss_triggered_camera_impact"));
   for (int32 I=0;I<2;++I)
   {
    auto* M=Targets[I].Get();
    Check(!M->FindComponentByClass<URogue10mHitFeedbackComponent>(),TEXT("miss_triggered_enemy_reaction"));
    M->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*145.0f+Character->GetActorRightVector()*(I==0?-55.0f:55.0f));
    ActorOrigins.Add(M->GetActorTransform());
   }
  }
  if (FrameIndex>=74 && FrameIndex<105)
  {
   bSawCamera|=Presentation->GetHitFeedbackWeight()>0;
   for (int32 I=0;I<2;++I)
   {
    auto* M=Targets[I].Get();
    if (auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>(); F && F->GetFeedbackWeight()>0)
    {
     SawRecoil[I]|=!M->GetMesh()->GetRelativeTransform().Equals(MeshOrigins[I],0.001f);
     SawFlash[I]|=M->GetMesh()->GetOverlayMaterial()!=nullptr;
    }
   }
  }
  if (FrameIndex==110)
  {
   Check(Presentation->GetConfirmedHitFeedbackCount()==1 && bSawCamera,TEXT("multi_target_camera_not_once"));
   Check(Presentation->GetHitFeedbackWeight()==0,TEXT("camera_impact_did_not_expire"));
   for (int32 I=0;I<2;++I)
   {
    auto* M=Targets[I].Get(); auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>();
    Check(M->GetRogueAttributeSet()->GetHealth()<2000 && F && F->GetHitCount()==1,TEXT("multi_target_actual_damage_or_reaction_missing"));
    Check(SawRecoil[I] && SawFlash[I],TEXT("visible_enemy_recoil_or_flash_missing"));
    Check(M->GetActorTransform().Equals(ActorOrigins[I],0.01f),TEXT("visual_recoil_moved_collision"));
    Check(M->GetMesh()->GetRelativeTransform().Equals(MeshOrigins[I],0.001f) && !M->GetMesh()->GetOverlayMaterial(),TEXT("enemy_reaction_did_not_restore"));
    Special->Damage=-Character->GetRogueAttributeSet()->GetAttackPower();
   }
  }
  if (FrameIndex==155)
  {
   Check(Presentation->GetConfirmedHitFeedbackCount()==1,TEXT("rejected_damage_triggered_camera_impact"));
   for (const auto& M:Targets) { auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>(); Check(F && F->GetHitCount()==1,TEXT("rejected_damage_triggered_enemy_reaction")); }
  }
  if (FrameIndex==160)
  {
   auto* M=Targets[0].Get(); auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>();
   if (F)
   {
    // Existing material ownership must survive this component's temporary effect.
    ExistingOverlay=M->GetMesh()->GetMaterial(0); M->GetMesh()->SetOverlayMaterial(ExistingOverlay.Get());
    F->PlayHit(1,Character->GetActorForwardVector()); F->PlayHit(1,Character->GetActorForwardVector());
    Check(M->GetMesh()->GetOverlayMaterial()==ExistingOverlay.Get(),TEXT("reaction_overwrote_existing_overlay"));
    Check(FVector::Distance(M->GetMesh()->GetRelativeLocation(),MeshOrigins[0].GetLocation())<=2.01,TEXT("repeated_recoil_accumulated"));
   }
  }
  if (FrameIndex==170)
  {
   auto* M=Targets[0].Get(); Check(M->GetMesh()->GetRelativeTransform().Equals(MeshOrigins[0],0.001f),TEXT("repeated_recoil_failed_restore"));
   Check(M->GetMesh()->GetOverlayMaterial()==ExistingOverlay.Get(),TEXT("existing_overlay_removed_on_restore")); M->GetMesh()->SetOverlayMaterial(nullptr);
   FMinimalViewInfo V; V.Location=FVector(10,20,30); V.Rotation=FRotator(3,4,5); const FMinimalViewInfo Base=V;
   Presentation->NotifyConfirmedBrawlerHit(1,1); const float Scale=Presentation->CameraMotionScale; Presentation->CameraMotionScale=0;
   Presentation->ApplyBodyMotionToView(1.0f/30,V);
   Check(V.Location.Equals(Base.Location,0.001f) && V.Rotation.Equals(Base.Rotation,0.001f) && Presentation->GetHitFeedbackWeight()==0,TEXT("motion_zero_did_not_clear_hit_impulse"));
   Presentation->CameraMotionScale=Scale;
   Presentation->NotifyConfirmedBrawlerHit(1,-1); V=Base; Presentation->ApplyBodyMotionToView(std::numeric_limits<float>::quiet_NaN(),V);
   Check(V.Location.Equals(Base.Location,0.001f) && V.Rotation.Equals(Base.Rotation,0.001f) && Presentation->GetHitFeedbackWeight()==0,TEXT("nonfinite_dt_did_not_clear_hit_impulse"));
  }
  if (FrameIndex==180)
  {
   Presentation->NotifyConfirmedBrawlerHit(1,1); Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
   Check(Presentation->GetHitFeedbackWeight()==0,TEXT("weapon_switch_retained_hit_impulse")); Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
  }
  if (FrameIndex==190)
  {
   auto* M=Targets[0].Get(); if (auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>()) { F->PlayHit(1,Character->GetActorForwardVector()); } M->SetCanBeDamaged(true); UGameplayStatics::ApplyDamage(M,10000,PC.Get(),Character.Get(),UDamageType::StaticClass());
  }
  if (FrameIndex==195)
  {
   // This monster destroys itself on death. A surviving corpse must have no pulse;
   // an expired weak reference confirms the component followed actor destruction.
   if (auto* M=Targets[0].Get())
   {
    auto* F=M->FindComponentByClass<URogue10mHitFeedbackComponent>();
    Check(F && F->GetFeedbackWeight()==0 && !M->GetMesh()->GetOverlayMaterial(),TEXT("death_retained_enemy_flash"));
   }
  }
  if (FrameIndex<180)
  {
   Check(Character->GetFirstPersonCameraComponent()->GetRelativeTransform().Equals(GuardCamera,0.01f) && PC->GetControlRotation().Equals(GuardControl,0.01f),TEXT("feedback_changed_gameplay_aim"));
  }
  if (FrameIndex==210) { Combat->HandleAttackPressed(false); }
  if (FrameIndex==234)
  {
   Combat->HandleAttackReleased(false);
   Check(Character->GetBasicBrawlerComponent()->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightStraight,TEXT("near_threshold_release_did_not_use_straight"));
  }
  if (FrameIndex>=228 && FrameIndex<=246)
  {
   const FVector Elbow=Character->GetFirstPersonCameraComponent()->GetComponentTransform().InverseTransformPosition(Character->GetFirstPersonMesh()->GetSocketLocation(TEXT("lowerarm_r")));
   if (bElbowSample) { MaxElbowStep=FMath::Max(MaxElbowStep,FVector::Distance(PreviousElbow,Elbow)); }
   PreviousElbow=Elbow; bElbowSample=true;
  }
  if (FrameIndex==280)
  {
   Check(MaxElbowStep<12.0,TEXT("near_threshold_release_elbow_teleported"));
   UE_LOG(LogRogue10m,Display,TEXT("BOXING_FEEDBACK near_threshold_elbow_step_cm=%.3f"),MaxElbowStep);
   Finish(); return;
  }
  ++FrameIndex;
 }
 void Finish()
 {
  if (Special.IsValid()) { Special->Damage=OldDamage; Special->BoxHalfWidth=OldWidth; Special->AttackRange=OldRange; Special->MaxTargetsPerHit=OldMaxTargets; }
  if (bOwnLookIgnore && PC.IsValid()) { PC->SetIgnoreLookInput(false); }
  FWorldDelegates::OnWorldPostActorTick.Remove(Tick);
  if (bFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=BOXING_FEEDBACK_%s frames=%d failures=%d"),Failures==0 && FrameIndex==280?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Failures);
  FPlatformMisc::RequestExitWithStatus(false,Failures==0 && FrameIndex==280?0:1);
 }
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<APlayerController> PC;
 TWeakObjectPtr<ARogue10mCharacter> Character;
 TWeakObjectPtr<URogue10mAttackSkillData> Special;
 TWeakObjectPtr<UMaterialInterface> ExistingOverlay;
 TArray<TWeakObjectPtr<ARogue10mBasicMonster>> Targets;
 TArray<FTransform> MeshOrigins,ActorOrigins;
 FTransform GuardCamera;
 FRotator GuardControl;
 FDelegateHandle Tick;
 int32 FrameIndex=0,Failures=0,OldMaxTargets=0;
 float OldDamage=0,OldWidth=0,OldRange=0;
 double OldDelta=0,MaxElbowStep=0;
 FVector PreviousElbow;
 bool bElbowSample=false;
 bool bOwnLookIgnore=false,bFixed=false,OldFixed=false,bSawCamera=false;
 bool SawRecoil[2]={false,false},SawFlash[2]={false,false};
};
static TSharedPtr<FRun> Active;
static void Run()
{
 if (!GEngine) { return; }
 for (const FWorldContext& C:GEngine->GetWorldContexts())
 { if (UWorld* W=C.World(); W && W->IsGameWorld()) { Active=MakeShared<FRun>(); Active->Start(W); return; } }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBoxingFeedback"),TEXT("Validates actual miss, multi-target hit, rejected damage, bounded reactions and restoration."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
