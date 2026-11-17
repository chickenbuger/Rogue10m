#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFiveVillageGameMode.h"
#include "Rogue10mFiveVillageRouteDataAsset.h"
#include "Rogue10mFiveVillageRunPortal.h"
#include "Rogue10mGameMode.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mFiveVillageRuntimeDefaultsTest,
	"Rogue10m.FiveVillage.RuntimeDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mFiveVillageRuntimeDefaultsTest::RunTest(const FString&)
{
	const auto* Base = GetDefault<ARogue10mGameMode>();
	const auto* Mode = GetDefault<ARogue10mFiveVillageGameMode>();
	const auto* Portal = GetDefault<ARogue10mFiveVillageRunPortal>();
	if (!TestNotNull(TEXT("Production GameMode CDO"), Base)
		|| !TestNotNull(TEXT("Five-village GameMode CDO"), Mode)
		|| !TestNotNull(TEXT("Five-village portal CDO"), Portal)) { return false; }
	for (const TCHAR* Name : {TEXT("DefaultPawnClass"), TEXT("PlayerControllerClass"),
		TEXT("PlayerStateClass"), TEXT("GameStateClass"), TEXT("HUDClass")})
	{
		const auto* Property = FindFProperty<FObjectPropertyBase>(Base->GetClass(), FName(Name));
		if (!TestNotNull(*FString::Printf(TEXT("Reflected %s property"), Name), Property)) { continue; }
		const UObject* Expected = Property->GetObjectPropertyValue_InContainer(Base);
		const UObject* Actual = Property->GetObjectPropertyValue_InContainer(Mode);
		const bool bExpectedValid = TestNotNull(*FString::Printf(TEXT("Production %s class"), Name), Expected);
		const bool bActualValid = TestNotNull(*FString::Printf(TEXT("Candidate %s class"), Name), Actual);
		if (bExpectedValid && bActualValid)
		{
			TestTrue(*FString::Printf(TEXT("Candidate preserves production %s"), Name), Actual == Expected);
		}
	}
	const auto CheckBool = [this](const UObject* Object, const TCHAR* Name, bool Expected)
	{
		if (!TestNotNull(*FString::Printf(TEXT("%s owner CDO"), Name), Object)) { return; }
		const auto* Property = FindFProperty<FBoolProperty>(Object->GetClass(), FName(Name));
		if (TestNotNull(*FString::Printf(TEXT("Reflected %s property"), Name), Property))
		{
			TestEqual(Name, Property->GetPropertyValue_InContainer(Object), Expected);
		}
	};
	CheckBool(Mode, TEXT("bStartRunOnStartPlay"), false);
	CheckBool(Mode, TEXT("bUsePrototypeRunDuration"), false);
	CheckBool(Portal, TEXT("bStartRunTimerOnTravel"), true);
	CheckBool(GetDefault<ARogue10mCharacter>(), TEXT("bUseThirdPersonGameplay"), true);
	const auto* Target = FindFProperty<FNameProperty>(Portal->GetClass(), TEXT("TargetLevelName"));
	if (TestNotNull(TEXT("Reflected portal target property"), Target))
	{
		TestEqual(TEXT("Portal uses unambiguous candidate package"), Target->GetPropertyValue_InContainer(Portal),
			FName(TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110")));
	}
	const auto* Options = FindFProperty<FStrProperty>(Portal->GetClass(), TEXT("AdditionalTravelOptions"));
	if (TestNotNull(TEXT("Reflected portal travel options property"), Options))
	{
		TestEqual(TEXT("Portal selects only the candidate mode"), Options->GetPropertyValue_InContainer(Portal),
			FString(TEXT("game=/Script/Rogue10m.Rogue10mFiveVillageGameMode")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mFiveVillageRouteDefinitionTest,
	"Rogue10m.FiveVillage.RouteDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mFiveVillageRouteDefinitionTest::RunTest(const FString&)
{
	auto* Definition = NewObject<URogue10mFiveVillageRouteDataAsset>();
	if (!TestNotNull(TEXT("Transient route definition"), Definition)) { return false; }
	FString Reason;
	TestTrue(TEXT("Default definition validates"), Definition->ValidateDefinition(Reason));
	TestTrue(TEXT("Valid definition clears error text"), Reason.IsEmpty());
	if (!TestEqual(TEXT("Default route has exactly five villages"), Definition->Villages.Num(), 5)) { return false; }
	const FVector ExpectedCenters[] = {FVector(-14000, 8000, 0), FVector(14000, 8000, 0),
		FVector(-30000, 23000, 0), FVector(6000, 23000, 0), FVector(30000, 23000, 0)};
	TSet<FName> Seen;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const auto& Village = Definition->Villages[Index];
		TestEqual(TEXT("Stable village ID"), Village.VillageId, FName(*FString::Printf(TEXT("V%d"), Index + 1)));
		TestFalse(TEXT("Village ID is unique"), Seen.Contains(Village.VillageId));
		Seen.Add(Village.VillageId);
		TestTrue(TEXT("Center matches authored route"), Village.VillageCenter.Equals(ExpectedCenters[Index]));
		TestTrue(TEXT("Village radius is finite and positive"), FMath::IsFinite(Village.RadiusCm) && Village.RadiusCm > 0);
	}
	TestTrue(TEXT("Boss approach matches authored route"), Definition->BossApproachLocation.Equals(FVector(0, 30000, 0)));
	TestTrue(TEXT("Boss approach radius is positive"), Definition->BossApproachRadiusCm > 0);
	const auto OriginalVillages = Definition->Villages;
	const auto ExpectInvalid = [this, Definition, &Reason](const TCHAR* Label)
	{
		TestFalse(Label, Definition->ValidateDefinition(Reason));
		TestFalse(TEXT("Rejected definition explains the problem"), Reason.IsEmpty());
	};
	Definition->Villages.Pop();
	ExpectInvalid(TEXT("Missing village rejected"));
	Definition->Villages = OriginalVillages;
	Definition->Villages[1].VillageId = Definition->Villages[0].VillageId;
	ExpectInvalid(TEXT("Duplicate village ID rejected"));
	Definition->Villages = OriginalVillages;
	Definition->Villages[0].RadiusCm = 0;
	ExpectInvalid(TEXT("Zero radius rejected"));
	Definition->Villages[0].RadiusCm = std::numeric_limits<float>::quiet_NaN();
	ExpectInvalid(TEXT("NaN radius rejected"));
	Definition->Villages = OriginalVillages;
	Definition->Villages[0].VillageCenter.X = std::numeric_limits<double>::infinity();
	ExpectInvalid(TEXT("Infinite village location rejected"));
	Definition->Villages = OriginalVillages;
	Definition->BossApproachRadiusCm = 0;
	ExpectInvalid(TEXT("Zero boss approach radius rejected"));
	return !HasAnyErrors();
}
#endif
