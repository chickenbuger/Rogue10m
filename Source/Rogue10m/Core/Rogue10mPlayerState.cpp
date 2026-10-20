// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Rogue10mAbilitySystemComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mTownProgressionComponent.h"

ARogue10mPlayerState::ARogue10mPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<URogue10mAbilitySystemComponent>(TEXT("Ability System Component"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<URogue10mAttributeSet>(TEXT("Attribute Set"));
	TownProgressionComponent = CreateDefaultSubobject<URogue10mTownProgressionComponent>(TEXT("Town Progression"));
}

UAbilitySystemComponent* ARogue10mPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARogue10mPlayerState::SetCharacterProfileIdentity(
	const FText& DisplayName, const FText& JobName)
{
	CharacterDisplayName = DisplayName;
	CharacterJobName = JobName;
}

void ARogue10mPlayerState::SetEquippedWeaponType(ERogue10mWeaponType NewWeaponType)
{
	EquippedWeaponType = NewWeaponType;
}

void ARogue10mPlayerState::SetCharacterDead(bool bNewCharacterDead)
{
	bCharacterDead = bNewCharacterDead;
}

void ARogue10mPlayerState::SetIdentityType(ERogue10mIdentityType NewIdentityType)
{
	IdentityType = NewIdentityType;
}

int32 ARogue10mPlayerState::GetPlayerLevel() const
{
	return AttributeSet ? FMath::Max(1, FMath::RoundToInt(AttributeSet->GetPlayerLevel())) : 1;
}

float ARogue10mPlayerState::GetCurrentExperience() const
{
	return AttributeSet ? FMath::Max(0.0f, AttributeSet->GetExperience()) : 0.0f;
}

float ARogue10mPlayerState::GetExperienceToNextLevel() const
{
	return AttributeSet ? FMath::Max(1.0f, AttributeSet->GetExperienceToNextLevel()) : 100.0f;
}

float ARogue10mPlayerState::GetExperienceNormalized() const
{
	const float RequiredExperience = GetExperienceToNextLevel();
	return RequiredExperience > 0.0f
		? FMath::Clamp(GetCurrentExperience() / RequiredExperience, 0.0f, 1.0f)
		: 0.0f;
}

void ARogue10mPlayerState::AddExperience(int32 ExperienceAmount)
{
	if (!HasAuthority() || !AttributeSet || ExperienceAmount <= 0)
	{
		return;
	}

	float CurrentExperience = GetCurrentExperience() + static_cast<float>(ExperienceAmount);
	float RequiredExperience = GetExperienceToNextLevel();
	int32 Level = GetPlayerLevel();

	while (CurrentExperience >= RequiredExperience)
	{
		CurrentExperience -= RequiredExperience;
		++Level;
		RequiredExperience = FMath::Max(100.0f, RequiredExperience * 1.18f);
	}

	AttributeSet->SetPlayerLevel(static_cast<float>(Level));
	AttributeSet->SetExperience(CurrentExperience);
	AttributeSet->SetExperienceToNextLevel(RequiredExperience);
	OnGrowthChanged.Broadcast();
}

void ARogue10mPlayerState::RecordSkillUse(FName SkillId, int32 Count)
{
	if (!HasAuthority() || SkillId.IsNone() || Count <= 0)
	{
		return;
	}

	int32& Value = SkillUseCounts.FindOrAdd(SkillId);
	Value = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Value) + Count));
	OnMartialArtsProgressChanged.Broadcast();
}

void ARogue10mPlayerState::RecordMonsterDefeat(FName MonsterId, int32 Count)
{
	if (!HasAuthority() || MonsterId.IsNone() || Count <= 0)
	{
		return;
	}

	int32& Value = MonsterDefeatCounts.FindOrAdd(MonsterId);
	Value = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Value) + Count));
	OnMartialArtsProgressChanged.Broadcast();
}

int32 ARogue10mPlayerState::GetSkillUseCount(FName SkillId) const
{
	return SkillId.IsNone() ? 0 : SkillUseCounts.FindRef(SkillId);
}

int32 ARogue10mPlayerState::GetMonsterDefeatCount(FName MonsterId) const
{
	return MonsterId.IsNone() ? 0 : MonsterDefeatCounts.FindRef(MonsterId);
}

int32 ARogue10mPlayerState::GetWeaponMasteryLevel(ERogue10mWeaponType WeaponType) const
{
	return WeaponType == ERogue10mWeaponType::Knuckle || WeaponType == ERogue10mWeaponType::Unarmed ? 1 : 0;
}

float ARogue10mPlayerState::GetIdentityCurrent() const
{
	return AttributeSet ? AttributeSet->GetIdentity() : 0.0f;
}

float ARogue10mPlayerState::GetIdentityMax() const
{
	return AttributeSet ? AttributeSet->GetMaxIdentity() : 100.0f;
}

float ARogue10mPlayerState::GetIdentityNormalized() const
{
	const float MaxIdentity = GetIdentityMax();
	return MaxIdentity > 0.0f ? FMath::Clamp(GetIdentityCurrent() / MaxIdentity, 0.0f, 1.0f) : 0.0f;
}

FRogue10mGrowthSnapshot ARogue10mPlayerState::CaptureGrowth() const
{
	FRogue10mGrowthSnapshot Snapshot;
	Snapshot.Level = GetPlayerLevel();
	Snapshot.Experience = GetCurrentExperience();
	Snapshot.ExperienceToNextLevel = GetExperienceToNextLevel();
	Snapshot.WeaponType = EquippedWeaponType;
	Snapshot.SkillUses = SkillUseCounts;
	Snapshot.MonsterDefeats = MonsterDefeatCounts;
	return Snapshot;
}

bool ARogue10mPlayerState::RestoreGrowth(const FRogue10mGrowthSnapshot& Snapshot)
{
	if (!HasAuthority() || !AttributeSet || Snapshot.Level < 1
		|| !FMath::IsFinite(Snapshot.Experience) || !FMath::IsFinite(Snapshot.ExperienceToNextLevel)
		|| Snapshot.Experience < 0.0f || Snapshot.ExperienceToNextLevel < 1.0f
		|| Snapshot.Experience >= Snapshot.ExperienceToNextLevel
		|| !StaticEnum<ERogue10mWeaponType>()->IsValidEnumValue(static_cast<int64>(Snapshot.WeaponType)))
	{
		return false;
	}
	for (const auto& Pair : Snapshot.SkillUses)
	{
		if (Pair.Key.IsNone() || Pair.Value < 0) { return false; }
	}
	for (const auto& Pair : Snapshot.MonsterDefeats)
	{
		if (Pair.Key.IsNone() || Pair.Value < 0) { return false; }
	}
	AttributeSet->SetPlayerLevel(static_cast<float>(Snapshot.Level));
	AttributeSet->SetExperience(Snapshot.Experience);
	AttributeSet->SetExperienceToNextLevel(Snapshot.ExperienceToNextLevel);
	EquippedWeaponType = Snapshot.WeaponType;
	SkillUseCounts = Snapshot.SkillUses;
	MonsterDefeatCounts = Snapshot.MonsterDefeats;
	OnGrowthChanged.Broadcast();
	OnMartialArtsProgressChanged.Broadcast();
	return true;
}
