// Opt-in presentation data. Gameplay attack timing and movement speed remain independent.
#pragma once
#include "CoreMinimal.h"
#include "Rogue10mWeightedFirstPersonSettings.generated.h"

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mWeightedFirstPersonSettings
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(ClampMin="70", ClampMax="110")) float WorldFieldOfView = 86.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(ClampMin="-60", ClampMax="0")) float InitialViewPitch = 0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="1")) float HeadMotionScale = 1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="1")) float HeadRotationWeight = 0.10f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="2")) float WalkingRotationDegrees = 0.75f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="4")) float RunningRotationDegrees = 1.25f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="3")) float AttackingRotationDegrees = 1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="5")) float WalkingPositionCm = 0.35f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="5")) float RunningPositionCm = 0.65f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="5")) float AttackingPositionCm = 0.45f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Comfort", meta=(ClampMin="0", ClampMax="60")) float HeadFollowSpeed = 16.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="1")) float HandMotionScale = 1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="6")) float IdleArmDropDegrees = 3.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="3")) float MovementHandSwayDegrees = 1.25f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="6")) float AttackWindupDegrees = 3.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="6")) float AttackRecoveryDegrees = 2.0f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0.05", ClampMax="1")) float GuardRaiseSeconds = 0.28f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0.05", ClampMax="2")) float GuardLowerSeconds = 0.60f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hands", meta=(ClampMin="0", ClampMax="60")) float UpperBodyFollowSpeed = 14.0f;
 FRogue10mWeightedFirstPersonSettings Sanitized() const;
};

namespace Rogue10mWeightedFirstPerson
{
 ROGUE10M_API bool IsRequested();
 ROGUE10M_API float ConsoleMotionScale();
 ROGUE10M_API float ResolveFieldOfView(float ProfileValue);
}
