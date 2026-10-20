#include "Rogue10mSkillTreeData.h"

URogue10mSkillTreeDataAsset::URogue10mSkillTreeDataAsset()
{
	BuildDefaultCatalog();
}

const FRogue10mSkillTreeNodeDefinition* URogue10mSkillTreeDataAsset::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const auto& Node) { return Node.NodeId == NodeId; });
}

ERogue10mSkillTreeTrack URogue10mSkillTreeDataAsset::ResolveTrack(ERogue10mWeaponType WeaponType)
{
	if (WeaponType == ERogue10mWeaponType::Unarmed || WeaponType == ERogue10mWeaponType::Knuckle) { return ERogue10mSkillTreeTrack::Brawler; }
	return WeaponType == ERogue10mWeaponType::GreatSword ? ERogue10mSkillTreeTrack::GreatSword : ERogue10mSkillTreeTrack::None;
}

bool URogue10mSkillTreeDataAsset::IsIdentityAllowed(ERogue10mSkillTreeTrack Track, ERogue10mSkillTreeIdentity Identity) const
{
	return Identity != ERogue10mSkillTreeIdentity::None && Nodes.ContainsByPredicate([Track, Identity](const auto& Node)
	{
		return Node.NodeType == ERogue10mHybridSkillTreeNodeType::IdentityEntry && Node.Identity == Identity && IsNodeInTrack(Node, Track);
	});
}

bool URogue10mSkillTreeDataAsset::IsNodeInTrack(const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTrack Track)
{
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
	if (Nodes.IsEmpty() || Nodes.Num() > 1024 || MaxMasteryLevel < 2 || MaxMasteryLevel > 1000) { return Fail(); }
	for (float Value : { IdentityGaugePerHit, IdentityGaugeThreshold, IdentityProcCooldown, InternalEnergyBaseBonus,
		InternalEnergyStaminaScaling, FightingSpiritBaseBonus, FightingSpiritAttackScaling, ManaBaseBonus, ManaProcCost,
		IdentityAdvancedGaugeBonus, IdentityNatureDamageBonus, IdentityRangeMultiplier, CirculationCostMultiplier, FocusRecoveryMultiplier, ResonanceDamageBonus })
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 100000.0f) { return Fail(); }
	}
	if (IdentityGaugeThreshold < 1.0f || IdentityRangeMultiplier < 1.0f || CirculationCostMultiplier > 1.0f || FocusRecoveryMultiplier > 1.0f) { return Fail(); }
	TSet<FName> Seen;
	for (const auto& Node : Nodes)
	{
		if (Node.NodeId.IsNone() || Seen.Contains(Node.NodeId) || Node.PointCost < 0 || Node.PointCost > 1000000
			|| static_cast<uint8>(Node.Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
			|| static_cast<uint8>(Node.Identity) > static_cast<uint8>(ERogue10mSkillTreeIdentity::Mana)
			|| static_cast<uint8>(Node.NodeType) > static_cast<uint8>(ERogue10mHybridSkillTreeNodeType::Resonance)
			|| !FMath::IsFinite(Node.Position.X) || !FMath::IsFinite(Node.Position.Y)) { return Fail(); }
		Seen.Add(Node.NodeId);
		if (Node.bAutomaticUnlock && (Node.PointCost != 0 || !Node.Choices.IsEmpty())) { return Fail(); }
		if (Node.Track == ERogue10mSkillTreeTrack::None && Node.AllowedTracks.IsEmpty()) { return Fail(); }
		TSet<ERogue10mSkillTreeTrack> Tracks;
		for (auto Track : Node.AllowedTracks)
		{
			if (Track == ERogue10mSkillTreeTrack::None || static_cast<uint8>(Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
				|| Tracks.Contains(Track) || (Node.Identity != ERogue10mSkillTreeIdentity::None && !IsIdentityAllowed(Track, Node.Identity))) { return Fail(); }
			Tracks.Add(Track);
		}
		if (Node.Identity != ERogue10mSkillTreeIdentity::None && Node.Track != ERogue10mSkillTreeTrack::None && !IsIdentityAllowed(Node.Track, Node.Identity)) { return Fail(); }
		for (const auto& Requirement : Node.MasteryAlternatives)
		{
			if (Requirement.Track == ERogue10mSkillTreeTrack::None || static_cast<uint8>(Requirement.Track) > static_cast<uint8>(ERogue10mSkillTreeTrack::GreatSword)
				|| Requirement.Level < 1 || Requirement.Level > MaxMasteryLevel || !FMath::IsFinite(Requirement.AttackBonus)
				|| !FMath::IsFinite(Requirement.StaminaBonus) || Requirement.AttackBonus < 0.0f || Requirement.StaminaBonus < 0.0f) { return Fail(); }
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
		for (const auto& Group : Node.PrerequisiteGroups)
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
	// Reject cycles across all alternatives; progression may never depend on its own result.
	TSet<FName> Visiting, Complete;
	TFunction<bool(FName)> Visit = [&](FName Id)
	{
		if (Visiting.Contains(Id)) { return false; }
		if (Complete.Contains(Id)) { return true; }
		Visiting.Add(Id);
		for (const auto& Group : FindNode(Id)->PrerequisiteGroups) { for (FName Parent : Group.NodeIds) { if (!Visit(Parent)) { return false; } } }
		Visiting.Remove(Id); Complete.Add(Id); return true;
	};
	for (const auto& Node : Nodes) { if (!Visit(Node.NodeId)) { return Fail(); } }
	return true;
}
