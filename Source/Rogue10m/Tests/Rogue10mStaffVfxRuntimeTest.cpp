#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mStylizedCharacter.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "TimerManager.h"
#include "ShaderCompiler.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace Rogue10mStaffVfxRuntime
{
class FRun : public TSharedFromThis<FRun>
{
public:
 void Start(UWorld* W)
 {
  World=W; FString Token,Profile,Progression,Keys;
  FParse::Value(FCommandLine::Get(),TEXT("RogueStaffVfxNamespace="),Token);
  FParse::Value(FCommandLine::Get(),TEXT("RogueProfileSaveSlot="),Profile);
  FParse::Value(FCommandLine::Get(),TEXT("RogueProgressionSavePrefix="),Progression);
  FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Safe=Token.Len()==32;for(TCHAR C:Token){Safe&=FChar::IsHexDigit(C);}
  Safe&=Profile==TEXT("Rogue10m_Town_QA_StaffVfx_")+Token&&Progression==TEXT("Rogue10m_Progression_QA_StaffVfx_")+Token&&Keys==TEXT("Rogue10m_Fighter_QA_StaffVfx_")+Token;
  if(!Safe){UE_LOG(LogRogue10m,Error,TEXT("STAFFVFX FAIL isolated_namespace_required"));bFinished=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
  FString Label=TEXT("Invalid");FParse::Value(FCommandLine::Get(),TEXT("RogueStaffVfxLabel="),Label);
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/staff-vfx-runtime-20261111")/FPaths::MakeValidFileName(Label);
  Images=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/staff-vfx-runtime-20261111/images")/FPaths::MakeValidFileName(Label);
  IFileManager::Get().MakeDirectory(*Output,true);IFileManager::Get().MakeDirectory(*Images,true);
  CaptureRunId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
  Rows=TEXT("frame,world_s,case,montage_s,health,damage,actor_x,actor_y,actor_z,actor_shift_cm,hand_travel_cm\n");
  CaptureRows=TEXT("event,skill,phase,case_index,fixture_frame,world_s,montage_s,file,bytes,cue_component,actual_sim_age_s,cue_min_age_s,cue_max_age_s,age_source,camera_view,camera_x,camera_y,camera_z,camera_pitch,camera_yaw,camera_roll,accepted_elapsed_s,montage_duration_s,montage_normalized_pos,montage_playing\n");
  VfxRows=TEXT("event,skill,kind,fixture_frame,world_s,world_path,component_path,asset_path,attach_socket,first_seen_frame,observed_frame_delta,controller_valid,actual_sim_age_s,age_source,active,visible,registered,complete,eligible,location_x,location_y,location_z,scale_x,scale_y,scale_z,bounds_origin_x,bounds_origin_y,bounds_origin_z,bounds_extent_x,bounds_extent_y,bounds_extent_z,custom_time_dilation,damage_pulses_observed,first_observed_damage_pulse,pinned_first_impact,attach_parent_path,expected_3p_scale,third_person_parent_valid,third_person_scale_valid\n");
  W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
 }
 bool Finished()const{return bFinished;}
private:
 void Check(bool B,const TCHAR* Reason){if(!B){++Failures;UE_LOG(LogRogue10m,Error,TEXT("STAFFVFX FAIL skill=%d frame=%d %s"),Index,Frame,*FString(Reason));}}
 float CaptureMontagePosition() const
 {
  const auto* Source=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const auto* Skill=Skills.IsValidIndex(Index)?Skills[Index].Get():nullptr;
  return Anim&&Skill?Anim->Montage_GetPosition(Skill->AttackMontage):-1.f;
 }
 FString CaptureTelemetry() const
 {
  const auto* Component=PendingCaptureComponent.Get();
  bool bAgeValid=false;double Age=-1.;
  if(Component){const auto Instance=Component->GetSystemInstanceController();bAgeValid=Instance.IsValid()&&Instance->IsValid();if(bAgeValid){Age=static_cast<double>(Instance->GetAge());}}
  FVector CameraLocation=FVector::ZeroVector;FRotator CameraRotation=FRotator::ZeroRotator;
  if(Controller.IsValid()){Controller->GetPlayerViewPoint(CameraLocation,CameraRotation);}
  const auto* Skill=Skills.IsValidIndex(Index)?Skills[Index].Get():nullptr;
  const double MontageDuration=Skill&&Skill->AttackMontage?Skill->AttackMontage->GetPlayLength():0.;
  const auto* Source=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const bool bPlaying=Anim&&Skill&&Anim->Montage_IsPlaying(Skill->AttackMontage);
  return FString::Printf(TEXT("%s,%.6f,%.6f,%.6f,%s,default_third_person,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d"),
   *GetPathNameSafe(Component),Age,PendingCaptureMinAge,PendingCaptureMaxAge,bAgeValid?TEXT("FNiagaraSystemInstanceController::GetAge"):TEXT("unavailable"),
   CameraLocation.X,CameraLocation.Y,CameraLocation.Z,CameraRotation.Pitch,CameraRotation.Yaw,CameraRotation.Roll,
   Accepted>=0&&World.IsValid()?World->GetTimeSeconds()-Accepted:-1.,MontageDuration,MontageDuration>0.?CaptureMontagePosition()/MontageDuration:-1.,bPlaying?1:0);
 }
 bool Capture(const TCHAR* Phase,UNiagaraComponent* EvidenceComponent=nullptr,double MinAge=0.,double MaxAge=0.)
 {
  if(bCapturePending||FScreenshotRequest::IsScreenshotRequested()){Check(false,TEXT("screenshot_request_already_pending"));return false;}
  PendingCaptureSkill=Skills.IsValidIndex(Index)&&Skills[Index].IsValid()?Skills[Index]->GetName():TEXT("setup");
  PendingCapturePhase=Phase;
  PendingCaptureComponent=EvidenceComponent;PendingCaptureMinAge=MinAge;PendingCaptureMaxAge=MaxAge;
  // A unique file cannot be mistaken for a successful capture left by an earlier run.
  PendingCapturePath=FPaths::ConvertRelativePathToFull(Images/(PendingCaptureSkill+FString::FromInt(Index)+TEXT("_")+Phase+TEXT("_")+CaptureRunId+TEXT(".png")));
  if(IFileManager::Get().FileExists(*PendingCapturePath)){Check(false,TEXT("screenshot_file_not_unique"));return false;}
  bCapturePending=true;CaptureRequestedAt=FPlatformTime::Seconds();CapturePaths.Add(PendingCapturePath);
  CaptureRows+=FString::Printf(TEXT("request,%s,%s,%d,%d,%.6f,%.6f,%s,-1,%s\n"),*PendingCaptureSkill,*PendingCapturePhase,Index,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath,*CaptureTelemetry());
  UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX CAPTURE_REQUEST skill=%s phase=%s frame=%d world=%.6f montage=%.6f file=%s"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
  FScreenshotRequest::RequestScreenshot(PendingCapturePath,true,false);
  return true;
 }
 void CaptureProcessed()
 {
  if(!bCapturePending){return;}
  // UE broadcasts after the save attempt, including failed attempts. Require actual bytes too.
  const int64 Bytes=IFileManager::Get().FileSize(*PendingCapturePath);
  const bool bSaved=Bytes>0;Check(bSaved,TEXT("processed_screenshot_file_missing_or_empty"));
  CaptureRows+=FString::Printf(TEXT("processed,%s,%s,%d,%d,%.6f,%.6f,%s,%lld,%s\n"),*PendingCaptureSkill,*PendingCapturePhase,Index,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath,static_cast<long long>(Bytes),*CaptureTelemetry());
  UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX CAPTURE_PROCESSED skill=%s phase=%s frame=%d world=%.6f montage=%.6f bytes=%lld"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),static_cast<long long>(Bytes));
  if(bSaved){++CompletedCaptures;}
  if(PendingCapturePhase==TEXT("ready")){bReadyCaptured=bSaved;}
  else if(PendingCapturePhase==TEXT("after_first_damage")){bFirstDamageCaptured=bSaved;}
  else if(PendingCapturePhase==TEXT("recovery")){bRecoveryCaptured=bSaved;}
  bCapturePending=false;
  if(!bSaved){bFinishRequested=true;}
 }
 // Read-only, per-component evidence. Exact asset pointers distinguish cast from impact.
 // Production spawns these transient components without pooling; exclude pre-input instances.
 struct FVfxObservation
 {
  TWeakObjectPtr<UNiagaraComponent> Component;
  int32 FirstFrame=0,LastFrame=0,FirstDamagePulse=0;bool bPresentationFailureReported=false;
 };
 bool IsVfxCase() const {return true;}
 void BaselineVfx()
 {
  if(!IsVfxCase()){return;}
  VfxExcluded.Reset();VfxObservations.Reset();FirstImpactComponent.Reset();bFirstImpactObserved=false;
  const auto* Skill=Skills[Index].Get();
  for(TObjectIterator<UNiagaraComponent> It;It;++It)
  {
   if(IsValid(*It)&&It->GetWorld()==World.Get()&&(It->GetAsset()==Skill->CastEffect.Get()||It->GetAsset()==Skill->ImpactEffect.Get()))
   {VfxExcluded.Add(TWeakObjectPtr<UNiagaraComponent>(*It));}
  }
 }
 // Returned weak reference is only valid for this frame's gate, never a latched readiness flag.
 TWeakObjectPtr<UNiagaraComponent> ObserveVfx(UNiagaraSystem* Asset,const TCHAR* Kind,const TCHAR* Event)
 {
  TWeakObjectPtr<UNiagaraComponent> Ready;
  const auto* Skill=Skills[Index].Get();
  for(TObjectIterator<UNiagaraComponent> It;It;++It)
  {
   auto* Component=*It;
   if(!IsValid(Component)||Component->GetWorld()!=World.Get()||!Asset||Component->GetAsset()!=Asset){continue;}
   const TWeakObjectPtr<UNiagaraComponent> Weak(Component);
   if(VfxExcluded.Contains(Weak)){continue;}
   const bool bImpact=Asset==Skill->ImpactEffect.Get();
   // Sampling precedes the cached health update; include this frame's HP drop in the pulse label.
   const int32 ObservedDamagePulses=PulseCount+(PreviousHealth-Target->GetRogueAttributeSet()->GetHealth()>.01?1:0);
   if(bImpact&&!bFirstImpactObserved){FirstImpactComponent=Weak;bFirstImpactObserved=true;}
   auto& Seen=VfxObservations.FindOrAdd(Weak);
   if(!Seen.Component.IsValid()||Seen.LastFrame<Frame-1)
   {Seen.Component=Weak;Seen.FirstFrame=Frame;Seen.FirstDamagePulse=ObservedDamagePulses;}
   Seen.LastFrame=Frame;
   const auto Instance=Component->GetSystemInstanceController();
   const bool bInstanceValid=Instance.IsValid()&&Instance->IsValid();
   const double SimAge=bInstanceValid?static_cast<double>(Instance->GetAge()):-1.;
   const bool bComplete=Component->IsComplete();
   const bool bThirdPerson=Character->IsThirdPersonGameplayActive();
   const double ExpectedScale=bImpact?Skill->ImpactEffectScale:Skill->CastEffectScale;
   const bool bThirdPersonParentValid=bImpact||Component->GetAttachParent()==Character->GetMesh();
   const bool bThirdPersonScaleValid=Component->GetComponentScale().Equals(FVector(ExpectedScale),1.e-4);
   if(!Seen.bPresentationFailureReported&&(!bThirdPerson||!bThirdPersonParentValid||!bThirdPersonScaleValid))
   {
    Seen.bPresentationFailureReported=true;
    Check(bThirdPerson,TEXT("vfx_third_person_gameplay_required"));
    Check(bThirdPersonParentValid,TEXT("cast_vfx_third_person_body_parent_required"));
    Check(bThirdPersonScaleValid,TEXT("vfx_world_scale_must_equal_third_person_skill_scale"));
    UE_LOG(LogRogue10m,Error,TEXT("STAFFVFX VFX_PRESENTATION_MISMATCH skill=%s kind=%s component=%s parent=%s expected_body=%s expected_3p_scale=%.6f actual_world_scale=%s third_person=%d parent_valid=%d scale_valid=%d"),
     *Skill->GetName(),Kind,*Component->GetPathName(),*GetPathNameSafe(Component->GetAttachParent()),*GetPathNameSafe(Character->GetMesh()),ExpectedScale,*Component->GetComponentScale().ToString(),bThirdPerson?1:0,bThirdPersonParentValid?1:0,bThirdPersonScaleValid?1:0);
   }
   const bool bEligible=Frame-Seen.FirstFrame>=2&&bInstanceValid&&FMath::IsFinite(SimAge)&&SimAge>0.
    &&Component->IsRegistered()&&Component->IsActive()&&Component->IsVisible()&&!bComplete
    &&bThirdPerson&&bThirdPersonParentValid&&bThirdPersonScaleValid
    &&(!bImpact||(Weak==FirstImpactComponent&&Seen.FirstDamagePulse==1));
   const FVector Location=Component->GetComponentLocation(),Scale=Component->GetComponentScale();
   const FVector BoundsOrigin=Component->Bounds.Origin,BoundsExtent=Component->Bounds.BoxExtent;
   VfxRows+=FString::Printf(TEXT("%s,%s,%s,%d,%.6f,%s,%s,%s,%s,%d,%d,%d,%.6f,%s,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%s,%.6f,%d,%d\n"),
    Event,*Skill->GetName(),Kind,Frame,World->GetTimeSeconds(),*World->GetPathName(),*Component->GetPathName(),*Asset->GetPathName(),*Component->GetAttachSocketName().ToString(),
    Seen.FirstFrame,Frame-Seen.FirstFrame,bInstanceValid?1:0,SimAge,TEXT("FNiagaraSystemInstanceController::GetAge"),Component->IsActive()?1:0,Component->IsVisible()?1:0,Component->IsRegistered()?1:0,bComplete?1:0,bEligible?1:0,
    Location.X,Location.Y,Location.Z,Scale.X,Scale.Y,Scale.Z,BoundsOrigin.X,BoundsOrigin.Y,BoundsOrigin.Z,BoundsExtent.X,BoundsExtent.Y,BoundsExtent.Z,static_cast<double>(Component->GetCustomTimeDilation()),ObservedDamagePulses,Seen.FirstDamagePulse,bImpact&&Weak==FirstImpactComponent?1:0,
    *GetPathNameSafe(Component->GetAttachParent()),ExpectedScale,bThirdPersonParentValid?1:0,bThirdPersonScaleValid?1:0);
   if(bEligible&&!Ready.IsValid()){Ready=Weak;}
  }
  return Ready;
 }
 bool CaptureVfx(const TCHAR* Phase,UNiagaraSystem* Asset,const TCHAR* Kind,double MinAge=0.,double MaxAge=0.)
 {
  const auto Ready=ObserveVfx(Asset,Kind,Phase);
  if(!Ready.IsValid()){return false;}
  const auto Instance=Ready->GetSystemInstanceController();
  const double Age=static_cast<double>(Instance->GetAge());
  // Gate against actual controller age, not accepted elapsed time or montage time.
  // Missing a short cue's window is a failed required capture, never a late substitute.
  if(Age<MinAge||(MaxAge>0.&&Age>MaxAge)){return false;}
  UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX VFX_GATE skill=%s phase=%s kind=%s component=%s asset=%s frame=%d observed_frames=%d actual_sim_age=%.6f age_source=FNiagaraSystemInstanceController::GetAge first_observed_damage_pulse=%d custom_time_dilation=%.6f visual_approval=0"),
   *Skills[Index]->GetName(),Phase,Kind,*Ready->GetPathName(),*Asset->GetPathName(),Frame,Frame-VfxObservations.FindChecked(Ready).FirstFrame,static_cast<double>(Instance->GetAge()),VfxObservations.FindChecked(Ready).FirstDamagePulse,static_cast<double>(Ready->GetCustomTimeDilation()));
  return Capture(Phase,Ready.Get(),MinAge,MaxAge);
 }
 void Prepare()
 {
  Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
  auto* Old=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
  UClass* Class=LoadClass<ARogue10mCharacter>(nullptr,TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
  if(!Old||!Class){Check(false,TEXT("fixture_character_missing"));Finish();return;}
  const FTransform T=Old->GetActorTransform();Old->SetActorEnableCollision(false);Old->SetActorHiddenInGame(true);
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Character=World->SpawnActor<ARogue10mCharacter>(Class,T,P);Controller->Possess(Character.Get());Controller->SetViewTarget(Character.Get());
  if(auto* S=Cast<ARogue10mStylizedCharacter>(Character.Get()))
  {
   FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("WeaponQA");Profile.Appearance.Race=S->GetAppearanceRace();Profile.Appearance.Gender=S->GetAppearanceGender();Check(S->ApplyCharacterProfile(Profile),TEXT("profile_failed"));
  }
  Character->SetCanBeDamaged(false);Character->DisableInput(Controller.Get());Character->GetCharacterMovement()->StopMovementImmediately();
  Character->SetActorRotation(FRotator(0,0,0));Controller->SetControlRotation(FRotator(-8,0,0));
  // The current user-requested default is third person. This fixture never toggles into first person.
  Check(Character->IsThirdPersonGameplayActive(),TEXT("third_person_mode_required"));
  Character->GetMesh()->SetOwnerNoSee(false);Character->GetMesh()->SetVisibility(true,false);
  for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It){It->ClearAITarget();if(It->GetController()){It->GetController()->UnPossess();}It->SetActorEnableCollision(false);It->GetCharacterMovement()->DisableMovement();}
  Target=World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),Character->GetActorLocation()+FVector(110,0,0),FRotator::ZeroRotator,P);
  if(!Target.IsValid()){Check(false,TEXT("target_failed"));Finish();return;}
  Target->GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
  Target->GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));
  Target->ClearAITarget();if(Target->GetController()){Target->GetController()->UnPossess();}Target->GetCharacterMovement()->DisableMovement();
  if(auto* R=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()){R->ConfigureRegeneration(0,0,0);}
  Target->GetRogueAttributeSet()->SetMaxHealth(100000);Target->GetRogueAttributeSet()->SetHealth(100000);
  Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
  auto* Skill=LoadObject<URogue10mAttackSkillData>(nullptr,TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01.DA_Attack_Staff_Primary01"));
  if(!Skill||!Skill->AttackMontage){Check(false,TEXT("native_skill_or_montage_missing"));Finish();return;}
  Skills.Add(Skill);Skills.Add(Skill);Skills.Add(Skill);
  const FString Base=TEXT("/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/");
  Check(GetPathNameSafe(Skill->CastEffect)==Base+TEXT("NS_MagicTelegraph_Preparation_Candidate.NS_MagicTelegraph_Preparation_Candidate"),TEXT("saved_cast_binding_required"));
  Check(GetPathNameSafe(Skill->ImpactEffect)==Base+TEXT("NS_MagicImpact_DiamondStar_Candidate.NS_MagicImpact_DiamondStar_Candidate"),TEXT("saved_impact_binding_required"));
  Check(Skill->bEnableAttackEffects&&FMath::IsNearlyEqual(Skill->CastEffectScale,1.f)&&FMath::IsNearlyEqual(Skill->ImpactEffectScale,1.f)&&FMath::IsNearlyEqual(Skill->AttackEffectTimeDilation,1.f)&&FMath::IsNearlyEqual(Skill->CastEffectEmissionDuration,.35f)&&FMath::IsNearlyEqual(Skill->ImpactEffectEmissionDuration,.27f),TEXT("eight_visual_fields_required"));
  if(FParse::Param(FCommandLine::Get(),TEXT("RogueStaffVfxReadabilityCandidate")))
  {
   Check(Skill->ThirdPersonCastEffectOffset.Equals(FVector(25,0,20),1.e-4)&&Skill->ThirdPersonCastEffectRotation.Equals(FRotator::ZeroRotator,1.e-4)
    &&Skill->bUseThirdPersonImpactBoundsSurface&&FMath::IsNearlyEqual(Skill->ThirdPersonImpactEffectSurfaceOffset,4.f),TEXT("four_readability_trial_fields_required"));
  }
  else
  {
   Check(Skill->ThirdPersonCastEffectOffset.IsNearlyZero(1.e-4)&&Skill->ThirdPersonCastEffectRotation.Equals(FRotator::ZeroRotator,1.e-4)
    &&!Skill->bUseThirdPersonImpactBoundsSurface&&FMath::IsNearlyZero(Skill->ThirdPersonImpactEffectSurfaceOffset,1.e-4f),TEXT("baseline_zero_placement_fields_required"));
  }
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  auto* Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
  if(!Tree){Check(false,TEXT("progression_missing"));Finish();return;}
  // Staff Primary01 comes from the native profile InitiallyUnlockedSkills; no fixture grant.
  UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX SCOPE combat_request=1 physical_input=0 exact_contact=0 visual_approval=0 native_defaults=1 montage=%s progression=native_profile_initial_unlock no_staff_mastery_track=1"),*Skill->AttackMontage->GetPathName());
  if(Failures){Finish();return;}
  TSet<UNiagaraSystem*> Effects;
  for(const auto& Entry:Skills){const auto* EffectSkill=Entry.Get();for(UNiagaraSystem* FX:{EffectSkill->CastEffect.Get(),EffectSkill->ChargeEffect.Get(),EffectSkill->ImpactEffect.Get()}){if(FX){Effects.Add(FX);}}}
  for(UNiagaraSystem* FX:Effects){FX->RequestCompile(false);FX->WaitForCompilationComplete(true,false);}
  if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
  OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);bOwnFixed=true;
  ScreenshotProcessed=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->CaptureProcessed();});
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
  Next();
 }

 void Next()
 {
  if(Index==3){Finish();return;}
  auto* C=Character->GetCombatComponent();C->CancelBoundSkillInput();C->CancelCombatVisuals();
  Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Staff);
  Check(C->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)==Skills[Index].Get(),TEXT("native_staff_primary_binding_required"));
  Check(C->IsAttackSkillUnlocked(Skills[Index].Get()),TEXT("durable_unlock_required"));
  const auto View=Character->GetCombatViewTransform();FVector Forward=View.GetRotation().GetForwardVector();Forward.Z=0;Forward.Normalize();
  Target->SetActorLocation(Character->GetActorLocation()+Forward*(Index==2?10000.:110.),false,nullptr,ETeleportType::TeleportPhysics);
  Target->SetCanBeDamaged(Index!=1);
  StartLocation=Character->GetActorLocation();ReadyHand=Character->GetMesh()->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation();
  Uses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Skills[Index]->GetFName());PreviousHealth=Target->GetRogueAttributeSet()->GetHealth();
  CaseStartedAt=World->GetTimeSeconds();Accepted=-1;AttackSent=false;PulseCount=0;MontageSamples=0;HandTravel=0;
  bReadyRequested=bReadyCaptured=bFirstDamageCaptured=bFirstDamageRequested=bAcceptanceRequested=bImpactResidualRequested=bRecoveryRequested=bRecoveryCaptured=false;
  bCastMidRequested=bCastLateRequested=bImpactMidRequested=bImpactLateRequested=bMotion030Requested=bMotion060Requested=bMotion100Requested=bMotionRecoveryRequested=false;
  bFirstImpactObserved=false;VfxExcluded.Reset();VfxObservations.Reset();FirstImpactComponent.Reset();
  UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX CASE index=%d scope=%s target=%s"),Index,Index==0?TEXT("positive_damage"):Index==1?TEXT("invulnerable_no_damage"):TEXT("miss_out_of_range"),*Target->GetActorLocation().ToString());
  if(Failures){Finish();}
 }
 void Step()
 {
  if(bFinished)return;++Frame;
  if(bCapturePending&&FPlatformTime::Seconds()-CaptureRequestedAt>30.){Check(false,TEXT("capture_timeout"));FScreenshotRequest::Reset();bCapturePending=false;bFinishRequested=true;}
  if(bFinishRequested){Finish();return;}
  if(!Character.IsValid()||!Target.IsValid()){Check(false,TEXT("fixture_lost"));Finish();return;}
  auto* Skill=Skills[Index].Get();auto* C=Character->GetCombatComponent();const double Now=World->GetTimeSeconds();
  if(Now-CaseStartedAt>20.){Check(false,TEXT("case_timeout"));Finish();return;}
  if(bRecoveryRequested){if(bCapturePending)return;Check(bRecoveryCaptured,TEXT("recovery_unprocessed"));++Index;Next();return;}
  if(!AttackSent&&Now-CaseStartedAt>=1.)
  {
   if(!bReadyRequested){Character->GetRogueAttributeSet()->RestoreVitals();bReadyRequested=true;Capture(TEXT("ready"));return;}
   if(bCapturePending)return;
   if(!bReadyCaptured){Check(false,TEXT("ready_unprocessed"));Finish();return;}
   if(C->GetAttackCooldownRemaining()>0.f||C->IsWeaponChangeBlocked())return;
   BaselineVfx();AttackSent=true;Pressed=Now;C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::Primary);
   UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX REQUEST index=%d slot=Primary native_binding=1"),Index);
  }
  const int32 Count=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Skill->GetFName());
  if(Accepted<0&&Count>Uses){Check(Count==Uses+1,TEXT("accepted_once_required"));Accepted=Now;UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX ACCEPT index=%d montage=%s"),Index,*Skill->AttackMontage->GetPathName());}
  if(Accepted<0)return;
  auto* Source=Character->GetAnimationPlaybackMesh();auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const bool Playing=Anim&&Anim->Montage_IsPlaying(Skill->AttackMontage);if(Playing)++MontageSamples;
  HandTravel=FMath::Max(HandTravel,FVector::Distance(ReadyHand,Character->GetMesh()->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation()));
  ObserveVfx(Skill->CastEffect.Get(),TEXT("cast"),TEXT("sample"));ObserveVfx(Skill->ImpactEffect.Get(),TEXT("impact"),TEXT("sample"));
  const double Health=Target->GetRogueAttributeSet()->GetHealth(),Drop=PreviousHealth-Health;
  if(Drop>.01){++PulseCount;UE_LOG(LogRogue10m,Display,TEXT("STAFFVFX HP_DROP index=%d pulse=%d frame=%d hp_before=%.6f hp_after=%.6f"),Index,PulseCount,Frame,PreviousHealth,Health);if(Index!=0)Check(false,TEXT("negative_case_damage"));}
  PreviousHealth=Health;
  if(Index!=0&&bFirstImpactObserved){Check(false,TEXT("negative_case_impact_spawned"));Finish();return;}
  Rows+=FString::Printf(TEXT("%d,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),Frame,Now,Index,CaptureMontagePosition(),Health,Drop,Character->GetActorLocation().X,Character->GetActorLocation().Y,Character->GetActorLocation().Z,FVector::Distance(StartLocation,Character->GetActorLocation()),HandTravel);
  // Each request is deferred to the renderer, never labeled as exact contact.
  if(!bCapturePending&&PulseCount>0&&!bFirstDamageRequested){bFirstDamageRequested=Capture(TEXT("after_first_damage"));}
  if(!bCapturePending&&!bAcceptanceRequested){bAcceptanceRequested=CaptureVfx(TEXT("acceptance_cue"),Skill->CastEffect.Get(),TEXT("cast"));}
  if(Index==0&&!bCapturePending&&PulseCount>0&&!bImpactResidualRequested){bImpactResidualRequested=CaptureVfx(TEXT("impact_residual"),Skill->ImpactEffect.Get(),TEXT("impact"));}
  // Keep the early diagnostic captures, then sample both cues later from the same
  // unmodified gameplay camera. This separates an early-sampling failure from
  // persistent body/weapon occlusion without hiding either in a new camera view.
  if(!bCapturePending&&!bCastMidRequested){bCastMidRequested=CaptureVfx(TEXT("cast_age_010"),Skill->CastEffect.Get(),TEXT("cast"),.10,.17);}
  if(Index==0&&!bCapturePending&&!bImpactMidRequested){bImpactMidRequested=CaptureVfx(TEXT("impact_age_010"),Skill->ImpactEffect.Get(),TEXT("impact"),.10,.17);}
  if(!bCapturePending&&!bCastLateRequested){bCastLateRequested=CaptureVfx(TEXT("cast_age_018"),Skill->CastEffect.Get(),TEXT("cast"),.18,.25);}
  if(Index==0&&!bCapturePending&&!bImpactLateRequested){bImpactLateRequested=CaptureVfx(TEXT("impact_age_018"),Skill->ImpactEffect.Get(),TEXT("impact"),.18,.25);}
  // Motion samples use accepted elapsed time and retain actual montage_s. They
  // are trajectory evidence only; they do not relabel damage as authored contact.
  if(!bCapturePending&&!bMotion030Requested&&Now-Accepted>=.30){bMotion030Requested=Capture(TEXT("motion_030"));}
  if(!bCapturePending&&!bMotion060Requested&&Now-Accepted>=.60){bMotion060Requested=Capture(TEXT("motion_060"));}
  if(!bCapturePending&&!bMotion100Requested&&Now-Accepted>=1.00){bMotion100Requested=Capture(TEXT("motion_100"));}
  const double MontagePosition=CaptureMontagePosition(),MontageDuration=Skill->AttackMontage->GetPlayLength();
  if(!bCapturePending&&!bMotionRecoveryRequested&&Playing&&MontageDuration>0.&&MontagePosition>0.&&MontagePosition>=.90*MontageDuration)
  {bMotionRecoveryRequested=Capture(TEXT("motion_recovery"));}
  if(Now-Accepted>3.&&!Playing&&!C->IsWeaponChangeBlocked()&&!bCapturePending)
  {
   Check(bAcceptanceRequested,TEXT("positive_age_cast_capture_required"));Check(MontageSamples>5&&HandTravel>2.,TEXT("continuous_rendered_motion_required"));
   Check(bCastMidRequested&&bCastLateRequested,TEXT("cast_mid_and_late_actual_age_captures_required"));
   Check(bMotion030Requested&&bMotion060Requested&&bMotion100Requested,TEXT("motion_trajectory_captures_required"));
   Check(bMotionRecoveryRequested,TEXT("active_montage_last_tenth_recovery_capture_required"));
   if(Index==0){Check(bImpactMidRequested&&bImpactLateRequested,TEXT("impact_mid_and_late_actual_age_captures_required"));}
   if(Index==0){Check(PulseCount>0&&bFirstDamageCaptured&&bImpactResidualRequested,TEXT("positive_damage_and_impact_capture_required"));}
   else{Check(PulseCount==0&&!bFirstImpactObserved,TEXT("negative_damage_impact_contract"));}
   bRecoveryRequested=true;Capture(TEXT("recovery"));
  }
 }
 void Finish()
 {
  if(bFinished)return;if(bCapturePending){bFinishRequested=true;return;}bFinished=true;
  FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessed);
  for(const auto& P:CapturePaths)Check(IFileManager::Get().FileSize(*P)>1024,TEXT("image_bytes_required"));
  Check(Index==3&&CompletedCaptures==31&&CapturePaths.Num()==31,TEXT("three_cases_thirty_one_processed_captures_required"));
  Check(FFileHelper::SaveStringToFile(Rows,*(Output/TEXT("frames.csv"))),TEXT("frames_write_failed"));
  Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("captures_write_failed"));
  Check(FFileHelper::SaveStringToFile(VfxRows,*(Output/TEXT("vfx-components.csv"))),TEXT("vfx_write_failed"));
  const FString Summary=FString::Printf(TEXT("failures=%d\ncompleted_cases=%d\ncompleted_captures=%d\ncapture_sampling_schema=2\nrequired_captures=31\ncamera_modified=0\ncombat_request=1\nphysical_input=0\nexact_contact=0\nvisual_approval=0\nNOTE=Real Primary01 requests through Combat with native profile initial unlock; no Staff mastery track exists. Native saved montage, weapon defaults and binding, no overrides. Positive HP drop plus cast/impact Niagara telemetry; invulnerable target and out-of-range miss negatives. Deferred screenshots, request and processing times differ. Original early captures are retained; mid/late cast and impact captures require actual Niagara controller ages in .10-.17/.18-.25s windows. Motion samples at accepted elapsed .30/.60/1.0s and inside the active montage final tenth preserve actual montage position/duration/normalized position and are not authored contact proof. Post-end recovery remains a separate capture. All captures retain the default third-person camera and log its view transform. Positive simulation age is not pixel approval. No full-run, physical-input, exact-contact, menu interruption, swap cancellation or first-person claim. Cold process and pinned disk hashes are launcher prerequisites; separate editor cold readback must be retained by coordinator.\n"),Failures,Index,CompletedCaptures);
  Check(FFileHelper::SaveStringToFile(Summary,*(Output/TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=STAFF_VFX_%s"),Failures==0?TEXT("PASSED"):TEXT("FAILED"));FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mBasicMonster> Target;
 TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;FTimerHandle PrepareTimer;FDelegateHandle Tick,ScreenshotProcessed;
 FString Output,Images,Rows,CaptureRows,CaptureRunId,PendingCaptureSkill,PendingCapturePhase,PendingCapturePath,VfxRows;TArray<FString> CapturePaths;
 TWeakObjectPtr<UNiagaraComponent> FirstImpactComponent,PendingCaptureComponent;TArray<TWeakObjectPtr<UNiagaraComponent>> VfxExcluded;TMap<TWeakObjectPtr<UNiagaraComponent>,FVfxObservation> VfxObservations;
 int32 Index=0,Frame=0,Failures=0,Uses=0,PulseCount=0,MontageSamples=0,CompletedCaptures=0;
 double CaseStartedAt=0,Accepted=-1,Pressed=0,PreviousHealth=0,OldDelta=1./60.,CaptureRequestedAt=0,HandTravel=0,PendingCaptureMinAge=0,PendingCaptureMaxAge=0;
 FVector StartLocation=FVector::ZeroVector,ReadyHand=FVector::ZeroVector;
 bool bFinished=false,bOwnFixed=false,OldFixed=false,AttackSent=false,bCapturePending=false,bFinishRequested=false,bReadyRequested=false,bReadyCaptured=false,bFirstDamageCaptured=false,bFirstDamageRequested=false,bAcceptanceRequested=false,bImpactResidualRequested=false,bRecoveryRequested=false,bRecoveryCaptured=false,bFirstImpactObserved=false;
 bool bCastMidRequested=false,bCastLateRequested=false,bImpactMidRequested=false,bImpactLateRequested=false,bMotion030Requested=false,bMotion060Requested=false,bMotion100Requested=false,bMotionRecoveryRequested=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts()){if(auto* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestStaffVfx"),TEXT("Real native Staff Primary01 cast/damage/impact and negative rendered evidence."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif



