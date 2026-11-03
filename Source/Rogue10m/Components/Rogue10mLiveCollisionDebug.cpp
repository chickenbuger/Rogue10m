#include "Rogue10mLiveCollisionDebug.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Chaos/Sphere.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/CollisionFilterData.h"

void Rogue10mLiveCollisionDebug::ReadQueryPrimitives(UPrimitiveComponent& Primitive,
    TArray<FRogue10mLiveQueryShape>& OutShapes,int32& Unsupported,int32& Unresolved)
{
    if (!Primitive.IsQueryCollisionEnabled()) return;
    TArray<FBodyInstance*> Bodies;
    if (const auto* Skeletal=Cast<USkeletalMeshComponent>(&Primitive)) Bodies=Skeletal->Bodies;
    else if (auto* Body=Primitive.GetBodyInstance(NAME_None,false)) Bodies.Add(Body);
    if (Bodies.IsEmpty()) { ++Unresolved; return; }
    TSet<FBodyInstance*> SeenBodies;
    for (FBodyInstance* Body:Bodies)
    {
        if (!Body) { ++Unresolved; continue; }
        if (SeenBodies.Contains(Body)) continue;
        SeenBodies.Add(Body);
        const FPhysicsActorHandle& ActorHandle=Body->GetActorReferenceWithWelding();
        if (!FPhysicsInterface::IsValid(ActorHandle)) { ++Unresolved; continue; }
        // No UObject draw calls while holding the scene lock; only copy shape values.
        const bool bRead=FPhysicsCommand::ExecuteRead(ActorHandle,[&](const FPhysicsActorHandle& Actor)
        {
            TArray<FPhysicsShapeHandle> Shapes;
            // Welded children have no own actor. Enumerate the locked root actor,
            // then retain only shapes bound to this original body instance.
            FPhysicsInterface::GetAllShapes_AssumedLocked(Actor,Shapes);
            for (const FPhysicsShapeHandle& Shape:Shapes)
            {
                if (!Shape.IsValid() || !Body->IsShapeBoundToBody(Shape) || !FPhysicsInterface::IsQueryShape(Shape)) continue;
                const auto Filter=FPhysicsInterface::GetShapeFilterData(Shape);
                // Read per-shape filters: skeletal body settings may differ from component presets.
                if (!Filter.HasFlag(Chaos::EFilterFlags::SimpleCollision)
                    || !(Filter.IsCollisionChannelSet(ECC_Pawn) || Filter.IsCollisionChannelSet(ECC_WorldDynamic)
                        || Filter.IsCollisionChannelSet(ECC_PhysicsBody))) continue;
                const auto Collection=FPhysicsInterface::GetGeometryCollection(Shape);
                const Chaos::FImplicitObject* Geometry=&Collection.GetGeometry();
                FTransform Pose=FPhysicsInterface::GetGlobalPose_AssumesLocked(Actor);
                int32 Depth=0;
                while (Geometry && Geometry->GetType()==Chaos::ImplicitObjectType::Transformed && Depth++<8)
                {
                    const auto* Transformed=Geometry->GetObject<Chaos::TImplicitObjectTransformed<Chaos::FReal,3>>();
                    const auto& Local=Transformed->GetTransform();
                    Pose=FTransform(FQuat(Local.GetRotation()),FVector(Local.GetTranslation()))*Pose;
                    Geometry=Transformed->GetTransformedObject();
                }
                if (!Geometry || Pose.ContainsNaN()) { ++Unresolved; continue; }
                FRogue10mLiveQueryShape View;
                // Exact raw primitives only. Scaled/instanced wrappers and all other
                // implicit types remain explicitly unshown, rather than guessed from bounds.
                if (Geometry->GetType()==Chaos::ImplicitObjectType::Sphere)
                {
                    const auto* Sphere=Geometry->GetObject<Chaos::FSphere>();
                    View.Type=ERogue10mLiveQueryShape::Sphere;
                    View.Center=Pose.TransformPosition(FVector(Sphere->GetCenterf())); View.Radius=Sphere->GetRadiusf();
                }
                else if (Geometry->GetType()==Chaos::ImplicitObjectType::Box)
                {
                    const auto* Box=Geometry->GetObject<Chaos::TBox<Chaos::FReal,3>>();
                    View.Type=ERogue10mLiveQueryShape::Box;
                    View.Center=Pose.TransformPosition(FVector(Box->GetCenter())); View.Extent=FVector(Box->Extents())*0.5;
                    View.Rotation=Pose.GetRotation();
                }
                else if (Geometry->GetType()==Chaos::ImplicitObjectType::Capsule)
                {
                    const auto* Capsule=Geometry->GetObject<Chaos::FCapsule>();
                    View.Type=ERogue10mLiveQueryShape::Capsule;
                    View.Center=Pose.TransformPosition(FVector(Capsule->GetCenterf())); View.Radius=Capsule->GetRadiusf();
                    View.HalfHeight=Capsule->GetHeightf()*0.5f+View.Radius;
                    const FVector Axis=Pose.TransformVectorNoScale(FVector(Capsule->GetAxisf())).GetSafeNormal();
                    View.Rotation=Axis.IsNearlyZero()?FQuat::Identity:FQuat::FindBetweenNormals(FVector::UpVector,Axis);
                }
                else { ++Unsupported; continue; }
                if (View.Center.ContainsNaN() || View.Extent.ContainsNaN() || View.Rotation.ContainsNaN()
                    || !FMath::IsFinite(View.Radius) || !FMath::IsFinite(View.HalfHeight)) { ++Unresolved; continue; }
                OutShapes.Add(View);
            }
        });
        if (!bRead) ++Unresolved;
    }
}
