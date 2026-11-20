// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacter.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "TimerManager.h"

float ARogue10mCharacter::GetCommonSkillValue(ERogue10mCommonSkill Skill) const
{
	const auto* State = GetPlayerState<ARogue10mPlayerState>();
	const auto* Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
	return Tree ? Tree->GetCommonSkillValue(Skill) : 0.0f;
}

void ARogue10mCharacter::SetWeaponJumpAllowance(int32 MaxJumps)
{
	WeaponJumpAllowance = FMath::Max(1, MaxJumps);
	JumpMaxCount = GetCommonSkillValue(ERogue10mCommonSkill::DoubleJump) > 0.0f
		? FMath::Max(2, WeaponJumpAllowance) : WeaponJumpAllowance;
}

void ARogue10mCharacter::CacheMobilityDefaults()
{
	if (bMobilityDefaultsCached) { return; }
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	BaseAirControl = Movement->AirControl;
	BaseGroundFriction = Movement->GroundFriction;
	BaseMaxAcceleration = Movement->MaxAcceleration;
	BaseAirBraking = Movement->BrakingDecelerationFalling;
	bMobilityDefaultsCached = true;
}

void ARogue10mCharacter::RefreshCommonMobility()
{
	CacheMobilityDefaults();
	SetWeaponJumpAllowance(WeaponJumpAllowance);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->AirControl = FMath::Clamp(BaseAirControl * (1.0f + GetCommonSkillValue(ERogue10mCommonSkill::AirControl)), 0.0f, 1.0f);
	Movement->BrakingDecelerationFalling = BaseAirBraking
		* (1.0f - FMath::Clamp(GetCommonSkillValue(ERogue10mCommonSkill::AirMomentum), 0.0f, 1.0f));
	RefreshGroundFriction();
	RefreshMovementAcceleration();
	RefreshMovementSpeed();
}

void ARogue10mCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (bMobilityDefaultsCached) { RefreshMovementAcceleration(); }
}

void ARogue10mCharacter::RefreshMovementAcceleration()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float GroundBonus = Movement->IsMovingOnGround() ? GetCommonSkillValue(ERogue10mCommonSkill::GroundAcceleration) : 0.0f;
	Movement->MaxAcceleration = BaseMaxAcceleration * (1.0f + GroundBonus);
}

void ARogue10mCharacter::RefreshMovementSpeed()
{
	const auto* Identity = FindComponentByClass<URogue10mWeaponIdentityComponent>();
	const UWorld* World = GetWorld();
	const bool bHaste = World && World->GetTimerManager().GetTimerRemaining(EvadeHasteTimerHandle) > 0.0f;
	const float Haste = bHaste ? GetCommonSkillValue(ERogue10mCommonSkill::EvadeHaste) : 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = (bIsSprinting ? SprintWalkSpeed : NormalWalkSpeed)
		* (Identity ? Identity->GetMovementMultiplier() : 1.0f) * (1.0f + Haste);
}

void ARogue10mCharacter::RefreshGroundFriction()
{
	const UWorld* World = GetWorld();
	const bool bStabilized = World && World->GetTimerManager().GetTimerRemaining(LandingStabilityTimerHandle) > 0.0f;
	const float Landing = bStabilized ? GetCommonSkillValue(ERogue10mCommonSkill::LandingStability) : 0.0f;
	GetCharacterMovement()->GroundFriction = BaseGroundFriction
		* (1.0f + GetCommonSkillValue(ERogue10mCommonSkill::GroundTurning) + Landing);
}

void ARogue10mCharacter::BeginLandingStability()
{
	UWorld* World = GetWorld();
	if (!World || IsDead() || GetCommonSkillValue(ERogue10mCommonSkill::LandingStability) <= 0.0f) { return; }
	World->GetTimerManager().SetTimer(LandingStabilityTimerHandle, this, &ThisClass::RefreshGroundFriction,
		FMath::Max(0.05f, LandingStabilityDuration), false);
	RefreshGroundFriction();
}

void ARogue10mCharacter::CheckJumpInput(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!bPressedJump || !Movement)
	{
		Super::CheckJumpInput(DeltaTime);
		return;
	}

	const bool bSecondJump = JumpCurrentCount == 1
		|| (JumpCurrentCount == 0 && Movement->IsFalling() && !CanJumpWhileFalling());
	const float Bonus = GetCommonSkillValue(ERogue10mCommonSkill::JumpHeight)
		+ (bSecondJump ? GetCommonSkillValue(ERogue10mCommonSkill::DoubleJumpUpgrade) : 0.0f);
	const float DefaultJumpSpeed = Movement->JumpZVelocity;
	Movement->JumpZVelocity *= 1.0f + Bonus;
	Super::CheckJumpInput(DeltaTime);
	Movement->JumpZVelocity = DefaultJumpSpeed;
}

float ARogue10mCharacter::GetDodgeCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(FMath::Max(0.0, NextDodgeAllowedTime - World->GetTimeSeconds())) : 0.0f;
}

bool ARogue10mCharacter::GetRollPresentation(float& OutProgress, FVector& OutDirection) const
{
	const UWorld* World = GetWorld();
	if (!World || !IsRolling() || IsDead() || ActiveDodgeDuration <= 0.0f) { return false; }
	OutProgress = FMath::Clamp(static_cast<float>((World->GetTimeSeconds() - DodgeStartedTime) / ActiveDodgeDuration), 0.0f, 1.0f);
	OutDirection = RollDirection;
	return true;
}

void ARogue10mCharacter::DoDodge()
{
	if (IsDead() || IsBlockingWindowVisible() || bIsDodging || GetDodgeCooldownRemaining() > 0.0f) { return; }
	const FVector Input = GetActorRightVector() * CachedMovementInput.X + GetActorForwardVector() * CachedMovementInput.Y;
	const FVector Direction = Input.GetSafeNormal2D(SMALL_NUMBER, GetActorForwardVector());
	if (HasAuthority()) { StartDodge(Direction); }
	else if (IsLocallyControlled()) { ServerRequestDodge(Direction); }
}

void ARogue10mCharacter::ServerRequestDodge_Implementation(FVector_NetQuantizeNormal Direction)
{
	StartDodge(Direction);
}

void ARogue10mCharacter::StartDodge(FVector Direction)
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!World || IsDead() || IsBlockingWindowVisible() || !Movement->IsMovingOnGround()
		|| (CombatComponent && CombatComponent->IsTechniqueMovementActive())
		|| bIsDodging || GetDodgeCooldownRemaining() > 0.0f || Direction.ContainsNaN()) { return; }

	const bool bRoll = GetCommonSkillValue(ERogue10mCommonSkill::Roll) > 0.0f;
	if (bRoll && CombatComponent && CombatComponent->IsWeaponChangeBlocked()) { return; }
	SetSprinting(false);
	const URogue10mDodgeSkillDataAsset* Dodge = CombatComponent ? CombatComponent->GetActiveDodgeSkill() : nullptr;
	const float BaseStaminaCost = Dodge ? Dodge->StaminaCost : 0.0f;
	const float StaminaCost = TraitRuntimeComponent ? TraitRuntimeComponent->GetDodgeStaminaCost(BaseStaminaCost) : BaseStaminaCost;
	URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	if (StaminaCost > 0.0f && (!Attributes || Attributes->GetStamina() < StaminaCost)) { return; }
	if (Attributes && StaminaCost > 0.0f) { Attributes->SetStamina(Attributes->GetStamina() - StaminaCost); }
	if (bRoll && CombatComponent) { CombatComponent->CancelHeldAttackInput(); }

	Direction = Direction.GetSafeNormal2D(SMALL_NUMBER, GetActorForwardVector());
	const float Distance = (bRoll ? RollDistance : Dodge ? Dodge->Distance : DodgeDistance)
		* (1.0f + GetCommonSkillValue(ERogue10mCommonSkill::EvadeDistance));
	const float BaseDuration = bRoll ? RollDuration : Dodge ? Dodge->Duration : DodgeDuration;
	const float RecoveryReduction = FMath::Clamp(GetCommonSkillValue(ERogue10mCommonSkill::EvadeRecovery), 0.0f, 0.9f);
	const float Duration = FMath::Max(0.05f, BaseDuration * (1.0f - RecoveryReduction));
	const float RollCooldownReduction = bRoll ? FMath::Clamp(GetCommonSkillValue(ERogue10mCommonSkill::RollCooldown), 0.0f, 1.0f) : 0.0f;
	const float Cooldown = FMath::Max(Duration, (Dodge ? Dodge->Cooldown : DodgeCooldown) * (1.0f - RollCooldownReduction));

	if (TraitRuntimeComponent) { TraitRuntimeComponent->NotifyDodgeStarted(); }
	const FVector DodgeVelocity = Direction * (Distance / Duration);
	if (bRoll)
	{
		// Roll travel is swept by character movement. The body pose does not move the capsule.
		TSharedPtr<FRootMotionSource_ConstantForce> Travel = MakeShared<FRootMotionSource_ConstantForce>();
		Travel->InstanceName = TEXT("CommonEvade");
		Travel->Priority = 500;
		Travel->AccumulateMode = ERootMotionAccumulateMode::Override;
		Travel->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
		Travel->Duration = Duration;
		Travel->Force = DodgeVelocity;
		RollMotionSourceId = Movement->ApplyRootMotionSource(Travel);
	}
	else
	{
		Movement->Velocity = DodgeVelocity;
	}

	MulticastDodgeStarted(Direction, Duration, Cooldown, bRoll);
}

void ARogue10mCharacter::MulticastDodgeStarted_Implementation(FVector_NetQuantizeNormal Direction,
	float Duration, float Cooldown, bool bRoll)
{
	PresentDodge(Direction, Duration, Cooldown, bRoll);
}

void ARogue10mCharacter::PresentDodge(FVector Direction, float Duration, float Cooldown, bool bRoll)
{
	UWorld* World = GetWorld();
	if (!World || IsDead()) { return; }
	SetSprinting(false);
	bIsDodging = true;
	bIsRolling = bRoll;
	RollDirection = Direction;
	ActiveDodgeDuration = Duration;
	DodgeStartedTime = World->GetTimeSeconds();
	NextDodgeAllowedTime = DodgeStartedTime + Cooldown;
	if (CharacterAnimationComponent && GetNetMode() != NM_DedicatedServer)
	{
		CharacterAnimationComponent->NotifyDodge(bRoll, Duration);
	}
	World->GetTimerManager().SetTimer(DodgeTimerHandle, this, &ThisClass::FinishDodge, Duration, false);
}

void ARogue10mCharacter::FinishDodge()
{
	bIsDodging = bIsRolling = false;
	if (HasAuthority())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		if (RollMotionSourceId != 0)
		{
			Movement->RemoveRootMotionSourceByID(RollMotionSourceId);
			RollMotionSourceId = 0;
		}
		Movement->Velocity.X = 0.0;
		Movement->Velocity.Y = 0.0;
	}
	UWorld* World = GetWorld();
	if (!World || IsDead() || GetCommonSkillValue(ERogue10mCommonSkill::EvadeHaste) <= 0.0f) { return; }
	World->GetTimerManager().SetTimer(EvadeHasteTimerHandle, this, &ThisClass::RefreshMovementSpeed,
		FMath::Max(0.05f, EvadeHasteDuration), false);
	RefreshMovementSpeed();
}

void ARogue10mCharacter::ClearCommonMobilityActions()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeTimerHandle);
		World->GetTimerManager().ClearTimer(LandingStabilityTimerHandle);
		World->GetTimerManager().ClearTimer(EvadeHasteTimerHandle);
	}
	if (RollMotionSourceId != 0)
	{
		GetCharacterMovement()->RemoveRootMotionSourceByID(RollMotionSourceId);
		RollMotionSourceId = 0;
	}
	bIsDodging = bIsRolling = false;
	NextDodgeAllowedTime = 0.0;
	RefreshCommonMobility();
}
