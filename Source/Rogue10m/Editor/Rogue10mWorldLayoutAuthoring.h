// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mWorldLayoutAuthoring.generated.h"

class UMaterialInterface;

/** Candidate-only Landscape operations; never targets the production OpenRunMap. */
UCLASS()
class ROGUE10M_API URogue10mWorldLayoutAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Imports the exact approved R16 into the empty WorldLayout20261112 WP candidate. Does not save. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|World Layout", meta=(DevelopmentOnly))
	static FString CreateLandscapeFromHeightmap(const FString& HeightmapFile, UMaterialInterface* Material, UObject* AutoLayerInfo);

	/** Strict, read-only terrain facts. Call again after editor updates/region loading before accepting the import. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|World Layout", meta=(DevelopmentOnly))
	static FString InspectTerrain();

	/** Loads only the candidate's bounded WP editor region; no automatic save or level switch. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|World Layout", meta=(DevelopmentOnly))
	static FString LoadEditorRegion();
};
