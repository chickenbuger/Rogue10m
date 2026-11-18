#include "Rogue10mVillageQuestComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mVillageQuestRuntime
{
	bool ResolveCharacter(ARogue10mCharacter* Character, FGuid& OutRunId, FGuid& OutProfileId)
	{
		if (!IsInGameThread() || !IsValid(Character) || !Character->HasAuthority() || Character->IsDead()
			|| !Character->IsLocallyControlled()) { return false; }
		UWorld* World = Character->GetWorld();
		if (!World || !World->IsGameWorld() || UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
			!= TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110")) { return false; }
		ARogue10mPlayerState* State = Character->GetPlayerState<ARogue10mPlayerState>();
		APlayerController* Controller = Cast<APlayerController>(Character->GetController());
		const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
		if (!IsValid(State) || !State->HasAuthority() || State->GetPawn() != Character
			|| !IsValid(Controller) || !Controller->IsLocalController() || Controller->GetPawn() != Character
			|| Controller->PlayerState != State || State->GetWorld() != World || !IsValid(Run) || !Run->IsRunActive()) { return false; }
		UGameInstance* Instance = World->GetGameInstance();
		const URogue10mProgressionSubsystem* Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		if (!Saves || !Saves->IsAttachedTo(*Character)) { return false; }
		OutRunId = Saves->GetAttachedRunId();
		OutProfileId = Saves->GetAttachedProfileId();
		return OutRunId.IsValid() && OutProfileId.IsValid();
	}
}

URogue10mVillageQuestComponent::URogue10mVillageQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URogue10mVillageQuestComponent* URogue10mVillageQuestComponent::GetOrCreate(ARogue10mCharacter* Character)
{
	FGuid RunId, ProfileId;
	if (!Rogue10mVillageQuestRuntime::ResolveCharacter(Character, RunId, ProfileId)) { return nullptr; }
	ARogue10mPlayerState* State = Character->GetPlayerState<ARogue10mPlayerState>();
	URogue10mVillageQuestComponent* Result = State->FindComponentByClass<URogue10mVillageQuestComponent>();
	if (!Result)
	{
		Result = NewObject<URogue10mVillageQuestComponent>(State, NAME_None, RF_Transient);
		State->AddInstanceComponent(Result);
		Result->RegisterComponent();
	}
	ARogue10mCharacter* CurrentCharacter = nullptr;
	return Result->SynchronizeContext(CurrentCharacter) && CurrentCharacter == Character ? Result : nullptr;
}

bool URogue10mVillageQuestComponent::ResolveContext(ARogue10mCharacter*& OutCharacter, FGuid& OutRunId, FGuid& OutProfileId) const
{
	OutCharacter = nullptr;
	const ARogue10mPlayerState* State = Cast<ARogue10mPlayerState>(GetOwner());
	ARogue10mCharacter* Character = State ? Cast<ARogue10mCharacter>(State->GetPawn()) : nullptr;
	if (!IsValid(this) || bStopped || !IsValid(State) || !State->HasAuthority() || !IsRegistered()
		|| !Rogue10mVillageQuestRuntime::ResolveCharacter(Character, OutRunId, OutProfileId)
		|| Character->GetPlayerState<ARogue10mPlayerState>() != State || GetWorld() != Character->GetWorld()) { return false; }
	OutCharacter = Character;
	return true;
}

void URogue10mVillageQuestComponent::BindRunState()
{
	ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (ObservedRun.Get() == Run) { return; }
	if (ObservedRun.IsValid()) { ObservedRun->OnRunPhaseChanged.RemoveDynamic(this, &URogue10mVillageQuestComponent::HandleRunPhaseChanged); }
	ObservedRun = Run;
	if (Run) { Run->OnRunPhaseChanged.AddUniqueDynamic(this, &URogue10mVillageQuestComponent::HandleRunPhaseChanged); }
}

bool URogue10mVillageQuestComponent::SynchronizeContext(ARogue10mCharacter*& OutCharacter)
{
	FGuid RunId, ProfileId;
	if (!ResolveContext(OutCharacter, RunId, ProfileId)) { return false; }
	BindRunState();
	if (RunId != ActiveRunId || ProfileId != ActiveProfileId)
	{
		++ContextGeneration;
		ActiveRunId = RunId;
		ActiveProfileId = ProfileId;
		bRunTerminated = false;
		for (TPair<FName, FQuestEntry>& Pair : Quests)
		{
			Pair.Value.Model.Reset();
			Pair.Value.DeferredToken = 0;
		}
	}
	if (bRunTerminated) { return false; }
	if (!bRewardInFlight)
	{
		// Inventory broadcasts may temporarily unpossess the pawn. Consume a successful
		// delivery only after the same run has a valid pawn again; never grant it twice.
		for (TPair<FName, FQuestEntry>& Pair : Quests)
		{
			FQuestEntry& Entry = Pair.Value;
			if (Entry.DeferredToken != 0)
			{
				Entry.Model.FinishClaim(Entry.DeferredToken, Entry.bDeferredSuccess);
				Entry.DeferredToken = 0;
			}
		}
	}
	return true;
}

bool URogue10mVillageQuestComponent::DefinitionMatches(FName QuestId, const FQuestEntry& Entry) const
{
	const TObjectPtr<URogue10mVillageQuestDataAsset>* Found = Definitions.Find(QuestId);
	const URogue10mVillageQuestDataAsset* Definition = Found ? Found->Get() : nullptr;
	return IsValid(Definition) && Definition->QuestId == QuestId && Definition->ObjectiveIds == Entry.Objectives
		&& Definition->RewardQuantity == Entry.Quantity && Definition->RewardItem.ToSoftObjectPath() == Entry.RewardPath;
}

bool URogue10mVillageQuestComponent::RegisterQuest(URogue10mVillageQuestDataAsset* Definition)
{
	ARogue10mCharacter* Character = nullptr;
	if (!SynchronizeContext(Character) || bRewardInFlight || !IsValid(Definition)) { return false; }
	FString Reason;
	if (!Definition->ValidateDefinition(Reason))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Village quest definition rejected: %s"), *Reason);
		return false;
	}
	if (const TObjectPtr<URogue10mVillageQuestDataAsset>* Existing = Definitions.Find(Definition->QuestId))
	{
		const FQuestEntry* Entry = Quests.Find(Definition->QuestId);
		return Existing->Get() == Definition && Entry && DefinitionMatches(Definition->QuestId, *Entry);
	}
	FQuestEntry Entry;
	if (!Entry.Model.Configure(static_cast<unsigned>(Definition->ObjectiveIds.Num()))) { return false; }
	Entry.Objectives = Definition->ObjectiveIds;
	Entry.RewardPath = Definition->RewardItem.ToSoftObjectPath();
	Entry.Quantity = Definition->RewardQuantity;
	Definitions.Add(Definition->QuestId, Definition);
	Quests.Add(Definition->QuestId, MoveTemp(Entry));
	return true;
}

bool URogue10mVillageQuestComponent::AcceptQuest(FName QuestId)
{
	ARogue10mCharacter* Character = nullptr;
	if (!SynchronizeContext(Character) || bRewardInFlight) { return false; }
	FQuestEntry* Entry = Quests.Find(QuestId);
	return Entry && DefinitionMatches(QuestId, *Entry) && Entry->Model.Accept();
}

bool URogue10mVillageQuestComponent::RecordObjective(FName QuestId, FName ObjectiveId)
{
	ARogue10mCharacter* Character = nullptr;
	if (!SynchronizeContext(Character) || bRewardInFlight) { return false; }
	FQuestEntry* Entry = Quests.Find(QuestId);
	if (!Entry || !DefinitionMatches(QuestId, *Entry)) { return false; }
	const int32 Index = Entry->Objectives.IndexOfByKey(ObjectiveId);
	return Index != INDEX_NONE && Entry->Model.RecordObjective(static_cast<unsigned>(Index));
}

bool URogue10mVillageQuestComponent::TryClaimReward(FName QuestId)
{
	ARogue10mCharacter* Character = nullptr;
	if (!SynchronizeContext(Character) || bRewardInFlight) { return false; }
	FQuestEntry* Entry = Quests.Find(QuestId);
	if (!Entry || !DefinitionMatches(QuestId, *Entry)) { return false; }
	const auto Ticket = Entry->Model.TryBeginClaim();
	if (Ticket == 0) { return false; }
	const FGuid ClaimRun = ActiveRunId, ClaimProfile = ActiveProfileId;
	const uint64 ClaimGeneration = ContextGeneration;
	const int32 Quantity = Entry->Quantity;
	const TSoftObjectPtr<URogue10mItemDataAsset> RewardReference(Entry->RewardPath);
	bool bAdded = false;
	{
		TGuardValue<bool> ClaimGuard(bRewardInFlight, true);
		TStrongObjectPtr<URogue10mItemDataAsset> Reward(RewardReference.LoadSynchronous());
		ARogue10mCharacter* CurrentCharacter = nullptr;
		// Loading can execute callbacks; no TMap entry or pawn pointer is retained across them.
		if (SynchronizeContext(CurrentCharacter) && ActiveRunId == ClaimRun && ActiveProfileId == ClaimProfile
			&& ContextGeneration == ClaimGeneration && IsValid(Reward.Get()))
		{
			const FQuestEntry* CurrentEntry = Quests.Find(QuestId);
			URogue10mInventoryComponent* Inventory = CurrentCharacter->GetInventoryComponent();
			if (CurrentEntry && DefinitionMatches(QuestId, *CurrentEntry) && IsValid(Inventory))
			{
				int32 ContainerIndex = INDEX_NONE;
				FGuid InstanceId;
				bAdded = Inventory->TryAddGridItem(Reward.Get(), Quantity, ContainerIndex, InstanceId);
			}
		}
		// Preserve the return value even if the inventory callback unpossessed the pawn.
		// Terminal or new-run generations are never completed by this old continuation.
		if (!bStopped && !bRunTerminated && ContextGeneration == ClaimGeneration
			&& ActiveRunId == ClaimRun && ActiveProfileId == ClaimProfile)
		{
			if (FQuestEntry* CurrentEntry = Quests.Find(QuestId))
			{
				CurrentEntry->DeferredToken = Ticket;
				CurrentEntry->bDeferredSuccess = bAdded;
			}
		}
	}
	ARogue10mCharacter* CurrentCharacter = nullptr;
	if (!SynchronizeContext(CurrentCharacter) || ContextGeneration != ClaimGeneration || ActiveRunId != ClaimRun) { return false; }
	const FQuestEntry* Finished = Quests.Find(QuestId);
	return bAdded && Finished && Finished->Model.GetStatus() == Rogue10mVillageQuest::EStatus::Claimed;
}

const URogue10mVillageQuestComponent::FQuestEntry* URogue10mVillageQuestComponent::FindReadableQuest(FName QuestId) const
{
	ARogue10mCharacter* Character = nullptr;
	FGuid RunId, ProfileId;
	if (!ResolveContext(Character, RunId, ProfileId) || bRunTerminated || RunId != ActiveRunId || ProfileId != ActiveProfileId) { return nullptr; }
	const FQuestEntry* Entry = Quests.Find(QuestId);
	return Entry && DefinitionMatches(QuestId, *Entry) ? Entry : nullptr;
}

ERogue10mVillageQuestStatus URogue10mVillageQuestComponent::GetQuestStatus(FName QuestId) const
{
	const FQuestEntry* Entry = FindReadableQuest(QuestId);
	if (!Entry) { return ERogue10mVillageQuestStatus::Unaccepted; }
	switch (Entry->Model.GetStatus())
	{
	case Rogue10mVillageQuest::EStatus::Accepted: return ERogue10mVillageQuestStatus::Accepted;
	case Rogue10mVillageQuest::EStatus::Ready: return ERogue10mVillageQuestStatus::Ready;
	case Rogue10mVillageQuest::EStatus::Claiming: return ERogue10mVillageQuestStatus::Claiming;
	case Rogue10mVillageQuest::EStatus::Claimed: return ERogue10mVillageQuestStatus::Claimed;
	default: return ERogue10mVillageQuestStatus::Unaccepted;
	}
}

int32 URogue10mVillageQuestComponent::GetCollectedCount(FName QuestId) const
{
	const FQuestEntry* Entry = FindReadableQuest(QuestId);
	return Entry ? static_cast<int32>(Entry->Model.GetCollectedCount()) : 0;
}

bool URogue10mVillageQuestComponent::HasObjective(FName QuestId, FName ObjectiveId) const
{
	const FQuestEntry* Entry = FindReadableQuest(QuestId);
	const int32 Index = Entry ? Entry->Objectives.IndexOfByKey(ObjectiveId) : INDEX_NONE;
	return Index != INDEX_NONE && Entry->Model.HasObjective(static_cast<unsigned>(Index));
}

void URogue10mVillageQuestComponent::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (Phase != ERogue10mRunPhase::Running && ActiveRunId.IsValid())
	{
		bRunTerminated = true;
		++ContextGeneration;
	}
}

void URogue10mVillageQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStopped = true;
	++ContextGeneration;
	if (ObservedRun.IsValid()) { ObservedRun->OnRunPhaseChanged.RemoveDynamic(this, &URogue10mVillageQuestComponent::HandleRunPhaseChanged); }
	ObservedRun.Reset();
	Super::EndPlay(EndPlayReason);
}
