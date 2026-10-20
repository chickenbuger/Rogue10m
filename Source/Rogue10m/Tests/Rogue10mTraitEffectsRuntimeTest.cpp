#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
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
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitCatalogDataAsset.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// Isolated executable integration fixture: investments, actual damage, rewards,
// both consumable paths and the regeneration timer. No asset packages are edited.
namespace Rogue10mTraitEffectsTest
{
static bool SafeOverride(const FString& Value, const TCHAR* Prefix)
{
	if (!Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
	for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString ProfileSlot, SavePrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), SavePrefix);
		if (!SafeOverride(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
			|| !SafeOverride(SavePrefix, TEXT("Rogue10m_Progression_QA_")))
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=TRAIT_EFFECTS_FAILED reason=missing_bounded_QA_save_overrides"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/trait16-runtime-20261011"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		InWorld.GetTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateLambda([Self=AsShared()] { Self->Prepare(); }), 3.0f, false);
	}
private:
	void Check(bool Condition, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!Condition) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("TRAIT_EFFECTS FAIL %s"), Reason); }
	}
	bool Near(float A, float B) const { return FMath::IsNearlyEqual(A, B, 0.001f); }

	ARogue10mBasicMonster* SpawnMonster(ERogue10mMonsterRank Rank, bool TestOnly=true, int32 XPReward=10)
	{
		URogue10mMonsterDataAsset* Data = NewObject<URogue10mMonsterDataAsset>();
		Data->MonsterId = TEXT("TraitEffects_QA_Monster");
		Data->MonsterRank = Rank;
		Data->ExperienceReward = XPReward;
		Data->HealthRegenerationPerSecond = 0.0f;
		const FTransform Transform(FVector(50000.0f + 500.0f * SpawnCount++, 50000.0f, 200.0f));
		ARogue10mBasicMonster* Monster = World->SpawnActorDeferred<ARogue10mBasicMonster>(
			ARogue10mBasicMonster::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Monster) { return nullptr; }
		// Assign this transient fixture asset before BeginPlay; no CDO or content asset changes.
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Monster->GetClass(), TEXT("MonsterData")))
		{
			Property->SetObjectPropertyValue_InContainer(Monster, Data);
		}
		if (TestOnly) { Monster->MarkCombatTestInstance(); }
		UGameplayStatics::FinishSpawningActor(Monster, Transform);
		Monsters.Add(Monster);
		return Monster;
	}

	void KillMonster(bool Direct, int32 XPReward=10)
	{
		ARogue10mBasicMonster* Monster = SpawnMonster(ERogue10mMonsterRank::Normal, false, XPReward);
		Check(Monster != nullptr, TEXT("reward_monster_spawned"));
		if (Monster)
		{
			UGameplayStatics::ApplyDamage(Monster, 10000.0f, Controller.Get(),
				Direct ? static_cast<AActor*>(Character.Get()) : static_cast<AActor*>(Normal.Get()), UDamageType::StaticClass());
		}
	}

	void Prepare()
	{
		UGameInstance* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		URogue10mCharacterProfileSubsystem* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Controller = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		Check(Profiles && Controller.IsValid(), TEXT("profile_subsystem_and_controller_exist"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("TraitEffQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("isolated_character_profile_created"));
		if (Failures) { Finish(); return; }
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Attributes = Character.IsValid() ? Character->GetRogueAttributeSet() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		Traits = Character.IsValid() ? Character->GetTraitRuntimeComponent() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		GameState = World->GetGameState<ARogue10mGameState>();
		Check(State && Attributes && Town && Traits && Inventory && GameState.IsValid(), TEXT("all_trait_runtime_dependencies_exist"));
		if (Failures) { Finish(); return; }
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth();
		Growth.Level = 60;
		Growth.Experience = 0.0f;
		Check(State->RestoreGrowth(Growth), TEXT("fixture_level_restored"));
		const FRogue10mCharacterStatSnapshot Before = Character->GetCharacterStatSnapshot();
		Attributes->SetHealth(40.0f);
		Attributes->SetStamina(20.0f);
		for (int32 I=0; I<16; ++I)
		{
			const ERogue10mTownTrait Trait = static_cast<ERogue10mTownTrait>(I);
			const FRogue10mTraitDefinition Definition = Town->GetTraitDefinition(Trait);
			for (int32 Rank=0; Rank<Definition.MaxRank; ++Rank)
			{
				Check(Town->TryUpgradeTrait(Trait), TEXT("trait_rank_purchase_applies_on_character"));
			}
		}
		const FRogue10mCharacterStatSnapshot After = Character->GetCharacterStatSnapshot();
		Check(Near(Attributes->GetHealth(), 40.0f) && Near(Attributes->GetStamina(), 20.0f), TEXT("investment_never_refills_current_health_or_stamina"));
		Check(Near(After.MaxHealth-Before.MaxHealth, 50.0f) && Near(After.AttackPower-Before.AttackPower, 10.0f), TEXT("original_health_and_power_traits_preserved"));
		Check(Near(After.Defense-Before.Defense, 6.0f) && Near(After.CriticalChance-Before.CriticalChance, 0.06f)
			&& Near(After.CriticalDamageMultiplier-Before.CriticalDamageMultiplier, 0.30f), TEXT("defense_critical_probability_and_critical_multiplier_applied"));
		Check(Near(After.MaxStamina-Before.MaxStamina, 30.0f) && Near(Attributes->GetMaxStamina(), After.MaxStamina), TEXT("maximum_stamina_applied_to_GAS"));
		Check(Near(After.MoveSpeed, Before.MoveSpeed*1.09f) && Near(After.SprintSpeed, Before.SprintSpeed*1.09f)
			&& Near(Character->GetCharacterMovement()->MaxWalkSpeed, After.MoveSpeed), TEXT("walk_and_sprint_multipliers_applied"));
		Check(Near(Traits->GetDodgeStaminaCost(20.0f), 17.0f), TEXT("dodge_resource_cost_discount"));
		Attributes->SetMinDamageRatio(1.0f);
		Attributes->SetMaxDamageRatio(1.0f);
		URogue10mAttackSkillData* CriticalSkill=NewObject<URogue10mAttackSkillData>();
		CriticalSkill->Damage=0.0f;
		CriticalSkill->CriticalChanceBonus=1.0f;
		bool Critical=false;
		const float CriticalDamage=CriticalSkill->RollDamage(Attributes,Critical);
		Check(Critical && Near(CriticalDamage,Attributes->GetAttackPower()*Attributes->GetCriticalDamageMultiplier()),
			TEXT("actual_skill_damage_roll_uses_invested_critical_multiplier"));
		Normal = SpawnMonster(ERogue10mMonsterRank::Normal);
		Boss = SpawnMonster(ERogue10mMonsterRank::MidBoss);
		Check(Normal.IsValid() && Boss.IsValid(), TEXT("normal_and_ranked_boss_fixture_spawned"));
		if (Failures) { Finish(); return; }
		GameState->StartRunWithDuration(1200.0f);
		Traits->ResetRunState();
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal, 0), 1.0f)
			&& Near(Traits->GetAttackDamageMultiplier(*Boss, 0), 1.12f), TEXT("boss_damage_bonus_excludes_normal_rank"));
		Traits->NotifyAttackHit(1, *Normal);
		Traits->NotifyAttackHit(1, *Normal);
		Check(Traits->GetComboHitCount()==1, TEXT("multi_hit_execution_receives_single_combo_credit"));
		Traits->NotifyAttackHit(2, *Boss);
		Check(Traits->GetComboHitCount()==1, TEXT("target_change_resets_combo"));
		Traits->NotifyAttackMissed(3);
		Check(Traits->GetComboHitCount()==0, TEXT("miss_resets_combo"));
		Traits->ResetRunState();
		Traits->NotifyAttackHit(1,*Normal);
		Traits->NotifyAttackHit(3,*Normal);
		Traits->NotifyAttackMissed(2);
		Traits->NotifyAttackHit(4,*Normal);
		Check(Traits->GetComboHitCount()==1 && Near(Traits->GetAttackDamageMultiplier(*Normal,0),1.0f),
			TEXT("delayed_older_miss_breaks_chain_in_confirmed_result_order"));
		Traits->ResetRunState();
		Traits->NotifyAttackHit(10,*Normal);
		Traits->NotifyAttackHit(12,*Normal);
		Traits->NotifyAttackHit(11,*Normal);
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal,0),1.12f),
			TEXT("delayed_hits_credit_distinct_executions_in_confirmed_result_order"));
		Traits->ResetRunState();
		Traits->NotifyAttackHit(4, *Normal);
		Traits->NotifyAttackHit(5, *Normal);
		Traits->NotifyAttackHit(6, *Normal);
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal, 0), 1.12f), TEXT("three_distinct_executions_enable_combo_bonus"));
		Traits->NotifyAttackHit(7, *Normal);
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal, 0), 1.12f), TEXT("combo_buff_does_not_stack"));
		Traits->ResetRunState();
		Traits->NotifyDodgeStarted();
		const float DodgeBonus = Traits->ConsumeDodgeCounterForAttack();
		Check(Near(DodgeBonus, 0.18f) && Near(Traits->ConsumeDodgeCounterForAttack(), 0.0f), TEXT("dodge_counter_consumed_by_first_accepted_execution"));
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal, DodgeBonus), 1.18f)
			&& Near(Traits->GetAttackDamageMultiplier(*Normal, DodgeBonus), 1.18f), TEXT("captured_dodge_bonus_applies_to_entire_multi_hit_execution"));
		Traits->NotifyDodgeStarted();
		Check(Near(Traits->ConsumeDodgeCounterForAttack(), 0.0f), TEXT("repeated_dodge_cannot_bypass_shared_cooldown"));
		Traits->ResetRunState();
		int32 Gold=0;
		for (int32 I=0; I<20; ++I) { Gold += Traits->CalculateMonsterGoldReward(1); }
		Check(Gold==23, TEXT("small_gold_rewards_accumulate_fractional_fifteen_percent_bonus"));
		TestPotions();
		Traits->ResetRunState();
		Attributes->SetHealth(20.0f);
		const int32 GoldBefore = Inventory->GetGold();
		const float XPBefore = State->GetCurrentExperience();
		KillMonster(true);
		Check(Near(Attributes->GetHealth(), 26.0f) && Inventory->GetGold()==GoldBefore+1
			&& Near(State->GetCurrentExperience(),XPBefore+10.0f), TEXT("direct_kill_recovers_and_gold_trait_never_multiplies_XP"));
		KillMonster(true);
		Check(Near(Attributes->GetHealth(), 26.0f), TEXT("multiple_kills_share_two_second_recovery_cooldown"));
		KillMonster(false);
		Check(Near(Attributes->GetHealth(), 26.0f), TEXT("indirect_kill_does_not_reward_direct_kill_healing"));
		const int32 BeforeZeroGold=Inventory->GetGold();
		const float BeforeZeroXP=State->GetCurrentExperience();
		KillMonster(true,0);
		Check(Inventory->GetGold()==BeforeZeroGold && Near(State->GetCurrentExperience(),BeforeZeroXP), TEXT("zero_XP_monster_remains_without_currency_reward"));
		if (URogue10mVitalRegenerationComponent* Regen=Character->FindComponentByClass<URogue10mVitalRegenerationComponent>())
		{
			Regen->ConfigureRegeneration(0.0f, 0.0f, 0.0f);
		}
		StaminaBeforeTimer = Attributes->GetStamina();
		Traits->NotifyAttackHit(10,*Normal);
		Traits->NotifyAttackHit(11,*Normal);
		Traits->NotifyAttackHit(12,*Normal);
		Traits->NotifyDodgeStarted();
		World->GetTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateLambda([Self=AsShared()] { Self->AfterCooldown(); }), 2.1f, false);
	}

	void TestPotions()
	{
		URogue10mItemDataAsset* Potion = LoadObject<URogue10mItemDataAsset>(nullptr,
			TEXT("/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.DA_Item_HealthPotion"));
		Check(Potion && Potion->RestoreHealth>0, TEXT("actual_potion_data_asset_exists"));
		if (!Potion) { return; }
		int32 Container=INDEX_NONE; FGuid ItemId;
		Attributes->SetHealth(1.0f);
		Check(Inventory->TryAddGridItem(Potion, 1, Container, ItemId)
			&& Inventory->TryUseGridConsumable(Container, ItemId), TEXT("grid_consumable_actual_use_succeeds"));
		Check(Near(Attributes->GetHealth(), FMath::Min(Attributes->GetMaxHealth(), 1.0f+Potion->RestoreHealth*1.15f)), TEXT("grid_potion_healing_scaled_and_capped"));
		FRogue10mInventorySnapshot Snapshot=Inventory->CaptureInventory();
		int32 Empty=INDEX_NONE;
		for (int32 I=0; I<Snapshot.ItemSlots.Num(); ++I) { if (!Snapshot.ItemSlots[I].Values.bOccupied) { Empty=I; break; } }
		Check(Empty!=INDEX_NONE, TEXT("legacy_inventory_has_fixture_slot"));
		if (Empty==INDEX_NONE) { return; }
		FRogue10mSavedItemStack& Saved=Snapshot.ItemSlots[Empty];
		Saved.Values=FRogue10mItemStack(); Saved.Values.InstanceId=FGuid::NewGuid();
		Saved.Values.ItemData=Potion; Saved.Values.Category=Potion->Category; Saved.Values.EquipSlotType=Potion->EquipSlotType;
		Saved.Values.Quantity=1; Saved.Values.RestoreHealth=Potion->RestoreHealth; Saved.ItemAsset=Potion;
		Check(Inventory->RestoreInventory(Snapshot) && Inventory->AssignConsumableToQuickSlot(Empty,0), TEXT("legacy_quick_slot_potion_fixture_assigned"));
		Attributes->SetHealth(1.0f);
		Check(Inventory->UseConsumableQuickSlot(0), TEXT("legacy_quick_slot_potion_actual_use_succeeds"));
		Check(Near(Attributes->GetHealth(), FMath::Min(Attributes->GetMaxHealth(), 1.0f+Potion->RestoreHealth*1.15f)), TEXT("legacy_potion_healing_scaled_and_capped"));
	}

	void AfterCooldown()
	{
		Check(Attributes->GetStamina()>=StaminaBeforeTimer+6.0f-0.001f
			&& Attributes->GetStamina()<=StaminaBeforeTimer+9.0f+0.001f, TEXT("stamina_trait_recovery_applies_on_existing_timer"));
		const float Before=Attributes->GetHealth();
		KillMonster(true);
		Check(Near(Attributes->GetHealth(), Before+6.0f), TEXT("kill_recovery_becomes_available_after_two_seconds"));
		Check(Near(Traits->ConsumeDodgeCounterForAttack(),0.0f), TEXT("dodge_counter_expires_after_two_seconds"));
		World->GetTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateLambda([Self=AsShared()] { Self->AfterLongCooldown(); }), 6.1f, false);
	}

	void AfterLongCooldown()
	{
		Check(Near(Traits->GetAttackDamageMultiplier(*Normal,0.0f),1.0f), TEXT("combo_buff_expires_after_three_seconds"));
		Traits->NotifyDodgeStarted();
		Check(Near(Traits->ConsumeDodgeCounterForAttack(),.18f), TEXT("dodge_counter_available_after_eight_seconds"));
		Traits->ResetRunState();
		Attributes->SetHealth(10.0f);
		UGameplayStatics::ApplyDamage(Character.Get(), 10000.0f, nullptr, Normal.Get(), UDamageType::StaticClass());
		Check(!Character->IsDead() && Traits->HasUsedLastStand()
			&& Near(Attributes->GetHealth(), Attributes->GetMaxHealth()*0.25f), TEXT("actual_lethal_damage_consumes_last_stand_and_restores_quarter_health"));
		Check(!Traits->TryPreventLethalDamage(10000.0f, Normal.Get()), TEXT("last_stand_cannot_repeat_in_same_run"));
		GameState->FinishRun(true);
		GameState->StartRunWithDuration(1200.0f);
		Check(GameState->GetRunPhase() == ERogue10mRunPhase::Victory && Traits->HasUsedLastStand(),
			TEXT("completed_run_cannot_restart_and_reset_last_stand_in_same_world"));
		// A new exploration now starts after actual hub travel; its pawn calls this reset in BeginPlay.
		Traits->ResetRunState();
		Check(!Traits->HasUsedLastStand(), TEXT("new_run_reset_clears_last_stand_state"));
		Check(!Traits->TryPreventLethalDamage(10000.0f,nullptr), TEXT("forced_sourceless_damage_cannot_trigger_last_stand"));
		Character->Die();
		Check(Character->IsDead() && !Traits->HasUsedLastStand(), TEXT("forced_death_bypasses_unused_last_stand"));
		Finish();
	}

	void Finish()
	{
		for (const TWeakObjectPtr<ARogue10mBasicMonster>& Monster : Monsters) { if (Monster.IsValid()) { Monster->Destroy(); } }
		Results+=FString::Printf(TEXT("failures=%d\n"),Failures);
		if (!Output.IsEmpty()) { Check(FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("effects-results.txt"))),TEXT("evidence_written")); }
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=TRAIT_EFFECTS_%s failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Failures,*Output);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mGameState> GameState;
	TWeakObjectPtr<ARogue10mBasicMonster> Normal, Boss;
	TArray<TWeakObjectPtr<ARogue10mBasicMonster>> Monsters;
	ARogue10mPlayerState* State=nullptr;
	URogue10mAttributeSet* Attributes=nullptr;
	URogue10mTownProgressionComponent* Town=nullptr;
	URogue10mTraitRuntimeComponent* Traits=nullptr;
	URogue10mInventoryComponent* Inventory=nullptr;
	FTimerHandle Timer;
	FString Output, Results;
	int32 Failures=0, SpawnCount=0;
	float StaminaBeforeTimer=0.0f;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestTraitEffects"),
	TEXT("Runs trait effects with bounded isolated QA save overrides in a standalone game world."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor) { return; }
		for (const FWorldContext& Context:GEngine->GetWorldContexts())
		{
			if (UWorld* World=Context.World(); World && World->WorldType==EWorldType::Game)
			{
				Active=MakeShared<FRun>(); Active->Start(*World); return;
			}
		}
	}));
}
#endif
