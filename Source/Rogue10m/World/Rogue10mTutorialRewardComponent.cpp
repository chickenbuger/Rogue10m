// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialRewardComponent.h"

URogue10mTutorialRewardComponent::URogue10mTutorialRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URogue10mTutorialRewardComponent::HasReceived(FName RewardId) const
{
	return ReceivedRewards.Contains(RewardId);
}

bool URogue10mTutorialRewardComponent::TryReserve(FName RewardId)
{
	if (RewardId.IsNone() || ReceivedRewards.Contains(RewardId) || PendingRewards.Contains(RewardId))
	{
		return false;
	}
	PendingRewards.Add(RewardId);
	return true;
}

void URogue10mTutorialRewardComponent::FinishReservation(FName RewardId, bool bSucceeded)
{
	if (PendingRewards.Remove(RewardId) > 0 && bSucceeded)
	{
		ReceivedRewards.Add(RewardId);
	}
}
