#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionRequirementTypes.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTownProgressionComponent.h"
#include "UObject/Package.h"

struct FRogue10mRequirementTestAccess
{
	static bool Attack(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill) { return Combat.ExecuteAttackSkill(Skill, false); }
};
namespace Rogue10mRequirementsTests
{
using K = ERogue10mProgressionRequirementKind;
using T = ERogue10mSkillTreeTrack;
using R = ERogue10mSkillTreeUnlockRoute;
static FRogue10mProgressionRequirement Requirement(K Kind, FName Id, int32 Minimum)
{ FRogue10mProgressionRequirement Result; Result.Kind = Kind; Result.Id = Id; Result.Minimum = Minimum; return Result; }
static TArray<FRogue10mProgressionRequirement> Mana()
{ return {Requirement(K::MasteryAtLeast, TEXT("GreatSword"), 3), Requirement(K::DisciplineRankAtLeast, TEXT("Discipline.Mage"), 2)}; }
static TArray<FRogue10mProgressionRequirement> InternalBladeRequirements()
{ return {Requirement(K::MasteryAtLeast, TEXT("GreatSword"), 3), Requirement(K::MasteryAtLeast, TEXT("Brawler"), 3)}; }
struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mPlayerState* Player = nullptr;
	URogue10mSkillTreeProgressionComponent* Tree = nullptr;
	URogue10mSkillTreeDataAsset* Catalog = nullptr;
	ARogue10mCharacter* Character = nullptr;
	ARogue10mTutorialNPC* NPC = nullptr;
	FFixture(bool bService = false)
	{
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(bService).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		// GetMapName uses the package, not the UWorld object's name. Never load/save the production hub.
		auto* Package = bService ? CreatePackage(TEXT("/Temp/Rogue10mRequirementsQA/StartHub")) : nullptr;
		if (Package) { Package->SetFlags(RF_Transient); }
		World = UWorld::CreateWorld(EWorldType::Game, false, bService ? FName(TEXT("StartHub")) : NAME_None, Package, true, ERHIFeatureLevel::Num, &Init);
		Player = World ? World->SpawnActor<ARogue10mPlayerState>() : nullptr;
		if (!Player) { return; }
		auto* ASC = Player->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(Player->GetRogueAttributeSet()); ASC->InitAbilityActorInfo(Player, Player);
		Player->GetRogueAttributeSet()->SetPlayerLevel(10.0f);
		Tree = Player->GetSkillTreeProgressionComponent();
		Catalog = NewObject<URogue10mSkillTreeDataAsset>(); Tree->Catalog = Catalog;
		Player->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
		Tree->RefreshAutomaticUnlocks();
		if (bService)
		{
			World->SetGameState(World->SpawnActor<ARogue10mGameState>());
			auto* Class = LoadClass<ARogue10mCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
			Character = Class ? World->SpawnActor<ARogue10mCharacter>(Class) : nullptr;
			if (Character) { Character->SetPlayerState(Player); ASC->InitAbilityActorInfo(Player, Character); }
			NPC = World->SpawnActor<ARogue10mTutorialNPC>();
			if (NPC) { NPC->SetActorLocation(FVector(200,0,0)); }
		}
	}
	~FFixture() { if (World) { World->DestroyWorld(false); } }
	bool Masteries(int32 SwordXP, int32 BrawlerXP)
	{
		auto Snapshot = Tree->Capture(); Snapshot.Masteries.Reset();
		for (auto Track : {T::GreatSword,T::Brawler}) { auto& Row = Snapshot.Masteries.AddDefaulted_GetRef(); Row.Track = Track; Row.TotalExperience = Track == T::GreatSword ? SwordXP : BrawlerXP; }
		return Tree->Restore(Snapshot);
	}
	FRogue10mSkillTreeNodeDefinition& AddNode(FName Id, bool bAutomatic = false)
	{
		auto& Node = Catalog->Nodes.AddDefaulted_GetRef(); Node.NodeId = Id; Node.Track = T::GreatSword;
		Node.NodeType = ERogue10mHybridSkillTreeNodeType::Stat; Node.bAutomaticUnlock = bAutomatic; Node.PointCost = bAutomatic ? 0 : 1;
		if (!bAutomatic) { auto& Alternative = Node.MasteryAlternatives.AddDefaulted_GetRef(); Alternative.Track = T::Brawler; Alternative.Level = 1; }
		return Node;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mRequirementTruthTest, "Rogue10m.ProgressionRequirements.TruthTable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mRequirementTruthTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mRequirementsTests;
	for (int32 Sword : {2,3}) for (int32 Brawler : {2,3}) for (int32 Mage : {0,1,2})
	{
		FRogue10mProgressionRequirementContext Context; Context.GreatSwordLevel = Sword; Context.BrawlerLevel = Brawler; Context.MageRank = Mage;
		const auto M = FRogue10mProgressionRequirements::Evaluate(Mana(), Context);
		const auto I = FRogue10mProgressionRequirements::Evaluate(InternalBladeRequirements(), Context);
		TestTrue(TEXT("supported context valid"), M.bValid && I.bValid);
		TestEqual(TEXT("sword AND mage truth table"), M.bSatisfied, Sword >= 3 && Mage >= 2);
		TestEqual(TEXT("sword AND brawler truth table"), I.bSatisfied, Sword >= 3 && Brawler >= 3);
		TestEqual(TEXT("missing list contains every unmet conjunct"), M.MissingRequirements.Num(), (Sword < 3 ? 1 : 0) + (Mage < 2 ? 1 : 0));
	}
	TestEqual(TEXT("mage starts zero"), FRogue10mProgressionRequirementContext().MageRank, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mRequirementInvalidTest, "Rogue10m.ProgressionRequirements.InvalidDataAndEmptyDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mRequirementInvalidTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mRequirementsTests;
	const FRogue10mProgressionRequirementContext Context;
	TestTrue(TEXT("empty required AND is true"), FRogue10mProgressionRequirements::Evaluate({}, Context).bSatisfied);
	for (auto Bad : {Requirement(K::MasteryAtLeast,TEXT("Sword"),3), Requirement(K::DisciplineRankAtLeast,TEXT("Mana"),2), Requirement(K::MasteryAtLeast,TEXT("GreatSword"),-1), Requirement(static_cast<K>(255),TEXT("GreatSword"),0)})
	{
		const auto Result = FRogue10mProgressionRequirements::Evaluate({Bad}, Context);
		TestFalse(TEXT("invalid requirement fails closed"), Result.bValid || Result.bSatisfied);
		auto* Catalog = NewObject<URogue10mSkillTreeDataAsset>(); Catalog->Nodes[0].RequiredAll = {Bad};
		TestFalse(TEXT("invalid requirement invalidates catalog"), Catalog->IsValidCatalog());
	}
	const auto Same = Requirement(K::MasteryAtLeast,TEXT("GreatSword"),3);
	TestFalse(TEXT("duplicate kind/id rejected"), FRogue10mProgressionRequirements::Evaluate({Same, Same}, Context).bValid);
	FRogue10mProgressionRequirementContext BadContext; BadContext.MageRank = -1;
	TestFalse(TEXT("negative rank input rejected"), FRogue10mProgressionRequirements::Evaluate({}, BadContext).bValid);
	auto* Catalog = NewObject<URogue10mSkillTreeDataAsset>();
	TestTrue(TEXT("all legacy nodes default to empty AND"), !Catalog->Nodes.ContainsByPredicate([](const auto& Node) { return !Node.RequiredAll.IsEmpty(); }));
	TestTrue(TEXT("legacy catalog remains valid"), Catalog->IsValidCatalog());
	Catalog->Nodes[0].RequiredAll = {Requirement(K::MasteryAtLeast,TEXT("GreatSword"),1001)};
	TestFalse(TEXT("impossible mastery threshold rejected"), Catalog->IsValidCatalog());
	Catalog->Nodes[0].RequiredAll.Reset(); Catalog->Nodes[0].PrerequisiteGroups.AddDefaulted();
	TestFalse(TEXT("empty inner prerequisite group invalid"), Catalog->IsValidCatalog());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mRequirementLegacyTest, "Rogue10m.ProgressionRequirements.LegacyAlternativesAndGroups", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mRequirementLegacyTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mRequirementsTests;
	for (bool bSword : {false,true})
	{
		FFixture F; if (!TestNotNull(TEXT("fixture tree"), F.Tree)) { return false; }
		auto& Node = F.AddNode(TEXT("Test.LegacyOR")); Node.PointCost = 0; Node.MasteryAlternatives.Reset();
		for (auto Track : {T::Brawler,T::GreatSword}) { auto& Row = Node.MasteryAlternatives.AddDefaulted_GetRef(); Row.Track = Track; Row.Level = 3; }
		TestTrue(TEXT("snapshot setup"), F.Masteries(bSword ? 25 : 10, bSword ? 10 : 25));
		TestTrue(TEXT("either legacy alternative is sufficient"), F.Tree->EvaluateNodeEligibility(Node.NodeId).CanAcquire(R::Mastery));
		Node.RequiredAll = InternalBladeRequirements();
		TestFalse(TEXT("mandatory AND cannot be replaced by either legacy route"), F.Tree->EvaluateNodeEligibility(Node.NodeId).CanAcquire(R::Mastery));
	}
	FFixture F;
	auto& Node = F.AddNode(TEXT("Test.Groups"));
	const auto Empty = F.Tree->EvaluateNodeEligibility(Node.NodeId);
	TestTrue(TEXT("empty outer prerequisites true"), Empty.bPrerequisitesSatisfied);
	FRogue10mSkillTreePrerequisiteGroup A; A.NodeIds = {TEXT("G.Root"),TEXT("G.Basic")};
	FRogue10mSkillTreePrerequisiteGroup B; B.NodeIds = {TEXT("F.Power.3"),TEXT("F.Breath.2")};
	Node.PrerequisiteGroups = {A,B};
	TestTrue(TEXT("one whole prerequisite group sufficient"), F.Tree->EvaluateNodeEligibility(Node.NodeId).bPrerequisitesSatisfied);
	Node.PrerequisiteGroups[0].NodeIds = {TEXT("G.Root"),TEXT("F.Power.3")};
	Node.PrerequisiteGroups[1].NodeIds = {TEXT("G.Basic"),TEXT("F.Breath.2")};
	TestFalse(TEXT("partial members from two groups cannot combine"), F.Tree->EvaluateNodeEligibility(Node.NodeId).bPrerequisitesSatisfied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mRequirementAcquisitionTest, "Rogue10m.ProgressionRequirements.LiveAcquisitionAndAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mRequirementAcquisitionTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mRequirementsTests;
	FFixture F(true);
	TestEqual(TEXT("transient service fixture package has hub short name"), F.World->GetMapName(), FString(TEXT("StartHub")));
	TestNotNull(TEXT("waiting game state"), F.World->GetGameState<ARogue10mGameState>());
	TestTrue(TEXT("pawn and state ownership linked"), F.Player->GetPawn() == F.Character);
	TestTrue(TEXT("trait service boundary open"), F.Player->GetTownProgressionComponent()->CanChangeTraits());
	TestTrue(TEXT("NPC service range"), F.NPC && F.NPC->IsWithinSkillTreeServiceRange(F.Character));
	if (!TestNotNull(TEXT("concrete character"),F.Character) || !TestTrue(TEXT("real service gate is open"), F.Tree->CanChangeInvestments())) { return false; }
	F.AddNode(TEXT("Test.Paid")).RequiredAll = InternalBladeRequirements();
	F.AddNode(TEXT("Test.Auto"),true).RequiredAll = InternalBladeRequirements();
	F.AddNode(TEXT("Test.Mage"),true).RequiredAll = Mana();
	TestTrue(TEXT("extended test catalog valid"), F.Catalog->IsValidCatalog());
	TestTrue(TEXT("M3 sword M2 brawler"), F.Masteries(25,10));
	F.Character->GetInventoryComponent()->AddGold(100000);
	FText Reason;
	for (auto Route : {R::Points,R::Mastery,R::Automatic,R::Legacy,static_cast<R>(255)})
	{
		TestFalse(TEXT("no acquisition route bypasses mandatory AND"), F.Tree->CanUnlockNode(TEXT("Test.Paid"),Route,NAME_None,Reason));
		TestFalse(TEXT("actual command rejects mandatory AND"), F.Tree->TryUnlockNode(TEXT("Test.Paid"),Route,NAME_None));
	}
	TestEqual(TEXT("UI state remains locked despite points and gold"), F.Tree->GetNodeState(TEXT("Test.Paid"),T::GreatSword), ERogue10mSkillTreeNodeState::Locked);
	TestTrue(TEXT("UI explains missing required ID"), F.Tree->GetNodeRequirementsText(TEXT("Test.Paid")).ToString().Contains(TEXT("Brawler")));
	TestFalse(TEXT("auto node locked by same AND"), F.Tree->IsNodeUnlocked(TEXT("Test.Auto")));
	for (auto Route : {R::Points,R::Mastery,R::Legacy})
	{
		auto Forged = F.Tree->Capture(); auto& Row = Forged.Investments.AddDefaulted_GetRef(); Row.NodeId = TEXT("Test.Paid"); Row.Route = Route; Row.SpentPoints = Route == R::Points ? 1 : 0;
		TestFalse(TEXT("snapshot cannot forge required conditions"), F.Tree->Validate(Forged,9,0,nullptr,F.Catalog));
	}
	TestTrue(TEXT("M3 plus M3 snapshot"), F.Masteries(25,25));
	TestEqual(TEXT("UI now available"), F.Tree->GetNodeState(TEXT("Test.Paid"),T::GreatSword), ERogue10mSkillTreeNodeState::Available);
	TestTrue(TEXT("paid route now possible"), F.Tree->CanUnlockNode(TEXT("Test.Paid"),R::Points,NAME_None,Reason));
	TestTrue(TEXT("mastery route now possible"), F.Tree->CanUnlockNode(TEXT("Test.Paid"),R::Mastery,NAME_None,Reason));
	TestTrue(TEXT("actual purchase commits"), F.Tree->TryUnlockNode(TEXT("Test.Paid"),R::Points,NAME_None));
	const auto Count = F.Tree->Capture().Investments.Num(); F.Tree->RefreshAutomaticUnlocks(); F.Tree->RefreshAutomaticUnlocks();
	TestTrue(TEXT("automatic acquisition uses same AND"), F.Tree->IsNodeUnlocked(TEXT("Test.Auto")));
	TestEqual(TEXT("automatic acquisition is idempotent"), F.Tree->Capture().Investments.Num(), Count);
	TestTrue(TEXT("legacy Mana identity can still be purchased"), F.Tree->TryUnlockNode(TEXT("I.Mana.Entry"),R::Points,NAME_None));
	TestEqual(TEXT("Mana identity does not imply mage rank"), F.Tree->GetRequirementContext().MageRank,0);
	TestFalse(TEXT("live MageRank2 node remains locked"), F.Tree->IsNodeUnlocked(TEXT("Test.Mage")));
	TestEqual(TEXT("gold untouched"), F.Character->GetInventoryComponent()->GetGold(),100000);
	TestFalse(TEXT("unknown node yields invalid result"), F.Tree->EvaluateNodeEligibility(TEXT("Unknown.Node")).bValid);
	auto* Skill = NewObject<URogue10mAttackSkillData>(); Skill->ResourceCosts = {{ERogue10mAttackResourceType::Stamina,10}}; Skill->HitStartDelaySeconds = 10; Skill->AttackCooldown = 0;
	F.Player->GetRogueAttributeSet()->SetStamina(100);
	TestTrue(TEXT("real attack acceptance after condition acquisition"), FRogue10mRequirementTestAccess::Attack(*F.Character->GetCombatComponent(),*Skill));
	TestEqual(TEXT("R11 debit remains exact"), F.Player->GetRogueAttributeSet()->GetStamina(),90.0f);
	TestEqual(TEXT("actual skill use recorded once"),F.Player->GetSkillUseCount(Skill->GetFName()),1);
	return true;
}
#endif
