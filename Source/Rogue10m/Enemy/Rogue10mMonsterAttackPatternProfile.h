// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mMonsterAttackPatternProfile.generated.h"

class UAnimSequence;
class UMaterialInterface;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class ERogue10mMonsterPatternState : uint8
{
	Idle,
	Windup,
	Recovery
};

UENUM(BlueprintType)
enum class ERogue10mMonsterAttackType : uint8
{
	Basic,
	FrontalWave,
	AbyssZone
};

/** Opt-in basic melee and owned animations; legacy monsters remain unchanged. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mMonsterAttackPatternProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Finite CPU burst systems authored for this species; cosmetic only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects")
	TSoftObjectPtr<UNiagaraSystem> AttackEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects")
	TSoftObjectPtr<UNiagaraSystem> FrontalWaveEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects")
	TSoftObjectPtr<UNiagaraSystem> AbyssZoneEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects", meta=(ClampMin="0.01"))
	float AttackEffectScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Effects", meta=(ClampMin="0.01"))
	float ImpactEffectScale = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="1.0", Units="cm"))
	float AttackRange = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="1.0", ClampMax="180.0"))
	float AttackArcDegrees = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="1.0", Units="cm"))
	float AttackHalfHeight = 120.0f;

	/** Time from animation start to the single basic hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="0.05", Units="s"))
	float AttackWindupSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="0.05", Units="s"))
	float AttackRecoverySeconds = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="0.0", Units="s"))
	float AttackCooldownSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|BasicAttack", meta=(ClampMin="0.0"))
	float AttackDamageMultiplier = 1.0f;

	/** Existing monster profiles retain their basic melee behavior unless explicitly enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss")
	bool bEnableBossPatterns = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="1.0", Units="cm"))
	float FrontalWaveRange = 850.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="1.0", Units="cm"))
	float FrontalWaveHalfWidth = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="1.0", Units="cm"))
	float FrontalWaveHalfHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="0.05", Units="s"))
	float FrontalWaveWindupSeconds = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="0.05", Units="s"))
	float FrontalWaveRecoverySeconds = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="0.0", Units="s"))
	float FrontalWaveCooldownSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|FrontalWave", meta=(ClampMin="0.0"))
	float FrontalWaveDamageMultiplier = 1.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="1.0", Units="cm"))
	float AbyssZoneCastRange = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="1.0", Units="cm"))
	float AbyssZoneRadius = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="1.0", Units="cm"))
	float AbyssZoneHalfHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="0.05", Units="s"))
	float AbyssZoneWindupSeconds = 1.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="0.05", Units="s"))
	float AbyssZoneRecoverySeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="0.0", Units="s"))
	float AbyssZoneCooldownSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|AbyssZone", meta=(ClampMin="0.0"))
	float AbyssZoneDamageMultiplier = 1.6f;

	/** Enrage is latched for the remaining encounter; existing health/damage attributes are never modified. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Enrage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EnrageHealthThreshold = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Enrage", meta=(ClampMin="0.05"))
	float EnrageWindupMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Enrage", meta=(ClampMin="0.05"))
	float EnrageRecoveryMultiplier = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Enrage", meta=(ClampMin="0.05"))
	float EnrageCooldownMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Enrage", meta=(ClampMin="0.0"))
	float EnrageDamageMultiplier = 1.25f;

	/** Deferred Decal materials: rectangle for the wave, an inscribed circular mask for the zone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Telegraph")
	TSoftObjectPtr<UMaterialInterface> FrontalWaveTelegraphMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Telegraph")
	TSoftObjectPtr<UMaterialInterface> AbyssZoneTelegraphMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Telegraph", meta=(ClampMin="1.0", Units="cm"))
	float TelegraphDepth = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Telegraph", meta=(ClampMin="0.0", Units="cm"))
	float TelegraphGroundOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Animation")
	TSoftObjectPtr<UAnimSequence> FrontalWaveAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Boss|Animation")
	TSoftObjectPtr<UAnimSequence> AbyssZoneAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Animation")
	TSoftObjectPtr<UAnimSequence> MovementAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Animation")
	TSoftObjectPtr<UAnimSequence> AttackAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Animation")
	TSoftObjectPtr<UAnimSequence> HitAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Animation")
	TSoftObjectPtr<UAnimSequence> DeathAnimation;
};
