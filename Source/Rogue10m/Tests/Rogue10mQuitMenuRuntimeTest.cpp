// Isolated actual UMG delegates and optional Slate keys; no physical-input/full-run claim.
#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMainMenuWidget.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mQuitMenuWidget.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunResultWidget.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Rogue10mQuitMenuQA
{
template<typename T> static bool Same(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }

// Fresh pawns recreate instance GUIDs. Compare ownership, placement and item state instead.
static FString InventoryLayout(const FRogue10mInventorySnapshot& Snapshot)
{
	TArray<FString> Rows;
	const auto AddStack=[&Rows](const FString& Place,const FRogue10mItemStack& Stack,const FString& Asset)
	{
		Rows.Add(FString::Printf(TEXT("%s|%s|category=%d|slot=%d|weapon=%d|qty=%d|enhance=%d,%.9g,%.9g|locked=%d|occupied=%d"),
			*Place,*Asset,int32(Stack.Category),int32(Stack.EquipSlotType),int32(Stack.WeaponType),Stack.Quantity,
			Stack.EnhancementLevel,Stack.EnhancementAttackBonus,Stack.EnhancementDefenseBonus,Stack.bLocked,Stack.bOccupied));
	};
	const auto Equipment=[&AddStack](const TCHAR* Side,const TArray<FRogue10mSavedEquipmentSlot>& Slots)
	{
		for(int32 Index=0;Index<Slots.Num();++Index)
		{
			const auto& Slot=Slots[Index];
			AddStack(FString::Printf(TEXT("equipment:%s:%d:type=%d:locked=%d:equipped=%d:has=%d"),Side,Index,int32(Slot.Values.SlotType),Slot.Values.bLocked,Slot.Values.bEquipped,Slot.Values.bHasEquippedItem),Slot.Values.EquippedItem,Slot.ItemAsset.ToSoftObjectPath().ToString());
		}
	};
	Equipment(TEXT("left"),Snapshot.LeftEquipment); Equipment(TEXT("right"),Snapshot.RightEquipment);
	for(int32 Index=0;Index<Snapshot.ItemSlots.Num();++Index)
	{ const auto& Stack=Snapshot.ItemSlots[Index]; AddStack(FString::Printf(TEXT("itemslot:%d"),Index),Stack.Values,Stack.ItemAsset.ToSoftObjectPath().ToString()); }
	for(const auto& Container:Snapshot.Containers)
	{
		Rows.Add(FString::Printf(TEXT("container:%s:size=%d,%d:bag=%s"),*Container.ContainerId.ToString(),Container.GridSize.X,Container.GridSize.Y,*Container.BagAsset.ToSoftObjectPath().ToString()));
		for(const auto& Entry:Container.Entries)
		{ Rows.Add(FString::Printf(TEXT("grid:%s:position=%d,%d:asset=%s:qty=%d:enhance=%d,%.9g,%.9g"),*Container.ContainerId.ToString(),Entry.Values.Position.X,Entry.Values.Position.Y,*Entry.ItemAsset.ToSoftObjectPath().ToString(),Entry.Values.Quantity,Entry.Values.EnhancementLevel,Entry.Values.EnhancementAttackBonus,Entry.Values.EnhancementDefenseBonus)); }
	}
	for(int32 Index=0;Index<Snapshot.ConsumableQuickSlots.Num();++Index) { Rows.Add(FString::Printf(TEXT("quickslot:%d:itemslot=%d"),Index,Snapshot.ConsumableQuickSlots[Index])); }
	Rows.Add(FString::Printf(TEXT("currency:%d,%d"),Snapshot.Gold,Snapshot.Crystals)); Rows.Sort();
	return FString::Join(Rows,TEXT("\n"));
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World=&InWorld;
		FString ProfileSlot,Prefix,Keys;
		const TCHAR* Args=FCommandLine::Get();
		FParse::Value(Args,TEXT("RogueQuitMenuLabel="),Label);
		FParse::Value(Args,TEXT("RogueQuitMenuMode="),Mode);
		FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot);
		FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);
		FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
		bool Valid=!Label.IsEmpty() && Label.Len()<=24;
		for(TCHAR Ch:Label) { Valid &= FChar::IsAlnum(Ch) || Ch==TCHAR('_'); }
		if(!Valid || ProfileSlot!=TEXT("Rogue10m_Town_QA_Quit_")+Label
			|| Prefix!=TEXT("Rogue10m_Progression_QA_Quit_")+Label || Keys!=TEXT("Rogue10m_Fighter_QA_Quit_")+Label
			|| (Mode!=TEXT("Hub") && Mode!=TEXT("Run") && Mode!=TEXT("Failure") && Mode!=TEXT("Menu") && Mode!=TEXT("Terminal") && Mode!=TEXT("Restore"))
			|| InWorld.GetNetMode()!=NM_Standalone)
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=QUIT_MENU_FAILED reason=exact_QA_namespace_and_mode_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/quit-menu-20261111")/(Mode==TEXT("Restore")?Label+TEXT("_restore"):Label);
		if(IFileManager::Get().DirectoryExists(*Output))
		{ UE_LOG(LogRogue10m,Error,TEXT("RESULT=QUIT_MENU_FAILED reason=fresh_evidence_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
		IFileManager::Get().MakeDirectory(*Output,true);
		bRendered=!FParse::Param(Args,TEXT("nullrhi")); Deadline=FPlatformTime::Seconds()+90.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if(Self->bFinished) { return false; }
			if(FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Abort(); return false; }
			if(Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
			return !Self->bFinished;
		}),.02f);
		Later(2.0,[this]{Prepare();});
	}
	bool Finished() const { return bFinished; }
private:
	void Check(bool OK,const TCHAR* Why)
	{
		++Checks; Results+=FString::Printf(TEXT("%s %s\n"),OK?TEXT("PASS"):TEXT("FAIL"),Why);
		if(!OK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("QUIT_MENU FAIL %s"),Why); }
	}
	void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
	void Photo(const TCHAR* Name) { if(bRendered) { FScreenshotRequest::RequestScreenshot(Output/Name,true,false); } }
	UButton* Button(const TCHAR* Name) const { return Menu.IsValid()?Cast<UButton>(Menu->GetWidgetFromName(Name)):nullptr; }
	void Click(const TCHAR* Name)
	{
		auto* Target=Button(Name); Check(Target && Target->GetIsEnabled(),TEXT("requested_actual_quit_button_enabled"));
		if(Target && Target->GetIsEnabled()) { Target->OnClicked.Broadcast(); }
	}
	FString Text(const TCHAR* Name) const
	{ const auto* Target=Menu.IsValid()?Cast<UTextBlock>(Menu->GetWidgetFromName(Name)):nullptr; return Target?Target->GetText().ToString():FString(); }
	void GetPawn()
	{
		Character=PC.IsValid()?Cast<ARogue10mCharacter>(PC->GetPawn()):nullptr;
		State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
		Inventory=Character.IsValid()?Character->GetInventoryComponent():nullptr;
		Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
		Run=World.IsValid()?World->GetGameState<ARogue10mGameState>():nullptr;
		auto* GM=World.IsValid()?World->GetAuthGameMode<ARogue10mGameMode>():nullptr;
		Completion=GM?GM->GetRunCompletionComponent():nullptr;
	}
	void Prepare()
	{
		auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
		Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
		Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
		PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
		Check(PC.IsValid() && Profiles && Saves && Profiles->GetProfileCount()==(Mode==TEXT("Restore")?1:0),TEXT("expected_isolated_QA_profile_count_and_real_controller"));
		if(Failures) { Abort(); return; }
		if(Mode==TEXT("Restore")) { RestorePrepare(); return; }
		if(Mode==TEXT("Menu"))
		{
			Check(PC->IsMainMenuVisible(),TEXT("actual_main_menu_visible"));
			for(TObjectIterator<URogue10mMainMenuWidget> It;It;++It)
			{ if(It->GetWorld()==World.Get() && It->GetOwningPlayer()==PC.Get() && It->IsInViewport()) { MainMenu=*It; break; } }
			auto* Quit=MainMenu.IsValid()?Cast<UButton>(MainMenu->GetWidgetFromName(TEXT("UI_QuitGameButton"))):nullptr;
			Check(Quit && Quit->GetIsEnabled(),TEXT("authored_main_menu_quit_button_exists"));
			if(!Quit) { Abort(); return; }
			Quit->OnClicked.Broadcast(); Menu=PC->GetQuitMenuWidget();
			Check(PC->IsQuitGameConfirmationOpen() && !IsEngineExitRequested(),TEXT("main_menu_quit_opens_confirmation_without_exiting"));
			Photo(TEXT("01_main_menu_confirmation.png")); Later(.7,[this]{CancelTests();}); return;
		}
		Check(UGameplayStatics::GetCurrentLevelName(World.Get(),true)==TEXT("StartHub"),TEXT("real_start_hub_loaded"));
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("QuitQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("new_QA_profile_created_and_selected"));
		if(Failures) { Abort(); return; }
		ProfileId=Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
		Check(Character.IsValid() && State && Inventory && Tree && Run.IsValid() && Completion.IsValid() && Saves->IsSaveReady(),TEXT("real_hub_character_and_persistence_attached"));
		if(Failures) { Abort(); return; }
		SaveSlot=Saves->GetSaveSlotForProfile(ProfileId);
		SaveFile=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(SaveSlot+TEXT(".sav"));
		Inventory->AddGold(13);
		if(Mode==TEXT("Run") || Mode==TEXT("Terminal"))
		{
			Check(Saves->SaveRunEntryCurrencyBaseline(),TEXT("existing_post_payment_entry_baseline_saved"));
			if(Mode==TEXT("Run"))
			{
				TArray<uint8> StarterBytes;
				Check(FFileHelper::LoadFileToArray(StarterBytes,*SaveFile) && FFileHelper::SaveArrayToFile(StarterBytes,*(Output/TEXT("starter-before-run.sav"))),TEXT("actual_gold13_starter_snapshot_preserved_before_run_seed"));
				Check(FFileHelper::SaveStringToFile(InventoryLayout(Inventory->CaptureInventory()),*(Output/TEXT("starter-inventory.txt"))),TEXT("actual_starter_inventory_semantics_preserved"));
				Results+=FString::Printf(TEXT("starter_equipped_weapon=%s\n"),*StaticEnum<ERogue10mWeaponType>()->GetNameStringByValue(int64(Character->GetEquippedWeaponType())));
			}
			Run->StartRunWithDuration(1200.f);
		}
		if(Mode==TEXT("Run"))
		{
			// Nontrivial interrupted growth, mastery and loot make the second-process reset observable.
			State->AddExperience(500);
			for(int32 Index=0;Index<30;++Index) { Tree->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::Brawler); }
			auto* Sword=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
			int32 ContainerIndex=INDEX_NONE; FGuid LootId;
			Check(Sword && Inventory->TryAddGridItem(Sword,1,ContainerIndex,LootId),TEXT("interrupted_Run_fixture_acquires_actual_sword_loot"));
			Check(State->GetPlayerLevel()>1 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)>0,TEXT("interrupted_Run_fixture_has_nontrivial_growth_and_mastery"));
		}
		Check(Saves->SaveNow(),TEXT("stable_baseline_saved_before_open_cancel_test"));
		RunId=Saves->GetAttachedRunId(); Growth=State->CaptureGrowth(); Items=Inventory->CaptureInventory(); Skills=Tree->Capture();
		Check(FFileHelper::LoadFileToArray(Baseline,*SaveFile),TEXT("baseline_disk_bytes_captured"));
		PC->ToggleSettings(); Menu=PC->GetQuitMenuWidget();
		Check(PC->IsSettingsVisible() && Menu.IsValid(),TEXT("settings_exposes_native_quit_launcher"));
		if(Failures) { Abort(); return; }
		Click(TEXT("QuitMenuButton"));
		Check(PC->IsQuitGameConfirmationOpen() && !IsEngineExitRequested(),TEXT("launcher_opens_modal_without_exit"));
		Check(!Text(TEXT("QuitPolicyText")).IsEmpty(),TEXT("actual_dialog_displays_policy"));
		Remaining=Run->GetRemainingRunSeconds(); WorldTime=World->GetTimeSeconds();
		Photo(TEXT("01_confirmation.png")); Later(1.4,[this]{CancelTests();});
	}
	void RestorePrepare()
	{
		Check(UGameplayStatics::GetCurrentLevelName(World.Get(),true)==TEXT("StartHub"),TEXT("restore_process_uses_actual_start_hub"));
		const auto& Profile=Profiles->GetProfiles()[0]; ProfileId=Profile.ProfileId;
		SaveSlot=Saves->GetSaveSlotForProfile(ProfileId);
		SaveFile=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(SaveSlot+TEXT(".sav"));
		// Character::BeginPlay may already have restored and overwritten the current slot.
		// Read the immutable real post-quit write retained by the first Run process instead.
		const FString Source=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/quit-menu-20261111")/Label/TEXT("interrupted-before-exit.sav");
		TArray<uint8> SourceBytes;
		Check(FFileHelper::LoadFileToArray(SourceBytes,*Source) && !SourceBytes.IsEmpty(),TEXT("prior_Run_actual_post_quit_disk_snapshot_preserved"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Before(SourceBytes.IsEmpty()?nullptr:Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(SourceBytes)));
		Check(Before.IsValid() && Before->ProfileId==ProfileId && Before->RunId.IsValid() && Before->bEnteredRun && !Before->bRunCompleted && Before->Inventory.Gold==20 && Before->EntryCurrencies.IsValid(),TEXT("prior_Run_snapshot_is_same_QA_profile_interrupted_at_gold20"));
		if(Failures) { Abort(); return; }
		const FGuid InterruptedRunId=Before->RunId;
		TArray<uint8> StarterBytes;
		Check(FFileHelper::LoadFileToArray(StarterBytes,*(FPaths::GetPath(Source)/TEXT("starter-before-run.sav"))),TEXT("same_Run_starter_snapshot_before_seed_available"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Starter(StarterBytes.IsEmpty()?nullptr:Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(StarterBytes)));
		Check(Starter.IsValid() && Starter->ProfileId==ProfileId && Starter->RunId==InterruptedRunId && !Starter->bEnteredRun && Starter->Inventory.Gold==13,TEXT("starter_comparison_uses_same_profile_and_run_gold13_preentry_snapshot"));
		if(Failures) { Abort(); return; }
		bool HadSword=false;
		for(const auto& Container:Before->Inventory.Containers)
		{ for(const auto& Entry:Container.Entries) { HadSword |= Entry.ItemAsset.ToSoftObjectPath().ToString().Contains(TEXT("DA_Item_IronLongsword")); } }
		Check(Before->Growth.Level>1 && HadSword,TEXT("prior_Run_has_growth_and_loot_to_reset"));
		Check(Profiles->SelectProfile(ProfileId),TEXT("existing_QA_profile_selected_in_restore_process"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
		Check(Character.IsValid() && State && Inventory && Tree && Run.IsValid() && Saves->IsSaveReady(),TEXT("restore_enters_existing_profile_through_real_hub_controller"));
		if(Failures) { Abort(); return; }
		Check(Run->GetRunPhase()==ERogue10mRunPhase::WaitingToStart && !PC->IsRunResultVisible(),TEXT("interrupted_process_reopens_in_waiting_hub_without_result"));
		Check(State->GetPlayerLevel()==1 && State->GetCurrentExperience()==0 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)==0,TEXT("existing_interruption_handler_resets_growth_experience_and_mastery"));
		bool HasSword=false;
		for(const auto& Container:Inventory->GetInventoryContainers())
		{ for(const auto& Entry:Container.Entries) { HasSword |= Entry.ItemData && Entry.ItemData->WeaponType==ERogue10mWeaponType::GreatSword; } }
		for(const auto& Item:Inventory->GetItemSlots()) { HasSword |= Item.bOccupied && Item.WeaponType==ERogue10mWeaponType::GreatSword; }
		const FString StarterItems=InventoryLayout(Starter->Inventory);
		const FString RestoredItems=InventoryLayout(Inventory->CaptureInventory());
		Check(StarterItems==RestoredItems,TEXT("existing_interruption_handler_restores_actual_starter_assets_quantities_placement_and_equipped_slots"));
		Check(Character->GetEquippedWeaponType()==Starter->Growth.WeaponType,TEXT("existing_interruption_handler_restores_actual_preentry_equipped_weapon"));
		Check(FFileHelper::SaveStringToFile(RestoredItems,*(Output/TEXT("restored-inventory.txt"))),TEXT("restored_inventory_semantics_recorded_without_fresh_GUIDs"));
		Results+=FString::Printf(TEXT("starter_equipped_weapon=%s\nrestored_equipped_weapon=%s\nrestored_has_greatsword=%d\n"),
			*StaticEnum<ERogue10mWeaponType>()->GetNameStringByValue(int64(Starter->Growth.WeaponType)),*StaticEnum<ERogue10mWeaponType>()->GetNameStringByValue(int64(Character->GetEquippedWeaponType())),HasSword);
		Check(Inventory->GetGold()==13 && Inventory->GetCrystals()==0 && Saves->GetLobbyAvailablePoints()==0,TEXT("existing_interruption_handler_restores_gold13_crystals0_lobby0_entry_baseline"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Restored(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)));
		Check(Restored.IsValid() && Restored->ProfileId==ProfileId && Restored->RunId!=InterruptedRunId && Restored->LastSettledRunId==InterruptedRunId
			&& !Restored->bEnteredRun && !Restored->bRunCompleted && !Restored->bAbandoned && !Restored->EntryCurrencies.bCaptured
			&& Restored->Inventory.Gold==13 && Restored->Growth.Level==1,TEXT("actual_restored_disk_has_new_run_and_single_consumed_interruption_receipt"));
		for(FName Node:Before->DiscoveredNodes) { Check(Saves->HasDiscoveredNode(Node),TEXT("interrupted_Run_discovery_survives_real_second_process_recovery")); }
		Results+=FString::Printf(TEXT("restored_gold=13\nrestored_crystals=0\nrestored_lobby_available=0\nrestored_level=1\nrestored_experience=0\ninterrupted_run_id=%s\nrestored_run_id=%s\n"),*InterruptedRunId.ToString(),Restored.IsValid()?*Restored->RunId.ToString():TEXT("missing"));
		PC->ToggleSettings(); Menu=PC->GetQuitMenuWidget(); Photo(TEXT("01_restored_hub.png"));
		Later(.7,[this]{FinalQuit();});
	}
	void Unchanged()
	{
		Check(!IsEngineExitRequested(),TEXT("cancel_does_not_request_engine_exit"));
		if(Mode==TEXT("Menu")) { Check(Profiles->GetProfileCount()==0,TEXT("menu_open_cancel_creates_no_profile")); return; }
		TArray<uint8> After;
		Check(Saves->GetAttachedProfileId()==ProfileId && Saves->GetAttachedRunId()==RunId,TEXT("open_cancel_preserves_profile_and_run_identity"));
		Check(Same(Growth,State->CaptureGrowth()) && Same(Items,Inventory->CaptureInventory()) && Same(Skills,Tree->Capture()),TEXT("open_cancel_preserves_runtime_growth_items_currency_and_skills"));
		Check(FFileHelper::LoadFileToArray(After,*SaveFile) && After==Baseline,TEXT("open_cancel_preserves_disk_bytes"));
	}
	void CancelTests()
	{
		Check(Menu.IsValid() && PC->IsQuitGameConfirmationOpen(),TEXT("confirmation_remains_open_until_action"));
		if(Failures) { Abort(); return; }
		if(Mode==TEXT("Run") || Mode==TEXT("Terminal"))
		{ Check(Run->IsRunActive() && !UGameplayStatics::IsGamePaused(World.Get()) && World->GetTimeSeconds()>WorldTime+.5f && Run->GetRemainingRunSeconds()<Remaining,TEXT("field_confirmation_preserves_running_timer_policy")); }
		if(bRendered) { Check(Button(TEXT("QuitCancelButton")) && Button(TEXT("QuitCancelButton"))->HasKeyboardFocus(),TEXT("safe_cancel_receives_default_keyboard_focus")); }
		Click(TEXT("QuitCancelButton")); Check(!PC->IsQuitGameConfirmationOpen(),TEXT("actual_cancel_button_closes_confirmation")); Unchanged();
		if(bRendered && FSlateApplication::IsInitialized())
		{
			for(const FKey Key:{EKeys::Escape,EKeys::BackSpace,EKeys::Gamepad_FaceButton_Right})
			{
				Check(PC->OpenQuitGameConfirmation(),TEXT("confirmation_reopened_for_Slate_cancel"));
				const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
				FSlateApplication::Get().ProcessKeyDownEvent(Event); FSlateApplication::Get().ProcessKeyUpEvent(Event);
				Check(!PC->IsQuitGameConfirmationOpen(),TEXT("Slate_cancel_key_closes_without_exit")); Unchanged();
			}
		}
		Check(PC->OpenQuitGameConfirmation(),TEXT("confirmation_reopened_for_panel_close"));
		PC->CloseAllBlockingPanels(); Check(!PC->IsQuitGameConfirmationOpen(),TEXT("closing_panels_cancels_confirmation_context")); Unchanged();
		if(Mode==TEXT("Menu"))
		{
			auto* Start=MainMenu.IsValid()?Cast<UButton>(MainMenu->GetWidgetFromName(TEXT("UI_StartGameButton"))):nullptr;
			Check(Start && Start->GetIsEnabled(),TEXT("authored_start_button_available_for_lobby_path"));
			if(Start) { Start->OnClicked.Broadcast(); }
			Check(PC->IsCharacterLobbyVisible(),TEXT("authored_start_button_opens_lobby"));
			Menu=PC->GetQuitMenuWidget(); Click(TEXT("QuitMenuButton"));
			Check(PC->IsQuitGameConfirmationOpen(),TEXT("lobby_launcher_opens_quit_confirmation"));
			Click(TEXT("QuitCancelButton")); Unchanged();
			Photo(TEXT("02_lobby_launcher.png")); Later(.7,[this]{FinalQuit();}); return;
		}
		PC->ToggleSettings(); Menu=PC->GetQuitMenuWidget();
		if(Mode==TEXT("Terminal")) { TerminalTest(); return; }
		Click(TEXT("QuitMenuButton"));
		if(Mode==TEXT("Failure"))
		{
			Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,true),TEXT("only_owned_QA_progression_file_write_locked"));
			Click(TEXT("QuitConfirmButton"));
			Check(!IsEngineExitRequested() && PC->IsQuitGameConfirmationOpen() && !Text(TEXT("QuitErrorText")).IsEmpty(),TEXT("failed_save_keeps_confirmation_open_and_displays_error"));
			Unchanged();
			Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false),TEXT("owned_QA_write_lock_released"));
			Photo(TEXT("02_save_failure.png"));
		}
		Later(.7,[this]{FinalQuit();});
	}
	void TerminalTest()
	{
		Click(TEXT("QuitMenuButton"));
		Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,true),TEXT("terminal_test_locks_only_owned_QA_save"));
		Run->FinishRun(false);
		Check(!PC->IsQuitGameConfirmationOpen(),TEXT("terminal_phase_change_cancels_stale_quit_confirmation"));
		Later(.6,[this]
		{
			Check(PC->IsRunResultVisible() && Completion->IsSettled() && !Completion->IsProgressionSaved(),TEXT("actual_result_reports_failed_settlement"));
			Menu=PC->GetQuitMenuWidget(); Click(TEXT("QuitMenuButton"));
			Check(!PC->ConfirmQuitGame() && !IsEngineExitRequested() && PC->IsQuitGameConfirmationOpen() && !Text(TEXT("QuitErrorText")).IsEmpty(),TEXT("unsaved_terminal_result_blocks_quit_and_explains_retry"));
			Photo(TEXT("02_terminal_pending.png"));
			Later(.7,[this]
			{
				PC->CancelQuitGameConfirmation();
				Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false),TEXT("terminal_QA_write_lock_released"));
				auto* Result=PC->GetRunResultWidget(); auto* Return=Result?Cast<UButton>(Result->GetWidgetFromName(TEXT("RunResultReturnToTown"))):nullptr;
				Check(Return && Return->GetIsEnabled(),TEXT("actual_result_save_retry_button_available"));
				OldWorld=World; if(Return) { Return->OnClicked.Broadcast(); }
				Check(Completion->IsTravelStarted(),TEXT("actual_retry_saves_settlement_and_starts_hub_return"));
				Later(.2,[this]{WaitForHub();});
			});
		});
	}
	void WaitForHub()
	{
		for(const FWorldContext& Context:GEngine->GetWorldContexts())
		{
			UWorld* Candidate=Context.World();
			if(!Candidate || Candidate==OldWorld.Get() || Context.WorldType!=EWorldType::Game || !Candidate->HasBegunPlay()) { continue; }
			auto* Controller=Cast<ARogue10mPlayerController>(Candidate->GetFirstPlayerController()); if(!Controller) { continue; }
			World=Candidate; PC=Controller; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
			Check(Character.IsValid() && Saves->IsSaveReady() && !PC->IsRunResultVisible() && Run.IsValid() && Run->GetRunPhase()==ERogue10mRunPhase::WaitingToStart,TEXT("saved_terminal_retry_returns_actual_fresh_hub"));
			if(Failures) { Abort(); return; }
			PC->ToggleSettings(); Menu=PC->GetQuitMenuWidget(); Photo(TEXT("03_returned_hub.png")); Later(.7,[this]{FinalQuit();}); return;
		}
		Later(.2,[this]{WaitForHub();});
	}
	void WriteResults(bool Requested)
	{
		const FString Body=Results+FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nquit_requested=%d\nphysical_input=not_performed\nfull_run=not_performed\ninput=actual_UMG_delegates_and_optional_programmatic_Slate_keys\n"),Checks,Failures,*Mode,bRendered,Requested);
		if(!FFileHelper::SaveStringToFile(Body,*(Output/TEXT("results.txt")))) { ++Failures; }
	}
	void FinalQuit()
	{
		if(Failures) { Abort(); return; }
		if(Mode!=TEXT("Menu")) { Inventory->AddGold(7); ExpectedGold=Inventory->GetGold(); }
		if(!PC->IsQuitGameConfirmationOpen()) { Check(PC->OpenQuitGameConfirmation(),TEXT("final_actual_quit_confirmation_opened")); }
		if(Failures) { Abort(); return; }
		const FString Receipt=Output/TEXT("exit_callback.txt");
		FCoreDelegates::OnPreExit.AddLambda([Receipt]
		{ FFileHelper::SaveStringToFile(TEXT("production_quit_reached_engine_preexit=1\n"),*Receipt); });
		WriteResults(false); // Preserve every completed check before the production quit call.
		Click(TEXT("QuitConfirmButton"));
		Check(IsEngineExitRequested(),TEXT("actual_confirm_button_requests_production_engine_exit"));
		if(Mode!=TEXT("Menu"))
		{
			TStrongObjectPtr<URogue10mProgressionSaveGame> Disk(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)));
			Check(Disk.IsValid() && Disk->Inventory.Gold==ExpectedGold,TEXT("quit_explicitly_flushes_new_runtime_currency_before_exit"));
			if(Mode==TEXT("Run"))
			{
				Check(Disk.IsValid() && Disk->bEnteredRun && !Disk->bRunCompleted && Disk->EntryCurrencies.IsValid(),TEXT("quit_keeps_existing_interrupted_run_marker_for_next_hub_policy"));
				TArray<uint8> InterruptedBytes;
				Check(FFileHelper::LoadFileToArray(InterruptedBytes,*SaveFile) && FFileHelper::SaveArrayToFile(InterruptedBytes,*(Output/TEXT("interrupted-before-exit.sav"))),TEXT("actual_post_quit_write_retained_for_second_process_restore_comparison"));
			}
		}
		WriteResults(IsEngineExitRequested()); bFinished=true;
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=QUIT_MENU_%s checks=%d failures=%d mode=%s output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Mode,*Output);
		if(Failures || !IsEngineExitRequested()) { FPlatformMisc::RequestExitWithStatus(false,1); }
	}
	void Abort()
	{
		if(bFinished) { return; } bFinished=true;
		if(!SaveFile.IsEmpty()) { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false); }
		WriteResults(false); UE_LOG(LogRogue10m,Error,TEXT("RESULT=QUIT_MENU_FAILED checks=%d failures=%d output=%s"),Checks,Failures,*Output);
		FPlatformMisc::RequestExitWithStatus(false,1);
	}
	TWeakObjectPtr<UWorld> World,OldWorld;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mGameState> Run;
	TWeakObjectPtr<URogue10mQuitMenuWidget> Menu;
	TWeakObjectPtr<URogue10mMainMenuWidget> MainMenu;
	TWeakObjectPtr<URogue10mRunCompletionComponent> Completion;
	URogue10mCharacterProfileSubsystem* Profiles=nullptr;
	URogue10mProgressionSubsystem* Saves=nullptr;
	ARogue10mPlayerState* State=nullptr;
	URogue10mInventoryComponent* Inventory=nullptr;
	URogue10mSkillTreeProgressionComponent* Tree=nullptr;
	FRogue10mGrowthSnapshot Growth;
	FRogue10mInventorySnapshot Items;
	FRogue10mSkillTreeSnapshot Skills;
	FGuid ProfileId,RunId;
	TArray<uint8> Baseline;
	FString Label,Mode,Output,SaveSlot,SaveFile,Results;
	TFunction<void()> Next;
	double Deadline=0,NextAt=0;
	float Remaining=0,WorldTime=0;
	int32 Checks=0,Failures=0,ExpectedGold=0;
	bool bRendered=false,bFinished=false;
};
static TSharedPtr<FRun> Active;
static void RunCommand()
{
	if(!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for(const FWorldContext& Context:GEngine->GetWorldContexts())
	{ if(UWorld* World=Context.World(); World && Context.WorldType==EWorldType::Game) { Active=MakeShared<FRun>(); Active->Start(*World); return; } }
	UE_LOG(LogRogue10m,Error,TEXT("RESULT=QUIT_MENU_FAILED reason=no_standalone_game_world")); FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestQuitMenu"),TEXT("Exact fresh Quit QA namespaces only: actual modal cancel, save failure, terminal guard, and production quit."),FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
