#if WITH_EDITOR
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
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
#include "Rogue10mHybridSkillTreeWidget.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

/** Exercises the real damage adapter with explicit executions, not a synthetic XP counter. */
struct FRogue10mHybridCombatTestAccess
{
	static bool Hit(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill,
		FRogue10mActiveAttackExecution& Execution, AActor& Target)
	{
		return Combat.ApplyAttackDamage(Skill, Execution, Target, FVector::ForwardVector, 1);
	}
};

namespace Rogue10mHybridRuntime
{
using ETrack = ERogue10mSkillTreeTrack;
using EIdentity = ERogue10mSkillTreeIdentity;
using ERoute = ERogue10mSkillTreeUnlockRoute;
static bool SafeFlag(const TCHAR* Key, const TCHAR* Prefix)
{
	FString Value;
	if (!FParse::Value(FCommandLine::Get(), Key, Value) || !Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
	for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		bRead = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridRead"));
		if (!SafeFlag(TEXT("RogueProfileSaveSlot="), TEXT("Rogue10m_Town_QA_"))
			|| !SafeFlag(TEXT("RogueProgressionSavePrefix="), TEXT("Rogue10m_Progression_QA_"))
			|| !SafeFlag(TEXT("RogueSkillKeySaveSlot="), TEXT("Rogue10m_Fighter_QA_")))
		{
			Check(false, TEXT("all_three_isolated_save_flags_required")); Finish(); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/hybrid-skill-tree-runtime-20261020"));
		Later(3.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->Done) { return false; }
			if (!Self->World.IsValid()) { Self->Check(false, TEXT("world_lost")); Self->Finish(); return false; }
			if (FPlatformTime::Seconds() >= Self->NextTime && Self->Next) { auto F = MoveTemp(Self->Next); F(); }
			return !Self->Done;
		}), 0.03f);
	}
private:
	void Check(bool Passed, const TCHAR* Message)
	{
		Report += FString::Printf(TEXT("%s %s\n"), Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
		UE_LOG(LogRogue10m, Display, TEXT("HYBRID_%s %s %s"), bRead ? TEXT("READ") : TEXT("RUNTIME"), Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
		if (!Passed) { ++Failures; }
	}
	void Later(double Seconds, TFunction<void()> F) { NextTime = FPlatformTime::Seconds() + Seconds; Next = MoveTemp(F); }
	bool Click(const TCHAR* Name)
	{
		UButton* Button = Widget.IsValid() ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
		FWidgetPath FocusPath;
		if (!Button || !Button->GetIsEnabled() || !FSlateApplication::Get().GeneratePathToWidgetUnchecked(Button->TakeWidget(), FocusPath))
		{ Check(false, TEXT("enabled_real_button_path")); return false; }
		const FVector2D Point = Button->GetCachedGeometry().GetAbsolutePositionAtCoordinates({0.5, 0.5});
		auto Path = FSlateApplication::Get().LocateWindowUnderMouse(Point, TArray<TSharedRef<SWindow>>{FocusPath.GetWindow()}, false, 0);
		const bool Hit = Path.IsValid() && Path.ContainsWidget(&Button->TakeWidget().Get());
		if (!Hit) { UE_LOG(LogRogue10m, Warning, TEXT("HYBRID_RUNTIME button outside hit path: %s point=%s size=%s"),
			Name, *Point.ToString(), *Button->GetCachedGeometry().GetLocalSize().ToString()); }
		Check(Hit, TEXT("real_mouse_hit_reaches_button")); if (!Hit) { return false; }
		// Slate updates SButton's hover state on pointer movement; a physical click
		// includes that movement before down/up. Match the existing UI QA helpers.
		FSlateApplication::Get().RoutePointerMoveEvent(Path,
			FPointerEvent(0, Point, Point, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		FSlateApplication::Get().RoutePointerDownEvent(Path, FPointerEvent(0, Point, Point, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		FSlateApplication::Get().RoutePointerUpEvent(Path, FPointerEvent(0, Point, Point, TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		return true;
	}
	void Shot(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output, Name), true, false); }
	void Prepare()
	{
		PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		if (bRead) { PrepareRead(Profiles); return; }
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("HybridTreeQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("new_profile_created"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Player = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Tree.IsValid() && NPC.IsValid() && Player.IsValid(), TEXT("player_tree_and_town_npc_exist"));
		if (Failures) { Finish(); return; }
		Check(Player->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) != nullptr,
			TEXT("new_profile_keeps_playable_primary_binding"));
		// The selected appearance's inventory may start with a greatsword. Select
		// the fighter explicitly so this fixture does not depend on that content.
		Player->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
		Check(Tree->GetActiveTrack() == ETrack::Brawler, TEXT("fighter_fixture_selects_unarmed_track"));
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 1 && Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5,
			TEXT("initial_M1_five_basic_skills"));
		ExerciseTemporaryBindings();
		Player->SetCanBeDamaged(false);
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 5; Growth.Experience = 0;
		Check(State->RestoreGrowth(Growth), TEXT("level5_four_shared_points_fixture"));
		Check(Tree->CanChangeInvestments(), TEXT("near_npc_allows_tree_investment"));
		ExerciseDamage();
		Check(State->GetTownProgressionComponent()->TryUpgradeTrait(ERogue10mTownTrait::Power), TEXT("trait_spends_one_shared_point"));
		Check(Tree->GetAvailableGrowthPoints() == 3, TEXT("tree_balance_reflects_trait_spending"));
		for (int32 I = 0; I < 385 && Tree->GetMasteryTotalExperience(ETrack::Brawler) < 385; ++I) { Tree->NotifyConfirmedAttack(ETrack::Brawler); }
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 12 && Tree->IsNodeUnlocked(TEXT("F.DiamondStrike")), TEXT("actual_M12_auto_technique_unlock"));
		Check(Tree->TryUnlockNode(TEXT("I.InternalEnergy.Entry"), ERoute::Points, NAME_None), TEXT("inner_entry_one_point"));
		Check(!Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None), TEXT("brawler_mana_purchase_rejected"));
		Check(Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Points, NAME_None)
			&& Tree->GetAvailableGrowthPoints() == 0
			&& Tree->RefundNode(TEXT("I.InternalEnergy.Advanced"))
			&& Tree->GetAvailableGrowthPoints() == 2 && Tree->IsIdentityLearned(EIdentity::InternalEnergy),
			TEXT("paid_inner_deepening_refund_returns_two_points_keeps_entry"));
		PC->ToggleSkillTree();
		for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
		{ if (It->GetWorld() == World && It->GetOwningPlayer() == PC) { Widget = *It; break; } }
		Check(Widget.IsValid() && Widget->GetVisibleNodeCount() > 25 && PC->IsAnyBlockingWindowVisible(),
			TEXT("real_K_window_native_radial_tree"));
		if (Failures) { Finish(); return; }
		Widget->HandleAction(100, TEXT("I.InternalEnergy.Advanced"));
		Later(0.7, [this]
		{
			Click(TEXT("HybridTreeLearnMastery"));
			Check(Tree->IsIdentityDeepened(EIdentity::InternalEnergy) && Tree->GetSpentSkillTreePoints() == 1,
				TEXT("real_UI_free_mastery_deepening"));
			Widget->HandleAction(100, TEXT("I.InternalEnergy.Choice"));
			Widget->HandleAction(6, NAME_None, TEXT("InternalEnergy.Release"));
			Later(0.35, [this]
			{
				Click(TEXT("HybridTreeLearnPoints"));
				Check(Tree->GetSelectedProperty(EIdentity::InternalEnergy) == TEXT("InternalEnergy.Release")
					&& Tree->GetAvailableGrowthPoints() == 0, TEXT("UI_property_spends_remaining_two_points"));
				Widget->HandleAction(100, TEXT("F.DiamondStrike"));
				Check(Widget->SetSelectedSkillKey(EKeys::F7), TEXT("learned_technique_key_assignment"));
				Widget->HandleAction(100, TEXT("F.Ultimate"));
				Check(!Widget->SetSelectedSkillKey(EKeys::F8), TEXT("locked_ultimate_key_rejected"));
				Shot(TEXT("brawler-1080p.png"));
				Later(0.6, [this] { GreatSword(); });
			});
		});
	}
	void PrepareRead(URogue10mCharacterProfileSubsystem* Profiles)
	{
		// Never create, select, migrate or inspect an unrelated profile in this phase.
		// The three command-line overrides were validated before reaching this point.
		const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 1
			&& Selected && Selected->CharacterName == TEXT("HybridTreeQa"), TEXT("read_existing_selected_isolated_HybridTreeQa_profile"));
		if (Failures) { Finish(); return; }
		ReadProfileId = Selected->ProfileId;
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		const FString Slot = Persistence ? Persistence->GetSaveSlotForProfile(ReadProfileId) : FString();
		Check(Persistence && Slot.StartsWith(TEXT("Rogue10m_Progression_QA_"))
			&& UGameplayStatics::DoesSaveGameExist(Slot, 0), TEXT("read_requires_existing_QA_progression_file"));
		if (Failures) { Finish(); return; }
		// Exercise the normal spawn/Possess/AttachCharacter route. No direct snapshot
		// restoration or manufactured mastery/trait state is used by the read phase.
		PC->EnterSelectedCharacter();
		PC->CloseAllBlockingPanels();
		Later(0.6, [this] { VerifyRead(); });
	}
	void VerifyRead()
	{
		Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Player.IsValid() && State.IsValid() && Tree.IsValid() && Persistence
			&& Persistence->IsAttachedTo(*Player) && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == ReadProfileId, TEXT("read_normal_AttachCharacter_completed_all_restore_domains"));
		if (Failures) { Finish(); return; }
		const FString Slot = Persistence->GetSaveSlotForProfile(ReadProfileId);
		Check(Slot.StartsWith(TEXT("Rogue10m_Progression_QA_")), TEXT("read_disk_comparison_stays_in_QA_namespace"));
		if (Failures) { Finish(); return; }
		const auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		Check(Saved && Saved->ProfileId == ReadProfileId && Saved->SaveVersion == 3, TEXT("read_disk_profile_guid_and_v3_match"));
		if (Failures) { Finish(); return; }
		Check(State->GetPlayerLevel() == 15 && State->GetPlayerLevel() == Saved->Growth.Level
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword, TEXT("read_growth_LV15_and_greatsword_restored"));
		const auto Inventory = Player->GetInventoryComponent()->CaptureInventory();
		Check(Inventory.Gold == Saved->Inventory.Gold && Inventory.Crystals == Saved->Inventory.Crystals
			&& Inventory.LeftEquipment.Num() == Saved->Inventory.LeftEquipment.Num()
			&& Inventory.RightEquipment.Num() == Saved->Inventory.RightEquipment.Num()
			&& Inventory.Containers.Num() == Saved->Inventory.Containers.Num(), TEXT("read_inventory_currency_and_equipment_container_domains_restored"));
		Check(State->GetTownProgressionComponent()->GetTraitLevel(ERogue10mTownTrait::Power) == 1
			&& State->GetTownProgressionComponent()->GetSpentTraitPoints() == 1, TEXT("read_trait_power_rank1_actual_cost1_restored"));
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 12 && Tree->GetMasteryTotalExperience(ETrack::Brawler) == 385
			&& Tree->GetMasteryLevel(ETrack::GreatSword) == 20 && Tree->GetMasteryTotalExperience(ETrack::GreatSword) == 1045,
			TEXT("read_brawler_M12_and_greatsword_M20_with_exact_XP_restored"));
		Check(Tree->GetSpentSkillTreePoints() == 8 && Tree->GetAvailableGrowthPoints() == 5,
			TEXT("read_tree_actual_cost8_and_shared_balance5_restored"));
		Check(Tree->GetActiveIdentity() == EIdentity::Mana && Tree->GetSelectedProperty(EIdentity::Mana) == TEXT("Mana.Spread")
			&& Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana), TEXT("read_active_mana_spread_and_ultimate_resonance_restored"));
		auto* Combat = Player->GetCombatComponent();
		Check(Combat && Combat->GetActiveSkillTreeSkills().Num() == 10 && Combat->GetUnlockedWeaponSkills().Num() == 10
			&& Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary), TEXT("read_ten_greatsword_attacks_and_playable_primary_restored"));
		const auto* DiamondNode = Tree->FindNode(TEXT("F.DiamondStrike"));
		auto* Diamond = DiamondNode ? DiamondNode->Skill.LoadSynchronous() : nullptr;
		auto* Keys = PC->GetSkillBindingComponent();
		Check(Keys && Diamond && Keys->GetSkillKey(Diamond) == EKeys::F7, TEXT("read_F_DiamondStrike_F7_key_restored_from_isolated_key_save"));
		const auto* IdentityCombat = Player->GetSkillTreeCombatComponent();
		Check(IdentityCombat && IdentityCombat->GetGauge() == 0.0f && IdentityCombat->GetCooldownRemaining() == 0.0f,
			TEXT("read_transient_identity_gauge_and_cooldown_start_empty"));
		Finish();
	}
	void ExerciseTemporaryBindings()
	{
		auto* Combat = Player->GetCombatComponent();
		const auto Baseline = Combat->CaptureCombat();
		auto* Primary = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		auto* Ultimate = Tree->FindNode(TEXT("F.Ultimate"))->Skill.LoadSynchronous();
		Combat->UnlockAllActiveTreeForTesting();
		Check(Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Primary),
			TEXT("temporary_test_skill_can_be_previewed"));
		const auto Preview = Combat->CaptureCombat();
		Check(Preview.SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary
			&& !Preview.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Ultimate))
			&& Tree->GetMasteryTotalExperience(ETrack::Brawler) == 0,
			TEXT("test_preview_preserves_normal_primary_without_unlock_or_XP_pollution"));
		Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Special);
		Check(Combat->CaptureCombat().SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary,
			TEXT("moving_test_preview_preserves_normal_primary"));
		Combat->UnassignSkillFromInputSlot(ERogue10mAttackInputSlot::Primary);
		Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Primary);
		Check(!Combat->CaptureCombat().SkillBindings.Contains(ERogue10mAttackInputSlot::Primary),
			TEXT("explicitly_empty_slot_remains_empty_under_test_preview"));
		Check(Combat->RestoreCombat(Baseline) && Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) == Primary
			&& !Combat->IsAttackSkillUnlocked(Ultimate), TEXT("restore_clears_test_unlocks_and_restores_normal_loadout"));
	}
	ARogue10mBasicMonster* Monster(bool Training = false)
	{
		auto* Result = World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),
			FTransform(Player->GetActorLocation() + FVector(0, 600, 0)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		auto* Data = NewObject<URogue10mMonsterDataAsset>(Result); Data->MaxHealth = 10000; Data->ExperienceReward = 1;
		Data->bDestroyOnDeath = false; Data->DetectionRange = 0; Data->AttackDamage = 0;
		auto* Property = FindFProperty<FObjectPropertyBase>(Result->GetClass(), TEXT("MonsterData"));
		Property->SetObjectPropertyValue_InContainer(Result, Data);
		if (Training) { Result->MarkCombatTestInstance(); }
		Result->FinishSpawning(FTransform(Result->GetActorLocation()));
		if (auto* AI = Result->GetController()) { AI->UnPossess(); }
		Result->ClearAITarget();
		return Result;
	}
	void ExerciseDamage()
	{
		auto* A = Monster(); auto* B = Monster(); auto* Dummy = Monster(true);
		auto* Skill = NewObject<URogue10mAttackSkillData>(Player.Get());
		Skill->Damage = 1; Skill->HitMode = ERogue10mAttackHitMode::MultiHit; Skill->HitCount = 3;
		FRogue10mActiveAttackExecution Execution; Execution.ExecutionId = 9001; Execution.MasteryTrack = ETrack::Brawler;
		auto& Combat = *Player->GetCombatComponent();
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *A);
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *A);
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *B);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("three_hits_two_targets_one_mastery_XP"));
		FRogue10mActiveAttackExecution DummyExecution; DummyExecution.ExecutionId = 9002; DummyExecution.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, DummyExecution, *Dummy);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("combat_training_target_no_XP"));
		A->SetCanBeDamaged(false);
		FRogue10mActiveAttackExecution Immune; Immune.ExecutionId = 9003; Immune.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Immune, *A);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("zero_applied_damage_no_XP"));
		B->GetRogueAttributeSet()->SetHealth(1);
		FRogue10mActiveAttackExecution Killing; Killing.ExecutionId = 9004; Killing.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Killing, *B);
		Check(B->IsDead() && Tree->GetMasteryTotalExperience(ETrack::Brawler) == 2, TEXT("killing_hit_awards_XP_once"));
		A->Destroy(); B->Destroy(); Dummy->Destroy();
	}
	void GreatSword()
	{
		Widget->HandleAction(2);
		Check(Player->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed, TEXT("browsing_greatsword_never_equips_weapon"));
		Shot(TEXT("greatsword-locked-1080p.png"));
		Later(0.6, [this]
		{
			PC->CloseAllBlockingPanels();
			Player->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
			Check(Player->GetCombatComponent()->GetActiveSkillTreeSkills().Num() == 10
				&& Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5, TEXT("greatsword_five_basics_five_locked_techniques"));
			Check(Tree->GetMasteryLevel(ETrack::GreatSword) == 1 && Tree->GetMasteryLevel(ETrack::Brawler) == 12,
				TEXT("separate_mastery_tracks"));
			Check(Tree->GetActiveIdentity() == EIdentity::None, TEXT("inner_energy_inactive_with_greatsword"));
			FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 15;
			State->RestoreGrowth(Growth);
			auto Snapshot = Tree->Capture(); FRogue10mSkillTreeMasteryRecord Row; Row.Track = ETrack::GreatSword; Row.TotalExperience = 1045;
			Snapshot.Masteries.Add(Row);
			Check(Tree->Restore(Snapshot), TEXT("M20_greatsword_fixture_restore_and_auto_rewards"));
			Check(Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 10, TEXT("all_five_greatsword_core_attacks_granted"));
			Check(Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread"))
				&& Tree->TrySetActiveIdentity(EIdentity::Mana), TEXT("greatsword_mana_hybrid_investment_and_activation"));
			Check(Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana), TEXT("core_and_property_both_required_for_resonance"));
			const int32 BeforeRefund = Tree->GetSpentSkillTreePoints();
			Check(Tree->RefundNode(TEXT("I.Mana.Advanced")) && Tree->IsIdentityLearned(EIdentity::Mana)
				&& !Tree->IsIdentityDeepened(EIdentity::Mana) && Tree->GetSelectedProperty(EIdentity::Mana).IsNone()
				&& !Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana)
				&& Tree->GetSpentSkillTreePoints() == BeforeRefund - 4, TEXT("advanced_refund_keeps_entry_removes_choice_and_resonance_returns_actual_cost"));
			Check(!Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Mastery, NAME_None),
				TEXT("mana_deepening_remains_points_only_at_M20"));
			Check(Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread")),
				TEXT("refunded_mana_deepening_and_choice_can_be_repurchased"));
			IdentityAndSave();
			PC->ToggleSkillTree(); Widget->HandleAction(2); Widget->HandleAction(100, TEXT("G.Ultimate"));
			PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
			Later(0.7, [this] { Shot(TEXT("greatsword-720p.png")); Later(0.6, [this] { Finish(); }); });
		});
	}
	void IdentityAndSave()
	{
		auto* CombatIdentity = Player->GetSkillTreeCombatComponent();
		auto* Ultimate = Tree->FindNode(TEXT("G.Ultimate"))->Skill.LoadSynchronous();
		for (int32 I = 0; I < 4; ++I) { CombatIdentity->NotifyConfirmedAttack(ETrack::GreatSword); }
		State->GetRogueAttributeSet()->SetMana(0);
		Check(!CombatIdentity->AcceptAttack(*Ultimate, false).bTriggered && CombatIdentity->GetGauge() == 100,
			TEXT("mana_proc_requires_MP_preserves_gauge_when_short"));
		State->GetRogueAttributeSet()->SetMana(100);
		auto Effect = CombatIdentity->AcceptAttack(*Ultimate, false);
		Check(Effect.bTriggered && Effect.DamageMultiplier > 1.1f && Effect.RangeMultiplier > 1
			&& State->GetRogueAttributeSet()->GetMana() == 95, TEXT("mana_core_resonance_consumes_MP_and_enlarges_actual_attack"));
		const float Cooldown = CombatIdentity->GetCooldownRemaining();
		Check(Tree->TrySetActiveIdentity(EIdentity::None) && Tree->TrySetActiveIdentity(EIdentity::Mana)
			&& CombatIdentity->GetGauge() == 0 && CombatIdentity->GetCooldownRemaining() >= Cooldown - .1f,
			TEXT("identity_switch_cannot_refill_or_reset_cooldown"));
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Persistence->SaveNow(), TEXT("actual_isolated_profile_save"));
		auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(
			Persistence->GetSaveSlotForProfile(Persistence->GetAttachedProfileId()), 0));
		Check(Saved && Saved->SaveVersion == 3 && Saved->SkillTree.Masteries.Num() == 2
			&& Saved->SkillTree.ActiveIdentity == EIdentity::Mana, TEXT("disk_roundtrip_v3_mastery_identity_ledger"));
		const float Health = State->GetRogueAttributeSet()->GetHealth();
		Check(Tree->Respec(EIdentity::Mana) && Tree->GetMasteryLevel(ETrack::GreatSword) == 20
			&& !Tree->IsIdentityLearned(EIdentity::Mana) && !Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana)
			&& State->GetRogueAttributeSet()->GetHealth() <= Health, TEXT("respec_removes_descendants_preserves_mastery_no_healing"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(1500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Check(!Tree->CanChangeInvestments() && !Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None),
			TEXT("out_of_NPC_range_rejects_purchase"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Check(Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
			&& Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
			&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread")),
			TEXT("mana_can_be_rebuilt_after_refund_for_final_UI"));
	}
	void Finish()
	{
		if (Done) { return; }
		if (!bRead && State.IsValid() && Tree.IsValid())
		{
			auto* Persistence = World.IsValid() ? World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
			Check(Persistence && Persistence->SaveNow(), TEXT("write_final_rebuilt_identity_state_saved_before_exit"));
		}
		Done = true;
		const TCHAR* Phase = bRead ? TEXT("READ") : TEXT("RUNTIME");
		Report += FString::Printf(TEXT("RESULT=HYBRID_%s_%s failures=%d\n"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		if (!Output.IsEmpty()) { FFileHelper::SaveStringToFile(Report, *FPaths::Combine(Output, bRead ? TEXT("runtime-read-results.txt") : TEXT("runtime-results.txt"))); }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=HYBRID_%s_%s failures=%d"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Tree;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> Widget;
	TFunction<void()> Next;
	double NextTime = 0;
	int32 Failures = 0;
	bool Done = false;
	bool bRead = false;
	FGuid ReadProfileId;
	FString Output, Report;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestHybridSkillTree"),
	TEXT("Runs real skill-tree UI, damage, identity and isolated persistence checks."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor || Active) { return; }
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (auto* World = Context.World(); World && World->WorldType == EWorldType::Game)
			{ Active = MakeShared<FRun>(); Active->Start(*World); return; }
		}
	}));
}
#endif
