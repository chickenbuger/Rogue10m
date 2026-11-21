// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mCombatHudAWidget.h"

#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInputKeys.h"
#include "Slate/UMGDragDropOp.h"
#include "Styling/CoreStyle.h"
#include "Styling/WidgetStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Rogue10mSkillDragDropOperation.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SPanel.h"
#include "Widgets/SToolTip.h"

namespace
{
	const FVector2f DesignSize(1672, 941);
	const FVector2f AtlasSize(1024, 512);

	enum class ESkin : uint8
	{
		PlateHealth,
		PlateStamina,
		PlateLevel,
		PlateExp,
		FillHealth,
		FillStamina,
		FillMana,
		FillExp,
		SlotQ,
		SlotE,
		SlotR,
		SlotRMB,
		SlotChord,
		SlotCharge,
		SlotAlt,
		SlotLock1,
		SlotLock2,
		SlotLock3,
		SlotLock4,
		PlateCtrl,
		SlotGeneric,
		GlyphLock,
		PlateIdentity,
		RingIdentity,
		IdentitySword,
		Count
	};

	struct FRegion
	{
		float X, Y, Width, Height;
		FVector2f Min() const { return FVector2f(X, Y); }
		FVector2f Size() const { return FVector2f(Width, Height); }
		FVector2f Center() const { return Min() + 0.5f * Size(); }
		FRegion Inset(float Horizontal, float Vertical) const
		{
			return {X + Horizontal, Y + Vertical, Width - 2 * Horizontal, Height - 2 * Vertical};
		}
	};

	struct FSprite
	{
		FRegion Design;
		FRegion Atlas;
	};

	// Pixel geometry from SourceArt/UI/CombatHudA/layout.json.
	constexpr FSprite Sprites[] = {
		{ {26, 22, 291, 49}, {4, 4, 291, 49} }, // PlateHealth
		{ {26, 68, 291, 51}, {299, 4, 291, 51} }, // PlateStamina
		{ {26, 115, 77, 39}, {594, 4, 77, 39} }, // PlateLevel
		{ {96, 118, 221, 31}, {675, 4, 221, 31} }, // PlateExp
		{ {37, 34, 268, 27}, {4, 59, 268, 27} }, // FillHealth
		{ {37, 80, 268, 27}, {276, 59, 268, 27} }, // FillStamina
		{ {37, 80, 268, 27}, {548, 59, 268, 27} }, // FillMana
		{ {144, 129, 160, 10}, {820, 59, 160, 10} }, // FillExp
		{ {532, 758, 95, 104}, {4, 90, 95, 104} }, // SlotQ
		{ {622, 757, 96, 105}, {103, 90, 96, 105} }, // SlotE
		{ {711, 757, 97, 105}, {203, 90, 97, 105} }, // SlotR
		{ {803, 757, 96, 105}, {304, 90, 96, 105} }, // SlotRMB
		{ {894, 757, 96, 105}, {404, 90, 96, 105} }, // SlotChord
		{ {984, 757, 100, 105}, {504, 90, 100, 105} }, // SlotCharge
		{ {413, 769, 77, 94}, {608, 90, 77, 94} }, // SlotAlt
		{ {673, 866, 69, 46}, {689, 90, 69, 46} }, // SlotLock1
		{ {737, 866, 69, 46}, {762, 90, 69, 46} }, // SlotLock2
		{ {801, 866, 69, 46}, {835, 90, 69, 46} }, // SlotLock3
		{ {865, 866, 69, 46}, {908, 90, 69, 46} }, // SlotLock4
		{ {1260, 845, 69, 45}, {4, 199, 69, 45} }, // PlateCtrl
		{ {532, 758, 95, 104}, {77, 199, 95, 104} }, // SlotGeneric
		{ {699, 876, 20, 25}, {176, 199, 20, 25} }, // GlyphLock
		{ {1113, 732, 170, 170}, {200, 199, 170, 170} }, // PlateIdentity
		{ {1113, 732, 170, 170}, {374, 199, 170, 170} }, // RingIdentity
		{ {1165, 746, 65, 86}, {548, 199, 65, 86} }, // IdentitySword
	};
	static_assert(UE_ARRAY_COUNT(Sprites) == static_cast<int32>(ESkin::Count));

	constexpr ESkin SkillPlates[] = { ESkin::SlotQ, ESkin::SlotE, ESkin::SlotR,
		ESkin::SlotRMB, ESkin::SlotChord, ESkin::SlotCharge };
	constexpr ESkin ItemPlates[] = { ESkin::SlotLock1, ESkin::SlotLock2, ESkin::SlotLock3, ESkin::SlotLock4 };

	const FSprite& Sprite(ESkin Skin)
	{
		return Sprites[static_cast<int32>(Skin)];
	}

	struct FHudPlacement
	{
		explicit FHudPlacement(const FVector2D& Size)
		{
			Scale = FMath::Max(0.0f, FMath::Min(static_cast<float>(Size.X) / DesignSize.X,
				static_cast<float>(Size.Y) / DesignSize.Y));
			// Layout A is placed relative to the original canvas center and bottom,
			// retaining its slight right bias from the identity medallion.
			CombatOffset = FVector2f((Size.X - DesignSize.X * Scale) * 0.5, Size.Y - DesignSize.Y * Scale);
		}
		FVector2f Offset(bool bCombat) const { return bCombat ? CombatOffset : FVector2f::ZeroVector; }
		FSlateLayoutTransform Transform(const FVector2f& DesignPoint, bool bCombat) const
		{
			return FSlateLayoutTransform(Scale, Offset(bCombat) + Scale * DesignPoint);
		}
		FBox2D Bounds(const FVector2f& Min, const FVector2f& Max, bool bCombat) const
		{
			const FVector2f Anchor = Offset(bCombat);
			return FBox2D(FVector2D(Anchor + Min * Scale), FVector2D(Anchor + Max * Scale));
		}
		float Scale = 0;
		FVector2f CombatOffset = FVector2f::ZeroVector;
	};

	FText CompactKey(const FRogue10mHudQuickSlotView* View, int32 Index)
	{
		static const TCHAR* Labels[] = {TEXT("Q"), TEXT("E"), TEXT("R"), TEXT("RMB"), TEXT("L+R"), TEXT("HOLD RMB")};
		if (View && !View->InputText.IsEmpty())
		{
			const FString& Raw = View->InputText.ToString();
			if (Raw != TEXT("우클릭") && Raw != TEXT("좌+우") && Raw != TEXT("우클릭 차징"))
			{
				return View->InputText;
			}
		}
		return FText::FromString(Labels[Index]);
	}

	FText WeaponName(const FRogue10mHudIdentityView& Identity)
	{
		if (Identity.WeaponType == ERogue10mWeaponType::Unarmed)
		{
			return NSLOCTEXT("Rogue10mHUD", "WeaponUnarmed", "맨손");
		}
		if (Identity.WeaponType == ERogue10mWeaponType::Knuckle)
		{
			return NSLOCTEXT("Rogue10mHUD", "WeaponKnuckle", "너클");
		}
		// The run HUD supplies "<weapon> 숙련도 <level>"; reuse its live weapon caption.
		FString Name;
		return Identity.Label.ToString().Split(TEXT(" 숙련도 "), &Name, nullptr)
			? FText::FromString(MoveTemp(Name)) : Identity.Label;
	}

	class FHudPainter
	{
	public:
		FHudPainter(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
			const FLinearColor& Tint, const FSlateBrush& AtlasBrush)
			: Geometry(Geometry), Elements(Elements), LastLayer(Layer), Tint(Tint), AtlasBrush(AtlasBrush),
			  Placement(Geometry.GetLocalSize())
		{
		}

		int32 GetLastLayer() const { return LastLayer; }
		bool HasSpace() const { return Placement.Scale > 0.0f; }

		void DrawSkin(ESkin Skin, bool bCombat, float Fraction = 1.0f, const FRegion* Destination = nullptr)
		{
			Fraction = FMath::Clamp(Fraction, 0.0f, 1.0f);
			if (Fraction <= 0.0f) { return; }
			const FSprite& Entry = Sprite(Skin);
			const FRegion& Region = Destination ? *Destination : Entry.Design;
			const FVector2f UVMin = Entry.Atlas.Min() / AtlasSize;
			const FVector2f UVMax = UVMin + FVector2f(Entry.Atlas.Width * Fraction, Entry.Atlas.Height) / AtlasSize;
			FSlateBrush Brush = AtlasBrush;
			Brush.SetUVRegion(FBox2f(UVMin, UVMax));
			FSlateDrawElement::MakeBox(Elements, ++LastLayer,
				Geometry.ToPaintGeometry(FVector2f(Region.Width * Fraction, Region.Height), Placement.Transform(Region.Min(), bCombat)),
				&Brush, ESlateDrawEffect::None, Tint);
		}

		void DrawFill(ESkin Skin, float Fraction)
		{
			const FSprite& Fill = Sprite(Skin);
			const float Width = Fill.Design.Width * FMath::Clamp(Fraction, 0.0f, 1.0f);
			if (Width <= 0.0f) { return; }
			const float Height = Fill.Design.Height;
			const float Bevel = FMath::Min(Skin == ESkin::FillExp ? 3.5f : 2.5f, Width * 0.5f);
			const FVector2f Corners[] = {{0.0f, 0.0f}, {Width - Bevel, 0.0f}, {Width, Bevel},
				{Width, Height - Bevel}, {Width - Bevel, Height}, {0.0f, Height}};
			const FSlateRenderTransform RenderTransform = Geometry.GetAccumulatedRenderTransform();
			TArray<FSlateVertex> Vertices;
			Vertices.Reserve(UE_ARRAY_COUNT(Corners));
			for (const FVector2f& Corner : Corners)
			{
				const FVector2f Position = (Fill.Design.Min() + Corner) * Placement.Scale;
				const FVector2f UV = (Fill.Atlas.Min() + Corner) / AtlasSize;
				Vertices.Add(FSlateVertex::Make(RenderTransform, Position, UV, Tint.ToFColor(true)));
			}
			const TArray<SlateIndex> Indices{0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};
			DrawAtlasVertices(Vertices, Indices);
		}

		void DrawBox(const FRegion& Region, const FLinearColor& Color)
		{
			FSlateDrawElement::MakeBox(Elements, ++LastLayer,
				Geometry.ToPaintGeometry(Region.Size(), Placement.Transform(Region.Min(), true)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Tint * Color);
		}

		void DrawTexture(UTexture2D* Texture, const FRegion& Region, const FLinearColor& Color)
		{
			if (!Texture || Texture->GetSizeX() <= 0 || Texture->GetSizeY() <= 0) { return; }
			const FVector2f TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
			const FVector2f ContainedSize = TextureSize * FMath::Min(Region.Width / TextureSize.X, Region.Height / TextureSize.Y);
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.SetResourceObject(Texture);
			FSlateDrawElement::MakeBox(Elements, ++LastLayer,
				Geometry.ToPaintGeometry(ContainedSize, Placement.Transform(Region.Center() - 0.5f * ContainedSize, true)),
				&Brush, ESlateDrawEffect::None, Tint * Color);
		}

		void DrawText(const FText& Text, const FVector2f& Center, float Size, bool bCombat,
			float MaxWidth = 0.0f, const FRegion* Clip = nullptr)
		{
			if (Text.IsEmpty()) { return; }
			// Font sizes are in points, but the approved HUD and its atlas use design pixels.
			constexpr float TextReduction = 2.0f * 72.0f / FontConstants::RenderDPI;
			const float ReducedSize = Size - TextReduction;
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", ReducedSize);
			Font.OutlineSettings.OutlineSize = 1;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.015f, 0.011f, 0.007f, 0.92f);
			const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FVector2f TextSize(Measure->Measure(Text, Font));
			if (MaxWidth > 0.0f && TextSize.X > MaxWidth)
			{
				Font.Size = ReducedSize * MaxWidth / TextSize.X;
				TextSize = FVector2f(Measure->Measure(Text, Font));
			}
			if (Clip)
			{
				Elements.PushClip(FSlateClippingZone(
					Geometry.ToPaintGeometry(Clip->Size(), Placement.Transform(Clip->Min(), bCombat))));
			}
			FSlateDrawElement::MakeText(Elements, ++LastLayer,
				Geometry.ToPaintGeometry(TextSize, Placement.Transform(Center - 0.5f * TextSize, bCombat)),
				Text, Font, ESlateDrawEffect::None, FLinearColor(0.92f, 0.90f, 0.85f) * Tint);
			if (Clip)
			{
				Elements.PopClip();
			}
		}

		void DrawIdentityRing(float Fraction)
		{
			Fraction = FMath::Clamp(Fraction, 0.0f, 1.0f);
			if (Fraction <= 0.0f) { return; }
			if (Fraction >= 1.0f)
			{
				DrawSkin(ESkin::RingIdentity, true);
				return;
			}

			// Draw a clockwise texture wedge from twelve o'clock. The transparent
			// ring sprite preserves the reference's textured metal and rounded edges.
			const FSprite& Ring = Sprite(ESkin::RingIdentity);
			const FVector2f Center = Ring.Design.Center();
			const FVector2f Half = Ring.Design.Size() * 0.5f;
			const FSlateRenderTransform RenderTransform = Geometry.GetAccumulatedRenderTransform();
			const FVector2f Anchor = Placement.Offset(true);
			const auto Vertex = [&](const FVector2f& Local)
			{
				const FVector2f AtlasPoint = Ring.Atlas.Min() + (Local - Ring.Design.Min());
				return FSlateVertex::Make(RenderTransform, Anchor + Local * Placement.Scale, AtlasPoint / AtlasSize, Tint.ToFColor(true));
			};

			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Fraction * 128.0f));
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			Vertices.Reserve(Steps + 2);
			Indices.Reserve(Steps * 3);
			Vertices.Add(Vertex(Center));
			for (int32 Segment = 0; Segment <= Steps; ++Segment)
			{
				const float Angle = -UE_HALF_PI + UE_TWO_PI * Fraction * static_cast<float>(Segment) / Steps;
				const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
				const float EndDistance = FMath::Min(Half.X / FMath::Max(FMath::Abs(Direction.X), UE_SMALL_NUMBER),
					Half.Y / FMath::Max(FMath::Abs(Direction.Y), UE_SMALL_NUMBER));
				Vertices.Add(Vertex(Center + Direction * EndDistance));
				if (Segment > 0)
				{
					Indices.Append({0, static_cast<SlateIndex>(Segment), static_cast<SlateIndex>(Segment + 1)});
				}
			}
			DrawAtlasVertices(Vertices, Indices);
		}

	private:
		void DrawAtlasVertices(const TArray<FSlateVertex>& Vertices, const TArray<SlateIndex>& Indices)
		{
			const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(AtlasBrush);
			FSlateDrawElement::MakeCustomVerts(Elements, ++LastLayer, Resource, Vertices, Indices, nullptr, 0, 0);
		}

		const FGeometry& Geometry;
		FSlateWindowElementList& Elements;
		int32 LastLayer;
		const FLinearColor Tint;
		const FSlateBrush& AtlasBrush;
		const FHudPlacement Placement;
	};

	void PaintVitals(FHudPainter& Paint, const URogue10mCombatHudAWidget& HUD)
	{
		const bool bMana = HUD.IsUsingMana();
		const FRogue10mHudProgressionView& Progression = HUD.GetDisplayedProgression();
		const auto Vital = [&Paint](const FRogue10mHudVitalView& View, ESkin Plate, ESkin Fill,
			const FText& Label, float TextY)
		{
			if (!View.bVisible) { return; }
			Paint.DrawSkin(Plate, false);
			Paint.DrawFill(Fill, View.Normalized);
			const FText Text = FText::Format(NSLOCTEXT("Rogue10mHUD", "LayoutAVital", "{0} {1} / {2}"),
				Label, FText::AsNumber(FMath::RoundToInt(View.Current)), FText::AsNumber(FMath::RoundToInt(View.Max)));
			Paint.DrawText(Text, FVector2f(171, TextY), 14.5f, false, 249.0f);
		};
		Vital(HUD.GetDisplayedHealth(), ESkin::PlateHealth, ESkin::FillHealth,
			NSLOCTEXT("Rogue10mHUD", "HealthLabel", "체력"), 46.0f);
		Vital(HUD.GetDisplayedResource(), ESkin::PlateStamina, bMana ? ESkin::FillMana : ESkin::FillStamina,
			bMana ? NSLOCTEXT("Rogue10mHUD", "ManaLabel", "마력")
				: NSLOCTEXT("Rogue10mHUD", "StaminaLabel", "스태미나"), 93.0f);
		Paint.DrawSkin(ESkin::PlateLevel, false);
		Paint.DrawSkin(ESkin::PlateExp, false);
		Paint.DrawFill(ESkin::FillExp, Progression.ExperienceNormalized);
		Paint.DrawText(FText::Format(NSLOCTEXT("Rogue10mHUD", "BottomLevel", "LV {0}"),
			FText::AsNumber(FMath::Max(1, Progression.Level))), FVector2f(63, 133), 13.0f, false, 53.0f);
	}

	void PaintSkills(FHudPainter& Paint, const URogue10mCombatHudAWidget& HUD)
	{
		const auto& Skills = HUD.GetDisplayedSkills();
		const bool bGreatsword = HUD.GetDisplayedIdentity().WeaponType == ERogue10mWeaponType::GreatSword;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SkillPlates); ++Index)
		{
			const FRogue10mHudQuickSlotView* View = Skills.IsValidIndex(Index) ? &Skills[Index] : nullptr;
			const ESkin Plate = SkillPlates[Index];
			const FRegion& Region = Sprite(Plate).Design;
			const FText Key = CompactKey(View, Index);
			// Slot art is the reference fallback. Authored equipment icons still win
			// when the equipped ability has a distinct glyph.
			const bool bApprovedArt = bGreatsword && (!View || !View->SkillIcon)
				&& Key.EqualTo(CompactKey(nullptr, Index));
			Paint.DrawSkin(bApprovedArt ? Plate : ESkin::SlotGeneric, true, 1.0f, &Region);
			const FRegion Icon{Region.X + 10.0f, Region.Y + 10.0f, Region.Width - 20.0f, 58.0f};
			if (!bApprovedArt)
			{
				Paint.DrawTexture(View ? View->SkillIcon.Get() : nullptr, Icon, View && View->bUnlocked ? View->IconColor : FLinearColor::White);
				const FRegion KeyBand{Region.X + 10.0f, Region.Y + 72.0f, Region.Width - 20.0f, 24.0f};
				Paint.DrawText(Key, KeyBand.Center(), 16.0f, true, KeyBand.Width - 6.0f, &KeyBand);
			}
			if (!View || !View->bUnlocked)
			{
				const FRegion Lock = bApprovedArt
					? FRegion{Icon.X + Icon.Width - 14.0f, Icon.Y + Icon.Height - 17.0f, 16.0f, 20.0f}
					: FRegion{Icon.Center().X - 10.0f, Icon.Center().Y - 12.5f, 20.0f, 25.0f};
				// A small corner badge keeps the approved sword icon visible
				// while accurately identifying an action that is not available.
				Paint.DrawBox(bApprovedArt ? Lock.Inset(-2.0f, -2.0f) : Icon,
					FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
				Paint.DrawSkin(ESkin::GlyphLock, true, 1.0f, &Lock);
			}
			else if (View->CooldownRemaining > 0.0f)
			{
				Paint.DrawBox(Icon, FLinearColor(0.0f, 0.0f, 0.0f, 0.60f));
				Paint.DrawText(FText::FromString(FString::Printf(TEXT("%.1f"), View->CooldownRemaining)), Icon.Center(), 19.0f, true);
			}
		}

		Paint.DrawSkin(ESkin::SlotAlt, true);
		const FRegion DodgeKeyBand{430, 834, 45, 19};
		Paint.DrawText(Rogue10mInput::DodgeKeyLabel(), DodgeKeyBand.Center(), 14.0f, true,
			DodgeKeyBand.Width - 6.0f, &DodgeKeyBand);
		if (HUD.GetDodgeCooldownRemaining() > 0.0f || HUD.IsDodging())
		{
			const FRegion& Alt = Sprite(ESkin::SlotAlt).Design;
			const FRegion Icon{Alt.X + 10, Alt.Y + 9, Alt.Width - 20, 55};
			Paint.DrawBox(Icon, FLinearColor(0, 0, 0, 0.48f));
			if (HUD.GetDodgeCooldownRemaining() > 0.0f)
			{
				Paint.DrawText(FText::FromString(FString::Printf(TEXT("%.1f"), HUD.GetDodgeCooldownRemaining())),
					Icon.Center(), 16.0f, true);
			}
		}

		const auto& Items = HUD.GetDisplayedItems();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ItemPlates); ++Index)
		{
			const ESkin Plate = ItemPlates[Index];
			const FRogue10mHudQuickSlotView* Item = Items.IsValidIndex(Index) ? &Items[Index] : nullptr;
			const FRegion& Region = Sprite(Plate).Design;
			const bool bHasItem = Item && Item->bUnlocked;
			Paint.DrawSkin(bHasItem ? ESkin::SlotGeneric : Plate, true, 1.0f, &Region);
			if (bHasItem)
			{
				Paint.DrawTexture(Item->SkillIcon.Get(), Region.Inset(8, 8), Item->IconColor);
			}
		}
	}

	void PaintIdentity(FHudPainter& Paint, const URogue10mCombatHudAWidget& HUD)
	{
		const FRogue10mHudIdentityView& Identity = HUD.GetDisplayedIdentity();
		Paint.DrawSkin(ESkin::PlateIdentity, true);
		if (HUD.ShowsIdentityPercent())
		{
			Paint.DrawIdentityRing(Identity.Normalized);
		}
		if (Identity.WeaponType == ERogue10mWeaponType::GreatSword)
		{
			Paint.DrawSkin(ESkin::IdentitySword, true);
		}
		else
		{
			Paint.DrawTexture(HUD.GetDisplayedIdentityIcon(), {1165, 751, 66, 75}, FLinearColor::White);
		}
		Paint.DrawText(WeaponName(Identity), FVector2f(1198, HUD.ShowsIdentityPercent() ? 844.0f : 851.0f),
			15.0f, true, 112.0f);
		if (HUD.ShowsIdentityPercent())
		{
			const float Percent = FMath::Clamp(Identity.Normalized, 0.0f, 1.0f) * 100.0f;
			Paint.DrawText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Percent)), FVector2f(1198, 866), 14.0f, true);
		}
		if (HUD.ShowsIdentityKey())
		{
			Paint.DrawSkin(ESkin::PlateCtrl, true);
			const FRegion IdentityKeyBand{1275, 857, 39, 20};
			Paint.DrawText(Rogue10mInput::IdentityKeyLabel(), IdentityKeyBand.Center(), 14.0f, true,
				IdentityKeyBand.Width - 6.0f, &IdentityKeyBand);
		}
	}

	constexpr int32 SkillZoneCount = UE_ARRAY_COUNT(SkillPlates);
	constexpr int32 IdentityZone = SkillZoneCount;
	constexpr int32 ExperienceZone = IdentityZone + 1;
}

class SCombatHudAHitZone final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCombatHudAHitZone) {}
		SLATE_ARGUMENT(URogue10mCombatHudAWidget*, HUD)
		SLATE_ARGUMENT(int32, Index)
	SLATE_END_ARGS()

	void Construct(const FArguments& Arguments)
	{
		HUD = Arguments._HUD;
		Index = Arguments._Index;
		SetToolTip(SNew(SToolTip).Text(this, &SCombatHudAHitZone::ToolTipText));
	}

private:
	FText ToolTipText() const
	{
		if (!RejectedDropText.IsEmpty()) { return RejectedDropText; }
		const URogue10mCombatHudAWidget* Widget = HUD.Get();
		if (!Widget) { return FText::GetEmpty(); }
		if (Index == IdentityZone)
		{
			const FRogue10mHudIdentityView& Identity = Widget->GetDisplayedIdentity();
			return Identity.ResourceLabel.IsEmpty() ? Identity.Label
				: FText::Format(NSLOCTEXT("Rogue10mHUD", "LayoutAIdentityTooltip", "{0}\n{1}"),
					Identity.Label, Identity.ResourceLabel);
		}
		if (Index == ExperienceZone)
		{
			const FRogue10mHudProgressionView& View = Widget->GetDisplayedProgression();
			const float Remaining = FMath::Max(0.0f, View.ExperienceToNextLevel - View.CurrentExperience);
			return FText::Format(NSLOCTEXT("Rogue10mHUD", "LayoutAXPTooltip",
				"경험치\n현재 경험치: {0} / {1}\n다음 레벨까지: {2}"),
				FText::AsNumber(View.CurrentExperience), FText::AsNumber(View.ExperienceToNextLevel),
				FText::AsNumber(Remaining));
		}
		const auto& Slots = Widget->GetDisplayedSkills();
		if (!Slots.IsValidIndex(Index)) { return FText::GetEmpty(); }
		const FRogue10mHudQuickSlotView& View = Slots[Index];
		const FText Key = CompactKey(&View, Index);
		return View.bUnlocked
			? FText::Format(NSLOCTEXT("Rogue10mHUD", "CompactSlotToolTip", "{0}: {1}"), Key, View.DisplayName)
			: FText::Format(NSLOCTEXT("Rogue10mHUD", "LayoutALockedSkill", "{0}: 해금되지 않음"), Key);
	}

	FReply OnDrop(const FGeometry&, const FDragDropEvent& DragEvent) override
	{
		if (Index >= SkillZoneCount) { return FReply::Unhandled(); }
		const TSharedPtr<FUMGDragDropOp> Drag = DragEvent.GetOperationAs<FUMGDragDropOp>();
		URogue10mCombatHudAWidget* Widget = HUD.Get();
		const auto* SkillDrag = Drag ? Cast<URogue10mSkillDragDropOperation>(Drag->GetOperation()) : nullptr;
		if (!Widget || !SkillDrag || !SkillDrag->bUnlocked || !SkillDrag->SkillData)
		{
			return FReply::Unhandled();
		}
		if (Widget->TryAssignSkill(Index, SkillDrag->SkillData))
		{
			RejectedDropText = FText::GetEmpty();
			return FReply::Handled();
		}
		RejectedDropText = SkillDrag->SkillData->GetAllowedSlotText();
		return FReply::Unhandled();
	}

	void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		RejectedDropText = FText::GetEmpty();
		SLeafWidget::OnMouseLeave(MouseEvent);
	}

	FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&,
		int32 Layer, const FWidgetStyle&, bool) const override { return Layer; }

	TWeakObjectPtr<URogue10mCombatHudAWidget> HUD;
	FText RejectedDropText;
	int32 Index = INDEX_NONE;
};

namespace
{
	class SCombatHudA final : public SPanel
	{
	public:
		SLATE_BEGIN_ARGS(SCombatHudA) {}
			SLATE_ARGUMENT(URogue10mCombatHudAWidget*, HUD)
			SLATE_ARGUMENT(UTexture2D*, Atlas)
		SLATE_END_ARGS()

		SCombatHudA() : HitZones(this) {}

		void Construct(const FArguments& Arguments)
		{
			HUD = Arguments._HUD;
			AtlasBrush.SetResourceObject(Arguments._Atlas);
			AtlasBrush.DrawAs = ESlateBrushDrawType::Image;
			SetVisibility(EVisibility::SelfHitTestInvisible);
			for (int32 Index = 0; Index <= ExperienceZone; ++Index)
			{
				HitZones.Add(SNew(SCombatHudAHitZone).HUD(Arguments._HUD).Index(Index));
			}
		}

	private:
		FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		FChildren* GetChildren() override { return &HitZones; }

		void OnArrangeChildren(const FGeometry& Geometry, FArrangedChildren& Arranged) const override
		{
			const FHudPlacement Place(Geometry.GetLocalSize());
			for (int32 Index = 0; Index < HitZones.Num(); ++Index)
			{
				const TSharedRef<SWidget> Zone = ConstCastSharedRef<SWidget>(HitZones.GetChildAt(Index));
				const ESkin Plate = Index < SkillZoneCount ? SkillPlates[Index]
					: Index == IdentityZone ? ESkin::PlateIdentity : ESkin::PlateExp;
				const FRegion Region = Sprite(Plate).Design.Inset(5.0f, 5.0f);
				Arranged.AddWidget(Zone->GetVisibility(), Geometry.MakeChild(Zone, Region.Size(),
					Place.Transform(Region.Min(), Index != ExperienceZone)));
			}
		}

		int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull, FSlateWindowElementList& Elements,
			int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
		{
			const URogue10mCombatHudAWidget* Widget = HUD.Get();
			if (!Widget || !Widget->HasAtlas()) { return Layer; }
			FHudPainter Paint(Geometry, Elements, Layer, Style.GetColorAndOpacityTint(), AtlasBrush);
			if (!Paint.HasSpace()) { return Layer; }
			PaintVitals(Paint, *Widget);
			PaintSkills(Paint, *Widget);
			PaintIdentity(Paint, *Widget);
			return SPanel::OnPaint(Args, Geometry, Cull, Elements, Paint.GetLastLayer(), Style, bEnabled);
		}

		TWeakObjectPtr<URogue10mCombatHudAWidget> HUD;
		FSlateBrush AtlasBrush;
		TSlotlessChildren<SWidget> HitZones;
	};
}

URogue10mCombatHudAWidget::URogue10mCombatHudAWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Art(
		TEXT("/Game/UI/HUD/CombatHudA/T_HudA_Atlas.T_HudA_Atlas"), LOAD_NoWarn | LOAD_Quiet);
	Atlas = Art.Get();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URogue10mCombatHudAWidget::SetOwningPlayerController(APlayerController* Player)
{
	OwningPlayer = Player;
}

TSharedRef<SWidget> URogue10mCombatHudAWidget::RebuildWidget()
{
	return SNew(SCombatHudA).HUD(this).Atlas(Atlas.Get());
}

void URogue10mCombatHudAWidget::SetFrequentViews(const FRogue10mHudVitalView& Health, const FRogue10mHudVitalView& Stamina,
	const FRogue10mHudVitalView& Mana, const FRogue10mHudProgressionView& Progression,
	const FRogue10mHudIdentityView& Identity, bool bUseMana, float CurrentDodgeCooldown, bool bCurrentDodging)
{
	if (Identity.WeaponType != IdentityView.WeaponType || Identity.IconTexture != IdentityView.IconTexture)
	{
		ResolvedIdentityIcon = Identity.WeaponType == ERogue10mWeaponType::GreatSword ? nullptr : Identity.IconTexture.LoadSynchronous();
	}
	HealthView = Health;
	bUsingMana = bUseMana && Mana.bVisible;
	ResourceView = bUsingMana ? Mana : Stamina;
	ProgressionView = Progression;
	IdentityView = Identity;
	DodgeCooldownRemaining = CurrentDodgeCooldown;
	bIsDodging = bCurrentDodging;
	InvalidatePaint();
}

void URogue10mCombatHudAWidget::SetSlowViews(const TArray<FRogue10mHudQuickSlotView>& Skills,
	const TArray<FRogue10mHudQuickSlotView>& Items)
{
	SkillViews = Skills;
	ItemViews = Items;
	InvalidatePaint();
}

bool URogue10mCombatHudAWidget::TryAssignSkill(int32 Index, URogue10mAttackSkillData* Skill)
{
	const APlayerController* Player = OwningPlayer.Get();
	const ARogue10mCharacter* Character = Player ? Cast<ARogue10mCharacter>(Player->GetPawn()) : nullptr;
	URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!Skill || !Combat || !SkillViews.IsValidIndex(Index)) { return false; }
	FRogue10mHudQuickSlotView& SlotView = SkillViews[Index];
	if (!Combat->AssignSkillToInputSlot(Skill, SlotView.InputSlot)) { return false; }
	SlotView.SkillData = Skill;
	SlotView.DisplayName = Skill->SkillName;
	SlotView.SkillIcon = Skill->SkillIcon;
	SlotView.IconColor = Skill->IconTint;
	SlotView.bUnlocked = true;
	InvalidatePaint();
	return true;
}

void URogue10mCombatHudAWidget::InvalidatePaint()
{
	if (const TSharedPtr<SWidget> Widget = GetCachedWidget())
	{
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FBox2D URogue10mCombatHudAWidget::GetVitalsBounds(const FVector2D& ViewportSize)
{
	return FHudPlacement(ViewportSize).Bounds({26, 22}, {317, 154}, false);
}

FBox2D URogue10mCombatHudAWidget::GetCombatBounds(const FVector2D& ViewportSize)
{
	return FHudPlacement(ViewportSize).Bounds({413, 732}, {1329, 912}, true);
}
