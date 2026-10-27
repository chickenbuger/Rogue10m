#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mSkillTreeCombatComponent.generated.h"

class URogue10mAttackSkillData;
class URogue10mSkillTreeProgressionComponent;

/** Captured at acceptance; later weapon/identity changes never amplify an in-flight attack. */
struct FRogue10mSkillTreeAttackEffect
{
	float DamageMultiplier = 1.0f;
	float RangeMultiplier = 1.0f;
	float RecoveryMultiplier = 1.0f;
	bool bTriggered = false;
	/** Planned optional cost; included in the attack's aggregate payment before commit. */
	float ManaCost = 0.0f;
	bool bStartsCirculation = false;
};

/** Transient combat resource. Durable unlocks remain on PlayerState. No ticking or free refills. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mSkillTreeCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mSkillTreeCombatComponent();
	void SynchronizeIdentity();
	void NotifyConfirmedAttack(ERogue10mSkillTreeTrack ExecutionTrack);
	float GetStaminaCostMultiplier(bool bComboAttack) const;
	FRogue10mSkillTreeAttackEffect AcceptAttack(const URogue10mAttackSkillData& Skill, bool bComboAttack);
	FRogue10mSkillTreeAttackEffect PreviewAttack(const URogue10mAttackSkillData& Skill, float AvailableMana) const;
	void CommitAttack(const FRogue10mSkillTreeAttackEffect& Effect, bool bComboAttack);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree|Combat") float GetGauge() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree|Combat") float GetGaugeMaximum() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree|Combat") float GetCooldownRemaining() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree|Combat") ERogue10mSkillTreeIdentity GetIdentity() const;
private:
	URogue10mSkillTreeProgressionComponent* GetProgression() const;
	ERogue10mSkillTreeIdentity CachedIdentity = ERogue10mSkillTreeIdentity::None;
	ERogue10mSkillTreeTrack CachedTrack = ERogue10mSkillTreeTrack::None;
	FName CachedProperty;
	float Gauge = 0.0f;
	float NextProcTime = 0.0f;
	float CirculationExpiresAt = 0.0f;
};
