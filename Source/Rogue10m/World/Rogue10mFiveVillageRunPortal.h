// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mFiveVillageRunPortal.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Separate hub entry; the original OpenRunMap portal is left untouched. */
UCLASS(Blueprintable)
class ROGUE10M_API ARogue10mFiveVillageRunPortal : public ARogue10mRunPortal
{
	GENERATED_BODY()

public:
	ARogue10mFiveVillageRunPortal();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> LeftPillar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> RightPillar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTextRenderComponent> RouteLabel;
};
