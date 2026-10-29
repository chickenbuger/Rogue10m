// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mRunCompletionComponent.generated.h"

class ARogue10mBasicMonster;
class ARogue10mCharacter;
class AController;
class ARogue10mGameState;
class ARogue10mPlayerController;
class UWorld;
enum class ERogue10mRunPhase : uint8;

/** A read-only projection; no UI command can mutate currency through this value. */
USTRUCT(BlueprintType)
struct FRogue10mRunResultEconomy
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bSaved = false;
	UPROPERTY(BlueprintReadOnly) bool bAbandoned = false;
	UPROPERTY(BlueprintReadOnly) int32 LobbyAward = 0;
	UPROPERTY(BlueprintReadOnly) int32 LobbyAvailable = 0;
	UPROPERTY(BlueprintReadOnly) int32 RunGold = 0;
	UPROPERTY(BlueprintReadOnly) int32 RunCrystals = 0;
};

/** Owns the local run settlement; retries only persist already-awarded rewards. */
UCLASS(ClassGroup=(Rogue10m))
class ROGUE10M_API URogue10mRunCompletionComponent : public UActorComponent
{

	GENERATED_BODY()
public:
	URogue10mRunCompletionComponent();
	void Initialize(ARogue10mGameState& State);
	void NotifyFinalBossDefeated(ARogue10mBasicMonster& Boss, AController* RewardController, int32 Experience, int32 Gold);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run") bool ReturnToTown();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsSettled() const { return bSettled; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") FRogue10mRunResultEconomy GetEconomySummary() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsVictory() const { return bVictory; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsAbandoned() const { return bAbandoned; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsProgressionSaved() const { return bProgressionSaved; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") bool IsTravelStarted() const { return bTravelStarted; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") int32 GetRewardExperience() const { return RewardExperience; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") int32 GetRewardGold() const { return RewardGold; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run") FText GetSaveStatus() const { return SaveStatus; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	TSoftObjectPtr<UWorld> TownMap;

private:
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	void FinalizeSettlement();
	bool SaveSettlement();
	void RefreshResult();
	TWeakObjectPtr<ARogue10mGameState> RunState;
	TWeakObjectPtr<ARogue10mPlayerController> ResultController;
	TWeakObjectPtr<ARogue10mCharacter> ResultCharacter;
	FGuid ResultProfileId;
	FTimerHandle FinalizeTimerHandle;
	UPROPERTY(Transient) FText SaveStatus;
	int32 RewardExperience = 0;
	int32 RewardGold = 0;
	bool bSettled = false;
	bool bSettlementPending = false;
	bool bVictory = false;
	bool bAbandoned = false;
	bool bProgressionSaved = false;
	bool bTravelStarted = false;
};
