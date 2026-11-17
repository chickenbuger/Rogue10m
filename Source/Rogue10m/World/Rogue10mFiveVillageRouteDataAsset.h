#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mFiveVillageRouteDataAsset.generated.h"

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mFiveVillageLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Five Villages")
	FName VillageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Five Villages", meta=(Units="cm"))
	FVector VillageCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Five Villages", meta=(ClampMin="1.0", Units="cm"))
	float RadiusCm = 2800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Five Villages", meta=(ClampMin="0.0", Units="cm"))
	float HeightToleranceCm = 300.0f;
};

/** Route observations are tuning data, not travel or quest prerequisites. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mFiveVillageRouteDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	URogue10mFiveVillageRouteDataAsset();
	bool ValidateDefinition(FString& OutReason) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Villages")
	TArray<FRogue10mFiveVillageLocation> Villages;

	/** A proximity observation; this is not the authored boss entrance crossing line. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Villages|Boss Approach", meta=(Units="cm"))
	FVector BossApproachLocation = FVector(0.0, 30000.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Villages|Boss Approach", meta=(ClampMin="1.0", Units="cm"))
	float BossApproachRadiusCm = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Five Villages|Boss Approach", meta=(ClampMin="0.0", Units="cm"))
	float BossHeightToleranceCm = 300.0f;
};
