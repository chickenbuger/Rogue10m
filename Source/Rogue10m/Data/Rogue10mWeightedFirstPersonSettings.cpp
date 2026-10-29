#include "Rogue10mWeightedFirstPersonSettings.h"
#include "HAL/IConsoleManager.h"

namespace
{
 TAutoConsoleVariable<int32> CVarWeightedFirstPersonEnabled(TEXT("Rogue10m.FPHeavy"),0,TEXT("Opt-in weighted first-person prototype. 0 restores the original presentation; 1 enables. No save changes."),ECVF_Default);
 TAutoConsoleVariable<float> CVarWeightedFirstPersonMotion(TEXT("Rogue10m.FPHeavyMotionScale"),1.0f,TEXT("Weighted first-person animation motion intensity, 0..1. Mouse input remains immediate."),ECVF_Default);
 TAutoConsoleVariable<float> CVarWeightedFirstPersonFov(TEXT("Rogue10m.FPHeavyFOV"),0.0f,TEXT("Weighted first-person world FOV override, 70..110; 0 uses profile."),ECVF_Default);
 float SanitizeWeightedValue(float V,float Default,float Low,float High) { return FMath::IsFinite(V)?FMath::Clamp(V,Low,High):Default; }
}
FRogue10mWeightedFirstPersonSettings FRogue10mWeightedFirstPersonSettings::Sanitized() const
{
 FRogue10mWeightedFirstPersonSettings S=*this;
 const FRogue10mWeightedFirstPersonSettings D;
 S.WorldFieldOfView=SanitizeWeightedValue(S.WorldFieldOfView,D.WorldFieldOfView,70.f,110.f);
 S.InitialViewPitch=SanitizeWeightedValue(S.InitialViewPitch,D.InitialViewPitch,-60.f,0.f);
 S.HeadMotionScale=SanitizeWeightedValue(S.HeadMotionScale,D.HeadMotionScale,0.f,1.f);
 S.HeadRotationWeight=SanitizeWeightedValue(S.HeadRotationWeight,D.HeadRotationWeight,0.f,1.f);
 S.WalkingRotationDegrees=SanitizeWeightedValue(S.WalkingRotationDegrees,D.WalkingRotationDegrees,0.f,2.f);
 S.RunningRotationDegrees=SanitizeWeightedValue(S.RunningRotationDegrees,D.RunningRotationDegrees,0.f,4.f);
 S.AttackingRotationDegrees=SanitizeWeightedValue(S.AttackingRotationDegrees,D.AttackingRotationDegrees,0.f,3.f);
 S.WalkingPositionCm=SanitizeWeightedValue(S.WalkingPositionCm,D.WalkingPositionCm,0.f,5.f);
 S.RunningPositionCm=SanitizeWeightedValue(S.RunningPositionCm,D.RunningPositionCm,0.f,5.f);
 S.AttackingPositionCm=SanitizeWeightedValue(S.AttackingPositionCm,D.AttackingPositionCm,0.f,5.f);
 S.HeadFollowSpeed=SanitizeWeightedValue(S.HeadFollowSpeed,D.HeadFollowSpeed,0.f,60.f);
 S.HandMotionScale=SanitizeWeightedValue(S.HandMotionScale,D.HandMotionScale,0.f,1.f);
 S.IdleArmDropDegrees=SanitizeWeightedValue(S.IdleArmDropDegrees,D.IdleArmDropDegrees,0.f,6.f);
 S.MovementHandSwayDegrees=SanitizeWeightedValue(S.MovementHandSwayDegrees,D.MovementHandSwayDegrees,0.f,3.f);
 S.AttackWindupDegrees=SanitizeWeightedValue(S.AttackWindupDegrees,D.AttackWindupDegrees,0.f,6.f);
 S.AttackRecoveryDegrees=SanitizeWeightedValue(S.AttackRecoveryDegrees,D.AttackRecoveryDegrees,0.f,6.f);
 S.GuardRaiseSeconds=SanitizeWeightedValue(S.GuardRaiseSeconds,D.GuardRaiseSeconds,0.05f,1.f);
 S.GuardLowerSeconds=SanitizeWeightedValue(S.GuardLowerSeconds,D.GuardLowerSeconds,0.05f,2.f);
 S.UpperBodyFollowSpeed=SanitizeWeightedValue(S.UpperBodyFollowSpeed,D.UpperBodyFollowSpeed,0.f,60.f);
 return S;
}
bool Rogue10mWeightedFirstPerson::IsRequested() { return CVarWeightedFirstPersonEnabled.GetValueOnGameThread()!=0; }
float Rogue10mWeightedFirstPerson::ConsoleMotionScale() { return SanitizeWeightedValue(CVarWeightedFirstPersonMotion.GetValueOnGameThread(),1.f,0.f,1.f); }
float Rogue10mWeightedFirstPerson::ResolveFieldOfView(float ProfileValue)
{
 const float Value=CVarWeightedFirstPersonFov.GetValueOnGameThread();
 return FMath::IsFinite(Value) && Value!=0.f?SanitizeWeightedValue(Value,86.f,70.f,110.f):SanitizeWeightedValue(ProfileValue,86.f,70.f,110.f);
}
