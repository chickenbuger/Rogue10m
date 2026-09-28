// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mStylizedCharacter.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rogue10m.h"
#include "Rogue10mCharacterCustomizationDataAsset.h"

ARogue10mStylizedCharacter::ARogue10mStylizedCharacter()
{
	AnimationSourceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Animation Source Mesh"));
	AnimationSourceMesh->SetupAttachment(GetCapsuleComponent());
	AnimationSourceMesh->SetRelativeLocationAndRotation(
		FVector(-20.0f, 0.0f, -96.0f),
		FRotator(0.0f, -90.0f, 0.0f));
	AnimationSourceMesh->SetCollisionProfileName(TEXT("NoCollision"));
	AnimationSourceMesh->SetGenerateOverlapEvents(false);
	AnimationSourceMesh->SetVisibility(false, false);
	AnimationSourceMesh->SetHiddenInGame(true, false);
	AnimationSourceMesh->VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	GetMesh()->SetupAttachment(AnimationSourceMesh);
	GetMesh()->SetRelativeTransform(FTransform::Identity);
	GetMesh()->VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->AddTickPrerequisiteComponent(AnimationSourceMesh);

	auto ConfigureAppearancePart = [this](USkeletalMeshComponent* MeshComponent)
	{
		MeshComponent->SetupAttachment(GetMesh());
		MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetOwnerNoSee(true);
		MeshComponent->FirstPersonPrimitiveType =
			EFirstPersonPrimitiveType::WorldSpaceRepresentation;
		MeshComponent->SetVisibility(false, false);
	};

	AppearanceHairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Appearance Hair Mesh"));
	ConfigureAppearancePart(AppearanceHairMesh);
	AppearanceFacialMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Appearance Facial Mesh"));
	ConfigureAppearancePart(AppearanceFacialMesh);

	CustomizationCatalog =
		TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset>(
			FSoftObjectPath(TEXT(
				"/Game/DataAsset/Character/Customization/"
				"DA_CharacterCustomizationCatalog")));
	AnimationSourceSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(
		FSoftObjectPath(TEXT(
			"/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")));
	AnimationSourceAnimClass = TSoftClassPtr<UAnimInstance>(
		FSoftClassPath(TEXT(
			"/Game/Rogue10m/Animation/Common/"
			"ABP_Common_Unarmed.ABP_Common_Unarmed_C")));
}

void ARogue10mStylizedCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	InitializeAnimationSource();
	InitializeInheritedAppearance();
}

bool ARogue10mStylizedCharacter::ApplyCharacterProfile(
	const FRogue10mCharacterProfile& Profile)
{
	if (!IsMatchingProfile(Profile))
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("프로필 종족·성별과 상속 Character 클래스가 일치하지 않습니다: %s"),
			*GetNameSafe(this));
		return false;
	}

	URogue10mCharacterCustomizationDataAsset* Catalog =
		CustomizationCatalog.LoadSynchronous();
	if (!Catalog)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("상속 Character가 커스터마이징 카탈로그를 불러오지 못했습니다."));
		return false;
	}

	const FRogue10mCharacterAppearance Normalized =
		Catalog->NormalizeAppearance(Profile.Appearance);
	if (!Catalog->ApplyAppearance(
		Normalized,
		GetMesh(),
		AppearanceHairMesh,
		AppearanceFacialMesh,
		false))
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("상속 Character의 외형 메시·머티리얼 적용에 실패했습니다: %s"),
			*GetNameSafe(this));
		return false;
	}

	GetMesh()->SetVisibility(true, false);
	return Super::ApplyCharacterProfile(Profile);
}

bool ARogue10mStylizedCharacter::InitializeAnimationSource()
{
	USkeletalMesh* SourceMesh = AnimationSourceSkeletalMesh.LoadSynchronous();
	UClass* SourceAnimClass = AnimationSourceAnimClass.LoadSynchronous();
	if (!AnimationSourceMesh || !SourceMesh || !SourceAnimClass)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("Manny 애니메이션 소스 메시 또는 AnimBP를 초기화하지 못했습니다."));
		return false;
	}

	AnimationSourceMesh->SetSkeletalMeshAsset(SourceMesh);
	AnimationSourceMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	AnimationSourceMesh->SetAnimInstanceClass(SourceAnimClass);
	AnimationSourceMesh->SetVisibility(false, false);
	AnimationSourceMesh->SetHiddenInGame(true, false);
	return true;
}

bool ARogue10mStylizedCharacter::InitializeInheritedAppearance()
{
	if (AppearanceBodyMesh.IsNull() && AppearanceAnimClass.IsNull())
	{
		return true;
	}

	USkeletalMesh* BodyMesh = AppearanceBodyMesh.LoadSynchronous();
	UClass* AnimClass = AppearanceAnimClass.LoadSynchronous();
	if (!BodyMesh || !AnimClass)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("상속 Character의 전신 메시 또는 Retarget AnimBP가 비어 있습니다: %s"),
			*GetNameSafe(this));
		return false;
	}

	GetMesh()->SetRelativeTransform(FTransform::Identity);
	GetMesh()->SetSkeletalMeshAsset(BodyMesh);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimClass);
	return true;
}

bool ARogue10mStylizedCharacter::IsMatchingProfile(
	const FRogue10mCharacterProfile& Profile) const
{
	return Profile.IsValid()
		&& Profile.Appearance.Race == AppearanceRace
		&& Profile.Appearance.Gender == AppearanceGender;
}
