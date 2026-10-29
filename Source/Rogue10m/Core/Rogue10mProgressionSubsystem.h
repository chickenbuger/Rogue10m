// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mProgressionSubsystem.generated.h"

class ARogue10mCharacter;
class ARogue10mPlayerState;
class ARogue10mGameState;
enum class ERogue10mRunPhase : uint8;
class URogue10mProgressionSaveGame;

/** Character-owned progression, inventory and NPC receipts survive travel and restart. */
UCLASS()
class ROGUE10M_API URogue10mProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	bool AttachCharacter(ARogue10mCharacter& Character);
	void DetachCharacter(ARogue10mCharacter& Character, bool bSave = true);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Save") bool SaveNow();
	bool IsAttachedTo(const ARogue10mCharacter& Character) const;
	FGuid GetAttachedProfileId() const { return AttachedProfileId; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Save") bool IsSaveReady() const;
	/** A successfully restored, still-attached session may retry a failed disk write. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Save") bool CanRetrySave() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Save") FText GetLastError() const { return LastError; }
	bool HasClaimedReward(FName RewardId) const;
	void RecordClaimedReward(FName RewardId);
	ERogue10mDifficulty GetSelectedDifficulty() const;
	FRogue10mDifficultyModifiers GetSelectedDifficultyModifiers() const;
	FString GetSaveSlotForProfile(const FGuid& ProfileId) const;
	FString GetLegacySaveSlotForProfile(const FGuid& ProfileId) const;
	/** Terminal phase only. Applies the run receipt once, including after a failed disk write. */
	bool SaveRunSettlement(bool bVictory);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Lobby") int32 GetLobbyAvailablePoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Lobby") bool HasDiscoveredNode(FName NodeId) const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Lobby", meta=(ClampMin="0", ClampMax="1000")) int32 LobbyPointsPerDefeat = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Lobby", meta=(ClampMin="0", ClampMax="10000")) int32 LobbyVictoryBonus = 10;
private:
	URogue10mProgressionSaveGame* LoadSelectedSnapshot() const;
	URogue10mProgressionSaveGame* LoadSnapshot(const FGuid& ProfileId, bool& bOutExists) const;
	void BindChanges(ARogue10mCharacter& Character);
	void UnbindChanges();
	void CaptureCurrentState();
	void SetError(const FText& Error) const;
	UFUNCTION() void HandleStateChanged();
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	TWeakObjectPtr<ARogue10mGameState> ObservedRun;
	FGuid SettlementProfileId;
	FGuid SettlementRunId;
	UPROPERTY(Transient) TObjectPtr<URogue10mProgressionSaveGame> SaveData;
	/** Captured before the first save restore on each pawn; reattach cannot recycle run loot. */
	UPROPERTY(Transient) TObjectPtr<URogue10mProgressionSaveGame> PawnStarterState;
	TWeakObjectPtr<ARogue10mCharacter> StarterCharacter;
	UPROPERTY(Transient) mutable TObjectPtr<URogue10mProgressionSaveGame> SelectedSnapshotCache;
	UPROPERTY(Transient) mutable FText LastError;
	TWeakObjectPtr<ARogue10mCharacter> CurrentCharacter;
	TWeakObjectPtr<ARogue10mPlayerState> CurrentPlayerState;
	FGuid AttachedProfileId;
	mutable FGuid CachedProfileId;
	FTimerHandle AutoSaveTimer;
	bool bSaveReady = false;
	bool bRestoreValid = false;
	bool bRestoring = false;
	bool bDirty = false;
};
