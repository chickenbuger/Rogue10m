// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/Rogue10mRecoveredCaveTransfer.h"

#if WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include "Core/Rogue10m.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/Parse.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UnrealEdGlobals.h"

namespace
{
	constexpr TCHAR ArchiveMap[] = TEXT("/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A");
	constexpr TCHAR ActiveMap[] = TEXT("/Game/Rogue10m/Maps/OpenRunMap");
	constexpr TCHAR ArchiveActorPrefix[] = TEXT("/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/");
	constexpr TCHAR ActiveActorPrefix[] = TEXT("/Game/__ExternalActors__/Rogue10m/Maps/OpenRunMap/");
	constexpr TCHAR ArchiveAssetPrefix[] = TEXT("/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/");
	constexpr TCHAR RecoveredAssetPrefix[] = TEXT("/Game/Rogue10m/World/OpenWorld/RecoveredTerrain20261119/Caves/");

	struct FCaveActorSpec
	{
		const TCHAR* Label;
		const TCHAR* Name;
		const TCHAR* RelativePackage;
	};

	const FCaveActorSpec Specs[] = {
		{ TEXT("CV18_Q_Structure"), TEXT("Actor_UAID_244BFE8DA9DFE81503_1400486843"), TEXT("9/EE/NR3SMNORZ57AQ1FV26VAFO") },
		{ TEXT("CV18_Q_Dressing"), TEXT("Actor_UAID_244BFE8DA9DFE81503_1542298845"), TEXT("7/0E/NL2M3149WONPRWLKFKVP6Y") },
		{ TEXT("CV18_Z_Structure"), TEXT("Actor_UAID_244BFE8DA9DFE81503_1400618844"), TEXT("3/BY/2CW0JPTDIEMKKD1T5LATX6") },
		{ TEXT("CV18_Z_Dressing"), TEXT("Actor_UAID_244BFE8DA9DFE81503_1542374846"), TEXT("2/VQ/PYZIJL655JALHJM6PS58A8") },
	};

	const FCaveActorSpec* FindSpecByLabel(const FString& Label)
	{
		for (const FCaveActorSpec& Spec : Specs)
		{
			if (Label == Spec.Label) { return &Spec; }
		}
		return nullptr;
	}

	UWorld* FindEditorWorld(const TCHAR* ExpectedPackage)
	{
		UWorld* World = GEditor && GUnrealEd && !GEditor->PlayWorld ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || World->WorldType != EWorldType::Editor || World->GetPackage()->GetName() != ExpectedPackage
			|| !World->GetWorldPartition() || !World->PersistentLevel || World->GetCurrentLevel() != World->PersistentLevel
			|| !World->PersistentLevel->IsUsingExternalActors())
		{
			UE_LOG(LogRogue10m, Error, TEXT("Cave recovery requires the idle World Partition editor for %s."), ExpectedPackage);
			return nullptr;
		}
		return World;
	}

	bool IsExactTransferText(const FString& Text)
	{
		if (Text.IsEmpty() || Text.Len() > 16 * 1024 * 1024) { return false; }
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		TSet<FString> Actors;
		for (const FString& Line : Lines)
		{
			const FString Header = Line.TrimStart();
			if (!Header.StartsWith(TEXT("Begin Actor "))) { continue; }
			FString Class;
			FString Name;
			if (!FParse::Value(*Header, TEXT("Class="), Class) || Class != TEXT("/Script/Engine.Actor")
				|| !FParse::Value(*Header, TEXT("Name="), Name) || Actors.Contains(Name)) { return false; }
			Actors.Add(MoveTemp(Name));
		}
		if (Actors.Num() != UE_ARRAY_COUNT(Specs)) { return false; }
		for (const FCaveActorSpec& Spec : Specs)
		{
			if (!Actors.Contains(Spec.Name)) { return false; }
		}
		return true;
	}

	bool HasRecoveredShell(const AActor& Actor, TCHAR Site)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(&Actor);
		const FString ShellName = FString::Printf(TEXT("Organic_%c_Shell"), Site);
		const FString MeshName = FString::Printf(TEXT("%sSM_CaveOrganic_%c.SM_CaveOrganic_%c"), RecoveredAssetPrefix, Site, Site);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (Component->GetName() != ShellName) { continue; }
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			return Mesh && Mesh->GetPathName() == MeshName && Component->GetVisibleFlag() && !Component->bHiddenInGame
				&& Component->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics
				&& Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
				&& Component->GetCollisionResponseToChannel(ECC_Camera) == ECR_Block;
		}
		return false;
	}
}
#endif

FString URogue10mRecoveredCaveTransfer::ExportArchivedCaveActors()
{
#if WITH_EDITOR
	UWorld* World = FindEditorWorld(ArchiveMap);
	if (!World) { return {}; }

	TArray<AActor*> Actors;
	TSet<FString> Labels;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		const FString Label = Actor->GetActorLabel(false);
		const FCaveActorSpec* Spec = FindSpecByLabel(Label);
		if (!Spec) { continue; }
		const UPackage* ExternalPackage = Actor->GetExternalPackage();
		if (Actor->GetClass() != AActor::StaticClass() || Actor->GetLevel() != World->PersistentLevel
			|| Actor->GetName() != Spec->Name || !ExternalPackage
			|| ExternalPackage->GetName() != FString(ArchiveActorPrefix) + Spec->RelativePackage || Labels.Contains(Label))
		{
			UE_LOG(LogRogue10m, Error, TEXT("Unexpected archived cave actor identity: %s."), *Actor->GetPathName());
			return {};
		}
		Labels.Add(Label);
		Actors.Add(Actor);
	}
	if (Actors.Num() != UE_ARRAY_COUNT(Specs))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Cave recovery found %d out of four loaded archived actors."), Actors.Num());
		return {};
	}

	FString ActorText;
	GUnrealEd->CopyActors(Actors, World, &ActorText);
	if (!IsExactTransferText(ActorText))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Native copy did not export the four expected cave actors."));
		return {};
	}
	return ActorText;
#else
	return {};
#endif
}

TArray<AActor*> URogue10mRecoveredCaveTransfer::PasteRecoveredCaveActors(const FString& ActorText, bool bConfirm)
{
#if WITH_EDITOR
	UWorld* World = FindEditorWorld(ActiveMap);
	if (!World || !bConfirm || !IsExactTransferText(ActorText) || ActorText.Contains(ArchiveAssetPrefix))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Cave paste needs confirmed, canonical-asset text for the exact four recovered actors."));
		return {};
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (FindSpecByLabel(It->GetActorLabel(false)))
		{
			UE_LOG(LogRogue10m, Error, TEXT("A recovered cave actor already exists in OpenRunMap."));
			return {};
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Rogue10m", "RecoverCaveActors", "Recover Q/Z cave actors"));
	TArray<AActor*> Pasted;
	GUnrealEd->PasteActors(Pasted, World, FVector::ZeroVector, false, false, &ActorText);
	if (Pasted.Num() != UE_ARRAY_COUNT(Specs))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Native paste made %d cave actors in memory; expected four. Nothing was saved."), Pasted.Num());
		return {};
	}

	TSet<FString> Labels;
	for (AActor* Actor : Pasted)
	{
		const FString Label = Actor ? Actor->GetActorLabel(false) : FString();
		const UPackage* ExternalPackage = Actor ? Actor->GetExternalPackage() : nullptr;
		if (!Actor || !FindSpecByLabel(Label) || Actor->GetClass() != AActor::StaticClass()
			|| Actor->GetLevel() != World->PersistentLevel || Actor->GetIsSpatiallyLoaded() || Labels.Contains(Label)
			|| !ExternalPackage || !ExternalPackage->GetName().StartsWith(ActiveActorPrefix)
			|| (Label.EndsWith(TEXT("_Dressing")) && !HasRecoveredShell(*Actor, Label[5])))
		{
			UE_LOG(LogRogue10m, Error, TEXT("Native paste cave actor validation failed for %s. Nothing was saved."), *Label);
			return {};
		}
		Labels.Add(Label);
	}
	return Pasted;
#else
	return {};
#endif
}
