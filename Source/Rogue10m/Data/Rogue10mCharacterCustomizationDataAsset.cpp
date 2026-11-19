// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterCustomizationDataAsset.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "Rogue10mCharacter.h"

namespace
{
	const FName SkinColorParameter(TEXT("RogueSkinColor"));
	constexpr FColor SkinColorPresets[] =
	{
		FColor(0xFF, 0xDF, 0xC3),
		FColor(0xEE, 0xC1, 0xA2),
		FColor(0xD6, 0xA0, 0x7A),
		FColor(0xBA, 0x7D, 0x59),
		FColor(0xA0, 0x63, 0x48),
		FColor(0x8B, 0x54, 0x3D),
		FColor(0x61, 0x3B, 0x31),
		FColor(0x81, 0x98, 0x62)
	};
	constexpr FColor HumanNaturalColors[] =
	{
		FColor(0xC8, 0x90, 0x70),
		FColor(0xDE, 0xA8, 0x88),
		FColor(0xA5, 0x69, 0x50),
		FColor(0x7D, 0x4C, 0x3C)
	};
	constexpr FColor DwarfNaturalColor(0xAE, 0x75, 0x51);
	constexpr FColor OrcNaturalColors[] =
	{
		FColor(0x74, 0x92, 0x4C),
		FColor(0x6F, 0x83, 0x5D),
		FColor(0x85, 0x77, 0x55)
	};

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

	void ApplySkinMaterial(
		USkeletalMeshComponent& MeshComponent, FName SlotName,
		UMaterialInterface* Variant, const FLinearColor& SkinColor)
	{
		const int32 SlotIndex = MeshComponent.GetMaterialIndex(SlotName);
		if (SlotIndex == INDEX_NONE)
		{
			return;
		}

		UMaterialInterface* Source = Variant ? Variant
			: MeshComponent.GetSkeletalMeshAsset()->GetMaterials()[SlotIndex].MaterialInterface.Get();
		UMaterialInstanceDynamic* Instance =
			Cast<UMaterialInstanceDynamic>(MeshComponent.GetMaterial(SlotIndex));
		if (!Instance || Instance->GetOuter() != &MeshComponent || Instance->Parent != Source)
		{
			FLinearColor DefaultColor;
			if (!Source || !Source->GetVectorParameterValue(
				FHashedMaterialParameterInfo(SkinColorParameter), DefaultColor))
			{
				ApplyMaterial(&MeshComponent, SlotName, Variant);
				return;
			}
			Instance = UMaterialInstanceDynamic::Create(Source, &MeshComponent);
			MeshComponent.SetMaterial(SlotIndex, Instance);
		}
		Instance->SetVectorParameterValue(SkinColorParameter, SkinColor);
	}

	template <typename ObjectType>
	ObjectType* LoadIndexed(const TArray<TSoftObjectPtr<ObjectType>>& Options, int32 Index)
	{
		return Options.IsValidIndex(Index) ? Options[Index].LoadSynchronous() : nullptr;
	}
}

TConstArrayView<FColor> URogue10mCharacterCustomizationDataAsset::GetSkinColorPresets()
{
	return MakeArrayView(SkinColorPresets);
}

FColor URogue10mCharacterCustomizationDataAsset::GetSkinColor(const FRogue10mCharacterAppearance& Appearance)
{
	if (Appearance.bUseCustomSkinColor)
	{
		const FColor& Color = Appearance.CustomSkinColor;
		return FColor(Color.R, Color.G, Color.B);
	}

	switch (Appearance.Race)
	{
	case ERogue10mCharacterRace::Dwarf:
		return DwarfNaturalColor;
	case ERogue10mCharacterRace::Orc:
		return OrcNaturalColors[
			NormalizeRequiredIndex(Appearance.SkinToneIndex, UE_ARRAY_COUNT(OrcNaturalColors))];
	default:
		return HumanNaturalColors[
			NormalizeRequiredIndex(Appearance.SkinToneIndex, UE_ARRAY_COUNT(HumanNaturalColors))];
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
	Result.CustomSkinColor.A = 255;
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

	if (BodyMesh->GetSkeletalMeshAsset() != LoadedBody)
	{
		if (!bSetBodyMesh)
		{
			return false;
		}
		BodyMesh->SetSkeletalMeshAsset(LoadedBody);
		BodyMesh->SetAnimInstanceClass(nullptr);
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}

	const int32 HeadMaterialIndex =
		Normalized.HeadStyleIndex * Archetype->SkinToneCount + Normalized.SkinToneIndex;
	const FLinearColor SkinColor = FLinearColor::FromSRGBColor(GetSkinColor(Normalized));
	ApplySkinMaterial(
		*BodyMesh, Archetype->HeadMaterialSlot,
		LoadIndexed(Archetype->HeadMaterials, HeadMaterialIndex), SkinColor);
	ApplyMaterial(
		BodyMesh, Archetype->EyeMaterialSlot,
		LoadIndexed(Archetype->EyeMaterials, Normalized.EyeColorIndex));
	ApplySkinMaterial(
		*BodyMesh, Archetype->BodyMaterialSlot,
		LoadIndexed(Archetype->BodyDetailMaterials, Normalized.BodyDetailIndex), SkinColor);

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
