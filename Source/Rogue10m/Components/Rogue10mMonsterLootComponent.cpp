#include "Rogue10mMonsterLootComponent.h"

#include "Engine/World.h"
#include "Rogue10m.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mGameState.h"
#include "Rogue10mLootDeliverySubsystem.h"
#include "Rogue10mLootTableDataAsset.h"

URogue10mMonsterLootComponent::URogue10mMonsterLootComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 URogue10mMonsterLootComponent::SpawnDeathLoot(const URogue10mLootTableDataAsset* Table, ARogue10mCharacter* Recipient)
{
	ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(GetOwner());
	UWorld* World = GetWorld();
	const ARogue10mGameState* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	if (bDeathLootProcessed || !Monster || !World || World->bIsTearingDown || !Monster->HasAuthority() || !Monster->IsDead()
		|| World->GetNetMode() != NM_Standalone || Monster->IsCombatTestInstance() || (Run && Run->IsRunTerminal()))
	{ return 0; }
	// Commit before rolling or callbacks; failed authored data is never rerolled.
	bDeathLootProcessed = true;
	if (!Table) { return 0; }
	FRandomStream Random(FMath::Rand());
	TArray<FRogue10mItemStack> Stacks;
	FText InvalidReason;
	if (!Table->RollDrops(Random, Stacks, InvalidReason))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s rejected loot table %s: %s"), *GetNameSafe(Monster), *GetNameSafe(Table), *InvalidReason.ToString());
		return 0;
	}
	if (auto* Delivery = World->GetSubsystem<URogue10mLootDeliverySubsystem>())
	{
		Delivery->QueueDeathLoot(Stacks, *Monster, Recipient, Table->DroppedItemClass,
			ScatterRadius, GroundTraceDistance, GroundClearance);
	}
	return CommittedDropCount;
}
