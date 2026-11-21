// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mCharacterStats.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mItemDataAsset.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ERogue10mItemRarity : uint8
{
	Common UMETA(DisplayName="일반"),
	Rare UMETA(DisplayName="레어"),
	Epic UMETA(DisplayName="에픽"),
	Unique UMETA(DisplayName="유니크"),
	Mythic UMETA(DisplayName="신화")
};

/** 아이템 인스턴스와 분리된 정적 정의 데이터입니다. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	FIntPoint GetClampedInventorySize() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Bag")
	FIntPoint GetClampedBagSize() const;

	/** 인벤토리 아이콘 뒤에 표시할 반투명 등급 배경색입니다. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Display")
	FLinearColor GetInventoryRarityBackgroundColor() const;

	/** 툴팁과 장비 메뉴의 아이템 이름에 사용할 불투명 등급 색상입니다. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Display")
	FLinearColor GetRarityTextColor() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Identity")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display")
	TSoftObjectPtr<UTexture2D> InventoryIcon;

	/** Uniform display scale applied after the inventory icon is fitted to the item footprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display",
		meta=(ClampMin="0.1", ClampMax="2.0", UIMin="0.1", UIMax="2.0"))
	float InventoryIconScale = 1.0f;

	/** Inventory fallback color used when an item does not have an icon yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display")
	FLinearColor InventoryTint = FLinearColor(0.16f, 0.42f, 0.68f, 0.85f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Display")
	ERogue10mItemRarity Rarity = ERogue10mItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Classification")
	ERogue10mItemCategory Category = ERogue10mItemCategory::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Classification")
	ERogue10mInventorySlotType EquipSlotType = ERogue10mInventorySlotType::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Classification")
	ERogue10mWeaponType WeaponType = ERogue10mWeaponType::Unarmed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Inventory", meta=(ClampMin="1", ClampMax="10"))
	FIntPoint InventorySize = FIntPoint(1, 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Inventory", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	/** 아이템 한 개의 무게(kg)입니다. 총 무게는 수량을 곱해 계산합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Inventory", meta=(ClampMin="0.0", Units="kg"))
	float UnitWeight = 0.1f;

	/** 체력 회복형 소비 아이템을 한 번 사용할 때 회복하는 양입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Consumable", meta=(ClampMin="0.0"))
	float RestoreHealth = 25.0f;

	/** A growth consumable adds a rank to the specified identity for the current run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Consumable")
	ERogue10mSkillTreeIdentity IdentityGrowthType = ERogue10mSkillTreeIdentity::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Consumable", meta=(ClampMin="0"))
	int32 IdentityGrowthAmount = 0;

	bool UsesIdentityGrowth() const
	{
		return IdentityGrowthType != ERogue10mSkillTreeIdentity::None || IdentityGrowthAmount != 0;
	}

	/** 장비 Tooltip과 향후 장비 효과 계산에서 공유하는 능력치 증가량입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Equipment", meta=(ShowOnlyInnerProperties))
	FRogue10mEquipmentStatModifiers EquipmentStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|World")
	TSoftObjectPtr<UStaticMesh> DroppedWorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|World")
	bool bCanDrop = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Bag")
	bool bProvidesInventory = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Bag", meta=(EditCondition="bProvidesInventory", ClampMin="1", ClampMax="20"))
	FIntPoint ProvidedInventorySize = FIntPoint(5, 5);
};
