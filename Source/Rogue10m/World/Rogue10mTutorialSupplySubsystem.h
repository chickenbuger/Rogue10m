// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Rogue10mTutorialSupplySubsystem.generated.h"

class URogue10mInventoryComponent;
class URogue10mItemDataAsset;

USTRUCT()
struct FRogue10mTutorialSupplyReceipt
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<const URogue10mItemDataAsset> ItemData;
	UPROPERTY()
	FGuid InstanceId;
	UPROPERTY()
	int32 RemainingQuantity = 0;
	UPROPERTY()
	bool bPendingRestore = false;
};

/** 프로필 수령 기록을 사용하며 프로필 없는 세션에서만 보급품을 따로 운반합니다. */
UCLASS()
class ROGUE10M_API URogue10mTutorialSupplySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	bool HasReceived(FName RewardId) const;
	void RecordGrant(FName RewardId, const URogue10mItemDataAsset* ItemData, int32 Quantity, FGuid InstanceId);
	void CaptureSuppliesForTravel(const URogue10mInventoryComponent& Inventory);
	void AttachInventory(URogue10mInventoryComponent& Inventory);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	int32 GetPendingSupplyQuantity() const;

private:
	bool HasPersistentProfile() const;
	void RestorePendingSupplies();
	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(Transient)
	TMap<FName, FRogue10mTutorialSupplyReceipt> Receipts;
	TWeakObjectPtr<URogue10mInventoryComponent> CurrentInventory;
	bool bRestoring = false;
};
