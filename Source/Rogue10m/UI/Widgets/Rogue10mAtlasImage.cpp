// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Rogue10mAtlasImage.h"

#include "Math/Box2D.h"
#include "Styling/SlateBrush.h"

void URogue10mAtlasImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyUVRegion();
}

void URogue10mAtlasImage::ApplyUVRegion()
{
	const FVector2f Min(
		FMath::Clamp(static_cast<float>(UVMin.X), 0.0f, 1.0f),
		FMath::Clamp(static_cast<float>(UVMin.Y), 0.0f, 1.0f));
	const FVector2f Max(
		FMath::Clamp(static_cast<float>(UVMax.X), 0.0f, 1.0f),
		FMath::Clamp(static_cast<float>(UVMax.Y), 0.0f, 1.0f));
	FSlateBrush RegionBrush = GetBrush();
	RegionBrush.SetUVRegion(Max.X > Min.X && Max.Y > Min.Y
		? FBox2f(Min, Max) : FBox2f(FVector2f::ZeroVector, FVector2f(1.0f, 1.0f)));
	// UImage::SetBrush only updates the brush and invalidates Slate; it does not resynchronize.
	SetBrush(RegionBrush);
}
