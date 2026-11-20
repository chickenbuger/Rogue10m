// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mMapPreloadSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Rogue10m.h"
#include "UObject/Package.h"

namespace
{
	const FName StartHubPackage(TEXT("/Game/Rogue10m/Maps/StartHub"));
	const FName OpenRunMapPackage(TEXT("/Game/Rogue10m/Maps/OpenRunMap"));

	FName GetNextMap(FName CurrentMap)
	{
		if (CurrentMap == StartHubPackage) { return OpenRunMapPackage; }
		if (CurrentMap == OpenRunMapPackage) { return StartHubPackage; }
		return NAME_None;
	}

	bool IsMapActive(FName PackageName)
	{
		if (!GEngine) { return false; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (const UWorld* World = Context.World(); World && World->GetPackage()->GetFName() == PackageName)
			{
				return true;
			}
		}
		return false;
	}
}

void URogue10mMapPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningCommandlet() || IsRunningDedicatedServer()) { return; }

	bPreloadEnabled = !FParse::Param(FCommandLine::Get(), TEXT("RogueNoMapPreload"));
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::OnPreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnPostLoadMap);
}

void URogue10mMapPreloadSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	ResetPreload();
	Super::Deinitialize();
}

void URogue10mMapPreloadSubsystem::OnPreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance != GetGameInstance() || Context.WorldType != EWorldType::Game) { return; }
	const UWorld* CurrentWorld = Context.World();
	const FName TargetPackage(*MapName);
	if (TargetPackage != PendingPackage
		&& (!CurrentWorld || CurrentWorld->GetPackage()->GetFName() != TargetPackage))
	{
		ResetPreload();
	}

	TravelStartedAt = FPlatformTime::Seconds();
	UE_LOG(LogRogue10m, Log, TEXT("MAP_PRELOAD travel target=%s prewarmed=%d"),
		*MapName, IsValid(PreparedWorld) && PreparedWorld->GetPackage()->GetFName() == TargetPackage);
}

void URogue10mMapPreloadSubsystem::OnPostLoadMap(UWorld* World)
{
	if (!World)
	{
		if (TravelStartedAt > 0.0)
		{
			TravelStartedAt = 0.0;
			ResetPreload();
		}
		return;
	}
	if (World->GetGameInstance() != GetGameInstance() || World->WorldType != EWorldType::Game) { return; }

	const FName CurrentMap = World->GetPackage()->GetFName();
	if (TravelStartedAt > 0.0)
	{
		UE_LOG(LogRogue10m, Log, TEXT("MAP_PRELOAD loaded map=%s reused=%d seconds=%.3f"),
			*CurrentMap.ToString(), World == PreparedWorld.Get(), FPlatformTime::Seconds() - TravelStartedAt);
		TravelStartedAt = 0.0;
	}

	const FName NextPackage = GetNextMap(CurrentMap);
	if (!bPreloadEnabled || World->GetNetMode() != NM_Standalone || NextPackage.IsNone())
	{
		ResetPreload();
		return;
	}
	// The frontend and playable hub share one package. Preserve an already queued field map across that reload.
	if (PendingPackage == NextPackage) { return; }

	ResetPreload();
	PendingPackage = NextPackage;
	KickoffHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this](float)
		{
			KickoffHandle.Reset();
			UWorld* SourceWorld = GetGameInstance()->GetWorld();
			if (SourceWorld && PendingPackage == GetNextMap(SourceWorld->GetPackage()->GetFName()))
			{
				BeginPreload(*SourceWorld);
			}
			else
			{
				ResetPreload();
			}
			return false;
		}));
}

void URogue10mMapPreloadSubsystem::BeginPreload(UWorld& SourceWorld)
{
	const FString PackageName = PendingPackage.ToString();
	if (IsMapActive(PendingPackage) || !FPackageName::DoesPackageExist(PackageName))
	{
		ResetPreload();
		return;
	}

	UE_LOG(LogRogue10m, Log, TEXT("MAP_PRELOAD queued source=%s target=%s"),
		*SourceWorld.GetPackage()->GetName(), *PackageName);
	LoadPackageAsync(PackageName,
		FLoadPackageAsyncDelegate::CreateUObject(this, &ThisClass::OnPackageLoaded, RequestGeneration),
		/*InPackagePriority=*/-1);
}

void URogue10mMapPreloadSubsystem::OnPackageLoaded(const FName& PackageName, UPackage* Package,
	EAsyncLoadingResult::Type Result, uint32 Generation)
{
	if (Generation != RequestGeneration || PackageName != PendingPackage) { return; }

	UWorld* LoadedWorld = Result == EAsyncLoadingResult::Succeeded && Package
		? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!IsValid(LoadedWorld) || LoadedWorld->WorldType != EWorldType::Inactive || LoadedWorld->IsInitialized()
#if WITH_EDITOR
		|| LoadedWorld->HasEverBeenInitialized()
#endif
		|| !LoadedWorld->PersistentLevel || IsMapActive(PackageName))
	{
		UE_LOG(LogRogue10m, Log, TEXT("MAP_PRELOAD unavailable target=%s"), *PackageName.ToString());
		ResetPreload();
		return;
	}

	PreparedWorld = LoadedWorld;
	UE_LOG(LogRogue10m, Log, TEXT("MAP_PRELOAD ready target=%s"), *PackageName.ToString());
}

void URogue10mMapPreloadSubsystem::ResetPreload()
{
	FTSTicker::RemoveTicker(KickoffHandle);
	KickoffHandle.Reset();
	++RequestGeneration;
	PendingPackage = NAME_None;
	PreparedWorld = nullptr;
}
