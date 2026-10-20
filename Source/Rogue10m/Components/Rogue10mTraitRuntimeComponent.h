#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mGameState.h"
#include "Rogue10mTraitRuntimeComponent.generated.h"

class ARogue10mCharacter;
class URogue10mTownProgressionComponent;

/** Pawn-owned transient trait effects; investment remains on PlayerState. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mTraitRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mTraitRuntimeComponent();
	float GetTraitBonus(ERogue10mTownTrait Trait) const;
	void ResetRunState();
	void NotifyDodgeStarted();
	/** Called once for an accepted execution, including attacks which subsequently miss. */
	float ConsumeDodgeCounterForAttack();
	void NotifyAttackHit(uint32 ExecutionId, AActor& Target);
	void NotifyAttackMissed(uint32 ExecutionId);
	float GetAttackDamageMultiplier(const AActor& Target, float ExecutionDodgeBonus) const;
	void NotifyDirectKill();
	bool TryPreventLethalDamage(float IncomingDamage, AActor* DamageCauser);
	float GetPotionHealingMultiplier() const;
	float GetDodgeStaminaCost(float BaseCost) const;
	int32 CalculateMonsterGoldReward(int32 BaseReward);
	bool HasUsedLastStand() const { return bLastStandUsed; }
	int32 GetComboHitCount() const { return ComboHitCount; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	ARogue10mCharacter* GetCharacter() const;
	const URogue10mTownProgressionComponent* GetTown() const;
	float Now() const;
	bool CanApplyEffects() const;
	void ClearComboChain();
	UFUNCTION()
	void HandleRunPhaseChanged(ERogue10mRunPhase Phase);
	TWeakObjectPtr<ARogue10mGameState> BoundGameState;
	TWeakObjectPtr<AActor> ComboTarget;
	uint32 LastComboExecutionId = 0;
	int32 ComboHitCount = 0;
	float ComboChainStartedAt = -1.0f;
	float ComboBuffExpiresAt = -1.0f;
	float DodgeCounterExpiresAt = -1.0f;
	float NextDodgeCounterAt = -1.0f;
	float NextKillRecoveryAt = -1.0f;
	bool bLastStandUsed = false;
	double GoldBonusRemainder = 0.0;
};
