#include "Rogue10mTraitRuntimeComponent.h"

#include "Engine/World.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitCatalogDataAsset.h"

URogue10mTraitRuntimeComponent::URogue10mTraitRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URogue10mTraitRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetRunState();
	if (UWorld* World = GetWorld())
	{
		BoundGameState = World->GetGameState<ARogue10mGameState>();
		if (ARogue10mGameState* State = BoundGameState.Get())
		{
			State->OnRunPhaseChanged.AddDynamic(this, &URogue10mTraitRuntimeComponent::HandleRunPhaseChanged);
		}
	}
}

void URogue10mTraitRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARogue10mGameState* State = BoundGameState.Get())
	{
		State->OnRunPhaseChanged.RemoveDynamic(this, &URogue10mTraitRuntimeComponent::HandleRunPhaseChanged);
	}
	BoundGameState.Reset();
	Super::EndPlay(EndPlayReason);
}

ARogue10mCharacter* URogue10mTraitRuntimeComponent::GetCharacter() const
{
	return Cast<ARogue10mCharacter>(GetOwner());
}

const URogue10mTownProgressionComponent* URogue10mTraitRuntimeComponent::GetTown() const
{
	const ARogue10mCharacter* Character = GetCharacter();
	const ARogue10mPlayerState* State = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	return State ? State->FindComponentByClass<URogue10mTownProgressionComponent>() : nullptr;
}

float URogue10mTraitRuntimeComponent::GetTraitBonus(ERogue10mTownTrait Trait) const
{
	const URogue10mTownProgressionComponent* Town = GetTown();
	return Town ? Town->GetTraitBonus(Trait) : 0.0f;
}

float URogue10mTraitRuntimeComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

bool URogue10mTraitRuntimeComponent::CanApplyEffects() const
{
	const ARogue10mCharacter* Character = GetCharacter();
	return Character && Character->HasAuthority() && !Character->IsDead();
}

void URogue10mTraitRuntimeComponent::ClearComboChain()
{
	ComboTarget.Reset();
	ComboHitCount = 0;
	ComboChainStartedAt = -1.0f;
}

void URogue10mTraitRuntimeComponent::ResetRunState()
{
	ClearComboChain();
	LastComboExecutionId = 0;
	ComboBuffExpiresAt = DodgeCounterExpiresAt = NextDodgeCounterAt = NextKillRecoveryAt = -1.0f;
	bLastStandUsed = false;
	GoldBonusRemainder = 0.0;
}

void URogue10mTraitRuntimeComponent::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (Phase == ERogue10mRunPhase::Running) { ResetRunState(); }
}

void URogue10mTraitRuntimeComponent::NotifyDodgeStarted()
{
	if (!CanApplyEffects() || GetTraitBonus(ERogue10mTownTrait::DodgeCounter) <= 0.0f || Now() < NextDodgeCounterAt)
	{
		return;
	}
	const FRogue10mTraitDefinition Definition = GetTown()->GetTraitDefinition(ERogue10mTownTrait::DodgeCounter);
	DodgeCounterExpiresAt = Now() + Definition.DurationSeconds;
	NextDodgeCounterAt = Now() + Definition.CooldownSeconds;
}

float URogue10mTraitRuntimeComponent::ConsumeDodgeCounterForAttack()
{
	if (!CanApplyEffects()) { return 0.0f; }
	const float Bonus = Now() <= DodgeCounterExpiresAt ? GetTraitBonus(ERogue10mTownTrait::DodgeCounter) : 0.0f;
	DodgeCounterExpiresAt = -1.0f;
	return Bonus;
}

void URogue10mTraitRuntimeComponent::NotifyAttackHit(uint32 ExecutionId, AActor& Target)
{
	if (!CanApplyEffects() || ExecutionId == 0 || ExecutionId == LastComboExecutionId
		|| GetTraitBonus(ERogue10mTownTrait::Combo) <= 0.0f) { return; }
	LastComboExecutionId = ExecutionId;
	const float Time = Now();
	if (Time < ComboBuffExpiresAt) { return; }
	const FRogue10mTraitDefinition Definition = GetTown()->GetTraitDefinition(ERogue10mTownTrait::Combo);
	if (ComboTarget.Get() != &Target || ComboChainStartedAt < 0.0f
		|| Time - ComboChainStartedAt > Definition.DurationSeconds)
	{
		ClearComboChain();
		ComboTarget = &Target;
		ComboChainStartedAt = Time;
	}
	if (++ComboHitCount >= FMath::Max(1, Definition.RequiredHits))
	{
		ComboBuffExpiresAt = Time + Definition.DurationSeconds;
		ClearComboChain();
	}
}

void URogue10mTraitRuntimeComponent::NotifyAttackMissed(uint32 ExecutionId)
{
	// Resolve combo results in confirmed-result order, not acceptance-ID order.
	// Delayed executions can miss after a newer execution has already hit.
	if (CanApplyEffects() && ExecutionId != 0)
	{
		LastComboExecutionId = ExecutionId;
		ClearComboChain();
	}
}

float URogue10mTraitRuntimeComponent::GetAttackDamageMultiplier(const AActor& Target, float ExecutionDodgeBonus) const
{
	if (!CanApplyEffects()) { return 1.0f; }
	float Bonus = FMath::Max(0.0f, ExecutionDodgeBonus);
	if (Now() < ComboBuffExpiresAt) { Bonus += GetTraitBonus(ERogue10mTownTrait::Combo); }
	const ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(&Target);
	const URogue10mMonsterDataAsset* Data = Monster ? Monster->GetMonsterData() : nullptr;
	if (Data && (Data->MonsterRank == ERogue10mMonsterRank::MidBoss || Data->MonsterRank == ERogue10mMonsterRank::FinalBoss))
	{
		Bonus += GetTraitBonus(ERogue10mTownTrait::BossHunter);
	}
	return 1.0f + FMath::Max(0.0f, Bonus);
}

void URogue10mTraitRuntimeComponent::NotifyDirectKill()
{
	if (!CanApplyEffects() || Now() < NextKillRecoveryAt) { return; }
	const float Recovery = GetTraitBonus(ERogue10mTownTrait::KillRecovery);
	ARogue10mCharacter* Character = GetCharacter();
	URogue10mAttributeSet* Attributes = Character ? Character->GetRogueAttributeSet() : nullptr;
	if (!Attributes || Recovery <= 0.0f || Attributes->GetHealth() <= 0.0f) { return; }
	NextKillRecoveryAt = Now() + GetTown()->GetTraitDefinition(ERogue10mTownTrait::KillRecovery).CooldownSeconds;
	Attributes->SetHealth(FMath::Min(Attributes->GetMaxHealth(), Attributes->GetHealth() + Recovery));
}

bool URogue10mTraitRuntimeComponent::TryPreventLethalDamage(float IncomingDamage, AActor* DamageCauser)
{
	// Forced death and run timeout use Character::Die directly and never pass this gate.
	const ARogue10mGameState* GameState = BoundGameState.Get();
	if (!CanApplyEffects() || bLastStandUsed || !DamageCauser || DamageCauser == GetOwner()
		|| (GameState && !GameState->IsRunActive()) || !FMath::IsFinite(IncomingDamage) || IncomingDamage <= 0.0f)
	{
		return false;
	}
	URogue10mAttributeSet* Attributes = GetCharacter()->GetRogueAttributeSet();
	const float RecoveryRatio = GetTraitBonus(ERogue10mTownTrait::LastStand);
	if (!Attributes || Attributes->GetHealth() <= 0.0f || IncomingDamage < Attributes->GetHealth() || RecoveryRatio <= 0.0f)
	{
		return false;
	}
	bLastStandUsed = true;
	Attributes->SetHealth(FMath::Max(1.0f, Attributes->GetMaxHealth() * FMath::Clamp(RecoveryRatio, 0.0f, 1.0f)));
	return true;
}

float URogue10mTraitRuntimeComponent::GetPotionHealingMultiplier() const
{
	return 1.0f + FMath::Max(0.0f, GetTraitBonus(ERogue10mTownTrait::PotionMastery));
}

float URogue10mTraitRuntimeComponent::GetDodgeStaminaCost(float BaseCost) const
{
	return FMath::Max(0.0f, BaseCost) * (1.0f - FMath::Clamp(GetTraitBonus(ERogue10mTownTrait::DodgeEfficiency), 0.0f, 1.0f));
}

int32 URogue10mTraitRuntimeComponent::CalculateMonsterGoldReward(int32 BaseReward)
{
	BaseReward = FMath::Max(0, BaseReward);
	if (!CanApplyEffects()) { return BaseReward; }
	const double Bonus = FMath::Clamp(static_cast<double>(GetTraitBonus(ERogue10mTownTrait::GoldFind)), 0.0, 0.15);
	const double Accumulated = GoldBonusRemainder + static_cast<double>(BaseReward) * Bonus;
	const int32 WholeBonus = FMath::FloorToInt(Accumulated + 0.000001);
	GoldBonusRemainder = FMath::Max(0.0, Accumulated - WholeBonus);
	return static_cast<int32>(FMath::Min(static_cast<int64>(MAX_int32), static_cast<int64>(BaseReward) + WholeBonus));
}
