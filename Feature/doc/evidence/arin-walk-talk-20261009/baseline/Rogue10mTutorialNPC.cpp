// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialNPC.h"

#include "Animation/AnimSequence.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mTutorialDataAsset.h"
#include "Rogue10mTutorialRewardComponent.h"
#include "Rogue10mTutorialSupplySubsystem.h"
#include "Rogue10mTutorialWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARogue10mTutorialNPC::ARogue10mTutorialNPC()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	Tags.AddUnique(TEXT("TutorialGuide"));
	InputPriority = 2;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	GuideMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GuideMesh"));
	GuideMesh->SetupAttachment(SceneRoot);
	GuideMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GuideMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideMesh->SetGenerateOverlapEvents(false);
	GuideMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));
	if (MeshFinder.Succeeded()) { GuideMesh->SetSkeletalMeshAsset(MeshFinder.Object); }
	if (IdleFinder.Succeeded())
	{
		GuideMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		GuideMesh->AnimationData.AnimToPlay = IdleFinder.Object;
		GuideMesh->AnimationData.bSavedLooping = true;
		GuideMesh->AnimationData.bSavedPlaying = true;
	}
	DetectionArea = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionArea"));
	DetectionArea->SetupAttachment(SceneRoot);
	DetectionArea->SetSphereRadius(DetectionRadius);
	DetectionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionArea->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DetectionArea->SetGenerateOverlapEvents(true);
	DetectionArea->SetCanEverAffectNavigation(false);
}

void ARogue10mTutorialNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DetectionArea->SetSphereRadius(FMath::Max(InteractionRadius + 50.0f, DetectionRadius));
	ApplyGuideAppearance();
}

void ARogue10mTutorialNPC::ApplyGuideAppearance()
{
	if (!GuideMesh || !TutorialData) { return; }
	if (TutorialData->GuideSkeletalMesh) { GuideMesh->SetSkeletalMeshAsset(TutorialData->GuideSkeletalMesh); }
	if (TutorialData->GuideIdleAnimation)
	{
		const USkeletalMesh* Mesh = GuideMesh->GetSkeletalMeshAsset();
		if (Mesh && Mesh->GetSkeleton() == TutorialData->GuideIdleAnimation->GetSkeleton())
		{
			GuideMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			GuideMesh->AnimationData.AnimToPlay = TutorialData->GuideIdleAnimation;
			GuideMesh->AnimationData.bSavedLooping = true;
			GuideMesh->AnimationData.bSavedPlaying = true;
			if (HasActorBegunPlay()) { GuideMesh->PlayAnimation(TutorialData->GuideIdleAnimation, true); }
		}
		else
		{
			UE_LOG(LogRogue10m, Warning, TEXT("Tutorial guide %s has incompatible idle and mesh skeletons."), *GetName());
		}
	}
}

void ARogue10mTutorialNPC::BeginPlay()
{
	Super::BeginPlay();
	ApplyGuideAppearance();
	DetectionArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleAreaBeginOverlap);
	DetectionArea->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleAreaEndOverlap);
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::FindOverlappingPlayer);
}

void ARogue10mTutorialNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseNearbyPlayer();
	Super::EndPlay(EndPlayReason);
}

const URogue10mTutorialDataAsset& ARogue10mTutorialNPC::GetDialogueData() const
{
	return TutorialData ? *TutorialData : *GetDefault<URogue10mTutorialDataAsset>();
}

bool ARogue10mTutorialNPC::IsPlayerAvailable(ARogue10mCharacter* Character, float Radius) const
{
	if (!IsValid(Character)) { return false; }
	const APlayerController* Controller = Cast<APlayerController>(Character->GetController());
	if (Character->IsDead() || !Controller || !Controller->IsLocalController()
		|| Controller->IsMoveInputIgnored() || UGameplayStatics::IsGamePaused(this)
		|| FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(Radius))
	{
		return false;
	}
	const ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(Controller);
	return !RogueController || !RogueController->IsAnyBlockingWindowVisible();
}

void ARogue10mTutorialNPC::FindOverlappingPlayer()
{
	TArray<AActor*> Overlaps;
	DetectionArea->GetOverlappingActors(Overlaps, ARogue10mCharacter::StaticClass());
	for (AActor* Actor : Overlaps)
	{
		ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(Actor);
		const APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (Controller && Controller->IsLocalController()) { ActivateNearbyPlayer(*Character); break; }
	}
}

void ARogue10mTutorialNPC::ActivateNearbyPlayer(ARogue10mCharacter& Character)
{
	APlayerController* Controller = Cast<APlayerController>(Character.GetController());
	if (!Controller || !Controller->IsLocalController() || NearbyCharacter.Get() == &Character) { return; }
	ReleaseNearbyPlayer();
	NearbyCharacter = &Character;
	NearbyController = Controller;
	EnableInput(Controller);
	bInputEnabled = true;
	if (InputComponent && !bInteractBound)
	{
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::HandleInteract).bConsumeInput = true;
		bInteractBound = true;
	}
	TutorialWidget = CreateWidget<URogue10mTutorialWidget>(Controller, URogue10mTutorialWidget::StaticClass());
	if (TutorialWidget) { TutorialWidget->AddToPlayerScreen(8); }
	GetWorldTimerManager().SetTimer(NearbyTimer, this, &ThisClass::UpdateNearbyPlayer, 0.1f, true);
	UpdateNearbyPlayer();
}

void ARogue10mTutorialNPC::ReleaseNearbyPlayer()
{
	GetWorldTimerManager().ClearTimer(NearbyTimer);
	if (NearbyController.IsValid()) { DisableInput(NearbyController.Get()); }
	bInputEnabled = false;
	if (TutorialWidget) { TutorialWidget->RemoveFromParent(); TutorialWidget = nullptr; }
	NearbyCharacter.Reset();
	NearbyController.Reset();
	bDialogueOpen = false;
	bRewardReceivedThisDialogue = false;
	DialogueStatus = FText::GetEmpty();
}

void ARogue10mTutorialNPC::HandleAreaBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(OtherActor)) { ActivateNearbyPlayer(*Character); }
}

void ARogue10mTutorialNPC::HandleAreaEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (OtherActor == NearbyCharacter.Get() && !DetectionArea->IsOverlappingActor(OtherActor))
	{
		ReleaseNearbyPlayer();
	}
}

void ARogue10mTutorialNPC::UpdateNearbyPlayer()
{
	ARogue10mCharacter* Character = NearbyCharacter.Get();
	if (!Character || !NearbyController.IsValid()
		|| Character->GetController() != NearbyController.Get()
		|| FVector::DistSquared(GetActorLocation(), Character->GetActorLocation())
			> FMath::Square(FMath::Max(InteractionRadius + 50.0f, DetectionRadius)))
	{
		ReleaseNearbyPlayer();
		return;
	}
	if (!IsPlayerAvailable(Character, InteractionRadius))
	{
		if (bInputEnabled) { DisableInput(NearbyController.Get()); bInputEnabled = false; }
		if (TutorialWidget) { TutorialWidget->SetVisibility(ESlateVisibility::Collapsed); }
		if (FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(InteractionRadius))
		{
			CloseDialogue();
		}
		return;
	}
	if (!bInputEnabled) { EnableInput(NearbyController.Get()); bInputEnabled = true; }
	RefreshDialogueView();
}

bool ARogue10mTutorialNPC::BeginDialogue(ARogue10mCharacter* Character)
{
	if (!IsPlayerAvailable(Character, InteractionRadius)) { return false; }
	if (NearbyCharacter.Get() != Character) { ActivateNearbyPlayer(*Character); }
	bDialogueOpen = true;
	bRewardReceivedThisDialogue = false;
	DialogueStatus = FText::GetEmpty();
	RefreshDialogueView();
	return true;
}

void ARogue10mTutorialNPC::CloseDialogue()
{
	bDialogueOpen = false;
	bRewardReceivedThisDialogue = false;
	DialogueStatus = FText::GetEmpty();
	if (IsPlayerAvailable(NearbyCharacter.Get(), InteractionRadius)) { RefreshDialogueView(); }
}

void ARogue10mTutorialNPC::HandleInteract()
{
	ARogue10mCharacter* Character = NearbyCharacter.Get();
	if (!IsPlayerAvailable(Character, InteractionRadius)) { return; }
	if (!bDialogueOpen) { BeginDialogue(Character); }
	else if (HasClaimedReward(Character) || !GetDialogueData().StartingItem) { CloseDialogue(); }
	else { ClaimStartingSupplies(); }
}

URogue10mTutorialRewardComponent* ARogue10mTutorialNPC::FindReceipts(ARogue10mCharacter* Character, bool bCreate) const
{
	APlayerState* State = Character ? Character->GetPlayerState() : nullptr;
	if (!State) { return nullptr; }
	URogue10mTutorialRewardComponent* Receipts = State->FindComponentByClass<URogue10mTutorialRewardComponent>();
	if (!Receipts && bCreate && State->HasAuthority())
	{
		Receipts = NewObject<URogue10mTutorialRewardComponent>(State, TEXT("TutorialRewardReceipts"));
		State->AddInstanceComponent(Receipts);
		Receipts->RegisterComponent();
	}
	return Receipts;
}

bool ARogue10mTutorialNPC::HasClaimedReward(ARogue10mCharacter* Character) const
{
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mTutorialSupplySubsystem* Supplies = Instance
		? Instance->GetSubsystem<URogue10mTutorialSupplySubsystem>() : nullptr;
	if (Supplies && Supplies->HasReceived(GetDialogueData().RewardId)) { return true; }
	const URogue10mTutorialRewardComponent* Receipts = FindReceipts(Character, false);
	return Receipts && Receipts->HasReceived(GetDialogueData().RewardId);
}

bool ARogue10mTutorialNPC::ClaimStartingSupplies()
{
	ARogue10mCharacter* Character = NearbyCharacter.Get();
	if (!bDialogueOpen || !IsPlayerAvailable(Character, InteractionRadius) || !Character->HasAuthority()) { return false; }
	const URogue10mTutorialDataAsset& Data = GetDialogueData();
	if (HasClaimedReward(Character))
	{
		DialogueStatus = NSLOCTEXT("Rogue10mTutorial", "AlreadyClaimed", "이번 보급품은 이미 수령했습니다.");
		RefreshDialogueView();
		return false;
	}
	URogue10mInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory || !Data.StartingItem || Data.RewardId.IsNone() || Data.StartingItemQuantity <= 0
		|| Data.StartingItemQuantity > FMath::Max(1, Data.StartingItem->MaxStackSize))
	{
		DialogueStatus = NSLOCTEXT("Rogue10mTutorial", "InvalidSupplies", "지금은 보급품을 준비할 수 없습니다.");
		UE_LOG(LogRogue10m, Warning, TEXT("Tutorial guide %s has invalid supply settings."), *GetName());
		RefreshDialogueView();
		return false;
	}
	URogue10mTutorialRewardComponent* Receipts = FindReceipts(Character, true);
	if (!Receipts || !Receipts->TryReserve(Data.RewardId)) { return false; }
	int32 ContainerIndex = INDEX_NONE;
	FGuid InstanceId;
	const bool bAdded = Inventory->TryAddGridItem(Data.StartingItem, Data.StartingItemQuantity, ContainerIndex, InstanceId);
	Receipts->FinishReservation(Data.RewardId, bAdded);
	bRewardReceivedThisDialogue = bAdded;
	DialogueStatus = bAdded
		? FText::Format(NSLOCTEXT("Rogue10mTutorial", "SuppliesReceived", "{0} × {1} 수령"),
			Data.StartingItem->DisplayName, FText::AsNumber(Data.StartingItemQuantity))
		: NSLOCTEXT("Rogue10mTutorial", "InventoryFull", "가방에 빈 공간이 부족합니다. I로 정리한 뒤 다시 받아 주세요.");
	if (bAdded)
	{
		if (UGameInstance* Instance = GetGameInstance())
		{
			if (URogue10mTutorialSupplySubsystem* Supplies = Instance->GetSubsystem<URogue10mTutorialSupplySubsystem>())
			{
				Supplies->RecordGrant(Data.RewardId, Data.StartingItem, Data.StartingItemQuantity, InstanceId);
			}
		}
		UE_LOG(LogRogue10m, Log, TEXT("Tutorial supply granted reward=%s player=%s item=%s quantity=%d container=%d instance=%s"),
			*Data.RewardId.ToString(), *Character->GetName(), *Data.StartingItem->GetName(),
			Data.StartingItemQuantity, ContainerIndex, *InstanceId.ToString());
	}
	RefreshDialogueView();
	return bAdded;
}

void ARogue10mTutorialNPC::RefreshDialogueView()
{
	if (!TutorialWidget) { return; }
	const URogue10mTutorialDataAsset& Data = GetDialogueData();
	const bool bReceived = HasClaimedReward(NearbyCharacter.Get());
	const FText Dialogue = !bDialogueOpen
		? NSLOCTEXT("Rogue10mTutorial", "NearbyPrompt", "출발 준비와 보급품을 안내해 드려요.")
		: bRewardReceivedThisDialogue ? Data.RewardReceivedText
		: bReceived ? Data.RewardAlreadyReceivedText : Data.WelcomeText;
	const FText Action = !bDialogueOpen
		? NSLOCTEXT("Rogue10mTutorial", "OpenAction", "F  대화하기")
		: bReceived || !Data.StartingItem
			? NSLOCTEXT("Rogue10mTutorial", "CloseAction", "F  대화 닫기")
			: NSLOCTEXT("Rogue10mTutorial", "ClaimAction", "F  출발 보급품 받기  ·  멀어지면 대화 종료");
	TutorialWidget->SetDialogueView(Data.GuideName, Data.GuideRole, Dialogue, DialogueStatus, Action);
	TutorialWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}
