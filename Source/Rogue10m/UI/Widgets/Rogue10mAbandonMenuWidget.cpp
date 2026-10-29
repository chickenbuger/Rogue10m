// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mAbandonMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Rogue10mPlayerController.h"

TSharedRef<SWidget> URogue10mAbandonMenuWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void URogue10mAbandonMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(bMenuOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	RefreshAvailability();
}

ARogue10mPlayerController* URogue10mAbandonMenuWidget::GetRogueController() const
{
	return Cast<ARogue10mPlayerController>(GetOwningPlayer());
}

void URogue10mAbandonMenuWidget::BuildTree()
{
	if (RootCanvas || !WidgetTree) { return; }
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AbandonMenuRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	const auto MakeText = [this](FName Name, const FText& Value, int32 Size, FLinearColor Color)
	{
		auto* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
		Text->SetText(Value); Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center); Text->SetAutoWrapText(true);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};
	const auto MakeButton = [this, &MakeText](FName Name, const FText& Label, bool bDestructive)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(bDestructive ? FLinearColor(0.47f, 0.16f, 0.08f) : FLinearColor(0.13f, 0.23f, 0.25f));
		Style.Hovered.TintColor = FSlateColor(bDestructive ? FLinearColor(0.65f, 0.25f, 0.11f) : FLinearColor(0.22f, 0.35f, 0.38f));
		Style.Pressed.TintColor = FSlateColor(bDestructive ? FLinearColor(0.32f, 0.09f, 0.04f) : FLinearColor(0.08f, 0.14f, 0.16f));
		Style.NormalPadding = FMargin(12, 7); Style.PressedPadding = FMargin(12, 7);
		Button->SetStyle(Style);
		auto* ButtonText = MakeText(NAME_None, Label, 19, FLinearColor(0.96f, 0.97f, 0.98f));
		ButtonText->SetAutoWrapText(false);
		Button->AddChild(ButtonText);
		return Button;
	};

	Footer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AbandonSettingsFooter"));
	Footer->SetBrushColor(FLinearColor(0.02f, 0.032f, 0.038f, 0.97f));
	Footer->SetPadding(FMargin(10, 8));
	auto* FooterSlot = RootCanvas->AddChildToCanvas(Footer);
	FooterSlot->SetAnchors(FAnchors(0.5f, 1.0f)); FooterSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	FooterSlot->SetPosition(FVector2D(0.0f, -115.0f)); FooterSlot->SetSize(FVector2D(480.0f, 95.0f));
	auto* FooterRows = WidgetTree->ConstructWidget<UVerticalBox>(); Footer->AddChild(FooterRows);
	auto* MenuButtonSize = WidgetTree->ConstructWidget<USizeBox>(); MenuButtonSize->SetHeightOverride(40);
	FooterRows->AddChildToVerticalBox(MenuButtonSize);
	MenuButton = MakeButton(TEXT("AbandonMenuButton"), NSLOCTEXT("Rogue10mAbandon", "Open", "탐험 포기"), true);
	MenuButtonSize->AddChild(MenuButton); MenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenConfirmation);
	ReasonText = MakeText(TEXT("AbandonReasonText"), FText::GetEmpty(), 14, FLinearColor(0.77f, 0.82f, 0.84f));
	ReasonText->SetAutoWrapText(false);
	FooterRows->AddChildToVerticalBox(ReasonText)->SetPadding(FMargin(0, 6, 0, 0));

	ConfirmationShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AbandonConfirmationShade"));
	ConfirmationShade->SetBrushColor(FLinearColor(0.005f, 0.01f, 0.014f, 0.86f));
	auto* ShadeSlot = RootCanvas->AddChildToCanvas(ConfirmationShade);
	ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ShadeSlot->SetOffsets(FMargin(0)); ShadeSlot->SetZOrder(1);
	ConfirmationContent = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("AbandonConfirmationContent"));
	ConfirmationContent->SetStretch(EStretch::ScaleToFit); ConfirmationContent->SetStretchDirection(EStretchDirection::DownOnly);
	ConfirmationContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* ContentSlot = RootCanvas->AddChildToCanvas(ConfirmationContent);
	ContentSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ContentSlot->SetOffsets(FMargin(24)); ContentSlot->SetZOrder(2);
	auto* CardSize = WidgetTree->ConstructWidget<USizeBox>(); CardSize->SetWidthOverride(580);
	ConfirmationContent->AddChild(CardSize);
	auto* CardScaleSlot = CastChecked<UScaleBoxSlot>(CardSize->Slot);
	CardScaleSlot->SetHorizontalAlignment(HAlign_Center); CardScaleSlot->SetVerticalAlignment(VAlign_Center);
	auto* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AbandonConfirmationCard"));
	Card->SetBrushColor(FLinearColor(0.02f, 0.032f, 0.038f, 1.0f)); Card->SetPadding(FMargin(28, 24)); CardSize->AddChild(Card);
	auto* CardOverlay = WidgetTree->ConstructWidget<UOverlay>(); Card->AddChild(CardOverlay);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	auto* RowsSlot = CardOverlay->AddChildToOverlay(Rows);
	RowsSlot->SetHorizontalAlignment(HAlign_Fill); RowsSlot->SetVerticalAlignment(VAlign_Fill);
	auto* Title = MakeText(TEXT("AbandonTitleText"), NSLOCTEXT("Rogue10mAbandon", "Title", "이번 탐험을 포기할까요?"), 26, FLinearColor(1.0f, 0.78f, 0.52f));
	Title->SetAutoWrapText(false);
	Rows->AddChildToVerticalBox(Title)->SetPadding(FMargin(22, 8, 54, 16));
	auto* Policy = MakeText(TEXT("AbandonPolicyText"), NSLOCTEXT("Rogue10mAbandon", "Policy", "런 물품·숙련·배운 기술은 초기화됩니다.\n공개한 기술 정보는 유지됩니다.\n재화는 입장 직전 값으로 복원됩니다.\n마을 준비 비용은 환불되지 않습니다."), 19, FLinearColor(0.86f, 0.9f, 0.92f));
	Policy->SetAutoWrapText(false);
	Policy->SetWrapTextAt(510); Rows->AddChildToVerticalBox(Policy)->SetPadding(FMargin(0, 0, 0, 12));
	ErrorText = MakeText(TEXT("AbandonErrorText"), FText::GetEmpty(), 16, FLinearColor(1.0f, 0.64f, 0.43f));
	Rows->AddChildToVerticalBox(ErrorText)->SetPadding(FMargin(0, 0, 0, 10));
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0, 6, 0, 8));
	CancelButton = MakeButton(TEXT("AbandonCancelButton"), NSLOCTEXT("Rogue10mAbandon", "Cancel", "계속 탐험"), false);
	ConfirmButton = MakeButton(TEXT("AbandonConfirmButton"), NSLOCTEXT("Rogue10mAbandon", "Confirm", "포기 확정"), true);
	auto* CancelSlot = Buttons->AddChildToHorizontalBox(CancelButton); CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CancelSlot->SetPadding(FMargin(0, 0, 7, 0));
	auto* ConfirmSlot = Buttons->AddChildToHorizontalBox(ConfirmButton); ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ConfirmSlot->SetPadding(FMargin(7, 0, 0, 0));
	CancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
	Rows->AddChildToVerticalBox(MakeText(TEXT("AbandonKeyboardHint"), NSLOCTEXT("Rogue10mAbandon", "Back", "Esc / Back: 취소"), 14, FLinearColor(0.64f, 0.71f, 0.75f)));
	CloseButton = MakeButton(TEXT("AbandonCloseButton"), FText::FromString(TEXT("×")), false);
	auto* CloseSlot = CardOverlay->AddChildToOverlay(CloseButton); CloseSlot->SetHorizontalAlignment(HAlign_Right); CloseSlot->SetVerticalAlignment(VAlign_Top);
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
	ConfirmationShade->SetVisibility(ESlateVisibility::Collapsed);
	ConfirmationContent->SetVisibility(ESlateVisibility::Collapsed);
}

void URogue10mAbandonMenuWidget::SetMenuOpen(bool bOpen)
{
	BuildTree();
	bMenuOpen = bOpen;
	if (!bOpen) { SetConfirmationOpen(false); }
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	RefreshAvailability();
}

void URogue10mAbandonMenuWidget::SetConfirmationOpen(bool bOpen)
{
	BuildTree();
	bConfirmationOpen = bOpen && bMenuOpen;
	if (ErrorText) { ErrorText->SetText(FText::GetEmpty()); ErrorText->SetVisibility(ESlateVisibility::Collapsed); }
	if (ConfirmationShade) { ConfirmationShade->SetVisibility(bConfirmationOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (ConfirmationContent) { ConfirmationContent->SetVisibility(bConfirmationOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	if (Footer) { Footer->SetVisibility(bConfirmationOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	RefreshAvailability();
	if (bConfirmationOpen) { FocusConfirmationCancel(); }
}

void URogue10mAbandonMenuWidget::RefreshAvailability()
{
	BuildTree();
	if (!MenuButton || !ReasonText) { return; }
	FText Reason;
	const auto* Controller = GetRogueController();
	const bool bAllowed = Controller && Controller->CanOpenAbandonMenuConfirmation(Reason);
	MenuButton->SetIsEnabled(bMenuOpen && bAllowed && !bHandlingPress);
	if (ConfirmButton) { ConfirmButton->SetIsEnabled(bConfirmationOpen && bAllowed && !bHandlingPress); }
	ReasonText->SetText(bAllowed
		? NSLOCTEXT("Rogue10mAbandon", "Ready", "마을 준비 비용은 환불되지 않습니다.")
		: Reason.IsEmpty() ? NSLOCTEXT("Rogue10mAbandon", "Unavailable", "지금은 탐험을 포기할 수 없습니다.") : Reason);
}

void URogue10mAbandonMenuWidget::ShowError(const FText& Error)
{
	BuildTree();
	if (ErrorText)
	{
		ErrorText->SetText(Error);
		ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void URogue10mAbandonMenuWidget::FocusConfirmationCancel()
{
	if (bConfirmationOpen && CancelButton) { CancelButton->SetKeyboardFocus(); }
}

void URogue10mAbandonMenuWidget::HandleOpenConfirmation()
{
	if (!bMenuOpen || bConfirmationOpen || bHandlingPress || !MenuButton || !MenuButton->GetIsEnabled()) { return; }
	{
		TGuardValue<bool> PressGuard(bHandlingPress, true);
		if (auto* Controller = GetRogueController()) { Controller->OpenAbandonMenuConfirmation(); }
	}
	RefreshAvailability();
}

void URogue10mAbandonMenuWidget::HandleConfirm()
{
	if (!bMenuOpen || !bConfirmationOpen || bHandlingPress || !ConfirmButton || !ConfirmButton->GetIsEnabled()) { return; }
	bool bAccepted = false;
	{
		TGuardValue<bool> PressGuard(bHandlingPress, true);
		ConfirmButton->SetIsEnabled(false);
		if (auto* Controller = GetRogueController()) { bAccepted = Controller->ConfirmAbandonMenu(); }
	}
	if (!bAccepted && bMenuOpen && bConfirmationOpen)
	{
		if (ErrorText && ErrorText->GetText().IsEmpty())
		{
			ShowError(NSLOCTEXT("Rogue10mAbandon", "Rejected", "포기 요청을 처리하지 못했습니다. 현재 탐험 상태를 확인해 주세요."));
		}
		RefreshAvailability();
	}
}

void URogue10mAbandonMenuWidget::HandleCancel()
{
	if (!bConfirmationOpen || bHandlingPress) { return; }
	if (auto* Controller = GetRogueController()) { Controller->CancelAbandonMenuConfirmation(); }
}

FReply URogue10mAbandonMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bConfirmationOpen)
	{
		const FKey Key = Event.GetKey();
		if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
		{
			if (!Event.IsRepeat()) { HandleCancel(); }
			return FReply::Handled();
		}
		if (Key == EKeys::O || Key == EKeys::F10)
		{
			if (!Event.IsRepeat()) { if (auto* Controller = GetRogueController()) { Controller->ToggleSettings(); } }
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply URogue10mAbandonMenuWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return bConfirmationOpen ? FReply::Handled() : Super::NativeOnMouseButtonDown(Geometry, Event);
}
