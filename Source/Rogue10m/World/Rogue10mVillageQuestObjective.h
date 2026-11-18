#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestObjective.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class URogue10mVillageQuestDataAsset;
class ARogue10mGameState;
class ARogue10mCharacter;
class ARogue10mVillageQuestDirector;
class URogue10mVillageQuestComponent;
enum class ERogue10mRunPhase : uint8;

/** Transient GameState ledger key; survives director reconstruction, never enters profile saves. */
ROGUE10M_API FName Rogue10mVillageQuestEncounterReceipt(FName QuestId, int32 ObjectiveIndex);
ROGUE10M_API FName Rogue10mVillageQuestEncounterDeathReceipt(FName QuestId, int32 ObjectiveIndex);

/** Run fact collector: crossing this visible marker records one objective once per player/run. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mVillageQuestObjective : public AActor
{
	GENERATED_BODY()
public:
	ARogue10mVillageQuestObjective();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests", meta=(ClampMin="0", ClampMax="1")) int32 ObjectiveIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") FText ObjectiveLabel;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") TObjectPtr<URogue10mVillageQuestDataAsset> QuestDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Presentation") FRogue10mVillageObjectivePresentation Presentation;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void ApplyPresentation();
	void CollectOverlappingPlayers();
	void Collect(AActor* Actor);
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* Other,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Area;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Marker;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> DefaultMarkerMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	TWeakObjectPtr<ARogue10mGameState> RunState;
};

/** An ordinary existing monster with a quest fact attributed to its actual damage instigator. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mVillageQuestMonster : public ARogue10mBasicMonster
{
	GENERATED_BODY()
public:
	ARogue10mVillageQuestMonster();
	/** Set before FinishSpawning; ordinary monster content is selected by the quest Data Asset. */
	bool ConfigureQuestMonsterData(URogue10mMonsterDataAsset* Data);
	virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") FName QuestId = TEXT("Q_V3_quarry");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") int32 ObjectiveIndex = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") TObjectPtr<URogue10mVillageQuestDataAsset> QuestDefinition;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnDeathStarted() override;
	virtual bool ShouldGrantDeathRewards() const override;
private:
	friend class ARogue10mVillageQuestDirector;
	friend struct FRogue10mVillageQuestCombatCreditTestAccess;
	UFUNCTION() void InvalidateQuestEncounter(ERogue10mRunPhase Phase);
	struct FQuestDamageContext
	{
		TWeakObjectPtr<ARogue10mCharacter> Character;
		TWeakObjectPtr<URogue10mVillageQuestComponent> Component;
		TWeakObjectPtr<ARogue10mGameState> Run;
		TWeakObjectPtr<ARogue10mVillageQuestDirector> Director;
		TWeakObjectPtr<URogue10mVillageQuestDataAsset> Definition;
		FGuid RunId;
		FName QuestId;
		int32 ObjectiveIndex = INDEX_NONE;
		uint64 CreditEpoch = 0;
		uint64 DirectorGeneration = 0;
		bool bObservedDeath = false;
	};
	FQuestDamageContext* CurrentQuestDamage = nullptr;
	TWeakObjectPtr<ARogue10mGameState> EncounterRun;
	FGuid EncounterRunId;
	TWeakObjectPtr<URogue10mVillageQuestDataAsset> EncounterDefinition;
	FName EncounterQuestId;
	int32 EncounterObjectiveIndex = INDEX_NONE;
	bool bEncounterEnded = true;
	bool bRecoveryEncounter = false;
	bool bDeathCustodyRecorded = false;
};
