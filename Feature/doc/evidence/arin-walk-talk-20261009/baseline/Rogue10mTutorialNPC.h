// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mTutorialNPC.generated.h"

class ARogue10mCharacter;
class APlayerController;
class UPrimitiveComponent;
class USphereComponent;
class USkeletalMeshComponent;
class URogue10mTutorialDataAsset;
class URogue10mTutorialRewardComponent;
class URogue10mTutorialWidget;

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

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Tutorial")
	void CloseDialogue();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Rogue10m|Tutorial")
	void ApplyGuideAppearance();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	bool HasClaimedReward(ARogue10mCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	bool IsDialogueOpen() const { return bDialogueOpen; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	FText GetDialogueStatus() const { return DialogueStatus; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	URogue10mTutorialWidget* GetTutorialWidget() const { return TutorialWidget; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	TObjectPtr<URogue10mTutorialDataAsset> TutorialData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial", meta=(ClampMin="100.0", Units="cm"))
	float InteractionRadius = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial", meta=(ClampMin="150.0", Units="cm"))
	float DetectionRadius = 650.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> GuideMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const URogue10mTutorialDataAsset& GetDialogueData() const;
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
	FText DialogueStatus;
	TWeakObjectPtr<ARogue10mCharacter> NearbyCharacter;
	TWeakObjectPtr<APlayerController> NearbyController;
	FTimerHandle NearbyTimer;
	bool bDialogueOpen = false;
	bool bRewardReceivedThisDialogue = false;
	bool bInteractBound = false;
	bool bInputEnabled = false;
};
