// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace Rogue10mTraitPersistenceTest
{
template <typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool IsBoundedSlot(const FString& Slot, const TCHAR* Prefix, int32 MaximumLength = 80)
{
	if (!Slot.StartsWith(Prefix) || Slot.Len() > MaximumLength) { return false; }
	for (TCHAR C : Slot) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld, bool bInRead)
	{
		World = &InWorld;
		bRead = bInRead;
		FString ProfileSlot, ProgressionPrefix, SkillSlot;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), SkillSlot);
		if (!IsBoundedSlot(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
			|| !IsBoundedSlot(ProgressionPrefix, TEXT("Rogue10m_Progression_QA_"))
			|| !IsBoundedSlot(SkillSlot, TEXT("Rogue10m_Fighter_QA_"), 100)
			|| InWorld.GetNetMode() != NM_Standalone)
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=TRAIT_PERSISTENCE_FAILED reason=missing_bounded_QA_save_overrides"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		ExpectedPrefix = ProgressionPrefix + TEXT("_");
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/trait16-runtime-20261011"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		InWorld.GetTimerManager().SetTimer(PrepareTimer,
			FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 3.0f, false);
	}

private:
	void Check(bool bPassed, const FString& Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
		if (!bPassed) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("TRAIT_PERSISTENCE FAIL %s"), *Reason); }
	}

	void CapturePointers()
	{
		Character = Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
	}

	void Prepare()
	{
		UGameInstance* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Persistence = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		Controller = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		Check(Profiles && Persistence && Controller.IsValid(), TEXT("profile_subsystems_and_controller_exist"));
		if (Failures) { Finish(); return; }
		if (!bRead)
		{
			Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_slot"));
			FRogue10mCharacterProfile Profile;
			if (Failures || !Profiles->CreateProfile(TEXT("TraitQaHero"), FRogue10mCharacterAppearance(), Profile))
			{
				Check(false, TEXT("isolated_profile_created")); Finish(); return;
			}
			ProfileId = Profile.ProfileId;
			Check(Profiles->SelectProfile(ProfileId), TEXT("isolated_profile_selected"));
		}
		else
		{
			const auto* Profile = Profiles->GetSelectedProfile();
			Check(Profile && Profile->CharacterName == TEXT("TraitQaHero"), TEXT("selected_profile_survives_independent_process"));
			if (!Profile) { Finish(); return; }
			ProfileId = Profile->ProfileId;
		}
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		CapturePointers();
		Check(Character.IsValid() && State && Inventory && Combat && Town && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == ProfileId, TEXT("selected_character_and_all_domains_attached"));
		if (Failures) { Finish(); return; }
		SaveSlot = Persistence->GetSaveSlotForProfile(ProfileId);
		SaveFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), SaveSlot + TEXT(".sav"));
		Check(SaveSlot.StartsWith(ExpectedPrefix), TEXT("target_progression_slot_is_QA_isolated"));
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(NPC.IsValid(), TEXT("actual_start_hub_NPC_exists"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		if (bRead) { Read(); } else { Write(); }
	}

	void Write()
	{
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth();
		Growth.Level = 12;
		Growth.Experience = 17.0f;
		Check(State->RestoreGrowth(Growth), TEXT("legacy_fixture_has_eleven_earned_trait_points"));
		Inventory->AddGold(1234);
		State->RecordSkillUse(TEXT("TraitQaSeparateSkill"), 17);
		State->RecordMonsterDefeat(TEXT("TraitQaSeparateMonster"), 9);
		Persistence->RecordClaimedReward(TEXT("TraitQaSeparateReceipt"));
		Check(Persistence->SaveNow(), TEXT("legacy_fixture_other_domains_saved"));
		if (!VerifyRealWriteFailureRecovery()) { Finish(); return; }
		const auto BeforeGrowth = State->CaptureGrowth();
		const auto BeforeInventory = Inventory->CaptureInventory();
		const auto BeforeCombat = Combat->CaptureCombat();
		Persistence->DetachCharacter(*Character, false);
		auto* Legacy = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		Check(Legacy != nullptr, TEXT("legacy_fixture_baseline_deserialized"));
		if (!Legacy) { Finish(); return; }
		Legacy->SaveVersion = 1;
		Legacy->Town.TraitInvestments.Reset();
		Legacy->Town.VitalityLevel = 2;
		Legacy->Town.PowerLevel = 1;
		Legacy->Town.TraitMaxHealthBonus = 27.0f;
		Legacy->Town.TraitAttackBonus = 3.75f;
		Legacy->Town.Difficulty = ERogue10mDifficulty::Hard;
		Legacy->Town.DifficultyModifiers.HealthMultiplier = 1.37f;
		Legacy->Town.DifficultyModifiers.DamageMultiplier = 1.41f;
		Legacy->Town.DifficultyModifiers.ExperienceMultiplier = 1.23f;
		Check(UGameplayStatics::SaveGameToSlot(Legacy, SaveSlot, 0), TEXT("isolated_version_one_custom_tuning_fixture_written"));
		Check(Persistence->AttachCharacter(*Character) && Persistence->IsSaveReady(), TEXT("version_one_automatically_migrates_after_successful_restore"));
		Check(Town->GetTraitLevel(ERogue10mTownTrait::Vitality) == 2 && Town->GetTraitLevel(ERogue10mTownTrait::Power) == 1
			&& Town->GetSpentTraitPoints() == 3 && Town->GetAvailableTraitPoints() == 8,
			TEXT("migration_keeps_legacy_ranks_and_refundable_point_budget"));
		Check(Town->GetTraitBonus(ERogue10mTownTrait::Vitality) == 27.0f
			&& Town->GetTraitBonus(ERogue10mTownTrait::Power) == 3.75f
			&& Same(Legacy->Town.DifficultyModifiers, Town->GetDifficultyModifiers())
			&& Town->GetDifficulty() == ERogue10mDifficulty::Hard, TEXT("migration_keeps_authored_legacy_bonuses_and_difficulty"));
		Check(Same(BeforeGrowth, State->CaptureGrowth()) && Same(BeforeInventory, Inventory->CaptureInventory())
			&& Same(BeforeCombat, Combat->CaptureCombat()) && Persistence->HasClaimedReward(TEXT("TraitQaSeparateReceipt")),
			TEXT("migration_does_not_change_separate_growth_inventory_skills_or_receipts"));
		auto* Migrated = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		Check(Migrated && Migrated->SaveVersion == 2 && Migrated->Town.TraitInvestments.Num() == 2,
			TEXT("successful_migration_writes_version_two_canonical_records"));
		if (Failures) { Finish(); return; }
		Check(NPC->BeginDialogue(Character.Get()), TEXT("actual_NPC_dialogue_opens_for_trait_investment"));
		const int32 PointsBeforeLock = Town->GetAvailableTraitPoints();
		Check(!NPC->UpgradeTrait(ERogue10mTownTrait::LastStand) && Town->GetAvailableTraitPoints() == PointsBeforeLock,
			TEXT("last_stand_is_locked_before_level_twenty_without_point_loss"));
		Growth = State->CaptureGrowth();
		Growth.Level = 53;
		Check(State->RestoreGrowth(Growth), TEXT("full_catalog_fixture_has_fifty_two_earned_points"));
		const auto GrowthBeforeTraits = State->CaptureGrowth();
		const auto InventoryBeforeTraits = Inventory->CaptureInventory();
		const auto CombatBeforeTraits = Combat->CaptureCombat();
		for (uint8 Value = 0; Value < 16; ++Value)
		{
			const auto Trait = static_cast<ERogue10mTownTrait>(Value);
			const int32 MaximumRank = Value < 2 ? 5 : (Trait == ERogue10mTownTrait::LastStand ? 1 : 3);
			for (int32 Rank = Town->GetTraitLevel(Trait); Rank < MaximumRank; ++Rank)
			{
				Check(NPC->UpgradeTrait(Trait), FString::Printf(TEXT("NPC_upgrades_trait_%u_rank_%d"), Value, Rank + 1));
			}
			Check(Town->GetTraitLevel(Trait) == MaximumRank, FString::Printf(TEXT("trait_%u_reaches_rank_cap"), Value));
			const int32 Points = Town->GetAvailableTraitPoints();
			Check(!NPC->UpgradeTrait(Trait) && Points == Town->GetAvailableTraitPoints(), FString::Printf(TEXT("trait_%u_cap_rejects_without_charge"), Value));
		}
		Check(Town->GetSpentTraitPoints() == 52 && Town->GetAvailableTraitPoints() == 0
			&& Town->CaptureProgression().TraitInvestments.Num() == 16, TEXT("all_sixteen_traits_spend_exactly_fifty_two_points"));
		Check(Same(GrowthBeforeTraits, State->CaptureGrowth()) && Same(InventoryBeforeTraits, Inventory->CaptureInventory())
			&& Same(CombatBeforeTraits, Combat->CaptureCombat()), TEXT("NPC_trait_upgrades_leave_separate_growth_inventory_and_skills_unchanged"));
		const auto FullyInvested = Town->CaptureProgression();
		Check(Town->TryResetTraits() && Town->GetSpentTraitPoints() == 0 && Town->GetAvailableTraitPoints() == 52,
			TEXT("free_redistribution_refunds_every_spent_point"));
		Check(Same(GrowthBeforeTraits, State->CaptureGrowth()) && Same(InventoryBeforeTraits, Inventory->CaptureInventory())
			&& Same(CombatBeforeTraits, Combat->CaptureCombat()) && Town->GetDifficulty() == ERogue10mDifficulty::Hard,
			TEXT("redistribution_only_changes_traits"));
		Check(Town->RestoreProgression(FullyInvested), TEXT("full_catalog_restored_after_refund_verification"));
		VerifyAuthoredCostRoundtrip(FullyInvested, GrowthBeforeTraits);
		for (const FName RuntimeField : { FName(TEXT("ComboHitCount")), FName(TEXT("bLastStandUsed")), FName(TEXT("ComboBuffExpiresAt")),
			FName(TEXT("DodgeCounterExpiresAt")), FName(TEXT("NextKillRecoveryAt")), FName(TEXT("GoldBonusRemainder")) })
		{
			Check(FindFProperty<FProperty>(FRogue10mTownProgressionSnapshot::StaticStruct(), RuntimeField) == nullptr,
				TEXT("snapshot_excludes_transient_run_state_") + RuntimeField.ToString());
		}
		NPC->CloseDialogue();
		Check(Persistence->SaveNow(), TEXT("full_catalog_version_two_save_succeeds"));
		auto* Expected = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		TArray<uint8> ExpectedBytes;
		Check(Expected && UGameplayStatics::SaveGameToMemory(Expected, ExpectedBytes)
			&& FFileHelper::SaveArrayToFile(ExpectedBytes, *FPaths::Combine(Output, TEXT("expected.bin"))),
			TEXT("independent_process_full_catalog_expected_snapshot_written"));
		Finish();
	}

	bool VerifyRealWriteFailureRecovery()
	{
		FString ResolvedFile = FPaths::ConvertRelativePathToFull(SaveFile);
		FString ResolvedSaveRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")));
		FPaths::NormalizeFilename(ResolvedFile);
		FPaths::NormalizeDirectoryName(ResolvedSaveRoot);
		const FString BackupFile = ResolvedFile + TEXT(".traitqa-write-backup");
		IFileManager& Files = IFileManager::Get();
		const bool bSafePath = SaveSlot.StartsWith(ExpectedPrefix)
			&& FPaths::IsUnderDirectory(ResolvedFile, ResolvedSaveRoot)
			&& FPaths::GetCleanFilename(ResolvedFile) == SaveSlot + TEXT(".sav")
			&& !Files.FileExists(*BackupFile) && !Files.DirectoryExists(*BackupFile);
		Check(bSafePath, TEXT("write_failure_target_is_exact_isolated_QA_save_path"));
		if (!bSafePath) { return false; }
		TArray<uint8> OriginalBytes;
		Check(FFileHelper::LoadFileToArray(OriginalBytes, *ResolvedFile) && !OriginalBytes.IsEmpty()
			&& Persistence->IsSaveReady() && Persistence->CanRetrySave(), TEXT("valid_attached_session_can_retry_before_write_failure"));
		if (OriginalBytes.IsEmpty() || !Persistence->CanRetrySave()) { return false; }
		// Preserve the existing valid QA file, then use an empty directory at the exact file
		// path to make the real platform save call fail. No ordinary user slot is accessed.
		const bool bBackedUp = Files.Move(*BackupFile, *ResolvedFile, false, false, false, true);
		Check(bBackedUp, TEXT("valid_QA_file_preserved_before_real_write_path_block"));
		if (!bBackedUp) { return false; }
		const bool bBlocked = Files.MakeDirectory(*ResolvedFile, false);
		Check(bBlocked, TEXT("isolated_QA_save_filename_is_blocked_by_empty_directory"));
		if (bBlocked)
		{
			Check(!Persistence->SaveNow() && !Persistence->IsSaveReady() && Persistence->CanRetrySave()
				&& !Persistence->GetLastError().IsEmpty(), TEXT("real_disk_write_failure_is_unready_but_safe_to_retry"));
			TArray<uint8> PreservedBytes;
			Check(FFileHelper::LoadFileToArray(PreservedBytes, *BackupFile) && PreservedBytes == OriginalBytes,
				TEXT("real_write_failure_preserves_valid_baseline_bytes"));
		}
		// Never delete recursively: this fixture owns only the empty blocking directory.
		const bool bUnblocked = !Files.DirectoryExists(*ResolvedFile) || Files.DeleteDirectory(*ResolvedFile, true, false);
		Check(bUnblocked, TEXT("only_empty_QA_blocking_directory_is_removed"));
		const bool bRestoredFile = bUnblocked && Files.Move(*ResolvedFile, *BackupFile, false, false, false, true);
		Check(bRestoredFile, TEXT("original_valid_QA_file_restored_without_overwrite"));
		if (!bBlocked || !bRestoredFile) { return false; }
		Check(Persistence->CanRetrySave() && Persistence->SaveNow() && Persistence->IsSaveReady()
			&& Persistence->GetLastError().IsEmpty(), TEXT("restored_write_path_allows_safe_retry_to_clear_error_and_resume_save_readiness"));
		return Failures == 0;
	}

	void VerifyAuthoredCostRoundtrip(const FRogue10mTownProgressionSnapshot& OriginalTown,
		const FRogue10mGrowthSnapshot& OriginalGrowth)
	{
		auto CustomTown = OriginalTown;
		auto* Investment = CustomTown.TraitInvestments.FindByPredicate([](const auto& Row)
		{
			return Row.Trait == ERogue10mTownTrait::CriticalChance;
		});
		Check(Investment != nullptr, TEXT("authored_cost_fixture_has_critical_chance_investment"));
		if (!Investment) { return; }
		// A valid old authored catalog may have charged two points per rank. Current defaults
		// must not retroactively reject or reprice the frozen investment and its actual refund.
		Investment->SpentPoints = Investment->Rank * 2;
		Investment->Bonus = .075f;
		auto CustomGrowth = OriginalGrowth;
		CustomGrowth.Level = 56;
		Check(URogue10mTownProgressionComponent::ValidateSnapshot(CustomTown, 55)
			&& State->RestoreGrowth(CustomGrowth) && Town->RestoreProgression(CustomTown)
			&& Town->GetSpentTraitPoints() == 55 && Town->GetAvailableTraitPoints() == 0,
			TEXT("old_authored_cost_and_bonus_are_accepted_without_current_default_repricing"));
		NPC->CloseDialogue();
		Check(Persistence->SaveNow(), TEXT("authored_cost_fixture_saves_as_version_two"));
		Persistence->DetachCharacter(*Character, false);
		Check(Persistence->AttachCharacter(*Character) && Persistence->IsSaveReady()
			&& Same(CustomTown, Town->CaptureProgression()) && Town->GetSpentTraitPoints() == 55,
			TEXT("version_two_reload_preserves_authored_cost_and_frozen_bonus"));
		Check(Town->TryResetTraits() && Town->GetAvailableTraitPoints() == 55,
			TEXT("redistribution_refunds_actual_authored_cost_instead_of_default_cost"));
		Check(Town->RestoreProgression(OriginalTown) && State->RestoreGrowth(OriginalGrowth)
			&& Town->GetSpentTraitPoints() == 52 && Town->GetAvailableTraitPoints() == 0,
			TEXT("fifty_two_point_fixture_restored_after_authored_cost_verification"));
	}

	void Read()
	{
		TArray<uint8> ExpectedBytes;
		Check(FFileHelper::LoadFileToArray(ExpectedBytes, *FPaths::Combine(Output, TEXT("expected.bin"))), TEXT("first_process_expected_snapshot_exists"));
		auto* Expected = ExpectedBytes.Num() ? Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(ExpectedBytes)) : nullptr;
		Check(Expected && Expected->SaveVersion == 2 && Expected->ProfileId == ProfileId, TEXT("expected_version_two_profile_identity_matches"));
		if (!Expected) { Finish(); return; }
		Check(Same(Expected->Town, Town->CaptureProgression()) && Town->GetSpentTraitPoints() == 52
			&& Town->GetAvailableTraitPoints() == 0, TEXT("independent_process_restores_all_sixteen_traits_ranks_bonuses_and_budget"));
		Check(Same(Expected->Growth, State->CaptureGrowth()) && Same(Expected->Inventory, Inventory->CaptureInventory())
			&& Same(Expected->Combat, Combat->CaptureCombat()) && Persistence->HasClaimedReward(TEXT("TraitQaSeparateReceipt")),
			TEXT("independent_process_keeps_separate_growth_inventory_skills_and_receipts"));
		Check(Town->GetTraitBonus(ERogue10mTownTrait::Vitality) == 57.0f && Town->GetTraitBonus(ERogue10mTownTrait::Power) == 11.75f,
			TEXT("legacy_custom_bonuses_survive_further_investment_and_restart"));
		VerifyProfileIsolation(*Expected);
		if (Failures) { Finish(); return; }
		Check(Persistence->SaveNow() && FFileHelper::LoadFileToArray(BaselineBytes, *SaveFile), TEXT("valid_full_catalog_disk_baseline_captured"));
		Persistence->DetachCharacter(*Character, false);
		RejectInvalidFiles();
		// Restore the isolated valid fixture after recording every rejected file. User saves are never used.
		Check(FFileHelper::SaveArrayToFile(BaselineBytes, *SaveFile) && Persistence->AttachCharacter(*Character)
			&& Same(Expected->Town, Town->CaptureProgression()), TEXT("valid_QA_fixture_restores_after_rejected_files"));
		Finish();
	}

	void VerifyProfileIsolation(const URogue10mProgressionSaveGame& Expected)
	{
		FRogue10mCharacterProfile Other;
		Check(Profiles->CreateProfile(TEXT("TraitQaOther"), FRogue10mCharacterAppearance(), Other)
			&& Profiles->SelectProfile(Other.ProfileId), TEXT("second_isolated_GUID_profile_created"));
		Check(!Persistence->CanRetrySave(), TEXT("selected_GUID_change_blocks_retry_of_previous_profile_session"));
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		CapturePointers();
		Check(Character.IsValid() && State && Inventory && Combat && Town && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == Other.ProfileId && State->GetPlayerLevel() == 1
			&& Inventory->GetGold() == 0 && Town->GetSpentTraitPoints() == 0 && Town->CaptureProgression().TraitInvestments.IsEmpty()
			&& State->GetSkillUseCount(TEXT("TraitQaSeparateSkill")) == 0 && !Persistence->HasClaimedReward(TEXT("TraitQaSeparateReceipt")),
			TEXT("different_GUID_does_not_inherit_any_trait_or_other_profile_progress"));
		Check(Profiles->SelectProfile(ProfileId), TEXT("original_GUID_reselected"));
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		CapturePointers();
		Check(State && Inventory && Combat && Town && Persistence->GetAttachedProfileId() == ProfileId
			&& Same(Expected.Town, Town->CaptureProgression()) && Same(Expected.Growth, State->CaptureGrowth())
			&& Same(Expected.Inventory, Inventory->CaptureInventory()) && Same(Expected.Combat, Combat->CaptureCombat()),
			TEXT("switching_back_recovers_original_sixteen_trait_profile"));
		if (Character.IsValid() && NPC.IsValid())
		{
			Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	void InvalidFile(const FString& Label, TFunction<void(URogue10mProgressionSaveGame&)> Mutate)
	{
		auto* Corrupt = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(BaselineBytes));
		Check(Corrupt != nullptr, Label + TEXT("_baseline_deserialized"));
		if (!Corrupt) { return; }
		Mutate(*Corrupt);
		const auto BeforeGrowth = State->CaptureGrowth();
		const auto BeforeInventory = Inventory->CaptureInventory();
		const auto BeforeCombat = Combat->CaptureCombat();
		const auto BeforeTown = Town->CaptureProgression();
		Check(UGameplayStatics::SaveGameToSlot(Corrupt, SaveSlot, 0), Label + TEXT("_invalid_QA_fixture_written"));
		TArray<uint8> InvalidBytes;
		Check(FFileHelper::LoadFileToArray(InvalidBytes, *SaveFile), Label + TEXT("_original_bytes_captured"));
		Check(!Persistence->AttachCharacter(*Character) && !Persistence->IsSaveReady() && !Persistence->GetLastError().IsEmpty(),
			Label + TEXT("_load_fails_closed"));
		Check(!Persistence->CanRetrySave(), Label + TEXT("_rejected_restore_cannot_retry_or_overwrite_file"));
		Check(Same(BeforeGrowth, State->CaptureGrowth()) && Same(BeforeInventory, Inventory->CaptureInventory())
			&& Same(BeforeCombat, Combat->CaptureCombat()) && Same(BeforeTown, Town->CaptureProgression()),
			Label + TEXT("_failed_load_keeps_every_runtime_domain"));
		Check(!Persistence->SaveNow(), Label + TEXT("_save_cannot_overwrite_rejected_file"));
		const bool bOpened = NPC->BeginDialogue(Character.Get());
		Check(!NPC->UpgradeTrait(ERogue10mTownTrait::Power) && !Town->TryResetTraits(), Label + TEXT("_unready_save_blocks_trait_mutations"));
		if (bOpened) { NPC->CloseDialogue(); }
		Persistence->DetachCharacter(*Character, true);
		TArray<uint8> AfterBytes;
		Check(FFileHelper::LoadFileToArray(AfterBytes, *SaveFile) && InvalidBytes == AfterBytes,
			Label + TEXT("_rejected_file_remains_byte_identical"));
		Check(FFileHelper::SaveArrayToFile(AfterBytes, *FPaths::Combine(Output, Label + TEXT(".sav"))),
			Label + TEXT("_rejected_file_evidence_preserved"));
	}

	void RejectInvalidFiles()
	{
		InvalidFile(TEXT("future_version"), [](auto& S) { S.SaveVersion = 999; });
		InvalidFile(TEXT("zero_version"), [](auto& S) { S.SaveVersion = 0; });
		InvalidFile(TEXT("wrong_GUID"), [](auto& S) { S.ProfileId = FGuid::NewGuid(); });
		InvalidFile(TEXT("unknown_trait"), [](auto& S) { S.Town.TraitInvestments[2].Trait = static_cast<ERogue10mTownTrait>(255); });
		InvalidFile(TEXT("duplicate_trait"), [](auto& S) { S.Town.TraitInvestments[3].Trait = S.Town.TraitInvestments[2].Trait; });
		InvalidFile(TEXT("negative_rank"), [](auto& S) { S.Town.TraitInvestments[2].Rank = -1; });
		InvalidFile(TEXT("excess_rank"), [](auto& S) { S.Town.TraitInvestments[2].Rank = 4; });
		InvalidFile(TEXT("spent_points_exceeds_total_budget"), [](auto& S) { S.Town.TraitInvestments[2].SpentPoints += 1; });
		InvalidFile(TEXT("negative_spent_points"), [](auto& S) { S.Town.TraitInvestments[2].SpentPoints = -1; });
		InvalidFile(TEXT("excess_budget"), [](auto& S) { S.Growth.Level = 52; });
		InvalidFile(TEXT("nan_bonus"), [](auto& S) { S.Town.TraitInvestments[2].Bonus = std::numeric_limits<float>::quiet_NaN(); });
		InvalidFile(TEXT("infinite_bonus"), [](auto& S) { S.Town.TraitInvestments[2].Bonus = std::numeric_limits<float>::infinity(); });
		InvalidFile(TEXT("negative_bonus"), [](auto& S) { S.Town.TraitInvestments[2].Bonus = -1.0f; });
		InvalidFile(TEXT("legacy_mirror_mismatch"), [](auto& S) { S.Town.TraitMaxHealthBonus += 1.0f; });
		InvalidFile(TEXT("empty_canonical_array"), [](auto& S) { S.Town.TraitInvestments.Reset(); });
		InvalidFile(TEXT("last_stand_level_bypass"), [](auto& S)
		{
			S.Growth.Level = 19;
			const auto LastStand = S.Town.TraitInvestments.FindByPredicate([](const auto& I) { return I.Trait == ERogue10mTownTrait::LastStand; });
			const FRogue10mTraitInvestment Investment = *LastStand;
			S.Town = FRogue10mTownProgressionSnapshot();
			S.Town.TraitInvestments.Add(Investment);
		});
		InvalidFile(TEXT("invalid_difficulty"), [](auto& S) { S.Town.Difficulty = static_cast<ERogue10mDifficulty>(255); });
		InvalidFile(TEXT("infinite_difficulty_multiplier"), [](auto& S) { S.Town.DifficultyModifiers.HealthMultiplier = std::numeric_limits<float>::infinity(); });
		InvalidFile(TEXT("version_one_new_traits"), [](auto& S) { S.SaveVersion = 1; });
		InvalidFile(TEXT("version_one_invalid_bonus"), [](auto& S)
		{
			S.SaveVersion = 1;
			S.Town.TraitInvestments.Reset();
			S.Town.TraitAttackBonus = std::numeric_limits<float>::quiet_NaN();
		});
		InvalidFile(TEXT("version_one_late_inventory_failure"), [](auto& S)
		{
			S.SaveVersion = 1;
			S.Town.TraitInvestments.Reset();
			S.Growth.Level = 54;
			S.Growth.SkillUses.Add(TEXT("TraitQaMustRollback"), 999);
			S.Inventory.Gold = -1;
		});
	}

	void Finish()
	{
		if (NPC.IsValid()) { NPC->CloseDialogue(); }
		Results += FString::Printf(TEXT("mode=%s\nfailures=%d\nprofile=%s\n"), bRead ? TEXT("read") : TEXT("write"), Failures, *ProfileId.ToString());
		Check(FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, bRead ? TEXT("read-results.txt") : TEXT("write-results.txt"))), TEXT("runtime_evidence_written"));
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=TRAIT_PERSISTENCE_%s mode=%s failures=%d output=%s"),
			Failures ? TEXT("FAILED") : TEXT("PASSED"), bRead ? TEXT("read") : TEXT("write"), Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	URogue10mCharacterProfileSubsystem* Profiles = nullptr;
	URogue10mProgressionSubsystem* Persistence = nullptr;
	ARogue10mPlayerState* State = nullptr;
	URogue10mInventoryComponent* Inventory = nullptr;
	URogue10mCombatComponent* Combat = nullptr;
	URogue10mTownProgressionComponent* Town = nullptr;
	FGuid ProfileId;
	FTimerHandle PrepareTimer;
	TArray<uint8> BaselineBytes;
	FString Output, ExpectedPrefix, SaveSlot, SaveFile, Results;
	int32 Failures = 0;
	bool bRead = false;
};

static TSharedPtr<FRun> Active;
static void Run(bool bRead)
{
	if (!GEngine || GIsEditor) { return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>(); Active->Start(*World, bRead); return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=TRAIT_PERSISTENCE_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}

static FAutoConsoleCommand WriteCommand(TEXT("Rogue10m.TestTraitPersistenceWrite"),
	TEXT("Migrates isolated legacy traits and saves all 16 traits for a second game process."),
	FConsoleCommandDelegate::CreateLambda([] { Run(false); }));
static FAutoConsoleCommand ReadCommand(TEXT("Rogue10m.TestTraitPersistenceRead"),
	TEXT("Verifies isolated 16-trait restart, GUID isolation, and rejected save byte preservation."),
	FConsoleCommandDelegate::CreateLambda([] { Run(true); }));
}
#endif
