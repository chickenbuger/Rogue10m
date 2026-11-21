#if WITH_EDITOR
#include "AbilitySystemComponent.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMeleeVfxComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mStylizedCharacter.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "TimerManager.h"
#include "ShaderCompiler.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace Rogue10mWeaponRuntime
{
class FRun : public TSharedFromThis<FRun>
{
public:
 void Start(UWorld* W)
 {
  World=W; FString Label=TEXT("Weapon3P");FParse::Value(FCommandLine::Get(),TEXT("RogueWeaponLabel="),Label);
  bMeleeVfxRun=FParse::Param(FCommandLine::Get(),TEXT("RogueMeleeVfxRun"));
  bMeleeVfxLogicOnly=bMeleeVfxRun&&FParse::Param(FCommandLine::Get(),TEXT("RogueMeleeVfxLogicOnly"));
  bBreakingKickAudit=bMeleeVfxRun&&FParse::Param(FCommandLine::Get(),TEXT("RogueBreakingKickQA"));
  bBreakingKickAuthored=bBreakingKickAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueBreakingKickAuthored"));
  bShoulderBashAudit=bMeleeVfxRun&&FParse::Param(FCommandLine::Get(),TEXT("RogueShoulderBashQA"));
  bShoulderBashAuthored=bShoulderBashAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueShoulderBashAuthored"));
  bCapstoneBraceRun=bMeleeVfxRun&&FParse::Param(FCommandLine::Get(),TEXT("RogueCapstoneBraceQA"));
  bCapstoneBraceRender=bCapstoneBraceRun&&!bMeleeVfxLogicOnly&&FParse::Param(FCommandLine::Get(),TEXT("RogueCapstoneBraceRender"));
  bCapstoneReleaseAudit=bCapstoneBraceRun&&FParse::Param(FCommandLine::Get(),TEXT("RogueCapstoneReleaseQA"));
  bCapstoneReleaseRender=bCapstoneReleaseAudit&&!bMeleeVfxLogicOnly&&FParse::Param(FCommandLine::Get(),TEXT("RogueCapstoneReleaseRender"));
  bSwordEquipAudit=bMeleeVfxRun&&FParse::Value(FCommandLine::Get(),TEXT("RogueSwordEquipAppearance="),SwordEquipAppearance);
  bSwordEquipWalking=bSwordEquipAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueSwordEquipWalking"));
  bSwordEquipMovingBoth=bSwordEquipWalking&&FParse::Param(FCommandLine::Get(),TEXT("RogueSwordEquipMovingBoth"));
  bSwordTechniqueAudit=FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordTechniqueQA"));
  bEarthSplitterRender=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordEarthSplitterCapture"));
  bEarthSplitterMoving=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordEarthSplitterMoving"));
  bEarthSplitterWall=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordEarthSplitterWall"));
  bEarthSplitterAudit=bSwordTechniqueAudit&&(bEarthSplitterRender||bEarthSplitterMoving||bEarthSplitterWall
   ||FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordEarthSplitterAudit")));
  if(bEarthSplitterAudit){FParse::Value(FCommandLine::Get(),TEXT("RogueGreatSwordEarthSplitterAppearance="),EarthSplitterAppearance);}
  const bool bWhirlwindCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordWhirlwindCapture"));
  const bool bGuardBreakCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordGuardBreakCapture"));
  bCrushingCapture=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordCrushingSlashCapture"));
  bCrushingEdge=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordCrushingEdgeQA"));
  bCrushingStock=bSwordTechniqueAudit&&FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordCrushingStockReference"));
  bSwordTechniqueRender=bSwordTechniqueAudit&&(bWhirlwindCapture||bGuardBreakCapture||bCrushingCapture||bEarthSplitterRender);
  bSwordGuardBreakAudit=bSwordTechniqueAudit&&(bGuardBreakCapture||FParse::Param(FCommandLine::Get(),TEXT("RogueGreatSwordGuardBreakAudit")));
  const bool bNullRhi=FParse::Param(FCommandLine::Get(),TEXT("nullrhi"));
  const bool bReleaseModeOK=bCapstoneReleaseRender?(!bNullRhi&&!bMeleeVfxLogicOnly):(bNullRhi&&bMeleeVfxLogicOnly);
  if(bCapstoneReleaseAudit&&(bCapstoneBraceRender||!bReleaseModeOK))
  {
   bFinished=true;
   UE_LOG(LogRogue10m,Error,TEXT("RESULT=CAPSTONE_RELEASE_FAILED reason=select_dedicated_nullrhi_or_render_mode"));
   FPlatformMisc::RequestExitWithStatus(false,1);return;
  }
  const bool bTechniqueModeOK=bSwordTechniqueRender?(!bMeleeVfxLogicOnly&&!bNullRhi):(bMeleeVfxLogicOnly&&bNullRhi);
  const bool bConflictingTechnique=(bSwordGuardBreakAudit&&(bWhirlwindCapture||bCrushingCapture||bCrushingEdge))
   ||(bWhirlwindCapture&&(bCrushingCapture||bCrushingEdge))||(bCrushingStock&&(!bCrushingEdge||!bMeleeVfxLogicOnly));
  if(bShoulderBashAudit&&(bBreakingKickAudit||bSwordTechniqueAudit||bSwordEquipAudit||bCapstoneBraceRun
   ||FParse::Param(FCommandLine::Get(),TEXT("RogueRequireAuthoredSword"))||bMeleeVfxLogicOnly!=bNullRhi))
  {
   bFinished=true;
   UE_LOG(LogRogue10m,Error,TEXT("RESULT=SHOULDER_BASH_FAILED reason=select_dedicated_nullrhi_or_render_mode"));
   FPlatformMisc::RequestExitWithStatus(false,1);
   return;
  }
  if(bSwordTechniqueAudit&&(!bMeleeVfxRun||bSwordEquipAudit||bCapstoneBraceRun||!bTechniqueModeOK||bConflictingTechnique))
  {
   bFinished=true;
   UE_LOG(LogRogue10m,Error,TEXT("RESULT=GREAT_SWORD_TECHNIQUES_FAILED reason=select_dedicated_nullrhi_or_render_mode"));
   FPlatformMisc::RequestExitWithStatus(false,1);
   return;
  }
  if(bMeleeVfxRun)
  {
   FString ProfileSlot,ProgressionPrefix,KeySlot;
   FParse::Value(FCommandLine::Get(),TEXT("RogueProfileSaveSlot="),ProfileSlot);
   FParse::Value(FCommandLine::Get(),TEXT("RogueProgressionSavePrefix="),ProgressionPrefix);
   FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),KeySlot);
   if(Label.IsEmpty()||W->GetNetMode()!=NM_Standalone||ProfileSlot!=TEXT("Rogue10m_Town_QA_MeleeVfx_")+Label||
    ProgressionPrefix!=TEXT("Rogue10m_Progression_QA_MeleeVfx_")+Label||KeySlot!=TEXT("Rogue10m_Fighter_QA_MeleeVfx_")+Label)
   {
    bFinished=true;
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=MELEE_VFX_FAILED reason=dedicated_save_namespace_required"));
    FPlatformMisc::RequestExitWithStatus(false,1);return;
   }
  }
  Output=FPaths::ProjectDir()/(bMeleeVfxRun?TEXT("tmp/MeleeVfxQARuns"):TEXT("Feature/doc/evidence/sword-knuckle-20261110"))/FPaths::MakeValidFileName(Label);
  Images=bMeleeVfxRun?Output/TEXT("images"):FPaths::ProjectDir()/TEXT("DevLog/20261110_SwordKnuckle")/FPaths::MakeValidFileName(Label);
  if(bSwordTechniqueAudit&&IFileManager::Get().DirectoryExists(*Output))
  {
   bFinished=true;
   UE_LOG(LogRogue10m,Error,TEXT("RESULT=GREAT_SWORD_TECHNIQUES_FAILED reason=fresh_output_required"));
   FPlatformMisc::RequestExitWithStatus(false,1);
   return;
  }
  IFileManager::Get().MakeDirectory(*Output,true);IFileManager::Get().MakeDirectory(*Images,true);
  CaptureRunId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
  bRequireAuthoredSword=FParse::Param(FCommandLine::Get(),TEXT("RogueRequireAuthoredSword"));
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P MODE require_authored_sword=%d criterion=first_HP_drop_world_post_actor_tick_blade_capsule_surface_gap_le_10cm abstract_capsule_timing_proxy_only=1"),bRequireAuthoredSword?1:0);
  Rows=TEXT("frame,world_s,skill,phase,montage_s,health,damage,elapsed,expected_delay,blade_target_cm,hand_r_x,hand_r_y,hand_r_z,hand_l_x,hand_l_y,hand_l_z,blade_base_x,blade_base_y,blade_base_z,blade_tip_x,blade_tip_y,blade_tip_z,min_foot_world_z,capsule_bottom_world_z,foot_above_capsule_bottom_cm,target_center_x,target_center_y,target_center_z,target_radius_cm,target_halfheight_cm,target_up_x,target_up_y,target_up_z,sword_capsule_gap_cm,montage_source_s,montage_source_path\n");
  CaptureRows=TEXT("event,skill,phase,fixture_frame,world_s,montage_s,file,bytes\n");
  VfxRows=TEXT("event,skill,kind,fixture_frame,world_s,world_path,component_path,asset_path,attach_socket,first_seen_frame,observed_frame_delta,controller_valid,actual_sim_age_s,age_source,active,visible,registered,complete,eligible,location_x,location_y,location_z,scale_x,scale_y,scale_z,bounds_origin_x,bounds_origin_y,bounds_origin_z,bounds_extent_x,bounds_extent_y,bounds_extent_z,custom_time_dilation,damage_pulses_observed,first_observed_damage_pulse,pinned_first_impact,attach_parent_path,expected_3p_scale,third_person_parent_valid,third_person_scale_valid\n");
  MeleeVfxRows=TEXT("case,skill,frame,world_s,observed_damage,range_count,impact_count,trail_samples,trail_triangles,range_triangles,impact_triangles,tier,outline_vertices,outline_diameter_cm,capsule_surface_gap_cm\n");
  if(bSwordTechniqueAudit)
  {
   SwordTechniquePoses=TEXT("case,skill,case_frame,elapsed,montage_s,montage_weight,mesh,revision,bone,x,y,z,qx,qy,qz,qw\n");
   SwordTechniqueActor=TEXT("case,skill,case_frame,elapsed,montage_s,actor_x,actor_y,actor_z,actor_yaw\n");
  }
  if(bSwordTechniqueRender||bCapstoneReleaseRender||(IsFocusedKnuckleCase()&&!bMeleeVfxLogicOnly))
  {
   TechniqueViewport=GEngine?GEngine->GameViewport:nullptr;
   Check(TechniqueViewport.IsValid(),TEXT("technique_capture_game_viewport_required"));
   if(Failures){Finish();return;}
   bPreviousWorldRenderingDisabled=TechniqueViewport->bDisableWorldRendering;
   TechniqueViewport->bDisableWorldRendering=true;
  }
  if(bBreakingKickAudit){BreakingKickPoseRows=TEXT("frame,world_s,montage_s,damage,mesh,bone,x,y,z,actor_x,actor_y,actor_z,toe_capsule_gap_cm\n");}
  if(bShoulderBashAudit)
  {
   ShoulderBashPoseRows=TEXT("frame,world_s,montage_s,source_s,damage,source_path,mesh,bone,x,y,z,actor_x,actor_y,actor_z,")
    TEXT("forward_x,forward_y,forward_z,forward_cm,shoulder_lead_cm\n");
  }
  if(bMeleeVfxRun)
  {
   FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){Self->Prepare();return false;}),4.f);
  }
  else{W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);}
 }
 bool Finished()const{return bFinished;}
private:
 void Check(bool B,const TCHAR* Reason){if(!B){++Failures;UE_LOG(LogRogue10m,Error,TEXT("WEAPON3P FAIL skill=%d frame=%d %s"),Index,Frame,*FString(Reason));}}
 static constexpr int32 TechniqueCount=4;
 static constexpr double GuardBreakDistances[]={85.,110.,140.};
 static constexpr double CrushingEdgeDistance=205.;
 static constexpr int32 GuardBreakCaseCount=UE_ARRAY_COUNT(GuardBreakDistances)+1;
 bool IsEarthSplitterCase() const
 {
  return bSwordTechniqueAudit&&Skills.IsValidIndex(Index)&&Skills[Index].IsValid()&&Skills[Index]->GetFName()==TEXT("DA_Hybrid_GreatSword_EarthSplitter");
 }
  bool IsFocusedKnuckleCase() const {return bBreakingKickAudit||bShoulderBashAudit;}
  int32 FocusedKnuckleCaseIndex() const {return bShoulderBashAudit?ShoulderBashIndex:BreakingKickIndex;}
 bool IsSwordCase() const {return bSwordTechniqueAudit||Index<5||(Index>=10&&Index<12);}
 bool IsMeleeMissCase() const
 {
  if(bSwordGuardBreakAudit){return Index==GuardBreakCaseCount-1;}
  return bSwordTechniqueAudit?Index>=TechniqueCount:bMeleeVfxRun&&(Index==11||Index==13);
 }
 bool IsMeleeMotionCase() const {return !bSwordTechniqueAudit&&bMeleeVfxRun&&(Index==0||Index==5||Index==10||Index==12);}
 void SendTechniqueKey(bool bPressed)
 {
  const FKey Key=CaseSlot==ERogue10mAttackInputSlot::ChargedSpecial?EKeys::RightMouseButton:EKeys::Q;
  Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,bPressed?IE_Pressed:IE_Released,bPressed?1.f:0.f));
 }
 float CaptureMontagePosition() const
 {
  const auto* Source=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const auto* Skill=bCapstoneBraceRender||bCapstoneReleaseRender?BraceSkill.Get():(Skills.IsValidIndex(Index)?Skills[Index].Get():nullptr);
  return Anim&&Skill?Anim->Montage_GetPosition(Skill->AttackMontage):-1.f;
 }
 bool Capture(const TCHAR* Phase)
 {
  if(bMeleeVfxLogicOnly)
  {
   // Run the same phase checks without counting screenshots on a NullRHI session.
   CompletePhase(FString(Phase),true);
   return true;
  }
  if(bCapturePending||FScreenshotRequest::IsScreenshotRequested()){Check(false,TEXT("screenshot_request_already_pending"));return false;}
  PendingCaptureSkill=Skills.IsValidIndex(Index)&&Skills[Index].IsValid()?Skills[Index]->GetName():TEXT("setup");
  if(bCapstoneBraceRender){PendingCaptureSkill=BraceCaseNames[Index];}
  if(bCapstoneReleaseRender){PendingCaptureSkill=TEXT("capstone");}
  if(IsMeleeMissCase()){PendingCaptureSkill+=TEXT("_Outside");}
  PendingCapturePhase=Phase;
  // A unique file cannot be mistaken for a successful capture left by an earlier run.
  PendingCapturePath=FPaths::ConvertRelativePathToFull(Images/(PendingCaptureSkill+TEXT("_")+Phase+TEXT("_")+CaptureRunId+TEXT(".png")));
  if(IFileManager::Get().FileExists(*PendingCapturePath)){Check(false,TEXT("screenshot_file_not_unique"));return false;}
  bCapturePending=true;CaptureRequestedAt=FPlatformTime::Seconds();CapturePaths.Add(PendingCapturePath);
  CaptureRows+=FString::Printf(TEXT("request,%s,%s,%d,%.6f,%.6f,%s,-1\n"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P CAPTURE_REQUEST skill=%s phase=%s frame=%d world=%.6f montage=%.6f file=%s"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
  if(TechniqueViewport.IsValid()){TechniqueViewport->bDisableWorldRendering=false;}
  FScreenshotRequest::RequestScreenshot(PendingCapturePath,!bSwordTechniqueRender&&!bCapstoneReleaseRender&&!IsFocusedKnuckleCase(),false);
  return true;
 }
 void CaptureProcessed()
 {
  if(!bCapturePending){return;}
  // UE broadcasts after the save attempt, including failed attempts. Require actual bytes too.
  const int64 Bytes=IFileManager::Get().FileSize(*PendingCapturePath);
  const bool bSaved=Bytes>0;Check(bSaved,TEXT("processed_screenshot_file_missing_or_empty"));
  CaptureRows+=FString::Printf(TEXT("processed,%s,%s,%d,%.6f,%.6f,%s,%lld\n"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath,static_cast<long long>(Bytes));
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P CAPTURE_PROCESSED skill=%s phase=%s frame=%d world=%.6f montage=%.6f bytes=%lld"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),static_cast<long long>(Bytes));
  if(bSaved){++CompletedCaptures;}
  CompletePhase(PendingCapturePhase,bSaved);
  bCapturePending=false;
  if(!bSaved){bFinishRequested=true;}
 }
 void CompletePhase(const FString& Phase,bool bCompleted)
 {
  if(Phase==TEXT("ready")){bReadyCaptured=bCompleted;}
  else if(Phase==TEXT("hold_peak")&&bCapstoneBraceRender){bBracePeakCaptured=bCompleted;}
  else if(Phase==TEXT("after_first_damage")||Phase==TEXT("contact_miss")){bFirstDamageCaptured=bCompleted;}
  else if(Phase==TEXT("overhead_load")){bCrushingLoadCaptured=bCompleted;}
  else if(Phase==TEXT("motion")){bMotionCaptured=bCompleted;}
  else if(Phase==TEXT("recovery")){bRecoveryCaptured=bCompleted;}
 }
 // Read-only, per-component evidence. Exact asset pointers distinguish cast from impact.
 // Production spawns these transient components without pooling; exclude pre-input instances.
 struct FVfxObservation
 {
  TWeakObjectPtr<UNiagaraComponent> Component;
  int32 FirstFrame=0,LastFrame=0,FirstDamagePulse=0;bool bPresentationFailureReported=false;
 };
 bool IsVfxCase() const {return !bMeleeVfxRun&&(Index==4||Index==7);}
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
    UE_LOG(LogRogue10m,Error,TEXT("WEAPON3P VFX_PRESENTATION_MISMATCH skill=%s kind=%s component=%s parent=%s expected_body=%s expected_3p_scale=%.6f actual_world_scale=%s third_person=%d parent_valid=%d scale_valid=%d"),
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
 bool CaptureVfx(const TCHAR* Phase,UNiagaraSystem* Asset,const TCHAR* Kind)
 {
  const auto Ready=ObserveVfx(Asset,Kind,Phase);
  if(!Ready.IsValid()){return false;}
  const auto Instance=Ready->GetSystemInstanceController();
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P VFX_GATE skill=%s phase=%s kind=%s component=%s asset=%s frame=%d observed_frames=%d actual_sim_age=%.6f age_source=FNiagaraSystemInstanceController::GetAge first_observed_damage_pulse=%d custom_time_dilation=%.6f visual_approval=0"),
   *Skills[Index]->GetName(),Phase,Kind,*Ready->GetPathName(),*Asset->GetPathName(),Frame,Frame-VfxObservations.FindChecked(Ready).FirstFrame,static_cast<double>(Instance->GetAge()),VfxObservations.FindChecked(Ready).FirstDamagePulse,static_cast<double>(Ready->GetCustomTimeDilation()));
  return Capture(Phase);
 }
 bool AddMeleeVfxSkillCases(ARogue10mPlayerState& State, URogue10mSkillTreeProgressionComponent& Tree)
 {
  // Use a validated run snapshot so the test map does not need a temporary trainer.
  FRogue10mGrowthSnapshot Growth=State.CaptureGrowth();
  Growth.Level=15;
  if(!State.RestoreGrowth(Growth)){return false;}
  FRogue10mSkillTreeSnapshot Snapshot=Tree.Capture();
  for(const TCHAR* Node:{TEXT("G.Whirlwind"),TEXT("G.GuardBreak"),TEXT("G.EarthSplitter"),TEXT("G.Ultimate")})
  {
   auto& Investment=Snapshot.Investments.AddDefaulted_GetRef();
   Investment.NodeId=Node;
   Investment.Route=ERogue10mSkillTreeUnlockRoute::Points;
   Investment.SpentPoints=1;
  }
  if(!Tree.Restore(Snapshot)){return false;}
  for(const TCHAR* Node:{TEXT("G.Ultimate"),TEXT("F.Ultimate")})
  {
   const auto* Entry=Tree.FindNode(Node);
   auto* Skill=Entry?Entry->Skill.LoadSynchronous():nullptr;
   if(!Skill||!Skill->AttackMontage||!Tree.IsSkillGranted(Skill)){return false;}
   Skills.Add(Skill);
   Skills.Add(Skill);
  }
  return true;
 }
 bool LearnSwordTechniqueCases()
 {
  auto* Inventory=Character->GetInventoryComponent();
  auto* Item=LoadObject<URogue10mItemDataAsset>(nullptr,
   TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  auto* Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
  TActorIterator<ARogue10mTutorialNPC> Guide(World.Get());
  int32 Grid=INDEX_NONE;
  FGuid ItemId;
  const bool bReady=Inventory&&Item&&Tree&&Guide&&Character->CanChangeWeapon();
  Check(bReady,TEXT("sword_technique_spawn_settled_and_services_ready"));
  if(!bReady){return false;}
  const bool bEquipped=Inventory->TryAddGridItem(Item,1,Grid,ItemId)&&Inventory->TryEquipGridItem(Grid,ItemId)
   &&Character->GetEquippedWeaponType()==ERogue10mWeaponType::GreatSword;
  Check(bEquipped,TEXT("sword_technique_actual_inventory_equip"));
  if(!bEquipped){return false;}
  Character->SetActorLocation(Guide->GetActorLocation()+FVector(100.,0.,100.),false,nullptr,ETeleportType::TeleportPhysics);
  auto Growth=State->CaptureGrowth();
  Growth.Level=15;
  if(!State->RestoreGrowth(Growth)){return false;}
  auto Snapshot=Tree->Capture();
  auto* Mastery=Snapshot.Masteries.FindByPredicate([](const auto& Record){return Record.Track==ERogue10mSkillTreeTrack::GreatSword;});
  if(!Mastery){Mastery=&Snapshot.Masteries.AddDefaulted_GetRef();}
  Mastery->Track=ERogue10mSkillTreeTrack::GreatSword;
  Mastery->TotalExperience=1045;
  if(!Tree->Restore(Snapshot)||!Tree->CanChangeInvestments()){return false;}
  const int32 Points=Tree->GetAvailableGrowthPoints();
  const TCHAR* Nodes[]={TEXT("G.CrushingSlash"),TEXT("G.Whirlwind"),TEXT("G.GuardBreak"),TEXT("G.EarthSplitter")};
  for(const TCHAR* Node:Nodes)
  {
   const auto* Entry=Tree->FindNode(Node);
   if(!Entry||!Tree->TryUnlockNode(Node,ERogue10mSkillTreeUnlockRoute::Points,NAME_None))
   {
    FText Reason;
    Tree->CanUnlockNode(Node,ERogue10mSkillTreeUnlockRoute::Points,NAME_None,Reason);
    UE_LOG(LogRogue10m,Error,TEXT("GREAT_SWORD_TECHNIQUE could not learn %s reason=%s"),Node,*Reason.ToString());
    return false;
   }
   auto* Skill=Entry->Skill.LoadSynchronous();
   if(!Skill||!Skill->AttackMontage||!Tree->IsSkillGranted(Skill)){return false;}
   const bool bCrushingSkill=Skill->GetFName()==TEXT("DA_Hybrid_GreatSword_CrushingSlash");
   if(bCrushingStock&&bCrushingSkill)
   {
    // The edge comparison uses the authored baseline montage on the same isolated skill and query.
    auto* Stock=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Primary03.AM_GreatSword_Primary03"));
    if(!Stock){return false;}
    Skill->AttackMontage=Stock;
   }
   if(bSwordGuardBreakAudit&&Skill->GetFName()!=TEXT("DA_Hybrid_GreatSword_GuardBreak")){continue;}
   if(bCrushingCapture&&!bCrushingSkill){continue;}
   if(bEarthSplitterRender&&Skill->GetFName()!=TEXT("DA_Hybrid_GreatSword_EarthSplitter")){continue;}
   if(bSwordTechniqueRender&&!bEarthSplitterRender&&!bSwordGuardBreakAudit&&!bCrushingCapture&&Skill->GetFName()!=TEXT("DA_Hybrid_GreatSword_Whirlwind")){continue;}
   Skills.Add(Skill);
  }
  if(Tree->GetAvailableGrowthPoints()!=Points-TechniqueCount){return false;}
  if(bSwordTechniqueRender){return Skills.Num()==1;}
  if(bSwordGuardBreakAudit)
  {
   if(Skills.Num()!=1){return false;}
   const auto Skill=Skills[0];
   while(Skills.Num()<GuardBreakCaseCount){Skills.Add(Skill);}
   return true;
  }
  for(int32 I=0;I<TechniqueCount;++I){const auto Skill=Skills[I];Skills.Add(Skill);}
  return true;
 }

 bool EnterMeleeProfile()
 {
  // StartHub uses the normal character lobby.
  auto* Instance=World->GetGameInstance();
  auto* Profiles=Instance?Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
  auto* Saves=Instance?Instance->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  FRogue10mCharacterProfile Profile;
  FRogue10mCharacterAppearance Appearance;
  if(bSwordEquipAudit||!EarthSplitterAppearance.IsEmpty())
  {
   const FString& AppearanceName=bSwordEquipAudit?SwordEquipAppearance:EarthSplitterAppearance;
   if(AppearanceName.StartsWith(TEXT("Dwarf"))){Appearance.Race=ERogue10mCharacterRace::Dwarf;}
   if(AppearanceName.StartsWith(TEXT("Orc"))){Appearance.Race=ERogue10mCharacterRace::Orc;}
   if(AppearanceName.EndsWith(TEXT("Female"))){Appearance.Gender=ERogue10mCharacterGender::Female;}
  }
  if(!Controller.IsValid()||!Profiles||!Saves||Profiles->GetProfileCount()!=0||
   !Profiles->CreateProfile(TEXT("MeleeVfxQA"),Appearance,Profile)||!Profiles->SelectProfile(Profile.ProfileId)){return false;}
  Controller->EnterSelectedCharacter();
  Controller->CloseAllBlockingPanels();
  Character=Cast<ARogue10mCharacter>(Controller->GetPawn());
  if(!Character.IsValid()||!Saves->IsAttachedTo(*Character)){return false;}
  return true;
 }

 bool PrepareMeleeFixture()
 {
  if(!bSwordTechniqueAudit&&!IsFocusedKnuckleCase()&&!EnterMeleeProfile()){return false;}
  auto* Instance=World->GetGameInstance();
  auto* Saves=Instance?Instance->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  if(!Character.IsValid()||!Saves||!Saves->IsAttachedTo(*Character)){return false;}
  if(IsFocusedKnuckleCase())
  {
   auto* Inventory=Character->GetInventoryComponent();
   auto* Item=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_StarterKnuckle"));
   int32 Grid=INDEX_NONE;FGuid ItemId;
   if(!Inventory||!Item||!Inventory->TryAddGridItem(Item,1,Grid,ItemId)||!Inventory->TryEquipGridItem(Grid,ItemId)
    ||Character->GetEquippedWeaponType()!=ERogue10mWeaponType::Knuckle){return false;}
  }
  if(bSwordTechniqueAudit&&!LearnSwordTechniqueCases())
  {
   Check(false,TEXT("sword_technique_inventory_and_real_point_investments_required"));
   return false;
  }
  Saves->DetachCharacter(*Character,false);

  // Use the same flat, transient test surface as the third-person and identity runtime fixtures.
  const FVector Start=Character->GetActorLocation();
  const double Surface=Start.Z+1800.;
  FActorSpawnParameters Params;
  Params.ObjectFlags|=RF_Transient;
  auto* Floor=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(FVector(Start.X,Start.Y,Surface-20.)),Params);
  auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  if(!Floor||!Cube){return false;}
  auto* Shape=NewObject<UBoxComponent>(Floor,NAME_None,RF_Transient);
  Floor->AddInstanceComponent(Shape);
  Floor->SetRootComponent(Shape);
  Shape->SetBoxExtent(FVector(2500.,2500.,20.));
  Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  Shape->SetCollisionObjectType(ECC_WorldStatic);
  Shape->SetCollisionResponseToAllChannels(ECR_Block);
  Shape->SetGenerateOverlapEvents(false);
  Shape->SetCanEverAffectNavigation(false);
  Shape->RegisterComponent();
  auto* Mesh=NewObject<UStaticMeshComponent>(Floor,NAME_None,RF_Transient);
  Floor->AddInstanceComponent(Mesh);
  Mesh->SetupAttachment(Shape);
  Mesh->SetStaticMesh(Cube);
  Mesh->SetRelativeScale3D(FVector(50.,50.,.4));
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Mesh->RegisterComponent();
  Floor->SetActorLocation(FVector(Start.X,Start.Y,Surface-20.));
  const double HalfHeight=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  Character->SetActorLocation(FVector(Start.X,Start.Y,Surface+HalfHeight),false,nullptr,ETeleportType::TeleportPhysics);
  return true;
 }
 void Prepare()
 {
  Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
  if((bSwordTechniqueAudit||IsFocusedKnuckleCase())&&!Character.IsValid())
  {
   Check(EnterMeleeProfile(),TEXT("technique_live_profile_required"));
   if(Failures){Finish();return;}
   World->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),.25f,false);
   return;
  }
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  if(bMeleeVfxRun)
  {
   Check(PrepareMeleeFixture(),TEXT("fresh_profile_start_hub_and_test_floor_required"));
   if(Failures){Finish();return;}
  }
  else
  {
   auto* Old=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
   UClass* Class=LoadClass<ARogue10mCharacter>(nullptr,TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
   if(!Old||!Class){Check(false,TEXT("fixture_character_missing"));Finish();return;}
   const FTransform T=Old->GetActorTransform();Old->SetActorEnableCollision(false);Old->SetActorHiddenInGame(true);
   Character=World->SpawnActor<ARogue10mCharacter>(Class,T,P);Controller->Possess(Character.Get());Controller->SetViewTarget(Character.Get());
   if(auto* S=Cast<ARogue10mStylizedCharacter>(Character.Get()))
   {
    FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("WeaponQA");Profile.Appearance.Race=S->GetAppearanceRace();Profile.Appearance.Gender=S->GetAppearanceGender();Check(S->ApplyCharacterProfile(Profile),TEXT("profile_failed"));
   }
  }
  Character->SetCanBeDamaged(false);
  if(!bCapstoneBraceRun&&!bSwordEquipWalking&&!bSwordTechniqueAudit&&!IsFocusedKnuckleCase()){Character->DisableInput(Controller.Get());}
  Character->GetCharacterMovement()->StopMovementImmediately();
  Character->SetActorRotation(FRotator(0,0,0));Controller->SetControlRotation(FRotator(-8,0,0));
  // The current user-requested default is third person. This fixture never toggles into first person.
  Check(Character->IsThirdPersonGameplayActive(),TEXT("third_person_mode_required"));
  Character->GetMesh()->SetOwnerNoSee(false);Character->GetMesh()->SetVisibility(true,false);
  if(bMeleeVfxLogicOnly||bSwordTechniqueRender||bCapstoneReleaseRender||IsFocusedKnuckleCase())
  {
   Character->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   if(auto* Playback=Character->GetAnimationPlaybackMesh())
   {Playback->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;}
  }
  for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It){It->ClearAITarget();if(It->GetController()){It->GetController()->UnPossess();}It->SetActorEnableCollision(false);It->GetCharacterMovement()->DisableMovement();}
  if(bSwordEquipAudit){PrepareSwordEquipAudit();return;}
  Target=World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),Character->GetActorLocation()+FVector(110,0,0),FRotator::ZeroRotator,P);
  if(!Target.IsValid()){Check(false,TEXT("target_failed"));Finish();return;}
  Target->GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
  Target->GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));
  if(bMeleeVfxRun){Target->Tags.AddUnique(TEXT("TrainingTarget"));}
  Target->ClearAITarget();if(Target->GetController()){Target->GetController()->UnPossess();}Target->GetCharacterMovement()->DisableMovement();
  if(auto* R=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()){R->ConfigureRegeneration(0,0,0);}
  Target->GetRogueAttributeSet()->SetMaxHealth(100000);Target->GetRogueAttributeSet()->SetHealth(100000);
  Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
  if(!bSwordTechniqueAudit)
  {
  for(const TCHAR* Weapon:{TEXT("GreatSword"),TEXT("Knuckle")})for(const TCHAR* Variant:{TEXT("Primary01"),TEXT("Primary02"),TEXT("Primary03"),TEXT("Special"),TEXT("Charged")})
  {
   const FString Path=FString::Printf(TEXT("/Game/DataAsset/AttackSkill/ClassCombat/%s/DA_Attack_%s_%s"),Weapon,Weapon,Variant);
   auto* S=LoadObject<URogue10mAttackSkillData>(nullptr,*Path);
   if(!S||!S->AttackMontage){Check(false,TEXT("skill_missing"));Finish();return;}
   if(bShoulderBashAudit&&!bShoulderBashAuthored&&S->GetFName()==TEXT("DA_Attack_Knuckle_Charged"))
   {
    // Keep identical input and combat conditions while sampling the original asset in this test process.
    auto* Original=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Knuckle_ShortBlow.AM_Knuckle_ShortBlow"));
    if(!Original){Check(false,TEXT("shoulder_bash_stock_montage_missing"));Finish();return;}
    S->AttackMontage=Original;
   }
   Skills.Add(S);
   if(bRequireAuthoredSword&&FString(Weapon)==TEXT("GreatSword"))
   {
    const bool bAuthored=S->AttackMontage->GetPathName().StartsWith(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/AM_"));
    const bool bPositiveDelay=FMath::IsFinite(S->HitStartDelaySeconds)&&S->HitStartDelaySeconds>0.f;
    Check(bAuthored,TEXT("authored_sword_montage_path_required"));Check(bPositiveDelay,TEXT("authored_sword_positive_hit_delay_required"));
    if(bAuthored&&bPositiveDelay){++AuthoredSwordValidated;}
    UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P AUTHORED skill=%s montage=%s hit_start_delay=%.6f valid=%d"),*S->GetName(),*S->AttackMontage->GetPathName(),S->HitStartDelaySeconds,bAuthored&&bPositiveDelay?1:0);
   }
  }
  // Valid isolated progression, not a skill-policy override or temporary preview unlock.
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  auto* Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
  Check(Tree!=nullptr,TEXT("fixture_progression_missing"));if(!Tree){Finish();return;}
  FRogue10mSkillTreeSnapshot Snapshot;
  // The catalog determines the automatic grants. The VFX cases also exercise the two capstones.
  for(auto Track:{ERogue10mSkillTreeTrack::GreatSword,ERogue10mSkillTreeTrack::Brawler})
  {auto& Mastery=Snapshot.Masteries.AddDefaulted_GetRef();Mastery.Track=Track;Mastery.TotalExperience=bMeleeVfxRun?1045:450;}
  Check(Tree->Restore(Snapshot),TEXT("fixture_valid_mastery_restore_failed"));
  if(bMeleeVfxRun)
  {
   Check(AddMeleeVfxSkillCases(*State,*Tree),TEXT("fixture_two_validated_capstone_grants_required"));
  }
  for(const auto& Entry:Skills){Check(Tree->IsSkillGranted(Entry.Get()),TEXT("fixture_catalog_skill_not_granted"));}
  if(bMeleeVfxRun){UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P FIXTURE progression=validated_M20_1045XP_each four_point_sword_route identity=None"));}
  else{UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P FIXTURE progression=validated_M13_450XP_each automatic_grants_only identity=None"));}
  }
  if(Failures){Finish();return;}
  TSet<UNiagaraSystem*> Effects;
  for(const auto& Entry:Skills){const auto* Skill=Entry.Get();for(UNiagaraSystem* FX:{Skill->CastEffect.Get(),Skill->ChargeEffect.Get(),Skill->ImpactEffect.Get()}){if(FX){Effects.Add(FX);}}}
  for(UNiagaraSystem* FX:Effects){FX->RequestCompile(false);FX->WaitForCompilationComplete(true,false);}
  if(bMeleeVfxRun)
  {
   // Resolve the production material on the idle frame, before timed animation samples or screenshots.
   const auto* Material=LoadObject<UMaterialInterface>(nullptr,
    TEXT("/Game/Rogue10m/VFX/Character/Combat/Melee20261121/M_MeleeReadability.M_MeleeReadability"));
   Check(Material!=nullptr,TEXT("production_melee_material_available_for_warmup"));
   if(Failures){Finish();return;}
   FAssetCompilingManager::Get().FinishAllCompilation();
  }
  if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
  if(bSwordTechniqueRender||(IsFocusedKnuckleCase()&&!bMeleeVfxLogicOnly))
  {
   const FVector Origin=Character->GetActorLocation();
   const FVector Forward=Character->GetActorForwardVector();
   const FVector Right=Character->GetActorRightVector();
   const bool bFrontalCapture=bSwordGuardBreakAudit||bCrushingCapture||bEarthSplitterRender;
   const FVector Position=IsFocusedKnuckleCase()?Origin+Forward*136.+Right*(bShoulderBashAudit?365.:-365.)+FVector(0,0,48):
    bFrontalCapture?Origin+Forward*120.-Right*420.+FVector(0,0,85):
    Origin-Forward*290.-Right*300.+FVector(0,0,65);
   const FVector Focus=IsFocusedKnuckleCase()?Origin+Forward*52.+FVector(0,0,2):
    Origin+FVector(0,0,35)+(bFrontalCapture?Forward*(bCrushingEdge?100.:52.):FVector::ZeroVector);
   FActorSpawnParameters CameraParams;
   CameraParams.ObjectFlags|=RF_Transient;
   auto* Camera=World->SpawnActor<ACameraActor>(Position,(Focus-Position).Rotation(),CameraParams);
   Check(Camera!=nullptr,TEXT("technique_capture_fixed_camera_spawned"));
   if(!Camera){Finish();return;}
   Camera->GetCameraComponent()->SetFieldOfView(IsFocusedKnuckleCase()?55.f:63.f);
   Controller->SetViewTarget(Camera);
  }
  OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);bOwnFixed=true;
  ScreenshotProcessed=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->CaptureProcessed();});
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
  if(bCapstoneBraceRun){PrepareCapstoneBrace();}
  else{if(IsFocusedKnuckleCase()){Index=FocusedKnuckleCaseIndex();}Next();}
 }
 enum class EBraceStep : uint8 { Settle, Hold, Release, PitchHigh, PitchLow, Cancel, Recenter, RenderCapture };
 static constexpr int32 BraceCaseCount=4;
 static constexpr int32 BraceRenderCaseCount=2;
 static constexpr float BraceRenderHoldSeconds=20.f/60.f;
 static constexpr const TCHAR* BraceCaseNames[]={TEXT("stock"),TEXT("brace"),TEXT("early"),TEXT("pitch_cancel")};
 const TCHAR* BraceCaseLabel() const {return bCapstoneReleaseRender?TEXT("brace_release"):BraceCaseNames[Index];}
 static constexpr const TCHAR* BraceBones[]={TEXT("hand_l"),TEXT("hand_r"),TEXT("clavicle_l"),TEXT("clavicle_r"),
  TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("foot_l"),TEXT("foot_r"),
  TEXT("middle_01_l"),TEXT("middle_01_r"),TEXT("pelvis")};

 struct FBracePose
 {
  FTransform Bones[UE_ARRAY_COUNT(BraceBones)];
  FVector BladeBase=FVector::ZeroVector,BladeTip=FVector::ZeroVector;
  uint32 Revision=0;
 };

 struct FBraceHoldSample
 {
  float HoldSeconds;
  FVector2D FootTravel;
  double AxialTravel;
  double BladeAngle;
 };

 static double BraceBoneTravel(const FBracePose& A,const FBracePose& B,int32 Bone)
 {return FVector::Distance(A.Bones[Bone].GetLocation(),B.Bones[Bone].GetLocation());}

 static double BraceBladeAngle(const FBracePose& A,const FBracePose& B)
 {
  const FVector AAxis=(A.BladeTip-A.BladeBase).GetSafeNormal(),BAxis=(B.BladeTip-B.BladeBase).GetSafeNormal();
  return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(AAxis,BAxis),-1.0,1.0)));
 }

 static double BraceLeftWristAxialPosition(const FBracePose& Pose)
 {
  const FVector Axis=(Pose.BladeTip-Pose.BladeBase).GetSafeNormal();
  return FVector::DotProduct(Pose.Bones[0].GetLocation()-Pose.BladeBase,Axis);
 }

 void ObserveBraceHold(const FBracePose& Pose,float Hold)
 {
  const auto* Body=Character->GetMesh();
  const auto* Capsule=Character->GetCapsuleComponent();
  const double GroundZ=Capsule->GetComponentLocation().Z-Capsule->GetScaledCapsuleHalfHeight();
  const double LeftHeight=Body->GetComponentTransform().TransformPosition(Pose.Bones[8].GetLocation()).Z-GroundZ;
  const double RightHeight=Body->GetComponentTransform().TransformPosition(Pose.Bones[9].GetLocation()).Z-GroundZ;
  Check(LeftHeight>=0.&&LeftHeight<=30.&&RightHeight>=0.&&RightHeight<=30.,TEXT("capstone_held_feet_stay_in_grounded_band"));

  const FVector2D FootTravel(BraceBoneTravel(BraceReady,Pose,8),BraceBoneTravel(BraceReady,Pose,9));
  const double AxialTravel=BraceLeftWristAxialPosition(Pose)-BraceLeftWristAxialPosition(BraceReady);
  const double BladeAngle=BraceBladeAngle(BraceReady,Pose);
  ++BraceHoldSamples;
  BraceWristMax=FMath::Max(BraceWristMax,FMath::Max(BraceBoneTravel(BraceReady,Pose,0),BraceBoneTravel(BraceReady,Pose,1)));
  if(Index==0)
  {
   BraceStockHold.Add({Hold,FootTravel,AxialTravel,BladeAngle});
   BraceFeetMax.X=FMath::Max(BraceFeetMax.X,FootTravel.X);
   BraceFeetMax.Y=FMath::Max(BraceFeetMax.Y,FootTravel.Y);
   return;
  }
  if(BraceStockHold.IsEmpty()){Check(false,TEXT("capstone_stock_hold_samples_missing"));return;}
  // Subtract each trial's ready pose to accommodate the authored idle phase.
  // One centimeter allows phase variation within the same planted stance.
  const auto* Nearest=&BraceStockHold[0];
  for(const auto& Sample:BraceStockHold)
  {
   if(FMath::Abs(Sample.HoldSeconds-Hold)<FMath::Abs(Nearest->HoldSeconds-Hold)){Nearest=&Sample;}
  }
  Check(FMath::IsNearlyEqual(Hold,Nearest->HoldSeconds,1.f/60.f+.001f),TEXT("capstone_hold_baseline_matches_time"));
  BraceFeetMax.X=FMath::Max(BraceFeetMax.X,FootTravel.X-Nearest->FootTravel.X);
  BraceFeetMax.Y=FMath::Max(BraceFeetMax.Y,FootTravel.Y-Nearest->FootTravel.Y);
  Check(FootTravel.X<=Nearest->FootTravel.X+1.&&FootTravel.Y<=Nearest->FootTravel.Y+1.,TEXT("capstone_added_held_foot_travel_exceeds_one_cm"));
  BraceAxialDriftMax=FMath::Max(BraceAxialDriftMax,FMath::Abs(AxialTravel-Nearest->AxialTravel));
  BraceAddedBladeMax=FMath::Max(BraceAddedBladeMax,BladeAngle-Nearest->BladeAngle);
 }

 void CheckBraceHoldMotion()
 {
  UE_LOG(LogRogue10m,Display,TEXT("CAPSTONE_BRACE HOLD case=%s samples=%d max_left_wrist_axial_drift_cm=%.4f max_added_blade_deg=%.4f max_wrist_travel_cm=%.4f"),
   BraceCaseNames[Index],BraceHoldSamples,BraceAxialDriftMax,BraceAddedBladeMax,BraceWristMax);
  if(Index==0){return;}
  Check(BraceAxialDriftMax<=3.5,TEXT("capstone_held_left_wrist_axial_drift_exceeds_3_5_cm"));
  Check(BraceAddedBladeMax<=8.,TEXT("capstone_added_held_blade_angle_exceeds_8_degrees"));
  Check(BraceWristMax<=6.,TEXT("capstone_held_wrist_travel_exceeds_6_cm"));
  if(Index!=2)
  {
   // Full holds must produce a measurable change on the displayed body.
   Check(BraceWristMax>=.75&&BraceAddedBladeMax>1.,TEXT("capstone_visible_hold_motion_required"));
  }
 }

 float BraceMontagePosition() const
 {
  const auto* Source=Character->GetAnimationPlaybackMesh();
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const auto* Montage=Anim&&BraceSkill.IsValid()?Anim->GetActiveInstanceForMontage(BraceSkill->AttackMontage):nullptr;
  return Montage?Montage->GetPosition():-1.f;
 }

 void BraceKey(const FKey& Key,bool bDown)
 {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,bDown?IE_Pressed:IE_Released,bDown?1.f:0.f));}

 bool ReadBracePose(FBracePose& Pose) const
 {
  const auto* Body=Character->GetMesh();
  const auto* Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
  const auto* Sword=Visual?Visual->GetRightWeaponMesh():nullptr;
  if(!Body||!Body->IsVisible()||!Sword||!Sword->IsVisible()||Sword->GetAttachParent()!=Body||Sword->GetAttachSocketName()!=TEXT("hand_r")){return false;}
  for(int32 Bone=0;Bone<UE_ARRAY_COUNT(BraceBones);++Bone)
  {
   if(!Body->DoesSocketExist(BraceBones[Bone])){return false;}
   Pose.Bones[Bone]=Body->GetSocketTransform(BraceBones[Bone],RTS_Component);
   if(Pose.Bones[Bone].ContainsNaN()){return false;}
  }
  const FTransform SwordTransform=Sword->GetComponentTransform();
  const FTransform BodyTransform=Body->GetComponentTransform();
  Pose.BladeBase=BodyTransform.InverseTransformPosition(SwordTransform.TransformPosition(FVector(0,0,18)));
  Pose.BladeTip=BodyTransform.InverseTransformPosition(SwordTransform.TransformPosition(FVector(0,0,140)));
  Pose.Revision=Body->GetBoneTransformRevisionNumber();
  return !Pose.BladeBase.ContainsNaN()&&!Pose.BladeTip.ContainsNaN()&&FVector::Distance(Pose.BladeBase,Pose.BladeTip)>1.;
 }

 void RecordBracePose(const TCHAR* Phase,const FBracePose& Pose,double Hold)
 {
  const auto* Source=Character->GetAnimationPlaybackMesh();
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const auto* Montage=Anim?Anim->GetActiveInstanceForMontage(BraceSkill->AttackMontage):nullptr;
  const auto* Camera=Character->GetGameplayCameraComponent();
  const double Pitch=Camera?Camera->GetComponentRotation().Pitch:0.;
  BraceRows+=FString::Printf(TEXT("%s,%s,%d,%.6f,%.6f,%.3f,%u,%d,%.6f,%.6f,%.6f,%.3f,%.6f,%.6f,"),
   BraceCaseLabel(),Phase,Frame,World->GetTimeSeconds(),Hold,Pitch,Pose.Revision,
   Montage?Montage->GetInstanceID():INDEX_NONE,Montage?Montage->GetPosition():-1.f,
   Montage?Montage->GetWeight():0.f,BraceDamagePosition,Target->GetRogueAttributeSet()->GetHealth(),
   BraceLeftWristAxialPosition(Pose),FVector::Distance(Pose.Bones[0].GetLocation(),Pose.Bones[1].GetLocation()));
  auto AppendVector=[this](const FVector& V){BraceRows+=FString::Printf(TEXT("%.6f,%.6f,%.6f,"),V.X,V.Y,V.Z);};
  AppendVector(Pose.BladeBase);AppendVector(Pose.BladeTip);
  for(const FTransform& Bone:Pose.Bones)
  {
   AppendVector(Bone.GetLocation());
   const FQuat Q=Bone.GetRotation();
   BraceRows+=FString::Printf(TEXT("%.8f,%.8f,%.8f,%.8f,"),Q.X,Q.Y,Q.Z,Q.W);
  }
  if(bCapstoneReleaseAudit)
  {
   const FTransform Body=Character->GetMesh()->GetComponentTransform();
   const auto* Capsule=Target->GetCapsuleComponent();
   const double Radius=Capsule->GetScaledCapsuleRadius();
   const FVector Axis=Capsule->GetUpVector()*FMath::Max(0.,Capsule->GetScaledCapsuleHalfHeight()-Radius);
   const FVector Center=Capsule->GetComponentLocation();
   FVector BladeClosest,CapsuleClosest;
   FMath::SegmentDistToSegmentSafe(Body.TransformPosition(Pose.BladeBase),Body.TransformPosition(Pose.BladeTip),
    Center-Axis,Center+Axis,BladeClosest,CapsuleClosest);
   const FVector LeftPalm=(Pose.Bones[0].GetLocation()+Pose.Bones[10].GetLocation())*.5;
   const FVector RightPalm=(Pose.Bones[1].GetLocation()+Pose.Bones[11].GetLocation())*.5;
   const FVector Actor=Character->GetActorLocation();
   const auto* OwnCapsule=Character->GetCapsuleComponent();
   const double Floor=OwnCapsule->GetComponentLocation().Z-OwnCapsule->GetScaledCapsuleHalfHeight();
   const double BladeFloor=FMath::Min(Body.TransformPosition(Pose.BladeBase).Z,Body.TransformPosition(Pose.BladeTip).Z)-Floor;
   BraceRows+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"),
    FVector::Distance(LeftPalm,RightPalm),FVector::Distance(BladeClosest,CapsuleClosest)-Radius,
    Actor.X,Actor.Y,Actor.Z,Character->GetActorRotation().Yaw,BladeFloor);
  }
  BraceRows.RemoveAt(BraceRows.Len()-1);
  BraceRows+=TEXT("\n");
 }

 void PrepareCapstoneBrace()
 {
  auto* Combat=Character->GetCombatComponent();
  auto* TargetAbility=Target->GetAbilitySystemComponent();
  BraceCVar=IConsoleManager::Get().FindConsoleVariable(TEXT("a.Rogue10m.GreatSwordCapstoneBrace"));
  BraceSkill=Skills[10];
  BraceSpecial=Skills[3];
  auto* Skill=BraceSkill.Get();
  const FString MontagePath=Skill&&Skill->AttackMontage?Skill->AttackMontage->GetPathName():FString();
  const bool bExpectedRelease=MontagePath==TEXT("/Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Charged.AM_GreatSword_Charged")
   ||MontagePath==TEXT("/Game/Rogue10m/Animation/Combat/FighterSkills/GreatSword/AM_GreatSword_CapstoneRelease.AM_GreatSword_CapstoneRelease");
  Check(Skill&&Skill->AttackMontage&&Skill->GetFName()==TEXT("DA_Hybrid_GreatSword_Ultimate")
   &&bExpectedRelease&&Skill->HitStartDelaySeconds==0.f&&FMath::IsNearlyEqual(Skill->ChargeSeconds,.72f)
   &&FMath::IsNearlyEqual(Skill->AttackRange,360.f)&&Skill->AttackShape==ERogue10mAttackShape::Arc,
   TEXT("capstone_stock_charge_and_release_contract"));
  Check(BraceCVar&&Combat&&TargetAbility&&BraceSpecial.IsValid()&&Character->GetGameplayCameraComponent(),TEXT("capstone_test_dependencies_missing"));
  if(Failures){Finish();return;}
  Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);
  Check(Combat->CanEquipSkillInSlot(Skill,ERogue10mAttackInputSlot::ChargedSpecial)
   &&Combat->AssignSkillToInputSlot(Skill,ERogue10mAttackInputSlot::ChargedSpecial)
   &&Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial)==Skill,TEXT("capstone_real_charged_special_assignment"));
  Check(Combat->AssignSkillToInputSlot(BraceSpecial.Get(),ERogue10mAttackInputSlot::Special)
   &&Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special)==BraceSpecial.Get(),TEXT("capstone_real_early_special_assignment"));
  if(Failures){Finish();return;}
  TargetAbility=Target->GetAbilitySystemComponent();
  BraceHealthDelegate=TargetAbility->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute()).AddLambda(
   [this](const FOnAttributeChangeData& Change)
   {
    if(Change.OldValue-Change.NewValue<=.01f||!BraceSkill.IsValid()
     ||Character->GetCombatComponent()->GetLastAcceptedAttackSkill()!=BraceSkill.Get()){return;}
    ++BraceDamageCount;
    if(BraceDamageCount==1)
    {
     BraceDamagePosition=BraceMontagePosition();
     UE_LOG(LogRogue10m,Display,TEXT("CAPSTONE_BRACE DAMAGE case=%s world=%.6f montage_s=%.6f drop=%.3f"),
      BraceCaseLabel(),World->GetTimeSeconds(),BraceDamagePosition,Change.OldValue-Change.NewValue);
    }
   });
  BraceOriginalCVar=BraceCVar->GetInt();
  bBraceCVarOwned=true;
  BraceRows=TEXT("case,phase,frame,world_s,hold_s,camera_pitch,body_revision,montage_instance,montage_s,montage_weight,damage_montage_s,health,left_wrist_blade_axis_cm,hand_span_cm,blade_base_x,blade_base_y,blade_base_z,blade_tip_x,blade_tip_y,blade_tip_z");
  for(const TCHAR* Bone:BraceBones)
  {
   for(const TCHAR* Field:{TEXT("x"),TEXT("y"),TEXT("z"),TEXT("qx"),TEXT("qy"),TEXT("qz"),TEXT("qw")})
   {BraceRows+=FString::Printf(TEXT(",%s_%s"),Bone,Field);}
  }
  if(bCapstoneReleaseAudit){BraceRows+=TEXT(",palm_span_cm,blade_capsule_gap_cm,actor_x,actor_y,actor_z,actor_yaw,blade_floor_cm");}
  BraceRows+=TEXT("\n");
  if(bCapstoneBraceRender||bCapstoneReleaseRender)
  {
   const FVector Origin=Character->GetActorLocation();
   const FVector Forward=Character->GetActorForwardVector();
   const FVector Right=Character->GetActorRightVector();
   const FVector Position=bCapstoneReleaseRender?Origin+Forward*120.-Right*420.+FVector(0,0,85):
    Origin+Forward*230.+Right*245.+FVector(0,0,75);
   const FVector Focus=Origin+(bCapstoneReleaseRender?Forward*52.+FVector(0,0,35):FVector(0,0,27));
   FActorSpawnParameters Params;
   Params.ObjectFlags|=RF_Transient;
   auto* Camera=World->SpawnActor<ACameraActor>(Position,(Focus-Position).Rotation(),Params);
   Check(Camera!=nullptr,TEXT("capstone_render_fixed_camera_spawned"));
   if(!Camera){Finish();return;}
   Camera->GetCameraComponent()->SetFieldOfView(bCapstoneReleaseRender?63.f:43.f);
   Controller->SetViewTarget(Camera);
   if(bCapstoneBraceRender){Target->SetActorHiddenInGame(true);}
  }
  NextBraceCase();
 }

 void SetBraceStep(EBraceStep Step)
 {BraceStep=Step;BraceStepStarted=World->GetTimeSeconds();}

 void NextBraceCase()
 {
  if(Index>=ExpectedBraceCases()){Finish();return;}
  BraceCVar->Set(Index==0&&!bCapstoneReleaseRender?0:1,ECVF_SetByCode);
  Controller->SetControlRotation(FRotator::ZeroRotator);
  Target->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*110.,
   false,nullptr,ETeleportType::TeleportPhysics);
  BraceDamageCount=0;
  BraceDamagePosition=-1.f;
  BraceFirstReleaseSampled=false;
  bBracePeakCaptured=false;
  BraceReleaseSamples=0;
  BraceRecoveryStartedAt=-1.;
  bBraceReleaseHoldShot=bBraceReleaseMotionShot=bBraceReleaseRecoveryShot=false;
  BraceHoldSamples=0;
  BraceFeetMax=FVector2D::ZeroVector;
  BraceAxialDriftMax=BraceAddedBladeMax=BraceWristMax=0.;
  SetBraceStep(EBraceStep::Settle);
 }

 void StepCapstoneBrace()
 {
  auto* Combat=Character->GetCombatComponent();
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  auto* Source=Character->GetAnimationPlaybackMesh();
  auto* Body=Character->GetMesh();
  auto* Anim=Source?Source->GetAnimInstance():nullptr;
  if(!Combat||!State||!Anim){Check(false,TEXT("capstone_lost_combat_or_playback_mesh"));Finish();return;}
  const double Age=World->GetTimeSeconds()-BraceStepStarted;
  const TOptional<float> Hold=Combat->GetGreatSwordCapstoneHoldSeconds();
  const int32 CapstoneUses=State->GetSkillUseCount(BraceSkill->GetFName());
  const int32 SpecialUses=State->GetSkillUseCount(BraceSpecial->GetFName());
  if(Age>9.){Check(false,TEXT("capstone_phase_timeout"));Finish();return;}
  if(BraceStep==EBraceStep::Settle)
  {
   if(Age<1.||Combat->GetAttackCooldownRemaining()>0.f||Combat->IsWeaponChangeBlocked()||Anim->IsAnyMontagePlaying()){return;}
   Character->GetRogueAttributeSet()->RestoreVitals();
   const bool bReady=!Hold.IsSet()&&Character->IsThirdPersonGameplayActive()&&Character->GetCharacterMovement()->IsMovingOnGround()
    &&ReadBracePose(BraceReady);
   Check(bReady,TEXT("capstone_ready_third_person_stationary_pose"));
   if(!bReady){Finish();return;}
   BraceStartUses=CapstoneUses;
   BraceStartSpecialUses=SpecialUses;
   BraceReadyHealth=Target->GetRogueAttributeSet()->GetHealth();
   RecordBracePose(TEXT("ready"),BraceReady,-1.);
   SetBraceStep(EBraceStep::Hold);
   BraceKey(EKeys::RightMouseButton,true);
   return;
  }
  FBracePose Pose;
  if(!ReadBracePose(Pose)){Check(false,TEXT("capstone_retargeted_display_pose_missing"));Finish();return;}
  if(BraceStep==EBraceStep::Hold)
  {
   if(!Hold.IsSet()&&Age<.20){return;}
   Check(Hold.IsSet()&&CapstoneUses==BraceStartUses&&SpecialUses==BraceStartSpecialUses&&BraceMontagePosition()<0.f
    &&FMath::IsNearlyEqual(Target->GetRogueAttributeSet()->GetHealth(),BraceReadyHealth,.01f),
    TEXT("capstone_real_RMB_hold_starts_without_accepted_attack_or_damage"));
   if(!Hold.IsSet()){Finish();return;}
   ObserveBraceHold(Pose,Hold.GetValue());
   RecordBracePose(TEXT("hold"),Pose,Hold.GetValue());
   if(bCapstoneBraceRender)
   {
    if(Hold.GetValue()<BraceRenderHoldSeconds-.001f){return;}
    Check(FMath::IsNearlyEqual(Hold.GetValue(),BraceRenderHoldSeconds,.001f),TEXT("capstone_render_samples_matching_hold_frame"));
    CheckBraceHoldMotion();
    RecordBracePose(TEXT("hold_peak"),Pose,Hold.GetValue());
    SetBraceStep(EBraceStep::RenderCapture);
    if(!Capture(TEXT("hold_peak"))){Finish();}
    return;
   }
   const float Threshold=Index==2?.20f:.90f;
   if(Hold.GetValue()<Threshold){return;}
   if(bCapstoneReleaseRender&&!bBraceReleaseHoldShot)
   {
    bBraceReleaseHoldShot=Capture(TEXT("held"));
    if(!bBraceReleaseHoldShot){Finish();}
    return;
   }
   if(bCapturePending){return;}
   CheckBraceHoldMotion();
   UE_LOG(LogRogue10m,Display,TEXT("CAPSTONE_BRACE FEET case=%s mode=%s hold_s=%.6f left_max_cm=%.4f right_max_cm=%.4f"),
    BraceCaseNames[Index],Index==0?TEXT("stock_travel"):TEXT("above_stock"),Hold.GetValue(),BraceFeetMax.X,BraceFeetMax.Y);
   BraceBeforeRelease=Pose;
   if(Index==3)
   {
    BraceSourceRate=Source->GlobalAnimRateScale;BraceBodyRate=Body->GlobalAnimRateScale;
    Source->GlobalAnimRateScale=0.f;Body->GlobalAnimRateScale=0.f;
    bBracePoseFrozen=true;
    SetBraceStep(EBraceStep::PitchHigh);
   }
   else
   {
    SetBraceStep(EBraceStep::Release);
    BraceKey(EKeys::RightMouseButton,false);
   }
   return;
  }
  if(BraceStep==EBraceStep::RenderCapture)
  {
   if(bCapturePending){return;}
   Check(bBracePeakCaptured,TEXT("capstone_render_capture_completed"));
   // End the hold before releasing its input key so the stock capture's short
   // press cannot start the Special montage during the matched brace setup.
   Combat->CancelHeldAttackInput();
   BraceKey(EKeys::RightMouseButton,false);
   ++Index;NextBraceCase();return;
  }
  if(BraceStep==EBraceStep::Release)
  {
   if(Index==2)
   {
    if(Age<.18){return;}
    Check(!Hold.IsSet()&&CapstoneUses==BraceStartUses&&BraceDamageCount==0&&BraceMontagePosition()<0.f
     &&SpecialUses==BraceStartSpecialUses+1&&Combat->GetLastAcceptedAttackSkill()==BraceSpecial.Get(),
     TEXT("capstone_short_RMB_release_uses_only_the_existing_special"));
    RecordBracePose(TEXT("early_special"),Pose,-1.);
    ++Index;NextBraceCase();return;
   }
   const float Montage=BraceMontagePosition();
   if(!BraceFirstReleaseSampled)
   {
    if((CapstoneUses==BraceStartUses||Pose.Revision==BraceBeforeRelease.Revision||Montage<=0.f)&&Age<.08){return;}
    Check(!Hold.IsSet()&&CapstoneUses==BraceStartUses+1&&SpecialUses==BraceStartSpecialUses
     &&Combat->GetLastAcceptedAttackSkill()==BraceSkill.Get()&&BraceDamageCount==1
     &&FMath::IsNearlyZero(BraceDamagePosition,.0001f),TEXT("capstone_RMB_release_first_damage_at_stock_montage_zero"));
    const bool bFreshPose=Pose.Revision!=BraceBeforeRelease.Revision&&Montage>0.f;
    Check(bFreshPose&&Montage<=.025f,
     TEXT("capstone_first_advanced_display_pose_available"));
    if(!bFreshPose){Finish();return;}
    BraceFirstReleaseSampled=true;
    const double Left=BraceBoneTravel(BraceBeforeRelease,Pose,0),Right=BraceBoneTravel(BraceBeforeRelease,Pose,1),
     Blade=BraceBladeAngle(BraceBeforeRelease,Pose);
    UE_LOG(LogRogue10m,Display,TEXT("CAPSTONE_BRACE RELEASE case=%s montage_s=%.6f wrist_l_cm=%.4f wrist_r_cm=%.4f blade_deg=%.4f"),
     BraceCaseLabel(),Montage,Left,Right,Blade);
    if(Index==0){BraceStockLeft=Left;BraceStockRight=Right;BraceStockBlade=Blade;BraceStockMontage=Montage;}
    else
    {
     Check(FMath::IsNearlyEqual(Montage,BraceStockMontage,.001f),TEXT("capstone_release_comparison_at_same_montage_time"));
     Check(Left<=BraceStockLeft+.5&&Right<=BraceStockRight+.5&&Blade<=BraceStockBlade+1.,
      TEXT("capstone_brace_first_release_wrist_or_blade_discontinuity"));
    }
   }
   if(bCapstoneReleaseAudit&&Montage<0.f&&BraceFirstReleaseSampled)
   {
    const double Now=World->GetTimeSeconds();
    if(BraceRecoveryStartedAt<0.){BraceRecoveryStartedAt=Now;}
    if(Now-BraceRecoveryStartedAt<.20||Combat->IsWeaponChangeBlocked()){return;}
    if(bCapturePending){return;}
    if(bCapstoneReleaseRender&&!bBraceReleaseRecoveryShot)
    {
     RecordBracePose(TEXT("recovery"),Pose,-1.);
     bBraceReleaseRecoveryShot=Capture(TEXT("recovery"));
     if(!bBraceReleaseRecoveryShot){Finish();}
     return;
    }
    if(!bCapstoneReleaseRender){RecordBracePose(TEXT("recovery"),Pose,-1.);}
    Check(BraceDamageCount==1&&BraceReleaseSamples>=90,TEXT("capstone_full_release_recovery_and_one_damage"));
    ++Index;NextBraceCase();return;
   }
   RecordBracePose(TEXT("release"),Pose,-1.);
   ++BraceReleaseSamples;
   if(bCapstoneReleaseRender&&Montage>=1.f&&!bBraceReleaseMotionShot&&!bCapturePending)
   {
    bBraceReleaseMotionShot=Capture(TEXT("swing"));
    if(!bBraceReleaseMotionShot){Finish();}
   }
   if(!bCapstoneReleaseAudit&&Montage>=.20f)
   {
    Check(BraceDamageCount==1&&BraceReleaseSamples>=12,TEXT("capstone_single_hit_and_release_trajectory_recorded"));
    ++Index;NextBraceCase();
   }
   return;
  }
  if(BraceStep==EBraceStep::PitchHigh||BraceStep==EBraceStep::PitchLow)
  {
   if(Age<.20){return;}
   const double ExpectedPitch=BraceStep==EBraceStep::PitchHigh?65.:-65.;
   if(BraceStep==EBraceStep::PitchHigh&&!BracePitchStarting)
   {
    BraceBeforeRelease=Pose;
    RecordBracePose(TEXT("pitch_zero"),Pose,Hold.Get(-1.f));
    Controller->SetControlRotation(FRotator(ExpectedPitch,0,0));
    BracePitchStarting=true;
    SetBraceStep(EBraceStep::PitchHigh);
    return;
   }
   Check(Hold.IsSet()&&Hold.GetValue()>=.90f&&Character->IsThirdPersonGameplayActive()
    &&FMath::IsNearlyEqual(Character->GetGameplayCameraComponent()->GetComponentRotation().Pitch,ExpectedPitch,2.)
    &&Pose.Revision!=BraceBeforeRelease.Revision,TEXT("capstone_fixed_phase_uses_real_view_pitch_and_fresh_pose"));
   for(int32 Bone=0;Bone<8;++Bone)
   {
    Check(BraceBoneTravel(BraceBeforeRelease,Pose,Bone)<=.5
     &&FMath::RadiansToDegrees(BraceBeforeRelease.Bones[Bone].GetRotation().AngularDistance(Pose.Bones[Bone].GetRotation()))<=1.,
     TEXT("capstone_hold_arms_follow_vertical_camera"));
   }
   Check(BraceBladeAngle(BraceBeforeRelease,Pose)<=1.,TEXT("capstone_blade_follows_vertical_camera"));
   RecordBracePose(BraceStep==EBraceStep::PitchHigh?TEXT("pitch_high"):TEXT("pitch_low"),Pose,Hold.Get(-1.f));
   if(BraceStep==EBraceStep::PitchHigh)
   {
    Controller->SetControlRotation(FRotator(-65,0,0));
    SetBraceStep(EBraceStep::PitchLow);
   }
   else
   {
    Source->GlobalAnimRateScale=BraceSourceRate;Body->GlobalAnimRateScale=BraceBodyRate;
    bBracePoseFrozen=false;
    Controller->SetControlRotation(FRotator::ZeroRotator);
    SetBraceStep(EBraceStep::Cancel);
    // A late second mouse button suppresses the held charge through the production mouse route.
    BraceKey(EKeys::LeftMouseButton,true);
   }
   return;
  }
  if(BraceStep==EBraceStep::Cancel)
  {
   if(Hold.IsSet()&&Age<.20){return;}
   Check(!Hold.IsSet()&&CapstoneUses==BraceStartUses&&SpecialUses==BraceStartSpecialUses&&BraceDamageCount==0,
    TEXT("capstone_late_second_mouse_button_cancels_the_charge"));
   BraceKey(EKeys::LeftMouseButton,false);BraceKey(EKeys::RightMouseButton,false);
   SetBraceStep(EBraceStep::Recenter);
   return;
  }
  if(Age>.30)
  {
   Check(!Hold.IsSet()&&CapstoneUses==BraceStartUses&&SpecialUses==BraceStartSpecialUses&&BraceDamageCount==0
    &&BraceMontagePosition()<0.f,TEXT("capstone_releasing_cancelled_mouse_buttons_has_no_attack"));
   RecordBracePose(TEXT("cancelled"),Pose,-1.);
   ++Index;NextBraceCase();
  }
 }

 int32 ExpectedBraceCases() const
 {return bCapstoneReleaseRender?1:bCapstoneBraceRender||bCapstoneReleaseAudit?BraceRenderCaseCount:BraceCaseCount;}

 void FinishCapstoneBrace()
 {
  if(Target.IsValid())
  {
   if(auto* Ability=Target->GetAbilitySystemComponent())
   {Ability->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute()).Remove(BraceHealthDelegate);}
  }
  if(bBracePoseFrozen&&Character.IsValid())
  {
   if(auto* Source=Character->GetAnimationPlaybackMesh()){Source->GlobalAnimRateScale=BraceSourceRate;}
   Character->GetMesh()->GlobalAnimRateScale=BraceBodyRate;
  }
  if(bBraceCVarOwned){BraceCVar->Set(BraceOriginalCVar,ECVF_SetByCode);}
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  Check(FFileHelper::SaveStringToFile(BraceRows,*(Output/TEXT("capstone-brace.csv"))),TEXT("capstone_evidence_save_failed"));
  if(bCapstoneBraceRender)
  {
   Check(CapturePaths.Num()==BraceRenderCaseCount&&CompletedCaptures==BraceRenderCaseCount,TEXT("capstone_render_pair_required"));
   Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("capstone_capture_evidence_save_failed"));
  }
  if(bCapstoneReleaseRender)
  {
   constexpr int32 ReleaseCaptureCount=3;
   Check(CapturePaths.Num()==ReleaseCaptureCount&&CompletedCaptures==ReleaseCaptureCount,TEXT("capstone_full_release_render_phases_required"));
   Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("capstone_release_capture_evidence_saved"));
  }
  const int32 ExpectedCases=ExpectedBraceCases();
  const bool bPassed=Failures==0&&Index==ExpectedCases;
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=%s_%s completed=%d failures=%d"),
   bCapstoneReleaseRender?TEXT("CAPSTONE_RELEASE_RENDER"):bCapstoneBraceRender?TEXT("CAPSTONE_BRACE_RENDER"):
    bCapstoneReleaseAudit?TEXT("CAPSTONE_RELEASE"):TEXT("CAPSTONE_BRACE"),
   bPassed?TEXT("PASSED"):TEXT("FAILED"),Index,Failures);
  FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);
 }

 void PrepareSwordEquipAudit()
 {
  SwordEquipRows=TEXT("frame,world_s,phase,stage,revision,lod,mesh,anim_class,weapon_visible,bone,x,y,z,qx,qy,qz,qw\n");
  auto* Inventory=Character->GetInventoryComponent();
  const auto* Item=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
  Check(Inventory&&Item&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed
   &&Inventory->TryAddGridItem(Item,1,SwordEquipGrid,SwordEquipItem),TEXT("equip_audit_existing_sword_added_to_unarmed_inventory"));
  if(Failures){Finish();return;}
  OldFixed=FApp::UseFixedTimeStep();
  OldDelta=FApp::GetFixedDeltaTime();
  FApp::SetUseFixedTimeStep(true);
  FApp::SetFixedDeltaTime(1./60.);
  bOwnFixed=true;
  if(!bMeleeVfxLogicOnly)
  {
   const FVector Origin=Character->GetActorLocation();
   const FVector Position=Origin+Character->GetActorForwardVector()*300.+Character->GetActorRightVector()*280.+FVector(0,0,55);
   FActorSpawnParameters Params;
   Params.ObjectFlags|=RF_Transient;
   auto* Camera=World->SpawnActor<ACameraActor>(Position,(Origin-Position).Rotation(),Params);
   Check(Camera!=nullptr,TEXT("equip_audit_render_fixed_camera_spawned"));
   if(!Camera){Finish();return;}
   Camera->GetCameraComponent()->SetFieldOfView(bSwordEquipMovingBoth?52.f:46.f);
   if(bSwordEquipMovingBoth)
   {
    SwordEquipCamera=Camera;
    SwordEquipCameraOffset=Position-Origin;
   }
   Controller->SetViewTarget(Camera);
   ScreenshotProcessed=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->CaptureProcessed();});
  }
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
  {
   if(Self->World.Get()==W){Self->StepSwordEquipAudit();}
  });
 }
 void StepSwordEquipAudit()
 {
  ++Frame;
  if(auto* Camera=SwordEquipCamera.Get())
  {
   Camera->SetActorLocation(Character->GetActorLocation()+SwordEquipCameraOffset);
  }
  if(Frame<23){return;}
  const auto* Source=Character->GetAnimationPlaybackMesh();
  const auto* Display=Character->GetMesh();
  Check(Source&&Display&&Character->GetCharacterMovement()->IsMovingOnGround(),TEXT("equip_audit_fresh_grounded_meshes"));
  if(Failures){Finish();return;}
  const auto* Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
  const auto* Sword=Visual?Visual->GetRightWeaponMesh():nullptr;
  const bool bSwordVisible=Sword&&Sword->IsVisible()&&!Sword->bHiddenInGame;
  const TCHAR* Phase=Frame<=30?TEXT("before_equip"):Frame<=68?TEXT("equipped"):TEXT("unequipped");
  if(Frame>=23)
  {
   for(const auto* Mesh:{Source,Display})
   {
    const FString MeshPath=GetPathNameSafe(Mesh->GetSkeletalMeshAsset());
    const auto* Anim=Mesh->GetAnimInstance();
    const FString ClassPath=Anim?Anim->GetClass()->GetPathName():FString();
    const TCHAR* Stage=Mesh==Source?TEXT("source"):TEXT("display");
    for(const FName Bone:{FName(TEXT("root")),FName(TEXT("pelvis")),FName(TEXT("clavicle_l")),FName(TEXT("clavicle_r")),
     FName(TEXT("upperarm_l")),FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_l")),FName(TEXT("lowerarm_r")),
     FName(TEXT("hand_l")),FName(TEXT("hand_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
    {
     const int32 BoneIndex=Mesh->GetBoneIndex(Bone);
     if(BoneIndex==INDEX_NONE){continue;}
     const FTransform T=Mesh->GetBoneTransform(BoneIndex,FTransform::Identity);
     const FVector V=T.GetLocation();const FQuat Q=T.GetRotation();
     SwordEquipRows+=FString::Printf(TEXT("%d,%.6f,%s,%s,%u,%d,%s,%s,%d,%s,%.6f,%.6f,%.6f,%.8f,%.8f,%.8f,%.8f\n"),
      Frame,World->GetTimeSeconds(),Phase,Stage,Mesh->GetBoneTransformRevisionNumber(),Mesh->GetPredictedLODLevel(),
      *MeshPath,*ClassPath,bSwordVisible?1:0,*Bone.ToString(),V.X,V.Y,V.Z,Q.X,Q.Y,Q.Z,Q.W);
    }
   }
  }
  auto* Inventory=Character->GetInventoryComponent();
  if(bSwordEquipWalking&&(FMath::Abs(Frame-30)<=8||FMath::Abs(Frame-68)<=8))
  {
   const auto* Anim=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
   const auto* Interface=Anim?IAnimClassInterface::GetFromClass(Anim->GetClass()):nullptr;
   const FAnimNode_BlendSpacePlayerBase* Ground=nullptr;
   if(Interface)for(const auto* Property:Interface->GetAnimNodeProperties())
   {
    if(!Property||!Property->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct())){continue;}
    const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(Anim);
    if(Node->GetBlendSpace()==Anim->GroundLocomotionBlendSpace&&(!Ground||Node->GetCachedBlendWeight()>Ground->GetCachedBlendWeight())){Ground=Node;}
   }
   if(Ground)
   {
    const FVector Input=Ground->GetPosition();
    const FVector Filtered=Ground->GetFilteredPosition();
    UE_LOG(LogRogue10m,Display,TEXT("SWORD_EQUIP_SOURCE frame=%d speed=%.5f guard=%.6f guard_asset=%s locomotion=%s gait=%.6f sample_s=%.6f weight=%.6f x=%.5f y=%.5f filtered_x=%.5f filtered_y=%.5f"),
     Frame,Character->GetVelocity().Size2D(),Anim->FighterCombatStanceAlpha,*GetPathNameSafe(Anim->FighterCombatStanceAnimation),
     *GetPathNameSafe(Anim->GroundLocomotionBlendSpace),Ground->GetAccumulatedTime(),Ground->GetCurrentAssetTime(),
     Ground->GetCachedBlendWeight(),Input.X,Input.Y,Filtered.X,Filtered.Y);
   }
  }
  if(bSwordEquipMovingBoth&&Frame==26){BraceKey(EKeys::W,true);}
  if(bSwordEquipMovingBoth&&Frame==47){BraceKey(EKeys::W,false);}
  if(bSwordEquipWalking&&Frame==64){BraceKey(EKeys::W,true);}
  if(Frame==30||Frame==68)
  {
   const bool bEquip=Frame==30;
   static const FName Bones[]={TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")};
   FTransform Before[UE_ARRAY_COUNT(Bones)];
   for(int32 IndexBone=0;IndexBone<UE_ARRAY_COUNT(Bones);++IndexBone)
   {
    Before[IndexBone]=Display->GetSocketTransform(Bones[IndexBone],RTS_Component);
   }
   const auto Revision=Display->GetBoneTransformRevisionNumber();
   if(bEquip)
   {
    if(bSwordEquipMovingBoth)
    {
     Check(Character->GetVelocity().Size2D()>=65.0,TEXT("equip_audit_equip_during_actual_walk_input"));
    }
    Check(Inventory->TryEquipGridItem(SwordEquipGrid,SwordEquipItem)
     &&Character->GetEquippedWeaponType()==ERogue10mWeaponType::GreatSword,TEXT("equip_audit_inventory_equipped_sword"));
   }
   else
   {
    if(bSwordEquipWalking)
    {
     const float Speed=Character->GetVelocity().Size2D();
     UE_LOG(LogRogue10m,Display,TEXT("SWORD_EQUIP_WALK speed_cm_s=%.6f"),Speed);
     Check(Speed>=65.,TEXT("equip_audit_unequip_during_actual_walk_input"));
    }
    Check(bSwordVisible&&Inventory->TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType::MainWeapon)
     &&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed,TEXT("equip_audit_inventory_unequipped_sword"));
   }
   double LargestMove=0.0,LargestAngle=0.0;
   for(int32 IndexBone=0;IndexBone<UE_ARRAY_COUNT(Bones);++IndexBone)
   {
    const FTransform After=Display->GetSocketTransform(Bones[IndexBone],RTS_Component);
    LargestMove=FMath::Max(LargestMove,FVector::Distance(Before[IndexBone].GetLocation(),After.GetLocation()));
    LargestAngle=FMath::Max(LargestAngle,FMath::RadiansToDegrees(Before[IndexBone].GetRotation().AngularDistance(After.GetRotation())));
   }
   UE_LOG(LogRogue10m,Display,TEXT("SWORD_EQUIP_SYNC appearance=%s equip=%d before_revision=%u after_revision=%u maximum_cm=%.6f maximum_deg=%.6f"),
    *SwordEquipAppearance,bEquip?1:0,Revision,Display->GetBoneTransformRevisionNumber(),LargestMove,LargestAngle);
   Check(LargestMove<=.8&&LargestAngle<=1.,TEXT("equip_audit_pose_preserved_during_inventory_call"));
  }
  if(!bMeleeVfxLogicOnly)
  {
   if(Frame==29){Capture(TEXT("before_equip"));}
   if(Frame==31){Capture(TEXT("first_equip"));}
   if(Frame==67){Capture(TEXT("sword_guard"));}
   if(Frame==69){Capture(TEXT("first_unequip"));}
   if(bSwordEquipMovingBoth&&Frame==82){Capture(TEXT("walk_recovered"));}
  }
  if(Frame==88)
  {
   if(bSwordEquipWalking){BraceKey(EKeys::W,false);}
   Check(!bSwordVisible,TEXT("equip_audit_sword_hidden_after_unequip"));
   Check(bMeleeVfxLogicOnly||CompletedCaptures==(bSwordEquipMovingBoth?5:4),TEXT("equip_audit_expected_render_captures"));
   Finish();
  }
 }
 void Next()
 {
  if(Index>=Skills.Num()||(IsFocusedKnuckleCase()&&Index>FocusedKnuckleCaseIndex())){Finish();return;}
  auto* C=Character->GetCombatComponent();C->CancelBoundSkillInput();C->CancelCombatVisuals();
  if(!bSwordTechniqueAudit&&!IsFocusedKnuckleCase()){Character->RestoreEquippedWeaponType(IsSwordCase()?ERogue10mWeaponType::GreatSword:ERogue10mWeaponType::Knuckle);}
  const UClass* BodyClass=Character->GetMesh()->GetAnimClass();
  const bool SwordRetarget=BodyClass&&BodyClass->GetPathName().Contains(TEXT("/GreatSword20261110/ABP_Human_GreatSword"));
  const auto Appearance=Character->GetCharacterAppearance();
  const bool bOtherEarthBody=bEarthSplitterAudit
   &&(Appearance.Race!=ERogue10mCharacterRace::Human||Appearance.Gender!=ERogue10mCharacterGender::Male);
  Check(BodyClass&&(SwordRetarget==(IsSwordCase()&&!bOtherEarthBody)),TEXT("weapon_specific_body_retarget_selection_failed"));
  // Range starts at the production body combat origin, never the camera behind the player.
  const FTransform CombatView=Character->GetCombatViewTransform();
  FVector Forward=CombatView.GetRotation().GetForwardVector();Forward.Z=0;Forward.Normalize();
  auto* Skill=Skills[Index].Get();
  const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
  FVector TargetOffset=Forward*110.;
  if(bCrushingEdge&&!IsMeleeMissCase()&&Skill->GetFName()==TEXT("DA_Hybrid_GreatSword_CrushingSlash"))
  {
   TargetOffset=Forward*CrushingEdgeDistance;
  }
  if(bSwordGuardBreakAudit&&!bSwordTechniqueRender&&!IsMeleeMissCase())
  {
   TargetOffset=Forward*GuardBreakDistances[Index];
  }
  if(bSwordTechniqueAudit&&!IsMeleeMissCase()&&Skill->AttackShape==ERogue10mAttackShape::Circle)
  {
   TargetOffset=Forward*(Skill->CircleForwardOffset-0.40*Skill->AttackRange)+Right*55.;
  }
  if(IsMeleeMissCase())
  {
   const double Radius=Target->GetCapsuleComponent()->GetScaledCapsuleRadius();
   switch(Skill->AttackShape)
   {
    case ERogue10mAttackShape::LinearBox: TargetOffset+=Right*(Skill->BoxHalfWidth+Radius+30.);break;
    case ERogue10mAttackShape::Arc: TargetOffset=Right*150.-Forward*40.;break;
    default: TargetOffset=Forward*(Skill->AttackRange+Skill->CircleForwardOffset+Radius+30.);break;
   }
  }
  const FVector CloseTarget=Character->GetActorLocation()+TargetOffset;
  Target->SetActorLocation(CloseTarget,false,nullptr,ETeleportType::TeleportPhysics);
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P TARGET skill=%s combat_origin=%s aim=%s grounded_target=%s"),*Skills[Index]->GetName(),*CombatView.GetLocation().ToCompactString(),*Forward.ToCompactString(),*CloseTarget.ToCompactString());
  Check(C->IsAttackSkillUnlocked(Skill),TEXT("fixture_catalog_unlock_failed"));
  auto* Basic=C->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
  Check(bSwordTechniqueAudit?(Basic&&Basic->GetFName()==TEXT("DA_Attack_GreatSword_Primary01")):
   Basic==Skills[IsSwordCase()?0:5].Get(),TEXT("production_primary_binding_changed"));
  CaseSlot=ERogue10mAttackInputSlot::Primary;Chain.Reset();ChainStep=0;ChainAcceptedAt=-1.;ChainNextAt=-1.;
  if(bSwordTechniqueAudit||IsFocusedKnuckleCase())
  {
   CaseSlot=Skill->RequiresLoadoutCharge()?ERogue10mAttackInputSlot::ChargedSpecial:ERogue10mAttackInputSlot::QSkill;
   if(bShoulderBashAudit)
   {
    Check(Skill->GetFName()==TEXT("DA_Attack_Knuckle_Charged")&&CaseSlot==ERogue10mAttackInputSlot::ChargedSpecial,
     TEXT("shoulder_bash_actual_knuckle_charge_lane"));
   }
   Check(C->CanEquipSkillInSlot(Skill,CaseSlot)&&C->AssignSkillToInputSlot(Skill,CaseSlot)
    &&C->GetEquippedSkill(CaseSlot)==Skill,TEXT("confirmed_technique_assigned_to_actual_q_or_rmb_lane"));
   UE_LOG(LogRogue10m,Display,TEXT("%s case=%d skill=%s miss=%d shape=%s range=%.3f circle_offset=%.3f charge_s=%.6f target_offset=%s"),
    bShoulderBashAudit?TEXT("SHOULDER_BASH_BINDING"):bBreakingKickAudit?TEXT("BREAKING_KICK_BINDING"):TEXT("GREAT_SWORD_TECHNIQUE"),
    Index,*Skill->GetName(),IsMeleeMissCase(),*UEnum::GetValueAsString(Skill->AttackShape),Skill->AttackRange,
    Skill->CircleForwardOffset,Skill->ChargeSeconds,*TargetOffset.ToCompactString());
  }
  else if(Skill!=Basic)
  {
   bool bAssigned=false;
   const ERogue10mAttackInputSlot Preferred[]={ERogue10mAttackInputSlot::Primary,ERogue10mAttackInputSlot::QSkill,ERogue10mAttackInputSlot::ESkill,ERogue10mAttackInputSlot::Special,ERogue10mAttackInputSlot::ChargedSpecial};
   for(auto Slot:{Preferred[Index%5],ERogue10mAttackInputSlot::QSkill,ERogue10mAttackInputSlot::ESkill,ERogue10mAttackInputSlot::Special,ERogue10mAttackInputSlot::MouseChord,ERogue10mAttackInputSlot::ChargedSpecial})
   {
    if(C->CanEquipSkillInSlot(Skill,Slot))
    {CaseSlot=Slot;bAssigned=C->AssignSkillToInputSlot(Skill,Slot);Check(bAssigned&&C->GetEquippedSkill(Slot)==Skill,TEXT("fixture_legal_slot_assignment_failed"));break;}
   }
   if(!bAssigned)
   {
    // Combo-only motions must be reached from the actual Primary, never rebound.
    auto* Cursor=Basic;TSet<const URogue10mAttackSkillData*> Seen;
    while(Cursor&&Cursor!=Skill&&!Seen.Contains(Cursor)&&Chain.Num()<8)
    {
     Seen.Add(Cursor);Chain.Add(Cursor);
     if(!Cursor->bEnableCombo||Cursor->ComboInputSlot!=ERogue10mAttackInputSlot::Primary){Cursor=nullptr;break;}
     Cursor=Cursor->NextComboSkill;
    }
    Check(Cursor==Skill&&!Chain.IsEmpty(),TEXT("fixture_no_legal_binding_or_primary_combo_path"));
    if(Cursor!=Skill||Chain.IsEmpty()){Finish();return;}
    Chain.Add(Skill);
   }
  }
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P ROUTE skill=%s slot=%s path=%s precursors=%d policy=%s"),*Skill->GetName(),*UEnum::GetValueAsString(CaseSlot),Chain.IsEmpty()?TEXT("production_binding"):TEXT("production_primary_combo"),FMath::Max(0,Chain.Num()-1),*UEnum::GetValueAsString(Skill->AllowedSlotPolicy));
  if(Failures){Finish();return;}
  if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
  CaseStartedAt=World->GetTimeSeconds();Accepted=-1;AttackSent=false;ChargeReleased=false;PulseCount=0;MontageSamples=0;
  GuardBreakNearestBladeGap=TNumericLimits<double>::Max();
  bEarthBeganStrafe=bEarthEndedStrafe=bEarthHeldCaptureRequested=false;
  EarthInitialActor=Character->GetActorLocation();
  if(EarthWall.IsValid()){EarthWall->Destroy();EarthWall.Reset();}
  if(bEarthSplitterWall&&IsEarthSplitterCase()&&!IsMeleeMissCase())
  {
   EarthInitialLook=Controller->GetControlRotation();
   const FTransform Basis=Character->GetActorTransform();
   FActorSpawnParameters Params;
   Params.ObjectFlags|=RF_Transient;
   auto* Wall=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
   if(Wall)
   {
    auto* Shape=NewObject<UBoxComponent>(Wall,NAME_None,RF_Transient);
    Wall->AddInstanceComponent(Shape);
    Wall->SetRootComponent(Shape);
    Shape->SetBoxExtent(FVector(28.,14.,63.));
    Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Shape->SetCollisionObjectType(ECC_WorldStatic);
    Shape->SetCollisionResponseToAllChannels(ECR_Block);
    Shape->SetGenerateOverlapEvents(false);
    Shape->SetCanEverAffectNavigation(false);
    Shape->RegisterComponent();
    Wall->SetActorLocation(Basis.TransformPosition(FVector(135.,129.,-28.)));
    EarthWall=Wall;
   }
   Check(EarthWall.IsValid(),TEXT("earthsplitter_wall_fixture_required"));
  }
  bReadyRequested=bReadyCaptured=bFirstDamageCaptured=bFirstDamageRequested=bAcceptanceRequested=bImpactResidualRequested=bRecoveryRequested=bRecoveryCaptured=false;RenderedHandMaxTravelCm=0;
  VfxExcluded.Reset();VfxObservations.Reset();FirstImpactComponent.Reset();bFirstImpactObserved=false;bVfxTimedOut=false;
  bSawMeleeTrail=bSawMeleeRange=bSawMeleeImpact=bMotionRequested=bMotionCaptured=false;
  bCrushingLoadRequested=bCrushingLoadCaptured=false;
  CasePresentationTier=FirstTrailMeshFrame=INDEX_NONE;
  TrailTrianglesThisFrame=0;
  if(IsVfxCase()){Check(Skill->CastEffect&&Skill->ImpactEffect&&Skill->CastEffect!=Skill->ImpactEffect,TEXT("vfx_distinct_cast_and_impact_assets_required"));}
  Uses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Skills[Index]->GetFName());PreviousHealth=Target->GetRogueAttributeSet()->GetHealth();
 }
 bool ObserveMeleeVfx(const URogue10mAttackSkillData& Skill, double Now, double Damage)
 {
  auto* Vfx=Character->FindComponentByClass<URogue10mMeleeVfxComponent>();
  if(!Vfx){return false;}
  TInlineComponentArray<UProceduralMeshComponent*> Meshes(Character.Get());
  UProceduralMeshComponent* CueMesh=nullptr;
  for(auto* Mesh:Meshes)
  {
   if(Mesh->ComponentHasTag(TEXT("Rogue10m.MeleeVfx"))){CueMesh=Mesh;break;}
  }
  const auto TriangleCount=[CueMesh](int32 SectionIndex)
  {
   const auto* Section=CueMesh&&CueMesh->IsRegistered()&&CueMesh->IsVisible()?CueMesh->GetProcMeshSection(SectionIndex):nullptr;
   return Section&&Section->bSectionVisible?Section->ProcIndexBuffer.Num()/3:0;
  };
  const int32 TrailTriangles=TriangleCount(0),RangeTriangles=TriangleCount(1),ImpactTriangles=TriangleCount(2);
  const int32 RangeCount=Vfx->GetActiveRangeCount(),ImpactCount=Vfx->GetActiveImpactCount();
  const auto* Outline=Vfx->GetLastRangeOutline();
  const auto Impact=Vfx->GetLastImpactPoint();
  const int32 Tier=Vfx->GetLastPresentationTier();
  double Diameter=-1.,SurfaceGap=-1.;
  TrailTrianglesThisFrame=TrailTriangles;
  if(TrailTriangles>0&&FirstTrailMeshFrame==INDEX_NONE){FirstTrailMeshFrame=Frame;}
  bSawMeleeTrail|=TrailTriangles>0;
  if(RangeCount>0&&!bSawMeleeRange)
  {
   Check(Outline&&Outline->Num()>=3,TEXT("range_cue_has_no_query_outline"));
   if(Outline&&Outline->Num()>=3)
   {
    Diameter=0.;
    for(int32 A=0;A<Outline->Num();++A)
    {
     for(int32 B=A+1;B<Outline->Num();++B){Diameter=FMath::Max(Diameter,FVector::Dist2D((*Outline)[A],(*Outline)[B]));}
    }
    const double MaxDiameter=Skill.AttackShape==ERogue10mAttackShape::LinearBox
      ?FVector(Skill.AttackRange,2.*Skill.BoxHalfWidth,2.*Skill.BoxHalfHeight).Length():2.*Skill.AttackRange;
    Check(FMath::IsFinite(Diameter)&&Diameter>=.5*Skill.AttackRange&&Diameter<=MaxDiameter+1.,
      TEXT("range_outline_diameter_outside_authored_query_envelope"));
   }
   CasePresentationTier=Tier;
   if(Index==0||Index==5){Check(Tier==0,TEXT("base_attack_should_use_first_vfx_tier"));}
   if(Index>=10){Check(Tier==3,TEXT("capstone_should_use_highest_vfx_tier"));}
   bSawMeleeRange=true;
  }
  if(Impact.IsSet()&&!bSawMeleeImpact)
  {
   const auto* Capsule=Target->GetCapsuleComponent();
   const FVector Center=Capsule->GetComponentLocation();
   const double Radius=Capsule->GetScaledCapsuleRadius();
   const FVector HalfAxis=Capsule->GetUpVector()*(Capsule->GetScaledCapsuleHalfHeight()-Radius);
   const FVector AxisPoint=FMath::ClosestPointOnSegment(Impact.GetValue(),Center-HalfAxis,Center+HalfAxis);
   SurfaceGap=FMath::Abs(FVector::Distance(Impact.GetValue(),AxisPoint)-Radius);
   Check(!IsMeleeMissCase()&&PulseCount>0,TEXT("impact_cue_without_confirmed_damage"));
   Check(FMath::IsFinite(SurfaceGap)&&SurfaceGap<=.5,TEXT("impact_cue_not_on_target_capsule_surface"));
   bSawMeleeImpact=true;
  }
  MeleeVfxRows+=FString::Printf(TEXT("%d,%s,%d,%.6f,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f\n"),
   Index,*Skill.GetName(),Frame,Now,Damage,RangeCount,ImpactCount,Vfx->GetTrailSampleCount(),TrailTriangles,RangeTriangles,ImpactTriangles,
   Tier,Outline?Outline->Num():0,Diameter,SurfaceGap);
  // Mesh sections may be generated one tick after the gameplay event. Wait for both cues to be drawn.
  return RangeTriangles>0&&(IsMeleeMissCase()?ImpactTriangles==0:ImpactTriangles>0&&PulseCount>0);
 }
 void CheckMeleeVfxRecovery()
 {
  const auto* Vfx=Character->FindComponentByClass<URogue10mMeleeVfxComponent>();
  Check(Vfx!=nullptr,TEXT("accepted_action_did_not_create_production_vfx"));
  if(!Vfx){return;}
  Check(bSawMeleeTrail&&bSawMeleeRange,TEXT("motion_trail_or_query_outline_missing"));
  Check(bSawMeleeImpact!=IsMeleeMissCase(),TEXT("confirmed_hit_marker_missing_or_miss_marker_present"));
  if(IsMeleeMotionCase()){Check(bMotionCaptured,TEXT("representative_visible_motion_capture_missing"));}
  Check(Vfx->GetActiveRangeCount()==0&&Vfx->GetActiveImpactCount()==0&&Vfx->GetTrailSampleCount()==0,
   TEXT("melee_vfx_did_not_expire_by_recovery"));
  UE_LOG(LogRogue10m,Display,TEXT("MELEE_VFX CASE index=%d skill=%s miss=%d tier=%d damage_pulses=%d trail=%d range=%d impact=%d"),
   Index,*Skills[Index]->GetName(),IsMeleeMissCase(),CasePresentationTier,PulseCount,bSawMeleeTrail,bSawMeleeRange,bSawMeleeImpact);
 }
 void RecordBreakingKickPose(const USkeletalMeshComponent& Source,double Now,float MontageTime,double Damage,
   const UCapsuleComponent& Capsule,const FString& MontageSourcePath)
 {
  const FTransform Actor=Character->GetActorTransform();
  const FVector Center=Capsule.GetComponentLocation();
  const double Radius=Capsule.GetScaledCapsuleRadius();
  const FVector Axis=Capsule.GetUpVector()*FMath::Max(0.,Capsule.GetScaledCapsuleHalfHeight()-Radius);
  const auto* Display=Character->GetMesh();
  const FVector LeadToe=Display->GetSocketLocation(TEXT("ball_r"));
  const double Gap=FVector::Dist(LeadToe,FMath::ClosestPointOnSegment(LeadToe,Center-Axis,Center+Axis))-Radius;
  for(const auto* Mesh:{&Source,static_cast<const USkeletalMeshComponent*>(Display)})
  {
   for(const FName Bone:{FName(TEXT("root")),FName(TEXT("pelvis")),FName(TEXT("head")),FName(TEXT("hand_l")),FName(TEXT("hand_r")),
    FName(TEXT("calf_l")),FName(TEXT("calf_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r")),FName(TEXT("ball_l")),FName(TEXT("ball_r"))})
   {
    const FVector At=Actor.InverseTransformPosition(Mesh->GetSocketLocation(Bone));
    Check(Mesh->GetBoneIndex(Bone)!=INDEX_NONE&&!At.ContainsNaN(),TEXT("breaking_kick_measured_bone_valid"));
    const FVector Position=Actor.GetLocation();
    BreakingKickPoseRows+=FString::Printf(TEXT("%d,%.6f,%.6f,%.3f,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
      Frame,Now,MontageTime,Damage,Mesh==&Source?TEXT("manny"):TEXT("display"),*Bone.ToString(),
      At.X,At.Y,At.Z,Position.X,Position.Y,Position.Z,Gap);
   }
  }
  if(Damage>.01)
  {
   const double Clearance=LeadToe.Z-Actor.GetLocation().Z+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   UE_LOG(LogRogue10m,Display,TEXT("BREAKING_KICK_CONTACT frame=%d montage=%.6f toe_above_ground=%.4f toe_capsule_gap=%.4f clip=%s"),
    Frame,MontageTime,Clearance,Gap,*MontageSourcePath);
   if(bBreakingKickAuthored)
   {
    Check(MontageSourcePath.EndsWith(TEXT("A_Fighter_BreakingKick.A_Fighter_BreakingKick")),TEXT("breaking_kick_new_clip_used_at_actual_damage"));
    Check(Clearance>22.0&&Clearance<72.0&&Gap<=0.0,TEXT("breaking_kick_visible_toe_intersects_capsule_at_low_height"));
   }
  }
 }
 void RecordShoulderBashContact(const USkeletalMeshComponent& Source,double Now,float MontageTime,
   float SourceTime,double Damage,const FString& SourcePath)
 {
  static const FName Bones[]={TEXT("upperarm_r"),TEXT("hand_r"),TEXT("hand_l"),TEXT("foot_r"),TEXT("foot_l")};
  const FVector Origin=Character->GetActorLocation();
  const FVector Forward=Character->GetActorForwardVector();
  const USkeletalMeshComponent* Display=Character->GetMesh();
  const FString Sample=FString::Printf(TEXT("%d,%.6f,%.6f,%.6f,%.3f,%s"),Frame,Now,MontageTime,SourceTime,Damage,*SourcePath);
  const TCHAR* ExpectedPath=bShoulderBashAuthored
   ?TEXT("/Game/Rogue10m/Animation/Combat/FighterSkills/A_Fighter_ShoulderBash.A_Fighter_ShoulderBash")
   :TEXT("/Game/Rogue10m/Animation/Combat/FistAuthored/A_Knuckle_ShortBlow.A_Knuckle_ShortBlow");
  Check(SourcePath==ExpectedPath&&FMath::IsFinite(SourceTime)&&SourceTime>=0.f,TEXT("shoulder_bash_expected_source_at_first_damage"));
  Check(!Origin.ContainsNaN()&&Forward.IsNormalized(),TEXT("shoulder_bash_actor_frame_valid"));
  for(const auto* Mesh:{&Source,Display})
  {
   FVector Position[UE_ARRAY_COUNT(Bones)];
   double Projection[UE_ARRAY_COUNT(Bones)];
   for(int32 BoneIndex=0;BoneIndex<UE_ARRAY_COUNT(Bones);++BoneIndex)
   {
    Position[BoneIndex]=Mesh->GetSocketLocation(Bones[BoneIndex]);
    Projection[BoneIndex]=FVector::DotProduct(Position[BoneIndex]-Origin,Forward);
    Check(Mesh->GetBoneIndex(Bones[BoneIndex])!=INDEX_NONE&&!Position[BoneIndex].ContainsNaN(),
     TEXT("shoulder_bash_measured_bone_valid"));
   }
   const double Lead=Projection[0]-FMath::Max(Projection[1],Projection[2]);
   for(int32 BoneIndex=0;BoneIndex<UE_ARRAY_COUNT(Bones);++BoneIndex)
   {
    const FVector& Point=Position[BoneIndex];
    ShoulderBashPoseRows+=FString::Printf(TEXT("%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
     *Sample,Mesh==&Source?TEXT("manny"):TEXT("display"),*Bones[BoneIndex].ToString(),Point.X,Point.Y,Point.Z,
     Origin.X,Origin.Y,Origin.Z,Forward.X,Forward.Y,Forward.Z,Projection[BoneIndex],Lead);
   }
   if(Mesh==Display)
   {
    UE_LOG(LogRogue10m,Display,TEXT("SHOULDER_BASH_CONTACT frame=%d montage=%.6f shoulder_forward=%.4f right_hand_forward=%.4f left_hand_forward=%.4f lead_cm=%.4f clip=%s"),
     Frame,MontageTime,Projection[0],Projection[1],Projection[2],Lead,*SourcePath);
    if(bShoulderBashAuthored){Check(FMath::IsFinite(Lead)&&Lead>0.,TEXT("shoulder_bash_visible_shoulder_leads_both_hands"));}
   }
  }
 }
 void Step()
 {
  if(bFinished){return;}
  if(TechniqueViewport.IsValid()){TechniqueViewport->bDisableWorldRendering=!FScreenshotRequest::IsScreenshotRequested();}
  if(bCapturePending&&FPlatformTime::Seconds()-CaptureRequestedAt>30.)
  {
   Check(false,TEXT("screenshot_processing_timeout"));
   CaptureRows+=FString::Printf(TEXT("timeout,%s,%s,%d,%.6f,%.6f,%s,-1\n"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
   FScreenshotRequest::Reset();bCapturePending=false;bFinishRequested=true;
  }
  if(bFinishRequested){Finish();return;}
  ++Frame; if(Frame>12000){Check(false,TEXT("fixture_timeout"));Finish();return;}
  if(!Character.IsValid()||!Target.IsValid()||!Controller.IsValid()){Check(false,TEXT("fixture_lost"));Finish();return;}
  if(bCapstoneBraceRun){StepCapstoneBrace();return;}
  if(bRecoveryRequested)
  {
   if(bCapturePending){return;}
   Check(bRecoveryCaptured,TEXT("recovery_capture_not_completed"));
   UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P COMPLETE skill=%s instance_samples=%d damage_pulses=%d rendered_hand_travel_cm=%.3f"),*Skills[Index]->GetName(),MontageSamples,PulseCount,RenderedHandMaxTravelCm);
   ++Index;Next();return;
  }
  auto* Skill=Skills[Index].Get();auto* C=Character->GetCombatComponent();auto* Source=Character->GetAnimationPlaybackMesh();auto* Anim=Source->GetAnimInstance();
  auto* Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();auto* Right=Visual?Visual->GetRightWeaponMesh():nullptr;auto* Left=Visual?Visual->GetLeftWeaponMesh():nullptr;
  if(Frame%60==0){Check(Right&&Right->IsVisible()&&Right->GetAttachParent()==Character->GetMesh()&&Right->GetAttachSocketName()==TEXT("hand_r"),TEXT("right_weapon_attachment_failed"));Check(IsSwordCase()?!Left||!Left->IsVisible():Left&&Left->IsVisible(),TEXT("offhand_visibility_failed"));}
  const double Now=World->GetTimeSeconds(),Age=Now-CaseStartedAt;
  const double MinFootZ=FMath::Min(Character->GetMesh()->GetSocketLocation(TEXT("foot_l")).Z,Character->GetMesh()->GetSocketLocation(TEXT("foot_r")).Z);
  const double CapsuleBottomZ=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const double FootAboveCapsuleBottomCm=MinFootZ-CapsuleBottomZ;
  const bool Charged=Skill->RequiresLoadoutCharge();
  if(!AttackSent&&Age>=(bSwordTechniqueRender?.35:1.))
  {
   if(!bReadyRequested)
   {
    if(auto* A=Character->GetRogueAttributeSet()){A->RestoreVitals();}
    Check(Character->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Character->GetMesh()->DoesSocketExist(TEXT("foot_r")),TEXT("rendered_foot_bones_missing"));
    Check(FMath::IsFinite(FootAboveCapsuleBottomCm)&&FootAboveCapsuleBottomCm>=0.&&FootAboveCapsuleBottomCm<=30.,TEXT("ready_rendered_feet_outside_capsule_ground_band"));
    UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P READY_GROUND skill=%s min_foot_world_z=%.3f capsule_bottom_world_z=%.3f marker_above_bottom_cm=%.3f allowed_cm=0..30"),*Skill->GetName(),MinFootZ,CapsuleBottomZ,FootAboveCapsuleBottomCm);
    bReadyRequested=true;if(!Capture(TEXT("ready"))){Finish();}return;
   }
   if(bCapturePending){return;}
   if(!bReadyCaptured){Check(false,TEXT("ready_capture_not_completed"));Finish();return;}
   // CancelCombatVisuals does not erase the real shared cooldown.
   if(C->GetAttackCooldownRemaining()>0.f||C->IsWeaponChangeBlocked()){return;}
   if(!Chain.IsEmpty()){ChainUses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Chain[0]->GetFName());}
   BaselineVfx();
   if(bSwordTechniqueAudit||IsFocusedKnuckleCase()){SendTechniqueKey(true);}else{C->HandleEquippedSkillPressed(CaseSlot);}
   AttackSent=true;Pressed=Now;
  }
  if(IsEarthSplitterCase()&&AttackSent&&!ChargeReleased&&bEarthSplitterMoving)
  {
   const double HoldAge=Now-Pressed;
   if(!bEarthBeganStrafe&&HoldAge>=.48)
   {
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::D,IE_Pressed,1.f));
    bEarthBeganStrafe=true;
   }
   else if(bEarthBeganStrafe&&!bEarthEndedStrafe&&HoldAge>=.62)
   {
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::D,IE_Released,0.f));
    bEarthEndedStrafe=true;
   }
  }
  if(EarthWall.IsValid()&&AttackSent&&!ChargeReleased)
  {
   FRotator Look=EarthInitialLook;
   Look.Yaw+=34.*FMath::Clamp((Now-Pressed-.45)/.28,0.,1.);
   Controller->SetControlRotation(Look);
  }
  if(bEarthSplitterRender&&AttackSent&&!ChargeReleased&&!bMotionRequested&&!bCapturePending&&Now-Pressed>=.30)
  {
   bMotionRequested=Capture(TEXT("motion"));
  }
  if(AttackSent&&!ChargeReleased&&(Charged?Now-Pressed>Skill->ChargeSeconds+.05:(bSwordTechniqueAudit||IsFocusedKnuckleCase())&&Now-Pressed>.025))
  {
   if(bEarthSplitterRender&&!bEarthHeldCaptureRequested)
   {
    bEarthHeldCaptureRequested=Capture(TEXT("fully_charged"));
    if(!bEarthHeldCaptureRequested){Finish();}
    return;
   }
   if(bEarthSplitterRender&&bCapturePending){return;}
   if(Charged){BaselineVfx();}
   if(bSwordTechniqueAudit||IsFocusedKnuckleCase()){SendTechniqueKey(false);}else{C->HandleEquippedSkillReleased(CaseSlot);}
   ChargeReleased=true;
  }
  if(AttackSent&&!Chain.IsEmpty()&&ChainStep<Chain.Num()-1)
  {
   auto* Precursor=Chain[ChainStep].Get();
   const int32 Count=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Precursor->GetFName());
   if(ChainAcceptedAt<0&&Count>ChainUses)
   {
    Check(Count==ChainUses+1,TEXT("combo_precursor_not_accepted_once"));ChainAcceptedAt=Now;
    const float Speed=FMath::Max(.01f,Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier());
    const float LastHit=Precursor->HitStartDelaySeconds/FMath::Max(.01f,Speed*Precursor->AnimationPlayRate)
     +(Precursor->HitMode==ERogue10mAttackHitMode::Single?0:FMath::Clamp(Precursor->HitCount,1,64)-1)*FMath::Max(.01f,Precursor->HitInterval/Speed);
    // Drain precursor pulses before attributing damage to the selected motion.
    const float Offset=FMath::Max(Precursor->ComboWindowOpenSeconds/Speed+.05f,LastHit+.10f);
    Check(Offset<Precursor->ComboWindowCloseSeconds/Speed-.025f,TEXT("combo_window_cannot_separate_precursor_damage"));
    if(Failures){Finish();return;}ChainNextAt=Now+Offset;
    UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P PRECURSOR_ACCEPT case=%s skill=%s uses=%d next_in=%.6f"),*Skill->GetName(),*Precursor->GetName(),Count,Offset);
   }
   if(ChainAcceptedAt>=0&&Now>=ChainNextAt)
   {
    ++ChainStep;ChainUses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Chain[ChainStep]->GetFName());
    // Baseline BEFORE input: zero-windup skills can damage synchronously on acceptance.
    PreviousHealth=Target->GetRogueAttributeSet()->GetHealth();
    if(ChainStep==Chain.Num()-1){BaselineVfx();}
    ChainAcceptedAt=-1.;C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::Primary);
   }
   if(ChainStep<Chain.Num()-1)
   {
    PreviousHealth=Target->GetRogueAttributeSet()->GetHealth();
    if(Now-Pressed>12.){Check(false,TEXT("combo_precursor_not_accepted"));Finish();}
    return;
   }
  }
  const int32 CurrentUses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(Skill->GetFName());
  if(Accepted<0&&CurrentUses>Uses)
  {
   Check(CurrentUses==Uses+1,TEXT("attack_not_accepted_once"));Check(!Charged||ChargeReleased,TEXT("charged_skill_accepted_before_release"));Accepted=Now;
   ReadyRightHandLocal=Character->GetMesh()->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation();
   ReadyLeftHandLocal=Character->GetMesh()->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation();
   Expected=Skill->HitStartDelaySeconds/FMath::Max(.01f,Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier()*Skill->AnimationPlayRate);
   UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P ACCEPT skill=%s expected=%.6f montage=%s"),*Skill->GetName(),Expected,*Skill->AttackMontage->GetPathName());

  }
  if(IsVfxCase()&&Accepted>=0)
  {
   ObserveVfx(Skill->CastEffect.Get(),TEXT("cast"),TEXT("sample"));
   ObserveVfx(Skill->ImpactEffect.Get(),TEXT("impact"),TEXT("sample"));
   if(!bAcceptanceRequested&&!bCapturePending&&!bVfxTimedOut)
   {bAcceptanceRequested=CaptureVfx(TEXT("acceptance_cue"),Skill->CastEffect.Get(),TEXT("cast"));}
  }
  // IsPlaying proves an active montage instance, not a visible/evaluated output pose.
  const bool Playing=Anim&&Anim->Montage_IsPlaying(Skill->AttackMontage);if(Playing){++MontageSamples;}
  if(Accepted>=0&&Playing)
  {
   const FVector RightLocal=Character->GetMesh()->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation();
   const FVector LeftLocal=Character->GetMesh()->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation();
   Check(!RightLocal.ContainsNaN()&&!LeftLocal.ContainsNaN(),TEXT("rendered_hand_pose_nonfinite"));
   RenderedHandMaxTravelCm=FMath::Max(RenderedHandMaxTravelCm,FMath::Max(FVector::Distance(RightLocal,ReadyRightHandLocal),FVector::Distance(LeftLocal,ReadyLeftHandLocal)));
  }
  // One OnWorldPostActorTick observation: cache source times, evaluated body geometry and health
  // before any screenshot request. This is not a hit callback or a synchronization barrier between
  // damage timers, source animation evaluation and rendered retarget bone finalization.
  const float MontageTime=Anim?Anim->Montage_GetPosition(Skill->AttackMontage):-1;
  float MontageSourceTime=-1.f;FString MontageSourcePath=TEXT("None");
  if(Playing&&Skill->AttackMontage->SlotAnimTracks.Num()==1)
  {
   const auto* Segment=Skill->AttackMontage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(MontageTime);
   if(Segment&&Segment->GetAnimReference()){MontageSourceTime=Segment->ConvertTrackPosToAnimPos(MontageTime);MontageSourcePath=Segment->GetAnimReference()->GetPathName();}
  }
  const FVector RH=Character->GetMesh()->GetSocketLocation(TEXT("hand_r")),LH=Character->GetMesh()->GetSocketLocation(TEXT("hand_l"));
  const FTransform WT=Right?Right->GetComponentTransform():FTransform::Identity;
  const FVector Base=WT.TransformPosition(FVector(0,0,18)),Tip=WT.TransformPosition(FVector(0,0,140));
  FVector A,B;const FVector Center=Target->GetActorLocation();FMath::SegmentDistToSegmentSafe(Base,Tip,Center-FVector(0,0,45),Center+FVector(0,0,45),A,B);const double BladeDistance=FVector::Distance(A,B);
  // Preserve legacy blade_target_cm above for default-mode evidence comparisons.
  // New sword-only proxy uses the target's actual scaled capsule and orientation, not its actor origin.
  const auto* TargetCapsule=Target->GetCapsuleComponent();
  const FVector TargetCenter=TargetCapsule->GetComponentLocation(),TargetUp=TargetCapsule->GetUpVector();
  const double TargetRadius=TargetCapsule->GetScaledCapsuleRadius(),TargetHalfHeight=TargetCapsule->GetScaledCapsuleHalfHeight();
  const double TargetAxisHalf=FMath::Max(0.,TargetHalfHeight-TargetRadius);
  FVector BladeClosest,CapsuleClosest;
  FMath::SegmentDistToSegmentSafe(Base,Tip,TargetCenter-TargetUp*TargetAxisHalf,TargetCenter+TargetUp*TargetAxisHalf,BladeClosest,CapsuleClosest);
  const double SwordCapsuleGap=IsSwordCase()?FVector::Distance(BladeClosest,CapsuleClosest)-TargetRadius:-1.;
  if(bSwordGuardBreakAudit&&Playing&&!IsMeleeMissCase()&&Right&&Right->IsVisible()
   &&FVector::DistSquared(Base,Tip)>1.&&FMath::IsFinite(SwordCapsuleGap))
  {
   GuardBreakNearestBladeGap=FMath::Min(GuardBreakNearestBladeGap,SwordCapsuleGap);
  }
  const double Health=Target->GetRogueAttributeSet()->GetHealth(),Drop=PreviousHealth-Health;
  if(bBreakingKickAudit){RecordBreakingKickPose(*Source,Now,MontageTime,Drop,*TargetCapsule,MontageSourcePath);}
  if(Drop>.01)
  {
   // This deferred capture is requested after health changes, not an exact contact-frame image.
   ++PulseCount;if(PulseCount==1){Check(Accepted>=0,TEXT("damage_before_acceptance"));Check(FMath::Abs((Now-Accepted)-Expected)<.05,TEXT("actual_damage_delay_mismatch"));}
   if(PulseCount==1)
   {
    if(bShoulderBashAudit){RecordShoulderBashContact(*Source,Now,MontageTime,MontageSourceTime,Drop,MontageSourcePath);}
    const bool bStrictSword=bRequireAuthoredSword&&Index<5;
    if(bStrictSword)
    {
     ++SwordFirstDamageChecks;
     Check(Right&&Right->IsVisible()&&!Base.ContainsNaN()&&!Tip.ContainsNaN()&&FVector::Distance(Base,Tip)>1.,TEXT("authored_sword_blade_segment_valid"));
     Check(FMath::IsFinite(TargetRadius)&&TargetRadius>0.&&FMath::IsFinite(TargetHalfHeight)&&TargetHalfHeight>=TargetRadius&&!TargetCenter.ContainsNaN()&&!TargetUp.ContainsNaN(),TEXT("authored_sword_target_capsule_valid"));
     Check(FMath::IsFinite(SwordCapsuleGap)&&SwordCapsuleGap<=10.,TEXT("authored_sword_first_damage_capsule_surface_gap_exceeds_10cm"));
     Check(Playing&&FMath::IsFinite(MontageSourceTime)&&MontageSourceTime>=0.f&&MontageSourcePath!=TEXT("None"),TEXT("authored_sword_first_damage_source_position_required"));
    }
    UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P FIRST_DAMAGE skill=%s frame=%d world=%.6f montage_s=%.6f source_s=%.6f source=%s sword_capsule_gap_cm=%.6f target_radius_cm=%.6f target_halfheight_cm=%.6f require_authored_sword=%d sword_gate=%d limit_cm=10 criterion=abstract_capsule_intersection_timing_proxy"),*Skill->GetName(),Frame,Now,MontageTime,MontageSourceTime,*MontageSourcePath,SwordCapsuleGap,TargetRadius,TargetHalfHeight,bRequireAuthoredSword?1:0,bStrictSword?1:0);
   }
   UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P DAMAGE skill=%s pulse=%d elapsed=%.6f expected=%.6f blade_target_cm=%.3f"),*Skill->GetName(),PulseCount,Now-Accepted,Expected,BladeDistance);
  }
  PreviousHealth=Health;
  const bool bMeleeVfxReady=!bSwordTechniqueAudit&&bMeleeVfxRun&&Accepted>=0&&ObserveMeleeVfx(*Skill,Now,Drop);
  const bool bMeleeContact=bSwordTechniqueAudit||IsFocusedKnuckleCase()?Accepted>=0&&(PulseCount>0||IsMeleeMissCase()):bMeleeVfxReady;
  if((bMeleeVfxRun?bMeleeContact:PulseCount>0)&&!bFirstDamageRequested&&!bCapturePending)
  {
   bFirstDamageRequested=Capture(IsMeleeMissCase()?TEXT("contact_miss"):TEXT("after_first_damage"));
  }
  else if(bCrushingCapture&&Playing&&bFirstDamageCaptured&&!bCrushingLoadRequested&&!bCapturePending&&MontageTime>=.50)
  {
   bCrushingLoadRequested=Capture(TEXT("overhead_load"));
  }
  else if(bSwordTechniqueRender&&Playing&&bFirstDamageCaptured&&!bMotionRequested&&!bCapturePending
   &&MontageTime>=(bSwordGuardBreakAudit?.694:bCrushingCapture?.761:1.02))
  {
   bMotionRequested=Capture(TEXT("motion"));
  }
  else if(IsMeleeMotionCase()&&Accepted>=0&&!bMotionRequested&&!bCapturePending
   &&TrailTrianglesThisFrame>0&&Frame>FirstTrailMeshFrame
   &&(PulseCount==0||(Index==10&&bFirstDamageCaptured)))
  {
   // The upper Sword pose first exposes its blade ribbon after the gameplay contact.
   bMotionRequested=Capture(TEXT("motion"));
  }
  if(IsVfxCase()&&PulseCount>0&&bFirstDamageCaptured&&!bImpactResidualRequested&&!bCapturePending&&!bVfxTimedOut)
  {bImpactResidualRequested=CaptureVfx(TEXT("impact_residual"),Skill->ImpactEffect.Get(),TEXT("impact"));}
  if(IsVfxCase()&&Accepted>=0&&!bVfxTimedOut&&Now-Accepted>Expected+2.&&(!bAcceptanceRequested||!bImpactResidualRequested))
  {
   bVfxTimedOut=true;
   Check(bAcceptanceRequested,TEXT("cast_component_two_frames_positive_actual_age_capture_timeout"));
   Check(bImpactResidualRequested,TEXT("impact_component_two_frames_positive_actual_age_capture_timeout"));
   UE_LOG(LogRogue10m,Error,TEXT("WEAPON3P VFX_TIMEOUT skill=%s frame=%d acceptance_requested=%d impact_requested=%d elapsed=%.6f"),*Skill->GetName(),Frame,bAcceptanceRequested?1:0,bImpactResidualRequested?1:0,Now-Accepted);
  }
  Rows+=FString::Printf(TEXT("%d,%.6f,%s,%s,%.6f,%.3f,%.3f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s\n"),Frame,Now,*Skill->GetName(),Accepted<0?TEXT("ready"):Playing?TEXT("attack"):TEXT("recovery"),MontageTime,Health,Drop,Accepted<0?-1:Now-Accepted,Expected,BladeDistance,RH.X,RH.Y,RH.Z,LH.X,LH.Y,LH.Z,Base.X,Base.Y,Base.Z,Tip.X,Tip.Y,Tip.Z,MinFootZ,CapsuleBottomZ,FootAboveCapsuleBottomCm,TargetCenter.X,TargetCenter.Y,TargetCenter.Z,TargetRadius,TargetHalfHeight,TargetUp.X,TargetUp.Y,TargetUp.Z,SwordCapsuleGap,MontageSourceTime,*MontageSourcePath);
  if(bSwordTechniqueAudit&&Accepted>=0)
  {
   const auto* Instance=Anim?Anim->GetActiveInstanceForMontage(Skill->AttackMontage):nullptr;
   const FString Sample=FString::Printf(TEXT("%d,%s,%d,%.6f,%.6f,%.6f"),Index,*Skill->GetName(),
    Frame,Now-Accepted,MontageTime,Instance?Instance->GetWeight():0.f);
   const FVector Actor=Character->GetActorLocation();
   SwordTechniqueActor+=FString::Printf(TEXT("%d,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),Index,*Skill->GetName(),
    Frame,Now-Accepted,MontageTime,Actor.X,Actor.Y,Actor.Z,Character->GetActorRotation().Yaw);
   static const FName Bones[]={TEXT("pelvis"),TEXT("head"),TEXT("upperarm_r"),TEXT("upperarm_l"),TEXT("lowerarm_r"),TEXT("lowerarm_l"),
    TEXT("hand_r"),TEXT("hand_l"),TEXT("middle_01_r"),TEXT("middle_01_l"),TEXT("foot_r"),TEXT("foot_l")};
   const USkeletalMeshComponent* Meshes[]={Source,Character->GetMesh()};
   for(const auto* Mesh:Meshes)
   {
    for(const FName Bone:Bones)
    {
     const FTransform Pose=Mesh->GetBoneTransform(Bone,RTS_Component);
     Check(!Pose.ContainsNaN()&&Mesh->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("learned_technique_source_and_visible_bones_are_finite"));
     const FVector P=Pose.GetLocation();
     const FQuat Q=Pose.GetRotation();
     SwordTechniquePoses+=FString::Printf(TEXT("%s,%s,%u,%s,%.4f,%.4f,%.4f,%.7f,%.7f,%.7f,%.7f\n"),
      *Sample,Mesh==Source?TEXT("manny"):TEXT("visible"),Mesh->GetBoneTransformRevisionNumber(),*Bone.ToString(),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W);
    }
   }
  }
  if(bEarthSplitterAudit&&IsEarthSplitterCase()&&AttackSent&&Right&&Right->GetStaticMesh())
  {
   const auto* Body=Character->GetMesh();
   const FVector RightPalm=(Body->GetSocketLocation(TEXT("hand_r"))+Body->GetSocketLocation(TEXT("middle_01_r")))*.5;
   const FVector LeftPalm=(Body->GetSocketLocation(TEXT("hand_l"))+Body->GetSocketLocation(TEXT("middle_01_l")))*.5;
   const FTransform Sword=Right->GetComponentTransform();
   const FVector BladeDirection=Sword.GetUnitAxis(EAxis::Z);
   const FBox BladeBounds=Right->GetStaticMesh()->GetBoundingBox().TransformBy(Sword);
   const FTransform ActorWorld=Character->GetActorTransform();
   const FVector LocalBlade=ActorWorld.InverseTransformVectorNoScale(BladeDirection);
   const FVector LocalRightPalm=ActorWorld.InverseTransformPosition(RightPalm);
   const FVector LocalLeftPalm=ActorWorld.InverseTransformPosition(LeftPalm);
   const double LocalPelvisZ=ActorWorld.InverseTransformPosition(Body->GetSocketLocation(TEXT("pelvis"))).Z;
   double WallAxisGap=-1.;
   if(EarthWall.IsValid())
   {
    const auto* Shape=CastChecked<UBoxComponent>(EarthWall->GetRootComponent());
    const FVector Gap=(BladeBounds.GetCenter()-Shape->GetComponentLocation()).GetAbs()-BladeBounds.GetExtent()-Shape->GetScaledBoxExtent();
    WallAxisGap=Gap.GetMax();
   }
   EarthSplitterRows+=FString::Printf(TEXT("%d,%d,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
    Index,Frame,Now-Pressed,Accepted<0.0?-1.0:Now-Accepted,MontageTime,ChargeReleased,Drop,
    BladeBounds.Min.Z-CapsuleBottomZ,Tip.Z-CapsuleBottomZ,LocalBlade.Z,LocalBlade.X,FVector::Distance(RightPalm,LeftPalm),
    FVector::Distance(RightPalm,Sword.GetLocation()),FVector::Dist2D(Character->GetActorLocation(),EarthInitialActor),
    Character->GetActorRotation().Yaw,WallAxisGap,LocalBlade.Y,
    LocalRightPalm.X,LocalRightPalm.Y,LocalRightPalm.Z,LocalLeftPalm.X,LocalLeftPalm.Y,LocalLeftPalm.Z,
    LocalPelvisZ,FootAboveCapsuleBottomCm);
  }
  if(Accepted>=0&&!Playing&&Now-Accepted>Expected+.7&&!C->IsWeaponChangeBlocked())
  {
   if(bCapturePending){return;}
   if(IsVfxCase()&&!bVfxTimedOut&&(!bAcceptanceRequested||!bImpactResidualRequested)){return;}
   const int32 Wanted=IsMeleeMissCase()?0:Skill->HitMode==ERogue10mAttackHitMode::Single?1:Skill->HitCount;
   Check(PulseCount==Wanted,TEXT("actual_damage_count_mismatch"));Check(MontageSamples>5,TEXT("montage_instance_not_playing_enough_samples"));
   Check(RenderedHandMaxTravelCm>2.,TEXT("rendered_body_hand_motion_missing"));Check(bFirstDamageCaptured,TEXT("after_first_damage_capture_not_completed"));
   if(bSwordTechniqueRender){Check(bMotionCaptured,TEXT("technique_rendered_motion_missing"));}
   if(bCrushingCapture){Check(bCrushingLoadCaptured,TEXT("crushing_slash_overhead_capture_missing"));}
   if(bSwordGuardBreakAudit&&!IsMeleeMissCase())
   {
    UE_LOG(LogRogue10m,Display,TEXT("GREAT_SWORD_GUARD_BREAK case=%d nearest_visible_blade_capsule_gap_cm=%.6f"),Index,GuardBreakNearestBladeGap);
    Check(GuardBreakNearestBladeGap<=0.,TEXT("guard_break_visible_axis_never_crossed_target_capsule"));
   }
   if(IsEarthSplitterCase()&&bEarthSplitterMoving)
   {
    Check(bEarthEndedStrafe&&FVector::Dist2D(Character->GetActorLocation(),EarthInitialActor)>2.0,TEXT("earthsplitter_input_really_moves_during_charge"));
   }
   if(EarthWall.IsValid())
   {
    Check(FMath::Abs(FRotator::NormalizeAxis(Character->GetActorRotation().Yaw-EarthInitialLook.Yaw))>25.,
     TEXT("earthsplitter_character_actually_turns_into_wall_corridor"));
    Controller->SetControlRotation(EarthInitialLook);
   }
   if(bMeleeVfxRun&&!bSwordTechniqueAudit){CheckMeleeVfxRecovery();}
   bRecoveryRequested=true;if(!Capture(TEXT("recovery"))){Finish();return;}
  }
  else if(Accepted<0&&AttackSent&&Now-Pressed>16.){Check(false,TEXT("attack_not_accepted"));++Index;Next();}
 }
 void Finish()
 {
  if(bFinished)return;
  if(bCapturePending){bFinishRequested=true;return;}
  if(TechniqueViewport.IsValid()){TechniqueViewport->bDisableWorldRendering=bPreviousWorldRenderingDisabled;}
  bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessed);
  if(bSwordEquipAudit)
  {
   if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
   Check(FFileHelper::SaveStringToFile(SwordEquipRows,*(Output/TEXT("sword-equip.csv"))),TEXT("equip_audit_trace_saved"));
   if(!bMeleeVfxLogicOnly)
   {
    Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("equip_audit_captures_saved"));
   }
   const bool bPassed=Failures==0&&Frame==88;
   UE_LOG(LogRogue10m,Display,TEXT("RESULT=SWORD_EQUIP_POSE_%s appearance=%s frames=%d failures=%d"),
    bPassed?TEXT("PASSED"):TEXT("FAILED"),*SwordEquipAppearance,Frame,Failures);
   FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);return;
  }
  if(bCapstoneBraceRun){FinishCapstoneBrace();return;}
  if(IsFocusedKnuckleCase())
  {
   if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
   const FString& Poses=bShoulderBashAudit?ShoulderBashPoseRows:BreakingKickPoseRows;
   const TCHAR* PoseFile=bShoulderBashAudit?TEXT("shoulder-bash-poses.csv"):TEXT("breaking-kick-poses.csv");
   Check(FFileHelper::SaveStringToFile(Poses,*(Output/PoseFile)),TEXT("focused_knuckle_pose_report_saved"));
   Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("focused_knuckle_captures_saved"));
   Check(bMeleeVfxLogicOnly||CompletedCaptures==3,TEXT("focused_knuckle_ready_contact_recovery_captures"));
   const bool bPass=Failures==0&&Index==FocusedKnuckleCaseIndex()+1;
   UE_LOG(LogRogue10m,Display,TEXT("RESULT=%s_%s completed=%d failures=%d"),
    bShoulderBashAudit?TEXT("SHOULDER_BASH"):TEXT("BREAKING_KICK"),bPass?TEXT("PASSED"):TEXT("FAILED"),bPass?1:0,Failures);
   FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);return;
  }
  for(const FString& Path:CapturePaths){Check(IFileManager::Get().FileSize(*Path)>0,TEXT("final_screenshot_file_missing_or_empty"));}
  const int32 ExpectedCases=bSwordTechniqueRender?1:bSwordGuardBreakAudit?GuardBreakCaseCount:
   bSwordTechniqueAudit?TechniqueCount*2:bMeleeVfxRun?14:10;
  const int32 ExpectedCaptures=bMeleeVfxLogicOnly?0:(bSwordTechniqueRender?((bCrushingCapture||bEarthSplitterRender)?5:4):bMeleeVfxRun?46:34);
  if(bSwordTechniqueAudit)
  {
   Check(FFileHelper::SaveStringToFile(SwordTechniquePoses,*(Output/TEXT("technique-poses.csv"))),
    TEXT("sword_technique_source_and_visible_trajectory_saved"));
   Check(FFileHelper::SaveStringToFile(SwordTechniqueActor,*(Output/TEXT("technique-actor.csv"))),
    TEXT("sword_technique_capsule_trajectory_saved"));
   if(!EarthSplitterRows.IsEmpty())
   {
    const FString Header=TEXT("case,frame,hold_s,accepted_s,montage_s,release,damage,bounds_min_above_capsule_cm,tip_above_capsule_cm,blade_dir_z,blade_dir_forward,palm_span_cm,grip_palm_error_cm,actor_travel_cm,actor_yaw_deg,wall_axis_gap_cm,blade_dir_right,right_palm_local_x,right_palm_local_y,right_palm_local_z,left_palm_local_x,left_palm_local_y,left_palm_local_z,pelvis_local_z_cm,lowest_foot_above_capsule_cm\n");
    Check(FFileHelper::SaveStringToFile(Header+EarthSplitterRows,*(Output/TEXT("earthsplitter-charge.csv"))),TEXT("earthsplitter_charge_geometry_saved"));
   }
  }
  if(Index==ExpectedCases){Check(CapturePaths.Num()==ExpectedCaptures&&CompletedCaptures==ExpectedCaptures,TEXT("all_phase_captures_required"));}
  if(bRequireAuthoredSword){Check(AuthoredSwordValidated==5&&SwordFirstDamageChecks==5,TEXT("all_five_authored_sword_contracts_and_first_damage_gates_required"));}
  Check(FFileHelper::SaveStringToFile(Rows,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_save_failed"));
  Check(FFileHelper::SaveStringToFile(CaptureRows,*(Output/TEXT("captures.csv"))),TEXT("capture_evidence_save_failed"));
  Check(FFileHelper::SaveStringToFile(VfxRows,*(Output/TEXT("vfx-components.csv"))),TEXT("vfx_component_evidence_save_failed"));
  if(bMeleeVfxRun)
  {
   Check(FFileHelper::SaveStringToFile(MeleeVfxRows,*(Output/TEXT("melee-vfx.csv"))),TEXT("melee_vfx_evidence_save_failed"));
  }
  const FString Summary=FString::Printf(TEXT("frames=%d\nfailures=%d\ncompleted_skills=%d\nrequested_captures=%d\ncompleted_captures=%d\nrequire_authored_sword=%d\nauthored_sword_validated=%d\nsword_first_damage_checks=%d\nsword_first_damage_surface_gap_limit_cm=10\nNOTE=Third-person-only; validated M13 mastery fixture with automatic catalog grants. Production Primary is unchanged; legal loadout inputs or actual Primary combo precursors select each exact skill. Ready for a combo case is before its precursor chain, not the follow-up attack pose. Accepted bound-skill inputs and target health. Ready/recovery transitions wait for screenshot processing and non-empty files. after_first_damage is a deferred screenshot requested after health loss; captures.csv records request/processing times, not exact rendered contact timing. Montage samples count active instances only; component-local rendered-body hand travel must exceed 2cm. Ready rendered foot markers must be 0..30cm above the capsule bottom; attack foot gaps are recorded without a strict bound. The optional authored-sword gate checks five task-owned montage paths and positive hit delays, then signed blade-to-actual-capsule surface gap <=10cm on the first HP-drop observation only. Knuckle gap is -1 (not applicable) and is never gated. This is an abstract capsule intersection/timing proxy, not skeletal-mesh contact, physical collision or visual approval. montage_s, montage_source_s/path, target capsule and rendered body/weapon coordinates are cached in the same world-post-actor-tick callback as HP observation. Damage timers, source pose and retarget bone finalization can have different update order or pose age; this is not an exact contact-frame synchronization guarantee. Screenshots are deferred and can show a later pose. acceptance_cue and impact_residual require a matching same-world post-input component observed for at least two fixture frames and positive actual simulation age from FNiagaraSystemInstanceController::GetAge; desired age is never used. vfx-components.csv records cast and impact separately per component, including request-phase readbacks. Positive simulation age is numerical evidence only, not proof of visible particles or approved pixels.\n"),Frame,Failures,Index,CapturePaths.Num(),CompletedCaptures,bRequireAuthoredSword?1:0,AuthoredSwordValidated,SwordFirstDamageChecks);
  const FString MeleeSummary=FString::Printf(
   TEXT("frames=%d\nfailures=%d\ncompleted_skills=%d\nrequested_captures=%d\ncompleted_captures=%d\nrequire_authored_sword=%d\nauthored_sword_validated=%d\nsword_first_damage_checks=%d\n")
   TEXT("NOTE=Actual third-person animation on a transient flat platform in StartHub, legal programmatic bindings and combo inputs, default camera, separate saves, M20 mastery with four paid Sword nodes. ")
   TEXT("The same montage/weapon path is checked for each capstone against an in-range target and a position outside the query. ")
   TEXT("A contact capture is deferred until after mesh sections report geometry. Four separate motion captures sample a frame after the trail appears. ")
   TEXT("Capture mode: %s. A capsule measurement is a collision proxy and visual quality requires inspecting the rendered PNGs.\n"),
   Frame,Failures,Index,CapturePaths.Num(),CompletedCaptures,bRequireAuthoredSword?1:0,AuthoredSwordValidated,SwordFirstDamageChecks,
   bMeleeVfxLogicOnly?TEXT("NullRHI, numeric phases only"):TEXT("rendered PNGs"));
  const FString TechniqueSummary=FString::Printf(TEXT("completed_skills=%d\nfailures=%d\nframes=%d\nNOTE=%s Real Q/RMB controller input, Manny and visible pose.\n"),Index,Failures,Frame,
   bSwordGuardBreakAudit?(bSwordTechniqueRender?TEXT("One GuardBreak at 110 cm with rendered phases."):
    TEXT("GuardBreak at three real frontal positions and one position outside its query.")):
   bCrushingCapture?TEXT("One CrushingSlash at a frontal placement with rendered phases."):
   bEarthSplitterRender?TEXT("One EarthSplitter case with measured charge and rendered phases."):
   bSwordTechniqueRender?TEXT("One Whirlwind case with a rear target and rendered phases."):TEXT("Four learned GreatSword skills, each inside and outside its combat query."));
  Check(FFileHelper::SaveStringToFile(bSwordTechniqueAudit?TechniqueSummary:bMeleeVfxRun?MeleeSummary:Summary,*(Output/TEXT("metrics.txt"))),TEXT("summary_evidence_save_failed"));
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  const bool Passed=Failures==0&&Index==ExpectedCases;
  const TCHAR* ResultName=bSwordGuardBreakAudit?TEXT("GREAT_SWORD_GUARD_BREAK"):
   bSwordTechniqueAudit?TEXT("GREAT_SWORD_TECHNIQUES"):
   bMeleeVfxLogicOnly?TEXT("MELEE_VFX_LOGIC"):(bMeleeVfxRun?TEXT("MELEE_VFX"):TEXT("WEAPON_THIRDPERSON"));
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=%s_%s completed=%d failures=%d"),ResultName,
   Passed?TEXT("PASSED"):TEXT("FAILED"),Index,Failures);
  FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mBasicMonster> Target;
  static constexpr int32 BreakingKickIndex=8,ShoulderBashIndex=9;
 bool bBreakingKickAudit=false,bBreakingKickAuthored=false;
  bool bShoulderBashAudit=false,bShoulderBashAuthored=false;
  FString BreakingKickPoseRows,ShoulderBashPoseRows;
 TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Chain;int32 ChainStep=0,ChainUses=0;double ChainAcceptedAt=-1.,ChainNextAt=-1.;
 TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;FTimerHandle PrepareTimer;FDelegateHandle Tick,ScreenshotProcessed;FString Output,Images,Rows,CaptureRows,CaptureRunId,PendingCaptureSkill,PendingCapturePhase,PendingCapturePath;TArray<FString> CapturePaths;
 TWeakObjectPtr<UNiagaraComponent> FirstImpactComponent;bool bFirstImpactObserved=false;
 TArray<TWeakObjectPtr<UNiagaraComponent>> VfxExcluded;
 TMap<TWeakObjectPtr<UNiagaraComponent>,FVfxObservation> VfxObservations;FString VfxRows;bool bVfxTimedOut=false;
 FString MeleeVfxRows;
 FString SwordEquipAppearance,SwordEquipRows,SwordTechniquePoses,SwordTechniqueActor;
 TWeakObjectPtr<ACameraActor> SwordEquipCamera;
 FVector SwordEquipCameraOffset=FVector::ZeroVector;
 TWeakObjectPtr<UGameViewportClient> TechniqueViewport;
 double GuardBreakNearestBladeGap=TNumericLimits<double>::Max();
 bool bPreviousWorldRenderingDisabled=false;
 bool bSwordTechniqueAudit=false,bSwordTechniqueRender=false,bSwordGuardBreakAudit=false;
 bool bCrushingCapture=false,bCrushingEdge=false,bCrushingStock=false;
 bool bCrushingLoadRequested=false,bCrushingLoadCaptured=false;
 bool bEarthSplitterAudit=false,bEarthSplitterRender=false,bEarthSplitterMoving=false,bEarthSplitterWall=false;
 bool bEarthHeldCaptureRequested=false,bEarthBeganStrafe=false,bEarthEndedStrafe=false;
 TWeakObjectPtr<AActor> EarthWall;
 FRotator EarthInitialLook=FRotator::ZeroRotator;
 FString EarthSplitterAppearance;
 FVector EarthInitialActor=FVector::ZeroVector;
 FString EarthSplitterRows;
 int32 SwordEquipGrid=INDEX_NONE;
 FGuid SwordEquipItem;
 bool bSwordEquipAudit=false,bSwordEquipWalking=false,bSwordEquipMovingBoth=false;
 TWeakObjectPtr<URogue10mAttackSkillData> BraceSkill,BraceSpecial;
 EBraceStep BraceStep=EBraceStep::Settle;
 IConsoleVariable* BraceCVar=nullptr;
 FDelegateHandle BraceHealthDelegate;
 FBracePose BraceReady,BraceBeforeRelease;
 TArray<FBraceHoldSample> BraceStockHold;
 FVector2D BraceFeetMax=FVector2D::ZeroVector;
 FString BraceRows;
 int32 BraceOriginalCVar=1,BraceStartUses=0,BraceStartSpecialUses=0,BraceDamageCount=0,BraceReleaseSamples=0,BraceHoldSamples=0;
 float BraceDamagePosition=-1.f,BraceStockMontage=-1.f,BraceReadyHealth=0.f,BraceSourceRate=1.f,BraceBodyRate=1.f;
 double BraceStepStarted=0.,BraceStockLeft=0.,BraceStockRight=0.,BraceStockBlade=0.;
 double BraceRecoveryStartedAt=-1.;
 double BraceAxialDriftMax=0.,BraceAddedBladeMax=0.,BraceWristMax=0.;
 bool bCapstoneBraceRun=false,bCapstoneBraceRender=false,bCapstoneReleaseAudit=false,bBracePeakCaptured=false,bBraceCVarOwned=false,bBracePoseFrozen=false,
  BraceFirstReleaseSampled=false,BracePitchStarting=false;
 bool bCapstoneReleaseRender=false,bBraceReleaseHoldShot=false,bBraceReleaseMotionShot=false,bBraceReleaseRecoveryShot=false;
 bool bMeleeVfxRun=false,bMeleeVfxLogicOnly=false,bSawMeleeTrail=false,bSawMeleeRange=false,bSawMeleeImpact=false,bMotionRequested=false,bMotionCaptured=false;
 int32 CasePresentationTier=INDEX_NONE,FirstTrailMeshFrame=INDEX_NONE,TrailTrianglesThisFrame=0;
 ERogue10mAttackInputSlot CaseSlot=ERogue10mAttackInputSlot::Primary;
 int32 Index=0,Frame=0,Failures=0,Uses=0,PulseCount=0,MontageSamples=0,CompletedCaptures=0;double CaseStartedAt=0,Pressed=0,Accepted=-1,Expected=0,PreviousHealth=0,OldDelta=1./60.,CaptureRequestedAt=0,RenderedHandMaxTravelCm=0;
 FVector ReadyRightHandLocal=FVector::ZeroVector,ReadyLeftHandLocal=FVector::ZeroVector;
 int32 AuthoredSwordValidated=0,SwordFirstDamageChecks=0;bool bRequireAuthoredSword=false;
 bool bFinished=false,bOwnFixed=false,OldFixed=false,AttackSent=false,ChargeReleased=false,bCapturePending=false,bFinishRequested=false,bReadyRequested=false,bReadyCaptured=false,bFirstDamageCaptured=false,bFirstDamageRequested=false,bAcceptanceRequested=false,bImpactResidualRequested=false,bRecoveryRequested=false,bRecoveryCaptured=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts()){if(auto* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestWeaponThirdPerson"),TEXT("Third-person real sword/knuckle input, attachment, montage and actual damage evidence."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
