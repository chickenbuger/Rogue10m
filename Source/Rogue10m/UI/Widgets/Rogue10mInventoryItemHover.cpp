// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mMenuWindowWidgets.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"

namespace
{
FText GetEquipmentTypeName(ERogue10mInventorySlotType Slot)
{
	switch (Slot)
	{
	case ERogue10mInventorySlotType::MainWeapon:
	case ERogue10mInventorySlotType::Weapon:
		return NSLOCTEXT("Rogue10mInventory", "TooltipWeaponType", "무기");
	case ERogue10mInventorySlotType::SecondaryWeapon:
		return NSLOCTEXT("Rogue10mInventory", "TooltipSecondaryWeaponType", "보조무기");
	case ERogue10mInventorySlotType::Head:
		return NSLOCTEXT("Rogue10mInventory", "TooltipHeadType", "투구");
	case ERogue10mInventorySlotType::Armor:
	case ERogue10mInventorySlotType::Chest:
		return NSLOCTEXT("Rogue10mInventory", "TooltipArmorType", "갑옷");
	case ERogue10mInventorySlotType::Hands:
		return NSLOCTEXT("Rogue10mInventory", "TooltipHandsType", "장갑");
	case ERogue10mInventorySlotType::Legs:
		return NSLOCTEXT("Rogue10mInventory", "TooltipLegsType", "하의");
	case ERogue10mInventorySlotType::Shoes:
		return NSLOCTEXT("Rogue10mInventory", "TooltipShoesType", "신발");
	case ERogue10mInventorySlotType::Ring:
		return NSLOCTEXT("Rogue10mInventory", "TooltipRingType", "반지");
	case ERogue10mInventorySlotType::Necklace:
		return NSLOCTEXT("Rogue10mInventory", "TooltipNecklaceType", "목걸이");
	default:
		return NSLOCTEXT("Rogue10mInventory", "TooltipEquipmentType", "장비");
	}
}

FText GetInventoryItemTypeName(const URogue10mItemDataAsset& ItemData)
{
	switch (ItemData.Category)
	{
	case ERogue10mItemCategory::Equipment:
		return GetEquipmentTypeName(ItemData.EquipSlotType);
	case ERogue10mItemCategory::Consumable:
		return NSLOCTEXT("Rogue10mInventory", "TooltipConsumableType", "소모품");
	case ERogue10mItemCategory::Currency:
		return NSLOCTEXT("Rogue10mInventory", "TooltipCurrencyType", "재화");
	case ERogue10mItemCategory::Quest:
		return NSLOCTEXT("Rogue10mInventory", "TooltipQuestType", "퀘스트");
	default:
		return NSLOCTEXT("Rogue10mInventory", "TooltipMaterialType", "재료");
	}
}

FVector2D GetInventoryTooltipPosition(const FGeometry& RootGeometry, const FGeometry& WindowGeometry,
	const FGeometry& ItemGeometry, const FVector2D& TooltipSize)
{
	constexpr double EdgeMargin = 10.0;
	constexpr double WindowGap = 12.0;
	const FVector2D RootSize = RootGeometry.GetLocalSize();
	const FVector2D WindowStart = RootGeometry.AbsoluteToLocal(
		WindowGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D WindowEnd = RootGeometry.AbsoluteToLocal(
		WindowGeometry.LocalToAbsolute(WindowGeometry.GetLocalSize()));
	const FVector2D ItemStart = RootGeometry.AbsoluteToLocal(
		ItemGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D MaxPosition(
		FMath::Max(EdgeMargin, RootSize.X - TooltipSize.X - EdgeMargin),
		FMath::Max(EdgeMargin, RootSize.Y - TooltipSize.Y - EdgeMargin));
	const double Right = WindowEnd.X + WindowGap;
	const double Left = WindowStart.X - TooltipSize.X - WindowGap;
	const double Above = WindowStart.Y - TooltipSize.Y - WindowGap;
	const double Below = WindowEnd.Y + WindowGap;

	FVector2D Position(Right, ItemStart.Y - WindowGap);
	if (Right > MaxPosition.X)
	{
		if (Left >= EdgeMargin)
		{
			Position.X = Left;
		}
		else if (Above >= EdgeMargin || Below <= MaxPosition.Y)
		{
			Position.X = ItemStart.X;
			Position.Y = Below <= MaxPosition.Y ? Below : Above;
		}
		else if (WindowStart.X > RootSize.X - WindowEnd.X)
		{
			Position.X = Left;
		}
	}
	return FVector2D(FMath::Clamp(Position.X, EdgeMargin, MaxPosition.X),
		FMath::Clamp(Position.Y, EdgeMargin, MaxPosition.Y));
}
}

void URogue10mInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (!InventoryWindow || !InventoryWindow->IsWindowOpen() || UWidgetBlueprintLibrary::IsDragDropping())
	{
		return;
	}
	if (URogue10mInventoryItemTooltipWidget* Tooltip = CreateInventoryTooltip())
	{
		InventoryWindow->ShowInventoryItemHover(*this, InGeometry, *Tooltip);
	}
}

void URogue10mInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (InventoryWindow)
	{
		InventoryWindow->HideInventoryItemHover(this);
	}
	Super::NativeOnMouseLeave(InMouseEvent);
}

URogue10mInventoryItemTooltipWidget* URogue10mInventoryItemWidget::CreateInventoryTooltip() const
{
	UClass* TooltipClass = ItemData ? InventoryItemTooltipWidgetClass.LoadSynchronous() : nullptr;
	if (!TooltipClass)
	{
		return nullptr;
	}
	URogue10mInventoryItemTooltipWidget* Tooltip =
		CreateWidget<URogue10mInventoryItemTooltipWidget>(GetOwningPlayer(), TooltipClass);
	if (Tooltip)
	{
		const URogue10mItemDataAsset* Equipped = Inventory && ItemData->Category == ERogue10mItemCategory::Equipment
			? Inventory->GetEquippedItemData(ItemData->EquipSlotType)
			: nullptr;
		Tooltip->InitializeItemTooltip(ItemData, Quantity, Equipped, false);
		Tooltip->SetInventoryItemKind(*ItemData, GetInventoryItemTypeName(*ItemData));
	}
	return Tooltip;
}

void URogue10mInventoryWindowWidget::ShowInventoryItemHover(URogue10mInventoryItemWidget& Item,
	const FGeometry& ItemGeometry, URogue10mInventoryItemTooltipWidget& Tooltip)
{
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!Canvas || !UI_WindowRoot || !IsWindowOpen() || ActiveGridDragOperation.IsValid())
	{
		return;
	}
	HideInventoryItemHover();
	UCanvasPanelSlot* TooltipSlot = Canvas->AddChildToCanvas(&Tooltip);
	if (!TooltipSlot)
	{
		return;
	}

	Tooltip.SetVisibility(ESlateVisibility::HitTestInvisible);
	TooltipSlot->SetAutoSize(true);
	TooltipSlot->SetZOrder(20);
	Tooltip.ForceLayoutPrepass();
	TooltipSlot->SetPosition(GetInventoryTooltipPosition(Canvas->GetCachedGeometry(),
		UI_WindowRoot->GetCachedGeometry(), ItemGeometry, Tooltip.GetDesiredSize()));

	InventoryHoverTooltip = &Tooltip;
	HoveredInventoryItem = &Item;
	Item.SetInventoryHoverVisual(true);
}

void URogue10mInventoryWindowWidget::HideInventoryItemHover(const URogue10mInventoryItemWidget* Item)
{
	if (Item && HoveredInventoryItem.Get() != Item)
	{
		return;
	}
	if (URogue10mInventoryItemWidget* HoveredItem = HoveredInventoryItem.Get())
	{
		HoveredItem->SetInventoryHoverVisual(false);
	}
	HoveredInventoryItem.Reset();
	if (InventoryHoverTooltip)
	{
		InventoryHoverTooltip->RemoveFromParent();
		InventoryHoverTooltip = nullptr;
	}
}
