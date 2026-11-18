#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Rogue10mVillageQuestActor.generated.h"

class ARogue10mCharacter;
class APlayerController;
class UStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;
class URogue10mVillageQuestDataAsset;

UENUM(BlueprintType)
enum class ERogue10mVillageQuestActorRole : uint8
{
	Board,
	Recovery
};

/** Optional in-world interaction. Never blocks roads, pauses, travels, or starts a run. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mVillageQuestActor : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mVillageQuestActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Same checks are applied to F input and programmatic callers. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Village Quest")
	bool Interact(ARogue10mCharacter* Character);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	ERogue10mVillageQuestActorRole Role = ERogue10mVillageQuestActorRole::Board;
	/** Null uses the shared native V1 definition; no per-actor progress is stored here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	TObjectPtr<URogue10mVillageQuestDataAsset> QuestDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quest")
	FName ObjectiveId = TEXT("V1_MissingSupply");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quest", meta=(ClampMin="100.0", ClampMax="400.0", Units="cm"))
	float InteractionRadius = 250.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Marker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTextRenderComponent> Prompt;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;
	URogue10mVillageQuestDataAsset* GetDefinition() const;
	bool CanInteract(const ARogue10mCharacter* Character) const;
	void RefreshNearbyPlayer();
	void SetInteractionInput(APlayerController* Controller);
	void HandleInteract();
	void RefreshPrompt(ARogue10mCharacter* Character);
	TWeakObjectPtr<ARogue10mCharacter> NearbyCharacter;
	TWeakObjectPtr<APlayerController> InputController;
	FTimerHandle ProximityTimer;
	bool bInputBound = false;
	FText LastFailure;
};
