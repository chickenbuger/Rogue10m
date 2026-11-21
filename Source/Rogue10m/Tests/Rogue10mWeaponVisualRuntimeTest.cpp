#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "AssetCompilingManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
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
#include "Rogue10mMeleeVfxComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
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

namespace Rogue10mWeaponRuntime
{
class FRun : public TSharedFromThis<FRun>
{
public:
 void Start(UWorld* W)
 {
  World=W; FString Label=TEXT("Weapon3P");FParse::Value(FCommandLine::Get(),TEXT("RogueWeaponLabel="),Label);
  bMeleeVfxRun=FParse::Param(FCommandLine::Get(),TEXT("RogueMeleeVfxRun"));
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
  IFileManager::Get().MakeDirectory(*Output,true);IFileManager::Get().MakeDirectory(*Images,true);
  CaptureRunId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
  bRequireAuthoredSword=FParse::Param(FCommandLine::Get(),TEXT("RogueRequireAuthoredSword"));
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P MODE require_authored_sword=%d criterion=first_HP_drop_world_post_actor_tick_blade_capsule_surface_gap_le_10cm abstract_capsule_timing_proxy_only=1"),bRequireAuthoredSword?1:0);
  Rows=TEXT("frame,world_s,skill,phase,montage_s,health,damage,elapsed,expected_delay,blade_target_cm,hand_r_x,hand_r_y,hand_r_z,hand_l_x,hand_l_y,hand_l_z,blade_base_x,blade_base_y,blade_base_z,blade_tip_x,blade_tip_y,blade_tip_z,min_foot_world_z,capsule_bottom_world_z,foot_above_capsule_bottom_cm,target_center_x,target_center_y,target_center_z,target_radius_cm,target_halfheight_cm,target_up_x,target_up_y,target_up_z,sword_capsule_gap_cm,montage_source_s,montage_source_path\n");
  CaptureRows=TEXT("event,skill,phase,fixture_frame,world_s,montage_s,file,bytes\n");
  VfxRows=TEXT("event,skill,kind,fixture_frame,world_s,world_path,component_path,asset_path,attach_socket,first_seen_frame,observed_frame_delta,controller_valid,actual_sim_age_s,age_source,active,visible,registered,complete,eligible,location_x,location_y,location_z,scale_x,scale_y,scale_z,bounds_origin_x,bounds_origin_y,bounds_origin_z,bounds_extent_x,bounds_extent_y,bounds_extent_z,custom_time_dilation,damage_pulses_observed,first_observed_damage_pulse,pinned_first_impact,attach_parent_path,expected_3p_scale,third_person_parent_valid,third_person_scale_valid\n");
  MeleeVfxRows=TEXT("case,skill,frame,world_s,observed_damage,range_count,impact_count,trail_samples,trail_triangles,range_triangles,impact_triangles,tier,outline_vertices,outline_diameter_cm,capsule_surface_gap_cm\n");
  if(bMeleeVfxRun)
  {
   FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){Self->Prepare();return false;}),4.f);
  }
  else{W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);}
 }
 bool Finished()const{return bFinished;}
private:
 void Check(bool B,const TCHAR* Reason){if(!B){++Failures;UE_LOG(LogRogue10m,Error,TEXT("WEAPON3P FAIL skill=%d frame=%d %s"),Index,Frame,*FString(Reason));}}
 bool IsSwordCase() const {return Index<5||(Index>=10&&Index<12);}
 bool IsMeleeMissCase() const {return bMeleeVfxRun&&(Index==11||Index==13);}
 bool IsMeleeMotionCase() const {return bMeleeVfxRun&&(Index==0||Index==5||Index==10||Index==12);}
 float CaptureMontagePosition() const
 {
  const auto* Source=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;
  const auto* Anim=Source?Source->GetAnimInstance():nullptr;
  const auto* Skill=Skills.IsValidIndex(Index)?Skills[Index].Get():nullptr;
  return Anim&&Skill?Anim->Montage_GetPosition(Skill->AttackMontage):-1.f;
 }
 bool Capture(const TCHAR* Phase)
 {
  if(bCapturePending||FScreenshotRequest::IsScreenshotRequested()){Check(false,TEXT("screenshot_request_already_pending"));return false;}
  PendingCaptureSkill=Skills.IsValidIndex(Index)&&Skills[Index].IsValid()?Skills[Index]->GetName():TEXT("setup");
  if(IsMeleeMissCase()){PendingCaptureSkill+=TEXT("_Outside");}
  PendingCapturePhase=Phase;
  // A unique file cannot be mistaken for a successful capture left by an earlier run.
  PendingCapturePath=FPaths::ConvertRelativePathToFull(Images/(PendingCaptureSkill+TEXT("_")+Phase+TEXT("_")+CaptureRunId+TEXT(".png")));
  if(IFileManager::Get().FileExists(*PendingCapturePath)){Check(false,TEXT("screenshot_file_not_unique"));return false;}
  bCapturePending=true;CaptureRequestedAt=FPlatformTime::Seconds();CapturePaths.Add(PendingCapturePath);
  CaptureRows+=FString::Printf(TEXT("request,%s,%s,%d,%.6f,%.6f,%s,-1\n"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
  UE_LOG(LogRogue10m,Display,TEXT("WEAPON3P CAPTURE_REQUEST skill=%s phase=%s frame=%d world=%.6f montage=%.6f file=%s"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
  FScreenshotRequest::RequestScreenshot(PendingCapturePath,true,false);
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
  if(PendingCapturePhase==TEXT("ready")){bReadyCaptured=bSaved;}
  else if(PendingCapturePhase==TEXT("after_first_damage")||PendingCapturePhase==TEXT("contact_miss")){bFirstDamageCaptured=bSaved;}
  else if(PendingCapturePhase==TEXT("motion")){bMotionCaptured=bSaved;}
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
 bool PrepareMeleeFixture()
 {
  // StartHub uses the normal character lobby. Detach this one isolated profile after possession.
  auto* Instance=World->GetGameInstance();
  auto* Profiles=Instance?Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
  auto* Saves=Instance?Instance->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  FRogue10mCharacterProfile Profile;
  if(!Controller.IsValid()||!Profiles||!Saves||Profiles->GetProfileCount()!=0||
   !Profiles->CreateProfile(TEXT("MeleeVfxQA"),FRogue10mCharacterAppearance(),Profile)||!Profiles->SelectProfile(Profile.ProfileId)){return false;}
  Controller->EnterSelectedCharacter();
  Controller->CloseAllBlockingPanels();
  Character=Cast<ARogue10mCharacter>(Controller->GetPawn());
  if(!Character.IsValid()||!Saves->IsAttachedTo(*Character)){return false;}
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
  if(bMeleeVfxRun){Target->Tags.AddUnique(TEXT("TrainingTarget"));}
  Target->ClearAITarget();if(Target->GetController()){Target->GetController()->UnPossess();}Target->GetCharacterMovement()->DisableMovement();
  if(auto* R=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()){R->ConfigureRegeneration(0,0,0);}
  Target->GetRogueAttributeSet()->SetMaxHealth(100000);Target->GetRogueAttributeSet()->SetHealth(100000);
  Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
  for(const TCHAR* Weapon:{TEXT("GreatSword"),TEXT("Knuckle")})for(const TCHAR* Variant:{TEXT("Primary01"),TEXT("Primary02"),TEXT("Primary03"),TEXT("Special"),TEXT("Charged")})
  {
   const FString Path=FString::Printf(TEXT("/Game/DataAsset/AttackSkill/ClassCombat/%s/DA_Attack_%s_%s"),Weapon,Weapon,Variant);
   auto* S=LoadObject<URogue10mAttackSkillData>(nullptr,*Path);if(!S||!S->AttackMontage){Check(false,TEXT("skill_missing"));Finish();return;}Skills.Add(S);
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
  OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);bOwnFixed=true;
  ScreenshotProcessed=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->CaptureProcessed();});
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
  Next();
 }
 void Next()
 {
  if(Index>=Skills.Num()){Finish();return;}
  auto* C=Character->GetCombatComponent();C->CancelBoundSkillInput();C->CancelCombatVisuals();
  Character->RestoreEquippedWeaponType(IsSwordCase()?ERogue10mWeaponType::GreatSword:ERogue10mWeaponType::Knuckle);
  const UClass* BodyClass=Character->GetMesh()->GetAnimClass();
  const bool SwordRetarget=BodyClass&&BodyClass->GetPathName().Contains(TEXT("/GreatSword20261110/ABP_Human_GreatSword"));
  Check(IsSwordCase()?SwordRetarget:!SwordRetarget,TEXT("weapon_specific_body_retarget_selection_failed"));
  // Range starts at the production body combat origin, never the camera behind the player.
  const FTransform CombatView=Character->GetCombatViewTransform();
  FVector Forward=CombatView.GetRotation().GetForwardVector();Forward.Z=0;Forward.Normalize();
  auto* Skill=Skills[Index].Get();
  const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
  FVector TargetOffset=Forward*110.;
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
  Check(Basic==Skills[IsSwordCase()?0:5].Get(),TEXT("production_primary_binding_changed"));
  CaseSlot=ERogue10mAttackInputSlot::Primary;Chain.Reset();ChainStep=0;ChainAcceptedAt=-1.;ChainNextAt=-1.;
  if(Skill!=Basic)
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
  bReadyRequested=bReadyCaptured=bFirstDamageCaptured=bFirstDamageRequested=bAcceptanceRequested=bImpactResidualRequested=bRecoveryRequested=bRecoveryCaptured=false;RenderedHandMaxTravelCm=0;
  VfxExcluded.Reset();VfxObservations.Reset();FirstImpactComponent.Reset();bFirstImpactObserved=false;bVfxTimedOut=false;
  bSawMeleeTrail=bSawMeleeRange=bSawMeleeImpact=bMotionRequested=bMotionCaptured=false;
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
 void Step()
 {
  if(bFinished){return;}
  if(bCapturePending&&FPlatformTime::Seconds()-CaptureRequestedAt>30.)
  {
   Check(false,TEXT("screenshot_processing_timeout"));
   CaptureRows+=FString::Printf(TEXT("timeout,%s,%s,%d,%.6f,%.6f,%s,-1\n"),*PendingCaptureSkill,*PendingCapturePhase,Frame,World.IsValid()?World->GetTimeSeconds():-1.,CaptureMontagePosition(),*PendingCapturePath);
   FScreenshotRequest::Reset();bCapturePending=false;bFinishRequested=true;
  }
  if(bFinishRequested){Finish();return;}
  ++Frame; if(Frame>12000){Check(false,TEXT("fixture_timeout"));Finish();return;}
  if(!Character.IsValid()||!Target.IsValid()||!Controller.IsValid()){Check(false,TEXT("fixture_lost"));Finish();return;}
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
  if(!AttackSent&&Age>=1.)
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
   BaselineVfx();C->HandleEquippedSkillPressed(CaseSlot);AttackSent=true;Pressed=Now;
  }
  if(AttackSent&&Charged&&!ChargeReleased&&Now-Pressed>Skill->ChargeSeconds+.05){BaselineVfx();C->HandleEquippedSkillReleased(CaseSlot);ChargeReleased=true;}
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
  const double Health=Target->GetRogueAttributeSet()->GetHealth(),Drop=PreviousHealth-Health;
  if(Drop>.01)
  {
   // This deferred capture is requested after health changes, not an exact contact-frame image.
   ++PulseCount;if(PulseCount==1){Check(Accepted>=0,TEXT("damage_before_acceptance"));Check(FMath::Abs((Now-Accepted)-Expected)<.05,TEXT("actual_damage_delay_mismatch"));}
   if(PulseCount==1)
   {
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
  const bool bMeleeContact=bMeleeVfxRun&&Accepted>=0&&ObserveMeleeVfx(*Skill,Now,Drop);
  if((bMeleeVfxRun?bMeleeContact:PulseCount>0)&&!bFirstDamageRequested&&!bCapturePending)
  {
   bFirstDamageRequested=Capture(IsMeleeMissCase()?TEXT("contact_miss"):TEXT("after_first_damage"));
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
  if(Accepted>=0&&!Playing&&Now-Accepted>Expected+.7&&!C->IsWeaponChangeBlocked())
  {
   if(bCapturePending){return;}
   if(IsVfxCase()&&!bVfxTimedOut&&(!bAcceptanceRequested||!bImpactResidualRequested)){return;}
   const int32 Wanted=IsMeleeMissCase()?0:Skill->HitMode==ERogue10mAttackHitMode::Single?1:Skill->HitCount;
   Check(PulseCount==Wanted,TEXT("actual_damage_count_mismatch"));Check(MontageSamples>5,TEXT("montage_instance_not_playing_enough_samples"));
   Check(RenderedHandMaxTravelCm>2.,TEXT("rendered_body_hand_motion_missing"));Check(bFirstDamageCaptured,TEXT("after_first_damage_capture_not_completed"));
   if(bMeleeVfxRun){CheckMeleeVfxRecovery();}
   bRecoveryRequested=true;if(!Capture(TEXT("recovery"))){Finish();return;}
  }
  else if(Accepted<0&&AttackSent&&Now-Pressed>16.){Check(false,TEXT("attack_not_accepted"));++Index;Next();}
 }
 void Finish()
 {
  if(bFinished)return;
  if(bCapturePending){bFinishRequested=true;return;}
  bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessed);
  for(const FString& Path:CapturePaths){Check(IFileManager::Get().FileSize(*Path)>0,TEXT("final_screenshot_file_missing_or_empty"));}
  const int32 ExpectedCases=bMeleeVfxRun?14:10,ExpectedCaptures=bMeleeVfxRun?46:34;
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
   TEXT("A capsule measurement is a collision proxy and visual quality requires inspecting the rendered PNGs.\n"),
   Frame,Failures,Index,CapturePaths.Num(),CompletedCaptures,bRequireAuthoredSword?1:0,AuthoredSwordValidated,SwordFirstDamageChecks);
  Check(FFileHelper::SaveStringToFile(bMeleeVfxRun?MeleeSummary:Summary,*(Output/TEXT("metrics.txt"))),TEXT("summary_evidence_save_failed"));
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  const bool Passed=Failures==0&&Index==ExpectedCases;
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=%s_%s completed=%d failures=%d"),bMeleeVfxRun?TEXT("MELEE_VFX"):TEXT("WEAPON_THIRDPERSON"),
   Passed?TEXT("PASSED"):TEXT("FAILED"),Index,Failures);
  FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mBasicMonster> Target;
 TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Chain;int32 ChainStep=0,ChainUses=0;double ChainAcceptedAt=-1.,ChainNextAt=-1.;
 TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;FTimerHandle PrepareTimer;FDelegateHandle Tick,ScreenshotProcessed;FString Output,Images,Rows,CaptureRows,CaptureRunId,PendingCaptureSkill,PendingCapturePhase,PendingCapturePath;TArray<FString> CapturePaths;
 TWeakObjectPtr<UNiagaraComponent> FirstImpactComponent;bool bFirstImpactObserved=false;
 TArray<TWeakObjectPtr<UNiagaraComponent>> VfxExcluded;
 TMap<TWeakObjectPtr<UNiagaraComponent>,FVfxObservation> VfxObservations;FString VfxRows;bool bVfxTimedOut=false;
 FString MeleeVfxRows;
 bool bMeleeVfxRun=false,bSawMeleeTrail=false,bSawMeleeRange=false,bSawMeleeImpact=false,bMotionRequested=false,bMotionCaptured=false;
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
