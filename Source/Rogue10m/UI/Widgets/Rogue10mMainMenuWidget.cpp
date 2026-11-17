// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mMainMenuWidget.h"

#include "Components/Button.h"
#include "Rogue10mPlayerController.h"

void URogue10mMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	UI_StartGameButton->OnClicked.AddDynamic(this, &ThisClass::HandleStartGameClicked);
	UI_QuitGameButton->OnClicked.AddDynamic(this, &ThisClass::HandleQuitGameClicked);
}

void URogue10mMainMenuWidget::NativeDestruct()
{
	UI_StartGameButton->OnClicked.RemoveAll(this);
	UI_QuitGameButton->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void URogue10mMainMenuWidget::HandleStartGameClicked()
{
	if (ARogue10mPlayerController* RogueController =
		GetOwningPlayer<ARogue10mPlayerController>())
	{
		RogueController->OpenCharacterLobbyFromMainMenu();
	}
}

void URogue10mMainMenuWidget::HandleQuitGameClicked()
{
	if (auto* Controller = GetOwningPlayer<ARogue10mPlayerController>())
	{
		Controller->OpenQuitGameConfirmation();
	}
}
