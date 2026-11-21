// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Rogue10mHumanoidAnimInstance.generated.h"
class UAnimSequence;
class UAnimMontage;
class URogue10mMonsterAttackPatternProfile;
struct FPoseSnapshot;

/** Native locomotion graph with a full-body DefaultSlot. Gameplay remains timer-authoritative. */
UCLASS(Transient, BlueprintType)
class ROGUE10M_API URogue10mHumanoidAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeInitializeAnimation() override;
 /** Pure timing helpers shared with automation tests. */
 static float ResolveContactTime(float AuthoredContactSeconds, float LengthSeconds);
 static float CalculatePhaseRate(float ClipSeconds, float GameplaySeconds);
 void Configure(URogue10mMonsterAttackPatternProfile* Profile);
 float PlayPresentation(UAnimMontage* Montage, UAnimSequence* Sequence, float ContactSeconds = 0.f,
   float WindupSeconds = 0.f, bool bHoldFinalPose = false, float SequenceBlendInSeconds = 0.08f);
 bool PlayDeathFromPose(UAnimSequence& Sequence, FPoseSnapshot&& StartPose);
 void BeginRecovery(float RecoverySeconds);
 void StopPresentation();
 UAnimSequence* GetIdleSequence() const { return IdleSequence; }
 UAnimSequence* GetMovementSequence() const { return MovementSequence; }
 float GetMovementPlayRate(float Speed) const;
#if WITH_EDITOR
 float GetMovementPlaybackTime() const;
#endif
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
private:
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleSequence;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> MovementSequence;
 UPROPERTY(Transient) TObjectPtr<UAnimMontage> PresentationMontage;
 float AuthoredWalkSpeed = 0.f;
 float PresentationContactSeconds = 0.f;
};
