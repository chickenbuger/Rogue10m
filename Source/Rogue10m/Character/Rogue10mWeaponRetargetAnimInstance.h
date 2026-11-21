// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Rogue10mWeaponRetargetAnimInstance.generated.h"

/** Smooth a change between compatible body retargeters using the last displayed pose. */
UCLASS(Transient, Blueprintable)
class ROGUE10M_API URogue10mWeaponRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation|Weapon", meta=(ClampMin="0.0", Units="s"))
	float EquipmentBlendSeconds = 0.18f;

protected:
	void NativeInitializeAnimation() override;
	FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
