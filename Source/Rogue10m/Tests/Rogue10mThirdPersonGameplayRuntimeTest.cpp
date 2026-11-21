#if WITH_EDITOR
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_RelevantAssetPlayerBase.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Camera/PlayerCameraManager.h"
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
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "Rogue10mWeaponVisualComponent.h"

struct FRogue10mMouseCombatTestAccess
{
 static void Configure(URogue10mCombatComponent& C, URogue10mWeaponSkillProfileDataAsset* P, const TArray<URogue10mAttackSkillData*>& Skills)
 {
  C.CancelCombatVisuals(); C.CharacterData=nullptr; C.DefaultAttackAbilityClass=nullptr; C.WeaponSkillProfiles={P};
  C.EquippedSkillBindings.Reset(); C.DurableSkillBindings.Reset(); C.WeaponSkillPresets.Reset(); C.AppliedProfileWeaponType=P->WeaponType; C.bWeaponPresetApplied=true;
  const ERogue10mAttackInputSlot Slots[]={ERogue10mAttackInputSlot::Primary,ERogue10mAttackInputSlot::Special,ERogue10mAttackInputSlot::ChargedSpecial,ERogue10mAttackInputSlot::MouseChord};
  for(int32 I=0;I<Skills.Num();++I){C.UnlockedAttackSkillNames.Add(Skills[I]->GetFName());C.EquippedSkillBindings.Add(Slots[I],Skills[I]);}
 }
 static TSubclassOf<UGameplayAbility> Ability(const URogue10mCombatComponent& C){return C.DefaultAttackAbilityClass;}
 static void Ability(URogue10mCombatComponent& C,TSubclassOf<UGameplayAbility> A){C.DefaultAttackAbilityClass=A;}
 static void Reset(URogue10mCombatComponent& C){C.CancelCombatVisuals();C.AttackCooldownStartTime=C.AttackCooldownEndTime=-1.f;C.AttackCooldownDuration=0;C.AttackActionEndTime=0;C.ResetComboWindow();}
 static void Slot(URogue10mCombatComponent& C,ERogue10mAttackInputSlot S,URogue10mAttackSkillData* V){if(V)C.EquippedSkillBindings.Add(S,V);else C.EquippedSkillBindings.Remove(S);}
};

namespace Rogue10mThirdPersonGameplayQA
{
struct FHeadPitchCase
{
 const TCHAR* Name;
 ERogue10mWeaponType Weapon;
 bool bRequestGuard;
};
static constexpr FHeadPitchCase HeadPitchCases[]={
 {TEXT("unarmed_idle"),ERogue10mWeaponType::Unarmed,false},
 {TEXT("unarmed_guard"),ERogue10mWeaponType::Unarmed,true},
 {TEXT("greatsword_ready"),ERogue10mWeaponType::GreatSword,false}
};
static const FName HeadPitchBones[]={TEXT("neck_01"),TEXT("head"),TEXT("upperarm_l"),TEXT("upperarm_r"),
 TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r")};

enum class EMotionCase : uint8 { SwordGuard, SwordGuard300Long, SwordGuard300, StillPunch, MovingPunch, SwordCharge, Count };
static const TCHAR* MotionCaseNames[]={TEXT("sword_guard_to_walk"),TEXT("sword_guard_300_long_control"),TEXT("sword_guard_300"),
 TEXT("unarmed_punch_still"),TEXT("unarmed_punch_to_walk"),TEXT("sword_charge_to_swing")};
static const FName MotionBones[]={TEXT("pelvis"),TEXT("spine_03"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")};

static bool IsPacedSwordGuard(EMotionCase Case)
{
 return Case==EMotionCase::SwordGuard300Long||Case==EMotionCase::SwordGuard300;
}

struct FObservedMotionPose
{
 FTransform Bones[UE_ARRAY_COUNT(MotionBones)];
 uint32 Revision=0;
};

static const FName JumpPoseBones[]={TEXT("pelvis"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),
 TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r")};

struct FObservedJumpPose
{
 FTransform Bones[UE_ARRAY_COUNT(JumpPoseBones)];
 uint32 Revision=0;
 int32 PairedFrames=0;
};

static FString DescribeJumpAnimation(UAnimInstance* Instance)
{
 if(!Instance){return FString();}
 const auto* Class=IAnimClassInterface::GetFromClass(Instance->GetClass());
 if(!Class){return FString();}
 FString Description;
 for(const auto& Definition:Class->GetBakedStateMachines())
 {
  const int32 MachineIndex=Instance->GetStateMachineIndex(Definition.MachineName);
  const auto* Machine=MachineIndex!=INDEX_NONE?Instance->GetStateMachineInstance(MachineIndex):nullptr;
  const int32 StateIndex=Machine?Machine->GetCurrentState():INDEX_NONE;
  if(!Definition.States.IsValidIndex(StateIndex)){continue;}
  const auto* Player=Instance->GetRelevantAssetPlayerInterfaceFromState(MachineIndex,StateIndex);
  if(!Description.IsEmpty()){Description+=TEXT("|");}
  Description+=FString::Printf(TEXT("%s:%s:%s:%.6f:%s:%.6f"),*GetNameSafe(Instance->GetClass()),
   *Definition.MachineName.ToString(),*Instance->GetCurrentStateName(MachineIndex).ToString(),
   Machine->GetCurrentStateElapsedTime(),*GetPathNameSafe(Player?Player->GetAnimAsset():nullptr),
   Player?Player->GetAccumulatedTime():0.0f);
 }
 return Description;
}

// Programmatic input and rendered QA, never a claim of physical input or full-run play.
class FRun : public TSharedFromThis<FRun>
{
public:
 bool Finished() const { return bFinished; }
 void Start(UWorld& W)
 {
  World=&W; FString Town,Prefix,Keys;const TCHAR* Args=FCommandLine::Get();
  FParse::Value(Args,TEXT("RogueThirdPersonQALabel="),Label);FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bCombatMotion=FParse::Param(Args,TEXT("RogueThirdPersonCombatMotionQA"));
  bCombatMotionCapture=bCombatMotion&&FParse::Param(Args,TEXT("RogueThirdPersonCombatMotionCapture"));
  bDoubleJumpMotion=FParse::Param(Args,TEXT("RogueThirdPersonDoubleJumpQA"));
  bDoubleJumpCapture=bDoubleJumpMotion&&FParse::Param(Args,TEXT("RogueThirdPersonDoubleJumpCapture"));
  bExpectSecondJumpPose=FParse::Param(Args,TEXT("RogueExpectSecondJumpPose"));
  if(bDoubleJumpMotion&&(bCombatMotion||bDoubleJumpCapture==FParse::Param(Args,TEXT("nullrhi"))))
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_FAILED reason=jump_audit_requires_its_own_explicit_capture_or_nullrhi_run"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  bool Valid=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label)Valid&=FChar::IsAlnum(C)||C==TEXT('_');
  TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  if(!Valid||W.GetNetMode()!=NM_Standalone||Town!=TEXT("Rogue10m_Town_QA_ThirdPerson_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_ThirdPerson_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_ThirdPerson_")+Label||UGameplayStatics::DoesSaveGameExist(Town,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty())
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_FAILED reason=exact_fresh_QA_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("DevLog/20261110_ThirdPerson")/Label;
  if(IFileManager::Get().DirectoryExists(*Output)){UE_LOG(LogRogue10m,Error,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_FAILED reason=fresh_evidence_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  IFileManager::Get().MakeDirectory(*Output,true);bRendered=!FParse::Param(Args,TEXT("nullrhi"));Deadline=FPlatformTime::Seconds()+180.;Frames=TEXT("frame,stage,dodging,view_x,view_y,view_z,actor_x,actor_y,actor_z,fov\n");
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){if(Self->bFinished)return false;if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("watchdog"));Self->Finish();return false;}if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto Work=MoveTemp(Self->Next);Work();}return !Self->bFinished;}),.01f);
  Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()&&!Self->bFinished){Self->Check(false,TEXT("early_world_cleanup"));Self->Finish(false);}});Later(3.,[this]{Prepare();});
 }
private:
 void Check(bool Value,const TCHAR* Why){++Checks;if(!Value){++Failures;Results+=FString(TEXT("FAIL "))+Why+TEXT("\n");if(Failures<25)UE_LOG(LogRogue10m,Error,TEXT("THIRD_PERSON FAIL %s"),Why);}}
 void Later(double Delay,TFunction<void()> Work){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(Work);}
 void Key(FKey K,bool Down){if(PC.IsValid())PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
 void Photo(const TCHAR* Name){if(!bRendered)return;Check(PendingPhoto.IsEmpty(),TEXT("previous_capture_completed"));PendingPhoto=Name;++RequestedPhotos;FScreenshotRequest::RequestScreenshot(Output/Name,!bCombatMotionCapture&&!bDoubleJumpCapture,false);}
 AActor* MakeBox(const FVector& Center,const FVector& Extent,const FRotator& Rotation)
 {
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;auto* A=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(Rotation,Center),P);if(!A)return nullptr;
  auto* Shape=NewObject<UBoxComponent>(A,NAME_None,RF_Transient);A->AddInstanceComponent(Shape);A->SetRootComponent(Shape);Shape->SetBoxExtent(Extent);Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Shape->SetCollisionObjectType(ECC_WorldStatic);Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->SetGenerateOverlapEvents(false);Shape->SetCanEverAffectNavigation(false);Shape->RegisterComponent();
  auto* Mesh=NewObject<UStaticMeshComponent>(A,NAME_None,RF_Transient);A->AddInstanceComponent(Mesh);Mesh->SetupAttachment(Shape);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetRelativeScale3D(Extent/50.);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();A->SetActorLocationAndRotation(Center,Rotation);return A;
 }
 void Prepare()
 {
  PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());auto* GI=World->GetGameInstance();auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  Check(PC.IsValid()&&Profiles&&Saves.IsValid(),TEXT("profile_controller_progression_available"));if(Failures){Finish();return;}
  FRogue10mCharacterProfile P;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("ThirdQa"),FRogue10mCharacterAppearance(),P)&&Profiles->SelectProfile(P.ProfileId),TEXT("fresh_profile_selected"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());Check(Character.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_pawn_and_save_attach"));if(Failures){Finish();return;}
  if(bDoubleJumpMotion){TrainDoubleJump();if(Failures){Finish();return;}}
  Saves->DetachCharacter(*Character,false);Character->SetCanBeDamaged(false);Check(Character->IsThirdPersonGameplayActive()&&Character->GetGameplayCameraComponent()&&Character->GetThirdPersonCameraBoom(),TEXT("third_person_default_components"));if(Failures){Finish();return;}
  RealNovicePrimary.Reset(Character->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary));RealNoviceAbility=FRogue10mMouseCombatTestAccess::Ability(*Character->GetCombatComponent());
  const auto* DodgeData=Character->GetCombatComponent()->GetActiveDodgeSkill();
  const auto* DistanceField=FindFProperty<FFloatProperty>(Character->GetClass(),TEXT("DodgeDistance"));
  const auto* DurationField=FindFProperty<FFloatProperty>(Character->GetClass(),TEXT("DodgeDuration"));
  ConfiguredDodgeDistance=DodgeData?DodgeData->Distance:DistanceField?DistanceField->GetPropertyValue_InContainer(Character.Get()):100.f;
  ConfiguredDodgeDuration=DodgeData?DodgeData->Duration:DurationField?DurationField->GetPropertyValue_InContainer(Character.Get()):.16f;
  Results+=FString::Printf(TEXT("configured_dodge_cm=%.6f\nconfigured_dodge_duration_s=%.6f\ndodge_config_source=%s\n"),ConfiguredDodgeDistance,ConfiguredDodgeDuration,DodgeData?*DodgeData->GetPathName():TEXT("Character_DodgeDistance_DodgeDuration"));
  Check(FMath::IsFinite(ConfiguredDodgeDistance)&&ConfiguredDodgeDistance>0.f&&FMath::IsFinite(ConfiguredDodgeDuration)&&ConfiguredDodgeDuration>0.f,TEXT("actual_dodge_configuration_readable"));
  const FVector Start=Character->GetActorLocation();const float Surface=Start.Z+1800.f;Floor=MakeBox(FVector(Start.X,Start.Y,Surface-20),FVector(2500,2500,20),FRotator::ZeroRotator);Check(Floor.IsValid(),TEXT("transient_floor"));
  Character->SetActorLocation(FVector(Start.X,Start.Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);PC->SetControlRotation(FRotator(-12,0,0));
  CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 W,int32 H,const TArray<FColor>& Pixels){if(Self->PendingPhoto.IsEmpty()||Self->bFinished)return;auto& Pic=Self->Photos.AddDefaulted_GetRef();Pic.Name=Self->PendingPhoto;Pic.Width=W;Pic.Height=H;Pic.Pixels=Pixels;Self->PendingPhoto.Empty();++Self->CompletedPhotos;});
  if(bRendered){FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();Check(!GShaderCompilingManager||!GShaderCompilingManager->IsCompiling(),TEXT("first_capture_waits_for_material_shaders"));}
  SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* X,ELevelTick,float){if(X==Self->World.Get())Self->Sample();});
  Later(.8,[this]{if(bDoubleJumpMotion){BeginDoubleJumpMotion();}else if(bCombatMotion){BeginCombatMotion();}else{OpenView();}});
 }

 void TrainDoubleJump()
 {
  auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  auto* Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
  TActorIterator<ARogue10mTutorialNPC> Services(World.Get());
  Check(Tree&&Services&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed
   &&Character->JumpMaxCount==1&&!Tree->IsCommonSkillLearned(ERogue10mCommonSkill::DoubleJump),
   TEXT("unarmed_player_has_one_jump_before_learning"));
  if(Failures){return;}
  State->AddExperience(500);
  const int32 PointsBefore=Tree->GetAvailableGrowthPoints();
  const FVector Location=Character->GetActorLocation();
  Character->SetActorLocation(Services->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
  constexpr auto Route=ERogue10mSkillTreeUnlockRoute::Points;
  Check(Tree->CanChangeInvestments()&&Tree->TryUnlockNode(TEXT("C.AirControl"),Route,NAME_None)
   &&Character->JumpMaxCount==1&&Tree->TryUnlockNode(TEXT("C.DoubleJump"),Route,NAME_None),
   TEXT("authored_town_service_learns_air_control_then_double_jump"));
  Check(Tree->IsCommonSkillLearned(ERogue10mCommonSkill::DoubleJump)&&Character->JumpMaxCount==2
   &&Tree->GetAvailableGrowthPoints()==PointsBefore-2,TEXT("common_investment_grants_two_real_jumps"));
  Character->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
 }

 void BeginDoubleJumpMotion()
 {
  const auto* Movement=Character->GetCharacterMovement();
  auto* Source=Character->GetAnimationPlaybackMesh();
  auto* Visible=Character->GetMesh();
  if(!Movement->IsMovingOnGround())
  {
   // The 2 cm setup drop may still be in progress on the slow software renderer.
   Later(.01,[this]{BeginDoubleJumpMotion();});
   return;
  }
  Check(FMath::IsNearlyEqual(World->GetDeltaSeconds(),1.0f/60.0f,.0001f),TEXT("jump_audit_uses_fixed_60_hz_ground"));
  Check(Source&&Visible&&Source!=Visible&&Source->GetAnimInstance()&&Visible->GetAnimInstance(),
   TEXT("jump_audit_uses_live_source_and_retargeted_mesh"));
  if(Failures){Finish();return;}
  const auto* Anim=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
  Check(!bExpectSecondJumpPose||(Anim&&Anim->GetSecondJumpPoseAnimation()),TEXT("expected_second_jump_clip_is_loaded"));
  if(Failures){Finish();return;}
  for(auto* Mesh:{Source,Visible})
  {
   Mesh->bEnableUpdateRateOptimizations=false;
   Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   for(const FName Bone:JumpPoseBones){Check(Mesh->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("jump_pose_bone_exists"));}
  }
  if(Failures){Finish();return;}
  if(bDoubleJumpCapture)
  {
   FActorSpawnParameters Parameters;
   Parameters.ObjectFlags|=RF_Transient;
   MotionCamera=World->SpawnActor<ACameraActor>(Parameters);
   Check(MotionCamera.IsValid(),TEXT("jump_capture_side_camera_created"));
   if(Failures){Finish();return;}
   MotionCamera->GetCameraComponent()->SetFieldOfView(60.0f);
   PC->SetViewTarget(MotionCamera.Get());
  }
  const auto* Motion=Character->GetCharacterAnimationComponent()->GetMotionData();
  Results+=FString::Printf(TEXT("jump_max_count=%d\njump_velocity_setting=%.6f\njump_source_mesh=%s\njump_visible_mesh=%s\njump_fx_profile=%s\ndouble_jump_fx=%s\n"),
   Character->JumpMaxCount,Movement->JumpZVelocity,*GetPathNameSafe(Source->GetSkeletalMeshAsset()),
   *GetPathNameSafe(Visible->GetSkeletalMeshAsset()),*GetPathNameSafe(Motion),
   *GetPathNameSafe(Motion?Motion->DoubleJumpEffect.Get():nullptr));
  JumpStateRows=TEXT("case,frame,world_seconds,jump_count,movement_mode,falling,velocity_z,actor_z,source_class,visible_class,source_graph,visible_graph,source_montage,second_jump_pose_applied\n");
  JumpPoseRows=TEXT("case,frame,mesh,revision,paired,bone,x,y,z,qx,qy,qz,qw,position_step_cm,angle_step_deg\n");
  bJumpMotionStarted=true;
 }

 void SampleDoubleJumpMotion()
 {
  const auto* Movement=Character->GetCharacterMovement();
  const bool bFalling=Movement->IsFalling();
  const float Velocity=Movement->Velocity.Z;
  const int32 Count=Character->JumpCurrentCount;
  const TCHAR* Case=bSecondJumpInput?TEXT("double_space"):TEXT("single_space");
  ++JumpFrame;
  if(JumpFrame>240){Check(false,TEXT("jump_motion_case_finishes_within_four_seconds"));Finish();return;}
  if(JumpFrame==8){Key(EKeys::SpaceBar,true);}
  if(JumpTakeoffFrame==0&&Count==1&&bFalling)
  {
   JumpTakeoffFrame=JumpFrame;
   JumpTakeoffVelocity=Velocity;
   Key(EKeys::SpaceBar,false);
  }
  if(JumpTakeoffFrame>0&&JumpRequestFrame==0&&Count==1&&bFalling&&Velocity<=-75.0f)
  {
   JumpRequestFrame=JumpFrame;
   if(bSecondJumpInput){Key(EKeys::SpaceBar,true);}
  }
  if(JumpBoostFrame==0&&Count==2)
  {
   JumpBoostFrame=JumpFrame;
   JumpBeforeBoostVelocity=PreviousJumpVelocity;
   JumpBoostVelocity=Velocity;
   Check(bSecondJumpInput&&JumpRequestFrame>0&&bFalling&&JumpBeforeBoostVelocity<=-75.0f
    &&JumpBoostVelocity>Movement->JumpZVelocity*.8f,TEXT("second_space_reverses_descent_while_still_falling"));
   Key(EKeys::SpaceBar,false);
  }
  if(JumpTakeoffFrame>0&&JumpLandFrame==0&&Movement->IsMovingOnGround()){JumpLandFrame=JumpFrame;}
  PreviousJumpVelocity=Velocity;

  const USkeletalMeshComponent* Meshes[]={Character->GetAnimationPlaybackMesh(),Character->GetMesh()};
  const auto* SourceAnim=Meshes[0]->GetAnimInstance();
  const auto* FighterAnim=Cast<URogue10mFighterCombatStanceAnimInstance>(SourceAnim);
  const bool bSecondPose=FighterAnim&&FighterAnim->WasSecondJumpPoseApplied();
  if(bSecondPose)
  {
   ++AppliedSecondJumpFrames;
   Check(Count==2&&bFalling&&JumpBoostFrame>0,TEXT("second_jump_pose_is_scoped_to_airborne_boost"));
  }
  FString Graph[2];
  for(int32 Index=0;Index<UE_ARRAY_COUNT(Meshes);++Index)
  {
   const auto* Mesh=Meshes[Index];
   Graph[Index]=DescribeJumpAnimation(Mesh->GetAnimInstance());
   for(auto* Linked:Mesh->GetLinkedAnimInstances())
   {
    const FString Details=DescribeJumpAnimation(Linked);
    if(!Details.IsEmpty()){if(!Graph[Index].IsEmpty()){Graph[Index]+=TEXT("|");}Graph[Index]+=Details;}
   }
   auto& Previous=JumpPrevious[Index];
   const uint32 Revision=Mesh->GetBoneTransformRevisionNumber();
   const bool bPaired=JumpFrame>1&&Revision!=Previous.Revision;
   Previous.PairedFrames+=bPaired?1:0;
   for(int32 BoneIndex=0;BoneIndex<UE_ARRAY_COUNT(JumpPoseBones);++BoneIndex)
   {
    const FName Bone=JumpPoseBones[BoneIndex];
    const FTransform Pose=Mesh->GetBoneTransform(Bone,RTS_Component);
    const FVector Location=Pose.GetLocation();
    const FQuat Rotation=Pose.GetRotation();
    Check(!Pose.ContainsNaN()&&Rotation.IsNormalized(),TEXT("valid_sampled_jump_bone_pose"));
    const FTransform& Prior=Previous.Bones[BoneIndex];
    const double Step=bPaired?FVector::Dist(Location,Prior.GetLocation()):0.0;
    const double Angle=bPaired?FMath::RadiansToDegrees(Rotation.AngularDistance(Prior.GetRotation())):0.0;
    JumpPoseRows+=FString::Printf(TEXT("%s,%d,%s,%u,%d,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
     Case,JumpFrame,Index==0?TEXT("source"):TEXT("visible"),Revision,bPaired,*Bone.ToString(),
     Location.X,Location.Y,Location.Z,Rotation.X,Rotation.Y,Rotation.Z,Rotation.W,Step,Angle);
    Previous.Bones[BoneIndex]=Pose;
   }
   Previous.Revision=Revision;
  }
  JumpStateRows+=FString::Printf(TEXT("%s,%d,%.6f,%d,%d,%d,%.6f,%.6f,%s,%s,%s,%s,%s,%d\n"),
   Case,JumpFrame,World->GetTimeSeconds(),Count,static_cast<int32>(Movement->MovementMode),bFalling,Velocity,
   Character->GetActorLocation().Z,*GetPathNameSafe(SourceAnim->GetClass()),*GetPathNameSafe(Meshes[1]->GetAnimInstance()->GetClass()),
   *Graph[0],*Graph[1],*GetPathNameSafe(SourceAnim->GetCurrentActiveMontage()),bSecondPose);
  if(bDoubleJumpCapture)
  {
   const FVector Target=Character->GetActorLocation()+FVector(0,0,6);
   const FVector Offset=-155.0*Character->GetActorForwardVector()+350.0*Character->GetActorRightVector()+FVector(0,0,40);
   MotionCamera->SetActorLocationAndRotation(Target+Offset,(-Offset).Rotation());
   if(JumpRequestFrame>0&&JumpFrame==JumpRequestFrame+8)
   {
    const auto* Name=bSecondJumpInput?TEXT("02_second_space_at_pose_peak.png"):TEXT("01_single_space_at_matched_frame.png");
    Results+=FString::Printf(TEXT("jump_capture_request=%s frame=%d world_seconds=%.6f\n"),Name,JumpFrame,World->GetTimeSeconds());
    Photo(Name);
   }
   if(JumpRequestFrame==0||JumpFrame<JumpRequestFrame+12||!PendingPhoto.IsEmpty()){return;}
   if(!bSecondJumpInput&&(JumpLandFrame==0||JumpFrame<JumpLandFrame+10)){return;}
  }
  else if(JumpLandFrame==0||JumpFrame<JumpLandFrame+24){return;}

  Results+=FString::Printf(TEXT("jump_case=%s frames=%d takeoff_frame=%d takeoff_vz=%.6f second_boundary=%d boost_frame=%d before_boost_vz=%.6f boost_vz=%.6f landing_frame=%d paired_source=%d paired_visible=%d second_pose_frames=%d\n"),
   Case,JumpFrame,JumpTakeoffFrame,JumpTakeoffVelocity,JumpRequestFrame,JumpBoostFrame,JumpBeforeBoostVelocity,
   JumpBoostVelocity,JumpLandFrame,JumpPrevious[0].PairedFrames,JumpPrevious[1].PairedFrames,AppliedSecondJumpFrames);
  if(bExpectSecondJumpPose)
  {
   Check(bSecondJumpInput?(AppliedSecondJumpFrames>=5&&AppliedSecondJumpFrames<=16):AppliedSecondJumpFrames==0,
    TEXT("brief_second_jump_pose_only_appears_after_second_space"));
  }
  Check(JumpTakeoffFrame>8&&JumpTakeoffVelocity>Movement->JumpZVelocity*.8f&&JumpRequestFrame>JumpTakeoffFrame
   &&(bSecondJumpInput?JumpBoostFrame>JumpRequestFrame:JumpBoostFrame==0)&&(bDoubleJumpCapture||Count==0),
   bDoubleJumpCapture?TEXT("real_jump_reaches_matched_capture"):TEXT("complete_real_jump_and_landing_sequence_observed"));
  Check(JumpPrevious[0].PairedFrames>=JumpFrame-2&&JumpPrevious[1].PairedFrames>=JumpFrame-2,
   TEXT("source_and_visible_meshes_have_adjacent_pose_revisions"));
  if(bSecondJumpInput)
  {
   Check(JumpTakeoffFrame==ControlJumpTakeoffFrame&&JumpRequestFrame==ControlJumpRequestFrame,
    TEXT("control_and_boost_share_the_same_takeoff_and_descent_boundary"));
   Finish();
   return;
  }
  bSecondJumpInput=true;
  ControlJumpTakeoffFrame=JumpTakeoffFrame;
  ControlJumpRequestFrame=JumpRequestFrame;
  JumpFrame=JumpTakeoffFrame=JumpRequestFrame=JumpBoostFrame=JumpLandFrame=0;
  AppliedSecondJumpFrames=0;
  JumpPrevious[0]={};
  JumpPrevious[1]={};
 }

 void BeginCombatMotion()
 {
  auto* Source=Character->GetAnimationPlaybackMesh();auto* Body=Character->GetMesh();
  Check(bRendered==bCombatMotionCapture&&FMath::IsNearlyEqual(World->GetDeltaSeconds(),1.0f/60.0f,.001f),TEXT("combat_motion_uses_fixed_60_hz_and_explicit_capture_mode"));
  Check(Source&&Body&&Source!=Body&&Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()),TEXT("combat_motion_has_live_Manny_and_retargeted_mesh"));
  if(Failures){Finish();return;}
  for(USkeletalMeshComponent* Mesh:{Source,Body})
  {
   Mesh->bEnableUpdateRateOptimizations=false;
   Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   for(const FName Bone:MotionBones){Check(Mesh->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("combat_motion_sampled_bone_exists"));}
  }
  if(Failures){Finish();return;}
  if(bCombatMotionCapture)
  {
   const FVector Origin=Character->GetActorLocation();
   const FVector Position=Origin-Character->GetActorForwardVector()*235.0+Character->GetActorRightVector()*330.0+FVector(0,0,56);
   const FVector Target=Origin+FVector(0,0,5);
   FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;
   MotionCamera=World->SpawnActor<ACameraActor>(Position,(Target-Position).Rotation(),Params);
   Check(MotionCamera.IsValid(),TEXT("combat_motion_fixed_side_rear_camera_created"));
   if(Failures){Finish();return;}
   MotionCamera->GetCameraComponent()->SetFieldOfView(64.0f);
   PC->SetViewTarget(MotionCamera.Get());
  }
  MotionRows=TEXT("case,frame,world_seconds,mesh,revision,samples_paired,ground_speed,guard_alpha,moving_attack_alpha,charge_held,montage_id,montage_seconds,bone,component_x,component_y,component_z,pelvis_relative_x,pelvis_relative_y,pelvis_relative_z,component_step_cm,component_angle_deg,pelvis_relative_step_cm\n");
  bCombatMotionStarted=true;
  BeginMotionCase(EMotionCase::SwordGuard);
 }

 void BeginMotionCase(EMotionCase NewCase)
 {
  RestoreMotionTuning();
  MotionCase=NewCase;
  if(NewCase==EMotionCase::Count){Finish();return;}
  Stage=MotionCaseNames[static_cast<int32>(NewCase)];
  const bool bPaced=IsPacedSwordGuard(NewCase);
  const bool bSword=NewCase==EMotionCase::SwordGuard||NewCase==EMotionCase::SwordCharge||bPaced;
  Character->RestoreEquippedWeaponType(bSword?ERogue10mWeaponType::GreatSword:ERogue10mWeaponType::Unarmed);
  if(bPaced)
  {
   auto* Movement=Character->GetCharacterMovement();
   MotionTunedAnimation=Cast<URogue10mFighterCombatStanceAnimInstance>(Character->GetAnimationPlaybackMesh()->GetAnimInstance());
   Check(MotionTunedAnimation.IsValid(),TEXT("combat_motion_paced_case_anim_instance_exists"));
   if(Failures){Finish();return;}
   MotionOriginalSpeed=Movement->MaxWalkSpeed;
   MotionOriginalLower=MotionTunedAnimation->GreatSwordMoveGuardLowerSeconds;
   Movement->MaxWalkSpeed=300.0f;
   if(NewCase==EMotionCase::SwordGuard300Long)
   {MotionTunedAnimation->GreatSwordMoveGuardLowerSeconds=MotionTunedAnimation->FighterCombatLowerSeconds;}
   bMotionTuningApplied=true;
  }
  const auto* Combat=Character->GetCombatComponent();
  const auto Slot=NewCase==EMotionCase::SwordCharge?ERogue10mAttackInputSlot::ChargedSpecial:ERogue10mAttackInputSlot::Primary;
  MotionSkill=Combat->GetEquippedSkill(Slot);
  if(NewCase==EMotionCase::SwordCharge&&(!MotionSkill.IsValid()||!MotionSkill->AttackMontage))
  {
   Results+=TEXT("motion_sword_charge=not_sampled_no_charge_montage_in_fresh_loadout\n");
   Finish();return;
  }
  Check(NewCase==EMotionCase::SwordGuard||bPaced||(MotionSkill.IsValid()&&MotionSkill->AttackMontage),TEXT("combat_motion_has_equipped_production_attack_montage"));
  if(Failures){Finish();return;}
  MotionWaitFrames=MotionFrame=MotionPairs=MotionMoveFrame=0;
  MotionMontageId=INDEX_NONE;
  bMotionReady=bMotionPrevious=bSawMotionCharge=bSawMotionBlend=bSawMotionWalk=false;
  PreviousGuardAlpha=PreviousMovingAlpha=0.0f;
  MotionGuardChestAngle=MotionGuardHandStep=0.0;
  MotionPaceGuardFrames=0;
  MotionGuardLowFrame=0;
  MotionPeakSpeed=0.0f;
  MotionUseCount=MotionSkill.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(MotionSkill->GetFName()):0;
  MotionReleaseFrame=NewCase==EMotionCase::SwordCharge?6+FMath::CeilToInt(FMath::Max(.05f,MotionSkill->ChargeSeconds)*60.0f)+4:8;
 }

 void RestoreMotionTuning()
 {
  if(!bMotionTuningApplied){return;}
  if(Character.IsValid()){Character->GetCharacterMovement()->MaxWalkSpeed=MotionOriginalSpeed;}
  if(MotionTunedAnimation.IsValid()){MotionTunedAnimation->GreatSwordMoveGuardLowerSeconds=MotionOriginalLower;}
  MotionTunedAnimation.Reset();
  bMotionTuningApplied=false;
 }

 void ObserveCombatMotion()
 {
  auto* Source=Character->GetAnimationPlaybackMesh();auto* Body=Character->GetMesh();
  auto* Anim=Source?Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()):nullptr;
  auto* Movement=Character->GetCharacterMovement();auto* Combat=Character->GetCombatComponent();
  if(!Anim||!Body||!Movement||!Combat){Check(false,TEXT("combat_motion_live_objects_retained"));Finish();return;}
  const float Speed=Movement->Velocity.Size2D();
  const bool bPaced=IsPacedSwordGuard(MotionCase);
  const bool bGuardCase=MotionCase==EMotionCase::SwordGuard||bPaced;
  const bool bChargeCase=MotionCase==EMotionCase::SwordCharge;
  if(!bMotionReady)
  {
   ++MotionWaitFrames;
   const bool bSettledGuard=(bGuardCase||bChargeCase)?Anim->FighterCombatStanceAlpha>.95f:Anim->FighterCombatStanceAlpha<.05f;
   bMotionReady=MotionWaitFrames>=12&&Movement->IsMovingOnGround()&&Speed<1.0f
    &&Combat->GetAttackCooldownRemaining()<=0.0f&&!Anim->IsAnyMontagePlaying()&&bSettledGuard;
   if(!bMotionReady&&MotionWaitFrames>600){Check(false,TEXT("combat_motion_grounded_ready_state_timed_out"));Finish();}
   if(!bMotionReady){return;}
  }
  ++MotionFrame;
  const FAnimMontageInstance* Live=MotionSkill.IsValid()&&MotionSkill->AttackMontage?Anim->GetActiveInstanceForMontage(MotionSkill->AttackMontage):nullptr;
  if(Live&&Combat->GetLastAcceptedAttackSkill()==MotionSkill.Get())
  {
   if(MotionMontageId==INDEX_NONE){MotionMontageId=Live->GetInstanceID();}
   Check(MotionMontageId==Live->GetInstanceID(),TEXT("combat_motion_transition_retains_montage_instance"));
   bSawMotionBlend|=Anim->MovingAttackUpperBodyAlpha>.9f;
  }
  bSawMotionCharge|=bChargeCase&&Combat->IsAttackChargeHeld();
  const float Guard=Anim->FighterCombatStanceAlpha;
  const float Moving=Anim->MovingAttackUpperBodyAlpha;
  MotionPeakSpeed=FMath::Max(MotionPeakSpeed,Speed);
  bSawMotionWalk|=Speed>10.0f&&Guard<.05f;
  if(bGuardCase&&MotionFrame>6&&MotionGuardLowFrame==0&&Guard<.05f){MotionGuardLowFrame=MotionFrame;}
  if(bPaced&&Speed>=299.0f&&Guard>=.05f){++MotionPaceGuardFrames;}
  USkeletalMeshComponent* Meshes[]={Source,Body};
  for(int32 MeshIndex=0;MeshIndex<UE_ARRAY_COUNT(Meshes);++MeshIndex)
  {
   const auto* Mesh=Meshes[MeshIndex];FObservedMotionPose Current;
   Current.Revision=Mesh->GetBoneTransformRevisionNumber();
   for(int32 Bone=0;Bone<UE_ARRAY_COUNT(MotionBones);++Bone)
   {
    Current.Bones[Bone]=Mesh->GetBoneTransform(MotionBones[Bone],RTS_Component);
    Check(!Current.Bones[Bone].ContainsNaN()&&Current.Bones[Bone].GetRotation().IsNormalized(),TEXT("combat_motion_finite_normalized_bones"));
   }
   const auto& Previous=MotionPrevious[MeshIndex];
   const bool bPaired=bMotionPrevious&&Current.Revision!=Previous.Revision;
   if(MeshIndex==1&&bPaired){++MotionPairs;}
   for(int32 Bone=0;Bone<UE_ARRAY_COUNT(MotionBones);++Bone)
   {
    const auto& Pose=Current.Bones[Bone];const FVector P=Pose.GetLocation();const FVector Relative=Current.Bones[0].InverseTransformPosition(P);
    const double Step=bPaired?FVector::Distance(P,Previous.Bones[Bone].GetLocation()):0.0;
    const double Angle=bPaired?FMath::RadiansToDegrees(Pose.GetRotation().AngularDistance(Previous.Bones[Bone].GetRotation())):0.0;
    const double RelativeStep=bPaired?FVector::Distance(Relative,Previous.Bones[0].InverseTransformPosition(Previous.Bones[Bone].GetLocation())):0.0;
    if(MeshIndex==1&&bPaired&&bGuardCase&&MotionFrame>=7&&MotionFrame<=13)
    {
     if(Bone==1){MotionGuardChestAngle=FMath::Max(MotionGuardChestAngle,Angle);}
     if(Bone==2||Bone==3){MotionGuardHandStep=FMath::Max(MotionGuardHandStep,RelativeStep);}
    }
    MotionRows+=FString::Printf(TEXT("%s,%d,%.6f,%s,%u,%d,%.6f,%.6f,%.6f,%d,%d,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
     *Stage,MotionFrame,World->GetTimeSeconds(),MeshIndex==0?TEXT("source"):TEXT("visible"),Current.Revision,bPaired,Speed,Guard,Moving,
     Combat->IsAttackChargeHeld(),Live?Live->GetInstanceID():INDEX_NONE,Live?Live->GetPosition():-1.0f,*MotionBones[Bone].ToString(),
     P.X,P.Y,P.Z,Relative.X,Relative.Y,Relative.Z,Step,Angle,RelativeStep);
   }
   MotionPrevious[MeshIndex]=Current;
  }
  if(bMotionPrevious&&(FMath::Abs(Guard-PreviousGuardAlpha)>.5f||FMath::Abs(Moving-PreviousMovingAlpha)>.5f))
  {
   Results+=FString::Printf(TEXT("motion_boundary=%s frame=%d speed=%.3f guard=%.6f,%.6f moving=%.6f,%.6f montage_s=%.6f\n"),
    *Stage,MotionFrame,Speed,PreviousGuardAlpha,Guard,PreviousMovingAlpha,Moving,Live?Live->GetPosition():-1.0f);
  }
  bMotionPrevious=true;PreviousGuardAlpha=Guard;PreviousMovingAlpha=Moving;

  if(bGuardCase)
  {
   if(bCombatMotionCapture&&(MotionFrame==5||MotionFrame==12))
   {
    Photo(MotionFrame==5?TEXT("01_sword_guard_before_W.png"):TEXT("02_sword_guard_fading.png"));
    Results+=FString::Printf(TEXT("motion_capture_request_frame=%d world_seconds=%.6f\n"),MotionFrame,World->GetTimeSeconds());
   }
   if(MotionFrame==6){Key(EKeys::W,true);}
   if(MotionFrame==(bPaced?37:27)){Key(EKeys::W,false);}
  }
  else
  {
   const FKey Attack=bChargeCase?EKeys::RightMouseButton:EKeys::LeftMouseButton;
   if(MotionFrame==6){Key(Attack,true);}if(MotionFrame==MotionReleaseFrame){Key(Attack,false);}
   if(MotionCase==EMotionCase::MovingPunch&&Live&&MotionMoveFrame==0&&(Live->GetPosition()>=.033f))
   {MotionMoveFrame=MotionFrame;Key(EKeys::W,true);}
   if(MotionMoveFrame>0&&MotionFrame==MotionMoveFrame+10){Key(EKeys::W,false);}
  }
  if(MotionFrame<FMath::Max(78,MotionReleaseFrame+72)){return;}
  Check(MotionPairs>=20,TEXT("combat_motion_has_paired_visible_frames"));
  if(bGuardCase)
  {
   Check(bSawMotionWalk,TEXT("combat_motion_W_observed_grounded_walk_after_guard"));
   Check(Speed<1.0f&&Guard>.95f&&Movement->IsMovingOnGround(),TEXT("combat_motion_sword_returns_to_settled_guard"));
   Check(MotionGuardChestAngle<15.0&&MotionGuardHandStep<8.0,TEXT("combat_motion_sword_guard_transition_has_no_large_pose_step"));
   Results+=FString::Printf(TEXT("motion_guard_metrics=%s chest_peak_deg=%.6f hand_peak_cm=%.6f first_below_five_percent_frame=%d paced_above_five_percent_frames=%d peak_speed_cm_s=%.3f\n"),
    *Stage,MotionGuardChestAngle,MotionGuardHandStep,MotionGuardLowFrame,MotionPaceGuardFrames,MotionPeakSpeed);
   if(bPaced)
   {
    Check(FMath::IsNearlyEqual(MotionPeakSpeed,300.0f,.1f),TEXT("combat_motion_paced_guard_measured_at_300_cm_per_second"));
    Check(MotionGuardLowFrame>6,TEXT("combat_motion_paced_guard_fade_finished"));
    Check(MotionCase==EMotionCase::SwordGuard300Long?MotionPaceGuardFrames>=10:MotionPaceGuardFrames<=4,
     TEXT("combat_motion_paced_guard_tail_matches_tested_duration"));
   }
  }
  if(!bGuardCase)
  {
   const int32 Uses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(MotionSkill->GetFName())-MotionUseCount;
   Check(Uses==1&&MotionMontageId!=INDEX_NONE,TEXT("combat_motion_real_mouse_input_accepted_one_montage"));
  }
  if(MotionCase==EMotionCase::MovingPunch){Check(MotionMoveFrame>0&&bSawMotionBlend,TEXT("combat_motion_W_during_punch_observed_moving_layer"));}
  if(MotionCase==EMotionCase::StillPunch){Check(!bSawMotionBlend&&!bSawMotionWalk,TEXT("combat_motion_stationary_punch_kept_full_body_layer"));}
  if(bChargeCase){Check(bSawMotionCharge,TEXT("combat_motion_RMB_hold_observed_before_release"));}
  Results+=FString::Printf(TEXT("motion_case=%s frames=%d visible_pairs=%d montage_id=%d\n"),*Stage,MotionFrame,MotionPairs,MotionMontageId);
  if(Failures){Finish();return;}
  if(bCombatMotionCapture){Finish();return;}
  BeginMotionCase(static_cast<EMotionCase>(static_cast<uint8>(MotionCase)+1));
 }
 void ViewChecks()
 {
  auto* Body=Character->GetMesh();auto* Arms=Character->GetFirstPersonMesh();auto* Appearance=Character->GetAppearanceCameraComponent();auto* Camera=Character->GetGameplayCameraComponent();const auto& V=PC->PlayerCameraManager->GetCameraCacheView();
  Check(Body&&Body->IsVisible()&&!Body->bHiddenInGame&&!Body->bOwnerNoSee&&!Body->IsBoneHiddenByName(TEXT("head")),TEXT("full_body_and_head_visible"));Check(!Arms||(!Arms->IsVisible()&&Arms->bHiddenInGame&&!Arms->IsComponentTickEnabled()),TEXT("legacy_arms_paused"));Check(!Appearance||(!Appearance->IsAppearanceCameraActive()&&!Appearance->IsThirdPersonInspectionActive()),TEXT("legacy_head_camera_inactive"));
  Check(Camera&&Camera!=Character->GetFirstPersonCameraComponent()&&Camera->IsActive()&&!Character->GetFirstPersonCameraComponent()->IsActive(),TEXT("only_follow_camera_active"));Check(!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsNearlyEqual(V.FOV,90.f,.02f),TEXT("automatic_camera_cache_finite_90_FOV"));for(USceneComponent* Child:Body->GetAttachChildren())if(auto* Part=Cast<USkeletalMeshComponent>(Child);Part&&Part!=Arms)Check(!Part->bOwnerNoSee,TEXT("cosmetic_visible_to_owner"));
 }
 void MaybeMouse()
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("RogueThirdPersonMouseQA"))){Finish();return;}
  if(Wall.IsValid()){Wall->Destroy();Wall.Reset();}
  Stage=TEXT("mouse_fixture");auto* C=Character->GetCombatComponent();auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
  Check(C&&State&&!Saves->IsAttachedTo(*Character),TEXT("mouse_fixture_requires_detached_fresh_profile"));if(Failures){Finish();return;}
  auto* P=NewObject<URogue10mWeaponSkillProfileDataAsset>(C,NAME_None,RF_Transient);P->WeaponType=ERogue10mWeaponType::GreatSword;
  const ERogue10mAttackInputSlot Slots[]={ERogue10mAttackInputSlot::Primary,ERogue10mAttackInputSlot::Special,ERogue10mAttackInputSlot::ChargedSpecial,ERogue10mAttackInputSlot::MouseChord};
  const TCHAR* Names[]={TEXT("MouseQA_Basic"),TEXT("MouseQA_Tap"),TEXT("MouseQA_Charge"),TEXT("MouseQA_Chord")};
  for(int32 I=0;I<4;++I)
  {
   auto* V=NewObject<URogue10mAttackSkillData>(C,FName(Names[I]),RF_Transient);V->InputSlot=Slots[I];V->AllowedSlotPolicy=I==0?ERogue10mSkillSlotPolicy::BasicOnly:I==1?ERogue10mSkillSlotPolicy::Common:I==2?ERogue10mSkillSlotPolicy::RightChargeOnly:ERogue10mSkillSlotPolicy::MouseChordOnly;
   V->AttackCooldown=.02f;V->ChargeSeconds=.2f;V->ResourceCosts.Reset();V->AttackMontage=nullptr;V->GameplayAbilityClass=nullptr;V->NextComboSkill=nullptr;MouseSkills.Add(V);P->SkillTreeSkills.Add(V);
  }
  P->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary,MouseSkills[0]);State->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);FRogue10mMouseCombatTestAccess::Configure(*C,P,MouseSkills);Later(.2,[this]{MouseCase(0);});
 }
 int32 Uses(int32 Index)const{const auto* State=Character->GetPlayerState<ARogue10mPlayerState>();return State?State->GetSkillUseCount(MouseSkills[Index]->GetFName()):-1;}
 void MouseCase(int32 Index)
 {
  Stage=FString::Printf(TEXT("mouse_case_%02d"),Index);MouseIndex=Index;auto* C=Character->GetCombatComponent();FRogue10mMouseCombatTestAccess::Reset(*C);
  for(int32 I=0;I<4;++I)BeforeUses[I]=Uses(I);for(int32& V:ExpectedUses)V=0;
  const FKey L=EKeys::LeftMouseButton,R=EKeys::RightMouseButton;
  auto Done=[this]{Later(.2,[this]{FinishMouseCase();});};
  if(Index==0){ExpectedUses[0]=1;Key(L,true);Later(.02,[this,L,Done]{Key(L,false);Done();});}
  else if(Index==1){ExpectedUses[0]=1;Key(L,true);Later(.3,[this,L,Done]{Key(L,false);Done();});}
  else if(Index==2){ExpectedUses[1]=1;Key(R,true);Later(.07,[this,R,Done]{Key(R,false);Done();});}
  else if(Index==3){ExpectedUses[2]=1;Key(R,true);Later(.32,[this,R,Done]{Key(R,false);Done();});}
  else if(Index==4||Index==5)
  {
   ExpectedUses[3]=1;const FKey First=Index==4?L:R,Second=Index==4?R:L;Key(First,true);
   Later(.02,[this,First,Second,Done]{Key(Second,true);Later(.03,[this,First,Second,Done]{Key(First,false);Later(.02,[this,Second,Done]{Key(Second,false);Done();});});});
  }
  else if(Index==6){ExpectedUses[0]=1;Key(L,true);Later(.14,[this,L,R,Done]{Key(R,true);Later(.04,[this,L,R,Done]{Key(L,false);Key(R,false);Done();});});}
  else if(Index==7){Key(L,true);Later(.02,[this,L,Done]{PC->ToggleSettings();Later(.15,[this,L,Done]{PC->CloseAllBlockingPanels();Key(L,false);Done();});});}
  else if(Index==8){Key(R,true);Later(.07,[this,R,Done]{PC->ToggleSettings();Later(.25,[this,R,Done]{PC->CloseAllBlockingPanels();Key(R,false);Done();});});}
  else if(Index==9){Key(R,true);Later(.07,[this,R,Done]{FRogue10mMouseCombatTestAccess::Slot(*Character->GetCombatComponent(),ERogue10mAttackInputSlot::ChargedSpecial,nullptr);Later(.25,[this,R,Done]{Key(R,false);Done();});});}
  else if(Index==10)
  {
   ExpectedUses[0]=1;FRogue10mMouseCombatTestAccess::Slot(*C,ERogue10mAttackInputSlot::MouseChord,nullptr);Key(L,true);
   Later(.025,[this,L,Done]{Check(Uses(0)==BeforeUses[0]+1,TEXT("no_chord_slot_basic_has_no_80ms_delay"));Key(L,false);Done();});
  }
  else if(Index==11)
  {
   Key(R,true);Later(.07,[this,R,Done]{FRogue10mMouseCombatTestAccess::Slot(*Character->GetCombatComponent(),ERogue10mAttackInputSlot::ChargedSpecial,MouseSkills[1]);Later(.25,[this,R,Done]{Key(R,false);Done();});});
  }
  else if(Index==12)
  {
   auto* State=Character->GetPlayerState<ARogue10mPlayerState>();State->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
   for(auto S:{ERogue10mAttackInputSlot::QSkill,ERogue10mAttackInputSlot::ESkill,ERogue10mAttackInputSlot::RSkill,ERogue10mAttackInputSlot::Special,ERogue10mAttackInputSlot::ChargedSpecial,ERogue10mAttackInputSlot::MouseChord})FRogue10mMouseCombatTestAccess::Slot(*C,S,nullptr);
   Check(RealNovicePrimary.IsValid()&&RealNovicePrimary.Get()!=MouseSkills[0]&&RealNovicePrimary->GetPathName().StartsWith(TEXT("/Game/")),TEXT("actual_novice_primary_is_real_content_not_synthetic"));
   if(!RealNovicePrimary.IsValid()){Finish();return;}
   FRogue10mMouseCombatTestAccess::Slot(*C,ERogue10mAttackInputSlot::Primary,RealNovicePrimary.Get());FRogue10mMouseCombatTestAccess::Ability(*C,RealNoviceAbility);BeforeRealNoviceUses=State->GetSkillUseCount(RealNovicePrimary->GetFName());
   Key(L,true);Later(.08,[this,L,R,Done]{Key(L,false);for(auto K:{EKeys::Q,EKeys::E,EKeys::R}){Key(K,true);Key(K,false);}Key(R,true);Later(.3,[this,R,Done]{Key(R,false);Key(Rogue10mInput::IdentityKey(),true);Key(Rogue10mInput::IdentityKey(),false);Done();});});
  }
 }
 void FinishMouseCase()
 {
  bool Passed=true;for(int32 I=0;I<4;++I){const int32 Delta=Uses(I)-BeforeUses[I];Check(Delta==ExpectedUses[I],TEXT("mouse_case_exact_use_count_no_double_fire"));Passed&=Delta==ExpectedUses[I];Results+=FString::Printf(TEXT("mouse_case_%02d_skill_%d_delta=%d expected=%d\n"),MouseIndex,I,Delta,ExpectedUses[I]);}
  Check(!Character->HasPendingMouseAttackInput()&&!Character->GetCombatComponent()->IsAnyAttackInputHeld(),TEXT("mouse_case_all_held_state_cleared"));
  if(MouseIndex==12)
  {
   const auto* State=Character->GetPlayerState<ARogue10mPlayerState>();const int32 Delta=State->GetSkillUseCount(RealNovicePrimary->GetFName())-BeforeRealNoviceUses;
   const auto* Identity=Character->FindComponentByClass<URogue10mWeaponIdentityComponent>();
   const bool BasicAccepted=Delta==1;const bool NoTechniques=Character->GetCombatComponent()->GetEquippedLoadoutSkillCount()==0&&(!Identity||!Identity->IsIdentityActive());
   Check(BasicAccepted,TEXT("actual_novice_InputKey_LMB_accepts_real_fixed_punch"));Check(NoTechniques,TEXT("actual_novice_QER_RMB_identity_key_cannot_activate_weapon_skills"));Passed&=BasicAccepted&&NoTechniques;
   Results+=FString::Printf(TEXT("actual_novice_primary=%s\nactual_novice_primary_use_delta=%d\nactual_novice_weapon_skills_rejected=%d\nactual_novice_input=Controller_InputKey_not_physical\nactual_novice_save_restart=not_performed\n"),*RealNovicePrimary->GetPathName(),Delta,NoTechniques);
  }
  ++MouseCases;if(Passed)++MousePassed;
  auto* C=Character->GetCombatComponent();FRogue10mMouseCombatTestAccess::Slot(*C,ERogue10mAttackInputSlot::ChargedSpecial,MouseSkills[2]);FRogue10mMouseCombatTestAccess::Slot(*C,ERogue10mAttackInputSlot::MouseChord,MouseSkills[3]);
  if(MouseIndex<12)Later(.1,[this]{MouseCase(MouseIndex+1);});else Finish();
 }
 void OpenView()
 {
  Stage=TEXT("open");Check(Character->GetCharacterMovement()->IsMovingOnGround(),TEXT("pawn_grounded"));ViewChecks();const FVector Pivot=Character->GetActorLocation()+Character->GetThirdPersonCameraBoom()->TargetOffset;OpenDistance=FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Pivot);Check(OpenDistance>300&&OpenDistance<340,TEXT("full_boom_open_distance"));const FTransform Aim=Character->GetCombatViewTransform();Check(FVector::Dist(Aim.GetLocation(),Character->GetActorLocation())<100&&FVector::DotProduct(Aim.GetRotation().GetForwardVector(),PC->GetControlRotation().Vector())>.98,TEXT("body_origin_screen_aim"));Photo(TEXT("01_default_third_person.png"));
  Later(.3,[this]{Stage=TEXT("camera_obstacle");const FVector Pivot=Character->GetActorLocation()+Character->GetThirdPersonCameraBoom()->TargetOffset;Obstacle=MakeBox(FMath::Lerp(Pivot,Character->GetGameplayCameraComponent()->GetComponentLocation(),.5),FVector(10,160,160),PC->GetControlRotation());Later(.35,[this]{BlockedView();});});
 }
 void BlockedView()
 {
  ViewChecks();auto* Boom=Character->GetThirdPersonCameraBoom();const FVector Pivot=Character->GetActorLocation()+Boom->TargetOffset;BlockedDistance=FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Pivot);Check(BlockedDistance<180&&BlockedDistance>50&&Boom->IsCollisionFixApplied(),TEXT("camera_contracts_before_wall"));Photo(TEXT("02_wall_camera_contraction.png"));
  Later(.3,[this]{if(Obstacle.IsValid())Obstacle->Destroy();Obstacle.Reset();Stage=TEXT("recovered");Later(.35,[this]{const FVector Pivot=Character->GetActorLocation()+Character->GetThirdPersonCameraBoom()->TargetOffset;RecoveredDistance=FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Pivot);Check(FMath::Abs(RecoveredDistance-OpenDistance)<1&&!Character->GetThirdPersonCameraBoom()->IsCollisionFixApplied(),TEXT("camera_recovers"));PC->ToggleSettings();Later(.15,[this]{Check(PC->IsSettingsVisible(),TEXT("settings_open"));PC->CloseAllBlockingPanels();Later(.2,[this]{ViewChecks();BeginHeadPitchCheck();});});});});
 }
 void BeginHeadPitchCheck()
 {
  HeadPitchSource=Character->GetAnimationPlaybackMesh();
  HeadPitchBody=Character->GetMesh();
  HeadPitchAnimation=HeadPitchSource.IsValid()?Cast<URogue10mFighterCombatStanceAnimInstance>(HeadPitchSource->GetAnimInstance()):nullptr;
  const bool bHasRetargetedBody=HeadPitchSource.IsValid()&&HeadPitchBody.IsValid()&&HeadPitchSource!=HeadPitchBody;
  Check(bHasRetargetedBody&&HeadPitchAnimation.IsValid(),TEXT("head_pitch_uses_source_and_visible_retargeted_body"));
  if(!bHasRetargetedBody||!HeadPitchAnimation.IsValid()){Finish();return;}
  const auto* Appearance=Character->GetAppearanceCameraComponent();
  const FRogue10mHeadMotionSettings HeadMotion=Appearance?Appearance->GetHeadMotionSettings():FRogue10mHeadMotionSettings();
  Check(FMath::IsNearlyEqual(HeadMotion.RotationWeight,.35f)&&FMath::IsNearlyEqual(HeadMotion.MaxRotationDegrees,5.f),TEXT("third_person_grounded_idle_uses_reviewed_head_motion"));

  HeadPitchOriginalControl=PC->GetControlRotation();
  HeadPitchOriginalWeapon=Character->GetEquippedWeaponType();
  bHeadPitchOriginalControllerYaw=Character->bUseControllerRotationYaw;
  HeadPitchOriginalRates[0]=HeadPitchSource->GlobalAnimRateScale;
  HeadPitchOriginalRates[1]=HeadPitchBody->GlobalAnimRateScale;
  HeadPitchOriginalAimSpeed=HeadPitchAnimation->UpperBodyAimInterpolationSpeed;
  bHeadPitchCheckActive=true;
  Character->bUseControllerRotationYaw=false;
  HeadPitchYaw=FRotator::NormalizeAxis(Character->GetActorRotation().Yaw+20.0);
  if(bRendered)
  {
   const FVector Position=Character->GetActorLocation()+Character->GetActorForwardVector()*110
    +Character->GetActorRightVector()*180+FVector::UpVector*54;
   const FVector Target=HeadPitchBody->GetBoneLocation(TEXT("head"))-FVector::UpVector*16;
   FActorSpawnParameters Params;
   Params.ObjectFlags|=RF_Transient;
   HeadPitchPhotoCamera=World->SpawnActor<ACameraActor>(Position,(Target-Position).Rotation(),Params);
   Check(HeadPitchPhotoCamera.IsValid(),TEXT("head_pitch_profile_camera_created"));
  }
  BeginHeadPitchCase();
 }
 void BeginHeadPitchCase()
 {
  const FHeadPitchCase& Case=HeadPitchCases[HeadPitchCaseIndex];
  Character->RestoreEquippedWeaponType(Case.Weapon);
  auto* Animation=Character->GetCharacterAnimationComponent();
  Animation->ResetFighterCombatStance();
  if(Case.bRequestGuard){Animation->RequestFighterCombatStance(5.f);}
  HeadPitchSource->GlobalAnimRateScale=HeadPitchOriginalRates[0];
  HeadPitchBody->GlobalAnimRateScale=HeadPitchOriginalRates[1];
  HeadPitchAnimation->UpperBodyAimInterpolationSpeed=HeadPitchOriginalAimSpeed;
  Stage=FString::Printf(TEXT("head_pitch_%s_prepare"),Case.Name);
  PC->SetControlRotation(FRotator(0,HeadPitchYaw,0));
  Later(.5,[this]
  {
   // Evaluate the production graphs at the same animation phase for all three inputs.
   // Keep the meshes ticking so the proxy and the retargeter consume the new input.
   HeadPitchSource->GlobalAnimRateScale=0;
   HeadPitchBody->GlobalAnimRateScale=0;
   // A frozen clip clock cannot advance aim interpolation. Measure its settled response directly.
   HeadPitchAnimation->UpperBodyAimInterpolationSpeed=0;
   HeadPitchSourceRevision=HeadPitchSource->GetBoneTransformRevisionNumber();
   HeadPitchBodyRevision=HeadPitchBody->GetBoneTransformRevisionNumber();
   ApplyHeadPitch(0);
  });
 }
 void ApplyHeadPitch(int32 Index)
 {
  static constexpr double Pitches[]={0,65,-65};
  const double Pitch=Pitches[Index];
  Stage=FString::Printf(TEXT("head_pitch_%s_%+.0f"),HeadPitchCases[HeadPitchCaseIndex].Name,Pitch);
  PC->SetControlRotation(FRotator(Pitch,HeadPitchYaw,0));
  Later(.25,[this,Index,Pitch]{MeasureHeadPitch(Index,Pitch);});
 }
 void MeasureHeadPitch(int32 Index,double Pitch)
 {
  auto* Source=HeadPitchSource.Get();
  auto* Body=HeadPitchBody.Get();
  const auto* Anim=Source?Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()):nullptr;
  Check(Anim&&Body,TEXT("head_pitch_gameplay_anim_instances_available"));
  if(!Anim||!Body){Finish();return;}
  const FHeadPitchCase& Case=HeadPitchCases[HeadPitchCaseIndex];
  Check(Character->GetEquippedWeaponType()==Case.Weapon,TEXT("head_pitch_expected_weapon_equipped"));
  const bool bExpectGuard=Case.bRequestGuard||Case.Weapon==ERogue10mWeaponType::GreatSword;
  Check(bExpectGuard?Anim->FighterCombatStanceAlpha>.95f:Anim->FighterCombatStanceAlpha<.05f,TEXT("head_pitch_expected_idle_or_ready_stance"));
  if(Case.Weapon==ERogue10mWeaponType::GreatSword)
  {
   const auto* Visual=Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
   const auto* Sword=Visual?Visual->GetRightWeaponMesh():nullptr;
   Check(Sword&&Sword->IsVisible()&&Sword->GetAttachParent()==Body&&Sword->GetAttachSocketName()==TEXT("hand_r"),TEXT("head_pitch_visible_greatsword_attached_to_hand"));
  }

  const uint32 SourceRevision=Source->GetBoneTransformRevisionNumber();
  const uint32 BodyRevision=Body->GetBoneTransformRevisionNumber();
  Check(SourceRevision!=HeadPitchSourceRevision,TEXT("head_pitch_source_reevaluated_at_fixed_phase"));
  Check(BodyRevision!=HeadPitchBodyRevision,TEXT("head_pitch_visible_body_reevaluated_at_fixed_phase"));
  HeadPitchSourceRevision=SourceRevision;
  HeadPitchBodyRevision=BodyRevision;
  const FRotator Control=PC->GetControlRotation();
  const double ResidualYaw=FRotator::NormalizeAxis(Control.Yaw-Character->GetActorRotation().Yaw);
  const FQuat ExpectedBasis(FVector::UpVector,FMath::DegreesToRadians(ResidualYaw));
  const double AimError=FMath::RadiansToDegrees(Anim->MouseDrivenHeadWorldAim.AngularDistance(ExpectedBasis));
  Check(FMath::IsNearlyEqual(ResidualYaw,20.0,.1),TEXT("head_pitch_has_nonzero_horizontal_aim"));
  Check(Anim->bMouseDrivenHeadApplied&&AimError<.05,TEXT("head_pitch_published_pose_basis_is_horizontal"));
  const double CameraPitch=FRotator::NormalizeAxis(PC->PlayerCameraManager->GetCameraCacheView().Rotation.Pitch);
  Check(FMath::IsNearlyEqual(FRotator::NormalizeAxis(Control.Pitch),Pitch,.1)&&FMath::IsNearlyEqual(CameraPitch,Pitch,.1),TEXT("third_person_camera_keeps_requested_pitch"));
  const FVector CombatAim=Character->GetCombatViewTransform().GetRotation().GetForwardVector();
  // A nearby floor hit changes the ray from the character to the crosshair's aim point.
  Check(Index==0||CombatAim.Z*FMath::Sign(Pitch)>.1,TEXT("combat_aim_follows_vertical_input"));
  Check(FMath::IsNearlyZero(Anim->UpperBodyAimPitch,.1f),TEXT("third_person_arms_ignore_vertical_input"));
  Check(FMath::IsNearlyEqual(static_cast<double>(Anim->UpperBodyAimYaw),ResidualYaw,.1),TEXT("third_person_upper_body_keeps_horizontal_aim"));

  USkeletalMeshComponent* Meshes[]={Source,Body};
  double MaxBoneDelta=0,MaxBoneTravel=0;
  for(int32 MeshIndex=0;MeshIndex<UE_ARRAY_COUNT(Meshes);++MeshIndex)
  {
   for(int32 BoneIndex=0;BoneIndex<UE_ARRAY_COUNT(HeadPitchBones);++BoneIndex)
   {
    auto* Mesh=Meshes[MeshIndex];
    const FName Bone=HeadPitchBones[BoneIndex];
    if(Mesh->GetBoneIndex(Bone)==INDEX_NONE){Check(false,TEXT("head_pitch_bone_exists"));continue;}
    const FTransform Pose=Mesh->GetBoneTransform(Bone,RTS_Component);
    Check(!Pose.ContainsNaN()&&Pose.GetRotation().IsNormalized(),TEXT("head_pitch_bone_transform_is_valid"));
    FTransform& Neutral=HeadPitchNeutral[MeshIndex][BoneIndex];
    if(Index==0){Neutral=Pose;continue;}
    const double Delta=FMath::RadiansToDegrees(Pose.GetRotation().AngularDistance(Neutral.GetRotation()));
    const double Travel=FVector::Distance(Pose.GetLocation(),Neutral.GetLocation());
    MaxBoneDelta=FMath::Max(MaxBoneDelta,Delta);
    MaxBoneTravel=FMath::Max(MaxBoneTravel,Travel);
    Check(Delta<.1&&Travel<.1,BoneIndex<2?TEXT("source_and_visible_head_and_neck_ignore_camera_pitch"):TEXT("source_and_visible_arms_ignore_camera_pitch"));
   }
  }
  Results+=FString::Printf(TEXT("head_pitch_input=%.3f pose=%s camera_pitch=%.3f upper_body_pitch=%.3f upper_body_yaw=%.3f stance_alpha=%.3f residual_yaw=%.3f basis_error=%.6f max_bone_delta=%.6f max_bone_travel_cm=%.6f\n"),
   Pitch,Case.Name,CameraPitch,Anim->UpperBodyAimPitch,Anim->UpperBodyAimYaw,Anim->FighterCombatStanceAlpha,ResidualYaw,AimError,MaxBoneDelta,MaxBoneTravel);
  auto NextPitch=[this,Index]
  {
   if(HeadPitchPhotoCamera.IsValid()){PC->SetViewTarget(Character.Get());}
   if(Index<2){ApplyHeadPitch(Index+1);return;}
   if(++HeadPitchCaseIndex<UE_ARRAY_COUNT(HeadPitchCases)){BeginHeadPitchCase();return;}
   RestoreHeadPitchCheck();
   Later(.3,[this]{BeginMovement();});
  };
  if(Index>0&&Case.Weapon==ERogue10mWeaponType::GreatSword&&HeadPitchPhotoCamera.IsValid())
  {
   // The gameplay view was measured above. A fixed side camera keeps the head visible for both captures.
   PC->SetViewTarget(HeadPitchPhotoCamera.Get());
   Later(.15,[this,Index,NextPitch]
   {
    Photo(Index==1?TEXT("06_look_up_head.png"):TEXT("07_look_down_head.png"));
    Later(.25,NextPitch);
   });
   return;
  }
  Later(.25,NextPitch);
 }
 void RestoreHeadPitchCheck()
 {
  if(!bHeadPitchCheckActive){return;}
  bHeadPitchCheckActive=false;
  if(HeadPitchSource.IsValid()){HeadPitchSource->GlobalAnimRateScale=HeadPitchOriginalRates[0];}
  if(HeadPitchBody.IsValid()){HeadPitchBody->GlobalAnimRateScale=HeadPitchOriginalRates[1];}
  if(HeadPitchAnimation.IsValid()){HeadPitchAnimation->UpperBodyAimInterpolationSpeed=HeadPitchOriginalAimSpeed;}
  if(Character.IsValid())
  {
   Character->bUseControllerRotationYaw=bHeadPitchOriginalControllerYaw;
   Character->GetCharacterAnimationComponent()->ResetFighterCombatStance();
   Character->RestoreEquippedWeaponType(HeadPitchOriginalWeapon);
  }
  if(PC.IsValid()){PC->SetControlRotation(HeadPitchOriginalControl);}
  if(HeadPitchPhotoCamera.IsValid())
  {
   if(PC.IsValid()&&Character.IsValid()){PC->SetViewTarget(Character.Get());}
   HeadPitchPhotoCamera->Destroy();
   HeadPitchPhotoCamera.Reset();
  }
 }
 void BeginMovement()
 {
  Stage=TEXT("movement");MoveStart=Character->GetActorLocation();Key(EKeys::W,true);Later(.4,[this]{Key(EKeys::W,false);MoveTravel=FVector::Dist2D(MoveStart,Character->GetActorLocation());Check(MoveTravel>30&&MoveTravel<500,TEXT("production_W_input_moves"));Photo(TEXT("03_third_person_movement.png"));Later(.35,[this]{BeginDodge(false);});});
 }
 void BeginDodge(bool WallCase)
 {
  Stage=WallCase?TEXT("wall_dodge"):TEXT("free_dodge");DodgeStart=Character->GetActorLocation();bSawDodge=false;bSawForwardDodgeMontage=false;bDodgePhoto=false;Character->GetCharacterMovement()->StopMovementImmediately();if(WallCase)Wall=MakeBox(DodgeStart+Character->GetActorForwardVector()*85,FVector(10,180,180),Character->GetActorRotation());Key(Rogue10mInput::DodgeKey(),true);
  Later(.35,[this,WallCase]
  {
   Key(Rogue10mInput::DodgeKey(),false);
   const float Travel=FVector::Dist2D(DodgeStart,Character->GetActorLocation());
   Check(bSawDodge,TEXT("production_dodge_key_activates_dodge"));
   Check(bSawForwardDodgeMontage,TEXT("forward_dodge_uses_configured_step_montage"));
   Check(!Character->IsDodging(),TEXT("dodge_finishes"));
   Results+=FString::Printf(TEXT("%s_cm=%.4f forward_step=%d\n"),*Stage,Travel,bSawForwardDodgeMontage);
   if(WallCase)
   {
    WallTravel=Travel;Check(Travel<=43&&Travel<FreeTravel,TEXT("dodge_stops_before_wall"));Photo(TEXT("05_wall_dodge_result.png"));
    Later(DodgeWait(),[this]{if(Wall.IsValid()){Wall->Destroy();}Wall.Reset();BeginOffAxisDodge(true);});
   }
   else
   {
    FreeTravel=Travel;
    Check(Travel>FMath::Min(20.f,ConfiguredDodgeDistance*.05f)&&Travel<=ConfiguredDodgeDistance+5.f,TEXT("free_dodge_travel_within_actual_configuration"));
    Later(DodgeWait(),[this]{BeginDodge(true);});
   }
  });
 }
 double DodgeWait()const{return FMath::Max(.35,static_cast<double>(Character->GetDodgeCooldownRemaining())+.1);}
 void BeginOffAxisDodge(bool bBackward)
 {
  const FKey MoveKey=bBackward?EKeys::S:EKeys::D;
  Stage=bBackward?TEXT("backward_dodge"):TEXT("right_dodge");
  bSawDodge=false;bSawForwardDodgeMontage=false;
  Key(MoveKey,true);
  Later(.10,[this,MoveKey,bBackward]
  {
   DodgeStart=Character->GetActorLocation();
   const FVector Expected=(bBackward?-Character->GetActorForwardVector():Character->GetActorRightVector()).GetSafeNormal2D();
   Character->GetCharacterMovement()->StopMovementImmediately();
   Key(Rogue10mInput::DodgeKey(),true);
   Later(.05,[this,MoveKey,bBackward,Expected]
   {
    Key(MoveKey,false);Key(Rogue10mInput::DodgeKey(),false);
    Later(FMath::Max(.35,static_cast<double>(ConfiguredDodgeDuration)+.1),[this,bBackward,Expected]
    {
     const FVector Displacement=Character->GetActorLocation()-DodgeStart;
     const double Along=FVector::DotProduct(Displacement,Expected);
     Check(bSawDodge&&!Character->IsDodging(),TEXT("off_axis_Alt_starts_and_finishes_dodge"));
     Check(Along>2.0&&Along>=Displacement.Size2D()*.85,TEXT("off_axis_dodge_travels_in_requested_direction"));
     Check(!bSawForwardDodgeMontage,TEXT("off_axis_dodge_never_uses_forward_step"));
     Results+=FString::Printf(TEXT("%s_cm=%.4f forward_step=%d\n"),*Stage,Along,bSawForwardDodgeMontage);
     Later(DodgeWait(),[this,bBackward]{if(bBackward){BeginOffAxisDodge(false);}else{MaybeMouse();}});
    });
   });
  });
 }
 void Sample()
 {
  if(bDoubleJumpMotion)
  {
   if(bJumpMotionStarted&&!bFinished&&Character.IsValid()){SampleDoubleJumpMotion();}
   return;
  }
  if(bCombatMotion){if(bCombatMotionStarted&&!bFinished){ObserveCombatMotion();}return;}
  if(bFinished||!Character.IsValid()||!PC.IsValid()||!PC->PlayerCameraManager)return;const auto& V=PC->PlayerCameraManager->GetCameraCacheView();const FVector P=Character->GetActorLocation();++Samples;Check(!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV),TEXT("sample_camera_finite"));
  if(Stage.EndsWith(TEXT("dodge"))&&Character->IsDodging())
  {
   bSawDodge=true;++DodgeSamples;
   const auto* Source=Character->GetAnimationPlaybackMesh();
   const auto* Anim=Source?Source->GetAnimInstance():nullptr;
   const auto* Motion=Character->GetCharacterAnimationComponent()->GetMotionData();
   bSawForwardDodgeMontage|=Anim&&Motion&&Motion->DodgeMontage&&Anim->Montage_IsActive(Motion->DodgeMontage.Get());
   if(Stage==TEXT("free_dodge")&&!bDodgePhoto){bDodgePhoto=true;Photo(TEXT("04_active_third_person_dodge.png"));}
  }
  Frames+=FString::Printf(TEXT("%llu,%s,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f\n"),GFrameCounter,*Stage,Character->IsDodging(),V.Location.X,V.Location.Y,V.Location.Z,P.X,P.Y,P.Z,V.FOV);
 }
 void Finish(bool Exit=true)
 {
  if(bDoubleJumpMotion&&bJumpMotionStarted&&!bFinished)
  {
   Key(EKeys::SpaceBar,false);
   Check(FFileHelper::SaveStringToFile(JumpStateRows,*(Output/TEXT("jump-state-frames.csv")))
    &&FFileHelper::SaveStringToFile(JumpPoseRows,*(Output/TEXT("jump-pose-frames.csv"))),TEXT("jump_pose_and_state_evidence_saved"));
  }
  if(bDoubleJumpCapture&&MotionCamera.IsValid()&&!bFinished)
  {
   if(PC.IsValid()&&Character.IsValid()){PC->SetViewTarget(Character.Get());}
   MotionCamera->Destroy();
  }
  if(bCombatMotion&&!bFinished)
  {
   RestoreMotionTuning();
   if(PC.IsValid()){Key(EKeys::W,false);Key(EKeys::LeftMouseButton,false);Key(EKeys::RightMouseButton,false);}
   if(MotionCamera.IsValid())
   {
    if(PC.IsValid()&&Character.IsValid()){PC->SetViewTarget(Character.Get());}
    MotionCamera->Destroy();
   }
   Check(!MotionRows.IsEmpty()&&FFileHelper::SaveStringToFile(MotionRows,*(Output/TEXT("motion-frames.csv"))),TEXT("combat_motion_frame_evidence_saved"));
  }
  if(bFinished)return;bFinished=true;Next=nullptr;RestoreHeadPitchCheck();FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);Key(EKeys::W,false);Key(EKeys::S,false);Key(EKeys::D,false);Key(Rogue10mInput::DodgeKey(),false);if(Character.IsValid()){if(Saves.IsValid()&&Saves->IsAttachedTo(*Character))Saves->DetachCharacter(*Character,false);Character->GetCharacterMovement()->StopMovementImmediately();}for(auto A:{Floor,Obstacle,Wall})if(A.IsValid())A->Destroy();
  if(bRendered)
  {
   const int32 ExpectedCaptures=bCombatMotionCapture||bDoubleJumpCapture?2:7;
   Check(RequestedPhotos==ExpectedCaptures&&CompletedPhotos==ExpectedCaptures&&PendingPhoto.IsEmpty(),TEXT("all_requested_rendered_captures_completed"));
  }
  for(const auto& Pic:Photos){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Pic.Width,Pic.Height,Pic.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Output/Pic.Name)),TEXT("captured_PNG_saved"));}Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_saved"));Results+=FString::Printf(TEXT("mouse_cases=%d\nmouse_passed=%d\n"),MouseCases,MousePassed);Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nsamples=%d\ndodge_samples=%d\nopen_cm=%.6f\nblocked_cm=%.6f\nrecovered_cm=%.6f\nmove_cm=%.6f\nfree_dodge_cm=%.6f\nwall_dodge_cm=%.6f\nrendered=%d\ncaptures=%d\ninput=programmatic_Controller_InputKey\nphysical_input=not_performed\nfull_run=not_performed\nmap_changes=none_transient_fixture_only\nfirst_person_QA=paused\n"),Checks,Failures,Samples,DodgeSamples,OpenDistance,BlockedDistance,RecoveredDistance,MoveTravel,FreeTravel,WallTravel,bRendered,CompletedPhotos);if(!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))))++Failures;UE_LOG(LogRogue10m,Display,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 TStrongObjectPtr<URogue10mAttackSkillData> RealNovicePrimary;TSubclassOf<UGameplayAbility> RealNoviceAbility;int32 BeforeRealNoviceUses=0;
 FObservedJumpPose JumpPrevious[2];
 FString JumpStateRows,JumpPoseRows;
 int32 JumpFrame=0,JumpTakeoffFrame=0,JumpRequestFrame=0,JumpBoostFrame=0,JumpLandFrame=0;
 int32 ControlJumpTakeoffFrame=0,ControlJumpRequestFrame=0;
 int32 AppliedSecondJumpFrames=0;
 float JumpTakeoffVelocity=0.0f,JumpBoostVelocity=0.0f,JumpBeforeBoostVelocity=0.0f,PreviousJumpVelocity=0.0f;
 bool bDoubleJumpMotion=false,bDoubleJumpCapture=false,bJumpMotionStarted=false,bSecondJumpInput=false,bExpectSecondJumpPose=false;
 TWeakObjectPtr<const URogue10mAttackSkillData> MotionSkill;
 TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> MotionTunedAnimation;
 TWeakObjectPtr<ACameraActor> MotionCamera;
 FObservedMotionPose MotionPrevious[2];
 FString MotionRows;
 EMotionCase MotionCase=EMotionCase::SwordGuard;
 int32 MotionWaitFrames=0,MotionFrame=0,MotionPairs=0,MotionMoveFrame=0,MotionMontageId=INDEX_NONE,MotionUseCount=0,MotionReleaseFrame=8;
 int32 MotionPaceGuardFrames=0,MotionGuardLowFrame=0;
 float PreviousGuardAlpha=0.0f,PreviousMovingAlpha=0.0f;
 float MotionOriginalSpeed=0.0f,MotionOriginalLower=0.0f,MotionPeakSpeed=0.0f;
 double MotionGuardChestAngle=0.0,MotionGuardHandStep=0.0;
 bool bCombatMotion=false,bCombatMotionCapture=false,bCombatMotionStarted=false,bMotionReady=false,bMotionPrevious=false,bSawMotionCharge=false,bSawMotionBlend=false,bSawMotionWalk=false,bMotionTuningApplied=false;
 TArray<URogue10mAttackSkillData*> MouseSkills;int32 BeforeUses[4]{},ExpectedUses[4]{};int32 MouseIndex=0,MouseCases=0,MousePassed=0;
 TWeakObjectPtr<USkeletalMeshComponent> HeadPitchSource,HeadPitchBody;
 TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> HeadPitchAnimation;
 TWeakObjectPtr<ACameraActor> HeadPitchPhotoCamera;
 FRotator HeadPitchOriginalControl=FRotator::ZeroRotator;
 ERogue10mWeaponType HeadPitchOriginalWeapon=ERogue10mWeaponType::Unarmed;
 FTransform HeadPitchNeutral[2][UE_ARRAY_COUNT(HeadPitchBones)]={};
 double HeadPitchYaw=0;
 float HeadPitchOriginalRates[2]={1,1};
 float HeadPitchOriginalAimSpeed=0;
 uint32 HeadPitchSourceRevision=0,HeadPitchBodyRevision=0;
 int32 HeadPitchCaseIndex=0;
 bool bHeadPitchCheckActive=false,bHeadPitchOriginalControllerYaw=false;
 struct FPhoto{FString Name;int32 Width=0,Height=0;TArray<FColor> Pixels;};TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;TWeakObjectPtr<AActor> Floor,Obstacle,Wall;TFunction<void()> Next;FDelegateHandle Cleanup,SampleHandle,CaptureHandle;TArray<FPhoto> Photos;FString Label,Output,Results,Frames,Stage=TEXT("prepare"),PendingPhoto;FVector MoveStart,DodgeStart;double Deadline=0,NextAt=0;float ConfiguredDodgeDistance=100.f,ConfiguredDodgeDuration=.16f;float OpenDistance=0,BlockedDistance=0,RecoveredDistance=0,MoveTravel=0,FreeTravel=0,WallTravel=0;int32 Checks=0,Failures=0,Samples=0,DodgeSamples=0,RequestedPhotos=0,CompletedPhotos=0;bool bFinished=false,bRendered=false,bSawDodge=false,bSawForwardDodgeMontage=false,bDodgePhoto=false;
};
static TSharedPtr<FRun> Active;static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_FAILED reason=no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}static FAutoConsoleCommand Command(TEXT("Rogue10m.TestThirdPersonGameplay"),TEXT("Isolated fresh ThirdPerson namespace: default camera, mesh, collision, W and Left Ctrl; first-person QA paused."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
