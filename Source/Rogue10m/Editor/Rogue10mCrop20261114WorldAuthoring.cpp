// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mCrop20261114WorldAuthoring.h"

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

namespace Rogue10mCrop20261114WorldAuthoring
{
	const TCHAR* const ExpectedProjectDirectory = TEXT("D:/Project/Rogue10m");
	const TCHAR* const SourceMap = TEXT("/Game/Rogue10m/Maps/Production/WorldLayout20261112");
	const TCHAR* const CandidateMap = TEXT("/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A");
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

FString URogue10mCrop20261114WorldAuthoring::InspectSourceTerrain()
{
#if WITH_EDITOR
	return Rogue10mCrop20261114WorldAuthoring::Inspect(Rogue10mCrop20261114WorldAuthoring::SourceMap, TEXT("inspect_source_terrain"));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world inspection is available only in an editor build.\"}");
#endif
}

FString URogue10mCrop20261114WorldAuthoring::InspectCandidateTerrain()
{
#if WITH_EDITOR
	return Rogue10mCrop20261114WorldAuthoring::Inspect(Rogue10mCrop20261114WorldAuthoring::CandidateMap, TEXT("inspect_candidate_terrain"));
#else
	return TEXT("{\"success\":false,\"error\":\"Compact world inspection is available only in an editor build.\"}");
#endif
}

FString URogue10mCrop20261114WorldAuthoring::LoadCandidateRegion()
{
#if WITH_EDITOR
	using namespace Rogue10mCrop20261114WorldAuthoring;
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

FString URogue10mCrop20261114WorldAuthoring::SelectOutsideComponents(bool bConfirmCandidateOnly)
{
#if WITH_EDITOR
	using namespace Rogue10mCrop20261114WorldAuthoring;
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
namespace Rogue10mCrop20261114RetainedSnapshot
{
	using namespace Rogue10mCrop20261114WorldAuthoring;
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

FString URogue10mCrop20261114WorldAuthoring::SnapshotRetainedTerrainData()
{
#if WITH_EDITOR
	return Rogue10mCrop20261114RetainedSnapshot::Snapshot();
#else
	return TEXT("{\"success\":false,\"error\":\"Retained terrain snapshots are available only in an editor build.\"}");
#endif
}

#if WITH_EDITOR
#include "Editor/Transactor.h"
#include "LandscapeBlueprintBrushBase.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "Selection.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "StructUtils/SharedStruct.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/PropertyOptional.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Rogue10mCrop20261114Dependencies
{
	using namespace Rogue10mCrop20261114WorldAuthoring;
	using namespace Rogue10mCrop20261114RetainedSnapshot;

	// Scope: copied source mip zero, and loaded reflected objects. Never a deletion-safety certificate.
	constexpr int32 MaxReferenceValues = 4000000;
	constexpr int32 MaxReferenceRows = 50000;
	constexpr int32 MaxReferenceDepth = 32;

	FString ObjectPath(const UObject* Object) { return Object ? Object->GetPathName() : FString(); }
	FString StoredObjectPath(const UObject* Object, const FName Name)
	{
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), Name);
		if (!Property) { return TEXT("unsupported_property"); }
		return Property->GetObjectPtrPropertyValue(Property->ContainerPtrToValuePtr<void>(Object)).GetPathName();
	}

	TArray<FString> SelectedPaths(const USelection* Selection)
	{
		TArray<FString> Paths;
		if (Selection)
		{
			for (int32 Index = 0; Index < Selection->Num(); ++Index)
			{
				Paths.Add(ObjectPath(Selection->GetSelectedObject(Index)));
			}
		}
		Paths.Sort();
		return Paths;
	}

	struct FEditorReadState
	{
		UEditorEngine* Editor = nullptr;
		UTransactor* Transactor = nullptr;
		USelection* Actors = nullptr;
		USelection* Components = nullptr;
		USelection* Objects = nullptr;
		int32 QueueLength = -1, UndoCount = -1;
		bool bTransactionActive = false;
		bool bTransactionHandleReadable = true;
		TArray<FString> ActorSelection, ComponentSelection, ObjectSelection;
		TMap<const UObject*, uint32> ObjectFlags;
	};

	FEditorReadState CaptureEditorState()
	{
		FEditorReadState State;
		State.Editor = GEditor;
		if (GEditor)
		{
			const TObjectPtr<UTransactor> TransactorCopy = GEditor->Trans;
			State.bTransactionHandleReadable = TransactorCopy.IsResolved();
			// Read only a copied, already-resolved handle; never resolve the editor's stored handle.
			State.Transactor = TransactorCopy.IsResolved() ? TransactorCopy.Get() : nullptr;
			State.Actors = GEditor->GetSelectedActors();
			State.Components = GEditor->GetSelectedComponents();
			State.Objects = GEditor->GetSelectedObjects();
			State.ActorSelection = SelectedPaths(State.Actors);
			State.ComponentSelection = SelectedPaths(State.Components);
			State.ObjectSelection = SelectedPaths(State.Objects);
			if (State.Transactor)
			{
				State.QueueLength = State.Transactor->GetQueueLength();
				State.UndoCount = State.Transactor->GetUndoCount();
				State.bTransactionActive = State.Transactor->IsActive();
			}
		}
		for (TObjectIterator<UObject> It; It; ++It) { State.ObjectFlags.Add(*It, uint32(It->GetFlags())); }
		return State;
	}

	bool RecordEditorStability(const FEditorReadState& Before, const TSharedRef<FJsonObject>& Result)
	{
		const FEditorReadState After = CaptureEditorState();
		bool bObjectsStable = Before.ObjectFlags.Num() == After.ObjectFlags.Num();
		for (const TPair<const UObject*, uint32>& Pair : Before.ObjectFlags)
		{
			const uint32* Flags = After.ObjectFlags.Find(Pair.Key);
			bObjectsStable &= Flags && *Flags == Pair.Value;
		}
		const bool bEditorStable = Before.Editor == After.Editor;
		const bool bTransactionStable = Before.bTransactionHandleReadable && After.bTransactionHandleReadable && Before.Transactor == After.Transactor && Before.QueueLength == After.QueueLength &&
			Before.UndoCount == After.UndoCount && Before.bTransactionActive == After.bTransactionActive;
		const bool bSelectionStable = Before.Actors == After.Actors && Before.Components == After.Components && Before.Objects == After.Objects &&
			Before.ActorSelection == After.ActorSelection && Before.ComponentSelection == After.ComponentSelection && Before.ObjectSelection == After.ObjectSelection;
		Result->SetBoolField(TEXT("editor_pointer_unchanged"), bEditorStable);
		Result->SetBoolField(TEXT("loaded_uobject_identities_and_flags_unchanged"), bObjectsStable);
		Result->SetBoolField(TEXT("editor_object_selection_unchanged"), bSelectionStable);
		Result->SetBoolField(TEXT("transaction_queue_and_active_state_unchanged"), bTransactionStable);
		Result->SetBoolField(TEXT("transaction_handle_readable"), Before.bTransactionHandleReadable && After.bTransactionHandleReadable);
		Result->SetNumberField(TEXT("loaded_uobject_count_before"), Before.ObjectFlags.Num());
		Result->SetNumberField(TEXT("loaded_uobject_count_after"), After.ObjectFlags.Num());
		Result->SetNumberField(TEXT("transaction_queue_length_before"), Before.QueueLength);
		Result->SetNumberField(TEXT("transaction_queue_length_after"), After.QueueLength);
		Result->SetNumberField(TEXT("transaction_undo_count_before"), Before.UndoCount);
		Result->SetNumberField(TEXT("transaction_undo_count_after"), After.UndoCount);
		Result->SetBoolField(TEXT("transaction_active_before"), Before.bTransactionActive);
		Result->SetBoolField(TEXT("transaction_active_after"), After.bTransactionActive);
		return bEditorStable && bObjectsStable && bTransactionStable && bSelectionStable;
	}

	TArray<TSharedPtr<FJsonValue>> Histogram(const TArray<int32>& Counts)
	{
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (int32 Value = 0; Value < Counts.Num(); ++Value)
		{
			if (Counts[Value] == 0) { continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("value"), Value); Row->SetNumberField(TEXT("count"), Counts[Value]);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		return Rows;
	}

	bool ReadRawBA(const TArray<const ULandscapeComponent*>& Components, const ULandscapeEditLayerBase* Layer,
		FMipCache& Cache, const FString& LayerMetadata, const TSharedRef<FJsonObject>& Row, FString& Error)
	{
		Row->SetStringField(TEXT("edit_layer_guid"), Layer->GetGuid().ToString());
		Row->SetStringField(TEXT("name"), Layer->GetName().ToString());
		Row->SetStringField(TEXT("class"), Layer->GetClass()->GetPathName());
		Row->SetStringField(TEXT("encoding"), TEXT("uint16LE_packed_BA; value=(B<<8)|A; bytes=A,B; Y630..945; X315..882_fastest"));
		Row->SetStringField(TEXT("interpretation_boundary"), TEXT("Native accessor projection only; use depends on layer blend/render mode. No universal default or ignored-channel claim."));
		Row->SetStringField(TEXT("alpha14_normalization"), TEXT("((B<<8)|A)>>2, divided by 16383 in native UnpackHeightAlpha; masked uint16 value is alpha14<<2."));
		if (!Layer->NeedsPersistentTextures()) { Error = TEXT("Nonpersistent raw edit layer is unsupported; no composite fallback."); return false; }
		TArray<uint8> Packed, AlphaMasked, Flags;
		Packed.SetNumZeroed(SampleCount * 2); AlphaMasked.SetNumZeroed(SampleCount * 2); Flags.SetNumZeroed(SampleCount);
		TBitArray<> Seen(false, SampleCount);
		int32 Unique = 0, Duplicates = 0;
		for (const ULandscapeComponent* Component : Components)
		{
			if (!Component->GetLayerData(Layer->GetGuid())) { Error = TEXT("Missing raw layer data; composite fallback is forbidden."); return false; }
			const FMipCopy* Mip = ReadMip(Component->GetHeightmap(Layer->GetGuid()), Cache, Error);
			if (!Mip) { return false; }
			const double U = Component->HeightmapScaleBias.Z * Mip->Width, V = Component->HeightmapScaleBias.W * Mip->Height;
			if (!FMath::IsFinite(U) || !FMath::IsFinite(V) || U < 0 || V < 0 || U >= MAX_int32 || V >= MAX_int32)
			{ Error = TEXT("Invalid raw height source UV offset."); return false; }
			const int32 OffsetX = int32(U), OffsetY = int32(V);
			if (OffsetX > Mip->Width - 64 || OffsetY > Mip->Height - 64)
			{ Error = TEXT("Raw component block exceeds copied mip bounds."); return false; }
			const FIntPoint Base = Component->GetSectionBase();
			for (int32 Y = 0; Y <= 63; ++Y)
			{
				for (int32 X = 0; X <= 63; ++X)
				{
					const int32 I = (Base.Y + Y - MinY) * Width + Base.X + X - MinX;
					if (!Flags.IsValidIndex(I)) { Error = TEXT("Raw B/A sample lies outside the retained rectangle."); return false; }
					const uint8* Pixel = Mip->Bytes.GetData() + 4 * (int64(OffsetY + Y) * Mip->Width + OffsetX + X);
					const uint8 B = Pixel[STRUCT_OFFSET(FColor, B)], A = Pixel[STRUCT_OFFSET(FColor, A)];
					if (Seen[I])
					{
						++Duplicates;
						if (Packed[I * 2] != A || Packed[I * 2 + 1] != B)
						{ Error = FString::Printf(TEXT("Raw B/A seam differs at (%d,%d)."), Base.X + X, Base.Y + Y); return false; }
					}
					else
					{
						Seen[I] = true; ++Unique;
						Packed[I * 2] = A; Packed[I * 2 + 1] = B;
						AlphaMasked[I * 2] = A & 0xFC; AlphaMasked[I * 2 + 1] = B; Flags[I] = A & 3;
					}
				}
			}
		}
		if (Unique != SampleCount || Duplicates != 4832) { Error = TEXT("Raw B/A coverage or seam count is incomplete."); return false; }
		TArray<int32> BCounts, ACounts, AlphaCounts, FlagCounts, PackedCounts;
		BCounts.Init(0, 256); ACounts.Init(0, 256); AlphaCounts.Init(0, 16384); FlagCounts.Init(0, 4); PackedCounts.Init(0, 65536);
		int32 Minimum = 65535, Maximum = 0;
		for (int32 I = 0; I < SampleCount; ++I)
		{
			const uint8 A = Packed[I * 2], B = Packed[I * 2 + 1]; const uint16 Value = (uint16(B) << 8) | A;
			++BCounts[B]; ++ACounts[A]; ++AlphaCounts[Value >> 2]; ++FlagCounts[Value & 3]; ++PackedCounts[Value];
			Minimum = FMath::Min(Minimum, int32(Value)); Maximum = FMath::Max(Maximum, int32(Value));
		}
		int32 Distinct = 0; FBlake3 HistogramHash;
		for (int32 Count : PackedCounts) { Distinct += Count != 0; HashUInt(HistogramHash, uint32(Count)); }
		FBlake3 Linked; HashText(Linked, LayerMetadata); HashText(Linked, Layer->GetGuid().ToString()); Linked.Update(Packed.GetData(), Packed.Num());
		Row->SetStringField(TEXT("packed_ba_digest"), LexToString(FBlake3::HashBuffer(Packed.GetData(), Packed.Num())));
		Row->SetStringField(TEXT("metadata_linked_digest"), LexToString(Linked.Finalize()));
		Row->SetStringField(TEXT("alpha_masked_uint16le_digest"), LexToString(FBlake3::HashBuffer(AlphaMasked.GetData(), AlphaMasked.Num())));
		Row->SetStringField(TEXT("flags_uint8_digest"), LexToString(FBlake3::HashBuffer(Flags.GetData(), Flags.Num())));
		Row->SetStringField(TEXT("packed_histogram_uint32le_digest"), LexToString(HistogramHash.Finalize()));
		Row->SetArrayField(TEXT("b_histogram"), Histogram(BCounts)); Row->SetArrayField(TEXT("a_histogram"), Histogram(ACounts));
		Row->SetArrayField(TEXT("alpha14_histogram"), Histogram(AlphaCounts)); Row->SetArrayField(TEXT("flags_histogram"), Histogram(FlagCounts));
		Row->SetNumberField(TEXT("packed_minimum"), Minimum); Row->SetNumberField(TEXT("packed_maximum"), Maximum);
		Row->SetNumberField(TEXT("packed_distinct_values"), Distinct); Row->SetBoolField(TEXT("packed_is_constant"), Distinct == 1);
		Row->SetNumberField(TEXT("sample_count"), Unique); Row->SetNumberField(TEXT("byte_count"), Packed.Num());
		Row->SetNumberField(TEXT("matching_seam_duplicates"), Duplicates); Row->SetBoolField(TEXT("complete"), true);
		return true;
	}

	bool HasNativeModule(const UObject* Object, const TCHAR* Prefix)
	{
		for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetOutermost()->GetName().StartsWith(Prefix)) { return true; }
		}
		return false;
	}

	bool BelongsToWorld(const UObject* Object, const UWorld* World)
	{
		return Object == World || Object->GetTypedOuter<UWorld>() == World || IsPackageInWorld(Object->GetPackage(), CandidateMap);
	}

	struct FReferenceRow
	{
		FString Source, Property, Kind, Target, State;
		bool bSourceInCandidate = false, bTerrainInternal = false;
		FString SortKey() const { return Source + TEXT("\n") + Property + TEXT("\n") + Kind + TEXT("\n") + Target + TEXT("\n") + State; }
	};

	struct FReferenceScan
	{
		UWorld* World = nullptr;
		TSet<const UObject*> Targets;
		TSet<FString> TargetPaths;
		TArray<FString> TargetRoots;
		TArray<FReferenceRow> Rows;
		TArray<FString> Unsupported;
		TMap<FString, int32> UnsupportedKinds;
		TMap<FString, int32> SupportedKinds;
		TMap<const FProperty*, bool> BearingCache;
		TMap<FGuid, const UObject*> LoadedGuidObjects;
		TSet<FGuid> AmbiguousLoadedGuids;
		TArray<TPair<const UScriptStruct*, const void*>> ActivePayloads;
		int32 ValuesVisited = 0, ObjectsScanned = 0, UnresolvedHandles = 0;
		bool bTruncated = false;
		void NoteUnsupported(const FProperty* Property, const TCHAR* Kind)
		{
			Unsupported.AddUnique(Property->GetPathName() + TEXT(": ") + Kind);
			++UnsupportedKinds.FindOrAdd(Kind);
		}
		void IndexLoadedGuids(const TArray<const UObject*>& Objects)
		{
			LoadedGuidObjects.Reset(); AmbiguousLoadedGuids.Reset();
			for (const UObject* Object : Objects)
			{
				if (!IsValid(Object)) { continue; }
				// This constructor only reads an existing annotation. Never create an ID,
				// call Lazy.Get/Resolve, or change an object's GUID/package during inspection.
				const FUniqueObjectGuid ExistingGuid(Object);
				if (!ExistingGuid.IsValid()) { continue; }
				const FGuid Guid = ExistingGuid.GetGuid();
				if (const UObject* const* Previous = LoadedGuidObjects.Find(Guid))
				{
					if (*Previous != Object) { AmbiguousLoadedGuids.Add(Guid); }
				}
				else { LoadedGuidObjects.Add(Guid, Object); }
			}
		}
		static const UScriptStruct* DynamicStorageBase(const UScriptStruct* Struct)
		{
			// Exact canonical base names avoid substring false positives, while the
			// already-loaded super chain includes native wrappers such as Niagara's
			// FInstancedStruct subclasses. No StaticStruct registration or asset load.
			for (const UStruct* Current = Struct; Current; Current = Current->GetSuperStruct())
			{
				const FString Type = Current->GetPathName();
				if (Type == TEXT("/Script/CoreUObject.InstancedStruct") || Type == TEXT("/Script/CoreUObject.SharedStruct") ||
					Type == TEXT("/Script/CoreUObject.ConstSharedStruct") || Type == TEXT("/Script/CoreUObject.InstancedPropertyBag"))
				{ return Cast<UScriptStruct>(Current); }
			}
			return nullptr;
		}

		void AddTarget(const UObject* Object)
		{
			if (Object) { Targets.Add(Object); TargetPaths.Add(Object->GetPathName()); }
		}
		bool Matches(const FString& Path) const
		{
			if (Path.IsEmpty()) { return false; }
			if (TargetPaths.Contains(Path)) { return true; }
			for (const FString& Root : TargetRoots) { if (Path.StartsWith(Root + TEXT("."))) { return true; } }
			return false;
		}
		bool ReferenceBearing(const FProperty* Property)
		{
			if (const bool* Cached = BearingCache.Find(Property)) { return *Cached; }
			// Conservative cycle value; actual values are bounded by depth and visit limits below.
			BearingCache.Add(Property, true);
			bool bBearing = CastField<FObjectPropertyBase>(Property) || CastField<FInterfaceProperty>(Property) ||
				CastField<FDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property) || CastField<FFieldPathProperty>(Property);
			if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				const FString Type = Struct->Struct->GetPathName();
				bBearing = Type == TEXT("/Script/CoreUObject.SoftObjectPath") || Type == TEXT("/Script/CoreUObject.SoftClassPath") ||
					Type == TEXT("/Script/CoreUObject.TopLevelAssetPath") || DynamicStorageBase(Struct->Struct) != nullptr;
				for (TFieldIterator<FProperty> It(Struct->Struct); It && !bBearing; ++It) { bBearing |= ReferenceBearing(*It); }
			}
			else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property)) { bBearing = ReferenceBearing(Array->Inner); }
			else if (const FSetProperty* Set = CastField<FSetProperty>(Property)) { bBearing = ReferenceBearing(Set->ElementProp); }
			else if (const FMapProperty* Map = CastField<FMapProperty>(Property)) { bBearing = ReferenceBearing(Map->KeyProp) || ReferenceBearing(Map->ValueProp); }
			else if (const FOptionalProperty* Optional = CastField<FOptionalProperty>(Property)) { bBearing = ReferenceBearing(Optional->GetValueProperty()); }
			if (!bBearing)
			{
				TArray<const FStructProperty*> Encountered;
				bBearing = Property->ContainsObjectReference(Encountered, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak | EPropertyObjectReferenceType::Soft);
			}
			BearingCache[Property] = bBearing;
			return bBearing;
		}
		void Add(const UObject* Source, const FString& Property, const TCHAR* Kind, const FString& Target, const TCHAR* State, bool bKeepAll)
		{
			if (Target.IsEmpty() || Target == TEXT("None") || (!bKeepAll && !Matches(Target))) { return; }
			if (Rows.Num() >= MaxReferenceRows) { bTruncated = true; return; }
			FReferenceRow Row;
			Row.Source = Source->GetPathName(); Row.Property = Property; Row.Kind = Kind; Row.Target = Target; Row.State = State;
			Row.bSourceInCandidate = BelongsToWorld(Source, World); Row.bTerrainInternal = Matches(Row.Source);
			Rows.Add(MoveTemp(Row));
		}
		void StructFields(const UObject* Source, const UScriptStruct* Type, const void* Data, const FString& Path, int32 Depth, bool bKeepAll, const UScriptStruct* StopBefore = nullptr)
		{
			for (TFieldIterator<FProperty> It(Type); It && !bTruncated; ++It)
			{
				if (It->GetOwnerStruct() == StopBefore) { break; }
				for (int32 Index = 0; Index < It->ArrayDim && !bTruncated; ++Index)
				{ Value(Source, *It, It->ContainerPtrToValuePtr<void>(Data, Index), Path + TEXT(".") + It->GetName() + FString::Printf(TEXT("[%d]"), Index), Depth + 1, bKeepAll); }
			}
		}
		void DynamicPayload(const UObject* Source, const FProperty* Property, const UScriptStruct* Type, const void* Data, const FString& Path, int32 Depth, bool bKeepAll)
		{
			if (!Type && !Data) { ++SupportedKinds.FindOrAdd(TEXT("empty_dynamic_storage")); return; }
			if (!Type || !Data) { NoteUnsupported(Property, TEXT("inconsistent_dynamic_storage")); return; }
			Add(Source, Path + TEXT(".$type"), TEXT("dynamic_struct_type"), Type->GetPathName(), TEXT("loaded_schema"), bKeepAll);
			const TPair<const UScriptStruct*, const void*> Key(Type, Data);
			if (ActivePayloads.Contains(Key))
			{
				++SupportedKinds.FindOrAdd(TEXT("dynamic_struct_cycle"));
				Add(Source, Path, TEXT("dynamic_struct_cycle"), Type->GetPathName(), TEXT("active_payload_back_edge"), true);
				return;
			}
			ActivePayloads.Add(Key);
			StructFields(Source, Type, Data, Path + TEXT(".$value"), Depth, bKeepAll);
			ActivePayloads.Pop(EAllowShrinking::No);
		}
		void Value(const UObject* Source, const FProperty* Property, const void* Data, const FString& Path, int32 Depth, bool bKeepAll)
		{
			if (!ReferenceBearing(Property) || bTruncated) { return; }
			if (Depth > MaxReferenceDepth || ++ValuesVisited > MaxReferenceValues) { bTruncated = true; return; }
			if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
			{ Add(Source, Path, TEXT("soft_object_path"), Soft->GetPropertyValue(Data).ToSoftObjectPath().ToString(), TEXT("not_resolved"), bKeepAll); }
			else if (const FWeakObjectProperty* Weak = CastField<FWeakObjectProperty>(Property))
			{ Add(Source, Path, TEXT("weak_object"), ObjectPath(Weak->GetPropertyValue(Data).Get()), TEXT("loaded_only"), bKeepAll); }
			else if (const FLazyObjectProperty* Lazy = CastField<FLazyObjectProperty>(Property))
			{
				const FLazyObjectPtr Copy = Lazy->GetPropertyValue(Data);
				const FGuid Guid = Copy.GetUniqueID().GetGuid();
				if (!Guid.IsValid()) { ++SupportedKinds.FindOrAdd(TEXT("lazy_guid_null")); }
				else if (AmbiguousLoadedGuids.Contains(Guid)) { NoteUnsupported(Property, TEXT("lazy_guid_ambiguous_loaded_identity")); }
				else if (const UObject* const* Target = LoadedGuidObjects.Find(Guid))
				{
					++SupportedKinds.FindOrAdd(TEXT("lazy_guid_loaded"));
					Add(Source, Path, TEXT("lazy_guid_loaded"), ObjectPath(*Target), TEXT("existing_guid_loaded_identity"), bKeepAll);
				}
				else
				{
					++SupportedKinds.FindOrAdd(TEXT("lazy_guid_unresolved"));
					Add(Source, Path, TEXT("lazy_guid_unresolved"), Guid.ToString(), TEXT("known_guid_not_loaded_no_path_match"), true);
				}
			}
			else if (const FObjectProperty* Object = CastField<FObjectProperty>(Property))
			{
				const TObjectPtr<UObject> Copy = Object->GetObjectPtrPropertyValue(Data);
				if (!Copy.IsResolved()) { ++UnresolvedHandles; }
				// GetPathName is explicitly non-resolving; do not call Get/Load on this handle.
				Add(Source, Path, TEXT("object_handle_path"), Copy.GetPathName(), Copy.IsResolved() ? TEXT("resolved_handle") : TEXT("unresolved_handle"), bKeepAll);
			}
			else if (const FInterfaceProperty* Interface = CastField<FInterfaceProperty>(Property))
			{
				FScriptInterface Copy = Interface->GetPropertyValue(Data);
				const TObjectPtr<UObject> Handle = Copy.GetObjectRef();
				if (!Handle.IsResolved()) { ++UnresolvedHandles; }
				Add(Source, Path, TEXT("interface_object_handle_path"), Handle.GetPathName(), Handle.IsResolved() ? TEXT("resolved_handle") : TEXT("unresolved_handle"), bKeepAll);
			}
			else if (const FDelegateProperty* Delegate = CastField<FDelegateProperty>(Property))
			{ Add(Source, Path, TEXT("delegate_object"), ObjectPath(Delegate->GetPropertyValue(Data).GetUObject()), TEXT("loaded_only"), bKeepAll); }
			else if (const FMulticastDelegateProperty* MulticastProperty = CastField<FMulticastDelegateProperty>(Property))
			{
				if (const FMulticastScriptDelegate* Multicast = MulticastProperty->GetMulticastDelegate(Data))
				{ for (const UObject* DelegateObject : Multicast->GetAllObjects()) { Add(Source, Path, TEXT("multicast_object"), ObjectPath(DelegateObject), TEXT("loaded_only"), bKeepAll); } }
			}
			else if (const FFieldPathProperty* Field = CastField<FFieldPathProperty>(Property))
			{
				// UE 5.8 has no non-resolving public owner getter. Copying into this
				// standard-C++ derived adapter reads protected storage without reinterpreting
				// the original or invoking ToString/GetTyped, which update resolution caches.
				struct FStoredFieldPath : FFieldPath
				{
					explicit FStoredFieldPath(const FFieldPath& In) : FFieldPath(In) {}
					const TWeakObjectPtr<UStruct>& Owner() const { return ResolvedOwner; }
					const TArray<FName>& Segments() const { return FFieldPath::Path; }
				};
				const FStoredFieldPath Copy(Field->GetPropertyValue(Data));
				if (const UStruct* Owner = Copy.Owner().Get())
				{
					++SupportedKinds.FindOrAdd(TEXT("field_path_owner"));
					Add(Source, Path, TEXT("field_path_owner"), Owner->GetPathName(), TEXT("loaded_owner_no_field_resolution"), bKeepAll);
				}
				else if (!Copy.Segments().IsEmpty())
				{
					TArray<FString> Segments;
					for (const FName& Segment : Copy.Segments()) { Segments.Add(Segment.ToString()); }
					++SupportedKinds.FindOrAdd(TEXT("field_path_unresolved_owner"));
					Add(Source, Path, TEXT("field_path_unresolved_owner"), FString::Join(Segments, TEXT("/")), TEXT("stored_segments_no_owner_resolution"), true);
				}
				else { ++SupportedKinds.FindOrAdd(TEXT("field_path_null")); }
			}
			else if (const FOptionalProperty* Optional = CastField<FOptionalProperty>(Property))
			{
				if (const void* Payload = Optional->GetValuePointerForReadIfSet(Data))
				{
					++SupportedKinds.FindOrAdd(TEXT("optional_set"));
					Value(Source, Optional->GetValueProperty(), Payload, Path + TEXT(".value"), Depth + 1, bKeepAll);
				}
				else { ++SupportedKinds.FindOrAdd(TEXT("optional_unset")); }
			}
			else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				const FString Type = Struct->Struct->GetPathName();
				const UScriptStruct* StorageBase = DynamicStorageBase(Struct->Struct);
				const FString StorageType = StorageBase ? StorageBase->GetPathName() : FString();
				if (Type == TEXT("/Script/CoreUObject.SoftObjectPath") || Type == TEXT("/Script/CoreUObject.SoftClassPath"))
				{ Add(Source, Path, TEXT("soft_path_struct"), static_cast<const FSoftObjectPath*>(Data)->ToString(), TEXT("not_resolved"), bKeepAll); }
				else if (Type == TEXT("/Script/CoreUObject.TopLevelAssetPath"))
				{ Add(Source, Path, TEXT("top_level_asset_path"), static_cast<const FTopLevelAssetPath*>(Data)->ToString(), TEXT("not_resolved"), bKeepAll); }
				else if (StorageType == TEXT("/Script/CoreUObject.InstancedStruct"))
				{
					++SupportedKinds.FindOrAdd(TEXT("instanced_struct"));
					const FInstancedStruct& Payload = *static_cast<const FInstancedStruct*>(Data);
					DynamicPayload(Source, Property, Payload.GetScriptStruct(), Payload.GetMemory(), Path, Depth, bKeepAll);
				}
				else if (StorageType == TEXT("/Script/CoreUObject.SharedStruct"))
				{
					++SupportedKinds.FindOrAdd(TEXT("shared_struct"));
					const FSharedStruct& Payload = *static_cast<const FSharedStruct*>(Data);
					DynamicPayload(Source, Property, Payload.GetScriptStruct(), Payload.GetMemory(), Path, Depth, bKeepAll);
				}
				else if (StorageType == TEXT("/Script/CoreUObject.ConstSharedStruct"))
				{
					++SupportedKinds.FindOrAdd(TEXT("const_shared_struct"));
					const FConstSharedStruct& Payload = *static_cast<const FConstSharedStruct*>(Data);
					DynamicPayload(Source, Property, Payload.GetScriptStruct(), Payload.GetMemory(), Path, Depth, bKeepAll);
				}
				else if (StorageType == TEXT("/Script/CoreUObject.InstancedPropertyBag"))
				{
					++SupportedKinds.FindOrAdd(TEXT("instanced_property_bag"));
					const FConstStructView View = static_cast<const FInstancedPropertyBag*>(Data)->GetValue();
					DynamicPayload(Source, Property, View.GetScriptStruct(), View.GetMemory(), Path, Depth, bKeepAll);
				}
				else
				{
					StructFields(Source, Struct->Struct, Data, Path, Depth, bKeepAll);
				}
				if (StorageBase && StorageBase != Struct->Struct)
				{ StructFields(Source, Struct->Struct, Data, Path, Depth, bKeepAll, StorageBase); }
			}
			else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(Array, Data);
				for (int32 Index = 0; Index < Helper.Num() && !bTruncated; ++Index)
				{ Value(Source, Array->Inner, Helper.GetRawPtr(Index), Path + FString::Printf(TEXT("[%d]"), Index), Depth + 1, bKeepAll); }
			}
			else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper Helper(Set, Data);
				for (int32 Index = 0; Index < Helper.GetMaxIndex() && !bTruncated; ++Index)
				{ if (Helper.IsValidIndex(Index)) { Value(Source, Set->ElementProp, Helper.GetElementPtr(Index), Path + FString::Printf(TEXT("{slot:%d}"), Index), Depth + 1, bKeepAll); } }
			}
			else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(Map, Data);
				for (int32 Index = 0; Index < Helper.GetMaxIndex() && !bTruncated; ++Index)
				{
					if (!Helper.IsValidIndex(Index)) { continue; }
					const FString Slot = Path + FString::Printf(TEXT("{slot:%d}"), Index);
					Value(Source, Map->KeyProp, Helper.GetKeyPtr(Index), Slot + TEXT(".key"), Depth + 1, bKeepAll);
					Value(Source, Map->ValueProp, Helper.GetValuePtr(Index), Slot + TEXT(".value"), Depth + 1, bKeepAll);
				}
			}
			else { NoteUnsupported(Property, TEXT("unsupported_reference_bearing_property_type")); }
		}
		void Scan(const UObject* Object, bool bKeepAll)
		{
			++ObjectsScanned;
			for (TFieldIterator<FProperty> It(Object->GetClass()); It && !bTruncated; ++It)
			{
				for (int32 Index = 0; Index < It->ArrayDim; ++Index)
				{ Value(Object, *It, It->ContainerPtrToValuePtr<void>(Object, Index), It->GetName() + FString::Printf(TEXT("[%d]"), Index), 0, bKeepAll); }
			}
		}
	};

	FString Inspect()
	{
		if (!IsInGameThread()) { return TEXT("{\"success\":false,\"error\":\"Dependency inspection requires the editor game thread.\"}"); }
		const FDirtySnapshot DirtyBefore = CaptureDirtyPackages();
		const TSharedRef<FJsonObject> Result = NewResult(TEXT("inspect_candidate_dependencies"), CandidateMap);
		Result->SetStringField(TEXT("digest_algorithm"), TEXT("BLAKE3"));
		Result->SetBoolField(TEXT("read_only_inspection"), true);
		Result->SetBoolField(TEXT("preservation_ready"), false);
		Result->SetBoolField(TEXT("project_wide_dependency_complete"), false);
		Result->SetBoolField(TEXT("crop_reference_safety_verified"), false);
		Result->SetStringField(TEXT("scope"), TEXT("Raw retained B/A source; all current-world actor descriptors; process-loaded reflected references. No AssetRegistry/on-disk package scan or native/custom serialization reference walk."));
		Result->SetStringField(TEXT("native_ba_evidence"), TEXT("LandscapeEditInterface.cpp:419-424,846-855; LandscapeEditLayerTypes.h:EHeightmapAlphaFlags; LandscapeCommon.ush:PackHeightAlpha/UnpackHeightAlpha. masked=((B<<8)|A)&0xFFFC; alpha14=packed>>2; flags=A&3 (0 Additive,1 Min,2 Max,3 AlphaBlend)."));
		Result->SetStringField(TEXT("native_default_boundary"), TEXT("No universal B/A default. SetHeightData uses flags=3 only when alpha input is supplied without flags; ClearEditLayer alpha-blend branch uses alpha=65535,flags=0 (stored packed=65532). Base GetBlendMode is additive; custom/subclass render usage is not inferred from raw bytes."));
		Result->SetBoolField(TEXT("reads_raw_layer_textures_only"), true);
		UWorld* World = nullptr; FString Error;
		if (!ResolveEditorWorld(CandidateMap, World, Result, Error)) { return Finish(Result, DirtyBefore, false, Error); }
		const FEditorReadState EditorBefore = CaptureEditorState();
		FTerrainState State; CollectTerrain(World, Result, State);
		auto Complete = [&](bool bSuccess, FString Message)
		{
			const bool bStable = RecordEditorStability(EditorBefore, Result);
			if (!bStable) { Message += TEXT(" Editor object/selection/transaction state changed during inspection."); }
			return Finish(Result, DirtyBefore, bSuccess && bStable, Message);
		};
		if (!ValidateLayout(World, State, Error)) { return Complete(false, Error); }
		const FGuid EditingBefore = State.Main->GetEditingLayer();
		const TSet<ULandscapeComponent*> SelectionBefore = State.Info->GetSelectedComponents();
		const TArray<const ULandscapeEditLayerBase*> Layers = State.Main->GetEditLayersConst();
		TArray<ULandscapeLayerInfoObject*> NoPaints; FString MetadataBefore;
		if (Layers.IsEmpty() || !Metadata(State, NoPaints, MetadataBefore, Error)) { return Complete(false, TEXT("Missing/invalid layer metadata: ") + Error); }
		Result->SetStringField(TEXT("layer_metadata_digest"), MetadataBefore);
		TArray<const ULandscapeComponent*> Components;
		for (const ULandscapeComponent* Component : State.Components) { if (IsKeptBase(Component->GetSectionBase())) { Components.Add(Component); } }
		Components.Sort([](const ULandscapeComponent& A, const ULandscapeComponent& B)
		{ const FIntPoint P = A.GetSectionBase(), Q = B.GetSectionBase(); return P.Y == Q.Y ? P.X < Q.X : P.Y < Q.Y; });
		if (Components.Num() != 45) { return Complete(false, TEXT("Exactly 45 retained components are required.")); }
		FMipCache Cache; TArray<FString> Errors; TArray<TSharedPtr<FJsonValue>> RawRows, BrushRows, OwnerRows; bool bRawBAComplete = true;
		FReferenceScan Scan; Scan.World = World;
		for (const ALandscapeProxy* Proxy : State.Proxies) { Scan.AddTarget(Proxy); Scan.TargetRoots.Add(Proxy->GetPathName()); }
		for (const ULandscapeComponent* Component : State.Components) { Scan.AddTarget(Component); Scan.AddTarget(Component->GetCollisionComponent()); }
		TSet<FGuid> LayerGuids;
		for (int32 Index = 0; Index < Layers.Num(); ++Index)
		{
			const ULandscapeEditLayerBase* Layer = Layers[Index]; Scan.AddTarget(Layer);
			if (!Layer->GetGuid().IsValid() || LayerGuids.Contains(Layer->GetGuid())) { bRawBAComplete = false; Errors.Add(TEXT("Invalid/duplicate raw layer GUID.")); continue; }
			LayerGuids.Add(Layer->GetGuid());
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>(); RawRows.Add(MakeShared<FJsonValueObject>(Row));
			Error.Reset();
			if (!ReadRawBA(Components, Layer, Cache, MetadataBefore, Row, Error))
			{ bRawBAComplete = false; Row->SetBoolField(TEXT("complete"), false); Row->SetStringField(TEXT("error"), Error); Errors.Add(Error); }
			const FLandscapeLayer* LegacyLayer = State.Main->GetLayerConst(Index);
			bool bBrushHandlesReadable = LegacyLayer && LegacyLayer->EditLayer.GetPathName() == Layer->GetPathName();
			if (bBrushHandlesReadable && !LegacyLayer->Brushes.IsEmpty())
			{
				const FObjectProperty* BrushProperty = FindFProperty<FObjectProperty>(FLandscapeLayerBrush::StaticStruct(), TEXT("BlueprintBrush"));
				bBrushHandlesReadable = BrushProperty != nullptr;
				if (BrushProperty)
				{
					for (const FLandscapeLayerBrush& Entry : LegacyLayer->Brushes)
					{
						const TObjectPtr<UObject> Copy = BrushProperty->GetObjectPtrPropertyValue(BrushProperty->ContainerPtrToValuePtr<void>(&Entry));
						if (!Copy.IsResolved())
						{
							bBrushHandlesReadable = false;
							const TSharedRef<FJsonObject> Unresolved = MakeShared<FJsonObject>();
							Unresolved->SetStringField(TEXT("layer_guid"), Layer->GetGuid().ToString()); Unresolved->SetStringField(TEXT("path"), Copy.GetPathName());
							Unresolved->SetStringField(TEXT("status"), TEXT("unresolved_brush_not_loaded")); BrushRows.Add(MakeShared<FJsonValueObject>(Unresolved));
						}
					}
				}
			}
			if (!bBrushHandlesReadable) { Errors.Add(TEXT("Layer brush handles/metadata cannot be read without resolving objects.")); continue; }
			for (const ALandscapeBlueprintBrushBase* Brush : State.Main->GetBrushesForLayer(Index))
			{
				const TSharedRef<FJsonObject> BrushRow = MakeShared<FJsonObject>(); BrushRows.Add(MakeShared<FJsonValueObject>(BrushRow));
				BrushRow->SetStringField(TEXT("layer_guid"), Layer->GetGuid().ToString()); BrushRow->SetNumberField(TEXT("layer_index"), Index);
				BrushRow->SetBoolField(TEXT("valid"), IsValid(Brush));
				if (!IsValid(Brush)) { Errors.Add(TEXT("An edit-layer brush is invalid.")); continue; }
				Scan.AddTarget(Brush); Scan.TargetRoots.Add(Brush->GetPathName());
				BrushRow->SetStringField(TEXT("path"), Brush->GetPathName()); BrushRow->SetStringField(TEXT("class"), Brush->GetClass()->GetPathName());
				BrushRow->SetStringField(TEXT("owner"), StoredObjectPath(Brush, TEXT("Owner"))); BrushRow->SetStringField(TEXT("owning_landscape"), StoredObjectPath(Brush, TEXT("OwningLandscape")));
				BrushRow->SetStringField(TEXT("world"), ObjectPath(Brush->GetWorld())); BrushRow->SetStringField(TEXT("level"), ObjectPath(Brush->GetLevel()));
				BrushRow->SetBoolField(TEXT("belongs_to_candidate"), Brush->GetWorld() == World && IsPackageInWorld(Brush->GetPackage(), CandidateMap));
				BrushRow->SetBoolField(TEXT("visible"), Brush->IsVisible());
				BrushRow->SetBoolField(TEXT("can_affect_height"), Brush->CanAffectHeightmap()); BrushRow->SetBoolField(TEXT("can_affect_weight"), Brush->CanAffectWeightmap());
			}
		}
		for (const TPair<UTexture2D*, TSharedPtr<FMipCopy>>& Pair : Cache) { Scan.AddTarget(Pair.Key); }
		Result->SetArrayField(TEXT("raw_edit_layer_ba"), RawRows); Result->SetArrayField(TEXT("layer_brushes"), BrushRows);
		Result->SetBoolField(TEXT("raw_ba_complete"), bRawBAComplete);
		Result->SetNumberField(TEXT("retained_sample_count"), SampleCount); Result->SetNumberField(TEXT("packed_ba_bytes_per_layer"), SampleCount * 2);

		int32 DescriptorCount = 0, LoadedDescriptors = 0; TArray<FString> UnloadedPaths; TArray<TSharedPtr<FJsonValue>> DescriptorReferences;
		TSet<FGuid> TerrainActorGuids;
		for (const ALandscapeProxy* Proxy : State.Proxies) { TerrainActorGuids.Add(Proxy->GetActorGuid()); }
		TMap<FGuid, TSet<FString>> LoadedActorPaths;
		for (TActorIterator<AActor> It(World); It; ++It) { LoadedActorPaths.FindOrAdd(It->GetActorGuid()).Add(It->GetPathName()); }
		FWorldPartitionHelpers::ForEachActorDescInstance<AActor>(World->GetWorldPartition(), [&](const FWorldPartitionActorDescInstance* Descriptor)
		{
			++DescriptorCount;
			const FString ActorPath = Descriptor->GetActorSoftPath().ToString();
			const TSet<FString>* LoadedPaths = LoadedActorPaths.Find(Descriptor->GetGuid());
			// GetActor/IsLoaded can flush async loading or update the descriptor's cached ActorPtr.
			// Match already-enumerated world actors by both GUID and exact full path instead.
			if (LoadedPaths && LoadedPaths->Contains(ActorPath)) { ++LoadedDescriptors; } else { UnloadedPaths.Add(ActorPath); }
			for (const FGuid& Reference : Descriptor->GetReferences())
			{
				if (!TerrainActorGuids.Contains(Reference)) { continue; }
				const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("actor_path"), Descriptor->GetActorSoftPath().ToString());
				Row->SetStringField(TEXT("referenced_terrain_actor_guid"), Reference.ToString()); DescriptorReferences.Add(MakeShared<FJsonValueObject>(Row));
			}
			return true;
		});
		UnloadedPaths.Sort(); Result->SetNumberField(TEXT("all_world_actor_descriptor_count"), DescriptorCount);
		Result->SetNumberField(TEXT("loaded_world_actor_descriptor_count"), LoadedDescriptors);
		Result->SetNumberField(TEXT("unloaded_world_actor_descriptor_count"), UnloadedPaths.Num());
		Result->SetArrayField(TEXT("unloaded_world_actor_paths"), StringArray(UnloadedPaths));
		Result->SetBoolField(TEXT("all_world_actor_descriptors_loaded"), DescriptorCount > 0 && UnloadedPaths.IsEmpty());
		Result->SetStringField(TEXT("actor_descriptor_loaded_evidence"), TEXT("GUID plus exact full soft path matched against TActorIterator<AActor> of already-loaded candidate actors; no descriptor GetActor/IsLoaded/async flush."));
		Result->SetArrayField(TEXT("descriptor_direct_terrain_actor_references"), DescriptorReferences);

		int32 CandidateActors = 0, PCGOwners = 0, FoliageOwners = 0, SplineOwners = 0, BrushOwners = 0;
		TArray<const UObject*> LoadedObjects; EditorBefore.ObjectFlags.GetKeys(LoadedObjects);
		LoadedObjects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
		Scan.IndexLoadedGuids(LoadedObjects);
		for (const UObject* Object : LoadedObjects)
		{
			if (!IsValid(Object) || Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) { continue; }
			const bool bCandidate = BelongsToWorld(Object, World);
			const bool bPCG = bCandidate && HasNativeModule(Object, TEXT("/Script/PCG"));
			const bool bFoliage = bCandidate && HasNativeModule(Object, TEXT("/Script/Foliage"));
			const bool bSpline = bCandidate && (Object->IsA<ULandscapeSplinesComponent>() || Object->IsA<ALandscapeSplineActor>());
			const bool bBrush = bCandidate && Object->IsA<ALandscapeBlueprintBrushBase>();
			if (bCandidate && Object->IsA<AActor>()) { ++CandidateActors; }
			PCGOwners += bPCG; FoliageOwners += bFoliage; SplineOwners += bSpline; BrushOwners += bBrush;
			if (bPCG || bFoliage || bSpline || bBrush)
			{
				const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>(); OwnerRows.Add(MakeShared<FJsonValueObject>(Row));
				Row->SetStringField(TEXT("path"), Object->GetPathName()); Row->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
				Row->SetStringField(TEXT("outer"), ObjectPath(Object->GetOuter()));
				Row->SetBoolField(TEXT("pcg_class_hierarchy"), bPCG); Row->SetBoolField(TEXT("foliage_class_hierarchy"), bFoliage);
				Row->SetBoolField(TEXT("spline_owner"), bSpline); Row->SetBoolField(TEXT("blueprint_brush"), bBrush);
				if (const ULandscapeSplinesComponent* Splines = Cast<ULandscapeSplinesComponent>(Object))
				{ Row->SetNumberField(TEXT("control_point_count"), Splines->GetControlPoints().Num()); Row->SetNumberField(TEXT("segment_count"), Splines->GetSegments().Num()); }
			}
			if (!Scan.bTruncated) { Scan.Scan(Object, bPCG || bFoliage || bSpline || bBrush); }
		}
		Result->SetArrayField(TEXT("loaded_dependency_owner_objects"), OwnerRows);
		Result->SetNumberField(TEXT("loaded_candidate_actor_count"), CandidateActors);
		Result->SetNumberField(TEXT("loaded_candidate_pcg_hierarchy_objects"), PCGOwners);
		Result->SetNumberField(TEXT("loaded_candidate_foliage_hierarchy_objects"), FoliageOwners);
		Result->SetNumberField(TEXT("loaded_candidate_spline_owner_objects"), SplineOwners);
		Result->SetNumberField(TEXT("loaded_candidate_blueprint_brush_objects"), BrushOwners);
		Result->SetBoolField(TEXT("loaded_candidate_dependency_owner_scan_complete"), true);
		Result->SetStringField(TEXT("foliage_native_base_reference_coverage"), FoliageOwners == 0 ? TEXT("no_loaded_owner_objects_observed") : TEXT("unsupported_native_unreflected_fields"));
		Result->SetStringField(TEXT("foliage_native_limitation"), TEXT("InstanceBaseCache, FoliageInfos and FFoliageInfo::Instances/BaseId/BaseComponent include non-UPROPERTY native state. Reflection is not complete foliage-base reference coverage; no Foliage module or typed data access was added."));
		Result->SetStringField(TEXT("pcg_native_reference_coverage"), PCGOwners == 0 ? TEXT("no_loaded_owner_objects_observed") : TEXT("reflected_only_native_and_custom_serialization_unsupported"));
		Result->SetStringField(TEXT("spline_native_reference_coverage"), SplineOwners == 0 ? TEXT("no_loaded_owner_objects_observed") : TEXT("public_counts_and_reflected_fields_only"));
		Result->SetStringField(TEXT("brush_native_reference_coverage"), BrushOwners == 0 ? TEXT("no_loaded_owner_objects_observed") : TEXT("public_owner_getters_and_reflected_fields_only"));
		Result->SetStringField(TEXT("zero_owner_boundary"), TEXT("Zero means no current candidate owner object in the process-loaded class hierarchy, not no dependency in assets, other maps, unloaded objects, or native registries."));
		Scan.Rows.Sort([](const FReferenceRow& A, const FReferenceRow& B) { return A.SortKey() < B.SortKey(); });
		TArray<TSharedPtr<FJsonValue>> ReferenceRows; FBlake3 ReferenceHash;
		for (const FReferenceRow& Reference : Scan.Rows)
		{
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>(); ReferenceRows.Add(MakeShared<FJsonValueObject>(Row));
			Row->SetStringField(TEXT("source"), Reference.Source); Row->SetStringField(TEXT("property_path"), Reference.Property);
			Row->SetStringField(TEXT("kind"), Reference.Kind); Row->SetStringField(TEXT("target_full_object_path"), Reference.Target); Row->SetStringField(TEXT("state"), Reference.State);
			Row->SetBoolField(TEXT("source_in_candidate"), Reference.bSourceInCandidate); Row->SetBoolField(TEXT("source_is_terrain_internal"), Reference.bTerrainInternal);
			HashText(ReferenceHash, Reference.SortKey());
		}
		Result->SetArrayField(TEXT("loaded_reflected_references"), ReferenceRows);
		Result->SetStringField(TEXT("loaded_reflected_reference_digest"), LexToString(ReferenceHash.Finalize()));
		Result->SetNumberField(TEXT("reference_objects_scanned"), Scan.ObjectsScanned); Result->SetNumberField(TEXT("reference_values_visited"), Scan.ValuesVisited);
		Result->SetNumberField(TEXT("unresolved_object_handles_observed"), Scan.UnresolvedHandles);
		Result->SetBoolField(TEXT("reference_scan_truncated"), Scan.bTruncated); Result->SetNumberField(TEXT("reference_value_limit"), MaxReferenceValues);
		Scan.Unsupported.Sort(); Result->SetArrayField(TEXT("unsupported_reflected_properties"), StringArray(Scan.Unsupported));
		TArray<FString> UnsupportedKindNames; Scan.UnsupportedKinds.GetKeys(UnsupportedKindNames); UnsupportedKindNames.Sort();
		const TSharedRef<FJsonObject> UnsupportedCounts = MakeShared<FJsonObject>();
		for (const FString& Kind : UnsupportedKindNames) { UnsupportedCounts->SetNumberField(Kind, Scan.UnsupportedKinds[Kind]); }
		Result->SetObjectField(TEXT("unsupported_reflected_property_kind_counts"), UnsupportedCounts);
		const TSharedRef<FJsonObject> SupportedCounts = MakeShared<FJsonObject>();
		TArray<FString> SupportedKindNames; Scan.SupportedKinds.GetKeys(SupportedKindNames); SupportedKindNames.Sort();
		for (const FString& Kind : SupportedKindNames) { SupportedCounts->SetNumberField(Kind, Scan.SupportedKinds[Kind]); }
		Result->SetObjectField(TEXT("supported_reflected_storage_value_counts"), SupportedCounts);
		Result->SetNumberField(TEXT("loaded_existing_guid_count"), Scan.LoadedGuidObjects.Num());
		Result->SetNumberField(TEXT("ambiguous_loaded_guid_count"), Scan.AmbiguousLoadedGuids.Num());
		TArray<FString> TargetNames = Scan.TargetPaths.Array(); TargetNames.Sort();
		Result->SetArrayField(TEXT("reference_target_paths"), StringArray(TargetNames));
		Result->SetNumberField(TEXT("reference_target_count"), TargetNames.Num());
		Result->SetBoolField(TEXT("loaded_reflection_walk_complete"), !Scan.bTruncated && Scan.Unsupported.IsEmpty());
		Result->SetStringField(TEXT("reference_scope_limitations"), TEXT("Loaded UObject UPROPERTY paths including arrays/maps/sets/structs, optional values, stored FieldPath owners, reflected delegates and known dynamic struct/property-bag payloads only. Lazy GUIDs and FieldPath segments with no loaded identity are preserved as unresolved rows; their target paths are not certified. No native AddReferencedObjects/custom Serialize, string-built paths, unloaded packages, AssetRegistry, or transitive dependency closure. CDO/archetypes excluded. Map/set slots are observation indices, not persistent identities. Dynamic payload cycles are represented as back edges. No object is classified as safe to delete."));
		bool bSourcesStable = true;
		for (const TPair<UTexture2D*, TSharedPtr<FMipCopy>>& Pair : Cache)
		{
			bSourcesStable &= Pair.Key->Source.GetId() == Pair.Value->SourceId && Pair.Key->GetPackage()->IsDirty() == Pair.Value->bDirty && Pair.Key->HasAnyFlags(RF_Transactional) == Pair.Value->bTransactional;
		}
		FString MetadataAfter; Error.Reset(); const bool bMetadataStable = Metadata(State, NoPaints, MetadataAfter, Error) && MetadataAfter == MetadataBefore;
		const TSet<ULandscapeComponent*> SelectionAfter = State.Info->GetSelectedComponents();
		const bool bLayerSelectionStable = SelectionBefore.Num() == SelectionAfter.Num() && SelectionBefore.Difference(SelectionAfter).IsEmpty() && State.Main->GetEditingLayer() == EditingBefore;
		Result->SetBoolField(TEXT("texture_sources_unchanged"), bSourcesStable); Result->SetBoolField(TEXT("layer_metadata_unchanged"), bMetadataStable);
		Result->SetBoolField(TEXT("landscape_selection_and_editing_layer_unchanged"), bLayerSelectionStable); Result->SetNumberField(TEXT("cached_texture_count"), Cache.Num());
		if (!bSourcesStable || !bMetadataStable || !bLayerSelectionStable) { Errors.Add(TEXT("Texture/layer/selection state changed during read.")); }
		if (Scan.bTruncated || !Scan.Unsupported.IsEmpty()) { Errors.Add(TEXT("The loaded reflection walk was truncated or contains unsupported properties.")); }
		Result->SetArrayField(TEXT("inspection_errors"), StringArray(Errors));
		return Complete(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | ")));
	}
}
#endif

FString URogue10mCrop20261114WorldAuthoring::InspectCandidateDependencies()
{
#if WITH_EDITOR
	return Rogue10mCrop20261114Dependencies::Inspect();
#else
	return TEXT("{\"success\":false,\"error\":\"Candidate dependency inspection is available only in an editor build.\"}");
#endif
}

#if WITH_EDITOR
#include "Misc/Base64.h"
#include "Misc/DateTime.h"

namespace Rogue10mCrop20261114RawTiles
{
	using namespace Rogue10mCrop20261114WorldAuthoring;
	using namespace Rogue10mCrop20261114RetainedSnapshot;
	constexpr int32 TileSize = 64;
	constexpr int32 TileSamples = TileSize * TileSize;

	TArray<TSharedPtr<FJsonValue>> PairArray(int32 X, int32 Y)
	{
		return {MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y)};
	}

	struct FTileChannel
	{
		FGuid LayerGuid;
		FString Kind;
		ULandscapeLayerInfoObject* Paint = nullptr;
	};

	FString PaintConfiguration(const TArray<ULandscapeLayerInfoObject*>& Paints)
	{
		FBlake3 Hash;
		for (const ULandscapeLayerInfoObject* Paint : Paints)
		{
			HashText(Hash, Paint->GetPathName()); HashUInt(Hash, uint32(Paint->GetBlendMethod())); HashText(Hash, Paint->GetBlendGroup().ToString());
		}
		return LexToString(Hash.Finalize());
	}

	bool Tile(const ULandscapeComponent* Component, const FTileChannel& Channel, FMipCache& Cache,
		TMap<UTexture2D*, FString>& SourceDigests, TArray<uint8>& Bytes, const TSharedRef<FJsonObject>& Row, FString& Error)
	{
		const bool bPaint = Channel.Paint != nullptr;
		const bool bHeightRG = Channel.Kind == TEXT("height_rg");
		const bool bRawRequired = Channel.LayerGuid.IsValid();
		const bool bHasRaw = bRawRequired && Component->GetLayerData(Channel.LayerGuid) != nullptr;
		const FIntPoint Base = Component->GetSectionBase();
		Row->SetArrayField(TEXT("sectionBase"), PairArray(Base.X, Base.Y)); Row->SetArrayField(TEXT("size"), PairArray(TileSize, TileSize));
		Row->SetStringField(TEXT("componentPath"), Component->GetPathName()); Row->SetStringField(TEXT("proxyPath"), Component->GetLandscapeProxy()->GetPathName());
		Row->SetBoolField(TEXT("rawLayerRequired"), bRawRequired); Row->SetBoolField(TEXT("hasLayerData"), bHasRaw);
		if (bRawRequired && !bHasRaw) { Error = TEXT("Missing requested raw component layer; no final/composite fallback was used."); return false; }
		UTexture2D* Texture = nullptr;
		int32 PaintMatches = 0, AllocatedMatches = 0, TextureIndex = INDEX_NONE, ChannelIndex = INDEX_NONE;
		if (bPaint)
		{
			for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(Channel.LayerGuid))
			{
				if (Allocation.LayerInfo != Channel.Paint) { continue; }
				++PaintMatches;
				if (!Allocation.IsAllocated()) { continue; }
				++AllocatedMatches; TextureIndex = Allocation.WeightmapTextureIndex; ChannelIndex = Allocation.WeightmapTextureChannel;
			}
			if (PaintMatches > 1 || AllocatedMatches > 1) { Error = TEXT("Duplicate/ambiguous paint allocation."); return false; }
			if (AllocatedMatches == 1)
			{
				const TArray<UTexture2D*>& Textures = Component->GetWeightmapTextures(Channel.LayerGuid);
				if (!Textures.IsValidIndex(TextureIndex) || ChannelIndex < 0 || ChannelIndex > 3 || !IsValid(Textures[TextureIndex]))
				{ Error = TEXT("Allocated paint channel has an invalid texture index/channel/source."); return false; }
				Texture = Textures[TextureIndex];
			}
		}
		else { Texture = Component->GetHeightmap(Channel.LayerGuid); }
		const bool bSemanticZero = bPaint && AllocatedMatches == 0;
		Row->SetBoolField(TEXT("paintAllocationPresent"), PaintMatches == 1); Row->SetBoolField(TEXT("paintAllocated"), AllocatedMatches == 1);
		Row->SetNumberField(TEXT("paintTextureIndex"), TextureIndex); Row->SetNumberField(TEXT("paintTextureChannel"), ChannelIndex);
		Row->SetBoolField(TEXT("sourceTexturePresent"), IsValid(Texture)); Row->SetBoolField(TEXT("semanticZeroFromUnallocatedPaint"), bSemanticZero);
		Row->SetStringField(TEXT("presence"), bSemanticZero ? TEXT("unallocated_paint_semantic_zero") : TEXT("texture_source_samples"));
		if (!Texture && !bSemanticZero) { Error = TEXT("Missing requested height texture; no zero/composite substitution."); return false; }
		const FMipCopy* Mip = Texture ? ReadMip(Texture, Cache, Error) : nullptr;
		if (Texture && !Mip) { return false; }
		int32 OffsetX = 0, OffsetY = 0;
		if (Mip)
		{
			const FVector4& Bias = bPaint ? Component->WeightmapScaleBias : Component->HeightmapScaleBias;
			const double U = Bias.Z * Mip->Width, V = Bias.W * Mip->Height;
			if (!FMath::IsFinite(U) || !FMath::IsFinite(V) || U < 0 || V < 0 || U >= MAX_int32 || V >= MAX_int32)
			{ Error = TEXT("Invalid source mip UV offset."); return false; }
			// Native GetHeightMapColor/GetWeightMapColor truncate offset*source dimension; weight bias can contain half a texel.
			OffsetX = int32(U); OffsetY = int32(V);
			if (OffsetX > Mip->Width - TileSize || OffsetY > Mip->Height - TileSize)
			{ Error = TEXT("The one-subsection 64 by 64 sample block exceeds source mip zero."); return false; }
			Row->SetStringField(TEXT("sourceTexture"), Texture->GetPathName()); Row->SetStringField(TEXT("sourceId"), Mip->SourceId.ToString(EGuidFormats::Digits));
			Row->SetStringField(TEXT("sourceFormat"), TEXT("TSF_BGRA8")); Row->SetNumberField(TEXT("mipIndex"), 0);
			Row->SetArrayField(TEXT("sourceSize"), PairArray(Mip->Width, Mip->Height)); Row->SetArrayField(TEXT("sourceMipOffset"), PairArray(OffsetX, OffsetY));
			Row->SetArrayField(TEXT("sourceScaleBias"), {MakeShared<FJsonValueNumber>(Bias.X), MakeShared<FJsonValueNumber>(Bias.Y), MakeShared<FJsonValueNumber>(Bias.Z), MakeShared<FJsonValueNumber>(Bias.W)});
			Row->SetNumberField(TEXT("sourcePixelStrideBytes"), 4); Row->SetNumberField(TEXT("sourceRowStrideBytes"), Mip->Width * 4);
			Row->SetStringField(TEXT("sourceAddressRule"), TEXT("4*((int(biasW*height)+localY)*width+int(biasZ*width)+localX)+FColor_channel_offset; NumSubsections=1,SubsectionSizeQuads=63"));
			if (!SourceDigests.Contains(Texture)) { SourceDigests.Add(Texture, LexToString(FBlake3::HashBuffer(Mip->Bytes.GetData(), uint64(Mip->Bytes.Num())))); }
			Row->SetStringField(TEXT("sourceMipBLAKE3"), SourceDigests[Texture]);
		}
		else
		{
			Row->SetField(TEXT("sourceTexture"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("sourceId"), MakeShared<FJsonValueNull>());
			Row->SetField(TEXT("sourceMipOffset"), MakeShared<FJsonValueNull>());
		}
		const int32 BytesPerSample = bHeightRG ? 2 : 1;
		// Init clears reused tile storage too; SetNumZeroed alone would retain prior bytes when the size is unchanged.
		Bytes.Init(0, TileSamples * BytesPerSample);
		const int32 ColorOffsets[] = {STRUCT_OFFSET(FColor, R), STRUCT_OFFSET(FColor, G), STRUCT_OFFSET(FColor, B), STRUCT_OFFSET(FColor, A)};
		for (int32 Y = 0; Y < TileSize && Mip; ++Y)
		{
			for (int32 X = 0; X < TileSize; ++X)
			{
				const uint8* Pixel = Mip->Bytes.GetData() + 4 * (int64(OffsetY + Y) * Mip->Width + OffsetX + X);
				const int32 Output = (Y * TileSize + X) * BytesPerSample;
				if (bHeightRG) { Bytes[Output] = Pixel[STRUCT_OFFSET(FColor, G)]; Bytes[Output + 1] = Pixel[STRUCT_OFFSET(FColor, R)]; }
				else if (bPaint) { Bytes[Output] = Pixel[ColorOffsets[ChannelIndex]]; }
				else { Bytes[Output] = Pixel[Channel.Kind == TEXT("raw_b") ? STRUCT_OFFSET(FColor, B) : STRUCT_OFFSET(FColor, A)]; }
			}
		}
		Row->SetStringField(TEXT("raw_base64"), FBase64::Encode(Bytes)); Row->SetNumberField(TEXT("sampleCount"), TileSamples); Row->SetNumberField(TEXT("byteCount"), Bytes.Num());
		Row->SetStringField(TEXT("dataBLAKE3"), LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())));
		return true;
	}

	bool Channel(const TArray<const ULandscapeComponent*>& Components, const FTileChannel& Spec, FMipCache& Cache,
		TMap<UTexture2D*, FString>& SourceDigests, const TSharedRef<FJsonObject>& Row, FString& Error)
	{
		const int32 BytesPerSample = Spec.Kind == TEXT("height_rg") ? 2 : 1;
		Row->SetStringField(TEXT("layerGuid"), Spec.LayerGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("kind"), Spec.Kind);
		if (Spec.Paint) { Row->SetStringField(TEXT("paintTargetPath"), Spec.Paint->GetPathName()); } else { Row->SetField(TEXT("paintTargetPath"), MakeShared<FJsonValueNull>()); }
		Row->SetStringField(TEXT("encoding"), BytesPerSample == 2 ? TEXT("u16le") : TEXT("u8")); Row->SetStringField(TEXT("sampleOrder"), TEXT("x_fast_y_increasing"));
		Row->SetStringField(TEXT("channelMeaning"), Spec.Kind == TEXT("raw_b") ? TEXT("literal_raw_B_byte_not_alpha") : Spec.Kind == TEXT("raw_a") ? TEXT("literal_raw_A_byte_not_flags") : Spec.Kind == TEXT("height_rg") ? TEXT("uint16_(R<<8)|G_stored_little_endian") : TEXT("literal_paint_weight_u8"));
		TArray<TSharedPtr<FJsonValue>> Tiles; TArray<uint8> Canonical, TileBytes; Canonical.SetNumZeroed(SampleCount * BytesPerSample);
		TBitArray<> Seen(false, SampleCount); int32 Unique = 0, Duplicates = 0, Mismatches = 0;
		for (const ULandscapeComponent* Component : Components)
		{
			const TSharedRef<FJsonObject> TileRow = MakeShared<FJsonObject>(); Tiles.Add(MakeShared<FJsonValueObject>(TileRow));
			if (!Tile(Component, Spec, Cache, SourceDigests, TileBytes, TileRow, Error))
			{ TileRow->SetStringField(TEXT("error"), Error); Row->SetArrayField(TEXT("tiles"), Tiles); Row->SetBoolField(TEXT("complete"), false); return false; }
			const FIntPoint Base = Component->GetSectionBase();
			for (int32 Y = 0; Y < TileSize; ++Y)
			{
				for (int32 X = 0; X < TileSize; ++X)
				{
					const int32 Index = (Base.Y + Y - MinY) * Width + Base.X + X - MinX;
					if (!Seen.IsValidIndex(Index)) { Error = TEXT("A tile sample is outside the canonical retained rectangle."); Row->SetArrayField(TEXT("tiles"), Tiles); return false; }
					const int32 Input = (Y * TileSize + X) * BytesPerSample, Output = Index * BytesPerSample;
					if (Seen[Index])
					{
						++Duplicates;
						if (Canonical[Output] != TileBytes[Input] || (BytesPerSample == 2 && Canonical[Output + 1] != TileBytes[Input + 1])) { ++Mismatches; }
					}
					else
					{
						Seen[Index] = true; ++Unique; Canonical[Output] = TileBytes[Input];
						if (BytesPerSample == 2) { Canonical[Output + 1] = TileBytes[Input + 1]; }
					}
				}
			}
		}
		Row->SetArrayField(TEXT("tiles"), Tiles); Row->SetNumberField(TEXT("tileCount"), Tiles.Num());
		Row->SetNumberField(TEXT("tileSampleOccurrences"), Components.Num() * TileSamples); Row->SetNumberField(TEXT("tileByteCount"), Components.Num() * TileSamples * BytesPerSample);
		Row->SetNumberField(TEXT("canonicalSampleCount"), Unique); Row->SetNumberField(TEXT("canonicalByteCount"), Canonical.Num());
		Row->SetNumberField(TEXT("seamDuplicateOccurrences"), Duplicates); Row->SetNumberField(TEXT("seamMismatchCount"), Mismatches);
		const bool bComplete = Tiles.Num() == 45 && Unique == SampleCount && Duplicates == 4832 && Mismatches == 0;
		Row->SetBoolField(TEXT("complete"), bComplete);
		if (bComplete) { Row->SetStringField(TEXT("canonicalBLAKE3"), LexToString(FBlake3::HashBuffer(Canonical.GetData(), Canonical.Num()))); }
		else { Error = TEXT("Retained tile coverage/seam comparison failed; payloads are evidence, not certified canonical data."); }
		return bComplete;
	}

	FString Export()
	{
		if (!IsInGameThread()) { return TEXT("{\"success\":false,\"error\":\"Raw tile export requires the editor game thread.\"}"); }
		const FDirtySnapshot DirtyBefore = CaptureDirtyPackages();
		const TSharedRef<FJsonObject> Result = NewResult(TEXT("export_candidate_retained_tiles"), CandidateMap);
		Result->SetNumberField(TEXT("schemaVersion"), 2); Result->SetStringField(TEXT("purpose"), TEXT("raw_inventory_prerequisites_only_not_execution_authorization"));
		Result->SetStringField(TEXT("payloadFormat"), TEXT("raw_base64_transport; wrapper writes BIN/files/SHA256 and pins Source/DLL; not a file inventory yet"));
		Result->SetBoolField(TEXT("fullFidelityPreservationCertified"), false); Result->SetBoolField(TEXT("cropExecutionAuthorized"), false);
		Result->SetBoolField(TEXT("explicitFileIOPerformed"), false); Result->SetBoolField(TEXT("rawLookupFallbackUsed"), false);
		Result->SetStringField(TEXT("dataBoundary"), TEXT("Current CPU Texture.Source mip zero only: composite RG/paint and raw layer RG/B/A/paint. Composite B/A normals, outer halo, full atlas/mips, GPU recomposition, collision, references and cold reload are not certified."));
		UWorld* World = nullptr; FString Error;
		if (!ResolveEditorWorld(CandidateMap, World, Result, Error)) { return Finish(Result, DirtyBefore, false, Error); }
		const Rogue10mCrop20261114Dependencies::FEditorReadState EditorBefore = Rogue10mCrop20261114Dependencies::CaptureEditorState();
		auto Complete = [&](bool bSuccess, FString Message)
		{
			const bool bEditorStable = Rogue10mCrop20261114Dependencies::RecordEditorStability(EditorBefore, Result);
			if (!bEditorStable) { Message += TEXT(" Editor object/selection/transaction state changed."); }
			return Finish(Result, DirtyBefore, bSuccess && bEditorStable, Message);
		};
		FTerrainState State; CollectTerrain(World, Result, State);
		if (!ValidateLayout(World, State, Error)) { return Complete(false, Error); }
		const FGuid EditingBefore = State.Main->GetEditingLayer(); const TSet<ULandscapeComponent*> SelectionBefore = State.Info->GetSelectedComponents();
		const TArray<const ULandscapeEditLayerBase*> Layers = State.Main->GetEditLayersConst();
		if (Layers.Num() != 1 || !IsValid(Layers[0]) || !Layers[0]->NeedsPersistentTextures() || !Layers[0]->GetGuid().IsValid())
		{ return Complete(false, TEXT("This eight-channel QA capture requires exactly one valid persistent raw edit layer; refresh the inventory plan if that contract changed.")); }
		TArray<const ULandscapeComponent*> Components;
		for (const ULandscapeComponent* Component : State.Components) { if (IsKeptBase(Component->GetSectionBase())) { Components.Add(Component); } }
		Components.Sort([](const ULandscapeComponent& Left, const ULandscapeComponent& Right)
		{ const FIntPoint P = Left.GetSectionBase(), Q = Right.GetSectionBase(); return P.Y == Q.Y ? P.X < Q.X : P.Y < Q.Y; });
		if (Components.Num() != 45) { return Complete(false, TEXT("Exactly 45 retained components are required.")); }
		TArray<FGuid> Guids = {FGuid(), Layers[0]->GetGuid()}; TSet<ULandscapeLayerInfoObject*> PaintSet;
		for (const FLandscapeInfoLayerSettings& Settings : State.Info->Layers)
		{ if (!IsValid(Settings.LayerInfoObj)) { return Complete(false, TEXT("A declared paint target has no LayerInfo asset.")); } PaintSet.Add(Settings.LayerInfoObj.Get()); }
		TArray<TSharedPtr<FJsonValue>> Presence; bool bPresenceComplete = true;
		for (const ULandscapeComponent* Component : Components)
		{
			const bool bHasLayer = Component->GetLayerData(Layers[0]->GetGuid()) != nullptr;
			UTexture2D* RawHeight = bHasLayer ? Component->GetHeightmap(Layers[0]->GetGuid()) : nullptr;
			const TSharedRef<FJsonObject> PresenceRow = MakeShared<FJsonObject>(); Presence.Add(MakeShared<FJsonValueObject>(PresenceRow));
			const FIntPoint Base = Component->GetSectionBase(); PresenceRow->SetArrayField(TEXT("sectionBase"), PairArray(Base.X, Base.Y));
			PresenceRow->SetStringField(TEXT("layerGuid"), Layers[0]->GetGuid().ToString(EGuidFormats::Digits));
			PresenceRow->SetBoolField(TEXT("hasLayerData"), bHasLayer); PresenceRow->SetBoolField(TEXT("heightTexturePresent"), IsValid(RawHeight));
			PresenceRow->SetStringField(TEXT("heightTexture"), RawHeight ? RawHeight->GetPathName() : FString());
			bPresenceComplete &= bHasLayer && IsValid(RawHeight);
			for (const FGuid& Guid : Guids)
			{
				if (Guid.IsValid() && !bHasLayer) { continue; }
				for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(Guid))
				{ if (Allocation.IsAllocated()) { if (!IsValid(Allocation.LayerInfo)) { return Complete(false, TEXT("An allocated paint channel has no LayerInfo asset.")); } PaintSet.Add(Allocation.LayerInfo.Get()); } }
			}
		}
		Result->SetArrayField(TEXT("componentLayerPresence"), Presence);
		if (!bPresenceComplete) { return Complete(false, TEXT("Raw layer presence/height texture is incomplete; no raw getter fallback or zero substitution was used.")); }
		TArray<ULandscapeLayerInfoObject*> Paints = PaintSet.Array();
		Paints.Sort([](const ULandscapeLayerInfoObject& Left, const ULandscapeLayerInfoObject& Right) { return Left.GetPathName() < Right.GetPathName(); });
		if (Paints.Num() != 2) { return Complete(false, TEXT("The eight-channel QA capture requires exactly two current paint targets; no targets were omitted.")); }
		FString MetadataBefore; if (!Metadata(State, Paints, MetadataBefore, Error)) { return Complete(false, Error); }
		const FString PaintConfigBefore = PaintConfiguration(Paints);
		Result->SetStringField(TEXT("metadataDigest"), MetadataBefore); Result->SetStringField(TEXT("paintConfigurationDigest"), PaintConfigBefore);
		Result->SetStringField(TEXT("sourceMap"), CandidateMap); Result->SetStringField(TEXT("sourceLandscapeGuid"), State.Main->GetLandscapeGuid().ToString(EGuidFormats::Digits));
		Result->SetArrayField(TEXT("cropSampleRectInclusive"), {MakeShared<FJsonValueNumber>(MinX), MakeShared<FJsonValueNumber>(MinY), MakeShared<FJsonValueNumber>(MaxX), MakeShared<FJsonValueNumber>(MaxY)});
		Result->SetArrayField(TEXT("sourceLayers"), Result->GetArrayField(TEXT("edit_layers")));
		TArray<TSharedPtr<FJsonValue>> PaintRows;
		for (const ULandscapeLayerInfoObject* Paint : Paints)
		{
			const TSharedRef<FJsonObject> PaintRow = MakeShared<FJsonObject>(); PaintRows.Add(MakeShared<FJsonValueObject>(PaintRow));
			PaintRow->SetStringField(TEXT("name"), Paint->GetLayerName().ToString()); PaintRow->SetStringField(TEXT("layerInfoPath"), Paint->GetPathName());
			PaintRow->SetNumberField(TEXT("blendMethod"), uint32(Paint->GetBlendMethod())); PaintRow->SetStringField(TEXT("blendGroup"), Paint->GetBlendGroup().ToString());
		}
		Result->SetArrayField(TEXT("paintTargets"), PaintRows);
		TMap<FGuid, TSet<FString>> LoadedActorPaths;
		for (TActorIterator<AActor> It(World); It; ++It) { LoadedActorPaths.FindOrAdd(It->GetActorGuid()).Add(It->GetPathName()); }
		int32 DescriptorCount = 0, LoadedCount = 0; TArray<FString> UnloadedPaths;
		FWorldPartitionHelpers::ForEachActorDescInstance<AActor>(World->GetWorldPartition(), [&](const FWorldPartitionActorDescInstance* Descriptor)
		{
			++DescriptorCount; const FString Path = Descriptor->GetActorSoftPath().ToString(); const TSet<FString>* Paths = LoadedActorPaths.Find(Descriptor->GetGuid());
			if (Paths && Paths->Contains(Path)) { ++LoadedCount; } else { UnloadedPaths.Add(Path); }
			return true;
		});
		const bool bAllLoaded = DescriptorCount > 0 && LoadedCount == DescriptorCount;
		const TSharedRef<FJsonObject> Capture = MakeShared<FJsonObject>(); Result->SetObjectField(TEXT("capture"), Capture);
		Capture->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601()); Capture->SetBoolField(TEXT("allDescriptorsLoaded"), bAllLoaded);
		Capture->SetObjectField(TEXT("dirtyBefore"), DirtyObject(DirtyBefore)); Capture->SetField(TEXT("sourcePackageManifestSha256"), MakeShared<FJsonValueNull>());
		Capture->SetField(TEXT("exporterSourceSha256"), MakeShared<FJsonValueNull>()); Capture->SetField(TEXT("exporterDllSha256"), MakeShared<FJsonValueNull>());
		Capture->SetStringField(TEXT("identityBoundary"), TEXT("Actual file SHA256/BIN paths and source/DLL pinning must be supplied by the external owner wrapper; this function performs no file I/O."));
		Result->SetNumberField(TEXT("allActorDescriptorCount"), DescriptorCount); Result->SetNumberField(TEXT("loadedActorDescriptorCount"), LoadedCount);
		UnloadedPaths.Sort(); Result->SetArrayField(TEXT("unloadedActorPaths"), StringArray(UnloadedPaths));
		TArray<FTileChannel> Specs; Specs.Add(FTileChannel{FGuid(), TEXT("height_rg"), nullptr});
		for (ULandscapeLayerInfoObject* Paint : Paints) { Specs.Add(FTileChannel{FGuid(), TEXT("paint"), Paint}); }
		Specs.Add(FTileChannel{Layers[0]->GetGuid(), TEXT("height_rg"), nullptr}); Specs.Add(FTileChannel{Layers[0]->GetGuid(), TEXT("raw_b"), nullptr}); Specs.Add(FTileChannel{Layers[0]->GetGuid(), TEXT("raw_a"), nullptr});
		for (ULandscapeLayerInfoObject* Paint : Paints) { Specs.Add(FTileChannel{Layers[0]->GetGuid(), TEXT("paint"), Paint}); }
		FMipCache Cache; TMap<UTexture2D*, FString> SourceDigests; TArray<TSharedPtr<FJsonValue>> Channels; TArray<FString> Errors;
		for (const FTileChannel& Spec : Specs)
		{
			const TSharedRef<FJsonObject> ChannelRow = MakeShared<FJsonObject>(); Channels.Add(MakeShared<FJsonValueObject>(ChannelRow)); Error.Reset();
			if (!Channel(Components, Spec, Cache, SourceDigests, ChannelRow, Error)) { ChannelRow->SetStringField(TEXT("error"), Error); Errors.Add(Error); }
		}
		Result->SetArrayField(TEXT("channels"), Channels); Result->SetNumberField(TEXT("channelCount"), Specs.Num());
		Result->SetNumberField(TEXT("tileCountPerChannel"), 45); Result->SetNumberField(TEXT("tilePayloadBytesExpected"), 1843200);
		Result->SetNumberField(TEXT("canonicalPayloadBytesExpected"), 1794880); Result->SetNumberField(TEXT("cachedTextureCount"), Cache.Num());
		bool bSourcesStable = true;
		for (const TPair<UTexture2D*, TSharedPtr<FMipCopy>>& SourcePair : Cache)
		{ bSourcesStable &= SourcePair.Key->Source.GetId() == SourcePair.Value->SourceId && SourcePair.Key->GetPackage()->IsDirty() == SourcePair.Value->bDirty && SourcePair.Key->HasAnyFlags(RF_Transactional) == SourcePair.Value->bTransactional; }
		FString MetadataAfter; Error.Reset(); const bool bMetadataStable = Metadata(State, Paints, MetadataAfter, Error) && MetadataAfter == MetadataBefore && PaintConfiguration(Paints) == PaintConfigBefore;
		const TSet<ULandscapeComponent*> SelectionAfter = State.Info->GetSelectedComponents();
		const bool bLayerSelectionStable = SelectionBefore.Num() == SelectionAfter.Num() && SelectionBefore.Difference(SelectionAfter).IsEmpty() && State.Main->GetEditingLayer() == EditingBefore;
		Result->SetBoolField(TEXT("textureSourcesUnchanged"), bSourcesStable); Result->SetBoolField(TEXT("metadataUnchanged"), bMetadataStable); Result->SetBoolField(TEXT("landscapeSelectionAndEditingLayerUnchanged"), bLayerSelectionStable);
		Capture->SetObjectField(TEXT("dirtyAfter"), DirtyObject(CaptureDirtyPackages()));
		if (!bSourcesStable || !bMetadataStable || !bLayerSelectionStable) { Errors.Add(TEXT("Texture/layer/paint/selection state changed during raw tile reading.")); }
		if (!bAllLoaded) { Errors.Add(TEXT("Not all actor descriptors match currently loaded world actors; no load was attempted.")); }
		Result->SetArrayField(TEXT("exportErrors"), StringArray(Errors)); Result->SetBoolField(TEXT("allRequestedTilesComplete"), Errors.IsEmpty());
		return Complete(Errors.IsEmpty(), FString::Join(Errors, TEXT(" | ")));
	}
}
#endif

FString URogue10mCrop20261114WorldAuthoring::ExportCandidateRetainedTiles()
{
#if WITH_EDITOR
	return Rogue10mCrop20261114RawTiles::Export();
#else
	return TEXT("{\"success\":false,\"error\":\"Raw retained tile export is available only in an editor build.\"}");
#endif
}

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Tests/Rogue10mReferenceScanTests.inl"
#endif


