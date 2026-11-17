#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mHitInterruptionTestTarget.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitRuntimeComponent.h"

// Native engine automation: these tests are authored for integration execution.
// They have not been run by the isolated authoring worker, and load no content.
struct FRogue10mHitInterruptionTestAccess
{
	static void Configure(URogue10mCombatComponent& Combat)
	{
		Combat.CharacterData = nullptr;
		Combat.bAllowAttackCombo = true;
	}
	static bool Execute(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{
		return Combat.ExecuteAttackSkill(Skill, false);
	}
	static int32 Executions(const URogue10mCombatComponent& Combat) { return Combat.ActiveAttackExecutions.Num(); }
	static float CooldownStart(const URogue10mCombatComponent& Combat) { return Combat.AttackCooldownStartTime; }
	static float CooldownEnd(const URogue10mCombatComponent& Combat) { return Combat.AttackCooldownEndTime; }
	static uint64 DamageEpoch(const URogue10mCombatComponent& Combat) { return Combat.DamageInterruptionEpoch; }
	static uint32 SeedContact(URogue10mCombatComponent& Combat, URogue10mAttackSkillData& Skill)
	{
		const uint32 Id = Combat.NextAttackExecutionId++;
		auto& Execution = Combat.ActiveAttackExecutions.Add(Id);
		Execution.ExecutionId = Id;
		Execution.SkillData = &Skill;
		Execution.MasteryTrack = ERogue10mSkillTreeTrack::GreatSword;
		return Id;
	}
	static void ResolveContact(URogue10mCombatComponent& Combat, uint32 Id) { Combat.ExecuteAttackHitPulse(Id); }
};

namespace Rogue10mHitInterruptionTests
{
using Access = FRogue10mHitInterruptionTestAccess;

struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mCatalogTestCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	APlayerController* Controller = nullptr;
	URogue10mCombatComponent* Combat = nullptr;
	URogue10mAttributeSet* Attributes = nullptr;
	UAbilitySystemComponent* ASC = nullptr;
	bool bDamageGameModeReady = false;

	explicit FFixture(FAutomationTestBase& Test)
	{
		if (!GEngine) { Test.AddError(TEXT("An initialized engine is required")); return; }
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!World) { Test.AddError(TEXT("Native physics world creation failed")); return; }
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		// APawn::ShouldTakeDamage rejects worlds without an authority GameMode.
		// This native fixture does not initialize production menus, profiles or saves.
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		bDamageGameModeReady = World->SetGameMode(URL);
		if (!bDamageGameModeReady) { Test.AddError(TEXT("Native authority damage GameMode creation failed")); return; }
		// AActor::ProcessEvent rejects interface dispatch until actors are initialized.
		// Match the normal lifecycle without BeginPlay or advancing gameplay time.
		World->InitializeActorsForPlay(URL);
		if (!World->AreActorsInitialized() || World->HasBegunPlay())
		{ Test.AddError(TEXT("Fixture actor script dispatch must be initialized without BeginPlay")); return; }
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
		if (!Combat || !ASC || !Attributes) { Test.AddError(TEXT("Native combat/ASC/attributes fixture failed")); return; }
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
	}
	~FFixture()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	}
	bool IsValid() const { return World && World->AreActorsInitialized() && bDamageGameModeReady && Character && State && Controller && Combat && ASC && Attributes; }
	float HitOwner(float Damage = 10.0f)
	{
		const FDamageEvent Event;
		return Character->TakeDamage(Damage, Event, nullptr, nullptr);
	}
	URogue10mAttackSkillData* MakeSkill()
	{
		auto* Skill = NewObject<URogue10mAttackSkillData>(Combat);
		Skill->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::Common;
		Skill->bAdvanceBeforeHit = false;
		Skill->bEnableCombo = false;
		Skill->NextComboSkill = nullptr;
		Skill->HitMode = ERogue10mAttackHitMode::Single;
		Skill->HitCount = 1;
		Skill->Damage = 10.0f;
		Skill->AttackCooldown = 1.5f;
		Skill->HitStartDelaySeconds = 0.1f;
		Skill->AttackShape = ERogue10mAttackShape::LinearBox;
		Skill->AttackRange = 400.0f;
		Skill->BoxHalfWidth = 50.0f;
		Skill->BoxHalfHeight = 50.0f;
		Skill->MaxTargetsPerHit = 2;
		return Skill;
	}
	ARogue10mHitInterruptionTestTarget* TargetAt(float Distance)
	{
		const FTransform View = Character->GetCombatViewTransform();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARogue10mHitInterruptionTestTarget>(
			View.GetLocation() + View.GetRotation().GetForwardVector() * Distance, FRotator::ZeroRotator, Spawn);
	}
	bool PrepareContactQuery(FAutomationTestBase& Test, const URogue10mAttackSkillData& Skill,
		const TArray<ARogue10mHitInterruptionTestTarget*>& Targets)
	{
		auto* Scene = World->GetPhysicsScene();
		if (!Test.TestNotNull(TEXT("Contact fixture has a native physics scene"), Scene)) { return false; }
		// This world never ticks. Finish queued body creation and publish Chaos query data
		// explicitly instead of resolving contact against unprocessed spawn state.
		// Flush advances solver commands by zero time; no gameplay BeginPlay/tick is needed.
		Scene->ProcessAsyncPhysicsStateJobs(true);
		Scene->ProcessDeferredCreatePhysicsState();
		Scene->Flush();
		const FTransform View = Character->GetCombatViewTransform();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mHitInterruptionFixture), false, Character);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps,
			View.GetLocation() + View.GetRotation().GetForwardVector() * (Skill.AttackRange * 0.5f),
			View.GetRotation(), Objects,
			FCollisionShape::MakeBox(FVector(Skill.AttackRange * 0.5f, Skill.BoxHalfWidth, Skill.BoxHalfHeight)), Query);
		bool bReady = true;
		for (auto* Target : Targets)
		{
			const auto* Collision = Target ? Cast<UPrimitiveComponent>(Target->GetRootComponent()) : nullptr;
			bReady &= Test.TestTrue(TEXT("Contact target owns registered query-enabled physics"),
				Collision && Collision->IsRegistered() && Collision->IsPhysicsStateCreated() && Collision->IsQueryCollisionEnabled());
			bReady &= Test.TestTrue(TEXT("Native attack-volume positive control finds each intended target before callbacks"),
				Overlaps.ContainsByPredicate([Target](const FOverlapResult& Hit) { return Hit.GetActor() == Target; }));
			bReady &= Test.TestEqual(TEXT("Geometry positive control does not invoke target filtering"), Target->FilterCalls, 0);
		}
		return bReady;
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
		Investment.Rank = 1;
		Investment.SpentPoints = 1;
		Investment.Bonus = 0.04f;
		if (!Test.TestTrue(TEXT("Native Combo trait investment restores without assets or a save"), Town->RestoreProgression(Snapshot))) { return false; }
		Traits->ResetRunState();
		return Test.TestTrue(TEXT("The runtime Combo trait has a real positive bonus"), Town->GetTraitBonus(ERogue10mTownTrait::Combo) > 0.0f);
	}
};

static bool RunHitDuringPayment(FAutomationTestBase& Test)
{
	FFixture F(Test);
	if (!Test.TestTrue(TEXT("Native authority damage fixture is ready"), F.IsValid())) { return false; }
	auto* Skill = F.MakeSkill();
	auto* Next = F.MakeSkill();
	Skill->bEnableCombo = true;
	Skill->NextComboSkill = Next;
	Skill->ComboWindowOpenSeconds = 1.0f;
	Skill->ComboWindowCloseSeconds = 2.0f;
	Skill->ResourceCosts = {{ERogue10mAttackResourceType::Stamina, 10.0f}, {ERogue10mAttackResourceType::Mana, 5.0f}};
	const uint64 EpochBefore = Access::DamageEpoch(*F.Combat);
	const int32 UsesBefore = F.State->GetSkillUseCount(Skill->GetFName());
	int32 CostNotifications = 0;
	float AppliedHit = 0.0f;
	bool bReentrantAccepted = false;
	auto& Changed = F.ASC->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetStaminaAttribute());
	const FDelegateHandle Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Data)
	{
		if (Data.NewValue >= Data.OldValue) { return; }
		++CostNotifications;
		if (CostNotifications != 1) { return; } // Broken guards fail assertions, never recurse indefinitely.
		AppliedHit = F.HitOwner();
		bReentrantAccepted = Access::Execute(*F.Combat, *Skill);
	});
	ON_SCOPE_EXIT { Changed.Remove(Handle); };
	Test.TestTrue(TEXT("A fully paid action remains one accepted use after the cost callback takes a hit"), Access::Execute(*F.Combat, *Skill));
	Test.TestTrue(TEXT("The cost callback used the actual finite positive pawn damage path"), FMath::IsFinite(AppliedHit) && AppliedHit > 0.0f);
	Test.TestEqual(TEXT("Actual damage changes health, without granting immunity"), F.Attributes->GetHealth(), 100.0f - AppliedHit);
	Test.TestEqual(TEXT("Exactly one stamina debit broadcasts"), CostNotifications, 1);
	Test.TestEqual(TEXT("Accepted stamina cost is retained"), F.Attributes->GetStamina(), 90.0f);
	Test.TestEqual(TEXT("Accepted mana cost is retained"), F.Attributes->GetMana(), 95.0f);
	Test.TestEqual(TEXT("Accepted use is recorded exactly once"), F.State->GetSkillUseCount(Skill->GetFName()), UsesBefore + 1);
	Test.TestTrue(TEXT("The real hit changes the damage interruption epoch"), Access::DamageEpoch(*F.Combat) > EpochBefore);
	Test.TestFalse(TEXT("Attack reentry from a hit callback is rejected before another payment"), bReentrantAccepted);
	Test.TestEqual(TEXT("Hit-cancelled acceptance schedules no contact execution"), Access::Executions(*F.Combat), 0);
	Test.TestFalse(TEXT("Hit-cancelled acceptance owns no technique movement"), F.Combat->IsTechniqueMovementActive());
	const float Now = static_cast<float>(F.World->GetTimeSeconds());
	Test.TestTrue(TEXT("Hit cancellation activates the shared cooldown immediately"), F.Combat->IsAttackOnCooldown(Now));
	Test.TestTrue(TEXT("A cancelled combo cannot leave a future cooldown-start gap"), Access::CooldownStart(*F.Combat) <= Now);
	Test.TestTrue(TEXT("The reserved combo cooldown deadline is retained"), FMath::IsNearlyEqual(Access::CooldownEnd(*F.Combat), Now + 2.0f + Skill->AttackCooldown));
	Test.TestFalse(TEXT("A hit-cancelled paid action never opens its combo window"), F.Combat->IsComboSequenceActive(Now));
	Test.TestFalse(TEXT("Immediate retry is blocked by the retained cooldown"), Access::Execute(*F.Combat, *Skill));
	Test.TestEqual(TEXT("Rejected retry pays no second cost"), CostNotifications, 1);
	Test.TestEqual(TEXT("Rejected retry records no second use"), F.State->GetSkillUseCount(Skill->GetFName()), UsesBefore + 1);
	return true;
}

static bool RunSecondFilterHit(FAutomationTestBase& Test)
{
	FFixture F(Test);
	if (!Test.TestTrue(TEXT("Native authority damage/query fixture is ready"), F.IsValid())) { return false; }
	auto* Skill = F.MakeSkill();
	auto* Target = F.TargetAt(100.0f);
	if (!Test.TestNotNull(TEXT("Native query target exists"), Target)) { return false; }
	if (!F.PrepareContactQuery(Test, *Skill, {Target})) { return false; }
	Target->TriggerFilterCall = 2; // First filter is Gather; second is immediately before ApplyDamage.
	float AppliedHit = 0.0f;
	Target->FilterCallback = [&]() { AppliedHit = F.HitOwner(); };
	const uint32 Id = Access::SeedContact(*F.Combat, *Skill);
	// Deliberately seed only the pending execution. This isolates actual overlap,
	// Blueprint-capable interface dispatch and damage ownership from acceptance/assets.
	Access::ResolveContact(*F.Combat, Id);
	Test.TestEqual(TEXT("Actual query and pre-damage paths each call the target filter"), Target->FilterCalls, 2);
	Test.TestTrue(TEXT("The second filter applied a real positive hit to the attack owner"), FMath::IsFinite(AppliedHit) && AppliedHit > 0.0f);
	Test.TestEqual(TEXT("A hit in the second target filter prevents the pending outgoing damage"), Target->DamageCalls, 0);
	Test.TestEqual(TEXT("Interrupted contact resolves no outgoing damage amount"), Target->TotalDamage, 0.0f);
	Test.TestEqual(TEXT("The interruption removes the pending execution safely"), Access::Executions(*F.Combat), 0);
	return true;
}

static bool RunReflectedHitCredit(FAutomationTestBase& Test)
{
	FFixture F(Test);
	if (!Test.TestTrue(TEXT("Native authority reflection fixture is ready"), F.IsValid()) || !F.EnableCombo(Test)) { return false; }
	auto* Skill = F.MakeSkill();
	auto* ReentrantSkill = F.MakeSkill();
	auto* First = F.TargetAt(100.0f);
	auto* Second = F.TargetAt(200.0f);
	if (!Test.TestNotNull(TEXT("Near reflecting target exists"), First)
		|| !Test.TestNotNull(TEXT("Far subsequent target exists"), Second)) { return false; }
	if (!F.PrepareContactQuery(Test, *Skill, {First, Second})) { return false; }
	auto* Traits = F.Character->GetTraitRuntimeComponent();
	constexpr uint32 PreviousConfirmedId = 0x7fff;
	Traits->NotifyAttackHit(PreviousConfirmedId, *First);
	if (!Test.TestEqual(TEXT("Positive control starts a real one-hit Combo chain on the same target"), Traits->GetComboHitCount(), 1)) { return false; }
	float ReflectedApplied = 0.0f;
	bool bReentrantAccepted = false;
	int32 ReflectionCalls = 0;
	First->DamageCallback = [&]()
	{
		++ReflectionCalls;
		ReflectedApplied = F.HitOwner();
		bReentrantAccepted = Access::Execute(*F.Combat, *ReentrantSkill);
	};
	const uint32 Id = Access::SeedContact(*F.Combat, *Skill);
	Access::ResolveContact(*F.Combat, Id);
	Test.TestEqual(TEXT("The near target receives the already-committed first damage exactly once"), First->DamageCalls, 1);
	Test.TestTrue(TEXT("The first committed damage remains positive after reflection"), First->TotalDamage > 0.0f);
	Test.TestEqual(TEXT("The target reflects only once"), ReflectionCalls, 1);
	Test.TestTrue(TEXT("Reflection applies finite positive damage to the real attack owner"), FMath::IsFinite(ReflectedApplied) && ReflectedApplied > 0.0f);
	Test.TestEqual(TEXT("The reflected hit changes the owner's health"), F.Attributes->GetHealth(), 100.0f - ReflectedApplied);
	Test.TestFalse(TEXT("A target callback cannot reenter acceptance during pulse resolution"), bReentrantAccepted);
	Test.TestEqual(TEXT("Reflection cancels the still-pending damage on the next target"), Second->DamageCalls, 0);
	Test.TestEqual(TEXT("The far target takes no residual outgoing damage"), Second->TotalDamage, 0.0f);
	Test.TestEqual(TEXT("The cancelled pulse has no residual execution"), Access::Executions(*F.Combat), 0);
	Test.TestEqual(TEXT("Committed hit credit increments the existing Combo chain once without a preceding miss"), Traits->GetComboHitCount(), 2);
	Traits->NotifyAttackHit(Id, *First);
	Test.TestEqual(TEXT("The same committed execution cannot award duplicate Combo hit credit"), Traits->GetComboHitCount(), 2);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHitDuringPaymentTest,
	"Rogue10m.TechniqueMotion.HitInterruption.DuringPaymentCancelsComboAcceptance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHitDuringPaymentTest::RunTest(const FString&)
{
	return Rogue10mHitInterruptionTests::RunHitDuringPayment(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSecondFilterHitTest,
	"Rogue10m.TechniqueMotion.HitInterruption.SecondTargetFilterPreventsDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSecondFilterHitTest::RunTest(const FString&)
{
	return Rogue10mHitInterruptionTests::RunSecondFilterHit(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mReflectedHitCreditTest,
	"Rogue10m.TechniqueMotion.HitInterruption.ReflectedHitPreservesCommittedCredit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mReflectedHitCreditTest::RunTest(const FString&)
{
	return Rogue10mHitInterruptionTests::RunReflectedHitCredit(*this);
}

#endif
