#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mSkillLoadoutDataAsset.h"

struct FRogue10mWeaponSwitchTestAccess
{
	static void Profiles(URogue10mCombatComponent& Combat, URogue10mWeaponSkillProfileDataAsset* A, URogue10mWeaponSkillProfileDataAsset* B)
	{
		Combat.CharacterData = nullptr; Combat.WeaponSkillProfiles = {A, B};
		auto* Unarmed = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		Unarmed->WeaponType = ERogue10mWeaponType::Unarmed;
		Combat.WeaponSkillProfiles.Add(Unarmed);
	}
	static bool Attack(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{ return Combat.ExecuteAttackSkill(Skill, false); }
	static float Deadline(const URogue10mCombatComponent& Combat) { return Combat.AttackCooldownEndTime; }
	static void Charge(URogue10mCombatComponent& Combat, bool bHeld) { Combat.LeftAttackPressedTime = bHeld ? 0.0f : -1.0f; }
	static void Dodge(ARogue10mCharacter& Character, bool bActive) { Character.bIsDodging = bActive; }
	static void Sprint(ARogue10mCharacter& Character) { Character.SetSprinting(true); }
	static void Prepare(URogue10mInventoryComponent& Inventory, URogue10mItemDataAsset* Old, URogue10mItemDataAsset* New)
	{
		Inventory.LeftEquipmentSlots.Reset(); Inventory.RightEquipmentSlots.Reset();
		auto& Main = Inventory.LeftEquipmentSlots.AddDefaulted_GetRef(); Main.SlotType = ERogue10mInventorySlotType::MainWeapon;
		Main.bHasEquippedItem = Main.bEquipped = true; Main.EquippedItem = Inventory.MakeItemFromDataAsset(Old, 1);
		Inventory.ItemSlots = {Inventory.MakeItemFromDataAsset(New, 1), Inventory.MakeEmptyItem(), Inventory.MakeEmptyItem()};
		Inventory.InventoryContainers.Reset(); auto& Grid = Inventory.InventoryContainers.AddDefaulted_GetRef();
		Grid.ContainerId = TEXT("SwitchQA"); Grid.GridSize = FIntPoint(2, 2);
		auto& Entry = Grid.Entries.AddDefaulted_GetRef(); Entry.InstanceId = FGuid::NewGuid(); Entry.ItemData = New; Entry.Quantity = 1;
	}
	static void DuplicateEquipped(URogue10mInventoryComponent& Inventory)
	{
		const auto Item = Inventory.LeftEquipmentSlots[0].EquippedItem;
		Inventory.ItemSlots[0] = Item;
		auto& Entry = Inventory.InventoryContainers[0].Entries.AddDefaulted_GetRef();
		Entry.InstanceId = Item.InstanceId; Entry.ItemData = Item.ItemData; Entry.Quantity = 1; Entry.Position = FIntPoint(1,1);
	}
};

namespace Rogue10mWeaponSwitchTests
{
using Slot = ERogue10mInventorySlotType;
struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mCharacter* Character = nullptr;
	ARogue10mPlayerState* State = nullptr;
	URogue10mAttackSkillData* Skill = nullptr;
	URogue10mItemDataAsset* OldData = nullptr;
	FGuid NewId;
	FFixture(bool bSameType = false)
	{
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		State = World->SpawnActor<ARogue10mPlayerState>();
		auto* Class = LoadClass<ARogue10mCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		Character = Class ? World->SpawnActor<ARogue10mCharacter>(Class) : nullptr;
		if (!State || !Character) { return; }
		Character->SetPlayerState(State); Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		auto* ASC = State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet()); ASC->InitAbilityActorInfo(State, Character);
		Skill = NewObject<URogue10mAttackSkillData>(); Skill->AttackCooldown = 0.0f; Skill->HitStartDelaySeconds = 10.0f;
		Skill->ResourceCosts = {{ERogue10mAttackResourceType::Stamina, 10.0f}};
		auto* A = NewObject<URogue10mWeaponSkillProfileDataAsset>(); auto* B = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		A->WeaponType = ERogue10mWeaponType::Knuckle; B->WeaponType = ERogue10mWeaponType::GreatSword;
		A->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, Skill); B->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, Skill);
		FRogue10mWeaponSwitchTestAccess::Profiles(Combat(), A, B);
		Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
		OldData = NewObject<URogue10mItemDataAsset>(); auto* NewData = NewObject<URogue10mItemDataAsset>();
		for (auto* Data : {OldData, NewData}) { Data->Category = ERogue10mItemCategory::Equipment; Data->EquipSlotType = Slot::MainWeapon; }
		OldData->WeaponType = ERogue10mWeaponType::Knuckle; OldData->EquipmentStats.AttackPowerBonus = 5.0f;
		NewData->WeaponType = bSameType ? ERogue10mWeaponType::Knuckle : ERogue10mWeaponType::GreatSword;
		NewData->EquipmentStats.AttackPowerBonus = 20.0f;
		FRogue10mWeaponSwitchTestAccess::Prepare(Inventory(), OldData, NewData);
		NewId = Inventory().GetInventoryContainers()[0].Entries[0].InstanceId;
		Character->RefreshCharacterStats();
	}
	~FFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } }
	bool IsValid() const { return Character && State && Skill && OldData; }
	URogue10mInventoryComponent& Inventory() { return *Character->GetInventoryComponent(); }
	URogue10mCombatComponent& Combat() { return *Character->GetCombatComponent(); }
	bool Attack() { return FRogue10mWeaponSwitchTestAccess::Attack(Combat(), *Skill); }
	float Deadline() { return FRogue10mWeaponSwitchTestAccess::Deadline(Combat()); }
	void Advance(float Seconds)
	{
		while (Seconds > UE_SMALL_NUMBER) { const float Step = FMath::Min(0.01f, Seconds); World->Tick(LEVELTICK_All, Step); Seconds -= Step; }
	}
	FGuid EquippedId() { return Inventory().GetLeftEquipmentSlots()[0].EquippedItem.InstanceId; }
	bool Command(int32 Index)
	{
		FRogue10mItemStack Removed;
		switch (Index)
		{
		case 0: return Inventory().TryEquipItemToSlot(0, Slot::MainWeapon);
		case 1: return Inventory().TryUnequipItemFromSlot(Slot::MainWeapon);
		case 2: return Inventory().TryUnequipItemFromSlotToItemSlot(Slot::MainWeapon, 1);
		case 3: return Inventory().RemoveEquippedItemFromSlot(Slot::MainWeapon, Removed);
		case 4: return Inventory().TryEquipGridItem(0, NewId);
		case 5: return Inventory().TryUnequipItemToGrid(Slot::MainWeapon, 0, FIntPoint(1,0));
		default: return false;
		}
	}
};

struct FBefore
{
	FRogue10mInventorySnapshot Inventory;
	FRogue10mCombatSnapshot Combat;
	FRogue10mCharacterStatSnapshot Stats;
	TArray<float> AttributeValues;
	ERogue10mWeaponType Type;
	float Deadline;
	bool bCanDamage;
	static TArray<float> Values(FFixture& F)
	{
		const auto* A = F.State->GetRogueAttributeSet();
		return {A->GetHealth(), A->GetStamina(), A->GetMana(), A->GetIdentity(), A->GetAttackPower(), A->GetMoveSpeed()};
	}
	explicit FBefore(FFixture& F) : Inventory(F.Inventory().CaptureInventory()), Combat(F.Combat().CaptureCombat()),
		Stats(F.Character->GetCharacterStatSnapshot()), AttributeValues(Values(F)), Type(F.Character->GetEquippedWeaponType()), Deadline(F.Deadline()), bCanDamage(F.Character->CanBeDamaged()) {}
	void Unchanged(FAutomationTestBase& Test, FFixture& F) const
	{
		const auto AfterInventory = F.Inventory().CaptureInventory(); const auto AfterCombat = F.Combat().CaptureCombat();
		const auto AfterStats = F.Character->GetCharacterStatSnapshot();
		Test.TestTrue(TEXT("entire inventory unchanged"), FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&Inventory, &AfterInventory, 0));
		Test.TestTrue(TEXT("bindings and unlocks unchanged"), FRogue10mCombatSnapshot::StaticStruct()->CompareScriptStruct(&Combat, &AfterCombat, 0));
		Test.TestTrue(TEXT("stats and resources unchanged"), FRogue10mCharacterStatSnapshot::StaticStruct()->CompareScriptStruct(&Stats, &AfterStats, 0));
		Test.TestTrue(TEXT("live vitals and attributes unchanged"), AttributeValues == Values(F));
		Test.TestEqual(TEXT("weapon type unchanged"), F.Character->GetEquippedWeaponType(), Type);
		Test.TestEqual(TEXT("deadline unchanged"), F.Deadline(), Deadline);
		Test.TestEqual(TEXT("damage permission unchanged"), F.Character->CanBeDamaged(), bCanDamage);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchLocomotionTest, "Rogue10m.WeaponSwitch.LocomotionAndNoInvulnerability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchLocomotionTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
		F.Character->GetCharacterMovement()->Velocity = FVector(Mode == 0 ? 0 : Mode == 1 ? 150 : 600,0,0);
		if (Mode == 2) { FRogue10mWeaponSwitchTestAccess::Sprint(*F.Character); }
		TestTrue(TEXT("idle/walk/sprint eligible"), F.Character->CanChangeWeapon());
		TestTrue(TEXT("owned grid weapon equipped"), F.Inventory().TryEquipGridItem(0, F.NewId));
		TestEqual(TEXT("exact item instance equipped"), F.EquippedId(), F.NewId);
		TestEqual(TEXT("new weapon type"), F.Character->GetEquippedWeaponType(), ERogue10mWeaponType::GreatSword);
		TestTrue(TEXT("swap grants no invulnerability"), F.Character->CanBeDamaged());
		TestTrue(TEXT("floor applied"), FMath::IsNearlyEqual(F.Combat().GetAttackCooldownRemaining(), 0.5f));
		if (Mode == 2) { TestTrue(TEXT("sprint is not an attack state"), F.Character->IsSprinting()); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchAtomicTest, "Rogue10m.WeaponSwitch.RejectBeforeMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchAtomicTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	for (bool bDead : {false, true})
	{
		FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
		if (bDead) { F.State->SetCharacterDead(true); } else { TestTrue(TEXT("actual attack starts"), F.Attack()); }
		const FBefore Before(F);
		for (int32 Path = 0; Path < 6; ++Path)
		{
			TestFalse(TEXT("all weapon mutation paths reject attack/death"), F.Command(Path)); Before.Unchanged(*this, F);
		}
		FRogue10mItemStack Removed; Removed.InstanceId = FGuid::NewGuid(); const FGuid Sentinel = Removed.InstanceId;
		TestFalse(TEXT("remove output also rejected"), F.Inventory().RemoveEquippedItemFromSlot(Slot::MainWeapon, Removed));
		TestEqual(TEXT("failure does not overwrite output"), Removed.InstanceId, Sentinel);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchAttackBoundaryTest, "Rogue10m.WeaponSwitch.AttackBoundaryAndLongCooldown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchAttackBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
	F.Skill->HitStartDelaySeconds = 0.2f; F.Skill->HitMode = ERogue10mAttackHitMode::MultiHit; F.Skill->HitCount = 3;
	F.Skill->HitInterval = 0.1f; F.Skill->AttackCooldown = 5.0f;
	TestTrue(TEXT("real timed attack accepted"), F.Attack()); const float End = F.Deadline();
	F.Advance(0.39f); TestFalse(TEXT("last pulse still pending"), F.Inventory().TryEquipGridItem(0, F.NewId));
	F.Advance(0.03f); TestTrue(TEXT("action ends before reuse cooldown"), F.Character->CanChangeWeapon());
	TestTrue(TEXT("swap allowed during remaining reuse cooldown"), F.Inventory().TryEquipGridItem(0, F.NewId));
	TestEqual(TEXT("long cooldown absolute end preserved"), F.Deadline(), End);
	TestTrue(TEXT("remaining cooldown still long"), F.Combat().GetAttackCooldownRemaining() > 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchIdentityTest, "Rogue10m.WeaponSwitch.SameTypeAndDuplicateInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchIdentityTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	FFixture F(true); if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
	const FGuid OldId = F.EquippedId(); TestTrue(TEXT("different instance same type equips"), F.Inventory().TryEquipGridItem(0, F.NewId));
	TestNotEqual(TEXT("different physical item"), F.EquippedId(), OldId);
	TestEqual(TEXT("same type retained"), F.Character->GetEquippedWeaponType(), ERogue10mWeaponType::Knuckle);
	TestTrue(TEXT("same type actual swap gets floor"), FMath::IsNearlyEqual(F.Combat().GetAttackCooldownRemaining(), 0.5f));
	F.Advance(0.1f); const FBefore Before(F);
	for (int32 Index = 0; Index < 5; ++Index) { TestFalse(TEXT("repeat same instance request is no-op rejection"), F.Inventory().TryEquipGridItem(0, F.NewId)); }
	Before.Unchanged(*this, F);
	FRogue10mWeaponSwitchTestAccess::DuplicateEquipped(F.Inventory()); const FBefore WithDuplicate(F);
	TestFalse(TEXT("duplicate grid reference cannot equip same item twice"), F.Inventory().TryEquipGridItem(0, F.NewId));
	TestFalse(TEXT("duplicate legacy reference cannot re-equip same item"), F.Inventory().TryEquipItemToSlot(0, Slot::MainWeapon));
	WithDuplicate.Unchanged(*this, F);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchValidationTest, "Rogue10m.WeaponSwitch.ValidationFailureIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchValidationTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
	F.OldData->InventorySize = FIntPoint(3,3); const FBefore Before(F);
	TestFalse(TEXT("old weapon cannot fit in replacement grid"), F.Inventory().TryEquipGridItem(0, F.NewId));
	TestFalse(TEXT("unknown instance rejected"), F.Inventory().TryEquipGridItem(0, FGuid::NewGuid()));
	TestFalse(TEXT("invalid container rejected"), F.Inventory().TryEquipGridItem(99, F.NewId));
	Before.Unchanged(*this, F);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchReentryTest, "Rogue10m.WeaponSwitch.ReentrantCommandsRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchReentryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	{
		FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
		bool bObserved = false, bNested = true;
		auto& Delegate = F.State->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetStaminaAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&) { bObserved = true; bNested = F.Inventory().TryEquipGridItem(0, F.NewId); });
		TestTrue(TEXT("outer real attack accepted"), F.Attack()); Delegate.Remove(Handle);
		TestTrue(TEXT("real cost notification observed"), bObserved); TestFalse(TEXT("cost callback cannot swap weapon"), bNested);
		TestEqual(TEXT("original weapon retained"), F.Character->GetEquippedWeaponType(), ERogue10mWeaponType::Knuckle);
	}
	{
		FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
		bool bObserved = false, bNested = true, bAttack = true;
		auto& Delegate = F.State->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetAttackPowerAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&) { bObserved = true; bNested = F.Command(0); bAttack = F.Attack(); });
		TestTrue(TEXT("outer real equipment change accepted"), F.Inventory().TryEquipGridItem(0, F.NewId)); Delegate.Remove(Handle);
		TestTrue(TEXT("real equipment stat notification observed"), bObserved);
		TestFalse(TEXT("equipment callback cannot nest another equip"), bNested); TestFalse(TEXT("equipment callback cannot accept attack"), bAttack);
		TestEqual(TEXT("outer item survives callbacks"), F.EquippedId(), F.NewId);
		TestEqual(TEXT("no reentrant resource debit"), F.State->GetRogueAttributeSet()->GetStamina(), 100.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchBusyTest, "Rogue10m.WeaponSwitch.ConservativeBusyStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchBusyTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
	for (auto Mode : {MOVE_Falling, MOVE_Flying, MOVE_None})
	{
		F.Character->GetCharacterMovement()->SetMovementMode(Mode); const FBefore Before(F);
		TestFalse(TEXT("non-locomotion swap denied"), F.Command(4)); Before.Unchanged(*this, F);
	}
	F.Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	FRogue10mWeaponSwitchTestAccess::Dodge(*F.Character, true); TestFalse(TEXT("dodge denied"), F.Command(4));
	FRogue10mWeaponSwitchTestAccess::Dodge(*F.Character, false);
	FRogue10mWeaponSwitchTestAccess::Charge(F.Combat(), true); TestFalse(TEXT("held attack denied"), F.Command(4));
	FRogue10mWeaponSwitchTestAccess::Charge(F.Combat(), false);
	F.State->GetRogueAttributeSet()->SetHealth(0.0f); TestFalse(TEXT("zero health before death flag denied"), F.Command(4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwitchPathsTest, "Rogue10m.WeaponSwitch.AllMutationPathsSucceedAtIdle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwitchPathsTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponSwitchTests;
	for (int32 Path = 0; Path < 6; ++Path)
	{
		FFixture F; if (!TestTrue(TEXT("fixture"), F.IsValid())) { return false; }
		TestTrue(TEXT("each supported mutation succeeds at idle"), F.Command(Path));
		TestFalse(TEXT("transaction guard released"), F.Inventory().IsEquipmentChangeInProgress());
		TestTrue(TEXT("successful mutation adds no invulnerability"), F.Character->CanBeDamaged());
	}
	return true;
}
#endif
