#include "Rogue10mVillageQuestDataAsset.h"

#include "Misc/PackageName.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mVillageQuestModel.h"

URogue10mVillageQuestDataAsset::URogue10mVillageQuestDataAsset()
{
	QuestId = TEXT("V1_SupplyRecovery");
	Title = FText::FromString(TEXT("잃어버린 보급품"));
	Description = FText::FromString(TEXT("돌샘촌(V3) 길가에서 잃어버린 보급품을 회수한 뒤 낙엽촌(V1) 보급 담당자에게 돌아가세요."));
	ObjectiveIds.Add(FName(TEXT("V1_MissingSupply")));
	RewardItem = TSoftObjectPtr<URogue10mItemDataAsset>(FSoftObjectPath(
		TEXT("/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.DA_Item_HealthPotion")));
}

bool URogue10mVillageQuestDataAsset::ValidateDefinition(FString& OutReason) const
{
	OutReason.Reset();
	if (QuestId.IsNone() || ObjectiveIds.IsEmpty() || ObjectiveIds.Num() > static_cast<int32>(Rogue10mVillageQuest::FQuestModel::MaxObjectives))
	{
		OutReason = TEXT("A quest needs an ID and between one and 32 objective IDs.");
		return false;
	}
	TSet<FName> Seen;
	for (FName Id : ObjectiveIds)
	{
		if (Id.IsNone() || Seen.Contains(Id))
		{
			OutReason = TEXT("Objective IDs must be nonempty and distinct.");
			return false;
		}
		Seen.Add(Id);
	}
	const FSoftObjectPath RewardPath = RewardItem.ToSoftObjectPath();
	if (RewardQuantity < 1 || RewardQuantity > 99 || RewardItem.IsNull() || !RewardPath.IsValid()
		|| !RewardPath.GetSubPathString().IsEmpty() || !FPackageName::IsValidObjectPath(RewardPath.ToString()))
	{
		OutReason = TEXT("Reward needs a valid item asset path and quantity from one to 99.");
		return false;
	}
	return true;
}
