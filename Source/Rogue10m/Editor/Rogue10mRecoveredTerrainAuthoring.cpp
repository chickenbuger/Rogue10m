// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mRecoveredTerrainAuthoring.h"

#if WITH_EDITOR
#include "Core/Rogue10m.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"

namespace Rogue10mRecoveredTerrainAuthoring
{
	constexpr int32 QuadsPerComponent = 63;
	constexpr int32 ComponentsPerAxis = 16;
	constexpr int32 ComponentCount = ComponentsPerAxis * ComponentsPerAxis;
	constexpr int32 LastVertex = QuadsPerComponent * ComponentsPerAxis;
	constexpr int32 VerticesPerAxis = LastVertex + 1;
	constexpr int32 VertexCount = VerticesPerAxis * VerticesPerAxis;
	constexpr uint16 NeutralHeight = 32768;
	const TCHAR* const MapPackage = TEXT("/Game/Rogue10m/Maps/OpenRunMap");
	const FName RecoveryLayerName(TEXT("RecoveredTerrain20261119"));
	const FVector LandscapeOrigin(-90000, -90000, 0);
	const FVector LandscapeScale(1250.0 / 7.0, 1250.0 / 7.0, 256.0);

	struct FState
	{
		ALandscape* Landscape = nullptr;
		ULandscapeInfo* Info = nullptr;
		TArray<ALandscapeProxy*> Proxies;
		TArray<ULandscapeComponent*> Components;
	};

	FString Finish(const TSharedRef<FJsonObject>& Result, const FString& Error = FString())
	{
		Result->SetBoolField(TEXT("success"), Error.IsEmpty());
		Result->SetBoolField(TEXT("packages_saved"), false);
		Result->SetStringField(TEXT("map"), MapPackage);
		if (!Error.IsEmpty())
		{
			Result->SetStringField(TEXT("error"), Error);
			UE_LOG(LogRogue10m, Warning, TEXT("Recovered terrain authoring: %s"), *Error);
		}
		FString Json;
		FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	bool Resolve(FState& State, FString& Error)
	{
		if (!IsInGameThread() || FCString::Stricmp(FApp::GetProjectName(), TEXT("Rogue10m")) != 0
			|| !GEditor || GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
		{
			Error = TEXT("An idle Rogue10m editor world is required.");
			return false;
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World || World->WorldType != EWorldType::Editor || World->GetPackage()->GetName() != MapPackage
			|| !World->GetWorldPartition() || !World->GetWorldPartition()->IsStreamingEnabled())
		{
			Error = TEXT("Load the existing World Partition OpenRunMap editor world first.");
			return false;
		}

		int32 StreamingProxyCount = 0;
		TSet<FGuid> LoadedGuids;
		for (TActorIterator<ALandscapeProxy> Actor(World); Actor; ++Actor)
		{
			ALandscapeProxy* Proxy = *Actor;
			State.Proxies.Add(Proxy);
			LoadedGuids.Add(Proxy->GetActorGuid());
			if (ALandscape* Main = Cast<ALandscape>(Proxy))
			{
				if (State.Landscape)
				{
					Error = TEXT("The editor world has multiple main Landscapes.");
					return false;
				}
				State.Landscape = Main;
			}
			else if (Proxy->IsA<ALandscapeStreamingProxy>())
			{
				++StreamingProxyCount;
			}
		}
		if (!State.Landscape || StreamingProxyCount != 16 || State.Proxies.Num() != 17
			|| !State.Landscape->GetActorLocation().Equals(LandscapeOrigin, 0.001)
			|| !State.Landscape->GetActorScale3D().Equals(LandscapeScale, 0.001)
			|| !State.Landscape->GetActorRotation().IsNearlyZero(0.001))
		{
			Error = TEXT("The full canonical 1.8 km Landscape with 16 loaded streaming proxies is required.");
			return false;
		}
		int32 DescriptorCount = 0;
		bool bAllTerrainLoaded = true;
		FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(World->GetWorldPartition(),
			[&](const FWorldPartitionActorDescInstance* Descriptor)
			{
				++DescriptorCount;
				bAllTerrainLoaded &= LoadedGuids.Contains(Descriptor->GetGuid());
				return true;
			});
		if (!bAllTerrainLoaded || DescriptorCount != State.Proxies.Num())
		{
			Error = TEXT("Every existing Landscape descriptor must be loaded.");
			return false;
		}
		for (const ALandscapeProxy* Proxy : State.Proxies)
		{
			if (Proxy->GetLandscapeGuid() != State.Landscape->GetLandscapeGuid())
			{
				Error = TEXT("A streaming proxy belongs to a different Landscape.");
				return false;
			}
		}

		State.Info = State.Landscape->GetLandscapeInfo();
		if (!State.Info || State.Info->XYtoComponentMap.Num() != ComponentCount
			|| State.Info->XYtoCollisionComponentMap.Num() != ComponentCount)
		{
			Error = TEXT("Expected all 256 terrain render and collision components in LandscapeInfo.");
			return false;
		}
		State.Components.Reserve(ComponentCount);
		for (int32 Y = 0; Y < ComponentsPerAxis; ++Y)
		{
			for (int32 X = 0; X < ComponentsPerAxis; ++X)
			{
				const FIntPoint Coordinate(X, Y);
				ULandscapeComponent* Component = State.Info->XYtoComponentMap.FindRef(Coordinate);
				ULandscapeHeightfieldCollisionComponent* Collision = State.Info->XYtoCollisionComponentMap.FindRef(Coordinate);
				if (!IsValid(Component) || !IsValid(Collision) || !Component->IsRegistered() || !Collision->IsRegistered()
					|| Component->GetSectionBase() != Coordinate * QuadsPerComponent || Component->GetCollisionComponent() != Collision
					|| Component->ComponentSizeQuads != QuadsPerComponent || Component->NumSubsections != 1
					|| Component->SubsectionSizeQuads != QuadsPerComponent)
				{
					Error = TEXT("A canonical 63-quad terrain tile or its collision component is missing.");
					return false;
				}
				State.Components.Add(Component);
			}
		}
		return true;
	}

	bool ReadGrid(const FState& State, const FGuid& Layer, TArray<uint16>& Heights)
	{
		for (const ULandscapeComponent* Component : State.Components)
		{
			const UTexture2D* Texture = Component->GetHeightmap(Layer);
			if ((Layer.IsValid() && !Component->GetLayerData(Layer)) || !IsValid(Texture)
				|| !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8)
			{
				return false;
			}
		}
		Heights.SetNumUninitialized(VertexCount);
		FLandscapeEditDataInterface Edit(State.Info, Layer, false);
		Edit.GetHeightDataFast(0, 0, LastVertex, LastVertex, Heights.GetData(), VerticesPerAxis);
		return true;
	}

	TArray<uint8> EncodeR16(const TArray<uint16>& Heights)
	{
		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(Heights.Num() * 2);
		for (int32 Index = 0; Index < Heights.Num(); ++Index)
		{
			Bytes[Index * 2] = static_cast<uint8>(Heights[Index]);
			Bytes[Index * 2 + 1] = static_cast<uint8>(Heights[Index] >> 8);
		}
		return Bytes;
	}

	FString HeightHash(const TArray<uint8>& Bytes)
	{
		return LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num()));
	}

	bool ResolveScratchR16(const FString& Input, FString& Output)
	{
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Input);
		FPaths::NormalizeFilename(Output);
		return FPaths::CollapseRelativeDirectories(Output)
			&& FPaths::IsUnderDirectory(Output, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tmp")))
			&& FPaths::GetExtension(Output).Equals(TEXT("r16"), ESearchCase::IgnoreCase);
	}

	void HashText(FBlake3& Hash, const FString& Value)
	{
		const FTCHARToUTF8 Encoded(*Value);
		const int32 Length = Encoded.Length();
		Hash.Update(&Length, sizeof(Length));
		Hash.Update(Encoded.Get(), Length);
	}

	bool ExistingLayersHash(const FState& State, FString& OutHash)
	{
		FBlake3 Hash;
		TSet<const UTexture2D*> Seen;
		const auto HashTexture = [&Hash, &Seen](UTexture2D* Texture)
		{
			if (!IsValid(Texture) || !Texture->Source.IsValid())
			{
				return false;
			}
			HashText(Hash, Texture->GetPathName());
			if (!Seen.Contains(Texture))
			{
				TArray64<uint8> Source;
				if (!Texture->Source.GetMipData(Source, 0))
				{
					return false;
				}
				HashText(Hash, FString::Printf(TEXT("%d:%d:%d"), Texture->Source.GetSizeX(), Texture->Source.GetSizeY(), int32(Texture->Source.GetFormat())));
				Hash.Update(Source.GetData(), Source.Num());
				Seen.Add(Texture);
			}
			return true;
		};

		for (const ULandscapeEditLayerBase* Layer : State.Landscape->GetEditLayersConst())
		{
			if (!Layer)
			{
				return false;
			}
			if (Layer->GetName() == RecoveryLayerName)
			{
				continue;
			}
			const FGuid Guid = Layer->GetGuid();
			HashText(Hash, Guid.ToString());
			HashText(Hash, Layer->GetName().ToString());
			HashText(Hash, Layer->GetClass()->GetPathName());
			HashText(Hash, FString::Printf(TEXT("%.9g:%.9g:%d:%d"),
				Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap),
				Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap), Layer->IsVisible(), Layer->IsLocked()));
			for (const ULandscapeComponent* Component : State.Components)
			{
				if (!Component->GetLayerData(Guid) || !HashTexture(Component->GetHeightmap(Guid)))
				{
					return false;
				}
				for (UTexture2D* Texture : Component->GetWeightmapTextures(Guid))
				{
					if (!HashTexture(Texture))
					{
						return false;
					}
				}
				for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(Guid))
				{
					HashText(Hash, IsValid(Allocation.LayerInfo) ? Allocation.LayerInfo->GetPathName() : TEXT(""));
					HashText(Hash, FString::Printf(TEXT("%d:%d"), Allocation.WeightmapTextureIndex, Allocation.WeightmapTextureChannel));
				}
			}
		}
		OutHash = LexToString(Hash.Finalize());
		return true;
	}

	void RecordFacts(const FState& State, const TSharedRef<FJsonObject>& Result)
	{
		Result->SetNumberField(TEXT("width"), VerticesPerAxis);
		Result->SetNumberField(TEXT("height"), VerticesPerAxis);
		Result->SetNumberField(TEXT("component_count"), State.Components.Num());
		Result->SetNumberField(TEXT("collision_count"), State.Info->XYtoCollisionComponentMap.Num());
		Result->SetNumberField(TEXT("streaming_proxy_count"), State.Proxies.Num() - 1);
		Result->SetNumberField(TEXT("xy_spacing_cm"), LandscapeScale.X);
		Result->SetNumberField(TEXT("height_unit_cm"), LandscapeScale.Z / 128.0);
		Result->SetNumberField(TEXT("origin_x_cm"), LandscapeOrigin.X);
		Result->SetNumberField(TEXT("origin_y_cm"), LandscapeOrigin.Y);
		Result->SetStringField(TEXT("landscape_guid"), State.Landscape->GetLandscapeGuid().ToString());
		Result->SetBoolField(TEXT("landscape_reports_up_to_date"), State.Landscape->IsUpToDate());
		Result->SetBoolField(TEXT("has_layers_content"), State.Landscape->HasLayersContent());
		TArray<TSharedPtr<FJsonValue>> LayerNames;
		for (const ULandscapeEditLayerBase* Layer : State.Landscape->GetEditLayersConst())
		{
			if (Layer)
			{
				LayerNames.Add(MakeShared<FJsonValueString>(Layer->GetName().ToString()));
			}
		}
		Result->SetArrayField(TEXT("layer_names"), MoveTemp(LayerNames));

		TArray<UPackage*> Maps;
		TArray<UPackage*> Assets;
		UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Maps);
		UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Assets);
		TSet<FString> DirtyNames;
		for (const TArray<UPackage*>* Packages : {&Maps, &Assets})
		{
			for (const UPackage* Package : *Packages)
			{
				if (Package)
				{
					DirtyNames.Add(Package->GetName());
				}
			}
		}
		TArray<FString> SortedNames = DirtyNames.Array();
		SortedNames.Sort();
		TArray<TSharedPtr<FJsonValue>> DirtyPackages;
		for (const FString& Name : SortedNames)
		{
			DirtyPackages.Add(MakeShared<FJsonValueString>(Name));
		}
		Result->SetArrayField(TEXT("dirty_packages"), MoveTemp(DirtyPackages));
	}
}
#endif

FString URogue10mRecoveredTerrainAuthoring::InspectOpenRunMapHeightfield(const FString& ExportR16Path)
{
#if WITH_EDITOR
	using namespace Rogue10mRecoveredTerrainAuthoring;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	FState State;
	FString Error;
	if (!Resolve(State, Error))
	{
		return Finish(Result, Error);
	}
	RecordFacts(State, Result);
	TArray<uint16> Heights;
	FString LayerHash;
	if (!ReadGrid(State, FGuid(), Heights) || !ExistingLayersHash(State, LayerHash))
	{
		return Finish(Result, TEXT("The native height or existing layer texture source could not be read."));
	}
	const TArray<uint8> Bytes = EncodeR16(Heights);
	Result->SetStringField(TEXT("composite_blake3"), HeightHash(Bytes));
	Result->SetStringField(TEXT("existing_layers_blake3"), LayerHash);
	if (!ExportR16Path.IsEmpty())
	{
		FString Path;
		if (!State.Landscape->IsUpToDate() || !ResolveScratchR16(ExportR16Path, Path)
			|| !IFileManager::Get().DirectoryExists(*FPaths::GetPath(Path)) || IFileManager::Get().FileExists(*Path)
			|| !FFileHelper::SaveArrayToFile(Bytes, *Path))
		{
			return Finish(Result, TEXT("R16 export requires a settled Landscape and a new project tmp file in an existing directory."));
		}
		Result->SetStringField(TEXT("export_r16_path"), Path);
	}
	return Finish(Result);
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required.\"}");
#endif
}

FString URogue10mRecoveredTerrainAuthoring::ApplyOpenRunMapHeightPatch(const FString& TargetR16Path, int32 MinX, int32 MinY,
	int32 MaxX, int32 MaxY, const FString& ExpectedLandscapeGuid, const FString& ExpectedCompositeBlake3)
{
#if WITH_EDITOR
	using namespace Rogue10mRecoveredTerrainAuthoring;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("mutation_started"), false);
	FState State;
	FString Error;
	if (!Resolve(State, Error))
	{
		return Finish(Result, Error);
	}
	RecordFacts(State, Result);
	if (!FApp::CanEverRender() || IsRunningCommandlet() || GEditor->IsTransactionActive() || !State.Landscape->IsUpToDate())
	{
		return Finish(Result, TEXT("A rendered editor with settled Landscape layers and no active transaction is required."));
	}
	if (!State.Landscape->HasLayersContent() || State.Landscape->GetEditLayersConst().IsEmpty())
	{
		return Finish(Result, TEXT("The Landscape has no editable base layer. Enable or repair Landscape edit layers before applying the recovery."));
	}
	if (State.Landscape->GetEditLayerConst(RecoveryLayerName))
	{
		return Finish(Result, TEXT("The recovery layer already exists. Inspect or undo it before applying again."));
	}
	FGuid ExpectedGuid;
	if (!FGuid::Parse(ExpectedLandscapeGuid, ExpectedGuid) || ExpectedGuid != State.Landscape->GetLandscapeGuid())
	{
		return Finish(Result, TEXT("Landscape GUID differs from the inspected source."));
	}
	FString Path;
	if (MinX < 0 || MinY < 0 || MaxX > LastVertex || MaxY > LastVertex || MinX >= MaxX || MinY >= MaxY
		|| !ResolveScratchR16(TargetR16Path, Path))
	{
		return Finish(Result, TEXT("Provide an inclusive rectangle within 0..1008 and a project tmp R16 file."));
	}
	const int32 PatchWidth = MaxX - MinX + 1;
	const int32 PatchHeight = MaxY - MinY + 1;
	const int32 ExpectedBytes = PatchWidth * PatchHeight * 2;
	TArray<uint8> Bytes;
	if (IFileManager::Get().FileSize(*Path) != ExpectedBytes || !FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != ExpectedBytes)
	{
		return Finish(Result, TEXT("The R16 input byte count differs from the rectangle dimensions."));
	}
	TArray<uint16> Current;
	FString LayerHash;
	if (!ReadGrid(State, FGuid(), Current) || !ExistingLayersHash(State, LayerHash))
	{
		return Finish(Result, TEXT("Could not read the current native heights and original layer contents."));
	}
	const FString CurrentHash = HeightHash(EncodeR16(Current));
	if (!CurrentHash.Equals(ExpectedCompositeBlake3, ESearchCase::IgnoreCase))
	{
		return Finish(Result, TEXT("The composite heights changed since inspection. Prepare the target against a fresh export."));
	}
	TArray<uint16> Delta;
	Delta.Reserve(PatchWidth * PatchHeight);
	int32 ChangedSamples = 0;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 ByteIndex = Delta.Num() * 2;
			const uint16 Target = uint16(Bytes[ByteIndex]) | (uint16(Bytes[ByteIndex + 1]) << 8);
			uint16& CurrentSample = Current[Y * VerticesPerAxis + X];
			const int32 Difference = int32(NeutralHeight) + int32(Target) - int32(CurrentSample);
			if (Difference < 0 || Difference > MAX_uint16)
			{
				return Finish(Result, TEXT("A height difference cannot be stored in one additive edit layer."));
			}
			ChangedSamples += Target != CurrentSample;
			CurrentSample = Target;
			Delta.Add(uint16(Difference));
		}
	}
	if (ChangedSamples == 0)
	{
		return Finish(Result, TEXT("The absolute patch is identical to the existing terrain."));
	}

	Result->SetStringField(TEXT("previous_composite_blake3"), CurrentHash);
	Result->SetStringField(TEXT("target_composite_blake3"), HeightHash(EncodeR16(Current)));
	Result->SetStringField(TEXT("existing_layers_blake3"), LayerHash);
	Result->SetNumberField(TEXT("changed_samples"), ChangedSamples);

	Result->SetBoolField(TEXT("mutation_started"), true);
	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10m", "ApplyRecoveredTerrain", "Apply Recovered OpenRunMap Terrain"));
	State.Landscape->Modify();
	const int32 LayerIndex = State.Landscape->CreateLayer(RecoveryLayerName, ULandscapeEditLayer::StaticClass());
	ULandscapeEditLayerBase* Layer = LayerIndex == INDEX_NONE ? nullptr : State.Landscape->GetEditLayer(LayerIndex);
	if (!Layer || Layer->GetName() != RecoveryLayerName)
	{
		return Finish(Result, TEXT("The recovery layer could not be created. Inspect the unsaved editor state."));
	}
	Result->SetStringField(TEXT("new_layer_guid"), Layer->GetGuid().ToString());
	Layer->SetAlphaForTargetType(ELandscapeToolTargetType::Heightmap, 1.0f, true, EPropertyChangeType::ValueSet);
	Layer->SetAlphaForTargetType(ELandscapeToolTargetType::Weightmap, 0.0f, true, EPropertyChangeType::ValueSet);
	TArray<uint16> EmptyHeights;
	if (!ReadGrid(State, Layer->GetGuid(), EmptyHeights)
		|| EmptyHeights.ContainsByPredicate([](uint16 Height) { return Height != NeutralHeight; }))
	{
		return Finish(Result, TEXT("The new layer did not initialize to neutral height. Inspect the unsaved layer."));
	}
	{
		FLandscapeEditDataInterface Edit(State.Info, Layer->GetGuid());
		Edit.SetHeightData(MinX, MinY, MaxX, MaxY, Delta.GetData(), PatchWidth, false, nullptr, nullptr, nullptr,
			false, nullptr, nullptr, true, true, true);
		Edit.Flush();
	}
	State.Landscape->ForceLayersFullUpdate();
	RecordFacts(State, Result);
	FString LayerHashAfter;
	if (!ExistingLayersHash(State, LayerHashAfter) || LayerHashAfter != LayerHash)
	{
		return Finish(Result, TEXT("Existing layer data changed during composition. Inspect the unsaved editor state."));
	}
	Result->SetBoolField(TEXT("existing_layers_preserved"), true);
	Result->SetBoolField(TEXT("full_update_requested"), true);
	UE_LOG(LogRogue10m, Log, TEXT("Applied %d recovered terrain heights to the existing OpenRunMap Landscape. Verify before saving."), ChangedSamples);
	return Finish(Result);
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required.\"}");
#endif
}
