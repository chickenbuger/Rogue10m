// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMenuWindowWidgets.generated.h"

class URogue10mAttackSkillData;
class URogue10mCombatComponent;
class URogue10mInventoryComponent;
class URogue10mItemDataAsset;
class URogue10mItemDragDropOperation;
class URogue10mEquipmentSlotActionWidget;
class URogue10mEquipmentDragSourceWidget;
class URogue10mEquipmentWindowWidget;
class URogue10mInventoryItemTooltipWidget;
class ARogue10mEquipmentPreviewActor;
class UImage;
class USizeBox;
class UCanvasPanel;
class UBorder;
class UButton;
class UPanelWidget;
class UProgressBar;
class UWidget;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mMenuWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	virtual void InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	virtual void SetWindowOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	URogue10mInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsWindowOpen() const { return bWindowOpen; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	/** 전체 화면 Widget 안에서 실제 창 내용만 이동시키는 공통 루트입니다. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|UI|Window Drag")
	TObjectPtr<UCanvasPanel> UI_WindowRoot;

	/** 이 Border 안에서 시작한 좌클릭만 창 이동으로 처리합니다. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|UI|Window Drag")
	TObjectPtr<UBorder> UI_WindowDragHandle;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|UI", meta=(DisplayName="Menu Window Initialized"))
	void BP_OnMenuWindowInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|UI", meta=(DisplayName="Menu Window Open Changed"))
	void BP_OnWindowOpenChanged(bool bOpen);

private:
	UPROPERTY(Transient)
	TObjectPtr<URogue10mInventoryComponent> InventoryComponent;

	bool bWindowOpen = false;
	bool bWindowDragging = false;
	FVector2D WindowDragStartScreenPosition = FVector2D::ZeroVector;
	FVector2D WindowDragStartCanvasPosition = FVector2D::ZeroVector;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mInventoryCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGridPosition(FIntPoint InGridPosition) { GridPosition = InGridPosition; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Inventory")
	FIntPoint GetGridPosition() const { return GridPosition; }

private:
	UPROPERTY(Transient)
	FIntPoint GridPosition = FIntPoint::ZeroValue;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mInventoryItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Inventory|Tooltip")
	void InitializeItemTooltip(const URogue10mItemDataAsset* InItemData, int32 InQuantity,
		const URogue10mItemDataAsset* InEquippedItemData, bool bInEquippedSlot);

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<USizeBox> UI_ItemTooltipSize;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UImage> UI_ItemIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_ItemNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_ItemDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_ItemWeightText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_ItemActionHintText;


	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UVerticalBox> UI_ItemStatsSection;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_ItemStatsTitleText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UVerticalBox> UI_ItemStatsContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UBorder> UI_EquippedItemPanel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UImage> UI_EquippedItemIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_EquippedItemNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UTextBlock> UI_EquippedItemDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UVerticalBox> UI_EquippedItemStatsContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory|Tooltip")
	TObjectPtr<UVerticalBox> UI_ComparisonStatsContainer;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mEquipmentSlotActionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeActionMenu(URogue10mInventoryComponent* InInventory,
		ERogue10mInventorySlotType InSlotType, const URogue10mItemDataAsset* InItemData,
		URogue10mEquipmentWindowWidget* InOwnerWindow);
	void SetMenuPosition(FVector2D InPosition, FVector2D InSize);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<USizeBox> UI_EquipmentSlotActionSize;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<UTextBlock> UI_EquipmentItemNameText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<UButton> UI_UnequipButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<UTextBlock> UI_UnequipButtonText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<UTextBlock> UI_ActionResultText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment|Action")
	TObjectPtr<UButton> UI_ActionDismissButton;

private:
	UFUNCTION()
	void HandleUnequipClicked();

	UPROPERTY(Transient)
	TObjectPtr<URogue10mInventoryComponent> Inventory;
	TWeakObjectPtr<URogue10mEquipmentWindowWidget> OwnerWindow;

	ERogue10mInventorySlotType SlotType = ERogue10mInventorySlotType::Material;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeGridItem(URogue10mInventoryComponent* InInventory, class URogue10mInventoryWindowWidget* InInventoryWindow,
		int32 InContainerIndex, const struct FRogue10mInventoryGridEntry& InEntry, float InCellSize);
	void SetPlacementPreview(bool bPreviewing, bool bCanPlace);
protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<USizeBox> UI_InventoryItemSize;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UImage> UI_InventoryItemIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UTextBlock> UI_InventoryItemQuantityText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UBorder> UI_InventoryItemRarityBackground;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UBorder> UI_InventoryItemPreviewBorder;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory|Tooltip")
	TSoftClassPtr<URogue10mInventoryItemTooltipWidget> InventoryItemTooltipWidgetClass =
		TSoftClassPtr<URogue10mInventoryItemTooltipWidget>(FSoftObjectPath(TEXT("/Game/Widget/Component/Inventory/WBP_InventoryItemTooltip.WBP_InventoryItemTooltip_C")));
private:
	UPROPERTY(Transient) TObjectPtr<URogue10mInventoryComponent> Inventory;
	UPROPERTY(Transient) TObjectPtr<class URogue10mInventoryWindowWidget> InventoryWindow;
	UPROPERTY(Transient) TObjectPtr<const URogue10mItemDataAsset> ItemData;
	int32 ContainerIndex = INDEX_NONE;
	FGuid InstanceId;
	int32 Quantity = 0;
	float CellSize = 44.0f;
	FIntPoint DragGrabOffset = FIntPoint::ZeroValue;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mBagTabWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeBagTab(class URogue10mInventoryWindowWidget* InInventoryWindow, int32 InContainerIndex,
		const FText& DisplayName, bool bSelected);
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UButton> UI_BagTabButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory") TObjectPtr<UTextBlock> UI_BagNameText;

private:
	UFUNCTION() void HandleBagTabClicked();
	UPROPERTY(Transient) TObjectPtr<class URogue10mInventoryWindowWidget> InventoryWindow;
	int32 ContainerIndex = INDEX_NONE;
};
UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mInventoryWindowWidget : public URogue10mMenuWindowWidget
{
	GENERATED_BODY()

public:
	virtual void InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent) override;
	virtual void SetWindowOpen(bool bOpen) override;

	void BeginGridItemDrag(URogue10mItemDragDropOperation* Operation);
	void EndGridItemDrag(URogue10mItemDragDropOperation* Operation);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory")
	TObjectPtr<UUniformGridPanel> UI_InventoryGrid;

	/** NxM 아이템 위젯을 셀 좌표와 크기에 맞춰 배치하는 Grid 상단 레이어입니다. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory")
	TObjectPtr<UCanvasPanel> UI_InventoryItemCanvas;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory")
	TObjectPtr<UTextBlock> UI_InventoryMoneyText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Inventory")
	TObjectPtr<UTextBlock> UI_InventoryWeightText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory")
	TSubclassOf<URogue10mInventoryCellWidget> InventoryCellWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory")
	TSubclassOf<URogue10mInventoryItemWidget> InventoryItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Inventory", meta=(ClampMin="16.0"))
	float InventoryCellSize = 44.0f;

private:
	UFUNCTION()
	void HandleInventoryGridChanged();

	void RefreshInventoryDisplay();
	void RebuildInventoryCells(FIntPoint GridSize);
	void RebuildInventoryItems();

	bool UpdateGridDropPreview(const FDragDropEvent& DragDropEvent, URogue10mItemDragDropOperation* Operation);
	void ClearGridDropPreview();

	FIntPoint CachedGridSize = FIntPoint::ZeroValue;
	static constexpr int32 PrimaryContainerIndex = 0;
	TWeakObjectPtr<URogue10mItemDragDropOperation> ActiveGridDragOperation;
};

/**
 * Cursor-positioned drag source used by equipped items.
 *
 * UMG animates a drag decorator from the drag source widget's absolute position for the first
 * 0.15 seconds. The equipment window itself fills the viewport, so using it as the source makes
 * the decorator fly in from the viewport's upper-left corner. This invisible proxy is sized and
 * positioned to match the decorator at mouse-down time, removing that unintended travel.
 */
UCLASS(Transient)
class ROGUE10M_API URogue10mEquipmentDragSourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeDragSource(URogue10mEquipmentWindowWidget* InEquipmentWindow,
		ERogue10mInventorySlotType InSlotType, const FAnchorData& InHomeLayout);
	void SetDragLayout(FVector2D ParentLocalCursorPosition, FVector2D DragVisualSize);
	void RestoreHomeLayout();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
private:
	TWeakObjectPtr<URogue10mEquipmentWindowWidget> EquipmentWindow;
	ERogue10mInventorySlotType SlotType = ERogue10mInventorySlotType::Material;
	FAnchorData HomeLayout;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mEquipmentWindowWidget : public URogue10mMenuWindowWidget
{
	GENERATED_BODY()

public:
	virtual void InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent) override;
	virtual void SetWindowOpen(bool bOpen) override;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;


	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UCanvasPanel> UI_EquipmentSlotContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_CharacterPreviewImage;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_WeaponSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_HeadSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_ChestSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_HandsSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_RingSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_FeetSlotIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UImage> UI_NecklaceSlotIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_WeaponSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_HeadSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_ChestSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_HandsSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_RingSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_FeetSlotFrame;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment")
	TObjectPtr<UBorder> UI_NecklaceSlotFrame;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Equipment|Drag Drop", meta=(ClampMin="16.0"))
	float InventoryDragCellSize = 44.0f;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Equipment")
	TObjectPtr<UVerticalBox> UI_EquipmentStatsContainer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_AttackStatText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_DefenseStatText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_MaxHealthStatText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_CriticalChanceStatText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_AttackSpeedStatText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Equipment|Stats")
	TObjectPtr<UTextBlock> UI_MoveSpeedStatText;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Equipment|Preview")
	FIntPoint CharacterPreviewResolution = FIntPoint(512, 768);
	/** 프리뷰 가로 드래그 1픽셀당 적용할 캐릭터 Yaw 각도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Equipment|Preview",
		meta=(ClampMin="0.01", ClampMax="2.0", Units="deg"))
	float CharacterPreviewRotationDegreesPerPixel = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Equipment|Tooltip")
	TSoftClassPtr<URogue10mInventoryItemTooltipWidget> EquipmentItemTooltipWidgetClass =
		TSoftClassPtr<URogue10mInventoryItemTooltipWidget>(FSoftObjectPath(TEXT("/Game/Widget/Component/Inventory/WBP_InventoryItemTooltip.WBP_InventoryItemTooltip_C")));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Equipment|Action")
	TSoftClassPtr<URogue10mEquipmentSlotActionWidget> EquipmentSlotActionWidgetClass =
		TSoftClassPtr<URogue10mEquipmentSlotActionWidget>(FSoftObjectPath(TEXT("/Game/Widget/Component/Equipment/WBP_EquipmentSlotAction.WBP_EquipmentSlotAction_C")));


	virtual void NativeDestruct() override;

private:
	friend class URogue10mEquipmentDragSourceWidget;
	friend class URogue10mEquipmentSlotActionWidget;


	UFUNCTION()
	void HandleEquipmentChanged();
	UFUNCTION()
	void HandleEquipmentDragFinished(UDragDropOperation* Operation);
	void CreateEquipmentItemDragOperation(UDragDropOperation*& OutOperation);
	FReply HandleEquipmentDragSourceMouseButtonDown(URogue10mEquipmentDragSourceWidget* DragSource,
		ERogue10mInventorySlotType SlotType, const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);
	void RebuildEquipmentDragSourceProxies();
	void ClearEquipmentDragSourceProxies();

	void RefreshEquipmentDisplay();
	bool EnsureCharacterPreview();
	void RefreshCharacterPreview();
	void DestroyCharacterPreview();

	void RefreshEquipmentSlotTooltip(UWidget* HitWidget, ERogue10mInventorySlotType SlotType);
	void OpenEquipmentSlotActionMenu(ERogue10mInventorySlotType SlotType,
		FVector2D ScreenPosition, UWidget* HitWidget);
	void CloseEquipmentSlotActionMenu();
	void RestoreSuppressedEquipmentSlotTooltip();
	const FRogue10mInventorySlot* FindEquipmentSlotData(ERogue10mInventorySlotType SlotType) const;
	TOptional<ERogue10mInventorySlotType> FindEquipmentDropSlot(
		FVector2D ScreenPosition, UBorder*& OutFrame) const;
	bool UpdateEquipmentDropPreview(const FDragDropEvent& DragDropEvent, URogue10mItemDragDropOperation* Operation);
	void ClearEquipmentDropPreview();
	TOptional<ERogue10mInventorySlotType> PendingEquipmentDragSlotType;
	FIntPoint PendingEquipmentGrabCellOffset = FIntPoint::ZeroValue;

	TWeakObjectPtr<URogue10mItemDragDropOperation> ActiveEquipmentDragOperation;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URogue10mEquipmentDragSourceWidget>> EquipmentDragSourceProxies;
	TWeakObjectPtr<URogue10mEquipmentDragSourceWidget> ActiveEquipmentDragSourceProxy;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mEquipmentSlotActionWidget> ActiveEquipmentSlotActionWidget;
	TWeakObjectPtr<UWidget> SuppressedEquipmentTooltipWidget;
	TOptional<ERogue10mInventorySlotType> SuppressedEquipmentTooltipSlotType;

	UPROPERTY(Transient)
	TObjectPtr<ARogue10mEquipmentPreviewActor> CharacterPreviewActor;
	bool bCharacterPreviewRotating = false;
	FVector2D LastCharacterPreviewDragScreenPosition = FVector2D::ZeroVector;
	TWeakObjectPtr<UBorder> ActiveEquipmentDropFrame;
	TWeakObjectPtr<URogue10mItemDragDropOperation> ActiveEquipmentDropOperation;


	FLinearColor ActiveEquipmentDropOriginalColor = FLinearColor::Transparent;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRogue10mSkillTreeEntrySelected, URogue10mAttackSkillData*, SkillData);

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mSkillTreeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void SetSkillData(
		URogue10mAttackSkillData* InSkillData, bool bInUnlocked,
		bool bInAvailable, float InUnlockProgress);

	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Skill Tree")
	FRogue10mSkillTreeEntrySelected OnSkillSelected;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UImage> UI_SkillIconImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillLockText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillTierText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillProgressText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UProgressBar> UI_SkillProgressBar;

private:
	UPROPERTY(Transient)
	TObjectPtr<URogue10mAttackSkillData> SkillData;

	bool bUnlocked = false;
	bool bCanDrag = false;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mSkillTreeWindowWidget : public URogue10mMenuWindowWidget
{
	GENERATED_BODY()

public:
	virtual void InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent) override;
	virtual void SetWindowOpen(bool bOpen) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void RefreshSkillTree();

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UPanelWidget> UI_SkillListContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SkillTreeTitleText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_ActiveWeaponText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_MartialArtStageText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_MasteryProgressText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UProgressBar> UI_MasteryProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UImage> UI_SelectedSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillTierText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillStatusText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillConditionsText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Skill Tree")
	TObjectPtr<UTextBlock> UI_SelectedSkillRewardText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TSubclassOf<URogue10mSkillTreeEntryWidget> SkillTreeEntryWidgetClass;

private:
	UFUNCTION()
	void HandleSkillSelected(URogue10mAttackSkillData* SkillData);

	UFUNCTION()
	void HandleSkillTreeChanged();

	void RefreshSelectedSkillDetails();
	URogue10mCombatComponent* GetCombatComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mAttackSkillData> SelectedSkill;
};
