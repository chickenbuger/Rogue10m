// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mTutorialNPC.generated.h"

class ARogue10mCharacter;
class APlayerController;
class UPrimitiveComponent;
class USphereComponent;
class USkeletalMeshComponent;
class URogue10mTutorialDataAsset;
class URogue10mTutorialAnimationComponent;
class URogue10mTutorialRewardComponent;
class URogue10mTutorialWidget;
class UInputComponent;
class URogue10mTownProgressionComponent;

/** F 대화와 출발 보급을 제공하며 전투/런 진행을 막지 않는 시작마을 안내인입니다. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mTutorialNPC : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mTutorialNPC();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Tutorial")
	bool BeginDialogue(ARogue10mCharacter* Character);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Tutorial")
	bool ClaimStartingSupplies();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool PurchasePotion();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool EnhanceEquippedItem(ERogue10mInventorySlotType SlotType);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool UpgradeTrait(ERogue10mTownTrait Trait);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits")
	bool OpenTraitWindow();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits")
	bool ResetTraits();
	/** The standalone trait UI may only operate on this NPC's live dialogue owner. */
	bool CanUseTraitWindow(FText& OutReason) const;
	const URogue10mTownServiceDataAsset* GetTraitServiceData() const { return &GetServiceData(); }
	/** Skill-tree browsing does not require dialogue; investment still requires this service range. */
	bool IsWithinSkillTreeServiceRange(const ARogue10mCharacter* Character) const;
	void ResumeAfterTraitWindow();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Town")
	bool SelectDifficulty(ERogue10mDifficulty Difficulty);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Tutorial")
	void CloseDialogue();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Rogue10m|Tutorial")
	void ApplyGuideAppearance();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	bool HasClaimedReward(ARogue10mCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	bool IsDialogueOpen() const { return bDialogueOpen; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	FText GetDialogueStatus() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	URogue10mTutorialWidget* GetTutorialWidget() const { return TutorialWidget; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	TObjectPtr<URogue10mTutorialDataAsset> TutorialData;

	/** Null uses C++ defaults, allowing existing Arin assets to offer services immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Town")
	TObjectPtr<URogue10mTownServiceDataAsset> ServiceData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial", meta=(ClampMin="100.0", Units="cm"))
	float InteractionRadius = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial", meta=(ClampMin="150.0", Units="cm"))
	float DetectionRadius = 650.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> GuideMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<URogue10mTutorialAnimationComponent> GuideAnimation;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const URogue10mTutorialDataAsset& GetDialogueData() const;
	const URogue10mTownServiceDataAsset& GetServiceData() const;
	bool CanUseTownServices();
	bool IsProgressionSaveAvailable();
	bool FinishService(bool bSucceeded, const FText& Success, const FText& Failure);
	URogue10mTownProgressionComponent* FindTownProgression() const;
	void SetServiceInputEnabled(bool bEnabled);
	void SelectPreviousService();
	void SelectNextService();
	void ExecuteSelectedService();
	FText BuildServiceText() const;
	bool IsPlayerAvailable(ARogue10mCharacter* Character, float Radius) const;
	void FindOverlappingPlayer();
	void ActivateNearbyPlayer(ARogue10mCharacter& Character);
	void ReleaseNearbyPlayer();
	void UpdateNearbyPlayer();
	void HandleInteract();
	void RefreshDialogueView();
	URogue10mTutorialRewardComponent* FindReceipts(ARogue10mCharacter* Character, bool bCreate) const;

	UFUNCTION()
	void HandleAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> DetectionArea;
	UPROPERTY(Transient)
	TObjectPtr<URogue10mTutorialWidget> TutorialWidget;
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> ServiceInputComponent;
	UPROPERTY(Transient)
	FText DialogueStatus;
	TWeakObjectPtr<ARogue10mCharacter> NearbyCharacter;
	TWeakObjectPtr<APlayerController> NearbyController;
	FTimerHandle NearbyTimer;
	bool bDialogueOpen = false;
	bool bRewardReceivedThisDialogue = false;
	bool bInteractBound = false;
	bool bInputEnabled = false;
	bool bServiceInputPushed = false;
	int32 SelectedServiceIndex = 0;
};
