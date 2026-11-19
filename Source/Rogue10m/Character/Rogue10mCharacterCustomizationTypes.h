// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rogue10mCharacterCustomizationTypes.generated.h"

UENUM(BlueprintType)
enum class ERogue10mCharacterRace : uint8
{
	Human UMETA(DisplayName="인간"),
	Dwarf UMETA(DisplayName="드워프"),
	Orc UMETA(DisplayName="오크")
};

UENUM(BlueprintType)
enum class ERogue10mCharacterGender : uint8
{
	Male UMETA(DisplayName="남성"),
	Female UMETA(DisplayName="여성")
};

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mCharacterAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	ERogue10mCharacterRace Race = ERogue10mCharacterRace::Human;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	ERogue10mCharacterGender Gender = ERogue10mCharacterGender::Male;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 HeadStyleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 SkinToneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Rogue10m|Character Customization")
	bool bUseCustomSkinColor = false;

	/** Opaque sRGB color, used when bUseCustomSkinColor is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Rogue10m|Character Customization")
	FColor CustomSkinColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 HairStyleIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 HairColorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 FacialStyleIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 EyeColorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rogue10m|Character Customization")
	int32 BodyDetailIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mCharacterProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Rogue10m|Character Profile")
	FGuid ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Rogue10m|Character Profile")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Rogue10m|Character Profile")
	FRogue10mCharacterAppearance Appearance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Rogue10m|Character Profile")
	FDateTime CreatedAt;

	bool IsValid() const
	{
		return ProfileId.IsValid() && !CharacterName.TrimStartAndEnd().IsEmpty();
	}
};
