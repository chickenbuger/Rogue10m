#include "Rogue10mVillageQuestDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Rogue10m.h"
#include "Rogue10mGameState.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestNPC.h"
#include "Rogue10mVillageQuestObjective.h"
#include "Rogue10mVillageQuestComponent.h"
#include "TimerManager.h"

ARogue10mVillageQuestDirector::ARogue10mVillageQuestDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARogue10mVillageQuestDirector::BeginPlay()
{
	Super::BeginPlay(); UWorld* World = GetWorld();
	if (!HasAuthority() || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone) { return; }
	// Authored data selects the allowed map; the fallback only names the legacy prototype.
	if (!QuestDefinition) { QuestDefinition = GetMutableDefault<URogue10mVillageQuestDataAsset>(); }
	FString Reason;
	if (!QuestDefinition->ValidateDefinition(Reason))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Village quest director rejected data: %s"), *Reason); return;
	}
	if (!QuestDefinition->IsSupportedWorld(World)) { return; }
	RunState = World->GetGameState<ARogue10mGameState>();
	if (!RunState.IsValid()) { return; }
	for (TActorIterator<ARogue10mVillageQuestDirector> It(World); It; ++It)
	{
		if (*It != this && It->bOwnsQuestRuntime && !It->IsActorBeingDestroyed())
		{
			UE_LOG(LogRogue10m, Warning, TEXT("Duplicate village quest director ignored.")); return;
		}
	}
	bOwnsQuestRuntime = true;
	EncounterRunId = Rogue10mVillageQuestRunId(World);
	if (RunState.IsValid())
	{
		RunState->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged);
		// A reconstructed director joins the current run without clearing spent encounters.
		if (RunState->IsRunActive()) { SpawnRunActors(); }
	}
}

void ARogue10mVillageQuestDirector::DestroyRunActors()
{
	for (AActor* Actor : RunActors) { if (IsValid(Actor)) { Actor->Destroy(); } }
	RunActors.Reset();
	ResolvedCombatData.Reset();
}

void ARogue10mVillageQuestDirector::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	++EncounterGeneration; PendingRecoveryKeys.Reset(); InitialIdentityAttempts = 0;
	GetWorldTimerManager().ClearAllTimersForObject(this);
	// Terminal transitions close/remove NPC input too. A fresh run reconstructs non-respawning objectives.
	DestroyRunActors();
	if (Phase != ERogue10mRunPhase::Running || !QuestDefinition) { return; }
	EncounterRunId = Rogue10mVillageQuestRunId(GetWorld());
	// This is an actual new-run event. Joining an already-running world takes the path above.
	if (RunState.IsValid())
	{
		for (const auto& Row : QuestDefinition->Quests)
		{
			if (Row.bEnabled && Row.CombatObjectiveIndex != INDEX_NONE)
			{
				RunState->Tags.Remove(Rogue10mVillageQuestEncounterReceipt(Row.QuestId, Row.CombatObjectiveIndex));
				RunState->Tags.Remove(Rogue10mVillageQuestEncounterDeathReceipt(Row.QuestId, Row.CombatObjectiveIndex));
			}
		}
	}
	SpawnRunActors();
}

void ARogue10mVillageQuestDirector::SpawnRunActors()
{
	if (!QuestDefinition || !RunState.IsValid() || !RunState->IsRunActive()) { return; }
	const uint64 Generation = EncounterGeneration;
	// Bind once before actors/events exist. A later RunId change cannot retarget live encounters.
	if (!EncounterRunId.IsValid() && RunActors.IsEmpty()) { EncounterRunId = Rogue10mVillageQuestRunId(GetWorld()); }
	if (!IsCurrentEncounterRun())
	{
		if (++InitialIdentityAttempts <= FMath::Clamp(MaxRecoverySpawnAttempts, 1, 16))
		{
			FTimerHandle Handle;
			const float Delay = FMath::IsFinite(RecoveryRetryDelaySeconds) ? FMath::Max(.1f, RecoveryRetryDelaySeconds) : .25f;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Generation]
			{ if (bOwnsQuestRuntime && Generation == EncounterGeneration) { SpawnRunActors(); } }), Delay, false);
		}
		else { UE_LOG(LogRogue10m, Error, TEXT("Village encounters await a valid attached run identity; use normal run entry or reconstruct director.")); }
		return;
	}
	ResolvedCombatData.Reset(); ResolvedCombatData.SetNum(QuestDefinition->Quests.Num());
	// Resolve required encounter content before creating any NPC or objective.
	for (int32 QuestIndex = 0; QuestIndex < QuestDefinition->Quests.Num(); ++QuestIndex)
	{
		const auto Row = QuestDefinition->Quests[QuestIndex];
		if (!Row.bEnabled || Row.CombatObjectiveIndex == INDEX_NONE
			|| RunState->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(Row.QuestId, Row.CombatObjectiveIndex))) { continue; }
		auto* Data = Row.CombatMonsterData.LoadSynchronous();
		if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
		ResolvedCombatData[QuestIndex] = Data;
		if (!ResolvedCombatData[QuestIndex] || ResolvedCombatData[QuestIndex]->MonsterRank != ERogue10mMonsterRank::Normal)
		{
			UE_LOG(LogRogue10m, Error, TEXT("Quest %s requires loadable ordinary monster data: %s"),
				*Row.QuestId.ToString(), *Row.CombatMonsterData.ToSoftObjectPath().ToString());
			ResolvedCombatData.Reset(); return;
		}
	}
	for (int32 QuestIndex = 0; QuestIndex < QuestDefinition->Quests.Num(); ++QuestIndex)
	{
		const auto Row = QuestDefinition->Quests[QuestIndex];
		if (!Row.bEnabled) { continue; }
		const FTransform NpcTransform(QuestDefinition->ToWorldRotation(Row.NpcRotation), QuestDefinition->ToWorldLocation(Row.NpcLocation));
		auto* Npc = GetWorld()->SpawnActorDeferred<ARogue10mVillageQuestNPC>(ARogue10mVillageQuestNPC::StaticClass(),
			NpcTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Npc) { UE_LOG(LogRogue10m, Error, TEXT("Could not spawn quest NPC %s"), *Row.QuestId.ToString()); continue; }
		if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { Npc->Destroy(); return; }
		Npc->QuestId = Row.QuestId; Npc->QuestDefinition = QuestDefinition;
		Npc->TutorialData = Row.NpcPresentation;
		Npc->InteractionRadius = Row.NpcInteractionRadiusCm; Npc->DetectionRadius = Row.NpcDetectionRadiusCm;
		RunActors.Add(Npc); Npc->FinishSpawning(NpcTransform);
		if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
		for (int32 Index = 0; Index < Row.ObjectiveLocations.Num(); ++Index)
		{
			const FTransform Transform(QuestDefinition->ToWorldRotation(FRotator::ZeroRotator), QuestDefinition->ToWorldLocation(Row.ObjectiveLocations[Index]));
			if (Index == Row.CombatObjectiveIndex)
			{
				if (RunState->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(Row.QuestId, Index))) { continue; }
				if (RunState->Tags.Contains(Rogue10mVillageQuestEncounterDeathReceipt(Row.QuestId, Index)))
				{
					RequestCombatRecovery(Row.QuestId, Index, EncounterGeneration);
				}
				else if (!SpawnCombatObjective(QuestIndex, Index, false) && IsCurrentEncounterRun()
					&& !RunState->Tags.Contains(Rogue10mVillageQuestEncounterDeathReceipt(Row.QuestId, Index)))
				{ UE_LOG(LogRogue10m, Error, TEXT("Could not spawn quest encounter %s"), *Row.QuestId.ToString()); }
				continue;
			}
			auto* Objective = GetWorld()->SpawnActorDeferred<ARogue10mVillageQuestObjective>(ARogue10mVillageQuestObjective::StaticClass(),
				Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Objective)
			{
				if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { Objective->Destroy(); return; }
				Objective->QuestId = Row.QuestId; Objective->ObjectiveIndex = Index;
				Objective->Presentation = Row.ObjectivePresentation;
				Objective->ObjectiveLabel = Row.ObjectiveLabels[Index]; Objective->QuestDefinition = QuestDefinition;
				RunActors.Add(Objective); Objective->FinishSpawning(Transform);
				if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
			}
			else { UE_LOG(LogRogue10m, Error, TEXT("Could not spawn quest objective %s:%d"), *Row.QuestId.ToString(), Index); }
		}
	}
}

void ARogue10mVillageQuestDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	bOwnsQuestRuntime = false; ++EncounterGeneration; PendingRecoveryKeys.Reset();
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (RunState.IsValid()) { RunState->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRunPhaseChanged); }
	DestroyRunActors(); bOwnsQuestRuntime = false; Super::EndPlay(Reason);
}

bool ARogue10mVillageQuestDirector::IsCurrentEncounterRun() const
{
	return bOwnsQuestRuntime && !IsActorBeingDestroyed() && QuestDefinition
		&& Rogue10mVillageQuestSameRun(GetWorld(), RunState.Get(), EncounterRunId);
}

bool ARogue10mVillageQuestDirector::SpawnCombatObjective(int32 QuestIndex, int32 ObjectiveIndex, bool bRecovery)
{
	if (!IsCurrentEncounterRun() || !QuestDefinition->Quests.IsValidIndex(QuestIndex)
		|| !ResolvedCombatData.IsValidIndex(QuestIndex) || !ResolvedCombatData[QuestIndex]) { return false; }
	const auto Row = QuestDefinition->Quests[QuestIndex];
	if (!Row.bEnabled || Row.CombatObjectiveIndex != ObjectiveIndex || !Row.ObjectiveLocations.IsValidIndex(ObjectiveIndex)
		|| RunState->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(Row.QuestId, ObjectiveIndex))) { return false; }
	for (const auto& Actor : RunActors)
	{
		const auto* Existing = Cast<ARogue10mVillageQuestMonster>(Actor.Get());
		if (IsValid(Existing) && !Existing->IsActorBeingDestroyed() && !Existing->IsDead()
			&& Existing->QuestId == Row.QuestId && Existing->ObjectiveIndex == ObjectiveIndex) { return true; }
	}
	const uint64 Generation = EncounterGeneration;
	const FTransform Transform(QuestDefinition->ToWorldRotation(FRotator::ZeroRotator), QuestDefinition->ToWorldLocation(Row.ObjectiveLocations[ObjectiveIndex]));
	auto* Monster = GetWorld()->SpawnActorDeferred<ARogue10mVillageQuestMonster>(ARogue10mVillageQuestMonster::StaticClass(),
		Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Monster) { return false; }
	if (!IsCurrentEncounterRun() || Generation != EncounterGeneration || !Monster->ConfigureQuestMonsterData(ResolvedCombatData[QuestIndex]))
	{ Monster->Destroy(); return false; }
	Monster->QuestId = Row.QuestId; Monster->ObjectiveIndex = ObjectiveIndex; Monster->QuestDefinition = QuestDefinition;
	Monster->bRecoveryEncounter = bRecovery;
	RunActors.Add(Monster); // Include deferred actors in dedup/cleanup before BeginPlay can re-enter.
	Monster->FinishSpawning(Transform);
	return IsValid(Monster) && !Monster->IsActorBeingDestroyed() && !Monster->IsDead()
		&& IsCurrentEncounterRun() && Generation == EncounterGeneration;
}

void ARogue10mVillageQuestDirector::RequestCombatRecovery(FName QuestId, int32 ObjectiveIndex, uint64 Generation)
{
	if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
	const FName Key = Rogue10mVillageQuestEncounterReceipt(QuestId, ObjectiveIndex);
	if (RunState->Tags.Contains(Key) || PendingRecoveryKeys.Contains(Key)
		|| !RunState->Tags.Contains(Rogue10mVillageQuestEncounterDeathReceipt(QuestId, ObjectiveIndex))) { return; }
	PendingRecoveryKeys.Add(Key);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, QuestId, ObjectiveIndex, Generation]
	{ TryCombatRecovery(QuestId, ObjectiveIndex, Generation, 1); }));
}

void ARogue10mVillageQuestDirector::TryCombatRecovery(FName QuestId, int32 ObjectiveIndex, uint64 Generation, int32 Attempt)
{
	if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
	const FName Key = Rogue10mVillageQuestEncounterReceipt(QuestId, ObjectiveIndex);
	const int32 Slot = QuestDefinition->FindQuestIndex(QuestId);
	if (!QuestDefinition->Quests.IsValidIndex(Slot) || QuestDefinition->Quests[Slot].CombatObjectiveIndex != ObjectiveIndex
		|| RunState->Tags.Contains(Key) || !RunState->Tags.Contains(Rogue10mVillageQuestEncounterDeathReceipt(QuestId, ObjectiveIndex)))
	{ PendingRecoveryKeys.Remove(Key); return; }
	if (SpawnCombatObjective(Slot, ObjectiveIndex, true)) { PendingRecoveryKeys.Remove(Key); return; }
	if (!IsCurrentEncounterRun() || Generation != EncounterGeneration) { return; }
	if (RunState->Tags.Contains(Key)) { PendingRecoveryKeys.Remove(Key); return; }
	if (Attempt >= FMath::Clamp(MaxRecoverySpawnAttempts, 1, 16))
	{
		PendingRecoveryKeys.Remove(Key);
		UE_LOG(LogRogue10m, Error, TEXT("Quest recovery spawn blocked after %d attempts: %s; inspect authored collision or reconstruct director."), Attempt, *QuestId.ToString());
		return;
	}
	const float Delay = FMath::IsFinite(RecoveryRetryDelaySeconds) ? FMath::Max(.1f, RecoveryRetryDelaySeconds) : .25f;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, QuestId, ObjectiveIndex, Generation, Attempt]
	{ TryCombatRecovery(QuestId, ObjectiveIndex, Generation, Attempt + 1); }), Delay, false);
}
