// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mMonsterAttackPatternComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "TimerManager.h"

namespace
{
float Positive(float Value, float Fallback)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.01f, Value) : Fallback;
}

float NonNegative(float Value, float Fallback)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : Fallback;
}

constexpr ERogue10mMonsterAttackType BossSequence[] = {
	ERogue10mMonsterAttackType::Basic, ERogue10mMonsterAttackType::FrontalWave,
	ERogue10mMonsterAttackType::Basic, ERogue10mMonsterAttackType::AbyssZone
};
}

URogue10mMonsterAttackPatternComponent::URogue10mMonsterAttackPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ARogue10mBasicMonster* URogue10mMonsterAttackPatternComponent::GetMonster() const
{
	return Cast<ARogue10mBasicMonster>(GetOwner());
}

void URogue10mMonsterAttackPatternComponent::ConfigureProfile(URogue10mMonsterAttackPatternProfile* InProfile)
{
	CancelAttack();
	UnbindHealthDelegate();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocomotionTimer);
	Profile = InProfile;
	const bool bRender = GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer;
	AttackEffect = Profile && bRender ? Profile->AttackEffect.LoadSynchronous() : nullptr;
	ImpactEffect = Profile && bRender ? Profile->ImpactEffect.LoadSynchronous() : nullptr;
	FrontalWaveEffect = Profile && bRender ? Profile->FrontalWaveEffect.LoadSynchronous() : nullptr;
	AbyssZoneEffect = Profile && bRender ? Profile->AbyssZoneEffect.LoadSynchronous() : nullptr;
	NextAttackTime = 0.0;
	NextFrontalWaveTime = 0.0;
	NextAbyssZoneTime = 0.0;
	NextSequenceIndex = 0;
	CurrentAttackType = ERogue10mMonsterAttackType::Basic;
	bEnraged = false;
	FrontalWaveMaterial = nullptr;
	AbyssZoneMaterial = nullptr;
	if (Profile && Profile->bEnableBossPatterns)
	{
		FrontalWaveMaterial = Profile->FrontalWaveTelegraphMaterial.LoadSynchronous();
		AbyssZoneMaterial = Profile->AbyssZoneTelegraphMaterial.LoadSynchronous();
		if (!FrontalWaveMaterial || !AbyssZoneMaterial)
		{
			UE_LOG(LogRogue10m, Warning, TEXT("%s boss telegraph material missing; that special attack stays disabled."),
				*GetNameSafe(GetOwner()));
		}
		if (ARogue10mBasicMonster* Monster = GetMonster())
		{
			if (UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent())
			{
				HealthDelegateASC = ASC;
				HealthDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute())
					.AddUObject(this, &ThisClass::OnHealthChanged);
			}
		}
		RefreshEnrage();
	}
	if (IsEnabled() && GetWorld())
	{
		SaveAnimationMode();
		UpdateLocomotionPresentation();
		GetWorld()->GetTimerManager().SetTimer(LocomotionTimer, this,
			&ThisClass::UpdateLocomotionPresentation, 0.15f, true);
	}
	else
	{
		RestoreAnimation();
	}
}

float URogue10mMonsterAttackPatternComponent::GetEngagementRange() const
{
	return Profile ? GetRangeForAttack(IsAttackActive() ? CurrentAttackType : SelectAvailableAttack()) : 0.0f;
}

ERogue10mMonsterAttackType URogue10mMonsterAttackPatternComponent::SelectAvailableAttack() const
{
	if (!Profile || !Profile->bEnableBossPatterns || !GetWorld()) return ERogue10mMonsterAttackType::Basic;
	const ERogue10mMonsterAttackType Scheduled = BossSequence[NextSequenceIndex];
	const double Now = GetWorld()->GetTimeSeconds();
	if (Scheduled == ERogue10mMonsterAttackType::FrontalWave && FrontalWaveMaterial && Now >= NextFrontalWaveTime)
		return Scheduled;
	if (Scheduled == ERogue10mMonsterAttackType::AbyssZone && AbyssZoneMaterial && Now >= NextAbyssZoneTime)
		return Scheduled;
	// When a scheduled special is cooling down, use melee's range and keep that slot pending.
	return ERogue10mMonsterAttackType::Basic;
}

float URogue10mMonsterAttackPatternComponent::GetRangeForAttack(ERogue10mMonsterAttackType AttackType) const
{
	if (!Profile) return 0.0f;
	if (AttackType == ERogue10mMonsterAttackType::FrontalWave) return Positive(Profile->FrontalWaveRange, 850.0f);
	if (AttackType == ERogue10mMonsterAttackType::AbyssZone) return Positive(Profile->AbyssZoneCastRange, 1000.0f);
	return Positive(Profile->AttackRange, 220.0f);
}

void URogue10mMonsterAttackPatternComponent::SnapshotAttackSettings()
{
	LockedRange = GetRangeForAttack(CurrentAttackType);
	LockedArcDegrees = FMath::Clamp(Profile->AttackArcDegrees, 1.0f, 180.0f);
	LockedHalfWidth = LockedRange;
	LockedHalfHeight = Positive(Profile->AttackHalfHeight, 120.0f);
	LockedWindupSeconds = Positive(Profile->AttackWindupSeconds, 0.35f);
	LockedRecoverySeconds = Positive(Profile->AttackRecoverySeconds, 0.45f);
	LockedCooldownSeconds = NonNegative(Profile->AttackCooldownSeconds, 0.4f);
	LockedDamageMultiplier = NonNegative(Profile->AttackDamageMultiplier, 0.0f);
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave)
	{
		LockedHalfWidth = Positive(Profile->FrontalWaveHalfWidth, 180.0f);
		LockedHalfHeight = Positive(Profile->FrontalWaveHalfHeight, 120.0f);
		LockedWindupSeconds = Positive(Profile->FrontalWaveWindupSeconds, 1.1f);
		LockedRecoverySeconds = Positive(Profile->FrontalWaveRecoverySeconds, 0.65f);
		LockedCooldownSeconds = NonNegative(Profile->FrontalWaveCooldownSeconds, 6.0f);
		LockedDamageMultiplier = NonNegative(Profile->FrontalWaveDamageMultiplier, 0.0f);
	}
	else if (CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone)
	{
		LockedRange = Positive(Profile->AbyssZoneRadius, 260.0f);
		LockedHalfWidth = LockedRange;
		LockedHalfHeight = Positive(Profile->AbyssZoneHalfHeight, 120.0f);
		LockedWindupSeconds = Positive(Profile->AbyssZoneWindupSeconds, 1.4f);
		LockedRecoverySeconds = Positive(Profile->AbyssZoneRecoverySeconds, 0.8f);
		LockedCooldownSeconds = NonNegative(Profile->AbyssZoneCooldownSeconds, 8.0f);
		LockedDamageMultiplier = NonNegative(Profile->AbyssZoneDamageMultiplier, 0.0f);
	}
	// Snapshot a special's phase at its start: HP loss never shortens a warning already shown.
	if (bEnraged && CurrentAttackType != ERogue10mMonsterAttackType::Basic)
	{
		LockedWindupSeconds *= Positive(Profile->EnrageWindupMultiplier, 0.75f);
		LockedRecoverySeconds *= Positive(Profile->EnrageRecoveryMultiplier, 0.8f);
		LockedCooldownSeconds *= Positive(Profile->EnrageCooldownMultiplier, 0.75f);
		LockedDamageMultiplier *= NonNegative(Profile->EnrageDamageMultiplier, 1.25f);
	}
}

void URogue10mMonsterAttackPatternComponent::RefreshEnrage()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Profile->bEnableBossPatterns || bEnraged || !Monster || Monster->IsDead()) return;
	const URogue10mAttributeSet* Attributes = Monster->GetRogueAttributeSet();
	if (!Attributes || !FMath::IsFinite(Attributes->GetHealth()) || !FMath::IsFinite(Attributes->GetMaxHealth())
		|| Attributes->GetHealth() <= 0.0f || Attributes->GetMaxHealth() <= 0.0f) return;
	const float Threshold = FMath::IsFinite(Profile->EnrageHealthThreshold)
		? FMath::Clamp(Profile->EnrageHealthThreshold, 0.0f, 1.0f) : 0.5f;
	if (Attributes->GetHealth() / Attributes->GetMaxHealth() <= Threshold)
	{
		bEnraged = true;
		UE_LOG(LogRogue10m, Log, TEXT("%s entered boss enrage phase at %.1f%% health."),
			*Monster->GetName(), 100.0f * Attributes->GetHealth() / Attributes->GetMaxHealth());
	}
}

void URogue10mMonsterAttackPatternComponent::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshEnrage();
	if (Data.NewValue <= 0.0f) CancelAttack();
}

void URogue10mMonsterAttackPatternComponent::UnbindHealthDelegate()
{
	if (UAbilitySystemComponent* ASC = HealthDelegateASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute()).Remove(HealthDelegateHandle);
	}
	HealthDelegateHandle.Reset();
	HealthDelegateASC.Reset();
}

bool URogue10mMonsterAttackPatternComponent::TryStartAttack(ARogue10mCharacter* Target)
{
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Monster || !Monster->HasAuthority() || Monster->IsDead()
		|| !IsValid(Target) || Target->IsDead() || !GetWorld() || IsAttackActive()
		|| GetWorld()->GetTimeSeconds() < NextAttackTime)
	{
		return false;
	}
	RefreshEnrage();
	CurrentAttackType = SelectAvailableAttack();
	if (FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation()) > GetRangeForAttack(CurrentAttackType)) return false;
	SnapshotAttackSettings();
	LockedTarget = Target;
	HitTargets.Reset();
	LockedSourceLocation = Monster->GetActorLocation();
	LockedLocation = CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone
		? Target->GetActorLocation() : LockedSourceLocation;
	LockedForward = (Target->GetActorLocation() - LockedSourceLocation).GetSafeNormal2D();
	if (LockedForward.IsNearlyZero()) LockedForward = Monster->GetActorForwardVector();
	if (CurrentAttackType != ERogue10mMonsterAttackType::Basic && !CreateTelegraph())
	{
		DestroyTelegraph();
		LockedTarget.Reset();
		return false;
	}
	Monster->SetActorRotation(LockedForward.Rotation());
	if (AAIController* AI = Cast<AAIController>(Monster->GetController()))
	{
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
	Monster->GetCharacterMovement()->StopMovementImmediately();
	GetWorld()->GetTimerManager().ClearTimer(HitReactionTimer);
	PatternState = ERogue10mMonsterPatternState::Windup;
	WindupStartedTime = GetWorld()->GetTimeSeconds();
	++StartedAttackCount;
	if (Profile->bEnableBossPatterns)
	{
		const ERogue10mMonsterAttackType Scheduled = BossSequence[NextSequenceIndex];
		const bool bScheduledMaterialMissing = (Scheduled == ERogue10mMonsterAttackType::FrontalWave && !FrontalWaveMaterial)
			|| (Scheduled == ERogue10mMonsterAttackType::AbyssZone && !AbyssZoneMaterial);
		if (CurrentAttackType == Scheduled || bScheduledMaterialMissing)
			NextSequenceIndex = (NextSequenceIndex + 1) % UE_ARRAY_COUNT(BossSequence);
		const double SpecialAvailableTime = GetWorld()->GetTimeSeconds() + LockedWindupSeconds + LockedRecoverySeconds + LockedCooldownSeconds;
		if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave) NextFrontalWaveTime = SpecialAvailableTime;
		if (CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone) NextAbyssZoneTime = SpecialAvailableTime;
		GetWorld()->GetTimerManager().SetTimer(AttackGuardTimer, this, &ThisClass::CheckAttackValidity, 0.05f, true);
	}
	PlayAttackPresentation();
	GetWorld()->GetTimerManager().SetTimer(StateTimer, this, &ThisClass::ExecuteAttack,
		LockedWindupSeconds, false);
	return true;
}

bool URogue10mMonsterAttackPatternComponent::TryStartCombatTestAttack(ARogue10mCharacter* Target,
	ERogue10mMonsterAttackType Type)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Monster || !Monster->IsCombatTestInstance() || !Profile || !GetWorld()
		|| GetWorld()->GetNetMode() != NM_Standalone || !Monster->HasAuthority()
		|| (Type != ERogue10mMonsterAttackType::Basic && !Profile->bEnableBossPatterns)
		|| (Type == ERogue10mMonsterAttackType::FrontalWave && !FrontalWaveMaterial)
		|| (Type == ERogue10mMonsterAttackType::AbyssZone && !AbyssZoneMaterial)) return false;
	CancelAttack();
	NextAttackTime = NextFrontalWaveTime = NextAbyssZoneTime = 0.0;
	NextSequenceIndex = Type == ERogue10mMonsterAttackType::FrontalWave ? 1
		: Type == ERogue10mMonsterAttackType::AbyssZone ? 3 : 0;
	return TryStartAttack(Target) && CurrentAttackType == Type;
#endif
}

bool URogue10mMonsterAttackPatternComponent::HasClearHitPath(const ARogue10mCharacter& Target) const
{
	if (!GetWorld()) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterBasicAttackVisibility), false, GetOwner());
	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, LockedSourceLocation,
		Target.GetActorLocation(), ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == &Target;
}

bool URogue10mMonsterAttackPatternComponent::IsInsideAttackShape(const ARogue10mCharacter& Target) const
{
	const UCapsuleComponent* Capsule = Target.GetCapsuleComponent();
	if (!Capsule) return false;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Offset = Target.GetActorLocation() - LockedLocation;
	if (FMath::Abs(Offset.Z) > LockedHalfHeight + Capsule->GetScaledCapsuleHalfHeight()) return false;
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave)
	{
		const float ForwardDistance = FVector::DotProduct(Offset, LockedForward);
		const FVector Right(-LockedForward.Y, LockedForward.X, 0.0f);
		return ForwardDistance >= 0.0f && ForwardDistance <= LockedRange + Radius
			&& FMath::Abs(FVector::DotProduct(Offset, Right)) <= LockedHalfWidth + Radius;
	}
	if (CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone)
	{
		return Offset.SizeSquared2D() <= FMath::Square(LockedRange + Radius);
	}
	const float Distance = Offset.Size2D();
	const float AngularPadding = FMath::Asin(FMath::Clamp(Radius / FMath::Max(Distance, Radius), 0.0f, 1.0f));
	const float HalfAngle = FMath::DegreesToRadians(LockedArcDegrees * 0.5f) + AngularPadding;
	return Distance <= LockedRange + Radius
		&& FVector::DotProduct(LockedForward, Offset.GetSafeNormal2D()) >= FMath::Cos(HalfAngle);
}

void URogue10mMonsterAttackPatternComponent::CheckAttackValidity()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	ARogue10mCharacter* Target = LockedTarget.Get();
	if (!IsAttackActive() || !Profile || !Monster || Monster->IsDead() || !Monster->HasAuthority()
		|| !IsValid(Target) || Target->IsDead())
	{
		CancelAttack();
		return;
	}
	if (TelegraphMaterialInstance && PatternState == ERogue10mMonsterPatternState::Windup && GetWorld())
	{
		TelegraphMaterialInstance->SetScalarParameterValue(TEXT("Progress"), FMath::Clamp(
			static_cast<float>((GetWorld()->GetTimeSeconds() - WindupStartedTime) / LockedWindupSeconds), 0.0f, 1.0f));
	}
}

bool URogue10mMonsterAttackPatternComponent::CreateTelegraph()
{
	DestroyTelegraph();
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !GetWorld() || !Monster) return false;
	UMaterialInterface* Material = CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave
		? FrontalWaveMaterial.Get() : AbyssZoneMaterial.Get();
	if (!Material) return false;
	FVector Center = LockedLocation;
	FVector Size(Positive(Profile->TelegraphDepth, 180.0f), LockedRange, LockedRange);
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave)
	{
		Center += LockedForward * (LockedRange * 0.5f);
		Size.Y = LockedHalfWidth;
		Size.Z = LockedRange * 0.5f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterBossTelegraphGround), false, Monster);
	if (LockedTarget.IsValid()) Params.AddIgnoredActor(LockedTarget.Get());
	FHitResult Ground;
	if (GetWorld()->LineTraceSingleByChannel(Ground, Center + FVector(0, 0, 1000),
		Center - FVector(0, 0, 2500), ECC_Visibility, Params))
	{
		Center.Z = Ground.ImpactPoint.Z;
	}
	else
	{
		Center.Z -= CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone && LockedTarget.IsValid()
			? LockedTarget->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	Center.Z += NonNegative(Profile->TelegraphGroundOffset, 2.0f);
	TelegraphDecal = NewObject<UDecalComponent>(Monster);
	if (!TelegraphDecal) return false;
	TelegraphDecal->SetDecalMaterial(Material);
	TelegraphDecal->DecalSize = Size;
	TelegraphDecal->SetAbsolute(true, true, true);
	TelegraphDecal->SetFadeScreenSize(0.001f);
	TelegraphMaterialInstance = TelegraphDecal->CreateDynamicMaterialInstance();
	if (TelegraphMaterialInstance)
	{
		const FLinearColor Color = bEnraged ? FLinearColor(1.0f, 0.025f, 0.02f)
			: CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave
				? FLinearColor(1.0f, 0.18f, 0.025f) : FLinearColor(0.62f, 0.055f, 0.95f);
		TelegraphMaterialInstance->SetVectorParameterValue(TEXT("WarningColor"), Color);
		TelegraphMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), 0.55f);
		TelegraphMaterialInstance->SetScalarParameterValue(TEXT("Progress"), 0.0f);
	}
	TelegraphDecal->SetWorldLocationAndRotation(Center, FRotator(-90.0f, LockedForward.Rotation().Yaw, 0.0f));
	TelegraphDecal->RegisterComponentWithWorld(GetWorld());
	return TelegraphDecal->IsRegistered();
}

void URogue10mMonsterAttackPatternComponent::DestroyTelegraph()
{
	if (IsValid(TelegraphDecal)) TelegraphDecal->DestroyComponent();
	TelegraphDecal = nullptr;
	TelegraphMaterialInstance = nullptr;
}

bool URogue10mMonsterAttackPatternComponent::IsTelegraphVisible() const
{
	return IsValid(TelegraphDecal) && TelegraphDecal->IsRegistered() && TelegraphDecal->IsVisible();
}

void URogue10mMonsterAttackPatternComponent::ExecuteAttack()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	ARogue10mCharacter* Target = LockedTarget.Get();
	if (!Profile || !Monster || Monster->IsDead() || !Monster->HasAuthority()
		|| !GetWorld() || !IsValid(Target) || Target->IsDead())
	{
		CancelAttack();
		return;
	}
	// Only the locked geometry is tested; phase changes and subsequent AI range selection cannot move it.
	UNiagaraSystem* Effect = AttackEffect;
	FVector EffectLocation = LockedSourceLocation + LockedForward * FMath::Min(LockedRange * 0.5f, 120.0f);
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave)
	{
		Effect = FrontalWaveEffect;
		EffectLocation = LockedSourceLocation + LockedForward * 80.0f;
	}
	else if (CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone)
	{
		Effect = AbyssZoneEffect;
		EffectLocation = LockedLocation;
	}
	if (SpawnEffect(Effect, EffectLocation, LockedForward.Rotation(), Profile->AttackEffectScale)) ++AttackEffectCount;
	FVector OverlapCenter = LockedLocation;
	FVector OverlapExtent(LockedRange, LockedHalfWidth, LockedHalfHeight);
	FQuat OverlapRotation = FQuat::Identity;
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave)
	{
		OverlapCenter += LockedForward * (LockedRange * 0.5f);
		OverlapExtent.X = LockedRange * 0.5f;
		OverlapRotation = LockedForward.Rotation().Quaternion();
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterBasicAttack), false, Monster);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, OverlapCenter, OverlapRotation, Objects,
		FCollisionShape::MakeBox(OverlapExtent), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		// Every pattern hits its selected living target at most once, even if it has multiple overlapping components.
		if (Overlap.GetActor() != Target) continue;
		if (IsInsideAttackShape(*Target) && HasClearHitPath(*Target))
		{
			DamageTarget(Target);
		}
		break;
	}
	// Hide at the actual hit, never retain a stale danger warning during recovery.
	DestroyTelegraph();
	if (!Profile || PatternState != ERogue10mMonsterPatternState::Windup || Monster->IsDead()) return;
	PatternState = ERogue10mMonsterPatternState::Recovery;
	GetWorld()->GetTimerManager().SetTimer(StateTimer, this, &ThisClass::FinishRecovery,
		LockedRecoverySeconds, false);
}

void URogue10mMonsterAttackPatternComponent::DamageTarget(ARogue10mCharacter* Target)
{
	ARogue10mBasicMonster* Monster = GetMonster();
	const TWeakObjectPtr<ARogue10mCharacter> TargetHandle(Target);
	if (!Profile || !Monster || !Monster->HasAuthority() || Monster->IsDead()
		|| !IsValid(Target) || Target->IsDead() || HitTargets.Contains(TargetHandle)) return;
	HitTargets.Add(TargetHandle);
	const float Damage = Monster->GetPatternBaseDamage() * LockedDamageMultiplier;
	if (Damage > 0.0f)
	{
		const float AppliedDamage = UGameplayStatics::ApplyDamage(Target, Damage,
			Monster->GetController(), Monster, UDamageType::StaticClass());
		if (AppliedDamage > 0.0f)
		{
			++SuccessfulHitCount;
			const FVector TowardMonster = (Monster->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			if (SpawnEffect(ImpactEffect, Target->GetActorLocation() + TowardMonster * 35.0f,
				(-TowardMonster).Rotation(), Profile->ImpactEffectScale)) ++ImpactEffectCount;
		}
	}
}

void URogue10mMonsterAttackPatternComponent::FinishRecovery()
{
	DestroyTelegraph();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AttackGuardTimer);
	PatternState = ERogue10mMonsterPatternState::Idle;
	LockedTarget.Reset();
	if (GetWorld() && Profile)
	{
		NextAttackTime = GetWorld()->GetTimeSeconds() + (FMath::IsFinite(Profile->AttackCooldownSeconds)
			? FMath::Max(0.0f, Profile->AttackCooldownSeconds) : 0.4f);
	}
	RestoreAnimation();
}

void URogue10mMonsterAttackPatternComponent::CancelAttack()
{
	ClearEffects();
	DestroyTelegraph();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StateTimer);
		GetWorld()->GetTimerManager().ClearTimer(HitReactionTimer);
		GetWorld()->GetTimerManager().ClearTimer(AttackGuardTimer);
	}
	PatternState = ERogue10mMonsterPatternState::Idle;
	LockedTarget.Reset();
	HitTargets.Reset();
	RestoreAnimation();
}

bool URogue10mMonsterAttackPatternComponent::SpawnEffect(UNiagaraSystem* System,
	const FVector& Location, const FRotator& Rotation, float Scale)
{
	if (!System || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;
	ActiveEffects.RemoveAll([](const auto& FX) { return !FX.IsValid(); });
	// AutoRelease components must never be destroyed manually: deactivation
	// returns them to Niagara's world pool when their finite burst completes.
	auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, Rotation,
		FVector(FMath::Clamp(Scale, 0.01f, 5.0f)), true, false, ENCPoolMethod::AutoRelease, false);
	if (!FX) return false;
	FX->OnSystemFinished.AddDynamic(this, &ThisClass::HandleEffectFinished);
	ActiveEffects.Add(FX);
	FX->Activate(true);
	return true;
}

int32 URogue10mMonsterAttackPatternComponent::GetActiveEffectCount() const
{
	int32 Count = 0;
	for (const auto& FX : ActiveEffects) if (FX.IsValid() && FX->IsActive()) ++Count;
	return Count;
}

void URogue10mMonsterAttackPatternComponent::ClearEffects()
{
	const auto Effects = MoveTemp(ActiveEffects);
	ActiveEffects.Reset();
	for (const auto& FX : Effects)
	{
		if (!FX.IsValid()) continue;
		FX->OnSystemFinished.RemoveDynamic(this, &ThisClass::HandleEffectFinished);
		FX->DeactivateImmediate();
	}
}

void URogue10mMonsterAttackPatternComponent::HandleEffectFinished(UNiagaraComponent* FinishedEffect)
{
	ActiveEffects.RemoveAll([FinishedEffect](const auto& FX) { return !FX.IsValid() || FX.Get() == FinishedEffect; });
	if (FinishedEffect) FinishedEffect->OnSystemFinished.RemoveDynamic(this, &ThisClass::HandleEffectFinished);
}

void URogue10mMonsterAttackPatternComponent::SaveAnimationMode()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!bSavedAnimationMode && Monster && Monster->GetMesh())
	{
		SavedAnimationMode = static_cast<uint8>(Monster->GetMesh()->GetAnimationMode());
		SavedAnimationClass = Monster->GetMesh()->GetAnimClass();
		bSavedAnimationMode = true;
	}
}

void URogue10mMonsterAttackPatternComponent::PlayAttackPresentation()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Monster || !Monster->GetMesh()) return;
	SaveAnimationMode();
	UAnimSequence* Animation = nullptr;
	if (CurrentAttackType == ERogue10mMonsterAttackType::FrontalWave) Animation = Profile->FrontalWaveAnimation.LoadSynchronous();
	if (CurrentAttackType == ERogue10mMonsterAttackType::AbyssZone) Animation = Profile->AbyssZoneAnimation.LoadSynchronous();
	if (!Animation) Animation = Profile->AttackAnimation.LoadSynchronous();
	if (Animation)
	{
		Monster->GetMesh()->PlayAnimation(Animation, false);
		const float Duration = LockedWindupSeconds + LockedRecoverySeconds;
		Monster->GetMesh()->SetPlayRate(Animation->GetPlayLength() / Duration);
	}
}

void URogue10mMonsterAttackPatternComponent::UpdateLocomotionPresentation()
{
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Monster || Monster->IsDead() || IsAttackActive() || !GetWorld()
		|| GetWorld()->GetTimerManager().IsTimerActive(HitReactionTimer)) return;
	UAnimSequence* Animation = Monster->GetVelocity().SizeSquared2D() > 100.0f
		? Profile->MovementAnimation.LoadSynchronous() : Profile->IdleAnimation.LoadSynchronous();
	if (!Animation) return;
	UAnimSingleNodeInstance* SingleNode = Monster->GetMesh()->GetSingleNodeInstance();
	if (!SingleNode || SingleNode->GetCurrentAsset() != Animation)
	{
		Monster->GetMesh()->PlayAnimation(Animation, true);
		Monster->GetMesh()->SetPlayRate(1.0f);
	}
}

void URogue10mMonsterAttackPatternComponent::RestoreAnimation()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitReactionTimer);
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Monster || !Monster->GetMesh()) return;
	if (Profile && !Monster->IsDead())
	{
		UpdateLocomotionPresentation();
		return;
	}
	if (bSavedAnimationMode)
	{
		Monster->GetMesh()->SetAnimationMode(static_cast<EAnimationMode::Type>(SavedAnimationMode));
		if (SavedAnimationClass && SavedAnimationMode == static_cast<uint8>(EAnimationMode::AnimationBlueprint))
			Monster->GetMesh()->SetAnimInstanceClass(SavedAnimationClass);
	}
}

void URogue10mMonsterAttackPatternComponent::PlayHitReaction()
{
	RefreshEnrage();
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Monster || Monster->IsDead() || IsAttackActive() || !GetWorld()) return;
	if (UAnimSequence* Animation = Profile->HitAnimation.LoadSynchronous())
	{
		Monster->GetMesh()->PlayAnimation(Animation, false);
		Monster->GetMesh()->SetPlayRate(1.0f);
		GetWorld()->GetTimerManager().SetTimer(HitReactionTimer, this, &ThisClass::RestoreAnimation,
			Positive(Animation->GetPlayLength(), 0.25f), false);
	}
}

float URogue10mMonsterAttackPatternComponent::PlayDeathPresentation()
{
	CancelAttack();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocomotionTimer);
	ARogue10mBasicMonster* Monster = GetMonster();
	if (!Profile || !Monster) return 0.0f;
	if (UAnimSequence* Animation = Profile->DeathAnimation.LoadSynchronous())
	{
		Monster->GetMesh()->PlayAnimation(Animation, false);
		Monster->GetMesh()->SetPlayRate(1.0f);
		return Positive(Animation->GetPlayLength(), 1.0f) + 0.05f;
	}
	return 0.0f;
}

void URogue10mMonsterAttackPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAttack();
	UnbindHealthDelegate();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocomotionTimer);
	Super::EndPlay(EndPlayReason);
}
