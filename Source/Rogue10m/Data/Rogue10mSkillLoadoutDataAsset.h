// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mSkillLoadoutDataAsset.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class ROGUE10M_API URogue10mDodgeSkillDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge|Identity")
	FName DodgeId = TEXT("DefaultDodge");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge|Display")
	FText DisplayName = FText::FromString(TEXT("회피"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge|Display", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge|Display")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge", meta=(ClampMin="0.0", Units="cm"))
	float Distance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge", meta=(ClampMin="0.05", Units="s"))
	float Duration = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge", meta=(ClampMin="0.0", Units="s"))
	float Cooldown = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge", meta=(ClampMin="0.0"))
	float StaminaCost = 0.0f;
};

/** Stable identity for a selectable technique, separate from basic/passive progression nodes.
 * Released TechniqueIds must remain stable. This does not migrate existing soft-path saves.
 */
USTRUCT(BlueprintType)
struct FRogue10mActiveTechniqueCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Catalog")
	FName TechniqueId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Catalog")
	TObjectPtr<URogue10mAttackSkillData> Skill;
};

UCLASS(BlueprintType)
class ROGUE10M_API URogue10mWeaponSkillProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Loadout")
	URogue10mAttackSkillData* FindDefaultSkill(ERogue10mAttackInputSlot InputSlot) const;

	static constexpr int32 RequiredActiveTechniqueCount = 10;

	/** Empty is an optional legacy catalog, not a replacement for SkillTreeSkills. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Loadout|Catalog")
	bool HasExplicitActiveTechniqueCatalog() const { return !ActiveTechniqueCatalog.IsEmpty(); }

	/** Validates the complete optional catalog without changing bindings, unlocks or progression. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Loadout|Catalog")
	bool ValidateActiveTechniqueCatalog(FText& OutReason) const;

	/** Explicit validated entries only. Absent/invalid catalogs return empty; no legacy tree fallback. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Loadout|Catalog")
	TArray<URogue10mAttackSkillData*> GetValidatedActiveTechniqueCatalog() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Loadout|Catalog")
	URogue10mAttackSkillData* FindActiveTechniqueById(FName TechniqueId) const;

	/** Catalog gate only; callers still own unlock, fixed-basic and slot checks. */
	bool CatalogAllowsLoadoutSkill(const URogue10mAttackSkillData* Skill) const;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout")
	FName ProfileId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Display", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Display")
	TObjectPtr<UTexture2D> IdentityIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout")
	ERogue10mWeaponType WeaponType = ERogue10mWeaponType::Unarmed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout")
	TMap<ERogue10mAttackInputSlot, TObjectPtr<URogue10mAttackSkillData>> DefaultSkillBindings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TArray<TObjectPtr<URogue10mAttackSkillData>> SkillTreeSkills;

	/** Opt-in melee candidate catalog. Keep the full basic/passive/legacy SkillTreeSkills intact.
	 * Empty preserves legacy behavior. Populated catalogs require exactly ten explicit-policy
	 * techniques already present in the full tree. Mage command/instant rules are separate.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Loadout|Catalog", meta=(TitleProperty="TechniqueId"))
	TArray<FRogue10mActiveTechniqueCatalogEntry> ActiveTechniqueCatalog;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TArray<TObjectPtr<URogue10mAttackSkillData>> InitiallyUnlockedSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Dodge")
	TObjectPtr<URogue10mDodgeSkillDataAsset> DefaultDodgeSkill;

	/** 이 프로필을 장착했을 때 허용되는 연속 점프 횟수입니다. 1은 단일 점프입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traversal", meta=(ClampMin="1", ClampMax="4"))
	int32 MaxJumpCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traversal|Display")
	FText ExtraJumpDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traversal|Display", meta=(MultiLine="true"))
	FText ExtraJumpDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traversal|Display")
	TObjectPtr<UTexture2D> ExtraJumpIcon;
};
