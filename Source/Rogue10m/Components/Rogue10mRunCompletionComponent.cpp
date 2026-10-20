// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mRunCompletionComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"

URogue10mRunCompletionComponent::URogue10mRunCompletionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TownMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Rogue10m/Maps/StartHub.StartHub")));
}

void URogue10mRunCompletionComponent::Initialize(ARogue10mGameState& State)
{
	RunState = &State;
	State.OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged);
}

void URogue10mRunCompletionComponent::NotifyFinalBossDefeated(
	ARogue10mBasicMonster& Boss, AController* RewardController, int32 Experience, int32 Gold)
{
	ARogue10mGameState* State = RunState.Get();
	ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(RewardController);
	const ARogue10mCharacter* Character = Controller ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
	const ARogue10mPlayerState* PlayerState = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const URogue10mMonsterDataAsset* Monster = Boss.GetMonsterData();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone
		|| !State || !State->IsRunActive() || State->GetRemainingRunSeconds() <= 0.0f || bSettled
		|| Boss.GetWorld() != GetWorld() || !Boss.IsDead() || Boss.IsCombatTestInstance()
		|| !Monster || Monster->MonsterRank != ERogue10mMonsterRank::FinalBoss
		|| !Controller || !Controller->IsLocalController() || !Character || Character->IsDead()
		|| !PlayerState || !PlayerState->GetRogueAttributeSet() || PlayerState->GetRogueAttributeSet()->GetHealth() <= 0.0f)
	{
		return;
	}
	ResultController = Controller;
	RewardExperience = FMath::Max(0, Experience);
	RewardGold = FMath::Max(0, Gold);
	State->FinishRun(true);
}

void URogue10mRunCompletionComponent::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (bSettled || (Phase != ERogue10mRunPhase::Victory && Phase != ERogue10mRunPhase::Defeat)
		|| !GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
	{
		return;
	}
	bSettled = true;
	bVictory = Phase == ERogue10mRunPhase::Victory;
	if (!ResultController.IsValid())
	{
		ResultController = Cast<ARogue10mPlayerController>(GetWorld()->GetFirstPlayerController());
	}
	if (ARogue10mPlayerController* Controller = ResultController.Get())
	{
		if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(Controller->GetPawn()))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
			if (URogue10mCombatComponent* Combat = Character->GetCombatComponent()) { Combat->CancelCombatVisuals(); }
		}
	}
	SaveSettlement();
	RefreshResult();
	UE_LOG(LogRogue10m, Log, TEXT("Run settled victory=%d boss_xp=%d boss_gold=%d saved=%d"),
		bVictory, RewardExperience, RewardGold, bProgressionSaved);
}

bool URogue10mRunCompletionComponent::SaveSettlement()
{
	ARogue10mPlayerController* Controller = ResultController.Get();
	const ARogue10mCharacter* Character = Controller ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	URogue10mProgressionSubsystem* Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	bProgressionSaved = Character && Progression && Progression->IsAttachedTo(*Character)
		&& Progression->CanRetrySave() && Progression->SaveNow();
	if (bProgressionSaved)
	{
		SaveStatus = NSLOCTEXT("Rogue10mRun", "Saved", "성장과 아이템을 저장했습니다.");
	}
	else
	{
		const FText Error = Progression ? Progression->GetLastError() : FText::GetEmpty();
		SaveStatus = Error.IsEmpty()
			? NSLOCTEXT("Rogue10mRun", "SaveNotReady", "선택한 캐릭터의 진행을 저장할 수 없습니다. 저장 상태를 확인한 뒤 다시 시도해 주세요.")
			: Error;
	}
	return bProgressionSaved;
}

void URogue10mRunCompletionComponent::RefreshResult()
{
	if (ARogue10mPlayerController* Controller = ResultController.Get()) { Controller->ShowRunResult(); }
}

bool URogue10mRunCompletionComponent::ReturnToTown()
{
	const ARogue10mGameState* State = RunState.Get();
	if (!bSettled || bTravelStarted || !State || State->IsRunActive() || !GetOwner()
		|| !GetOwner()->HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
	{
		return false;
	}
	const FString TownPackage = TownMap.ToSoftObjectPath().GetLongPackageName();
	if (TownPackage.IsEmpty() || !FPackageName::DoesPackageExist(TownPackage))
	{
		SaveStatus = NSLOCTEXT("Rogue10mRun", "TownMissing", "마을을 불러올 수 없습니다. 게임 콘텐츠를 확인한 뒤 다시 시도해 주세요.");
		RefreshResult();
		return false;
	}
	// Never re-enter the monster reward path, including after a disk write failure.
	if (!SaveSettlement()) { RefreshResult(); return false; }
	bTravelStarted = true;
	UGameplayStatics::SetGamePaused(this, false);
	UE_LOG(LogRogue10m, Log, TEXT("Run settlement saved; returning to %s without StartRun options."), *TownPackage);
	UGameplayStatics::OpenLevel(this, FName(*TownPackage), true, TEXT(""));
	return true;
}
