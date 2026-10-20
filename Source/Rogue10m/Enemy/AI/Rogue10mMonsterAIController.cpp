// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mMonsterAIController.h"

#include "AITypes.h"
#include "BTTask_Rogue10mMonsterDecision.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterAttackPatternComponent.h"

const FName ARogue10mMonsterAIController::TargetActorKey(TEXT("TargetActor"));
const FName ARogue10mMonsterAIController::HomeLocationKey(TEXT("HomeLocation"));
const FName ARogue10mMonsterAIController::PatrolLocationKey(TEXT("PatrolLocation"));
const FName ARogue10mMonsterAIController::LastKnownLocationKey(TEXT("LastKnownLocation"));
const FName ARogue10mMonsterAIController::HasTargetKey(TEXT("HasTarget"));
const FName ARogue10mMonsterAIController::IsInAttackRangeKey(TEXT("IsInAttackRange"));
const FName ARogue10mMonsterAIController::WasDamagedKey(TEXT("WasDamaged"));

ARogue10mMonsterAIController::ARogue10mMonsterAIController()
{
	MonsterBlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Monster Blackboard"));
	MonsterBehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("Monster Behavior Tree"));
	BrainComponent = MonsterBehaviorTreeComponent;

	MonsterPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Monster Perception"));
	SetPerceptionComponent(*MonsterPerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	SightConfig->SightRadius = 1800.0f;
	SightConfig->LoseSightRadius = 2200.0f;
	// These creatures have no directional look/scan behavior. Sense nearby
	// players all around, while sight still respects range and world occlusion.
	SightConfig->PeripheralVisionAngleDegrees = 180.0f;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	MonsterPerceptionComponent->ConfigureSense(*SightConfig);
	MonsterPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("Damage Config"));
	DamageConfig->SetMaxAge(8.0f);
	MonsterPerceptionComponent->ConfigureSense(*DamageConfig);
	MonsterPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this, &ThisClass::HandleTargetPerceptionUpdated);
}

void ARogue10mMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InitializeMonsterAI();
}

void ARogue10mMonsterAIController::InitializeMonsterAI()
{
	ARogue10mBasicMonster* Monster = GetMonsterPawn();
	if (!Monster || Monster->IsDead())
	{
		return;
	}

	SightConfig->SightRadius = Monster->GetDetectionRange();
	SightConfig->LoseSightRadius = FMath::Max(
		Monster->GetLoseSightRange(), Monster->GetDetectionRange());
	SightConfig->SetMaxAge(Monster->GetForgetTargetSeconds());
	DamageConfig->SetMaxAge(Monster->GetForgetTargetSeconds());
	MonsterPerceptionComponent->ConfigureSense(*SightConfig);
	MonsterPerceptionComponent->ConfigureSense(*DamageConfig);
	MonsterPerceptionComponent->RequestStimuliListenerUpdate();

	if (MonsterBehaviorTreeComponent && MonsterBehaviorTreeComponent->IsRunning())
	{
		MonsterBehaviorTreeComponent->StopTree(EBTStopMode::Safe);
	}
	BuildRuntimeBlackboard();
	BuildRuntimeBehaviorTree();
	UBehaviorTree* BehaviorTree = Monster->ResolveBehaviorTreeAsset();
	if (!BehaviorTree || !BehaviorTree->RootNode || !BehaviorTree->BlackboardAsset)
	{
		BehaviorTree = RuntimeBehaviorTree;
	}

	UBlackboardComponent* ResolvedBlackboard = MonsterBlackboardComponent;
	if (!BehaviorTree || !BehaviorTree->BlackboardAsset
		|| !UseBlackboard(BehaviorTree->BlackboardAsset, ResolvedBlackboard))
	{
		return;
	}
	MonsterBlackboardComponent = ResolvedBlackboard;
	MonsterBlackboardComponent->SetValueAsVector(HomeLocationKey, Monster->GetActorLocation());
	MonsterBlackboardComponent->SetValueAsVector(PatrolLocationKey, Monster->GetActorLocation());
	MonsterBlackboardComponent->SetValueAsVector(LastKnownLocationKey, Monster->GetActorLocation());
	MonsterBlackboardComponent->ClearValue(TargetActorKey);
	MonsterBlackboardComponent->SetValueAsBool(HasTargetKey, false);
	MonsterBlackboardComponent->SetValueAsBool(IsInAttackRangeKey, false);
	MonsterBlackboardComponent->SetValueAsBool(WasDamagedKey, false);
	LastTargetSenseTime = -1.0;
	NextPatrolMoveTime = 0.0;
	bWaitingForPatrolArrival = false;
	bAIEnabled = RunBehaviorTree(BehaviorTree);
}
void ARogue10mMonsterAIController::OnUnPossess()
{
	StopMonsterAI();
	Super::OnUnPossess();
}

bool ARogue10mMonsterAIController::ExecuteBehaviorDecision()
{
	ARogue10mBasicMonster* Monster = GetMonsterPawn();
	if (!bAIEnabled || !Monster || Monster->IsDead() || !MonsterBlackboardComponent || !GetWorld())
	{
		return false;
	}

	ARogue10mCharacter* Target = GetBlackboardTarget();
	if (Target && !IsValidCombatTarget(Target))
	{
		ClearTarget();
		Target = nullptr;
	}

	if (Target)
	{
		const FVector HomeLocation =
			MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey);
		const double CurrentTime = GetWorld()->GetTimeSeconds();
		const FAISenseID SightSenseId = UAISense::GetSenseID<UAISense_Sight>();
		const bool bHasActiveSight =
			MonsterPerceptionComponent->HasActiveStimulus(*Target, SightSenseId);
		if (bHasActiveSight)
		{
			LastTargetSenseTime = CurrentTime;
			MonsterBlackboardComponent->SetValueAsVector(
				LastKnownLocationKey, Target->GetActorLocation());
		}

		const bool bTargetOutsideLeash = FVector::Dist2D(
			HomeLocation, Target->GetActorLocation()) > Monster->GetMaxChaseDistance();
		const bool bMonsterOutsideLeash = FVector::Dist2D(
			HomeLocation, Monster->GetActorLocation()) > Monster->GetMaxChaseDistance();
		const bool bForgotTarget = !bHasActiveSight && LastTargetSenseTime >= 0.0
			&& CurrentTime - LastTargetSenseTime > Monster->GetForgetTargetSeconds();
		if (bTargetOutsideLeash || bMonsterOutsideLeash || bForgotTarget)
		{
			ClearTarget();
			Target = nullptr;
		}
	}

	// Resume while already visible also works without a synthetic damage stimulus.
	if (!Target && FVector::Dist2D(MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey),
		Monster->GetActorLocation()) <= Monster->GetMaxChaseDistance())
	{
		TArray<AActor*> VisibleActors;
		MonsterPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
		float NearestDistance = TNumericLimits<float>::Max();
		const FVector Home = MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey);
		for (AActor* Actor : VisibleActors)
		{
			auto* Candidate = Cast<ARogue10mCharacter>(Actor);
			if (!IsValidCombatTarget(Candidate)
				|| FVector::Dist2D(Home, Candidate->GetActorLocation()) > Monster->GetMaxChaseDistance()) continue;
			const float Distance = FVector::DistSquared2D(Monster->GetActorLocation(), Candidate->GetActorLocation());
			if (Distance < NearestDistance) { NearestDistance = Distance; Target = Candidate; }
		}
		if (Target) { AcquireTarget(Target, false); Target = GetBlackboardTarget(); }
	}
	if (Target)
	{
		UpdateCombatBehavior(*Monster, *Target);
	}
	else
	{
		UpdatePatrolBehavior(*Monster);
	}
	return true;
}

void ARogue10mMonsterAIController::NotifyDamageStimulus(
	AActor* SourceActor, float DamageAmount)
{
	ARogue10mCharacter* SourceCharacter = Cast<ARogue10mCharacter>(SourceActor);
	ARogue10mBasicMonster* Monster = GetMonsterPawn();
	if (!bAIEnabled || !Monster || !IsValidCombatTarget(SourceCharacter))
	{
		return;
	}

	AcquireTarget(SourceCharacter, true);
	UAISense_Damage::ReportDamageEvent(
		this,
		Monster,
		SourceCharacter,
		DamageAmount,
		SourceCharacter->GetActorLocation(),
		Monster->GetActorLocation());
}

void ARogue10mMonsterAIController::StopMonsterAI()
{
	bAIEnabled = false;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (MonsterBehaviorTreeComponent)
	{
		MonsterBehaviorTreeComponent->StopTree(EBTStopMode::Safe);
	}
	ClearTarget();
}

void ARogue10mMonsterAIController::HandleTargetPerceptionUpdated(
	AActor* Actor, FAIStimulus Stimulus)
{
	ARogue10mCharacter* Target = Cast<ARogue10mCharacter>(Actor);
	if (!bAIEnabled || !IsValidCombatTarget(Target) || !GetWorld())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		const bool bFromDamage =
			Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>();
		AcquireTarget(Target, bFromDamage);
	}
	else if (GetBlackboardTarget() == Target && MonsterBlackboardComponent)
	{
		MonsterBlackboardComponent->SetValueAsVector(
			LastKnownLocationKey, Target->GetActorLocation());
		LastTargetSenseTime = GetWorld()->GetTimeSeconds();
	}
}

ARogue10mBasicMonster* ARogue10mMonsterAIController::GetMonsterPawn() const
{
	return Cast<ARogue10mBasicMonster>(GetPawn());
}

ARogue10mCharacter* ARogue10mMonsterAIController::GetBlackboardTarget() const
{
	return MonsterBlackboardComponent
		? Cast<ARogue10mCharacter>(
			MonsterBlackboardComponent->GetValueAsObject(TargetActorKey))
		: nullptr;
}

bool ARogue10mMonsterAIController::IsValidCombatTarget(
	const ARogue10mCharacter* Target) const
{
	return IsValid(Target) && Target->GetWorld() == GetWorld() && !Target->IsDead();
}

void ARogue10mMonsterAIController::AcquireTarget(
	ARogue10mCharacter* Target, bool bFromDamage)
{
	if (!bAIEnabled || !IsValidCombatTarget(Target) || !MonsterBlackboardComponent || !GetWorld())
	{
		return;
	}
	const auto* MonsterPawn = GetMonsterPawn();
	if (!MonsterPawn || MonsterPawn->IsDead()
		|| FVector::Dist2D(MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey), MonsterPawn->GetActorLocation())
			> MonsterPawn->GetMaxChaseDistance()
		|| FVector::Dist2D(MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey), Target->GetActorLocation())
			> MonsterPawn->GetMaxChaseDistance()) return;
	MonsterBlackboardComponent->SetValueAsObject(TargetActorKey, Target);
	MonsterBlackboardComponent->SetValueAsVector(
		LastKnownLocationKey, Target->GetActorLocation());
	MonsterBlackboardComponent->SetValueAsBool(HasTargetKey, true);
	MonsterBlackboardComponent->SetValueAsBool(WasDamagedKey, bFromDamage);
	LastTargetSenseTime = GetWorld()->GetTimeSeconds();
	if (const ARogue10mBasicMonster* Monster = GetMonsterPawn(); Monster && Monster->IsPatternAttackActive())
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
	else
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);
	}
}

void ARogue10mMonsterAIController::ClearTarget()
{
	if (ARogue10mBasicMonster* Monster = GetMonsterPawn())
	{
		Monster->ClearAITarget();
		// A finished attack can still have a short cosmetic tail in Idle.
		if (auto* Pattern = Monster->GetAttackPatternComponent()) Pattern->CancelAttack();
	}
	if (MonsterBlackboardComponent)
	{
		MonsterBlackboardComponent->ClearValue(TargetActorKey);
		MonsterBlackboardComponent->SetValueAsBool(HasTargetKey, false);
		MonsterBlackboardComponent->SetValueAsBool(IsInAttackRangeKey, false);
		MonsterBlackboardComponent->SetValueAsBool(WasDamagedKey, false);
	}
	LastTargetSenseTime = -1.0;
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();
}

void ARogue10mMonsterAIController::UpdateCombatBehavior(
	ARogue10mBasicMonster& Monster, ARogue10mCharacter& Target)
{
	const float Distance = FVector::Dist2D(
		Monster.GetActorLocation(), Target.GetActorLocation());
	const float AttackRange = Monster.GetEffectiveAttackRange();
	const bool bInAttackRange = Distance <= AttackRange;
	MonsterBlackboardComponent->SetValueAsBool(IsInAttackRangeKey, bInAttackRange);
	MonsterBlackboardComponent->SetValueAsVector(
		LastKnownLocationKey, Target.GetActorLocation());
	Monster.SetAITarget(&Target);
	if (Monster.IsPatternAttackActive())
	{
		// Hold a basic swing's facing until its short recovery completes.
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}
	SetFocus(&Target, EAIFocusPriority::Gameplay);

	if (bInAttackRange)
	{
		StopMovement();
		Monster.ExecuteAICombat(&Target);
		return;
	}

	// Combat uses center-to-center distances. Exclude both reach-test radii so
	// MoveTo's initial and final arrival checks use the same distance contract.
	// Otherwise short-range monsters can finish outside melee range and never attack.
	const float DesiredCenterDistance = FMath::Max(
		0.0f, FMath::Min(Monster.GetStopDistance(), AttackRange * 0.8f));
	FAIMoveRequest MoveRequest(&Target);
	MoveRequest.SetAcceptanceRadius(DesiredCenterDistance);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetCanStrafe(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetNavigationFilter(GetDefaultNavigationFilterClass());
	// Actor goals follow moving targets. Keep useful paths instead of aborting
	// and resubmitting them on every quarter-second decision.
	if (GetMoveStatus() == EPathFollowingStatus::Idle || !GetPathFollowingComponent()
		|| GetPathFollowingComponent()->GetMoveGoal() != &Target)
	{
		MoveTo(MoveRequest);
	}
}

void ARogue10mMonsterAIController::UpdatePatrolBehavior(
	ARogue10mBasicMonster& Monster)
{
	if (!GetWorld())
	{
		return;
	}

	const FVector HomeLocation =
		MonsterBlackboardComponent->GetValueAsVector(HomeLocationKey);
	const float DistanceFromHome = FVector::Dist2D(
		Monster.GetActorLocation(), HomeLocation);
	if (DistanceFromHome > Monster.GetPatrolRadius() + 100.0f)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToLocation(HomeLocation, 60.0f, true, true, true, false);
			bWaitingForPatrolArrival = true;
		}
		return;
	}

	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		bWaitingForPatrolArrival = true;
		return;
	}

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (bWaitingForPatrolArrival)
	{
		bWaitingForPatrolArrival = false;
		NextPatrolMoveTime = CurrentTime + Monster.GetPatrolWaitSeconds();
		return;
	}
	if (CurrentTime < NextPatrolMoveTime)
	{
		return;
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation PatrolLocation;
	if (NavigationSystem && NavigationSystem->GetRandomReachablePointInRadius(
		HomeLocation, Monster.GetPatrolRadius(), PatrolLocation))
	{
		MonsterBlackboardComponent->SetValueAsVector(
			PatrolLocationKey, PatrolLocation.Location);
		MoveToLocation(PatrolLocation.Location, 50.0f, true, true, true, false);
		bWaitingForPatrolArrival = true;
	}
	else
	{
		NextPatrolMoveTime = CurrentTime + Monster.GetPatrolWaitSeconds();
	}
}
void ARogue10mMonsterAIController::BuildRuntimeBlackboard()
{
	if (RuntimeBlackboard)
	{
		return;
	}
	RuntimeBlackboard = NewObject<UBlackboardData>(
		this, TEXT("Runtime Monster Blackboard"), RF_Transient);
	UBlackboardKeyType_Object* TargetKeyType =
		RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Object>(TargetActorKey);
	if (TargetKeyType)
	{
		TargetKeyType->BaseClass = AActor::StaticClass();
	}
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(HomeLocationKey);
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(PatrolLocationKey);
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Vector>(LastKnownLocationKey);
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Bool>(HasTargetKey);
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Bool>(IsInAttackRangeKey);
	RuntimeBlackboard->UpdatePersistentKey<UBlackboardKeyType_Bool>(WasDamagedKey);
	RuntimeBlackboard->UpdateKeyIDs();
}

void ARogue10mMonsterAIController::BuildRuntimeBehaviorTree()
{
	if (RuntimeBehaviorTree)
	{
		return;
	}
	RuntimeBehaviorTree = NewObject<UBehaviorTree>(
		this, TEXT("Runtime Monster Behavior Tree"), RF_Transient);
	RuntimeBehaviorTree->BlackboardAsset = RuntimeBlackboard;

	UBTComposite_Sequence* RootSequence = NewObject<UBTComposite_Sequence>(
		RuntimeBehaviorTree, TEXT("Monster Decision Loop"));
	UBTTask_Rogue10mMonsterDecision* DecisionTask =
		NewObject<UBTTask_Rogue10mMonsterDecision>(RuntimeBehaviorTree);
	UBTTask_Wait* WaitTask = NewObject<UBTTask_Wait>(RuntimeBehaviorTree);
	WaitTask->WaitTime = FValueOrBBKey_Float(0.25f);
	WaitTask->RandomDeviation = FValueOrBBKey_Float(0.0f);

	FBTCompositeChild DecisionChild;
	DecisionChild.ChildTask = DecisionTask;
	RootSequence->Children.Add(DecisionChild);
	FBTCompositeChild WaitChild;
	WaitChild.ChildTask = WaitTask;
	RootSequence->Children.Add(WaitChild);
	RuntimeBehaviorTree->RootNode = RootSequence;
}
