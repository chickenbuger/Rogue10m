// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "Rogue10mMapPreloadSubsystem.generated.h"

struct FWorldContext;
class UPackage;
class UWorld;

/** Preloads the other gameplay map without starting its world or changing travel. */
UCLASS()
class URogue10mMapPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnPreLoadMap(const FWorldContext& Context, const FString& MapName);
	void OnPostLoadMap(UWorld* World);
	void BeginPreload(UWorld& SourceWorld);
	void OnPackageLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, uint32 Generation);
	void ResetPreload();

	// Keep the uninitialized destination world through LoadMap's memory trim.
	UPROPERTY(Transient)
	TObjectPtr<UWorld> PreparedWorld;

	FDelegateHandle PreLoadHandle;
	FDelegateHandle PostLoadHandle;
	FTSTicker::FDelegateHandle KickoffHandle;
	FName PendingPackage;
	uint32 RequestGeneration = 0;
	double TravelStartedAt = 0.0;
	bool bPreloadEnabled = true;
};
