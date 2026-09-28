// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "Rogue10mCharacterStats.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UInputAction;
class URogue10mAttackSkillData;
class URogue10mAttributeSet;
class URogue10mCharacterAnimationComponent;
class URogue10mCombatComponent;
class URogue10mBasicBrawlerComponent;
class URogue10mFirstPersonPresentationComponent;
class URogue10mAppearanceCameraComponent;
class URogue10mInventoryComponent;
class URogue10mPlayerFeedbackComponent;
class URogue10mVitalRegenerationComponent;
class USkeletalMeshComponent;
struct FInputActionValue;

/** 1인칭 이동과 입력 전달, 캐릭터 생명주기를 담당합니다. */
UCLASS(abstract)
class ROGUE10M_API ARogue10mCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARogue10mCharacter();
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|State")
	virtual void Die();

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|State", meta=(DisplayName="On Run Death"))
	void BP_OnRunDeath();

	UFUNCTION(BlueprintPure, Category="Rogue10m|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character")
	FText GetCharacterDisplayName() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character")
	FText GetCharacterJobName() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat")
	ERogue10mWeaponType GetEquippedWeaponType() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void SetEquippedWeaponType(ERogue10mWeaponType NewWeaponType);

	UFUNCTION(BlueprintPure, Category="Rogue10m|GAS")
	URogue10mAttributeSet* GetRogueAttributeSet() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character|Stats")
	FRogue10mCharacterStatSnapshot GetCharacterStatSnapshot() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Character|Customization")
	virtual bool ApplyCharacterProfile(const FRogue10mCharacterProfile& Profile);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character|Customization")
	FRogue10mCharacterAppearance GetCharacterAppearance() const { return CharacterAppearance; }

	/** Character Data 기본값과 현재 장비 보너스를 최종 런타임 수치에 반영합니다. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Character|Stats")
	void RefreshCharacterStats(bool bRestoreVitals = false);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Movement")
	bool IsSprinting() const { return bIsSprinting; }

	URogue10mAppearanceCameraComponent* GetAppearanceCameraComponent() const { return AppearanceCameraComponent; }
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	URogue10mFirstPersonPresentationComponent* GetFirstPersonPresentationComponent() const { return FirstPersonPresentationComponent; }
	URogue10mInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	URogue10mCombatComponent* GetCombatComponent() const { return CombatComponent; }
	URogue10mBasicBrawlerComponent* GetBasicBrawlerComponent() const { return BasicBrawlerComponent; }
	URogue10mCharacterAnimationComponent* GetCharacterAnimationComponent() const { return CharacterAnimationComponent; }
	URogue10mPlayerFeedbackComponent* GetPlayerFeedbackComponent() const { return PlayerFeedbackComponent; }

	/** 공통 Manny Skeleton 모션을 평가하는 Mesh입니다. 종족 캐릭터는 숨김 소스 Mesh를 반환합니다. */
	virtual USkeletalMeshComponent* GetAnimationPlaybackMesh() const;

	/** 공통 소스 Mesh에서 재생하며, 구형 1인칭 표현이 활성일 때만 복제 재생합니다. */
	bool PlayCommonMontage(class UAnimMontage* Montage, float PlayRate = 1.0f);

	const URogue10mAttackSkillData* GetDisplayedAttackSkillForHUD() const;
	float GetAttackCooldownRemaining() const;
	float GetAttackCooldownDuration() const;
	bool ExecutePendingAttackSkillFromAbility();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Movement")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Movement")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Movement|Dodge")
	virtual void DoDodge();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Movement")
	virtual void DoSprintStart();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Movement")
	virtual void DoSprintEnd();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	virtual void DoPrimaryAttackPressed();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	virtual void DoPrimaryAttackReleased();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	virtual void DoSpecialAttackPressed();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	virtual void DoSpecialAttackReleased();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	virtual void DoUnarmedAttack();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory")
	virtual void DoToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory")
	virtual void DoToggleItemWindow();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	virtual void DoToggleSkillTree();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Settings")
	virtual void DoToggleSettings();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Debug")
	virtual void DoToggleCombatLog();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	virtual void DoQuickSlot1();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	virtual void DoQuickSlot2();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	virtual void DoQuickSlot3();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	virtual void DoQuickSlot4();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	virtual void DoQuickSlot5();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mAppearanceCameraComponent> AppearanceCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mBasicBrawlerComponent> BasicBrawlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mCharacterAnimationComponent> CharacterAnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mFirstPersonPresentationComponent> FirstPersonPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mPlayerFeedbackComponent> PlayerFeedbackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mVitalRegenerationComponent> VitalRegenerationComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Sprint", meta=(ClampMin="0.0", Units="cm/s"))
	float SprintWalkSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Sprint", meta=(ClampMin="0.0"))
	float SprintStaminaCostPerSecond = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Sprint", meta=(ClampMin="0.05", ClampMax="1.0", Units="s"))
	float SprintStaminaDrainInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Dodge", meta=(ClampMin="0.0", Units="cm"))
	float DodgeDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Dodge", meta=(ClampMin="0.05", ClampMax="1.0", Units="s"))
	float DodgeDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Movement|Dodge", meta=(ClampMin="0.0", Units="s"))
	float DodgeCooldown = 0.45f;

	UPROPERTY(Transient)
	FRogue10mCharacterAppearance CharacterAppearance;

private:
	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);
	void SetSprinting(bool bNewSprinting);
	void ConsumeSprintStamina();
	void FinishDodge();
	bool IsBlockingWindowVisible() const;
	void ActivateQuickSlot(int32 SlotNumber);
	void InitializeAbilityActorInfo();

	FTimerHandle SprintStaminaTimerHandle;
	FTimerHandle DodgeTimerHandle;
	FVector2D CachedMovementInput = FVector2D::ZeroVector;
	float NormalWalkSpeed = 600.0f;
	float NextDodgeAllowedTime = 0.0f;
	bool bIsSprinting = false;
	bool bIsDodging = false;
};
