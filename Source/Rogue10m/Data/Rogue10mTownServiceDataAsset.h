// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mTownServiceDataAsset.generated.h"

class URogue10mTownServiceDataAsset;

UENUM(BlueprintType)
enum class ERogue10mDifficulty : uint8
{
	Easy,
	Normal,
	Hard
};

UENUM(BlueprintType)
enum class ERogue10mTownTrait : uint8
{
	Vitality = 0,
	Power = 1,
	CriticalChance,
	CriticalDamage,
	Combo,
	Defense,
	KillRecovery,
	LastStand,
	MoveSpeed,
	MaxStamina,
	DodgeEfficiency,
	DodgeCounter,
	GoldFind,
	StaminaRecovery,
	PotionMastery,
	BossHunter,
	Count UMETA(Hidden)
};

/** Frozen purchase values; transient combat state never belongs in this record. */
USTRUCT(BlueprintType)
struct FRogue10mTraitInvestment
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Traits")
	ERogue10mTownTrait Trait = ERogue10mTownTrait::Vitality;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Traits")
	int32 Rank = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Traits")
	int32 SpentPoints = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Traits")
	float Bonus = 0.0f;
};

USTRUCT(BlueprintType)
struct FRogue10mDifficultyModifiers
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Difficulty", meta=(ClampMin="0.1", ClampMax="10.0"))
	float HealthMultiplier = 1.0f;
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Difficulty", meta=(ClampMin="0.1", ClampMax="10.0"))
	float DamageMultiplier = 1.0f;
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Difficulty", meta=(ClampMin="0.1", ClampMax="10.0"))
	float ExperienceMultiplier = 1.0f;
};

/** Character profile progression; the current run and vitals are intentionally excluded. */
USTRUCT(BlueprintType)
struct FRogue10mTownProgressionSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	int32 VitalityLevel = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	int32 PowerLevel = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	ERogue10mDifficulty Difficulty = ERogue10mDifficulty::Normal;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	float TraitMaxHealthBonus = 0.0f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	float TraitAttackBonus = 0.0f;
	/** Frozen when selected so an authored NPC's tuning survives map/profile reload. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Town")
	FRogue10mDifficultyModifiers DifficultyModifiers;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Traits")
	TArray<FRogue10mTraitInvestment> TraitInvestments;
};

/** Native defaults make existing NPC children usable without editing any binary assets. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mTownServiceDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	URogue10mTownServiceDataAsset();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Items", meta=(ClampMin="1"))
	int32 PotionGoldCost = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Items", meta=(ClampMin="1", ClampMax="429496729"))
	int32 EnhancementGoldCost = 25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Items", meta=(ClampMin="1", ClampMax="5"))
	int32 MaximumEnhancementLevel = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Items", meta=(ClampMin="0.0", ClampMax="200000.0"))
	float EnhancementAttackPerLevel = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Items", meta=(ClampMin="0.0", ClampMax="200000.0"))
	float EnhancementDefensePerLevel = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Traits", meta=(ClampMin="1", ClampMax="5"))
	int32 MaximumTraitLevel = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Traits", meta=(ClampMin="0.0", ClampMax="200000.0"))
	float VitalityHealthPerLevel = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Traits", meta=(ClampMin="0.0", ClampMax="200000.0"))
	float PowerAttackPerLevel = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Next Run")
	FRogue10mDifficultyModifiers Easy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Next Run")
	FRogue10mDifficultyModifiers Normal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town|Next Run")
	FRogue10mDifficultyModifiers Hard;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town|Difficulty")
	FRogue10mDifficultyModifiers GetDifficultyModifiers(ERogue10mDifficulty Difficulty) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	bool IsTuningValid() const;
};
