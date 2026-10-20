// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mGameMode.h"

#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterCustomizationDataAsset.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ARogue10mGameMode::ARogue10mGameMode()
{
	GameStateClass = ARogue10mGameState::StaticClass();
	HUDClass = AHUD::StaticClass();
	PlayerStateClass = ARogue10mPlayerState::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> FirstPersonCharacterClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	if (FirstPersonCharacterClass.Succeeded())
	{
		DefaultPawnClass = FirstPersonCharacterClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> FirstPersonPlayerControllerClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	if (FirstPersonPlayerControllerClass.Succeeded())
	{
		PlayerControllerClass = FirstPersonPlayerControllerClass.Class;
	}

	CustomizationCatalog =
		TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset>(
			FSoftObjectPath(TEXT("/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog")));
}

void ARogue10mGameMode::StartPlay()
{
	Super::StartPlay();

	if (ARogue10mGameState* RogueGameState = GetGameState<ARogue10mGameState>())
	{
		RogueGameState->OnRunPhaseChanged.AddDynamic(this, &ARogue10mGameMode::HandleRunPhaseChanged);
		if (bStartRunOnStartPlay || ShouldStartRunFromTravelOptions())
		{
			StartConfiguredRun(*RogueGameState);
		}
	}
}

UClass* ARogue10mGameMode::GetDefaultPawnClassForController_Implementation(
	AController* InController)
{
	if (const TSubclassOf<ARogue10mCharacter> SelectedClass =
		ResolveSelectedCharacterClass())
	{
		return SelectedClass.Get();
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

ARogue10mCharacter* ARogue10mGameMode::SpawnSelectedCharacterForController(
	AController* Controller)
{
	if (!Controller || !GetWorld())
	{
		return nullptr;
	}

	const TSubclassOf<ARogue10mCharacter> SelectedClass =
		ResolveSelectedCharacterClass();
	if (!SelectedClass)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("선택 프로필에 대응하는 상속 Character 클래스를 찾지 못했습니다."));
		return nullptr;
	}

	ARogue10mCharacter* PreviousCharacter =
		Cast<ARogue10mCharacter>(Controller->GetPawn());
	if (PreviousCharacter && PreviousCharacter->IsA(SelectedClass))
	{
		const UGameInstance* Instance = GetGameInstance();
		const URogue10mCharacterProfileSubsystem* SelectedProfiles = Instance
			? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		const FRogue10mCharacterProfile* Selected = SelectedProfiles ? SelectedProfiles->GetSelectedProfile() : nullptr;
		const URogue10mProgressionSubsystem* Progression = Instance
			? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		if (Selected && Progression && Progression->IsAttachedTo(*PreviousCharacter)
			&& Progression->GetAttachedProfileId() == Selected->ProfileId)
		{
			return PreviousCharacter;
		}
	}

	if (PreviousCharacter && GetGameInstance())
	{
		URogue10mProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		if (Progression && Progression->IsAttachedTo(*PreviousCharacter))
		{
			if (!Progression->SaveNow())
			{
				UE_LOG(LogRogue10m, Error, TEXT("Character switch stopped: the previous profile could not be saved."));
				return nullptr;
			}
			// Spawning the replacement may update the controller's shared PlayerState.
			// Snapshot and unbind the old avatar before constructing the new one.
			Progression->DetachCharacter(*PreviousCharacter, false);
		}
	}
	const FTransform SpawnTransform = PreviousCharacter
		? PreviousCharacter->GetActorTransform()
		: FTransform::Identity;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Controller;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogue10mCharacter* NewCharacter =
		GetWorld()->SpawnActor<ARogue10mCharacter>(
			SelectedClass, SpawnTransform, SpawnParameters);
	if (!NewCharacter)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("선택된 상속 Character Pawn 생성에 실패했습니다."));
		return nullptr;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URogue10mCharacterProfileSubsystem* Profiles = GameInstance
		? GameInstance->GetSubsystem<URogue10mCharacterProfileSubsystem>()
		: nullptr;
	const FRogue10mCharacterProfile* Profile =
		Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (!Profile || !NewCharacter->ApplyCharacterProfile(*Profile))
	{
		NewCharacter->Destroy();
		UE_LOG(
			LogRogue10m, Error,
			TEXT("새 상속 Character에 선택 프로필을 적용하지 못해 기존 Pawn을 유지합니다."));
		return nullptr;
	}

	Controller->Possess(NewCharacter);
	if (Controller->GetPawn() != NewCharacter)
	{
		NewCharacter->Destroy();
		UE_LOG(
			LogRogue10m, Error,
			TEXT("선택된 상속 Character Pawn Possess에 실패했습니다."));
		return nullptr;
	}

	if (PreviousCharacter)
	{
		PreviousCharacter->Destroy();
	}
	return NewCharacter;
}

TSubclassOf<ARogue10mCharacter>
ARogue10mGameMode::ResolveSelectedCharacterClass() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const URogue10mCharacterProfileSubsystem* Profiles = GameInstance
		? GameInstance->GetSubsystem<URogue10mCharacterProfileSubsystem>()
		: nullptr;
	const FRogue10mCharacterProfile* Profile =
		Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (!Profile)
	{
		return nullptr;
	}

	URogue10mCharacterCustomizationDataAsset* Catalog =
		CustomizationCatalog.LoadSynchronous();
	if (!Catalog)
	{
		Catalog = LoadObject<URogue10mCharacterCustomizationDataAsset>(
			nullptr,
			TEXT("/Game/DataAsset/Character/Customization/"
				"DA_CharacterCustomizationCatalog."
				"DA_CharacterCustomizationCatalog"));
	}
	if (Catalog)
	{
		if (const TSubclassOf<ARogue10mCharacter> CatalogClass =
			Catalog->LoadCharacterClass(Profile->Appearance))
		{
			return CatalogClass;
		}
	}

	const bool bFemale =
		Profile->Appearance.Gender == ERogue10mCharacterGender::Female;
	const TCHAR* CharacterAssetName = nullptr;
	switch (Profile->Appearance.Race)
	{
	case ERogue10mCharacterRace::Human:
		CharacterAssetName = bFemale
			? TEXT("BP_Rogue10m_HumanFemaleCharacter")
			: TEXT("BP_Rogue10m_HumanMaleCharacter");
		break;
	case ERogue10mCharacterRace::Dwarf:
		CharacterAssetName = bFemale
			? TEXT("BP_Rogue10m_DwarfFemaleCharacter")
			: TEXT("BP_Rogue10m_DwarfMaleCharacter");
		break;
	case ERogue10mCharacterRace::Orc:
		CharacterAssetName = bFemale
			? TEXT("BP_Rogue10m_OrcFemaleCharacter")
			: TEXT("BP_Rogue10m_OrcMaleCharacter");
		break;
	default:
		break;
	}

	if (!CharacterAssetName)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("선택 프로필의 종족·성별 값이 유효하지 않습니다."));
		return nullptr;
	}

	const FString CharacterClassPath = FString::Printf(
		TEXT("/Game/Character/Customization/Characters/%s.%s_C"),
		CharacterAssetName,
		CharacterAssetName);
	UClass* FallbackClass = LoadClass<ARogue10mCharacter>(
		nullptr, *CharacterClassPath);
	if (!FallbackClass)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("카탈로그와 fallback 경로 모두 상속 Character 클래스를 "
				"불러오지 못했습니다: %s"),
			*CharacterClassPath);
	}
	else
	{
		UE_LOG(
			LogRogue10m, Warning,
			TEXT("카탈로그 CharacterClass를 불러오지 못해 fallback을 "
				"사용합니다: %s"),
			*CharacterClassPath);
	}
	return FallbackClass;
}

bool ARogue10mGameMode::ShouldStartRunFromTravelOptions() const
{
	const UWorld* World = GetWorld();
	return World && World->URL.HasOption(TEXT("StartRun"));
}

void ARogue10mGameMode::StartConfiguredRun(ARogue10mGameState& RogueGameState) const
{
	if (bUsePrototypeRunDuration)
	{
		RogueGameState.StartRunWithDuration(PrototypeRunDurationSeconds);
	}
	else
	{
		RogueGameState.StartRun();
	}
}

void ARogue10mGameMode::HandleRunPhaseChanged(ERogue10mRunPhase NewPhase)
{
	if (NewPhase == ERogue10mRunPhase::Defeat)
	{
		HandleRunDefeat();
	}
}

void ARogue10mGameMode::HandleRunDefeat()
{
	ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!RogueCharacter)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Run ended in defeat, but no Rogue10m character was found to kill."));
		return;
	}

	RogueCharacter->Die();
}
