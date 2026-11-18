// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mRecoveredTerrainAuthoring.generated.h"

/** Reads and applies a height-only recovery layer on the existing OpenRunMap Landscape. */
UCLASS()
class ROGUE10M_API URogue10mRecoveredTerrainAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the complete registered grid and optionally exports headerless little-endian R16 to project tmp. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Recovered Terrain", meta=(DevelopmentOnly))
	static FString InspectOpenRunMapHeightfield(const FString& ExportR16Path);

	/** Writes inclusive grid bounds to a new additive edit layer. Does not save packages. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Recovered Terrain", meta=(DevelopmentOnly))
	static FString ApplyOpenRunMapHeightPatch(const FString& TargetR16Path, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY,
		const FString& ExpectedLandscapeGuid, const FString& ExpectedCompositeBlake3);
};
