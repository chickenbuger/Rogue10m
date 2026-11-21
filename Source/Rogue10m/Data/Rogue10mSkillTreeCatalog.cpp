#include "Rogue10mSkillTreeData.h"

#include "Rogue10mAttackSkillData.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

namespace Rogue10mHybridCatalog
{
using ETrack = ERogue10mSkillTreeTrack;
using EIdentity = ERogue10mSkillTreeIdentity;
using ENode = ERogue10mHybridSkillTreeNodeType;

static TSoftObjectPtr<URogue10mAttackSkillData> SkillPath(const TCHAR* Path)
{
	const FString PackagePath(Path);
	const FString ObjectPath = PackagePath.Contains(TEXT(".")) ? PackagePath
		: PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
	return TSoftObjectPtr<URogue10mAttackSkillData>(FSoftObjectPath(ObjectPath));
}

static FRogue10mSkillTreePrerequisiteGroup All(const TArray<FName>& Ids)
{
	FRogue10mSkillTreePrerequisiteGroup Group;
	Group.NodeIds = Ids;
	return Group;
}

static FRogue10mSkillTreeMasteryRequirement Mastery(ETrack Track, int32 Level, float Attack = 0.f, float Stamina = 0.f)
{
	FRogue10mSkillTreeMasteryRequirement Requirement;
	Requirement.Track = Track;
	Requirement.Level = Level;
	Requirement.AttackBonus = Attack;
	Requirement.StaminaBonus = Stamina;
	return Requirement;
}

static FRogue10mSkillTreeChoiceDefinition Choice(const TCHAR* Id, const TCHAR* Name, const TCHAR* Description)
{
	FRogue10mSkillTreeChoiceDefinition Definition;
	Definition.ChoiceId = FName(Id);
	Definition.DisplayName = FText::FromString(Name);
	Definition.Description = FText::FromString(Description);
	return Definition;
}

struct FSwordInvestment
{
	FName NodeId;
	TArray<FRogue10mSkillTreePrerequisiteGroup> Prerequisites;
};

static const TArray<FSwordInvestment>& SwordInvestments()
{
	static const TArray<FSwordInvestment> Investments = {
		{ TEXT("G.CrushingSlash"), { All({TEXT("G.Power.1")}), All({TEXT("G.Flow.1")}) } },
		{ TEXT("G.Whirlwind"), { All({TEXT("G.Flow.1")}), All({TEXT("G.Body.1")}) } },
		{ TEXT("G.GuardBreak"), { All({TEXT("G.Body.1")}), All({TEXT("G.Power.1")}) } },
		{ TEXT("G.EarthSplitter"), { All({TEXT("G.Power.2")}), All({TEXT("G.Whirlwind"), TEXT("G.GuardBreak")}) } },
		{ TEXT("G.Ultimate"), {
			All({TEXT("G.Power.3"), TEXT("G.EarthSplitter")}),
			All({TEXT("G.Flow.3"), TEXT("G.Whirlwind")}),
			All({TEXT("G.Body.3"), TEXT("G.GuardBreak")}),
			All({TEXT("G.EarthSplitter"), TEXT("G.Whirlwind"), TEXT("G.GuardBreak")}) } }
	};
	return Investments;
}

}

void URogue10mSkillTreeDataAsset::ApplySwordInvestments()
{
	using namespace Rogue10mHybridCatalog;
	for (const FSwordInvestment& Investment : SwordInvestments())
	{
		FRogue10mSkillTreeNodeDefinition* Node = Nodes.FindByPredicate([&Investment](const FRogue10mSkillTreeNodeDefinition& Candidate)
		{
			return Candidate.NodeId == Investment.NodeId;
		});
		if (!Node) { continue; }

		Node->bAcceptHistoricalAutomaticUnlock = true;
		Node->HistoricalAutomaticMasteryAlternatives = MoveTemp(Node->MasteryAlternatives);
		Node->HistoricalAutomaticPrerequisiteGroups = MoveTemp(Node->PrerequisiteGroups);
		Node->PrerequisiteGroups = Investment.Prerequisites;
		Node->bAutomaticUnlock = false;
		Node->PointCost = 1;
		Node->Description = NSLOCTEXT("Rogue10mTree", "SwordTechniqueInvestment", "선행을 충족한 뒤 공용 성장 1P로 전수합니다. 전수한 기술은 공격 슬롯 또는 직접 키에 지정할 수 있습니다.");
	}
}

void URogue10mSkillTreeDataAsset::BuildDefaultCatalog()
{
	using namespace Rogue10mHybridCatalog;
	Nodes.Reset();
	MaxMasteryLevel = 20;
	const FLinearColor CombatColor(1.f, 0.57f, 0.14f, 1.f);
	const FLinearColor InternalColor(0.10f, 0.95f, 0.66f, 1.f);
	const FLinearColor SpiritColor(1.f, 0.21f, 0.14f, 1.f);
	const FLinearColor ManaColor(0.20f, 0.52f, 1.f, 1.f);

	// Positions are offsets from the 1040x760 graph's (520,390) origin.
	const auto Add = [&](const TCHAR* Id, ETrack Track, ENode Type, const TCHAR* Name,
		FVector2D Position, int32 Level, const TArray<FName>& Parents = TArray<FName>()) -> FRogue10mSkillTreeNodeDefinition&
	{
		auto& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(Id);
		Node.Track = Track;
		Node.NodeType = Type;
		Node.DisplayName = FText::FromString(Name);
		Node.Position = Position;
		Node.Ring = FMath::Clamp(FMath::RoundToInt(Position.Size() / 70.f), 0, 6);
		Node.Color = CombatColor;
		Node.bAutomaticUnlock = true;
		if (!Parents.IsEmpty()) { Node.PrerequisiteGroups.Add(All(Parents)); }
		if (Level > 1) { Node.MasteryAlternatives.Add(Mastery(Track, Level)); }
		return Node;
	};
	const auto Stat = [&](const TCHAR* Id, ETrack Track, const TCHAR* Name, FVector2D Position,
		int32 Level, const TCHAR* Parent, float Attack, float Stamina, float Health, float Defense, float Speed = 0.f)
	{
		auto& Node = Add(Id, Track, ENode::Stat, Name, Position, Level, { FName(Parent) });
		Node.Description = FText::FromString(TEXT("격투/무기 숙련 단계에 따라 자동 전수되는 영구 성장. 현재 무기 계열에 한 번만 적용된다."));
		Node.StatReward.AttackPowerBonus = Attack;
		Node.StatReward.MaxStaminaBonus = Stamina;
		Node.StatReward.MaxHealthBonus = Health;
		Node.StatReward.DefenseBonus = Defense;
		Node.StatReward.AttackSpeedBonus = Speed;
	};
	const auto Technique = [&](const TCHAR* Id, ETrack Track, const TCHAR* Name, FVector2D Position,
		int32 Level, const TCHAR* Parent, const TCHAR* Path, ENode Type = ENode::Technique)
	{
		auto& Node = Add(Id, Track, Type, Name, Position, Level, { FName(Parent) });
		Node.Skill = SkillPath(Path);
		Node.Description = FText::FromString(Type == ENode::Passive
			? TEXT("숙련과 선행을 충족하면 자동 전수되는 상시 심법. 공격 키를 지정하지 않는다.")
			: TEXT("숙련과 선행을 충족하면 포인트 소모 없이 자동 전수. 전수 후 기존 공격 슬롯 또는 직접 키에 지정한다."));
	};

	const auto KnucklePosition = [](double X, double Y) { return FVector2D(X - 520.0, Y - 390.0); };
	struct FFoundation
	{
		const TCHAR* NodeId;
		const TCHAR* Name;
		const TCHAR* Asset;
		double X;
	};
	const FFoundation Foundations[] = {
		{TEXT("F.Basic.LeftJab"), TEXT("왼손 잽"), TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"), 240.0},
		{TEXT("F.Basic.RightJab"), TEXT("오른손 잽"), TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"), 466.0},
		{TEXT("F.Basic.RightStraight"), TEXT("오른손 스트레이트"), TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"), 649.0},
		{TEXT("F.Basic.RightHook"), TEXT("차지 오른손 훅"), TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"), 885.0},
	};
	auto& BrawlerRoot = Add(TEXT("F.Root"), ETrack::Brawler, ENode::Root, TEXT("시작"), {0, 0}, 1);
	BrawlerRoot.bShowInTree = false;
	BrawlerRoot.Description = FText::FromString(TEXT("너클을 장착하고 유효 타격을 적중시키면 격투 숙련이 오릅니다."));
	{
		auto& Basic = Add(TEXT("F.Basic"), ETrack::Brawler, ENode::Basic, TEXT("기본기 · 연환권"), {0, -60}, 1, {TEXT("F.Root")});
		Basic.bShowInTree = false;
		Basic.Description = FText::FromString(TEXT("기본기4와 연환권을 무료 전수한다. 기존 기본 공격·회피·콤보 바인딩은 유지한다."));
		for (const auto& Foundation : Foundations) { Basic.GrantedSkills.Add(SkillPath(Foundation.Asset)); }
		Basic.GrantedSkills.Add(SkillPath(TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01")));
	}
	for (const auto& Foundation : Foundations)
	{
		auto& Node = Add(Foundation.NodeId, ETrack::Brawler, ENode::Basic, Foundation.Name, KnucklePosition(Foundation.X, 675.0), 1);
		Node.Skill = SkillPath(Foundation.Asset);
		Node.Description = FText::FromString(TEXT("처음부터 전수된 너클 기초 무공입니다. 연결된 수련의 선행 무공입니다."));
	}
	Stat(TEXT("F.Power.1"), ETrack::Brawler, TEXT("공격력 +1"), {0,-110}, 3, TEXT("F.Basic"), 1,0,0,0);
	Technique(TEXT("F.Bengquan"), ETrack::Brawler, TEXT("붕권"), {0,-160}, 5, TEXT("F.Power.1"), TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02"));
	Stat(TEXT("F.Power.2"), ETrack::Brawler, TEXT("공격력 +2"), {0,-210}, 8, TEXT("F.Bengquan"), 2,0,0,0);
	Technique(TEXT("F.DiamondStrike"), ETrack::Brawler, TEXT("금강쇄"), {0,-260}, 12, TEXT("F.Power.2"), TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight"));
	Stat(TEXT("F.Power.3"), ETrack::Brawler, TEXT("공격력 +2"), {0,-310}, 16, TEXT("F.DiamondStrike"), 2,0,0,0);
	Stat(TEXT("F.Breath.1"), ETrack::Brawler, TEXT("스태미나 +5"), {-90,-65}, 4, TEXT("F.Basic"), 0,5,0,0);
	Technique(TEXT("F.CloudHands"), ETrack::Brawler, TEXT("유운수"), {-185,-90}, 6, TEXT("F.Breath.1"), TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Jab"));
	Stat(TEXT("F.Breath.2"), ETrack::Brawler, TEXT("스태미나 +5"), {-255,-135}, 9, TEXT("F.CloudHands"), 0,5,0,0);
	Technique(TEXT("F.HundredFists"), ETrack::Brawler, TEXT("백열난무"), {-330,-190}, 13, TEXT("F.Breath.2"), TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03"));
	Stat(TEXT("F.Flow.3"), ETrack::Brawler, TEXT("공격 속도 +2%"), {-380,-250}, 17, TEXT("F.HundredFists"), 0,0,0,0,.02f);
	Stat(TEXT("F.Body.1"), ETrack::Brawler, TEXT("최대 HP +5"), {90,-55}, 4, TEXT("F.Basic"), 0,0,5,0);
	Technique(TEXT("F.Shoulder"), ETrack::Brawler, TEXT("철산고"), {180,-80}, 7, TEXT("F.Body.1"), TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged"));
	Stat(TEXT("F.Body.2"), ETrack::Brawler, TEXT("방어력 +1"), {250,-120}, 9, TEXT("F.Shoulder"), 0,0,0,1);
	Technique(TEXT("F.BreakingKick"), ETrack::Brawler, TEXT("파진각"), {320,-165}, 10, TEXT("F.Body.2"), TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special"));
	Stat(TEXT("F.Body.3"), ETrack::Brawler, TEXT("최대 HP +5"), {385,-220}, 14, TEXT("F.BreakingKick"), 0,0,5,0);
	Technique(TEXT("F.MountainSlam"), ETrack::Brawler, TEXT("태산압정"), {410,-285}, 15, TEXT("F.Body.3"), TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_JumpSlam"));
	Technique(TEXT("F.GoldenBody"), ETrack::Brawler, TEXT("금강불괴"), {370,-355}, 18, TEXT("F.MountainSlam"), TEXT("/Game/DataAsset/AttackSkill/MartialArts/DA_MartialArt_GoldenBody"), ENode::Passive);
	Technique(TEXT("F.Ultimate"), ETrack::Brawler, TEXT("무극패왕권"), {0,-360}, 20, TEXT("F.Power.3"), TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave"));
	Nodes.Last().PrerequisiteGroups = {
		All({TEXT("F.Power.3"), TEXT("F.DiamondStrike")}),
		All({TEXT("F.Flow.3"), TEXT("F.HundredFists")}),
		All({TEXT("F.Body.3"), TEXT("F.MountainSlam")}) };
	for (auto& Node : Nodes)
	{
		if (Node.NodeId == TEXT("F.Power.2") || Node.NodeId == TEXT("F.Power.3") || Node.NodeId == TEXT("F.Breath.2")
			|| Node.NodeId == TEXT("F.Flow.3") || Node.NodeId == TEXT("F.Body.3")) { Node.bShowInTree = false; }
	}
	const auto PlaceKnuckleNode = [&](FName Id, double X, double Y, const TArray<FName>& Parents) -> FRogue10mSkillTreeNodeDefinition&
	{
		auto* Node = Nodes.FindByPredicate([Id](const FRogue10mSkillTreeNodeDefinition& Candidate) { return Candidate.NodeId == Id; });
		check(Node);
		Node->Position = KnucklePosition(X, Y);
		Node->Ring = FMath::Clamp(FMath::RoundToInt(Node->Position.Size() / 70.f), 0, 6);
		Node->bAcceptHistoricalAutomaticUnlock = true;
		Node->HistoricalAutomaticMasteryAlternatives = Node->MasteryAlternatives;
		Node->HistoricalAutomaticPrerequisiteGroups = MoveTemp(Node->PrerequisiteGroups);
		Node->PrerequisiteGroups = { All(Parents) };
		return *Node;
	};
	const FText ChargeTraining = NSLOCTEXT("Rogue10mTree", "KnuckleChargeTraining", "숙련과 선행 무공을 충족하면 자동 전수됩니다. 전수 후 우클릭 차징 슬롯에 장착할 수 있습니다.");
	PlaceKnuckleNode(TEXT("F.Power.1"), 285.0, 483.0, {TEXT("F.Basic.LeftJab")});
	PlaceKnuckleNode(TEXT("F.Breath.1"), 470.0, 477.0, {TEXT("F.Basic.RightJab")});
	PlaceKnuckleNode(TEXT("F.Body.1"), 645.0, 475.0, {TEXT("F.Basic.RightStraight")});
	PlaceKnuckleNode(TEXT("F.Body.2"), 880.0, 465.0, {TEXT("F.Basic.RightHook")});
	PlaceKnuckleNode(TEXT("F.Bengquan"), 228.0, 300.0, {TEXT("F.Power.1")});
	PlaceKnuckleNode(TEXT("F.CloudHands"), 475.0, 296.0, {TEXT("F.Power.1"), TEXT("F.Breath.1")});
	PlaceKnuckleNode(TEXT("F.Shoulder"), 640.0, 274.0, {TEXT("F.Body.1")}).Description = ChargeTraining;
	PlaceKnuckleNode(TEXT("F.BreakingKick"), 170.0, 128.0, {TEXT("F.Bengquan")});
	PlaceKnuckleNode(TEXT("F.DiamondStrike"), 300.0, 137.0, {TEXT("F.Bengquan")});
	PlaceKnuckleNode(TEXT("F.HundredFists"), 425.0, 123.0, {TEXT("F.CloudHands")});
	PlaceKnuckleNode(TEXT("F.Ultimate"), 575.0, 120.0, {TEXT("F.CloudHands"), TEXT("F.Shoulder")}).Description = ChargeTraining;
	PlaceKnuckleNode(TEXT("F.MountainSlam"), 730.0, 104.0, {TEXT("F.Shoulder")}).Description =
		NSLOCTEXT("Rogue10mTree", "KnuckleAerialTraining", "숙련과 철산고 전수를 충족하면 자동 전수됩니다. 전수 후 공용 슬롯에 장착하며 공중에서 사용합니다.");
	PlaceKnuckleNode(TEXT("F.GoldenBody"), 880.0, 157.0, {TEXT("F.Body.2")});

	Add(TEXT("G.Root"), ETrack::GreatSword, ENode::Root, TEXT("시작"), {0,0}, 1).Description =
		FText::FromString(TEXT("대검 기본기에서 시작합니다. 강타·연계·방어 경로에서 공용 성장 포인트로 기술을 전수할 수 있습니다."));
	{
		auto& Basic = Add(TEXT("G.Basic"), ETrack::GreatSword, ENode::Basic, TEXT("대검 기본기"), {0,-60}, 1, {TEXT("G.Root")});
		Basic.Description = FText::FromString(TEXT("기존 대검3연계·특수·차징5기술과 회피를 유지한다. 성장 핵심기는 기존 동작을 재사용한 별도 전투 데이터다."));
		for (const TCHAR* Path : {
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Primary01"),
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Primary02"),
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Primary03"),
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Special"),
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Charged") })
		{ Basic.GrantedSkills.Add(SkillPath(Path)); }
	}
	Stat(TEXT("G.Power.1"), ETrack::GreatSword, TEXT("공격력 +1"), {0,-110}, 3, TEXT("G.Basic"), 1,0,0,0);
	Technique(TEXT("G.CrushingSlash"), ETrack::GreatSword, TEXT("분쇄참"), {0,-160}, 5, TEXT("G.Power.1"), TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_CrushingSlash"));
	Stat(TEXT("G.Power.2"), ETrack::GreatSword, TEXT("공격력 +2"), {0,-210}, 8, TEXT("G.CrushingSlash"), 2,0,0,0);
	Technique(TEXT("G.EarthSplitter"), ETrack::GreatSword, TEXT("대지파쇄"), {0,-260}, 12, TEXT("G.Power.2"), TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_EarthSplitter"));
	Stat(TEXT("G.Power.3"), ETrack::GreatSword, TEXT("공격력 +2"), {0,-310}, 16, TEXT("G.EarthSplitter"), 2,0,0,0);
	Stat(TEXT("G.Flow.1"), ETrack::GreatSword, TEXT("스태미나 +5"), {-90,-65}, 4, TEXT("G.Basic"), 0,5,0,0);
	Technique(TEXT("G.Whirlwind"), ETrack::GreatSword, TEXT("회전참"), {-185,-90}, 6, TEXT("G.Flow.1"), TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_Whirlwind"));
	Stat(TEXT("G.Flow.2"), ETrack::GreatSword, TEXT("스태미나 +5"), {-255,-135}, 9, TEXT("G.Whirlwind"), 0,5,0,0);
	Stat(TEXT("G.Flow.3"), ETrack::GreatSword, TEXT("공격 속도 +2%"), {-330,-190}, 17, TEXT("G.Flow.2"), 0,0,0,0,.02f);
	Stat(TEXT("G.Body.1"), ETrack::GreatSword, TEXT("최대 HP +5"), {90,-55}, 4, TEXT("G.Basic"), 0,0,5,0);
	Technique(TEXT("G.GuardBreak"), ETrack::GreatSword, TEXT("파쇄참"), {180,-80}, 7, TEXT("G.Body.1"), TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_GuardBreak"));
	Stat(TEXT("G.Body.2"), ETrack::GreatSword, TEXT("방어력 +1"), {250,-120}, 9, TEXT("G.GuardBreak"), 0,0,0,1);
	Stat(TEXT("G.Body.3"), ETrack::GreatSword, TEXT("최대 HP +5"), {320,-165}, 14, TEXT("G.Body.2"), 0,0,5,0);
	Technique(TEXT("G.Ultimate"), ETrack::GreatSword, TEXT("패왕참"), {0,-360}, 20, TEXT("G.Power.3"), TEXT("/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_Ultimate"));
	Nodes.Last().PrerequisiteGroups = {
		All({TEXT("G.Power.3"), TEXT("G.EarthSplitter")}),
		All({TEXT("G.Flow.3"), TEXT("G.Whirlwind")}),
		All({TEXT("G.Body.3"), TEXT("G.GuardBreak")}) };

	const auto Identity = [&](const TCHAR* Prefix, EIdentity IdentityType, const TCHAR* Name,
		bool bLeft, const FLinearColor& Color, const TArray<ETrack>& Allowed,
		const TArray<FRogue10mSkillTreeMasteryRequirement>& Alternatives,
		const TArray<FRogue10mSkillTreeChoiceDefinition>& Choices)
	{
		const FString EntryId = FString(Prefix) + TEXT(".Entry");
		const FString AdvancedId = FString(Prefix) + TEXT(".Advanced");
		const FString ChoiceId = FString(Prefix) + TEXT(".Choice");
		const float Side = bLeft ? -1.f : 1.f;
		const auto Setup = [&](FRogue10mSkillTreeNodeDefinition& Node, int32 Cost)
		{
			Node.bAutomaticUnlock = false;
			Node.PointCost = Cost;
			Node.AllowedTracks = Allowed;
			Node.Identity = IdentityType;
			Node.Color = Color;
		};
		auto& Entry = Add(*EntryId, ETrack::None, ENode::IdentityEntry,
			*FString::Printf(TEXT("%s 입문"), Name), {Side*250.f,80}, 1);
		Setup(Entry, 1);
		Entry.Description = FText::FromString(TEXT("숙련 조건 없이 공용 성장1P로 입문. 활성 아이덴티티는 하나이며 무기별 허용 계열만 적용된다."));
		for (ETrack Track : Allowed) { Entry.PrerequisiteGroups.Add(All({Track == ETrack::Brawler ? FName(TEXT("F.Root")) : FName(TEXT("G.Root"))})); }
		auto& Advanced = Add(*AdvancedId, ETrack::None, ENode::IdentityAdvanced,
			*FString::Printf(TEXT("%s 심화"), Name), {Side*300.f,160}, 1, {FName(*EntryId)});
		Setup(Advanced, 2);
		Advanced.MasteryAlternatives = Alternatives;
		Advanced.Description = FText::FromString(Alternatives.IsEmpty()
			? TEXT("입문 전수 뒤 공용 성장2P로 심화. 격투 숙련을 추가 필수로 요구하지 않는다. 유효 적중당 아이덴티티 충전량 +5.")
			: TEXT("입문 전수 뒤2P 또는 해당 숙련M12·영구 성장 조건으로 전수. 수련 전수의 실제 비용은0P다. 유효 적중당 아이덴티티 충전량 +5."));
		auto& Nature = Add(*ChoiceId, ETrack::None, ENode::IdentityChoice,
			*FString::Printf(TEXT("%s 성질"), Name), {Side*250.f,240}, 1, {FName(*AdvancedId)});
		Setup(Nature, 2);
		Nature.Choices = Choices;
		Nature.Description = FText::FromString(TEXT("공용 성장2P로 두 성질 중 하나를 선택한다. 두 효과를 동시에 구매하지 않으며 마을 재분배로 변경한다."));
	};
	Identity(TEXT("I.InternalEnergy"), EIdentity::InternalEnergy, TEXT("내공"), true, InternalColor,
		{ETrack::Brawler}, {Mastery(ETrack::Brawler,12,0,10)}, {
		Choice(TEXT("InternalEnergy.Cycle"), TEXT("순환"), TEXT("아이덴티티 발동 후 다음 연계의 스태미나 비용25% 감소.")),
		Choice(TEXT("InternalEnergy.Release"), TEXT("방출"), TEXT("아이덴티티 발동 핵심기의 범위35% 증가.")) });
	Identity(TEXT("I.FightingSpirit"), EIdentity::FightingSpirit, TEXT("투지"), false, SpiritColor,
		{ETrack::Brawler,ETrack::GreatSword}, {Mastery(ETrack::Brawler,12,3),Mastery(ETrack::GreatSword,12,3)}, {
		Choice(TEXT("FightingSpirit.Burst"), TEXT("폭발"), TEXT("아이덴티티가 발동한 강타·핵심기의 추가 피해 배율 +0.15. 대검의 광전 계열.")),
		Choice(TEXT("FightingSpirit.Focus"), TEXT("집중"), TEXT("강타 추가 피해를 절반으로 낮추고 발동 공격 후 회수 시간을25% 줄인다.")) });
	Identity(TEXT("I.Mana"), EIdentity::Mana, TEXT("마나"), true, ManaColor,
		{ETrack::GreatSword}, {}, {
		Choice(TEXT("Mana.Pursuit"), TEXT("추격"), TEXT("아이덴티티가 발동한 공격에 집중된 마력 후속 피해. 대검의 화염 계열.")),
		Choice(TEXT("Mana.Spread"), TEXT("확산"), TEXT("집중 후속 피해를 낮추고 발동 핵심기의 범위를35% 늘린다.")) });

	const auto Resonance = [&](const TCHAR* Id, ETrack Track, EIdentity IdentityType, const TCHAR* Name,
		const TCHAR* Ultimate, const TCHAR* ChoiceNode, const TCHAR* ChoiceId, FVector2D Position, const FLinearColor& Color)
	{
		auto& Node = Add(Id, Track, ENode::Resonance, Name, Position, 1, {FName(Ultimate),FName(ChoiceNode)});
		Node.Identity = IdentityType;
		Node.RequiredChoiceId = FName(ChoiceId);
		Node.Color = Color;
		Node.bShowInTree = Track != ETrack::Brawler;
	};
	Resonance(TEXT("F.Resonance.Cycle"), ETrack::Brawler, EIdentity::InternalEnergy, TEXT("순환 공명"), TEXT("F.Ultimate"), TEXT("I.InternalEnergy.Choice"), TEXT("InternalEnergy.Cycle"), {-360,315}, InternalColor);
	Resonance(TEXT("F.Resonance.Release"), ETrack::Brawler, EIdentity::InternalEnergy, TEXT("방출 공명"), TEXT("F.Ultimate"), TEXT("I.InternalEnergy.Choice"), TEXT("InternalEnergy.Release"), {-140,315}, InternalColor);
	Resonance(TEXT("F.Resonance.Burst"), ETrack::Brawler, EIdentity::FightingSpirit, TEXT("폭발 공명"), TEXT("F.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Burst"), {140,315}, SpiritColor);
	Resonance(TEXT("F.Resonance.Focus"), ETrack::Brawler, EIdentity::FightingSpirit, TEXT("집중 공명"), TEXT("F.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Focus"), {360,315}, SpiritColor);
	Resonance(TEXT("G.Resonance.Pursuit"), ETrack::GreatSword, EIdentity::Mana, TEXT("화염 · 추격 공명"), TEXT("G.Ultimate"), TEXT("I.Mana.Choice"), TEXT("Mana.Pursuit"), {-360,315}, ManaColor);
	Resonance(TEXT("G.Resonance.Spread"), ETrack::GreatSword, EIdentity::Mana, TEXT("화염 · 확산 공명"), TEXT("G.Ultimate"), TEXT("I.Mana.Choice"), TEXT("Mana.Spread"), {-140,315}, ManaColor);
	Resonance(TEXT("G.Resonance.Burst"), ETrack::GreatSword, EIdentity::FightingSpirit, TEXT("광전 · 폭발 공명"), TEXT("G.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Burst"), {140,315}, SpiritColor);
	Resonance(TEXT("G.Resonance.Focus"), ETrack::GreatSword, EIdentity::FightingSpirit, TEXT("광전 · 집중 공명"), TEXT("G.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Focus"), {360,315}, SpiritColor);
	ApplySwordInvestments();
	AppendDefaultCommonWeb();
	ApplyIdentityGrowthDescriptions();
}

void URogue10mSkillTreeDataAsset::AppendDefaultCommonWeb()
{
	using ESkill = ERogue10mCommonSkill;
	using ENode = ERogue10mHybridSkillTreeNodeType;
	const FLinearColor Gold(0.96f, 0.73f, 0.38f);
	const FLinearColor Sky(0.42f, 0.72f, 1.0f);
	const FLinearColor Jade(0.28f, 0.81f, 0.67f);
	const FLinearColor Amber(1.0f, 0.57f, 0.22f);
	TMap<ESkill, int32> Indices;
	const auto Add = [&](ESkill Skill, const TCHAR* Id, const TCHAR* Name, const TCHAR* Description,
		FVector2D Position, const FLinearColor& Color, float Value, ESkill Required = ESkill::None)
	{
		Indices.Add(Skill, Nodes.Num());
		auto& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(Id);
		Node.CommonSkill = Skill;
		Node.CommonRequiredSkill = Required;
		Node.CommonValue = Value;
		Node.DisplayName = FText::FromString(Name);
		Node.Description = FText::FromString(Description);
		Node.Position = Position;
		Node.Color = Color;
		Node.bAutomaticUnlock = Skill == ESkill::Foundation;
		Node.NodeType = Node.bAutomaticUnlock ? ENode::Root : ENode::Passive;
		Node.PointCost = Node.bAutomaticUnlock ? 0 : 1;
	};
	// Absolute coordinates in the common web's 1040×665 canvas.
	Add(ESkill::Foundation, TEXT("C.Foundation"), TEXT("기초 단련"),
		TEXT("공통 스킬트리의 시작점입니다. 연결된 능력부터 습득할 수 있습니다."), {520, 410}, Gold, 0.0f);
	Add(ESkill::AirControl, TEXT("C.AirControl"), TEXT("공중 제어"),
		TEXT("공중에서 방향을 조절하는 힘이 25% 증가합니다."), {520, 238}, Sky, 0.25f);
	Add(ESkill::DoubleJump, TEXT("C.DoubleJump"), TEXT("이단 점프"),
		TEXT("어떤 무기를 들고 있어도 공중에서 한 번 더 도약할 수 있습니다."), {396, 144}, Sky, 1.0f);
	Add(ESkill::JumpHeight, TEXT("C.JumpHeight"), TEXT("도약 높이 증가"),
		TEXT("점프의 수직 도약 속도가 8% 증가합니다."), {644, 144}, Sky, 0.08f);
	Add(ESkill::DoubleJumpUpgrade, TEXT("C.DoubleJumpUpgrade"), TEXT("이단 점프 강화"),
		TEXT("두 번째 점프의 수직 도약 속도에 12%를 더합니다."), {520, 57}, Sky, 0.12f, ESkill::DoubleJump);
	Add(ESkill::MoveSpeed, TEXT("C.MoveSpeed"), TEXT("이동 속도 증가"),
		TEXT("이동 속도가 5% 증가합니다."), {350, 410}, Jade, 0.05f);
	Add(ESkill::GroundTurning, TEXT("C.GroundTurning"), TEXT("방향 전환 향상"),
		TEXT("지상에서 방향을 바꿀 때 적용되는 마찰이 25% 증가합니다."), {205, 338}, Jade, 0.25f);
	Add(ESkill::GroundAcceleration, TEXT("C.GroundAcceleration"), TEXT("가속 향상"),
		TEXT("지상 이동 가속도가 15% 증가합니다."), {205, 510}, Jade, 0.15f);
	Add(ESkill::MoveSpeedUpgrade, TEXT("C.MoveSpeedUpgrade"), TEXT("이동 속도 강화"),
		TEXT("이동 속도 보너스에 5%를 더합니다."), {78, 410}, Jade, 0.05f, ESkill::MoveSpeed);
	Add(ESkill::EvadeRecovery, TEXT("C.EvadeRecovery"), TEXT("회피 후 회복 단축"),
		TEXT("회피 동작을 마치기까지 걸리는 시간이 15% 감소합니다."), {690, 410}, Amber, 0.15f);
	Add(ESkill::Roll, TEXT("C.Roll"), TEXT("구르기"),
		TEXT("Alt로 이동 방향으로 구릅니다. 이동 입력이 없으면 바라보는 방향으로 구릅니다."), {835, 338}, Amber, 1.0f);
	Add(ESkill::EvadeDistance, TEXT("C.EvadeDistance"), TEXT("회피 거리 증가"),
		TEXT("회피 동작의 이동 거리가 15% 증가합니다."), {835, 510}, Amber, 0.15f);
	Add(ESkill::RollCooldown, TEXT("C.RollCooldown"), TEXT("구르기 재사용 단축"),
		TEXT("구르기의 재사용 대기시간이 최대 20% 감소합니다. 동작이 끝난 뒤 다시 구를 수 있습니다."),
		{962, 410}, Amber, 0.20f, ESkill::Roll);
	Add(ESkill::AirMomentum, TEXT("C.AirMomentum"), TEXT("공중 속도 유지"),
		TEXT("공중에서 방향 입력을 놓았을 때 감속이 40% 감소합니다."), {251, 218}, Sky, 0.40f);
	Add(ESkill::LandingStability, TEXT("C.LandingStability"), TEXT("착지 안정"),
		TEXT("착지 후 0.25초 동안 지상에서 제동과 방향 전환에 적용되는 마찰이 50% 증가합니다."), {789, 218}, Sky, 0.50f);
	Add(ESkill::EvadeHaste, TEXT("C.EvadeHaste"), TEXT("회피 후 가속"),
		TEXT("회피 동작을 마친 뒤 0.8초 동안 이동 속도가 10% 증가합니다."), {520, 572}, Jade, 0.10f);

	const ESkill Edges[][2] = {
		{ESkill::Foundation, ESkill::AirControl}, {ESkill::Foundation, ESkill::MoveSpeed}, {ESkill::Foundation, ESkill::EvadeRecovery},
		{ESkill::AirControl, ESkill::DoubleJump}, {ESkill::AirControl, ESkill::JumpHeight},
		{ESkill::DoubleJump, ESkill::DoubleJumpUpgrade}, {ESkill::JumpHeight, ESkill::DoubleJumpUpgrade},
		{ESkill::MoveSpeed, ESkill::GroundTurning}, {ESkill::MoveSpeed, ESkill::GroundAcceleration},
		{ESkill::GroundTurning, ESkill::MoveSpeedUpgrade}, {ESkill::GroundAcceleration, ESkill::MoveSpeedUpgrade},
		{ESkill::EvadeRecovery, ESkill::Roll}, {ESkill::EvadeRecovery, ESkill::EvadeDistance},
		{ESkill::Roll, ESkill::RollCooldown}, {ESkill::EvadeDistance, ESkill::RollCooldown},
		{ESkill::GroundTurning, ESkill::AirMomentum}, {ESkill::AirMomentum, ESkill::DoubleJump},
		{ESkill::JumpHeight, ESkill::LandingStability}, {ESkill::LandingStability, ESkill::Roll},
		{ESkill::GroundAcceleration, ESkill::EvadeHaste}, {ESkill::EvadeHaste, ESkill::EvadeDistance}
	};
	for (const auto& Edge : Edges)
	{
		auto& First = Nodes[Indices.FindChecked(Edge[0])];
		auto& Second = Nodes[Indices.FindChecked(Edge[1])];
		First.CommonConnections.Add(Second.NodeId);
		Second.CommonConnections.Add(First.NodeId);
	}
}
