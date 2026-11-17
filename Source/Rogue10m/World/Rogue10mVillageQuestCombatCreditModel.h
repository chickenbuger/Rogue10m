#pragma once

#include "Rogue10mVillageQuestModel.h"

#include <cstdint>
#include <limits>

namespace Rogue10mVillageQuest
{
namespace Detail
{
/** Zero denotes exhaustion; never wrap a live generation back to an earlier one. */
constexpr std::uint64_t NextCombatCreditGeneration(std::uint64_t Current) noexcept
{
	return Current == (std::numeric_limits<std::uint64_t>::max)() ? 0 : Current + 1;
}
}

/** Portable delivery reservations only. The caller validates run/character context and records facts. */
class FCombatCreditDeliveryModel
{
public:
	struct FLease
	{
		int Slot = -1;
		std::uint64_t Epoch = 0;
		std::uint64_t Ticket = 0;

		bool IsValid() const noexcept
		{
			return Slot >= 0 && Slot < QuestCount && Epoch != 0 && Ticket != 0;
		}
	};

	std::uint64_t GetEpoch() const noexcept { return Epoch; }

	/** One event per quest/run. A stale callback cannot create work in a later epoch. */
	bool Queue(int Slot, std::uint64_t CapturedEpoch) noexcept
	{
		if (!IsValidSlot(Slot) || Epoch == 0 || CapturedEpoch != Epoch
			|| Entries[Slot].State != EDeliveryState::Empty) { return false; }
		const std::uint64_t Ticket = Detail::NextCombatCreditGeneration(LastTicket);
		if (Ticket == 0) { return false; }
		LastTicket = Ticket;
		Entries[Slot] = {EDeliveryState::Pending, Ticket};
		return true;
	}

	/** Reserve before calling external observation code, which may reenter or reset the caller. */
	FLease Begin(int Slot) noexcept
	{
		if (!IsValidSlot(Slot) || Epoch == 0 || Entries[Slot].State != EDeliveryState::Pending) { return {}; }
		// Every attempt gets a fresh lease, including retries of the same queued event.
		const std::uint64_t Ticket = Detail::NextCombatCreditGeneration(LastTicket);
		if (Ticket == 0) { return {}; }
		LastTicket = Ticket;
		Entries[Slot].Ticket = Ticket;
		Entries[Slot].State = EDeliveryState::Delivering;
		return {Slot, Epoch, Ticket};
	}

	/** Failed observation retains the same event for retry; success blocks duplicate delivery. */
	bool Finish(const FLease& Lease, bool bSuccess) noexcept
	{
		if (!MatchesDelivery(Lease)) { return false; }
		Entries[Lease.Slot].State = bSuccess ? EDeliveryState::Delivered : EDeliveryState::Pending;
		return true;
	}

	/** Cancel only the live reservation; later Queue calls receive a distinct ticket. */
	bool Cancel(const FLease& Lease) noexcept
	{
		if (!MatchesDelivery(Lease)) { return false; }
		Entries[Lease.Slot] = {};
		return true;
	}

	void Reset() noexcept
	{
		for (FEntry& Entry : Entries) { Entry = {}; }
		// Once exhausted, keep the model disabled instead of reusing epoch 1.
		if (Epoch != 0) { Epoch = Detail::NextCombatCreditGeneration(Epoch); }
		LastTicket = 0;
	}

	bool HasPending(int Slot) const noexcept
	{
		return IsValidSlot(Slot)
			&& (Entries[Slot].State == EDeliveryState::Pending || Entries[Slot].State == EDeliveryState::Delivering);
	}

	bool IsDelivered(int Slot) const noexcept
	{
		return IsValidSlot(Slot) && Entries[Slot].State == EDeliveryState::Delivered;
	}

private:
	enum class EDeliveryState : unsigned char { Empty, Pending, Delivering, Delivered };
	struct FEntry
	{
		EDeliveryState State = EDeliveryState::Empty;
		std::uint64_t Ticket = 0;
	};

	static bool IsValidSlot(int Slot) noexcept { return Slot >= 0 && Slot < QuestCount; }

	bool MatchesDelivery(const FLease& Lease) const noexcept
	{
		return Lease.IsValid() && Epoch != 0 && Lease.Epoch == Epoch
			&& Entries[Lease.Slot].State == EDeliveryState::Delivering && Entries[Lease.Slot].Ticket == Lease.Ticket;
	}

	FEntry Entries[QuestCount] = {};
	std::uint64_t Epoch = 1;
	std::uint64_t LastTicket = 0;
};
}
