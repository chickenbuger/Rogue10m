#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCatalogTestCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mSkillLoadoutDataAsset.h"

struct FRogue10mCatalogConsumptionTestAccess
{
	static void Configure(URogue10mCombatComponent& Combat, ARogue10mPlayerState& State,
		URogue10mWeaponSkillProfileDataAsset* A, URogue10mWeaponSkillProfileDataAsset* B)
	{
		Combat.CharacterData = nullptr;
		Combat.WeaponSkillProfiles = {A, B};
		auto* Novice = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		auto* Punch = NewObject<URogue10mAttackSkillData>(Novice);
		Punch->AllowedSlotPolicy = ERogue10mSkillSlotPolicy::BasicOnly;
		Novice->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, Punch);
		Combat.WeaponSkillProfiles.Add(Novice);
		Combat.BoundProgressionState = &State;
		for (const auto* Profile : {A, B})
		{
			for (const URogue10mAttackSkillData* Skill : Profile->SkillTreeSkills) { Combat.UnlockedAttackSkillNames.Add(Skill->GetFName()); }
		}
	}
	static void Switch(URogue10mCombatComponent& Combat, ARogue10mPlayerState& State, ERogue10mWeaponType Weapon)
	{
		// Match production ordering: state changes before outgoing choices are remembered.
		State.SetEquippedWeaponType(Weapon);
		Combat.ApplyActiveWeaponProfile();
	}
	static bool CanUse(const URogue10mCombatComponent& Combat, URogue10mAttackSkillData* Skill)
	{ return Combat.CanUseBoundSkill(Skill); }
	static void Inject(URogue10mCombatComponent& Combat, URogue10mAttackSkillData* Skill)
	{ Combat.EquippedSkillBindings.Add(ERogue10mAttackInputSlot::QSkill, Skill); }
	static void SetLearned(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData* Skill, bool bLearned)
	{ if (bLearned) { Combat.UnlockedAttackSkillNames.Add(Skill->GetFName()); } else { Combat.UnlockedAttackSkillNames.Remove(Skill->GetFName()); } }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCatalogConsumptionTest,
	"Rogue10m.SkillLoadout.CatalogConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mCatalogConsumptionTest::RunTest(const FString&)
{
	using Slot = ERogue10mAttackInputSlot;
	using Policy = ERogue10mSkillSlotPolicy;
	const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Character = World->SpawnActor<ARogue10mCatalogTestCharacter>();
	auto* State = World->SpawnActor<ARogue10mPlayerState>();
	auto* Controller = World->SpawnActor<APlayerController>();
	if (!Character || !State || !Controller) { AddError(TEXT("Native content-independent fixture creation failed")); return false; }
	// This world has no GameMode/ULocalPlayer. Match GameMode's explicit local-controller designation.
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	Character->SetPlayerState(State);
	auto* ASC = State->GetAbilitySystemComponent();
	ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet());
	ASC->InitAbilityActorInfo(State, Character);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("Fixture pawn is locally controlled"), Character->IsLocallyControlled());
	TestFalse(TEXT("Fixture starts grounded for non-jump techniques"), Character->GetCharacterMovement()->IsFalling());
	auto* Combat = Character->GetCombatComponent();
	const auto MakeProfile = [](ERogue10mWeaponType Weapon)
	{
		auto* Profile = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		Profile->WeaponType = Weapon;
		auto* Basic = NewObject<URogue10mAttackSkillData>(Profile);
		Basic->AllowedSlotPolicy = Policy::BasicOnly;
		Profile->DefaultSkillBindings.Add(Slot::Primary, Basic);
		Profile->SkillTreeSkills.Add(Basic);
		auto* Passive = NewObject<URogue10mAttackSkillData>(Profile);
		Passive->SkillTreeNodeType = ERogue10mSkillTreeNodeType::PassiveTechnique;
		Passive->PassiveDamageReduction = .1f;
		Profile->SkillTreeSkills.Add(Passive);
		for (int32 Index = 0; Index < 10; ++Index)
		{
			auto* Skill = NewObject<URogue10mAttackSkillData>(Profile);
			Skill->AllowedSlotPolicy = Index < 6 ? Policy::Common : Index < 8 ? Policy::MouseChordOnly : Policy::RightChargeOnly;
			Profile->SkillTreeSkills.Add(Skill);
			auto& Entry = Profile->ActiveTechniqueCatalog.AddDefaulted_GetRef();
			Entry.TechniqueId = FName(*FString::Printf(TEXT("Catalog.Test.%02d"), Index));
			Entry.Skill = Skill;
		}
		auto* Outside = NewObject<URogue10mAttackSkillData>(Profile);
		Outside->AllowedSlotPolicy = Policy::Common;
		Profile->SkillTreeSkills.Add(Outside);
		Profile->DefaultSkillBindings.Add(Slot::QSkill, Outside); // Default path must not bypass catalog.
		return Profile;
	};
	auto* Knuckle = MakeProfile(ERogue10mWeaponType::Knuckle);
	auto* Sword = MakeProfile(ERogue10mWeaponType::GreatSword);
	FRogue10mCatalogConsumptionTestAccess::Configure(*Combat, *State, Knuckle, Sword);
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, Knuckle->WeaponType);
	auto* Outside = Knuckle->SkillTreeSkills.Last().Get();
	const auto& Entries = Knuckle->ActiveTechniqueCatalog;
	TestEqual(TEXT("Full growth tree preserved"), Combat->GetActiveSkillTreeSkills().Num(), 13);
	TestEqual(TEXT("Selectable catalog consumed"), Combat->GetSelectableActiveTechniques().Num(), 10);
	TestTrue(TEXT("Passive remains active"), Combat->GetUnlockedPassiveDamageReduction() >= .1f);
	TestTrue(TEXT("Outside skill still learned"), Combat->IsAttackSkillUnlocked(Outside));
	TestTrue(TEXT("Outside skill remains a progression node"), Combat->IsSkillInActiveTree(Outside));
	TestFalse(TEXT("Learned outside skill is not selectable"), Combat->IsSelectableActiveTechnique(Outside));
	TestNull(TEXT("Outside default cannot equip"), Combat->GetEquippedSkill(Slot::QSkill));
	TestNotNull(TEXT("Fixed primary preserved"), Combat->GetEquippedSkill(Slot::Primary));
	TestFalse(TEXT("Outside assignment denied"), Combat->AssignSkillToInputSlot(Outside, Slot::QSkill));
	FRogue10mCatalogConsumptionTestAccess::SetLearned(*Combat, Entries[4].Skill, false);
	TestTrue(TEXT("Locked candidate stays visible"), Combat->GetSelectableActiveTechniques().Contains(Entries[4].Skill));
	TestFalse(TEXT("Catalog membership does not grant unlock"), Combat->CanEquipSkillInSlot(Entries[4].Skill, Slot::QSkill));
	FRogue10mCatalogConsumptionTestAccess::SetLearned(*Combat, Entries[4].Skill, true);
	const Slot Slots[] = {Slot::QSkill, Slot::ESkill, Slot::RSkill, Slot::Special, Slot::MouseChord, Slot::ChargedSpecial};
	const int32 Indices[] = {0, 1, 2, 3, 6, 8};
	for (int32 Index = 0; Index < 6; ++Index)
	{ TestTrue(TEXT("Six legitimate choices equip"), Combat->AssignSkillToInputSlot(Entries[Indices[Index]].Skill, Slots[Index])); }
	TestEqual(TEXT("Six slots remain the maximum"), Combat->GetEquippedLoadoutSkillCount(), 6);
	TestFalse(TEXT("Chord cannot enter a common slot"), Combat->CanEquipSkillInSlot(Entries[6].Skill, Slot::QSkill));
	TestFalse(TEXT("Charge cannot enter a common slot"), Combat->CanEquipSkillInSlot(Entries[8].Skill, Slot::Special));
	TestTrue(TEXT("Legitimate bound execution has a valid fixture"), FRogue10mCatalogConsumptionTestAccess::CanUse(*Combat, Entries[0].Skill));
	FRogue10mCatalogConsumptionTestAccess::Inject(*Combat, Outside);
	TestNull(TEXT("Stale outside binding hidden"), Combat->GetEquippedSkill(Slot::QSkill));
	TestFalse(TEXT("Stale outside bound execution denied"), FRogue10mCatalogConsumptionTestAccess::CanUse(*Combat, Outside));
	TestTrue(TEXT("Restore legitimate choice"), Combat->AssignSkillToInputSlot(Entries[0].Skill, Slot::QSkill));
	const auto Saved = Combat->CaptureCombat();
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, Sword->WeaponType);
	TestTrue(TEXT("Sword has its own candidates"), Combat->AssignSkillToInputSlot(Sword->ActiveTechniqueCatalog[0].Skill, Slot::ESkill));
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, Knuckle->WeaponType);
	for (int32 Index = 0; Index < 6; ++Index)
	{ TestEqual(TEXT("Outgoing preset survives new-state-first switch"), Combat->GetEquippedSkill(Slots[Index]), Entries[Indices[Index]].Skill.Get()); }
	TestTrue(TEXT("Valid catalog snapshot round trips"), Combat->RestoreCombat(Saved));
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, Sword->WeaponType);
	TestTrue(TEXT("Sword choice restored"), Combat->AssignSkillToInputSlot(Sword->ActiveTechniqueCatalog[0].Skill, Slot::ESkill));
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, Knuckle->WeaponType);
	const auto BeforeRejected = Combat->CaptureCombat();
	auto InactiveInvalid = BeforeRejected;
	auto* InactivePreset = InactiveInvalid.WeaponPresets.FindByPredicate([](const auto& Preset)
	{ return Preset.WeaponType == ERogue10mWeaponType::GreatSword; });
	if (!TestNotNull(TEXT("Inactive sword preset exists"), InactivePreset)) { return false; }
	InactivePreset->Slots.Add(Slot::QSkill, Sword->SkillTreeSkills.Last().Get());
	TestFalse(TEXT("Inactive catalog gate uses its own weapon profile"), Combat->RestoreCombat(InactiveInvalid));
	const auto AfterRejected = Combat->CaptureCombat();
	TestTrue(TEXT("Rejected restore preserves all learned skills, slots and both presets"),
		FRogue10mCombatSnapshot::StaticStruct()->CompareScriptStruct(&BeforeRejected, &AfterRejected, 0));
	auto Invalid = Saved;
	Invalid.SkillBindings.Add(Slot::QSkill, Outside);
	TestFalse(TEXT("Outside active save binding denied transactionally"), Combat->RestoreCombat(Invalid));
	Invalid = Saved;
	Invalid.WeaponPresets[0].Slots.Add(Slot::QSkill, Outside);
	TestFalse(TEXT("Outside preset denied transactionally"), Combat->RestoreCombat(Invalid));
	TestEqual(TEXT("Rejected restore leaves choice intact"), Combat->GetEquippedSkill(Slot::QSkill), Entries[0].Skill.Get());
	const auto ValidCatalog = Knuckle->ActiveTechniqueCatalog;
	Knuckle->ActiveTechniqueCatalog.Last().TechniqueId = NAME_None;
	TestTrue(TEXT("Invalid catalog publishes no candidates"), Combat->GetSelectableActiveTechniques().IsEmpty());
	TestEqual(TEXT("Invalid catalog hides stale loadout slots"), Combat->GetEquippedLoadoutSkillCount(), 0);
	TestFalse(TEXT("Invalid final entry blocks unchanged first skill"), Combat->CanEquipSkillInSlot(Entries[0].Skill, Slot::QSkill));
	TestFalse(TEXT("Invalid catalog blocks bound execution"), FRogue10mCatalogConsumptionTestAccess::CanUse(*Combat, Entries[0].Skill));
	TestNotNull(TEXT("Invalid catalog keeps basic"), Combat->GetEquippedSkill(Slot::Primary));
	TestTrue(TEXT("Invalid catalog keeps learned outside growth"), Combat->CaptureCombat().UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Outside)));
	Knuckle->ActiveTechniqueCatalog = ValidCatalog;
	TestTrue(TEXT("Repair restores catalog behavior"), Combat->CanEquipSkillInSlot(Entries[0].Skill, Slot::QSkill));
	Knuckle->ActiveTechniqueCatalog.Reset();
	TestTrue(TEXT("Absent optional catalog preserves legacy choice"), Combat->CanEquipSkillInSlot(Outside, Slot::QSkill));
	Knuckle->ActiveTechniqueCatalog = ValidCatalog;
	Sword->WeaponType = ERogue10mWeaponType::Staff;
	Sword->ActiveTechniqueCatalog.Reset();
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, ERogue10mWeaponType::Staff);
	TestTrue(TEXT("Mage legacy profile remains independent"), Combat->IsSelectableActiveTechnique(Sword->SkillTreeSkills.Last()));
	FRogue10mCatalogConsumptionTestAccess::Switch(*Combat, *State, ERogue10mWeaponType::Unarmed);
	TestTrue(TEXT("Novice has no selectable weapon techniques"), Combat->GetSelectableActiveTechniques().IsEmpty());
	TestNotNull(TEXT("Novice keeps fixed punch"), Combat->GetEquippedSkill(Slot::Primary));
	return true;
}
#endif
