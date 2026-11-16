// Copyright Rogue10m. All Rights Reserved.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Rogue10m::Magic
{
enum class ECommandToken : std::uint8_t
{
	Q,
	E,
	R
};

enum class ECommandKeyStatus : std::uint8_t
{
	Appended,
	Released,
	Repeated,
	Full,
	Inactive,
	StaleSession,
	InvalidToken
};

struct FCommandKeyResult
{
	ECommandKeyStatus Status;
	// If true, the router must not forward this event to an instant skill binding.
	bool bConsumed;
};

/**
 * Ordered physical Q/E/R edges for one command session. Call from one input thread.
 * This class does not bind Ctrl, recognize spells, auto-submit, or spend resources.
 * Capacity is supplied by the caller, not a gameplay limit chosen by this class.
 * A router must feed both Down and Up and capture the epoch when queuing events.
 */
template <std::size_t Capacity>
class TCommandBuffer
{
	static_assert(Capacity > 0, "A command buffer must have storage.");

public:
	using FEpoch = std::uint64_t;

	struct FSnapshot
	{
		FEpoch Epoch = 0;
		std::array<ECommandToken, Capacity> Tokens{};
		std::size_t Count = 0;
		bool bActive = false;
	};

	static constexpr std::uint8_t KeyMask(ECommandToken Token)
	{
		// Validate before any indexing or shifting; cast enum values may be invalid.
		switch (Token)
		{
		case ECommandToken::Q: return 1;
		case ECommandToken::E: return 2;
		case ECommandToken::R: return 4;
		default: return 0;
		}
	}

	/** Capture physical keys already held BEFORE entering, even from the prior mode. */
	FEpoch BeginSession(std::uint8_t InitialPhysicalDownMask)
	{
		AdvanceEpoch();
		Count = 0;
		Tokens.fill(ECommandToken::Q);
		PhysicalDownMask = InitialPhysicalDownMask & 7;
		bActive = true;
		return Epoch;
	}

	FCommandKeyResult KeyDown(FEpoch CapturedEpoch, ECommandToken Token)
	{
		if (CapturedEpoch != Epoch) { return Result(ECommandKeyStatus::StaleSession); }
		const auto Mask = KeyMask(Token);
		if (!Mask) { return Result(ECommandKeyStatus::InvalidToken); }
		if (!bActive) { return Result(ECommandKeyStatus::Inactive); }
		if (PhysicalDownMask & Mask) { return Result(ECommandKeyStatus::Repeated); }
		PhysicalDownMask |= Mask;
		if (Count == Capacity) { return Result(ECommandKeyStatus::Full); }
		Tokens[Count++] = Token;
		return Result(ECommandKeyStatus::Appended);
	}

	FCommandKeyResult KeyUp(FEpoch CapturedEpoch, ECommandToken Token)
	{
		if (CapturedEpoch != Epoch) { return Result(ECommandKeyStatus::StaleSession); }
		const auto Mask = KeyMask(Token);
		if (!Mask) { return Result(ECommandKeyStatus::InvalidToken); }
		if (!bActive) { return Result(ECommandKeyStatus::Inactive); }
		PhysicalDownMask &= static_cast<std::uint8_t>(~Mask);
		return Result(ECommandKeyStatus::Released);
	}

	/** System cancellation discards input; it never means spell failure or penalty. */
	bool CancelSession(FEpoch CapturedEpoch)
	{
		if (!bActive || CapturedEpoch != Epoch) { return false; }
		bActive = false;
		Count = 0;
		Tokens.fill(ECommandToken::Q);
		PhysicalDownMask = 0;
		AdvanceEpoch();
		return true;
	}

	FSnapshot ReadSnapshot() const { return {Epoch, Tokens, Count, bActive}; }
	FEpoch GetEpoch() const { return Epoch; }
	bool IsActive() const { return bActive; }

private:
	FCommandKeyResult Result(ECommandKeyStatus Status) const
	{
		// A queued command event remains consumed after cancellation, even in normal mode.
		return {Status, bActive || Status == ECommandKeyStatus::StaleSession};
	}
	void AdvanceEpoch()
	{
		++Epoch;
		if (Epoch == 0) { ++Epoch; }
	}

	std::array<ECommandToken, Capacity> Tokens{};
	std::size_t Count = 0;
	FEpoch Epoch = 0;
	std::uint8_t PhysicalDownMask = 0;
	bool bActive = false;
};
} // namespace Rogue10m::Magic
