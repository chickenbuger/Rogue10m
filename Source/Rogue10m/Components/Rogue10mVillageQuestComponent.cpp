#include "Rogue10mVillageQuestComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Misc/ScopeExit.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestObjective.h"
#include "Rogue10mProgressionSubsystem.h"
#include "TimerManager.h"

FGuid Rogue10mVillageQuestRunId(const UWorld* World)
{
	const auto* Instance = World ? World->GetGameInstance() : nullptr;
	const auto* Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	return Progression ? Progression->GetAttachedRunId() : FGuid();
}

bool Rogue10mVillageQuestSameRun(const UWorld* World, const ARogue10mGameState* Run, const FGuid& RunId)
{
	if (!World || World->bIsTearingDown || !IsValid(Run) || World->GetGameState() != Run || !Run->IsRunActive()) { return false; }
	// No-GI native fixtures have no profile domain. An unresolved production GUID is not that fallback.
	return World->GetGameInstance() ? RunId.IsValid() && RunId == Rogue10mVillageQuestRunId(World) : !RunId.IsValid();
}

URogue10mVillageQuestComponent::URogue10mVillageQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URogue10mVillageQuestComponent* URogue10mVillageQuestComponent::FindOrCreate(
	ARogue10mCharacter* Character, URogue10mVillageQuestDataAsset* Definition)
{
	APlayerState* State = IsValid(Character) ? Character->GetPlayerState() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	auto* Existing = State ? State->FindComponentByClass<URogue10mVillageQuestComponent>() : nullptr;
	auto* CandidateDefinition = Definition ? Definition : Existing ? Existing->ActiveDefinition.Get()
		: GetMutableDefault<URogue10mVillageQuestDataAsset>();
	if (!State || !State->HasAuthority() || !World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !CandidateDefinition || !CandidateDefinition->IsSupportedWorld(World)) { return nullptr; }
	if (Existing)
	{
		// A different director/data definition cannot reinterpret a live run's receipts.
		return Existing->ActiveDefinition == CandidateDefinition ? Existing : nullptr;
	}
	auto* Component = NewObject<URogue10mVillageQuestComponent>(State, TEXT("VillageQuests"), RF_Transient);
	Component->ActiveDefinition = CandidateDefinition;
	State->AddInstanceComponent(Component);
	Component->RegisterComponent();
	return Component;
}

void URogue10mVillageQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!ActiveDefinition) { ActiveDefinition = GetMutableDefault<URogue10mVillageQuestDataAsset>(); }
	FString Reason;
	bDefinitionValid = ActiveDefinition->ValidateDefinition(Reason);
	if (!bDefinitionValid) { UE_LOG(LogRogue10m, Error, TEXT("Village quests disabled: %s"), *Reason); }
	RunState = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (RunState.IsValid())
	{
		RunState->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged);
		HandleRunPhaseChanged(RunState->GetRunPhase());
	}
}

void URogue10mVillageQuestComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bCombatCreditOwnerEnded = true; ResetCombatDeaths();
	if (RunState.IsValid()) { RunState->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRunPhaseChanged); }
	Super::EndPlay(Reason);
}

void URogue10mVillageQuestComponent::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	ResetCombatDeaths();
	if (Phase == ERogue10mRunPhase::Running)
	{
		if (bRewardGrantInProgress) { bResetAfterGrant = true; return; }
		Progress.Reset(); OptionalClue.Reset(); OnQuestChanged.Broadcast();
	}
}

bool URogue10mVillageQuestComponent::CanUse(const ARogue10mCharacter* Character, bool bAllowRewardGrant) const
{
	const auto* State = Cast<APlayerState>(GetOwner());
	return bDefinitionValid && (bAllowRewardGrant || !bRewardGrantInProgress) && IsValid(Character) && State && State->HasAuthority() && Character->HasAuthority()
		&& State == Character->GetPlayerState() && State->GetPawn() == Character && !Character->IsDead()
		&& Character->GetWorld() == GetWorld() && RunState.IsValid() && RunState->IsRunActive();
}

void URogue10mVillageQuestComponent::ResetCombatDeaths()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(CombatFlushTimer); }
	bCombatFlushScheduled = false;
	CombatCredits.Reset();
	for (auto& Death : PendingCombatDeaths) { Death = {}; }
}

bool URogue10mVillageQuestComponent::IsCurrentCombatDeath(const FCombatDeath& Death, uint64 Epoch, int32 Slot) const
{
	return IsValid(this) && !bCombatCreditOwnerEnded && Epoch == CombatCredits.GetEpoch()
		&& Death.ObjectiveIndex >= 0 && Death.ObjectiveIndex < Rogue10mVillageQuest::ObjectiveCount
		&& Death.Definition.IsValid() && Death.Definition.Get() == ActiveDefinition.Get() && Death.Run == RunState
		&& Rogue10mVillageQuestSameRun(GetWorld(), Death.Run.Get(), Death.RunId)
		&& CanUse(Death.Character.Get(), true) && ActiveDefinition->FindQuestIndex(Death.QuestId) == Slot
		&& ActiveDefinition->Quests.IsValidIndex(Slot)
		&& ActiveDefinition->Quests[Slot].CombatObjectiveIndex == Death.ObjectiveIndex;
}

bool URogue10mVillageQuestComponent::EnqueueCombatDeath(FName QuestId, int32 ObjectiveIndex,
	ARogue10mCharacter* Character, uint64 Epoch, const FGuid& RunId)
{
	const int32 Slot = ActiveDefinition ? ActiveDefinition->FindQuestIndex(QuestId) : INDEX_NONE;
	FCombatDeath Death;
	Death.Character = Character; Death.Run = RunState; Death.Definition = ActiveDefinition;
	Death.RunId = RunId; Death.QuestId = QuestId; Death.ObjectiveIndex = ObjectiveIndex;
	if (!IsCurrentCombatDeath(Death, Epoch, Slot) || !Death.Run->Tags.Contains(Rogue10mVillageQuestEncounterDeathReceipt(QuestId, ObjectiveIndex))) { return false; }
	if (CombatCredits.HasPending(Slot) && !IsCurrentCombatDeath(PendingCombatDeaths[Slot], Epoch, Slot))
	{
		const auto Stale = CombatCredits.Begin(Slot);
		if (Stale.IsValid()) { CombatCredits.Cancel(Stale); }
	}
	if (!CombatCredits.Queue(Slot, Epoch)) { return false; }
	PendingCombatDeaths[Slot] = Death;
	ScheduleCombatDeathFlush();
	return true;
}

void URogue10mVillageQuestComponent::ScheduleCombatDeathFlush()
{
	if (bCombatFlushScheduled || bRewardGrantInProgress || bCombatCreditOwnerEnded || !GetWorld() || GetWorld()->bIsTearingDown) { return; }
	bCombatFlushScheduled = true;
	const uint64 Epoch = CombatCredits.GetEpoch();
	CombatFlushTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Epoch]
	{
		bCombatFlushScheduled = false;
		if (!bCombatCreditOwnerEnded && Epoch == CombatCredits.GetEpoch()) { FlushCombatDeaths(); }
	}));
}

void URogue10mVillageQuestComponent::FlushCombatDeaths()
{
	if (bFlushingCombatDeaths || bRewardGrantInProgress || bCombatCreditOwnerEnded || !IsValid(this)) { return; }
	bFlushingCombatDeaths = true;
	ON_SCOPE_EXIT
	{
		bFlushingCombatDeaths = false;
		for (int32 Slot = 0; Slot < Rogue10mVillageQuest::QuestCount; ++Slot)
		{
			if (CombatCredits.HasPending(Slot)) { ScheduleCombatDeathFlush(); break; }
		}
	};
	for (int32 Slot = 0; Slot < Rogue10mVillageQuest::QuestCount; ++Slot)
	{
		const auto Lease = CombatCredits.Begin(Slot);
		if (!Lease.IsValid()) { continue; }
		const FCombatDeath Death = PendingCombatDeaths[Slot];
		if (!IsCurrentCombatDeath(Death, Lease.Epoch, Slot)) { CombatCredits.Cancel(Lease); continue; }
		ObserveObjective(Death.QuestId, Death.ObjectiveIndex, Death.Character.Get());
		// Observe notifies synchronously. Director replacement is harmless; a run/owner reset is not.
		const bool bRecorded = IsCurrentCombatDeath(Death, Lease.Epoch, Slot)
			&& HasObservedObjective(Death.QuestId, Death.ObjectiveIndex);
		if (bRecorded && CombatCredits.Finish(Lease, true))
		{
			Death.Run->Tags.AddUnique(Rogue10mVillageQuestEncounterReceipt(Death.QuestId, Death.ObjectiveIndex));
		}
		else { CombatCredits.Cancel(Lease); }
	}
}

ERogue10mVillageQuestState URogue10mVillageQuestComponent::GetQuestState(FName QuestId) const
{
	return bDefinitionValid && ActiveDefinition
		? static_cast<ERogue10mVillageQuestState>(Progress.GetState(ActiveDefinition->FindQuestIndex(QuestId)))
		: ERogue10mVillageQuestState::Unavailable;
}

int32 URogue10mVillageQuestComponent::GetObjectiveProgress(FName QuestId) const
{
	return bDefinitionValid && ActiveDefinition ? Progress.GetProgress(ActiveDefinition->FindQuestIndex(QuestId)) : 0;
}

bool URogue10mVillageQuestComponent::HasObservedObjective(FName QuestId, int32 ObjectiveIndex) const
{
	return bDefinitionValid && ActiveDefinition
		&& Progress.HasObserved(ActiveDefinition->FindQuestIndex(QuestId), ObjectiveIndex);
}

ERogue10mVillageOptionalClueState URogue10mVillageQuestComponent::GetOptionalClueState() const
{
	return bDefinitionValid ? static_cast<ERogue10mVillageOptionalClueState>(OptionalClue.GetState())
		: ERogue10mVillageOptionalClueState::Missing;
}

bool URogue10mVillageQuestComponent::AnalyzeOptionalClue(FName AnalystQuestId, ARogue10mCharacter* Character)
{
	if (!CanUse(Character)) { return false; }
	const int32 Index = ActiveDefinition->FindQuestIndex(AnalystQuestId);
	if (!ActiveDefinition->Quests.IsValidIndex(Index) || !ActiveDefinition->Quests[Index].bOffersClueAnalysis
		|| !OptionalClue.Analyze()) { return false; }
	OnQuestChanged.Broadcast(); return true;
}

bool URogue10mVillageQuestComponent::ReportOptionalClue(FName ReportQuestId, ARogue10mCharacter* Character)
{
	if (!CanUse(Character)) { return false; }
	const int32 Index = ActiveDefinition->FindQuestIndex(ReportQuestId);
	if (!ActiveDefinition->Quests.IsValidIndex(Index) || !ActiveDefinition->Quests[Index].bAcceptsClueReport
		|| !OptionalClue.Report()) { return false; }
	OnQuestChanged.Broadcast(); return true;
}

bool URogue10mVillageQuestComponent::AcceptQuest(FName QuestId, ARogue10mCharacter* Character)
{
	if (!CanUse(Character) || !Progress.Accept(ActiveDefinition->FindQuestIndex(QuestId))) { return false; }
	OnQuestChanged.Broadcast(); return true;
}

bool URogue10mVillageQuestComponent::ObserveObjective(FName QuestId, int32 ObjectiveIndex, ARogue10mCharacter* Character)
{
	if (!CanUse(Character) || !Progress.Observe(ActiveDefinition->FindQuestIndex(QuestId), ObjectiveIndex)) { return false; }
	const int32 Index = ActiveDefinition->FindQuestIndex(QuestId);
	if (ActiveDefinition->Quests.IsValidIndex(Index)
		&& ActiveDefinition->Quests[Index].SupplyClueObjectiveIndex == ObjectiveIndex) { OptionalClue.ObserveSource(); }
	OnQuestChanged.Broadcast(); return true;
}

bool URogue10mVillageQuestComponent::ClaimReward(FName QuestId, ARogue10mCharacter* Character, FText& OutReason)
{
	OutReason = NSLOCTEXT("Rogue10mVillageQuest", "Unavailable", "지금은 이 의뢰의 보상을 받을 수 없습니다.");
	if (!CanUse(Character)) { return false; }
	const int32 Index = ActiveDefinition->FindQuestIndex(QuestId);
	if (!ActiveDefinition->Quests.IsValidIndex(Index)) { return false; }
	// Inventory delegates may re-enter user code; retain the promised payout across callbacks.
	const FRogue10mVillageQuestDefinition Row = ActiveDefinition->Quests[Index];
	auto* Inventory = Character->GetInventoryComponent();
	if (!Inventory || static_cast<int64>(Inventory->GetGold()) + Row.RewardGold > MAX_int32)
	{
		OutReason = NSLOCTEXT("Rogue10mVillageQuest", "CurrencyFull", "보상 재화를 받을 공간이 없습니다."); return false;
	}
	if (!Progress.BeginClaim(Index)) { return false; }
	bRewardGrantInProgress = true;
	ON_SCOPE_EXIT
	{
		bRewardGrantInProgress = false;
		if (bResetAfterGrant) { bResetAfterGrant = false; Progress.Reset(); OptionalClue.Reset(); }
		FlushCombatDeaths();
		OnQuestChanged.Broadcast();
	};
	if (Row.RewardItem)
	{
		int32 Container = INDEX_NONE; FGuid Instance;
		if (!Inventory->TryAddGridItem(Row.RewardItem, Row.RewardQuantity, Container, Instance))
		{
			Progress.FinishClaim(Index, false);
			OutReason = NSLOCTEXT("Rogue10mVillageQuest", "BagFull", "가방에 공간이 부족합니다. 정리한 뒤 다시 보고해 주세요.");
			return false;
		}
	}
	// Reserve before item delegates and commit before currency delegates can re-enter dialogue.
	Progress.FinishClaim(Index, true);
	Inventory->AddGold(Row.RewardGold);
	OutReason = Row.RewardItem
		? NSLOCTEXT("Rogue10mVillageQuest", "ItemRewarded", "의뢰 완료 · 보상 아이템을 가방에 넣었습니다.")
		: FText::Format(NSLOCTEXT("Rogue10mVillageQuest", "Rewarded", "의뢰 완료 · 이번 판 골드 +{0}"), FText::AsNumber(Row.RewardGold));
	UE_LOG(LogRogue10m, Log, TEXT("Village quest rewarded quest=%s player=%s gold=%d"), *QuestId.ToString(), *Character->GetName(), Row.RewardGold);
	return true;
}
