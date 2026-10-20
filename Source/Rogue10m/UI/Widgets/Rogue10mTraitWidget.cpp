// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTraitWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Math/Box2D.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitCatalogDataAsset.h"
#include "Rogue10mTutorialNPC.h"

namespace Rogue10mTraitsUI
{
	const FLinearColor Gold(0.86f, 0.65f, 0.28f);
	const FLinearColor White(0.94f, 0.92f, 0.86f);
	const FLinearColor Muted(0.66f, 0.70f, 0.69f);
	const FLinearColor Teal(0.27f, 0.89f, 0.80f);
	const FLinearColor Panel(0.025f, 0.035f, 0.034f, 0.99f);
	const ERogue10mTownTrait Order[] = {
		ERogue10mTownTrait::Power, ERogue10mTownTrait::CriticalChance,
		ERogue10mTownTrait::CriticalDamage, ERogue10mTownTrait::Combo,
		ERogue10mTownTrait::Vitality, ERogue10mTownTrait::Defense,
		ERogue10mTownTrait::KillRecovery, ERogue10mTownTrait::LastStand,
		ERogue10mTownTrait::MoveSpeed, ERogue10mTownTrait::MaxStamina,
		ERogue10mTownTrait::DodgeEfficiency, ERogue10mTownTrait::DodgeCounter,
		ERogue10mTownTrait::GoldFind, ERogue10mTownTrait::StaminaRecovery,
		ERogue10mTownTrait::PotionMastery, ERogue10mTownTrait::BossHunter
	};
	FText CategoryName(int32 Category)
	{
		switch (Category)
		{
		case 0: return FText::FromString(TEXT("전투"));
		case 1: return FText::FromString(TEXT("생존"));
		case 2: return FText::FromString(TEXT("기동"));
		case 3: return FText::FromString(TEXT("탐험"));
		default: return FText::FromString(TEXT("전체"));
		}
	}
	FLinearColor CategoryColor(int32 Category)
	{
		switch (Category)
		{
		case 0: return FLinearColor(0.76f, 0.30f, 0.23f);
		case 1: return FLinearColor(0.28f, 0.65f, 0.39f);
		case 2: return FLinearColor(0.27f, 0.54f, 0.75f);
		default: return Gold;
		}
	}
	void StyleButton(UButton* Button, bool bSelected = false)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(bSelected ? FLinearColor(0.19f, 0.14f, 0.065f) : FLinearColor(0.045f, 0.058f, 0.055f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(0.22f, 0.18f, 0.09f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.10f, 0.26f, 0.23f));
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.05f, 0.055f, 0.055f));
		Button->SetStyle(Style);
	}
	FText Number(float Value)
	{
		FNumberFormattingOptions Format;
		Format.SetMaximumFractionalDigits(2);
		return FText::AsNumber(Value, &Format);
	}
}

URogue10mTraitActionButton::URogue10mTraitActionButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}

void URogue10mTraitActionButton::Configure(URogue10mTraitWidget* Owner, int32 InAction, ERogue10mTownTrait InTrait)
{
	OwnerWidget = Owner;
	Action = InAction;
	Trait = InTrait;
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void URogue10mTraitActionButton::HandleClicked()
{
	if (URogue10mTraitWidget* Owner = OwnerWidget.Get()) { Owner->HandleAction(Action, Trait); }
}

URogue10mTraitWidget::URogue10mTraitWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TraitIconAtlasAsset = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Traits/T_TraitCatalogAtlas.T_TraitCatalogAtlas")));
}

TSharedRef<SWidget> URogue10mTraitWidget::RebuildWidget()
{
	EnsureNativeWidget();
	return Super::RebuildWidget();
}

void URogue10mTraitWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	EnsureNativeWidget();
	RefreshFromData();
}

void URogue10mTraitWidget::NativeDestruct()
{
	ReleaseContext();
	Super::NativeDestruct();
}

void URogue10mTraitWidget::InitializeTraits(URogue10mTownProgressionComponent* Town, ARogue10mTutorialNPC* NPC)
{
	ReleaseContext();
	TownComponent = Town;
	ServiceNPC = NPC;
	if (Town)
	{
		Town->OnTownProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		BoundPlayerState = Cast<ARogue10mPlayerState>(Town->GetOwner());
		if (BoundPlayerState.IsValid())
		{
			BoundPlayerState->OnGrowthChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		}
	}
	if (!IconAtlas)
	{
		IconAtlas = TraitIconAtlasAsset.LoadSynchronous();
	}
	CategoryFilter = -1;
	SelectedTrait = ERogue10mTownTrait::CriticalChance;
	EnsureNativeWidget();
	RebuildCards();
	SetMessage(FText::FromString(TEXT("특성은 캐릭터별로 저장됩니다. 레벨이 오를 때 1포인트를 얻습니다.")), false);
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mProgressionSubsystem* Save = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	if (Save && !Save->GetLastError().IsEmpty())
	{
		SetMessage(FText::Format(FText::FromString(TEXT("이전 저장을 완료하지 못했습니다. 저장 재시도가 필요합니다. {0}")), Save->GetLastError()), true);
	}
	RefreshFromData();
}

void URogue10mTraitWidget::ReleaseContext()
{
	if (TownComponent.IsValid()) { TownComponent->OnTownProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged); }
	if (BoundPlayerState.IsValid()) { BoundPlayerState->OnGrowthChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged); }
	TownComponent.Reset();
	BoundPlayerState.Reset();
	ServiceNPC.Reset();
	bResetConfirmation = false;
	if (ResetOverlay) { ResetOverlay->SetVisibility(ESlateVisibility::Collapsed); }
	if (MainContent) { MainContent->SetIsEnabled(true); }
}

UTextBlock* URogue10mTraitWidget::MakeText(FName Name, int32 Size, FLinearColor Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

URogue10mTraitActionButton* URogue10mTraitWidget::MakeButton(FName Name, const FText& Label, int32 Action, ERogue10mTownTrait Trait)
{
	URogue10mTraitActionButton* Button = WidgetTree->ConstructWidget<URogue10mTraitActionButton>(URogue10mTraitActionButton::StaticClass(), Name);
	Button->Configure(this, Action, Trait);
	Rogue10mTraitsUI::StyleButton(Button);
	UTextBlock* Text = MakeText(NAME_None, 23, Rogue10mTraitsUI::White);
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	return Button;
}

UImage* URogue10mTraitWidget::MakeIcon(ERogue10mTownTrait Trait, float Size)
{
	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	SetIconBrush(Icon, Trait, Size);
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Icon;
}

void URogue10mTraitWidget::SetIconBrush(UImage* Icon, ERogue10mTownTrait Trait, float Size) const
{
	int32 Index = 0;
	for (int32 Candidate = 0; Candidate < UE_ARRAY_COUNT(Rogue10mTraitsUI::Order); ++Candidate)
	{
		if (Rogue10mTraitsUI::Order[Candidate] == Trait) { Index = Candidate; break; }
	}
	// The atlas retains the complete 1672 x 941 source; only icon pixels are sampled.
	const float Left[] = { 86.0f, 375.0f, 662.0f, 948.0f };
	const float Top[] = { 278.0f, 427.0f, 575.0f, 722.0f };
	FSlateBrush Brush;
	Brush.SetResourceObject(IconAtlas);
	Brush.SetImageSize(FVector2f(Size, Size));
	Brush.DrawAs = IconAtlas ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
	Brush.SetUVRegion(FBox2f(FVector2f(Left[Index / 4] / 1672.0f, Top[Index % 4] / 941.0f),
		FVector2f((Left[Index / 4] + 104.0f) / 1672.0f, (Top[Index % 4] + 102.0f) / 941.0f)));
	Icon->SetBrush(Brush);
}

void URogue10mTraitWidget::EnsureNativeWidget()
{
	if (CardGrid || !WidgetTree) { return; }
	using namespace Rogue10mTraitsUI;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TraitCanvas"));
	WidgetTree->RootWidget = Canvas;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TraitBackdrop"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimSlot->SetOffsets(FMargin(0.0f));
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TraitResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	UCanvasPanelSlot* ScaleSlot = Canvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0.035f, 0.035f, 0.965f, 0.965f));
	ScaleSlot->SetOffsets(FMargin(0.0f));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TraitDesignSize"));
	Size->SetWidthOverride(1560.0f);
	Size->SetHeightOverride(1000.0f);
	Scale->AddChild(Size);
	UBorder* Outer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TraitGoldFrame"));
	Outer->SetBrushColor(Gold);
	Outer->SetPadding(FMargin(2.0f));
	Size->AddChild(Outer);
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TraitWindowFrame"));
	Frame->SetBrushColor(Panel);
	Frame->SetPadding(FMargin(24.0f, 16.0f));
	Outer->AddChild(Frame);
	MainContent = Frame;
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TraitWindowRows"));
	Frame->AddChild(Rows);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 14));
	UTextBlock* Title = MakeText(TEXT("TraitTitle"), 37, White);
	Title->SetText(FText::FromString(TEXT("특성 강화")));
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	IdentityText = MakeText(TEXT("TraitCharacterIdentity"), 22, Muted);
	Header->AddChildToHorizontalBox(IdentityText)->SetPadding(FMargin(12, 0, 24, 0));
	PointsText = MakeText(TEXT("TraitAvailablePoints"), 32, Gold);
	Header->AddChildToHorizontalBox(PointsText)->SetPadding(FMargin(0, 0, 24, 0));
	Header->AddChildToHorizontalBox(MakeButton(TEXT("TraitCloseButton"), FText::FromString(TEXT("닫기  ×")), 24));
	UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TraitCategoryFilters"));
	Rows->AddChildToVerticalBox(FilterRow)->SetPadding(FMargin(0, 0, 0, 12));
	for (int32 Category = -1; Category < 4; ++Category)
	{
		URogue10mTraitActionButton* Filter = MakeButton(FName(*FString::Printf(TEXT("TraitFilter%d"), Category + 1)),
			Rogue10mTraitsUI::CategoryName(Category), 11 + Category);
		FilterButtons.Add(Filter);
		UHorizontalBoxSlot* FilterSlot = FilterRow->AddChildToHorizontalBox(Filter);
		FilterSlot->SetPadding(FMargin(0, 0, 8, 0));
		FilterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* LeftSlot = Body->AddChildToHorizontalBox(Left);
	LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LeftSlot->SetPadding(FMargin(0, 0, 20, 0));
	UHorizontalBox* Categories = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TraitColumnCategories"));
	Left->AddChildToVerticalBox(Categories)->SetPadding(FMargin(4, 0, 4, 8));
	for (int32 Category = 0; Category < 4; ++Category)
	{
		UTextBlock* Label = MakeText(NAME_None, 23, CategoryColor(Category));
		Label->SetText(Rogue10mTraitsUI::CategoryName(Category));
		Label->SetJustification(ETextJustify::Center);
		Categories->AddChildToHorizontalBox(Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	CardScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TraitListScroll"));
	CardScroll->SetClipping(EWidgetClipping::ClipToBounds);
	Left->AddChildToVerticalBox(CardScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TraitCardGrid"));
	CardGrid->SetSlotPadding(FMargin(4));
	CardScroll->AddChild(CardGrid);
	ResetButton = MakeButton(TEXT("TraitResetButton"), FText::FromString(TEXT("포인트 재분배  ·  마을에서 무료")), 21);
	Left->AddChildToVerticalBox(ResetButton)->SetPadding(FMargin(4, 10, 4, 0));
	USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DetailSize->SetWidthOverride(420.0f);
	Body->AddChildToHorizontalBox(DetailSize);
	UBorder* DetailFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DetailFrame->SetBrushColor(FLinearColor(0.085f, 0.074f, 0.047f));
	DetailFrame->SetPadding(FMargin(14));
	DetailSize->AddChild(DetailFrame);
	UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	DetailFrame->AddChild(DetailScroll);
	UVerticalBox* DetailRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TraitDetailRows"));
	DetailScroll->AddChild(DetailRows);
	UHorizontalBox* DetailHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DetailRows->AddChildToVerticalBox(DetailHeader)->SetPadding(FMargin(0, 0, 0, 12));
	USizeBox* DetailIconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DetailIconSize->SetWidthOverride(88);
	DetailIconSize->SetHeightOverride(88);
	DetailIcon = MakeIcon(SelectedTrait, 88);
	DetailIconSize->AddChild(DetailIcon);
	DetailHeader->AddChildToHorizontalBox(DetailIconSize)->SetPadding(FMargin(0, 0, 14, 0));
	UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DetailHeader->AddChildToHorizontalBox(Names)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DetailName = MakeText(TEXT("SelectedTraitName"), 29, White);
	DetailName->SetAutoWrapText(true);
	Names->AddChildToVerticalBox(DetailName);
	DetailCategory = MakeText(TEXT("SelectedTraitCategory"), 19, Muted);
	Names->AddChildToVerticalBox(DetailCategory)->SetPadding(FMargin(0, 6, 0, 0));
	auto DetailLine = [this, DetailRows](FName Name, int32 FontSize, FLinearColor Color, float Gap)
	{
		UTextBlock* Text = MakeText(Name, FontSize, Color);
		Text->SetAutoWrapText(true);
		Text->SetWrapTextAt(378);
		DetailRows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 0, 0, Gap));
		return Text;
	};
	DetailDescription = DetailLine(TEXT("SelectedTraitDescription"), 21, White, 18);
	DetailRank = DetailLine(TEXT("SelectedTraitRank"), 22, Gold, 12);
	CurrentEffect = DetailLine(TEXT("SelectedTraitCurrentEffect"), 22, White, 8);
	NextEffect = DetailLine(TEXT("SelectedTraitNextEffect"), 22, Teal, 18);
	DetailCondition = DetailLine(TEXT("SelectedTraitConditions"), 20, Muted, 18);
	CostText = DetailLine(TEXT("SelectedTraitCost"), 22, White, 8);
	RemainingText = DetailLine(TEXT("SelectedTraitRemainingPoints"), 22, White, 18);
	UpgradeButton = MakeButton(TEXT("TraitUpgradeButton"), FText::FromString(TEXT("특성 강화")), 20);
	DetailRows->AddChildToVerticalBox(UpgradeButton)->SetPadding(FMargin(0, 0, 0, 12));
	StatusText = MakeText(TEXT("TraitStatusMessage"), 20, Muted);
	StatusText->SetAutoWrapText(true);
	Rows->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 12, 0, 6));
	SaveRetryButton = MakeButton(TEXT("TraitSaveRetryButton"), FText::FromString(TEXT("현재 특성 다시 저장")), 25);
	Rows->AddChildToVerticalBox(SaveRetryButton)->SetPadding(FMargin(0, 0, 0, 6));
	SaveRetryButton->SetVisibility(ESlateVisibility::Collapsed);
	UTextBlock* Hint = MakeText(TEXT("TraitKeyboardHint"), 18, Muted);
	Hint->SetText(FText::FromString(TEXT("방향키  선택  ·  Enter  강화  ·  Tab/1~5  분류  ·  R  재분배  ·  Esc  닫기")));
	Rows->AddChildToVerticalBox(Hint);
	UOverlay* Modal = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TraitResetConfirmation"));
	UCanvasPanelSlot* ModalSlot = Canvas->AddChildToCanvas(Modal);
	ModalSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ModalSlot->SetOffsets(FMargin(0));
	ModalSlot->SetZOrder(10);
	ResetOverlay = Modal;
	UBorder* ModalDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TraitResetBackdrop"));
	ModalDim->SetBrushColor(FLinearColor(0, 0, 0, 0.85f));
	UOverlaySlot* DimOverlaySlot = Modal->AddChildToOverlay(ModalDim);
	DimOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
	DimOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	UScaleBox* ModalScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	ModalScale->SetStretch(EStretch::ScaleToFit);
	ModalScale->SetStretchDirection(EStretchDirection::DownOnly);
	UOverlaySlot* ModalScaleSlot = Modal->AddChildToOverlay(ModalScale);
	ModalScaleSlot->SetPadding(FMargin(30));
	ModalScaleSlot->SetHorizontalAlignment(HAlign_Fill);
	ModalScaleSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* ModalSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TraitResetDialogSize"));
	ModalSize->SetWidthOverride(750);
	ModalSize->SetHeightOverride(570);
	UScaleBoxSlot* DialogScaleSlot = CastChecked<UScaleBoxSlot>(ModalScale->AddChild(ModalSize));
	DialogScaleSlot->SetHorizontalAlignment(HAlign_Center);
	DialogScaleSlot->SetVerticalAlignment(VAlign_Center);
	UBorder* ModalFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TraitResetDialogFrame"));
	ModalFrame->SetBrushColor(Panel);
	ModalFrame->SetPadding(FMargin(35));
	ModalSize->AddChild(ModalFrame);
	UVerticalBox* ModalRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ModalFrame->AddChild(ModalRows);
	UTextBlock* ModalTitle = MakeText(TEXT("TraitResetTitle"), 34, Gold);
	ModalTitle->SetText(FText::FromString(TEXT("특성 포인트 재분배")));
	ModalRows->AddChildToVerticalBox(ModalTitle)->SetPadding(FMargin(0, 0, 0, 22));
	ResetSummary = MakeText(TEXT("TraitResetSummary"), 24, White);
	ResetSummary->SetAutoWrapText(true);
	ModalRows->AddChildToVerticalBox(ResetSummary)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* ModalActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ModalRows->AddChildToVerticalBox(ModalActions)->SetPadding(FMargin(0, 20, 0, 12));
	UHorizontalBoxSlot* CancelSlot = ModalActions->AddChildToHorizontalBox(MakeButton(TEXT("TraitCancelResetButton"), FText::FromString(TEXT("취소")), 23));
	CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CancelSlot->SetPadding(FMargin(0, 0, 12, 0));
	ConfirmResetButton = MakeButton(TEXT("TraitConfirmResetButton"), FText::FromString(TEXT("재분배 실행")), 22);
	ModalActions->AddChildToHorizontalBox(ConfirmResetButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* ModalHint = MakeText(TEXT("TraitResetKeyboardHint"), 19, Muted);
	ModalHint->SetText(FText::FromString(TEXT("Enter  재분배 실행  ·  Esc  취소  ·  마을에서만 이용 가능")));
	ModalRows->AddChildToVerticalBox(ModalHint);
	Modal->SetVisibility(ESlateVisibility::Collapsed);
}

void URogue10mTraitWidget::RebuildCards()
{
	if (!CardGrid) { return; }
	CardGrid->ClearChildren();
	if (CardScroll) { CardScroll->SetScrollOffset(0); }
	Cards.Reset();
	VisibleTraits.Reset();
	if (!TownComponent.IsValid()) { return; }
	if (UWidget* Headings = WidgetTree->FindWidget(TEXT("TraitColumnCategories")))
	{
		Headings->SetVisibility(CategoryFilter < 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rogue10mTraitsUI::Order); ++Index)
	{
		const ERogue10mTownTrait Trait = Rogue10mTraitsUI::Order[Index];
		const FRogue10mTraitDefinition Definition = TownComponent->GetUpgradeDefinition(Trait, GetServiceData());
		if (CategoryFilter >= 0 && static_cast<int32>(Definition.Category) != CategoryFilter) { continue; }
		const int32 VisibleIndex = VisibleTraits.Add(Trait);
		FRogue10mTraitCardView Card;
		Card.Trait = Trait;
		const bool bFullCatalog = CategoryFilter < 0;
		const float CardWidth = bFullCatalog ? 248.0f : 500.0f;
		const float CardInnerWidth = CardWidth - 20.0f;
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(CardWidth);
		CardSize->SetMinDesiredHeight(bFullCatalog ? 192.0f : 240.0f);
		CardSize->SetClipping(EWidgetClipping::ClipToBounds);
		Card.Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card.Frame->SetPadding(FMargin(2));
		CardSize->AddChild(Card.Frame);
		Card.Button = MakeButton(NAME_None, FText::GetEmpty(), 0, Trait);
		Card.Button->ClearChildren();
		FButtonStyle CardStyle = Card.Button->GetStyle();
		CardStyle.NormalPadding = FMargin(0);
		CardStyle.PressedPadding = FMargin(0);
		Card.Button->SetStyle(CardStyle);
		Card.Frame->AddChild(Card.Button);
		UBorder* CardPadding = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CardPadding->SetBrushColor(FLinearColor::Transparent);
		CardPadding->SetPadding(FMargin(8));
		UButtonSlot* CardContentSlot = CastChecked<UButtonSlot>(Card.Button->AddChild(CardPadding));
		CardContentSlot->SetPadding(FMargin(0));
		CardContentSlot->SetHorizontalAlignment(HAlign_Fill);
		CardContentSlot->SetVerticalAlignment(VAlign_Fill);
		UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CardPadding->AddChild(Contents);
		UHorizontalBox* CardHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Contents->AddChildToVerticalBox(CardHeader)->SetPadding(FMargin(0,0,0,6));
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		const float IconPixels = bFullCatalog ? 38.0f : 72.0f;
		IconSize->SetWidthOverride(IconPixels);
		IconSize->SetHeightOverride(IconPixels);
		IconSize->AddChild(MakeIcon(Trait, IconPixels));
		CardHeader->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0,0,8,0));
		Card.Name = MakeText(NAME_None, bFullCatalog ? 18 : 25, Rogue10mTraitsUI::White);
		Card.Name->SetAutoWrapText(true);
		Card.Name->SetWrapTextAt(CardInnerWidth-IconPixels-8.0f);
		UHorizontalBoxSlot* NameSlot = CardHeader->AddChildToHorizontalBox(Card.Name);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		Card.Rank = MakeText(NAME_None, bFullCatalog ? 18 : 20, Rogue10mTraitsUI::Gold);
		Contents->AddChildToVerticalBox(Card.Rank)->SetPadding(FMargin(0,0,0,4));
		Card.Effect = MakeText(NAME_None, bFullCatalog ? 17 : 21, Rogue10mTraitsUI::Muted);
		Card.Effect->SetAutoWrapText(true);
		Card.Effect->SetWrapTextAt(CardInnerWidth);
		Contents->AddChildToVerticalBox(Card.Effect);
		Card.State = MakeText(NAME_None, bFullCatalog ? 16 : 18, Rogue10mTraitsUI::Teal);
		Contents->AddChildToVerticalBox(Card.State)->SetPadding(FMargin(0,4,0,0));
		UUniformGridSlot* GridSlot = CardGrid->AddChildToUniformGrid(CardSize,
			CategoryFilter < 0 ? Index % 4 : VisibleIndex / 2,
			CategoryFilter < 0 ? Index / 4 : VisibleIndex % 2);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
		Cards.Add(Card);
	}
	if (!VisibleTraits.Contains(SelectedTrait) && VisibleTraits.Num() > 0) { SelectedTrait = VisibleTraits[0]; }
}

const URogue10mTownServiceDataAsset* URogue10mTraitWidget::GetServiceData() const
{
	return ServiceNPC.IsValid() ? ServiceNPC->GetTraitServiceData() : nullptr;
}

bool URogue10mTraitWidget::CanInteract(FText& OutReason) const
{
	if (!TownComponent.IsValid() || !ServiceNPC.IsValid())
	{
		OutReason = FText::FromString(TEXT("특성 서비스를 준비하지 못했습니다."));
		return false;
	}
	return ServiceNPC->CanUseTraitWindow(OutReason);
}

void URogue10mTraitWidget::RefreshFromData()
{
	EnsureNativeWidget();
	if (!CardGrid || !TownComponent.IsValid()) { return; }
	if (Cards.Num() == 0) { RebuildCards(); }
	const int32 Points = TownComponent->GetAvailableTraitPoints();
	PointsText->SetText(FText::Format(FText::FromString(TEXT("보유 포인트  {0}")), FText::AsNumber(Points)));
	if (BoundPlayerState.IsValid())
	{
		IdentityText->SetText(FText::Format(FText::FromString(TEXT("{0}  ·  LV {1}")),
			BoundPlayerState->GetCharacterDisplayName(), FText::AsNumber(BoundPlayerState->GetPlayerLevel())));
	}
	for (FRogue10mTraitCardView& Card : Cards)
	{
		const FRogue10mTraitDefinition Definition = TownComponent->GetUpgradeDefinition(Card.Trait, GetServiceData());
		const int32 Rank = TownComponent->GetTraitLevel(Card.Trait);
		const bool bLocked = BoundPlayerState.IsValid() && BoundPlayerState->GetPlayerLevel() < Definition.UnlockLevel;
		Card.Name->SetText(Definition.DisplayName);
		Card.Rank->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), FText::AsNumber(Rank), FText::AsNumber(Definition.MaxRank)));
		Card.Effect->SetText(Definition.EffectSummary);
		if (Card.Trait == ERogue10mTownTrait::Vitality || Card.Trait == ERogue10mTownTrait::Power)
		{
			Card.Effect->SetText(FText::Format(FText::FromString(Card.Trait == ERogue10mTownTrait::Vitality
				? TEXT("최대 체력 +{0}/단계") : TEXT("공격력 +{0}/단계")), Rogue10mTraitsUI::Number(Definition.ValuePerRank)));
		}
		Card.State->SetText(bLocked ? FText::Format(FText::FromString(TEXT("잠김 · LV {0}")), FText::AsNumber(Definition.UnlockLevel))
			: Rank >= Definition.MaxRank ? FText::FromString(TEXT("최대 단계"))
			: Points < Definition.PointCost ? FText::FromString(TEXT("포인트 부족")) : FText::GetEmpty());
		Card.Name->SetColorAndOpacity(FSlateColor(bLocked ? Rogue10mTraitsUI::Muted : Rogue10mTraitsUI::White));
		Card.Frame->SetBrushColor(Card.Trait == SelectedTrait ? Rogue10mTraitsUI::Gold
			: bLocked ? FLinearColor(0.19f, 0.20f, 0.20f) : Rogue10mTraitsUI::CategoryColor(static_cast<int32>(Definition.Category)) * 0.65f);
		Rogue10mTraitsUI::StyleButton(Card.Button, Card.Trait == SelectedTrait);
	}
	for (int32 Index = 0; Index < FilterButtons.Num(); ++Index)
	{
		Rogue10mTraitsUI::StyleButton(FilterButtons[Index], Index - 1 == CategoryFilter);
	}
	FText Reason;
	const bool bCanInteract = CanInteract(Reason);
	ResetButton->SetIsEnabled(bCanInteract && TownComponent->CanChangeTraits() && TownComponent->GetSpentTraitPoints() > 0);
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mProgressionSubsystem* Save = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	const bool bCanRetry = Save && Save->GetAttachedProfileId().IsValid() && !Save->GetLastError().IsEmpty();
	SaveRetryButton->SetVisibility(bCanRetry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SaveRetryButton->SetIsEnabled(bCanInteract && !bResetConfirmation && Save && Save->CanRetrySave());
	RefreshDetail();
	if (bResetConfirmation)
	{
		const int32 Spent = TownComponent->GetSpentTraitPoints();
		ResetSummary->SetText(FText::Format(FText::FromString(TEXT("투자한 특성을 초기화하고 사용한 포인트를 모두 돌려받습니다.\n\n사용한 포인트  {0}\n돌려받는 포인트  +{0}\n재분배 후 보유  {1} → {2}\n\n재분배 비용 없음\n레벨과 스킬, 아이템은 유지됩니다.")),
			FText::AsNumber(Spent), FText::AsNumber(Points), FText::AsNumber(static_cast<int64>(Points) + Spent)));
		ConfirmResetButton->SetIsEnabled(bCanInteract && TownComponent->CanChangeTraits() && Spent > 0);
	}
}

FText URogue10mTraitWidget::FormatEffect(ERogue10mTownTrait Trait, float Bonus, bool bNext) const
{
	using namespace Rogue10mTraitsUI;
	const URogue10mAttributeSet* Attributes = BoundPlayerState.IsValid() ? BoundPlayerState->GetRogueAttributeSet() : nullptr;
	const float Increment = bNext && TownComponent.IsValid()
		? TownComponent->GetUpgradeDefinition(Trait, GetServiceData()).ValuePerRank : 0.0f;
	FString Label;
	float Value = Bonus;
	FString Unit;
	switch (Trait)
	{
	case ERogue10mTownTrait::Vitality: Label = TEXT("최대 체력"); Value = Attributes ? Attributes->GetMaxHealth() + Increment : Bonus; break;
	case ERogue10mTownTrait::Power: Label = TEXT("공격력"); Value = Attributes ? Attributes->GetAttackPower() + Increment : Bonus; break;
	case ERogue10mTownTrait::Defense: Label = TEXT("방어력"); Value = Attributes ? Attributes->GetDefense() + Increment : Bonus; break;
	case ERogue10mTownTrait::CriticalChance:
		Label = TEXT("치명타 확률"); Value = Attributes ? FMath::Clamp(Attributes->GetCriticalChance() + Increment, 0.0f, 1.0f) * 100.0f : Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::CriticalDamage: Label = TEXT("치명타 피해"); Value = Attributes ? FMath::Clamp(Attributes->GetCriticalDamageMultiplier() + Increment, 0.0f, 10.0f) : Bonus; Unit = TEXT("배"); break;
	case ERogue10mTownTrait::MaxStamina: Label = TEXT("최대 스태미나"); Value = Attributes ? Attributes->GetMaxStamina() + Increment : Bonus; break;
	case ERogue10mTownTrait::KillRecovery: Label = TEXT("처치 시 체력 회복"); break;
	case ERogue10mTownTrait::StaminaRecovery: Label = TEXT("추가 스태미나 재생"); Unit = TEXT("/초"); break;
	case ERogue10mTownTrait::MoveSpeed: Label = TEXT("이동/질주 추가 속도"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::Combo: Label = TEXT("연격 발동 추가 피해"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::DodgeEfficiency: Label = TEXT("회피 소모 감소"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::DodgeCounter: Label = TEXT("첫 공격 추가 피해"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::GoldFind: Label = TEXT("몬스터 추가 금화"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::PotionMastery: Label = TEXT("물약 회복량 증가"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::BossHunter: Label = TEXT("보스 추가 피해"); Value = Bonus * 100.0f; Unit = TEXT("%"); break;
	case ERogue10mTownTrait::LastStand: Label = TEXT("치명상 생존 회복"); Value = Bonus * 100.0f; Unit = TEXT("% · 탐험당 1회"); break;
	default: Label = TEXT("특성 효과"); break;
	}
	return FText::Format(FText::FromString(TEXT("{0}  {1}{2}")), FText::FromString(Label), Number(Value), FText::FromString(Unit));
}

void URogue10mTraitWidget::RefreshDetail()
{
	if (!TownComponent.IsValid() || !DetailName) { return; }
	const FRogue10mTraitDefinition Definition = TownComponent->GetUpgradeDefinition(SelectedTrait, GetServiceData());
	const int32 Rank = TownComponent->GetTraitLevel(SelectedTrait);
	const bool bMax = Rank >= Definition.MaxRank;
	const float Bonus = TownComponent->GetTraitBonus(SelectedTrait);
	DetailName->SetText(Definition.DisplayName);
	DetailCategory->SetText(FText::Format(FText::FromString(TEXT("{0}  ·  영구 특성")), Rogue10mTraitsUI::CategoryName(static_cast<int32>(Definition.Category))));
	DetailDescription->SetText(Definition.Description);
	DetailRank->SetText(FText::Format(FText::FromString(bMax ? TEXT("강화 단계  {0}/{1}  ·  최대 단계") : TEXT("강화 단계  {0} → {2}  / {1}")),
		FText::AsNumber(Rank), FText::AsNumber(Definition.MaxRank), FText::AsNumber(FMath::Min(Rank + 1, Definition.MaxRank))));
	CurrentEffect->SetText(FText::Format(FText::FromString(TEXT("현재  {0}")), FormatEffect(SelectedTrait, Bonus, false)));
	NextEffect->SetText(bMax ? FText::FromString(TEXT("다음  최대 단계입니다."))
		: FText::Format(FText::FromString(TEXT("다음  {0}")), FormatEffect(SelectedTrait, Bonus + Definition.ValuePerRank, true)));
	FText Reason;
	bool bAllowed = CanInteract(Reason);
	if (bAllowed) { bAllowed = TownComponent->CanUpgradeTrait(SelectedTrait, Reason, GetServiceData()); }
	FString Conditions = FString::Printf(TEXT("단계당 %d포인트 · 최대 %d단계\nLV %d부터 투자 가능"), Definition.PointCost, Definition.MaxRank, Definition.UnlockLevel);
	if (Definition.DurationSeconds > 0) { Conditions += FString::Printf(TEXT("\n효과 조건 시간 %g초"), Definition.DurationSeconds); }
	if (Definition.CooldownSeconds > 0) { Conditions += FString::Printf(TEXT("\n재사용 시간 %g초"), Definition.CooldownSeconds); }
	if (!bAllowed && !Reason.IsEmpty()) { Conditions += TEXT("\n") + Reason.ToString(); }
	DetailCondition->SetText(FText::FromString(Conditions));
	DetailCondition->SetColorAndOpacity(FSlateColor(bAllowed ? Rogue10mTraitsUI::Muted : FLinearColor(0.95f, 0.67f, 0.35f)));
	CostText->SetText(FText::Format(FText::FromString(TEXT("필요 포인트  {0}")), FText::AsNumber(bMax ? 0 : Definition.PointCost)));
	const int32 Available = TownComponent->GetAvailableTraitPoints();
	RemainingText->SetText(!bMax && Available < Definition.PointCost
		? FText::Format(FText::FromString(TEXT("포인트 부족  {0} 추가 필요")), FText::AsNumber(Definition.PointCost - Available))
		: FText::Format(FText::FromString(TEXT("강화 후 잔여  {0}")), FText::AsNumber(bMax ? Available : Available - Definition.PointCost)));
	UpgradeButton->SetIsEnabled(bAllowed && !bResetConfirmation);
	SetIconBrush(DetailIcon, SelectedTrait, 88);
}

void URogue10mTraitWidget::HandleProgressionChanged() { RefreshFromData(); }

void URogue10mTraitWidget::SelectTrait(ERogue10mTownTrait Trait)
{
	if (bResetConfirmation || !VisibleTraits.Contains(Trait)) { return; }
	SelectedTrait = Trait;
	RefreshFromData();
	if (CardScroll)
	{
		for (const FRogue10mTraitCardView& Card : Cards)
		{
			if (Card.Trait == SelectedTrait)
			{
				CardScroll->ScrollWidgetIntoView(Card.Frame,true,EDescendantScrollDestination::IntoView,8.0f);
				break;
			}
		}
	}
}

void URogue10mTraitWidget::SetCategoryFilter(int32 Category)
{
	if (bResetConfirmation) { return; }
	CategoryFilter = FMath::Clamp(Category, -1, 3);
	RebuildCards();
	RefreshFromData();
}

void URogue10mTraitWidget::SetMessage(const FText& Message, bool bError)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(0.97f, 0.54f, 0.35f) : Rogue10mTraitsUI::Muted));
	}
}

void URogue10mTraitWidget::RefreshSaveMessage(const FText& Success)
{
	const UGameInstance* Instance = GetGameInstance();
	URogue10mProgressionSubsystem* Save = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	if (Save && Save->GetAttachedProfileId().IsValid())
	{
		if (!Save->SaveNow())
		{
			SetMessage(FText::Format(FText::FromString(TEXT("현재 세션에는 반영되었으나 저장하지 못했습니다. 저장 재시도가 필요합니다. {0}")), Save->GetLastError()), true);
			return;
		}
		SetMessage(FText::Format(FText::FromString(TEXT("{0}  ·  캐릭터별 저장 완료")), Success), false);
		return;
	}
	SetMessage(FText::Format(FText::FromString(TEXT("{0}  ·  선택 캐릭터가 없는 테스트 세션입니다.")), Success), false);
}

void URogue10mTraitWidget::RequestUpgrade()
{
	if (bResetConfirmation) { return; }
	FText Reason;
	if (!CanInteract(Reason) || !TownComponent->CanUpgradeTrait(SelectedTrait, Reason, GetServiceData()))
	{
		SetMessage(Reason, true);
		RefreshFromData();
		return;
	}
	if (!ServiceNPC->UpgradeTrait(SelectedTrait))
	{
		SetMessage(ServiceNPC->GetDialogueStatus(), true);
		RefreshFromData();
		return;
	}
	RefreshSaveMessage(FText::FromString(TEXT("특성 강화 완료")));
	RefreshFromData();
}

void URogue10mTraitWidget::RequestReset()
{
	FText Reason;
	if (!CanInteract(Reason) || !TownComponent->CanChangeTraits() || TownComponent->GetSpentTraitPoints() <= 0)
	{
		SetMessage(Reason.IsEmpty() ? FText::FromString(TEXT("돌려받을 특성 포인트가 없습니다.")) : Reason, true);
		return;
	}
	bResetConfirmation = true;
	MainContent->SetIsEnabled(false);
	ResetOverlay->SetVisibility(ESlateVisibility::Visible);
	RefreshFromData();
}

void URogue10mTraitWidget::ConfirmReset()
{
	if (!bResetConfirmation) { return; }
	FText Reason;
	if (!CanInteract(Reason) || !ServiceNPC->ResetTraits())
	{
		CancelReset();
		SetMessage(Reason.IsEmpty() && ServiceNPC.IsValid() ? ServiceNPC->GetDialogueStatus() : Reason, true);
		RefreshFromData();
		return;
	}
	CancelReset();
	RefreshSaveMessage(FText::FromString(TEXT("특성 재분배 완료")));
	RefreshFromData();
}

void URogue10mTraitWidget::CancelReset()
{
	bResetConfirmation = false;
	if (ResetOverlay) { ResetOverlay->SetVisibility(ESlateVisibility::Collapsed); }
	if (MainContent) { MainContent->SetIsEnabled(true); }
	RefreshFromData();
}

void URogue10mTraitWidget::RequestClose()
{
	if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(GetOwningPlayer())) { Controller->CloseTraitWindow(); }
}

void URogue10mTraitWidget::RetrySave()
{
	if (bResetConfirmation) { return; }
	FText Reason;
	if (!CanInteract(Reason)) { SetMessage(Reason, true); return; }
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mProgressionSubsystem* Save = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	if (!Save || !Save->CanRetrySave())
	{
		SetMessage(FText::FromString(TEXT("이 캐릭터의 현재 세션을 안전하게 다시 저장할 수 없습니다.")), true);
		return;
	}
	RefreshSaveMessage(FText::FromString(TEXT("저장 재시도 완료")));
	RefreshFromData();
}

void URogue10mTraitWidget::HandleAction(int32 Action, ERogue10mTownTrait Trait)
{
	if (Action == 0) { SelectTrait(Trait); }
	else if (Action >= 10 && Action <= 14) { SetCategoryFilter(Action - 11); }
	else if (Action == 20) { RequestUpgrade(); }
	else if (Action == 21) { RequestReset(); }
	else if (Action == 22) { ConfirmReset(); }
	else if (Action == 23) { CancelReset(); }
	else if (Action == 24) { RequestClose(); }
	else if (Action == 25) { RetrySave(); }
	if (IsInViewport() && GetOwningPlayer()) { SetUserFocus(GetOwningPlayer()); }
}

void URogue10mTraitWidget::MoveSelection(int32 Horizontal, int32 Vertical)
{
	if (VisibleTraits.Num() == 0 || bResetConfirmation) { return; }
	const int32 Index = FMath::Max(0, VisibleTraits.IndexOfByKey(SelectedTrait));
	const int32 Columns = CategoryFilter < 0 ? 4 : 2;
	const int32 Rows = CategoryFilter < 0 ? 4 : FMath::DivideAndRoundUp(VisibleTraits.Num(), Columns);
	const int32 Column = CategoryFilter < 0 ? Index / Rows : Index % Columns;
	const int32 Row = CategoryFilter < 0 ? Index % Rows : Index / Columns;
	const int32 NextColumn = FMath::Clamp(Column + Horizontal, 0, Columns - 1);
	const int32 NextRow = FMath::Clamp(Row + Vertical, 0, Rows - 1);
	const int32 NextIndex = CategoryFilter < 0 ? NextColumn * Rows + NextRow : NextRow * Columns + NextColumn;
	SelectTrait(VisibleTraits[FMath::Clamp(NextIndex, 0, VisibleTraits.Num() - 1)]);
}

FReply URogue10mTraitWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		if (bResetConfirmation) { CancelReset(); } else { RequestClose(); }
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		if (!KeyEvent.IsRepeat()) { if (bResetConfirmation) { ConfirmReset(); } else { RequestUpgrade(); } }
		return FReply::Handled();
	}
	if (bResetConfirmation) { return FReply::Handled(); }
	if (Key == EKeys::Left || Key == EKeys::A) { MoveSelection(-1, 0); return FReply::Handled(); }
	if (Key == EKeys::Right || Key == EKeys::D) { MoveSelection(1, 0); return FReply::Handled(); }
	if (Key == EKeys::Up || Key == EKeys::W) { MoveSelection(0, -1); return FReply::Handled(); }
	if (Key == EKeys::Down || Key == EKeys::S) { MoveSelection(0, 1); return FReply::Handled(); }
	if (Key == EKeys::R) { if (!KeyEvent.IsRepeat()) { RequestReset(); } return FReply::Handled(); }
	if (Key == EKeys::Tab) { SetCategoryFilter((CategoryFilter + 2) % 5 - 1); return FReply::Handled(); }
	if (Key == EKeys::One) { SetCategoryFilter(-1); return FReply::Handled(); }
	if (Key == EKeys::Two) { SetCategoryFilter(0); return FReply::Handled(); }
	if (Key == EKeys::Three) { SetCategoryFilter(1); return FReply::Handled(); }
	if (Key == EKeys::Four) { SetCategoryFilter(2); return FReply::Handled(); }
	if (Key == EKeys::Five) { SetCategoryFilter(3); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

int32 URogue10mTraitWidget::GetDisplayedAvailablePoints() const { return TownComponent.IsValid() ? TownComponent->GetAvailableTraitPoints() : 0; }
FText URogue10mTraitWidget::GetDisplayedCurrentEffect() const { return CurrentEffect ? CurrentEffect->GetText() : FText::GetEmpty(); }
FText URogue10mTraitWidget::GetDisplayedNextEffect() const { return NextEffect ? NextEffect->GetText() : FText::GetEmpty(); }
FText URogue10mTraitWidget::GetDisplayedStatus() const { return StatusText ? StatusText->GetText() : FText::GetEmpty(); }
