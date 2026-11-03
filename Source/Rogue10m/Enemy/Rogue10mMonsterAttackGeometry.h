#pragma once

#include "CoreMinimal.h"
#include "Rogue10mMonsterAttackPatternProfile.h"

/** Locked narrow-phase geometry. Damage also requires physical overlap, selected target and visibility. */
struct ROGUE10M_API FRogue10mMonsterAttackShape
{
    ERogue10mMonsterAttackType AttackType = ERogue10mMonsterAttackType::Basic;
    ERogue10mMonsterPatternState State = ERogue10mMonsterPatternState::Idle;
    FVector SourceLocation = FVector::ZeroVector;
    FVector Location = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    float Range = 0.0f;
    float HalfWidth = 0.0f;
    float HalfHeight = 0.0f;
    float ArcDegrees = 0.0f;
    float TargetCapsuleRadius = 0.0f;
    float TargetCapsuleHalfHeight = 0.0f;
    float WindupRemaining = 0.0f;
    float WindupDuration = 0.0f;
    bool bActive = false;
    bool bWindup = false;
};

/** Floor projection of narrow-phase target-center positions, expanded by the selected capsule. */
struct ROGUE10M_API FRogue10mMonsterWarningProjection
{
    FVector Center = FVector::ZeroVector;
    float HalfForward = 0.0f;
    float HalfRight = 0.0f;
    float CapsuleRadiusFraction = 0.0f;
};

namespace Rogue10mMonsterAttackGeometry
{
    ROGUE10M_API bool ContainsCapsule(const FRogue10mMonsterAttackShape& Shape, const FVector& CapsuleCenter,
        float CapsuleRadius, float CapsuleHalfHeight);
    ROGUE10M_API FRogue10mMonsterWarningProjection MakeWarningProjection(
        const FRogue10mMonsterAttackShape& Shape, float CapsuleRadius);
}
