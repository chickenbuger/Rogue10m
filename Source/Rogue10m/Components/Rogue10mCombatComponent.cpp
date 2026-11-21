// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCombatComponent.h"
#include "Rogue10mCombatVfxPlacement.h"
#include "Rogue10mMeleeVfxComponent.h"
#include "Rogue10mMeleeVfxGeometry.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "Rogue10mTechniqueMotionComponent.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Rogue10mCombatTestComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackResourceBudget.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mHitFeedbackComponent.h"
#include "Rogue10mAttackTargetInterface.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mGameplayAbility_Attack.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerFeedbackComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mMonsterDataAsset.h"
#include "TimerManager.h"

namespace
{
int32 ResolveMeleeVfxTier(const ARogue10mCharacter& Character, const URogue10mAttackSkillData& Skill,
	ERogue10mWeaponType Weapon, ERogue10mSkillTreeTrack Track)
{
	if (!Skill.bEnableMeleeReadabilityVfx || !Character.IsThirdPersonGameplayActive()
		|| Character.GetNetMode() == NM_DedicatedServer || Skill.AttackShape == ERogue10mAttackShape::Projectile
		|| Skill.SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique
		|| (Weapon != ERogue10mWeaponType::Knuckle && Weapon != ERogue10mWeaponType::GreatSword))
	{
		return INDEX_NONE;
	}

	const auto* State = Character.GetPlayerState<ARogue10mPlayerState>();
	const auto* Progression = State ? State->GetSkillTreeProgressionComponent() : nullptr;
	const auto* Catalog = Progression ? Progression->GetCatalog() : URogue10mSkillTreeProgressionComponent::GetDefaultCatalog();
	if (Catalog)
	{
		for (const auto& Node : Catalog->Nodes)
		{
			if (Node.Track != Track || Node.Skill.Get() != &Skill) { continue; }
			const auto MatchesTrack = [Track](const FRogue10mSkillTreeMasteryRequirement& Row) { return Row.Track == Track; };
			const auto* Requirement = Node.MasteryAlternatives.FindByPredicate(MatchesTrack);
			if (!Requirement)
			{
				// The Sword web retains the original levels for old saves after switching to point purchases.
				Requirement = Node.HistoricalAutomaticMasteryAlternatives.FindByPredicate(MatchesTrack);
			}
			if (Requirement)
			{
				const int32 Level = Requirement->Level;
				return Level >= 20 ? 3 : Level >= 10 ? 2 : Level >= 5 ? 1 : 0;
			}
			break;
		}
	}

	// Innate charged and special actions still read heavier than a tap.
	const bool bHeavy = Skill.InputSlot == ERogue10mAttackInputSlot::ChargedPrimary
		|| Skill.InputSlot == ERogue10mAttackInputSlot::ChargedSpecial
		|| Skill.InputSlot == ERogue10mAttackInputSlot::Special;
	return FMath::Max(bHeavy ? 1 : 0, FMath::Clamp(Skill.SkillTreeTier - 1, 0, 3));
}

UStaticMeshComponent* GetMeleeBlade(const ARogue10mCharacter& Character)
{
	const auto* Visual = Character.FindComponentByClass<URogue10mWeaponVisualComponent>();
	return Visual ? Visual->GetRightWeaponMesh() : nullptr;
}

Rogue10m::MeleeVfx::FImpactPoint SampleMeleeImpact(const ARogue10mCharacter& Character, const AActor& Target,
	const URogue10mAttackSkillData& Skill, bool bSword)
{
	using namespace Rogue10m::MeleeVfx;
	const FVector Origin = Character.GetPawnViewLocation();
	if (bSword)
	{
		FVector Base, Tip;
		if (const auto* Blade = GetMeleeBlade(Character); Blade && GetBladeSegment(*Blade, Base, Tip))
		{
			return FindImpact(Target, Base, Tip, Origin);
		}
	}

	const auto* Mesh = Character.GetMesh();
	const auto HandPosition = [Mesh, &Origin](FName Socket)
	{
		return Mesh && Mesh->DoesSocketExist(Socket) ? Mesh->GetSocketLocation(Socket) : Origin;
	};
	const FVector Hand = HandPosition(Skill.EffectAttachSocket);
	const auto MainHit = FindImpact(Target, Hand, Hand, Origin);
	if (Skill.bSpawnEffectOnOffHand)
	{
		const FVector OtherHand = HandPosition(Skill.OffHandEffectAttachSocket);
		const auto OtherHit = FindImpact(Target, OtherHand, OtherHand, Origin);
		if (FVector::DistSquared(OtherHand, OtherHit.Location) < FVector::DistSquared(Hand, MainHit.Location))
		{
			return OtherHit;
		}
	}
	return MainHit;
}
}

URogue10mCombatComponent::URogue10mCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultAttackAbilityClass = URogue10mGameplayAbility_Attack::StaticClass();
}

void URogue10mCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeSpawnedLoadout();
	if (GetOwner() && !GetOwner()->FindComponentByClass<URogue10mWeaponIdentityComponent>())
	{
		auto* Identity = NewObject<URogue10mWeaponIdentityComponent>(GetOwner());
		GetOwner()->AddInstanceComponent(Identity); Identity->RegisterComponent();
	}
}

void URogue10mCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARogue10mPlayerState* ProgressionState = BoundProgressionState.Get())
	{
		ProgressionState->OnMartialArtsProgressChanged.RemoveDynamic(
			this, &URogue10mCombatComponent::HandleMartialArtsProgressChanged);
		if (auto* Tree = ProgressionState->GetSkillTreeProgressionComponent())
		{
			Tree->OnChanged.RemoveDynamic(this, &URogue10mCombatComponent::HandleHybridSkillTreeChanged);
		}
	}
	BoundProgressionState.Reset();
	CancelCombatVisuals();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthoredContactMonitorTimer);
		for (TPair<uint32, FRogue10mActiveAttackExecution>& Pair : ActiveAttackExecutions)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
		}
	}
	ActiveAttackExecutions.Reset();
	Super::EndPlay(EndPlayReason);
}

bool URogue10mCombatComponent::TryActivateWeaponIdentity()
{
	auto* Identity = GetOwner() ? GetOwner()->FindComponentByClass<URogue10mWeaponIdentityComponent>() : nullptr;
	return CanUseCombatInput() && Identity && Identity->TryActivate();
}

void URogue10mCombatComponent::InitializeAbilitySystem()
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!Character || !AbilitySystem)
	{
		return;
	}

	if (Character->HasAuthority() && DefaultAttackAbilityClass
		&& !AbilitySystem->FindAbilitySpecFromClass(DefaultAttackAbilityClass))
	{
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(DefaultAttackAbilityClass, 1, INDEX_NONE, this));
	}
}

void URogue10mCombatComponent::InitializeSpawnedLoadout()
{
	ApplyCharacterData();
	BindProgressionState();
	ApplyActiveWeaponProfile();
	EvaluateActiveSkillUnlocks();
	InitializeAbilitySystem();

	const URogue10mAttackSkillData* PrimarySkill =
		GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
	if (!PrimarySkill)
	{
		UE_LOG(
			LogRogue10m,
			Warning,
			TEXT("%s 스폰 로드아웃에 좌클릭 Primary 공격이 없습니다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(
		LogRogue10m,
		Log,
		TEXT("%s 스폰 로드아웃 적용: %s / %s"),
		*GetNameSafe(GetOwner()),
		*UEnum::GetValueAsString(AppliedProfileWeaponType),
		*PrimarySkill->SkillName.ToString());
}

bool URogue10mCombatComponent::IsAttackChargeHeld() const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	if (Character && Character->HasPendingMouseAttackInput()) { return true; }
	if (const URogue10mBasicBrawlerComponent* Basic = Character ? Character->GetBasicBrawlerComponent() : nullptr;
		Basic && Basic->IsBasicBrawlerActive())
	{
		return Basic->GetSnapshot().Phase == ERogue10mBasicBrawlerPhase::Charging || HeldBoundSkill.IsValid();
	}
	return LeftAttackPressedTime >= 0.0f || RightAttackPressedTime >= 0.0f || HeldBoundSkill.IsValid();
}

bool URogue10mCombatComponent::IsWeaponChangeBlocked() const
{
	if (bAcceptingAttack || PendingAbilityAttackSkill.IsValid() || IsAttackChargeHeld()
		|| (GetWorld() && GetWorld()->GetTimeSeconds() < AttackActionEndTime)) { return true; }
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const URogue10mBasicBrawlerComponent* Basic = Character ? Character->GetBasicBrawlerComponent() : nullptr;
	return Basic && Basic->IsBasicBrawlerActive()
		&& Basic->GetSnapshot().Phase == ERogue10mBasicBrawlerPhase::Attacking;
}

void URogue10mCombatComponent::HandleAttackPressed(bool bPrimaryAttack)
{
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (Character->IsRolling()) { return; }
		if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent(); Basic && Basic->IsBasicBrawlerActive())
		{
			Basic->HandleAttackPressed(bPrimaryAttack);
			return;
		}
	}
	if (!CanUseCombatInput() || !GetWorld())
	{
		return;
	}

	float& PressedTime = bPrimaryAttack ? LeftAttackPressedTime : RightAttackPressedTime;
	PressedTime = GetWorld()->GetTimeSeconds();
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const bool bJumpAttack = Character && Character->GetCharacterMovement()->IsFalling();
	if (const URogue10mAttackSkillData* ChargedSkill = ResolveChargedAttackSkill(bPrimaryAttack, bJumpAttack))
	{
		if (IsAttackSkillUnlocked(ChargedSkill))
		{
			StartChargeEffect(*ChargedSkill);
		}
	}
	AddCombatLog(
		FString::Printf(TEXT("%s 입력: 차징 확인 시작"), bPrimaryAttack ? TEXT("좌클릭") : TEXT("우클릭")),
		FLinearColor(0.72f, 0.84f, 1.0f, 1.0f));
}

bool URogue10mCombatComponent::IsAnyAttackInputHeld() const
{
	const ARogue10mCharacter* OwnerCharacter = GetOwnerCharacter();
	const URogue10mBasicBrawlerComponent* Basic = OwnerCharacter ? OwnerCharacter->GetBasicBrawlerComponent() : nullptr;
	return (OwnerCharacter && OwnerCharacter->HasPendingMouseAttackInput())
		|| LeftAttackPressedTime >= 0.0f || RightAttackPressedTime >= 0.0f || HeldBoundSkill.IsValid()
		|| (Basic && Basic->HasHeldInput());
}

void URogue10mCombatComponent::CancelHeldAttackInput()
{
	++AdvanceCancellationEpoch;
	if (TechniqueMotion) { TechniqueMotion->CancelAdvance(); }
	if (auto* Character = GetOwnerCharacter()) { Character->CancelPendingMouseAttackInput(); }
	LeftAttackPressedTime = RightAttackPressedTime = -1.0f;
	CancelBoundSkillInput();
	StopChargeEffect();
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent())
		{ Basic->CancelHeldInput(); }
	}
}

void URogue10mCombatComponent::InterruptAttacksOnHit()
{
	++DamageInterruptionEpoch;
	if (bInterruptingAttacks) { return; }
	TGuardValue<bool> InterruptGuard(bInterruptingAttacks, true);
	TGuardValue<bool> AcceptingGuard(bAcceptingAttack, true);
	struct FPlayback { TWeakObjectPtr<UAnimInstance> Anim; TWeakObjectPtr<UAnimMontage> Montage; int32 Id; };
	TArray<FPlayback> Playbacks;
	const auto RememberPlayback = [&Playbacks](TWeakObjectPtr<UAnimInstance> Anim, TWeakObjectPtr<UAnimMontage> Montage, int32 Id)
	{
		if (Anim.IsValid() && Montage.IsValid() && Id != INDEX_NONE
			&& !Playbacks.ContainsByPredicate([&](const FPlayback& Item) { return Item.Anim == Anim && Item.Id == Id; }))
		{ Playbacks.Add({Anim, Montage, Id}); }
	};
	RememberPlayback(LastAcceptedAnimInstance, LastAcceptedMontage, LastAcceptedMontageId);
	for (const auto& Pair : ActiveAttackExecutions)
	{
		RememberPlayback(Pair.Value.AdvanceAnimInstance, Pair.Value.AdvanceMontage, Pair.Value.AdvanceMontageInstanceId);
	}
	CancelCombatVisuals(false); // Combat owns exact-ID playback stops for hit interruption.
	TArray<uint32> Remaining;
	ActiveAttackExecutions.GetKeys(Remaining);
	for (const uint32 Id : Remaining) { FinishAttackHitSequence(Id); } // Includes remaining pulses after a first hit.
	PendingAbilityAttackSkill.Reset(); bPendingAbilityComboAttack = false;
	ResetComboWindow(); AttackActionEndTime = 0.0;
	if (const UWorld* World = GetWorld(); World && AttackCooldownEndTime > World->GetTimeSeconds())
	{ AttackCooldownStartTime = FMath::Min(AttackCooldownStartTime, static_cast<float>(World->GetTimeSeconds())); }
	for (const auto& Playback : Playbacks)
	{
		if (auto* Anim = Playback.Anim.Get())
		{
			if (auto* Instance = Anim->GetMontageInstanceForID(Playback.Id); Instance && Instance->Montage == Playback.Montage.Get())
			{ Instance->Stop(FAlphaBlend(0.08f)); }
		}
	}
}

void URogue10mCombatComponent::HandleAttackReleased(bool bPrimaryAttack)
{
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent(); Basic && Basic->IsBasicBrawlerActive())
		{
			Basic->HandleAttackReleased(bPrimaryAttack);
			return;
		}
	}
	StopChargeEffect();
	if (!CanUseCombatInput() || !GetWorld())
	{
		return;
	}

	float& PressedTime = bPrimaryAttack ? LeftAttackPressedTime : RightAttackPressedTime;
	if (PressedTime < 0.0f)
	{
		return;
	}

	const float HeldTime = GetWorld()->GetTimeSeconds() - PressedTime;
	PressedTime = -1.0f;
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const bool bJumpAttack = Character && Character->GetCharacterMovement()->IsFalling();
	const URogue10mAttackSkillData* ChargedSkill = ResolveChargedAttackSkill(bPrimaryAttack, bJumpAttack);
	const float RequiredCharge = ChargedSkill ? ChargedSkill->ChargeSeconds : DefaultChargeThreshold;
	ExecuteCombatAttack(bPrimaryAttack, ChargedSkill && HeldTime >= RequiredCharge);
}

void URogue10mCombatComponent::CancelCombatVisuals(bool bStopBasicMontage)
{
	++AdvanceCancellationEpoch;
	if (MeleeVfx) { MeleeVfx->EndTrail(0); }
	if (TechniqueMotion) { TechniqueMotion->CancelAdvance(); }
	if (auto* Character = GetOwnerCharacter()) { Character->CancelPendingMouseAttackInput(); }
	LastAcceptedAttackSkill.Reset();
	LastAcceptedAnimInstance.Reset(); LastAcceptedMontage.Reset(); LastAcceptedMontageId = INDEX_NONE;
	CancelBoundSkillInput();
	CancelPendingAttackHits();
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent())
		{
			Basic->CancelInput(bStopBasicMontage);
		}
	}
	LeftAttackPressedTime = -1.0f;
	RightAttackPressedTime = -1.0f;
	StopChargeEffect();
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mCharacterAnimationComponent* Animation = Character->GetCharacterAnimationComponent())
		{
			Animation->ResetFighterCombatStance();
		}
	}
}

bool URogue10mCombatComponent::ExecutePendingAttackSkillFromAbility()
{
	const URogue10mAttackSkillData* SkillData = PendingAbilityAttackSkill.Get();
	if (!SkillData)
	{
		AddCombatLog(TEXT("GAS 공격 실행 실패: 예약된 공격 Data Asset이 없습니다."), FLinearColor(1.0f, 0.35f, 0.25f, 1.0f));
		return false;
	}

	TGuardValue<bool> Guard(bExecutingAttackFromAbility, true);
	const bool bExecuted = ExecuteAttackSkill(*SkillData, bPendingAbilityComboAttack);
	bLastAbilityAttackExecuted = bExecuted;
	PendingAbilityAttackSkill.Reset();
	bPendingAbilityComboAttack = false;
	return bExecuted;
}

bool URogue10mCombatComponent::ActivateQuickSlot(int32 SlotNumber)
{
	// Compatibility for DoAttack/old HUD callers. Never bypass the six-slot charge/input policy.
	if (SlotNumber != 1 || !CanUseCombatInput() || !GetWorld()) { return false; }
	const auto* Skill = GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
	if (!Skill || !IsAttackSkillUnlocked(Skill) || IsAttackOnCooldown(GetWorld()->GetTimeSeconds())
		|| IsComboSequenceActive(GetWorld()->GetTimeSeconds()) || !CanPayResourceCosts(*Skill)) { return false; }
	const auto* Character = GetOwnerCharacter();
	if (Character && Character->GetAbilitySystemComponent() && (Skill->GameplayAbilityClass || DefaultAttackAbilityClass))
	{
		bLastAbilityAttackExecuted = false;
		return TryActivateAttackAbility(*Skill, false) && bLastAbilityAttackExecuted;
	}
	return ExecuteAttackSkill(*Skill, false);
}

void URogue10mCombatComponent::UnlockAttackSkill(URogue10mAttackSkillData* SkillData)
{
	if (const auto* State = BoundProgressionState.Get())
	{
		const auto* Tree = State->GetSkillTreeProgressionComponent();
		if (Tree && Tree->IsSkillManaged(SkillData) && !Tree->IsSkillGranted(SkillData)) { return; }
	}
	if (!SkillData || UnlockedAttackSkillNames.Contains(SkillData->GetFName()))
	{
		return;
	}

	UnlockedAttackSkillNames.Add(SkillData->GetFName());
	UnlockedSkillAssets.AddUnique(SkillData);
	OnSkillTreeChanged.Broadcast();
}

bool URogue10mCombatComponent::CanUnlockActiveTreeForTesting() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	return Character && Character->HasAuthority() && Character->IsLocallyControlled()
		&& !Character->IsDead() && !GetActiveSkillTreeSkills().IsEmpty();
#endif
}

int32 URogue10mCombatComponent::UnlockAllActiveTreeForTesting()
{
	if (!CanUnlockActiveTreeForTesting()) { return 0; }
	int32 Added = 0;
	for (const URogue10mAttackSkillData* Skill : GetActiveSkillTreeSkills())
	{
		if (Skill && !IsAttackSkillUnlocked(Skill))
		{
			TestUnlockedAttackSkillNames.Add(Skill->GetFName());
			++Added;
		}
	}
	if (Added > 0) { OnSkillTreeChanged.Broadcast(); }
	return Added;
}

int32 URogue10mCombatComponent::GetEquippedLoadoutSkillCount() const
{
	int32 Count = 0;
	for (const auto& Pair : EquippedSkillBindings) { if (URogue10mAttackSkillData::IsLoadoutSlot(Pair.Key) && GetEquippedSkill(Pair.Key)) { ++Count; } }
	return Count;
}
bool URogue10mCombatComponent::CanEquipSkillInSlot(const URogue10mAttackSkillData* Skill, ERogue10mAttackInputSlot Slot) const
{
	const auto* Character = GetOwnerCharacter();
	const auto* Profile = FindActiveWeaponProfile();
	const auto* Basic = Profile ? Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary) : nullptr;
	return Character && Character->GetEquippedWeaponType() != ERogue10mWeaponType::Unarmed
		&& Skill && Skill != Basic && Skill->IsAllowedInLoadoutSlot(Slot) && IsSelectableActiveTechnique(Skill) && IsAttackSkillUnlocked(Skill);
}

bool URogue10mCombatComponent::IsTechniqueMovementActive() const
{
	return TechniqueMotion && TechniqueMotion->IsAdvancing();
}

bool URogue10mCombatComponent::HasActiveAuthoredContacts() const
{
	for (const auto& Pair : ActiveAttackExecutions) { if (Pair.Value.bAuthoredContactSequence) { return true; } }
	return false;
}

bool URogue10mCombatComponent::CanContinueAuthoredContacts(const FRogue10mActiveAttackExecution& Execution) const
{
	const auto* Character = GetOwnerCharacter();
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Skill = Execution.SkillData.Get();
	auto* Anim = Execution.AdvanceAnimInstance.Get();
	const auto* Instance = Anim ? Anim->GetMontageInstanceForID(Execution.AdvanceMontageInstanceId) : nullptr;
	return Skill && !Skill->bAdvanceBeforeHit && !Skill->bEnableCombo && Skill->AttackShape != ERogue10mAttackShape::Projectile
		&& Execution.AdvanceMontage.IsValid()
		&& Skill->HitMode == Execution.AcceptedHitMode && Skill->HitCount == Execution.AuthoredTimeline.TotalCount()
		&& Skill->AttackMontage == Execution.AdvanceMontage.Get()
		&& Execution.AdvanceEpoch == AdvanceCancellationEpoch && Character && Character->HasAuthority()
		&& (!Execution.AuthoredTimeline.HasRecoveryOverride()
			|| (GetWorld() && GetWorld()->GetTimeSeconds() <= Execution.AuthoredTimeline.RecoveryDeadline() + FRogue10mAuthoredContactTimeline::ClockTolerance))
		&& Character->GetController() && Character->IsThirdPersonGameplayActive() && !Character->IsDodging()
		&& Character->GetEquippedWeaponType() == Execution.AcceptedWeaponType
		&& Movement && Movement->MovementMode == MOVE_Walking && CanUseCombatInput()
		&& Instance && Instance->GetPlayRate() == Execution.AcceptedMontagePlayRate
		&& Skill->AttackMontage->RateScale == Execution.AcceptedMontageRateScale
		&& (!Skill->AttackMontage->bEnableAutoBlendOut || Skill->AttackMontage->BlendOutTriggerTime == 0.0f)
		&& URogue10mTechniqueMotionComponent::DoesMontageContinue(*Character, Execution.AdvanceAnimInstance.Get(),
			Execution.AdvanceMontage.Get(), Execution.AdvanceMontageInstanceId);
}

void URogue10mCombatComponent::ScheduleNextAuthoredContact(uint32 ExecutionId)
{
	auto* Execution = ActiveAttackExecutions.Find(ExecutionId);
	if (!Execution || !Execution->bAuthoredContactSequence || !GetWorld()) { return; }
	const double Delay = Execution->AuthoredTimeline.SecondsUntilNext(GetWorld()->GetTimeSeconds());
	if (!FMath::IsFinite(Delay) || !CanContinueAuthoredContacts(*Execution)) { FinishAttackHitSequence(ExecutionId); return; }
	FTimerDelegate Contact;
	const auto Arm = Execution->AuthoredTimeline.ArmNextContact();
	Contact.BindUObject(this, &ThisClass::HandleAuthoredContactTimer, ExecutionId, static_cast<uint64>(Arm.Generation), Arm.ContactNumber);
	// A zero/overdue contact must not clear the UE timer or execute a whole burst inline.
	GetWorld()->GetTimerManager().SetTimer(Execution->TimerHandle, Contact, static_cast<float>(FMath::Max(0.001, Delay)), false);
}

void URogue10mCombatComponent::HandleAuthoredContactTimer(uint32 ExecutionId, uint64 ArmGeneration, int32 ExpectedContact)
{
	auto* Execution = ActiveAttackExecutions.Find(ExecutionId);
	if (!Execution || !Execution->bAuthoredContactSequence || !Execution->AuthoredTimeline.ConsumeArm(ArmGeneration, ExpectedContact)) { return; }
	ExecuteAttackHitPulse(ExecutionId);
}

void URogue10mCombatComponent::ReleaseAuthoredRecoveryAction(const FRogue10mActiveAttackExecution& Execution)
{
	// Restore only the action deadline this execution installed; preserve unrelated ownership.
	if (Execution.AuthoredOwnedActionEndTime > 0.0 && AttackActionEndTime == Execution.AuthoredOwnedActionEndTime)
	{ AttackActionEndTime = Execution.AuthoredPreviousActionEndTime; }
}

void URogue10mCombatComponent::MonitorAuthoredContacts()
{
	TArray<uint32> Ids;
	ActiveAttackExecutions.GetKeys(Ids);
	for (const uint32 Id : Ids)
	{
		const auto* Execution = ActiveAttackExecutions.Find(Id);
		if (Execution && Execution->bAuthoredContactSequence && !CanContinueAuthoredContacts(*Execution))
		{ FinishAttackHitSequence(Id); }
		else if (Execution && Execution->bAuthoredContactSequence && !Execution->AuthoredTimeline.IsActive()
			&& !Execution->AuthoredTimeline.IsRecoveryPending(GetWorld()->GetTimeSeconds()))
		{ FinishAttackHitSequence(Id); }
	}
}

TArray<URogue10mAttackSkillData*> URogue10mCombatComponent::GetSelectableActiveTechniques() const
{
	TArray<URogue10mAttackSkillData*> Result;
	const auto* Profile = FindActiveWeaponProfile();
	if (!Profile || Profile->WeaponType == ERogue10mWeaponType::Unarmed) { return Result; }
	if (Profile->HasExplicitActiveTechniqueCatalog()) { return Profile->GetValidatedActiveTechniqueCatalog(); }
	for (URogue10mAttackSkillData* Skill : Profile->SkillTreeSkills)
	{
		if (IsSelectableActiveTechnique(Skill)) { Result.AddUnique(Skill); }
	}
	return Result;
}

bool URogue10mCombatComponent::IsSelectableActiveTechnique(const URogue10mAttackSkillData* Skill) const
{
	const auto* Profile = FindActiveWeaponProfile();
	return Profile && Profile->WeaponType != ERogue10mWeaponType::Unarmed && Skill
		&& Skill != Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary)
		&& Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique
		&& Skill->AllowedSlotPolicy != ERogue10mSkillSlotPolicy::BasicOnly
		&& Profile->SkillTreeSkills.Contains(Skill) && Profile->CatalogAllowsLoadoutSkill(Skill);
}
void URogue10mCombatComponent::HandleEquippedSkillPressed(ERogue10mAttackInputSlot Slot)
{
	if (Slot == ERogue10mAttackInputSlot::Primary)
	{
		auto* Character = GetOwnerCharacter();
		auto* Basic = Character ? Character->GetBasicBrawlerComponent() : nullptr;
		if (Basic && Basic->IsBasicBrawlerActive()) { HandleAttackPressed(true); }
		else if (CanUseCombatInput()) { ExecuteCombatAttack(true, false); }
		return;
	}
	if (URogue10mAttackSkillData::IsLoadoutSlot(Slot)) { HandleBoundSkillPressed(GetEquippedSkill(Slot)); }
}
void URogue10mCombatComponent::HandleEquippedSkillReleased(ERogue10mAttackInputSlot Slot)
{
	if (Slot == ERogue10mAttackInputSlot::Primary)
	{
		auto* Character = GetOwnerCharacter();
		auto* Basic = Character ? Character->GetBasicBrawlerComponent() : nullptr;
		if (Basic && Basic->IsBasicBrawlerActive()) { HandleAttackReleased(true); }
		return;
	}
	if (URogue10mAttackSkillData::IsLoadoutSlot(Slot)) { HandleBoundSkillReleased(GetEquippedSkill(Slot)); }
}

bool URogue10mCombatComponent::CanUseBoundSkill(const URogue10mAttackSkillData* Skill) const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	if (!Skill || !Character || !Character->IsLocallyControlled() || !CanUseCombatInput() || IsTechniqueMovementActive() || HasActiveAuthoredContacts()
		|| !IsSelectableActiveTechnique(Skill) || !IsAttackSkillUnlocked(Skill)
		|| Skill->SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique || !GetWorld()) { return false; }
	bool bEquipped = false;
	for (const auto& Pair : EquippedSkillBindings)
	{
		bEquipped |= Pair.Value == Skill && URogue10mAttackSkillData::IsLoadoutSlot(Pair.Key)
			&& Skill->IsAllowedInLoadoutSlot(Pair.Key);
	}
	if (!bEquipped || Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed) { return false; }
	const bool bJump = Skill->InputSlot == ERogue10mAttackInputSlot::JumpPrimary
		|| Skill->InputSlot == ERogue10mAttackInputSlot::JumpSpecial;
	return Character->GetCharacterMovement() && bJump == Character->GetCharacterMovement()->IsFalling();
}

bool URogue10mCombatComponent::ActivateTreeSkill(URogue10mAttackSkillData* Skill)
{
	if (!CanUseBoundSkill(Skill)) { return false; }
	const bool bCharged = Skill->RequiresLoadoutCharge();
	if (bCharged && !bBoundChargeReady) { return false; }
	const float Now = GetWorld()->GetTimeSeconds();
	// Check before GAS or presentation changes; a rejected shortcut must not cancel an attack.
	if (IsAttackOnCooldown(Now) || IsComboSequenceActive(Now) || !CanPayResourceCosts(*Skill)) { return false; }
	ARogue10mCharacter* Character = GetOwnerCharacter();
	if (Character->GetAbilitySystemComponent() && (Skill->GameplayAbilityClass || DefaultAttackAbilityClass))
	{
		bLastAbilityAttackExecuted = false;
		return TryActivateAttackAbility(*Skill, false) && bLastAbilityAttackExecuted;
	}
	return ExecuteAttackSkill(*Skill, false);
}

void URogue10mCombatComponent::HandleBoundSkillPressed(URogue10mAttackSkillData* Skill)
{
	if (!CanUseBoundSkill(Skill) || HeldBoundSkill.IsValid()) { return; }
	const bool bCharged = Skill->RequiresLoadoutCharge();
	if (!bCharged) { ActivateTreeSkill(Skill); return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (IsAttackOnCooldown(Now) || IsComboSequenceActive(Now) || !CanPayResourceCosts(*Skill)) { return; }
	HeldBoundSkill = Skill;
	BoundSkillPressedTime = Now;
	StartChargeEffect(*Skill);
	GetWorld()->GetTimerManager().SetTimer(BoundSkillMonitorTimer, this,
		&URogue10mCombatComponent::MonitorBoundSkillInput, 0.02f, true);
}

void URogue10mCombatComponent::HandleBoundSkillReleased(URogue10mAttackSkillData* Skill)
{
	if (!Skill || HeldBoundSkill.Get() != Skill) { return; }
	const bool bReady = CanUseBoundSkill(Skill) && BoundSkillPressedTime >= 0.f
		&& GetWorld()->GetTimeSeconds() - BoundSkillPressedTime >= FMath::Max(0.05f, Skill->ChargeSeconds);
	CancelBoundSkillInput();
	if (bReady) { TGuardValue<bool> Guard(bBoundChargeReady, true); ActivateTreeSkill(Skill); }
}

void URogue10mCombatComponent::CancelBoundSkillInput()
{
	if (HeldBoundSkill.IsValid()) { StopChargeEffect(); }
	HeldBoundSkill.Reset();
	BoundSkillPressedTime = -1.f;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(BoundSkillMonitorTimer); }
}

void URogue10mCombatComponent::MonitorBoundSkillInput()
{
	if (!CanUseBoundSkill(HeldBoundSkill.Get())) { CancelBoundSkillInput(); }
}

bool URogue10mCombatComponent::IsAttackSkillUnlocked(const URogue10mAttackSkillData* SkillData) const
{
	if (!SkillData) { return false; }
#if !UE_BUILD_SHIPPING
	if (TestUnlockedAttackSkillNames.Contains(SkillData->GetFName())) { return true; }
#endif
	return IsSkillDurablyUnlocked(SkillData);
}

bool URogue10mCombatComponent::IsSkillDurablyUnlocked(const URogue10mAttackSkillData* SkillData) const
{
	if (!SkillData) { return false; }
	const auto* Character = GetOwnerCharacter();
	const auto* Profile = FindActiveWeaponProfile();
	// Newbie's fixed punch is always usable; it does not grant the weapon's mastery or identity.
	if (Character && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed && Profile
		&& Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary) == SkillData) { return true; }
	const auto* State = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const auto* Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
	if (Tree && Tree->IsSkillManaged(SkillData)) { return Tree->IsSkillGranted(SkillData); }
	return UnlockedAttackSkillNames.Contains(SkillData->GetFName());
}

bool URogue10mCombatComponent::IsSkillInActiveTree(const URogue10mAttackSkillData* SkillData) const
{
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	return SkillData && Profile && Profile->SkillTreeSkills.Contains(SkillData);
}

TArray<URogue10mAttackSkillData*> URogue10mCombatComponent::GetUnlockedWeaponSkills() const
{
	TArray<URogue10mAttackSkillData*> Result;
	for (URogue10mAttackSkillData* Skill : GetActiveSkillTreeSkills())
	{
		if (IsAttackSkillUnlocked(Skill))
		{
			Result.Add(Skill);
		}
	}
	return Result;
}


bool URogue10mCombatComponent::AssignSkillToInputSlot(
	URogue10mAttackSkillData* SkillData, ERogue10mAttackInputSlot InputSlot)
{
	if (!CanEquipSkillInSlot(SkillData, InputSlot))
	{
		return false;
	}

	// A preview may have become genuinely learned since it was equipped. Preserve
	// that real choice before another temporary preview replaces the slot.
	for (const auto& Pair : EquippedSkillBindings)
	{
		if (Pair.Value && IsSkillDurablyUnlocked(Pair.Value)) { DurableSkillBindings.Add(Pair.Key, Pair.Value); }
	}
	if (IsSkillDurablyUnlocked(SkillData))
	{
		for (auto It = DurableSkillBindings.CreateIterator(); It; ++It)
		{
			if (It.Value() == SkillData) { It.RemoveCurrent(); }
		}
		DurableSkillBindings.Add(InputSlot, SkillData);
	}

	for (TPair<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>>& Pair : EquippedSkillBindings)
	{
		if (Pair.Value == SkillData)
		{
			Pair.Value = nullptr;
		}
	}
	CancelHeldAttackInput();
	EquippedSkillBindings.Add(InputSlot, SkillData);
	RememberActiveWeaponPreset();
	OnSkillTreeChanged.Broadcast();
	return true;
}

bool URogue10mCombatComponent::UnassignSkillFromInputSlot(ERogue10mAttackInputSlot InputSlot)
{
	if (!URogue10mAttackSkillData::IsLoadoutSlot(InputSlot)) { return false; }
	CancelHeldAttackInput();
	const bool bEquippedChanged = EquippedSkillBindings.Remove(InputSlot) > 0;
	// Explicit clearing is a real user choice even while a test preview occupies
	// the slot; do not restore a remembered/default skill over that empty choice.
	const bool bDurableChanged = DurableSkillBindings.Remove(InputSlot) > 0;
	const bool bChanged = bEquippedChanged || bDurableChanged;
	if (bChanged) { RememberActiveWeaponPreset(); OnSkillTreeChanged.Broadcast(); }
	return bChanged;
}

URogue10mAttackSkillData* URogue10mCombatComponent::GetEquippedSkill(ERogue10mAttackInputSlot InputSlot) const
{
	const auto* Character = GetOwnerCharacter();
	if (Character && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
		&& InputSlot != ERogue10mAttackInputSlot::Primary) { return nullptr; }
	if (const TObjectPtr<URogue10mAttackSkillData>* Skill = EquippedSkillBindings.Find(InputSlot))
	{
		const auto* Profile = FindActiveWeaponProfile();
		if (InputSlot != ERogue10mAttackInputSlot::Primary && Profile && !Profile->CatalogAllowsLoadoutSkill(Skill->Get())) { return nullptr; }
		return Skill->Get();
	}
	if (FindActiveWeaponProfile())
	{
		return nullptr;
	}

	switch (InputSlot)
	{
	case ERogue10mAttackInputSlot::Primary: return PrimaryAttackSkill;
	case ERogue10mAttackInputSlot::Special: return SpecialAttackSkill;
	case ERogue10mAttackInputSlot::JumpPrimary: return JumpPrimaryAttackSkill;
	case ERogue10mAttackInputSlot::JumpSpecial: return JumpSpecialAttackSkill;
	case ERogue10mAttackInputSlot::ChargedPrimary: return ChargedPrimaryAttackSkill;
	case ERogue10mAttackInputSlot::ChargedSpecial: return ChargedSpecialAttackSkill;
	default: return nullptr;
	}
}

TArray<URogue10mAttackSkillData*> URogue10mCombatComponent::GetActiveSkillTreeSkills() const
{
	TArray<URogue10mAttackSkillData*> Result;
	if (const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile())
	{
		for (URogue10mAttackSkillData* Skill : Profile->SkillTreeSkills)
		{
			if (Skill)
			{
				Result.AddUnique(Skill);
			}
		}
	}
	return Result;
}

ERogue10mWeaponType URogue10mCombatComponent::GetActiveSkillTreeWeaponType() const
{
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	return Profile ? Profile->WeaponType : ERogue10mWeaponType::Unarmed;
}

FText URogue10mCombatComponent::GetActiveSkillTreeDisplayName() const
{
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	return Profile ? Profile->DisplayName : FText::FromString(TEXT("무공 수련"));
}

bool URogue10mCombatComponent::IsSkillAvailableToUnlock(const URogue10mAttackSkillData* SkillData) const
{
	if (!SkillData || !IsSkillInActiveTree(SkillData) || IsAttackSkillUnlocked(SkillData))
	{
		return false;
	}
	const auto* Character = GetOwnerCharacter();
	const auto* State = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const auto* Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
	// Managed techniques use the new source-of-truth ledger, never old usage/kill conditions.
	if (Tree && Tree->IsSkillManaged(SkillData)) { return false; }

	for (const URogue10mAttackSkillData* Prerequisite : SkillData->PrerequisiteSkills)
	{
		if (!IsAttackSkillUnlocked(Prerequisite))
		{
			return false;
		}
	}

	for (const FRogue10mSkillUnlockCondition& Condition : SkillData->UnlockConditions)
	{
		if (!IsSkillUnlockConditionComplete(Condition))
		{
			return false;
		}
	}
	return true;
}

int32 URogue10mCombatComponent::GetSkillUnlockConditionCurrent(
	const FRogue10mSkillUnlockCondition& Condition) const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const ARogue10mPlayerState* ProgressionState = Character
		? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	if (!ProgressionState)
	{
		return 0;
	}

	if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount)
	{
		return Condition.RequiredSkill
			? ProgressionState->GetSkillUseCount(Condition.RequiredSkill->GetFName()) : 0;
	}
	return ProgressionState->GetMonsterDefeatCount(Condition.RequiredMonsterId);
}

bool URogue10mCombatComponent::IsSkillUnlockConditionComplete(
	const FRogue10mSkillUnlockCondition& Condition) const
{
	return GetSkillUnlockConditionCurrent(Condition) >= FMath::Max(1, Condition.RequiredCount);
}

float URogue10mCombatComponent::GetSkillUnlockProgress(const URogue10mAttackSkillData* SkillData) const
{
	if (!SkillData)
	{
		return 0.0f;
	}
	if (IsAttackSkillUnlocked(SkillData))
	{
		return 1.0f;
	}

	float ProgressTotal = 0.0f;
	int32 ProgressPartCount = 0;
	for (const URogue10mAttackSkillData* Prerequisite : SkillData->PrerequisiteSkills)
	{
		ProgressTotal += IsAttackSkillUnlocked(Prerequisite) ? 1.0f : 0.0f;
		++ProgressPartCount;
	}
	for (const FRogue10mSkillUnlockCondition& Condition : SkillData->UnlockConditions)
	{
		const int32 Required = FMath::Max(1, Condition.RequiredCount);
		ProgressTotal += FMath::Clamp(
			static_cast<float>(GetSkillUnlockConditionCurrent(Condition)) / static_cast<float>(Required),
			0.0f, 1.0f);
		++ProgressPartCount;
	}
	return ProgressPartCount > 0 ? ProgressTotal / static_cast<float>(ProgressPartCount) : 0.0f;
}

float URogue10mCombatComponent::GetUnlockedPassiveDamageReduction() const
{
	float TotalReduction = 0.0f;
	for (const URogue10mAttackSkillData* Skill : GetActiveSkillTreeSkills())
	{
		if (Skill && IsAttackSkillUnlocked(Skill)
			&& Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique)
		{
			TotalReduction += Skill->PassiveDamageReduction;
		}
	}
	return FMath::Clamp(TotalReduction, 0.0f, 0.8f);
}

void URogue10mCombatComponent::EvaluateActiveSkillUnlocks()
{
	bool bUnlockedSkill = false;
	do
	{
		bUnlockedSkill = false;
		for (URogue10mAttackSkillData* Skill : GetActiveSkillTreeSkills())
		{
			if (IsSkillAvailableToUnlock(Skill))
			{
				UnlockAttackSkill(Skill);
				AddCombatLog(
					FString::Printf(TEXT("무공 해금: %s"), *Skill->SkillName.ToString()),
					FLinearColor(0.35f, 0.92f, 0.62f, 1.0f));
				bUnlockedSkill = true;
			}
		}
	}
	while (bUnlockedSkill);
}

const URogue10mDodgeSkillDataAsset* URogue10mCombatComponent::GetActiveDodgeSkill() const
{
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	return Profile ? Profile->DefaultDodgeSkill : nullptr;
}

void URogue10mCombatComponent::HandleEquippedWeaponChanged(bool bApplySwapCooldown)
{
	if (auto* Identity = GetOwner() ? GetOwner()->FindComponentByClass<URogue10mWeaponIdentityComponent>() : nullptr) { Identity->DeactivateIdentity(); }
	const auto PreviousCooldownSource = AttackCooldownSourceSkill;
	CancelWeaponTransitionState();
	if (bApplySwapCooldown && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const float SwapFloorEnd = Now + 0.5f;
		// A combo can reserve a cooldown that starts in the future. Once its
		// combo is cancelled by a swap, block immediately through that original
		// deadline rather than allowing an attack before the reserved start.
		if (!IsAttackOnCooldown(Now) || AttackCooldownEndTime < SwapFloorEnd)
		{
			AttackCooldownEndTime = FMath::Max(AttackCooldownEndTime, SwapFloorEnd);
			AttackCooldownStartTime = Now;
			AttackCooldownDuration = AttackCooldownEndTime - Now;
		}
		AttackCooldownSourceSkill = PreviousCooldownSource;
	}
	ApplyActiveWeaponProfile();
	HandleHybridSkillTreeChanged();
}

const URogue10mWeaponSkillProfileDataAsset* URogue10mCombatComponent::FindActiveWeaponProfile() const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const ERogue10mWeaponType WeaponType = Character
		? Character->GetEquippedWeaponType()
		: ERogue10mWeaponType::Unarmed;

	for (const URogue10mWeaponSkillProfileDataAsset* Profile : WeaponSkillProfiles)
	{
		if (Profile && Profile->WeaponType == WeaponType)
		{
			return Profile;
		}
	}
	return nullptr;
}

void URogue10mCombatComponent::ApplyCharacterData()
{
	if (CharacterData && !CharacterData->WeaponSkillProfiles.IsEmpty())
	{
		WeaponSkillProfiles = CharacterData->WeaponSkillProfiles;
		if (ARogue10mCharacter* Character = GetOwnerCharacter())
		{
			// A configured default must not replace the weapon already owned by the equipment slot.
			ERogue10mWeaponType EffectiveWeapon = ERogue10mWeaponType::Unarmed;
			if (const URogue10mInventoryComponent* Inventory = Character->GetInventoryComponent())
			{
				for (const FRogue10mInventorySlot& Slot : Inventory->GetLeftEquipmentSlots())
				{
					if (Slot.SlotType == ERogue10mInventorySlotType::MainWeapon && Slot.bHasEquippedItem
						&& Slot.EquippedItem.bOccupied)
					{
						EffectiveWeapon = Slot.EquippedItem.WeaponType;
						break;
					}
				}
			}
			Character->RestoreEquippedWeaponType(EffectiveWeapon);
		}
	}
}
TMap<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>> URogue10mCombatComponent::BuildDurableSkillBindings() const
{
	// The character may already expose the incoming weapon while we remember outgoing choices.
	const URogue10mWeaponSkillProfileDataAsset* AppliedProfile = nullptr;
	for (const URogue10mWeaponSkillProfileDataAsset* Profile : WeaponSkillProfiles) { if (Profile && Profile->WeaponType == AppliedProfileWeaponType) { AppliedProfile = Profile; break; } }
	const auto Allowed = [AppliedProfile](ERogue10mAttackInputSlot Slot, const URogue10mAttackSkillData* Skill)
	{ return Slot == ERogue10mAttackInputSlot::Primary || !AppliedProfile || AppliedProfile->CatalogAllowsLoadoutSkill(Skill); };
	auto Choices = DurableSkillBindings;
	for (auto It = Choices.CreateIterator(); It; ++It)
	{
		if (!It.Value() || !IsSkillDurablyUnlocked(It.Value()) || !Allowed(It.Key(), It.Value())) { It.RemoveCurrent(); }
	}
	for (const auto& Pair : EquippedSkillBindings)
	{
		if (!Pair.Value || !IsSkillDurablyUnlocked(Pair.Value) || !Allowed(Pair.Key, Pair.Value)) { continue; }
		for (auto It = Choices.CreateIterator(); It; ++It) { if (It.Value() == Pair.Value) { It.RemoveCurrent(); } }
		Choices.Add(Pair.Key, Pair.Value);
	}
	return Choices;
}
void URogue10mCombatComponent::RememberActiveWeaponPreset()
{
	if (!bWeaponPresetApplied) { return; }
	DurableSkillBindings = BuildDurableSkillBindings();
	auto& Preset = WeaponSkillPresets.FindOrAdd(AppliedProfileWeaponType);
	Preset.Slots.Reset();
	for (const auto& Pair : DurableSkillBindings)
	{
		if (Pair.Value && URogue10mAttackSkillData::IsLoadoutSlot(Pair.Key) && Pair.Value->IsAllowedInLoadoutSlot(Pair.Key)
			&& IsSkillDurablyUnlocked(Pair.Value)) { Preset.Slots.Add(Pair.Key, Pair.Value); }
	}
}
void URogue10mCombatComponent::ApplyActiveWeaponProfile()
{
	RememberActiveWeaponPreset();
	bWeaponPresetApplied = false;
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mFirstPersonPresentationComponent* Presentation = Character->GetFirstPersonPresentationComponent())
		{
			Presentation->RefreshPresentation();
		}
	}
	EquippedSkillBindings.Reset();
	DurableSkillBindings.Reset();
	AppliedProfileWeaponType = ERogue10mWeaponType::Unarmed;
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	if (!Profile)
	{
		if (ARogue10mCharacter* Character = GetOwnerCharacter())
		{
			Character->SetWeaponJumpAllowance(1);
			UE_LOG(
				LogRogue10m, Warning,
				TEXT("%s 장착 무기 %s에 연결된 스킬 프로필이 없습니다."),
				*GetNameSafe(GetOwner()),
				*UEnum::GetValueAsString(Character->GetEquippedWeaponType()));
		}
		return;
	}

	AppliedProfileWeaponType = Profile->WeaponType;
	bWeaponPresetApplied = true;
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		Character->SetWeaponJumpAllowance(Profile->MaxJumpCount);
	}
	for (const TPair<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>>& Pair
		: Profile->DefaultSkillBindings)
	{
		const auto* Tree = BoundProgressionState.IsValid() ? BoundProgressionState->GetSkillTreeProgressionComponent() : nullptr;
		const auto TargetSlot = Pair.Key == ERogue10mAttackInputSlot::ChargedPrimary
			&& !Profile->DefaultSkillBindings.Contains(ERogue10mAttackInputSlot::ChargedSpecial)
			? ERogue10mAttackInputSlot::ChargedSpecial : Pair.Key;
		const bool bFixedBasic = TargetSlot == ERogue10mAttackInputSlot::Primary;
		const bool bAllowed = bFixedBasic || (Profile->WeaponType != ERogue10mWeaponType::Unarmed && Pair.Value
			&& Pair.Value->IsAllowedInLoadoutSlot(TargetSlot) && Profile->CatalogAllowsLoadoutSkill(Pair.Value));
		if (Pair.Value && bAllowed && ((bFixedBasic && Profile->WeaponType == ERogue10mWeaponType::Unarmed)
			|| !Tree || !Tree->IsSkillManaged(Pair.Value) || Tree->IsSkillGranted(Pair.Value)))
		{
			EquippedSkillBindings.Add(TargetSlot, Pair.Value);
			DurableSkillBindings.Add(TargetSlot, Pair.Value);
			UnlockedAttackSkillNames.Add(Pair.Value->GetFName());
		}
	}
	for (URogue10mAttackSkillData* Skill : Profile->InitiallyUnlockedSkills)
	{
		UnlockAttackSkill(Skill);
	}
	EvaluateActiveSkillUnlocks();
	if (const auto* Preset = WeaponSkillPresets.Find(Profile->WeaponType))
	{
		for (auto It = EquippedSkillBindings.CreateIterator(); It; ++It) { if (URogue10mAttackSkillData::IsLoadoutSlot(It.Key())) { It.RemoveCurrent(); } }
		for (auto It = DurableSkillBindings.CreateIterator(); It; ++It) { if (URogue10mAttackSkillData::IsLoadoutSlot(It.Key())) { It.RemoveCurrent(); } }
		for (const auto& Pair : Preset->Slots)
		{
			if (CanEquipSkillInSlot(Pair.Value, Pair.Key)) { EquippedSkillBindings.Add(Pair.Key, Pair.Value); DurableSkillBindings.Add(Pair.Key, Pair.Value); }
		}
	}
	OnSkillTreeChanged.Broadcast();
	UE_LOG(
		LogRogue10m, Log,
		TEXT("%s 장착 무기 스킬트리 활성화: %s / %s / 스킬 %d개"),
		*GetNameSafe(GetOwner()),
		*UEnum::GetValueAsString(Profile->WeaponType),
		*Profile->DisplayName.ToString(),
		Profile->SkillTreeSkills.Num());
}

void URogue10mCombatComponent::BindProgressionState()
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	ARogue10mPlayerState* ProgressionState = Character
		? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	if (BoundProgressionState.Get() == ProgressionState)
	{
		return;
	}
	if (ARogue10mPlayerState* PreviousState = BoundProgressionState.Get())
	{
		PreviousState->OnMartialArtsProgressChanged.RemoveDynamic(
			this, &URogue10mCombatComponent::HandleMartialArtsProgressChanged);
		if (auto* Tree = PreviousState->GetSkillTreeProgressionComponent())
		{
			Tree->OnChanged.RemoveDynamic(this, &URogue10mCombatComponent::HandleHybridSkillTreeChanged);
		}
	}
	BoundProgressionState = ProgressionState;
	if (ProgressionState)
	{
		ProgressionState->OnMartialArtsProgressChanged.AddUniqueDynamic(
			this, &URogue10mCombatComponent::HandleMartialArtsProgressChanged);
		if (auto* Tree = ProgressionState->GetSkillTreeProgressionComponent())
		{
			Tree->OnChanged.AddUniqueDynamic(this, &URogue10mCombatComponent::HandleHybridSkillTreeChanged);
			Tree->RefreshAutomaticUnlocks();
		}
	}
}

void URogue10mCombatComponent::HandleHybridSkillTreeChanged()
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	auto* Tree = BoundProgressionState.IsValid() ? BoundProgressionState->GetSkillTreeProgressionComponent() : nullptr;
	if (!Character || !Tree) { return; }
	for (auto* Skill : Tree->GetGrantedSkills(Tree->GetActiveTrack()))
	{
		if (!Skill) { continue; }
		UnlockedAttackSkillNames.Add(Skill->GetFName());
		UnlockedSkillAssets.AddUnique(Skill);
	}
	if (auto* Identity = Character->GetSkillTreeCombatComponent()) { Identity->SynchronizeIdentity(); }
	Character->RefreshCharacterStats(false);
	OnSkillTreeChanged.Broadcast();
}

void URogue10mCombatComponent::HandleMartialArtsProgressChanged()
{
	EvaluateActiveSkillUnlocks();
	OnSkillTreeChanged.Broadcast();
}

void URogue10mCombatComponent::CancelWeaponTransitionState()
{
	AttackActionEndTime = 0.0;
	CancelCombatVisuals();
	ResetComboWindow();
	PendingAbilityAttackSkill.Reset();
	bPendingAbilityComboAttack = false;
	AttackCooldownSourceSkill.Reset();

	if (UWorld* World = GetWorld())
	{
		for (TPair<uint32, FRogue10mActiveAttackExecution>& Pair : ActiveAttackExecutions)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
		}
	}
	ActiveAttackExecutions.Reset();
}

const URogue10mAttackSkillData* URogue10mCombatComponent::ResolveAttackSkill(bool bPrimaryAttack, bool bChargedAttack, bool bJumpAttack) const
{
	if (bChargedAttack)
	{
		return ResolveChargedAttackSkill(bPrimaryAttack, bJumpAttack);
	}
	if (bJumpAttack)
	{
		return GetEquippedSkill(bPrimaryAttack
			? ERogue10mAttackInputSlot::JumpPrimary
			: ERogue10mAttackInputSlot::JumpSpecial);
	}
	return GetEquippedSkill(bPrimaryAttack
		? ERogue10mAttackInputSlot::Primary
		: ERogue10mAttackInputSlot::Special);
}

const URogue10mAttackSkillData* URogue10mCombatComponent::ResolveChargedAttackSkill(bool bPrimaryAttack, bool bJumpAttack) const
{
	return bJumpAttack ? nullptr : GetEquippedSkill(bPrimaryAttack ? ERogue10mAttackInputSlot::ChargedPrimary : ERogue10mAttackInputSlot::ChargedSpecial);
}

const URogue10mAttackSkillData* URogue10mCombatComponent::ResolveComboAttackSkill(bool bPrimaryAttack, bool bJumpAttack) const
{
	if (!bAllowAttackCombo || !GetWorld())
	{
		return nullptr;
	}

	const URogue10mAttackSkillData* Source = ActiveComboSourceSkill.Get();
	if (!Source || !Source->bEnableCombo || !Source->NextComboSkill)
	{
		return nullptr;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < ActiveComboWindowOpenTime || CurrentTime > ActiveComboWindowCloseTime
		|| Source->ComboInputSlot != GetAttackInputSlot(bPrimaryAttack, false, bJumpAttack))
	{
		return nullptr;
	}
	return Source->NextComboSkill;
}

ERogue10mAttackInputSlot URogue10mCombatComponent::GetAttackInputSlot(bool bPrimaryAttack, bool bChargedAttack, bool bJumpAttack) const
{
	if (bChargedAttack)
	{
		return bPrimaryAttack ? ERogue10mAttackInputSlot::ChargedPrimary : ERogue10mAttackInputSlot::ChargedSpecial;
	}
	if (bJumpAttack)
	{
		return bPrimaryAttack ? ERogue10mAttackInputSlot::JumpPrimary : ERogue10mAttackInputSlot::JumpSpecial;
	}
	return bPrimaryAttack ? ERogue10mAttackInputSlot::Primary : ERogue10mAttackInputSlot::Special;
}

bool URogue10mCombatComponent::IsAttackOnCooldown(float CurrentTime) const
{
	return CurrentTime >= AttackCooldownStartTime && CurrentTime < AttackCooldownEndTime;
}

bool URogue10mCombatComponent::IsComboSequenceActive(float CurrentTime) const
{
	return ActiveComboSourceSkill.IsValid() && CurrentTime <= ActiveComboWindowCloseTime;
}

float URogue10mCombatComponent::GetAttackCooldownRemaining() const
{
	return GetWorld() && IsAttackOnCooldown(GetWorld()->GetTimeSeconds())
		? FMath::Max(0.0f, AttackCooldownEndTime - GetWorld()->GetTimeSeconds())
		: 0.0f;
}

const URogue10mAttackSkillData* URogue10mCombatComponent::GetDisplayedAttackSkill() const
{
	const URogue10mAttackSkillData* ComboSource = ActiveComboSourceSkill.Get();
	if (GetWorld() && ComboSource && ComboSource->NextComboSkill
		&& GetWorld()->GetTimeSeconds() <= ActiveComboWindowCloseTime)
	{
		return ComboSource->NextComboSkill;
	}
	return AttackCooldownSourceSkill.IsValid()
		? AttackCooldownSourceSkill.Get()
		: GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
}

ARogue10mCharacter* URogue10mCombatComponent::GetOwnerCharacter() const
{
	return Cast<ARogue10mCharacter>(GetOwner());
}

URogue10mAttributeSet* URogue10mCombatComponent::GetOwnerAttributes() const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	return Character ? Character->GetRogueAttributeSet() : nullptr;
}

float URogue10mCombatComponent::GetAttackSpeedMultiplier() const
{
	const URogue10mAttributeSet* Attributes = GetOwnerAttributes();
	const auto* Identity = GetOwner() ? GetOwner()->FindComponentByClass<URogue10mWeaponIdentityComponent>() : nullptr;
	return (Attributes ? FMath::Clamp(Attributes->GetAttackSpeedMultiplier(), 0.1f, 5.0f) : 1.0f)
		* (Identity ? Identity->GetAttackSpeedMultiplier() : 1.f);
}

bool URogue10mCombatComponent::CanUseCombatInput() const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const ARogue10mPlayerController* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	return Character && !Character->IsDead() && !Character->IsRolling() && !UGameplayStatics::IsGamePaused(this)
		&& (!Character->GetInventoryComponent() || !Character->GetInventoryComponent()->IsEquipmentChangeInProgress())
		&& (!Controller || !Controller->IsAnyBlockingWindowVisible());
}

void URogue10mCombatComponent::ExecuteCombatAttack(bool bPrimaryAttack, bool bChargedAttack)
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const bool bJumpAttack = Character && Character->GetCharacterMovement()->IsFalling();
	const URogue10mAttackSkillData* ComboSkill = bChargedAttack ? nullptr : ResolveComboAttackSkill(bPrimaryAttack, bJumpAttack);

	if (!ComboSkill && !bChargedAttack && GetWorld() && IsComboSequenceActive(GetWorld()->GetTimeSeconds()))
	{
		AddCombatLog(TEXT("콤보 입력 시간이 아닙니다."), FLinearColor(1.0f, 0.76f, 0.36f, 1.0f));
		return;
	}

	const URogue10mAttackSkillData* SkillData = ComboSkill
		? ComboSkill
		: ResolveAttackSkill(bPrimaryAttack, bChargedAttack, bJumpAttack);
	if (!SkillData)
	{
		AddCombatLog(
			FString::Printf(TEXT("%s 공격 잠김: Data Asset이 지정되지 않았습니다."), *GetAttackInputText(bPrimaryAttack, bJumpAttack)),
			FLinearColor(1.0f, 0.35f, 0.25f, 1.0f));
		return;
	}
	if (!IsAttackSkillUnlocked(SkillData))
	{
		AddCombatLog(
			FString::Printf(TEXT("%s 사용 불가: 스킬트리에서 해금되지 않았습니다."), *SkillData->SkillName.ToString()),
			FLinearColor(1.0f, 0.42f, 0.24f, 1.0f));
		ResetComboWindow();
		return;
	}

	AddCombatLog(
		FString::Printf(TEXT("%s 실행: 피해 %.0f"), *SkillData->SkillName.ToString(), SkillData->Damage),
		bPrimaryAttack ? FLinearColor(1.0f, 0.72f, 0.42f, 1.0f) : FLinearColor(0.62f, 0.82f, 1.0f));
	if (!TryActivateAttackAbility(*SkillData, ComboSkill != nullptr))
	{
		ExecuteAttackSkill(*SkillData, ComboSkill != nullptr);
	}
}

bool URogue10mCombatComponent::ExecuteAttackSkill(const URogue10mAttackSkillData& SkillData, bool bComboAttack)
{
	if (bAcceptingAttack || IsTechniqueMovementActive() || HasActiveAuthoredContacts()) { return false; }
	TGuardValue<bool> AcceptingGuard(bAcceptingAttack, true);
	ARogue10mCharacter* Character = GetOwnerCharacter();
	UCameraComponent* Camera = Character ? Character->GetGameplayCameraComponent() : nullptr;
	if (!Character || Character->IsDead() || Character->IsRolling() || !Camera || !GetWorld()
		|| (Character->GetInventoryComponent() && Character->GetInventoryComponent()->IsEquipmentChangeInProgress()))
	{
		return false;
	}
	// A rejected turn/roll must not spend resources, start a cooldown, schedule
	// damage or record skill use. Safe Data Assets retain the original timing.
	if (!URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(SkillData.AttackMontage))
	{
		AddCombatLog(TEXT("시야를 뒤집는 공격 모션은 사용할 수 없습니다."), FLinearColor(1.0f, 0.65f, 0.35f));
		return false;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (!bComboAttack && IsAttackOnCooldown(CurrentTime))
	{
		AddCombatLog(
			FString::Printf(TEXT("%s 재사용 대기 중: %.1f초"), *SkillData.SkillName.ToString(), GetAttackCooldownRemaining()),
			FLinearColor(1.0f, 0.65f, 0.35f, 1.0f));
		return false;
	}
	const float AnimationRate = FMath::Max(0.01f, GetAttackSpeedMultiplier() * SkillData.AnimationPlayRate);
	const TArray<float> ContactTimesAtAccept = SkillData.AuthoredContactTimes;
	const float RecoveryEndAtAccept = SkillData.AuthoredRecoveryEndTime;
	const bool bAuthoredContacts = !ContactTimesAtAccept.IsEmpty();
	const auto HitModeAtAccept = SkillData.HitMode;
	const int32 HitCountAtAccept = SkillData.HitCount;
	const TWeakObjectPtr<UAnimMontage> MontageAtAccept = SkillData.AttackMontage;
	const float MontageScaleAtAccept = MontageAtAccept.IsValid() ? MontageAtAccept->RateScale : 1.0f;
	const float EffectiveContactRate = AnimationRate * MontageScaleAtAccept;
	const uint64 AdvanceEpochAtAccept = AdvanceCancellationEpoch;
	const uint64 DamageEpochAtAccept = DamageInterruptionEpoch;
	const auto WasInterrupted = [this, &SkillData, AdvanceEpochAtAccept, DamageEpochAtAccept,
		bAuthoredContacts, HitModeAtAccept, HitCountAtAccept, MontageAtAccept, MontageScaleAtAccept]()
	{
		return DamageInterruptionEpoch != DamageEpochAtAccept
			|| ((SkillData.bAdvanceBeforeHit || bAuthoredContacts) && AdvanceEpochAtAccept != AdvanceCancellationEpoch)
			|| (bAuthoredContacts && (SkillData.HitMode != HitModeAtAccept || SkillData.HitCount != HitCountAtAccept
				|| SkillData.AttackMontage != MontageAtAccept.Get() || !MontageAtAccept.IsValid()
				|| MontageAtAccept->RateScale != MontageScaleAtAccept));
	};
	const TWeakObjectPtr<ARogue10mPlayerState> AcceptedProgressionState = Character->GetPlayerState<ARogue10mPlayerState>();
	TWeakObjectPtr<UAnimInstance> AcceptedAnimAtPlay;
	TWeakObjectPtr<UAnimMontage> AcceptedMontageAtPlay;
	int32 AcceptedMontageId = INDEX_NONE;
	const auto RecordAcceptedUse = [AcceptedProgressionState, &SkillData]()
	{
		if (auto* State = AcceptedProgressionState.Get()) { State->RecordSkillUse(SkillData.GetFName()); }
	};
	const auto FinalizeInterruptedUse = [this, &RecordAcceptedUse, &AcceptedAnimAtPlay, &AcceptedMontageAtPlay, &AcceptedMontageId](bool bStopMontage)
	{
		if (bStopMontage && AcceptedAnimAtPlay.IsValid())
		{
			if (auto* Instance = AcceptedAnimAtPlay->GetMontageInstanceForID(AcceptedMontageId);
				Instance && Instance->Montage == AcceptedMontageAtPlay.Get())
			{ Instance->Stop(FAlphaBlend(0.08f)); }
		}
		if (const UWorld* World = GetWorld(); World && AttackCooldownEndTime > World->GetTimeSeconds())
		{ AttackCooldownStartTime = FMath::Min(AttackCooldownStartTime, static_cast<float>(World->GetTimeSeconds())); }
		ResetComboWindow(); RecordAcceptedUse();
	};
	if (bAuthoredContacts)
	{
		const auto* Movement = Character->GetCharacterMovement();
		const auto Aim = Character->GetCombatViewTransform().GetRotation();
		const bool bSlot = SkillData.AllowedSlotPolicy == ERogue10mSkillSlotPolicy::Common
			|| SkillData.AllowedSlotPolicy == ERogue10mSkillSlotPolicy::MouseChordOnly
			|| SkillData.AllowedSlotPolicy == ERogue10mSkillSlotPolicy::RightChargeOnly;
		if (!bSlot || !Character->HasAuthority() || !Character->GetController() || !Character->IsThirdPersonGameplayActive()
			|| Character->IsDodging() || !Movement || Movement->MovementMode != MOVE_Walking || !CanUseCombatInput()
			|| !FMath::IsFinite(SkillData.AnimationPlayRate) || SkillData.AnimationPlayRate <= 0.0f
			|| bComboAttack || SkillData.bEnableCombo || SkillData.bAdvanceBeforeHit || SkillData.AttackShape == ERogue10mAttackShape::Projectile
			|| (SkillData.HitMode != ERogue10mAttackHitMode::MultiHit && SkillData.HitMode != ERogue10mAttackHitMode::Continuous)
			|| ContactTimesAtAccept.Num() != SkillData.HitCount
			|| !SkillData.AttackMontage || SkillData.AttackMontage->HasRootMotion() || Aim.ContainsNaN()
			|| !FMath::IsFinite(MontageScaleAtAccept) || MontageScaleAtAccept <= 0.0f
			|| !FMath::IsFinite(SkillData.AttackMontage->GetPlayLength() / EffectiveContactRate)
			|| (SkillData.AttackMontage->bEnableAutoBlendOut && SkillData.AttackMontage->BlendOutTriggerTime != 0.0f)
			|| !FRogue10mAuthoredContactTimeline::Validate(ContactTimesAtAccept.GetData(), ContactTimesAtAccept.Num(), EffectiveContactRate,
				SkillData.AttackMontage->GetPlayLength(), GetWorld()->GetTimeSeconds(), RecoveryEndAtAccept))
		{
			AddCombatLog(TEXT("명시 접촉 시각·타수·지상/몽타주 설정을 확인하세요."), FLinearColor(1.0f, 0.65f, 0.35f));
			return false;
		}
		AcceptedAdvanceRotation = Aim; // Explicit three-cut prototype keeps its accepted aim.
	}
	if (SkillData.bAdvanceBeforeHit)
	{
		const FQuat AimRotation = Character->GetCombatViewTransform().GetRotation();
		const FVector AimDirection = AimRotation.GetForwardVector();
		const bool bSlot = SkillData.AllowedSlotPolicy == ERogue10mSkillSlotPolicy::Common
			|| SkillData.AllowedSlotPolicy == ERogue10mSkillSlotPolicy::RightChargeOnly;
		if (!bSlot || bComboAttack || SkillData.bEnableCombo || SkillData.HitMode != ERogue10mAttackHitMode::Single
			|| SkillData.HitCount != 1 || SkillData.AttackShape == ERogue10mAttackShape::Projectile
			|| !FMath::IsFinite(AnimationRate) || !FMath::IsFinite(SkillData.AdvanceDuration)
			|| SkillData.AdvanceDuration < 0.05f || SkillData.AdvanceDuration > 2.0f
			|| !SkillData.AttackMontage || SkillData.AttackMontage->HasRootMotion()
			|| AimDirection.ContainsNaN() || AimDirection.GetSafeNormal2D().IsNearlyZero()
			|| !URogue10mTechniqueMotionComponent::CanStart(*Character, SkillData.AdvanceDistance,
				SkillData.AdvanceDuration / AnimationRate, SkillData.HitStartDelaySeconds / AnimationRate))
		{
			AddCombatLog(TEXT("전진 기술의 지상/모션/단타 설정을 확인하세요."), FLinearColor(1.0f, 0.65f, 0.35f));
			return false;
		}
		AcceptedAdvanceRotation = AimRotation;
	}
	if (!TryPayResourceCosts(SkillData, bComboAttack))
	{
		return false;
	}
	// Payment is acceptance: even a synchronous cancellation retains exactly one shared cooldown.
	// Resource callbacks may already have reserved a longer weapon-change/other deadline.
	StartSharedAttackCooldown(SkillData, bComboAttack, SkillData.bAdvanceBeforeHit || bAuthoredContacts || DamageEpochAtAccept != DamageInterruptionEpoch);
	// An accepted action may be cancelled synchronously by resource/montage callbacks.
	// It retains its paid cost/use; cancellation must never schedule a new advance afterwards.
	if (WasInterrupted())
	{ FinalizeInterruptedUse(false); return true; }

	if (AcceptedIdentityEffect.bTriggered)
	{
		AddCombatLog(TEXT("아이덴티티 발동"), FLinearColor(0.25f, 0.90f, 0.70f));
	}

	const float Windup = FMath::Max(0.0f, SkillData.HitStartDelaySeconds) / AnimationRate;
	const int32 Pulses = SkillData.HitMode == ERogue10mAttackHitMode::Single ? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
	const float MeleeTail = SkillData.AttackShape != ERogue10mAttackShape::Projectile
		? (Pulses - 1) * FMath::Max(0.01f, SkillData.HitInterval / GetAttackSpeedMultiplier()) : 0.0f;
	float AnimationDuration = SkillData.AttackMontage
		? SkillData.AttackMontage->GetPlayLength() / (bAuthoredContacts ? EffectiveContactRate : AnimationRate) : 0.0f;
	if (!SkillData.AttackMontage && Character->GetBasicBrawlerComponent()
		&& Character->GetBasicBrawlerComponent()->IsBasicBrawlerActive())
	{
		AnimationDuration = FMath::Max(0.05f, SkillData.AttackCooldown / GetAttackSpeedMultiplier());
	}
	if (!bAuthoredContacts || RecoveryEndAtAccept == 0.0f) { AttackActionEndTime = FMath::Max(AttackActionEndTime,
		GetWorld()->GetTimeSeconds() + FMath::Max3(AnimationDuration,
			bAuthoredContacts ? ContactTimesAtAccept.Last() / EffectiveContactRate : Windup + MeleeTail,
			SkillData.bAdvanceBeforeHit ? SkillData.AdvanceDuration / AnimationRate : 0.0f)); }
	const double MontageStartedAt = GetWorld()->GetTimeSeconds();
	AcceptedMontageAtPlay = SkillData.AttackMontage;
	const bool bMontagePlayed = Character->PlayCommonMontage(AcceptedMontageAtPlay.Get(), AnimationRate);
	const auto* PlaybackMesh = Character->GetAnimationPlaybackMesh();
	AcceptedAnimAtPlay = PlaybackMesh ? PlaybackMesh->GetAnimInstance() : nullptr;
	const auto* PlayedInstance = AcceptedAnimAtPlay.IsValid() ? AcceptedAnimAtPlay->GetActiveInstanceForMontage(AcceptedMontageAtPlay.Get()) : nullptr;
	AcceptedMontageId = PlayedInstance ? PlayedInstance->GetInstanceID() : INDEX_NONE;
	if (WasInterrupted())
	{ FinalizeInterruptedUse(true); return true; }
	if ((SkillData.bAdvanceBeforeHit || bAuthoredContacts) && !bMontagePlayed)
	{
		LastAcceptedAttackSkill.Reset();
		LastAcceptedAnimInstance.Reset(); LastAcceptedMontage.Reset(); LastAcceptedMontageId = INDEX_NONE;
		AddCombatLog(TEXT("기술 동작을 재생하지 못해 공격을 취소했습니다."), FLinearColor(1.0f, 0.65f, 0.35f));
		RecordAcceptedUse(); return true;
	}
	if (SkillData.bAdvanceBeforeHit || bAuthoredContacts)
	{
		if (!PlayedInstance || !PlayedInstance->IsActive() || !PlayedInstance->IsPlaying())
		{
			LastAcceptedAttackSkill.Reset();
			LastAcceptedAnimInstance.Reset(); LastAcceptedMontage.Reset(); LastAcceptedMontageId = INDEX_NONE;
			RecordAcceptedUse(); return true;
		}
	}
	LastAcceptedAttackSkill = bMontagePlayed ? &SkillData : nullptr;
	LastAcceptedAnimInstance = AcceptedAnimAtPlay;
	LastAcceptedMontage = AcceptedMontageAtPlay;
	LastAcceptedMontageId = AcceptedMontageId;
	if (URogue10mCharacterAnimationComponent* Animation = Character->GetCharacterAnimationComponent())
	{
		Animation->RequestFighterCombatStance(bAuthoredContacts ? AnimationDuration
			: SkillData.AttackMontage ? SkillData.AttackMontage->GetPlayLength() / AnimationRate : 0.0f);
	}
	SpawnCastEffect(SkillData);

	if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent())
	{
		Basic->NotifySkillAccepted(SkillData);
	}
	if (WasInterrupted())
	{ FinalizeInterruptedUse(true); return true; }
	StartAttackHitSequence(SkillData, AnimationRate, AcceptedAnimAtPlay.Get(), AcceptedMontageId, &ContactTimesAtAccept, MontageStartedAt, RecoveryEndAtAccept);
	if (WasInterrupted()) { FinalizeInterruptedUse(true); return true; }
	OpenComboWindow(SkillData);
	RecordAcceptedUse();
	return true;
}

void URogue10mCombatComponent::StartAttackHitSequence(const URogue10mAttackSkillData& SkillData, float AcceptedAnimationRate,
	UAnimInstance* AcceptedAdvanceAnim, int32 AcceptedMontageId, const TArray<float>* AcceptedContactTimes, double AcceptedMontageStartedAt,
	float AcceptedRecoveryEndTime)
{
	if (!GetWorld())
	{
		return;
	}
	const float Rate = AcceptedAnimationRate > 0.0f ? AcceptedAnimationRate
		: FMath::Max(0.01f, GetAttackSpeedMultiplier() * SkillData.AnimationPlayRate);

	uint32 ExecutionId = NextAttackExecutionId++;
	if (ExecutionId == 0)
	{
		ExecutionId = NextAttackExecutionId++;
	}
	FRogue10mActiveAttackExecution& Execution = ActiveAttackExecutions.Add(ExecutionId);
	Execution.ExecutionId = ExecutionId;
	Execution.SkillData = &SkillData;
	Execution.AdvanceAnimInstance = AcceptedAdvanceAnim;
	Execution.AdvanceMontage = SkillData.AttackMontage;
	Execution.AdvanceMontageInstanceId = AcceptedMontageId;
	ARogue10mCharacter* Character = GetOwnerCharacter();
	Execution.AcceptedWeaponType = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	Execution.AdvanceEpoch = AdvanceCancellationEpoch;
	Execution.MasteryTrack = URogue10mSkillTreeDataAsset::ResolveTrack(Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed);
	Execution.IdentityEffect = AcceptedIdentityEffect;
	if (URogue10mTraitRuntimeComponent* Traits = Character ? Character->GetTraitRuntimeComponent() : nullptr)
	{
		Execution.TraitDodgeBonus = Traits->ConsumeDodgeCounterForAttack();
	}
	const URogue10mBasicBrawlerComponent* Brawler = Character ? Character->FindComponentByClass<URogue10mBasicBrawlerComponent>() : nullptr;
	// Presentation belongs to the accepted attack, not to a weapon equipped between pulses.
	Execution.bBrawlerPresentation = Brawler && Brawler->IsBasicBrawlerActive();
	const auto& ContactTimes = AcceptedContactTimes ? *AcceptedContactTimes : SkillData.AuthoredContactTimes;
	Execution.MeleeVfxTier = Character
		? ResolveMeleeVfxTier(*Character, SkillData, Execution.AcceptedWeaponType, Execution.MasteryTrack) : INDEX_NONE;
	if (Execution.MeleeVfxTier != INDEX_NONE)
	{
		if (!MeleeVfx)
		{
			MeleeVfx = NewObject<URogue10mMeleeVfxComponent>(Character);
			Character->AddInstanceComponent(MeleeVfx);
			MeleeVfx->RegisterComponent();
		}
		const bool bSword = Execution.AcceptedWeaponType == ERogue10mWeaponType::GreatSword;
		const float FirstContact = ContactTimes.IsEmpty() || !SkillData.AttackMontage ? SkillData.HitStartDelaySeconds / Rate
			: ContactTimes[0] / (Rate * SkillData.AttackMontage->RateScale);
		MeleeVfx->BeginTrail(ExecutionId, AcceptedAdvanceAnim, SkillData.AttackMontage, AcceptedMontageId,
			Character->GetMesh(), bSword ? GetMeleeBlade(*Character) : nullptr, SkillData.EffectAttachSocket,
			SkillData.bSpawnEffectOnOffHand ? SkillData.OffHandEffectAttachSocket : NAME_None,
			Execution.MeleeVfxTier, bSword, FirstContact);
	}
	if (!ContactTimes.IsEmpty())
	{
		Execution.bAuthoredContactSequence = true;
		Execution.AcceptedHitMode = SkillData.HitMode;
		Execution.AcceptedCombatRotation = AcceptedAdvanceRotation;
		Execution.AcceptedMontagePlayRate = Rate;
		Execution.AcceptedMontageRateScale = SkillData.AttackMontage ? SkillData.AttackMontage->RateScale : 1.0f;
		const double StartedAt = AcceptedMontageStartedAt >= 0.0 ? AcceptedMontageStartedAt : GetWorld()->GetTimeSeconds();
		const float RecoveryEnd = AcceptedRecoveryEndTime >= 0.0f ? AcceptedRecoveryEndTime : SkillData.AuthoredRecoveryEndTime;
		if (!SkillData.AttackMontage || !Execution.AuthoredTimeline.Begin(ContactTimes.GetData(), ContactTimes.Num(),
			Rate * Execution.AcceptedMontageRateScale, SkillData.AttackMontage->GetPlayLength(), StartedAt, RecoveryEnd)
			|| !CanContinueAuthoredContacts(Execution))
		{ FinishAttackHitSequence(ExecutionId); return; }
		if (Execution.AuthoredTimeline.HasRecoveryOverride())
		{
			Execution.AuthoredPreviousActionEndTime = AttackActionEndTime;
			const double ReleaseAt = Execution.AuthoredTimeline.RecoveryDeadline();
			if (ReleaseAt >= AttackActionEndTime) { AttackActionEndTime = Execution.AuthoredOwnedActionEndTime = ReleaseAt; }
		}
		GetWorld()->GetTimerManager().SetTimer(AuthoredContactMonitorTimer, this, &ThisClass::MonitorAuthoredContacts, 0.01f, true);
		ScheduleNextAuthoredContact(ExecutionId);
		return;
	}
	if (SkillData.bAdvanceBeforeHit)
	{
		Execution.bAdvanceEndpointContact = true;
		Execution.AdvanceEpoch = AdvanceCancellationEpoch;
		Execution.AcceptedCombatRotation = AcceptedAdvanceRotation;
		if (!TechniqueMotion)
		{
			TechniqueMotion = NewObject<URogue10mTechniqueMotionComponent>(GetOwner());
			GetOwner()->AddInstanceComponent(TechniqueMotion);
			TechniqueMotion->RegisterComponent();
			TechniqueMotion->OnAdvanceFinished.AddUObject(this, &ThisClass::HandleTechniqueAdvanceFinished);
		}
		if (!TechniqueMotion->StartAdvance(ExecutionId, Execution.AcceptedCombatRotation.GetForwardVector(),
			SkillData.AdvanceDistance, SkillData.AdvanceDuration / Rate, SkillData.HitStartDelaySeconds / Rate,
			AcceptedAdvanceAnim, SkillData.AttackMontage, AcceptedMontageId))
		{
			FinishAttackHitSequence(ExecutionId);
		}
		return;
	}
	if (SkillData.HitStartDelaySeconds > 0.0f)
	{
		// Capture the same speed used by the accepted montage; later stat changes cannot move its first hit.
		const float Delay = SkillData.HitStartDelaySeconds / Rate;
		const int32 PulseCount = SkillData.HitMode == ERogue10mAttackHitMode::Single
			? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
		FTimerDelegate PulseDelegate;
		PulseDelegate.BindUObject(this, &URogue10mCombatComponent::ExecuteAttackHitPulse, ExecutionId);
		// Keep the existing multi-hit interval contract, including its attack-speed scaling.
		const float Interval = FMath::Max(0.01f, SkillData.HitInterval / GetAttackSpeedMultiplier());
		GetWorld()->GetTimerManager().SetTimer(Execution.TimerHandle, PulseDelegate,
			Interval, PulseCount > 1, FMath::Max(0.001f, Delay));
		return;
	}
	ExecuteAttackHitPulse(ExecutionId);

	const int32 PulseCount = SkillData.HitMode == ERogue10mAttackHitMode::Single
		? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
	if (PulseCount > 1 && ActiveAttackExecutions.Contains(ExecutionId))
	{
		FTimerDelegate PulseDelegate;
		PulseDelegate.BindUObject(this, &URogue10mCombatComponent::ExecuteAttackHitPulse, ExecutionId);
		const float Interval = FMath::Max(0.01f, SkillData.HitInterval / GetAttackSpeedMultiplier());
		GetWorld()->GetTimerManager().SetTimer(
			ActiveAttackExecutions.FindChecked(ExecutionId).TimerHandle,
			PulseDelegate, Interval, true);
	}
}

void URogue10mCombatComponent::HandleTechniqueAdvanceFinished(uint32 ExecutionId, bool bCommitContact)
{
	const auto* Execution = ActiveAttackExecutions.Find(ExecutionId);
	if (!Execution || !Execution->bAdvanceEndpointContact || Execution->CompletedPulses != 0) { return; }
	const auto* Character = GetOwnerCharacter();
	// A preceding completion listener may stop/replace playback after Motion's final poll.
	if (!bCommitContact || Execution->AdvanceEpoch != AdvanceCancellationEpoch || !CanUseCombatInput() || !Character
		|| !URogue10mTechniqueMotionComponent::DoesMontageContinue(*Character, Execution->AdvanceAnimInstance.Get(),
			Execution->AdvanceMontage.Get(), Execution->AdvanceMontageInstanceId))
	{ FinishAttackHitSequence(ExecutionId); return; }
	ExecuteAttackHitPulse(ExecutionId);
}

URogue10mCombatTestComponent* URogue10mCombatComponent::GetSpatialDebugTools() const
{
    const auto* OwnerCharacter=GetOwnerCharacter();
    const auto* PC=OwnerCharacter?Cast<ARogue10mPlayerController>(OwnerCharacter->GetController()):nullptr;
    auto* Tools=PC?PC->GetCombatTestComponent():nullptr;
    return Tools && Tools->IsSpatialDebugEnabled()?Tools:nullptr;
}

void URogue10mCombatComponent::ExecuteAttackHitPulse(uint32 ExecutionId)
{
	const auto* StoredExecution = ActiveAttackExecutions.Find(ExecutionId);
	if (!StoredExecution) { return; }
	// Damage/query callbacks can interrupt, reset or reallocate the execution map.
	// Resolve with a local snapshot; never retain a map reference across those callbacks.
	FRogue10mActiveAttackExecution PulseExecution = *StoredExecution;
	auto* Execution = &PulseExecution;
	TGuardValue<bool> ResolvingGuard(bAcceptingAttack, true);
	TGuardValue<uint32> ExecutionGuard(ResolvingAttackExecutionId, ExecutionId);
	ON_SCOPE_EXIT
	{
		// Cancellation may erase the map inside ApplyDamage. Its positive result must
		// settle hit credit before a miss can be reported for this contact.
		if (!PulseExecution.bTraitHitRegistered && !ActiveAttackExecutions.Contains(ExecutionId))
		{
			if (const auto* OwnerCharacter = GetOwnerCharacter())
			{
				if (auto* Traits = OwnerCharacter->GetTraitRuntimeComponent()) { Traits->NotifyAttackMissed(ExecutionId); }
			}
		}
	};
	const uint64 DamageEpochAtPulse = DamageInterruptionEpoch;
	const auto WasInterrupted = [this, ExecutionId, DamageEpochAtPulse]()
	{ return DamageInterruptionEpoch != DamageEpochAtPulse || !ActiveAttackExecutions.Contains(ExecutionId); };
	const URogue10mAttackSkillData* SkillData = Execution ? Execution->SkillData.Get() : nullptr;
	ARogue10mCharacter* Character = GetOwnerCharacter();
	UCameraComponent* Camera = Character ? Character->GetGameplayCameraComponent() : nullptr;
	if (!Execution || !SkillData || !Character || Character->IsDead() || !Camera || !GetWorld())
	{
		FinishAttackHitSequence(ExecutionId);
		return;
	}
	if (Execution->bAuthoredContactSequence)
	{
		if (!CanContinueAuthoredContacts(*Execution)) { FinishAttackHitSequence(ExecutionId); return; }
		const auto Contact = Execution->AuthoredTimeline.Poll(GetWorld()->GetTimeSeconds());
		if (!Contact.bValid) { FinishAttackHitSequence(ExecutionId); return; }
		if (!Contact.bDue)
		{
			ActiveAttackExecutions.FindChecked(ExecutionId).AuthoredTimeline = Execution->AuthoredTimeline;
			ScheduleNextAuthoredContact(ExecutionId); return;
		}
		if (Contact.ContactNumber != Execution->CompletedPulses + 1) { FinishAttackHitSequence(ExecutionId); return; }
	}
	const auto HitMode = Execution->bAuthoredContactSequence ? Execution->AcceptedHitMode : SkillData->HitMode;

	if (Execution->CompletedPulses == 0 && (SkillData->HitStartDelaySeconds > 0.0f || Execution->bAdvanceEndpointContact || Execution->bAuthoredContactSequence))
	{
		if (!CanUseCombatInput()
			|| (Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
				&& Character->GetCharacterMovement()->IsFalling()
				&& SkillData->InputSlot != ERogue10mAttackInputSlot::JumpPrimary
				&& SkillData->InputSlot != ERogue10mAttackInputSlot::JumpSpecial))
		{
			FinishAttackHitSequence(ExecutionId);
			return;
		}
	}

	const FTransform CombatView = Character->GetCombatViewTransform();
	const FQuat QueryRotation = Execution->bAdvanceEndpointContact || Execution->bAuthoredContactSequence
		? Execution->AcceptedCombatRotation : CombatView.GetRotation();
	++Execution->CompletedPulses;
	// Preserve existing pre-contact cancellation semantics while external callbacks resolve.
	ActiveAttackExecutions.FindChecked(ExecutionId).CompletedPulses = Execution->CompletedPulses;
	const FVector Origin = CombatView.GetLocation();
	const FVector Forward = QueryRotation.GetForwardVector();
	TArray<AActor*> Targets;

	if (HitMode == ERogue10mAttackHitMode::MultiHit && Execution->CompletedPulses > 1)
	{
		for (const TWeakObjectPtr<AActor>& LockedTarget : Execution->LockedTargets)
		{
			if (AActor* Target = LockedTarget.Get())
			{
				Targets.Add(Target);
			}
		}
        if (auto* Tools=GetSpatialDebugTools())
        {
            FRogue10mAttackDebugQuery Debug;
            Debug.Origin=Origin; Debug.Forward=Forward; Debug.ExecutionId=ExecutionId; Debug.Pulse=Execution->CompletedPulses;
            Debug.bLockedTargetReuse=true;
            for (AActor* Actor:Targets) Debug.EligibleTargets.Add(Actor);
            Tools->RecordAttackQuery(Debug);
        }
	}
	else
	{
		GatherAttackTargets(*SkillData, *Execution, Origin, Forward, QueryRotation, Targets);
		if (HitMode == ERogue10mAttackHitMode::MultiHit)
		{
			Execution->LockedTargets.Reserve(Targets.Num());
			for (AActor* Target : Targets)
			{
				Execution->LockedTargets.Add(Target);
			}
		}
	}

	const int32 MaxTargets = FMath::Clamp(SkillData->MaxTargetsPerHit, 1, 64);
	if (WasInterrupted()) { FinishAttackHitSequence(ExecutionId); return; }
	if (Execution->bAuthoredContactSequence && !CanContinueAuthoredContacts(*Execution)) { FinishAttackHitSequence(ExecutionId); return; }
	int32 AppliedTargetCount = 0;
	for (AActor* Target : Targets)
	{
		if (WasInterrupted() || (Execution->bAuthoredContactSequence && !CanContinueAuthoredContacts(*Execution))) { break; }
		if (Target && ApplyAttackDamage(*SkillData, *Execution, *Target, Forward, Execution->CompletedPulses))
		{
            if (auto* Tools=GetSpatialDebugTools())
            { Tools->RecordAcceptedTarget(*Target,ExecutionId,Execution->CompletedPulses,
				HitMode==ERogue10mAttackHitMode::MultiHit && Execution->CompletedPulses>1); }
			if (++AppliedTargetCount >= MaxTargets)
			{
				break;
			}
		}
	}
	if (WasInterrupted()) { FinishAttackHitSequence(ExecutionId); return; }
	// Only publish resolved hit counts/locks when this execution still owns its map entry.
	ActiveAttackExecutions.FindChecked(ExecutionId) = MoveTemp(PulseExecution);
	Execution = ActiveAttackExecutions.Find(ExecutionId);
	if (Execution->bAuthoredContactSequence)
	{
		if (!CanContinueAuthoredContacts(*Execution)
			|| (!Execution->AuthoredTimeline.IsActive() && !Execution->AuthoredTimeline.IsRecoveryPending(GetWorld()->GetTimeSeconds())))
		{ FinishAttackHitSequence(ExecutionId); }
		else if (!Execution->AuthoredTimeline.IsActive()) { /* Recovery retains ownership; no more contact timers. */ }
		else { ScheduleNextAuthoredContact(ExecutionId); }
		return;
	}

	const int32 PulseCount = SkillData->HitMode == ERogue10mAttackHitMode::Single
		? 1 : FMath::Clamp(SkillData->HitCount, 1, 64);
	if (Execution->CompletedPulses >= PulseCount
		|| (SkillData->HitMode == ERogue10mAttackHitMode::MultiHit && Execution->LockedTargets.IsEmpty()))
	{
		FinishAttackHitSequence(ExecutionId);
	}
}

void URogue10mCombatComponent::GatherAttackTargets(
	const URogue10mAttackSkillData& SkillData, FRogue10mActiveAttackExecution& Execution,
	const FVector& Origin, const FVector& Forward, const FQuat& Rotation, TArray<AActor*>& OutTargets) const
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	if (!Character || !GetWorld())
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Rogue10mAttackSkill), false, Character);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FOverlapResult> Overlaps;
	FVector QueryCenter = Origin;
	FCollisionShape QueryShape;
	FQuat QueryRotation = FQuat::Identity;
	const float RangeScale = FMath::Clamp(Execution.IdentityEffect.RangeMultiplier, 1.0f, 2.0f);
	const float AttackRange = SkillData.AttackRange * RangeScale;

	switch (SkillData.AttackShape)
	{
	case ERogue10mAttackShape::LinearBox:
		QueryCenter = Origin + Forward * (AttackRange * 0.5f);
		QueryShape = FCollisionShape::MakeBox(FVector(
			AttackRange * 0.5f, SkillData.BoxHalfWidth * RangeScale, SkillData.BoxHalfHeight));
		QueryRotation = Rotation;
		break;
	case ERogue10mAttackShape::Projectile:
	{
		const int32 PulseCount = SkillData.HitMode == ERogue10mAttackHitMode::Single
			? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
		const float SegmentLength = AttackRange / PulseCount;
		QueryCenter = Origin + Forward * (Execution.ProjectileTravelDistance + SegmentLength * 0.5f);
		Execution.ProjectileTravelDistance += SegmentLength;
		QueryShape = FCollisionShape::MakeBox(FVector(
			SegmentLength * 0.5f, SkillData.AttackTraceRadius, SkillData.AttackTraceRadius));
		QueryRotation = Rotation;
		break;
	}
	case ERogue10mAttackShape::Arc:
		QueryCenter = Origin;
		QueryShape = FCollisionShape::MakeSphere(AttackRange);
		break;
	case ERogue10mAttackShape::Circle:
		QueryCenter = Origin + Forward * SkillData.CircleForwardOffset;
		QueryShape = FCollisionShape::MakeSphere(AttackRange);
		break;
	default:
		return;
	}

	GetWorld()->OverlapMultiByObjectType(
		Overlaps, QueryCenter, QueryRotation, ObjectQueryParams, QueryShape, QueryParams);
	if (MeleeVfx && Execution.MeleeVfxTier != INDEX_NONE)
	{
		const double FeetZ = Character->GetCapsuleComponent()->GetComponentLocation().Z
			- Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const auto Outline = Rogue10m::MeleeVfx::MakeRangeOutline(*GetWorld(), Character, SkillData.AttackShape,
			QueryCenter, QueryRotation, QueryShape.GetExtent(), Forward, SkillData.ArcAngleDegrees, FeetZ);
		MeleeVfx->ShowRange(Outline, Execution.MeleeVfxTier, Execution.AcceptedWeaponType == ERogue10mWeaponType::GreatSword);
	}

	TSet<TWeakObjectPtr<AActor>> UniqueActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == Character || UniqueActors.Contains(Target)
			|| !Target->GetClass()->ImplementsInterface(URogue10mAttackTargetInterface::StaticClass())
			|| !IRogue10mAttackTargetInterface::Execute_CanReceiveRogue10mAttack(Target, Character))
		{
			continue;
		}

		if (Execution.bAdvanceEndpointContact || Execution.bAuthoredContactSequence)
		{
			FHitResult Blocker;
			if (GetWorld()->LineTraceSingleByChannel(Blocker, Origin, Target->GetActorLocation(), ECC_Visibility, QueryParams)
				&& Blocker.GetActor() != Target) { continue; }
		}
		if (SkillData.AttackShape == ERogue10mAttackShape::Arc)
		{
			const FVector Direction = (Target->GetActorLocation() - Origin).GetSafeNormal();
			const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(
				FMath::Clamp(SkillData.ArcAngleDegrees, 1.0f, 180.0f) * 0.5f));
			if (FVector::DotProduct(Forward, Direction) < MinimumDot)
			{
				continue;
			}
		}

		UniqueActors.Add(Target);
		OutTargets.Add(Target);
	}

	OutTargets.Sort([Origin](const AActor& Left, const AActor& Right)
	{
		return FVector::DistSquared(Origin, Left.GetActorLocation())
			< FVector::DistSquared(Origin, Right.GetActorLocation());
	});

    if (auto* Tools=GetSpatialDebugTools())
    {
        FRogue10mAttackDebugQuery Debug;
        Debug.Origin=Origin; Debug.Forward=Forward; Debug.Center=QueryCenter; Debug.Rotation=QueryRotation;
        Debug.Shape=SkillData.AttackShape; Debug.Extent=QueryShape.GetExtent(); Debug.Radius=AttackRange;
        Debug.ArcAngleDegrees=FMath::Clamp(SkillData.ArcAngleDegrees,1.f,180.f);
        Debug.ExecutionId=Execution.ExecutionId; Debug.Pulse=Execution.CompletedPulses;
        for (const auto& Overlap:Overlaps) if (auto* Primitive=Overlap.GetComponent()) Debug.OverlapComponents.AddUnique(Primitive);
        for (AActor* Actor:OutTargets) Debug.EligibleTargets.Add(Actor);
        Tools->RecordAttackQuery(Debug);
    }
    // Per-asset bDrawDebugAttack does not grant a normal-play debug path. Explicit test gate owns all visual output.

}

bool URogue10mCombatComponent::ApplyAttackDamage(
	const URogue10mAttackSkillData& SkillData, FRogue10mActiveAttackExecution& Execution,
	AActor& TargetActor, const FVector& DamageDirection, int32 PulseIndex)
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	const uint64 DamageEpochAtContact = DamageInterruptionEpoch;
	const bool bManagedExecution = ActiveAttackExecutions.Contains(Execution.ExecutionId);
	const auto WasInterrupted = [this, &Execution, Character, DamageEpochAtContact, bManagedExecution]()
	{
		return DamageInterruptionEpoch != DamageEpochAtContact
			|| (bManagedExecution && !ActiveAttackExecutions.Contains(Execution.ExecutionId))
			|| (Execution.bAuthoredContactSequence && !CanContinueAuthoredContacts(Execution))
			|| (Execution.bAdvanceEndpointContact && (!Character
				|| !URogue10mTechniqueMotionComponent::DoesMontageContinue(*Character, Execution.AdvanceAnimInstance.Get(),
					Execution.AdvanceMontage.Get(), Execution.AdvanceMontageInstanceId)));
	};
	if (!Character || !Character->HasAuthority() || !TargetActor.GetClass()->ImplementsInterface(URogue10mAttackTargetInterface::StaticClass())
		|| !IRogue10mAttackTargetInterface::Execute_CanReceiveRogue10mAttack(&TargetActor, Character))
	{
		return false;
	}
	if (WasInterrupted()) { return false; } // CanReceive is a Blueprint-capable callback.

	const int32 AllowedHits = Execution.bAuthoredContactSequence ? Execution.AuthoredTimeline.TotalCount()
		: SkillData.HitMode == ERogue10mAttackHitMode::Single ? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
	if (Execution.TargetHitCounts.FindRef(&TargetActor) >= AllowedHits)
	{
		return false;
	}

	bool bCriticalHit = false;
	URogue10mTraitRuntimeComponent* Traits = Character->GetTraitRuntimeComponent();
	const float TraitMultiplier = Traits ? Traits->GetAttackDamageMultiplier(TargetActor, Execution.TraitDodgeBonus) : 1.0f;
	const ARogue10mBasicMonster* MasteryTarget = Cast<ARogue10mBasicMonster>(&TargetActor);
	// Check life before damage so the killing hit still counts; training/dummy targets do not.
	const bool bEligibleMasteryTarget = MasteryTarget && !MasteryTarget->IsDead() && !MasteryTarget->IsCombatTestInstance()
		&& MasteryTarget->GetMonsterData() && MasteryTarget->GetMonsterData()->ExperienceReward > 0
		&& !TargetActor.ActorHasTag(TEXT("TrainingTarget"));
	const float RolledDamage = SkillData.RollDamage(GetOwnerAttributes(), bCriticalHit) * TraitMultiplier
		* Execution.IdentityEffect.DamageMultiplier;
	if (WasInterrupted()) { return false; }
	const bool bSwordVfx = Execution.AcceptedWeaponType == ERogue10mWeaponType::GreatSword;
	TOptional<Rogue10m::MeleeVfx::FImpactPoint> MeleeImpact;
	if (MeleeVfx && Execution.MeleeVfxTier != INDEX_NONE)
	{
		// Sample before damage callbacks can move the target, replace its collision or trigger ragdoll.
		MeleeImpact = SampleMeleeImpact(*Character, TargetActor, SkillData, bSwordVfx);
	}
	const float AppliedDamage = UGameplayStatics::ApplyDamage(
		&TargetActor, RolledDamage, Character->GetController(), Character, UDamageType::StaticClass());
	if (!FMath::IsFinite(AppliedDamage) || AppliedDamage <= 0.0f)
	{
		return false;
	}

	Execution.TargetHitCounts.FindOrAdd(&TargetActor) += 1;
	if (bEligibleMasteryTarget && !Execution.bMasteryAwarded)
	{
		Execution.bMasteryAwarded = true;
		if (auto* State = Character->GetPlayerState<ARogue10mPlayerState>())
		{
			if (auto* Tree = State->GetSkillTreeProgressionComponent()) { Tree->NotifyConfirmedAttack(Execution.MasteryTrack); }
		}
		if (auto* Identity = Character->GetSkillTreeCombatComponent()) { Identity->NotifyConfirmedAttack(Execution.MasteryTrack); }
	}
	if (Traits && !Execution.bTraitHitRegistered)
	{
		// First confirmed target owns this execution's combo credit; multi-hit pulses cannot add credit.
		Execution.bTraitHitRegistered = true;
		Traits->NotifyAttackHit(Execution.ExecutionId, TargetActor);
	}
	const URogue10mBasicBrawlerComponent* Brawler = Character->FindComponentByClass<URogue10mBasicBrawlerComponent>();
	const bool bBrawlerPresentation = Execution.bBrawlerPresentation && Brawler && Brawler->IsBasicBrawlerActive();
	if (MeleeImpact.IsSet() && IsValid(MeleeVfx))
	{
		MeleeVfx->ShowImpact(MeleeImpact->Location, MeleeImpact->Normal, Execution.MeleeVfxTier, bSwordVfx);
	}
	SpawnImpactEffect(SkillData, TargetActor, bBrawlerPresentation, MeleeImpact.IsSet() ? &MeleeImpact.GetValue() : nullptr);
	if (bBrawlerPresentation)
	{
		const float Strength = SkillData.InputSlot == ERogue10mAttackInputSlot::ChargedSpecial ? 1.0f
			: SkillData.InputSlot == ERogue10mAttackInputSlot::Special ? 0.7f : 0.45f;
		if (ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(&TargetActor); Monster && !Monster->IsDead())
		{
			URogue10mHitFeedbackComponent* Feedback = Monster->FindComponentByClass<URogue10mHitFeedbackComponent>();
			if (!Feedback)
			{
				Feedback = NewObject<URogue10mHitFeedbackComponent>(Monster);
				Monster->AddInstanceComponent(Feedback);
				Feedback->RegisterComponent();
			}
			Feedback->PlayHit(Strength, DamageDirection);
		}
		if (!Execution.bConfirmedCameraFeedbackSent)
		{
			Execution.bConfirmedCameraFeedbackSent = true;
			if (URogue10mFirstPersonPresentationComponent* Presentation = Character->FindComponentByClass<URogue10mFirstPersonPresentationComponent>())
			{ Presentation->NotifyConfirmedBrawlerHit(Strength, SkillData.EffectAttachSocket == TEXT("hand_l") ? -1.0f : 1.0f); }
		}
	}
	if (ARogue10mPlayerController* PlayerController = Cast<ARogue10mPlayerController>(Character->GetController()))
	{
		PlayerController->AddFloatingDamageNumber(&TargetActor, AppliedDamage, bCriticalHit);
	}
	UE_LOG(LogRogue10m, Verbose, TEXT("%s %d타: %s 피해 %.1f%s"),
		*SkillData.SkillName.ToString(), PulseIndex, *GetNameSafe(&TargetActor), AppliedDamage,
		bCriticalHit ? TEXT(" (치명타)") : TEXT(""));
	return true;
}

void URogue10mCombatComponent::StartChargeEffect(const URogue10mAttackSkillData& SkillData)
{
	StopChargeEffect();
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mCharacterAnimationComponent* Animation = Character->GetCharacterAnimationComponent())
		{
			Animation->SetFighterCombatChargeHeld(true);
		}
	}
	bool bFirstPerson = false;
	USkeletalMeshComponent* AttachMesh = GetEffectAttachMesh(bFirstPerson);
	if (!SkillData.bEnableAttackEffects || !AttachMesh || !SkillData.ChargeEffect)
	{
		return;
	}

	const bool bUseFirstPersonOverrides = bFirstPerson && SkillData.bUseFirstPersonEffectOverrides;
	const float EffectScale = SkillData.ChargeEffectScale
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonChargeScaleMultiplier : 1.0f);
	const FVector PrimaryOffset = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectOffset : FVector::ZeroVector;
	const FRotator EffectRotation = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectRotation : FRotator::ZeroRotator;

	auto SpawnChargeOnSocket = [&, this](FName RequestedSocket, bool bOffHand)
	{
		const FName SocketName = AttachMesh->DoesSocketExist(RequestedSocket)
			? RequestedSocket : NAME_None;
		UNiagaraComponent* EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			SkillData.ChargeEffect, AttachMesh, SocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::None, true);
		if (EffectComponent)
		{
			const float SocketEffectScale = EffectScale *
				(bOffHand && bUseFirstPersonOverrides
					? SkillData.FirstPersonOffHandEffectScaleMultiplier : 1.0f);
			FVector RelativeOffset = PrimaryOffset;
			if (bOffHand && bUseFirstPersonOverrides)
			{
				RelativeOffset.Y *= -1.0f;
			}
			EffectComponent->SetRelativeLocationAndRotation(RelativeOffset, EffectRotation);
			EffectComponent->SetRelativeScale3D(FVector(FMath::Max(0.01f, SocketEffectScale)));
			EffectComponent->SetCustomTimeDilation(
				FMath::Max(0.1f, SkillData.AttackEffectTimeDilation));
		}
		return EffectComponent;
	};

	ActiveChargeEffectComponent = SpawnChargeOnSocket(SkillData.EffectAttachSocket, false);
	if (SkillData.bSpawnEffectOnOffHand)
	{
		ActiveOffHandChargeEffectComponent = SpawnChargeOnSocket(
			SkillData.OffHandEffectAttachSocket, true);
	}
}

void URogue10mCombatComponent::StopChargeEffect()
{
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mCharacterAnimationComponent* Animation = Character->GetCharacterAnimationComponent())
		{
			Animation->SetFighterCombatChargeHeld(false);
		}
	}
	auto StopEffect = [](TObjectPtr<UNiagaraComponent>& EffectComponent)
	{
		if (EffectComponent)
		{
			EffectComponent->DeactivateImmediate();
			EffectComponent->DestroyComponent();
			EffectComponent = nullptr;
		}
	};
	StopEffect(ActiveChargeEffectComponent);
	StopEffect(ActiveOffHandChargeEffectComponent);
}

void URogue10mCombatComponent::SpawnCastEffect(const URogue10mAttackSkillData& SkillData) const
{
	if (!SkillData.bEnableAttackEffects)
	{
		return;
	}

	bool bFirstPerson = false;
	USkeletalMeshComponent* AttachMesh = GetEffectAttachMesh(bFirstPerson);
	UNiagaraSystem* Effect = SkillData.CastEffect;
	if (!Effect && !SkillData.AttackEffect.IsNull())
	{
		Effect = Cast<UNiagaraSystem>(SkillData.AttackEffect.LoadSynchronous());
	}
	if (!AttachMesh || !Effect)
	{
		return;
	}

	const bool bUseFirstPersonOverrides = bFirstPerson && SkillData.bUseFirstPersonEffectOverrides;
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const bool bUseThirdPersonPlacement = Character && Character->IsThirdPersonGameplayActive();
	const float EffectScale = SkillData.CastEffectScale
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonCastScaleMultiplier : 1.0f);
	const float EmissionDuration = SkillData.CastEffectEmissionDuration
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonEmissionDurationMultiplier : 1.0f);
	const FVector PrimaryOffset = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectOffset
		: (bUseThirdPersonPlacement ? SkillData.ThirdPersonCastEffectOffset : FVector::ZeroVector);
	const FRotator EffectRotation = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectRotation
		: (bUseThirdPersonPlacement ? SkillData.ThirdPersonCastEffectRotation : FRotator::ZeroRotator);

	auto SpawnCastOnSocket = [&](FName RequestedSocket, bool bOffHand)
	{
		const FName SocketName = AttachMesh->DoesSocketExist(RequestedSocket)
			? RequestedSocket : NAME_None;
		UNiagaraComponent* SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Effect, AttachMesh, SocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::None, true);
		if (SpawnedEffect)
		{
			const float SocketEffectScale = EffectScale *
				(bOffHand && bUseFirstPersonOverrides
					? SkillData.FirstPersonOffHandEffectScaleMultiplier : 1.0f);
			FVector RelativeOffset = PrimaryOffset;
			if (bOffHand && bUseFirstPersonOverrides)
			{
				RelativeOffset.Y *= -1.0f;
			}
			SpawnedEffect->SetRelativeLocationAndRotation(RelativeOffset, EffectRotation);
			ConfigureTransientEffect(
				SpawnedEffect, SocketEffectScale, EmissionDuration, SkillData.AttackEffectTimeDilation);
		}
	};

	SpawnCastOnSocket(SkillData.EffectAttachSocket, false);
	if (SkillData.bSpawnEffectOnOffHand)
	{
		SpawnCastOnSocket(SkillData.OffHandEffectAttachSocket, true);
	}
}

void URogue10mCombatComponent::SpawnImpactEffect(
	const URogue10mAttackSkillData& SkillData, const AActor& TargetActor, bool bBrawlerPresentation,
	const Rogue10m::MeleeVfx::FImpactPoint* MeleeImpact) const
{
	if (!SkillData.bEnableAttackEffects || !SkillData.ImpactEffect)
	{
		return;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	TargetActor.GetActorBounds(true, BoundsOrigin, BoundsExtent);
	bool bFirstPerson = false;
	GetEffectAttachMesh(bFirstPerson);
	const bool bUseFirstPersonOverrides = bFirstPerson && SkillData.bUseFirstPersonEffectOverrides;
	const float EffectScale = SkillData.ImpactEffectScale
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonImpactScaleMultiplier : 1.0f);
	const float EmissionDuration = SkillData.ImpactEffectEmissionDuration
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonEmissionDurationMultiplier : 1.0f);
	FVector EffectLocation = BoundsOrigin;
	FRotator EffectRotation = TargetActor.GetActorRotation();
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	if (MeleeImpact)
	{
		EffectLocation = MeleeImpact->Location;
		EffectRotation = (-MeleeImpact->Normal).Rotation();
	}
	else if (bBrawlerPresentation && Character)
	{
		// Attack targeting is an overlap, so this is a conservative near-side bounds
		// estimate, not an exact physics contact point. Keep it at torso height.
		const FVector TowardAttacker = (Character->GetActorLocation() - BoundsOrigin).GetSafeNormal2D();
		EffectLocation += TowardAttacker * FMath::Min(BoundsExtent.X, BoundsExtent.Y);
		EffectRotation = (-TowardAttacker).Rotation();
	}
	else if (Character && Character->IsThirdPersonGameplayActive()
		&& SkillData.bUseThirdPersonImpactBoundsSurface)
	{
		// Keep the original positive-damage event and target-bounds torso height.
		// This optional placement changes pixels only, never damage or targeting.
		EffectLocation = Rogue10m::CombatVfx::NearSideBoundsLocation(
			BoundsOrigin, BoundsExtent, Character->GetActorLocation(),
			SkillData.ThirdPersonImpactEffectSurfaceOffset);
	}
	UNiagaraComponent* SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, SkillData.ImpactEffect, EffectLocation, EffectRotation,
		FVector::OneVector,
		true, true, ENCPoolMethod::None, true);
	ConfigureTransientEffect(
		SpawnedEffect, EffectScale, EmissionDuration, SkillData.AttackEffectTimeDilation);
}

void URogue10mCombatComponent::ConfigureTransientEffect(
	UNiagaraComponent* EffectComponent, float UniformScale,
	float EmissionDuration, float TimeDilation) const
{
	if (!EffectComponent)
	{
		return;
	}

	EffectComponent->SetRelativeScale3D(FVector(FMath::Max(0.01f, UniformScale)));
	EffectComponent->SetCustomTimeDilation(FMath::Max(0.1f, TimeDilation));
	if (UWorld* World = GetWorld())
	{
		FTimerHandle DeactivateTimerHandle;
		World->GetTimerManager().SetTimer(
			DeactivateTimerHandle,
			FTimerDelegate::CreateWeakLambda(EffectComponent, [EffectComponent]()
			{
				EffectComponent->DeactivateImmediate();
				EffectComponent->DestroyComponent();
			}),
			FMath::Max(0.01f, EmissionDuration), false);
	}
}
USkeletalMeshComponent* URogue10mCombatComponent::GetEffectAttachMesh(bool& bOutFirstPerson) const
{
	bOutFirstPerson = false;
	ARogue10mCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return nullptr;
	}
	// Third-person effects follow the rendered body and never use inactive first-person overrides.
	if (Character->IsThirdPersonGameplayActive())
	{
		return Character->GetMesh();
	}
	if (const auto* AppearanceCamera = Character->GetAppearanceCameraComponent();
		AppearanceCamera && AppearanceCamera->IsAppearanceCameraActive())
	{
		return Character->GetMesh();
	}
	if (Character->IsLocallyControlled() && Character->GetFirstPersonMesh()
		&& Character->GetFirstPersonMesh()->GetSkeletalMeshAsset())
	{
		bOutFirstPerson = true;
		return Character->GetFirstPersonMesh();
	}
	return Character->GetMesh();
}


void URogue10mCombatComponent::CancelPendingAttackHits(const URogue10mAttackSkillData* SkillData)
{
	for (auto It = ActiveAttackExecutions.CreateIterator(); It; ++It)
	{
		const URogue10mAttackSkillData* ActiveSkill = It.Value().SkillData.Get();
		if (ActiveSkill && (!SkillData || ActiveSkill == SkillData)
			&& (It.Value().bAuthoredContactSequence || (ActiveSkill->HitStartDelaySeconds > 0.0f && It.Value().CompletedPulses == 0)))
		{
			if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(It.Value().TimerHandle); }
			if (MeleeVfx) { MeleeVfx->EndTrail(It.Key()); }
			ReleaseAuthoredRecoveryAction(It.Value());
			if (ARogue10mCharacter* Character = GetOwnerCharacter(); Character && It.Key() != ResolvingAttackExecutionId
				&& !It.Value().bTraitHitRegistered)
			{
				if (URogue10mTraitRuntimeComponent* Traits = Character->GetTraitRuntimeComponent()) { Traits->NotifyAttackMissed(It.Key()); }
			}
			It.RemoveCurrent();
		}
	}
	if (GetWorld() && !HasActiveAuthoredContacts()) { GetWorld()->GetTimerManager().ClearTimer(AuthoredContactMonitorTimer); }
}

void URogue10mCombatComponent::FinishAttackHitSequence(uint32 ExecutionId)
{
	if (FRogue10mActiveAttackExecution* Execution = ActiveAttackExecutions.Find(ExecutionId))
	{
		if (!Execution->bTraitHitRegistered && ExecutionId != ResolvingAttackExecutionId)
		{
			if (ARogue10mCharacter* Character = GetOwnerCharacter())
			{
				if (URogue10mTraitRuntimeComponent* Traits = Character->GetTraitRuntimeComponent()) { Traits->NotifyAttackMissed(ExecutionId); }
			}
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Execution->TimerHandle);
		}
		ReleaseAuthoredRecoveryAction(*Execution);
	}
	ActiveAttackExecutions.Remove(ExecutionId);
	if (GetWorld() && !HasActiveAuthoredContacts()) { GetWorld()->GetTimerManager().ClearTimer(AuthoredContactMonitorTimer); }
}

bool URogue10mCombatComponent::TryActivateAttackAbility(const URogue10mAttackSkillData& SkillData, bool bComboAttack)
{
	if (bExecutingAttackFromAbility)
	{
		return false;
	}

	ARogue10mCharacter* Character = GetOwnerCharacter();
	UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const TSubclassOf<UGameplayAbility> AbilityClass = SkillData.GameplayAbilityClass
		? SkillData.GameplayAbilityClass
		: DefaultAttackAbilityClass;
	if (!Character || !AbilitySystem || !AbilityClass)
	{
		return false;
	}

	PendingAbilityAttackSkill = &SkillData;
	bPendingAbilityComboAttack = bComboAttack;
	if (!AbilitySystem->FindAbilitySpecFromClass(AbilityClass) && Character->HasAuthority())
	{
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}

	if (!AbilitySystem->TryActivateAbilityByClass(AbilityClass))
	{
		PendingAbilityAttackSkill.Reset();
		bPendingAbilityComboAttack = false;
		return false;
	}
	return true;
}

bool URogue10mCombatComponent::CanPayResourceCosts(const URogue10mAttackSkillData& SkillData, bool bComboAttack) const
{
	const auto* Attributes = GetOwnerAttributes();
	auto* Character = GetOwnerCharacter();
	if (!Attributes || !Character) { return false; }
	const auto* Identity = Character->GetSkillTreeCombatComponent();
	const auto Budget = FRogue10mAttackResourceBudget::Build(SkillData.ResourceCosts,
		Identity ? Identity->GetStaminaCostMultiplier(bComboAttack) : 1.0f);
	if (!Budget.CanPay(*Attributes))
	{
		if (Budget.bValid && Attributes->GetStamina() < Budget.Stamina)
		{
			if (auto* Feedback = Character->GetPlayerFeedbackComponent()) { Feedback->NotifyInsufficientStamina(); }
		}
		AddCombatLog(FString::Printf(TEXT("%s 사용 불가: 자원 비용을 지불할 수 없습니다."), *SkillData.SkillName.ToString()),
			FLinearColor(1.0f, 0.42f, 0.24f, 1.0f));
		return false;
	}
	return true;
}

bool URogue10mCombatComponent::TryPayResourceCosts(const URogue10mAttackSkillData& SkillData, bool bComboAttack)
{
	auto* Attributes = GetOwnerAttributes();
	auto* Character = GetOwnerCharacter();
	if (!Attributes || !Character) { return false; }
	auto* Identity = Character->GetSkillTreeCombatComponent();
	if (Identity) { Identity->SynchronizeIdentity(); }
	if (!CanPayResourceCosts(SkillData, bComboAttack)) { return false; }
	auto Budget = FRogue10mAttackResourceBudget::Build(SkillData.ResourceCosts,
		Identity ? Identity->GetStaminaCostMultiplier(bComboAttack) : 1.0f);
	// Optional identity effects can use only the mana left after the full base cost.
	const auto Effect = Identity ? Identity->PreviewAttack(SkillData, Attributes->GetMana() - Budget.Mana)
		: FRogue10mSkillTreeAttackEffect();
	Budget.Mana += Effect.ManaCost;
	if (!Budget.TryPay(*Attributes)) { return false; }
	AcceptedIdentityEffect = Effect;
	if (const auto* Burst = GetOwner() ? GetOwner()->FindComponentByClass<URogue10mWeaponIdentityComponent>() : nullptr)
	{
		AcceptedIdentityEffect.DamageMultiplier *= Burst->GetDamageMultiplier(SkillData);
		AcceptedIdentityEffect.RangeMultiplier *= Burst->GetRangeMultiplier(SkillData);
	}
	if (Identity) { Identity->CommitAttack(Effect, bComboAttack); }
	return true;
}

void URogue10mCombatComponent::StartSharedAttackCooldown(const URogue10mAttackSkillData& SkillData, bool bComboAttack, bool bPreserveLongerCooldown)
{
	if (!GetWorld())
	{
		return;
	}

	const URogue10mAttackSkillData* CooldownSource = &SkillData;
	if (bComboAttack && ActiveComboRootSkill.IsValid())
	{
		CooldownSource = ActiveComboRootSkill.Get();
	}
	else if (!bComboAttack)
	{
		ActiveComboRootSkill = &SkillData;
	}

	const float AttackSpeed = GetAttackSpeedMultiplier();
	const float Duration = FMath::Max(0.0f, CooldownSource->AttackCooldown) / AttackSpeed
		* FMath::Clamp(AcceptedIdentityEffect.RecoveryMultiplier, 0.1f, 1.0f);
	const bool bHasNextCombo = bAllowAttackCombo && SkillData.bEnableCombo && SkillData.NextComboSkill;
	const float StartDelay = bHasNextCombo
		? FMath::Max(SkillData.ComboWindowOpenSeconds, SkillData.ComboWindowCloseSeconds) / AttackSpeed
		: 0.0f;
	const float Start = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, StartDelay);
	if (bPreserveLongerCooldown && AttackCooldownEndTime > Start + Duration) { return; }
	AttackCooldownSourceSkill = CooldownSource;
	AttackCooldownDuration = Duration;
	AttackCooldownStartTime = Start;
	AttackCooldownEndTime = Start + Duration;
}

void URogue10mCombatComponent::OpenComboWindow(const URogue10mAttackSkillData& SkillData)
{
	if (!bAllowAttackCombo || !SkillData.bEnableCombo || !SkillData.NextComboSkill || !GetWorld())
	{
		ResetComboWindow();
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float AttackSpeed = GetAttackSpeedMultiplier();
	const float OpenOffset = FMath::Max(0.0f, SkillData.ComboWindowOpenSeconds) / AttackSpeed;
	const float CloseOffset = FMath::Max(OpenOffset, SkillData.ComboWindowCloseSeconds / AttackSpeed);
	ActiveComboSourceSkill = &SkillData;
	ActiveComboWindowOpenTime = CurrentTime + OpenOffset;
	ActiveComboWindowCloseTime = CurrentTime + CloseOffset;
	AddCombatLog(
		FString::Printf(TEXT("콤보 대기: %.2f초 ~ %.2f초"), OpenOffset, CloseOffset),
		FLinearColor(0.72f, 0.88f, 1.0f, 1.0f));
}

void URogue10mCombatComponent::ResetComboWindow()
{
	ActiveComboSourceSkill.Reset();
	ActiveComboRootSkill.Reset();
	ActiveComboWindowOpenTime = -1.0f;
	ActiveComboWindowCloseTime = -1.0f;
}

void URogue10mCombatComponent::AddCombatLog(const FString& Message, const FLinearColor& Color) const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	if (ARogue10mPlayerController* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr)
	{
		Controller->AddCombatLogMessage(Message, Color);
	}
	UE_LOG(LogRogue10m, Log, TEXT("%s"), *Message);
}

FString URogue10mCombatComponent::GetAttackInputText(bool bPrimaryAttack, bool bJumpAttack) const
{
	if (bJumpAttack)
	{
		return bPrimaryAttack ? TEXT("점프 좌클릭") : TEXT("점프 우클릭");
	}
	return bPrimaryAttack ? TEXT("좌클릭") : TEXT("우클릭");
}

FRogue10mCombatSnapshot URogue10mCombatComponent::CaptureCombat() const
{
	FRogue10mCombatSnapshot Snapshot;
	TSet<URogue10mAttackSkillData*> KnownSkills;
	for (URogue10mAttackSkillData* Skill : UnlockedSkillAssets) { KnownSkills.Add(Skill); }
	for (const URogue10mWeaponSkillProfileDataAsset* Profile : WeaponSkillProfiles)
	{
		if (!Profile) { continue; }
		for (URogue10mAttackSkillData* Skill : Profile->SkillTreeSkills) { KnownSkills.Add(Skill); }
		for (URogue10mAttackSkillData* Skill : Profile->InitiallyUnlockedSkills) { KnownSkills.Add(Skill); }
		for (const auto& Pair : Profile->DefaultSkillBindings) { KnownSkills.Add(Pair.Value); }
	}
	for (URogue10mAttackSkillData* Skill : { PrimaryAttackSkill.Get(), SpecialAttackSkill.Get(),
		JumpPrimaryAttackSkill.Get(), JumpSpecialAttackSkill.Get(), ChargedPrimaryAttackSkill.Get(),
		ChargedSpecialAttackSkill.Get() }) { KnownSkills.Add(Skill); }
	for (URogue10mAttackSkillData* Skill : KnownSkills)
	{
		if (IsSkillDurablyUnlocked(Skill))
		{
			Snapshot.UnlockedSkills.Add(TSoftObjectPtr<URogue10mAttackSkillData>(Skill));
		}
	}
	Snapshot.UnlockedSkills.Sort([](const auto& Left, const auto& Right)
	{
		return Left.ToSoftObjectPath().ToString() < Right.ToSoftObjectPath().ToString();
	});
	// One merge rule for save and swap; newly learned previews become real choices, still-locked previews do not.
	for (const auto& Pair : BuildDurableSkillBindings())
	{
		if (Pair.Value && Snapshot.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Pair.Value.Get())))
		{ Snapshot.SkillBindings.Add(Pair.Key, TSoftObjectPtr<URogue10mAttackSkillData>(Pair.Value.Get())); }
	}
	TMap<ERogue10mWeaponType, FRogue10mRuntimeWeaponSkillPreset> Presets = WeaponSkillPresets;
	if (bWeaponPresetApplied)
	{
		auto& Current = Presets.FindOrAdd(AppliedProfileWeaponType); Current.Slots.Reset();
		for (const auto& Pair : Snapshot.SkillBindings)
		{
			if (URogue10mAttackSkillData::IsLoadoutSlot(Pair.Key)) { Current.Slots.Add(Pair.Key, Pair.Value.Get()); }
		}
	}
	for (const auto& Preset : Presets)
	{
		const URogue10mWeaponSkillProfileDataAsset* PresetProfile = nullptr;
		for (const URogue10mWeaponSkillProfileDataAsset* Profile : WeaponSkillProfiles) { if (Profile && Profile->WeaponType == Preset.Key) { PresetProfile = Profile; break; } }
		auto& Saved = Snapshot.WeaponPresets.AddDefaulted_GetRef(); Saved.WeaponType = Preset.Key;
		for (const auto& Pair : Preset.Value.Slots)
		{
			if (Pair.Value && Pair.Value->IsAllowedInLoadoutSlot(Pair.Key) && PresetProfile && PresetProfile->CatalogAllowsLoadoutSkill(Pair.Value)
				&& Snapshot.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Pair.Value.Get())))
			{ Saved.Slots.Add(Pair.Key, TSoftObjectPtr<URogue10mAttackSkillData>(Pair.Value.Get())); }
		}
	}
	Snapshot.WeaponPresets.Sort([](const auto& A, const auto& B) { return static_cast<uint8>(A.WeaponType) < static_cast<uint8>(B.WeaponType); });
	return Snapshot;
}

bool URogue10mCombatComponent::RestoreCombat(const FRogue10mCombatSnapshot& Snapshot)
{
	TSet<FName> RestoredNames;
	TArray<TObjectPtr<URogue10mAttackSkillData>> RestoredAssets;
	TMap<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>> RestoredBindings;
	for (const auto& Reference : Snapshot.UnlockedSkills)
	{
		URogue10mAttackSkillData* Skill = Reference.LoadSynchronous();
		if (!Skill) { return false; }
		RestoredNames.Add(Skill->GetFName());
		RestoredAssets.AddUnique(Skill);
	}
	TMap<ERogue10mWeaponType, FRogue10mRuntimeWeaponSkillPreset> RestoredPresets;
	if (Snapshot.WeaponPresets.Num() > 12) { return false; }
	for (const auto& Saved : Snapshot.WeaponPresets)
	{
		const URogue10mWeaponSkillProfileDataAsset* Profile = nullptr;
		for (const URogue10mWeaponSkillProfileDataAsset* Candidate : WeaponSkillProfiles) { if (Candidate && Candidate->WeaponType == Saved.WeaponType) { Profile = Candidate; break; } }
		if (!Profile || Saved.Slots.Num() > 6 || RestoredPresets.Contains(Saved.WeaponType)) { return false; }
		auto& Restored = RestoredPresets.Add(Saved.WeaponType); TSet<const URogue10mAttackSkillData*> Seen;
		for (const auto& Pair : Saved.Slots)
		{
			// Resolve against the configured profile, never load arbitrary saved paths.
			URogue10mAttackSkillData* Skill = nullptr;
			for (URogue10mAttackSkillData* Candidate : Profile->SkillTreeSkills) { if (Candidate && FSoftObjectPath(Candidate) == Pair.Value.ToSoftObjectPath()) { Skill = Candidate; break; } }
			if (!Skill || Skill == Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary)
				|| !Profile->CatalogAllowsLoadoutSkill(Skill)
				|| Saved.WeaponType == ERogue10mWeaponType::Unarmed || !RestoredNames.Contains(Skill->GetFName())
				|| !Skill->IsAllowedInLoadoutSlot(Pair.Key) || Seen.Contains(Skill)) { return false; }
			Seen.Add(Skill); Restored.Slots.Add(Pair.Key, Skill);
		}
	}
	TMap<ERogue10mAttackInputSlot, TSoftObjectPtr<URogue10mAttackSkillData>> IncomingBindings = Snapshot.SkillBindings;
	if (!IncomingBindings.Contains(ERogue10mAttackInputSlot::ChargedSpecial))
	{
		if (const auto* LegacyCharge = IncomingBindings.Find(ERogue10mAttackInputSlot::ChargedPrimary))
		{ IncomingBindings.Add(ERogue10mAttackInputSlot::ChargedSpecial, *LegacyCharge); }
	}
	TSet<const URogue10mAttackSkillData*> SeenActive;
	for (const auto& Pair : IncomingBindings)
	{
		if (static_cast<uint8>(Pair.Key) > static_cast<uint8>(ERogue10mAttackInputSlot::MouseChord)) { return false; }
		if (Pair.Key != ERogue10mAttackInputSlot::Primary && !URogue10mAttackSkillData::IsLoadoutSlot(Pair.Key)) { continue; }
		const auto* Character = GetOwnerCharacter();
		if (Character && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed && Pair.Key != ERogue10mAttackInputSlot::Primary) { continue; }
		URogue10mAttackSkillData* Skill = Pair.Value.LoadSynchronous();
		const auto* Profile = FindActiveWeaponProfile();
		const auto* DefaultSkill = Profile ? Profile->DefaultSkillBindings.Find(Pair.Key) : nullptr;
		if (Pair.Key == ERogue10mAttackInputSlot::Primary && (!DefaultSkill || DefaultSkill->Get() != Skill)) { continue; }
		if (Pair.Key != ERogue10mAttackInputSlot::Primary && Profile && Skill == Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary)) { continue; }
		if (SeenActive.Contains(Skill)) { return false; }
		SeenActive.Add(Skill);
		// Existing profiles include default jump attacks outside their visible
		// tree. Preserve that exact default slot without admitting other skills.
		const bool bProfileBinding = IsSkillInActiveTree(Skill) || (DefaultSkill && DefaultSkill->Get() == Skill);
		if ((Pair.Key == ERogue10mAttackInputSlot::Primary && (!DefaultSkill || DefaultSkill->Get() != Skill))
			|| (Pair.Key != ERogue10mAttackInputSlot::Primary && (!Skill || !Skill->IsAllowedInLoadoutSlot(Pair.Key)
				|| !Profile || !Profile->CatalogAllowsLoadoutSkill(Skill)))
			|| !Skill || !RestoredNames.Contains(Skill->GetFName()) || !bProfileBinding
			|| Skill->SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique)
		{
			return false;
		}
		RestoredBindings.Add(Pair.Key, Skill);
	}
	if (const auto* Profile = FindActiveWeaponProfile())
	{
		auto* Basic = Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary);
		if (Basic && RestoredNames.Contains(Basic->GetFName())) { RestoredBindings.Add(ERogue10mAttackInputSlot::Primary, Basic); }
	}
	CancelWeaponTransitionState();
	TestUnlockedAttackSkillNames.Reset();
	UnlockedAttackSkillNames = MoveTemp(RestoredNames);
	UnlockedSkillAssets = MoveTemp(RestoredAssets);
	WeaponSkillPresets = MoveTemp(RestoredPresets);
	if (const auto* Preset = WeaponSkillPresets.Find(AppliedProfileWeaponType))
	{
		for (auto It = RestoredBindings.CreateIterator(); It; ++It) { if (URogue10mAttackSkillData::IsLoadoutSlot(It.Key())) { It.RemoveCurrent(); } }
		for (const auto& Pair : Preset->Slots) { RestoredBindings.Add(Pair.Key, Pair.Value); }
	}
	DurableSkillBindings = RestoredBindings;
	EquippedSkillBindings = MoveTemp(RestoredBindings);
	EvaluateActiveSkillUnlocks();
	OnSkillTreeChanged.Broadcast();
	return true;
}

void URogue10mCombatComponent::ResetCombatProgression()
{
	CancelWeaponTransitionState();
	UnlockedAttackSkillNames.Reset();
	TestUnlockedAttackSkillNames.Reset();
	UnlockedSkillAssets.Reset();
	WeaponSkillPresets.Reset();
	bWeaponPresetApplied = false;
	ApplyActiveWeaponProfile();
}
