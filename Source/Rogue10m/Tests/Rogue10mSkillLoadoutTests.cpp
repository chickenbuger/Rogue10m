#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mWeaponIdentityComponent.h"

struct FRogue10mSkillLoadoutTestAccess
{
	static void Configure(URogue10mCombatComponent& C, URogue10mWeaponSkillProfileDataAsset* A, URogue10mWeaponSkillProfileDataAsset* B)
	{
		C.CharacterData = nullptr; C.WeaponSkillProfiles = {A,B};
		auto* Novice=NewObject<URogue10mWeaponSkillProfileDataAsset>(); Novice->WeaponType=ERogue10mWeaponType::Unarmed;
		auto* Punch=NewObject<URogue10mAttackSkillData>(); Punch->AllowedSlotPolicy=ERogue10mSkillSlotPolicy::BasicOnly;
		Novice->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary,Punch); C.WeaponSkillProfiles.Add(Novice);
		C.TestUnlockedAttackSkillNames.Reset(); C.UnlockedAttackSkillNames.Reset(); C.WeaponSkillPresets.Reset(); C.bWeaponPresetApplied = false;
		for (const auto* P : {A,B}) { for (const URogue10mAttackSkillData* S : P->SkillTreeSkills) { C.UnlockedAttackSkillNames.Add(S->GetFName()); } }
	}
	static void Preview(URogue10mCombatComponent& C, URogue10mAttackSkillData* S) { C.TestUnlockedAttackSkillNames.Add(S->GetFName()); C.UnlockedAttackSkillNames.Remove(S->GetFName()); }
	static void Learn(URogue10mCombatComponent& C, URogue10mAttackSkillData* S) { C.TestUnlockedAttackSkillNames.Remove(S->GetFName()); C.UnlockedAttackSkillNames.Add(S->GetFName()); }
	static void ForceBurst(URogue10mWeaponIdentityComponent& C) { C.EndsAt = C.GetWorld()->GetTimeSeconds() + 1.f; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLoadoutPolicyTest, "Rogue10m.SkillLoadout.SlotPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLoadoutPolicyTest::RunTest(const FString&)
{
	using S=ERogue10mAttackInputSlot; using P=ERogue10mSkillSlotPolicy;
	auto* Skill=NewObject<URogue10mAttackSkillData>();
	Skill->AllowedSlotPolicy=P::Common;
	for (auto Slot : {S::QSkill,S::ESkill,S::RSkill,S::Special}) { TestTrue(TEXT("common accepts common slot"),Skill->IsAllowedInLoadoutSlot(Slot)); }
	for (auto Slot : {S::Primary,S::JumpPrimary,S::JumpSpecial,S::ChargedPrimary,S::ChargedSpecial,S::MouseChord}) { TestFalse(TEXT("common rejects fixed/dedicated"),Skill->IsAllowedInLoadoutSlot(Slot)); }
	Skill->AllowedSlotPolicy=P::MouseChordOnly;
	TestTrue(TEXT("chord accepts chord"),Skill->IsAllowedInLoadoutSlot(S::MouseChord)); TestFalse(TEXT("chord cannot use Q"),Skill->IsAllowedInLoadoutSlot(S::QSkill)); TestFalse(TEXT("chord cannot charge"),Skill->IsAllowedInLoadoutSlot(S::ChargedSpecial));
	Skill->AllowedSlotPolicy=P::RightChargeOnly;
	TestTrue(TEXT("charge accepts charge"),Skill->IsAllowedInLoadoutSlot(S::ChargedSpecial)); TestFalse(TEXT("charge cannot chord"),Skill->IsAllowedInLoadoutSlot(S::MouseChord)); TestFalse(TEXT("charge cannot RMB tap"),Skill->IsAllowedInLoadoutSlot(S::Special));
	Skill->AllowedSlotPolicy=P::Legacy; Skill->InputSlot=S::ChargedPrimary;
	TestTrue(TEXT("legacy left charge migrates to RMB hold"),Skill->IsAllowedInLoadoutSlot(S::ChargedSpecial)); TestFalse(TEXT("legacy charge cannot use Q"),Skill->IsAllowedInLoadoutSlot(S::QSkill));
	Skill->AllowedSlotPolicy=P::Common; Skill->SkillTreeNodeType=ERogue10mSkillTreeNodeType::PassiveTechnique;
	TestFalse(TEXT("passive cannot equip"),Skill->IsAllowedInLoadoutSlot(S::QSkill));
	TestEqual(TEXT("legacy enum numeric preserved"),static_cast<uint8>(S::ChargedSpecial),uint8(5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mLoadoutPresetTest, "Rogue10m.SkillLoadout.PresetsAndNovice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mLoadoutPresetTest::RunTest(const FString&)
{
	using S=ERogue10mAttackInputSlot; using P=ERogue10mSkillSlotPolicy;
	const auto Init=UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* State=World->SpawnActor<ARogue10mPlayerState>();
	auto* Class=LoadClass<ARogue10mCharacter>(nullptr,TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	auto* Character=Class ? World->SpawnActor<ARogue10mCharacter>(Class) : nullptr; auto* C=Character ? Character->GetCombatComponent() : nullptr;
	if (!C || !State) { AddError(TEXT("fixture character/state failed")); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	Character->SetPlayerState(State); auto* ASC=State->GetAbilitySystemComponent(); ASC->AddAttributeSetSubobject(State->GetRogueAttributeSet()); ASC->InitAbilityActorInfo(State,Character);
	auto* A=NewObject<URogue10mWeaponSkillProfileDataAsset>(); A->WeaponType=ERogue10mWeaponType::Knuckle;
	auto* B=NewObject<URogue10mWeaponSkillProfileDataAsset>(); B->WeaponType=ERogue10mWeaponType::GreatSword;
	TArray<URogue10mAttackSkillData*> Skills;
	for (int32 I=0;I<9;++I) { auto* V=NewObject<URogue10mAttackSkillData>(); V->AllowedSlotPolicy=I==6 ? P::MouseChordOnly : I==7 ? P::RightChargeOnly : P::Common; Skills.Add(V); A->SkillTreeSkills.Add(V); }
	A->DefaultSkillBindings.Add(S::Primary,Skills[8]);
	auto* Sword=NewObject<URogue10mAttackSkillData>(); Sword->AllowedSlotPolicy=P::Common; B->SkillTreeSkills.Add(Sword); B->DefaultSkillBindings.Add(S::Primary,Sword);
	FRogue10mSkillLoadoutTestAccess::Configure(*C,A,B); Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestFalse(TEXT("fixed basic cannot be equipped"),C->AssignSkillToInputSlot(Skills[8],S::QSkill));
	const S Slots[]={S::QSkill,S::ESkill,S::RSkill,S::Special,S::MouseChord,S::ChargedSpecial}; const int32 Ids[]={0,1,2,3,6,7};
	for (int32 I=0;I<6;++I) { TestTrue(TEXT("equip six valid candidates"),C->AssignSkillToInputSlot(Skills[Ids[I]],Slots[I])); }
	TestEqual(TEXT("six max slots"),C->GetEquippedLoadoutSkillCount(),6);
	TestFalse(TEXT("legacy quick slot cannot bypass charge"),C->ActivateQuickSlot(4));
	TestFalse(TEXT("cannot replace primary"),C->AssignSkillToInputSlot(Skills[4],S::Primary));
	TestFalse(TEXT("cannot cross dedicated slots"),C->AssignSkillToInputSlot(Skills[6],S::ChargedSpecial));
	TestTrue(TEXT("move common within four"),C->AssignSkillToInputSlot(Skills[0],S::ESkill)); TestNull(TEXT("old slot emptied"),C->GetEquippedSkill(S::QSkill));
	Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword); TestEqual(TEXT("sword no knuckle choices"),C->GetEquippedLoadoutSkillCount(),0);
	TestFalse(TEXT("cannot cross weapons"),C->AssignSkillToInputSlot(Skills[0],S::QSkill));
	Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle); TestEqual(TEXT("knuckle choice restored"),C->GetEquippedSkill(S::ESkill),Skills[0]); TestNull(TEXT("cleared slot stays empty"),C->GetEquippedSkill(S::QSkill));
	auto* Burst=NewObject<URogue10mWeaponIdentityComponent>(Character); Character->AddInstanceComponent(Burst); Burst->RegisterComponent();
	FRogue10mSkillLoadoutTestAccess::ForceBurst(*Burst);
	TestTrue(TEXT("knuckle burst active"),Burst->IsIdentityActive()); TestTrue(TEXT("burst speed overlay"),Burst->GetAttackSpeedMultiplier()>1.f); TestTrue(TEXT("burst movement overlay"),Burst->GetMovementMultiplier()>1.f);
	TestTrue(TEXT("burst skill enhancement"),Burst->GetRangeMultiplier(*Skills[0])>1.f); TestEqual(TEXT("basic range not boosted"),Burst->GetRangeMultiplier(*Skills[8]),1.f);
	Burst->DeactivateIdentity(); TestEqual(TEXT("burst movement clears"),Burst->GetMovementMultiplier(),1.f); TestEqual(TEXT("burst damage clears"),Burst->GetDamageMultiplier(*Skills[0]),1.f);
	const auto Save=C->CaptureCombat(); TestTrue(TEXT("round trip snapshot"),C->RestoreCombat(Save));
	auto Legacy=Save; Legacy.WeaponPresets.Reset(); Legacy.SkillBindings.Remove(S::ChargedSpecial); Legacy.SkillBindings.Add(S::ChargedPrimary,Skills[7]);
	TestTrue(TEXT("old left-charge snapshot migrates"),C->RestoreCombat(Legacy)); TestEqual(TEXT("left charge becomes dedicated RMB hold"),C->GetEquippedSkill(S::ChargedSpecial),Skills[7]);
	TestTrue(TEXT("restore modern snapshot after migration test"),C->RestoreCombat(Save));
	if (!TestTrue(TEXT("saved preset exists before malformed-input test"),!Save.WeaponPresets.IsEmpty())) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	auto Invalid=Save; Invalid.WeaponPresets[0].Slots.Add(S::QSkill,Skills[6]); TestFalse(TEXT("reject bad saved policy transactionally"),C->RestoreCombat(Invalid)); TestEqual(TEXT("bad restore preserves choice"),C->GetEquippedSkill(S::ESkill),Skills[0]);
	FRogue10mSkillLoadoutTestAccess::Preview(*C,Skills[5]); TestTrue(TEXT("preview equip"),C->AssignSkillToInputSlot(Skills[5],S::RSkill)); const auto Preview=C->CaptureCombat();
	TestFalse(TEXT("preview not durable"),Preview.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Skills[5])));
	FRogue10mSkillLoadoutTestAccess::Learn(*C,Skills[5]); Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword); Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("newly learned preview survives weapon swap"),C->GetEquippedSkill(S::RSkill),Skills[5]); TestTrue(TEXT("learned preview snapshot valid"),C->RestoreCombat(C->CaptureCombat()));
	auto* Tree=State->GetSkillTreeProgressionComponent();
	auto Growth=State->CaptureGrowth(); Growth.Level=15; TestTrue(TEXT("growth fixture level"),State->RestoreGrowth(Growth));
	auto TreeState=Tree->Capture(); TreeState.Masteries.Reset(); auto& Mastery=TreeState.Masteries.AddDefaulted_GetRef(); Mastery.Track=ERogue10mSkillTreeTrack::Brawler; Mastery.TotalExperience=385;
	if (!TreeState.Investments.ContainsByPredicate([](const auto& I){return I.NodeId==TEXT("I.InternalEnergy.Entry");}))
	{ auto& Entry=TreeState.Investments.AddDefaulted_GetRef(); Entry.NodeId=TEXT("I.InternalEnergy.Entry"); Entry.Route=ERogue10mSkillTreeUnlockRoute::Points; Entry.SpentPoints=1; }
	TreeState.ActiveIdentity=ERogue10mSkillTreeIdentity::InternalEnergy;
	TestTrue(TEXT("existing knuckle mastery/identity fixture restores"),Tree->Restore(TreeState));
	const auto BeforeNovice=Tree->Capture();
	Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Unarmed); TestFalse(TEXT("novice cannot equip techniques"),C->AssignSkillToInputSlot(Skills[0],S::QSkill)); TestEqual(TEXT("novice empty six slots"),C->GetEquippedLoadoutSkillCount(),0);
	TestEqual(TEXT("novice has no weapon growth track"),Tree->GetActiveTrack(),ERogue10mSkillTreeTrack::None);
	TestEqual(TEXT("novice has no active old identity"),Tree->GetActiveIdentity(),ERogue10mSkillTreeIdentity::None);
	TestEqual(TEXT("novice no brawler mastery stat overlay"),Tree->GetMasteryStatModifiers(ERogue10mWeaponType::Unarmed).AttackPowerBonus,0.f);
	TestTrue(TEXT("stored brawler mastery still exists"),Tree->GetTrackMasteryStatModifiers(ERogue10mSkillTreeTrack::Brawler).AttackPowerBonus>0.f);
	TestNotNull(TEXT("novice fixed punch remains equipped"),C->GetEquippedSkill(S::Primary)); TestTrue(TEXT("novice punch is independently unlocked"),C->IsAttackSkillUnlocked(C->GetEquippedSkill(S::Primary)));
	TestTrue(TEXT("novice punch accepted by GAS"),C->ActivateQuickSlot(1)); TestNull(TEXT("novice no legacy RMB fallback"),C->GetEquippedSkill(S::Special));
	Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	TestEqual(TEXT("knuckle track restores on equipment"),Tree->GetActiveTrack(),ERogue10mSkillTreeTrack::Brawler);
	TestEqual(TEXT("remembered knuckle identity restores"),Tree->GetActiveIdentity(),ERogue10mSkillTreeIdentity::InternalEnergy);
	TestEqual(TEXT("knuckle mastery experience preserved"),Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler),385);
	const auto AfterNovice=Tree->Capture(); TestTrue(TEXT("novice transition does not erase stored growth"),FRogue10mSkillTreeSnapshot::StaticStruct()->CompareScriptStruct(&BeforeNovice,&AfterNovice,0));
	TestEqual(TEXT("knuckle equipped choice restored after novice"),C->GetEquippedSkill(S::RSkill),Skills[5]);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}
#endif
