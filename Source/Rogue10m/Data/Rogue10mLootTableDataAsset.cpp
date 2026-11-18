#include "Rogue10mLootTableDataAsset.h"

#include "Rogue10mItemDataAsset.h"

FPrimaryAssetId URogue10mLootTableDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Rogue10mLootTable"), LootTableId.IsNone() ? GetFName() : LootTableId);
}

bool URogue10mLootTableDataAsset::ValidateLootTable(FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	if (Entries.Num() > MaximumEntries)
	{
		OutReason = NSLOCTEXT("Rogue10mLoot", "TooManyEntries", "전리품 표의 행 수가 기술적 상한을 넘습니다.");
		return false;
	}
	for (const FRogue10mLootEntry& Entry : Entries)
	{
		if (!Entry.ItemData || !FMath::IsFinite(Entry.DropChance) || Entry.DropChance < 0.0f || Entry.DropChance > 1.0f
			|| Entry.MinQuantity < 1 || Entry.MaxQuantity < Entry.MinQuantity || Entry.ItemData->MaxStackSize < 1
			|| Entry.MaxQuantity > Entry.ItemData->MaxStackSize)
		{
			OutReason = NSLOCTEXT("Rogue10mLoot", "InvalidEntry", "전리품의 아이템·확률·수량 또는 스택 상한이 올바르지 않습니다.");
			return false;
		}
	}
	return true;
}

bool URogue10mLootTableDataAsset::RollDrops(FRandomStream& Random,
	TArray<FRogue10mItemStack>& OutStacks, FText& OutReason) const
{
	OutStacks.Reset();
	if (!ValidateLootTable(OutReason)) { return false; }
	for (const FRogue10mLootEntry& Entry : Entries)
	{
		if (Entry.DropChance <= 0.0f || (Entry.DropChance < 1.0f && Random.FRand() >= Entry.DropChance)) { continue; }
		const URogue10mItemDataAsset& Item = *Entry.ItemData;
		FRogue10mItemStack Stack;
		Stack.InstanceId = FGuid::NewGuid();
		Stack.ItemData = &Item;
		Stack.Category = Item.Category;
		Stack.EquipSlotType = Item.EquipSlotType;
		Stack.WeaponType = Item.WeaponType;
		Stack.DisplayName = Item.DisplayName;
		Stack.Description = Item.Description;
		Stack.Quantity = Random.RandRange(Entry.MinQuantity, Entry.MaxQuantity);
		Stack.ItemColor = Item.InventoryTint;
		Stack.RestoreHealth = FMath::Max(0.0f, Item.RestoreHealth);
		Stack.bOccupied = true;
		OutStacks.Add(MoveTemp(Stack));
	}
	return true;
}
