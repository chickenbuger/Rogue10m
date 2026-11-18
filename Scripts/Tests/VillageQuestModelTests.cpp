#include "../../Source/Rogue10m/World/Rogue10mVillageQuestModel.h"

#include <cstdio>
#include <limits>

using namespace Rogue10mVillageQuest;

namespace
{
int Checks = 0;
int Failures = 0;

void Check(bool bCondition, const char* Description)
{
	++Checks;
	if (!bCondition)
	{
		++Failures;
		std::printf("FAIL %s\n", Description);
	}
}

void CheckFresh(const FQuestModel& Model)
{
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		Check(Model.GetState(Quest) == EState::Available, "fresh quest is available without another village prerequisite");
		Check(Model.GetProgress(Quest) == 0, "fresh quest has no observed objectives");
		Check(!Model.IsClaimPending(Quest), "fresh quest has no pending reward");
		Check(!Model.HasObserved(Quest, 0) && !Model.HasObserved(Quest, 1), "fresh quest exposes no completed objective checkbox");
	}
}

void ObjectiveInspection()
{
	FQuestModel Model;
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		Check(Model.Observe(Quest, 1), "inspector records second objective first");
		const FQuestModel& ReadOnly = Model;
		Check(!ReadOnly.HasObserved(Quest, 0) && ReadOnly.HasObserved(Quest, 1), "checkboxes identify the remaining distinct objective");
		Check(ReadOnly.GetState(Quest) == EState::Available, "inspecting observed facts cannot accept quest");
		Check(Model.Accept(Quest) && Model.GetState(Quest) == EState::Active, "partial preacceptance facts retain active state");
		Check(Model.Observe(Quest, 0), "inspector records missing first objective");
		Check(ReadOnly.HasObserved(Quest, 0) && ReadOnly.HasObserved(Quest, 1), "ready quest displays both objective checkboxes");
		Check(Model.BeginClaim(Quest), "inspector reserves ready reward");
		Check(ReadOnly.HasObserved(Quest, 0) && ReadOnly.HasObserved(Quest, 1), "pending reward retains objective history");
		Model.FinishClaim(Quest, false);
		Check(ReadOnly.HasObserved(Quest, 0) && ReadOnly.HasObserved(Quest, 1), "failed reward retains objective history");
		Check(Model.BeginClaim(Quest), "inspector retries failed reward"); Model.FinishClaim(Quest, true);
		Check(ReadOnly.HasObserved(Quest, 0) && ReadOnly.HasObserved(Quest, 1), "claimed reward retains objective history");
	}
	const int Invalid[] = {-1, QuestCount, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
	const int InvalidGoals[] = {-1, ObjectiveCount, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
	for (int Quest : Invalid)
	{
		Check(!Model.HasObserved(Quest, 0) && !Model.HasObserved(Quest, 1), "invalid quest cannot expose another quest's fact");
	}
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		for (int Goal : InvalidGoals)
		{
			Check(!Model.HasObserved(Quest, Goal), "invalid objective query returns false without touching claimed quest");
		}
		Check(Model.GetState(Quest) == EState::Claimed && Model.GetProgress(Quest) == 2, "invalid inspector queries preserve receipts and counts");
	}
	Model.Reset(); CheckFresh(Model);
}

void PreAcceptanceFacts()
{
	// Both objective orders, for every village. Discovery cannot silently accept a quest.
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		for (int FirstObjective = 0; FirstObjective < ObjectiveCount; ++FirstObjective)
		{
			FQuestModel Model;
			const int SecondObjective = 1 - FirstObjective;
			Check(Model.Observe(Quest, FirstObjective), "first discovery records before acceptance");
			Check(Model.GetProgress(Quest) == 1, "partial preacceptance discovery remains visible");
			Check(Model.GetState(Quest) == EState::Available, "partial discovery leaves quest available");
			Check(!Model.BeginClaim(Quest), "unaccepted partial quest cannot grant a reward");
			Check(Model.Observe(Quest, SecondObjective), "second discovery records before acceptance");
			Check(Model.GetProgress(Quest) == ObjectiveCount, "both preacceptance facts retained");
			Check(Model.GetState(Quest) == EState::Available, "completed discovery still requires explicit acceptance");
			Check(!Model.BeginClaim(Quest), "unaccepted completed quest cannot grant a reward");
			Check(Model.Accept(Quest), "accepting a previously discovered quest succeeds");
			Check(Model.GetState(Quest) == EState::Ready, "preacceptance facts make accepted quest immediately ready");
			Check(!Model.Accept(Quest), "ready quest cannot be accepted again");
			Check(Model.BeginClaim(Quest), "ready preobserved quest can reserve reward");
			Model.FinishClaim(Quest, true);
			Check(Model.GetState(Quest) == EState::Claimed, "preobserved accepted quest rewards normally");
		}
	}
}

void ReverseOrderAndRevisits()
{
	FQuestModel Model;
	// Village five can complete while villages one through four remain untouched.
	for (int Quest = QuestCount - 1; Quest >= 0; --Quest)
	{
		Check(Model.Accept(Quest), "reverse-order village can accept without predecessor");
		Check(Model.GetState(Quest) == EState::Active, "newly accepted undiscovered quest is active");
		Check(!Model.Accept(Quest), "active quest rejects duplicate acceptance");
		Check(!Model.BeginClaim(Quest), "active quest cannot grant early");
		Check(Model.Observe(Quest, 1), "second objective may happen first");
		Check(Model.GetState(Quest) == EState::Active, "one objective does not complete quest");
		for (int Revisit = 0; Revisit < 3; ++Revisit)
		{
			Check(!Model.Observe(Quest, 1), "revisiting one objective is deduplicated");
			Check(Model.GetProgress(Quest) == 1, "revisits cannot substitute for other objective");
		}
		Check(!Model.BeginClaim(Quest), "repeated one-objective visits cannot grant early");
		Check(Model.Observe(Quest, 0), "missing first objective completes reverse-order quest");
		Check(Model.GetState(Quest) == EState::Ready, "both distinct objectives complete active quest");
		Check(Model.BeginClaim(Quest), "completed reverse-order quest reserves reward");
		Model.FinishClaim(Quest, true);
		Check(!Model.BeginClaim(Quest), "completed reward rejects a second reservation");
		Check(!Model.Accept(Quest), "claimed quest cannot be accepted again");
		Check(!Model.Observe(Quest, 0) && !Model.Observe(Quest, 1), "claimed quest revisit facts remain deduplicated");
		Check(Model.GetProgress(Quest) == ObjectiveCount, "claimed quest retains completed facts");
		for (int Skipped = 0; Skipped < Quest; ++Skipped)
		{
			Check(Model.GetState(Skipped) == EState::Available && Model.GetProgress(Skipped) == 0,
				"completing later village does not accept or advance skipped village");
		}
	}
}

void SkippingAndPartialPreAcceptance()
{
	FQuestModel Model;
	Check(Model.Accept(4), "last village can be accepted first");
	Check(Model.Observe(4, 0), "last village can advance independently");
	Check(Model.Observe(1, 1), "discovered skipped village objective is remembered");
	Check(Model.Accept(1), "partly discovered skipped village accepts normally");
	Check(Model.GetState(1) == EState::Active && Model.GetProgress(1) == 1,
		"partly discovered quest accepts with retained partial progress");
	Check(Model.Observe(1, 0), "returning supplies missing objective");
	Check(Model.GetState(1) == EState::Ready, "revisited earlier village becomes ready");
	Check(Model.GetState(4) == EState::Active && Model.GetProgress(4) == 1,
		"earlier village completion does not overwrite later village partial progress");
	const int UnvisitedQuests[] = {0, 2, 3};
	for (int Quest : UnvisitedQuests)
	{
		Check(Model.GetState(Quest) == EState::Available && Model.GetProgress(Quest) == 0,
			"unvisited optional village remains available");
	}
}

void RewardFailureAndRecursiveClaims()
{
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		FQuestModel Model;
		Model.FinishClaim(Quest, true);
		Check(Model.GetState(Quest) == EState::Available, "finish without reservation cannot grant available quest");
		Check(Model.Accept(Quest), "reward test accepts quest");
		Model.FinishClaim(Quest, true);
		Check(Model.GetState(Quest) == EState::Active, "finish without reservation cannot bypass objectives");
		Check(Model.Observe(Quest, 0) && Model.Observe(Quest, 1), "reward test records distinct objectives");
		Model.FinishClaim(Quest, true);
		Check(Model.GetState(Quest) == EState::Ready, "unreserved finish cannot mark ready quest claimed");
		Check(Model.BeginClaim(Quest), "first reward attempt obtains reservation");
		Check(Model.IsClaimPending(Quest), "reward callback sees pending reservation");
		Check(Model.GetState(Quest) == EState::Ready, "pending reservation preserves ready display state");
		Check(!Model.BeginClaim(Quest), "recursive reward attempt is denied while external grant runs");
		Check(!Model.Accept(Quest), "pending quest cannot be accepted again");
		Check(!Model.Observe(Quest, 0), "pending quest revisit remains a duplicate");
		Model.FinishClaim(Quest, false);
		Check(!Model.IsClaimPending(Quest), "failed grant releases reservation");
		Check(Model.GetState(Quest) == EState::Ready && Model.GetProgress(Quest) == ObjectiveCount,
			"failed grant keeps objectives and readiness for revisit retry");
		int Grants = 0;
		const auto GrantWithReentrantCallback = [&Model, Quest, &Grants]()
		{
			if (!Model.BeginClaim(Quest)) { return; }
			++Grants;
			if (Model.BeginClaim(Quest)) { ++Grants; }
			Model.FinishClaim(Quest, true);
		};
		GrantWithReentrantCallback();
		GrantWithReentrantCallback();
		Check(Grants == 1, "external reward and its reentrant callback grant exactly once across revisit");
		Check(Model.GetState(Quest) == EState::Claimed && !Model.IsClaimPending(Quest),
			"successful retry finalizes receipt and clears pending state");
		Model.FinishClaim(Quest, false);
		Model.FinishClaim(Quest, true);
		Check(Model.GetState(Quest) == EState::Claimed, "stale finish cannot undo or regrant claimed reward");
	}
}

void InvalidInputs()
{
	FQuestModel Model;
	const int InvalidQuestIndices[] = {-1, QuestCount, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
	for (int Quest : InvalidQuestIndices)
	{
		Check(Model.GetState(Quest) == EState::Unavailable, "invalid quest has unavailable state");
		Check(Model.GetProgress(Quest) == 0, "invalid quest has zero progress");
		Check(!Model.Accept(Quest), "invalid quest cannot accept");
		Check(!Model.Observe(Quest, 0) && !Model.Observe(Quest, 1), "invalid quest cannot observe");
		Check(!Model.BeginClaim(Quest) && !Model.IsClaimPending(Quest), "invalid quest cannot reserve a reward");
		Model.FinishClaim(Quest, true);
		Model.FinishClaim(Quest, false);
		Check(Model.GetState(Quest) == EState::Unavailable, "invalid finish has no effect");
	}
	const int InvalidObjectiveIndices[] = {-1, ObjectiveCount, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		for (int Objective : InvalidObjectiveIndices)
		{
			Check(!Model.Observe(Quest, Objective), "invalid objective is rejected for every village");
		}
	}
	CheckFresh(Model);
}

void ResetEveryState()
{
	FQuestModel Model;
	// Available with facts, Active, Ready, Claimed, and a pending reward coexist.
	Model.Observe(0, 1);
	Model.Accept(1);
	Model.Observe(1, 0);
	for (int Quest = 2; Quest < QuestCount; ++Quest)
	{
		Model.Accept(Quest);
		Model.Observe(Quest, 0);
		Model.Observe(Quest, 1);
	}
	Model.BeginClaim(3);
	Model.FinishClaim(3, true);
	Model.BeginClaim(4);
	Check(Model.GetState(0) == EState::Available && Model.GetProgress(0) == 1, "reset fixture includes preacceptance facts");
	Check(Model.GetState(1) == EState::Active, "reset fixture includes active quest");
	Check(Model.GetState(2) == EState::Ready, "reset fixture includes ready quest");
	Check(Model.GetState(3) == EState::Claimed, "reset fixture includes claimed receipt");
	Check(Model.IsClaimPending(4), "reset fixture includes pending reward");
	Model.Reset();
	CheckFresh(Model);
	Model.FinishClaim(4, true);
	Check(Model.GetState(4) == EState::Available, "old pending completion after reset cannot claim fresh quest");
	Model.Reset();
	CheckFresh(Model);
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		Check(Model.Accept(Quest), "reset permits acceptance in next run");
		Check(Model.GetState(Quest) == EState::Active, "reset leaves no objective facts from previous run");
	}
}
}

int main()
{
	FQuestModel Model;
	CheckFresh(Model);
	PreAcceptanceFacts();
	ReverseOrderAndRevisits();
	SkippingAndPartialPreAcceptance();
	RewardFailureAndRecursiveClaims();
	InvalidInputs();
	ResetEveryState();
	ObjectiveInspection();
	std::printf("RESULT=VILLAGE_QUEST_MODEL_%s checks=%d failures=%d scenarios=8\n", Failures ? "FAILED" : "PASSED", Checks, Failures);
	return Failures ? 1 : 0;
}
