#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestObjective.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace Rogue10mVillageQuestTuningTests
{
// Scope checks need only package identity. These are bare transient objects,
// never initialized as maps and never passed to a package/asset load API.
struct FScopeWorld
{
	TStrongObjectPtr<UPackage> Package;
	TStrongObjectPtr<UWorld> World;

	explicit FScopeWorld(const FString& PackageName)
		: Package(CreatePackage(*PackageName)),
		  World(NewObject<UWorld>(Package.Get(), TEXT("ScopeWorld"), RF_Transient))
	{
		Package->SetFlags(RF_Transient);
	}
};

struct FMonsterWorld
{
	UWorld* World = nullptr;

	FMonsterWorld()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.InitializeScenes(false).CreatePhysicsScene(false).CreateFXSystem(false)
			.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false,
			FName(*FString::Printf(TEXT("VillageQuestTuning_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
			GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
		// Keep the world, its actors and components before BeginPlay. No GI,
		// profile subsystem, player, Blueprint, map or soft asset is initialized.
	}

	~FMonsterWorld()
	{
		if (World) { World->BeginTearingDown(); World->DestroyWorld(false); }
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestLayoutTuningTest,
	"Rogue10m.VillageQuest.Tuning.AnchoredLayoutPreservesCentimetres", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestLayoutTuningTest::RunTest(const FString&)
{
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Data(NewObject<URogue10mVillageQuestDataAsset>());
	const FVector Local(300, -400, 50);
	const FRotator LocalRotation(23, -47, 11);
	const auto OriginalQuests = Data->Quests;
	TestTrue(TEXT("Default anchor preserves authored centimetres"), Data->ToWorldLocation(Local).Equals(Local));
	TestTrue(TEXT("Default anchor preserves authored orientation"), Data->ToWorldRotation(LocalRotation).Equals(LocalRotation));
	for (const auto& Row : Data->Quests)
	{
		TestTrue(TEXT("Default NPC position is unchanged"), Data->ToWorldLocation(Row.NpcLocation).Equals(Row.NpcLocation));
		for (const FVector& Location : Row.ObjectiveLocations)
		{
			TestTrue(TEXT("Default objective position is unchanged"), Data->ToWorldLocation(Location).Equals(Location));
		}
	}
	Data->LayoutOriginCm = FVector(1000, 2000, -25);
	Data->LayoutRotation = FRotator(0, 90, 0);
	TestTrue(TEXT("Local origin maps exactly to the editable anchor"), Data->ToWorldLocation(FVector::ZeroVector).Equals(FVector(1000, 2000, -25)));
	TestTrue(TEXT("Quarter turn rotates before translating"), Data->ToWorldLocation(Local).Equals(FVector(1400, 2300, 25), .001));
	TestTrue(TEXT("Rigid layout preserves travel distance without scaling"),
		FMath::IsNearlyEqual(FVector::Distance(Data->ToWorldLocation(Local), Data->ToWorldLocation(-Local)), 2.0 * Local.Size(), .001));
	const FRotator WorldRotation = Data->ToWorldRotation(FRotator(90, 0, 0));
	TestTrue(TEXT("Local pitch then anchor yaw points forward up"), WorldRotation.RotateVector(FVector::ForwardVector).Equals(FVector::UpVector, .001));
	TestTrue(TEXT("Noncommuting local pitch then yaw maps right backward"), WorldRotation.RotateVector(FVector::RightVector).Equals(-FVector::ForwardVector, .001));
	TestTrue(TEXT("Noncommuting local pitch then yaw maps up left"), WorldRotation.RotateVector(FVector::UpVector).Equals(-FVector::RightVector, .001));
	for (int32 Index = 0; Index < OriginalQuests.Num(); ++Index)
	{
		TestTrue(TEXT("World conversion never rewrites authored NPC positions"), Data->Quests[Index].NpcLocation.Equals(OriginalQuests[Index].NpcLocation));
		TestTrue(TEXT("World conversion never rewrites authored objective positions"), Data->Quests[Index].ObjectiveLocations == OriginalQuests[Index].ObjectiveLocations);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestWorldScopeTuningTest,
	"Rogue10m.VillageQuest.Tuning.EditableWorldScopeAndPIEIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestWorldScopeTuningTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestTuningTests;
	const FString Root = TEXT("/Temp/VillageQuestScope_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FScopeWorld Selected(Root / TEXT("Selected"));
	FScopeWorld PIE(Root / TEXT("UEDPIE_7_Selected"));
	FScopeWorld Other(Root / TEXT("Other"));
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Data(NewObject<URogue10mVillageQuestDataAsset>());
	if (!TestTrue(TEXT("Scope fixtures have no initialized maps or GameInstances"),
		Selected.World.IsValid() && PIE.World.IsValid() && Other.World.IsValid()
		&& !Selected.World->IsInitialized() && !PIE.World->IsInitialized() && !Other.World->IsInitialized()
		&& !Selected.World->GetGameInstance() && !PIE.World->GetGameInstance() && !Other.World->GetGameInstance())) { return false; }
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(Selected.World->GetPathName()));
	TestTrue(TEXT("Edited world selection matches its package"), Data->IsSupportedWorld(Selected.World.Get()));
	TestTrue(TEXT("PIE copy matches the configured source package"), Data->IsSupportedWorld(PIE.World.Get()));
	TestFalse(TEXT("Different map package cannot attach quests"), Data->IsSupportedWorld(Other.World.Get()));
	TestFalse(TEXT("Null world is rejected"), Data->IsSupportedWorld(nullptr));
	Data->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(Other.World->GetPathName()));
	TestTrue(TEXT("Changing selection immediately enables the other world"), Data->IsSupportedWorld(Other.World.Get()));
	TestFalse(TEXT("Changing selection immediately excludes the old world"), Data->IsSupportedWorld(Selected.World.Get()));
	Data->SupportedWorld.Reset();
	TestFalse(TEXT("Empty world scope never enables the current map"), Data->IsSupportedWorld(Selected.World.Get()));
	TestFalse(TEXT("Empty world scope also rejects null"), Data->IsSupportedWorld(nullptr));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestValidationTuningTest,
	"Rogue10m.VillageQuest.Tuning.RejectsInvalidEditsAndAllowsExploration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestValidationTuningTest::RunTest(const FString&)
{
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Data(NewObject<URogue10mVillageQuestDataAsset>());
	const TSoftObjectPtr<UWorld> FixtureWorld(FSoftObjectPath(TEXT("/Temp/VillageQuestTuning.VillageQuestTuning")));
	Data->SupportedWorld = FixtureWorld;
	FString Reason;
	if (!TestTrue(TEXT("Explicitly scoped definition is structurally valid"), Data->ValidateDefinition(Reason))) { return false; }
	const auto Reject = [this, &FixtureWorld](const TCHAR* Message, TFunctionRef<void(URogue10mVillageQuestDataAsset&)> Edit)
	{
		TStrongObjectPtr<URogue10mVillageQuestDataAsset> Candidate(NewObject<URogue10mVillageQuestDataAsset>());
		Candidate->SupportedWorld = FixtureWorld;
		Edit(*Candidate);
		FString RejectionReason;
		TestFalse(Message, Candidate->ValidateDefinition(RejectionReason));
		TestFalse(FString(Message) + TEXT(" supplies a correction reason"), RejectionReason.IsEmpty());
	};
	Reject(TEXT("Unselected target world rejected"), [](auto& D) { D.SupportedWorld.Reset(); });
	Reject(TEXT("NaN layout origin rejected"), [](auto& D) { D.LayoutOriginCm.X = std::numeric_limits<double>::quiet_NaN(); });
	Reject(TEXT("Infinite anchor rotation rejected"), [](auto& D) { D.LayoutRotation.Yaw = std::numeric_limits<double>::infinity(); });
	Reject(TEXT("NaN NPC rotation rejected"), [](auto& D) { D.Quests[0].NpcRotation.Roll = std::numeric_limits<double>::quiet_NaN(); });
	Reject(TEXT("Negative NPC interaction radius rejected"), [](auto& D) { D.Quests[0].NpcInteractionRadiusCm = -1; });
	Reject(TEXT("NaN NPC detection radius rejected"), [](auto& D) { D.Quests[0].NpcDetectionRadiusCm = std::numeric_limits<float>::quiet_NaN(); });
	Reject(TEXT("Detection sphere must exceed interaction by fifty centimetres"), [](auto& D) { D.Quests[0].NpcDetectionRadiusCm = D.Quests[0].NpcInteractionRadiusCm + 49; });
	Reject(TEXT("Negative objective radius rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.RadiusCm = -1; });
	Reject(TEXT("NaN objective radius rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.RadiusCm = std::numeric_limits<float>::quiet_NaN(); });
	Reject(TEXT("Zero marker scale rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.MarkerScale.X = 0; });
	Reject(TEXT("Negative marker scale rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.MarkerScale.Y = -1; });
	Reject(TEXT("Infinite marker scale rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.MarkerScale.Z = std::numeric_limits<double>::infinity(); });
	Reject(TEXT("NaN label offset rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.LabelOffset.X = std::numeric_limits<double>::quiet_NaN(); });
	Reject(TEXT("Invalid label size rejected"), [](auto& D) { D.Quests[0].ObjectivePresentation.LabelWorldSize = 0; });
	Reject(TEXT("Combat goal needs selected monster data"), [](auto& D)
	{
		for (auto& Row : D.Quests) { if (Row.CombatObjectiveIndex != INDEX_NONE) { Row.CombatMonsterData.Reset(); } }
	});
	using FStory = FRogue10mVillageOptionalStoryText;
	FText FStory::* const StoryFields[] = {&FStory::AnalyzeAction, &FStory::ReportAction,
		&FStory::AnalysisSuccess, &FStory::ReportSuccess, &FStory::AnalystMissingHint,
		&FStory::AnalystAnalyzedHint, &FStory::SourceFoundHint, &FStory::SourceReportedHint};
	for (FText FStory::* Field : StoryFields)
	{
		Reject(TEXT("Each active optional-story caption is required"), [Field](auto& D) { D.OptionalStoryText.*Field = FText::GetEmpty(); });
	}
	// These are actual accepted configuration boundaries, not editor ClampMin metadata.
	Data->Quests[0].NpcInteractionRadiusCm = 100;
	Data->Quests[0].NpcDetectionRadiusCm = 150;
	Data->Quests[0].ObjectivePresentation.RadiusCm = 1;
	Data->Quests[0].ObjectivePresentation.LabelWorldSize = 1;
	Data->Quests[0].ObjectivePresentation.bShowMarker = false;
	Data->Quests[0].ObjectivePresentation.bShowLabel = false;
	for (auto& Row : Data->Quests)
	{
		Row.CombatObjectiveIndex = INDEX_NONE;
		Row.CombatMonsterData.Reset();
		Row.SupplyClueObjectiveIndex = INDEX_NONE;
		Row.bOffersClueAnalysis = false;
		Row.bAcceptsClueReport = false;
	}
	for (FText FStory::* Field : StoryFields) { Data->OptionalStoryText.*Field = FText::GetEmpty(); }
	Reason = TEXT("stale invalid result");
	TestTrue(TEXT("Exploration-only goals and disabled story need no combat asset or story text"), Data->ValidateDefinition(Reason));
	TestTrue(TEXT("Successful validation clears the prior reason"), Reason.IsEmpty());
	Data->ValidateConfiguration();
	const FText ValidResult = Data->LastValidationResult;
	TestFalse(TEXT("Editor validation exposes a visible result"), ValidResult.IsEmpty());
	Data->Quests[0].ObjectivePresentation.RadiusCm = -1;
	Data->ValidateDefinition(Reason);
	Data->ValidateConfiguration();
	TestEqual(TEXT("Editor action shows the current rejection reason"), Data->LastValidationResult.ToString(), Reason);
	Data->Quests[0].ObjectivePresentation.RadiusCm = 1;
	Data->ValidateConfiguration();
	TestTrue(TEXT("Fixing the edit replaces the stale error result"), Data->LastValidationResult.EqualTo(ValidResult));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestMonsterTuningTest,
	"Rogue10m.VillageQuest.Tuning.OrdinaryMonsterConfigurationBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestMonsterTuningTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestTuningTests;
	FMonsterWorld Fixture;
	if (!TestTrue(TEXT("Native monster world is transient without GI or BeginPlay"), Fixture.World && Fixture.World->IsInitialized()
		&& !Fixture.World->GetGameInstance() && !Fixture.World->HasBegunPlay() && !Fixture.World->AreActorsInitialized())) { return false; }
	auto* Monster = Fixture.World->SpawnActorDeferred<ARogue10mVillageQuestMonster>(ARogue10mVillageQuestMonster::StaticClass(),
		FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Deferred native quest monster"), Monster)) { return false; }
	Monster->AutoPossessAI = EAutoPossessAI::Disabled;
	TStrongObjectPtr<URogue10mMonsterDataAsset> Ordinary(NewObject<URogue10mMonsterDataAsset>());
	TStrongObjectPtr<URogue10mMonsterDataAsset> Replacement(NewObject<URogue10mMonsterDataAsset>());
	TStrongObjectPtr<URogue10mMonsterDataAsset> Boss(NewObject<URogue10mMonsterDataAsset>());
	TestFalse(TEXT("Empty selection rejected before construction finishes"), Monster->ConfigureQuestMonsterData(nullptr));
	TestTrue(TEXT("Normal authored monster is accepted"), Monster->ConfigureQuestMonsterData(Ordinary.Get()));
	for (ERogue10mMonsterRank Rank : {ERogue10mMonsterRank::MidBoss, ERogue10mMonsterRank::FinalBoss})
	{
		Boss->MonsterRank = Rank;
		TestFalse(TEXT("Quest encounter rejects boss data"), Monster->ConfigureQuestMonsterData(Boss.Get()));
		TestTrue(TEXT("Rejected boss edit preserves the previously selected ordinary data"), Monster->GetMonsterData() == Ordinary.Get());
	}
	TestFalse(TEXT("Clearing a valid encounter selection is rejected"), Monster->ConfigureQuestMonsterData(nullptr));
	TestTrue(TEXT("Rejected empty selection preserves the configured data"), Monster->GetMonsterData() == Ordinary.Get());
	TestTrue(TEXT("A second ordinary asset remains an editable choice"), Monster->ConfigureQuestMonsterData(Replacement.Get()));
	Monster->FinishSpawning(FTransform::Identity);
	TestTrue(TEXT("Native construction retains exactly the selected authored data"), Monster->GetMonsterData() == Replacement.Get());
	TestFalse(TEXT("Fixture never starts monster gameplay or asset-loading BeginPlay"), Monster->HasActorBegunPlay());
	TestNull(TEXT("Fixture never creates an AI controller"), Monster->GetController());
	return !HasAnyErrors();
}
#endif
