// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rogue10mCharacterStats.generated.h"

/** 장비 한 개 또는 현재 장착 장비 전체가 제공하는 고정 능력치 증가량입니다. */
USTRUCT(BlueprintType)
struct FRogue10mEquipmentStatModifiers
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float AttackPowerBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float DefenseBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float MaxHealthBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float MaxStaminaBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float CriticalDamageMultiplierBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float MoveSpeedMultiplierBonus = 0.0f;

	/** 0.05는 치명타 확률 5%p 증가를 의미합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float CriticalChanceBonus = 0.0f;

	/** 0.05는 공격 속도 배율 5% 증가를 의미합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float AttackSpeedBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment")
	float MoveSpeedBonus = 0.0f;

	void Accumulate(const FRogue10mEquipmentStatModifiers& Other)
	{
		AttackPowerBonus += Other.AttackPowerBonus;
		DefenseBonus += Other.DefenseBonus;
		MaxHealthBonus += Other.MaxHealthBonus;
		MaxStaminaBonus += Other.MaxStaminaBonus;
		CriticalDamageMultiplierBonus += Other.CriticalDamageMultiplierBonus;
		MoveSpeedMultiplierBonus += Other.MoveSpeedMultiplierBonus;
		CriticalChanceBonus += Other.CriticalChanceBonus;
		AttackSpeedBonus += Other.AttackSpeedBonus;
		MoveSpeedBonus += Other.MoveSpeedBonus;
	}
};

/** Character Data Asset이 소유하는 장비 적용 전 기본 스탯입니다. */
USTRUCT(BlueprintType)
struct FRogue10mCharacterBaseStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Vitals")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Vitals")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Vitals")
	float MaxMana = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Combat")
	float AttackPower = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Combat")
	float Defense = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Combat")
	float CriticalChance = 0.05f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Combat")
	float CriticalDamageMultiplier = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Combat")
	float AttackSpeedMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Movement")
	float MoveSpeed = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Movement")
	float SprintSpeed = 900.0f;
};

/** 장비창과 런타임 적용에서 공유하는 기본·장비·최종 스탯 스냅샷입니다. */
USTRUCT(BlueprintType)
struct FRogue10mCharacterStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	FRogue10mCharacterBaseStats Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	FRogue10mEquipmentStatModifiers Equipment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float AttackPower = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float Defense = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float CriticalChance = 0.05f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float CriticalDamageMultiplier = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float AttackSpeedMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float MoveSpeed = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Character|Stats")
	float SprintSpeed = 900.0f;

	void Recalculate()
	{
		MaxHealth = FMath::Max(1.0f, Base.MaxHealth + Equipment.MaxHealthBonus);
		MaxStamina = FMath::Max(0.0f, Base.MaxStamina + Equipment.MaxStaminaBonus);
		AttackPower = FMath::Max(0.0f, Base.AttackPower + Equipment.AttackPowerBonus);
		Defense = FMath::Max(0.0f, Base.Defense + Equipment.DefenseBonus);
		CriticalChance = FMath::Clamp(Base.CriticalChance + Equipment.CriticalChanceBonus, 0.0f, 1.0f);
		CriticalDamageMultiplier = FMath::Max(1.0f, Base.CriticalDamageMultiplier + Equipment.CriticalDamageMultiplierBonus);
		AttackSpeedMultiplier = FMath::Clamp(
			Base.AttackSpeedMultiplier + Equipment.AttackSpeedBonus, 0.1f, 5.0f);
		const float MovementMultiplier = FMath::Max(0.0f, 1.0f + Equipment.MoveSpeedMultiplierBonus);
		MoveSpeed = FMath::Max(0.0f, Base.MoveSpeed + Equipment.MoveSpeedBonus) * MovementMultiplier;
		SprintSpeed = FMath::Max(0.0f, Base.SprintSpeed + Equipment.MoveSpeedBonus) * MovementMultiplier;
	}
};
