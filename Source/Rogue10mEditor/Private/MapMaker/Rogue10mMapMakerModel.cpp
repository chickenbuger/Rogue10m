#include "MapMaker/Rogue10mMapMakerModel.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "LevelEditorViewport.h"
#include "LevelUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

namespace Rogue10mMapMaker
{
namespace
{
constexpr const TCHAR* MapPackage = TEXT("/Game/Rogue10m/Maps/OpenRunMap");
const FName OwnedTag(TEXT("Rogue10m.OpenWorld"));
const FName MakerTag(TEXT("Rogue10m.MapMaker"));
constexpr double VerticalLimitCm = 100000.0;

bool IsFinite(const FVector& Position)
{
	return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
}

bool IsAllowedPackage(const FString& PackageName)
{
	if (PackageName == MapPackage)
	{
		return true;
	}
#if WITH_DEV_AUTOMATION_TESTS
	// This exception exists only while Unreal's automation framework is running.
	// It never grants a normal editor session access to arbitrary maps.
	return GIsAutomationTesting && PackageName.StartsWith(TEXT("/Game/__MapMakerTests/MapMakerFixture_"));
#else
	return false;
#endif
}

bool IsCoordinateValid(FIntPoint Coordinate)
{
	return Coordinate.X >= MinCell && Coordinate.X <= MaxCell && Coordinate.Y >= MinCell && Coordinate.Y <= MaxCell;
}

bool IsInsideWorld(const FBox& Bounds)
{
	return Bounds.IsValid && IsFinite(Bounds.Min) && IsFinite(Bounds.Max) &&
		Bounds.Min.X >= -WorldHalfSizeCm && Bounds.Max.X <= WorldHalfSizeCm &&
		Bounds.Min.Y >= -WorldHalfSizeCm && Bounds.Max.Y <= WorldHalfSizeCm &&
		Bounds.Min.Z >= -VerticalLimitCm && Bounds.Max.Z <= VerticalLimitCm;
}

bool FindLandscapeSurface(UWorld* World, const FVector2D& Position, FVector& OutSurface)
{
	const FVector Start(Position.X, Position.Y, VerticalLimitCm);
	const FVector End(Position.X, Position.Y, -VerticalLimitCm);
	TArray<FHitResult> Hits;
	// Match FEdModeLandscape::LandscapeTrace: Landscape exposes an editor-specific
	// heightfield shape with ECC_Visibility as its object type. A component's generic
	// body trace does not select that editor shape in the same way.
	World->LineTraceMultiByObjectType(Hits, Start, End, FCollisionObjectQueryParams(ECC_Visibility),
		FCollisionQueryParams(SCENE_QUERY_STAT(Rogue10mMapMakerLandscape), true));
	bool bFound = false;
	double HighestZ = -VerticalLimitCm;
	// Component filtering is required even for the special editor object query:
	// scenery or a bridge must never become an accidental placement surface.
	for (const FHitResult& Hit : Hits)
	{
		ULandscapeHeightfieldCollisionComponent* Collision = Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent());
		if (IsValid(Collision) && Collision->GetWorld() == World && Collision->IsRegistered() &&
			Hit.bBlockingHit && IsFinite(Hit.ImpactPoint) && Hit.ImpactPoint.Z > HighestZ)
		{
			HighestZ = Hit.ImpactPoint.Z;
			OutSurface = Hit.ImpactPoint;
			bFound = true;
		}
	}
	return bFound;
}

bool HasRegisteredLoader(UWorldPartition* Partition, UWorldPartitionEditorLoaderAdapter* Loader)
{
	return Partition && Loader && Partition->GetRegisteredEditorLoaderAdapters().Contains(Loader);
}

bool IsMapExternalPackage(const FString& PackageName, const TArray<FString>& ExternalPaths)
{
	for (const FString& Path : ExternalPaths)
	{
		if (PackageName.StartsWith(Path + TEXT("/")))
		{
			return true;
		}
	}
	return false;
}
}

bool TryGetCell(const FVector& Position, FIntPoint& OutCoordinate)
{
	if (!IsFinite(Position) || Position.X < -WorldHalfSizeCm || Position.X > WorldHalfSizeCm ||
		Position.Y < -WorldHalfSizeCm || Position.Y > WorldHalfSizeCm)
	{
		return false;
	}
	// floor, rather than integer truncation, gives negative coordinates the same
	// half-open interval rule as positive coordinates. The outer +900m edge belongs
	// to the last clipped cell so the complete authored map remains addressable.
	OutCoordinate = FIntPoint(
		FMath::Clamp(FMath::FloorToInt(Position.X / CellSizeCm), MinCell, MaxCell),
		FMath::Clamp(FMath::FloorToInt(Position.Y / CellSizeCm), MinCell, MaxCell));
	return true;
}

FBox GetCellBounds(FIntPoint Coordinate)
{
	if (!IsCoordinateValid(Coordinate))
	{
		return FBox(ForceInit);
	}
	return FBox(
		FVector(FMath::Max(-WorldHalfSizeCm, Coordinate.X * CellSizeCm), FMath::Max(-WorldHalfSizeCm, Coordinate.Y * CellSizeCm), -VerticalLimitCm),
		FVector(FMath::Min(WorldHalfSizeCm, (Coordinate.X + 1) * CellSizeCm), FMath::Min(WorldHalfSizeCm, (Coordinate.Y + 1) * CellSizeCm), VerticalLimitCm));
}

bool IsEditableEnvironmentActor(const AActor* Actor)
{
	if (!IsValid(Actor) || Actor->GetClass() != AStaticMeshActor::StaticClass() || !Actor->Tags.Contains(OwnedTag) ||
		Actor->IsTemplate() || Actor->HasAnyFlags(RF_Transient) || !Actor->IsEditable() || Actor->IsLockLocation())
	{
		return false;
	}
	const FString Folder = Actor->GetFolderPath().ToString();
	const FString Label = Actor->GetActorLabel();
	// A positive folder allow-list keeps new gameplay categories protected by default.
	const TCHAR* const AllowedFolders[] = { TEXT("Scenery"), TEXT("Camp"), TEXT("Roads"), TEXT("River"), TEXT("Bridges"), TEXT("Wayfinding"), TEXT("MapMaker") };
	bool bAllowedFolder = false;
	for (const TCHAR* Category : AllowedFolders)
	{
		const FString Prefix = FString(TEXT("OpenWorld/")) + Category;
		bAllowedFolder |= Folder == Prefix || Folder.StartsWith(Prefix + TEXT("/"));
	}
	return bAllowedFolder && !Label.Contains(TEXT("Boss")) && !Label.Contains(TEXT("Boundary")) &&
		!Label.Contains(TEXT("PlayerStart")) && !Label.Contains(TEXT("Bootstrap")) &&
		!Label.Contains(TEXT("StreamProbe")) && !Label.Contains(TEXT("Checkpoint")) &&
		Actor->GetAttachParentActor() == nullptr && Actor->GetRootComponent() &&
		Actor->GetRootComponent()->GetAttachChildren().IsEmpty();
}

FModel::~FModel()
{
	ReleaseCells();
}

UWorld* FModel::GetWorld(FString& OutError) const
{
	OutError.Reset();
	if (!GEditor || GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
	{
		OutError = TEXT("Stop Play/Simulate before using World Map Maker.");
		return nullptr;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!IsValid(World) || World->WorldType != EWorldType::Editor || !IsAllowedPackage(World->GetOutermost()->GetName()))
	{
		OutError = TEXT("Open /Game/Rogue10m/Maps/OpenRunMap in the editor to use this tool.");
		return nullptr;
	}
	if (!World->PersistentLevel || !World->GetWorldPartition() || !World->GetWorldPartition()->IsInitialized())
	{
		OutError = TEXT("The current map requires an initialized World Partition.");
		return nullptr;
	}
	return World;
}

FString FModel::DescribeWorld() const
{
	FString Error;
	UWorld* World = GetWorld(Error);
	if (!World)
	{
		return Error;
	}
	UWorldPartition* Partition = World->GetWorldPartition();
	UWorldPartitionRuntimeHash* RuntimeHash = Partition->RuntimeHash;
	FString RuntimeDescription = RuntimeHash ? RuntimeHash->GetClass()->GetName() : TEXT("Unavailable");
	if (const UWorldPartitionRuntimeHashSet* HashSet = Cast<UWorldPartitionRuntimeHashSet>(RuntimeHash))
	{
		if (const URuntimePartition* MainPartition = HashSet->ResolveRuntimePartition(HashSet->GetDefaultGrid(), true))
		{
			const TOptional<FFixedGridInfo> Grid = MainPartition->GetFixedGridInfo();
			if (Grid.IsSet())
			{
				RuntimeDescription += FString::Printf(TEXT(" | runtime cell %.0fm | load range %.0fm"), Grid->CellSize / 100.0, MainPartition->LoadingRange / 100.0);
			}
		}
	}
	return FString::Printf(TEXT("%s\nWorld Partition: %s | %s\nEditing regions: 256m, 8 x 8, clipped to 1800m. Region coordinates are not runtime cell IDs."),
		*World->GetOutermost()->GetName(), Partition->IsStreamingEnabled() ? TEXT("streaming enabled") : TEXT("streaming disabled"), *RuntimeDescription);
}

TArray<FCell> FModel::Inspect(FString& OutError) const
{
	TArray<FCell> Cells;
	UWorld* World = GetWorld(OutError);
	if (!World)
	{
		return Cells;
	}
	Cells.Reserve(64);
	for (int32 Y = MinCell; Y <= MaxCell; ++Y)
	{
		for (int32 X = MinCell; X <= MaxCell; ++X)
		{
			FCell& Cell = Cells.AddDefaulted_GetRef();
			Cell.Coordinate = FIntPoint(X, Y);
			Cell.Bounds = GetCellBounds(Cell.Coordinate);
			Cell.bLoadedByTool = IsCellLoaded(Cell.Coordinate);
		}
	}
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		FIntPoint Coordinate;
		if (IsEditableEnvironmentActor(*It) && TryGetCell(It->GetActorLocation(), Coordinate))
		{
			++Cells[(Coordinate.Y - MinCell) * 8 + Coordinate.X - MinCell].LoadedEnvironmentActors;
		}
	}
	return Cells;
}

bool FModel::LoadCell(FIntPoint Coordinate, FString& OutMessage)
{
	UWorld* World = GetWorld(OutMessage);
	if (!World || !IsCoordinateValid(Coordinate))
	{
		if (World) OutMessage = TEXT("Choose an editing region within -4..3 on both axes.");
		return false;
	}
	if (LoadedWorld.Get() != World)
	{
		ReleaseCells();
		LoadedWorld = World;
	}
	if (IsCellLoaded(Coordinate))
	{
		OutMessage = TEXT("This editing region is already held by the tool.");
		return true;
	}
	UWorldPartition* Partition = World->GetWorldPartition();
	UWorldPartitionEditorLoaderAdapter* Loader = Partition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
		World, GetCellBounds(Coordinate), FString::Printf(TEXT("Rogue10m Map Maker (%d,%d)"), Coordinate.X, Coordinate.Y));
	Loader->GetLoaderAdapter()->SetUserCreated(false);
	Loader->GetLoaderAdapter()->Load();
	Loaders.Add(Coordinate, Loader);
	const bool bLoaded = Loader->GetLoaderAdapter()->IsLoaded();
	OutMessage = bLoaded ? TEXT("Editing region loaded. Other World Partition loader regions are preserved.") : TEXT("World Partition did not complete loading this editing region.");
	return bLoaded;
}

void FModel::ReleaseCells()
{
	if (UWorld* World = LoadedWorld.Get())
	{
		if (UWorldPartition* Partition = World->GetWorldPartition())
		{
			for (const auto& Entry : Loaders)
			{
				if (UWorldPartitionEditorLoaderAdapter* Loader = Entry.Value.Get(); HasRegisteredLoader(Partition, Loader))
				{
					Partition->ReleaseEditorLoaderAdapter(Loader);
				}
			}
		}
	}
	Loaders.Reset();
	LoadedWorld.Reset();
}

bool FModel::IsCellLoaded(FIntPoint Coordinate) const
{
	FString Error;
	UWorld* World = GetWorld(Error);
	const TWeakObjectPtr<UWorldPartitionEditorLoaderAdapter>* Entry = Loaders.Find(Coordinate);
	UWorldPartitionEditorLoaderAdapter* Loader = Entry ? Entry->Get() : nullptr;
	return World && LoadedWorld.Get() == World && HasRegisteredLoader(World->GetWorldPartition(), Loader) && Loader->GetLoaderAdapter()->IsLoaded();
}

bool FModel::FocusCell(FIntPoint Coordinate, FString& OutMessage) const
{
	UWorld* World = GetWorld(OutMessage);
	FBox Bounds = GetCellBounds(Coordinate);
	if (!World || !Bounds.IsValid)
	{
		if (World) OutMessage = TEXT("Choose a valid editing region first.");
		return false;
	}
	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient || !GEditor->GetLevelViewportClients().Contains(ViewportClient) ||
		ViewportClient->GetWorld() != World || !ViewportClient->Viewport)
	{
		OutMessage = TEXT("Activate a level editor viewport before focusing an editing region.");
		return false;
	}
	if (ViewportClient->IsAnyActorLocked())
	{
		OutMessage = TEXT("Stop piloting or unlock the active viewport camera before focusing an editing region.");
		return false;
	}
	// Vertical loader padding is intentionally larger than the camera focus height.
	Bounds.Min.Z = 0.0;
	Bounds.Max.Z = 16000.0;
	if (ViewportClient->IsPerspective())
	{
		// Native framing backs away along the current camera direction. A ground-level
		// direction can put the camera behind this map's boundary walls, so region
		// focus deliberately uses an elevated overview while retaining heading.
		const double CurrentYaw = ViewportClient->GetViewRotation().Yaw;
		// The outer cell center is only 61m from the boundary wall (top at Z=310m).
		// At 80 degrees its center ray clears that wall even when native framing
		// backs the camera outside the authored bounds; 70 degrees does not.
		ViewportClient->SetViewRotation(FRotator(-80.0, FMath::IsFinite(CurrentYaw) ? CurrentYaw : 0.0, 0.0));
	}
	// Frame just this viewport immediately; native framing also handles orthographic
	// zoom. Calling the client directly avoids linked-viewport camera changes.
	ViewportClient->FocusViewportOnBox(Bounds, true);
	OutMessage = ViewportClient->IsPerspective()
		? TEXT("Focused the active level viewport on the editing region from an elevated overview.")
		: TEXT("Focused the active level viewport on the editing region with native orthographic framing.");
	return true;
}

bool FModel::SelectCell(FIntPoint Coordinate, const FString& LabelFilter, FString& OutMessage) const
{
	UWorld* World = GetWorld(OutMessage);
	if (!World || !IsCoordinateValid(Coordinate))
	{
		if (World) OutMessage = TEXT("Choose a valid editing region first.");
		return false;
	}
	TArray<AActor*> MatchingActors;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		FIntPoint ActorCell;
		if (IsEditableEnvironmentActor(*It) && !FLevelUtils::IsLevelLocked(*It) &&
			TryGetCell(It->GetActorLocation(), ActorCell) && ActorCell == Coordinate &&
			(LabelFilter.IsEmpty() || It->GetActorLabel().Contains(LabelFilter, ESearchCase::IgnoreCase)))
		{
			MatchingActors.Add(*It);
		}
	}
	GEditor->SelectNone(false, true, false);
	for (AActor* Actor : MatchingActors)
	{
		GEditor->SelectActor(Actor, true, false, true);
	}
	GEditor->NoteSelectionChange();
	OutMessage = FString::Printf(TEXT("Selected %d loaded environment actors. Gameplay and system actors remain protected."), MatchingActors.Num());
	return true;
}

bool FModel::PlaceMesh(UStaticMesh* Mesh, FIntPoint Coordinate, double HeightOffsetCm, double YawDegrees, double UniformScale, FString& OutMessage)
{
	UWorld* World = GetWorld(OutMessage);
	if (!World) return false;
	if (!IsValid(Mesh) || !IsCoordinateValid(Coordinate) || !FMath::IsFinite(HeightOffsetCm) ||
		!FMath::IsFinite(YawDegrees) || !FMath::IsFinite(UniformScale) || FMath::Abs(HeightOffsetCm) > 10000.0 || UniformScale < 0.01 || UniformScale > 100.0)
	{
		OutMessage = TEXT("Select a Static Mesh and valid region. Height offset: +/-100m; uniform scale: 0.01..100; all values must be finite.");
		return false;
	}
	if (FLevelUtils::IsLevelLocked(World->PersistentLevel))
	{
		OutMessage = TEXT("The persistent level is locked.");
		return false;
	}
	const FVector Center = GetCellBounds(Coordinate).GetCenter();
	FVector Surface;
	if (!FindLandscapeSurface(World, FVector2D(Center.X, Center.Y), Surface))
	{
		OutMessage = TEXT("No loaded Landscape collision at the region center. Load the region before placing a mesh.");
		return false;
	}
	const FBox MeshBounds = Mesh->GetBoundingBox();
	const FVector Position(Surface.X, Surface.Y, Surface.Z + HeightOffsetCm - MeshBounds.Min.Z * UniformScale);
	const FTransform Transform(FRotator(0.0, FMath::UnwindDegrees(YawDegrees), 0.0), Position, FVector(UniformScale));
	if (!IsInsideWorld(MeshBounds.TransformBy(Transform)))
	{
		OutMessage = TEXT("The mesh bounds would leave the supported map bounds. Reduce its scale or choose another region.");
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10mMapMaker", "PlaceMesh", "Map Maker: Place Environment Mesh"));
	AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GEditor->AddActor(World->PersistentLevel, AStaticMeshActor::StaticClass(), Transform, true, RF_Transactional, true));
	if (!Actor)
	{
		OutMessage = TEXT("The editor could not create the environment mesh.");
		return false;
	}
	Actor->Modify();
	UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
	Component->Modify();
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionProfileName(TEXT("BlockAll"));
	Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	Actor->Tags.AddUnique(OwnedTag);
	Actor->Tags.AddUnique(MakerTag);
	Actor->SetActorLabel(FString::Printf(TEXT("OW_MM_%s"), *Mesh->GetName()));
	Actor->SetFolderPath(TEXT("OpenWorld/MapMaker"));
	if (Actor->CanChangeIsSpatiallyLoadedFlag()) Actor->SetIsSpatiallyLoaded(true);
	// AddActor follows Unreal's level placement path and creates the OFPA package.
	// Explicitly enforce it for a new actor if a factory/default was changed.
	if (!Actor->IsPackageExternal()) Actor->SetPackageExternal(true);
	Component->MarkPackageDirty();
	Actor->MarkPackageDirty();
	Actor->PostEditChange();
	Actor->PostEditMove(true);
	GEditor->RedrawLevelEditingViewports();
	OutMessage = FString::Printf(TEXT("Placed %s at region center (%d,%d), aligned to Landscape. Undo is available; save the map to keep it."), *Mesh->GetName(), Coordinate.X, Coordinate.Y);
	return true;
}

bool FModel::MoveSelectionToCell(FIntPoint Coordinate, FString& OutMessage)
{
	UWorld* World = GetWorld(OutMessage);
	if (!World) return false;
	if (!IsCoordinateValid(Coordinate))
	{
		OutMessage = TEXT("Choose a valid destination editing region.");
		return false;
	}
	TArray<AActor*> Actors;
	FVector AverageLocation = FVector::ZeroVector;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		FIntPoint SourceCell;
		if (!Actor || Actor->GetWorld() != World || Actor->GetLevel() != World->PersistentLevel ||
			!IsEditableEnvironmentActor(Actor) || FLevelUtils::IsLevelLocked(Actor) || !TryGetCell(Actor->GetActorLocation(), SourceCell))
		{
			OutMessage = TEXT("The entire selection must contain unlocked, unattached, project-owned environment StaticMeshActors. Protected, movement-locked or mixed selections cannot move.");
			return false;
		}
		Actors.Add(Actor);
		AverageLocation += Actor->GetActorLocation();
	}
	if (Actors.IsEmpty())
	{
		OutMessage = TEXT("Select environment actors before moving them.");
		return false;
	}
	AverageLocation /= Actors.Num();
	const FVector TargetCenter = GetCellBounds(Coordinate).GetCenter();
	const FVector Delta(TargetCenter.X - AverageLocation.X, TargetCenter.Y - AverageLocation.Y, 0.0);
	for (AActor* Actor : Actors)
	{
		if (!IsInsideWorld(Actor->GetComponentsBoundingBox(true).ShiftBy(Delta)))
		{
			OutMessage = TEXT("The group would leave the supported map bounds. No actors were moved.");
			return false;
		}
	}
	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10mMapMaker", "MoveEnvironment", "Map Maker: Move Environment Group"));
	for (AActor* Actor : Actors)
	{
		Actor->Modify();
		Actor->GetRootComponent()->Modify();
		Actor->SetActorLocation(Actor->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
		Actor->PostEditMove(true);
		Actor->GetRootComponent()->MarkPackageDirty();
		Actor->MarkPackageDirty();
	}
	GEditor->RedrawLevelEditingViewports();
	OutMessage = FString::Printf(TEXT("Moved %d environment actors to region (%d,%d). Relative offsets and heights were preserved; inspect terrain contact before saving."), Actors.Num(), Coordinate.X, Coordinate.Y);
	return true;
}

bool FModel::SaveMap(FString& OutMessage) const
{
	UWorld* World = GetWorld(OutMessage);
	if (!World) return false;
	UPackage* WorldPackage = World->GetOutermost();
	const TArray<FString> ExternalPaths = ULevel::GetExternalObjectsPaths(WorldPackage->GetName());
	TArray<UPackage*> Packages;
	if (WorldPackage->IsDirty()) Packages.Add(WorldPackage);
	// Include loaded external objects even when their actors have been deleted or
	// moved out of a loaded region. Package names must belong to this exact map.
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (Package->IsDirty() && !Package->HasAnyFlags(RF_Transient) && IsMapExternalPackage(Package->GetName(), ExternalPaths))
		{
			Packages.AddUnique(Package);
		}
	}
	if (Packages.IsEmpty())
	{
		OutMessage = TEXT("The current map and its loaded external actor packages have no unsaved changes.");
		return true;
	}
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	bool bStillDirty = false;
	for (UPackage* Package : Packages) bStillDirty |= Package->IsDirty();
	OutMessage = bSaved && !bStillDirty
		? FString::Printf(TEXT("Saved %d packages belonging to the current map. Other maps and asset packages were excluded."), Packages.Num())
		: TEXT("The current map save did not complete. Inspect the editor log and remaining dirty packages before closing the map.");
	return bSaved && !bStillDirty;
}
}
