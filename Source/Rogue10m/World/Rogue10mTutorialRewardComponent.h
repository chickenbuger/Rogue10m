// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mTutorialRewardComponent.generated.h"

/** PlayerState에 붙는 현재 세션의 보급 수령 기록입니다. 영구 저장은 하지 않습니다. */
UCLASS()
class ROGUE10M_API URogue10mTutorialRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mTutorialRewardComponent();
	bool HasReceived(FName RewardId) const;
	bool TryReserve(FName RewardId);
	void FinishReservation(FName RewardId, bool bSucceeded);

private:
	UPROPERTY(Transient)
	TSet<FName> ReceivedRewards;
	UPROPERTY(Transient)
	TSet<FName> PendingRewards;
};
