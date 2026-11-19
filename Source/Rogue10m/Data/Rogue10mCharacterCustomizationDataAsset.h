// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Engine/DataAsset.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mCharacterCustomizationDataAsset.generated.h"

class ARogue10mCharacter;
class UAnimInstance;
class UIKRetargeter;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mCharacterArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	ERogue10mCharacterRace Race = ERogue10mCharacterRace::Human;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	ERogue10mCharacterGender Gender = ERogue10mCharacterGender::Male;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TSoftClassPtr<ARogue10mCharacter> CharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TSoftObjectPtr<UIKRetargeter> Retargeter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TSoftClassPtr<UAnimInstance> RetargetAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization", meta=(ClampMin="1"))
	int32 HeadStyleCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization", meta=(ClampMin="1"))
	int32 SkinToneCount = 1;

	/** HeadStyleIndex * SkinToneCount + SkinToneIndex 순서입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<UMaterialInterface>> HeadMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<UMaterialInterface>> EyeMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<USkeletalMesh>> HairMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<UMaterialInterface>> HairMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<USkeletalMesh>> FacialMeshes;

	/** 수염처럼 색상 변경이 가능한 얼굴 파츠 Material입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<UMaterialInterface>> FacialMaterials;

	/** 문신처럼 Body Material Slot을 대체하는 옵션입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<TSoftObjectPtr<UMaterialInterface>> BodyDetailMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	FName BodyMaterialSlot = TEXT("M_Body");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	FName HeadMaterialSlot = TEXT("M_Head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	FName EyeMaterialSlot = TEXT("M_Eyes");
};

UCLASS(BlueprintType)
class ROGUE10M_API URogue10mCharacterCustomizationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static TConstArrayView<FColor> GetSkinColorPresets();
	static FColor GetSkinColor(const FRogue10mCharacterAppearance& Appearance);

	const FRogue10mCharacterArchetype* FindArchetype(
		ERogue10mCharacterRace Race, ERogue10mCharacterGender Gender) const;

	TSubclassOf<ARogue10mCharacter> LoadCharacterClass(
		const FRogue10mCharacterAppearance& Appearance) const;

	FRogue10mCharacterAppearance NormalizeAppearance(
		const FRogue10mCharacterAppearance& Appearance) const;

	bool ApplyAppearance(
		const FRogue10mCharacterAppearance& Appearance,
		USkeletalMeshComponent* BodyMesh,
		USkeletalMeshComponent* HairMesh,
		USkeletalMeshComponent* FacialMesh,
		bool bSetBodyMesh = true) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character Customization")
	TArray<FRogue10mCharacterArchetype> Archetypes;
};
