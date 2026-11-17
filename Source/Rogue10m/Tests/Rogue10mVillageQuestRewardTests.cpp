#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
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
#include "Rogue10mVillageQuestRewardTestProxy.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Rogue10mVillageQuestRewardTests
{
static bool SameInventory(const FRogue10mInventorySnapshot& A, const FRogue10mInventorySnapshot& B)
{
	return FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	ARogue10mGameState* Run = nullptr;
	URogue10mVillageQuestComponent* Quests = nullptr;
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Definition;
	TStrongObjectPtr<URogue10mItemDataAsset> RewardItem;
	TStrongObjectPtr<URogue10mItemDataAsset> FillerItem;
	FString SetupError;

	FFixture()
	{
		// No GI, subsystem initialization, Blueprint loads or save APIs. Neither the
		// world nor the character begins play; only the quest owner does so below.
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.InitializeScenes(false).CreatePhysicsScene(false).CreateFXSystem(false)
			.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false,
			FName(*FString::Printf(TEXT("VillageQuestReward_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
			GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
		if (!World || World->GetGameInstance()) { SetupError = TEXT("Expected transient world without a GameInstance"); return; }
		World->InitializeActorsForPlay(FURL());
		Run = World->SpawnActor<ARogue10mGameState>();
		if (!Run) { SetupError = TEXT("Native run spawn failed"); return; }
		World->SetGameState(Run);
		if (!SpawnPlayer(Character, State)) { SetupError = TEXT("Native player spawn failed"); return; }
		Run->StartRun();

		RewardItem.Reset(NewObject<URogue10mItemDataAsset>());
		RewardItem->ItemId = TEXT("VillageQuestRewardItem");
		RewardItem->InventorySize = FIntPoint(1, 1);
		RewardItem->MaxStackSize = 5;
		FillerItem.Reset(NewObject<URogue10mItemDataAsset>());
		FillerItem->ItemId = TEXT("VillageQuestFiller");
		FillerItem->InventorySize = FIntPoint(1, 1);
		FillerItem->MaxStackSize = 1;
		Definition.Reset(NewObject<URogue10mVillageQuestDataAsset>());
		Definition->Quests[0].RewardGold = 0;
		Definition->Quests[0].RewardItem = RewardItem.Get();
		Definition->Quests[0].RewardQuantity = 2;
		FString Reason;
		if (!Definition->ValidateDefinition(Reason)) { SetupError = Reason; return; }

		Quests = NewObject<URogue10mVillageQuestComponent>(State, NAME_None, RF_Transient);
		auto* Property = FindFProperty<FObjectPropertyBase>(Quests->GetClass(), TEXT("ActiveDefinition"));
		if (!Property) { SetupError = TEXT("ActiveDefinition reflected property missing"); return; }
		Property->SetObjectPropertyValue_InContainer(Quests, Definition.Get());
		State->AddInstanceComponent(Quests);
		Quests->RegisterComponent();
		State->DispatchBeginPlay();

		auto Snapshot = Inventory().CaptureInventory();
		if (Snapshot.Containers.IsEmpty()) { SetupError = TEXT("Native base inventory missing"); return; }
		Snapshot.Containers.SetNum(1);
		Snapshot.Containers[0].GridSize = FIntPoint(1, 1);
		Snapshot.Containers[0].Entries.Reset();
		Snapshot.Containers[0].BagAsset.Reset();
		if (!Inventory().RestoreInventory(Snapshot)) { SetupError = TEXT("Public one-cell inventory restore failed"); }
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

	bool SpawnPlayer(ARogue10mCharacter*& OutCharacter, ARogue10mPlayerState*& OutState)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		OutState = World->SpawnActor<ARogue10mPlayerState>();
		OutCharacter = World->SpawnActor<ARogue10mCatalogTestCharacter>(
			ARogue10mCatalogTestCharacter::StaticClass(), FTransform::Identity, Spawn);
		if (!OutCharacter || !OutState) { return false; }
		OutCharacter->SetPlayerState(OutState); // Also sets APlayerState::GetPawn in UE 5.8.
		auto* ASC = OutState->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(OutState->GetRogueAttributeSet());
		ASC->InitAbilityActorInfo(OutState, OutCharacter);
		OutState->GetRogueAttributeSet()->InitializeVitals(135.0f, 100.0f, 100.0f);
		return OutState->GetPawn() == OutCharacter;
	}

	bool Check(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(FString(TEXT("Initialized no-GI quest/inventory fixture: ")) + SetupError,
			SetupError.IsEmpty() && World && World->IsInitialized() && World->AreActorsInitialized()
			&& !World->GetGameInstance() && !World->HasBegunPlay() && Character && State && Run && Quests
			&& !Character->HasActorBegunPlay() && State->HasActorBegunPlay() && Quests->HasBegunPlay()
			&& State->GetPawn() == Character && Character->GetPlayerState() == State
			&& State->GetRogueAttributeSet()->GetHealth() > 0.0f && !Character->IsDead()
			&& World->GetGameState() == Run && Run->IsRunActive()
			&& Quests->GetDefinition() == Definition.Get()
			&& Quests->GetQuestState(Id(0)) == ERogue10mVillageQuestState::Available);
	}

	FName Id(int32 Index) const { return Definition->Quests[Index].QuestId; }
	URogue10mInventoryComponent& Inventory() const { return *Character->GetInventoryComponent(); }
	bool Ready(FAutomationTestBase& Test, int32 Index)
	{
		return Test.TestTrue(TEXT("Public quest acceptance"), Quests->AcceptQuest(Id(Index), Character))
			&& Test.TestTrue(TEXT("First public objective observation"), Quests->ObserveObjective(Id(Index), 0, Character))
			&& Test.TestTrue(TEXT("Second public objective observation"), Quests->ObserveObjective(Id(Index), 1, Character))
			&& Test.TestTrue(TEXT("Public calls establish Ready"), Quests->GetQuestState(Id(Index)) == ERogue10mVillageQuestState::Ready);
	}
	int32 Quantity(const URogue10mItemDataAsset* Item) const
	{
		int32 Result = 0;
		for (const auto& Container : Inventory().GetInventoryContainers())
		{
			for (const auto& Entry : Container.Entries) { if (Entry.ItemData == Item) { Result += Entry.Quantity; } }
		}
		return Result;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestItemCapacityTest,
	"Rogue10m.VillageQuest.Reward.ItemCapacityRetryExactlyOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestItemCapacityTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestRewardTests;
	FFixture F;
	if (!F.Check(*this) || !F.Ready(*this, 0)) { return false; }
	F.Inventory().AddGold(17);
	int32 Container = INDEX_NONE; FGuid FillerId;
	if (!TestTrue(TEXT("Fill the actual one-cell grid"), F.Inventory().TryAddGridItem(F.FillerItem.Get(), 1, Container, FillerId))) { return false; }
	const auto Full = F.Inventory().CaptureInventory();
	FText Reason;
	TestFalse(TEXT("Full grid rejects the actual item reward"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestFalse(TEXT("Capacity failure returns a reason"), Reason.IsEmpty());
	TestTrue(TEXT("Failed grant preserves full inventory, gold and item identities"), SameInventory(Full, F.Inventory().CaptureInventory()));
	TestTrue(TEXT("Failed reward stays Ready"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Ready);
	TestEqual(TEXT("Failed grant preserves completed facts"), F.Quests->GetObjectiveProgress(F.Id(0)), 2);
	TestEqual(TEXT("Failed grant creates no partial item quantity"), F.Quantity(F.RewardItem.Get()), 0);
	TestEqual(TEXT("Filler remains after rejection"), F.Quantity(F.FillerItem.Get()), 1);
	FRogue10mInventoryGridEntry Removed;
	if (!TestTrue(TEXT("Public removal frees capacity"), F.Inventory().RemoveGridItem(Container, FillerId, Removed))) { return false; }
	TestTrue(TEXT("Removed the original filler identity"), Removed.InstanceId == FillerId);
	if (!TestTrue(TEXT("Retry grants after capacity becomes available"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason))) { return false; }
	TestTrue(TEXT("Successful item grant commits the receipt"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Claimed);
	TestEqual(TEXT("Exact configured item quantity"), F.Quantity(F.RewardItem.Get()), 2);
	TestEqual(TEXT("Item-only reward preserves existing gold"), F.Inventory().GetGold(), 17);
	const auto& Entries = F.Inventory().GetInventoryContainers()[0].Entries;
	TestEqual(TEXT("One stack was granted"), Entries.Num(), 1);
	if (Entries.Num() == 1) { TestTrue(TEXT("Reward has a fresh valid identity"), Entries[0].InstanceId.IsValid() && Entries[0].InstanceId != FillerId); }
	const auto Claimed = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Repeat claim is rejected"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestTrue(TEXT("Repeat claim preserves exact successful snapshot"), SameInventory(Claimed, F.Inventory().CaptureInventory()));
	if (Entries.Num() == 1)
	{
		const FGuid RewardId = Entries[0].InstanceId;
		if (!TestTrue(TEXT("Make room so the receipt is tested independently of capacity"), F.Inventory().RemoveGridItem(0, RewardId, Removed))) { return false; }
		const auto EmptyAgain = F.Inventory().CaptureInventory();
		TestFalse(TEXT("Claimed receipt rejects another item even with an empty bag"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
		TestTrue(TEXT("No replacement item appears after repeat"), SameInventory(EmptyAgain, F.Inventory().CaptureInventory()));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestGoldCapacityTest,
	"Rogue10m.VillageQuest.Reward.GoldCapacityRetryExactlyOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestGoldCapacityTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestRewardTests;
	FFixture F;
	if (!F.Check(*this) || !F.Ready(*this, 1)) { return false; }
	const int32 Award = F.Definition->Quests[1].RewardGold;
	F.Inventory().AddGold(MAX_int32 - Award + 1);
	const auto Before = F.Inventory().CaptureInventory(); FText Reason;
	TestFalse(TEXT("Gold capacity rejects the whole reward"), F.Quests->ClaimReward(F.Id(1), F.Character, Reason));
	TestFalse(TEXT("Gold capacity returns a reason"), Reason.IsEmpty());
	TestTrue(TEXT("Gold cap failure preserves full snapshot"), SameInventory(Before, F.Inventory().CaptureInventory()));
	TestTrue(TEXT("Gold cap failure preserves Ready receipt"), F.Quests->GetQuestState(F.Id(1)) == ERogue10mVillageQuestState::Ready);
	if (!TestTrue(TEXT("Public spend frees exact missing capacity"), F.Inventory().TrySpendGold(1))) { return false; }
	if (!TestTrue(TEXT("Gold reward retries at exact capacity"), F.Quests->ClaimReward(F.Id(1), F.Character, Reason))) { return false; }
	TestEqual(TEXT("Successful reward reaches exact int32 cap"), F.Inventory().GetGold(), MAX_int32);
	TestTrue(TEXT("Successful gold reward commits receipt"), F.Quests->GetQuestState(F.Id(1)) == ERogue10mVillageQuestState::Claimed);
	if (!TestTrue(TEXT("Make room so repeat is tested independently of cap"), F.Inventory().TrySpendGold(Award))) { return false; }
	const auto Claimed = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Claimed receipt prevents another payment with capacity available"), F.Quests->ClaimReward(F.Id(1), F.Character, Reason));
	TestTrue(TEXT("No duplicate currency or inventory mutation"), SameInventory(Claimed, F.Inventory().CaptureInventory()));
	F.Inventory().AddGold(Award);
	if (!F.Ready(*this, 0)) { return false; }
	TestTrue(TEXT("Item-only reward still succeeds when gold is capped"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
	TestEqual(TEXT("Capped gold is unchanged by item-only reward"), F.Inventory().GetGold(), MAX_int32);
	TestEqual(TEXT("Item-only reward retains its full quantity at gold cap"), F.Quantity(F.RewardItem.Get()), 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestUseBoundariesTest,
	"Rogue10m.VillageQuest.Reward.OwnerAuthorityDeathAndTerminalBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestUseBoundariesTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestRewardTests;
	enum class ERestriction { WrongOwner, OwnerAuthority, PawnAuthority, Dead, Victory, Defeat };
	for (ERestriction Restriction : {ERestriction::WrongOwner, ERestriction::OwnerAuthority, ERestriction::PawnAuthority,
		ERestriction::Dead, ERestriction::Victory, ERestriction::Defeat})
	{
		FFixture F;
		if (!F.Check(*this) || !F.Ready(*this, 0)) { return false; }
		ARogue10mCharacter* Caller = F.Character;
		ARogue10mCharacter* OtherCharacter = nullptr; ARogue10mPlayerState* OtherState = nullptr;
		if (Restriction == ERestriction::WrongOwner)
		{
			if (!TestTrue(TEXT("Separate legitimate player fixture"), F.SpawnPlayer(OtherCharacter, OtherState))) { return false; }
			Caller = OtherCharacter;
		}
		else if (Restriction == ERestriction::OwnerAuthority) { F.State->SetRole(ROLE_SimulatedProxy); }
		else if (Restriction == ERestriction::PawnAuthority) { F.Character->SetRole(ROLE_SimulatedProxy); }
		else if (Restriction == ERestriction::Dead)
		{
			// Isolate the death predicate using its public state API; lethal damage
			// would also end the run and conflate the two independently tested guards.
			F.State->SetCharacterDead(true);
			TestTrue(TEXT("Dead caller precondition"), Caller->IsDead());
		}
		else { F.Run->FinishRun(Restriction == ERestriction::Victory); TestTrue(TEXT("Terminal run precondition"), F.Run->IsRunTerminal()); }
		const auto OwnerBefore = F.Inventory().CaptureInventory();
		const auto CallerBefore = Caller->GetInventoryComponent()->CaptureInventory();
		const FString Context = FString::Printf(TEXT("restriction=%d"), static_cast<int32>(Restriction));
		FText Reason;
		TestFalse(Context + TEXT(" rejects acceptance"), F.Quests->AcceptQuest(F.Id(1), Caller));
		TestFalse(Context + TEXT(" rejects observation"), F.Quests->ObserveObjective(F.Id(2), 0, Caller));
		TestFalse(Context + TEXT(" rejects Ready reward"), F.Quests->ClaimReward(F.Id(0), Caller, Reason));
		TestTrue(Context + TEXT(" preserves owner's inventory and gold"), SameInventory(OwnerBefore, F.Inventory().CaptureInventory()));
		TestTrue(Context + TEXT(" preserves caller's inventory and gold"), SameInventory(CallerBefore, Caller->GetInventoryComponent()->CaptureInventory()));
		TestTrue(Context + TEXT(" preserves available quest"), F.Quests->GetQuestState(F.Id(1)) == ERogue10mVillageQuestState::Available);
		TestEqual(Context + TEXT(" records no forbidden objective fact"), F.Quests->GetObjectiveProgress(F.Id(2)), 0);
		TestTrue(Context + TEXT(" preserves Ready receipt"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Ready);
		TestEqual(Context + TEXT(" preserves completed objective facts"), F.Quests->GetObjectiveProgress(F.Id(0)), 2);
		F.State->SetRole(ROLE_Authority); F.Character->SetRole(ROLE_Authority); F.State->SetCharacterDead(false);
		if (!F.Run->IsRunTerminal())
		{
			TestTrue(Context + TEXT(" legitimate owner accepts after restriction lifts"), F.Quests->AcceptQuest(F.Id(1), F.Character));
			TestTrue(Context + TEXT(" legitimate owner observes after restriction lifts"), F.Quests->ObserveObjective(F.Id(2), 0, F.Character));
			TestTrue(Context + TEXT(" legitimate owner claims after restriction lifts"), F.Quests->ClaimReward(F.Id(0), F.Character, Reason));
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestRewardReentryTest,
	"Rogue10m.VillageQuest.Reward.SynchronousInventoryDelegateReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestRewardReentryTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestRewardTests;
	FFixture F;
	if (!F.Check(*this) || !F.Ready(*this, 0) || !F.Ready(*this, 1) || !F.Ready(*this, 2)) { return false; }
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Grid(NewObject<URogue10mVillageQuestRewardTestProxy>());
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Changed(NewObject<URogue10mVillageQuestRewardTestProxy>());
	FName OuterId = F.Id(0), OtherReadyId = F.Id(1);
	bool bInsideCallback = false, bOuterClaimInProgress = false;
	int32 GridCalls = 0, ChangedCalls = 0, NestedNotifications = 0;
	auto AttemptReentry = [this, &F, &OuterId, &OtherReadyId, &bInsideCallback, &bOuterClaimInProgress, &NestedNotifications]()
	{
		if (bInsideCallback) { ++NestedNotifications; return; } // A regression must fail, not recurse indefinitely.
		TGuardValue<bool> CallbackGuard(bInsideCallback, true);
		TestTrue(TEXT("Inventory observer executes synchronously inside actual ClaimReward"), bOuterClaimInProgress);
		const auto Before = F.Inventory().CaptureInventory(); FText NestedReason;
		TestFalse(TEXT("Synchronous callback cannot claim the same quest"), F.Quests->ClaimReward(OuterId, F.Character, NestedReason));
		TestFalse(TEXT("Synchronous callback cannot claim a different Ready gold quest"), F.Quests->ClaimReward(OtherReadyId, F.Character, NestedReason));
		TestFalse(TEXT("Synchronous callback cannot accept another quest"), F.Quests->AcceptQuest(F.Id(3), F.Character));
		TestFalse(TEXT("Synchronous callback cannot observe another quest"), F.Quests->ObserveObjective(F.Id(4), 0, F.Character));
		TestFalse(TEXT("Synchronous callback cannot analyze the optional clue"), F.Quests->AnalyzeOptionalClue(F.Id(3), F.Character));
		TestTrue(TEXT("Nested attempts preserve current transaction snapshot"), SameInventory(Before, F.Inventory().CaptureInventory()));
	};
	Grid->OnNotification = [&GridCalls, &AttemptReentry] { ++GridCalls; AttemptReentry(); };
	Changed->OnNotification = [&ChangedCalls, &AttemptReentry] { ++ChangedCalls; AttemptReentry(); };
	F.Inventory().OnInventoryGridChanged.AddDynamic(Grid.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Inventory().OnInventoryChanged.AddDynamic(Changed.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	FText Reason;
	bOuterClaimInProgress = true;
	const bool bItemGranted = F.Quests->ClaimReward(OuterId, F.Character, Reason);
	bOuterClaimInProgress = false;
	TestTrue(TEXT("Outer item reward completes"), bItemGranted);
	TestEqual(TEXT("Real grid insertion calls grid observer once"), GridCalls, 1);
	TestEqual(TEXT("Real grid insertion calls inventory observer once"), ChangedCalls, 1);
	TestEqual(TEXT("Item quantity is granted exactly once"), F.Quantity(F.RewardItem.Get()), 2);
	TestEqual(TEXT("Nested different-quest claim grants no gold"), F.Inventory().GetGold(), 0);
	TestTrue(TEXT("Other Ready receipt remains retryable"), F.Quests->GetQuestState(OtherReadyId) == ERogue10mVillageQuestState::Ready);
	TestTrue(TEXT("Item receipt commits after delegate returns"), F.Quests->GetQuestState(OuterId) == ERogue10mVillageQuestState::Claimed);

	// Gold uses only OnInventoryChanged, after its receipt is committed. The
	// different Ready gold quest still isolates the component-wide guard.
	OuterId = F.Id(1); OtherReadyId = F.Id(2);
	bOuterClaimInProgress = true;
	const bool bGoldGranted = F.Quests->ClaimReward(OuterId, F.Character, Reason);
	bOuterClaimInProgress = false;
	TestTrue(TEXT("Formerly denied quest claims normally after item transaction"), bGoldGranted);
	TestEqual(TEXT("Gold grant sends no grid event"), GridCalls, 1);
	TestEqual(TEXT("Gold grant sends one real inventory event"), ChangedCalls, 2);
	TestEqual(TEXT("Gold payment occurs exactly once"), F.Inventory().GetGold(), F.Definition->Quests[1].RewardGold);
	TestEqual(TEXT("No unexpected recursively generated inventory events"), NestedNotifications, 0);
	TestTrue(TEXT("Unrelated Ready quest remains unclaimed"), F.Quests->GetQuestState(OtherReadyId) == ERogue10mVillageQuestState::Ready);
	TestTrue(TEXT("Forbidden acceptance left quest Available"), F.Quests->GetQuestState(F.Id(3)) == ERogue10mVillageQuestState::Available);
	TestEqual(TEXT("Forbidden observation recorded no fact"), F.Quests->GetObjectiveProgress(F.Id(4)), 0);
	TestTrue(TEXT("Forbidden analysis preserves the found clue"), F.Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Found);
	F.Inventory().OnInventoryGridChanged.RemoveDynamic(Grid.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Inventory().OnInventoryChanged.RemoveDynamic(Changed.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Final nested target claims after outer transaction returns"), F.Quests->ClaimReward(OtherReadyId, F.Character, Reason));
	TestEqual(TEXT("Only two explicitly claimed gold quests pay"), F.Inventory().GetGold(), F.Definition->Quests[1].RewardGold + F.Definition->Quests[2].RewardGold);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestOptionalStoryBoundariesTest,
	"Rogue10m.VillageQuest.Reward.OptionalStoryHasNoPrimaryPayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestOptionalStoryBoundariesTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestRewardTests;
	FFixture F;
	if (!F.Check(*this)) { return false; }
	const auto Before = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Analysis requires discovering the source"), F.Quests->AnalyzeOptionalClue(F.Id(3), F.Character));
	TestFalse(TEXT("Report requires prior analysis"), F.Quests->ReportOptionalClue(F.Id(0), F.Character));
	if (!TestTrue(TEXT("Public source observation finds the optional clue before acceptance"), F.Quests->ObserveObjective(F.Id(0), 1, F.Character))) { return false; }
	TestTrue(TEXT("Source observation records Found"), F.Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Found);
	TestFalse(TEXT("Another village cannot analyze the clue"), F.Quests->AnalyzeOptionalClue(F.Id(1), F.Character));
	ARogue10mCharacter* OtherCharacter = nullptr; ARogue10mPlayerState* OtherState = nullptr;
	if (!TestTrue(TEXT("Optional-story wrong-owner player has valid pawn ownership"), F.SpawnPlayer(OtherCharacter, OtherState))) { return false; }
	TestFalse(TEXT("Wrong owner cannot analyze a found clue"), F.Quests->AnalyzeOptionalClue(F.Id(3), OtherCharacter));
	F.State->SetCharacterDead(true);
	TestFalse(TEXT("Dead character cannot analyze a found clue"), F.Quests->AnalyzeOptionalClue(F.Id(3), F.Character));
	F.State->SetCharacterDead(false);
	if (!TestTrue(TEXT("Configured analyst accepts a found clue"), F.Quests->AnalyzeOptionalClue(F.Id(3), F.Character))) { return false; }
	TestTrue(TEXT("Correct analysis records Analyzed"), F.Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Analyzed);
	TestFalse(TEXT("Analyst cannot impersonate the configured report village"), F.Quests->ReportOptionalClue(F.Id(3), F.Character));
	TestFalse(TEXT("Wrong owner cannot report an analyzed clue"), F.Quests->ReportOptionalClue(F.Id(0), OtherCharacter));
	TestTrue(TEXT("Configured report village completes optional story"), F.Quests->ReportOptionalClue(F.Id(0), F.Character));
	TestTrue(TEXT("Correct report records Reported"), F.Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Reported);
	TestFalse(TEXT("Repeated optional report is rejected"), F.Quests->ReportOptionalClue(F.Id(0), F.Character));
	TestTrue(TEXT("Narrative chain grants no gold or items"), SameInventory(Before, F.Inventory().CaptureInventory()));
	TestTrue(TEXT("Narrative chain does not accept the source quest"), F.Quests->GetQuestState(F.Id(0)) == ERogue10mVillageQuestState::Available);
	TestEqual(TEXT("Only actual source observation contributes primary progress"), F.Quests->GetObjectiveProgress(F.Id(0)), 1);
	TestTrue(TEXT("Narrative chain does not accept the analyst quest"), F.Quests->GetQuestState(F.Id(3)) == ERogue10mVillageQuestState::Available);
	TestEqual(TEXT("Narrative analysis contributes no analyst objective progress"), F.Quests->GetObjectiveProgress(F.Id(3)), 0);
	return !HasAnyErrors();
}
#endif
