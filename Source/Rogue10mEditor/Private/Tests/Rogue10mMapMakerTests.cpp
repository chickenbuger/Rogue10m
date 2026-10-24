#if WITH_DEV_AUTOMATION_TESTS

#include "MapMaker/Rogue10mMapMakerModel.h"
#include "MapMaker/SRogue10mMapMaker.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Landscape.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#include <limits>

using namespace Rogue10mMapMaker;

namespace
{
bool CleanupFixtureExternalPackages(const FString& FixturePackage, FAutomationTestBase& Test)
{
	const FString Prefix(TEXT("/Game/__MapMakerTests/MapMakerFixture_"));
	FGuid FixtureGuid;
	if (!FixturePackage.StartsWith(Prefix, ESearchCase::CaseSensitive) ||
		FixturePackage.Len() != Prefix.Len() + 32 ||
		!FGuid::ParseExact(FixturePackage.Mid(Prefix.Len()), EGuidFormats::Digits, FixtureGuid))
	{
		Test.AddError(TEXT("External-package cleanup requires one exact MapMakerFixture_ plus 32-digit GUID package."));
		return false;
	}
	if (FPackageName::DoesPackageExist(FixturePackage))
	{
		Test.AddError(TEXT("Delete the disposable fixture map through the editor before cleaning orphan external packages."));
		return false;
	}
	const FString FixtureName = FPackageName::GetShortName(FixturePackage);
	TArray<UPackage*> Packages;
	TArray<FString> Directories;
	for (const FString& ExternalPath : ULevel::GetExternalObjectsPaths(FixturePackage))
	{
		const FString ExpectedActorPath = TEXT("/Game/__ExternalActors__/__MapMakerTests/") + FixtureName;
		const FString ExpectedObjectPath = TEXT("/Game/__ExternalObjects__/__MapMakerTests/") + FixtureName;
		if (ExternalPath != ExpectedActorPath && ExternalPath != ExpectedObjectPath)
		{
			Test.AddError(FString::Printf(TEXT("Refused external path outside the exact fixture roots: %s"), *ExternalPath));
			return false;
		}
		const FString Directory = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(ExternalPath));
		Directories.Add(Directory);
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.uasset"), true, false);
		for (const FString& Filename : Files)
		{
			FString PackageName;
			if (!FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName) ||
				!PackageName.StartsWith(ExternalPath + TEXT("/"), ESearchCase::CaseSensitive))
			{
				Test.AddError(FString::Printf(TEXT("Refused package outside the exact fixture subtree: %s"), *Filename));
				return false;
			}
			Packages.AddUnique(CreatePackage(*PackageName));
		}
	}
	if (!Packages.IsEmpty())
	{
		// Use Unreal's package deletion path for orphan actor/folder packages that are
		// absent from the Asset Registry and therefore invisible to DeleteDirectory.
		// The exact unique-GUID roots and absent fixture map were checked above.
		ObjectTools::CleanupAfterSuccessfulDelete(Packages, false);
	}
	bool bClean = true;
	for (const FString& Directory : Directories)
	{
		TArray<FString> Remaining;
		IFileManager::Get().FindFilesRecursive(Remaining, *Directory, TEXT("*"), true, false);
		bClean &= Test.TestEqual(FString::Printf(TEXT("No files remain in isolated external fixture root %s"), *Directory), Remaining.Num(), 0);
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMapMakerGridTest,
	"Rogue10m.MapMaker.GridBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mMapMakerGridTest::RunTest(const FString& Parameters)
{
	FIntPoint Cell;
	TestTrue(TEXT("Origin is in the map"), TryGetCell(FVector::ZeroVector, Cell));
	TestEqual(TEXT("Origin uses cell zero"), Cell, FIntPoint::ZeroValue);
	TestTrue(TEXT("Negative fractional point is in the map"), TryGetCell(FVector(-0.01, -25600.01, 0), Cell));
	TestEqual(TEXT("Negative coordinates floor rather than truncate"), Cell, FIntPoint(-1, -2));
	TestTrue(TEXT("Internal negative boundary is in the map"), TryGetCell(FVector(-25600, 25600, 0), Cell));
	TestEqual(TEXT("Internal boundaries belong to the higher cell"), Cell, FIntPoint(-1, 1));
	TestTrue(TEXT("Minimum map corner is included"), TryGetCell(FVector(-90000, -90000, 0), Cell));
	TestEqual(TEXT("Minimum map corner maps to -4,-4"), Cell, FIntPoint(-4, -4));
	TestTrue(TEXT("Maximum map corner is included"), TryGetCell(FVector(90000, 90000, 0), Cell));
	TestEqual(TEXT("Maximum map corner maps to 3,3"), Cell, FIntPoint(3, 3));
	TestFalse(TEXT("Point below west boundary is rejected"), TryGetCell(FVector(-90000.01, 0, 0), Cell));
	TestFalse(TEXT("Point above north boundary is rejected"), TryGetCell(FVector(0, 90000.01, 0), Cell));
	TestFalse(TEXT("Huge finite coordinates are rejected before integer conversion"), TryGetCell(FVector(1.e100, 0, 0), Cell));
	TestFalse(TEXT("NaN is rejected"), TryGetCell(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), Cell));
	TestFalse(TEXT("Infinity is rejected"), TryGetCell(FVector(0, std::numeric_limits<double>::infinity(), 0), Cell));
	TestFalse(TEXT("Nonfinite Z is rejected"), TryGetCell(FVector(0, 0, std::numeric_limits<double>::infinity()), Cell));
	TestFalse(TEXT("Out of range negative grid coordinate has no bounds"), GetCellBounds(FIntPoint(-5, 0)).IsValid != 0);
	TestFalse(TEXT("Out of range positive grid coordinate has no bounds"), GetCellBounds(FIntPoint(4, 0)).IsValid != 0);

	double TotalArea = 0;
	int32 Count = 0;
	for (int32 Y = MinCell; Y <= MaxCell; ++Y)
	{
		for (int32 X = MinCell; X <= MaxCell; ++X)
		{
			const FIntPoint Coordinate(X, Y);
			const FBox Bounds = GetCellBounds(Coordinate);
			TestTrue(TEXT("Every visible grid cell has valid bounds"), Bounds.IsValid != 0);
			TestTrue(TEXT("Grid center resolves back to its own cell"), TryGetCell(Bounds.GetCenter(), Cell) && Cell == Coordinate);
			TestTrue(TEXT("Clipped grid bounds stay within the map"),
				Bounds.Min.X >= -WorldHalfSizeCm && Bounds.Min.Y >= -WorldHalfSizeCm &&
				Bounds.Max.X <= WorldHalfSizeCm && Bounds.Max.Y <= WorldHalfSizeCm);
			TestTrue(TEXT("Each cell spans at most 256m per horizontal axis"),
				Bounds.GetSize().X <= CellSizeCm && Bounds.GetSize().Y <= CellSizeCm);
			TotalArea += Bounds.GetSize().X * Bounds.GetSize().Y;
			++Count;
		}
	}
	TestEqual(TEXT("Eight by eight work grid"), Count, 64);
	TestEqual(TEXT("Clipped cells cover exactly the 1.8km square"), TotalArea, 180000.0 * 180000.0);
	TestEqual(TEXT("West edge is clipped"), GetCellBounds(FIntPoint(-4, 0)).Min.X, -90000.0);
	TestEqual(TEXT("East edge is clipped"), GetCellBounds(FIntPoint(3, 0)).Max.X, 90000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMapMakerEnvironmentPolicyTest,
	"Rogue10m.MapMaker.EnvironmentPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mMapMakerEnvironmentPolicyTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false);
	UWorld* Preview = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("Create isolated preview world"), Preview))
	{
		return false;
	}
	ON_SCOPE_EXIT { Preview->DestroyWorld(false); };
	AStaticMeshActor* Actor = Preview->SpawnActor<AStaticMeshActor>();
	if (!TestNotNull(TEXT("Create policy test mesh"), Actor))
	{
		return false;
	}
	Actor->SetActorLabel(TEXT("MapMakerPolicyMesh"));
	Actor->SetFolderPath(FName(TEXT("OpenWorld/MapMaker")));
	TestFalse(TEXT("Null actor is not editable"), IsEditableEnvironmentActor(nullptr));
	TestFalse(TEXT("Untagged vendor or user actor is not opted in"), IsEditableEnvironmentActor(Actor));
	Actor->Tags.Add(FName(TEXT("Rogue10m.OpenWorld")));
	TestTrue(TEXT("Project owned native environment mesh is editable"), IsEditableEnvironmentActor(Actor));
	Actor->SetActorLabel(TEXT("OW_PersistentBossGround"));
	TestFalse(TEXT("Persistent boss ground is protected even with ownership tag"), IsEditableEnvironmentActor(Actor));
	Actor->SetActorLabel(TEXT("MapMakerPolicyMesh"));
	for (const TCHAR* Folder : { TEXT("OpenWorld/Gameplay"), TEXT("OpenWorld/Terrain"), TEXT("OpenWorld/Boundaries") })
	{
		Actor->SetFolderPath(FName(Folder));
		TestFalse(FString::Printf(TEXT("Protected folder %s is not movable"), Folder), IsEditableEnvironmentActor(Actor));
	}
	AActor* GameplayActor = Preview->SpawnActor<AActor>();
	GameplayActor->Tags.Add(FName(TEXT("Rogue10m.OpenWorld")));
	TestFalse(TEXT("Gameplay Actor ownership does not bypass environment class policy"), IsEditableEnvironmentActor(GameplayActor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMapMakerIntegrationTest,
	"Rogue10m.MapMaker.EditorIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRogue10mMapMakerIntegrationTest::RunTest(const FString& Parameters)
{
	// NewMap replaces the current editor world. This test is intentionally opt-in for an isolated unattended process.
	if (!FApp::IsUnattended() || !FParse::Param(FCommandLine::Get(), TEXT("MapMakerIntegrationTest")))
	{
		AddError(TEXT("Run this fixture in a separate UnrealEditor-Cmd process with -Unattended -MapMakerIntegrationTest."));
		return false;
	}
	if (!TestNotNull(TEXT("Editor engine available"), GEditor))
	{
		return false;
	}
	FString StaleFixturePackage;
	if (FParse::Value(FCommandLine::Get(), TEXT("MapMakerCleanupFixture="), StaleFixturePackage))
	{
		// The optional old fixture is also an explicit unique-GUID package, and its map
		// must already have been deleted. Never clean a current editor world.
		GEditor->NewMap(false);
		if (!CleanupFixtureExternalPackages(StaleFixturePackage, *this)) { return false; }
	}
	const FString FixturePackage = TEXT("/Game/__MapMakerTests/MapMakerFixture_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString FixtureFilename = FPackageName::LongPackageNameToFilename(FixturePackage, FPackageName::GetMapPackageExtension());
	UEditorAssetSubsystem* Assets = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!TestNotNull(TEXT("Editor asset subsystem available for fixture cleanup"), Assets))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		GEditor->NewMap(false);
		if (FPaths::FileExists(FixtureFilename))
		{
			TestTrue(TEXT("Delete the disposable fixture through the editor"), Assets->DeleteAsset(FixturePackage));
		}
		// Saving a new World Partition world also creates these two companion assets.
		// Delete only exact names derived from this test's unique fixture, never a shared folder.
		for (const TCHAR* Suffix : { TEXT("_HLODLayer_Instanced"), TEXT("_HLODLayer_Merged") })
		{
			const FString Companion = FixturePackage + Suffix;
			if (Assets->DoesAssetExist(Companion))
			{
				TestTrue(FString::Printf(TEXT("Delete isolated fixture companion %s"), Suffix), Assets->DeleteAsset(Companion));
			}
			TestFalse(FString::Printf(TEXT("Fixture companion %s is removed"), Suffix), Assets->DoesAssetExist(Companion));
		}
		CleanupFixtureExternalPackages(FixturePackage, *this);
	};

	UWorld* World = GEditor->NewMap(true);
	if (!TestNotNull(TEXT("Create new scratch editor world"), World) ||
		!TestNotNull(TEXT("Scratch world has World Partition"), World->GetWorldPartition()))
	{
		return false;
	}
	FModel Model;
	FString Message;
	TestNull(TEXT("An unsaved unrelated WP world is rejected"), Model.GetWorld(Message));
	TestFalse(TEXT("Placement cannot target an unrelated map"), Model.PlaceMesh(nullptr, FIntPoint(0, 0), 0, 0, 1, Message));
	TestFalse(TEXT("Saving cannot target an unrelated map"), Model.SaveMap(Message));

	// A 252m square of flat, real Landscape collision covers work-cell (0,0)'s center.
	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector(200, 200, 0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Create fixture Landscape"), Landscape))
	{
		return false;
	}
	Landscape->SetActorScale3D(FVector(400, 400, 100));
	TArray<uint16> Heights;
	Heights.Init(32768, 64 * 64);
	TMap<FGuid, TArray<uint16>> HeightLayers;
	HeightLayers.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
	MaterialLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	Landscape->Import(FGuid::NewGuid(), 0, 0, 63, 63, 1, 63, HeightLayers, TEXT(""), MaterialLayers,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	Landscape->SetActorLabel(TEXT("MapMakerFixtureTerrain"));
	Landscape->SetPackageExternal(true);
	Landscape->MarkPackageDirty();
	World->UpdateWorldComponents(true, false);
	if (!TestTrue(TEXT("Save newly created fixture under isolated package"), FEditorFileUtils::SaveMap(World, FixtureFilename)))
	{
		return false;
	}
	TestEqual(TEXT("Fixture world package is isolated"), World->GetOutermost()->GetName(), FixturePackage);
	if (!TestNotNull(TEXT("Opt-in test world accepted"), Model.GetWorld(Message)))
	{
		AddError(Message);
		return false;
	}
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<SRogue10mMapMaker> Panel = SNew(SRogue10mMapMaker);
		Panel->SlatePrepass(1.0f);
		TestTrue(TEXT("Native Map Maker panel constructs and computes a nonzero desired size"),
			Panel->GetDesiredSize().X > 0 && Panel->GetDesiredSize().Y > 0);
		// Scope destruction also exercises delegate unregistration and owned-region cleanup.
	}
	else
	{
		AddInfo(TEXT("Slate application unavailable in this process; panel construction smoke was not executed."));
	}
	{
		const TObjectPtr<UWorld> PreviousPlayWorld = GEditor->PlayWorld;
		GEditor->PlayWorld = World;
		TestNull(TEXT("PIE guard rejects editor operations"), Model.GetWorld(Message));
		TestFalse(TEXT("PIE guard rejects saving"), Model.SaveMap(Message));
		GEditor->PlayWorld = PreviousPlayWorld;
	}

	UWorldPartition* Partition = World->GetWorldPartition();
	const int32 InitialLoaderCount = Partition->GetRegisteredEditorLoaderAdapters().Num();
	UWorldPartitionEditorLoaderAdapter* UserLoader = Partition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World,
		GetCellBounds(FIntPoint(0, 0)), TEXT("MapMaker fixture: independent user region"));
	UserLoader->GetLoaderAdapter()->Load();
	ON_SCOPE_EXIT
	{
		Model.ReleaseCells();
		if (IsValid(Partition) && Partition->GetRegisteredEditorLoaderAdapters().Contains(UserLoader))
		{
			Partition->ReleaseEditorLoaderAdapter(UserLoader);
		}
	};
	if (!TestTrue(TEXT("Load one editor work cell"), Model.LoadCell(FIntPoint(0, 0), Message)))
	{
		AddError(Message);
		return false;
	}
	TestTrue(TEXT("Cell reports tool ownership"), Model.IsCellLoaded(FIntPoint(0, 0)));
	const int32 OwnedLoaderCount = Partition->GetRegisteredEditorLoaderAdapters().Num();
	TestEqual(TEXT("Tool adds exactly one region alongside the user's region"), OwnedLoaderCount, InitialLoaderCount + 2);
	TestTrue(TEXT("Loading same cell twice is idempotent"), Model.LoadCell(FIntPoint(0, 0), Message));
	TestEqual(TEXT("Idempotent load has no extra handles"), Partition->GetRegisteredEditorLoaderAdapters().Num(), OwnedLoaderCount);
	Model.ReleaseCells();
	TestFalse(TEXT("Tool release clears its cell state"), Model.IsCellLoaded(FIntPoint(0, 0)));
	TestTrue(TEXT("Tool release preserves the independent user loader"), Partition->GetRegisteredEditorLoaderAdapters().Contains(UserLoader));
	TestEqual(TEXT("Only the tool's own loader was released"), Partition->GetRegisteredEditorLoaderAdapters().Num(), InitialLoaderCount + 1);
	TestTrue(TEXT("Reload work cell after release"), Model.LoadCell(FIntPoint(0, 0), Message));

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Reuse existing engine mesh"), Mesh))
	{
		return false;
	}
	const FString MeshAssetPath = Mesh->GetPathName();
	TestFalse(TEXT("Missing palette asset is rejected"), Model.PlaceMesh(nullptr, FIntPoint(0, 0), 0, 0, 1, Message));
	TestFalse(TEXT("Zero scale is rejected"), Model.PlaceMesh(Mesh, FIntPoint(0, 0), 0, 0, 0, Message));
	TestFalse(TEXT("NaN height is rejected"), Model.PlaceMesh(Mesh, FIntPoint(0, 0), std::numeric_limits<double>::quiet_NaN(), 0, 1, Message));
	TestFalse(TEXT("Cell without Landscape cannot receive a mesh"), Model.PlaceMesh(Mesh, FIntPoint(3, 3), 0, 0, 1, Message));
	if (!TestTrue(TEXT("Place reused mesh on real Landscape"), Model.PlaceMesh(Mesh, FIntPoint(0, 0), 125, 45, 1.5, Message)))
	{
		AddError(Message);
		return false;
	}
	AStaticMeshActor* Placed = Cast<AStaticMeshActor>(GEditor->GetSelectedActors()->GetTop<AActor>());
	if (!TestNotNull(TEXT("New mesh selected for Details editing"), Placed))
	{
		return false;
	}
	const FName PlacedName = Placed->GetFName();
	const FGuid PlacedGuid = Placed->GetActorGuid();
	const FVector PlacedLocation = Placed->GetActorLocation();
	const double ExpectedZ = 125.0 - Mesh->GetBoundingBox().Min.Z * 1.5;
	TestTrue(TEXT("Placed XY is selected center and mesh bottom is offset above Landscape"), PlacedLocation.Equals(FVector(12800, 12800, ExpectedZ), 1.0));
	TestTrue(TEXT("Placed scale is retained"), Placed->GetActorScale3D().Equals(FVector(1.5), 0.001));
	TestTrue(TEXT("Placed yaw is retained"), FMath::IsNearlyEqual(Placed->GetActorRotation().Yaw, 45.0, 0.01));
	TestTrue(TEXT("Placed actor participates in OFPA"), Placed->IsPackageExternal());
	if (!TestTrue(TEXT("Placed external package belongs to the scratch fixture"),
		Placed->GetPackage()->GetName().Contains(TEXT("/__MapMakerTests/"), ESearchCase::IgnoreCase) &&
		Placed->GetPackage()->GetName().Contains(FPackageName::GetShortName(FixturePackage), ESearchCase::IgnoreCase)))
	{
		return false;
	}
	TestTrue(TEXT("Placement is undoable"), GEditor->UndoTransaction());
	AStaticMeshActor* AfterUndo = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->GetActorGuid() == PlacedGuid) { AfterUndo = *It; }
	}
	TestNull(TEXT("Undo removes the placed mesh"), AfterUndo);
	TestTrue(TEXT("Placement is redoable"), GEditor->RedoTransaction());
	Placed = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->GetActorGuid() == PlacedGuid) { Placed = *It; }
	}
	if (!TestNotNull(TEXT("Redo restores the placed mesh"), Placed)) { return false; }
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Placed, true, true);
	Placed->SetLockLocation(true);
	TestFalse(TEXT("Actor Lock Movement rejects the entire group move"), Model.MoveSelectionToCell(FIntPoint(1, 0), Message));
	TestTrue(TEXT("Rejected locked move leaves mesh unchanged"), Placed->GetActorLocation().Equals(PlacedLocation, 0.01));
	Placed->SetLockLocation(false);
	AActor* Protected = World->SpawnActor<AActor>();
	Protected->SetActorLabel(TEXT("FixtureProtectedGameplay"));
	GEditor->SelectActor(Protected, true, true);
	TestFalse(TEXT("Mixed protected selection rejects the entire group move"), Model.MoveSelectionToCell(FIntPoint(1, 0), Message));
	TestTrue(TEXT("Rejected mixed move leaves mesh unchanged"), Placed->GetActorLocation().Equals(PlacedLocation, 0.01));
	GEditor->SelectActor(Protected, false, true);
	World->DestroyActor(Protected);
	TestTrue(TEXT("Move environment selection to adjacent cell"), Model.MoveSelectionToCell(FIntPoint(1, 0), Message));
	const FVector MovedLocation = Placed->GetActorLocation();
	TestTrue(TEXT("Group move preserves height and uses target center"), MovedLocation.Equals(FVector(38400, 12800, PlacedLocation.Z), 1.0));
	TestTrue(TEXT("Move is undoable"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo movement restores original location"), Placed->GetActorLocation().Equals(PlacedLocation, 0.01));
	TestTrue(TEXT("Move is redoable"), GEditor->RedoTransaction());
	TestTrue(TEXT("Redo movement restores target location"), Placed->GetActorLocation().Equals(MovedLocation, 0.01));
	TestTrue(TEXT("Select a cell's environment actors"), Model.SelectCell(FIntPoint(1, 0), TEXT(""), Message));
	TestEqual(TEXT("Only placed environment mesh selected"), GEditor->GetSelectedActorCount(), 1);
	TestTrue(TEXT("Label filter selection completes"), Model.SelectCell(FIntPoint(1, 0), TEXT("NoSuchFixtureLabel"), Message));
	TestEqual(TEXT("Unmatched filter selects no actors"), GEditor->GetSelectedActorCount(), 0);
	if (!TestTrue(TEXT("Save current fixture map and OFPA actor changes"), Model.SaveMap(Message)))
	{
		AddError(Message);
		return false;
	}
	const FString SavedActorPackage = Placed->GetPackage()->GetName();
	TestTrue(TEXT("OFPA actor package exists on disk"), FPackageName::DoesPackageExist(SavedActorPackage));
	Model.ReleaseCells();
	Partition->ReleaseEditorLoaderAdapter(UserLoader);
	UserLoader = nullptr;
	Partition = nullptr;
	GEditor->NewMap(false);
	World = UEditorLoadingAndSavingUtils::LoadMap(FixtureFilename);
	if (!TestNotNull(TEXT("Reload isolated saved fixture"), World)) { return false; }
	TestTrue(TEXT("Load saved actor's work cell after independent reload"), Model.LoadCell(FIntPoint(1, 0), Message));
	Placed = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->GetActorGuid() == PlacedGuid) { Placed = *It; }
	}
	if (!TestNotNull(TEXT("OFPA placed actor survives map reload"), Placed)) { return false; }
	TestTrue(TEXT("Saved movement survives map reload"), Placed->GetActorLocation().Equals(MovedLocation, 0.01));
	TestTrue(TEXT("Saved scale survives map reload"), Placed->GetActorScale3D().Equals(FVector(1.5), 0.001));
	const UStaticMesh* ReloadedMesh = Placed->GetStaticMeshComponent()->GetStaticMesh();
	if (TestNotNull(TEXT("Saved static mesh reference is resolved after map reload"), ReloadedMesh))
	{
		TestEqual(TEXT("Saved static mesh asset path survives map reload"), ReloadedMesh->GetPathName(), MeshAssetPath);
	}
	TestEqual(TEXT("Stable actor name survives map reload"), Placed->GetFName(), PlacedName);
	Model.ReleaseCells();
	return true;
}

#endif
