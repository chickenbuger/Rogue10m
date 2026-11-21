#include "Rogue10mWeaponIdentityComponent.h"

#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"

URogue10mWeaponIdentityComponent::URogue10mWeaponIdentityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Reuse a project-owned combat derivative. Dedicated aura authoring remains a visual QA step.
	AuraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Charge.NS_Knuckle_Charge")));
}
bool URogue10mWeaponIdentityComponent::IsIdentityActive() const
{
	const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
	return Character && GetWorld() && !Character->IsDead()
		&& Character->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle
		&& EndsAt > GetWorld()->GetTimeSeconds();
}
float URogue10mWeaponIdentityComponent::GetRemainingSeconds() const
{
	return IsIdentityActive() ? FMath::Max(0.f, EndsAt - GetWorld()->GetTimeSeconds()) : 0.f;
}
float URogue10mWeaponIdentityComponent::GetCooldownRemaining() const
{
	return GetWorld() ? FMath::Max(0.f, NextActivationAt - GetWorld()->GetTimeSeconds()) : 0.f;
}
float URogue10mWeaponIdentityComponent::GetMovementMultiplier() const { return IsIdentityActive() ? FMath::Clamp(MovementMultiplier, 1.f, 2.f) : 1.f; }
float URogue10mWeaponIdentityComponent::GetAttackSpeedMultiplier() const { return IsIdentityActive() ? FMath::Clamp(AttackSpeedMultiplier, 1.f, 2.f) : 1.f; }
float URogue10mWeaponIdentityComponent::GetDamageMultiplier(const URogue10mAttackSkillData& Skill) const { return IsIdentityActive() ? FMath::Clamp(DamageMultiplier, 1.f, 2.f) : 1.f; }
float URogue10mWeaponIdentityComponent::GetRangeMultiplier(const URogue10mAttackSkillData& Skill) const
{
	const auto* Character = Cast<ARogue10mCharacter>(GetOwner());
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	return IsIdentityActive() && Combat && Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) != &Skill
		? FMath::Clamp(SkillRangeMultiplier, 1.f, 1.5f) : 1.f;
}
bool URogue10mWeaponIdentityComponent::TryActivate()
{
	auto* Character = Cast<ARogue10mCharacter>(GetOwner());
	const auto* PC = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	const auto* State = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (!Character || !Character->HasAuthority() || !Character->IsLocallyControlled() || Character->IsDead() || !GetWorld()
		|| Character->GetEquippedWeaponType() != ERogue10mWeaponType::Knuckle || IsIdentityActive() || GetCooldownRemaining() > 0.f
		|| UGameplayStatics::IsGamePaused(this) || (PC && PC->IsAnyBlockingWindowVisible())
		|| (State && State->IsRunTerminal()) || !FMath::IsFinite(DurationSeconds) || !FMath::IsFinite(CooldownSeconds)) { return false; }
	EndsAt = GetWorld()->GetTimeSeconds() + FMath::Clamp(DurationSeconds, .1f, 60.f);
	NextActivationAt = GetWorld()->GetTimeSeconds() + FMath::Clamp(CooldownSeconds, .1f, 300.f);
	Character->RefreshCharacterStats(false);
	PostProcess = NewObject<UPostProcessComponent>(Character);
	PostProcess->SetupAttachment(Character->GetRootComponent());
	PostProcess->bUnbound = true; PostProcess->Priority = 10.f; PostProcess->BlendWeight = .6f;
	PostProcess->Settings.bOverride_SceneColorTint = true;
	PostProcess->Settings.SceneColorTint = FLinearColor(1.f, .96f, .88f, 1.f);
	PostProcess->Settings.bOverride_VignetteIntensity = true; PostProcess->Settings.VignetteIntensity = .25f;
	PostProcess->Settings.bOverride_BloomIntensity = true; PostProcess->Settings.BloomIntensity = .65f;
	PostProcess->RegisterComponent();
	if (auto* System = AuraSystem.LoadSynchronous())
	{
		Aura = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Character->GetRootComponent(), NAME_None,
			FVector(0,0,-40), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false);
		if (Aura) { Aura->SetRelativeScale3D(FVector(.65f)); Aura->Activate(true); }
	}
	if (auto* GameState = GetWorld()->GetGameState<ARogue10mGameState>()) { GameState->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhase); }
	GetWorld()->GetTimerManager().SetTimer(MonitorTimer, this, &ThisClass::Monitor, .1f, true);
	UE_LOG(LogRogue10m, Log, TEXT("Knuckle identity start (prototype): duration %.2f speed %.2f move %.2f damage %.2f"), DurationSeconds, AttackSpeedMultiplier, MovementMultiplier, DamageMultiplier);
	return true;
}
void URogue10mWeaponIdentityComponent::ClearVisuals()
{
	if (Aura) { Aura->DeactivateImmediate(); Aura->DestroyComponent(); Aura = nullptr; }
	if (PostProcess) { PostProcess->DestroyComponent(); PostProcess = nullptr; }
}
void URogue10mWeaponIdentityComponent::DeactivateIdentity()
{
	const bool bWasActive = EndsAt >= 0.f; EndsAt = -1.f;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(MonitorTimer); }
	ClearVisuals();
	if (bWasActive)
	{
		if (auto* Character = Cast<ARogue10mCharacter>(GetOwner()); Character && !Character->IsActorBeingDestroyed()) { Character->RefreshCharacterStats(false); }
		UE_LOG(LogRogue10m, Log, TEXT("Knuckle identity cleared; durable attributes unchanged"));
	}
}
void URogue10mWeaponIdentityComponent::Monitor() { if (!IsIdentityActive()) { DeactivateIdentity(); } }
void URogue10mWeaponIdentityComponent::HandleRunPhase(ERogue10mRunPhase Phase)
{
	if (Phase != ERogue10mRunPhase::Running) { DeactivateIdentity(); }
}
void URogue10mWeaponIdentityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	EndsAt = -1.f;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MonitorTimer);
		if (auto* State = GetWorld()->GetGameState<ARogue10mGameState>()) { State->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRunPhase); }
	}
	ClearVisuals(); Super::EndPlay(Reason);
}
