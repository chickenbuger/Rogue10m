#pragma once

#include "CoreMinimal.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mVillageQuestNPC.generated.h"

class URogue10mVillageQuestComponent;
class URogue10mVillageQuestDataAsset;
struct FRogue10mVillageQuestDefinition;

/** Existing F/Slate dialogue lifecycle with independent run-quest actions. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mVillageQuestNPC : public ARogue10mTutorialNPC
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") FName QuestId = TEXT("Q_V1_supply");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests") TObjectPtr<URogue10mVillageQuestDataAsset> QuestDefinition;
protected:
	virtual void BeginPlay() override;
	virtual void HandleInteract() override;
	virtual void RefreshDialogueView() override;
	virtual void ExecuteSelectedService() override;
	virtual FText BuildServiceText() const override;
	virtual TArray<FText> BuildServiceChoices() const override;
private:
	URogue10mVillageQuestComponent* FindQuests(bool bCreate) const;
	const FRogue10mVillageQuestDefinition* FindDefinition() const;
};
