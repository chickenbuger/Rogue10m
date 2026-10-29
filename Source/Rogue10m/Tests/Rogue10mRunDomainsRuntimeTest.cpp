// Isolated QA profile integration; no shipping gameplay or user save mutations.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mRunDomainsQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString ProfileSlot, Prefix, KeySlot;
		const TCHAR* Args = FCommandLine::Get();
		FParse::Value(Args,TEXT("RogueRunDomainsLabel="),Label);
		FParse::Value(Args,TEXT("RogueRunDomainsMode="),Mode);
		FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot);
		FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);
		FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
		bool bLabel = !Label.IsEmpty() && Label.Len() <= 24;
		for (TCHAR Ch : Label) { bLabel &= FChar::IsAlnum(Ch) || Ch == TCHAR('_'); }
		if (!bLabel || ProfileSlot != TEXT("Rogue10m_Town_QA_Domains_") + Label
			|| Prefix != TEXT("Rogue10m_Progression_QA_Domains_") + Label
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_Domains_") + Label
			|| (Mode != TEXT("Victory") && Mode != TEXT("Defeat") && Mode != TEXT("Timeout"))
			|| InWorld.GetNetMode() != NM_Standalone)
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_DOMAINS_FAILED reason=exact_fresh_QA_overrides_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(),TEXT("Feature/doc/evidence/run-domains-20261029"),Label);
		Images = FPaths::Combine(FPaths::ProjectDir(),TEXT("DevLog/20261029_RunDomains_") + Label);
		if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_DOMAINS_FAILED reason=fresh_evidence_label_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		IFileManager::Get().MakeDirectory(*Output,true);
		bRendered = !FParse::Param(Args,TEXT("nullrhi"));
		if (bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
		Deadline = FPlatformTime::Seconds()+180.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (FPlatformTime::Seconds() > Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
			if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
			return !Self->bFinished;
		}),0.02f);
		Later(3.0,[this]{Prepare();});
	}
	bool Finished() const { return bFinished; }
private:
	void Check(bool bOK,const TCHAR* Why)
	{
		++Checks; Results += FString::Printf(TEXT("%s %s\n"),bOK?TEXT("PASS"):TEXT("FAIL"),Why);
		if (!bOK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("RUN_DOMAINS FAIL %s"),Why); }
	}
	void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
	void Photo(const TCHAR* Name)
	{
		if (!bRendered) { return; }
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
	bool OnlyKnuckleWeapons() const
	{
		const auto IsPermitted=[](const FRogue10mItemStack& Item)
		{ return !Item.bOccupied || Item.EquipSlotType != ERogue10mInventorySlotType::MainWeapon || Item.WeaponType == ERogue10mWeaponType::Knuckle; };
		for (const auto& Item:Inventory->GetItemSlots()) { if (!IsPermitted(Item)) { return false; } }
		for (const auto& EquipmentSlot:Inventory->GetLeftEquipmentSlots()) { if (EquipmentSlot.bHasEquippedItem && !IsPermitted(EquipmentSlot.EquippedItem)) { return false; } }
		for (const auto& Container:Inventory->GetInventoryContainers())
		{ for (const auto& Entry:Container.Entries) { if (Entry.ItemData && Entry.ItemData->EquipSlotType == ERogue10mInventorySlotType::MainWeapon && Entry.ItemData->WeaponType != ERogue10mWeaponType::Knuckle) { return false; } } }
		return true;
	}
	void Prepare()
	{
		auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
		Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
		Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
		PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
		Run=World.IsValid()?World->GetGameState<ARogue10mGameState>():nullptr;
		Check(Profiles && Saves && PC.IsValid() && Run.IsValid(),TEXT("real_game_subsystems_and_run_state"));
		if (Failures) { Finish(); return; }
		Check(Profiles->GetProfileCount()==0,TEXT("fresh_QA_profile_slot"));
		FRogue10mCharacterProfile Profile;
		Check(!Failures && Profiles->CreateProfile(TEXT("DomainsQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("new_isolated_profile_selected"));
		if (Failures) { Finish(); return; }
		ProfileId=Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
		Check(Character.IsValid() && State && Inventory && Combat && Tree && Town && Saves->IsSaveReady(),TEXT("real_character_attached_and_ready"));
		if (Failures) { Finish(); return; }
		Check(Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle && OnlyKnuckleWeapons(),TEXT("fresh_profile_has_knuckle_and_no_other_weapons"));
		Check(Town->UsesLobbyBudget() && Town->GetAvailableTraitPoints()==0,TEXT("new_lobby_budget_starts_zero"));
		InitialGold=Inventory->GetGold(); InitialCrystals=Inventory->GetCrystals(); InitialCombat=Combat->CaptureCombat();
		for (const auto& Row:Tree->Capture().Investments) { InitialNodes.Add(Row.NodeId); }
		Photo(TEXT("01_fresh_knuckle.png")); Later(0.5,[this]{SeedRun();});
	}
	void SeedRun()
	{
		State->AddExperience(500); State->RecordMonsterDefeat(TEXT("QA.DomainsEnemy"),2);
		for (int32 I=0;I<30;++I) { Tree->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::Brawler); }
		Inventory->AddGold(77); Saves->RecordClaimedReward(TEXT("QA.RunReceipt"));
		auto* AcquiredSword=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
		int32 LootContainer=INDEX_NONE; FGuid LootId;
		Check(AcquiredSword && Inventory->TryAddGridItem(AcquiredSword,1,LootContainer,LootId),TEXT("run_acquired_real_sword_item_added"));
		Check(State->GetPlayerLevel()>1 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)>0,TEXT("run_level_and_mastery_grow"));
		Check(Town->GetAvailableTraitPoints()==0 && Tree->GetAvailableGrowthPoints()==State->GetPlayerLevel()-1,TEXT("run_points_do_not_fund_lobby_upgrades"));
		Check(Saves->SaveNow(),TEXT("active_run_snapshot_saved"));
		Slot=Saves->GetSaveSlotForProfile(ProfileId);
		Seed.Reset(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Seed.IsValid() && Seed->SaveVersion==4 && Seed->RunId.IsValid(),TEXT("v4_slot_has_explicit_run_identity"));
		if (Failures) { Finish(); return; }
		RunId=Seed->RunId;
		for (const auto& Investment:Tree->Capture().Investments) { Discovered.Add(Investment.NodeId); }
		Check(!Discovered.IsEmpty(),TEXT("mastery_seed_learns_discoverable_nodes"));
		Run->StartRunWithDuration(Mode==TEXT("Timeout")?1.f:1200.f);
		Check(Run->IsRunActive(),TEXT("actual_run_started"));
		Check(!Saves->SaveRunSettlement(false),TEXT("active_run_cannot_settle"));
		ReadOnlyFile=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Slot+TEXT(".sav"));
		Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,true),TEXT("isolated_v4_file_read_only_failure_fixture_enabled"));
		if (Failures) { Finish(); return; }
		if (Mode != TEXT("Timeout"))
		{
			if (Mode==TEXT("Defeat")) { State->SetCharacterDead(true); }
			Run->FinishRun(Mode==TEXT("Victory"));
		}
		Later(Mode==TEXT("Timeout")?1.5:0.3,[this]{CheckSettlement();});
	}
	void CheckSettlement()
	{
		const bool bVictory=Mode==TEXT("Victory");
		Check(Run->GetRunPhase()==(bVictory?ERogue10mRunPhase::Victory:ERogue10mRunPhase::Defeat),TEXT("requested_terminal_phase_reached"));
		if (Mode==TEXT("Timeout")) { Check(Run->GetRemainingRunSeconds()==0.f,TEXT("real_timer_expired")); }
		ExpectedLobby=2*FMath::Clamp(Saves->LobbyPointsPerDefeat,0,1000)+(bVictory?FMath::Clamp(Saves->LobbyVictoryBonus,0,10000):0);
		Check(!Saves->SaveRunSettlement(bVictory),TEXT("terminal_disk_failure_reported"));
		Check(!Saves->IsSaveReady() && Saves->CanRetrySave(),TEXT("failed_write_keeps_session_retryable_but_not_ready"));
		auto* GameMode=World->GetAuthGameMode<ARogue10mGameMode>();
		auto* Completion=GameMode?GameMode->FindComponentByClass<URogue10mRunCompletionComponent>():nullptr;
		Check(Completion && !Completion->ReturnToTown(),TEXT("failed_settlement_blocks_return_to_town"));
		Check(Saves->GetLobbyAvailablePoints()==ExpectedLobby,TEXT("failed_write_applies_reward_only_once_in_memory"));
		Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,false),TEXT("isolated_disk_failure_fixture_released"));
		Check(Saves->SaveRunSettlement(bVictory),TEXT("same_run_retries_successfully_after_disk_failure"));
		Check(Saves->GetLobbyAvailablePoints()==ExpectedLobby,TEXT("kill_and_victory_lobby_reward_exact"));
		for (int32 I=0;I<4;++I) { Check(Saves->SaveRunSettlement(bVictory),TEXT("settlement_retry_accepted")); }
		Check(Saves->GetLobbyAvailablePoints()==ExpectedLobby,TEXT("repeated_settlement_has_no_duplicate_reward"));
		Check(Saves->SaveNow(),TEXT("ordinary_autosave_preserves_terminal_receipt"));
		Seed.Reset(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Seed.IsValid() && Seed->bRunCompleted && Seed->RunId==RunId && Seed->LastSettledRunId==RunId,TEXT("disk_has_durable_completed_run_receipt"));
		if (Failures) { Finish(); return; }
		// Restoration fixture: a purchased, capped legacy-compatible lobby upgrade, not a simulated shop interaction.
		auto Permanent=Town->CaptureProgression();
		auto& Investment=Permanent.TraitInvestments.AddDefaulted_GetRef();
		Investment.Trait=ERogue10mTownTrait::Vitality; Investment.Rank=1; Investment.SpentPoints=1; Investment.Bonus=10.f;
		Permanent.VitalityLevel=1; Permanent.TraitMaxHealthBonus=10.f;
		Check(Town->RestoreProgression(Permanent) && Saves->SaveNow(),TEXT("permanent_upgrade_fixture_saved_with_lobby_budget"));
		Photo(TEXT("02_save_failure_gate.png"));
		Later(0.5,[this]
		{
			auto* GameMode=World->GetAuthGameMode<ARogue10mGameMode>();
			auto* Completion=GameMode?GameMode->GetRunCompletionComponent():nullptr;
			OldWorld=World;
			Check(Completion && Completion->ReturnToTown(),TEXT("successful_retry_enters_real_town_travel"));
			if (Failures) { Finish(); return; }
			Later(0.2,[this]{WaitForTown();});
		});
	}
	void WaitForTown()
	{
		for (const FWorldContext& Context:GEngine->GetWorldContexts())
		{
			UWorld* Candidate=Context.World();
			if (!Candidate || Candidate==OldWorld.Get() || Context.WorldType!=EWorldType::Game
				|| !Candidate->HasBegunPlay() || !Candidate->GetFirstPlayerController()) { continue; }
			World=Candidate; PC=Cast<ARogue10mPlayerController>(Candidate->GetFirstPlayerController());
			Run=Candidate->GetGameState<ARogue10mGameState>();
			if (!PC.IsValid() || !Run.IsValid()) { continue; }
			PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
			Check(UGameplayStatics::GetCurrentLevelName(Candidate,true)==TEXT("StartHub")
				&& Run->GetRunPhase()==ERogue10mRunPhase::WaitingToStart,TEXT("real_return_loads_hub_with_run_timer_waiting"));
			Check(!PC->IsRunResultVisible(),TEXT("prior_run_result_overlay_gone_after_real_travel"));
			ResetPawn(); return;
		}
		Later(0.2,[this]{WaitForTown();});
	}
	void ResetPawn()
	{
		Check(Character.IsValid() && State && Inventory && Combat && Tree && Town && Saves->IsSaveReady(),TEXT("fresh_pawn_restores_completed_snapshot"));
		if (Failures) { Finish(); return; }
		Check(State->GetPlayerLevel()==1 && State->GetCurrentExperience()==0 && State->GetMonsterDefeatCount(TEXT("QA.DomainsEnemy"))==0,TEXT("run_level_experience_and_kills_reset"));
		Check(Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler)==0,TEXT("run_mastery_reset"));
		Check(Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle && OnlyKnuckleWeapons(),TEXT("next_run_starts_knuckle_only"));
		Check(Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)!=nullptr,TEXT("next_run_has_primary_attack_binding"));
		Check(!Saves->SaveRunSettlement(Mode==TEXT("Victory")),TEXT("new_run_cannot_reuse_prior_settlement"));
		Check(Inventory->GetGold()==InitialGold && Inventory->GetCrystals()==InitialCrystals,TEXT("run_currency_reset_to_starter_baseline"));
		Check(!Saves->HasClaimedReward(TEXT("QA.RunReceipt")),TEXT("run_reward_receipts_reset"));
		Check(Town->GetTraitLevel(ERogue10mTownTrait::Vitality)==1 && Town->GetStatModifiers().MaxHealthBonus==10.f,TEXT("permanent_upgrade_and_stat_bonus_survive_run_reset"));
		Check(Saves->GetLobbyAvailablePoints()==ExpectedLobby-1 && Town->GetAvailableTraitPoints()==ExpectedLobby-1,TEXT("lobby_currency_and_spending_survive_reset"));
		for (FName Node:Discovered)
		{
			Check(Saves->HasDiscoveredNode(Node),TEXT("discovery_survives_reset_without_granting_mastery"));
			if (!InitialNodes.Contains(Node)) { Check(!Tree->IsNodeUnlocked(Node),TEXT("discovery_does_not_grant_previous_run_learned_node")); }
		}
		const auto RestoredCombat=Combat->CaptureCombat();
		Check(FRogue10mCombatSnapshot::StaticStruct()->CompareScriptStruct(&InitialCombat,&RestoredCombat,0),TEXT("combat_skills_and_bindings_reset_to_starter_profile"));
		Check(Saves->SaveNow(),TEXT("reset_snapshot_roundtrip_saved"));
		Seed.Reset(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
		Check(Seed.IsValid() && Seed->RunId!=RunId && !Seed->bRunCompleted && Seed->LastSettledRunId==RunId,TEXT("next_run_has_new_id_and_previous_receipt"));
		Photo(TEXT("03_lobby_upgrade_next_run.png"));
		Later(0.5,[this]{LegacyCase();});
	}
	void LegacyCase()
	{
		FRogue10mCharacterProfile LegacyProfile;
		Check(Profiles->CreateProfile(TEXT("LegacyQa"),FRogue10mCharacterAppearance(),LegacyProfile),TEXT("legacy_fixture_profile_created"));
		if (Failures || !Seed.IsValid()) { Finish(); return; }
		Seed->ProfileId=LegacyProfile.ProfileId; Seed->SaveVersion=3; Seed->Growth.Level=5;
		Seed->Growth.Experience=7.f; Seed->Growth.ExperienceToNextLevel=100.f; Seed->Inventory.Gold=55;
		Seed->SkillTree=FRogue10mSkillTreeSnapshot(); Seed->bRunCompleted=false;
		Seed->Growth.MonsterDefeats.Add(TEXT("QA.OldLegacyKills"),100);
		auto* Sword=LoadObject<URogue10mItemDataAsset>(nullptr,TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
		Check(Sword!=nullptr,TEXT("real_legacy_sword_asset_exists"));
		// Build a coherent legacy snapshot from an actual owned sword equip. Mixing the
		// Knuckle combat bindings with a Sword inventory is correctly rejected by RestoreCombat.
		int32 LegacyContainer=INDEX_NONE; FGuid LegacySwordId;
		Check(Sword && Inventory->TryAddGridItem(Sword,1,LegacyContainer,LegacySwordId)
			&& Inventory->TryEquipGridItem(LegacyContainer,LegacySwordId),TEXT("legacy_fixture_equips_real_sword_before_capture"));
		if (Failures) { Finish(); return; }
		Seed->Inventory=Inventory->CaptureInventory();
		Seed->Inventory.Gold=55;
		Seed->Combat=Combat->CaptureCombat();
		for (auto& Equipment:Seed->Inventory.LeftEquipment)
		{
			if (Equipment.Values.SlotType!=ERogue10mInventorySlotType::MainWeapon) { continue; }
			Equipment.Values.EquippedItem.ItemData=Sword; Equipment.Values.EquippedItem.WeaponType=ERogue10mWeaponType::GreatSword;
			Equipment.ItemAsset=TSoftObjectPtr<URogue10mItemDataAsset>(Sword);
		}
		Seed->Growth.WeaponType=ERogue10mWeaponType::GreatSword;
		const FString LegacySlot=Saves->GetLegacySaveSlotForProfile(LegacyProfile.ProfileId);
		const FString LegacyFile=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),LegacySlot+TEXT(".sav"));
		Check(UGameplayStatics::SaveGameToSlot(Seed.Get(),LegacySlot,0),TEXT("isolated_v3_fixture_written"));
		TArray<uint8> Before,After;
		Check(FFileHelper::LoadFileToArray(Before,*LegacyFile),TEXT("legacy_original_bytes_captured"));
		Check(Profiles->SelectProfile(LegacyProfile.ProfileId),TEXT("legacy_fixture_selected"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); GetPawn();
		Check(Saves->IsSaveReady() && State->GetPlayerLevel()==5 && Inventory->GetGold()==55
			&& Character->GetEquippedWeaponType()==ERogue10mWeaponType::GreatSword,TEXT("legacy_growth_inventory_and_sword_preserved_on_import"));
		Check(Town->GetTraitLevel(ERogue10mTownTrait::Vitality)==1 && Saves->GetLobbyAvailablePoints()==0,TEXT("legacy_purchased_upgrade_only_is_funded_no_unspent_run_conversion"));
		Check(Saves->SaveNow(),TEXT("migrated_v4_saved_separately"));
		TStrongObjectPtr<URogue10mProgressionSaveGame> Imported(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->GetSaveSlotForProfile(LegacyProfile.ProfileId),0)));
		Check(Imported.IsValid() && Imported->RewardDefeatBaseline==100,TEXT("legacy_kills_are_baselined_not_awarded_retroactively"));
		Check(FFileHelper::LoadFileToArray(After,*LegacyFile) && Before==After,TEXT("legacy_v3_bytes_unchanged_after_v4_import_and_save"));
		const FString MigratedSlot=Saves->GetSaveSlotForProfile(LegacyProfile.ProfileId);
		TStrongObjectPtr<URogue10mProgressionSaveGame> Corrupt(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(MigratedSlot,0)));
		Check(Corrupt.IsValid(),TEXT("v4_import_readable"));
		if (Corrupt.IsValid())
		{
			Saves->DetachCharacter(*Character,false); Corrupt->SaveVersion=999;
			Check(UGameplayStatics::SaveGameToSlot(Corrupt.Get(),MigratedSlot,0),TEXT("isolated_future_v4_fixture_written"));
			Check(!Saves->AttachCharacter(*Character) && !Saves->CanRetrySave(),TEXT("bad_v4_does_not_silently_fall_back_to_v3"));
			Saves->DetachCharacter(*Character,false);
			Check(FFileHelper::LoadFileToArray(After,*LegacyFile) && Before==After,TEXT("legacy_bytes_preserved_after_rejected_v4"));
		}
		Finish();
	}
	void Finish()
	{
		if (bFinished) { return; } bFinished=true;
		if (!ReadOnlyFile.IsEmpty()) { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile,false); }
		Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nmanual_play=not_performed\n"),Checks,Failures,*Mode,bRendered);
		if (!FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=RUN_DOMAINS_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World,OldWorld;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mGameState> Run;
	URogue10mCharacterProfileSubsystem* Profiles=nullptr;
	URogue10mProgressionSubsystem* Saves=nullptr;
	ARogue10mPlayerState* State=nullptr;
	URogue10mInventoryComponent* Inventory=nullptr;
	URogue10mCombatComponent* Combat=nullptr;
	URogue10mSkillTreeProgressionComponent* Tree=nullptr;
	URogue10mTownProgressionComponent* Town=nullptr;
	TStrongObjectPtr<URogue10mProgressionSaveGame> Seed;
	FGuid ProfileId,RunId;
	TArray<FName> Discovered,InitialNodes;
	FRogue10mCombatSnapshot InitialCombat;
	FString Label,Mode,Output,Images,Slot,Results,ReadOnlyFile;
	TFunction<void()> Next;
	double Deadline=0,NextAt=0;
	int32 Checks=0,Failures=0,ExpectedLobby=0,InitialGold=0,InitialCrystals=0;
	bool bFinished=false,bRendered=false;
};
static TSharedPtr<FRun> Active;
static void RunCommand()
{
	if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for (const FWorldContext& Context:GEngine->GetWorldContexts())
	{
		if (UWorld* World=Context.World(); World && World->WorldType==EWorldType::Game)
		{ Active=MakeShared<FRun>(); Active->Start(*World); return; }
	}
	UE_LOG(LogRogue10m,Error,TEXT("RESULT=RUN_DOMAINS_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestRunDomains"),TEXT("Isolated QA for fresh Knuckle, run reset, lobby budget, v3 preservation, and terminal receipts."),FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
