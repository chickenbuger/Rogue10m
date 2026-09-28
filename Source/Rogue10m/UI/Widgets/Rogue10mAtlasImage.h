// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Rogue10mAtlasImage.generated.h"

/** Draws a texture region while keeping its UV bounds serialized in the widget asset. */
UCLASS(BlueprintType, meta=(DisplayName="Rogue10m Atlas Image"))
class ROGUE10M_API URogue10mAtlasImage : public UImage
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|HUD|Atlas")
	FVector2D UVMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|HUD|Atlas")
	FVector2D UVMax = FVector2D(1.0, 1.0);

	/** Call after changing UV bounds at runtime; designer synchronization calls this automatically. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Atlas")
	void ApplyUVRegion();
};
