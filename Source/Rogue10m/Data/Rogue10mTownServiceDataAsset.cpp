#include "Rogue10mTownServiceDataAsset.h"

URogue10mTownServiceDataAsset::URogue10mTownServiceDataAsset()
{
	Easy.HealthMultiplier = 0.75f;
	Easy.DamageMultiplier = 0.75f;
	Easy.ExperienceMultiplier = 0.75f;
	Hard.HealthMultiplier = 1.5f;
	Hard.DamageMultiplier = 1.25f;
	Hard.ExperienceMultiplier = 1.5f;
}

FRogue10mDifficultyModifiers URogue10mTownServiceDataAsset::GetDifficultyModifiers(ERogue10mDifficulty Difficulty) const
{
	switch (Difficulty)
	{
	case ERogue10mDifficulty::Easy: return Easy;
	case ERogue10mDifficulty::Hard: return Hard;
	default: return Normal;
	}
}

bool URogue10mTownServiceDataAsset::IsTuningValid() const
{
	const auto Positive = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.1f && Value <= 10.0f; };
	const auto Nonnegative = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 200000.0f; };
	const auto ValidDifficulty = [&Positive](const FRogue10mDifficultyModifiers& Value)
	{
		return Positive(Value.HealthMultiplier) && Positive(Value.DamageMultiplier) && Positive(Value.ExperienceMultiplier);
	};
	return PotionGoldCost > 0 && EnhancementGoldCost > 0 && EnhancementGoldCost <= MAX_int32 / 5
		&& MaximumEnhancementLevel >= 1 && MaximumEnhancementLevel <= 5
		&& MaximumTraitLevel >= 1 && MaximumTraitLevel <= 5
		&& Nonnegative(EnhancementAttackPerLevel) && Nonnegative(EnhancementDefensePerLevel)
		&& Nonnegative(VitalityHealthPerLevel) && Nonnegative(PowerAttackPerLevel)
		&& ValidDifficulty(Easy) && ValidDifficulty(Normal) && ValidDifficulty(Hard);
}
