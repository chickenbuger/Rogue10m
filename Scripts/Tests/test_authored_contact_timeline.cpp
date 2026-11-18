#include "../../Source/Rogue10m/Components/Rogue10mAuthoredContactTimeline.h"
#include <cstdio>

#define CHECK(Condition) do { ++Checks; if (!(Condition)) { std::printf("FAIL line %d: %s\n", __LINE__, #Condition); return 1; } } while (false)
static bool Near(double A, double B) { return std::fabs(A - B) < 0.000001; }

int main()
{
	int Checks = 0, Scenarios = 0;
	using FTimeline = FRogue10mAuthoredContactTimeline;
	const float Times[] = {.2f, .5f, .95f};
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();
	{
		++Scenarios;
		CHECK(!FTimeline::Validate(nullptr, 3, 1, 2, 0)); CHECK(!FTimeline::Validate(Times, 0, 1, 2, 0));
		CHECK(!FTimeline::Validate(Times, 65, 1, 2, 0)); CHECK(!FTimeline::Validate(Times, -1, 1, 2, 0));
	}
	{
		++Scenarios;
		CHECK(!FTimeline::Validate(Times, 3, 0, 2, 0)); CHECK(!FTimeline::Validate(Times, 3, -1, 2, 0));
		CHECK(!FTimeline::Validate(Times, 3, Inf, 2, 0)); CHECK(!FTimeline::Validate(Times, 3, NaN, 2, 0));
		CHECK(!FTimeline::Validate(Times, 3, 1, 0, 0)); CHECK(!FTimeline::Validate(Times, 3, 1, Inf, 0));
		CHECK(!FTimeline::Validate(Times, 3, 1, 2, -1)); CHECK(!FTimeline::Validate(Times, 3, 1, 2, Inf));
	}
	{
		++Scenarios;
		const float Negative[] = {-.1f, .5f}; const float Duplicate[] = {.1f, .1f};
		const float Backward[] = {.5f, .1f}; const float Bad[] = {.1f, NaN};
		const float Infinite[] = {.1f, std::numeric_limits<float>::infinity()};
		CHECK(!FTimeline::Validate(Negative, 2, 1, 2, 0)); CHECK(!FTimeline::Validate(Duplicate, 2, 1, 2, 0));
		CHECK(!FTimeline::Validate(Backward, 2, 1, 2, 0)); CHECK(!FTimeline::Validate(Bad, 2, 1, 2, 0));
		CHECK(!FTimeline::Validate(Infinite, 2, 1, 2, 0));
	}
	{
		++Scenarios;
		const float AtEnd[] = {.1f, 2.f}; const float Beyond[] = {.1f, 2.1f};
		CHECK(!FTimeline::Validate(AtEnd, 2, 1, 2, 0)); CHECK(!FTimeline::Validate(Beyond, 2, 1, 2, 0));
		CHECK(!FTimeline::Validate(Times, 3, .01, 2, 0));
		const float Limit[] = {15.f}; CHECK(FTimeline::Validate(Limit, 1, .5, 16, 0));
		CHECK(!FTimeline::Validate(Limit, 1, .499, 16, 0));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 10)); CHECK(T.TotalCount() == 3);
		CHECK(Near(T.SecondsUntilNext(10), .2)); CHECK(!T.Poll(10.1).bDue);
		CHECK(T.Poll(10.0 + static_cast<double>(Times[0])).ContactNumber == 1); CHECK(Near(T.SecondsUntilNext(10.3), .2));
		CHECK(T.Poll(10.5).ContactNumber == 2); CHECK(Near(T.SecondsUntilNext(10.6), .35));
		CHECK(T.Poll(10.0 + static_cast<double>(Times[2])).ContactNumber == 3); CHECK(!T.IsActive()); CHECK(T.CompletedCount() == 3);
		CHECK(!T.Poll(12).bDue);
	}
	{
		++Scenarios;
		FTimeline T; float Accepted[] = {.2f, .5f, .95f}; double AcceptedRate = 2.0;
		CHECK(T.Begin(Accepted, 3, AcceptedRate, 2, 0));
		Accepted[1] = 1.7f; AcceptedRate = 10.0; // The accepted copy does not observe later asset/stat mutations.
		CHECK(AcceptedRate == 10.0); CHECK(Accepted[1] == 1.7f);
		CHECK(T.Poll(.10001).ContactNumber == 1); CHECK(Near(T.SecondsUntilNext(.15), .1));
		CHECK(T.Poll(.25).ContactNumber == 2); CHECK(T.Poll(.47501).ContactNumber == 3);
	}
	{
		++Scenarios;
		FTimeline T; const float Zero[] = {0.f, .3f, .7f};
		CHECK(T.Begin(Zero, 3, 1, 2, 3)); CHECK(Near(T.SecondsUntilNext(3), 0));
		CHECK(T.Poll(3).ContactNumber == 1); CHECK(!T.Poll(3).bDue); CHECK(T.CompletedCount() == 1);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0));
		CHECK(T.Poll(1.0).ContactNumber == 1); CHECK(T.CompletedCount() == 1); CHECK(T.IsActive());
		CHECK(Near(T.SecondsUntilNext(1.0), 0)); CHECK(T.Poll(1.0).ContactNumber == 2);
		CHECK(T.Poll(1.0).ContactNumber == 3); CHECK(!T.Poll(1.0).bDue);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 4)); CHECK(!T.Poll(4.1).bDue);
		CHECK(!T.Poll(4.05).bValid); CHECK(!T.IsActive()); CHECK(!T.Poll(6).bDue);
		CHECK(T.Begin(Times, 3, 1, 2, 8)); CHECK(T.Poll(8.21).ContactNumber == 1);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0));
		CHECK(!T.Poll(.199).bDue); CHECK(T.CompletedCount() == 0); CHECK(T.Poll(.20001).bDue);
		CHECK(!T.Poll(.499).bDue); CHECK(T.CompletedCount() == 1); CHECK(T.Poll(.5).bDue);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); T.Cancel(); CHECK(!T.Poll(1).bDue);
		CHECK(T.Begin(Times, 3, 1, 2, 2)); CHECK(T.Poll(2.21).bDue); T.Cancel();
		CHECK(!T.Poll(3).bDue); CHECK(T.CompletedCount() == 1);
	}
	{
		++Scenarios;
		FTimeline T; float Many[FTimeline::MaxContacts];
		for (int Index = 0; Index < FTimeline::MaxContacts; ++Index) { Many[Index] = .01f * (Index + 1); }
		CHECK(T.Begin(Many, FTimeline::MaxContacts, 1, 2, 100));
		for (int Index = 0; Index < FTimeline::MaxContacts; ++Index)
		{ CHECK(T.Poll(100.0 + static_cast<double>(Many[Index])).ContactNumber == Index + 1); }
		CHECK(!T.IsActive()); CHECK(!T.Poll(102).bDue);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); FTimeline Snapshot = T; T.Cancel();
		CHECK(Snapshot.Poll(.21).ContactNumber == 1); CHECK(!T.Poll(.21).bDue);
		CHECK(Snapshot.Poll(.51).ContactNumber == 2); CHECK(!T.IsActive());
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); CHECK(!T.Begin(Times, 3, 2, 2, 0));
		CHECK(!T.Poll(Inf).bValid); CHECK(!T.IsActive()); CHECK(T.Begin(Times, 3, 2, 2, 1));
		CHECK(T.Poll(2).ContactNumber == 1);
	}
	{
		++Scenarios;
		FTimeline T; const float Exact[] = {.25f}; CHECK(T.Begin(Exact, 1, 1, 2, 100));
		CHECK(!T.Poll(100.25 - 2.0 * FTimeline::ClockTolerance).bDue);
		CHECK(T.Poll(100.25 - .5 * FTimeline::ClockTolerance).ContactNumber == 1);
	}
	{
		++Scenarios;
		CHECK(!FTimeline::Validate(Times, 3, 1, 2, 0, NaN)); CHECK(!FTimeline::Validate(Times, 3, 1, 2, 0, Inf));
		CHECK(!FTimeline::Validate(Times, 3, 1, 2, 0, -1)); CHECK(!FTimeline::Validate(Times, 3, 1, 2, 0, .5));
		CHECK(!FTimeline::Validate(Times, 3, 1, 2, 0, 2.1)); CHECK(!FTimeline::Validate(Times, 3, .04, 2, 0, 1.5));
		CHECK(FTimeline::Validate(Times, 3, 1, 2, 0, Times[2])); CHECK(FTimeline::Validate(Times, 3, 1, 2, 0, 2));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 2, 2, 10, 1.5)); CHECK(Near(T.RecoveryDeadline(), 10.75));
		CHECK(T.Poll(10.11).ContactNumber == 1); CHECK(T.Poll(10.26).ContactNumber == 2); CHECK(T.Poll(10.48).ContactNumber == 3);
		CHECK(!T.IsActive()); CHECK(T.IsRecoveryPending(10.5)); CHECK(T.IsRecoveryPending(10.74)); CHECK(!T.IsRecoveryPending(10.75));
		FTimeline Snapshot = T; T.Cancel(); CHECK(!T.IsRecoveryPending(10.5)); CHECK(Snapshot.IsRecoveryPending(10.5));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 2, 2, 10, 1.5));
		T.Poll(10.11); T.Poll(10.26); T.Poll(10.48);
		CHECK(!T.Begin(Times, 3, 2, 2, 10.55, 1.5)); CHECK(!T.Begin(Times, 3, 2, 2, 10, 1.5));
		CHECK(T.Begin(Times, 3, 2, 2, 10.75, 1.5)); CHECK(T.CompletedCount() == 0);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); const auto A = T.ArmNextContact();
		CHECK(A.Generation != 0 && A.ContactNumber == 1); CHECK(T.ConsumeArm(A.Generation, A.ContactNumber));
		CHECK(!T.ConsumeArm(A.Generation, A.ContactNumber)); CHECK(!T.Poll(.1).bDue);
		const auto B = T.ArmNextContact(); CHECK(B.Generation != A.Generation); CHECK(B.ContactNumber == 1);
		CHECK(!T.ConsumeArm(A.Generation, 1)); CHECK(!T.ConsumeArm(B.Generation, 2));
		CHECK(T.ConsumeArm(B.Generation, 1)); CHECK(T.Poll(.21).ContactNumber == 1);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); const auto A = T.ArmNextContact();
		CHECK(T.ConsumeArm(A.Generation, 1)); CHECK(T.Poll(.21).ContactNumber == 1);
		const auto B = T.ArmNextContact(); CHECK(!T.ConsumeArm(A.Generation, 1)); CHECK(T.CompletedCount() == 1);
		CHECK(T.ConsumeArm(B.Generation, 2)); CHECK(T.Poll(.51).ContactNumber == 2);
		const auto C = T.ArmNextContact(); CHECK(!T.ConsumeArm(C.Generation, 1)); CHECK(T.ConsumeArm(C.Generation, 3));
		CHECK(T.Poll(.96).ContactNumber == 3); CHECK(T.ArmNextContact().Generation == 0); CHECK(!T.ConsumeArm(C.Generation, 3));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0)); const auto Old = T.ArmNextContact(); T.Cancel();
		CHECK(!T.ConsumeArm(Old.Generation, 1)); CHECK(T.Begin(Times, 3, 1, 2, 3)); const auto New = T.ArmNextContact();
		CHECK(New.Generation != Old.Generation); CHECK(!T.ConsumeArm(Old.Generation, 1));
		CHECK(T.ConsumeArm(New.Generation, 1)); CHECK(T.Poll(3.21).ContactNumber == 1);
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0, 1.5)); const auto A = T.ArmNextContact();
		CHECK(T.ConsumeArm(A.Generation, 1)); CHECK(!T.Poll(Inf).bValid); CHECK(!T.IsRecoveryPending(1));
		CHECK(!T.ConsumeArm(A.Generation, 1)); CHECK(T.Begin(Times, 3, 1, 2, 3, 1.5));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0, Times[2]));
		T.Poll(.21); T.Poll(.51); CHECK(T.Poll(static_cast<double>(Times[2])).ContactNumber == 3); CHECK(!T.IsRecoveryPending(.96));
		FTimeline Legacy; CHECK(Legacy.Begin(Times, 3, 1, 2, 0)); CHECK(!Legacy.HasRecoveryOverride());
		Legacy.Poll(.21); Legacy.Poll(.51); Legacy.Poll(.96); CHECK(!Legacy.IsRecoveryPending(1));
	}
	{
		++Scenarios;
		FTimeline T; CHECK(T.Begin(Times, 3, 1, 2, 0, 1.3)); CHECK(T.Poll(.21).ContactNumber == 1);
		CHECK(!T.Poll(1.31).bValid); CHECK(T.CompletedCount() == 1); CHECK(!T.IsActive());
		FTimeline Boundary; CHECK(Boundary.Begin(Times, 3, 1, 2, 0, Times[2]));
		CHECK(Boundary.Poll(.21).ContactNumber == 1); CHECK(Boundary.Poll(.51).ContactNumber == 2);
		CHECK(Boundary.Poll(static_cast<double>(Times[2])).ContactNumber == 3);
	}
	std::printf("authored_contact_timeline: %d scenarios, %d checks passed; Unreal/animation/physics not run\n", Scenarios, Checks);
	return 0;
}
