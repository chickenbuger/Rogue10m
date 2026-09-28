// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mBasicBrawlerComponent.generated.h"

class URogue10mAttackSkillData;
class UAnimMontage;

UENUM(BlueprintType)
enum class ERogue10mBasicBrawlerAttack : uint8 { None = 0, LeftJab = 1, RightStraight = 2, RightUppercut = 3, RightJab = 4, RightHook = 5 };

UENUM(BlueprintType)
enum class ERogue10mBasicBrawlerPhase : uint8 { Idle, Charging, Attacking };

USTRUCT(BlueprintType)
struct FRogue10mBasicBrawlerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bActive = false;
	UPROPERTY(BlueprintReadOnly) ERogue10mBasicBrawlerPhase Phase = ERogue10mBasicBrawlerPhase::Idle;
	UPROPERTY(BlueprintReadOnly) ERogue10mBasicBrawlerAttack Attack = ERogue10mBasicBrawlerAttack::None;
	UPROPERTY(BlueprintReadOnly) int32 AcceptedSerial = 0;
	UPROPERTY(BlueprintReadOnly) float AttackElapsed = 0.0f;
	UPROPERTY(BlueprintReadOnly) float AttackDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HitFraction = 0.34f;
	UPROPERTY(BlueprintReadOnly) bool bComboBuffered = false;
	UPROPERTY(BlueprintReadOnly) ERogue10mBasicBrawlerAttack NextAttack = ERogue10mBasicBrawlerAttack::None;
	UPROPERTY(BlueprintReadOnly) float ChargeAlpha = 0.0f;
	UPROPERTY(BlueprintReadOnly) float ReleasedChargeAlpha = 0.0f;
};

/** Unarmed-only input policy. The existing combat/GAS path owns damage, resources and cooldowns. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mBasicBrawlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mBasicBrawlerComponent();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Basic Brawler")
	bool IsBasicBrawlerActive() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Basic Brawler")
	FRogue10mBasicBrawlerSnapshot GetSnapshot() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Basic Brawler")
	bool HasBufferedAttack() const { return bBufferedPrimary; }

	void HandleAttackPressed(bool bPrimary);
	void HandleAttackReleased(bool bPrimary);
	void NotifySkillAccepted(const URogue10mAttackSkillData& Skill);
	void CancelInput();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Basic Brawler", meta=(ClampMin="0.0", ClampMax="0.25", Units="s"))
	float InputBufferSeconds = 0.15f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanUseInput() const;
	bool TryPrimary(bool bAllowBuffer);
	bool TrySkill(const URogue10mAttackSkillData* Skill, ERogue10mBasicBrawlerAttack Attack, bool bCombo);
	void QueuePrimary(float ExecuteAt, float ExpiresAt);
	void ConsumeBufferedPrimary();
	void MonitorActiveInput();
	void StartMonitor();
	float Now() const;
	float ChargeAlpha() const;
	float ChargeDuration() const;

	FTimerHandle BufferTimer;
	FTimerHandle MonitorTimer;
	bool bPrimaryHeld = false;
	bool bSecondaryHeld = false;
	bool bCharging = false;
	bool bBufferedPrimary = false;
	bool bNextRight = false;
	float BufferedExpiry = -1.0f;
	float ChargeStartedAt = -1.0f;
	float LeftAcceptedAt = -1.0f;
	float LastAcceptedAt = -1.0f;
	float LastAttackDuration = 0.0f;
	float LastHitFraction = 0.34f;
	float LastReleasedChargeAlpha = 0.0f;
	float RequestedChargeAlpha = 0.0f;
	int32 AcceptedSerial = 0;
	ERogue10mBasicBrawlerAttack LastAttack = ERogue10mBasicBrawlerAttack::None;
	ERogue10mBasicBrawlerAttack RequestedAttack = ERogue10mBasicBrawlerAttack::None;
	TWeakObjectPtr<const URogue10mAttackSkillData> RequestedSkill;
	TWeakObjectPtr<UAnimMontage> OwnedMontage;
	TWeakObjectPtr<const URogue10mAttackSkillData> OwnedAttackSkill;
};
