// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialSupplySubsystem.h"

#include "Rogue10m.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Engine/GameInstance.h"

bool URogue10mTutorialSupplySubsystem::HasPersistentProfile() const
{
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mCharacterProfileSubsystem* Profiles = Instance
		? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	return Profiles && Profiles->GetSelectedProfile() && Profiles->GetSelectedProfile()->IsValid();
}

void URogue10mTutorialSupplySubsystem::Deinitialize()
{
	if (CurrentInventory.IsValid())
	{
		CurrentInventory->OnInventoryGridChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	CurrentInventory.Reset();
	Receipts.Empty();
	Super::Deinitialize();
}

bool URogue10mTutorialSupplySubsystem::HasReceived(FName RewardId) const
{
	if (HasPersistentProfile())
	{
		return GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->HasClaimedReward(RewardId);
	}
	return Receipts.Contains(RewardId);
}

void URogue10mTutorialSupplySubsystem::RecordGrant(FName RewardId,
	const URogue10mItemDataAsset* ItemData, int32 Quantity, FGuid InstanceId)
{
	if (RewardId.IsNone() || !ItemData || Quantity <= 0 || !InstanceId.IsValid() || HasReceived(RewardId)) { return; }
	if (HasPersistentProfile())
	{
		GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->RecordClaimedReward(RewardId);
		return;
	}
	FRogue10mTutorialSupplyReceipt& Receipt = Receipts.Add(RewardId);
	Receipt.ItemData = ItemData;
	Receipt.InstanceId = InstanceId;
	Receipt.RemainingQuantity = Quantity;
}

void URogue10mTutorialSupplySubsystem::CaptureSuppliesForTravel(const URogue10mInventoryComponent& Inventory)
{
	if (HasPersistentProfile()) { return; }
	for (TPair<FName, FRogue10mTutorialSupplyReceipt>& Pair : Receipts)
	{
		FRogue10mTutorialSupplyReceipt& Receipt = Pair.Value;
		// A previous restore blocked by a full bag remains in transit, not consumed.
		if (Receipt.bPendingRestore) { continue; }
		Receipt.RemainingQuantity = 0;
		for (const FRogue10mInventoryContainer& Container : Inventory.GetInventoryContainers())
		{
			for (const FRogue10mInventoryGridEntry& Entry : Container.Entries)
			{
				if (Entry.InstanceId == Receipt.InstanceId && Entry.ItemData == Receipt.ItemData)
				{
					Receipt.RemainingQuantity = FMath::Max(0, Entry.Quantity);
				}
			}
		}
		Receipt.bPendingRestore = Receipt.RemainingQuantity > 0;
		UE_LOG(LogRogue10m, Log, TEXT("Tutorial supply travel snapshot reward=%s quantity=%d instance=%s"),
			*Pair.Key.ToString(), Receipt.RemainingQuantity, *Receipt.InstanceId.ToString());
	}
	if (CurrentInventory.IsValid())
	{
		CurrentInventory->OnInventoryGridChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	CurrentInventory.Reset();
}

void URogue10mTutorialSupplySubsystem::AttachInventory(URogue10mInventoryComponent& Inventory)
{
	if (CurrentInventory.IsValid())
	{
		CurrentInventory->OnInventoryGridChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	CurrentInventory = &Inventory;
	if (HasPersistentProfile()) { return; }
	Inventory.OnInventoryGridChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	RestorePendingSupplies();
}

void URogue10mTutorialSupplySubsystem::HandleInventoryChanged()
{
	RestorePendingSupplies();
}

void URogue10mTutorialSupplySubsystem::RestorePendingSupplies()
{
	if (HasPersistentProfile()) { return; }
	if (bRestoring || !CurrentInventory.IsValid()) { return; }
	TGuardValue<bool> RestoreGuard(bRestoring, true);
	for (TPair<FName, FRogue10mTutorialSupplyReceipt>& Pair : Receipts)
	{
		FRogue10mTutorialSupplyReceipt& Receipt = Pair.Value;
		if (!Receipt.bPendingRestore || !Receipt.ItemData || Receipt.RemainingQuantity <= 0) { continue; }
		int32 ContainerIndex = INDEX_NONE;
		FGuid RestoredInstanceId;
		if (CurrentInventory->TryAddGridItem(Receipt.ItemData, Receipt.RemainingQuantity, ContainerIndex, RestoredInstanceId))
		{
			// The standard inventory allocator creates a fresh instance. Track that
			// exact instance for the next portal, so consumed items are never recreated.
			Receipt.InstanceId = RestoredInstanceId;
			Receipt.bPendingRestore = false;
			UE_LOG(LogRogue10m, Log, TEXT("Tutorial supply travel restore reward=%s quantity=%d instance=%s"),
				*Pair.Key.ToString(), Receipt.RemainingQuantity, *Receipt.InstanceId.ToString());
		}
	}
}

int32 URogue10mTutorialSupplySubsystem::GetPendingSupplyQuantity() const
{
	int32 Quantity = 0;
	for (const TPair<FName, FRogue10mTutorialSupplyReceipt>& Pair : Receipts)
	{
		if (Pair.Value.bPendingRestore) { Quantity += Pair.Value.RemainingQuantity; }
	}
	return Quantity;
}
