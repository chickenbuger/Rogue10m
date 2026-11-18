// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rogue10mRunCurrencyBaseline.h"

namespace Rogue10mCurrencyBaselineTests
{
static const FGuid FirstRun(1, 2, 3, 4);
static const FGuid SecondRun(5, 6, 7, 8);
static bool Same(const FRogue10mRunCurrencyBaseline& A, const FRogue10mRunCurrencyBaseline& B)
{
	return A.RunId == B.RunId && A.Gold == B.Gold && A.Crystals == B.Crystals
		&& A.LobbyEarnedPoints == B.LobbyEarnedPoints && A.bCaptured == B.bCaptured;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyPostPaymentTest,
	"Rogue10m.RunCurrencyBaseline.PostPaymentRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyPostPaymentTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	// Town gold/crystal costs reduce balances directly. Lobby purchases instead
	// retain the gross earned total and record spending in the separate Town ledger.
	// Abandonment restores the entry gross total without refunding that ledger.
	constexpr int32 BeforeTownGold = 100, BeforeTownCrystals = 10, LobbyEarnedTotal = 50;
	constexpr int32 CommittedTownSpent = 5;
	FRogue10mRunCurrencyBaseline Baseline;
	TestTrue(TEXT("Capture after committed town costs"), Baseline.TryCapture(FirstRun,
		BeforeTownGold - 30, BeforeTownCrystals - 4, LobbyEarnedTotal));
	for (const FIntVector Current : {FIntVector(200, 20, 90), FIntVector(0, 0, 0), FIntVector(63, 5, 41)})
	{
		int32 Gold = Current.X, Crystals = Current.Y, Lobby = Current.Z;
		TestTrue(TEXT("Run gains or spending resolve to entry checkpoint"), Baseline.TryResolve(FirstRun, Gold, Crystals, Lobby));
		TestEqual(TEXT("Town gold cost stays spent"), Gold, 70);
		TestEqual(TEXT("Town crystal cost stays spent"), Crystals, 6);
		TestEqual(TEXT("Lobby gross total is restored without subtracting town costs twice"), Lobby, LobbyEarnedTotal);
		TestEqual(TEXT("Separate committed Town spending stays spent"), Lobby - CommittedTownSpent, 45);
		TestNotEqual(TEXT("No refund to pre-town balance"), Gold, BeforeTownGold);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyImmutableCaptureTest,
	"Rogue10m.RunCurrencyBaseline.ImmutableCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyImmutableCaptureTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	FRogue10mRunCurrencyBaseline Baseline;
	TestTrue(TEXT("Initial capture succeeds"), Baseline.TryCapture(FirstRun, 70, 6, 45));
	const auto Original = Baseline;
	TestTrue(TEXT("Identical capture is idempotent"), Baseline.TryCapture(FirstRun, 70, 6, 45));
	TestFalse(TEXT("In-run gain cannot replace entry checkpoint"), Baseline.TryCapture(FirstRun, 900, 6, 45));
	TestFalse(TEXT("In-run spending cannot replace entry checkpoint"), Baseline.TryCapture(FirstRun, 10, 2, 1));
	TestFalse(TEXT("Another run cannot replace captured checkpoint"), Baseline.TryCapture(SecondRun, 70, 6, 45));
	TestTrue(TEXT("Rejected recaptures preserve all baseline fields"), Same(Original, Baseline));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyInvalidCaptureTest,
	"Rogue10m.RunCurrencyBaseline.InvalidCaptureIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyInvalidCaptureTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	FRogue10mRunCurrencyBaseline Baseline;
	const auto Empty = Baseline;
	TestFalse(TEXT("Invalid run identity rejected"), Baseline.TryCapture(FGuid(), 1, 2, 3));
	TestFalse(TEXT("Negative gold rejected"), Baseline.TryCapture(FirstRun, -1, 2, 3));
	TestFalse(TEXT("Negative crystals rejected"), Baseline.TryCapture(FirstRun, 1, MIN_int32, 3));
	TestFalse(TEXT("Negative lobby budget rejected"), Baseline.TryCapture(FirstRun, 1, 2, -1));
	TestTrue(TEXT("Invalid captures never mark or partially fill the checkpoint"), Same(Empty, Baseline));
	TestTrue(TEXT("A valid capture remains possible afterward"), Baseline.TryCapture(FirstRun, 1, 2, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyInvalidResolveTest,
	"Rogue10m.RunCurrencyBaseline.InvalidResolutionIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyInvalidResolveTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	FRogue10mRunCurrencyBaseline Valid;
	Valid.TryCapture(FirstRun, 70, 6, 45);
	TArray<FRogue10mRunCurrencyBaseline> Invalid;
	Invalid.AddDefaulted();
	Invalid.Add(Valid); Invalid.Last().RunId.Invalidate();
	Invalid.Add(Valid); Invalid.Last().bCaptured = false;
	Invalid.Add(Valid); Invalid.Last().Gold = -1;
	Invalid.Add(Valid); Invalid.Last().Crystals = -1;
	Invalid.Add(Valid); Invalid.Last().LobbyEarnedPoints = MIN_int32;
	for (const auto& Baseline : Invalid)
	{
		int32 Gold = 91, Crystals = 92, Lobby = 93;
		TestFalse(TEXT("Missing or malformed saved checkpoint rejected"), Baseline.TryResolve(FirstRun, Gold, Crystals, Lobby));
		TestEqual(TEXT("Gold output remains unchanged"), Gold, 91);
		TestEqual(TEXT("Crystal output remains unchanged"), Crystals, 92);
		TestEqual(TEXT("Lobby output remains unchanged"), Lobby, 93);
	}
	for (const FGuid Identity : {FGuid(), SecondRun})
	{
		int32 Gold = 91, Crystals = 92, Lobby = 93;
		TestFalse(TEXT("Invalid or different current run rejected"), Valid.TryResolve(Identity, Gold, Crystals, Lobby));
		TestTrue(TEXT("Identity failure does not write any output"), Gold == 91 && Crystals == 92 && Lobby == 93);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyRepeatTest,
	"Rogue10m.RunCurrencyBaseline.RepeatedRollbackAndRunIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyRepeatTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	FRogue10mRunCurrencyBaseline Baseline;
	Baseline.TryCapture(FirstRun, 70, 6, 45);
	int32 Gold = 999, Crystals = 999, Lobby = 999;
	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		TestTrue(TEXT("A repeated request resolves successfully"), Baseline.TryResolve(FirstRun, Gold, Crystals, Lobby));
		TestTrue(TEXT("It restores absolute balances without adding a reward"), Gold == 70 && Crystals == 6 && Lobby == 45);
	}
	FRogue10mRunCurrencyBaseline NextRun;
	TestTrue(TEXT("New checkpoint captures the next run's own paid costs"), NextRun.TryCapture(SecondRun, 40, 2, 20));
	TestFalse(TEXT("Previous run checkpoint cannot affect next run"), Baseline.TryResolve(SecondRun, Gold, Crystals, Lobby));
	TestTrue(TEXT("Next run resolves its own checkpoint"), NextRun.TryResolve(SecondRun, Gold, Crystals, Lobby));
	TestTrue(TEXT("Next run does not inherit prior balances"), Gold == 40 && Crystals == 2 && Lobby == 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mCurrencyBoundaryTest,
	"Rogue10m.RunCurrencyBaseline.ZeroAndMaximumBalances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mCurrencyBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace Rogue10mCurrencyBaselineTests;
	for (const int32 Value : {0, MAX_int32})
	{
		FRogue10mRunCurrencyBaseline Baseline;
		TestTrue(TEXT("Legal integer boundary captured without arithmetic"), Baseline.TryCapture(FirstRun, Value, Value, Value));
		int32 Gold = -1, Crystals = -1, Lobby = -1;
		TestTrue(TEXT("Boundary resolved exactly"), Baseline.TryResolve(FirstRun, Gold, Crystals, Lobby));
		TestTrue(TEXT("No overflow or underflow on resolution"), Gold == Value && Crystals == Value && Lobby == Value);
	}
	return true;
}
#endif
