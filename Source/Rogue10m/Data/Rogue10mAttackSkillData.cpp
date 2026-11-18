// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mAttackSkillData.h"

#include "Rogue10mAttributeSet.h"

URogue10mAttackSkillData::URogue10mAttackSkillData()
{
	ResourceCosts =
	{
		{ ERogue10mAttackResourceType::Health, 0.0f },
		{ ERogue10mAttackResourceType::Stamina, 0.0f }
	};
}

float URogue10mAttackSkillData::RollDamage(const URogue10mAttributeSet* SourceAttributes, bool& bOutCritical) const
{
	const float CharacterMinRatio = SourceAttributes ? SourceAttributes->GetMinDamageRatio() : 0.9f;
	const float CharacterMaxRatio = SourceAttributes ? SourceAttributes->GetMaxDamageRatio() : 1.1f;
	const float AdjustedA = FMath::Max(0.0f, CharacterMinRatio * MinDamageRatioMultiplier);
	const float AdjustedB = FMath::Max(0.0f, CharacterMaxRatio * MaxDamageRatioMultiplier);
	const float MinimumRatio = FMath::Min(AdjustedA, AdjustedB);
	const float MaximumRatio = FMath::Max(AdjustedA, AdjustedB);
	const float CharacterAttackPower = SourceAttributes ? SourceAttributes->GetAttackPower() : 0.0f;
	float RolledDamage = FMath::Max(0.0f, Damage + CharacterAttackPower)
		* FMath::FRandRange(MinimumRatio, MaximumRatio);

	const float CharacterCriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.0f;
	const float FinalCriticalChance = FMath::Clamp(CharacterCriticalChance + CriticalChanceBonus, 0.0f, 1.0f);
	bOutCritical = FMath::FRand() < FinalCriticalChance;
	if (bOutCritical)
	{
		const float CharacterCriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalDamageMultiplier() : 1.5f;
		const float FinalCriticalMultiplier = FMath::Max(0.0f, CharacterCriticalMultiplier + CriticalDamageMultiplierBonus);
		RolledDamage *= FinalCriticalMultiplier;
	}
	return RolledDamage;
}

bool URogue10mAttackSkillData::IsCommonLoadoutSlot(ERogue10mAttackInputSlot Slot)
{
	return Slot == ERogue10mAttackInputSlot::QSkill || Slot == ERogue10mAttackInputSlot::ESkill
		|| Slot == ERogue10mAttackInputSlot::RSkill || Slot == ERogue10mAttackInputSlot::Special;
}
bool URogue10mAttackSkillData::IsLoadoutSlot(ERogue10mAttackInputSlot Slot)
{
	return IsCommonLoadoutSlot(Slot) || Slot == ERogue10mAttackInputSlot::MouseChord
		|| Slot == ERogue10mAttackInputSlot::ChargedSpecial;
}
bool URogue10mAttackSkillData::RequiresLoadoutCharge() const
{
	return AllowedSlotPolicy == ERogue10mSkillSlotPolicy::RightChargeOnly
		|| (AllowedSlotPolicy == ERogue10mSkillSlotPolicy::Legacy
			&& (InputSlot == ERogue10mAttackInputSlot::ChargedPrimary || InputSlot == ERogue10mAttackInputSlot::ChargedSpecial));
}
bool URogue10mAttackSkillData::IsAllowedInLoadoutSlot(ERogue10mAttackInputSlot Slot) const
{
	if (SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique || !IsLoadoutSlot(Slot)) { return false; }
	if (AllowedSlotPolicy == ERogue10mSkillSlotPolicy::BasicOnly) { return false; }
	if (AllowedSlotPolicy == ERogue10mSkillSlotPolicy::MouseChordOnly) { return Slot == ERogue10mAttackInputSlot::MouseChord; }
	if (RequiresLoadoutCharge()) { return Slot == ERogue10mAttackInputSlot::ChargedSpecial; }
	return IsCommonLoadoutSlot(Slot);
}
FText URogue10mAttackSkillData::GetAllowedSlotText() const
{
	if (SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique || AllowedSlotPolicy == ERogue10mSkillSlotPolicy::BasicOnly)
		{ return FText::FromString(TEXT("기본공격/패시브: 장착 슬롯 제외")); }
	if (AllowedSlotPolicy == ERogue10mSkillSlotPolicy::MouseChordOnly) { return FText::FromString(TEXT("좌+우 동시 전용")); }
	if (RequiresLoadoutCharge()) { return FText::FromString(TEXT("우클릭 차징 전용")); }
	return FText::FromString(TEXT("공용: Q / E / R / 우클릭"));
}
