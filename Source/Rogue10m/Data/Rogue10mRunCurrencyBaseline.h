// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Rogue10mRunCurrencyBaseline.generated.h"

/**
 * Currency checkpoint captured after town entry costs are committed, immediately
 * before entering the open world. It is not a reward, refund, or discovery policy.
 * A new run uses a new checkpoint; an existing checkpoint cannot be recaptured at
 * a more favorable in-run balance.
 */
USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mRunCurrencyBaseline
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Run|Currency") FGuid RunId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Run|Currency") int32 Gold = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Run|Currency") int32 Crystals = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Run|Currency") int32 LobbyEarnedPoints = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Run|Currency") bool bCaptured = false;

	bool IsValid() const
	{
		return bCaptured && RunId.IsValid() && Gold >= 0 && Crystals >= 0 && LobbyEarnedPoints >= 0;
	}

	/** Failed or conflicting capture leaves the previous checkpoint unchanged. */
	bool TryCapture(const FGuid& InRunId, int32 InGold, int32 InCrystals, int32 InLobbyEarnedPoints)
	{
		if (!InRunId.IsValid() || InGold < 0 || InCrystals < 0 || InLobbyEarnedPoints < 0) { return false; }
		if (bCaptured)
		{
			return IsValid() && RunId == InRunId && Gold == InGold
				&& Crystals == InCrystals && LobbyEarnedPoints == InLobbyEarnedPoints;
		}
		RunId = InRunId;
		Gold = InGold;
		Crystals = InCrystals;
		LobbyEarnedPoints = InLobbyEarnedPoints;
		bCaptured = true;
		return true;
	}

	/** Pure, repeatable resolution. A failed lookup never partially changes outputs. */
	bool TryResolve(const FGuid& CurrentRunId, int32& OutGold, int32& OutCrystals, int32& OutLobbyEarnedPoints) const
	{
		if (!IsValid() || CurrentRunId != RunId) { return false; }
		OutGold = Gold;
		OutCrystals = Crystals;
		OutLobbyEarnedPoints = LobbyEarnedPoints;
		return true;
	}
};
