// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mHumanoidAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseSnapshot.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BoneContainer.h"
#include "GameFramework/Pawn.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"

namespace
{
class FHumanoidAnimProxy final : public FAnimInstanceProxy
{
public:
 explicit FHumanoidAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
 {
  Idle.SetLoopAnimation(true);
  Movement.SetLoopAnimation(true);
  Locomotion.A.SetLinkNode(&Idle);
  Locomotion.B.SetLinkNode(&Movement);
  Slot.Source.SetLinkNode(&Locomotion);
  Slot.SlotName = TEXT("DefaultSlot");
  Slot.bAlwaysUpdateSourcePose = true;
 }
 virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
 void SetDeathStartPose(FPoseSnapshot&& Pose)
 {
  DeathStartPose = MoveTemp(Pose);
  DeathBlendElapsed = 0.f;
 }
#if WITH_EDITOR
 float GetMovementPlaybackTime() const { return Movement.GetAccumulatedTime(); }
#endif
 virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
 {
  FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
  if (DeathStartPose.bIsValid && (DeathBlendElapsed += FMath::Max(0.f, DeltaSeconds)) >= DeathBlendSeconds)
  {
   DeathStartPose = FPoseSnapshot();
  }
  const auto* Humanoid = CastChecked<URogue10mHumanoidAnimInstance>(Instance);
  Idle.SetSequence(Humanoid->GetIdleSequence());
  Movement.SetSequence(Humanoid->GetMovementSequence() ? Humanoid->GetMovementSequence() : Humanoid->GetIdleSequence());
  const APawn* Pawn = Instance->TryGetPawnOwner();
  const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
  Movement.SetPlayRate(Humanoid->GetMovementPlayRate(Speed));
  const float TargetAlpha = Speed > 10.f ? 1.f : 0.f;
  Locomotion.Alpha = FMath::FInterpTo(Locomotion.Alpha, TargetAlpha, DeltaSeconds, 8.f);
 }

 bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* Root) override
 {
  if (!Root || Root != GetRootNode()) { return false; }
  EvaluateAnimationNode_WithRoot(Output, Root);
  if (!DeathStartPose.bIsValid) { return true; }
  const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
  const FReferenceSkeleton& ReferenceSkeleton = Bones.GetReferenceSkeleton();
  const float Weight = 1.f - FMath::SmoothStep(0.f, 1.f, DeathBlendElapsed / DeathBlendSeconds);
  for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
  {
   const int32 Index = Bones.MakeMeshPoseIndex(Bone).GetInt();
   if (DeathStartPose.LocalTransforms.IsValidIndex(Index) && DeathStartPose.BoneNames.IsValidIndex(Index)
    && ReferenceSkeleton.GetBoneName(Index) == DeathStartPose.BoneNames[Index])
   {
    Output.Pose[Bone].BlendWith(DeathStartPose.LocalTransforms[Index], Weight);
   }
  }
  return true;
 }
private:
 static constexpr float DeathBlendSeconds = .15f;
 FPoseSnapshot DeathStartPose;
 float DeathBlendElapsed = 0.f;
 FAnimNode_SequencePlayer_Standalone Idle;
 FAnimNode_SequencePlayer_Standalone Movement;
 FAnimNode_TwoWayBlend Locomotion;
 FAnimNode_Slot Slot;
};
}

FAnimInstanceProxy* URogue10mHumanoidAnimInstance::CreateAnimInstanceProxy() { return new FHumanoidAnimProxy(this); }
void URogue10mHumanoidAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
void URogue10mHumanoidAnimInstance::NativeInitializeAnimation()
{
 Super::NativeInitializeAnimation();
 SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
 if (AActor* Owner = GetOwningActor())
 {
  if (auto* Component = Owner->FindComponentByClass<URogue10mMonsterAttackPatternComponent>())
  {
   Configure(Component->GetProfile());
   FPoseSnapshot StartPose;
   if (Component->ConsumeDeathStartPose(*this, StartPose))
   {
    GetProxyOnGameThread<FHumanoidAnimProxy>().SetDeathStartPose(MoveTemp(StartPose));
   }
  }
 }
}
void URogue10mHumanoidAnimInstance::Configure(URogue10mMonsterAttackPatternProfile* Profile)
{
 IdleSequence = Profile ? Profile->IdleAnimation.LoadSynchronous() : nullptr;
 MovementSequence = Profile ? Profile->MovementAnimation.LoadSynchronous() : nullptr;
 AuthoredWalkSpeed = Profile ? Profile->AuthoredWalkSpeed : 0.f;
}
float URogue10mHumanoidAnimInstance::GetMovementPlayRate(float Speed) const
{
 // The sequence player applies the asset rate after applying the node rate.
 const float Reference = MovementSequence ? AuthoredWalkSpeed * MovementSequence->RateScale : 0.f;
 return Reference > UE_SMALL_NUMBER && FMath::IsFinite(Reference) && FMath::IsFinite(Speed)
  ? FMath::Clamp(Speed / Reference, 0.f, 3.f) : 1.f;
}
#if WITH_EDITOR
float URogue10mHumanoidAnimInstance::GetMovementPlaybackTime() const
{
 return GetProxyOnGameThread<FHumanoidAnimProxy>().GetMovementPlaybackTime();
}
#endif
float URogue10mHumanoidAnimInstance::ResolveContactTime(float AuthoredContactSeconds, float LengthSeconds)
{
 const float Length = FMath::IsFinite(LengthSeconds) ? FMath::Max(0.002f, LengthSeconds) : 0.002f;
 const float Contact = FMath::IsFinite(AuthoredContactSeconds) ? AuthoredContactSeconds : Length * 0.5f;
 return FMath::Clamp(Contact, 0.001f, Length - 0.001f);
}
float URogue10mHumanoidAnimInstance::CalculatePhaseRate(float ClipSeconds, float GameplaySeconds)
{
 if (!FMath::IsFinite(ClipSeconds) || !FMath::IsFinite(GameplaySeconds) || GameplaySeconds <= 0.f) return 1.f;
 return FMath::Max(0.001f, ClipSeconds) / GameplaySeconds;
}
float URogue10mHumanoidAnimInstance::PlayPresentation(UAnimMontage* Montage, UAnimSequence* Sequence,
 float ContactSeconds, float WindupSeconds, bool bHoldFinalPose, float SequenceBlendInSeconds)
{
 StopPresentation();
 // Clone authored montages: runtime settings must never mutate shared assets.
 PresentationMontage = Montage ? DuplicateObject<UAnimMontage>(Montage, this)
  : (Sequence ? UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), SequenceBlendInSeconds, 0.1f) : nullptr);
 if (!PresentationMontage) return 0.f;
 // Phase mapping owns rate; an authored asset RateScale must not multiply it again.
 PresentationMontage->RateScale = 1.f;
 const float Length = PresentationMontage->GetPlayLength();
 if (!FMath::IsFinite(Length) || Length <= UE_SMALL_NUMBER) { PresentationMontage = nullptr; return 0.f; }
 const bool bTimedAttack = FMath::IsFinite(WindupSeconds) && WindupSeconds > 0.f;
 PresentationContactSeconds = bTimedAttack ? ResolveContactTime(ContactSeconds, Length) : 0.f;
 const float Rate = bTimedAttack ? CalculatePhaseRate(PresentationContactSeconds, WindupSeconds) : 1.f;
 // Hold timed attacks through contact; the authoritative recovery timer blends them out.
 PresentationMontage->bEnableAutoBlendOut = !bTimedAttack && !bHoldFinalPose;
 return Montage_Play(PresentationMontage, Rate, EMontagePlayReturnType::Duration);
}
bool URogue10mHumanoidAnimInstance::PlayDeathFromPose(UAnimSequence& Sequence, FPoseSnapshot&& StartPose)
{
 // Use the visible pose for the blend-in while the original clip advances at its normal rate.
 if (PlayPresentation(nullptr, &Sequence, 0.f, 0.f, true, 0.f) <= 0.f) { return false; }
 if (StartPose.bIsValid) { GetProxyOnGameThread<FHumanoidAnimProxy>().SetDeathStartPose(MoveTemp(StartPose)); }
 return true;
}
void URogue10mHumanoidAnimInstance::BeginRecovery(float RecoverySeconds)
{
 if (!PresentationMontage || !Montage_IsActive(PresentationMontage) || RecoverySeconds <= 0.f) return;
 // Snap to authored contact at the authoritative damage event, including enrage and low frame rates.
 Montage_SetPosition(PresentationMontage, PresentationContactSeconds);
 Montage_SetPlayRate(PresentationMontage,
  CalculatePhaseRate(PresentationMontage->GetPlayLength() - PresentationContactSeconds, RecoverySeconds));
}
void URogue10mHumanoidAnimInstance::StopPresentation()
{
 if (PresentationMontage) Montage_Stop(0.1f, PresentationMontage);
 PresentationMontage = nullptr;
}
