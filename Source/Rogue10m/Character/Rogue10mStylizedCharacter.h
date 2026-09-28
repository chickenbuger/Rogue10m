// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rogue10mCharacter.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mStylizedCharacter.generated.h"

class UAnimInstance;
class URogue10mCharacterCustomizationDataAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * 종족·성별 Blueprint가 상속하는 외형 Pawn 기반 클래스입니다.
 * 숨겨진 Manny는 애니메이션 소스로만 사용하고 실제 GetMesh()는 자기 Skeleton의
 * Retarget AnimBP를 평가합니다.
 */
UCLASS(Abstract, Blueprintable)
class ROGUE10M_API ARogue10mStylizedCharacter : public ARogue10mCharacter
{
	GENERATED_BODY()

public:
	ARogue10mStylizedCharacter();

	virtual bool ApplyCharacterProfile(
		const FRogue10mCharacterProfile& Profile) override;
	virtual USkeletalMeshComponent* GetAnimationPlaybackMesh() const override
	{
		return AnimationSourceMesh;
	}

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character|Customization")
	ERogue10mCharacterRace GetAppearanceRace() const { return AppearanceRace; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character|Customization")
	ERogue10mCharacterGender GetAppearanceGender() const { return AppearanceGender; }

	USkeletalMeshComponent* GetAnimationSourceMesh() const
	{
		return AnimationSourceMesh;
	}

protected:
	virtual void PostInitializeComponents() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Customization")
	TObjectPtr<USkeletalMeshComponent> AnimationSourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Customization")
	TObjectPtr<USkeletalMeshComponent> AppearanceHairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Customization")
	TObjectPtr<USkeletalMeshComponent> AppearanceFacialMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Customization")
	ERogue10mCharacterRace AppearanceRace = ERogue10mCharacterRace::Human;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Customization")
	ERogue10mCharacterGender AppearanceGender = ERogue10mCharacterGender::Male;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Customization")
	TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset> CustomizationCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Appearance")
	TSoftObjectPtr<USkeletalMesh> AppearanceBodyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Appearance")
	TSoftClassPtr<UAnimInstance> AppearanceAnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Animation")
	TSoftObjectPtr<USkeletalMesh> AnimationSourceSkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character|Animation")
	TSoftClassPtr<UAnimInstance> AnimationSourceAnimClass;

private:
	bool InitializeInheritedAppearance();
	bool InitializeAnimationSource();
	bool IsMatchingProfile(const FRogue10mCharacterProfile& Profile) const;
};
