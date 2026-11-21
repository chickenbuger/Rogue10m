#include "Rogue10mMeleeVfxGeometry.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatVfxPlacement.h"

namespace Rogue10m::MeleeVfx
{
namespace
{
constexpr int32 CurveSteps = 24;
constexpr double GroundLiftCm = 2.5;

double SignedTurn(const FVector& A, const FVector& B, const FVector& C)
{
	return FVector::CrossProduct(B - A, C - A).Z;
}

TArray<FVector> ProjectBox(const FVector& Center, const FQuat& Rotation, const FVector& Extent)
{
	TArray<FVector, TInlineAllocator<8>> Corners;
	for (double X : {-Extent.X, Extent.X})
	{
		for (double Y : {-Extent.Y, Extent.Y})
		{
			for (double Z : {-Extent.Z, Extent.Z})
			{
				const FVector Corner = Center + Rotation.RotateVector(FVector(X, Y, Z));
				Corners.Emplace(Corner.X, Corner.Y, Center.Z);
			}
		}
	}
	Corners.Sort([](const FVector& A, const FVector& B)
	{
		return A.X == B.X ? A.Y < B.Y : A.X < B.X;
	});

	// Convex hull of the eight projected corners retains thickness exposed by pitch and roll.
	TArray<FVector> Hull;
	Hull.Reserve(Corners.Num());
	for (const FVector& Corner : Corners)
	{
		while (Hull.Num() >= 2 && SignedTurn(Hull[Hull.Num() - 2], Hull.Last(), Corner) <= UE_SMALL_NUMBER)
		{
			Hull.Pop(EAllowShrinking::No);
		}
		Hull.Add(Corner);
	}
	const int32 LowerCount = Hull.Num();
	for (int32 Index = Corners.Num() - 2; Index >= 0; --Index)
	{
		const FVector& Corner = Corners[Index];
		while (Hull.Num() > LowerCount && SignedTurn(Hull[Hull.Num() - 2], Hull.Last(), Corner) <= UE_SMALL_NUMBER)
		{
			Hull.Pop(EAllowShrinking::No);
		}
		Hull.Add(Corner);
	}
	Hull.Pop(EAllowShrinking::No); // First and last are the same.
	if (Hull.Num() < 3)
	{
		Hull.Reset();
	}
	return Hull;
}

TArray<FVector> ProjectArc(const FVector& Center, double Radius, const FVector& Forward, float Angle)
{
	const FVector UnitForward = Forward.GetSafeNormal();
	if (UnitForward.IsNearlyZero())
	{
		return {};
	}

	const double HalfAngle = FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(Angle), 1.0, 180.0) * 0.5);
	const double DirectionDot = FMath::Cos(HalfAngle);
	const double Vertical = FMath::Abs(UnitForward.Z);
	const double Horizontal = UnitForward.Size2D();
	const double Yaw = FMath::Atan2(UnitForward.Y, UnitForward.X);
	const bool bSurroundsOrigin = Vertical > DirectionDot;
	// A pitched spherical cone can have a wider projected sector than its center plane.
	const double AzimuthLimit = bSurroundsOrigin ? UE_DOUBLE_PI :
		FMath::Acos(FMath::Clamp(FMath::Sqrt(FMath::Max(0.0, FMath::Square(DirectionDot) - FMath::Square(Vertical)))
			/ FMath::Max(Horizontal, UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0));

	TArray<FVector> Points;
	Points.Reserve(CurveSteps + 2);
	if (!bSurroundsOrigin)
	{
		Points.Add(Center);
	}
	const int32 Samples = bSurroundsOrigin ? CurveSteps : CurveSteps + 1;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Azimuth = FMath::Lerp(-AzimuthLimit, AzimuthLimit, static_cast<double>(Index) / CurveSteps);
		const double Along = Horizontal * FMath::Cos(Azimuth);
		double RadialFraction = 1.0;
		if (Along < DirectionDot)
		{
			// Furthest horizontal projection of a radius-R ray satisfying the cone's forward-dot test.
			const double Denominator = FMath::Square(Along) + FMath::Square(Vertical);
			const double Discriminant = FMath::Max(0.0, Denominator - FMath::Square(DirectionDot));
			RadialFraction = (DirectionDot * Along + Vertical * FMath::Sqrt(Discriminant)) /
				FMath::Max(Denominator, UE_DOUBLE_SMALL_NUMBER);
		}
		const double Distance = Radius * FMath::Clamp(RadialFraction, 0.0, 1.0);
		Points.Add(Center + Distance * FVector(FMath::Cos(Yaw + Azimuth), FMath::Sin(Yaw + Azimuth), 0.0));
	}
	return Points;
}

FVector ResolveSurfaceNormal(const FVector& Preferred, const FVector& FromOrigin)
{
	const FVector Normal = Preferred.GetSafeNormal();
	if (!Normal.IsNearlyZero())
	{
		return Normal;
	}
	return FromOrigin.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::ForwardVector);
}
}

TArray<FVector> MakeRangeFootprint(ERogue10mAttackShape Shape, const FVector& QueryCenter,
	const FQuat& QueryRotation, const FVector& QueryExtent, const FVector& Forward, float ArcAngleDegrees)
{
	if (QueryCenter.ContainsNaN() || QueryExtent.ContainsNaN() || QueryExtent.X <= UE_SMALL_NUMBER)
	{
		return {};
	}
	switch (Shape)
	{
	case ERogue10mAttackShape::LinearBox:
		if (QueryRotation.ContainsNaN() || !QueryRotation.IsNormalized() || QueryExtent.Y < 0.0 || QueryExtent.Z < 0.0)
		{
			return {};
		}
		return ProjectBox(QueryCenter, QueryRotation, QueryExtent);
	case ERogue10mAttackShape::Arc:
		return Forward.ContainsNaN() || !FMath::IsFinite(ArcAngleDegrees) ? TArray<FVector>{} :
			ProjectArc(QueryCenter, QueryExtent.X, Forward, ArcAngleDegrees);
	case ERogue10mAttackShape::Circle:
	{
		TArray<FVector> Points;
		Points.Reserve(CurveSteps);
		for (int32 Index = 0; Index < CurveSteps; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / CurveSteps;
			Points.Add(QueryCenter + QueryExtent.X * FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0));
		}
		return Points;
	}
	default:
		return {};
	}
}

TArray<FVector> MakeRangeOutline(const UWorld& World, const AActor* Attacker, ERogue10mAttackShape Shape,
	const FVector& QueryCenter, const FQuat& QueryRotation, const FVector& QueryExtent,
	const FVector& Forward, float ArcAngleDegrees, double FallbackFeetZ)
{
	if (!FMath::IsFinite(FallbackFeetZ))
	{
		return {};
	}
	TArray<FVector> Points = MakeRangeFootprint(Shape, QueryCenter, QueryRotation, QueryExtent, Forward, ArcAngleDegrees);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Rogue10mMeleeVfxFloor), false, Attacker);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (FVector& Point : Points)
	{
		const FVector From(Point.X, Point.Y, FallbackFeetZ + 55.0);
		const FVector To(Point.X, Point.Y, FallbackFeetZ - 140.0);
		FHitResult Floor;
		const bool bFoundFloor = World.LineTraceSingleByObjectType(Floor, From, To, Objects, Params)
			&& Floor.ImpactNormal.Z >= 0.45;
		Point.Z = (bFoundFloor ? Floor.ImpactPoint.Z : FallbackFeetZ) + GroundLiftCm;
	}
	return Points;
}

bool GetBladeSegment(const UStaticMeshComponent& WeaponMesh, FVector& OutBase, FVector& OutTip)
{
	if (!WeaponMesh.IsRegistered() || !WeaponMesh.IsVisible() || !WeaponMesh.GetStaticMesh())
	{
		return false;
	}
	const FTransform& Transform = WeaponMesh.GetComponentTransform();
	const FVector Base = Transform.TransformPosition(FVector(0, 0, 18));
	const FVector Tip = Transform.TransformPosition(FVector(0, 0, 140));
	if (Base.ContainsNaN() || Tip.ContainsNaN() || Base.Equals(Tip))
	{
		return false;
	}
	OutBase = Base;
	OutTip = Tip;
	return true;
}

FImpactPoint FindCapsuleImpact(const FVector& Center, const FVector& Up, double Radius, double HalfHeight,
	const FVector& AttackStart, const FVector& AttackEnd, const FVector& AttackOrigin)
{
	const FVector Direction = Up.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
	const double AxisHalf = FMath::Max(0.0, HalfHeight - Radius);
	const FVector Bottom = Center - Direction * AxisHalf;
	const FVector Top = Center + Direction * AxisHalf;
	FVector Weapon = AttackStart;
	FVector CapsuleAxis = Center;
	if (!AttackStart.Equals(AttackEnd) && AxisHalf > UE_DOUBLE_SMALL_NUMBER)
	{
		FMath::SegmentDistToSegmentSafe(AttackStart, AttackEnd, Bottom, Top, Weapon, CapsuleAxis);
	}
	else if (AxisHalf > UE_DOUBLE_SMALL_NUMBER)
	{
		CapsuleAxis = FMath::ClosestPointOnSegment(AttackStart, Bottom, Top);
	}
	else if (!AttackStart.Equals(AttackEnd))
	{
		Weapon = FMath::ClosestPointOnSegment(Center, AttackStart, AttackEnd);
	}
	// On the curved side, an axis tie must choose a radial direction to reach the surface.
	FVector FromOrigin = AttackOrigin - CapsuleAxis;
	if (AxisHalf > UE_DOUBLE_SMALL_NUMBER && FVector::DistSquared(CapsuleAxis, Center) < FMath::Square(AxisHalf))
	{
		FromOrigin = FVector::VectorPlaneProject(FromOrigin, Direction);
		if (FromOrigin.IsNearlyZero())
		{
			FVector Secondary;
			Direction.FindBestAxisVectors(FromOrigin, Secondary);
		}
	}
	const FVector Normal = ResolveSurfaceNormal(Weapon - CapsuleAxis, FromOrigin);
	return { CapsuleAxis + Normal * FMath::Max(0.0, Radius), Normal };
}

FImpactPoint FindImpact(const AActor& Target, const FVector& AttackStart, const FVector& AttackEnd,
	const FVector& AttackOrigin)
{
	const FVector Origin = AttackOrigin.ContainsNaN() ? Target.GetActorLocation() : AttackOrigin;
	const FVector Start = AttackStart.ContainsNaN() ? Origin : AttackStart;
	const FVector End = AttackEnd.ContainsNaN() ? Start : AttackEnd;
	if (const ACharacter* Character = Cast<ACharacter>(&Target))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (Capsule && Capsule->GetScaledCapsuleRadius() > UE_DOUBLE_SMALL_NUMBER)
		{
			return FindCapsuleImpact(Capsule->GetComponentLocation(), Capsule->GetUpVector(),
				Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), Start, End, Origin);
		}
	}

	FVector BoundsCenter, BoundsExtent;
	Target.GetActorBounds(true, BoundsCenter, BoundsExtent);
	FVector Weapon = Start.Equals(End) ? Start : FMath::ClosestPointOnSegment(BoundsCenter, Start, End);
	if ((Weapon - BoundsCenter).IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		Weapon = Origin;
	}
	const FVector Normal = ResolveSurfaceNormal((Weapon - BoundsCenter).GetSafeNormal2D(), Origin - BoundsCenter);
	FVector Location = CombatVfx::NearSideBoundsLocation(BoundsCenter, BoundsExtent, BoundsCenter + Normal, 0.0f);
	Location.Z = FMath::Clamp(Weapon.Z, BoundsCenter.Z - BoundsExtent.Z, BoundsCenter.Z + BoundsExtent.Z);
	return {Location, Normal};
}
}
