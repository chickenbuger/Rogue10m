// Isolated real StartHub NPC service UI QA. Slate routing is not physical input.
#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTutorialDataAsset.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "UnrealClient.h"
namespace Rogue10mNpcDialogueQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
 bool Finished() const{return bFinished;}
 void Start(UWorld& W)
 {
  World=&W;FString TownSlot,Prefix,Keys;const TCHAR* Args=FCommandLine::Get();
  FParse::Value(Args,TEXT("RogueNpcDialogueLabel="),Label);FParse::Value(Args,TEXT("RogueProfileSaveSlot="),TownSlot);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Valid=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label){Valid&=FChar::IsAlnum(C)||C==TEXT('_');}
  TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  Output=FPaths::ProjectDir()/TEXT("DevLog/20261110_NpcDialogue")/Label;
  if(!Valid||W.GetNetMode()!=NM_Standalone||TownSlot!=TEXT("Rogue10m_Town_QA_NpcDialog_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_NpcDialog_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_NpcDialog_")+Label||UGameplayStatics::DoesSaveGameExist(TownSlot,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty()||IFileManager::Get().DirectoryExists(*Output)||FParse::Param(Args,TEXT("nullrhi")))
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=NPC_DIALOGUE_FAILED exact_fresh_rendered_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  bManual=FParse::Param(Args,TEXT("RogueNpcDialogueManual"));IFileManager::Get().MakeDirectory(*Output,true);Deadline=FPlatformTime::Seconds()+(bManual?900.:180.);
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
  {
   if(Self->bFinished){return false;}
   if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("bounded_watchdog"));Self->Finish();return false;}
   if(Self->bManual&&IFileManager::Get().FileExists(*(Self->Output/TEXT("manual-finish.txt")))){Self->Finish();return false;}
   if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto Work=MoveTemp(Self->Next);Work();}
   return !Self->bFinished;
  }),.02f);
  Later(4.,[this]{Prepare();});
 }
private:
 void Check(bool Good,const TCHAR* Name){++Checks;Results+=FString(Good?TEXT("PASS "):TEXT("FAIL "))+Name+TEXT("\n");if(!Good){++Failures;UE_LOG(LogRogue10m,Error,TEXT("NPC_DIALOGUE FAIL %s"),Name);}}
 void Later(double Delay,TFunction<void()> Work){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(Work);}
 void Photo(const TCHAR* Name){FScreenshotRequest::RequestScreenshot(Output/Name,true,false);}
 void PhotoThen(const TCHAR* Name,TFunction<void()> After)
 { Photo(Name);WaitPhoto(FString(Name),FPlatformTime::Seconds()+8.,MoveTemp(After)); }
 void WaitPhoto(FString Name,double Limit,TFunction<void()> After)
 {
  if(!FScreenshotRequest::IsScreenshotRequested()&&IFileManager::Get().FileSize(*(Output/Name))>1024){After();return;}
  if(FPlatformTime::Seconds()>Limit){Check(false,TEXT("rendered_capture_completed_before_UI_transition"));Finish();return;}
  Later(.05,[this,Name=MoveTemp(Name),Limit,After=MoveTemp(After)]()mutable{WaitPhoto(MoveTemp(Name),Limit,MoveTemp(After));});
 }
 void Key(FKey K){auto& A=FSlateApplication::Get();A.ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));A.ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));}
 void FPress(TFunction<void()> After)
 {
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F,IE_Pressed,1.f));
  Later(.1,[this,After=MoveTemp(After)]()mutable{PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F,IE_Released,0.f));Later(.35,MoveTemp(After));});
 }
 bool Click(const TCHAR* Name)
 {
  auto* W=NPC.IsValid()?NPC->GetTutorialWidget():nullptr;auto* B=W?Cast<UButton>(W->GetWidgetFromName(Name)):nullptr;FWidgetPath P;
  if(!B||!B->GetIsEnabled()||!FSlateApplication::Get().GeneratePathToWidgetUnchecked(B->TakeWidget(),P)){Check(false,TEXT("click_has_real_button_path"));return false;}
  const FVector2D Point=B->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(.5,.5));
  auto& A=FSlateApplication::Get();const FWidgetPath Hit=A.LocateWindowUnderMouse(Point,TArray<TSharedRef<SWindow>>{P.GetWindow()},false,0);
  const bool Good=Hit.IsValid()&&Hit.ContainsWidget(&B->TakeWidget().Get());Check(Good,*FString::Printf(TEXT("actual_hit_test_%s"),Name));if(!Good){return false;}
  A.RoutePointerMoveEvent(Hit,FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::Invalid,0,FModifierKeysState()),false);
  A.RoutePointerDownEvent(Hit,FPointerEvent(0,Point,Point,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
  A.RoutePointerUpEvent(Hit,FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState()));return true;
 }
 bool Restored()const{return !NPC->IsDialogueOpen()&&!PC->GetDialogueNPC()&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored()&&!PC->bShowMouseCursor&&!PC->IsAnyBlockingWindowVisible();}
 void Prepare()
 {
  PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());auto* GI=World->GetGameInstance();auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Save=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  Check(PC.IsValid()&&Profiles&&Save.IsValid()&&FSlateApplication::IsInitialized(),TEXT("real_profile_controller_save_and_Slate"));if(Failures){Finish();return;}
  FRogue10mCharacterProfile Profile;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("NpcDialogQA"),FRogue10mCharacterAppearance(),Profile)&&Profiles->SelectProfile(Profile.ProfileId),TEXT("normal_fresh_profile_created_selected"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Player=Cast<ARogue10mCharacter>(PC->GetPawn());
  State=Player.IsValid()?Player->GetPlayerState<ARogue10mPlayerState>():nullptr;Inventory=Player.IsValid()?Player->GetInventoryComponent():nullptr;Town=State.IsValid()?State->GetTownProgressionComponent():nullptr;
  for(TActorIterator<ARogue10mTutorialNPC> It(World.Get());It;++It){NPC=*It;break;}
  Run=World->GetGameState<ARogue10mGameState>();
  Check(Player.IsValid()&&State.IsValid()&&Inventory.IsValid()&&Town.IsValid()&&NPC.IsValid()&&NPC->TutorialData&&NPC->TutorialData->StartingItem&&Run.IsValid()&&Save->IsAttachedTo(*Player),TEXT("actual_StartHub_NPC_data_and_save_attached"));if(Failures){Finish();return;}
  Check(Player->IsThirdPersonGameplayActive(),TEXT("production_third_person_mode_active"));
  Near=NPC->GetActorLocation()+FVector(180,0,95);Player->SetActorLocation(Near,false,nullptr,ETeleportType::TeleportPhysics);
  PC->SetControlRotation((NPC->GetActorLocation()-Near).Rotation()+FRotator(-12,0,0));
  const auto& D=NPC->ServiceData?*NPC->ServiceData:*GetDefault<URogue10mTownServiceDataAsset>();PotionCost=D.PotionGoldCost;EnhanceCost=D.EnhancementGoldCost;
  Inventory->AddGold(1000);Remaining=Run->GetRemainingRunSeconds();
  Later(1.,[this]
  {
   Photo(TEXT("01_proximity_prompt.png"));
   if(bManual){FFileHelper::SaveStringToFile(TEXT("Ready: normal isolated profile, real NPC, 3P; use OS input.\n"),*(Output/TEXT("manual-ready.txt")));return;}
   Later(.7,[this]{FPress([this]{Opened();});});
  });
 }
 void Opened()
 {
  auto* W=NPC->GetTutorialWidget();Check(NPC->IsDialogueOpen()&&PC->IsNpcDialogueVisible()&&PC->GetDialogueNPC()==NPC.Get()&&W,TEXT("F_real_input_stack_opens_owned_dialogue"));
  Check(W&&W->GetVisibility()==ESlateVisibility::SelfHitTestInvisible&&PC->bShowMouseCursor&&PC->IsAnyBlockingWindowVisible()&&PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored(),TEXT("mouse_hit_test_focus_cursor_and_gameplay_guard"));if(Failures){Finish();return;}
  PhotoThen(TEXT("02_selectable_dialogue.png"),[this]{Key(EKeys::Down);Check(NPC->GetSelectedServiceIndex()==1,TEXT("Slate_down_selects_second_service"));Key(EKeys::Up);Key(EKeys::Up);Check(NPC->GetSelectedServiceIndex()==6,TEXT("Slate_up_wraps_to_last_service"));Key(EKeys::Down);
  const int32 Gold=Inventory->GetGold();Key(EKeys::Enter);Check(Inventory->GetGold()==Gold-PotionCost,TEXT("Slate_enter_buys_exactly_one_potion"));
  FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,true,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
  Check(Inventory->GetGold()==Gold-PotionCost,TEXT("held_enter_repeat_does_not_purchase_again"));
  Later(.5,[this]{Enhance();});});
 }
 void Enhance()
 {
  Check(!Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon)&&Player->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed,TEXT("fresh_novice_has_no_equipped_weapon"));
  const int32 UnarmedGold=Inventory->GetGold();Click(TEXT("TownService1"));
  Check(Inventory->GetGold()==UnarmedGold&&!NPC->GetDialogueStatus().IsEmpty(),TEXT("mouse_unarmed_enhancement_rejected_without_charge"));
  int32 WeaponContainer=INDEX_NONE;FGuid WeaponId;
  const auto& Containers=Inventory->GetInventoryContainers();
  for(int32 Index=0;Index<Containers.Num()&&WeaponContainer==INDEX_NONE;++Index)
  {
   for(const auto& Entry:Containers[Index].Entries)
   {
    if(Entry.ItemData&&Entry.ItemData->EquipSlotType==ERogue10mInventorySlotType::MainWeapon&&Entry.ItemData->WeaponType==ERogue10mWeaponType::Knuckle)
    {WeaponContainer=Index;WeaponId=Entry.InstanceId;break;}
   }
  }
  Check(WeaponContainer!=INDEX_NONE&&Inventory->TryEquipGridItem(WeaponContainer,WeaponId)&&Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon),TEXT("QA_explicitly_equips_existing_starter_knuckle_for_enhancement"));
  if(Failures){Finish();return;}
  const int32 Gold=Inventory->GetGold(),Level=Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon);
  Click(TEXT("TownService1"));Check(Inventory->GetGold()==Gold-EnhanceCost*(Level+1)&&Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon)==Level+1,TEXT("mouse_weapon_enhance_exact_cost_and_level"));
  const int32 ArmorGold=Inventory->GetGold();const int32 ArmorLevel=Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::Armor);const bool HasArmor=Inventory->GetEquippedItemData(ERogue10mInventorySlotType::Armor)!=nullptr;
  Click(TEXT("TownService2"));Check(HasArmor?(Inventory->GetGold()==ArmorGold-EnhanceCost*(ArmorLevel+1)&&Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::Armor)==ArmorLevel+1):(Inventory->GetGold()==ArmorGold&&!NPC->GetDialogueStatus().IsEmpty()),TEXT("mouse_armor_service_success_or_explicit_missing_equipment"));
  Later(.4,[this]{Difficulties(0);});
 }
 void Difficulties(int32 Index)
 {
  const TCHAR* Names[]={TEXT("TownService4"),TEXT("TownService5"),TEXT("TownService6")};
  const ERogue10mDifficulty Values[]={ERogue10mDifficulty::Easy,ERogue10mDifficulty::Normal,ERogue10mDifficulty::Hard};
  Click(Names[Index]);Check(Town->GetDifficulty()==Values[Index]&&!NPC->GetDialogueStatus().IsEmpty(),*FString::Printf(TEXT("mouse_difficulty_%d_applies_and_reports_result"),Index));
  if(Index<2){Later(.4,[this,Index]{Difficulties(Index+1);});}else{Later(.4,[this]{Supplies();});}
 }
 void Supplies()
 {
  Check(!NPC->HasClaimedReward(Player.Get()),TEXT("fresh_supply_receipt_unclaimed"));Click(TEXT("TownClaimSupplies"));Check(NPC->HasClaimedReward(Player.Get())&&!NPC->GetDialogueStatus().IsEmpty(),TEXT("mouse_claim_records_supply_receipt"));
  const auto* W=NPC->GetTutorialWidget();const auto* B=W?W->GetWidgetFromName(TEXT("TownClaimSupplies")):nullptr;Check(B&&B->GetVisibility()==ESlateVisibility::Collapsed,TEXT("claimed_supply_choice_disappears"));
  const int32 Gold=Inventory->GetGold();Check(!NPC->ClaimStartingSupplies()&&Inventory->GetGold()==Gold,TEXT("repeat_supply_claim_rejected"));
  PhotoThen(TEXT("03_service_result.png"),[this]{Click(TEXT("TownService3"));Later(.5,[this]{Traits();});});
 }
 void Traits()
 {
  Check(PC->IsTraitWindowVisible()&&NPC->IsDialogueOpen()&&PC->bShowMouseCursor,TEXT("mouse_traits_opens_child_without_losing_dialogue"));PhotoThen(TEXT("04_trait_branch.png"),[this]{Key(EKeys::Escape);
  Check(!PC->IsTraitWindowVisible()&&NPC->IsDialogueOpen()&&PC->IsNpcDialogueVisible()&&PC->bShowMouseCursor&&!UGameplayStatics::IsGamePaused(World.Get()),TEXT("trait_escape_returns_to_dialogue_and_releases_only_child_pause"));
  Later(.4,[this]
  {
   auto* Widget=NPC->GetTutorialWidget();Check(Widget&&Widget->HasUserFocus(PC.Get()),TEXT("trait_back_restores_actual_keyboard_focus_before_mouse_click"));
   Key(EKeys::Down);Key(EKeys::Down);Key(EKeys::Down);Key(EKeys::Down);Check(NPC->GetSelectedServiceIndex()==0,TEXT("keyboard_navigation_operates_after_trait_back"));
   const int32 Gold=Inventory->GetGold();Key(EKeys::Enter);Check(Inventory->GetGold()==Gold-PotionCost,TEXT("keyboard_enter_operates_after_trait_back"));
   Click(TEXT("TownService0"));Check(Inventory->GetGold()==Gold-2*PotionCost,TEXT("mouse_choice_still_operates_after_trait_back"));PC->ToggleInventory();Later(.4,[this]{Menus();});
  });});
 }
 void Menus()
 {
  Check(!NPC->IsDialogueOpen()&&!PC->GetDialogueNPC()&&PC->IsAnyBlockingWindowVisible(),TEXT("inventory_replaces_dialogue_without_stale_context"));Check(NPC->GetTutorialWidget()&&NPC->GetTutorialWidget()->GetVisibility()==ESlateVisibility::Collapsed,TEXT("inventory_takeover_hides_NPC_prompt_even_while_world_paused"));PC->CloseAllBlockingPanels();Check(Restored(),TEXT("inventory_close_restores_gameplay_input"));
  FPress([this]
  {
   Check(NPC->IsDialogueOpen(),TEXT("F_reopens_after_inventory"));PC->ToggleSettings();Check(!NPC->IsDialogueOpen()&&PC->IsSettingsVisible(),TEXT("settings_replaces_dialogue"));Check(NPC->GetTutorialWidget()&&NPC->GetTutorialWidget()->GetVisibility()==ESlateVisibility::Collapsed,TEXT("settings_takeover_hides_NPC_prompt_even_while_world_paused"));PC->CloseAllBlockingPanels();Check(Restored(),TEXT("settings_close_restores_gameplay"));
   FPress([this]
   {
    Check(NPC->IsDialogueOpen(),TEXT("F_reopens_for_owned_ignore_pairing"));PC->SetIgnoreLookInput(true);PC->SetIgnoreMoveInput(true);NPC->CloseDialogue();
    Check(PC->IsLookInputIgnored()&&PC->IsMoveInputIgnored(),TEXT("dialogue_close_preserves_unrelated_ignore_reasons"));PC->SetIgnoreLookInput(false);PC->SetIgnoreMoveInput(false);Check(Restored(),TEXT("all_owned_ignore_pairs_balanced"));
    FPress([this]{Player->SetActorLocation(Near+FVector(1800,0,0),false,nullptr,ETeleportType::TeleportPhysics);Later(.5,[this]{Distance();});});
   });
  });
 }
 void Distance()
 {
  Check(Restored(),TEXT("leaving_range_closes_dialogue_cursor_and_input_context"));const int32 Gold=Inventory->GetGold();NPC->ActivateService(0);Check(Inventory->GetGold()==Gold,TEXT("stale_service_outside_range_cannot_spend_gold"));
  Player->SetActorLocation(Near,false,nullptr,ETeleportType::TeleportPhysics);Later(.6,[this]{FPress([this]
  {
   Check(NPC->IsDialogueOpen(),TEXT("range_reentry_single_F_reopens_dialogue"));Click(TEXT("TownCloseDialogue"));Check(Restored(),TEXT("mouse_close_button_restores_gameplay"));
   const int32 Gold=Inventory->GetGold(),Level=Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon);const auto Difficulty=Town->GetDifficulty();
   Check(Save->SaveNow(),TEXT("selected_isolated_progression_save_succeeds"));Save->DetachCharacter(*Player,false);Check(Save->AttachCharacter(*Player)&&Save->IsSaveReady()&&Inventory->GetGold()==Gold&&Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon)==Level&&Town->GetDifficulty()==Difficulty&&NPC->HasClaimedReward(Player.Get()),TEXT("service_cost_equipment_difficulty_and_receipt_roundtrip"));
   Check(Run->GetRunPhase()==ERogue10mRunPhase::WaitingToStart&&FMath::IsNearlyEqual(Remaining,Run->GetRemainingRunSeconds()),TEXT("safe_hub_timer_never_starts_or_loses_time_during_dialogue"));
   Photo(TEXT("05_closed_restored.png"));Later(.6,[this]{FPress([this]{UGameplayStatics::ApplyDamage(Player.Get(),1000000.f,PC.Get(),NPC.Get(),nullptr);Later(.6,[this]{Death();});});});
  });});
 }
 void Death()
 {
  Check(Player->IsDead(),TEXT("real_damage_death_boundary_reached"));Check(!NPC->IsDialogueOpen()&&!PC->GetDialogueNPC(),TEXT("death_closes_dialogue_and_owned_context"));const int32 Gold=Inventory->GetGold();NPC->ActivateService(0);Check(Inventory->GetGold()==Gold&&!NPC->BeginDialogue(Player.Get()),TEXT("dead_player_cannot_reopen_or_purchase"));Photo(TEXT("06_death_boundary.png"));Later(1.,[this]{Finish();});
 }
 void Finish()
 {
  if(bFinished){return;}bFinished=true;if(NPC.IsValid()){NPC->CloseDialogue();}
  const FString Meta=FString::Printf(TEXT("label=%s\nchecks=%d\nfailures=%d\nmanual_mode=%d\ninput=Controller_InputKey_and_actual_Slate_hit_test_pointer_and_key_routing\nphysical_input_performed=0\nnormal_field_run=0\nvalidation_scope=%s\n"),*Label,Checks,Failures,bManual,bManual?TEXT("manual_setup_only_service_assertions_not_run"):TEXT("automated_service_suite"));
  Check(FFileHelper::SaveStringToFile(Meta+Results,*(Output/TEXT("results.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM),TEXT("results_written"));
  UE_LOG(LogRogue10m,Display,TEXT("RESULT=NPC_DIALOGUE_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):(bManual?TEXT("MANUAL_SETUP_COMPLETE"):TEXT("PASSED")),Checks,Failures,*Output);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Player;TWeakObjectPtr<ARogue10mPlayerState> State;TWeakObjectPtr<ARogue10mTutorialNPC> NPC;TWeakObjectPtr<ARogue10mGameState> Run;
 TWeakObjectPtr<URogue10mInventoryComponent> Inventory;TWeakObjectPtr<URogue10mTownProgressionComponent> Town;TWeakObjectPtr<URogue10mProgressionSubsystem> Save;
 FString Label,Output,Results;TFunction<void()> Next;double NextAt=0,Deadline=0;FVector Near;float Remaining=0;int32 Checks=0,Failures=0,PotionCost=0,EnhanceCost=0;bool bFinished=false,bManual=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
 if(!GEngine||GIsEditor||(Active&&!Active->Finished())){return;}
 for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestNpcDialogue"),TEXT("Exact fresh isolated NpcDialog slots only; actual StartHub NPC selectable UI rendered QA."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
