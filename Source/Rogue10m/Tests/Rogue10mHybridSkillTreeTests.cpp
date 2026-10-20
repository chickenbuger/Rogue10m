#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"

namespace Rogue10mHybridTests
{
using ETrack = ERogue10mSkillTreeTrack;
using EIdentity = ERogue10mSkillTreeIdentity;
using ERoute = ERogue10mSkillTreeUnlockRoute;

/** No GameInstance, profile subsystem, disk writes or production slots. */
struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mPlayerState* Player = nullptr;
	URogue10mSkillTreeProgressionComponent* Tree = nullptr;
	FFixture()
	{
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		Player = World ? World->SpawnActor<ARogue10mPlayerState>() : nullptr;
		if (Player)
		{
			// This isolated world never begins play or possesses a Pawn. Register the
			// PlayerState-owned set and actor info explicitly before GAS value setters.
			if (UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent())
			{
				AbilitySystem->AddAttributeSetSubobject(Player->GetRogueAttributeSet());
				AbilitySystem->InitAbilityActorInfo(Player, Player);
			}
		}
		Tree = Player ? Player->GetSkillTreeProgressionComponent() : nullptr;
		if (Tree) { Tree->RefreshAutomaticUnlocks(); }
	}
	~FFixture() { if (World) { World->DestroyWorld(false); } }
	void Hits(int32 Count, ETrack Track = ETrack::Brawler) { for (int32 Index = 0; Index < Count; ++Index) { Tree->NotifyConfirmedAttack(Track); } }
};

static void Invest(FRogue10mSkillTreeSnapshot& Snapshot, const TCHAR* Id, ERoute Route, int32 Cost, FName Choice = NAME_None)
{
	auto& Row = Snapshot.Investments.AddDefaulted_GetRef();
	Row.NodeId = FName(Id); Row.Route = Route; Row.SpentPoints = Cost; Row.ChoiceId = Choice;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHybridMasteryTest, "Rogue10m.HybridSkillTree.MasteryAndRewards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHybridMasteryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mHybridTests;
	FFixture Fixture;
	if (!TestNotNull(TEXT("isolated player"), Fixture.Player) || !TestNotNull(TEXT("progression component"), Fixture.Tree)) { return false; }
	auto& Tree = *Fixture.Tree;
	TestTrue(TEXT("catalog valid"), Tree.GetCatalog()->IsValidCatalog());
	TestEqual(TEXT("both fist weapons resolve one mastery"), Tree.ResolveTrack(ERogue10mWeaponType::Unarmed), Tree.ResolveTrack(ERogue10mWeaponType::Knuckle));
	TestTrue(TEXT("free initial fighter grant"), Tree.IsNodeUnlocked(TEXT("F.Basic")));
	TestFalse(TEXT("core initially locked"), Tree.IsNodeUnlocked(TEXT("F.Ultimate")));
	Fixture.Hits(385);
	TestEqual(TEXT("385 confirmed executions reach M12"), Tree.GetMasteryLevel(ETrack::Brawler), 12);
	TestEqual(TEXT("M12 remainder zero"), Tree.GetMasteryExperience(ETrack::Brawler), 0);
	TestEqual(TEXT("next mastery requires 65XP"), Tree.GetMasteryExperienceToNextLevel(ETrack::Brawler), 65);
	TestEqual(TEXT("M12 mastery attack only +3"), Tree.GetMasteryStatModifiers(ETrack::Brawler).AttackPowerBonus, 3.0f);
	TestEqual(TEXT("M12 mastery stamina +10"), Tree.GetMasteryStatModifiers(ETrack::Brawler).MaxStaminaBonus, 10.0f);
	TestTrue(TEXT("M12 sequential technique granted"), Tree.IsNodeUnlocked(TEXT("F.DiamondStrike")));
	TestFalse(TEXT("M13 technique still locked"), Tree.IsNodeUnlocked(TEXT("F.HundredFists")));
	TestEqual(TEXT("other track receives no XP"), Tree.GetMasteryLevel(ETrack::GreatSword), 1);
	TestEqual(TEXT("other weapon receives no fighter attack growth"), Tree.GetMasteryStatModifiers(ERogue10mWeaponType::GreatSword).AttackPowerBonus, 0.0f);
	Fixture.Hits(1045 - 385);
	TestEqual(TEXT("1045 confirmed executions reach M20"), Tree.GetMasteryLevel(ETrack::Brawler), 20);
	const auto Final = Tree.GetMasteryStatModifiers(ETrack::Brawler);
	TestEqual(TEXT("final attack +5"), Final.AttackPowerBonus, 5.0f);
	TestEqual(TEXT("final stamina +10"), Final.MaxStaminaBonus, 10.0f);
	TestEqual(TEXT("final health +10"), Final.MaxHealthBonus, 10.0f);
	TestEqual(TEXT("final defense +1"), Final.DefenseBonus, 1.0f);
	TestEqual(TEXT("final attack speed +.02"), Final.AttackSpeedBonus, .02f);
	TestTrue(TEXT("ultimate automatically learned"), Tree.IsNodeUnlocked(TEXT("F.Ultimate")));
	TestTrue(TEXT("passive automatically learned"), Tree.IsNodeUnlocked(TEXT("F.GoldenBody")));
	const auto Saved = Tree.Capture();
	TestTrue(TEXT("restoring durable record succeeds"), Tree.Restore(Saved));
	Tree.RefreshAutomaticUnlocks();
	Fixture.Player->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("replay and fist weapon swap do not duplicate rewards"), Tree.GetMasteryStatModifiers(ETrack::Brawler).AttackPowerBonus, 5.0f);
	Fixture.Hits(10);
	TestEqual(TEXT("mastery cap XP is bounded"), Tree.GetMasteryTotalExperience(ETrack::Brawler), 1045);
	TestTrue(TEXT("capped snapshot valid"), Tree.Validate(Tree.Capture(), 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHybridLedgerTest, "Rogue10m.HybridSkillTree.SharedLedgerAndValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHybridLedgerTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mHybridTests;
	FFixture Fixture;
	if (!Fixture.Tree) { AddError(TEXT("fixture creation failed")); return false; }
	auto Base = Fixture.Tree->Capture();
	Invest(Base, TEXT("I.InternalEnergy.Entry"), ERoute::Points, 1);
	Base.ActiveIdentity = EIdentity::InternalEnergy;
	TestTrue(TEXT("LV2 one entry investment fits one point"), Fixture.Tree->Validate(Base, 1, 0));
	TestFalse(TEXT("traits and skill tree cannot spend same point"), Fixture.Tree->Validate(Base, 1, 1));
	TestTrue(TEXT("shared point budget accounts actual trait cost"), Fixture.Tree->Validate(Base, 2, 1));
	auto* Retuned = NewObject<URogue10mSkillTreeDataAsset>();
	Retuned->Nodes.FindByPredicate([](const auto& Node) { return Node.NodeId == FName(TEXT("I.InternalEnergy.Entry")); })->PointCost = 7;
	TestTrue(TEXT("historical paid price survives later catalog cost changes"), Fixture.Tree->Validate(Base, 1, 0, nullptr, Retuned));
	auto Bad = Base;
	const FRogue10mSkillTreeInvestment DuplicateInvestment = Bad.Investments.Last();
	Bad.Investments.Add(DuplicateInvestment);
	TestFalse(TEXT("duplicate node rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Bad.Investments.Last().SpentPoints = -1;
	TestFalse(TEXT("negative actual cost rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Bad.Investments.Last().NodeId = TEXT("Unknown.Node");
	TestFalse(TEXT("unknown node rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Bad.ActiveIdentity = EIdentity::Mana;
	TestFalse(TEXT("unlearned active identity rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Invest(Bad, TEXT("F.Power.1"), ERoute::Automatic, 0);
	TestFalse(TEXT("unearned permanent stat rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Invest(Bad, TEXT("I.InternalEnergy.Advanced"), ERoute::Mastery, 0);
	TestFalse(TEXT("M1 cannot claim free advanced"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; Invest(Bad, TEXT("I.InternalEnergy.Advanced"), ERoute::Points, 2);
	TestTrue(TEXT("paid advanced works at M1 without mastery"), Fixture.Tree->Validate(Bad, 3, 0));
	Invest(Bad, TEXT("I.InternalEnergy.Choice"), ERoute::Points, 2, TEXT("InternalEnergy.Cycle"));
	TestTrue(TEXT("paid full identity at M1 fits five points"), Fixture.Tree->Validate(Bad, 5, 0));
	Bad.Investments.Last().ChoiceId = TEXT("Mana.Pursuit");
	TestFalse(TEXT("cross identity choice rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad = Base; auto& XP = Bad.Masteries.AddDefaulted_GetRef(); XP.Track = ETrack::Brawler; XP.TotalExperience = -1;
	TestFalse(TEXT("negative mastery XP rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	Bad.Masteries.Last().TotalExperience = 1046;
	TestFalse(TEXT("out of bounds mastery XP rejected"), Fixture.Tree->Validate(Bad, 10, 0));
	const auto* Catalog = Fixture.Tree->GetCatalog();
	TestTrue(TEXT("fighter internal energy allowed"), Catalog->IsIdentityAllowed(ETrack::Brawler, EIdentity::InternalEnergy));
	TestFalse(TEXT("fighter mana forbidden"), Catalog->IsIdentityAllowed(ETrack::Brawler, EIdentity::Mana));
	TestTrue(TEXT("greatsword mana allowed"), Catalog->IsIdentityAllowed(ETrack::GreatSword, EIdentity::Mana));
	TestFalse(TEXT("greatsword internal energy forbidden"), Catalog->IsIdentityAllowed(ETrack::GreatSword, EIdentity::InternalEnergy));
	TestTrue(TEXT("fighting spirit shared by both"), Catalog->IsIdentityAllowed(ETrack::Brawler, EIdentity::FightingSpirit) && Catalog->IsIdentityAllowed(ETrack::GreatSword, EIdentity::FightingSpirit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHybridSourceTest, "Rogue10m.HybridSkillTree.MasteryAlternativesAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHybridSourceTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mHybridTests;
	FFixture Fixture;
	if (!Fixture.Tree) { AddError(TEXT("fixture creation failed")); return false; }
	Fixture.Player->GetRogueAttributeSet()->SetPlayerLevel(10.0f);
	Fixture.Hits(385);
	auto Source = Fixture.Tree->Capture();
	Invest(Source, TEXT("I.InternalEnergy.Entry"), ERoute::Points, 1);
	Invest(Source, TEXT("I.InternalEnergy.Advanced"), ERoute::Mastery, 0);
	Source.ActiveIdentity = EIdentity::InternalEnergy;
	TestTrue(TEXT("M12 plus stamina source grants free internal advanced"), Fixture.Tree->Validate(Source, 1, 0));
	auto Bad = Source;
	Bad.Investments.RemoveAll([](const auto& Row) { return Row.NodeId == FName(TEXT("F.Breath.2")); });
	TestFalse(TEXT("level alone cannot replace missing permanent stamina source"), Fixture.Tree->Validate(Bad, 10, 0));
	TestTrue(TEXT("source restore"), Fixture.Tree->Restore(Source));
	TestEqual(TEXT("actual point spend remains one"), Fixture.Tree->GetSpentSkillTreePoints(), 1);
	const auto BeforeDeniedRefund = Fixture.Tree->Capture();
	TestFalse(TEXT("refund requires the live town NPC service boundary"), Fixture.Tree->RefundNode(TEXT("I.InternalEnergy.Advanced")));
	TestEqual(TEXT("denied refund preserves every learned stage"), Fixture.Tree->Capture().Investments.Num(), BeforeDeniedRefund.Investments.Num());
	TestTrue(TEXT("denied refund preserves learned advanced"), Fixture.Tree->IsIdentityDeepened(EIdentity::InternalEnergy));
	Fixture.Player->GetRogueAttributeSet()->SetPlayerLevel(2.0f);
	FRogue10mTownProgressionSnapshot TraitAttempt;
	TraitAttempt.VitalityLevel = 1; TraitAttempt.TraitMaxHealthBonus = 5.0f;
	auto& Trait = TraitAttempt.TraitInvestments.AddDefaulted_GetRef();
	Trait.Trait = ERogue10mTownTrait::Vitality; Trait.Rank = 1; Trait.SpentPoints = 1; Trait.Bonus = 5.0f;
	TestFalse(TEXT("live trait restore cannot double spend identity point"), Fixture.Player->GetTownProgressionComponent()->RestoreProgression(TraitAttempt));
	TestEqual(TEXT("trait failure preserves shared balance"), Fixture.Tree->GetAvailableGrowthPoints(), 0);
	Fixture.Player->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	TestEqual(TEXT("unsupported identity has no active effect after swap"), Fixture.Tree->GetActiveIdentity(), EIdentity::None);
	TestTrue(TEXT("learning remembered after swap"), Fixture.Tree->IsIdentityLearned(EIdentity::InternalEnergy));
	TestEqual(TEXT("permanent source recorded independent of current weapon"), Fixture.Tree->GetMasteryStatModifiers(ETrack::Brawler).MaxStaminaBonus, 10.0f);
	TestEqual(TEXT("no cross weapon stat accumulation"), Fixture.Tree->GetMasteryStatModifiers(ERogue10mWeaponType::GreatSword).MaxStaminaBonus, 0.0f);
	FFixture Sword;
	Sword.Hits(385, ETrack::GreatSword);
	auto SwordSource = Sword.Tree->Capture();
	Invest(SwordSource, TEXT("I.FightingSpirit.Entry"), ERoute::Points, 1);
	Invest(SwordSource, TEXT("I.FightingSpirit.Advanced"), ERoute::Mastery, 0);
	TestTrue(TEXT("shared spirit accepts greatsword source alternative"), Sword.Tree->Validate(SwordSource, 1, 0));
	const auto* Ultimate = Fixture.Tree->FindNode(TEXT("F.Ultimate"));
	if (!TestNotNull(TEXT("legacy ultimate definition"), Ultimate)) { return false; }
	auto Legacy = Fixture.Tree->MigrateLegacy({Ultimate->Skill});
	TestTrue(TEXT("legacy grant recorded without manufactured XP"), Legacy.Masteries.IsEmpty());
	TestTrue(TEXT("legacy grant exception validates missing new predecessors"), Fixture.Tree->Validate(Legacy, 0, 0));
	FFixture Migrated;
	TestTrue(TEXT("legacy snapshot restore"), Migrated.Tree->Restore(Legacy));
	TestEqual(TEXT("legacy starts M1"), Migrated.Tree->GetMasteryLevel(ETrack::Brawler), 1);
	TestEqual(TEXT("legacy technique does not retroactively award stats"), Migrated.Tree->GetMasteryStatModifiers(ETrack::Brawler).AttackPowerBonus, 0.0f);
	TestTrue(TEXT("old ultimate remains learned"), Migrated.Tree->IsNodeUnlocked(TEXT("F.Ultimate")));
	TestFalse(TEXT("legacy does not manufacture source stats"), Migrated.Tree->IsNodeUnlocked(TEXT("F.Power.3")));
	return true;
}

#endif
