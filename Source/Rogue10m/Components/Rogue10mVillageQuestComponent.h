#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mVillageQuestModel.h"
#include "Rogue10mVillageOptionalClueModel.h"
#include "Rogue10mVillageQuestCombatCreditModel.h"
#include "Engine/TimerHandle.h"
#include "Rogue10mVillageQuestComponent.generated.h"

class ARogue10mCharacter;
class ARogue10mGameState;
class URogue10mVillageQuestDataAsset;
class UWorld;

/** Metadata only: never loads, saves or initializes a profile. */
ROGUE10M_API FGuid Rogue10mVillageQuestRunId(const UWorld* World);
ROGUE10M_API bool Rogue10mVillageQuestSameRun(const UWorld* World, const ARogue10mGameState* Run, const FGuid& RunId);
enum class ERogue10mRunPhase : uint8;

UENUM(BlueprintType)
enum class ERogue10mVillageQuestState : uint8 { Unavailable, Available, Active, Ready, Claimed };

UENUM(BlueprintType)
enum class ERogue10mVillageOptionalClueState : uint8 { Missing, Found, Analyzed, Reported };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mVillageQuestChanged);

/** Run-local PlayerState ownership survives NPC streaming and same-world revisits. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mVillageQuestComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mVillageQuestComponent();
	static URogue10mVillageQuestComponent* FindOrCreate(ARogue10mCharacter* Character,
		URogue10mVillageQuestDataAsset* Definition = nullptr);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quests")
	ERogue10mVillageQuestState GetQuestState(FName QuestId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quests")
	int32 GetObjectiveProgress(FName QuestId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quests")
	bool HasObservedObjective(FName QuestId, int32 ObjectiveIndex) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quests|Optional Story")
	ERogue10mVillageOptionalClueState GetOptionalClueState() const;
	/** Optional narrative only: does not grant currency/items or change primary quest completion. */
	bool AnalyzeOptionalClue(FName AnalystQuestId, ARogue10mCharacter* Character);
	bool ReportOptionalClue(FName ReportQuestId, ARogue10mCharacter* Character);
	bool AcceptQuest(FName QuestId, ARogue10mCharacter* Character);
	bool ObserveObjective(FName QuestId, int32 ObjectiveIndex, ARogue10mCharacter* Character);
	bool ClaimReward(FName QuestId, ARogue10mCharacter* Character, FText& OutReason);
	const URogue10mVillageQuestDataAsset* GetDefinition() const { return ActiveDefinition; }
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Village Quests") FRogue10mVillageQuestChanged OnQuestChanged;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class ARogue10mVillageQuestMonster;
	friend struct FRogue10mVillageQuestCombatCreditTestAccess;
	bool CanUse(const ARogue10mCharacter* Character, bool bAllowRewardGrant = false) const;
	bool EnqueueCombatDeath(FName QuestId, int32 ObjectiveIndex, ARogue10mCharacter* Character, uint64 Epoch, const FGuid& RunId);
	void FlushCombatDeaths();
	void ScheduleCombatDeathFlush();
	void ResetCombatDeaths();
	struct FCombatDeath
	{
		TWeakObjectPtr<ARogue10mCharacter> Character;
		TWeakObjectPtr<ARogue10mGameState> Run;
		TWeakObjectPtr<URogue10mVillageQuestDataAsset> Definition;
		FGuid RunId;
		FName QuestId;
		int32 ObjectiveIndex = INDEX_NONE;
	};
	bool IsCurrentCombatDeath(const FCombatDeath& Death, uint64 Epoch, int32 Slot) const;
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	UPROPERTY(Transient) TObjectPtr<URogue10mVillageQuestDataAsset> ActiveDefinition;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	Rogue10mVillageQuest::FQuestModel Progress;
	Rogue10mVillageQuest::FOptionalClueModel OptionalClue;
	bool bDefinitionValid = false;
	bool bRewardGrantInProgress = false;
	bool bResetAfterGrant = false;
	Rogue10mVillageQuest::FCombatCreditDeliveryModel CombatCredits;
	FCombatDeath PendingCombatDeaths[Rogue10mVillageQuest::QuestCount];
	bool bFlushingCombatDeaths = false;
	bool bCombatCreditOwnerEnded = false;
	bool bCombatFlushScheduled = false;
	FTimerHandle CombatFlushTimer;
};
