#include "Rogue10mVillageQuestNPC.h"
#include "GameFramework/PlayerState.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mTutorialWidget.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"

void ARogue10mVillageQuestNPC::BeginPlay()
{
	if (!QuestDefinition) { QuestDefinition = GetMutableDefault<URogue10mVillageQuestDataAsset>(); }
	Super::BeginPlay();
}

URogue10mVillageQuestComponent* ARogue10mVillageQuestNPC::FindQuests(bool bCreate) const
{
	ARogue10mCharacter* Character = GetDialogueCharacter();
	if (bCreate) { return URogue10mVillageQuestComponent::FindOrCreate(Character, QuestDefinition); }
	const APlayerState* State = Character ? Character->GetPlayerState() : nullptr;
	auto* Existing = State ? State->FindComponentByClass<URogue10mVillageQuestComponent>() : nullptr;
	return Existing && Existing->GetDefinition() == QuestDefinition ? Existing : nullptr;
}

const FRogue10mVillageQuestDefinition* ARogue10mVillageQuestNPC::FindDefinition() const
{
	const int32 Index = QuestDefinition ? QuestDefinition->FindQuestIndex(QuestId) : INDEX_NONE;
	return QuestDefinition && QuestDefinition->Quests.IsValidIndex(Index) ? &QuestDefinition->Quests[Index] : nullptr;
}

void ARogue10mVillageQuestNPC::HandleInteract()
{
	ARogue10mCharacter* Character = GetDialogueCharacter();
	if (!IsPlayerAvailable(Character, InteractionRadius)) { return; }
	if (IsDialogueOpen()) { CloseDialogue(); }
	else { FindQuests(true); BeginDialogue(Character); }
}

TArray<FText> ARogue10mVillageQuestNPC::BuildServiceChoices() const
{
	const auto* Quests = FindQuests(false);
	const auto State = Quests ? Quests->GetQuestState(QuestId) : ERogue10mVillageQuestState::Unavailable;
	TArray<FText> Choices;
	switch (State)
	{
	case ERogue10mVillageQuestState::Available:
		Choices.Add(NSLOCTEXT("Rogue10mVillageQuest", "Accept", "의뢰 수락하기")); break;
	case ERogue10mVillageQuestState::Ready:
		Choices.Add(NSLOCTEXT("Rogue10mVillageQuest", "Report", "완료 보고하고 보상 받기")); break;
	case ERogue10mVillageQuestState::Active:
		Choices.Add(NSLOCTEXT("Rogue10mVillageQuest", "Progress", "진행 상황 확인하기")); break;
	case ERogue10mVillageQuestState::Claimed:
		Choices.Add(NSLOCTEXT("Rogue10mVillageQuest", "Complete", "완료한 의뢰 확인하기")); break;
	default:
		Choices.Add(NSLOCTEXT("Rogue10mVillageQuest", "NotRunning", "탐험 진행 중에 의뢰를 이용할 수 있습니다")); break;
	}
	const auto* Row = FindDefinition();
	const auto Clue = Quests ? Quests->GetOptionalClueState() : ERogue10mVillageOptionalClueState::Missing;
	if (Row && Row->bOffersClueAnalysis && Clue == ERogue10mVillageOptionalClueState::Found)
	{
		Choices.Add(QuestDefinition->OptionalStoryText.AnalyzeAction);
	}
	else if (Row && Row->bAcceptsClueReport && Clue == ERogue10mVillageOptionalClueState::Analyzed)
	{
		Choices.Add(QuestDefinition->OptionalStoryText.ReportAction);
	}
	return Choices;
}

FText ARogue10mVillageQuestNPC::BuildServiceText() const
{
	const auto* Row = FindDefinition(); const auto* Quests = FindQuests(false);
	if (!Row) { return NSLOCTEXT("Rogue10mVillageQuest", "BadDefinition", "의뢰 설정을 확인해 주세요."); }
	if (Row->RewardItem)
	{
		return FText::Format(NSLOCTEXT("Rogue10mVillageQuest", "ItemSummary", "목표 진행 {0}/2 · 보상 {1} × {2}\n다른 마을로 떠나도 진행은 유지됩니다."),
			FText::AsNumber(Quests ? Quests->GetObjectiveProgress(QuestId) : 0), Row->RewardItem->DisplayName, FText::AsNumber(Row->RewardQuantity));
	}
	return FText::Format(NSLOCTEXT("Rogue10mVillageQuest", "Summary", "목표 진행 {0}/2 · 보상 이번 판 골드 {1}\n다른 마을로 떠나도 진행은 유지됩니다."),
		FText::AsNumber(Quests ? Quests->GetObjectiveProgress(QuestId) : 0), FText::AsNumber(Row->RewardGold));
}

void ARogue10mVillageQuestNPC::RefreshDialogueView()
{
	if (IsDialogueOpen()) { FindQuests(true); }
	auto* Widget = GetTutorialWidget(); const auto* Row = FindDefinition();
	if (!Widget || !Row) { return; }
	FString Description = Row->Description.ToString();
	if (IsDialogueOpen())
	{
		const auto* Quests = FindQuests(false);
		const auto* Character = GetDialogueCharacter();
		const auto State = Quests ? Quests->GetQuestState(QuestId) : ERogue10mVillageQuestState::Unavailable;
		if (State == ERogue10mVillageQuestState::Active) { Description = Row->InProgressText.ToString(); }
		else if (State == ERogue10mVillageQuestState::Ready) { Description = Row->ReadyText.ToString(); }
		else if (State == ERogue10mVillageQuestState::Claimed) { Description = Row->CompletedText.ToString(); }
		const auto Clue = Quests ? Quests->GetOptionalClueState() : ERogue10mVillageOptionalClueState::Missing;
		if (Row->bOffersClueAnalysis && Clue == ERogue10mVillageOptionalClueState::Missing)
		{
			Description += TEXT("\n");
			Description += QuestDefinition->OptionalStoryText.AnalystMissingHint.ToString();
		}
		else if (Row->bOffersClueAnalysis && Clue == ERogue10mVillageOptionalClueState::Analyzed)
		{
			Description += TEXT("\n");
			Description += QuestDefinition->OptionalStoryText.AnalystAnalyzedHint.ToString();
		}
		else if (Row->bAcceptsClueReport && Clue == ERogue10mVillageOptionalClueState::Found)
		{
			Description += TEXT("\n");
			Description += QuestDefinition->OptionalStoryText.SourceFoundHint.ToString();
		}
		else if (Row->bAcceptsClueReport && Clue == ERogue10mVillageOptionalClueState::Reported)
		{
			Description += TEXT("\n");
			Description += QuestDefinition->OptionalStoryText.SourceReportedHint.ToString();
		}
		for (int32 Index = 0; Index < Row->ObjectiveLabels.Num(); ++Index)
		{
			const bool bDone = Quests && Quests->HasObservedObjective(QuestId, Index);
			Description += FString::Printf(TEXT("\n%s %s"), bDone ? TEXT("[완료]") : TEXT("[ ]"), *Row->ObjectiveLabels[Index].ToString());
			if (!bDone && Character && Row->ObjectiveLocations.IsValidIndex(Index))
			{
				const double DistanceM = FVector::Dist2D(Character->GetActorLocation(), QuestDefinition->ToWorldLocation(Row->ObjectiveLocations[Index])) / 100.0;
				Description += FString::Printf(TEXT(" · 표식까지 약 %.0fm"), DistanceM);
			}
		}
	}
	Widget->SetDialogueView(Row->NpcName, FText::Format(NSLOCTEXT("Rogue10mVillageQuest", "NpcRole", "{0} · {1}"), Row->VillageName, Row->Title), FText::FromString(Description), GetDialogueStatus(),
		IsDialogueOpen() ? NSLOCTEXT("Rogue10mVillageQuest", "Controls", "클릭 또는 Enter로 선택 · F / Esc 닫기")
		: NSLOCTEXT("Rogue10mVillageQuest", "Prompt", "F  의뢰 대화하기"));
	Widget->SetServiceView(IsDialogueOpen() ? BuildServiceText() : FText::GetEmpty());
	Widget->SetServiceChoices(IsDialogueOpen() ? this : nullptr, BuildServiceChoices(), GetSelectedServiceIndex(), false);
}

void ARogue10mVillageQuestNPC::ExecuteSelectedService()
{
	ARogue10mCharacter* Character = GetDialogueCharacter();
	if (!IsDialogueOpen() || !IsPlayerAvailable(Character, InteractionRadius)) { return; }
	auto* Quests = FindQuests(true);
	if (!Quests) { return; }
	if (GetSelectedServiceIndex() == 1)
	{
		const auto* Row = FindDefinition();
		const auto Clue = Quests->GetOptionalClueState();
		bool bSucceeded = false;
		FText Success;
		if (Row && Row->bOffersClueAnalysis && Clue == ERogue10mVillageOptionalClueState::Found)
		{
			Success = QuestDefinition->OptionalStoryText.AnalysisSuccess;
			bSucceeded = Quests->AnalyzeOptionalClue(QuestId, Character);
		}
		else if (Row && Row->bAcceptsClueReport && Clue == ERogue10mVillageOptionalClueState::Analyzed)
		{
			Success = QuestDefinition->OptionalStoryText.ReportSuccess;
			bSucceeded = Quests->ReportOptionalClue(QuestId, Character);
		}
		// The optional action disappears after success; keep keyboard selection on a visible row.
		NavigateService(-GetSelectedServiceIndex());
		FinishService(bSucceeded, Success, NSLOCTEXT("Rogue10mVillageQuest", "ClueUnavailable", "지금은 이 기록 이야기를 진행할 수 없습니다."));
		return;
	}
	if (GetSelectedServiceIndex() != 0) { return; }
	const auto State = Quests->GetQuestState(QuestId);
	if (State == ERogue10mVillageQuestState::Available)
	{
		FinishService(Quests->AcceptQuest(QuestId, Character),
			NSLOCTEXT("Rogue10mVillageQuest", "Accepted", "의뢰를 수락했습니다. 목표를 마치고 돌아와 보고해 주세요."),
			NSLOCTEXT("Rogue10mVillageQuest", "AcceptFailed", "지금은 의뢰를 수락할 수 없습니다."));
	}
	else if (State == ERogue10mVillageQuestState::Ready)
	{
		FText Reason; const bool bGranted = Quests->ClaimReward(QuestId, Character, Reason);
		FinishService(bGranted, Reason, Reason);
	}
	else
	{
		FinishService(false, FText::GetEmpty(), State == ERogue10mVillageQuestState::Unavailable
			? NSLOCTEXT("Rogue10mVillageQuest", "InactiveRun", "탐험 진행 중에 의뢰를 이용할 수 있습니다.")
			: State == ERogue10mVillageQuestState::Claimed
			? NSLOCTEXT("Rogue10mVillageQuest", "AlreadyPaid", "이번 탐험에서 이미 보상을 받았습니다.")
			: NSLOCTEXT("Rogue10mVillageQuest", "KeepGoing", "두 목표를 마치면 완료 보고를 할 수 있습니다. 언제든 돌아오세요."));
	}
}
