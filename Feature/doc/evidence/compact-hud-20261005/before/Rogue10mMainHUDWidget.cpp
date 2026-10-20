// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mMainHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Rogue10mBottomHUDWidget.h"

namespace
{
	const FLinearColor PrototypePanelColor(0.02f, 0.025f, 0.03f, 0.68f);
	const FLinearColor PrototypeTextColor(0.92f, 0.90f, 0.84f, 1.0f);

	UTextBlock* CreatePrototypeText(UWidgetTree* WidgetTree, FName WidgetName, const FString& Text, float FontSize = 12.0f)
	{
		UTextBlock* TextBlock = WidgetTree ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName) : nullptr;
		if (!TextBlock)
		{
			return nullptr;
		}

		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(FSlateColor(PrototypeTextColor));

		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = static_cast<int32>(FontSize);
		TextBlock->SetFont(FontInfo);

		return TextBlock;
	}

	UBorder* CreatePrototypePanel(UWidgetTree* WidgetTree, FName WidgetName)
	{
		UBorder* Border = WidgetTree ? WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), WidgetName) : nullptr;
		if (!Border)
		{
			return nullptr;
		}

		Border->SetBrushColor(PrototypePanelColor);
		Border->SetPadding(FMargin(8.0f));
		return Border;
	}

	void AddCanvasChild(UCanvasPanel* RootCanvas, UWidget* Widget, const FVector2D& AnchorsMin, const FVector2D& AnchorsMax, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		if (!RootCanvas || !Widget)
		{
			return;
		}

		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Widget);
		if (!CanvasSlot)
		{
			return;
		}

		CanvasSlot->SetAnchors(FAnchors(AnchorsMin.X, AnchorsMin.Y, AnchorsMax.X, AnchorsMax.Y));
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
	}
}

void URogue10mMainHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AssignOwningMainHUDToBoundWidgets();
	RefreshBoundWidgetData();
}

void URogue10mMainHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (BottomHUDWidget)
	{
		if (UCanvasPanelSlot* BottomSlot = Cast<UCanvasPanelSlot>(BottomHUDWidget->Slot))
		{
			BottomSlot->SetAutoSize(false);
			BottomSlot->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
			BottomSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			BottomSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, BottomHUDWidget->GetResponsiveLayoutHeight()));
		}
	}

	const auto AnchorPeripheralWidget = [](UWidget* Widget, const FVector2D& Anchor,
		const FVector2D& Alignment, const FMargin& Offsets)
	{
		if (Widget)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
				CanvasSlot->SetAlignment(Alignment);
				CanvasSlot->SetOffsets(Offsets);
			}
		}
	};
	AnchorPeripheralWidget(MonsterInfoWidget, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f),
		FMargin(0.0f, 24.0f, 560.0f, 68.0f));
	AnchorPeripheralWidget(UI_RunTimerText, FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f),
		FMargin(-24.0f, 24.0f, 140.0f, 32.0f));
	const float LogBottomOffset = (BottomHUDWidget ? BottomHUDWidget->GetResponsiveLayoutHeight() : 184.0f) + 24.0f;
	AnchorPeripheralWidget(SystemLogContainer, FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
		FMargin(24.0f, -LogBottomOffset, 360.0f, 48.0f));
	AnchorPeripheralWidget(ItemAcquisitionContainer, FVector2D(1.0f, 0.65f), FVector2D(1.0f, 0.0f),
		FMargin(-24.0f, 0.0f, 260.0f, 72.0f));
}

void URogue10mMainHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsurePrototypeLayout();

	const TArray<URogue10mShortcutHintWidget*> HiddenShortcuts =
	{
		EquipmentShortcutWidget, ItemWindowShortcutWidget, SkillTreeShortcutWidget, SettingsShortcutWidget
	};
	for (URogue10mShortcutHintWidget* ShortcutWidget : HiddenShortcuts)
	{
		if (ShortcutWidget)
		{
			ShortcutWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URogue10mMainHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FrequentRefreshElapsed += InDeltaTime;
	SlowRefreshElapsed += InDeltaTime;
	if (FrequentRefreshElapsed >= FrequentRefreshInterval)
	{
		FrequentRefreshElapsed = FMath::Fmod(FrequentRefreshElapsed, FrequentRefreshInterval);
		RefreshFrequentWidgetData();
	}
	if (SlowRefreshElapsed >= SlowRefreshInterval)
	{
		SlowRefreshElapsed = FMath::Fmod(SlowRefreshElapsed, SlowRefreshInterval);
		RefreshSlowWidgetData();
	}
}

void URogue10mMainHUDWidget::RefreshBoundWidgetData()
{
	RefreshFrequentWidgetData();
	RefreshSlowWidgetData();
}

void URogue10mMainHUDWidget::RefreshFrequentWidgetData()
{
	if (BottomHUDWidget)
	{
		BottomHUDWidget->SetFrequentViews(
			GetHealthView(),
			GetStaminaView(),
			GetManaView(),
			GetProgressionView(),
			GetIdentityView());
	}
	if (MonsterInfoWidget)
	{
		MonsterInfoWidget->SetMonsterInfoView(GetLookedAtMonsterInfoView());
	}
	if (UI_RunTimerText)
	{
		const FRogue10mHudRunTimerView TimerView = GetRunTimerView();
		UI_RunTimerText->SetText(TimerView.RemainingText);
		UI_RunTimerText->SetVisibility(TimerView.bVisible
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// MiniMap is intentionally disabled until its revised UI is ready.
	BP_OnBoundWidgetDataRefreshed();
}

void URogue10mMainHUDWidget::RefreshSlowWidgetData()
{
	if (BottomHUDWidget)
	{
		BottomHUDWidget->SetSlowViews(
			GetSkillQuickSlotViews(),
			GetItemQuickSlotViews(),
			QuickSlotWidgetClass);
	}
	RefreshLogContainer(SystemLogContainer, GetSystemLogEntries(), 2);
	RefreshLogContainer(ItemAcquisitionContainer, GetItemAcquisitionEntries(), 3);
}
void URogue10mMainHUDWidget::AssignOwningMainHUDToBoundWidgets()
{
	if (BottomHUDWidget)
	{
		BottomHUDWidget->InitializeBottomHUD(this);
	}

	if (MonsterInfoWidget)
	{
		MonsterInfoWidget->SetOwningMainHUD(this);
	}

	if (EquipmentShortcutWidget)
	{
		EquipmentShortcutWidget->SetOwningMainHUD(this);
	}

	if (ItemWindowShortcutWidget)
	{
		ItemWindowShortcutWidget->SetOwningMainHUD(this);
	}

	if (SkillTreeShortcutWidget)
	{
		SkillTreeShortcutWidget->SetOwningMainHUD(this);
	}

	if (SettingsShortcutWidget)
	{
		SettingsShortcutWidget->SetOwningMainHUD(this);
	}
}

void URogue10mMainHUDWidget::RefreshLogContainer(UPanelWidget* Container,
	const TArray<FRogue10mHudLogEntryView>& Views, int32 MaxEntries)
{
	if (!Container || !LogLineWidgetClass)
	{
		return;
	}

	// The controller stores newest entries first; retain that order within the HUD budget.
	const int32 VisibleCount = FMath::Min(Views.Num(), FMath::Max(0, MaxEntries));
	Container->SetVisibility(VisibleCount > 0
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	while (Container->GetChildrenCount() > VisibleCount)
	{
		Container->RemoveChildAt(Container->GetChildrenCount() - 1);
	}
	while (Container->GetChildrenCount() < VisibleCount)
	{
		URogue10mLogLineWidget* LogWidget = CreateWidget<URogue10mLogLineWidget>(GetOwningPlayer(), LogLineWidgetClass);
		if (!LogWidget)
		{
			break;
		}
		LogWidget->SetOwningMainHUD(this);
		Container->AddChild(LogWidget);
	}
	for (int32 Index = 0; Index < FMath::Min(VisibleCount, Container->GetChildrenCount()); ++Index)
	{
		if (URogue10mLogLineWidget* LogWidget = Cast<URogue10mLogLineWidget>(Container->GetChildAt(Index)))
		{
			LogWidget->SetLogEntryView(Views[Index]);
		}
	}
}
void URogue10mMainHUDWidget::EnsurePrototypeLayout()
{
	if (!bCreatePrototypeLayoutWhenEmpty || !WidgetTree)
	{
		return;
	}

	// Blueprint에서 이미 UI를 직접 배치했다면 C++ 임시 골격은 만들지 않는다.
	if (BottomHUDWidget || MonsterInfoWidget || GetWidgetFromName(TEXT("BottomHUDPanel")))
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
		WidgetTree->RootWidget = RootCanvas;
	}

	if (!RootCanvas || RootCanvas->GetChildrenCount() > 0)
	{
		return;
	}

	UBorder* MonsterPanel = CreatePrototypePanel(WidgetTree, TEXT("MonsterInfoWidget_PrototypePanel"));
	if (MonsterPanel)
	{
		MonsterPanel->SetContent(CreatePrototypeText(WidgetTree, TEXT("Text_MonsterInfoPlaceholder"), TEXT("몬스터 정보: 이름 / 레벨 / 속성 / 체력 %"), 12.0f));
		AddCanvasChild(RootCanvas, MonsterPanel, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 24.0f), FVector2D(420.0f, 52.0f), FVector2D(0.5f, 0.0f));
	}


	UBorder* ItemAcquisitionPanel = CreatePrototypePanel(WidgetTree, TEXT("Panel_ItemAcquisition"));
	if (ItemAcquisitionPanel)
	{
		ItemAcquisitionPanel->SetContent(CreatePrototypeText(WidgetTree, TEXT("Text_ItemAcquisitionPlaceholder"), TEXT("아이템 획득 알림\n최근 획득 항목부터 위에 표시"), 12.0f));
		AddCanvasChild(RootCanvas, ItemAcquisitionPanel, FVector2D(1.0f, 0.35f), FVector2D(1.0f, 0.35f), FVector2D(-24.0f, 0.0f), FVector2D(260.0f, 96.0f), FVector2D(1.0f, 0.5f));
	}

	UBorder* SystemLogPanel = CreatePrototypePanel(WidgetTree, TEXT("Panel_SystemLog"));
	if (SystemLogPanel)
	{
		SystemLogPanel->SetContent(CreatePrototypeText(WidgetTree, TEXT("Text_SystemLogPlaceholder"), TEXT("시스템 로그\n아이템, 경험치, 전투 메시지 표시"), 12.0f));
		AddCanvasChild(RootCanvas, SystemLogPanel, FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(24.0f, -24.0f), FVector2D(360.0f, 190.0f), FVector2D(0.0f, 1.0f));
	}

	UBorder* BottomPanel = CreatePrototypePanel(WidgetTree, TEXT("BottomHUDPanel"));
	UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box_BottomHUD"));
	if (BottomPanel && BottomBox)
	{
		BottomPanel->SetContent(BottomBox);

		UTextBlock* BottomHUDText = CreatePrototypeText(
			WidgetTree,
			TEXT("BottomHUDWidget_PrototypeText"),
			TEXT("BottomHUDWidget - HP / MP·스테미나 / 아이덴티티 / 스킬 / 아이템 / 경험치"),
			12.0f);

		TArray<UWidget*> BottomChildren = { BottomHUDText };
		for (UWidget* ChildWidget : BottomChildren)
		{
			if (ChildWidget)
			{
				if (UVerticalBoxSlot* ChildSlot = BottomBox->AddChildToVerticalBox(ChildWidget))
				{
					ChildSlot->SetPadding(FMargin(0.0f, 2.0f));
				}
			}
		}

		AddCanvasChild(RootCanvas, BottomPanel, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -24.0f), FVector2D(700.0f, 148.0f), FVector2D(0.5f, 1.0f));
	}

}
