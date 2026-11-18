// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mPlayerController.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Widgets/Rogue10mQuitMenuWidget.h"

void ARogue10mPlayerController::RefreshQuitMenu()
{
	if (bMenuPauseEndingPlay || !IsLocalPlayerController()) { return; }
	if (!QuitMenuWidget)
	{
		QuitMenuWidget = CreateWidget<URogue10mQuitMenuWidget>(this, URogue10mQuitMenuWidget::StaticClass());
		if (QuitMenuWidget) { QuitMenuWidget->AddToPlayerScreen(2000); }
	}
	if (QuitMenuWidget)
	{
		// The authored main menu keeps its own quit button. Other menu surfaces share this action.
		QuitMenuWidget->SetMenuContextVisible((bSettingsVisible && !bAbandonMenuConfirmOpen)
			|| bCharacterLobbyVisible || bRunResultVisible);
	}
}

void ARogue10mPlayerController::ResetQuitGameConfirmation()
{
	bQuitGameConfirmationOpen = false;
	QuitGameWorld.Reset(); QuitGameCharacter.Reset();
	QuitGameProfile.Invalidate(); QuitGameRun.Invalidate(); QuitGameSelectedProfile.Invalidate();
	if (QuitMenuWidget) { QuitMenuWidget->SetConfirmationOpen(false); }
}

bool ARogue10mPlayerController::OpenQuitGameConfirmation()
{
	if (!IsLocalPlayerController() || bMenuPauseEndingPlay || bQuitGameSubmitting || bAbandonMenuConfirmOpen
		|| !GetWorld() || !(bSettingsVisible || bMainMenuVisible || bCharacterLobbyVisible || bRunResultVisible)) { return false; }
	if (bQuitGameConfirmationOpen) { return true; }
	RefreshQuitMenu();
	if (!QuitMenuWidget) { return false; }
	auto* Instance = GetGameInstance();
	auto* Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	QuitGameWorld = GetWorld(); QuitGameCharacter = Cast<ARogue10mCharacter>(GetPawn());
	QuitGameProfile = Saves ? Saves->GetAttachedProfileId() : FGuid();
	QuitGameRun = Saves ? Saves->GetAttachedRunId() : FGuid();
	QuitGameSelectedProfile = Selected ? Selected->ProfileId : FGuid();
	FText Policy = NSLOCTEXT("Rogue10mQuit", "MenuPolicy", "게임을 종료하고 바탕화면으로 돌아갑니다.");
	const auto* Run = GetWorld()->GetGameState<ARogue10mGameState>();
	if (QuitGameCharacter.IsValid())
	{
		Policy = NSLOCTEXT("Rogue10mQuit", "HubPolicy", "현재 기록을 저장한 뒤 게임을 종료합니다.");
		if (Run && Run->IsRunActive())
		{
			int32 Gold = 0, Crystals = 0, Lobby = 0;
			Policy = Saves && Saves->TryResolveAbandonCurrencyBalances(Gold, Crystals, Lobby)
				? NSLOCTEXT("Rogue10mQuit", "RunPolicy", "현재 탐험은 이어서 진행할 수 없습니다.\n다음 마을 접속 시 탐험 아이템·성장·습득 스킬이\n초기화되고 재화는 입장 비용을 지불한 뒤의\n기준으로 돌아갑니다.\n발견 기록과 영구 강화는 유지됩니다.\n\n필드에서는 확인 중에도 시간과 전투가 진행됩니다.")
				: NSLOCTEXT("Rogue10mQuit", "LegacyRunPolicy", "현재 탐험은 이어서 진행할 수 없습니다.\n이 탐험의 입장 기준 기록을 확인할 수 없어\n재접속 시 복구 결과를 보장할 수 없습니다.\n\n필드에서는 확인 중에도 시간과 전투가 진행됩니다.");
		}
		else if (Run && Run->IsRunTerminal())
		{
			Policy = NSLOCTEXT("Rogue10mQuit", "ResultPolicy", "탐험 결과가 정상적으로 저장되었는지 확인한 뒤\n게임을 종료합니다.");
		}
	}
	bQuitGameConfirmationOpen = true;
	QuitMenuWidget->SetConfirmationOpen(true, Policy);
	RefreshInputMode();
	return true;
}

void ARogue10mPlayerController::CancelQuitGameConfirmation()
{
	if (bQuitGameSubmitting || !bQuitGameConfirmationOpen) { return; }
	ResetQuitGameConfirmation();
	RefreshInputMode();
}

bool ARogue10mPlayerController::ConfirmQuitGame()
{
	if (!bQuitGameConfirmationOpen || bQuitGameSubmitting || !QuitMenuWidget || bMenuPauseEndingPlay) { return false; }
	TGuardValue<bool> Guard(bQuitGameSubmitting, true);
	auto* Instance = GetGameInstance();
	auto* Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	const FGuid SelectedId = Selected ? Selected->ProfileId : FGuid();
	auto* PawnCharacter = Cast<ARogue10mCharacter>(GetPawn());
	const FGuid AttachedProfile = Saves ? Saves->GetAttachedProfileId() : FGuid();
	const FGuid AttachedRun = Saves ? Saves->GetAttachedRunId() : FGuid();
	if (QuitGameWorld.Get() != GetWorld() || QuitGameCharacter.Get() != PawnCharacter
		|| QuitGameProfile != AttachedProfile || QuitGameRun != AttachedRun || QuitGameSelectedProfile != SelectedId)
	{
		QuitMenuWidget->ShowError(NSLOCTEXT("Rogue10mQuit", "ContextChanged", "캐릭터 또는 탐험이 변경되었습니다. 취소 후 다시 확인해 주세요."));
		return false;
	}
	const auto* Run = GetWorld()->GetGameState<ARogue10mGameState>();
	if (Run && Run->IsRunTerminal())
	{
		const auto* Mode = GetWorld()->GetAuthGameMode<ARogue10mGameMode>();
		const auto* Completion = Mode ? Mode->GetRunCompletionComponent() : nullptr;
		if (!Completion || !Completion->IsSettled() || !Completion->IsProgressionSaved())
		{
			QuitMenuWidget->ShowError(NSLOCTEXT("Rogue10mQuit", "SettlementPending", "탐험 결과 저장이 완료되지 않았습니다.\n취소 후 결과 화면에서 마을로 돌아가기를 눌러 저장을 재시도해 주세요."));
			return false;
		}
	}
	if (AttachedProfile.IsValid() || (PawnCharacter && SelectedId.IsValid()))
	{
		if (!PawnCharacter || !Saves || !Saves->IsAttachedTo(*PawnCharacter) || !Saves->CanRetrySave() || !Saves->SaveNow())
		{
			QuitMenuWidget->ShowError(NSLOCTEXT("Rogue10mQuit", "SaveFailed", "기록을 저장하지 못해 종료하지 않았습니다.\n종료 확인을 눌러 다시 시도하거나 취소해 주세요."));
			UE_LOG(LogRogue10m, Warning, TEXT("QUIT_MENU save_failed; application remains open"));
			return false;
		}
		UE_LOG(LogRogue10m, Log, TEXT("QUIT_MENU progression_saved profile=%s run=%s"), *AttachedProfile.ToString(), *AttachedRun.ToString());
	}
	else
	{
		UE_LOG(LogRogue10m, Log, TEXT("QUIT_MENU no_attached_session; no_progression_write_required"));
	}
	UE_LOG(LogRogue10m, Log, TEXT("QUIT_MENU confirmed; requesting_application_exit"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	return true;
}
