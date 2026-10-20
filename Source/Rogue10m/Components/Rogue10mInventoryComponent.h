// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mCharacterStats.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mInventoryComponent.generated.h"

class ARogue10mDroppedItem;
class URogue10mItemDataAsset;
struct FRogue10mInventorySnapshot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mInventoryGridChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mEquipmentChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mInventoryChanged);

UENUM(BlueprintType)
enum class ERogue10mInventorySlotType : uint8
{
	MainWeapon,
	SecondaryWeapon,
	Head,
	Armor,
	Shoes,
	Chest,
	Legs,
	Hands,
	Weapon,
	Charm,
	Ring,
	Earring,
	Relic,
	Consumable,
	Material,
	/** 목걸이 장비 슬롯. 기존 enum 직렬화 호환성을 위해 마지막에 추가합니다. */
	Necklace
};

UENUM(BlueprintType)
enum class ERogue10mItemCategory : uint8
{
	Equipment,
	Consumable,
	Material,
	Currency,
	Quest
};

USTRUCT(BlueprintType)
struct FRogue10mItemStack
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	FGuid InstanceId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items|Enhancement")
	int32 EnhancementLevel = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items|Enhancement")
	float EnhancementAttackBonus = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items|Enhancement")
	float EnhancementDefenseBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	TObjectPtr<const URogue10mItemDataAsset> ItemData;

	// 아이템의 큰 분류입니다. 장비/소모품/재료/재화/퀘스트 아이템을 구분합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	ERogue10mItemCategory Category = ERogue10mItemCategory::Material;

	// 장비 아이템일 때 들어갈 장비 슬롯입니다. 무기는 Weapon 슬롯을 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	ERogue10mInventorySlotType EquipSlotType = ERogue10mInventorySlotType::Material;

	// 무기 장비일 때 캐릭터의 현재 무기 타입으로 반영할 값입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	ERogue10mWeaponType WeaponType = ERogue10mWeaponType::Unarmed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items", meta=(ClampMin="0"))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	FLinearColor ItemColor = FLinearColor(0.25f, 0.85f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	bool bOccupied = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items|Consumable", meta=(ClampMin="0.0", Units="s"))
	float UseCooldown = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Items|Consumable", meta=(ClampMin="0.0"))
	float RestoreHealth = 25.0f;
};

USTRUCT(BlueprintType)
struct FRogue10mInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	ERogue10mInventorySlotType SlotType = ERogue10mInventorySlotType::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	FLinearColor SlotColor = FLinearColor(0.85f, 0.68f, 0.32f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	bool bEquipped = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	bool bHasEquippedItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory")
	FRogue10mItemStack EquippedItem;
};

USTRUCT(BlueprintType)
struct FRogue10mInventoryGridEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Enhancement")
	int32 EnhancementLevel = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Enhancement")
	float EnhancementAttackBonus = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Enhancement")
	float EnhancementDefenseBonus = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	TObjectPtr<const URogue10mItemDataAsset> ItemData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid", meta=(ClampMin="1"))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	FIntPoint Position = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FRogue10mInventoryContainer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	FName ContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	FIntPoint GridSize = FIntPoint(10, 10);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	TObjectPtr<const URogue10mItemDataAsset> SourceBagItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	TArray<FRogue10mInventoryGridEntry> Entries;
};

/**
 * Prototype inventory/equipment data for the first-person character.
 */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Inventory|Grid")
	FRogue10mInventoryGridChanged OnInventoryGridChanged;
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Inventory|Equipment")
	FRogue10mEquipmentChanged OnEquipmentChanged;
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Inventory")
	FRogue10mInventoryChanged OnInventoryChanged;

	FRogue10mInventorySnapshot CaptureInventory() const;
	bool RestoreInventory(const FRogue10mInventorySnapshot& Snapshot);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Currency")
	void AddGold(int32 Amount);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Currency")
	bool TrySpendGold(int32 Amount);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Enhancement")
	int32 GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType SlotType) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Enhancement")
	bool TryEnhanceEquippedItem(ERogue10mInventorySlotType SlotType, int32 Cost, int32 MaxLevel,
		float AttackPerLevel, float DefensePerLevel);
	/** Re-acquires a dropped instance without discarding its enhancement state. */
	bool TryAddItemStack(const FRogue10mItemStack& Stack, int32& OutContainerIndex, FGuid& OutInstanceId);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Currency")
	bool TryPurchaseGridItem(const URogue10mItemDataAsset* ItemData, int32 Quantity, int32 GoldCost);


	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory")
	const TArray<FRogue10mInventorySlot>& GetLeftEquipmentSlots() const { return LeftEquipmentSlots; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory")
	const TArray<FRogue10mInventorySlot>& GetRightEquipmentSlots() const { return RightEquipmentSlots; }

	/** 지정 부위에 실제 장착된 Item Data를 반환합니다. 비어 있거나 잠긴 슬롯은 nullptr입니다. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Equipment")
	const URogue10mItemDataAsset* GetEquippedItemData(ERogue10mInventorySlotType SlotType) const;

	/** 현재 장착된 모든 Item Data Asset의 능력치 증가량 합계입니다. */
	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Equipment")
	FRogue10mEquipmentStatModifiers GetEquipmentStatModifiers() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	const TArray<FRogue10mItemStack>& GetItemSlots() const { return ItemSlots; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	int32 GetItemGridColumns() const { return ItemGridColumns; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	int32 GetCrystals() const { return Crystals; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Weight")
	float GetTotalInventoryWeight() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Weight")
	float GetMaxCarryWeight() const { return MaxCarryWeight; }

	// 아이템 슬롯의 장비를 기본 장착 슬롯으로 장착합니다.
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryEquipItemFromSlot(int32 ItemSlotIndex);

	// 아이템 슬롯의 장비를 지정한 장착 슬롯으로 장착합니다.
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryEquipItemToSlot(int32 ItemSlotIndex, ERogue10mInventorySlotType TargetSlotType);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryUnequipItemFromSlot(ERogue10mInventorySlotType TargetSlotType);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryUnequipItemFromSlotToItemSlot(ERogue10mInventorySlotType TargetSlotType, int32 TargetItemSlotIndex);

	// 아이템 창 내부에서 드래그 앤 드랍으로 아이템 위치를 이동하거나 교체합니다.
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryMoveItemSlot(int32 SourceItemSlotIndex, int32 TargetItemSlotIndex);

	/** Moves an equipped Data Asset item directly into an exact NxM grid position. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Equipment")
	bool TryUnequipItemToGrid(ERogue10mInventorySlotType SourceSlotType, int32 TargetContainerIndex,
		FIntPoint TargetPosition);

	/** Moves equipped gear to the first MxN position available across all inventory containers. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Equipment")
	bool TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType SourceSlotType);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool RemoveItemFromSlot(int32 ItemSlotIndex, FRogue10mItemStack& OutRemovedItem);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool RemoveEquippedItemFromSlot(ERogue10mInventorySlotType TargetSlotType, FRogue10mItemStack& OutRemovedItem);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items|Quick Slot") bool AssignConsumableToQuickSlot(int32 ItemSlotIndex, int32 QuickSlotIndex);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items|Quick Slot") bool UnassignConsumableQuickSlot(int32 QuickSlotIndex);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items|Quick Slot") bool MoveConsumableQuickSlot(int32 SourceQuickSlotIndex, int32 TargetQuickSlotIndex);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items|Quick Slot") bool UseConsumableQuickSlot(int32 QuickSlotIndex);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Quick Slot") FRogue10mItemStack GetConsumableQuickSlotItem(int32 QuickSlotIndex) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Quick Slot") int32 GetConsumableQuickSlotInventoryIndex(int32 QuickSlotIndex) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Items|Quick Slot") float GetConsumableQuickSlotCooldownRemaining(int32 QuickSlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Grid")
	const TArray<FRogue10mInventoryContainer>& GetInventoryContainers() const { return InventoryContainers; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory|Grid")
	bool CanPlaceGridItem(int32 ContainerIndex, const URogue10mItemDataAsset* ItemData, FIntPoint Position,
		FGuid IgnoredInstanceId) const;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Grid")
	bool TryAddGridItem(const URogue10mItemDataAsset* ItemData, int32 Quantity, int32& OutContainerIndex, FGuid& OutInstanceId);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Grid")
	bool TryMoveGridItem(int32 SourceContainerIndex, FGuid InstanceId, int32 TargetContainerIndex,
		FIntPoint TargetPosition);

	/** Uses one consumable from an NxM grid entry. The item is not consumed when its effect cannot apply. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Grid")
	bool TryUseGridConsumable(int32 ContainerIndex, FGuid InstanceId);

	/** Equips an NxM grid item and atomically returns replaced equipment to a valid grid position. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Grid")
	bool TryEquipGridItem(int32 ContainerIndex, FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Grid")
	bool RemoveGridItem(int32 ContainerIndex, FGuid InstanceId, FRogue10mInventoryGridEntry& OutRemovedEntry);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Bag")
	bool AddInventoryFromBag(const URogue10mItemDataAsset* BagItemData, FName RequestedContainerId);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Drop")
	bool DropGridItem(int32 ContainerIndex, FGuid InstanceId, FVector WorldLocation);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory")
	TArray<FRogue10mInventorySlot> LeftEquipmentSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory")
	TArray<FRogue10mInventorySlot> RightEquipmentSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items")
	TArray<FRogue10mItemStack> ItemSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items", meta=(ClampMin="1"))
	int32 ItemGridColumns = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid", meta=(ClampMin="1", ClampMax="20"))
	FIntPoint BaseInventorySize = FIntPoint(10, 10);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Grid")
	TArray<FRogue10mInventoryContainer> InventoryContainers;

	/** Adds the configured prototype items once when the base inventory starts empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Prototype")
	bool bAddPrototypeStartingItems = true;
	/** Automatically equips compatible data-driven starter equipment for UI verification. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Prototype")
	bool bEquipPrototypeStarterEquipment = true;


	/** Data-driven starter items used to exercise NxM inventory placement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Prototype")
	TArray<TSoftObjectPtr<URogue10mItemDataAsset>> PrototypeStartingItems;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Drop")
	TSubclassOf<ARogue10mDroppedItem> DroppedItemClass;

	// 현재는 UI 표시용 0 고정값입니다. 이후 재화 관리 컴포넌트에서 받아오도록 교체합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items", meta=(ClampMin="0"))
	int32 Gold = 0;

	// 현재는 UI 표시용 0 고정값입니다. 이후 재화 관리 컴포넌트에서 받아오도록 교체합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items", meta=(ClampMin="0"))
	int32 Crystals = 0;

	/** 현재는 UI 표시 기준이며, 초과 시 이동 제한 정책은 후속 구현합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Weight", meta=(ClampMin="0.0", Units="kg"))
	float MaxCarryWeight = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Items|Quick Slot", meta=(EditFixedSize))
	TArray<int32> ConsumableQuickSlotItemIndices;
	UPROPERTY(Transient)
	TArray<float> ConsumableQuickSlotCooldownEndTimes;

private:
	bool FindFirstGridPosition(int32 ContainerIndex, const URogue10mItemDataAsset* ItemData, FIntPoint& OutPosition,
		FGuid IgnoredInstanceId = FGuid()) const;
	FRogue10mInventoryGridEntry* FindGridEntry(int32 ContainerIndex, FGuid InstanceId);
	const FRogue10mInventoryGridEntry* FindGridEntry(int32 ContainerIndex, FGuid InstanceId) const;

	// 장비창의 개별 슬롯 표시 데이터를 만듭니다.
	static FRogue10mInventorySlot MakeSlot(ERogue10mInventorySlotType SlotType, const TCHAR* DisplayName, const FLinearColor& SlotColor, bool bLocked, bool bEquipped);

	// 아이템 창에 들어갈 일반 아이템 데이터를 만듭니다.
	static FRogue10mItemStack MakeItem(ERogue10mItemCategory Category, ERogue10mInventorySlotType EquipSlotType, const TCHAR* DisplayName, const TCHAR* Description, int32 Quantity, const FLinearColor& ItemColor, bool bLocked = false);

	// Item Data Asset을 기존 아이템/장비 슬롯 스택 표현으로 변환합니다.
	static FRogue10mItemStack MakeItemFromDataAsset(const URogue10mItemDataAsset* ItemData, int32 Quantity);

	// 무기 슬롯에 장착 가능한 무기 아이템 데이터를 만듭니다.
	static FRogue10mItemStack MakeWeaponItem(ERogue10mWeaponType WeaponType, const TCHAR* DisplayName, const TCHAR* Description, const FLinearColor& ItemColor, ERogue10mInventorySlotType EquipSlotType = ERogue10mInventorySlotType::MainWeapon, bool bLocked = false);

	// 비어 있는 인벤토리 칸을 표현하는 아이템 데이터를 만듭니다.
	static FRogue10mItemStack MakeEmptyItem();

	// 좌/우 장비 슬롯 배열에서 목표 슬롯 타입을 찾습니다.
	FRogue10mInventorySlot* FindEquipmentSlot(ERogue10mInventorySlotType SlotType);
	void NotifyEquipmentChanged();
	void NotifyGridChanged();
	int32 FindFirstEmptyItemSlot() const;
	void ClearQuickSlotAssignmentsForItemSlot(int32 ItemSlotIndex);
	void ResetEquipmentSlotDisplay(FRogue10mInventorySlot& EquipmentSlot, ERogue10mInventorySlotType TargetSlotType);
};
