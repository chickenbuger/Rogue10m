#include "Widgets/Rogue10mHybridSkillTreeWidget.h"

#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InputKeySelector.h"
#include "Components/NativeWidgetHost.h"
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
#include "Slate/UMGDragDropOp.h"
#include "Styling/WidgetStyle.h"
#include "Widgets/Rogue10mCommonSkillChrome.h"
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
	const FVector2D CommonGraphSize(1380.0f, 825.0f);
	const FVector2D Center(520.0f, 390.0f);
	const FVector2D SwordHub(520.0f, 405.0f);
	const FLinearColor KnuckleBrass(0.73f, 0.42f, 0.16f);
	const FLinearColor KnuckleBronze(0.38f, 0.20f, 0.08f);
	const FLinearColor KnuckleLight(1.0f, 0.73f, 0.38f);
	const FLinearColor SwordSteel(0.38f, 0.46f, 0.52f);
	const FLinearColor SwordBrass(0.54f, 0.34f, 0.15f);
	enum EAction { Fighter = 1, GreatSword, LearnPoints, LearnMastery, Activate, Choose,
		Refund, CancelRefund, ClearKey, Close, Common = 11, Magic = 12, ToggleCommonView = 13, SelectNode = 100 };

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
		if (Node.CommonSkill != ERogue10mCommonSkill::None) return Node.Color;
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
	float NodeDiameter(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		using ENode = ERogue10mHybridSkillTreeNodeType;
		using ECommon = ERogue10mCommonSkill;
		switch (Node.CommonSkill)
		{
		case ECommon::None: break;
		case ECommon::Foundation: return 176.0f;
		case ECommon::DoubleJump: return 130.0f;
		case ECommon::MoveSpeed:
		case ECommon::EvadeRecovery: return 138.0f;
		case ECommon::AirMomentum:
		case ECommon::LandingStability: return 112.0f;
		default: return 123.0f;
		}
		if (Node.Track == ERogue10mSkillTreeTrack::Brawler) return 68.0f;
		if (Node.Track == ERogue10mSkillTreeTrack::GreatSword)
		{
			switch (Node.NodeType)
			{
			case ENode::Basic: return 72.0f;
			case ENode::Technique: return Node.NodeId == TEXT("G.Ultimate") ? 68.0f : 62.0f;
			case ENode::Root: return 38.0f;
			case ENode::Stat: return 32.0f;
			default: break;
			}
		}
		return Diameter(Node.NodeType);
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
	FButtonStyle KnuckleMedallionStyle(ERogue10mSkillTreeNodeState State, bool bSelected, float Radius)
	{
		const bool bLocked = State == ERogue10mSkillTreeNodeState::Locked;
		const FLinearColor Rim = bSelected ? KnuckleLight : (bLocked ? KnuckleBronze : KnuckleBrass);
		FButtonStyle Style = ButtonStyle(Rim, false, !bLocked, Radius);
		const FLinearColor Fill = bSelected ? FLinearColor(0.032f, 0.017f, 0.008f)
			: (bLocked ? FLinearColor(0.012f, 0.007f, 0.004f) : FLinearColor(0.021f, 0.012f, 0.006f));
		Style.Normal = FSlateRoundedBoxBrush(Fill, Radius, Rim, bSelected ? 2.5f : 1.5f);
		Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.048f, 0.024f, 0.010f), Radius, KnuckleLight, 2.5f);
		Style.Pressed = FSlateRoundedBoxBrush(FLinearColor(0.060f, 0.029f, 0.012f), Radius, KnuckleBrass, 2.5f);
		return Style;
	}
	FButtonStyle SwordNodeStyle(FLinearColor Accent, ERogue10mSkillTreeNodeState State, bool bSelected, float Radius)
	{
		const bool bLocked = State == ERogue10mSkillTreeNodeState::Locked;
		const FLinearColor Rim = bSelected ? KnuckleLight : (bLocked ? SwordSteel * 0.55f : Accent);
		const FLinearColor Fill = bSelected ? FLinearColor(0.045f, 0.027f, 0.014f)
			: (bLocked ? FLinearColor(0.011f, 0.014f, 0.017f) : FLinearColor(0.022f, 0.024f, 0.023f));
		FButtonStyle Style = ButtonStyle(Rim, false, !bLocked, Radius);
		Style.Normal = FSlateRoundedBoxBrush(Fill, Radius, Rim, bSelected ? 2.1f : 1.3f);
		Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.043f, 0.045f, 0.043f), Radius, Pale, 1.9f);
		Style.Pressed = FSlateRoundedBoxBrush(FLinearColor(0.063f, 0.041f, 0.022f), Radius, KnuckleLight, 1.9f);
		return Style;
	}
	FButtonStyle CommonMedallionStyle(FLinearColor Accent, ERogue10mSkillTreeNodeState State,
		bool bSelected, float Radius, bool bHasArt)
	{
		using EState = ERogue10mSkillTreeNodeState;
		const bool bLocked = State == EState::Locked;
		const FLinearColor Rim = bSelected ? KnuckleLight : bLocked ? Accent * 0.42f : Accent;
		const FLinearColor Fill = (bSelected ? Accent * 0.15f : Accent * (bLocked ? 0.035f : 0.085f)).CopyWithNewOpacity(1.0f);
		FButtonStyle Style = ButtonStyle(Rim, false, !bLocked, Radius);
		if (bHasArt)
		{
			// The painted inset, metal rim and relief are already in the illustration.
			Style.Normal = FSlateNoResource();
			Style.Hovered = FSlateNoResource();
			Style.Pressed = FSlateNoResource();
			Style.Disabled = FSlateNoResource();
			Style.NormalForeground = FSlateColor(FLinearColor::White);
			Style.HoveredForeground = FSlateColor(FLinearColor(1.0f, 0.97f, 0.89f));
			Style.PressedForeground = FSlateColor(FLinearColor(0.88f, 0.66f, 0.41f));
			Style.NormalPadding = Style.PressedPadding = FMargin(0.0f);
			return Style;
		}
		Style.Normal = FSlateRoundedBoxBrush(Fill, Radius, Rim.CopyWithNewOpacity(1.0f), bSelected ? 2.6f : 1.5f);
		Style.Hovered = FSlateRoundedBoxBrush((Accent * 0.20f).CopyWithNewOpacity(1.0f), Radius, Pale, 2.4f);
		Style.Pressed = FSlateRoundedBoxBrush((Accent * 0.25f).CopyWithNewOpacity(1.0f), Radius, KnuckleLight, 2.4f);
		return Style;
	}
	UTexture2D* CommonMedallionAtlas()
	{
		static TWeakObjectPtr<UTexture2D> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UTexture2D>(nullptr,
				TEXT("/Game/UI/CommonSkillTree/T_CommonSkillMedallions.T_CommonSkillMedallions"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return Cached.Get();
	}
	float CommonVisualRadius(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		// A cell reserves an even transparent gutter around its 235-pixel metal rim.
		return NodeDiameter(Node) * (235.0f / 512.0f);
	}
	FSlateBrush CommonMedallionBrush(UTexture2D& Atlas, ERogue10mCommonSkill Skill, float ArtSize)
	{
		constexpr int32 AtlasColumns = 4;
		constexpr float CellSize = 1.0f / AtlasColumns;
		const int32 Index = static_cast<int32>(Skill) - 1;
		const FVector2f MinUV((Index % AtlasColumns) * CellSize, (Index / AtlasColumns) * CellSize);
		FSlateBrush Brush;
		Brush.SetResourceObject(&Atlas);
		Brush.SetImageSize(FVector2f(ArtSize, ArtSize));
		Brush.SetUVRegion(FBox2f(MinUV, MinUV + FVector2f(CellSize, CellSize)));
		return Brush;
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
	FVector2D LayoutPosition(const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTab Tab)
	{
		using EIdentity = ERogue10mSkillTreeIdentity;
		using ENode = ERogue10mHybridSkillTreeNodeType;
		if (Node.CommonSkill != ERogue10mCommonSkill::None)
		{
			// Centers measured from the approved composition, in enum/atlas order.
			static const FVector2D CommonPositions[] = {
				{652, 518}, {652, 300}, {486, 183}, {814, 183}, {652, 64},
				{427, 518}, {248, 446}, {248, 651}, {96, 519}, {879, 517},
				{1068, 446}, {1068, 652}, {1272, 518}, {303, 272}, {1002, 272}, {652, 732}
			};
			const int32 Index = static_cast<int32>(Node.CommonSkill) - 1;
			if (Index >= 0 && Index < UE_ARRAY_COUNT(CommonPositions)) return CommonPositions[Index];
			return Node.Position + FVector2D(0.0, 46.0);
		}
		if (Tab == ERogue10mSkillTreeTab::Common && Node.Identity != EIdentity::None)
		{
			const double X = Node.Identity == EIdentity::InternalEnergy ? 240.0
				: Node.Identity == EIdentity::FightingSpirit ? 520.0 : 800.0;
			const double Y = Node.NodeType == ENode::IdentityEntry ? 595.0
				: Node.NodeType == ENode::IdentityAdvanced ? 410.0 : 225.0;
			return {X, Y};
		}
		if (Tab != ERogue10mSkillTreeTab::Sword) return Position(Node);
		static const TMap<FName, FVector2D> SwordPositions = {
			{TEXT("G.Root"), {520, 501}}, {TEXT("G.Basic"), SwordHub},
			{TEXT("G.Power.1"), {520, 275}}, {TEXT("G.Flow.1"), {375, 485}}, {TEXT("G.Body.1"), {665, 485}},
			{TEXT("G.CrushingSlash"), {305, 320}}, {TEXT("G.Whirlwind"), {520, 646}}, {TEXT("G.GuardBreak"), {735, 320}},
			{TEXT("G.Power.2"), {254, 210}}, {TEXT("G.EarthSplitter"), {520, 154}}, {TEXT("G.Power.3"), {681, 181}},
			{TEXT("G.Flow.2"), {305, 632}}, {TEXT("G.Flow.3"), {150, 538}},
			{TEXT("G.Body.2"), {824, 420}}, {TEXT("G.Body.3"), {895, 545}},
			{TEXT("G.Ultimate"), {520, 51}},
			{TEXT("G.Resonance.Pursuit"), {105, 62}}, {TEXT("G.Resonance.Spread"), {300, 62}},
			{TEXT("G.Resonance.Burst"), {740, 62}}, {TEXT("G.Resonance.Focus"), {935, 62}}
		};
		if (const FVector2D* Location = SwordPositions.Find(Node.NodeId)) return *Location;
		return Position(Node);
	}
	bool IsLearned(ERogue10mSkillTreeNodeState State)
	{
		return State == ERogue10mSkillTreeNodeState::Learned || State == ERogue10mSkillTreeNodeState::Active;
	}
	ERogue10mSkillTreeNodeState DisplayState(const URogue10mSkillTreeProgressionComponent& Progression,
		const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTrack ContextTrack)
	{
		if (Node.Identity != ERogue10mSkillTreeIdentity::None && Node.Track == ERogue10mSkillTreeTrack::None
			&& !Node.AllowedTracks.Contains(ContextTrack))
		{
			return Progression.IsNodeUnlocked(Node.NodeId) ? ERogue10mSkillTreeNodeState::Learned : ERogue10mSkillTreeNodeState::Locked;
		}
		return Progression.GetNodeState(Node.NodeId, ContextTrack);
	}
	bool HasIdentityFrame(ERogue10mHybridSkillTreeNodeType Type)
	{
		return Type == ERogue10mHybridSkillTreeNodeType::IdentityEntry
			|| Type == ERogue10mHybridSkillTreeNodeType::IdentityAdvanced
			|| Type == ERogue10mHybridSkillTreeNodeType::IdentityChoice;
	}
	struct FNodeFrame
	{
		FName Shape = TEXT("Circle");
		int32 OutlineCount = 1;
		float Extent = 0.0f;
		TArray<FVector2f> OuterPoints;
	};
	TArray<FVector2f> CirclePoints(FVector2f Location, float Radius)
	{
		constexpr int32 Segments = 48;
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Step = 0; Step < Segments; ++Step)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(Step) / Segments;
			Points.Add(Location + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		Points.Add(Location + FVector2f(Radius, 0.0f));
		return Points;
	}
	FNodeFrame NodeFrame(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		FNodeFrame Frame;
		const float Radius = NodeDiameter(Node) * 0.5f;
		Frame.Extent = Radius;
		const FVector2f Location(Position(Node));
		if (Node.CommonSkill != ERogue10mCommonSkill::None)
		{
			Frame.Shape = TEXT("CommonMedallion");
			Frame.OutlineCount = 3;
			Frame.Extent = Radius + 7.0f;
			Frame.OuterPoints = CirclePoints(Location, Radius + 5.0f);
		}
		else if (Node.Track == ERogue10mSkillTreeTrack::Brawler)
		{
			Frame.Shape = TEXT("KnuckleMedallion");
			Frame.OutlineCount = 3;
			Frame.Extent = Radius + 8.0f;
			Frame.OuterPoints = CirclePoints(Location, Radius + 6.0f);
		}
		else if (HasIdentityFrame(Node.NodeType))
		{
			Frame.Shape = TEXT("IdentityDiamond");
			// Enclose the unchanged circular control with a small clearance at each edge.
			const float DiamondRadius = Radius * UE_SQRT_2 + 3.0f;
			Frame.Extent = DiamondRadius + 2.0f;
			Frame.OuterPoints = {Location + FVector2f(0.0f, -DiamondRadius), Location + FVector2f(DiamondRadius, 0.0f),
				Location + FVector2f(0.0f, DiamondRadius), Location + FVector2f(-DiamondRadius, 0.0f), Location + FVector2f(0.0f, -DiamondRadius)};
		}
		else if (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance)
		{
			Frame.Shape = TEXT("ResonanceDoubleRing");
			Frame.OutlineCount = 2; // The native button supplies the inner ring.
			Frame.Extent = Radius + 7.0f;
			Frame.OuterPoints = CirclePoints(Location, Radius + 5.0f);
		}
		else if (Node.Track == ERogue10mSkillTreeTrack::GreatSword)
		{
			const bool bMedallion = Node.NodeType == ERogue10mHybridSkillTreeNodeType::Basic
				|| Node.NodeType == ERogue10mHybridSkillTreeNodeType::Technique;
			const float OuterRadius = Radius + (bMedallion ? 6.0f : 3.0f);
			Frame.Shape = bMedallion ? TEXT("SwordMedallion") : TEXT("SwordSocket");
			Frame.OutlineCount = bMedallion ? 3 : 2;
			Frame.Extent = OuterRadius + 2.0f;
			Frame.OuterPoints = CirclePoints(Location, OuterRadius);
		}
		return Frame;
	}
	void PaintCommonGlyph(ERogue10mCommonSkill Skill, const FGeometry& Geometry,
		FSlateWindowElementList& Elements, int32 Layer, FLinearColor Ink)
	{
		const FVector2f CanvasSize(Geometry.GetLocalSize());
		const FVector2f Origin = CanvasSize * 0.5f;
		const float Scale = FMath::Min(CanvasSize.X, CanvasSize.Y) * 0.32f;
		const float LineScale = FMath::Clamp(Scale / 21.0f, 1.0f, 1.65f);
		const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
		const auto Stroke = [&](TArray<FVector2f> Points, float Width = 1.9f, float Opacity = 1.0f)
		{
			for (FVector2f& Point : Points) Point = Origin + Point * Scale;
			FSlateDrawElement::MakeLines(Elements, Layer, PaintGeometry, Points, ESlateDrawEffect::None,
				Ink.CopyWithNewOpacity(Ink.A * Opacity), true, Width * LineScale);
		};
		const auto Arrow = [&Stroke](FVector2f Start, FVector2f End)
		{
			const FVector2f Direction = (End - Start).GetSafeNormal();
			const FVector2f Tail = End - Direction * 0.26f;
			const FVector2f Wing = FVector2f(-Direction.Y, Direction.X) * 0.19f;
			Stroke({Start, End});
			Stroke({Tail + Wing, End, Tail - Wing});
		};
		const auto Arc = [&Stroke](FVector2f Offset, FVector2f Radius, float Start, float ArcLength, float Width = 1.8f)
		{
			TArray<FVector2f> Points;
			Points.Reserve(25);
			for (int32 Step = 0; Step <= 24; ++Step)
			{
				const float Angle = Start + ArcLength * static_cast<float>(Step) / 24.0f;
				Points.Add(Offset + FVector2f(FMath::Cos(Angle) * Radius.X, FMath::Sin(Angle) * Radius.Y));
			}
			Stroke(MoveTemp(Points), Width);
		};
		const auto Jump = [&Stroke](FVector2f Offset)
		{
			Stroke({Offset + FVector2f(-.55f, .37f), Offset + FVector2f(-.32f, .05f),
				Offset + FVector2f(-.06f, -.12f), Offset + FVector2f(.30f, -.22f)});
			Stroke({Offset + FVector2f(.02f, -.38f), Offset + FVector2f(.30f, -.22f), Offset + FVector2f(.10f, .02f)});
		};
		using ESkill = ERogue10mCommonSkill;
		switch (Skill)
		{
		case ESkill::Foundation:
			Stroke({{-.33f,.74f}, {-.41f,.43f}, {-.70f,.06f}, {-.64f,-.11f}, {-.37f,.08f},
				{-.42f,-.55f}, {-.27f,-.60f}, {-.19f,-.14f}, {-.17f,-.76f}, {-.01f,-.79f},
				{.06f,-.20f}, {.12f,-.65f}, {.27f,-.64f}, {.27f,-.17f}, {.42f,-.47f},
				{.56f,-.41f}, {.48f,.36f}, {.34f,.59f}, {.32f,.74f}, {-.33f,.74f}}, 2.25f);
			Stroke({{-.30f,.31f}, {-.05f,.14f}, {.22f,.22f}}, 1.3f, 0.72f);
			break;
		case ESkill::AirControl:
			Stroke({{-.66f,.65f}, {-.19f,.09f}, {.47f,-.52f}}, 2.1f);
			Stroke({{-.27f,.19f}, {-.21f,-.23f}, {.18f,-.55f}, {.65f,-.64f}, {.52f,-.12f},
				{.30f,.13f}, {-.27f,.19f}});
			Stroke({{-.73f,.27f}, {-.53f,.33f}}, 1.4f, 0.70f);
			break;
		case ESkill::DoubleJump:
		case ESkill::DoubleJumpUpgrade:
			Jump({-.24f,.34f});
			Jump({.42f,-.24f});
			if (Skill == ESkill::DoubleJumpUpgrade)
			{
				Stroke({{-.54f,-.68f}, {-.54f,-.16f}});
				Stroke({{-.80f,-.42f}, {-.28f,-.42f}});
			}
			else Stroke({{-.78f,.76f}, {-.45f,.76f}}, 1.3f, 0.67f);
			break;
		case ESkill::JumpHeight:
			Arrow({.05f,.64f}, {.05f,-.75f});
			Arrow({-.50f,.47f}, {-.50f,-.38f});
			Stroke({{.40f,.52f}, {.40f,-.18f}}, 1.6f, 0.70f);
			break;
		case ESkill::MoveSpeed:
		case ESkill::MoveSpeedUpgrade:
			Stroke({{-.18f,-.56f}, {.35f,-.56f}, {.23f,.10f}, {.60f,.27f}, {.72f,.50f}, {.62f,.61f},
				{-.40f,.61f}, {-.48f,.47f}, {-.27f,.29f}, {-.18f,-.56f}}, 2.15f);
			Stroke({{-.09f,-.29f}, {.23f,-.29f}}, 1.4f, 0.78f);
			Stroke({{-.77f,-.13f}, {-.39f,-.13f}}, 1.6f, 0.75f);
			Stroke({{-.85f,.12f}, {-.49f,.12f}}, 1.6f, 0.55f);
			if (Skill == ESkill::MoveSpeedUpgrade) Stroke({{-.79f,-.38f}, {-.43f,-.38f}}, 1.85f);
			break;
		case ESkill::GroundTurning:
			Arc({0,0}, {.73f,.50f}, 0.40f, 4.35f);
			Stroke({{-.28f,-.70f}, {.03f,-.50f}, {-.23f,-.25f}});
			Arc({0,0}, {.38f,.22f}, 3.0f, -2.5f, 1.5f);
			break;
		case ESkill::GroundAcceleration:
			for (const float X : {-.46f, -.02f, .42f})
			{
				Stroke({{X-.25f,-.39f}, {X+.25f,0}, {X-.25f,.39f}});
			}
			break;
		case ESkill::EvadeRecovery:
		case ESkill::RollCooldown:
			Arc({.08f,.05f}, {.61f,.61f}, 0.0f, 2.0f * UE_PI, 2.0f);
			Stroke({{.08f,-.31f}, {.08f,.05f}, {.38f,-.08f}});
			Stroke({{-.05f,-.71f}, {.21f,-.71f}}, 1.6f);
			if (Skill == ESkill::RollCooldown)
			{
				Arc({.08f,.05f}, {.79f,.79f}, 1.65f, 1.55f, 1.4f);
				Stroke({{-.85f,.25f}, {-.71f,.03f}, {-.55f,.22f}});
			}
			else Stroke({{-.83f,.36f}, {-.62f,.36f}}, 1.4f, 0.73f);
			break;
		case ESkill::Roll:
			Arc({0,0}, {.79f,.68f}, 0.8f, 4.5f, 2.15f);
			Stroke({{.16f,-.72f}, {.42f,-.56f}, {.16f,-.34f}}, 1.7f);
			Arc({.31f,-.06f}, {.16f,.16f}, 0, 2.0f * UE_PI, 1.55f);
			Stroke({{.13f,-.08f}, {-.13f,-.20f}, {-.40f,-.03f}, {-.40f,.23f}, {-.19f,.37f},
				{.15f,.29f}, {.29f,.17f}, {.06f,.12f}, {-.08f,.24f}}, 1.9f);
			break;
		case ESkill::EvadeDistance:
			Arrow({-.33f,-.11f}, {-.81f,-.11f});
			Arrow({.33f,-.11f}, {.81f,-.11f});
			Stroke({{-.10f,-.13f}, {0,-.28f}, {.11f,-.13f}, {0,.04f}, {-.10f,-.13f}}, 1.45f);
			Stroke({{-.65f,.35f}, {.65f,.35f}}, 1.5f, 0.67f);
			break;
		case ESkill::AirMomentum:
			Stroke({{-.72f,-.17f}, {-.38f,-.25f}, {-.08f,-.49f}, {.32f,-.52f}, {.70f,-.38f}}, 2.0f);
			Stroke({{-.80f,.25f}, {-.45f,.16f}, {-.10f,-.08f}, {.33f,-.08f}, {.79f,.04f}}, 2.0f);
			Stroke({{-.65f,.59f}, {-.34f,.47f}, {-.05f,.31f}, {.32f,.35f}}, 1.7f, 0.82f);
			break;
		case ESkill::LandingStability:
			Arrow({0,-.77f}, {0,.29f});
			Arc({0,.41f}, {.69f,.24f}, 0.0f, 2.0f * UE_PI, 1.8f);
			break;
		case ESkill::EvadeHaste:
			Stroke({{.24f,-.70f}, {-.39f,.05f}, {-.04f,.05f}, {-.25f,.73f}, {.52f,-.15f}, {.17f,-.15f}, {.24f,-.70f}}, 2.0f);
			Stroke({{-.80f,.34f}, {-.43f,.34f}}, 1.6f, 0.75f);
			break;
		default: break;
		}
	}
	UTexture2D* KnuckleTrainingIcon(const FRogue10mSkillTreeNodeDefinition& Node)
	{
		// Use the existing combat icon set for training and for attacks authored without thumbnails.
		static const TMap<FName, const TCHAR*> Icons = {
			{TEXT("F.Basic.RightStraight"), TEXT("T_Skill_StoneFist_Straight")},
			{TEXT("F.Basic.RightHook"), TEXT("T_Skill_StoneFist_Dodge")},
			{TEXT("F.Power.1"), TEXT("T_Skill_StoneFist_ChargedShockwave")},
			{TEXT("F.Breath.1"), TEXT("T_Skill_StoneFist_Dodge")},
			{TEXT("F.Body.1"), TEXT("T_Skill_StoneFist_JumpSlam")},
			{TEXT("F.Body.2"), TEXT("T_Skill_StoneFist_Straight")},
			{TEXT("F.Bengquan"), TEXT("T_Skill_StoneFist_Straight")},
			{TEXT("F.Shoulder"), TEXT("T_Skill_StoneFist_ChargedShockwave")},
			{TEXT("F.BreakingKick"), TEXT("T_Skill_StoneFist_DoubleJump")},
			{TEXT("F.HundredFists"), TEXT("T_Skill_StoneFist_ChargedShockwave")},
			{TEXT("F.GoldenBody"), TEXT("T_Skill_StoneFist_Dodge")}
		};
		const TCHAR* const* Icon = Icons.Find(Node.NodeId);
		const TCHAR* Name = Icon ? *Icon : TEXT("T_Skill_StoneFist_Jab");
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Icons/StoneFist/%s.%s"), Name, Name));
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
int32 URogue10mHybridSkillTreeEntryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style,
	bool bParentEnabled) const
{
	const int32 ContentLayer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
	if (CommonGlyph != ERogue10mCommonSkill::None)
	{
		Rogue10mHybridTreeUI::PaintCommonGlyph(CommonGlyph, Geometry, Elements, ContentLayer + 1,
			GlyphTint * Style.GetColorAndOpacityTint());
		return ContentLayer + 1;
	}
	if (!bPaintSwordGlyph) return ContentLayer;

	const FVector2f CanvasSize(Geometry.GetLocalSize());
	const FVector2f CenterPoint = CanvasSize * 0.5f;
	const float Size = FMath::Min(CanvasSize.X, CanvasSize.Y) * 0.36f;
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
	const FLinearColor Ink = GlyphTint * Style.GetColorAndOpacityTint();
	const auto Stroke = [&](TArray<FVector2f> Points, float Width = 1.5f, float Opacity = 1.0f)
	{
		for (FVector2f& Point : Points) Point = CenterPoint + Point * Size;
		FLinearColor Color = Ink;
		Color.A *= Opacity;
		FSlateDrawElement::MakeLines(Elements, ContentLayer + 1, PaintGeometry, Points, ESlateDrawEffect::None, Color, true, Width);
	};
	const auto Blade = [&Stroke](float Angle, float Scale = 1.0f, FVector2f Offset = FVector2f::ZeroVector)
	{
		float Sin, Cos;
		FMath::SinCos(&Sin, &Cos, Angle);
		const auto Rotate = [Sin, Cos, Scale, Offset](float X, float Y)
		{
			return Offset + FVector2f(X * Cos - Y * Sin, X * Sin + Y * Cos) * Scale;
		};
		Stroke({Rotate(0, -.82f), Rotate(.15f, -.49f), Rotate(.11f, .31f), Rotate(-.11f, .31f), Rotate(-.15f, -.49f), Rotate(0, -.82f)}, 2.1f);
		Stroke({Rotate(0, -.76f), Rotate(0, .29f)}, 1.15f, .72f);
		Stroke({Rotate(-.29f, .27f), Rotate(-.22f, .37f), Rotate(.22f, .37f), Rotate(.29f, .27f)}, 2.2f);
		Stroke({Rotate(0, .36f), Rotate(0, .62f), Rotate(-.09f, .69f), Rotate(0, .77f), Rotate(.09f, .69f), Rotate(0, .62f)}, 1.7f);
	};
	const auto Spiral = [&Stroke](float XRadius, float YRadius, float Start, float End, float Width)
	{
		TArray<FVector2f> Points;
		for (int32 Step = 0; Step <= 24; ++Step)
		{
			const float Angle = FMath::Lerp(Start, End, static_cast<float>(Step) / 24.0f);
			const float X = FMath::Cos(Angle) * XRadius;
			Points.Emplace(X, FMath::Sin(Angle) * YRadius - X * .28f);
		}
		Stroke(MoveTemp(Points), Width);
	};

	if (NodeId == TEXT("G.CrushingSlash"))
	{
		Blade(0.85f);
		Stroke({{-.82f, .37f}, {-.34f, .01f}, {.14f, -.23f}}, 1.6f, .65f);
		Stroke({{-.59f, .78f}, {-.13f, .48f}, {.43f, .31f}}, 1.9f, .85f);
	}
	else if (NodeId == TEXT("G.Whirlwind"))
	{
		Spiral(.87f, .43f, -2.15f, 3.0f, 2.3f);
		Spiral(.62f, .28f, -2.03f, 2.32f, 1.2f);
		Stroke({{-.38f, .44f}, {-.86f, .31f}, {-.63f, -.08f}}, 2.0f);
	}
	else if (NodeId == TEXT("G.GuardBreak"))
	{
		Stroke({{-.64f, -.48f}, {-.74f, -.34f}, {-.64f, .30f}, {-.39f, .60f}, {-.11f, .73f}, {.31f, .44f}, {.44f, -.12f}}, 1.7f, .75f);
		Stroke({{-.72f, -.06f}, {-.47f, .07f}, {-.42f, -.12f}, {-.23f, .28f}, {-.34f, .53f}}, 1.45f);
		Blade(.53f, .86f, {.22f, -.16f});
	}
	else if (NodeId == TEXT("G.EarthSplitter"))
	{
		Blade(UE_PI + .28f, .88f, {0, -.13f});
		Stroke({{-.83f, .65f}, {-.54f, .48f}, {-.29f, .64f}, {-.19f, .43f}, {.19f, .55f}, {.47f, .39f}, {.80f, .62f}}, 1.65f);
		Stroke({{-.52f, .35f}, {-.70f, .19f}}, 1.5f, .75f);
		Stroke({{.33f, .34f}, {.53f, .12f}}, 1.5f, .75f);
	}
	else if (NodeId == TEXT("G.Ultimate"))
	{
		Blade(UE_PI, 1.03f);
		Stroke({{-.84f, -.15f}, {-.21f, .64f}, {-.60f, .44f}}, 1.8f);
		Stroke({{.84f, -.15f}, {.21f, .64f}, {.60f, .44f}}, 1.8f);
		Stroke({{-.50f, -.49f}, {-.24f, -.10f}}, 1.45f, .80f);
		Stroke({{.50f, -.49f}, {.24f, -.10f}}, 1.45f, .80f);
	}
	else if (NodeId == TEXT("G.Root"))
	{
		Stroke({{0, -.69f}, {.47f, 0}, {0, .69f}, {-.47f, 0}, {0, -.69f}}, 2.25f);
		Stroke({{-.25f, 0}, {.25f, 0}}, 1.35f);
	}
	else if (NodeId.ToString().StartsWith(TEXT("G.Flow.")))
	{
		Stroke({{-.58f, .02f}, {0, -.43f}, {.58f, .02f}}, 1.7f);
		Stroke({{-.58f, .42f}, {0, -.03f}, {.58f, .42f}}, 1.7f);
	}
	else if (NodeId.ToString().StartsWith(TEXT("G.Body.")))
	{
		Stroke({{0, -.60f}, {.49f, -.34f}, {.40f, .28f}, {0, .63f}, {-.40f, .28f}, {-.49f, -.34f}, {0, -.60f}}, 1.65f);
		Stroke({{0, -.34f}, {0, .35f}}, 1.2f, .70f);
	}
	else if (NodeId.ToString().StartsWith(TEXT("G.Resonance.")))
	{
		Spiral(.66f, .42f, -2.90f, 2.80f, 1.5f);
		Spiral(.42f, .24f, -2.25f, 2.18f, 1.35f);
	}
	else
	{
		Stroke({{0, -.67f}, {.15f, -.16f}, {.65f, 0}, {.15f, .16f}, {0, .67f}, {-.15f, .16f}, {-.65f, 0}, {-.15f, -.16f}, {0, -.67f}}, 1.5f);
	}
	return ContentLayer + 1;
}
void URogue10mHybridSkillTreeEntryWidget::BuildEntry()
{
	if (NodeButton || !WidgetTree) return;
	// The complete visible frame receives input; the original circular body stays centered.
	SetVisibility(ESlateVisibility::Visible);
	UOverlay* FrameRoot = WidgetTree->ConstructWidget<UOverlay>();
	FrameRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = FrameRoot;
	BodyBox = WidgetTree->ConstructWidget<USizeBox>();
	UOverlaySlot* BodySlot = FrameRoot->AddChildToOverlay(BodyBox);
	BodySlot->SetHorizontalAlignment(HAlign_Center); BodySlot->SetVerticalAlignment(VAlign_Center);
	NodeButton = WidgetTree->ConstructWidget<URogue10mHybridSkillTreeButton>();
	BodyBox->AddChild(NodeButton);
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
	const FRogue10mSkillTreeNodeDefinition& Definition, ERogue10mSkillTreeNodeState State,
	bool bSelected, bool bCurrentTrack, float DiameterOverride)
{
	using namespace Rogue10mHybridTreeUI;
	BuildEntry(); OwnerWidget = Owner; NodeId = Definition.NodeId;
	const bool bKnuckle = Definition.Track == ERogue10mSkillTreeTrack::Brawler;
	const bool bSword = Definition.Track == ERogue10mSkillTreeTrack::GreatSword;
	const bool bCommon = Definition.CommonSkill != ERogue10mCommonSkill::None;
	UTexture2D* CommonArt = bCommon ? CommonMedallionAtlas() : nullptr;
	CommonGlyph = CommonArt ? ERogue10mCommonSkill::None : Definition.CommonSkill;
	const float BodySize = DiameterOverride > 0.0f ? DiameterOverride : NodeDiameter(Definition);
	BodyBox->SetWidthOverride(BodySize); BodyBox->SetHeightOverride(BodySize);
	bHasOuterFrame = bKnuckle || bSword || (bCommon && DiameterOverride <= 0.0f)
		|| HasIdentityFrame(Definition.NodeType) || Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance;
	Skill = Definition.Skill.LoadSynchronous();
	if (!Skill && !Definition.GrantedSkills.IsEmpty()) Skill = Definition.GrantedSkills[0].LoadSynchronous();
	if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic && Owner && Owner->GetSelectedNodeId() == NodeId)
	{
		Skill = Owner->GetSelectedAttackSkill();
	}
	const bool bLearned = IsLearned(State);
	const auto* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	bCanDrag = bCurrentTrack && bLearned && Skill
		&& (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Technique || Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic)
		&& Combat && Combat->IsSelectableActiveTechnique(Skill);
	NodeButton->Configure(Owner, SelectNode, NodeId);
	FLinearColor Accent = NodeColor(Definition);
	if (State == ERogue10mSkillTreeNodeState::Locked) Accent = Muted * 0.55f;
	if (State == ERogue10mSkillTreeNodeState::Available) Accent = FLinearColor::LerpUsingHSV(Accent, Pale, 0.38f);
	FButtonStyle Style = bKnuckle ? KnuckleMedallionStyle(State, bSelected, BodySize * 0.5f)
		: bSword ? SwordNodeStyle(Definition.Identity == ERogue10mSkillTreeIdentity::None ? SwordBrass : NodeColor(Definition), State, bSelected, BodySize * 0.5f)
		: bCommon ? CommonMedallionStyle(NodeColor(Definition), State, bSelected, BodySize * 0.5f, CommonArt != nullptr)
		: ButtonStyle(Accent, bSelected, bLearned, BodySize * 0.5f);
	// The diamond is the identity's resting silhouette. Keep the native circle's fill,
	// hover/press feedback, without a second competing resting border.
	if (HasIdentityFrame(Definition.NodeType)) Style.Normal.OutlineSettings.Width = 0.0f;
	NodeButton->SetStyle(Style);
	NodeButton->SetToolTipText(FText::Format(FText::FromString(TEXT("{0}\n{1} · {2}\n{3}")),
		Definition.DisplayName, StateName(State), CostText(Definition), Definition.Description));
	SetToolTipText(NodeButton->GetToolTipText());
	UTexture2D* Texture = CommonArt ? CommonArt
		: Skill && (bKnuckle || Definition.NodeType != ERogue10mHybridSkillTreeNodeType::Basic) ? Skill->SkillIcon.Get() : nullptr;
	if (bKnuckle && (!Texture || Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic))
	{
		if (UTexture2D* TrainingIcon = KnuckleTrainingIcon(Definition)) Texture = TrainingIcon;
	}
	const bool bSwordHub = bSword && Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic;
	if (bSwordHub)
	{
		Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/HUD/Split/T_Identity_GreatSword.T_Identity_GreatSword"));
	}
	const bool bHasIcon = Texture != nullptr;
	bPaintSwordGlyph = bSword && !bHasIcon;
	GlyphTint = State == ERogue10mSkillTreeNodeState::Locked ? SwordSteel
		: (Definition.Identity == ERogue10mSkillTreeIdentity::None ? Pale : NodeColor(Definition));
	if (bCommon)
	{
		GlyphTint = FLinearColor::LerpUsingHSV(NodeColor(Definition), Pale, bSelected ? 0.42f : 0.20f);
		if (State == ERogue10mSkillTreeNodeState::Locked && !bSelected) GlyphTint *= 0.67f;
		GlyphTint.A = 1.0f;
	}
	if (CommonArt)
	{
		UOverlaySlot* IconSlot = CastChecked<UOverlaySlot>(Icon->Slot);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f));
		Icon->SetBrush(CommonMedallionBrush(*CommonArt, Definition.CommonSkill, BodySize));
		Icon->SetDesiredSizeOverride(FVector2D(BodySize));
	}
	else
	{
		Icon->SetBrushFromTexture(Texture);
	}
	if (bKnuckle || bSword)
	{
		UOverlaySlot* IconSlot = CastChecked<UOverlaySlot>(Icon->Slot);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f));
		const FVector2D ImageSize = bSwordHub ? FVector2D(28.0, 53.0) : FVector2D(bKnuckle ? 52.0 : 43.0);
		if (bSword)
		{
			FSlateBrush ImageBrush = Icon->GetBrush();
			ImageBrush.SetImageSize(FVector2f(ImageSize));
			if (bSwordHub) ImageBrush.SetUVRegion(FBox2f(FVector2f(0.35f, 0.10f), FVector2f(0.66f, 0.90f)));
			Icon->SetBrush(ImageBrush);
		}
		Icon->SetDesiredSizeOverride(ImageSize);
	}
	Icon->SetRenderScale(Definition.NodeId == TEXT("F.Basic.RightJab") ? FVector2D(-1.0, 1.0) : FVector2D(1.0, 1.0));
	const FLinearColor KnuckleIconTint = State == ERogue10mSkillTreeNodeState::Locked
		? FLinearColor(0.70f, 0.53f, 0.32f) : FLinearColor(1.0f, 0.88f, 0.65f);
	if (CommonArt)
	{
		const FLinearColor ArtTint = bSelected || bLearned ? FLinearColor::White
			: State == ERogue10mSkillTreeNodeState::Available ? FLinearColor(0.97f, 0.93f, 0.87f)
			: FLinearColor(0.76f, 0.74f, 0.72f);
		Icon->SetColorAndOpacity(ArtTint);
	}
	else
	{
		Icon->SetColorAndOpacity(bKnuckle ? KnuckleIconTint : (bSwordHub ? FLinearColor::White : (bLearned ? Pale : Muted)));
	}
	Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	FString Glyph = TEXT("+");
	if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Root) Glyph = TEXT("시작");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Basic) Glyph = TEXT("기본");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance) Glyph = TEXT("공명");
	else if (Definition.Identity != ERogue10mSkillTreeIdentity::None) Glyph = IdentityName(Definition.Identity).ToString();
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Passive) Glyph = TEXT("심법");
	else if (Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Technique) Glyph = Definition.Track == ERogue10mSkillTreeTrack::GreatSword ? TEXT("검") : TEXT("권");
	Symbol->SetText(FText::FromString(Glyph));
	SetTextStyle(Symbol, Definition.NodeType == ERogue10mHybridSkillTreeNodeType::Stat ? 17 : 14, Accent);
	Symbol->SetVisibility(bHasIcon || bPaintSwordGlyph || bCommon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
FReply URogue10mHybridSkillTreeEntryWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// Central button input is handled by SButton first. Only the surrounding frame bubbles here.
	if (bHasOuterFrame && !bCanDrag && Event.GetEffectingButton() == EKeys::LeftMouseButton && OwnerWidget.IsValid())
	{
		OwnerWidget->HandleAction(Rogue10mHybridTreeUI::SelectNode, NodeId);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}
void URogue10mHybridSkillTreeEntryWidget::NativeOnDragDetected(const FGeometry& Geometry,
	const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	if (!bCanDrag || !Skill) return;
	URogue10mSkillDragDropOperation* Drag = NewObject<URogue10mSkillDragDropOperation>(this);
	Drag->SkillData = Skill; Drag->bUnlocked = true; Drag->Pivot = EDragPivot::MouseDown;
	if (WidgetTree)
	{
		UBorder* Visual = WidgetTree->ConstructWidget<UBorder>();
		Visual->SetBrushColor(Rogue10mHybridTreeUI::Iron); Visual->SetPadding(FMargin(8));
		Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); Visual->AddChild(Row);
		if (Skill->SkillIcon)
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
			IconSize->SetWidthOverride(36); IconSize->SetHeightOverride(36);
			UImage* DragIcon = WidgetTree->ConstructWidget<UImage>(); DragIcon->SetBrushFromTexture(Skill->SkillIcon);
			IconSize->AddChild(DragIcon); Row->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0, 0, 8, 0));
		}
		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(); Name->SetText(Skill->SkillName);
		Rogue10mHybridTreeUI::SetTextStyle(Name, 16, Rogue10mHybridTreeUI::Pale);
		Row->AddChildToHorizontalBox(Name)->SetVerticalAlignment(VAlign_Center);
		Drag->DefaultDragVisual = Visual;
	}
	Operation = Drag;
	if (OwnerWidget.IsValid()) OwnerWidget->BeginSkillDrag(Drag);
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
	URogue10mSkillTreeProgressionComponent* InProgression, ERogue10mSkillTreeTab InTab,
	ERogue10mSkillTreeTrack ContextTrack, FName SelectedNode)
{
	using namespace Rogue10mHybridTreeUI;
	TArray<FRogue10mSkillTreeNodeDefinition> NewNodes = Owner && InProgression
		? Owner->GetDisplayNodes() : TArray<FRogue10mSkillTreeNodeDefinition>();
	for (FRogue10mSkillTreeNodeDefinition& Node : NewNodes)
	{
		Node.Position = LayoutPosition(Node, InTab) - Center;
	}
	bool bRebuild = Tab != InTab || Nodes.Num() != NewNodes.Num();
	if (!bRebuild)
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].NodeId != NewNodes[Index].NodeId || !Nodes[Index].Position.Equals(NewNodes[Index].Position)
				|| Nodes[Index].NodeType != NewNodes[Index].NodeType)
			{
				bRebuild = true;
				break;
			}
		}
	}
	Progression = InProgression;
	Tab = InTab;
	Selected = SelectedNode;
	Nodes = MoveTemp(NewNodes);
	if (!Canvas) { TakeWidget(); bRebuild = true; }
	if (bRebuild) BuildGraph();

	States.Reset(Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FRogue10mSkillTreeNodeDefinition& Node = Nodes[Index];
		const ERogue10mSkillTreeNodeState State = DisplayState(*InProgression, Node, ContextTrack);
		States.Add(State);
		const bool bCurrentTrack = Node.Track != ERogue10mSkillTreeTrack::None && InProgression->GetActiveTrack() == Node.Track;
		if (Entries.IsValidIndex(Index) && Entries[Index])
		{
			Entries[Index]->Configure(Owner, Node, State, Selected == Node.NodeId, bCurrentTrack);
		}
		if (Labels.IsValidIndex(Index) && Labels[Index])
		{
			const bool bKnuckle = Tab == ERogue10mSkillTreeTab::Knuckle;
			const bool bSword = Tab == ERogue10mSkillTreeTab::Sword;
			const bool bCommon = Node.CommonSkill != ERogue10mCommonSkill::None;
			FText Label = Node.DisplayName;
			if (bSword && Node.PointCost > 0 && !IsLearned(State))
			{
				Label = FText::Format(NSLOCTEXT("Rogue10mTree", "SwordNodePrice", "{0} · {1}P"), Node.DisplayName, Node.PointCost);
			}
			else if (!bKnuckle && !bSword && !bCommon)
			{
				Label = FText::Format(FText::FromString(TEXT("{0}\n{1}")), Node.DisplayName, GraphCaption(Node, State));
			}
			Labels[Index]->SetText(Label);
			const FLinearColor LockedText = bKnuckle ? FLinearColor(0.64f, 0.51f, 0.36f)
				: bSword ? FLinearColor(0.52f, 0.57f, 0.60f)
				: bCommon ? FLinearColor(0.78f, 0.69f, 0.55f) : Muted;
			const FLinearColor OpenText = bCommon ? FLinearColor(1.0f, 0.91f, 0.73f) : Pale;
			Labels[Index]->SetColorAndOpacity(FSlateColor(State == ERogue10mSkillTreeNodeState::Locked && Selected != Node.NodeId ? LockedText : OpenText));
		}
	}
	InvalidateLayoutAndVolatility();
}

void URogue10mHybridSkillTreeGraphWidget::BuildGraph()
{
	using namespace Rogue10mHybridTreeUI;
	if (!Canvas || !WidgetTree) return;
	Canvas->ClearChildren();
	Entries.Reset();
	Labels.Reset();
	Entries.SetNumZeroed(Nodes.Num());
	Labels.SetNumZeroed(Nodes.Num());
	if (Nodes.IsEmpty()) return;

	const bool bKnuckle = Tab == ERogue10mSkillTreeTab::Knuckle;
	const bool bSword = Tab == ERogue10mSkillTreeTab::Sword;
	const bool bCommon = Nodes[0].CommonSkill != ERogue10mCommonSkill::None;
	const FVector2D CanvasSize = bCommon ? CommonGraphSize : GraphSize;
	if (bCommon)
	{
		UBorder* Field = WidgetTree->ConstructWidget<UBorder>();
		FSlateBrush Brush = FSlateRoundedBoxBrush(FLinearColor(0.0065f, 0.0060f, 0.0055f), 0.0f);
		if (UTexture2D* Charcoal = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/UI/CommonSkillTree/T_CommonSkillCharcoal.T_CommonSkillCharcoal"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Brush = FSlateBrush();
			Brush.SetResourceObject(Charcoal);
			Brush.SetImageSize(FVector2f(CanvasSize));
		}
		Field->SetBrush(Brush);
		Field->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* FieldSlot = Canvas->AddChildToCanvas(Field);
		FieldSlot->SetSize(CanvasSize);
		FieldSlot->SetZOrder(-1);
	}
	else if (bKnuckle || bSword)
	{
		if (UTexture2D* Frame = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/MartialArts/T_UI_MartialArtsSkillTreeFrame.T_UI_MartialArtsSkillTreeFrame")))
		{
			UImage* Texture = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush Brush;
			Brush.SetResourceObject(Frame);
			Brush.ImageSize = FVector2f(GraphSize);
			Brush.SetUVRegion(FBox2f(FVector2f(0.215f, 0.075f), FVector2f(0.785f, 0.935f)));
			Texture->SetBrush(Brush);
			Texture->SetRenderOpacity(bKnuckle ? 0.34f : 0.18f);
			Texture->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* TextureSlot = Canvas->AddChildToCanvas(Texture);
			TextureSlot->SetSize(GraphSize);
			TextureSlot->SetZOrder(-1);
		}
	}

	URogue10mHybridSkillTreeConnectionsWidget* Connections = CreateWidget<URogue10mHybridSkillTreeConnectionsWidget>(GetOwningPlayer());
	Connections->SetGraph(this);
	Connections->SetVisibility(ESlateVisibility::HitTestInvisible);
	Canvas->AddChildToCanvas(Connections)->SetSize(CanvasSize);

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FRogue10mSkillTreeNodeDefinition& Node = Nodes[Index];
		URogue10mHybridSkillTreeEntryWidget* Entry = CreateWidget<URogue10mHybridSkillTreeEntryWidget>(GetOwningPlayer());
		if (!Entry) continue;
		Entries[Index] = Entry;
		const FVector2D Location = Position(Node);
		const float FrameExtent = NodeFrame(Node).Extent;
		UCanvasPanelSlot* EntrySlot = Canvas->AddChildToCanvas(Entry);
		EntrySlot->SetPosition(Location);
		EntrySlot->SetAlignment(FVector2D(0.5f));
		EntrySlot->SetSize(FVector2D(FrameExtent * 2.0f));
		EntrySlot->SetZOrder(3);

		if (!bKnuckle && !bSword && !bCommon
			&& (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Root || Node.NodeType == ERogue10mHybridSkillTreeNodeType::Basic)) continue;

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Labels[Index] = Label;
		const bool bMainSwordNode = bSword && (Node.NodeType == ERogue10mHybridSkillTreeNodeType::Basic || Node.NodeType == ERogue10mHybridSkillTreeNodeType::Technique);
		if (bCommon)
		{
			const bool bFoundation = Node.CommonSkill == ERogue10mCommonSkill::Foundation;
			Rogue10mCommonSkillChrome::StyleText(*Label, bFoundation ? 24 : 18, true);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			SetTextStyle(Label, bKnuckle || bMainSwordNode ? 16 : 15, Pale);
		}
		if (Node.CommonSkill == ERogue10mCommonSkill::EvadeRecovery)
		{
			Label->SetText(NSLOCTEXT("Rogue10mTree", "EvadeRecoveryPlaque", "회피 후\n회복 단축"));
		}
		else if (Node.CommonSkill == ERogue10mCommonSkill::RollCooldown)
		{
			Label->SetText(NSLOCTEXT("Rogue10mTree", "RollCooldownPlaque", "구르기\n재사용 단축"));
		}
		Label->SetWrapTextAt(bCommon ? 0.0f : 156.0f);
		Label->SetJustification(ETextJustify::Center);
		UCanvasPanelSlot* LabelSlot;
		if (bCommon)
		{
			UOverlay* Plaque = WidgetTree->ConstructWidget<UOverlay>();
			Plaque->SetVisibility(ESlateVisibility::HitTestInvisible);
			UNativeWidgetHost* Surface = WidgetTree->ConstructWidget<UNativeWidgetHost>();
			Surface->SetContent(Rogue10mCommonSkillChrome::MakeSurface(Rogue10mCommonSkillChrome::ESurface::Plaque));
			UOverlaySlot* SurfaceSlot = Plaque->AddChildToOverlay(Surface);
			SurfaceSlot->SetHorizontalAlignment(HAlign_Fill);
			SurfaceSlot->SetVerticalAlignment(VAlign_Fill);
			Plaque->AddChildToOverlay(Label)->SetPadding(FMargin(12.0f, 3.0f));
			LabelSlot = Canvas->AddChildToCanvas(Plaque);
			LabelSlot->SetAutoSize(true);
			LabelSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			LabelSlot->SetPosition(Location + FVector2D(0.0, CommonVisualRadius(Node) - 11.0f));
		}
		else if (bKnuckle || bSword)
		{
			UBorder* Plaque = WidgetTree->ConstructWidget<UBorder>();
			const FLinearColor Background = bKnuckle ? FLinearColor(0.013f, 0.010f, 0.007f) : FLinearColor(0.012f, 0.014f, 0.015f);
			const FLinearColor FrameColor = bKnuckle ? FLinearColor(0.15f, 0.090f, 0.039f)
				: bMainSwordNode ? FLinearColor(0.23f, 0.14f, 0.065f) : FLinearColor(0.12f, 0.14f, 0.15f);
			Plaque->SetBrush(FSlateRoundedBoxBrush(Background, 4.0f, FrameColor, 0.7f));
			Plaque->SetVisibility(ESlateVisibility::HitTestInvisible);
			Plaque->SetPadding(FMargin(6.0f, 2.0f));
			Plaque->AddChild(Label);
			LabelSlot = Canvas->AddChildToCanvas(Plaque);
			LabelSlot->SetAutoSize(true);
			const bool bSideLabel = bSword && Node.NodeType == ERogue10mHybridSkillTreeNodeType::Root;
			LabelSlot->SetAlignment(bSideLabel ? FVector2D(0.0, 0.5) : FVector2D(0.5, 0.0));
			const double LabelGap = bMainSwordNode ? -3.0 : 6.0;
			LabelSlot->SetPosition(Location + (bSideLabel ? FVector2D(FrameExtent + 9.0, 0.0) : FVector2D(0.0, FrameExtent + LabelGap)));
		}
		else
		{
			LabelSlot = Canvas->AddChildToCanvas(Label);
			LabelSlot->SetPosition(Location + FVector2D(-78.0, FrameExtent + 6.0));
			LabelSlot->SetSize(FVector2D(156.0, 50.0));
		}
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
namespace Rogue10mHybridTreeUI
{
	TArray<FVector2f> DependencyPath(const FRogue10mSkillTreeNodeDefinition& Parent,
		const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTab Tab)
	{
		const FVector2D Start = Position(Parent);
		const FVector2D End = Position(Node);
		TArray<FVector2D> Path = {Start};
		const bool bCommon = Node.CommonSkill != ERogue10mCommonSkill::None;

		if (bCommon && (Parent.CommonSkill == ERogue10mCommonSkill::EvadeHaste || Node.CommonSkill == ERogue10mCommonSkill::EvadeHaste))
		{
			// The long lower connections bend beneath the diamond-shaped branches.
			const FVector2D Control = (Start + End) * 0.5 + FVector2D(0.0, 79.0);
			for (int32 Step = 1; Step < 24; ++Step)
			{
				const double Progress = static_cast<double>(Step) / 24.0;
				Path.Add(FMath::Lerp(FMath::Lerp(Start, Control, Progress), FMath::Lerp(Control, End, Progress), Progress));
			}
			// Shorten the curve at the circles instead of pulling just its first/last samples inward.
			const double StartExtent = CommonVisualRadius(Parent) - 1.0;
			const double EndExtent = CommonVisualRadius(Node) - 1.0;
			Path.RemoveAll([&](const FVector2D& Point)
			{
				return FVector2D::DistSquared(Point, Start) < FMath::Square(StartExtent)
					|| FVector2D::DistSquared(Point, End) < FMath::Square(EndExtent);
			});
			if (!Path.IsEmpty())
			{
				Path.Insert(Start + (Path[0] - Start).GetSafeNormal() * StartExtent, 0);
				Path.Add(End + (Path.Last() - End).GetSafeNormal() * EndExtent);
				TArray<FVector2f> Curve;
				Curve.Reserve(Path.Num());
				for (const FVector2D& Point : Path) Curve.Add(FVector2f(Point));
				return Curve;
			}
			Path.Add(Start);
		}
		else if (Tab == ERogue10mSkillTreeTab::Sword)
		{
			const bool bToUltimate = Node.NodeId == TEXT("G.Ultimate");
			// Keep long links clear of controls on the inner rings.
			if (Parent.NodeId == TEXT("G.Whirlwind") && (bToUltimate || Node.NodeId == TEXT("G.EarthSplitter")))
			{
				const double Side = bToUltimate ? -115.0 : 115.0;
				Path.Append({{Start.X + Side, Start.Y - 95.0}, {End.X + Side, End.Y + 48.0}});
			}
			else if (bToUltimate && Parent.NodeId == TEXT("G.Flow.3"))
			{
				Path.Append({{175.0, 307.0}, {408.0, 119.0}});
			}
			else if (bToUltimate && Parent.NodeId == TEXT("G.Body.3"))
			{
				Path.Append({{929.0, 304.0}, {671.0, 115.0}});
			}
			else if (Parent.NodeId == TEXT("G.Ultimate") && Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance
				&& FMath::Abs(End.X - Start.X) > 300.0)
			{
				const double Side = FMath::Sign(End.X - Start.X);
				Path.Append({{Start.X + 82.0 * Side, 14.0}, {End.X - 40.0 * Side, 14.0}});
			}
		}

		Path.Add(End);
		const double EdgePadding = Tab == ERogue10mSkillTreeTab::Knuckle || Tab == ERogue10mSkillTreeTab::Sword ? -2.0 : 2.0;
		const double StartExtent = bCommon ? CommonVisualRadius(Parent) - 1.0 : NodeFrame(Parent).Extent + EdgePadding;
		const double EndExtent = bCommon ? CommonVisualRadius(Node) - 1.0 : NodeFrame(Node).Extent + EdgePadding;
		Path[0] += (Path[1] - Start).GetSafeNormal() * StartExtent;
		Path.Last() -= (End - Path[Path.Num() - 2]).GetSafeNormal() * EndExtent;
		TArray<FVector2f> Points;
		Points.Reserve(Path.Num());
		for (const FVector2D& Point : Path) Points.Add(FVector2f(Point));
		return Points;
	}
}

int32 URogue10mHybridSkillTreeGraphWidget::PaintConnections(const FGeometry& Geometry,
	FSlateWindowElementList& Elements, int32 LayerId) const
{
	using namespace Rogue10mHybridTreeUI;
	using EState = ERogue10mSkillTreeNodeState;
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
	const bool bKnuckle = Tab == ERogue10mSkillTreeTab::Knuckle;
	const bool bSword = Tab == ERogue10mSkillTreeTab::Sword;
	if (bSword)
	{
		for (const FVector2D& Radius : {FVector2D(132, 112), FVector2D(244, 192), FVector2D(375, 273)})
		{
			TArray<FVector2f> Guide;
			Guide.Reserve(73);
			for (int32 Step = 0; Step <= 72; ++Step)
			{
				const double Angle = 2.0 * UE_DOUBLE_PI * Step / 72.0;
				Guide.Add(FVector2f(SwordHub + FVector2D(Radius.X * FMath::Cos(Angle), Radius.Y * FMath::Sin(Angle))));
			}
			FSlateDrawElement::MakeLines(Elements, LayerId, PaintGeometry, Guide, ESlateDrawEffect::None,
				FLinearColor(0.19f, 0.23f, 0.25f, 0.38f), true, 0.9f);
		}
	}

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FRogue10mSkillTreeNodeDefinition& Node = Nodes[Index];
		const EState State = States.IsValidIndex(Index) ? States[Index] : EState::Locked;
		if (Node.CommonSkill != ERogue10mCommonSkill::None)
		{
			for (FName NeighborId : Node.CommonConnections)
			{
				const int32 NeighborIndex = Nodes.IndexOfByPredicate([NeighborId](const auto& Item) { return Item.NodeId == NeighborId; });
				if (NeighborIndex <= Index || !States.IsValidIndex(NeighborIndex)) continue;
				const FRogue10mSkillTreeNodeDefinition& Neighbor = Nodes[NeighborIndex];
				const EState NeighborState = States[NeighborIndex];
				const bool bLearnedPath = IsLearned(State) && IsLearned(NeighborState);
				const bool bReadyPath = (IsLearned(State) && NeighborState == EState::Available)
					|| (IsLearned(NeighborState) && State == EState::Available);
				TArray<FVector2f> Points = DependencyPath(Node, Neighbor, Tab);
				const FLinearColor Core = bLearnedPath ? FLinearColor(1.0f, 0.84f, 0.50f)
					: bReadyPath ? FLinearColor(0.87f, 0.53f, 0.22f) : FLinearColor(0.64f, 0.37f, 0.18f);
				FSlateDrawElement::MakeLines(Elements, LayerId, PaintGeometry, Points, ESlateDrawEffect::None,
					FLinearColor(0.015f, 0.006f, 0.002f, 0.96f), true, 7.0f);
				if (bLearnedPath)
				{
					FSlateDrawElement::MakeLines(Elements, LayerId, PaintGeometry, Points, ESlateDrawEffect::None,
						FLinearColor(1.0f, 0.39f, 0.04f, 0.11f), true, 18.0f);
					FSlateDrawElement::MakeLines(Elements, LayerId, PaintGeometry, Points, ESlateDrawEffect::None,
						FLinearColor(1.0f, 0.44f, 0.06f, 0.25f), true, 10.0f);
				}
				FSlateDrawElement::MakeLines(Elements, LayerId + 1, PaintGeometry, Points, ESlateDrawEffect::None,
					Core * FLinearColor(0.51f, 0.37f, 0.27f, 1.0f), true, bLearnedPath ? 5.4f : 4.1f);
				FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry, MoveTemp(Points), ESlateDrawEffect::None,
					Core, true, bLearnedPath ? 2.65f : 1.45f);
			}

			const bool bSelected = Node.NodeId == Selected;
			if (bSelected || IsLearned(State))
			{
				const FVector2f Location(Position(Node));
				TArray<FVector2f> Rim = CirclePoints(Location, CommonVisualRadius(Node) + (bSelected ? 3.0f : 1.4f));
				FSlateDrawElement::MakeLines(Elements, LayerId + 1, PaintGeometry, Rim, ESlateDrawEffect::None,
					FLinearColor(1.0f, 0.38f, 0.025f, bSelected ? 0.11f : 0.038f), true, bSelected ? 22.0f : 12.0f);
				FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry, Rim, ESlateDrawEffect::None,
					FLinearColor(1.0f, 0.43f, 0.06f, bSelected ? 0.28f : 0.11f), true, bSelected ? 11.0f : 6.0f);
				FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry, MoveTemp(Rim), ESlateDrawEffect::None,
					FLinearColor(1.0f, 0.72f, 0.29f, bSelected ? 0.97f : 0.63f), true, bSelected ? 3.2f : 1.7f);
			}
			continue;
		}
		TMap<FName, bool> ReadyByParent;
		for (const FRogue10mSkillTreePrerequisiteGroup& Group : Node.PrerequisiteGroups)
		{
			const bool bGroupReady = Progression.IsValid() && !Group.NodeIds.ContainsByPredicate([this](FName ParentId)
			{
				return !Progression->IsNodeUnlocked(ParentId);
			});
			for (const FName ParentId : Group.NodeIds)
			{
				ReadyByParent.FindOrAdd(ParentId) |= bGroupReady;
			}
		}

		for (const TPair<FName, bool>& Pair : ReadyByParent)
		{
			const FRogue10mSkillTreeNodeDefinition* Parent = Nodes.FindByPredicate([&Pair](const auto& Item) { return Item.NodeId == Pair.Key; });
			if (!Parent) continue;
			const bool bInvestedPath = Pair.Value && IsLearned(State);
			const bool bAvailablePath = Pair.Value && State == EState::Available;
			TArray<FVector2f> Points = DependencyPath(*Parent, Node, Tab);
			if (bKnuckle)
			{
				const bool bOpenPath = bInvestedPath || bAvailablePath;
				const FLinearColor Glow(0.55f, 0.23f, 0.04f, bOpenPath ? 0.22f : 0.10f);
				FSlateDrawElement::MakeLines(Elements, LayerId, PaintGeometry, Points, ESlateDrawEffect::None, Glow, true, 5.0f);
				FSlateDrawElement::MakeLines(Elements, LayerId + 1, PaintGeometry, Points, ESlateDrawEffect::None,
					bOpenPath ? KnuckleLight : KnuckleBrass, true, bOpenPath ? 2.7f : 1.9f);
				continue;
			}
			FLinearColor Color = bInvestedPath ? NodeColor(Node) : bAvailablePath
				? FLinearColor::LerpUsingHSV(NodeColor(Node), Pale, 0.4f)
				: FLinearColor(0.25f, 0.25f, 0.23f, 0.84f);
			float Thickness = bInvestedPath ? 2.7f : bAvailablePath ? 2.2f : 1.3f;
			int32 RouteLayer = LayerId + 1;
			if (bSword)
			{
				const bool bFocused = Node.NodeId == Selected || Parent->NodeId == Selected;
				const bool bResonant = Node.Identity != ERogue10mSkillTreeIdentity::None;
				if (bInvestedPath)
				{
					Color = bResonant ? NodeColor(Node) : (bFocused ? KnuckleLight : SwordBrass);
					Color.A = bFocused ? 1.0f : 0.85f;
					Thickness = bFocused ? 2.4f : 1.45f;
				}
				else if (bAvailablePath)
				{
					Color = bFocused ? FLinearColor(0.69f, 0.78f, 0.84f) : SwordSteel;
					Thickness = bFocused ? 2.2f : 1.45f;
				}
				else
				{
					Color = bFocused ? FLinearColor(0.33f, 0.36f, 0.40f, 0.84f) : FLinearColor(0.22f, 0.25f, 0.28f, 0.56f);
					Thickness = bFocused ? 1.4f : 1.0f;
				}
				if (bFocused && (bInvestedPath || bAvailablePath)) RouteLayer = LayerId + 2;
			}
			const bool bAlternative = Node.PrerequisiteGroups.ContainsByPredicate([&Pair](const auto& Group)
			{
				return !Group.NodeIds.Contains(Pair.Key);
			});
			if (bAlternative)
			{
				FSlateDrawElement::MakeDashedLines(Elements, RouteLayer, PaintGeometry, MoveTemp(Points),
					ESlateDrawEffect::None, Color, Thickness, bSword ? 5.0f : 7.0f);
			}
			else
			{
				FSlateDrawElement::MakeLines(Elements, RouteLayer, PaintGeometry, Points, ESlateDrawEffect::None,
					Color, true, Thickness);
			}
		}

		FNodeFrame Frame = NodeFrame(Node);
		if (Frame.OuterPoints.IsEmpty()) continue;
		const bool bSelected = Node.NodeId == Selected;
		FLinearColor Color = bSelected ? Pale : (State == EState::Locked ? Muted * 0.55f : NodeColor(Node));
		if (bKnuckle)
		{
			const FLinearColor Highlight = bSelected || State != EState::Locked ? KnuckleLight : KnuckleBrass;
			FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry,
				CirclePoints(FVector2f(Position(Node)), NodeDiameter(Node) * 0.5f + 3.0f), ESlateDrawEffect::None,
				Highlight, true, bSelected ? 2.4f : 1.6f);
			Color = bSelected ? KnuckleLight : KnuckleBronze;
		}
		else if (bSword)
		{
			const bool bResonance = Node.NodeType == ERogue10mHybridSkillTreeNodeType::Resonance;
			Color = bSelected ? KnuckleLight : State == EState::Locked ? SwordSteel * 0.65f
				: bResonance ? NodeColor(Node) : SwordBrass;
			if (Frame.OutlineCount == 3)
			{
				const FLinearColor Highlight = bSelected ? KnuckleLight : State == EState::Locked ? SwordSteel : KnuckleBrass;
				FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry,
					CirclePoints(FVector2f(Position(Node)), NodeDiameter(Node) * 0.5f + 3.0f), ESlateDrawEffect::None,
					Highlight, true, bSelected ? 2.8f : 1.25f);
			}
		}
		FSlateDrawElement::MakeLines(Elements, LayerId + 2, PaintGeometry, MoveTemp(Frame.OuterPoints),
			ESlateDrawEffect::None, Color, true, bSelected ? 2.5f : 1.5f);
	}
	return LayerId + 3;
}
URogue10mHybridSkillTreeEntryWidget* URogue10mHybridSkillTreeGraphWidget::FindNodeEntry(FName NodeId) const
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (Nodes[Index].NodeId == NodeId && Entries.IsValidIndex(Index)) return Entries[Index].Get();
	}
	return nullptr;
}
FName URogue10mHybridSkillTreeGraphWidget::GetNodeFrameShape(FName NodeId) const
{
	const auto* Node = Nodes.FindByPredicate([NodeId](const auto& Item) { return Item.NodeId == NodeId; });
	return Node ? Rogue10mHybridTreeUI::NodeFrame(*Node).Shape : NAME_None;
}
int32 URogue10mHybridSkillTreeGraphWidget::GetNodeFrameOutlineCount(FName NodeId) const
{
	const auto* Node = Nodes.FindByPredicate([NodeId](const auto& Item) { return Item.NodeId == NodeId; });
	return Node ? Rogue10mHybridTreeUI::NodeFrame(*Node).OutlineCount : 0;
}
TArray<FVector2f> URogue10mHybridSkillTreeGraphWidget::GetNodeOuterFramePoints(FName NodeId) const
{
	const auto* Node = Nodes.FindByPredicate([NodeId](const auto& Item) { return Item.NodeId == NodeId; });
	return Node ? Rogue10mHybridTreeUI::NodeFrame(*Node).OuterPoints : TArray<FVector2f>();
}

namespace Rogue10mHybridTreeUI
{
	constexpr ERogue10mSkillTreeTab DefaultTabOrder[] = {
		ERogue10mSkillTreeTab::Common, ERogue10mSkillTreeTab::Knuckle,
		ERogue10mSkillTreeTab::Sword, ERogue10mSkillTreeTab::Magic
	};

	ERogue10mSkillTreeTab TabForTrack(ERogue10mSkillTreeTrack Track)
	{
		switch (Track)
		{
		case ERogue10mSkillTreeTrack::Brawler: return ERogue10mSkillTreeTab::Knuckle;
		case ERogue10mSkillTreeTrack::GreatSword: return ERogue10mSkillTreeTab::Sword;
		default: return ERogue10mSkillTreeTab::Common;
		}
	}

	ERogue10mSkillTreeTab TabForWeapon(ERogue10mWeaponType Weapon)
	{
		switch (Weapon)
		{
		case ERogue10mWeaponType::Knuckle: return ERogue10mSkillTreeTab::Knuckle;
		case ERogue10mWeaponType::GreatSword:
		case ERogue10mWeaponType::LongSword:
		case ERogue10mWeaponType::DualBlades:
		case ERogue10mWeaponType::SwordBuckler: return ERogue10mSkillTreeTab::Sword;
		case ERogue10mWeaponType::Staff: return ERogue10mSkillTreeTab::Magic;
		default: return ERogue10mSkillTreeTab::Common;
		}
	}

	FText TabLabel(ERogue10mSkillTreeTab Tab)
	{
		switch (Tab)
		{
		case ERogue10mSkillTreeTab::Knuckle: return NSLOCTEXT("Rogue10mTree", "KnuckleTab", "너클");
		case ERogue10mSkillTreeTab::Sword: return NSLOCTEXT("Rogue10mTree", "SwordTab", "검");
		case ERogue10mSkillTreeTab::Magic: return NSLOCTEXT("Rogue10mTree", "MagicTab", "마법");
		default: return NSLOCTEXT("Rogue10mTree", "CommonTab", "공통");
		}
	}
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
	if (bActive)
	{
		bSelectEquippedTabOnRefresh = true;
		InitializeTree();
	}
	else { CancelOwnedSkillDrag(); CancelKeySelection(); bRefundConfirmation = false; }
}
void URogue10mHybridSkillTreeWidget::BeginSkillDrag(UDragDropOperation* Operation)
{
	if (!Operation) return;
	RestoreSkillDragPresentation();
	CancelKeySelection();
	VisibilityBeforeSkillDrag = GetVisibility(); OpacityBeforeSkillDrag = GetRenderOpacity();
	ActiveSkillDrag = Operation; bSkillDragPresentation = true;
	Operation->OnDrop.AddUniqueDynamic(this, &ThisClass::HandleSkillDragEnded);
	Operation->OnDragCancelled.AddUniqueDynamic(this, &ThisClass::HandleSkillDragEnded);
	// Keep the menu/input-blocking state, but expose the actual HUD drop targets below it.
	SetRenderOpacity(OpacityBeforeSkillDrag * 0.12f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
void URogue10mHybridSkillTreeWidget::HandleSkillDragEnded(UDragDropOperation* Operation)
{
	if (ActiveSkillDrag.Get() == Operation) RestoreSkillDragPresentation();
}
void URogue10mHybridSkillTreeWidget::CancelOwnedSkillDrag()
{
	if (ActiveSkillDrag.IsValid() && FSlateApplication::IsInitialized())
	{
		const TSharedPtr<FDragDropOperation> Content = FSlateApplication::Get().GetDragDroppingContent();
		if (Content.IsValid() && Content->IsOfType<FUMGDragDropOp>()
			&& StaticCastSharedPtr<FUMGDragDropOp>(Content)->GetOperation() == ActiveSkillDrag.Get())
		{
			FSlateApplication::Get().CancelDragDrop();
		}
	}
	// Cancellation can synchronously broadcast OnDragCancelled and already restore us.
	RestoreSkillDragPresentation();
}
void URogue10mHybridSkillTreeWidget::RestoreSkillDragPresentation()
{
	if (UDragDropOperation* Operation = ActiveSkillDrag.Get())
	{
		Operation->OnDrop.RemoveDynamic(this, &ThisClass::HandleSkillDragEnded);
		Operation->OnDragCancelled.RemoveDynamic(this, &ThisClass::HandleSkillDragEnded);
	}
	ActiveSkillDrag.Reset();
	if (!bSkillDragPresentation) return;
	bSkillDragPresentation = false;
	SetRenderOpacity(OpacityBeforeSkillDrag); SetVisibility(VisibilityBeforeSkillDrag);
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
	using namespace Rogue10mCommonSkillChrome;
	if (Graph || !WidgetTree) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
	auto AddBackdrop = [this, Canvas](UWidget* Backdrop, int32 ZOrder)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Backdrop);
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0.0f));
		Slot->SetZOrder(ZOrder);
		Backdrop->SetVisibility(ESlateVisibility::Collapsed);
		CommonChromeWidgets.Add(Backdrop);
	};
	UBackgroundBlur* WorldBlur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	WorldBlur->SetBlurStrength(10.0f);
	AddBackdrop(WorldBlur, -2);
	UBorder* Veil = WidgetTree->ConstructWidget<UBorder>();
	Veil->SetBrushColor(FLinearColor(0.006f, 0.003f, 0.001f, 0.73f));
	AddBackdrop(Veil, -1);

	UTexture2D* Charcoal = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/UI/CommonSkillTree/T_CommonSkillCharcoal.T_CommonSkillCharcoal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto MakeChrome = [this](ESurface Surface, UTexture2D* Texture = nullptr)
	{
		UNativeWidgetHost* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>();
		Host->SetContent(MakeSurface(Surface, Texture));
		Host->SetVisibility(ESlateVisibility::Collapsed);
		CommonChromeWidgets.Add(Host);
		return Host;
	};
	const auto Layer = [](UOverlay* Overlay, UWidget* Content)
	{
		UOverlaySlot* Slot = Overlay->AddChildToOverlay(Content);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
		return Slot;
	};
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
	UCanvasPanelSlot* ScaleSlot = Canvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0.02f, 0.02f, 0.98f, 0.98f)); ScaleSlot->SetOffsets(FMargin(0));
	WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	Scale->AddChild(WindowSize);
	UOverlay* Window = WidgetTree->ConstructWidget<UOverlay>(); WindowSize->AddChild(Window);
	Layer(Window, MakeChrome(ESurface::Window, Charcoal));
	WindowRim = WidgetTree->ConstructWidget<UBorder>(); WindowRim->SetPadding(FMargin(2)); Layer(Window, WindowRim);
	WindowInterior = WidgetTree->ConstructWidget<UBorder>(); WindowInterior->SetPadding(FMargin(18, 10)); WindowRim->AddChild(WindowInterior);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); WindowInterior->AddChild(Rows);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 7));
	UTextBlock* Title = MakeText(TEXT("HybridTreeTitle"), FText::FromString(TEXT("ROGUE10M  ·  스킬트리")), 28, Gold);
	Title->SetWrapTextAt(1200); Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CommonGrowthText = MakeText(TEXT("HybridTreeCommonGrowth"), FText::GetEmpty(), 17, BurnishedGold);
	CommonGrowthText->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* BalanceSlot = Header->AddChildToHorizontalBox(CommonGrowthText);
	BalanceSlot->SetPadding(FMargin(0, 0, 14, 0));
	BalanceSlot->SetVerticalAlignment(VAlign_Center);
	Header->AddChildToHorizontalBox(MakeButton(TEXT("HybridTreeClose"), FText::FromString(TEXT("닫기 · K")), Close));
	TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HybridTreeTabs"));
	Rows->AddChildToVerticalBox(TabBar)->SetPadding(FMargin(0, 0, 0, 9));
	CommonTab = MakeButton(TEXT("HybridTreeCommon"), TabLabel(ERogue10mSkillTreeTab::Common), Common);
	FighterTab = MakeButton(TEXT("HybridTreeFighter"), TabLabel(ERogue10mSkillTreeTab::Knuckle), Fighter);
	GreatSwordTab = MakeButton(TEXT("HybridTreeGreatSword"), TabLabel(ERogue10mSkillTreeTab::Sword), GreatSword);
	MagicTab = MakeButton(TEXT("HybridTreeMagic"), TabLabel(ERogue10mSkillTreeTab::Magic), Magic);
	TreeBody = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(TreeBody)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MagicPlaceholder = MakeText(TEXT("HybridTreeMagicPlaceholder"),
		NSLOCTEXT("Rogue10mTree", "MagicTreePending", "마법 스킬트리는 준비 중입니다."), 22, Muted);
	MagicPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* PlaceholderSlot = Rows->AddChildToVerticalBox(MagicPlaceholder);
	PlaceholderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
	PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
	LeftColumnSize = WidgetTree->ConstructWidget<USizeBox>(); TreeBody->AddChildToHorizontalBox(LeftColumnSize);
	UOverlay* DetailPanel = WidgetTree->ConstructWidget<UOverlay>(); LeftColumnSize->AddChild(DetailPanel);
	Layer(DetailPanel, MakeChrome(ESurface::Detail, Charcoal));
	UVerticalBox* LeftRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HybridTreeLeftColumn"));
	DetailPanelSlot = Layer(DetailPanel, LeftRoot);
	UVerticalBox* Left = LeftRoot;
	auto AddText = [this, &Left](const FName Name, const TCHAR* Value, int32 FontSize, FLinearColor Color)
	{
		UTextBlock* Text = MakeText(Name, FText::FromString(Value), FontSize, Color);
		Left->AddChildToVerticalBox(Text)->SetPadding(FMargin(2, 4)); return Text;
	};
	GrowthText = AddText(TEXT("HybridTreeGrowth"), TEXT("캐릭터 성장"), 21, Pale);
	ContextText = AddText(TEXT("HybridTreeContext"), TEXT(""), 15, Muted);
	CommonViewToggle = MakeButton(TEXT("HybridTreeCommonViewToggle"),
		NSLOCTEXT("Rogue10mTree", "OpenCommonIdentity", "아이덴티티"), ToggleCommonView);
	CommonViewToggle->SetVisibility(ESlateVisibility::Collapsed);
	MasterySection = WidgetTree->ConstructWidget<UVerticalBox>();
	LeftRoot->AddChildToVerticalBox(MasterySection);
	Left = MasterySection;
	MasteryText = AddText(TEXT("HybridTreeMastery"), TEXT("숙련 M1"), 20, Gold);
	MasteryBar = WidgetTree->ConstructWidget<UProgressBar>(); MasteryBar->SetFillColorAndOpacity(Gold);
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(); BarSize->SetHeightOverride(9); BarSize->AddChild(MasteryBar);
	Left->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(2, 4, 2, 9));
	AddText(TEXT("HybridTreeMasteryStatsHeading"), TEXT("숙련 성장"), 16, Gold);
	MasteryStatsText = AddText(TEXT("HybridTreeMasteryStats"), TEXT(""), 16, Pale);
	Left = LeftRoot;
	IdentityText = AddText(TEXT("HybridTreeIdentity"), TEXT("활성 아이덴티티 · 미활성"), 18, Green);
	// Only the long description and requirements scroll. Actions are a separate bottom slot.
	UScrollBox* DetailsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HybridTreeDetailsScroll"));
	LeftRoot->AddChildToVerticalBox(DetailsScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Left = WidgetTree->ConstructWidget<UVerticalBox>(); DetailsScroll->AddChild(Left);
	DetailHeading = AddText(NAME_None, TEXT("선택 노드"), 17, Gold);
	DetailName = AddText(TEXT("HybridTreeDetailName"), TEXT(""), 23, Pale);
	UOverlay* StateOverlay = WidgetTree->ConstructWidget<UOverlay>();
	Layer(StateOverlay, MakeChrome(ESurface::Plaque));
	DetailState = MakeText(TEXT("HybridTreeDetailState"), FText::GetEmpty(), 17, Muted);
	UOverlaySlot* StateSlot = Layer(StateOverlay, DetailState);
	StateSlot->SetPadding(FMargin(2, 4));
	Left->AddChildToVerticalBox(StateOverlay);
	CommonDetailIcon = CreateWidget<URogue10mHybridSkillTreeEntryWidget>(GetOwningPlayer(),
		URogue10mHybridSkillTreeEntryWidget::StaticClass(), TEXT("HybridTreeCommonDetailIcon"));
	UVerticalBoxSlot* CommonDetailSlot = Left->AddChildToVerticalBox(CommonDetailIcon);
	CommonDetailSlot->SetHorizontalAlignment(HAlign_Center);
	CommonDetailSlot->SetPadding(FMargin(0, 16, 0, 14));
	CommonDetailIcon->SetVisibility(ESlateVisibility::Collapsed);
	Left->AddChildToVerticalBox(MakeChrome(ESurface::Rule))->SetPadding(FMargin(0, 2, 0, 16));
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
	Footer->AddChildToVerticalBox(CommonViewToggle)->SetPadding(FMargin(1, 7, 1, 1));
	UOverlay* GraphPanel = WidgetTree->ConstructWidget<UOverlay>(); TreeBody->AddChildToHorizontalBox(GraphPanel);
	Layer(GraphPanel, MakeChrome(ESurface::Graph));
	GraphViewportSize = WidgetTree->ConstructWidget<USizeBox>();
	GraphPanelSlot = Layer(GraphPanel, GraphViewportSize);
	Graph = CreateWidget<URogue10mHybridSkillTreeGraphWidget>(GetOwningPlayer()); GraphViewportSize->AddChild(Graph);
	LegendText = MakeText(TEXT("HybridTreeLegend"), FText::GetEmpty(), 14, Muted);
	LegendText->SetWrapTextAt(1420); Rows->AddChildToVerticalBox(LegendText)->SetPadding(FMargin(0, 8, 0, 0));
	ApplyCommonChrome(false);
}

void URogue10mHybridSkillTreeWidget::ApplyCommonChrome(bool bCommonWeb)
{
	using namespace Rogue10mHybridTreeUI;
	using namespace Rogue10mCommonSkillChrome;
	if (bCommonChromeEnabled.IsSet() && bCommonChromeEnabled.GetValue() == bCommonWeb) return;
	bCommonChromeEnabled = bCommonWeb;
	const auto OrnamentVisibility = bCommonWeb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	for (UWidget* Ornament : CommonChromeWidgets)
	{
		Ornament->SetVisibility(OrnamentVisibility);
	}
	WindowSize->SetWidthOverride(bCommonWeb ? 1724.0f : 1500.0f);
	WindowSize->SetHeightOverride(bCommonWeb ? 995.0f : 940.0f);
	WindowRim->SetBrushColor(bCommonWeb ? FLinearColor::Transparent : Gold * 0.72f);
	WindowInterior->SetBrushColor(bCommonWeb ? FLinearColor::Transparent : Iron);
	LeftColumnSize->SetWidthOverride(bCommonWeb ? 285.0f : 372.0f);
	CastChecked<UHorizontalBoxSlot>(LeftColumnSize->Slot)->SetPadding(FMargin(0, 0, bCommonWeb ? 7.0f : 20.0f, 0));
	DetailPanelSlot->SetPadding(bCommonWeb ? FMargin(16, 18, 16, 13) : FMargin(0));
	GraphPanelSlot->SetPadding(bCommonWeb ? FMargin(6, 5) : FMargin(0));
	GraphViewportSize->SetWidthOverride(bCommonWeb ? CommonGraphSize.X : GraphSize.X);
	GraphViewportSize->SetHeightOverride(bCommonWeb ? CommonGraphSize.Y : GraphSize.Y);

	const auto LegacyVisibility = bCommonWeb ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	GrowthText->SetVisibility(LegacyVisibility);
	ContextText->SetVisibility(LegacyVisibility);
	DetailHeading->SetVisibility(LegacyVisibility);
	CommonGrowthText->SetVisibility(OrnamentVisibility);
	const auto ApplyText = [bCommonWeb](UTextBlock& Text, int32 CommonSize, int32 LegacySize,
		FLinearColor CommonColor, FLinearColor LegacyColor, bool bHeading, bool bCenter)
	{
		if (bCommonWeb) StyleText(Text, CommonSize, bHeading, CommonColor);
		else
		{
			Text.SetFont(UTextBlock::StaticClass()->GetDefaultObject<UTextBlock>()->GetFont());
			SetTextStyle(&Text, LegacySize, LegacyColor);
			Text.SetShadowColorAndOpacity(FLinearColor::Transparent);
			Text.SetShadowOffset(FVector2D::ZeroVector);
		}
		Text.SetJustification(bCenter && bCommonWeb ? ETextJustify::Center : ETextJustify::Left);
	};
	UTextBlock* Title = CastChecked<UTextBlock>(WidgetTree->FindWidget(TEXT("HybridTreeTitle")));
	ApplyText(*Title, 30, 28, BurnishedGold, Gold, true, false);
	StyleText(*CommonGrowthText, 17, true, BurnishedGold);
	ApplyText(*DetailName, 31, 23, CopperPaper, Pale, true, true);
	ApplyText(*DetailState, 18, 17, CopperPaper, Muted, true, true);
	ApplyText(*DetailDescription, 19, 17, CopperPaper, Pale, false, true);
	ApplyText(*DetailRequirements, 14, 16, SecondaryText, Muted, false, false);
	ApplyText(*DetailCost, 15, 16, BurnishedGold, Gold, false, false);
	ApplyText(*LegendText, 13, 14, SecondaryText, Muted, false, false);
	LegendText->SetJustification(bCommonWeb ? ETextJustify::Center : ETextJustify::Left);
	for (UTextBlock* Text : {DetailName.Get(), DetailDescription.Get(), DetailRequirements.Get(), DetailCost.Get(), MessageText.Get()})
	{
		Text->SetWrapTextAt(bCommonWeb ? 240.0f : 340.0f);
	}
	CastChecked<UVerticalBoxSlot>(DetailName->Slot)->SetPadding(bCommonWeb ? FMargin(2, 34, 2, 12) : FMargin(2, 4));
	CastChecked<UVerticalBoxSlot>(DetailState->GetParent()->Slot)->SetHorizontalAlignment(bCommonWeb ? HAlign_Center : HAlign_Fill);
	CastChecked<UOverlaySlot>(DetailState->Slot)->SetPadding(bCommonWeb ? FMargin(25, 8) : FMargin(2, 4));
	CastChecked<UVerticalBoxSlot>(DetailDescription->Slot)->SetPadding(bCommonWeb ? FMargin(2, 4, 2, 16) : FMargin(2, 4));
	for (ERogue10mSkillTreeTab Tab : DefaultTabOrder)
	{
		UTextBlock* Caption = CastChecked<UTextBlock>(GetTabButton(Tab)->GetContent());
		ApplyText(*Caption, 23, 17, CopperPaper, Pale, true, true);
		Caption->SetJustification(ETextJustify::Center);
	}
	URogue10mHybridSkillTreeButton* CloseButton = CastChecked<URogue10mHybridSkillTreeButton>(WidgetTree->FindWidget(TEXT("HybridTreeClose")));
	CastChecked<UHorizontalBoxSlot>(CloseButton->Slot)->SetPadding(bCommonWeb ? FMargin(0, 0, 108.0f, 0) : FMargin(0));
	for (URogue10mHybridSkillTreeButton* Button : {CommonViewToggle.Get(), PointsButton.Get(), CloseButton})
	{
		UTextBlock* Caption = CastChecked<UTextBlock>(Button->GetContent());
		ApplyText(*Caption, 16, 17, CopperPaper, Pale, true, true);
		Caption->SetJustification(ETextJustify::Center);
		FButtonStyle Style = ButtonStyle(Gold);
		if (bCommonWeb)
		{
			const bool bPrimary = Button == PointsButton.Get();
			const FLinearColor Fill = bPrimary ? FLinearColor(0.091f, 0.037f, 0.005f) : FLinearColor(0.010f, 0.008f, 0.006f);
			const FLinearColor Rim = bPrimary ? BurnishedGold : FLinearColor(0.32f, 0.17f, 0.05f);
			Style.Normal = FSlateRoundedBoxBrush(Fill, 3.0f, Rim, 1.0f);
			Style.Hovered = FSlateRoundedBoxBrush(Fill + FLinearColor(0.08f, 0.038f, 0.008f, 0.0f), 3.0f, BurnishedGold, 1.5f);
			Style.Pressed = FSlateRoundedBoxBrush(Fill + FLinearColor(0.04f, 0.017f, 0.003f, 0.0f), 3.0f, Rim, 1.5f);
		}
		Button->SetStyle(Style);
	}
	CastChecked<UScrollBox>(WidgetTree->FindWidget(TEXT("HybridTreeDetailsScroll")))->SetScrollBarVisibility(
		bCommonWeb ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void URogue10mHybridSkillTreeWidget::BindContext()
{
	ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	URogue10mSkillTreeProgressionComponent* Component = State ? State->FindComponentByClass<URogue10mSkillTreeProgressionComponent>() : nullptr;
	if (Progression.Get() == Component && BoundState.Get() == State && Component
		&& BoundCombat.Get() == GetCombat() && BoundBindings.Get() == GetBindings()
		&& BoundTown.Get() == (State ? State->GetTownProgressionComponent() : nullptr)) return;
	if (BoundState.Get() != State) bSelectEquippedTabOnRefresh = true;
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
	CancelOwnedSkillDrag();
	CancelKeySelection();
	if (Progression.IsValid()) Progression->OnChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundState.IsValid()) BoundState->OnGrowthChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundTown.IsValid()) BoundTown->OnTownProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundCombat.IsValid()) BoundCombat->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	if (BoundBindings.IsValid()) BoundBindings->OnSkillBindingsChanged.RemoveDynamic(this, &ThisClass::HandleBindingsChanged);
	Progression.Reset(); BoundState.Reset(); BoundTown.Reset(); BoundCombat.Reset(); BoundBindings.Reset();
}

URogue10mHybridSkillTreeButton* URogue10mHybridSkillTreeWidget::GetTabButton(ERogue10mSkillTreeTab Tab) const
{
	switch (Tab)
	{
	case ERogue10mSkillTreeTab::Common: return CommonTab;
	case ERogue10mSkillTreeTab::Knuckle: return FighterTab;
	case ERogue10mSkillTreeTab::Sword: return GreatSwordTab;
	case ERogue10mSkillTreeTab::Magic: return MagicTab;
	default: return nullptr;
	}
}

TArray<ERogue10mSkillTreeTab> URogue10mHybridSkillTreeWidget::GetTabOrder() const
{
	TArray<ERogue10mSkillTreeTab> Result;
	if (TabBar)
	{
		Result.SetNum(TabBar->GetChildrenCount());
		for (ERogue10mSkillTreeTab Tab : Rogue10mHybridTreeUI::DefaultTabOrder)
		{
			const int32 Index = TabBar->GetChildIndex(GetTabButton(Tab));
			if (Result.IsValidIndex(Index)) Result[Index] = Tab;
		}
	}
	return Result;
}

ERogue10mSkillTreeTrack URogue10mHybridSkillTreeWidget::GetDisplayedTrack() const
{
	switch (DisplayedTab)
	{
	case ERogue10mSkillTreeTab::Knuckle: return ERogue10mSkillTreeTrack::Brawler;
	case ERogue10mSkillTreeTab::Sword: return ERogue10mSkillTreeTrack::GreatSword;
	default: return ERogue10mSkillTreeTrack::None;
	}
}

bool URogue10mHybridSkillTreeWidget::IsDisplayingActiveTrack() const
{
	const auto Track = GetDisplayedTrack();
	return Track != ERogue10mSkillTreeTrack::None && Progression.IsValid() && Progression->GetActiveTrack() == Track;
}

TArray<FRogue10mSkillTreeNodeDefinition> URogue10mHybridSkillTreeWidget::GetDisplayNodes() const
{
	if (!Progression.IsValid() || DisplayedTab == ERogue10mSkillTreeTab::Magic) return {};
	const auto Track = GetDisplayedTrack();
	const bool bIdentityView = bShowingCommonIdentity;
	return Progression->GetCatalog()->Nodes.FilterByPredicate([Track, bIdentityView](const FRogue10mSkillTreeNodeDefinition& Node)
	{
		if (Node.Track != Track || !Node.bShowInTree) return false;
		if (Track != ERogue10mSkillTreeTrack::None) return true;
		return bIdentityView ? Node.Identity != ERogue10mSkillTreeIdentity::None : Node.CommonSkill != ERogue10mCommonSkill::None;
	});
}

void URogue10mHybridSkillTreeWidget::SetDisplayedTab(ERogue10mSkillTreeTab Tab, bool bShowCommonIdentity)
{
	bShowCommonIdentity = Tab == ERogue10mSkillTreeTab::Common && bShowCommonIdentity;
	if (DisplayedTab == Tab && bShowingCommonIdentity == bShowCommonIdentity) return;
	CancelOwnedSkillDrag();
	CancelKeySelection();
	DisplayedTab = Tab;
	bShowingCommonIdentity = bShowCommonIdentity;
	SelectedNodeId = NAME_None;
	PendingChoiceId = NAME_None;
	SelectedBasicSkill.Reset();
	bRefundConfirmation = false;
	ShowMessage(FText::GetEmpty());
}

void URogue10mHybridSkillTreeWidget::RefreshEquipmentTab()
{
	using namespace Rogue10mHybridTreeUI;
	const auto Weapon = BoundState.IsValid() ? BoundState->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	const auto Tab = TabForWeapon(Weapon);
	const bool bEquipmentChanged = !EquippedWeapon.IsSet() || EquippedWeapon.GetValue() != Weapon;
	if (!EquippedWeapon.IsSet() || TabForWeapon(EquippedWeapon.GetValue()) != Tab)
	{
		TabBar->ClearChildren();
		const auto AddTab = [this](ERogue10mSkillTreeTab Item)
		{
			UHorizontalBoxSlot* Slot = TabBar->AddChildToHorizontalBox(GetTabButton(Item));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 0, 4, 0));
		};
		AddTab(Tab);
		for (ERogue10mSkillTreeTab Item : DefaultTabOrder)
		{
			if (Item != Tab) AddTab(Item);
		}
	}
	EquippedWeapon = Weapon;
	if (bEquipmentChanged || bSelectEquippedTabOnRefresh) SetDisplayedTab(Tab);
	bSelectEquippedTabOnRefresh = false;
}

void URogue10mHybridSkillTreeWidget::RefreshFromData()
{
	using namespace Rogue10mHybridTreeUI;
	BuildTree(); BindContext();
	if (!Graph) return;
	RefreshEquipmentTab();
	const bool bCommon = DisplayedTab == ERogue10mSkillTreeTab::Common;
	const bool bCommonWeb = bCommon && !bShowingCommonIdentity;
	ApplyCommonChrome(bCommonWeb);
	if (UTextBlock* Title = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("HybridTreeTitle"))))
	{
		Title->SetText(DisplayedTab == ERogue10mSkillTreeTab::Sword
			? NSLOCTEXT("Rogue10mTree", "SwordTreeTitle", "ROGUE10M  ·  대검 스킬트리")
			: bCommonWeb ? NSLOCTEXT("Rogue10mTree", "CommonTreeTitle", "ROGUE10M  ·  공통 스킬트리")
			: bShowingCommonIdentity ? NSLOCTEXT("Rogue10mTree", "IdentityTreeTitle", "ROGUE10M  ·  아이덴티티")
			: NSLOCTEXT("Rogue10mTree", "SkillTreeTitle", "ROGUE10M  ·  스킬트리"));
	}
	for (ERogue10mSkillTreeTab Tab : DefaultTabOrder)
	{
		const bool bSelected = DisplayedTab == Tab;
		FButtonStyle Style = ButtonStyle(Gold, bSelected);
		if (bCommonWeb)
		{
			const FLinearColor Rim = bSelected ? FLinearColor(1.0f, 0.68f, 0.24f) : FLinearColor(0.42f, 0.26f, 0.12f);
			const FLinearColor Fill = bSelected ? FLinearColor(0.21f, 0.081f, 0.010f) : FLinearColor(0.012f, 0.011f, 0.010f);
			Style.Normal = FSlateRoundedBoxBrush(Fill, 4.0f, Rim, bSelected ? 2.6f : 1.0f);
			Style.Hovered = FSlateRoundedBoxBrush(Fill + FLinearColor(0.056f, 0.026f, 0.005f, 0.0f), 4.0f, Rim, 2.1f);
			Style.Pressed = FSlateRoundedBoxBrush(Fill * 0.82f, 4.0f, Rim, 1.8f);
			Style.NormalPadding = Style.PressedPadding = FMargin(0.0f, 6.0f);
		}
		else if (bSelected && Tab != ERogue10mSkillTreeTab::Magic)
		{
			Style.Normal = FSlateRoundedBoxBrush(FLinearColor(0.13f, 0.061f, 0.018f), 5.0f, KnuckleLight, 2.0f);
			Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.17f, 0.082f, 0.023f), 5.0f, KnuckleLight, 2.5f);
		}
		GetTabButton(Tab)->SetStyle(Style);
	}
	const bool bMagic = DisplayedTab == ERogue10mSkillTreeTab::Magic;
	TreeBody->SetVisibility(bMagic ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	LegendText->SetVisibility(bMagic || bCommonWeb ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MagicPlaceholder->SetVisibility(bMagic ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CommonViewToggle->SetVisibility(bCommon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CastChecked<UTextBlock>(CommonViewToggle->GetContent())->SetText(bCommonWeb
		? NSLOCTEXT("Rogue10mTree", "OpenCommonIdentity", "아이덴티티")
		: NSLOCTEXT("Rogue10mTree", "OpenCommonWeb", "공통 능력"));
	IdentityText->SetVisibility(!bCommonWeb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bCommonWeb || !Progression.IsValid()) CommonDetailIcon->SetVisibility(ESlateVisibility::Collapsed);
	const auto Track = GetDisplayedTrack();
	if (bMagic || !Progression.IsValid())
	{
		Graph->SetData(this, Progression.Get(), DisplayedTab, Track, NAME_None);
		CommonGrowthText->SetText(FText::GetEmpty());
		if (!bMagic) ContextText->SetText(FText::FromString(TEXT("성장 정보를 준비 중입니다. 창을 다시 열어 주세요.")));
		return;
	}
	const auto Nodes = GetDisplayNodes();
	const auto ActiveTrack = Progression->GetActiveTrack();
	// The loadout can still open the legacy combo bundle directly without adding it to the visual tree.
	const bool bShowingLegacyCombo = DisplayedTab == ERogue10mSkillTreeTab::Knuckle && SelectedNodeId == TEXT("F.Basic")
		&& SelectedBasicSkill.IsValid();
	if (!bShowingLegacyCombo && !Nodes.ContainsByPredicate([this](const auto& Node) { return Node.NodeId == SelectedNodeId; }))
	{
		const auto* PreferredNode = bShowingCommonIdentity ? Nodes.FindByPredicate([ActiveTrack](const auto& Node)
		{
			return URogue10mSkillTreeDataAsset::IsNodeInTrack(Node, ActiveTrack);
		}) : bCommonWeb ? Nodes.FindByPredicate([](const auto& Node)
		{
			return Node.CommonSkill == ERogue10mCommonSkill::Foundation;
		}) : nullptr;
		SelectedNodeId = PreferredNode ? PreferredNode->NodeId : (Nodes.IsEmpty() ? NAME_None : Nodes[0].NodeId);
		PendingChoiceId = NAME_None;
	}
	GrowthText->SetText(FText::FromString(FString::Printf(TEXT("LV%d · 공용 성장 %dP"), BoundState.IsValid() ? BoundState->GetPlayerLevel() : 1, Progression->GetAvailableGrowthPoints())));
	if (bCommonWeb)
	{
		CommonGrowthText->SetText(FText::Format(NSLOCTEXT("Rogue10mTree", "CommonGrowthBalance", "공용 성장 {0}P"),
			FText::AsNumber(Progression->GetAvailableGrowthPoints())));
	}
	if (bCommon)
	{
		ContextText->SetText(bCommonWeb
			? NSLOCTEXT("Rogue10mTree", "CommonWebContext", "습득한 능력은 모든 무기에서 적용됩니다.")
			: NSLOCTEXT("Rogue10mTree", "CommonTreeContext", "장착 무기에 따라 전수할 수 있는 아이덴티티가 달라집니다."));
	}
	else
	{
		const FText Label = TabLabel(DisplayedTab);
		const FText RequiredWeapon = DisplayedTab == ERogue10mSkillTreeTab::Sword ? NSLOCTEXT("Rogue10mTree", "GreatSwordRequired", "대검") : Label;
		ContextText->SetText(IsDisplayingActiveTrack()
			? FText::Format(NSLOCTEXT("Rogue10mTree", "CurrentWeaponTab", "{0} · 현재 무기"), Label)
			: FText::Format(NSLOCTEXT("Rogue10mTree", "PreviewWeaponTab", "{0} · 미리보기\n{1} 장착 후 전수·활성·키 지정"), Label, RequiredWeapon));
	}
	ContextText->SetAutoWrapText(true);
	MasterySection->SetVisibility(bCommon ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (!bCommon)
	{
		const int32 CurrentXP = Progression->GetMasteryExperience(Track);
		const int32 NeededXP = Progression->GetMasteryExperienceToNextLevel(Track);
		const FString MasteryLabel = FString::Printf(TEXT("%s 숙련 M%d"), *TabLabel(DisplayedTab).ToString(), Progression->GetMasteryLevel(Track));
		const float TrainingProgress = NeededXP > 0 ? FMath::Clamp(static_cast<float>(CurrentXP) / NeededXP, 0.0f, 1.0f) : 1.0f;
		MasteryText->SetText(FText::FromString(NeededXP > 0 ? FString::Printf(TEXT("%s    %.1f / 100"), *MasteryLabel, TrainingProgress * 100.0f) : MasteryLabel + TEXT(" · 완성")));
		MasteryBar->SetPercent(TrainingProgress);
		const FText StatSummary = RewardText(Progression->GetTrackMasteryStatModifiers(Track));
		MasteryStatsText->SetText(StatSummary);
		MasteryStatsText->SetVisibility(StatSummary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (UWidget* Heading = WidgetTree->FindWidget(TEXT("HybridTreeMasteryStatsHeading"))) Heading->SetVisibility(MasteryStatsText->GetVisibility());
	}
	const auto ContextTrack = bCommon ? ActiveTrack : Track;
	auto Identity = Progression->GetActiveIdentity();
	if (!Progression->GetCatalog()->IsIdentityAllowed(ContextTrack, Identity)) Identity = ERogue10mSkillTreeIdentity::None;
	IdentityText->SetText(FText::Format(FText::FromString(TEXT("활성 아이덴티티 · {0}")), IdentityName(Identity)));
	IdentityText->SetColorAndOpacity(FSlateColor(IdentityColor(Identity)));
	LegendText->SetText(FText::FromString(bCommonWeb
		? TEXT("금색 선 · 습득 경로   밝은 테두리 · 조건 충족   어두운 테두리 · 잠김")
		: bCommon
		? TEXT("내공 · 녹색   투지 · 붉은색   마나 · 푸른색")
		: DisplayedTab == ERogue10mSkillTreeTab::Knuckle
		? TEXT("실선 · 선행 수련   밝은 그림 · 전수 또는 조건 충족   어두운 그림 · 잠김")
		: TEXT("황동 · 전수   은빛 · 전수 가능   회색 · 잠김   실선 · 필수 선행   점선 · 대체 경로")));
	Graph->SetData(this, Progression.Get(), DisplayedTab, ContextTrack, SelectedNodeId);
	RefreshDetails();
}
void URogue10mHybridSkillTreeWidget::RefreshDetails()
{
	using namespace Rogue10mHybridTreeUI;
	const FRogue10mSkillTreeNodeDefinition* Node = Progression.IsValid() ? Progression->FindNode(SelectedNodeId) : nullptr;
	if (!Node) return;
	const auto ActiveTrack = Progression->GetActiveTrack();
	const auto ContextTrack = DisplayedTab == ERogue10mSkillTreeTab::Common ? ActiveTrack : GetDisplayedTrack();
	const bool bLearned = Progression->IsNodeUnlocked(Node->NodeId);
	const auto State = DisplayState(*Progression.Get(), *Node, ContextTrack);
	const bool bCommonNode = Node->CommonSkill != ERogue10mCommonSkill::None;
	if (bCommonNode)
	{
		CommonDetailIcon->Configure(nullptr, *Node, State, false, false, 224.0f);
		CommonDetailIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	const URogue10mAttackSkillData* BasicSkill = Node->NodeType == ERogue10mHybridSkillTreeNodeType::Basic ? SelectedBasicSkill.Get() : nullptr;
	DetailName->SetText(BasicSkill ? BasicSkill->SkillName : Node->DisplayName);
	FText Description = BasicSkill ? BasicSkill->SkillDescription : Node->Description;
	if (Node->Track == ERogue10mSkillTreeTrack::GreatSword && Node->NodeType == ERogue10mHybridSkillTreeNodeType::Technique
		&& Node->PointCost > 0)
	{
		if (const URogue10mAttackSkillData* Technique = GetSelectedSkill())
		{
			Description = FText::Format(NSLOCTEXT("Rogue10mTree", "SwordTechniqueDetails",
				"선행 경로를 충족하면 공용 성장 {0}P로 전수합니다.\n전수 후 장착 슬롯 · {1}"),
				FText::AsNumber(Node->PointCost), Technique->GetAllowedSlotText());
		}
	}
	DetailDescription->SetText(Description);
	DetailState->SetText(StateName(State)); DetailState->SetColorAndOpacity(FSlateColor(bLearned ? Green : Gold));
	if (bCommonNode)
	{
		DetailState->SetText(Node->CommonSkill == ERogue10mCommonSkill::Foundation
			? NSLOCTEXT("Rogue10mTree", "CommonStartingNode", "시작점")
			: bLearned ? NSLOCTEXT("Rogue10mTree", "CommonNodeLearned", "습득 완료") : StateName(State));
		DetailState->SetColorAndOpacity(FSlateColor(Rogue10mCommonSkillChrome::CopperPaper));
	}
	if (Node->bAutomaticUnlock && Node->NodeType != ERogue10mHybridSkillTreeNodeType::Root)
	{
		DetailState->SetText(FText::Format(FText::FromString(TEXT("{0} · {1}")), StateName(State),
			FText::FromString(Node->NodeType == ERogue10mHybridSkillTreeNodeType::Stat ? TEXT("숙련 스탯 성장") : TEXT("자동 전수"))));
	}
	DetailRequirements->SetText(Progression->GetNodeRequirementsText(Node->NodeId));
	DetailCost->SetVisibility(bCommonNode && bLearned ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
	const bool bCanInvest = URogue10mSkillTreeDataAsset::IsNodeInTrack(*Node, ActiveTrack);
	FText PointReason, MasteryReason;
	const bool bCanPoints = Progression->CanUnlockNode(Node->NodeId, ERogue10mSkillTreeUnlockRoute::Points, PendingChoiceId, PointReason);
	const bool bCanMastery = Progression->CanUnlockNode(Node->NodeId, ERogue10mSkillTreeUnlockRoute::Mastery, PendingChoiceId, MasteryReason);
	PointsButton->SetVisibility(Node->PointCost > 0 && !bLearned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CastChecked<UTextBlock>(PointsButton->GetContent())->SetText(FText::Format(bCommonNode
		? NSLOCTEXT("Rogue10mTree", "CommonLearnPoints", "습득 · {0}P")
		: NSLOCTEXT("Rogue10mTree", "LearnPoints", "전수 · {0}P"), FText::AsNumber(Node->PointCost)));
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
		for (const auto& GrantedSkill : Node->GrantedSkills)
		{
			if (auto* Loaded = GrantedSkill.LoadSynchronous()) BasicSkillPicker->AddOption(Loaded->SkillName.ToString());
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
	switch (Action)
	{
	case Common: BrowseTab(ERogue10mSkillTreeTab::Common); return;
	case Fighter: BrowseTab(ERogue10mSkillTreeTab::Knuckle); return;
	case GreatSword: BrowseTab(ERogue10mSkillTreeTab::Sword); return;
	case Magic: BrowseTab(ERogue10mSkillTreeTab::Magic); return;
	case ToggleCommonView:
		BrowseTabView(ERogue10mSkillTreeTab::Common, !bShowingCommonIdentity);
		return;
	default: break;
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
		BrowseTab(Rogue10mHybridTreeUI::TabForTrack(Track));
}
void URogue10mHybridSkillTreeWidget::BrowseTab(ERogue10mSkillTreeTab Tab)
{
	BrowseTabView(Tab, false);
}
void URogue10mHybridSkillTreeWidget::BrowseCommonIdentity()
{
	BrowseTabView(ERogue10mSkillTreeTab::Common, true);
}
void URogue10mHybridSkillTreeWidget::BrowseTabView(ERogue10mSkillTreeTab Tab, bool bShowCommonIdentity)
{
	BuildTree(); BindContext();
	if (!TabBar || !GetTabButton(Tab)) return;
	RefreshEquipmentTab();
	SetDisplayedTab(Tab, bShowCommonIdentity);
	RefreshFromData();
}
void URogue10mHybridSkillTreeWidget::SelectNode(FName NodeId)
{
	if (const auto* Node = Progression.IsValid() ? Progression->FindNode(NodeId) : nullptr)
	{
		SetDisplayedTab(Rogue10mHybridTreeUI::TabForTrack(Node->Track),
			Node->Track == ERogue10mSkillTreeTrack::None && Node->Identity != ERogue10mSkillTreeIdentity::None);
		HandleAction(Rogue10mHybridTreeUI::SelectNode, NodeId);
	}
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
		&& IsDisplayingActiveTrack()
		&& Combat->CanEquipSkillInSlot(Skill, ERogue10mAttackInputSlot::QSkill);
	const FKey Key = Skill && Bindings ? Bindings->GetSkillKey(Skill) : EKeys::Invalid;
	KeySelector->SetIsEnabled(bCanBind); KeySelector->SetSelectedKey(FInputChord(Key));
	ClearKeyButton->SetIsEnabled(bCanBind && Key.IsValid());
	const ESlateVisibility BindingVisibility = Skill ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	KeySelector->SetVisibility(BindingVisibility); ClearKeyButton->SetVisibility(BindingVisibility);
	BindingText->SetVisibility(Skill ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BindingText->SetText(FText::FromString(Skill
		? FString::Printf(TEXT("%s · 키 %s"), *Skill->SkillName.ToString(), Key.IsValid() ? *Key.GetDisplayName().ToString() : TEXT("미지정")) : TEXT("")));
	FString Hint = FText::Format(NSLOCTEXT("Rogue10mSkillTree", "IdentitySlotHint",
		"공용 기술은 Q/E/R 지정 또는 HUD 드래그. 전용 기술은 해당 전용 슬롯에만 장착. 기본공격·아이덴티티({0}) 별도, 최대 6개."),
		Rogue10mInput::IdentityKeyLabel()).ToString();
	if (Skill) { Hint += TEXT(" ") + Skill->GetAllowedSlotText().ToString(); }
	if (Skill && Skill->RequiresLoadoutCharge())
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
	const auto MatchesSkill = [Skill](const FRogue10mSkillTreeNodeDefinition& Node)
	{
		return Node.Skill.LoadSynchronous() == Skill || Node.GrantedSkills.ContainsByPredicate([Skill](const auto& Item)
		{
			return Item.LoadSynchronous() == Skill;
		});
	};
	const auto DisplayNodes = GetDisplayNodes();
	const auto ActiveNodes = Progression->GetNodes(Progression->GetActiveTrack());
	const auto* Node = DisplayNodes.FindByPredicate(MatchesSkill);
	if (!Node) Node = ActiveNodes.FindByPredicate([&MatchesSkill](const auto& Item) { return Item.bShowInTree && MatchesSkill(Item); });
	if (!Node) Node = ActiveNodes.FindByPredicate([&MatchesSkill](const auto& Item) { return Item.NodeId == TEXT("F.Basic") && MatchesSkill(Item); });
	if (Node)
	{
		CancelKeySelection();
		SetDisplayedTab(Rogue10mHybridTreeUI::TabForTrack(Node->Track));
		SelectedNodeId = Node->NodeId;
		SelectedBasicSkill = Skill;
		PendingChoiceId = NAME_None;
		bRefundConfirmation = false;
		RefreshFromData();
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
			CancelKeySelection();
			SelectedBasicSkill = Loaded;
			RefreshFromData();
			return;
		}
	}
}
int32 URogue10mHybridSkillTreeWidget::GetVisibleNodeCount() const
{
	return GetDisplayNodes().Num();
}
URogue10mHybridSkillTreeEntryWidget* URogue10mHybridSkillTreeWidget::FindNodeEntry(FName NodeId) const
{
	return Graph ? Graph->FindNodeEntry(NodeId) : nullptr;
}
FName URogue10mHybridSkillTreeWidget::GetNodeFrameShape(FName NodeId) const
{
	return Graph ? Graph->GetNodeFrameShape(NodeId) : NAME_None;
}
int32 URogue10mHybridSkillTreeWidget::GetNodeFrameOutlineCount(FName NodeId) const
{
	return Graph ? Graph->GetNodeFrameOutlineCount(NodeId) : 0;
}
TArray<FVector2f> URogue10mHybridSkillTreeWidget::GetNodeOuterFramePoints(FName NodeId) const
{
	return Graph ? Graph->GetNodeOuterFramePoints(NodeId) : TArray<FVector2f>();
}
