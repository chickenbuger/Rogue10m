// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Rogue10mOpenWorldBootstrap.generated.h"

class APawn;
class UNavigationSystemConfig;

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

private:
	void RefreshPlayerNavigationInvoker();
	void ReleasePlayerNavigationInvoker();

	TWeakObjectPtr<APawn> RegisteredPawn;
	TWeakObjectPtr<UNavigationSystemV1> RegisteredNavigationSystem;
	FTimerHandle InvokerTimerHandle;
	float RegisteredGenerationRadius = 0.0f;
	float RegisteredRemovalRadius = 0.0f;
	int32 InitialRegistrationAttempts = 0;
	bool bRegistrationIssued = false;
	bool bHasRegisteredOnce = false;
};
