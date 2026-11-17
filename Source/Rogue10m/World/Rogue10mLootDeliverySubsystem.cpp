#include "Rogue10mLootDeliverySubsystem.h"

#include "Engine/World.h"
#include "Rogue10m.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mMonsterLootComponent.h"
#include "TimerManager.h"

int32 URogue10mLootDeliverySubsystem::FindReceipt(FGuid Id) const
{
	return Pending.IndexOfByPredicate([Id](const FRogue10mPendingMonsterLoot& Entry) { return Entry.Stack.InstanceId == Id; });
}

void URogue10mLootDeliverySubsystem::QueueDeathLoot(const TArray<FRogue10mItemStack>& Stacks,
	ARogue10mBasicMonster& Source, ARogue10mCharacter* Recipient, TSubclassOf<ARogue10mDroppedItem> DropClass,
	float ScatterRadius, float TraceDistance, float Clearance)
{
	UWorld* World = GetWorld();
	const ARogue10mGameState* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	if (!World || World->bIsTearingDown || World != Source.GetWorld() || World->GetNetMode() != NM_Standalone || !Source.HasAuthority()
		|| !Source.IsDead() || Source.IsCombatTestInstance() || (Run && Run->IsRunTerminal())) { return; }
	const bool bValidPlacement = FMath::IsFinite(ScatterRadius) && ScatterRadius >= 0.0f && ScatterRadius <= 500.0f
		&& FMath::IsFinite(TraceDistance) && TraceDistance >= 100.0f && TraceDistance <= 100000.0f
		&& FMath::IsFinite(Clearance) && Clearance >= 0.0f && Clearance <= 500.0f;
	// Secure every rolled stack before inventory delegates or spawn callbacks run.
	for (const FRogue10mItemStack& Stack : Stacks)
	{
		if (!Stack.InstanceId.IsValid() || Receipts.Contains(Stack.InstanceId) || !Stack.ItemData || Stack.Quantity <= 0) { continue; }
		Receipts.Add(Stack.InstanceId);
		FRogue10mPendingMonsterLoot& Entry = Pending.AddDefaulted_GetRef();
		Entry.Stack = Stack;
		Entry.Source = &Source;
		Entry.Recipient = Recipient;
		Entry.DropClass = DropClass;
		Entry.TraceDistance = TraceDistance;
		Entry.Clearance = Clearance;
		Entry.bPlacementValid = bValidPlacement;
		Entry.TraceOrigin = Source.GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
		if (bValidPlacement)
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Radius = FMath::FRandRange(0.0f, ScatterRadius);
			Entry.TraceOrigin += FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
		}
	}
	RetryPendingLoot();
}

void URogue10mLootDeliverySubsystem::RetryPendingLoot()
{
	UWorld* World = GetWorld();
	if (bProcessing || !World || World->bIsTearingDown || World->GetNetMode() != NM_Standalone) { return; }
	const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
	if (Run && Run->IsRunTerminal()) { World->GetTimerManager().ClearTimer(RetryTimer); return; }
	TGuardValue<bool> ProcessingGuard(bProcessing, true);
	TArray<FGuid> Batch;
	for (const auto& Entry : Pending) { Batch.Add(Entry.Stack.InstanceId); }
	for (FGuid Id : Batch)
	{
		const auto* CurrentRun = World->GetGameState<ARogue10mGameState>();
		if (World->bIsTearingDown || (CurrentRun && CurrentRun->IsRunTerminal())) { break; }
		int32 Index = FindReceipt(Id);
		if (Index == INDEX_NONE) { continue; }
		if (Pending[Index].bRejectedInput) { continue; }
		if (!Pending[Index].bInventoryAttempted)
		{
			// Copy before callbacks: a nested death can append and reallocate Pending.
			const FRogue10mPendingMonsterLoot Entry = Pending[Index];
			ARogue10mCharacter* Character = Entry.Recipient.Get();
			URogue10mInventoryComponent* Inventory = Character && Character->GetWorld() == World
				&& Character->HasAuthority() && !Character->IsDead() && !Character->IsActorBeingDestroyed()
				? Character->GetInventoryComponent() : nullptr;
			Pending[Index].bInventoryAttempted = true;
			if (Inventory)
			{
				FRogue10mItemStack Remainder;
				int32 Added = 0;
				const bool bValid = Inventory->TryReceiveMonsterLoot(Entry.Stack, Remainder, Added);
				Index = FindReceipt(Id);
				if (Index == INDEX_NONE) { continue; }
				if (!bValid)
				{
					// Rejected data/replay is not a capacity overflow. Keep custody;
					// do not manufacture a second actor carrying an owned GUID.
					Pending[Index].bRejectedInput = true;
					continue;
				}
				Pending[Index].Stack = Remainder;
				if (ARogue10mBasicMonster* Source = Entry.Source.Get())
				{
					if (auto* Loot = Source->GetMonsterLootComponent()) { Loot->AutomaticallyReceivedQuantity += Added; }
				}
				if (Remainder.Quantity == 0) { Pending.RemoveAt(Index); continue; }
			}
		}
		const auto* AfterCallbacks = World->GetGameState<ARogue10mGameState>();
		if (World->bIsTearingDown || (AfterCallbacks && AfterCallbacks->IsRunTerminal())) { break; }
		TrySpawnOverflow(Id);
	}
	const auto* FinalRun = World->GetGameState<ARogue10mGameState>();
	if (World->bIsTearingDown || (FinalRun && FinalRun->IsRunTerminal())
		|| !Pending.ContainsByPredicate([](const FRogue10mPendingMonsterLoot& Entry) { return !Entry.bRejectedInput; }))
	{ World->GetTimerManager().ClearTimer(RetryTimer); }
	else if (!World->GetTimerManager().IsTimerActive(RetryTimer))
	{ World->GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::RetryPendingLoot, 1.0f, true); }
}

bool URogue10mLootDeliverySubsystem::TrySpawnOverflow(FGuid Id)
{
	const int32 Index = FindReceipt(Id);
	UWorld* World = GetWorld();
	if (Index == INDEX_NONE || !World) { return false; }
	const FRogue10mPendingMonsterLoot Entry = Pending[Index];
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mLootOverflowGround), false, Entry.Source.Get());
	if (!Entry.bPlacementValid || !World->LineTraceSingleByObjectType(Ground, Entry.TraceOrigin,
		Entry.TraceOrigin - FVector(0.0f, 0.0f, Entry.TraceDistance), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
		|| !Ground.bBlockingHit || Ground.bStartPenetrating || Ground.ImpactNormal.Z < 0.65f)
	{
		if (!Pending[Index].bWarned)
		{
			UE_LOG(LogRogue10m, Log, TEXT("Loot overflow %s retained until supported placement is available."), *Id.ToString());
			Pending[Index].bWarned = true;
		}
		return false;
	}
	const FTransform Placement(FRotator::ZeroRotator, Ground.ImpactPoint + FVector(0.0f, 0.0f, Entry.Clearance));
	FActorSpawnParameters Parameters;
	Parameters.Owner = Entry.Source.Get();
	Parameters.OverrideLevel = World->PersistentLevel;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.bDeferConstruction = true;
	UClass* Class = Entry.DropClass ? Entry.DropClass.Get() : ARogue10mDroppedItem::StaticClass();
	if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) { Class = ARogue10mDroppedItem::StaticClass(); }
	ARogue10mDroppedItem* Pickup = World->SpawnActor<ARogue10mDroppedItem>(Class, Placement, Parameters);
	if (!IsValid(Pickup) && Class != ARogue10mDroppedItem::StaticClass())
	{ Pickup = World->SpawnActor<ARogue10mDroppedItem>(ARogue10mDroppedItem::StaticClass(), Placement, Parameters); }
	if (!IsValid(Pickup) || Pickup->IsActorBeingDestroyed()) { return false; }
	const auto* AfterSpawn = World->GetGameState<ARogue10mGameState>();
	if (World->bIsTearingDown || (AfterSpawn && AfterSpawn->IsRunTerminal()))
	{ Pickup->Destroy(); return false; }
	Pickup->Tags.AddUnique(TEXT("Rogue10m.MonsterLoot"));
	Pickup->InitializeDroppedItem(Entry.Stack);
	const int32 Current = FindReceipt(Id);
	if (Current != INDEX_NONE) { Pending.RemoveAt(Current); }
	if (ARogue10mBasicMonster* Source = Entry.Source.Get())
	{ if (auto* Loot = Source->GetMonsterLootComponent()) { ++Loot->CommittedDropCount; } }
	// Ownership has moved to the real actor before BeginPlay can auto-pick it up.
	Pickup->FinishSpawning(Placement);
	return true;
}

void URogue10mLootDeliverySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(RetryTimer); }
	Pending.Reset();
	Receipts.Reset();
	Super::Deinitialize();
}
