#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mWeaponVisualComponent.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mStaffUprightMathTest,"Rogue10m.Weapons.StaffUprightMath",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRogue10mStaffUprightMathTest::RunTest(const FString& Parameters)
{
 const FTransform Grip(FRotator(15,25,35),FVector(3,-2,1),FVector(.79));
 for(const float Yaw:{0.f,90.f,179.f})
 for(const float Pitch:{-90.f,-89.999f,0.f,89.999f,90.f})
 for(const float Weight:{0.f,.25f,.5f,.75f,1.f})
 {
  const FTransform Hand(FRotator(Pitch,Yaw,180),FVector(9,8,7));
  const FVector Forward=FRotator(0,Yaw,0).Vector();FTransform Out;
  TestTrue(TEXT("finite normalized grip"),URogue10mWeaponVisualComponent::BuildStaffUprightGrip(Hand,Grip,Forward,Weight,Out)&&Out.IsValid());
  TestTrue(TEXT("authored pivot retained"),Out.GetLocation().Equals(Grip.GetLocation(),.000001));
  TestTrue(TEXT("authored scale retained"),Out.GetScale3D().Equals(Grip.GetScale3D(),.000001));
  if(Weight==0.f){const FVector Head=(Hand.GetRotation()*Out.GetRotation()).RotateVector(FVector::YAxisVector);TestTrue(TEXT("idle jewel up at inverted/arbitrary hand"),FVector::DotProduct(Head,FVector::UpVector)>.99999);}
  if(Weight==1.f){TestTrue(TEXT("full montage exact authored grip"),Out.Equals(Grip,.000001));}
 }
 FTransform Sentinel=Grip,Out=Sentinel;
 TestFalse(TEXT("invalid weight rejected"),URogue10mWeaponVisualComponent::BuildStaffUprightGrip(FTransform::Identity,Grip,FVector::ForwardVector,std::numeric_limits<float>::quiet_NaN(),Out));
 TestTrue(TEXT("failure leaves output unchanged"),Out.Equals(Sentinel));
 TestFalse(TEXT("degenerate facing rejected"),URogue10mWeaponVisualComponent::BuildStaffUprightGrip(FTransform::Identity,Grip,FVector::UpVector,0.f,Out));
 FTransform Invalid=Grip;Invalid.SetScale3D(FVector::ZeroVector);
 TestFalse(TEXT("zero scale rejected"),URogue10mWeaponVisualComponent::BuildStaffUprightGrip(FTransform::Identity,Invalid,FVector::ForwardVector,0.f,Out));
 return !HasAnyErrors();
}
#endif
