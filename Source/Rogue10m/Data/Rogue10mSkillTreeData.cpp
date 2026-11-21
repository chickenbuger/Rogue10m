#include "Rogue10mSkillTreeData.h"

URogue10mSkillTreeDataAsset::URogue10mSkillTreeDataAsset()
{
	BuildDefaultCatalog();
}

void URogue10mSkillTreeDataAsset::PostLoad()
{
	Super::PostLoad();
	// Older catalog assets serialized the Nodes array before common skills existed.
	if (!Nodes.IsEmpty() && !Nodes.ContainsByPredicate([](const auto& Node) { return Node.CommonSkill != ERogue10mCommonSkill::None; }))
	{
		AppendDefaultCommonWeb();
	}
	ApplyIdentityGrowthDescriptions();
}

void URogue10mSkillTreeDataAsset::ApplyIdentityGrowthDescriptions()
{
	for (auto& Node : Nodes)
	{
		if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance)
		{
			Node.Description = NSLOCTEXT("Rogue10mTree", "GrowthResonanceDescription",
				"최종 기술과 해당 등급에 도달하면 자동 공명합니다. 공명마다 활성 아이덴티티가 발동한 최종기의 추가 피해가 증가합니다.");
		}
	}
}

const FRogue10mSkillTreeNodeDefinition* URogue10mSkillTreeDataAsset::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const auto& Node) { return Node.NodeId == NodeId; });
}

const FRogue10mSkillTreeNodeDefinition* URogue10mSkillTreeDataAsset::FindCommonSkill(ERogue10mCommonSkill Skill) const
{
	return Skill == ERogue10mCommonSkill::None ? nullptr
		: Nodes.FindByPredicate([Skill](const auto& Node) { return Node.CommonSkill == Skill; });
}

bool URogue10mSkillTreeDataAsset::MeetsCommonPrerequisites(const FRogue10mSkillTreeNodeDefinition& Node, TFunctionRef<bool(FName)> IsLearned) const
{
	if (Node.CommonSkill == ERogue10mCommonSkill::Foundation) { return true; }
	if (Node.CommonRequiredSkill != ERogue10mCommonSkill::None)
	{
		const auto* Required = FindCommonSkill(Node.CommonRequiredSkill);
		if (!Required || !IsLearned(Required->NodeId)) { return false; }
	}
	return Node.CommonConnections.ContainsByPredicate(IsLearned);
}

TSet<FName> URogue10mSkillTreeDataAsset::GetReachableCommonNodes(const TSet<FName>& Candidates) const
{
	TSet<FName> Reachable;
	int32 PreviousCount;
	do
	{
		PreviousCount = Reachable.Num();
		for (const auto& Node : Nodes)
		{
			if (Node.CommonSkill != ERogue10mCommonSkill::None && Candidates.Contains(Node.NodeId) && !Reachable.Contains(Node.NodeId)
				&& MeetsCommonPrerequisites(Node, [&Reachable](FName Id) { return Reachable.Contains(Id); }))
			{
				Reachable.Add(Node.NodeId);
			}
		}
	} while (Reachable.Num() != PreviousCount);
	return Reachable;
}

ERogue10mSkillTreeTrack URogue10mSkillTreeDataAsset::ResolveTrack(ERogue10mWeaponType WeaponType)
{
	if (WeaponType == ERogue10mWeaponType::Knuckle) { return ERogue10mSkillTreeTrack::Brawler; }
	return WeaponType == ERogue10mWeaponType::GreatSword ? ERogue10mSkillTreeTrack::GreatSword : ERogue10mSkillTreeTrack::None;
}

bool URogue10mSkillTreeDataAsset::IsIdentityAllowed(ERogue10mSkillTreeTrack Track, ERogue10mSkillTreeIdentity Identity) const
{
	return Identity != ERogue10mSkillTreeIdentity::None && Nodes.ContainsByPredicate([Track, Identity](const auto& Node)
	{
		return Node.NodeType == ERogue10mHybridSkillTreeNodeType::IdentityEntry && Node.Identity == Identity && IsNodeInTrack(Node, Track);
	});
}

int32 URogue10mSkillTreeDataAsset::GetResonanceRank(const FRogue10mSkillTreeNodeDefinition& Node) const
{
	const auto* Nature = Nodes.FindByPredicate([&Node](const FRogue10mSkillTreeNodeDefinition& Candidate)
	{
		return Candidate.NodeType == ERogue10mHybridSkillTreeNodeType::IdentityChoice && Candidate.Identity == Node.Identity;
	});
	const int32 Variant = Nature ? Nature->Choices.IndexOfByPredicate([&Node](const FRogue10mSkillTreeChoiceDefinition& Choice)
	{
		return Choice.ChoiceId == Node.RequiredChoiceId;
	}) : INDEX_NONE;
	return Variant > 0 ? SecondResonanceRank : FirstResonanceRank;
}

bool URogue10mSkillTreeDataAsset::IsNodeInTrack(const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTrack Track)
{
	if (Node.CommonSkill != ERogue10mCommonSkill::None) { return true; }
	if (Track == ERogue10mSkillTreeTrack::None) { return false; }
	return Node.Track == Track || (Node.Track == ERogue10mSkillTreeTrack::None && Node.AllowedTracks.Contains(Track));
}

TArray<FRogue10mSkillTreeNodeDefinition> URogue10mSkillTreeDataAsset::GetNodes(ERogue10mSkillTreeTrack Track) const
{
	TArray<FRogue10mSkillTreeNodeDefinition> Result;
	for (const auto& Node : Nodes) { if (IsNodeInTrack(Node, Track)) { Result.Add(Node); } }
	return Result;
}

bool URogue10mSkillTreeDataAsset::IsValidCatalog(FText* OutReason) const
{
	const auto Fail = [OutReason]() { if (OutReason) { *OutReason = NSLOCTEXT("Rogue10mTree", "InvalidCatalog", "스킬트리 설정을 확인해 주세요."); } return false; };
	if (Nodes.IsEmpty() || Nodes.Num() > 1024 || MaxMasteryLevel < 2 || MaxMasteryLevel > 1000
		|| IdentityDeepenedRank < 1 || FirstResonanceRank < 1 || SecondResonanceRank <= FirstResonanceRank) { return Fail(); }
	for (float Value : { IdentityGaugePerHit, IdentityGaugeThreshold, IdentityProcCooldown, InternalEnergyBaseBonus,
		InternalEnergyStaminaScaling, FightingSpiritBaseBonus, FightingSpiritAttackScaling, ManaBaseBonus, ManaProcCost,
		IdentityAdvancedGaugeBonus, IdentityNatureDamageBonus, IdentityRangeMultiplier, CirculationCostMultiplier,
		FocusRecoveryMultiplier, ResonanceDamageBonus, IdentityDamageBonusPerRank, ManaCapacityPerRank })
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 100000.0f) { return Fail(); }
	}
	if (IdentityGaugeThreshold < 1.0f || IdentityRangeMultiplier < 1.0f || CirculationCostMultiplier > 1.0f || FocusRecoveryMultiplier > 1.0f) { return Fail(); }
	TSet<FName> Seen;
	TSet<FName> CommonIds;
	TSet<ERogue10mCommonSkill> CommonSkills;
	for (const auto& Node : Nodes)
	{
		if (Node.NodeId.IsNone() || Seen.Contains(Node.NodeId) || Node.PointCost < 0 || Node.PointCost > 1000000
			|| static_cast<uint8>(Node.Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
			|| static_cast<uint8>(Node.Identity) > static_cast<uint8>(ERogue10mSkillTreeIdentity::Mana)
			|| static_cast<uint8>(Node.NodeType) > static_cast<uint8>(ERogue10mHybridSkillTreeNodeType::Resonance)
			|| !FMath::IsFinite(Node.Position.X) || !FMath::IsFinite(Node.Position.Y)) { return Fail(); }
		Seen.Add(Node.NodeId);
		if (!FRogue10mProgressionRequirements::IsValid(Node.RequiredAll)) { return Fail(); }
		for (const auto& Requirement : Node.RequiredAll)
		{
			if (Requirement.Kind == ERogue10mProgressionRequirementKind::MasteryAtLeast && Requirement.Minimum > MaxMasteryLevel) { return Fail(); }
		}
		if (Node.bAutomaticUnlock && (Node.PointCost != 0 || !Node.Choices.IsEmpty())) { return Fail(); }
		if (Node.bAcceptHistoricalAutomaticUnlock)
		{
			const bool bTrainingNode = Node.Track == ERogue10mSkillTreeTrack::Brawler && Node.bAutomaticUnlock;
			const bool bPaidSwordTechnique = Node.Track == ERogue10mSkillTreeTrack::GreatSword
				&& Node.NodeType == ERogue10mHybridSkillTreeNodeType::Technique && !Node.bAutomaticUnlock && Node.PointCost > 0 && Node.Choices.IsEmpty();
			if (!bTrainingNode && !bPaidSwordTechnique) { return Fail(); }
		}
		else if (!Node.HistoricalAutomaticMasteryAlternatives.IsEmpty() || !Node.HistoricalAutomaticPrerequisiteGroups.IsEmpty()) { return Fail(); }
		if (Node.CommonSkill != ERogue10mCommonSkill::None)
		{
			const bool bFoundation = Node.CommonSkill == ERogue10mCommonSkill::Foundation;
			const bool bCorrectAcquisition = bFoundation
				? Node.NodeType == ERogue10mHybridSkillTreeNodeType::Root && Node.bAutomaticUnlock && Node.CommonRequiredSkill == ERogue10mCommonSkill::None
				: Node.NodeType == ERogue10mHybridSkillTreeNodeType::Passive && !Node.bAutomaticUnlock && Node.PointCost > 0;
			if (Node.Track != ERogue10mSkillTreeTrack::None || !Node.AllowedTracks.IsEmpty()
				|| Node.Identity != ERogue10mSkillTreeIdentity::None || !Node.PrerequisiteGroups.IsEmpty()
				|| !Node.RequiredAll.IsEmpty() || !Node.MasteryAlternatives.IsEmpty() || !Node.RequiredChoiceId.IsNone()
				|| !Node.Skill.IsNull() || !Node.GrantedSkills.IsEmpty() || !Node.Choices.IsEmpty()
				|| static_cast<uint8>(Node.CommonSkill) > static_cast<uint8>(ERogue10mCommonSkill::EvadeHaste)
				|| CommonSkills.Contains(Node.CommonSkill) || Node.CommonConnections.IsEmpty()
				|| !FMath::IsFinite(Node.CommonValue) || Node.CommonValue < 0.0f || Node.CommonValue > 10.0f
				|| !bCorrectAcquisition) { return Fail(); }
			CommonSkills.Add(Node.CommonSkill);
			CommonIds.Add(Node.NodeId);
		}
		else if ((Node.Track == ERogue10mSkillTreeTrack::None && Node.AllowedTracks.IsEmpty())
			|| !Node.CommonConnections.IsEmpty() || Node.CommonRequiredSkill != ERogue10mCommonSkill::None || Node.CommonValue != 0.0f) { return Fail(); }
		TSet<ERogue10mSkillTreeTrack> Tracks;
		for (auto Track : Node.AllowedTracks)
		{
			if (Track == ERogue10mSkillTreeTrack::None || static_cast<uint8>(Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
				|| Tracks.Contains(Track) || (Node.Identity != ERogue10mSkillTreeIdentity::None && !IsIdentityAllowed(Track, Node.Identity))) { return Fail(); }
			Tracks.Add(Track);
		}
		if (Node.Identity != ERogue10mSkillTreeIdentity::None && Node.Track != ERogue10mSkillTreeTrack::None && !IsIdentityAllowed(Node.Track, Node.Identity)) { return Fail(); }
		for (const auto* Alternatives : {&Node.MasteryAlternatives, &Node.HistoricalAutomaticMasteryAlternatives})
		{
			for (const auto& Requirement : *Alternatives)
			{
				if (Requirement.Track == ERogue10mSkillTreeTrack::None || static_cast<uint8>(Requirement.Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
					|| Requirement.Level < 1 || Requirement.Level > MaxMasteryLevel || !FMath::IsFinite(Requirement.AttackBonus)
					|| !FMath::IsFinite(Requirement.StaminaBonus) || Requirement.AttackBonus < 0.0f || Requirement.StaminaBonus < 0.0f) { return Fail(); }
			}
		}
		const auto& Reward = Node.StatReward;
		for (float Value : { Reward.AttackPowerBonus, Reward.DefenseBonus, Reward.MaxHealthBonus, Reward.MaxStaminaBonus,
			Reward.CriticalDamageMultiplierBonus, Reward.MoveSpeedMultiplierBonus, Reward.CriticalChanceBonus, Reward.AttackSpeedBonus, Reward.MoveSpeedBonus })
		{
			if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 100000.0f) { return Fail(); }
		}
		TSet<FName> Choices;
		for (const auto& Choice : Node.Choices)
		{
			if (Choice.ChoiceId.IsNone() || Choices.Contains(Choice.ChoiceId)) { return Fail(); }
			Choices.Add(Choice.ChoiceId);
		}
		if ((Node.NodeType == ERogue10mHybridSkillTreeNodeType::IdentityChoice) != !Node.Choices.IsEmpty()) { return Fail(); }
	}
	for (const auto& Node : Nodes)
	{
		if (Node.CommonSkill != ERogue10mCommonSkill::None)
		{
			if (Node.CommonRequiredSkill != ERogue10mCommonSkill::None
				&& (!CommonSkills.Contains(Node.CommonRequiredSkill) || Node.CommonRequiredSkill == Node.CommonSkill)) { return Fail(); }
			TSet<FName> Neighbors;
			for (FName NeighborId : Node.CommonConnections)
			{
				const auto* Neighbor = FindNode(NeighborId);
				if (NeighborId == Node.NodeId || Neighbors.Contains(NeighborId) || !Neighbor
					|| Neighbor->CommonSkill == ERogue10mCommonSkill::None || !Neighbor->CommonConnections.Contains(Node.NodeId)) { return Fail(); }
				Neighbors.Add(NeighborId);
			}
		}
		for (const auto* Groups : {&Node.PrerequisiteGroups, &Node.HistoricalAutomaticPrerequisiteGroups})
		{
			for (const auto& Group : *Groups)
			{
				if (Group.NodeIds.IsEmpty()) { return Fail(); }
				TSet<FName> GroupSeen;
				for (FName Id : Group.NodeIds)
				{
					if (!Seen.Contains(Id) || Id == Node.NodeId || GroupSeen.Contains(Id)) { return Fail(); }
					GroupSeen.Add(Id);
				}
			}
		}
	}
	// Common edges can form loops, but every skill needs a route that can actually be acquired.
	if (GetReachableCommonNodes(CommonIds).Num() != CommonIds.Num()) { return Fail(); }
	// Reject cycles across directed weapon and identity alternatives.
	TSet<FName> Visiting, Complete;
	TFunction<bool(FName)> Visit = [&](FName Id)
	{
		if (Visiting.Contains(Id)) { return false; }
		if (Complete.Contains(Id)) { return true; }
		Visiting.Add(Id);
		const auto& Node = *FindNode(Id);
		for (const auto* Groups : {&Node.PrerequisiteGroups, &Node.HistoricalAutomaticPrerequisiteGroups})
		{
			for (const auto& Group : *Groups)
			{
				for (FName Parent : Group.NodeIds) { if (!Visit(Parent)) { return false; } }
			}
		}
		Visiting.Remove(Id); Complete.Add(Id); return true;
	};
	for (const auto& Node : Nodes) { if (!Visit(Node.NodeId)) { return Fail(); } }
	return true;
}
