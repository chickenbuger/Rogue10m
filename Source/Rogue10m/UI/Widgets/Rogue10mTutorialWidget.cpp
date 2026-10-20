// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

TSharedRef<SWidget> URogue10mTutorialWidget::RebuildWidget()
{
	// The native WidgetTree must exist before UUserWidget builds its Slate
	// content. NativeConstruct is called after that initial Slate rebuild.
	EnsureNativeWidget();
	return Super::RebuildWidget();
}

void URogue10mTutorialWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	EnsureNativeWidget();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URogue10mTutorialWidget::EnsureNativeWidget()
{
	if (NameText || !WidgetTree) { return; }
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TutorialCanvas"));
	WidgetTree->RootWidget = Canvas;
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TutorialScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Scale);
	CanvasSlot->SetAnchors(FAnchors(0.58f, 0.10f, 0.98f, 0.91f));
	CanvasSlot->SetOffsets(FMargin(0.0f));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TutorialSize"));
	Size->SetWidthOverride(420.0f);
	Scale->AddChild(Size);
	if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Size->Slot))
	{
		ScaleSlot->SetHorizontalAlignment(HAlign_Right);
		ScaleSlot->SetVerticalAlignment(VAlign_Top);
	}
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TutorialCard"));
	Border->SetBrushColor(FLinearColor(0.028f, 0.040f, 0.035f, 0.95f));
	Border->SetPadding(FMargin(18.0f, 14.0f));
	Size->AddChild(Border);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TutorialRows"));
	Border->AddChild(Rows);
	auto AddText = [this, Rows](const FName WidgetName, int32 FontSize, FLinearColor Color, float BottomGap)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		Text->SetWrapTextAt(384.0f);
		Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomGap));
		return Text;
	};
	NameText = AddText(TEXT("GuideName"), 25, FLinearColor(0.96f, 0.83f, 0.53f), 2.0f);
	RoleText = AddText(TEXT("GuideRole"), 15, FLinearColor(0.66f, 0.78f, 0.70f), 12.0f);
	DialogueText = AddText(TEXT("TutorialDialogue"), 19, FLinearColor(0.91f, 0.93f, 0.89f), 12.0f);
	ServicesText = AddText(TEXT("TownServices"), 16, FLinearColor(0.91f, 0.93f, 0.89f), 12.0f);
	ServicesText->SetVisibility(ESlateVisibility::Collapsed);
	StatusText = AddText(TEXT("TutorialStatus"), 16, FLinearColor(0.69f, 0.83f, 0.72f), 10.0f);
	ActionText = AddText(TEXT("TutorialAction"), 16, FLinearColor(0.96f, 0.83f, 0.53f), 0.0f);
}

void URogue10mTutorialWidget::SetServiceView(const FText& Services)
{
	EnsureNativeWidget();
	if (!ServicesText) { return; }
	ServicesText->SetText(Services);
	ServicesText->SetVisibility(Services.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void URogue10mTutorialWidget::SetDialogueView(const FText& Name, const FText& Role,
	const FText& Dialogue, const FText& Status, const FText& Action)
{
	EnsureNativeWidget();
	if (!NameText) { return; }
	NameText->SetText(Name);
	RoleText->SetText(Role);
	DialogueText->SetText(Dialogue);
	StatusText->SetText(Status);
	ActionText->SetText(Action);
}

FText URogue10mTutorialWidget::GetDisplayedDialogue() const
{
	return DialogueText ? DialogueText->GetText() : FText::GetEmpty();
}

FText URogue10mTutorialWidget::GetDisplayedServices() const
{
	return ServicesText ? ServicesText->GetText() : FText::GetEmpty();
}
