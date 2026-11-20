#include "Widgets/Rogue10mCommonSkillChrome.h"

#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/WidgetStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"

namespace Rogue10mCommonSkillChrome
{
	namespace
	{
		TArray<FVector2f> Corners(FVector2f Size, float Inset, float Chamfer)
		{
			const float Left = Inset;
			const float Right = Size.X - Inset;
			const float Top = Inset;
			const float Bottom = Size.Y - Inset;
			const float Cut = FMath::Clamp(Chamfer, 0.0f, 0.5f * FMath::Min(Right - Left, Bottom - Top));
			return {{Left + Cut, Top}, {Right - Cut, Top}, {Right, Top + Cut}, {Right, Bottom - Cut},
				{Right - Cut, Bottom}, {Left + Cut, Bottom}, {Left, Bottom - Cut}, {Left, Top + Cut}};
		}

		class SCommonSkillSurface final : public SLeafWidget
		{
		public:
			SLATE_BEGIN_ARGS(SCommonSkillSurface) {}
				SLATE_ARGUMENT(ESurface, Kind)
				SLATE_ARGUMENT(UTexture2D*, Charcoal)
			SLATE_END_ARGS()

			void Construct(const FArguments& Args)
			{
				Kind = Args._Kind;
				SetVisibility(EVisibility::HitTestInvisible);
				if (Args._Charcoal)
				{
					Charcoal.Reset(Args._Charcoal);
					TextureBrush.SetResourceObject(Charcoal.Get());
					TextureBrush.DrawAs = ESlateBrushDrawType::Image;
				}
			}

		private:
			virtual FVector2D ComputeDesiredSize(float) const override
			{
				return Kind == ESurface::Rule ? FVector2D(0.0, 8.0) : FVector2D::ZeroVector;
			}

			virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
				FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
			{
				const FVector2f Size(Geometry.GetLocalSize());
				if (Size.X < 12.0f || Size.Y < 4.0f) return Layer;
				const FLinearColor Tint = Style.GetColorAndOpacityTint();
				const auto Stroke = [&](int32 Offset, TArray<FVector2f> Points, const FLinearColor& Color, float Width)
				{
					FSlateDrawElement::MakeLines(Elements, Layer + Offset, Geometry.ToPaintGeometry(), MoveTemp(Points),
						ESlateDrawEffect::None, Color * Tint, true, Width);
				};
				const auto Edge = [&](int32 Offset, float Inset, float Chamfer, const FLinearColor& Color, float Width)
				{
					auto Points = Corners(Size, Inset, Chamfer);
					const FVector2f First = Points[0];
					Points.Add(First);
					Stroke(Offset, MoveTemp(Points), Color, Width);
				};
				if (Kind == ESurface::Rule)
				{
					const float Y = Size.Y * 0.5f;
					const TArray<FVector2f> Points{{1.0f, Y}, {Size.X * 0.22f, Y}, {Size.X * 0.5f, Y},
						{Size.X * 0.78f, Y}, {Size.X - 1.0f, Y}};
					const TArray<FLinearColor> Colors{FLinearColor::Transparent, FLinearColor(0.32f, 0.13f, 0.034f),
						BurnishedGold, FLinearColor(0.32f, 0.13f, 0.034f), FLinearColor::Transparent};
					FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, Colors,
						ESlateDrawEffect::None, Tint, true, 1.2f);
					return Layer;
				}

				const bool bWindow = Kind == ESurface::Window;
				const bool bPanel = Kind == ESurface::Detail || Kind == ESurface::Graph;
				const bool bPlaque = Kind == ESurface::Plaque;
				const float Cut = bWindow ? 17.0f : bPanel ? 9.0f : 7.0f;
				const auto Fill = [&](int32 Offset, const FSlateBrush& Brush, const FLinearColor& Top, const FLinearColor& Bottom, bool bTexture)
				{
					const auto Shape = Corners(Size, bWindow ? 2.0f : 1.1f, Cut);
					const auto Transform = Geometry.GetAccumulatedRenderTransform();
					const auto MakeVertex = [&](FVector2f Point)
					{
						const FVector2f UV = bTexture ? Point / Size : FVector2f::ZeroVector;
						const FLinearColor Color = FMath::Lerp(Top, Bottom, Point.Y / Size.Y) * Tint;
						return FSlateVertex::Make(Transform, Point, UV, Color.ToFColor(true));
					};
					TArray<FSlateVertex> Vertices;
					Vertices.Reserve(Shape.Num() + 1);
					for (FVector2f Point : Shape) Vertices.Add(MakeVertex(Point));
					Vertices.Add(MakeVertex(Size * 0.5f));
					TArray<SlateIndex> Indices;
					Indices.Reserve(Shape.Num() * 3);
					const auto Center = static_cast<SlateIndex>(Shape.Num());
					for (SlateIndex I = 0; I < Center; ++I)
					{
						Indices.Append({Center, I, static_cast<SlateIndex>((I + 1) % Center)});
					}
					const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush);
					FSlateDrawElement::MakeCustomVerts(Elements, Layer + Offset, Resource, Vertices, Indices, nullptr, 0, 0);
				};
				const FLinearColor Top = bWindow ? FLinearColor(0.013f, 0.011f, 0.010f) : FLinearColor(0.007f, 0.006f, 0.005f);
				const FLinearColor Bottom = bPlaque ? FLinearColor(0.037f, 0.022f, 0.010f) : FLinearColor(0.003f, 0.0027f, 0.0024f);
				Fill(0, *FCoreStyle::Get().GetBrush("WhiteBrush"), Top, Bottom, false);
				if (Charcoal.IsValid() && (bWindow || Kind == ESurface::Detail))
				{
					Fill(1, TextureBrush, FLinearColor(0.86f, 0.81f, 0.76f, 0.56f),
						FLinearColor(0.67f, 0.66f, 0.64f, 0.56f), true);
				}

				const FLinearColor Antique(0.27f, 0.135f, 0.054f);
				Edge(2, 1.1f, Cut, FLinearColor(0.001f, 0.001f, 0.001f, 0.85f), bWindow ? 6.0f : 3.5f);
				Edge(3, 1.7f, Cut, Antique, bWindow ? 1.8f : 1.0f);
				Edge(4, bWindow ? 6.6f : 4.0f, Cut * 0.67f, Antique * 0.33f, 0.8f);

				const float X = Cut + 2.0f;
				const FLinearColor Highlight(0.58f, 0.37f, 0.16f);
				const TArray<FLinearColor> Highlights{Antique * 0.65f, Highlight, Highlight * 0.58f, Antique * 0.65f};
				for (float Y : {2.0f, Size.Y - 2.0f})
				{
					const TArray<FVector2f> Points{{X, Y}, {Size.X * 0.22f, Y}, {Size.X * 0.71f, Y}, {Size.X - X, Y}};
					FSlateDrawElement::MakeLines(Elements, Layer + 5, Geometry.ToPaintGeometry(), Points, Highlights,
						ESlateDrawEffect::None, Tint, true, 0.8f);
				}
				if (bPanel)
				{
					for (const FVector2f Corner : {FVector2f(0.0f, 0.0f), FVector2f(Size.X, 0.0f),
						FVector2f(Size.X, Size.Y), FVector2f(0.0f, Size.Y)})
					{
						const FVector2f Direction(Corner.X == 0.0f ? 1.0f : -1.0f, Corner.Y == 0.0f ? 1.0f : -1.0f);
						const auto P = [&](float XOffset, float YOffset) { return Corner + Direction * FVector2f(XOffset, YOffset); };
						Stroke(6, {P(5, 29), P(5, 17), P(13, 9), P(23, 6), P(30, 6)}, Antique, 1.0f);
						Stroke(6, {P(6, 13), P(13, 6), P(18, 11), P(11, 18), P(6, 13)}, Highlight, 1.0f);
					}
				}
				return Layer + 6;
			}

			ESurface Kind = ESurface::Window;
			TStrongObjectPtr<UTexture2D> Charcoal;
			FSlateBrush TextureBrush;
		};
	}

	TSharedRef<SWidget> MakeSurface(ESurface Kind, UTexture2D* Charcoal)
	{
		return SNew(SCommonSkillSurface).Kind(Kind).Charcoal(Charcoal);
	}

	void StyleText(UTextBlock& Text, int32 Size, bool bHeading, FLinearColor Color)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = bHeading ? TEXT("Bold") : TEXT("Regular");
		Font.OutlineSettings.OutlineSize = bHeading ? 1 : 0;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.01f, 0.006f, 0.002f, 0.95f);
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
		Text.SetShadowOffset(FVector2D(0.0, bHeading ? 2.0 : 1.0));
	}
}
