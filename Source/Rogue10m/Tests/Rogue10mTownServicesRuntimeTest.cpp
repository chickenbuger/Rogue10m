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
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTutorialDataAsset.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "TimerManager.h"

// Two independent -game processes exercise the public NPC and profile-spawn paths.
// Both bounded QA overrides are mandatory before any profile/runtime mutation.
namespace Rogue10mTownServicesTest
{
template <typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool SafeOverride(const FString& Value, const TCHAR* Prefix)
{
	if (!Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
	for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld, bool bInRead)
	{
		World = &InWorld;
		bRead = bInRead;
		FString ProfileSlot, ProgressionPrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		if (!SafeOverride(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
			|| !SafeOverride(ProgressionPrefix, TEXT("Rogue10m_Progression_QA_")))
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=TOWN_SERVICES_FAILED reason=missing_bounded_QA_save_overrides"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(),
			TEXT("Feature/doc/evidence/town-services-persistence-20261011"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		InWorld.GetTimerManager().SetTimer(PrepareTimer,
			FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 3.0f, false);
	}

private:
	void Check(bool bCondition, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bCondition ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bCondition) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("TOWN_SERVICES FAIL %s"), Reason); }
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
			if (Failures || !Profiles->CreateProfile(TEXT("TownQaHero"), FRogue10mCharacterAppearance(), Profile))
			{
				Check(false, TEXT("isolated_profile_created")); Finish(); return;
			}
			ProfileId = Profile.ProfileId;
			Check(Profiles->SelectProfile(ProfileId), TEXT("isolated_profile_selected"));
		}
		else
		{
			const FRogue10mCharacterProfile* Profile = Profiles->GetSelectedProfile();
			Check(Profile && Profile->CharacterName == TEXT("TownQaHero"), TEXT("selected_profile_survives_new_process"));
			if (!Profile) { Finish(); return; }
			ProfileId = Profile->ProfileId;
		}
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		Check(Character.IsValid() && State && Inventory && Combat && Town, TEXT("selected_character_game_mode_spawn_and_components"));
		Check(Character.IsValid() && Persistence->IsAttachedTo(*Character) && Persistence->IsSaveReady(),
			TEXT("selected_character_automatically_attached_and_save_ready"));
		if (Failures) { Finish(); return; }
		if (!bRead)
		{
			const FRogue10mGrowthSnapshot FreshGrowth = State->CaptureGrowth();
			const FRogue10mInventorySnapshot FreshInventory = Inventory->CaptureInventory();
			const FRogue10mCombatSnapshot FreshCombat = Combat->CaptureCombat();
			const FRogue10mTownProgressionSnapshot FreshTown = Town->CaptureProgression();
			bool bWeaponConsistent = true;
			for (const auto* Slots : { &FreshInventory.LeftEquipment, &FreshInventory.RightEquipment })
			{
				for (const FRogue10mSavedEquipmentSlot& Slot : *Slots)
				{
					if (Slot.Values.SlotType == ERogue10mInventorySlotType::MainWeapon && Slot.Values.bHasEquippedItem)
					{
						bWeaponConsistent = Slot.Values.EquippedItem.WeaponType == FreshGrowth.WeaponType;
					}
				}
			}
			Check(bWeaponConsistent, TEXT("fresh_spawn_active_weapon_matches_actual_equipped_starter_item"));
			Check(Persistence->SaveNow(), TEXT("fresh_spawn_unmodified_progression_save_succeeds"));
			Persistence->DetachCharacter(*Character, false);
			Check(Persistence->AttachCharacter(*Character) && Persistence->IsSaveReady()
				&& Same(FreshGrowth, State->CaptureGrowth()) && Same(FreshInventory, Inventory->CaptureInventory())
				&& Same(FreshCombat, Combat->CaptureCombat()) && Same(FreshTown, Town->CaptureProgression()),
				TEXT("fresh_unmodified_character_roundtrip_keeps_weapon_inventory_and_bindings_consistent"));
			if (Failures) { Finish(); return; }
		}
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(NPC.IsValid() && NPC->TutorialData && NPC->TutorialData->StartingItem, TEXT("actual_start_hub_NPC_and_supply_asset_exist"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Check(NPC->BeginDialogue(Character.Get()) && NPC->IsDialogueOpen(), TEXT("actual_NPC_dialogue_opens"));
		Check(NPC->GetTutorialWidget() && !NPC->GetTutorialWidget()->GetDisplayedServices().IsEmpty(),
			TEXT("actual_dialogue_widget_displays_town_services"));
		if (Failures) { Finish(); return; }
		if (bRead) { Read(); } else { Write(); }
	}

	void Write()
	{
		const URogue10mTownServiceDataAsset& Tuning = NPC->ServiceData ? *NPC->ServiceData : *GetDefault<URogue10mTownServiceDataAsset>();
		Check(!NPC->PurchasePotion() && Inventory->GetGold() == 0, TEXT("potion_insufficient_gold_rejected_without_charge"));
		Check(!NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon), TEXT("enhancement_insufficient_gold_rejected"));
		Check(!NPC->UpgradeTrait(ERogue10mTownTrait::Vitality) && Town->GetAvailableTraitPoints() == 0,
			TEXT("trait_without_level_points_rejected"));
		Inventory->AddGold(10000);
		const int32 GoldBeforePotion = Inventory->GetGold();
		const int32 PotionsBefore = CountItem(NPC->TutorialData->StartingItem);
		Check(NPC->PurchasePotion(), TEXT("NPC_potion_purchase_succeeds"));
		Check(Inventory->GetGold() == GoldBeforePotion - Tuning.PotionGoldCost
			&& CountItem(NPC->TutorialData->StartingItem) == PotionsBefore + 1, TEXT("potion_purchase_cost_and_quantity_match"));
		Check(NPC->ClaimStartingSupplies() && NPC->HasClaimedReward(Character.Get()), TEXT("tutorial_receipt_granted"));
		const FRogue10mInventorySnapshot BeforeFull = Inventory->CaptureInventory();
		int32 Container = INDEX_NONE;
		FGuid InstanceId;
		int32 FillCount = 0;
		while (FillCount < 1000 && Inventory->TryAddGridItem(NPC->TutorialData->StartingItem, 1, Container, InstanceId)) { ++FillCount; }
		const int32 FullGold = Inventory->GetGold();
		Check(FillCount < 1000 && !NPC->PurchasePotion() && Inventory->GetGold() == FullGold,
			TEXT("full_bag_purchase_rejected_without_charge"));
		Check(Inventory->RestoreInventory(BeforeFull), TEXT("temporary_full_bag_state_restored"));

		Check(Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon) != nullptr, TEXT("real_starter_weapon_equipped"));
		const float AttackBeforeEnhance = State->GetRogueAttributeSet()->GetAttackPower();
		const int32 GoldBeforeEnhance = Inventory->GetGold();
		for (int32 Level = 1; Level <= Tuning.MaximumEnhancementLevel; ++Level)
		{
			Check(NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon), TEXT("NPC_weapon_enhancement_succeeds"));
			Check(Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon) == Level,
				TEXT("weapon_enhancement_level_matches"));
		}
		Check(Inventory->GetGold() == GoldBeforeEnhance - Tuning.EnhancementGoldCost
			* Tuning.MaximumEnhancementLevel * (Tuning.MaximumEnhancementLevel + 1) / 2, TEXT("enhancement_progressive_gold_cost_matches"));
		Check(FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetAttackPower(), AttackBeforeEnhance
			+ Tuning.EnhancementAttackPerLevel * Tuning.MaximumEnhancementLevel), TEXT("enhancement_changes_actual_GAS_attack"));
		const int32 GoldAtCap = Inventory->GetGold();
		Check(!NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon) && Inventory->GetGold() == GoldAtCap,
			TEXT("max_enhancement_rejected_without_charge"));
		const float DefenseBefore = State->GetRogueAttributeSet()->GetDefense();
		const int32 GoldBeforeArmor = Inventory->GetGold();
		Check(NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::Armor)
			&& Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::Armor) == 1
			&& Inventory->GetGold() == GoldBeforeArmor - Tuning.EnhancementGoldCost
			&& FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetDefense(), DefenseBefore + Tuning.EnhancementDefensePerLevel),
			TEXT("NPC_armor_enhancement_charges_once_and_changes_actual_GAS_defense"));
		FGuid WeaponId;
		for (const FRogue10mInventorySlot& Slot : Inventory->GetLeftEquipmentSlots())
		{
			if (Slot.SlotType == ERogue10mInventorySlotType::MainWeapon) { WeaponId = Slot.EquippedItem.InstanceId; }
		}
		for (const FRogue10mInventorySlot& Slot : Inventory->GetRightEquipmentSlots())
		{
			if (Slot.SlotType == ERogue10mInventorySlotType::MainWeapon) { WeaponId = Slot.EquippedItem.InstanceId; }
		}
		Check(WeaponId.IsValid() && Inventory->TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType::MainWeapon),
			TEXT("enhanced_weapon_unequips_to_grid"));
		int32 WeaponContainer = INDEX_NONE;
		for (int32 Index = 0; Index < Inventory->GetInventoryContainers().Num(); ++Index)
		{
			for (const FRogue10mInventoryGridEntry& Entry : Inventory->GetInventoryContainers()[Index].Entries)
			{
				if (Entry.InstanceId == WeaponId)
				{
					WeaponContainer = Index;
					Check(Entry.EnhancementLevel == Tuning.MaximumEnhancementLevel, TEXT("enhancement_and_instance_identity_survive_unequip"));
				}
			}
		}
		Check(WeaponContainer != INDEX_NONE && Inventory->TryEquipGridItem(WeaponContainer, WeaponId)
			&& Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon) == Tuning.MaximumEnhancementLevel,
			TEXT("enhanced_weapon_re_equips_without_loss"));

		State->AddExperience(1500);
		Check(State->GetPlayerLevel() > 2 && State->GetCurrentExperience() > 0.0f, TEXT("growth_level_and_partial_XP_progress"));
		const float MaxHealthBefore = State->GetRogueAttributeSet()->GetMaxHealth();
		const float AttackBeforeTrait = State->GetRogueAttributeSet()->GetAttackPower();
		const int32 PointsBefore = Town->GetAvailableTraitPoints();
		Check(NPC->UpgradeTrait(ERogue10mTownTrait::Vitality) && NPC->UpgradeTrait(ERogue10mTownTrait::Power),
			TEXT("NPC_vitality_and_power_traits_upgrade"));
		Check(Town->GetAvailableTraitPoints() == PointsBefore - 2, TEXT("trait_points_consumed_once"));
		Check(FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetMaxHealth(), MaxHealthBefore + Tuning.VitalityHealthPerLevel)
			&& FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetAttackPower(), AttackBeforeTrait + Tuning.PowerAttackPerLevel),
			TEXT("traits_change_actual_GAS_max_health_and_attack"));
		for (int32 Level = 1; Level < Tuning.MaximumTraitLevel; ++Level)
		{
			Check(NPC->UpgradeTrait(ERogue10mTownTrait::Vitality), TEXT("vitality_can_reach_authored_cap"));
		}
		const int32 PointsAtCap = Town->GetAvailableTraitPoints();
		Check(!NPC->UpgradeTrait(ERogue10mTownTrait::Vitality) && Town->GetAvailableTraitPoints() == PointsAtCap,
			TEXT("max_trait_rejected_without_point_loss"));
		Check(NPC->SelectDifficulty(ERogue10mDifficulty::Easy) && Town->GetDifficulty() == ERogue10mDifficulty::Easy
			&& NPC->SelectDifficulty(ERogue10mDifficulty::Hard) && Town->GetDifficulty() == ERogue10mDifficulty::Hard,
			TEXT("NPC_difficulty_selection_changes_next_run"));

		int32 KnuckleSlot = INDEX_NONE;
		for (int32 Index = 0; Index < Inventory->GetItemSlots().Num(); ++Index)
		{
			const FRogue10mItemStack& Item = Inventory->GetItemSlots()[Index];
			if (Item.bOccupied && Item.Category == ERogue10mItemCategory::Equipment
				&& Item.EquipSlotType == ERogue10mInventorySlotType::MainWeapon && Item.WeaponType == ERogue10mWeaponType::Knuckle)
			{
				KnuckleSlot = Index; break;
			}
		}
		Check(KnuckleSlot != INDEX_NONE && Inventory->TryEquipItemFromSlot(KnuckleSlot)
			&& Character->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle,
			TEXT("real_knuckle_item_equipped_before_martial_progression"));
		State->RecordSkillUse(TEXT("TownQaSkill"), 17);
		State->RecordMonsterDefeat(TEXT("TownQaMonster"), 9);
		const TArray<URogue10mAttackSkillData*> Skills = Combat->GetActiveSkillTreeSkills();
		const int32 UnlocksBefore = Combat->GetUnlockedWeaponSkills().Num();
		for (const URogue10mAttackSkillData* Skill : Skills)
		{
			if (!Skill) { continue; }
			for (const FRogue10mSkillUnlockCondition& Condition : Skill->UnlockConditions)
			{
				if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount && Condition.RequiredSkill)
				{
					State->RecordSkillUse(Condition.RequiredSkill->GetFName(), Condition.RequiredCount);
				}
				else if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::MonsterDefeatCount)
				{
					State->RecordMonsterDefeat(Condition.RequiredMonsterId, Condition.RequiredCount);
				}
			}
		}
		Combat->EvaluateActiveSkillUnlocks();
		Check(Combat->GetUnlockedWeaponSkills().Num() > UnlocksBefore, TEXT("real_skill_conditions_unlock_new_martial_skills"));
		bool bAssigned = false;
		for (URogue10mAttackSkillData* Skill : Skills)
		{
			if (Skill && Skill->SkillTreeTier > 1 && Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique)
			{
				bAssigned = Combat->AssignSkillToInputSlot(Skill, ERogue10mAttackInputSlot::Special);
				if (bAssigned) { break; }
			}
		}
		Check(bAssigned, TEXT("unlocked_nondefault_skill_bound_to_special_input"));
		int32 ConsumableSlot = INDEX_NONE;
		for (int32 Index = 0; Index < Inventory->GetItemSlots().Num(); ++Index)
		{
			const FRogue10mItemStack& Item = Inventory->GetItemSlots()[Index];
			if (Item.bOccupied && !Item.bLocked && Item.Quantity > 0
				&& Item.Category == ERogue10mItemCategory::Consumable
				&& Item.ItemData == NPC->TutorialData->StartingItem)
			{
				ConsumableSlot = Index; break;
			}
		}
		Check(ConsumableSlot != INDEX_NONE && Inventory->AssignConsumableToQuickSlot(ConsumableSlot, 0)
			&& Inventory->GetConsumableQuickSlotInventoryIndex(0) == ConsumableSlot,
			TEXT("actual_health_potion_assigned_to_consumable_quick_slot"));
		Check(Inventory->MoveConsumableQuickSlot(0, 2)
			&& Inventory->GetConsumableQuickSlotInventoryIndex(0) == INDEX_NONE
			&& Inventory->GetConsumableQuickSlotInventoryIndex(2) == ConsumableSlot,
			TEXT("occupied_consumable_quick_slot_moves_before_save"));
		RejectInvalidSnapshots();
		URogue10mProgressionSaveGame* Expected = NewObject<URogue10mProgressionSaveGame>();
		Expected->ProfileId = ProfileId;
		Expected->Growth = State->CaptureGrowth();
		Expected->Inventory = Inventory->CaptureInventory();
		Expected->Combat = Combat->CaptureCombat();
		Expected->Town = Town->CaptureProgression();
		Expected->ClaimedRewards.Add(NPC->TutorialData->RewardId);
		TArray<uint8> ExpectedBytes;
		Check(UGameplayStatics::SaveGameToMemory(Expected, ExpectedBytes)
			&& FFileHelper::SaveArrayToFile(ExpectedBytes, *FPaths::Combine(Output, TEXT("expected.bin"))),
			TEXT("independent_process_expected_snapshot_written"));
		if (ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>())
		{
			Run->StartRunWithDuration(600.0f);
			const int32 RunGold = Inventory->GetGold();
			Check(!NPC->SelectDifficulty(ERogue10mDifficulty::Easy) && !NPC->UpgradeTrait(ERogue10mTownTrait::Power)
				&& !NPC->PurchasePotion() && !NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon)
				&& Inventory->GetGold() == RunGold && Town->GetDifficulty() == ERogue10mDifficulty::Hard,
				TEXT("active_run_rejects_all_town_service_mutations"));
		}
		else { Check(false, TEXT("actual_run_game_state_exists")); }
		World->GetTimerManager().SetTimer(PrepareTimer, FTimerDelegate::CreateLambda([Self = AsShared()]
		{
			const auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(
				Self->Persistence->GetSaveSlotForProfile(Self->ProfileId), 0));
			Self->Check(Saved && Same(Saved->Growth, Self->State->CaptureGrowth())
				&& Same(Saved->Inventory, Self->Inventory->CaptureInventory())
				&& Same(Saved->Combat, Self->Combat->CaptureCombat())
				&& Same(Saved->Town, Self->Town->CaptureProgression())
				&& Saved->ClaimedRewards.Contains(Self->NPC->TutorialData->RewardId),
				TEXT("event_driven_autosave_writes_complete_progression_without_explicit_save"));
			Self->Check(Self->Persistence->SaveNow(), TEXT("profile_progression_explicit_save_succeeds"));
			Self->Finish();
		}), 1.5f, false);
	}

	void RejectInvalidSnapshots()
	{
		const FRogue10mInventorySnapshot BeforeInventory = Inventory->CaptureInventory();
		FRogue10mInventorySnapshot BadInventory = BeforeInventory;
		BadInventory.Gold = -1;
		Check(!Inventory->RestoreInventory(BadInventory) && Same(BeforeInventory, Inventory->CaptureInventory()),
			TEXT("invalid_inventory_snapshot_rejected_transactionally"));
		const FRogue10mGrowthSnapshot BeforeGrowth = State->CaptureGrowth();
		FRogue10mGrowthSnapshot BadGrowth = BeforeGrowth;
		BadGrowth.Level = 0;
		Check(!State->RestoreGrowth(BadGrowth) && Same(BeforeGrowth, State->CaptureGrowth()),
			TEXT("invalid_growth_snapshot_rejected_transactionally"));
		const FRogue10mTownProgressionSnapshot BeforeTown = Town->CaptureProgression();
		FRogue10mTownProgressionSnapshot BadTown = BeforeTown;
		BadTown.VitalityLevel = -1;
		Check(!Town->RestoreProgression(BadTown) && Same(BeforeTown, Town->CaptureProgression()),
			TEXT("invalid_town_snapshot_rejected_transactionally"));
	}

	void Read()
	{
		TArray<uint8> Bytes;
		Check(FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Output, TEXT("expected.bin"))), TEXT("first_process_expected_snapshot_exists"));
		URogue10mProgressionSaveGame* Expected = Bytes.Num()
			? Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		Check(Expected && Expected->ProfileId == ProfileId, TEXT("expected_snapshot_matches_selected_profile_ID"));
		if (!Expected) { Finish(); return; }
		Check(Same(Expected->Growth, State->CaptureGrowth()), TEXT("restart_restores_level_XP_weapon_and_skill_monster_counters"));
		Check(Same(Expected->Inventory, Inventory->CaptureInventory()), TEXT("restart_restores_gold_grid_positions_instance_IDs_equipment_enhancements_quick_slots"));
		const FRogue10mItemStack RestoredQuickItem = Inventory->GetConsumableQuickSlotItem(2);
		Check(Expected->Inventory.ConsumableQuickSlots.IsValidIndex(2)
			&& Expected->Inventory.ConsumableQuickSlots[2] != INDEX_NONE
			&& Inventory->GetConsumableQuickSlotInventoryIndex(2) == Expected->Inventory.ConsumableQuickSlots[2]
			&& RestoredQuickItem.bOccupied && RestoredQuickItem.Quantity > 0
			&& RestoredQuickItem.ItemData == NPC->TutorialData->StartingItem
			&& Inventory->GetConsumableQuickSlotInventoryIndex(0) == INDEX_NONE,
			TEXT("restart_restores_nonempty_moved_health_potion_quick_slot"));
		Check(Same(Expected->Combat, Combat->CaptureCombat()), TEXT("restart_restores_unlocked_skills_and_input_bindings"));
		Check(Same(Expected->Town, Town->CaptureProgression()), TEXT("restart_restores_traits_difficulty_and_authored_modifiers"));
		Check(State->GetSkillUseCount(TEXT("TownQaSkill")) == 17 && State->GetMonsterDefeatCount(TEXT("TownQaMonster")) == 9,
			TEXT("named_progression_counters_survive_independent_process"));
		const FRogue10mInventorySnapshot BeforeClaim = Inventory->CaptureInventory();
		Check(NPC->HasClaimedReward(Character.Get()) && !NPC->ClaimStartingSupplies()
			&& Same(BeforeClaim, Inventory->CaptureInventory()), TEXT("tutorial_reward_receipt_survives_restart_without_duplicate_supply"));
		RejectInvalidSnapshots();
		NPC->CloseDialogue();
		FRogue10mCharacterProfile OtherProfile;
		FRogue10mCharacterAppearance OtherAppearance;
		Check(Profiles->CreateProfile(TEXT("TownQaOther"), OtherAppearance, OtherProfile)
			&& Profiles->SelectProfile(OtherProfile.ProfileId), TEXT("second_isolated_profile_created_and_selected"));
		Controller->EnterSelectedCharacter();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		Check(State && Inventory && Town && Persistence->GetAttachedProfileId() == OtherProfile.ProfileId
			&& State->GetPlayerLevel() == 1 && State->GetSkillUseCount(TEXT("TownQaSkill")) == 0
			&& Inventory->GetGold() == 0 && Town->GetTraitLevel(ERogue10mTownTrait::Vitality) == 0
			&& Town->GetDifficulty() == ERogue10mDifficulty::Normal
			&& !Persistence->HasClaimedReward(NPC->TutorialData->RewardId), TEXT("same_character_class_profile_progression_and_receipts_are_separated"));
		Check(Character.IsValid() && NPC->BeginDialogue(Character.Get()) && !NPC->HasClaimedReward(Character.Get())
			&& NPC->ClaimStartingSupplies() && NPC->HasClaimedReward(Character.Get()),
			TEXT("second_profile_claims_its_own_supply_without_inheriting_first_profile_receipt"));
		NPC->CloseDialogue();
		Check(Profiles->SelectProfile(ProfileId), TEXT("original_profile_reselected"));
		Controller->EnterSelectedCharacter();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		Check(State && Inventory && Combat && Town && Persistence->GetAttachedProfileId() == ProfileId
			&& Same(Expected->Growth, State->CaptureGrowth()) && Same(Expected->Inventory, Inventory->CaptureInventory())
			&& Same(Expected->Combat, Combat->CaptureCombat()) && Same(Expected->Town, Town->CaptureProgression())
			&& Persistence->HasClaimedReward(NPC->TutorialData->RewardId), TEXT("switching_back_restores_original_profile_without_other_profile_state"));
		Finish();
	}

	int32 CountItem(const URogue10mItemDataAsset* Data) const
	{
		int32 Count = 0;
		for (const FRogue10mInventoryContainer& Bag : Inventory->GetInventoryContainers())
		{
			for (const FRogue10mInventoryGridEntry& Entry : Bag.Entries) { if (Entry.ItemData == Data) { Count += Entry.Quantity; } }
		}
		return Count;
	}

	void Finish()
	{
		if (NPC.IsValid())
		{
			NPC->CloseDialogue();
			Check(!NPC->GetTutorialWidget() || NPC->GetTutorialWidget()->GetDisplayedServices().IsEmpty(),
				TEXT("closing_dialogue_clears_service_display"));
		}
		Results += FString::Printf(TEXT("mode=%s\nfailures=%d\nprofile=%s\n"), bRead ? TEXT("read") : TEXT("write"), Failures, *ProfileId.ToString());
		Check(FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, bRead ? TEXT("read-results.txt") : TEXT("write-results.txt"))),
			TEXT("runtime_evidence_written"));
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=TOWN_SERVICES_%s mode=%s failures=%d output=%s"),
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
	FString Output, Results;
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
			Active = MakeShared<FRun>();
			Active->Start(*World, bRead);
			return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=TOWN_SERVICES_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}

static FAutoConsoleCommand WriteCommand(TEXT("Rogue10m.TestTownServicesWrite"),
	TEXT("Tests actual StartHub NPC services and writes an isolated progression save for a second process."),
	FConsoleCommandDelegate::CreateLambda([] { Run(false); }));
static FAutoConsoleCommand ReadCommand(TEXT("Rogue10m.TestTownServicesRead"),
	TEXT("Verifies that a second game process restores the isolated NPC progression snapshot and receipts."),
	FConsoleCommandDelegate::CreateLambda([] { Run(true); }));
}

#endif
