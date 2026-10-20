// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mRunResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mRunCompletionComponent.h"

TSharedRef<SWidget> URogue10mRunResultWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void URogue10mRunResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	RefreshResult();
}

URogue10mRunCompletionComponent* URogue10mRunResultWidget::GetCompletion() const
{
	const ARogue10mGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
	return GameMode ? GameMode->GetRunCompletionComponent() : nullptr;
}

void URogue10mRunResultWidget::BuildTree()
{
	if (TitleText || !WidgetTree) { return; }
	auto* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RunResultOverlay"));
	WidgetTree->RootWidget = Overlay;
	auto* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunResultShade"));
	Shade->SetBrushColor(FLinearColor(0.005f, 0.010f, 0.014f, 0.86f));
	auto* ShadeSlot = Overlay->AddChildToOverlay(Shade);
	ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
	ShadeSlot->SetVerticalAlignment(VAlign_Fill);

	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RunResultScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	auto* ScaleSlot = Overlay->AddChildToOverlay(Scale);
	ScaleSlot->SetPadding(FMargin(28));
	ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
	ScaleSlot->SetVerticalAlignment(VAlign_Fill);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(640);
	Scale->AddChild(Size);
	auto* ContentSlot = CastChecked<UScaleBoxSlot>(Size->Slot);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);
	auto* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunResultCard"));
	Card->SetBrushColor(FLinearColor(0.020f, 0.032f, 0.038f, 1.0f));
	Card->SetPadding(FMargin(40, 32));
	Size->AddChild(Card);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->AddChild(Rows);
	auto AddText = [this, Rows](FName Name, int32 FontSize, FLinearColor Color)
	{
		auto* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		Text->SetWrapTextAt(560);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 8));
		return Text;
	};
	TitleText = AddText(TEXT("RunResultTitle"), 42, FLinearColor(0.96f, 0.79f, 0.39f));
	DescriptionText = AddText(TEXT("RunResultDescription"), 20, FLinearColor(0.84f, 0.89f, 0.90f));
	RewardText = AddText(TEXT("RunResultRewards"), 25, FLinearColor(0.96f, 0.86f, 0.59f));
	SaveStatusText = AddText(TEXT("RunResultSaveStatus"), 17, FLinearColor(0.52f, 0.84f, 0.73f));
	ReturnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RunResultReturnToTown"));
	ReturnButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnToTown);
	FButtonStyle Style = ReturnButton->GetStyle();
	Style.Normal.TintColor = FSlateColor(FLinearColor(0.16f, 0.27f, 0.29f));
	Style.Hovered.TintColor = FSlateColor(FLinearColor(0.24f, 0.41f, 0.42f));
	Style.Pressed.TintColor = FSlateColor(FLinearColor(0.10f, 0.23f, 0.25f));
	Style.NormalPadding = FMargin(18, 15);
	Style.PressedPadding = FMargin(18, 15);
	ReturnButton->SetStyle(Style);
	ReturnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RunResultReturnLabel"));
	FSlateFontInfo Font = ReturnText->GetFont();
	Font.Size = 22;
	ReturnText->SetFont(Font);
	ReturnText->SetJustification(ETextJustify::Center);
	ReturnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.97f, 0.96f)));
	ReturnButton->AddChild(ReturnText);
	Rows->AddChildToVerticalBox(ReturnButton)->SetPadding(FMargin(0, 18, 0, 8));
	AddText(TEXT("RunResultKeyboardHint"), 14, FLinearColor(0.61f, 0.69f, 0.72f))
		->SetText(FText::FromString(TEXT("Enter / Space  확인")));
}

void URogue10mRunResultWidget::RefreshResult()
{
	BuildTree();
	if (!TitleText) { return; }
	const URogue10mRunCompletionComponent* Completion = GetCompletion();
	const bool bSettled = Completion && Completion->IsSettled();
	const bool bVictory = bSettled && Completion->IsVictory();
	TitleText->SetText(FText::FromString(bVictory ? TEXT("토벌 성공") : TEXT("탐험 실패")));
	TitleText->SetColorAndOpacity(FSlateColor(bVictory ? FLinearColor(0.96f, 0.79f, 0.39f) : FLinearColor(0.91f, 0.52f, 0.42f)));
	DescriptionText->SetText(FText::FromString(bVictory
		? TEXT("아자토르를 처치했습니다. 마을로 돌아가 다음 탐험을 준비하세요.")
		: TEXT("이번 탐험을 마쳤습니다. 마을에서 다시 도전할 수 있습니다.")));
	RewardText->SetVisibility(bVictory ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RewardText->SetText(FText::Format(FText::FromString(TEXT("보스 처치 보상\n경험치  +{0}     골드  +{1}")),
		FText::AsNumber(Completion ? Completion->GetRewardExperience() : 0),
		FText::AsNumber(Completion ? Completion->GetRewardGold() : 0)));
	const bool bSaved = bSettled && Completion->IsProgressionSaved();
	const bool bTravelStarted = bSettled && Completion->IsTravelStarted();
	SaveStatusText->SetText(Completion ? Completion->GetSaveStatus() : FText::FromString(TEXT("탐험 결과를 불러오지 못했습니다.")));
	SaveStatusText->SetColorAndOpacity(FSlateColor(bSaved ? FLinearColor(0.52f, 0.84f, 0.73f) : FLinearColor(1.0f, 0.64f, 0.43f)));
	ReturnText->SetText(FText::FromString(bTravelStarted ? TEXT("마을로 이동 중…")
		: bSaved ? TEXT("마을로 돌아가기") : TEXT("저장 재시도 후 귀환")));
	ReturnButton->SetIsEnabled(bSettled && !bTravelStarted);
}

void URogue10mRunResultWidget::HandleReturnToTown()
{
	if (!ReturnButton || !ReturnButton->GetIsEnabled()) { return; }
	if (URogue10mRunCompletionComponent* Completion = GetCompletion())
	{
		Completion->ReturnToTown();
	}
	RefreshResult();
}

FReply URogue10mRunResultWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		if (!Event.IsRepeat()) { HandleReturnToTown(); }
		return FReply::Handled();
	}
	// Terminal results cannot be dismissed into the completed gameplay session.
	if (Key == EKeys::Escape || Key == EKeys::F6) { return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
