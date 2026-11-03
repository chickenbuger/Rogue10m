#include "Rogue10mMonsterLootComponent.h"

#include "Engine/World.h"
#include "Rogue10m.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mLootTableDataAsset.h"

URogue10mMonsterLootComponent::URogue10mMonsterLootComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 URogue10mMonsterLootComponent::SpawnDeathLoot(const URogue10mLootTableDataAsset* Table)
{
	ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(GetOwner());
	UWorld* World = GetWorld();
	const ARogue10mGameState* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	if (bDeathLootProcessed || !Monster || !World || !Monster->HasAuthority() || !Monster->IsDead()
		|| World->GetNetMode() != NM_Standalone || Monster->IsCombatTestInstance() || (Run && Run->IsRunTerminal()))
	{ return 0; }
	// Commit before rolling, spawning or BeginPlay callbacks. A failed authored table is not rerolled.
	bDeathLootProcessed = true;
	if (!Table) { return 0; }
	if (!FMath::IsFinite(ScatterRadius) || ScatterRadius < 0.0f || ScatterRadius > 500.0f
		|| !FMath::IsFinite(GroundTraceDistance) || GroundTraceDistance < 100.0f || GroundTraceDistance > 100000.0f
		|| !FMath::IsFinite(GroundClearance) || GroundClearance < 0.0f || GroundClearance > 500.0f)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s rejected invalid death loot placement settings."), *GetNameSafe(Monster));
		return 0;
	}
	FRandomStream Random(FMath::Rand());
	TArray<FRogue10mItemStack> Stacks;
	FText InvalidReason;
	if (!Table->RollDrops(Random, Stacks, InvalidReason))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s rejected loot table %s: %s"), *GetNameSafe(Monster), *GetNameSafe(Table), *InvalidReason.ToString());
		return 0;
	}
	for (const FRogue10mItemStack& Stack : Stacks)
	{
		const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
		const float Radius = Random.FRandRange(0.0f, ScatterRadius);
		const FVector Origin = Monster->GetActorLocation() + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 100.0f);
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mDeathLootGround), false, Monster);
		if (!World->LineTraceSingleByObjectType(Ground, Origin, Origin - FVector(0.0f, 0.0f, GroundTraceDistance),
			FCollisionObjectQueryParams(ECC_WorldStatic), Query) || !Ground.bBlockingHit || Ground.bStartPenetrating
			|| Ground.ImpactNormal.Z < 0.65f)
		{
			UE_LOG(LogRogue10m, Warning, TEXT("%s skipped one death loot stack because no supported ground was found."), *GetNameSafe(Monster));
			continue;
		}
		const FTransform Placement(FRotator::ZeroRotator, Ground.ImpactPoint + FVector(0.0f, 0.0f, GroundClearance));
		FActorSpawnParameters Parameters;
		Parameters.Owner = Monster;
		Parameters.OverrideLevel = World->PersistentLevel;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.bDeferConstruction = true;
		TSubclassOf<ARogue10mDroppedItem> PickupClass = Table->DroppedItemClass;
		if (!PickupClass) { PickupClass = ARogue10mDroppedItem::StaticClass(); }
		ARogue10mDroppedItem* Pickup = World->SpawnActor<ARogue10mDroppedItem>(PickupClass, Placement, Parameters);
		if (!Pickup)
		{
			UE_LOG(LogRogue10m, Warning, TEXT("%s failed to spawn one death loot pickup."), *GetNameSafe(Monster));
			continue;
		}
		Pickup->Tags.AddUnique(TEXT("Rogue10m.MonsterLoot"));
		Pickup->InitializeDroppedItem(Stack);
		++CommittedDropCount;
		Pickup->FinishSpawning(Placement);
	}
	return CommittedDropCount;
}
