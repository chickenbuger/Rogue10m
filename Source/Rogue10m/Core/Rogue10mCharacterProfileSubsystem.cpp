// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterProfileSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mCharacterProfileSaveGame.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void URogue10mCharacterProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if WITH_EDITOR
	FString TestSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), TestSlot))
	{
		bool bValid = TestSlot.StartsWith(TEXT("Rogue10m_Town_QA_")) && TestSlot.Len() <= 100;
		for (TCHAR Character : TestSlot) { bValid &= FChar::IsAlnum(Character) || Character == TCHAR('_'); }
		if (bValid) { SaveSlotName = TestSlot; }
		else { UE_LOG(LogRogue10m, Error, TEXT("Invalid isolated profile test slot; production slot remains selected.")); }
	}
#endif
	LoadProfiles();
}

const TArray<FRogue10mCharacterProfile>& URogue10mCharacterProfileSubsystem::GetProfiles() const
{
	static const TArray<FRogue10mCharacterProfile> EmptyProfiles;
	return SaveData ? SaveData->Profiles : EmptyProfiles;
}

const FRogue10mCharacterProfile* URogue10mCharacterProfileSubsystem::FindProfile(
	const FGuid& ProfileId) const
{
	return SaveData ? SaveData->Profiles.FindByPredicate(
		[&ProfileId](const FRogue10mCharacterProfile& Profile)
		{
			return Profile.ProfileId == ProfileId;
		}) : nullptr;
}

const FRogue10mCharacterProfile* URogue10mCharacterProfileSubsystem::GetSelectedProfile() const
{
	return SaveData ? FindProfile(SaveData->SelectedProfileId) : nullptr;
}

bool URogue10mCharacterProfileSubsystem::CreateProfile(
	const FString& CharacterName,
	const FRogue10mCharacterAppearance& Appearance,
	FRogue10mCharacterProfile& OutProfile)
{
	OutProfile = FRogue10mCharacterProfile();
	if (!SaveData)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "NoSaveData", "캐릭터 저장 데이터를 불러오지 못했습니다."));
		return false;
	}

	FString NormalizedName;
	if (!ValidateName(CharacterName, NormalizedName))
	{
		return false;
	}
	if (SaveData->Profiles.Num() >= MaximumProfileCount)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "SlotsFull", "캐릭터 슬롯이 모두 사용 중입니다."));
		return false;
	}
	if (SaveData->Profiles.ContainsByPredicate([&NormalizedName](const FRogue10mCharacterProfile& Profile)
		{
			return Profile.CharacterName.Equals(NormalizedName, ESearchCase::IgnoreCase);
		}))
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "DuplicateName", "이미 사용 중인 캐릭터 이름입니다."));
		return false;
	}

	const TArray<FRogue10mCharacterProfile> PreviousProfiles = SaveData->Profiles;
	const FGuid PreviousSelection = SaveData->SelectedProfileId;

	FRogue10mCharacterProfile NewProfile;
	NewProfile.ProfileId = FGuid::NewGuid();
	NewProfile.CharacterName = MoveTemp(NormalizedName);
	NewProfile.Appearance = Appearance;
	NewProfile.CreatedAt = FDateTime::UtcNow();
	SaveData->Profiles.Add(NewProfile);
	SaveData->SelectedProfileId = NewProfile.ProfileId;

	if (!SaveProfiles())
	{
		SaveData->Profiles = PreviousProfiles;
		SaveData->SelectedProfileId = PreviousSelection;
		return false;
	}

	OutProfile = NewProfile;
	LastError = FText::GetEmpty();
	return true;
}

bool URogue10mCharacterProfileSubsystem::DeleteProfile(const FGuid& ProfileId)
{
	if (!SaveData)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "NoSaveDataDelete", "캐릭터 저장 데이터를 불러오지 못했습니다."));
		return false;
	}

	const TArray<FRogue10mCharacterProfile> PreviousProfiles = SaveData->Profiles;
	const FGuid PreviousSelection = SaveData->SelectedProfileId;
	const int32 RemovedCount = SaveData->Profiles.RemoveAll(
		[&ProfileId](const FRogue10mCharacterProfile& Profile)
		{
			return Profile.ProfileId == ProfileId;
		});
	if (RemovedCount == 0)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "ProfileNotFoundDelete", "삭제할 캐릭터를 찾을 수 없습니다."));
		return false;
	}
	if (SaveData->SelectedProfileId == ProfileId)
	{
		SaveData->SelectedProfileId.Invalidate();
	}

	if (!SaveProfiles())
	{
		SaveData->Profiles = PreviousProfiles;
		SaveData->SelectedProfileId = PreviousSelection;
		return false;
	}

	LastError = FText::GetEmpty();
	return true;
}

bool URogue10mCharacterProfileSubsystem::SelectProfile(const FGuid& ProfileId)
{
	if (!SaveData || !FindProfile(ProfileId))
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "ProfileNotFoundSelect", "접속할 캐릭터를 찾을 수 없습니다."));
		return false;
	}

	const FGuid PreviousSelection = SaveData->SelectedProfileId;
	SaveData->SelectedProfileId = ProfileId;
	if (!SaveProfiles())
	{
		SaveData->SelectedProfileId = PreviousSelection;
		return false;
	}

	LastError = FText::GetEmpty();
	return true;
}

bool URogue10mCharacterProfileSubsystem::SaveProfiles()
{
	if (!SaveData || !UGameplayStatics::SaveGameToSlot(
		SaveData, SaveSlotName, SaveUserIndex))
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "SaveFailed", "캐릭터 정보를 저장하지 못했습니다."));
		UE_LOG(LogRogue10m, Error, TEXT("캐릭터 프로필 SaveGame 저장 실패: %s"), *SaveSlotName);
		return false;
	}
	return true;
}

int32 URogue10mCharacterProfileSubsystem::GetProfileCount() const
{
	return SaveData ? SaveData->Profiles.Num() : 0;
}

void URogue10mCharacterProfileSubsystem::LoadProfiles()
{
	SaveData = Cast<URogue10mCharacterProfileSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!SaveData)
	{
		SaveData = Cast<URogue10mCharacterProfileSaveGame>(
			UGameplayStatics::CreateSaveGameObject(
				URogue10mCharacterProfileSaveGame::StaticClass()));
	}
	if (!SaveData)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "CreateSaveFailed", "캐릭터 저장 데이터를 만들지 못했습니다."));
		return;
	}

	SaveData->Profiles.RemoveAll([](const FRogue10mCharacterProfile& Profile)
	{
		return !Profile.IsValid();
	});
	if (!FindProfile(SaveData->SelectedProfileId))
	{
		SaveData->SelectedProfileId.Invalidate();
	}
}

bool URogue10mCharacterProfileSubsystem::ValidateName(
	const FString& CharacterName, FString& OutNormalizedName)
{
	OutNormalizedName = CharacterName.TrimStartAndEnd();
	if (OutNormalizedName.Len() < 2 || OutNormalizedName.Len() > 12)
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "InvalidNameLength", "캐릭터 이름은 2~12자로 입력해 주세요."));
		return false;
	}
	if (OutNormalizedName.Contains(TEXT("\n")) || OutNormalizedName.Contains(TEXT("\r"))
		|| OutNormalizedName.Contains(TEXT("\t")))
	{
		SetError(NSLOCTEXT("Rogue10mCharacterProfile", "InvalidNameCharacters", "캐릭터 이름에 줄바꿈이나 탭을 사용할 수 없습니다."));
		return false;
	}
	return true;
}

void URogue10mCharacterProfileSubsystem::SetError(const FText& Error)
{
	LastError = Error;
}
