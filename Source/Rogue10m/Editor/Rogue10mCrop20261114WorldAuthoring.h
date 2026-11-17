// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mCrop20261114WorldAuthoring.generated.h"

/** Read-only terrain inspection and candidate-only editor selection; never crops or saves a world. */
UCLASS()
class ROGUE10M_API URogue10mCrop20261114WorldAuthoring : public UBlueprintFunctionLibrary
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

	/** Reads retained height/paint samples with Core BLAKE3. Does not edit layers, textures, selection, or packages. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString SnapshotRetainedTerrainData();

	/** Reads raw B/A samples and scoped terrain dependencies; never imports, exports, deletes, or saves. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString InspectCandidateDependencies();

	/** Returns retained CPU-source tile bytes as JSON/base64. No file I/O, asset edits, import, or save. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Compact World", meta=(DevelopmentOnly))
	static FString ExportCandidateRetainedTiles();
};

