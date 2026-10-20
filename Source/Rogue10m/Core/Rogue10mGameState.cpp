// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mGameState.h"

#include "Engine/World.h"
#include "Internationalization/Text.h"
#include "TimerManager.h"

ARogue10mGameState::ARogue10mGameState()
{
	RemainingRunSeconds = RunDurationSeconds;
}

void ARogue10mGameState::BeginPlay()
{
	Super::BeginPlay();

	RemainingRunSeconds = RunDurationSeconds;
	BroadcastTimer();
}

void ARogue10mGameState::StartRun()
{
	if (!HasAuthority() || RunPhase != ERogue10mRunPhase::WaitingToStart) { return; }
	RunSeed = FMath::Rand();
	CurrentFloor = 1;
	DefeatedEnemyCount = 0;
	RemainingRunSeconds = RunDurationSeconds;

	SetRunPhase(ERogue10mRunPhase::Running);
	BroadcastTimer();

	GetWorldTimerManager().SetTimer(RunTimerHandle, this, &ARogue10mGameState::TickRunTimer, 1.0f, true);
}

void ARogue10mGameState::StartRunWithDuration(float NewRunDurationSeconds)
{
	if (!HasAuthority() || RunPhase != ERogue10mRunPhase::WaitingToStart) { return; }
	RunDurationSeconds = FMath::Max(1.0f, NewRunDurationSeconds);
	StartRun();
}

void ARogue10mGameState::FinishRun(bool bVictory)
{
	if (!HasAuthority() || !IsRunActive()) { return; }
	GetWorldTimerManager().ClearTimer(RunTimerHandle);
	SetRunPhase(bVictory ? ERogue10mRunPhase::Victory : ERogue10mRunPhase::Defeat);
	BroadcastTimer();
}

FText ARogue10mGameState::GetRemainingTimeText() const
{
	const int32 RoundedSeconds = FMath::Max(0, FMath::CeilToInt(RemainingRunSeconds));
	const int32 Minutes = RoundedSeconds / 60;
	const int32 Seconds = RoundedSeconds % 60;

	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
}

float ARogue10mGameState::GetRunProgressAlpha() const
{
	if (RunDurationSeconds <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(RemainingRunSeconds / RunDurationSeconds, 0.0f, 1.0f);
}

void ARogue10mGameState::TickRunTimer()
{
	if (!IsRunActive())
	{
		return;
	}

	RemainingRunSeconds = FMath::Max(0.0f, RemainingRunSeconds - 1.0f);
	BroadcastTimer();

	if (RemainingRunSeconds <= 0.0f)
	{
		FinishRun(false);
	}
}

void ARogue10mGameState::SetRunPhase(ERogue10mRunPhase NewPhase)
{
	if (RunPhase == NewPhase)
	{
		return;
	}

	RunPhase = NewPhase;
	OnRunPhaseChanged.Broadcast(RunPhase);
}

void ARogue10mGameState::BroadcastTimer()
{
	OnRunTimerUpdated.Broadcast(RemainingRunSeconds, RunDurationSeconds, GetRunProgressAlpha());
}
