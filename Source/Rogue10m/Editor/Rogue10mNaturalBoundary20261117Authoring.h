// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mNaturalBoundary20261117Authoring.generated.h"

/** Exact-candidate editor authoring. Never replaces a Landscape or saves all dirty packages. */
UCLASS()
class ROGUE10M_API URogue10mNaturalBoundary20261117Authoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the registered retained45 lattice, original-layer hash and composite grid. No loading or editing. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Natural Boundary", meta=(DevelopmentOnly))
	static FString InspectCandidate(bool bIncludeHeights = false);

	/** Read-only simple ECC_Pawn sweep of an upright capsule; endpoints are capsule centers in world cm. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Natural Boundary", meta=(DevelopmentOnly))
	static FString ProbePawnCapsule(const FVector& StartCm, const FVector& EndCm, float RadiusCm = 42.0f, float HalfHeightCm = 96.0f);

	/** Validates the entire JSON grid before creating one additive layer. Leaves changes unsaved for review. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Natural Boundary", meta=(DevelopmentOnly))
	static FString ApplyHeightPlan(const FString& HeightPlanJson, bool bConfirm = false);

	/** Revises only the existing named additive layer; validates the full grid against the original raw layer first. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Natural Boundary", meta=(DevelopmentOnly))
	static FString ReviseHeightPlan(const FString& HeightRevisionJson, bool bConfirm = false);

	/** Saves only the supplied, loaded candidate/owned packages after exact dirty-set and height-hash guards. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Natural Boundary", meta=(DevelopmentOnly))
	static FString SaveCandidatePackages(const FString& SavePlanJson);
};
