#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

enum class ERogue10mLiveQueryShape : uint8 { Sphere, Box, Capsule };

/** Value-only snapshot of a supported simple-query shape from the current physics scene. */
struct FRogue10mLiveQueryShape
{
    ERogue10mLiveQueryShape Type=ERogue10mLiveQueryShape::Sphere;
    FVector Center=FVector::ZeroVector, Extent=FVector::ZeroVector;
    FQuat Rotation=FQuat::Identity;
    float Radius=0.f, HalfHeight=0.f;
};

namespace Rogue10mLiveCollisionDebug
{
    /** Appends live primitives eligible for GatherAttackTargets' simple Pawn/Dynamic/PhysicsBody object query.
     *  Does not query overlaps, evaluate animation, mutate physics or fall back to authored body/actor bounds.
     *  Unsupported counts eligible shapes that cannot be represented; Unresolved counts unavailable live bodies/poses.
     */
    void ReadQueryPrimitives(UPrimitiveComponent& Primitive,TArray<FRogue10mLiveQueryShape>& OutShapes,
        int32& Unsupported,int32& Unresolved);
}
