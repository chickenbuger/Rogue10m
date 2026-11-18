#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

/** Accepted montage-relative contact deadlines. The actual Combat path uses this exact kernel. */
struct FRogue10mAuthoredContactTimeline
{
	static constexpr int MaxContacts = 64;
	static constexpr double MaxDuration = 30.0;
	static constexpr double ClockTolerance = 0.000001; // Float-authored times on a double world clock.
	struct FPoll
	{
		bool bValid = false;
		bool bDue = false;
		int ContactNumber = 0;
	};
	struct FArm { std::uint64_t Generation = 0; int ContactNumber = 0; };

	static bool Validate(const float* Times, int Count, double Rate, double MontageLength, double Start, double RecoveryEnd = 0.0)
	{
		if (!Times || Count < 1 || Count > MaxContacts || !std::isfinite(Rate) || Rate <= 0.0
			|| !std::isfinite(MontageLength) || MontageLength <= 0.0 || !std::isfinite(Start) || Start < 0.0) { return false; }
		double Previous = -1.0;
		for (int Index = 0; Index < Count; ++Index)
		{
			const double Time = Times[Index];
			const double Scaled = Time / Rate;
			if (!std::isfinite(Time) || Time < 0.0 || Time <= Previous || Time >= MontageLength
				|| !std::isfinite(Scaled) || Scaled > MaxDuration || !std::isfinite(Start + Scaled)) { return false; }
			Previous = Time;
		}
		return std::isfinite(RecoveryEnd) && RecoveryEnd >= 0.0
			&& (RecoveryEnd == 0.0 || (RecoveryEnd >= Previous && RecoveryEnd <= MontageLength
				&& RecoveryEnd / Rate <= MaxDuration && std::isfinite(Start + RecoveryEnd / Rate)));
	}
	bool Begin(const float* Times, int Count, double Rate, double MontageLength, double Start, double RecoveryEnd = 0.0)
	{
		if (bActive || (!bCancelled && bRecoveryOverride && ContactCount > 0 && NextContact >= ContactCount
			&& Start + ClockTolerance < RecoveryDeadline())
			|| !Validate(Times, Count, Rate, MontageLength, Start, RecoveryEnd)) { return false; }
		for (int Index = 0; Index < Count; ++Index) { Offsets[Index] = Times[Index] / Rate; }
		ContactCount = Count; NextContact = 0; StartedAt = LastObservedAt = Start; bActive = true; bCancelled = false;
		bRecoveryOverride = RecoveryEnd > 0.0; ReleaseOffset = RecoveryEnd / Rate;
		bArmed = false; ++ArmGeneration;
		return true;
	}
	double SecondsUntilNext(double Now) const
	{
		if (!bActive || !std::isfinite(Now) || Now < LastObservedAt
			|| (bRecoveryOverride && Now > RecoveryDeadline() + ClockTolerance)) { return std::numeric_limits<double>::quiet_NaN(); }
		const double Remaining = Offsets[NextContact] - (Now - StartedAt);
		return Remaining > ClockTolerance ? Remaining : 0.0;
	}
	FPoll Poll(double Now)
	{
		const double Remaining = SecondsUntilNext(Now);
		if (!std::isfinite(Remaining)) { Cancel(); return {}; }
		LastObservedAt = Now;
		if (Remaining > 0.0) { return {true, false, 0}; }
		const int Number = ++NextContact;
		bArmed = false;
		bActive = NextContact < ContactCount;
		return {true, true, Number}; // One contact per callback, including after a hitch.
	}
	FArm ArmNextContact()
	{
		if (!bActive || bCancelled) { return {}; }
		if (++ArmGeneration == 0) { ++ArmGeneration; }
		bArmed = true; return {ArmGeneration, NextContact + 1};
	}
	bool ConsumeArm(std::uint64_t Generation, int ExpectedContact)
	{
		if (!bActive || bCancelled || !bArmed || Generation != ArmGeneration || ExpectedContact != NextContact + 1) { return false; }
		bArmed = false; return true;
	}
	std::uint64_t GetArmGeneration() const { return ArmGeneration; }
	bool HasRecoveryOverride() const { return bRecoveryOverride; }
	double RecoveryDeadline() const { return StartedAt + ReleaseOffset; }
	bool IsRecoveryPending(double Now) const
	{
		return !bCancelled && bRecoveryOverride && ContactCount > 0 && NextContact >= ContactCount
			&& std::isfinite(Now) && Now >= LastObservedAt && Now + ClockTolerance < RecoveryDeadline();
	}
	void Cancel() { bActive = bArmed = false; bCancelled = true; ++ArmGeneration; }
	bool IsActive() const { return bActive; }
	int CompletedCount() const { return NextContact; }
	int TotalCount() const { return ContactCount; }

private:
	std::array<double, MaxContacts> Offsets{};
	double StartedAt = 0.0, LastObservedAt = 0.0;
	double ReleaseOffset = 0.0;
	std::uint64_t ArmGeneration = 0;
	int ContactCount = 0, NextContact = 0;
	bool bActive = false, bArmed = false, bCancelled = false, bRecoveryOverride = false;
};
