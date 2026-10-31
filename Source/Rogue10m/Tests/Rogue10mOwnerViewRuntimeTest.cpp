// Isolated, opt-in QA. Programmatic real input and viewport capture, never physical-input/full-run QA.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
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
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mOwnerArmsComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace Rogue10mOwnerViewQA
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
  FParse::Value(Args,TEXT("RogueOwnerViewLabel="),Label);FParse::Value(Args,TEXT("RogueOwnerViewProfile="),Profile);bFramed=FParse::Param(Args,TEXT("RogueOwnerViewFramed"));
  FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Good=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label){Good&=(C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_');}
  TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  if(!Good||W.GetNetMode()!=NM_Standalone||(Profile!=TEXT("Original")&&Profile!=TEXT("Weighted"))||Town!=TEXT("Rogue10m_Town_QA_OwnerView_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_OwnerView_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_OwnerView_")+Label||UGameplayStatics::DoesSaveGameExist(Town,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty())
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_VIEW_FAILED reason=exact_fresh_QA_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/owner-view-20261029")/Label;Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261029_OwnerView_")+Label);
  bRendered=!FParse::Param(Args,TEXT("nullrhi"));bOffscreen=FParse::Param(Args,TEXT("RenderOffScreen"));bWeighted=Profile==TEXT("Weighted");
  if(IFileManager::Get().DirectoryExists(*Output)||IFileManager::Get().DirectoryExists(*Images)){UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_VIEW_FAILED reason=fresh_evidence_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  IFileManager::Get().MakeDirectory(*Output,true);if(bRendered)IFileManager::Get().MakeDirectory(*Images,true);
  Frames=TEXT("frame,stage,world_s,dt,wall_dt,profile,control_pitch,control_yaw,view_pitch,fov,actor_x,actor_y,actor_z,speed_xy,velocity_z,dodging,cooldown,stamina,grounded,ui,flushes,viewport_events,camera_trace_cm,camera_trace_deg,hand_l_depth,hand_r_depth,chest_depth,pelvis_depth,foot_l_depth,foot_r_depth,head_anchor_x,head_anchor_y,head_anchor_z,eye_delta_actor_forward,eye_delta_actor_right,eye_delta_world_z\n");
  Captures=TEXT("name,event,engine_frame,world_s,dodging,pitch,yaw,stage,attack_busy,attack_uses,montage_s,target_health,montage_active,owner_proxy_active,attack_cooldown,combo_active\n");AttackFrames=TEXT("frame,stage,world_s,dt,wall_dt,pitch,uses,action_busy,cooldown,montage_s,montage_rate,montage_weight,expected_rate,target_health\n");AnimationFrames=TEXT("frame,stage,world_s,dt,wall_dt,skill_asset,montage_asset,montage_s,hand_l_world_x,hand_l_world_y,hand_l_world_z,hand_r_world_x,hand_r_world_y,hand_r_world_z,hand_l_view_forward,hand_l_view_right,hand_l_view_up,hand_r_view_forward,hand_r_view_right,hand_r_view_up\n");PresentationFrames=TEXT("frame,world_s,stage,framing_active,proxy_available,cache_time,proxy_l_world_x,proxy_l_world_y,proxy_l_world_z,proxy_r_world_x,proxy_r_world_y,proxy_r_world_z,proxy_l_view_forward,proxy_l_view_right,proxy_l_view_up,proxy_r_view_forward,proxy_r_view_right,proxy_r_view_up\n");Deadline=FPlatformTime::Seconds()+240.;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){if(Self->bFinished)return false;if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("watchdog"));Self->Finish();return false;}if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto Work=MoveTemp(Self->Next);Work();}return !Self->bFinished;}),.01f);
  Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()&&!Self->bFinished){Self->Check(false,TEXT("world_cleanup_before_finish"));Self->Finish(false);}});
  Later(3.,[this]{Prepare();});
 }
private:
 struct FCapturedPixels { FString Name; int32 Width=0,Height=0; TArray<FColor> Pixels; };
 void Check(bool OK,const TCHAR* Why){++Checks;if(!OK){++Failures;Results+=FString(TEXT("FAIL "))+Why+TEXT("\n");if(Failures<25)UE_LOG(LogRogue10m,Error,TEXT("OWNER_VIEW FAIL %s"),Why);}}
 void Later(double Delay,TFunction<void()> F){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(F);}
 void Key(FKey K,bool Down){if(PC.IsValid())PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
 void Stop(){Key(EKeys::W,false);Key(EKeys::LeftShift,false);Key(EKeys::E,false);if(Character.IsValid())Character->GetCharacterMovement()->StopMovementImmediately();}
 float MontagePosition() const {auto* A=Source.IsValid()?Source->GetAnimInstance():nullptr;return A&&AttackSkill.IsValid()&&AttackSkill->AttackMontage?A->Montage_GetPosition(AttackSkill->AttackMontage):0.f;}
 FString CaptureRow(const FString& Name,const TCHAR* Event) const {const bool ActiveMontage=AttackSkill.IsValid()&&Source.IsValid()&&Source->GetAnimInstance()&&Source->GetAnimInstance()->Montage_IsActive(AttackSkill->AttackMontage);return FString::Printf(TEXT("%s,%s,%llu,%.9f,%d,%.6f,%.6f,%s,%d,%d,%.9f,%.6f,%d,%d,%.9f,%d\n"),*Name,Event,GFrameCounter,World->GetTimeSeconds(),Character->IsDodging(),PC->GetControlRotation().Pitch,PC->GetControlRotation().Yaw,*Stage,Combat->IsWeaponChangeBlocked(),State.IsValid()&&AttackSkill.IsValid()?State->GetSkillUseCount(AttackSkill->GetFName()):0,MontagePosition(),Target.IsValid()?Target->GetRogueAttributeSet()->GetHealth():0.f,ActiveMontage,Appearance->IsOwnerArmsViewActive(),Combat->GetAttackCooldownRemaining(),Combat->IsComboSequenceActive(World->GetTimeSeconds()));}
 void Photo(const TCHAR* Name)
 {
  if(!bRendered)return;Check(PendingCapture.IsEmpty(),TEXT("previous_screenshot_completed_before_next_request"));if(!PendingCapture.IsEmpty())return;++RequestedCaptures;PendingCapture=Name;Captures+=CaptureRow(PendingCapture,TEXT("request"));
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
  FRogue10mCharacterProfile P;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("OwnerViewQa"),FRogue10mCharacterAppearance(),P)&&Profiles->SelectProfile(P.ProfileId),TEXT("fresh_profile_created_selected"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());Check(Character.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_attaches_actual_pawn"));if(Failures){Finish();return;}
  Saves->DetachCharacter(*Character,false);Check(!Saves->IsAttachedTo(*Character),TEXT("autosave_detached_before_transient_fixture"));
  Appearance=Character->GetAppearanceCameraComponent();Body=Character->GetMesh();Source=Character->GetAnimationPlaybackMesh();Camera=Character->GetFirstPersonCameraComponent();Combat=Character->GetCombatComponent();
  Check(Appearance.IsValid()&&Body.IsValid()&&Source.IsValid()&&Camera.IsValid()&&Combat.IsValid()&&Character->GetRogueAttributeSet()&&PC->PlayerCameraManager,TEXT("camera_body_source_combat_attributes_available"));if(Failures){Finish();return;}
  State=Character->GetPlayerState<ARogue10mPlayerState>();Inventory=Character->GetInventoryComponent();
  InitialOwnerArms=Appearance->bEnableOwnerArms;
  Check(State.IsValid()&&Inventory.IsValid()&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle,TEXT("normal_entry_uses_actual_knuckle_inventory_and_player_state"));
  Check(!InitialOwnerArms&&!Appearance->IsOwnerArmsViewActive()&&!GetDefault<URogue10mAppearanceCameraComponent>()->bEnableOwnerArms,TEXT("normal_entry_and_product_default_keep_unaccepted_owner_view_disabled"));if(Failures){Finish();return;}
  // Opt in only this detached QA pawn. Never change the CDO, profile, or authored asset defaults.
  bOwnerArmsOverrideApplied=true;Appearance->bEnableOwnerArms=true;Appearance->RefreshAppearanceCamera();
  OwnerArms=Character->FindComponentByClass<URogue10mOwnerArmsComponent>();Proxy=OwnerArms.IsValid()?OwnerArms->GetOwnerArmsMesh():nullptr;
  Check(OwnerArms.IsValid()&&Proxy.IsValid()&&Appearance->bEnableOwnerArms&&Appearance->IsOwnerArmsViewActive(),TEXT("explicit_QA_instance_optin_activates_integrated_owner_arms_components"));if(Failures){Finish();return;}
  OldFraming=OwnerArms->bEnableViewFraming;OwnerArms->bEnableViewFraming=bFramed;
  Check(!GetDefault<URogue10mOwnerArmsComponent>()->bEnableViewFraming,TEXT("product_framing_stays_default_OFF"));
  Results+=TEXT("owner_view_activation=detached_QA_pawn_instance_only_product_default_OFF\n");
  SourceAsset=Source->GetSkeletalMeshAsset();BodyAsset=Body->GetSkeletalMeshAsset();SourceClass=Source->GetAnimClass();BodyClass=Body->GetAnimClass();
  Dodge=Combat->GetActiveDodgeSkill();DodgeConfig=Dodge.IsValid()?Editable(*Dodge):TEXT("actual_character_fallback");
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
  CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 Width,int32 Height,const TArray<FColor>& Pixels){if(Self->bFinished||Self->PendingCapture.IsEmpty())return;Self->Captures+=Self->CaptureRow(Self->PendingCapture,TEXT("captured"));++Self->CompletedCaptures;if(Self->PendingCapture==TEXT("06_level_E_dodge.png")||Self->PendingCapture==TEXT("07_downlook_E_dodge.png"))Self->Check(Self->Character->IsDodging(),TEXT("dodge_screenshot_captured_during_actual_active_dodge"));if(Self->PendingCapture==TEXT("09_level_actual_LMB.png")||Self->PendingCapture==TEXT("10_downlook_actual_LMB.png")||Self->PendingCapture==TEXT("11_deep85_actual_LMB_no_target.png"))Self->Check(Self->Combat->IsWeaponChangeBlocked()&&Self->AttackSkill.IsValid()&&Self->Source->GetAnimInstance()&&Self->Source->GetAnimInstance()->Montage_IsActive(Self->AttackSkill->AttackMontage),TEXT("attack_screenshot_captured_during_actual_attack_action_and_active_montage"));if(Self->PendingCapture==TEXT("15_level_ready_before_LMB.png")||Self->PendingCapture==TEXT("18_downlook_ready_before_LMB.png"))Self->Check(!Self->Combat->IsWeaponChangeBlocked()&&Self->Target.IsValid()&&Self->Target->GetRogueAttributeSet()->GetHealth()==10000.f,TEXT("ready_photo_captured_before_actual_LMB_with_undamaged_target"));if(Self->PendingCapture==TEXT("16_level_first_damage_observed.png")||Self->PendingCapture==TEXT("19_downlook_first_damage_observed.png"))Self->Check(Self->FirstDamage>=0&&Self->Target.IsValid()&&Self->Target->GetRogueAttributeSet()->GetHealth()<9999.99f,TEXT("contact_photo_requested_only_after_actual_first_damage_observation"));if(Self->PendingCapture==TEXT("17_level_action_recovered.png")||Self->PendingCapture==TEXT("20_downlook_action_recovered.png"))Self->Check(!Self->Combat->IsWeaponChangeBlocked()&&Self->Combat->GetAttackCooldownRemaining()<=.001f&&!Self->Combat->IsComboSequenceActive(Self->World->GetTimeSeconds()),TEXT("recovery_photo_captured_after_actual_action_cooldown_and_combo_recovery"));auto& Capture=Self->CapturedImages.AddDefaulted_GetRef();Capture.Name=Self->PendingCapture;Capture.Width=Width;Capture.Height=Height;Capture.Pixels=Pixels;Self->PendingCapture.Empty();});
  SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* X,ELevelTick,float DT){if(X==Self->World.Get())Self->Sample(DT);});
  GroundDeadline=FPlatformTime::Seconds()+15.;Later(.7,[this]{WaitInitialGround();});
 }
 void WaitInitialGround()
 {
  if(!Character->CanChangeWeapon()&&FPlatformTime::Seconds()<GroundDeadline){Later(.1,[this]{WaitInitialGround();});return;}
  Check(Character->CanChangeWeapon(),TEXT("actual_grounded_idle_ready"));if(Failures){Finish();return;}
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);
  auto* Box=Ground.IsValid()?NewObject<UBoxComponent>(Ground.Get(),TEXT("OwnerViewGround"),RF_Transient):nullptr;Check(Box!=nullptr,TEXT("transient_floor_created"));if(Failures){Finish();return;}
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
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Wall=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);auto* Box=Wall.IsValid()?NewObject<UBoxComponent>(Wall.Get(),TEXT("OwnerViewWall"),RF_Transient):nullptr;Check(Box!=nullptr,TEXT("owned_collision_wall_created"));if(!Box)return;
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
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(OwnerViewWall),false,Character.Get());const auto* C=Character->GetCapsuleComponent();World->SweepSingleByChannel(Hit,Character->GetActorLocation(),Character->GetActorLocation()+Forward*5.f,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),Q);
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
  Later(.06,[this,S]{Key(EKeys::E,false);Check(!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f&&Character->GetRogueAttributeSet()->GetStamina()>=S-.01f,TEXT("airborne_E_is_rejected_without_dodge_or_cost"));Later(.7,[this]{Check((!Dodge.IsValid()||Editable(*Dodge)==DodgeConfig)&&Editable(*Appearance)==AppearanceConfig,TEXT("authored_dodge_and_appearance_data_unchanged"));Check(MeasuredFlushes==0&&MeasuredViewportEvents==0,TEXT("measured_movement_has_no_flush_or_external_viewport_input"));AuditProxyWeights();AttackCase(0);});});
 }
 void OwnerContract()
 {
  Proxy=Appearance->GetOwnerArmsMesh();
  Check(OwnerArms.IsValid()&&OwnerArms->IsActive()&&Appearance->IsOwnerArmsViewActive()&&Proxy.IsValid(),TEXT("integrated_owner_arms_proxy_is_active"));if(!Proxy.IsValid())return;
  Check(Proxy->bOnlyOwnerSee&&!Proxy->bOwnerNoSee&&Proxy->IsVisible()&&!Proxy->bHiddenInGame&&Proxy->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!Proxy->CastShadow&&!Proxy->GetIsReplicated(),TEXT("proxy_is_owner_only_visible_noncolliding_shadowless_nonreplicated"));
  if(!bFramed){auto* Skeletal=Cast<USkeletalMeshComponent>(Proxy.Get());Check(Skeletal&&Proxy->LeaderPoseComponent.Get()==Body.Get()&&Skeletal->GetAnimClass()==nullptr,TEXT("legacy_proxy_uses_exact_body_leader_without_own_animation_notifies"));}
  else{const auto* Posed=Cast<UPoseableMeshComponent>(Proxy.Get());Check(Posed&&OwnerArms->IsViewFramingActive()&&OwnerArms->HasCapturedNeutralPose()&&Proxy->LeaderPoseComponent.Get()==nullptr&&!Proxy->IsComponentTickEnabled(),TEXT("framed_proxy_uses_explicit_pose_copy_without_leader_animation_or_tick"));if(Posed){const auto& SrcPose=Body->GetBoneSpaceTransforms();const auto& ViewPose=Posed->BoneSpaceTransforms;Check(SrcPose.Num()==ViewPose.Num(),TEXT("framed_and_body_local_pose_counts_match"));const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();for(int32 I=0;I<FMath::Min(SrcPose.Num(),ViewPose.Num());++I){const FName N=Ref.GetBoneName(I);const bool Arm=N==TEXT("upperarm_l")||N==TEXT("lowerarm_l")||N==TEXT("upperarm_r")||N==TEXT("lowerarm_r");Check(ViewPose[I].GetTranslation().Equals(SrcPose[I].GetTranslation(),.000001)&&ViewPose[I].GetScale3D().Equals(SrcPose[I].GetScale3D(),.000001),TEXT("framed_copy_preserves_every_local_translation_and_scale"));if(!Arm)Check(ViewPose[I].GetRotation().Equals(SrcPose[I].GetRotation(),.000001),TEXT("framed_copy_preserves_all_rotations_except_four_arm_IK_bones"));}}}
  Check(Body->bOwnerNoSee&&Body->IsVisible()&&!Body->bHiddenInGame&&!Body->IsBoneHiddenByName(Appearance->HeadBone),TEXT("full_body_is_world_visible_owner_hidden_and_head_not_collapsed"));
  const FVector HeadScale=Body->GetSocketTransform(Appearance->HeadBone,RTS_Component).GetScale3D();Check(FMath::Min3(FMath::Abs(HeadScale.X),FMath::Abs(HeadScale.Y),FMath::Abs(HeadScale.Z))>UE_SMALL_NUMBER,TEXT("body_evaluated_head_scale_remains_nonzero"));
  Check(Character->GetAnimationPlaybackMesh()==Source.Get()&&Source->GetSkeletalMeshAsset()==SourceAsset.Get()&&Body->GetSkeletalMeshAsset()==BodyAsset.Get()&&Source->GetAnimClass()==SourceClass.Get()&&Body->GetAnimClass()==BodyClass.Get(),TEXT("gameplay_source_getter_assets_and_animation_classes_are_unchanged"));
  Check(Cast<USkeletalMesh>(Proxy->GetSkinnedAsset())&&Cast<USkeletalMesh>(Proxy->GetSkinnedAsset())->GetSkeleton()==Body->GetSkeletalMeshAsset()->GetSkeleton(),TEXT("proxy_and_current_body_use_exact_skeleton"));
  for(int32 I=0;I<Body->GetNumMaterials();++I)Check(Proxy->GetMaterial(I)==Body->GetMaterial(I),TEXT("proxy_materials_match_current_body_overrides"));
  if(!bFramed)for(FName Bone:{FName(TEXT("hand_l")),FName(TEXT("hand_r"))})Check(Proxy->GetSocketTransform(Bone).Equals(Body->GetSocketTransform(Bone),.01f),TEXT("proxy_hand_sockets_match_body_leader_world_pose"));
 }
 static bool ArmBone(const FReferenceSkeleton& Ref,int32 Bone){while(Ref.IsValidIndex(Bone)){const FName N=Ref.GetBoneName(Bone);if(N==TEXT("upperarm_l")||N==TEXT("upperarm_r"))return true;Bone=Ref.GetParentIndex(Bone);}return false;}
 void AuditProxyWeights()
 {
  OwnerContract();if(!Proxy.IsValid()||!Cast<USkeletalMesh>(Proxy->GetSkinnedAsset()))return;const auto* Mesh=Cast<USkeletalMesh>(Proxy->GetSkinnedAsset());const auto* Render=Mesh->GetResourceForRendering();Results+=TEXT("runtime_geometry_scope=current_loaded_proxy_weighted_vertex_sample_only_full_triangle_and_six_variant_audit_separate_editor_evidence\n");
  if(!Render){Results+=TEXT("runtime_proxy_weights=unavailable_no_render_data_no_geometry_PASS_claim\n");return;}
  const auto& Ref=Mesh->GetRefSkeleton();uint64 Sampled=0,NonArm=0;bool AllReadable=true;
  for(int32 L=0;L<Render->LODRenderData.Num();++L){const auto& Lod=Render->LODRenderData[L];const auto& W=Lod.SkinWeightVertexBuffer;const bool Readable=W.GetDataVertexBuffer()&&W.GetDataVertexBuffer()->GetWeightData()&&(!W.GetVariableBonesPerVertex()||(W.GetLookupVertexBuffer()&&W.GetLookupVertexBuffer()->GetLookupData()));if(!Readable){AllReadable=false;continue;}for(const auto& Section:Lod.RenderSections){for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V){if(V>=W.GetNumVertices()){AllReadable=false;continue;}uint64 Total=0,Arm=0;for(uint32 I=0;I<W.GetMaxBoneInfluences();++I){const uint16 Weight=W.GetBoneWeight(V,I);if(!Weight)continue;Total+=Weight;const uint32 B=W.GetBoneIndex(V,I);if(Section.BoneMap.IsValidIndex(B)&&ArmBone(Ref,Section.BoneMap[B]))Arm+=Weight;}++Sampled;if(!Total||static_cast<double>(Arm)/Total<.49)++NonArm;}}}
  Results+=FString::Printf(TEXT("runtime_proxy_weight_vertices_sampled=%llu\nruntime_proxy_non_arm_weight_vertices=%llu\nruntime_proxy_all_weight_buffers_readable=%d\n"),Sampled,NonArm,AllReadable);if(AllReadable)Check(Sampled>0&&NonArm==0,TEXT("current_runtime_proxy_has_zero_nonarm_weight_vertices_in_readable_buffers"));
 }
 bool SamePose(const TArray<FTransform>& A,const TArray<FTransform>& B)const{if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].Equals(B[I],.000001f))return false;return true;}
 void ToggleInvariant()
 {
  FMinimalViewInfo Before;Character->CalcCamera(0,Before);FTransform Trace;Check(Appearance->ReadCameraTransform(Trace),TEXT("pre_toggle_combat_camera_readable"));const auto SP=Source->GetBoneSpaceTransforms();const auto BP=Body->GetBoneSpaceTransforms();const double At=World->GetTimeSeconds();const float Pos=MontagePosition(),CD=Combat->GetAttackCooldownRemaining();const int32 Uses=State->GetSkillUseCount(AttackSkill->GetFName());const float HP=Target.IsValid()?Target->GetRogueAttributeSet()->GetHealth():0.f;auto* Anim=Source->GetAnimInstance();const auto* Montage=Anim?Anim->GetCurrentActiveMontage():nullptr;const auto* Instance=Anim?Anim->GetInstanceForMontage(AttackSkill->AttackMontage):nullptr;const float Rate=Instance?Instance->GetPlayRate():0,Weight=Instance?Instance->GetWeight():0;const bool Active=Anim&&Anim->Montage_IsActive(AttackSkill->AttackMontage);
  for(bool Enable:{false,true}){Appearance->bEnableOwnerArms=Enable;Appearance->RefreshAppearanceCamera();FMinimalViewInfo After;Character->CalcCamera(0,After);FTransform NewTrace;const bool Have=Appearance->ReadCameraTransform(NewTrace);Check(World->GetTimeSeconds()==At&&SamePose(SP,Source->GetBoneSpaceTransforms())&&SamePose(BP,Body->GetBoneSpaceTransforms()),TEXT("same_frame_proxy_toggle_preserves_every_source_and_body_local_pose"));Check(Have&&Trace.Equals(NewTrace,.000001f)&&Before.Location.Equals(After.Location,.000001f)&&Before.Rotation.Equals(After.Rotation,.000001f)&&FMath::Abs(Before.FOV-After.FOV)<.000001f,TEXT("same_frame_proxy_toggle_preserves_render_and_combat_camera"));const auto* AfterInstance=Anim?Anim->GetInstanceForMontage(AttackSkill->AttackMontage):nullptr;Check(AfterInstance==Instance&&(!Instance||(AfterInstance->GetPlayRate()==Rate&&AfterInstance->GetWeight()==Weight))&&(Anim&&Anim->Montage_IsActive(AttackSkill->AttackMontage))==Active,TEXT("same_frame_each_proxy_toggle_preserves_montage_instance_rate_weight_and_active_state"));Check(MontagePosition()==Pos&&Source->GetAnimInstance()==Anim&&(!Anim||Anim->GetCurrentActiveMontage()==Montage)&&Combat->GetAttackCooldownRemaining()==CD&&State->GetSkillUseCount(AttackSkill->GetFName())==Uses&&(!Target.IsValid()||Target->GetRogueAttributeSet()->GetHealth()==HP),TEXT("same_frame_proxy_toggle_preserves_montage_clock_cooldown_use_count_and_damage"));Check(Appearance->IsOwnerArmsViewActive()==Enable,TEXT("explicit_proxy_toggle_changes_only_owner_view_policy"));}
  ++InvariantPairs;OwnerContract();
 }
 void WaitAttackReady(TFunction<void()> Done)
 {
  if((!Character->CanChangeWeapon()||Combat->GetAttackCooldownRemaining()>.001f||Combat->IsComboSequenceActive(World->GetTimeSeconds()))&&FPlatformTime::Seconds()<GroundDeadline){Later(.05,[this,Done=MoveTemp(Done)]()mutable{WaitAttackReady(MoveTemp(Done));});return;}Check(Character->CanChangeWeapon()&&Combat->GetAttackCooldownRemaining()<=.001f&&!Combat->IsComboSequenceActive(World->GetTimeSeconds()),TEXT("attack_probe_waits_for_original_action_cooldown_and_combo_recovery"));if(Failures){Finish();return;}Done();
 }
 void AttackCase(int32 Index)
 {
  GroundDeadline=FPlatformTime::Seconds()+20.;WaitAttackReady([this,Index]{ResetFixture();SetLook(Index==0?0.f:(Index==1?-30.f:-85.f),[this,Index]{AttackIndex=Index;AttackSkill=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);Check(AttackSkill.IsValid()&&AttackSkill->AttackMontage,TEXT("unchanged_authored_knuckle_primary_with_montage_available"));if(Failures){Finish();return;}AttackConfig=Editable(*AttackSkill);Results+=FString::Printf(TEXT("attack_%d_skill_asset=%s\nattack_%d_montage_asset=%s\nattack_%d_montage_length_s=%.9f\nattack_%d_authored_hit_delay_montage_s=%.9f\nattack_%d_authored_play_rate=%.9f\n"),Index,*AttackSkill->GetPathName(),Index,*AttackSkill->AttackMontage->GetPathName(),Index,AttackSkill->AttackMontage->GetPlayLength(),Index,AttackSkill->HitStartDelaySeconds,Index,AttackSkill->AnimationPlayRate);for(int32 Slot=0;Slot<AttackSkill->AttackMontage->SlotAnimTracks.Num();++Slot){const auto& Track=AttackSkill->AttackMontage->SlotAnimTracks[Slot];for(int32 Segment=0;Segment<Track.AnimTrack.AnimSegments.Num();++Segment){const auto& Part=Track.AnimTrack.AnimSegments[Segment];Results+=FString::Printf(TEXT("attack_%d_slot_%d_segment_%d_asset=%s\nattack_%d_slot_%d_segment_%d_source_start_s=%.9f\nattack_%d_slot_%d_segment_%d_source_end_s=%.9f\nattack_%d_slot_%d_segment_%d_montage_start_s=%.9f\nattack_%d_slot_%d_segment_%d_rate=%.9f\n"),Index,Slot,Segment,*GetPathNameSafe(Part.GetAnimReference().Get()),Index,Slot,Segment,Part.AnimStartTime,Index,Slot,Segment,Part.AnimEndTime,Index,Slot,Segment,Part.StartPos,Index,Slot,Segment,Part.AnimPlayRate);}}ExpectedRate=FMath::Max(.01f,FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f)*AttackSkill->AnimationPlayRate);if(Target.IsValid())Target->Destroy();Target=nullptr;
   if(Index<2){UClass* Class=LoadClass<ARogue10mBasicMonster>(nullptr,TEXT("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_ThornWarden.BP_OW_ThornWarden_C"));FMinimalViewInfo V;Character->CalcCamera(0,V);const FVector Ray=V.Location+V.Rotation.Vector()*FMath::Min(125.f,AttackSkill->AttackRange*.65f);FTransform At(Character->GetActorRotation(),Ray);Target=Class?World->SpawnActorDeferred<ARogue10mBasicMonster>(Class,At,PC.Get(),Character.Get(),ESpawnActorCollisionHandlingMethod::AlwaysSpawn):nullptr;Check(Target.IsValid(),TEXT("authored_monster_spawned_for_actual_camera_ray_hit_probe"));if(Failures){Finish();return;}Target->MarkCombatTestInstance();UGameplayStatics::FinishSpawningActor(Target.Get(),At);if(auto* AI=Cast<ARogue10mMonsterAIController>(Target->GetController()))AI->StopMonsterAI();Target->GetCharacterMovement()->DisableMovement();Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);FVector Location=Ray;Location.Z=Surface+Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.;Target->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);Target->GetRogueAttributeSet()->SetMaxHealth(10000);Target->GetRogueAttributeSet()->SetHealth(10000);if(auto* R=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>())R->ConfigureRegeneration(0,0,0);FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(OwnerViewAttackFixture),false,Character.Get());World->LineTraceSingleByChannel(Hit,V.Location,V.Location+V.Rotation.Vector()*AttackSkill->AttackRange,ECC_Pawn,Q);Check(!Location.ContainsNaN()&&Location.Z-Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()>=Surface+1.f&&Hit.bBlockingHit&&Hit.GetActor()==Target.Get(),TEXT("grounded_target_capsule_is_above_floor_and_intersects_real_attack_ray_within_range"));Results+=FString::Printf(TEXT("attack_%d_fixture_pitch=%.6f\nattack_%d_target_bottom_above_floor_cm=%.6f\nattack_%d_ray_first_hit_target=%d\n"),Index,FRotator::NormalizeAxis(PC->GetControlRotation().Pitch),Index,Location.Z-Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Surface,Index,Hit.GetActor()==Target.Get());if(Failures){Finish();return;}}
   if(Index<2){Stage=FString::Printf(TEXT("attack_%d_ready_before_input"),Index);Photo(Index==0?TEXT("15_level_ready_before_LMB.png"):TEXT("18_downlook_ready_before_LMB.png"));}
   Later(.4,[this,Index]{Stage=FString::Printf(TEXT("attack_%d"),Index);UsesBefore=State->GetSkillUseCount(AttackSkill->GetFName());AttackAccepted=-1;FirstDamage=-1;AcceptedDT=0;DamageDT=0;MontageSamples=0;bAttackToggled=false;bObserveAttack=true;Character->GetRogueAttributeSet()->RestoreVitals();Key(EKeys::LeftMouseButton,true);Later(.08,[this]{Key(EKeys::LeftMouseButton,false);GroundDeadline=FPlatformTime::Seconds()+20.;Later(.1,[this]{WaitAttackEnd();});});});});});
 }
 void WaitAttackEnd()
 {
  if((!Character->CanChangeWeapon()||Combat->GetAttackCooldownRemaining()>.001f||Combat->IsComboSequenceActive(World->GetTimeSeconds()))&&FPlatformTime::Seconds()<GroundDeadline){Later(.05,[this]{WaitAttackEnd();});return;}bObserveAttack=false;Check(Character->CanChangeWeapon()&&Combat->GetAttackCooldownRemaining()<=.001f&&!Combat->IsComboSequenceActive(World->GetTimeSeconds()),TEXT("actual_knuckle_attack_action_cooldown_and_combo_all_recover_before_timeout"));Check(State->GetSkillUseCount(AttackSkill->GetFName())==UsesBefore+1&&AttackAccepted>=0,TEXT("actual_LMB_accepts_exactly_one_original_knuckle_skill"));Check(MontageSamples>=2&&bAttackToggled,TEXT("actual_montage_evaluates_and_owner_view_toggle_invariant_was_measured"));
  if(AttackIndex<2){const float Damage=10000.f-Target->GetRogueAttributeSet()->GetHealth();const double Error=FirstDamage-AttackAccepted-AttackSkill->HitStartDelaySeconds/ExpectedRate;const double Early=-AcceptedDT-.001,Late=DamageDT+.001;Check(Damage>0&&FirstDamage>=0&&FMath::IsFinite(Error)&&Error>=Early&&Error<=Late,TEXT("actual_damage_and_authored_first_hit_clock_stay_within_recorded_frame_quantization"));Results+=FString::Printf(TEXT("attack_%d_damage=%.6f\nattack_%d_first_hit_error_s=%.9f\nattack_%d_allowed_early_s=%.9f\nattack_%d_allowed_late_s=%.9f\n"),AttackIndex,Damage,AttackIndex,Error,AttackIndex,Early,AttackIndex,Late);}else Results+=TEXT("attack_2_minus85_no_target_acceptance_and_montage_only_not_hit_validation\n");Check(Editable(*AttackSkill)==AttackConfig,TEXT("authored_attack_timing_resources_and_data_unchanged"));
  if(AttackIndex<2){Stage=FString::Printf(TEXT("attack_%d_recovered"),AttackIndex);Photo(AttackIndex==0?TEXT("17_level_action_recovered.png"):TEXT("20_downlook_action_recovered.png"));}
  Later(.85,[this]{if(Target.IsValid())Target->Destroy();Target=nullptr;if(AttackIndex<2)AttackCase(AttackIndex+1);else InspectionAndSword();});
 }
 bool EquipSword(){for(int32 I=0;I<Inventory->GetInventoryContainers().Num();++I)for(const auto& E:Inventory->GetInventoryContainers()[I].Entries)if(E.InstanceId==SwordId)return Inventory->TryEquipGridItem(I,SwordId);return false;}
 void InspectionAndSword()
 {
  ResetFixture();SetLook(-30.f,[this]{Stage=TEXT("fullbody_inspection");Key(EKeys::V,true);Later(.04,[this]{Key(EKeys::V,false);Later(.25,[this]{Check(Appearance->IsThirdPersonInspectionActive()&&!Appearance->IsOwnerArmsViewActive()&&Proxy.IsValid()&&(!Proxy->IsVisible()||Proxy->bHiddenInGame)&&!Body->bOwnerNoSee&&Body->IsVisible()&&!Body->bHiddenInGame&&!Body->IsBoneHiddenByName(Appearance->HeadBone),TEXT("actual_V_inspection_restores_full_body_head_and_hides_owner_proxy"));Photo(TEXT("12_fullbody_inspection.png"));Later(.85,[this]{Key(EKeys::V,true);Later(.04,[this]{Key(EKeys::V,false);Later(.25,[this]{OwnerContract();Sword.Reset(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));Sword->Category=ERogue10mItemCategory::Equipment;Sword->EquipSlotType=ERogue10mInventorySlotType::MainWeapon;Sword->WeaponType=ERogue10mWeaponType::GreatSword;Sword->DisplayName=FText::FromString(TEXT("QA OwnerView GreatSword"));int32 I=INDEX_NONE;Check(Inventory->TryAddGridItem(Sword.Get(),1,I,SwordId)&&EquipSword()&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::GreatSword,TEXT("QA_owned_greatsword_equipped_through_real_inventory_without_getter_redirect"));Later(.8,[this]{Stage=TEXT("owned_greatsword");OwnerContract();ValidateView();Photo(TEXT("13_owned_greatsword.png"));Later(.85,[this]{Stage=TEXT("owner_view_return");ValidateView();Check(Editable(*Appearance)==AppearanceConfig&&Character->GetAnimationPlaybackMesh()==Source.Get()&&InvariantPairs==3,TEXT("appearance_authored_settings_source_and_three_attack_invariant_pairs_preserved"));Photo(TEXT("14_owner_view_return.png"));Later(.85,[this]{Finish();});});});});});});});});});
 }
 void ValidateView()
 {
  FMinimalViewInfo V;Character->CalcCamera(0,V);FTransform T;const bool OK=Appearance->ReadCameraTransform(T);Check(OK&&Appearance->IsAppearanceCameraActive()&&!Appearance->IsThirdPersonInspectionActive()&&Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("requested_live_first_person_profile_and_trace_available"));
  Check(!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV)&&OK&&V.Location.Equals(T.GetLocation(),.001f)&&V.Rotation.Equals(T.Rotator(),.001f),TEXT("actual_camera_and_combat_trace_share_finite_origin_rotation"));OwnerContract();
 }
 double Depth(FName Bone,const FMinimalViewInfo& V)const{return Body->GetBoneIndex(Bone)!=INDEX_NONE?FVector::DotProduct(Body->GetSocketLocation(Bone)-V.Location,V.Rotation.Vector()):-99999.;}
 void Sample(float DT)
 {
  if(bFinished||!bPrepared||!Character.IsValid()||!Appearance.IsValid())return;const double Now=FPlatformTime::Seconds();const double WallDT=LastWall>0?Now-LastWall:0.;LastWall=Now;FMinimalViewInfo V;Character->CalcCamera(0,V);FTransform T;const bool Have=Appearance->ReadCameraTransform(T);const FVector P=Character->GetActorLocation();const FVector Vel=Character->GetVelocity();
  if(bMeasured)
  {
   OwnerContract();
   Check(Have&&!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV),TEXT("measured_camera_is_finite_and_trace_readable"));Check(Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("profile_stays_active_during_actual_movement"));Check(PC->GetControlRotation().Equals(FRotator(WantedPitch,InitialControl.Yaw,0),.04f),TEXT("measured_control_rotation_is_not_changed_by_sprint_dodge_or_external_input"));
   if(Stage.StartsWith(TEXT("dodge_"))){MaxDodgeDT=FMath::Max(MaxDodgeDT,static_cast<double>(DT));if(Character->IsDodging()){++DashSamples;if(AcceptedAt<0.){AcceptedAt=World->GetTimeSeconds();FirstActiveDT=DT;Check(Character->GetDodgeCooldownRemaining()<=Cooldown+.001f&&(Cooldown<=0.f||Character->GetDodgeCooldownRemaining()>0.f),TEXT("accepted_dodge_starts_authored_cooldown"));if(DodgeIndex<2&&!bDashPhoto){bDashPhoto=true;Photo(DodgeIndex==0?TEXT("06_level_E_dodge.png"):TEXT("07_downlook_E_dodge.png"));}}}else if(AcceptedAt>=0.&&DodgeEndObserved<0.){DodgeEndObserved=World->GetTimeSeconds();EndObservedDT=DT;}}
  }
  if(bObserveAttack&&AttackSkill.IsValid())
  {
   const double At=World->GetTimeSeconds();if(AttackAccepted<0&&State->GetSkillUseCount(AttackSkill->GetFName())>UsesBefore){AttackAccepted=At;AcceptedDT=DT;Check(Combat->GetLastAcceptedAttackSkill()==AttackSkill.Get(),TEXT("accepted_presentation_skill_is_the_original_primary_skill"));Photo(AttackIndex==0?TEXT("09_level_actual_LMB.png"):(AttackIndex==1?TEXT("10_downlook_actual_LMB.png"):TEXT("11_deep85_actual_LMB_no_target.png")));}
   float Rate=0,Weight=0;if(auto* A=Source->GetAnimInstance()){if(const auto* Instance=A->GetInstanceForMontage(AttackSkill->AttackMontage)){Rate=Instance->GetPlayRate();Weight=Instance->GetWeight();if(Weight>UE_SMALL_NUMBER){++MontageSamples;Check(FMath::Abs(Rate-ExpectedRate)<.001f,TEXT("actual_attack_montage_rate_remains_authored_rate"));if(!bAttackToggled){bAttackToggled=true;ToggleInvariant();}}}}
   if(Target.IsValid()&&FirstDamage<0&&Target->GetRogueAttributeSet()->GetHealth()<9999.99f){FirstDamage=At;DamageDT=DT;Photo(AttackIndex==0?TEXT("16_level_first_damage_observed.png"):TEXT("19_downlook_first_damage_observed.png"));}OwnerContract();
   AttackFrames+=FString::Printf(TEXT("%llu,%s,%.9f,%.9f,%.9f,%.6f,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.6f\n"),GFrameCounter,*Stage,At,DT,WallDT,FRotator::NormalizeAxis(PC->GetControlRotation().Pitch),State->GetSkillUseCount(AttackSkill->GetFName()),Combat->IsWeaponChangeBlocked(),Combat->GetAttackCooldownRemaining(),MontagePosition(),Rate,Weight,ExpectedRate,Target.IsValid()?Target->GetRogueAttributeSet()->GetHealth():0.f);
  }
  const auto* VisibleArms=Appearance->GetOwnerArmsMesh();const bool HaveArms=VisibleArms!=nullptr;const FVector OwnerL=HaveArms?VisibleArms->GetSocketLocation(TEXT("hand_l")):FVector::ZeroVector,OwnerR=HaveArms?VisibleArms->GetSocketLocation(TEXT("hand_r")):FVector::ZeroVector;
  const auto& CachedView=PC->PlayerCameraManager->GetCameraCacheView();const FVector OwnerVL=HaveArms?CachedView.Rotation.Quaternion().UnrotateVector(OwnerL-CachedView.Location):FVector::ZeroVector,OwnerVR=HaveArms?CachedView.Rotation.Quaternion().UnrotateVector(OwnerR-CachedView.Location):FVector::ZeroVector;
  PresentationFrames+=FString::Printf(TEXT("%llu,%.9f,%s,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),GFrameCounter,World->GetTimeSeconds(),*Stage,OwnerArms.IsValid()&&OwnerArms->IsViewFramingActive(),HaveArms,PC->PlayerCameraManager->GetCameraCacheTime(),OwnerL.X,OwnerL.Y,OwnerL.Z,OwnerR.X,OwnerR.Y,OwnerR.Z,OwnerVL.X,OwnerVL.Y,OwnerVL.Z,OwnerVR.X,OwnerVR.Y,OwnerVR.Z);
  const FVector HandL=Body->GetSocketLocation(TEXT("hand_l")),HandR=Body->GetSocketLocation(TEXT("hand_r"));const FVector ViewL=V.Rotation.Quaternion().UnrotateVector(HandL-V.Location),ViewR=V.Rotation.Quaternion().UnrotateVector(HandR-V.Location);
  AnimationFrames+=FString::Printf(TEXT("%llu,%s,%.9f,%.9f,%.9f,%s,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),GFrameCounter,*Stage,World->GetTimeSeconds(),DT,WallDT,AttackSkill.IsValid()?*AttackSkill->GetPathName():TEXT("None"),AttackSkill.IsValid()?*GetPathNameSafe(AttackSkill->AttackMontage):TEXT("None"),MontagePosition(),HandL.X,HandL.Y,HandL.Z,HandR.X,HandR.Y,HandR.Z,ViewL.X,ViewL.Y,ViewL.Z,ViewR.X,ViewR.Y,ViewR.Z);
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
  // This cleanup also runs if opt-in failed before the rest of the fixture was prepared.
  if(bOwnerArmsOverrideApplied&&Appearance.IsValid()){if(Appearance->IsThirdPersonInspectionActive())Appearance->ToggleThirdPersonInspection();if(OwnerArms.IsValid())OwnerArms->bEnableViewFraming=OldFraming;Appearance->bEnableOwnerArms=InitialOwnerArms;Appearance->RefreshAppearanceCamera();Check(Appearance->bEnableOwnerArms==InitialOwnerArms&&!Appearance->IsOwnerArmsViewActive()&&!GetDefault<URogue10mAppearanceCameraComponent>()->bEnableOwnerArms,TEXT("QA_instance_optin_restored_to_original_OFF_with_product_default_unchanged"));bOwnerArmsOverrideApplied=false;}
  if(bPrepared&&Character.IsValid()){if(Saves.IsValid()&&Saves->IsAttachedTo(*Character))Saves->DetachCharacter(*Character,false);PC->CloseAllBlockingPanels();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetCanBeDamaged(bInitialDamage);Character->SetActorTransform(InitialTransform,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(InitialControl);}
  if(Target.IsValid())Target->Destroy();if(Wall.IsValid())Wall->Destroy();if(Ground.IsValid())Ground->Destroy();if(bOwnCVars){Heavy->Set(OldHeavy,ECVF_SetByConsole);Motion->Set(OldMotion,ECVF_SetByConsole);Fov->Set(OldFov,ECVF_SetByConsole);}
  if(bRendered)Check(RequestedCaptures==20&&CompletedCaptures==20&&PendingCapture.IsEmpty(),TEXT("all_original_fourteen_and_six_phase_actual_viewport_captures_completed"));
  // Keep PNG compression and disk IO out of the measured movement/dodge frames.
  for(const auto& Capture:CapturedImages){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Capture.Width,Capture.Height,Capture.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Images/Capture.Name)),TEXT("actual_viewport_pixels_saved_as_PNG"));}CapturedImages.Empty();
  Check(FFileHelper::SaveStringToFile(AnimationFrames,*(Output/TEXT("animationframes.csv"))),TEXT("actual_hand_world_and_camera_relative_trajectory_frames_written"));
  Check(FFileHelper::SaveStringToFile(PresentationFrames,*(Output/TEXT("presentationframes.csv"))),TEXT("actual_owner_presentation_hand_trajectory_written"));Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("raw_frames_written"));Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))),TEXT("capture_metadata_written"));Check(FFileHelper::SaveStringToFile(AttackFrames,*(Output/TEXT("attackframes.csv"))),TEXT("actual_attack_frame_clock_evidence_written"));Results+=FString::Printf(TEXT("owner_view_toggle_invariant_pairs=%d\nowner_proxy_mesh=%s\noriginal_animation_source=%s\nbody_mesh=%s\n"),InvariantPairs,Proxy.IsValid()?*GetPathNameSafe(Cast<USkeletalMesh>(Proxy->GetSkinnedAsset())):TEXT("None"),*GetPathNameSafe(SourceAsset.Get()),*GetPathNameSafe(BodyAsset.Get()));
  Results+=FString::Printf(TEXT("framing_mode=%d\n"),bFramed);
  Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nprofile=%s\nrendered=%d\noffscreen=%d\ninput=programmatic_Controller_InputKey_and_reflected_DoAim\nphysical_input=not_performed\nmanual_field_completion=not_performed\nvisual_clipping=requires_actual_PNG_review_numeric_depths_are_diagnostics_only\nfixture=transient_visible_floor_and_collision_wall_existing_engine_cube_no_saved_map_edits\nstamina_assertion_scope=observed_cost_ceiling_only_regeneration_retained_exact_billing_not_claimed\nmeasured_flushes=%d\nmeasured_viewport_events=%d\nrequested_captures=%d\ncompleted_captures=%d\nanimation_capture_scope=09_11_acceptance_15_18_ready_before_input_16_19_first_damage_observed_17_20_action_cooldown_combo_recovered\ncontact_capture_latency=request_at_first_damage_observation_actual_pixels_on_recorded_capture_frame_not_exact_subframe_contact\nanimation_edit_scope=original_assets_and_gameplay_clocks_retained_framed_mode_changes_four_owner_arm_rotations_and_component_frame\nNOTE=E invokes existing DoDodge; timing/cost/assets unchanged. Distance data sets initial velocity and is not asserted as realized travel. Pictures during dodge retain original duration and record request/capture frame; idle pictures hold state before case changes. Captures are not complete motion/mesh clipping certification.\n"),Checks,Failures,*Profile,bRendered,bOffscreen,MeasuredFlushes,MeasuredViewportEvents,RequestedCaptures,CompletedCaptures);
  if(!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))))++Failures;UE_LOG(LogRogue10m,Display,TEXT("RESULT=OWNER_VIEW_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;TWeakObjectPtr<USkeletalMeshComponent> Body,Source;TWeakObjectPtr<UCameraComponent> Camera;TWeakObjectPtr<URogue10mCombatComponent> Combat;TWeakObjectPtr<const URogue10mDodgeSkillDataAsset> Dodge;TWeakObjectPtr<UGameViewportClient> Viewport;TWeakObjectPtr<AActor> Ground,Wall;TWeakObjectPtr<UStaticMesh> Cube;
 TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;IConsoleVariable* Heavy=nullptr;IConsoleVariable* Motion=nullptr;IConsoleVariable* Fov=nullptr;FDelegateHandle SampleHandle,Cleanup,FlushHandle,InputHandle,CaptureHandle;TFunction<void()> Next;
 TWeakObjectPtr<URogue10mOwnerArmsComponent> OwnerArms;TWeakObjectPtr<USkinnedMeshComponent> Proxy;TWeakObjectPtr<ARogue10mPlayerState> State;TWeakObjectPtr<URogue10mInventoryComponent> Inventory;TWeakObjectPtr<URogue10mAttackSkillData> AttackSkill;TWeakObjectPtr<ARogue10mBasicMonster> Target;TWeakObjectPtr<USkeletalMesh> SourceAsset,BodyAsset;TWeakObjectPtr<UClass> SourceClass,BodyClass;TStrongObjectPtr<URogue10mItemDataAsset> Sword;FGuid SwordId;
 FString AttackConfig,AttackFrames,AnimationFrames,PresentationFrames;double AttackAccepted=-1,FirstDamage=-1,AcceptedDT=0,DamageDT=0;float ExpectedRate=1;int32 UsesBefore=0,AttackIndex=0,MontageSamples=0,InvariantPairs=0;bool bObserveAttack=false,bAttackToggled=false,InitialOwnerArms=false,bOwnerArmsOverrideApplied=false,bFramed=false,OldFraming=false;
 TArray<FCapturedPixels> CapturedImages;FString Label,Profile,Output,Images,Stage=TEXT("prepare"),Frames,Results,Captures,PendingCapture,DodgeConfig,AppearanceConfig;FTransform InitialTransform,Fixture;FRotator InitialControl;FVector MoveOrigin,DodgeOrigin,Forward;
 double Deadline=0,NextAt=0,GroundDeadline=0,LastWall=0,Surface=0,AcceptedAt=-1,DodgeRequestWorld=0,DodgeRequestWall=0,RepeatDeadline=0,FreeTravel[2]{},DodgeEndObserved=-1,FirstActiveDT=0,EndObservedDT=0,MaxDodgeDT=0;float PitchPerInput=1,WantedPitch=0,Duration=.16f,Cooldown=.45f,Distance=100,Cost=0,BeforeStamina=0,RepeatStamina=0,OldMotion=1,OldFov=0;int32 Checks=0,Failures=0,OldHeavy=0,DodgeIndex=0,DashSamples=0,Flushes=0,ViewportEvents=0,MeasuredFlushes=0,MeasuredViewportEvents=0,RequestedCaptures=0,CompletedCaptures=0;bool bFinished=false,bPrepared=false,bRendered=false,bOffscreen=false,bWeighted=false,bOwnCVars=false,bInitialDamage=true,bFlushHook=false,bMeasured=false,bDashPhoto=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_VIEW_FAILED reason=no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestOwnerView"),TEXT("Exact fresh OwnerView QA profiles only. Production downlook clamp, sprint, E dodge, collision and cooldown; viewport photos require visual clipping review."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
