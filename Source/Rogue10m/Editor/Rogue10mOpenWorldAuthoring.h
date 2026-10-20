// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mOpenWorldAuthoring.generated.h"

class UMaterialInterface;

/** Restricted editor authoring operations for the project's OpenRunMap Landscape. */
UCLASS()
class ROGUE10M_API URogue10mOpenWorldAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Imports the approved 1009-square RAW heightmap into an empty OpenRunMap. Does not save packages. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Open World", meta=(DevelopmentOnly))
	static FString CreateLandscapeFromHeightmap(const FString& HeightmapFile, UMaterialInterface* Material, UObject* AutoLayerInfo);

	/** Returns native Landscape and World Partition facts for the current OpenRunMap without modifying it. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Open World", meta=(DevelopmentOnly))
	static FString InspectOpenWorldTerrain();

	/** Loads the map's bounded editor region through the native World Partition region loader. Does not save packages. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Open World", meta=(DevelopmentOnly))
	static FString LoadOpenWorldEditorRegion();
};
