#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mVillageQuestDataAsset.generated.h"

class URogue10mItemDataAsset;

UENUM(BlueprintType)
enum class ERogue10mVillageQuestStatus : uint8 { Unaccepted, Accepted, Ready, Claiming, Claimed };

/** Definition only. Acceptance, recovered facts and claim receipts belong to one PlayerState/run. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mVillageQuestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	URogue10mVillageQuestDataAsset();
	bool ValidateDefinition(FString& OutReason) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	FName QuestId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	FText Title;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest", meta=(MultiLine="true"))
	FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	TArray<FName> ObjectiveIds;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	TSoftObjectPtr<URogue10mItemDataAsset> RewardItem;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Village Quest", meta=(ClampMin="1", ClampMax="99"))
	int32 RewardQuantity = 1;
};
