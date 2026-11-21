#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
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
	TestEqual(TEXT("novice has no weapon mastery"), Tree.ResolveTrack(ERogue10mWeaponType::Unarmed), ETrack::None);
	TestEqual(TEXT("equipped knuckle owns brawler mastery"), Tree.ResolveTrack(ERogue10mWeaponType::Knuckle), ETrack::Brawler);
	TestTrue(TEXT("free initial fighter grant"), Tree.IsNodeUnlocked(TEXT("F.Basic")));
	TestFalse(TEXT("core initially locked"), Tree.IsNodeUnlocked(TEXT("F.Ultimate")));
	const auto VisibleNodes = Tree.GetCatalog()->Nodes.FilterByPredicate([](const FRogue10mSkillTreeNodeDefinition& Node)
	{
		return Node.Track == ETrack::Brawler && Node.bShowInTree;
	});
	TestEqual(TEXT("the approved knuckle tree has exactly 17 visible nodes"), VisibleNodes.Num(), 17);
	struct FTrainingStep
	{
		FName Id;
		TArray<FName> Parents;
	};
	const FTrainingStep Steps[] = {
		{TEXT("F.Basic.LeftJab"), {}}, {TEXT("F.Basic.RightJab"), {}},
		{TEXT("F.Basic.RightStraight"), {}}, {TEXT("F.Basic.RightHook"), {}},
		{TEXT("F.Power.1"), {TEXT("F.Basic.LeftJab")}}, {TEXT("F.Breath.1"), {TEXT("F.Basic.RightJab")}},
		{TEXT("F.Body.1"), {TEXT("F.Basic.RightStraight")}}, {TEXT("F.Body.2"), {TEXT("F.Basic.RightHook")}},
		{TEXT("F.Bengquan"), {TEXT("F.Power.1")}}, {TEXT("F.CloudHands"), {TEXT("F.Power.1"), TEXT("F.Breath.1")}},
		{TEXT("F.Shoulder"), {TEXT("F.Body.1")}},
		{TEXT("F.BreakingKick"), {TEXT("F.Bengquan")}}, {TEXT("F.DiamondStrike"), {TEXT("F.Bengquan")}},
		{TEXT("F.HundredFists"), {TEXT("F.CloudHands")}}, {TEXT("F.Ultimate"), {TEXT("F.CloudHands"), TEXT("F.Shoulder")}},
		{TEXT("F.MountainSlam"), {TEXT("F.Shoulder")}}, {TEXT("F.GoldenBody"), {TEXT("F.Body.2")}}
	};
	for (const FTrainingStep& Step : Steps)
	{
		const auto* Node = Tree.FindNode(Step.Id);
		const FString Label = Step.Id.ToString();
		if (!TestTrue(*Label, Node && Node->bShowInTree && Node->bAutomaticUnlock && Node->PointCost == 0)) { continue; }
		if (Step.Parents.IsEmpty())
		{
			TestTrue(*(Label + TEXT(" starts as a learned single-skill root")), Node->PrerequisiteGroups.IsEmpty()
				&& (!Node->Skill.IsNull() || Node->GrantedSkills.Num() == 1) && Tree.IsNodeUnlocked(Step.Id));
			continue;
		}
		TestFalse(*(Label + TEXT(" requires training")), Tree.IsNodeUnlocked(Step.Id));
		TestTrue(*(Label + TEXT(" has exactly the approved AND predecessors")), Node->PrerequisiteGroups.Num() == 1
			&& Node->PrerequisiteGroups[0].NodeIds.Num() == Step.Parents.Num()
			&& !Node->PrerequisiteGroups[0].NodeIds.ContainsByPredicate([&Step](FName Id) { return !Step.Parents.Contains(Id); }));
		for (const FName ParentId : Step.Parents)
		{
			const auto* Parent = Tree.FindNode(ParentId);
			TestTrue(*(Label + TEXT(" is visually above ") + ParentId.ToString()), Parent && Parent->Position.Y > Node->Position.Y);
		}
	}
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
	for (const FTrainingStep& Step : Steps)
	{
		TestTrue(*(Step.Id.ToString() + TEXT(" is acquired by the final mastery milestone")), Tree.IsNodeUnlocked(Step.Id));
	}
	const auto Saved = Tree.Capture();
	TestTrue(TEXT("restoring durable record succeeds"), Tree.Restore(Saved));
	Tree.RefreshAutomaticUnlocks();
	Fixture.Player->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("replay and fist weapon swap do not duplicate rewards"), Tree.GetMasteryStatModifiers(ETrack::Brawler).AttackPowerBonus, 5.0f);
	Fixture.Hits(10);
	TestEqual(TEXT("mastery cap XP is bounded"), Tree.GetMasteryTotalExperience(ETrack::Brawler), 1045);
	TestTrue(TEXT("capped snapshot valid"), Tree.Validate(Tree.Capture(), 0, 0));

	FRogue10mSkillTreeSnapshot OldKnuckle;
	auto& OldMastery = OldKnuckle.Masteries.AddDefaulted_GetRef();
	OldMastery.Track = ETrack::Brawler;
	OldMastery.TotalExperience = 100;
	for (const TCHAR* Id : {TEXT("F.Root"), TEXT("F.Basic"), TEXT("F.Breath.1"), TEXT("F.CloudHands")})
	{
		Invest(OldKnuckle, Id, ERoute::Automatic, 0);
	}
	TestTrue(TEXT("old M6 cloud hands with its previous single prerequisite still validates"), Tree.Validate(OldKnuckle, 0, 0));
	auto IncompleteOld = OldKnuckle;
	IncompleteOld.Investments.RemoveAt(2);
	TestFalse(TEXT("a recorded technique without either set of prerequisites is rejected"), Tree.Validate(IncompleteOld, 0, 0));
	TestTrue(TEXT("restoring old training keeps cloud hands and adds the new roots"), Tree.Restore(OldKnuckle)
		&& Tree.IsNodeUnlocked(TEXT("F.CloudHands")) && Tree.IsNodeUnlocked(TEXT("F.Basic.RightHook"))
		&& Tree.GetMasteryLevel(ETrack::Brawler) == 6 && Tree.GetSpentSkillTreePoints() == 0);
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
	TestEqual(TEXT("unsupported legacy identity gives way to a compatible rank"), Fixture.Tree->GetActiveIdentity(), EIdentity::FightingSpirit);
	TestTrue(TEXT("learning remembered after swap"), Fixture.Tree->IsIdentityLearned(EIdentity::InternalEnergy));
	TestEqual(TEXT("permanent source recorded independent of current weapon"), Fixture.Tree->GetMasteryStatModifiers(ETrack::Brawler).MaxStaminaBonus, 10.0f);
	TestEqual(TEXT("no cross weapon stat accumulation"), Fixture.Tree->GetMasteryStatModifiers(ERogue10mWeaponType::GreatSword).MaxStaminaBonus, 0.0f);
	FFixture Sword;
	const auto* AuthoredCatalog = LoadObject<URogue10mSkillTreeDataAsset>(nullptr,
		TEXT("/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog.DA_HybridSkillTreeCatalog"));
	if (!TestNotNull(TEXT("shipped hybrid catalog is loaded"), AuthoredCatalog)) { return false; }
	const auto* SwordTechnique = AuthoredCatalog->FindNode(TEXT("G.CrushingSlash"));
	TestTrue(TEXT("shipped catalog exposes the paid technique and historical route"), SwordTechnique
		&& SwordTechnique->PointCost == 1 && !SwordTechnique->bAutomaticUnlock && SwordTechnique->bAcceptHistoricalAutomaticUnlock);
	Sword.Hits(45, ETrack::GreatSword);
	const auto EarlySword = Sword.Tree->Capture();
	TestEqual(TEXT("M4 exposes the three entry branches"), Sword.Tree->GetMasteryLevel(ETrack::GreatSword), 4);
	auto StrengthRoute = EarlySword;
	Invest(StrengthRoute, TEXT("G.CrushingSlash"), ERoute::Points, 1);
	TestTrue(TEXT("one point affords a direct strength-route attack"), Sword.Tree->Validate(StrengthRoute, 1, 0));
	auto SideRoute = EarlySword;
	Invest(SideRoute, TEXT("G.Whirlwind"), ERoute::Points, 1);
	Invest(SideRoute, TEXT("G.GuardBreak"), ERoute::Points, 1);
	Invest(SideRoute, TEXT("G.EarthSplitter"), ERoute::Points, 1);
	Invest(SideRoute, TEXT("G.Ultimate"), ERoute::Points, 1);
	TestTrue(TEXT("four points reach the capstone from the sweeping and defensive branches"), Sword.Tree->Validate(SideRoute, 4, 0));
	TestFalse(TEXT("combined side route still needs the full point budget"), Sword.Tree->Validate(SideRoute, 3, 0));
	SideRoute.Investments.RemoveAll([](const auto& Row) { return Row.NodeId == TEXT("G.GuardBreak"); });
	TestFalse(TEXT("combined side route cannot skip its guard-breaking predecessor"), Sword.Tree->Validate(SideRoute, 4, 0));

	Sword.Hits(385 - 45, ETrack::GreatSword);
	auto SwordSource = Sword.Tree->Capture();
	TestFalse(TEXT("current sword techniques are not automatically granted at M12"), Sword.Tree->IsNodeUnlocked(TEXT("G.CrushingSlash")));
	auto OldSword = SwordSource;
	Invest(OldSword, TEXT("G.CrushingSlash"), ERoute::Automatic, 0);
	Invest(OldSword, TEXT("G.Power.2"), ERoute::Automatic, 0);
	Invest(OldSword, TEXT("G.EarthSplitter"), ERoute::Automatic, 0);
	TestTrue(TEXT("old automatic sword grants restore under their original requirements"), Sword.Tree->Restore(OldSword));
	TestTrue(TEXT("restored technique stays learned without debiting points"), Sword.Tree->IsNodeUnlocked(TEXT("G.EarthSplitter"))
		&& Sword.Tree->GetSpentSkillTreePoints() == 0);
	auto Forged = OldSword;
	Forged.Investments.RemoveAll([](const auto& Row) { return Row.NodeId == FName(TEXT("G.Power.1")); });
	TestFalse(TEXT("historical automatic route still requires its original prerequisite"), Sword.Tree->Validate(Forged, 0, 0));
	Forged = OldSword;
	Forged.Masteries[0].TotalExperience = 0;
	TestFalse(TEXT("historical automatic route still requires mastery"), Sword.Tree->Validate(Forged, 0, 0));
	Invest(SwordSource, TEXT("G.CrushingSlash"), ERoute::Points, 1);
	Invest(SwordSource, TEXT("G.Power.2"), ERoute::Automatic, 0);
	Invest(SwordSource, TEXT("I.FightingSpirit.Entry"), ERoute::Points, 1);
	Invest(SwordSource, TEXT("I.FightingSpirit.Advanced"), ERoute::Mastery, 0);
	TestTrue(TEXT("shared spirit accepts greatsword source alternative after sword investment"), Sword.Tree->Validate(SwordSource, 2, 0));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mIdentityGrowthTest, "Rogue10m.HybridSkillTree.LevelAndConsumableIdentityGrowth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mIdentityGrowthTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mHybridTests;
	FFixture Fixture;
	if (!TestNotNull(TEXT("identity progression"), Fixture.Tree)) { return false; }
	auto& Tree = *Fixture.Tree;
	auto& Player = *Fixture.Player;
	Player.SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	TestEqual(TEXT("level one starts with zero base rank"), Tree.GetIdentityRank(EIdentity::Mana), 0);
	TestEqual(TEXT("zero rank does not activate a weapon identity"), Tree.GetActiveIdentity(), EIdentity::None);

	Player.AddExperience(FMath::CeilToInt(Player.GetExperienceToNextLevel()));
	TestEqual(TEXT("normal experience path reaches level two"), Player.GetPlayerLevel(), 2);
	TestEqual(TEXT("leveling grows internal energy"), Tree.GetIdentityRank(EIdentity::InternalEnergy), 1);
	TestEqual(TEXT("leveling grows fighting spirit"), Tree.GetIdentityRank(EIdentity::FightingSpirit), 1);
	TestEqual(TEXT("leveling grows mana"), Tree.GetIdentityRank(EIdentity::Mana), 1);
	TestEqual(TEXT("unspent level point remains available"), Tree.GetAvailableGrowthPoints(), 1);
	FText Reason;
	TestFalse(TEXT("retired identity nodes reject fresh point purchases"), Tree.CanUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None, Reason));
	TestTrue(TEXT("purchase rejection explains rank acquisition"), Reason.ToString().Contains(TEXT("레벨업")));

	int32 Committed = 0;
	const auto Commit = [&Committed]() { ++Committed; };
	TestFalse(TEXT("invalid family cannot consume item"), Tree.TryAddIdentityGrowth(EIdentity::None, 10, Commit));
	TestFalse(TEXT("nonpositive amount cannot consume item"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 0, Commit));
	TestFalse(TEXT("cap cannot be exceeded"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 101, Commit));
	TestEqual(TEXT("failed grants leave the item commit untouched"), Committed, 0);
	TestTrue(TEXT("a valid bonus commits the item before notifications"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 9, [&]()
	{
		++Committed;
		TestEqual(TEXT("item commit observes the newly granted rank"), Tree.GetIdentityRank(EIdentity::Mana), 10);
	}));
	TestEqual(TEXT("item commit occurs once"), Committed, 1);
	TestEqual(TEXT("typed mana bonus leaves fighting spirit alone"), Tree.GetIdentityRank(EIdentity::FightingSpirit), 1);
	TestEqual(TEXT("stronger grown identity activates on the compatible weapon"), Tree.GetActiveIdentity(), EIdentity::Mana);
	TestTrue(TEXT("rank unlocks the deepened gauge bonus"), Tree.IsIdentityDeepened(EIdentity::Mana));
	TestEqual(TEXT("mana rank increases capacity"), Tree.GetManaCapacityBonus(), 10.0f * Tree.GetCatalog()->ManaCapacityPerRank);
	TestFalse(TEXT("rank alone cannot grant resonance without the weapon ultimate"), Tree.IsNodeUnlocked(TEXT("G.Resonance.Pursuit")));

	const auto* Ultimate = Tree.FindNode(TEXT("G.Ultimate"));
	if (!TestNotNull(TEXT("sword ultimate definition"), Ultimate)) { return false; }
	auto Saved = Tree.Capture();
	const auto LegacySkill = Tree.MigrateLegacy({ Ultimate->Skill });
	Saved.LegacyGrantedSkills = LegacySkill.LegacyGrantedSkills;
	Saved.Investments.Append(LegacySkill.Investments);
	auto* Save = NewObject<URogue10mProgressionSaveGame>();
	Save->SkillTree = Saved;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("run snapshot serializes through SaveGame"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) { return false; }
	const auto* Loaded = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("run snapshot deserializes"), Loaded)) { return false; }
	TestTrue(TEXT("saved typed bonus restores in the existing run snapshot"), Tree.Restore(Loaded->SkillTree));
	TestEqual(TEXT("item bonus is applied once after restore"), Tree.GetIdentityRank(EIdentity::Mana), 10);
	TestEqual(TEXT("item growth and automatic resonance consume no shared points"), Tree.GetSpentSkillTreePoints(), 0);
	TestTrue(TEXT("first resonance unlocks at rank ten with the weapon ultimate"), Tree.IsNodeUnlocked(TEXT("G.Resonance.Pursuit")));
	TestFalse(TEXT("second resonance remains locked below rank fifteen"), Tree.IsNodeUnlocked(TEXT("G.Resonance.Spread")));
	TestTrue(TEXT("typed bonus can reach the second resonance"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 5, Commit));
	TestTrue(TEXT("second resonance unlocks while the first stays learned"), Tree.IsNodeUnlocked(TEXT("G.Resonance.Pursuit"))
		&& Tree.IsNodeUnlocked(TEXT("G.Resonance.Spread")));
	TestEqual(TEXT("both earned resonances boost the active ultimate"), Tree.GetResonanceDamageBonus(ETrack::GreatSword, EIdentity::Mana),
		2.0f * Tree.GetCatalog()->ResonanceDamageBonus);

	TestTrue(TEXT("maximum valid item growth can be reached exactly"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 86, Commit));
	TestFalse(TEXT("further item use is rejected without debit"), Tree.TryAddIdentityGrowth(EIdentity::Mana, 1, Commit));
	TestEqual(TEXT("exactly the three successful items were debited"), Committed, 3);
	auto Invalid = Tree.Capture();
	Invalid.IdentityConsumableRanks.Add(EIdentity::Mana, 101);
	TestFalse(TEXT("corrupt out-of-bounds typed bonus is rejected"), Tree.Validate(Invalid, 1, 0));
	Invalid = Tree.Capture();
	Invalid.IdentityConsumableRanks.Add(EIdentity::None, 1);
	TestFalse(TEXT("corrupt invalid identity key is rejected"), Tree.Validate(Invalid, 1, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCommonSkillWebTest, "Rogue10m.HybridSkillTree.CommonSkillWeb", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCommonSkillWebTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mHybridTests;
	using ECommon = ERogue10mCommonSkill;
	FFixture Fixture;
	if (!TestNotNull(TEXT("common skill progression"), Fixture.Tree)) { return false; }
	auto& Tree = *Fixture.Tree;
	const auto* Catalog = Tree.GetCatalog();
	if (!TestTrue(TEXT("active catalog exposes a valid common graph"), Catalog && Catalog->IsValidCatalog())) { return false; }
	const auto Common = Catalog->Nodes.FilterByPredicate([](const auto& Node) { return Node.CommonSkill != ECommon::None; });
	TestEqual(TEXT("free starting node plus fifteen trainable abilities"), Common.Num(), 16);
	int32 DirectedEdges = 0;
	for (const auto& Node : Common)
	{
		TestTrue(TEXT("common ability visible for all tracks"), Node.bShowInTree
			&& URogue10mSkillTreeDataAsset::IsNodeInTrack(Node, ETrack::None)
			&& URogue10mSkillTreeDataAsset::IsNodeInTrack(Node, ETrack::Brawler)
			&& URogue10mSkillTreeDataAsset::IsNodeInTrack(Node, ETrack::GreatSword));
		TestEqual(TEXT("only foundation is free"), Node.PointCost, Node.CommonSkill == ECommon::Foundation ? 0 : 1);
		DirectedEdges += Node.CommonConnections.Num();
		for (FName Neighbor : Node.CommonConnections)
		{
			const auto* Other = Tree.FindNode(Neighbor);
			TestTrue(TEXT("every common connection is reciprocal"), Other && Other->CommonConnections.Contains(Node.NodeId));
		}
	}
	TestEqual(TEXT("twenty-one connections as drawn"), DirectedEdges, 42);
	const auto Start = Tree.Capture();
	TestTrue(TEXT("free foundation learned"), Tree.IsCommonSkillLearned(ECommon::Foundation));
	TestFalse(TEXT("double jump still costs an investment"), Tree.IsCommonSkillLearned(ECommon::DoubleJump));
	FRogue10mSkillTreeSnapshot Legacy;
	TestTrue(TEXT("snapshot from before the common tree restores the foundation"), Tree.Restore(Legacy)
		&& Tree.IsCommonSkillLearned(ECommon::Foundation));
	Fixture.Player->GetRogueAttributeSet()->SetPlayerLevel(8.0f);
	auto Route = Start;
	for (const TCHAR* Node : {TEXT("C.DoubleJump"), TEXT("C.AirMomentum"), TEXT("C.GroundTurning"), TEXT("C.MoveSpeed")})
	{
		Invest(Route, Node, ERoute::Points, 1);
	}
	TestTrue(TEXT("bridge reaches double jump without buying air control, even with reverse save order"), Tree.Validate(Route, 4, 0));
	TestFalse(TEXT("a trait and a common node cannot debit the same point"), Tree.Validate(Route, 4, 1));
	TestTrue(TEXT("cross-region route restores"), Tree.Restore(Route));
	TestTrue(TEXT("double jump applies independently of active weapon track"), Tree.IsCommonSkillLearned(ECommon::DoubleJump)
		&& !Tree.IsCommonSkillLearned(ECommon::AirControl));
	Fixture.Player->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	TestTrue(TEXT("weapon switch retains common investments"), Tree.IsCommonSkillLearned(ECommon::DoubleJump)
		&& Tree.IsCommonSkillLearned(ECommon::MoveSpeed) && Tree.GetSpentSkillTreePoints() == 4);
	TestTrue(TEXT("an acquired outer node opens backwards into the entry"),
		Tree.EvaluateNodeEligibility(TEXT("C.AirControl")).CanAcquire(ERoute::Points));
	TestTrue(TEXT("learned double jump unlocks its upgrade"),
		Tree.EvaluateNodeEligibility(TEXT("C.DoubleJumpUpgrade")).CanAcquire(ERoute::Points));

	auto Bypass = Start;
	for (const TCHAR* Node : {TEXT("C.AirControl"), TEXT("C.JumpHeight"), TEXT("C.DoubleJumpUpgrade")})
	{
		Invest(Bypass, Node, ERoute::Points, 1);
	}
	TestFalse(TEXT("adjacent jump height cannot replace the required double jump"), Tree.Validate(Bypass, 3, 0));
	auto Island = Start;
	for (const TCHAR* Node : {TEXT("C.AirMomentum"), TEXT("C.GroundTurning"), TEXT("C.DoubleJump"), TEXT("C.DoubleJumpUpgrade")})
	{
		Invest(Island, Node, ERoute::Points, 1);
	}
	TestFalse(TEXT("neighbors without a path to the foundation cannot validate each other"), Tree.Validate(Island, 4, 0));
	auto Forged = Start;
	Invest(Forged, TEXT("C.EvadeRecovery"), ERoute::Legacy, 0);
	TestFalse(TEXT("cannot grant a common ability as a free legacy import"), Tree.Validate(Forged, 4, 0));
	return true;
}

#endif
