#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Rogue10mAttackResourceBudget.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillTreeCombatComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include <limits>

struct FRogue10mAttackCostTestAccess
{
	static bool Execute(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{ return Combat.ExecuteAttackSkill(Skill, false); }
	static int32 Executions(const URogue10mCombatComponent& Combat) { return Combat.ActiveAttackExecutions.Num(); }
	static const FRogue10mSkillTreeAttackEffect& Effect(const URogue10mCombatComponent& Combat) { return Combat.AcceptedIdentityEffect; }
};

namespace Rogue10mAttackCosts
{
using R = ERogue10mAttackResourceType;
struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mPlayerState* State = nullptr;
	ARogue10mCharacter* Character = nullptr;
	URogue10mAttackSkillData* Skill = nullptr;
	FFixture()
	{
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		State = World->SpawnActor<ARogue10mPlayerState>();
		// The native base is abstract; use the same concrete Blueprint class as GameMode.
		auto* CharacterClass = LoadClass<ARogue10mCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		Character = CharacterClass ? World->SpawnActor<ARogue10mCharacter>(CharacterClass) : nullptr;
		if (!State || !Character) { return; }
		Character->SetPlayerState(State);
		auto* ASC = State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet());
		ASC->InitAbilityActorInfo(State, Character);
		Skill = NewObject<URogue10mAttackSkillData>(GetTransientPackage());
		Skill->AttackCooldown = 0.0f;
		Skill->HitStartDelaySeconds = 10.0f; // No hits, game instance, profiles or disk writes.
	}
	~FFixture() { if (World) { World->DestroyWorld(false); } }
	bool IsValid() const { return World && State && Character && Skill; }
	URogue10mAttributeSet& Attributes() { return *State->GetRogueAttributeSet(); }
	URogue10mCombatComponent& Combat() { return *Character->GetCombatComponent(); }
	bool Execute() { return FRogue10mAttackCostTestAccess::Execute(Combat(), *Skill); }
	bool ActivateMana()
	{
		State->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
		Attributes().SetPlayerLevel(2.0f);
		auto* Tree = State->GetSkillTreeProgressionComponent();
		Tree->RefreshAutomaticUnlocks();
		auto Snapshot = Tree->Capture();
		auto& Entry = Snapshot.Investments.AddDefaulted_GetRef();
		Entry.NodeId = TEXT("I.Mana.Entry"); Entry.Route = ERogue10mSkillTreeUnlockRoute::Points; Entry.SpentPoints = 1;
		Snapshot.ActiveIdentity = ERogue10mSkillTreeIdentity::Mana;
		if (!Tree->Restore(Snapshot)) { return false; }
		for (int32 Index = 0; Index < 5; ++Index) { Character->GetSkillTreeCombatComponent()->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::GreatSword); }
		return Character->GetSkillTreeCombatComponent()->GetGauge() == 100.0f;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAttackAggregateTest, "Rogue10m.AttackResources.AggregationAndAtomicFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAttackAggregateTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mAttackCosts;
	FFixture F;
	if (!TestTrue(TEXT("concrete character fixture"), F.IsValid())) { return false; }
	F.Skill->ResourceCosts = {{R::Health, 10}, {R::Stamina, 60}, {R::Stamina, 60}, {R::Mana, 20}};
	TestFalse(TEXT("duplicate stamina rows require their sum"), F.Execute());
	TestEqual(TEXT("failure preserves health"), F.Attributes().GetHealth(), 100.0f);
	TestEqual(TEXT("failure preserves stamina"), F.Attributes().GetStamina(), 100.0f);
	TestEqual(TEXT("failure preserves mana"), F.Attributes().GetMana(), 100.0f);
	TestEqual(TEXT("failure schedules no execution"), FRogue10mAttackCostTestAccess::Executions(F.Combat()), 0);
	TestEqual(TEXT("failure records no use"), F.State->GetSkillUseCount(F.Skill->GetFName()), 0);
	TestEqual(TEXT("failure starts no cooldown"), F.Combat().GetAttackCooldownRemaining(), 0.0f);
	F.Skill->ResourceCosts = {{R::Health, 10}, {R::Mana, 70}, {R::Mana, 40}};
	TestFalse(TEXT("late resource failure cannot partially debit earlier health"), F.Execute());
	TestEqual(TEXT("health still intact"), F.Attributes().GetHealth(), 100.0f);
	F.Skill->ResourceCosts = {{R::Health, 40}, {R::Health, 60}};
	TestFalse(TEXT("aggregate health equal to balance must reject"), F.Execute());
	F.Skill->ResourceCosts = {{R::Health, 40}, {R::Health, 59}, {R::Stamina, 40}, {R::Stamina, 60}, {R::Mana, 100}, {R::Energy, 10}};
	F.Attributes().SetIdentity(10.0f);
	TestTrue(TEXT("all costs accepted at allowed boundary"), F.Execute());
	TestEqual(TEXT("health stays alive"), F.Attributes().GetHealth(), 1.0f);
	TestEqual(TEXT("exact stamina debited"), F.Attributes().GetStamina(), 0.0f);
	TestEqual(TEXT("exact mana debited"), F.Attributes().GetMana(), 0.0f);
	TestEqual(TEXT("exact energy debited"), F.Attributes().GetIdentity(), 0.0f);
	TestEqual(TEXT("accepted once"), F.State->GetSkillUseCount(F.Skill->GetFName()), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAttackInvalidCostTest, "Rogue10m.AttackResources.InvalidDataAndDiscount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAttackInvalidCostTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mAttackCosts;
	FFixture F;
	if (!TestTrue(TEXT("concrete character fixture"), F.IsValid())) { return false; }
	for (float Invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		F.Skill->ResourceCosts = {{R::Health, 10}, {R::Mana, Invalid}};
		TestFalse(TEXT("invalid costs fail closed"), F.Execute());
		TestEqual(TEXT("invalid data cannot charge health"), F.Attributes().GetHealth(), 100.0f);
	}
	F.Skill->ResourceCosts = {{static_cast<R>(255), 0}};
	TestFalse(TEXT("unknown resource rejected even at zero"), F.Execute());
	const auto Discounted = FRogue10mAttackResourceBudget::Build({{R::Stamina, 80}, {R::Stamina, 40}}, 0.75f);
	TestEqual(TEXT("same discount applied to all rows"), Discounted.Stamina, 90.0f);
	TestTrue(TEXT("discounted sum payable"), Discounted.TryPay(F.Attributes()));
	TestEqual(TEXT("discounted sum charged once"), F.Attributes().GetStamina(), 10.0f);
	const auto Overflow = FRogue10mAttackResourceBudget::Build({{R::Mana, MAX_flt}, {R::Mana, MAX_flt}}, 1.0f);
	TestFalse(TEXT("aggregate overflow rejected"), Overflow.CanPay(F.Attributes()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAttackManaTest, "Rogue10m.AttackResources.OptionalManaAndExecutionSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAttackManaTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mAttackCosts;
	for (float Mana : {9.0f, 10.0f, 14.0f, 15.0f})
	{
		FFixture F;
		if (!TestTrue(TEXT("concrete character fixture"), F.IsValid())) { return false; }
		if (!TestTrue(TEXT("mana fixture activation"), F.ActivateMana())) { return false; }
		F.Skill->ResourceCosts = {{R::Mana, 6}, {R::Mana, 4}, {R::Stamina, 10}};
		F.Skill->HitMode = ERogue10mAttackHitMode::MultiHit;
		F.Skill->HitCount = 5;
		F.Attributes().SetMana(Mana);
		const bool bAccepted = F.Execute();
		TestEqual(TEXT("only base affordability gates attack"), bAccepted, Mana >= 10.0f);
		const auto* Identity = F.Character->GetSkillTreeCombatComponent();
		if (Mana < 10.0f)
		{
			TestEqual(TEXT("base rejection preserves all resources"), F.Attributes().GetStamina(), 100.0f);
			TestEqual(TEXT("base rejection preserves mana"), F.Attributes().GetMana(), Mana);
			TestEqual(TEXT("base rejection preserves gauge"), Identity->GetGauge(), 100.0f);
			TestEqual(TEXT("base rejection has no proc cooldown"), Identity->GetCooldownRemaining(), 0.0f);
			continue;
		}
		const bool bProc = Mana == 15.0f;
		TestEqual(TEXT("optional proc needs full remaining cost"), FRogue10mAttackCostTestAccess::Effect(F.Combat()).bTriggered, bProc);
		TestEqual(TEXT("aggregate base and optional cost debited once"), F.Attributes().GetMana(), Mana - 10.0f - (bProc ? 5.0f : 0.0f));
		TestEqual(TEXT("gauge consumed only by paid proc"), Identity->GetGauge(), bProc ? 0.0f : 100.0f);
		TestEqual(TEXT("one execution for five hit pulses"), FRogue10mAttackCostTestAccess::Executions(F.Combat()), 1);
		TestEqual(TEXT("one use per accepted execution"), F.State->GetSkillUseCount(F.Skill->GetFName()), 1);
		TestEqual(TEXT("one stamina payment"), F.Attributes().GetStamina(), 90.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAttackReentryTest, "Rogue10m.AttackResources.ReentrantAcceptanceRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAttackReentryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mAttackCosts;
	FFixture F;
	if (!TestTrue(TEXT("concrete character fixture"), F.IsValid())) { return false; }
	F.Skill->ResourceCosts = {{R::Stamina, 10}, {R::Mana, 5}};
	bool bReentered = false;
	bool bNestedAccepted = true;
	auto& Delegate = F.State->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetStaminaAttribute());
	const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&) { bReentered = true; bNestedAccepted = F.Execute(); });
	TestTrue(TEXT("outer acceptance succeeds"), F.Execute());
	Delegate.Remove(Handle);
	TestTrue(TEXT("real attribute notification attempted reentry"), bReentered);
	TestFalse(TEXT("nested acceptance rejected before any charge"), bNestedAccepted);
	TestEqual(TEXT("one execution"), FRogue10mAttackCostTestAccess::Executions(F.Combat()), 1);
	TestEqual(TEXT("one stamina debit"), F.Attributes().GetStamina(), 90.0f);
	TestEqual(TEXT("one mana debit"), F.Attributes().GetMana(), 95.0f);
	return true;
}

#endif
