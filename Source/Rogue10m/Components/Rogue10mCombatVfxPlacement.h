#pragma once

#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

namespace Rogue10m::CombatVfx
{

/** Presentation-only near-side AABB estimate, never a hit/contact query. */
inline FVector NearSideBoundsLocation(const FVector& BoundsOrigin, const FVector& BoundsExtent,
	const FVector& AttackerLocation, float SurfaceOffset)
{
	const FVector TowardAttacker = (AttackerLocation - BoundsOrigin).GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
	{
		return BoundsOrigin;
	}

	// Intersect the center-to-attacker ray with the horizontal bounds rectangle.
	// min(Extent.X, Extent.Y) alone leaves effects inside non-square targets.
	double SurfaceDistance = TNumericLimits<double>::Max();
	if (FMath::Abs(TowardAttacker.X) > UE_SMALL_NUMBER)
	{
		SurfaceDistance = FMath::Min(SurfaceDistance,
			FMath::Max(0.0, BoundsExtent.X) / FMath::Abs(TowardAttacker.X));
	}
	if (FMath::Abs(TowardAttacker.Y) > UE_SMALL_NUMBER)
	{
		SurfaceDistance = FMath::Min(SurfaceDistance,
			FMath::Max(0.0, BoundsExtent.Y) / FMath::Abs(TowardAttacker.Y));
	}
	return BoundsOrigin + TowardAttacker * (SurfaceDistance + FMath::Max(0.0f, SurfaceOffset));
}

} // namespace Rogue10m::CombatVfx
