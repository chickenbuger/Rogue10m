#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mVillageQuestDirector.generated.h"

class URogue10mVillageQuestDataAsset;
class ARogue10mGameState;
class URogue10mMonsterDataAsset;
enum class ERogue10mRunPhase : uint8;

/** Explicit candidate-map opt-in. Spawns run-local NPCs/targets without editing binary assets. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mVillageQuestDirector : public AActor
{
	GENERATED_BODY()
public:
	ARogue10mVillageQuestDirector();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") TObjectPtr<URogue10mVillageQuestDataAsset> QuestDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Recovery", meta=(ClampMin="1", ClampMax="16")) int32 MaxRecoverySpawnAttempts = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Recovery", meta=(ClampMin="0.1", Units="s")) float RecoveryRetryDelaySeconds = .25f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class ARogue10mVillageQuestMonster;
	friend struct FRogue10mVillageQuestCombatCreditTestAccess;
	void DestroyRunActors();
	void SpawnRunActors();
	bool SpawnCombatObjective(int32 QuestIndex, int32 ObjectiveIndex, bool bRecovery);
	void RequestCombatRecovery(FName QuestId, int32 ObjectiveIndex, uint64 Generation);
	void TryCombatRecovery(FName QuestId, int32 ObjectiveIndex, uint64 Generation, int32 Attempt);
	bool IsCurrentEncounterRun() const;
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> RunActors;
	UPROPERTY(Transient) TArray<TObjectPtr<URogue10mMonsterDataAsset>> ResolvedCombatData;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	bool bOwnsQuestRuntime = false;
	uint64 EncounterGeneration = 1;
	FGuid EncounterRunId;
	TSet<FName> PendingRecoveryKeys;
	int32 InitialIdentityAttempts = 0;
};
