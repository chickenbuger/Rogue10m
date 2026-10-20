// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mGameMode.generated.h"

class ARogue10mCharacter;
class ARogue10mGameState;
class APawn;
class APlayerController;
class AHUD;
class URogue10mCharacterCustomizationDataAsset;
class URogue10mRunCompletionComponent;
enum class ERogue10mRunPhase : uint8;

/**
 *  GameMode for the single-player roguelike run.
 */
UCLASS()
class ARogue10mGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogue10mGameMode();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Run")
	URogue10mRunCompletionComponent* GetRunCompletionComponent() const { return RunCompletionComponent; }

	ARogue10mCharacter* SpawnSelectedCharacterForController(
		AController* Controller);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rogue10m|Run")
	TObjectPtr<URogue10mRunCompletionComponent> RunCompletionComponent;

	virtual void StartPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(
		AController* InController) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character")
	TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset> CustomizationCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Run")
	bool bStartRunOnStartPlay = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Prototype")
	bool bUsePrototypeRunDuration = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Prototype", meta=(EditCondition="bUsePrototypeRunDuration", ClampMin="1.0"))
	float PrototypeRunDurationSeconds = 30.0f;

private:
	UFUNCTION()
	void HandleRunPhaseChanged(ERogue10mRunPhase NewPhase);

	bool ShouldStartRunFromTravelOptions() const;
	void StartConfiguredRun(ARogue10mGameState& RogueGameState) const;
	void HandleRunDefeat();
	TSubclassOf<ARogue10mCharacter> ResolveSelectedCharacterClass() const;
};



