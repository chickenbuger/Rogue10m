// Standalone quest state tests. No Unreal, UBT, world, save, or reward mutation.
#include "Rogue10mVillageQuestModel.h"

#include <cstdint>
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
	using Rogue10mVillageQuest::EStatus;
	using Rogue10mVillageQuest::FQuestModel;
	static_assert(FQuestModel::MaxObjectives == 32, "Authored objective capacity changed; review boundary tests.");
	FQuestModel Empty;
	Check(!Empty.IsConfigured() && Empty.GetObjectiveCount() == 0, "fresh model has no implicit quest definition");
	Check(Empty.GetStatus() == EStatus::Unaccepted && Empty.GetCollectedCount() == 0, "fresh model has no acceptance/facts");
	Check(!Empty.HasObjective(0) && !Empty.HasObjective(std::numeric_limits<unsigned>::max()), "unconfigured objective queries are safely false");
	Check(!Empty.Accept(), "unconfigured quest cannot be accepted");
	Check(!Empty.RecordObjective(0), "unconfigured quest cannot collect facts");
	Check(Empty.TryBeginClaim() == 0, "unconfigured quest cannot claim");
	Check(!Empty.FinishClaim(1, true), "unconfigured quest cannot finish a fabricated ticket");
	Check(!Empty.Configure(0) && !Empty.IsConfigured(), "zero-objective definition rejected");
	Check(!Empty.Configure(33) && !Empty.IsConfigured(), "33-objective definition rejected");
	Check(!Empty.Configure(std::numeric_limits<unsigned>::max()), "huge-objective definition rejected before shifts");
	Check(Empty.GetObjectiveCount() == 0 && Empty.GetStatus() == EStatus::Unaccepted, "invalid initial configuration does not mutate state");

	FQuestModel Retroactive;
	Check(Retroactive.Configure(2), "valid two-objective definition configured");
	Check(Retroactive.RecordObjective(1), "unaccepted quest can remember an actual collection fact");
	Check(Retroactive.HasObjective(1) && !Retroactive.HasObjective(0) && !Retroactive.HasObjective(2),
		"objective query distinguishes exact pre-accept fact from missing/out-of-range facts");
	Check(!Retroactive.RecordObjective(1) && Retroactive.GetCollectedCount() == 1, "duplicate collection is idempotent");
	Check(!Retroactive.RecordObjective(2) && !Retroactive.RecordObjective(std::numeric_limits<unsigned>::max()), "out-of-definition facts rejected");
	Check(Retroactive.GetStatus() == EStatus::Unaccepted && Retroactive.TryBeginClaim() == 0, "collection without acceptance cannot claim");
	Check(Retroactive.RecordObjective(0) && Retroactive.GetCollectedCount() == 2, "all pre-accept facts preserved");
	Check(Retroactive.GetStatus() == EStatus::Unaccepted && Retroactive.TryBeginClaim() == 0, "fully collected unaccepted quest remains optional");
	Check(Retroactive.Accept() && Retroactive.GetStatus() == EStatus::Ready, "acceptance retroactively recognizes collected facts");
	Check(!Retroactive.Accept() && Retroactive.GetCollectedCount() == 2, "duplicate acceptance does not reset ready progress");

	FQuestModel Partial;
	Check(Partial.Configure(3) && Partial.Accept(), "accepted partial quest starts");
	Check(Partial.GetStatus() == EStatus::Accepted && Partial.TryBeginClaim() == 0, "acceptance alone cannot claim");
	Check(Partial.RecordObjective(0) && Partial.RecordObjective(2), "two distinct objectives collected");
	Check(Partial.GetStatus() == EStatus::Accepted && Partial.GetCollectedCount() == 2, "partial collection stays accepted");
	Check(Partial.HasObjective(0) && !Partial.HasObjective(1) && Partial.HasObjective(2), "accepted objective UI queries do not mark unrelated facts complete");
	Check(Partial.TryBeginClaim() == 0, "partial collection cannot obtain a reward ticket");
	Check(!Partial.RecordObjective(2) && Partial.GetCollectedCount() == 2, "duplicate partial fact cannot finish quest");
	Check(Partial.RecordObjective(1) && Partial.GetStatus() == EStatus::Ready, "last distinct objective makes quest ready");
	Check(Partial.HasObjective(0) && Partial.HasObjective(1) && Partial.HasObjective(2), "ready objective queries preserve all distinct facts");
	Check(!Partial.Configure(0) && !Partial.Configure(33), "invalid reconfiguration rejected");
	Check(Partial.GetObjectiveCount() == 3 && Partial.GetCollectedCount() == 3 && Partial.GetStatus() == EStatus::Ready,
		"invalid reconfiguration preserves existing ready state");

	const std::uint64_t FirstTicket = Partial.TryBeginClaim();
	Check(FirstTicket != 0 && Partial.GetStatus() == EStatus::Claiming, "ready quest starts one in-flight claim");
	Check(Partial.TryBeginClaim() == 0, "reentrant begin-claim is blocked");
	Check(!Partial.Accept() && !Partial.RecordObjective(0), "accept/fact callbacks cannot mutate an in-flight claim");
	Check(!Partial.Configure(1) && Partial.GetObjectiveCount() == 3, "reentrant configuration cannot replace an in-flight claim");
	Check(!Partial.FinishClaim(0, true) && !Partial.FinishClaim(FirstTicket + 1, true), "zero/foreign ticket cannot commit reward");
	Check(Partial.GetStatus() == EStatus::Claiming, "invalid completion preserves the valid in-flight ticket");
	Check(Partial.FinishClaim(FirstTicket, false), "reward failure callback consumes its valid ticket");
	Check(Partial.GetStatus() == EStatus::Ready && Partial.GetCollectedCount() == 3, "reward failure leaves ready facts available for retry");
	Check(!Partial.FinishClaim(FirstTicket, true), "late success from failed attempt is rejected");
	const std::uint64_t RetryTicket = Partial.TryBeginClaim();
	Check(RetryTicket != 0 && RetryTicket != FirstTicket, "reward retry gets a fresh transaction ticket");
	Check(!Partial.FinishClaim(FirstTicket, true) && Partial.GetStatus() == EStatus::Claiming, "stale callback cannot commit the retry");
	Check(Partial.FinishClaim(RetryTicket, true), "successful reward callback commits once");
	Check(Partial.GetStatus() == EStatus::Claimed && Partial.GetCollectedCount() == 3, "successful claim retains completed objective facts");
	Check(Partial.TryBeginClaim() == 0 && !Partial.FinishClaim(RetryTicket, true), "repeated claim/completion cannot repeat committed reward");
	Check(!Partial.FinishClaim(RetryTicket, false) && Partial.GetStatus() == EStatus::Claimed, "late failure cannot reopen committed reward");
	Check(!Partial.Accept() && !Partial.RecordObjective(0), "completed quest cannot restart by reaccepting or duplicate collection");

	Partial.Reset();
	Check(Partial.IsConfigured() && Partial.GetObjectiveCount() == 3, "new-run reset retains the authored definition");
	Check(Partial.GetStatus() == EStatus::Unaccepted && Partial.GetCollectedCount() == 0, "new-run reset clears acceptance/facts/claim receipt");
	Check(!Partial.HasObjective(0) && !Partial.HasObjective(1) && !Partial.HasObjective(2), "new-run reset clears every objective UI query");
	Check(!Partial.FinishClaim(RetryTicket, true), "previous-run receipt cannot mutate reset quest");
	Check(Partial.TryBeginClaim() == 0, "reset quest cannot claim without new acceptance/facts");

	FQuestModel Cancel;
	Check(Cancel.Configure(1) && Cancel.Accept() && Cancel.RecordObjective(0), "one-objective minimum reaches ready state");
	const std::uint64_t CancelledTicket = Cancel.TryBeginClaim();
	Check(CancelledTicket != 0, "claim exists before run reset");
	Cancel.Reset();
	Check(Cancel.GetStatus() == EStatus::Unaccepted && !Cancel.FinishClaim(CancelledTicket, true), "reset cancels an in-flight transaction");
	Check(Cancel.Accept() && Cancel.RecordObjective(0), "new run can complete same authored objective independently");
	const std::uint64_t NewRunTicket = Cancel.TryBeginClaim();
	Check(NewRunTicket != 0 && NewRunTicket != CancelledTicket, "run reset never reuses a stale transaction ticket");
	Check(!Cancel.FinishClaim(CancelledTicket, true) && Cancel.GetStatus() == EStatus::Claiming, "old-run callback cannot claim new-run reward");
	Check(Cancel.FinishClaim(NewRunTicket, true), "new-run transaction can commit normally");

	FQuestModel Maximum;
	Check(Maximum.Configure(32) && Maximum.Accept(), "maximum objective definition accepted");
	bool ReverseFacts = true;
	for (unsigned Index = 32; Index > 0; --Index)
	{
		ReverseFacts = Maximum.RecordObjective(Index - 1) && ReverseFacts;
		ReverseFacts = Maximum.GetCollectedCount() == 33 - Index && ReverseFacts;
		if (Index > 1) { ReverseFacts = Maximum.TryBeginClaim() == 0 && ReverseFacts; }
	}
	Check(ReverseFacts && Maximum.GetCollectedCount() == 32 && Maximum.GetStatus() == EStatus::Ready,
		"all32 objective bits work in reverse order without early completion");
	Check(!Maximum.RecordObjective(31) && !Maximum.RecordObjective(32), "highest duplicate and first out-of-range bit are rejected");
	Check(Maximum.HasObjective(31) && !Maximum.HasObjective(32) && !Maximum.HasObjective(std::numeric_limits<unsigned>::max()),
		"objective query supports bit31 and rejects32plus without shifting out of range");
	const auto MaximumTicket = Maximum.TryBeginClaim();
	Check(MaximumTicket != 0 && Maximum.FinishClaim(MaximumTicket, true), "32-objective quest claims successfully");

	bool AllCounts = true;
	for (unsigned Count = 1; Count <= 32; ++Count)
	{
		FQuestModel Sized;
		AllCounts = Sized.Configure(Count) && Sized.Accept() && AllCounts;
		for (unsigned Index = 0; Index < Count; ++Index)
		{
			AllCounts = Sized.RecordObjective(Index) && Sized.HasObjective(Index) && AllCounts;
			if (Index + 1 < Count) { AllCounts = Sized.TryBeginClaim() == 0 && AllCounts; }
		}
		const auto Ticket = Sized.TryBeginClaim();
		AllCounts = Ticket != 0 && Sized.FinishClaim(Ticket, true) && Sized.GetStatus() == EStatus::Claimed && AllCounts;
	}
	Check(AllCounts, "every supported objective count1through32 preserves partial/ready/claim transitions");

	constexpr unsigned StressIterations = 20000;
	FQuestModel Stress;
	Check(Stress.Configure(1), "stress definition configured");
	bool StressOkay = true;
	std::uint64_t PreviousTicket = 0;
	unsigned CommittedRewards = 0;
	for (unsigned Iteration = 0; Iteration < StressIterations; ++Iteration)
	{
		Stress.Reset();
		StressOkay = Stress.Accept() && Stress.RecordObjective(0) && StressOkay;
		const auto Ticket = Stress.TryBeginClaim();
		StressOkay = Ticket != 0 && Ticket != PreviousTicket && Stress.TryBeginClaim() == 0 && StressOkay;
		if (PreviousTicket != 0) { StressOkay = !Stress.FinishClaim(PreviousTicket, true) && StressOkay; }
		if (Stress.FinishClaim(Ticket, true)) { ++CommittedRewards; } else { StressOkay = false; }
		StressOkay = !Stress.FinishClaim(Ticket, true) && Stress.TryBeginClaim() == 0 && StressOkay;
		PreviousTicket = Ticket;
	}
	Check(StressOkay && CommittedRewards == StressIterations, "20k resets/reentrant starts/stale finishes commit exactly once per run");
	Check(Empty.GetObjectiveCount() == 0 && Retroactive.GetStatus() == EStatus::Ready, "independent quest models never share state");

	std::cout << "{\"schema_version\":1,\"status\":\"" << (Failures ? "failed" : "passed")
		<< "\",\"checks\":" << Checks << ",\"failures\":" << Failures
		<< ",\"stress_iterations\":" << StressIterations << ",\"stress_commits\":" << CommittedRewards
		<< ",\"objective_counts_exercised\":32,\"unreal_editor\":\"not_run\",\"ubt\":\"not_run\"}\n";
	return Failures ? 1 : 0;
}
