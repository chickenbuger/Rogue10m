// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Rogue10mMonsterAttackPatternProfile.h"
#include "Rogue10mMonsterAttackPatternComponent.generated.h"

class ARogue10mBasicMonster;
class ARogue10mCharacter;
class UAnimInstance;
class UAbilitySystemComponent;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;
struct FOnAttributeChangeData;

/** Timer-driven basic melee and opt-in telegraphed boss attacks. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mMonsterAttackPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mMonsterAttackPatternComponent();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Monster|BasicAttack")
	void ConfigureProfile(URogue10mMonsterAttackPatternProfile* InProfile);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	URogue10mMonsterAttackPatternProfile* GetProfile() const { return Profile; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	bool IsEnabled() const { return Profile != nullptr; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	bool IsAttackActive() const { return PatternState != ERogue10mMonsterPatternState::Idle; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	ERogue10mMonsterPatternState GetPatternState() const { return PatternState; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	int32 GetStartedAttackCount() const { return StartedAttackCount; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	int32 GetSuccessfulHitCount() const { return SuccessfulHitCount; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|BasicAttack")
	FVector GetLockedAttackLocation() const { return LockedLocation; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Boss")
	FVector GetLockedAttackForward() const { return LockedForward; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Boss")
	ERogue10mMonsterAttackType GetCurrentAttackType() const { return CurrentAttackType; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Boss")
	bool IsEnraged() const { return bEnraged; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Boss")
	bool IsTelegraphVisible() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Effects")
	int32 GetAttackEffectCount() const { return AttackEffectCount; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Effects")
	int32 GetImpactEffectCount() const { return ImpactEffectCount; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Monster|Effects")
	int32 GetActiveEffectCount() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Monster|BasicAttack")
	bool TryStartAttack(ARogue10mCharacter* Target);
	/** Uses the normal warning/damage pipeline, limited to marked standalone test instances. */
	bool TryStartCombatTestAttack(ARogue10mCharacter* Target, ERogue10mMonsterAttackType Type);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Monster|BasicAttack")
	void CancelAttack();

	float GetEngagementRange() const;
	void PlayHitReaction();
	float PlayDeathPresentation();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ARogue10mBasicMonster* GetMonster() const;
	ERogue10mMonsterAttackType SelectAvailableAttack() const;
	float GetRangeForAttack(ERogue10mMonsterAttackType AttackType) const;
	void SnapshotAttackSettings();
	bool IsInsideAttackShape(const ARogue10mCharacter& Target) const;
	void RefreshEnrage();
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void UnbindHealthDelegate();
	void CheckAttackValidity();
	bool CreateTelegraph();
	void DestroyTelegraph();
	void ExecuteAttack();
	void FinishRecovery();
	void DamageTarget(ARogue10mCharacter* Target);
	bool HasClearHitPath(const ARogue10mCharacter& Target) const;
	void PlayAttackPresentation();
	void RestoreAnimation();
	void SaveAnimationMode();
	void UpdateLocomotionPresentation();
	bool SpawnEffect(UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, float Scale);
	void ClearEffects();
	UFUNCTION()
	void HandleEffectFinished(UNiagaraComponent* FinishedEffect);

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> AttackEffect;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ImpactEffect;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> FrontalWaveEffect;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> AbyssZoneEffect;
	TArray<TWeakObjectPtr<UNiagaraComponent>> ActiveEffects;
	int32 AttackEffectCount = 0;
	int32 ImpactEffectCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mMonsterAttackPatternProfile> Profile;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> SavedAnimationClass;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> TelegraphDecal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FrontalWaveMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AbyssZoneMaterial;

	TWeakObjectPtr<ARogue10mCharacter> LockedTarget;
	TWeakObjectPtr<UAbilitySystemComponent> HealthDelegateASC;
	FDelegateHandle HealthDelegateHandle;
	TSet<TWeakObjectPtr<ARogue10mCharacter>> HitTargets;
	FTimerHandle StateTimer;
	FTimerHandle HitReactionTimer;
	FTimerHandle LocomotionTimer;
	FTimerHandle AttackGuardTimer;
	ERogue10mMonsterPatternState PatternState = ERogue10mMonsterPatternState::Idle;
	ERogue10mMonsterAttackType CurrentAttackType = ERogue10mMonsterAttackType::Basic;
	FVector LockedLocation = FVector::ZeroVector;
	FVector LockedSourceLocation = FVector::ZeroVector;
	FVector LockedForward = FVector::ForwardVector;
	double NextAttackTime = 0.0;
	double NextFrontalWaveTime = 0.0;
	double NextAbyssZoneTime = 0.0;
	double WindupStartedTime = 0.0;
	float LockedRange = 0.0f;
	float LockedHalfWidth = 0.0f;
	float LockedHalfHeight = 0.0f;
	float LockedArcDegrees = 0.0f;
	float LockedWindupSeconds = 0.0f;
	float LockedRecoverySeconds = 0.0f;
	float LockedCooldownSeconds = 0.0f;
	float LockedDamageMultiplier = 0.0f;
	int32 NextSequenceIndex = 0;
	int32 StartedAttackCount = 0;
	int32 SuccessfulHitCount = 0;
	bool bEnraged = false;
	bool bSavedAnimationMode = false;
	uint8 SavedAnimationMode = 0;
};
