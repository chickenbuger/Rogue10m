#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"

struct FRogue10mWeaponCooldownTestAccess
{
	static void Profiles(URogue10mCombatComponent& Combat, URogue10mWeaponSkillProfileDataAsset* Knuckle,
		URogue10mWeaponSkillProfileDataAsset* Sword)
	{
		Combat.CharacterData = nullptr;
		Combat.WeaponSkillProfiles = {Knuckle, Sword};
	}
	static bool Attack(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
	{ return Combat.ExecuteAttackSkill(Skill, false); }
	static float Deadline(const URogue10mCombatComponent& Combat) { return Combat.AttackCooldownEndTime; }
};

namespace Rogue10mWeaponCooldownTests
{
struct FFixture
{
	UWorld* World = nullptr;
	ARogue10mPlayerState* State = nullptr;
	ARogue10mCharacter* Character = nullptr;
	URogue10mAttackSkillData* Skill = nullptr;
	URogue10mAttackSkillData* SwordSkill = nullptr;
	FFixture()
	{
		const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		// UWorld::Tick queries seamless travel through the engine's world context.
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		State = World->SpawnActor<ARogue10mPlayerState>();
		auto* Class = LoadClass<ARogue10mCharacter>(nullptr,
			TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		Character = Class ? World->SpawnActor<ARogue10mCharacter>(Class) : nullptr;
		if (!Character || !State) { return; }
		Character->SetPlayerState(State);
		auto* ASC = State->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet()); ASC->InitAbilityActorInfo(State, Character);
		Skill = NewObject<URogue10mAttackSkillData>(); SwordSkill = NewObject<URogue10mAttackSkillData>();
		for (auto* Attack : {Skill, SwordSkill})
		{
			Attack->AttackCooldown = 0.0f;
			Attack->HitStartDelaySeconds = 10.0f; // No hits or save system in this isolated world.
			Attack->ResourceCosts = {{ERogue10mAttackResourceType::Stamina, 10.0f}};
		}
		auto* Knuckle = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		auto* Sword = NewObject<URogue10mWeaponSkillProfileDataAsset>();
		Knuckle->WeaponType = ERogue10mWeaponType::Knuckle; Sword->WeaponType = ERogue10mWeaponType::GreatSword;
		Knuckle->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, Skill);
		Sword->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, SwordSkill);
		FRogue10mWeaponCooldownTestAccess::Profiles(Combat(), Knuckle, Sword);
		Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	}
	~FFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } }
	bool IsValid() const { return Character && State && Skill && SwordSkill; }
	URogue10mCombatComponent& Combat() { return *Character->GetCombatComponent(); }
	bool Attack(URogue10mAttackSkillData* AttackSkill = nullptr)
	{ return FRogue10mWeaponCooldownTestAccess::Attack(Combat(), *(AttackSkill ? AttackSkill : Skill)); }
	void Advance(float Seconds)
	{
		// Exercise normal world time; avoid large-frame delta clamping in the engine.
		while (Seconds > UE_SMALL_NUMBER)
		{
			const float Step = FMath::Min(0.05f, Seconds);
			World->Tick(LEVELTICK_All, Step); Seconds -= Step;
		}
	}
	float Deadline() { return FRogue10mWeaponCooldownTestAccess::Deadline(Combat()); }
	void Swap() { Character->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwapFloorTest, "Rogue10m.WeaponCooldown.FloorBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwapFloorTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponCooldownTests;
	for (float Duration : {0.0f, 0.2f, 0.5f, 5.0f})
	{
		FFixture F; if (!TestTrue(TEXT("concrete fixture"), F.IsValid())) { return false; }
		TestEqual(TEXT("initial profile does not add a floor"), F.Combat().GetAttackCooldownRemaining(), 0.0f);
		F.Skill->AttackCooldown = Duration;
		TestTrue(TEXT("real attack accepted before swap"), F.Attack());
		const float OldDeadline = F.Deadline();
		F.Swap();
		const float Expected = FMath::Max(Duration, 0.5f);
		TestTrue(TEXT("max remaining, 0.5; never addition"), FMath::IsNearlyEqual(F.Combat().GetAttackCooldownRemaining(), Expected));
		if (Duration >= 0.5f) { TestEqual(TEXT("long absolute deadline unchanged"), F.Deadline(), OldDeadline); }
		TestEqual(TEXT("target weapon profile applied"), F.Combat().GetEquippedSkill(ERogue10mAttackInputSlot::Primary), F.SwordSkill);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwapNoopTest, "Rogue10m.WeaponCooldown.SameTypeAndRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwapNoopTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponCooldownTests;
	FFixture F; if (!TestTrue(TEXT("concrete fixture"), F.IsValid())) { return false; }
	F.Swap(); const float OriginalDeadline = F.Deadline(); F.Advance(0.2f);
	F.Swap();
	TestEqual(TEXT("same weapon notification cannot extend floor"), F.Deadline(), OriginalDeadline);
	F.Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("restore preserves existing deadline"), F.Deadline(), OriginalDeadline);
	F.Advance(0.4f);
	F.Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);
	F.Combat().HandleEquippedWeaponChanged();
	TestEqual(TEXT("restore/refresh after expiry adds no floor"), F.Combat().GetAttackCooldownRemaining(), 0.0f);
	TestTrue(TEXT("restored character can attack immediately"), F.Attack(F.SwordSkill));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwapComboTest, "Rogue10m.WeaponCooldown.ReservedComboDeadline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwapComboTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponCooldownTests;
	FFixture F; if (!TestTrue(TEXT("concrete fixture"), F.IsValid())) { return false; }
	F.Skill->AttackCooldown = 5.0f; F.Skill->bEnableCombo = true; F.Skill->NextComboSkill = F.SwordSkill;
	F.Skill->ComboWindowOpenSeconds = 1.0f; F.Skill->ComboWindowCloseSeconds = 2.0f;
	TestTrue(TEXT("combo root attack accepted"), F.Attack());
	const float ReservedEnd = F.Deadline();
	TestFalse(TEXT("original combo cooldown starts in future"), F.Combat().IsAttackOnCooldown(F.World->GetTimeSeconds()));
	F.Swap();
	TestEqual(TEXT("future absolute deadline preserved"), F.Deadline(), ReservedEnd);
	TestTrue(TEXT("cancelled combo becomes immediate cooldown"), F.Combat().IsAttackOnCooldown(F.World->GetTimeSeconds()));
	TestFalse(TEXT("combo window removed"), F.Combat().IsComboSequenceActive(F.World->GetTimeSeconds()));
	TestFalse(TEXT("target weapon cannot attack in old pre-cooldown gap"), F.Attack(F.SwordSkill));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwapRoundTripTest, "Rogue10m.WeaponCooldown.TimeAcrossWeapons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwapRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponCooldownTests;
	FFixture F; if (!TestTrue(TEXT("concrete fixture"), F.IsValid())) { return false; }
	F.Skill->AttackCooldown = 5.0f; TestTrue(TEXT("source attack accepted"), F.Attack());
	const float End = F.Deadline(); F.Advance(1.0f); F.Swap();
	F.Advance(1.0f); F.Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("away and return preserve original deadline"), F.Deadline(), End);
	// The public cooldown is float while UE world time is double.
	TestTrue(TEXT("unequipped time continues to elapse"), FMath::IsNearlyEqual(
		static_cast<double>(F.Combat().GetAttackCooldownRemaining()), End - F.World->GetTimeSeconds(), 1.e-4));
	TestTrue(TEXT("remaining time decreased"), F.Combat().GetAttackCooldownRemaining() < 5.0f);
	F.Advance(5.1f);
	TestEqual(TEXT("original timer expires naturally"), F.Combat().GetAttackCooldownRemaining(), 0.0f);
	TestTrue(TEXT("original weapon attack resumes"), F.Attack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwapAttackGateTest, "Rogue10m.WeaponCooldown.RealAttackAcceptance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwapAttackGateTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mWeaponCooldownTests;
	FFixture F; if (!TestTrue(TEXT("concrete fixture"), F.IsValid())) { return false; }
	F.Swap();
	TestFalse(TEXT("real attack rejected during floor"), F.Attack(F.SwordSkill));
	TestEqual(TEXT("rejection cannot spend stamina"), F.State->GetRogueAttributeSet()->GetStamina(), 100.0f);
	TestEqual(TEXT("rejection cannot record skill use"), F.State->GetSkillUseCount(F.SwordSkill->GetFName()), 0);
	F.Advance(0.51f);
	TestTrue(TEXT("real attack accepted after floor"), F.Attack(F.SwordSkill));
	TestEqual(TEXT("one resource payment"), F.State->GetRogueAttributeSet()->GetStamina(), 90.0f);
	TestEqual(TEXT("one accepted skill use"), F.State->GetSkillUseCount(F.SwordSkill->GetFName()), 1);
	return true;
}

#endif
