// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mOpenWorldBootstrap.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystemTypes.h"
#include "Rogue10m.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mItemDataAsset.h"
#include "TimerManager.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#endif

#if WITH_EDITOR
#include "AI/NavigationSystemConfig.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#endif

namespace Rogue10mEntryLoot
{
	const FName PlacedTag(TEXT("Rogue10m.EntryGreatSword.Placed"));
	const FName PickupTag(TEXT("Rogue10m.EntryGreatSword.Pickup"));
}

namespace Rogue10mBossSouthApproach
{
	const FName PlatformTag(TEXT("Rogue10m.OpenWorld"));
	const FName PlacedTag(TEXT("Rogue10m.BossSouthApproach.Placed"));
	const FName RampTag(TEXT("Rogue10m.BossSouthApproach.Ramp"));
	const FVector PlatformCenter(15000.0, 68000.0, 9550.0);
	const FVector PlatformExtent(5500.0, 5500.0, 50.0);

	bool IsRunWorld(const UWorld* World)
	{
		return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
			== TEXT("/Game/Rogue10m/Maps/OpenRunMap");
	}

	// Local X follows the slope. Offsetting along its normal keeps both TOP endpoints exact.
	bool MakeTransform(const FVector& Start, const FVector& End, double Width, double Thickness,
		double MaximumSlope, FTransform& Out)
	{
		if (Start.ContainsNaN() || End.ContainsNaN() || !FMath::IsFinite(Width)
			|| !FMath::IsFinite(Thickness) || !FMath::IsFinite(MaximumSlope)
			|| Width < 100.0 || Thickness < 10.0 || MaximumSlope <= 0.0 || MaximumSlope > 30.0) { return false; }
		const FVector Delta = End - Start;
		const double Horizontal = Delta.Size2D();
		const double Slope = FMath::RadiansToDegrees(FMath::Atan2(Delta.Z, Horizontal));
		if (Horizontal < 100.0 || Delta.Z <= 0.0 || !FMath::IsFinite(Slope) || Slope > MaximumSlope) { return false; }
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal(), FVector::UpVector).ToQuat();
		Out = FTransform(Rotation, (Start + End) * 0.5 - Rotation.GetUpVector() * (Thickness * 0.5),
			FVector(Delta.Size() / 100.0, Width / 100.0, Thickness / 100.0));
		return !Out.ContainsNaN();
	}

	bool IsLandscape(const AActor* Actor)
	{
		// Reflection avoids adding a new runtime module dependency for one type check.
		for (const UClass* Class = Actor ? Actor->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetPathName() == TEXT("/Script/Landscape.LandscapeProxy")) { return true; }
		}
		return false;
	}
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
		if (bEnableBossSouthApproach && HasAuthority() && World->GetNetMode() == NM_Standalone
			&& Rogue10mBossSouthApproach::IsRunWorld(World))
		{
			World->GetTimerManager().SetTimer(BossSouthApproachTimerHandle, this,
				&ARogue10mOpenWorldBootstrap::TryPlaceBossSouthApproach, 1.0f, true);
		}
	}
}

void ARogue10mOpenWorldBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvokerTimerHandle);
		World->GetTimerManager().ClearTimer(BossSouthApproachTimerHandle);
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

void ARogue10mOpenWorldBootstrap::TryPlaceBossSouthApproach()
{
	using namespace Rogue10mBossSouthApproach;
	UWorld* World = GetWorld();
	if (!World) { return; }
	const auto Finish = [this, World]()
	{
		bBossSouthApproachFinished = true;
		World->GetTimerManager().ClearTimer(BossSouthApproachTimerHandle);
	};
	if (bBossSouthApproachFinished || !bEnableBossSouthApproach || !World->IsGameWorld()
		|| !HasAuthority() || World->GetNetMode() != NM_Standalone
		|| !IsRunWorld(World)) { Finish(); return; }
	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings) { return; }
	if (Settings->ActorHasTag(PlacedTag)) { Finish(); return; }
	++BossSouthApproachAttempts;
	ON_SCOPE_EXIT
	{
		if (!bBossSouthApproachFinished && BossSouthApproachAttempts >= FMath::Clamp(BossSouthApproachMaximumAttempts, 1, 1200))
		{
			UE_LOG(LogRogue10m, Warning, TEXT("%s stopped south boss approach after %d attempts; no unverified geometry was spawned."),
				*GetName(), BossSouthApproachAttempts);
			Finish();
		}
	};
	const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
	if (!Run || !Run->IsRunActive() || !IsWorldStreamingReady()) { return; }
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || !Player->GetController()) { return; }
	AStaticMeshActor* Platform = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(RampTag)) { Finish(); return; }
		if (!It->ActorHasTag(PlatformTag)) { continue; }
		const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		FVector Origin, Extent; It->GetActorBounds(false, Origin, Extent);
		if (!Mesh || Mesh->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")
			|| !Origin.Equals(PlatformCenter, 1.0) || !Extent.Equals(PlatformExtent, 1.0)
			|| !Mesh->GetBounds().Origin.IsNearlyZero(0.1) || !Mesh->GetBounds().BoxExtent.Equals(FVector(50.0), 0.1)) { continue; }
		if (Platform)
		{
			UE_LOG(LogRogue10m, Warning, TEXT("%s found duplicate matching boss platforms; approach disabled."), *GetName());
			Finish(); return;
		}
		Platform = *It;
	}
	if (!Platform) { return; }
	if (BossSouthApproachStart.ContainsNaN() || BossSouthApproachEnd.ContainsNaN()) { Finish(); return; }
	FHitResult Ground, Top;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mBossSouthApproachGround), false, this);
	const auto Trace = [World, &Query](const FVector2D& XY, FHitResult& Hit)
	{
		return World->LineTraceSingleByObjectType(Hit, FVector(XY.X, XY.Y, 20000.0), FVector(XY.X, XY.Y, 0.0),
			FCollisionObjectQueryParams(ECC_WorldStatic), Query) && Hit.bBlockingHit && !Hit.bStartPenetrating;
	};
	if (!Trace(BossSouthApproachStart, Ground) || !IsLandscape(Ground.GetActor()) || Ground.ImpactNormal.Z < 0.65
		|| !Trace(BossSouthApproachEnd, Top) || Top.GetActor() != Platform || Top.ImpactNormal.Z < 0.99
		|| !FMath::IsNearlyEqual(Top.ImpactPoint.Z, PlatformCenter.Z + PlatformExtent.Z, 1.0)) { return; }
	FTransform Placement;
	const FVector Start = Ground.ImpactPoint + FVector(0.0, 0.0, 2.0);
	const FVector End = Top.ImpactPoint + FVector(0.0, 0.0, 2.0);
	if (!MakeTransform(Start, End, BossSouthApproachWidth, BossSouthApproachThickness, BossSouthApproachMaximumSlope, Placement))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s rejected invalid/steep south approach geometry."), *GetName()); Finish(); return;
	}
	const FBox Footprint = FBox(FVector(-50.0), FVector(50.0)).TransformBy(Placement.ToMatrixWithScale()).ExpandBy(500.0);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		FVector PawnOrigin, PawnExtent;
		It->GetActorBounds(true, PawnOrigin, PawnExtent);
		if (PawnOrigin.ContainsNaN() || PawnExtent.ContainsNaN()
			|| Footprint.IntersectXY(FBox(PawnOrigin - PawnExtent, PawnOrigin + PawnExtent)))
		{
			if (BossSouthApproachAttempts % 10 == 0)
			{
				UE_LOG(LogRogue10m, Warning, TEXT("%s deferred south approach: pawn %s is near the proposed footprint."), *GetName(), *It->GetName());
			}
			return;
		}
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = Settings; Parameters.OverrideLevel = World->PersistentLevel;
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.bDeferConstruction = true;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Ramp = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Placement, Parameters);
	UStaticMeshComponent* Component = Ramp ? Ramp->GetStaticMeshComponent() : nullptr;
	if (!Component) { if (Ramp) { Ramp->Destroy(); } return; }
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetMobility(EComponentMobility::Movable);
	if (!Component->SetStaticMesh(Platform->GetStaticMeshComponent()->GetStaticMesh()))
	{
		Ramp->Destroy(); return; // New transient actor only; no world receipt on failed setup.
	}
	for (int32 Index = 0; Index < Platform->GetStaticMeshComponent()->GetNumMaterials(); ++Index)
	{
		Component->SetMaterial(Index, Platform->GetStaticMeshComponent()->GetMaterial(Index));
	}
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionProfileName(TEXT("BlockAll"));
	Component->SetCollisionObjectType(ECC_WorldStatic);
	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ramp->Tags.AddUnique(RampTag);
	Settings->Tags.AddUnique(PlacedTag); // Commit before FinishSpawning can re-enter a duplicate bootstrap.
	Finish(); Ramp->FinishSpawning(Placement);
	UE_LOG(LogRogue10m, Log, TEXT("%s placed transient south approach from %s to %s; platform %s unchanged (attempt %d)."),
		*GetName(), *Start.ToString(), *End.ToString(), *Platform->GetPathName(), BossSouthApproachAttempts);
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


#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossSouthApproachTransformTest, "Rogue10m.OpenWorld.BossSouthApproach.Transform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogueBossSouthApproachTransformTest::RunTest(const FString& Parameters)
{
    const FVector Start(12000,60000,8722), End(15000,62550,9602);
    FTransform Placement;
    TestTrue(TEXT("valid authored ramp contract"), Rogue10mBossSouthApproach::MakeTransform(Start,End,160,60,20,Placement));
    TestTrue(TEXT("lower top endpoint touches ground"), Placement.TransformPosition(FVector(-50,0,50)).Equals(Start,0.001));
    TestTrue(TEXT("upper top endpoint touches platform"), Placement.TransformPosition(FVector(50,0,50)).Equals(End,0.001));
    const double TopWidth = FVector::Distance(Placement.TransformPosition(FVector(0,-50,50)), Placement.TransformPosition(FVector(0,50,50)));
    TestTrue(TEXT("actual top width is160cm"), FMath::IsNearlyEqual(TopWidth,160.0,0.001));
    TestTrue(TEXT("upward face and positive nonuniform scale"), Placement.GetRotation().GetUpVector().Z > .95 && Placement.GetScale3D().GetMin() > 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossSouthApproachInvalidTest, "Rogue10m.OpenWorld.BossSouthApproach.InvalidGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogueBossSouthApproachInvalidTest::RunTest(const FString& Parameters)
{
    const FVector Start(12000,60000,8722), End(15000,62550,9602); FTransform Out;
    const double NaN=std::numeric_limits<double>::quiet_NaN(), Inf=std::numeric_limits<double>::infinity();
    TestFalse(TEXT("zero horizontal run"), Rogue10mBossSouthApproach::MakeTransform(Start,Start+FVector(0,0,100),160,60,20,Out));
    TestFalse(TEXT("descending endpoint rejected"), Rogue10mBossSouthApproach::MakeTransform(End,Start,160,60,20,Out));
    TestFalse(TEXT("too steep for configured maximum"), Rogue10mBossSouthApproach::MakeTransform(Start,End,160,60,5,Out));
    TestFalse(TEXT("unsupported narrow width rejected"), Rogue10mBossSouthApproach::MakeTransform(Start,End,99,60,20,Out));
    TestFalse(TEXT("invalid thickness rejected"), Rogue10mBossSouthApproach::MakeTransform(Start,End,160,9,20,Out));
    TestFalse(TEXT("nonfinite point rejected"), Rogue10mBossSouthApproach::MakeTransform(FVector(NaN,0,0),End,160,60,20,Out));
    TestFalse(TEXT("nonfinite size rejected"), Rogue10mBossSouthApproach::MakeTransform(Start,End,Inf,60,20,Out));
    TestFalse(TEXT("nonfinite slope rejected"), Rogue10mBossSouthApproach::MakeTransform(Start,End,160,60,NaN,Out));
    return true;
}
#endif
