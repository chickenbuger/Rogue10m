// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mMenuWindowWidgets.h"

#include "Rogue10mInventoryComponent.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mEquipmentPreviewActor.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Widgets/Rogue10mItemDragDropOperation.h"
#include "Widgets/Rogue10mSkillDragDropOperation.h"

void URogue10mMenuWindowWidget::InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
	BP_OnMenuWindowInitialized();
}

URogue10mInventoryComponent* URogue10mMenuWindowWidget::GetInventoryComponent() const
{
	if (InventoryComponent)
	{
		return InventoryComponent;
	}

	const ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	return RogueCharacter ? RogueCharacter->GetInventoryComponent() : nullptr;
}

void URogue10mMenuWindowWidget::SetWindowOpen(bool bOpen)
{
	const bool bChanged = bWindowOpen != bOpen;
	bWindowOpen = bOpen;
	if (!bWindowOpen)
	{
		bWindowDragging = false;
	}
	// 전체 화면 UserWidget의 투명 영역은 다른 메뉴 창의 입력을 막지 않고,
	// 실제 창 자식만 Hit Test에 참여하도록 합니다.
	SetVisibility(bWindowOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bWindowOpen)
	{
		if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(GetOwningPlayer()))
		{
			Controller->BringMenuWindowToFront(this);
		}
	}
	if (bChanged)
	{
		BP_OnWindowOpenChanged(bWindowOpen);
	}
}

void URogue10mMenuWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

FReply URogue10mMenuWindowWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsWindowOpen() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(GetOwningPlayer()))
		{
			Controller->BringMenuWindowToFront(this);
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URogue10mMenuWindowWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& UI_WindowDragHandle
		&& UI_WindowDragHandle->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		if (const UCanvasPanelSlot* WindowSlot = Cast<UCanvasPanelSlot>(UI_WindowRoot ? UI_WindowRoot->Slot : nullptr))
		{
			bWindowDragging = true;
			WindowDragStartScreenPosition = ScreenPosition;
			WindowDragStartCanvasPosition = WindowSlot->GetPosition();
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URogue10mMenuWindowWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bWindowDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bWindowDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply URogue10mMenuWindowWidget::NativeOnMouseMove(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UCanvasPanelSlot* WindowSlot = Cast<UCanvasPanelSlot>(UI_WindowRoot ? UI_WindowRoot->Slot : nullptr);
	if (!bWindowDragging || !WindowSlot || !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D CurrentLocal = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D StartLocal = InGeometry.AbsoluteToLocal(WindowDragStartScreenPosition);
	FVector2D TargetPosition = WindowDragStartCanvasPosition + CurrentLocal - StartLocal;

	const FVector2D ParentSize = InGeometry.GetLocalSize();
	const FVector2D WindowSize = WindowSlot->GetSize();
	const FVector2D Alignment = WindowSlot->GetAlignment();
	const FAnchors Anchors = WindowSlot->GetAnchors();
	const FVector2D AnchorPosition(
		ParentSize.X * Anchors.Minimum.X,
		ParentSize.Y * Anchors.Minimum.Y);
	const FVector2D MinimumPosition(
		WindowSize.X * Alignment.X - AnchorPosition.X,
		WindowSize.Y * Alignment.Y - AnchorPosition.Y);
	const FVector2D MaximumPosition(
		ParentSize.X - WindowSize.X * (1.0f - Alignment.X) - AnchorPosition.X,
		ParentSize.Y - WindowSize.Y * (1.0f - Alignment.Y) - AnchorPosition.Y);
	const auto ClampAxis = [](float Value, float Minimum, float Maximum)
	{
		return Minimum <= Maximum ? FMath::Clamp(Value, Minimum, Maximum) : (Minimum + Maximum) * 0.5f;
	};
	TargetPosition.X = ClampAxis(TargetPosition.X, MinimumPosition.X, MaximumPosition.X);
	TargetPosition.Y = ClampAxis(TargetPosition.Y, MinimumPosition.Y, MaximumPosition.Y);
	WindowSlot->SetPosition(TargetPosition);
	return FReply::Handled();
}

void URogue10mMenuWindowWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bWindowDragging = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

namespace
{
FText FormatTooltipWeight(const URogue10mItemDataAsset* ItemData, int32 Quantity)
{
	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumFractionalDigits = 0;
	NumberFormat.MaximumFractionalDigits = 2;
	const float UnitWeight = ItemData ? FMath::Max(0.0f, ItemData->UnitWeight) : 0.0f;
	const int32 SafeQuantity = FMath::Max(1, Quantity);
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("UnitWeight"), FText::AsNumber(UnitWeight, &NumberFormat));
	if (SafeQuantity > 1)
	{
		Arguments.Add(TEXT("Quantity"), FText::AsNumber(SafeQuantity));
		Arguments.Add(TEXT("TotalWeight"), FText::AsNumber(UnitWeight * SafeQuantity, &NumberFormat));
		return FText::Format(NSLOCTEXT("Rogue10mInventory", "StackItemTooltipWeight",
			"무게: {UnitWeight} kg × {Quantity} = {TotalWeight} kg"), Arguments);
	}
	return FText::Format(NSLOCTEXT("Rogue10mInventory", "SingleItemTooltipWeight",
		"무게: {UnitWeight} kg"), Arguments);
}

void SetTooltipIcon(UImage* Image, const URogue10mItemDataAsset* ItemData)
{
	if (!Image)
	{
		return;
	}
	UTexture2D* Icon = ItemData ? ItemData->InventoryIcon.LoadSynchronous() : nullptr;
	Image->SetBrushFromTexture(Icon, false);
	Image->SetDesiredSizeOverride(FVector2D(52.0f, 52.0f));
	Image->SetBrushTintColor(FSlateColor(FLinearColor::White));
	Image->SetColorAndOpacity(FLinearColor::White);
	Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void SetRarityNameColor(UTextBlock* TextBlock, const URogue10mItemDataAsset* ItemData)
{
	if (TextBlock)
	{
		TextBlock->SetColorAndOpacity(FSlateColor(ItemData
			? ItemData->GetRarityTextColor()
			: FLinearColor(0.92f, 0.92f, 0.92f, 1.0f)));
	}
}

void AddTooltipStatRow(UVerticalBox* Container, const FText& Label, float Value,
	bool bPercent, const FLinearColor& Color)
{
	if (!Container)
	{
		return;
	}
	const float DisplayValue = bPercent ? Value * 100.0f : Value;
	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumFractionalDigits = 0;
	NumberFormat.MaximumFractionalDigits = 2;
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Label"), Label);
	Arguments.Add(TEXT("Sign"), FText::FromString(DisplayValue > KINDA_SMALL_NUMBER
		? TEXT("+") : DisplayValue < -KINDA_SMALL_NUMBER ? TEXT("-") : TEXT("")));
	Arguments.Add(TEXT("Value"), FText::AsNumber(FMath::Abs(DisplayValue), &NumberFormat));
	Arguments.Add(TEXT("Suffix"), bPercent ? FText::FromString(TEXT("%")) : FText::GetEmpty());

	UTextBlock* Row = NewObject<UTextBlock>(Container);
	Row->SetText(FText::Format(NSLOCTEXT("Rogue10mInventory", "EquipmentStatRow",
		"{Label}: {Sign}{Value}{Suffix}"), Arguments));
	FSlateFontInfo Font = Row->GetFont();
	Font.Size = 12;
	Row->SetFont(Font);
	Row->SetColorAndOpacity(FSlateColor(Color));
	Row->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Row->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	Container->AddChildToVerticalBox(Row);
}

void AddEmptyStatRow(UVerticalBox* Container, const FText& Text)
{
	if (!Container)
	{
		return;
	}
	UTextBlock* Row = NewObject<UTextBlock>(Container);
	Row->SetText(Text);
	FSlateFontInfo Font = Row->GetFont();
	Font.Size = 12;
	Row->SetFont(Font);
	Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.64f, 0.68f, 1.0f)));
	Container->AddChildToVerticalBox(Row);
}

void PopulateEquipmentStatRows(UVerticalBox* Container,
	const FRogue10mEquipmentStatModifiers& Stats)
{
	if (!Container)
	{
		return;
	}
	Container->ClearChildren();
	int32 AddedRows = 0;
	const auto Add = [&](const FText& Label, float Value, bool bPercent)
	{
		if (!FMath::IsNearlyZero(Value))
		{
			AddTooltipStatRow(Container, Label, Value, bPercent,
				FLinearColor(0.38f, 0.82f, 1.0f, 1.0f));
			++AddedRows;
		}
	};
	Add(NSLOCTEXT("Rogue10mInventory", "AttackPowerStat", "공격력"), Stats.AttackPowerBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "DefenseStat", "방어력"), Stats.DefenseBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "MaxHealthStat", "최대 체력"), Stats.MaxHealthBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "CriticalChanceStat", "치명타 확률"), Stats.CriticalChanceBonus, true);
	Add(NSLOCTEXT("Rogue10mInventory", "AttackSpeedStat", "공격 속도"), Stats.AttackSpeedBonus, true);
	Add(NSLOCTEXT("Rogue10mInventory", "MoveSpeedStat", "이동 속도"), Stats.MoveSpeedBonus, false);
	if (AddedRows == 0)
	{
		AddEmptyStatRow(Container, NSLOCTEXT("Rogue10mInventory", "NoEquipmentStats",
			"능력치 증가 없음"));
	}
}

void PopulateComparisonRows(UVerticalBox* Container,
	const FRogue10mEquipmentStatModifiers& Candidate,
	const FRogue10mEquipmentStatModifiers& Equipped)
{
	if (!Container)
	{
		return;
	}
	Container->ClearChildren();
	int32 AddedRows = 0;
	const auto Add = [&](const FText& Label, float CandidateValue, float EquippedValue, bool bPercent)
	{
		if (FMath::IsNearlyZero(CandidateValue) && FMath::IsNearlyZero(EquippedValue))
		{
			return;
		}
		const float Delta = CandidateValue - EquippedValue;
		const FLinearColor Color = Delta > KINDA_SMALL_NUMBER
			? FLinearColor(0.25f, 0.9f, 0.35f, 1.0f)
			: Delta < -KINDA_SMALL_NUMBER
				? FLinearColor(0.95f, 0.3f, 0.25f, 1.0f)
				: FLinearColor(0.68f, 0.7f, 0.74f, 1.0f);
		AddTooltipStatRow(Container, Label, Delta, bPercent, Color);
		++AddedRows;
	};
	Add(NSLOCTEXT("Rogue10mInventory", "AttackPowerStat", "공격력"), Candidate.AttackPowerBonus, Equipped.AttackPowerBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "DefenseStat", "방어력"), Candidate.DefenseBonus, Equipped.DefenseBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "MaxHealthStat", "최대 체력"), Candidate.MaxHealthBonus, Equipped.MaxHealthBonus, false);
	Add(NSLOCTEXT("Rogue10mInventory", "CriticalChanceStat", "치명타 확률"), Candidate.CriticalChanceBonus, Equipped.CriticalChanceBonus, true);
	Add(NSLOCTEXT("Rogue10mInventory", "AttackSpeedStat", "공격 속도"), Candidate.AttackSpeedBonus, Equipped.AttackSpeedBonus, true);
	Add(NSLOCTEXT("Rogue10mInventory", "MoveSpeedStat", "이동 속도"), Candidate.MoveSpeedBonus, Equipped.MoveSpeedBonus, false);
	if (AddedRows == 0)
	{
		AddEmptyStatRow(Container, NSLOCTEXT("Rogue10mInventory", "NoEquipmentStatChange",
			"능력치 변화 없음"));
	}
}

void SetCharacterStatText(UTextBlock* TextBlock, const FText& Label, float FinalValue,
	float BaseValue, float EquipmentValue, bool bPercent)
{
	if (!TextBlock)
	{
		return;
	}

	const float DisplayScale = bPercent ? 100.0f : 1.0f;
	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumFractionalDigits = 0;
	NumberFormat.MaximumFractionalDigits = 2;
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Label"), Label);
	Arguments.Add(TEXT("Final"), FText::AsNumber(FinalValue * DisplayScale, &NumberFormat));
	Arguments.Add(TEXT("Base"), FText::AsNumber(BaseValue * DisplayScale, &NumberFormat));
	Arguments.Add(TEXT("Equipment"), FText::AsNumber(FMath::Abs(EquipmentValue * DisplayScale), &NumberFormat));
	Arguments.Add(TEXT("EquipmentSign"), FText::FromString(EquipmentValue < 0.0f ? TEXT("-") : TEXT("+")));
	Arguments.Add(TEXT("Suffix"), bPercent ? FText::FromString(TEXT("%")) : FText::GetEmpty());

	const bool bHasEquipmentBonus = !FMath::IsNearlyZero(EquipmentValue);
	TextBlock->SetText(FText::Format(
		bHasEquipmentBonus
			? NSLOCTEXT("Rogue10mEquipment", "CharacterStatWithEquipment",
				"{Label} {Final}{Suffix}  (기본 {Base}{Suffix} {EquipmentSign} 장비 {Equipment}{Suffix})")
			: NSLOCTEXT("Rogue10mEquipment", "CharacterStatBaseOnly",
				"{Label} {Final}{Suffix}  (기본 {Base}{Suffix})"),
		Arguments));
}
}

void URogue10mInventoryItemTooltipWidget::InitializeItemTooltip(
	const URogue10mItemDataAsset* InItemData, int32 InQuantity,
	const URogue10mItemDataAsset* InEquippedItemData, bool bInEquippedSlot)
{
	const FText FallbackName = NSLOCTEXT("Rogue10mInventory", "UnnamedItem", "이름 없는 아이템");
	const FText FallbackDescription = NSLOCTEXT(
		"Rogue10mInventory", "MissingItemDescription", "아이템 정보가 없습니다.");
	const bool bEquipment = InItemData
		&& InItemData->Category == ERogue10mItemCategory::Equipment;
	const bool bHasEquippedComparison = bEquipment && InEquippedItemData
		&& InEquippedItemData->Category == ERogue10mItemCategory::Equipment
		&& InEquippedItemData->EquipSlotType == InItemData->EquipSlotType;

	if (UI_ItemTooltipSize)
	{
		UI_ItemTooltipSize->SetWidthOverride(bHasEquippedComparison ? 580.0f : 280.0f);
	}
	SetTooltipIcon(UI_ItemIcon, InItemData);

	if (UI_ItemNameText)
	{
		UI_ItemNameText->SetText(InItemData && !InItemData->DisplayName.IsEmpty()
			? InItemData->DisplayName
			: FallbackName);
		SetRarityNameColor(UI_ItemNameText, InItemData);
	}
	if (UI_ItemDescriptionText)
	{
		UI_ItemDescriptionText->SetText(InItemData && !InItemData->Description.IsEmpty()
			? InItemData->Description
			: FallbackDescription);
	}
	if (UI_ItemWeightText)
	{
		UI_ItemWeightText->SetText(FormatTooltipWeight(InItemData, InQuantity));
	}
	if (UI_ItemStatsSection)
	{
		if (UI_ItemStatsTitleText)
		{
			UI_ItemStatsTitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
	if (UI_ItemActionHintText)
	{
		FText ActionHint;
		if (bInEquippedSlot)
		{
			ActionHint = NSLOCTEXT("Rogue10mInventory", "UnequipItemHint",
				"\uC6B0\uD074\uB9AD : \uC7A5\uCC29 \uD574\uC81C");
		}
		else if (InItemData && InItemData->Category == ERogue10mItemCategory::Equipment)
		{
			ActionHint = NSLOCTEXT("Rogue10mInventory", "EquipItemHint",
				"\uC6B0\uD074\uB9AD : \uC7A5\uCC29\uD558\uAE30");
		}
		else if (InItemData && InItemData->Category == ERogue10mItemCategory::Consumable)
		{
			ActionHint = NSLOCTEXT("Rogue10mInventory", "UseItemHint",
				"\uC6B0\uD074\uB9AD : \uC0AC\uC6A9\uD558\uAE30");
		}
		UI_ItemActionHintText->SetText(ActionHint);
		UI_ItemActionHintText->SetVisibility(ActionHint.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

		UI_ItemStatsSection->SetVisibility(bEquipment
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bEquipment)
	{
		PopulateEquipmentStatRows(UI_ItemStatsContainer, InItemData->EquipmentStats);
	}

	if (UI_EquippedItemPanel)
	{
		UI_EquippedItemPanel->SetVisibility(bHasEquippedComparison
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bHasEquippedComparison)
	{
		SetTooltipIcon(UI_EquippedItemIcon, InEquippedItemData);
		if (UI_EquippedItemNameText)
		{
			UI_EquippedItemNameText->SetText(!InEquippedItemData->DisplayName.IsEmpty()
				? InEquippedItemData->DisplayName : FallbackName);
			SetRarityNameColor(UI_EquippedItemNameText, InEquippedItemData);
		}
		if (UI_EquippedItemDescriptionText)
		{
			UI_EquippedItemDescriptionText->SetText(!InEquippedItemData->Description.IsEmpty()
				? InEquippedItemData->Description : FallbackDescription);
		}
		PopulateEquipmentStatRows(UI_EquippedItemStatsContainer,
			InEquippedItemData->EquipmentStats);
		PopulateComparisonRows(UI_ComparisonStatsContainer, InItemData->EquipmentStats,
			InEquippedItemData->EquipmentStats);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URogue10mEquipmentSlotActionWidget::InitializeActionMenu(
	URogue10mInventoryComponent* InInventory,
	ERogue10mInventorySlotType InSlotType,
	const URogue10mItemDataAsset* InItemData,
	URogue10mEquipmentWindowWidget* InOwnerWindow)
{
	Inventory = InInventory;
	SlotType = InSlotType;
	OwnerWindow = InOwnerWindow;

	if (UI_EquipmentItemNameText)
	{
		UI_EquipmentItemNameText->SetText(InItemData && !InItemData->DisplayName.IsEmpty()
			? InItemData->DisplayName
			: NSLOCTEXT("Rogue10mEquipment", "UnnamedEquippedItem", "\uC7A5\uCC29 \uC544\uC774\uD15C"));
		SetRarityNameColor(UI_EquipmentItemNameText, InItemData);
	}
	if (UI_UnequipButtonText)
	{
		UI_UnequipButtonText->SetText(
			NSLOCTEXT("Rogue10mEquipment", "UnequipAction", "\uC7A5\uCC29 \uD574\uC81C"));
	}
	if (UI_ActionResultText)
	{
		UI_ActionResultText->SetText(FText::GetEmpty());
		UI_ActionResultText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URogue10mEquipmentSlotActionWidget::SetMenuPosition(
	FVector2D InPosition, FVector2D InSize)
{
	if (UI_EquipmentSlotActionSize)
	{
		if (UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(UI_EquipmentSlotActionSize->Slot))
		{
			MenuSlot->SetPosition(InPosition);
			MenuSlot->SetSize(InSize);
		}
	}
}

void URogue10mEquipmentSlotActionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UI_UnequipButton)
	{
		UI_UnequipButton->OnClicked.AddUniqueDynamic(
			this, &URogue10mEquipmentSlotActionWidget::HandleUnequipClicked);
	}
}

FReply URogue10mEquipmentSlotActionWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (UI_EquipmentSlotActionSize
		&& !UI_EquipmentSlotActionSize->GetCachedGeometry().IsUnderLocation(
			InMouseEvent.GetScreenSpacePosition()))
	{
		if (OwnerWindow.IsValid())
		{
			OwnerWindow->CloseEquipmentSlotActionMenu();
		}
		else
		{
			RemoveFromParent();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void URogue10mEquipmentSlotActionWidget::HandleUnequipClicked()
{
	if (Inventory && Inventory->TryUnequipItemToFirstAvailableGrid(SlotType))
	{
		RemoveFromParent();
		return;
	}
	if (UI_ActionResultText)
	{
		UI_ActionResultText->SetText(NSLOCTEXT(
			"Rogue10mEquipment", "UnequipNoInventorySpace", "\uC778\uBCA4\uD1A0\uB9AC \uACF5\uAC04\uC774 \uBD80\uC871\uD569\uB2C8\uB2E4."));
		UI_ActionResultText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}


void URogue10mInventoryItemWidget::InitializeGridItem(
	URogue10mInventoryComponent* InInventory,
	URogue10mInventoryWindowWidget* InInventoryWindow,
	int32 InContainerIndex,
	const FRogue10mInventoryGridEntry& InEntry,
	float InCellSize)
{
	Inventory = InInventory;
	InventoryWindow = InInventoryWindow;
	ContainerIndex = InContainerIndex;
	InstanceId = InEntry.InstanceId;
	ItemData = InEntry.ItemData;
	Quantity = InEntry.Quantity;
	CellSize = InCellSize;

	const FIntPoint Footprint = ItemData ? ItemData->GetClampedInventorySize() : FIntPoint(1, 1);
	if (UI_InventoryItemSize)
	{
		UI_InventoryItemSize->SetWidthOverride(Footprint.X * CellSize);
		UI_InventoryItemSize->SetHeightOverride(Footprint.Y * CellSize);
		UI_InventoryItemSize->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	}
	if (UI_InventoryItemRarityBackground)
	{
		UI_InventoryItemRarityBackground->SetBrushColor(ItemData
			? ItemData->GetInventoryRarityBackgroundColor()
			: FLinearColor(0.12f, 0.12f, 0.12f, 0.2f));
	}
	UTexture2D* Icon = ItemData ? ItemData->InventoryIcon.LoadSynchronous() : nullptr;

	if (UI_InventoryItemIcon)
	{
		// Compute the fitted size explicitly so the icon always receives non-zero paint geometry.
		UI_InventoryItemIcon->SetBrushFromTexture(Icon, false);
		UI_InventoryItemIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));
		UI_InventoryItemIcon->SetColorAndOpacity(FLinearColor::White);
		UI_InventoryItemIcon->SetRenderOpacity(1.0f);
		if (Icon)
		{
			const float TargetWidth = Footprint.X * CellSize; 
			const float TargetHeight = Footprint.Y * CellSize; 

			FSlateBrush IconBrush = UI_InventoryItemIcon->GetBrush();
			IconBrush.SetResourceObject(Icon);

			// margin 4px to avoid clipping the icon
			IconBrush.SetImageSize(FVector2D(TargetWidth-8.0f, TargetHeight-8.0f));

			UI_InventoryItemIcon->SetBrush(IconBrush);
		}
		UI_InventoryItemIcon->SetRenderScale(FVector2D::UnitVector);
		UI_InventoryItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UI_InventoryItemQuantityText)
	{
		const bool bShowQuantity = ItemData
			&& ItemData->Category != ERogue10mItemCategory::Equipment
			&& Quantity > 1;
		FText Label = FText::GetEmpty();
		if (Icon)
		{
			Label = bShowQuantity ? FText::AsNumber(Quantity) : FText::GetEmpty();
		}
		else
		{
			const FString FallbackLabel = bShowQuantity
				? FString::Printf(TEXT("%d×%d · %d"), Footprint.X, Footprint.Y, Quantity)
				: FString::Printf(TEXT("%d×%d"), Footprint.X, Footprint.Y);
			Label = FText::FromString(FallbackLabel);
		}
		UI_InventoryItemQuantityText->SetText(Label);
	}
	SetToolTip(nullptr);
	UClass* TooltipClass = ItemData ? InventoryItemTooltipWidgetClass.LoadSynchronous() : nullptr;
	if (TooltipClass)
	{
		if (URogue10mInventoryItemTooltipWidget* TooltipWidget =
			CreateWidget<URogue10mInventoryItemTooltipWidget>(GetOwningPlayer(), TooltipClass))
		{
			const URogue10mItemDataAsset* EquippedItemData = ItemData && Inventory
				&& ItemData->Category == ERogue10mItemCategory::Equipment
				? Inventory->GetEquippedItemData(ItemData->EquipSlotType) : nullptr;
			TooltipWidget->InitializeItemTooltip(ItemData, Quantity, EquippedItemData, false);
			SetToolTip(TooltipWidget);
		}
	}
	SetPlacementPreview(false, true);
}

void URogue10mInventoryItemWidget::SetPlacementPreview(bool bPreviewing, bool bCanPlace)
{
	if (UI_InventoryItemPreviewBorder)
	{
		UI_InventoryItemPreviewBorder->SetBrushColor(bPreviewing
			? (bCanPlace ? FLinearColor(0.1f, 0.8f, 0.2f, 0.45f) : FLinearColor(0.9f, 0.08f, 0.05f, 0.55f))
			: FLinearColor(0.015f, 0.018f, 0.024f, 0.08f));
	}
}


FReply URogue10mInventoryItemWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ItemData && Inventory && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		switch (ItemData->Category)
		{
		case ERogue10mItemCategory::Consumable:
			Inventory->TryUseGridConsumable(ContainerIndex, InstanceId);
			return FReply::Handled();
		case ERogue10mItemCategory::Equipment:
			Inventory->TryEquipGridItem(ContainerIndex, InstanceId);
			return FReply::Handled();
		default:
			break;
		}
	}

	if (ItemData && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FIntPoint Size = ItemData->GetClampedInventorySize();
		DragGrabOffset = FIntPoint(
			FMath::Clamp(FMath::FloorToInt(Local.X / CellSize), 0, Size.X - 1),
			FMath::Clamp(FMath::FloorToInt(Local.Y / CellSize), 0, Size.Y - 1));
		return UWidgetBlueprintLibrary::DetectDragIfPressed(
			InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URogue10mInventoryItemWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	if (!Inventory || !InventoryWindow || !ItemData || !InstanceId.IsValid())
	{
		return;
	}

	URogue10mItemDragDropOperation* Operation = NewObject<URogue10mItemDragDropOperation>(this);
	Operation->InitializeGridItemDrag(Inventory, InventoryWindow, ContainerIndex, InstanceId,
		ItemData, CellSize);
	Operation->Pivot = EDragPivot::MouseDown;
	Operation->GrabCellOffset = DragGrabOffset;

	URogue10mInventoryItemWidget* Preview = CreateWidget<URogue10mInventoryItemWidget>(
		GetOwningPlayer(), GetClass());
	if (Preview)
	{
		FRogue10mInventoryGridEntry PreviewEntry;
		PreviewEntry.InstanceId = InstanceId;
		PreviewEntry.ItemData = ItemData;
		PreviewEntry.Quantity = Quantity;
		Preview->InitializeGridItem(Inventory, InventoryWindow, ContainerIndex, PreviewEntry, CellSize);
		Preview->SetToolTip(nullptr);
		Preview->SetPlacementPreview(true, true);
		Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
		Operation->PreviewWidget = Preview;
		Operation->DefaultDragVisual = Preview;
	}
	InventoryWindow->BeginGridItemDrag(Operation);
	OutOperation = Operation;
}

void URogue10mBagTabWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UI_BagTabButton)
	{
		UI_BagTabButton->OnClicked.AddUniqueDynamic(this, &URogue10mBagTabWidget::HandleBagTabClicked);
	}
}

void URogue10mBagTabWidget::InitializeBagTab(
	URogue10mInventoryWindowWidget* InInventoryWindow,
	int32 InContainerIndex,
	const FText& DisplayName,
	bool bSelected)
{
	InventoryWindow = InInventoryWindow;
	ContainerIndex = InContainerIndex;
	if (UI_BagNameText)
	{
		UI_BagNameText->SetText(DisplayName);
	}
	SetRenderOpacity(bSelected ? 1.0f : 0.55f);
}

void URogue10mBagTabWidget::HandleBagTabClicked()
{
	// 현재 인벤토리 화면은 기본 컨테이너 하나로 고정되어 있습니다.
}
void URogue10mInventoryWindowWidget::InitializeMenuWindow(
	URogue10mInventoryComponent* InInventoryComponent)
{
	if (URogue10mInventoryComponent* PreviousInventory = GetInventoryComponent())
	{
		PreviousInventory->OnInventoryGridChanged.RemoveDynamic(
			this, &URogue10mInventoryWindowWidget::HandleInventoryGridChanged);
	}

	Super::InitializeMenuWindow(InInventoryComponent);
	if (InInventoryComponent)
	{
		InInventoryComponent->OnInventoryGridChanged.AddUniqueDynamic(
			this, &URogue10mInventoryWindowWidget::HandleInventoryGridChanged);
	}
	RefreshInventoryDisplay();
}

void URogue10mInventoryWindowWidget::SetWindowOpen(bool bOpen)
{
	Super::SetWindowOpen(bOpen);
	if (bOpen)
	{
		RefreshInventoryDisplay();
	}
}

void URogue10mInventoryWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (URogue10mInventoryComponent* Inventory = GetInventoryComponent())
	{
		Inventory->OnInventoryGridChanged.AddUniqueDynamic(
			this, &URogue10mInventoryWindowWidget::HandleInventoryGridChanged);
	}
	RefreshInventoryDisplay();
}

void URogue10mInventoryWindowWidget::NativeDestruct()
{
	if (URogue10mInventoryComponent* Inventory = GetInventoryComponent())
	{
		Inventory->OnInventoryGridChanged.RemoveDynamic(
			this, &URogue10mInventoryWindowWidget::HandleInventoryGridChanged);
	}
	Super::NativeDestruct();
}

void URogue10mInventoryWindowWidget::BeginGridItemDrag(URogue10mItemDragDropOperation* Operation)
{
	ActiveGridDragOperation = Operation;
	Operation->InventoryWindow = this;
	SetIsFocusable(true);
	SetFocus();
}

void URogue10mInventoryWindowWidget::EndGridItemDrag(URogue10mItemDragDropOperation* Operation)
{
	if (!Operation || ActiveGridDragOperation.Get() == Operation)
	{
		ClearGridDropPreview();
		ActiveGridDragOperation.Reset();
	}
}


bool URogue10mInventoryWindowWidget::NativeOnDragOver(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URogue10mItemDragDropOperation* Operation = Cast<URogue10mItemDragDropOperation>(InOperation);
	const bool bGridDropSource = Operation
		&& (Operation->Source == ERogue10mItemDragSource::GridInventory
			|| Operation->Source == ERogue10mItemDragSource::Equipment);
	if (!bGridDropSource)
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}
	if (ActiveGridDragOperation.Get() != Operation)
	{
		BeginGridItemDrag(Operation);
	}
	return UpdateGridDropPreview(InDragDropEvent, Operation);
}

bool URogue10mInventoryWindowWidget::NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URogue10mItemDragDropOperation* Operation = Cast<URogue10mItemDragDropOperation>(InOperation);
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Operation || !Inventory
		|| (Operation->Source != ERogue10mItemDragSource::GridInventory
			&& Operation->Source != ERogue10mItemDragSource::Equipment))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	UpdateGridDropPreview(InDragDropEvent, Operation);
	bool bMoved = false;
	if (Operation->bPreviewCanPlace)
	{
		bMoved = Operation->Source == ERogue10mItemDragSource::Equipment
			? Inventory->TryUnequipItemToGrid(Operation->EquipmentSlotType, PrimaryContainerIndex,
				Operation->PreviewGridPosition)
			: Inventory->TryMoveGridItem(Operation->SourceContainerIndex, Operation->InstanceId,
				PrimaryContainerIndex, Operation->PreviewGridPosition);
	}
	if (bMoved)
	{
		Operation->MarkDropHandled();
	}
	EndGridItemDrag(Operation);
	return bMoved;
}

void URogue10mInventoryWindowWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	ClearGridDropPreview();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool URogue10mInventoryWindowWidget::UpdateGridDropPreview(const FDragDropEvent& DragDropEvent,
	URogue10mItemDragDropOperation* Operation)
{
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory || !UI_InventoryItemCanvas || !Operation || !Operation->ItemData)
	{
		return false;
	}
	const FVector2D Local = UI_InventoryItemCanvas->GetCachedGeometry().AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	Operation->PreviewGridPosition = FIntPoint(
		FMath::FloorToInt(Local.X / InventoryCellSize) - Operation->GrabCellOffset.X,
		FMath::FloorToInt(Local.Y / InventoryCellSize) - Operation->GrabCellOffset.Y);
	const FGuid IgnoredId = Operation->SourceContainerIndex == PrimaryContainerIndex ? Operation->InstanceId : FGuid();
	Operation->bPreviewCanPlace = Inventory->CanPlaceGridItem(PrimaryContainerIndex, Operation->ItemData,
		Operation->PreviewGridPosition, IgnoredId);
	Operation->SetGridPlacementPreview(true, Operation->bPreviewCanPlace);
	return true;
}

void URogue10mInventoryWindowWidget::ClearGridDropPreview()
{
	if (URogue10mItemDragDropOperation* Operation = ActiveGridDragOperation.Get())
	{
		Operation->SetGridPlacementPreview(false, false);
	}
}
void URogue10mInventoryWindowWidget::HandleInventoryGridChanged()
{
	RefreshInventoryDisplay();
}

void URogue10mInventoryWindowWidget::RefreshInventoryDisplay()
{
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory)
	{
		return;
	}
	const TArray<FRogue10mInventoryContainer>& Containers = Inventory->GetInventoryContainers();
	if (Containers.IsValidIndex(PrimaryContainerIndex))
	{
		RebuildInventoryCells(Containers[PrimaryContainerIndex].GridSize);
		RebuildInventoryItems();
	}
	if (UI_InventoryMoneyText)
	{
		UI_InventoryMoneyText->SetText(FText::Format(NSLOCTEXT("Rogue10mInventory", "InventoryMoney", "Gold: {0}"), FText::AsNumber(Inventory->GetGold())));
	}
	if (UI_InventoryWeightText)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 1;
		Format.MaximumFractionalDigits = 1;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(Inventory->GetTotalInventoryWeight(), &Format));
		Args.Add(TEXT("Maximum"), FText::AsNumber(Inventory->GetMaxCarryWeight(), &Format));
		UI_InventoryWeightText->SetText(FText::Format(NSLOCTEXT("Rogue10mInventory", "InventoryWeight", "Weight: {Current} / {Maximum} kg"), Args));
	}
}
void URogue10mInventoryWindowWidget::RebuildInventoryCells(FIntPoint GridSize)
{
	if (!UI_InventoryGrid)
	{
		return;
	}

	GridSize.X = FMath::Clamp(GridSize.X, 1, 20);
	GridSize.Y = FMath::Clamp(GridSize.Y, 1, 20);
	if (GridSize == CachedGridSize && UI_InventoryGrid->GetChildrenCount() == GridSize.X * GridSize.Y)
	{
		return;
	}

	UI_InventoryGrid->ClearChildren();
	CachedGridSize = GridSize;
	if (!InventoryCellWidgetClass)
	{
		UE_LOG(LogRogue10m, Warning,
			TEXT("InventoryCellWidgetClass가 설정되지 않아 인벤토리 셀을 생성하지 못했습니다."));
		return;
	}

	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			URogue10mInventoryCellWidget* Cell = CreateWidget<URogue10mInventoryCellWidget>(
				GetOwningPlayer(), InventoryCellWidgetClass);
			if (!Cell)
			{
				continue;
			}

			Cell->SetGridPosition(FIntPoint(X, Y));
			UI_InventoryGrid->AddChildToUniformGrid(Cell, Y, X);
		}
	}
}

void URogue10mInventoryWindowWidget::RebuildInventoryItems()
{
	if (!UI_InventoryItemCanvas)
	{
		return;
	}
	UI_InventoryItemCanvas->ClearChildren();
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory || !InventoryItemWidgetClass || !Inventory->GetInventoryContainers().IsValidIndex(PrimaryContainerIndex))
	{
		return;
	}
	const FRogue10mInventoryContainer& Container = Inventory->GetInventoryContainers()[PrimaryContainerIndex];
	for (const FRogue10mInventoryGridEntry& Entry : Container.Entries)
	{
		if (!Entry.ItemData)
		{
			continue;
		}
		URogue10mInventoryItemWidget* ItemWidget = CreateWidget<URogue10mInventoryItemWidget>(GetOwningPlayer(), InventoryItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}
		ItemWidget->InitializeGridItem(Inventory, this, PrimaryContainerIndex, Entry, InventoryCellSize);
		UCanvasPanelSlot* CanvasSlot = UI_InventoryItemCanvas->AddChildToCanvas(ItemWidget);
		const FIntPoint Size = Entry.ItemData->GetClampedInventorySize();
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetPosition(FVector2D(Entry.Position.X * InventoryCellSize, Entry.Position.Y * InventoryCellSize));
		CanvasSlot->SetSize(FVector2D(Size.X * InventoryCellSize, Size.Y * InventoryCellSize));
	}
}


void URogue10mEquipmentDragSourceWidget::InitializeDragSource(
	URogue10mEquipmentWindowWidget* InEquipmentWindow,
	ERogue10mInventorySlotType InSlotType,
	const FAnchorData& InHomeLayout)
{
	EquipmentWindow = InEquipmentWindow;
	SlotType = InSlotType;
	HomeLayout = InHomeLayout;
	SetIsFocusable(false);
}


void URogue10mEquipmentDragSourceWidget::SetDragLayout(
	FVector2D ParentLocalCursorPosition,
	FVector2D DragVisualSize)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		FAnchorData DragLayout;
		DragLayout.Anchors = FAnchors(0.0f, 0.0f);
		DragLayout.Alignment = FVector2D::ZeroVector;
		DragLayout.Offsets = FMargin(
			ParentLocalCursorPosition.X - DragVisualSize.X * 0.5,
			ParentLocalCursorPosition.Y - DragVisualSize.Y * 0.5,
			DragVisualSize.X,
			DragVisualSize.Y);
		CanvasSlot->SetLayout(DragLayout);
	}
}

void URogue10mEquipmentDragSourceWidget::RestoreHomeLayout()
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetLayout(HomeLayout);
	}
}

FReply URogue10mEquipmentDragSourceWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (URogue10mEquipmentWindowWidget* Window = EquipmentWindow.Get())
	{
		return Window->HandleEquipmentDragSourceMouseButtonDown(
			this, SlotType, InGeometry, InMouseEvent);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
void URogue10mEquipmentDragSourceWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	if (URogue10mEquipmentWindowWidget* Window = EquipmentWindow.Get())
	{
		Window->CreateEquipmentItemDragOperation(OutOperation);
		return;
	}

	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
}

FReply URogue10mEquipmentDragSourceWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (URogue10mEquipmentWindowWidget* Window = EquipmentWindow.Get())
	{
		Window->HandleEquipmentDragFinished(nullptr);
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void URogue10mEquipmentWindowWidget::ClearEquipmentDragSourceProxies()
{
	for (URogue10mEquipmentDragSourceWidget* DragSourceProxy : EquipmentDragSourceProxies)
	{
		if (DragSourceProxy)
		{
			DragSourceProxy->RemoveFromParent();
		}
	}
	EquipmentDragSourceProxies.Reset();
	ActiveEquipmentDragSourceProxy.Reset();
}

void URogue10mEquipmentWindowWidget::RebuildEquipmentDragSourceProxies()
{
	ClearEquipmentDragSourceProxies();
	if (!UI_EquipmentSlotContainer)
	{
		return;
	}

	struct FEquipmentDragSourceCandidate
	{
		ERogue10mInventorySlotType SlotType;
		UBorder* Frame;
	};
	const FEquipmentDragSourceCandidate Candidates[] = {
		{ERogue10mInventorySlotType::MainWeapon, UI_WeaponSlotFrame},
		{ERogue10mInventorySlotType::Head, UI_HeadSlotFrame},
		{ERogue10mInventorySlotType::Armor, UI_ChestSlotFrame},
		{ERogue10mInventorySlotType::Hands, UI_HandsSlotFrame},
		{ERogue10mInventorySlotType::Ring, UI_RingSlotFrame},
		{ERogue10mInventorySlotType::Shoes, UI_FeetSlotFrame},
		{ERogue10mInventorySlotType::Necklace, UI_NecklaceSlotFrame}
	};

	for (const FEquipmentDragSourceCandidate& Candidate : Candidates)
	{
		UCanvasPanelSlot* FrameSlot = Candidate.Frame
			? Cast<UCanvasPanelSlot>(Candidate.Frame->Slot) : nullptr;
		if (!FrameSlot)
		{
			continue;
		}

		URogue10mEquipmentDragSourceWidget* DragSourceProxy =
			CreateWidget<URogue10mEquipmentDragSourceWidget>(
				GetOwningPlayer(), URogue10mEquipmentDragSourceWidget::StaticClass());
		if (!DragSourceProxy)
		{
			continue;
		}

		const FAnchorData HomeLayout = FrameSlot->GetLayout();
		UCanvasPanelSlot* ProxySlot = UI_EquipmentSlotContainer->AddChildToCanvas(DragSourceProxy);
		ProxySlot->SetLayout(HomeLayout);
		ProxySlot->SetAutoSize(FrameSlot->GetAutoSize());
		ProxySlot->SetZOrder(FrameSlot->GetZOrder() + 20);
		DragSourceProxy->InitializeDragSource(this, Candidate.SlotType, HomeLayout);
		DragSourceProxy->SetRenderOpacity(0.0f);

		const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(Candidate.SlotType);
		const bool bHasEquippedItem = EquipmentSlot && !EquipmentSlot->bLocked
			&& EquipmentSlot->bHasEquippedItem && EquipmentSlot->EquippedItem.bOccupied
			&& EquipmentSlot->EquippedItem.ItemData;
		DragSourceProxy->SetVisibility(
			bHasEquippedItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RefreshEquipmentSlotTooltip(DragSourceProxy, Candidate.SlotType);
		EquipmentDragSourceProxies.Add(DragSourceProxy);
	}
}

void URogue10mEquipmentWindowWidget::InitializeMenuWindow(URogue10mInventoryComponent* InInventoryComponent)
{
	if (URogue10mInventoryComponent* PreviousInventory = GetInventoryComponent())
	{
		PreviousInventory->OnEquipmentChanged.RemoveDynamic(
			this, &URogue10mEquipmentWindowWidget::HandleEquipmentChanged);
	}
	Super::InitializeMenuWindow(InInventoryComponent);
	if (URogue10mInventoryComponent* Inventory = GetInventoryComponent())
	{
		Inventory->OnEquipmentChanged.AddUniqueDynamic(
			this, &URogue10mEquipmentWindowWidget::HandleEquipmentChanged);
	}
	RefreshEquipmentDisplay();
}

void URogue10mEquipmentWindowWidget::SetWindowOpen(bool bOpen)
{
	if (!bOpen)
	{
		bCharacterPreviewRotating = false;
		HandleEquipmentDragFinished(nullptr);
	}
	Super::SetWindowOpen(bOpen);
	ClearEquipmentDropPreview();
	if (bOpen)
	{
		RefreshEquipmentDisplay();
		if (EnsureCharacterPreview())
		{
			CharacterPreviewActor->SetPreviewActive(true);
		}
	}
	else
	{
		CloseEquipmentSlotActionMenu();
		if (CharacterPreviewActor)
		{
			CharacterPreviewActor->SetPreviewActive(false);
		}
	}
}

void URogue10mEquipmentWindowWidget::NativeDestruct()
{
	bCharacterPreviewRotating = false;
	HandleEquipmentDragFinished(nullptr);
	ClearEquipmentDragSourceProxies();
	ClearEquipmentDropPreview();
	CloseEquipmentSlotActionMenu();
	if (URogue10mInventoryComponent* Inventory = GetInventoryComponent())
	{
		Inventory->OnEquipmentChanged.RemoveDynamic(
			this, &URogue10mEquipmentWindowWidget::HandleEquipmentChanged);
	}
	DestroyCharacterPreview();
	Super::NativeDestruct();
}

void URogue10mEquipmentWindowWidget::HandleEquipmentChanged()
{
	ClearEquipmentDropPreview();
	CloseEquipmentSlotActionMenu();
	RefreshEquipmentDisplay();
	RefreshCharacterPreview();
}

const FRogue10mInventorySlot* URogue10mEquipmentWindowWidget::FindEquipmentSlotData(
	ERogue10mInventorySlotType SlotType) const
{
	const URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory)
	{
		return nullptr;
	}
	for (const FRogue10mInventorySlot& EquipmentSlot : Inventory->GetLeftEquipmentSlots())
	{
		if (EquipmentSlot.SlotType == SlotType)
		{
			return &EquipmentSlot;
		}
	}
	for (const FRogue10mInventorySlot& EquipmentSlot : Inventory->GetRightEquipmentSlots())
	{
		if (EquipmentSlot.SlotType == SlotType)
		{
			return &EquipmentSlot;
		}
	}
	return nullptr;
}

void URogue10mEquipmentWindowWidget::RefreshEquipmentSlotTooltip(
	UWidget* HitWidget, ERogue10mInventorySlotType SlotType)
{
	if (!HitWidget)
	{
		return;
	}

	HitWidget->SetToolTip(nullptr);
	const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(SlotType);
	const URogue10mItemDataAsset* ItemData = EquipmentSlot && !EquipmentSlot->bLocked
		&& EquipmentSlot->bHasEquippedItem && EquipmentSlot->EquippedItem.bOccupied
		? EquipmentSlot->EquippedItem.ItemData.Get() : nullptr;
	UClass* TooltipClass = ItemData ? EquipmentItemTooltipWidgetClass.LoadSynchronous() : nullptr;
	if (!TooltipClass)
	{
		return;
	}

	if (URogue10mInventoryItemTooltipWidget* TooltipWidget =
		CreateWidget<URogue10mInventoryItemTooltipWidget>(GetOwningPlayer(), TooltipClass))
	{
		TooltipWidget->InitializeItemTooltip(
			ItemData, EquipmentSlot->EquippedItem.Quantity, nullptr, true);
		HitWidget->SetToolTip(TooltipWidget);
	}
}

void URogue10mEquipmentWindowWidget::OpenEquipmentSlotActionMenu(
	ERogue10mInventorySlotType SlotType, FVector2D ScreenPosition, UWidget* HitWidget)
{
	CloseEquipmentSlotActionMenu();
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(SlotType);
	const URogue10mItemDataAsset* ItemData = EquipmentSlot && !EquipmentSlot->bLocked
		&& EquipmentSlot->bHasEquippedItem && EquipmentSlot->EquippedItem.bOccupied
		? EquipmentSlot->EquippedItem.ItemData.Get() : nullptr;
	UClass* ActionClass = ItemData ? EquipmentSlotActionWidgetClass.LoadSynchronous() : nullptr;
	if (!Inventory || !ActionClass || !ItemData)
	{
		return;
	}

	URogue10mEquipmentSlotActionWidget* ActionWidget =
		CreateWidget<URogue10mEquipmentSlotActionWidget>(GetOwningPlayer(), ActionClass);
	if (!ActionWidget)
	{
		return;
	}

	ActionWidget->InitializeActionMenu(Inventory, SlotType, ItemData, this);
	const FVector2D MenuSize(220.0, 96.0);
	const FGeometry PlayerScreenGeometry =
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer());
	const FVector2D ViewportSize = PlayerScreenGeometry.GetLocalSize();
	const FVector2D CursorPosition = PlayerScreenGeometry.AbsoluteToLocal(ScreenPosition);
	FVector2D MenuPosition = CursorPosition + FVector2D(12.0, 0.0);
	MenuPosition.X = FMath::Clamp(MenuPosition.X, 0.0,
		FMath::Max(0.0, ViewportSize.X - MenuSize.X));
	MenuPosition.Y = FMath::Clamp(MenuPosition.Y, 0.0,
		FMath::Max(0.0, ViewportSize.Y - MenuSize.Y));
	ActionWidget->AddToViewport(250);
	ActionWidget->SetMenuPosition(MenuPosition, MenuSize);
	ActiveEquipmentSlotActionWidget = ActionWidget;
	SuppressedEquipmentTooltipWidget = HitWidget;
	SuppressedEquipmentTooltipSlotType = SlotType;
	if (HitWidget)
	{
		HitWidget->SetToolTip(nullptr);
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().CloseToolTip();
	}
}

void URogue10mEquipmentWindowWidget::CloseEquipmentSlotActionMenu()
{
	if (ActiveEquipmentSlotActionWidget)
	{
		ActiveEquipmentSlotActionWidget->RemoveFromParent();
		ActiveEquipmentSlotActionWidget = nullptr;
	}
	RestoreSuppressedEquipmentSlotTooltip();
}

void URogue10mEquipmentWindowWidget::RestoreSuppressedEquipmentSlotTooltip()
{
	if (SuppressedEquipmentTooltipWidget.IsValid()
		&& SuppressedEquipmentTooltipSlotType.IsSet())
	{
		RefreshEquipmentSlotTooltip(
			SuppressedEquipmentTooltipWidget.Get(),
			SuppressedEquipmentTooltipSlotType.GetValue());
	}
	SuppressedEquipmentTooltipWidget.Reset();
	SuppressedEquipmentTooltipSlotType.Reset();
}

FReply URogue10mEquipmentWindowWidget::HandleEquipmentDragSourceMouseButtonDown(
	URogue10mEquipmentDragSourceWidget* DragSource,
	ERogue10mInventorySlotType SlotType,
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(SlotType);
	if (!DragSource || !EquipmentSlot || EquipmentSlot->bLocked
		|| !EquipmentSlot->bHasEquippedItem || !EquipmentSlot->EquippedItem.bOccupied
		|| !EquipmentSlot->EquippedItem.ItemData)
	{
		return FReply::Unhandled();
	}

	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OpenEquipmentSlotActionMenu(SlotType, ScreenPosition, DragSource);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	CloseEquipmentSlotActionMenu();
	const URogue10mItemDataAsset* ItemData = EquipmentSlot->EquippedItem.ItemData.Get();
	const FIntPoint Footprint = ItemData->GetClampedInventorySize();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(ScreenPosition);
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	PendingEquipmentGrabCellOffset = FIntPoint(
		FMath::Clamp(FMath::FloorToInt(LocalPosition.X / FMath::Max(1.0, LocalSize.X) * Footprint.X), 0, Footprint.X - 1),
		FMath::Clamp(FMath::FloorToInt(LocalPosition.Y / FMath::Max(1.0, LocalSize.Y) * Footprint.Y), 0, Footprint.Y - 1));
	PendingEquipmentDragSlotType = SlotType;

	const FVector2D DragVisualSize(
		Footprint.X * InventoryDragCellSize,
		Footprint.Y * InventoryDragCellSize);
	const FVector2D ParentLocalCursorPosition =
		UI_EquipmentSlotContainer->GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	DragSource->SetDragLayout(ParentLocalCursorPosition, DragVisualSize);
	UI_EquipmentSlotContainer->ForceLayoutPrepass();
	ActiveEquipmentDragSourceProxy = DragSource;

	return UWidgetBlueprintLibrary::DetectDragIfPressed(
		InMouseEvent, DragSource, EKeys::LeftMouseButton).NativeReply;
}

FReply URogue10mEquipmentWindowWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{

	PendingEquipmentDragSlotType.Reset();
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		struct FEquipmentActionCandidate
		{
			ERogue10mInventorySlotType SlotType;
			UWidget* HitWidget;
		};
		const FEquipmentActionCandidate Candidates[] = {
			{ERogue10mInventorySlotType::MainWeapon, UI_WeaponSlotFrame ? static_cast<UWidget*>(UI_WeaponSlotFrame) : UI_WeaponSlotIcon},
			{ERogue10mInventorySlotType::Head, UI_HeadSlotFrame ? static_cast<UWidget*>(UI_HeadSlotFrame) : UI_HeadSlotIcon},
			{ERogue10mInventorySlotType::Armor, UI_ChestSlotFrame ? static_cast<UWidget*>(UI_ChestSlotFrame) : UI_ChestSlotIcon},
			{ERogue10mInventorySlotType::Hands, UI_HandsSlotFrame ? static_cast<UWidget*>(UI_HandsSlotFrame) : UI_HandsSlotIcon},
			{ERogue10mInventorySlotType::Ring, UI_RingSlotFrame ? static_cast<UWidget*>(UI_RingSlotFrame) : UI_RingSlotIcon},
			{ERogue10mInventorySlotType::Shoes, UI_FeetSlotFrame ? static_cast<UWidget*>(UI_FeetSlotFrame) : UI_FeetSlotIcon},
			{ERogue10mInventorySlotType::Necklace, UI_NecklaceSlotFrame ? static_cast<UWidget*>(UI_NecklaceSlotFrame) : UI_NecklaceSlotIcon}
		};
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		for (const FEquipmentActionCandidate& Candidate : Candidates)
		{
			const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(Candidate.SlotType);
			if (!Candidate.HitWidget || !EquipmentSlot || EquipmentSlot->bLocked
				|| !EquipmentSlot->bHasEquippedItem || !EquipmentSlot->EquippedItem.bOccupied
				|| !EquipmentSlot->EquippedItem.ItemData
				|| !Candidate.HitWidget->GetCachedGeometry().IsUnderLocation(ScreenPosition))
			{
				continue;
			}
			OpenEquipmentSlotActionMenu(Candidate.SlotType, ScreenPosition, Candidate.HitWidget);
			return FReply::Handled();
		}
		CloseEquipmentSlotActionMenu();
	}
	else if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CloseEquipmentSlotActionMenu();
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		if (UI_CharacterPreviewImage
			&& UI_CharacterPreviewImage->GetCachedGeometry().IsUnderLocation(ScreenPosition)
			&& EnsureCharacterPreview())
		{
			bCharacterPreviewRotating = true;
			LastCharacterPreviewDragScreenPosition = ScreenPosition;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URogue10mEquipmentWindowWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bCharacterPreviewRotating
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bCharacterPreviewRotating = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	FReply Reply = Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	if (!ActiveEquipmentDragOperation.IsValid())
	{
		PendingEquipmentDragSlotType.Reset();
		HandleEquipmentDragFinished(nullptr);
	}
	return Reply;
}

FReply URogue10mEquipmentWindowWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bCharacterPreviewRotating)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bCharacterPreviewRotating = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	const FVector2D CurrentScreenPosition = InMouseEvent.GetScreenSpacePosition();
	const float DeltaYaw = static_cast<float>(
		CurrentScreenPosition.X - LastCharacterPreviewDragScreenPosition.X)
		* CharacterPreviewRotationDegreesPerPixel;
	LastCharacterPreviewDragScreenPosition = CurrentScreenPosition;
	if (CharacterPreviewActor && IsValid(CharacterPreviewActor))
	{
		CharacterPreviewActor->AddPreviewYaw(DeltaYaw);
	}
	return FReply::Handled();
}

void URogue10mEquipmentWindowWidget::NativeOnMouseCaptureLost(
	const FCaptureLostEvent& CaptureLostEvent)
{
	bCharacterPreviewRotating = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}


void URogue10mEquipmentWindowWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	CreateEquipmentItemDragOperation(OutOperation);
	if (!OutOperation)
	{
		Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	}
}

void URogue10mEquipmentWindowWidget::CreateEquipmentItemDragOperation(
	UDragDropOperation*& OutOperation)
{
	if (!PendingEquipmentDragSlotType.IsSet())
	{
		return;
	}

	const ERogue10mInventorySlotType SlotType = PendingEquipmentDragSlotType.GetValue();
	PendingEquipmentDragSlotType.Reset();
	const FRogue10mInventorySlot* EquipmentSlot = FindEquipmentSlotData(SlotType);
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory || !EquipmentSlot || EquipmentSlot->bLocked || !EquipmentSlot->bHasEquippedItem
		|| !EquipmentSlot->EquippedItem.bOccupied || !EquipmentSlot->EquippedItem.ItemData)
	{
		HandleEquipmentDragFinished(nullptr);
		return;
	}

	const URogue10mItemDataAsset* ItemData = EquipmentSlot->EquippedItem.ItemData.Get();
	URogue10mItemDragDropOperation* Operation = NewObject<URogue10mItemDragDropOperation>(this);
	Operation->InitializeEquipmentItemDrag(Inventory, SlotType, ItemData,
		EquipmentSlot->EquippedItem.Quantity, InventoryDragCellSize);
	Operation->GrabCellOffset = PendingEquipmentGrabCellOffset;
	Operation->Pivot = EDragPivot::CenterCenter;

	USizeBox* PreviewSize = NewObject<USizeBox>(Operation);
	UBorder* PreviewBorder = NewObject<UBorder>(PreviewSize);
	UScaleBox* PreviewScale = NewObject<UScaleBox>(PreviewBorder);
	UImage* PreviewIcon = NewObject<UImage>(PreviewScale);
	const FIntPoint Footprint = ItemData->GetClampedInventorySize();
	PreviewSize->SetWidthOverride(Footprint.X * InventoryDragCellSize);
	PreviewSize->SetHeightOverride(Footprint.Y * InventoryDragCellSize);
	PreviewSize->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PreviewBorder->SetPadding(FMargin(4.0f));
	Operation->PreviewNeutralColor = ItemData->InventoryTint;
	Operation->PreviewNeutralColor.A = FMath::Min(Operation->PreviewNeutralColor.A, 0.35f);
	PreviewBorder->SetBrushColor(Operation->PreviewNeutralColor);
	PreviewScale->SetStretch(EStretch::ScaleToFit);
	PreviewScale->SetStretchDirection(EStretchDirection::Both);
	PreviewIcon->SetBrushFromTexture(ItemData->InventoryIcon.LoadSynchronous(), true);
	PreviewIcon->SetColorAndOpacity(FLinearColor::White);
	PreviewIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	PreviewScale->AddChild(PreviewIcon);
	PreviewBorder->AddChild(PreviewScale);
	PreviewSize->AddChild(PreviewBorder);
	PreviewSize->SetVisibility(ESlateVisibility::HitTestInvisible);
	Operation->PreviewBorder = PreviewBorder;
	Operation->DefaultDragVisual = PreviewSize;
	Operation->OnDrop.AddDynamic(this, &URogue10mEquipmentWindowWidget::HandleEquipmentDragFinished);
	Operation->OnDragCancelled.AddDynamic(this, &URogue10mEquipmentWindowWidget::HandleEquipmentDragFinished);
	ActiveEquipmentDragOperation = Operation;
	OutOperation = Operation;
}

void URogue10mEquipmentWindowWidget::HandleEquipmentDragFinished(UDragDropOperation* Operation)
{
	if (Operation && ActiveEquipmentDragOperation.Get() != Operation)
	{
		return;
	}

	PendingEquipmentDragSlotType.Reset();
	ActiveEquipmentDragOperation.Reset();
	if (URogue10mEquipmentDragSourceWidget* DragSourceProxy = ActiveEquipmentDragSourceProxy.Get())
	{
		DragSourceProxy->RestoreHomeLayout();
	}
	ActiveEquipmentDragSourceProxy.Reset();
}

TOptional<ERogue10mInventorySlotType> URogue10mEquipmentWindowWidget::FindEquipmentDropSlot(
	FVector2D ScreenPosition, UBorder*& OutFrame) const
{
	OutFrame = nullptr;
	struct FEquipmentDropCandidate
	{
		ERogue10mInventorySlotType SlotType;
		UBorder* Frame;
	};
	const FEquipmentDropCandidate Candidates[] = {
		{ERogue10mInventorySlotType::MainWeapon, UI_WeaponSlotFrame},
		{ERogue10mInventorySlotType::Head, UI_HeadSlotFrame},
		{ERogue10mInventorySlotType::Armor, UI_ChestSlotFrame},
		{ERogue10mInventorySlotType::Hands, UI_HandsSlotFrame},
		{ERogue10mInventorySlotType::Ring, UI_RingSlotFrame},
		{ERogue10mInventorySlotType::Shoes, UI_FeetSlotFrame},
		{ERogue10mInventorySlotType::Necklace, UI_NecklaceSlotFrame}
	};
	for (const FEquipmentDropCandidate& Candidate : Candidates)
	{
		if (Candidate.Frame && Candidate.Frame->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			OutFrame = Candidate.Frame;
			return Candidate.SlotType;
		}
	}
	return TOptional<ERogue10mInventorySlotType>();
}

bool URogue10mEquipmentWindowWidget::UpdateEquipmentDropPreview(
	const FDragDropEvent& DragDropEvent, URogue10mItemDragDropOperation* Operation)
{
	ClearEquipmentDropPreview();
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Operation || !Inventory || Operation->Inventory != Inventory
		|| Operation->Source != ERogue10mItemDragSource::GridInventory
		|| !Operation->ItemData
		|| Operation->ItemData->Category != ERogue10mItemCategory::Equipment)
	{
		return false;
	}

	UBorder* TargetFrame = nullptr;
	const TOptional<ERogue10mInventorySlotType> TargetSlot = FindEquipmentDropSlot(
		DragDropEvent.GetScreenSpacePosition(), TargetFrame);
	if (!TargetSlot.IsSet() || !TargetFrame)
	{
		Operation->SetGridPlacementPreview(false, false);
		return false;
	}

	const bool bCompatible = Operation->ItemData->EquipSlotType == TargetSlot.GetValue();
	ActiveEquipmentDropFrame = TargetFrame;
	ActiveEquipmentDropOperation = Operation;
	ActiveEquipmentDropOriginalColor = TargetFrame->GetBrushColor();
	TargetFrame->SetBrushColor(bCompatible
		? FLinearColor(0.08f, 0.42f, 0.12f, 0.88f)
		: FLinearColor(0.48f, 0.06f, 0.04f, 0.88f));
	Operation->SetGridPlacementPreview(true, bCompatible);
	return true;
}

void URogue10mEquipmentWindowWidget::ClearEquipmentDropPreview()
{
	if (UBorder* Frame = ActiveEquipmentDropFrame.Get())
	{
		Frame->SetBrushColor(ActiveEquipmentDropOriginalColor);
	}
	if (URogue10mItemDragDropOperation* Operation = ActiveEquipmentDropOperation.Get())
	{
		Operation->SetGridPlacementPreview(false, false);
	}
	ActiveEquipmentDropFrame.Reset();
	ActiveEquipmentDropOperation.Reset();
	ActiveEquipmentDropOriginalColor = FLinearColor::Transparent;
}

bool URogue10mEquipmentWindowWidget::NativeOnDragOver(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URogue10mItemDragDropOperation* Operation = Cast<URogue10mItemDragDropOperation>(InOperation);
	if (!Operation || Operation->Source != ERogue10mItemDragSource::GridInventory)
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}
	return UpdateEquipmentDropPreview(InDragDropEvent, Operation);
}

bool URogue10mEquipmentWindowWidget::NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	URogue10mItemDragDropOperation* Operation = Cast<URogue10mItemDragDropOperation>(InOperation);
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Operation || !Inventory || Operation->Inventory != Inventory
		|| Operation->Source != ERogue10mItemDragSource::GridInventory
		|| !Operation->ItemData
		|| Operation->ItemData->Category != ERogue10mItemCategory::Equipment)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	UBorder* TargetFrame = nullptr;
	const TOptional<ERogue10mInventorySlotType> TargetSlot = FindEquipmentDropSlot(
		InDragDropEvent.GetScreenSpacePosition(), TargetFrame);
	const bool bCompatible = TargetSlot.IsSet() && TargetFrame
		&& Operation->ItemData->EquipSlotType == TargetSlot.GetValue();
	ClearEquipmentDropPreview();
	if (!bCompatible)
	{
		return false;
	}

	const bool bEquipped = Inventory->TryEquipGridItem(
		Operation->SourceContainerIndex, Operation->InstanceId);
	if (bEquipped)
	{
		Operation->MarkDropHandled();
		if (URogue10mInventoryWindowWidget* InventoryWindow = Operation->InventoryWindow.Get())
		{
			InventoryWindow->EndGridItemDrag(Operation);
		}
	}
	return bEquipped;
}

void URogue10mEquipmentWindowWidget::NativeOnDragLeave(
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearEquipmentDropPreview();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void URogue10mEquipmentWindowWidget::RefreshEquipmentDisplay()
{
	URogue10mInventoryComponent* Inventory = GetInventoryComponent();
	if (!Inventory)
	{
		return;
	}

	if (const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn()))
	{
		const FRogue10mCharacterStatSnapshot Stats = Character->GetCharacterStatSnapshot();
		SetCharacterStatText(UI_AttackStatText,
			NSLOCTEXT("Rogue10mEquipment", "AttackPower", "공격력"),
			Stats.AttackPower, Stats.Base.AttackPower, Stats.Equipment.AttackPowerBonus, false);
		SetCharacterStatText(UI_DefenseStatText,
			NSLOCTEXT("Rogue10mEquipment", "Defense", "방어력"),
			Stats.Defense, Stats.Base.Defense, Stats.Equipment.DefenseBonus, false);
		SetCharacterStatText(UI_MaxHealthStatText,
			NSLOCTEXT("Rogue10mEquipment", "MaxHealth", "최대 체력"),
			Stats.MaxHealth, Stats.Base.MaxHealth, Stats.Equipment.MaxHealthBonus, false);
		SetCharacterStatText(UI_CriticalChanceStatText,
			NSLOCTEXT("Rogue10mEquipment", "CriticalChance", "치명타 확률"),
			Stats.CriticalChance, Stats.Base.CriticalChance, Stats.Equipment.CriticalChanceBonus, true);
		SetCharacterStatText(UI_AttackSpeedStatText,
			NSLOCTEXT("Rogue10mEquipment", "AttackSpeed", "공격 속도"),
			Stats.AttackSpeedMultiplier, Stats.Base.AttackSpeedMultiplier,
			Stats.Equipment.AttackSpeedBonus, true);
		SetCharacterStatText(UI_MoveSpeedStatText,
			NSLOCTEXT("Rogue10mEquipment", "MoveSpeed", "이동 속도"),
			Stats.MoveSpeed, Stats.Base.MoveSpeed, Stats.Equipment.MoveSpeedBonus, false);
	}

	const auto FindSlot = [Inventory](ERogue10mInventorySlotType SlotType) -> const FRogue10mInventorySlot*
	{
		for (const FRogue10mInventorySlot& Slot : Inventory->GetLeftEquipmentSlots())
		{
			if (Slot.SlotType == SlotType)
			{
				return &Slot;
			}
		}
		for (const FRogue10mInventorySlot& Slot : Inventory->GetRightEquipmentSlots())
		{
			if (Slot.SlotType == SlotType)
			{
				return &Slot;
			}
		}
		return nullptr;
	};

	const auto SetSlotIcon = [](UImage* Image, const FRogue10mInventorySlot* EquipmentSlot)
	{
		if (!Image)
		{
			return;
		}
		const URogue10mItemDataAsset* ItemData = EquipmentSlot && EquipmentSlot->bHasEquippedItem
			? EquipmentSlot->EquippedItem.ItemData.Get()
			: nullptr;
		UTexture2D* Icon = ItemData ? ItemData->InventoryIcon.LoadSynchronous() : nullptr;
		// 슬롯 레이어가 Texture 원본 픽셀 크기에 의해 확장되지 않도록 Brush 크기를 유지합니다.
		Image->SetBrushFromTexture(Icon, false);
		Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (!Icon)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		// 원본 종횡비는 유지하되 Desired Size가 슬롯 레이아웃을 확장하지 않도록 정규화합니다.
		constexpr double MaxIconDesiredSize = 64.0;
		const FVector2D SourceSize(static_cast<double>(Icon->GetSizeX()), static_cast<double>(Icon->GetSizeY()));
		const double SourceMaxDimension = FMath::Max(SourceSize.X, SourceSize.Y);
		const double NormalizedScale = SourceMaxDimension > 0.0 ? MaxIconDesiredSize / SourceMaxDimension : 1.0;
		Image->SetDesiredSizeOverride(SourceSize * NormalizedScale);

		// ScaleBox가 슬롯 크기에 맞추고 1.0 이하의 사용자 배율로 원본 종횡비를 유지합니다.
		const float IconScale = FMath::Clamp(ItemData->InventoryIconScale, 0.1f, 1.0f);
		Image->SetRenderScale(FVector2D(IconScale, IconScale));
		Image->SetColorAndOpacity(FLinearColor::White);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	};

	SetSlotIcon(UI_WeaponSlotIcon, FindSlot(ERogue10mInventorySlotType::MainWeapon));
	SetSlotIcon(UI_HeadSlotIcon, FindSlot(ERogue10mInventorySlotType::Head));
	SetSlotIcon(UI_ChestSlotIcon, FindSlot(ERogue10mInventorySlotType::Armor));
	SetSlotIcon(UI_HandsSlotIcon, FindSlot(ERogue10mInventorySlotType::Hands));
	SetSlotIcon(UI_RingSlotIcon, FindSlot(ERogue10mInventorySlotType::Ring));
	SetSlotIcon(UI_FeetSlotIcon, FindSlot(ERogue10mInventorySlotType::Shoes));
	SetSlotIcon(UI_NecklaceSlotIcon, FindSlot(ERogue10mInventorySlotType::Necklace));
	RefreshEquipmentSlotTooltip(
		UI_WeaponSlotFrame ? static_cast<UWidget*>(UI_WeaponSlotFrame) : UI_WeaponSlotIcon, ERogue10mInventorySlotType::MainWeapon);
	RefreshEquipmentSlotTooltip(
		UI_HeadSlotFrame ? static_cast<UWidget*>(UI_HeadSlotFrame) : UI_HeadSlotIcon, ERogue10mInventorySlotType::Head);
	RefreshEquipmentSlotTooltip(
		UI_ChestSlotFrame ? static_cast<UWidget*>(UI_ChestSlotFrame) : UI_ChestSlotIcon, ERogue10mInventorySlotType::Armor);
	RefreshEquipmentSlotTooltip(
		UI_HandsSlotFrame ? static_cast<UWidget*>(UI_HandsSlotFrame) : UI_HandsSlotIcon, ERogue10mInventorySlotType::Hands);
	RefreshEquipmentSlotTooltip(
		UI_RingSlotFrame ? static_cast<UWidget*>(UI_RingSlotFrame) : UI_RingSlotIcon, ERogue10mInventorySlotType::Ring);
	RefreshEquipmentSlotTooltip(
		UI_FeetSlotFrame ? static_cast<UWidget*>(UI_FeetSlotFrame) : UI_FeetSlotIcon, ERogue10mInventorySlotType::Shoes);
	RefreshEquipmentSlotTooltip(
		UI_NecklaceSlotFrame ? static_cast<UWidget*>(UI_NecklaceSlotFrame) : UI_NecklaceSlotIcon, ERogue10mInventorySlotType::Necklace);
	RebuildEquipmentDragSourceProxies();
}
bool URogue10mEquipmentWindowWidget::EnsureCharacterPreview()
{
	if (CharacterPreviewActor && IsValid(CharacterPreviewActor))
	{
		return true;
	}

	ACharacter* SourceCharacter = Cast<ACharacter>(GetOwningPlayerPawn());
	UWorld* World = GetWorld();
	if (!SourceCharacter || !World || !UI_CharacterPreviewImage)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = SourceCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	CharacterPreviewActor = World->SpawnActor<ARogue10mEquipmentPreviewActor>(
		ARogue10mEquipmentPreviewActor::StaticClass(),
		SourceCharacter->GetActorTransform(),
		SpawnParameters);
	if (!CharacterPreviewActor
		|| !CharacterPreviewActor->InitializePreview(SourceCharacter, CharacterPreviewResolution))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("장비창 캐릭터 프리뷰를 초기화하지 못했습니다."));
		DestroyCharacterPreview();
		return false;
	}

	UI_CharacterPreviewImage->SetBrushResourceObject(CharacterPreviewActor->GetRenderTarget());
	UI_CharacterPreviewImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
	UI_CharacterPreviewImage->SetColorAndOpacity(FLinearColor::White);
	UI_CharacterPreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void URogue10mEquipmentWindowWidget::RefreshCharacterPreview()
{
	if (!IsWindowOpen())
	{
		return;
	}

	if (EnsureCharacterPreview())
	{
		CharacterPreviewActor->RefreshFromCharacter();
	}
}

void URogue10mEquipmentWindowWidget::DestroyCharacterPreview()
{
	if (CharacterPreviewActor && IsValid(CharacterPreviewActor))
	{
		CharacterPreviewActor->Destroy();
	}
	CharacterPreviewActor = nullptr;

	if (UI_CharacterPreviewImage)
	{
		UI_CharacterPreviewImage->SetBrushResourceObject(nullptr);
		UI_CharacterPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}
namespace
{
FText GetMartialArtsTierText(int32 Tier)
{
	switch (Tier)
	{
	case 1: return FText::FromString(TEXT("입문"));
	case 2: return FText::FromString(TEXT("초식"));
	case 3: return FText::FromString(TEXT("절기"));
	case 4: return FText::FromString(TEXT("심법"));
	default: return FText::FromString(TEXT("무공"));
	}
}
}

void URogue10mSkillTreeEntryWidget::SetSkillData(
	URogue10mAttackSkillData* InSkillData, bool bInUnlocked,
	bool bInAvailable, float InUnlockProgress)
{
	SkillData = InSkillData;
	bUnlocked = bInUnlocked;
	bCanDrag = bUnlocked && SkillData
		&& SkillData->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique;
	SetIsEnabled(true);
	SetRenderOpacity(bUnlocked ? 1.0f : (bInAvailable ? 0.78f : 0.42f));
	SetToolTipText(SkillData ? SkillData->SkillDescription : FText::GetEmpty());

	if (UI_SkillNameText)
	{
		UI_SkillNameText->SetText(SkillData ? SkillData->SkillName : FText::GetEmpty());
	}
	if (UI_SkillDescriptionText)
	{
		UI_SkillDescriptionText->SetText(SkillData ? SkillData->SkillDescription : FText::GetEmpty());
	}
	if (UI_SkillLockText)
	{
		const FText StatusText = bUnlocked
			? (bCanDrag ? FText::FromString(TEXT("해금 · 장착 가능")) : FText::FromString(TEXT("해금 · 상시 발동")))
			: (bInAvailable ? FText::FromString(TEXT("수련 완료 · 해금 대기")) : FText::FromString(TEXT("잠김")));
		UI_SkillLockText->SetText(StatusText);
	}
	if (UI_SkillTierText)
	{
		UI_SkillTierText->SetText(SkillData ? GetMartialArtsTierText(SkillData->SkillTreeTier) : FText::GetEmpty());
	}
	if (UI_SkillProgressText)
	{
		UI_SkillProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("수련 %d%%"), FMath::RoundToInt(FMath::Clamp(InUnlockProgress, 0.0f, 1.0f) * 100.0f))));
	}
	if (UI_SkillProgressBar)
	{
		UI_SkillProgressBar->SetPercent(FMath::Clamp(InUnlockProgress, 0.0f, 1.0f));
		UI_SkillProgressBar->SetFillColorAndOpacity(
			bUnlocked ? FLinearColor(0.30f, 0.92f, 0.56f, 1.0f) : FLinearColor(0.66f, 0.48f, 0.20f, 1.0f));
	}
	if (UI_SkillIconImage)
	{
		UI_SkillIconImage->SetBrushFromTexture(SkillData ? SkillData->SkillIcon : nullptr);
		UI_SkillIconImage->SetColorAndOpacity(SkillData ? SkillData->IconTint : FLinearColor::White);
	}
}

FReply URogue10mSkillTreeEntryWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (SkillData && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSkillSelected.Broadcast(SkillData);
		if (bCanDrag)
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(
				InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URogue10mSkillTreeEntryWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	if (!bCanDrag || !SkillData)
	{
		return;
	}

	URogue10mSkillDragDropOperation* Operation = NewObject<URogue10mSkillDragDropOperation>(this);
	Operation->SkillData = SkillData;
	Operation->bUnlocked = true;
	Operation->Pivot = EDragPivot::MouseDown;
	OutOperation = Operation;
}

void URogue10mSkillTreeWindowWidget::InitializeMenuWindow(
	URogue10mInventoryComponent* InInventoryComponent)
{
	Super::InitializeMenuWindow(InInventoryComponent);
	if (URogue10mCombatComponent* Combat = GetCombatComponent())
	{
		Combat->OnSkillTreeChanged.AddUniqueDynamic(
			this, &URogue10mSkillTreeWindowWidget::HandleSkillTreeChanged);
	}
	RefreshSkillTree();
}

void URogue10mSkillTreeWindowWidget::NativeDestruct()
{
	if (URogue10mCombatComponent* Combat = GetCombatComponent())
	{
		Combat->OnSkillTreeChanged.RemoveDynamic(
			this, &URogue10mSkillTreeWindowWidget::HandleSkillTreeChanged);
	}
	Super::NativeDestruct();
}

void URogue10mSkillTreeWindowWidget::SetWindowOpen(bool bOpen)
{
	Super::SetWindowOpen(bOpen);
	if (bOpen)
	{
		RefreshSkillTree();
	}
}

void URogue10mSkillTreeWindowWidget::RefreshSkillTree()
{
	URogue10mCombatComponent* Combat = GetCombatComponent();
	UCanvasPanel* SkillCanvas = Cast<UCanvasPanel>(UI_SkillListContainer);
	if (!SkillCanvas || !SkillTreeEntryWidgetClass || !Combat)
	{
		return;
	}

	const TArray<URogue10mAttackSkillData*> Skills = Combat->GetActiveSkillTreeSkills();
	SkillCanvas->ClearChildren();
	if (UI_SkillTreeTitleText)
	{
		UI_SkillTreeTitleText->SetText(FText::Format(
			FText::FromString(TEXT("{0} 무공 수련")), Combat->GetActiveSkillTreeDisplayName()));
	}
	if (UI_ActiveWeaponText)
	{
		UI_ActiveWeaponText->SetText(FText::FromString(FString::Printf(
			TEXT("활성 무기 · %s"), *UEnum::GetDisplayValueAsText(Combat->GetActiveSkillTreeWeaponType()).ToString())));
	}

	int32 UnlockedCount = 0;
	int32 HighestTier = 1;
	for (URogue10mAttackSkillData* Skill : Skills)
	{
		if (Skill && Combat->IsAttackSkillUnlocked(Skill))
		{
			++UnlockedCount;
			HighestTier = FMath::Max(HighestTier, Skill->SkillTreeTier);
		}
	}
	if (UI_MartialArtStageText)
	{
		UI_MartialArtStageText->SetText(FText::Format(
			FText::FromString(TEXT("현재 경지 · {0}")), GetMartialArtsTierText(HighestTier)));
	}
	if (UI_MasteryProgressText)
	{
		UI_MasteryProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("해금 무공 %d / %d"), UnlockedCount, Skills.Num())));
	}
	if (UI_MasteryProgressBar)
	{
		UI_MasteryProgressBar->SetPercent(
			Skills.IsEmpty() ? 0.0f : static_cast<float>(UnlockedCount) / static_cast<float>(Skills.Num()));
	}

	for (URogue10mAttackSkillData* Skill : Skills)
	{
		if (!Skill)
		{
			continue;
		}
		for (URogue10mAttackSkillData* Prerequisite : Skill->PrerequisiteSkills)
		{
			if (!Prerequisite || !Skills.Contains(Prerequisite))
			{
				continue;
			}
			const FVector2D Delta = Skill->SkillTreePosition - Prerequisite->SkillTreePosition;
			const float Length = Delta.Size();
			UBorder* Connection = NewObject<UBorder>(SkillCanvas);
			Connection->SetBrushColor(
				Combat->IsAttackSkillUnlocked(Prerequisite) && Combat->IsAttackSkillUnlocked(Skill)
					? FLinearColor(0.18f, 0.74f, 0.44f, 0.82f)
					: FLinearColor(0.28f, 0.22f, 0.14f, 0.72f));
			Connection->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
			if (UCanvasPanelSlot* ConnectionSlot = SkillCanvas->AddChildToCanvas(Connection))
			{
				ConnectionSlot->SetPosition((Skill->SkillTreePosition + Prerequisite->SkillTreePosition) * 0.5f);
				ConnectionSlot->SetSize(FVector2D(Length, 4.0f));
				ConnectionSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				ConnectionSlot->SetZOrder(0);
			}
		}
	}

	for (URogue10mAttackSkillData* Skill : Skills)
	{
		if (!Skill)
		{
			continue;
		}
		URogue10mSkillTreeEntryWidget* Entry = CreateWidget<URogue10mSkillTreeEntryWidget>(
			GetOwningPlayer(), SkillTreeEntryWidgetClass);
		if (!Entry)
		{
			continue;
		}
		Entry->SetSkillData(
			Skill, Combat->IsAttackSkillUnlocked(Skill), Combat->IsSkillAvailableToUnlock(Skill),
			Combat->GetSkillUnlockProgress(Skill));
		Entry->OnSkillSelected.AddUniqueDynamic(
			this, &URogue10mSkillTreeWindowWidget::HandleSkillSelected);
		if (UCanvasPanelSlot* EntrySlot = SkillCanvas->AddChildToCanvas(Entry))
		{
			EntrySlot->SetPosition(Skill->SkillTreePosition);
			EntrySlot->SetSize(FVector2D(156.0f, 118.0f));
			EntrySlot->SetAlignment(FVector2D(0.5f, 0.5f));
			EntrySlot->SetZOrder(2);
		}
	}

	if (!SelectedSkill || !Skills.Contains(SelectedSkill))
	{
		SelectedSkill = Skills.IsEmpty() ? nullptr : Skills[0];
	}
	RefreshSelectedSkillDetails();
}

void URogue10mSkillTreeWindowWidget::HandleSkillSelected(URogue10mAttackSkillData* SkillData)
{
	SelectedSkill = SkillData;
	RefreshSelectedSkillDetails();
}

void URogue10mSkillTreeWindowWidget::HandleSkillTreeChanged()
{
	RefreshSkillTree();
}

void URogue10mSkillTreeWindowWidget::RefreshSelectedSkillDetails()
{
	URogue10mCombatComponent* Combat = GetCombatComponent();
	if (!Combat || !SelectedSkill)
	{
		return;
	}
	if (UI_SelectedSkillIcon)
	{
		UI_SelectedSkillIcon->SetBrushFromTexture(SelectedSkill->SkillIcon);
		UI_SelectedSkillIcon->SetColorAndOpacity(SelectedSkill->IconTint);
	}
	if (UI_SelectedSkillNameText)
	{
		UI_SelectedSkillNameText->SetText(SelectedSkill->SkillName);
	}
	if (UI_SelectedSkillTierText)
	{
		UI_SelectedSkillTierText->SetText(FText::Format(
			FText::FromString(TEXT("{0} · {1}")), GetMartialArtsTierText(SelectedSkill->SkillTreeTier),
			SelectedSkill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique
				? FText::FromString(TEXT("상시 심법")) : FText::FromString(TEXT("공격 무공"))));
	}
	if (UI_SelectedSkillDescriptionText)
	{
		UI_SelectedSkillDescriptionText->SetText(SelectedSkill->SkillDescription);
	}
	if (UI_SelectedSkillStatusText)
	{
		const bool bUnlocked = Combat->IsAttackSkillUnlocked(SelectedSkill);
		UI_SelectedSkillStatusText->SetText(bUnlocked
			? FText::FromString(TEXT("● 해금 완료"))
			: FText::FromString(FString::Printf(TEXT("◆ 수련 진행 %d%%"),
				FMath::RoundToInt(Combat->GetSkillUnlockProgress(SelectedSkill) * 100.0f))));
		UI_SelectedSkillStatusText->SetColorAndOpacity(bUnlocked
			? FSlateColor(FLinearColor(0.32f, 0.95f, 0.58f, 1.0f))
			: FSlateColor(FLinearColor(0.86f, 0.67f, 0.31f, 1.0f)));
	}

	FString Conditions;
	for (const URogue10mAttackSkillData* Prerequisite : SelectedSkill->PrerequisiteSkills)
	{
		if (Prerequisite)
		{
			Conditions += FString::Printf(TEXT("%s 선행 · %s\n"),
				Combat->IsAttackSkillUnlocked(Prerequisite) ? TEXT("✓") : TEXT("○"),
				*Prerequisite->SkillName.ToString());
		}
	}
	for (const FRogue10mSkillUnlockCondition& Condition : SelectedSkill->UnlockConditions)
	{
		const int32 Current = Combat->GetSkillUnlockConditionCurrent(Condition);
		const int32 Required = FMath::Max(1, Condition.RequiredCount);
		FText TargetName = Condition.TargetDisplayName;
		if (TargetName.IsEmpty())
		{
			TargetName = Condition.RequiredSkill
				? Condition.RequiredSkill->SkillName : FText::FromName(Condition.RequiredMonsterId);
		}
		const TCHAR* TypeText = Condition.ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount
			? TEXT("수련") : TEXT("토벌");
		Conditions += FString::Printf(TEXT("%s %s · %s  %d / %d\n"),
			Current >= Required ? TEXT("✓") : TEXT("○"), TypeText,
			*TargetName.ToString(), FMath::Min(Current, Required), Required);
	}
	if (Conditions.IsEmpty())
	{
		Conditions = TEXT("문파 입문 시 기본 전수");
	}
	if (UI_SelectedSkillConditionsText)
	{
		UI_SelectedSkillConditionsText->SetText(FText::FromString(Conditions));
	}
	if (UI_SelectedSkillRewardText)
	{
		FText Reward = SelectedSkill->UnlockRewardText;
		if (Reward.IsEmpty())
		{
			Reward = SelectedSkill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique
				? FText::FromString(TEXT("해금 즉시 상시 적용"))
				: FText::FromString(TEXT("해금 후 하단 HUD 스킬 슬롯에 장착 가능"));
		}
		UI_SelectedSkillRewardText->SetText(Reward);
	}
}

URogue10mCombatComponent* URogue10mSkillTreeWindowWidget::GetCombatComponent() const
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	return Character ? Character->GetCombatComponent() : nullptr;
}
