// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mRunPortal.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTutorialSupplySubsystem.h"
#include "Rogue10mTutorialWidget.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 StayChoice = 0;
	constexpr int32 DepartChoice = 1;
	constexpr int32 DepartureChoiceCount = 2;
}

ARogue10mRunPortal::ARogue10mRunPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	InputPriority = 2;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);

	PortalCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Portal Collision"));
	PortalCollision->SetupAttachment(SceneRoot);
	PortalCollision->SetBoxExtent(FVector(160.0f, 220.0f, 180.0f));
	PortalCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PortalCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PortalCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PortalCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PortalCollision->SetGenerateOverlapEvents(true);
	PortalCollision->SetCanEverAffectNavigation(false);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Portal Mesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARogue10mRunPortal::BeginPlay()
{
	Super::BeginPlay();
	PortalCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandlePortalBeginOverlap);
	PortalCollision->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandlePortalEndOverlap);
	UpdateNearbyPlayer();
}

void ARogue10mRunPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PortalCollision->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandlePortalBeginOverlap);
	PortalCollision->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandlePortalEndOverlap);
	GetWorldTimerManager().ClearTimer(NearbyTimer);
	ReleaseNearbyPlayer();
	Super::EndPlay(EndPlayReason);
}

void ARogue10mRunPortal::HandlePortalBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (OtherActor && OtherActor->IsA<ARogue10mCharacter>())
	{
		UpdateNearbyPlayer();
	}
}

void ARogue10mRunPortal::HandlePortalEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (OtherActor && OtherActor->IsA<ARogue10mCharacter>())
	{
		UpdateNearbyPlayer();
	}
}

ARogue10mPlayerController* ARogue10mRunPortal::FindNearbyController(ARogue10mCharacter* Character) const
{
	const UWorld* World = GetWorld();
	if (!World || !IsValid(Character) || Character->IsDead()
		|| !Character->HasAuthority() || Character->GetWorld() != World
		|| !PortalCollision->IsOverlappingComponent(Character->GetCapsuleComponent()))
	{
		return nullptr;
	}

	ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(Character->GetController());
	return IsValid(Controller) && Controller->IsLocalPlayerController() && Controller->GetPawn() == Character
		? Controller : nullptr;
}

bool ARogue10mRunPortal::CanUseDeparture() const
{
	ARogue10mPlayerController* Controller = NearbyController.Get();
	if (bTravelStarted || TargetLevelName.IsNone() || !Controller
		|| FindNearbyController(NearbyCharacter.Get()) != Controller || UGameplayStatics::IsGamePaused(this))
	{
		return false;
	}
	const ARogue10mGameState* Run = GetWorld()->GetGameState<ARogue10mGameState>();
	if (bStartRunTimerOnTravel && Run && Run->GetRunPhase() != ERogue10mRunPhase::WaitingToStart)
	{
		return false;
	}
	if (bDepartureDialogueOpen)
	{
		return Controller->GetDepartureDialoguePortal() == this && Controller->IsDepartureDialogueVisible();
	}
	return !Controller->IsMoveInputIgnored() && !Controller->IsAnyBlockingWindowVisible();
}

void ARogue10mRunPortal::UpdateNearbyPlayer()
{
	TArray<AActor*> OverlappingCharacters;
	PortalCollision->GetOverlappingActors(OverlappingCharacters, ARogue10mCharacter::StaticClass());
	if (OverlappingCharacters.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(NearbyTimer);
	}
	else if (!GetWorldTimerManager().IsTimerActive(NearbyTimer))
	{
		// Watch only during overlap so a late possession or loss of input ownership is handled.
		GetWorldTimerManager().SetTimer(NearbyTimer, this, &ThisClass::UpdateNearbyPlayer, 0.1f, true);
	}

	ARogue10mCharacter* Character = nullptr;
	ARogue10mPlayerController* Controller = nullptr;
	for (AActor* Actor : OverlappingCharacters)
	{
		ARogue10mCharacter* Candidate = Cast<ARogue10mCharacter>(Actor);
		Controller = FindNearbyController(Candidate);
		if (Controller)
		{
			Character = Candidate;
			break;
		}
	}
	if (Character != NearbyCharacter.Get() || Controller != NearbyController.Get() || (!Controller && DepartureWidget))
	{
		ReleaseNearbyPlayer();
		NearbyCharacter = Character;
		NearbyController = Controller;
	}
	if (!Controller || bTravelStarted)
	{
		return;
	}
	if (!CanUseDeparture())
	{
		CloseDepartureDialogue();
		SetInteractionInputEnabled(false);
		if (DepartureWidget)
		{
			DepartureWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (!DepartureWidget)
	{
		DepartureWidget = CreateWidget<URogue10mTutorialWidget>(Controller, URogue10mTutorialWidget::StaticClass());
		if (!DepartureWidget)
		{
			return;
		}
		DepartureWidget->AddToPlayerScreen(180);
		RefreshDepartureView();
	}
	else if (DepartureWidget->GetVisibility() == ESlateVisibility::Collapsed)
	{
		RefreshDepartureView();
	}
	SetInteractionInputEnabled(true);
}

void ARogue10mRunPortal::ReleaseNearbyPlayer()
{
	SetInteractionInputEnabled(false);
	CloseDepartureDialogue();
	if (DepartureWidget)
	{
		DepartureWidget->RemoveFromParent();
		DepartureWidget = nullptr;
	}
	NearbyCharacter.Reset();
	NearbyController.Reset();
}

void ARogue10mRunPortal::SetInteractionInputEnabled(bool bEnabled)
{
	if (bEnabled == bInteractionInputEnabled)
	{
		return;
	}
	ARogue10mPlayerController* Controller = NearbyController.Get();
	if (bEnabled && Controller)
	{
		EnableInput(Controller);
		bInteractionInputEnabled = true;
		if (InputComponent && !bInteractBound)
		{
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::Interact).bConsumeInput = true;
			bInteractBound = true;
		}
	}
	else if (!bEnabled)
	{
		if (Controller)
		{
			DisableInput(Controller);
		}
		bInteractionInputEnabled = false;
	}
}

void ARogue10mRunPortal::Interact()
{
	if (bDepartureDialogueOpen && !bTravelStarted)
	{
		CloseDepartureDialogue();
	}
	else
	{
		BeginDepartureDialogue(NearbyCharacter.Get());
	}
}

bool ARogue10mRunPortal::BeginDepartureDialogue(ARogue10mCharacter* Character)
{
	if (!FindNearbyController(Character))
	{
		return false;
	}
	if (Character != NearbyCharacter.Get() || !DepartureWidget)
	{
		UpdateNearbyPlayer();
	}
	if (Character != NearbyCharacter.Get() || !DepartureWidget || !CanUseDeparture())
	{
		return false;
	}
	if (!bDepartureDialogueOpen)
	{
		bDepartureDialogueOpen = true;
		SelectedDepartureChoice = StayChoice;
		DepartureStatus = FText::GetEmpty();
		RefreshDepartureView();
		NearbyController->BeginDepartureDialogueInput(this);
	}
	return true;
}

void ARogue10mRunPortal::CloseDepartureDialogue()
{
	if (!bDepartureDialogueOpen)
	{
		return;
	}
	bDepartureDialogueOpen = false;
	SelectedDepartureChoice = StayChoice;
	DepartureStatus = FText::GetEmpty();
	if (ARogue10mPlayerController* Controller = NearbyController.Get())
	{
		Controller->EndDepartureDialogueInput(this);
	}
	if (DepartureWidget)
	{
		if (CanUseDeparture())
		{
			RefreshDepartureView();
		}
		else
		{
			DepartureWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void ARogue10mRunPortal::NavigateDepartureChoice(int32 Delta)
{
	if (bDepartureDialogueOpen && CanUseDeparture())
	{
		SelectedDepartureChoice = (SelectedDepartureChoice + Delta % DepartureChoiceCount + DepartureChoiceCount)
			% DepartureChoiceCount;
		RefreshDepartureView();
	}
}

void ARogue10mRunPortal::SelectDepartureChoice(int32 Index)
{
	if (!bDepartureDialogueOpen || !CanUseDeparture())
	{
		return;
	}
	if (Index == StayChoice)
	{
		CloseDepartureDialogue();
	}
	else if (Index == DepartChoice)
	{
		SelectedDepartureChoice = DepartChoice;
		ConfirmDeparture(*NearbyCharacter.Get());
	}
}

void ARogue10mRunPortal::RefreshDepartureView()
{
	if (!DepartureWidget)
	{
		return;
	}
	const FText Dialogue = bStartRunTimerOnTravel
		? NSLOCTEXT("Rogue10mDeparture", "RunDialogue", "관문을 넘으면 20분 탐험이 시작됩니다.")
		: NSLOCTEXT("Rogue10mDeparture", "Dialogue", "관문을 넘어 이동할까요?");
	const FText Action = bDepartureDialogueOpen
		? NSLOCTEXT("Rogue10mDeparture", "Controls", "↑↓ 선택  ·  Enter 확인  ·  Esc 닫기")
		: NSLOCTEXT("Rogue10mDeparture", "Prompt", "F  대화하기");
	DepartureWidget->SetDialogueView(NSLOCTEXT("Rogue10mDeparture", "GateName", "북쪽 관문"),
		NSLOCTEXT("Rogue10mDeparture", "GateRole", "탐험 출발"),
		bDepartureDialogueOpen ? Dialogue : FText::GetEmpty(), DepartureStatus, Action);
	DepartureWidget->SetDepartureChoices(this, bDepartureDialogueOpen, SelectedDepartureChoice);
}

void ARogue10mRunPortal::ConfirmDeparture(ARogue10mCharacter& Character)
{
	bTravelStarted = true;
	if (UGameInstance* Instance = GetGameInstance())
	{
		URogue10mProgressionSubsystem* Progression = Instance->GetSubsystem<URogue10mProgressionSubsystem>();
		if (Progression && Progression->IsAttachedTo(Character)
			&& !(bStartRunTimerOnTravel ? Progression->SaveRunEntryCurrencyBaseline() : Progression->SaveNow()))
		{
			bTravelStarted = false;
			DepartureStatus = NSLOCTEXT("Rogue10mDeparture", "SaveFailed", "진행 상황을 저장하지 못했습니다. 다시 시도해 주세요.");
			RefreshDepartureView();
			UE_LOG(LogRogue10m, Error, TEXT("Portal travel stopped: character progression could not be saved."));
			return;
		}
		if (URogue10mTutorialSupplySubsystem* Supplies = Instance->GetSubsystem<URogue10mTutorialSupplySubsystem>())
		{
			if (const URogue10mInventoryComponent* Inventory = Character.GetInventoryComponent())
			{
				Supplies->CaptureSuppliesForTravel(*Inventory);
			}
		}
	}

	DepartureStatus = NSLOCTEXT("Rogue10mDeparture", "Departing", "관문을 열고 있습니다…");
	RefreshDepartureView();
	const FString TravelOptions = BuildTravelOptions();
	UE_LOG(LogRogue10m, Log, TEXT("Run portal opened level '%s' with options '%s'."), *TargetLevelName.ToString(), *TravelOptions);
	UGameplayStatics::OpenLevel(this, TargetLevelName, true, TravelOptions);
}

FString ARogue10mRunPortal::BuildTravelOptions() const
{
	FString TravelOptions = AdditionalTravelOptions;
	if (bStartRunTimerOnTravel)
	{
		if (!TravelOptions.IsEmpty() && !TravelOptions.EndsWith(TEXT("?")))
		{
			TravelOptions += TEXT("?");
		}
		TravelOptions += TEXT("StartRun=1");
	}

	return TravelOptions;
}
