// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterCustomizationDataAsset.h"

#include "Components/SkeletalMeshComponent.h"
#include "Rogue10mCharacter.h"
#include "Materials/MaterialInterface.h"

namespace
{
	int32 NormalizeRequiredIndex(int32 Value, int32 Count)
	{
		return Count > 0 ? FMath::Clamp(Value, 0, Count - 1) : 0;
	}

	int32 NormalizeOptionalIndex(int32 Value, int32 Count)
	{
		return Count > 0 ? FMath::Clamp(Value, INDEX_NONE, Count - 1) : INDEX_NONE;
	}

	void ApplyMaterial(
		USkeletalMeshComponent* MeshComponent, FName SlotName, UMaterialInterface* Material)
	{
		if (!MeshComponent)
		{
			return;
		}

		int32 MaterialIndex = MeshComponent->GetMaterialIndex(SlotName);
		if (MaterialIndex == INDEX_NONE)
		{
			if (SlotName == TEXT("M_Eyes"))
			{
				MaterialIndex = MeshComponent->GetMaterialIndex(TEXT("M_Eye"));
			}
			else if (SlotName == TEXT("M_Eye"))
			{
				MaterialIndex = MeshComponent->GetMaterialIndex(TEXT("M_Eyes"));
			}
		}
		if (MaterialIndex != INDEX_NONE && (Material || MaterialIndex < MeshComponent->GetNumOverrideMaterials()))
		{
			// No variant means the current mesh should supply this slot's material.
			MeshComponent->SetMaterial(MaterialIndex, Material);
		}
	}

	template <typename ObjectType>
	ObjectType* LoadIndexed(const TArray<TSoftObjectPtr<ObjectType>>& Options, int32 Index)
	{
		return Options.IsValidIndex(Index) ? Options[Index].LoadSynchronous() : nullptr;
	}
}

FPrimaryAssetId URogue10mCharacterCustomizationDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("CharacterCustomization"), GetFName());
}

const FRogue10mCharacterArchetype* URogue10mCharacterCustomizationDataAsset::FindArchetype(
	ERogue10mCharacterRace Race, ERogue10mCharacterGender Gender) const
{
	return Archetypes.FindByPredicate([Race, Gender](const FRogue10mCharacterArchetype& Archetype)
	{
		return Archetype.Race == Race && Archetype.Gender == Gender;
	});
}

TSubclassOf<ARogue10mCharacter>
URogue10mCharacterCustomizationDataAsset::LoadCharacterClass(
	const FRogue10mCharacterAppearance& Appearance) const
{
	const FRogue10mCharacterAppearance Normalized = NormalizeAppearance(Appearance);
	const FRogue10mCharacterArchetype* Archetype =
		FindArchetype(Normalized.Race, Normalized.Gender);
	return Archetype ? Archetype->CharacterClass.LoadSynchronous() : nullptr;
}

FRogue10mCharacterAppearance URogue10mCharacterCustomizationDataAsset::NormalizeAppearance(
	const FRogue10mCharacterAppearance& Appearance) const
{
	FRogue10mCharacterAppearance Result = Appearance;
	const FRogue10mCharacterArchetype* Archetype =
		FindArchetype(Result.Race, Result.Gender);
	if (!Archetype)
	{
		Result.Race = ERogue10mCharacterRace::Human;
		Result.Gender = ERogue10mCharacterGender::Male;
		Archetype = FindArchetype(Result.Race, Result.Gender);
	}
	if (!Archetype)
	{
		return Result;
	}

	Result.HeadStyleIndex = NormalizeRequiredIndex(Result.HeadStyleIndex, Archetype->HeadStyleCount);
	Result.SkinToneIndex = NormalizeRequiredIndex(Result.SkinToneIndex, Archetype->SkinToneCount);
	Result.HairStyleIndex = NormalizeOptionalIndex(Result.HairStyleIndex, Archetype->HairMeshes.Num());
	Result.HairColorIndex = NormalizeRequiredIndex(Result.HairColorIndex, Archetype->HairMaterials.Num());
	Result.FacialStyleIndex = NormalizeOptionalIndex(Result.FacialStyleIndex, Archetype->FacialMeshes.Num());
	Result.EyeColorIndex = NormalizeRequiredIndex(Result.EyeColorIndex, Archetype->EyeMaterials.Num());
	Result.BodyDetailIndex = NormalizeOptionalIndex(Result.BodyDetailIndex, Archetype->BodyDetailMaterials.Num());
	return Result;
}

bool URogue10mCharacterCustomizationDataAsset::ApplyAppearance(
	const FRogue10mCharacterAppearance& Appearance,
	USkeletalMeshComponent* BodyMesh,
	USkeletalMeshComponent* HairMesh,
	USkeletalMeshComponent* FacialMesh,
	bool bSetBodyMesh) const
{
	if (!BodyMesh)
	{
		return false;
	}

	const FRogue10mCharacterAppearance Normalized = NormalizeAppearance(Appearance);
	const FRogue10mCharacterArchetype* Archetype =
		FindArchetype(Normalized.Race, Normalized.Gender);
	USkeletalMesh* LoadedBody = Archetype ? Archetype->BodyMesh.LoadSynchronous() : nullptr;
	if (!Archetype || !LoadedBody)
	{
		return false;
	}

	if (bSetBodyMesh)
	{
		BodyMesh->SetSkeletalMeshAsset(LoadedBody);
		BodyMesh->SetAnimInstanceClass(nullptr);
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}
	else if (BodyMesh->GetSkeletalMeshAsset() != LoadedBody)
	{
		return false;
	}

	const int32 HeadMaterialIndex =
		Normalized.HeadStyleIndex * Archetype->SkinToneCount + Normalized.SkinToneIndex;
	ApplyMaterial(
		BodyMesh, Archetype->HeadMaterialSlot,
		LoadIndexed(Archetype->HeadMaterials, HeadMaterialIndex));
	ApplyMaterial(
		BodyMesh, Archetype->EyeMaterialSlot,
		LoadIndexed(Archetype->EyeMaterials, Normalized.EyeColorIndex));
	ApplyMaterial(
		BodyMesh, Archetype->BodyMaterialSlot,
		LoadIndexed(Archetype->BodyDetailMaterials, Normalized.BodyDetailIndex));

	auto ConfigurePart = [BodyMesh](USkeletalMeshComponent* PartMesh, USkeletalMesh* Mesh)
	{
		if (!PartMesh)
		{
			return;
		}
		PartMesh->SetSkeletalMeshAsset(Mesh);
		PartMesh->SetVisibility(Mesh != nullptr, false);
		if (Mesh)
		{
			PartMesh->SetLeaderPoseComponent(BodyMesh, true, false);
		}
	};

	ConfigurePart(HairMesh, LoadIndexed(Archetype->HairMeshes, Normalized.HairStyleIndex));
	if (HairMesh)
	{
		ApplyMaterial(HairMesh, HairMesh->GetMaterialSlotNames().IsEmpty()
			? NAME_None : HairMesh->GetMaterialSlotNames()[0],
			LoadIndexed(Archetype->HairMaterials, Normalized.HairColorIndex));
	}

	ConfigurePart(FacialMesh, LoadIndexed(Archetype->FacialMeshes, Normalized.FacialStyleIndex));
	if (FacialMesh)
	{
		ApplyMaterial(FacialMesh, FacialMesh->GetMaterialSlotNames().IsEmpty()
			? NAME_None : FacialMesh->GetMaterialSlotNames()[0],
			LoadIndexed(Archetype->FacialMaterials, Normalized.HairColorIndex));
	}

	return true;
}
