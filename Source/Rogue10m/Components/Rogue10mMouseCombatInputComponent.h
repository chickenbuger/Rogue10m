#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mMouseCombatInputComponent.generated.h"
class ARogue10mCharacter;
class URogue10mAttackSkillData;
class URogue10mCombatComponent;

/** Physical LMB/RMB arbitration only. Gameplay authorization, costs and timing remain in Combat. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mMouseCombatInputComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URogue10mMouseCombatInputComponent();
 void Press(bool bPrimary);
 void Release(bool bPrimary);
 /** false is reset-only, used by Combat cancellation to avoid recursive callbacks. */
 void CancelInput(bool bCancelCombat = true);
 bool IsInputHeld() const { return bLeftHeld || bRightHeld; }
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Input", meta=(ClampMin="0.0",ClampMax="0.2",Units="s"))
 float ChordWindowSeconds = 0.08f;
protected:
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 bool CanUseInput() const;
 ARogue10mCharacter* Character() const;
 URogue10mCombatComponent* Combat() const;
 void DispatchPrimary();
 void MonitorInput();
 void StartMonitor();
 void FinishReleasedCycle();
 FTimerHandle PrimaryDelayTimer;
 FTimerHandle MonitorTimer;
 TWeakObjectPtr<URogue10mAttackSkillData> PrimarySnapshot;
 TWeakObjectPtr<URogue10mAttackSkillData> ChordSnapshot;
 TWeakObjectPtr<URogue10mAttackSkillData> ChargeSnapshot;
 double LeftPressedAt = -1.0;
 double RightPressedAt = -1.0;
 bool bLeftHeld = false;
 bool bRightHeld = false;
 bool bPrimaryDispatched = false;
 bool bRightRoute = false;
 bool bChordConsumed = false;
 bool bSuppressed = false;
};