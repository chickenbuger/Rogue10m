// Portable run-local observations. No Unreal, save, reward, or travel dependency.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Rogue10mFiveVillage
{
class FProgressModel
{
public:
	static constexpr int VillageCount = 5;
	static constexpr std::uint16_t MaximumVisits = std::numeric_limits<std::uint16_t>::max();

	void Reset() noexcept
	{
		Visits.fill(0);
		CurrentVillage = -1;
		Revisits = 0;
		FirstBossApproachSeconds = -1.0;
	}

	/** -1 is a real observed exit. Other invalid indices leave all state intact. */
	bool ObserveVillage(int VillageIndex) noexcept
	{
		if (VillageIndex < -1 || VillageIndex >= VillageCount || VillageIndex == CurrentVillage)
		{
			return false;
		}
		CurrentVillage = VillageIndex;
		if (VillageIndex >= 0)
		{
			std::uint16_t& Count = Visits[static_cast<std::size_t>(VillageIndex)];
			if (Count > 0 && Revisits < std::numeric_limits<std::uint32_t>::max())
			{
				++Revisits;
			}
			if (Count < MaximumVisits)
			{
				++Count;
			}
		}
		return true;
	}

	/** Observation only: there is deliberately no village/quest requirement. */
	bool ObserveBossApproach(double ActiveRunSeconds) noexcept
	{
		if (HasEnteredBossApproach() || !std::isfinite(ActiveRunSeconds) || ActiveRunSeconds < 0.0)
		{
			return false;
		}
		FirstBossApproachSeconds = ActiveRunSeconds;
		return true;
	}

	int GetCurrentVillageIndex() const noexcept { return CurrentVillage; }
	std::uint16_t GetVisitCount(int VillageIndex) const noexcept
	{
		return VillageIndex >= 0 && VillageIndex < VillageCount
			? Visits[static_cast<std::size_t>(VillageIndex)] : 0;
	}
	std::uint32_t GetRevisitCount() const noexcept { return Revisits; }
	bool HasEnteredBossApproach() const noexcept { return FirstBossApproachSeconds >= 0.0; }
	double GetFirstBossApproachActiveSeconds() const noexcept { return FirstBossApproachSeconds; }

private:
	std::array<std::uint16_t, VillageCount> Visits{};
	int CurrentVillage = -1;
	std::uint32_t Revisits = 0;
	double FirstBossApproachSeconds = -1.0;
};
}
