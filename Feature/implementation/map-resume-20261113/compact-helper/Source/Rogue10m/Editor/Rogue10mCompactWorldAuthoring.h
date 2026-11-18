// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mCompactWorldAuthoring.generated.h"

/** Read-only terrain inspection and candidate-only editor selection; never crops or saves a world. */
UCLASS()
class ROGUE10M_API URogue10mCompactWorldAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Inspects the exact original WorldLayout20261112 editor world without loading regions or changing data. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString InspectSourceTerrain();

	/** Inspects the exact compact candidate. Reports both the original and retained component layouts. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString InspectCandidateTerrain();

	/** Loads the candidate's full original Landscape editor region. Does not switch maps, alter terrain, or save. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString LoadCandidateRegion();

	/** Previews the exact outside set; true only permits editor selection after all original-layout guards pass. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString SelectOutsideComponents(bool bConfirmCandidateOnly = false);
};

