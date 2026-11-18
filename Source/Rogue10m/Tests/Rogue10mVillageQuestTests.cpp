#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestModel.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestDefinitionsTest,
	"Rogue10m.VillageQuest.Definitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestDefinitionsTest::RunTest(const FString&)
{
	const auto* Defaults = GetDefault<URogue10mVillageQuestDataAsset>();
	if (!TestNotNull(TEXT("Quest defaults"), Defaults)) { return false; }
	TestTrue(TEXT("Quest defaults require an authored world"), Defaults->SupportedWorld.IsNull());
	auto* Data = NewObject<URogue10mVillageQuestDataAsset>();
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/VillageQuestDefinition.VillageQuestDefinition")));
	FString Reason;
	if (!TestNotNull(TEXT("Quest definitions"), Data) || !TestTrue(TEXT("Default definitions valid"), Data->ValidateDefinition(Reason))) { return false; }
	const FName Expected[] = {TEXT("Q_V1_supply"), TEXT("Q_V2_stolen_grain"), TEXT("Q_V3_quarry"), TEXT("Q_V4_record"), TEXT("Q_V5_embers")};
	TestEqual(TEXT("Five independent quests"), Data->Quests.Num(), Rogue10mVillageQuest::QuestCount);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
	{
		TestEqual(TEXT("Stable quest index"), Data->FindQuestIndex(Expected[Index]), Index);
		const auto& Row = Data->Quests[Index];
		TestEqual(TEXT("Unique village ID"), Row.VillageId, FName(*FString::Printf(TEXT("V%d"), Index + 1)));
		TestEqual(TEXT("Two objective labels"), Row.ObjectiveLabels.Num(), 2);
		TestEqual(TEXT("Two objective anchors"), Row.ObjectiveLocations.Num(), 2);
		TestFalse(TEXT("Village has a named NPC"), Row.NpcName.IsEmpty());
		TestFalse(TEXT("Active dialogue authored"), Row.InProgressText.IsEmpty());
		TestFalse(TEXT("Ready dialogue authored"), Row.ReadyText.IsEmpty());
		TestFalse(TEXT("Revisit after payment dialogue authored"), Row.CompletedText.IsEmpty());
		TestTrue(TEXT("Default reward is positive run currency"), Row.RewardGold > 0 && !Row.RewardItem);
		TestEqual(TEXT("Only quarry requires attributed combat"), Row.CombatObjectiveIndex, Index == 2 ? 1 : INDEX_NONE);
		TestEqual(TEXT("Transport record is the optional clue source"), Row.SupplyClueObjectiveIndex, Index == 0 ? 1 : INDEX_NONE);
		TestEqual(TEXT("Only records village offers optional analysis"), Row.bOffersClueAnalysis, Index == 3);
		TestEqual(TEXT("Only supply village accepts optional report"), Row.bAcceptsClueReport, Index == 0);
	}
	TestEqual(TEXT("Unknown quest rejected"), Data->FindQuestIndex(TEXT("Unknown")), INDEX_NONE);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestInvalidDataTest,
	"Rogue10m.VillageQuest.InvalidData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestInvalidDataTest::RunTest(const FString&)
{
	auto* Data = NewObject<URogue10mVillageQuestDataAsset>();
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/VillageQuestDefinition.VillageQuestDefinition")));
	const auto Original = Data->Quests; FString Reason;
	const auto Invalid = [this, Data, &Reason](const TCHAR* Message)
	{
		TestFalse(Message, Data->ValidateDefinition(Reason)); TestFalse(TEXT("Invalid data explains rejection"), Reason.IsEmpty());
	};
	Data->Quests.Pop(); Invalid(TEXT("Missing quest rejected")); Data->Quests = Original;
	Data->Quests[1].QuestId = Data->Quests[0].QuestId; Invalid(TEXT("Duplicate quest rejected")); Data->Quests = Original;
	Data->Quests[1].VillageId = Data->Quests[0].VillageId; Invalid(TEXT("Duplicate village rejected")); Data->Quests = Original;
	Data->Quests[0].ObjectiveLocations.Pop(); Invalid(TEXT("Missing objective rejected")); Data->Quests = Original;
	Data->Quests[0].ObjectiveLocations[0].X = std::numeric_limits<double>::infinity(); Invalid(TEXT("Nonfinite objective rejected")); Data->Quests = Original;
	Data->Quests[0].RewardGold = 0; Invalid(TEXT("Empty reward rejected")); Data->Quests = Original;
	Data->Quests[0].RewardGold = -1; Invalid(TEXT("Negative reward rejected")); Data->Quests = Original;
	Data->Quests[0].CombatObjectiveIndex = 2; Invalid(TEXT("Invalid combat goal rejected")); Data->Quests = Original;
	Data->Quests[0].CompletedText = FText::GetEmpty(); Invalid(TEXT("Missing revisit dialogue rejected")); Data->Quests = Original;
	Data->Quests[0].SupplyClueObjectiveIndex = 2; Invalid(TEXT("Invalid optional clue goal rejected")); Data->Quests = Original;
	Data->Quests[0].SupplyClueObjectiveIndex = INDEX_NONE; Invalid(TEXT("Partial optional story roles rejected")); Data->Quests = Original;
	Data->Quests[2].bOffersClueAnalysis = true; Invalid(TEXT("Ambiguous analysts rejected")); Data->Quests = Original;
	Data->Quests[0].bAcceptsClueReport = false; Data->Quests[1].bAcceptsClueReport = true;
	Invalid(TEXT("Story report must return to the source village")); Data->Quests = Original;
	for (auto& Row : Data->Quests) { Row.SupplyClueObjectiveIndex = INDEX_NONE; Row.bOffersClueAnalysis = false; Row.bAcceptsClueReport = false; }
	TestTrue(TEXT("Primary quests support disabling the optional story entirely"), Data->ValidateDefinition(Reason)); Data->Quests = Original;
	auto* Item = NewObject<URogue10mItemDataAsset>(); Item->MaxStackSize = 3;
	Data->Quests[0].RewardItem = Item;
	Invalid(TEXT("Mixed gold/item grant rejected to keep reward atomic"));
	Data->Quests[0].RewardGold = 0; Data->Quests[0].RewardQuantity = 4;
	Invalid(TEXT("Oversized item reward rejected"));
	Data->Quests[0].RewardQuantity = 2;
	TestTrue(TEXT("Item-only reward is supported"), Data->ValidateDefinition(Reason));
	return !HasAnyErrors();
}
#endif
