// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Internationalization/Text.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "UObject/UnrealType.h"

namespace Rogue10mLegacyPortalQA
{
constexpr auto Hub = TEXT("/Game/Rogue10m/Maps/StartHub");
constexpr auto OpenWorld = TEXT("/Game/Rogue10m/Maps/OpenRunMap");
constexpr auto ReplayRoot = TEXT("tmp/LegacyPortal20261118/replays");
constexpr int32 Gold = 137;
constexpr int32 MasteryExperience = 3;

bool ContainsVersionTag(const TArray<uint8>& Bytes)
{
	constexpr ANSICHAR VersionTag[] = "SaveVersion";
	for (int32 Index = 0; Index <= Bytes.Num() - static_cast<int32>(sizeof(VersionTag)); ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, VersionTag, sizeof(VersionTag)) == 0) { return true; }
	}
	return false;
}

bool MakeUnversionedFixture(URogue10mProgressionSaveGame& Save, TArray<uint8>& Bytes)
{
	// Historical saves omitted their default-valued SaveVersion. Keep the
	// fixture's field at the current default to reproduce omission with native I/O.
	return UGameplayStatics::SaveGameToMemory(&Save, Bytes) && !ContainsVersionTag(Bytes);
}

bool SameSlot(const FString& Slot, const TArray<uint8>& Expected)
{
	TArray<uint8> Actual;
	return UGameplayStatics::LoadDataFromSlot(Actual, Slot, 0) && Actual == Expected;
}

FString MapPackage(const UWorld& World)
{
	return UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
}

bool EqualOptionalString(const TOptional<FString>& Before, const TOptional<FString>& After)
{
	return Before.IsSet() == After.IsSet() && (!Before.IsSet() || Before.GetValue().Equals(After.GetValue(), ESearchCase::CaseSensitive));
}

bool EqualSavedText(const FText& Before, const FText& After)
{
	if (FTextInspector::GetFlags(Before) != FTextInspector::GetFlags(After) || Before.IsFromStringTable() != After.IsFromStringTable()
		|| !EqualOptionalString(FTextInspector::GetNamespace(Before), FTextInspector::GetNamespace(After))
		|| !EqualOptionalString(FTextInspector::GetKey(Before), FTextInspector::GetKey(After))
		|| !FTextInspector::GetDisplayString(Before).Equals(FTextInspector::GetDisplayString(After), ESearchCase::CaseSensitive))
	{
		return false;
	}
	if (Before.IsFromStringTable())
	{
		FName BeforeTable, AfterTable;
		FString BeforeKey, AfterKey;
		if (!FTextInspector::GetTableIdAndKey(Before, BeforeTable, BeforeKey)
			|| !FTextInspector::GetTableIdAndKey(After, AfterTable, AfterKey)
			|| BeforeTable != AfterTable || !BeforeKey.Equals(AfterKey, ESearchCase::CaseSensitive)) { return false; }
	}
	const FString* BeforeSource = FTextInspector::GetSourceString(Before);
	const FString* AfterSource = FTextInspector::GetSourceString(After);
	if ((BeforeSource == nullptr) != (AfterSource == nullptr)
		|| (BeforeSource && !BeforeSource->Equals(*AfterSource, ESearchCase::CaseSensitive))) { return false; }

	// Preserve history such as a text format or number options in addition to
	// the current rendered value. Pointer identity can change after save/load.
	FString BeforeHistory, AfterHistory;
	FTextStringHelper::WriteToBuffer(BeforeHistory, Before, true);
	FTextStringHelper::WriteToBuffer(AfterHistory, After, true);
	return BeforeHistory.Equals(AfterHistory, ESearchCase::CaseSensitive);
}

void CollectDifferences(const UStruct& Type, const void* Before, const void* After, const FString& Path,
	TArray<FString>& Differences, int32& EqualTextCount);

void CollectPropertyDifferences(const FProperty& Property, const void* Before, const void* After, const FString& Path,
	TArray<FString>& Differences, int32& EqualTextCount)
{
	constexpr int32 MaximumReportedDifferences = 24;
	if (Differences.Num() >= MaximumReportedDifferences || Property.Identical(Before, After, 0)) { return; }
	if (CastField<FTextProperty>(&Property) && EqualSavedText(*static_cast<const FText*>(Before), *static_cast<const FText*>(After)))
	{
		++EqualTextCount;
		return;
	}
	if (const auto* Struct = CastField<FStructProperty>(&Property))
	{
		CollectDifferences(*Struct->Struct, Before, After, Path, Differences, EqualTextCount);
		return;
	}
	else if (const auto* Array = CastField<FArrayProperty>(&Property))
	{
		FScriptArrayHelper BeforeArray(Array, Before);
		FScriptArrayHelper AfterArray(Array, After);
		if (BeforeArray.Num() != AfterArray.Num())
		{
			Differences.Add(Path + TEXT(" (array lengths differ)"));
			return;
		}
		for (int32 Index = 0; Index < BeforeArray.Num() && Differences.Num() < MaximumReportedDifferences; ++Index)
		{
			CollectPropertyDifferences(*Array->Inner, BeforeArray.GetRawPtr(Index), AfterArray.GetRawPtr(Index),
				FString::Printf(TEXT("%s[%d]"), *Path, Index), Differences, EqualTextCount);
		}
		return;
	}
	Differences.Add(Path);
}

void CollectDifferences(const UStruct& Type, const void* Before, const void* After, const FString& Path,
	TArray<FString>& Differences, int32& EqualTextCount)
{
	for (TFieldIterator<FProperty> It(&Type); It; ++It)
	{
		const FProperty& Property = **It;
		CollectPropertyDifferences(Property, Property.ContainerPtrToValuePtr<void>(Before), Property.ContainerPtrToValuePtr<void>(After),
			Path + TEXT(".") + Property.GetName(), Differences, EqualTextCount);
	}
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& World)
	{
		const TCHAR* Args = FCommandLine::Get();
		FString Label, ProfileSlot, KeySlot;
		FParse::Value(Args, TEXT("RogueLegacyPortalLabel="), Label);
		FParse::Value(Args, TEXT("RogueLegacyPortalCase="), Scenario);
		FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
		FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), KeySlot);
		bool bLabelValid = !Label.IsEmpty() && Label.Len() <= 24;
		for (TCHAR C : Label) { bLabelValid &= FChar::IsAlnum(C) || C == TEXT('_'); }
		const FString Suffix = Label + TEXT("_") + Scenario;
		const bool bSupported = Scenario == TEXT("Legacy1") || Scenario == TEXT("Legacy2") || Scenario == TEXT("Legacy3")
			|| Scenario == TEXT("Omitted4") || Scenario == TEXT("Omitted5")
			|| Scenario == TEXT("Invalid0") || Scenario == TEXT("Future999");
		if (bLabelValid && IsCopiedReplay() && MapPackage(World) == Hub && World.GetNetMode() == NM_Standalone)
		{
			if (!ProfileSlot.IsEmpty() || !Prefix.IsEmpty() || !KeySlot.IsEmpty() || !StartsInReplayDirectory(Label))
			{
				Fail(TEXT("copied_replay_requires_private_UserDir_and_default_save_names"));
				return;
			}
			StartTicker(World);
			return;
		}
		if (!bLabelValid || !bSupported || MapPackage(World) != Hub || World.GetNetMode() != NM_Standalone
			|| ProfileSlot != TEXT("Rogue10m_Town_QA_LegacyPortal_") + Suffix
			|| Prefix != TEXT("Rogue10m_Progression_QA_LegacyPortal_") + Suffix
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_LegacyPortal_") + Suffix)
		{
			Fail(TEXT("exact_isolated_StartHub_arguments_required"));
			return;
		}
		TArray<FString> Existing;
		IFileManager::Get().FindFiles(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Prefix + TEXT("_*.sav"))), true, false);
		Instance = World.GetGameInstance();
		const auto* Profiles = Instance.IsValid() ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		if (!Profiles || Profiles->GetProfileCount() != 0 || Profiles->GetSelectedProfile() || !Existing.IsEmpty()
			|| UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0) || UGameplayStatics::DoesSaveGameExist(KeySlot, 0))
		{
			Fail(TEXT("fresh_isolated_save_namespace_required"));
			return;
		}
		StartTicker(World);
	}

private:
	enum class EStage { Seed, Restore, Walk };

	static bool StartsInReplayDirectory(const FString& Label)
	{
		FString ExpectedUserDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / ReplayRoot / Label);
		FString ActualUserDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir());
		FString ActualSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		FPaths::NormalizeDirectoryName(ExpectedUserDir);
		FPaths::NormalizeDirectoryName(ActualUserDir);
		FPaths::NormalizeDirectoryName(ActualSavedDir);
		return ActualUserDir.Equals(ExpectedUserDir, ESearchCase::IgnoreCase)
			&& ActualSavedDir.Equals(ExpectedUserDir / TEXT("Saved"), ESearchCase::IgnoreCase);
	}

	void StartTicker(UWorld& World)
	{
		Instance = World.GetGameInstance();
		Deadline = FPlatformTime::Seconds() + 180.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float DeltaSeconds)
		{
			return Self->Tick(DeltaSeconds);
		}));
	}

	void Fail(const TCHAR* Reason)
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=LEGACY_PORTAL_FAILED case=%s reason=%s"), *Scenario, Reason);
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}

	void Pass()
	{
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=LEGACY_PORTAL_PASSED case=%s checks=%d actual_portal=%d walk_input=%d"),
			*Scenario, Checks, WalksPortal(), WalksPortal());
		FPlatformMisc::RequestExitWithStatus(false, 0);
	}

	bool Require(bool bResult, const TCHAR* Check)
	{
		if (!bResult) { Fail(Check); return false; }
		++Checks;
		UE_LOG(LogRogue10m, Display, TEXT("LEGACY_PORTAL PASS %s"), Check);
		return true;
	}

	bool IsRejectedScenario() const { return Scenario == TEXT("Invalid0") || Scenario == TEXT("Future999"); }
	bool IsCopiedReplay() const { return Scenario == TEXT("CopiedLegacy"); }
	bool WalksPortal() const { return Scenario == TEXT("Legacy1") || Scenario == TEXT("Legacy3") || IsCopiedReplay(); }

	bool ReadCopiedSnapshot(ARogue10mPlayerController& Controller, ARogue10mCharacter& Pawn, URogue10mProgressionSubsystem& Progression)
	{
		const auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
		if (!Require(Selected && Selected->ProfileId.IsValid(), TEXT("copied_profile_is_selected_in_private_game_instance"))) { return false; }
		ProfileId = Selected->ProfileId;
		LegacySlot = Progression.GetLegacySaveSlotForProfile(ProfileId);
		CurrentSlot = Progression.GetSaveSlotForProfile(ProfileId);
		FixtureSlot = LegacySlot;
		if (!Require(LegacySlot == TEXT("Rogue10mProgression_") + ProfileId.ToString(EGuidFormats::Digits)
			&& CurrentSlot == LegacySlot + TEXT("_v5") && UGameplayStatics::LoadDataFromSlot(FixtureBytes, LegacySlot, 0)
			&& !ContainsVersionTag(FixtureBytes), TEXT("actual_bare_progression_snapshot_read_from_private_copy"))) { return false; }
		const auto* Save = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(FixtureBytes));
		if (!Require(Save && Save->ProfileId == ProfileId, TEXT("copied_snapshot_matches_selected_profile"))) { return false; }
		ExpectedLevel = Save->Growth.Level;
		ExpectedGold = Save->Inventory.Gold;
		ExpectedCrystals = Save->Inventory.Crystals;
		for (const FRogue10mSkillTreeMasteryRecord& Mastery : Save->SkillTree.Masteries)
		{
			if (Mastery.Track == ERogue10mSkillTreeTrack::Brawler) { ExpectedMastery = Mastery.TotalExperience; }
		}
		// A Live Coding child may have attempted attachment before the patch was
		// applied. Keep a healthy startup attachment and retry an unready one
		// through normal profile entry, without writing the rejected snapshot.
		if (Progression.IsAttachedTo(Pawn) && !Progression.IsSaveReady()) { Progression.DetachCharacter(Pawn, false); }
		if (!Progression.IsAttachedTo(Pawn)) { Controller.EnterSelectedCharacter(); }
		Controller.CloseAllBlockingPanels();
		Stage = EStage::Restore;
		return true;
	}

	bool Seed(ARogue10mPlayerController& Controller, ARogue10mCharacter& Pawn, URogue10mProgressionSubsystem& Progression)
	{
		auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		FRogue10mCharacterProfile Profile;
		if (!Require(Profiles && Profiles->CreateProfile(TEXT("PortalQA"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("normal_profile_created_and_selected"))) { return false; }
		ProfileId = Profile.ProfileId;
		LegacySlot = Progression.GetLegacySaveSlotForProfile(ProfileId);
		CurrentSlot = Progression.GetSaveSlotForProfile(ProfileId);
		if (!Require(LegacySlot == Prefix + TEXT("_") + ProfileId.ToString(EGuidFormats::Digits)
			&& CurrentSlot == LegacySlot + TEXT("_v5"), TEXT("progression_slots_are_QA_only"))) { return false; }

		URogue10mProgressionSaveGame* SeedSave = NewObject<URogue10mProgressionSaveGame>();
		SeedSave->ProfileId = ProfileId;
		SeedSave->Growth.Level = 4;
		SeedSave->Growth.WeaponType = Pawn.GetEquippedWeaponType();
		SeedSave->Inventory = Pawn.GetInventoryComponent()->CaptureInventory();
		SeedSave->Inventory.Gold = Gold;
		SeedSave->Combat = Pawn.GetCombatComponent()->CaptureCombat();
		SeedSave->Combat.WeaponPresets.Reset(); // This optional field was added to v5 later.
		if (Scenario == TEXT("Legacy1") || Scenario == TEXT("Legacy2"))
		{
			SeedSave->Town.VitalityLevel = 1;
			SeedSave->Town.TraitMaxHealthBonus = 10.0f;
			if (Scenario == TEXT("Legacy2"))
			{
				auto& Investment = SeedSave->Town.TraitInvestments.AddDefaulted_GetRef();
				Investment.Rank = 1;
				Investment.SpentPoints = 1;
				Investment.Bonus = 10.0f;
			}
		}
		if (Scenario == TEXT("Legacy3"))
		{
			auto& Mastery = SeedSave->SkillTree.Masteries.AddDefaulted_GetRef();
			Mastery.Track = ERogue10mSkillTreeTrack::Brawler;
			Mastery.TotalExperience = MasteryExperience;
			ExpectedMastery = MasteryExperience;
		}
		ExpectedCrystals = SeedSave->Inventory.Crystals;
		FixtureSlot = LegacySlot;
		if (Scenario == TEXT("Omitted4") || Scenario == TEXT("Omitted5"))
		{
			SeedRunId = FGuid::NewGuid();
			SeedSave->RunId = SeedRunId;
			SeedSave->HighestRunLevel = SeedSave->Growth.Level;
			FixtureSlot = Scenario == TEXT("Omitted4") ? LegacySlot + TEXT("_v4") : CurrentSlot;
		}
		if (!Require(MakeUnversionedFixture(*SeedSave, FixtureBytes)
			&& UGameplayStatics::SaveDataToSlot(FixtureBytes, FixtureSlot, 0), TEXT("historical_payload_written_with_no_SaveVersion_tag"))) { return false; }
		if (IsRejectedScenario())
		{
			// Make a usable older slot available, then ensure newest-invalid stays authoritative.
			SeedSave->SaveVersion = Scenario == TEXT("Invalid0") ? 0 : 999;
			if (!Require(UGameplayStatics::SaveGameToMemory(SeedSave, InvalidBytes) && ContainsVersionTag(InvalidBytes)
				&& UGameplayStatics::SaveDataToSlot(InvalidBytes, CurrentSlot, 0), TEXT("explicit_invalid_current_slot_over_valid_legacy"))) { return false; }
		}
		Controller.EnterSelectedCharacter();
		Controller.CloseAllBlockingPanels();
		Stage = EStage::Restore;
		return true;
	}

	bool Restore(ARogue10mCharacter& Pawn, URogue10mProgressionSubsystem& Progression, UWorld& World)
	{
		if (!Progression.IsAttachedTo(Pawn) || Progression.GetAttachedProfileId() != ProfileId) { return true; }
		if (IsRejectedScenario())
		{
			if (Require(!Progression.IsSaveReady() && !Progression.GetLastError().IsEmpty() && !Progression.SaveNow()
				&& SameSlot(CurrentSlot, InvalidBytes) && SameSlot(LegacySlot, FixtureBytes), TEXT("invalid_current_blocks_save_and_older_fallback"))) { Pass(); }
			return false;
		}
		auto* PlayerState = Pawn.GetPlayerState<ARogue10mPlayerState>();
		const FGuid RunId = Progression.GetAttachedRunId();
		if (!Require(Progression.IsSaveReady() && PlayerState && RunId.IsValid()
			&& (!SeedRunId.IsValid() || SeedRunId == RunId) && PlayerState->GetPlayerLevel() == ExpectedLevel
			&& Pawn.GetInventoryComponent()->GetGold() == ExpectedGold && Pawn.GetInventoryComponent()->GetCrystals() == ExpectedCrystals,
			TEXT("hub_legacy_progression_restored_through_normal_profile_entry"))) { return false; }
		if (Scenario == TEXT("Legacy1") || Scenario == TEXT("Legacy2"))
		{
			const auto Town = PlayerState->GetTownProgressionComponent()->CaptureProgression();
			if (!Require(Town.VitalityLevel == 1 && Town.TraitMaxHealthBonus == 10.0f && Town.TraitInvestments.Num() == 1
				&& Town.TraitInvestments[0].SpentPoints == 1, TEXT("legacy_town_upgrade_preserved_as_canonical_ledger"))) { return false; }
		}
		if (!Require(SameSlot(FixtureSlot, FixtureBytes), TEXT("original_fixture_bytes_unchanged_by_restore"))) { return false; }
		if (!WalksPortal()) { Pass(); return false; }
		if ((Scenario == TEXT("Legacy3") || IsCopiedReplay()) && !Require(PlayerState->GetSkillTreeProgressionComponent()->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler) == ExpectedMastery,
			TEXT("legacy_v3_mastery_not_replaced_by_pre_v3_migration"))) { return false; }

		const FNameProperty* Destination = FindFProperty<FNameProperty>(ARogue10mRunPortal::StaticClass(), TEXT("TargetLevelName"));
		TArray<ARogue10mRunPortal*> Gates;
		for (TActorIterator<ARogue10mRunPortal> It(&World); Destination && It; ++It)
		{
			if (Destination->GetPropertyValue_InContainer(*It) == FName(OpenWorld)) { Gates.Add(*It); }
		}
		if (!Require(Gates.Num() == 1 && World.GetGameState<ARogue10mGameState>()->GetRunPhase() == ERogue10mRunPhase::WaitingToStart,
			TEXT("authored_OpenRunMap_portal_found_in_waiting_hub"))) { return false; }
		EntryRunId = RunId;
		EntryInventory = Pawn.GetInventoryComponent()->CaptureInventory();
		const FVector Center = Gates[0]->GetActorLocation();
		const FVector Axis = Gates[0]->GetActorRightVector();
		Waypoints = {FVector(650, -1100, 0), FVector(650, 950, 0), FVector(0, 1800, 0), Center - Axis * 300.0, Center + Axis * 300.0};
		Stage = EStage::Walk;
		return true;
	}

	bool CheckTravelState(const ARogue10mCharacter& Pawn, const ARogue10mPlayerState& PlayerState, const URogue10mProgressionSubsystem& Progression)
	{
		bool bPassed = true;
		const auto Record = [this, &bPassed](bool bThisPassed, const TCHAR* TestName)
		{
			if (bThisPassed) { ++Checks; }
			bPassed &= bThisPassed;
			UE_LOG(LogRogue10m, Display, TEXT("LEGACY_PORTAL %s %s"), bThisPassed ? TEXT("PASS") : TEXT("MISMATCH"), TestName);
		};
		const auto FieldInventory = Pawn.GetInventoryComponent()->CaptureInventory();
		Record(Progression.IsSaveReady(), TEXT("arrival_progression_save_ready"));
		Record(Progression.GetAttachedProfileId() == ProfileId, TEXT("arrival_profile_unchanged"));
		Record(Progression.GetAttachedRunId() == EntryRunId, TEXT("arrival_run_id_unchanged"));
		Record(PlayerState.GetPlayerLevel() == ExpectedLevel, TEXT("arrival_level_preserved"));
		Record(FieldInventory.Gold == ExpectedGold, TEXT("arrival_gold_preserved"));
		Record(FieldInventory.Crystals == ExpectedCrystals, TEXT("arrival_crystals_preserved"));
		Record(PlayerState.GetSkillTreeProgressionComponent()->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler) == ExpectedMastery,
			TEXT("arrival_brawler_mastery_preserved"));
		const bool bStrictInventoryIdentity = FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&EntryInventory, &FieldInventory, 0);
		TArray<FString> Differences;
		int32 EqualSavedTextCount = 0;
		if (!bStrictInventoryIdentity)
		{
			CollectDifferences(*FRogue10mInventorySnapshot::StaticStruct(), &EntryInventory, &FieldInventory, TEXT("Inventory"), Differences, EqualSavedTextCount);
			for (const FString& Difference : Differences)
			{
				UE_LOG(LogRogue10m, Display, TEXT("LEGACY_PORTAL INVENTORY_DIFFERENCE field=%s"), *Difference);
			}
		}
		UE_LOG(LogRogue10m, Display, TEXT("LEGACY_PORTAL INVENTORY_OBSERVATION strict_identity=%d equivalent_saved_text_fields=%d differences=%d"),
			bStrictInventoryIdentity, EqualSavedTextCount, Differences.Num());
		Record(bStrictInventoryIdentity || (EqualSavedTextCount > 0 && Differences.IsEmpty()), TEXT("arrival_inventory_snapshot_values_equal"));
		return bPassed;
	}

	bool Tick(float DeltaSeconds)
	{
		if (FPlatformTime::Seconds() > Deadline) { Fail(TEXT("watchdog_waiting_for_profile_or_portal_travel")); return false; }
		UWorld* World = Instance.IsValid() ? Instance->GetWorld() : nullptr;
		auto* Controller = World ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		auto* Pawn = Controller ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
		auto* PlayerState = Pawn ? Pawn->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		auto* GameState = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
		auto* Progression = Instance.IsValid() ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		if (!World || !World->HasBegunPlay() || !Pawn || !PlayerState || !GameState || !Progression) { return true; }

		if (Stage == EStage::Seed)
		{
			return IsCopiedReplay() ? ReadCopiedSnapshot(*Controller, *Pawn, *Progression) : Seed(*Controller, *Pawn, *Progression);
		}
		if (Stage == EStage::Restore) { return Restore(*Pawn, *Progression, *World); }
		if (MapPackage(*World) == OpenWorld)
		{
			if (!Progression->IsAttachedTo(*Pawn) || !Pawn->GetCharacterMovement()->IsMovingOnGround()) { return true; }
			if (!Require(GameState->IsRunActive() && GameState->GetRunDurationSeconds() == 1200.0f
				&& GameState->GetRemainingRunSeconds() > 1185.0f, TEXT("real_OpenRunMap_loaded_and_normal_timer_started"))) { return false; }
			const bool bTravelStatePassed = CheckTravelState(*Pawn, *PlayerState, *Progression);
			const auto* Current = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(CurrentSlot, 0));
			if (Require(Current && Current->SaveVersion == URogue10mProgressionSaveGame::CurrentVersion
				&& Current->ProfileId == ProfileId && Current->RunId == EntryRunId
				&& Current->EntryCurrencies.IsValid() && Current->EntryCurrencies.RunId == EntryRunId
				&& Current->EntryCurrencies.Gold == ExpectedGold && Current->EntryCurrencies.Crystals == ExpectedCrystals
				&& SameSlot(LegacySlot, FixtureBytes), TEXT("current_save_has_same_run_with_entry_baseline_and_old_bytes_preserved")))
			{
				if (bTravelStatePassed) { Pass(); }
				else { Fail(TEXT("arrival_state_mismatch_see_individual_checks")); }
			}
			return false;
		}
		if (MapPackage(*World) != Hub) { Fail(TEXT("unexpected_travel_destination")); return false; }
		if (Waypoint == Waypoints.Num()) { Fail(TEXT("walked_through_gate_without_travel")); return false; }
		FVector Direction = Waypoints[Waypoint] - Pawn->GetActorLocation();
		Direction.Z = 0.0;
		const double Distance = Direction.Size();
		if (Distance < 50.0) { ++Waypoint; return true; }
		const double MaxFrameStep = Pawn->GetCharacterMovement()->MaxWalkSpeed * DeltaSeconds;
		Pawn->AddMovementInput(Direction / Distance, FMath::Min(1.0, Distance / FMath::Max(100.0, MaxFrameStep * 1.1)), false);
		return true;
	}

	TWeakObjectPtr<UGameInstance> Instance;
	FString Scenario, Prefix, LegacySlot, CurrentSlot, FixtureSlot;
	TArray<uint8> FixtureBytes, InvalidBytes;
	TArray<FVector> Waypoints;
	FRogue10mInventorySnapshot EntryInventory;
	FGuid ProfileId, SeedRunId, EntryRunId;
	EStage Stage = EStage::Seed;
	double Deadline = 0.0;
	int32 ExpectedLevel = 4;
	int32 ExpectedGold = Gold;
	int32 ExpectedCrystals = 0;
	int32 ExpectedMastery = 0;
	int32 Checks = 0;
	int32 Waypoint = 0;
};

bool TryStart()
{
	if (!GEngine || GIsEditor) { return false; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && Context.WorldType == EWorldType::Game && World->HasBegunPlay())
		{
			MakeShared<FRun>()->Start(*World);
			return true;
		}
	}
	return false;
}

void ScheduleOptInRun()
{
	static bool bScheduled = false;
	if (bScheduled || GIsEditor || !FParse::Param(FCommandLine::Get(), TEXT("RogueLegacyPortalAutoStart"))) { return; }
	bScheduled = true;
	const double Deadline = FPlatformTime::Seconds() + 60.0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline](float)
	{
		if (TryStart()) { return false; }
		if (FPlatformTime::Seconds() < Deadline) { return true; }
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=LEGACY_PORTAL_FAILED reason=no_game_world_after_opt_in"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return false;
	}));
}

// The first run waits until the launch map is ready. A validator started with
// the base module can also begin when the explicit Live Coding patch arrives.
static FDelayedAutoRegisterHelper AutoStart(EDelayedRegisterRunPhase::EndOfEngineInit, &ScheduleOptInRun, true);
}
#endif
