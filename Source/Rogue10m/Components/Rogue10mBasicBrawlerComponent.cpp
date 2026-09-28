// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mBasicBrawlerComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "TimerManager.h"

URogue10mBasicBrawlerComponent::URogue10mBasicBrawlerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URogue10mBasicBrawlerComponent::IsBasicBrawlerActive() const
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	return Character && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed;
}

float URogue10mBasicBrawlerComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

bool URogue10mBasicBrawlerComponent::CanUseInput() const
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	const URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	return IsBasicBrawlerActive() && GetWorld() && Combat && Combat->CanUseCombatInput();
}

float URogue10mBasicBrawlerComponent::ChargeDuration() const
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	const URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	const URogue10mAttackSkillData* Charged = Combat ? Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial) : nullptr;
	return Charged ? FMath::Max(0.05f, Charged->ChargeSeconds) : 0.65f;
}

float URogue10mBasicBrawlerComponent::ChargeAlpha() const
{
	return bCharging && ChargeStartedAt >= 0.0f ? FMath::Clamp((Now() - ChargeStartedAt) / ChargeDuration(), 0.0f, 1.0f) : 0.0f;
}

FRogue10mBasicBrawlerSnapshot URogue10mBasicBrawlerComponent::GetSnapshot() const
{
	FRogue10mBasicBrawlerSnapshot Result;
	Result.bActive = IsBasicBrawlerActive();
	Result.AcceptedSerial = AcceptedSerial;
	Result.ReleasedChargeAlpha = LastReleasedChargeAlpha;
	Result.HitFraction = LastHitFraction;
	Result.bComboBuffered = bBufferedPrimary && bNextRight;
	Result.NextAttack = Result.bComboBuffered ? ERogue10mBasicBrawlerAttack::RightJab : ERogue10mBasicBrawlerAttack::None;
	if (!Result.bActive || !CanUseInput()) { return Result; }
	if (bCharging)
	{
		Result.Phase = ERogue10mBasicBrawlerPhase::Charging;
		Result.Attack = ERogue10mBasicBrawlerAttack::RightHook;
		Result.ChargeAlpha = ChargeAlpha();
	}
	else if (LastAcceptedAt >= 0.0f && Now() - LastAcceptedAt < LastAttackDuration)
	{
		Result.Phase = ERogue10mBasicBrawlerPhase::Attacking;
		Result.Attack = LastAttack;
		Result.AttackElapsed = FMath::Max(0.0f, Now() - LastAcceptedAt);
		Result.AttackDuration = LastAttackDuration;
	}
	return Result;
}

void URogue10mBasicBrawlerComponent::HandleAttackPressed(bool bPrimary)
{
	if (!CanUseInput()) { CancelInput(); return; }
	ARogue10mCharacter* Character = CastChecked<ARogue10mCharacter>(GetOwner());
	URogue10mCombatComponent* Combat = Character->GetCombatComponent();
	if (bPrimary)
	{
		if (bPrimaryHeld || bCharging) { return; }
		bPrimaryHeld = true;
		TryPrimary(true);
		return;
	}
	if (bSecondaryHeld) { return; }
	bSecondaryHeld = true;
	bBufferedPrimary = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(BufferTimer); }
	if (Character->GetCharacterMovement()->IsFalling()
		|| Combat->IsAttackOnCooldown(Now()) || Combat->IsComboSequenceActive(Now()))
	{
		return;
	}
	const URogue10mAttackSkillData* Tap = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
	const URogue10mAttackSkillData* Charged = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial);
	if (!Tap || !Charged || !Combat->IsAttackSkillUnlocked(Tap) || !Combat->IsAttackSkillUnlocked(Charged)
		|| !Combat->CanPayResourceCosts(*Tap))
	{
		return;
	}
	bCharging = true;
	bNextRight = false;
	ChargeStartedAt = Now();
	Combat->StartChargeEffect(*Charged);
	StartMonitor();
}

void URogue10mBasicBrawlerComponent::HandleAttackReleased(bool bPrimary)
{
	if (bPrimary) { bPrimaryHeld = false; return; }
	if (!bSecondaryHeld) { return; }
	bSecondaryHeld = false;
	if (!bCharging) { return; }
	if (!CanUseInput()) { CancelInput(); return; }
	ARogue10mCharacter* Character = CastChecked<ARogue10mCharacter>(GetOwner());
	if (Character->GetCharacterMovement()->IsFalling()) { CancelInput(); return; }
	URogue10mCombatComponent* Combat = Character->GetCombatComponent();
	RequestedChargeAlpha = ChargeAlpha();
	bCharging = false;
	ChargeStartedAt = -1.0f;
	Combat->StopChargeEffect();
	const bool bFullyCharged = RequestedChargeAlpha >= 1.0f - KINDA_SMALL_NUMBER;
	TrySkill(Combat->GetEquippedSkill(bFullyCharged ? ERogue10mAttackInputSlot::ChargedSpecial
		: ERogue10mAttackInputSlot::Special), bFullyCharged ? ERogue10mBasicBrawlerAttack::RightHook
		: ERogue10mBasicBrawlerAttack::RightStraight, false);
}

bool URogue10mBasicBrawlerComponent::TryPrimary(bool bAllowBuffer)
{
	if (!CanUseInput() || bCharging) { return false; }
	ARogue10mCharacter* Character = CastChecked<ARogue10mCharacter>(GetOwner());
	if (Character->GetCharacterMovement()->IsFalling()) { return false; }
	URogue10mCombatComponent* Combat = Character->GetCombatComponent();
	const URogue10mAttackSkillData* Left = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
	if (!Left) { return false; }
	const float Speed = Combat->GetAttackSpeedMultiplier();
	const float Open = LeftAcceptedAt + Left->ComboWindowOpenSeconds / Speed;
	const float Close = LeftAcceptedAt + Left->ComboWindowCloseSeconds / Speed;
	if (bNextRight && Now() > Close) { bNextRight = false; }
	if (bNextRight)
	{
		if (Now() < Open)
		{
			if (bAllowBuffer) { QueuePrimary(Open, FMath::Min(Close, Open + InputBufferSeconds)); }
			return false;
		}
		return TrySkill(Left->NextComboSkill, ERogue10mBasicBrawlerAttack::RightJab, true);
	}
	if (Combat->IsComboSequenceActive(Now())) { return false; }
	const float Cooldown = Combat->GetAttackCooldownRemaining();
	if (Cooldown > 0.0f)
	{
		if (bAllowBuffer && Cooldown <= InputBufferSeconds)
		{
			QueuePrimary(Now() + Cooldown + 0.001f, Now() + InputBufferSeconds + 0.01f);
		}
		return false;
	}
	RequestedChargeAlpha = 0.0f;
	return TrySkill(Left, ERogue10mBasicBrawlerAttack::LeftJab, false);
}

bool URogue10mBasicBrawlerComponent::TrySkill(const URogue10mAttackSkillData* Skill,
	ERogue10mBasicBrawlerAttack Attack, bool bCombo)
{
	if (!CanUseInput() || !Skill) { return false; }
	ARogue10mCharacter* Character = CastChecked<ARogue10mCharacter>(GetOwner());
	URogue10mCombatComponent* Combat = Character->GetCombatComponent();
	if (!Combat->IsAttackSkillUnlocked(Skill)) { return false; }
	RequestedSkill = Skill;
	RequestedAttack = Attack;
	const int32 Before = AcceptedSerial;
	if (!Combat->TryActivateAttackAbility(*Skill, bCombo))
	{
		Combat->ExecuteAttackSkill(*Skill, bCombo);
	}
	RequestedSkill.Reset();
	RequestedAttack = ERogue10mBasicBrawlerAttack::None;
	return AcceptedSerial != Before;
}

void URogue10mBasicBrawlerComponent::NotifySkillAccepted(const URogue10mAttackSkillData& Skill)
{
	if (!IsBasicBrawlerActive()) { return; }
	// The synchronous default ability reports here only after resource/cooldown validation and execution.
	ERogue10mBasicBrawlerAttack Attack = RequestedSkill.Get() == &Skill ? RequestedAttack : ERogue10mBasicBrawlerAttack::None;
	if (Attack == ERogue10mBasicBrawlerAttack::None) { return; }
	const ARogue10mCharacter* Character = CastChecked<ARogue10mCharacter>(GetOwner());
	const URogue10mCombatComponent* Combat = Character->GetCombatComponent();
	AcceptedSerial = AcceptedSerial == MAX_int32 ? 1 : AcceptedSerial + 1;
	bBufferedPrimary = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(BufferTimer); }
	LastAttack = Attack;
	LastAcceptedAt = Now();
	LastReleasedChargeAlpha = (Attack == ERogue10mBasicBrawlerAttack::RightStraight
		|| Attack == ERogue10mBasicBrawlerAttack::RightHook || Attack == ERogue10mBasicBrawlerAttack::RightUppercut)
		? RequestedChargeAlpha : 0.0f;
	const float Rate = FMath::Max(0.01f, Combat->GetAttackSpeedMultiplier() * Skill.AnimationPlayRate);
	LastAttackDuration = Skill.AttackMontage ? Skill.AttackMontage->GetPlayLength() / Rate
		: FMath::Max(0.05f, Skill.AttackCooldown / Combat->GetAttackSpeedMultiplier());
	LastHitFraction = LastAttackDuration > UE_SMALL_NUMBER
		? FMath::Clamp(Skill.HitStartDelaySeconds / Rate / LastAttackDuration, 0.05f, 0.90f) : 0.34f;
	OwnedMontage = Skill.AttackMontage;
	OwnedAttackSkill = &Skill;
	bNextRight = Attack == ERogue10mBasicBrawlerAttack::LeftJab;
	if (bNextRight) { LeftAcceptedAt = Now(); }
	StartMonitor();
}

void URogue10mBasicBrawlerComponent::QueuePrimary(float ExecuteAt, float ExpiresAt)
{
	if (bBufferedPrimary || ExecuteAt > ExpiresAt || !GetWorld()) { return; }
	bBufferedPrimary = true;
	BufferedExpiry = ExpiresAt;
	GetWorld()->GetTimerManager().SetTimer(BufferTimer, this,
		&URogue10mBasicBrawlerComponent::ConsumeBufferedPrimary, FMath::Max(0.001f, ExecuteAt - Now()), false);
	StartMonitor();
}

void URogue10mBasicBrawlerComponent::ConsumeBufferedPrimary()
{
	const bool bCanConsume = bBufferedPrimary && Now() <= BufferedExpiry;
	bBufferedPrimary = false;
	BufferedExpiry = -1.0f;
	if (bCanConsume && CanUseInput()) { TryPrimary(false); }
}

void URogue10mBasicBrawlerComponent::StartMonitor()
{
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(MonitorTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(MonitorTimer, this,
			&URogue10mBasicBrawlerComponent::MonitorActiveInput, 0.02f, true);
	}
}

void URogue10mBasicBrawlerComponent::MonitorActiveInput()
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	if (!CanUseInput() || (bCharging && Character && Character->GetCharacterMovement()->IsFalling()))
	{
		CancelInput();
		return;
	}
	if (!bCharging && !bBufferedPrimary && Now() >= LastAcceptedAt + LastAttackDuration)
	{
		GetWorld()->GetTimerManager().ClearTimer(MonitorTimer);
	}
}

void URogue10mBasicBrawlerComponent::CancelInput()
{
	if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner()))
	{
		if (URogue10mCombatComponent* Combat = Character->GetCombatComponent())
		{
			if (OwnedAttackSkill.IsValid()) { Combat->CancelPendingAttackHits(OwnedAttackSkill.Get()); }
			Combat->StopChargeEffect();
			if (bNextRight)
			{
				// A cancelled first jab cannot keep advertising the second hit, nor bypass a
				// cooldown whose start was deferred until the original combo window closed.
				// Preserve its existing end time and make the remaining cooldown active now.
				if (Combat->AttackCooldownEndTime > Now())
				{
					Combat->AttackCooldownStartTime = FMath::Min(Combat->AttackCooldownStartTime, Now());
				}
				Combat->ResetComboWindow();
			}
		}
		if (UAnimMontage* Montage = OwnedMontage.Get())
		{
			USkeletalMeshComponent* Source = Character->GetAnimationPlaybackMesh();
			USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
			if (UAnimInstance* Anim = Source ? Source->GetAnimInstance() : nullptr) { Anim->Montage_Stop(0.08f, Montage); }
			if (Arms != Source)
			{
				if (UAnimInstance* Anim = Arms ? Arms->GetAnimInstance() : nullptr) { Anim->Montage_Stop(0.08f, Montage); }
			}
		}
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BufferTimer);
		GetWorld()->GetTimerManager().ClearTimer(MonitorTimer);
	}
	bPrimaryHeld = bSecondaryHeld = bCharging = bBufferedPrimary = bNextRight = false;
	ChargeStartedAt = LeftAcceptedAt = LastAcceptedAt = BufferedExpiry = -1.0f;
	LastAttackDuration = LastReleasedChargeAlpha = RequestedChargeAlpha = 0.0f;
	LastAttack = RequestedAttack = ERogue10mBasicBrawlerAttack::None;
	RequestedSkill.Reset();
	OwnedMontage.Reset();
	OwnedAttackSkill.Reset();
}

void URogue10mBasicBrawlerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelInput();
	Super::EndPlay(EndPlayReason);
}
