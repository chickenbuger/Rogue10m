#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMeshComponent;
class UWorld;
enum class ERogue10mAttackShape : uint8;

namespace Rogue10m::MeleeVfx
{
struct FImpactPoint
{
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
};

/** Ground-plane projection of the resolved query. The last vertex joins the first. */
TArray<FVector> MakeRangeFootprint(ERogue10mAttackShape Shape, const FVector& QueryCenter,
	const FQuat& QueryRotation, const FVector& QueryExtent, const FVector& Forward, float ArcAngleDegrees);

/** As above, lifted slightly above nearby static walkable surfaces. */
TArray<FVector> MakeRangeOutline(const UWorld& World, const AActor* Attacker, ERogue10mAttackShape Shape,
	const FVector& QueryCenter, const FQuat& QueryRotation, const FVector& QueryExtent,
	const FVector& Forward, float ArcAngleDegrees, double FallbackFeetZ);

/** Visible project greatsword, measured in weapon mesh local centimeters. */
bool GetBladeSegment(const UStaticMeshComponent& WeaponMesh, FVector& OutBase, FVector& OutTip);

FImpactPoint FindCapsuleImpact(const FVector& Center, const FVector& Up, double Radius, double HalfHeight,
	const FVector& AttackStart, const FVector& AttackEnd, const FVector& AttackOrigin);

/** Presentation location for a confirmed overlap hit. A hand can pass the same point at both ends. */
FImpactPoint FindImpact(const AActor& Target, const FVector& AttackStart, const FVector& AttackEnd,
	const FVector& AttackOrigin);
}
