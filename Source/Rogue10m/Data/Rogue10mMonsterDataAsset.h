// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mMonsterDataAsset.generated.h"

class UAnimInstance;
class UBehaviorTree;
class USkeletalMesh;
class UTexture2D;
class URogue10mAttackSkillData;
class URogue10mMonsterAttackPatternProfile;

UENUM(BlueprintType)
enum class ERogue10mMonsterRank : uint8
{
	Normal UMETA(DisplayName="일반"),
	MidBoss UMETA(DisplayName="중간 보스"),
	FinalBoss UMETA(DisplayName="최종 보스")
};

/** 몬스터의 외형, 능력치, 보상과 전투 설정을 정의합니다. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mMonsterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity")
	FName MonsterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity", meta=(ClampMin="1"))
	int32 Level = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity")
	ERogue10mMonsterRank MonsterRank = ERogue10mMonsterRank::Normal;

	/** 기존 에셋 호환 필드입니다. 신규 로직은 MonsterRank를 기준으로 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity")
	bool bIsBoss = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Identity")
	TSoftObjectPtr<UTexture2D> PortraitIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Rewards", meta=(ClampMin="0"))
	int32 ExperienceReward = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Vitals", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Vitals", meta=(ClampMin="0.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Vitals", meta=(ClampMin="0.0"))
	float MaxMana = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Regeneration", meta=(ClampMin="0.0"))
	float HealthRegenerationPerSecond = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Regeneration", meta=(ClampMin="0.0"))
	float StaminaRegenerationPerSecond = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Regeneration", meta=(ClampMin="0.0"))
	float ManaRegenerationPerSecond = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Visual")
	TSubclassOf<UAnimInstance> AnimationBlueprintClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Visual")
	FVector MeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Visual")
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Visual")
	FVector MeshRelativeScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Movement", meta=(ClampMin="0.0"))
	float WalkSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Movement", meta=(ClampMin="0.0"))
	float DetectionRange = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Movement", meta=(ClampMin="0.0"))
	float StopDistance = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float LoseSightRange = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.1", Units="s"))
	float ForgetTargetSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float PatrolRadius = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0", Units="s"))
	float PatrolWaitSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|AI", meta=(ClampMin="0.0"))
	float MaxChaseDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Combat")
	TObjectPtr<URogue10mAttackSkillData> AttackSkill;

	/** Optional timed attacks and presentation; unset preserves legacy behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Combat")
	TObjectPtr<URogue10mMonsterAttackPatternProfile> AttackPatternProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.0"))
	float AttackRange = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.0"))
	float AttackDamage = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Combat", meta=(ClampMin="0.01"))
	float AttackInterval = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Monster|Lifecycle")
	bool bDestroyOnDeath = true;
};
