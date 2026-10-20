#include "Rogue10mCombatTestWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

URogue10mCombatTestButton::URogue10mCombatTestButton() { InitIsFocusable(false); }
void URogue10mCombatTestButton::Configure(URogue10mCombatTestComponent* InTools, ERogue10mCombatTestAction InAction)
{
	Tools = InTools; Action = InAction;
	OnClicked.AddUniqueDynamic(this, &ThisClass::Clicked);
}
void URogue10mCombatTestButton::Clicked() { if (Tools.IsValid()) Tools->ExecuteAction(Action); }

void URogue10mCombatTestWidget::InitializeTools(URogue10mCombatTestComponent* InTools)
{
	Tools = InTools;
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildTree();
}

TSharedRef<SWidget> URogue10mCombatTestWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

URogue10mCombatTestButton* URogue10mCombatTestWidget::AddAction(UPanelWidget* Parent,
	const FName Name, const TCHAR* Label, ERogue10mCombatTestAction Action)
{
	auto* Button = WidgetTree->ConstructWidget<URogue10mCombatTestButton>(URogue10mCombatTestButton::StaticClass(), Name);
	Button->Configure(Tools.Get(), Action);
	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(FLinearColor(0.12f,0.19f,0.22f,1));
	Style.Hovered.TintColor = FSlateColor(FLinearColor(0.22f,0.38f,0.40f,1));
	Style.Pressed.TintColor = FSlateColor(FLinearColor(0.10f,0.30f,0.34f,1));
	Style.NormalPadding = FMargin(10,8); Style.PressedPadding = FMargin(10,8);
	Button->SetStyle(Style);
	auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(Label));
	FSlateFontInfo Font = Text->GetFont(); Font.Size = 16; Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f,0.96f,0.95f,1)));
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	if (auto* Row = Cast<UHorizontalBox>(Parent))
	{
		auto* ButtonSlot = Row->AddChildToHorizontalBox(Button);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ButtonSlot->SetPadding(FMargin(3,3));
	}
	else if (auto* Rows = Cast<UVerticalBox>(Parent)) Rows->AddChildToVerticalBox(Button)->SetPadding(FMargin(3,3));
	else Parent->AddChild(Button);
	return Button;
}

void URogue10mCombatTestWidget::BuildTree()
{
	if (Panel || !WidgetTree || !Tools.IsValid()) return;
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatTestCanvas"));
	WidgetTree->RootWidget = Canvas;
	Hint = AddAction(Canvas, TEXT("CombatTestHint"), TEXT("F6  몬스터 테스트"), ERogue10mCombatTestAction::TogglePanel);
	auto* HintSlot = CastChecked<UCanvasPanelSlot>(Hint->Slot);
	HintSlot->SetAnchors(FAnchors(1,0)); HintSlot->SetAlignment(FVector2D(1,0));
	HintSlot->SetPosition(FVector2D(-20,20)); HintSlot->SetAutoSize(true);
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("CombatTestScale"));
	Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
	auto* ScaleSlot = Canvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0.48f,0.06f,0.98f,0.96f)); ScaleSlot->SetOffsets(FMargin(0));
	Panel = Scale;
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(500);
	Scale->AddChild(Size);
	CastChecked<UScaleBoxSlot>(Size->Slot)->SetVerticalAlignment(VAlign_Center);
	auto* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CombatTestCard"));
	Border->SetBrushColor(FLinearColor(0.018f,0.026f,0.036f,0.98f)); Border->SetPadding(FMargin(18,14)); Size->AddChild(Border);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Border->AddChild(Rows);
	auto Text = [this, Rows](const FName Name, const TCHAR* Label, int32 FontSize, FLinearColor Color)
	{
		auto* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		LabelWidget->SetText(FText::FromString(Label)); FSlateFontInfo Font = LabelWidget->GetFont();
		Font.Size = FontSize; LabelWidget->SetFont(Font); LabelWidget->SetColorAndOpacity(FSlateColor(Color));
		LabelWidget->SetAutoWrapText(true); LabelWidget->SetWrapTextAt(460);
		Rows->AddChildToVerticalBox(LabelWidget)->SetPadding(FMargin(3,4)); return LabelWidget;
	};
	Text(TEXT("CombatTestTitle"), TEXT("몬스터 테스트"), 25, FLinearColor(0.95f,0.82f,0.53f,1));
	Text(TEXT("CombatTestDescription"), TEXT("현재 위치에서 바로 시험 · 창을 열면 일시정지\n공격/자동 전투를 누르면 창이 닫히고 시작됩니다."), 14, FLinearColor(0.67f,0.78f,0.82f,1));
	CreaturePicker = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("CombatTestCreature"));
	for (const auto& Creature : Tools->GetCreatures()) CreaturePicker->AddOption(Creature.Name.ToString());
	CreaturePicker->SetSelectedIndex(FMath::Max(0, Tools->GetCreatures().Num()-1));
	CreaturePicker->OnSelectionChanged.AddDynamic(this, &ThisClass::CreatureSelected);
	Rows->AddChildToVerticalBox(CreaturePicker)->SetPadding(FMargin(3,6));
	AddAction(Rows, TEXT("CombatTestSpawn"), TEXT("선택한 몬스터를 내 앞에 소환"), ERogue10mCombatTestAction::Spawn);
	TargetText = Text(TEXT("CombatTestTarget"), TEXT(""), 15, FLinearColor(0.82f,0.91f,0.91f,1));
	auto Row = [this, Rows]() { auto* R = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(R); return R; };
	auto* Patterns = Row();
	TargetButtons.Add(AddAction(Patterns,TEXT("CombatTestBasic"),TEXT("기본 공격"),ERogue10mCombatTestAction::Basic));
	BossButtons.Add(AddAction(Patterns,TEXT("CombatTestWave"),TEXT("정면 파동"),ERogue10mCombatTestAction::FrontalWave));
	BossButtons.Add(AddAction(Patterns,TEXT("CombatTestZone"),TEXT("심연 영역"),ERogue10mCombatTestAction::AbyssZone));
	auto* BossRow = Row();
	BossButtons.Add(AddAction(BossRow,TEXT("CombatTestEnrage"),TEXT("보스 HP40% · 강화"),ERogue10mCombatTestAction::Enrage));
	TargetButtons.Add(AddAction(BossRow,TEXT("CombatTestReset"),TEXT("대상 초기화"),ERogue10mCombatTestAction::Reset));
	TargetButtons.Add(AddAction(Rows,TEXT("CombatTestAI"),TEXT("자동 AI 전투 시작 / 정지"),ERogue10mCombatTestAction::ToggleAI));
	auto* UtilityRow = Row();
	AddAction(UtilityRow,TEXT("CombatTestHeal"),TEXT("플레이어 회복"),ERogue10mCombatTestAction::Heal);
	ImmunityButton = AddAction(UtilityRow,TEXT("CombatTestImmunity"),TEXT("무적: 꺼짐"),ERogue10mCombatTestAction::Invulnerable);
	AddAction(Rows,TEXT("CombatTestClear"),TEXT("소환한 테스트 몬스터 모두 정리"),ERogue10mCombatTestAction::Clear);
	StatusText = Text(TEXT("CombatTestStatus"),TEXT(""),15,FLinearColor(0.50f,0.86f,0.73f,1));
	Text(TEXT("CombatTestRewardHint"),TEXT("테스트 소환: 경험치·금화·토벌 보상 없음\n스킬 사용 수련은 정상 적용 · 일반 몬스터는 유지"),12,FLinearColor(0.62f,0.68f,0.74f,1));
	AddAction(Rows,TEXT("CombatTestClose"),TEXT("닫기  ·  F6 / ESC"),ERogue10mCombatTestAction::TogglePanel);
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void URogue10mCombatTestWidget::CreatureSelected(FString Selection, ESelectInfo::Type SelectionType)
{
	if (Tools.IsValid()) Tools->SelectCreature(CreaturePicker->FindOptionIndex(Selection));
}

void URogue10mCombatTestWidget::RefreshView(bool bOpen, const FText& Status,
	const FText& TargetSummary, bool bHasTarget, bool bBoss, bool bImmune)
{
	BuildTree();
	if (!Panel) return;
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Hint->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	StatusText->SetText(Status); TargetText->SetText(TargetSummary);
	CastChecked<UTextBlock>(ImmunityButton->GetContent())->SetText(FText::FromString(bImmune ? TEXT("무적: 켜짐") : TEXT("무적: 꺼짐")));
	CastChecked<UTextBlock>(CastChecked<UButton>(Hint)->GetContent())->SetText(FText::FromString(
		bImmune ? TEXT("F6  테스트 · 무적 켜짐") : TEXT("F6  몬스터 테스트")));
	for (const auto& Button : TargetButtons) Button->SetIsEnabled(bHasTarget);
	for (const auto& Button : BossButtons) Button->SetIsEnabled(bBoss);
}

FReply URogue10mCombatTestWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Tools.IsValid() && Tools->IsPanelOpen() && (Event.GetKey() == EKeys::F6 || Event.GetKey() == EKeys::Escape))
	{
		Tools->SetPanelOpen(false); return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
