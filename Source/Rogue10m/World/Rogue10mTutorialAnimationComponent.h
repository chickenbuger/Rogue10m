// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mTutorialAnimationComponent.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
class URogue10mTutorialDataAsset;

UENUM(BlueprintType)
enum class ERogue10mTutorialAnimationState : uint8
{
	Idle,
	Walk,
	Talk
};

/** 실제 이동과 대화 이벤트에 맞춰 안내인의 반복 모션을 재생합니다. 이동 자체는 만들지 않습니다. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mTutorialAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mTutorialAnimationComponent();
	void Configure(USkeletalMeshComponent* InMesh, URogue10mTutorialDataAsset* InData);
	void SetDialogueOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	ERogue10mTutorialAnimationState GetAnimationState() const { return AnimationState; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	UAnimSequence* GetCurrentAnimation() const { return CurrentAnimation; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	float GetMeasuredSpeed() const { return MeasuredSpeed; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial|Animation")
	bool IsDialogueAnimationRequested() const { return bDialogueOpen; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResetMovementSample();
	void SampleMovement();
	void RefreshAnimation();
	UAnimSequence* GetCompatibleAnimation(UAnimSequence* Candidate) const;

	TWeakObjectPtr<USkeletalMeshComponent> GuideMesh;
	TWeakObjectPtr<URogue10mTutorialDataAsset> TutorialData;
	TWeakObjectPtr<UAnimSequence> FallbackIdleAnimation;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnimation;
	FTimerHandle MovementTimer;
	FVector LastLocation = FVector::ZeroVector;
	double LastSampleTime = 0.0;
	float MeasuredSpeed = 0.0f;
	ERogue10mTutorialAnimationState AnimationState = ERogue10mTutorialAnimationState::Idle;
	bool bHasMovementSample = false;
	bool bMoving = false;
	bool bDialogueOpen = false;
	bool bStopping = false;
};
