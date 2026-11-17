#if WITH_EDITOR
// Rendered presentation and synthetic-item real inventory transaction. No skill request, hit or damage test.
#include "AbilitySystemComponent.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mStylizedCharacter.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mStaffPresentation
{
static constexpr const TCHAR* StaffPath=TEXT("/Game/Weapons/Staff20261110/SM_BattleWizardStaff01.SM_BattleWizardStaff01");
static constexpr const TCHAR* MannySkeleton=TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
static constexpr const TCHAR* CommonClass=TEXT("/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed.ABP_Common_Unarmed_C");
static constexpr const TCHAR* RetargetClass=TEXT("/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M.ABP_Retarget_Hu_M_C");
static FTransform CandidateGrip()
{
 // Exact provisional StaffMeshDerivativeA/grip-spec-a.json. Never written to a CDO or package.
 return FTransform(FQuat(.28596543351450643,.5380152286970158,.6652892006679809,.431455286211118),
  FVector(-5.897337436676025,-.12185341864824561,2.099235296249381),FVector(.7902026362595275));
}
class FRun : public TSharedFromThis<FRun>
{
public:
 void Start(UWorld* W)
 {
  World=W;StartedWall=FPlatformTime::Seconds();FString Label,Token,Profile,Progression,Keys;
  FParse::Value(FCommandLine::Get(),TEXT("RogueStaffLabel="),Label);
  FParse::Value(FCommandLine::Get(),TEXT("RogueStaffSaveNamespace="),Token);
  FParse::Value(FCommandLine::Get(),TEXT("RogueProfileSaveSlot="),Profile);
  FParse::Value(FCommandLine::Get(),TEXT("RogueProgressionSavePrefix="),Progression);
  FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Keys);
  bool bSafe=!Label.IsEmpty()&&Label==FPaths::MakeValidFileName(Label)&&!Token.IsEmpty();
  for(const TCHAR C:Token){bSafe&=FChar::IsAlnum(C)||C==TEXT('_');}
  bSafe&=Profile==TEXT("Rogue10m_Town_QA_Staff_")+Token&&Progression==TEXT("Rogue10m_Progression_QA_Staff_")+Token&&Keys==TEXT("Rogue10m_Fighter_QA_Staff_")+Token;
  if(!bSafe){UE_LOG(LogRogue10m,Error,TEXT("STAFF_PRESENTATION FAIL exact_QA_save_namespace_required_use_VerifyStaffPresentation"));bFinished=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/staff-equipment-20261110")/Label;
  Images=FPaths::ProjectDir()/TEXT("DevLog/20261110_StaffPresentation")/Label;
  IFileManager::Get().MakeDirectory(*Output,true);IFileManager::Get().MakeDirectory(*Images,true);
  RunId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
  Frames=TEXT("frame,world_s,case,phase,montage_s,playing,body_hand_travel_cm,source_hand_travel_cm,right_palm_shaft_cm,left_palm_shaft_cm,actor_shift_cm,actor_angle_deg,hand_r_x,hand_r_y,hand_r_z,palm_r_x,palm_r_y,palm_r_z,shaft_a_x,shaft_a_y,shaft_a_z,shaft_b_x,shaft_b_y,shaft_b_z,body_class,source_class,default_slot_weight,jewel_up_dot,authored_rotation_error_deg\n");
  Captures=TEXT("event,case,phase,camera,frame,world_s,montage_s,playing,file,bytes\n");
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION SCOPE actual_game_actor=1 continuous_montage=1 combat_request=0 inventory_equip_proof=1 damage_proof=0 frozen_pose=0 visual_approval=0 save_namespace=%s"),*Token);
  W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
 }
 bool Finished()const{return bFinished;}
private:
 void Check(bool Passed,const TCHAR* Reason)
 {if(!Passed){++Failures;UE_LOG(LogRogue10m,Error,TEXT("STAFF_PRESENTATION FAIL frame=%d %s"),Frame,Reason);}}
 UAnimInstance* Anim()const{return Character.IsValid()&&Character->GetAnimationPlaybackMesh()?Character->GetAnimationPlaybackMesh()->GetAnimInstance():nullptr;}
 bool Playing()const{return Anim()&&Montage.IsValid()&&Anim()->Montage_IsPlaying(Montage.Get());}
 float Position()const{return Anim()&&Montage.IsValid()?Anim()->Montage_GetPosition(Montage.Get()):-1.f;}
 const TCHAR* CaseName()const{return CaseIndex==0?TEXT("StaffStrike"):TEXT("StaffShot");}
 void Prepare()
 {
  Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
  auto* Old=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
  UClass* Class=LoadClass<ARogue10mStylizedCharacter>(nullptr,TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
  if(!Old||!Class){Check(false,TEXT("normal_world_player_or_human_BP_missing"));Finish();return;}
  // Preserve the original transform/control rotation; never rotate the body to satisfy the camera.
  InitialTransform=Old->GetActorTransform();const FRotator Control=Controller->GetControlRotation();
  Old->SetActorEnableCollision(false);Old->SetActorHiddenInGame(true);
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.ObjectFlags|=RF_Transient;
  Character=World->SpawnActor<ARogue10mStylizedCharacter>(Class,InitialTransform,P);
  if(!Character.IsValid()){Check(false,TEXT("human_BP_spawn_failed"));Finish();return;}
  Controller->Possess(Character.Get());Controller->SetControlRotation(Control);Controller->SetViewTarget(Character.Get());
  FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("StaffPresentationQA");
  Profile.Appearance.Race=Character->GetAppearanceRace();Profile.Appearance.Gender=Character->GetAppearanceGender();
  Check(Character->ApplyCharacterProfile(Profile),TEXT("normal_character_profile_failed"));
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();auto* ASC=State?State->GetAbilitySystemComponent():nullptr;
  Check(ASC&&ASC->GetOwnerActor()==State&&ASC->GetAvatarActor()==Character.Get()&&ASC->GetSet<URogue10mAttributeSet>()==Character->GetRogueAttributeSet(),TEXT("normal_possession_ASC_registration_failed"));
  Character->SetCanBeDamaged(false);Character->DisableInput(Controller.Get());
  Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
  {It->ClearAITarget();if(It->GetController()){It->GetController()->UnPossess();}It->SetActorEnableCollision(false);It->GetCharacterMovement()->DisableMovement();}
  auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();
  Check(Character->IsThirdPersonGameplayActive()&&Body&&Body->IsVisible()&&!Body->bOwnerNoSee,TEXT("default_third_person_rendered_body_required"));
  Check(Source&&Source!=Body&&Source==Character->GetAnimationSourceMesh()&&Source->GetSkeletalMeshAsset()&&GetPathNameSafe(Source->GetSkeletalMeshAsset()->GetSkeleton())==MannySkeleton,TEXT("native_Manny_source_skeleton_required"));
  Check(Source&&GetPathNameSafe(Source->GetAnimClass())==CommonClass,TEXT("existing_Common_Unarmed_source_ABP_required"));
  if(!Body||!Source||!Anim()){Check(false,TEXT("animation_chain_missing"));Finish();return;}
  for(const FName Bone:{FName(TEXT("hand_r")),FName(TEXT("hand_l")),FName(TEXT("middle_01_r")),FName(TEXT("middle_01_l"))})
  {Check(Body->GetBoneIndex(Bone)!=INDEX_NONE&&Source->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("hand_and_palm_proxy_bones_required"));}
  Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
  if(!Visual.IsValid()){Check(false,TEXT("native_visual_component_missing"));Finish();return;}
  const auto* Defaults=GetDefault<URogue10mWeaponVisualComponent>();DefaultStaff=Defaults->StaffMesh.ToSoftObjectPath();DefaultGrip=Defaults->StaffGrip;DefaultUpright=Defaults->bStaffUprightIdle;
  Check(!Visual->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&Visual->GetOwner()==Character.Get(),TEXT("QA_override_must_be_runtime_instance"));
  Check(Visual->StaffMesh.ToSoftObjectPath()==FSoftObjectPath(StaffPath)&&Visual->StaffGrip.Equals(CandidateGrip(),.000001)&&Visual->bStaffUprightIdle,
   TEXT("actual_BP_must_inherit_native_staff_defaults_without_instance_override"));
  // A synthetic owned QA item exercises the real inventory transaction, never a production save/item asset.
  auto* Inventory=Character->GetInventoryComponent();
  if(!Inventory){Check(false,TEXT("inventory_missing"));Finish();return;}
  QAStaffData=TStrongObjectPtr<URogue10mItemDataAsset>(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));
  QAStaffData->ItemId=TEXT("StaffPresentationQA");QAStaffData->Category=ERogue10mItemCategory::Equipment;
  QAStaffData->EquipSlotType=ERogue10mInventorySlotType::MainWeapon;QAStaffData->WeaponType=ERogue10mWeaponType::Staff;
  QAStaffData->DisplayName=FText::FromString(TEXT("StaffPresentationQA"));
  int32 Container=INDEX_NONE;FGuid ItemId;
  const bool bAdded=Inventory->TryAddGridItem(QAStaffData.Get(),1,Container,ItemId);
  Check(bAdded&&Inventory->TryEquipGridItem(Container,ItemId),TEXT("real_inventory_staff_transaction_failed"));
  bool bExact=false;
  for(const auto& Equipped:Inventory->GetLeftEquipmentSlots())
  {if(Equipped.SlotType==ERogue10mInventorySlotType::MainWeapon){bExact=Equipped.bHasEquippedItem&&Equipped.EquippedItem.InstanceId==ItemId&&Equipped.EquippedItem.ItemData==QAStaffData.Get();}}
  Check(bExact&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Staff,TEXT("inventory_exact_staff_instance_required"));
  bInventoryEquip=bExact;Visual->RefreshWeaponVisuals();ValidateVisual();Character->GetCharacterMovement()->DisableMovement();
  Check(GetPathNameSafe(Body->GetAnimClass())==RetargetClass,TEXT("normal_HumanMale_retarget_ABP_required"));
  for(int32 I=0;I<2;++I)
  {
   const FString Name=I==0?TEXT("A_Staff_StaffStrike_Manny"):TEXT("A_Staff_StaffShot_Manny");
   const FString Path=TEXT("/Game/Rogue10m/Animation/Combat/Staff20261110/StaffResumeC/")+Name+TEXT(".")+Name;
   auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path);Sequences.Emplace(Sequence);
   const int32 Tracks=Sequence&&Sequence->GetDataModel()?Sequence->GetDataModel()->GetNumBoneTracks():-1;
   Check(Sequence&&Sequence->GetClass()==UAnimSequence::StaticClass()&&GetPathNameSafe(Sequence->GetSkeleton())==MannySkeleton&&Tracks==89,TEXT("exact_sequence_class_Manny_skeleton_89_tracks_required"));
   Check(Sequence&&FMath::IsNearlyEqual(Sequence->GetPlayLength(),I==0?2.f:2.9f,.001f)&&!Sequence->bEnableRootMotion,TEXT("full_clip_length_and_no_root_motion_required"));
   UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION ASSET path=%s class=%s skeleton=%s tracks=%d duration=%.6f root_motion=%d"),*Path,*GetNameSafe(Sequence?Sequence->GetClass():nullptr),*GetPathNameSafe(Sequence?Sequence->GetSkeleton():nullptr),Tracks,Sequence?Sequence->GetPlayLength():-1.f,Sequence&&Sequence->bEnableRootMotion?1:0);
  }
  if(Failures){Finish();return;}
  const FBox Bounds=Visual->GetRightWeaponMesh()->GetStaticMesh()->GetBoundingBox();
  Check(Bounds.IsValid&&Bounds.GetSize().Y>Bounds.GetSize().X&&Bounds.GetSize().Y>Bounds.GetSize().Z,TEXT("verified_model_local_Y_shaft_axis_required"));
  ShaftLocalA=FVector(0,Bounds.Min.Y,0);ShaftLocalB=FVector(0,Bounds.Max.Y,0);
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION FIXTURE profile_valid=1 ASC_initialized=1 native_visual_transient=1 shaft_proxy=local_Y_bounds_centerline right_palm_limit_cm=15 production_default=%s"),*DefaultStaff.ToString());
  if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
  OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);bOwnFixed=true;
  ScreenshotProcessed=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->CaptureProcessed();});
  Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
  StageAt=World->GetTimeSeconds();
 }
 void ValidateVisual()
 {
  auto* Right=Visual.IsValid()?Visual->GetRightWeaponMesh():nullptr;auto* Left=Visual.IsValid()?Visual->GetLeftWeaponMesh():nullptr;
  Check(Character.IsValid()&&Character->IsThirdPersonGameplayActive(),TEXT("third_person_changed"));
  Check(Right&&Right->IsVisible()&&!Right->bHiddenInGame&&Right->IsRegistered()&&Right->GetAttachParent()==Character->GetMesh()&&Right->GetAttachSocketName()==TEXT("hand_r")&&GetPathNameSafe(Right->GetStaticMesh())==StaffPath,TEXT("native_staff_visible_asset_parent_socket_required"));
  Check(Right&&Right->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!Right->GetGenerateOverlapEvents(),TEXT("staff_must_not_own_collision"));
  Check(!Left||!Left->IsVisible()||Left->bHiddenInGame,TEXT("offhand_must_be_hidden"));
  Check(Right&&Right->GetRelativeTransform().GetLocation().Equals(CandidateGrip().GetLocation(),.0001)
   &&Right->GetRelativeTransform().GetScale3D().Equals(CandidateGrip().GetScale3D(),.0001),TEXT("staff_pivot_or_scale_changed"));
 }
 void FixedCamera()
 {
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
  Camera=World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),P);
  if(!Camera.IsValid()){Check(false,TEXT("review_camera_spawn_failed"));return;}
  const FVector Target=InitialTransform.GetLocation()+FVector(0,0,5);
  const FVector Location=InitialTransform.TransformPositionNoScale(FVector(350,300,100));
  Camera->SetActorLocationAndRotation(Location,(Target-Location).Rotation());Camera->GetCameraComponent()->SetFieldOfView(60.f);
  Controller->SetViewTarget(Camera.Get());bFixedCamera=true;
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION CAMERA fixture_only=1 location=%s target=%s fov=60 body_transform_unchanged=1"),*Location.ToString(),*Target.ToString());
 }
 bool Capture(const TCHAR* Phase)
 {
  if(bCapturePending||FScreenshotRequest::IsScreenshotRequested()){Check(false,TEXT("overlapping_screenshot_request"));return false;}
  if(FString(Phase)==TEXT("ready")||FString(Phase)==TEXT("recovery"))
  {
   auto* Right=Visual->GetRightWeaponMesh();
   const double Up=Right?FVector::DotProduct(Right->GetComponentQuat().RotateVector(FVector::YAxisVector),FVector::UpVector):-1.;
   Check(Up>.995,TEXT("idle_recovery_staff_jewel_must_point_up"));
   UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION IDLE_UP case=%s phase=%s dot=%.6f"),CaseName(),Phase,Up);
  }
  PendingCase=CaseName();PendingPhase=Phase;PendingCamera=bFixedCamera?TEXT("fixed_front_side"):TEXT("default_third_person");
  PendingPath=FPaths::ConvertRelativePathToFull(Images/(FString::Printf(TEXT("%02d_"),CapturePaths.Num()+1)+PendingCase+TEXT("_")+Phase+TEXT("_")+RunId+TEXT(".png")));
  if(IFileManager::Get().FileExists(*PendingPath)){Check(false,TEXT("capture_path_must_be_fresh"));return false;}
  bCapturePending=true;CaptureWall=FPlatformTime::Seconds();CapturePaths.Add(PendingPath);CaptureRow(TEXT("request"),-1);
  FScreenshotRequest::RequestScreenshot(PendingPath,true,false);return true;
 }
 void CaptureRow(const TCHAR* Event,int64 Bytes)
 {
  Captures+=FString::Printf(TEXT("%s,%s,%s,%s,%d,%.6f,%.6f,%d,%s,%lld\n"),Event,*PendingCase,*PendingPhase,*PendingCamera,Frame,World->GetTimeSeconds(),Position(),Playing()?1:0,*PendingPath,static_cast<long long>(Bytes));
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION CAPTURE_%s case=%s phase=%s camera=%s frame=%d montage=%.6f playing=%d bytes=%lld file=%s"),Event,*PendingCase,*PendingPhase,*PendingCamera,Frame,Position(),Playing()?1:0,static_cast<long long>(Bytes),*PendingPath);
 }
 void CaptureProcessed()
 {
  if(!bCapturePending){return;}const int64 Bytes=IFileManager::Get().FileSize(*PendingPath);
  CaptureRow(TEXT("processed"),Bytes);Check(Bytes>=1024,TEXT("processed_capture_missing_or_too_small"));
  if(Bytes>=1024){++CompletedCaptures;}bCapturePending=false;if(Bytes<1024){bFinishRequested=true;}
 }
 void BeginClip()
 {
  auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();
  BodyReadyHand=Body->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation();
  SourceReadyHand=Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation();
  BodyTravel=SourceTravel=MaxRightGap=MaxPosition=0.;PlayingSamples=ProgressSamples=FullWeightSamples=0;PreviousPosition=-1.f;
  // Existing native hidden source ABP and its DefaultSlot stay in control; no AnimBP, skill or montage asset edits.
  Montage=Anim()->PlaySlotAnimationAsDynamicMontage(Sequences[CaseIndex].Get(),TEXT("DefaultSlot"),.10f,.15f,1.f,1,-1.f,0.f);
  Check(Montage.IsValid()&&Playing(),TEXT("dynamic_DefaultSlot_montage_not_playing"));
  ClipStarted=World->GetTimeSeconds();
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION PLAY case=%s source=%s montage=%s slot=DefaultSlot rate=1"),CaseName(),*Sequences[CaseIndex]->GetPathName(),*GetPathNameSafe(Montage.Get()));
 }
 void Observe()
 {
  ValidateVisual();auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();auto* Right=Visual->GetRightWeaponMesh();
  if(!Right){return;}
  const bool bPlaying=Playing();const float Pos=Position();
  const FVector RH=Body->GetSocketLocation(TEXT("hand_r")),LH=Body->GetSocketLocation(TEXT("hand_l"));
  const FVector RP=(RH+Body->GetSocketLocation(TEXT("middle_01_r")))*.5,LP=(LH+Body->GetSocketLocation(TEXT("middle_01_l")))*.5;
  const FVector A=Right->GetComponentTransform().TransformPosition(ShaftLocalA),B=Right->GetComponentTransform().TransformPosition(ShaftLocalB);
  const double RG=FVector::Distance(RP,FMath::ClosestPointOnSegment(RP,A,B)),LG=FVector::Distance(LP,FMath::ClosestPointOnSegment(LP,A,B));
  const double Shift=FVector::Distance(Character->GetActorLocation(),InitialTransform.GetLocation());
  const double Angle=FMath::RadiansToDegrees(Character->GetActorQuat().AngularDistance(InitialTransform.GetRotation()));
  Check(Shift<=1.&&Angle<=.1,TEXT("actor_movement_or_rotation_guard_failed"));
  Check(!A.ContainsNaN()&&!B.ContainsNaN()&&FVector::Distance(A,B)>100.&&FMath::IsFinite(RG)&&FMath::IsFinite(LG),TEXT("finite_shaft_and_palm_proxy_required"));
  const float Weight=Anim()->GetSlotMontageGlobalWeight(TEXT("DefaultSlot"));
  const double Up=FVector::DotProduct(Right->GetComponentQuat().RotateVector(FVector::YAxisVector),FVector::UpVector);
  const double RotationError=FMath::RadiansToDegrees(Right->GetRelativeTransform().GetRotation().AngularDistance(CandidateGrip().GetRotation()));
  Check(RG<=15.,TEXT("all_phases_right_palm_shaft_proxy_exceeds_15cm"));
  Check(FMath::IsFinite(Weight)&&FMath::IsFinite(Up)&&FMath::IsFinite(RotationError),TEXT("finite_staff_orientation_required"));
  if(Weight>=.9999f){++FullWeightSamples;Check(RotationError<.1,TEXT("full_weight_montage_authored_grip_required"));}
  if(bPlaying)
  {
   ++PlayingSamples;if(PreviousPosition>=0&&Pos>PreviousPosition+.0001f){++ProgressSamples;}
   Check(PreviousPosition<0||Pos+.001f>=PreviousPosition,TEXT("montage_position_went_backwards"));PreviousPosition=Pos;MaxPosition=FMath::Max(MaxPosition,static_cast<double>(Pos));
   BodyTravel=FMath::Max(BodyTravel,FVector::Distance(BodyReadyHand,Body->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation()));
   SourceTravel=FMath::Max(SourceTravel,FVector::Distance(SourceReadyHand,Source->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation()));
   MaxRightGap=FMath::Max(MaxRightGap,RG);Check(RG<=15.,TEXT("right_palm_to_model_shaft_proxy_exceeds_15cm"));
  }
  Frames+=FString::Printf(TEXT("%d,%.6f,%s,%d,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%s,%.6f,%.6f,%.6f\n"),
   Frame,World->GetTimeSeconds(),CaseName(),Stage,Pos,bPlaying?1:0,BodyTravel,SourceTravel,RG,LG,Shift,Angle,RH.X,RH.Y,RH.Z,RP.X,RP.Y,RP.Z,A.X,A.Y,A.Z,B.X,B.Y,B.Z,*GetPathNameSafe(Body->GetAnimClass()),*GetPathNameSafe(Source->GetAnimClass()),Weight,Up,RotationError);
 }
 void CompleteClip()
 {
  Check(FullWeightSamples>=10&&PlayingSamples>=20&&ProgressSamples>=15&&MaxPosition>=Sequences[CaseIndex]->GetPlayLength()-.3,TEXT("continuous_clip_progress_not_proven"));
  Check(BodyTravel>2.&&SourceTravel>2.,TEXT("rendered_and_source_hand_motion_required"));
  UE_LOG(LogRogue10m,Display,TEXT("STAFF_PRESENTATION COMPLETE case=%s playing_samples=%d progression_samples=%d max_position=%.6f body_hand_travel_cm=%.6f source_hand_travel_cm=%.6f max_right_palm_shaft_cm=%.6f"),CaseName(),PlayingSamples,ProgressSamples,MaxPosition,BodyTravel,SourceTravel,MaxRightGap);
  ++CompletedClips;
 }
 void Step()
 {
  if(bFinished){return;}++Frame;const double Wall=FPlatformTime::Seconds();
  if(bCapturePending&&Wall-CaptureWall>30.){Check(false,TEXT("screenshot_callback_timeout"));CaptureRow(TEXT("timeout"),-1);FScreenshotRequest::Reset();bCapturePending=false;bFinishRequested=true;}
  if(Wall-StartedWall>150.||Frame>5400){Check(false,TEXT("bounded_fixture_timeout"));bFinishRequested=true;}
  if(bFinishRequested){Finish();return;}
  if(!Character.IsValid()||!Controller.IsValid()||!Visual.IsValid()||!Anim()){Check(false,TEXT("fixture_lost"));Finish();return;}
  Observe();if(Failures){Finish();return;}if(bCapturePending){return;}
  const double Now=World->GetTimeSeconds();
  // Each stage advances only after its prior real screenshot callback and non-empty file.
  if(Stage==0&&Now-StageAt>=1.){Check(!Anim()->IsAnyMontagePlaying(),TEXT("ready_must_be_unarmed_locomotion_idle"));if(Capture(TEXT("ready"))){Stage=1;}return;}
  if(Stage==1){FixedCamera();StageAt=Now;Stage=2;return;}
  if(Stage==2&&Now-StageAt>=.5){BeginClip();Stage=3;return;}
  if(Stage==3&&Playing()&&Position()>=(CaseIndex==0?.20f:.30f)){if(Capture(TEXT("windup"))){Stage=4;}return;}
  if(Stage==4&&Playing()&&Position()>=(CaseIndex==0?.80f:1.20f)){if(Capture(TEXT("action"))){Stage=5;}return;}
  if(Stage==5&&!Playing()&&Now-ClipStarted>=Sequences[CaseIndex]->GetPlayLength()+.35)
  {CompleteClip();if(Capture(TEXT("recovery"))){Stage=6;}return;}
  if(Stage==6)
  {
   if(CaseIndex==0){CaseIndex=1;Montage.Reset();StageAt=Now;Stage=7;return;}
   CheckReturns();Finish();return;
  }
  if(Stage==7&&Now-StageAt>=.5){Check(!Anim()->IsAnyMontagePlaying(),TEXT("shot_ready_must_be_idle"));if(Capture(TEXT("ready"))){Stage=8;}return;}
  if(Stage==8){BeginClip();Stage=3;return;}
  if(Stage>=3&&Stage<=5&&Now-ClipStarted>Sequences[CaseIndex]->GetPlayLength()+3.){Check(false,TEXT("clip_phase_capture_timeout"));Finish();}
 }
 void CheckReturns()
 {
  // Cheap regression checks only. These are restore-path presentation checks, not gameplay equip tests.
  Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);Visual->RefreshWeaponVisuals();
  auto* R=Visual->GetRightWeaponMesh();auto* L=Visual->GetLeftWeaponMesh();
  Check(R&&R->IsVisible()&&R->GetStaticMesh()==Visual->GreatSwordMesh.Get()&&(!L||!L->IsVisible()),TEXT("return_sword_presentation_failed"));
  Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);Visual->RefreshWeaponVisuals();R=Visual->GetRightWeaponMesh();L=Visual->GetLeftWeaponMesh();
  Check(R&&L&&R->IsVisible()&&L->IsVisible()&&R->GetStaticMesh()==Visual->KnuckleMesh.Get()&&L->GetStaticMesh()==Visual->KnuckleMesh.Get(),TEXT("return_knuckle_presentation_failed"));
  Check(GetPathNameSafe(Character->GetMesh()->GetAnimClass())==RetargetClass,TEXT("return_restores_HumanMale_retarget_class"));
  bReturnChecks=true;
 }
 void Finish()
 {
  if(bFinished){return;}if(bCapturePending){bFinishRequested=true;return;}bFinished=true;
  FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessed);
  if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
  Check(bInventoryEquip&&CapturePaths.Num()==8&&CompletedCaptures==8&&CompletedClips==2&&bReturnChecks,TEXT("strict_eight_captures_two_clips_and_return_checks_required"));
  if(Visual.IsValid()){const auto* D=GetDefault<URogue10mWeaponVisualComponent>();Check(D->StaffMesh.ToSoftObjectPath()==DefaultStaff&&D->StaffGrip.Equals(DefaultGrip,.000001)&&D->bStaffUprightIdle==DefaultUpright,TEXT("production_native_defaults_changed"));}
  for(const FString& Path:CapturePaths){Check(IFileManager::Get().FileSize(*Path)>=1024,TEXT("final_capture_missing"));}
  Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frames_save_failed"));
  Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))),TEXT("captures_save_failed"));
  const FString Summary=FString::Printf(TEXT("frames=%d\nfailures=%d\ncompleted_clips=%d\nrequested_captures=%d\ncompleted_captures=%d\nreturn_checks=%d\ncombat_request=0\ninventory_equip_proof=%d\ndamage_proof=0\nfrozen_pose=0\nvisual_approval=0\nnative_default_integration=1\nNOTE=Actual game HumanMale BP, normal possession/Profile/ASC and appearance materials. Synthetic owned Staff item equipped through the actual inventory transaction; actual Blueprint character inherits native Staff mesh/grip/upright defaults without an instance override. Opt-in upright idle uses actual DefaultSlot global weight. Existing Common_Unarmed source ABP DefaultSlot plays two full continuous Manny sequences into the normal HumanMale retarget body. First ready uses default third-person camera; seven subsequent frames use a fixed fixture-only front-side camera. Actor transform is guarded, no camera/body transform tuning or first-person support. Eight captures require actual async screenshot completion and files. Request and processed montage times are distinct; screenshots are deferred and do not certify exact animation pose timing. Recovery is after natural montage end and blend-out. Hand motion is component-local, not camera motion. Shaft metric is model-local Y bounding-centerline to right hand/middle-finger midpoint, limit 15cm; left hand is recorded only. This is a coarse attachment proxy, not geometry contact, a grip approval, combat input, damage or production item/save-restart proof. Source and retarget bones may have different evaluation age. PNG visual review is still required. No animation/montage/skill/Blueprint or source package is saved.\n"),Frame,Failures,CompletedClips,CapturePaths.Num(),CompletedCaptures,bReturnChecks?1:0,bInventoryEquip?1:0);
  Check(FFileHelper::SaveStringToFile(Summary,*(Output/TEXT("metrics.txt"))),TEXT("metrics_save_failed"));
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=STAFF_PRESENTATION_%s completed_clips=%d captures=%d failures=%d"),Failures?TEXT("FAILED"):TEXT("PASSED"),CompletedClips,CompletedCaptures,Failures);
  FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mStylizedCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;
 TWeakObjectPtr<URogue10mWeaponVisualComponent> Visual;TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<UAnimMontage> Montage;
 TStrongObjectPtr<URogue10mItemDataAsset> QAStaffData;
 TArray<TStrongObjectPtr<UAnimSequence>> Sequences;FTimerHandle PrepareTimer;FDelegateHandle Tick,ScreenshotProcessed;
 FString Output,Images,RunId,Frames,Captures,PendingCase,PendingPhase,PendingCamera,PendingPath;TArray<FString> CapturePaths;
 FSoftObjectPath DefaultStaff;FTransform InitialTransform,DefaultGrip;FVector ShaftLocalA,ShaftLocalB,BodyReadyHand,SourceReadyHand;
 int32 Failures=0,Frame=0,Stage=0,CaseIndex=0,CompletedCaptures=0,CompletedClips=0,PlayingSamples=0,ProgressSamples=0,FullWeightSamples=0;
 double StartedWall=0,CaptureWall=0,StageAt=0,ClipStarted=0,BodyTravel=0,SourceTravel=0,MaxRightGap=0,MaxPosition=0,OldDelta=0;float PreviousPosition=-1.f;
 bool bInventoryEquip=false,DefaultUpright=false;
 bool bFinished=false,bFinishRequested=false,bCapturePending=false,bFixedCamera=false,bOwnFixed=false,OldFixed=false,bReturnChecks=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
 if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->Finished())){return;}
 for(const FWorldContext& Context:GEngine->GetWorldContexts())
 {if(auto* W=Context.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestStaffPresentation"),TEXT("Isolated actual-game Staff presentation captures, not combat verification; includes synthetic-item inventory transaction."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
