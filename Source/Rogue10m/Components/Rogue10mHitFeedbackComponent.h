// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mHitFeedbackComponent.generated.h"
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Bounded visual-only recoil and optional overlay. Never moves actor collision. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mHitFeedbackComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URogue10mHitFeedbackComponent();
 void PlayHit(float Strength, const FVector& WorldDirection);
 void ResetFeedback();
 int32 GetHitCount() const { return HitCount; }
 float GetFeedbackWeight() const { return FeedbackWeight; }
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Hit Feedback")
 TSoftObjectPtr<UMaterialInterface> FlashMaterial;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Hit Feedback", meta=(ClampMin="0", ClampMax="1"))
 float FeedbackScale = 1.0f;
protected:
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 void UpdateFeedback();
 TWeakObjectPtr<USkeletalMeshComponent> ActiveMesh;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlashInstance;
 FTransform OriginalTransform;
 FVector LocalDirection = FVector::ZeroVector;
 FTimerHandle FeedbackTimer;
 double StartedAt = 0.0;
 float ActiveStrength = 0.0f;
 float FeedbackWeight = 0.0f;
 int32 HitCount = 0;
};
