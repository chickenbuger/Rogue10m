#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Rogue10mVillageQuestRewardTestProxy.generated.h"

/** Reflected receiver for the inventory's real dynamic multicast delegates. */
UCLASS(Transient, NotBlueprintable)
class URogue10mVillageQuestRewardTestProxy final : public UObject
{
	GENERATED_BODY()
public:
	// Keep the class and inline function defined in every target UHT generates.
	// Only development automation installs or invokes the test callback.
	TFunction<void()> OnNotification;
	UFUNCTION()
	void HandleInventoryChanged()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (OnNotification) { OnNotification(); }
#endif
	}
};
