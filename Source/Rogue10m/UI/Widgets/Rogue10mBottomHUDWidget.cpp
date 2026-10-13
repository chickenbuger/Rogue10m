// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mBottomHUDWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Widgets/Rogue10mMainHUDWidget.h"

namespace
{
	FRogue10mHudVitalView MakeIdentityVitalView(
		const FRogue10mHudIdentityView& IdentityView,
		const FLinearColor& FillColor)
	{
		FRogue10mHudVitalView VitalView;
		VitalView.Current = IdentityView.Current;
		VitalView.Max = IdentityView.Max;
		VitalView.Normalized = IdentityView.Normalized;
		VitalView.Percent = IdentityView.Normalized * 100.0f;
		VitalView.ValueText = FText::FromString(FString::Printf(
			TEXT("%.0f / %.0f"), IdentityView.Current, IdentityView.Max));
		VitalView.PercentText = FText::FromString(FString::Printf(TEXT("%.0f%%"), VitalView.Percent));
		VitalView.FillColor = FillColor;
		VitalView.bVisible = IdentityView.bHasIdentityResource;
		return VitalView;
	}

	FLinearColor GetIdentityFillColor(ERogue10mBottomHUDTheme Theme)
	{
		switch (Theme)
		{
		case ERogue10mBottomHUDTheme::Wizard:
			return FLinearColor(0.36f, 0.28f, 0.92f, 1.0f);
		case ERogue10mBottomHUDTheme::Warrior:
			return FLinearColor(0.82f, 0.16f, 0.06f, 1.0f);
		case ERogue10mBottomHUDTheme::MartialArtist:
			return FLinearColor(0.16f, 0.70f, 0.38f, 1.0f);
		case ERogue10mBottomHUDTheme::Rogue:
			return FLinearColor(0.20f, 0.66f, 0.30f, 1.0f);
		default:
			return FLinearColor(0.52f, 0.54f, 0.58f, 1.0f);
		}
	}

	FText GetThemeDisplayName(ERogue10mBottomHUDTheme Theme)
	{
		switch (Theme)
		{
		case ERogue10mBottomHUDTheme::Wizard:
			return NSLOCTEXT("Rogue10mHUD", "BottomThemeWizard", "마법사");
		case ERogue10mBottomHUDTheme::Warrior:
			return NSLOCTEXT("Rogue10mHUD", "BottomThemeWarrior", "전사");
		case ERogue10mBottomHUDTheme::MartialArtist:
			return NSLOCTEXT("Rogue10mHUD", "BottomThemeMartialArtist", "권사");
		case ERogue10mBottomHUDTheme::Rogue:
			return NSLOCTEXT("Rogue10mHUD", "BottomThemeRogue", "도적");
		default:
			return FText::GetEmpty();
		}
	}
}

void URogue10mBottomHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyResponsiveLayout();
	RefreshResolvedTheme();
}

float URogue10mBottomHUDWidget::GetResponsiveLayoutHeight() const
{
	const double DesignHeight = bUseSplitCombatLayout
		? FMath::Max(FMath::Max(1.0, LeftCombatDesignSize.Y), FMath::Max(1.0, RightCombatDesignSize.Y))
		: FMath::Max(1.0, CombatDesignSize.Y);
	return static_cast<float>(DesignHeight) + FMath::Max(4.0f, CombatBottomMargin);
}

FBox2D URogue10mBottomHUDWidget::GetResponsiveCombatBounds(const FVector2D& ViewportSize) const
{
	if (bUseSplitCombatLayout)
	{
		const FBox2D LeftBounds = GetResponsiveLeftCombatBounds(ViewportSize);
		const FBox2D RightBounds = GetResponsiveRightCombatBounds(ViewportSize);
		return FBox2D(FVector2D(LeftBounds.Min.X, FMath::Min(LeftBounds.Min.Y, RightBounds.Min.Y)),
			FVector2D(RightBounds.Max.X, FMath::Max(LeftBounds.Max.Y, RightBounds.Max.Y)));
	}
	const FVector2D DesignSize(FMath::Max(1.0, CombatDesignSize.X), FMath::Max(1.0, CombatDesignSize.Y));
	const double AvailableWidth = FMath::Max(0.0, ViewportSize.X - 2.0 * FMath::Max(0.0f, CombatSideMargin));
	const double Scale = FMath::Min(1.0, AvailableWidth / DesignSize.X);
	const FVector2D CoreSize = DesignSize * Scale;
	const FVector2D CoreMin((ViewportSize.X - CoreSize.X) * 0.5,
		ViewportSize.Y - FMath::Max(4.0f, CombatBottomMargin) - CoreSize.Y);
	return FBox2D(CoreMin, CoreMin + CoreSize);
}

double URogue10mBottomHUDWidget::GetSplitLayoutScale(const FVector2D& ViewportSize) const
{
	if (ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0)
	{
		return 1.0;
	}
	const double AvailableWidth = FMath::Max(0.0, ViewportSize.X
		- 2.0 * FMath::Max(0.0f, CombatSideMargin) - FMath::Max(0.0f, SplitMinimumCenterGap));
	const double AvailableHeight = FMath::Max(0.0, ViewportSize.Y - 2.0 * FMath::Max(4.0f, CombatBottomMargin));
	const double DesignWidth = FMath::Max(1.0, LeftCombatDesignSize.X) + FMath::Max(1.0, RightCombatDesignSize.X);
	const double DesignHeight = FMath::Max(FMath::Max(1.0, LeftCombatDesignSize.Y), FMath::Max(1.0, RightCombatDesignSize.Y));
	return FMath::Min(1.0, FMath::Min(AvailableWidth / DesignWidth, AvailableHeight / DesignHeight));
}

FBox2D URogue10mBottomHUDWidget::GetResponsiveLeftCombatBounds(const FVector2D& ViewportSize) const
{
	if (!bUseSplitCombatLayout)
	{
		return GetResponsiveCombatBounds(ViewportSize);
	}
	const FVector2D PanelSize = FVector2D(FMath::Max(1.0, LeftCombatDesignSize.X),
		FMath::Max(1.0, LeftCombatDesignSize.Y)) * GetSplitLayoutScale(ViewportSize);
	const FVector2D PanelMin(FMath::Max(0.0f, CombatSideMargin),
		ViewportSize.Y - FMath::Max(4.0f, CombatBottomMargin) - PanelSize.Y);
	return FBox2D(PanelMin, PanelMin + PanelSize);
}

FBox2D URogue10mBottomHUDWidget::GetResponsiveRightCombatBounds(const FVector2D& ViewportSize) const
{
	if (!bUseSplitCombatLayout)
	{
		return GetResponsiveCombatBounds(ViewportSize);
	}
	const FVector2D PanelSize = FVector2D(FMath::Max(1.0, RightCombatDesignSize.X),
		FMath::Max(1.0, RightCombatDesignSize.Y)) * GetSplitLayoutScale(ViewportSize);
	const FVector2D PanelMax(ViewportSize.X - FMath::Max(0.0f, CombatSideMargin),
		ViewportSize.Y - FMath::Max(4.0f, CombatBottomMargin));
	return FBox2D(PanelMax - PanelSize, PanelMax);
}

void URogue10mBottomHUDWidget::UpdateResponsiveLayout(const FVector2D& ViewportSize)
{
	CachedResponsiveViewportSize = ViewportSize;
	ApplyResponsiveLayout();
}

void URogue10mBottomHUDWidget::ApplyResponsiveLayout()
{
	if (UI_CombatScaleBox)
	{
		UI_CombatScaleBox->SetVisibility(bUseSplitCombatLayout
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	const double SplitScale = GetSplitLayoutScale(CachedResponsiveViewportSize);
	const auto ConfigureSplitPanel = [this, SplitScale](UScaleBox* PanelScaleBox, USizeBox* PanelDesignSize,
		const FVector2D& InDesignSize, bool bRightPanel)
	{
		if (!PanelScaleBox || !PanelDesignSize)
		{
			return;
		}
		PanelScaleBox->SetVisibility(bUseSplitCombatLayout
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (!bUseSplitCombatLayout)
		{
			return;
		}
		const FVector2D PanelSize(FMath::Max(1.0, InDesignSize.X), FMath::Max(1.0, InDesignSize.Y));
		PanelDesignSize->SetWidthOverride(PanelSize.X);
		PanelDesignSize->SetHeightOverride(PanelSize.Y);
		if (UScaleBoxSlot* PanelContentSlot = Cast<UScaleBoxSlot>(PanelDesignSize->Slot))
		{
			PanelContentSlot->SetHorizontalAlignment(bRightPanel ? HAlign_Right : HAlign_Left);
			PanelContentSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		PanelScaleBox->SetStretch(EStretch::ScaleToFit);
		PanelScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
		PanelScaleBox->SetIgnoreInheritedScale(false);
		if (UCanvasPanelSlot* PanelCanvasSlot = Cast<UCanvasPanelSlot>(PanelScaleBox->Slot))
		{
			PanelCanvasSlot->SetAutoSize(false);
			PanelCanvasSlot->SetAnchors(FAnchors(bRightPanel ? 1.0f : 0.0f, 1.0f));
			PanelCanvasSlot->SetAlignment(FVector2D(bRightPanel ? 1.0f : 0.0f, 1.0f));
			const float HorizontalOffset = FMath::Max(0.0f, CombatSideMargin) * (bRightPanel ? -1.0f : 1.0f);
			PanelCanvasSlot->SetOffsets(FMargin(HorizontalOffset, -FMath::Max(4.0f, CombatBottomMargin),
				PanelSize.X * SplitScale, PanelSize.Y * SplitScale));
		}
	};
	ConfigureSplitPanel(UI_LeftCombatScaleBox, UI_LeftCombatDesignSize, LeftCombatDesignSize, false);
	ConfigureSplitPanel(UI_RightCombatScaleBox, UI_RightCombatDesignSize, RightCombatDesignSize, true);
	const FVector2D DesignSize(FMath::Max(1.0, CombatDesignSize.X), FMath::Max(1.0, CombatDesignSize.Y));
	if (UI_CombatDesignSize)
	{
		UI_CombatDesignSize->SetWidthOverride(DesignSize.X);
		UI_CombatDesignSize->SetHeightOverride(DesignSize.Y);
		if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(UI_CombatDesignSize->Slot))
		{
			ScaleSlot->SetHorizontalAlignment(HAlign_Center);
			ScaleSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}
	if (UI_CombatScaleBox)
	{
		// Slate responds to allotted geometry changes, including live window resizing.
		// Keep inherited DPI; a second viewport scale would shrink the HUD twice.
		UI_CombatScaleBox->SetStretch(EStretch::ScaleToFit);
		UI_CombatScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
		UI_CombatScaleBox->SetIgnoreInheritedScale(false);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(UI_CombatScaleBox->Slot))
		{
			const float SideMargin = FMath::Max(0.0f, CombatSideMargin);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(SideMargin, -FMath::Max(4.0f, CombatBottomMargin), SideMargin, DesignSize.Y));
		}
	}
	ApplyQuickSlotLayout(SkillSlotContainer);
	ApplyQuickSlotLayout(ItemSlotContainer);
}

void URogue10mBottomHUDWidget::ApplyQuickSlotLayout(UPanelWidget* Container)
{
	if (!Container)
	{
		return;
	}
	for (int32 Index = 0; Index < Container->GetChildrenCount(); ++Index)
	{
		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Container->GetChildAt(Index)->Slot))
		{
			HorizontalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HorizontalSlot->SetHorizontalAlignment(HAlign_Fill);
			HorizontalSlot->SetVerticalAlignment(VAlign_Center);
			HorizontalSlot->SetPadding(FMargin(0.0f, 0.0f,
				Index + 1 < Container->GetChildrenCount() ? FMath::Max(0.0f, QuickSlotSpacing) : 0.0f, 0.0f));
		}
	}
}

void URogue10mBottomHUDWidget::InitializeBottomHUD(URogue10mMainHUDWidget* InOwningMainHUD)
{
	SetOwningMainHUD(InOwningMainHUD);
	AssignOwningMainHUDToChildren(InOwningMainHUD);
}

void URogue10mBottomHUDWidget::SetFrequentViews(
	const FRogue10mHudVitalView& HealthView,
	const FRogue10mHudVitalView& StaminaView,
	const FRogue10mHudVitalView& ManaView,
	const FRogue10mHudProgressionView& ProgressionView,
	const FRogue10mHudIdentityView& IdentityView)
{
	CachedIdentityView = IdentityView;
	RefreshResolvedTheme();

	static const FText HealthLabel = NSLOCTEXT("Rogue10mHUD", "HealthLabel", "체력");
	static const FText StaminaLabel = NSLOCTEXT("Rogue10mHUD", "StaminaLabel", "스태미나");
	static const FText ManaLabel = NSLOCTEXT("Rogue10mHUD", "ManaLabel", "마력");
	static const FText IdentityLabel = NSLOCTEXT("Rogue10mHUD", "IdentityLabel", "아이덴티티");

	if (HealthBarWidget)
	{
		FRogue10mHudVitalView PresentedHealthView = HealthView;
		if (bUseSplitCombatLayout)
		{
			PresentedHealthView.FillColor = SplitHealthFillColor;
		}
		HealthBarWidget->SetVitalView(HealthLabel, PresentedHealthView);
	}

	const bool bUseMana = ResolvedTheme == ERogue10mBottomHUDTheme::Wizard && ManaView.bVisible;
	if (StaminaBarWidget)
	{
		StaminaBarWidget->SetVisibility(
			bUseMana ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		StaminaBarWidget->SetVitalView(StaminaLabel, StaminaView);
	}
	if (ManaBarWidget)
	{
		ManaBarWidget->SetVisibility(
			bUseMana ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ManaBarWidget->SetVitalView(ManaLabel, ManaView);
	}
	if (UI_ResourceTypeText)
	{
		UI_ResourceTypeText->SetText(bUseMana ? ManaLabel : StaminaLabel);
	}

	if (ProgressionWidget)
	{
		ProgressionWidget->SetProgressionView(ProgressionView);
	}
	if (UI_LevelText)
	{
		UI_LevelText->SetText(FText::Format(NSLOCTEXT("Rogue10mHUD", "BottomLevel", "LV {0}"),
			FText::AsNumber(FMath::Max(1, ProgressionView.Level))));
	}
	if (IdentityWidget)
	{
		IdentityWidget->SetIdentityView(IdentityView);
	}
	if (IdentityBarWidget)
	{
		const ESlateVisibility IdentityVisibility = IdentityView.bHasIdentityResource
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		IdentityBarWidget->SetVisibility(IdentityVisibility);
		IdentityBarWidget->SetVitalView(
			IdentityLabel,
			MakeIdentityVitalView(IdentityView, GetIdentityFillColor(ResolvedTheme)));
	}
}

void URogue10mBottomHUDWidget::SetSlowViews(
	const TArray<FRogue10mHudQuickSlotView>& SkillViews,
	const TArray<FRogue10mHudQuickSlotView>& ItemViews,
	TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass)
{
	const TSubclassOf<URogue10mQuickSlotWidget> SlotClass = BottomQuickSlotWidgetClass
		? BottomQuickSlotWidgetClass : InQuickSlotWidgetClass;
	RefreshQuickSlotContainer(SkillSlotContainer, SkillViews, SlotClass);
	RefreshQuickSlotContainer(ItemSlotContainer, ItemViews, SlotClass);
}

void URogue10mBottomHUDWidget::SetThemeOverride(ERogue10mBottomHUDTheme InThemeOverride)
{
	if (ThemeOverride == InThemeOverride)
	{
		return;
	}

	ThemeOverride = InThemeOverride;
	RefreshResolvedTheme();
}

FText URogue10mBottomHUDWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("통합 하단 전투 HUD"));
}

FText URogue10mBottomHUDWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("HP, MP/스태미나, 아이덴티티, 스킬, 아이템, 경험치를 한 Widget에서 조립합니다."));
}

FVector2D URogue10mBottomHUDWidget::GetPrototypeDesignSize() const
{
	return FVector2D(CombatDesignSize.X, GetResponsiveLayoutHeight());
}

ERogue10mBottomHUDTheme URogue10mBottomHUDWidget::ResolveTheme(
	const FRogue10mHudIdentityView& IdentityView) const
{
	if (ThemeOverride != ERogue10mBottomHUDTheme::Auto)
	{
		return ThemeOverride;
	}

	if (IdentityView.IdentityType == ERogue10mIdentityType::Mana
		|| IdentityView.WeaponType == ERogue10mWeaponType::Staff)
	{
		return ERogue10mBottomHUDTheme::Wizard;
	}
	if (IdentityView.IdentityType == ERogue10mIdentityType::Rage
		|| IdentityView.WeaponType == ERogue10mWeaponType::GreatSword)
	{
		return ERogue10mBottomHUDTheme::Warrior;
	}
	if (IdentityView.IdentityType == ERogue10mIdentityType::StoneFist
		|| IdentityView.IdentityType == ERogue10mIdentityType::Vigor
		|| IdentityView.WeaponType == ERogue10mWeaponType::Knuckle
		|| IdentityView.WeaponType == ERogue10mWeaponType::Unarmed)
	{
		return ERogue10mBottomHUDTheme::MartialArtist;
	}

	return ERogue10mBottomHUDTheme::Rogue;
}

void URogue10mBottomHUDWidget::RefreshResolvedTheme()
{
	ResolvedTheme = ResolveTheme(CachedIdentityView);
	ApplyThemePresentation();
}

void URogue10mBottomHUDWidget::ApplyThemePresentation()
{
	const TArray<TPair<UImage*, ERogue10mBottomHUDTheme>> ThemeFrames =
	{
		{UI_WizardThemeFrame, ERogue10mBottomHUDTheme::Wizard},
		{UI_WarriorThemeFrame, ERogue10mBottomHUDTheme::Warrior},
		{UI_MartialArtistThemeFrame, ERogue10mBottomHUDTheme::MartialArtist},
		{UI_RogueThemeFrame, ERogue10mBottomHUDTheme::Rogue}
	};

	for (const TPair<UImage*, ERogue10mBottomHUDTheme>& ThemeFrame : ThemeFrames)
	{
		if (ThemeFrame.Key)
		{
			ThemeFrame.Key->SetVisibility(ThemeFrame.Value == ResolvedTheme
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	if (UI_ThemeNameText)
	{
		UI_ThemeNameText->SetText(GetThemeDisplayName(ResolvedTheme));
	}
}

void URogue10mBottomHUDWidget::AssignOwningMainHUDToChildren(
	URogue10mMainHUDWidget* InOwningMainHUD)
{
	const TArray<URogue10mHudPartWidget*> Parts =
	{
		HealthBarWidget,
		StaminaBarWidget,
		ManaBarWidget,
		IdentityBarWidget,
		ProgressionWidget,
		IdentityWidget
	};
	for (URogue10mHudPartWidget* Part : Parts)
	{
		if (Part)
		{
			Part->SetOwningMainHUD(InOwningMainHUD);
		}
	}
}

void URogue10mBottomHUDWidget::RefreshQuickSlotContainer(
	UPanelWidget* Container,
	const TArray<FRogue10mHudQuickSlotView>& Views,
	TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass)
{
	if (!Container || !InQuickSlotWidgetClass)
	{
		return;
	}

	bool bLayoutChanged = false;
	while (Container->GetChildrenCount() > Views.Num())
	{
		Container->RemoveChildAt(Container->GetChildrenCount() - 1);
		bLayoutChanged = true;
	}
	while (Container->GetChildrenCount() < Views.Num())
	{
		URogue10mQuickSlotWidget* SlotWidget = CreateWidget<URogue10mQuickSlotWidget>(
			GetOwningPlayer(), InQuickSlotWidgetClass);
		if (!SlotWidget)
		{
			break;
		}
		SlotWidget->SetOwningMainHUD(GetOwningMainHUD());
		Container->AddChild(SlotWidget);
		bLayoutChanged = true;
	}

	if (bLayoutChanged)
	{
		ApplyQuickSlotLayout(Container);
	}

	for (int32 Index = 0; Index < Views.Num(); ++Index)
	{
		if (URogue10mQuickSlotWidget* SlotWidget = Cast<URogue10mQuickSlotWidget>(
			Container->GetChildAt(Index)))
		{
			SlotWidget->SetOwningMainHUD(GetOwningMainHUD());
			FRogue10mHudQuickSlotView PresentedView = Views[Index];
			if (Container == SkillSlotContainer && !PresentedView.SkillIcon
				&& DefaultSkillIcons.IsValidIndex(Index))
			{
				PresentedView.SkillIcon = DefaultSkillIcons[Index];
			}
			SlotWidget->SetQuickSlotView(PresentedView);
		}
	}
}
