// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Rogue10mOpenWorldBootstrap.generated.h"

class APawn;
class UNavigationSystemConfig;
class URogue10mItemDataAsset;

/** Opt-in navigation system selected only by the OpenRunMap WorldSettings. */
UCLASS()
class ROGUE10M_API URogue10mOpenWorldNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

public:
	virtual void Configure(const UNavigationSystemConfig& Config) override;
};

/** Keeps local dynamic navigation around the possessed player without changing its class. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mOpenWorldBootstrap : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mOpenWorldBootstrap();

	/** Requires an issued registration and a matching engine active-invoker sample.
	 * The engine's public samples contain position/radii, not actor identity.
	 * May return false until the next navigation update after registration or teleport.
	 */
	UFUNCTION(BlueprintPure, Category="Rogue10m|OpenWorld|Debug")
	bool IsPlayerNavigationInvokerRegistered() const;

	/** True only in a streaming-enabled partitioned world after its streaming work completes. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|OpenWorld|Debug")
	bool IsWorldStreamingReady() const;
	/** World-lifetime receipt; remains true after pickup and is shared by duplicate bootstraps. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|OpenWorld|Entry Loot")
	bool HasPlacedEntryGreatSword() const;

	/** Editor-only operation; returns JSON and leaves saving/reloading to the caller. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|OpenWorld|Editor", meta=(DevelopmentOnly))
	static FString ConfigureOpenWorldNavigation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Navigation", meta=(ClampMin="100.0", Units="cm"))
	float NavigationGenerationRadius = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Navigation", meta=(ClampMin="100.0", Units="cm"))
	float NavigationRemovalRadius = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Navigation", meta=(ClampMin="0.1", Units="s"))
	float InitialRegistrationIntervalSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Navigation", meta=(ClampMin="0.1", Units="s"))
	float PawnRefreshIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Navigation", meta=(ClampMin="1"))
	int32 MaximumInitialRegistrationAttempts = 120;

	/** Runtime placement only; existing map packages and source item assets are never edited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Entry Loot")
	bool bEnableEntryGreatSword = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Entry Loot")
	TSoftObjectPtr<URogue10mItemDataAsset> EntryGreatSwordItem = TSoftObjectPtr<URogue10mItemDataAsset>(
		FSoftObjectPath(TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword")));

	/** XY placement and vertical trace origin. The pickup is placed above traced ground, never midair. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Entry Loot", meta=(Units="cm"))
	FVector EntryGreatSwordLocation = FVector(600.0, -62600.0, 3000.0);

	/** One transient south approach; never edits the existing boss fallback platform or map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach")
	bool bEnableBossSouthApproach = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(Units="cm"))
	FVector2D BossSouthApproachStart = FVector2D(12000.0, 60000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(Units="cm"))
	FVector2D BossSouthApproachEnd = FVector2D(15000.0, 62550.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(ClampMin="100.0", Units="cm"))
	float BossSouthApproachWidth = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(ClampMin="10.0", Units="cm"))
	float BossSouthApproachThickness = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(ClampMin="1.0", ClampMax="30.0", Units="deg"))
	float BossSouthApproachMaximumSlope = 20.0f;

	/** One-second callback budget, independent of navigation registration and distant-cell loading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|OpenWorld|Boss Approach", meta=(ClampMin="1", ClampMax="1200"))
	int32 BossSouthApproachMaximumAttempts = 1200;

private:
	void RefreshPlayerNavigationInvoker();
	void ReleasePlayerNavigationInvoker();
	void TryPlaceEntryGreatSword();
	void TryPlaceBossSouthApproach();

	TWeakObjectPtr<APawn> RegisteredPawn;
	TWeakObjectPtr<UNavigationSystemV1> RegisteredNavigationSystem;
	FTimerHandle InvokerTimerHandle;
	FTimerHandle BossSouthApproachTimerHandle;
	float RegisteredGenerationRadius = 0.0f;
	float RegisteredRemovalRadius = 0.0f;
	int32 InitialRegistrationAttempts = 0;
	bool bRegistrationIssued = false;
	bool bHasRegisteredOnce = false;
	int32 EntryGreatSwordAttempts = 0;
	bool bEntryGreatSwordFinished = false;
	int32 BossSouthApproachAttempts = 0;
	bool bBossSouthApproachFinished = false;
};
