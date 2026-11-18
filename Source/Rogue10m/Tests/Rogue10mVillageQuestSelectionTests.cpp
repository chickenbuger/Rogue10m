#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestModel.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestSelectedSlotsTest,
	"Rogue10m.VillageQuest.Selection.DisabledDraftsKeepStableIndices", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestSelectedSlotsTest::RunTest(const FString&)
{
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Data(NewObject<URogue10mVillageQuestDataAsset>());
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/VillageQuestSelection.VillageQuestSelection")));
	auto Selected = Data->Quests[0];
	Selected.SupplyClueObjectiveIndex = INDEX_NONE; Selected.bAcceptsClueReport = false;
	for (auto& Row : Data->Quests)
	{
		Row.bEnabled = false;
		Row.QuestId = Selected.QuestId; Row.VillageId = Selected.VillageId;
		Row.Description = FText::GetEmpty(); Row.ObjectiveLocations.Reset();
		Row.CombatObjectiveIndex = 1; Row.CombatMonsterData.Reset();
		Row.NpcInteractionRadiusCm = std::numeric_limits<float>::quiet_NaN();
	}
	Data->Quests[4] = Selected;
	FString Reason;
	TestEqual(TEXT("Only the authored quest is enabled"), Data->GetEnabledQuestCount(), 1);
	TestTrue(TEXT("Incomplete duplicate disabled drafts cannot block the selected quest"), Data->ValidateDefinition(Reason));
	TestEqual(TEXT("Lookup skips earlier disabled duplicate IDs without compacting slots"), Data->FindQuestIndex(Selected.QuestId), 4);
	TestEqual(TEXT("Unknown ID remains unavailable"), Data->FindQuestIndex(TEXT("DisabledUnknown")), INDEX_NONE);
	Data->Quests[0] = Selected;
	TestFalse(TEXT("Enabling a duplicate now rejects the definition"), Data->ValidateDefinition(Reason));
	Data->Quests[0].bEnabled = false;
	Data->Quests[4].bOffersClueAnalysis = true;
	TestFalse(TEXT("An enabled partial story chain is still rejected"), Data->ValidateDefinition(Reason));
	Data->Quests[4].bOffersClueAnalysis = false;
	TestTrue(TEXT("Turning off the partial chain restores the selected definition"), Data->ValidateDefinition(Reason));
	Data->Quests[4].bEnabled = false;
	TestEqual(TEXT("All disabled slots are unavailable"), Data->FindQuestIndex(Selected.QuestId), INDEX_NONE);
	TestFalse(TEXT("No enabled quest is an invalid definition"), Data->ValidateDefinition(Reason));
	TestFalse(TEXT("Empty selection gives a correction reason"), Reason.IsEmpty());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestIntroExampleTest,
	"Rogue10m.VillageQuest.Selection.IntroExampleRequiresExplicitWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestIntroExampleTest::RunTest(const FString&)
{
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Data(NewObject<URogue10mVillageQuestDataAsset>());
	Data->LayoutOriginCm = FVector(10000, 20000, 30000); Data->LayoutRotation = FRotator(10, 20, 30);
	Data->ResetToIntroSupplyExample();
	TestEqual(TEXT("Internal slot capacity stays stable"), Data->Quests.Num(), Rogue10mVillageQuest::QuestCount);
	TestEqual(TEXT("One introductory NPC quest is enabled"), Data->GetEnabledQuestCount(), 1);
	TestTrue(TEXT("Template explicitly clears the legacy map binding"), Data->SupportedWorld.IsNull());
	TestFalse(TEXT("An unbound example cannot pass validation"), Data->bLastValidationPassed);
	TestFalse(TEXT("World-selection requirement is visible"), Data->LastValidationResult.IsEmpty());
	TestTrue(TEXT("Template clears inherited production anchor"), Data->LayoutOriginCm.IsZero() && Data->LayoutRotation.IsZero());
	const auto& Row = Data->Quests[0];
	TestEqual(TEXT("Stable introductory quest ID"), Data->FindQuestIndex(TEXT("Q_Intro_supply")), 0);
	TestEqual(TEXT("Example uses guide NPC name"), Row.NpcName.ToString(), FString(TEXT("서린")));
	TestTrue(TEXT("Intro collection has two distinct fixed positions"), Row.ObjectiveLocations.Num() == 2
		&& !Row.ObjectiveLocations[0].Equals(Row.ObjectiveLocations[1]));
	TestTrue(TEXT("Intro reward is positive gold without a book or item grant"), Row.RewardGold > 0 && !Row.RewardItem);
	for (int32 Index = 0; Index < Data->Quests.Num(); ++Index)
	{
		const auto& Slot = Data->Quests[Index];
		TestEqual(TEXT("Only slot zero is active"), Slot.bEnabled, Index == 0);
		TestTrue(TEXT("Fresh slots carry no inherited combat or story roles"), Slot.CombatObjectiveIndex == INDEX_NONE
			&& Slot.CombatMonsterData.IsNull() && Slot.SupplyClueObjectiveIndex == INDEX_NONE
			&& !Slot.bOffersClueAnalysis && !Slot.bAcceptsClueReport);
	}
	// Structural scope only: no world or asset is loaded for this test.
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/IntroQuestScope.IntroQuestScope")));
	Data->ValidateConfiguration();
	TestTrue(TEXT("Explicit world choice makes the intro structurally valid"), Data->bLastValidationPassed);
	Data->Quests[0].RewardGold = 0;
	Data->ValidateConfiguration();
	TestFalse(TEXT("Machine-readable validation follows later invalid edits"), Data->bLastValidationPassed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestLegacyDefaultTest,
	"Rogue10m.VillageQuest.Selection.NativeDefaultsSurviveTemplateAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestLegacyDefaultTest::RunTest(const FString&)
{
	auto* Defaults = GetMutableDefault<URogue10mVillageQuestDataAsset>();
	const auto WorldBefore = Defaults->SupportedWorld;
	const auto QuestsBefore = Defaults->Quests;
	Defaults->ResetToIntroSupplyExample();
	TestEqual(TEXT("Legacy defaults remain enabled for existing fixtures"), Defaults->GetEnabledQuestCount(), Rogue10mVillageQuest::QuestCount);
	TestTrue(TEXT("Template action cannot change native default world binding"), Defaults->SupportedWorld == WorldBefore);
	for (int32 Index = 0; Index < QuestsBefore.Num(); ++Index)
	{
		TestEqual(TEXT("Template action leaves each default quest ID unchanged"), Defaults->Quests[Index].QuestId, QuestsBefore[Index].QuestId);
		TestEqual(TEXT("Default quest index remains stable"), Defaults->FindQuestIndex(QuestsBefore[Index].QuestId), Index);
		TestTrue(TEXT("Default NPC coordinate remains unchanged"), Defaults->Quests[Index].NpcLocation.Equals(QuestsBefore[Index].NpcLocation));
	}
	FString Reason;
	TestTrue(TEXT("Shared quest default is unbound"), Defaults->SupportedWorld.IsNull());
	TestFalse(TEXT("An explicit world is needed to validate the default rows"), Defaults->ValidateDefinition(Reason));
	return !HasAnyErrors();
}
#endif
