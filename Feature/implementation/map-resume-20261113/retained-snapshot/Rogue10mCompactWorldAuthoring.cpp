// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mCompactWorldAuthoring.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Containers/BitArray.h"
#include "Containers/StringConv.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Hash/Blake3.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEditLayer.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

namespace Rogue10mCompactWorldAuthoring
{
	const TCHAR* const ExpectedProjectDirectory = TEXT("D:/Project/Rogue10m");
	const TCHAR* const SourceMap = TEXT("/Game/Rogue10m/Maps/Production/WorldLayout20261112");
	const TCHAR* const CandidateMap = TEXT("/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A");
	constexpr int32 QuadsPerComponent = 63;
	constexpr int32 SourceComponentsPerAxis = 16;
	constexpr int32 SourceComponentCount = 256;
	constexpr int32 SourceMaximumSample = 1008;
	constexpr int32 KeepComponentMinX = 5;
	constexpr int32 KeepComponentMaxX = 13;
	constexpr int32 KeepComponentMinY = 10;
	constexpr int32 KeepComponentMaxY = 14;
	constexpr int32 KeepComponentCount = 45;
	constexpr int32 OutsideComponentCount = SourceComponentCount - KeepComponentCount;
	constexpr double SampleSpacingCm = 1250.0 / 7.0;
	const FVector ExpectedOrigin(-90000.0, -90000.0, 0.0);
	const FVector ExpectedScale(SampleSpacingCm, SampleSpacingCm, 256.0);

	struct FDirtySnapshot
	{
		TArray<FString> Maps;
		TArray<FString> Content;

		bool Equals(const FDirtySnapshot& Other) const
		{
			return Maps == Other.Maps && Content == Other.Content;
		}
	};

	struct FTerrainState
	{
		ALandscape* Main = nullptr;
		ULandscapeInfo* Info = nullptr;
		TArray<ALandscapeProxy*> Proxies;
		TSet<ULandscapeComponent*> Components;
		TSet<ULandscapeHeightfieldCollisionComponent*> Collisions;
		TSet<FGuid> LoadedProxyGuids;
		TSet<FIntPoint> SectionBases;
		int32 MainCount = 0;
		int32 StreamingProxyCount = 0;
		int32 ComponentEntries = 0;
		int32 CollisionEntries = 0;
		int32 DescriptorCount = 0;
		bool bInvalidComponentEntry = false;
		bool bInvalidCollisionEntry = false;
		bool bAllDescriptorsLoaded = true;
		bool bHasExtent = false;
		FIntRect Extent;
	};

	TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Strings)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Strings.Num());
		for (const FString& String : Strings)
		{
			Values.Add(MakeShared<FJsonValueString>(String));
		}
		return Values;
	}

	FDirtySnapshot CaptureDirtyPackages()
	{
		FDirtySnapshot Snapshot;
		if (!GEditor)
		{
			return Snapshot;
		}
		TArray<UPackage*> Maps;
		TArray<UPackage*> Content;
		UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Maps);
		UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
		for (const UPackage* Package : Maps)
		{
			if (Package)
			{
				Snapshot.Maps.AddUnique(Package->GetName());
			}
		}
		for (const UPackage* Package : Content)
		{
			if (Package)
			{
				Snapshot.Content.AddUnique(Package->GetName());
			}
		}
		Snapshot.Maps.Sort();
		Snapshot.Content.Sort();
		return Snapshot;
	}

	TSharedRef<FJsonObject> DirtyObject(const FDirtySnapshot& Snapshot)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetArrayField(TEXT("maps"), StringArray(Snapshot.Maps));
		Object->SetArrayField(TEXT("content"), StringArray(Snapshot.Content));
		return Object;
	}

	bool IsFiniteVector(const FVector& Vector)
	{
		return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
	}

	TSharedRef<FJsonObject> VectorObject(const FVector& Vector)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		const bool bFinite = IsFiniteVector(Vector);
		Object->SetBoolField(TEXT("finite"), bFinite);
		if (bFinite)
		{
			Object->SetNumberField(TEXT("x"), Vector.X);
			Object->SetNumberField(TEXT("y"), Vector.Y);
			Object->SetNumberField(TEXT("z"), Vector.Z);
		}
		return Object;
	}

	TSharedRef<FJsonObject> PointObject(const FIntPoint& Point)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), Point.X);
		Object->SetNumberField(TEXT("y"), Point.Y);
		return Object;
	}

	TSharedRef<FJsonObject> BoundsObject(const FBox& Bounds)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		const bool bValid = Bounds.IsValid && IsFiniteVector(Bounds.Min) && IsFiniteVector(Bounds.Max);
		Object->SetBoolField(TEXT("valid"), bValid);
		if (bValid)
		{
			Object->SetObjectField(TEXT("min"), VectorObject(Bounds.Min));
			Object->SetObjectField(TEXT("max"), VectorObject(Bounds.Max));
			Object->SetObjectField(TEXT("size"), VectorObject(Bounds.GetSize()));
		}
		return Object;
	}

	TSharedRef<FJsonObject> NewResult(const TCHAR* Operation, const TCHAR* ExpectedMap)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("operation"), Operation);
		Result->SetStringField(TEXT("expected_map_package"), ExpectedMap);
		Result->SetStringField(TEXT("map_package"), FString());
		Result->SetBoolField(TEXT("success"), false);
		Result->SetBoolField(TEXT("selection_performed"), false);
		Result->SetBoolField(TEXT("crop_performed"), false);
		Result->SetBoolField(TEXT("packages_saved"), false);
		Result->SetBoolField(TEXT("requires_save"), false);
		Result->SetBoolField(TEXT("persistent_terrain_data_written"), false);
		Result->SetStringField(TEXT("validation_boundary"),
			TEXT("Inspection/editor selection only. Dirty-set equality does not prove package bytes, latest editing preservation, crop completion, or runtime validation."));
		return Result;
	}

	FString Finish(const TSharedRef<FJsonObject>& Result, const FDirtySnapshot& Before, bool bSuccess, FString Error = FString())
	{
		const FDirtySnapshot After = CaptureDirtyPackages();
		const bool bDirtyUnchanged = Before.Equals(After);
		Result->SetObjectField(TEXT("dirty_before"), DirtyObject(Before));
		Result->SetObjectField(TEXT("dirty_after"), DirtyObject(After));
		Result->SetBoolField(TEXT("dirty_package_names_unchanged"), bDirtyUnchanged);
		if (bSuccess && !bDirtyUnchanged)
		{
			bSuccess = false;
			Error = TEXT("Dirty package names changed during this operation. Nothing was saved; inspect the before/after sets.");
		}
		Result->SetBoolField(TEXT("success"), bSuccess);
		if (!Error.IsEmpty())
		{
			Result->SetStringField(TEXT("error"), Error);
		}
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Result, Writer);
		return Json;
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

	bool ResolveEditorWorld(const TCHAR* ExpectedMap, UWorld*& OutWorld, const TSharedRef<FJsonObject>& Result, FString& OutError)
	{
		OutWorld = nullptr;
		if (!CanonicalPath(FPaths::ProjectDir()).Equals(ExpectedProjectDirectory, ESearchCase::IgnoreCase))
		{
			OutError = TEXT("This helper is restricted to D:/Project/Rogue10m.");
			return false;
		}
		if (!GEditor || GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
		{
			OutError = TEXT("A valid editor world with PIE stopped is required.");
			return false;
		}
		OutWorld = GEditor->GetEditorWorldContext().World();
		if (!OutWorld || OutWorld->WorldType != EWorldType::Editor)
		{
			OutError = TEXT("No valid editor world is loaded.");
			return false;
		}
		Result->SetStringField(TEXT("map_package"), OutWorld->GetOutermost()->GetName());
		if (OutWorld->GetOutermost()->GetName() != ExpectedMap)
		{
			OutError = FString::Printf(TEXT("Only %s is accepted. No map was switched."), ExpectedMap);
			return false;
		}
		if (!OutWorld->GetWorldPartition())
		{
			OutError = TEXT("The exact requested world must already use World Partition.");
			return false;
		}
		return true;
	}

	bool IsPackageInWorld(const UPackage* Package, const FString& Map)
	{
		if (!Package)
		{
			return false;
		}
		const FString Name = Package->GetName();
		const FString Tail = Map.RightChop(6); // Exact /Game/ package prefix was checked by ResolveEditorWorld.
		return Name == Map ||
			Name.StartsWith(TEXT("/Game/__ExternalActors__/") + Tail + TEXT("/")) ||
			Name.StartsWith(TEXT("/Game/__ExternalObjects__/") + Tail + TEXT("/"));
	}

	bool IsKeptBase(const FIntPoint& Base)
	{
		return Base.X >= KeepComponentMinX * QuadsPerComponent && Base.X < (KeepComponentMaxX + 1) * QuadsPerComponent &&
			Base.Y >= KeepComponentMinY * QuadsPerComponent && Base.Y < (KeepComponentMaxY + 1) * QuadsPerComponent;
	}

	void CollectTerrain(UWorld* World, const TSharedRef<FJsonObject>& Result, FTerrainState& State)
	{
		UWorldPartition* Partition = World->GetWorldPartition();
		Result->SetBoolField(TEXT("world_partition"), Partition != nullptr);
		Result->SetBoolField(TEXT("streaming_enabled"), Partition && Partition->IsStreamingEnabled());
		Result->SetStringField(TEXT("runtime_hash_class"), Partition && Partition->RuntimeHash ? Partition->RuntimeHash->GetClass()->GetPathName() : FString());
		TArray<TSharedPtr<FJsonValue>> ProxyRows;
		TArray<TSharedPtr<FJsonValue>> ComponentRows;
		FBox ObservedBounds(EForceInit::ForceInit);
		int32 NonemptyStreamingProxies = 0;
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			ALandscapeProxy* Proxy = *It;
			State.Proxies.Add(Proxy);
			State.LoadedProxyGuids.Add(Proxy->GetActorGuid());
			if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
			{
				++State.MainCount;
				State.Main = Landscape;
			}
			else if (Proxy->IsA<ALandscapeStreamingProxy>())
			{
				++State.StreamingProxyCount;
				NonemptyStreamingProxies += Proxy->LandscapeComponents.IsEmpty() ? 0 : 1;
			}
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("path"), Proxy->GetPathName());
			Row->SetStringField(TEXT("class"), Proxy->GetClass()->GetPathName());
			Row->SetStringField(TEXT("package"), Proxy->GetPackage()->GetName());
			Row->SetStringField(TEXT("actor_guid"), Proxy->GetActorGuid().ToString());
			Row->SetStringField(TEXT("landscape_guid"), Proxy->GetLandscapeGuid().ToString());
			Row->SetBoolField(TEXT("spatially_loaded"), Proxy->GetIsSpatiallyLoaded());
			Row->SetNumberField(TEXT("component_count"), Proxy->LandscapeComponents.Num());
			Row->SetNumberField(TEXT("collision_component_count"), Proxy->CollisionComponents.Num());
			Row->SetObjectField(TEXT("origin_cm"), VectorObject(Proxy->GetActorLocation()));
			const FBox ProxyBounds = Proxy->GetProxyBounds();
			Row->SetObjectField(TEXT("bounds"), BoundsObject(ProxyBounds));
			if (ProxyBounds.IsValid && ProxyBounds.GetSize() != FVector::ZeroVector)
			{
				ObservedBounds += ProxyBounds;
			}
			ProxyRows.Add(MakeShared<FJsonValueObject>(Row));
			State.ComponentEntries += Proxy->LandscapeComponents.Num();
			State.CollisionEntries += Proxy->CollisionComponents.Num();
			for (ULandscapeComponent* Component : Proxy->LandscapeComponents)
			{
				if (!IsValid(Component))
				{
					State.bInvalidComponentEntry = true;
					continue;
				}
				State.Components.Add(Component);
				const FIntPoint Base = Component->GetSectionBase();
				State.SectionBases.Add(Base);
				const TSharedRef<FJsonObject> ComponentRow = MakeShared<FJsonObject>();
				ComponentRow->SetStringField(TEXT("path"), Component->GetPathName());
				ComponentRow->SetStringField(TEXT("proxy"), Proxy->GetPathName());
				ComponentRow->SetObjectField(TEXT("section_base"), PointObject(Base));
				ComponentRow->SetNumberField(TEXT("quads"), Component->ComponentSizeQuads);
				ComponentRow->SetNumberField(TEXT("subsections"), Component->NumSubsections);
				ComponentRow->SetNumberField(TEXT("subsection_quads"), Component->SubsectionSizeQuads);
				ComponentRow->SetBoolField(TEXT("registered"), Component->IsRegistered());
				ComponentRow->SetBoolField(TEXT("keep_in_9_by_5"), IsKeptBase(Base));
				ComponentRow->SetBoolField(TEXT("has_collision"), IsValid(Component->GetCollisionComponent()));
				ComponentRows.Add(MakeShared<FJsonValueObject>(ComponentRow));
			}
			for (ULandscapeHeightfieldCollisionComponent* Collision : Proxy->CollisionComponents)
			{
				if (IsValid(Collision))
				{
					State.Collisions.Add(Collision);
				}
				else
				{
					State.bInvalidCollisionEntry = true;
				}
			}
		}
		TArray<TSharedPtr<FJsonValue>> Descriptors;
		FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(Partition, [&State, &Descriptors](const FWorldPartitionActorDescInstance* Descriptor)
		{
			++State.DescriptorCount;
			const bool bLoaded = State.LoadedProxyGuids.Contains(Descriptor->GetGuid());
			State.bAllDescriptorsLoaded &= bLoaded;
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("actor_guid"), Descriptor->GetGuid().ToString());
			Row->SetStringField(TEXT("actor_path"), Descriptor->GetActorSoftPath().ToString());
			Row->SetStringField(TEXT("package"), Descriptor->GetActorPackage().ToString());
			Row->SetBoolField(TEXT("loaded"), bLoaded);
			Descriptors.Add(MakeShared<FJsonValueObject>(Row));
			return true;
		});
		Result->SetArrayField(TEXT("proxies"), ProxyRows);
		Result->SetArrayField(TEXT("components"), ComponentRows);
		Result->SetArrayField(TEXT("landscape_descriptors"), Descriptors);
		Result->SetNumberField(TEXT("main_landscape_count"), State.MainCount);
		Result->SetNumberField(TEXT("streaming_proxy_count"), State.StreamingProxyCount);
		Result->SetNumberField(TEXT("nonempty_streaming_proxy_count"), NonemptyStreamingProxies);
		Result->SetNumberField(TEXT("empty_streaming_proxy_count"), State.StreamingProxyCount - NonemptyStreamingProxies);
		Result->SetNumberField(TEXT("component_count"), State.ComponentEntries);
		Result->SetNumberField(TEXT("collision_component_count"), State.CollisionEntries);
		Result->SetNumberField(TEXT("unique_component_count"), State.Components.Num());
		Result->SetNumberField(TEXT("unique_collision_component_count"), State.Collisions.Num());
		Result->SetNumberField(TEXT("landscape_descriptor_count"), State.DescriptorCount);
		Result->SetBoolField(TEXT("all_landscape_descriptors_loaded"), State.bAllDescriptorsLoaded);
		Result->SetObjectField(TEXT("observed_loaded_bounds"), BoundsObject(ObservedBounds));
		if (State.MainCount != 1 || !State.Main)
		{
			return;
		}
		State.Info = State.Main->GetLandscapeInfo();
		Result->SetStringField(TEXT("landscape_actor_path"), State.Main->GetPathName());
		Result->SetStringField(TEXT("landscape_guid"), State.Main->GetLandscapeGuid().ToString());
		Result->SetObjectField(TEXT("actor_origin_cm"), VectorObject(State.Main->GetActorLocation()));
		Result->SetObjectField(TEXT("actor_scale"), VectorObject(State.Main->GetActorScale3D()));
		const FRotator Rotation = State.Main->GetActorRotation();
		Result->SetObjectField(TEXT("actor_rotation_pitch_yaw_roll"), VectorObject(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
		Result->SetStringField(TEXT("material_path"), State.Main->LandscapeMaterial ? State.Main->LandscapeMaterial->GetPathName() : FString());
		TArray<TSharedPtr<FJsonValue>> EditLayers;
		const TArray<const ULandscapeEditLayerBase*> Layers = State.Main->GetEditLayersConst();
		for (int32 Index = 0; Index < Layers.Num(); ++Index)
		{
			const ULandscapeEditLayerBase* Layer = Layers[Index];
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("index"), Index);
			Row->SetBoolField(TEXT("valid"), IsValid(Layer));
			if (IsValid(Layer))
			{
				Row->SetStringField(TEXT("path"), Layer->GetPathName());
				Row->SetStringField(TEXT("class"), Layer->GetClass()->GetPathName());
				Row->SetStringField(TEXT("name"), Layer->GetName().ToString());
				Row->SetStringField(TEXT("guid"), Layer->GetGuid().ToString());
				Row->SetBoolField(TEXT("visible"), Layer->IsVisible());
				Row->SetBoolField(TEXT("locked"), Layer->IsLocked());
				Row->SetBoolField(TEXT("persistent_textures"), Layer->NeedsPersistentTextures());
				Row->SetBoolField(TEXT("editing_tools"), Layer->SupportsEditingTools());
				Row->SetNumberField(TEXT("height_alpha"), Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap));
				Row->SetNumberField(TEXT("weight_alpha"), Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap));
			}
			EditLayers.Add(MakeShared<FJsonValueObject>(Row));
		}
		Result->SetNumberField(TEXT("edit_layer_count"), Layers.Num());
		Result->SetArrayField(TEXT("edit_layers"), EditLayers);
		if (!State.Info)
		{
			return;
		}
		Result->SetStringField(TEXT("landscape_info_path"), State.Info->GetPathName());
		const int32 SelectedCount = State.Info->GetSelectedComponents().Num();
		Result->SetNumberField(TEXT("selected_component_count"), SelectedCount);
		Result->SetNumberField(TEXT("selected_component_count_before"), SelectedCount);
		Result->SetNumberField(TEXT("selected_component_count_after"), SelectedCount);
		const TSharedRef<FJsonObject> Layout = MakeShared<FJsonObject>();
		Layout->SetNumberField(TEXT("component_size_quads"), State.Info->ComponentSizeQuads);
		Layout->SetNumberField(TEXT("subsection_size_quads"), State.Info->SubsectionSizeQuads);
		Layout->SetNumberField(TEXT("subsections_per_component_axis"), State.Info->ComponentNumSubsections);
		Layout->SetObjectField(TEXT("observed_draw_scale"), VectorObject(State.Info->DrawScale));
		Result->SetObjectField(TEXT("layout"), Layout);
		Result->SetBoolField(TEXT("all_components_registered"), State.Info->AreAllComponentsRegistered());
		State.bHasExtent = State.Info->GetLandscapeExtent(State.Extent);
		const TSharedRef<FJsonObject> Extent = MakeShared<FJsonObject>();
		Extent->SetBoolField(TEXT("valid"), State.bHasExtent);
		Extent->SetStringField(TEXT("units"), TEXT("inclusive_landscape_sample_coordinates"));
		if (State.bHasExtent)
		{
			Extent->SetObjectField(TEXT("min"), PointObject(State.Extent.Min));
			Extent->SetObjectField(TEXT("max"), PointObject(State.Extent.Max));
			Extent->SetNumberField(TEXT("samples_x"), State.Extent.Width() + 1);
			Extent->SetNumberField(TEXT("samples_y"), State.Extent.Height() + 1);
		}
		Result->SetObjectField(TEXT("landscape_extent"), Extent);
		Result->SetObjectField(TEXT("complete_bounds"), BoundsObject(State.Info->GetCompleteBounds()));
		Result->SetObjectField(TEXT("loaded_bounds"), BoundsObject(State.Info->GetLoadedBounds()));
		TArray<TSharedPtr<FJsonValue>> PaintLayers;
		for (const FLandscapeInfoLayerSettings& Settings : State.Info->Layers)
		{
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Settings.GetLayerName().ToString());
			Row->SetStringField(TEXT("layer_info_path"), Settings.LayerInfoObj ? Settings.LayerInfoObj->GetPathName() : FString());
			Row->SetStringField(TEXT("layer_info_class"), Settings.LayerInfoObj ? Settings.LayerInfoObj->GetClass()->GetPathName() : FString());
			PaintLayers.Add(MakeShared<FJsonValueObject>(Row));
		}
		Result->SetArrayField(TEXT("paint_layers"), PaintLayers);
	}

	bool ValidateOriginalLayout(UWorld* World, const FTerrainState& State, FString& OutError)
	{
		if (State.MainCount != 1 || !State.Main || !State.Info || State.Info->LandscapeActor.Get() != State.Main)
		{
			OutError = TEXT("Exactly one main Landscape with its own LandscapeInfo is required.");
			return false;
		}
		if (!World->GetWorldPartition()->IsStreamingEnabled() || State.StreamingProxyCount != 16)
		{
			OutError = TEXT("The uncropped candidate must retain streaming and exactly 16 loaded LandscapeStreamingProxy actors.");
			return false;
		}
		if (!State.Main->GetActorLocation().Equals(ExpectedOrigin, 0.01) ||
			!State.Main->GetActorScale3D().Equals(ExpectedScale, 0.0001) ||
			!State.Main->GetActorRotation().IsNearlyZero(0.001))
		{
			OutError = TEXT("Landscape origin/rotation/scale differ from the original (-90000,-90000,0), zero rotation, (1250/7,1250/7,256) contract.");
			return false;
		}
		if (State.Info->ComponentSizeQuads != QuadsPerComponent || State.Info->SubsectionSizeQuads != QuadsPerComponent ||
			State.Info->ComponentNumSubsections != 1 || State.Main->GetEditLayersConst().IsEmpty())
		{
			OutError = TEXT("The original 63-quads, one-section, editable-layer layout is required.");
			return false;
		}
		if (State.ComponentEntries != SourceComponentCount || State.Components.Num() != SourceComponentCount ||
			State.CollisionEntries != SourceComponentCount || State.Collisions.Num() != SourceComponentCount ||
			State.bInvalidComponentEntry || State.bInvalidCollisionEntry)
		{
			OutError = TEXT("Exactly 256 distinct loaded render components and 256 distinct collision components are required, without invalid entries.");
			return false;
		}
		if (!State.bAllDescriptorsLoaded || !State.Info->AreAllComponentsRegistered() ||
			State.Info->HasUnloadedComponentsInRegion(0, 0, SourceMaximumSample, SourceMaximumSample))
		{
			OutError = TEXT("All original Landscape descriptors and components must be loaded and registered; load the candidate region first.");
			return false;
		}
		if (!State.bHasExtent || State.Extent.Min != FIntPoint(0, 0) ||
			State.Extent.Max != FIntPoint(SourceMaximumSample, SourceMaximumSample) || State.SectionBases.Num() != SourceComponentCount)
		{
			OutError = TEXT("The candidate must retain the complete original sample extent 0..1008 on both axes.");
			return false;
		}
		const FGuid LandscapeGuid = State.Main->GetLandscapeGuid();
		if (!LandscapeGuid.IsValid())
		{
			OutError = TEXT("The candidate Landscape GUID is invalid.");
			return false;
		}
		const FString Map = World->GetOutermost()->GetName();
		for (const ALandscapeProxy* Proxy : State.Proxies)
		{
			if (Proxy->GetWorld() != World || Proxy->GetLandscapeGuid() != LandscapeGuid ||
				!IsPackageInWorld(Proxy->GetPackage(), Map))
			{
				OutError = TEXT("A loaded proxy does not belong exclusively to this world's Landscape and package namespace.");
				return false;
			}
		}
		for (int32 Y = 0; Y < SourceComponentsPerAxis; ++Y)
		{
			for (int32 X = 0; X < SourceComponentsPerAxis; ++X)
			{
				if (!State.SectionBases.Contains(FIntPoint(X * QuadsPerComponent, Y * QuadsPerComponent)))
				{
					OutError = TEXT("The original 16 by 16 section-base lattice is incomplete or shifted.");
					return false;
				}
			}
		}
		for (ULandscapeComponent* Component : State.Components)
		{
			ALandscapeProxy* Proxy = Component->GetLandscapeProxy();
			ULandscapeHeightfieldCollisionComponent* Collision = Component->GetCollisionComponent();
			const FIntPoint Base = Component->GetSectionBase();
			const FVector ExpectedLocation = ExpectedOrigin + FVector(Base.X * SampleSpacingCm, Base.Y * SampleSpacingCm, 0.0);
			if (!Proxy || Proxy->GetWorld() != World || Component->GetOwner() != Proxy || Component->GetLandscapeInfo() != State.Info ||
				Component->ComponentSizeQuads != QuadsPerComponent || Component->SubsectionSizeQuads != QuadsPerComponent ||
				Component->NumSubsections != 1 || !Component->IsRegistered() ||
				!Component->GetComponentLocation().Equals(ExpectedLocation, 0.1) ||
				!Component->GetComponentScale().Equals(ExpectedScale, 0.0001) ||
				!Component->GetComponentRotation().IsNearlyZero(0.001))
			{
				OutError = TEXT("A render component has a foreign owner/info, unexpected section configuration, registration, or world transform.");
				return false;
			}
			if (!IsValid(Collision) || !State.Collisions.Contains(Collision) || !Collision->IsRegistered() ||
				Collision->GetLandscapeProxy() != Proxy || Collision->GetRenderComponent() != Component ||
				Collision->GetSectionBase() != Base)
			{
				OutError = TEXT("A render component does not have its own registered matching collision component.");
				return false;
			}
		}
		return true;
	}

	void RecordSelectionPlan(const FTerrainState& State, const TSharedRef<FJsonObject>& Result, TSet<ULandscapeComponent*>& Outside)
	{
		TArray<TSharedPtr<FJsonValue>> KeepRows;
		TArray<TSharedPtr<FJsonValue>> OutsideRows;
		TArray<ULandscapeComponent*> Sorted = State.Components.Array();
		Sorted.Sort([](const ULandscapeComponent& A, const ULandscapeComponent& B)
		{
			const FIntPoint ABase = A.GetSectionBase();
			const FIntPoint BBase = B.GetSectionBase();
			return ABase.Y == BBase.Y ? ABase.X < BBase.X : ABase.Y < BBase.Y;
		});
		for (ULandscapeComponent* Component : Sorted)
		{
			const FIntPoint Base = Component->GetSectionBase();
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("path"), Component->GetPathName());
			Row->SetObjectField(TEXT("section_base"), PointObject(Base));
			Row->SetObjectField(TEXT("component_index"), PointObject(Base / QuadsPerComponent));
			if (IsKeptBase(Base))
			{
				KeepRows.Add(MakeShared<FJsonValueObject>(Row));
			}
			else
			{
				Outside.Add(Component);
				OutsideRows.Add(MakeShared<FJsonValueObject>(Row));
			}
		}
		Result->SetNumberField(TEXT("planned_keep_component_count"), KeepRows.Num());
		Result->SetNumberField(TEXT("planned_outside_component_count"), OutsideRows.Num());
		Result->SetArrayField(TEXT("keep_components"), KeepRows);
		Result->SetArrayField(TEXT("outside_components"), OutsideRows);
		Result->SetObjectField(TEXT("expected_retained_bounds_min_cm"), VectorObject(FVector(-33750.0, 22500.0, 0.0)));
		Result->SetObjectField(TEXT("expected_retained_bounds_max_xy_cm"), PointObject(FIntPoint(67500, 78750)));
		Result->SetStringField(TEXT("origin_policy"), TEXT("Retain the existing Landscape actor origin. Retained bounds min is not a new actor location."));
	}

	FString Inspect(const TCHAR* ExpectedMap, const TCHAR* Operation)
	{
		const FDirtySnapshot Before = CaptureDirtyPackages();
		const TSharedRef<FJsonObject> Result = NewResult(Operation, ExpectedMap);
		Result->SetBoolField(TEXT("read_only_inspection"), true);
		UWorld* World = nullptr;
		FString Error;
		if (!ResolveEditorWorld(ExpectedMap, World, Result, Error))
		{
			return Finish(Result, Before, false, Error);
		}
		FTerrainState State;
		CollectTerrain(World, Result, State);
		FString OriginalLayoutError;
		const bool bOriginalLayout = ValidateOriginalLayout(World, State, OriginalLayoutError);
		Result->SetBoolField(TEXT("original_layout_contract_passed"), bOriginalLayout);
		if (!bOriginalLayout)
		{
			Result->SetStringField(TEXT("original_layout_contract_error"), OriginalLayoutError);
		}
		bool bRetainedLattice = State.Components.Num() == KeepComponentCount && State.CollisionEntries == KeepComponentCount;
		for (const FIntPoint& Base : State.SectionBases)
		{
			bRetainedLattice &= Base.X % QuadsPerComponent == 0 && Base.Y % QuadsPerComponent == 0 && IsKeptBase(Base);
		}
		bRetainedLattice &= State.SectionBases.Num() == KeepComponentCount && State.bHasExtent &&
			State.Extent.Min == FIntPoint(315, 630) && State.Extent.Max == FIntPoint(882, 945);
		Result->SetBoolField(TEXT("retained_9_by_5_lattice_observed"), bRetainedLattice);
		Result->SetStringField(TEXT("lattice_validation_boundary"), TEXT("A retained lattice flag alone does not certify edit-layer data, collision heights, package preservation, or play."));
		return Finish(Result, Before, State.MainCount == 1 && State.Info != nullptr,
			State.MainCount == 1 && State.Info ? FString() : TEXT("Exactly one loaded main Landscape and valid LandscapeInfo were not found."));
	}
}
#endif

FString URogue10mCompactWorldAuthoring::InspectSourceTerrain()
{
#if WITH_EDITOR
	return Rogue10mCompactWorldAuthoring::Inspect(Rogue10mCompactWorldAuthoring::SourceMap, TEXT("inspect_source_terrain"));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world inspection is available only in an editor build.\"}");
#endif
}

FString URogue10mCompactWorldAuthoring::InspectCandidateTerrain()
{
#if WITH_EDITOR
	return Rogue10mCompactWorldAuthoring::Inspect(Rogue10mCompactWorldAuthoring::CandidateMap, TEXT("inspect_candidate_terrain"));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world inspection is available only in an editor build.\"}");
#endif
}

FString URogue10mCompactWorldAuthoring::LoadCandidateRegion()
{
#if WITH_EDITOR
	using namespace Rogue10mCompactWorldAuthoring;
	const FDirtySnapshot Before = CaptureDirtyPackages();
	const TSharedRef<FJsonObject> Result = NewResult(TEXT("load_candidate_editor_region"), CandidateMap);
	UWorld* World = nullptr;
	FString Error;
	if (!ResolveEditorWorld(CandidateMap, World, Result, Error))
	{
		return Finish(Result, Before, false, Error);
	}
	UWorldPartition* Partition = World->GetWorldPartition();
	const FBox Region(FVector(-91000.0, -91000.0, -100000.0), FVector(91000.0, 91000.0, 100000.0));
	const auto HasFullRegion = [Partition, &Region]()
	{
		for (const FBox& Loaded : Partition->GetUserLoadedEditorRegions())
		{
			if (Loaded.IsValid && Loaded.IsInsideOrOn(Region))
			{
				return true;
			}
		}
		return false;
	};
	const bool bAlreadyLoaded = HasFullRegion();
	if (!bAlreadyLoaded)
	{
		TArray<FBox> Regions;
		Regions.Add(Region);
		Partition->LoadLastLoadedRegions(Regions);
	}
	const bool bLoaded = HasFullRegion();
	FTerrainState State;
	CollectTerrain(World, Result, State);
	Result->SetBoolField(TEXT("editor_region_already_loaded"), bAlreadyLoaded);
	Result->SetBoolField(TEXT("editor_region_loaded"), bLoaded);
	Result->SetObjectField(TEXT("editor_region"), BoundsObject(Region));
	Result->SetBoolField(TEXT("editor_state_only"), true);
	return Finish(Result, Before, bLoaded, bLoaded ? FString() : TEXT("The bounded candidate editor region is not yet fully loaded. Inspect after editor updates; do not re-import."));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world region loading is available only in an editor build.\"}");
#endif
}

FString URogue10mCompactWorldAuthoring::SelectOutsideComponents(bool bConfirmCandidateOnly)
{
#if WITH_EDITOR
	using namespace Rogue10mCompactWorldAuthoring;
	const FDirtySnapshot Before = CaptureDirtyPackages();
	const TSharedRef<FJsonObject> Result = NewResult(TEXT("select_outside_candidate_components"), CandidateMap);
	Result->SetBoolField(TEXT("confirmation_received"), bConfirmCandidateOnly);
	Result->SetBoolField(TEXT("editor_state_only"), true);
	UWorld* World = nullptr;
	FString Error;
	if (!ResolveEditorWorld(CandidateMap, World, Result, Error))
	{
		return Finish(Result, Before, false, Error);
	}
	FTerrainState State;
	CollectTerrain(World, Result, State);
	if (!ValidateOriginalLayout(World, State, Error))
	{
		return Finish(Result, Before, false, Error);
	}
	TSet<ULandscapeComponent*> Outside;
	RecordSelectionPlan(State, Result, Outside);
	if (Outside.Num() != OutsideComponentCount || State.Components.Num() - Outside.Num() != KeepComponentCount)
	{
		return Finish(Result, Before, false, TEXT("The exact keep-45 / outside-211 partition failed. No selection was changed."));
	}
	if (!bConfirmCandidateOnly)
	{
		Result->SetBoolField(TEXT("confirmation_required"), true);
		return Finish(Result, Before, false, TEXT("Read-only selection preview. Pass bConfirmCandidateOnly=true to prepare this candidate-only selection; nothing has been selected or deleted."));
	}
	const TSet<ULandscapeComponent*> PreviousSelection = State.Info->GetSelectedComponents();
	Result->SetNumberField(TEXT("selected_component_count_before"), PreviousSelection.Num());
	// Public Landscape API only. This changes editor selection, never height/weight data or component ownership.
	State.Info->UpdateSelectedComponents(Outside, true);
	const TSet<ULandscapeComponent*> ActualSelection = State.Info->GetSelectedComponents();
	const bool bExactSelection = ActualSelection.Num() == OutsideComponentCount && ActualSelection.Difference(Outside).IsEmpty();
	Result->SetBoolField(TEXT("selection_performed"), true);
	Result->SetBoolField(TEXT("selection_matches_outside_set"), bExactSelection);
	Result->SetNumberField(TEXT("selected_component_count_after"), ActualSelection.Num());
	Result->SetNumberField(TEXT("selected_component_count"), ActualSelection.Num());
	Result->SetStringField(TEXT("next_step"), TEXT("The editor owner must apply native Landscape Manage/Delete and validate the retained terrain separately. No components were deleted or saved by this function."));
	return Finish(Result, Before, bExactSelection, bExactSelection ? FString() : TEXT("The editor selection does not match the planned 211 components. No component destruction or save was attempted."));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world component selection is available only in an editor build.\"}");
#endif
}

#if WITH_EDITOR
namespace Rogue10mCompactRetainedSnapshot
{
	using namespace Rogue10mCompactWorldAuthoring;
	constexpr int32 MinX = 315, MinY = 630, MaxX = 882, MaxY = 945;
	constexpr int32 Width = MaxX - MinX + 1, Height = MaxY - MinY + 1;
	constexpr int32 SampleCount = Width * Height;

	struct FMipCopy
	{
		TArray64<uint8> Bytes;
		int32 Width = 0, Height = 0;
		FGuid SourceId;
		bool bDirty = false, bTransactional = false;
	};
	using FMipCache = TMap<UTexture2D*, TSharedPtr<FMipCopy>>;

	void HashUInt(FBlake3& Hash, uint32 Value)
	{
		const uint8 Bytes[] = {uint8(Value), uint8(Value >> 8), uint8(Value >> 16), uint8(Value >> 24)};
		Hash.Update(Bytes, sizeof(Bytes));
	}

	void HashText(FBlake3& Hash, const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		HashUInt(Hash, uint32(Utf8.Length()));
		Hash.Update(Utf8.Get(), uint64(Utf8.Length()));
	}

	void HashFloat(FBlake3& Hash, float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		HashUInt(Hash, Bits);
	}

	bool ValidateLayout(UWorld* World, const FTerrainState& State, FString& Error)
	{
		if (State.Components.Num() == SourceComponentCount)
		{
			return ValidateOriginalLayout(World, State, Error);
		}
		if (State.MainCount != 1 || !State.Main || !State.Info || State.Info->LandscapeActor.Get() != State.Main ||
			State.ComponentEntries != KeepComponentCount || State.Components.Num() != KeepComponentCount ||
			State.CollisionEntries != KeepComponentCount || State.Collisions.Num() != KeepComponentCount ||
			State.SectionBases.Num() != KeepComponentCount || State.bInvalidComponentEntry || State.bInvalidCollisionEntry ||
			!State.bAllDescriptorsLoaded || !State.Info->AreAllComponentsRegistered() || !State.bHasExtent ||
			State.Extent.Min != FIntPoint(MinX, MinY) || State.Extent.Max != FIntPoint(MaxX, MaxY))
		{
			Error = TEXT("Snapshot requires the complete original 256-component layout or the exact retained 45-component lattice, fully loaded with matching collisions.");
			return false;
		}
		if (!World->GetWorldPartition()->IsStreamingEnabled() || !State.Main->GetLandscapeGuid().IsValid() ||
			!State.Main->GetActorLocation().Equals(ExpectedOrigin, 0.01) ||
			!State.Main->GetActorScale3D().Equals(ExpectedScale, 0.0001) ||
			!State.Main->GetActorRotation().IsNearlyZero(0.001) ||
			State.Info->ComponentSizeQuads != 63 || State.Info->SubsectionSizeQuads != 63 || State.Info->ComponentNumSubsections != 1 ||
			State.Info->HasUnloadedComponentsInRegion(MinX, MinY, MaxX, MaxY))
		{
			Error = TEXT("The retained Landscape transform, one-section/63-quads layout, or loaded-region contract is invalid.");
			return false;
		}
		for (const ALandscapeProxy* Proxy : State.Proxies)
		{
			if (Proxy->GetWorld() != World || Proxy->GetLandscapeGuid() != State.Main->GetLandscapeGuid() ||
				!IsPackageInWorld(Proxy->GetPackage(), CandidateMap))
			{
				Error = TEXT("A retained proxy has a foreign world, Landscape GUID, or package.");
				return false;
			}
		}
		for (const ULandscapeComponent* Component : State.Components)
		{
			const FIntPoint Base = Component->GetSectionBase();
			const ALandscapeProxy* Proxy = Component->GetLandscapeProxy();
			ULandscapeHeightfieldCollisionComponent* Collision = Component->GetCollisionComponent();
			if (!IsKeptBase(Base) || Base.X % 63 || Base.Y % 63 || !Proxy || Proxy->GetWorld() != World ||
				Component->GetOwner() != Proxy || Component->GetLandscapeInfo() != State.Info ||
				Component->ComponentSizeQuads != 63 || Component->SubsectionSizeQuads != 63 || Component->NumSubsections != 1 ||
				!Component->IsRegistered() || !Component->GetComponentScale().Equals(ExpectedScale, 0.0001) ||
				!Component->GetComponentRotation().IsNearlyZero(0.001) ||
				!Component->GetComponentLocation().Equals(ExpectedOrigin + FVector(Base.X * SampleSpacingCm, Base.Y * SampleSpacingCm, 0.0), 0.1) ||
				!IsValid(Collision) || !State.Collisions.Contains(Collision) ||
				!Collision->IsRegistered() || Collision->GetLandscapeProxy() != Proxy ||
				Collision->GetRenderComponent() != Component || Collision->GetSectionBase() != Base)
			{
				Error = TEXT("A retained component has an invalid section base, owner, transform, registration, or collision pair.");
				return false;
			}
		}
		return true;
	}

	const FMipCopy* ReadMip(UTexture2D* Texture, FMipCache& Cache, FString& Error)
	{
		if (!IsValid(Texture))
		{
			Error = TEXT("Missing texture source; no fallback data was substituted.");
			return nullptr;
		}
		if (const TSharedPtr<FMipCopy>* Existing = Cache.Find(Texture))
		{
			return Existing->Get();
		}
		const int64 W = Texture->Source.GetSizeX(), H = Texture->Source.GetSizeY();
		if (Texture->Source.GetFormat() != TSF_BGRA8 || Texture->Source.GetNumBlocks() != 1 ||
			Texture->Source.GetNumLayers() != 1 || Texture->Source.GetNumSlices() != 1 || Texture->Source.GetNumMips() < 1 ||
			W <= 0 || H <= 0 || W > MAX_int32 || H > MAX_int32 || W > MAX_int32 / 4 / H)
		{
			Error = TEXT("Expected one-block, one-layer, one-slice BGRA8 source mip with valid dimensions: ") + Texture->GetPathName();
			return nullptr;
		}
		TSharedPtr<FMipCopy> Copy = MakeShared<FMipCopy>();
		Copy->Width = int32(W);
		Copy->Height = int32(H);
		Copy->SourceId = Texture->Source.GetId();
		Copy->bDirty = Texture->GetPackage()->IsDirty();
		Copy->bTransactional = Texture->HasAnyFlags(RF_Transactional);
		// GetMipData copies through FTextureSource::FMipLock(ReadOnly). No Landscape edit-data cache is constructed.
		if (!Texture->Source.GetMipData(Copy->Bytes, 0) || Copy->Bytes.Num() != W * H * 4 ||
			Texture->Source.GetId() != Copy->SourceId || Texture->GetPackage()->IsDirty() != Copy->bDirty ||
			Texture->HasAnyFlags(RF_Transactional) != Copy->bTransactional)
		{
			Error = TEXT("Texture source copy failed or source state changed while reading: ") + Texture->GetPathName();
			return nullptr;
		}
		Cache.Add(Texture, Copy);
		return Copy.Get();
	}

	bool ReadChannel(const TArray<const ULandscapeComponent*>& Components, const FGuid& Guid,
		ULandscapeLayerInfoObject* Paint, FMipCache& Cache, const FString& MetadataDigest, const FString& Label,
		const TSharedRef<FJsonObject>& Row, FBlake3& Aggregate, FString& Error)
	{
		const bool bHeight = Paint == nullptr;
		const int32 BytesPerSample = bHeight ? 2 : 1;
		TArray<uint8> Data;
		Data.SetNumZeroed(SampleCount * BytesPerSample);
		TBitArray<> Seen(false, SampleCount);
		int32 Unique = 0, Duplicates = 0;
		const int32 ChannelOffsets[] = {STRUCT_OFFSET(FColor, R), STRUCT_OFFSET(FColor, G), STRUCT_OFFSET(FColor, B), STRUCT_OFFSET(FColor, A)};
		for (const ULandscapeComponent* Component : Components)
		{
			if (Guid.IsValid() && Component->GetLayerData(Guid) == nullptr)
			{
				Error = TEXT("Missing requested edit-layer component data; final/composite fallback is forbidden.");
				return false;
			}
			UTexture2D* Texture = bHeight ? Component->GetHeightmap(Guid) : nullptr;
			int32 Channel = 0;
			if (!bHeight)
			{
				int32 Matches = 0;
				const TArray<UTexture2D*>& Textures = Component->GetWeightmapTextures(Guid);
				for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(Guid))
				{
					if (Allocation.LayerInfo == Paint && Allocation.IsAllocated())
					{
						if (++Matches != 1 || !Textures.IsValidIndex(Allocation.WeightmapTextureIndex) || Allocation.WeightmapTextureChannel >= 4)
						{
							Error = TEXT("Ambiguous or invalid paint allocation: ") + Paint->GetPathName();
							return false;
						}
						Texture = Textures[Allocation.WeightmapTextureIndex];
						Channel = ChannelOffsets[Allocation.WeightmapTextureChannel];
						if (!IsValid(Texture))
						{
							Error = TEXT("Allocated paint channel has no texture: ") + Paint->GetPathName();
							return false;
						}
					}
				}
			}
			const FMipCopy* Mip = Texture ? ReadMip(Texture, Cache, Error) : nullptr;
			if ((Texture && !Mip) || (bHeight && !Mip))
			{
				if (Error.IsEmpty()) { Error = TEXT("Missing requested height texture."); }
				return false;
			}
			int32 OffsetX = 0, OffsetY = 0;
			if (Mip)
			{
				const FVector4& Bias = bHeight ? Component->HeightmapScaleBias : Component->WeightmapScaleBias;
				const double U = Bias.Z * Mip->Width, V = Bias.W * Mip->Height;
				if (!FMath::IsFinite(U) || !FMath::IsFinite(V) || U < 0 || V < 0 || U >= MAX_int32 || V >= MAX_int32)
				{
					Error = TEXT("Invalid source mip UV offsets for the component's 64 by 64 sample block.");
					return false;
				}
				OffsetX = int32(U); OffsetY = int32(V);
				// Match the native reader's truncation: weightmaps intentionally have a half-texel UV bias.
				if (OffsetX > Mip->Width - 64 || OffsetY > Mip->Height - 64)
				{
					Error = TEXT("The component's 64 by 64 sample block exceeds its source mip.");
					return false;
				}
			}
			const FIntPoint Base = Component->GetSectionBase();
			for (int32 Y = 0; Y <= 63; ++Y)
			{
				for (int32 X = 0; X <= 63; ++X)
				{
					const int32 I = (Base.Y + Y - MinY) * Width + Base.X + X - MinX;
					if (!Data.IsValidIndex(I * BytesPerSample))
					{
						Error = TEXT("A component sample lies outside the fixed retained rectangle.");
						return false;
					}
					const uint8* Pixel = Mip ? Mip->Bytes.GetData() + 4 * (int64(OffsetY + Y) * Mip->Width + OffsetX + X) : nullptr;
					const uint8 Low = bHeight ? Pixel[STRUCT_OFFSET(FColor, G)] : (Pixel ? Pixel[Channel] : 0);
					const uint8 High = bHeight ? Pixel[STRUCT_OFFSET(FColor, R)] : 0;
					if (Seen[I])
					{
						++Duplicates;
						if (Data[I * BytesPerSample] != Low || (bHeight && Data[I * 2 + 1] != High))
						{
							Error = FString::Printf(TEXT("Shared seam sample differs at (%d,%d) in %s."), Base.X + X, Base.Y + Y, *Label);
							return false;
						}
					}
					else
					{
						Seen[I] = true; ++Unique;
						Data[I * BytesPerSample] = Low;
						if (bHeight) { Data[I * 2 + 1] = High; }
					}
				}
			}
		}
		if (Unique != SampleCount || Duplicates != 4832)
		{
			Error = TEXT("The retained rectangle does not have complete 179488-sample coverage and the expected 4832 matching seam duplicates.");
			return false;
		}
		const FString Digest = LexToString(FBlake3::HashBuffer(Data.GetData(), uint64(Data.Num())));
		FBlake3 Linked;
		HashText(Linked, MetadataDigest); HashText(Linked, Label);
		Linked.Update(Data.GetData(), uint64(Data.Num()));
		Row->SetStringField(TEXT("digest"), Digest);
		Row->SetStringField(TEXT("metadata_linked_digest"), LexToString(Linked.Finalize()));
		Row->SetStringField(TEXT("encoding"), bHeight ? TEXT("uint16_little_endian_RG_height") : TEXT("uint8_paint_weight"));
		Row->SetNumberField(TEXT("sample_count"), Unique);
		Row->SetNumberField(TEXT("byte_count"), Data.Num());
		Row->SetNumberField(TEXT("matching_seam_duplicates"), Duplicates);
		HashText(Aggregate, Label); HashText(Aggregate, Digest);
		return true;
	}

	bool Metadata(const FTerrainState& State, const TArray<ULandscapeLayerInfoObject*>& Paints, FString& Digest, FString& Error)
	{
		FBlake3 Hash;
		HashText(Hash, TEXT("Rogue10m.retained-samples.v1;BLAKE3;315,630,882,945;X_fastest;uint16LE"));
		const TArray<const ULandscapeEditLayerBase*> Layers = State.Main->GetEditLayersConst();
		HashUInt(Hash, uint32(Layers.Num()));
		for (const ULandscapeEditLayerBase* Layer : Layers)
		{
			if (!IsValid(Layer))
			{
				Error = TEXT("An edit-layer object is missing.");
				return false;
			}
			const float HeightAlpha = Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap);
			const float WeightAlpha = Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap);
			if (!FMath::IsFinite(HeightAlpha) || !FMath::IsFinite(WeightAlpha))
			{
				Error = TEXT("An edit-layer alpha is not finite.");
				return false;
			}
			HashText(Hash, Layer->GetGuid().ToString()); HashText(Hash, Layer->GetName().ToString());
			HashText(Hash, Layer->GetClass()->GetPathName());
			HashFloat(Hash, HeightAlpha); HashFloat(Hash, WeightAlpha);
			HashUInt(Hash, Layer->IsVisible()); HashUInt(Hash, Layer->IsLocked());
			HashUInt(Hash, Layer->NeedsPersistentTextures()); HashUInt(Hash, Layer->SupportsEditingTools());
		}
		HashUInt(Hash, uint32(Paints.Num()));
		for (const ULandscapeLayerInfoObject* Paint : Paints)
		{
			HashText(Hash, Paint->GetPathName()); HashText(Hash, Paint->GetLayerName().ToString());
			HashText(Hash, Paint->GetClass()->GetPathName());
		}
		Digest = LexToString(Hash.Finalize());
		return true;
	}

	bool ReadGroup(const TArray<const ULandscapeComponent*>& Components, const FGuid& Guid,
		const TArray<ULandscapeLayerInfoObject*>& Paints, FMipCache& Cache, const FString& MetadataDigest,
		const FString& Label, const TSharedRef<FJsonObject>& Group, FBlake3& Aggregate, FString& Error)
	{
		const TSharedRef<FJsonObject> Heights = MakeShared<FJsonObject>();
		Group->SetStringField(TEXT("label"), Label);
		Group->SetStringField(TEXT("edit_layer_guid"), Guid.ToString());
		Group->SetObjectField(TEXT("height"), Heights);
		if (!ReadChannel(Components, Guid, nullptr, Cache, MetadataDigest, Label + TEXT("/height"), Heights, Aggregate, Error))
		{
			Group->SetBoolField(TEXT("complete"), false);
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Weights;
		for (ULandscapeLayerInfoObject* Paint : Paints)
		{
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("layer_info_path"), Paint->GetPathName());
			Row->SetStringField(TEXT("layer_name"), Paint->GetLayerName().ToString());
			Weights.Add(MakeShared<FJsonValueObject>(Row));
			if (!ReadChannel(Components, Guid, Paint, Cache, MetadataDigest, Label + TEXT("/weight/") + Paint->GetPathName(), Row, Aggregate, Error))
			{
				Group->SetArrayField(TEXT("paint_weights"), Weights);
				Group->SetBoolField(TEXT("complete"), false);
				return false;
			}
		}
		Group->SetArrayField(TEXT("paint_weights"), Weights);
		Group->SetBoolField(TEXT("complete"), true);
		return true;
	}

	FString Snapshot()
	{
		if (!IsInGameThread()) { return TEXT("{\"success\":false,\"error\":\"Retained terrain snapshots require the editor game thread.\"}"); }
		const FDirtySnapshot Before = CaptureDirtyPackages();
		const TSharedRef<FJsonObject> Result = NewResult(TEXT("snapshot_retained_terrain_data"), CandidateMap);
		Result->SetStringField(TEXT("digest_algorithm"), TEXT("BLAKE3"));
		Result->SetBoolField(TEXT("read_only_inspection"), true);
		Result->SetStringField(TEXT("sample_order"), TEXT("Y630..945 rows; X315..882 fastest; height uint16 little-endian; paint uint8"));
		Result->SetStringField(TEXT("data_boundary"), TEXT("Current settled CPU Texture.Source RG height and paint channels. Texture packing and height B/A are excluded; no GPU export or gameplay certification."));
		UWorld* World = nullptr;
		FString Error;
		if (!ResolveEditorWorld(CandidateMap, World, Result, Error)) { return Finish(Result, Before, false, Error); }
		FTerrainState State;
		CollectTerrain(World, Result, State);
		if (!ValidateLayout(World, State, Error))
		{
			return Finish(Result, Before, false, Error);
		}
		if (!State.Main->IsUpToDate())
		{
			return Finish(Result, Before, false, TEXT("Landscape composition/readback is pending. Allow normal editor updates and retry; this function will not force an update."));
		}
		const FGuid EditingLayerBefore = State.Main->GetEditingLayer();
		const TSet<ULandscapeComponent*> SelectionBefore = State.Info->GetSelectedComponents();
		TArray<const ULandscapeComponent*> Components;
		for (const ULandscapeComponent* Component : State.Components) { if (IsKeptBase(Component->GetSectionBase())) { Components.Add(Component); } }
		Components.Sort([](const ULandscapeComponent& A, const ULandscapeComponent& B)
		{
			const FIntPoint P = A.GetSectionBase(), Q = B.GetSectionBase();
			return P.Y == Q.Y ? P.X < Q.X : P.Y < Q.Y;
		});
		if (Components.Num() != 45) { return Finish(Result, Before, false, TEXT("Exactly 45 retained components are required.")); }
		TSet<ULandscapeLayerInfoObject*> PaintSet;
		for (const FLandscapeInfoLayerSettings& Settings : State.Info->Layers)
		{
			if (!IsValid(Settings.LayerInfoObj)) { return Finish(Result, Before, false, TEXT("A declared paint layer has no LayerInfo asset; paint preservation is incomplete.")); }
			PaintSet.Add(Settings.LayerInfoObj.Get());
		}
		const TArray<const ULandscapeEditLayerBase*> Layers = State.Main->GetEditLayersConst();
		if (Layers.IsEmpty()) { return Finish(Result, Before, false, TEXT("No edit-layer objects are available.")); }
		TArray<FGuid> Guids; Guids.Add(FGuid());
		for (const ULandscapeEditLayerBase* Layer : Layers)
		{
			if (!IsValid(Layer) || !Layer->GetGuid().IsValid() || Guids.Contains(Layer->GetGuid())) { return Finish(Result, Before, false, TEXT("An edit-layer object/GUID is invalid or duplicated.")); }
			Guids.Add(Layer->GetGuid());
		}
		for (const ULandscapeComponent* Component : Components)
		{
			for (const FGuid& Guid : Guids)
			{
				if (Guid.IsValid() && !Component->GetLayerData(Guid)) { continue; }
				for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(Guid))
				{
					if (Allocation.IsAllocated())
					{
						if (!IsValid(Allocation.LayerInfo)) { return Finish(Result, Before, false, TEXT("An allocated paint channel has no LayerInfo asset.")); }
						PaintSet.Add(Allocation.LayerInfo.Get());
					}
				}
			}
		}
		TArray<ULandscapeLayerInfoObject*> Paints = PaintSet.Array();
		Paints.Sort([](const ULandscapeLayerInfoObject& A, const ULandscapeLayerInfoObject& B) { return A.GetPathName() < B.GetPathName(); });
		if (Paints.IsEmpty()) { return Finish(Result, Before, false, TEXT("No paint LayerInfo assets are available to verify.")); }
		FString MetadataBefore;
		if (!Metadata(State, Paints, MetadataBefore, Error)) { return Finish(Result, Before, false, Error); }
		Result->SetStringField(TEXT("metadata_digest"), MetadataBefore);
		Result->SetNumberField(TEXT("retained_sample_count"), SampleCount);
		Result->SetNumberField(TEXT("height_byte_count"), SampleCount * 2);
		Result->SetNumberField(TEXT("paint_bytes_per_layer"), SampleCount);
		FMipCache Cache;
		FBlake3 Aggregate; HashText(Aggregate, MetadataBefore);
		TArray<FString> Errors;
		const TSharedRef<FJsonObject> Composite = MakeShared<FJsonObject>();
		Result->SetObjectField(TEXT("composite"), Composite);
		if (!ReadGroup(Components, FGuid(), Paints, Cache, MetadataBefore, TEXT("composite"), Composite, Aggregate, Error)) { Composite->SetStringField(TEXT("error"), Error); Errors.Add(Error); }
		TArray<TSharedPtr<FJsonValue>> RawLayers;
		for (const ULandscapeEditLayerBase* Layer : Layers)
		{
			const TSharedRef<FJsonObject> Group = MakeShared<FJsonObject>();
			RawLayers.Add(MakeShared<FJsonValueObject>(Group));
			Group->SetStringField(TEXT("name"), Layer->GetName().ToString());
			Group->SetStringField(TEXT("edit_layer_guid"), Layer->GetGuid().ToString());
			TArray<FString> Missing;
			for (const ULandscapeComponent* Component : Components)
			{
				if (Component->GetLayerData(Layer->GetGuid()) == nullptr) { Missing.Add(Component->GetSectionBase().ToString()); }
			}
			if (!Layer->NeedsPersistentTextures() || !Missing.IsEmpty())
			{
				Group->SetBoolField(TEXT("complete"), false);
				Group->SetStringField(TEXT("status"), Layer->NeedsPersistentTextures() ? TEXT("missing_component_layer_data") : TEXT("unsupported_nonpersistent_layer"));
				Group->SetArrayField(TEXT("missing_component_section_bases"), StringArray(Missing));
				Errors.Add(TEXT("Raw edit-layer source data is incomplete or unsupported: ") + Layer->GetName().ToString());
				continue;
			}
			Error.Reset();
			if (!ReadGroup(Components, Layer->GetGuid(), Paints, Cache, MetadataBefore, TEXT("edit-layer/") + Layer->GetGuid().ToString(), Group, Aggregate, Error))
			{
				Group->SetStringField(TEXT("error"), Error); Errors.Add(Error);
			}
		}
		Result->SetArrayField(TEXT("raw_edit_layers"), RawLayers);
		bool bSourceStable = true;
		for (const TPair<UTexture2D*, TSharedPtr<FMipCopy>>& Pair : Cache)
		{
			bSourceStable &= Pair.Key->Source.GetId() == Pair.Value->SourceId && Pair.Key->GetPackage()->IsDirty() == Pair.Value->bDirty &&
				Pair.Key->HasAnyFlags(RF_Transactional) == Pair.Value->bTransactional;
		}
		FString MetadataAfter, MetadataError;
		const bool bMetadataStable = Metadata(State, Paints, MetadataAfter, MetadataError) && MetadataAfter == MetadataBefore;
		const TSet<ULandscapeComponent*> SelectionAfter = State.Info->GetSelectedComponents();
		const bool bSelectionStable = SelectionBefore.Num() == SelectionAfter.Num() && SelectionBefore.Difference(SelectionAfter).IsEmpty();
		const bool bEditingLayerStable = State.Main->GetEditingLayer() == EditingLayerBefore;
		const bool bSettled = State.Main->IsUpToDate();
		Result->SetBoolField(TEXT("texture_sources_unchanged"), bSourceStable);
		Result->SetBoolField(TEXT("metadata_unchanged"), bMetadataStable);
		Result->SetBoolField(TEXT("selection_unchanged"), bSelectionStable);
		Result->SetBoolField(TEXT("editing_layer_unchanged"), bEditingLayerStable);
		Result->SetBoolField(TEXT("composite_settled_after_read"), bSettled);
		Result->SetNumberField(TEXT("cached_texture_count"), Cache.Num());
		if (!bSourceStable || !bMetadataStable || !bSelectionStable || !bEditingLayerStable || !bSettled)
		{
			Errors.Add(TEXT("Editor/source/metadata state changed or composition became pending during the snapshot."));
		}
		if (!Before.Equals(CaptureDirtyPackages()))
		{
			Errors.Add(TEXT("Dirty package sets changed during the snapshot; no semantic digest is certified."));
		}
		const bool bComplete = Errors.IsEmpty();
		Result->SetBoolField(TEXT("all_requested_data_complete"), bComplete);
		Result->SetArrayField(TEXT("snapshot_errors"), StringArray(Errors));
		if (bComplete) { Result->SetStringField(TEXT("semantic_snapshot_digest"), LexToString(Aggregate.Finalize())); }
		return Finish(Result, Before, bComplete, bComplete ? FString() : FString::Join(Errors, TEXT(" | ")));
	}
}
#endif

FString URogue10mCompactWorldAuthoring::SnapshotRetainedTerrainData()
{
#if WITH_EDITOR
	return Rogue10mCompactRetainedSnapshot::Snapshot();
#else
	return TEXT("{\"success\":false,\"error\":\"Retained terrain snapshots are available only in an editor build.\"}");
#endif
}


