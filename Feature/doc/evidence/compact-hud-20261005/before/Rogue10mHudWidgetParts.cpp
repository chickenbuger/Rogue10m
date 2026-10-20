// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mHudWidgetParts.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Rendering/DrawElementTypes.h"
#include "Widgets/SWidget.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Widgets/Rogue10mSkillDragDropOperation.h"

namespace
{
	const FLinearColor PartPanelColor(0.015f, 0.018f, 0.022f, 0.7f);
	const FLinearColor PartTitleColor(0.86f, 0.68f, 0.34f, 1.0f);
	const FLinearColor PartTextColor(0.88f, 0.86f, 0.8f, 1.0f);

	UTextBlock* CreatePartText(UWidgetTree* WidgetTree, FName WidgetName, const FText& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = WidgetTree ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName) : nullptr;
		if (!TextBlock)
		{
			return nullptr;
		}

		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);

		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		return TextBlock;
	}
}

void URogue10mHudPartWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	EnsurePrototypeDesignScaffold();
}

void URogue10mHudPartWidget::SetOwningMainHUD(URogue10mMainHUDWidget* InOwningMainHUD)
{
	if (OwningMainHUD == InOwningMainHUD)
	{
		return;
	}

	OwningMainHUD = InOwningMainHUD;
	BP_OnOwningMainHUDChanged();
}

FText URogue10mHudPartWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("HUD 파트"));
}

FText URogue10mHudPartWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("내부 디자인을 이 Widget Blueprint에서 제작합니다."));
}

FVector2D URogue10mHudPartWidget::GetPrototypeDesignSize() const
{
	return FVector2D(240.0f, 80.0f);
}

void URogue10mHudPartWidget::EnsurePrototypeDesignScaffold()
{
	if (!bCreatePrototypeDesignScaffold || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (RootCanvas && RootCanvas->GetChildrenCount() > 0)
	{
		return;
	}

	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
		WidgetTree->RootWidget = RootCanvas;
	}

	if (!RootCanvas || RootCanvas->GetChildrenCount() > 0)
	{
		return;
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_DesignScaffold"));
	UVerticalBox* TextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box_DesignScaffoldText"));
	if (!Background || !TextBox)
	{
		return;
	}

	Background->SetBrushColor(PartPanelColor);
	Background->SetPadding(FMargin(8.0f));
	Background->SetContent(TextBox);

	UTextBlock* TitleText = CreatePartText(WidgetTree, TEXT("Text_DesignTitle"), GetPrototypeDesignTitle(), 13, PartTitleColor);
	UTextBlock* DescriptionText = CreatePartText(WidgetTree, TEXT("Text_DesignDescription"), GetPrototypeDesignDescription(), 11, PartTextColor);
	if (TitleText)
	{
		if (UVerticalBoxSlot* TitleSlot = TextBox->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}
	}

	if (DescriptionText)
	{
		TextBox->AddChildToVerticalBox(DescriptionText);
	}

	UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Background);
	if (CanvasSlot)
	{
		const FVector2D DesignSize = GetPrototypeDesignSize();
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
		CanvasSlot->SetSize(DesignSize);
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	}
}

void URogue10mVitalBarWidget::SetVitalView(FText InLabel, const FRogue10mHudVitalView& InVitalView)
{
	const bool bUnchanged = VitalLabel.EqualTo(InLabel)
		&& FMath::IsNearlyEqual(VitalView.Current, InVitalView.Current)
		&& FMath::IsNearlyEqual(VitalView.Max, InVitalView.Max)
		&& FMath::IsNearlyEqual(VitalView.Normalized, InVitalView.Normalized)
		&& VitalView.FillColor.Equals(InVitalView.FillColor)
		&& VitalView.bVisible == InVitalView.bVisible;
	if (bUnchanged)
	{
		return;
	}

	VitalLabel = MoveTemp(InLabel);
	VitalView = InVitalView;
	if (UI_ValueProgressBar)
	{
		UI_ValueProgressBar->SetPercent(VitalView.Normalized);
		UI_ValueProgressBar->SetFillColorAndOpacity(VitalView.FillColor);
	}
	if (UI_ValueText)
	{
		const FText ValueText = FText::FromString(FString::Printf(
			TEXT("%d / %d"), FMath::RoundToInt(VitalView.Current), FMath::RoundToInt(VitalView.Max)));
		UI_ValueText->SetText(bShowVitalLabel
			? FText::Format(NSLOCTEXT("Rogue10mHUD", "LabeledVitalValue", "{0}  {1}"), VitalLabel, ValueText)
			: ValueText);
	}
	BP_OnVitalViewChanged();
}

FText URogue10mVitalBarWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("체력/스테미나/마나 바"));
}

FText URogue10mVitalBarWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("ProgressBar와 수치 텍스트를 배치합니다. 메인 위젯에서는 HealthBarWidget, StaminaBarWidget, IdentityBarWidget 이름으로 사용합니다."));
}

FVector2D URogue10mVitalBarWidget::GetPrototypeDesignSize() const
{
	return FVector2D(360.0f, 34.0f);
}

void URogue10mProgressionWidget::SetProgressionView(const FRogue10mHudProgressionView& InProgressionView)
{
	const bool bUnchanged = ProgressionView.Level == InProgressionView.Level
		&& ProgressionView.CurrentExperience == InProgressionView.CurrentExperience
		&& ProgressionView.ExperienceToNextLevel == InProgressionView.ExperienceToNextLevel
		&& FMath::IsNearlyEqual(ProgressionView.ExperienceNormalized, InProgressionView.ExperienceNormalized);
	if (bUnchanged)
	{
		return;
	}

	ProgressionView = InProgressionView;
	bHasProgressionView = true;

	const float CurrentExperience = FMath::Max(0.0f, ProgressionView.CurrentExperience);
	const float ExperienceToNextLevel = FMath::Max(0.0f, ProgressionView.ExperienceToNextLevel);
	const float RemainingExperience = FMath::Max(0.0f, ExperienceToNextLevel - CurrentExperience);
	const FText ExperienceValueText = FText::FromString(FString::Printf(
		TEXT("%.3f / %.3f"), CurrentExperience, ExperienceToNextLevel));
	const FText ExperienceToolTip = FText::FromString(FString::Printf(
		TEXT("경험치\n현재 경험치: %.3f / %.3f\n다음 레벨까지: %.3f"),
		CurrentExperience, ExperienceToNextLevel, RemainingExperience));

	if (UI_ExperienceBar)
	{
		UI_ExperienceBar->SetPercent(FMath::Clamp(ProgressionView.ExperienceNormalized, 0.0f, 1.0f));
		UI_ExperienceBar->SetToolTipText(ExperienceToolTip);
	}

	if (UI_ExperienceText)
	{
		UI_ExperienceText->SetText(ExperienceValueText);
		UI_ExperienceText->SetToolTipText(ExperienceToolTip);
	}

	if (UI_LevelText)
	{
		UI_LevelText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::Max(1, ProgressionView.Level))));
	}

	BP_OnProgressionViewChanged();
}

FText URogue10mProgressionWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("레벨/경험치"));
}

FText URogue10mProgressionWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("레벨 텍스트와 경험치 ProgressBar를 배치합니다."));
}

FVector2D URogue10mProgressionWidget::GetPrototypeDesignSize() const
{
	return FVector2D(300.0f, 46.0f);
}

void URogue10mIdentityWidget::SetIdentityView(const FRogue10mHudIdentityView& InIdentityView)
{
	const bool bGaugeChanged = !FMath::IsNearlyEqual(IdentityView.Normalized, InIdentityView.Normalized)
		|| IdentityView.bHasIdentityResource != InIdentityView.bHasIdentityResource;
	const bool bIconChanged = IdentityView.IconTexture != InIdentityView.IconTexture;
	IdentityView = InIdentityView;
	if (UI_IdentityIcon)
	{
		if (bIconChanged)
		{
			// Clear and cancel any prior request before the engine streams the new icon.
			UI_IdentityIcon->SetBrushFromTexture(nullptr);
			if (!IdentityView.IconTexture.IsNull())
			{
				UI_IdentityIcon->SetBrushFromSoftTexture(IdentityView.IconTexture);
			}
		}
		UI_IdentityIcon->SetVisibility(IdentityView.IconTexture.IsValid()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (UI_IdentityFallbackIcon)
	{
		UI_IdentityFallbackIcon->SetVisibility(IdentityView.IconTexture.IsValid()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (UI_MasteryText)
	{
		UI_MasteryText->SetText(FText::Format(NSLOCTEXT("Rogue10mHUD", "IdentityMastery", "숙련 {0}"),
			FText::AsNumber(FMath::Max(0, IdentityView.MasteryLevel))));
		UI_MasteryText->SetColorAndOpacity(FSlateColor(IdentityView.OutlineColor));
	}
	if (UI_IdentityPercentText)
	{
		UI_IdentityPercentText->SetText(IdentityView.bHasIdentityResource
			? FText::FromString(FString::Printf(TEXT("%.0f%%"), FMath::Clamp(IdentityView.Normalized, 0.0f, 1.0f) * 100.0f))
			: FText::FromString(TEXT("--")));
	}
	if (bUseCircularResourceGauge && bGaugeChanged)
	{
		if (const TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
		{
			SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
	SetToolTipText(IdentityView.Label);
	BP_OnIdentityViewChanged();
}

int32 URogue10mIdentityWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 LastLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bUseCircularResourceGauge)
	{
		return LastLayer;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Diameter = static_cast<float>(FMath::Min(Size.X, Size.Y));
	const float Thickness = FMath::Clamp(CircularGaugeThickness, 1.0f, 20.0f);
	const float Radius = Diameter * 0.5f - Thickness * 0.5f - 3.0f;
	if (Radius <= 0.0f)
	{
		return LastLayer;
	}

	const FVector2f Center(static_cast<float>(Size.X * 0.5), static_cast<float>(Size.Y * 0.5));
	const ESlateDrawEffect DrawEffect = (bParentEnabled && GetIsEnabled())
		? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor StyleTint = InWidgetStyle.GetColorAndOpacityTint();
	const auto DrawArc = [&](float Fraction, const FLinearColor& Color)
	{
		const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt(128.0f * Fraction));
		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		for (int32 PointIndex = 0; PointIndex <= SegmentCount; ++PointIndex)
		{
			// Start at twelve o'clock and fill clockwise in screen coordinates.
			const float Angle = -UE_HALF_PI + UE_TWO_PI * Fraction
				* static_cast<float>(PointIndex) / static_cast<float>(SegmentCount);
			Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, ++LastLayer, AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points), DrawEffect, Color * StyleTint, true, Thickness);
	};

	DrawArc(1.0f, FLinearColor(0.085f, 0.10f, 0.09f, 1.0f));
	const float ResourceFraction = IdentityView.bHasIdentityResource
		? FMath::Clamp(IdentityView.Normalized, 0.0f, 1.0f) : 0.0f;
	if (ResourceFraction > 0.0f)
	{
		DrawArc(ResourceFraction, CircularGaugeColor);
	}
	return LastLayer;
}

FText URogue10mIdentityWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("아이덴티티"));
}

FText URogue10mIdentityWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("무기 숙련도, 기력/마력 등 특수 자원 게이지를 배치합니다."));
}

FVector2D URogue10mIdentityWidget::GetPrototypeDesignSize() const
{
	return FVector2D(116.0f, 136.0f);
}

void URogue10mMonsterInfoWidget::SetMonsterInfoView(const FRogue10mHudMonsterInfoView& InMonsterInfoView)
{
	MonsterInfoView = InMonsterInfoView;
	SetVisibility(MonsterInfoView.bHasMonster ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (UI_MonsterNameText)
	{
		UI_MonsterNameText->SetText(FText::FromString(FString::Printf(TEXT("LV %d : %s"),
			MonsterInfoView.Level, *MonsterInfoView.Name.ToString())));
	}
	if (UI_MonsterHealthBar)
	{
		UI_MonsterHealthBar->SetPercent(FMath::Clamp(MonsterInfoView.Health.Normalized, 0.0f, 1.0f));
	}
	if (UI_MonsterHealthText)
	{
		UI_MonsterHealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			FMath::RoundToInt(MonsterInfoView.Health.Current), FMath::RoundToInt(MonsterInfoView.Health.Max))));
	}
	BP_OnMonsterInfoViewChanged();
}

FText URogue10mMonsterInfoWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("몬스터 정보"));
}

FText URogue10mMonsterInfoWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("이름, 레벨, 속성, 상태, 체력 퍼센트를 표시합니다."));
}

FVector2D URogue10mMonsterInfoWidget::GetPrototypeDesignSize() const
{
	return FVector2D(420.0f, 64.0f);
}

void URogue10mQuickSlotWidget::SetQuickSlotView(const FRogue10mHudQuickSlotView& InQuickSlotView)
{
	QuickSlotView = InQuickSlotView;

	if (UI_KeyText)
	{
		FText KeyText = QuickSlotView.InputText;
		if (bUseCompactInputLabels)
		{
			// Leave other labels intact, including empty item slots and the dodge key.
			const FString InputLabel = KeyText.ToString();
			if (InputLabel == TEXT("좌클릭"))
			{
				KeyText = FText::FromString(TEXT("LMB"));
			}
			else if (InputLabel == TEXT("우클릭"))
			{
				KeyText = FText::FromString(TEXT("RMB"));
			}
			else if (InputLabel == TEXT("좌클릭 차징"))
			{
				KeyText = FText::FromString(TEXT("HOLD LMB"));
			}
			else if (InputLabel == TEXT("우클릭 차징"))
			{
				KeyText = FText::FromString(TEXT("HOLD RMB"));
			}
		}
		UI_KeyText->SetText(KeyText);
	}
	if (UI_LockedText)
	{
		UI_LockedText->SetText(NSLOCTEXT("Rogue10mHUD", "QuickSlotLocked", "잠금"));
		UI_LockedText->SetVisibility(QuickSlotView.bUnlocked
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (UI_CooldownText)
	{
		UI_CooldownText->SetText(QuickSlotView.CooldownRemaining > 0.0f
			? FText::FromString(FString::Printf(TEXT("%.1f"), QuickSlotView.CooldownRemaining))
			: FText::GetEmpty());
	}

	if (UI_CooldownShade)
	{
		UI_CooldownShade->SetVisibility(QuickSlotView.CooldownRemaining > 0.0f
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UI_IconImage)
	{
		UI_IconImage->SetBrushFromTexture(QuickSlotView.SkillIcon);
		UI_IconImage->SetColorAndOpacity(QuickSlotView.IconColor);
		UI_IconImage->SetVisibility(QuickSlotView.SkillIcon
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	const FText SlotToolTipText = QuickSlotView.bUnlocked
		? QuickSlotView.DisplayName
		: FText::FromString(FString::Printf(TEXT("%s: 해금되지 않음"), *QuickSlotView.InputText.ToString()));
	SetToolTipText(bUseCompactInputLabels && QuickSlotView.bUnlocked && !QuickSlotView.InputText.IsEmpty()
		? FText::Format(NSLOCTEXT("Rogue10mHUD", "CompactSlotToolTip", "{0}: {1}"),
			QuickSlotView.InputText, SlotToolTipText)
		: SlotToolTipText);

	if (UI_SlotFrame)
	{
		UI_SlotFrame->SetIsEnabled(QuickSlotView.bUnlocked);
		UI_SlotFrame->SetRenderOpacity(QuickSlotView.bUnlocked ? 1.0f : 0.35f);
	}

	BP_OnQuickSlotViewChanged();
}

bool URogue10mQuickSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const URogue10mSkillDragDropOperation* SkillOperation = Cast<URogue10mSkillDragDropOperation>(InOperation);
	if (!SkillOperation || !SkillOperation->bUnlocked || !SkillOperation->SkillData || QuickSlotView.bDodgeSlot)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!Combat || !Combat->AssignSkillToInputSlot(SkillOperation->SkillData, QuickSlotView.InputSlot))
	{
		return false;
	}

	QuickSlotView.SkillData = SkillOperation->SkillData;
	QuickSlotView.DisplayName = SkillOperation->SkillData->SkillName;
	QuickSlotView.SkillIcon = SkillOperation->SkillData->SkillIcon;
	QuickSlotView.IconColor = SkillOperation->SkillData->IconTint;
	QuickSlotView.bUnlocked = true;
	SetQuickSlotView(QuickSlotView);
	return true;
}
FText URogue10mQuickSlotWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("퀵 슬롯"));
}

FText URogue10mQuickSlotWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("아이콘, 입력 키, 쿨타임 오버레이, 잠금 표시를 배치합니다."));
}

FVector2D URogue10mQuickSlotWidget::GetPrototypeDesignSize() const
{
	return FVector2D(54.0f, 54.0f);
}

void URogue10mLogLineWidget::SetLogEntryView(const FRogue10mHudLogEntryView& InLogEntryView)
{
	LogEntryView = InLogEntryView;
	const FText MessageText = LogEntryView.bItemAcquisition
		? FText::Format(NSLOCTEXT("Rogue10mHUD", "ItemAcquisitionMessage", "{0} ×{1}"),
			LogEntryView.Message, FText::AsNumber(FMath::Max(0, LogEntryView.Quantity)))
		: LogEntryView.Message;
	if (UI_MessageText)
	{
		UI_MessageText->SetText(MessageText);
		UI_MessageText->SetColorAndOpacity(FSlateColor(LogEntryView.Color));
	}
	SetToolTipText(MessageText);
	if (LogEntryView.bItemAcquisition)
	{
		if (UI_ItemIconImage)
		{
			UI_ItemIconImage->SetBrushFromTexture(LogEntryView.ItemIcon);
			UI_ItemIconImage->SetVisibility(LogEntryView.ItemIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UI_ItemCountText)
		{
			UI_ItemCountText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), FMath::Max(0, LogEntryView.Quantity))));
		}
		if (UI_ItemNameText)
		{
			UI_ItemNameText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	BP_OnLogEntryViewChanged();
}

void URogue10mSkillSlotPanelWidget::SetSkillSlotViews(const TArray<FRogue10mHudQuickSlotView>& Views,
	TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass)
{
	RefreshSlot(UI_SkillSlotFrame1, 0, Views, InQuickSlotWidgetClass);
	RefreshSlot(UI_SkillSlotFrame2, 1, Views, InQuickSlotWidgetClass);
	RefreshSlot(UI_SkillSlotFrame3, 2, Views, InQuickSlotWidgetClass);
	RefreshSlot(UI_SkillSlotFrame4, 3, Views, InQuickSlotWidgetClass);
	RefreshSlot(UI_SkillSlotFrame5, 4, Views, InQuickSlotWidgetClass);
}

void URogue10mSkillSlotPanelWidget::RefreshSlot(UBorder* Frame, int32 Index,
	const TArray<FRogue10mHudQuickSlotView>& Views,
	TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass)
{
	if (!Frame || !Views.IsValidIndex(Index) || !InQuickSlotWidgetClass)
	{
		return;
	}

	URogue10mQuickSlotWidget* SlotWidget = Cast<URogue10mQuickSlotWidget>(Frame->GetContent());
	if (!SlotWidget)
	{
		SlotWidget = CreateWidget<URogue10mQuickSlotWidget>(GetOwningPlayer(), InQuickSlotWidgetClass);
		if (!SlotWidget)
		{
			return;
		}
		SlotWidget->SetOwningMainHUD(GetOwningMainHUD());
		Frame->SetContent(SlotWidget);
	}
	SlotWidget->SetQuickSlotView(Views[Index]);
}
FText URogue10mLogLineWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("로그/획득 알림 한 줄"));
}

FText URogue10mLogLineWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("시스템 로그 또는 아이템 획득 메시지 한 줄을 표시합니다."));
}

FVector2D URogue10mLogLineWidget::GetPrototypeDesignSize() const
{
	return FVector2D(340.0f, 24.0f);
}

void URogue10mMinimapMarkerWidget::SetMinimapMarkerView(const FRogue10mHudMinimapMarkerView& InMinimapMarkerView)
{
	MinimapMarkerView = InMinimapMarkerView;
	BP_OnMinimapMarkerViewChanged();
}

FText URogue10mMinimapMarkerWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("미니맵 마커"));
}

FText URogue10mMinimapMarkerWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("플레이어, 몬스터, NPC, 포탈 아이콘을 표시하는 반복 요소입니다."));
}

FVector2D URogue10mMinimapMarkerWidget::GetPrototypeDesignSize() const
{
	return FVector2D(24.0f, 24.0f);
}

void URogue10mShortcutHintWidget::SetShortcutHint(FText InKeyText, FText InLabelText, bool bInActive)
{
	KeyText = MoveTemp(InKeyText);
	LabelText = MoveTemp(InLabelText);
	bActive = bInActive;
	BP_OnShortcutHintChanged();
}

FText URogue10mShortcutHintWidget::GetPrototypeDesignTitle() const
{
	return FText::FromString(TEXT("단축키 힌트"));
}

FText URogue10mShortcutHintWidget::GetPrototypeDesignDescription() const
{
	return FText::FromString(TEXT("키 텍스트와 장비/아이템/스킬/설정 라벨을 표시합니다."));
}

FVector2D URogue10mShortcutHintWidget::GetPrototypeDesignSize() const
{
	return FVector2D(96.0f, 36.0f);
}
