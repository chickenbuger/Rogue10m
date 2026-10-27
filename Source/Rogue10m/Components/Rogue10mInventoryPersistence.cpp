// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mInventoryComponent.h"

#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mProgressionSaveGame.h"

namespace Rogue10mInventoryPersistence
{
static bool ValidEnhancement(int32 Level, float Attack, float Defense)
{
	return Level >= 0 && Level <= 5 && FMath::IsFinite(Attack) && FMath::IsFinite(Defense)
		&& Attack >= 0.0f && Defense >= 0.0f
		&& Attack <= 200000.0f * Level && Defense <= 200000.0f * Level;
}

static bool ResolveStack(FRogue10mItemStack& Stack, const TSoftObjectPtr<URogue10mItemDataAsset>& Asset)
{
	Stack.ItemData = Asset.IsNull() ? nullptr : Asset.LoadSynchronous();
	if (!Asset.IsNull() && !Stack.ItemData) { return false; }
	if (!ValidEnhancement(Stack.EnhancementLevel, Stack.EnhancementAttackBonus, Stack.EnhancementDefenseBonus))
	{
		return false;
	}
	if (!Stack.bOccupied) { return Stack.Quantity == 0 && Asset.IsNull(); }
	if (Stack.Quantity <= 0) { return false; }
	if (Stack.ItemData && (Stack.Quantity > FMath::Max(1, Stack.ItemData->MaxStackSize)
		|| Stack.Category != Stack.ItemData->Category || Stack.EquipSlotType != Stack.ItemData->EquipSlotType))
	{
		return false;
	}
	return true;
}
}

FRogue10mInventorySnapshot URogue10mInventoryComponent::CaptureInventory() const
{
	FRogue10mInventorySnapshot Snapshot;
	const auto CaptureEquipment = [](const TArray<FRogue10mInventorySlot>& Slots,
		TArray<FRogue10mSavedEquipmentSlot>& Result)
	{
		for (const auto& Slot : Slots)
		{
			FRogue10mSavedEquipmentSlot Saved;
			Saved.Values = Slot;
			Saved.ItemAsset = const_cast<URogue10mItemDataAsset*>(Slot.EquippedItem.ItemData.Get());
			Saved.Values.EquippedItem.ItemData = nullptr;
			Result.Add(MoveTemp(Saved));
		}
	};
	CaptureEquipment(LeftEquipmentSlots, Snapshot.LeftEquipment);
	CaptureEquipment(RightEquipmentSlots, Snapshot.RightEquipment);
	for (const FRogue10mItemStack& Stack : ItemSlots)
	{
		FRogue10mSavedItemStack Saved;
		Saved.Values = Stack;
		Saved.ItemAsset = const_cast<URogue10mItemDataAsset*>(Stack.ItemData.Get());
		Saved.Values.ItemData = nullptr;
		Snapshot.ItemSlots.Add(MoveTemp(Saved));
	}
	for (const FRogue10mInventoryContainer& Container : InventoryContainers)
	{
		FRogue10mSavedInventoryContainer Saved;
		Saved.ContainerId = Container.ContainerId;
		Saved.DisplayName = Container.DisplayName;
		Saved.GridSize = Container.GridSize;
		Saved.BagAsset = const_cast<URogue10mItemDataAsset*>(Container.SourceBagItem.Get());
		for (const FRogue10mInventoryGridEntry& Entry : Container.Entries)
		{
			FRogue10mSavedGridEntry SavedEntry;
			SavedEntry.Values = Entry;
			SavedEntry.ItemAsset = const_cast<URogue10mItemDataAsset*>(Entry.ItemData.Get());
			SavedEntry.Values.ItemData = nullptr;
			Saved.Entries.Add(MoveTemp(SavedEntry));
		}
		Snapshot.Containers.Add(MoveTemp(Saved));
	}
	Snapshot.ConsumableQuickSlots = ConsumableQuickSlotItemIndices;
	Snapshot.Gold = Gold;
	Snapshot.Crystals = Crystals;
	return Snapshot;
}

bool URogue10mInventoryComponent::RestoreInventory(const FRogue10mInventorySnapshot& Snapshot)
{
	using namespace Rogue10mInventoryPersistence;
	if (Snapshot.Gold < 0 || Snapshot.Crystals < 0 || Snapshot.Containers.IsEmpty()
		|| Snapshot.Containers.Num() > 64 || Snapshot.ItemSlots.Num() > 80
		|| Snapshot.ConsumableQuickSlots.Num() != ConsumableQuickSlotItemIndices.Num())
	{
		return false;
	}
	TArray<FRogue10mInventorySlot> RestoredLeft;
	TArray<FRogue10mInventorySlot> RestoredRight;
	TArray<FRogue10mItemStack> RestoredItems;
	TArray<FRogue10mInventoryContainer> RestoredContainers;
	TSet<FGuid> InstanceIds;
	const auto ValidateId = [&InstanceIds](const FRogue10mItemStack& Stack)
	{
		if (!Stack.bOccupied || !Stack.InstanceId.IsValid()) { return true; }
		if (InstanceIds.Contains(Stack.InstanceId)) { return false; }
		InstanceIds.Add(Stack.InstanceId);
		return true;
	};
	const auto RestoreEquipment = [&ValidateId](const TArray<FRogue10mSavedEquipmentSlot>& Saved,
		const TArray<FRogue10mInventorySlot>& Layout, TArray<FRogue10mInventorySlot>& Result)
	{
		if (Saved.Num() != Layout.Num()) { return false; }
		for (int32 Index = 0; Index < Saved.Num(); ++Index)
		{
			FRogue10mInventorySlot Slot = Saved[Index].Values;
			if (Slot.SlotType != Layout[Index].SlotType || !ResolveStack(Slot.EquippedItem, Saved[Index].ItemAsset)
				|| Slot.bHasEquippedItem != Slot.EquippedItem.bOccupied || !ValidateId(Slot.EquippedItem))
			{
				return false;
			}
			if (Slot.bHasEquippedItem && (Slot.EquippedItem.Category != ERogue10mItemCategory::Equipment
				|| Slot.EquippedItem.EquipSlotType != Slot.SlotType)) { return false; }
			Result.Add(MoveTemp(Slot));
		}
		return true;
	};
	if (!RestoreEquipment(Snapshot.LeftEquipment, LeftEquipmentSlots, RestoredLeft)
		|| !RestoreEquipment(Snapshot.RightEquipment, RightEquipmentSlots, RestoredRight)) { return false; }
	for (const FRogue10mSavedItemStack& Saved : Snapshot.ItemSlots)
	{
		FRogue10mItemStack Stack = Saved.Values;
		if (!ResolveStack(Stack, Saved.ItemAsset) || !ValidateId(Stack)) { return false; }
		RestoredItems.Add(MoveTemp(Stack));
	}
	TSet<FName> ContainerIds;
	for (const FRogue10mSavedInventoryContainer& Saved : Snapshot.Containers)
	{
		if (Saved.ContainerId.IsNone() || ContainerIds.Contains(Saved.ContainerId)
			|| Saved.GridSize.X < 1 || Saved.GridSize.X > 20 || Saved.GridSize.Y < 1 || Saved.GridSize.Y > 20)
		{
			return false;
		}
		ContainerIds.Add(Saved.ContainerId);
		FRogue10mInventoryContainer Container;
		Container.ContainerId = Saved.ContainerId;
		Container.DisplayName = Saved.DisplayName;
		Container.GridSize = Saved.GridSize;
		Container.SourceBagItem = Saved.BagAsset.IsNull() ? nullptr : Saved.BagAsset.LoadSynchronous();
		if (!Saved.BagAsset.IsNull() && (!Container.SourceBagItem || !Container.SourceBagItem->bProvidesInventory))
		{
			return false;
		}
		TSet<int32> OccupiedCells;
		for (const FRogue10mSavedGridEntry& SavedEntry : Saved.Entries)
		{
			FRogue10mInventoryGridEntry Entry = SavedEntry.Values;
			Entry.ItemData = SavedEntry.ItemAsset.LoadSynchronous();
			if (!Entry.ItemData || !Entry.InstanceId.IsValid() || InstanceIds.Contains(Entry.InstanceId)
				|| Entry.Quantity < 1 || Entry.Quantity > FMath::Max(1, Entry.ItemData->MaxStackSize)
				|| !ValidEnhancement(Entry.EnhancementLevel, Entry.EnhancementAttackBonus, Entry.EnhancementDefenseBonus))
			{
				return false;
			}
			const FIntPoint Size = Entry.ItemData->GetClampedInventorySize();
			if (Entry.Position.X < 0 || Entry.Position.Y < 0
				|| Entry.Position.X > Container.GridSize.X - Size.X
				|| Entry.Position.Y > Container.GridSize.Y - Size.Y) { return false; }
			for (int32 Y = Entry.Position.Y; Y < Entry.Position.Y + Size.Y; ++Y)
			{
				for (int32 X = Entry.Position.X; X < Entry.Position.X + Size.X; ++X)
				{
					const int32 Cell = Y * Container.GridSize.X + X;
					if (OccupiedCells.Contains(Cell)) { return false; }
					OccupiedCells.Add(Cell);
				}
			}
			InstanceIds.Add(Entry.InstanceId);
			Container.Entries.Add(MoveTemp(Entry));
		}
		RestoredContainers.Add(MoveTemp(Container));
	}
	for (int32 Index : Snapshot.ConsumableQuickSlots)
	{
		if (Index != INDEX_NONE && (!RestoredItems.IsValidIndex(Index) || !RestoredItems[Index].bOccupied
			|| RestoredItems[Index].Category != ERogue10mItemCategory::Consumable)) { return false; }
	}
	// No state or events change until every asset, identity and grid cell has passed validation.
	LeftEquipmentSlots = MoveTemp(RestoredLeft);
	RightEquipmentSlots = MoveTemp(RestoredRight);
	ItemSlots = MoveTemp(RestoredItems);
	InventoryContainers = MoveTemp(RestoredContainers);
	ConsumableQuickSlotItemIndices = Snapshot.ConsumableQuickSlots;
	ConsumableQuickSlotCooldownEndTimes.Init(0.0f, ConsumableQuickSlotItemIndices.Num());
	Gold = Snapshot.Gold;
	Crystals = Snapshot.Crystals;
	if (const FRogue10mInventorySlot* MainWeapon = FindEquipmentSlot(ERogue10mInventorySlotType::MainWeapon))
	{
		if (MainWeapon->bHasEquippedItem)
		{
			if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner()))
			{
				Character->RestoreEquippedWeaponType(MainWeapon->EquippedItem.WeaponType);
			}
		}
		else if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner()))
		{
			Character->GetCombatComponent()->HandleEquippedWeaponChanged();
		}
	}
	NotifyGridChanged();
	NotifyEquipmentChanged();
	return true;
}
