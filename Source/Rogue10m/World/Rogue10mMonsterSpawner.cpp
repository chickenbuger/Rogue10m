// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mMonsterSpawner.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Rogue10m.h"
#include "Rogue10mBasicMonster.h"
#include "TimerManager.h"

ARogue10mMonsterSpawner::ARogue10mMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("Spawn Area"));
	SpawnArea->SetupAttachment(SceneRoot);
	SpawnArea->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetGenerateOverlapEvents(false);
}

void ARogue10mMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (!MonsterClass)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s has no Monster Class and cannot spawn monsters."), *GetNameSafe(this));
		return;
	}

	if (MaxAliveMonsters <= 0)
	{
		UE_LOG(LogRogue10m, Verbose, TEXT("%s is disabled because Max Alive Monsters is 0."), *GetNameSafe(this));
		return;
	}

	if (bFillToMaximumOnBeginPlay)
	{
		RefillToMaximum();
	}

	if (bMaintainPopulation)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&ARogue10mMonsterSpawner::HandleSpawnTimer,
				FMath::Max(0.1f, SpawnIntervalSeconds),
				true);
		}
	}
}

void ARogue10mMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	for (const TWeakObjectPtr<ARogue10mBasicMonster>& MonsterReference : SpawnedMonsters)
	{
		if (ARogue10mBasicMonster* Monster = MonsterReference.Get())
		{
			Monster->OnDestroyed.RemoveDynamic(this, &ARogue10mMonsterSpawner::HandleSpawnedMonsterDestroyed);
		}
	}
	SpawnedMonsters.Reset();

	Super::EndPlay(EndPlayReason);
}

ARogue10mBasicMonster* ARogue10mMonsterSpawner::SpawnOneMonster()
{
	if (!HasAuthority() || !MonsterClass || MaxAliveMonsters <= 0)
	{
		return nullptr;
	}

	CleanupTrackedMonsters();
	if (SpawnedMonsters.Num() >= MaxAliveMonsters)
	{
		return nullptr;
	}

	FTransform SpawnTransform;
	if (!FindSpawnTransform(SpawnTransform))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s could not find ground inside its spawn area."), *GetNameSafe(this));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	// Keep spawned characters in the spawner's streaming level when its cell unloads.
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.SpawnCollisionHandlingOverride = MonsterSpawnCollisionHandlingMethod;

	ARogue10mBasicMonster* SpawnedMonster =
		World->SpawnActor<ARogue10mBasicMonster>(MonsterClass, SpawnTransform, SpawnParameters);
	if (!SpawnedMonster)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s failed to spawn %s."), *GetNameSafe(this), *GetNameSafe(MonsterClass));
		return nullptr;
	}

	SpawnedMonster->OnDestroyed.AddDynamic(this, &ARogue10mMonsterSpawner::HandleSpawnedMonsterDestroyed);
	SpawnedMonsters.Add(SpawnedMonster);
	UE_LOG(
		LogRogue10m,
		Log,
		TEXT("%s spawned %s (%d/%d alive)."),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedMonster),
		SpawnedMonsters.Num(),
		MaxAliveMonsters);
	return SpawnedMonster;
}

void ARogue10mMonsterSpawner::RefillToMaximum()
{
	if (!HasAuthority())
	{
		return;
	}

	CleanupTrackedMonsters();
	while (SpawnedMonsters.Num() < MaxAliveMonsters)
	{
		if (!SpawnOneMonster())
		{
			break;
		}
	}
}

int32 ARogue10mMonsterSpawner::GetAliveMonsterCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<ARogue10mBasicMonster>& MonsterReference : SpawnedMonsters)
	{
		const ARogue10mBasicMonster* Monster = MonsterReference.Get();
		if (IsValid(Monster) && !Monster->IsDead())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

void ARogue10mMonsterSpawner::HandleSpawnTimer()
{
	CleanupTrackedMonsters();
	if (SpawnedMonsters.Num() < MaxAliveMonsters)
	{
		SpawnOneMonster();
	}
}

void ARogue10mMonsterSpawner::CleanupTrackedMonsters()
{
	for (int32 Index = SpawnedMonsters.Num() - 1; Index >= 0; --Index)
	{
		ARogue10mBasicMonster* Monster = SpawnedMonsters[Index].Get();
		if (!IsValid(Monster))
		{
			SpawnedMonsters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		if (Monster->IsDead())
		{
			Monster->OnDestroyed.RemoveDynamic(this, &ARogue10mMonsterSpawner::HandleSpawnedMonsterDestroyed);
			SpawnedMonsters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

bool ARogue10mMonsterSpawner::FindSpawnTransform(FTransform& OutSpawnTransform) const
{
	if (!SpawnArea)
	{
		return false;
	}

	const FVector BoxExtent = SpawnArea->GetUnscaledBoxExtent();
	const FTransform AreaTransform = SpawnArea->GetComponentTransform();
	const FVector RandomLocalPoint(
		FMath::FRandRange(-BoxExtent.X, BoxExtent.X),
		FMath::FRandRange(-BoxExtent.Y, BoxExtent.Y),
		bProjectSpawnLocationToGround ? BoxExtent.Z : FMath::FRandRange(-BoxExtent.Z, BoxExtent.Z));
	FVector SpawnLocation = AreaTransform.TransformPosition(RandomLocalPoint);

	if (bProjectSpawnLocationToGround)
	{
		const FVector TraceStart = SpawnLocation + FVector::UpVector * GroundTraceDistance;
		const FVector TraceEnd =
			AreaTransform.TransformPosition(FVector(RandomLocalPoint.X, RandomLocalPoint.Y, -BoxExtent.Z))
			- FVector::UpVector * GroundTraceDistance;
		FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(Rogue10mMonsterSpawnerGroundTrace), false, this);
		FHitResult GroundHit;
		const UWorld* World = GetWorld();
		if (!World || !World->LineTraceSingleByChannel(
			GroundHit,
			TraceStart,
			TraceEnd,
			ECC_WorldStatic,
			QueryParameters))
		{
			return false;
		}
		SpawnLocation = GroundHit.ImpactPoint;
		// Character locations are capsule centers, while a ground trace returns the floor.
		// Starting the capsule inside Landscape can make AdjustIfPossible reject every spawn.
		if (const ARogue10mBasicMonster* MonsterTemplate = MonsterClass.GetDefaultObject())
		{
			if (const UCapsuleComponent* Capsule = MonsterTemplate->GetCapsuleComponent())
			{
				SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight() + 2.0f;
			}
		}
	}

	const FRotator SpawnRotation(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f);
	OutSpawnTransform = FTransform(SpawnRotation, SpawnLocation);
	return true;
}

void ARogue10mMonsterSpawner::HandleSpawnedMonsterDestroyed(AActor* DestroyedActor)
{
	SpawnedMonsters.RemoveAllSwap(
		[DestroyedActor](const TWeakObjectPtr<ARogue10mBasicMonster>& MonsterReference)
		{
			return !MonsterReference.IsValid() || MonsterReference.Get() == DestroyedActor;
		},
		EAllowShrinking::No);
}
