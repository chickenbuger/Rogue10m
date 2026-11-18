// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mHumanoidAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
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
 virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
 {
  FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
  const auto* Humanoid = CastChecked<URogue10mHumanoidAnimInstance>(Instance);
  Idle.SetSequence(Humanoid->GetIdleSequence());
  Movement.SetSequence(Humanoid->GetMovementSequence() ? Humanoid->GetMovementSequence() : Humanoid->GetIdleSequence());
  const APawn* Pawn = Instance->TryGetPawnOwner();
  const float TargetAlpha = Pawn && Pawn->GetVelocity().SizeSquared2D() > 100.f ? 1.f : 0.f;
  Locomotion.Alpha = FMath::FInterpTo(Locomotion.Alpha, TargetAlpha, DeltaSeconds, 8.f);
 }
private:
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
  if (auto* Component = Owner->FindComponentByClass<URogue10mMonsterAttackPatternComponent>()) Configure(Component->GetProfile());
}
void URogue10mHumanoidAnimInstance::Configure(URogue10mMonsterAttackPatternProfile* Profile)
{
 IdleSequence = Profile ? Profile->IdleAnimation.LoadSynchronous() : nullptr;
 MovementSequence = Profile ? Profile->MovementAnimation.LoadSynchronous() : nullptr;
}
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
 float ContactSeconds, float WindupSeconds, bool bHoldFinalPose)
{
 StopPresentation();
 // Clone authored montages: runtime settings must never mutate shared assets.
 PresentationMontage = Montage ? DuplicateObject<UAnimMontage>(Montage, this)
  : (Sequence ? UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.08f, 0.1f) : nullptr);
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
