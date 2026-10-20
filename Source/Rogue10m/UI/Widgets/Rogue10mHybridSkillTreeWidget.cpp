#include "Widgets/Rogue10mHybridSkillTreeWidget.h"

#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InputKeySelector.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Rogue10mSkillDragDropOperation.h"

namespace Rogue10mHybridTreeUI
{
	const FLinearColor Iron(0.012f, 0.018f, 0.022f, 0.99f);
	const FLinearColor Raised(0.032f, 0.043f, 0.046f, 1.0f);
	const FLinearColor Gold(0.86f, 0.62f, 0.24f, 1.0f);
	const FLinearColor Pale(0.94f, 0.90f, 0.79f, 1.0f);
	const FLinearColor Muted(0.59f, 0.65f, 0.64f, 1.0f);
	const FLinearColor Green(0.23f, 0.85f, 0.57f, 1.0f);
	const FLinearColor Red(0.96f, 0.32f, 0.24f, 1.0f);
	const FLinearColor Blue(0.30f, 0.60f, 0.98f, 1.0f);
	const FVector2D GraphSize(1040.0f, 790.0f);
	const FVector2D Center(520.0f, 390.0f);
	enum EAction { Fighter = 1, GreatSword, LearnPoints, LearnMastery, Activate, Choose,
		Refund, CancelRefund, ClearKey, Close, SelectNode = 100 };

	FText TrackName(ERogue10mSkillTreeTrack Track)
	{
		return FText::FromString(Track == ERogue10mSkillTreeTrack::GreatSword ? TEXT("대검술사 · 대검") : TEXT("격투가 · 맨손 / 너클"));
	}
	FText IdentityName(ERogue10mSkillTreeIdentity Identity)
	{
		switch (Identity)
		{
		case ERogue10mSkillTreeIdentity::InternalEnergy: return FText::FromString(TEXT("내공"));
		case ERogue10mSkillTreeIdentity::FightingSpirit: return FText::FromString(TEXT("투지"));
		case ERogue10mSkillTreeIdentity::Mana: return FText::FromString(TEXT("마나"));
		default: return FText::FromString(TEXT("미활성"));
		}
	}
	FLinearColor IdentityColor(ERogue10mSkillTreeIdentity Identity)
	{
		return Identity == ERogue10mSkillTreeIdentity::InternalEnergy ? Green
			: Identity == ERogue10mSkillTreeIdentity::FightingSpirit ? Red
			: Identity == ERogue10mSkillTreeIdentity::Mana ? Blue : Gold;
	}
	FLinearColor NodeColor(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		return Node.Identity == ERogue10mSkillTreeIdentity::None ? Gold : IdentityColor(Node.Identity);
	}
	float Diameter(ERogue10mHybridSkillTreeNodeType Type)
	{
		switch (Type)
		{
		case ERogue10mHybridSkillTreeNodeType::Root: return 62.0f;
		case ERogue10mHybridSkillTreeNodeType::Basic: return 54.0f;
		case ERogue10mHybridSkillTreeNodeType::Stat: return 26.0f;
		case ERogue10mHybridSkillTreeNodeType::Resonance: return 40.0f;
		default: return 44.0f;
		}
	}
	FText StateName(ERogue10mSkillTreeNodeState State)
	{
		switch (State)
		{
		case ERogue10mSkillTreeNodeState::Active: return FText::FromString(TEXT("활성 중"));
		case ERogue10mSkillTreeNodeState::Learned: return FText::FromString(TEXT("전수 완료"));
		case ERogue10mSkillTreeNodeState::Available: return FText::FromString(TEXT("조건 충족"));
		default: return FText::FromString(TEXT("잠김"));
		}
	}
	FText ShortStateName(ERogue10mSkillTreeNodeState State)
	{
		return FText::FromString(State == ERogue10mSkillTreeNodeState::Active ? TEXT("활성")
			: State == ERogue10mSkillTreeNodeState::Learned ? TEXT("전수")
			: State == ERogue10mSkillTreeNodeState::Available ? TEXT("가능") : TEXT("잠김"));
	}
	bool CanRefundType(ERogue10mHybridSkillTreeNodeType Type)
	{
		return Type == ERogue10mHybridSkillTreeNodeType::IdentityEntry || Type == ERogue10mHybridSkillTreeNodeType::IdentityAdvanced || Type == ERogue10mHybridSkillTreeNodeType::IdentityChoice;
	}
	FText CostText(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance) return FText::FromString(TEXT("공명 · 0P"));
		if (Node.PointCost > 0 && !Node.MasteryAlternatives.IsEmpty()) return FText::FromString(FString::Printf(TEXT("%dP 또는 수련"), Node.PointCost));
		if (Node.PointCost > 0) return FText::FromString(FString::Printf(TEXT("%dP"), Node.PointCost));
		if (!Node.MasteryAlternatives.IsEmpty()) return FText::FromString(FString::Printf(TEXT("M%d · 0P"), Node.MasteryAlternatives[0].Level));
		return FText::FromString(TEXT("무료 전수"));
	}
	FText GraphCaption(const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeNodeState State)
	{
		const FString Status = ShortStateName(State).ToString();
		if (Node.PointCost > 0) return FText::FromString(Node.MasteryAlternatives.IsEmpty()
			? FString::Printf(TEXT("%dP · %s"), Node.PointCost, *Status)
			: FString::Printf(TEXT("%dP / 수련 · %s"), Node.PointCost, *Status));
		if (!Node.MasteryAlternatives.IsEmpty()) return FText::FromString(FString::Printf(TEXT("M%d · %s"), Node.MasteryAlternatives[0].Level, *Status));
		return ShortStateName(State);
	}
	FButtonStyle ButtonStyle(FLinearColor Accent, bool bSelected = false, bool bLearned = false, float Radius = 5.0f)
	{
		FButtonStyle Style;
		const FLinearColor Fill = bSelected ? FLinearColor(0.10f, 0.13f, 0.13f) : (bLearned ? Raised : Iron);
		Style.Normal = FSlateRoundedBoxBrush(Fill, Radius, bSelected ? Pale : Accent, bSelected ? 2.5f : 1.3f);
		Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.11f, 0.14f, 0.14f), Radius, Pale, 2.0f);
		Style.Pressed = FSlateRoundedBoxBrush(FLinearColor(0.16f, 0.18f, 0.15f), Radius, Accent, 2.5f);
		Style.Disabled = FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.030f, 0.033f), Radius,
			FLinearColor(0.18f, 0.22f, 0.22f), 1.0f);
		Style.NormalPadding = Radius > 10.0f ? FMargin(3.0f) : FMargin(10.0f, 7.0f);
		Style.PressedPadding = Style.NormalPadding;
		return Style;
	}
	void SetTextStyle(UTextBlock* Text, int32 Size, FLinearColor Color)
	{
		FSlateFontInfo Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	FVector2D Position(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		return Center + Node.Position;
	}
	FText RewardText(const FRogue10mEquipmentStatModifiers& Stats)
	{
		TArray<FString> First, Second, Lines;
		if (!FMath::IsNearlyZero(Stats.AttackPowerBonus)) First.Add(FString::Printf(TEXT("공격력 +%.0f"), Stats.AttackPowerBonus));
		if (!FMath::IsNearlyZero(Stats.MaxStaminaBonus)) First.Add(FString::Printf(TEXT("스태미나 +%.0f"), Stats.MaxStaminaBonus));
		if (!FMath::IsNearlyZero(Stats.MaxHealthBonus)) Second.Add(FString::Printf(TEXT("HP +%.0f"), Stats.MaxHealthBonus));
		if (!FMath::IsNearlyZero(Stats.DefenseBonus)) Second.Add(FString::Printf(TEXT("방어 +%.0f"), Stats.DefenseBonus));
		if (!FMath::IsNearlyZero(Stats.AttackSpeedBonus)) Second.Add(FString::Printf(TEXT("공속 +%.0f%%"), Stats.AttackSpeedBonus * 100.0f));
		if (!First.IsEmpty()) Lines.Add(FString::Join(First, TEXT(" · ")));
		if (!Second.IsEmpty()) Lines.Add(FString::Join(Second, TEXT(" · ")));
		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}
}

void URogue10mHybridSkillTreeButton::Configure(URogue10mHybridSkillTreeWidget* InOwner, int32 InAction, FName InNodeId, FName InChoiceId)
{
	OwnerWidget = InOwner; Action = InAction; NodeId = InNodeId; ChoiceId = InChoiceId;
	OnClicked.AddUniqueDynamic(this, &ThisClass::Clicked);
}
void URogue10mHybridSkillTreeButton::Clicked()
{
	if (OwnerWidget.IsValid()) OwnerWidget->HandleAction(Action, NodeId, ChoiceId);
}

TSharedRef<SWidget> URogue10mHybridSkillTreeEntryWidget::RebuildWidget()
{
	BuildEntry();
	return Super::RebuildWidget();
}
void URogue10mHybridSkillTreeEntryWidget::BuildEntry()
{
	if (NodeButton || !WidgetTree) return;
	NodeButton = WidgetTree->ConstructWidget<URogue10mHybridSkillTreeButton>();
	WidgetTree->RootWidget = NodeButton;
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	NodeButton->AddChild(Overlay);
	Icon = WidgetTree->ConstructWidget<UImage>();
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	Overlay->AddChildToOverlay(Icon)->SetPadding(FMargin(2));
	Symbol = WidgetTree->ConstructWidget<UTextBlock>();
	Rogue10mHybridTreeUI::SetTextStyle(Symbol, 17, Rogue10mHybridTreeUI::Pale);
	Symbol->SetJustification(ETextJustify::Center);
	UOverlaySlot* SymbolSlot = Overlay->AddChildToOverlay(Symbol);
	SymbolSlot->SetHorizontalAlignment(HAlign_Center); SymbolSlot->SetVerticalAlignment(VAlign_Center);
}
void URogue10mHybridSkillTreeEntryWidget::Configure(URogue10mHybridSkillTreeWidget* Owner,
	const FRogue10mSkillTreeNodeDefinition& Definition, ERogue10mSkillTreeNodeState State, bool bSelected, bool bCurrentTrack)
{
	using namespace Rogue10mHybridTreeUI;
	BuildEntry(); OwnerWidget = Owner; NodeId = Definition.NodeId;
	Skill = Definition.Skill.LoadSynchronous();
	if (!Skill && !Definition.GrantedSkills.IsEmpty()) Skill = Definition.GrantedSkills[0].LoadSynchronous();
	if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic && Owner && Owner->GetSelectedNodeId() == NodeId)
	{
		Skill = Owner->GetSelectedAttackSkill();
	}
	const bool bLearned = State == ERogue10mSkillTreeNodeState::Learned || State == ERogue10mSkillTreeNodeState::Active;
	bCanDrag = bCurrentTrack && bLearned && Skill
		&& (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Technique || Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic)
		&& Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique;
	NodeButton->Configure(Owner, SelectNode, NodeId);
	FLinearColor Accent = NodeColor(Definition);
	if (State == ERogue10mSkillTreeNodeState::Locked) Accent = Muted * 0.55f;
	NodeButton->SetStyle(ButtonStyle(Accent, bSelected, bLearned, Diameter(Definition.NodeType) * 0.5f));
	NodeButton->SetToolTipText(FText::Format(FText::FromString(TEXT("{0}\n{1} · {2}\n{3}")),
		Definition.DisplayName, StateName(State), CostText(Definition), Definition.Description));
	const bool bHasIcon = Skill && Skill->SkillIcon && Definition.NodeType != ERogue10mHybridSkillTreeNodeType::Basic;
	Icon->SetBrushFromTexture(bHasIcon ? Skill->SkillIcon : nullptr);
	Icon->SetColorAndOpacity(bLearned ? Pale : Muted);
	Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	FString Glyph = TEXT("+");
	if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Root) Glyph = TEXT("시작");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic) Glyph = TEXT("기본");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance) Glyph = TEXT("공명");
	else if (Definition.Identity != ERogue10mSkillTreeIdentity::None) Glyph = IdentityName(Definition.Identity).ToString();
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Passive) Glyph = TEXT("심법");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Technique) Glyph = Definition.Track == ERogue10mSkillTreeTrack::GreatSword ? TEXT("검") : TEXT("권");
	Symbol->SetText(FText::FromString(Glyph));
	Symbol->SetVisibility(bHasIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SetTextStyle(Symbol, Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Stat ? 17 : 14, Accent);
}
FReply URogue10mHybridSkillTreeEntryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bCanDrag && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (OwnerWidget.IsValid()) OwnerWidget->HandleAction(Rogue10mHybridTreeUI::SelectNode, NodeId);
		return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}
void URogue10mHybridSkillTreeEntryWidget::NativeOnDragDetected(const FGeometry& Geometry,
	const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	if (!bCanDrag || !Skill) return;
	URogue10mSkillDragDropOperation* Drag = NewObject<URogue10mSkillDragDropOperation>(this);
	Drag->SkillData = Skill; Drag->bUnlocked = true; Drag->Pivot = EDragPivot::MouseDown;
	Operation = Drag;
}

TSharedRef<SWidget> URogue10mHybridSkillTreeGraphWidget::RebuildWidget()
{
	if (!Canvas && WidgetTree)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}
void URogue10mHybridSkillTreeGraphWidget::SetData(URogue10mHybridSkillTreeWidget* Owner,
	URogue10mSkillTreeProgressionComponent* InProgression, ERogue10mSkillTreeTrack InTrack, FName SelectedNode)
{
	using namespace Rogue10mHybridTreeUI;
	OwnerWidget = Owner; Progression = InProgression; Track = InTrack; Selected = SelectedNode;
	const TArray<FRogue10mSkillTreeNodeDefinition> NewNodes = InProgression ? InProgression->GetNodes(Track) : TArray<FRogue10mSkillTreeNodeDefinition>();
	bool bRebuild = Entries.Num() != NewNodes.Num();
	if (!bRebuild)
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].NodeId != NewNodes[Index].NodeId || !Nodes[Index].Position.Equals(NewNodes[Index].Position)
				|| Nodes[Index].NodeType != NewNodes[Index].NodeType) { bRebuild = true; break; }
		}
	}
	Nodes = NewNodes;
	if (!Canvas) { TakeWidget(); bRebuild = true; }
	if (bRebuild) BuildGraph();
	for (int32 Index = 0; Index < Entries.Num() && Index < Nodes.Num(); ++Index)
	{
		if (!Entries[Index]) continue;
		const auto State = InProgression->GetNodeState(Nodes[Index].NodeId, Track);
		Entries[Index]->Configure(Owner, Nodes[Index], InProgression->GetNodeState(Nodes[Index].NodeId, Track),
			Selected == Nodes[Index].NodeId, InProgression->GetActiveTrack() == Track);
		if (Labels.IsValidIndex(Index) && Labels[Index])
		{
			Labels[Index]->SetText(FText::Format(FText::FromString(TEXT("{0}\n{1}")), Nodes[Index].DisplayName, GraphCaption(Nodes[Index], State)));
			Labels[Index]->SetColorAndOpacity(FSlateColor(State == ERogue10mSkillTreeNodeState::Locked ? Muted : Pale));
		}
	}
	InvalidateLayoutAndVolatility();
}
void URogue10mHybridSkillTreeGraphWidget::BuildGraph()
{
	using namespace Rogue10mHybridTreeUI;
	if (!Canvas || !WidgetTree) return;
	Canvas->ClearChildren(); Entries.Reset(); Labels.Reset(); Entries.SetNumZeroed(Nodes.Num()); Labels.SetNumZeroed(Nodes.Num());
	if (UTexture2D* Frame = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/MartialArts/T_UI_MartialArtsSkillTreeFrame.T_UI_MartialArtsSkillTreeFrame")))
	{
		UImage* Texture = WidgetTree->ConstructWidget<UImage>();
		FSlateBrush Brush; Brush.SetResourceObject(Frame); Brush.ImageSize = FVector2f(GraphSize);
		Brush.SetUVRegion(FBox2f(FVector2f(0.215f, 0.075f), FVector2f(0.785f, 0.935f)));
		Texture->SetBrush(Brush); Texture->SetRenderOpacity(0.38f); Texture->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* TextureSlot = Canvas->AddChildToCanvas(Texture); TextureSlot->SetPosition(FVector2D::ZeroVector); TextureSlot->SetSize(GraphSize); TextureSlot->SetZOrder(-1);
	}
	URogue10mHybridSkillTreeConnectionsWidget* Connections = CreateWidget<URogue10mHybridSkillTreeConnectionsWidget>(GetOwningPlayer());
	Connections->SetGraph(this); Connections->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* ConnectionsSlot = Canvas->AddChildToCanvas(Connections); ConnectionsSlot->SetPosition(FVector2D::ZeroVector); ConnectionsSlot->SetSize(GraphSize); ConnectionsSlot->SetZOrder(0);
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FRogue10mSkillTreeNodeDefinition& Node = Nodes[Index];
		URogue10mHybridSkillTreeEntryWidget* Entry = CreateWidget<URogue10mHybridSkillTreeEntryWidget>(GetOwningPlayer());
		if (!Entry) continue;
		Entries[Index] = Entry;
		const FVector2D Location = Position(Node);
		const float Size = Diameter(Node.NodeType);
		UCanvasPanelSlot* EntrySlot = Canvas->AddChildToCanvas(Entry);
		EntrySlot->SetPosition(Location); EntrySlot->SetAlignment(FVector2D(0.5f));
		EntrySlot->SetSize(FVector2D(Size)); EntrySlot->SetZOrder(3);
		if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Root || Node.NodeType == ERogue10mHybridSkillTreeNodeType::Basic) continue;
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Labels[Index] = Label;
		Label->SetText(FText::Format(FText::FromString(TEXT("{0}\n{1}")), Node.DisplayName, CostText(Node)));
		SetTextStyle(Label, 15, Pale); Label->SetWrapTextAt(144.0f);
		UCanvasPanelSlot* LabelSlot = Canvas->AddChildToCanvas(Label);
		const bool bResonance = Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance;
		// Three separate label corridors: outside each side branch, left of the central spine.
		const float LeftX = Location.X - Size * 0.5f - 153.0f;
		const float RightX = Location.X + Size * 0.5f + 9.0f;
		const bool bCentralSpine = FMath::IsNearlyZero(Node.Position.X);
		const bool bLeftBranch = Node.Position.Y < 0.0f && Node.Position.X < 0.0f;
		const bool bPlaceLeft = bCentralSpine || (bLeftBranch ? LeftX >= 8.0f : RightX + 144.0f > GraphSize.X - 8.0f);
		FVector2D LabelPosition(bPlaceLeft ? LeftX : RightX, FMath::Max(8.0f, Location.Y - 18.0f));
		Label->SetJustification(bPlaceLeft ? ETextJustify::Right : ETextJustify::Left);
		if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Stat && Node.Position.X < 0.0f && Node.Position.X > -120.0f)
		{
			LabelPosition = Location + FVector2D(-125.0f, 24.0f); Label->SetJustification(ETextJustify::Left);
		}
		if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Stat && Node.Position.X > 0.0f && Node.Position.X < 120.0f)
		{
			LabelPosition = Location + FVector2D(35.0f, 23.0f); Label->SetJustification(ETextJustify::Left);
		}
		if (bResonance) { LabelPosition = Location + FVector2D(-83.0f, 27.0f); Label->SetJustification(ETextJustify::Center); }
		LabelSlot->SetPosition(LabelPosition); LabelSlot->SetSize(FVector2D(bResonance ? 166.0f : 144.0f, 57.0f));
		LabelSlot->SetZOrder(4);
	}
}
void URogue10mHybridSkillTreeConnectionsWidget::SetGraph(URogue10mHybridSkillTreeGraphWidget* InGraph)
{
	Graph = InGraph;
}
int32 URogue10mHybridSkillTreeConnectionsWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
	return Graph.IsValid() ? Graph->PaintConnections(Geometry, Elements, BaseLayer) : BaseLayer;
}
int32 URogue10mHybridSkillTreeGraphWidget::PaintConnections(const FGeometry& Geometry,
	FSlateWindowElementList& Elements, int32 LayerId) const
{
	using namespace Rogue10mHybridTreeUI;
	for (float Radius : {95.0f, 165.0f, 235.0f, 305.0f, 370.0f})
	{
		TArray<FVector2f> Points;
		for (int32 Step = 0; Step <= 96; ++Step)
		{
			const float Angle = 2.0f * PI * static_cast<float>(Step) / 96.0f;
			Points.Add(FVector2f(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius));
		}
		FSlateDrawElement::MakeLines(Elements, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			FLinearColor(0.28f, 0.20f, 0.10f, 0.42f), true, 1.0f);
	}
	for (const FRogue10mSkillTreeNodeDefinition& Node : Nodes)
	{
		for (const FRogue10mSkillTreePrerequisiteGroup& Group : Node.PrerequisiteGroups)
		{
			for (FName ParentId : Group.NodeIds)
			{
				const FRogue10mSkillTreeNodeDefinition* Parent = Nodes.FindByPredicate([ParentId](const auto& Item) { return Item.NodeId == ParentId; });
				if (!Parent) continue;
				const FVector2D Start = Position(*Parent), End = Position(Node);
				const FVector2D Direction = (End - Start).GetSafeNormal();
				const FVector2D A = Start + Direction * (Diameter(Parent->NodeType) * 0.5f + 3.0f);
				const FVector2D B = End - Direction * (Diameter(Node.NodeType) * 0.5f + 3.0f);
				const bool bLearned = Progression.IsValid() && Progression->IsNodeUnlocked(ParentId) && Progression->IsNodeUnlocked(Node.NodeId);
				FLinearColor Color = bLearned ? NodeColor(Node) : FLinearColor(0.26f, 0.29f, 0.29f, 0.8f);
				if (Node.NodeId == Selected) Color = Pale;
				if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance && Parent->Identity == ERogue10mSkillTreeIdentity::None)
				{
					// Route core links around the outside so they cannot imply dependencies through inner nodes.
					const float StartAngle = FMath::Atan2(Parent->Position.Y, Parent->Position.X);
					float EndAngle = FMath::Atan2(Node.Position.Y, Node.Position.X);
					if (Node.Position.X < 0.0f && EndAngle > StartAngle) EndAngle -= 2.0f * PI;
					TArray<FVector2f> Points;
					Points.Add(FVector2f(Start + Parent->Position.GetSafeNormal() * (Diameter(Parent->NodeType) * 0.5f + 2.0f)));
					for (int32 Step = 0; Step <= 48; ++Step)
					{
						const float Angle = FMath::Lerp(StartAngle, EndAngle, static_cast<float>(Step) / 48.0f);
						Points.Add(FVector2f(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 385.0f));
					}
					const FVector2D TailDirection = (End - FVector2D(Points.Last())).GetSafeNormal();
					Points.Add(FVector2f(End - TailDirection * (Diameter(Node.NodeType) * 0.5f + 3.0f)));
					FSlateDrawElement::MakeLines(Elements, LayerId + 1, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, bLearned ? 2.5f : 1.4f);
					continue;
				}
				const bool bDashed = Node.PrerequisiteGroups.Num() > 1 || (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance && Parent->Identity != ERogue10mSkillTreeIdentity::None);
				if (bDashed)
				{
					TArray<FVector2f> Points = {FVector2f(A), FVector2f(B)};
					FSlateDrawElement::MakeDashedLines(Elements, LayerId + 1, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, bLearned ? 2.5f : 1.4f, 7.0f);
				}
				else
				{
					TArray<FVector2f> Points = {FVector2f(A), FVector2f(B)};
					FSlateDrawElement::MakeLines(Elements, LayerId + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, bLearned ? 2.5f : 1.4f);
				}
			}
		}
	}
	return LayerId + 2;
}

TSharedRef<SWidget> URogue10mHybridSkillTreeWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}
void URogue10mHybridSkillTreeWidget::NativeConstruct()
{
	Super::NativeConstruct(); InitializeTree();
}
void URogue10mHybridSkillTreeWidget::NativeDestruct()
{
	ReleaseContext(); Super::NativeDestruct();
}
void URogue10mHybridSkillTreeWidget::InitializeTree()
{
	BuildTree(); BindContext(); RefreshFromData();
}
void URogue10mHybridSkillTreeWidget::SetWindowActive(bool bActive)
{
	bWindowActive = bActive;
	if (bActive) InitializeTree();
	else { CancelKeySelection(); bRefundConfirmation = false; }
}
UTextBlock* URogue10mHybridSkillTreeWidget::MakeText(FName Name, const FText& Text, int32 Size, FLinearColor Color)
{
	UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Result->SetText(Text); Rogue10mHybridTreeUI::SetTextStyle(Result, Size, Color);
	Result->SetWrapTextAt(340.0f); return Result;
}
URogue10mHybridSkillTreeButton* URogue10mHybridSkillTreeWidget::MakeButton(FName Name, const FText& Label,
	int32 Action, FName NodeId, FName ChoiceId)
{
	URogue10mHybridSkillTreeButton* Button = WidgetTree->ConstructWidget<URogue10mHybridSkillTreeButton>(URogue10mHybridSkillTreeButton::StaticClass(), Name);
	Button->Configure(this, Action, NodeId, ChoiceId);
	Button->SetStyle(Rogue10mHybridTreeUI::ButtonStyle(Rogue10mHybridTreeUI::Gold));
	UTextBlock* Text = MakeText(NAME_None, Label, 17, Rogue10mHybridTreeUI::Pale);
	Text->SetJustification(ETextJustify::Center); Button->AddChild(Text);
	return Button;
}
void URogue10mHybridSkillTreeWidget::BuildTree()
{
	using namespace Rogue10mHybridTreeUI;
	if (Graph || !WidgetTree) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
	UCanvasPanelSlot* ScaleSlot = Canvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0.02f, 0.02f, 0.98f, 0.98f)); ScaleSlot->SetOffsets(FMargin(0));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(1500); Size->SetHeightOverride(920); Scale->AddChild(Size);
	UBorder* GoldFrame = WidgetTree->ConstructWidget<UBorder>(); GoldFrame->SetBrushColor(Gold * 0.72f); GoldFrame->SetPadding(FMargin(2)); Size->AddChild(GoldFrame);
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(); Frame->SetBrushColor(Iron); Frame->SetPadding(FMargin(18, 10)); GoldFrame->AddChild(Frame);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Frame->AddChild(Rows);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 7));
	UTextBlock* Title = MakeText(TEXT("HybridTreeTitle"), FText::FromString(TEXT("ROGUE10M  ·  스킬트리")), 28, Gold);
	Title->SetWrapTextAt(1200); Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Header->AddChildToHorizontalBox(MakeButton(TEXT("HybridTreeClose"), FText::FromString(TEXT("닫기 · K")), Close));
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(); LeftSize->SetWidthOverride(372); Body->AddChildToHorizontalBox(LeftSize)->SetPadding(FMargin(0, 0, 20, 0));
	UVerticalBox* LeftRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HybridTreeLeftColumn")); LeftSize->AddChild(LeftRoot);
	UVerticalBox* Left = LeftRoot;
	auto AddText = [this, &Left](const FName Name, const TCHAR* Value, int32 FontSize, FLinearColor Color)
	{
		UTextBlock* Text = MakeText(Name, FText::FromString(Value), FontSize, Color);
		Left->AddChildToVerticalBox(Text)->SetPadding(FMargin(2, 4)); return Text;
	};
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(); Left->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 8));
	FighterTab = MakeButton(TEXT("HybridTreeFighter"), FText::FromString(TEXT("격투가")), Fighter);
	GreatSwordTab = MakeButton(TEXT("HybridTreeGreatSword"), FText::FromString(TEXT("대검술사")), GreatSword);
	Tabs->AddChildToHorizontalBox(FighterTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Tabs->AddChildToHorizontalBox(GreatSwordTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GrowthText = AddText(TEXT("HybridTreeGrowth"), TEXT("캐릭터 성장"), 21, Pale);
	ContextText = AddText(TEXT("HybridTreeContext"), TEXT(""), 15, Muted);
	MasteryText = AddText(TEXT("HybridTreeMastery"), TEXT("숙련 M1"), 20, Gold);
	MasteryBar = WidgetTree->ConstructWidget<UProgressBar>(); MasteryBar->SetFillColorAndOpacity(Gold);
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(); BarSize->SetHeightOverride(9); BarSize->AddChild(MasteryBar);
	Left->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(2, 4, 2, 9));
	AddText(TEXT("HybridTreeMasteryStatsHeading"), TEXT("숙련 성장"), 16, Gold);
	MasteryStatsText = AddText(TEXT("HybridTreeMasteryStats"), TEXT(""), 16, Pale);
	IdentityText = AddText(TEXT("HybridTreeIdentity"), TEXT("활성 아이덴티티 · 미활성"), 18, Green);
	// Only the long description and requirements scroll. Actions are a separate bottom slot.
	UScrollBox* DetailsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HybridTreeDetailsScroll"));
	LeftRoot->AddChildToVerticalBox(DetailsScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Left = WidgetTree->ConstructWidget<UVerticalBox>(); DetailsScroll->AddChild(Left);
	AddText(NAME_None, TEXT("선택 노드"), 17, Gold);
	DetailName = AddText(TEXT("HybridTreeDetailName"), TEXT(""), 23, Pale);
	DetailState = AddText(TEXT("HybridTreeDetailState"), TEXT(""), 17, Muted);
	DetailDescription = AddText(TEXT("HybridTreeDetailDescription"), TEXT(""), 17, Pale);
	DetailRequirements = AddText(TEXT("HybridTreeDetailRequirements"), TEXT(""), 16, Muted);
	DetailCost = AddText(TEXT("HybridTreeDetailCost"), TEXT(""), 16, Gold);
	MessageText = AddText(TEXT("HybridTreeMessage"), TEXT(""), 16, Muted);
	UVerticalBox* Footer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HybridTreeActionFooter"));
	LeftRoot->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 7, 0, 0)); Left = Footer;
	ChoiceAButton = MakeButton(TEXT("HybridTreeChoiceA"), FText::GetEmpty(), Choose);
	ChoiceBButton = MakeButton(TEXT("HybridTreeChoiceB"), FText::GetEmpty(), Choose);
	UHorizontalBox* ChoiceRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Footer->AddChildToVerticalBox(ChoiceRow);
	for (URogue10mHybridSkillTreeButton* Button : {ChoiceAButton.Get(), ChoiceBButton.Get()}) ChoiceRow->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* LearnRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Left->AddChildToVerticalBox(LearnRow)->SetPadding(FMargin(0, 5));
	PointsButton = MakeButton(TEXT("HybridTreeLearnPoints"), FText::FromString(TEXT("포인트 전수")), LearnPoints);
	MasteryButton = MakeButton(TEXT("HybridTreeLearnMastery"), FText::FromString(TEXT("수련 전수 · 0P")), LearnMastery);
	for (URogue10mHybridSkillTreeButton* Button : {PointsButton.Get(), MasteryButton.Get()}) LearnRow->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ActivateButton = MakeButton(TEXT("HybridTreeActivate"), FText::FromString(TEXT("아이덴티티 활성화")), Activate);
	Left->AddChildToVerticalBox(ActivateButton)->SetPadding(FMargin(0, 4));
	BindingText = AddText(TEXT("HybridTreeBindingText"), TEXT(""), 15, Muted);
	BasicSkillPicker = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("HybridTreeBasicSkillPicker"));
	BasicSkillPicker->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleBasicSkillSelected);
	Left->AddChildToVerticalBox(BasicSkillPicker)->SetPadding(FMargin(0, 3));
	KeySelector = WidgetTree->ConstructWidget<UInputKeySelector>(UInputKeySelector::StaticClass(), TEXT("HybridTreeKeySelector"));
	KeySelector->SetAllowModifierKeys(false); KeySelector->SetAllowGamepadKeys(false); KeySelector->SetEscapeKeys({EKeys::Escape});
	KeySelector->SetKeySelectionText(FText::FromString(TEXT("키 입력 · Esc 취소"))); KeySelector->SetNoKeySpecifiedText(FText::FromString(TEXT("실행 키 지정")));
	KeySelector->OnKeySelected.AddUniqueDynamic(this, &ThisClass::HandleKeySelected);
	KeySelector->SetButtonStyle(ButtonStyle(Gold));
	UHorizontalBox* KeyRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Footer->AddChildToVerticalBox(KeyRow)->SetPadding(FMargin(0, 3));
	KeyRow->AddChildToHorizontalBox(KeySelector)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ClearKeyButton = MakeButton(TEXT("HybridTreeClearKey"), FText::FromString(TEXT("키 해제")), ClearKey);
	KeyRow->AddChildToHorizontalBox(ClearKeyButton);
	RefundButton = MakeButton(TEXT("HybridTreeRefund"), FText::FromString(TEXT("선택 단계 부분 재분배")), Refund);
	UHorizontalBox* RefundRow = WidgetTree->ConstructWidget<UHorizontalBox>(); Footer->AddChildToVerticalBox(RefundRow)->SetPadding(FMargin(0, 3));
	RefundRow->AddChildToHorizontalBox(RefundButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RefundCancelButton = MakeButton(TEXT("HybridTreeRefundCancel"), FText::FromString(TEXT("취소")), CancelRefund);
	RefundRow->AddChildToHorizontalBox(RefundCancelButton); RefundCancelButton->SetVisibility(ESlateVisibility::Collapsed);
	USizeBox* GraphSizeBox = WidgetTree->ConstructWidget<USizeBox>(); GraphSizeBox->SetWidthOverride(GraphSize.X); GraphSizeBox->SetHeightOverride(GraphSize.Y);
	Body->AddChildToHorizontalBox(GraphSizeBox);
	Graph = CreateWidget<URogue10mHybridSkillTreeGraphWidget>(GetOwningPlayer()); GraphSizeBox->AddChild(Graph);
	LegendText = MakeText(TEXT("HybridTreeLegend"), FText::GetEmpty(), 14, Muted);
	LegendText->SetWrapTextAt(1420); Rows->AddChildToVerticalBox(LegendText)->SetPadding(FMargin(0, 8, 0, 0));
}

void URogue10mHybridSkillTreeWidget::BindContext()
{
	ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	URogue10mSkillTreeProgressionComponent* Component = State ? State->FindComponentByClass<URogue10mSkillTreeProgressionComponent>() : nullptr;
	if (Progression.Get() == Component && BoundState.Get() == State && Component
		&& BoundCombat.Get() == GetCombat() && BoundBindings.Get() == GetBindings()
		&& BoundTown.Get() == (State ? State->GetTownProgressionComponent() : nullptr)) return;
	ReleaseContext(); BoundState = State; Progression = Component;
	if (State)
	{
		State->OnGrowthChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		BoundTown = State->GetTownProgressionComponent();
		if (BoundTown.IsValid()) BoundTown->OnTownProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
	}
	if (Component) Component->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
	BoundCombat = GetCombat();
	if (BoundCombat.IsValid()) BoundCombat->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
	BoundBindings = GetBindings();
	if (BoundBindings.IsValid()) BoundBindings->OnSkillBindingsChanged.AddUniqueDynamic(this, &ThisClass::HandleBindingsChanged);
}
void URogue10mHybridSkillTreeWidget::ReleaseContext()
{
	CancelKeySelection();
	if (Progression.IsValid()) Progression->OnChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundState.IsValid()) BoundState->OnGrowthChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundTown.IsValid()) BoundTown->OnTownProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundCombat.IsValid()) BoundCombat->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundBindings.IsValid()) BoundBindings->OnSkillBindingsChanged.RemoveDynamic(this, &ThisClass::HandleBindingsChanged);
	Progression.Reset(); BoundState.Reset(); BoundTown.Reset(); BoundCombat.Reset(); BoundBindings.Reset();
}
void URogue10mHybridSkillTreeWidget::RefreshFromData()
{
	using namespace Rogue10mHybridTreeUI;
	BuildTree(); BindContext();
	if (!Graph || !Progression.IsValid())
	{
		if (ContextText) ContextText->SetText(FText::FromString(TEXT("성장 정보를 준비 중입니다. 창을 다시 열어 주세요.")));
		return;
	}
	if (!bHasChosenTrack)
	{
		DisplayedTrack = Progression->GetActiveTrack() == ERogue10mSkillTreeTrack::GreatSword ? ERogue10mSkillTreeTrack::GreatSword : ERogue10mSkillTreeTrack::Brawler;
		bHasChosenTrack = true;
	}
	const auto Nodes = Progression->GetNodes(DisplayedTrack);
	if (!Nodes.ContainsByPredicate([this](const auto& Node) { return Node.NodeId == SelectedNodeId; }))
	{
		SelectedNodeId = Nodes.IsEmpty() ? NAME_None : Nodes[0].NodeId; PendingChoiceId = NAME_None;
	}
	const bool bCurrentTrack = Progression->GetActiveTrack() == DisplayedTrack;
	FighterTab->SetStyle(ButtonStyle(Gold, DisplayedTrack == ERogue10mSkillTreeTrack::Brawler));
	GreatSwordTab->SetStyle(ButtonStyle(Gold, DisplayedTrack == ERogue10mSkillTreeTrack::GreatSword));
	GrowthText->SetText(FText::FromString(FString::Printf(TEXT("LV%d · 공용 성장 %dP"), BoundState.IsValid() ? BoundState->GetPlayerLevel() : 1, Progression->GetAvailableGrowthPoints())));
	ContextText->SetText(FText::Format(FText::FromString(bCurrentTrack ? TEXT("{0} · 현재 무기") : TEXT("{0} · 미리보기\n해당 무기 장착 후 전수·활성·키 지정")), TrackName(DisplayedTrack)));
	const int32 CurrentXP = Progression->GetMasteryExperience(DisplayedTrack);
	const int32 NeededXP = Progression->GetMasteryExperienceToNextLevel(DisplayedTrack);
	const FString MasteryLabel = FString::Printf(TEXT("%s 숙련 M%d"), DisplayedTrack == ERogue10mSkillTreeTrack::Brawler ? TEXT("격투") : TEXT("대검"), Progression->GetMasteryLevel(DisplayedTrack));
	MasteryText->SetText(FText::FromString(NeededXP > 0 ? FString::Printf(TEXT("%s    %d / %d"), *MasteryLabel, CurrentXP, NeededXP) : MasteryLabel + TEXT(" · 완성")));
	MasteryBar->SetPercent(NeededXP > 0 ? FMath::Clamp(static_cast<float>(CurrentXP) / NeededXP, 0.0f, 1.0f) : 1.0f);
	const FText StatSummary = RewardText(Progression->GetTrackMasteryStatModifiers(DisplayedTrack));
	MasteryStatsText->SetText(StatSummary);
	MasteryStatsText->SetVisibility(StatSummary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UWidget* Heading = WidgetTree->FindWidget(TEXT("HybridTreeMasteryStatsHeading"))) Heading->SetVisibility(MasteryStatsText->GetVisibility());
	auto Identity = Progression->GetActiveIdentity();
	if (Progression->GetCatalog() && !Progression->GetCatalog()->IsIdentityAllowed(DisplayedTrack, Identity)) Identity = ERogue10mSkillTreeIdentity::None;
	IdentityText->SetText(FText::Format(FText::FromString(TEXT("활성 아이덴티티 · {0}")), IdentityName(Identity)));
	IdentityText->SetColorAndOpacity(FSlateColor(IdentityColor(Identity)));
	LegendText->SetText(FText::FromString(DisplayedTrack == ERogue10mSkillTreeTrack::Brawler
		? TEXT("작은 원 · 스탯   큰 원 · 기술   내공 · 녹색 / 투지 · 붉은색   실선 · 필수 선행   점선 · 경로 선택 / 공명")
		: TEXT("작은 원 · 스탯   큰 원 · 기술   투지 · 붉은색 / 마나 · 푸른색   실선 · 필수 선행   점선 · 경로 선택 / 공명")));
	Graph->SetData(this, Progression.Get(), DisplayedTrack, SelectedNodeId);
	RefreshDetails();
}
void URogue10mHybridSkillTreeWidget::RefreshDetails()
{
	using namespace Rogue10mHybridTreeUI;
	const FRogue10mSkillTreeNodeDefinition* Node = Progression.IsValid() ? Progression->FindNode(SelectedNodeId) : nullptr;
	if (!Node) return;
	const auto State = Progression->GetNodeState(Node->NodeId, DisplayedTrack);
	const bool bLearned = Progression->IsNodeUnlocked(Node->NodeId);
	DetailName->SetText(Node->DisplayName); DetailDescription->SetText(Node->Description);
	DetailState->SetText(StateName(State)); DetailState->SetColorAndOpacity(FSlateColor(bLearned ? Green : Gold));
	if (Node->bAutomaticUnlock && Node->NodeType != ERogue10mHybridSkillTreeNodeType::Root)
	{
		DetailState->SetText(FText::Format(FText::FromString(TEXT("{0} · {1}")), StateName(State),
			FText::FromString(Node->NodeType == ERogue10mHybridSkillTreeNodeType::Stat ? TEXT("숙련 스탯 성장") : TEXT("자동 전수"))));
	}
	DetailRequirements->SetText(Progression->GetNodeRequirementsText(Node->NodeId));
	DetailCost->SetText(FText::Format(FText::FromString(TEXT("해금 방식 · {0}")), CostText(*Node)));
	if (bLearned)
	{
		const FRogue10mSkillTreeSnapshot Snapshot = Progression->Capture();
		if (const auto* Investment = Snapshot.Investments.FindByPredicate([Node](const auto& Item) { return Item.NodeId == Node->NodeId; }))
		{
			DetailCost->SetText(FText::FromString(FString::Printf(TEXT("실제 투자 · %dP%s"), Investment->SpentPoints,
				Investment->Route == ERogue10mSkillTreeUnlockRoute::Mastery ? TEXT(" · 수련 전수") : TEXT(""))));
		}
	}
	if (PendingChoiceId.IsNone()) PendingChoiceId = Progression->GetSelectedChoice(Node->NodeId);
	if (PendingChoiceId.IsNone() && !Node->Choices.IsEmpty()) PendingChoiceId = Node->Choices[0].ChoiceId;
	int32 ChoiceIndex = 0;
	for (URogue10mHybridSkillTreeButton* Button : {ChoiceAButton.Get(), ChoiceBButton.Get()})
	{
		const bool bHasChoice = Node->Choices.IsValidIndex(ChoiceIndex);
		Button->SetVisibility(bHasChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bHasChoice)
		{
			const auto& Choice = Node->Choices[ChoiceIndex];
			Button->Configure(this, Choose, Node->NodeId, Choice.ChoiceId);
			CastChecked<UTextBlock>(Button->GetContent())->SetText(Choice.DisplayName);
			Button->SetToolTipText(Choice.Description);
			Button->SetStyle(ButtonStyle(NodeColor(*Node), PendingChoiceId == Choice.ChoiceId));
			Button->SetIsEnabled(!bLearned);
		}
		++ChoiceIndex;
	}
	const bool bCanInvest = Progression->GetActiveTrack() == DisplayedTrack;
	FText PointReason, MasteryReason;
	const bool bCanPoints = Progression->CanUnlockNode(Node->NodeId, ERogue10mSkillTreeUnlockRoute::Points, PendingChoiceId, PointReason);
	const bool bCanMastery = Progression->CanUnlockNode(Node->NodeId, ERogue10mSkillTreeUnlockRoute::Mastery, PendingChoiceId, MasteryReason);
	PointsButton->SetVisibility(Node->PointCost > 0 && !bLearned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CastChecked<UTextBlock>(PointsButton->GetContent())->SetText(FText::FromString(FString::Printf(TEXT("전수 · %dP"), Node->PointCost)));
	PointsButton->SetIsEnabled(bCanInvest && bCanPoints);
	PointsButton->SetToolTipText(PointReason);
	MasteryButton->SetVisibility(!bLearned && !Node->bAutomaticUnlock && !Node->MasteryAlternatives.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MasteryButton->SetIsEnabled(bCanInvest && bCanMastery); MasteryButton->SetToolTipText(MasteryReason);
	MasteryButton->SetStyle(ButtonStyle(Green, bCanInvest && bCanMastery));
	ActivateButton->SetVisibility(Node->Identity != ERogue10mSkillTreeIdentity::None && Progression->IsIdentityLearned(Node->Identity)
		&& Progression->GetActiveIdentity() != Node->Identity ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ActivateButton->SetIsEnabled(bCanInvest && Progression->CanChangeInvestments() && Progression->IsIdentityLearned(Node->Identity)
		&& Progression->GetActiveIdentity() != Node->Identity);
	RefundButton->SetVisibility(Node->Identity != ERogue10mSkillTreeIdentity::None && bLearned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefundButton->SetIsEnabled(bCanInvest && CanRefundType(Node->NodeType) && bLearned && Progression->CanChangeInvestments());
	RefundButton->SetToolTipText(FText::FromString(TEXT("선택한 입문·심화·성질 단계부터 유효 선행을 잃는 후속 노드와 공명만 철회하고 실제 투자 P를 환급합니다. 상위 단계·무료 숙련 성장·기술·XP는 유지합니다.")));
	CastChecked<UTextBlock>(RefundButton->GetContent())->SetText(FText::FromString(CanRefundType(Node->NodeType) ? TEXT("선택 단계 재분배") : TEXT("선행 단계에서 재분배")));
	if (bRefundConfirmation) CastChecked<UTextBlock>(RefundButton->GetContent())->SetText(FText::FromString(TEXT("부분 재분배 확정")));
	RefundCancelButton->SetVisibility(bRefundConfirmation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	{
		TGuardValue<bool> Guard(bUpdatingKeys, true);
		BasicSkillPicker->ClearOptions();
		BasicSkillPicker->SetVisibility(Node->NodeType == ERogue10mHybridSkillTreeNodeType::Basic && !Node->GrantedSkills.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		for (const auto& BasicSkill : Node->GrantedSkills)
		{
			if (auto* Loaded = BasicSkill.LoadSynchronous()) BasicSkillPicker->AddOption(Loaded->SkillName.ToString());
		}
		if (auto* SelectedSkill = GetSelectedSkill()) BasicSkillPicker->SetSelectedOption(SelectedSkill->SkillName.ToString());
	}
	RefreshKeyControls();
}

void URogue10mHybridSkillTreeWidget::HandleAction(int32 Action, FName NodeId, FName ChoiceId)
{
	using namespace Rogue10mHybridTreeUI;
	if (Action == Close)
	{
		if (auto* Controller = Cast<ARogue10mPlayerController>(GetOwningPlayer())) Controller->ToggleSkillTree();
		return;
	}
	if (Action == Fighter || Action == GreatSword)
	{
		CancelKeySelection(); bHasChosenTrack = true; bRefundConfirmation = false;
		DisplayedTrack = Action == Fighter ? ERogue10mSkillTreeTrack::Brawler : ERogue10mSkillTreeTrack::GreatSword;
		SelectedNodeId = NAME_None; PendingChoiceId = NAME_None; SelectedBasicSkill.Reset(); ShowMessage(FText::GetEmpty()); RefreshFromData(); return;
	}
	if (Action == Rogue10mHybridTreeUI::SelectNode)
	{
		CancelKeySelection(); if (SelectedNodeId != NodeId) SelectedBasicSkill.Reset(); SelectedNodeId = NodeId; PendingChoiceId = NAME_None; bRefundConfirmation = false;
		ShowMessage(FText::GetEmpty()); RefreshFromData(); return;
	}
	if (Action == ClearKey) { ClearSelectedSkillKey(); return; }
	if (!Progression.IsValid()) return;
	const FRogue10mSkillTreeNodeDefinition* Node = Progression->FindNode(SelectedNodeId);
	if (!Node) return;
	if (Action == Choose)
	{
		PendingChoiceId = ChoiceId; RefreshDetails(); return;
	}
	if (Action == CancelRefund) { bRefundConfirmation = false; ShowMessage(FText::GetEmpty()); RefreshDetails(); return; }
	if (Action == Refund && !CanRefundType(Node->NodeType))
	{
		ShowMessage(FText::FromString(TEXT("공명은 입문·심화·성질 단계에서 함께 재분배할 수 있습니다.")), true); return;
	}
	if (Action == Refund && !bRefundConfirmation)
	{
		bRefundConfirmation = true;
		ShowMessage(FText::FromString(TEXT("선택 단계와 선행을 잃는 후속 노드·공명만 철회합니다. 실제 투자 P만 환급하며 상위 단계·무료 숙련 성장·기술·XP는 유지합니다.")));
		RefreshDetails(); return;
	}
	bool bSuccess = false;
	{
		TGuardValue<bool> Guard(bApplyingAction, true);
		if (Action == LearnPoints || Action == LearnMastery)
		{
			FText Reason;
			const auto Route = Action == LearnMastery ? ERogue10mSkillTreeUnlockRoute::Mastery : ERogue10mSkillTreeUnlockRoute::Points;
			if (!Progression->CanUnlockNode(Node->NodeId, Route, PendingChoiceId, Reason)) { ShowMessage(Reason, true); return; }
			bSuccess = Progression->TryUnlockNode(Node->NodeId, Route, PendingChoiceId);
		}
		else if (Action == Activate) bSuccess = Progression->TrySetActiveIdentity(Node->Identity);
		else if (Action == Refund) { bSuccess = Progression->RefundNode(Node->NodeId); bRefundConfirmation = false; PendingChoiceId = NAME_None; }
	}
	ShowMessage(FText::FromString(bSuccess ? TEXT("변경을 적용했습니다.") : TEXT("변경할 수 없습니다. 무기·마을 서비스·잔액·선행 조건을 확인하세요.")), !bSuccess);
	RefreshFromData();
}
void URogue10mHybridSkillTreeWidget::BrowseTrack(ERogue10mSkillTreeTrack Track)
{
	if (Track == ERogue10mSkillTreeTrack::Brawler || Track == ERogue10mSkillTreeTrack::GreatSword)
		HandleAction(Track == ERogue10mSkillTreeTrack::Brawler ? Rogue10mHybridTreeUI::Fighter : Rogue10mHybridTreeUI::GreatSword);
}
void URogue10mHybridSkillTreeWidget::SelectNode(FName NodeId)
{
	if (Progression.IsValid() && Progression->GetNodes(DisplayedTrack).ContainsByPredicate([NodeId](const auto& Node) { return Node.NodeId == NodeId; }))
		HandleAction(Rogue10mHybridTreeUI::SelectNode, NodeId);
}
void URogue10mHybridSkillTreeWidget::ShowMessage(const FText& Message, bool bError)
{
	if (!MessageText) return;
	MessageText->SetText(Message); MessageText->SetColorAndOpacity(FSlateColor(bError ? Rogue10mHybridTreeUI::Red : Rogue10mHybridTreeUI::Muted));
}
void URogue10mHybridSkillTreeWidget::HandleProgressionChanged()
{
	if (!bApplyingAction && bWindowActive) RefreshFromData();
}
URogue10mCombatComponent* URogue10mHybridSkillTreeWidget::GetCombat() const
{
	const auto* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	return Character ? Character->GetCombatComponent() : nullptr;
}
URogue10mSkillBindingComponent* URogue10mHybridSkillTreeWidget::GetBindings() const
{
	const auto* Controller = Cast<ARogue10mPlayerController>(GetOwningPlayer());
	return Controller ? Controller->GetSkillBindingComponent() : nullptr;
}
URogue10mAttackSkillData* URogue10mHybridSkillTreeWidget::GetSelectedSkill() const
{
	const auto* Node = Progression.IsValid() ? Progression->FindNode(SelectedNodeId) : nullptr;
	if (!Node || (Node->NodeType != ERogue10mHybridSkillTreeNodeType::Technique && Node->NodeType != ERogue10mHybridSkillTreeNodeType::Basic)) return nullptr;
	if (Node->NodeType == ERogue10mHybridSkillTreeNodeType::Basic && !Node->GrantedSkills.IsEmpty())
	{
		for (const auto& BasicSkill : Node->GrantedSkills) if (SelectedBasicSkill.IsValid() && BasicSkill.Get() == SelectedBasicSkill.Get()) return SelectedBasicSkill.Get();
		return Node->GrantedSkills[0].LoadSynchronous();
	}
	return Node->Skill.LoadSynchronous();
}
void URogue10mHybridSkillTreeWidget::RefreshKeyControls()
{
	if (!KeySelector) return;
	TGuardValue<bool> Guard(bUpdatingKeys, true);
	auto* Skill = GetSelectedSkill(); auto* Bindings = GetBindings(); auto* Combat = GetCombat();
	const bool bCanBind = Skill && Bindings && Combat && Progression.IsValid()
		&& Progression->GetActiveTrack() == DisplayedTrack && Combat->IsAttackSkillUnlocked(Skill)
		&& Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique;
	const FKey Key = Skill && Bindings ? Bindings->GetSkillKey(Skill) : EKeys::Invalid;
	KeySelector->SetIsEnabled(bCanBind); KeySelector->SetSelectedKey(FInputChord(Key));
	ClearKeyButton->SetIsEnabled(bCanBind && Key.IsValid());
	const ESlateVisibility BindingVisibility = Skill ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	KeySelector->SetVisibility(BindingVisibility); ClearKeyButton->SetVisibility(BindingVisibility);
	BindingText->SetVisibility(Skill ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BindingText->SetText(FText::FromString(Skill
		? FString::Printf(TEXT("%s · 키 %s"), *Skill->SkillName.ToString(), Key.IsValid() ? *Key.GetDisplayName().ToString() : TEXT("미지정")) : TEXT("")));
	FString Hint = TEXT("현재 무기의 전수 공격 기술만 지정. 단일 키 · 이동/메뉴 키 제외 · Esc 취소. 선택 노드를 HUD로 드래그해 장착할 수 있습니다.");
	if (Skill && (Skill->InputSlot == ERogue10mAttackInputSlot::ChargedPrimary || Skill->InputSlot == ERogue10mAttackInputSlot::ChargedSpecial))
		Hint += FString::Printf(TEXT(" %.2f초 이상 누른 뒤 놓아 사용합니다."), Skill->ChargeSeconds);
	KeySelector->SetToolTipText(FText::FromString(Hint));
}
void URogue10mHybridSkillTreeWidget::CancelKeySelection()
{
	if (KeySelector && KeySelector->GetIsSelectingKey() && FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
}
void URogue10mHybridSkillTreeWidget::HandleKeySelected(FInputChord Key)
{
	if (bUpdatingKeys) return;
	const FModifierKeysState Modifiers = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetModifierKeys() : FModifierKeysState();
	if (Key.bShift || Key.bCtrl || Key.bAlt || Key.bCmd || Modifiers.IsShiftDown() || Modifiers.IsControlDown() || Modifiers.IsAltDown() || Modifiers.IsCommandDown())
	{
		ShowMessage(FText::FromString(TEXT("조합 키는 사용할 수 없습니다. 키 하나만 지정하세요.")), true); RefreshKeyControls(); return;
	}
	SetSelectedSkillKey(Key.Key);
}
bool URogue10mHybridSkillTreeWidget::SetSelectedSkillKey(FKey Key)
{
	FText Error; bool bSuccess = false;
	{ TGuardValue<bool> Guard(bApplyingAction, true); if (auto* Bindings = GetBindings()) bSuccess = Bindings->SetSkillKey(GetSelectedSkill(), Key, Error); }
	RefreshKeyControls(); ShowMessage(bSuccess ? FText::FromString(TEXT("키 설정을 저장했습니다.")) : Error, !bSuccess); return bSuccess;
}
bool URogue10mHybridSkillTreeWidget::ClearSelectedSkillKey()
{
	CancelKeySelection(); FText Error; bool bSuccess = false;
	{ TGuardValue<bool> Guard(bApplyingAction, true); if (auto* Bindings = GetBindings()) bSuccess = Bindings->ClearSkillKey(GetSelectedSkill(), Error); }
	RefreshKeyControls(); ShowMessage(bSuccess ? FText::FromString(TEXT("키 설정을 해제했습니다.")) : Error, !bSuccess); return bSuccess;
}
void URogue10mHybridSkillTreeWidget::HandleBindingsChanged()
{
	if (!bApplyingAction && !bUpdatingKeys) RefreshKeyControls();
}
void URogue10mHybridSkillTreeWidget::SelectAttackSkill(URogue10mAttackSkillData* Skill)
{
	if (!Progression.IsValid() || !Skill) return;
	for (const auto& Node : Progression->GetNodes(DisplayedTrack))
	{
		if (Node.Skill.LoadSynchronous() == Skill || Node.GrantedSkills.ContainsByPredicate([Skill](const auto& Item) { return Item.LoadSynchronous() == Skill; }))
		{
			CancelKeySelection(); SelectedNodeId = Node.NodeId; SelectedBasicSkill = Skill; PendingChoiceId = NAME_None; bRefundConfirmation = false;
			RefreshFromData(); return;
		}
	}
}
void URogue10mHybridSkillTreeWidget::HandleBasicSkillSelected(FString Selection, ESelectInfo::Type SelectionType)
{
	if (bUpdatingKeys || !Progression.IsValid()) return;
	const auto* Node = Progression->FindNode(SelectedNodeId);
	if (!Node) return;
	for (const auto& Skill : Node->GrantedSkills)
	{
		if (auto* Loaded = Skill.LoadSynchronous(); Loaded && Loaded->SkillName.ToString() == Selection)
		{
			CancelKeySelection(); SelectedBasicSkill = Loaded; Graph->SetData(this, Progression.Get(), DisplayedTrack, SelectedNodeId); RefreshKeyControls(); return;
		}
	}
}
int32 URogue10mHybridSkillTreeWidget::GetVisibleNodeCount() const
{
	return Progression.IsValid() ? Progression->GetNodes(DisplayedTrack).Num() : 0;
}
