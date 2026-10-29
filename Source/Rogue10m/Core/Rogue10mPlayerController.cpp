// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mPlayerController.h"
#include "Data/Rogue10mItemDataAsset.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/GameViewportSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Rogue10mProgressionSubsystem.h"
#include "EnhancedInputSubsystems.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mAttackTargetInterface.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCameraManager.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mCombatTestWidget.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mMenuGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mRunHUD.h"
#include "Widgets/Rogue10mCharacterLobbyWidget.h"
#include "Widgets/Rogue10mDamageIndicatorWidget.h"
#include "Widgets/Rogue10mMainMenuWidget.h"
#include "Widgets/Rogue10mMenuWindowWidgets.h"
#include "Widgets/Rogue10mTraitWidget.h"
#include "Widgets/Rogue10mRunResultWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	template <typename TWidget>
	TSubclassOf<TWidget> ResolveMenuWidgetClass(
		const TSubclassOf<TWidget> ConfiguredClass,
		const TSoftClassPtr<TWidget>& DefaultClass)
	{
		if (ConfiguredClass)
		{
			return ConfiguredClass;
		}

		UClass* LoadedClass = DefaultClass.LoadSynchronous();
		return LoadedClass && LoadedClass->IsChildOf(TWidget::StaticClass())
			? TSubclassOf<TWidget>(LoadedClass)
			: nullptr;
	}
}

ARogue10mPlayerController::ARogue10mPlayerController()
{
	SkillBindingComponent = CreateDefaultSubobject<URogue10mSkillBindingComponent>(TEXT("SkillBindingComponent"));
	CombatTestComponent = CreateDefaultSubobject<URogue10mCombatTestComponent>(TEXT("CombatTestComponent"));
	PlayerCameraManagerClass = ARogue10mCameraManager::StaticClass();
	static ConstructorHelpers::FClassFinder<URogue10mDamageIndicatorWidget> DamageIndicatorWidgetFinder(
		TEXT("/Game/Widget/Damage/WBP_DamageIndicator"));
	DamageIndicatorWidgetClass = URogue10mDamageIndicatorWidget::StaticClass();
	if (DamageIndicatorWidgetFinder.Succeeded())
	{
		DamageIndicatorWidgetClass = DamageIndicatorWidgetFinder.Class;
	}
	DefaultRunHUDClass = TSoftClassPtr<URogue10mRunHUD>(
		FSoftClassPath(TEXT("/Game/Widget/WBP_Rogue10mMainHUD.WBP_Rogue10mMainHUD_C")));
	DefaultInventoryWindowWidgetClass = TSoftClassPtr<URogue10mInventoryWindowWidget>(
		FSoftClassPath(TEXT("/Game/Widget/Component/Inventory/WBP_InventoryWindow.WBP_InventoryWindow_C")));
	DefaultEquipmentWindowWidgetClass = TSoftClassPtr<URogue10mEquipmentWindowWidget>(
		FSoftClassPath(TEXT("/Game/Widget/Component/Equipment/WBP_EquipmentWindow.WBP_EquipmentWindow_C")));
	DefaultSkillTreeWindowWidgetClass = TSoftClassPtr<URogue10mSkillTreeWindowWidget>(
		FSoftClassPath(TEXT("/Game/Widget/Component/SkillTree/WBP_SkillTreeWindow.WBP_SkillTreeWindow_C")));
	DefaultCharacterLobbyWidgetClass = TSoftClassPtr<URogue10mCharacterLobbyWidget>(
		FSoftClassPath(TEXT("/Game/Widget/Lobby/WBP_CharacterLobby.WBP_CharacterLobby_C")));
	DefaultMainMenuWidgetClass = TSoftClassPtr<URogue10mMainMenuWidget>(
		FSoftClassPath(TEXT("/Game/Widget/Menu/WBP_MainMenu.WBP_MainMenu_C")));
	GameplayMap = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/Rogue10m/Maps/StartHub.StartHub")));
}

void ARogue10mPlayerController::UpdateRotation(float DeltaTime)
{
	// PossessedBy/PawnClientRestart precede GameMode's final spawn rotation. Waiting
	// for this existing update avoids that overwrite without a new timer or tick.
	if (IsLocalController())
	{
		if (ARogue10mCharacter* ControlledCharacter = Cast<ARogue10mCharacter>(GetPawn()))
		{
			if (URogue10mFirstPersonPresentationComponent* Presentation = ControlledCharacter->GetFirstPersonPresentationComponent())
			{
				Presentation->ApplyInitialFullBodyViewPitchOnce(*this);
			}
		}
	}
	// First-frame mouse input is applied after the initial baseline, just like later input.
	Super::UpdateRotation(DeltaTime);
}

bool ARogue10mPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	if (bPause && SkillBindingComponent) { SkillBindingComponent->CancelHeldInput(); }
	return Super::SetPause(bPause, CanUnpauseDelegate);
}

void ARogue10mPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetFpsLimit(CurrentFpsLimit);
	if (IsMenuWorld())
	{
		InitializeCharacterLobby();
		InitializeMainMenu();
		RefreshInputMode();
		return;
	}

	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogRogue10m, Error, TEXT("모바일 조작 위젯을 생성하지 못했습니다."));
		}
	}

	InitializeRunHUD();
	InitializeMenuWindows();
	InitializeDamageIndicatorPool();
	if (CombatTestComponent) CombatTestComponent->InitializeUI();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	UGameplayStatics::SetGamePaused(this, false);
	RefreshInputMode();
}
void ARogue10mPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (IsMenuWorld())
	{
		return;
	}

	InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ARogue10mPlayerController::ToggleInventory);
	if (CombatTestComponent) CombatTestComponent->BindInput(InputComponent);
	if (SkillBindingComponent) { SkillBindingComponent->InitializeBindings(InputComponent); }
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ARogue10mPlayerController::ToggleItemWindow);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ARogue10mPlayerController::ToggleSkillTree);
	InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ARogue10mPlayerController::ToggleSettings);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARogue10mPlayerController::ToggleSettings);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ARogue10mPlayerController::ToggleSettings);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ARogue10mPlayerController::HandleToggleCombatLog);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARogue10mPlayerController::ToggleInspectionCamera);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* Context : DefaultMappingContexts)
		{
			if (Context)
			{
				Subsystem->AddMappingContext(Context, 0);
			}
		}

		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* Context : MobileExcludedMappingContexts)
			{
				if (Context)
				{
					Subsystem->AddMappingContext(Context, 0);
				}
			}
		}
	}
}

void ARogue10mPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAbandonConfirmation();
	// Do not revive the dialogue/input stack while the controller is leaving its world.
	TraitServiceNPC.Reset();
	CloseTraitWindow();
	Super::EndPlay(EndPlayReason);
}

void ARogue10mPlayerController::ClearAbandonConfirmation()
{
	AbandonConfirmationWorld.Reset();
	AbandonConfirmationProfile.Invalidate();
	AbandonConfirmationRun.Invalidate();
	AbandonConfirmationDeadline = 0.0;
}

bool ARogue10mPlayerController::ResolveAbandonContext(URogue10mProgressionSubsystem*& OutProgression, ARogue10mGameState*& OutRun) const
{
	OutProgression = nullptr;
	OutRun = nullptr;
	const UWorld* World = GetWorld();
	const auto* PawnCharacter = Cast<ARogue10mCharacter>(GetPawn());
	const auto* RogueState = PawnCharacter ? PawnCharacter->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	UGameInstance* Instance = GetGameInstance();
	auto* Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	auto* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	if (!IsLocalPlayerController() || !HasAuthority() || !World || World->GetNetMode() != NM_Standalone
		|| IsMenuWorld() || bMainMenuVisible || bCharacterLobbyVisible || bRunResultVisible
		|| !Run || !Run->IsRunActive() || Run->GetRemainingRunSeconds() <= 0.0f || !PawnCharacter || PawnCharacter->GetController() != this
		|| PawnCharacter->IsDead() || !RogueState || RogueState->IsCharacterDead()
		|| !Progression || !Progression->IsAttachedTo(*PawnCharacter) || !Progression->CanRetrySave()
		|| !Progression->GetAttachedProfileId().IsValid() || !Progression->GetAttachedRunId().IsValid()) { return false; }
	OutProgression = Progression;
	OutRun = Run;
	return true;
}

bool ARogue10mPlayerController::RequestAbandonCurrentRun()
{
	ClearAbandonConfirmation();
	URogue10mProgressionSubsystem* Progression = nullptr;
	ARogue10mGameState* Run = nullptr;
	if (!ResolveAbandonContext(Progression, Run)) { return false; }
	int32 EntryGold = 0, EntryCrystals = 0, EntryLobby = 0;
	if (!Progression->TryResolveAbandonCurrencyBalances(EntryGold, EntryCrystals, EntryLobby))
	{
		bShowCombatLog = true;
		AddCombatLogMessage(TEXT("입장 전 재화 기준선이 없는 저장입니다. 기존 진행과 보상을 보존하기 위해 포기를 실행하지 않았습니다."),
			FLinearColor(1.0f, 0.64f, 0.43f), 6.0f);
		return false;
	}
	if (!Run->AbandonRun()) { return false; }
	CloseAllBlockingPanels();
	UGameplayStatics::SetGamePaused(this, false);
	return true;
}

void ARogue10mPlayerController::AbandonRun()
{
	HandleAbandonConfirmation();
}

void ARogue10mPlayerController::HandleAbandonConfirmation()
{
	URogue10mProgressionSubsystem* Progression = nullptr;
	ARogue10mGameState* Run = nullptr;
	if (!ResolveAbandonContext(Progression, Run)) { ClearAbandonConfirmation(); return; }
	int32 EntryGold = 0, EntryCrystals = 0, EntryLobby = 0;
	if (!Progression->TryResolveAbandonCurrencyBalances(EntryGold, EntryCrystals, EntryLobby))
	{
		RequestAbandonCurrentRun(); // Shares the same preservation notice and never creates a missing checkpoint.
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (AbandonConfirmationDeadline > 0.0 && Now <= AbandonConfirmationDeadline
		&& AbandonConfirmationWorld.Get() == GetWorld()
		&& AbandonConfirmationProfile == Progression->GetAttachedProfileId()
		&& AbandonConfirmationRun == Progression->GetAttachedRunId())
	{
		RequestAbandonCurrentRun();
		return;
	}
	ClearAbandonConfirmation();
	AbandonConfirmationWorld = GetWorld();
	AbandonConfirmationProfile = Progression->GetAttachedProfileId();
	AbandonConfirmationRun = Progression->GetAttachedRunId();
	AbandonConfirmationDeadline = Now + 3.0;
	bShowCombatLog = true;
	AddCombatLogMessage(TEXT("포기 확인: 3초 안에 콘솔에서 AbandonRun을 한 번 더 실행하세요. 런 물품은 초기화하고 입장 전 재화를 복원합니다. 마을 비용은 환불하지 않으며 습득 정보는 보존합니다."),
		FLinearColor(1.0f, 0.76f, 0.42f), 3.0f);
}

void ARogue10mPlayerController::ToggleInspectionCamera()
{
	// GameAndUI input can reach this binding when a focused widget does not handle V.
	// Reject it explicitly, including programmatic calls while gameplay input is blocked.
	if (!IsLocalPlayerController() || IsMenuWorld() || IsAnyBlockingWindowVisible()
		|| bShowMouseCursor || IsLookInputIgnored() || IsMoveInputIgnored()
		|| UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	ARogue10mCharacter* ControlledCharacter = Cast<ARogue10mCharacter>(GetPawn());
	if (!IsValid(ControlledCharacter) || ControlledCharacter->IsDead()
		|| ControlledCharacter->GetController() != this || GetViewTarget() != ControlledCharacter)
	{
		return;
	}
	if (URogue10mAppearanceCameraComponent* Camera = ControlledCharacter->GetAppearanceCameraComponent())
	{
		if (Camera->IsAppearanceCameraActive())
		{
			Camera->ToggleThirdPersonInspection();
		}
	}
}

void ARogue10mPlayerController::ToggleInventory()
{
	if (bRunResultVisible) { return; }
	if (!InventoryWindowWidget)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("InventoryWindowWidgetClass가 설정되지 않았습니다."));
		return;
	}
	SetInventoryEquipmentPanelVisible(bInventoryVisible, !bInventoryVisible, InventoryWindowWidget);
}

void ARogue10mPlayerController::ToggleItemWindow()
{
	if (bRunResultVisible) { return; }
	if (!EquipmentWindowWidget)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("EquipmentWindowWidgetClass가 설정되지 않았습니다."));
		return;
	}
	SetInventoryEquipmentPanelVisible(bItemWindowVisible, !bItemWindowVisible, EquipmentWindowWidget);
}

void ARogue10mPlayerController::ToggleSkillTree()
{
	if (bRunResultVisible) { return; }
	if (!SkillTreeWindowWidget)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("SkillTreeWindowWidgetClass가 설정되지 않았습니다."));
		return;
	}
	SetPanelVisible(bSkillTreeVisible, !bSkillTreeVisible, SkillTreeWindowWidget);
}

void ARogue10mPlayerController::ToggleSettings()
{
	if (bRunResultVisible) { return; }
	if (bTraitWindowVisible) { CloseTraitWindow(); return; }
	if (CombatTestComponent && CombatTestComponent->IsPanelOpen()) { CombatTestComponent->SetPanelOpen(false); return; }
	SetPanelVisible(bSettingsVisible, !bSettingsVisible, nullptr);
}

void ARogue10mPlayerController::CloseAllBlockingPanels()
{
	CloseTraitWindow();
	if (CombatTestComponent) CombatTestComponent->SetPanelOpen(false);
	bInventoryVisible = false;
	bItemWindowVisible = false;
	bSkillTreeVisible = false;
	bSettingsVisible = false;
	ApplyMenuWindowVisibility();
	RefreshInputMode();
}

void ARogue10mPlayerController::ShowRunResult()
{
	if (!IsLocalPlayerController() || IsMenuWorld()) { return; }
	CloseAllBlockingPanels();
	if (!RunResultWidget)
	{
		RunResultWidget = CreateWidget<URogue10mRunResultWidget>(this, URogue10mRunResultWidget::StaticClass());
	}
	if (!RunResultWidget)
	{
		UE_LOG(LogRogue10m, Error, TEXT("탐험 결과 위젯을 생성하지 못했습니다."));
		return;
	}
	bRunResultVisible = true;
	if (!RunResultWidget->IsInViewport()) { RunResultWidget->AddToPlayerScreen(300); }
	RunResultWidget->SetVisibility(ESlateVisibility::Visible);
	RunResultWidget->RefreshResult();
	if (RunHUD) { RunHUD->SetVisibility(ESlateVisibility::Collapsed); }
	if (MobileControlsWidget) { MobileControlsWidget->SetVisibility(ESlateVisibility::Collapsed); }
	if (CombatTestComponent && CombatTestComponent->GetWidget())
	{
		CombatTestComponent->GetWidget()->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ARogue10mCharacter* ResultCharacter = Cast<ARogue10mCharacter>(GetPawn()))
	{
		ResultCharacter->GetCharacterMovement()->StopMovementImmediately();
		if (URogue10mCombatComponent* Combat = ResultCharacter->GetCombatComponent()) { Combat->CancelCombatVisuals(); }
	}
	RefreshInputMode();
	RunResultWidget->SetUserFocus(this);
}

bool ARogue10mPlayerController::OpenTraitWindow(ARogue10mTutorialNPC* ServiceNPC)
{
	if (!IsLocalPlayerController() || !IsValid(ServiceNPC) || IsMenuWorld()
		|| (IsAnyBlockingWindowVisible() && (!bTraitWindowVisible || TraitServiceNPC.Get() != ServiceNPC)))
	{
		return false;
	}
	FText Reason;
	if (!ServiceNPC->CanUseTraitWindow(Reason)) { return false; }
	ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	URogue10mTownProgressionComponent* Town = State ? State->GetTownProgressionComponent() : nullptr;
	// NPC validation also admits a valid session awaiting a save retry. The widget
	// still uses CanChangeTraits/CanUpgradeTrait for every investment and refund.
	if (!Town) { return false; }
	if (bTraitWindowVisible && TraitServiceNPC.Get() == ServiceNPC) { return true; }
	if (!TraitWidget) { TraitWidget = CreateWidget<URogue10mTraitWidget>(this, URogue10mTraitWidget::StaticClass()); }
	if (!TraitWidget) { return false; }
	TraitServiceNPC = ServiceNPC;
	bTraitWindowVisible = true;
	// Every ignore push is paired with our own pop; unrelated ignore reasons are retained.
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bTraitAddedMoveIgnore = true;
	bTraitAddedLookIgnore = true;
	if (SkillBindingComponent) { SkillBindingComponent->CancelHeldInput(); }
	if (ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetPawn()))
	{
		RogueCharacter->GetCharacterMovement()->StopMovementImmediately();
		if (URogue10mCombatComponent* Combat = RogueCharacter->GetCombatComponent()) { Combat->CancelCombatVisuals(); }
	}
	TraitWidget->InitializeTraits(Town, ServiceNPC);
	if (!TraitWidget->IsInViewport() && !TraitWidget->AddToPlayerScreen(200))
	{
		CloseTraitWindow();
		return false;
	}
	TraitWidget->SetVisibility(ESlateVisibility::Visible);
	RefreshInputMode();
	TraitWidget->SetUserFocus(this);
	return true;
}

void ARogue10mPlayerController::CloseTraitWindow()
{
	if (!bTraitWindowVisible && !bTraitAddedMoveIgnore && !bTraitAddedLookIgnore) { return; }
	TWeakObjectPtr<ARogue10mTutorialNPC> PreviousNPC = TraitServiceNPC;
	bTraitWindowVisible = false;
	TraitServiceNPC.Reset();
	if (TraitWidget)
	{
		TraitWidget->ReleaseContext();
		TraitWidget->RemoveFromParent();
	}
	if (bTraitAddedMoveIgnore) { SetIgnoreMoveInput(false); bTraitAddedMoveIgnore = false; }
	if (bTraitAddedLookIgnore) { SetIgnoreLookInput(false); bTraitAddedLookIgnore = false; }
	RefreshInputMode();
	if (PreviousNPC.IsValid()) { PreviousNPC->ResumeAfterTraitWindow(); }
}

void ARogue10mPlayerController::EnterSelectedCharacter()
{
	ClearAbandonConfirmation();
	URogue10mCharacterProfileSubsystem* Profiles = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>()
		: nullptr;
	const FRogue10mCharacterProfile* Profile = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (!Profile)
	{
		UE_LOG(LogRogue10m, Error,
			TEXT("Cannot enter gameplay without a selected character profile."));
		return;
	}

	if (IsMenuWorld())
	{
		if (!TravelToGameplayMap())
		{
			UE_LOG(LogRogue10m, Error,
				TEXT("Character selection succeeded, but gameplay map travel failed."));
		}
		return;
	}

	ARogue10mGameMode* GameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
	ARogue10mCharacter* RogueCharacter = GameMode
		? GameMode->SpawnSelectedCharacterForController(this)
		: nullptr;
	if (!Profile || !RogueCharacter
		|| !RogueCharacter->ApplyCharacterProfile(*Profile))
	{
		UE_LOG(LogRogue10m, Error, TEXT("선택 상속 Character 생성 또는 외형 적용에 실패해 게임 접속을 중단했습니다."));
		return;
	}

	bCharacterLobbyVisible = false;
	if (CharacterLobbyWidget)
	{
		CharacterLobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RunHUD)
	{
		RunHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (MobileControlsWidget)
	{
		MobileControlsWidget->SetVisibility(ESlateVisibility::Visible);
	}
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	UGameplayStatics::SetGamePaused(this, false);
	RefreshInputMode();
	UE_LOG(LogRogue10m, Log,
		TEXT("Gameplay input restored. Pawn=%s MoveIgnored=%d LookIgnored=%d Paused=%d"),
		*GetNameSafe(GetPawn()), IsMoveInputIgnored(), IsLookInputIgnored(),
		UGameplayStatics::IsGamePaused(this));
	AddCombatLogMessage(
		FString::Printf(TEXT("%s 캐릭터로 접속했습니다."), *Profile->CharacterName),
		FLinearColor(0.55f, 0.88f, 1.0f, 1.0f));
	AddCombatLogMessage(TEXT("탐험 포기(프로토타입): 콘솔에서 AbandonRun을 3초 안에 두 번 실행하세요."),
		FLinearColor(0.75f, 0.82f, 0.88f), 6.0f);
}


bool ARogue10mPlayerController::IsMenuWorld() const
{
	const UWorld* World = GetWorld();
	return World && World->GetAuthGameMode<ARogue10mMenuGameMode>() != nullptr;
}

bool ARogue10mPlayerController::TravelToGameplayMap()
{
	const FSoftObjectPath MapPath = GameplayMap.ToSoftObjectPath();
	const FString MapPackageName = MapPath.GetLongPackageName();
	if (!MapPath.IsValid() || MapPackageName.IsEmpty())
	{
		UE_LOG(LogRogue10m, Error,
			TEXT("Gameplay map is not configured. Path=%s"),
			*MapPath.ToString());
		return false;
	}

	bMainMenuVisible = false;
	bCharacterLobbyVisible = false;
	UGameplayStatics::SetGamePaused(this, false);
	UE_LOG(LogRogue10m, Log,
		TEXT("Traveling from menu to starting hub. Map=%s"),
		*MapPackageName);
	// The hub keeps the run waiting; only its combat portal supplies StartRun.
	UGameplayStatics::OpenLevel(this, FName(*MapPackageName), true);
	return true;
}
void ARogue10mPlayerController::OpenCharacterLobbyFromMainMenu()
{
	if (!CharacterLobbyWidget)
	{
		UE_LOG(LogRogue10m, Error, TEXT("Character Lobby Widget is unavailable."));
		return;
	}

	bMainMenuVisible = false;
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	bCharacterLobbyVisible = true;
	CharacterLobbyWidget->SetVisibility(ESlateVisibility::Visible);
	if (RunHUD)
	{
		RunHUD->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MobileControlsWidget)
	{
		MobileControlsWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	CloseAllBlockingPanels();
	bCharacterLobbyVisible = true;
	if (!IsMoveInputIgnored())
	{
		SetIgnoreMoveInput(true);
	}
	if (!IsLookInputIgnored())
	{
		SetIgnoreLookInput(true);
	}
	UGameplayStatics::SetGamePaused(this, true);
	RefreshInputMode();
}
bool ARogue10mPlayerController::IsAnyBlockingWindowVisible() const
{
	return bMainMenuVisible || bCharacterLobbyVisible || bInventoryVisible
		|| bItemWindowVisible || bSkillTreeVisible || bSettingsVisible || bTraitWindowVisible || bRunResultVisible
		|| (CombatTestComponent && CombatTestComponent->IsPanelOpen());
}

void ARogue10mPlayerController::SetLookSensitivity(float NewSensitivityX, float NewSensitivityY)
{
	LookSensitivityX = FMath::Clamp(NewSensitivityX, 0.1f, 5.0f);
	LookSensitivityY = FMath::Clamp(NewSensitivityY, 0.1f, 5.0f);
}

void ARogue10mPlayerController::SetFpsLimit(int32 FpsValue)
{
	const int32 AllowedLimits[] = { 60, 120, 140 };
	int32 BestLimit = AllowedLimits[0];
	for (const int32 AllowedLimit : AllowedLimits)
	{
		if (FMath::Abs(FpsValue - AllowedLimit) < FMath::Abs(FpsValue - BestLimit))
		{
			BestLimit = AllowedLimit;
		}
	}
	CurrentFpsLimit = BestLimit;
	if (GEngine)
	{
		GEngine->SetMaxFPS(static_cast<float>(CurrentFpsLimit));
	}
}

void ARogue10mPlayerController::AddCombatLogMessage(const FString& Message, const FLinearColor& Color, float Duration)
{
	if (!bShowCombatLog || !GetWorld())
	{
		return;
	}

	FRogue10mRuntimeLogEntry Entry;
	Entry.Message = Message;
	Entry.Color = Color;
	Entry.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	CombatLogEntries.Insert(MoveTemp(Entry), 0);
	if (CombatLogEntries.Num() > 40)
	{
		CombatLogEntries.SetNum(40);
	}

	if (Message.Contains(TEXT("획득")) || Message.Contains(TEXT("경험치")))
	{
		AddItemAcquisitionMessage(Message, Color);
	}
}

void ARogue10mPlayerController::AddItemAcquisitionMessage(const FString& Message, const FLinearColor& Color, float Duration)
{
	if (!GetWorld())
	{
		return;
	}

	FRogue10mRuntimeLogEntry Entry;
	Entry.Message = Message;
	Entry.Color = Color;
	Entry.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	ItemAcquisitionEntries.Insert(MoveTemp(Entry), 0);
	if (ItemAcquisitionEntries.Num() > 8)
	{
		ItemAcquisitionEntries.SetNum(8);
	}
}

void ARogue10mPlayerController::AddItemAcquisitionItem(const URogue10mItemDataAsset* ItemData, int32 Quantity, float Duration)
{
	if (!ItemData || Quantity <= 0 || !GetWorld())
	{
		return;
	}

	FRogue10mRuntimeLogEntry Entry;
	Entry.Message = ItemData->DisplayName.ToString();
	Entry.Color = FLinearColor::White;
	Entry.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	Entry.ItemIcon = ItemData->InventoryIcon.LoadSynchronous();
	Entry.Quantity = Quantity;
	ItemAcquisitionEntries.Insert(MoveTemp(Entry), 0);
	if (ItemAcquisitionEntries.Num() > 8)
	{
		ItemAcquisitionEntries.SetNum(8);
	}
}
void ARogue10mPlayerController::AddFloatingDamageNumber(AActor* TargetActor, float DamageAmount, bool bCriticalHit)
{
	AActor* AttackSource = GetPawn();
	if (!TargetActor || !AttackSource || TargetActor == AttackSource || !GetWorld() || DamageAmount <= 0.0f
		|| !TargetActor->GetClass()->ImplementsInterface(URogue10mAttackTargetInterface::StaticClass())
		|| !IRogue10mAttackTargetInterface::Execute_CanReceiveRogue10mAttack(TargetActor, AttackSource))
	{
		return;
	}

	FVector IndicatorLocation =
		IRogue10mAttackTargetInterface::Execute_GetRogue10mDamageIndicatorLocation(TargetActor);
	IndicatorLocation += FVector(
		FMath::FRandRange(-8.0f, 8.0f),
		FMath::FRandRange(-8.0f, 8.0f),
		FMath::FRandRange(-4.0f, 8.0f));

	if (HasAuthority() && !IsLocalPlayerController())
	{
		ClientShowDamageIndicator(IndicatorLocation, DamageAmount, bCriticalHit);
		return;
	}
	if (!IsLocalPlayerController())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	FloatingDamageEntries.RemoveAll([CurrentTime](const FRogue10mFloatingDamageEntry& Entry)
	{
		return !Entry.TargetActor.IsValid() || Entry.ExpireTime <= CurrentTime;
	});
	if (FloatingDamageEntries.Num() >= 32)
	{
		FloatingDamageEntries.RemoveAt(0);
	}

	FRogue10mFloatingDamageEntry Entry;
	Entry.TargetActor = TargetActor;
	Entry.DamageAmount = DamageAmount;
	Entry.StartTime = CurrentTime;
	Entry.ExpireTime = CurrentTime + DamageIndicatorDuration;
	FloatingDamageEntries.Add(Entry);
	ShowDamageIndicatorAtLocation(IndicatorLocation, DamageAmount, bCriticalHit);
}

void ARogue10mPlayerController::ClientShowDamageIndicator_Implementation(
	FVector WorldLocation, float DamageAmount, bool bCriticalHit)
{
	ShowDamageIndicatorAtLocation(WorldLocation, DamageAmount, bCriticalHit);
}

void ARogue10mPlayerController::InitializeDamageIndicatorPool()
{
	if (!IsLocalPlayerController() || !DamageIndicatorWidgetClass)
	{
		return;
	}

	const int32 MaximumSize = FMath::Clamp(MaxDamageIndicatorPoolSize, 1, 128);
	const int32 InitialSize = FMath::Clamp(InitialDamageIndicatorPoolSize, 0, MaximumSize);
	DamageIndicatorPool.Reserve(MaximumSize);
	for (int32 Index = DamageIndicatorPool.Num(); Index < InitialSize; ++Index)
	{
		URogue10mDamageIndicatorWidget* Widget = CreateWidget<URogue10mDamageIndicatorWidget>(
			this, DamageIndicatorWidgetClass);
		if (!Widget)
		{
			break;
		}
		Widget->AddToPlayerScreen(100);
		Widget->DeactivateIndicator();
		DamageIndicatorPool.Add(Widget);
	}
}

URogue10mDamageIndicatorWidget* ARogue10mPlayerController::AcquireDamageIndicator()
{
	for (URogue10mDamageIndicatorWidget* Widget : DamageIndicatorPool)
	{
		if (Widget && Widget->IsAvailableForReuse())
		{
			return Widget;
		}
	}

	const int32 MaximumSize = FMath::Clamp(MaxDamageIndicatorPoolSize, 1, 128);
	if (DamageIndicatorPool.Num() < MaximumSize && DamageIndicatorWidgetClass)
	{
		if (URogue10mDamageIndicatorWidget* Widget = CreateWidget<URogue10mDamageIndicatorWidget>(
			this, DamageIndicatorWidgetClass))
		{
			Widget->AddToPlayerScreen(100);
			Widget->DeactivateIndicator();
			DamageIndicatorPool.Add(Widget);
			return Widget;
		}
	}

	URogue10mDamageIndicatorWidget* OldestWidget = nullptr;
	for (URogue10mDamageIndicatorWidget* Widget : DamageIndicatorPool)
	{
		if (Widget && (!OldestWidget || Widget->GetActivationTime() < OldestWidget->GetActivationTime()))
		{
			OldestWidget = Widget;
		}
	}
	if (OldestWidget)
	{
		OldestWidget->DeactivateIndicator();
	}
	return OldestWidget;
}

void ARogue10mPlayerController::ShowDamageIndicatorAtLocation(
	const FVector& WorldLocation, float DamageAmount, bool bCriticalHit)
{
	if (!IsLocalPlayerController() || !GetWorld() || !DamageIndicatorWidgetClass || DamageAmount <= 0.0f)
	{
		return;
	}

	if (URogue10mDamageIndicatorWidget* IndicatorWidget = AcquireDamageIndicator())
	{
		IndicatorWidget->InitializeIndicator(DamageAmount, WorldLocation, DamageIndicatorDuration, bCriticalHit);
	}
}

void ARogue10mPlayerController::NotifyPlayerDamaged(float DamageAmount)
{
	if (!GetWorld() || DamageAmount <= 0.0f)
	{
		return;
	}

	PlayerDamageFeedbackStrength = FMath::Clamp(DamageAmount / 35.0f, 0.25f, 1.0f);
	PlayerDamageFeedbackEndTime = GetWorld()->GetTimeSeconds() + PlayerDamageFeedbackDuration;
}

bool ARogue10mPlayerController::ToggleCombatLogVisible()
{
	bShowCombatLog = !bShowCombatLog;
	if (!bShowCombatLog)
	{
		CombatLogEntries.Reset();
	}
	return bShowCombatLog;
}

bool ARogue10mPlayerController::ActivateQuickSlot(int32 SlotNumber)
{
	ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetPawn());
	URogue10mInventoryComponent* Inventory = RogueCharacter ? RogueCharacter->GetInventoryComponent() : nullptr;
	return Inventory && Inventory->UseConsumableQuickSlot(SlotNumber - 1);
}

ARogue10mBasicMonster* ARogue10mPlayerController::FindLookedAtMonster() const
{
	if (!PlayerCameraManager || !GetWorld())
	{
		return nullptr;
	}

	const FVector TraceStart = PlayerCameraManager->GetCameraLocation();
	const FVector TraceEnd = TraceStart + PlayerCameraManager->GetCameraRotation().Vector() * 3000.0f;
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Rogue10mMonsterInfo), false, GetPawn());
	return GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Pawn, QueryParams)
		? Cast<ARogue10mBasicMonster>(Hit.GetActor())
		: nullptr;
}

float ARogue10mPlayerController::GetPlayerDamageFeedbackAlpha() const
{
	if (!GetWorld() || PlayerDamageFeedbackEndTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp((PlayerDamageFeedbackEndTime - GetWorld()->GetTimeSeconds()) / FMath::Max(0.05f, PlayerDamageFeedbackDuration), 0.0f, 1.0f);
}

bool ARogue10mPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ARogue10mPlayerController::InitializeRunHUD()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const TSubclassOf<URogue10mRunHUD> ResolvedClass = ResolveRunHUDClass();
	if (!ResolvedClass)
	{
		return;
	}

	RunHUD = CreateWidget<URogue10mRunHUD>(this, ResolvedClass);
	if (!RunHUD)
	{
		UE_LOG(LogRogue10m, Error, TEXT("메인 HUD 위젯을 생성하지 못했습니다."));
		return;
	}

	RunHUD->AddToPlayerScreen(0);
	if (ARogue10mGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr)
	{
		GameState->OnRunTimerUpdated.AddDynamic(RunHUD, &URogue10mRunHUD::BP_UpdateRunTimer);
		GameState->OnRunPhaseChanged.AddDynamic(RunHUD, &URogue10mRunHUD::BP_OnRunPhaseChanged);
		RunHUD->BP_UpdateRunTimer(GameState->GetRemainingRunSeconds(), GameState->GetRunDurationSeconds(), GameState->GetRunProgressAlpha());
	}
}

TSubclassOf<URogue10mRunHUD> ARogue10mPlayerController::ResolveRunHUDClass()
{
	if (RunHUDClass)
	{
		return RunHUDClass;
	}

	UClass* LoadedClass = DefaultRunHUDClass.LoadSynchronous();
	if (!LoadedClass || !LoadedClass->IsChildOf(URogue10mRunHUD::StaticClass()))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("기본 메인 HUD가 없거나 Rogue10mRunHUD를 상속하지 않았습니다."));
		return nullptr;
	}
	return LoadedClass;
}

void ARogue10mPlayerController::InitializeMenuWindows()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	const ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetPawn());
	URogue10mInventoryComponent* Inventory = RogueCharacter ? RogueCharacter->GetInventoryComponent() : nullptr;
	const TSubclassOf<URogue10mInventoryWindowWidget> ResolvedInventoryClass = ResolveMenuWidgetClass(
		InventoryWindowWidgetClass, DefaultInventoryWindowWidgetClass);
	const TSubclassOf<URogue10mEquipmentWindowWidget> ResolvedEquipmentClass = ResolveMenuWidgetClass(
		EquipmentWindowWidgetClass, DefaultEquipmentWindowWidgetClass);
	const TSubclassOf<URogue10mSkillTreeWindowWidget> ResolvedSkillTreeClass = ResolveMenuWidgetClass(
		SkillTreeWindowWidgetClass, DefaultSkillTreeWindowWidgetClass);

	auto InitializeWindow = [this, Inventory](TSubclassOf<URogue10mMenuWindowWidget> WidgetClass,
		TObjectPtr<URogue10mMenuWindowWidget>& OutWidget)
	{
		if (!WidgetClass)
		{
			return;
		}
		OutWidget = CreateWidget<URogue10mMenuWindowWidget>(this, WidgetClass);
		if (OutWidget)
		{
			OutWidget->InitializeMenuWindow(Inventory);
			BringMenuWindowToFront(OutWidget);
			OutWidget->SetWindowOpen(false);
		}
	};

	TObjectPtr<URogue10mMenuWindowWidget> InventoryBase;
	InitializeWindow(ResolvedInventoryClass, InventoryBase);
	InventoryWindowWidget = Cast<URogue10mInventoryWindowWidget>(InventoryBase);

	TObjectPtr<URogue10mMenuWindowWidget> EquipmentBase;
	InitializeWindow(ResolvedEquipmentClass, EquipmentBase);
	EquipmentWindowWidget = Cast<URogue10mEquipmentWindowWidget>(EquipmentBase);

	TObjectPtr<URogue10mMenuWindowWidget> SkillTreeBase;
	InitializeWindow(ResolvedSkillTreeClass, SkillTreeBase);
	SkillTreeWindowWidget = Cast<URogue10mSkillTreeWindowWidget>(SkillTreeBase);
}

void ARogue10mPlayerController::ApplyMenuWindowVisibility()
{
	if (InventoryWindowWidget && InventoryWindowWidget->IsWindowOpen() != bInventoryVisible)
	{
		InventoryWindowWidget->SetWindowOpen(bInventoryVisible);
	}
	if (EquipmentWindowWidget && EquipmentWindowWidget->IsWindowOpen() != bItemWindowVisible)
	{
		EquipmentWindowWidget->SetWindowOpen(bItemWindowVisible);
	}
	if (SkillTreeWindowWidget && SkillTreeWindowWidget->IsWindowOpen() != bSkillTreeVisible)
	{
		SkillTreeWindowWidget->SetWindowOpen(bSkillTreeVisible);
	}
}

void ARogue10mPlayerController::BringMenuWindowToFront(URogue10mMenuWindowWidget* Window)
{
	if (!Window)
	{
		return;
	}

	MenuWindowStack.RemoveAll([Window](const TWeakObjectPtr<URogue10mMenuWindowWidget>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Window;
	});
	MenuWindowStack.Add(Window);

	int32 StackIndex = 0;
	UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
	for (const TWeakObjectPtr<URogue10mMenuWindowWidget>& Entry : MenuWindowStack)
	{
		if (URogue10mMenuWindowWidget* MenuWindow = Entry.Get())
		{
			const int32 WindowZOrder = MenuWindowBaseZOrder + StackIndex++;
			if (ViewportSubsystem && ViewportSubsystem->IsWidgetAdded(MenuWindow))
			{
				FGameViewportWidgetSlot ViewportSlot = ViewportSubsystem->GetWidgetSlot(MenuWindow);
				if (ViewportSlot.ZOrder != WindowZOrder)
				{
					ViewportSlot.ZOrder = WindowZOrder;
					ViewportSubsystem->SetWidgetSlot(MenuWindow, ViewportSlot);
				}
			}
			else if (!MenuWindow->IsInViewport())
			{
				MenuWindow->AddToPlayerScreen(WindowZOrder);
			}
		}
	}
}

URogue10mMenuWindowWidget* ARogue10mPlayerController::GetTopmostOpenMenuWindow() const
{
	for (int32 Index = MenuWindowStack.Num() - 1; Index >= 0; --Index)
	{
		if (URogue10mMenuWindowWidget* MenuWindow = MenuWindowStack[Index].Get())
		{
			if (MenuWindow->IsWindowOpen())
			{
				return MenuWindow;
			}
		}
	}
	return nullptr;
}

void ARogue10mPlayerController::InitializeCharacterLobby()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	TSubclassOf<URogue10mCharacterLobbyWidget> ResolvedClass = CharacterLobbyWidgetClass;
	if (!ResolvedClass)
	{
		UClass* LoadedClass = DefaultCharacterLobbyWidgetClass.LoadSynchronous();
		if (LoadedClass && LoadedClass->IsChildOf(URogue10mCharacterLobbyWidget::StaticClass()))
		{
			ResolvedClass = LoadedClass;
		}
	}
	if (!ResolvedClass)
	{
		UE_LOG(LogRogue10m, Error, TEXT("캐릭터 로비 Widget 클래스를 찾을 수 없습니다."));
		return;
	}

	CharacterLobbyWidget = CreateWidget<URogue10mCharacterLobbyWidget>(this, ResolvedClass);
	if (!CharacterLobbyWidget)
	{
		UE_LOG(LogRogue10m, Error, TEXT("캐릭터 로비 Widget을 생성하지 못했습니다."));
		return;
	}
	CharacterLobbyWidget->InitializeCharacterLobby(this);
	CharacterLobbyWidget->AddToPlayerScreen(1000);
	CharacterLobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	bCharacterLobbyVisible = false;
}

void ARogue10mPlayerController::InitializeMainMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const TSubclassOf<URogue10mMainMenuWidget> ResolvedClass =
		ResolveMenuWidgetClass(MainMenuWidgetClass, DefaultMainMenuWidgetClass);
	if (!ResolvedClass)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Main Menu Widget is unavailable; opening Character Lobby."));
		OpenCharacterLobbyFromMainMenu();
		return;
	}

	MainMenuWidget = CreateWidget<URogue10mMainMenuWidget>(this, ResolvedClass);
	if (!MainMenuWidget)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Failed to create Main Menu Widget; opening Character Lobby."));
		OpenCharacterLobbyFromMainMenu();
		return;
	}

	MainMenuWidget->AddToPlayerScreen(1100);
	bMainMenuVisible = true;
	if (RunHUD)
	{
		RunHUD->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MobileControlsWidget)
	{
		MobileControlsWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!IsMoveInputIgnored())
	{
		SetIgnoreMoveInput(true);
	}
	if (!IsLookInputIgnored())
	{
		SetIgnoreLookInput(true);
	}
	UGameplayStatics::SetGamePaused(this, true);
}
void ARogue10mPlayerController::RefreshInputMode()
{
	if (IsAnyBlockingWindowVisible() && SkillBindingComponent) { SkillBindingComponent->CancelHeldInput(); }
	if (bRunResultVisible && RunResultWidget)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(RunResultWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}
	if (bTraitWindowVisible && TraitWidget)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TraitWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}
	if (bMainMenuVisible && MainMenuWidget)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}
	if (bCharacterLobbyVisible && CharacterLobbyWidget)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(CharacterLobbyWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	const bool bBlocking = IsAnyBlockingWindowVisible();
	bShowMouseCursor = bBlocking;
	bEnableClickEvents = bBlocking;
	bEnableMouseOverEvents = bBlocking;

	if (bBlocking)
	{
		FInputModeGameAndUI InputMode;
		UUserWidget* FocusWidget = GetTopmostOpenMenuWindow();
		if (CombatTestComponent && CombatTestComponent->IsPanelOpen()) FocusWidget = CombatTestComponent->GetWidget();
		if (!FocusWidget) FocusWidget = RunHUD;
		if (FocusWidget)
		{
			InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ARogue10mPlayerController::SetPanelVisible(
	bool& PanelState, bool bVisible, URogue10mMenuWindowWidget* ActivatedWindow)
{
	if (bVisible && CombatTestComponent) CombatTestComponent->SetPanelOpen(false);
	if (bVisible)
	{
		CloseTraitWindow();
		bInventoryVisible = false;
		bItemWindowVisible = false;
		bSkillTreeVisible = false;
		bSettingsVisible = false;
	}
	PanelState = bVisible;
	ApplyMenuWindowVisibility();

	if (bVisible)
	{
		BringMenuWindowToFront(ActivatedWindow);
	}
	if (bVisible)
	{
		if (ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetPawn()))
		{
			RogueCharacter->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	RefreshInputMode();
}
void ARogue10mPlayerController::SetInventoryEquipmentPanelVisible(
	bool& PanelState, bool bVisible, URogue10mMenuWindowWidget* ActivatedWindow)
{
	if (bVisible && CombatTestComponent) CombatTestComponent->SetPanelOpen(false);
	if (bVisible)
	{
		CloseTraitWindow();
		// 인벤토리와 장비창은 한 그룹으로 함께 열 수 있고, 다른 Blocking 창만 닫습니다.
		bSkillTreeVisible = false;
		bSettingsVisible = false;
	}
	PanelState = bVisible;
	ApplyMenuWindowVisibility();

	if (bVisible)
	{
		BringMenuWindowToFront(ActivatedWindow);
	}
	if (bVisible)
	{
		if (ARogue10mCharacter* RogueCharacter = Cast<ARogue10mCharacter>(GetPawn()))
		{
			RogueCharacter->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	RefreshInputMode();
}

void ARogue10mPlayerController::HandleToggleCombatLog()
{
	const bool bVisible = ToggleCombatLogVisible();
	if (bVisible)
	{
		AddCombatLogMessage(TEXT("전투 로그 표시 켜짐"), FLinearColor(0.58f, 0.85f, 1.0f, 1.0f));
	}
}
