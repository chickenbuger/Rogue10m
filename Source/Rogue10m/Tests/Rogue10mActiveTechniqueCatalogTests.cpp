#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mSkillLoadoutDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mActiveTechniqueCatalogTest,
	"Rogue10m.SkillLoadout.ActiveTechniqueCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mActiveTechniqueCatalogTest::RunTest(const FString&)
{
	using P = ERogue10mSkillSlotPolicy;
	auto* Profile = NewObject<URogue10mWeaponSkillProfileDataAsset>();
	Profile->WeaponType = ERogue10mWeaponType::Knuckle;
	auto* Basic = NewObject<URogue10mAttackSkillData>(Profile);
	Basic->AllowedSlotPolicy = P::Common; // Isolate fixed-primary rejection from policy rejection.
	Profile->DefaultSkillBindings.Add(ERogue10mAttackInputSlot::Primary, Basic);
	Profile->SkillTreeSkills.Add(Basic);
	auto* Passive = NewObject<URogue10mAttackSkillData>(Profile);
	Passive->SkillTreeNodeType = ERogue10mSkillTreeNodeType::PassiveTechnique;
	Passive->AllowedSlotPolicy = P::Common; // Isolate passive rejection.
	Profile->SkillTreeSkills.Add(Passive);
	FText Reason;
	TestTrue(TEXT("absent optional catalog preserves legacy validity"), Profile->ValidateActiveTechniqueCatalog(Reason));
	TestFalse(TEXT("legacy does not opt into the catalog"), Profile->HasExplicitActiveTechniqueCatalog());
	TestTrue(TEXT("absent catalog never falls back to full tree"), Profile->GetValidatedActiveTechniqueCatalog().IsEmpty());
	for (int32 I = 0; I < 10; ++I)
	{
		auto* Skill = NewObject<URogue10mAttackSkillData>(Profile);
		Skill->AllowedSlotPolicy = I < 6 ? P::Common : (I < 8 ? P::MouseChordOnly : P::RightChargeOnly);
		Profile->SkillTreeSkills.Add(Skill);
		auto& Entry = Profile->ActiveTechniqueCatalog.AddDefaulted_GetRef();
		Entry.TechniqueId = FName(*FString::Printf(TEXT("Test.Technique.%02d"), I));
		Entry.Skill = Skill;
	}
	Profile->InitiallyUnlockedSkills = {Basic, Profile->ActiveTechniqueCatalog[0].Skill};
	const auto OriginalTree = Profile->SkillTreeSkills;
	const auto OriginalDefaults = Profile->DefaultSkillBindings;
	const auto OriginalUnlocks = Profile->InitiallyUnlockedSkills;
	const auto Valid = Profile->ActiveTechniqueCatalog;
	TestTrue(TEXT("ten explicit unique active techniques accepted"), Profile->ValidateActiveTechniqueCatalog(Reason));
	TestTrue(TEXT("valid clears old error"), Reason.IsEmpty());
	TestEqual(TEXT("exact ten returned independently of twelve full nodes"), Profile->GetValidatedActiveTechniqueCatalog().Num(), 10);
	TestEqual(TEXT("lookup uses stable id"), Profile->FindActiveTechniqueById(Valid[7].TechniqueId), Valid[7].Skill.Get());
	TestNull(TEXT("unknown id does not fall back"), Profile->FindActiveTechniqueById(TEXT("Missing")));
	const auto Reject = [this, Profile, &Reason, &Valid](const TCHAR* Message)
	{
		TestFalse(Message, Profile->ValidateActiveTechniqueCatalog(Reason));
		TestFalse(TEXT("invalid catalog explains rejection"), Reason.IsEmpty());
		TestTrue(TEXT("invalid catalog publishes no partial entries"), Profile->GetValidatedActiveTechniqueCatalog().IsEmpty());
		TestNull(TEXT("invalid catalog cannot resolve even an unchanged entry"), Profile->FindActiveTechniqueById(Valid[5].TechniqueId));
		Profile->ActiveTechniqueCatalog = Valid;
	};
	Profile->ActiveTechniqueCatalog.Pop(); Reject(TEXT("nine rejected"));
	Profile->ActiveTechniqueCatalog.Add(Valid[0]); Reject(TEXT("eleven rejected"));
	Profile->ActiveTechniqueCatalog[0].TechniqueId = NAME_None; Reject(TEXT("missing stable id rejected"));
	Profile->ActiveTechniqueCatalog[1].TechniqueId = Valid[0].TechniqueId; Reject(TEXT("duplicate stable id rejected"));
	Profile->ActiveTechniqueCatalog[0].Skill = nullptr; Reject(TEXT("null skill rejected"));
	Profile->ActiveTechniqueCatalog[1].Skill = Valid[0].Skill; Reject(TEXT("duplicate skill rejected"));
	Profile->ActiveTechniqueCatalog[0].Skill = Basic; Reject(TEXT("fixed primary rejected"));
	Profile->ActiveTechniqueCatalog[0].Skill = Passive; Reject(TEXT("passive rejected"));
	auto* Outside = NewObject<URogue10mAttackSkillData>(Profile);
	Outside->AllowedSlotPolicy = P::Common;
	Profile->ActiveTechniqueCatalog[0].Skill = Outside; Reject(TEXT("outside progression tree rejected independently of policy"));
	Profile->ActiveTechniqueCatalog.Last().TechniqueId = NAME_None; Reject(TEXT("invalid final entry rejects unchanged earlier entries"));
	for (P Policy : {P::Legacy, P::BasicOnly})
	{
		Valid[0].Skill->AllowedSlotPolicy = Policy; Reject(TEXT("implicit or basic-only policy rejected"));
	}
	Valid[0].Skill->AllowedSlotPolicy = P::Common;
	Profile->WeaponType = ERogue10mWeaponType::Unarmed; Reject(TEXT("novice must not acquire ten weapon techniques"));
	Profile->WeaponType = ERogue10mWeaponType::Staff; Reject(TEXT("mage command rules not assumed to use ten-technique catalog"));
	Profile->WeaponType = ERogue10mWeaponType::Knuckle;
	TestTrue(TEXT("fully restored catalog valid"), Profile->ValidateActiveTechniqueCatalog(Reason));
	TestTrue(TEXT("valid restored catalog clears prior error"), Reason.IsEmpty());
	TestTrue(TEXT("full tree stays unchanged"), Profile->SkillTreeSkills == OriginalTree);
	TestTrue(TEXT("default bindings stay unchanged"), Profile->DefaultSkillBindings.OrderIndependentCompareEqual(OriginalDefaults));
	TestTrue(TEXT("unlock records stay unchanged"), Profile->InitiallyUnlockedSkills == OriginalUnlocks);
	return true;
}
#endif
