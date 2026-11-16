// Standalone tests of the production quantity planner. No Unreal runtime is loaded.
#include "Components/Rogue10mLootStackTransfer.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Rogue10m::LootStackTransfer::FPlan;
using Rogue10m::LootStackTransfer::PlanTransfer;
std::uint64_t Checks = 0;
int Cases = 0;

void Check(bool bCondition, const std::string& Message)
{
	++Checks;
	if (!bCondition) { throw std::runtime_error(Message); }
}

void VerifyValidPlan(std::int32_t Incoming, std::int32_t Maximum,
	const std::vector<std::int32_t>& Existing, bool bFreeFootprint, const FPlan& Plan)
{
	Check(Plan.bValidInput, "valid input rejected");
	Check(Plan.StackIncrements.size() == Existing.size(), "target alignment changed");
	std::int64_t TotalCapacity = 0;
	std::int64_t TotalMerged = 0;
	for (std::size_t Index = 0; Index < Existing.size(); ++Index)
	{
		const std::int64_t Quantity = Existing[Index];
		const std::int64_t Increment = Plan.StackIncrements[Index];
		Check(Increment >= 0, "negative merge");
		if (Quantity > 0 && Quantity <= Maximum)
		{
			TotalCapacity += Maximum - Quantity;
			Check(Quantity + Increment <= Maximum, "target exceeds stack maximum");
		}
		else
		{
			Check(Increment == 0, "malformed existing stack modified");
		}
		TotalMerged += Increment;
	}
	// Independent wide-arithmetic oracle: total usable capacity determines acceptance.
	const std::int64_t ExpectedMerged = std::min<std::int64_t>(Incoming, TotalCapacity);
	const std::int64_t ExpectedAccepted = bFreeFootprint ? Incoming : ExpectedMerged;
	Check(TotalMerged == ExpectedMerged, "available merge capacity was missed");
	Check(Plan.AcceptedQuantity == ExpectedAccepted, "accepted quantity differs from capacity oracle");
	Check(Plan.RemainderQuantity == Incoming - ExpectedAccepted, "incorrect overflow quantity");
	Check(TotalMerged + Plan.NewEntryQuantity == Plan.AcceptedQuantity, "inventory delta differs from acceptance");
	Check(static_cast<std::int64_t>(Plan.AcceptedQuantity) + Plan.RemainderQuantity == Incoming,
		"incoming quantity not conserved");
	Check(Plan.NewEntryQuantity == (bFreeFootprint ? Incoming - ExpectedMerged : 0), "incorrect new-entry quantity");
	Check(Plan.NewEntryQuantity >= 0 && Plan.NewEntryQuantity <= Maximum, "invalid new-entry size");
	Check(Plan.NewEntryQuantity == 0 || Plan.RemainderQuantity == 0, "new entry shares incoming GUID with overflow");
}

void TestNamedCases()
{
	{
		const FPlan Plan = PlanTransfer(5, 20, {}, true);
		VerifyValidPlan(5, 20, {}, true, Plan);
		Check(Plan.NewEntryQuantity == 5 && Plan.AcceptedQuantity == 5, "full acceptance into empty footprint");
		++Cases;
	}
	{
		const FPlan Plan = PlanTransfer(5, 20, {18}, false);
		VerifyValidPlan(5, 20, {18}, false, Plan);
		Check(Plan.StackIncrements == std::vector<std::int32_t>{2}
			&& Plan.AcceptedQuantity == 2 && Plan.RemainderQuantity == 3, "18/20 + 5 must leave 3");
		++Cases;
	}
	{
		const FPlan Plan = PlanTransfer(5, 20, {18}, true);
		VerifyValidPlan(5, 20, {18}, true, Plan);
		Check(Plan.StackIncrements == std::vector<std::int32_t>{2} && Plan.NewEntryQuantity == 3,
			"merge before inserting remainder");
		++Cases;
	}
	{
		const FPlan Partial = PlanTransfer(5, 20, {18, 19, 20}, false);
		VerifyValidPlan(5, 20, {18, 19, 20}, false, Partial);
		Check(Partial.StackIncrements == std::vector<std::int32_t>({2, 1, 0}) && Partial.RemainderQuantity == 2,
			"multiple stacks must fill in order");
		const FPlan Complete = PlanTransfer(5, 20, {18, 16, 1}, false);
		VerifyValidPlan(5, 20, {18, 16, 1}, false, Complete);
		Check(Complete.StackIncrements == std::vector<std::int32_t>({2, 3, 0}) && Complete.NewEntryQuantity == 0,
			"later targets must stay unchanged after all incoming units merge");
		++Cases;
	}
	{
		const FPlan Full = PlanTransfer(5, 20, {20, 20}, false);
		VerifyValidPlan(5, 20, {20, 20}, false, Full);
		Check(Full.AcceptedQuantity == 0 && Full.RemainderQuantity == 5, "full inventory is valid zero acceptance");
		const FPlan Empty = PlanTransfer(1, 1, {}, false);
		VerifyValidPlan(1, 1, {}, false, Empty);
		++Cases;
	}
	{
		const auto Min = std::numeric_limits<std::int32_t>::min();
		for (const auto& Input : std::vector<std::vector<std::int32_t>>{{0,20}, {-1,20}, {Min,20},
			{21,20}, {1,0}, {1,-1}, {1,Min}})
		{
			for (const bool bFree : {false, true})
			{
				const FPlan Plan = PlanTransfer(Input[0], Input[1], {1}, bFree);
				Check(!Plan.bValidInput && Plan.AcceptedQuantity == 0 && Plan.NewEntryQuantity == 0,
					"invalid input must not transfer");
				Check(Plan.RemainderQuantity == Input[0] && Plan.StackIncrements.empty(),
					"invalid input must remain intact");
			}
		}
		++Cases;
	}
	{
		const std::vector<std::int32_t> Existing{0, -1, std::numeric_limits<std::int32_t>::min(), 21, 20, 18};
		const auto Original = Existing;
		const FPlan Plan = PlanTransfer(5, 20, Existing, false);
		VerifyValidPlan(5, 20, Existing, false, Plan);
		Check(Plan.StackIncrements == std::vector<std::int32_t>({0, 0, 0, 0, 0, 2}),
			"invalid existing quantities must not create artificial capacity");
		Check(Existing == Original, "planner changed its inputs");
		++Cases;
	}
	{
		const auto Max = std::numeric_limits<std::int32_t>::max();
		for (const bool bFree : {false, true})
		{
			VerifyValidPlan(Max, Max, {Max - 1, Max - 1}, bFree,
				PlanTransfer(Max, Max, {Max - 1, Max - 1}, bFree));
			VerifyValidPlan(Max, Max, {1, 1, 1}, bFree, PlanTransfer(Max, Max, {1, 1, 1}, bFree));
			VerifyValidPlan(Max, Max, {}, bFree, PlanTransfer(Max, Max, {}, bFree));
			VerifyValidPlan(1, 1, {1}, bFree, PlanTransfer(1, 1, {1}, bFree));
		}
		const FPlan Partial = PlanTransfer(Max, Max, {Max - 1, Max - 1}, false);
		Check(Partial.AcceptedQuantity == 2 && Partial.RemainderQuantity == Max - 2, "INT_MAX partial transfer");
		++Cases;
	}
}

void TestRandomizedConservation()
{
	std::mt19937 Random(0x10A070u);
	const auto Max = std::numeric_limits<std::int32_t>::max();
	for (int Iteration = 0; Iteration < 50000; ++Iteration)
	{
		const std::int32_t Maximum = Iteration % 7 == 0 ? Max
			: std::uniform_int_distribution<std::int32_t>(1, Max)(Random);
		const std::int32_t Incoming = std::uniform_int_distribution<std::int32_t>(1, Maximum)(Random);
		const auto Count = std::uniform_int_distribution<int>(0, 32)(Random);
		std::vector<std::int32_t> Existing;
		for (int Index = 0; Index < Count; ++Index)
		{
			switch (Random() % 6)
			{
			case 0: Existing.push_back(0); break;
			case 1: Existing.push_back(-1); break;
			case 2: Existing.push_back(Maximum < Max ? Maximum + 1 : Maximum); break;
			case 3: Existing.push_back(Maximum); break;
			default: Existing.push_back(std::uniform_int_distribution<std::int32_t>(1, Maximum)(Random)); break;
			}
		}
		const bool bFree = (Random() & 1u) != 0;
		VerifyValidPlan(Incoming, Maximum, Existing, bFree, PlanTransfer(Incoming, Maximum, Existing, bFree));
	}
	++Cases;
}
}

int main()
{
	try
	{
		TestNamedCases();
		TestRandomizedConservation();
		std::cout << "RESULT=LOOT_STACK_TRANSFER_PASSED cases=" << Cases << " randomized=50000 checks=" << Checks << '\n';
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "RESULT=LOOT_STACK_TRANSFER_FAILED " << Error.what() << '\n';
		return 1;
	}
}
