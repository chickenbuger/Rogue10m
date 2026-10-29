// Isolated, opt-in QA. Programmatic real input and viewport capture, never physical-input/full-run QA.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
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
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerInputDebugging.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace Rogue10mLookDashQA
{
static FString Editable(const UObject& Object)
{
 FString S; for (TFieldIterator<FProperty> It(Object.GetClass());It;++It) { if (!It->HasAnyPropertyFlags(CPF_Edit)) continue;
 FString V;It->ExportTextItem_Direct(V,It->ContainerPtrToValuePtr<void>(&Object),nullptr,nullptr,PPF_None);S+=It->GetName()+TEXT("=")+V+TEXT("\n"); }return S;
}
static float FloatField(const UObject& Object,const TCHAR* Name,float Fallback)
{const auto* P=FindFProperty<FFloatProperty>(Object.GetClass(),Name);return P?P->GetPropertyValue_InContainer(&Object):Fallback;}
class FRun:public TSharedFromThis<FRun>
{
public:
 bool Finished()const{return bFinished;}
 void Start(UWorld& W)
 {
  World=&W;const TCHAR* Args=FCommandLine::Get();FString Town,Prefix,Keys;
  FParse::Value(Args,TEXT("RogueLookDashLabel="),Label);FParse::Value(Args,TEXT("RogueLookDashProfile="),Profile);
  FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Good=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label){Good&=(C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_');}
  TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  if(!Good||W.GetNetMode()!=NM_Standalone||(Profile!=TEXT("Original")&&Profile!=TEXT("Weighted"))||Town!=TEXT("Rogue10m_Town_QA_LookDash_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_LookDash_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_LookDash_")+Label||UGameplayStatics::DoesSaveGameExist(Town,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty())
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=LOOK_DASH_FAILED reason=exact_fresh_QA_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/look-dash-20261029")/Label;Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261029_LookDash_")+Label);
  bRendered=!FParse::Param(Args,TEXT("nullrhi"));bOffscreen=FParse::Param(Args,TEXT("RenderOffScreen"));bWeighted=Profile==TEXT("Weighted");
  if(IFileManager::Get().DirectoryExists(*Output)||IFileManager::Get().DirectoryExists(*Images)){UE_LOG(LogRogue10m,Error,TEXT("RESULT=LOOK_DASH_FAILED reason=fresh_evidence_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  IFileManager::Get().MakeDirectory(*Output,true);if(bRendered)IFileManager::Get().MakeDirectory(*Images,true);
  Frames=TEXT("frame,stage,world_s,dt,wall_dt,profile,control_pitch,control_yaw,view_pitch,fov,actor_x,actor_y,actor_z,speed_xy,velocity_z,dodging,cooldown,stamina,grounded,ui,flushes,viewport_events,camera_trace_cm,camera_trace_deg,hand_l_depth,hand_r_depth,chest_depth,pelvis_depth,foot_l_depth,foot_r_depth,head_anchor_x,head_anchor_y,head_anchor_z,eye_delta_actor_forward,eye_delta_actor_right,eye_delta_world_z\n");
  Captures=TEXT("name,event,engine_frame,world_s,dodging,pitch,yaw,stage\n");Deadline=FPlatformTime::Seconds()+200.;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){if(Self->bFinished)return false;if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("watchdog"));Self->Finish();return false;}if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto Work=MoveTemp(Self->Next);Work();}return !Self->bFinished;}),.01f);
  Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()&&!Self->bFinished){Self->Check(false,TEXT("world_cleanup_before_finish"));Self->Finish(false);}});
  Later(3.,[this]{Prepare();});
 }
private:
 struct FCapturedPixels { FString Name; int32 Width=0,Height=0; TArray<FColor> Pixels; };
 void Check(bool OK,const TCHAR* Why){++Checks;if(!OK){++Failures;Results+=FString(TEXT("FAIL "))+Why+TEXT("\n");if(Failures<25)UE_LOG(LogRogue10m,Error,TEXT("LOOK_DASH FAIL %s"),Why);}}
 void Later(double Delay,TFunction<void()> F){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(F);}
 void Key(FKey K,bool Down){if(PC.IsValid())PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
 void Stop(){Key(EKeys::W,false);Key(EKeys::LeftShift,false);Key(EKeys::E,false);if(Character.IsValid())Character->GetCharacterMovement()->StopMovementImmediately();}
 void Photo(const TCHAR* Name)
 {
  if(!bRendered)return;Check(PendingCapture.IsEmpty(),TEXT("previous_screenshot_completed_before_next_request"));++RequestedCaptures;PendingCapture=Name;Captures+=FString::Printf(TEXT("%s,request,%llu,%.9f,%d,%.6f,%.6f,%s\n"),*PendingCapture,GFrameCounter,World->GetTimeSeconds(),Character->IsDodging(),PC->GetControlRotation().Pitch,PC->GetControlRotation().Yaw,*Stage);
  FScreenshotRequest::RequestScreenshot(Images/Name,true,false);
 }
 bool Aim(float DeltaPitch)
 {
  UFunction* Fn=Character->FindFunction(TEXT("DoAim"));int32 N=0;bool OK=Fn&&Fn->ParmsSize==sizeof(float)*2;
  if(Fn)for(TFieldIterator<FProperty> It(Fn);It&&It->HasAnyPropertyFlags(CPF_Parm);++It){OK&=CastField<FFloatProperty>(*It)!=nullptr;++N;}
  Check(OK&&N==2,TEXT("real_DoAim_reflection_has_exact_two_float_parameters"));if(!OK||N!=2)return false;
  struct{float Yaw;float Pitch;} P{0.f,DeltaPitch};Character->ProcessEvent(Fn,&P);return true;
 }
 void SetLook(float Pitch,TFunction<void()> Done)
 {
  PC->SetControlRotation(FRotator(0,InitialControl.Yaw,0));Aim(Pitch/PitchPerInput);Stage=TEXT("look_settle");
  Later(.25,[this,Pitch,Done=MoveTemp(Done)]()mutable{const float Actual=FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);Check(FMath::Abs(Actual-Pitch)<.04f,TEXT("actual_DoAim_and_CameraManager_reach_requested_production_pitch"));WantedPitch=Pitch;if(Failures){Finish();return;}Done();});
 }
 void Prepare()
 {
  PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;auto* GI=World->GetGameInstance();auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  Check(PC.IsValid()&&Profiles&&Saves.IsValid(),TEXT("actual_profile_controller_progression_available"));if(Failures){Finish();return;}
  FRogue10mCharacterProfile P;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("LookDashQa"),FRogue10mCharacterAppearance(),P)&&Profiles->SelectProfile(P.ProfileId),TEXT("fresh_profile_created_selected"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());Check(Character.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_attaches_actual_pawn"));if(Failures){Finish();return;}
  Saves->DetachCharacter(*Character,false);Check(!Saves->IsAttachedTo(*Character),TEXT("autosave_detached_before_transient_fixture"));
  Appearance=Character->GetAppearanceCameraComponent();Body=Character->GetMesh();Source=Character->GetAnimationPlaybackMesh();Camera=Character->GetFirstPersonCameraComponent();Combat=Character->GetCombatComponent();
  Check(Appearance.IsValid()&&Body.IsValid()&&Source.IsValid()&&Camera.IsValid()&&Combat.IsValid()&&Character->GetRogueAttributeSet()&&PC->PlayerCameraManager,TEXT("camera_body_source_combat_attributes_available"));if(Failures){Finish();return;}
  Dodge=Combat->GetActiveDodgeSkill();DodgeConfig=Dodge.IsValid()?Editable(*Dodge):TEXT("actual_character_fallback");
  SavedEyeOffset=Appearance->EyeOffset;float EyeForward=0.f;
  if(FParse::Value(FCommandLine::Get(),TEXT("RogueLookDashEyeForward="),EyeForward))
  {
   Check(FMath::IsFinite(EyeForward)&&EyeForward>0.f&&EyeForward<=48.f,TEXT("explicit_QA_eye_forward_calibration_within_48cm"));if(Failures){Finish();return;}
   Appearance->EyeOffset.X=EyeForward;bOwnEyeOffset=true;
   Results+=FString::Printf(TEXT("QA_eye_forward_override_cm=%.6f\nauthored_eye_offset_before_QA=%s\n"),EyeForward,*SavedEyeOffset.ToString());
  }
  AppearanceConfig=Editable(*Appearance);InitialTransform=Character->GetActorTransform();InitialControl=PC->GetControlRotation();bInitialDamage=Character->CanBeDamaged();bPrepared=true;Character->SetCanBeDamaged(false);
  Duration=Dodge.IsValid()?Dodge->Duration:FloatField(*Character,TEXT("DodgeDuration"),.16f);Cooldown=Dodge.IsValid()?Dodge->Cooldown:FloatField(*Character,TEXT("DodgeCooldown"),.45f);Distance=Dodge.IsValid()?Dodge->Distance:FloatField(*Character,TEXT("DodgeDistance"),100.f);const float BaseCost=Dodge.IsValid()?Dodge->StaminaCost:0.f;Cost=Character->GetTraitRuntimeComponent()?Character->GetTraitRuntimeComponent()->GetDodgeStaminaCost(BaseCost):BaseCost;
  Check(FMath::IsFinite(Duration)&&Duration>=.05f&&FMath::IsFinite(Cooldown)&&Cooldown>=0.f&&FMath::IsFinite(Distance)&&Distance>0.f&&FMath::IsFinite(Cost)&&Cost>=0.f,TEXT("actual_authored_or_fallback_dodge_configuration_is_valid"));if(Failures){Finish();return;}
  Results+=FString::Printf(TEXT("dodge_asset=%s\nconfigured_distance_cm=%.6f\nconfigured_duration_s=%.6f\nconfigured_cooldown_s=%.6f\neffective_stamina_cost=%.6f\nauthored_eye_offset=%s\n"),Dodge.IsValid()?*Dodge->GetPathName():TEXT("Character_fallback"),Distance,Duration,Cooldown,Cost,*Appearance->EyeOffset.ToString());
  Heavy=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy"));Motion=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyMotionScale"));Fov=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyFOV"));Check(Heavy&&Motion&&Fov,TEXT("presentation_controls_registered"));if(Failures){Finish();return;}
  OldHeavy=Heavy->GetInt();OldMotion=Motion->GetFloat();OldFov=Fov->GetFloat();bOwnCVars=true;PC->ToggleSettings();Check(PC->RestoreOriginalFirstPersonPresentation(),TEXT("restore_original_profile_via_settings_API"));if(bWeighted)Check(PC->SetWeightedFirstPersonEnabled(true)&&PC->SetWeightedFirstPersonMotionScale(1.f),TEXT("enable_weighted_via_real_settings_API"));PC->CloseAllBlockingPanels();
  Viewport=World->GetGameViewport();Check(Viewport.IsValid(),TEXT("owned_viewport_available"));
#if UE_PLAYER_INPUT_INCLUDE_DEBUG
  FlushHandle=UE::Input::FPlayerInputDebugging::OnPlayerInputFlushed.AddLambda([Self=AsShared()](const UPlayerInput* Input){if(Self->PC.IsValid()&&Input==Self->PC->PlayerInput){++Self->Flushes;if(Self->bMeasured)++Self->MeasuredFlushes;}});
  bFlushHook=true;
#endif
  Check(bFlushHook,TEXT("official_flush_integrity_hook_available"));
  InputHandle=Viewport->OnInputKey().AddLambda([Self=AsShared()](const FInputKeyEventArgs&){++Self->ViewportEvents;if(Self->bMeasured)++Self->MeasuredViewportEvents;});
  CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 Width,int32 Height,const TArray<FColor>& Pixels){if(Self->bFinished||Self->PendingCapture.IsEmpty())return;Self->Captures+=FString::Printf(TEXT("%s,captured,%llu,%.9f,%d,%.6f,%.6f,%s\n"),*Self->PendingCapture,GFrameCounter,Self->World->GetTimeSeconds(),Self->Character->IsDodging(),Self->PC->GetControlRotation().Pitch,Self->PC->GetControlRotation().Yaw,*Self->Stage);++Self->CompletedCaptures;if(Self->PendingCapture==TEXT("06_level_E_dodge.png")||Self->PendingCapture==TEXT("07_downlook_E_dodge.png"))Self->Check(Self->Character->IsDodging(),TEXT("dodge_screenshot_captured_during_actual_active_dodge"));auto& Capture=Self->CapturedImages.AddDefaulted_GetRef();Capture.Name=Self->PendingCapture;Capture.Width=Width;Capture.Height=Height;Capture.Pixels=Pixels;Self->PendingCapture.Empty();});
  SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* X,ELevelTick,float DT){if(X==Self->World.Get())Self->Sample(DT);});
  GroundDeadline=FPlatformTime::Seconds()+15.;Later(.7,[this]{WaitInitialGround();});
 }
 void WaitInitialGround()
 {
  if(!Character->CanChangeWeapon()&&FPlatformTime::Seconds()<GroundDeadline){Later(.1,[this]{WaitInitialGround();});return;}
  Check(Character->CanChangeWeapon(),TEXT("actual_grounded_idle_ready"));if(Failures){Finish();return;}
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);
  auto* Box=Ground.IsValid()?NewObject<UBoxComponent>(Ground.Get(),TEXT("LookDashGround"),RF_Transient):nullptr;Check(Box!=nullptr,TEXT("transient_floor_created"));if(Failures){Finish();return;}
  Ground->AddInstanceComponent(Box);Ground->SetRootComponent(Box);Box->SetBoxExtent(FVector(10000,10000,20));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
  Surface=Character->GetActorLocation().Z+1000.;Ground->SetActorLocation(FVector(InitialTransform.GetLocation().X,InitialTransform.GetLocation().Y,Surface-20.));
  Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));Check(Cube.IsValid(),TEXT("existing_engine_cube_for_visible_QA_floor"));if(Failures){Finish();return;}
  auto* Visible=NewObject<UStaticMeshComponent>(Ground.Get(),TEXT("VisibleFloor"),RF_Transient);Ground->AddInstanceComponent(Visible);Visible->SetupAttachment(Box);Visible->SetStaticMesh(Cube.Get());Visible->SetRelativeScale3D(FVector(200,200,.4f));Visible->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visible->SetCanEverAffectNavigation(false);Visible->RegisterComponent();
  Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorLocation(FVector(InitialTransform.GetLocation().X,InitialTransform.GetLocation().Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.),false,nullptr,ETeleportType::TeleportPhysics);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Character->GetCharacterMovement()->bForceNextFloorCheck=true;
  Later(.5,[this]{Check(Character->GetCharacterMovement()->IsMovingOnGround()&&Character->GetMovementBaseObject()==Ground->GetRootComponent(),TEXT("pawn_landed_on_owned_collision_floor"));if(Failures){Finish();return;}Fixture=Character->GetActorTransform();CalibrateAim();});
 }
 void CalibrateAim()
 {
  Stage=TEXT("calibrate_actual_look_input");PC->SetControlRotation(FRotator(0,InitialControl.Yaw,0));Aim(1.f);
  Later(.1,[this]{PitchPerInput=FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);Check(FMath::IsFinite(PitchPerInput)&&FMath::Abs(PitchPerInput)>.01f&&FMath::Abs(PitchPerInput)<20.f,TEXT("actual_DoAim_pitch_input_has_finite_nonzero_effect"));if(Failures){Finish();return;}
   Results+=FString::Printf(TEXT("actual_pitch_delta_per_DoAim_unit=%.9f\nproduction_pitch_min=%.6f\n"),PitchPerInput,PC->PlayerCameraManager->ViewPitchMin);
   PC->SetControlRotation(FRotator(0,InitialControl.Yaw,0));Aim(-120.f/PitchPerInput);Later(.15,[this]{Check(FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch)+85.f)<.04f,TEXT("real_CameraManager_clamps_beyond_downward_request_at_minus85"));if(Failures){Finish();return;}LookCase(0);});});
 }
 void LookCase(int32 Index)
 {
  static const float Pitches[]={0.f,-60.f,-80.f,-85.f};static const TCHAR* Photos[]={TEXT("01_neutral.png"),TEXT("02_look_down60.png"),TEXT("03_look_down80.png"),TEXT("04_look_down85.png")};
  SetLook(Pitches[Index],[this,Index]{Stage=FString::Printf(TEXT("look_%d"),Index);ValidateView();Photo(Photos[Index]);Later(.85,[this,Index]{if(Index<3)LookCase(Index+1);else SprintDown();});});
 }
 void ResetFixture(){Stop();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorTransform(Fixture,false,nullptr,ETeleportType::TeleportPhysics);Character->GetRogueAttributeSet()->RestoreVitals();}
 void SprintDown()
 {
  ResetFixture();SetLook(-80.f,[this]{Stage=TEXT("downlook_walk");MoveOrigin=Character->GetActorLocation();bMeasured=true;Key(EKeys::W,true);Later(.5,[this]{Check(FVector::Dist2D(MoveOrigin,Character->GetActorLocation())>5.f,TEXT("actual_W_walks_while_looking_down"));Key(EKeys::LeftShift,true);Stage=TEXT("downlook_sprint");Later(.4,[this]{Check(Character->IsSprinting()&&Character->GetVelocity().Size2D()>100.f,TEXT("actual_shift_sprints_while_looking_down"));ValidateView();Photo(TEXT("05_downlook_sprint.png"));Later(.85,[this]{bMeasured=false;Stop();Later(.5,[this]{DodgeCase(0);});});});});});
 }
 void WaitReady(TFunction<void()> Done)
 {
  if((Character->IsDodging()||Character->GetDodgeCooldownRemaining()>.001f||!Character->CanChangeWeapon())&&FPlatformTime::Seconds()<GroundDeadline){Later(.05,[this,Done=MoveTemp(Done)]()mutable{WaitReady(MoveTemp(Done));});return;}
  Check(!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f&&Character->CanChangeWeapon(),TEXT("dodge_case_waits_for_real_recovery_and_cooldown"));if(Failures){Finish();return;}Done();
 }
 void MakeWall()
 {
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Wall=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);auto* Box=Wall.IsValid()?NewObject<UBoxComponent>(Wall.Get(),TEXT("LookDashWall"),RF_Transient):nullptr;Check(Box!=nullptr,TEXT("owned_collision_wall_created"));if(!Box)return;
  Wall->AddInstanceComponent(Box);Wall->SetRootComponent(Box);Box->SetBoxExtent(FVector(10,500,500));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
  Forward=Character->GetActorForwardVector().GetSafeNormal2D();Wall->SetActorRotation(Forward.Rotation());Wall->SetActorLocation(Character->GetActorLocation()+Forward*(Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+8.f+10.f));
  auto* Visual=NewObject<UStaticMeshComponent>(Wall.Get(),TEXT("VisibleWall"),RF_Transient);Wall->AddInstanceComponent(Visual);Visual->SetupAttachment(Box);Visual->SetStaticMesh(Cube.Get());Visual->SetRelativeScale3D(FVector(.2f,10,10));Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->RegisterComponent();
 }
 void DodgeCase(int32 Index)
 {
  GroundDeadline=FPlatformTime::Seconds()+12.;WaitReady([this,Index]{ResetFixture();SetLook(Index==0?0.f:-85.f,[this,Index]{if(Index==2)MakeWall();if(Failures){Finish();return;}Stage=FString::Printf(TEXT("dodge_%d"),Index);DodgeIndex=Index;DodgeOrigin=Character->GetActorLocation();Forward=Character->GetActorForwardVector().GetSafeNormal2D();BeforeStamina=Character->GetRogueAttributeSet()->GetStamina();AcceptedAt=-1;DodgeEndObserved=-1;FirstActiveDT=0;EndObservedDT=0;MaxDodgeDT=0;DashSamples=0;bDashPhoto=false;bMeasured=true;DodgeRequestWorld=World->GetTimeSeconds();DodgeRequestWall=FPlatformTime::Seconds();Key(EKeys::E,true);
   Later(.04,[this,Index]{Key(EKeys::E,false);Check(AcceptedAt>=0.,TEXT("actual_E_key_accepts_owned_weapon_dodge"));if(Failures){Finish();return;}Later(.02,[this,Index]{if(!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f){Results+=FString::Printf(TEXT("case_%d_early_repeat_guard=not_applicable_no_active_or_cooldown_window\n"),Index);WaitDodgeEnd(Index);return;}RepeatDeadline=World->GetTimeSeconds()+Character->GetDodgeCooldownRemaining();RepeatStamina=Character->GetRogueAttributeSet()->GetStamina();Key(EKeys::E,true);Later(.03,[this,Index]{Key(EKeys::E,false);Check(Cooldown>0.f?FMath::Abs((World->GetTimeSeconds()+Character->GetDodgeCooldownRemaining())-RepeatDeadline)<.005f:Character->GetDodgeCooldownRemaining()<=.001f,TEXT("early_repeated_E_does_not_extend_cooldown"));Check(Character->GetRogueAttributeSet()->GetStamina()>=RepeatStamina-.01f,TEXT("repeat_interval_stamina_does_not_drop_in_observed_samples"));WaitDodgeEnd(Index);});});});});});
 }
 void WaitDodgeEnd(int32 Index)
 {
  if(Character->IsDodging()){if(FPlatformTime::Seconds()>DodgeRequestWall+6.||World->GetTimeSeconds()>DodgeRequestWorld+Duration+MaxDodgeDT+.3){Check(false,TEXT("active_dodge_exceeds_bounded_authored_timer_window"));Finish();return;}Later(.025,[this,Index]{WaitDodgeEnd(Index);});return;}
  const double Travel=FVector::DotProduct(Character->GetActorLocation()-DodgeOrigin,Forward);const double DZ=FMath::Abs(Character->GetActorLocation().Z-DodgeOrigin.Z);
  Check(DashSamples>0&&DZ<1.f,TEXT("dodge_has_actual_sampled_horizontal_movement_without_vertical_launch"));
  const double Elapsed=DodgeEndObserved-AcceptedAt;const double Early=FMath::Max(0.,static_cast<double>(Duration)-FirstActiveDT-.002);const double Late=Duration+EndObservedDT+.002;
  Check(DodgeEndObserved>=AcceptedAt&&Elapsed>=Early&&Elapsed<=Late,TEXT("active_dodge_ends_within_authored_duration_and_recorded_frame_quantization"));
  Check(Character->GetRogueAttributeSet()->GetStamina()>=BeforeStamina-Cost-.05f,TEXT("observed_stamina_not_below_single_effective_cost_ceiling"));
  if(Index<2){Check(Travel>2.f,TEXT("free_actual_dodge_moves_forward"));FreeTravel[Index]=Travel;}
  else
  {
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LookDashWall),false,Character.Get());const auto* C=Character->GetCapsuleComponent();World->SweepSingleByChannel(Hit,Character->GetActorLocation(),Character->GetActorLocation()+Forward*5.f,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),Q);
   Check(Hit.bBlockingHit&&Hit.GetActor()==Wall.Get(),TEXT("actual_dodge_reaches_owned_wall_contact"));Check(Travel<=8.3&&Travel>=0.&&Travel<FreeTravel[1],TEXT("capsule_stops_before_wall_surface_without_tunneling"));Photo(TEXT("08_downlook_wall_contact.png"));
  }
  Results+=FString::Printf(TEXT("case_%d_travel_cm=%.9f\ncase_%d_vertical_cm=%.9f\ncase_%d_active_samples=%d\ncase_%d_first_active_world_s=%.9f\ncase_%d_end_observed_world_s=%.9f\ncase_%d_elapsed_world_s=%.9f\ncase_%d_allowed_elapsed_min_s=%.9f\ncase_%d_allowed_elapsed_max_s=%.9f\ncase_%d_first_active_dt_s=%.9f\ncase_%d_end_observed_dt_s=%.9f\n"),Index,Travel,Index,DZ,Index,DashSamples,Index,AcceptedAt,Index,DodgeEndObserved,Index,Elapsed,Index,Early,Index,Late,Index,FirstActiveDT,Index,EndObservedDT);
  bMeasured=false;
  if(Character->GetDodgeCooldownRemaining()>.04f)
  {
   const float Remaining=Character->GetDodgeCooldownRemaining();const float Stam=Character->GetRogueAttributeSet()->GetStamina();Key(EKeys::E,true);Later(.025,[this,Index,Remaining,Stam]{Key(EKeys::E,false);Check(!Character->IsDodging(),TEXT("post_active_E_is_blocked_until_real_cooldown_expires"));Check(Character->GetDodgeCooldownRemaining()<=Remaining+.001f&&Character->GetRogueAttributeSet()->GetStamina()>=Stam-.01f,TEXT("cooldown_rejection_does_not_restart_or_charge"));AfterDodgeCase(Index);});
  }
  else{Results+=FString::Printf(TEXT("case_%d_post_active_cooldown_guard=not_applicable_no_remaining_window\n"),Index);AfterDodgeCase(Index);}
 }
 void AfterDodgeCase(int32 Index)
 {
  Later(.85,[this,Index]{if(Wall.IsValid())Wall->Destroy();if(Index<2)DodgeCase(Index+1);else BlockingBoundaries();});
 }
 void BlockingBoundaries()
 {
  GroundDeadline=FPlatformTime::Seconds()+12.;WaitReady([this]{ResetFixture();PC->ToggleSettings();const float S=Character->GetRogueAttributeSet()->GetStamina();Key(EKeys::E,true);Later(.06,[this,S]{Key(EKeys::E,false);Check(PC->IsSettingsVisible()&&!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f&&Character->GetRogueAttributeSet()->GetStamina()>=S-.01f,TEXT("settings_blocks_actual_E_without_partial_dodge_or_cost"));PC->CloseAllBlockingPanels();Later(.3,[this]{AirBoundary();});});});
 }
 void AirBoundary()
 {
  ResetFixture();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->AddActorWorldOffset(FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Check(Character->GetCharacterMovement()->IsFalling(),TEXT("air_boundary_is_actual_falling"));const float S=Character->GetRogueAttributeSet()->GetStamina();Key(EKeys::E,true);
  Later(.06,[this,S]{Key(EKeys::E,false);Check(!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f&&Character->GetRogueAttributeSet()->GetStamina()>=S-.01f,TEXT("airborne_E_is_rejected_without_dodge_or_cost"));Later(.7,[this]{Check((!Dodge.IsValid()||Editable(*Dodge)==DodgeConfig)&&Editable(*Appearance)==AppearanceConfig,TEXT("authored_dodge_and_appearance_data_unchanged"));Check(MeasuredFlushes==0&&MeasuredViewportEvents==0,TEXT("measured_movement_has_no_flush_or_external_viewport_input"));Finish();});});
 }
 void ValidateView()
 {
  FMinimalViewInfo V;Character->CalcCamera(0,V);FTransform T;const bool OK=Appearance->ReadCameraTransform(T);Check(OK&&Appearance->IsAppearanceCameraActive()&&!Appearance->IsThirdPersonInspectionActive()&&Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("requested_live_first_person_profile_and_trace_available"));
  Check(!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV)&&OK&&V.Location.Equals(T.GetLocation(),.001f)&&V.Rotation.Equals(T.Rotator(),.001f),TEXT("actual_camera_and_combat_trace_share_finite_origin_rotation"));Check(Body->IsBoneHiddenByName(Appearance->HeadBone),TEXT("appearance_head_remains_hidden_for_owner"));
 }
 double Depth(FName Bone,const FMinimalViewInfo& V)const{return Body->GetBoneIndex(Bone)!=INDEX_NONE?FVector::DotProduct(Body->GetSocketLocation(Bone)-V.Location,V.Rotation.Vector()):-99999.;}
 void Sample(float DT)
 {
  if(bFinished||!bPrepared||!Character.IsValid()||!Appearance.IsValid())return;const double Now=FPlatformTime::Seconds();const double WallDT=LastWall>0?Now-LastWall:0.;LastWall=Now;FMinimalViewInfo V;Character->CalcCamera(0,V);FTransform T;const bool Have=Appearance->ReadCameraTransform(T);const FVector P=Character->GetActorLocation();const FVector Vel=Character->GetVelocity();
  if(bMeasured)
  {
   Check(Have&&!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV),TEXT("measured_camera_is_finite_and_trace_readable"));Check(Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("profile_stays_active_during_actual_movement"));Check(PC->GetControlRotation().Equals(FRotator(WantedPitch,InitialControl.Yaw,0),.04f),TEXT("measured_control_rotation_is_not_changed_by_sprint_dodge_or_external_input"));
   if(Stage.StartsWith(TEXT("dodge_"))){MaxDodgeDT=FMath::Max(MaxDodgeDT,static_cast<double>(DT));if(Character->IsDodging()){++DashSamples;if(AcceptedAt<0.){AcceptedAt=World->GetTimeSeconds();FirstActiveDT=DT;Check(Character->GetDodgeCooldownRemaining()<=Cooldown+.001f&&(Cooldown<=0.f||Character->GetDodgeCooldownRemaining()>0.f),TEXT("accepted_dodge_starts_authored_cooldown"));if(DodgeIndex<2&&!bDashPhoto){bDashPhoto=true;Photo(DodgeIndex==0?TEXT("06_level_E_dodge.png"):TEXT("07_downlook_E_dodge.png"));}}}else if(AcceptedAt>=0.&&DodgeEndObserved<0.){DodgeEndObserved=World->GetTimeSeconds();EndObservedDT=DT;}}
  }
  FTransform Head;FQuat HeadMotion;const bool HaveHead=Appearance->ReadAnimatedHead(Head,HeadMotion);const FVector Anchor=HaveHead?Head.GetLocation():FVector::ZeroVector;const FVector EyeDelta=V.Location-Anchor;
  Frames+=FString::Printf(TEXT("%llu,%s,%.9f,%.9f,%.9f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.9f,%.6f,%d,%d,%d,%d,%.9f,%.9f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),GFrameCounter,*Stage,World->GetTimeSeconds(),DT,WallDT,*Profile,PC->GetControlRotation().Pitch,PC->GetControlRotation().Yaw,V.Rotation.Pitch,V.FOV,P.X,P.Y,P.Z,Vel.Size2D(),Vel.Z,Character->IsDodging(),Character->GetDodgeCooldownRemaining(),Character->GetRogueAttributeSet()->GetStamina(),Character->GetCharacterMovement()->IsMovingOnGround(),PC->IsAnyBlockingWindowVisible(),Flushes,ViewportEvents,Have?FVector::Dist(V.Location,T.GetLocation()):-1.,Have?FMath::RadiansToDegrees(V.Rotation.Quaternion().AngularDistance(T.GetRotation())):-1.,Depth(TEXT("hand_l"),V),Depth(TEXT("hand_r"),V),Depth(TEXT("spine_03"),V),Depth(TEXT("pelvis"),V),Depth(TEXT("foot_l"),V),Depth(TEXT("foot_r"),V),Anchor.X,Anchor.Y,Anchor.Z,FVector::DotProduct(EyeDelta,Character->GetActorForwardVector()),FVector::DotProduct(EyeDelta,Character->GetActorRightVector()),EyeDelta.Z);
 }
 void Finish(bool Exit=true)
 {
  if(bFinished)return;bFinished=true;Next=nullptr;FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
#if UE_PLAYER_INPUT_INCLUDE_DEBUG
  UE::Input::FPlayerInputDebugging::OnPlayerInputFlushed.Remove(FlushHandle);
#endif
  if(Viewport.IsValid())Viewport->OnInputKey().Remove(InputHandle);UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);Stop();
  if(bPrepared&&Character.IsValid()){if(Saves.IsValid()&&Saves->IsAttachedTo(*Character))Saves->DetachCharacter(*Character,false);PC->CloseAllBlockingPanels();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetCanBeDamaged(bInitialDamage);Character->SetActorTransform(InitialTransform,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(InitialControl);}
  if(bOwnEyeOffset&&Appearance.IsValid()){Appearance->EyeOffset=SavedEyeOffset;}
  if(Wall.IsValid())Wall->Destroy();if(Ground.IsValid())Ground->Destroy();if(bOwnCVars){Heavy->Set(OldHeavy,ECVF_SetByConsole);Motion->Set(OldMotion,ECVF_SetByConsole);Fov->Set(OldFov,ECVF_SetByConsole);}
  if(bRendered)Check(RequestedCaptures==8&&CompletedCaptures==8&&PendingCapture.IsEmpty(),TEXT("all_eight_actual_viewport_captures_completed"));
  // Keep PNG compression and disk IO out of the measured movement/dodge frames.
  for(const auto& Capture:CapturedImages){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Capture.Width,Capture.Height,Capture.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Images/Capture.Name)),TEXT("actual_viewport_pixels_saved_as_PNG"));}CapturedImages.Empty();
  Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("raw_frames_written"));Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))),TEXT("capture_metadata_written"));
  Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nprofile=%s\nrendered=%d\noffscreen=%d\ninput=programmatic_Controller_InputKey_and_reflected_DoAim\nphysical_input=not_performed\nmanual_field_completion=not_performed\nvisual_clipping=requires_actual_PNG_review_numeric_depths_are_diagnostics_only\nfixture=transient_visible_floor_and_collision_wall_existing_engine_cube_no_saved_map_edits\nstamina_assertion_scope=observed_cost_ceiling_only_regeneration_retained_exact_billing_not_claimed\nmeasured_flushes=%d\nmeasured_viewport_events=%d\nrequested_captures=%d\ncompleted_captures=%d\nNOTE=E invokes existing DoDodge; timing/cost/assets unchanged. Distance data sets initial velocity and is not asserted as realized travel. Pictures during dodge retain original duration and record request/capture frame; idle pictures hold state before case changes. Captures are not complete motion/mesh clipping certification.\n"),Checks,Failures,*Profile,bRendered,bOffscreen,MeasuredFlushes,MeasuredViewportEvents,RequestedCaptures,CompletedCaptures);
  if(!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))))++Failures;UE_LOG(LogRogue10m,Display,TEXT("RESULT=LOOK_DASH_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;TWeakObjectPtr<USkeletalMeshComponent> Body,Source;TWeakObjectPtr<UCameraComponent> Camera;TWeakObjectPtr<URogue10mCombatComponent> Combat;TWeakObjectPtr<const URogue10mDodgeSkillDataAsset> Dodge;TWeakObjectPtr<UGameViewportClient> Viewport;TWeakObjectPtr<AActor> Ground,Wall;TWeakObjectPtr<UStaticMesh> Cube;
 TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;IConsoleVariable* Heavy=nullptr;IConsoleVariable* Motion=nullptr;IConsoleVariable* Fov=nullptr;FDelegateHandle SampleHandle,Cleanup,FlushHandle,InputHandle,CaptureHandle;TFunction<void()> Next;
 TArray<FCapturedPixels> CapturedImages;FString Label,Profile,Output,Images,Stage=TEXT("prepare"),Frames,Results,Captures,PendingCapture,DodgeConfig,AppearanceConfig;FTransform InitialTransform,Fixture;FRotator InitialControl;FVector MoveOrigin,DodgeOrigin,Forward,SavedEyeOffset;
 double Deadline=0,NextAt=0,GroundDeadline=0,LastWall=0,Surface=0,AcceptedAt=-1,DodgeRequestWorld=0,DodgeRequestWall=0,RepeatDeadline=0,FreeTravel[2]{},DodgeEndObserved=-1,FirstActiveDT=0,EndObservedDT=0,MaxDodgeDT=0;float PitchPerInput=1,WantedPitch=0,Duration=.16f,Cooldown=.45f,Distance=100,Cost=0,BeforeStamina=0,RepeatStamina=0,OldMotion=1,OldFov=0;int32 Checks=0,Failures=0,OldHeavy=0,DodgeIndex=0,DashSamples=0,Flushes=0,ViewportEvents=0,MeasuredFlushes=0,MeasuredViewportEvents=0,RequestedCaptures=0,CompletedCaptures=0;bool bFinished=false,bPrepared=false,bRendered=false,bOffscreen=false,bWeighted=false,bOwnCVars=false,bOwnEyeOffset=false,bInitialDamage=true,bFlushHook=false,bMeasured=false,bDashPhoto=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=LOOK_DASH_FAILED reason=no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestLookDash"),TEXT("Exact fresh LookDash QA profiles only. Production downlook clamp, sprint, E dodge, collision and cooldown; viewport photos require visual clipping review."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
