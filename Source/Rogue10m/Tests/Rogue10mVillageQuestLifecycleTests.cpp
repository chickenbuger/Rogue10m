#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestObjective.h"
#include "Rogue10mVillageQuestRewardTestProxy.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mVillageQuestLifecycleTests
{
static bool SameInventory(const FRogue10mInventorySnapshot& A, const FRogue10mInventorySnapshot& B)
{
	return FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mCatalogTestCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	ARogue10mGameState* Run = nullptr;
	URogue10mVillageQuestComponent* Quests = nullptr;
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Definition;
	TStrongObjectPtr<URogue10mItemDataAsset> RewardItem;
	FString SetupError;

	explicit FFixture(bool bItemReward = false, bool bSupplyOnly = false)
	{
		// Same minimal native setup as the reward fixture. No GI, map/Blueprint
		// loads, save APIs, world tick, or character gameplay initialization.
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.InitializeScenes(false).CreatePhysicsScene(false).CreateFXSystem(false)
			.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false,
			FName(*FString::Printf(TEXT("VillageQuestLifecycle_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
			GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
		if (!World || World->GetGameInstance()) { SetupError = TEXT("Expected transient world without a GameInstance"); return; }
		World->InitializeActorsForPlay(FURL());
		Run = World->SpawnActor<ARogue10mGameState>();
		if (!Run) { SetupError = TEXT("Native run spawn failed"); return; }
		World->SetGameState(Run);
		State = World->SpawnActor<ARogue10mPlayerState>();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Character = World->SpawnActor<ARogue10mCatalogTestCharacter>(
			ARogue10mCatalogTestCharacter::StaticClass(), FTransform::Identity, Spawn);
		if (!State || !Character) { SetupError = TEXT("Native player spawn failed"); return; }
		Character->SetPlayerState(State);
		auto* ASC = State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet());
		ASC->InitAbilityActorInfo(State, Character);
		State->GetRogueAttributeSet()->InitializeVitals(135.0f, 100.0f, 100.0f);

		Definition.Reset(NewObject<URogue10mVillageQuestDataAsset>());
		if (bSupplyOnly)
		{
			for (int32 Index = 0; Index < Definition->Quests.Num(); ++Index)
			{
				auto& Row = Definition->Quests[Index];
				Row.bEnabled = Index == 0;
				Row.SupplyClueObjectiveIndex = INDEX_NONE;
				Row.bOffersClueAnalysis = false;
				Row.bAcceptsClueReport = false;
			}
		}
		// Public attachment checks package identity without loading the soft world.
		Definition->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(World->GetPathName()));
		if (bItemReward)
		{
			RewardItem.Reset(NewObject<URogue10mItemDataAsset>());
			RewardItem->ItemId = TEXT("VillageQuestLifecycleReward");
			RewardItem->InventorySize = FIntPoint(1, 1);
			RewardItem->MaxStackSize = 5;
			Definition->Quests[0].RewardGold = 0;
			Definition->Quests[0].RewardItem = RewardItem.Get();
			Definition->Quests[0].RewardQuantity = 2;
		}
		if (!Definition->ValidateDefinition(SetupError)) { return; }
		Run->StartRun();
		Quests = URogue10mVillageQuestComponent::FindOrCreate(Character, Definition.Get());
		if (!Quests) { SetupError = TEXT("Public component attachment failed"); return; }
		State->DispatchBeginPlay();
	}

	~FFixture()
	{
		if (!World) { return; }
		World->BeginTearingDown();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->HasActorBegunPlay()) { It->RouteEndPlay(EEndPlayReason::Quit); }
		}
		World->DestroyWorld(false);
	}

	bool Check(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(FString(TEXT("Initialized no-GI public-attachment fixture: ")) + SetupError,
			SetupError.IsEmpty() && World && World->IsInitialized() && World->AreActorsInitialized()
			&& !World->GetGameInstance() && !World->HasBegunPlay() && Character && State && Run && Quests
			&& !Character->HasActorBegunPlay() && State->HasActorBegunPlay() && Quests->HasBegunPlay()
			&& State->GetPawn() == Character && Character->GetPlayerState() == State && !Character->IsDead()
			&& World->GetGameState() == Run && Run->IsRunActive() && Quests->GetOwner() == State
			&& Quests->GetDefinition() == Definition.Get()
			&& Quests->GetQuestState(Id(0)) == ERogue10mVillageQuestState::Available);
	}

	FName Id(int32 Index) const { return Definition->Quests[Index].QuestId; }
	URogue10mInventoryComponent& Inventory() const { return *Character->GetInventoryComponent(); }

	bool Ready(FAutomationTestBase& Test, int32 Index)
	{
		return Test.TestTrue(TEXT("Public acceptance succeeds"), Quests->AcceptQuest(Id(Index), Character))
			&& Test.TestTrue(TEXT("First public observation succeeds"), Quests->ObserveObjective(Id(Index), 0, Character))
			&& Test.TestTrue(TEXT("Second public observation succeeds"), Quests->ObserveObjective(Id(Index), 1, Character))
			&& Test.TestTrue(TEXT("Accepted completed quest is Ready"), Quests->GetQuestState(Id(Index)) == ERogue10mVillageQuestState::Ready);
	}

	bool CompleteStory(FAutomationTestBase& Test)
	{
		return (Quests->HasObservedObjective(Id(0), 1)
			|| Test.TestTrue(TEXT("Source is observed before optional analysis"), Quests->ObserveObjective(Id(0), 1, Character)))
			&& Test.TestTrue(TEXT("Configured analyst accepts the clue"), Quests->AnalyzeOptionalClue(Id(3), Character))
			&& Test.TestTrue(TEXT("Configured source accepts the report"), Quests->ReportOptionalClue(Id(0), Character));
	}

	bool CheckReset(FAutomationTestBase& Test) const
	{
		bool bValid = Test.TestTrue(TEXT("New-run boundary clears optional story"),
			Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Missing);
		for (int32 Index = 0; Index < Definition->Quests.Num(); ++Index)
		{
			const FString Context = Id(Index).ToString();
			const auto Expected = Definition->Quests[Index].bEnabled
				? ERogue10mVillageQuestState::Available : ERogue10mVillageQuestState::Unavailable;
			bValid &= Test.TestTrue(Context + TEXT(" has its fresh-run enabled/disabled state"),
				Quests->GetQuestState(Id(Index)) == Expected);
			bValid &= Test.TestEqual(Context + TEXT(" has no prior-run progress"), Quests->GetObjectiveProgress(Id(Index)), 0);
			for (int32 Objective = 0; Objective < 2; ++Objective)
			{
				bValid &= Test.TestFalse(Context + TEXT(" has no prior-run objective fact"), Quests->HasObservedObjective(Id(Index), Objective));
			}
		}
		return bValid;
	}

	ARogue10mVillageQuestObjective* SpawnCollector(int32 QuestIndex, int32 ObjectiveIndex)
	{
		auto* Collector = World->SpawnActorDeferred<ARogue10mVillageQuestObjective>(
			ARogue10mVillageQuestObjective::StaticClass(), FTransform::Identity, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Collector) { return nullptr; }
		Collector->QuestId = Id(QuestIndex);
		Collector->ObjectiveIndex = ObjectiveIndex;
		Collector->QuestDefinition = Definition.Get();
		Collector->ObjectiveLabel = Definition->Quests[QuestIndex].ObjectiveLabels[ObjectiveIndex];
		Collector->Presentation = Definition->Quests[QuestIndex].ObjectivePresentation;
		Collector->FinishSpawning(FTransform::Identity);
		Collector->DispatchBeginPlay();
		return Collector;
	}

	bool EmitCollectorEvent(FAutomationTestBase& Test, ARogue10mVillageQuestObjective* Collector)
	{
		auto* Area = Collector ? Collector->FindComponentByClass<USphereComponent>() : nullptr;
		if (!Test.TestTrue(TEXT("Native collector has begun play and bound its sphere"),
			Collector && Collector->HasActorBegunPlay() && Area)) { return false; }
		// Explicit event injection: tests the real collector callback/FindOrCreate
		// route, not physical overlap detection in a world without a physics scene.
		Area->OnComponentBeginOverlap.Broadcast(Area, Character, Character->GetCapsuleComponent(), 0, false, FHitResult());
		return true;
	}

	int32 RewardQuantity() const
	{
		int32 Quantity = 0;
		for (const auto& Container : Inventory().GetInventoryContainers())
		{
			for (const auto& Entry : Container.Entries)
			{
				if (RewardItem.IsValid() && Entry.ItemData == RewardItem.Get()) { Quantity += Entry.Quantity; }
			}
		}
		return Quantity;
	}
};

static bool CheckReentrantReset(FAutomationTestBase& Test, bool bItemReward)
{
	FFixture F(bItemReward);
	if (!F.Check(Test) || !F.Ready(Test, 0) || !F.Ready(Test, 1) || !F.CompleteStory(Test)) { return false; }
	F.Inventory().AddGold(17);
	const int32 Award = F.Definition->Quests[0].RewardGold;
	int32 InventoryCallbacks = 0, QuestCallbacks = 0;
	bool bInsideClaim = false;
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> InventoryObserver(NewObject<URogue10mVillageQuestRewardTestProxy>());
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> QuestObserver(NewObject<URogue10mVillageQuestRewardTestProxy>());
	InventoryObserver->OnNotification = [&]
	{
		++InventoryCallbacks;
		if (InventoryCallbacks > 1) { return; } // Regression protection against recursive notifications.
		Test.TestTrue(TEXT("Actual inventory notification occurs inside ClaimReward"), bInsideClaim);
		// GameState has no public same-instance restart API. Inject only the
		// production Running notification to test the component's reset boundary.
		F.Run->OnRunPhaseChanged.Broadcast(ERogue10mRunPhase::Running);
		Test.TestEqual(TEXT("Reset remains deferred while payout is notifying observers"), F.Quests->GetObjectiveProgress(F.Id(0)), 2);
		Test.TestTrue(TEXT("Other quest progress remains readable until grant returns"), F.Quests->GetQuestState(F.Id(1)) == ERogue10mVillageQuestState::Ready);
		Test.TestTrue(TEXT("Optional story reset is also deferred"), F.Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Reported);
		Test.TestEqual(TEXT("Running notification does not expose an intermediate quest reset"), QuestCallbacks, 0);
		FText NestedReason;
		Test.TestFalse(TEXT("Reset request cannot reopen another reward during the grant"), F.Quests->ClaimReward(F.Id(1), F.Character, NestedReason));
		Test.TestFalse(TEXT("Reset request cannot reopen acceptance during the grant"), F.Quests->AcceptQuest(F.Id(4), F.Character));
		Test.TestFalse(TEXT("Reset request cannot collect new-run facts during the grant"), F.Quests->ObserveObjective(F.Id(4), 0, F.Character));
	};
	QuestObserver->OnNotification = [&]
	{
		++QuestCallbacks;
		F.CheckReset(Test); // Observers must see the complete reset, not a stale receipt.
	};
	F.Quests->OnQuestChanged.AddDynamic(QuestObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	if (bItemReward)
	{
		F.Inventory().OnInventoryGridChanged.AddDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	}
	else
	{
		F.Inventory().OnInventoryChanged.AddDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	}
	FText Reason;
	bInsideClaim = true;
	const bool bGranted = F.Quests->ClaimReward(F.Id(0), F.Character, Reason);
	bInsideClaim = false;
	F.Inventory().OnInventoryGridChanged.RemoveDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Inventory().OnInventoryChanged.RemoveDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Quests->OnQuestChanged.RemoveDynamic(QuestObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	Test.TestTrue(TEXT("Outer reward finishes despite synchronous reset request"), bGranted);
	Test.TestEqual(TEXT("One actual inventory notification exercised the reset boundary"), InventoryCallbacks, 1);
	Test.TestEqual(TEXT("One final quest notification publishes the reset"), QuestCallbacks, 1);
	F.CheckReset(Test);
	Test.TestEqual(TEXT("Old-run grant pays its configured gold exactly once"), F.Inventory().GetGold(), 17 + Award);
	Test.TestEqual(TEXT("Old-run grant pays its configured item quantity exactly once"), F.RewardQuantity(), bItemReward ? 2 : 0);
	const auto AfterGrant = F.Inventory().CaptureInventory();
	Test.TestFalse(TEXT("Cleared receipt cannot pay without new-run work"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	Test.TestTrue(TEXT("Premature new-run claim preserves inventory"), SameInventory(AfterGrant, F.Inventory().CaptureInventory()));
	if (!F.Ready(Test, 0)) { return false; }
	Test.TestTrue(TEXT("Grant guard releases for legitimate new-run completion"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	Test.TestTrue(TEXT("New-run completion commits its own receipt"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Claimed);
	Test.TestEqual(TEXT("Two completed lifecycles across the reset receive exactly two gold grants"), F.Inventory().GetGold(), 17 + 2 * Award);
	Test.TestEqual(TEXT("Two completed lifecycles across the reset receive exactly two item grants"), F.RewardQuantity(), bItemReward ? 4 : 0);
	return !Test.HasAnyErrors();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestCollectorsLifecycleTest,
	"Rogue10m.VillageQuest.Lifecycle.PreacceptCollectorsClaimAndSameRunRevisit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestCollectorsLifecycleTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestLifecycleTests;
	FFixture F(false, true);
	if (!F.Check(*this)) { return false; }
	FText Reason;
	const auto BeforeDisabledAttempts = F.Inventory().CaptureInventory();
	TestEqual(TEXT("Introductory fixture exposes one enabled quest"), F.Definition->GetEnabledQuestCount(), 1);
	for (int32 Index = 1; Index < F.Definition->Quests.Num(); ++Index)
	{
		TestTrue(TEXT("Disabled named row is unavailable through the actual component"), F.Quests->GetQuestState(F.Id(Index)) == ERogue10mVillageQuestState::Unavailable);
		TestFalse(TEXT("Disabled named row cannot record observations"), F.Quests->ObserveObjective(F.Id(Index), 0, F.Character));
		TestFalse(TEXT("Disabled named row cannot be accepted"), F.Quests->AcceptQuest(F.Id(Index), F.Character));
		TestFalse(TEXT("Disabled named row cannot grant a reward"), F.Quests->ClaimReward(F.Id(Index), F.Character, Reason));
	}
	TestTrue(TEXT("Disabled-row calls preserve actual inventory"), SameInventory(BeforeDisabledAttempts, F.Inventory().CaptureInventory()));
	auto* First = F.SpawnCollector(0, 0);
	auto* Second = F.SpawnCollector(0, 1);
	if (!F.EmitCollectorEvent(*this, First)) { return false; }
	TestEqual(TEXT("First collector contributes one preaccept fact"), F.Quests->GetObjectiveProgress(F.Id(0)), 1);
	if (!F.EmitCollectorEvent(*this, First) || !F.EmitCollectorEvent(*this, Second)) { return false; }
	TestEqual(TEXT("Repeated overlap cannot add a third fact"), F.Quests->GetObjectiveProgress(F.Id(0)), 2);
	TestTrue(TEXT("Completed preaccept facts leave acceptance available"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Available);
	const auto BeforeAcceptance = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Observations alone cannot claim a reward"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestTrue(TEXT("Preaccept reward attempt leaves inventory intact"), SameInventory(BeforeAcceptance, F.Inventory().CaptureInventory()));
	if (!TestTrue(TEXT("Accept preobserved quest through actual component"), F.Quests->AcceptQuest(F.Id(0), F.Character))) { return false; }
	TestTrue(TEXT("Preobserved accepted quest is immediately Ready"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Ready);
	F.Inventory().AddGold(17);
	if (!TestTrue(TEXT("Ready quest grants actual reward"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason))) { return false; }
	TestEqual(TEXT("Exact configured payment"), F.Inventory().GetGold(), 17 + F.Definition->Quests[0].RewardGold);
	const auto Claimed = F.Inventory().CaptureInventory();
	if (!TestTrue(TEXT("First collector can leave the world"), First->Destroy())
		|| !TestTrue(TEXT("Second collector can leave the world"), Second->Destroy())) { return false; }
	auto* RecreatedFirst = F.SpawnCollector(0, 0);
	auto* RecreatedSecond = F.SpawnCollector(0, 1);
	if (!F.EmitCollectorEvent(*this, RecreatedFirst) || !F.EmitCollectorEvent(*this, RecreatedSecond)) { return false; }
	TestTrue(TEXT("Same-definition lookup returns the original PlayerState component"),
		URogue10mVillageQuestComponent::FindOrCreate(F.Character, F.Definition.Get()) == F.Quests);
	TestTrue(TEXT("Lookup without explicit definition retains the run's configured component"),
		URogue10mVillageQuestComponent::FindOrCreate(F.Character) == F.Quests);
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> OtherDefinition(NewObject<URogue10mVillageQuestDataAsset>());
	OtherDefinition->SupportedWorld = F.Definition->SupportedWorld;
	TestNull(TEXT("Another definition cannot reinterpret the live receipt"),
		URogue10mVillageQuestComponent::FindOrCreate(F.Character, OtherDefinition.Get()));
	TArray<URogue10mVillageQuestComponent*> Components;
	F.State->GetComponents(Components);
	TestEqual(TEXT("Revisits create no duplicate quest component"), Components.Num(), 1);
	F.Run->StartRun(); // Running -> Running is deliberately not a new-run event.
	TestTrue(TEXT("Same-run collector recreation and repeated StartRun preserve Claimed"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Claimed);
	TestEqual(TEXT("Revisit keeps both observed facts"), F.Quests->GetObjectiveProgress(F.Id(0)), 2);
	TestTrue(TEXT("Revisit keeps first fact inspectable"), F.Quests->HasObservedObjective(F.Id(0), 0));
	TestTrue(TEXT("Revisit keeps second fact inspectable"), F.Quests->HasObservedObjective(F.Id(0), 1));
	TestFalse(TEXT("Revisit cannot accept a paid quest again"), F.Quests->AcceptQuest(F.Id(0), F.Character));
	TestFalse(TEXT("Revisit cannot claim a second reward"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestTrue(TEXT("Entire inventory remains stable throughout revisit"), SameInventory(Claimed, F.Inventory().CaptureInventory()));
	F.Run->OnRunPhaseChanged.Broadcast(ERogue10mRunPhase::Running);
	F.CheckReset(*this);
	TestTrue(TEXT("Single enabled quest reset preserves inventory"), SameInventory(Claimed, F.Inventory().CaptureInventory()));
	if (!F.EmitCollectorEvent(*this, RecreatedFirst) || !F.EmitCollectorEvent(*this, RecreatedSecond)
		|| !TestTrue(TEXT("Enabled quest accepts fresh collector facts after reset"), F.Quests->AcceptQuest(F.Id(0), F.Character))) { return false; }
	TestTrue(TEXT("Enabled quest becomes Ready after fresh collection"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Ready);
	TestTrue(TEXT("Enabled quest grants its post-reset reward"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestEqual(TEXT("Exactly one payment per completed enabled-quest lifecycle"), F.Inventory().GetGold(), 17 + 2 * F.Definition->Quests[0].RewardGold);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestRunningResetLifecycleTest,
	"Rogue10m.VillageQuest.Lifecycle.RunningPhaseEventResetsQuestAndClueState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestRunningResetLifecycleTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestLifecycleTests;
	FFixture F;
	if (!F.Check(*this) || !F.Ready(*this, 0)) { return false; }
	FText Reason;
	if (!TestTrue(TEXT("Seed a real paid receipt"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason))
		|| !F.Ready(*this, 1)
		|| !TestTrue(TEXT("Seed an accepted incomplete quest"), F.Quests->AcceptQuest(F.Id(2), F.Character))
		|| !TestTrue(TEXT("Seed incomplete accepted progress"), F.Quests->ObserveObjective(F.Id(2), 0, F.Character))
		|| !TestTrue(TEXT("Seed an unaccepted fact"), F.Quests->ObserveObjective(F.Id(3), 1, F.Character))
		|| !F.CompleteStory(*this)) { return false; }
	TestTrue(TEXT("Reset includes an Active quest"), F.Quests->GetQuestState(F.Id(2)) == ERogue10mVillageQuestState::Active);
	TestTrue(TEXT("Reset includes an Available quest with facts"), F.Quests->GetQuestState(F.Id(3)) == ERogue10mVillageQuestState::Available);
	const auto BeforeReset = F.Inventory().CaptureInventory();
	int32 Notifications = 0;
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	Observer->OnNotification = [&] { ++Notifications; F.CheckReset(*this); };
	F.Quests->OnQuestChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	// Explicit component event-boundary injection, not a second StartRun call.
	F.Run->OnRunPhaseChanged.Broadcast(ERogue10mRunPhase::Running);
	F.Quests->OnQuestChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestEqual(TEXT("Reset publishes one complete state to observers"), Notifications, 1);
	F.CheckReset(*this);
	TestTrue(TEXT("Reset preserves the attached component identity"), URogue10mVillageQuestComponent::FindOrCreate(F.Character) == F.Quests);
	TestTrue(TEXT("Quest reset does not change inventory or prior payout"), SameInventory(BeforeReset, F.Inventory().CaptureInventory()));
	TestFalse(TEXT("Prior-run receipt cannot pay without new work"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	if (!TestTrue(TEXT("Same collector fact is new again after reset"), F.Quests->ObserveObjective(F.Id(0), 0, F.Character))
		|| !TestTrue(TEXT("Quest may be accepted for the new run"), F.Quests->AcceptQuest(F.Id(0), F.Character))) { return false; }
	TestTrue(TEXT("One new-run fact gives Active rather than stale Ready"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Active);
	TestFalse(TEXT("Partial new-run work cannot reuse the old Ready receipt"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestTrue(TEXT("Incomplete new-run attempts preserve inventory"), SameInventory(BeforeReset, F.Inventory().CaptureInventory()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestItemResetLifecycleTest,
	"Rogue10m.VillageQuest.Lifecycle.ItemRewardDefersReentrantRunReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestItemResetLifecycleTest::RunTest(const FString&)
{
	return Rogue10mVillageQuestLifecycleTests::CheckReentrantReset(*this, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestGoldResetLifecycleTest,
	"Rogue10m.VillageQuest.Lifecycle.GoldRewardDefersReentrantRunReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestGoldResetLifecycleTest::RunTest(const FString&)
{
	return Rogue10mVillageQuestLifecycleTests::CheckReentrantReset(*this, false);
}
#endif
