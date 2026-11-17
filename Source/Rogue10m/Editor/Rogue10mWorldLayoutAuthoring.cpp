// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mWorldLayoutAuthoring.h"

#if WITH_EDITOR
#include "Core/Rogue10m.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

namespace Rogue10mWorldLayoutAuthoring
{
	constexpr int32 SamplesPerAxis = 1009;
	constexpr int32 QuadsPerSection = 63;
	constexpr int32 ComponentsPerAxis = 16;
	constexpr int32 GridComponentsPerAxis = 4;
	constexpr int32 ExpectedComponentCount = ComponentsPerAxis * ComponentsPerAxis;
	constexpr int32 ExpectedProxyCount = 16;
	constexpr int32 HeightmapByteCount = SamplesPerAxis * SamplesPerAxis * 2;
	constexpr double WorldSizeCm = 180000.0;
	constexpr double HeightScale = 256.0;
	const TCHAR* const ExpectedProjectDirectory = TEXT("D:/Project/Rogue10m");
	const TCHAR* const ExpectedMapPackage = TEXT("/Game/Rogue10m/Maps/Production/WorldLayout20261112");
	const TCHAR* const ExpectedLayerInfoPackage = TEXT("/Game/Stylized_Village/Assets/Terrain/TerrainLayerWeights/AutoLayer_LayerInfo");
	const FName OwnedTag(TEXT("Rogue10m.WorldLayout20261112"));
	const FName TerrainFolder(TEXT("WorldLayout20261112/Terrain"));

	FString SerializeResult(const TSharedRef<FJsonObject>& Result)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Result, Writer);
		return Json;
	}

	FString Failure(const FString& Error)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), Error);
		Result->SetStringField(TEXT("map_package"), ExpectedMapPackage);
		UE_LOG(LogRogue10m, Warning, TEXT("World-layout candidate authoring refused: %s"), *Error);
		return SerializeResult(Result);
	}

	FString CanonicalPath(const FString& Path)
	{
		FString Canonical = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Canonical);
		FPaths::CollapseRelativeDirectories(Canonical);
		while (Canonical.EndsWith(TEXT("/")))
		{
			Canonical.LeftChopInline(1);
		}
		return Canonical;
	}

	bool ResolveEditorWorld(UWorld*& OutWorld, FString& OutError)
	{
		const FString ProjectDirectory = CanonicalPath(FPaths::ProjectDir());
		if (!ProjectDirectory.Equals(ExpectedProjectDirectory, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("Expected project %s, found %s."), ExpectedProjectDirectory, *ProjectDirectory);
			return false;
		}
		if (!GEditor || GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
		{
			OutError = TEXT("An editor world is required, and PIE must be stopped.");
			return false;
		}
		OutWorld = GEditor->GetEditorWorldContext().World();
		if (!OutWorld || OutWorld->WorldType != EWorldType::Editor)
		{
			OutError = TEXT("No valid editor world is loaded.");
			return false;
		}
		if (OutWorld->GetOutermost()->GetName() != ExpectedMapPackage)
		{
			OutError = FString::Printf(TEXT("Only %s may be authored; current map is %s."), ExpectedMapPackage, *OutWorld->GetOutermost()->GetName());
			return false;
		}
		if (!OutWorld->GetWorldPartition())
		{
			OutError = TEXT("WorldLayout20261112 must already use World Partition before terrain authoring.");
			return false;
		}
		return true;
	}

	// Read-only gate: the caller must preserve unrelated unsaved maps before candidate mutation.
	bool RejectUnrelatedDirtyMaps(const UWorld* World, FString& OutError)
	{
		TArray<UPackage*> DirtyMaps;
		UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
		for (const UPackage* Package : DirtyMaps)
		{
			// OFPA Landscape proxies belong to this candidate even though they
			// appear separately in GetDirtyMapPackages immediately after Import.
			const bool bCandidateExternalPackage = Package &&
				(Package->GetName().StartsWith(TEXT("/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayout20261112/")) ||
				 Package->GetName().StartsWith(TEXT("/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayout20261112/")));
			if (Package && Package != World->GetOutermost() && !bCandidateExternalPackage)
			{
				OutError = FString::Printf(TEXT("Unrelated map package is dirty: %s. Nothing was saved or discarded."), *Package->GetName());
				return false;
			}
		}
		return true;
	}
	TSharedRef<FJsonObject> VectorObject(const FVector& Vector)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("x"), Vector.X);
		Result->SetNumberField(TEXT("y"), Vector.Y);
		Result->SetNumberField(TEXT("z"), Vector.Z);
		return Result;
	}

	TSharedRef<FJsonObject> TerrainFacts(UWorld* World)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		Result->SetStringField(TEXT("map_package"), World->GetOutermost()->GetName());
		Result->SetBoolField(TEXT("world_partition"), WorldPartition != nullptr);
		Result->SetBoolField(TEXT("streaming_enabled"), WorldPartition && WorldPartition->IsStreamingEnabled());
		Result->SetStringField(TEXT("runtime_hash_class"), WorldPartition && WorldPartition->RuntimeHash ? WorldPartition->RuntimeHash->GetClass()->GetPathName() : FString());
		Result->SetStringField(TEXT("runtime_hash_path"), WorldPartition && WorldPartition->RuntimeHash ? WorldPartition->RuntimeHash->GetPathName() : FString());
		Result->SetNumberField(TEXT("samples_per_axis"), SamplesPerAxis);
		Result->SetNumberField(TEXT("quads_per_section"), QuadsPerSection);
		Result->SetNumberField(TEXT("components_per_axis"), ComponentsPerAxis);
		Result->SetNumberField(TEXT("proxy_grid_components_per_axis"), GridComponentsPerAxis);

		int32 MainLandscapeCount = 0;
		int32 StreamingProxyCount = 0;
		int32 ComponentCount = 0;
		int32 CollisionComponentCount = 0;
		int32 SpatialProxyCount = 0;
		int32 OwnedProxyCount = 0;
		int32 LoadedProxyBoundsCount = 0;
		FBox ObservedLoadedBounds(EForceInit::ForceInit);
		ALandscape* MainLandscape = nullptr;
		TArray<TSharedPtr<FJsonValue>> Proxies;
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			ALandscapeProxy* Proxy = *It;
			ComponentCount += Proxy->LandscapeComponents.Num();
			CollisionComponentCount += Proxy->CollisionComponents.Num();
			const FBox ProxyBounds = Proxy->GetProxyBounds();
			if (ProxyBounds.IsValid && ProxyBounds.GetSize() != FVector::ZeroVector)
			{
				ObservedLoadedBounds += ProxyBounds;
				++LoadedProxyBoundsCount;
			}
			if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
			{
				++MainLandscapeCount;
				MainLandscape = Landscape;
			}
			else if (Proxy->IsA<ALandscapeStreamingProxy>())
			{
				++StreamingProxyCount;
				SpatialProxyCount += Proxy->GetIsSpatiallyLoaded() ? 1 : 0;
				OwnedProxyCount += Proxy->Tags.Contains(OwnedTag) ? 1 : 0;
				const TSharedRef<FJsonObject> ProxyFacts = MakeShared<FJsonObject>();
				ProxyFacts->SetStringField(TEXT("actor_path"), Proxy->GetPathName());
				ProxyFacts->SetNumberField(TEXT("component_count"), Proxy->LandscapeComponents.Num());
				ProxyFacts->SetNumberField(TEXT("collision_component_count"), Proxy->CollisionComponents.Num());
				ProxyFacts->SetBoolField(TEXT("spatially_loaded"), Proxy->GetIsSpatiallyLoaded());
				ProxyFacts->SetBoolField(TEXT("owned"), Proxy->Tags.Contains(OwnedTag));
				Proxies.Add(MakeShared<FJsonValueObject>(ProxyFacts));
			}
		}
		Result->SetNumberField(TEXT("landscape_actor_count"), MainLandscapeCount);
		Result->SetNumberField(TEXT("streaming_proxy_count"), StreamingProxyCount);
		Result->SetNumberField(TEXT("component_count"), ComponentCount);
		Result->SetNumberField(TEXT("collision_component_count"), CollisionComponentCount);
		Result->SetNumberField(TEXT("spatial_proxy_count"), SpatialProxyCount);
		Result->SetNumberField(TEXT("owned_proxy_count"), OwnedProxyCount);
		Result->SetNumberField(TEXT("loaded_proxy_bounds_count"), LoadedProxyBoundsCount);
		Result->SetArrayField(TEXT("streaming_proxies"), Proxies);

		bool bValidBounds = false;
		if (MainLandscape)
		{
			Result->SetStringField(TEXT("landscape_actor_path"), MainLandscape->GetPathName());
			Result->SetBoolField(TEXT("main_landscape_spatially_loaded"), MainLandscape->GetIsSpatiallyLoaded());
			Result->SetBoolField(TEXT("main_landscape_owned"), MainLandscape->Tags.Contains(OwnedTag));
			Result->SetObjectField(TEXT("actor_location"), VectorObject(MainLandscape->GetActorLocation()));
			Result->SetObjectField(TEXT("actor_scale"), VectorObject(MainLandscape->GetActorScale3D()));
			Result->SetStringField(TEXT("material_path"), MainLandscape->LandscapeMaterial ? MainLandscape->LandscapeMaterial->GetPathName() : FString());
			Result->SetNumberField(TEXT("edit_layer_count"), MainLandscape->GetEditLayersConst().Num());
			if (ULandscapeInfo* Info = MainLandscape->GetLandscapeInfo())
			{
				// In WP, GetCompleteBounds walks actor descriptors. Newly imported, unsaved proxies may
				// not have descriptors yet. Accumulate actual loaded component/collision bounds as well.
				const FBox CompleteBounds = Info->GetCompleteBounds();
				const FBox LoadedBounds = Info->GetLoadedBounds();
				FBox Bounds(EForceInit::ForceInit);
				if (CompleteBounds.IsValid)
				{
					Bounds += CompleteBounds;
				}
				if (LoadedBounds.IsValid)
				{
					Bounds += LoadedBounds;
				}
				if (ObservedLoadedBounds.IsValid)
				{
					Bounds += ObservedLoadedBounds;
				}
				Result->SetBoolField(TEXT("complete_bounds_valid"), CompleteBounds.IsValid != 0);
				Result->SetBoolField(TEXT("loaded_bounds_valid"), LoadedBounds.IsValid != 0);
				Result->SetBoolField(TEXT("observed_loaded_bounds_valid"), ObservedLoadedBounds.IsValid != 0);
				Result->SetStringField(TEXT("bounds_source"), CompleteBounds.IsValid ? TEXT("actor_descriptors_and_loaded_components") : TEXT("loaded_components_and_collision"));
				if (Bounds.IsValid)
				{
					Result->SetObjectField(TEXT("bounds_min"), VectorObject(Bounds.Min));
					Result->SetObjectField(TEXT("bounds_max"), VectorObject(Bounds.Max));
					Result->SetObjectField(TEXT("bounds_size"), VectorObject(Bounds.GetSize()));
					bValidBounds = FMath::IsNearlyEqual(Bounds.GetSize().X, WorldSizeCm, 1.0) &&
						FMath::IsNearlyEqual(Bounds.GetSize().Y, WorldSizeCm, 1.0) &&
						FMath::IsNearlyEqual(Bounds.Min.X, -WorldSizeCm / 2.0, 1.0) &&
						FMath::IsNearlyEqual(Bounds.Min.Y, -WorldSizeCm / 2.0, 1.0) &&
						FMath::IsNearlyEqual(Bounds.Max.X, WorldSizeCm / 2.0, 1.0) &&
						FMath::IsNearlyEqual(Bounds.Max.Y, WorldSizeCm / 2.0, 1.0);
				}
			}
		}
		const FVector ExpectedLocation(-WorldSizeCm / 2.0, -WorldSizeCm / 2.0, 0.0);
		const FVector ExpectedScale(WorldSizeCm / (SamplesPerAxis - 1), WorldSizeCm / (SamplesPerAxis - 1), HeightScale);
		const bool bValidTransform = MainLandscape && MainLandscape->GetActorLocation().Equals(ExpectedLocation, 0.01) &&
			MainLandscape->GetActorScale3D().Equals(ExpectedScale, 0.0001) && MainLandscape->GetActorRotation().IsNearlyZero(0.001);
		const bool bHasEditLayer = MainLandscape && !MainLandscape->GetEditLayersConst().IsEmpty();
		Result->SetBoolField(TEXT("transform_matches_contract"), bValidTransform);
		Result->SetBoolField(TEXT("has_edit_layer"), bHasEditLayer);
		const bool bValid = MainLandscapeCount == 1 && StreamingProxyCount == ExpectedProxyCount &&
			ComponentCount == ExpectedComponentCount && CollisionComponentCount == ExpectedComponentCount &&
			SpatialProxyCount == ExpectedProxyCount && OwnedProxyCount == ExpectedProxyCount &&
			MainLandscape && !MainLandscape->GetIsSpatiallyLoaded() && MainLandscape->Tags.Contains(OwnedTag) &&
			WorldPartition && WorldPartition->IsStreamingEnabled() && bValidBounds && bValidTransform && bHasEditLayer;
		Result->SetBoolField(TEXT("success"), bValid);
		if (!bValid)
		{
			Result->SetStringField(TEXT("error"), TEXT("Loaded candidate terrain is not yet complete: expected exact origin/scale, 1.8 km bounds, edit layer, 256 collision components and 16 owned spatial proxies. After creation, preserve the actor and inspect again after editor updates/region loading; do not re-import."));
		}
		return Result;
	}
}
#endif

FString URogue10mWorldLayoutAuthoring::CreateLandscapeFromHeightmap(const FString& HeightmapFile, UMaterialInterface* Material, UObject* AutoLayerInfo)
{
#if WITH_EDITOR
	using namespace Rogue10mWorldLayoutAuthoring;
	UWorld* World = nullptr;
	FString Error;
	if (!ResolveEditorWorld(World, Error))
	{
		return Failure(Error);
	}
	if (!RejectUnrelatedDirtyMaps(World, Error))
	{
		return Failure(Error);
	}
	FString NormalizedInputPath = HeightmapFile;
	FPaths::NormalizeFilename(NormalizedInputPath);
	if (NormalizedInputPath.StartsWith(TEXT("../")) || NormalizedInputPath.EndsWith(TEXT("/..")) ||
		NormalizedInputPath.Contains(TEXT("/../")))
	{
		return Failure(TEXT("Parent traversal is not allowed in the heightmap path."));
	}
	const FString FullHeightmapPath = CanonicalPath(HeightmapFile);
	const FString ApprovedHeightmapPath = CanonicalPath(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Feature/doc/evidence/landscape-world-blockout-20261112/terrain-height.r16")));
	if (!FullHeightmapPath.Equals(ApprovedHeightmapPath, ESearchCase::IgnoreCase))
	{
		return Failure(TEXT("Only Feature/doc/evidence/landscape-world-blockout-20261112/terrain-height.r16 may be imported."));
	}
	if (!IsValid(Material))
	{
		return Failure(TEXT("A valid Landscape material is required."));
	}
	ULandscapeLayerInfoObject* LayerInfo = Cast<ULandscapeLayerInfoObject>(AutoLayerInfo);
	if (!IsValid(LayerInfo) || LayerInfo->GetOutermost()->GetName() != ExpectedLayerInfoPackage ||
		LayerInfo->GetLayerName() != FName(TEXT("Auto Layer")))
	{
		return Failure(TEXT("The supplied LayerInfo must be the approved Stylized_Village AutoLayer_LayerInfo asset with layer name 'Auto Layer'."));
	}
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		return Failure(TEXT("A Landscape or LandscapeStreamingProxy already exists. Existing terrain will not be replaced."));
	}
	bool bHasExistingTerrainDescriptor = false;
	FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(World->GetWorldPartition(), [&bHasExistingTerrainDescriptor](const FWorldPartitionActorDescInstance*)
	{
		bHasExistingTerrainDescriptor = true;
		return false;
	});
	if (bHasExistingTerrainDescriptor)
	{
		return Failure(TEXT("World Partition already contains a Landscape actor descriptor. Unloaded terrain will not be replaced."));
	}
	TArray<uint8> RawHeightData;
	if (!FFileHelper::LoadFileToArray(RawHeightData, *FullHeightmapPath) || RawHeightData.Num() != HeightmapByteCount)
	{
		return Failure(FString::Printf(TEXT("Heightmap must contain exactly %d little-endian bytes; loaded %d."), HeightmapByteCount, RawHeightData.Num()));
	}
	const FString HeightmapSha1 = FSHA1::HashBuffer(RawHeightData.GetData(), RawHeightData.Num()).ToString();
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(SamplesPerAxis * SamplesPerAxis);
	uint16 MinimumHeight = MAX_uint16;
	uint16 MaximumHeight = 0;
	for (int32 Index = 0; Index < HeightData.Num(); ++Index)
	{
		const uint16 Sample = static_cast<uint16>(RawHeightData[Index * 2]) | (static_cast<uint16>(RawHeightData[Index * 2 + 1]) << 8);
		HeightData[Index] = Sample;
		MinimumHeight = FMath::Min(MinimumHeight, Sample);
		MaximumHeight = FMath::Max(MaximumHeight, Sample);
	}
	ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>();
	if (!LandscapeSubsystem || !LandscapeSubsystem->IsGridBased())
	{
		return Failure(TEXT("World Partition Landscape actor partitioning is unavailable."));
	}

	// All guards above are read-only. Follow the same native Import and ChangeGridSize path as Epic's Landscape editor.
	World->Modify();
	World->GetWorldPartition()->SetEnableStreaming(true);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector(-WorldSizeCm / 2.0, -WorldSizeCm / 2.0, 0.0), FRotator::ZeroRotator);
	if (!Landscape)
	{
		return Failure(TEXT("Could not spawn the Landscape actor."));
	}
	Landscape->Modify();
	Landscape->Tags.AddUnique(OwnedTag);
	Landscape->SetActorLabel(TEXT("WL20261112_Landscape_1800m"));
	Landscape->SetFolderPath(TerrainFolder);
	Landscape->LandscapeMaterial = Material;
	Landscape->bAreNewLandscapeActorsSpatiallyLoaded = true;
	Landscape->SetIsSpatiallyLoaded(false);
	Landscape->SetActorRelativeScale3D(FVector(WorldSizeCm / (SamplesPerAxis - 1), WorldSizeCm / (SamplesPerAxis - 1), HeightScale));
	Landscape->StaticLightingLOD = 0;

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));
	FLandscapeImportLayerInfo ImportedLayer(LayerInfo->GetLayerName());
	ImportedLayer.LayerInfo = LayerInfo;
	ImportedLayer.LayerData.Init(255, SamplesPerAxis * SamplesPerAxis);
	TArray<FLandscapeImportLayerInfo> ImportedLayers;
	ImportedLayers.Add(MoveTemp(ImportedLayer));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialDataPerLayers;
	MaterialDataPerLayers.Add(FGuid(), MoveTemp(ImportedLayers));
	Landscape->Import(FGuid::NewGuid(), 0, 0, SamplesPerAxis - 1, SamplesPerAxis - 1, 1, QuadsPerSection,
		HeightDataPerLayers, *FullHeightmapPath, MaterialDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return Failure(TEXT("Landscape import did not create LandscapeInfo. The new actor is retained for inspection; packages were not saved."));
	}
	LandscapeInfo->UpdateLayerInfoMap(Landscape);
	LandscapeSubsystem->ChangeGridSize(LandscapeInfo, GridComponentsPerAxis);
	LandscapeInfo->ForEachLandscapeProxy([](ALandscapeProxy* Proxy)
	{
		Proxy->Modify();
		Proxy->Tags.AddUnique(OwnedTag);
		Proxy->SetFolderPath(TerrainFolder);
		if (Proxy->IsA<ALandscapeStreamingProxy>())
		{
			Proxy->SetIsSpatiallyLoaded(true);
		}
		Proxy->MarkPackageDirty();
		return true;
	});
	World->MarkPackageDirty();
	const TSharedRef<FJsonObject> Result = TerrainFacts(World);
	Result->SetBoolField(TEXT("creation_performed"), true);
	Result->SetStringField(TEXT("heightmap_file"), FullHeightmapPath);
	Result->SetStringField(TEXT("heightmap_sha1"), HeightmapSha1);
	Result->SetNumberField(TEXT("heightmap_bytes"), RawHeightData.Num());
	Result->SetStringField(TEXT("imported_layer_name"), LayerInfo->GetLayerName().ToString());
	Result->SetNumberField(TEXT("minimum_height_sample"), MinimumHeight);
	Result->SetNumberField(TEXT("maximum_height_sample"), MaximumHeight);
	Result->SetBoolField(TEXT("requires_save"), true);
	UE_LOG(LogRogue10m, Log, TEXT("Created WorldLayout20261112 Landscape from %s; component count=%d, proxy count=%d. Packages require explicit editor save."),
		*FullHeightmapPath, static_cast<int32>(Result->GetNumberField(TEXT("component_count"))), static_cast<int32>(Result->GetNumberField(TEXT("streaming_proxy_count"))));
	return Rogue10mWorldLayoutAuthoring::SerializeResult(Result);
#else
	return TEXT("{\"success\":false,\"error\":\"World-layout candidate authoring is available only in an editor build.\"}");
#endif
}

FString URogue10mWorldLayoutAuthoring::InspectTerrain()
{
#if WITH_EDITOR
	using namespace Rogue10mWorldLayoutAuthoring;
	UWorld* World = nullptr;
	FString Error;
	if (!ResolveEditorWorld(World, Error))
	{
		return Failure(Error);
	}
	const TSharedRef<FJsonObject> Result = TerrainFacts(World);
	Result->SetBoolField(TEXT("requires_save"), false);
	return Rogue10mWorldLayoutAuthoring::SerializeResult(Result);
#else
	return TEXT("{\"success\":false,\"error\":\"World-layout candidate inspection is available only in an editor build.\"}");
#endif
}

FString URogue10mWorldLayoutAuthoring::LoadEditorRegion()
{
#if WITH_EDITOR
	using namespace Rogue10mWorldLayoutAuthoring;
	UWorld* World = nullptr;
	FString Error;
	if (!ResolveEditorWorld(World, Error))
	{
		return Failure(Error);
	}
	if (!RejectUnrelatedDirtyMaps(World, Error))
	{
		return Failure(Error);
	}
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	// The full RAW height range at Z scale 256 is +/-65536 cm. Keep the editor load region
	// bounded to this map, with enough vertical margin for its Landscape and landmarks.
	const FBox Region(FVector(-91000.0, -91000.0, -100000.0), FVector(91000.0, 91000.0, 100000.0));
	bool bAlreadyLoaded = false;
	for (const FBox& LoadedRegion : WorldPartition->GetUserLoadedEditorRegions())
	{
		if (LoadedRegion.IsValid && LoadedRegion.IsInsideOrOn(Region))
		{
			bAlreadyLoaded = true;
			break;
		}
	}
	if (!bAlreadyLoaded)
	{
		// This is the same owned loader-adapter path used by the World Partition editor.
		// The World Partition instance retains the region until the editor map is unloaded.
		TArray<FBox> Regions;
		Regions.Add(Region);
		WorldPartition->LoadLastLoadedRegions(Regions);
	}
	bool bRegionLoaded = false;
	for (const FBox& LoadedRegion : WorldPartition->GetUserLoadedEditorRegions())
	{
		if (LoadedRegion.IsValid && LoadedRegion.IsInsideOrOn(Region))
		{
			bRegionLoaded = true;
			break;
		}
	}
	const TSharedRef<FJsonObject> Result = TerrainFacts(World);
	Result->SetBoolField(TEXT("editor_region_loaded"), bRegionLoaded);
	Result->SetBoolField(TEXT("editor_region_already_loaded"), bAlreadyLoaded);
	Result->SetObjectField(TEXT("editor_region_min"), VectorObject(Region.Min));
	Result->SetObjectField(TEXT("editor_region_max"), VectorObject(Region.Max));
	Result->SetBoolField(TEXT("requires_save"), false);
	UE_LOG(LogRogue10m, Log, TEXT("Loaded the bounded WorldLayout20261112 editor region; component count=%d, proxy count=%d."),
		static_cast<int32>(Result->GetNumberField(TEXT("component_count"))), static_cast<int32>(Result->GetNumberField(TEXT("streaming_proxy_count"))));
	return Rogue10mWorldLayoutAuthoring::SerializeResult(Result);
#else
	return TEXT("{\"success\":false,\"error\":\"World-layout candidate editor region loading is available only in an editor build.\"}");
#endif
}
