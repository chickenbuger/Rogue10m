// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mQuitMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Rogue10mPlayerController.h"

TSharedRef<SWidget> URogue10mQuitMenuWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void URogue10mQuitMenuWidget::BuildTree()
{
	if (RootCanvas || !WidgetTree) { return; }
	SetIsFocusable(true);
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuitMenuRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const auto MakeText = [this](FName Name, const FText& Text, int32 Size, FLinearColor Color)
	{
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(Text); Label->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(true);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
	};
	const auto MakeButton = [this, &MakeText](FName Name, const FText& Text, bool bDestructive)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(bDestructive ? FLinearColor(0.47f, 0.16f, 0.08f) : FLinearColor(0.13f, 0.23f, 0.25f));
		Style.Hovered.TintColor = FSlateColor(bDestructive ? FLinearColor(0.65f, 0.25f, 0.11f) : FLinearColor(0.22f, 0.35f, 0.38f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.14f, 0.16f));
		Style.NormalPadding = FMargin(20, 12); Style.PressedPadding = Style.NormalPadding;
		Button->SetStyle(Style);
		Button->AddChild(MakeText(NAME_None, Text, 18, FLinearColor(0.96f, 0.97f, 0.98f)));
		return Button;
	};
	MenuButton = MakeButton(TEXT("QuitMenuButton"), NSLOCTEXT("Rogue10mQuit", "Open", "게임 종료"), true);
	auto* MenuSlot = RootCanvas->AddChildToCanvas(MenuButton);
	MenuSlot->SetAnchors(FAnchors(1, 0)); MenuSlot->SetAlignment(FVector2D(1, 0));
	MenuSlot->SetPosition(FVector2D(-24, 88)); MenuSlot->SetSize(FVector2D(170, 48));
	MenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpen);

	auto* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuitConfirmationShade"));
	Dim->SetBrushColor(FLinearColor(0.005f, 0.01f, 0.014f, 0.90f));
	auto* DimSlot = RootCanvas->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0, 0, 1, 1)); DimSlot->SetOffsets(FMargin(0)); DimSlot->SetZOrder(1);
	Shade = Dim;
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("QuitConfirmationContent"));
	Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
	auto* ScaleSlot = RootCanvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ScaleSlot->SetOffsets(FMargin(24)); ScaleSlot->SetZOrder(2);
	CardContent = Scale;
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(620);
	Scale->AddChild(Size);
	auto* InnerSlot = CastChecked<UScaleBoxSlot>(Size->Slot);
	InnerSlot->SetHorizontalAlignment(HAlign_Center); InnerSlot->SetVerticalAlignment(VAlign_Center);
	auto* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(FLinearColor(0.02f, 0.032f, 0.038f, 1)); Card->SetPadding(FMargin(28));
	Size->AddChild(Card);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Card->AddChild(Rows);
	Rows->AddChildToVerticalBox(MakeText(TEXT("QuitTitleText"), NSLOCTEXT("Rogue10mQuit", "Title", "게임을 종료할까요?"), 26, FLinearColor(1, 0.78f, 0.52f)))->SetPadding(FMargin(0, 0, 0, 20));
	PolicyText = MakeText(TEXT("QuitPolicyText"), FText::GetEmpty(), 18, FLinearColor(0.86f, 0.9f, 0.92f));
	PolicyText->SetWrapTextAt(560);
	Rows->AddChildToVerticalBox(PolicyText)->SetPadding(FMargin(0, 0, 0, 16));
	ErrorText = MakeText(TEXT("QuitErrorText"), FText::GetEmpty(), 17, FLinearColor(1, 0.64f, 0.43f));
	Rows->AddChildToVerticalBox(ErrorText)->SetPadding(FMargin(0, 0, 0, 12));
	auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Buttons);
	CancelButton = MakeButton(TEXT("QuitCancelButton"), NSLOCTEXT("Rogue10mQuit", "Cancel", "취소"), false);
	ConfirmButton = MakeButton(TEXT("QuitConfirmButton"), NSLOCTEXT("Rogue10mQuit", "Confirm", "종료 확인"), true);
	auto* CancelSlot = Buttons->AddChildToHorizontalBox(CancelButton);
	CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CancelSlot->SetPadding(FMargin(0, 0, 8, 0));
	auto* ConfirmSlot = Buttons->AddChildToHorizontalBox(ConfirmButton);
	ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ConfirmSlot->SetPadding(FMargin(8, 0, 0, 0));
	CancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
	Rows->AddChildToVerticalBox(MakeText(TEXT("QuitKeyboardHint"), NSLOCTEXT("Rogue10mQuit", "Hint", "Esc / Backspace / 게임패드 B: 취소"), 14, FLinearColor(0.64f, 0.73f, 0.77f)))->SetPadding(FMargin(0, 16, 0, 0));
	Shade->SetVisibility(ESlateVisibility::Collapsed); CardContent->SetVisibility(ESlateVisibility::Collapsed);
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
}

void URogue10mQuitMenuWidget::SetMenuContextVisible(bool bVisible)
{
	BuildTree(); bContextVisible = bVisible;
	SetVisibility(bContextVisible || bConfirmationOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (MenuButton) { MenuButton->SetVisibility(bContextVisible && !bConfirmationOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
}

void URogue10mQuitMenuWidget::SetConfirmationOpen(bool bOpen, const FText& Policy)
{
	BuildTree();
	const bool bChanged = bConfirmationOpen != bOpen;
	bConfirmationOpen = bOpen;
	if (PolicyText && bOpen) { PolicyText->SetText(Policy); }
	if (bChanged) { ShowError(FText::GetEmpty()); }
	Shade->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CardContent->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetMenuContextVisible(bContextVisible);
	if (bOpen && bChanged) { FocusCancel(); }
}

void URogue10mQuitMenuWidget::ShowError(const FText& Error)
{
	if (ErrorText) { ErrorText->SetText(Error); ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
}

void URogue10mQuitMenuWidget::FocusCancel()
{
	if (bConfirmationOpen && CancelButton) { CancelButton->SetKeyboardFocus(); }
}

void URogue10mQuitMenuWidget::HandleOpen()
{
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwningPlayer())) { PC->OpenQuitGameConfirmation(); }
}
void URogue10mQuitMenuWidget::HandleCancel()
{
	if (!bConfirmationOpen || bSubmitting) { return; }
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwningPlayer())) { PC->CancelQuitGameConfirmation(); }
}
void URogue10mQuitMenuWidget::HandleConfirm()
{
	if (!bConfirmationOpen || bSubmitting) { return; }
	TGuardValue<bool> Guard(bSubmitting, true);
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwningPlayer())) { PC->ConfirmQuitGame(); }
}

FReply URogue10mQuitMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bConfirmationOpen)
	{
		const FKey Key = Event.GetKey();
		if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
		{
			if (!Event.IsRepeat()) { HandleCancel(); }
			return FReply::Handled();
		}
		// Keep menu hotkeys from reaching either the confirmation buttons or gameplay.
		if (Key == EKeys::O || Key == EKeys::F10) { return FReply::Handled(); }
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
