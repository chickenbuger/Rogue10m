#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mCharacterStats.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTraitCatalogDataAsset.h"
#include "Rogue10mTownProgressionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mTownProgressionChanged);

/** PlayerState-owned traits and the selected difficulty for the next run. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mTownProgressionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mTownProgressionComponent();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	FRogue10mTownProgressionSnapshot CaptureProgression() const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool RestoreProgression(const FRogue10mTownProgressionSnapshot& Snapshot);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	FRogue10mEquipmentStatModifiers GetStatModifiers() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	FRogue10mDifficultyModifiers GetDifficultyModifiers() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	ERogue10mDifficulty GetDifficulty() const { return Difficulty; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	int32 GetTraitLevel(ERogue10mTownTrait Trait) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	int32 GetAvailableTraitPoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	float GetTraitBonus(ERogue10mTownTrait Trait) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	int32 GetSpentTraitPoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	FRogue10mTraitDefinition GetTraitDefinition(ERogue10mTownTrait Trait) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	TArray<FRogue10mTraitDefinition> GetTraitDefinitions() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	FRogue10mTraitDefinition GetUpgradeDefinition(ERogue10mTownTrait Trait, const URogue10mTownServiceDataAsset* Data = nullptr) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits")
	bool CanChangeTraits() const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits")
	bool CanUpgradeTrait(ERogue10mTownTrait Trait, FText& OutReason, const URogue10mTownServiceDataAsset* Data = nullptr) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits")
	bool TryResetTraits();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traits")
	TObjectPtr<URogue10mTraitCatalogDataAsset> TraitCatalog;
	static bool ValidateSnapshot(const FRogue10mTownProgressionSnapshot& Snapshot, int32 EarnedPoints, int32 UnlockLevel = INDEX_NONE);
	/** A negative budget retains legacy standalone fixtures. Attached v4 profiles always set an independent budget. */
	void SetLobbyBudget(int32 EarnedPoints, int32 HighestLevel) { LobbyEarnedPoints = EarnedPoints; LobbyUnlockLevel = HighestLevel; }
	bool UsesLobbyBudget() const { return LobbyEarnedPoints >= 0; }
	int32 GetLobbyEarnedBudget() const { return LobbyEarnedPoints; }
	int32 GetLobbyUnlockLevel() const { return LobbyUnlockLevel; }
	int32 GetRunSharedTraitSpent() const { return UsesLobbyBudget() ? 0 : GetSpentTraitPoints(); }
	static FRogue10mTownProgressionSnapshot MigrateLegacySnapshot(const FRogue10mTownProgressionSnapshot& Snapshot);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool TrySetDifficulty(ERogue10mDifficulty NewDifficulty, const URogue10mTownServiceDataAsset* Data = nullptr);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool TryUpgradeTrait(ERogue10mTownTrait Trait, const URogue10mTownServiceDataAsset* Data = nullptr);
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Town")
	FRogue10mTownProgressionChanged OnTownProgressionChanged;
private:
	const URogue10mTraitCatalogDataAsset& GetCatalog() const;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Traits", meta=(AllowPrivateAccess="true"))
	TArray<FRogue10mTraitInvestment> TraitInvestments;
	bool CanChangeProgression() const;
	int32 LobbyEarnedPoints = INDEX_NONE;
	int32 LobbyUnlockLevel = 1;
	void NotifyChanged();
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Town", meta=(AllowPrivateAccess="true"))
	ERogue10mDifficulty Difficulty = ERogue10mDifficulty::Normal;
	UPROPERTY()
	FRogue10mDifficultyModifiers DifficultyModifiers;
};
