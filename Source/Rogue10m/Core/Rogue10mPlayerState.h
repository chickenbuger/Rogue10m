// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Rogue10mIdentityTypes.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mPlayerState.generated.h"

class UAbilitySystemComponent;
class URogue10mAbilitySystemComponent;
class URogue10mAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mMartialArtsProgressChanged);

UCLASS()
class ROGUE10M_API ARogue10mPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARogue10mPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|GAS")
	URogue10mAbilitySystemComponent* GetRogueAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|GAS")
	URogue10mAttributeSet* GetRogueAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character")
	FText GetCharacterDisplayName() const { return CharacterDisplayName; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character")
	FText GetCharacterJobName() const { return CharacterJobName; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Character")
	void SetCharacterProfileIdentity(const FText& DisplayName, const FText& JobName);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat")
	ERogue10mWeaponType GetEquippedWeaponType() const { return EquippedWeaponType; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void SetEquippedWeaponType(ERogue10mWeaponType NewWeaponType);

	UFUNCTION(BlueprintPure, Category="Rogue10m|State")
	bool IsCharacterDead() const { return bCharacterDead; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|State")
	void SetCharacterDead(bool bNewCharacterDead);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression")
	int32 GetPlayerLevel() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression")
	float GetCurrentExperience() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression")
	float GetExperienceToNextLevel() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression")
	float GetExperienceNormalized() const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Progression")
	void AddExperience(int32 ExperienceAmount);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Progression|Martial Arts")
	void RecordSkillUse(FName SkillId, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Progression|Martial Arts")
	void RecordMonsterDefeat(FName MonsterId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression|Martial Arts")
	int32 GetSkillUseCount(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Progression|Martial Arts")
	int32 GetMonsterDefeatCount(FName MonsterId) const;

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Progression|Martial Arts")
	FRogue10mMartialArtsProgressChanged OnMartialArtsProgressChanged;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	int32 GetWeaponMasteryLevel(ERogue10mWeaponType WeaponType) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	float GetIdentityCurrent() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	float GetIdentityMax() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	float GetIdentityNormalized() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	ERogue10mIdentityType GetIdentityType() const { return IdentityType; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Identity")
	void SetIdentityType(ERogue10mIdentityType NewIdentityType);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Identity")
	bool HasIdentityResource() const { return IdentityType != ERogue10mIdentityType::None; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Vitals")
	bool IsManaEnabled() const { return bManaEnabled; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Vitals")
	void SetManaEnabled(bool bEnabled) { bManaEnabled = bEnabled; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|GAS", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|GAS", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URogue10mAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character", meta=(AllowPrivateAccess="true"))
	FText CharacterDisplayName = FText::FromString(TEXT("로그"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character", meta=(AllowPrivateAccess="true"))
	FText CharacterJobName = FText::FromString(TEXT("모험가"));

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(AllowPrivateAccess="true"))
	ERogue10mWeaponType EquippedWeaponType = ERogue10mWeaponType::Unarmed;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|State", meta=(AllowPrivateAccess="true"))
	bool bCharacterDead = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(AllowPrivateAccess="true"))
	ERogue10mIdentityType IdentityType = ERogue10mIdentityType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Progression|Martial Arts", meta=(AllowPrivateAccess="true"))
	TMap<FName, int32> SkillUseCounts;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Progression|Martial Arts", meta=(AllowPrivateAccess="true"))
	TMap<FName, int32> MonsterDefeatCounts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Vitals", meta=(AllowPrivateAccess="true"))
	bool bManaEnabled = false;
};