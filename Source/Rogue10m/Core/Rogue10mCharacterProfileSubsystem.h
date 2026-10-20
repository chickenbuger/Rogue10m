// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "Rogue10mCharacterProfileSubsystem.generated.h"

class URogue10mCharacterProfileSaveGame;

UCLASS()
class ROGUE10M_API URogue10mCharacterProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaximumProfileCount = 3;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TArray<FRogue10mCharacterProfile>& GetProfiles() const;
	const FRogue10mCharacterProfile* FindProfile(const FGuid& ProfileId) const;
	const FRogue10mCharacterProfile* GetSelectedProfile() const;

	bool CreateProfile(
		const FString& CharacterName,
		const FRogue10mCharacterAppearance& Appearance,
		FRogue10mCharacterProfile& OutProfile);
	bool DeleteProfile(const FGuid& ProfileId);
	bool SelectProfile(const FGuid& ProfileId);
	bool SaveProfiles();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character Profile")
	int32 GetProfileCount() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character Profile")
	FText GetLastError() const { return LastError; }

private:
	void LoadProfiles();
	bool ValidateName(const FString& CharacterName, FString& OutNormalizedName);
	void SetError(const FText& Error);

	UPROPERTY(Transient)
	TObjectPtr<URogue10mCharacterProfileSaveGame> SaveData;

	UPROPERTY(Transient)
	FText LastError;

	FString SaveSlotName = TEXT("Rogue10mCharacterProfiles");
	static constexpr int32 SaveUserIndex = 0;
};
