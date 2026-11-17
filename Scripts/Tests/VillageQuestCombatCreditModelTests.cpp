#include "../../Source/Rogue10m/World/Rogue10mVillageQuestCombatCreditModel.h"

#include <cstdio>
#include <limits>
#include <type_traits>
#include <utility>

using Rogue10mVillageQuest::FCombatCreditDeliveryModel;
using Rogue10mVillageQuest::QuestCount;
using FLease = FCombatCreditDeliveryModel::FLease;

static_assert(QuestCount == 5, "The production quest model has five stable slots.");
static_assert(std::is_same_v<decltype(std::declval<const FCombatCreditDeliveryModel&>().GetEpoch()), std::uint64_t>);
static_assert(noexcept(std::declval<FCombatCreditDeliveryModel&>().Queue(0, 1)));
static_assert(noexcept(std::declval<FCombatCreditDeliveryModel&>().Begin(0)));
static_assert(noexcept(std::declval<FCombatCreditDeliveryModel&>().Finish(FLease{}, true)));
static_assert(noexcept(std::declval<FCombatCreditDeliveryModel&>().Cancel(FLease{})));
static_assert(noexcept(std::declval<FCombatCreditDeliveryModel&>().Reset()));
static_assert(noexcept(std::declval<const FCombatCreditDeliveryModel&>().HasPending(0)));
static_assert(noexcept(std::declval<const FCombatCreditDeliveryModel&>().IsDelivered(0)));

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

void CheckEmpty(const FCombatCreditDeliveryModel& Model)
{
	for (int Slot = 0; Slot < QuestCount; ++Slot)
	{
		Check(!Model.HasPending(Slot) && !Model.IsDelivered(Slot), "empty slot has neither work nor delivered receipt");
	}
}

enum class EFixtureState { Empty, Pending, Delivering, Delivered };
enum class EAction { Queue, Begin, FinishSuccess, FinishFailure, Cancel };

FLease Prepare(FCombatCreditDeliveryModel& Model, int Slot, EFixtureState State)
{
	if (State == EFixtureState::Empty) { return {}; }
	Check(Model.Queue(Slot, Model.GetEpoch()), "fixture queues valid context");
	FLease Lease = Model.Begin(Slot);
	Check(Lease.IsValid(), "fixture obtains delivery lease");
	if (State == EFixtureState::Pending) { Check(Model.Finish(Lease, false), "fixture retains failed delivery"); }
	if (State == EFixtureState::Delivered) { Check(Model.Finish(Lease, true), "fixture completes delivery"); }
	return Lease;
}

void ActionMatrix()
{
	const EFixtureState States[] = {EFixtureState::Empty, EFixtureState::Pending,
		EFixtureState::Delivering, EFixtureState::Delivered};
	const EAction Actions[] = {EAction::Queue, EAction::Begin, EAction::FinishSuccess,
		EAction::FinishFailure, EAction::Cancel};
	for (int Slot = 0; Slot < QuestCount; ++Slot)
	{
		for (EFixtureState State : States)
		{
			for (EAction Action : Actions)
			{
				FCombatCreditDeliveryModel Model;
				Check(Model.GetEpoch() == 1, "initial epoch is nonzero one");
				const FLease Lease = Prepare(Model, Slot, State);
				bool bActual = false;
				bool bExpected = false;
				EFixtureState Expected = State;
				switch (Action)
				{
				case EAction::Queue:
					bActual = Model.Queue(Slot, Model.GetEpoch());
					bExpected = State == EFixtureState::Empty;
					if (bExpected) { Expected = EFixtureState::Pending; }
					break;
				case EAction::Begin:
				{
					const FLease Attempt = Model.Begin(Slot);
					bActual = Attempt.IsValid();
					bExpected = State == EFixtureState::Pending;
					if (bExpected)
					{
						Expected = EFixtureState::Delivering;
						Check(Attempt.Slot == Slot && Attempt.Epoch == Model.GetEpoch(), "lease describes the requested slot and current epoch");
					}
					break;
				}
				case EAction::FinishSuccess:
					bActual = Model.Finish(Lease, true);
					bExpected = State == EFixtureState::Delivering;
					if (bExpected) { Expected = EFixtureState::Delivered; }
					break;
				case EAction::FinishFailure:
					bActual = Model.Finish(Lease, false);
					bExpected = State == EFixtureState::Delivering;
					if (bExpected) { Expected = EFixtureState::Pending; }
					break;
				case EAction::Cancel:
					bActual = Model.Cancel(Lease);
					bExpected = State == EFixtureState::Delivering;
					if (bExpected) { Expected = EFixtureState::Empty; }
					break;
				}
				Check(bActual == bExpected, "every state/action pair accepts only its specified transition");
				Check(Model.HasPending(Slot) == (Expected == EFixtureState::Pending || Expected == EFixtureState::Delivering),
					"pending query includes an active delivery reservation");
				Check(Model.IsDelivered(Slot) == (Expected == EFixtureState::Delivered), "delivered receipt follows only successful delivery");
				for (int Other = 0; Other < QuestCount; ++Other)
				{
					if (Other != Slot)
					{
						Check(!Model.HasPending(Other) && !Model.IsDelivered(Other), "transition never affects a different quest slot");
					}
				}
			}
		}
	}
}

void DelayedContextAndReset()
{
	FCombatCreditDeliveryModel Model;
	const std::uint64_t Captured = Model.GetEpoch();
	Check(!Model.Queue(0, 0) && !Model.Queue(0, Captured + 1), "unknown and future contexts cannot queue a lethal event");
	Check(Model.Queue(0, Captured), "a delayed event still queues within its captured run");
	const FLease Old = Model.Begin(0);
	Model.Reset();
	Check(Model.GetEpoch() == Captured + 1, "new run advances delivery context");
	CheckEmpty(Model);
	Check(!Model.Queue(0, Captured), "old delayed lethal event cannot queue in a new run");
	Check(Model.Queue(0, Model.GetEpoch()), "new run can queue same quest slot");
	const FLease Current = Model.Begin(0);
	Check(!Model.Finish(Old, true) && !Model.Finish(Old, false) && !Model.Cancel(Old), "old run lease cannot complete or cancel new delivery");
	Check(Model.HasPending(0) && !Model.IsDelivered(0), "stale run callback preserves active new-run delivery");
	Check(Model.Finish(Current, true), "current run lease still completes after stale callbacks");
}

void ReentrancyAndRetries()
{
	for (int Slot = 0; Slot < QuestCount; ++Slot)
	{
		FCombatCreditDeliveryModel Model;
		Check(Model.Queue(Slot, Model.GetEpoch()), "first lethal event queues");
		const FLease First = Model.Begin(Slot);
		Check(First.IsValid(), "first observer obtains reservation");
		Check(!Model.Begin(Slot).IsValid(), "reentrant observation cannot reserve the delivering slot");
		Check(!Model.Queue(Slot, Model.GetEpoch()), "reentrant lethal callback cannot queue a second receipt");
		Check(Model.HasPending(Slot) && !Model.IsDelivered(Slot), "receipt remains pending during external observation");
		Check(Model.Finish(First, false), "temporary observation failure releases the reservation for retry");
		Check(!Model.Finish(First, true) && !Model.Cancel(First), "completed failed attempt cannot act while work is pending");
		const FLease Retry = Model.Begin(Slot);
		Check(Retry.IsValid() && Retry.Ticket != First.Ticket, "retry receives a fresh attempt token");
		Check(!Model.Finish(First, true) && !Model.Finish(First, false) && !Model.Cancel(First),
			"failed first-attempt callback cannot finish or cancel the active retry");
		Check(Model.Finish(Retry, true), "current retry records successful delivery");
		Check(!Model.Finish(Retry, true) && !Model.Cancel(Retry) && !Model.Begin(Slot).IsValid(), "successful receipt cannot deliver twice");
		Check(!Model.Queue(Slot, Model.GetEpoch()) && Model.IsDelivered(Slot), "duplicate lethal event cannot replace delivered receipt");
	}
}

void CancelQueueABA()
{
	FCombatCreditDeliveryModel Model;
	for (int Slot = 0; Slot < QuestCount; ++Slot)
	{
		Check(Model.Queue(Slot, Model.GetEpoch()), "cancel fixture queues event");
		const FLease First = Model.Begin(Slot);
		Check(Model.Cancel(First), "matching active reservation cancels");
		Check(!Model.HasPending(Slot) && !Model.IsDelivered(Slot), "cancel leaves empty slot without delivered receipt");
		Check(!Model.Cancel(First) && !Model.Finish(First, true), "cancelled lease cannot complete empty slot");
		Check(Model.Queue(Slot, Model.GetEpoch()), "same epoch can queue a new event after cancellation");
		const FLease Second = Model.Begin(Slot);
		Check(Second.IsValid() && Second.Epoch == First.Epoch && Second.Ticket != First.Ticket,
			"requeue in same epoch uses a distinct ticket");
		Check(!Model.Finish(First, true) && !Model.Cancel(First), "cancel-requeue ABA cannot reuse old lease");
		Check(Model.Finish(Second, true), "new event completes after stale old-event callbacks");
	}
}

void WrongLeaseFieldsAndBounds()
{
	FCombatCreditDeliveryModel Model;
	Check(!FLease{}.IsValid(), "default lease is invalid");
	Check(Model.Queue(0, Model.GetEpoch()), "invalid-input fixture queues event");
	const FLease Valid = Model.Begin(0);
	const int Invalid[] = {-1, QuestCount, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()};
	for (int Slot : Invalid)
	{
		Check(!Model.Queue(Slot, Model.GetEpoch()) && !Model.Begin(Slot).IsValid(), "invalid slot rejects queue and reservation without indexing");
		Check(!Model.HasPending(Slot) && !Model.IsDelivered(Slot), "invalid slot queries are false");
		FLease Forged = Valid;
		Forged.Slot = Slot;
		Check(!Forged.IsValid() && !Model.Finish(Forged, true) && !Model.Cancel(Forged), "invalid-slot lease cannot index or alter state");
	}
	FLease Bad[] = {Valid, Valid, Valid, Valid, Valid};
	Bad[0].Epoch = 0;
	Bad[1].Epoch = Valid.Epoch + 1;
	Bad[2].Ticket = 0;
	Bad[3].Ticket = Valid.Ticket + 1;
	Bad[4].Slot = 1;
	for (const FLease& Forged : Bad)
	{
		Check(!Model.Finish(Forged, true) && !Model.Finish(Forged, false) && !Model.Cancel(Forged),
			"wrong epoch, ticket, or slot cannot complete or cancel the live reservation");
		Check(Model.HasPending(0) && !Model.IsDelivered(0), "forged fields preserve original live delivery");
	}
	Check(Model.Finish(Valid, true), "correct lease survives all rejected inputs");
}

void IndependentSlots()
{
	FCombatCreditDeliveryModel Model;
	FLease Leases[QuestCount];
	for (int Slot = 0; Slot < QuestCount; ++Slot)
	{
		Check(Model.Queue(Slot, Model.GetEpoch()), "all quest slots can hold independent events");
		Leases[Slot] = Model.Begin(Slot);
		Check(Leases[Slot].IsValid(), "all independent events can be reserved simultaneously");
		for (int Previous = 0; Previous < Slot; ++Previous)
		{
			Check(Leases[Slot].Ticket != Leases[Previous].Ticket, "simultaneous slots have distinct tickets");
		}
	}
	FLease WrongSlot = Leases[0];
	WrongSlot.Slot = QuestCount - 1;
	Check(!Model.Finish(WrongSlot, true) && !Model.Cancel(WrongSlot), "one active slot lease cannot consume another active slot");
	for (int Slot = QuestCount - 1; Slot >= 0; --Slot)
	{
		Check(Model.Finish(Leases[Slot], true), "independent events may finish in reverse order");
		for (int Waiting = 0; Waiting < Slot; ++Waiting)
		{
			Check(Model.HasPending(Waiting) && !Model.IsDelivered(Waiting), "finishing another slot preserves outstanding reservations");
		}
	}
}

void ResetEveryState()
{
	FCombatCreditDeliveryModel Model;
	Prepare(Model, 1, EFixtureState::Pending);
	const FLease Delivering = Prepare(Model, 2, EFixtureState::Delivering);
	const FLease Delivered = Prepare(Model, 3, EFixtureState::Delivered);
	const FLease Failed = Prepare(Model, 4, EFixtureState::Pending);
	Model.Reset();
	CheckEmpty(Model);
	Check(!Model.Finish(Delivering, true) && !Model.Cancel(Delivering), "reset invalidates outstanding reservation");
	Check(!Model.Finish(Delivered, false) && !Model.Finish(Failed, true), "reset invalidates completed and failed-attempt leases");
	for (int Run = 0; Run < 12; ++Run)
	{
		const std::uint64_t Epoch = Model.GetEpoch();
		Check(Epoch != 0, "ordinary resets keep usable nonzero epochs");
		Check(Model.Queue(0, Epoch), "fresh run can queue after reset");
		const FLease Lease = Model.Begin(0);
		Check(Model.Finish(Lease, true), "fresh run can deliver after reset");
		Model.Reset();
		Check(Model.GetEpoch() == Epoch + 1, "every reset advances the epoch even after delivered receipt");
		Check(!Model.Queue(0, Epoch) && !Model.Finish(Lease, true), "old run cannot reenter after repeated resets");
		CheckEmpty(Model);
	}
}

void CounterBoundaries()
{
	using Rogue10mVillageQuest::Detail::NextCombatCreditGeneration;
	constexpr std::uint64_t Maximum = (std::numeric_limits<std::uint64_t>::max)();
	static_assert(NextCombatCreditGeneration(0) == 1);
	static_assert(NextCombatCreditGeneration(Maximum - 1) == Maximum);
	static_assert(NextCombatCreditGeneration(Maximum) == 0);
	Check(NextCombatCreditGeneration(1) == 2, "shared epoch/ticket arithmetic advances ordinary generation");
	Check(NextCombatCreditGeneration(Maximum - 1) == Maximum, "last nonzero generation is representable");
	Check(NextCombatCreditGeneration(Maximum) == 0, "exhausted generation returns invalid zero rather than reusing one");
}

void BoundedMixedSequences()
{
	// All 6^5 short traces exercise ordering, cancellation, retry, and reset mixtures.
	constexpr int Depth = 5;
	constexpr int ActionCount = 6;
	constexpr int TraceCount = 7776;
	for (int Trace = 0; Trace < TraceCount; ++Trace)
	{
		FCombatCreditDeliveryModel Model;
		FLease Last;
		int Encoded = Trace;
		int DeliveriesThisEpoch = 0;
		for (int Step = 0; Step < Depth; ++Step)
		{
			const int Action = Encoded % ActionCount;
			Encoded /= ActionCount;
			const bool bWasDelivered = Model.IsDelivered(0);
			const bool bWasPending = Model.HasPending(0);
			const std::uint64_t Epoch = Model.GetEpoch();
			switch (Action)
			{
			case 0:
			{
				const bool bQueued = Model.Queue(0, Epoch);
				Check(bQueued == (!bWasPending && !bWasDelivered), "mixed trace queues only empty slot");
				break;
			}
			case 1:
			{
				const FLease Attempt = Model.Begin(0);
				if (Attempt.IsValid()) { Last = Attempt; }
				break;
			}
			case 2:
				if (Model.Finish(Last, true))
				{
					++DeliveriesThisEpoch;
					Check(DeliveriesThisEpoch == 1 && bWasPending, "mixed trace cannot complete same quest event twice within epoch");
				}
				break;
			case 3:
				if (Model.Finish(Last, false))
				{
					Check(Model.HasPending(0) && !Model.IsDelivered(0), "mixed trace preserves failed event for retry");
				}
				break;
			case 4:
				if (Model.Cancel(Last))
				{
					Check(!Model.HasPending(0) && !Model.IsDelivered(0), "mixed trace cancel clears only live reservation");
				}
				break;
			case 5:
				Model.Reset();
				DeliveriesThisEpoch = 0;
				break;
			}
			Check(!(Model.HasPending(0) && Model.IsDelivered(0)), "mixed trace cannot be both pending and delivered");
			if (bWasDelivered && Model.GetEpoch() == Epoch)
			{
				Check(Model.IsDelivered(0), "delivered receipt is monotonic until a new epoch");
			}
			for (int Other = 1; Other < QuestCount; ++Other)
			{
				Check(!Model.HasPending(Other) && !Model.IsDelivered(Other), "mixed trace preserves every untouched quest slot");
			}
		}
	}
}
}

int main()
{
	ActionMatrix();
	DelayedContextAndReset();
	ReentrancyAndRetries();
	CancelQueueABA();
	WrongLeaseFieldsAndBounds();
	IndependentSlots();
	ResetEveryState();
	CounterBoundaries();
	BoundedMixedSequences();
	std::printf("RESULT=VILLAGE_COMBAT_CREDIT_MODEL_%s checks=%d failures=%d scenarios=9 mixed_traces=7776\n",
		Failures ? "FAILED" : "PASSED", Checks, Failures);
	return Failures ? 1 : 0;
}
