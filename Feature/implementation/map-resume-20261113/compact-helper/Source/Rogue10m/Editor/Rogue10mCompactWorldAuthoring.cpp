// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mCompactWorldAuthoring.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
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

