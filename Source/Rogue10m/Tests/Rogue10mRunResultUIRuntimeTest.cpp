// Isolated QA: actual UMG text and settlement sources, not a manual boss clear.
#if WITH_EDITOR
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mBasicMonster.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunResultWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mRunResultUIQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World=&InWorld;
		FString ProfileSlot,Prefix,KeySlot;
		const TCHAR* Args=FCommandLine::Get();
		FParse::Value(Args,TEXT("RogueResultUILabel="),Label);
		FParse::Value(Args,TEXT("RogueResultUIMode="),Mode);
		FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot);
		FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);
		FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
		bool bLabel=!Label.IsEmpty() && Label.Len()<=24;
		for(TCHAR Ch:Label) { bLabel &= FChar::IsAlnum(Ch) || Ch==TCHAR('_'); }
		if(!bLabel || ProfileSlot!=TEXT("Rogue10m_Town_QA_ResultUI_")+Label
			|| Prefix!=TEXT("Rogue10m_Progression_QA_ResultUI_")+Label
			|| KeySlot!=TEXT("Rogue10m_Fighter_QA_ResultUI_")+Label
			|| (Mode!=TEXT("Normal") && Mode!=TEXT("Failure") && Mode!=TEXT("Zero") && Mode!=TEXT("Cap") && Mode!=TEXT("AbandonGain") && Mode!=TEXT("AbandonSpend") && Mode!=TEXT("Interrupted"))
			|| InWorld.GetNetMode()!=NM_Standalone)
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_RESULT_UI_FAILED reason=exact_fresh_QA_overrides_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("Feature/doc/evidence/run-result-ui-20261029"),Label);
		Images=FPaths::Combine(FPaths::ProjectDir(),TEXT("DevLog/20261029_RunResultUI_")+Label);
		if(IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_RESULT_UI_FAILED reason=fresh_evidence_label_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		IFileManager::Get().MakeDirectory(*Output,true);
		bRendered=!FParse::Param(Args,TEXT("nullrhi"));
		if(bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
		Deadline=FPlatformTime::Seconds()+180.0;
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
	bool IsAbandon() const { return Mode==TEXT("AbandonGain") || Mode==TEXT("AbandonSpend"); }
	bool IsFailure() const { return Mode==TEXT("Failure") || Mode==TEXT("AbandonGain"); }
private:
	void Check(bool bOK,const TCHAR* Why)
	{
		++Checks; Results+=FString::Printf(TEXT("%s %s\n"),bOK?TEXT("PASS"):TEXT("FAIL"),Why);
		if(!bOK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("RUN_RESULT_UI FAIL %s"),Why); }
	}
	void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
	void Photo(const TCHAR* Name)
	{
		if(!bRendered) { return; }
		const FString File=FPaths::Combine(Images,Name);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float)
		{ FScreenshotRequest::RequestScreenshot(File,true,false); return false; }),0.2f);
	}
	FString TextOf(const TCHAR* Name) const
	{
		const auto* Text=Widget.IsValid()?Cast<UTextBlock>(Widget->GetWidgetFromName(FName(Name))):nullptr;
		return Text?Text->GetText().ToString():TEXT("<missing-widget>");
	}
	void VerifyUI(bool bSaved)
	{
		if(!Completion.IsValid() || !Widget.IsValid()) { Check(false,TEXT("result_components_exist_for_projection")); return; }
		Widget->RefreshResult();
		const auto Summary=Completion->GetEconomySummary();
		Check(Summary.bAvailable && Summary.bSaved==bSaved,TEXT("economy_projection_reports_actual_save_state"));
		Check(Summary.LobbyAward==ExpectedAward && Summary.LobbyAvailable==ExpectedBalance,TEXT("economy_projection_uses_actual_credited_delta_and_balance"));
		Check(Summary.RunGold==ExpectedGold && Summary.RunCrystals==ExpectedCrystals,TEXT("economy_projection_keeps_run_currencies_separate"));
		Check(Summary.bAbandoned==IsAbandon(),TEXT("economy_projection_distinguishes_abandonment_from_rewarded_endings"));
		const FString Award=IsAbandon()
			? FString(TEXT("포기 보상 없음  +0"))
			: FText::Format(FText::FromString(TEXT("이번 판 로비 포인트  +{0}")),FText::AsNumber(ExpectedAward)).ToString();
		const FString Balance=FText::Format(FText::FromString(TEXT("정산 후 로비 사용 가능  {0}")),FText::AsNumber(ExpectedBalance)).ToString();
		const FString RunCurrency=FText::Format(FText::FromString(IsAbandon()
			? TEXT("복원된 재화 잔액  골드 {0} · 크리스탈 {1}\n마을 준비 비용은 환불되지 않습니다.")
			: TEXT("런 재화 잔액  골드 {0} · 크리스탈 {1}\n다음 판에는 유지되지 않습니다.")),FText::AsNumber(ExpectedGold),FText::AsNumber(ExpectedCrystals)).ToString();
		Check(TextOf(TEXT("LobbyRewardText"))==Award,TEXT("actual_UMG_award_text_matches_credited_delta"));
		Check(TextOf(TEXT("LobbyBalanceText"))==Balance,TEXT("actual_UMG_lobby_balance_matches_available_amount"));
		Check(TextOf(TEXT("RunCurrencyText"))==RunCurrency,TEXT("actual_UMG_run_currency_has_reset_scope_label"));
		Check(TextOf(TEXT("RunResultSaveStatus")).Contains(bSaved?TEXT("저장 완료"):TEXT("저장 대기")),TEXT("actual_UMG_distinguishes_saved_and_pending"));
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
		Check(Profiles && Saves && PC.IsValid() && RunState.IsValid() && Completion.IsValid(),TEXT("real_subsystems_run_state_and_completion"));
		if(Failures) { Finish(); return; }
		Check(Profiles->GetProfileCount()==0,TEXT("fresh_QA_profile_slot"));
		FRogue10mCharacterProfile Profile;
		Check(!Failures && Profiles->CreateProfile(TEXT("ResultUiQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("new_QA_profile_selected"));
		if(Failures) { Finish(); return; }
		ProfileId=Profile.ProfileId;
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Character=Cast<ARogue10mCharacter>(PC->GetPawn());
		State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
		Inventory=Character.IsValid()?Character->GetInventoryComponent():nullptr;
		Check(Character.IsValid() && State && Inventory && Saves->IsSaveReady(),TEXT("QA_character_attached_and_save_ready"));
		if(Failures) { Finish(); return; }
		for(const auto& Row:State->GetSkillTreeProgressionComponent()->Capture().Investments) { InitialNodes.Add(Row.NodeId); }
		const FString Slot=Saves->GetSaveSlotForProfile(ProfileId);
		ReadOnlyFile=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Slot+TEXT(".sav"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Seed(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Seed.IsValid() && Seed->SaveVersion==URogue10mProgressionSaveGame::CurrentVersion,TEXT("new_QA_current_baseline_readable"));
		if(Failures) { Finish(); return; }
		Saves->DetachCharacter(*Character,false);
		InitialLobby=Mode==TEXT("Cap")?MAX_int32-5:100;
		Seed->LobbyEarnedPoints=InitialLobby;
		// Existing lobby upgrade costs remain in Town's spent ledger, independently of gross earnings.
		auto& PaidTrait=Seed->Town.TraitInvestments.AddDefaulted_GetRef();
		PaidTrait.Trait=ERogue10mTownTrait::Vitality; PaidTrait.Rank=2; PaidTrait.SpentPoints=2; PaidTrait.Bonus=20.f;
		Seed->Town.VitalityLevel=2; Seed->Town.TraitMaxHealthBonus=20.f;
		Seed->Inventory.Gold=100; Seed->Inventory.Crystals=10;
		Check(UGameplayStatics::SaveGameToSlot(Seed.Get(),Slot,0) && Saves->AttachCharacter(*Character),TEXT("isolated_lobby_and_run_currency_fixture_restored"));
		if(Failures) { Finish(); return; }
		Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,true),TEXT("entry_checkpoint_write_failure_enabled"));
		Check(!Saves->SaveRunEntryCurrencyBaseline(),TEXT("entry_checkpoint_write_failure_blocks_travel_gate"));
		int32 MissingGold=-1,MissingCrystals=-1,MissingLobby=-1;
		Check(!Saves->TryResolveAbandonCurrencyBalances(MissingGold,MissingCrystals,MissingLobby)
			&& MissingGold==-1 && MissingCrystals==-1 && MissingLobby==-1,TEXT("failed_checkpoint_does_not_leave_a_frozen_baseline"));
		Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,false),TEXT("entry_checkpoint_write_failure_released"));
		auto PaidInventory=Inventory->CaptureInventory(); PaidInventory.Gold=70; PaidInventory.Crystals=5;
		Check(Inventory->RestoreInventory(PaidInventory),TEXT("town_cost_commits_after_rejected_entry_attempt"));
		Check(Saves->SaveRunEntryCurrencyBaseline(),TEXT("post_payment_currency_baseline_saved_before_field_entry"));
		Check(Saves->SaveRunEntryCurrencyBaseline(),TEXT("unchanged_entry_capture_is_idempotent"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> EntryProof(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(EntryProof.IsValid() && EntryProof->EntryCurrencies.IsValid() && !EntryProof->bEnteredRun
			&& EntryProof->EntryCurrencies.Gold==70 && EntryProof->EntryCurrencies.Crystals==5
			&& EntryProof->EntryCurrencies.LobbyEarnedPoints==InitialLobby,TEXT("durable_entry_baseline_excludes_paid_town_costs"));
		if(Mode==TEXT("Zero")) { Saves->LobbyPointsPerDefeat=0; Saves->LobbyVictoryBonus=0; }
		else { Saves->LobbyPointsPerDefeat=1; Saves->LobbyVictoryBonus=10; }
		State->RecordMonsterDefeat(TEXT("QA.ResultUiEnemy"),2);
		Inventory->AddGold(77);
		const int32 LiveGold=Mode==TEXT("AbandonSpend")?40:147;
		const int32 LiveCrystals=Mode==TEXT("AbandonSpend")?2:9;
		ExpectedGold=LiveGold; ExpectedCrystals=LiveCrystals;
		auto RunInventory=Inventory->CaptureInventory(); RunInventory.Gold=LiveGold; RunInventory.Crystals=LiveCrystals;
		Check(Inventory->RestoreInventory(RunInventory),TEXT("isolated_run_currency_gain_fixture_applied"));
		if(IsAbandon() || Mode==TEXT("Interrupted"))
		{
			State->AddExperience(500);
			for(int32 Index=0;Index<30;++Index) { State->GetSkillTreeProgressionComponent()->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::Brawler); }
			for(const auto& Row:State->GetSkillTreeProgressionComponent()->Capture().Investments) { DiscoveredNodes.Add(Row.NodeId); }
			Check(DiscoveredNodes.Num()>InitialNodes.Num(),TEXT("abandon_fixture_learns_new_discoverable_techniques"));
			auto* Sword=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
			int32 ContainerIndex=INDEX_NONE; FGuid LootId;
			Check(Sword && Inventory->TryAddGridItem(Sword,1,ContainerIndex,LootId),TEXT("abandon_fixture_acquires_real_run_sword"));
		}
		Check(Saves->SaveNow(),TEXT("active_run_fixture_saved"));
		Check(!Completion->GetEconomySummary().bAvailable,TEXT("no_result_amount_before_a_terminal_receipt"));
		RunState->StartRunWithDuration(1200.f);
		int32 AbandonGold=-1,AbandonCrystals=-1,AbandonLobby=-1;
		Check(Saves->TryResolveAbandonCurrencyBalances(AbandonGold,AbandonCrystals,AbandonLobby)
			&& AbandonGold==70 && AbandonCrystals==5 && AbandonLobby==InitialLobby,TEXT("read_only_abandon_resolution_keeps_post_payment_baseline"));
		Check(Inventory->GetGold()==LiveGold && Inventory->GetCrystals()==LiveCrystals && Saves->GetLobbyAvailablePoints()==InitialLobby-2,TEXT("baseline_resolution_does_not_apply_unapproved_abandon_policy"));
		Check(!Saves->SaveRunEntryCurrencyBaseline(),TEXT("in_run_balance_cannot_replace_entry_checkpoint"));
		if(IsAbandon() || Mode==TEXT("Interrupted"))
		{
			// QA-only snapshot injection tests future in-run lobby currency gains/spends
			// without inventing a production grant or purchase route.
			TStrongObjectPtr<URogue10mProgressionSaveGame> InRunCurrency(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
			Check(InRunCurrency.IsValid(),TEXT("abandon_lobby_gain_or_spend_fixture_readable"));
			if(Failures) { Finish(); return; }
			Saves->DetachCharacter(*Character,false);
			const int32 LiveLobby=Mode==TEXT("AbandonSpend")?80:155;
			InRunCurrency->LobbyEarnedPoints=LiveLobby;
			Check(UGameplayStatics::SaveGameToSlot(InRunCurrency.Get(),Slot,0) && Saves->AttachCharacter(*Character),TEXT("running_QA_snapshot_injects_lobby_gain_or_spend"));
			Check(Saves->GetLobbyAvailablePoints()==LiveLobby-2 && Inventory->GetGold()==LiveGold && Inventory->GetCrystals()==LiveCrystals,TEXT("running_reattach_preserves_active_currency_variation"));
			int32 RollbackGold=-1,RollbackCrystals=-1,RollbackLobby=-1;
			Check(Saves->TryResolveAbandonCurrencyBalances(RollbackGold,RollbackCrystals,RollbackLobby)
				&& RollbackGold==70 && RollbackCrystals==5 && RollbackLobby==100,TEXT("lobby_variation_cannot_replace_post_payment_checkpoint"));
		}
		ExpectedAward=IsAbandon()?0:Mode==TEXT("Cap")?5:Mode==TEXT("Zero")?0:12;
		if(IsAbandon()) { ExpectedGold=70; ExpectedCrystals=5; }
		ExpectedBalance=static_cast<int32>(static_cast<int64>(InitialLobby)+ExpectedAward-2);
		if(IsFailure()) { Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,true),TEXT("bounded_QA_write_failure_enabled")); }
		if(Failures) { Finish(); return; }
		if(Mode==TEXT("Interrupted"))
		{
			Check(RunState->IsRunActive() && Saves->SaveNow(),TEXT("interrupted_active_run_is_durable_before_world_reload"));
			TStrongObjectPtr<URogue10mProgressionSaveGame> ActiveDisk(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->GetSaveSlotForProfile(ProfileId),0)));
			Check(ActiveDisk.IsValid() && !ActiveDisk->bRunCompleted && ActiveDisk->EntryCurrencies.IsValid() && ActiveDisk->bEnteredRun
				&& ActiveDisk->Inventory.Gold==147 && ActiveDisk->Inventory.Crystals==9 && ActiveDisk->LobbyEarnedPoints==155,
				TEXT("interrupted_disk_snapshot_has_active_growth_and_unsettled_currency_gain"));
			if(Failures) { Finish(); return; }
			InterruptedRunId=ActiveDisk->RunId;
			Photo(TEXT("01_interrupted_active_run.png"));
			Later(0.5,[this]
			{
				OldWorld=World;
				// Reload an actual world from a durable active snapshot; do not kill an editor or touch user saves.
				UGameplayStatics::OpenLevel(World.Get(),FName(TEXT("/Game/Rogue10m/Maps/StartHub")),true,TEXT(""));
				Later(0.2,[this]{WaitForTown();});
			});
			return;
		}
		if(IsAbandon())
		{
			LateTarget=World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),Character->GetActorLocation()+FVector(5000,0,0),FRotator::ZeroRotator);
			Check(LateTarget.IsValid() && LateTarget->TakeDamage(1.f,FDamageEvent(),PC.Get(),Character.Get())>0.f,TEXT("real_monster_damage_source_seeded_before_abandonment"));
		}
		if(Mode==TEXT("AbandonGain")) { PC->ConsoleCommand(TEXT("AbandonRun"),true); Check(RunState->IsRunActive(),TEXT("first_console_abandon_command_only_requests_confirmation")); PC->ConsoleCommand(TEXT("AbandonRun"),true); Check(RunState->GetRunPhase()==ERogue10mRunPhase::Abandoned,TEXT("confirmed_console_command_enters_abandoned_phase")); } else if(IsAbandon()) { Check(PC->RequestAbandonCurrentRun(),TEXT("owned_controller_enters_explicit_abandoned_phase")); }
		else { RunState->FinishRun(true); }
		Later(0.5,[this]{FirstResult();});
	}
	void FirstResult()
	{
		PC->ShowRunResult(); Widget=PC->GetRunResultWidget();
		Check(Widget.IsValid() && PC->IsRunResultVisible() && Completion->IsSettled(),TEXT("actual_terminal_result_widget_visible"));
		if(Failures) { Finish(); return; }
		VerifyUI(!IsFailure());
		if(IsAbandon() && LateTarget.IsValid())
		{
			const int32 GoldBefore=Inventory->GetGold(); const float ExperienceBefore=State->GetCurrentExperience();
			Check(RunState->IsRunTerminal() && Character->TakeDamage(50.f,FDamageEvent(),nullptr,LateTarget.Get())==0.f,TEXT("abandoned_terminal_state_rejects_late_player_damage"));
			Check(LateTarget->TakeDamage(50.f,FDamageEvent(),PC.Get(),Character.Get())==0.f,TEXT("abandoned_terminal_state_rejects_late_monster_damage"));
			Check(LateTarget->TakeDamage(50000.f,FDamageEvent(),PC.Get(),Character.Get())==0.f && !LateTarget->IsDead(),TEXT("late_lethal_hit_does_not_kill_after_abandonment"));
			Check(Inventory->GetGold()==GoldBefore && State->GetCurrentExperience()==ExperienceBefore,TEXT("late_monster_hit_cannot_award_gold_or_experience_after_abandonment"));
		}
		Photo(TEXT("01_initial_result.png"));
		Later(0.5,[this]{RepeatedRequests();});
	}
	void RepeatedRequests()
	{
		for(int32 Index=0;Index<4;++Index)
		{
			if(IsFailure()) { Check(!Completion->ReturnToTown() && !Completion->IsTravelStarted(),TEXT("failed_retry_cannot_travel_or_reaward")); }
			else { Check(IsAbandon()?Saves->SaveRunAbandonment():Saves->SaveRunSettlement(true),TEXT("duplicate_save_is_accepted_without_new_award")); }
			VerifyUI(!IsFailure());
		}
		Photo(TEXT("02_repeat_result.png"));
		Later(0.5,[this]{ConfirmSaved();});
	}
	void ConfirmSaved()
	{
		if(IsFailure())
		{
			Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,false),TEXT("QA_write_failure_released"));
			Check(IsAbandon()?Saves->SaveRunAbandonment():Saves->SaveRunSettlement(true),TEXT("same_receipt_saved_after_disk_recovery"));
		}
		PC->ShowRunResult(); Widget=PC->GetRunResultWidget(); VerifyUI(true);
		Check(Saves->GetLobbyAvailablePoints()==ExpectedBalance && Inventory->GetGold()==ExpectedGold && Inventory->GetCrystals()==ExpectedCrystals,TEXT("UI_refresh_does_not_mutate_either_currency_domain"));
		Photo(TEXT("03_saved_result.png"));
		Later(0.6,[this]{ProfileGuard();});
	}
	void ProfileGuard()
	{
		FRogue10mCharacterProfile Other;
		Check(Profiles->CreateProfile(TEXT("OtherUiQa"),FRogue10mCharacterAppearance(),Other) && Profiles->SelectProfile(Other.ProfileId),TEXT("different_QA_profile_selected_without_reusing_result"));
		OtherProfileId=Other.ProfileId;
		Check(!Completion->GetEconomySummary().bAvailable,TEXT("result_projection_rejects_profile_mismatch"));
		Widget->RefreshResult();
		Check(TextOf(TEXT("LobbyRewardText")).Contains(TEXT("—")),TEXT("profile_mismatch_displays_unavailable_instead_of_stale_amount"));
		Check(!TextOf(TEXT("RunResultSaveStatus")).Contains(TEXT("저장 완료")),TEXT("profile_mismatch_does_not_claim_saved_reward"));
		const auto* Return=Cast<UButton>(Widget->GetWidgetFromName(TEXT("RunResultReturnToTown")));
		Check(Return && !Return->GetIsEnabled(),TEXT("profile_mismatch_disables_return_action"));
		Check(Profiles->SelectProfile(ProfileId),TEXT("original_QA_profile_restored"));
		VerifyUI(true);
		Results+=TEXT("\nAWARD_TEXT=")+TextOf(TEXT("LobbyRewardText"))+TEXT("\nBALANCE_TEXT=")+TextOf(TEXT("LobbyBalanceText"))+TEXT("\nRUN_TEXT=")+TextOf(TEXT("RunCurrencyText"))+TEXT("\n");
		if(IsAbandon()) { Later(0.3,[this]{ReturnAfterAbandon();}); }
		else if(Mode==TEXT("Normal")) { V4ImportCase(); }
		else { Finish(); }
	}
	template<typename T> static bool SameSnapshot(const T& A,const T& B)
	{
		return T::StaticStruct()->CompareScriptStruct(&A,&B,0);
	}
	static bool SameNames(const TSet<FName>& A,const TSet<FName>& B)
	{
		if(A.Num()!=B.Num()) { return false; }
		for(FName Name:A) { if(!B.Contains(Name)) { return false; } }
		return true;
	}
	void V4ImportCase()
	{
		if(Failures) { Finish(); return; }
		TStrongObjectPtr<URogue10mProgressionSaveGame> Seed(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->GetSaveSlotForProfile(ProfileId),0)));
		Check(Seed.IsValid(),TEXT("v4_import_source_QA_snapshot_readable"));
		FRogue10mCharacterProfile ImportedProfile;
		Check(Profiles->CreateProfile(TEXT("V4ImportQa"),FRogue10mCharacterAppearance(),ImportedProfile),TEXT("v4_import_fixture_profile_created"));
		if(Failures) { Finish(); return; }
		Seed->ProfileId=ImportedProfile.ProfileId; Seed->SaveVersion=4;
		Seed->RunId=FGuid::NewGuid(); Seed->LastSettledRunId=FGuid::NewGuid();
		Seed->bRunCompleted=false; Seed->bAbandoned=false;
		Seed->EntryCurrencies=FRogue10mRunCurrencyBaseline();
		const FString V4Slot=Saves->GetLegacySaveSlotForProfile(ImportedProfile.ProfileId)+TEXT("_v4");
		const FString V5Slot=Saves->GetSaveSlotForProfile(ImportedProfile.ProfileId);
		const FString V4File=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),V4Slot+TEXT(".sav"));
		Check(V5Slot.EndsWith(TEXT("_v5")) && !UGameplayStatics::DoesSaveGameExist(V4Slot,0)
			&& !UGameplayStatics::DoesSaveGameExist(V5Slot,0),TEXT("v4_fixture_and_v5_destination_are_fresh_distinct_QA_slots"));
		if(Failures) { Finish(); return; }
		Check(UGameplayStatics::SaveGameToSlot(Seed.Get(),V4Slot,0),TEXT("v4_original_fixture_written_without_v5_file"));
		TArray<uint8> Before,After;
		Check(FFileHelper::LoadFileToArray(Before,*V4File) && !Before.IsEmpty(),TEXT("v4_original_bytes_preserved_for_comparison"));
		Check(Profiles->SelectProfile(ImportedProfile.ProfileId),TEXT("v4_fixture_selected_for_real_character_import"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Character=Cast<ARogue10mCharacter>(PC->GetPawn());
		State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
		Inventory=Character.IsValid()?Character->GetInventoryComponent():nullptr;
		Check(Character.IsValid() && State && Inventory && Saves->IsSaveReady()
			&& Saves->GetAttachedProfileId()==ImportedProfile.ProfileId,TEXT("v4_fixture_automatically_imports_into_ready_character"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Imported(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(V5Slot,0)));
		Check(Imported.IsValid() && Imported->SaveVersion==URogue10mProgressionSaveGame::CurrentVersion,TEXT("v4_import_creates_readable_current_version_v5_slot"));
		if(Failures) { Finish(); return; }
		Check(SameSnapshot(Seed->Growth,Imported->Growth) && SameSnapshot(Seed->Inventory,Imported->Inventory)
			&& SameSnapshot(Seed->Combat,Imported->Combat),TEXT("v4_import_preserves_growth_inventory_and_combat_domains"));
		Check(SameSnapshot(Seed->Town,Imported->Town) && SameSnapshot(Seed->SkillTree,Imported->SkillTree),TEXT("v4_import_preserves_lobby_investments_and_run_skill_ledger"));
		Check(Imported->LobbyEarnedPoints==Seed->LobbyEarnedPoints && Imported->HighestRunLevel==Seed->HighestRunLevel
			&& Imported->RewardDefeatBaseline==Seed->RewardDefeatBaseline,TEXT("v4_import_preserves_lobby_currency_and_growth_metadata"));
		Check(SameNames(Seed->DiscoveredNodes,Imported->DiscoveredNodes) && SameNames(Seed->ClaimedRewards,Imported->ClaimedRewards),TEXT("v4_import_preserves_discovery_and_reward_receipts"));
		Check(Imported->ProfileId==Seed->ProfileId && Imported->RunId==Seed->RunId && Imported->LastSettledRunId==Seed->LastSettledRunId
			&& !Imported->bRunCompleted && !Imported->bAbandoned && !Imported->EntryCurrencies.bCaptured,TEXT("v4_import_preserves_run_identity_and_existing_receipt_without_reset"));
		Check(FFileHelper::LoadFileToArray(After,*V4File) && Before==After,TEXT("v4_original_bytes_unchanged_after_automatic_v5_import"));
		Check(Saves->SaveNow(),TEXT("imported_v5_can_save_normally"));
		Check(FFileHelper::LoadFileToArray(After,*V4File) && Before==After,TEXT("v4_original_bytes_unchanged_after_v5_save"));
		Saves->DetachCharacter(*Character,false);
		Imported->SaveVersion=999;
		Check(UGameplayStatics::SaveGameToSlot(Imported.Get(),V5Slot,0),TEXT("future_v5_QA_fixture_written"));
		const FString V5File=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),V5Slot+TEXT(".sav"));
		TArray<uint8> FutureBefore,FutureAfter;
		Check(FFileHelper::LoadFileToArray(FutureBefore,*V5File),TEXT("future_v5_bytes_captured"));
		Check(!Saves->AttachCharacter(*Character) && !Saves->CanRetrySave() && !Saves->SaveNow(),TEXT("future_v5_rejects_without_falling_back_to_readable_v4"));
		Saves->DetachCharacter(*Character,false);
		Check(FFileHelper::LoadFileToArray(FutureAfter,*V5File) && FutureBefore==FutureAfter,TEXT("rejected_v5_file_remains_byte_identical"));
		Check(FFileHelper::LoadFileToArray(After,*V4File) && Before==After,TEXT("v4_original_bytes_preserved_after_rejected_v5"));
		BadV4Priority(*Seed);
	}
	void BadV4Priority(const URogue10mProgressionSaveGame& ValidSource)
	{
		FRogue10mCharacterProfile BadProfile;
		BadProfile.ProfileId=OtherProfileId;
		Check(BadProfile.ProfileId.IsValid(),TEXT("existing_other_QA_profile_reused_within_three_profile_limit"));
		if(Failures) { Finish(); return; }
		TStrongObjectPtr<URogue10mProgressionSaveGame> Bare(DuplicateObject<URogue10mProgressionSaveGame>(&ValidSource,GetTransientPackage()));
		Bare->ProfileId=BadProfile.ProfileId; Bare->SaveVersion=3;
		Bare->Growth.Level=5; Bare->Growth.Experience=7.f; Bare->Growth.ExperienceToNextLevel=100.f;
		Bare->HighestRunLevel=5;
		const FString BareSlot=Saves->GetLegacySaveSlotForProfile(BadProfile.ProfileId);
		const FString BadV4Slot=BareSlot+TEXT("_v4");
		const FString BadV5Slot=Saves->GetSaveSlotForProfile(BadProfile.ProfileId);
		Check(!UGameplayStatics::DoesSaveGameExist(BareSlot,0) && !UGameplayStatics::DoesSaveGameExist(BadV4Slot,0)
			&& !UGameplayStatics::DoesSaveGameExist(BadV5Slot,0),TEXT("priority_test_uses_only_fresh_QA_slots"));
		if(Failures) { Finish(); return; }
		Check(UGameplayStatics::SaveGameToSlot(Bare.Get(),BareSlot,0),TEXT("valid_legacy_v3_fixture_with_level_five_and_two_town_points_written"));
		const FString BareFile=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),BareSlot+TEXT(".sav"));
		TArray<uint8> BareBefore,BareAfter;
		Check(FFileHelper::LoadFileToArray(BareBefore,*BareFile),TEXT("legacy_priority_fixture_bytes_captured"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> BadV4(DuplicateObject<URogue10mProgressionSaveGame>(Bare.Get(),GetTransientPackage()));
		BadV4->SaveVersion=4; BadV4->ProfileId=FGuid::NewGuid(); // Invalid identity at the higher-priority v4 location.
		Check(UGameplayStatics::SaveGameToSlot(BadV4.Get(),BadV4Slot,0),TEXT("invalid_identity_v4_fixture_written_above_valid_legacy"));
		const FString BadV4File=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),BadV4Slot+TEXT(".sav"));
		TArray<uint8> BadBefore,BadAfter;
		Check(FFileHelper::LoadFileToArray(BadBefore,*BadV4File),TEXT("invalid_v4_priority_bytes_captured"));
		Check(Profiles->SelectProfile(BadProfile.ProfileId) && !Saves->AttachCharacter(*Character)
			&& !Saves->CanRetrySave() && !Saves->SaveNow(),TEXT("invalid_v4_does_not_fall_back_to_valid_legacy_slot"));
		Saves->DetachCharacter(*Character,false);
		Check(!UGameplayStatics::DoesSaveGameExist(BadV5Slot,0),TEXT("invalid_v4_cannot_create_a_recovered_v5_balance"));
		Check(FFileHelper::LoadFileToArray(BareAfter,*BareFile) && BareBefore==BareAfter
			&& FFileHelper::LoadFileToArray(BadAfter,*BadV4File) && BadBefore==BadAfter,TEXT("both_priority_source_files_remain_byte_identical_after_rejection"));
		Finish();
	}
	void ReturnAfterAbandon()
	{
		if(Failures) { Finish(); return; }
		Check(RunState->GetRunPhase()==ERogue10mRunPhase::Abandoned && !RunState->IsRunActive(),TEXT("abandonment_is_distinct_terminal_phase"));
		Check(!RunState->AbandonRun(),TEXT("repeated_abandon_command_cannot_end_again"));
		Check(!Saves->SaveRunSettlement(true),TEXT("abandoned_run_cannot_claim_victory_reward"));
		OldWorld=World;
		Check(Completion->ReturnToTown(),TEXT("saved_abandonment_enters_actual_town_travel"));
		if(Failures) { Finish(); return; }
		Later(0.2,[this]{WaitForTown();});
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
			PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
			Character=Cast<ARogue10mCharacter>(PC->GetPawn());
			State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
			Inventory=Character.IsValid()?Character->GetInventoryComponent():nullptr;
			Check(Character.IsValid() && State && Inventory && Saves->IsSaveReady(),TEXT("abandoned_run_restores_new_hub_pawn"));
			if(Failures) { Finish(); return; }
			Check(UGameplayStatics::GetCurrentLevelName(Candidate,true)==TEXT("StartHub")
				&& RunState->GetRunPhase()==ERogue10mRunPhase::WaitingToStart && !PC->IsRunResultVisible(),TEXT("abandon_return_waits_in_hub_without_prior_result_overlay"));
			Check(Inventory->GetGold()==70 && Inventory->GetCrystals()==5,TEXT("abandon_return_preserves_post_payment_currency_not_pre_cost_or_run_balance"));
			Check(Saves->GetLobbyAvailablePoints()==98 && State->GetTownProgressionComponent()->GetTraitLevel(ERogue10mTownTrait::Vitality)==2,TEXT("abandon_return_keeps_lobby_investment_and_grants_no_reward"));
			Check(State->GetPlayerLevel()==1 && State->GetCurrentExperience()==0
				&& State->GetSkillTreeProgressionComponent()->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)==0,TEXT("abandon_return_clears_run_level_and_mastery"));
			bool bSwordFound=false;
			for(const auto& Container:Inventory->GetInventoryContainers())
			{ for(const auto& Entry:Container.Entries) { bSwordFound |= Entry.ItemData && Entry.ItemData->WeaponType==ERogue10mWeaponType::GreatSword; } }
			for(const auto& Item:Inventory->GetItemSlots()) { bSwordFound |= Item.bOccupied && Item.WeaponType==ERogue10mWeaponType::GreatSword; }
			for(const auto& EquipmentSlot:Inventory->GetLeftEquipmentSlots()) { bSwordFound |= EquipmentSlot.bHasEquippedItem && EquipmentSlot.EquippedItem.WeaponType==ERogue10mWeaponType::GreatSword; }
			Check(!bSwordFound && Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle,TEXT("abandon_return_discards_run_sword_and_restores_knuckle_start"));
			for(FName NodeId:DiscoveredNodes)
			{
				Check(Saves->HasDiscoveredNode(NodeId),TEXT("abandonment_preserves_discovery_information"));
				if(!InitialNodes.Contains(NodeId)) { Check(!State->GetSkillTreeProgressionComponent()->IsNodeUnlocked(NodeId),TEXT("discovery_does_not_regrant_abandoned_run_technique")); }
			}
			Check(Saves->SaveNow(),TEXT("abandon_reset_snapshot_saved_without_extra_credit"));
			Check(Saves->GetLobbyAvailablePoints()==98 && Inventory->GetGold()==70 && Inventory->GetCrystals()==5,TEXT("post_return_save_does_not_restore_twice_or_award_currency"));
			if(Mode==TEXT("Interrupted"))
			{
				TStrongObjectPtr<URogue10mProgressionSaveGame> Recovered(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->GetSaveSlotForProfile(ProfileId),0)));
				Check(Recovered.IsValid() && !Recovered->bEnteredRun && Recovered->RunId!=InterruptedRunId && Recovered->LastSettledRunId==InterruptedRunId
					&& !Recovered->bRunCompleted && !Recovered->bAbandoned && !Recovered->EntryCurrencies.bCaptured
					&& Recovered->Inventory.Gold==70 && Recovered->Inventory.Crystals==5 && Recovered->LobbyEarnedPoints==100,
					TEXT("interrupted_hub_recovery_commits_new_run_and_post_payment_currency_without_reward"));
				Photo(TEXT("02_interrupted_recovered_hub.png"));
			}
			else { Photo(TEXT("04_abandon_returned_hub.png")); }
			Later(0.6,[this]{Finish();}); return;
		}
		Later(0.2,[this]{WaitForTown();});
	}
	void Finish()
	{
		if(bFinished) { return; } bFinished=true;
		if(!ReadOnlyFile.IsEmpty()) { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,false); }
		Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nmanual_play=not_performed\n"),Checks,Failures,*Mode,bRendered);
		if(!FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=RUN_RESULT_UI_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World,OldWorld;
	TWeakObjectPtr<ARogue10mBasicMonster> LateTarget;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	TWeakObjectPtr<URogue10mRunCompletionComponent> Completion;
	TWeakObjectPtr<URogue10mRunResultWidget> Widget;
	URogue10mCharacterProfileSubsystem* Profiles=nullptr;
	URogue10mProgressionSubsystem* Saves=nullptr;
	ARogue10mPlayerState* State=nullptr;
	URogue10mInventoryComponent* Inventory=nullptr;
	FGuid ProfileId,InterruptedRunId,OtherProfileId;
	TArray<FName> InitialNodes,DiscoveredNodes;
	FString Label,Mode,Output,Images,Results,ReadOnlyFile;
	TFunction<void()> Next;
	double Deadline=0,NextAt=0;
	int32 Checks=0,Failures=0,InitialLobby=0,ExpectedAward=0,ExpectedBalance=0,ExpectedGold=0,ExpectedCrystals=9;
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
	UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_RESULT_UI_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestRunResultUI"),TEXT("Checks actual UMG result currency text, pending/confirmed saves, duplicates, zero and saturated awards in isolated QA slots."),FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
