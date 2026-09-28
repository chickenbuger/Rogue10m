// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mCombatComponent.generated.h"

class ARogue10mCharacter;
class ARogue10mPlayerState;
class UNiagaraComponent;
class UNiagaraSystem;
class URogue10mAttributeSet;
class URogue10mBasicBrawlerComponent;
class URogue10mCharacterDataAsset;
class URogue10mDodgeSkillDataAsset;
class URogue10mWeaponSkillProfileDataAsset;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mSkillTreeChanged);

struct FRogue10mActiveAttackExecution
{
	TWeakObjectPtr<const URogue10mAttackSkillData> SkillData;
	TMap<TWeakObjectPtr<AActor>, int32> TargetHitCounts;
	TArray<TWeakObjectPtr<AActor>> LockedTargets;
	FTimerHandle TimerHandle;
	float ProjectileTravelDistance = 0.0f;
	int32 CompletedPulses = 0;
	bool bConfirmedCameraFeedbackSent = false;
	bool bBrawlerPresentation = false;
};

UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mCombatComponent();

	void InitializeAbilitySystem();
	void InitializeSpawnedLoadout();
	void HandleAttackPressed(bool bPrimaryAttack);
	void HandleAttackReleased(bool bPrimaryAttack);
	void CancelCombatVisuals();
	bool ExecutePendingAttackSkillFromAbility();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	bool ActivateQuickSlot(int32 SlotNumber);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat|Skill Tree")
	void UnlockAttackSkill(URogue10mAttackSkillData* SkillData);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	bool IsAttackSkillUnlocked(const URogue10mAttackSkillData* SkillData) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	bool IsSkillInActiveTree(const URogue10mAttackSkillData* SkillData) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat")
	TArray<URogue10mAttackSkillData*> GetUnlockedWeaponSkills() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat|Loadout")
	bool AssignSkillToInputSlot(URogue10mAttackSkillData* SkillData, ERogue10mAttackInputSlot InputSlot);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat|Loadout")
	bool UnassignSkillFromInputSlot(ERogue10mAttackInputSlot InputSlot);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Loadout")
	URogue10mAttackSkillData* GetEquippedSkill(ERogue10mAttackInputSlot InputSlot) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	TArray<URogue10mAttackSkillData*> GetActiveSkillTreeSkills() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	ERogue10mWeaponType GetActiveSkillTreeWeaponType() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	FText GetActiveSkillTreeDisplayName() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	bool IsSkillAvailableToUnlock(const URogue10mAttackSkillData* SkillData) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	float GetSkillUnlockProgress(const URogue10mAttackSkillData* SkillData) const;

	int32 GetSkillUnlockConditionCurrent(const FRogue10mSkillUnlockCondition& Condition) const;
	bool IsSkillUnlockConditionComplete(const FRogue10mSkillUnlockCondition& Condition) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Skill Tree")
	float GetUnlockedPassiveDamageReduction() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat|Skill Tree")
	void EvaluateActiveSkillUnlocks();

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Combat|Skill Tree")
	FRogue10mSkillTreeChanged OnSkillTreeChanged;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Dodge")
	const URogue10mDodgeSkillDataAsset* GetActiveDodgeSkill() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Character Data")
	const URogue10mCharacterDataAsset* GetCharacterData() const { return CharacterData; }

	void HandleEquippedWeaponChanged();
	const URogue10mAttackSkillData* ResolveAttackSkill(bool bPrimaryAttack, bool bChargedAttack, bool bJumpAttack) const;
	const URogue10mAttackSkillData* ResolveChargedAttackSkill(bool bPrimaryAttack, bool bJumpAttack) const;
	const URogue10mAttackSkillData* ResolveComboAttackSkill(bool bPrimaryAttack, bool bJumpAttack) const;
	TArray<const URogue10mAttackSkillData*> GetWeaponQuickSlotSkills() const;
	const URogue10mAttackSkillData* GetSkillForInputSlot(ERogue10mAttackInputSlot InputSlot) const;
	ERogue10mAttackInputSlot GetAttackInputSlot(bool bPrimaryAttack, bool bChargedAttack, bool bJumpAttack) const;

	bool IsAttackOnCooldown(float CurrentTime) const;
	bool IsComboSequenceActive(float CurrentTime) const;
	float GetAttackCooldownRemaining() const;
	float GetAttackCooldownDuration() const { return AttackCooldownDuration; }
	const URogue10mAttackSkillData* GetDisplayedAttackSkill() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class URogue10mBasicBrawlerComponent;
	ARogue10mCharacter* GetOwnerCharacter() const;
	URogue10mAttributeSet* GetOwnerAttributes() const;
	float GetAttackSpeedMultiplier() const;
	bool CanUseCombatInput() const;
	void ExecuteCombatAttack(bool bPrimaryAttack, bool bChargedAttack);
	bool ExecuteAttackSkill(const URogue10mAttackSkillData& SkillData, bool bComboAttack);
	bool TryActivateAttackAbility(const URogue10mAttackSkillData& SkillData, bool bComboAttack);
	void StartAttackHitSequence(const URogue10mAttackSkillData& SkillData);
	void ExecuteAttackHitPulse(uint32 ExecutionId);
	void GatherAttackTargets(const URogue10mAttackSkillData& SkillData, FRogue10mActiveAttackExecution& Execution,
		const FVector& Origin, const FVector& Forward, const FQuat& Rotation, TArray<AActor*>& OutTargets) const;
	bool ApplyAttackDamage(const URogue10mAttackSkillData& SkillData, FRogue10mActiveAttackExecution& Execution,
		AActor& TargetActor, const FVector& DamageDirection, int32 PulseIndex);
	void FinishAttackHitSequence(uint32 ExecutionId);
	void CancelPendingAttackHits(const URogue10mAttackSkillData* SkillData = nullptr);
	bool CanPayResourceCosts(const URogue10mAttackSkillData& SkillData) const;
	void ConsumeResourceCosts(const URogue10mAttackSkillData& SkillData);
	void StartSharedAttackCooldown(const URogue10mAttackSkillData& SkillData, bool bComboAttack);
	void OpenComboWindow(const URogue10mAttackSkillData& SkillData);
	void ResetComboWindow();
	void DrawAttackDebug(const FVector& TraceStart, const FVector& TraceEnd, float TraceRadius, const FLinearColor& Color, bool bHit, const FHitResult& Hit) const;
	void AddCombatLog(const FString& Message, const FLinearColor& Color = FLinearColor::White) const;
	FString GetAttackInputText(bool bPrimaryAttack, bool bJumpAttack) const;
	const URogue10mWeaponSkillProfileDataAsset* FindActiveWeaponProfile() const;
	void ApplyCharacterData();
	void ApplyActiveWeaponProfile();
	void BindProgressionState();

	UFUNCTION()
	void HandleMartialArtsProgressChanged();

	void CancelWeaponTransitionState();
	void StartChargeEffect(const URogue10mAttackSkillData& SkillData);
	void StopChargeEffect();
	void SpawnCastEffect(const URogue10mAttackSkillData& SkillData) const;
	void SpawnImpactEffect(const URogue10mAttackSkillData& SkillData, const AActor& TargetActor, bool bBrawlerPresentation) const;
	void ConfigureTransientEffect(
		UNiagaraComponent* EffectComponent, float UniformScale,
		float EmissionDuration, float TimeDilation) const;
	USkeletalMeshComponent* GetEffectAttachMesh(bool& bOutFirstPerson) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> PrimaryAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> SpecialAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> JumpPrimaryAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> JumpSpecialAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> ChargedPrimaryAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttackSkillData> ChargedSpecialAttackSkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Combo", meta=(AllowPrivateAccess="true"))
	bool bAllowAttackCombo = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Debug", meta=(AllowPrivateAccess="true"))
	bool bDrawAttackDebug = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Charge", meta=(AllowPrivateAccess="true", ClampMin="0.05"))
	float DefaultChargeThreshold = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|GAS", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UGameplayAbility> DefaultAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Tree", meta=(AllowPrivateAccess="true"))
	TSet<FName> UnlockedAttackSkillNames;

	/** 캐릭터의 기본 전투 프로필을 묶는 GDD Character Data Asset입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Character Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mCharacterDataAsset> CharacterData;

	/** CharacterData가 비어 있을 때 사용할 수 있는 직접 지정 fallback입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat|Skill Loadout", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<URogue10mWeaponSkillProfileDataAsset>> WeaponSkillProfiles;

	UPROPERTY(Transient)
	TMap<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>> EquippedSkillBindings;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveChargeEffectComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveOffHandChargeEffectComponent;

	ERogue10mWeaponType AppliedProfileWeaponType = ERogue10mWeaponType::Unarmed;
	float LeftAttackPressedTime = -1.0f;
	float RightAttackPressedTime = -1.0f;
	TWeakObjectPtr<const URogue10mAttackSkillData> ActiveComboSourceSkill;
	TWeakObjectPtr<const URogue10mAttackSkillData> ActiveComboRootSkill;
	TWeakObjectPtr<const URogue10mAttackSkillData> AttackCooldownSourceSkill;
	TWeakObjectPtr<const URogue10mAttackSkillData> PendingAbilityAttackSkill;
	TWeakObjectPtr<ARogue10mPlayerState> BoundProgressionState;
	float ActiveComboWindowOpenTime = -1.0f;
	float ActiveComboWindowCloseTime = -1.0f;
	float AttackCooldownStartTime = -1.0f;
	float AttackCooldownEndTime = -1.0f;
	float AttackCooldownDuration = 0.0f;
	bool bPendingAbilityComboAttack = false;
	bool bExecutingAttackFromAbility = false;
	uint32 NextAttackExecutionId = 1;
	TMap<uint32, FRogue10mActiveAttackExecution> ActiveAttackExecutions;
};
