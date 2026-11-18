// Standalone regression cases: no UE, UBT, generated header, or production save.
#include "Rogue10mFiveVillageProgressModel.h"

#include <iostream>
#include <limits>

namespace
{
int Checks = 0;
int Failures = 0;
void Check(bool Condition, const char* Name)
{
	++Checks;
	if (!Condition)
	{
		++Failures;
		std::cerr << "FAIL " << Name << '\n';
	}
}
}

int main()
{
	using Rogue10mFiveVillage::FProgressModel;
	FProgressModel State;
	Check(State.GetCurrentVillageIndex() == -1, "initial outside village");
	Check(State.GetRevisitCount() == 0, "initial no revisits");
	Check(!State.HasEnteredBossApproach(), "initial boss not observed");
	Check(State.GetFirstBossApproachActiveSeconds() == -1.0, "initial approach sentinel");
	for (int Index = 0; Index < FProgressModel::VillageCount; ++Index)
	{
		Check(State.GetVisitCount(Index) == 0, "initial village not visited");
	}

	Check(State.ObserveVillage(0), "first village entry changes state");
	Check(State.GetVisitCount(0) == 1, "first visit counted once");
	Check(!State.ObserveVillage(0), "remaining in zone idempotent");
	Check(State.GetVisitCount(0) == 1, "polling does not duplicate visit");
	Check(!State.ObserveVillage(-2), "negative invalid village rejected");
	Check(!State.ObserveVillage(5), "upper invalid village rejected");
	Check(State.GetCurrentVillageIndex() == 0 && State.GetVisitCount(0) == 1,
		"invalid observations cannot erase active location");
	Check(State.GetVisitCount(-1) == 0 && State.GetVisitCount(5) == 0,
		"invalid getter indices are bounded");
	Check(State.ObserveVillage(-1), "real leave observed");
	Check(!State.ObserveVillage(-1), "outside polling idempotent");
	Check(State.GetVisitCount(0) == 1, "leave preserves progress");
	Check(State.ObserveVillage(0) && State.GetVisitCount(0) == 2, "return increments visit");
	Check(State.GetRevisitCount() == 1, "first return counted");

	Check(State.ObserveVillage(3), "can skip directly to fourth village");
	Check(State.GetVisitCount(1) == 0 && State.GetVisitCount(2) == 0 && State.GetVisitCount(4) == 0,
		"skipped villages remain unvisited");
	Check(State.ObserveVillage(0) && State.GetVisitCount(0) == 3, "reverse order return allowed");
	Check(State.GetRevisitCount() == 2, "second return counted");
	Check(State.ObserveBossApproach(900.0), "approach recorded without all village visits");
	Check(!State.ObserveBossApproach(901.0), "approach receipt idempotent");
	Check(State.GetFirstBossApproachActiveSeconds() == 900.0, "first approach time preserved");
	Check(State.ObserveVillage(-1), "terminal outside observation changes current zone");
	Check(State.GetVisitCount(0) == 3 && State.HasEnteredBossApproach(),
		"terminal zone clear preserves result observations");

	State.Reset();
	Check(State.GetCurrentVillageIndex() == -1 && State.GetRevisitCount() == 0,
		"next run clears current and revisit count");
	Check(!State.HasEnteredBossApproach() && State.GetFirstBossApproachActiveSeconds() == -1.0,
		"next run clears approach receipt");
	for (int Index = 0; Index < FProgressModel::VillageCount; ++Index)
	{
		Check(State.GetVisitCount(Index) == 0, "next run clears each visit");
	}
	Check(State.ObserveBossApproach(0.0), "boss approach possible with zero villages");
	Check(State.GetVisitCount(0) == 0 && State.GetVisitCount(4) == 0,
		"boss observation never fabricates village progress");

	FProgressModel InvalidTime;
	Check(!InvalidTime.ObserveBossApproach(-0.01), "negative time rejected");
	Check(!InvalidTime.ObserveBossApproach(std::numeric_limits<double>::quiet_NaN()), "NaN time rejected");
	Check(!InvalidTime.ObserveBossApproach(std::numeric_limits<double>::infinity()), "infinite time rejected");
	Check(!InvalidTime.ObserveBossApproach(-std::numeric_limits<double>::infinity()), "negative infinite time rejected");
	Check(!InvalidTime.HasEnteredBossApproach(), "invalid times do not commit receipt");
	Check(InvalidTime.ObserveBossApproach(49.25), "valid fractional active time accepted");
	Check(!InvalidTime.ObserveBossApproach(20.0) && InvalidTime.GetFirstBossApproachActiveSeconds() == 49.25,
		"late callback cannot rewrite first observation");
	Check(State.GetFirstBossApproachActiveSeconds() == 0.0, "separate models do not share state");

	FProgressModel Saturation;
	for (int Visit = 0; Visit < 70000; ++Visit)
	{
		Saturation.ObserveVillage(0);
		Saturation.ObserveVillage(-1);
	}
	Check(Saturation.GetVisitCount(0) == FProgressModel::MaximumVisits,
		"long-session count saturates instead of wrapping");
	Check(Saturation.GetRevisitCount() == 69999, "revisits stay independent of per-village saturation");
	Check(Saturation.GetVisitCount(1) == 0, "saturation cannot bleed into another village");
	Saturation.Reset();
	Check(Saturation.GetVisitCount(0) == 0 && Saturation.GetRevisitCount() == 0,
		"saturated state still resets for a new run");

	std::cout << "{\"status\":\"" << (Failures ? "failed" : "passed")
		<< "\",\"checks\":" << Checks << ",\"failures\":" << Failures
		<< ",\"reentry_stress_iterations\":70000,\"ue_editor\":\"not_run\",\"ubt\":\"not_run\"}\n";
	return Failures ? 1 : 0;
}
