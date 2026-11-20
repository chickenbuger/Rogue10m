// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mRunPortal.generated.h"

class ARogue10mCharacter;
class ARogue10mPlayerController;
class UBoxComponent;
class URogue10mTutorialWidget;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mRunPortal : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mRunPortal();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Portal")
	bool BeginDepartureDialogue(ARogue10mCharacter* Character);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Portal")
	void SelectDepartureChoice(int32 Index);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Portal")
	bool IsDepartureDialogueOpen() const { return bDepartureDialogueOpen; }

	void NavigateDepartureChoice(int32 Delta);
	int32 GetSelectedDepartureChoice() const { return SelectedDepartureChoice; }
	void CloseDepartureDialogue();
	URogue10mTutorialWidget* GetDepartureWidget() const { return DepartureWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> PortalCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Portal")
	FName TargetLevelName = FName(TEXT("OpenRunMap"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Portal")
	bool bStartRunTimerOnTravel = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Portal")
	FString AdditionalTravelOptions;

private:
	UFUNCTION()
	void HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	ARogue10mPlayerController* FindNearbyController(ARogue10mCharacter* Character) const;
	bool CanUseDeparture() const;
	void UpdateNearbyPlayer();
	void ReleaseNearbyPlayer();
	void SetInteractionInputEnabled(bool bEnabled);
	void Interact();
	void RefreshDepartureView();
	void ConfirmDeparture(ARogue10mCharacter& Character);
	FString BuildTravelOptions() const;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mTutorialWidget> DepartureWidget;

	TWeakObjectPtr<ARogue10mCharacter> NearbyCharacter;
	TWeakObjectPtr<ARogue10mPlayerController> NearbyController;
	FTimerHandle NearbyTimer;
	FText DepartureStatus;
	int32 SelectedDepartureChoice = 0;
	bool bInteractionInputEnabled = false;
	bool bInteractBound = false;
	bool bDepartureDialogueOpen = false;
	bool bTravelStarted = false;
};
