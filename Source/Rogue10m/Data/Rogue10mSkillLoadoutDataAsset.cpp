// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mSkillLoadoutDataAsset.h"

FPrimaryAssetId URogue10mDodgeSkillDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Rogue10mDodgeSkill"), GetFName());
}

FPrimaryAssetId URogue10mWeaponSkillProfileDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Rogue10mWeaponSkillProfile"), GetFName());
}

URogue10mAttackSkillData* URogue10mWeaponSkillProfileDataAsset::FindDefaultSkill(
	ERogue10mAttackInputSlot InputSlot) const
{
	const TObjectPtr<URogue10mAttackSkillData>* Skill = DefaultSkillBindings.Find(InputSlot);
	return Skill ? Skill->Get() : nullptr;
}
bool URogue10mWeaponSkillProfileDataAsset::ValidateActiveTechniqueCatalog(FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	if (ActiveTechniqueCatalog.IsEmpty()) { return true; }
	const auto Fail = [&OutReason](const TCHAR* Reason)
	{
		OutReason = FText::FromString(Reason);
		return false;
	};
	if (WeaponType == ERogue10mWeaponType::Unarmed || WeaponType == ERogue10mWeaponType::Staff)
	{
		return Fail(TEXT("Novice and mage command profiles do not use the melee ten-technique catalog."));
	}
	if (ActiveTechniqueCatalog.Num() != RequiredActiveTechniqueCount)
	{
		return Fail(TEXT("An explicit melee catalog requires exactly ten techniques."));
	}
	TSet<FName> SeenIds;
	TSet<const URogue10mAttackSkillData*> SeenSkills;
	const URogue10mAttackSkillData* Basic = FindDefaultSkill(ERogue10mAttackInputSlot::Primary);
	for (const FRogue10mActiveTechniqueCatalogEntry& Entry : ActiveTechniqueCatalog)
	{
		const URogue10mAttackSkillData* Skill = Entry.Skill.Get();
		if (Entry.TechniqueId.IsNone() || SeenIds.Contains(Entry.TechniqueId))
		{
			return Fail(TEXT("TechniqueIds must be non-empty and unique."));
		}
		if (!Skill || SeenSkills.Contains(Skill))
		{
			return Fail(TEXT("Catalog skill references must be non-null and unique."));
		}
		if (Skill == Basic || Skill->SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique)
		{
			return Fail(TEXT("Fixed basic attacks and passive nodes are not selectable techniques."));
		}
		if (!SkillTreeSkills.Contains(Skill))
		{
			return Fail(TEXT("Catalog techniques must also belong to the preserved full progression tree."));
		}
		const ERogue10mSkillSlotPolicy Policy = Skill->AllowedSlotPolicy;
		if (Policy != ERogue10mSkillSlotPolicy::Common
			&& Policy != ERogue10mSkillSlotPolicy::MouseChordOnly
			&& Policy != ERogue10mSkillSlotPolicy::RightChargeOnly)
		{
			return Fail(TEXT("Catalog techniques require an explicit common, chord-only or right-charge-only policy."));
		}
		SeenIds.Add(Entry.TechniqueId);
		SeenSkills.Add(Skill);
	}
	return true;
}

TArray<URogue10mAttackSkillData*> URogue10mWeaponSkillProfileDataAsset::GetValidatedActiveTechniqueCatalog() const
{
	TArray<URogue10mAttackSkillData*> Result;
	FText Reason;
	if (!ValidateActiveTechniqueCatalog(Reason)) { return Result; }
	Result.Reserve(ActiveTechniqueCatalog.Num());
	for (const FRogue10mActiveTechniqueCatalogEntry& Entry : ActiveTechniqueCatalog) { Result.Add(Entry.Skill.Get()); }
	return Result;
}

URogue10mAttackSkillData* URogue10mWeaponSkillProfileDataAsset::FindActiveTechniqueById(FName TechniqueId) const
{
	FText Reason;
	if (TechniqueId.IsNone() || !ValidateActiveTechniqueCatalog(Reason)) { return nullptr; }
	for (const FRogue10mActiveTechniqueCatalogEntry& Entry : ActiveTechniqueCatalog)
	{
		if (Entry.TechniqueId == TechniqueId) { return Entry.Skill.Get(); }
	}
	return nullptr;
}

bool URogue10mWeaponSkillProfileDataAsset::CatalogAllowsLoadoutSkill(const URogue10mAttackSkillData* Skill) const
{
	if (!Skill) { return false; }
	if (!HasExplicitActiveTechniqueCatalog()) { return true; }
	FText Reason;
	return ValidateActiveTechniqueCatalog(Reason)
		&& ActiveTechniqueCatalog.ContainsByPredicate([Skill](const FRogue10mActiveTechniqueCatalogEntry& Entry)
		{ return Entry.Skill.Get() == Skill; });
}
