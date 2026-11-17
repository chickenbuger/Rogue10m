#include "Rogue10mTechniqueMotionComponent.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10m.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"

namespace
{
bool HasSupportedFloor(const UCharacterMovementComponent& Movement, const FVector& Location)
{
	if (Location.ContainsNaN()) { return false; }
	FFindFloorResult Floor;
	Movement.FindFloor(Location, Floor, false);
	const float Distance = Floor.bLineTrace ? Floor.LineDist : Floor.FloorDist;
	// This prototype stays on its current ground plane; it does not step or float over drops.
	return Floor.IsWalkableFloor() && !Floor.HitResult.bStartPenetrating
		&& FMath::IsFinite(Distance) && Distance >= -0.5f && Distance <= 4.0f;
}
}

URogue10mTechniqueMotionComponent::URogue10mTechniqueMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URogue10mTechniqueMotionComponent::CanStart(const ARogue10mCharacter& Owner, double Distance, double Duration, double ContactDelay)
{
	const auto* Movement = Owner.GetCharacterMovement();
	const auto* Existing = Owner.FindComponentByClass<URogue10mTechniqueMotionComponent>();
	return Owner.HasAuthority() && !Owner.IsDead() && !Owner.IsDodging() && Owner.IsThirdPersonGameplayActive() && Owner.GetController() && Owner.GetWorld() && Movement
		&& Movement->MovementMode == MOVE_Walking && Movement->UpdatedComponent == Owner.GetCapsuleComponent()
		&& HasSupportedFloor(*Movement, Owner.GetActorLocation())
		&& (!Existing || !Existing->IsAdvancing())
		&& FRogue10mAdvanceTimeline::IsValid(Distance, Duration, ContactDelay, Owner.GetWorld()->GetTimeSeconds());
}

bool URogue10mTechniqueMotionComponent::DoesMontageContinue(const ARogue10mCharacter& Owner,
	UAnimInstance* AnimInstance, const UAnimMontage* Montage, int32 MontageInstanceId)
{
	const auto* Mesh = Owner.GetAnimationPlaybackMesh();
	if (!IsValid(AnimInstance) || !IsValid(Montage) || MontageInstanceId == INDEX_NONE
		|| !Mesh || Mesh->GetAnimInstance() != AnimInstance) { return false; }
	const auto* Instance = AnimInstance->GetMontageInstanceForID(MontageInstanceId);
	// Stopped/blending-out, paused, removed or replaced playback never commits pending movement/contact.
	return Instance && Instance->Montage == Montage && Instance->IsActive() && Instance->IsPlaying();
}

bool URogue10mTechniqueMotionComponent::StartAdvance(uint32 ExecutionId, const FVector& Direction,
	double Distance, double Duration, double ContactDelay, UAnimInstance* AnimInstance, UAnimMontage* Montage, int32 MontageInstanceId)
{
	auto* Owner = Cast<ARogue10mCharacter>(GetOwner());
	const FVector Planar = FVector(Direction.X, Direction.Y, 0.0).GetSafeNormal();
	if (!ExecutionId || !Owner || Planar.IsNearlyZero() || Planar.ContainsNaN()
		|| !CanStart(*Owner, Distance, Duration, ContactDelay)
		|| !DoesMontageContinue(*Owner, AnimInstance, Montage, MontageInstanceId)
		|| !Timeline.Begin(Distance, Duration, ContactDelay, GetWorld()->GetTimeSeconds())) { return false; }
	Character = Owner; LockedDirection = Planar; ActiveExecutionId = ExecutionId;
	AcceptedAnimInstance = AnimInstance; AcceptedMontage = Montage; AcceptedMontageInstanceId = MontageInstanceId;
	const uint64 StartedGeneration = ++Generation;
	LastOwnedLocation = Owner->GetActorLocation();
	AcceptedOrigin = LastOwnedLocation;
	AcceptedWeapon = Owner->GetEquippedWeaponType();
	auto* Movement = Owner->GetCharacterMovement();
	PreviousMovementMode = static_cast<uint8>(Movement->MovementMode); PreviousCustomMode = Movement->CustomMovementMode;
	Movement->StopMovementImmediately();
	if (Generation != StartedGeneration || ActiveExecutionId != ExecutionId) { return false; }
	Movement->SetMovementMode(MOVE_None);
	if (Generation != StartedGeneration || ActiveExecutionId != ExecutionId) { return false; }
	Owner->ConsumeMovementInputVector();
	Owner->SetActorRotation(LockedDirection.Rotation());
	if (Generation != StartedGeneration || ActiveExecutionId != ExecutionId) { return false; }
	if (Owner->IsDead() || Owner->GetActorLocation().ContainsNaN()
		|| !DoesMontageContinue(*Owner, AcceptedAnimInstance.Get(), AcceptedMontage.Get(), AcceptedMontageInstanceId)
		|| FVector::DistSquared(Owner->GetActorLocation(), LastOwnedLocation) > 1.0)
	{
		FinishAdvance(false); return false;
	}
	GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &ThisClass::AdvanceStep, 0.01f, true);
	UE_LOG(LogRogue10m, Display, TEXT("TECHNIQUE_ADVANCE START execution=%u distance_cm=%.3f duration_s=%.3f contact_delay_s=%.3f"),
		ExecutionId, Distance, Duration, ContactDelay);
	return true;
}

void URogue10mTechniqueMotionComponent::AdvanceStep()
{
	const uint64 StepGeneration = Generation;
	const uint32 StepExecution = ActiveExecutionId;
	const auto StillOwned = [this, StepGeneration, StepExecution]()
	{ return StepExecution != 0 && Generation == StepGeneration && ActiveExecutionId == StepExecution; };
	auto* Owner = Character.Get();
	auto* Movement = Owner ? Owner->GetCharacterMovement() : nullptr;
	const auto* Controller = Owner ? Cast<ARogue10mPlayerController>(Owner->GetController()) : nullptr;
	const auto* Inventory = Owner ? Owner->GetInventoryComponent() : nullptr;
	if (!Owner || !Owner->HasAuthority() || Owner->IsDead() || !Owner->IsThirdPersonGameplayActive() || !Owner->GetController() || !Movement
		|| Movement->MovementMode != MOVE_None || Owner->GetEquippedWeaponType() != AcceptedWeapon
		|| Movement->UpdatedComponent != Owner->GetCapsuleComponent() || Owner->IsDodging()
		|| !DoesMontageContinue(*Owner, AcceptedAnimInstance.Get(), AcceptedMontage.Get(), AcceptedMontageInstanceId)
		|| !HasSupportedFloor(*Movement, Owner->GetActorLocation())
		|| Owner->GetActorLocation().ContainsNaN() || FVector::DistSquared(Owner->GetActorLocation(), LastOwnedLocation) > 1.0
		|| (Controller && Controller->IsAnyBlockingWindowVisible())
		|| (Inventory && Inventory->IsEquipmentChangeInProgress())) { FinishAdvance(false); return; }
	Owner->ConsumeMovementInputVector();
	Owner->SetActorRotation(LockedDirection.Rotation());
	if (!StillOwned()) { return; }
	if (FVector::DistSquared(Owner->GetActorLocation(), LastOwnedLocation) > 1.0) { FinishAdvance(false); return; }
	Movement->StopMovementImmediately();
	if (!StillOwned()) { return; }
	if (!DoesMontageContinue(*Owner, AcceptedAnimInstance.Get(), AcceptedMontage.Get(), AcceptedMontageInstanceId))
	{ FinishAdvance(false); return; }
	const auto Step = Timeline.Step(GetWorld()->GetTimeSeconds());
	if (!Step.bValid) { FinishAdvance(false); return; }
	if (Step.Distance > 0.0)
	{
		// Bound ground probes even after a hitch; a valid far endpoint must not bridge a wide pit.
		double Remaining = Step.Distance;
		while (Remaining > 0.001)
		{
			const double Distance = FMath::Min(Remaining, 20.0);
			const FVector Delta = LockedDirection * Distance;
			if (!DoesMontageContinue(*Owner, AcceptedAnimInstance.Get(), AcceptedMontage.Get(), AcceptedMontageInstanceId))
			{ FinishAdvance(false); return; }
			if (!HasSupportedFloor(*Movement, Owner->GetActorLocation() + Delta)) { Timeline.StopTravel(); break; }
			FHitResult Hit;
			const FVector BeforeMove = Owner->GetActorLocation();
			Movement->SafeMoveUpdatedComponent(Delta, Owner->GetActorQuat(), true, Hit);
			if (!StillOwned()) { return; }
			if (!DoesMontageContinue(*Owner, AcceptedAnimInstance.Get(), AcceptedMontage.Get(), AcceptedMontageInstanceId))
			{ FinishAdvance(false); return; }
			const FVector ActualDelta = Owner->GetActorLocation() - BeforeMove;
			const double Along = FVector::DotProduct(ActualDelta, LockedDirection);
			if (Owner->GetActorLocation().ContainsNaN() || Along < -1.0 || Along > Distance + 1.0
				|| (ActualDelta - LockedDirection * Along).SizeSquared() > 1.0)
			{
				FinishAdvance(false); return;
			}
			LastOwnedLocation = Owner->GetActorLocation();
			if (Hit.bBlockingHit || Hit.bStartPenetrating) { Timeline.StopTravel(); break; }
			Remaining -= Distance;
		}
	}
	// Hold the real endpoint until the original accepted montage contact deadline.
	if (Step.bContactReady) { FinishAdvance(true); }
}

void URogue10mTechniqueMotionComponent::CancelAdvance()
{
	// A cancellation during Walking restoration must also invalidate the pending contact broadcast.
	if (!ActiveExecutionId) { ++Generation; return; }
	FinishAdvance(false);
}

void URogue10mTechniqueMotionComponent::FinishAdvance(bool bCommitContact)
{
	if (!ActiveExecutionId) { return; }
	const uint32 FinishedId = ActiveExecutionId;
	const ERogue10mWeaponType FinishedWeapon = AcceptedWeapon;
	const FVector FinishedEndpoint = LastOwnedLocation;
	const FVector FinishedOrigin = AcceptedOrigin;
	const TWeakObjectPtr<UAnimInstance> FinishedAnimInstance = AcceptedAnimInstance;
	const TWeakObjectPtr<UAnimMontage> FinishedMontage = AcceptedMontage;
	const int32 FinishedMontageId = AcceptedMontageInstanceId;
	const uint64 FinishedGeneration = ++Generation;
	auto* FinishedOwner = Character.Get();
	ActiveExecutionId = 0; Timeline.Cancel();
	Character.Reset();
	AcceptedAnimInstance.Reset(); AcceptedMontage.Reset(); AcceptedMontageInstanceId = INDEX_NONE;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(StepTimer); }
	if (auto* Owner = FinishedOwner)
	{
		Owner->ConsumeMovementInputVector();
		if (auto* Movement = Owner->GetCharacterMovement(); Movement && Movement->MovementMode == MOVE_None
			&& Movement->UpdatedComponent == Owner->GetCapsuleComponent() && !Owner->IsDead())
		{
			Movement->StopMovementImmediately();
			if (Generation == FinishedGeneration)
			{ Movement->SetMovementMode(static_cast<EMovementMode>(PreviousMovementMode), PreviousCustomMode); }
		}
	}
	bool bStableEndpoint = false;
	if (IsValid(FinishedOwner) && !FinishedOwner->IsDead() && !FinishedOwner->IsDodging()
		&& FinishedOwner->IsThirdPersonGameplayActive() && FinishedOwner->GetController()
		&& FinishedOwner->GetEquippedWeaponType() == FinishedWeapon)
	{
		const auto* Movement = FinishedOwner->GetCharacterMovement();
		const FVector Displacement = FinishedOwner->GetActorLocation() - FinishedEndpoint;
		// Walking restoration may adjust floor height; it must not relocate the contact horizontally.
		bStableEndpoint = Movement && Movement->MovementMode == MOVE_Walking
			&& Movement->UpdatedComponent == FinishedOwner->GetCapsuleComponent()
			&& HasSupportedFloor(*Movement, FinishedOwner->GetActorLocation())
			&& !Displacement.ContainsNaN() && Displacement.SizeSquared2D() <= 1.0
			&& FMath::Abs(Displacement.Z) <= Movement->MaxStepHeight;
	}
	const bool bCommit = bCommitContact && Generation == FinishedGeneration && bStableEndpoint
		&& DoesMontageContinue(*FinishedOwner, FinishedAnimInstance.Get(), FinishedMontage.Get(), FinishedMontageId);
	UE_LOG(LogRogue10m, Display, TEXT("TECHNIQUE_ADVANCE END execution=%u commit=%d swept_distance_cm=%.3f endpoint=%s"),
		FinishedId, bCommit ? 1 : 0, (FinishedEndpoint - FinishedOrigin).Size2D(), *FinishedEndpoint.ToString());
	OnAdvanceFinished.Broadcast(FinishedId, bCommit);
}

void URogue10mTechniqueMotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelAdvance();
	Super::EndPlay(Reason);
}
