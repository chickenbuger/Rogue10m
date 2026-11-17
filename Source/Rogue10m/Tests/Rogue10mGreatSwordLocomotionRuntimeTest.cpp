#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimClassInterface.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "UObject/UnrealType.h"
#include "Animation/BlendSpace.h"
#include "AssetCompilingManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
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
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mStylizedCharacter.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mGreatSwordLocomotionQA
{
static const FString Root=TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/");
static const FString OriginalBS=TEXT("/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference.BS_Unarmed_PalmReference");
class FRun : public TSharedFromThis<FRun>
{
public:
 bool Finished()const{return bFinished;}
 void Start(UWorld* W)
 {
  World=W;const TCHAR* Args=FCommandLine::Get();FString Town,Progression,Keys,Palm;
  FParse::Value(Args,TEXT("RogueSwordLocomotionLabel="),Label);
  FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Progression);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool bValid=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label){bValid&=FChar::IsAlnum(C)||C==TEXT('_');}
  bValid&=W->GetNetMode()==NM_Standalone&&!FParse::Param(Args,TEXT("nullrhi"));
  bValid&=Town==TEXT("Rogue10m_Town_QA_SwordMove_")+Label&&Progression==TEXT("Rogue10m_Progression_QA_SwordMove_")+Label&&Keys==TEXT("Rogue10m_Fighter_QA_SwordMove_")+Label;
  if(!bValid){UE_LOG(LogRogue10m,Error,TEXT("RESULT=GREATSWORD_LOCOMOTION_FAILED reason=rendered_exact_QA_namespace_required"));bFinished=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/sword-knuckle-20261110/locomotion")/Label;
  Images=FPaths::ProjectDir()/TEXT("DevLog/20261110_SwordKnuckle")/(TEXT("Locomotion_")+Label);
  if(IFileManager::Get().DirectoryExists(*Images)||IFileManager::Get().FileExists(*(Output/TEXT("metrics.txt"))))
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=GREATSWORD_LOCOMOTION_FAILED reason=fresh_evidence_required"));bFinished=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
  IFileManager::Get().MakeDirectory(*Output,true);IFileManager::Get().MakeDirectory(*Images,true);
  FParse::Value(Args,TEXT("RogueSwordHiltMinZ="),HiltMinZ);FParse::Value(Args,TEXT("RogueSwordHiltMaxZ="),HiltMaxZ);
  if(FParse::Value(Args,TEXT("RogueSwordPalmR="),Palm)){PalmR=FName(Palm);}if(FParse::Value(Args,TEXT("RogueSwordPalmL="),Palm)){PalmL=FName(Palm);}
  Check(FMath::IsFinite(HiltMinZ)&&FMath::IsFinite(HiltMaxZ)&&HiltMinZ<HiltMaxZ,TEXT("hilt_axis_parameters_valid"));
  Rows=TEXT("frame,case,world_s,age_s,desired_cm_s,speed_cm_s,direction_dot,grounded,stance_alpha,foot_r_gap_cm,foot_l_gap_cm,body_foot_travel_cm,source_foot_travel_cm,palm_r_axis_cm,palm_l_axis_cm,palm_r_x,palm_r_y,palm_r_z,palm_l_x,palm_l_y,palm_l_z,hilt_a_x,hilt_a_y,hilt_a_z,hilt_b_x,hilt_b_y,hilt_b_z,palm_r_hilt_local_z,palm_l_hilt_local_z,hand_span_cm\n");
  Captures=TEXT("event,case,frame,world_s,file,bytes\n");
  W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
 }
private:
 void Check(bool bValue,const TCHAR* Why){if(!bValue){++Failures;UE_LOG(LogRogue10m,Error,TEXT("SWORDMOVE FAIL case=%d frame=%d %s"),Case,Frame,Why);}}
 URogue10mFighterCombatStanceAnimInstance* Native() const
 {auto* Mesh=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;return Mesh?Cast<URogue10mFighterCombatStanceAnimInstance>(Mesh->GetAnimInstance()):nullptr;}
 bool Moving()const{return Case>=1&&Case<=18;}
 bool Sword()const{return Case<=16||Case==19;}
 void Move(float Right,float Forward)
 {
  struct FMoveArgs { float Right; float Forward; } Args{Right,Forward};
  Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&Args);
 }
 void StopSprint()
 {
  Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoSprintEnd")),nullptr);
 }
 void Prepare()
 {
  PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());APawn* Old=PC.IsValid()?PC->GetPawn():nullptr;
  UClass* Class=LoadClass<ARogue10mStylizedCharacter>(nullptr,TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
  Check(Old&&Class,TEXT("human_male_class_and_controller"));if(Failures){Finish();return;}
  PC->CloseAllBlockingPanels();const FTransform Initial=Old->GetActorTransform();Old->SetActorEnableCollision(false);Old->SetActorHiddenInGame(true);
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Character=World->SpawnActor<ARogue10mStylizedCharacter>(Class,Initial,P);
  Check(Character.IsValid(),TEXT("human_male_spawned"));if(Failures){Finish();return;}
  PC->Possess(Character.Get());PC->SetViewTarget(Character.Get());
  FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("SwordMoveQA");Profile.Appearance.Race=Character->GetAppearanceRace();Profile.Appearance.Gender=Character->GetAppearanceGender();
  Check(Character->ApplyCharacterProfile(Profile),TEXT("human_male_profile_applied"));Character->SetCanBeDamaged(false);
  Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Unarmed);
  OriginalBodyClass=Character->GetMesh()->GetAnimClass();OriginalBodyMesh=Character->GetMesh()->GetSkeletalMeshAsset();
  Check(OriginalBodyClass.IsValid()&&!OriginalBodyClass->GetPathName().Contains(TEXT("ABP_Human_GreatSword")),TEXT("original_body_retarget_captured"));
  Check(Native()!=nullptr,TEXT("native_stance_source_exists"));
  // Isolated flat surface: move through CharacterMovement, never set its Velocity or play a pose directly.
  const FVector Start=Character->GetActorLocation();FloorZ=Start.Z+1800.;
  AActor* Floor=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(FVector(Start.X,Start.Y,FloorZ-20.)),P);
  Check(Floor!=nullptr,TEXT("transient_ground_spawned"));if(Failures){Finish();return;}
  auto* Shape=NewObject<UBoxComponent>(Floor,NAME_None,RF_Transient);Floor->AddInstanceComponent(Shape);Floor->SetRootComponent(Shape);Shape->SetBoxExtent(FVector(3500,3500,20));Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Shape->SetCollisionObjectType(ECC_WorldStatic);Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->SetCanEverAffectNavigation(false);Shape->RegisterComponent();Floor->SetActorLocation(FVector(Start.X,Start.Y,FloorZ-20.));
  auto* FloorMesh=NewObject<UStaticMeshComponent>(Floor,NAME_None,RF_Transient);Floor->AddInstanceComponent(FloorMesh);FloorMesh->SetupAttachment(Shape);FloorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));FloorMesh->SetRelativeScale3D(FVector(70,70,.4));FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);FloorMesh->RegisterComponent();
  Origin=FVector(Start.X,Start.Y,FloorZ+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.);
  PC->SetControlRotation(FRotator(-12,0,0));Character->SetActorRotation(FRotator::ZeroRotator);
  FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
  OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);bOwnFixed=true;
  ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Self=AsShared()]{Self->Processed();});
  PreHandle=FWorldDelegates::OnWorldPreActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get())Self->Input();});
  PostHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get())Self->Step();});
  Deadline=FPlatformTime::Seconds()+180.;BeginCase();
 }
 void BeginCase()
 {
  if(Case>=20){Finish();return;}
  const TCHAR* Directions[]={TEXT("F"),TEXT("FR"),TEXT("R"),TEXT("BR"),TEXT("B"),TEXT("BL"),TEXT("L"),TEXT("FL")};
  CaseName=Case==0?TEXT("sword_idle"):Case==17?TEXT("knuckle_restore"):Case==18?TEXT("unarmed_restore"):Case==19?TEXT("sword_return"):FString(Case<=8?TEXT("walk_"):TEXT("run_"))+Directions[(Case-1)%8];
  StopSprint();Character->GetCharacterMovement()->StopMovementImmediately();Move(0,0);Character->GetCharacterAnimationComponent()->SetMovementState(false,false);
  Character->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics);Character->SetActorRotation(FRotator::ZeroRotator);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Character->RestoreEquippedWeaponType(Sword()?ERogue10mWeaponType::GreatSword:Case==17?ERogue10mWeaponType::Knuckle:ERogue10mWeaponType::Unarmed);
  DesiredSpeed=Moving()?((Case<=8||Case>=17)?300.f:600.f):0.f;
  const double Angle=Case>=1&&Case<=16?FMath::DegreesToRadians(double((Case-1)%8)*45.):0.;Direction=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0);
  CaseStart=World->GetTimeSeconds();Samples=0;SpeedSum=0;MinDot=1.;MaxBodyTravel=MaxSourceTravel=0.;bPoseSeeded=false;bRequested=bProcessed=false;
  UE_LOG(LogRogue10m,Display,TEXT("SWORDMOVE BEGIN case=%s desired=%.1f"),*CaseName,DesiredSpeed);
 }
 void Input()
 {
  if(bFinished||!Character.IsValid()||Case>=20){return;}
  const float Cap=Character->GetCharacterMovement()->MaxWalkSpeed;
  const float Scale=Moving()&&Cap>0?FMath::Clamp(DesiredSpeed/Cap,0.f,1.f):0.f;
  Move(Direction.Y*Scale,Direction.X*Scale);
  Character->GetCharacterAnimationComponent()->SetMovementState(Moving(),false);
 }
 void Paths()
 {
  auto* Anim=Native();auto* Body=Character->GetMesh();Check(Anim&&Anim->GroundLocomotionBlendSpace&&Anim->FighterCombatStanceAnimation,TEXT("native_weapon_assets_available"));if(!Anim||!Anim->GroundLocomotionBlendSpace||!Anim->FighterCombatStanceAnimation){return;}
  Check(Anim->GroundLocomotionBlendSpace->GetPathName()==(Sword()?Root+TEXT("BS_GreatSword_Ground.BS_GreatSword_Ground"):OriginalBS),TEXT("native_ground_blendspace_path"));
  Check(Anim->FighterCombatStanceAnimation->GetPathName()==(Sword()?Root+TEXT("A_GreatSword_Idle.A_GreatSword_Idle"):TEXT("/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle.A_Fighter_CombatIdle")),TEXT("native_stance_idle_path"));
  Check(Sword()?Body->GetAnimClass()&&Body->GetAnimClass()->GetPathName()==Root+TEXT("ABP_Human_GreatSword.ABP_Human_GreatSword_C"):Body->GetAnimClass()==OriginalBodyClass.Get()&&Body->GetSkeletalMeshAsset()==OriginalBodyMesh.Get(),TEXT("weapon_specific_body_retarget_or_original_restored"));
  if(Case==0)
  {
   auto* Original=LoadObject<UBlendSpace>(nullptr,*OriginalBS);const auto& SamplesData=Anim->GroundLocomotionBlendSpace->GetBlendSamples();Check(Original&&SamplesData.Num()==27&&Original->GetBlendSamples().Num()==27,TEXT("exact_27_locomotion_coordinates"));
   TSet<FString> UniqueAnimations;FString Catalog=TEXT("index,x,y,z,animation\n");
   for(int32 I=0;I<SamplesData.Num();++I)
   {
    const auto& S=SamplesData[I];const FString Path=S.Animation?S.Animation->GetPathName():TEXT("None");
    UniqueAnimations.Add(Path);
    Check(S.Animation&&Path.StartsWith(Root+TEXT("A_Armed_"))&&S.Animation->GetSkeleton()==Anim->GroundLocomotionBlendSpace->GetSkeleton(),TEXT("task_owned_armed_sample_and_skeleton"));
    if(Original&&Original->GetBlendSamples().IsValidIndex(I)){const auto& O=Original->GetBlendSample(I);Check(S.SampleValue.Equals(O.SampleValue,.001),TEXT("original_direction_speed_coordinates_preserved"));if(O.Animation){const FString Expected=TEXT("A_Armed_")+O.Animation->GetName();Check(S.Animation&&S.Animation->GetName()==Expected,TEXT("exact_corresponding_armed_derivative"));}}
    Catalog+=FString::Printf(TEXT("%d,%.3f,%.3f,%.3f,%s\n"),I,S.SampleValue.X,S.SampleValue.Y,S.SampleValue.Z,*Path);
   }
   Check(UniqueAnimations.Num()==17,TEXT("exact_17_unique_armed_derivatives"));
   Check(FFileHelper::SaveStringToFile(Catalog,*(Output/TEXT("samples.csv"))),TEXT("sample_catalog_saved"));
  }
  UE_LOG(LogRogue10m,Display,TEXT("SWORDMOVE PATH case=%s body=%s source=%s stance=%s blendspace=%s"),*CaseName,*GetPathNameSafe(Body->GetAnimClass()),*GetPathNameSafe(Character->GetAnimationPlaybackMesh()->GetAnimClass()),*Anim->FighterCombatStanceAnimation->GetPathName(),*Anim->GroundLocomotionBlendSpace->GetPathName());
 }
 void ObserveBlendPlayer()
 {
  auto* Anim=Native();const auto* Interface=Anim?IAnimClassInterface::GetFromClass(Anim->GetClass()):nullptr;bool bActiveExpected=false;
  if(Interface)for(const auto* Property:Interface->GetAnimNodeProperties())
  {
   if(!Property||!Property->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct())){continue;}
   const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(Anim);
   const float Weight=Node->GetCachedBlendWeight();const FVector Position=Node->GetPosition();
   if(Node->GetBlendSpace()==Anim->GroundLocomotionBlendSpace&&Weight>.01f){bActiveExpected=true;}
   UE_LOG(LogRogue10m,Display,TEXT("SWORDMOVE PLAYER case=%s property=%s asset=%s weight=%.6f x=%.6f y=%.6f time=%.6f"),*CaseName,*Property->GetName(),*GetPathNameSafe(Node->GetBlendSpace()),Weight,Position.X,Position.Y,Node->GetCurrentAssetTime());
  }
  // The native property alone is insufficient: require a contributing graph player too.
  if(Moving()){Check(bActiveExpected,TEXT("actual_graph_player_uses_expected_weapon_blendspace"));}
 }
 void Step()
 {
  if(bFinished){return;}++Frame;
  if(FPlatformTime::Seconds()>Deadline||Frame>12000){Check(false,TEXT("fixture_timeout"));Finish();return;}
  if(bRequested&&!bProcessed&&FPlatformTime::Seconds()-ShotAt>30.){Check(false,TEXT("capture_timeout"));FScreenshotRequest::Reset();bRequested=false;Finish();return;}
  if(!Character.IsValid()||!PC.IsValid()||!Native()){Check(false,TEXT("fixture_lost"));Finish();return;}
  if(bProcessed){++Case;BeginCase();return;}
  const double Now=World->GetTimeSeconds(),Age=Now-CaseStart;if(Age<.8){return;}
  auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();auto* Movement=Character->GetCharacterMovement();auto* Anim=Native();auto* Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();auto* Weapon=Visual?Visual->GetRightWeaponMesh():nullptr;auto* Left=Visual?Visual->GetLeftWeaponMesh():nullptr;
  const double Speed=Movement->Velocity.Size2D(),Dot=Moving()?FVector::DotProduct(Movement->Velocity.GetSafeNormal2D(),Direction):1.;
  const double Bottom=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const FVector RF=Body->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation(),LF=Body->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation();
  const FVector SR=Source->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation(),SL=Source->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation();
  if(!bPoseSeeded){SeedRF=RF;SeedLF=LF;SeedSR=SR;SeedSL=SL;bPoseSeeded=true;Paths();}
  MaxBodyTravel=FMath::Max(MaxBodyTravel,FMath::Max(FVector::Distance(RF,SeedRF),FVector::Distance(LF,SeedLF)));MaxSourceTravel=FMath::Max(MaxSourceTravel,FMath::Max(FVector::Distance(SR,SeedSR),FVector::Distance(SL,SeedSL)));
  const FVector PR=(Body->GetSocketLocation(TEXT("hand_r"))+Body->GetSocketLocation(PalmR))*.5,PL=(Body->GetSocketLocation(TEXT("hand_l"))+Body->GetSocketLocation(PalmL))*.5;
  const FTransform WT=Weapon?Weapon->GetComponentTransform():FTransform::Identity;
  const FVector A=WT.TransformPosition(FVector(0,0,HiltMinZ)),B=WT.TransformPosition(FVector(0,0,HiltMaxZ));
  const double RAxis=Sword()?FVector::Distance(PR,FMath::ClosestPointOnSegment(PR,A,B)):-1.,LAxis=Sword()?FVector::Distance(PL,FMath::ClosestPointOnSegment(PL,A,B)):-1.;
  const double RGap=Body->GetSocketLocation(TEXT("foot_r")).Z-Bottom,LGap=Body->GetSocketLocation(TEXT("foot_l")).Z-Bottom;
  Check(Character->IsThirdPersonGameplayActive()&&Body->IsVisible()&&!Body->bOwnerNoSee,TEXT("third_person_rendered_body_required"));
  Check(Movement->IsMovingOnGround(),TEXT("real_grounded_movement"));
  Check(!RF.ContainsNaN()&&!LF.ContainsNaN()&&!PR.ContainsNaN()&&!PL.ContainsNaN(),TEXT("rendered_pose_finite"));
  if(Sword()){Check(Weapon&&Weapon->IsVisible()&&Weapon->GetAttachParent()==Body&&Weapon->GetAttachSocketName()==TEXT("hand_r")&&(!Left||!Left->IsVisible()),TEXT("sword_body_attachment_visible"));}
  if(Case==17){Check(Weapon&&Left&&Weapon->IsVisible()&&Left->IsVisible(),TEXT("knuckle_both_visible"));}
  if(Case==18){Check((!Weapon||!Weapon->IsVisible())&&(!Left||!Left->IsVisible()),TEXT("unarmed_weapons_hidden"));}
  if(Samples==0){Check(Body->DoesSocketExist(PalmR)&&Body->DoesSocketExist(PalmL)&&Body->DoesSocketExist(TEXT("foot_r"))&&Body->DoesSocketExist(TEXT("foot_l")),TEXT("required_rendered_palm_and_foot_bones"));}
  ++Samples;SpeedSum+=Speed;MinDot=FMath::Min(MinDot,Dot);
  Rows+=FString::Printf(TEXT("%d,%s,%.6f,%.6f,%.3f,%.3f,%.6f,%d,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),Frame,*CaseName,Now,Age,DesiredSpeed,Speed,Dot,Movement->IsMovingOnGround()?1:0,Anim->FighterCombatStanceAlpha,RGap,LGap,MaxBodyTravel,MaxSourceTravel,RAxis,LAxis,PR.X,PR.Y,PR.Z,PL.X,PL.Y,PL.Z,A.X,A.Y,A.Z,B.X,B.Y,B.Z,Sword()?WT.InverseTransformPosition(PR).Z:0.,Sword()?WT.InverseTransformPosition(PL).Z:0.,FVector::Distance(PR,PL));
  if(Age>=1.8&&!bRequested)
  {
   Check(Samples>=30,TEXT("stable_pose_sample_window"));ObserveBlendPlayer();
   if(Moving()){Check(FMath::Abs(SpeedSum/Samples-DesiredSpeed)<DesiredSpeed*.15&&MinDot>.95,TEXT("actual_velocity_matches_direction_and_speed"));Check(MaxBodyTravel>2.&&MaxSourceTravel>2.,TEXT("source_and_rendered_body_stride_motion"));Check(Anim->FighterCombatStanceAlpha<.01f,TEXT("moving_pose_yields_idle_stance"));}
   else{Check(Speed<1.,TEXT("stationary_velocity"));Check(FMath::Min(RGap,LGap)>=0.&&FMath::Min(RGap,LGap)<=30.,TEXT("stationary_foot_marker_ground_band_0_30cm"));if(Sword()){Check(Anim->FighterCombatStanceAlpha>.99f,TEXT("native_sword_idle_stance_fully_raised"));}}
   UE_LOG(LogRogue10m,Display,TEXT("SWORDMOVE CASE case=%s samples=%d mean_speed=%.3f min_direction_dot=%.6f body_foot_travel=%.3f source_foot_travel=%.3f palm_r_axis_cm=%.3f palm_l_axis_cm=%.3f"),*CaseName,Samples,SpeedSum/Samples,MinDot,MaxBodyTravel,MaxSourceTravel,RAxis,LAxis);
   Capture();
  }
 }
 void Capture()
 {
  Check(!FScreenshotRequest::IsScreenshotRequested(),TEXT("no_competing_capture"));if(FScreenshotRequest::IsScreenshotRequested()){Finish();return;}
  ShotPath=FPaths::ConvertRelativePathToFull(Images/(CaseName+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".png")));ShotAt=FPlatformTime::Seconds();bRequested=true;++Requested;PathsRequested.Add(ShotPath);
  Captures+=FString::Printf(TEXT("request,%s,%d,%.6f,%s,-1\n"),*CaseName,Frame,World->GetTimeSeconds(),*ShotPath);FScreenshotRequest::RequestScreenshot(ShotPath,true,false);
 }
 void Processed()
 {
  if(!bRequested||bProcessed||bFinished){return;}const int64 Bytes=IFileManager::Get().FileSize(*ShotPath);Check(Bytes>=1024,TEXT("processed_png_exists_nonempty"));
  Captures+=FString::Printf(TEXT("processed,%s,%d,%.6f,%s,%lld\n"),*CaseName,Frame,World->GetTimeSeconds(),*ShotPath,static_cast<long long>(Bytes));bProcessed=true;if(Bytes>=1024){++Completed;}
 }
 void Finish()
 {
  if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPreActorTick.Remove(PreHandle);FWorldDelegates::OnWorldPostActorTick.Remove(PostHandle);FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
  if(Character.IsValid()){Move(0,0);Character->GetCharacterMovement()->StopMovementImmediately();}
  if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
  Check(Case==20&&Requested==20&&Completed==20,TEXT("all_20_cases_and_processed_captures"));for(const auto& Path:PathsRequested){Check(IFileManager::Get().FileSize(*Path)>=1024,TEXT("final_png_retained"));}
  Check(FFileHelper::SaveStringToFile(Rows,*(Output/TEXT("frames.csv"))),TEXT("frames_saved"));Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))),TEXT("captures_saved"));
  const FString Summary=FString::Printf(TEXT("failures=%d\ncompleted_cases=%d\nrequested_captures=%d\ncompleted_captures=%d\npalm_r=%s\npalm_l=%s\nhilt_local_z_min=%.3f\nhilt_local_z_max=%.3f\nNOTE=Programmatic DoMove analog input through real CharacterMovement, not physical input or full-run gameplay. 300/600cm/s are walk/run blendspace probes, not sprint. Knuckle/Unarmed restoration cases also walk forward at 300cm/s to verify actual original graph player contribution. Native source assets plus rendered HumanMale pose travel are checked; screenshots still require visual review. Palm is midpoint of hand and configured middle finger base. Hilt shaft local Z is parameterized (default -26..14cm; pommel -30..-26cm). Local palm Z and hand span expose out-of-shaft positions, without a pass threshold. Palm-to-hilt-axis distances are observational, not strict contact certification. Moving foot gaps are recorded, stationary minimum foot marker must be 0..30cm above capsule bottom. Captures are deferred, request and processing times are recorded. No first-person mode, forced montage, pose playback, or injected velocity.\n"),Failures,Case,Requested,Completed,*PalmR.ToString(),*PalmL.ToString(),HiltMinZ,HiltMaxZ);
  Check(FFileHelper::SaveStringToFile(Summary,*(Output/TEXT("metrics.txt"))),TEXT("metrics_saved"));const bool bPass=Failures==0;UE_LOG(LogRogue10m,Display,TEXT("RESULT=GREATSWORD_LOCOMOTION_%s cases=%d captures=%d failures=%d"),bPass?TEXT("PASSED"):TEXT("FAILED"),Case,Completed,Failures);FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mStylizedCharacter> Character;TWeakObjectPtr<UClass> OriginalBodyClass;TWeakObjectPtr<USkeletalMesh> OriginalBodyMesh;
 FTimerHandle PrepareTimer;FDelegateHandle PreHandle,PostHandle,ShotHandle;FString Label,Output,Images,Rows,Captures,CaseName,ShotPath;TArray<FString> PathsRequested;
 FVector Origin,Direction,SeedRF,SeedLF,SeedSR,SeedSL;FName PalmR=TEXT("middle_01_r"),PalmL=TEXT("middle_01_l");float HiltMinZ=-26.f,HiltMaxZ=14.f,DesiredSpeed=0;
 int32 Case=0,Frame=0,Failures=0,Samples=0,Requested=0,Completed=0;double FloorZ=0,CaseStart=0,Deadline=0,ShotAt=0,SpeedSum=0,MinDot=1,MaxBodyTravel=0,MaxSourceTravel=0,OldDelta=1./60.;bool bFinished=false,bOwnFixed=false,OldFixed=false,bRequested=false,bProcessed=false,bPoseSeeded=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->Finished())){return;}for(const FWorldContext& C:GEngine->GetWorldContexts()){if(auto* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestGreatSwordLocomotion"),TEXT("Rendered third-person HumanMale sword idle, 16 directional walk/run probes and weapon restoration QA."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
