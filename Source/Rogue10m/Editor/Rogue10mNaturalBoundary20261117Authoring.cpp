// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mNaturalBoundary20261117Authoring.h"

#if WITH_EDITOR
#include "Core/Rogue10m.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/BitArray.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/HitResult.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Hash/Blake3.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"

namespace Rogue10mNaturalBoundary20261117
{
	const FString Map = TEXT("/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A");
	const FString OwnedPrefix = TEXT("/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/");
	const FName NewLayerName(TEXT("NaturalBoundary20261117"));
	const FGuid LandscapeGuid(0x147B73D2, 0x47543E06, 0xFDA1A7AF, 0x9D467458);
	const FGuid OriginalGuid(0x9C4488BD, 0x4196BCC8, 0x1051B5A0, 0x01047577);
	constexpr int32 MinX = 315, MinY = 630, MaxX = 882, MaxY = 945;
	constexpr int32 Width = 568, Height = 316, Samples = Width * Height;
	constexpr double Spacing = 1250.0 / 7.0;

	struct FState
	{
		UWorld* World = nullptr;
		ALandscape* Main = nullptr;
		ULandscapeInfo* Info = nullptr;
		TArray<ULandscapeComponent*> Components;
		TArray<ALandscapeProxy*> Proxies;
	};
	struct FMip { int32 W = 0, H = 0; TArray64<uint8> Bytes; };
	using FMips = TMap<UTexture2D*, TSharedPtr<FMip>>;

	TArray<UPackage*> DirtyPackages()
	{
		TArray<UPackage*> Maps, Content;
		if (!IsInGameThread() || !GEditor) { return Maps; }
		UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Maps);
		UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
		for (UPackage* Package : Content) { Maps.AddUnique(Package); }
		Maps.Remove(nullptr);
		return Maps;
	}
	TArray<FString> DirtyNames()
	{
		TArray<FString> Names;
		for (const UPackage* Package : DirtyPackages()) { Names.Add(Package->GetName()); }
		Names.Sort();
		return Names;
	}
	TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Names)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Name : Names) { Values.Add(MakeShared<FJsonValueString>(Name)); }
		return Values;
	}
	FString Finish(const TSharedRef<FJsonObject>& Result, bool bSuccess, const FString& Error = FString())
	{
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("map"), Map);
		Result->SetArrayField(TEXT("dirty_packages"), Strings(DirtyNames()));
		if (!Error.IsEmpty())
		{
			Result->SetStringField(TEXT("error"), Error);
			UE_LOG(LogRogue10m, Warning, TEXT("NaturalBoundary20261117: %s"), *Error);
		}
		FString Text;
		FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
		return Text;
	}
	bool Resolve(FState& State, FString& Error)
	{
		FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(Project);
		if (!IsInGameThread() || Project != TEXT("D:/Project/Rogue10m") || !GEditor || GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
		{ Error = TEXT("Exact project editor game thread with PIE stopped required."); return false; }
		State.World = GEditor->GetEditorWorldContext().World();
		if (!State.World || State.World->WorldType != EWorldType::Editor || State.World->GetPackage()->GetName() != Map || !State.World->GetWorldPartition())
		{ Error = TEXT("Exact candidate World Partition editor world must already be loaded; no world is switched."); return false; }
		int32 MainCount = 0;
		TSet<FIntPoint> Bases;
		TSet<ULandscapeHeightfieldCollisionComponent*> Collisions;
		for (TActorIterator<ALandscapeProxy> It(State.World); It; ++It)
		{
			ALandscapeProxy* Proxy = *It;
			if (Proxy->GetLandscapeGuid() != LandscapeGuid) { Error = TEXT("Unexpected Landscape GUID."); return false; }
			State.Proxies.Add(Proxy);
			if (ALandscape* Main = Cast<ALandscape>(Proxy)) { State.Main = Main; ++MainCount; }
			for (ULandscapeComponent* Component : Proxy->LandscapeComponents)
			{
				if (!IsValid(Component) || !Component->IsRegistered() || Component->ComponentSizeQuads != 63 || Component->NumSubsections != 1 || Component->SubsectionSizeQuads != 63)
				{ Error = TEXT("Invalid, unregistered or unexpected Landscape component layout."); return false; }
				const FIntPoint Base = Component->GetSectionBase();
				if (Base.X < MinX || Base.X >= MaxX || Base.Y < MinY || Base.Y >= MaxY || Base.X % 63 || Base.Y % 63 || Bases.Contains(Base))
				{ Error = TEXT("Duplicate/outside retained45 component."); return false; }
				ULandscapeHeightfieldCollisionComponent* Collision = Component->GetCollisionComponent();
				if (!IsValid(Collision) || !Collision->IsRegistered() || Collisions.Contains(Collision))
				{ Error = TEXT("Missing/unregistered/duplicate terrain collision."); return false; }
				Bases.Add(Base); Collisions.Add(Collision); State.Components.Add(Component);
			}
		}
		if (MainCount != 1 || State.Components.Num() != 45 || Collisions.Num() != 45 ||
			!State.Main->GetActorLocation().Equals(FVector(-90000, -90000, 0), 0.001) ||
			!State.Main->GetActorScale3D().Equals(FVector(Spacing, Spacing, 256), 0.001) || !State.Main->GetActorRotation().IsNearlyZero(0.001))
		{ Error = TEXT("Exact45 lattice/origin/scale/rotation required."); return false; }
		TSet<FGuid> LoadedProxyGuids;
		for (ALandscapeProxy* Proxy : State.Proxies) { LoadedProxyGuids.Add(Proxy->GetActorGuid()); }
		int32 DescriptorCount = 0; bool bAllTerrainLoaded = true;
		FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(State.World->GetWorldPartition(),
			[&](const FWorldPartitionActorDescInstance* Descriptor)
			{
				++DescriptorCount;
				bAllTerrainLoaded &= LoadedProxyGuids.Contains(Descriptor->GetGuid());
				return true;
			});
		if (!bAllTerrainLoaded || DescriptorCount != LoadedProxyGuids.Num())
		{ Error = TEXT("Every persistent Landscape descriptor must already be loaded; nothing was loaded by this helper."); return false; }
		State.Info = State.Main->GetLandscapeInfo();
		if (!State.Info || State.Info->XYtoComponentMap.Num() != 45 || State.Info->XYtoCollisionComponentMap.Num() != 45)
		{ Error = TEXT("LandscapeInfo must map exactly45 render/collision components."); return false; }
		for (ULandscapeComponent* Component : State.Components)
		{
			if (State.Info->XYtoComponentMap.FindRef(Component->GetSectionBase() / Component->ComponentSizeQuads) != Component ||
				State.Info->XYtoCollisionComponentMap.FindRef(Component->GetCollisionComponent()->GetSectionBase() / Component->GetCollisionComponent()->CollisionSizeQuads) != Component->GetCollisionComponent())
			{ Error = TEXT("LandscapeInfo component/collision mapping mismatch."); return false; }
		}
		const auto Layers = State.Main->GetEditLayersConst();
		if (!State.Info || Layers.Num() < 1 || Layers.Num() > 2 || !Layers[0] || Layers[0]->GetGuid() != OriginalGuid)
		{ Error = TEXT("Expected original edit layer is missing or layer count is unexpected."); return false; }
		if (Layers.Num() == 2 && (!Layers[1] || Layers[1]->GetName() != NewLayerName || Layers[1]->GetClass() != ULandscapeEditLayer::StaticClass()))
		{ Error = TEXT("Only the original and exact NaturalBoundary20261117 standard layers are accepted."); return false; }
		State.Components.Sort([](const ULandscapeComponent& A, const ULandscapeComponent& B)
		{ return A.GetSectionBase().Y == B.GetSectionBase().Y ? A.GetSectionBase().X < B.GetSectionBase().X : A.GetSectionBase().Y < B.GetSectionBase().Y; });
		return true;
	}
	const FMip* Mip(UTexture2D* Texture, FMips& Cache, FString& Error)
	{
		if (const TSharedPtr<FMip>* Found = Cache.Find(Texture)) { return Found->Get(); }
		if (!IsValid(Texture) || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8 || Texture->Source.GetNumBlocks() != 1 || Texture->Source.GetNumLayers() != 1)
		{ Error = TEXT("Expected single-block BGRA8 CPU texture source."); return nullptr; }
		TSharedPtr<FMip> Copy = MakeShared<FMip>();
		Copy->W = Texture->Source.GetSizeX(); Copy->H = Texture->Source.GetSizeY();
		if (Copy->W <= 0 || Copy->H <= 0 || !Texture->Source.GetMipData(Copy->Bytes, 0) || Copy->Bytes.Num() != int64(Copy->W) * Copy->H * 4)
		{ Error = TEXT("CPU mip0 read failed."); return nullptr; }
		return Cache.Add(Texture, MoveTemp(Copy)).Get();
	}
	bool ReadHeights(const FState& State, const FGuid& Guid, TArray<uint16>& Data, FString& Error)
	{
		Data.Init(0, Samples);
		TBitArray<> Seen(false, Samples);
		FMips Cache;
		int32 Unique = 0, Duplicates = 0;
		for (const ULandscapeComponent* Component : State.Components)
		{
			const FMip* Source = Mip(Component->GetHeightmap(Guid), Cache, Error);
			if (!Source) { return false; }
			const FIntPoint Base = Component->GetSectionBase();
			const int32 OffsetX = FMath::RoundToInt(Source->W * Component->HeightmapScaleBias.Z);
			const int32 OffsetY = FMath::RoundToInt(Source->H * Component->HeightmapScaleBias.W);
			if (OffsetX < 0 || OffsetY < 0 || OffsetX + 63 >= Source->W || OffsetY + 63 >= Source->H)
			{ Error = TEXT("Height texture coordinates exceed mip0."); return false; }
			const FColor* Colors = reinterpret_cast<const FColor*>(Source->Bytes.GetData());
			for (int32 Y = 0; Y <= 63; ++Y) for (int32 X = 0; X <= 63; ++X)
			{
				const int32 Index = Base.X + X - MinX + (Base.Y + Y - MinY) * Width;
				if (!Data.IsValidIndex(Index)) { Error = TEXT("Height lattice index exceeds the retained rectangle."); return false; }
				const FColor& Color = Colors[OffsetX + X + (OffsetY + Y) * Source->W];
				const uint16 Value = (uint16(Color.R) << 8) | Color.G;
				if (Seen[Index]) { if (Data[Index] != Value) { Error = FString::Printf(TEXT("Shared height seam mismatch at (%d,%d): %u != %u."), Base.X + X, Base.Y + Y, Data[Index], Value); return false; } ++Duplicates; }
				else { Seen[Index] = true; Data[Index] = Value; ++Unique; }
			}
		}
		if (Unique != Samples || Duplicates != 4832) { Error = TEXT("Incomplete retained grid/seam coverage."); return false; }
		return true;
	}
	void HashText(FBlake3& Hash, const FString& Text)
	{
		FTCHARToUTF8 Utf8(*Text);
		const int32 Length = Utf8.Length();
		Hash.Update(&Length, sizeof(Length)); Hash.Update(Utf8.Get(), Length);
	}
	FString HeightHash(const TArray<uint16>& Data)
	{
		// This exact helper targets Win64 (little endian); rows are Y-major, X-fastest.
		return LexToString(FBlake3::HashBuffer(Data.GetData(), uint64(Data.Num()) * sizeof(uint16)));
	}
	bool OriginalHash(const FState& State, FString& Digest, FString& Error)
	{
		FBlake3 Hash;
		const ULandscapeEditLayerBase* Layer = State.Main->GetEditLayerConst(OriginalGuid);
		if (!Layer) { Error = TEXT("Original layer disappeared."); return false; }
		HashText(Hash, Layer->GetGuid().ToString()); HashText(Hash, Layer->GetName().ToString()); HashText(Hash, Layer->GetClass()->GetPathName());
		HashText(Hash, FString::Printf(TEXT("%.9g;%.9g;%d;%d"), Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap), Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap), Layer->IsVisible(), Layer->IsLocked()));
		TSet<UTexture2D*> TextureSet;
		for (const ULandscapeComponent* Component : State.Components)
		{
			HashText(Hash, Component->GetPathName());
			UTexture2D* HeightTexture = Component->GetHeightmap(OriginalGuid);
			if (!IsValid(HeightTexture)) { Error = TEXT("Original layer height texture is missing."); return false; }
			HashText(Hash, HeightTexture->GetPathName()); TextureSet.Add(HeightTexture);
			const auto& HB = Component->HeightmapScaleBias;
			const auto& WB = Component->WeightmapScaleBias;
			HashText(Hash, FString::Printf(TEXT("%.17g;%.17g;%.17g;%.17g;%.17g;%.17g;%.17g;%.17g"),
				HB.X, HB.Y, HB.Z, HB.W, WB.X, WB.Y, WB.Z, WB.W));
			for (UTexture2D* Texture : Component->GetWeightmapTextures(OriginalGuid))
			{
				if (!IsValid(Texture)) { Error = TEXT("Original layer paint texture is missing."); return false; }
				HashText(Hash, Texture->GetPathName()); TextureSet.Add(Texture);
			}
			for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(OriginalGuid))
			{
				HashText(Hash, IsValid(Allocation.LayerInfo) ? Allocation.LayerInfo->GetPathName() : TEXT("null"));
				HashText(Hash, FString::Printf(TEXT("%u;%u"), Allocation.WeightmapTextureIndex, Allocation.WeightmapTextureChannel));
			}
		}
		TArray<UTexture2D*> Textures = TextureSet.Array();
		Textures.Sort([](const UTexture2D& A, const UTexture2D& B) { return A.GetPathName() < B.GetPathName(); });
		FMips Cache;
		for (UTexture2D* Texture : Textures)
		{
			const FMip* Source = Mip(Texture, Cache, Error);
			if (!Source) { return false; }
			HashText(Hash, Texture->GetPathName()); HashText(Hash, FString::Printf(TEXT("%d;%d"), Source->W, Source->H));
			Hash.Update(Source->Bytes.GetData(), Source->Bytes.Num());
		}
		Digest = LexToString(Hash.Finalize());
		return true;
	}
	bool Parse(const FString& Text, const FString& Schema, TSharedPtr<FJsonObject>& Json, FString& Error)
	{
		if (Text.Len() > 8 * 1024 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json)
		{ Error = TEXT("Invalid/oversized JSON."); return false; }
		FString ActualSchema, ActualMap;
		if (!Json->TryGetStringField(TEXT("schema"), ActualSchema) || ActualSchema != Schema || !Json->TryGetStringField(TEXT("map"), ActualMap) || ActualMap != Map)
		{ Error = TEXT("Exact JSON schema/map required."); return false; }
		return true;
	}
	bool ExpectedHashes(const TSharedPtr<FJsonObject>& Json, const FState& State, TArray<uint16>& Current, FString& Original, FString& Error)
	{
		FString ExpectedComposite, ExpectedOriginal;
		if (!Json->TryGetStringField(TEXT("expected_composite_blake3"), ExpectedComposite) || !Json->TryGetStringField(TEXT("expected_original_layer_blake3"), ExpectedOriginal) ||
			!ReadHeights(State, FGuid(), Current, Error) || !OriginalHash(State, Original, Error))
		{ if (Error.IsEmpty()) { Error = TEXT("Both native inspection hashes are required."); } return false; }
		if (!HeightHash(Current).Equals(ExpectedComposite, ESearchCase::IgnoreCase) || !Original.Equals(ExpectedOriginal, ESearchCase::IgnoreCase))
		{ Error = TEXT("Stale/unexpected composite or original-layer data; nothing was edited."); return false; }
		return true;
	}
	bool PackageAllowed(const FString& Name)
	{
		const FString Tail = Map.RightChop(6) + TEXT("/");
		return FPackageName::IsValidLongPackageName(Name) && (Name == Map || Name.StartsWith(TEXT("/Game/__ExternalActors__/") + Tail) ||
			Name.StartsWith(TEXT("/Game/__ExternalObjects__/") + Tail) || Name.StartsWith(OwnedPrefix));
	}
}
#endif // WITH_EDITOR

FString URogue10mNaturalBoundary20261117Authoring::InspectCandidate(bool bIncludeHeights)
{
#if WITH_EDITOR
	using namespace Rogue10mNaturalBoundary20261117;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const TArray<FString> Before = DirtyNames();
	FState State; FString Error, Original; TArray<uint16> Current;
	if (!Resolve(State, Error) || !ReadHeights(State, FGuid(), Current, Error) || !OriginalHash(State, Original, Error)) { return Finish(Result, false, Error); }
	Result->SetNumberField(TEXT("component_count"), 45); Result->SetNumberField(TEXT("collision_count"), 45);
	Result->SetStringField(TEXT("landscape_guid"), LandscapeGuid.ToString()); Result->SetStringField(TEXT("source_layer_guid"), OriginalGuid.ToString());
	Result->SetNumberField(TEXT("layer_count"), State.Main->GetEditLayersConst().Num());
	Result->SetNumberField(TEXT("width"), Width); Result->SetNumberField(TEXT("height"), Height);
	Result->SetStringField(TEXT("composite_blake3"), HeightHash(Current)); Result->SetStringField(TEXT("original_layer_blake3"), Original);
	Result->SetBoolField(TEXT("landscape_reports_up_to_date"), State.Main->IsUpToDate());
	Result->SetBoolField(TEXT("read_only"), true); Result->SetBoolField(TEXT("dirty_package_names_unchanged"), Before == DirtyNames());
	if (bIncludeHeights)
	{
		TArray<TSharedPtr<FJsonValue>> Values; Values.Reserve(Samples);
		for (uint16 Value : Current) { Values.Add(MakeShared<FJsonValueNumber>(double(Value))); }
		Result->SetArrayField(TEXT("composite_heights_uint16"), Values);
	}
	return Finish(Result, Before == DirtyNames(), Before == DirtyNames() ? FString() : TEXT("Read-only inspection changed dirty package names."));
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required\"}");
#endif
}

FString URogue10mNaturalBoundary20261117Authoring::ProbePawnCapsule(const FVector& StartCm, const FVector& EndCm, float RadiusCm, float HalfHeightCm)
{
#if WITH_EDITOR
	using namespace Rogue10mNaturalBoundary20261117;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("read_only"), true);
	FState State; FString Error;
	if (!Resolve(State, Error) || GEditor->IsTransactionActive() || !State.Main->IsUpToDate())
	{ return Finish(Result, false, Error.IsEmpty() ? TEXT("Idle, settled exact candidate required for the capsule probe.") : Error); }
	const auto InBounds = [](const FVector& P)
	{
		return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z) &&
			P.X >= -38750 && P.X <= 72500 && P.Y >= 17500 && P.Y <= 83750 && P.Z >= -10000 && P.Z <= 40000;
	};
	if (!InBounds(StartCm) || !InBounds(EndCm) || !FMath::IsFinite(RadiusCm) || !FMath::IsFinite(HalfHeightCm) ||
		RadiusCm < 1 || RadiusCm > 100 || HalfHeightCm < RadiusCm || HalfHeightCm > 200 ||
		FVector::DistSquared(StartCm, EndCm) < 1 || FVector::DistSquared(StartCm, EndCm) > FMath::Square(10000.0))
	{ return Finish(Result, false, TEXT("Finite capsule centers within the candidate envelope+50m, Z[-100m,400m], sweep1cm..100m, radius1..100cm and halfheight[radius..200cm] required.")); }
	const TArray<FString> Before = DirtyNames();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mNaturalBoundaryPawnProbe), false);
	Query.bFindInitialOverlaps = true;
	Query.bReturnPhysicalMaterial = false;
	FHitResult Hit;
	const bool bBlocked = State.World->SweepSingleByChannel(Hit, StartCm, EndCm, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeCapsule(RadiusCm, HalfHeightCm), Query);
	const auto VectorJson = [](const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Add(MakeShared<FJsonValueNumber>(Value.X)); Array.Add(MakeShared<FJsonValueNumber>(Value.Y)); Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return Array;
	};
	Result->SetStringField(TEXT("query"), TEXT("upright_simple_capsule_sweep"));
	Result->SetStringField(TEXT("channel"), TEXT("ECC_Pawn"));
	Result->SetBoolField(TEXT("trace_complex"), false);
	Result->SetNumberField(TEXT("radius_cm"), RadiusCm); Result->SetNumberField(TEXT("half_height_cm"), HalfHeightCm);
	Result->SetArrayField(TEXT("start_cm"), VectorJson(StartCm)); Result->SetArrayField(TEXT("end_cm"), VectorJson(EndCm));
	Result->SetBoolField(TEXT("blocking_hit"), bBlocked);
	Result->SetBoolField(TEXT("dirty_package_names_unchanged"), Before == DirtyNames());
	Result->SetStringField(TEXT("boundary"), TEXT("One static Pawn-channel query; no movement, jump, navigation or physical-input certification."));
	if (bBlocked)
	{
		if (!Hit.bBlockingHit || Hit.Location.ContainsNaN() || Hit.ImpactPoint.ContainsNaN() || Hit.Normal.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
			!FMath::IsFinite(Hit.Time) || !FMath::IsFinite(Hit.Distance) || !FMath::IsFinite(Hit.PenetrationDepth))
		{ return Finish(Result, false, TEXT("Native capsule query returned invalid/nonfinite hit data.")); }
		const TSharedRef<FJsonObject> Facts = MakeShared<FJsonObject>();
		Facts->SetStringField(TEXT("actor"), Hit.GetActor() ? Hit.GetActor()->GetPathName() : TEXT(""));
		Facts->SetStringField(TEXT("component"), Hit.GetComponent() ? Hit.GetComponent()->GetPathName() : TEXT(""));
		Facts->SetArrayField(TEXT("location_cm"), VectorJson(Hit.Location));
		Facts->SetArrayField(TEXT("impact_point_cm"), VectorJson(Hit.ImpactPoint));
		Facts->SetArrayField(TEXT("normal"), VectorJson(Hit.Normal));
		Facts->SetArrayField(TEXT("impact_normal"), VectorJson(Hit.ImpactNormal));
		Facts->SetNumberField(TEXT("fraction"), Hit.Time); Facts->SetNumberField(TEXT("distance_cm"), Hit.Distance);
		Facts->SetBoolField(TEXT("start_penetrating"), Hit.bStartPenetrating); Facts->SetNumberField(TEXT("penetration_depth_cm"), Hit.PenetrationDepth);
		Result->SetObjectField(TEXT("hit"), Facts);
	}
	else { Result->SetField(TEXT("hit"), MakeShared<FJsonValueNull>()); }
	return Finish(Result, Before == DirtyNames(), Before == DirtyNames() ? FString() : TEXT("Read-only capsule query changed dirty package names."));
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required\"}");
#endif
}

FString URogue10mNaturalBoundary20261117Authoring::ApplyHeightPlan(const FString& HeightPlanJson, bool bConfirm)
{
#if WITH_EDITOR
	using namespace Rogue10mNaturalBoundary20261117;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("mutation_started"), false); Result->SetBoolField(TEXT("packages_saved"), false);
	FState State; FString Error, Original; TArray<uint16> Current; TSharedPtr<FJsonObject> Json;
	if (!bConfirm || !Resolve(State, Error) || !FApp::CanEverRender() || IsRunningCommandlet() || GEditor->IsTransactionActive() ||
		!DirtyNames().IsEmpty() || State.Main->GetEditLayersConst().Num() != 1 || State.Main->GetLayerIndex(NewLayerName) != INDEX_NONE || !State.Main->IsUpToDate())
	{ return Finish(Result, false, Error.IsEmpty() ? TEXT("Explicit confirmation, rendered/settled editor, clean packages, no active transaction and sole original layer required.") : Error); }
	if (!Parse(HeightPlanJson, TEXT("Rogue10m.NaturalBoundaryHeightPlan.20261117.v1"), Json, Error) || !ExpectedHashes(Json, State, Current, Original, Error)) { return Finish(Result, false, Error); }
	const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	FString Guid, SourceGuid, Name; double W = 0, H = 0, Plateau = 0;
	if (!Json->TryGetStringField(TEXT("landscape_guid"), Guid) || !Json->TryGetStringField(TEXT("source_layer_guid"), SourceGuid) || !Json->TryGetStringField(TEXT("new_layer_name"), Name) ||
		Guid != LandscapeGuid.ToString() || SourceGuid != OriginalGuid.ToString() || Name != NewLayerName.ToString() ||
		!Json->TryGetNumberField(TEXT("width"), W) || W != Width || !Json->TryGetNumberField(TEXT("height"), H) || H != Height ||
		!Json->TryGetNumberField(TEXT("plateau_z_cm"), Plateau) || Plateau != 9600 ||
		!Json->TryGetArrayField(TEXT("vertex_bounds"), Bounds) || Bounds->Num() != 4 ||
		!Json->TryGetArrayField(TEXT("target_heights"), Values) || Values->Num() != Samples)
	{ return Finish(Result, false, TEXT("Exact grid dimensions/bounds/GUID/layer/9600cm plateau and full target array required.")); }
	const int32 ExpectedBounds[] = {MinX, MinY, MaxX, MaxY};
	for (int32 I = 0; I < 4; ++I) { double Value = 0; if (!(*Bounds)[I]->TryGetNumber(Value) || Value != ExpectedBounds[I]) { return Finish(Result, false, TEXT("Vertex bounds mismatch.")); } }
	TArray<uint16> Target, Delta; Target.Reserve(Samples); Delta.Reserve(Samples);
	for (int32 I = 0; I < Samples; ++I)
	{
		double Value = 0;
		if (!(*Values)[I]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0 || Value > 65535 || FMath::FloorToDouble(Value) != Value)
		{ return Finish(Result, false, TEXT("Target sample is not a finite uint16 integer.")); }
		const int32 Difference = 32768 + int32(Value) - int32(Current[I]);
		if (Difference < 0 || Difference > 65535) { return Finish(Result, false, TEXT("Additive delta would overflow uint16; no layer was created.")); }
		Target.Add(uint16(Value)); Delta.Add(uint16(Difference));
	}
	// All guards and all 179488 target/delta samples have been checked before the first mutation.
	if (!DirtyNames().IsEmpty()) { return Finish(Result, false, TEXT("A package became dirty during preflight; nothing was edited.")); }
	Result->SetStringField(TEXT("target_composite_blake3"), HeightHash(Target));
	Result->SetStringField(TEXT("expected_original_layer_blake3"), Original);
	Result->SetBoolField(TEXT("mutation_started"), true);
	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10m", "NaturalBoundary20261117", "Author Natural Boundary 20261117"));
	State.Main->Modify();
	for (ALandscapeProxy* Proxy : State.Proxies) { Proxy->Modify(); }
	const int32 LayerIndex = State.Main->CreateLayer(NewLayerName, ULandscapeEditLayer::StaticClass());
	ULandscapeEditLayerBase* Layer = LayerIndex != INDEX_NONE ? State.Main->GetEditLayer(LayerIndex) : nullptr;
	if (!Layer || Layer->GetName() != NewLayerName || Layer->GetClass() != ULandscapeEditLayer::StaticClass())
	{ return Finish(Result, false, TEXT("Layer creation failed/partial; inspect unsaved editor state, do not retry.")); }
	Result->SetStringField(TEXT("new_layer_guid"), Layer->GetGuid().ToString());
	Layer->SetAlphaForTargetType(ELandscapeToolTargetType::Heightmap, 1.0f, true, EPropertyChangeType::ValueSet);
	Layer->SetAlphaForTargetType(ELandscapeToolTargetType::Weightmap, 0.0f, true, EPropertyChangeType::ValueSet);
	TArray<uint16> Neutral;
	if (!ReadHeights(State, Layer->GetGuid(), Neutral, Error) || Neutral.ContainsByPredicate([](uint16 Value) { return Value != 32768; }))
	{ return Finish(Result, false, TEXT("New layer is not initialized to native neutral32768; partial state retained, no automatic undo/retry.")); }
	{
		FLandscapeEditDataInterface Edit(State.Info, Layer->GetGuid());
		Edit.SetHeightData(MinX, MinY, MaxX, MaxY, Delta.GetData(), Width, false, nullptr, nullptr, nullptr, false, nullptr, nullptr, true, true, true);
		Edit.Flush();
	}
	State.Main->ForceLayersFullUpdate();
	FString OriginalAfter;
	if (!OriginalHash(State, OriginalAfter, Error) || OriginalAfter != Original)
	{ UE_LOG(LogRogue10m, Error, TEXT("NaturalBoundary20261117 original layer preservation failed; unsaved partial state retained.")); return Finish(Result, false, TEXT("Original layer RG/paint/B/A/metadata hash changed; do not save or retry.")); }
	Result->SetStringField(TEXT("new_layer_guid"), Layer->GetGuid().ToString());
	Result->SetBoolField(TEXT("original_layer_preserved"), true);
	Result->SetBoolField(TEXT("full_update_requested"), true);
	Result->SetBoolField(TEXT("settled_and_verified"), false);
	Result->SetStringField(TEXT("status"), TEXT("applied_unsaved_requires_settle_and_verification"));
	UE_LOG(LogRogue10m, Log, TEXT("NaturalBoundary20261117 applied one height-only additive layer; 45 components retained, unsaved; verify settled composite/collision before saving."));
	return Finish(Result, true);
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required\"}");
#endif
}

FString URogue10mNaturalBoundary20261117Authoring::ReviseHeightPlan(const FString& HeightRevisionJson, bool bConfirm)
{
#if WITH_EDITOR
	using namespace Rogue10mNaturalBoundary20261117;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("mutation_started"), false);
	Result->SetBoolField(TEXT("packages_saved"), false);
	Result->SetNumberField(TEXT("layers_created"), 0);
	FState State; FString Error, Original; TArray<uint16> Current; TSharedPtr<FJsonObject> Json;
	if (!bConfirm || !Resolve(State, Error) || !FApp::CanEverRender() || IsRunningCommandlet() || GEditor->IsTransactionActive() ||
		!DirtyNames().IsEmpty() || State.Main->GetEditLayersConst().Num() != 2 || !State.Main->IsUpToDate())
	{ return Finish(Result, false, Error.IsEmpty() ? TEXT("Revision requires confirmation, rendered/settled exact two-layer editor, clean packages and no active transaction.") : Error); }
	const ULandscapeEditLayerBase* Base = State.Main->GetEditLayerConst(OriginalGuid);
	ULandscapeEditLayerBase* Layer = State.Main->GetEditLayer(NewLayerName);
	if (!Base || Base->GetClass() != ULandscapeEditLayer::StaticClass() || !Base->IsVisible() ||
		Base->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap) != 1.0f || Base->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap) != 1.0f ||
		!Layer || Layer->GetClass() != ULandscapeEditLayer::StaticClass() || !Layer->IsVisible() || Layer->IsLocked() ||
		Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap) != 1.0f || Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap) != 0.0f)
	{ return Finish(Result, false, TEXT("Exact visible standard layers required: original alpha1/1; unlocked named boundary alpha1/0.")); }
	if (!Parse(HeightRevisionJson, TEXT("Rogue10m.NaturalBoundaryHeightRevision.20261118.v1"), Json, Error) ||
		!ExpectedHashes(Json, State, Current, Original, Error)) { return Finish(Result, false, Error); }
	const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	FString Guid, SourceGuid, ExpectedLayerGuid, Name; double W = 0, H = 0, Plateau = 0;
	if (!Json->TryGetStringField(TEXT("landscape_guid"), Guid) || Guid != LandscapeGuid.ToString() ||
		!Json->TryGetStringField(TEXT("source_layer_guid"), SourceGuid) || SourceGuid != OriginalGuid.ToString() ||
		!Json->TryGetStringField(TEXT("expected_new_layer_guid"), ExpectedLayerGuid) || ExpectedLayerGuid != Layer->GetGuid().ToString() ||
		!Json->TryGetStringField(TEXT("new_layer_name"), Name) || Name != NewLayerName.ToString() ||
		!Json->TryGetNumberField(TEXT("width"), W) || W != Width || !Json->TryGetNumberField(TEXT("height"), H) || H != Height ||
		!Json->TryGetNumberField(TEXT("plateau_z_cm"), Plateau) || Plateau != 9600 ||
		!Json->TryGetArrayField(TEXT("vertex_bounds"), Bounds) || Bounds->Num() != 4 ||
		!Json->TryGetArrayField(TEXT("target_heights"), Values) || Values->Num() != Samples)
	{ return Finish(Result, false, TEXT("Revision GUIDs/name/bounds/dimensions/9600cm plateau and complete target grid must match the saved candidate.")); }
	const int32 ExpectedBounds[] = {MinX, MinY, MaxX, MaxY};
	for (int32 I = 0; I < 4; ++I)
	{ double Value = 0; if (!(*Bounds)[I]->TryGetNumber(Value) || Value != ExpectedBounds[I]) { return Finish(Result, false, TEXT("Revision vertex bounds mismatch.")); } }
	TArray<uint16> OriginalHeights, ExistingDelta;
	if (!ReadHeights(State, OriginalGuid, OriginalHeights, Error) || !ReadHeights(State, Layer->GetGuid(), ExistingDelta, Error))
	{ return Finish(Result, false, Error); }
	TArray<uint16> Target, Delta; Target.Reserve(Samples); Delta.Reserve(Samples);
	int32 ChangedSamples = 0;
	for (int32 I = 0; I < Samples; ++I)
	{
		const int32 ExistingComposite = int32(OriginalHeights[I]) + int32(ExistingDelta[I]) - 32768;
		if (ExistingComposite < 0 || ExistingComposite > 65535 || ExistingComposite != int32(Current[I]))
		{ return Finish(Result, false, TEXT("Current composite is not the exact unit-alpha sum of the two raw layers; no revision was performed.")); }
		double Value = 0;
		if (!(*Values)[I]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0 || Value > 65535 || FMath::FloorToDouble(Value) != Value)
		{ return Finish(Result, false, TEXT("Revision target sample is not a finite uint16 integer.")); }
		// Replacement content is relative to ORIGINAL RAW data, not to the
		// already-composited old boundary. Subtracting Current would be wrong.
		const int32 Difference = 32768 + int32(Value) - int32(OriginalHeights[I]);
		if (Difference < 0 || Difference > 65535)
		{ return Finish(Result, false, TEXT("Revision additive delta overflows uint16; nothing was modified.")); }
		Target.Add(uint16(Value)); Delta.Add(uint16(Difference));
		ChangedSamples += uint16(Value) != Current[I] ? 1 : 0;
	}
	if (ChangedSamples == 0 || !DirtyNames().IsEmpty())
	{ return Finish(Result, false, TEXT("Revision is a no-op or a package became dirty during preflight; nothing was modified.")); }
	Result->SetStringField(TEXT("previous_composite_blake3"), HeightHash(Current));
	Result->SetStringField(TEXT("target_composite_blake3"), HeightHash(Target));
	Result->SetStringField(TEXT("original_raw_height_blake3"), HeightHash(OriginalHeights));
	Result->SetStringField(TEXT("expected_original_layer_blake3"), Original);
	Result->SetStringField(TEXT("new_layer_guid"), Layer->GetGuid().ToString());
	Result->SetNumberField(TEXT("changed_samples"), ChangedSamples);
	// The complete target/delta and current raw composition were checked
	// before any transaction, Modify, source write or update request.
	Result->SetBoolField(TEXT("mutation_started"), true);
	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10m", "NaturalBoundaryHeightRevision20261118", "Revise Natural Boundary Height 20261118"));
	Layer->Modify();
	for (ALandscapeProxy* Proxy : State.Proxies) { Proxy->Modify(); }
	{
		FLandscapeEditDataInterface Edit(State.Info, Layer->GetGuid());
		Edit.SetHeightData(MinX, MinY, MaxX, MaxY, Delta.GetData(), Width, false, nullptr, nullptr, nullptr, false, nullptr, nullptr, true, true, true);
		Edit.Flush();
	}
	State.Main->ForceLayersFullUpdate();
	FString OriginalAfter;
	if (!OriginalHash(State, OriginalAfter, Error) || OriginalAfter != Original)
	{ UE_LOG(LogRogue10m, Error, TEXT("NaturalBoundary20261118 revision changed original layer data; unsaved partial state retained.")); return Finish(Result, false, TEXT("Revision original-layer preservation failed; do not save or retry.")); }
	Result->SetBoolField(TEXT("original_layer_preserved"), true);
	Result->SetBoolField(TEXT("existing_layer_revised"), true);
	Result->SetBoolField(TEXT("unsaved"), true);
	Result->SetBoolField(TEXT("full_update_requested"), true);
	Result->SetBoolField(TEXT("settled_and_verified"), false);
	Result->SetStringField(TEXT("status"), TEXT("height_revision_unsaved_requires_settle_and_verification"));
	UE_LOG(LogRogue10m, Log, TEXT("NaturalBoundary20261118 revised %d height samples in the existing layer; no layer created and no packages saved; verify settled composite/collision."), ChangedSamples);
	return Finish(Result, true);
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required\"}");
#endif
}

FString URogue10mNaturalBoundary20261117Authoring::SaveCandidatePackages(const FString& SavePlanJson)
{
#if WITH_EDITOR
	using namespace Rogue10mNaturalBoundary20261117;
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("save_called"), false);
	FState State; FString Error, Original; TArray<uint16> Current; TSharedPtr<FJsonObject> Json;
	if (!Resolve(State, Error) || GEditor->IsTransactionActive() || !State.Main->IsUpToDate() ||
		!Parse(SavePlanJson, TEXT("Rogue10m.NaturalBoundaryPackageSave.20261117.v1"), Json, Error) || !ExpectedHashes(Json, State, Current, Original, Error))
	{ return Finish(Result, false, Error.IsEmpty() ? TEXT("Settled candidate with no active transaction required.") : Error); }
	bool bConfirm = false; const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	if (!Json->TryGetBoolField(TEXT("confirm_save"), bConfirm) || !bConfirm || !Json->TryGetArrayField(TEXT("packages"), Names) || Names->IsEmpty() || Names->Num() > 4096)
	{ return Finish(Result, false, TEXT("Explicit confirm_save and nonempty bounded package allowlist required.")); }
	TArray<UPackage*> Packages; TSet<FString> Allowed; TArray<FString> PackageNames;
	for (const TSharedPtr<FJsonValue>& Value : *Names)
	{
		FString Name;
		if (!Value->TryGetString(Name) || !PackageAllowed(Name) || Allowed.Contains(Name)) { return Finish(Result, false, TEXT("Duplicate/foreign/invalid save package; nothing saved.")); }
		UPackage* Package = FindPackage(nullptr, *Name);
		if (!Package) { return Finish(Result, false, TEXT("Every explicit save package must already be loaded; no implicit asset loading.")); }
		Allowed.Add(Name); PackageNames.Add(Name); Packages.Add(Package);
	}
	const TArray<FString> Before = DirtyNames();
	for (const FString& Dirty : Before)
	{ if (!Allowed.Contains(Dirty)) { return Finish(Result, false, TEXT("Unexpected dirty package outside the exact supplied allowlist: ") + Dirty); } }
	Result->SetArrayField(TEXT("requested_packages"), Strings(PackageNames));
	Result->SetArrayField(TEXT("dirty_before"), Strings(Before));
	Result->SetBoolField(TEXT("save_called"), true);
	const bool bNative = UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	const TArray<FString> After = DirtyNames();
	Result->SetBoolField(TEXT("native_save_returned"), bNative);
	Result->SetBoolField(TEXT("save_all_dirty_called"), false);
	Result->SetBoolField(TEXT("requires_disk_and_cold_verification"), true);
	UE_LOG(LogRogue10m, Log, TEXT("NaturalBoundary20261117 exact loaded-package save returned=%d for %d packages; disk/cold proof required."), bNative, Packages.Num());
	return Finish(Result, bNative && After.IsEmpty(), bNative && After.IsEmpty() ? FString() : TEXT("Native package save failed or dirty packages remain; preserve partial results, no automatic retry."));
#else
	return TEXT("{\"success\":false,\"error\":\"Editor build required\"}");
#endif
}
