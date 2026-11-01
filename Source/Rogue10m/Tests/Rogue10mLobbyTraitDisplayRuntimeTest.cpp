// Editor executable UI fixture only: no selected profile, award, purchase or SaveNow.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitWidget.h"
#include "Rogue10mTutorialNPC.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UnrealClient.h"

namespace Rogue10mLobbyTraitDisplayQA
{
static FString Csv(FString Value)
{
	return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")) + TEXT("\"");
}
template <typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}
static bool SafeLabel(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 24) { return false; }
	for (TCHAR C : Value)
	{
		if (!((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z'))
			|| (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_'))) { return false; }
	}
	return true;
}
class FRun : public TSharedFromThis<FRun>
{
public:
	bool Finished() const { return bFinished; }
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString TownSlot, Prefix, KeySlot;
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyTraitDisplayLabel="), Label);
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyTraitDisplayOutput="), Output);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), TownSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), Prefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), KeySlot);
		FString Expected = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("DevLog/LobbyTraitDisplayQA") / Label / TEXT("Driver"));
		const bool bHasOutput = !Output.IsEmpty();
		Output = bHasOutput ? FPaths::ConvertRelativePathToFull(Output) : FString();
		FPaths::NormalizeDirectoryName(Expected); FPaths::NormalizeDirectoryName(Output);
		FPaths::CollapseRelativeDirectories(Expected); FPaths::CollapseRelativeDirectories(Output);
		TArray<FString> Existing;
		IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(Prefix + TEXT("_*.sav")), true, false);
		auto* Profiles = InWorld.GetGameInstance() ? InWorld.GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		const auto* Run = InWorld.GetGameState<ARogue10mGameState>();
		const bool bValid = SafeLabel(Label) && bHasOutput && Output.Equals(Expected, ESearchCase::IgnoreCase)
			&& TownSlot.Len() <= 80 && Prefix.Len() <= 80 && KeySlot.Len() <= 80
			&& TownSlot == TEXT("Rogue10m_Town_QA_LobbyDisplay_") + Label
			&& Prefix == TEXT("Rogue10m_Progression_QA_LobbyDisplay_") + Label
			&& KeySlot == TEXT("Rogue10m_Fighter_QA_LobbyDisplay_") + Label
			&& !UGameplayStatics::DoesSaveGameExist(TownSlot, 0) && !UGameplayStatics::DoesSaveGameExist(KeySlot, 0) && Existing.IsEmpty()
			&& Profiles && Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile()
			&& InWorld.GetNetMode() == NM_Standalone && Run && Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart
			&& UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName()) == TEXT("/Game/Rogue10m/Maps/StartHub")
			&& GEngine && GEngine->GameViewport && FSlateApplication::IsInitialized()
			&& !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && !IFileManager::Get().DirectoryExists(*Output);
		if (!bValid)
		{
			UE_LOG(LogRogue10m, Error, TEXT("LOBBY_TRAIT_DISPLAY_REJECTED exact_fresh_three_slots_output_rendered_StartHub_profile0_required"));
			bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 2); return;
		}
		ExpectedTownSlot = TownSlot; ExpectedPrefix = Prefix; ExpectedKeySlot = KeySlot;
		if (!IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true))
		{
			bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 2); return;
		}
		Started = FPlatformTime::Seconds();
		ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak = TWeakPtr<FRun>(AsShared())]
		{
			if (const auto Self = Weak.Pin(); Self && !Self->bFinished)
			{
				Self->Check(false, TEXT("external_process_exit_before_completion")); Self->Finish(true, false);
			}
		});
		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([Weak = TWeakPtr<FRun>(AsShared())](UWorld* W, bool, bool)
		{
			if (const auto Self = Weak.Pin(); Self && !Self->bFinished && W == Self->World.Get())
			{
				Self->Check(false, TEXT("world_cleanup_before_completion")); Self->Finish(false, false);
			}
		});
		Later(1.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (!Self->World.IsValid() || FPlatformTime::Seconds() - Self->Started > 90.0)
			{
				Self->Check(false, TEXT("world_lost_or_90_second_watchdog")); Self->Finish(Self->World.IsValid()); return false;
			}
			if (Self->Next && FPlatformTime::Seconds() >= Self->NextTime)
			{
				auto Work = MoveTemp(Self->Next); Work();
			}
			return !Self->bFinished;
		}), 0.03f);
	}
private:
	void Check(bool Condition, const TCHAR* Name)
	{
		++Checks; if (!Condition) { ++Failures; }
		Results += FString::Printf(TEXT("%s %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), Name);
		UE_LOG(LogRogue10m, Display, TEXT("LOBBY_TRAIT_DISPLAY_CHECK %s %s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Name);
	}
	void Later(double Delay, TFunction<void()> Work) { NextTime = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work); }
	FString Text(const TCHAR* Name) const
	{
		const auto* Block = Widget.IsValid() ? Cast<UTextBlock>(Widget->GetWidgetFromName(Name)) : nullptr;
		return Block ? Block->GetText().ToString() : FString();
	}
	void Prepare()
	{
		PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Town = State.IsValid() ? State->GetTownProgressionComponent() : nullptr;
		Inventory = Player.IsValid() ? Player->GetInventoryComponent() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		auto* Save = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(PC.IsValid() && PC->IsLocalPlayerController() && PC->HasAuthority() && Player.IsValid()
			&& State.IsValid() && Town.IsValid() && Inventory.IsValid() && Tree.IsValid(), TEXT("normal_local_StartHub_pawn_state_components"));
		Check(Save && !Save->GetAttachedProfileId().IsValid() && (!Player.IsValid() || !Save->IsAttachedTo(*Player)), TEXT("no_attached_profile_no_SaveData_mutation"));
		Check(PC.IsValid() && !PC->IsAnyBlockingWindowVisible() && !PC->GetTraitWidget(), TEXT("no_existing_blocking_or_trait_widget_to_disturb"));
		if (Failures) { Finish(); return; }
		OriginalGrowth = State->CaptureGrowth(); OriginalTown = Town->CaptureProgression();
		OriginalInventory = Inventory->CaptureInventory(); OriginalTree = Tree->Capture();
		OriginalBudget = Town->GetLobbyEarnedBudget(); OriginalUnlock = Town->GetLobbyUnlockLevel(); bPrepared = true;
		Check(State->GetPlayerLevel() == 1 && OriginalTown.TraitInvestments.IsEmpty(), TEXT("fresh_level1_uninvested_fixture"));
		Town->SetLobbyBudget(0, 1);
		State->AddExperience(1500);
		Check(State->GetPlayerLevel() > 1 && Town->GetAvailableTraitPoints() == 0 && Town->GetLobbyEarnedBudget() == 0,
			TEXT("run_XP_does_not_create_lobby_budget_UI_fixture_only"));
		Check(State->RestoreGrowth(OriginalGrowth) && State->GetPlayerLevel() == 1, TEXT("run_XP_fixture_restored_before_UI_cases"));
		if (Failures) { Finish(); return; }
		Definition = Town->GetUpgradeDefinition(Trait);
		Check(Definition.UnlockLevel > 1 && Definition.UnlockLevel <= 20 && Definition.PointCost > 0 && Definition.PointCost <= 20,
			TEXT("actual_trait_tuning_supports_highest20_budget20_cases"));
		if (Failures) { Finish(); return; }
		FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient; Params.OverrideLevel = World->PersistentLevel;
		const FVector NpcAt = Player->GetActorLocation() + Player->GetActorForwardVector() * 160.0
			- FVector(0.0, 0.0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		NPC = World->SpawnActor<ARogue10mTutorialNPC>(ARogue10mTutorialNPC::StaticClass(), FTransform(FRotator::ZeroRotator, NpcAt), Params);
		Check(NPC.IsValid(), TEXT("owned_transient_native_NPC_no_map_edit"));
		if (Failures) { Finish(); return; }
		Later(0.6, [this] { OpenFixture(); });
	}
	void OpenFixture()
	{
		Check(NPC.IsValid() && NPC->BeginDialogue(Player.Get()), TEXT("actual_NPC_dialogue_fixture_context"));
		Check(NPC.IsValid() && NPC->OpenTraitWindow(), TEXT("actual_Controller_TraitWidget_opened_without_selected_profile"));
		Widget = PC->GetTraitWidget();
		Check(Widget.IsValid() && Widget->IsInViewport() && Widget->IsVisible()
			&& PC->GetTraitServiceNPC() == NPC.Get(), TEXT("owned_visible_real_trait_widget"));
		if (Failures) { Finish(); return; }
		Town->SetLobbyBudget(20, 20); Widget->SelectTrait(Trait); Widget->RefreshFromData();
		if (auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("TraitListScroll"))))
		{
			for (const auto& Card : Widget->GetVisibleTraitCards()) { if (Card.Trait == Trait) { Scroll->ScrollWidgetIntoView(Card.Button, false); } }
		}
		Later(0.4, [this]
		{
			Probe(TEXT("highest20_budget20"), 20, 20, false, true);
			if (bFinished) { return; }
			PhotoThen(TEXT("01_highest20_budget20.png"), [this] { LowHighest(); });
		});
	}
	void Probe(const TCHAR* Name, int32 Budget, int32 Highest, bool bExpectedLocked, bool bExpectedAllowed)
	{
		if (!Widget.IsValid() || !Town.IsValid() || !State.IsValid()) { Check(false, TEXT("UI_fixture_context_lost")); Finish(); return; }
		FText Reason; const bool bAllowed = Town->CanUpgradeTrait(Trait, Reason);
		const auto* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("TraitUpgradeButton")));
		const FRogue10mTraitCardView* Card = Widget->GetVisibleTraitCards().FindByPredicate([this](const auto& View) { return View.Trait == Trait; });
		const FString CardText = Card && Card->State ? Card->State->GetText().ToString() : FString();
		const bool bCardLocked = CardText.Contains(TEXT("LV"));
		Check(State->GetPlayerLevel() == 1 && Town->GetLobbyEarnedBudget() == Budget && Town->GetLobbyUnlockLevel() == Highest, TEXT("case_temporary_Town_budget_and_highest_values_retained"));
		Check(Card && Card->State && bCardLocked == bExpectedLocked, TEXT("actual_card_level_lock_matches_highest_threshold"));
		Check(bAllowed == bExpectedAllowed && Button && Button->GetIsEnabled() == bAllowed, TEXT("real_upgrade_button_enabled_matches_CanUpgradeTrait_no_purchase"));
		Check(Text(TEXT("TraitCharacterIdentity")).Contains(FString::Printf(TEXT("최고 런 LV %d"), Highest)), TEXT("identity_labels_actual_highest_run_level_value"));
		Check(Text(TEXT("TraitAvailablePoints")) == TEXT("보유 로비 재화  ") + FText::AsNumber(Budget).ToString()
			&& Text(TEXT("SelectedTraitCost")) == TEXT("필요 로비 재화  ") + FText::AsNumber(Definition.PointCost).ToString(), TEXT("actual_widget_lobby_budget_and_cost_text_values_match_backend"));
		Check(Text(TEXT("SelectedTraitConditions")).Contains(FString::Printf(TEXT("최고 런 LV %d"), Definition.UnlockLevel)), TEXT("actual_condition_text_labels_actual_unlock_threshold"));
		Cases += FString::Printf(TEXT("%s,%d,%d,%d,%d,%d,%s,%s,%s,%s\n"), Name, Budget, Highest, bCardLocked, bAllowed,
			Button && Button->GetIsEnabled(), *Csv(CardText), *Csv(Text(TEXT("TraitCharacterIdentity"))), *Csv(Text(TEXT("TraitAvailablePoints"))), *Csv(Reason.ToString()));
	}
	void LowHighest()
	{
		Town->SetLobbyBudget(20, 1); Widget->RefreshFromData();
		Probe(TEXT("highest1_budget20"), 20, 1, true, false);
		if (bFinished) { return; }
		PhotoThen(TEXT("02_highest1_budget20.png"), [this]
		{
			Town->SetLobbyBudget(0, 20); Widget->RefreshFromData();
			Probe(TEXT("highest20_budget0"), 0, 20, false, false);
			if (bFinished) { return; }
			Check(Text(TEXT("SelectedTraitRemainingPoints")).Contains(TEXT("로비 재화 부족")), TEXT("zero_budget_shortage_uses_lobby_currency_label"));
			Check(Text(TEXT("SelectedTraitConditions")).Contains(TEXT("로비 재화가 부족합니다.")), TEXT("backend_reason_and_detail_use_lobby_currency_not_legacy_trait_points"));
			Check(Same(OriginalInventory, Inventory->CaptureInventory()) && Same(OriginalTown, Town->CaptureProgression())
				&& Same(OriginalTree, Tree->Capture()), TEXT("no_inventory_investment_or_learning_mutation_no_purchase"));
			PhotoThen(TEXT("03_highest20_budget0.png"), [this] { Finish(); });
		});
	}
	void PhotoThen(const TCHAR* Name, TFunction<void()> After)
	{
		if (bFinished) { return; }
		const FString File = Output / TEXT("screenshots") / Name; Photos.Add(File);
		Later(0.1, [this, File, After = MoveTemp(After)]() mutable
		{
			FScreenshotRequest::RequestScreenshot(File, true, false);
			Later(0.85, [this, File, After = MoveTemp(After)]() mutable
			{
				Check(IFileManager::Get().FileSize(*File) > 0, TEXT("requested_game_view_screenshot_file_observed_not_visually_reviewed"));
				if (After) { After(); }
			});
		});
	}
	void Cleanup(bool bWorldUsable)
	{
		if (bCleaned) { return; } bCleaned = true;
		if (!bWorldUsable) { CleanupNote = TEXT("world_teardown_snapshot_restore_skipped"); return; }
		if (PC.IsValid() && NPC.IsValid() && PC->GetTraitServiceNPC() == NPC.Get()) { PC->CloseTraitWindow(); }
		if (Widget.IsValid()) { Widget->ReleaseContext(); Widget->RemoveFromParent(); }
		if (NPC.IsValid()) { NPC->CloseDialogue(); NPC->Destroy(); }
		if (bPrepared && State.IsValid() && Town.IsValid())
		{
			const bool bGrowth = State->RestoreGrowth(OriginalGrowth);
			Town->SetLobbyBudget(OriginalBudget, OriginalUnlock);
			bool bTown = Same(OriginalTown, Town->CaptureProgression());
			if (!bTown) { bTown = Town->RestoreProgression(OriginalTown); }
			Check(bGrowth && bTown && Same(OriginalGrowth, State->CaptureGrowth())
				&& Town->GetLobbyEarnedBudget() == OriginalBudget && Town->GetLobbyUnlockLevel() == OriginalUnlock, TEXT("original_growth_Town_snapshot_budget_highest_restored"));
			Check(Inventory.IsValid() && Tree.IsValid() && Same(OriginalInventory, Inventory->CaptureInventory())
				&& Same(OriginalTree, Tree->Capture()), TEXT("original_inventory_and_tree_remain_unchanged"));
			CleanupNote = TEXT("restored_original_Growth_Town_budget_highest_owned_NPC_widget_only");
		}
		else { CleanupNote = bPrepared ? TEXT("prepared_context_lost_restore_incomplete") : TEXT("not_prepared_nothing_to_restore"); if (bPrepared) { Check(false, TEXT("snapshot_restore_context_lost")); } }
	}
	void Finish(bool bWorldUsable = true, bool bRequestExit = true)
	{
		if (bFinished) { return; }
		bFinished = true; Next = nullptr;
		FCoreDelegates::OnPreExit.Remove(ExitHandle); FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		Cleanup(bWorldUsable);
		if (bWorldUsable && World.IsValid())
		{
			const auto* Profiles = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
			TArray<FString> Existing;
			IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(ExpectedPrefix + TEXT("_*.sav")), true, false);
			Check(Profiles && Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile() && Existing.IsEmpty()
				&& !UGameplayStatics::DoesSaveGameExist(ExpectedTownSlot, 0) && !UGameplayStatics::DoesSaveGameExist(ExpectedKeySlot, 0), TEXT("no_profile_selection_creation_or_QA_save_files_after_display_fixture"));
		}
		if (!Output.IsEmpty())
		{
			const FString Summary = FString::Printf(TEXT("checks=%d\nfailures=%d\nscope=display_fixture_only\nprofile_mutation=none\nphysical_input=not_performed\npurchase=not_performed\nSaveNow=not_called\ncleanup=%s\n"), Checks, Failures, *CleanupNote);
			const bool bFiles = FFileHelper::SaveStringToFile(Results, *(Output / TEXT("checks.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
				&& FFileHelper::SaveStringToFile(Cases, *(Output / TEXT("cases.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
				&& FFileHelper::SaveStringToFile(Summary, *(Output / TEXT("summary.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			if (!bFiles) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("LOBBY_TRAIT_DISPLAY_RESULT_WRITE_FAILED")); }
		}
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=LOBBY_TRAIT_DISPLAY_%s checks=%d failures=%d scope=display_fixture_only profile_mutation=none output=%s"),
			Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, *Output);
		if (bRequestExit) { FPlatformMisc::RequestExitWithStatus(false, Failures ? 2 : 0); }
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mTownProgressionComponent> Town;
	TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Tree;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<URogue10mTraitWidget> Widget;
	FRogue10mGrowthSnapshot OriginalGrowth;
	FRogue10mTownProgressionSnapshot OriginalTown;
	FRogue10mInventorySnapshot OriginalInventory;
	FRogue10mSkillTreeSnapshot OriginalTree;
	FRogue10mTraitDefinition Definition;
	const ERogue10mTownTrait Trait = ERogue10mTownTrait::LastStand;
	int32 OriginalBudget = INDEX_NONE, OriginalUnlock = 1, Checks = 0, Failures = 0;
	double Started = 0, NextTime = 0;
	FString Label, Output, Results, CleanupNote;
	FString ExpectedTownSlot, ExpectedPrefix, ExpectedKeySlot;
	FString Cases = TEXT("case,budget,highest,card_locked,backend_allowed,button_enabled,card_text,identity_text,budget_text,backend_reason\n");
	TArray<FString> Photos;
	TFunction<void()> Next;
	FDelegateHandle ExitHandle, CleanupHandle;
	bool bPrepared = false, bCleaned = false, bFinished = false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
	if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>(); Active->Start(*World); return;
		}
	}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestLobbyTraitDisplay"),
	TEXT("Fresh exact LobbyDisplay QA slots, rendered StartHub/profile0 only. Temporary Town display fixture; no purchase, award or save validation."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
