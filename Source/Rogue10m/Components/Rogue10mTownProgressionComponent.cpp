#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mProgressionSubsystem.h"

URogue10mTownProgressionComponent::URogue10mTownProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const URogue10mTraitCatalogDataAsset& URogue10mTownProgressionComponent::GetCatalog() const
{
	return TraitCatalog ? *TraitCatalog : *GetDefault<URogue10mTraitCatalogDataAsset>();
}

FRogue10mTraitDefinition URogue10mTownProgressionComponent::GetTraitDefinition(ERogue10mTownTrait Trait) const
{
	return GetCatalog().GetTraitDefinition(Trait);
}

TArray<FRogue10mTraitDefinition> URogue10mTownProgressionComponent::GetTraitDefinitions() const
{
	return GetCatalog().Definitions;
}

FRogue10mTraitDefinition URogue10mTownProgressionComponent::GetUpgradeDefinition(
	ERogue10mTownTrait Trait, const URogue10mTownServiceDataAsset* Data) const
{
	FRogue10mTraitDefinition Definition = GetTraitDefinition(Trait);
	if (Data && (Trait == ERogue10mTownTrait::Vitality || Trait == ERogue10mTownTrait::Power))
	{
		Definition.MaxRank = FMath::Min(Definition.MaxRank, Data->MaximumTraitLevel);
		Definition.ValuePerRank = Trait == ERogue10mTownTrait::Vitality
			? Data->VitalityHealthPerLevel : Data->PowerAttackPerLevel;
	}
	return Definition;
}

int32 URogue10mTownProgressionComponent::GetTraitLevel(ERogue10mTownTrait Trait) const
{
	const auto* Investment = TraitInvestments.FindByPredicate([Trait](const auto& Row) { return Row.Trait == Trait; });
	return Investment ? Investment->Rank : 0;
}

float URogue10mTownProgressionComponent::GetTraitBonus(ERogue10mTownTrait Trait) const
{
	const auto* Investment = TraitInvestments.FindByPredicate([Trait](const auto& Row) { return Row.Trait == Trait; });
	return Investment ? Investment->Bonus : 0.0f;
}

int32 URogue10mTownProgressionComponent::GetSpentTraitPoints() const
{
	int64 Total = 0;
	for (const auto& Investment : TraitInvestments) { Total += Investment.SpentPoints; }
	return static_cast<int32>(FMath::Clamp<int64>(Total, 0, MAX_int32));
}

int32 URogue10mTownProgressionComponent::GetAvailableTraitPoints() const
{
	const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner());
	if (State && State->GetSkillTreeProgressionComponent()) { return State->GetSkillTreeProgressionComponent()->GetAvailableGrowthPoints(); }
	return State ? FMath::Max(0, State->GetPlayerLevel() - 1) - GetSpentTraitPoints() : 0;
}

FRogue10mTownProgressionSnapshot URogue10mTownProgressionComponent::CaptureProgression() const
{
	FRogue10mTownProgressionSnapshot Snapshot;
	Snapshot.TraitInvestments = TraitInvestments;
	Snapshot.VitalityLevel = GetTraitLevel(ERogue10mTownTrait::Vitality);
	Snapshot.PowerLevel = GetTraitLevel(ERogue10mTownTrait::Power);
	Snapshot.TraitMaxHealthBonus = GetTraitBonus(ERogue10mTownTrait::Vitality);
	Snapshot.TraitAttackBonus = GetTraitBonus(ERogue10mTownTrait::Power);
	Snapshot.Difficulty = Difficulty;
	Snapshot.DifficultyModifiers = DifficultyModifiers;
	return Snapshot;
}

FRogue10mTownProgressionSnapshot URogue10mTownProgressionComponent::MigrateLegacySnapshot(
	const FRogue10mTownProgressionSnapshot& Snapshot)
{
	FRogue10mTownProgressionSnapshot Result = Snapshot;
	if (!Result.TraitInvestments.IsEmpty()) { return Result; }
	const auto Add = [&Result](ERogue10mTownTrait Trait, int32 Rank, float Bonus)
	{
		if (Rank > 0)
		{
			auto& Row = Result.TraitInvestments.AddDefaulted_GetRef();
			Row.Trait = Trait; Row.Rank = Rank; Row.SpentPoints = Rank; Row.Bonus = Bonus;
		}
	};
	Add(ERogue10mTownTrait::Vitality, Snapshot.VitalityLevel, Snapshot.TraitMaxHealthBonus);
	Add(ERogue10mTownTrait::Power, Snapshot.PowerLevel, Snapshot.TraitAttackBonus);
	return Result;
}

bool URogue10mTownProgressionComponent::ValidateSnapshot(
	const FRogue10mTownProgressionSnapshot& Snapshot, int32 EarnedPoints)
{
	const auto ValidScale = [](float Value) { return FMath::IsFinite(Value) && Value >= .1f && Value <= 10.0f; };
	const auto ValidLegacy = [](int32 Rank, float Bonus)
	{
		return Rank >= 0 && Rank <= 5 && FMath::IsFinite(Bonus) && Bonus >= 0.0f
			&& Bonus <= 200000.0f * Rank && (Rank != 0 || Bonus == 0.0f);
	};
	if (EarnedPoints < 0 || !ValidLegacy(Snapshot.VitalityLevel, Snapshot.TraitMaxHealthBonus)
		|| !ValidLegacy(Snapshot.PowerLevel, Snapshot.TraitAttackBonus)
		|| static_cast<uint8>(Snapshot.Difficulty) > static_cast<uint8>(ERogue10mDifficulty::Hard)
		|| !ValidScale(Snapshot.DifficultyModifiers.HealthMultiplier)
		|| !ValidScale(Snapshot.DifficultyModifiers.DamageMultiplier)
		|| !ValidScale(Snapshot.DifficultyModifiers.ExperienceMultiplier)) { return false; }
	const auto Canonical = MigrateLegacySnapshot(Snapshot);
	if (Canonical.TraitInvestments.Num() > static_cast<int32>(ERogue10mTownTrait::Count)) { return false; }
	TSet<ERogue10mTownTrait> Seen;
	int64 Spent = 0;
	int32 Vitality = 0, Power = 0;
	float HealthBonus = 0.0f, AttackBonus = 0.0f;
	for (const auto& Row : Canonical.TraitInvestments)
	{
		if (!URogue10mTraitCatalogDataAsset::IsSupportedTrait(Row.Trait) || Seen.Contains(Row.Trait)
			|| Row.Rank < 1 || Row.Rank > URogue10mTraitCatalogDataAsset::GetMaximumSupportedRank(Row.Trait)
			|| Row.SpentPoints < Row.Rank || Row.SpentPoints > 1000 * Row.Rank
			|| !FMath::IsFinite(Row.Bonus) || Row.Bonus < 0.0f
			|| Row.Bonus > URogue10mTraitCatalogDataAsset::GetMaximumBonusPerRank(Row.Trait) * Row.Rank) { return false; }
		if (Row.Trait == ERogue10mTownTrait::LastStand
			&& (EarnedPoints < 19 || Row.SpentPoints < 3 || Row.Bonus <= 0.0f)) { return false; }
		if (Row.Trait == ERogue10mTownTrait::Vitality)
		{
			if (Row.SpentPoints != Row.Rank) { return false; }
			Vitality = Row.Rank; HealthBonus = Row.Bonus;
		}
		if (Row.Trait == ERogue10mTownTrait::Power)
		{
			if (Row.SpentPoints != Row.Rank) { return false; }
			Power = Row.Rank; AttackBonus = Row.Bonus;
		}
		Spent += Row.SpentPoints;
		Seen.Add(Row.Trait);
	}
	return Spent <= EarnedPoints && Vitality == Snapshot.VitalityLevel && Power == Snapshot.PowerLevel
		&& HealthBonus == Snapshot.TraitMaxHealthBonus && AttackBonus == Snapshot.TraitAttackBonus;
}

bool URogue10mTownProgressionComponent::RestoreProgression(const FRogue10mTownProgressionSnapshot& Snapshot)
{
	const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner());
	const int32 TreeSpent = State && State->GetSkillTreeProgressionComponent() ? State->GetSkillTreeProgressionComponent()->GetSpentSkillTreePoints() : 0;
	if (!State || !State->HasAuthority() || !ValidateSnapshot(Snapshot, FMath::Max(0, State->GetPlayerLevel() - 1) - TreeSpent)) { return false; }
	const auto Canonical = MigrateLegacySnapshot(Snapshot);
	TraitInvestments = Canonical.TraitInvestments;
	Difficulty = Canonical.Difficulty;
	DifficultyModifiers = Canonical.DifficultyModifiers;
	NotifyChanged();
	return true;
}

FRogue10mEquipmentStatModifiers URogue10mTownProgressionComponent::GetStatModifiers() const
{
	FRogue10mEquipmentStatModifiers Result;
	Result.MaxHealthBonus = GetTraitBonus(ERogue10mTownTrait::Vitality);
	Result.AttackPowerBonus = GetTraitBonus(ERogue10mTownTrait::Power);
	return Result;
}

FRogue10mDifficultyModifiers URogue10mTownProgressionComponent::GetDifficultyModifiers() const
{
	return DifficultyModifiers;
}

bool URogue10mTownProgressionComponent::CanChangeProgression() const
{
	const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner());
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	return State && State->HasAuthority() && !State->IsCharacterDead()
		&& Run && Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart;
}

bool URogue10mTownProgressionComponent::CanChangeTraits() const
{
	if (!CanChangeProgression() || GetWorld()->GetNetMode() != NM_Standalone
		|| UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("StartHub")) { return false; }
	const UGameInstance* Instance = GetWorld()->GetGameInstance();
	const auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (Selected)
	{
		const auto* Persistence = Instance->GetSubsystem<URogue10mProgressionSubsystem>();
		return Selected->IsValid() && Persistence && Persistence->IsSaveReady();
	}
	return true;
}

bool URogue10mTownProgressionComponent::CanUpgradeTrait(
	ERogue10mTownTrait Trait, FText& OutReason, const URogue10mTownServiceDataAsset* Data) const
{
	OutReason = FText::GetEmpty();
	if (!CanChangeTraits())
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "TownOnly", "마을에서 살아 있는 상태로 출발 전에만 강화할 수 있습니다.");
		return false;
	}
	const FRogue10mTraitDefinition Def = GetUpgradeDefinition(Trait, Data);
	if (!URogue10mTraitCatalogDataAsset::IsSupportedTrait(Trait) || !GetCatalog().IsTuningValid()
		|| !Def.IsTuningValid() || (Data && !Data->IsTuningValid()))
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "InvalidTuning", "특성 설정을 확인해 주세요.");
		return false;
	}
	const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner());
	if (!State || State->GetPlayerLevel() < Def.UnlockLevel)
	{
		OutReason = FText::Format(NSLOCTEXT("Rogue10mTraits", "LevelLock", "LV {0} 해금"), FText::AsNumber(Def.UnlockLevel));
		return false;
	}
	if (GetTraitLevel(Trait) >= Def.MaxRank)
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "Maximum", "최대 단계입니다.");
		return false;
	}
	if (GetAvailableTraitPoints() < Def.PointCost)
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "NoPoints", "특성 포인트가 부족합니다.");
		return false;
	}
	return true;
}

bool URogue10mTownProgressionComponent::TryUpgradeTrait(ERogue10mTownTrait Trait, const URogue10mTownServiceDataAsset* Data)
{
	FText Reason;
	if (!CanUpgradeTrait(Trait, Reason, Data)) { return false; }
	const auto Def = GetUpgradeDefinition(Trait, Data);
	auto Candidate = CaptureProgression();
	auto* Row = Candidate.TraitInvestments.FindByPredicate([Trait](const auto& Investment) { return Investment.Trait == Trait; });
	if (!Row)
	{
		Row = &Candidate.TraitInvestments.AddDefaulted_GetRef();
		Row->Trait = Trait;
	}
	++Row->Rank;
	Row->SpentPoints += Def.PointCost;
	Row->Bonus += Def.ValuePerRank;
	if (Trait == ERogue10mTownTrait::Vitality) { Candidate.VitalityLevel = Row->Rank; Candidate.TraitMaxHealthBonus = Row->Bonus; }
	if (Trait == ERogue10mTownTrait::Power) { Candidate.PowerLevel = Row->Rank; Candidate.TraitAttackBonus = Row->Bonus; }
	return RestoreProgression(Candidate);
}

bool URogue10mTownProgressionComponent::TryResetTraits()
{
	if (!CanChangeTraits() || GetSpentTraitPoints() <= 0) { return false; }
	TraitInvestments.Reset();
	NotifyChanged();
	return true;
}

bool URogue10mTownProgressionComponent::TrySetDifficulty(ERogue10mDifficulty NewDifficulty, const URogue10mTownServiceDataAsset* Data)
{
	const URogue10mTownServiceDataAsset& Settings = Data ? *Data : *GetDefault<URogue10mTownServiceDataAsset>();
	if (!CanChangeProgression() || !Settings.IsTuningValid()
		|| static_cast<uint8>(NewDifficulty) > static_cast<uint8>(ERogue10mDifficulty::Hard)) { return false; }
	Difficulty = NewDifficulty;
	DifficultyModifiers = Settings.GetDifficultyModifiers(NewDifficulty);
	NotifyChanged();
	return true;
}

void URogue10mTownProgressionComponent::NotifyChanged()
{
	if (const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner()))
	{
		if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(State->GetPawn())) { Character->RefreshCharacterStats(false); }
	}
	OnTownProgressionChanged.Broadcast();
	if (const auto* State = Cast<ARogue10mPlayerState>(GetOwner()))
	{
		if (auto* Tree = State->GetSkillTreeProgressionComponent()) { Tree->NotifySharedPointsChanged(); }
	}
}
