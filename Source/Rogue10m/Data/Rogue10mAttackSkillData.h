// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mAttackSkillData.generated.h"

class UAnimMontage;
class UGameplayAbility;
class UTexture2D;
class UNiagaraSystem;
class URogue10mAttributeSet;
class URogue10mAttackSkillData;

UENUM(BlueprintType)
enum class ERogue10mAttackInputSlot : uint8
{
	Primary,
	Special,
	JumpPrimary,
	JumpSpecial,
	ChargedPrimary,
	ChargedSpecial
};

UENUM(BlueprintType)
enum class ERogue10mAttackResourceType : uint8
{
	Health,
	Stamina,
	Mana,
	Energy
};

UENUM(BlueprintType)
enum class ERogue10mAttackShape : uint8
{
	LinearBox UMETA(DisplayName="직선 박스"),
	Projectile UMETA(DisplayName="투사체 경로"),
	Arc UMETA(DisplayName="부채꼴"),
	Circle UMETA(DisplayName="원형")
};

UENUM(BlueprintType)
enum class ERogue10mAttackHitMode : uint8
{
	Single UMETA(DisplayName="단타"),
	Continuous UMETA(DisplayName="연속 공격"),
	MultiHit UMETA(DisplayName="적중 후 다단히트")
};

UENUM(BlueprintType)
enum class ERogue10mSkillTreeNodeType : uint8
{
	ActiveTechnique UMETA(DisplayName="공격 무공"),
	PassiveTechnique UMETA(DisplayName="상시 심법")
};

UENUM(BlueprintType)
enum class ERogue10mSkillUnlockConditionType : uint8
{
	SkillUseCount UMETA(DisplayName="기술 수련"),
	MonsterDefeatCount UMETA(DisplayName="몬스터 토벌")
};

USTRUCT(BlueprintType)
struct FRogue10mSkillUnlockCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	ERogue10mSkillUnlockConditionType ConditionType = ERogue10mSkillUnlockConditionType::SkillUseCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree",
		meta=(EditCondition="ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount", EditConditionHides))
	TObjectPtr<URogue10mAttackSkillData> RequiredSkill;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree",
		meta=(EditCondition="ConditionType == ERogue10mSkillUnlockConditionType::MonsterDefeatCount", EditConditionHides))
	FName RequiredMonsterId;

	/** UI에 표시할 수련 대상 또는 무협식 토벌 대상의 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	FText TargetDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="1"))
	int32 RequiredCount = 1;
};

USTRUCT(BlueprintType)
struct FRogue10mAttackResourceCost
{
	GENERATED_BODY()

	FRogue10mAttackResourceCost() = default;

	FRogue10mAttackResourceCost(ERogue10mAttackResourceType InResourceType, float InCost)
		: ResourceType(InResourceType)
		, Cost(InCost)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Cost")
	ERogue10mAttackResourceType ResourceType = ERogue10mAttackResourceType::Stamina;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Cost", meta=(ClampMin="0.0"))
	float Cost = 0.0f;
};

UCLASS(BlueprintType)
class ROGUE10M_API URogue10mAttackSkillData : public UDataAsset
{
	GENERATED_BODY()

public:
	URogue10mAttackSkillData();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat|Damage")
	float RollDamage(const URogue10mAttributeSet* SourceAttributes, bool& bOutCritical) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill")
	FText SkillName = FText::FromString(TEXT("공격 스킬"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill", meta=(MultiLine="true"))
	FText SkillDescription = FText::FromString(TEXT("공격 스킬 설명을 입력하세요."));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill")
	ERogue10mAttackInputSlot InputSlot = ERogue10mAttackInputSlot::Primary;

	/** 스킬트리에서 공격 슬롯에 장착하는 무공인지, 해금 즉시 적용되는 심법인지 구분합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	ERogue10mSkillTreeNodeType SkillTreeNodeType = ERogue10mSkillTreeNodeType::ActiveTechnique;

	/** 입문(1), 초식(2), 절기(3), 심법(4) 단계입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="1", ClampMax="4"))
	int32 SkillTreeTier = 1;

	/** 스킬트리 중앙 Canvas 안에서 노드 중심의 픽셀 위치입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	FVector2D SkillTreePosition = FVector2D(425.0f, 560.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TArray<TObjectPtr<URogue10mAttackSkillData>> PrerequisiteSkills;

	/** 배열의 모든 수련/토벌 조건을 만족해야 자동 해금됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TArray<FRogue10mSkillUnlockCondition> UnlockConditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(MultiLine="true"))
	FText UnlockRewardText;

	/** 상시 심법 해금 시 받는 피해 감소 비율입니다. 0.15는 15%입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree|Passive",
		meta=(EditCondition="SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique", EditConditionHides, ClampMin="0.0", ClampMax="0.8"))
	float PassiveDamageReduction = 0.0f;

	/** 이 스킬 Data Asset을 실행할 GAS Ability입니다. 비어 있으면 캐릭터의 기본 공격 Ability를 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|GAS")
	TSubclassOf<UGameplayAbility> GameplayAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(ClampMin="0.0"))
	float Damage = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Shape")
	ERogue10mAttackShape AttackShape = ERogue10mAttackShape::LinearBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Hit Mode")
	ERogue10mAttackHitMode HitMode = ERogue10mAttackHitMode::Single;
	/** 캐릭터 최소 피해 비율에 곱하는 스킬 보정값입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Damage", meta=(ClampMin="0.0"))
	float MinDamageRatioMultiplier = 1.0f;

	/** 캐릭터 최대 피해 비율에 곱하는 스킬 보정값입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Damage", meta=(ClampMin="0.0"))
	float MaxDamageRatioMultiplier = 1.0f;
	/** 캐릭터 치명타 확률에 더하는 스킬 고유 보정입니다. 0.1은 10%p 증가입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Critical", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float CriticalChanceBonus = 0.0f;

	/** 캐릭터 치명타 피해 배율에 더하는 스킬 고유 보정입니다. 0.2는 150%를 170%로 만듭니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Critical", meta=(ClampMin="-10.0", ClampMax="10.0"))
	float CriticalDamageMultiplierBonus = 0.0f;

	/** 공격 한 번을 구성하는 판정 펄스 수입니다. 1이면 기존 단일 타격과 같습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Multi Hit", meta=(ClampMin="1", ClampMax="64"))
	int32 HitCount = 1;

	/** Initial hit time in montage seconds; zero preserves immediate legacy attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Timing", meta=(ClampMin="0.0", Units="s"))
	float HitStartDelaySeconds = 0.0f;

	/** 다단히트 판정 펄스 사이의 간격입니다. 공격속도 배율에 따라 짧아집니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Multi Hit", meta=(EditCondition="HitCount > 1", ClampMin="0.01", Units="s"))
	float HitInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Multi Hit", meta=(ClampMin="1", ClampMax="64"))
	int32 MaxTargetsPerHit = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Multi Hit", meta=(ClampMin="1", ClampMax="64"))
	int32 MaxHitsPerTarget = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(ClampMin="1.0", Units="cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(ClampMin="1.0", Units="cm"))
	float AttackTraceRadius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Shape", meta=(ClampMin="1.0", Units="cm"))
	float BoxHalfWidth = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Shape", meta=(ClampMin="1.0", Units="cm"))
	float BoxHalfHeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Shape", meta=(ClampMin="1.0", ClampMax="180.0", Units="deg"))
	float ArcAngleDegrees = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat|Shape", meta=(ClampMin="0.0", Units="cm"))
	float CircleForwardOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(ClampMin="0.0", Units="s"))
	float AttackCooldown = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combat", meta=(ClampMin="0.0", Units="s"))
	float ChargeSeconds = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Cost")
	TArray<FRogue10mAttackResourceCost> ResourceCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combo")
	bool bEnableCombo = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combo", meta=(EditCondition="bEnableCombo", ClampMin="0.0", Units="s"))
	float ComboWindowOpenSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combo", meta=(EditCondition="bEnableCombo", ClampMin="0.0", Units="s"))
	float ComboWindowCloseSeconds = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combo", meta=(EditCondition="bEnableCombo"))
	ERogue10mAttackInputSlot ComboInputSlot = ERogue10mAttackInputSlot::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Combo", meta=(EditCondition="bEnableCombo"))
	TObjectPtr<URogue10mAttackSkillData> NextComboSkill;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 무기별 동작 무게감을 조절하는 Montage 재생 속도입니다. 캐릭터 공격속도 배율과 곱해집니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Animation", meta=(ClampMin="0.1", ClampMax="3.0"))
	float AnimationPlayRate = 1.0f;

	/** 애니메이션 단독 프리뷰에서는 끄고, 공격 파티클 확인 시에만 켭니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	bool bEnableAttackEffects = false;

	/** 공격 동작이 시작될 때 손 소켓에 재생할 UE 5.8 Niagara Cue입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	TObjectPtr<UNiagaraSystem> CastEffect;

	/** 차징 입력을 누르는 동안 손 소켓에 부착할 Niagara Cue입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	TObjectPtr<UNiagaraSystem> ChargeEffect;

	/** 실제 피해가 적용된 대상 위치에 재생할 Niagara Cue입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	FName EffectAttachSocket = TEXT("hand_r");

	/** 쌍수 무기는 같은 Cue를 보조 손 소켓에도 생성합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara")
	bool bSpawnEffectOnOffHand = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara", meta=(EditCondition="bSpawnEffectOnOffHand"))
	FName OffHandEffectAttachSocket = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.01"))
	float CastEffectScale = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.01"))
	float ChargeEffectScale = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.01"))
	float ImpactEffectScale = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.01", Units="s"))
	float CastEffectEmissionDuration = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.01", Units="s"))
	float ImpactEffectEmissionDuration = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|Tuning", meta=(ClampMin="0.1"))
	float AttackEffectTimeDilation = 2.0f;

	/** 로컬 1인칭에서 화면 중앙을 비우는 전용 보정을 적용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person")
	bool bUseFirstPersonEffectOverrides = true;

	/** 손 소켓의 로컬 공간 기준으로 Cast/Charge Cue를 화면 바깥쪽·아래쪽에 배치합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides"))
	FVector FirstPersonEffectOffset = FVector(0.0f, 10.0f, -8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides"))
	FRotator FirstPersonEffectRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides", ClampMin="0.01", ClampMax="1.0"))
	float FirstPersonCastScaleMultiplier = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides", ClampMin="0.01", ClampMax="1.0"))
	float FirstPersonChargeScaleMultiplier = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides", ClampMin="0.01", ClampMax="1.0"))
	float FirstPersonImpactScaleMultiplier = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides", ClampMin="0.1", ClampMax="1.0"))
	float FirstPersonEmissionDurationMultiplier = 0.70f;

	/** 쌍수 공격의 보조 손 효과가 1인칭에서 주손보다 작게 보이도록 추가로 곱하는 배율입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual|Niagara|First Person", meta=(EditCondition="bUseFirstPersonEffectOverrides && bSpawnEffectOnOffHand", ClampMin="0.1", ClampMax="1.0"))
	float FirstPersonOffHandEffectScaleMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual")
	TObjectPtr<UTexture2D> SkillIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual")
	FText IconLabel = FText::FromString(TEXT("공"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual")
	FLinearColor IconTint = FLinearColor(1.0f, 0.72f, 0.22f, 1.0f);

	/** 이전 범용 Effect 필드입니다. Niagara CastEffect가 비어 있을 때만 호환용으로 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Visual", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem,/Script/Engine.ParticleSystem"))
	TSoftObjectPtr<UObject> AttackEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Debug")
	bool bDrawDebugAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Debug")
	FLinearColor DebugColor = FLinearColor(1.0f, 0.72f, 0.22f, 1.0f);
};
