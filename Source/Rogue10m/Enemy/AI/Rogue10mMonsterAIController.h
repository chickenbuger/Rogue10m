// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Rogue10mMonsterAIController.generated.h"

class ARogue10mBasicMonster;
class ARogue10mCharacter;
class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBlackboardData;
class UAIPerceptionComponent;
class UAISenseConfig_Damage;
class UAISenseConfig_Sight;

UCLASS()
class ROGUE10M_API ARogue10mMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARogue10mMonsterAIController();

	void InitializeMonsterAI();

	bool ExecuteBehaviorDecision();
	void NotifyDamageStimulus(AActor* SourceActor, float DamageAmount);
	void StopMonsterAI();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster AI")
	bool IsMonsterAIEnabled() const { return bAIEnabled; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster AI")
	ARogue10mCharacter* GetCombatTarget() const { return GetBlackboardTarget(); }

	static const FName TargetActorKey;
	static const FName HomeLocationKey;
	static const FName PatrolLocationKey;
	static const FName LastKnownLocationKey;
	static const FName HasTargetKey;
	static const FName IsInAttackRangeKey;
	static const FName WasDamagedKey;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster AI")
	TObjectPtr<UAIPerceptionComponent> MonsterPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster AI")
	TObjectPtr<UBlackboardComponent> MonsterBlackboardComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster AI")
	TObjectPtr<UBehaviorTreeComponent> MonsterBehaviorTreeComponent;

private:
	ARogue10mBasicMonster* GetMonsterPawn() const;
	ARogue10mCharacter* GetBlackboardTarget() const;
	bool IsValidCombatTarget(const ARogue10mCharacter* Target) const;
	void AcquireTarget(ARogue10mCharacter* Target, bool bFromDamage);
	void ClearTarget();
	void UpdateCombatBehavior(ARogue10mBasicMonster& Monster, ARogue10mCharacter& Target);
	void UpdatePatrolBehavior(ARogue10mBasicMonster& Monster);
	void BuildRuntimeBlackboard();
	void BuildRuntimeBehaviorTree();

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardData> RuntimeBlackboard;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> RuntimeBehaviorTree;

	double LastTargetSenseTime = -1.0;
	double NextPatrolMoveTime = 0.0;
	bool bWaitingForPatrolArrival = false;
	bool bAIEnabled = false;
};
