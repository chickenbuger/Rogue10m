#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mAdvanceTimeline.h"
#include "Rogue10mWeaponTypes.h"
#include "TimerManager.h"
#include "Rogue10mTechniqueMotionComponent.generated.h"

class ARogue10mCharacter;
class UAnimInstance;
class UAnimMontage;
DECLARE_MULTICAST_DELEGATE_TwoParams(FRogue10mTechniqueAdvanceFinished, uint32, bool);

/** Optional grounded capsule advance; does not pay costs, deal damage, dodge or alter camera modes. */
UCLASS(ClassGroup=(Rogue10m))
class ROGUE10M_API URogue10mTechniqueMotionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mTechniqueMotionComponent();
	static bool CanStart(const ARogue10mCharacter& Character, double Distance, double Duration, double ContactDelay);
	bool StartAdvance(uint32 ExecutionId, const FVector& Direction, double Distance, double Duration, double ContactDelay,
		UAnimInstance* AnimInstance, UAnimMontage* Montage, int32 MontageInstanceId);
	void CancelAdvance();
	bool IsAdvancing() const { return ActiveExecutionId != 0; }
	uint32 GetExecutionId() const { return ActiveExecutionId; }
	FRogue10mTechniqueAdvanceFinished OnAdvanceFinished;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	friend struct FRogue10mTechniqueInterruptionTestAccess;
	friend class URogue10mCombatComponent;
	static bool DoesMontageContinue(const ARogue10mCharacter& Owner, UAnimInstance* AnimInstance,
		const UAnimMontage* Montage, int32 MontageInstanceId);
	void AdvanceStep();
	void FinishAdvance(bool bCommitContact);
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<UAnimInstance> AcceptedAnimInstance;
	TWeakObjectPtr<UAnimMontage> AcceptedMontage;
	int32 AcceptedMontageInstanceId = INDEX_NONE;
	FRogue10mAdvanceTimeline Timeline;
	FTimerHandle StepTimer;
	FVector LockedDirection = FVector::ForwardVector;
	FVector LastOwnedLocation = FVector::ZeroVector;
	FVector AcceptedOrigin = FVector::ZeroVector;
	ERogue10mWeaponType AcceptedWeapon = ERogue10mWeaponType::Unarmed;
	uint32 ActiveExecutionId = 0;
	uint64 Generation = 0;
	uint8 PreviousMovementMode = 0, PreviousCustomMode = 0;
};
