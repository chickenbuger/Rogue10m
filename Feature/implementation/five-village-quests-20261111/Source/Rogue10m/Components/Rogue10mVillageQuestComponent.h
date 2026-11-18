#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestModel.h"
#include "UObject/SoftObjectPath.h"
#include "Rogue10mVillageQuestComponent.generated.h"

class ARogue10mCharacter;
class ARogue10mGameState;
enum class ERogue10mRunPhase : uint8;

/** Optional, run-local collection quests. This component never gates travel or writes saves. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mVillageQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mVillageQuestComponent();
	/** Only creates on the currently possessed local PlayerState in the active candidate run. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	static URogue10mVillageQuestComponent* GetOrCreate(ARogue10mCharacter* Character);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	bool RegisterQuest(URogue10mVillageQuestDataAsset* Definition);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	bool AcceptQuest(FName QuestId);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	bool RecordObjective(FName QuestId, FName ObjectiveId);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	bool TryClaimReward(FName QuestId);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quest")
	ERogue10mVillageQuestStatus GetQuestStatus(FName QuestId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quest")
	int32 GetCollectedCount(FName QuestId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quest")
	bool HasObjective(FName QuestId, FName ObjectiveId) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FQuestEntry
	{
		Rogue10mVillageQuest::FQuestModel Model;
		TArray<FName> Objectives;
		FSoftObjectPath RewardPath;
		int32 Quantity = 0;
		Rogue10mVillageQuest::FQuestModel::FClaimToken DeferredToken = 0;
		bool bDeferredSuccess = false;
	};
	bool ResolveContext(ARogue10mCharacter*& OutCharacter, FGuid& OutRunId, FGuid& OutProfileId) const;
	bool SynchronizeContext(ARogue10mCharacter*& OutCharacter);
	bool DefinitionMatches(FName QuestId, const FQuestEntry& Entry) const;
	const FQuestEntry* FindReadableQuest(FName QuestId) const;
	void BindRunState();
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<URogue10mVillageQuestDataAsset>> Definitions;
	TMap<FName, FQuestEntry> Quests;
	TWeakObjectPtr<ARogue10mGameState> ObservedRun;
	FGuid ActiveRunId;
	FGuid ActiveProfileId;
	uint64 ContextGeneration = 0;
	bool bRewardInFlight = false;
	bool bRunTerminated = false;
	bool bStopped = false;
};
