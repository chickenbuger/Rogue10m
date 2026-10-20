// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mOpenWorldBootstrap.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystemTypes.h"
#include "Rogue10m.h"
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
#include "GameFramework/WorldSettings.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#endif

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
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	URogue10mOpenWorldNavigationSystem* NavigationSystem =
		Cast<URogue10mOpenWorldNavigationSystem>(UNavigationSystemV1::GetCurrent(World));
	if (!Pawn || !Pawn->GetController() || !NavigationSystem
		|| !NavigationSystem->IsActiveTilesGenerationEnabled())
	{
		ReleasePlayerNavigationInvoker();
		if (!bHasRegisteredOnce
			&& ++InitialRegistrationAttempts >= FMath::Max(1, MaximumInitialRegistrationAttempts))
		{
			World->GetTimerManager().ClearTimer(InvokerTimerHandle);
			UE_LOG(LogRogue10m, Error,
				TEXT("%s could not register a possessed player with the OpenRun navigation system after %d attempts."),
				*GetName(), InitialRegistrationAttempts);
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
