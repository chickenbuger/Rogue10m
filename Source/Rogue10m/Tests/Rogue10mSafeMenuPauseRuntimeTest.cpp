// Isolated programmatic menu/pause fixture. No physical-input or full-run claim.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UnrealClient.h"
#include "UObject/Package.h"

namespace Rogue10mSafeMenuPauseQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		const TCHAR* Args = FCommandLine::Get();
		FString ProfileSlot, Prefix, KeySlot;
		FParse::Value(Args, TEXT("RogueSafeMenuPauseLabel="), Label);
		FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
		FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), KeySlot);
		bool bValidLabel = !Label.IsEmpty() && Label.Len() <= 24;
		for (TCHAR Ch : Label)
		{
			bValidLabel &= (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('a') && Ch <= TEXT('z'))
				|| (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
		}
		if (!bValidLabel || ProfileSlot != TEXT("Rogue10m_Town_QA_MenuPause_") + Label
			|| Prefix != TEXT("Rogue10m_Progression_QA_MenuPause_") + Label
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_MenuPause_") + Label
			|| InWorld.GetNetMode() != NM_Standalone
			|| UWorld::RemovePIEPrefix(InWorld.GetPackage()->GetName()) != TEXT("/Game/Rogue10m/Maps/StartHub"))
		{
			Reject(TEXT("exact_isolated_slots_and_StartHub_required")); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/safe-menu-pause-20261103"), Label);
		Images = FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLog/20261103_SafeMenuPause_") + Label);
		TArray<FString> ExistingProgression;
		const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
		IFileManager::Get().FindFiles(ExistingProgression, *(SaveDirectory / (Prefix + TEXT("_*.sav"))), true, false);
		if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().FileExists(*Output)
			|| IFileManager::Get().DirectoryExists(*Images) || IFileManager::Get().FileExists(*Images)
			|| IFileManager::Get().FileExists(*(SaveDirectory / (ProfileSlot + TEXT(".sav"))))
			|| IFileManager::Get().FileExists(*(SaveDirectory / (KeySlot + TEXT(".sav"))))
			|| !ExistingProgression.IsEmpty())
		{
			Reject(TEXT("fresh_evidence_and_save_namespace_required")); return;
		}
		if (!IFileManager::Get().MakeDirectory(*Output, true))
		{
			Reject(TEXT("evidence_directory_creation_failed")); return;
		}
		bRendered = !FParse::Param(Args, TEXT("nullrhi"));
		if (bRendered && !IFileManager::Get().MakeDirectory(*Images, true))
		{
			Reject(TEXT("screenshot_directory_creation_failed")); return;
		}
		Started = FPlatformTime::Seconds(); Deadline = Started + 90.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (FPlatformTime::Seconds() >= Self->Deadline)
			{
				Self->Check(false, TEXT("ninety_second_wall_watchdog")); Self->Finish(); return false;
			}
			if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt)
			{
				auto Work = MoveTemp(Self->Next); Work();
			}
			return !Self->bFinished;
		}), 0.02f);
		Later(3.0, [this] { Prepare(); });
	}
	bool Finished() const { return bFinished; }

private:
	void Reject(const TCHAR* Reason)
	{
		bFinished = true;
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=SAFE_MENU_PAUSE_FAILED reason=%s"), Reason);
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}
	void Check(bool bOK, const TCHAR* Reason)
	{
		++Checks;
		Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bOK)
		{
			++Failures; UE_LOG(LogRogue10m, Error, TEXT("SAFE_MENU_PAUSE FAIL %s"), Reason);
		}
	}
	bool Continue()
	{
		if (Failures || !World.IsValid() || !PC.IsValid() || !GameMode.IsValid() || !RunState.IsValid())
		{
			if (!Failures) { Check(false, TEXT("fixture_world_controller_and_run_still_valid")); }
			Finish(); return false;
		}
		return true;
	}
	void Later(double Delay, TFunction<void()> Work)
	{
		NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work);
	}
	bool IsPaused() const { return World.IsValid() && UGameplayStatics::IsGamePaused(World.Get()); }
	void Sample()
	{
		WorldSeconds = World->GetTimeSeconds(); RemainingSeconds = RunState->GetRemainingRunSeconds();
	}
	void Photo(const TCHAR* Name)
	{
		if (bRendered) { FScreenshotRequest::RequestScreenshot(Images / Name, true, false); }
	}
	void Prepare()
	{
		auto* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		PC = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		GameMode = World.IsValid() ? World->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
		RunState = World.IsValid() ? World->GetGameState<ARogue10mGameState>() : nullptr;
		Check(Profiles && Saves && PC.IsValid() && GameMode.IsValid() && RunState.IsValid(), TEXT("real_gameplay_subsystems_available"));
		if (!Continue()) { return; }
		Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_is_empty"));
		FRogue10mCharacterProfile Profile;
		Check(!Failures && Profiles->CreateProfile(TEXT("PauseQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("QA_profile_created_and_selected"));
		if (!Continue()) { return; }
		ProfileId = Profile.ProfileId;
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(PC->GetPawn());
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Brawler = Character.IsValid() ? Character->GetBasicBrawlerComponent() : nullptr;
		PlayerState = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Check(Character.IsValid() && Saves->IsSaveReady() && Saves->GetAttachedProfileId() == ProfileId,
			TEXT("QA_character_attached_and_save_ready"));
		Check(Combat.IsValid() && Brawler.IsValid() && PlayerState.IsValid() && Character->GetRogueAttributeSet(),
			TEXT("real_combat_brawler_and_vital_sources_available_for_cancel_boundary"));
		Check(GameMode->IsSafeMenuContext() && RunState->GetRunPhase() == ERogue10mRunPhase::WaitingToStart,
			TEXT("explicit_StartHub_waiting_context_is_safe"));
		Check(!PC->IsAnyBlockingWindowVisible() && !GameMode->HasMenuPause(PC.Get()) && !IsPaused(),
			TEXT("safe_context_without_menu_does_not_pause"));
		if (!Continue()) { return; }
		Sample(); Later(0.8, [this] { IdleHub(); });
	}
	void IdleHub()
	{
		if (!Continue()) { return; }
		Check(World->GetTimeSeconds() > WorldSeconds + 0.3f, TEXT("idle_hub_world_time_advances"));
		Check(RunState->GetRemainingRunSeconds() == RemainingSeconds && !RunState->IsRunActive(),
			TEXT("waiting_hub_countdown_is_inactive_not_a_running_timer_pause_test"));
		PC->ToggleInventory();
		Check(PC->IsInventoryVisible() && GameMode->HasMenuPause(PC.Get()) && IsPaused(),
			TEXT("inventory_acquires_scoped_hub_pause"));
		Check(GameMode->AcquireMenuPause(PC.Get()) && GameMode->AcquireMenuPause(PC.Get()),
			TEXT("repeated_menu_acquisition_is_idempotent"));
		Sample(); Photo(TEXT("01_inventory_paused.png"));
		Later(0.8, [this] { NestedMenus(); });
	}
	void NestedMenus()
	{
		if (!Continue()) { return; }
		Check(IsPaused() && FMath::IsNearlyEqual(World->GetTimeSeconds(), WorldSeconds, 0.001f),
			TEXT("hub_inventory_freezes_world_time_across_wall_time"));
		Check(RunState->GetRemainingRunSeconds() == RemainingSeconds,
			TEXT("hub_menu_preserves_waiting_countdown_value"));
		PC->ToggleItemWindow();
		Check(PC->IsInventoryVisible() && PC->IsItemWindowVisible() && IsPaused(),
			TEXT("inventory_and_equipment_can_overlap_without_unpausing"));
		Photo(TEXT("02_nested_menus_paused.png"));
		Later(0.5, [this]
		{
			if (!Continue()) { return; }
			PC->ToggleInventory();
			Check(!PC->IsInventoryVisible() && PC->IsItemWindowVisible() && IsPaused() && GameMode->HasMenuPause(PC.Get()),
				TEXT("closing_one_of_two_windows_keeps_pause"));
			PC->ToggleItemWindow();
			Check(!PC->IsAnyBlockingWindowVisible() && !GameMode->HasMenuPause(PC.Get()) && !IsPaused(),
				TEXT("last_window_close_releases_only_menu_pause_including_repeated_acquisition"));
			Sample(); Later(0.6, [this] { MenuTransitions(); });
		});
	}
	void MenuTransitions()
	{
		if (!Continue()) { return; }
		Check(World->GetTimeSeconds() > WorldSeconds + 0.2f, TEXT("world_time_resumes_after_last_menu_close"));
		PC->ToggleInventory(); PC->ToggleSkillTree();
		Check(!PC->IsInventoryVisible() && PC->IsSkillTreeVisible() && IsPaused(),
			TEXT("inventory_to_skill_tree_transition_retains_pause"));
		PC->ToggleSettings();
		Check(!PC->IsSkillTreeVisible() && PC->IsSettingsVisible() && IsPaused(),
			TEXT("skill_tree_to_settings_transition_retains_pause"));
		PC->CloseAllBlockingPanels();
		Check(!IsPaused() && !GameMode->HasMenuPause(PC.Get()), TEXT("close_all_releases_menu_lease"));
		HeldMouseBeforeMenus();
	}
	void CaptureHeldUseCounts()
	{
		HeldUseCounts.Reset();
		TArray<const URogue10mAttackSkillData*> Pending;
		for (const auto* Skill : Combat->GetUnlockedWeaponSkills()) { if (Skill) { Pending.Add(Skill); } }
		while (!Pending.IsEmpty() && HeldUseCounts.Num() < 128)
		{
			const auto* Skill = Pending.Pop();
			if (!Skill || HeldUseCounts.Contains(Skill->GetFName())) { continue; }
			HeldUseCounts.Add(Skill->GetFName(), PlayerState->GetSkillUseCount(Skill->GetFName()));
			if (Skill->NextComboSkill) { Pending.Add(Skill->NextComboSkill); }
		}
	}
	bool HeldUseCountsUnchanged() const
	{
		for (const auto& Pair : HeldUseCounts)
		{
			if (PlayerState->GetSkillUseCount(Pair.Key) != Pair.Value) { return false; }
		}
		return true;
	}
	void HeldMouseBeforeMenus()
	{
		if (!Continue()) { return; }
		Check(!Combat->IsAnyAttackInputHeld(), TEXT("mouse_hold_fixture_starts_without_pending_attack_input"));
		Combat->HandleAttackPressed(true); Combat->HandleAttackPressed(false);
		Check(Combat->IsAnyAttackInputHeld(), TEXT("real_primary_and_secondary_press_create_observable_held_input"));
		const auto BeforeMenu = Brawler->GetSnapshot();
		HeldAcceptedSerial = BeforeMenu.AcceptedSerial;
		Results += FString::Printf(TEXT("held_fixture_basic_brawler_active=%d\n"), BeforeMenu.bActive);
		CaptureHeldUseCounts();
		Check(!HeldUseCounts.IsEmpty(), TEXT("held_input_fixture_records_real_unlocked_skill_use_counts"));
		PC->ToggleInventory();
		Check(IsPaused() && !Combat->IsAnyAttackInputHeld(), TEXT("opening_safe_inventory_cancels_all_mouse_and_bound_holds_immediately"));
		Combat->HandleAttackReleased(true); Combat->HandleAttackReleased(false);
		Check(!Combat->IsAnyAttackInputHeld() && HeldUseCountsUnchanged()
			&& Brawler->GetSnapshot().AcceptedSerial == HeldAcceptedSerial,
			TEXT("mouse_releases_during_pause_do_not_preserve_hold_or_accept_new_attack"));
		Later(0.3, [this]
		{
			if (!Continue()) { return; }
			PC->ToggleInventory();
			const auto* Vitals = Character->GetRogueAttributeSet();
			const float Health = Vitals->GetHealth(), Stamina = Vitals->GetStamina();
			const float Mana = Vitals->GetMana(), Identity = Vitals->GetIdentity();
			Combat->HandleAttackReleased(true); Combat->HandleAttackReleased(false);
			Check(!Combat->IsAnyAttackInputHeld() && HeldUseCountsUnchanged()
				&& Brawler->GetSnapshot().AcceptedSerial == HeldAcceptedSerial,
				TEXT("late_mouse_releases_after_resume_cannot_fire_cancelled_attack"));
			Check(Vitals->GetHealth() == Health && Vitals->GetStamina() == Stamina
				&& Vitals->GetMana() == Mana && Vitals->GetIdentity() == Identity,
				TEXT("late_release_consumes_no_health_stamina_mana_or_identity"));
			const auto AfterResume = Brawler->GetSnapshot();
			Check(AfterResume.Phase != ERogue10mBasicBrawlerPhase::Charging && !AfterResume.bComboBuffered
				&& !Brawler->HasBufferedAttack(), TEXT("resume_has_no_stale_brawler_charge_or_buffer"));
			Later(0.4, [this]
			{
				if (!Continue()) { return; }
				Check(!Combat->IsAnyAttackInputHeld() && HeldUseCountsUnchanged()
					&& Brawler->GetSnapshot().AcceptedSerial == HeldAcceptedSerial,
					TEXT("normal_world_monitor_after_resume_does_not_accept_ghost_attack"));
				DebugHandoff();
			});
		});
	}
	void DebugHandoff()
	{
		if (!Continue()) { return; }
		PC->ToggleSettings();
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F6, IE_Pressed, 1.0f));
		Later(0.2, [this]
		{
			if (!Continue()) { return; }
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F6, IE_Released, 0.0f));
			Later(0.2, [this]
			{
				if (!Continue()) { return; }
				auto* Tools = PC->GetCombatTestComponent();
				Check(Tools && Tools->IsPanelOpen() && !PC->IsSettingsVisible() && IsPaused()
					&& !GameMode->HasMenuPause(PC.Get()), TEXT("paused_F6_input_hands_menu_pause_to_debug_panel"));
				PC->ToggleInventory();
				Check(Tools && !Tools->IsPanelOpen() && PC->IsInventoryVisible() && IsPaused()
					&& GameMode->HasMenuPause(PC.Get()), TEXT("debug_panel_to_inventory_hands_pause_back_without_stale_lease"));
				PC->CloseAllBlockingPanels();
				Check(!IsPaused(), TEXT("closing_menu_after_debug_handoff_resumes_world"));
				KeyIndex = 0; Later(0.2, [this] { BeginCloseKey(); });
			});
		});
	}
	void BeginCloseKey()
	{
		if (!Continue()) { return; }
		const TArray<FKey> Keys = {EKeys::I, EKeys::P, EKeys::K, EKeys::O, EKeys::Escape, EKeys::F10};
		if (KeyIndex >= Keys.Num()) { ExternalPauseBeforeMenu(); return; }
		CurrentKey = Keys[KeyIndex];
		if (CurrentKey == EKeys::I) { PC->ToggleInventory(); }
		else if (CurrentKey == EKeys::P) { PC->ToggleItemWindow(); }
		else if (CurrentKey == EKeys::K) { PC->ToggleSkillTree(); }
		else { PC->ToggleSettings(); }
		Check(IsPaused() && PC->IsAnyBlockingWindowVisible(), TEXT("programmatic_close_key_fixture_starts_with_paused_menu"));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(CurrentKey, IE_Pressed, 1.0f));
		Later(0.2, [this]
		{
			if (!Continue()) { return; }
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(CurrentKey, IE_Released, 0.0f));
			Later(0.2, [this]
			{
				if (!Continue()) { return; }
				Check(!PC->IsAnyBlockingWindowVisible() && !IsPaused() && !GameMode->HasMenuPause(PC.Get()),
					*FString::Printf(TEXT("controller_input_%s_closes_menu_while_paused"), *CurrentKey.ToString()));
				++KeyIndex; Later(0.1, [this] { BeginCloseKey(); });
			});
		});
	}
	void ExternalPauseBeforeMenu()
	{
		if (!Continue()) { return; }
		Check(PC->SetPause(true) && IsPaused(), TEXT("external_pause_before_menu_is_registered"));
		PC->ToggleInventory();
		Check(GameMode->HasMenuPause(PC.Get()), TEXT("menu_can_register_own_lease_during_existing_pause"));
		PC->ToggleInventory();
		Check(!GameMode->HasMenuPause(PC.Get()) && IsPaused(), TEXT("closing_menu_preserves_preexisting_external_pause"));
		PC->SetPause(false);
		Check(!IsPaused(), TEXT("explicit_external_unpause_releases_preexisting_pause"));
		PC->ToggleSettings();
		Check(PC->SetPause(true), TEXT("external_pause_during_menu_is_registered_despite_engine_short_circuit"));
		PC->ToggleSettings();
		Check(!GameMode->HasMenuPause(PC.Get()) && IsPaused(), TEXT("closing_menu_preserves_external_pause_added_during_menu"));
		PC->SetPause(false);
		Check(!IsPaused(), TEXT("explicit_external_unpause_releases_pause_added_during_menu"));
		PC->ToggleSettings(); PC->SetPause(true); PC->SetPause(false);
		Check(IsPaused() && GameMode->HasMenuPause(PC.Get()), TEXT("external_unpause_does_not_clear_open_menu_lease"));
		PC->ToggleSettings();
		Check(!IsPaused(), TEXT("menu_close_resumes_after_external_owner_explicitly_released"));
		ForeignPause();
	}
	void ForeignPause()
	{
		if (!Continue()) { return; }
		bForeignMayRelease = false;
		ForeignEvaluations = 0;
		Check(GameMode->SetPause(PC.Get(), FCanUnpause::CreateLambda([this]
			{ ++ForeignEvaluations; return bForeignMayRelease; })),
			TEXT("foreign_bound_pause_delegate_registered"));
		PC->ToggleInventory(); PC->CloseAllBlockingPanels();
		Check(IsPaused() && !GameMode->HasMenuPause(PC.Get()), TEXT("menu_release_preserves_foreign_bound_delegate"));
		Check(ForeignEvaluations == 0, TEXT("menu_release_does_not_evaluate_foreign_unpause_predicate"));
		PC->SetPause(false);
		Check(IsPaused(), TEXT("unready_foreign_delegate_still_prevents_global_unpause"));
		Check(ForeignEvaluations > 0, TEXT("explicit_global_unpause_evaluates_foreign_predicate"));
		bForeignMayRelease = true;
		const int32 BeforeReadyMenuClose = ForeignEvaluations;
		PC->ToggleInventory(); PC->CloseAllBlockingPanels();
		Check(IsPaused() && ForeignEvaluations == BeforeReadyMenuClose,
			TEXT("menu_close_neither_evaluates_nor_releases_even_ready_foreign_predicate"));
		PC->SetPause(false);
		Check(!IsPaused(), TEXT("foreign_delegate_releases_only_when_its_own_condition_is_ready"));
		PC->ToggleSettings();
		Check(IsPaused() && GameMode->HasMenuPause(PC.Get()), TEXT("phase_transition_fixture_starts_with_hub_menu_pause"));
		Check(PC->SetPause(true), TEXT("phase_transition_fixture_adds_separate_external_pause"));
		RunState->StartRun();
		Check(RunState->IsRunActive() && !GameMode->IsSafeMenuContext(), TEXT("active_run_in_hub_is_not_safe_context"));
		Check(PC->IsSettingsVisible() && IsPaused() && !GameMode->HasMenuPause(PC.Get()),
			TEXT("run_phase_event_releases_only_menu_pause_and_preserves_external_owner"));
		PC->SetPause(false);
		Check(!IsPaused() && PC->IsSettingsVisible(), TEXT("explicit_external_release_resumes_active_run_with_settings_open"));
		Sample(); Photo(TEXT("03_active_run_menu_continues.png"));
		Later(1.4, [this] { UnsafeMenus(); });
	}
	void UnsafeMenus()
	{
		if (!Continue()) { return; }
		Check(!IsPaused() && World->GetTimeSeconds() > WorldSeconds + 0.7f
			&& RunState->GetRemainingRunSeconds() < RemainingSeconds,
			TEXT("unsafe_active_run_settings_leave_world_and_countdown_running"));
		PC->CloseAllBlockingPanels(); PC->ToggleInventory(); PC->ToggleItemWindow();
		Check(PC->IsInventoryVisible() && PC->IsItemWindowVisible() && !IsPaused() && !GameMode->HasMenuPause(PC.Get()),
			TEXT("unsafe_inventory_equipment_group_does_not_acquire_pause"));
		Sample(); Later(1.4, [this]
		{
			if (!Continue()) { return; }
			Check(!IsPaused() && World->GetTimeSeconds() > WorldSeconds + 0.7f
				&& RunState->GetRemainingRunSeconds() < RemainingSeconds,
				TEXT("unsafe_inventory_equipment_leave_world_and_countdown_running"));
			PC->ToggleSkillTree();
			Check(PC->IsSkillTreeVisible() && !PC->IsInventoryVisible() && !PC->IsItemWindowVisible()
				&& !IsPaused() && !GameMode->HasMenuPause(PC.Get()), TEXT("unsafe_skill_tree_does_not_acquire_pause"));
			Sample(); Later(1.4, [this]
			{
				if (!Continue()) { return; }
				Check(!IsPaused() && World->GetTimeSeconds() > WorldSeconds + 0.7f
					&& RunState->GetRemainingRunSeconds() < RemainingSeconds,
					TEXT("unsafe_skill_tree_leaves_world_and_countdown_running"));
				PC->CloseAllBlockingPanels();
				Check(Saves->GetAttachedProfileId() == ProfileId && Saves->IsSaveReady(),
					TEXT("menu_tests_preserve_selected_QA_profile_and_save_readiness"));
				Finish();
			});
		});
	}
	void Finish()
	{
		if (bFinished) { return; }
		bFinished = true;
		if (PC.IsValid())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(CurrentKey, IE_Released, 0.0f));
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F6, IE_Released, 0.0f));
			PC->CloseAllBlockingPanels(); bForeignMayRelease = true; PC->SetPause(false);
		}
		Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nrendered=%d\nwall_seconds=%.3f\nphysical_input=not_performed\nnormal_run=not_performed\nfixture=isolated_StartHub_profile_and_explicit_run_start\nsafe_countdown_scope=WaitingToStart_inactive_timer_only\nunsafe_countdown_scope=real_active_run_TimerManager\nNOTE=Controller InputKey pulses are programmatic. Screenshots require separate visual review. No production save edits, combat rewards, timer duration overrides or map asset edits.\n"),
			Checks, Failures, bRendered, FPlatformTime::Seconds() - Started);
		if (!FFileHelper::SaveStringToFile(Results, *(Output / TEXT("results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=SAFE_MENU_PAUSE_%s checks=%d failures=%d output=%s"),
			Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mGameMode> GameMode;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<URogue10mCombatComponent> Combat;
	TWeakObjectPtr<URogue10mBasicBrawlerComponent> Brawler;
	TWeakObjectPtr<ARogue10mPlayerState> PlayerState;
	TMap<FName, int32> HeldUseCounts;
	URogue10mCharacterProfileSubsystem* Profiles = nullptr;
	URogue10mProgressionSubsystem* Saves = nullptr;
	FGuid ProfileId;
	FString Label, Output, Images, Results;
	FKey CurrentKey = EKeys::I;
	TFunction<void()> Next;
	double Started = 0.0, Deadline = 0.0, NextAt = 0.0;
	float WorldSeconds = 0.0f, RemainingSeconds = 0.0f;
	int32 Checks = 0, Failures = 0, KeyIndex = 0, ForeignEvaluations = 0, HeldAcceptedSerial = 0;
	bool bFinished = false, bRendered = false, bForeignMayRelease = false;
};

static TSharedPtr<FRun> Active;
static void RunCommand()
{
	if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>(); Active->Start(*World); return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=SAFE_MENU_PAUSE_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestSafeMenuPause"),
	TEXT("Fresh isolated profile: safe menu aggregation, actual world time, paused close keys, exact pause ownership and active-run countdown."),
	FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
