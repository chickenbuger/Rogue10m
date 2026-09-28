// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Rogue10mCameraManager.generated.h"

class URogue10mFirstPersonPresentationComponent;

/** First-person view limits and cosmetic motion applied after the base camera calculation. */
UCLASS()
class ARogue10mCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ARogue10mCameraManager();

	/** Full body can be inspected by looking almost straight down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|First Person", meta=(ClampMin="-89", ClampMax="-70", Units="deg"))
	float FullBodyViewPitchMin = -85.0f;

protected:
	virtual void LimitViewPitch(FRotator& ViewRotation, float InViewPitchMin, float InViewPitchMax) override;
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	TWeakObjectPtr<URogue10mFirstPersonPresentationComponent> LastFistPresentation;
};
