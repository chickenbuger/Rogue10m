// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Rogue10mAttackTargetInterface.h"
#include "Rogue10mBasicMonster.generated.h"

class ARogue10mCharacter;
class UBehaviorTree;
class UAbilitySystemComponent;
class URogue10mAbilitySystemComponent;
class URogue10mAttributeSet;
class URogue10mAttackSkillData;
class URogue10mMonsterDataAsset;
class URogue10mMonsterAttackPatternComponent;
class URogue10mMonsterAttackPatternProfile;
class URogue10mVitalRegenerationComponent;
class ARogue10mBasicMonster;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRogue10mMonsterDied, ARogue10mBasicMonster*, Monster);

UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mBasicMonster : public ACharacter, public IAbilitySystemInterface, public IRogue10mAttackTargetInterface
{
	GENERATED_BODY()

public:
	ARogue10mBasicMonster();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool CanReceiveRogue10mAttack_Implementation(AActor* AttackSource) const override;
	virtual FVector GetRogue10mDamageIndicatorLocation_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	URogue10mAttributeSet* GetRogueAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	FText GetMonsterDisplayName() const { return MonsterDisplayName; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	const URogue10mMonsterDataAsset* GetMonsterData() const { return MonsterData; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	int32 GetMonsterLevel() const { return MonsterLevel; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	FText GetMonsterAttributeText() const { return MonsterAttributeText; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster")
	bool IsDead() const { return bIsDead; }
	/** Set before FinishSpawning; development fixtures never grant progression rewards. */
	void MarkCombatTestInstance();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test")
	bool IsCombatTestInstance() const { return bCombatTestInstance; }

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Monster")
	FRogue10mMonsterDied OnMonsterDied;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Pattern")
	URogue10mMonsterAttackPatternComponent* GetAttackPatternComponent() const { return AttackPatternComponent; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Monster|Pattern")
	void ConfigureAttackPatternProfile(URogue10mMonsterAttackPatternProfile* Profile);

	bool IsPatternAttackActive() const;
	float GetPatternBaseDamage() const { return FMath::Max(0.0f, AttackDamage); }

	float GetDetectionRange() const { return DetectionRange; }
	float GetLoseSightRange() const { return LoseSightRange; }
	float GetForgetTargetSeconds() const { return ForgetTargetSeconds; }
	float GetPatrolRadius() const { return PatrolRadius; }
	float GetPatrolWaitSeconds() const { return PatrolWaitSeconds; }
	float GetMaxChaseDistance() const { return MaxChaseDistance; }
	float GetStopDistance() const { return StopDistance; }
	float GetEffectiveAttackRange() const;
	UBehaviorTree* ResolveBehaviorTreeAsset() const;
	void SetAITarget(ARogue10mCharacter* NewTarget);
	void ClearAITarget();
	void ExecuteAICombat(ARogue10mCharacter* Target);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|GAS")
	TObjectPtr<URogue10mAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|GAS")
	TObjectPtr<URogue10mAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster")
	TObjectPtr<URogue10mVitalRegenerationComponent> VitalRegenerationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Pattern")
	TObjectPtr<URogue10mMonsterAttackPatternComponent> AttackPatternComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Data")
	TObjectPtr<URogue10mMonsterDataAsset> MonsterData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster")
	FText MonsterDisplayName = FText::FromString(TEXT("기본 몬스터"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster", meta=(ClampMin="1"))
	int32 MonsterLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster")
	FText MonsterAttributeText = FText::FromString(TEXT("일반"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Vitals", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Movement", meta=(ClampMin="0.0"))
	float DetectionRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Movement", meta=(ClampMin="0.0"))
	float StopDistance = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float LoseSightRange = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.1", Units="s"))
	float ForgetTargetSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float PatrolRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0", Units="s"))
	float PatrolWaitSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float MaxChaseDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Combat")
	TObjectPtr<URogue10mAttackSkillData> AttackSkillData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.0"))
	float AttackRange = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.0"))
	float AttackDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.01"))
	float AttackInterval = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Monster|Combat")
	bool bDestroyOnDeath = true;

private:
	float DifficultyDamageMultiplier = 1.0f;
	void TryAttackTarget(float DistanceToTarget);
	void StartMonsterAttackSequence();
	void ExecuteMonsterAttackPulse();
	bool IsTargetInsideAttackShape(const ARogue10mCharacter& Target, const URogue10mAttackSkillData& SkillData) const;
	void Die();

	TWeakObjectPtr<ARogue10mCharacter> TargetCharacter;
	TWeakObjectPtr<AController> LastDamageInstigator;
	TWeakObjectPtr<ARogue10mCharacter> LastDirectDamageCharacter;
	int32 ExperienceReward = 10;
	float LastAttackTime = -1000.0f;
	FTimerHandle AttackSequenceTimer;
	TWeakObjectPtr<ARogue10mCharacter> LockedAttackTarget;
	int32 CompletedAttackPulses = 0;
	bool bIsDead = false;
	bool bCombatTestInstance = false;
};
