#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mLootTableDataAsset.generated.h"

class ARogue10mDroppedItem;
class URogue10mItemDataAsset;

USTRUCT(BlueprintType)
struct FRogue10mLootEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Loot")
	TObjectPtr<URogue10mItemDataAsset> ItemData;
	/** Independent probability for this row. Zero never drops; one always drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Loot", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Loot", meta=(ClampMin="1"))
	int32 MinQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Loot", meta=(ClampMin="1"))
	int32 MaxQuantity = 1;
};

/** A row rolls independently and yields at most one valid existing inventory stack. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mLootTableDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	static constexpr int32 MaximumEntries = 64;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot")
	FName LootTableId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot")
	TArray<FRogue10mLootEntry> Entries;
	/** Optional project-owned presentation; null uses the existing native dropped item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot")
	TSubclassOf<ARogue10mDroppedItem> DroppedItemClass;
	/** Authoring note only. Example probabilities are not a final economy decision. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot|Authoring", meta=(MultiLine="true"))
	FText BalanceNotes;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Loot")
	bool ValidateLootTable(FText& OutReason) const;
	bool RollDrops(FRandomStream& Random, TArray<FRogue10mItemStack>& OutStacks, FText& OutReason) const;
};
