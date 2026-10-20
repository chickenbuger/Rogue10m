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

	Add(TEXT("F.Root"), ETrack::Brawler, ENode::Root, TEXT("시작"), {0, 0}, 1).Description =
		FText::FromString(TEXT("맨손과 너클이 하나의 격투 숙련을 공유한다. 기본 공격과 회피는 아이덴티티 없이 사용할 수 있다."));
	{
		auto& Basic = Add(TEXT("F.Basic"), ETrack::Brawler, ENode::Basic, TEXT("기본기 · 연환권"), {0, -60}, 1, {TEXT("F.Root")});
		Basic.Description = FText::FromString(TEXT("기본기4와 연환권을 무료 전수한다. 기존 기본 공격·회피·콤보 바인딩은 유지한다."));
		for (const TCHAR* Path : {
			TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),
			TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),
			TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),
			TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),
			TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01") })
		{ Basic.GrantedSkills.Add(SkillPath(Path)); }
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

	Add(TEXT("G.Root"), ETrack::GreatSword, ENode::Root, TEXT("시작"), {0,0}, 1).Description =
		FText::FromString(TEXT("대검의 기본 전투를 보존하고 강타·연계·방어 수련으로 신규 핵심 기술을 배운다."));
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
		Node.Description = FText::FromString(TEXT("최종 기술과 선택 성질을 모두 전수하면0P로 공명. 해당 아이덴티티가 활성화된 최종기의 발동 추가 피해 배율 +0.10."));
	};
	Resonance(TEXT("F.Resonance.Cycle"), ETrack::Brawler, EIdentity::InternalEnergy, TEXT("순환 공명"), TEXT("F.Ultimate"), TEXT("I.InternalEnergy.Choice"), TEXT("InternalEnergy.Cycle"), {-360,315}, InternalColor);
	Resonance(TEXT("F.Resonance.Release"), ETrack::Brawler, EIdentity::InternalEnergy, TEXT("방출 공명"), TEXT("F.Ultimate"), TEXT("I.InternalEnergy.Choice"), TEXT("InternalEnergy.Release"), {-140,315}, InternalColor);
	Resonance(TEXT("F.Resonance.Burst"), ETrack::Brawler, EIdentity::FightingSpirit, TEXT("폭발 공명"), TEXT("F.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Burst"), {140,315}, SpiritColor);
	Resonance(TEXT("F.Resonance.Focus"), ETrack::Brawler, EIdentity::FightingSpirit, TEXT("집중 공명"), TEXT("F.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Focus"), {360,315}, SpiritColor);
	Resonance(TEXT("G.Resonance.Pursuit"), ETrack::GreatSword, EIdentity::Mana, TEXT("화염 · 추격 공명"), TEXT("G.Ultimate"), TEXT("I.Mana.Choice"), TEXT("Mana.Pursuit"), {-360,315}, ManaColor);
	Resonance(TEXT("G.Resonance.Spread"), ETrack::GreatSword, EIdentity::Mana, TEXT("화염 · 확산 공명"), TEXT("G.Ultimate"), TEXT("I.Mana.Choice"), TEXT("Mana.Spread"), {-140,315}, ManaColor);
	Resonance(TEXT("G.Resonance.Burst"), ETrack::GreatSword, EIdentity::FightingSpirit, TEXT("광전 · 폭발 공명"), TEXT("G.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Burst"), {140,315}, SpiritColor);
	Resonance(TEXT("G.Resonance.Focus"), ETrack::GreatSword, EIdentity::FightingSpirit, TEXT("광전 · 집중 공명"), TEXT("G.Ultimate"), TEXT("I.FightingSpirit.Choice"), TEXT("FightingSpirit.Focus"), {360,315}, SpiritColor);
}
