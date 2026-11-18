#include "Rogue10mMonsterAttackGeometry.h"

bool Rogue10mMonsterAttackGeometry::ContainsCapsule(const FRogue10mMonsterAttackShape& Shape,
    const FVector& CapsuleCenter, float CapsuleRadius, float CapsuleHalfHeight)
{
    if (CapsuleCenter.ContainsNaN() || Shape.Location.ContainsNaN() || Shape.Forward.ContainsNaN()
        || !FMath::IsFinite(CapsuleRadius) || !FMath::IsFinite(CapsuleHalfHeight)
        || CapsuleRadius < 0.0f || CapsuleHalfHeight < 0.0f || !FMath::IsFinite(Shape.Range)
        || !FMath::IsFinite(Shape.HalfWidth) || !FMath::IsFinite(Shape.HalfHeight)
        || !FMath::IsFinite(Shape.ArcDegrees) || Shape.Range < 0.0f || Shape.HalfHeight < 0.0f) { return false; }
    const FVector Offset = CapsuleCenter - Shape.Location;
    if (FMath::Abs(Offset.Z) > Shape.HalfHeight + CapsuleHalfHeight) { return false; }
    if (Shape.AttackType == ERogue10mMonsterAttackType::FrontalWave)
    {
        const float ForwardDistance = FVector::DotProduct(Offset, Shape.Forward);
        const FVector Right(-Shape.Forward.Y, Shape.Forward.X, 0.0f);
        return ForwardDistance >= 0.0f && ForwardDistance <= Shape.Range + CapsuleRadius
            && FMath::Abs(FVector::DotProduct(Offset, Right)) <= Shape.HalfWidth + CapsuleRadius;
    }
    if (Shape.AttackType == ERogue10mMonsterAttackType::AbyssZone)
    { return Offset.SizeSquared2D() <= FMath::Square(Shape.Range + CapsuleRadius); }
    const float Distance = Offset.Size2D();
    const float Padding = FMath::Asin(FMath::Clamp(CapsuleRadius /
        FMath::Max(FMath::Max(Distance, CapsuleRadius), UE_SMALL_NUMBER), 0.0f, 1.0f));
    const float HalfAngle = FMath::DegreesToRadians(FMath::Clamp(Shape.ArcDegrees, 1.0f, 180.0f) * 0.5f) + Padding;
    return Distance <= Shape.Range + CapsuleRadius
        && FVector::DotProduct(Shape.Forward, Offset.GetSafeNormal2D()) >= FMath::Cos(HalfAngle);
}

FRogue10mMonsterWarningProjection Rogue10mMonsterAttackGeometry::MakeWarningProjection(
    const FRogue10mMonsterAttackShape& Shape, float CapsuleRadius)
{
    FRogue10mMonsterWarningProjection Projection;
    const float Radius = FMath::IsFinite(CapsuleRadius) ? FMath::Max(0.0f, CapsuleRadius) : 0.0f;
    const float ExpandedRange = FMath::Max(0.0f, Shape.Range) + Radius;
    Projection.Center = Shape.Location;
    if (Shape.AttackType == ERogue10mMonsterAttackType::FrontalWave)
    {
        Projection.HalfForward = ExpandedRange * 0.5f;
        Projection.HalfRight = FMath::Max(0.0f, Shape.HalfWidth) + Radius;
        Projection.Center += Shape.Forward * Projection.HalfForward;
    }
    else
    {
        Projection.HalfForward = Projection.HalfRight = ExpandedRange;
        Projection.CapsuleRadiusFraction = ExpandedRange > UE_SMALL_NUMBER ? Radius / ExpandedRange : 0.0f;
    }
    return Projection;
}
