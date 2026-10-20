// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mRunResultWidget.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace Rogue10mRunCompletionTest
{
template<typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool Safe(const FString& Value, const TCHAR* Prefix, int32 Maximum)
{
	if (!Value.StartsWith(Prefix) || Value.Len() > Maximum) { return false; }
	for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}

/** Only runs in its own -game process. Runtime fixtures never save editor assets. */
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld, const FString& InMode)
	{
		World = &InWorld;
		Mode = InMode;
		bRead = Mode.EndsWith(TEXT("Read"));
		bFailure = Mode.StartsWith(TEXT("Failure"));
		FString Keys;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), SavePrefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), Keys);
		if (!Safe(ProfileSlot, TEXT("Rogue10m_Town_QA_RunCompletion_"), 100)
			|| !Safe(SavePrefix, TEXT("Rogue10m_Progression_QA_RunCompletion_"), 80)
			|| !Safe(Keys, TEXT("Rogue10m_Fighter_QA_RunCompletion_"), 100)
			|| InWorld.GetNetMode() != NM_Standalone || GIsEditor)
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=RUN_COMPLETION_FAILED reason=isolated_standalone_QA_overrides_required"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/boss-run-completion-20261020"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		Deadline = FPlatformTime::Seconds() + 420.0;
		Watchdog = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			const double Now = FPlatformTime::Seconds();
			if (Now > Self->Deadline)
			{
				Self->Check(false, TEXT("bounded_run_completion_watchdog")); Self->Finish(); return false;
			}
			Self->ObserveInitialVictorySave();
			if (Self->Next && Now >= Self->NextAt)
			{
				TFunction<void()> Callback = MoveTemp(Self->Next);
				Callback();
			}
			return !Self->bFinished;
		}), 0.05f);
		LoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([Self = AsShared()](UWorld* NewWorld)
		{
			if (Self->bFinished || !NewWorld || NewWorld->WorldType != EWorldType::Game) { return; }
			Self->World = NewWorld;
			if (Self->bExpectTown) { Self->Later(4.0, [Self] { Self->VerifyTown(); }); }
			else if (Self->bExpectRun) { Self->Later(4.0, [Self] { Self->VerifyRun(); }); }
		});
		Later(4.0, [this] { Prepare(); });
	}

private:
	void Check(bool bOK, const TCHAR* Reason)
	{
		++Checks;
		Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("RUN_COMPLETION FAIL %s"), Reason); }
	}
	void Later(double Delay, TFunction<void()> Callback)
	{
		NextAt = FPlatformTime::Seconds() + Delay;
		Next = MoveTemp(Callback);
	}
	bool Refresh()
	{
		UGameInstance* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Persistence = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		Controller = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		Character = Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		RunState = World.IsValid() ? World->GetGameState<ARogue10mGameState>() : nullptr;
		ARogue10mGameMode* GameMode = World.IsValid() ? World->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
		Completion = GameMode ? GameMode->GetRunCompletionComponent() : nullptr;
		return Profiles && Persistence && Controller.IsValid() && Character.IsValid() && State && Inventory && RunState.IsValid() && Completion.IsValid();
	}
	void Prepare()
	{
		Check(World.IsValid() && UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub"), TEXT("actual_StartHub_initial_world"));
		Refresh();
		Check(Profiles && Persistence && Controller.IsValid(), TEXT("profile_subsystems_and_controller_present"));
		if (Failures) { Finish(); return; }
		if (bRead)
		{
			const FRogue10mCharacterProfile* Profile = Profiles->GetSelectedProfile();
			Check(Profile && Profile->CharacterName == TEXT("RunFinishQa"), TEXT("independent_process_selected_profile_restored"));
			if (!Profile) { Finish(); return; }
			ProfileId = Profile->ProfileId;
		}
		else
		{
			Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_slot"));
			FRogue10mCharacterProfile Profile;
			Check(!Failures && Profiles->CreateProfile(TEXT("RunFinishQa"), FRogue10mCharacterAppearance(), Profile)
				&& Profiles->SelectProfile(Profile.ProfileId), TEXT("isolated_profile_created_and_selected"));
			ProfileId = Profile.ProfileId;
		}
		if (Failures) { Finish(); return; }
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Check(Refresh() && Persistence->IsAttachedTo(*Character) && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == ProfileId, TEXT("selected_character_auto_attached_to_matching_profile"));
		if (Failures) { Finish(); return; }
		SaveSlot = Persistence->GetSaveSlotForProfile(ProfileId);
		Check(SaveSlot == SavePrefix + TEXT("_") + ProfileId.ToString(EGuidFormats::Digits), TEXT("progression_target_is_exact_QA_slot"));
		if (bRead)
		{
			TArray<uint8> Bytes;
			Check(FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Output, TEXT("expected.bin"))), TEXT("first_process_expected_snapshot_exists"));
			Expected.Reset(Bytes.Num() ? Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr);
			Check(Expected && Expected->ProfileId == ProfileId, TEXT("expected_snapshot_profile_matches_second_process"));
			if (Expected) { VerifySnapshots(); }
			Check(RunState->GetRunPhase() == ERogue10mRunPhase::WaitingToStart, TEXT("second_process_hub_waits_without_StartRun"));
			Finish(); return;
		}
		Inventory->AddGold(77);
		State->RecordSkillUse(TEXT("RunCompletionQaPreserved"), 3);
		SeedSkillTree();
		if (Failures) { Finish(); return; }
		Check(Persistence->SaveNow(), TEXT("initial_QA_progression_saved_before_portal"));
		Expected.Reset(NewObject<URogue10mProgressionSaveGame>());
		CaptureExpected();
		ARogue10mRunPortal* Portal = nullptr;
		for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It) { Portal = *It; break; }
		UBoxComponent* Box = Portal ? Portal->FindComponentByClass<UBoxComponent>() : nullptr;
		Check(Box != nullptr, TEXT("authored_StartHub_portal_collision_found"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(Box->GetComponentLocation() + FVector(500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		bExpectRun = true;
		Character->SetActorLocation(Box->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	void SeedSkillTree()
	{
		auto* Tree = State->GetSkillTreeProgressionComponent();
		ARogue10mTutorialNPC* ServiceNPC = nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { ServiceNPC = *It; break; }
		Check(Tree && ServiceNPC, TEXT("v3_skill_tree_and_authored_town_service_exist"));
		if (!Tree || !ServiceNPC) { return; }
		const auto Track = Tree->GetActiveTrack();
		Check(Track == ERogue10mSkillTreeTrack::Brawler || Track == ERogue10mSkillTreeTrack::GreatSword,
			TEXT("v3_fixture_uses_supported_authored_weapon_track"));
		if (Failures) { return; }
		const int32 BeforeMastery = Tree->GetMasteryTotalExperience(Track);
		for (int32 Count = 0; Count < 7; ++Count) { Tree->NotifyConfirmedAttack(Track); }
		Check(Tree->GetMasteryTotalExperience(Track) == BeforeMastery + 7, TEXT("v3_fixture_has_nonzero_mastery_from_public_notification"));
		// Give this disposable profile one earned point and buy through the actual NPC-range API.
		State->AddExperience(100);
		const FVector OriginalLocation = Character->GetActorLocation();
		Character->SetActorLocation(ServiceNPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Check(Tree->CanChangeInvestments() && Tree->TryUnlockNode(TEXT("I.FightingSpirit.Entry"), ERogue10mSkillTreeUnlockRoute::Points, NAME_None),
			TEXT("v3_fixture_purchases_identity_entry_with_actual_shared_point"));
		Check(Tree->GetSpentSkillTreePoints() == 1 && Tree->GetActiveIdentity() == ERogue10mSkillTreeIdentity::FightingSpirit,
			TEXT("v3_fixture_has_paid_ledger_and_active_identity"));
		Character->SetActorLocation(OriginalLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	void VerifyRun()
	{
		Check(UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("OpenRunMap"), TEXT("native_portal_loaded_actual_OpenRunMap"));
		Check(Refresh() && Persistence->IsSaveReady() && Persistence->GetAttachedProfileId() == ProfileId, TEXT("run_profile_restored_after_hard_travel"));
		if (Failures) { Finish(); return; }
		VerifySnapshots();
		Check(RunState->IsRunActive() && RunState->GetRemainingRunSeconds() > 0, TEXT("portal_StartRun_timer_active"));
		if (Failures) { Finish(); return; }
		if (Mode == TEXT("Timeout"))
		{
			// Exercise the real countdown without restarting the run or resetting its settlement.
			FFloatProperty* Remaining = FindFProperty<FFloatProperty>(RunState->GetClass(), TEXT("RemainingRunSeconds"));
			Check(Remaining != nullptr, TEXT("countdown_runtime_fixture_property_found"));
			if (!Remaining) { Finish(); return; }
			Remaining->SetPropertyValue_InContainer(RunState.Get(), 1.0f);
			Later(2.0, [this] { VerifyDefeat(); }); return;
		}
		if (Mode == TEXT("Death"))
		{
			const float Damage = Character->GetRogueAttributeSet()->GetMaxHealth() * 10.0f + 10000.0f;
			UGameplayStatics::ApplyDamage(Character.Get(), Damage, nullptr, nullptr, UDamageType::StaticClass());
			Later(0.5, [this] { VerifyDefeat(); }); return;
		}
		VerifyExcludedMonsters();
		if (Failures) { Finish(); return; }
		Character->SetCanBeDamaged(false); // Disposable navigation setup, restored before post-victory damage check.
		Character->SetActorLocation(FVector(15000, 69000, 10000), false, nullptr, ETeleportType::TeleportPhysics);
		BossDeadline = FPlatformTime::Seconds() + 90.0;
		Later(2.0, [this] { FindBoss(); });
	}
	void VerifyExcludedMonsters()
	{
		for (const bool bTestBoss : { false, true })
		{
			const FTransform Transform(FRotator::ZeroRotator, Character->GetActorLocation() + FVector(600, 0, 100));
			ARogue10mBasicMonster* Fixture = World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(), Transform,
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Check(Fixture != nullptr, TEXT("negative_monster_fixture_spawned"));
			if (!Fixture) { continue; }
			URogue10mMonsterDataAsset* Data = NewObject<URogue10mMonsterDataAsset>(Fixture);
			Data->MonsterId = bTestBoss ? TEXT("RunCompletionQaTestBoss") : TEXT("RunCompletionQaNormal");
			Data->MonsterRank = bTestBoss ? ERogue10mMonsterRank::FinalBoss : ERogue10mMonsterRank::Normal;
			Data->MaxHealth = 10; Data->ExperienceReward = 10; Data->bDestroyOnDeath = false;
			Data->HealthRegenerationPerSecond = 0; Data->StaminaRegenerationPerSecond = 0; Data->ManaRegenerationPerSecond = 0;
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Fixture->GetClass(), TEXT("MonsterData"));
			Check(Property != nullptr, TEXT("negative_fixture_authored_data_property_found"));
			if (!Property) { Fixture->Destroy(); continue; }
			Property->SetObjectPropertyValue_InContainer(Fixture, Data);
			if (bTestBoss) { Fixture->MarkCombatTestInstance(); }
			Fixture->AutoPossessAI = EAutoPossessAI::Disabled;
			UGameplayStatics::FinishSpawningActor(Fixture, Transform);
			const int32 BeforeGold = Inventory->GetGold();
			const auto BeforeGrowth = State->CaptureGrowth();
			UGameplayStatics::ApplyDamage(Fixture, 11, Controller.Get(), Character.Get(), UDamageType::StaticClass());
			Check(Fixture->IsDead() && RunState->IsRunActive() && !Completion->IsSettled(),
				bTestBoss ? TEXT("marked_final_boss_cannot_finish_run") : TEXT("ordinary_monster_cannot_finish_run"));
			if (bTestBoss)
			{
				Check(Inventory->GetGold() == BeforeGold && Same(BeforeGrowth, State->CaptureGrowth()), TEXT("test_boss_grants_no_gold_XP_or_kill_record"));
			}
			const auto OnceGrowth = State->CaptureGrowth();
			const int32 OnceGold = Inventory->GetGold();
			UGameplayStatics::ApplyDamage(Fixture, 100, Controller.Get(), Character.Get(), UDamageType::StaticClass());
			Check(Inventory->GetGold() == OnceGold && Same(OnceGrowth, State->CaptureGrowth()), TEXT("dead_monster_duplicate_damage_cannot_repeat_rewards"));
			Fixture->Destroy();
		}
	}
	void FindBoss()
	{
		for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
		{
			if (auto* AI = Cast<ARogue10mMonsterAIController>(It->GetController())) { AI->StopMonsterAI(); }
			if (It->GetMonsterData() && It->GetMonsterData()->MonsterRank == ERogue10mMonsterRank::FinalBoss
				&& !It->IsCombatTestInstance() && !It->IsDead()) { Boss = *It; }
		}
		if (!Boss.IsValid())
		{
			if (FPlatformTime::Seconds() < BossDeadline) { Later(0.5, [this] { FindBoss(); }); return; }
			Check(false, TEXT("authored_northern_boss_streamed_within_deadline")); Finish(); return;
		}
		Check(Boss->GetClass()->GetName().Contains(TEXT("BP_OW_FinalBoss"))
			&& Boss->GetMonsterData()->GetPathName().StartsWith(TEXT("/Game/")), TEXT("actual_authored_OpenRunMap_final_boss_and_DataAsset"));
		BossId = Boss->GetMonsterData()->MonsterId;
		ExpectedXP = Boss->GetMonsterData()->ExperienceReward;
		ExpectedGold = FMath::Max(1, ExpectedXP / 10);
		Check(!BossId.IsNone() && ExpectedXP > 0, TEXT("authored_boss_has_positive_reward_and_persistent_ID"));
		Results += FString::Printf(TEXT("boss=%s\nboss_data=%s\nboss_id=%s\n"), *Boss->GetPathName(), *Boss->GetMonsterData()->GetPathName(), *BossId.ToString());
		if (Failures) { Finish(); return; }
		const FVector BossLocation = Boss->GetActorLocation();
		const float Floor = BossLocation.Z - Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Position(BossLocation.X, BossLocation.Y - 140.0f, Floor + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f);
		Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->SetActorRotation(FRotator(0, 90, 0));
		Controller->SetControlRotation(FRotator(0, 90, 0));
		Later(2.0, [this] { PrepareFinalHit(); });
	}
	void PrepareFinalHit()
	{
		if (Character.IsValid() && Character->GetCharacterMovement()->IsFalling()
			&& FPlatformTime::Seconds() < BossDeadline)
		{
			Later(0.5, [this] { PrepareFinalHit(); }); return;
		}
		Check(Boss.IsValid() && !Boss->IsDead() && RunState->IsRunActive() && !Character->IsDead(), TEXT("live_authored_boss_and_player_before_final_hit"));
		Check(!Character->GetCharacterMovement()->IsFalling(), TEXT("player_grounded_in_actual_northern_ruins"));
		if (Failures) { Finish(); return; }
		BeforeBossGrowth = State->CaptureGrowth(); BeforeBossGold = Inventory->GetGold();
		UGameplayStatics::ApplyDamage(Boss.Get(), FMath::Max(0.0f, Boss->GetRogueAttributeSet()->GetHealth() - 1.0f),
			Controller.Get(), Character.Get(), UDamageType::StaticClass());
		if (auto* AI = Cast<ARogue10mMonsterAIController>(Boss->GetController())) { AI->StopMonsterAI(); }
		if (Boss->GetAttackPatternComponent()) { Boss->GetAttackPatternComponent()->CancelAttack(); }
		Check(!Boss->IsDead() && Boss->GetRogueAttributeSet()->GetHealth() > 0, TEXT("controlled_nonlethal_damage_leaves_real_final_hit_to_player_combat"));
		Check(Persistence->SaveNow(), TEXT("prekill_progression_saved_before_write_failure_fixture"));
		if (bFailure && !LockSaveFile()) { Finish(); return; }
		const auto* Tree = State->GetSkillTreeProgressionComponent();
		BossMasteryTrack = Tree->GetActiveTrack();
		BeforeBossMastery = Tree->GetMasteryTotalExperience(BossMasteryTrack);
		bWatchInitialVictorySave = !bFailure;
		// The actual accepted player attack, montage timing and damage query deliver the final hit.
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
		HitDeadline = FPlatformTime::Seconds() + 5.0;
		Later(0.15, [this]
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
			Later(0.35, [this] { WaitFinalHit(); });
		});
	}
	void ObserveInitialVictorySave()
	{
		if (!bWatchInitialVictorySave || !Completion.IsValid() || !Controller.IsValid()
			|| !Completion->IsProgressionSaved() || !Controller->IsRunResultVisible()) { return; }
		bWatchInitialVictorySave = false;
		bInitialVictorySaveObserved = true;
		// Core ticker observes the first saved result before the later screenshot/return steps.
		// Do not call SaveNow here: that would hide the final-hit/settlement ordering regression.
		VerifyDiskSnapshot(TEXT("initial_victory_before_autosave_or_return"));
	}
	void WaitFinalHit()
	{
		if (RunState->IsRunActive() && FPlatformTime::Seconds() < HitDeadline)
		{
			Later(0.2, [this] { WaitFinalHit(); }); return;
		}
		Check(RunState->GetRunPhase() == ERogue10mRunPhase::Victory, TEXT("real_player_attack_final_hit_transitions_run_to_Victory"));
		Check(Completion->IsSettled() && Completion->IsVictory(), TEXT("native_completion_settles_victory_once"));
		Check(bFailure || bInitialVictorySaveObserved, TEXT("first_victory_save_observed_before_return_or_explicit_resave"));
		Check(State->GetSkillTreeProgressionComponent()->GetMasteryTotalExperience(BossMasteryTrack) == BeforeBossMastery + 1,
			TEXT("actual_final_player_attack_grants_exactly_one_mastery_XP"));
		Check(Completion->GetRewardExperience() == ExpectedXP && Completion->GetRewardGold() == ExpectedGold, TEXT("result_reports_existing_authored_boss_rewards"));
		auto ExpectedGrowth = BeforeBossGrowth;
		ExpectedGrowth.Experience += static_cast<float>(ExpectedXP);
		while (ExpectedGrowth.Experience >= ExpectedGrowth.ExperienceToNextLevel)
		{
			ExpectedGrowth.Experience -= ExpectedGrowth.ExperienceToNextLevel; ++ExpectedGrowth.Level;
			ExpectedGrowth.ExperienceToNextLevel = FMath::Max(100.0f, ExpectedGrowth.ExperienceToNextLevel * 1.18f);
		}
		Check(State->GetPlayerLevel() == ExpectedGrowth.Level && FMath::IsNearlyEqual(State->GetCurrentExperience(), ExpectedGrowth.Experience)
			&& FMath::IsNearlyEqual(State->GetExperienceToNextLevel(), ExpectedGrowth.ExperienceToNextLevel), TEXT("actual_boss_XP_delta_and_level_crossings_exactly_once"));
		Check(Inventory->GetGold() == BeforeBossGold + ExpectedGold
			&& State->GetMonsterDefeatCount(BossId) == BeforeBossGrowth.MonsterDefeats.FindRef(BossId) + 1, TEXT("actual_boss_gold_and_kill_record_awarded_once"));
		if (Failures) { Finish(); return; }
		const auto OnceGrowth = State->CaptureGrowth(); const int32 OnceGold = Inventory->GetGold();
		const auto OnceTree = State->GetSkillTreeProgressionComponent()->Capture();
		if (Boss.IsValid())
		{
			UGameplayStatics::ApplyDamage(Boss.Get(), 99999, Controller.Get(), Character.Get(), UDamageType::StaticClass());
			Completion->NotifyFinalBossDefeated(*Boss, Controller.Get(), ExpectedXP, ExpectedGold);
		}
		RunState->FinishRun(false); RunState->StartRun(); RunState->StartRunWithDuration(1);
		Check(RunState->GetRunPhase() == ERogue10mRunPhase::Victory && !RunState->IsRunActive(), TEXT("terminal_Victory_cannot_flip_or_restart"));
		Character->SetCanBeDamaged(true);
		const float HP = Character->GetRogueAttributeSet()->GetHealth();
		UGameplayStatics::ApplyDamage(Character.Get(), HP + 99999, nullptr, nullptr, UDamageType::StaticClass());
		Check(!Character->IsDead() && FMath::IsNearlyEqual(HP, Character->GetRogueAttributeSet()->GetHealth()), TEXT("postvictory_lethal_damage_cannot_kill_player"));
		Check(OnceGold == Inventory->GetGold() && Same(OnceGrowth, State->CaptureGrowth())
			&& Same(OnceTree, State->GetSkillTreeProgressionComponent()->Capture()), TEXT("duplicate_boss_notification_and_terminal_calls_do_not_repeat_rewards"));
		Check(Controller->IsRunResultVisible() && Controller->GetRunResultWidget() && Controller->IsAnyBlockingWindowVisible(), TEXT("actual_native_result_widget_visible_and_blocks_gameplay"));
		Check(Completion->IsProgressionSaved() != bFailure, TEXT("settlement_save_status_matches_real_platform_write_outcome"));
		if (bFailure)
		{
			Check(!Completion->ReturnToTown() && !Completion->IsTravelStarted() && World.IsValid()
				&& UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("OpenRunMap"), TEXT("blocked_disk_write_prevents_ReturnToTown_and_keeps_result"));
			Check(!Completion->GetSaveStatus().IsEmpty() && Persistence->CanRetrySave(), TEXT("failed_save_has_visible_reason_and_valid_retry_session"));
			Check(OnceGold == Inventory->GetGold() && Same(OnceGrowth, State->CaptureGrowth())
				&& Same(OnceTree, State->GetSkillTreeProgressionComponent()->Capture()), TEXT("failed_return_retry_preserves_exactly_once_rewards"));
		}
		CaptureExpected();
		TArray<uint8> Bytes;
		Check(UGameplayStatics::SaveGameToMemory(Expected.Get(), Bytes)
			&& FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Output, TEXT("expected.bin"))), TEXT("expected_postboss_snapshot_preserved_outside_Saved"));
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output, TEXT("run-result.png")), true, false);
		Later(2.0, [this] { ReturnViaEnter(); });
	}
	void VerifyDefeat()
	{
		Check(RunState->GetRunPhase() == ERogue10mRunPhase::Defeat && Character->IsDead(), TEXT("actual_timeout_or_lethal_damage_ends_run_in_Defeat"));
		Check(Completion->IsSettled() && !Completion->IsVictory() && Completion->IsProgressionSaved(), TEXT("Defeat_settles_and_saves_without_victory_reward"));
		Check(Completion->GetRewardExperience() == 0 && Completion->GetRewardGold() == 0, TEXT("Defeat_reports_no_boss_completion_reward"));
		RunState->FinishRun(true); RunState->StartRunWithDuration(1);
		Check(RunState->GetRunPhase() == ERogue10mRunPhase::Defeat, TEXT("terminal_Death_or_timeout_cannot_flip_to_Victory"));
		Check(Controller->IsRunResultVisible() && Controller->GetRunResultWidget(), TEXT("Defeat_native_result_visible_even_when_Pawn_input_disabled"));
		CaptureExpected();
		VerifyDiskSnapshot(TEXT("defeat_settlement"));
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output, TEXT("run-result.png")), true, false);
		Later(2.0, [this] { ReturnViaEnter(); });
	}
	void ReturnViaEnter()
	{
		Check(IFileManager::Get().FileSize(*FPaths::Combine(Output, TEXT("run-result.png"))) > 24, TEXT("actual_rendered_result_screenshot_written"));
		UnlockSaveFile();
		if (Failures) { Finish(); return; }
		URogue10mRunResultWidget* Widget = Controller->GetRunResultWidget();
		Check(Widget && FSlateApplication::IsInitialized(), TEXT("result_widget_and_Slate_input_available"));
		if (!Widget || !FSlateApplication::IsInitialized()) { Finish(); return; }
		Widget->SetKeyboardFocus();
		bExpectTown = true; bExpectRun = false;
		// Route through Slate focus and the widget's native key handler, not ReturnToTown directly.
		FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
		FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
		Check(Completion->IsTravelStarted() && Completion->IsProgressionSaved(), TEXT("focused_result_Enter_saves_and_requests_town_travel"));
		VerifyDiskSnapshot(TEXT("result_Enter_saved_before_hard_travel"));
	}
	void VerifyTown()
	{
		Check(UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub"), TEXT("result_Enter_loaded_actual_StartHub"));
		Check(Refresh() && Persistence->IsSaveReady() && Persistence->GetAttachedProfileId() == ProfileId, TEXT("town_return_restored_same_character_profile"));
		if (Failures) { Finish(); return; }
		Check(RunState->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && !World->URL.HasOption(TEXT("StartRun")), TEXT("town_return_clears_StartRun_option_and_timer"));
		Check(!Character->IsDead() && !Controller->IsRunResultVisible(), TEXT("town_return_restores_alive_character_without_stale_result_panel"));
		Check(Character->GetTraitRuntimeComponent() && !Character->GetTraitRuntimeComponent()->HasUsedLastStand(), TEXT("fresh_town_Pawn_resets_transient_LastStand_usage"));
		VerifySnapshots();
		Finish();
	}
	void CaptureExpected()
	{
		Expected->ProfileId = ProfileId;
		Expected->SaveVersion = URogue10mProgressionSaveGame::CurrentVersion;
		Expected->Growth = State->CaptureGrowth(); Expected->Inventory = Inventory->CaptureInventory();
		Expected->Combat = Character->GetCombatComponent()->CaptureCombat();
		Expected->Town = State->GetTownProgressionComponent()->CaptureProgression();
		Expected->SkillTree = State->GetSkillTreeProgressionComponent()->Capture();
	}
	void VerifySnapshots()
	{
		Check(Expected && Same(Expected->Growth, State->CaptureGrowth()), TEXT("full_growth_XP_and_monster_kills_restored"));
		Check(Expected && Same(Expected->Inventory, Inventory->CaptureInventory()), TEXT("full_inventory_gold_and_item_GUIDs_restored"));
		const FRogue10mCombatSnapshot ActualCombat = Character->GetCombatComponent()->CaptureCombat();
		const bool bCombatMatches = Expected && Same(Expected->Combat, ActualCombat);
		Check(bCombatMatches, TEXT("full_skill_unlocks_and_bindings_restored"));
		if (!bCombatMatches)
		{
			auto DumpCombat = [this](const TCHAR* Label, const FRogue10mCombatSnapshot& Snapshot)
			{
				FString Details = FString::Printf(TEXT("combat_%s unlocked=%d bindings=%d\n"), Label, Snapshot.UnlockedSkills.Num(), Snapshot.SkillBindings.Num());
				for (const auto& Skill : Snapshot.UnlockedSkills)
				{
					Details += FString::Printf(TEXT("combat_%s unlocked=%s\n"), Label, *Skill.ToSoftObjectPath().ToString());
				}
				for (const auto& Pair : Snapshot.SkillBindings)
				{
					Details += FString::Printf(TEXT("combat_%s slot=%d skill=%s\n"), Label, static_cast<int32>(Pair.Key), *Pair.Value.ToSoftObjectPath().ToString());
				}
				Results += Details;
				UE_LOG(LogRogue10m, Error, TEXT("RUN_COMPLETION_DIAGNOSTIC %s"), *Details);
			};
			if (Expected) { DumpCombat(TEXT("expected"), Expected->Combat); }
			DumpCombat(TEXT("actual"), ActualCombat);
		}
		Check(Expected && Same(Expected->Town, State->GetTownProgressionComponent()->CaptureProgression()), TEXT("full_traits_and_difficulty_restored"));
		Check(Expected && Expected->SaveVersion == URogue10mProgressionSaveGame::CurrentVersion
			&& Same(Expected->SkillTree, State->GetSkillTreeProgressionComponent()->Capture()), TEXT("full_v3_mastery_investment_choice_and_active_identity_ledger_restored"));
		VerifyDiskSnapshot(bRead ? TEXT("fresh_process_read") : TEXT("restored_world"));
	}
	void VerifyDiskSnapshot(const TCHAR* Stage)
	{
		const auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		Check(Saved && Saved->ProfileId == ProfileId && Saved->SaveVersion == URogue10mProgressionSaveGame::CurrentVersion,
			*FString::Printf(TEXT("%s_disk_profile_and_current_save_version_match"), Stage));
		if (!Saved) { return; }
		Check(Same(Saved->Growth, State->CaptureGrowth()) && Same(Saved->Inventory, Inventory->CaptureInventory())
			&& Same(Saved->Combat, Character->GetCombatComponent()->CaptureCombat())
			&& Same(Saved->Town, State->GetTownProgressionComponent()->CaptureProgression()),
			*FString::Printf(TEXT("%s_disk_growth_inventory_combat_and_town_match_memory"), Stage));
		Check(Same(Saved->SkillTree, State->GetSkillTreeProgressionComponent()->Capture()),
			*FString::Printf(TEXT("%s_disk_complete_SkillTree_ledger_matches_memory"), Stage));
	}
	bool LockSaveFile()
	{
		FString SaveRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")));
		FString SaveFile = FPaths::ConvertRelativePathToFull(FPaths::Combine(SaveRoot, SaveSlot + TEXT(".sav")));
		FPaths::NormalizeDirectoryName(SaveRoot); FPaths::NormalizeFilename(SaveFile);
		Check(SaveSlot.StartsWith(SavePrefix + TEXT("_")) && FPaths::IsUnderDirectory(SaveFile, SaveRoot)
			&& FPaths::GetCleanFilename(SaveFile) == SaveSlot + TEXT(".sav"), TEXT("write_failure_lock_targets_exact_isolated_QA_save"));
		if (Failures) { return false; }
#if PLATFORM_WINDOWS
		// A read-only sharing handle blocks the actual platform save without editing any Saved bytes.
		SaveLock = ::CreateFileW(*SaveFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Check(SaveLock != INVALID_HANDLE_VALUE, TEXT("existing_QA_save_read_lock_acquired_without_file_mutation"));
		return SaveLock != INVALID_HANDLE_VALUE;
#else
		Check(false, TEXT("Windows_read_share_lock_required_for_real_write_failure_case")); return false;
#endif
	}
	void UnlockSaveFile()
	{
#if PLATFORM_WINDOWS
		if (SaveLock != INVALID_HANDLE_VALUE) { ::CloseHandle(SaveLock); SaveLock = INVALID_HANDLE_VALUE; }
#endif
	}
	void Finish()
	{
		if (bFinished) { return; }
		bFinished = true; UnlockSaveFile(); Next = nullptr;
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadHandle);
		Results += FString::Printf(TEXT("mode=%s\nchecks=%d\nfailures=%d\nprofile=%s\n"), *Mode, Checks, Failures, *ProfileId.ToString());
		if (!Output.IsEmpty() && !FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, Mode.ToLower() + TEXT("-results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=RUN_COMPLETION_%s mode=%s checks=%d failures=%d output=%s"),
			Failures ? TEXT("FAILED") : TEXT("PASSED"), *Mode.ToLower(), Checks, Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	TWeakObjectPtr<URogue10mRunCompletionComponent> Completion;
	TWeakObjectPtr<ARogue10mBasicMonster> Boss;
	URogue10mCharacterProfileSubsystem* Profiles = nullptr;
	URogue10mProgressionSubsystem* Persistence = nullptr;
	ARogue10mPlayerState* State = nullptr;
	URogue10mInventoryComponent* Inventory = nullptr;
	TStrongObjectPtr<URogue10mProgressionSaveGame> Expected;
	FRogue10mGrowthSnapshot BeforeBossGrowth;
	ERogue10mSkillTreeTrack BossMasteryTrack = ERogue10mSkillTreeTrack::None;
	FGuid ProfileId;
	FName BossId;
	FString Mode, ProfileSlot, SavePrefix, SaveSlot, Output, Results;
	FDelegateHandle LoadHandle;
	FTSTicker::FDelegateHandle Watchdog;
	TFunction<void()> Next;
	double NextAt = 0, Deadline = 0, BossDeadline = 0, HitDeadline = 0;
	int32 Checks = 0, Failures = 0, BeforeBossGold = 0, ExpectedXP = 0, ExpectedGold = 0;
	int32 BeforeBossMastery = 0;
	bool bRead = false, bFailure = false, bFinished = false, bExpectRun = false, bExpectTown = false;
	bool bWatchInitialVictorySave = false, bInitialVictorySaveObserved = false;
#if PLATFORM_WINDOWS
	HANDLE SaveLock = INVALID_HANDLE_VALUE;
#endif
};

static TSharedPtr<FRun> Active;
static void Run(const FString& Mode)
{
	if (!GEngine || GIsEditor) { return; }
	if (Active) { UE_LOG(LogRogue10m, Error, TEXT("Run completion QA requires a fresh game process.")); return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>(); Active->Start(*World, Mode); return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=RUN_COMPLETION_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}
static FAutoConsoleCommand Write(TEXT("Rogue10m.TestRunCompletionWrite"), TEXT("Real portal, authored boss final player hit, settlement, result Enter and hub with isolated saves."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("Write")); }));
static FAutoConsoleCommand Read(TEXT("Rogue10m.TestRunCompletionRead"), TEXT("Reads the successful run's complete snapshot in a fresh process."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("Read")); }));
static FAutoConsoleCommand Failure(TEXT("Rogue10m.TestRunCompletionFailure"), TEXT("Real QA save sharing failure, blocked return, retry without duplicate rewards and hub."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("Failure")); }));
static FAutoConsoleCommand FailureRead(TEXT("Rogue10m.TestRunCompletionFailureRead"), TEXT("Reads the failed-write/retry run's snapshot in a fresh process."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("FailureRead")); }));
static FAutoConsoleCommand Timeout(TEXT("Rogue10m.TestRunCompletionTimeout"), TEXT("Real timer expiry, terminal precedence, result input and hub."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("Timeout")); }));
static FAutoConsoleCommand Death(TEXT("Rogue10m.TestRunCompletionDeath"), TEXT("Real player lethal damage, terminal precedence, result input and hub."), FConsoleCommandDelegate::CreateLambda([] { Run(TEXT("Death")); }));
}
#endif
