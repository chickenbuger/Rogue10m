#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mVillageQuestModel.h"
#include "Engine/World.h"

bool FRogue10mVillageObjectivePresentation::IsValid() const
{
	return FMath::IsFinite(RadiusCm) && RadiusCm > 0 && !MarkerScale.ContainsNaN()
		&& MarkerScale.X > 0 && MarkerScale.Y > 0 && MarkerScale.Z > 0
		&& !LabelOffset.ContainsNaN() && !LabelRotation.ContainsNaN()
		&& FMath::IsFinite(LabelWorldSize) && LabelWorldSize > 0;
}

URogue10mVillageQuestDataAsset::URogue10mVillageQuestDataAsset()
{
	SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.FiveVillageGraybox20261110")));
	const auto Add = [this](const TCHAR* Id, const TCHAR* Village, const TCHAR* Name,
		const TCHAR* Title, const TCHAR* Description, const TCHAR* First, const TCHAR* Second,
		const FVector& Center, int32 Gold, int32 CombatIndex = INDEX_NONE)
	{
		auto& Row = Quests.AddDefaulted_GetRef();
		Row.QuestId = FName(Id); Row.VillageId = FName(Village); Row.VillageName = FText::FromString(Name);
		Row.Title = FText::FromString(Title); Row.Description = FText::FromString(Description);
		Row.ObjectiveLabels = {FText::FromString(First), FText::FromString(Second)};
		// All markers stay on the authored village platform, away from the central boss road.
		Row.NpcLocation = Center + FVector(0, -500, 20);
		Row.ObjectiveLocations = {Center + FVector(-900, 400, 100), Center + FVector(900, 400, 100)};
		Row.RewardGold = Gold; Row.CombatObjectiveIndex = CombatIndex;
	};
	Add(TEXT("Q_V1_supply"), TEXT("V1"), TEXT("낙엽촌"), TEXT("뿌리길 보급품"),
		TEXT("흩어진 보급품과 운송 기록을 확인해 주세요. 다른 길로 떠나도 의뢰는 유지됩니다."),
		TEXT("보급 상자 회수"), TEXT("운송 기록 확인"), FVector(-14000, 8000, 0), 40);
	Add(TEXT("Q_V2_stolen_grain"), TEXT("V2"), TEXT("풍차장터"), TEXT("사라진 곡물"),
		TEXT("도난 흔적을 찾고 창고의 남은 곡물을 확인해 주세요."),
		TEXT("도난 흔적 조사"), TEXT("창고 곡물 확인"), FVector(14000, 8000, 0), 55);
	Add(TEXT("Q_V3_quarry"), TEXT("V3"), TEXT("돌샘촌"), TEXT("채석장 수련"),
		TEXT("채석장 표식을 조사하고 그곳의 일반 몬스터를 처치해 주세요."),
		TEXT("채석장 표식 조사"), TEXT("채석장 몬스터 처치"), FVector(-30000, 23000, 0), 80, 1);
	Add(TEXT("Q_V4_record"), TEXT("V4"), TEXT("기록촌"), TEXT("흩어진 현지 기록"),
		TEXT("현지 기록을 회수해 해석대에서 확인해 주세요. 다른 마을의 단서는 필요 없습니다."),
		TEXT("현지 기록 회수"), TEXT("해석대 확인"), FVector(6000, 23000, 0), 65);
	Add(TEXT("Q_V5_embers"), TEXT("V5"), TEXT("잔불촌"), TEXT("외곽의 잔불"),
		TEXT("유적 외곽 표식을 확인하고 잔불 보급 상자를 회수해 주세요."),
		TEXT("유적 외곽 조사"), TEXT("잔불 상자 회수"), FVector(30000, 23000, 0), 100);
	const TCHAR* Names[] = {TEXT("보급지기 소라"), TEXT("곡물상인 로한"), TEXT("채석장 교관 단"), TEXT("기록지기 유나"), TEXT("잔불 감시자 렌")};
	const TCHAR* ProgressLines[] = {
		TEXT("보급 상자와 운송 기록을 모두 확인해 주세요. 다른 마을을 먼저 다녀와도 여기서 기다리겠습니다."),
		TEXT("도난 흔적과 창고의 곡물을 확인하면 피해 규모를 알 수 있을 겁니다."),
		TEXT("채석장 표식을 살펴보고 근처 몬스터를 직접 처치해 보세요. 다른 마을 의뢰는 필요 없습니다."),
		TEXT("현지 기록과 해석대만 확인하면 됩니다. 낙엽촌에 가지 않았어도 이 의뢰는 끝낼 수 있어요."),
		TEXT("외곽 표식과 잔불 상자를 확인해 주세요. 제련로에 들어갈 필요는 없습니다.")};
	const TCHAR* ReadyLines[] = {
		TEXT("보급품과 기록을 모두 찾았군요. 물자가 더 흩어지기 전에 회수할 수 있겠습니다."),
		TEXT("이제 사라진 곡물의 흔적을 정리할 수 있겠어요. 약속한 보상을 드리겠습니다."),
		TEXT("표식 조사와 실전 수련을 모두 해냈군요. 수고한 만큼 보상하겠습니다."),
		TEXT("기록을 회수하고 해석까지 마쳤군요. 이 마을의 기억을 되찾았습니다."),
		TEXT("외곽과 보급 상자를 모두 확인했군요. 유적을 지켜볼 여유가 생겼습니다.")};
	const TCHAR* CompleteLines[] = {
		TEXT("덕분에 보급길을 다시 정리했습니다. 이번 탐험의 보상은 이미 받으셨어요. 북쪽 길은 언제든 열려 있습니다."),
		TEXT("곡물 기록은 정리해 두었습니다. 이번 의뢰 보상은 지급됐어요. 다른 길도 자유롭게 살펴보세요."),
		TEXT("오늘 수련은 충분합니다. 보상도 지급했으니 원하는 길로 나아가세요. 보스와 싸울지는 스스로 정하면 됩니다."),
		TEXT("되찾은 기록은 잘 보관하겠습니다. 이번 탐험의 의뢰는 마쳤어요. 다른 마을을 방문하지 않아도 괜찮습니다."),
		TEXT("외곽 소식 덕분에 준비를 마쳤습니다. 보상은 지급됐고, 제련로나 보스 쪽으로 가는 것은 자유입니다.")};
	for (int32 Index = 0; Index < Quests.Num(); ++Index)
	{
		Quests[Index].NpcName = FText::FromString(Names[Index]);
		Quests[Index].InProgressText = FText::FromString(ProgressLines[Index]);
		Quests[Index].ReadyText = FText::FromString(ReadyLines[Index]);
		Quests[Index].CompletedText = FText::FromString(CompleteLines[Index]);
	}
	Quests[0].SupplyClueObjectiveIndex = 1;
	Quests[0].bAcceptsClueReport = true;
	Quests[3].bOffersClueAnalysis = true;
	Quests[2].CombatMonsterData = TSoftObjectPtr<URogue10mMonsterDataAsset>(FSoftObjectPath(
		TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_VerdantSlime.DA_OW_VerdantSlime")));
}

bool URogue10mVillageQuestDataAsset::IsSupportedWorld(const UWorld* World) const
{
	return World && !SupportedWorld.IsNull()
		&& UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == SupportedWorld.ToSoftObjectPath().GetLongPackageName();
}

FVector URogue10mVillageQuestDataAsset::ToWorldLocation(const FVector& AuthoredLocation) const
{
	return LayoutOriginCm + LayoutRotation.RotateVector(AuthoredLocation);
}

FRotator URogue10mVillageQuestDataAsset::ToWorldRotation(const FRotator& AuthoredRotation) const
{
	return (LayoutRotation.Quaternion() * AuthoredRotation.Quaternion()).Rotator();
}

int32 URogue10mVillageQuestDataAsset::FindQuestIndex(FName QuestId) const
{
	return QuestId.IsNone() ? INDEX_NONE : Quests.IndexOfByPredicate(
		[QuestId](const auto& Row) { return Row.bEnabled && Row.QuestId == QuestId; });
}

int32 URogue10mVillageQuestDataAsset::GetEnabledQuestCount() const
{
	int32 Count = 0;
	for (const auto& Row : Quests) { if (Row.bEnabled) { ++Count; } }
	return Count;
}

void URogue10mVillageQuestDataAsset::ResetToIntroSupplyExample()
{
	// A template action must never rewrite the shared native default definition.
	if (HasAnyFlags(RF_ClassDefaultObject)) { return; }
#if WITH_EDITOR
	Modify();
	SupportedWorld.Reset();
	LayoutOriginCm = FVector::ZeroVector; LayoutRotation = FRotator::ZeroRotator;
	OptionalStoryText = FRogue10mVillageOptionalStoryText();
	Quests.Reset(); Quests.SetNum(Rogue10mVillageQuest::QuestCount);
	for (auto& Row : Quests) { Row.bEnabled = false; }
	auto& Row = Quests[0]; Row.bEnabled = true;
	Row.QuestId = TEXT("Q_Intro_supply"); Row.VillageId = TEXT("H");
	Row.VillageName = NSLOCTEXT("Rogue10mVillageQuest", "IntroVillage", "낙엽 수렵촌");
	Row.NpcName = NSLOCTEXT("Rogue10mVillageQuest", "IntroNpc", "서린");
	Row.Title = NSLOCTEXT("Rogue10mVillageQuest", "IntroTitle", "남겨진 보급 상자");
	Row.Description = NSLOCTEXT("Rogue10mVillageQuest", "IntroDescription", "근처에 보급 상자 두 개가 남아 있어요. 확인하고 돌아오시면 보수를 드릴게요.");
	Row.InProgressText = NSLOCTEXT("Rogue10mVillageQuest", "IntroProgress", "표시된 두 상자를 확인해 주세요. 먼저 찾은 상자도 이번 탐험의 기록에 남아 있어요.");
	Row.ReadyText = NSLOCTEXT("Rogue10mVillageQuest", "IntroReady", "상자를 모두 확인하셨네요. 보수를 받아 가세요.");
	Row.CompletedText = NSLOCTEXT("Rogue10mVillageQuest", "IntroComplete", "덕분에 보급품 위치를 알았어요. 이번 보수는 이미 드렸으니 편하게 둘러보세요.");
	Row.ObjectiveLabels = {
		NSLOCTEXT("Rogue10mVillageQuest", "IntroGoalOne", "첫 번째 보급 상자 확인"),
		NSLOCTEXT("Rogue10mVillageQuest", "IntroGoalTwo", "두 번째 보급 상자 확인")};
	// Small local authoring samples, never production-map coordinates or measured routes.
	Row.NpcLocation = FVector(0, 0, 20);
	Row.ObjectiveLocations = {FVector(-500, 400, 100), FVector(500, 400, 100)};
	Row.RewardGold = 40;
	ValidateConfiguration();
	MarkPackageDirty();
#endif
}

void URogue10mVillageQuestDataAsset::ValidateConfiguration()
{
	FString Reason;
	bLastValidationPassed = ValidateDefinition(Reason);
	LastValidationResult = bLastValidationPassed
		? NSLOCTEXT("Rogue10mVillageQuest", "DefinitionValid", "설정 형식 검사를 통과했습니다. 자산 로드와 실제 플레이는 별도로 확인해 주세요.")
		: FText::FromString(Reason);
}

bool URogue10mVillageQuestDataAsset::ValidateDefinition(FString& OutReason) const
{
	OutReason.Reset();
	if (SupportedWorld.IsNull() || !SupportedWorld.ToSoftObjectPath().IsValid())
	{
		OutReason = TEXT("Select a supported world asset for the quest definition."); return false;
	}
	if (LayoutOriginCm.ContainsNaN() || LayoutRotation.ContainsNaN())
	{
		OutReason = TEXT("Layout origin and rotation must contain finite values."); return false;
	}
	if (Quests.Num() != Rogue10mVillageQuest::QuestCount)
	{
		OutReason = TEXT("Keep five stable quest slots and enable the quests used by this definition."); return false;
	}
	TSet<FName> Ids, Villages;
	int32 Sources = 0, Analysts = 0, Reports = 0;
	FName SourceQuest, AnalystQuest, ReportQuest;
	for (const auto& Row : Quests)
	{
		if (!Row.bEnabled) { continue; }
		if (!FMath::IsFinite(Row.NpcInteractionRadiusCm) || Row.NpcInteractionRadiusCm < 100.0f
			|| !FMath::IsFinite(Row.NpcDetectionRadiusCm) || Row.NpcDetectionRadiusCm < Row.NpcInteractionRadiusCm + 50.0f)
		{
			OutReason = FString::Printf(TEXT("Quest %s: interaction radius needs at least 100cm; detection needs interaction + 50cm."), *Row.QuestId.ToString()); return false;
		}
		if (!Row.ObjectivePresentation.IsValid())
		{
			OutReason = FString::Printf(TEXT("Quest %s: goal radius, label size and scale must be positive; label transforms must be finite."), *Row.QuestId.ToString()); return false;
		}
		if (Row.CombatObjectiveIndex != INDEX_NONE
			&& (Row.CombatMonsterData.IsNull() || !Row.CombatMonsterData.ToSoftObjectPath().IsValid()))
		{
			OutReason = FString::Printf(TEXT("Quest %s: select ordinary monster data for the combat objective."), *Row.QuestId.ToString()); return false;
		}
		if (Row.QuestId.IsNone() || Row.VillageId.IsNone() || Ids.Contains(Row.QuestId) || Villages.Contains(Row.VillageId)
			|| Row.Title.IsEmpty() || Row.VillageName.IsEmpty() || Row.NpcName.IsEmpty() || Row.Description.IsEmpty()
			|| Row.InProgressText.IsEmpty() || Row.ReadyText.IsEmpty() || Row.CompletedText.IsEmpty()
			|| Row.SupplyClueObjectiveIndex < INDEX_NONE || Row.SupplyClueObjectiveIndex >= 2
			|| Row.ObjectiveLabels.Num() != 2 || Row.ObjectiveLocations.Num() != 2 || Row.NpcLocation.ContainsNaN()
			|| Row.NpcRotation.ContainsNaN()
			|| Row.CombatObjectiveIndex < INDEX_NONE || Row.CombatObjectiveIndex >= 2
			|| Row.RewardGold < 0 || (Row.RewardGold == 0 && !Row.RewardItem)
			|| (Row.RewardItem && (Row.RewardGold != 0 || Row.RewardQuantity <= 0
				|| Row.RewardQuantity > FMath::Max(1, Row.RewardItem->MaxStackSize))))
		{
			OutReason = TEXT("Quest IDs/villages must be unique; two objectives and valid rewards are required."); return false;
		}
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (Row.ObjectiveLabels[Index].IsEmpty() || Row.ObjectiveLocations[Index].ContainsNaN())
			{
				OutReason = TEXT("Objective text and finite positions are required."); return false;
			}
		}
		Ids.Add(Row.QuestId); Villages.Add(Row.VillageId);
		if (Row.SupplyClueObjectiveIndex != INDEX_NONE) { ++Sources; SourceQuest = Row.QuestId; }
		if (Row.bOffersClueAnalysis) { ++Analysts; AnalystQuest = Row.QuestId; }
		if (Row.bAcceptsClueReport) { ++Reports; ReportQuest = Row.QuestId; }
	}
	if (Ids.IsEmpty())
	{
		OutReason = TEXT("Enable at least one quest slot before using this definition."); return false;
	}
	if ((Sources + Analysts + Reports > 0)
		&& (Sources != 1 || Analysts != 1 || Reports != 1 || SourceQuest != ReportQuest || SourceQuest == AnalystQuest))
	{
		OutReason = TEXT("Optional story needs one source/report quest and one separate analyst, or no story roles."); return false;
	}
	if (Sources > 0 && !OptionalStoryText.IsValid())
	{
		OutReason = TEXT("Fill all optional story action, hint and success texts, or disable all optional story roles."); return false;
	}
	return true;
}
