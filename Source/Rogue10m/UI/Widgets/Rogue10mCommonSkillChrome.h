#pragma once

#include "CoreMinimal.h"

class SWidget;
class UTextBlock;
class UTexture2D;

namespace Rogue10mCommonSkillChrome
{
	enum class ESurface : uint8 { Window, Detail, Graph, Plaque, Rule };

	inline const FLinearColor CopperPaper(0.95f, 0.86f, 0.70f);
	inline const FLinearColor BurnishedGold(1.0f, 0.72f, 0.35f);
	inline const FLinearColor SecondaryText(0.68f, 0.60f, 0.49f);

	TSharedRef<SWidget> MakeSurface(ESurface Kind, UTexture2D* Charcoal = nullptr);
	void StyleText(UTextBlock& Text, int32 Size, bool bHeading = false, FLinearColor Color = CopperPaper);
}
