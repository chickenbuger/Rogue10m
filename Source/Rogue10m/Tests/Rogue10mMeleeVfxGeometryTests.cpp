#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mMeleeVfxGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMeleeVfxRangeTest,
	"Rogue10m.Combat.MeleeVfx.RangeProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mMeleeVfxRangeTest::RunTest(const FString& Parameters)
{
	using Rogue10m::MeleeVfx::MakeRangeFootprint;
	using EShape = ERogue10mAttackShape;
	const FVector Center(120.0, -40.0, 150.0);
	const FVector BoxExtent(130.0, 42.0, 70.0);
	const FQuat BoxRotation = FRotator(-32.0, 22.0, 5.0).Quaternion();
	const auto Box = MakeRangeFootprint(EShape::LinearBox, Center, BoxRotation, BoxExtent, FVector::ForwardVector, 0.0f);
	if (!TestTrue(TEXT("Projected box is a bounded convex outline"), Box.Num() >= 4 && Box.Num() <= 6))
	{
		return false;
	}
	const FBox BoxBounds = FBox(Box);
	const FVector AlongX = BoxRotation.GetAxisX().GetAbs() * BoxExtent.X;
	const FVector AlongY = BoxRotation.GetAxisY().GetAbs() * BoxExtent.Y;
	const FVector AlongZ = BoxRotation.GetAxisZ().GetAbs() * BoxExtent.Z;
	const FVector Extent = AlongX + AlongY + AlongZ;
	TestTrue(TEXT("Box projection includes the direction, width and height of the actual tilted volume"),
		BoxBounds.GetExtent().Equals(FVector(Extent.X, Extent.Y, 0.0), 1.e-5));
	TestTrue(TEXT("Box projection keeps the query center"), BoxBounds.GetCenter().Equals(Center, 1.e-5));

	const double Radius = 180.0;
	const FVector SphereExtent(Radius);
	const auto Circle = MakeRangeFootprint(EShape::Circle, Center, FQuat::Identity, SphereExtent, FVector::ForwardVector, 0.0f);
	TestTrue(TEXT("Circle remains centered on the query's resolved world center"),
		Circle.Num() >= 16 && Circle[0].Equals(Center + FVector(Radius, 0, 0)) &&
		FBox(Circle).GetCenter().Equals(Center));
	for (const FVector& Point : Circle)
	{
		TestTrue(TEXT("Circle vertices remain on the true horizontal radius"),
			FMath::IsNearlyEqual(FVector::Dist2D(Point, Center), Radius, 1.e-5) && Point.Z == Center.Z);
	}

	const auto LevelArc = MakeRangeFootprint(EShape::Arc, Center, FQuat::Identity, SphereExtent, FVector::RightVector, 90.0f);
	if (TestTrue(TEXT("Level arc has an apex and two limits"), LevelArc.Num() >= 3))
	{
		const FVector FirstEdge = LevelArc[1] - Center;
		const FVector LastEdge = LevelArc.Last() - Center;
		const double Limit = FMath::Cos(UE_DOUBLE_PI / 4.0) * Radius;
		TestTrue(TEXT("Level arc edges respect the 90 degree width around query yaw"),
			LevelArc[0].Equals(Center) && FirstEdge.Equals(FVector(Limit, Limit, 0.0), 1.e-5) &&
			LastEdge.Equals(FVector(-Limit, Limit, 0.0), 1.e-5));
	}
	const FVector PitchedForward = FRotator(-35.0, 70.0, 0.0).Vector();
	const double DotThreshold = FMath::Cos(FMath::DegreesToRadians(40.0));
	const auto PitchedArc = MakeRangeFootprint(EShape::Arc, Center, FQuat::Identity, SphereExtent, PitchedForward, 80.0f);
	TestTrue(TEXT("Pitched arc has an outline"), PitchedArc.Num() >= 3);
	for (const FVector& Point : PitchedArc)
	{
		const FVector ProjectedDirection = (Point - Center) / Radius;
		if (!ProjectedDirection.IsNearlyZero())
		{
			const double Vertical = FMath::Sqrt(FMath::Max(0.0, 1.0 - ProjectedDirection.SizeSquared()));
			const FVector BoundaryDirection = ProjectedDirection + FVector(0.0, 0.0, -Vertical);
			TestTrue(TEXT("Pitched outline remains inside the attack's radius and forward-dot cone"),
				ProjectedDirection.SizeSquared() <= 1.0 + 1.e-5 &&
				FVector::DotProduct(BoundaryDirection, PitchedForward) >= DotThreshold - 1.e-5);
		}
	}
	TestTrue(TEXT("Non-melee queries do not render a footprint"),
		MakeRangeFootprint(EShape::Projectile, Center, FQuat::Identity, BoxExtent, FVector::ForwardVector, 90.0f).IsEmpty());
	TestTrue(TEXT("Degenerate arc direction does not render a footprint"),
		MakeRangeFootprint(EShape::Arc, Center, FQuat::Identity, SphereExtent, FVector::ZeroVector, 90.0f).IsEmpty());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMeleeVfxContactTest,
	"Rogue10m.Combat.MeleeVfx.ImpactPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mMeleeVfxContactTest::RunTest(const FString& Parameters)
{
	using Rogue10m::MeleeVfx::FindCapsuleImpact;
	const FVector Center(200.0, 400.0, 500.0);
	const FVector Hand = Center + FVector(-50.0, 0.0, 33.0);
	const auto Punch = FindCapsuleImpact(Center, FVector::UpVector, 30.0, 90.0, Hand, Hand, Center - FVector(200.0, 0.0, 0.0));
	TestTrue(TEXT("Hand cue preserves the striking hand's height on the near capsule side"),
		Punch.Location.Equals(Center + FVector(-30.0, 0.0, 33.0)) && Punch.Normal.Equals(-FVector::ForwardVector));

	const FVector BladeStart = Center + FVector(-55.0, -70.0, 25.0);
	const FVector BladeEnd = Center + FVector(-55.0, 70.0, 25.0);
	const auto Blade = FindCapsuleImpact(Center, FVector::UpVector, 30.0, 90.0, BladeStart, BladeEnd, BladeStart);
	TestTrue(TEXT("Cue uses nearest point along a blade, including contact height"),
		Blade.Location.Equals(Center + FVector(-30.0, 0.0, 25.0)) && Blade.Normal.Equals(-FVector::ForwardVector));

	const FVector SideHand = Center + FVector(0.0, 25.0, 52.0);
	const auto Rotated = FindCapsuleImpact(Center, FVector::RightVector, 30.0, 90.0, SideHand, SideHand, SideHand);
	TestTrue(TEXT("Cue uses the actual up axis on tilted capsules"),
		Rotated.Location.Equals(Center + FVector(0.0, 25.0, 30.0)) && Rotated.Normal.Equals(FVector::UpVector));

	const FVector Above = Center + FVector(0.0, 0.0, 150.0);
	const auto Cap = FindCapsuleImpact(Center, FVector::UpVector, 30.0, 90.0, Above, Above, Above);
	TestTrue(TEXT("Overhead hits can point at the actual upper cap"),
		Cap.Location.Equals(Center + FVector(0.0, 0.0, 90.0)) && Cap.Normal.Equals(FVector::UpVector));

	const auto Degenerate = FindCapsuleImpact(Center, FVector::UpVector, 30.0, 90.0, Center, Center, Center);
	TestTrue(TEXT("A point exactly on the axis falls back to a valid radial point"),
		!Degenerate.Location.ContainsNaN() && FMath::IsNearlyEqual(FVector::Dist2D(Degenerate.Location, Center), 30.0, 1.e-5) &&
		FMath::IsNearlyEqual(Degenerate.Location.Z, Center.Z, 1.e-5));
	return !HasAnyErrors();
}

#endif
