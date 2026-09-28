// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCombatComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mHitFeedbackComponent.h"
#include "Rogue10mAttackTargetInterface.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mGameplayAbility_Attack.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerFeedbackComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "TimerManager.h"

URogue10mCombatComponent::URogue10mCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultAttackAbilityClass = URogue10mGameplayAbility_Attack::StaticClass();
}

void URogue10mCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeSpawnedLoadout();
}

void URogue10mCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARogue10mPlayerState* ProgressionState = BoundProgressionState.Get())
	{
		ProgressionState->OnMartialArtsProgressChanged.RemoveDynamic(
			this, &URogue10mCombatComponent::HandleMartialArtsProgressChanged);
	}
	BoundProgressionState.Reset();
	CancelCombatVisuals();
	if (UWorld* World = GetWorld())
	{
		for (TPair<uint32, FRogue10mActiveAttackExecution>& Pair : ActiveAttackExecutions)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
		}
	}
	ActiveAttackExecutions.Reset();
	Super::EndPlay(EndPlayReason);
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

void URogue10mCombatComponent::HandleAttackPressed(bool bPrimaryAttack)
{
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
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

void URogue10mCombatComponent::CancelCombatVisuals()
{
	CancelPendingAttackHits();
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent())
		{
			Basic->CancelInput();
		}
	}
	LeftAttackPressedTime = -1.0f;
	RightAttackPressedTime = -1.0f;
	StopChargeEffect();
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
	PendingAbilityAttackSkill.Reset();
	bPendingAbilityComboAttack = false;
	return bExecuted;
}

bool URogue10mCombatComponent::ActivateQuickSlot(int32 SlotNumber)
{
	if (!CanUseCombatInput())
	{
		return false;
	}

	static constexpr ERogue10mAttackInputSlot Slots[] = {
		ERogue10mAttackInputSlot::Primary,
		ERogue10mAttackInputSlot::Special,
		ERogue10mAttackInputSlot::ChargedPrimary,
		ERogue10mAttackInputSlot::ChargedSpecial,
		ERogue10mAttackInputSlot::JumpPrimary,
		ERogue10mAttackInputSlot::JumpSpecial
	};
	if (!FMath::IsWithinInclusive(SlotNumber, 1, static_cast<int32>(UE_ARRAY_COUNT(Slots))))
	{
		return false;
	}

	const URogue10mAttackSkillData* EquippedSkill = GetEquippedSkill(Slots[SlotNumber - 1]);
	if (!EquippedSkill)
	{
		return false;
	}
	const URogue10mAttackSkillData& SkillData = *EquippedSkill;
	return TryActivateAttackAbility(SkillData, false) || ExecuteAttackSkill(SkillData, false);
}

void URogue10mCombatComponent::UnlockAttackSkill(URogue10mAttackSkillData* SkillData)
{
	if (!SkillData || UnlockedAttackSkillNames.Contains(SkillData->GetFName()))
	{
		return;
	}

	UnlockedAttackSkillNames.Add(SkillData->GetFName());
	OnSkillTreeChanged.Broadcast();
}

bool URogue10mCombatComponent::IsAttackSkillUnlocked(const URogue10mAttackSkillData* SkillData) const
{
	return SkillData && UnlockedAttackSkillNames.Contains(SkillData->GetFName());
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
	if (!IsAttackSkillUnlocked(SkillData) || !IsSkillInActiveTree(SkillData)
		|| SkillData->SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique)
	{
		return false;
	}

	for (TPair<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>>& Pair : EquippedSkillBindings)
	{
		if (Pair.Value == SkillData)
		{
			Pair.Value = nullptr;
		}
	}
	EquippedSkillBindings.Add(InputSlot, SkillData);
	return true;
}

bool URogue10mCombatComponent::UnassignSkillFromInputSlot(ERogue10mAttackInputSlot InputSlot)
{
	return EquippedSkillBindings.Remove(InputSlot) > 0;
}

URogue10mAttackSkillData* URogue10mCombatComponent::GetEquippedSkill(ERogue10mAttackInputSlot InputSlot) const
{
	if (const TObjectPtr<URogue10mAttackSkillData>* Skill = EquippedSkillBindings.Find(InputSlot))
	{
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

void URogue10mCombatComponent::HandleEquippedWeaponChanged()
{
	CancelWeaponTransitionState();
	ApplyActiveWeaponProfile();
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
			Character->SetEquippedWeaponType(CharacterData->DefaultWeaponType);
		}
	}
}
void URogue10mCombatComponent::ApplyActiveWeaponProfile()
{
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		if (URogue10mFirstPersonPresentationComponent* Presentation = Character->GetFirstPersonPresentationComponent())
		{
			Presentation->RefreshPresentation();
		}
	}
	EquippedSkillBindings.Reset();
	AppliedProfileWeaponType = ERogue10mWeaponType::Unarmed;
	const URogue10mWeaponSkillProfileDataAsset* Profile = FindActiveWeaponProfile();
	if (!Profile)
	{
		if (ARogue10mCharacter* Character = GetOwnerCharacter())
		{
			Character->JumpMaxCount = 1;
			UE_LOG(
				LogRogue10m, Warning,
				TEXT("%s 장착 무기 %s에 연결된 스킬 프로필이 없습니다."),
				*GetNameSafe(GetOwner()),
				*UEnum::GetValueAsString(Character->GetEquippedWeaponType()));
		}
		return;
	}

	AppliedProfileWeaponType = Profile->WeaponType;
	if (ARogue10mCharacter* Character = GetOwnerCharacter())
	{
		Character->JumpMaxCount = FMath::Max(1, Profile->MaxJumpCount);
	}
	for (const TPair<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>>& Pair
		: Profile->DefaultSkillBindings)
	{
		if (Pair.Value)
		{
			EquippedSkillBindings.Add(Pair.Key, Pair.Value);
			UnlockedAttackSkillNames.Add(Pair.Value->GetFName());
		}
	}
	for (URogue10mAttackSkillData* Skill : Profile->InitiallyUnlockedSkills)
	{
		UnlockAttackSkill(Skill);
	}
	EvaluateActiveSkillUnlocks();
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
	}
	BoundProgressionState = ProgressionState;
	if (ProgressionState)
	{
		ProgressionState->OnMartialArtsProgressChanged.AddUniqueDynamic(
			this, &URogue10mCombatComponent::HandleMartialArtsProgressChanged);
	}
}

void URogue10mCombatComponent::HandleMartialArtsProgressChanged()
{
	EvaluateActiveSkillUnlocks();
	OnSkillTreeChanged.Broadcast();
}

void URogue10mCombatComponent::CancelWeaponTransitionState()
{
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

TArray<const URogue10mAttackSkillData*> URogue10mCombatComponent::GetWeaponQuickSlotSkills() const
{
	TArray<const URogue10mAttackSkillData*> Skills;
	static constexpr ERogue10mAttackInputSlot Slots[] = {
		ERogue10mAttackInputSlot::Primary,
		ERogue10mAttackInputSlot::Special,
		ERogue10mAttackInputSlot::ChargedPrimary,
		ERogue10mAttackInputSlot::ChargedSpecial,
		ERogue10mAttackInputSlot::JumpPrimary,
		ERogue10mAttackInputSlot::JumpSpecial
	};
	for (ERogue10mAttackInputSlot Slot : Slots)
	{
		if (const URogue10mAttackSkillData* Skill = GetEquippedSkill(Slot))
		{
			Skills.AddUnique(Skill);
		}
	}
	return Skills;
}
const URogue10mAttackSkillData* URogue10mCombatComponent::GetSkillForInputSlot(ERogue10mAttackInputSlot InputSlot) const
{
	return GetEquippedSkill(InputSlot);
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
	return Attributes ? FMath::Clamp(Attributes->GetAttackSpeedMultiplier(), 0.1f, 5.0f) : 1.0f;
}

bool URogue10mCombatComponent::CanUseCombatInput() const
{
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const ARogue10mPlayerController* Controller = Character ? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	return Character && !Character->IsDead() && (!Controller || !Controller->IsAnyBlockingWindowVisible());
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
	ARogue10mCharacter* Character = GetOwnerCharacter();
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
	if (!Character || Character->IsDead() || !Camera || !GetWorld())
	{
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
	if (!CanPayResourceCosts(SkillData))
	{
		return false;
	}

	ConsumeResourceCosts(SkillData);
	StartSharedAttackCooldown(SkillData, bComboAttack);

	Character->PlayCommonMontage(
		SkillData.AttackMontage, GetAttackSpeedMultiplier() * SkillData.AnimationPlayRate);
	SpawnCastEffect(SkillData);

	if (URogue10mBasicBrawlerComponent* Basic = Character->GetBasicBrawlerComponent())
	{
		Basic->NotifySkillAccepted(SkillData);
	}
	StartAttackHitSequence(SkillData);
	OpenComboWindow(SkillData);
	if (ARogue10mPlayerState* ProgressionState = Character->GetPlayerState<ARogue10mPlayerState>())
	{
		ProgressionState->RecordSkillUse(SkillData.GetFName());
	}
	return true;
}

void URogue10mCombatComponent::StartAttackHitSequence(const URogue10mAttackSkillData& SkillData)
{
	if (!GetWorld())
	{
		return;
	}

	uint32 ExecutionId = NextAttackExecutionId++;
	if (ExecutionId == 0)
	{
		ExecutionId = NextAttackExecutionId++;
	}
	FRogue10mActiveAttackExecution& Execution = ActiveAttackExecutions.Add(ExecutionId);
	Execution.SkillData = &SkillData;
	const ARogue10mCharacter* Character = GetOwnerCharacter();
	const URogue10mBasicBrawlerComponent* Brawler = Character ? Character->FindComponentByClass<URogue10mBasicBrawlerComponent>() : nullptr;
	// Presentation belongs to the accepted attack, not to a weapon equipped between pulses.
	Execution.bBrawlerPresentation = Brawler && Brawler->IsBasicBrawlerActive();
	if (SkillData.HitStartDelaySeconds > 0.0f)
	{
		// Capture the same speed used by the accepted montage; later stat changes cannot move its first hit.
		const float Delay = SkillData.HitStartDelaySeconds
			/ FMath::Max(0.01f, GetAttackSpeedMultiplier() * SkillData.AnimationPlayRate);
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

void URogue10mCombatComponent::ExecuteAttackHitPulse(uint32 ExecutionId)
{
	FRogue10mActiveAttackExecution* Execution = ActiveAttackExecutions.Find(ExecutionId);
	const URogue10mAttackSkillData* SkillData = Execution ? Execution->SkillData.Get() : nullptr;
	ARogue10mCharacter* Character = GetOwnerCharacter();
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
	if (!Execution || !SkillData || !Character || Character->IsDead() || !Camera || !GetWorld())
	{
		FinishAttackHitSequence(ExecutionId);
		return;
	}

	if (Execution->CompletedPulses == 0 && SkillData->HitStartDelaySeconds > 0.0f)
	{
		if (!CanUseCombatInput()
			|| (Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
				&& Character->GetCharacterMovement()->IsFalling()))
		{
			FinishAttackHitSequence(ExecutionId);
			return;
		}
	}

	if (auto* AppearanceCamera = Character->GetAppearanceCameraComponent())
	{
		AppearanceCamera->UpdateCameraFromAppearance();
	}
	++Execution->CompletedPulses;
	const FVector Origin = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();
	TArray<AActor*> Targets;

	if (SkillData->HitMode == ERogue10mAttackHitMode::MultiHit && Execution->CompletedPulses > 1)
	{
		for (const TWeakObjectPtr<AActor>& LockedTarget : Execution->LockedTargets)
		{
			if (AActor* Target = LockedTarget.Get())
			{
				Targets.Add(Target);
			}
		}
	}
	else
	{
		GatherAttackTargets(*SkillData, *Execution, Origin, Forward, Camera->GetComponentQuat(), Targets);
		if (SkillData->HitMode == ERogue10mAttackHitMode::MultiHit)
		{
			Execution->LockedTargets.Reserve(Targets.Num());
			for (AActor* Target : Targets)
			{
				Execution->LockedTargets.Add(Target);
			}
		}
	}

	const int32 MaxTargets = FMath::Clamp(SkillData->MaxTargetsPerHit, 1, 64);
	int32 AppliedTargetCount = 0;
	for (AActor* Target : Targets)
	{
		if (Target && ApplyAttackDamage(*SkillData, *Execution, *Target, Forward, Execution->CompletedPulses))
		{
			if (++AppliedTargetCount >= MaxTargets)
			{
				break;
			}
		}
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

	switch (SkillData.AttackShape)
	{
	case ERogue10mAttackShape::LinearBox:
		QueryCenter = Origin + Forward * (SkillData.AttackRange * 0.5f);
		QueryShape = FCollisionShape::MakeBox(FVector(
			SkillData.AttackRange * 0.5f, SkillData.BoxHalfWidth, SkillData.BoxHalfHeight));
		QueryRotation = Rotation;
		break;
	case ERogue10mAttackShape::Projectile:
	{
		const int32 PulseCount = SkillData.HitMode == ERogue10mAttackHitMode::Single
			? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
		const float SegmentLength = SkillData.AttackRange / PulseCount;
		QueryCenter = Origin + Forward * (Execution.ProjectileTravelDistance + SegmentLength * 0.5f);
		Execution.ProjectileTravelDistance += SegmentLength;
		QueryShape = FCollisionShape::MakeBox(FVector(
			SegmentLength * 0.5f, SkillData.AttackTraceRadius, SkillData.AttackTraceRadius));
		QueryRotation = Rotation;
		break;
	}
	case ERogue10mAttackShape::Arc:
		QueryCenter = Origin;
		QueryShape = FCollisionShape::MakeSphere(SkillData.AttackRange);
		break;
	case ERogue10mAttackShape::Circle:
		QueryCenter = Origin + Forward * SkillData.CircleForwardOffset;
		QueryShape = FCollisionShape::MakeSphere(SkillData.AttackRange);
		break;
	default:
		return;
	}

	GetWorld()->OverlapMultiByObjectType(
		Overlaps, QueryCenter, QueryRotation, ObjectQueryParams, QueryShape, QueryParams);

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

	if (SkillData.bDrawDebugAttack)
	{
		const FColor Color = SkillData.DebugColor.ToFColor(true);
		if (SkillData.AttackShape == ERogue10mAttackShape::Arc)
		{
			DrawDebugCone(GetWorld(), Origin, Forward, SkillData.AttackRange,
				FMath::DegreesToRadians(SkillData.ArcAngleDegrees * 0.5f),
				FMath::DegreesToRadians(SkillData.ArcAngleDegrees * 0.5f), 24, Color, false, 1.2f);
		}
		else if (SkillData.AttackShape == ERogue10mAttackShape::Circle)
		{
			DrawDebugSphere(GetWorld(), QueryCenter, SkillData.AttackRange, 24, Color, false, 1.2f);
		}
		else
		{
			DrawDebugBox(GetWorld(), QueryCenter, QueryShape.GetExtent(), QueryRotation, Color, false, 1.2f);
		}
	}
}

bool URogue10mCombatComponent::ApplyAttackDamage(
	const URogue10mAttackSkillData& SkillData, FRogue10mActiveAttackExecution& Execution,
	AActor& TargetActor, const FVector& DamageDirection, int32 PulseIndex)
{
	ARogue10mCharacter* Character = GetOwnerCharacter();
	if (!Character || !TargetActor.GetClass()->ImplementsInterface(URogue10mAttackTargetInterface::StaticClass())
		|| !IRogue10mAttackTargetInterface::Execute_CanReceiveRogue10mAttack(&TargetActor, Character))
	{
		return false;
	}

	const int32 AllowedHits = SkillData.HitMode == ERogue10mAttackHitMode::Single
		? 1 : FMath::Clamp(SkillData.HitCount, 1, 64);
	if (Execution.TargetHitCounts.FindRef(&TargetActor) >= AllowedHits)
	{
		return false;
	}

	bool bCriticalHit = false;
	const float RolledDamage = SkillData.RollDamage(GetOwnerAttributes(), bCriticalHit);
	const float AppliedDamage = UGameplayStatics::ApplyDamage(
		&TargetActor, RolledDamage, Character->GetController(), Character, UDamageType::StaticClass());
	if (!FMath::IsFinite(AppliedDamage) || AppliedDamage <= 0.0f)
	{
		return false;
	}

	Execution.TargetHitCounts.FindOrAdd(&TargetActor) += 1;
	const URogue10mBasicBrawlerComponent* Brawler = Character->FindComponentByClass<URogue10mBasicBrawlerComponent>();
	const bool bBrawlerPresentation = Execution.bBrawlerPresentation && Brawler && Brawler->IsBasicBrawlerActive();
	SpawnImpactEffect(SkillData, TargetActor, bBrawlerPresentation);
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
	const float EffectScale = SkillData.CastEffectScale
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonCastScaleMultiplier : 1.0f);
	const float EmissionDuration = SkillData.CastEffectEmissionDuration
		* (bUseFirstPersonOverrides ? SkillData.FirstPersonEmissionDurationMultiplier : 1.0f);
	const FVector PrimaryOffset = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectOffset : FVector::ZeroVector;
	const FRotator EffectRotation = bUseFirstPersonOverrides
		? SkillData.FirstPersonEffectRotation : FRotator::ZeroRotator;

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
	const URogue10mAttackSkillData& SkillData, const AActor& TargetActor, bool bBrawlerPresentation) const
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
	if (bBrawlerPresentation && Character)
	{
		// Attack targeting is an overlap, so this is a conservative near-side bounds
		// estimate, not an exact physics contact point. Keep it at torso height.
		const FVector TowardAttacker = (Character->GetActorLocation() - BoundsOrigin).GetSafeNormal2D();
		EffectLocation += TowardAttacker * FMath::Min(BoundsExtent.X, BoundsExtent.Y);
		EffectRotation = (-TowardAttacker).Rotation();
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
			&& ActiveSkill->HitStartDelaySeconds > 0.0f && It.Value().CompletedPulses == 0)
		{
			if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(It.Value().TimerHandle); }
			It.RemoveCurrent();
		}
	}
}

void URogue10mCombatComponent::FinishAttackHitSequence(uint32 ExecutionId)
{
	if (FRogue10mActiveAttackExecution* Execution = ActiveAttackExecutions.Find(ExecutionId))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Execution->TimerHandle);
		}
	}
	ActiveAttackExecutions.Remove(ExecutionId);
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

bool URogue10mCombatComponent::CanPayResourceCosts(const URogue10mAttackSkillData& SkillData) const
{
	const URogue10mAttributeSet* Attributes = GetOwnerAttributes();
	if (!Attributes)
	{
		return false;
	}

	for (const FRogue10mAttackResourceCost& Cost : SkillData.ResourceCosts)
	{
		if (Cost.Cost <= 0.0f)
		{
			continue;
		}

		const bool bEnough = Cost.ResourceType == ERogue10mAttackResourceType::Health
			? Attributes->GetHealth() > Cost.Cost
			: Cost.ResourceType == ERogue10mAttackResourceType::Stamina
				? Attributes->GetStamina() >= Cost.Cost
				: Cost.ResourceType == ERogue10mAttackResourceType::Mana
					? Attributes->GetMana() >= Cost.Cost
					: Attributes->GetIdentity() >= Cost.Cost;
		if (!bEnough)
		{
			if (Cost.ResourceType == ERogue10mAttackResourceType::Stamina)
			{
				if (ARogue10mCharacter* Character = GetOwnerCharacter())
				{
					if (URogue10mPlayerFeedbackComponent* Feedback = Character->GetPlayerFeedbackComponent())
					{
						Feedback->NotifyInsufficientStamina();
					}
				}
			}
			AddCombatLog(
				FString::Printf(TEXT("%s 사용 불가: 자원이 부족합니다."), *SkillData.SkillName.ToString()),
				FLinearColor(1.0f, 0.42f, 0.24f, 1.0f));
			return false;
		}
	}
	return true;
}

void URogue10mCombatComponent::ConsumeResourceCosts(const URogue10mAttackSkillData& SkillData)
{
	URogue10mAttributeSet* Attributes = GetOwnerAttributes();
	if (!Attributes)
	{
		return;
	}

	for (const FRogue10mAttackResourceCost& Cost : SkillData.ResourceCosts)
	{
		switch (Cost.ResourceType)
		{
		case ERogue10mAttackResourceType::Health:
			Attributes->ConsumeHealth(Cost.Cost);
			break;
		case ERogue10mAttackResourceType::Stamina:
			Attributes->ConsumeStamina(Cost.Cost);
			break;
		case ERogue10mAttackResourceType::Mana:
			Attributes->ConsumeMana(Cost.Cost);
			break;
		case ERogue10mAttackResourceType::Energy:
			Attributes->ConsumeIdentity(Cost.Cost);
			break;
		default:
			break;
		}
	}
}

void URogue10mCombatComponent::StartSharedAttackCooldown(const URogue10mAttackSkillData& SkillData, bool bComboAttack)
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

	AttackCooldownSourceSkill = CooldownSource;
	const float AttackSpeed = GetAttackSpeedMultiplier();
	AttackCooldownDuration = FMath::Max(0.0f, CooldownSource->AttackCooldown) / AttackSpeed;
	const bool bHasNextCombo = bAllowAttackCombo && SkillData.bEnableCombo && SkillData.NextComboSkill;
	const float StartDelay = bHasNextCombo
		? FMath::Max(SkillData.ComboWindowOpenSeconds, SkillData.ComboWindowCloseSeconds) / AttackSpeed
		: 0.0f;
	AttackCooldownStartTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, StartDelay);
	AttackCooldownEndTime = AttackCooldownStartTime + AttackCooldownDuration;
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

void URogue10mCombatComponent::DrawAttackDebug(
	const FVector& TraceStart, const FVector& TraceEnd, float TraceRadius,
	const FLinearColor& Color, bool bHit, const FHitResult& Hit) const
{
	if (!bDrawAttackDebug || !GetWorld())
	{
		return;
	}

	const FColor DrawColor = Color.ToFColor(true);
	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, DrawColor, false, 1.2f, 0, 2.0f);
	DrawDebugSphere(GetWorld(), TraceEnd, TraceRadius, 16, DrawColor, false, 1.2f, 0, 1.5f);
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, TraceRadius * 1.25f, 16, FColor::Red, false, 1.2f, 0, 2.5f);
	}
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
