#if WITH_EDITOR
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
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Rogue10mWeaponIdentityComponent.h"

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
// Programmatic input and rendered QA, never a claim of physical input or full-run play.
class FRun : public TSharedFromThis<FRun>
{
public:
 bool Finished() const { return bFinished; }
 void Start(UWorld& W)
 {
  World=&W; FString Town,Prefix,Keys;const TCHAR* Args=FCommandLine::Get();
  FParse::Value(Args,TEXT("RogueThirdPersonQALabel="),Label);FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
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
 void Photo(const TCHAR* Name){if(!bRendered)return;Check(PendingPhoto.IsEmpty(),TEXT("previous_capture_completed"));PendingPhoto=Name;++RequestedPhotos;FScreenshotRequest::RequestScreenshot(Output/Name,true,false);}
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
  SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* X,ELevelTick,float){if(X==Self->World.Get())Self->Sample();});Later(.8,[this]{OpenView();});
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
  Later(.3,[this]{if(Obstacle.IsValid())Obstacle->Destroy();Obstacle.Reset();Stage=TEXT("recovered");Later(.35,[this]{const FVector Pivot=Character->GetActorLocation()+Character->GetThirdPersonCameraBoom()->TargetOffset;RecoveredDistance=FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Pivot);Check(FMath::Abs(RecoveredDistance-OpenDistance)<1&&!Character->GetThirdPersonCameraBoom()->IsCollisionFixApplied(),TEXT("camera_recovers"));PC->ToggleSettings();Later(.15,[this]{Check(PC->IsSettingsVisible(),TEXT("settings_open"));PC->CloseAllBlockingPanels();Later(.2,[this]{ViewChecks();BeginMovement();});});});});
 }
 void BeginMovement()
 {
  Stage=TEXT("movement");MoveStart=Character->GetActorLocation();Key(EKeys::W,true);Later(.4,[this]{Key(EKeys::W,false);MoveTravel=FVector::Dist2D(MoveStart,Character->GetActorLocation());Check(MoveTravel>30&&MoveTravel<500,TEXT("production_W_input_moves"));Photo(TEXT("03_third_person_movement.png"));Later(.35,[this]{BeginDodge(false);});});
 }
 void BeginDodge(bool WallCase)
 {
  Stage=WallCase?TEXT("wall_dodge"):TEXT("free_dodge");DodgeStart=Character->GetActorLocation();bWallDodge=WallCase;bSawDodge=false;bDodgePhoto=false;Character->GetCharacterMovement()->StopMovementImmediately();if(WallCase)Wall=MakeBox(DodgeStart+Character->GetActorForwardVector()*85,FVector(10,180,180),Character->GetActorRotation());Key(Rogue10mInput::DodgeKey(),true);
  Later(.35,[this,WallCase]{Key(Rogue10mInput::DodgeKey(),false);const float Travel=FVector::Dist2D(DodgeStart,Character->GetActorLocation());Check(bSawDodge,TEXT("production_LeftCtrl_activates_dodge"));Check(!Character->IsDodging(),TEXT("dodge_finishes"));if(WallCase){WallTravel=Travel;Check(Travel<=43&&Travel<FreeTravel,TEXT("dodge_stops_before_wall"));Photo(TEXT("05_wall_dodge_result.png"));Later(.35,[this]{MaybeMouse();});}else{FreeTravel=Travel;Check(Travel>FMath::Min(20.f,ConfiguredDodgeDistance*.05f)&&Travel<=ConfiguredDodgeDistance+5.f,TEXT("free_dodge_travel_within_actual_configuration"));Later(.5,[this]{BeginDodge(true);});}});
 }
 void Sample()
 {
  if(bFinished||!Character.IsValid()||!PC.IsValid()||!PC->PlayerCameraManager)return;const auto& V=PC->PlayerCameraManager->GetCameraCacheView();const FVector P=Character->GetActorLocation();++Samples;Check(!V.Location.ContainsNaN()&&!V.Rotation.ContainsNaN()&&FMath::IsFinite(V.FOV),TEXT("sample_camera_finite"));if(Stage.EndsWith(TEXT("dodge"))&&Character->IsDodging()){bSawDodge=true;++DodgeSamples;if(!bWallDodge&&!bDodgePhoto){bDodgePhoto=true;Photo(TEXT("04_active_third_person_dodge.png"));}}Frames+=FString::Printf(TEXT("%llu,%s,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f\n"),GFrameCounter,*Stage,Character->IsDodging(),V.Location.X,V.Location.Y,V.Location.Z,P.X,P.Y,P.Z,V.FOV);
 }
 void Finish(bool Exit=true)
 {
  if(bFinished)return;bFinished=true;Next=nullptr;FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);Key(EKeys::W,false);Key(Rogue10mInput::DodgeKey(),false);if(Character.IsValid()){if(Saves.IsValid()&&Saves->IsAttachedTo(*Character))Saves->DetachCharacter(*Character,false);Character->GetCharacterMovement()->StopMovementImmediately();}for(auto A:{Floor,Obstacle,Wall})if(A.IsValid())A->Destroy();if(bRendered)Check(RequestedPhotos==5&&CompletedPhotos==5&&PendingPhoto.IsEmpty(),TEXT("five_rendered_captures"));
  for(const auto& Pic:Photos){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Pic.Width,Pic.Height,Pic.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Output/Pic.Name)),TEXT("captured_PNG_saved"));}Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_saved"));Results+=FString::Printf(TEXT("mouse_cases=%d\nmouse_passed=%d\n"),MouseCases,MousePassed);Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nsamples=%d\ndodge_samples=%d\nopen_cm=%.6f\nblocked_cm=%.6f\nrecovered_cm=%.6f\nmove_cm=%.6f\nfree_dodge_cm=%.6f\nwall_dodge_cm=%.6f\nrendered=%d\ncaptures=%d\ninput=programmatic_Controller_InputKey\nphysical_input=not_performed\nfull_run=not_performed\nmap_changes=none_transient_fixture_only\nfirst_person_QA=paused\n"),Checks,Failures,Samples,DodgeSamples,OpenDistance,BlockedDistance,RecoveredDistance,MoveTravel,FreeTravel,WallTravel,bRendered,CompletedPhotos);if(!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))))++Failures;UE_LOG(LogRogue10m,Display,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 TStrongObjectPtr<URogue10mAttackSkillData> RealNovicePrimary;TSubclassOf<UGameplayAbility> RealNoviceAbility;int32 BeforeRealNoviceUses=0;
 TArray<URogue10mAttackSkillData*> MouseSkills;int32 BeforeUses[4]{},ExpectedUses[4]{};int32 MouseIndex=0,MouseCases=0,MousePassed=0;
 struct FPhoto{FString Name;int32 Width=0,Height=0;TArray<FColor> Pixels;};TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;TWeakObjectPtr<AActor> Floor,Obstacle,Wall;TFunction<void()> Next;FDelegateHandle Cleanup,SampleHandle,CaptureHandle;TArray<FPhoto> Photos;FString Label,Output,Results,Frames,Stage=TEXT("prepare"),PendingPhoto;FVector MoveStart,DodgeStart;double Deadline=0,NextAt=0;float ConfiguredDodgeDistance=100.f,ConfiguredDodgeDuration=.16f;float OpenDistance=0,BlockedDistance=0,RecoveredDistance=0,MoveTravel=0,FreeTravel=0,WallTravel=0;int32 Checks=0,Failures=0,Samples=0,DodgeSamples=0,RequestedPhotos=0,CompletedPhotos=0;bool bFinished=false,bRendered=false,bWallDodge=false,bSawDodge=false,bDodgePhoto=false;
};
static TSharedPtr<FRun> Active;static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=THIRD_PERSON_GAMEPLAY_FAILED reason=no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}static FAutoConsoleCommand Command(TEXT("Rogue10m.TestThirdPersonGameplay"),TEXT("Isolated fresh ThirdPerson namespace: default camera, mesh, collision, W and Left Ctrl; first-person QA paused."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif