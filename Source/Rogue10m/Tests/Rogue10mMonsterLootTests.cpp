#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
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
#include "UObject/StrongObjectPtr.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mLootTableDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterLootComponent.h"
#include "Rogue10mPlayerState.h"
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
			// World::EndPlay skips a world that never fully began. Route only actors
			// explicitly begun by this fixture while their GI/subsystems are still valid.
			for (TActorIterator<AActor> It(World); It; ++It)
			{ if (It->HasActorBegunPlay()) { It->RouteEndPlay(EEndPlayReason::Quit); } }
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
#endif
