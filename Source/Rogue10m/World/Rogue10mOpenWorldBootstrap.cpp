// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mOpenWorldBootstrap.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystemTypes.h"
#include "Rogue10m.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mItemDataAsset.h"
#include "TimerManager.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#if WITH_EDITOR
#include "AI/NavigationSystemConfig.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#endif

namespace Rogue10mEntryLoot
{
	const FName PlacedTag(TEXT("Rogue10m.EntryGreatSword.Placed"));
	const FName PickupTag(TEXT("Rogue10m.EntryGreatSword.Pickup"));
}

void URogue10mOpenWorldNavigationSystem::Configure(const UNavigationSystemConfig& Config)
{
	Super::Configure(Config);
	// Configure precedes world navigation initialization; no global config or late rebuild is needed.
	bGenerateNavigationOnlyAroundNavigationInvokers = true;
}

ARogue10mOpenWorldBootstrap::ARogue10mOpenWorldBootstrap()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARogue10mOpenWorldBootstrap::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		World->GetTimerManager().SetTimer(
			InvokerTimerHandle, this, &ARogue10mOpenWorldBootstrap::RefreshPlayerNavigationInvoker,
			FMath::Max(0.1f, InitialRegistrationIntervalSeconds), true);
	}
}

void ARogue10mOpenWorldBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvokerTimerHandle);
	}
	ReleasePlayerNavigationInvoker();
	Super::EndPlay(EndPlayReason);
}

void ARogue10mOpenWorldBootstrap::RefreshPlayerNavigationInvoker()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TryPlaceEntryGreatSword();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	URogue10mOpenWorldNavigationSystem* NavigationSystem =
		Cast<URogue10mOpenWorldNavigationSystem>(UNavigationSystemV1::GetCurrent(World));
	if (!Pawn || !Pawn->GetController() || !NavigationSystem
		|| !NavigationSystem->IsActiveTilesGenerationEnabled())
	{
		ReleasePlayerNavigationInvoker();
		if (!bHasRegisteredOnce)
		{
			++InitialRegistrationAttempts;
			const int32 NavigationBudget = FMath::Max(1, MaximumInitialRegistrationAttempts);
			if (InitialRegistrationAttempts == NavigationBudget)
			{
				UE_LOG(LogRogue10m, Error,
					TEXT("%s could not register a possessed player with the OpenRun navigation system after %d attempts."),
					*GetName(), InitialRegistrationAttempts);
			}
			const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
			const bool bPendingEntryPlacement = bEnableEntryGreatSword && !bEntryGreatSwordFinished
				&& HasAuthority() && World->GetNetMode() == NM_Standalone && Run && Run->IsRunActive()
				&& UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("OpenRunMap");
			// The shared timer must not silently cancel the loot's remaining bounded attempts.
			if (InitialRegistrationAttempts >= NavigationBudget && !bPendingEntryPlacement)
			{
				World->GetTimerManager().ClearTimer(InvokerTimerHandle);
			}
		}
		return;
	}

	const float GenerationRadius = FMath::Max(100.0f, NavigationGenerationRadius);
	const float RemovalRadius = FMath::Max(GenerationRadius, NavigationRemovalRadius);
	if (RegisteredPawn.Get() == Pawn && RegisteredNavigationSystem.Get() == NavigationSystem
		&& bRegistrationIssued
		&& FMath::IsNearlyEqual(RegisteredGenerationRadius, GenerationRadius)
		&& FMath::IsNearlyEqual(RegisteredRemovalRadius, RemovalRadius))
	{
		return;
	}

	ReleasePlayerNavigationInvoker();
	NavigationSystem->RegisterNavigationInvoker(Pawn, GenerationRadius, RemovalRadius);
	RegisteredPawn = Pawn;
	RegisteredNavigationSystem = NavigationSystem;
	RegisteredGenerationRadius = GenerationRadius;
	RegisteredRemovalRadius = RemovalRadius;
	bRegistrationIssued = true;
	if (!bHasRegisteredOnce)
	{
		bHasRegisteredOnce = true;
		World->GetTimerManager().SetTimer(
			InvokerTimerHandle, this, &ARogue10mOpenWorldBootstrap::RefreshPlayerNavigationInvoker,
			FMath::Max(0.1f, PawnRefreshIntervalSeconds), true);
	}
	UE_LOG(LogRogue10m, Log, TEXT("%s registered %s for local navigation (generation %.0f cm, removal %.0f cm)."),
		*GetName(), *GetNameSafe(Pawn), GenerationRadius, RemovalRadius);
}

void ARogue10mOpenWorldBootstrap::ReleasePlayerNavigationInvoker()
{
	if (bRegistrationIssued)
	{
		if (UNavigationSystemV1* NavigationSystem = RegisteredNavigationSystem.Get())
		{
			if (APawn* Pawn = RegisteredPawn.Get())
			{
				NavigationSystem->UnregisterNavigationInvoker(Pawn);
			}
		}
	}
	RegisteredPawn.Reset();
	RegisteredNavigationSystem.Reset();
	bRegistrationIssued = false;
}

bool ARogue10mOpenWorldBootstrap::IsPlayerNavigationInvokerRegistered() const
{
	const APawn* Pawn = RegisteredPawn.Get();
	const UNavigationSystemV1* NavigationSystem = RegisteredNavigationSystem.Get();
	if (!bRegistrationIssued || !Pawn || !NavigationSystem
		|| !NavigationSystem->IsActiveTilesGenerationEnabled()
		|| UGameplayStatics::GetPlayerPawn(GetWorld(), 0) != Pawn)
	{
		return false;
	}
	// GetInvokerLocations is the public engine cache, refreshed periodically. It has no actor IDs.
	// Permit normal movement between updates; a distant teleport waits for a fresh sample.
	const double PositionTolerance = FMath::Max(300.0, Pawn->GetVelocity().Size() * 2.0);
	for (const FNavigationInvokerRaw& Invoker : NavigationSystem->GetInvokerLocations())
	{
		if (FMath::IsNearlyEqual(Invoker.RadiusMin, RegisteredGenerationRadius, 1.0f)
			&& FMath::IsNearlyEqual(Invoker.RadiusMax, RegisteredRemovalRadius, 1.0f)
			&& FVector::DistSquared(Invoker.Location, Pawn->GetActorLocation()) <= FMath::Square(PositionTolerance))
		{
			return true;
		}
	}
	return false;
}

bool ARogue10mOpenWorldBootstrap::IsWorldStreamingReady() const
{
	UWorld* World = GetWorld();
	UWorldPartition* Partition = World ? World->GetWorldPartition() : nullptr;
	UWorldPartitionSubsystem* Streaming = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	return Partition && Partition->IsStreamingEnabled() && Streaming && Streaming->IsAllStreamingCompleted();
}

bool ARogue10mOpenWorldBootstrap::HasPlacedEntryGreatSword() const
{
	const UWorld* World = GetWorld();
	const AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	return Settings && Settings->ActorHasTag(Rogue10mEntryLoot::PlacedTag);
}

void ARogue10mOpenWorldBootstrap::TryPlaceEntryGreatSword()
{
	if (bEntryGreatSwordFinished || !bEnableEntryGreatSword) { return; }
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !HasAuthority() || World->GetNetMode() != NM_Standalone
		|| UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("OpenRunMap")) { return; }
	if (HasPlacedEntryGreatSword()) { bEntryGreatSwordFinished = true; return; }
	const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
	if (!Run || !Run->IsRunActive()) { return; }
	const int32 Budget = FMath::Clamp(MaximumInitialRegistrationAttempts, 1, 600);
	++EntryGreatSwordAttempts;
	ON_SCOPE_EXIT
	{
		if (!bEntryGreatSwordFinished && EntryGreatSwordAttempts >= Budget)
		{
			bEntryGreatSwordFinished = true;
			UE_LOG(LogRogue10m, Warning, TEXT("%s exhausted %d entry greatsword placement attempts; no airborne fallback was spawned."), *GetName(), EntryGreatSwordAttempts);
		}
	};
	if (!IsWorldStreamingReady() || EntryGreatSwordLocation.ContainsNaN()) { return; }
	AWorldSettings* Settings = World->GetWorldSettings();
	const URogue10mItemDataAsset* Item = EntryGreatSwordItem.LoadSynchronous();
	if (!Settings || !Item || Item->ItemId.IsNone() || Item->Category != ERogue10mItemCategory::Equipment
		|| Item->WeaponType != ERogue10mWeaponType::GreatSword || Item->MaxStackSize < 1)
	{
		bEntryGreatSwordFinished = true;
		UE_LOG(LogRogue10m, Error, TEXT("%s has invalid entry greatsword item settings; placement stopped."), *GetName());
		return;
	}
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mEntryGreatSwordGround), false, this);
	const FVector TraceTop = EntryGreatSwordLocation + FVector(0.0, 0.0, 20000.0);
	const FVector TraceBottom = EntryGreatSwordLocation - FVector(0.0, 0.0, 30000.0);
	if (!World->LineTraceSingleByObjectType(Ground, TraceTop, TraceBottom,
		FCollisionObjectQueryParams(ECC_WorldStatic), Query) || !Ground.bBlockingHit || Ground.ImpactNormal.Z < 0.65f) { return; }

	FRogue10mItemStack Stack;
	Stack.ItemData = Item;
	Stack.InstanceId = FGuid::NewGuid();
	Stack.Category = Item->Category;
	Stack.EquipSlotType = Item->EquipSlotType;
	Stack.WeaponType = Item->WeaponType;
	Stack.DisplayName = Item->DisplayName;
	Stack.Description = Item->Description;
	Stack.Quantity = 1;
	Stack.ItemColor = Item->InventoryTint;
	Stack.RestoreHealth = FMath::Max(0.0f, Item->RestoreHealth);
	Stack.bOccupied = true;
	const FTransform Placement(FRotator::ZeroRotator, Ground.ImpactPoint + FVector(0.0, 0.0, 35.0));
	FActorSpawnParameters Parameters;
	Parameters.Owner = Settings;
	Parameters.OverrideLevel = World->PersistentLevel;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.bDeferConstruction = true;
	ARogue10mDroppedItem* Pickup = World->SpawnActor<ARogue10mDroppedItem>(ARogue10mDroppedItem::StaticClass(), Placement, Parameters);
	if (!Pickup) { return; }
	Pickup->Tags.AddUnique(Rogue10mEntryLoot::PickupTag);
	Pickup->InitializeDroppedItem(Stack);
	// Commit before BeginPlay/overlap can pick it up or re-enter another bootstrap.
	// This tag belongs only to the running UWorld's settings actor, never an editor map package.
	Settings->Tags.AddUnique(Rogue10mEntryLoot::PlacedTag);
	bEntryGreatSwordFinished = true;
	Pickup->FinishSpawning(Placement);
	UE_LOG(LogRogue10m, Log, TEXT("%s placed one entry greatsword at %s (instance %s, ground %s, normal Z %.3f)."),
		*GetName(), *Placement.GetLocation().ToString(), *Stack.InstanceId.ToString(), *GetNameSafe(Ground.GetActor()), Ground.ImpactNormal.Z);
}

#if WITH_EDITOR
namespace Rogue10mOpenWorldEditor
{
	constexpr const TCHAR* MapPackage = TEXT("/Game/Rogue10m/Maps/OpenRunMap");

	FString JsonResult(const TSharedRef<FJsonObject>& Result)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Result, Writer);
		return Output;
	}

	FString Failure(const FString& Message)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("status"), TEXT("failed"));
		Result->SetStringField(TEXT("message"), Message);
		Result->SetStringField(TEXT("expected_map"), MapPackage);
		return JsonResult(Result);
	}
}
#endif

FString ARogue10mOpenWorldBootstrap::ConfigureOpenWorldNavigation()
{
#if WITH_EDITOR
	using namespace Rogue10mOpenWorldEditor;
	if (!GEditor || GEditor->IsPlaySessionInProgress())
	{
		return Failure(TEXT("An idle Unreal Editor without PIE or a queued PIE session is required."));
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || World->WorldType != EWorldType::Editor || World->GetPackage()->GetName() != MapPackage)
	{
		return Failure(TEXT("Only the editor world /Game/Rogue10m/Maps/OpenRunMap may be configured."));
	}
	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings || !World->GetWorldPartition())
	{
		return Failure(TEXT("OpenRunMap must have WorldSettings and World Partition."));
	}
	FObjectPropertyBase* ConfigProperty = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
	FEnumProperty* GenerationProperty = FindFProperty<FEnumProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration"));
	FBoolProperty* PartitionProperty = FindFProperty<FBoolProperty>(ARecastNavMesh::StaticClass(), TEXT("bIsWorldPartitioned"));
	if (!ConfigProperty || !GenerationProperty || !PartitionProperty)
	{
		return Failure(TEXT("The engine's reflected navigation configuration properties are unavailable."));
	}
	TArray<ARecastNavMesh*> NavMeshes;
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		NavMeshes.Add(*It);
	}
	if (NavMeshes.IsEmpty())
	{
		return Failure(TEXT("OpenRunMap must contain its own RecastNavMesh actor before configuration."));
	}
	if (Settings->GetNavigationSystemConfigOverride())
	{
		return Failure(TEXT("A transient navigation configuration override is active; persistent map configuration cannot be verified."));
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10mOpenWorld", "ConfigureNavigation", "Configure OpenRunMap Navigation"));
	Settings->Modify();
	UNavigationSystemConfig* Config = Settings->GetNavigationSystemConfig();
	if (!Config || Config->GetOuter() != Settings)
	{
		Config = NewObject<UNavigationSystemConfig>(Settings, UNavigationSystemConfig::StaticClass(), NAME_None, RF_Transactional, Config);
		ConfigProperty->SetObjectPropertyValue_InContainer(Settings, Config);
	}
	Config->Modify();
	Config->NavigationSystemClass = FSoftClassPath(URogue10mOpenWorldNavigationSystem::StaticClass());
	Config->MarkPackageDirty();
	Settings->MarkPackageDirty();
	TArray<TSharedPtr<FJsonValue>> ConfiguredActors;
	for (ARecastNavMesh* NavMesh : NavMeshes)
	{
		NavMesh->Modify();
		GenerationProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			GenerationProperty->ContainerPtrToValuePtr<void>(NavMesh), static_cast<int64>(ERuntimeGenerationType::Dynamic));
		PartitionProperty->SetPropertyValue_InContainer(NavMesh, true);
		if (NavMesh->CanChangeIsSpatiallyLoadedFlag())
		{
			NavMesh->SetIsSpatiallyLoaded(false);
		}
		NavMesh->MarkPackageDirty();
		ConfiguredActors.Add(MakeShared<FJsonValueString>(NavMesh->GetPathName()));
	}
	// Do not rebuild the old editor navigation system. Saving and reloading initializes the selected class.
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), TEXT("configured"));
	Result->SetStringField(TEXT("map"), World->GetPackage()->GetName());
	Result->SetStringField(TEXT("nav_system_class"), Config->NavigationSystemClass.ToString());
	Result->SetBoolField(TEXT("invokers_only"), true);
	Result->SetStringField(TEXT("runtime_generation"), TEXT("Dynamic"));
	Result->SetBoolField(TEXT("is_world_partitioned_navmesh"), true);
	Result->SetNumberField(TEXT("recast_count"), NavMeshes.Num());
	Result->SetArrayField(TEXT("recast_actors"), ConfiguredActors);
	Result->SetBoolField(TEXT("saved"), false);
	Result->SetBoolField(TEXT("requires_reload"), true);
	UE_LOG(LogRogue10m, Log, TEXT("Configured map-owned OpenRun navigation and %d Recast actors; save/reload is required."), NavMeshes.Num());
	return JsonResult(Result);
#else
	return TEXT("{\"success\":false,\"status\":\"unsupported\",\"message\":\"ConfigureOpenWorldNavigation requires an editor build.\"}");
#endif
}
