// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mCharacterAnimationComponent.generated.h"

class ARogue10mCharacter;
class URogue10mCharacterMotionDataAsset;

/** 이동 이벤트를 공통 모션과 Niagara Cue로 변환합니다. 상시 Tick을 사용하지 않습니다. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mCharacterAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mCharacterAnimationComponent();

	void SetMovementState(bool bHasMovementInput, bool bSprinting);
	void NotifyJump(int32 JumpCount);
	void NotifyLanded();
	void NotifyDodge();
	void StopMotionEffects();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Animation")
	const URogue10mCharacterMotionDataAsset* GetMotionData() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ARogue10mCharacter* GetOwnerCharacter() const;
	void RefreshFootstepTimer();
	void SpawnFootstepEffect();
	void SpawnGroundEffect(
		class UNiagaraSystem* Effect, float UniformScale, float EmissionDuration,
		const FVector& LocalOffset = FVector::ZeroVector) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<URogue10mCharacterMotionDataAsset> MotionData;

	FTimerHandle FootstepTimerHandle;
	bool bMovementInputActive = false;
	bool bSprintActive = false;
	bool bNextFootstepRight = false;
};
