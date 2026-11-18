// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Rogue10mGameState.generated.h"

UENUM(BlueprintType)
enum class ERogue10mRunPhase : uint8
{
	WaitingToStart,
	Running,
	Victory,
	Defeat,
	Abandoned
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRogue10mRunTimerUpdated, float, RemainingSeconds, float, TotalSeconds, float, ProgressAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRogue10mRunPhaseChanged, ERogue10mRunPhase, NewPhase);

/**
 * Runtime state for a single 20-minute roguelike run.
 */
UCLASS()
class ROGUE10M_API ARogue10mGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARogue10mGameState();

	/** Starts a new run and resets the countdown. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run")
	void StartRun();

	/** Starts a new run with a temporary duration override for prototypes and tests. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run")
	void StartRunWithDuration(float NewRunDurationSeconds);

	/** Finishes the current run. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run")
	void FinishRun(bool bVictory);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run") bool AbandonRun();

	/** Remaining run time formatted as MM:SS. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	FText GetRemainingTimeText() const;

	/** Normalized timer value where 1.0 is full time and 0.0 is expired. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	float GetRunProgressAlpha() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	float GetRunDurationSeconds() const { return RunDurationSeconds; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	float GetRemainingRunSeconds() const { return RemainingRunSeconds; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	bool IsRunActive() const { return RunPhase == ERogue10mRunPhase::Running; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsRunTerminal() const
	{ return RunPhase == ERogue10mRunPhase::Victory || RunPhase == ERogue10mRunPhase::Defeat || RunPhase == ERogue10mRunPhase::Abandoned; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	ERogue10mRunPhase GetRunPhase() const { return RunPhase; }

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Run")
	FRogue10mRunTimerUpdated OnRunTimerUpdated;

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Run")
	FRogue10mRunPhaseChanged OnRunPhaseChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Run", meta=(ClampMin="1.0", UIMin="60.0"))
	float RunDurationSeconds = 1200.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	float RemainingRunSeconds = 1200.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	ERogue10mRunPhase RunPhase = ERogue10mRunPhase::WaitingToStart;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	int32 RunSeed = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	int32 CurrentFloor = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	int32 DefeatedEnemyCount = 0;

private:
	void TickRunTimer();
	void SetRunPhase(ERogue10mRunPhase NewPhase);
	void BroadcastTimer();

	FTimerHandle RunTimerHandle;
};
