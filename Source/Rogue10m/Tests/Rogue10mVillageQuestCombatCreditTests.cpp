#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMenuPlayerController.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterLootComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestObjective.h"
#include "Rogue10mVillageQuestRewardTestProxy.h"
#include "TimerManager.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

/** Setup/retry seam only: all death and credit production begins with actual TakeDamage. */
struct FRogue10mVillageQuestCombatCreditTestAccess
{
	static void MarkRecovery(ARogue10mVillageQuestMonster& Monster) { Monster.bRecoveryEncounter = true; }
	static void Flush(URogue10mVillageQuestComponent& Quests) { Quests.FlushCombatDeaths(); }
	static uint64 Epoch(const URogue10mVillageQuestComponent& Quests) { return Quests.CombatCredits.GetEpoch(); }
	static bool HasPending(const URogue10mVillageQuestComponent& Quests, int32 Slot) { return Quests.CombatCredits.HasPending(Slot); }
	static bool IsFlushScheduled(const URogue10mVillageQuestComponent& Quests) { return Quests.bCombatFlushScheduled; }
};

namespace Rogue10mVillageQuestCombatCreditTests
{
static bool SameInventory(const FRogue10mInventorySnapshot& A, const FRogue10mInventorySnapshot& B)
{
	return FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool SameGrowth(const FRogue10mGrowthSnapshot& A, const FRogue10mGrowthSnapshot& B)
{
	return FRogue10mGrowthSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

struct FPlayer
{
	ARogue10mCatalogTestCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	ARogue10mMenuPlayerController* Controller = nullptr;
	URogue10mVillageQuestComponent* Quests = nullptr;
	URogue10mInventoryComponent& Inventory() const { return *Character->GetInventoryComponent(); }
};

struct FFixture
{
	UWorld* World = nullptr;
	AGameModeBase* Mode = nullptr;
	ARogue10mGameState* Run = nullptr;
	FPlayer Owner, Other;
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> Definition;
	TStrongObjectPtr<URogue10mMonsterDataAsset> MonsterData;
	FString SetupError;

	explicit FFixture(bool bSecondCombat = false)
	{
		// No GI, profile/save initialization, world tick or world BeginPlay. The
		// concrete controller's existing constructor retains its hard widget finder.
		const auto Values = UWorld::InitializationValues()
			.InitializeScenes(false).CreatePhysicsScene(false).CreateFXSystem(false)
			.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false,
			FName(*FString::Printf(TEXT("VillageQuestCombatCredit_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
			GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
		if (!World || World->GetGameInstance()) { SetupError = TEXT("Expected a native world without a GameInstance"); return; }
		World->InitializeActorsForPlay(FURL());
		// APawn damage requires a real authority GameMode. Build that framework
		// through public APIs without a GameInstance, profile/subsystem init or BeginPlay.
		Mode = World->SpawnActorDeferred<AGameModeBase>(AGameModeBase::StaticClass(),
			FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Mode) { SetupError = TEXT("Native authority GameMode spawn failed"); return; }
		Mode->GameStateClass = ARogue10mGameState::StaticClass();
		Mode->FinishSpawning(FTransform::Identity);
		Run = Mode->GetGameState<ARogue10mGameState>();
		if (!Run) { SetupError = TEXT("Native GameMode failed to create its configured GameState"); return; }
		World->CopyGameState(Mode, Run);
		Definition.Reset(NewObject<URogue10mVillageQuestDataAsset>());
		Definition->SupportedWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(World->GetPathName()));
		if (bSecondCombat)
		{
			// Configure and validate the second combat row before either public attachment.
			Definition->Quests[1].CombatObjectiveIndex = 1;
			Definition->Quests[1].CombatMonsterData = Definition->Quests[2].CombatMonsterData;
		}
		if (!Definition->ValidateDefinition(SetupError)) { return; }
		MonsterData.Reset(NewObject<URogue10mMonsterDataAsset>());
		MonsterData->MonsterId = TEXT("VillageQuestCombatCreditFixture");
		MonsterData->MaxHealth = 100.0f;
		MonsterData->ExperienceReward = 20;
		MonsterData->bDestroyOnDeath = false;
		MonsterData->HealthRegenerationPerSecond = 0;
		MonsterData->StaminaRegenerationPerSecond = 0;
		MonsterData->ManaRegenerationPerSecond = 0;
		Run->StartRun();
		if (!SpawnPlayer(Owner) || !SpawnPlayer(Other)) { SetupError = TEXT("Native player identity setup failed"); }
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

	bool SpawnPlayer(FPlayer& Player)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player.State = World->SpawnActor<ARogue10mPlayerState>();
		Player.Controller = World->SpawnActor<ARogue10mMenuPlayerController>();
		Player.Character = World->SpawnActor<ARogue10mCatalogTestCharacter>(
			ARogue10mCatalogTestCharacter::StaticClass(), FTransform::Identity, Spawn);
		if (!Player.State || !Player.Controller || !Player.Character) { return false; }
		Player.Character->SetPlayerState(Player.State);
		Player.Controller->SetPlayerState(Player.State);
		Player.Controller->SetPawn(Player.Character);
		Player.Character->SetController(Player.Controller);
		auto* ASC = Player.State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(Player.State->GetRogueAttributeSet());
		ASC->InitAbilityActorInfo(Player.State, Player.Character);
		Player.State->GetRogueAttributeSet()->InitializeVitals(135.0f, 100.0f, 100.0f);
		if (!Player.State->RestoreGrowth(FRogue10mGrowthSnapshot())) { return false; }
		Player.Quests = URogue10mVillageQuestComponent::FindOrCreate(Player.Character, Definition.Get());
		if (!Player.Quests) { return false; }
		Player.State->DispatchBeginPlay();
		return true;
	}

	bool Check(FAutomationTestBase& Test) const
	{
		bool bValid = Test.TestTrue(FString(TEXT("Initialized native no-GI combat-credit fixture: ")) + SetupError,
			SetupError.IsEmpty() && World && World->IsInitialized() && World->AreActorsInitialized()
			&& !World->GetGameInstance() && !World->HasBegunPlay() && Run && Run->IsRunActive());
		bValid &= Test.TestTrue(TEXT("Real native authority GameMode and Run are linked through public framework setup"),
			Mode && Mode->GetClass() == AGameModeBase::StaticClass() && Mode->HasAuthority()
			&& Mode->GetWorld() == World && !Mode->HasActorBegunPlay()
			&& World->GetAuthGameMode() == Mode && Mode->GetGameState<ARogue10mGameState>() == Run
			&& Run && Run->GetWorld() == World && Run->AuthorityGameMode == Mode);
		for (const FPlayer* Player : {&Owner, &Other})
		{
			bValid &= Test.TestTrue(TEXT("Actual unbegun character/controller and begun quest-owning PlayerState agree"),
				Player->Character && Player->State && Player->Controller && Player->Quests
				&& !Player->Character->HasActorBegunPlay() && !Player->Controller->HasActorBegunPlay()
				&& Player->State->HasActorBegunPlay() && Player->Quests->HasBegunPlay()
				&& Player->Character->GetPlayerState() == Player->State && Player->State->GetPawn() == Player->Character
				&& Player->Controller->GetPlayerState<ARogue10mPlayerState>() == Player->State
				&& Player->Controller->GetPawn() == Player->Character && Player->Character->GetController() == Player->Controller
				&& !Player->Character->IsDead() && Player->Quests->GetDefinition() == Definition.Get());
		}
		return bValid;
	}

	FName CombatId() const { return Definition->Quests[2].QuestId; }
	FName DeathReceipt() const { return Rogue10mVillageQuestEncounterDeathReceipt(CombatId(), 1); }
	FName CreditReceipt() const { return Rogue10mVillageQuestEncounterReceipt(CombatId(), 1); }

	ARogue10mVillageQuestMonster* SpawnMonster(bool bRecovery = false, int32 QuestSlot = 2)
	{
		auto* Monster = World->SpawnActorDeferred<ARogue10mVillageQuestMonster>(
			ARogue10mVillageQuestMonster::StaticClass(), FTransform::Identity, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Monster) { return nullptr; }
		Monster->AutoPossessAI = EAutoPossessAI::Disabled;
		Monster->QuestId = Definition->Quests[QuestSlot].QuestId;
		Monster->ObjectiveIndex = 1;
		Monster->QuestDefinition = Definition.Get();
		if (!Monster->ConfigureQuestMonsterData(MonsterData.Get())) { Monster->Destroy(); return nullptr; }
		if (bRecovery) { FRogue10mVillageQuestCombatCreditTestAccess::MarkRecovery(*Monster); }
		Monster->GetAbilitySystemComponent()->AddAttributeSetSubobject(Monster->GetRogueAttributeSet());
		Monster->FinishSpawning(FTransform::Identity);
		Monster->DispatchBeginPlay();
		return Monster;
	}

	bool Kill(FAutomationTestBase& Test, ARogue10mVillageQuestMonster* Monster, FPlayer* Player)
	{
		if (!Test.TestTrue(TEXT("Real initialized non-test monster has no AI"), Monster && Monster->HasActorBegunPlay()
			&& !Monster->IsDead() && !Monster->IsCombatTestInstance() && !Monster->GetController()
			&& Monster->GetRogueAttributeSet()->GetHealth() > 0.0f)) { return false; }
		const float Applied = Monster->TakeDamage(1000.0f, FDamageEvent(), Player ? Player->Controller : nullptr,
			Player ? Player->Character : nullptr);
		return Test.TestTrue(TEXT("Actual TakeDamage applies lethal damage"), Applied > 0.0f && Monster->IsDead());
	}

	bool ReadySupply(FAutomationTestBase& Test)
	{
		const FName Id = Definition->Quests[0].QuestId;
		return Test.TestTrue(TEXT("Accept independent supply quest"), Owner.Quests->AcceptQuest(Id, Owner.Character))
			&& Test.TestTrue(TEXT("Observe independent supply goal one"), Owner.Quests->ObserveObjective(Id, 0, Owner.Character))
			&& Test.TestTrue(TEXT("Observe independent supply goal two"), Owner.Quests->ObserveObjective(Id, 1, Owner.Character));
	}

	void CheckNoCredit(FAutomationTestBase& Test) const
	{
		Test.TestFalse(TEXT("Original player has no combat fact"), Owner.Quests->HasObservedObjective(CombatId(), 1));
		Test.TestFalse(TEXT("Other player has no redirected combat fact"), Other.Quests->HasObservedObjective(CombatId(), 1));
		Test.TestFalse(TEXT("No completed encounter receipt exists without a fact"), Run->Tags.Contains(CreditReceipt()));
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestLethalOwnerCreditTest,
	"Rogue10m.VillageQuest.CombatCredit.AttributedLethalOwnerReceivesOneFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestLethalOwnerCreditTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this)) { return false; }
	auto* Monster = F.SpawnMonster();
	if (!TestNotNull(TEXT("Actual quest monster"), Monster)) { return false; }
	TestEqual(TEXT("Fixture monster starts with configured health"), Monster->GetRogueAttributeSet()->GetHealth(), 100.0f);
	TestTrue(TEXT("Original player deals nonlethal damage"), Monster->TakeDamage(10.0f, FDamageEvent(), F.Owner.Controller, F.Owner.Character) > 0.0f);
	TestFalse(TEXT("Nonlethal damage creates no death ledger entry"), F.Run->Tags.Contains(F.DeathReceipt()));
	if (!F.Kill(*this, Monster, &F.Other)) { return false; }
	TestTrue(TEXT("Physical death is recorded"), F.Run->Tags.Contains(F.DeathReceipt()));
	TestFalse(TEXT("Earlier nonlethal instigator receives no fact"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Exact lethal owner receives the combat fact"), F.Other.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Completed encounter receipt follows the recorded fact"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestEqual(TEXT("Positive control awards configured XP"), F.Other.State->GetCurrentExperience(), 20.0f);
	TestEqual(TEXT("Positive control awards base death gold"), F.Other.Inventory().GetGold(), 2);
	TestEqual(TEXT("Positive control records one monster defeat"), F.Other.State->GetMonsterDefeatCount(F.MonsterData->MonsterId), 1);
	TestTrue(TEXT("Positive control enters the ordinary death-loot path"), Monster->GetMonsterLootComponent()->HasProcessedDeathLoot());
	const auto Inventory = F.Other.Inventory().CaptureInventory();
	const auto Growth = F.Other.State->CaptureGrowth();
	Monster->TakeDamage(1000.0f, FDamageEvent(), F.Owner.Controller, F.Owner.Character);
	FRogue10mVillageQuestCombatCreditTestAccess::Flush(*F.Other.Quests);
	TestTrue(TEXT("Corpse damage and retry cannot reissue inventory rewards"), SameInventory(Inventory, F.Other.Inventory().CaptureInventory()));
	TestTrue(TEXT("Corpse damage and retry cannot reissue XP or defeat credit"), SameGrowth(Growth, F.Other.State->CaptureGrowth()));
	TestEqual(TEXT("One kill records exactly one objective fact"), F.Other.Quests->GetObjectiveProgress(F.CombatId()), 1);
	TestFalse(TEXT("Corpse damage cannot transfer credit to another instigator"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Corpse may be destroyed after credit"), Monster->Destroy());
	TestTrue(TEXT("PlayerState credit survives corpse destruction"),
		URogue10mVillageQuestComponent::FindOrCreate(F.Other.Character) == F.Other.Quests
		&& F.Other.Quests->HasObservedObjective(F.CombatId(), 1));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestReentrantLethalContextTest,
	"Rogue10m.VillageQuest.CombatCredit.InventoryReentryCannotRedirectLethalContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestReentrantLethalContextTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this)) { return false; }
	auto* Monster = F.SpawnMonster();
	if (!TestNotNull(TEXT("Actual quest monster"), Monster)) { return false; }
	TStrongObjectPtr<URogue10mVillageQuestDataAsset> OtherDefinition(NewObject<URogue10mVillageQuestDataAsset>());
	OtherDefinition->SupportedWorld = F.Definition->SupportedWorld;
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	int32 Callbacks = 0;
	Observer->OnNotification = [&]
	{
		if (++Callbacks != 1) { return; }
		TestTrue(TEXT("Actual gold callback occurs after death starts"), Monster->IsDead());
		TestTrue(TEXT("Death ledger is committed before reward callbacks"), F.Run->Tags.Contains(F.DeathReceipt()));
		Monster->QuestId = F.Definition->Quests[1].QuestId;
		Monster->ObjectiveIndex = 0;
		Monster->QuestDefinition = OtherDefinition.Get();
		// Already-dead TakeDamage can return positive; only its side effects must be inert.
		Monster->TakeDamage(1.0f, FDamageEvent(), F.Other.Controller, F.Other.Character);
	};
	F.Owner.Inventory().OnInventoryChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	const bool bKilled = F.Kill(*this, Monster, &F.Owner);
	F.Owner.Inventory().OnInventoryChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Outer actual lethal damage completes"), bKilled);
	TestEqual(TEXT("Actual base-gold notification exercised synchronous reentry once"), Callbacks, 1);
	TestTrue(TEXT("Captured original quest/definition still receives the lethal fact"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestFalse(TEXT("Mutable actor quest fields cannot redirect the fact"), F.Owner.Quests->HasObservedObjective(F.Definition->Quests[1].QuestId, 0));
	TestFalse(TEXT("Nested corpse damage cannot replace the captured player"), F.Other.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Receipt uses the original lethal objective"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestEqual(TEXT("Synchronous reentry creates no extra gold"), F.Owner.Inventory().GetGold(), 2);
	TestEqual(TEXT("Synchronous reentry creates no extra XP"), F.Owner.State->GetCurrentExperience(), 20.0f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestUnattributedRecoveryTest,
	"Rogue10m.VillageQuest.CombatCredit.UnattributedDeathKeepsRecoveryOpportunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestUnattributedRecoveryTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this)) { return false; }
	auto* Original = F.SpawnMonster();
	if (!F.Kill(*this, Original, nullptr)) { return false; }
	TestTrue(TEXT("Unattributed physical death is recorded"), F.Run->Tags.Contains(F.DeathReceipt()));
	F.CheckNoCredit(*this);
	TestTrue(TEXT("Ordinary unattributed death still processes its base loot policy"), Original->GetMonsterLootComponent()->HasProcessedDeathLoot());
	if (!TestTrue(TEXT("Destroy unattributed corpse"), Original->Destroy())) { return false; }
	TestTrue(TEXT("Physical death ledger survives corpse removal"), F.Run->Tags.Contains(F.DeathReceipt()));
	const auto Inventory = F.Owner.Inventory().CaptureInventory();
	const auto Growth = F.Owner.State->CaptureGrowth();
	auto* Recovery = F.SpawnMonster(true);
	if (!F.Kill(*this, Recovery, &F.Owner)) { return false; }
	TestTrue(TEXT("Recovery records the previously missing player fact"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Recovery completion creates the credited receipt"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestFalse(TEXT("Recovery bypasses death-loot emission entirely"), Recovery->GetMonsterLootComponent()->HasProcessedDeathLoot());
	TestTrue(TEXT("Recovery creates no gold or inventory mutation"), SameInventory(Inventory, F.Owner.Inventory().CaptureInventory()));
	TestTrue(TEXT("Recovery creates no XP or monster-defeat mutation"), SameGrowth(Growth, F.Owner.State->CaptureGrowth()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestOwnerSwapRecoveryTest,
	"Rogue10m.VillageQuest.CombatCredit.OwnerSwapDiscardsCreditAndRecoveryCannotFarmRewards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestOwnerSwapRecoveryTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this)) { return false; }
	auto* Original = F.SpawnMonster();
	auto* ExistingDuplicate = F.SpawnMonster(false);
	if (!TestNotNull(TEXT("Actual quest monster"), Original)
		|| !TestNotNull(TEXT("Ordinary duplicate exists before any death ledger entry"), ExistingDuplicate)) { return false; }
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	int32 Callbacks = 0;
	Observer->OnNotification = [&]
	{
		if (++Callbacks == 1) { F.Owner.Character->SetPlayerState(F.Other.State); }
	};
	F.Owner.Inventory().OnInventoryChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	const bool bKilled = F.Kill(*this, Original, &F.Owner);
	F.Owner.Inventory().OnInventoryChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Actual lethal hit completes despite owner swap"), bKilled);
	TestEqual(TEXT("Owner swap occurred in the real base-gold callback"), Callbacks, 1);
	F.CheckNoCredit(*this);
	TestTrue(TEXT("Uncredited physical death remains available for recovery"), F.Run->Tags.Contains(F.DeathReceipt()));
	// Restore both pawn backlinks after SetPlayerState temporarily moved custody.
	F.Owner.Character->SetPlayerState(F.Owner.State);
	F.Other.Character->SetPlayerState(F.Other.State);
	FRogue10mVillageQuestCombatCreditTestAccess::Flush(*F.Owner.Quests);
	F.CheckNoCredit(*this); // Restored custody must not resurrect the discarded death.
	TestEqual(TEXT("Ordinary death positive control paid gold once"), F.Owner.Inventory().GetGold(), 2);
	TestEqual(TEXT("Ordinary death positive control paid XP once"), F.Owner.State->GetCurrentExperience(), 20.0f);
	TestEqual(TEXT("Ordinary death positive control recorded one defeat"), F.Owner.State->GetMonsterDefeatCount(F.MonsterData->MonsterId), 1);
	TestTrue(TEXT("Ordinary death positive control processed loot once"), Original->GetMonsterLootComponent()->HasProcessedDeathLoot());
	if (!TestTrue(TEXT("Destroy uncredited corpse before retry"), Original->Destroy())) { return false; }
	const auto OwnerInventory = F.Owner.Inventory().CaptureInventory();
	const auto OtherInventory = F.Other.Inventory().CaptureInventory();
	const auto OwnerGrowth = F.Owner.State->CaptureGrowth();
	const auto OtherGrowth = F.Other.State->CaptureGrowth();
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		// First retry uses explicit recovery setup. The second actor already
		// existed as ordinary; only the production death ledger may suppress it.
		auto* Recovery = Attempt == 0 ? F.SpawnMonster(true) : ExistingDuplicate;
		if (!F.Kill(*this, Recovery, &F.Owner)) { return false; }
		Recovery->TakeDamage(1000.0f, FDamageEvent(), F.Other.Controller, F.Other.Character);
		FRogue10mVillageQuestCombatCreditTestAccess::Flush(*F.Owner.Quests);
		TestTrue(TEXT("Recovery gives the exact original owner one combat fact"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
		TestEqual(TEXT("Repeated recovery cannot add another fact"), F.Owner.Quests->GetObjectiveProgress(F.CombatId()), 1);
		TestFalse(TEXT("Recovery never invokes base death-loot processing"), Recovery->GetMonsterLootComponent()->HasProcessedDeathLoot());
		TestTrue(TEXT("Recovery cannot farm original-owner gold/items"), SameInventory(OwnerInventory, F.Owner.Inventory().CaptureInventory()));
		TestTrue(TEXT("Recovery cannot farm original-owner XP/defeats"), SameGrowth(OwnerGrowth, F.Owner.State->CaptureGrowth()));
		TestTrue(TEXT("Recovery cannot pay redirected-owner gold/items"), SameInventory(OtherInventory, F.Other.Inventory().CaptureInventory()));
		TestTrue(TEXT("Recovery cannot pay redirected-owner XP/defeats"), SameGrowth(OtherGrowth, F.Other.State->CaptureGrowth()));
		TestTrue(TEXT("Recovery corpse can leave without losing credit"), Recovery->Destroy());
	}
	TestTrue(TEXT("Successful recovery has a completed receipt"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestFalse(TEXT("Owner swap never awards the replacement player's combat fact"), F.Other.Quests->HasObservedObjective(F.CombatId(), 1));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestCorpseIndependentQueueTest,
	"Rogue10m.VillageQuest.CombatCredit.QueuedCreditSurvivesCorpseDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestCorpseIndependentQueueTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this) || !F.ReadySupply(*this)) { return false; }
	auto* Monster = F.SpawnMonster();
	if (!TestNotNull(TEXT("Actual quest monster"), Monster)) { return false; }
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	bool bHandled = false, bKilled = false, bDestroyed = false;
	int32 DeferredDeaths = 0;
	Observer->OnNotification = [&]
	{
		if (bHandled) { return; }
		bHandled = true;
		++DeferredDeaths;
		bKilled = F.Kill(*this, Monster, &F.Owner);
		TestFalse(TEXT("Actual death cannot observe through the in-progress quest reward guard"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
		TestFalse(TEXT("Pending death has no premature completed receipt"), F.Run->Tags.Contains(F.CreditReceipt()));
		bDestroyed = Monster->Destroy();
	};
	F.Owner.Inventory().OnInventoryChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	FText Reason;
	const bool bClaimed = F.Owner.Quests->ClaimReward(F.Definition->Quests[0].QuestId, F.Owner.Character, Reason);
	F.Owner.Inventory().OnInventoryChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Independent outer quest reward completes"), bClaimed);
	TestEqual(TEXT("Exactly one actual lethal event entered the guarded queue"), DeferredDeaths, 1);
	TestTrue(TEXT("Corpse was destroyed before outer reward returned"), bKilled && bDestroyed && !IsValid(Monster));
	TestTrue(TEXT("Component drains its retained credit after reward guard exits"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Completed receipt commits after corpse-independent delivery"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestEqual(TEXT("Outer quest and one ordinary death pay exact gold"), F.Owner.Inventory().GetGold(), F.Definition->Quests[0].RewardGold + 2);
	const auto Inventory = F.Owner.Inventory().CaptureInventory();
	const auto Growth = F.Owner.State->CaptureGrowth();
	FRogue10mVillageQuestCombatCreditTestAccess::Flush(*F.Owner.Quests);
	TestTrue(TEXT("Retry after corpse destruction cannot duplicate gold/items"), SameInventory(Inventory, F.Owner.Inventory().CaptureInventory()));
	TestTrue(TEXT("Retry after corpse destruction cannot duplicate XP/defeats"), SameGrowth(Growth, F.Owner.State->CaptureGrowth()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestResetInvalidatesDeathQueueTest,
	"Rogue10m.VillageQuest.CombatCredit.RunningEventInvalidatesQueuedDeathCredit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestResetInvalidatesDeathQueueTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F;
	if (!F.Check(*this) || !F.ReadySupply(*this)) { return false; }
	auto* Monster = F.SpawnMonster();
	if (!TestNotNull(TEXT("Actual quest monster"), Monster)) { return false; }
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	bool bHandled = false, bKilled = false, bDestroyed = false;
	int32 ResetCallbacks = 0;
	const uint64 OriginalEpoch = FRogue10mVillageQuestCombatCreditTestAccess::Epoch(*F.Owner.Quests);
	Observer->OnNotification = [&]
	{
		if (bHandled) { return; }
		bHandled = true;
		++ResetCallbacks;
		bKilled = F.Kill(*this, Monster, &F.Owner);
		TestFalse(TEXT("Guarded lethal event is pending before reset"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
		// Explicit new-Running event injection: no GI RunId, restart or travel is simulated.
		F.Run->OnRunPhaseChanged.Broadcast(ERogue10mRunPhase::Running);
		TestTrue(TEXT("Running invalidates the credit epoch immediately inside the reward guard"),
			FRogue10mVillageQuestCombatCreditTestAccess::Epoch(*F.Owner.Quests) != OriginalEpoch);
		bDestroyed = Monster->Destroy();
	};
	F.Owner.Inventory().OnInventoryChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	FText Reason;
	const bool bClaimed = F.Owner.Quests->ClaimReward(F.Definition->Quests[0].QuestId, F.Owner.Character, Reason);
	F.Owner.Inventory().OnInventoryChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Promised outer reward completes"), bClaimed);
	TestEqual(TEXT("Actual reward callback injected one reset boundary"), ResetCallbacks, 1);
	TestTrue(TEXT("Old corpse was destroyed before delivery could retry"), bKilled && bDestroyed);
	FRogue10mVillageQuestCombatCreditTestAccess::Flush(*F.Owner.Quests);
	F.CheckNoCredit(*this);
	TestEqual(TEXT("Fresh epoch has no old combat progress"), F.Owner.Quests->GetObjectiveProgress(F.CombatId()), 0);
	TestTrue(TEXT("Primary quest reset also completed"), F.Owner.Quests->GetQuestState(F.Definition->Quests[0].QuestId) == ERogue10mVillageQuestState::Available);
	TestEqual(TEXT("Invalidated delivery creates no second base or quest payment"), F.Owner.Inventory().GetGold(), F.Definition->Quests[0].RewardGold + 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestLowerSlotTimerDeliveryTest,
	"Rogue10m.VillageQuest.CombatCredit.ReentrantLowerSlotDeathDrainsThroughScheduledTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestLowerSlotTimerDeliveryTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F(true);
	if (!F.Check(*this)) { return false; }
	auto* Higher = F.SpawnMonster();
	auto* Lower = F.SpawnMonster(false, 1);
	if (!TestNotNull(TEXT("Higher-slot actual quest monster"), Higher)
		|| !TestNotNull(TEXT("Lower-slot actual quest monster"), Lower)) { return false; }
	const FName LowerId = F.Definition->Quests[1].QuestId;
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> Observer(NewObject<URogue10mVillageQuestRewardTestProxy>());
	bool bNestedDeathStarted = false, bNestedKilled = false;
	int32 Notifications = 0;
	Observer->OnNotification = [&]
	{
		++Notifications;
		if (bNestedDeathStarted || !F.Owner.Quests->HasObservedObjective(F.CombatId(), 1)) { return; }
		bNestedDeathStarted = true;
		// The slot-2 observation is notifying while its real Flush loop is active.
		// A slot-1 death therefore arrives after the loop has passed its position.
		bNestedKilled = F.Kill(*this, Lower, &F.Owner);
		TestFalse(TEXT("Nested lower-slot death is not observed by the reentry-blocked flush"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
		TestTrue(TEXT("Nested lower-slot death is retained by the actual component queue"),
			FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
	};
	F.Owner.Quests->OnQuestChanged.AddDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	const bool bHigherKilled = F.Kill(*this, Higher, &F.Owner);
	TestTrue(TEXT("Both real lethal events completed"), bHigherKilled && bNestedKilled);
	TestEqual(TEXT("Only the higher slot notified before the deferred timer"), Notifications, 1);
	TestTrue(TEXT("Higher slot committed its combat fact"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestFalse(TEXT("Lower slot remains unobserved after the outer loop passes it"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
	TestTrue(TEXT("A real deferred component flush remains scheduled"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
	const auto Inventory = F.Owner.Inventory().CaptureInventory();
	const auto Growth = F.Owner.State->CaptureGrowth();
	{
		// Tick only this isolated world's actual TimerManager. UE permits next-tick
		// delegates to enter a pending heap first; two scoped frame advances cover
		// both timer-delay settings without ticking/beginning the gameplay world.
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
		F.World->GetTimerManager().Tick(0.01f);
		++GFrameCounter;
		F.World->GetTimerManager().Tick(0.01f);
	}
	F.Owner.Quests->OnQuestChanged.RemoveDynamic(Observer.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestEqual(TEXT("Scheduled delivery sends the second actual quest notification"), Notifications, 2);
	TestTrue(TEXT("Scheduled timer delivers the skipped lower-slot fact without another explicit flush"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
	TestTrue(TEXT("Lower-slot delivery commits its completed receipt"), F.Run->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(LowerId, 1)));
	TestFalse(TEXT("Scheduled timer drains lower-slot pending work"), FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
	TestFalse(TEXT("Scheduled timer clears its scheduling guard"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
	TestTrue(TEXT("Deferred delivery issues no extra gold/items"), SameInventory(Inventory, F.Owner.Inventory().CaptureInventory()));
	TestTrue(TEXT("Deferred delivery issues no extra XP/defeats"), SameGrowth(Growth, F.Owner.State->CaptureGrowth()));
	TestFalse(TEXT("Deferred delivery does not begin the gameplay world"), F.World->HasBegunPlay());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mVillageQuestNestedRewardTimerDeliveryTest,
	"Rogue10m.VillageQuest.CombatCredit.GuardedNestedRewardsScheduleSkippedLowerSlotDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mVillageQuestNestedRewardTimerDeliveryTest::RunTest(const FString&)
{
	using namespace Rogue10mVillageQuestCombatCreditTests;
	FFixture F(true);
	if (!F.Check(*this) || !F.ReadySupply(*this)) { return false; }
	const FName OuterId = F.Definition->Quests[0].QuestId;
	const FName InnerId = F.Definition->Quests[4].QuestId;
	const FName LowerId = F.Definition->Quests[1].QuestId;
	if (!TestTrue(TEXT("Accept the independent nested-reward quest"), F.Owner.Quests->AcceptQuest(InnerId, F.Owner.Character))
		|| !TestTrue(TEXT("Observe nested-reward quest goal one"), F.Owner.Quests->ObserveObjective(InnerId, 0, F.Owner.Character))
		|| !TestTrue(TEXT("Observe nested-reward quest goal two"), F.Owner.Quests->ObserveObjective(InnerId, 1, F.Owner.Character))) { return false; }
	auto* Higher = F.SpawnMonster();
	auto* Lower = F.SpawnMonster(false, 1);
	if (!TestNotNull(TEXT("Higher-slot actual quest monster"), Higher)
		|| !TestNotNull(TEXT("Lower-slot actual quest monster"), Lower)) { return false; }
	enum class EStage { AwaitOuterGold, KillingHigher, AwaitInnerGold, KillingLower, Complete };
	EStage Stage = EStage::AwaitOuterGold;
	bool bHigherKilled = false, bLowerKilled = false, bInnerClaimStarted = false, bInnerClaimed = false;
	int32 OuterRewardCallbacks = 0, InnerRewardCallbacks = 0;
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> InventoryObserver(NewObject<URogue10mVillageQuestRewardTestProxy>());
	TStrongObjectPtr<URogue10mVillageQuestRewardTestProxy> QuestObserver(NewObject<URogue10mVillageQuestRewardTestProxy>());
	InventoryObserver->OnNotification = [&]
	{
		if (Stage == EStage::AwaitOuterGold)
		{
			Stage = EStage::KillingHigher; // Ignore the monster's recursively emitted gold notification.
			++OuterRewardCallbacks;
			TestFalse(TEXT("No combat flush timer exists before the first guarded death"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
			bHigherKilled = F.Kill(*this, Higher, &F.Owner);
			TestTrue(TEXT("First guarded death remains in the real component queue"), FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 2));
			TestFalse(TEXT("First reward guard prevents timer scheduling"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
			TestFalse(TEXT("First reward guard also prevents observation"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
			Stage = EStage::AwaitInnerGold;
		}
		else if (Stage == EStage::AwaitInnerGold)
		{
			Stage = EStage::KillingLower;
			++InnerRewardCallbacks;
			bLowerKilled = F.Kill(*this, Lower, &F.Owner);
			TestTrue(TEXT("Nested guarded lower-slot death remains queued"), FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
			TestFalse(TEXT("Nested reward guard still cannot schedule the missing timer"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
			TestFalse(TEXT("Nested guarded death cannot bypass the active outer flush"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
			Stage = EStage::Complete;
		}
	};
	QuestObserver->OnNotification = [&]
	{
		if (bInnerClaimStarted || Stage != EStage::AwaitInnerGold
			|| !F.Owner.Quests->HasObservedObjective(F.CombatId(), 1)) { return; }
		bInnerClaimStarted = true;
		FText InnerReason;
		// The outer Flush has passed slot 1 and is notifying from slot 2. A real
		// nested reward installs its guard before its gold callback kills slot 1.
		bInnerClaimed = F.Owner.Quests->ClaimReward(InnerId, F.Owner.Character, InnerReason);
		TestTrue(TEXT("Inner reward scope cannot consume lower-slot pending work while outer flush is active"),
			FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
		TestFalse(TEXT("No timer has been scheduled before the outer flush exits"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
	};
	F.Owner.Inventory().OnInventoryChanged.AddDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Owner.Quests->OnQuestChanged.AddDynamic(QuestObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	FText Reason;
	const bool bOuterClaimed = F.Owner.Quests->ClaimReward(OuterId, F.Owner.Character, Reason);
	F.Owner.Inventory().OnInventoryChanged.RemoveDynamic(InventoryObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	F.Owner.Quests->OnQuestChanged.RemoveDynamic(QuestObserver.Get(), &URogue10mVillageQuestRewardTestProxy::HandleInventoryChanged);
	TestTrue(TEXT("Both actual rewards and both actual deaths completed"), bOuterClaimed && bInnerClaimed && bHigherKilled && bLowerKilled && Stage == EStage::Complete);
	TestEqual(TEXT("Outer actual reward callback ran its bounded stage once"), OuterRewardCallbacks, 1);
	TestEqual(TEXT("Inner actual reward callback ran its bounded stage once"), InnerRewardCallbacks, 1);
	TestTrue(TEXT("Outer quest receipt is committed before deferred combat delivery"), F.Owner.Quests->GetQuestState(OuterId) == ERogue10mVillageQuestState::Claimed);
	TestTrue(TEXT("Inner quest receipt is committed before deferred combat delivery"), F.Owner.Quests->GetQuestState(InnerId) == ERogue10mVillageQuestState::Claimed);
	TestTrue(TEXT("Higher-slot combat credit was delivered by the outer flush"), F.Owner.Quests->HasObservedObjective(F.CombatId(), 1));
	TestTrue(TEXT("Higher-slot combat receipt is committed"), F.Run->Tags.Contains(F.CreditReceipt()));
	TestFalse(TEXT("Outer loop has not revisited the skipped lower-slot fact"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
	TestFalse(TEXT("Pending lower-slot combat receipt cannot commit early"), F.Run->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(LowerId, 1)));
	TestTrue(TEXT("Skipped lower-slot credit is pending after both reward scopes exit"), FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
	TestTrue(TEXT("Outer flush scope exit schedules previously unscheduled pending work"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
	const auto Inventory = F.Owner.Inventory().CaptureInventory();
	const auto Growth = F.Owner.State->CaptureGrowth();
	{
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
		F.World->GetTimerManager().Tick(0.01f);
		++GFrameCounter;
		F.World->GetTimerManager().Tick(0.01f);
	}
	TestTrue(TEXT("Scheduled timer delivers the guarded nested lower-slot death without manual flush"), F.Owner.Quests->HasObservedObjective(LowerId, 1));
	TestTrue(TEXT("Delivered lower-slot death commits its encounter receipt"), F.Run->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(LowerId, 1)));
	TestFalse(TEXT("Deferred timer clears lower-slot pending work"), FRogue10mVillageQuestCombatCreditTestAccess::HasPending(*F.Owner.Quests, 1));
	TestFalse(TEXT("Deferred timer releases its schedule guard"), FRogue10mVillageQuestCombatCreditTestAccess::IsFlushScheduled(*F.Owner.Quests));
	TestTrue(TEXT("Deferred delivery cannot repay either quest or monster"), SameInventory(Inventory, F.Owner.Inventory().CaptureInventory()));
	TestTrue(TEXT("Deferred delivery cannot repeat XP or monster-defeat rewards"), SameGrowth(Growth, F.Owner.State->CaptureGrowth()));
	TestEqual(TEXT("Exactly two quest rewards and two base monster gold grants occur"), F.Owner.Inventory().GetGold(),
		F.Definition->Quests[0].RewardGold + F.Definition->Quests[4].RewardGold + 4);
	TestEqual(TEXT("Exactly two ordinary deaths grant XP"), F.Owner.State->GetCurrentExperience(), 40.0f);
	TestEqual(TEXT("Exactly two ordinary deaths grant monster-defeat credit"), F.Owner.State->GetMonsterDefeatCount(F.MonsterData->MonsterId), 2);
	return !HasAnyErrors();
}
#endif
