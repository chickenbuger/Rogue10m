// Actual UMG menu delegates in isolated profiles. No physical mouse/keyboard or full-run claim.
#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAbandonMenuWidget.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunResultWidget.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mMenuAbandonQA
{
template<typename T> static bool Same(const T& A,const T& B)
{ return T::StaticStruct()->CompareScriptStruct(&A,&B,0); }

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World=&InWorld;
		FString ProfileSlot,Prefix,KeySlot;
		const TCHAR* Args=FCommandLine::Get();
		FParse::Value(Args,TEXT("RogueMenuAbandonLabel="),Label);
		FParse::Value(Args,TEXT("RogueMenuAbandonMode="),Mode);
		FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot);
		FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);
		FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
		bool bLabel=!Label.IsEmpty() && Label.Len()<=24;
		for(TCHAR Ch:Label) { bLabel &= FChar::IsAlnum(Ch) || Ch==TCHAR('_'); }
		if(!bLabel || ProfileSlot!=TEXT("Rogue10m_Town_QA_Menu_")+Label
			|| Prefix!=TEXT("Rogue10m_Progression_QA_Menu_")+Label
			|| KeySlot!=TEXT("Rogue10m_Fighter_QA_Menu_")+Label
			|| (Mode!=TEXT("Normal") && Mode!=TEXT("SaveFailure")) || InWorld.GetNetMode()!=NM_Standalone)
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=MENU_ABANDON_FAILED reason=exact_fresh_QA_overrides_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("Feature/doc/evidence/menu-abandon-20261029"),Label);
		Images=FPaths::Combine(FPaths::ProjectDir(),TEXT("DevLog/20261029_MenuAbandon_")+Label);
		if(IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=MENU_ABANDON_FAILED reason=fresh_evidence_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		IFileManager::Get().MakeDirectory(*Output,true);
		bRendered=!FParse::Param(Args,TEXT("nullrhi"));
		if(bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
		Deadline=FPlatformTime::Seconds()+210.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if(Self->bFinished) { return false; }
			if(FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
			if(Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
			return !Self->bFinished;
		}),0.02f);
		Later(3.0,[this]{Prepare();});
	}
	bool Finished() const { return bFinished; }
private:
	void Check(bool bOK,const TCHAR* Why)
	{
		++Checks; Results+=FString::Printf(TEXT("%s %s\n"),bOK?TEXT("PASS"):TEXT("FAIL"),Why);
		if(!bOK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("MENU_ABANDON FAIL %s"),Why); }
	}
	void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
	void Photo(const TCHAR* Name)
	{
		if(!bRendered) { return; }
		const FString File=FPaths::Combine(Images,Name);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float)
		{ FScreenshotRequest::RequestScreenshot(File,true,false); return false; }),0.2f);
	}
	void GetPawn()
	{
		Character=PC.IsValid()?Cast<ARogue10mCharacter>(PC->GetPawn()):nullptr;
		State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
		Inventory=Character.IsValid()?Character->GetInventoryComponent():nullptr;
		Combat=Character.IsValid()?Character->GetCombatComponent():nullptr;
		Tree=State?State->GetSkillTreeProgressionComponent():nullptr;
		Town=State?State->GetTownProgressionComponent():nullptr;
	}
	UButton* Button(const TCHAR* Name) const
	{ return Menu.IsValid()?Cast<UButton>(Menu->GetWidgetFromName(FName(Name))):nullptr; }
	void Click(const TCHAR* Name)
	{
		auto* Target=Button(Name);
		Check(Target && Target->GetIsEnabled(),TEXT("requested_actual_menu_button_is_enabled"));
		if(Target && Target->GetIsEnabled()) { Target->OnClicked.Broadcast(); }
	}
	FString TextOf(const TCHAR* Name) const
	{
		const auto* Text=Menu.IsValid()?Cast<UTextBlock>(Menu->GetWidgetFromName(FName(Name))):nullptr;
		return Text?Text->GetText().ToString():TEXT("<missing-widget>");
	}
	void CheckUnchanged()
	{
		Check(RunState.IsValid() && RunState->IsRunActive() && !Completion->IsSettled(),TEXT("cancel_close_and_rejected_commands_leave_run_active"));
		Check(Saves->GetAttachedRunId()==RunId && Saves->GetAttachedProfileId()==ProfileId,TEXT("cancel_close_preserve_run_and_profile_identity"));
		Check(Same(GrowthBefore,State->CaptureGrowth()) && Same(InventoryBefore,Inventory->CaptureInventory()) && Same(TreeBefore,Tree->Capture()),TEXT("cancel_close_do_not_mutate_growth_items_mastery_or_currency"));
		Check(Town->GetAvailableTraitPoints()==98,TEXT("cancel_close_preserve_lobby_spending_and_balance"));
	}
	void Prepare()
	{
		auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
		Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
		Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
		PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
		RunState=World.IsValid()?World->GetGameState<ARogue10mGameState>():nullptr;
		auto* GameMode=World.IsValid()?World->GetAuthGameMode<ARogue10mGameMode>():nullptr;
		Completion=GameMode?GameMode->GetRunCompletionComponent():nullptr;
		Check(Profiles && Saves && PC.IsValid() && RunState.IsValid() && Completion.IsValid(),TEXT("real_menu_game_subsystems_available"));
		if(Failures) { Finish(); return; }
		Check(Profiles->GetProfileCount()==0,TEXT("fresh_isolated_profile_slot"));
		FRogue10mCharacterProfile Profile;
		Check(!Failures && Profiles->CreateProfile(TEXT("MenuQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("new_QA_profile_selected"));
		if(Failures) { Finish(); return; }
		ProfileId=Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
		Check(Character.IsValid() && State && Inventory && Combat && Tree && Town && Saves->IsSaveReady(),TEXT("normal_character_attached_before_menu_test"));
		if(Failures) { Finish(); return; }
		PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget();
		Check(PC->IsSettingsVisible() && Menu.IsValid(),TEXT("existing_settings_path_creates_native_abandon_action"));
		if(Failures) { Finish(); return; }
		const auto* HubButton=Button(TEXT("AbandonMenuButton"));
		Check(HubButton && !HubButton->GetIsEnabled(),TEXT("hub_without_active_run_disables_abandon_action"));
		Check(!PC->OpenAbandonMenuConfirmation() && !Menu->IsConfirmationOpen(),TEXT("hub_cannot_open_abandon_confirmation"));
		PC->ToggleSettings();
		const FString Slot=Saves->GetSaveSlotForProfile(ProfileId);
		SaveFile=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Slot+TEXT(".sav"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Seed(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Seed.IsValid(),TEXT("isolated_saved_baseline_readable"));
		if(Failures) { Finish(); return; }
		Saves->DetachCharacter(*Character,false);
		Seed->LobbyEarnedPoints=100; Seed->Inventory.Gold=70; Seed->Inventory.Crystals=5;
		auto& Paid=Seed->Town.TraitInvestments.AddDefaulted_GetRef();
		Paid.Trait=ERogue10mTownTrait::Vitality; Paid.Rank=2; Paid.SpentPoints=2; Paid.Bonus=20.f;
		Seed->Town.VitalityLevel=2; Seed->Town.TraitMaxHealthBonus=20.f;
		Check(UGameplayStatics::SaveGameToSlot(Seed.Get(),Slot,0) && Saves->AttachCharacter(*Character),TEXT("post_payment_QA_currency_and_existing_lobby_upgrade_restored"));
		Check(Saves->SaveRunEntryCurrencyBaseline(),TEXT("actual_entry_checkpoint_saved_after_town_costs"));
		for(const auto& Row:Tree->Capture().Investments) { InitialNodes.Add(Row.NodeId); }
		State->AddExperience(500);
		for(int32 Index=0;Index<30;++Index) { Tree->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::Brawler); }
		for(const auto& Row:Tree->Capture().Investments) { Discovered.Add(Row.NodeId); }
		auto* Sword=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
		int32 ContainerIndex=INDEX_NONE; FGuid LootId;
		Check(Sword && Inventory->TryAddGridItem(Sword,1,ContainerIndex,LootId),TEXT("actual_run_loot_sword_added_for_reset_test"));
		Inventory->AddGold(77);
		auto Live=Inventory->CaptureInventory(); Live.Crystals=9;
		Check(Inventory->RestoreInventory(Live) && Saves->SaveNow(),TEXT("run_growth_loot_and_currency_snapshot_saved"));
		RunId=Saves->GetAttachedRunId();
		RunState->StartRunWithDuration(1200.f);
		Later(0.7,[this]{OpenSettings();});
	}
	void OpenSettings()
	{
		Primary=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		Check(Primary.IsValid(),TEXT("normal_primary_skill_exists_for_held_input_boundary"));
		if(Failures) { Finish(); return; }
		Combat->HandleBoundSkillPressed(Primary.Get());
		UsesBeforeMenu=State->GetSkillUseCount(Primary->GetFName());
		PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget();
		Check(PC->IsSettingsVisible() && Menu.IsValid() && !Menu->IsConfirmationOpen(),TEXT("settings_opens_without_implicitly_confirming_abandonment"));
		const auto* Action=Button(TEXT("AbandonMenuButton"));
		Check(Action && Action->GetIsEnabled(),TEXT("active_owned_checkpoint_enables_abandon_button"));
		GrowthBefore=State->CaptureGrowth(); InventoryBefore=Inventory->CaptureInventory(); TreeBefore=Tree->Capture();
		WorldSeconds=World->GetTimeSeconds(); RemainingSeconds=RunState->GetRemainingRunSeconds();
		Photo(TEXT("01_settings_menu.png"));
		Later(1.3,[this]{OpenConfirmation();});
	}
	void OpenConfirmation()
	{
		Check(!UGameplayStatics::IsGamePaused(World.Get()) && World->GetTimeSeconds()>WorldSeconds+0.7f && RunState->GetRemainingRunSeconds()<RemainingSeconds,TEXT("field_settings_leave_world_and_run_timer_running"));
		Check(State->GetSkillUseCount(Primary->GetFName())==UsesBeforeMenu,TEXT("opening_menu_does_not_execute_a_pending_bound_attack"));
		Click(TEXT("AbandonMenuButton"));
		Check(Menu->IsConfirmationOpen() && RunState->IsRunActive(),TEXT("button_opens_confirmation_without_ending_run"));
		const FString Policy=TextOf(TEXT("AbandonPolicyText"));
		Check(Policy.Contains(TEXT("초기화")) && Policy.Contains(TEXT("공개")) && Policy.Contains(TEXT("유지")) && Policy.Contains(TEXT("환불되지")),TEXT("actual_confirmation_explains_reset_discovery_and_no_town_refund"));
		WorldSeconds=World->GetTimeSeconds(); RemainingSeconds=RunState->GetRemainingRunSeconds();
		Photo(TEXT("02_confirmation_policy.png"));
		Later(1.3,[this]{CancelAndClose();});
	}
	void CancelAndClose()
	{
		Check(!UGameplayStatics::IsGamePaused(World.Get()) && World->GetTimeSeconds()>WorldSeconds+0.7f && RunState->GetRemainingRunSeconds()<RemainingSeconds,TEXT("field_confirmation_leaves_world_and_run_timer_running"));
		Click(TEXT("AbandonCancelButton"));
		Check(!Menu->IsConfirmationOpen() && PC->IsSettingsVisible(),TEXT("cancel_returns_to_existing_settings"));
		CheckUnchanged();
		Click(TEXT("AbandonMenuButton")); Click(TEXT("AbandonCloseButton"));
		Check(!Menu->IsConfirmationOpen() && PC->IsSettingsVisible(),TEXT("close_button_cancels_without_closing_settings"));
		CheckUnchanged();
		if(bRendered && FSlateApplication::IsInitialized())
		{
			Click(TEXT("AbandonMenuButton"));
			const FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),0,false,0,0);
			FSlateApplication::Get().ProcessKeyDownEvent(Escape);
			FSlateApplication::Get().ProcessKeyUpEvent(Escape);
			Check(!Menu->IsConfirmationOpen() && PC->IsSettingsVisible(),TEXT("Slate_escape_cancels_confirmation_and_keeps_settings"));
			CheckUnchanged();
		}
		Click(TEXT("AbandonMenuButton")); PC->ToggleSettings();
		Check(!PC->IsSettingsVisible() && !Menu->IsConfirmationOpen() && !PC->IsAnyBlockingWindowVisible(),TEXT("settings_toggle_closes_modal_and_returns_game_input"));
		Combat->HandleBoundSkillReleased(Primary.Get());
		Later(0.3,[this]{StaleProfile();});
	}
	void StaleProfile()
	{
		Check(State->GetSkillUseCount(Primary->GetFName())==UsesBeforeMenu,TEXT("releasing_cancelled_bound_input_after_close_does_not_attack"));
		CheckUnchanged();
		PC->ToggleSettings(); Click(TEXT("AbandonMenuButton"));
		FRogue10mCharacterProfile Other;
		Check(Profiles->CreateProfile(TEXT("OtherMenuQa"),FRogue10mCharacterAppearance(),Other) && Profiles->SelectProfile(Other.ProfileId),TEXT("another_QA_profile_selected_for_stale_confirmation"));
		Click(TEXT("AbandonConfirmButton"));
		Check(RunState->IsRunActive() && !Completion->IsSettled() && Inventory->GetGold()==147 && Inventory->GetCrystals()==9,TEXT("stale_profile_confirmation_cannot_abandon_or_restore_currency"));
		Check(Button(TEXT("AbandonConfirmButton")) && !Button(TEXT("AbandonConfirmButton"))->GetIsEnabled() && !TextOf(TEXT("AbandonErrorText")).IsEmpty(),TEXT("stale_confirmation_is_disabled_and_requires_reopening"));
		Check(Profiles->SelectProfile(ProfileId),TEXT("original_profile_restored_after_rejected_confirmation"));
		PC->CancelAbandonMenuConfirmation();
		CheckUnchanged();
		Click(TEXT("AbandonMenuButton"));
		Check(Saves->SaveNow(),TEXT("run_snapshot_saved_before_QA_run_identity_change"));
		const FString Slot=Saves->GetSaveSlotForProfile(ProfileId);
		TStrongObjectPtr<URogue10mProgressionSaveGame> Changed(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Changed.IsValid(),TEXT("stale_run_fixture_uses_current_QA_slot"));
		if(Failures || !Changed.IsValid()) { Finish(); return; }
		Saves->DetachCharacter(*Character,false);
		Changed->RunId=FGuid::NewGuid(); Changed->EntryCurrencies.RunId=Changed->RunId;
		Check(UGameplayStatics::SaveGameToSlot(Changed.Get(),Slot,0) && Saves->AttachCharacter(*Character),TEXT("new_run_identity_restored_into_same_QA_pawn"));
		RunId=Changed->RunId;
		Click(TEXT("AbandonConfirmButton"));
		Check(RunState->IsRunActive() && !Completion->IsSettled() && Saves->GetAttachedRunId()==RunId,TEXT("stale_run_identity_confirmation_cannot_end_replacement_run"));
		Check(Button(TEXT("AbandonConfirmButton")) && !Button(TEXT("AbandonConfirmButton"))->GetIsEnabled() && !TextOf(TEXT("AbandonErrorText")).IsEmpty(),TEXT("stale_confirmation_is_disabled_and_requires_reopening"));
		PC->CancelAbandonMenuConfirmation(); CheckUnchanged();
		Photo(TEXT("03_cancelled_settings.png"));
		Later(0.5,[this]{ConfirmOnce();});
	}
	void ConfirmOnce()
	{
		if(Failures) { Finish(); return; }
		if(Mode==TEXT("SaveFailure")) { Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,true),TEXT("only_current_QA_save_is_locked_for_write_failure")); }
		PC->ConsoleCommand(TEXT("AbandonRun"),true);
		Check(RunState->IsRunActive(),TEXT("one_console_command_remains_an_unconfirmed_console_request"));
		Click(TEXT("AbandonMenuButton"));
		auto* Confirm=Button(TEXT("AbandonConfirmButton"));
		Click(TEXT("AbandonConfirmButton"));
		if(Confirm) { Confirm->OnClicked.Broadcast(); Confirm->OnClicked.Broadcast(); }
		Check(RunState->GetRunPhase()==ERogue10mRunPhase::Abandoned && !Menu->IsConfirmationOpen() && !PC->IsSettingsVisible(),TEXT("single_UI_confirmation_ends_run_and_consumes_dialog_without_console_delay"));
		Check(!PC->ConfirmAbandonMenu() && !PC->RequestAbandonCurrentRun(),TEXT("reentrant_or_repeated_confirmation_cannot_end_run_twice"));
		Later(0.5,[this]{ResultAndRetry();});
	}
	void ResultAndRetry()
	{
		Check(PC->IsRunResultVisible() && Completion->IsSettled() && Completion->IsAbandoned(),TEXT("accepted_menu_confirmation_reaches_actual_abandon_result"));
		Check(Inventory->GetGold()==70 && Inventory->GetCrystals()==5 && Saves->GetLobbyAvailablePoints()==98,TEXT("result_currency_rolls_back_after_town_cost_without_new_reward"));
		auto Summary=Completion->GetEconomySummary();
		Check(Summary.bAvailable && Summary.bAbandoned && Summary.LobbyAward==0 && Summary.bSaved==(Mode==TEXT("Normal")),TEXT("result_economy_reports_pending_or_saved_zero_reward"));
		Result=PC->GetRunResultWidget();
		Check(Result.IsValid(),TEXT("actual_result_widget_exists_after_confirmation"));
		if(Failures) { Finish(); return; }
		const auto* Status=Cast<UTextBlock>(Result->GetWidgetFromName(TEXT("RunResultSaveStatus")));
		Check(Status && Status->GetText().ToString().Contains(Mode==TEXT("Normal")?TEXT("저장 완료"):TEXT("저장 대기")),TEXT("result_status_explains_durable_or_pending_rollback"));
		Photo(TEXT("04_abandon_result.png"));
		Later(0.5,[this]{RetryAndReturn();});
	}
	void RetryAndReturn()
	{
		auto* Return=Result.IsValid()?Cast<UButton>(Result->GetWidgetFromName(TEXT("RunResultReturnToTown"))):nullptr;
		Check(Return && Return->GetIsEnabled(),TEXT("actual_result_return_or_retry_button_available"));
		if(!Return) { Finish(); return; }
		if(Mode==TEXT("SaveFailure"))
		{
			for(int32 Index=0;Index<3;++Index)
			{
				Return->OnClicked.Broadcast();
				Check(PC->IsRunResultVisible() && !Completion->IsTravelStarted() && !Completion->IsProgressionSaved(),TEXT("failed_button_retries_keep_result_open_and_block_travel"));
				Check(Inventory->GetGold()==70 && Inventory->GetCrystals()==5 && Saves->GetLobbyAvailablePoints()==98,TEXT("failed_button_retries_never_restore_or_award_twice"));
			}
			Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false),TEXT("QA_write_failure_released_for_actual_button_retry"));
		}
		Photo(TEXT("05_retry_boundary.png"));
		Later(0.5,[this]
		{
			OldWorld=World;
			auto* ReturnButton=Result.IsValid()?Cast<UButton>(Result->GetWidgetFromName(TEXT("RunResultReturnToTown"))):nullptr;
			if(ReturnButton) { ReturnButton->OnClicked.Broadcast(); }
			Check(Completion->IsTravelStarted(),TEXT("actual_result_button_starts_saved_return_travel"));
			Later(0.2,[this]{WaitForTown();});
		});
	}
	void WaitForTown()
	{
		for(const FWorldContext& Context:GEngine->GetWorldContexts())
		{
			UWorld* Candidate=Context.World();
			if(!Candidate || Candidate==OldWorld.Get() || Context.WorldType!=EWorldType::Game
				|| !Candidate->HasBegunPlay() || !Candidate->GetFirstPlayerController()) { continue; }
			World=Candidate; PC=Cast<ARogue10mPlayerController>(Candidate->GetFirstPlayerController());
			RunState=Candidate->GetGameState<ARogue10mGameState>();
			if(!PC.IsValid() || !RunState.IsValid()) { continue; }
			PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
			Check(Character.IsValid() && State && Inventory && Tree && Town && Saves->IsSaveReady(),TEXT("menu_abandonment_restores_actual_new_hub_character"));
			if(Failures) { Finish(); return; }
			Check(UGameplayStatics::GetCurrentLevelName(Candidate,true)==TEXT("StartHub") && RunState->GetRunPhase()==ERogue10mRunPhase::WaitingToStart
				&& !PC->IsRunResultVisible() && !PC->IsSettingsVisible(),TEXT("return_completes_in_hub_with_no_stale_result_or_settings"));
			Check(Inventory->GetGold()==70 && Inventory->GetCrystals()==5 && Saves->GetLobbyAvailablePoints()==98,TEXT("hub_preserves_post_payment_baseline_and_existing_lobby_costs"));
			Check(State->GetPlayerLevel()==1 && State->GetCurrentExperience()==0 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)==0,TEXT("abandoned_run_growth_and_mastery_reset_on_hub_return"));
			bool bSword=false;
			for(const auto& Container:Inventory->GetInventoryContainers()) { for(const auto& Item:Container.Entries) { bSword |= Item.ItemData && Item.ItemData->WeaponType==ERogue10mWeaponType::GreatSword; } }
			for(const auto& Item:Inventory->GetItemSlots()) { bSword |= Item.bOccupied && Item.WeaponType==ERogue10mWeaponType::GreatSword; }
			Check(!bSword && Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle,TEXT("run_loot_is_discarded_and_knuckle_start_restored"));
			Check(Town->GetTraitLevel(ERogue10mTownTrait::Vitality)==2,TEXT("permanent_lobby_upgrade_survives_menu_abandonment"));
			for(FName Node:Discovered)
			{
				Check(Saves->HasDiscoveredNode(Node),TEXT("discovery_information_survives_menu_abandonment"));
				if(!InitialNodes.Contains(Node)) { Check(!Tree->IsNodeUnlocked(Node),TEXT("discovery_does_not_regrant_abandoned_run_learning")); }
			}
			Check(Saves->SaveNow(),TEXT("new_hub_snapshot_saves_without_reapplying_rollback"));
			TStrongObjectPtr<URogue10mProgressionSaveGame> Saved(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->GetSaveSlotForProfile(ProfileId),0)));
			Check(Saved.IsValid() && Saved->RunId!=RunId && Saved->LastSettledRunId==RunId && !Saved->EntryCurrencies.bCaptured
				&& Saved->Inventory.Gold==70 && Saved->Inventory.Crystals==5 && Saved->LobbyEarnedPoints==100,TEXT("disk_roundtrip_contains_new_run_and_single_abandon_receipt"));
			PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget();
			Check(Menu.IsValid() && !Menu->IsConfirmationOpen() && Button(TEXT("AbandonMenuButton")) && !Button(TEXT("AbandonMenuButton"))->GetIsEnabled(),TEXT("hub_settings_do_not_reuse_previous_run_confirmation"));
			PC->ToggleSettings(); Photo(TEXT("06_returned_hub.png"));
			Later(0.6,[this]{Finish();}); return;
		}
		Later(0.2,[this]{WaitForTown();});
	}
	void Finish()
	{
		if(bFinished) { return; } bFinished=true;
		if(!SaveFile.IsEmpty()) { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false); }
		Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nmanual_play=not_performed\nNOTE=Actual UMG delegates, programmatic Slate key events when rendered, QA-owned post-payment fixture. Not physical input or a normal full run.\n"),Checks,Failures,*Mode,bRendered);
		if(!FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=MENU_ABANDON_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World,OldWorld;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	TWeakObjectPtr<URogue10mAbandonMenuWidget> Menu;
	TWeakObjectPtr<URogue10mRunResultWidget> Result;
	TWeakObjectPtr<URogue10mRunCompletionComponent> Completion;
	TWeakObjectPtr<URogue10mAttackSkillData> Primary;
	URogue10mCharacterProfileSubsystem* Profiles=nullptr;
	URogue10mProgressionSubsystem* Saves=nullptr;
	ARogue10mPlayerState* State=nullptr;
	URogue10mInventoryComponent* Inventory=nullptr;
	URogue10mCombatComponent* Combat=nullptr;
	URogue10mSkillTreeProgressionComponent* Tree=nullptr;
	URogue10mTownProgressionComponent* Town=nullptr;
	FRogue10mGrowthSnapshot GrowthBefore;
	FRogue10mInventorySnapshot InventoryBefore;
	FRogue10mSkillTreeSnapshot TreeBefore;
	FGuid ProfileId,RunId;
	TArray<FName> InitialNodes,Discovered;
	FString Label,Mode,Output,Images,SaveFile,Results;
	TFunction<void()> Next;
	double Deadline=0,NextAt=0;
	float WorldSeconds=0.f,RemainingSeconds=0.f;
	int32 Checks=0,Failures=0,UsesBeforeMenu=0;
	bool bFinished=false,bRendered=false;
};
static TSharedPtr<FRun> Active;
static void RunCommand()
{
	if(!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for(const FWorldContext& Context:GEngine->GetWorldContexts())
	{
		if(UWorld* World=Context.World(); World && World->WorldType==EWorldType::Game)
		{ Active=MakeShared<FRun>(); Active->Start(*World); return; }
	}
	UE_LOG(LogRogue10m,Error,TEXT("RESULT=MENU_ABANDON_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestMenuAbandon"),TEXT("Isolated settings confirmation, cancel/close/context guards, single abandonment, failed save retry and actual hub return."),FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
