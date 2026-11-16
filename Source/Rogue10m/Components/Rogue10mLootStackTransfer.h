// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include <vector>

/** Engine-independent quantity planning used by the monster-loot inventory adapter. */
namespace Rogue10m::LootStackTransfer
{
struct FPlan
{
	bool bValidInput = false;
	std::int32_t AcceptedQuantity = 0;
	std::int32_t RemainderQuantity = 0;
	std::int32_t NewEntryQuantity = 0;
	std::vector<std::int32_t> StackIncrements;
};

/**
 * The adapter supplies only compatible grid stacks and an actual free footprint.
 * Invalid existing quantities are left untouched. A free footprint accepts all
 * residual quantity in one entry; an incoming stack never exceeds MaxStackSize.
 */
inline FPlan PlanTransfer(std::int32_t IncomingQuantity, std::int32_t MaxStackSize,
	const std::vector<std::int32_t>& ExistingQuantities, bool bHasFreeFootprint)
{
	FPlan Plan;
	Plan.RemainderQuantity = IncomingQuantity;
	if (MaxStackSize <= 0 || IncomingQuantity <= 0 || IncomingQuantity > MaxStackSize)
	{
		return Plan;
	}

	Plan.bValidInput = true;
	Plan.StackIncrements.resize(ExistingQuantities.size(), 0);
	for (std::size_t Index = 0; Index < ExistingQuantities.size() && Plan.RemainderQuantity > 0; ++Index)
	{
		const std::int32_t Quantity = ExistingQuantities[Index];
		if (Quantity <= 0 || Quantity >= MaxStackSize)
		{
			continue;
		}

		const std::int32_t Capacity = MaxStackSize - Quantity;
		const std::int32_t Increment = Capacity < Plan.RemainderQuantity ? Capacity : Plan.RemainderQuantity;
		Plan.StackIncrements[Index] = Increment;
		Plan.RemainderQuantity -= Increment;
	}

	if (bHasFreeFootprint)
	{
		Plan.NewEntryQuantity = Plan.RemainderQuantity;
		Plan.RemainderQuantity = 0;
	}
	// Derive accepted quantity without summing capacities or risking signed overflow.
	Plan.AcceptedQuantity = IncomingQuantity - Plan.RemainderQuantity;
	return Plan;
}
}
