#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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
#include "NiagaraComponent.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

struct FRogue10mWeaponIdentityTestAccess
{
 static UNiagaraComponent* Aura(const URogue10mWeaponIdentityComponent& I){return I.Aura;}
 static UPostProcessComponent* Post(const URogue10mWeaponIdentityComponent& I){return I.PostProcess;}
 static float End(const URogue10mWeaponIdentityComponent& I){return I.EndsAt;}
 static void ReadyForIndependentCase(URogue10mWeaponIdentityComponent& I){I.NextActivationAt=-1.f;}
 static void RunEndHandler(URogue10mWeaponIdentityComponent& I){I.HandleRunPhase(ERogue10mRunPhase::Victory);}
 static float CombatSpeed(const URogue10mCombatComponent& C){return C.GetAttackSpeedMultiplier();}
};

namespace Rogue10mWeaponIdentityQA
{
struct FRaw
{
 float Move=0,Speed=0,Power=0,MaxHealth=0,MaxMana=0;
 static FRaw Read(const ARogue10mCharacter& C){const auto* A=C.GetRogueAttributeSet();return A?FRaw{A->GetMoveSpeed(),A->GetAttackSpeedMultiplier(),A->GetAttackPower(),A->GetMaxHealth(),A->GetMaxMana()}:FRaw();}
 bool Same(const FRaw& B)const{return Move==B.Move&&Speed==B.Speed&&Power==B.Power&&MaxHealth==B.MaxHealth&&MaxMana==B.MaxMana;}
};
class FRun : public TSharedFromThis<FRun>
{
public:
 bool Finished()const{return bFinished;}
 void Start(UWorld& W)
 {
  World=&W;FString Town,Prefix,Keys;const TCHAR* A=FCommandLine::Get();FParse::Value(A,TEXT("RogueWeaponIdentityQALabel="),Label);FParse::Value(A,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(A,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(A,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Valid=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label)Valid&=FChar::IsAlnum(C)||C==TEXT('_');TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  if(!Valid||W.GetNetMode()!=NM_Standalone||Town!=TEXT("Rogue10m_Town_QA_WeaponIdentity_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_WeaponIdentity_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_WeaponIdentity_")+Label||UGameplayStatics::DoesSaveGameExist(Town,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty())
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEAPON_IDENTITY_FAILED reason=exact_fresh_QA_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("DevLog/20261110_WeaponIdentity")/Label;if(IFileManager::Get().DirectoryExists(*Output)){UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEAPON_IDENTITY_FAILED reason=fresh_evidence_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}IFileManager::Get().MakeDirectory(*Output,true);
  bRendered=!FParse::Param(A,TEXT("nullrhi"));Deadline=FPlatformTime::Seconds()+180.;Frames=TEXT("world_time,stage,active,remaining,cooldown,max_walk,attribute_move,attribute_speed,attribute_power,combat_speed,aura_exists,aura_active,aura_complete,post_exists\n");
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){if(Self->bFinished)return false;if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("watchdog"));Self->Finish();return false;}if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto Work=MoveTemp(Self->Next);Work();}return !Self->bFinished;}),.01f);
  Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()&&!Self->bFinished){Self->Check(false,TEXT("early_world_cleanup"));Self->Finish(false);}});Later(3.,[this]{Prepare();});
 }
private:
 void Check(bool Value,const TCHAR* Why){++Checks;if(!Value){++Failures;Results+=FString(TEXT("FAIL "))+Why+TEXT("\n");if(Failures<30)UE_LOG(LogRogue10m,Error,TEXT("WEAPON_IDENTITY FAIL %s"),Why);}}
 void Later(double Delay,TFunction<void()> Work){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(Work);}
 void Key(FKey K,bool Down){if(PC.IsValid())PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
 void Ctrl(TFunction<void()> After){Key(EKeys::LeftControl,true);Later(.06,[this,After=MoveTemp(After)]()mutable{Key(EKeys::LeftControl,false);Later(.22,MoveTemp(After));});}
 void Photo(const TCHAR* Name){if(!bRendered)return;Check(PendingPhoto.IsEmpty(),TEXT("previous_capture_completed"));PendingPhoto=Name;++RequestedPhotos;FScreenshotRequest::RequestScreenshot(Output/Name,true,false);}
 URogue10mCombatComponent* Combat()const{return Character.IsValid()?Character->GetCombatComponent():nullptr;}
 void Sample(const TCHAR* Name)
 {
  if(!Character.IsValid()||!Identity.IsValid())return;Stage=Name;const FRaw A=FRaw::Read(*Character);auto* N=FRogue10mWeaponIdentityTestAccess::Aura(*Identity);auto* P=FRogue10mWeaponIdentityTestAccess::Post(*Identity);
  Frames+=FString::Printf(TEXT("%.6f,%s,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%d,%d\n"),World->GetTimeSeconds(),Name,Identity->IsIdentityActive(),Identity->GetRemainingSeconds(),Identity->GetCooldownRemaining(),Character->GetCharacterMovement()->MaxWalkSpeed,A.Move,A.Speed,A.Power,FRogue10mWeaponIdentityTestAccess::CombatSpeed(*Combat()),N!=nullptr,N&&N->IsActive(),N&&N->IsComplete(),P!=nullptr);
 }
 bool VisualsGone()const{return !FRogue10mWeaponIdentityTestAccess::Aura(*Identity)&&!FRogue10mWeaponIdentityTestAccess::Post(*Identity);}
 void Prepare()
 {
  PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());auto* GI=World->GetGameInstance();auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  Check(PC.IsValid()&&Profiles&&Saves.IsValid(),TEXT("normal_profile_services"));if(Failures){Finish();return;}FRogue10mCharacterProfile Profile;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("IdentityQa"),FRogue10mCharacterAppearance(),Profile)&&Profiles->SelectProfile(Profile.ProfileId),TEXT("fresh_QA_profile_selected"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());Check(Character.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_attached"));if(Failures){Finish();return;}Saves->DetachCharacter(*Character,false);Character->SetCanBeDamaged(false);Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);Identity=Character->FindComponentByClass<URogue10mWeaponIdentityComponent>();
  Check(Identity.IsValid()&&Character->IsThirdPersonGameplayActive()&&Character->GetRogueAttributeSet()&&Combat()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary),TEXT("production_knuckle_third_person_components"));if(Failures){Finish();return;}
  for(auto* S:Combat()->GetActiveSkillTreeSkills())if(S&&S!=Combat()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)&&S->SkillTreeNodeType==ERogue10mSkillTreeNodeType::ActiveTechnique){SampleSkill.Reset(S);break;}
  Check(SampleSkill.IsValid(),TEXT("production_skill_range_sample"));if(Failures){Finish();return;}
  const FVector Start=Character->GetActorLocation();const float Surface=Start.Z+1800;FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Floor=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(FVector(Start.X,Start.Y,Surface-20)),Params);Check(Floor.IsValid(),TEXT("transient_floor"));if(Failures){Finish();return;}
  auto* Shape=NewObject<UBoxComponent>(Floor.Get(),NAME_None,RF_Transient);Floor->AddInstanceComponent(Shape);Floor->SetRootComponent(Shape);Shape->SetBoxExtent(FVector(2500,2500,20));Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Shape->SetCollisionObjectType(ECC_WorldStatic);Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->SetGenerateOverlapEvents(false);Shape->SetCanEverAffectNavigation(false);Shape->RegisterComponent();
  auto* Mesh=NewObject<UStaticMeshComponent>(Floor.Get(),NAME_None,RF_Transient);Floor->AddInstanceComponent(Mesh);Mesh->SetupAttachment(Shape);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetRelativeScale3D(FVector(50,50,.4f));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();Floor->SetActorLocation(FVector(Start.X,Start.Y,Surface-20));
  Character->SetActorLocation(FVector(Start.X,Start.Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);PC->SetControlRotation(FRotator(-12,0,0));
  Capture=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 W,int32 H,const TArray<FColor>& Pixels){if(Self->PendingPhoto.IsEmpty()||Self->bFinished)return;auto& Pic=Self->Photos.AddDefaulted_GetRef();Pic.Name=Self->PendingPhoto;Pic.Width=W;Pic.Height=H;Pic.Pixels=Pixels;Self->PendingPhoto.Empty();++Self->CompletedPhotos;});
  if(bRendered){FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();}
  Results+=FString::Printf(TEXT("primary_asset=%s\nsample_skill=%s\naura_source=%s\nprototype_duration=%.4f\nprototype_cooldown=%.4f\nprototype_attack_speed=%.4f\nprototype_move=%.4f\nprototype_damage=%.4f\nprototype_skill_range=%.4f\n"),*Combat()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)->GetPathName(),*SampleSkill->GetPathName(),*Identity->AuraSystem.ToSoftObjectPath().ToString(),Identity->DurationSeconds,Identity->CooldownSeconds,Identity->AttackSpeedMultiplier,Identity->MovementMultiplier,Identity->DamageMultiplier,Identity->SkillRangeMultiplier);
  Later(.8,[this]{Key(EKeys::LeftShift,true);Later(.2,[this]{BaseSprint=Character->GetCharacterMovement()->MaxWalkSpeed;Key(EKeys::LeftShift,false);Later(.2,[this]{Baseline();});});});
 }
 void Baseline()
 {
  BaseRaw=FRaw::Read(*Character);BaseWalk=Character->GetCharacterMovement()->MaxWalkSpeed;BaseCombatSpeed=FRogue10mWeaponIdentityTestAccess::CombatSpeed(*Combat());SavedBefore=Combat()->CaptureCombat();Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("baseline_no_identity_visuals"));Sample(TEXT("baseline"));Photo(TEXT("01_baseline.png"));Later(.4,[this]{Ctrl([this]{Active();});});
 }
 void Active()
 {
  Check(Identity->IsIdentityActive(),TEXT("Ctrl_activates_production_knuckle_identity"));Check(FRaw::Read(*Character).Same(BaseRaw),TEXT("activation_does_not_write_base_GAS_stats"));Check(FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed,BaseWalk*Identity->MovementMultiplier,.1f),TEXT("actual_walk_speed_boost"));Check(FMath::IsNearlyEqual(FRogue10mWeaponIdentityTestAccess::CombatSpeed(*Combat()),BaseCombatSpeed*Identity->AttackSpeedMultiplier,.0001f),TEXT("combat_speed_boost"));Check(FMath::IsNearlyEqual(Identity->GetDamageMultiplier(*SampleSkill),Identity->DamageMultiplier),TEXT("damage_overlay"));Check(FMath::IsNearlyEqual(Identity->GetRangeMultiplier(*SampleSkill),Identity->SkillRangeMultiplier),TEXT("skill_enhancement_overlay"));
  auto* N=FRogue10mWeaponIdentityTestAccess::Aura(*Identity);auto* P=FRogue10mWeaponIdentityTestAccess::Post(*Identity);Check(N&&N->IsRegistered()&&N->IsActive()&&!N->IsComplete(),TEXT("active_aura_emits"));Check(P&&P->IsRegistered()&&P->BlendWeight>0.f,TEXT("active_post_process_registered"));const float End=FRogue10mWeaponIdentityTestAccess::End(*Identity);Check(!Combat()->TryActivateWeaponIdentity()&&FRogue10mWeaponIdentityTestAccess::End(*Identity)==End,TEXT("repeated_activation_does_not_stack_or_extend"));
  if(bRendered){FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();}Sample(TEXT("active"));Photo(TEXT("02_active.png"));Key(EKeys::LeftShift,true);Later(.2,[this]{Check(FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed,BaseSprint*Identity->MovementMultiplier,.1f),TEXT("actual_sprint_speed_boost"));Key(EKeys::LeftShift,false);Later(5.2,[this]{LateActive();});});
 }
 void LateActive()
 {
  Check(Identity->IsIdentityActive(),TEXT("identity_still_active_before_eight_second_expiry"));auto* N=FRogue10mWeaponIdentityTestAccess::Aura(*Identity);Check(N&&N->IsActive()&&!N->IsComplete(),TEXT("aura_sustains_for_buff_duration"));Check(FRogue10mWeaponIdentityTestAccess::Post(*Identity)!=nullptr,TEXT("post_process_sustains"));Sample(TEXT("late_active"));Photo(TEXT("03_late_active.png"));Later(Identity->GetRemainingSeconds()+.4,[this]{Expired();});
 }
 void Expired()
 {
  Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("expiry_removes_visuals"));Check(FRaw::Read(*Character).Same(BaseRaw),TEXT("expiry_preserves_base_stats"));Check(FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed,BaseWalk,.1f),TEXT("expiry_restores_actual_walk_speed"));Check(FMath::IsNearlyEqual(FRogue10mWeaponIdentityTestAccess::CombatSpeed(*Combat()),BaseCombatSpeed,.0001f),TEXT("expiry_restores_combat_speed"));const auto After=Combat()->CaptureCombat();Check(FRogue10mCombatSnapshot::StaticStruct()->CompareScriptStruct(&SavedBefore,&After,0),TEXT("active_buff_excluded_from_durable_combat_snapshot"));Sample(TEXT("expired"));Photo(TEXT("04_expired.png"));Later(.4,[this]{FRogue10mWeaponIdentityTestAccess::ReadyForIndependentCase(*Identity);Ctrl([this]{SwapActive();});});
 }
 void SwapActive()
 {
  Check(Identity->IsIdentityActive(),TEXT("independent_swap_case_active"));Sample(TEXT("swap_active"));Photo(TEXT("05_before_weapon_swap.png"));Later(.3,[this]{Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("weapon_swap_clears_identity_immediately"));Check(Identity->GetMovementMultiplier()==1.f&&Identity->GetAttackSpeedMultiplier()==1.f,TEXT("weapon_swap_clears_multipliers"));Sample(TEXT("swap_cleared"));Photo(TEXT("06_weapon_swap_cleared.png"));Later(.4,[this]{Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("re_equipping_knuckle_does_not_resurrect_buff"));FRogue10mWeaponIdentityTestAccess::ReadyForIndependentCase(*Identity);Ctrl([this]{RunEnd();});});});
 }
 void RunEnd()
 {
  Check(Identity->IsIdentityActive(),TEXT("run_end_handler_case_active"));FRogue10mWeaponIdentityTestAccess::RunEndHandler(*Identity);Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("run_end_handler_clears_visuals_and_overlay"));Sample(TEXT("run_end_handler_cleared"));FRogue10mWeaponIdentityTestAccess::ReadyForIndependentCase(*Identity);Ctrl([this]{DeathActive();});
 }
 void DeathActive()
 {
  Check(Identity->IsIdentityActive(),TEXT("independent_death_case_active"));Sample(TEXT("death_active"));Photo(TEXT("07_before_death.png"));Later(.3,[this]{Character->Die();Later(.4,[this]{Check(Character->IsDead(),TEXT("production_Die_sets_dead"));Check(!Identity->IsIdentityActive()&&VisualsGone(),TEXT("death_clears_visuals_and_overlay"));Check(Identity->GetMovementMultiplier()==1.f&&Identity->GetAttackSpeedMultiplier()==1.f,TEXT("death_clears_multipliers"));Check(Character->GetCharacterMovement()->MovementMode==MOVE_None,TEXT("cleanup_does_not_revive_movement"));Sample(TEXT("death_cleared"));Photo(TEXT("08_death_cleared.png"));Later(.5,[this]{Finish();});});});
 }
 void Finish(bool Exit=true)
 {
  if(bFinished)return;bFinished=true;Next=nullptr;FWorldDelegates::OnWorldCleanup.Remove(Cleanup);UGameViewportClient::OnScreenshotCaptured().Remove(Capture);Key(EKeys::LeftShift,false);Key(EKeys::LeftControl,false);if(Identity.IsValid())Identity->DeactivateIdentity();if(Floor.IsValid())Floor->Destroy();if(bRendered)Check(RequestedPhotos==8&&CompletedPhotos==8&&PendingPhoto.IsEmpty(),TEXT("eight_rendered_captures"));for(const auto& P:Photos){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(P.Width,P.Height,P.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Output/P.Name)),TEXT("capture_saved"));}Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("samples.csv"))),TEXT("numeric_samples_saved"));Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nrendered=%d\ncaptures=%d\ninput=programmatic_Controller_InputKey\nphysical_input=not_performed\nfull_run=not_performed\nrun_end_case=direct_production_handler_not_full_run_settlement\nrepeat_lifecycle_cases=QA_only_cooldown_reset_between_independent_cases\nprototype_balance=not_user_confirmed\nproduction_assets_edited=none\nfirst_person_QA=paused\n"),Checks,Failures,bRendered,CompletedPhotos);if(!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))))++Failures;UE_LOG(LogRogue10m,Display,TEXT("RESULT=WEAPON_IDENTITY_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 struct FPhoto{FString Name;int32 Width=0,Height=0;TArray<FColor> Pixels;};TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mWeaponIdentityComponent> Identity;TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;TWeakObjectPtr<AActor> Floor;TStrongObjectPtr<URogue10mAttackSkillData> SampleSkill;TFunction<void()> Next;FDelegateHandle Cleanup,Capture;TArray<FPhoto> Photos;FString Label,Output,Results,Frames,Stage,PendingPhoto;FRaw BaseRaw;FRogue10mCombatSnapshot SavedBefore;double Deadline=0,NextAt=0;float BaseWalk=0,BaseSprint=0,BaseCombatSpeed=0;int32 Checks=0,Failures=0,RequestedPhotos=0,CompletedPhotos=0;bool bFinished=false,bRendered=false;
};
static TSharedPtr<FRun> Active;static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEAPON_IDENTITY_FAILED reason=no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}static FAutoConsoleCommand Command(TEXT("Rogue10m.TestWeaponIdentity"),TEXT("Fresh WeaponIdentity QA only: production Ctrl, timed expiry, swap, death and rendered aura/PP. No production save writes."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
