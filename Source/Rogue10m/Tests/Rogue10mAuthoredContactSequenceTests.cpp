#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mHitInterruptionTestTarget.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

// Native UE integration tests. Authoring this file does not run an engine, load
// content, prove Montage_Play, render a pose or validate authored animation/VFX.
struct FRogue10mAuthoredContactSequenceTestAccess
{
	struct FContactTicket
	{
		uint32 ExecutionId = 0;
		uint64 ArmGeneration = 0;
		int32 ExpectedContact = 0;
		FTimerHandle TimerHandle;
	};

	static void Configure(URogue10mCombatComponent& Combat) { Combat.CharacterData = nullptr; }
	static bool Execute(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{ return Combat.ExecuteAttackSkill(Skill, false); }
	static int32 Executions(const URogue10mCombatComponent& Combat) { return Combat.ActiveAttackExecutions.Num(); }
	static bool HasMonitor(const URogue10mCombatComponent& Combat)
	{ return Combat.GetWorld()->GetTimerManager().TimerExists(Combat.AuthoredContactMonitorTimer); }
	static FContactTicket Capture(const URogue10mCombatComponent& Combat)
	{
		for (const auto& Pair : Combat.ActiveAttackExecutions)
		{
			if (Pair.Value.bAuthoredContactSequence)
			{
				return {Pair.Key, Pair.Value.AuthoredTimeline.GetArmGeneration(),
					Pair.Value.AuthoredTimeline.CompletedCount() + 1, Pair.Value.TimerHandle};
			}
		}
		return {};
	}
	static int32 Completed(const URogue10mCombatComponent& Combat, uint32 Id)
	{
		const auto* Execution = Combat.ActiveAttackExecutions.Find(Id);
		return Execution ? Execution->AuthoredTimeline.CompletedCount() : INDEX_NONE;
	}
	static void Rearm(URogue10mCombatComponent& Combat, uint32 Id) { Combat.ScheduleNextAuthoredContact(Id); }
	static void PauseContact(URogue10mCombatComponent& Combat, const FContactTicket& Ticket)
	{ Combat.GetWorld()->GetTimerManager().PauseTimer(Ticket.TimerHandle); }
	static void Replay(URogue10mCombatComponent& Combat, const FContactTicket& Ticket)
	{ Combat.HandleAuthoredContactTimer(Ticket.ExecutionId, Ticket.ArmGeneration, Ticket.ExpectedContact); }
	static void Start(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill,
		float AcceptedRate, UAnimInstance* Anim, int32 MontageId)
	{
		// This is the production scheduling boundary after accepted presentation.
		// It deliberately does not claim payment or skeleton-backed Montage_Play.
		Combat.AcceptedAdvanceRotation = CastChecked<ARogue10mCharacter>(Combat.GetOwner())->GetCombatViewTransform().GetRotation();
		Combat.StartAttackHitSequence(Skill, AcceptedRate, Anim, MontageId);
	}
	static void Clear(URogue10mCombatComponent& Combat)
	{
		Combat.CancelCombatVisuals(false);
		TArray<uint32> Ids;
		Combat.ActiveAttackExecutions.GetKeys(Ids);
		for (uint32 Id : Ids) { Combat.FinishAttackHitSequence(Id); }
	}
};

namespace Rogue10mAuthoredContactSequenceTests
{
using Access = FRogue10mAuthoredContactSequenceTestAccess;
constexpr float StepSeconds = 0.005f;
constexpr double TimingTolerance = 0.012;

// A real native montage instance and its identity, without animation ticking.
// Strong references are required because these fixtures survive latent frames.
struct FPlayback
{
	USkeletalMeshComponent* Mesh;
	TStrongObjectPtr<UAnimInstance> Anim;
	TStrongObjectPtr<UAnimMontage> Montage;
	TArray<FAnimMontageInstance*> OwnedInstances;
	FAnimMontageInstance* Accepted = nullptr;
	FAnimMontageInstance* Replacement = nullptr;

	explicit FPlayback(ARogue10mCharacter& Character)
		: Mesh(Character.GetMesh()), Anim(NewObject<UAnimInstance>(Mesh)),
		  Montage(NewObject<UAnimMontage>(Anim.Get()))
	{
		Mesh->SetComponentTickEnabled(false);
		Mesh->AnimScriptInstance = Anim.Get();
		Montage->SetCompositeLength(2.0f);
		Montage->RateScale = 1.0f;
		Montage->BlendOutTriggerTime = 0.0f; // The authored path rejects early automatic blend-out.
		Montage->AddAnimCompositeSection(TEXT("Default"), 0.0f);
		Accepted = AddInstance();
	}
	~FPlayback()
	{
		// Full animation ticks never own/delete these manual allocations.
		for (auto* Instance : OwnedInstances)
		{
			Anim->MontageInstances.Remove(Instance);
			delete Instance;
		}
		Mesh->AnimScriptInstance = nullptr;
	}
	FAnimMontageInstance* AddInstance()
	{
		auto* Instance = new FAnimMontageInstance(Anim.Get());
		Instance->Initialize(Montage.Get());
		Instance->Play(1.0f);
		Anim->MontageInstances.Add(Instance);
		OwnedInstances.Add(Instance);
		return Instance;
	}
	void ReplaceSameAsset()
	{
		Anim->MontageInstances.Remove(Accepted);
		Replacement = AddInstance();
	}
};

struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mCatalogTestCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	APlayerController* Controller = nullptr;
	URogue10mCombatComponent* Combat = nullptr;
	UAbilitySystemComponent* ASC = nullptr;
	URogue10mAttributeSet* Attributes = nullptr;
	ARogue10mHitInterruptionTestTarget* Target = nullptr;
	TStrongObjectPtr<URogue10mAttackSkillData> Skill;
	TStrongObjectPtr<URogue10mAttackSkillData> ProbeSkill;
	TUniquePtr<FPlayback> Playback;
	TArray<double> DamageTimes;
	double StartedAt = 0.0;
	float ReflectedDamage = 0.0f;
	bool bReady = false;

	explicit FFixture(FAutomationTestBase& Test)
	{
		if (!GEngine) { Test.AddError(TEXT("An initialized engine is required")); return; }
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!World) { Test.AddError(TEXT("Native timer/query world creation failed")); return; }
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		if (!World->SetGameMode(URL)) { Test.AddError(TEXT("Authority damage GameMode creation failed")); return; }
		World->InitializeActorsForPlay(URL);
		if (!World->AreActorsInitialized() || World->HasBegunPlay())
		{ Test.AddError(TEXT("Native interface dispatch must be initialized without gameplay BeginPlay")); return; }
		Character = World->SpawnActor<ARogue10mCatalogTestCharacter>();
		State = World->SpawnActor<ARogue10mPlayerState>();
		Controller = World->SpawnActor<APlayerController>();
		if (!Character || !State || !Controller) { Test.AddError(TEXT("Native pawn/state/controller fixture failed")); return; }
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Character);
		Character->SetPlayerState(State);
		Character->SetCanBeDamaged(true);
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		State->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
		Combat = Character->GetCombatComponent();
		ASC = State->GetAbilitySystemComponent();
		Attributes = State->GetRogueAttributeSet();
		if (!Combat || !ASC || !Attributes) { Test.AddError(TEXT("Native combat/ASC/attributes missing")); return; }
		ASC->AddAttributeSetSubobject(Attributes);
		ASC->InitAbilityActorInfo(State, Character);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetHealthAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetManaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 1.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetMinDamageRatioAttribute(), 1.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetMaxDamageRatioAttribute(), 1.0f);
		ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetCriticalChanceAttribute(), 0.0f);
		Access::Configure(*Combat);
		Playback = MakeUnique<FPlayback>(*Character);
		Skill.Reset(NewObject<URogue10mAttackSkillData>(Combat));
		Skill->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::MouseChordOnly;
		Skill->bAdvanceBeforeHit = false;
		Skill->bEnableCombo = false;
		Skill->NextComboSkill = nullptr;
		Skill->HitMode = ERogue10mAttackHitMode::Continuous;
		Skill->HitCount = 3;
		Skill->MaxTargetsPerHit = 1;
		Skill->Damage = 10.0f;
		Skill->AttackCooldown = 1.5f;
		Skill->HitStartDelaySeconds = 0.03f; // Conflicting legacy fields must not drive an authored schedule.
		Skill->HitInterval = 0.06f;
		Skill->AnimationPlayRate = 1.0f;
		Skill->AttackShape = ERogue10mAttackShape::LinearBox;
		Skill->AttackRange = 400.0f;
		Skill->BoxHalfWidth = Skill->BoxHalfHeight = 50.0f;
		Skill->AttackMontage = Playback->Montage.Get();
		Skill->AuthoredContactTimes = {0.20f, 0.50f, 0.95f};
		Skill->AuthoredRecoveryEndTime = 0.0f; // Preserve the V1 contact-only release contract.
		Skill->ResourceCosts = {{ERogue10mAttackResourceType::Stamina, 10.0f}};
		const FTransform View = Character->GetCombatViewTransform();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Target = World->SpawnActor<ARogue10mHitInterruptionTestTarget>(
			View.GetLocation() + View.GetRotation().GetForwardVector() * 100.0f, FRotator::ZeroRotator, Spawn);
		if (!Target) { Test.AddError(TEXT("Native contact target spawn failed")); return; }
		auto* Scene = World->GetPhysicsScene();
		if (!Scene) { Test.AddError(TEXT("A real physics query scene is required")); return; }
		Scene->ProcessAsyncPhysicsStateJobs(true);
		Scene->ProcessDeferredCreatePhysicsState();
		Scene->Flush();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mAuthoredSequenceFixture), false, Character);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, View.GetLocation() + View.GetRotation().GetForwardVector() * 200.0f,
			View.GetRotation(), Objects, FCollisionShape::MakeBox(FVector(200.0f, 50.0f, 50.0f)), Query);
		if (!Test.TestTrue(TEXT("Native attack-volume positive control finds the registered target before filtering"),
			Overlaps.ContainsByPredicate([this](const FOverlapResult& Hit) { return Hit.GetActor() == Target; }))) { return; }
		// Prime pending timers at zero time before recording the start. Never mutate GFrameCounter.
		World->GetTimerManager().Tick(0.0f);
		bReady = Test.TestTrue(TEXT("Fixture is locally controlled, grounded and uses third-person gameplay"),
			Character->IsLocallyControlled() && Character->GetCharacterMovement()->IsMovingOnGround()
			&& Character->IsThirdPersonGameplayActive());
	}
	~FFixture()
	{
		if (Target) { Target->DamageCallback = {}; Target->FilterCallback = {}; }
		if (Combat) { Access::Clear(*Combat); }
		Playback.Reset();
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	float HitOwner()
	{
		const FDamageEvent Damage;
		return Character->TakeDamage(10.0f, Damage, nullptr, nullptr);
	}
	void AdvanceOneEngineFrame()
	{
		// TimeOnly advances the real UWorld clock without character/animation ticks.
		// The latent command calls this once per engine frame, so TimerManager's
		// LastTickedFrame guard is respected without modifying the global counter.
		const int32 PreviousHits = Target->DamageCalls;
		World->Tick(LEVELTICK_TimeOnly, StepSeconds);
		World->GetTimerManager().Tick(StepSeconds);
		for (int32 Hit = PreviousHits; Hit < Target->DamageCalls; ++Hit)
		{ DamageTimes.Add(World->GetTimeSeconds() - StartedAt); }
	}
	bool EnableCombo(FAutomationTestBase& Test)
	{
		auto* Town = State->FindComponentByClass<URogue10mTownProgressionComponent>();
		auto* Traits = Character->GetTraitRuntimeComponent();
		if (!Town || !Traits) { Test.AddError(TEXT("Native Combo trait components missing")); return false; }
		Town->SetLobbyBudget(1, 1);
		auto Snapshot = Town->CaptureProgression();
		auto& Investment = Snapshot.TraitInvestments.AddDefaulted_GetRef();
		Investment.Trait = ERogue10mTownTrait::Combo;
		Investment.Rank = Investment.SpentPoints = 1;
		Investment.Bonus = 0.04f;
		if (!Test.TestTrue(TEXT("Native Combo investment restores without assets or saves"), Town->RestoreProgression(Snapshot))) { return false; }
		Traits->ResetRunState();
		Traits->NotifyAttackHit(0x7fff, *Target);
		return Test.TestEqual(TEXT("Positive control starts a one-hit Combo chain"), Traits->GetComboHitCount(), 1);
	}
};

enum class ECase : uint8
{
	NonUniform, SpeedSnapshot, FixedAim, ZeroFirstContact, HitBeforeFirst, HitAfterFirst,
	MontageStop, SameAssetReplacement, ReflectedHit, EmptyLegacy, MontageRateScale,
	InstanceRateChange, AssetRateChange, AutoBlendOutDisabled
};

class FRunSequence final : public IAutomationLatentCommand
{
public:
	FRunSequence(FAutomationTestBase& InTest, TSharedRef<FFixture> InFixture, ECase InCase)
		: Test(InTest), F(MoveTemp(InFixture)), Case(InCase), LastEngineFrame(GFrameCounter), StartedRealTime(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - StartedRealTime > 30.0)
		{ Test.AddError(TEXT("Authored-contact fixture did not complete within 30 seconds of real engine frames")); return true; }
		if (LastEngineFrame == GFrameCounter) { return false; }
		LastEngineFrame = GFrameCounter;
		F->AdvanceOneEngineFrame();
		const double Elapsed = F->World->GetTimeSeconds() - F->StartedAt;
		const double At = Case == ECase::HitBeforeFirst ? 0.05 : Case == ECase::SpeedSnapshot ? 0.15 : 0.30;
		if (!bIntervened && Elapsed >= At)
		{
			bIntervened = true;
			switch (Case)
			{
			case ECase::SpeedSnapshot:
				F->ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 0.25f);
				break;
			case ECase::FixedAim:
				F->Controller->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
				F->Character->GetGameplayCameraComponent()->SetWorldRotation(FRotator(0.0f, 90.0f, 0.0f));
				{
					const FTransform NewView = F->Character->GetCombatViewTransform();
					Test.TestTrue(TEXT("Positive control: current combat aim really turns away from the accepted target"),
						FVector::DotProduct(NewView.GetRotation().GetForwardVector(),
							(F->Target->GetActorLocation() - NewView.GetLocation()).GetSafeNormal()) < 0.30f);
				}
				break;
			case ECase::HitBeforeFirst:
			case ECase::HitAfterFirst:
				Test.TestTrue(TEXT("Intervention applies a finite positive hit through the actual pawn path"), F->HitOwner() > 0.0f);
				Test.TestEqual(TEXT("An actual hit immediately removes the contact execution"), Access::Executions(*F->Combat), 0);
				break;
			case ECase::MontageStop: F->Playback->Accepted->Stop(FAlphaBlend(0.08f)); break;
			case ECase::InstanceRateChange: F->Playback->Accepted->SetPlayRate(2.0f); break;
			case ECase::AssetRateChange: F->Playback->Montage->RateScale = 2.0f; break;
			case ECase::SameAssetReplacement:
				F->Playback->ReplaceSameAsset();
				Test.TestTrue(TEXT("Same-asset replacement has a different native instance ID"),
					F->Playback->Accepted->GetInstanceID() != F->Playback->Replacement->GetInstanceID());
				break;
			default: break;
			}
		}
		if (!bCheckedMonitor && bIntervened && Elapsed >= At + 0.04
			&& (Case == ECase::MontageStop || Case == ECase::SameAssetReplacement
				|| Case == ECase::InstanceRateChange || Case == ECase::AssetRateChange))
		{
			bCheckedMonitor = true;
			Test.TestEqual(TEXT("The cancellation monitor removes stopped, replaced or rate-mutated playback before the next contact"),
				Access::Executions(*F->Combat), 0);
		}
		if (Elapsed < 1.15) { return false; }
		const bool bCancelled = Case == ECase::HitBeforeFirst || Case == ECase::HitAfterFirst
			|| Case == ECase::MontageStop || Case == ECase::SameAssetReplacement || Case == ECase::ReflectedHit
			|| Case == ECase::InstanceRateChange || Case == ECase::AssetRateChange;
		const int32 ExpectedHits = Case == ECase::HitBeforeFirst ? 0 : bCancelled ? 1 : 3;
		Test.TestEqual(TEXT("Only the expected native damage contacts occur, with no residual hits"), F->Target->DamageCalls, ExpectedHits);
		Test.TestEqual(TEXT("Damage observations match virtual target calls"), F->DamageTimes.Num(), ExpectedHits);
		Test.TestEqual(TEXT("The completed or cancelled sequence owns no execution"), Access::Executions(*F->Combat), 0);
		Test.TestFalse(TEXT("The completed or cancelled sequence leaves no repeating cancellation monitor"), Access::HasMonitor(*F->Combat));
		if (!bCancelled && F->DamageTimes.Num() == 3)
		{
			TArray<double> Expected{0.20, 0.50, 0.95};
			if (Case == ECase::SpeedSnapshot || Case == ECase::MontageRateScale) { Expected = {0.10, 0.25, 0.475}; }
			if (Case == ECase::ZeroFirstContact) { Expected = {0.0, 0.30, 0.80}; }
			if (Case == ECase::EmptyLegacy) { Expected = {0.10, 0.185, 0.27}; }
			for (int32 Index = 0; Index < Expected.Num(); ++Index)
			{
				Test.TestTrue(*FString::Printf(TEXT("Contact %d follows its accepted deadline (observed %.4f, expected %.4f)"),
					Index + 1, F->DamageTimes[Index], Expected[Index]), FMath::Abs(F->DamageTimes[Index] - Expected[Index]) <= TimingTolerance);
			}
		}
		if (Case == ECase::SameAssetReplacement)
		{
			Test.TestTrue(TEXT("Cancelling the accepted ID does not stop the replacement playback"),
				F->Playback->Replacement && F->Playback->Replacement->IsActive() && F->Playback->Replacement->IsPlaying());
		}
		if (Case == ECase::ReflectedHit)
		{
			Test.TestTrue(TEXT("The first committed target contact reflects finite positive real damage"),
				FMath::IsFinite(F->ReflectedDamage) && F->ReflectedDamage > 0.0f);
			Test.TestEqual(TEXT("Reflected damage changes the owner's actual health"), F->Attributes->GetHealth(), 100.0f - F->ReflectedDamage);
		}
		if (Case == ECase::ReflectedHit || Case == ECase::HitAfterFirst || Case == ECase::NonUniform)
		{
			Test.TestEqual(TEXT("The committed execution awards one Combo hit credit and is never later reclassified as a miss"),
				F->Character->GetTraitRuntimeComponent()->GetComboHitCount(), 2);
		}
		return true;
	}
private:
	FAutomationTestBase& Test;
	TSharedRef<FFixture> F;
	ECase Case;
	uint64 LastEngineFrame;
	double StartedRealTime;
	bool bIntervened = false;
	bool bCheckedMonitor = false;
};

static bool Enqueue(FAutomationTestBase& Test, ECase Case)
{
	TSharedPtr<FFixture> F = MakeShared<FFixture>(Test);
	if (!Test.TestTrue(TEXT("Content-independent native timer/query fixture is ready"), F->bReady)) { return false; }
	float Rate = 1.0f;
	if (Case == ECase::SpeedSnapshot)
	{
		F->ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 2.5f);
		F->Skill->AnimationPlayRate = 0.8f;
		Rate = 2.0f;
		F->Playback->Accepted->Play(Rate);
	}
	if (Case == ECase::ZeroFirstContact) { F->Skill->AuthoredContactTimes = {0.0f, 0.30f, 0.80f}; }
	if (Case == ECase::MontageRateScale) { F->Playback->Montage->RateScale = 2.0f; }
	if (Case == ECase::AutoBlendOutDisabled)
	{
		F->Playback->Montage->bEnableAutoBlendOut = false;
		F->Playback->Montage->BlendOutTriggerTime = -1.0f;
		F->Playback->Accepted->Play(Rate); // FAnimMontageInstance caches this flag when Play is called.
		Test.TestFalse(TEXT("The accepted native instance also disables automatic blend-out"), F->Playback->Accepted->bEnableAutoBlendOut);
	}
	if (Case == ECase::ReflectedHit || Case == ECase::HitAfterFirst || Case == ECase::NonUniform)
	{
		if (!F->EnableCombo(Test)) { return false; }
	}
	if (Case == ECase::ReflectedHit)
	{
		F->Target->DamageCallback = [Fixture = F.Get()]() { Fixture->ReflectedDamage = Fixture->HitOwner(); };
	}
	F->StartedAt = F->World->GetTimeSeconds();
	if (Case == ECase::EmptyLegacy)
	{
		F->Skill->AuthoredContactTimes.Reset();
		F->Skill->AttackMontage = nullptr;
		F->Skill->AnimationPlayRate = 0.5f;
		F->Skill->HitStartDelaySeconds = 0.10f;
		F->Skill->HitInterval = 0.17f;
		F->ASC->SetNumericAttributeBase(URogue10mAttributeSet::GetAttackSpeedMultiplierAttribute(), 2.0f);
		const int32 Uses = F->State->GetSkillUseCount(F->Skill->GetFName());
		if (!Test.TestTrue(TEXT("An empty authored array accepts the actual legacy action without a montage"), Access::Execute(*F->Combat, *F->Skill))) { return false; }
		Test.TestEqual(TEXT("Actual legacy acceptance pays one stamina cost"), F->Attributes->GetStamina(), 90.0f);
		Test.TestEqual(TEXT("Actual legacy acceptance records one use"), F->State->GetSkillUseCount(F->Skill->GetFName()), Uses + 1);
	}
	else
	{
		Access::Start(*F->Combat, *F->Skill, Rate, F->Playback->Anim.Get(), F->Playback->Accepted->GetInstanceID());
	}
	if (!Test.TestEqual(TEXT("The native scheduling boundary creates one actual execution"), Access::Executions(*F->Combat), 1)) { return false; }
	Test.TestEqual(TEXT("No contact occurs before the first positive timer tick, including offset zero"), F->Target->DamageCalls, 0);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRunSequence>(Test, F.ToSharedRef(), Case));
	return true;
}

enum class EOwnershipCase : uint8
{
	StaleArmRearm, CancelRestart, RecoveryGate, ShortRecovery, RecoveryHit, RecoveryStop, RecoveryExpiresPendingContacts
};

// These tests replay the guarded production entry point with captured arguments.
// They do not assert that TimerManager itself duplicates a one-shot delegate.
class FRunOwnership final : public IAutomationLatentCommand
{
public:
	FRunOwnership(FAutomationTestBase& InTest, TSharedRef<FFixture> InFixture, EOwnershipCase InCase,
		Access::FContactTicket InOldTicket, Access::FContactTicket InCurrentTicket)
		: Test(InTest), F(MoveTemp(InFixture)), Case(InCase), OldTicket(InOldTicket), CurrentTicket(InCurrentTicket),
		  LastEngineFrame(GFrameCounter), StartedRealTime(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - StartedRealTime > 30.0)
		{ Test.AddError(TEXT("Authored action ownership fixture exceeded 30 seconds of real engine frames")); return true; }
		if (LastEngineFrame == GFrameCounter) { return false; }
		LastEngineFrame = GFrameCounter;
		F->AdvanceOneEngineFrame();
		const double Elapsed = F->World->GetTimeSeconds() - F->StartedAt;
		if (Case == EOwnershipCase::StaleArmRearm || Case == EOwnershipCase::CancelRestart)
		{
			if (Stage == 0 && Elapsed >= 0.23)
			{
				AssertReplayIgnored(OldTicket, 0, 0);
				Access::Replay(*F->Combat, CurrentTicket);
				Test.TestEqual(TEXT("The valid current arm still commits the first due contact"), F->Target->DamageCalls, 1);
				Test.TestEqual(TEXT("The valid current arm advances exactly once"), Access::Completed(*F->Combat, CurrentTicket.ExecutionId), 1);
				OldTicket = CurrentTicket; // A consumed first-contact callback is stale after the next arm.
				CurrentTicket = Access::Capture(*F->Combat);
				if (Case == EOwnershipCase::StaleArmRearm)
				{
					Test.TestTrue(TEXT("Contact two has a new arm generation"), OldTicket.ArmGeneration != CurrentTicket.ArmGeneration);
					Test.TestEqual(TEXT("Contact two keeps its original absolute ordinal"), CurrentTicket.ExpectedContact, 2);
					Access::PauseContact(*F->Combat, CurrentTicket);
				}
				Stage = 1;
			}
			if (Case == EOwnershipCase::StaleArmRearm && Stage == 1 && Elapsed >= 0.55)
			{
				// The next contact is now due. An unguarded old callback would consume it.
				AssertReplayIgnored(OldTicket, 1, 1);
				Access::Replay(*F->Combat, CurrentTicket);
				Test.TestEqual(TEXT("The valid second-contact arm survives stale replay"), F->Target->DamageCalls, 2);
				Test.TestEqual(TEXT("The valid second-contact arm advances exactly once"), Access::Completed(*F->Combat, CurrentTicket.ExecutionId), 2);
				Stage = 2;
			}
			if (Elapsed < 1.15) { return false; }
			Test.TestEqual(TEXT("The replacement or rearmed action still delivers exactly three contacts"), F->Target->DamageCalls, 3);
			Test.TestEqual(TEXT("No replayed callback leaves an execution after completion"), Access::Executions(*F->Combat), 0);
			Test.TestFalse(TEXT("No replayed callback resurrects the cancellation monitor"), Access::HasMonitor(*F->Combat));
			return true;
		}

		if (Case == EOwnershipCase::RecoveryExpiresPendingContacts)
		{
			auto& Timers = F->World->GetTimerManager();
			if (Stage == 0 && Elapsed >= 0.55)
			{
				Test.TestEqual(TEXT("Two contacts are the positive control before withholding the final contact"), F->Target->DamageCalls, 2);
				CurrentTicket = Access::Capture(*F->Combat);
				Test.TestEqual(TEXT("The withheld valid arm owns the final contact"), CurrentTicket.ExpectedContact, 3);
				Access::PauseContact(*F->Combat, CurrentTicket);
				Test.TestTrue(TEXT("The final contact timer is paused before its authored deadline"), Timers.IsTimerPaused(CurrentTicket.TimerHandle));
				Stage = 1;
			}
			if (Stage == 1 && Elapsed >= 1.05)
			{
				Test.TestEqual(TEXT("The withheld final contact causes no damage before recovery expires"), F->Target->DamageCalls, 2);
				Test.TestEqual(TEXT("A pending final contact still owns the action before expiration"), Access::Executions(*F->Combat), 1);
				Test.TestEqual(TEXT("The pending final contact remains unconsumed"), Access::Completed(*F->Combat, CurrentTicket.ExecutionId), 2);
				Test.TestTrue(TEXT("The pending action retains its cancellation monitor"), Access::HasMonitor(*F->Combat));
				Test.TestTrue(TEXT("The pending action retains its paused timer"), Timers.IsTimerPaused(CurrentTicket.TimerHandle));
				Test.TestTrue(TEXT("The pending action owns the weapon-change gate before expiration"), F->Combat->IsWeaponChangeBlocked());
				Test.TestFalse(TEXT("The pending action blocks an otherwise-ready next action before expiration"), Access::Execute(*F->Combat, *F->ProbeSkill));
				Stage = 2;
			}
			if (Stage == 2 && Elapsed >= 1.34)
			{
				Test.TestEqual(TEXT("Hard recovery expiration removes an execution with unfinished contacts"), Access::Executions(*F->Combat), 0);
				Test.TestFalse(TEXT("Hard recovery expiration removes the paused contact timer"), Timers.TimerExists(CurrentTicket.TimerHandle));
				Test.TestFalse(TEXT("Hard recovery expiration removes the cancellation monitor"), Access::HasMonitor(*F->Combat));
				Test.TestFalse(TEXT("Hard recovery expiration releases its action-owned weapon-change gate"), F->Combat->IsWeaponChangeBlocked());
				Access::Replay(*F->Combat, CurrentTicket);
				Test.TestEqual(TEXT("Replaying the formerly valid final arm after expiration causes no damage"), F->Target->DamageCalls, 2);
				Test.TestEqual(TEXT("A formerly valid arm cannot recreate an expired execution"), Access::Executions(*F->Combat), 0);
				Test.TestFalse(TEXT("A formerly valid arm cannot recreate an expired timer"), Timers.TimerExists(CurrentTicket.TimerHandle));
				Test.TestFalse(TEXT("A formerly valid arm cannot recreate the cancellation monitor"), Access::HasMonitor(*F->Combat));
				const int32 Uses = F->State->GetSkillUseCount(F->ProbeSkill->GetFName());
				Test.TestTrue(TEXT("A zero-cost next action positively accepts after hard expiration"), Access::Execute(*F->Combat, *F->ProbeSkill));
				Test.TestEqual(TEXT("The next action records one accepted use after hard expiration"), F->State->GetSkillUseCount(F->ProbeSkill->GetFName()), Uses + 1);
				Stage = 3;
			}
			if (Elapsed < 1.45) { return false; }
			Test.TestEqual(TEXT("The hard-expiration scenario reaches its final replay check"), Stage, 3);
			Test.TestEqual(TEXT("The expired pending contact never produces residual damage"), F->Target->DamageCalls, 2);
			Test.TestEqual(TEXT("No action is revived after the expiration checks"), Access::Executions(*F->Combat), 0);
			Test.TestFalse(TEXT("No monitor survives the expiration checks"), Access::HasMonitor(*F->Combat));
			return true;
		}

		const double ReleaseAt = Case == EOwnershipCase::ShortRecovery ? 1.10 : 1.30;
		if (Stage == 0 && Elapsed >= 1.02)
		{
			Test.TestEqual(TEXT("All contacts finish before the recovery gate probe"), F->Target->DamageCalls, 3);
			Test.TestEqual(TEXT("Recovery retains the authored execution after its final contact"), Access::Executions(*F->Combat), 1);
			Test.TestTrue(TEXT("Recovery retains cancellation monitoring"), Access::HasMonitor(*F->Combat));
			const int32 Uses = F->State->GetSkillUseCount(F->ProbeSkill->GetFName());
			const float Stamina = F->Attributes->GetStamina();
			Test.TestFalse(TEXT("Actual next-action acceptance is blocked during recovery without a cooldown or resource restriction"),
				Access::Execute(*F->Combat, *F->ProbeSkill));
			Test.TestEqual(TEXT("The recovery gate records no rejected probe use"), F->State->GetSkillUseCount(F->ProbeSkill->GetFName()), Uses);
			Test.TestEqual(TEXT("The recovery gate spends no resource"), F->Attributes->GetStamina(), Stamina);
			Test.TestFalse(TEXT("The recovery gate is independent of the shared attack cooldown"),
				F->Combat->IsAttackOnCooldown(F->World->GetTimeSeconds()));
			if (Case == EOwnershipCase::RecoveryGate)
			{
				// Editor tuning applies to future actions; an accepted deadline remains fixed.
				F->Skill->AuthoredRecoveryEndTime = 1.02f;
			}
			if (Case == EOwnershipCase::RecoveryHit)
			{
				Test.TestTrue(TEXT("A real owner hit during recovery applies positive damage"), F->HitOwner() > 0.0f);
				Test.TestEqual(TEXT("A real owner hit releases recovery immediately"), Access::Executions(*F->Combat), 0);
				Test.TestFalse(TEXT("A real owner hit clears the recovery monitor immediately"), Access::HasMonitor(*F->Combat));
			}
			if (Case == EOwnershipCase::RecoveryStop) { F->Playback->Accepted->Stop(FAlphaBlend(0.08f)); }
			Stage = 1;
		}
		if (Case == EOwnershipCase::RecoveryGate && !bCheckedSnapshot && Elapsed >= 1.15)
		{
			bCheckedSnapshot = true;
			Test.TestEqual(TEXT("Editing recovery after acceptance preserves the original release deadline"), Access::Executions(*F->Combat), 1);
			Test.TestFalse(TEXT("Editing recovery cannot admit a next action before the captured deadline"), Access::Execute(*F->Combat, *F->ProbeSkill));
		}
		const bool bCancelledRecovery = Case == EOwnershipCase::RecoveryHit || Case == EOwnershipCase::RecoveryStop;
		const double ProbeAt = bCancelledRecovery ? 1.07 : ReleaseAt + 0.04;
		if (Stage == 1 && Elapsed >= ProbeAt)
		{
			Test.TestEqual(TEXT("Recovery completion or cancellation removes the action owner"), Access::Executions(*F->Combat), 0);
			Test.TestFalse(TEXT("Recovery completion or cancellation clears the monitor"), Access::HasMonitor(*F->Combat));
			const int32 Uses = F->State->GetSkillUseCount(F->ProbeSkill->GetFName());
			Test.TestTrue(TEXT("Actual zero-cost legacy next action accepts after recovery releases"), Access::Execute(*F->Combat, *F->ProbeSkill));
			Test.TestEqual(TEXT("The accepted next action records one use"), F->State->GetSkillUseCount(F->ProbeSkill->GetFName()), Uses + 1);
			Stage = 2;
		}
		if (Elapsed < 1.45) { return false; }
		Test.TestEqual(TEXT("Recovery and its cancellation create no extra authored contacts"), F->Target->DamageCalls, 3);
		Test.TestEqual(TEXT("The released action cannot revive after its former recovery deadline"), Access::Executions(*F->Combat), 0);
		Test.TestFalse(TEXT("No recovery monitor survives the ownership checks"), Access::HasMonitor(*F->Combat));
		return true;
	}

private:
	void AssertReplayIgnored(const Access::FContactTicket& StaleTicket, int32 Hits, int32 Completed)
	{
		auto& Timers = F->World->GetTimerManager();
		const float Remaining = Timers.GetTimerRemaining(CurrentTicket.TimerHandle);
		Test.TestTrue(TEXT("The current timer is deliberately withheld past its authored deadline"), Timers.IsTimerPaused(CurrentTicket.TimerHandle));
		Access::Replay(*F->Combat, StaleTicket);
		const auto After = Access::Capture(*F->Combat);
		Test.TestEqual(TEXT("Stale replay applies no contact damage"), F->Target->DamageCalls, Hits);
		Test.TestEqual(TEXT("Stale replay leaves the current execution owned"), After.ExecutionId, CurrentTicket.ExecutionId);
		Test.TestEqual(TEXT("Stale replay leaves completed contacts unchanged"), Access::Completed(*F->Combat, CurrentTicket.ExecutionId), Completed);
		Test.TestEqual(TEXT("Stale replay cannot consume or replace the current arm generation"), After.ArmGeneration, CurrentTicket.ArmGeneration);
		Test.TestEqual(TEXT("Stale replay leaves the expected contact ordinal unchanged"), After.ExpectedContact, CurrentTicket.ExpectedContact);
		Test.TestTrue(TEXT("Stale replay preserves the current timer identity"), After.TimerHandle == CurrentTicket.TimerHandle);
		Test.TestTrue(TEXT("Stale replay leaves the current timer paused"), Timers.IsTimerPaused(CurrentTicket.TimerHandle));
		Test.TestEqual(TEXT("Stale replay does not rearm the current timer"), Timers.GetTimerRemaining(CurrentTicket.TimerHandle), Remaining);
	}

	FAutomationTestBase& Test;
	TSharedRef<FFixture> F;
	EOwnershipCase Case;
	Access::FContactTicket OldTicket;
	Access::FContactTicket CurrentTicket;
	uint64 LastEngineFrame;
	double StartedRealTime;
	int32 Stage = 0;
	bool bCheckedSnapshot = false;
};

static bool EnqueueOwnership(FAutomationTestBase& Test, EOwnershipCase Case)
{
	TSharedPtr<FFixture> F = MakeShared<FFixture>(Test);
	if (!Test.TestTrue(TEXT("Native ownership fixture is ready"), F->bReady)) { return false; }
	const bool bReplay = Case == EOwnershipCase::StaleArmRearm || Case == EOwnershipCase::CancelRestart;
	F->Skill->AttackCooldown = 0.0f;
	F->Skill->AuthoredRecoveryEndTime = bReplay ? 0.0f : Case == EOwnershipCase::ShortRecovery ? 1.10f : 1.30f;
	F->ProbeSkill.Reset(NewObject<URogue10mAttackSkillData>(F->Combat));
	F->ProbeSkill->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::Common;
	F->ProbeSkill->HitMode = ERogue10mAttackHitMode::Single;
	F->ProbeSkill->HitCount = 1;
	F->ProbeSkill->bEnableCombo = false;
	F->ProbeSkill->AttackMontage = nullptr;
	F->ProbeSkill->AttackCooldown = 0.0f;
	F->ProbeSkill->ResourceCosts.Reset();
	F->ProbeSkill->AttackShape = ERogue10mAttackShape::LinearBox;
	F->ProbeSkill->AttackRange = F->ProbeSkill->BoxHalfWidth = F->ProbeSkill->BoxHalfHeight = 1.0f;
	F->StartedAt = F->World->GetTimeSeconds();
	Access::Start(*F->Combat, *F->Skill, 1.0f, F->Playback->Anim.Get(), F->Playback->Accepted->GetInstanceID());
	if (!Test.TestEqual(TEXT("The scheduling boundary creates an owned authored action"), Access::Executions(*F->Combat), 1)) { return false; }
	auto OldTicket = Access::Capture(*F->Combat);
	auto CurrentTicket = OldTicket;
	if (Case == EOwnershipCase::StaleArmRearm)
	{
		Access::Rearm(*F->Combat, OldTicket.ExecutionId);
		CurrentTicket = Access::Capture(*F->Combat);
		Test.TestEqual(TEXT("Rearming retains the same execution"), CurrentTicket.ExecutionId, OldTicket.ExecutionId);
		Test.TestEqual(TEXT("Rearming retains the same expected first contact"), CurrentTicket.ExpectedContact, OldTicket.ExpectedContact);
		if (!Test.TestTrue(TEXT("Rearming invalidates the previous callback generation"), CurrentTicket.ArmGeneration != OldTicket.ArmGeneration)) { return false; }
	}
	if (Case == EOwnershipCase::CancelRestart)
	{
		F->Combat->CancelCombatVisuals(false);
		Test.TestEqual(TEXT("Cancellation removes the old execution before restart"), Access::Executions(*F->Combat), 0);
		Access::Start(*F->Combat, *F->Skill, 1.0f, F->Playback->Anim.Get(), F->Playback->Accepted->GetInstanceID());
		CurrentTicket = Access::Capture(*F->Combat);
		if (!Test.TestTrue(TEXT("Restart creates a distinct nonzero execution ID"), CurrentTicket.ExecutionId != 0 && CurrentTicket.ExecutionId != OldTicket.ExecutionId)) { return false; }
	}
	if (bReplay) { Access::PauseContact(*F->Combat, CurrentTicket); }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRunOwnership>(Test, F.ToSharedRef(), Case, OldTicket, CurrentTicket));
	return true;
}

static bool RejectMalformed(FAutomationTestBase& Test)
{
	FFixture F(Test);
	if (!Test.TestTrue(TEXT("Native cost validation fixture is ready"), F.bReady)) { return false; }
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	const TArray<TArray<float>> InvalidTimes{
		{-0.10f, 0.5f, 0.9f}, {NaN, 0.5f, 0.9f}, {0.2f, Infinity, 0.9f},
		{0.0f, 0.0f, 0.9f}, {0.5f, 0.2f, 0.9f}, {0.2f, 0.5f},
		{0.2f, 0.5f, 2.0f}, {0.2f, 0.5f, 2.1f}
	};
	const int32 UsesBefore = F.State->GetSkillUseCount(F.Skill->GetFName());
	for (int32 Index = 0; Index < InvalidTimes.Num(); ++Index)
	{
		F.Skill->AuthoredContactTimes = InvalidTimes[Index];
		Test.TestFalse(*FString::Printf(TEXT("Malformed authored schedule %d is rejected by actual acceptance"), Index), Access::Execute(*F.Combat, *F.Skill));
		Test.TestEqual(TEXT("Rejected authored data pays no stamina"), F.Attributes->GetStamina(), 100.0f);
		Test.TestEqual(TEXT("Rejected authored data records no use"), F.State->GetSkillUseCount(F.Skill->GetFName()), UsesBefore);
		Test.TestEqual(TEXT("Rejected authored data schedules no execution"), Access::Executions(*F.Combat), 0);
		Test.TestEqual(TEXT("Rejected authored data causes no damage"), F.Target->DamageCalls, 0);
		Test.TestFalse(TEXT("Rejected authored data starts no cooldown"), F.Combat->IsAttackOnCooldown(F.World->GetTimeSeconds()));
	}
	// Keep offsets short enough that a broken legacy clamp to rate .01 would
	// still fit the 30-second bound. These cases must reject the raw asset rate.
	F.Skill->AuthoredContactTimes = {0.005f, 0.10f, 0.20f};
	for (float InvalidRate : {0.0f, -1.0f, NaN, Infinity})
	{
		F.Skill->AnimationPlayRate = InvalidRate;
		Test.TestFalse(TEXT("Nonpositive or nonfinite authored animation rate is rejected before legacy clamping"), Access::Execute(*F.Combat, *F.Skill));
		Test.TestEqual(TEXT("Rejected authored rate pays no stamina"), F.Attributes->GetStamina(), 100.0f);
		Test.TestEqual(TEXT("Rejected authored rate records no use"), F.State->GetSkillUseCount(F.Skill->GetFName()), UsesBefore);
		Test.TestEqual(TEXT("Rejected authored rate schedules no execution"), Access::Executions(*F.Combat), 0);
		Test.TestEqual(TEXT("Rejected authored rate causes no damage"), F.Target->DamageCalls, 0);
		Test.TestFalse(TEXT("Rejected authored rate starts no cooldown"), F.Combat->IsAttackOnCooldown(F.World->GetTimeSeconds()));
	}
	F.Skill->AnimationPlayRate = 1.0f;
	F.Skill->AuthoredContactTimes = {0.20f, 0.50f, 0.95f};
	const auto RejectWithoutAcceptance = [&Test, &F, UsesBefore](const TCHAR* Reason)
	{
		Test.TestFalse(Reason, Access::Execute(*F.Combat, *F.Skill));
		Test.TestEqual(TEXT("Rejected timing or playback data pays no stamina"), F.Attributes->GetStamina(), 100.0f);
		Test.TestEqual(TEXT("Rejected timing or playback data records no use"), F.State->GetSkillUseCount(F.Skill->GetFName()), UsesBefore);
		Test.TestEqual(TEXT("Rejected timing or playback data schedules no execution"), Access::Executions(*F.Combat), 0);
		Test.TestEqual(TEXT("Rejected timing or playback data causes no damage"), F.Target->DamageCalls, 0);
		Test.TestFalse(TEXT("Rejected timing or playback data starts no cooldown"), F.Combat->IsAttackOnCooldown(F.World->GetTimeSeconds()));
	};
	for (float InvalidRecovery : {-0.10f, NaN, Infinity, 0.90f, 2.10f})
	{
		F.Skill->AuthoredRecoveryEndTime = InvalidRecovery;
		RejectWithoutAcceptance(TEXT("Negative, nonfinite, pre-contact or out-of-montage recovery is rejected before payment"));
	}
	F.Playback->Montage->SetCompositeLength(100.0f);
	F.Skill->AuthoredRecoveryEndTime = 31.0f;
	RejectWithoutAcceptance(TEXT("Recovery beyond the scaled 30-second bound is rejected even when all contacts fit"));
	F.Playback->Montage->SetCompositeLength(2.0f);
	F.Skill->AuthoredRecoveryEndTime = 0.0f;
	for (float InvalidScale : {0.0f, -1.0f, NaN, Infinity})
	{
		F.Playback->Montage->RateScale = InvalidScale;
		RejectWithoutAcceptance(TEXT("Nonpositive or nonfinite montage RateScale is rejected before payment"));
	}
	F.Playback->Montage->RateScale = 1.0f;
	F.Playback->Montage->bEnableAutoBlendOut = true;
	for (float UnsupportedTrigger : {-1.0f, 0.10f, NaN, Infinity})
	{
		F.Playback->Montage->BlendOutTriggerTime = UnsupportedTrigger;
		RejectWithoutAcceptance(TEXT("Unsupported early or nonfinite automatic blend-out is rejected before payment"));
	}
	return true;
}

static bool CheckEditableTimingProperties(FAutomationTestBase& Test)
{
	const auto Check = [&Test](FProperty* Property, const TCHAR* Label)
	{
		if (!Test.TestNotNull(Label, Property)) { return; }
		Test.TestTrue(TEXT("Timing tuning is exposed to the editor"), Property->HasAnyPropertyFlags(CPF_Edit));
		Test.TestTrue(TEXT("Timing tuning is Blueprint-readable"), Property->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly));
		Test.TestFalse(TEXT("Timing tuning can be edited on defaults and instances"),
			Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate | CPF_EditConst));
	};
	Check(FindFProperty<FFloatProperty>(URogue10mAttackSkillData::StaticClass(), TEXT("AuthoredRecoveryEndTime")),
		TEXT("Authored recovery release is a reflected float tuning property"));
	Check(FindFProperty<FArrayProperty>(URogue10mAttackSkillData::StaticClass(), TEXT("AuthoredContactTimes")),
		TEXT("Authored contact times are a reflected array tuning property"));
	return true;
}
}

#define ROGUE_AUTHORED_SEQUENCE_TEST(ClassName, TestSuffix, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Rogue10m.AuthoredContactSequence." TestSuffix, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) { return Rogue10mAuthoredContactSequenceTests::Enqueue(*this, Rogue10mAuthoredContactSequenceTests::ECase::Scenario); }

ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredNonUniformTest, "NonUniformThreeContacts", NonUniform)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredSpeedSnapshotTest, "AcceptedSpeedSnapshot", SpeedSnapshot)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredFixedAimTest, "AcceptedAimSnapshot", FixedAim)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredZeroContactTest, "ZeroFirstContact", ZeroFirstContact)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredHitBeforeTest, "HitBeforeFirstContact", HitBeforeFirst)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredHitBetweenTest, "HitBetweenContacts", HitAfterFirst)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredMontageStopTest, "MontageStopBetweenContacts", MontageStop)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredSameAssetTest, "SameAssetReplacement", SameAssetReplacement)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredReflectionTest, "ReflectedHitPreservesCredit", ReflectedHit)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredLegacyTest, "EmptyArrayPreservesLegacy", EmptyLegacy)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredMontageScaleTest, "MontageRateScaleIsPartOfEffectiveRate", MontageRateScale)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredInstanceRateChangeTest, "InstanceRateMutationCancels", InstanceRateChange)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredAssetRateChangeTest, "MontageRateScaleMutationCancels", AssetRateChange)
ROGUE_AUTHORED_SEQUENCE_TEST(FRogue10mAuthoredNoAutoBlendOutTest, "DisabledAutomaticBlendOutIsSupported", AutoBlendOutDisabled)

#undef ROGUE_AUTHORED_SEQUENCE_TEST

#define ROGUE_AUTHORED_OWNERSHIP_TEST(ClassName, TestSuffix, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Rogue10m.AuthoredContactSequence." TestSuffix, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) { return Rogue10mAuthoredContactSequenceTests::EnqueueOwnership(*this, Rogue10mAuthoredContactSequenceTests::EOwnershipCase::Scenario); }

ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredStaleArmTest, "StaleArmCannotConsumeRearmedContacts", StaleArmRearm)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredCancelRestartTest, "CancelledExecutionCannotConsumeRestartedContacts", CancelRestart)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredRecoveryGateTest, "RecoveryOwnsInputUntilCapturedDeadline", RecoveryGate)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredShortRecoveryTest, "RecoveryTuningChangesReleaseDeadline", ShortRecovery)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredRecoveryHitTest, "HitDuringRecoveryReleasesOwnership", RecoveryHit)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredRecoveryStopTest, "StoppedRecoveryReleasesOwnership", RecoveryStop)
ROGUE_AUTHORED_OWNERSHIP_TEST(FRogue10mAuthoredRecoveryExpiryTest, "RecoveryExpiresPendingContacts", RecoveryExpiresPendingContacts)

#undef ROGUE_AUTHORED_OWNERSHIP_TEST

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAuthoredMalformedTest,
	"Rogue10m.AuthoredContactSequence.MalformedDataRejectsBeforePayment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAuthoredMalformedTest::RunTest(const FString&)
{
	return Rogue10mAuthoredContactSequenceTests::RejectMalformed(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAuthoredTimingEditableTest,
	"Rogue10m.AuthoredContactSequence.RecoveryTimingIsEditable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAuthoredTimingEditableTest::RunTest(const FString&)
{
	return Rogue10mAuthoredContactSequenceTests::CheckEditableTimingProperties(*this);
}

#endif
