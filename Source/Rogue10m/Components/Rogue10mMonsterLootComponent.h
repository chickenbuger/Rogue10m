#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mMonsterLootComponent.generated.h"

class URogue10mLootTableDataAsset;

/** Run-local, authority-owned death loot. It does not serialize uncollected world actors. */
UCLASS(ClassGroup=(Rogue10m))
class ROGUE10M_API URogue10mMonsterLootComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mMonsterLootComponent();
	int32 SpawnDeathLoot(const URogue10mLootTableDataAsset* Table);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Loot")
	bool HasProcessedDeathLoot() const { return bDeathLootProcessed; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Loot")
	int32 GetCommittedDropCount() const { return CommittedDropCount; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot|Placement", meta=(ClampMin="0.0", ClampMax="500.0", Units="cm"))
	float ScatterRadius = 70.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot|Placement", meta=(ClampMin="100.0", ClampMax="100000.0", Units="cm"))
	float GroundTraceDistance = 3000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Loot|Placement", meta=(ClampMin="0.0", ClampMax="500.0", Units="cm"))
	float GroundClearance = 35.0f;

private:
	bool bDeathLootProcessed = false;
	int32 CommittedDropCount = 0;
};
