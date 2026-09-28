// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mHitFeedbackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rogue10mBasicMonster.h"
#include "TimerManager.h"

URogue10mHitFeedbackComponent::URogue10mHitFeedbackComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
 FlashMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Rogue10m/VFX/Character/Combat/Unarmed/M_BrawlerHitFlash.M_BrawlerHitFlash")));
}
void URogue10mHitFeedbackComponent::PlayHit(float Strength, const FVector& WorldDirection)
{
 ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(GetOwner());
 if (!Monster || Monster->IsDead() || !GetWorld() || !FMath::IsFinite(Strength)
  || !FMath::IsFinite(FeedbackScale) || Strength <= 0.0f || FeedbackScale <= 0.0f || WorldDirection.ContainsNaN())
 { ResetFeedback(); return; }
 USkeletalMeshComponent* Mesh = Monster->GetMesh();
 if (!Mesh || Mesh->IsSimulatingPhysics()) { ResetFeedback(); return; }
 const float NewStrength = FMath::Clamp(Strength * FeedbackScale, 0.0f, 1.0f);
 // A repeated hit refreshes one bounded envelope, never accumulates transforms.
 if (!ActiveMesh.IsValid())
 {
  ActiveMesh = Mesh;
  OriginalTransform = Mesh->GetRelativeTransform();
  TArray<TObjectPtr<UMaterialInterface>> SlotOverlays;
  Mesh->GetMaterialSlotsOverlayMaterial(SlotOverlays);
  const bool bHasSlotOverlay = SlotOverlays.ContainsByPredicate([](const TObjectPtr<UMaterialInterface>& Material) { return Material != nullptr; });
  if (!Mesh->GetOverlayMaterial() && !bHasSlotOverlay)
  {
   if (UMaterialInterface* Material = FlashMaterial.LoadSynchronous())
   {
    FlashInstance = UMaterialInstanceDynamic::Create(Material, this);
    Mesh->SetOverlayMaterial(FlashInstance);
   }
  }
 }
 ActiveStrength = FMath::Max(NewStrength, FeedbackWeight);
 LocalDirection = Monster->GetActorTransform().InverseTransformVectorNoScale(WorldDirection).GetSafeNormal();
 StartedAt = GetWorld()->GetTimeSeconds();
 ++HitCount;
 UpdateFeedback();
 GetWorld()->GetTimerManager().SetTimer(FeedbackTimer, this, &URogue10mHitFeedbackComponent::UpdateFeedback, 1.0f / 60.0f, true);
}
void URogue10mHitFeedbackComponent::UpdateFeedback()
{
 ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(GetOwner());
 USkeletalMeshComponent* Mesh = ActiveMesh.Get();
 if (!GetWorld() || !Monster || Monster->IsDead() || !Mesh || Mesh->IsSimulatingPhysics()
  || !FMath::IsFinite(FeedbackScale) || FeedbackScale <= 0.0f)
 { ResetFeedback(); return; }
 const float Alpha = FMath::Clamp(float((GetWorld()->GetTimeSeconds() - StartedAt) / 0.18), 0.0f, 1.0f);
 if (Alpha >= 1.0f) { ResetFeedback(); return; }
 FeedbackWeight = ActiveStrength * FMath::Square(1.0f - Alpha);
 FTransform Pose = OriginalTransform;
 Pose.AddToTranslation(LocalDirection * (2.0f * FeedbackWeight));
 Pose.SetRotation((FRotator(-3.0f * LocalDirection.X * FeedbackWeight, 0.0f, 3.0f * LocalDirection.Y * FeedbackWeight).Quaternion() * OriginalTransform.GetRotation()).GetNormalized());
 Mesh->SetRelativeTransform(Pose);
 if (FlashInstance && Mesh->GetOverlayMaterial() == FlashInstance)
 { FlashInstance->SetScalarParameterValue(TEXT("HitOpacity"), 0.35f * FeedbackWeight); }
}
void URogue10mHitFeedbackComponent::ResetFeedback()
{
 if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer); }
 if (USkeletalMeshComponent* Mesh = ActiveMesh.Get())
 {
  if (!Mesh->IsSimulatingPhysics()) { Mesh->SetRelativeTransform(OriginalTransform); }
  // Never remove an overlay another system installed while this pulse was active.
  if (FlashInstance && Mesh->GetOverlayMaterial() == FlashInstance) { Mesh->SetOverlayMaterial(nullptr); }
 }
 ActiveMesh.Reset(); FlashInstance = nullptr; FeedbackWeight = ActiveStrength = 0.0f;
}
void URogue10mHitFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 ResetFeedback(); Super::EndPlay(EndPlayReason);
}
