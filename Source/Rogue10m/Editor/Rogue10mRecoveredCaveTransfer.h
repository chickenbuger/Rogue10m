// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Rogue10mRecoveredCaveTransfer.generated.h"

class AActor;

/** Transfers the four archived Q/Z cave actors without saving or creating a world. */
UCLASS()
class ROGUE10M_API URogue10mRecoveredCaveTransfer : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Cave Recovery", meta=(DevelopmentOnly))
	static FString ExportArchivedCaveActors();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Editor|Cave Recovery", meta=(DevelopmentOnly))
	static TArray<AActor*> PasteRecoveredCaveActors(const FString& ActorText, bool bConfirm = false);
};
