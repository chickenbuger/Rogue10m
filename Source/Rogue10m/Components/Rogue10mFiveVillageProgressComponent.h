#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mFiveVillageProgressModel.h"
#include "TimerManager.h"
#include "Rogue10mFiveVillageProgressComponent.generated.h"

class APlayerState;
class APawn;
class ARogue10mGameState;
class URogue10mFiveVillageRouteDataAsset;
enum class ERogue10mRunPhase : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mFiveVillageRouteProgressChanged);

/** Run-local PlayerState observations. Never grants rewards or gates travel. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mFiveVillageProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mFiveVillageProgressComponent();

	/** Configure before RegisterComponent. Null uses an owned default route object. */
	bool Configure(URogue10mFiveVillageRouteDataAsset* Definition);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages")
	TArray<FName> GetVisitedVillageIds() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages")
	int32 GetVillageVisitCount(FName VillageId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages")
	FName GetCurrentVillageId() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages")
	int32 GetTotalRevisitCount() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages|Boss Approach")
	bool HasEnteredBossApproach() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Villages|Boss Approach")
	float GetFirstBossApproachActiveSeconds() const;

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Five Villages")
	FRogue10mFiveVillageRouteProgressChanged OnRouteProgressChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Villages", meta=(ClampMin="0.05", Units="s"))
	float ObservationIntervalSeconds = 0.25f;

private:
	bool InitializeDefinition(URogue10mFiveVillageRouteDataAsset* Definition);
	void BindRunState();
	void ObserveProgress();
	UFUNCTION() void HandleRunPhaseChanged(ERogue10mRunPhase Phase);

	UPROPERTY(Transient)
	TObjectPtr<URogue10mFiveVillageRouteDataAsset> ActiveRoute;
	TWeakObjectPtr<APlayerState> OwnerState;
	TWeakObjectPtr<APawn> ObservedPawn;
	TWeakObjectPtr<ARogue10mGameState> RunState;
	FTimerHandle ObservationTimer;
	Rogue10mFiveVillage::FProgressModel Progress;
	bool bDefinitionValid = false;
	bool bObservedRunning = false;
};
