#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTechniqueMotionComponent.h"
#include "TimerManager.h"

// These are native UE automation tests, not mocked Unreal tests. They require an
// initialized engine and physics world; authoring this file does not execute them.
struct FRogue10mTechniqueInterruptionTestAccess
{
	struct FCooldownSnapshot
	{
		float Start;
		float End;
		float Duration;
		const URogue10mAttackSkillData* Source;
	};

	static void Configure(URogue10mCombatComponent& Combat)
	{
		Combat.CharacterData = nullptr;
	}
	static bool Execute(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{
		return Combat.ExecuteAttackSkill(Skill, false);
	}
	static int32 ExecutionCount(const URogue10mCombatComponent& Combat)
	{
		return Combat.ActiveAttackExecutions.Num();
	}
	static uint64 CancellationEpoch(const URogue10mCombatComponent& Combat)
	{
		return Combat.AdvanceCancellationEpoch;
	}
	static FCooldownSnapshot Cooldown(const URogue10mCombatComponent& Combat)
	{
		return {Combat.AttackCooldownStartTime, Combat.AttackCooldownEndTime,
			Combat.AttackCooldownDuration, Combat.AttackCooldownSourceSkill.Get()};
	}
	static void ReserveCooldown(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{
		Combat.StartSharedAttackCooldown(Skill, false);
	}
	static bool MontageContinues(const ARogue10mCharacter& Character, UAnimInstance* Anim,
		const UAnimMontage* Montage, int32 InstanceId)
	{
		return URogue10mTechniqueMotionComponent::DoesMontageContinue(Character, Anim, Montage, InstanceId);
	}
	static uint32 SeedPendingContact(URogue10mCombatComponent& Combat, URogue10mAttackSkillData& Skill,
		UAnimInstance& Anim, UAnimMontage& Montage, int32 InstanceId)
	{
		const uint32 Id = Combat.NextAttackExecutionId++;
		auto& Execution = Combat.ActiveAttackExecutions.Add(Id);
		Execution.ExecutionId = Id;
		Execution.SkillData = &Skill;
		Execution.bAdvanceEndpointContact = true;
		Execution.AdvanceEpoch = Combat.AdvanceCancellationEpoch;
		Execution.AdvanceAnimInstance = &Anim;
		Execution.AdvanceMontage = &Montage;
		Execution.AdvanceMontageInstanceId = InstanceId;
		return Id;
	}
	static int32 Pulses(const URogue10mCombatComponent& Combat, uint32 Id)
	{
		const auto* Execution = Combat.ActiveAttackExecutions.Find(Id);
		return Execution ? Execution->CompletedPulses : 0;
	}
	static void Complete(URogue10mCombatComponent& Combat, uint32 Id, bool bCommit)
	{
		Combat.HandleTechniqueAdvanceFinished(Id, bCommit);
	}
	static void DiscardPending(URogue10mCombatComponent& Combat, uint32 Id)
	{
		Combat.FinishAttackHitSequence(Id);
	}
	static void BindMotion(URogue10mCombatComponent& Combat, URogue10mTechniqueMotionComponent& Motion)
	{
		Combat.TechniqueMotion = &Motion;
		Motion.OnAdvanceFinished.AddUObject(&Combat, &URogue10mCombatComponent::HandleTechniqueAdvanceFinished);
	}
	static bool HasMotionTimer(const URogue10mTechniqueMotionComponent& Motion)
	{
		return Motion.GetWorld()->GetTimerManager().TimerExists(Motion.StepTimer);
	}
	static void MakePendingNonAdvance(URogue10mCombatComponent& Combat, uint32 Id)
	{
		Combat.ActiveAttackExecutions.FindChecked(Id).bAdvanceEndpointContact = false;
	}
};

namespace Rogue10mTechniqueInterruptionTests
{
using Access = FRogue10mTechniqueInterruptionTestAccess;

// Native montage state without a skeleton, content load, animation graph, world
// tick or rendered pose. Production Montage_Play is intentionally not claimed.
struct FSyntheticMontagePlayback
{
	USkeletalMeshComponent* Mesh;
	UAnimInstance* Anim;
	UAnimMontage* Montage;
	TArray<FAnimMontageInstance*> OwnedInstances;

	explicit FSyntheticMontagePlayback(ARogue10mCharacter& Character)
		: Mesh(Character.GetMesh()), Anim(NewObject<UAnimInstance>(Mesh)), Montage(NewObject<UAnimMontage>(Anim))
	{
		Mesh->AnimScriptInstance = Anim;
	}
	~FSyntheticMontagePlayback()
	{
		// GetMontageInstanceForID only searches this public array. Remove the
		// synthetic allocations before engine teardown can own/delete them.
		for (auto* Instance : OwnedInstances)
		{
			Anim->MontageInstances.Remove(Instance);
			delete Instance;
		}
		Mesh->AnimScriptInstance = nullptr;
	}
	FAnimMontageInstance* AddInstance()
	{
		auto* Instance = new FAnimMontageInstance(Anim);
		Instance->Initialize(Montage);
		Instance->Play(1.0f);
		Anim->MontageInstances.Add(Instance);
		OwnedInstances.Add(Instance);
		return Instance;
	}
};

static UWorld* CreatePhysicsWorld(FAutomationTestBase& Test)
{
	if (!GEngine) { Test.AddError(TEXT("An initialized engine is required")); return nullptr; }
	const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (World) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	else { Test.AddError(TEXT("Physics world creation failed")); }
	return World;
}

static URogue10mAttackSkillData* MakeAdvanceSkill(UObject* Outer)
{
	auto* Skill = NewObject<URogue10mAttackSkillData>(Outer);
	Skill->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::Common;
	Skill->AttackShape = ERogue10mAttackShape::LinearBox;
	Skill->HitMode = ERogue10mAttackHitMode::Single;
	Skill->HitCount = 1;
	Skill->bEnableCombo = false;
	Skill->NextComboSkill = nullptr;
	Skill->bAdvanceBeforeHit = true;
	Skill->AdvanceDistance = 150.0f;
	Skill->AdvanceDuration = 0.25f;
	Skill->AnimationPlayRate = 1.0f;
	Skill->HitStartDelaySeconds = 0.1f;
	Skill->AttackCooldown = 1.5f;
	Skill->ResourceCosts.Emplace(ERogue10mAttackResourceType::Stamina, 10.0f);
	// No content load or rendered playback is needed: the resource callback
	// cancels the accepted advance before montage playback or execution creation.
	Skill->AttackMontage = NewObject<UAnimMontage>(Skill);
	return Skill;
}

static void CheckCooldown(FAutomationTestBase& Test, const Access::FCooldownSnapshot& Actual,
	const Access::FCooldownSnapshot& Expected)
{
	Test.TestEqual(TEXT("Shared cooldown start is preserved"), Actual.Start, Expected.Start);
	Test.TestEqual(TEXT("Shared cooldown deadline is preserved"), Actual.End, Expected.End);
	Test.TestEqual(TEXT("Shared cooldown duration is preserved"), Actual.Duration, Expected.Duration);
	Test.TestTrue(TEXT("Shared cooldown source is preserved"), Actual.Source == Expected.Source);
}

static bool RunResourceCallbackCase(FAutomationTestBase& Test, bool bReserveLongerCooldown)
{
	if (!GEngine) { Test.AddError(TEXT("An initialized engine is required")); return false; }
	const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (!World) { Test.AddError(TEXT("Physics world creation failed")); return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

	auto* FloorActor = World->SpawnActor<AActor>();
	if (!FloorActor) { Test.AddError(TEXT("Floor actor creation failed")); return false; }
	auto* Floor = NewObject<UBoxComponent>(FloorActor);
	Floor->SetMobility(EComponentMobility::Static);
	Floor->SetBoxExtent(FVector(1000.0, 1000.0, 10.0));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionObjectType(ECC_WorldStatic);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	FloorActor->SetRootComponent(Floor);
	FloorActor->AddInstanceComponent(Floor);
	FloorActor->SetActorLocation(FVector(0.0, 0.0, -10.0));
	Floor->RegisterComponent();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Character = World->SpawnActor<ARogue10mCatalogTestCharacter>(FVector(0.0, 0.0, 100.0),
		FRotator::ZeroRotator, SpawnParameters);
	auto* State = World->SpawnActor<ARogue10mPlayerState>();
	auto* Controller = World->SpawnActor<APlayerController>();
	if (!Character || !State || !Controller) { Test.AddError(TEXT("Native pawn/state/controller fixture failed")); return false; }
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	Character->SetPlayerState(State);
	State->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	Character->SetActorLocation(FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0));
	auto* Movement = Character->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	auto* ASC = State->GetAbilitySystemComponent();
	auto* Attributes = State->GetRogueAttributeSet();
	auto* Combat = Character->GetCombatComponent();
	if (!ASC || !Attributes || !Combat) { Test.AddError(TEXT("Native combat/ASC/attribute fixture failed")); return false; }
	ASC->AddAttributeSetSubobject(Attributes);
	ASC->InitAbilityActorInfo(State, Character);
	ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 1.0f);
	Access::Configure(*Combat);
	auto* Skill = MakeAdvanceSkill(Combat);
	auto* LongerCooldownSkill = NewObject<URogue10mAttackSkillData>(Combat);
	LongerCooldownSkill->AttackCooldown = 10.0f;
	if (!Test.TestTrue(TEXT("Real floor/authority/controller/Walking fixture accepts advance preflight"),
		URogue10mTechniqueMotionComponent::CanStart(*Character, Skill->AdvanceDistance,
			Skill->AdvanceDuration, Skill->HitStartDelaySeconds))) { return false; }

	const FVector Origin = Character->GetActorLocation();
	const int32 UsesBefore = State->GetSkillUseCount(Skill->GetFName());
	const uint64 EpochBefore = Access::CancellationEpoch(*Combat);
	int32 CostNotifications = 0;
	bool bReentrantAccepted = false;
	Access::FCooldownSnapshot ReservedCooldown{};
	auto& StaminaChanged = ASC->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetStaminaAttribute());
	const FDelegateHandle Handle = StaminaChanged.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (Change.NewValue >= Change.OldValue) { return; }
		++CostNotifications;
		Combat->CancelHeldAttackInput();
		if (bReserveLongerCooldown)
		{
			Access::ReserveCooldown(*Combat, *LongerCooldownSkill);
			ReservedCooldown = Access::Cooldown(*Combat);
		}
		// Cancellation during acceptance must not permit reentrant payment.
		// Attempt once so a broken guard fails assertions rather than recursing forever.
		if (CostNotifications == 1) { bReentrantAccepted = Access::Execute(*Combat, *Skill); }
	});
	ON_SCOPE_EXIT { StaminaChanged.Remove(Handle); };

	Test.TestTrue(TEXT("Paid action remains accepted when the real stamina delegate cancels synchronously"),
		Access::Execute(*Combat, *Skill));
	Test.TestEqual(TEXT("Stamina cost broadcasts exactly once"), CostNotifications, 1);
	Test.TestEqual(TEXT("Exactly one stamina cost is retained"), Attributes->GetStamina(), 90.0f);
	Test.TestEqual(TEXT("Exactly one use is retained"), State->GetSkillUseCount(Skill->GetFName()), UsesBefore + 1);
	Test.TestTrue(TEXT("The cancellation callback actually advances the cancellation epoch"),
		Access::CancellationEpoch(*Combat) > EpochBefore);
	Test.TestFalse(TEXT("Reentrant input during the callback is rejected"), bReentrantAccepted);
	Test.TestEqual(TEXT("Cancelled acceptance schedules no attack executions"), Access::ExecutionCount(*Combat), 0);
	Test.TestFalse(TEXT("Cancelled acceptance owns no technique movement"), Combat->IsTechniqueMovementActive());
	Test.TestTrue(TEXT("Cancelled acceptance moves the capsule zero distance"), Character->GetActorLocation().Equals(Origin));
	Test.TestEqual(TEXT("Cancelled acceptance keeps Walking mode"), static_cast<int32>(Movement->MovementMode), static_cast<int32>(MOVE_Walking));
	Test.TestTrue(TEXT("The accepted action still has a shared cooldown"), Combat->IsAttackOnCooldown(World->GetTimeSeconds()));
	const auto PaidCooldown = Access::Cooldown(*Combat);
	if (bReserveLongerCooldown)
	{
		Test.TestTrue(TEXT("Callback reserves a cooldown longer than the accepted skill"),
			ReservedCooldown.End > World->GetTimeSeconds() + Skill->AttackCooldown);
		CheckCooldown(Test, PaidCooldown, ReservedCooldown);
	}
	else
	{
		const float Now = static_cast<float>(World->GetTimeSeconds());
		const Access::FCooldownSnapshot Expected{Now, Now + Skill->AttackCooldown, Skill->AttackCooldown, Skill};
		CheckCooldown(Test, PaidCooldown, Expected);
	}
	Test.TestFalse(TEXT("Immediate input after the cancelled action is rejected by its cooldown"), Access::Execute(*Combat, *Skill));
	Test.TestEqual(TEXT("Rejected retry cannot pay a second stamina cost"), CostNotifications, 1);
	Test.TestEqual(TEXT("Rejected retry leaves stamina unchanged"), Attributes->GetStamina(), 90.0f);
	Test.TestEqual(TEXT("Rejected retry cannot record a second use"), State->GetSkillUseCount(Skill->GetFName()), UsesBefore + 1);
	CheckCooldown(Test, Access::Cooldown(*Combat), PaidCooldown);
	return true;
}

static bool RunMontageIdentityCase(FAutomationTestBase& Test)
{
	auto* World = CreatePhysicsWorld(Test);
	if (!World) { return false; }
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Character = World->SpawnActor<ARogue10mCatalogTestCharacter>();
	if (!Character) { Test.AddError(TEXT("Native montage fixture pawn failed")); return false; }
	FSyntheticMontagePlayback Playback(*Character);
	auto* Instance = Playback.AddInstance();
	const int32 AcceptedId = Instance->GetInstanceID();
	const auto Continues = [&]() { return Access::MontageContinues(*Character, Playback.Anim, Playback.Montage, AcceptedId); };
	if (!Test.TestTrue(TEXT("Synthetic UE montage is active and playing on the actual playback mesh"), Continues())) { return false; }
	Test.TestFalse(TEXT("Null animation instance is rejected"), Access::MontageContinues(*Character, nullptr, Playback.Montage, AcceptedId));
	Test.TestFalse(TEXT("Null montage is rejected"), Access::MontageContinues(*Character, Playback.Anim, nullptr, AcceptedId));
	Test.TestFalse(TEXT("Unset montage instance ID is rejected"), Access::MontageContinues(*Character, Playback.Anim, Playback.Montage, INDEX_NONE));
	Test.TestFalse(TEXT("Missing montage instance ID is rejected"), Access::MontageContinues(*Character, Playback.Anim, Playback.Montage, MAX_int32));
	auto* DifferentMontage = NewObject<UAnimMontage>(Playback.Anim);
	Test.TestFalse(TEXT("Matching instance ID cannot authorize a different montage asset"),
		Access::MontageContinues(*Character, Playback.Anim, DifferentMontage, AcceptedId));
	Instance->Pause();
	Test.TestFalse(TEXT("A paused accepted instance cannot continue movement/contact"), Continues());
	Instance->Play(1.0f);
	Test.TestTrue(TEXT("Resuming the exact same accepted instance restores the guard"), Continues());
	Instance->Stop(FAlphaBlend(0.2f));
	Test.TestFalse(TEXT("A stopped/blending-out accepted instance cannot continue"), Continues());
	Instance->Play(1.0f);
	Test.TestTrue(TEXT("Positive control remains valid after replaying the same instance"), Continues());
	Playback.Anim->MontageInstances.Remove(Instance);
	Test.TestFalse(TEXT("Removing the accepted instance cancels its identity even while its object is playing"), Continues());
	auto* Restart = Playback.AddInstance();
	Test.TestTrue(TEXT("Restarting the same montage produces a different UE instance ID"), Restart->GetInstanceID() != AcceptedId);
	Test.TestFalse(TEXT("A same-asset restart cannot revive the removed accepted instance"), Continues());
	Test.TestTrue(TEXT("The replacement instance has its own valid identity"),
		Access::MontageContinues(*Character, Playback.Anim, Playback.Montage, Restart->GetInstanceID()));
	auto* ReplacementAnim = NewObject<UAnimInstance>(Playback.Mesh);
	Playback.Mesh->AnimScriptInstance = ReplacementAnim;
	Test.TestFalse(TEXT("Replacing the playback mesh's animation instance rejects the old live montage"),
		Access::MontageContinues(*Character, Playback.Anim, Playback.Montage, Restart->GetInstanceID()));
	return true;
}

static bool RunCompletionListenerCase(FAutomationTestBase& Test, bool bReplaceAnim)
{
	auto* World = CreatePhysicsWorld(Test);
	if (!World) { return false; }
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Character = World->SpawnActor<ARogue10mCatalogTestCharacter>();
	auto* State = World->SpawnActor<ARogue10mPlayerState>();
	auto* Controller = World->SpawnActor<APlayerController>();
	if (!Character || !State || !Controller) { Test.AddError(TEXT("Native completion fixture failed")); return false; }
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	Character->SetPlayerState(State);
	auto* ASC = State->GetAbilitySystemComponent();
	ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet());
	ASC->InitAbilityActorInfo(State, Character);
	auto* Combat = Character->GetCombatComponent();
	Access::Configure(*Combat);
	FSyntheticMontagePlayback Playback(*Character);
	auto* Instance = Playback.AddInstance();
	auto* Skill = MakeAdvanceSkill(Combat);
	// Observation-only sentinel: a Single pulse erases itself immediately. A
	// synthetic two-pulse execution lets a missing guard leave pulse 1 visible.
	// This deliberately bypasses acceptance; Continuous advance is NOT supported
	// gameplay. The resource tests above use valid Single/HitCount=1 acceptance.
	Skill->HitMode = ERogue10mAttackHitMode::Continuous;
	Skill->HitCount = 2;
	const uint32 ControlId = Access::SeedPendingContact(*Combat, *Skill, *Playback.Anim, *Playback.Montage, Instance->GetInstanceID());
	Access::Complete(*Combat, ControlId, true);
	if (!Test.TestEqual(TEXT("Positive control: a valid completion reaches the first actual native pulse"), Access::Pulses(*Combat, ControlId), 1)) { return false; }
	Access::DiscardPending(*Combat, ControlId);
	Combat->CancelHeldAttackInput();
	const uint32 PendingId = Access::SeedPendingContact(*Combat, *Skill, *Playback.Anim, *Playback.Montage, Instance->GetInstanceID());
	auto* CompletionDispatcher = NewObject<URogue10mTechniqueMotionComponent>(Character);
	int32 ListenerCalls = 0;
	bool bForwardedCommit = false;
	// The adapter deterministically models a preceding completion listener and
	// then invokes the real Combat handler. Multicast registration order itself
	// is not assumed; UE does not promise listener ordering.
	CompletionDispatcher->OnAdvanceFinished.AddLambda([&](uint32 Id, bool bCommit)
	{
		++ListenerCalls;
		bForwardedCommit = bCommit;
		if (bReplaceAnim) { Playback.Mesh->AnimScriptInstance = NewObject<UAnimInstance>(Playback.Mesh); }
		else { Instance->Stop(FAlphaBlend(0.2f)); }
		Access::Complete(*Combat, Id, bCommit);
	});
	CompletionDispatcher->OnAdvanceFinished.Broadcast(PendingId, true);
	Test.TestEqual(TEXT("Completion listener adapter executes exactly once"), ListenerCalls, 1);
	Test.TestTrue(TEXT("Combat receives true commit despite playback changing in the preceding listener"), bForwardedCommit);
	Test.TestEqual(TEXT("Changed playback cannot execute the sentinel first pulse"), Access::Pulses(*Combat, PendingId), 0);
	Test.TestEqual(TEXT("Changed playback removes the pending native attack execution"), Access::ExecutionCount(*Combat), 0);
	CompletionDispatcher->OnAdvanceFinished.Clear();
	return true;
}

static bool RunActualDamageCase(FAutomationTestBase& Test)
{
	auto* World = CreatePhysicsWorld(Test);
	if (!World) { return false; }
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	// APawn::ShouldTakeDamage requires an authority GameMode. Use only native
	// engine classes; do not initialize production GameMode, UI, assets or saves.
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
	if (!Test.TestTrue(TEXT("Native damage fixture creates its required authority GameMode"), World->SetGameMode(URL))) { return false; }
	auto* FloorActor = World->SpawnActor<AActor>();
	if (!FloorActor) { Test.AddError(TEXT("Damage fixture floor failed")); return false; }
	auto* Floor = NewObject<UBoxComponent>(FloorActor);
	Floor->SetMobility(EComponentMobility::Static);
	Floor->SetBoxExtent(FVector(1000.0, 1000.0, 10.0));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionObjectType(ECC_WorldStatic);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	FloorActor->SetRootComponent(Floor);
	FloorActor->AddInstanceComponent(Floor);
	FloorActor->SetActorLocation(FVector(0.0, 0.0, -10.0));
	Floor->RegisterComponent();

	auto* Character = World->SpawnActor<ARogue10mCatalogTestCharacter>();
	auto* State = World->SpawnActor<ARogue10mPlayerState>();
	auto* Controller = World->SpawnActor<APlayerController>();
	if (!Character || !State || !Controller) { Test.AddError(TEXT("Native damage pawn/state/controller fixture failed")); return false; }
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	Character->SetPlayerState(State);
	Character->SetCanBeDamaged(true);
	State->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	Character->SetActorLocation(FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0));
	auto* Movement = Character->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	auto* ASC = State->GetAbilitySystemComponent();
	auto* Attributes = State->GetRogueAttributeSet();
	ASC->AddAttributeSetSubobject(Attributes);
	ASC->InitAbilityActorInfo(State, Character);
	auto* Combat = Character->GetCombatComponent();
	Access::Configure(*Combat);
	FSyntheticMontagePlayback Playback(*Character);
	auto* Instance = Playback.AddInstance();
	auto* Skill = MakeAdvanceSkill(Combat);
	// Seed already-paid state to isolate interruption from Montage_Play, which
	// requires authored skeleton content. Real acceptance/payment is covered by
	// the stamina delegate tests. This test does not claim asset playback.
	ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetStaminaAttribute(), 90.0f);
	ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetHealthAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 1.0f);
	State->RecordSkillUse(Skill->GetFName());
	Access::ReserveCooldown(*Combat, *Skill);
	const auto PaidCooldown = Access::Cooldown(*Combat);
	const int32 PaidUseCount = State->GetSkillUseCount(Skill->GetFName());
	const uint32 AdvanceId = Access::SeedPendingContact(*Combat, *Skill, *Playback.Anim, *Playback.Montage, Instance->GetInstanceID());
	const uint32 NonAdvanceId = Access::SeedPendingContact(*Combat, *Skill, *Playback.Anim, *Playback.Montage, Instance->GetInstanceID());
	Access::MakePendingNonAdvance(*Combat, NonAdvanceId);
	auto* Motion = NewObject<URogue10mTechniqueMotionComponent>(Character);
	Character->AddInstanceComponent(Motion);
	Motion->RegisterComponent();
	Access::BindMotion(*Combat, *Motion);
	int32 FinishCalls = 0;
	bool bCommittedContact = false;
	Motion->OnAdvanceFinished.AddLambda([&](uint32 Id, bool bCommit)
	{
		Test.TestEqual(TEXT("Actual motion completion reports the seeded advance ID"), Id, AdvanceId);
		++FinishCalls;
		bCommittedContact |= bCommit;
	});
	ON_SCOPE_EXIT { Motion->OnAdvanceFinished.Clear(); Motion->CancelAdvance(); };
	if (!Test.TestTrue(TEXT("Actual grounded Motion.StartAdvance accepts the synthetic live montage identity"),
		Motion->StartAdvance(AdvanceId, FVector::ForwardVector, Skill->AdvanceDistance,
			Skill->AdvanceDuration, Skill->HitStartDelaySeconds, Playback.Anim, Playback.Montage, Instance->GetInstanceID()))) { return false; }
	Test.TestTrue(TEXT("Motion actually owns the advance before damage"), Motion->IsAdvancing());
	Test.TestEqual(TEXT("Advance owns MOVE_None before damage"), static_cast<int32>(Movement->MovementMode), static_cast<int32>(MOVE_None));
	Test.TestTrue(TEXT("Actual advance has an owned timer before damage"), Access::HasMotionTimer(*Motion));
	const FVector Origin = Character->GetActorLocation();
	const uint64 EpochBefore = Access::CancellationEpoch(*Combat);
	Attributes->SetHealth(99.0f);
	Test.TestTrue(TEXT("A direct nonlethal health attribute write is not an attack hit"), Motion->IsAdvancing());
	Test.TestEqual(TEXT("Health resource change does not discard pending attacks"), Access::ExecutionCount(*Combat), 2);
	Test.TestEqual(TEXT("Health resource change has no cancellation callback"), FinishCalls, 0);
	const FDamageEvent DamageEvent;
	Test.TestEqual(TEXT("Zero TakeDamage is rejected by the actual pawn path"), Character->TakeDamage(0.0f, DamageEvent, nullptr, nullptr), 0.0f);
	Test.TestTrue(TEXT("Zero damage leaves the advance running"), Motion->IsAdvancing());
	Test.TestEqual(TEXT("Zero damage leaves cancellation epoch unchanged"), Access::CancellationEpoch(*Combat), EpochBefore);
	const float HealthBeforeHit = Attributes->GetHealth();
	const float Applied = Character->TakeDamage(10.0f, DamageEvent, nullptr, nullptr);
	// Essential positive assertion: a missing GameMode must fail the fixture,
	// never count a silently rejected hit as a passing interruption test.
	if (!Test.TestTrue(TEXT("Actual Character/APawn damage path applies a finite positive hit"), FMath::IsFinite(Applied) && Applied > 0.0f)) { return false; }
	Test.TestEqual(TEXT("Health changes by the actual applied damage"), Attributes->GetHealth(), HealthBeforeHit - Applied);
	Test.TestFalse(TEXT("A real hit cancels owned advance immediately"), Motion->IsAdvancing());
	Test.TestFalse(TEXT("A real hit clears the owned advance timer"), Access::HasMotionTimer(*Motion));
	Test.TestEqual(TEXT("A real hit completes the advance exactly once"), FinishCalls, 1);
	Test.TestFalse(TEXT("Hit cancellation commits no endpoint contact"), bCommittedContact);
	Test.TestEqual(TEXT("A real hit discards both advance and ordinary pending attack executions"), Access::ExecutionCount(*Combat), 0);
	Test.TestTrue(TEXT("A real hit advances the cancellation epoch"), Access::CancellationEpoch(*Combat) > EpochBefore);
	Test.TestEqual(TEXT("A real hit restores Walking movement"), static_cast<int32>(Movement->MovementMode), static_cast<int32>(MOVE_Walking));
	Test.TestTrue(TEXT("Hit before the first movement step has zero travelled distance"), Character->GetActorLocation().Equals(Origin));
	Test.TestEqual(TEXT("Hit cancellation preserves already-paid stamina"), Attributes->GetStamina(), 90.0f);
	Test.TestEqual(TEXT("Hit cancellation preserves the accepted use count"), State->GetSkillUseCount(Skill->GetFName()), PaidUseCount);
	CheckCooldown(Test, Access::Cooldown(*Combat), PaidCooldown);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueResourceCallbackInterruptionTest,
	"Rogue10m.TechniqueMotion.ResourceCallbackInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueResourceCallbackInterruptionTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunResourceCallbackCase(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueResourceCallbackCooldownFloorTest,
	"Rogue10m.TechniqueMotion.ResourceCallbackCooldownFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueResourceCallbackCooldownFloorTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunResourceCallbackCase(*this, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueMontageIdentityTest,
	"Rogue10m.TechniqueMotion.MontageInstanceIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueMontageIdentityTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunMontageIdentityCase(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueCompletionStopTest,
	"Rogue10m.TechniqueMotion.CompletionListenerStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueCompletionStopTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunCompletionListenerCase(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueCompletionAnimReplacementTest,
	"Rogue10m.TechniqueMotion.CompletionListenerAnimReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueCompletionAnimReplacementTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunCompletionListenerCase(*this, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mTechniqueActualDamageInterruptionTest,
	"Rogue10m.TechniqueMotion.TakeDamageInterruptsPendingAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mTechniqueActualDamageInterruptionTest::RunTest(const FString&)
{
	return Rogue10mTechniqueInterruptionTests::RunActualDamageCase(*this);
}

#endif
