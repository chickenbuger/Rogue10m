#include "Rogue10mSkillTreeCombatComponent.h"

#include "Engine/World.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillTreeProgressionComponent.h"

URogue10mSkillTreeCombatComponent::URogue10mSkillTreeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URogue10mSkillTreeProgressionComponent* URogue10mSkillTreeCombatComponent::GetProgression() const
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	const ARogue10mPlayerState* State = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	return State ? State->GetSkillTreeProgressionComponent() : nullptr;
}

ERogue10mSkillTreeIdentity URogue10mSkillTreeCombatComponent::GetIdentity() const
{
	const auto* Progression = GetProgression();
	return Progression ? Progression->GetActiveIdentity() : ERogue10mSkillTreeIdentity::None;
}

void URogue10mSkillTreeCombatComponent::SynchronizeIdentity()
{
	const auto* Progression = GetProgression();
	const auto Identity = GetIdentity();
	const auto Track = Progression ? Progression->GetActiveTrack() : ERogue10mSkillTreeTrack::None;
	const FName Property = Progression ? Progression->GetSelectedProperty(Identity) : NAME_None;
	if (Identity != CachedIdentity || Track != CachedTrack || Property != CachedProperty)
	{
		CachedIdentity = Identity;
		CachedTrack = Track;
		CachedProperty = Property;
		Gauge = 0.0f;
		CirculationExpiresAt = 0.0f;
		// Keep the shared cooldown across switching, refunds and re-investment.
	}
}

float URogue10mSkillTreeCombatComponent::GetGauge() const
{
	const auto* Progression = GetProgression();
	return GetIdentity() == CachedIdentity && Progression && Progression->GetActiveTrack() == CachedTrack ? Gauge : 0.0f;
}

float URogue10mSkillTreeCombatComponent::GetGaugeMaximum() const
{
	const auto* Progression = GetProgression();
	return Progression && Progression->GetCatalog() ? Progression->GetCatalog()->IdentityGaugeThreshold : 100.0f;
}

float URogue10mSkillTreeCombatComponent::GetCooldownRemaining() const
{
	return GetWorld() ? FMath::Max(0.0f, NextProcTime - GetWorld()->GetTimeSeconds()) : 0.0f;
}

void URogue10mSkillTreeCombatComponent::NotifyConfirmedAttack(ERogue10mSkillTreeTrack ExecutionTrack)
{
	SynchronizeIdentity();
	const auto* Progression = GetProgression();
	const auto* Data = Progression ? Progression->GetCatalog() : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Data || CachedIdentity == ERogue10mSkillTreeIdentity::None
		|| ExecutionTrack != CachedTrack) { return; }
	const float Charge = Data->IdentityGaugePerHit
		+ (Progression->IsIdentityDeepened(CachedIdentity) ? Data->IdentityAdvancedGaugeBonus : 0.0f);
	Gauge = FMath::Clamp(Gauge + Charge, 0.0f, GetGaugeMaximum());
}

float URogue10mSkillTreeCombatComponent::GetStaminaCostMultiplier(bool bComboAttack) const
{
	const auto* Progression = GetProgression();
	if (bComboAttack && Progression && GetIdentity() == ERogue10mSkillTreeIdentity::InternalEnergy
		&& CachedIdentity == GetIdentity() && CachedTrack == Progression->GetActiveTrack()
		&& GetWorld() && CirculationExpiresAt > GetWorld()->GetTimeSeconds())
	{
		return FMath::Clamp(Progression->GetCatalog()->CirculationCostMultiplier, 0.0f, 1.0f);
	}
	return 1.0f;
}

FRogue10mSkillTreeAttackEffect URogue10mSkillTreeCombatComponent::AcceptAttack(
	const URogue10mAttackSkillData& Skill, bool bComboAttack)
{
	SynchronizeIdentity();
	FRogue10mSkillTreeAttackEffect Result;
	const auto* Progression = GetProgression();
	const auto* Data = Progression ? Progression->GetCatalog() : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Data || !GetWorld()) { return Result; }
	if (bComboAttack) { CirculationExpiresAt = 0.0f; }
	if (CachedIdentity == ERogue10mSkillTreeIdentity::None || Gauge < GetGaugeMaximum() || GetCooldownRemaining() > 0.0f)
	{
		return Result;
	}
	bool bCore = false;
	bool bUltimate = false;
	for (const auto& Node : Data->Nodes)
	{
		if (Node.Skill.ToSoftObjectPath() != FSoftObjectPath(&Skill) || Node.Track != CachedTrack) { continue; }
		for (const auto& Requirement : Node.MasteryAlternatives)
		{
			bCore |= Requirement.Level >= 10;
			bUltimate |= Requirement.Level >= Data->MaxMasteryLevel;
		}
	}
	const bool bStrong = bCore || Skill.InputSlot == ERogue10mAttackInputSlot::ChargedPrimary
		|| Skill.InputSlot == ERogue10mAttackInputSlot::ChargedSpecial || Skill.InputSlot == ERogue10mAttackInputSlot::Special;
	if (CachedIdentity == ERogue10mSkillTreeIdentity::FightingSpirit && !bStrong) { return Result; }
	auto* Character = Cast<ARogue10mCharacter>(GetOwner());
	auto* Attributes = Character ? Character->GetRogueAttributeSet() : nullptr;
	if (CachedIdentity == ERogue10mSkillTreeIdentity::Mana
		&& (!Attributes || Attributes->GetMana() < Data->ManaProcCost)) { return Result; }
	const auto Growth = Progression->GetTrackMasteryStatModifiers(CachedTrack);
	const FName Property = Progression->GetSelectedProperty(CachedIdentity);
	float Bonus = 0.0f;
	if (CachedIdentity == ERogue10mSkillTreeIdentity::InternalEnergy)
	{
		Bonus = Data->InternalEnergyBaseBonus + Data->InternalEnergyStaminaScaling * Growth.MaxStaminaBonus;
		if (Property == TEXT("InternalEnergy.Cycle")) { CirculationExpiresAt = GetWorld()->GetTimeSeconds() + Data->IdentityProcCooldown; }
		if (Property == TEXT("InternalEnergy.Release") && bCore) { Result.RangeMultiplier = Data->IdentityRangeMultiplier; }
	}
	else if (CachedIdentity == ERogue10mSkillTreeIdentity::FightingSpirit)
	{
		Bonus = Data->FightingSpiritBaseBonus + Data->FightingSpiritAttackScaling * Growth.AttackPowerBonus;
		if (Property == TEXT("FightingSpirit.Burst")) { Bonus += Data->IdentityNatureDamageBonus; }
		if (Property == TEXT("FightingSpirit.Focus"))
		{
			Bonus *= 0.5f;
			Result.RecoveryMultiplier = Data->FocusRecoveryMultiplier;
		}
	}
	else if (CachedIdentity == ERogue10mSkillTreeIdentity::Mana)
	{
		Attributes->ConsumeMana(Data->ManaProcCost);
		Bonus = Data->ManaBaseBonus;
		if (Property == TEXT("Mana.Pursuit")) { Bonus += Data->IdentityNatureDamageBonus; }
		if (Property == TEXT("Mana.Spread") && bCore) { Result.RangeMultiplier = Data->IdentityRangeMultiplier; }
	}
	if (bUltimate && Progression->HasResonance(CachedTrack, CachedIdentity)) { Bonus += Data->ResonanceDamageBonus; }
	Result.DamageMultiplier = 1.0f + FMath::Max(0.0f, Bonus);
	Result.bTriggered = true;
	Gauge = 0.0f;
	NextProcTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.01f, Data->IdentityProcCooldown);
	return Result;
}
