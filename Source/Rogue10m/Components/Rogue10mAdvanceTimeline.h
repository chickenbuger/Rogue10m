#pragma once

#include <cmath>

/** Engine-independent timeline used by the actual capsule-advance component. Distances are cm. */
struct FRogue10mAdvanceTimeline
{
	struct FStep { double Distance = 0.0; bool bContactReady = false; bool bValid = true; };

	static bool IsValid(double Distance, double Duration, double ContactDelay, double Start)
	{
		return std::isfinite(Distance) && Distance > 0.0 && Distance <= 600.0
			&& std::isfinite(Duration) && Duration > 0.0 && Duration <= 30.0
			&& std::isfinite(ContactDelay) && ContactDelay >= 0.0 && ContactDelay <= 30.0
			&& std::isfinite(Start);
	}
	bool Begin(double Distance, double Duration, double ContactDelay, double Start)
	{
		if (bActive || !IsValid(Distance, Duration, ContactDelay, Start)) { return false; }
		TotalDistance = Distance; MoveDuration = Duration; EarliestContact = ContactDelay;
		StartedAt = Start; LastElapsed = LastProgress = 0.0; bTravelStopped = false; bActive = true;
		return true;
	}
	FStep Step(double Now)
	{
		if (!bActive) { return {}; }
		if (!std::isfinite(Now)) { Cancel(); return {0.0, false, false}; }
		const double Elapsed = Now - StartedAt > LastElapsed ? Now - StartedAt : LastElapsed;
		const double Fraction = Elapsed / MoveDuration;
		const double Progress = bTravelStopped ? LastProgress : (Fraction < 0.0 ? 0.0 : (Fraction > 1.0 ? 1.0 : Fraction));
		const double Delta = (Progress - LastProgress) * TotalDistance;
		LastElapsed = Elapsed; LastProgress = Progress;
		bTravelStopped = bTravelStopped || Progress >= 1.0;
		const bool bReady = bTravelStopped && Elapsed >= EarliestContact;
		if (bReady) { bActive = false; }
		return {Delta, bReady, true};
	}
	void StopTravel() { bTravelStopped = true; }
	void Cancel() { bActive = false; }
	bool IsActive() const { return bActive; }

private:
	double TotalDistance = 0.0, MoveDuration = 0.0, EarliestContact = 0.0, StartedAt = 0.0;
	double LastElapsed = 0.0, LastProgress = 0.0;
	bool bActive = false, bTravelStopped = false;
};
