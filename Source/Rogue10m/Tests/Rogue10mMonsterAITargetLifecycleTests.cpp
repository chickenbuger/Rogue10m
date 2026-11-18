// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/Rogue10mMonsterAIController.h"
#include "AITypes.h"
#include "AbilitySystemComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"

namespace Rogue10mMonsterAITargetLifecycleTests
{
/** Uses the concrete HumanMale Blueprint; no GameInstance, BeginPlay, world ticks, or save slots. */
class FFixture
{
public:
	explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}
	~FFixture()
	{
		if (IsValid(AI)) AI->StopMonsterAI();
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) GEngine->DestroyWorldContext(World);
		}
	}

	bool Initialize(bool bSpawnTarget = true)
	{
		if (!Test.TestNotNull(TEXT("Engine available for the isolated world context"), GEngine)) return false;
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(true).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!Test.TestNotNull(TEXT("Isolated synthetic game world"), World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		if (!Test.TestTrue(TEXT("Real AI system exists without a GameInstance"),
			World->GetAISystem() != nullptr && World->GetGameInstance() == nullptr && !World->HasBegunPlay())) return false;

		const FTransform MonsterTransform(FVector(0.0f, 0.0f, 100.0f));
		Monster = World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),
			MonsterTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Test.TestNotNull(TEXT("Actual native monster pawn"), Monster)) return false;
		Monster->AutoPossessAI = EAutoPossessAI::Disabled;
		Monster->MarkCombatTestInstance();
		Monster->FinishSpawning(MonsterTransform);
		Pattern = Monster->GetAttackPatternComponent();
		if (!Test.TestNotNull(TEXT("Production attack pattern component"), Pattern)) return false;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		AI = World->SpawnActor<ARogue10mMonsterAIController>(ARogue10mMonsterAIController::StaticClass(),
			FTransform::Identity, SpawnParameters);
		if (!Test.TestNotNull(TEXT("Actual monster AI controller"), AI)) return false;
		AI->Possess(Monster);
		Blackboard = AI->GetBlackboardComponent();
		Tree = Cast<UBehaviorTreeComponent>(AI->GetBrainComponent());
		if (!Test.TestTrue(TEXT("Possession starts the production runtime behavior tree"),
			AI->GetPawn() == Monster && AI->IsMonsterAIEnabled() && Tree && Tree->IsRunning())) return false;
		if (!Test.TestTrue(TEXT("Runtime tree uses the initialized production blackboard"),
			Blackboard && Blackboard->GetBlackboardAsset() && Tree->GetRootTree()
			&& Tree->GetRootTree()->BlackboardAsset == Blackboard->GetBlackboardAsset())) return false;
		if (!Test.TestTrue(TEXT("Production target and state keys exist"),
			Blackboard->GetKeyID(ARogue10mMonsterAIController::TargetActorKey) != FBlackboard::InvalidKey
			&& Blackboard->GetKeyID(ARogue10mMonsterAIController::HasTargetKey) != FBlackboard::InvalidKey
			&& Blackboard->GetKeyID(ARogue10mMonsterAIController::IsInAttackRangeKey) != FBlackboard::InvalidKey
			&& Blackboard->GetKeyID(ARogue10mMonsterAIController::WasDamagedKey) != FBlackboard::InvalidKey)) return false;

		if (bSpawnTarget)
		{
			const FTransform TargetTransform(FVector(100.0f, 0.0f, 100.0f));
			UClass* TargetClass = LoadClass<ARogue10mCharacter>(nullptr,
				TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
			if (!Test.TestNotNull(TEXT("Concrete HumanMale target class"), TargetClass)
				|| !Test.TestFalse(TEXT("Target class is not abstract"), TargetClass->HasAnyClassFlags(CLASS_Abstract))) return false;
			Target = World->SpawnActorDeferred<ARogue10mCharacter>(TargetClass,
				TargetTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Test.TestNotNull(TEXT("Actual concrete HumanMale character target"), Target)) return false;
			Target->AutoPossessAI = EAutoPossessAI::Disabled;
			Target->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Target->FinishSpawning(TargetTransform);
			// Match existing isolated combat fixtures: register attributes and ASC owner/avatar
			// explicitly because neither possession nor PlayerState BeginPlay runs here.
			auto* State = World->SpawnActor<ARogue10mPlayerState>();
			if (!Test.TestNotNull(TEXT("Target PlayerState"), State)) return false;
			Target->SetPlayerState(State);
			auto* ASC = State->GetAbilitySystemComponent();
			auto* Attributes = State->GetRogueAttributeSet();
			if (!Test.TestNotNull(TEXT("Target ASC"), ASC)
				|| !Test.TestNotNull(TEXT("Target AttributeSet"), Attributes)) return false;
			ASC->AddAttributeSetSubobject(Attributes);
			ASC->InitAbilityActorInfo(State, Target);
			if (!Test.TestTrue(TEXT("Target ASC attributes and owner/avatar are initialized"),
				ASC->GetSet<URogue10mAttributeSet>() == Attributes
				&& ASC->GetOwnerActor() == State && ASC->GetAvatarActor() == Target)) return false;
			if (!Test.TestFalse(TEXT("Target is alive without a GameInstance or BeginPlay"), Target->IsDead())) return false;
		}
		return true;
	}

	bool StartBasicWindup()
	{
		auto* Profile = NewObject<URogue10mMonsterAttackPatternProfile>(Monster, NAME_None, RF_Transient);
		Profile->AttackRange = 220.0f;
		Profile->AttackWindupSeconds = 5.0f;
		Profile->bEnableBossPatterns = false;
		Monster->ConfigureAttackPatternProfile(Profile);
		AI->NotifyDamageStimulus(Target, 1.0f);
		if (!Test.TestTrue(TEXT("Damage stimulus acquires a real target and sets its blackboard state"),
			AI->GetCombatTarget() == Target
			&& Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey)
			&& Blackboard->GetValueAsBool(ARogue10mMonsterAIController::WasDamagedKey))) return false;
		if (!Test.TestTrue(TEXT("A real combat decision starts an asset-free basic windup"),
			AI->ExecuteBehaviorDecision() && Pattern->GetPatternState() == ERogue10mMonsterPatternState::Windup
			&& Blackboard->GetValueAsBool(ARogue10mMonsterAIController::IsInAttackRangeKey))) return false;
		return true;
	}

	void CheckClearedState()
	{
		Test.TestTrue(TEXT("Target object key and public combat target are empty"), !AI->GetCombatTarget()
			&& !Blackboard->GetValueAsObject(ARogue10mMonsterAIController::TargetActorKey));
		Test.TestFalse(TEXT("HasTarget is cleared"), Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey));
		Test.TestFalse(TEXT("Attack-range state is cleared"), Blackboard->GetValueAsBool(ARogue10mMonsterAIController::IsInAttackRangeKey));
		Test.TestFalse(TEXT("Damage-stimulus state is cleared"), Blackboard->GetValueAsBool(ARogue10mMonsterAIController::WasDamagedKey));
		Test.TestFalse(TEXT("Pending basic windup is canceled"), Pattern->IsAttackActive());
		Test.TestTrue(TEXT("Pattern returns to Idle"), Pattern->GetPatternState() == ERogue10mMonsterPatternState::Idle);
		Test.TestTrue(TEXT("Gameplay focus is cleared"), AI->GetFocusActor() == nullptr);
	}

	UWorld* World = nullptr;
	ARogue10mBasicMonster* Monster = nullptr;
	ARogue10mCharacter* Target = nullptr;
	ARogue10mMonsterAIController* AI = nullptr;
	UBlackboardComponent* Blackboard = nullptr;
	UBehaviorTreeComponent* Tree = nullptr;
	URogue10mMonsterAttackPatternComponent* Pattern = nullptr;

private:
	FAutomationTestBase& Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAIDestroyedTargetTest,
	"Rogue10m.MonsterAI.TargetLifecycle.DestroyedActorClearsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAIDestroyedTargetTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterAITargetLifecycleTests;
	FFixture Fixture(*this);
	if (!Fixture.Initialize() || !Fixture.StartBasicWindup()) return false;
	const TWeakObjectPtr<ARogue10mCharacter> TargetHandle(Fixture.Target);
	if (!TestTrue(TEXT("Destroy succeeds on the selected actual target actor"), Fixture.Target->Destroy())) return false;
	Fixture.Target = nullptr;
	TestFalse(TEXT("Destroyed actor weak reference no longer resolves"), TargetHandle.IsValid());
	TestTrue(TEXT("Real blackboard weak object key becomes empty without clearing HasTarget"),
		!Fixture.Blackboard->GetValueAsObject(ARogue10mMonsterAIController::TargetActorKey)
		&& Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey));
	TestTrue(TEXT("Windup is still pending before the next decision"), Fixture.Pattern->IsAttackActive());
	TestTrue(TEXT("Decision handles the destroyed target"), Fixture.AI->ExecuteBehaviorDecision());
	Fixture.CheckClearedState();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAIClearedObjectKeyTest,
	"Rogue10m.MonsterAI.TargetLifecycle.ClearedObjectKeyCancelsWindup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAIClearedObjectKeyTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterAITargetLifecycleTests;
	FFixture Fixture(*this);
	if (!Fixture.Initialize() || !Fixture.StartBasicWindup()) return false;
	Fixture.Blackboard->ClearValue(ARogue10mMonsterAIController::TargetActorKey);
	TestTrue(TEXT("External object-key clear leaves a living target and stale HasTarget"),
		IsValid(Fixture.Target) && !Fixture.Target->IsDead() && !Fixture.AI->GetCombatTarget()
		&& Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey));
	TestTrue(TEXT("Basic windup remains pending before the next decision"), Fixture.Pattern->IsAttackActive());
	TestTrue(TEXT("Decision handles the externally cleared object key"), Fixture.AI->ExecuteBehaviorDecision());
	Fixture.CheckClearedState();
	TestTrue(TEXT("No perception tick silently reacquires the still-living target"), IsValid(Fixture.Target) && !Fixture.AI->GetCombatTarget());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAILiveTargetTest,
	"Rogue10m.MonsterAI.TargetLifecycle.LiveTargetRetainsWindup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAILiveTargetTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterAITargetLifecycleTests;
	FFixture Fixture(*this);
	if (!Fixture.Initialize() || !Fixture.StartBasicWindup()) return false;
	const int32 Started = Fixture.Pattern->GetStartedAttackCount();
	for (int32 Decision = 0; Decision < 2; ++Decision)
	{
		TestTrue(TEXT("Decision accepts the living combat target"), Fixture.AI->ExecuteBehaviorDecision());
		TestTrue(TEXT("Living target and state flags remain coherent"), Fixture.AI->GetCombatTarget() == Fixture.Target
			&& Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey)
			&& Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::IsInAttackRangeKey)
			&& Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::WasDamagedKey));
		TestTrue(TEXT("Living target's windup is retained"), Fixture.Pattern->GetPatternState() == ERogue10mMonsterPatternState::Windup);
		TestEqual(TEXT("Repeated decisions do not restart the pending attack"), Fixture.Pattern->GetStartedAttackCount(), Started);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAIStoppedControlTest,
	"Rogue10m.MonsterAI.TargetLifecycle.StoppedAIIgnoresStimulus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAIStoppedControlTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterAITargetLifecycleTests;
	FFixture Fixture(*this);
	if (!Fixture.Initialize() || !Fixture.StartBasicWindup()) return false;
	const int32 Started = Fixture.Pattern->GetStartedAttackCount();
	Fixture.AI->StopMonsterAI();
	TestFalse(TEXT("Explicit stop disables AI"), Fixture.AI->IsMonsterAIEnabled());
	TestFalse(TEXT("Explicit stop stops the actual runtime behavior tree"), Fixture.Tree->IsRunning());
	Fixture.CheckClearedState();
	Fixture.AI->NotifyDamageStimulus(Fixture.Target, 1.0f);
	TestFalse(TEXT("Stopped AI rejects subsequent decisions"), Fixture.AI->ExecuteBehaviorDecision());
	Fixture.CheckClearedState();
	TestEqual(TEXT("Stopped AI never starts another attack"), Fixture.Pattern->GetStartedAttackCount(), Started);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mMonsterAIEmptyPatrolControlTest,
	"Rogue10m.MonsterAI.TargetLifecycle.EmptyTargetPreservesPendingPatrol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mMonsterAIEmptyPatrolControlTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterAITargetLifecycleTests;
	FFixture Fixture(*this);
	if (!Fixture.Initialize(false)) return false;
	Fixture.CheckClearedState();
	UPathFollowingComponent* Following = Fixture.AI->GetPathFollowingComponent();
	if (!TestNotNull(TEXT("Actual path-following component"), Following)) return false;

	// A real accepted request awaiting its path needs no navmesh or world ticks.
	// Clearing an already-empty target unconditionally would abort this patrol request.
	FNavPathSharedPtr PendingPath = MakeShared<FNavigationPath, ESPMode::ThreadSafe>();
	FAIMoveRequest Request(Fixture.Monster->GetActorLocation() + FVector(1000.0f, 0.0f, 0.0f));
	Request.SetAcceptanceRadius(10.0f);
	Request.SetUsePathfinding(false);
	Request.SetProjectGoalLocation(false);
	const FAIRequestID RequestId = Fixture.AI->RequestMove(Request, PendingPath);
	if (!TestTrue(TEXT("Real direct request is accepted while its path is pending"), RequestId.IsValid()
		&& Following->GetStatus() == EPathFollowingStatus::Waiting && Following->GetPath().Get() == PendingPath.Get())) return false;
	for (int32 Decision = 0; Decision < 2; ++Decision)
	{
		TestTrue(TEXT("Empty-target patrol decision succeeds"), Fixture.AI->ExecuteBehaviorDecision());
		TestTrue(TEXT("Empty-target decisions preserve the pending patrol request identity and path"),
			Following->GetStatus() == EPathFollowingStatus::Waiting && Following->GetCurrentRequestId() == RequestId
			&& Following->GetPath().Get() == PendingPath.Get());
		TestFalse(TEXT("Empty patrol never gains a target"), Fixture.Blackboard->GetValueAsBool(ARogue10mMonsterAIController::HasTargetKey));
		TestTrue(TEXT("Empty patrol public combat target stays empty"), Fixture.AI->GetCombatTarget() == nullptr);
	}
	return !HasAnyErrors();
}

#endif
