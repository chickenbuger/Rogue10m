// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Rogue10mGameMode.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mFiveVillageGameMode.generated.h"

class APlayerController;
class URogue10mFiveVillageProgressComponent;
class URogue10mFiveVillageRouteDataAsset;

/** Opt-in candidate mode. Inherits the production avatar, camera and run settlement. */
UCLASS()
class ROGUE10M_API ARogue10mFiveVillageGameMode : public ARogue10mGameMode
{
	GENERATED_BODY()

public:
	ARogue10mFiveVillageGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Five Village")
	URogue10mFiveVillageProgressComponent* GetRouteProgress(APlayerController* Controller) const;

protected:
	/** Null uses the project's five-village default definition; no asset is loaded in a constructor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Village")
	TSoftObjectPtr<URogue10mFiveVillageRouteDataAsset> RouteDefinition;

private:
	bool IsCandidateWorld() const;
};
