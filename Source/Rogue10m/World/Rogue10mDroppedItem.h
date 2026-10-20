// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mDroppedItem.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class ARogue10mCharacter;

UCLASS()
class ROGUE10M_API ARogue10mDroppedItem : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mDroppedItem();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	void InitializeDroppedItem(const FRogue10mItemStack& NewItemStack);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Items")
	const FRogue10mItemStack& GetItemStack() const { return ItemStack; }
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	bool TryPickup(ARogue10mCharacter* Character);

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Items")
	TObjectPtr<UTextRenderComponent> ItemNameText;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Rogue10m|Items")
	FRogue10mItemStack ItemStack;
	bool bPickupInProgress = false;
};
