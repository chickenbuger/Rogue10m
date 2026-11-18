#include "../../Source/Rogue10m/World/Rogue10mVillageOptionalClueModel.h"
#include "../../Source/Rogue10m/World/Rogue10mVillageQuestModel.h"

#include <cstdio>
#include <utility>

using namespace Rogue10mVillageQuest;

static_assert(noexcept(std::declval<FOptionalClueModel&>().Reset()), "Reset must be noexcept");
static_assert(noexcept(std::declval<const FOptionalClueModel&>().GetState()), "GetState must be const noexcept");
static_assert(noexcept(std::declval<FOptionalClueModel&>().ObserveSource()), "ObserveSource must be noexcept");
static_assert(noexcept(std::declval<FOptionalClueModel&>().Analyze()), "Analyze must be noexcept");
static_assert(noexcept(std::declval<FOptionalClueModel&>().Report()), "Report must be noexcept");

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

enum class EAction { ObserveSource, Analyze, Report };

bool Apply(FOptionalClueModel& Model, EAction Action)
{
	switch (Action)
	{
	case EAction::ObserveSource: return Model.ObserveSource();
	case EAction::Analyze: return Model.Analyze();
	case EAction::Report: return Model.Report();
	}
	return false;
}

void AdvanceTo(FOptionalClueModel& Model, EOptionalClueState Target)
{
	if (Target == EOptionalClueState::Missing) { return; }
	Check(Model.ObserveSource(), "fixture observes source");
	if (Target == EOptionalClueState::Found) { return; }
	Check(Model.Analyze(), "fixture analyzes found clue");
	if (Target == EOptionalClueState::Analyzed) { return; }
	Check(Model.Report(), "fixture reports analyzed clue");
}

void ValidNarrativeFlow()
{
	FOptionalClueModel Model;
	const FOptionalClueModel& ReadOnly = Model;
	Check(ReadOnly.GetState() == EOptionalClueState::Missing, "fresh run has no optional clue");
	Check(Model.ObserveSource(), "V1 source discovery succeeds");
	Check(ReadOnly.GetState() == EOptionalClueState::Found, "source discovery waits for V4 analysis");
	Check(Model.Analyze(), "V4 analysis succeeds after source discovery");
	Check(ReadOnly.GetState() == EOptionalClueState::Analyzed, "analysis waits for return to V1");
	Check(Model.Report(), "V1 report succeeds after V4 analysis");
	Check(ReadOnly.GetState() == EOptionalClueState::Reported, "valid V1 to V4 to V1 flow finishes narrative");
}

void AllStateActionPairs()
{
	const EOptionalClueState States[] = {
		EOptionalClueState::Missing, EOptionalClueState::Found,
		EOptionalClueState::Analyzed, EOptionalClueState::Reported
	};
	const EAction Actions[] = {EAction::ObserveSource, EAction::Analyze, EAction::Report};
	for (EOptionalClueState State : States)
	{
		for (EAction Action : Actions)
		{
			FOptionalClueModel Model;
			AdvanceTo(Model, State);
			const bool bAllowed =
				(State == EOptionalClueState::Missing && Action == EAction::ObserveSource)
				|| (State == EOptionalClueState::Found && Action == EAction::Analyze)
				|| (State == EOptionalClueState::Analyzed && Action == EAction::Report);
			EOptionalClueState ExpectedState = State;
			if (bAllowed)
			{
				switch (Action)
				{
				case EAction::ObserveSource: ExpectedState = EOptionalClueState::Found; break;
				case EAction::Analyze: ExpectedState = EOptionalClueState::Analyzed; break;
				case EAction::Report: ExpectedState = EOptionalClueState::Reported; break;
				}
			}
			Check(Apply(Model, Action) == bAllowed, "every state/action pair returns whether it advances");
			Check(Model.GetState() == ExpectedState, "invalid ordering or repeat leaves state unchanged");
		}
	}
}

void RepeatedVisitsNeverRegress()
{
	FOptionalClueModel Model;
	for (int Visit = 0; Visit < 4; ++Visit)
	{
		Check(!Model.Analyze() && !Model.Report(), "V4 or V1 dialogue without source rejects early actions");
		Check(Model.GetState() == EOptionalClueState::Missing, "early dialogue cannot manufacture source discovery");
	}
	Check(Model.ObserveSource(), "first real source observation records clue");
	for (int Visit = 0; Visit < 4; ++Visit)
	{
		Check(!Model.ObserveSource() && !Model.Report(), "source revisits and premature report do not advance found clue");
		Check(Model.GetState() == EOptionalClueState::Found, "found clue remains available for analysis");
	}
	Check(Model.Analyze(), "analysis remains possible after source revisits");
	for (int Visit = 0; Visit < 4; ++Visit)
	{
		Check(!Model.ObserveSource() && !Model.Analyze(), "source resampling and duplicate analysis are ignored");
		Check(Model.GetState() == EOptionalClueState::Analyzed, "source resampling cannot regress analyzed clue");
	}
	Check(Model.Report(), "report remains possible after analyzed clue revisits");
	for (int Visit = 0; Visit < 4; ++Visit)
	{
		Check(!Model.ObserveSource() && !Model.Analyze() && !Model.Report(), "finished narrative rejects all repeated actions");
		Check(Model.GetState() == EOptionalClueState::Reported, "source resampling cannot regress reported clue");
	}
}

void ResetEveryState()
{
	const EOptionalClueState States[] = {
		EOptionalClueState::Missing, EOptionalClueState::Found,
		EOptionalClueState::Analyzed, EOptionalClueState::Reported
	};
	for (EOptionalClueState State : States)
	{
		FOptionalClueModel Model;
		AdvanceTo(Model, State);
		Model.Reset();
		Check(Model.GetState() == EOptionalClueState::Missing, "reset clears optional state from every stage");
		Model.Reset();
		Check(Model.GetState() == EOptionalClueState::Missing, "repeated reset remains missing");
		Check(!Model.Analyze() && !Model.Report(), "old analysis and report cannot continue after run reset");
		Check(Model.ObserveSource() && Model.Analyze() && Model.Report(), "new run can replay full narrative after every reset state");
		Check(Model.GetState() == EOptionalClueState::Reported, "replayed narrative reaches reported state");
	}
}

void CheckPrimaryUnchanged(const FQuestModel& Actual, const FQuestModel& Expected)
{
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		Check(Actual.GetState(Quest) == Expected.GetState(Quest), "optional action preserves primary quest state");
		Check(Actual.GetProgress(Quest) == Expected.GetProgress(Quest), "optional action preserves primary objective count");
		Check(Actual.IsClaimPending(Quest) == Expected.IsClaimPending(Quest), "optional action preserves primary reward reservation");
		for (int Objective = 0; Objective < ObjectiveCount; ++Objective)
		{
			Check(Actual.HasObserved(Quest, Objective) == Expected.HasObserved(Quest, Objective),
				"optional action preserves each primary objective fact");
		}
	}
}

void CheckPrimaryFresh(const FQuestModel& Model)
{
	for (int Quest = 0; Quest < QuestCount; ++Quest)
	{
		Check(Model.GetState(Quest) == EState::Available, "primary reset makes all five quests independently available");
		Check(Model.GetProgress(Quest) == 0, "primary reset clears objective progress");
		Check(!Model.IsClaimPending(Quest), "primary reset clears pending reward");
		for (int Objective = 0; Objective < ObjectiveCount; ++Objective)
		{
			Check(!Model.HasObserved(Quest, Objective), "primary reset clears every observed objective");
		}
	}
}

void PreAcceptanceSourceAndPrimaryIndependence()
{
	FQuestModel Primary;
	FOptionalClueModel Optional;
	// Default V1 objective 0 is the supply crate; objective 1 is the transport record.
	// The caller forwards only the real transport-record fact, even before accepting V1.
	Check(Primary.Observe(0, 1), "real V1 transport-record objective records before primary acceptance");
	Check(Primary.GetState(0) == EState::Available, "source discovery leaves V1 unaccepted");
	const FQuestModel BeforeOptional = Primary;
	Check(Optional.ObserveSource(), "caller can forward preacceptance V1 source to optional narrative");
	CheckPrimaryUnchanged(Primary, BeforeOptional);
	Check(Optional.Analyze(), "optional analysis requires no primary V4 acceptance");
	CheckPrimaryUnchanged(Primary, BeforeOptional);
	Check(Optional.Report(), "optional report requires no primary V1 completion");
	CheckPrimaryUnchanged(Primary, BeforeOptional);
	Check(Primary.GetState(3) == EState::Available && Primary.GetProgress(3) == 0,
		"optional V4 analysis leaves its primary quest untouched");
	Check(!Primary.BeginClaim(0), "optional narrative cannot authorize primary V1 reward");
	Check(!Primary.BeginClaim(3), "optional narrative cannot authorize primary V4 reward");
	Optional.Reset();
	CheckPrimaryUnchanged(Primary, BeforeOptional);
	Check(Optional.GetState() == EOptionalClueState::Missing, "optional reset clears only narrative state");
}

void MixedPrimaryStatesSurviveOptionalActions()
{
	FQuestModel Primary;
	Primary.Observe(0, 0); // Available, with one remembered fact.
	Primary.Accept(1); // Active.
	for (int Quest = 2; Quest < QuestCount; ++Quest)
	{
		Primary.Accept(Quest);
		Primary.Observe(Quest, 0);
		Primary.Observe(Quest, 1);
	}
	Primary.BeginClaim(3);
	Primary.FinishClaim(3, true); // Claimed.
	Primary.BeginClaim(4); // Ready with pending reward; quest 2 is Ready without one.
	const FQuestModel BeforeOptional = Primary;
	FOptionalClueModel Optional;
	const EAction Actions[] = {EAction::Report, EAction::Analyze, EAction::ObserveSource,
		EAction::Report, EAction::ObserveSource, EAction::Analyze, EAction::ObserveSource,
		EAction::Analyze, EAction::Report, EAction::ObserveSource, EAction::Report};
	for (EAction Action : Actions)
	{
		Apply(Optional, Action);
		CheckPrimaryUnchanged(Primary, BeforeOptional);
	}
	Check(Optional.GetState() == EOptionalClueState::Reported, "mixed valid and invalid dialogue finishes optional narrative");
	Optional.Reset();
	CheckPrimaryUnchanged(Primary, BeforeOptional);
	Primary.Reset();
	CheckPrimaryFresh(Primary);
	Check(Optional.GetState() == EOptionalClueState::Missing, "joint run reset clears both independent models");
	Primary.FinishClaim(4, true);
	CheckPrimaryFresh(Primary);
}

void AllPrimaryQuestsCompleteWithoutOptionalClue()
{
	FQuestModel Primary;
	FOptionalClueModel Optional;
	// Reverse village order exercises all five quests with the narrative entirely skipped.
	for (int Quest = QuestCount - 1; Quest >= 0; --Quest)
	{
		Check(Primary.Accept(Quest), "primary quest accepts while optional clue is missing");
		Check(Primary.Observe(Quest, 1) && Primary.Observe(Quest, 0), "primary objectives complete while optional clue is missing");
		Check(Primary.GetState(Quest) == EState::Ready, "primary readiness has no optional clue gate");
		Check(Primary.BeginClaim(Quest), "primary reward reservation has no optional clue gate");
		Primary.FinishClaim(Quest, true);
		Check(Primary.GetState(Quest) == EState::Claimed, "primary quest claims while optional clue is missing");
		Check(Optional.GetState() == EOptionalClueState::Missing, "primary progress and reward cannot manufacture optional clue state");
	}
	AdvanceTo(Optional, EOptionalClueState::Reported);
	Primary.Reset();
	CheckPrimaryFresh(Primary);
	Check(Optional.GetState() == EOptionalClueState::Reported, "primary model reset does not silently reset independent optional model");
	Optional.Reset();
	Check(Optional.GetState() == EOptionalClueState::Missing, "caller resets optional state explicitly for new run");
	CheckPrimaryFresh(Primary);
}
}

int main()
{
	ValidNarrativeFlow();
	AllStateActionPairs();
	RepeatedVisitsNeverRegress();
	ResetEveryState();
	PreAcceptanceSourceAndPrimaryIndependence();
	MixedPrimaryStatesSurviveOptionalActions();
	AllPrimaryQuestsCompleteWithoutOptionalClue();
	std::printf("RESULT=VILLAGE_OPTIONAL_CLUE_MODEL_%s checks=%d failures=%d scenarios=7\n",
		Failures ? "FAILED" : "PASSED", Checks, Failures);
	return Failures ? 1 : 0;
}
