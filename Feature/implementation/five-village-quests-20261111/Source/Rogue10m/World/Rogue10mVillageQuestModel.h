// Portable run-local collection quest. No Unreal, item, save or currency dependency.
#pragma once

#include <cstdint>

namespace Rogue10mVillageQuest
{
enum class EStatus : std::uint8_t { Unaccepted, Accepted, Ready, Claiming, Claimed };

class FQuestModel
{
public:
	static constexpr unsigned MaxObjectives = 32;
	using FClaimToken = std::uint64_t;

	/** Invalid definitions or reconfiguration during a claim leave all state intact. */
	bool Configure(unsigned ObjectiveCount) noexcept
	{
		if (ObjectiveCount == 0 || ObjectiveCount > MaxObjectives || CurrentClaim != 0) { return false; }
		Count = ObjectiveCount;
		Reset();
		return true;
	}

	/** Keep the definition and monotonically increasing tickets across run resets. */
	void Reset() noexcept
	{
		Facts = 0;
		bAccepted = false;
		bClaimed = false;
		CurrentClaim = 0;
	}

	bool Accept() noexcept
	{
		if (!IsConfigured() || bAccepted || bClaimed || CurrentClaim != 0) { return false; }
		bAccepted = true;
		return true;
	}

	/** A fact may precede acceptance. Repeated, unknown or post-claim facts do not mutate. */
	bool RecordObjective(unsigned Index) noexcept
	{
		if (!IsConfigured() || Index >= Count || CurrentClaim != 0 || bClaimed) { return false; }
		const std::uint32_t Bit = std::uint32_t{1} << Index;
		if ((Facts & Bit) != 0) { return false; }
		Facts |= Bit;
		return true;
	}

	/** Lock before invoking inventory or any other reentrant callback. Zero means rejected. */
	FClaimToken TryBeginClaim() noexcept
	{
		if (GetStatus() != EStatus::Ready || NextClaim == ~FClaimToken{0}) { return 0; }
		CurrentClaim = ++NextClaim;
		return CurrentClaim;
	}

	/** A stale ticket can never finish a later retry or a claim from a new run. */
	bool FinishClaim(FClaimToken Token, bool bSucceeded) noexcept
	{
		if (Token == 0 || Token != CurrentClaim || GetStatus() != EStatus::Claiming) { return false; }
		CurrentClaim = 0;
		bClaimed = bSucceeded;
		return true;
	}

	EStatus GetStatus() const noexcept
	{
		if (CurrentClaim != 0) { return EStatus::Claiming; }
		if (bClaimed) { return EStatus::Claimed; }
		if (!bAccepted) { return EStatus::Unaccepted; }
		return Facts == RequiredFacts() ? EStatus::Ready : EStatus::Accepted;
	}
	unsigned GetCollectedCount() const noexcept
	{
		unsigned Result = 0;
		for (std::uint32_t Bits = Facts; Bits != 0; Bits &= Bits - 1) { ++Result; }
		return Result;
	}
	unsigned GetObjectiveCount() const noexcept { return Count; }
	bool HasObjective(unsigned Index) const noexcept
	{
		return Index < Count && (Facts & (std::uint32_t{1} << Index)) != 0;
	}
	bool IsConfigured() const noexcept { return Count > 0 && Count <= MaxObjectives; }

private:
	std::uint32_t RequiredFacts() const noexcept
	{
		return Count == MaxObjectives ? ~std::uint32_t{0} : (Count == 0 ? 0 : (std::uint32_t{1} << Count) - 1);
	}
	unsigned Count = 0;
	std::uint32_t Facts = 0;
	FClaimToken NextClaim = 0;
	FClaimToken CurrentClaim = 0;
	bool bAccepted = false;
	bool bClaimed = false;
};
}
