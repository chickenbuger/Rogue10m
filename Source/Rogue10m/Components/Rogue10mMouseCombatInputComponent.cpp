#include "Rogue10mMouseCombatInputComponent.h"
#include "Engine/World.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"

URogue10mMouseCombatInputComponent::URogue10mMouseCombatInputComponent() { PrimaryComponentTick.bCanEverTick = false; }
ARogue10mCharacter* URogue10mMouseCombatInputComponent::Character() const { return Cast<ARogue10mCharacter>(GetOwner()); }
URogue10mCombatComponent* URogue10mMouseCombatInputComponent::Combat() const { const auto* C=Character(); return C?C->GetCombatComponent():nullptr; }
bool URogue10mMouseCombatInputComponent::CanUseInput() const
{
 const auto* C=Character();const auto* PC=C?Cast<ARogue10mPlayerController>(C->GetController()):nullptr;
 return C && !C->IsDead() && PC && PC->GetPawn()==C && !PC->IsAnyBlockingWindowVisible() && !PC->IsPaused() && Combat() && GetWorld();
}
void URogue10mMouseCombatInputComponent::StartMonitor()
{
 if(GetWorld()&&!GetWorld()->GetTimerManager().IsTimerActive(MonitorTimer))
  GetWorld()->GetTimerManager().SetTimer(MonitorTimer,this,&URogue10mMouseCombatInputComponent::MonitorInput,.02f,true);
}
void URogue10mMouseCombatInputComponent::Press(bool Primary)
{
 if(!CanUseInput()){CancelInput();return;}
 bool& Held=Primary?bLeftHeld:bRightHeld;if(Held)return;
 const bool OtherHeld=Primary?bRightHeld:bLeftHeld;const double Now=GetWorld()->GetTimeSeconds();Held=true;
 if(Primary)LeftPressedAt=Now;else RightPressedAt=Now;
 auto* C=Combat();
 if(!OtherHeld)
 {
  PrimarySnapshot=const_cast<URogue10mAttackSkillData*>(C->GetEquippedSkill(ERogue10mAttackInputSlot::Primary));
  ChordSnapshot=const_cast<URogue10mAttackSkillData*>(C->GetEquippedSkill(ERogue10mAttackInputSlot::MouseChord));
 }
 StartMonitor();
 if(bChordConsumed||bSuppressed)return;
 if(OtherHeld)
 {
  const double Gap=FMath::Abs(LeftPressedAt-RightPressedAt);
  if(!bPrimaryDispatched && ChordSnapshot.IsValid() && C->GetEquippedSkill(ERogue10mAttackInputSlot::MouseChord)==ChordSnapshot.Get()
   && Gap<=FMath::Clamp(ChordWindowSeconds,0.f,.2f))
  {
   GetWorld()->GetTimerManager().ClearTimer(PrimaryDelayTimer);C->CancelBoundSkillInput();bRightRoute=false;ChargeSnapshot.Reset();bChordConsumed=true;
   C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::MouseChord);C->HandleEquippedSkillReleased(ERogue10mAttackInputSlot::MouseChord);
  }
  else
  {
   // A late second button cannot reinterpret an already committed basic attack.
   bSuppressed=true;GetWorld()->GetTimerManager().ClearTimer(PrimaryDelayTimer);C->CancelHeldAttackInput();
   // CancelHeldAttackInput clears this component through the reset-only Character hook.
   bLeftHeld=bRightHeld=true;bSuppressed=true;StartMonitor();
  }
  return;
 }
 if(Primary)
 {
  if(ChordSnapshot.IsValid()&&ChordWindowSeconds>0.f)GetWorld()->GetTimerManager().SetTimer(PrimaryDelayTimer,this,&URogue10mMouseCombatInputComponent::DispatchPrimary,FMath::Clamp(ChordWindowSeconds,.001f,.2f),false);
  else DispatchPrimary();
 }
 else
 {
  bRightRoute=true;ChargeSnapshot=const_cast<URogue10mAttackSkillData*>(C->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial));
  if(ChargeSnapshot.IsValid())C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::ChargedSpecial);
 }
}
void URogue10mMouseCombatInputComponent::DispatchPrimary()
{
 if(!CanUseInput()||bChordConsumed||bSuppressed||bPrimaryDispatched)return;
 auto* C=Combat();if(C->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)!=PrimarySnapshot.Get()){CancelInput();return;}
 bPrimaryDispatched=true;C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::Primary);
}
void URogue10mMouseCombatInputComponent::Release(bool Primary)
{
 bool& Held=Primary?bLeftHeld:bRightHeld;if(!Held)return;Held=false;
 if(!CanUseInput()){CancelInput();return;}
 auto* C=Combat();
 if(bChordConsumed||bSuppressed){FinishReleasedCycle();return;}
 if(Primary)
 {
  GetWorld()->GetTimerManager().ClearTimer(PrimaryDelayTimer);
  if(!bPrimaryDispatched)DispatchPrimary();
  if(bPrimaryDispatched)C->HandleEquippedSkillReleased(ERogue10mAttackInputSlot::Primary);
 }
 else if(bRightRoute)
 {
  const auto* Charged=ChargeSnapshot.Get();
  if(C->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial)!=Charged){CancelInput();return;}
  const bool Ready=Charged&&RightPressedAt>=0.&&GetWorld()->GetTimeSeconds()-RightPressedAt>=FMath::Max(.05f,Charged->ChargeSeconds);
  bRightRoute=false;
  if(Ready)C->HandleEquippedSkillReleased(ERogue10mAttackInputSlot::ChargedSpecial);
  else {C->CancelBoundSkillInput();C->HandleEquippedSkillPressed(ERogue10mAttackInputSlot::Special);C->HandleEquippedSkillReleased(ERogue10mAttackInputSlot::Special);}
 }
 FinishReleasedCycle();
}
void URogue10mMouseCombatInputComponent::FinishReleasedCycle(){if(!bLeftHeld&&!bRightHeld)CancelInput(false);}
void URogue10mMouseCombatInputComponent::MonitorInput()
{
 if(!CanUseInput()){CancelInput();return;}
 if(bRightRoute&&Combat()->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial)!=ChargeSnapshot.Get())CancelInput();
}
void URogue10mMouseCombatInputComponent::CancelInput(bool CancelCombat)
{
 if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(PrimaryDelayTimer);GetWorld()->GetTimerManager().ClearTimer(MonitorTimer);}
 bLeftHeld=bRightHeld=bPrimaryDispatched=bRightRoute=bChordConsumed=bSuppressed=false;LeftPressedAt=RightPressedAt=-1.;PrimarySnapshot.Reset();ChordSnapshot.Reset();ChargeSnapshot.Reset();
 if(CancelCombat)if(auto* C=Combat())C->CancelHeldAttackInput();
}
void URogue10mMouseCombatInputComponent::EndPlay(const EEndPlayReason::Type Reason){CancelInput();Super::EndPlay(Reason);}