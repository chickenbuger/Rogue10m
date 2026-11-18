#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mLootDeliverySubsystem.generated.h"

class ARogue10mBasicMonster;
class ARogue10mCharacter;
class ARogue10mDroppedItem;

/** Custody of uncollected overflow is world-local, independent of corpse lifetime. */
USTRUCT()
struct FRogue10mPendingMonsterLoot
{
	GENERATED_BODY()
	UPROPERTY() FRogue10mItemStack Stack;
	UPROPERTY() TWeakObjectPtr<ARogue10mBasicMonster> Source;
	UPROPERTY() TWeakObjectPtr<ARogue10mCharacter> Recipient;
	UPROPERTY() TSubclassOf<ARogue10mDroppedItem> DropClass;
	UPROPERTY() FVector TraceOrigin = FVector::ZeroVector;
	UPROPERTY() float TraceDistance = 3000.0f;
	UPROPERTY() float Clearance = 35.0f;
	UPROPERTY() bool bPlacementValid = true;
	UPROPERTY() bool bInventoryAttempted = false;
	UPROPERTY() bool bRejectedInput = false;
	UPROPERTY() bool bWarned = false;
};

UCLASS()
class ROGUE10M_API URogue10mLootDeliverySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void QueueDeathLoot(const TArray<FRogue10mItemStack>& Stacks, ARogue10mBasicMonster& Source,
		ARogue10mCharacter* Recipient, TSubclassOf<ARogue10mDroppedItem> DropClass,
		float ScatterRadius, float TraceDistance, float Clearance);
	/** Also used by controlled automation after repairing a missing floor. */
	void RetryPendingLoot();
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Loot")
	int32 GetPendingStackCount() const { return Pending.Num(); }
	const TArray<FRogue10mPendingMonsterLoot>& GetPendingLoot() const { return Pending; }

private:
	int32 FindReceipt(FGuid Id) const;
	bool TrySpawnOverflow(FGuid Id);
	UPROPERTY() TArray<FRogue10mPendingMonsterLoot> Pending;
	TSet<FGuid> Receipts;
	FTimerHandle RetryTimer;
	bool bProcessing = false;
};
