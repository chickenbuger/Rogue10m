#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mVillageQuestPresentation.h"
#include "Rogue10mVillageQuestDataAsset.generated.h"

class URogue10mItemDataAsset;
class URogue10mMonsterDataAsset;
class URogue10mTutorialDataAsset;
class UStaticMesh;
class UWorld;

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mVillageObjectivePresentation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal", meta=(ClampMin="1.0", Units="cm")) float RadiusCm = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") TObjectPtr<UStaticMesh> MarkerMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") FVector MarkerScale = FVector(.65, .65, .45);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") bool bShowMarker = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") bool bShowLabel = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal", meta=(Units="cm")) FVector LabelOffset = FVector(0, 0, 110);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") FRotator LabelRotation = FRotator(0, -90, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal", meta=(ClampMin="1.0", Units="cm")) float LabelWorldSize = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") FColor LabelColor = FColor(255, 220, 100);
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mVillageQuestDefinition
{
	GENERATED_BODY()
	/** Disabled draft slots are not validated, spawned or exposed to quest interactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity") FName VillageId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText VillageName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText NpcName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText InProgressText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText ReadyText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") FText CompletedText;
	/** Optional story fact. It never substitutes for a primary objective or reward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story", meta=(ClampMin="-1", ClampMax="1"))
	int32 SupplyClueObjectiveIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	bool bOffersClueAnalysis = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	bool bAcceptsClueReport = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") TArray<FText> ObjectiveLabels;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta=(Units="cm")) FVector NpcLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC") FRotator NpcRotation = FRotator::ZeroRotator;
	/** Existing guide mesh/animations can be selected without changing the quest dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC") TObjectPtr<URogue10mTutorialDataAsset> NpcPresentation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta=(ClampMin="100.0", Units="cm")) float NpcInteractionRadiusCm = 320.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta=(ClampMin="150.0", Units="cm")) float NpcDetectionRadiusCm = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal", meta=(ToolTip="Objective positions in centimetres.")) TArray<FVector> ObjectiveLocations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Goal") FRogue10mVillageObjectivePresentation ObjectivePresentation;
	/** INDEX_NONE for exploration; otherwise this objective requires an actual attributed kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="-1", ClampMax="1")) int32 CombatObjectiveIndex = INDEX_NONE;
	/** Required only for a combat objective. No asset is loaded during definition validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat") TSoftObjectPtr<URogue10mMonsterDataAsset> CombatMonsterData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reward", meta=(ClampMin="0")) int32 RewardGold = 0;
	/** Item instead of gold (RewardGold must be zero); full inventory retains the ready state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reward") TObjectPtr<URogue10mItemDataAsset> RewardItem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reward", meta=(ClampMin="1")) int32 RewardQuantity = 1;
};

/** Independent optional run quests. No travel, boss, persistent profile or skill prerequisite. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mVillageQuestDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	URogue10mVillageQuestDataAsset();
	bool ValidateDefinition(FString& OutReason) const;
	/** Structural editor check; asset loading and play validation remain separate. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Rogue10m|Village Quests|Validation")
	void ValidateConfiguration();
	/** Editor authoring only; replaces data with one collection quest and clears the map binding. No-op outside editor builds. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Rogue10m|Village Quests|Templates")
	void ResetToIntroSupplyExample();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Village Quests")
	int32 GetEnabledQuestCount() const;
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category="Rogue10m|Village Quests|Validation")
	bool bLastValidationPassed = false;
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category="Rogue10m|Village Quests|Validation")
	FText LastValidationResult;
	int32 FindQuestIndex(FName QuestId) const;
	/** Select a project map explicitly. Defaults retain only the legacy prototype candidate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|World")
	TSoftObjectPtr<UWorld> SupportedWorld;
	bool IsSupportedWorld(const UWorld* World) const;
	/** Local centimetres -> authored world anchor. Does not alter world size or movement speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Layout", meta=(Units="cm"))
	FVector LayoutOriginCm = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Layout")
	FRotator LayoutRotation = FRotator::ZeroRotator;
	FVector ToWorldLocation(const FVector& AuthoredLocation) const;
	FRotator ToWorldRotation(const FRotator& AuthoredRotation) const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FRogue10mVillageOptionalStoryText OptionalStoryText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests")
	TArray<FRogue10mVillageQuestDefinition> Quests;
};
