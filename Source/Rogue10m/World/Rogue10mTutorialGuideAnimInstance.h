// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/AnimInstance.h"
#include "Rogue10mTutorialGuideAnimInstance.generated.h"

class UAnimSequence;

/** Runtime pose transitions for a guide's existing compatible in-place clips. */
UCLASS(Transient, BlueprintType)
class ROGUE10M_API URogue10mTutorialGuideAnimInstance final : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetPlayback(UAnimSequence* Sequence, float Rate, float BlendSeconds);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	UAnimSequence* GetPlaybackSequence() const { return PlaybackSequence; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	float GetPlaybackRate() const { return PlaybackRate; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	float GetPlaybackPosition() const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PlaybackSequence;

	float PlaybackRate = 1.0f;
};
