#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCombatVfxPlacement.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCombatVfxPlacementTest,
	"Rogue10m.Combat.VfxNearSideBoundsPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mCombatVfxPlacementTest::RunTest(const FString& Parameters)
{
	using Rogue10m::CombatVfx::NearSideBoundsLocation;
	const FVector Center(100.0, 200.0, 300.0);
	const FVector Extent(80.0, 20.0, 100.0);
	TestTrue(TEXT("Long X target exits X face, preserving torso height"),
		NearSideBoundsLocation(Center, Extent, Center + FVector(200, 0, 0), 4)
		.Equals(Center + FVector(84, 0, 0)));
	TestTrue(TEXT("Negative Y direction exits near Y face"),
		NearSideBoundsLocation(Center, Extent, Center + FVector(0, -200, 0), 4)
		.Equals(Center + FVector(0, -24, 0)));
	const FVector Diagonal = NearSideBoundsLocation(Center, Extent, Center + FVector(200, 200, 50), 0);
	TestTrue(TEXT("Diagonal ray hits first rectangle face instead of minimum-extent radius"),
		Diagonal.Equals(Center + FVector(20, 20, 0)));
	TestTrue(TEXT("No horizontal attacker direction keeps center"),
		NearSideBoundsLocation(Center, Extent, Center + FVector(0, 0, 100), 4).Equals(Center));
	TestTrue(TEXT("Negative margin does not push feedback into bounds"),
		NearSideBoundsLocation(Center, Extent, Center + FVector(200, 0, 0), -4)
		.Equals(Center + FVector(80, 0, 0)));
	TestTrue(TEXT("Degenerate point bounds stay finite"),
		NearSideBoundsLocation(Center, FVector::ZeroVector, Center + FVector(200, 0, 0), 4)
		.Equals(Center + FVector(4, 0, 0)));
	const URogue10mAttackSkillData* Defaults = GetDefault<URogue10mAttackSkillData>();
	TestTrue(TEXT("Existing assets retain center placement by default"),
		!Defaults->bUseThirdPersonImpactBoundsSurface && Defaults->ThirdPersonImpactEffectSurfaceOffset == 0);
	TestTrue(TEXT("Existing assets retain socket cast placement by default"),
		Defaults->ThirdPersonCastEffectOffset.IsZero() && Defaults->ThirdPersonCastEffectRotation.IsZero());
	return !HasAnyErrors();
}
#endif
