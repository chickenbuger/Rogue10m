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
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "TimerManager.h"

namespace Rogue10mProgressionFailureTest
{
template <typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool IsBoundedSlot(const FString& Slot, const TCHAR* Prefix)
{
	if (!Slot.StartsWith(Prefix) || Slot.Len() > 80) { return false; }
	for (TCHAR Character : Slot) { if (!FChar::IsAlnum(Character) && Character != TCHAR('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString ProfileSlot, ProgressionPrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		if (!IsBoundedSlot(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
			|| !IsBoundedSlot(ProgressionPrefix, TEXT("Rogue10m_Progression_QA_")) || InWorld.GetNetMode() != NM_Standalone)
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=PROGRESSION_FAILURES_FAILED reason=missing_bounded_QA_save_overrides"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		ExpectedPrefix = ProgressionPrefix + TEXT("_");
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/town-services-persistence-20261011"),
			ProfileSlot, TEXT("failures"));
		IFileManager::Get().MakeDirectory(*Output, true);
		InWorld.GetTimerManager().SetTimer(PrepareTimer,
			FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 3.0f, false);
	}

private:
	void Check(bool bPassed, const FString& Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
		if (!bPassed) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("PROGRESSION_FAILURES FAIL %s"), *Reason); }
	}

	void Prepare()
	{
		UGameInstance* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Persistence = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		Controller = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		Check(Profiles && Persistence && Controller.IsValid(), TEXT("profile_subsystems_and_controller_exist"));
		if (Failures) { Finish(); return; }
		Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_slot"));
		FRogue10mCharacterProfile Profile;
		if (Failures || !Profiles->CreateProfile(TEXT("FailureQa"), FRogue10mCharacterAppearance(), Profile))
		{
			Check(false, TEXT("isolated_profile_created")); Finish(); return;
		}
		Check(Profiles->SelectProfile(Profile.ProfileId), TEXT("isolated_profile_selected"));
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		Check(Character.IsValid() && State && Inventory && Combat && Town && Persistence->IsSaveReady(),
			TEXT("valid_profile_character_automatically_attached"));
		if (Failures) { Finish(); return; }
		SaveSlot = Persistence->GetSaveSlotForProfile(Profile.ProfileId);
		Check(SaveSlot.StartsWith(ExpectedPrefix), TEXT("target_progression_slot_is_QA_isolated"));
		if (Failures) { Finish(); return; }
		SaveFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), SaveSlot + TEXT(".sav"));
		Check(Persistence->SaveNow(), TEXT("valid_baseline_saved"));
		Check(FFileHelper::LoadFileToArray(BaselineBytes, *SaveFile) && !BaselineBytes.IsEmpty(), TEXT("valid_baseline_disk_bytes_captured"));
		Persistence->DetachCharacter(*Character, false);
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(NPC.IsValid(), TEXT("actual_start_hub_NPC_exists"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		CorruptCase(TEXT("version"), true);
		CorruptCase(TEXT("missing_asset_after_growth"), false);
		Finish();
	}

	void CorruptCase(const FString& Label, bool bBadVersion)
	{
		URogue10mProgressionSaveGame* Corrupt = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(BaselineBytes));
		Check(Corrupt != nullptr, Label + TEXT("_baseline_deserialized"));
		if (!Corrupt) { return; }
		if (bBadVersion) { Corrupt->SaveVersion = 999; }
		else
		{
			Check(!Corrupt->Inventory.Containers.IsEmpty() && !Corrupt->Inventory.Containers[0].Entries.IsEmpty(),
				Label + TEXT("_real_grid_entry_exists"));
			if (Corrupt->Inventory.Containers.IsEmpty() || Corrupt->Inventory.Containers[0].Entries.IsEmpty()) { return; }
			// Growth succeeds first; the later asset failure must roll all earlier imports back.
			Corrupt->Growth.Level = 3;
			Corrupt->Growth.Experience = 7.0f;
			Corrupt->Growth.ExperienceToNextLevel = 100.0f;
			Corrupt->Growth.SkillUses.Add(TEXT("CorruptQaSeed"), 999);
			Corrupt->Inventory.Containers[0].Entries[0].ItemAsset = TSoftObjectPtr<URogue10mItemDataAsset>(
				FSoftObjectPath(TEXT("/Game/Rogue10m/QA/IntentionallyMissingItem.IntentionallyMissingItem")));
		}
		const FRogue10mGrowthSnapshot BeforeGrowth = State->CaptureGrowth();
		const FRogue10mInventorySnapshot BeforeInventory = Inventory->CaptureInventory();
		const FRogue10mCombatSnapshot BeforeCombat = Combat->CaptureCombat();
		const FRogue10mTownProgressionSnapshot BeforeTown = Town->CaptureProgression();
		Check(UGameplayStatics::SaveGameToSlot(Corrupt, SaveSlot, 0), Label + TEXT("_isolated_invalid_snapshot_written"));
		TArray<uint8> InvalidBytes;
		Check(FFileHelper::LoadFileToArray(InvalidBytes, *SaveFile) && !InvalidBytes.IsEmpty(), Label + TEXT("_invalid_disk_bytes_captured"));
		Check(!Persistence->AttachCharacter(*Character) && !Persistence->IsSaveReady()
			&& !Persistence->GetLastError().IsEmpty(), Label + TEXT("_attach_fails_closed_with_error"));
		Check(Same(BeforeGrowth, State->CaptureGrowth()) && Same(BeforeInventory, Inventory->CaptureInventory())
			&& Same(BeforeCombat, Combat->CaptureCombat()) && Same(BeforeTown, Town->CaptureProgression()),
			Label + TEXT("_failed_import_rolls_back_runtime_growth_inventory_combat_town"));
		Check(!Persistence->SaveNow(), Label + TEXT("_failed_import_cannot_overwrite_disk"));
		Check(NPC->BeginDialogue(Character.Get()) && !NPC->PurchasePotion() && !NPC->ClaimStartingSupplies()
			&& !NPC->UpgradeTrait(ERogue10mTownTrait::Vitality) && !NPC->SelectDifficulty(ERogue10mDifficulty::Hard),
			Label + TEXT("_NPC_services_and_supply_claim_reject_unready_save"));
		NPC->CloseDialogue();
		Persistence->DetachCharacter(*Character, true);
		TArray<uint8> AfterBytes;
		Check(FFileHelper::LoadFileToArray(AfterBytes, *SaveFile) && InvalidBytes == AfterBytes,
			Label + TEXT("_disk_is_byte_identical_after_rejected_save_and_detach"));
		Check(FFileHelper::SaveArrayToFile(AfterBytes, *FPaths::Combine(Output, Label + TEXT(".sav"))),
			Label + TEXT("_invalid_snapshot_preserved_as_evidence"));
	}

	void Finish()
	{
		if (NPC.IsValid()) { NPC->CloseDialogue(); }
		Results += FString::Printf(TEXT("failures=%d\nslot=%s\n"), Failures, *SaveSlot);
		Check(FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, TEXT("results.txt"))), TEXT("failure_evidence_written"));
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=PROGRESSION_FAILURES_%s failures=%d output=%s"),
			Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures, *Output);
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
	FTimerHandle PrepareTimer;
	TArray<uint8> BaselineBytes;
	FString Output, ExpectedPrefix, SaveSlot, SaveFile, Results;
	int32 Failures = 0;
};

static TSharedPtr<FRun> Active;
static void Run()
{
	if (!GEngine || GIsEditor) { return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>(); Active->Start(*World); return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=PROGRESSION_FAILURES_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}

static FAutoConsoleCommand Command(TEXT("Rogue10m.TestProgressionFailures"),
	TEXT("Checks version/asset restore failures preserve isolated QA disk bytes and reject NPC/save mutations."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif
