#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/DamageEvents.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10mCharacterProfileSaveGame.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mLootTableDataAsset.h"
#include "Rogue10mLootDeliverySubsystem.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterLootComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSaveGame.h"
#include <limits>

struct FRogue10mMonsterLootTestAccess
{
	static void Configure(ARogue10mBasicMonster& Monster, URogue10mMonsterDataAsset* Data)
	{
		Monster.MonsterData = Data;
		Monster.bDestroyOnDeath = false;
		Monster.AutoPossessAI = EAutoPossessAI::Disabled;
	}
};

namespace Rogue10mMonsterLootTests
{
static URogue10mItemDataAsset* Item(int32 MaxStack = 20)
{
	auto* Result = NewObject<URogue10mItemDataAsset>();
	Result->ItemId = TEXT("LootTestItem");
	Result->MaxStackSize = MaxStack;
	Result->InventorySize = FIntPoint(1, 1);
	return Result;
}
static URogue10mLootTableDataAsset* Guaranteed(URogue10mItemDataAsset* Data)
{
	auto* Table = NewObject<URogue10mLootTableDataAsset>();
	FRogue10mLootEntry Row; Row.ItemData = Data; Row.DropChance = 1.0f; Table->Entries.Add(Row);
	return Table;
}
static bool Same(const FRogue10mInventorySnapshot& A, const FRogue10mInventorySnapshot& B)
{ return FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); }

static bool HasIsolatedSaveOptions()
{
	TArray<FString> Tokens, Options; FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
	const TCHAR* Names[] = { TEXT("RogueProfileSaveSlot="), TEXT("RogueProgressionSavePrefix="), TEXT("RogueSkillKeySaveSlot=") };
	const TCHAR* Prefixes[] = { TEXT("Rogue10m_Town_QA_"), TEXT("Rogue10m_Progression_QA_"), TEXT("Rogue10m_Fighter_QA_") };
	FString Suffix;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		int32 Count = 0; FString Value;
		for (const FString& Option : Options)
		{
			if (Option.StartsWith(Names[Index], ESearchCase::IgnoreCase)) { ++Count; Value = Option.Mid(FCString::Strlen(Names[Index])); }
		}
		if (Count != 1 || !Value.StartsWith(Prefixes[Index])) { return false; }
		const FString ThisSuffix = Value.Mid(FCString::Strlen(Prefixes[Index]));
		if (ThisSuffix.IsEmpty() || ThisSuffix.Len() > 32) { return false; }
		for (TCHAR C : ThisSuffix) { if (!((FChar::IsAlnum(C) && C < 128) || C == TEXT('_'))) { return false; } }
		if (Index == 0) { Suffix = ThisSuffix; } else if (Suffix != ThisSuffix) { return false; }
	}
	return true;
}

struct FFixture
{
	TStrongObjectPtr<UGameInstance> Instance;
	UWorld* World = nullptr;
	ARogue10mCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	AActor* Floor = nullptr;
	FString SetupError;
	FFixture()
	{
		// InitializeStandalone initializes real GI subsystems. Require isolated namespaces
		// before that call so native automation never reads the user's default profile.
		if (!GEngine || !HasIsolatedSaveOptions())
		{ SetupError = TEXT("Requires one matching set of RogueProfileSaveSlot/RogueProgressionSavePrefix/RogueSkillKeySaveSlot QA overrides"); return; }
		// Seed only the already-validated isolated slot before GI subsystem loading.
		// Keep it empty/unselected so this fixture never attaches player persistence.
		FString ProfileSlot;
		if (!FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot))
		{ SetupError = TEXT("Missing isolated profile slot"); return; }
		if (!UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0))
		{
			auto* EmptyProfile = NewObject<URogue10mCharacterProfileSaveGame>();
			if (!UGameplayStatics::SaveGameToSlot(EmptyProfile, ProfileSlot, 0))
			{ SetupError = TEXT("Failed to seed isolated empty profile save"); return; }
		}
		Instance.Reset(NewObject<UGameInstance>(GEngine));
		Instance->InitializeStandalone(FName(*FString::Printf(TEXT("MonsterLoot_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
		World = Instance->GetWorld(); // GI owns the single initialized Game WorldContext.
		if (!World || !World->GetWorldSettings()) { SetupError = TEXT("Standalone game world was not initialized"); return; }
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		const FURL URL;
		if (!World->SetGameMode(URL) || !World->GetAuthGameMode()) { SetupError = TEXT("Public SetGameMode failed to create AuthGameMode"); return; }
		World->InitializeActorsForPlay(URL);
		// Whole-world BeginPlay is intentionally deferred: this unit fixture needs an
		// empty inventory and no selected/possessed character or persistence attachment.
		Floor = World->SpawnActor<AActor>();
		if (!Floor) { SetupError = TEXT("Fixture floor spawn failed"); return; }
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
		Box->SetBoxExtent(FVector(1000.0f, 1000.0f, 20.0f));
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent(); Floor->SetActorLocation(FVector(0.0f, 0.0f, -20.0f));
		State = World->SpawnActor<ARogue10mPlayerState>();
		auto* CharacterClass = LoadClass<ARogue10mCharacter>(nullptr,
			TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		Character = CharacterClass ? World->SpawnActor<ARogue10mCharacter>(CharacterClass, FVector(500.0f, 0.0f, 120.0f), FRotator::ZeroRotator) : nullptr;
		if (!Character || !State) { return; }
		Character->SetPlayerState(State);
		auto* ASC = State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet()); ASC->InitAbilityActorInfo(State, Character);
		State->GetRogueAttributeSet()->InitializeVitals(135.0f, 100.0f, 100.0f);
		World->Tick(LEVELTICK_All, 0.01f);
	}
	~FFixture()
	{
		if (World)
		{
			World->BeginTearingDown();
			if (World->HasBegunPlay())
			{
				// End the world as well as its actors before subsystem shutdown.
				World->EndPlay(EEndPlayReason::Quit);
			}
			else
			{
				// Older unit fixtures manually begin only their monster actors.
				for (TActorIterator<AActor> It(World); It; ++It)
				{ if (It->HasActorBegunPlay()) { It->RouteEndPlay(EEndPlayReason::Quit); } }
			}
		}
		if (Instance.IsValid()) { Instance->Shutdown(); }
		if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	}
	bool Valid() const
	{ return Instance.IsValid() && World && World->GetGameInstance() == Instance.Get()
		&& World->IsInitialized() && World->AreActorsInitialized() && World->GetAuthGameMode()
		&& Character && State && Floor && SetupError.IsEmpty(); }
	URogue10mInventoryComponent& Inventory() { return *Character->GetInventoryComponent(); }
	ARogue10mBasicMonster* Monster(URogue10mLootTableDataAsset* Table)
	{
		auto* Data = NewObject<URogue10mMonsterDataAsset>();
		Data->LootTable = Table; Data->MaxHealth = 100.0f; Data->ExperienceReward = 0;
		Data->HealthRegenerationPerSecond = Data->StaminaRegenerationPerSecond = Data->ManaRegenerationPerSecond = 0.0f;
		Data->bDestroyOnDeath = false;
		FActorSpawnParameters Parameters; Parameters.bDeferConstruction = true;
		const FTransform Transform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 100.0f));
		auto* Result = World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(), Transform, Parameters);
		if (!Result) { return nullptr; }
		FRogue10mMonsterLootTestAccess::Configure(*Result, Data);
		// This isolated world dispatches BeginPlay manually, so register the real
		// attribute subobject explicitly before Monster.BeginPlay initializes vitals.
		Result->GetAbilitySystemComponent()->AddAttributeSetSubobject(Result->GetRogueAttributeSet());
		Result->FinishSpawning(Transform);
		if (!Result->HasActorBegunPlay()) { Result->DispatchBeginPlay(); }
		return Result;
	}
	TArray<ARogue10mDroppedItem*> Pickups() const
	{
		TArray<ARogue10mDroppedItem*> Result;
		for (TActorIterator<ARogue10mDroppedItem> It(World); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->Tags.Contains(TEXT("Rogue10m.MonsterLoot"))) { Result.Add(*It); }
		}
		return Result;
	}
	bool BeginOverlapMode()
	{
		// Component notifications require a begun world. No controller/profile is
		// selected, and the fixture already requires all three QA save overrides.
		if (!Valid()) { return false; }
		World->BeginPlay();
		if (!World->HasBegunPlay() || !Character->HasActorBegunPlay()) { return false; }
		Character->GetCharacterMovement()->DisableMovement();
		auto Snapshot = Inventory().CaptureInventory();
		if (Snapshot.Containers.IsEmpty()) { return false; }
		Snapshot.Containers.SetNum(1);
		Snapshot.Containers[0].Entries.Reset();
		Snapshot.Containers[0].GridSize = FIntPoint(1, 1);
		Snapshot.Containers[0].BagAsset.Reset();
		return Inventory().RestoreInventory(Snapshot);
	}
	ARogue10mDroppedItem* OverlapDrop(FAutomationTestBase& Test)
	{
		auto* Table = Guaranteed(Item(5));
		Table->Entries[0].MinQuantity = Table->Entries[0].MaxQuantity = 2;
		auto* Target = Monster(Table);
		if (!Test.TestNotNull(TEXT("Death-drop fixture monster"), Target) || !KillAndCheck(Test, *Target)) { return nullptr; }
		const auto Drops = Pickups();
		if (!Test.TestEqual(TEXT("One actual death drop before overlap"), Drops.Num(), 1)) { return nullptr; }
		return Drops[0];
	}
	bool EnterPickup(ARogue10mDroppedItem& Pickup)
	{
		const FVector Location = Pickup.GetActorLocation() + FVector(0.0f, 0.0f,
			Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 35.0f);
		Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Character->GetCapsuleComponent()->UpdateOverlaps();
		const auto* Mesh = Pickup.FindComponentByClass<UStaticMeshComponent>();
		return Mesh && Mesh->IsOverlappingActor(Character);
	}
	int32 CountOwned(FGuid InstanceId, int32* OutQuantity = nullptr)
	{
		int32 Count = 0;
		for (const auto& Container : Inventory().GetInventoryContainers())
		{
			for (const auto& Entry : Container.Entries)
			{
				if (Entry.InstanceId == InstanceId)
				{ ++Count; if (OutQuantity) { *OutQuantity = Entry.Quantity; } }
			}
		}
		return Count;
	}
	float Kill(ARogue10mBasicMonster& Target)
	{ return Target.TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr); }
	bool KillAndCheck(FAutomationTestBase& Test, ARogue10mBasicMonster& Target)
	{
		const float InitialHP = Target.GetRogueAttributeSet() ? Target.GetRogueAttributeSet()->GetHealth() : 0.0f;
		const FString Context = FString::Printf(TEXT("GM=%s role=%d damageable=%d initialHP=%.2f"),
			*GetNameSafe(World ? World->GetAuthGameMode() : nullptr), static_cast<int32>(Target.GetLocalRole()), Target.CanBeDamaged(), InitialHP);
		if (!Test.TestTrue(FString(TEXT("Living initialized damage fixture: ")) + Context, InitialHP > 0.0f && !Target.IsDead())) { return false; }
		const float Applied = Kill(Target);
		return Test.TestTrue(FString::Printf(TEXT("Actual lethal damage applied %.2f: %s"), Applied, *Context), Applied > 0.0f);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootTableBoundariesTest, "Rogue10m.MonsterLoot.TableBoundariesAndValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootTableBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	auto* Table = Guaranteed(Item(5));
	Table->Entries[0].MinQuantity = Table->Entries[0].MaxQuantity = 2;
	FRogue10mLootEntry Never = Table->Entries[0]; Never.DropChance = 0.0f; Table->Entries.Insert(Never, 0);
	FRandomStream Random(17); FText Reason; TArray<FRogue10mItemStack> Drops;
	TestTrue(TEXT("Valid table rolls"), Table->RollDrops(Random, Drops, Reason));
	TestEqual(TEXT("Zero chance never and one chance always"), Drops.Num(), 1);
	if (Drops.Num() == 1) { TestEqual(TEXT("Configured quantity"), Drops[0].Quantity, 2); TestTrue(TEXT("Valid unique instance"), Drops[0].InstanceId.IsValid()); }
	Table->Entries[0].DropChance = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN rejects complete table before any drop"), Table->RollDrops(Random, Drops, Reason)); TestEqual(TEXT("No partial output"), Drops.Num(), 0);
	Table->Entries[0].DropChance = 0.5f; Table->Entries[1].MaxQuantity = 6;
	TestFalse(TEXT("Reject quantity beyond item stack capacity"), Table->RollDrops(Random, Drops, Reason));
	Table->Entries[1].MaxQuantity = 2; Table->Entries[0].DropChance = 1.01f;
	TestFalse(TEXT("Reject probability outside zero to one"), Table->RollDrops(Random, Drops, Reason));
	Table->Entries[0].DropChance = 0.5f; Table->Entries[0].MinQuantity = 0;
	TestFalse(TEXT("Reject non-positive quantity"), Table->RollDrops(Random, Drops, Reason));
	Table->Entries[0].MinQuantity = 1; Table->Entries[0].ItemData = nullptr;
	TestFalse(TEXT("Reject missing item"), Table->RollDrops(Random, Drops, Reason));
	Table->Entries.Reset();
	TestTrue(TEXT("Empty table is a safe no-drop default"), Table->RollDrops(Random, Drops, Reason));
	Table->Entries.SetNum(URogue10mLootTableDataAsset::MaximumEntries + 1);
	TestFalse(TEXT("Bound authored row count"), Table->ValidateLootTable(Reason));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootSeedTest, "Rogue10m.MonsterLoot.SeededRollsAndUniqueInstances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootSeedTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	auto* Table = Guaranteed(Item()); Table->Entries[0].DropChance = 0.5f; Table->Entries[0].MaxQuantity = 4;
	FRandomStream A(123), B(123); TSet<FGuid> Ids; int32 DropCount = 0;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		TArray<FRogue10mItemStack> Left, Right; FText Reason;
		TestTrue(TEXT("First seeded roll"), Table->RollDrops(A, Left, Reason));
		TestTrue(TEXT("Second seeded roll"), Table->RollDrops(B, Right, Reason));
		TestEqual(TEXT("Same seed preserves chance result"), Left.Num(), Right.Num());
		if (Left.Num() == 1 && Right.Num() == 1)
		{
			TestEqual(TEXT("Same seed preserves quantity"), Left[0].Quantity, Right[0].Quantity);
			TestTrue(TEXT("Quantity in configured range"), Left[0].Quantity >= 1 && Left[0].Quantity <= 4);
			TestTrue(TEXT("Distinct instance per generated stack"), Left[0].InstanceId.IsValid() && !Ids.Contains(Left[0].InstanceId));
			Ids.Add(Left[0].InstanceId); ++DropCount;
		}
	}
	TestTrue(TEXT("Seed exercises both outcomes"), DropCount > 0 && DropCount < 64);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootDeathTest, "Rogue10m.MonsterLoot.ActualDeathExactlyOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootDeathTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(FString(TEXT("Isolated real game world and player: ")) + F.SetupError, F.Valid())) { return false; }
	auto* DeadMonster = F.Monster(Guaranteed(Item()));
	if (!TestNotNull(TEXT("Real monster"), DeadMonster)) { return false; }
	if (!F.KillAndCheck(*this, *DeadMonster)) { return false; }
	TestTrue(TEXT("Actual damage causes death"), DeadMonster->IsDead());
	TestEqual(TEXT("Zero-XP monster still drops configured item"), DeadMonster->GetMonsterLootComponent()->GetCommittedDropCount(), 1);
	TestEqual(TEXT("One world pickup exists"), F.Pickups().Num(), 1);
	F.Kill(*DeadMonster); DeadMonster->GetMonsterLootComponent()->SpawnDeathLoot(DeadMonster->GetMonsterData()->LootTable.Get());
	TestEqual(TEXT("Repeated lethal damage and direct callback do not duplicate"), F.Pickups().Num(), 1);
	TestTrue(TEXT("Receipt committed before callbacks"), DeadMonster->GetMonsterLootComponent()->HasProcessedDeathLoot());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootPickupTest, "Rogue10m.MonsterLoot.PickupFailureRetryAndPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootPickupTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(FString(TEXT("Isolated real game inventory fixture: ")) + F.SetupError, F.Valid())) { return false; }
	auto* Potion = LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.DA_Item_HealthPotion"));
	if (!TestNotNull(TEXT("Existing persistent potion definition"), Potion)) { return false; }
	auto* DeadMonster = F.Monster(Guaranteed(Potion)); if (!TestNotNull(TEXT("Monster"), DeadMonster)) { return false; }
	if (!F.KillAndCheck(*this, *DeadMonster)) { return false; }
	auto Drops = F.Pickups(); if (!TestEqual(TEXT("Death created pickup"), Drops.Num(), 1)) { return false; }
	auto* Pickup = Drops[0]; const FRogue10mItemStack Stack = Pickup->GetItemStack();
	TestFalse(TEXT("Remote pickup rejected"), Pickup->TryPickup(F.Character));
	auto* Filler = Item(1); Filler->InventorySize = F.Inventory().GetInventoryContainers()[0].GridSize;
	int32 Container; FGuid FillerId;
	if (!TestTrue(TEXT("Fill actual grid"), F.Inventory().TryAddGridItem(Filler, 1, Container, FillerId))) { return false; }
	F.Character->SetActorLocation(FVector(0.0f, 0.0f, 120.0f));
	const auto Before = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Full grid rejects whole pickup"), Pickup->TryPickup(F.Character));
	TestTrue(TEXT("Full failure preserves inventory"), Same(Before, F.Inventory().CaptureInventory()));
	TestTrue(TEXT("Full failure preserves world item identity"), !Pickup->IsActorBeingDestroyed() && Pickup->GetItemStack().InstanceId == Stack.InstanceId);
	FRogue10mInventoryGridEntry Removed;
	TestTrue(TEXT("Free a grid position"), F.Inventory().RemoveGridItem(Container, FillerId, Removed));
	TestTrue(TEXT("Retry succeeds once capacity exists"), Pickup->TryPickup(F.Character));
	TestFalse(TEXT("Consumed pickup cannot grant twice"), Pickup->TryPickup(F.Character));
	const auto Snapshot = F.Inventory().CaptureInventory();
	TestTrue(TEXT("Existing snapshot restores collected item"), F.Inventory().RestoreInventory(Snapshot));
	TestTrue(TEXT("Snapshot preserves asset quantity and GUID"), Same(Snapshot, F.Inventory().CaptureInventory()));
	auto* Duplicate = F.World->SpawnActor<ARogue10mDroppedItem>(); Duplicate->InitializeDroppedItem(Stack);
	TestFalse(TEXT("Duplicate GUID cannot be collected again"), Duplicate->TryPickup(F.Character));
	TestFalse(TEXT("Rejected duplicate remains a world actor"), Duplicate->IsActorBeingDestroyed());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootIsolationTest, "Rogue10m.MonsterLoot.TestAndAuthorityIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootIsolationTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(FString(TEXT("Isolated initialized game world: ")) + F.SetupError, F.Valid())) { return false; }
	auto* Table = Guaranteed(Item()); auto* Training = F.Monster(Table);
	if (!TestNotNull(TEXT("Training monster"), Training)) { return false; }
	Training->MarkCombatTestInstance();
	if (!F.KillAndCheck(*this, *Training)) { return false; }
	TestTrue(TEXT("Combat-test fixture actually died"), Training->IsDead());
	TestEqual(TEXT("Combat-test death never grants loot"), F.Pickups().Num(), 0);
	auto* Client = F.Monster(Table); if (!TestNotNull(TEXT("Authority guard fixture"), Client)) { return false; }
	Client->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("Client damage is rejected before application"), F.Kill(*Client), 0.0f);
	TestFalse(TEXT("Client damage does not author a death"), Client->IsDead());
	TestEqual(TEXT("Client does not grant loot"), F.Pickups().Num(), 0);
	Client->SetRole(ROLE_Authority);
	auto* NoTable = F.Monster(nullptr); if (!TestNotNull(TEXT("Unassigned-table monster"), NoTable)) { return false; }
	if (!F.KillAndCheck(*this, *NoTable)) { return false; }
	TestTrue(TEXT("Unassigned-table fixture actually died"), NoTable->IsDead());
	TestEqual(TEXT("Unassigned existing monsters keep no-drop behavior"), F.Pickups().Num(), 0);
	auto* Ended = F.Monster(Table); auto* Run = F.World->SpawnActor<ARogue10mGameState>();
	if (!TestNotNull(TEXT("Terminal-run monster"), Ended) || !TestNotNull(TEXT("Run state"), Run)) { return false; }
	F.World->SetGameState(Run); Run->StartRun(); Run->FinishRun(false);
	TestTrue(TEXT("Terminal-run fixture entered defeat"), Run->IsRunTerminal());
	TestEqual(TEXT("Terminal run rejects actual damage"), F.Kill(*Ended), 0.0f);
	TestFalse(TEXT("Terminal run does not author a new death"), Ended->IsDead());
	TestEqual(TEXT("Terminal run does not grant loot"), F.Pickups().Num(), 0);
	return !HasAnyErrors();
}

namespace Rogue10mMonsterLootTests
{
enum class ERetryScenario { CapacityTransaction, BagExpansion, Leave, Destroy };

// TimerManager ticks once per GFrameCounter. Keep the real fixture alive and
// advance each scenario across actual automation frames rather than fake ticks.
class FOverlapRetryCommand final : public IAutomationLatentCommand
{
public:
	FOverlapRetryCommand(FAutomationTestBase& InTest, ERetryScenario InScenario)
		: Test(InTest), Scenario(InScenario) {}
	virtual bool Update() override
	{
		if (LastFrame == GFrameCounter) { return false; }
		LastFrame = GFrameCounter;
		if (Step == 0) { return Setup(); }
		F->World->Tick(LEVELTICK_All, 0.02f);
		if (Scenario == ERetryScenario::CapacityTransaction) { return CheckCapacity(); }
		if (Scenario == ERetryScenario::BagExpansion)
		{
			Test.TestEqual(TEXT("Stationary pickup uses added bag"), F->CountOwned(Stack.InstanceId), 1);
			Test.TestFalse(TEXT("Bag expansion consumes actor"), Pickup.IsValid());
			return true;
		}
		Test.TestEqual(TEXT("Canceled retry grants nothing after leaving/destruction"), F->CountOwned(Stack.InstanceId), 0);
		if (Scenario == ERetryScenario::Leave)
		{
			if (!Test.TestTrue(TEXT("Leaving preserves world item"), Pickup.IsValid())) { return true; }
			F->EnterPickup(*Pickup.Get());
			Test.TestEqual(TEXT("Later real re-entry grants exact GUID once"), F->CountOwned(Stack.InstanceId), 1);
		}
		return true;
	}
private:
	bool IsWatching() const
	{ return F->Inventory().OnInventoryGridChanged.Contains(Pickup.Get(), FName(TEXT("HandlePickupInventoryChanged"))); }
	bool Setup()
	{
		F = MakeUnique<FFixture>();
		if (!Test.TestTrue(TEXT("Begun isolated overlap world"), F->BeginOverlapMode())) { return true; }
		auto* Filler = Item(1);
		if (!Test.TestTrue(TEXT("Fill one-cell grid"), F->Inventory().TryAddGridItem(Filler, 1, Container, FillerId))) { return true; }
		Pickup = F->OverlapDrop(Test); if (!Test.TestTrue(TEXT("Death-drop fixture pickup"), Pickup.IsValid())) { return true; }
		Stack = Pickup->GetItemStack();
		if (!Test.TestTrue(TEXT("Real capsule entered death-drop overlap"), F->EnterPickup(*Pickup.Get()))) { return true; }
		Test.TestTrue(TEXT("Full inventory preserves world GUID and retry subscription"), !Pickup->IsActorBeingDestroyed()
			&& Pickup->GetItemStack().InstanceId == Stack.InstanceId && IsWatching());
		if (Test.HasAnyErrors()) { return true; }
		if (Scenario == ERetryScenario::BagExpansion)
		{
			auto* Bag = Item(1); Bag->bProvidesInventory = true; Bag->ProvidedInventorySize = FIntPoint(1, 1);
			if (!Test.TestTrue(TEXT("Expand using public bag API"), F->Inventory().AddInventoryFromBag(Bag, TEXT("OverlapRetryBag")))) { return true; }
			Test.TestEqual(TEXT("Bag change queues rather than immediately grants"), F->CountOwned(Stack.InstanceId), 0);
		}
		else
		{
			FRogue10mInventoryGridEntry Removed;
			if (!Test.TestTrue(TEXT("Remove filler and queue retry"), F->Inventory().RemoveGridItem(Container, FillerId, Removed))) { return true; }
			Test.TestEqual(TEXT("Callback does not grant inside removal transaction"), F->CountOwned(Stack.InstanceId), 0);
			if (Scenario == ERetryScenario::CapacityTransaction)
			{
				// Finish the larger operation before the next frame. A synchronous
				// pickup would steal this temporary cell before rollback could finish.
				if (!Test.TestTrue(TEXT("Complete transaction by reoccupying freed cell"), F->Inventory().TryAddGridItem(Filler, 1, Container, FillerId))) { return true; }
			}
			else
			{
				ARogue10mDroppedItem* PendingPickup = Pickup.Get();
				if (Scenario == ERetryScenario::Destroy) { Test.TestTrue(TEXT("Destroy pending pickup"), PendingPickup->Destroy()); }
				else
				{
					F->Character->SetActorLocation(FVector(500.0f, 0.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
					F->Character->GetCapsuleComponent()->UpdateOverlaps();
				}
				Test.TestFalse(TEXT("Leaving/EndPlay removes inventory subscription"), F->Inventory().OnInventoryGridChanged.Contains(PendingPickup, FName(TEXT("HandlePickupInventoryChanged"))));
				F->Inventory().OnInventoryGridChanged.Broadcast();
			}
		}
		Step = 1;
		return Test.HasAnyErrors();
	}
	bool CheckCapacity()
	{
		if (Step == 1)
		{
			Test.TestEqual(TEXT("Retry sees final full transaction and grants nothing"), F->CountOwned(Stack.InstanceId), 0);
			if (!Test.TestTrue(TEXT("Still-full retry keeps actor"), Pickup.IsValid())) { return true; }
			Stationary = F->Character->GetActorLocation();
			FRogue10mInventoryGridEntry Removed;
			if (!Test.TestTrue(TEXT("Free final capacity"), F->Inventory().RemoveGridItem(Container, FillerId, Removed))) { return true; }
			Step = 2;
			return Test.HasAnyErrors();
		}
		if (Step == 2)
		{
			// Only inventory mutation and a later frame triggered this acquisition.
			// There has been no direct TryPickup and no step out/re-entry.
			int32 Quantity = 0;
			Test.TestEqual(TEXT("Stationary retry grants exact GUID once"), F->CountOwned(Stack.InstanceId, &Quantity), 1);
			Test.TestEqual(TEXT("Stationary retry preserves quantity"), Quantity, Stack.Quantity);
			Test.TestFalse(TEXT("Success consumes world actor"), Pickup.IsValid());
			Test.TestTrue(TEXT("Player remained stationary"), F->Character->GetActorLocation().Equals(Stationary));
			F->Inventory().OnInventoryGridChanged.Broadcast();
			Step = 3;
			return Test.HasAnyErrors();
		}
		Test.TestEqual(TEXT("Later notifications cannot duplicate consumed GUID"), F->CountOwned(Stack.InstanceId), 1);
		return true;
	}
	FAutomationTestBase& Test;
	ERetryScenario Scenario;
	TUniquePtr<FFixture> F;
	TWeakObjectPtr<ARogue10mDroppedItem> Pickup;
	FRogue10mItemStack Stack;
	FGuid FillerId;
	FVector Stationary = FVector::ZeroVector;
	int32 Container = INDEX_NONE;
	int32 Step = 0;
	uint64 LastFrame = MAX_uint64;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootStationaryRetryTest, "Rogue10m.MonsterLoot.OverlapRetry.StationaryCapacityAndTransaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootStationaryRetryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOverlapRetryCommand>(*this, ERetryScenario::CapacityTransaction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootBagExpansionRetryTest, "Rogue10m.MonsterLoot.OverlapRetry.BagExpansion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootBagExpansionRetryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOverlapRetryCommand>(*this, ERetryScenario::BagExpansion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLootRetryCleanupTest, "Rogue10m.MonsterLoot.OverlapRetry.LeaveAndDestroyCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLootRetryCleanupTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOverlapRetryCommand>(*this, ERetryScenario::Leave));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOverlapRetryCommand>(*this, ERetryScenario::Destroy));
	return true;
}

namespace Rogue10mMonsterLootTests
{
static ARogue10mPlayerController* LootController(FFixture& F)
{
	// The native controller is abstract. Use the same concrete controller as the
	// production GameMode so the real Die() reward-controller cast is exercised.
	auto* ControllerClass = LoadClass<ARogue10mPlayerController>(nullptr,
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	auto* PC = ControllerClass && !ControllerClass->HasAnyClassFlags(CLASS_Abstract)
		? F.World->SpawnActor<ARogue10mPlayerController>(ControllerClass) : nullptr;
	if (PC) { PC->SetPlayerState(F.State); PC->Possess(F.Character); }
	return PC && PC->GetPawn() == F.Character && F.Character->GetController() == PC
		&& PC->GetPlayerState<ARogue10mPlayerState>() == F.State
		&& F.Character->GetPlayerState<ARogue10mPlayerState>() == F.State ? PC : nullptr;
}
static bool OneCell(FFixture& F)
{
	auto Snapshot = F.Inventory().CaptureInventory();
	if (Snapshot.Containers.IsEmpty()) { return false; }
	Snapshot.Containers.SetNum(1); Snapshot.Containers[0].Entries.Reset();
	Snapshot.Containers[0].GridSize = FIntPoint(1, 1); Snapshot.Containers[0].BagAsset.Reset();
	return F.Inventory().RestoreInventory(Snapshot);
}
static FRogue10mItemStack Stack(URogue10mItemDataAsset* Data, int32 Quantity)
{
	FRogue10mItemStack Result;
	Result.InstanceId = FGuid::NewGuid(); Result.ItemData = Data; Result.Quantity = Quantity; Result.bOccupied = true;
	return Result;
}
static URogue10mLootTableDataAsset* QuantityTable(URogue10mItemDataAsset* Data, int32 Quantity)
{
	auto* Table = Guaranteed(Data);
	Table->Entries[0].MinQuantity = Table->Entries[0].MaxQuantity = Quantity;
	return Table;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAutoLootDirectTest, "Rogue10m.AutoLoot.DirectDeathAndPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAutoLootDirectTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(TEXT("Isolated fixture"), F.Valid())) { return false; }
	auto* PC = LootController(F); if (!TestNotNull(TEXT("Actual reward controller"), PC)) { return false; }
	auto* Potion = LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.DA_Item_HealthPotion"));
	if (!TestNotNull(TEXT("Persistent potion"), Potion)) { return false; }
	auto* Target = F.Monster(QuantityTable(Potion, 1)); if (!TestNotNull(TEXT("Zero-XP monster"), Target)) { return false; }
	F.Floor->FindComponentByClass<UBoxComponent>()->SetCollisionEnabled(ECollisionEnabled::NoCollision); F.Floor->Destroy(); // Auto-deposit must not depend on supported drop ground.
	TestTrue(TEXT("Actual killer damage applied"), Target->TakeDamage(1000.0f, FDamageEvent(), PC, F.Character) > 0.0f);
	TestEqual(TEXT("One item automatically received"), Target->GetMonsterLootComponent()->GetAutomaticallyReceivedQuantity(), static_cast<int64>(1));
	TestEqual(TEXT("No unnecessary world actor"), F.Pickups().Num(), 0);
	const auto& Entries = F.Inventory().GetInventoryContainers()[0].Entries;
	if (!TestEqual(TEXT("One new inventory entry"), Entries.Num(), 1)) { return false; }
	const FGuid Id = Entries[0].InstanceId;
	const auto Saved = F.Inventory().CaptureInventory();
	TestTrue(TEXT("Existing snapshot restores automatically received item"), F.Inventory().RestoreInventory(Saved));
	TestTrue(TEXT("Snapshot preserves GUID/quantity/asset"), Same(Saved, F.Inventory().CaptureInventory()));
	F.Kill(*Target); Target->GetMonsterLootComponent()->SpawnDeathLoot(Target->GetMonsterData()->LootTable.Get(), F.Character);
	TestEqual(TEXT("Repeated death cannot redeposit"), F.CountOwned(Id), 1);
	TestEqual(TEXT("One-shot amount remains one"), Target->GetMonsterLootComponent()->GetAutomaticallyReceivedQuantity(), static_cast<int64>(1));
	auto* Delivery = F.World->GetSubsystem<URogue10mLootDeliverySubsystem>();
	if (TestNotNull(TEXT("Delivery subsystem"), Delivery)) { TestEqual(TEXT("No pending item after complete deposit"), Delivery->GetPendingStackCount(), 0); }
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAutoLootPartialTest, "Rogue10m.AutoLoot.PartialStackDropsOnlyRemainder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAutoLootPartialTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(TEXT("One-cell fixture"), F.Valid() && OneCell(F))) { return false; }
	auto* PC = LootController(F); if (!TestNotNull(TEXT("Killer controller"), PC)) { return false; }
	auto* Data = Item(20); int32 Container; FGuid ExistingId;
	if (!TestTrue(TEXT("Existing18/20 stack fills grid"), F.Inventory().TryAddGridItem(Data, 18, Container, ExistingId))) { return false; }
	auto* Target = F.Monster(QuantityTable(Data, 5)); if (!TestNotNull(TEXT("Monster drops five"), Target)) { return false; }
	TestTrue(TEXT("Real death"), Target->TakeDamage(1000.0f, FDamageEvent(), PC, F.Character) > 0.0f);
	const auto& Existing = F.Inventory().GetInventoryContainers()[Container].Entries[0];
	TestEqual(TEXT("Existing GUID preserved"), Existing.InstanceId, ExistingId);
	TestEqual(TEXT("Only two merge into inventory"), Existing.Quantity, 20);
	TestEqual(TEXT("Auto counter counts units not stacks"), Target->GetMonsterLootComponent()->GetAutomaticallyReceivedQuantity(), static_cast<int64>(2));
	const auto Drops = F.Pickups(); if (!TestEqual(TEXT("Exactly one overflow actor"), Drops.Num(), 1)) { return false; }
	TestEqual(TEXT("Only remainder three drops"), Drops[0]->GetItemStack().Quantity, 3);
	TestTrue(TEXT("Remainder has its own original rolled identity"), Drops[0]->GetItemStack().InstanceId.IsValid() && Drops[0]->GetItemStack().InstanceId != ExistingId);
	TestEqual(TEXT("Conservation18+5=20+3"), Existing.Quantity + Drops[0]->GetItemStack().Quantity, 23);
	F.Kill(*Target); Target->GetMonsterLootComponent()->SpawnDeathLoot(Target->GetMonsterData()->LootTable.Get(), F.Character);
	TestEqual(TEXT("Repeated death cannot duplicate overflow"), F.Pickups().Num(), 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAutoLootAdapterTest, "Rogue10m.AutoLoot.GridMergesMetadataAndInvalidInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAutoLootAdapterTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(TEXT("Grid fixture"), F.Valid())) { return false; }
	auto* Data = Item(20); int32 Container; FGuid OldId;
	if (!TestTrue(TEXT("Existing18 stack"), F.Inventory().TryAddGridItem(Data, 18, Container, OldId))) { return false; }
	const auto Incoming = Stack(Data, 5); FRogue10mItemStack Remainder; int32 Added = -1;
	TestTrue(TEXT("Process valid incoming"), F.Inventory().TryReceiveMonsterLoot(Incoming, Remainder, Added));
	TestEqual(TEXT("Merge2 plus new3 received"), Added, 5); TestEqual(TEXT("Nothing to drop"), Remainder.Quantity, 0);
	TestEqual(TEXT("Original incoming GUID owns new3 entry"), F.CountOwned(Incoming.InstanceId, &Added), 1);
	TestEqual(TEXT("New entry contains remaining three"), Added, 3);
	auto Enhanced = Stack(Data, 4); Enhanced.EnhancementLevel = 1; Enhanced.EnhancementAttackBonus = 5.0f;
	TestTrue(TEXT("Enhanced loot processed"), F.Inventory().TryReceiveMonsterLoot(Enhanced, Remainder, Added));
	TestEqual(TEXT("Enhancement mismatch makes separate entry"), F.CountOwned(Enhanced.InstanceId), 1);
	for (const auto& Grid : F.Inventory().GetInventoryContainers())
	{
		for (const auto& Entry : Grid.Entries)
		{
			if (Entry.InstanceId == Enhanced.InstanceId)
			{
				TestEqual(TEXT("Enhancement level preserved"), Entry.EnhancementLevel, 1);
				TestEqual(TEXT("Enhancement attack bonus preserved"), Entry.EnhancementAttackBonus, 5.0f);
			}
		}
	}
	const auto Before = F.Inventory().CaptureInventory();
	TestFalse(TEXT("Owned GUID replay rejected"), F.Inventory().TryReceiveMonsterLoot(Enhanced, Remainder, Added));
	TestEqual(TEXT("Rejected replay adds zero"), Added, 0);
	TestTrue(TEXT("Replay does not mutate inventory"), Same(Before, F.Inventory().CaptureInventory()));
	auto Invalid = Stack(Data, 2); Invalid.EnhancementAttackBonus = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Invalid enhancement rejected before merging"), F.Inventory().TryReceiveMonsterLoot(Invalid, Remainder, Added));
	TestTrue(TEXT("Invalid input preserves inventory"), Same(Before, F.Inventory().CaptureInventory()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAutoLootCustodyTest, "Rogue10m.AutoLoot.PendingOverflowOutlivesCorpse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAutoLootCustodyTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(TEXT("Full one-cell fixture"), F.Valid() && OneCell(F))) { return false; }
	auto* PC = LootController(F); if (!TestNotNull(TEXT("Killer controller"), PC)) { return false; }
	int32 Container; FGuid FillerId;
	if (!TestTrue(TEXT("Fill grid with unrelated item"), F.Inventory().TryAddGridItem(Item(1), 1, Container, FillerId))) { return false; }
	F.Floor->FindComponentByClass<UBoxComponent>()->SetCollisionEnabled(ECollisionEnabled::NoCollision); F.Floor->Destroy();
	auto* Target = F.Monster(QuantityTable(Item(20), 5)); if (!TestNotNull(TEXT("Overflow monster"), Target)) { return false; }
	TestTrue(TEXT("Death without supporting ground"), Target->TakeDamage(1000.0f, FDamageEvent(), PC, F.Character) > 0.0f);
	auto* Delivery = F.World->GetSubsystem<URogue10mLootDeliverySubsystem>();
	if (!TestNotNull(TEXT("World delivery custody"), Delivery) || !TestEqual(TEXT("Missing ground retains one stack"), Delivery->GetPendingStackCount(), 1)) { return false; }
	const auto Pending = Delivery->GetPendingLoot()[0];
	TestEqual(TEXT("Retained complete five"), Pending.Stack.Quantity, 5);
	TestTrue(TEXT("Corpse can be destroyed"), Target->Destroy());
	TestEqual(TEXT("Custody survives corpse destruction"), Delivery->GetPendingStackCount(), 1);
	auto* Floor = F.World->SpawnActor<AActor>(); if (!TestNotNull(TEXT("Restored floor"), Floor)) { return false; }
	auto* Box = NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
	Box->SetBoxExtent(FVector(1000.0f,1000.0f,20.0f)); Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Floor->SetActorLocation(FVector(0.0f,0.0f,-20.0f));
	Delivery->RetryPendingLoot();
	TestEqual(TEXT("Custody transfers to actual actor"), Delivery->GetPendingStackCount(), 0);
	const auto Drops = F.Pickups(); if (!TestEqual(TEXT("One recovered overflow"), Drops.Num(), 1)) { return false; }
	TestEqual(TEXT("Recovery keeps exact GUID"), Drops[0]->GetItemStack().InstanceId, Pending.Stack.InstanceId);
	TestEqual(TEXT("Recovery keeps exact quantity"), Drops[0]->GetItemStack().Quantity, 5);
	Delivery->RetryPendingLoot(); TestEqual(TEXT("Repeated retries cannot duplicate"), F.Pickups().Num(), 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mAutoLootRejectedTest, "Rogue10m.AutoLoot.RejectedReceiptStaysQuarantined", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mAutoLootRejectedTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mMonsterLootTests;
	FFixture F; if (!TestTrue(TEXT("Isolated replay fixture"), F.Valid())) { return false; }
	int32 Container; FGuid OwnedId; auto* Data = Item(20);
	if (!TestTrue(TEXT("Owned original item"), F.Inventory().TryAddGridItem(Data, 1, Container, OwnedId))) { return false; }
	auto Replayed = Stack(Data, 1); Replayed.InstanceId = OwnedId;
	auto* Target = F.Monster(nullptr); if (!TestNotNull(TEXT("Dead source"), Target)) { return false; }
	if (!F.KillAndCheck(*this, *Target)) { return false; }
	auto* Delivery = F.World->GetSubsystem<URogue10mLootDeliverySubsystem>();
	if (!TestNotNull(TEXT("Delivery subsystem"), Delivery)) { return false; }
	TArray<FRogue10mItemStack> Input; Input.Add(Replayed);
	Delivery->QueueDeathLoot(Input, *Target, F.Character, nullptr, 0.0f, 3000.0f, 35.0f);
	if (!TestEqual(TEXT("Rejected receipt retained"), Delivery->GetPendingStackCount(), 1)) { return false; }
	TestTrue(TEXT("Rejected receipt is explicitly quarantined"), Delivery->GetPendingLoot()[0].bRejectedInput);
	F.Character->Destroy(); Delivery->RetryPendingLoot();
	TestEqual(TEXT("Recipient destruction cannot convert replay to world loot"), F.Pickups().Num(), 0);
	TestEqual(TEXT("Receipt remains in custody"), Delivery->GetPendingStackCount(), 1);
	return !HasAnyErrors();
}
#endif
