#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mWeaponIdentityComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UPostProcessComponent;
class URogue10mAttackSkillData;
enum class ERogue10mRunPhase : uint8;

/** Prototype knuckle identity burst. Derived overlays never write persistent GAS base attributes. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mWeaponIdentityComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mWeaponIdentityComponent();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Weapon Identity") bool TryActivate();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Weapon Identity") void DeactivateIdentity();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Weapon Identity") bool IsIdentityActive() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Weapon Identity") float GetRemainingSeconds() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Weapon Identity") float GetCooldownRemaining() const;
	float GetMovementMultiplier() const;
	float GetAttackSpeedMultiplier() const;
	float GetDamageMultiplier(const URogue10mAttackSkillData& Skill) const;
	float GetRangeMultiplier(const URogue10mAttackSkillData& Skill) const;

	/** All values below are provisional design assumptions, not approved balance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="0.1")) float DurationSeconds = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="0.1")) float CooldownSeconds = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="1.0")) float AttackSpeedMultiplier = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="1.0")) float MovementMultiplier = 1.20f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="1.0")) float DamageMultiplier = 1.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Prototype", meta=(ClampMin="1.0")) float SkillRangeMultiplier = 1.10f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Weapon Identity|Visual") TSoftObjectPtr<UNiagaraSystem> AuraSystem;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend struct FRogue10mSkillLoadoutTestAccess;
	friend struct FRogue10mWeaponIdentityTestAccess;
	void Monitor();
	void ClearVisuals();
	UFUNCTION() void HandleRunPhase(ERogue10mRunPhase Phase);
	float EndsAt = -1.f;
	float NextActivationAt = -1.f;
	FTimerHandle MonitorTimer;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Aura;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> PostProcess;
};
