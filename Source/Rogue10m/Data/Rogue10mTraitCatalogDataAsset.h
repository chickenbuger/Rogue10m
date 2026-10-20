#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTraitCatalogDataAsset.generated.h"

UENUM(BlueprintType)
enum class ERogue10mTraitCategory : uint8
{
	Combat,
	Survival,
	Mobility,
	Exploration
};

USTRUCT(BlueprintType)
struct FRogue10mTraitDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	ERogue10mTownTrait Trait = ERogue10mTownTrait::Vitality;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	ERogue10mTraitCategory Category = ERogue10mTraitCategory::Survival;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	FText EffectSummary;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="1", ClampMax="5"))
	int32 MaxRank = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="1", ClampMax="1000"))
	int32 PointCost = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="1", ClampMax="1000000"))
	int32 UnlockLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="0"))
	float ValuePerRank = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="0", ClampMax="60"))
	float DurationSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="0", ClampMax="300"))
	float CooldownSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(ClampMin="1", ClampMax="20"))
	int32 RequiredHits = 1;

	bool IsTuningValid() const;
};

/** Native defaults keep existing NPC assets usable; designers can author a replacement. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mTraitCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	URogue10mTraitCatalogDataAsset();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Traits")
	TArray<FRogue10mTraitDefinition> Definitions;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	FRogue10mTraitDefinition GetTraitDefinition(ERogue10mTownTrait Trait) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	bool IsTuningValid() const;
	static bool IsSupportedTrait(ERogue10mTownTrait Trait);
	static int32 GetMaximumSupportedRank(ERogue10mTownTrait Trait);
	static float GetMaximumBonusPerRank(ERogue10mTownTrait Trait);
};
