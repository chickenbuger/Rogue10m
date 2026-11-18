#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mMonsterAttackGeometry.h"

namespace Rogue10mMonsterAttackGeometryTests
{
FRogue10mMonsterAttackShape Shape(ERogue10mMonsterAttackType Type)
{
    FRogue10mMonsterAttackShape Result;
    Result.AttackType = Type; Result.Location = FVector::ZeroVector; Result.Forward = FVector::ForwardVector;
    Result.Range = Type == ERogue10mMonsterAttackType::Basic ? 300.0f
        : Type == ERogue10mMonsterAttackType::FrontalWave ? 850.0f : 260.0f;
    Result.HalfWidth = 180.0f; Result.HalfHeight = 160.0f; Result.ArcDegrees = 100.0f;
    return Result;
}
bool Contains(const FRogue10mMonsterAttackShape& Value, const FVector& Center, float Radius = 42.0f, float HalfHeight = 96.0f)
{ return Rogue10mMonsterAttackGeometry::ContainsCapsule(Value, Center, Radius, HalfHeight); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAttackCapsuleBoundsTest,
    "Rogue10m.MonsterAttackGeometry.CapsuleBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAttackCapsuleBoundsTest::RunTest(const FString& Parameters)
{
    using namespace Rogue10mMonsterAttackGeometryTests;
    const auto Basic = Shape(ERogue10mMonsterAttackType::Basic);
    TestTrue(TEXT("Basic includes capsule-expanded far edge"), Contains(Basic, FVector(342, 0, 0)));
    TestFalse(TEXT("Basic excludes beyond expanded far edge"), Contains(Basic, FVector(343, 0, 0)));
    TestFalse(TEXT("Basic excludes rear"), Contains(Basic, FVector(-200, 0, 0)));
    const float InsideAngle = FMath::DegreesToRadians(62.0f), OutsideAngle = FMath::DegreesToRadians(63.0f);
    TestTrue(TEXT("Capsule can intersect a cone outside its raw 50 degree half arc"), Contains(Basic, FVector(200 * FMath::Cos(InsideAngle), 200 * FMath::Sin(InsideAngle), 0)));
    TestFalse(TEXT("Capsule angular padding has a finite boundary"), Contains(Basic, FVector(200 * FMath::Cos(OutsideAngle), 200 * FMath::Sin(OutsideAngle), 0)));
    TestTrue(TEXT("Basic includes center when a capsule overlaps the apex"), Contains(Basic, FVector::ZeroVector));
    TestTrue(TEXT("Height includes exact capsule boundary"), Contains(Basic, FVector(200, 0, 256)));
    TestFalse(TEXT("Height excludes above capsule boundary"), Contains(Basic, FVector(200, 0, 257)));

    const auto Wave = Shape(ERogue10mMonsterAttackType::FrontalWave);
    TestTrue(TEXT("Instant wave narrow phase includes expanded corner and height; raw-box overlap is separate"), Contains(Wave, FVector(892, 222, 256)));
    TestFalse(TEXT("Instant wave excludes a center behind source"), Contains(Wave, FVector(-0.1, 0, 0)));
    TestFalse(TEXT("Instant wave excludes beyond range plus capsule"), Contains(Wave, FVector(893, 0, 0)));
    TestFalse(TEXT("Instant wave excludes beyond width plus capsule"), Contains(Wave, FVector(400, 223, 0)));

    const auto Zone = Shape(ERogue10mMonsterAttackType::AbyssZone);
    TestTrue(TEXT("Zone includes expanded circle edge"), Contains(Zone, FVector(302, 0, 0)));
    TestFalse(TEXT("Zone excludes outside expanded circle"), Contains(Zone, FVector(303, 0, 0)));
    TestFalse(TEXT("Zone's box query is not its final circular damage shape"), Contains(Zone, FVector(250, 250, 0)));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterWarningProjectionTest,
    "Rogue10m.MonsterAttackGeometry.WarningProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterWarningProjectionTest::RunTest(const FString& Parameters)
{
    using namespace Rogue10mMonsterAttackGeometryTests;
    auto Wave = Shape(ERogue10mMonsterAttackType::FrontalWave);
    const auto Projection = Rogue10mMonsterAttackGeometry::MakeWarningProjection(Wave, 42.0f);
    TestTrue(TEXT("Wave warning begins at center-distance zero"), FMath::IsNearlyEqual(Projection.Center.X - Projection.HalfForward, 0.0));
    TestTrue(TEXT("Wave warning ends where expanded native geometry ends"), FMath::IsNearlyEqual(Projection.Center.X + Projection.HalfForward, 892.0));
    TestEqual(TEXT("Wave warning right extent includes target capsule"), Projection.HalfRight, 222.0f);

    Wave.Location = FVector(5000, 7000, 100); Wave.Forward = FVector(0, 1, 0);
    const auto Rotated = Rogue10mMonsterAttackGeometry::MakeWarningProjection(Wave, 42.0f);
    TestTrue(TEXT("Projection respects locked world basis"), Rotated.Center.Equals(FVector(5000, 7446, 100)));
    TestTrue(TEXT("Rotated wave's expanded native corner remains inside"), Contains(Wave, FVector(5222, 7892, 100)));
    TestFalse(TEXT("Rotated wave's outside side is rejected"), Contains(Wave, FVector(5223, 7500, 100)));

    const auto Basic = Rogue10mMonsterAttackGeometry::MakeWarningProjection(Shape(ERogue10mMonsterAttackType::Basic), 42.0f);
    TestEqual(TEXT("Cone projector covers expanded radial envelope"), Basic.HalfForward, 342.0f);
    TestTrue(TEXT("Cone shader radius fraction is in the same world scale"), FMath::IsNearlyEqual(Basic.CapsuleRadiusFraction * Basic.HalfForward, 42.0f, 0.001f));
    const auto Zone = Rogue10mMonsterAttackGeometry::MakeWarningProjection(Shape(ERogue10mMonsterAttackType::AbyssZone), 42.0f);
    TestEqual(TEXT("Circle material gets expanded radial extent"), Zone.HalfRight, 302.0f);
    const auto Large = Rogue10mMonsterAttackGeometry::MakeWarningProjection(Shape(ERogue10mMonsterAttackType::FrontalWave), 84.0f);
    TestEqual(TEXT("Larger capsule changes width"), Large.HalfRight, 264.0f);
    TestEqual(TEXT("Larger capsule changes far distance"), Large.HalfForward * 2.0f, 934.0f);
    return !HasAnyErrors();
}
#endif
