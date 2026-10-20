#include "Rogue10mSkillTreeProgressionComponent.h"

#include "Rogue10mAttackSkillData.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"

namespace Rogue10mSkillTree
{
using FNode = FRogue10mSkillTreeNodeDefinition;
using FSnapshot = FRogue10mSkillTreeSnapshot;
using FInvestment = FRogue10mSkillTreeInvestment;
using ETrack = ERogue10mSkillTreeTrack;
using EIdentity = ERogue10mSkillTreeIdentity;
using EType = ERogue10mHybridSkillTreeNodeType;
using ERoute = ERogue10mSkillTreeUnlockRoute;

static bool IsTrack(ETrack Track) { return Track == ETrack::Brawler || Track == ETrack::GreatSword; }
static int32 RequiredTotal(int32 Level)
{
	const int64 Steps = FMath::Clamp(Level - 1, 0, 999);
	return static_cast<int32>(Steps * 10 + 5 * Steps * (Steps - 1) / 2);
}
static int32 TotalXP(const FSnapshot& Snapshot, ETrack Track)
{
	const auto* Row = Snapshot.Masteries.FindByPredicate([Track](const auto& Item) { return Item.Track == Track; });
	return Row ? Row->TotalExperience : 0;
}
static int32 Level(const FSnapshot& Snapshot, ETrack Track, const URogue10mSkillTreeDataAsset& Catalog)
{
	if (!IsTrack(Track)) { return 0; }
	int32 Result = 1;
	const int32 Total = TotalXP(Snapshot, Track);
	while (Result < Catalog.MaxMasteryLevel && Total >= RequiredTotal(Result + 1)) { ++Result; }
	return Result;
}
static const FInvestment* Find(const FSnapshot& Snapshot, FName Id)
{
	return Snapshot.Investments.FindByPredicate([Id](const auto& Row) { return Row.NodeId == Id; });
}
static bool Prerequisites(const FSnapshot& Snapshot, const FNode& Node)
{
	if (!Node.RequiredChoiceId.IsNone() && !Snapshot.Investments.ContainsByPredicate([&Node](const auto& Row) { return Row.ChoiceId == Node.RequiredChoiceId; })) { return false; }
	if (Node.PrerequisiteGroups.IsEmpty()) { return true; }
	for (const auto& Group : Node.PrerequisiteGroups)
	{
		bool bComplete = true;
		for (FName Id : Group.NodeIds) { bComplete &= Find(Snapshot, Id) != nullptr; }
		if (bComplete) { return true; }
	}
	return false;
}
static FRogue10mEquipmentStatModifiers Stats(const FSnapshot& Snapshot, ETrack Track, const URogue10mSkillTreeDataAsset& Catalog)
{
	FRogue10mEquipmentStatModifiers Result;
	for (const auto& Row : Snapshot.Investments)
	{
		const auto* Node = Catalog.FindNode(Row.NodeId);
		if (Node && Node->Track == Track && Node->NodeType == EType::Stat && Row.Route != ERoute::Legacy) { Result.Accumulate(Node->StatReward); }
	}
	return Result;
}
static bool Mastery(const FSnapshot& Snapshot, const FNode& Node, const URogue10mSkillTreeDataAsset& Catalog)
{
	for (const auto& Requirement : Node.MasteryAlternatives)
	{
		const auto Growth = Stats(Snapshot, Requirement.Track, Catalog);
		if (Level(Snapshot, Requirement.Track, Catalog) >= Requirement.Level
			&& Growth.AttackPowerBonus + KINDA_SMALL_NUMBER >= Requirement.AttackBonus
			&& Growth.MaxStaminaBonus + KINDA_SMALL_NUMBER >= Requirement.StaminaBonus) { return true; }
	}
	return Node.MasteryAlternatives.IsEmpty() && Node.bAutomaticUnlock;
}
static bool LearnedIdentity(const FSnapshot& Snapshot, EIdentity Identity, const URogue10mSkillTreeDataAsset& Catalog)
{
	if (Identity == EIdentity::None) { return false; }
	for (const auto& Row : Snapshot.Investments)
	{
		const auto* Node = Catalog.FindNode(Row.NodeId);
		if (Node && Node->Identity == Identity && Node->NodeType == EType::IdentityEntry) { return true; }
	}
	return false;
}
static bool ChoiceValid(const FNode& Node, FName Choice)
{
	return Node.Choices.IsEmpty() ? Choice.IsNone() : Node.Choices.ContainsByPredicate([Choice](const auto& Def) { return Def.ChoiceId == Choice; });
}
static int64 Spent(const FSnapshot& Snapshot)
{
	int64 Result = 0;
	for (const auto& Row : Snapshot.Investments) { Result += Row.SpentPoints; }
	return Result;
}
static const URogue10mSkillTreeDataAsset* ResolveCatalog(const TSoftObjectPtr<URogue10mSkillTreeDataAsset>& Reference)
{
	if (const auto* Loaded = Reference.Get()) { return Loaded; }
	const auto Path = Reference.ToSoftObjectPath();
	// Bootstrap authoring and isolated code tests may run before the optional package exists.
	return !Path.IsNull() && FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? Reference.LoadSynchronous() : nullptr;
}
}

URogue10mSkillTreeProgressionComponent::URogue10mSkillTreeProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Catalog = TSoftObjectPtr<URogue10mSkillTreeDataAsset>(FSoftObjectPath(TEXT("/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog.DA_HybridSkillTreeCatalog")));
}

const ARogue10mPlayerState* URogue10mSkillTreeProgressionComponent::GetPlayerState() const { return Cast<ARogue10mPlayerState>(GetOwner()); }
const URogue10mSkillTreeDataAsset* URogue10mSkillTreeProgressionComponent::GetDefaultCatalog()
{
	const auto& Reference = GetDefault<URogue10mSkillTreeProgressionComponent>()->Catalog;
	if (const auto* Configured = Rogue10mSkillTree::ResolveCatalog(Reference)) { return Configured; }
	return GetDefault<URogue10mSkillTreeDataAsset>();
}
const URogue10mSkillTreeDataAsset* URogue10mSkillTreeProgressionComponent::GetCatalog() const
{
	if (const auto* Configured = Rogue10mSkillTree::ResolveCatalog(Catalog)) { return Configured; }
	return GetDefaultCatalog();
}
const FRogue10mSkillTreeNodeDefinition* URogue10mSkillTreeProgressionComponent::FindNode(FName NodeId) const { return GetCatalog()->FindNode(NodeId); }
TArray<FRogue10mSkillTreeNodeDefinition> URogue10mSkillTreeProgressionComponent::GetNodes(ERogue10mSkillTreeTrack Track) const { return GetCatalog()->GetNodes(Track); }
ERogue10mSkillTreeTrack URogue10mSkillTreeProgressionComponent::GetActiveTrack() const
{
	const auto* Player = GetPlayerState();
	return Player ? ResolveTrack(Player->GetEquippedWeaponType()) : ERogue10mSkillTreeTrack::None;
}
int32 URogue10mSkillTreeProgressionComponent::GetMasteryLevel(ERogue10mSkillTreeTrack Track) const { return Rogue10mSkillTree::Level(State, Track, *GetCatalog()); }
int32 URogue10mSkillTreeProgressionComponent::GetMasteryTotalExperience(ERogue10mSkillTreeTrack Track) const { return Rogue10mSkillTree::TotalXP(State, Track); }
int32 URogue10mSkillTreeProgressionComponent::GetMasteryExperience(ERogue10mSkillTreeTrack Track) const
{
	const int32 Level = GetMasteryLevel(Track);
	return Level < 1 || Level >= GetCatalog()->MaxMasteryLevel ? 0 : GetMasteryTotalExperience(Track) - Rogue10mSkillTree::RequiredTotal(Level);
}
int32 URogue10mSkillTreeProgressionComponent::GetMasteryExperienceToNextLevel(ERogue10mSkillTreeTrack Track) const
{
	const int32 Level = GetMasteryLevel(Track);
	return Level < 1 || Level >= GetCatalog()->MaxMasteryLevel ? 0 : 10 + 5 * (Level - 1);
}
int32 URogue10mSkillTreeProgressionComponent::GetSpentSkillTreePoints() const { return static_cast<int32>(FMath::Clamp<int64>(Rogue10mSkillTree::Spent(State), 0, MAX_int32)); }
int32 URogue10mSkillTreeProgressionComponent::GetAvailableGrowthPoints() const
{
	const auto* Player = GetPlayerState();
	const auto* Town = Player ? Player->GetTownProgressionComponent() : nullptr;
	return Player && Town ? FMath::Max(0, Player->GetPlayerLevel() - 1) - Town->GetSpentTraitPoints() - GetSpentSkillTreePoints() : 0;
}
bool URogue10mSkillTreeProgressionComponent::IsNodeUnlocked(FName NodeId) const { return Rogue10mSkillTree::Find(State, NodeId) != nullptr; }
FName URogue10mSkillTreeProgressionComponent::GetSelectedChoice(FName NodeId) const
{
	const auto* Row = Rogue10mSkillTree::Find(State, NodeId); return Row ? Row->ChoiceId : NAME_None;
}
bool URogue10mSkillTreeProgressionComponent::PrerequisitesComplete(const FRogue10mSkillTreeNodeDefinition& Node) const { return Rogue10mSkillTree::Prerequisites(State, Node); }
bool URogue10mSkillTreeProgressionComponent::MasteryRequirementsComplete(const FRogue10mSkillTreeNodeDefinition& Node) const { return Rogue10mSkillTree::Mastery(State, Node, *GetCatalog()); }

ERogue10mSkillTreeNodeState URogue10mSkillTreeProgressionComponent::GetNodeState(FName NodeId, ERogue10mSkillTreeTrack Track) const
{
	const auto* Node = FindNode(NodeId);
	if (!Node || !URogue10mSkillTreeDataAsset::IsNodeInTrack(*Node, Track)) { return ERogue10mSkillTreeNodeState::Locked; }
	if (Node->Identity != ERogue10mSkillTreeIdentity::None && !GetCatalog()->IsIdentityAllowed(Track, Node->Identity)) { return ERogue10mSkillTreeNodeState::Locked; }
	if (IsNodeUnlocked(NodeId))
	{
		const bool bIdentityActive = Node->Identity != ERogue10mSkillTreeIdentity::None && GetActiveIdentity(Track) == Node->Identity;
		return bIdentityActive ? ERogue10mSkillTreeNodeState::Active : ERogue10mSkillTreeNodeState::Learned;
	}
	const bool bAvailable = PrerequisitesComplete(*Node)
		&& ((Node->PointCost > 0 && GetAvailableGrowthPoints() >= Node->PointCost) || MasteryRequirementsComplete(*Node));
	return bAvailable ? ERogue10mSkillTreeNodeState::Available : ERogue10mSkillTreeNodeState::Locked;
}

FText URogue10mSkillTreeProgressionComponent::GetNodeRequirementsText(FName NodeId) const
{
	const auto* Node = FindNode(NodeId);
	if (!Node) { return FText::GetEmpty(); }
	TArray<FString> Lines;
	if (!Node->PrerequisiteGroups.IsEmpty())
	{
		TArray<FString> Alternatives;
		for (const auto& Group : Node->PrerequisiteGroups)
		{
			TArray<FString> Parents;
			for (FName Id : Group.NodeIds)
			{
				const auto* Parent = FindNode(Id);
				Parents.Add(FString::Printf(TEXT("%s %s"), IsNodeUnlocked(Id) ? TEXT("완료") : TEXT("미완료"), Parent ? *Parent->DisplayName.ToString() : *Id.ToString()));
			}
			Alternatives.Add(FString::Join(Parents, TEXT(" + ")));
		}
		Lines.Add(TEXT("선행: ") + FString::Join(Alternatives, TEXT(" 또는 ")));
	}
	TArray<FString> Routes;
	if (Node->PointCost > 0) { Routes.Add(FString::Printf(TEXT("포인트 %dP (잔액 %dP)"), Node->PointCost, GetAvailableGrowthPoints())); }
	for (const auto& Requirement : Node->MasteryAlternatives)
	{
		FString Text = FString::Printf(TEXT("%s M%d (현재 M%d)"), Requirement.Track == ERogue10mSkillTreeTrack::Brawler ? TEXT("격투") : TEXT("대검"), Requirement.Level, GetMasteryLevel(Requirement.Track));
		const auto Growth = GetTrackMasteryStatModifiers(Requirement.Track);
		if (Requirement.AttackBonus > 0.0f) { Text += FString::Printf(TEXT(" + 숙련 공격력 %.0f/%.0f"), Growth.AttackPowerBonus, Requirement.AttackBonus); }
		if (Requirement.StaminaBonus > 0.0f) { Text += FString::Printf(TEXT(" + 숙련 스태미나 %.0f/%.0f"), Growth.MaxStaminaBonus, Requirement.StaminaBonus); }
		Routes.Add(Text + TEXT(" · 0P"));
	}
	if (!Routes.IsEmpty()) { Lines.Add(FString::Join(Routes, TEXT(" 또는 "))); }
	if (Node->bAutomaticUnlock) { Lines.Add(TEXT("조건 충족 시 무료 자동 전수")); }
	if (const auto* Row = Rogue10mSkillTree::Find(State, NodeId))
	{
		Lines.Add(Row->Route == ERogue10mSkillTreeUnlockRoute::Legacy ? TEXT("이전 전수 유지 · 숙련 성장 보상 제외") : FString::Printf(TEXT("전수 완료 · 실제 지출 %dP"), Row->SpentPoints));
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

bool URogue10mSkillTreeProgressionComponent::CanChangeInvestments() const
{
	const auto* Player = GetPlayerState();
	if (!Player || !Player->HasAuthority() || !Player->GetTownProgressionComponent() || !Player->GetTownProgressionComponent()->CanChangeTraits()) { return false; }
	const auto* Character = Cast<ARogue10mCharacter>(Player->GetPawn());
	if (!Character || !GetWorld()) { return false; }
	for (TActorIterator<ARogue10mTutorialNPC> It(GetWorld()); It; ++It)
	{
		if (It->IsWithinSkillTreeServiceRange(Character)) { return true; }
	}
	return false;
}

bool URogue10mSkillTreeProgressionComponent::CanUnlockNode(FName NodeId, ERogue10mSkillTreeUnlockRoute Route, FName ChoiceId, FText& OutReason) const
{
	const auto Fail = [&OutReason](const TCHAR* Reason) { OutReason = FText::FromString(Reason); return false; };
	OutReason = FText::GetEmpty();
	const auto* Node = FindNode(NodeId);
	if (!Node || !GetCatalog()->IsValidCatalog()) { return Fail(TEXT("스킬트리 설정을 확인해 주세요.")); }
	if (!CanChangeInvestments()) { return Fail(TEXT("마을 안내인 근처에서 살아 있는 상태로 출발 전에만 전수할 수 있습니다.")); }
	if (!URogue10mSkillTreeDataAsset::IsNodeInTrack(*Node, GetActiveTrack())) { return Fail(TEXT("현재 장착 무기의 계열에서 전수할 수 없습니다.")); }
	if (Node->Identity != ERogue10mSkillTreeIdentity::None && !GetCatalog()->IsIdentityAllowed(GetActiveTrack(), Node->Identity)) { return Fail(TEXT("현재 직업에서 사용할 수 없는 아이덴티티입니다.")); }
	if (IsNodeUnlocked(NodeId)) { return Fail(TEXT("이미 전수한 노드입니다.")); }
	if (Node->bAutomaticUnlock) { return Fail(TEXT("숙련 조건을 만족하면 자동 전수됩니다.")); }
	if (!PrerequisitesComplete(*Node)) { return Fail(TEXT("선행 노드를 먼저 전수해 주세요.")); }
	if (!Rogue10mSkillTree::ChoiceValid(*Node, ChoiceId)) { return Fail(TEXT("성질을 하나 선택해 주세요.")); }
	const auto* Player = GetPlayerState();
	if (!Validate(State, FMath::Max(0, Player->GetPlayerLevel() - 1), Player->GetTownProgressionComponent()->GetSpentTraitPoints(), nullptr, GetCatalog())) { return Fail(TEXT("성장 포인트 원장을 확인해 주세요.")); }
	if (Route == ERogue10mSkillTreeUnlockRoute::Points)
	{
		if (Node->PointCost <= 0 || GetAvailableGrowthPoints() < Node->PointCost) { return Fail(TEXT("공용 성장 포인트가 부족합니다.")); }
	}
	else if (Route == ERogue10mSkillTreeUnlockRoute::Mastery)
	{
		if (Node->MasteryAlternatives.IsEmpty() || !MasteryRequirementsComplete(*Node)) { return Fail(TEXT("숙련과 숙련 출처 영구 성장 조건이 부족합니다.")); }
	}
	else { return Fail(TEXT("허용되지 않는 전수 경로입니다.")); }
	return true;
}

bool URogue10mSkillTreeProgressionComponent::TryUnlockNode(FName NodeId, ERogue10mSkillTreeUnlockRoute Route, FName ChoiceId)
{
	FText Reason;
	if (!CanUnlockNode(NodeId, Route, ChoiceId, Reason)) { return false; }
	const auto& Node = *FindNode(NodeId);
	auto& Row = State.Investments.AddDefaulted_GetRef();
	Row.NodeId = NodeId; Row.Route = Route; Row.ChoiceId = ChoiceId;
	Row.SpentPoints = Route == ERogue10mSkillTreeUnlockRoute::Points ? Node.PointCost : 0;
	const bool bFirstIdentity = Node.NodeType == ERogue10mHybridSkillTreeNodeType::IdentityEntry && State.ActiveIdentity == ERogue10mSkillTreeIdentity::None;
	if (bFirstIdentity) { State.ActiveIdentity = Node.Identity; }
	RefreshAutomaticUnlocks();
	NotifyChanged(bFirstIdentity);
	return true;
}

bool URogue10mSkillTreeProgressionComponent::IsIdentityLearned(ERogue10mSkillTreeIdentity Identity) const { return Rogue10mSkillTree::LearnedIdentity(State, Identity, *GetCatalog()); }
ERogue10mSkillTreeIdentity URogue10mSkillTreeProgressionComponent::GetActiveIdentity() const { return GetActiveIdentity(GetActiveTrack()); }
ERogue10mSkillTreeIdentity URogue10mSkillTreeProgressionComponent::GetActiveIdentity(ERogue10mSkillTreeTrack Track) const
{
	return GetCatalog()->IsIdentityAllowed(Track, State.ActiveIdentity) && IsIdentityLearned(State.ActiveIdentity) ? State.ActiveIdentity : ERogue10mSkillTreeIdentity::None;
}
FName URogue10mSkillTreeProgressionComponent::GetSelectedProperty(ERogue10mSkillTreeIdentity Identity) const
{
	for (const auto& Row : State.Investments)
	{
		const auto* Node = FindNode(Row.NodeId);
		if (Node && Node->Identity == Identity && Node->NodeType == ERogue10mHybridSkillTreeNodeType::IdentityChoice) { return Row.ChoiceId; }
	}
	return NAME_None;
}
FName URogue10mSkillTreeProgressionComponent::GetActiveIdentityChoice() const { return GetSelectedProperty(GetActiveIdentity()); }
bool URogue10mSkillTreeProgressionComponent::IsIdentityDeepened(ERogue10mSkillTreeIdentity Identity) const
{
	for (const auto& Row : State.Investments)
	{
		const auto* Node = FindNode(Row.NodeId);
		if (Node && Node->Identity == Identity && Node->NodeType == ERogue10mHybridSkillTreeNodeType::IdentityAdvanced) { return true; }
	}
	return false;
}
bool URogue10mSkillTreeProgressionComponent::HasResonance(ERogue10mSkillTreeTrack Track, ERogue10mSkillTreeIdentity Identity) const
{
	if (GetActiveIdentity(Track) != Identity || Identity == ERogue10mSkillTreeIdentity::None) { return false; }
	for (const auto& Row : State.Investments)
	{
		const auto* Node = FindNode(Row.NodeId);
		if (Node && Node->Track == Track && Node->Identity == Identity && Node->NodeType == ERogue10mHybridSkillTreeNodeType::Resonance && PrerequisitesComplete(*Node)) { return true; }
	}
	return false;
}
bool URogue10mSkillTreeProgressionComponent::TrySetActiveIdentity(ERogue10mSkillTreeIdentity Identity)
{
	if (!CanChangeInvestments() || State.ActiveIdentity == Identity) { return false; }
	if (Identity != ERogue10mSkillTreeIdentity::None && (!GetCatalog()->IsIdentityAllowed(GetActiveTrack(), Identity) || !IsIdentityLearned(Identity))) { return false; }
	State.ActiveIdentity = Identity; NotifyChanged(true); return true;
}
bool URogue10mSkillTreeProgressionComponent::TrySetActiveChoice(FName NodeId, FName ChoiceId)
{
	// A learned nature is immutable until its actual investment is refunded.
	return TryUnlockNode(NodeId, ERogue10mSkillTreeUnlockRoute::Points, ChoiceId);
}
bool URogue10mSkillTreeProgressionComponent::Respec(ERogue10mSkillTreeIdentity Identity)
{
	if (!CanChangeInvestments() || static_cast<uint8>(Identity) > static_cast<uint8>(ERogue10mSkillTreeIdentity::Mana)) { return false; }
	const int32 Removed = State.Investments.RemoveAll([&](const auto& Row)
	{
		const auto* Node = FindNode(Row.NodeId);
		return Node && (Identity == ERogue10mSkillTreeIdentity::None
			? Node->Identity != ERogue10mSkillTreeIdentity::None || Row.SpentPoints > 0
			: Node->Identity == Identity);
	});
	if (Removed == 0) { return false; }
	if (!IsIdentityLearned(State.ActiveIdentity)) { State.ActiveIdentity = ERogue10mSkillTreeIdentity::None; }
	RefreshAutomaticUnlocks(); NotifyChanged(true); return true;
}

bool URogue10mSkillTreeProgressionComponent::RefundNode(FName NodeId)
{
	const auto* Node = FindNode(NodeId);
	if (!CanChangeInvestments() || !Node || (Node->NodeType != ERogue10mHybridSkillTreeNodeType::IdentityEntry
		&& Node->NodeType != ERogue10mHybridSkillTreeNodeType::IdentityAdvanced && Node->NodeType != ERogue10mHybridSkillTreeNodeType::IdentityChoice)) { return false; }
	auto Candidate = State;
	if (Candidate.Investments.RemoveAll([NodeId](const auto& Row) { return Row.NodeId == NodeId; }) == 0) { return false; }
	int32 Removed;
	do
	{
		TSet<FName> Orphaned;
		for (const auto& Row : Candidate.Investments)
		{
			const auto* Dependent = FindNode(Row.NodeId);
			// Free weapon mastery and historical combat grants never depend on reallocating an identity.
			if (Dependent && Dependent->Identity != ERogue10mSkillTreeIdentity::None
				&& Row.Route != ERogue10mSkillTreeUnlockRoute::Legacy && !Rogue10mSkillTree::Prerequisites(Candidate, *Dependent)) { Orphaned.Add(Row.NodeId); }
		}
		Removed = Candidate.Investments.RemoveAll([&Orphaned](const auto& Row) { return Orphaned.Contains(Row.NodeId); });
	} while (Removed > 0);
	if (!Rogue10mSkillTree::LearnedIdentity(Candidate, Candidate.ActiveIdentity, *GetCatalog())) { Candidate.ActiveIdentity = ERogue10mSkillTreeIdentity::None; }
	const auto* Player = GetPlayerState();
	if (!Player || !Validate(Candidate, FMath::Max(0, Player->GetPlayerLevel() - 1), Player->GetTownProgressionComponent()->GetSpentTraitPoints(), nullptr, GetCatalog())) { return false; }
	State = MoveTemp(Candidate);
	RefreshAutomaticUnlocks(); NotifyChanged(true); return true;
}

FRogue10mEquipmentStatModifiers URogue10mSkillTreeProgressionComponent::GetTrackMasteryStatModifiers(ERogue10mSkillTreeTrack Track) const
{
	return Rogue10mSkillTree::Stats(State, Track, *GetCatalog());
}
FRogue10mEquipmentStatModifiers URogue10mSkillTreeProgressionComponent::GetMasteryStatModifiers(ERogue10mWeaponType WeaponType) const { return GetTrackMasteryStatModifiers(ResolveTrack(WeaponType)); }
bool URogue10mSkillTreeProgressionComponent::IsSkillManaged(const URogue10mAttackSkillData* Skill) const
{
	if (!Skill) { return false; }
	const FSoftObjectPath Path(Skill);
	for (const auto& Node : GetCatalog()->Nodes)
	{
		if (Node.Skill.ToSoftObjectPath() == Path || Node.GrantedSkills.ContainsByPredicate([&Path](const auto& Ref) { return Ref.ToSoftObjectPath() == Path; })) { return true; }
	}
	return false;
}
bool URogue10mSkillTreeProgressionComponent::IsSkillGranted(const URogue10mAttackSkillData* Skill) const
{
	if (!Skill) { return false; }
	const FSoftObjectPath Path(Skill);
	const bool bLegacy = State.LegacyGrantedSkills.ContainsByPredicate([&Path](const auto& Ref) { return Ref.ToSoftObjectPath() == Path; });
	bool bManaged = false;
	for (const auto& Node : GetCatalog()->Nodes)
	{
		if (Node.Skill.ToSoftObjectPath() != Path && !Node.GrantedSkills.ContainsByPredicate([&Path](const auto& Ref) { return Ref.ToSoftObjectPath() == Path; })) { continue; }
		bManaged = true;
		if (Node.Identity != ERogue10mSkillTreeIdentity::None && GetActiveIdentity() != Node.Identity) { continue; }
		if (bLegacy || IsNodeUnlocked(Node.NodeId)) { return true; }
	}
	return bLegacy && !bManaged;
}
TArray<URogue10mAttackSkillData*> URogue10mSkillTreeProgressionComponent::GetGrantedSkills(ERogue10mSkillTreeTrack Track) const
{
	TArray<URogue10mAttackSkillData*> Result;
	for (const auto& Node : GetCatalog()->Nodes)
	{
		if (!URogue10mSkillTreeDataAsset::IsNodeInTrack(Node, Track)) { continue; }
		if (Node.Identity != ERogue10mSkillTreeIdentity::None && GetActiveIdentity(Track) != Node.Identity) { continue; }
		if (IsNodeUnlocked(Node.NodeId))
		{
			if (auto* Skill = Node.Skill.LoadSynchronous()) { Result.AddUnique(Skill); }
			for (const auto& Ref : Node.GrantedSkills) { if (auto* Skill = Ref.LoadSynchronous()) { Result.AddUnique(Skill); } }
		}
	}
	return Result;
}

void URogue10mSkillTreeProgressionComponent::NotifyConfirmedAttack(ERogue10mSkillTreeTrack Track)
{
	const auto* Player = GetPlayerState();
	if (!Player || !Player->HasAuthority() || Player->IsCharacterDead() || !Rogue10mSkillTree::IsTrack(Track) || !GetCatalog()->IsValidCatalog()) { return; }
	auto* Row = State.Masteries.FindByPredicate([Track](const auto& Item) { return Item.Track == Track; });
	if (!Row) { Row = &State.Masteries.AddDefaulted_GetRef(); Row->Track = Track; }
	const int32 Cap = Rogue10mSkillTree::RequiredTotal(GetCatalog()->MaxMasteryLevel);
	if (Row->TotalExperience >= Cap) { return; }
	++Row->TotalExperience;
	RefreshAutomaticUnlocks(); NotifyChanged();
}
void URogue10mSkillTreeProgressionComponent::RefreshAutomaticUnlocks()
{
	const auto* Player = GetPlayerState();
	if (bRefreshing || !Player || !Player->HasAuthority() || !GetCatalog()->IsValidCatalog()) { return; }
	TGuardValue<bool> Guard(bRefreshing, true);
	bool bChanged;
	do
	{
		bChanged = false;
		for (const auto& Node : GetCatalog()->Nodes)
		{
			if (!Node.bAutomaticUnlock || IsNodeUnlocked(Node.NodeId) || !PrerequisitesComplete(Node) || !MasteryRequirementsComplete(Node)) { continue; }
			auto& Row = State.Investments.AddDefaulted_GetRef(); Row.NodeId = Node.NodeId; Row.Route = ERogue10mSkillTreeUnlockRoute::Automatic;
			bChanged = true;
		}
	} while (bChanged);
}
void URogue10mSkillTreeProgressionComponent::NotifyWeaponChanged() { RefreshAutomaticUnlocks(); NotifyChanged(); }
void URogue10mSkillTreeProgressionComponent::NotifyChanged(bool bIdentityChanged)
{
	// Execution state, resource changes and derived stats are synchronized by consumers.
	(void)bIdentityChanged;
	OnChanged.Broadcast();
}

bool URogue10mSkillTreeProgressionComponent::Validate(const FRogue10mSkillTreeSnapshot& Snapshot, int32 EarnedPoints, int32 TraitSpentPoints, FText* OutReason, const URogue10mSkillTreeDataAsset* InCatalog)
{
	using namespace Rogue10mSkillTree;
	const auto& Catalog = InCatalog ? *InCatalog : *GetDefaultCatalog();
	const auto Fail = [OutReason]() { if (OutReason) { *OutReason = NSLOCTEXT("Rogue10mTree", "InvalidSnapshot", "숙련 또는 성장 포인트 기록이 맞지 않습니다. 기존 저장은 보존됩니다."); } return false; };
	if (!Catalog.IsValidCatalog() || EarnedPoints < 0 || TraitSpentPoints < 0 || TraitSpentPoints > EarnedPoints
		|| Snapshot.Masteries.Num() > 2 || Snapshot.Investments.Num() > Catalog.Nodes.Num() || Snapshot.LegacyGrantedSkills.Num() > 256
		|| static_cast<uint8>(Snapshot.ActiveIdentity) > static_cast<uint8>(EIdentity::Mana)) { return Fail(); }
	TSet<ETrack> SeenTracks;
	for (const auto& Row : Snapshot.Masteries)
	{
		if (!IsTrack(Row.Track) || SeenTracks.Contains(Row.Track) || Row.TotalExperience < 0 || Row.TotalExperience > RequiredTotal(Catalog.MaxMasteryLevel)) { return Fail(); }
		SeenTracks.Add(Row.Track);
	}
	TSet<FSoftObjectPath> Legacy;
	for (const auto& Ref : Snapshot.LegacyGrantedSkills)
	{
		const auto Path = Ref.ToSoftObjectPath();
		if (Ref.IsNull() || Legacy.Contains(Path) || !Ref.LoadSynchronous()) { return Fail(); }
		Legacy.Add(Path);
	}
	TSet<FName> Seen;
	for (const auto& Row : Snapshot.Investments)
	{
		const auto* Node = Catalog.FindNode(Row.NodeId);
		if (!Node || Seen.Contains(Row.NodeId) || Row.SpentPoints < 0 || Row.SpentPoints > 1000000
			|| static_cast<uint8>(Row.Route) > static_cast<uint8>(ERoute::Legacy) || !ChoiceValid(*Node, Row.ChoiceId)) { return Fail(); }
		Seen.Add(Row.NodeId);
		if (Row.Route == ERoute::Legacy)
		{
			if (Row.SpentPoints != 0 || (Node->NodeType != EType::Technique && Node->NodeType != EType::Passive) || Node->Skill.IsNull() || !Legacy.Contains(Node->Skill.ToSoftObjectPath())) { return Fail(); }
			continue;
		}
		if (!Prerequisites(Snapshot, *Node)) { return Fail(); }
		if (Row.Route == ERoute::Points)
		{
			if (Node->bAutomaticUnlock || Node->PointCost <= 0 || Row.SpentPoints <= 0) { return Fail(); }
		}
		else if (Row.Route == ERoute::Mastery)
		{
			if (Node->bAutomaticUnlock || Row.SpentPoints != 0 || Node->MasteryAlternatives.IsEmpty() || !Mastery(Snapshot, *Node, Catalog)) { return Fail(); }
		}
		else if (Row.SpentPoints != 0 || !Node->bAutomaticUnlock || !Mastery(Snapshot, *Node, Catalog)) { return Fail(); }
	}
	if (Spent(Snapshot) + TraitSpentPoints > EarnedPoints) { return Fail(); }
	if (Snapshot.ActiveIdentity != EIdentity::None && !LearnedIdentity(Snapshot, Snapshot.ActiveIdentity, Catalog)) { return Fail(); }
	return true;
}

bool URogue10mSkillTreeProgressionComponent::Restore(const FRogue10mSkillTreeSnapshot& Snapshot, bool bNotify)
{
	const auto* Player = GetPlayerState();
	const auto* Town = Player ? Player->GetTownProgressionComponent() : nullptr;
	return Town && RestoreForTransaction(Snapshot, bNotify, Town->GetSpentTraitPoints());
}

bool URogue10mSkillTreeProgressionComponent::RestoreForTransaction(const FRogue10mSkillTreeSnapshot& Snapshot, bool bNotify, int32 TraitSpentPoints)
{
	const auto* Player = GetPlayerState();
	const auto* Town = Player ? Player->GetTownProgressionComponent() : nullptr;
	if (!Player || !Player->HasAuthority() || !Town || !Validate(Snapshot, FMath::Max(0, Player->GetPlayerLevel() - 1), TraitSpentPoints, nullptr, GetCatalog())) { return false; }
	State = Snapshot;
	if (bNotify) { RefreshAutomaticUnlocks(); NotifyChanged(true); }
	return true;
}

FRogue10mSkillTreeSnapshot URogue10mSkillTreeProgressionComponent::MigrateLegacy(const TArray<TSoftObjectPtr<URogue10mAttackSkillData>>& Skills, const URogue10mSkillTreeDataAsset* InCatalog)
{
	const auto& Catalog = InCatalog ? *InCatalog : *GetDefaultCatalog();
	FRogue10mSkillTreeSnapshot Result;
	for (const auto& Skill : Skills) { if (!Skill.IsNull()) { Result.LegacyGrantedSkills.AddUnique(Skill); } }
	for (const auto& Node : Catalog.Nodes)
	{
		if ((Node.NodeType == ERogue10mHybridSkillTreeNodeType::Technique || Node.NodeType == ERogue10mHybridSkillTreeNodeType::Passive)
			&& !Node.Skill.IsNull() && Result.LegacyGrantedSkills.Contains(Node.Skill))
		{
			auto& Row = Result.Investments.AddDefaulted_GetRef(); Row.NodeId = Node.NodeId; Row.Route = ERogue10mSkillTreeUnlockRoute::Legacy;
		}
	}
	return Result;
}
