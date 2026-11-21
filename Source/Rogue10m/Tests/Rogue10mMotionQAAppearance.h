#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "Rogue10mStylizedCharacter.h"

namespace Rogue10mMotionQA
{
struct FAppearance
{
	const TCHAR* Name;
	const TCHAR* MeshName;
	ERogue10mCharacterRace Race;
	ERogue10mCharacterGender Gender;

	FRogue10mCharacterAppearance MakeProfileAppearance() const
	{
		FRogue10mCharacterAppearance Appearance;
		Appearance.Race = Race;
		Appearance.Gender = Gender;
		return Appearance;
	}

	bool Matches(const ARogue10mCharacter* Character) const
	{
		const auto* Stylized = Cast<ARogue10mStylizedCharacter>(Character);
		const auto* Body = Stylized && Stylized->GetMesh() ? Stylized->GetMesh()->GetSkeletalMeshAsset() : nullptr;
		return Body && Body->GetFName() == FName(MeshName)
			&& Stylized->GetAppearanceRace() == Race && Stylized->GetAppearanceGender() == Gender;
	}
};

inline const FAppearance* FindAppearance(const FString& Name)
{
	using ERace = ERogue10mCharacterRace;
	using EGender = ERogue10mCharacterGender;
	static constexpr FAppearance Appearances[] = {
		{TEXT("HumanMale"), TEXT("SK_Hu_M_FullBody"), ERace::Human, EGender::Male},
		{TEXT("HumanFemale"), TEXT("SK_Hu_F_FullBody"), ERace::Human, EGender::Female},
		{TEXT("DwarfMale"), TEXT("SK_Dw_M_FullBody"), ERace::Dwarf, EGender::Male},
		{TEXT("DwarfFemale"), TEXT("SK_Dw_F_FullBody"), ERace::Dwarf, EGender::Female},
		{TEXT("OrcMale"), TEXT("SK_Or_M_FullBody"), ERace::Orc, EGender::Male},
		{TEXT("OrcFemale"), TEXT("SK_Or_F_FullBody"), ERace::Orc, EGender::Female},
	};
	for (const FAppearance& Appearance : Appearances)
	{
		if (Name == Appearance.Name) { return &Appearance; }
	}
	return nullptr;
}
}
