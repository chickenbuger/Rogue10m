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
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialAnimationComponent.h"
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
	GuideAnimation = CreateDefaultSubobject<URogue10mTutorialAnimationComponent>(TEXT("GuideAnimation"));
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
	GuideAnimation->Configure(GuideMesh, TutorialData);
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

const URogue10mTownServiceDataAsset& ARogue10mTutorialNPC::GetServiceData() const
{
	return ServiceData ? *ServiceData : *GetDefault<URogue10mTownServiceDataAsset>();
}

bool ARogue10mTutorialNPC::IsWithinSkillTreeServiceRange(const ARogue10mCharacter* Character) const
{
	return IsValid(Character) && !IsActorBeingDestroyed() && !Character->IsDead() && Character->GetWorld() == GetWorld()
		&& FMath::IsFinite(InteractionRadius) && InteractionRadius > 0.0f
		&& FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

bool ARogue10mTutorialNPC::IsPlayerAvailable(ARogue10mCharacter* Character, float Radius) const
{
	if (!IsValid(Character)) { return false; }
	const APlayerController* Controller = Cast<APlayerController>(Character->GetController());
	const ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(Controller);
	const bool bOwnTraitWindow = RogueController && RogueController->IsTraitWindowVisible()
		&& RogueController->GetTraitServiceNPC() == this && NearbyController.Get() == Controller;
	if (Character->IsDead() || !Controller || !Controller->IsLocalController()
		|| (Controller->IsMoveInputIgnored() && !bOwnTraitWindow) || UGameplayStatics::IsGamePaused(this)
		|| FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(Radius))
	{
		return false;
	}
	return !RogueController || !RogueController->IsAnyBlockingWindowVisible() || bOwnTraitWindow;
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
	SetServiceInputEnabled(false);
	// Clear the dialogue first, so closing the child window cannot resume service input.
	bDialogueOpen = false;
	if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get()))
	{
		if (Controller->GetTraitServiceNPC() == this) { Controller->CloseTraitWindow(); }
	}
	if (NearbyController.IsValid()) { DisableInput(NearbyController.Get()); }
	bInputEnabled = false;
	if (TutorialWidget) { TutorialWidget->RemoveFromParent(); TutorialWidget = nullptr; }
	NearbyCharacter.Reset();
	NearbyController.Reset();
	bDialogueOpen = false;
	GuideAnimation->SetDialogueOpen(false);
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
	if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get()))
	{
		if (Controller->IsTraitWindowVisible() && Controller->GetTraitServiceNPC() == this)
		{
			FText Reason;
			if (!CanUseTraitWindow(Reason)) { CloseDialogue(); }
			else
			{
				SetServiceInputEnabled(false);
				if (bInputEnabled) { DisableInput(Controller); bInputEnabled = false; }
				if (TutorialWidget) { TutorialWidget->SetVisibility(ESlateVisibility::Collapsed); }
			}
			return;
		}
	}
	if (!IsPlayerAvailable(Character, InteractionRadius))
	{
		CloseDialogue();
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
	SelectedServiceIndex = 0;
	SetServiceInputEnabled(true);
	GuideAnimation->Configure(GuideMesh, TutorialData);
	GuideAnimation->SetDialogueOpen(true);
	bRewardReceivedThisDialogue = false;
	DialogueStatus = FText::GetEmpty();
	RefreshDialogueView();
	return true;
}

void ARogue10mTutorialNPC::CloseDialogue()
{
	SetServiceInputEnabled(false);
	bDialogueOpen = false;
	if (ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get()))
	{
		if (Controller->GetTraitServiceNPC() == this) { Controller->CloseTraitWindow(); }
	}
	GuideAnimation->SetDialogueOpen(false);
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
	const URogue10mCharacterProfileSubsystem* Profiles = Instance
		? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	const URogue10mTutorialSupplySubsystem* Supplies = Instance
		? Instance->GetSubsystem<URogue10mTutorialSupplySubsystem>() : nullptr;
	// Controller/PlayerState can be reused when switching same-class profiles.
	// Persistent receipts are authoritative; another profile's transient receipt must not leak.
	if (Selected)
	{
		if (!Selected->IsValid()) { return false; }
		if (Supplies) { return Supplies->HasReceived(GetDialogueData().RewardId); }
		const URogue10mProgressionSubsystem* Progression = Instance->GetSubsystem<URogue10mProgressionSubsystem>();
		return Progression && Progression->HasClaimedReward(GetDialogueData().RewardId);
	}
	if (Supplies && Supplies->HasReceived(GetDialogueData().RewardId)) { return true; }
	const URogue10mTutorialRewardComponent* Receipts = FindReceipts(Character, false);
	return Receipts && Receipts->HasReceived(GetDialogueData().RewardId);
}

bool ARogue10mTutorialNPC::ClaimStartingSupplies()
{
	ARogue10mCharacter* Character = NearbyCharacter.Get();
	if (!bDialogueOpen || !IsPlayerAvailable(Character, InteractionRadius) || !Character->HasAuthority()) { return false; }
	if (!IsProgressionSaveAvailable()) { return false; }
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
	FName ReservationId = Data.RewardId;
	if (const UGameInstance* Instance = GetGameInstance())
	{
		const URogue10mCharacterProfileSubsystem* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
		if (Selected)
		{
			// IsProgressionSaveAvailable already rejects invalid/unready profiles.
			ReservationId = FName(*(Data.RewardId.ToString() + TEXT("_") + Selected->ProfileId.ToString(EGuidFormats::Digits)));
		}
	}
	if (!Receipts || !Receipts->TryReserve(ReservationId)) { return false; }
	int32 ContainerIndex = INDEX_NONE;
	FGuid InstanceId;
	const bool bAdded = Inventory->TryAddGridItem(Data.StartingItem, Data.StartingItemQuantity, ContainerIndex, InstanceId);
	Receipts->FinishReservation(ReservationId, bAdded);
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
	const ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get());
	if (Controller && Controller->IsTraitWindowVisible() && Controller->GetTraitServiceNPC() == this)
	{
		TutorialWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
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
	TutorialWidget->SetDialogueView(Data.GuideName, Data.GuideRole, Dialogue, GetDialogueStatus(), Action);
	TutorialWidget->SetServiceView(bDialogueOpen ? BuildServiceText() : FText::GetEmpty());
	TutorialWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

URogue10mTownProgressionComponent* ARogue10mTutorialNPC::FindTownProgression() const
{
	const ARogue10mCharacter* Character = NearbyCharacter.Get();
	const APlayerState* State = Character ? Character->GetPlayerState() : nullptr;
	return State ? State->FindComponentByClass<URogue10mTownProgressionComponent>() : nullptr;
}

bool ARogue10mTutorialNPC::CanUseTownServices()
{
	ARogue10mCharacter* Character = NearbyCharacter.Get();
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (!bDialogueOpen || !HasAuthority() || !Character || !Character->HasAuthority()
		|| !IsPlayerAvailable(Character, InteractionRadius)
		|| UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("StartHub")
		|| !Run || Run->GetRunPhase() != ERogue10mRunPhase::WaitingToStart)
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "Unavailable", "마을에서 살아 있는 상태로 출발 전에만 이용할 수 있습니다."));
	}
	if (!IsProgressionSaveAvailable()) { return false; }
	if (!GetServiceData().IsTuningValid())
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "InvalidSettings", "마을 서비스의 비용 또는 능력치 설정을 확인해 주세요."));
	}
	return true;
}

bool ARogue10mTutorialNPC::IsProgressionSaveAvailable()
{
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mCharacterProfileSubsystem* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const URogue10mProgressionSubsystem* Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (Selected && !Selected->IsValid())
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "InvalidProfile", "선택한 캐릭터 정보가 올바르지 않아 보급과 서비스를 이용할 수 없습니다."));
	}
	if (Selected && (!Progression || !Progression->IsSaveReady()))
	{
		return FinishService(false, FText::GetEmpty(), Progression && !Progression->GetLastError().IsEmpty()
			? Progression->GetLastError() : NSLOCTEXT("Rogue10mTown", "SaveUnavailable", "캐릭터 진행 저장을 준비하지 못했습니다. 저장 오류를 확인해 주세요."));
	}
	return true;
}

FText ARogue10mTutorialNPC::GetDialogueStatus() const
{
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mCharacterProfileSubsystem* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const URogue10mProgressionSubsystem* Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	if (Profiles && Profiles->GetSelectedProfile() && Progression && !Progression->GetLastError().IsEmpty())
	{
		const FText SaveError = Progression->GetLastError();
		if (DialogueStatus.IsEmpty() || DialogueStatus.ToString() == SaveError.ToString()) { return SaveError; }
		return FText::Format(NSLOCTEXT("Rogue10mTown", "ResultAndSaveError", "{0}\n저장 확인: {1}"), DialogueStatus, SaveError);
	}
	return DialogueStatus;
}

bool ARogue10mTutorialNPC::FinishService(bool bSucceeded, const FText& Success, const FText& Failure)
{
	DialogueStatus = bSucceeded ? Success : Failure;
	RefreshDialogueView();
	return bSucceeded;
}

bool ARogue10mTutorialNPC::PurchasePotion()
{
	if (!CanUseTownServices()) { return false; }
	URogue10mInventoryComponent* Inventory = NearbyCharacter->GetInventoryComponent();
	const URogue10mItemDataAsset* Potion = GetDialogueData().StartingItem;
	const int32 Cost = GetServiceData().PotionGoldCost;
	if (!Inventory || !Potion || Potion->Category != ERogue10mItemCategory::Consumable)
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "PotionUnavailable", "지금은 판매할 회복 물약이 없습니다."));
	}
	if (Inventory->GetGold() < Cost)
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "NeedGold", "골드가 부족합니다. 탐험에서 몬스터를 처치해 모아 주세요."));
	}
	return FinishService(Inventory->TryPurchaseGridItem(Potion, 1, Cost),
		FText::Format(NSLOCTEXT("Rogue10mTown", "Purchased", "{0} × 1 구매 완료 · {1} 골드"), Potion->DisplayName, FText::AsNumber(Cost)),
		NSLOCTEXT("Rogue10mTown", "BagFull", "가방 공간이 부족합니다. 골드는 차감되지 않았습니다."));
}

bool ARogue10mTutorialNPC::EnhanceEquippedItem(ERogue10mInventorySlotType SlotType)
{
	if (!CanUseTownServices()) { return false; }
	URogue10mInventoryComponent* Inventory = NearbyCharacter->GetInventoryComponent();
	const URogue10mTownServiceDataAsset& Data = GetServiceData();
	if (!Inventory || (SlotType != ERogue10mInventorySlotType::MainWeapon && SlotType != ERogue10mInventorySlotType::Armor)
		|| !Inventory->GetEquippedItemData(SlotType))
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "NeedEquipment", "먼저 강화할 무기 또는 몸통 방어구를 장착해 주세요."));
	}
	const int32 Level = Inventory->GetEquippedItemEnhancementLevel(SlotType);
	if (Level >= Data.MaximumEnhancementLevel)
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "EnhanceMax", "이미 최대 강화 단계입니다."));
	}
	const int32 Cost = Data.EnhancementGoldCost * (Level + 1);
	if (Inventory->GetGold() < Cost)
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "EnhanceNeedGold", "강화에 필요한 골드가 부족합니다."));
	}
	const bool bWeapon = SlotType == ERogue10mInventorySlotType::MainWeapon;
	return FinishService(Inventory->TryEnhanceEquippedItem(SlotType, Cost, Data.MaximumEnhancementLevel,
		bWeapon ? Data.EnhancementAttackPerLevel : 0.0f, bWeapon ? 0.0f : Data.EnhancementDefensePerLevel),
		FText::Format(NSLOCTEXT("Rogue10mTown", "Enhanced", "강화 +{0} 완료 · {1} 골드"), FText::AsNumber(Level + 1), FText::AsNumber(Cost)),
		NSLOCTEXT("Rogue10mTown", "EnhanceFailed", "현재 장비를 강화할 수 없습니다."));
}

bool ARogue10mTutorialNPC::UpgradeTrait(ERogue10mTownTrait Trait)
{
	if (!CanUseTownServices()) { return false; }
	FText Reason;
	if (!CanUseTraitWindow(Reason)) { return FinishService(false, FText::GetEmpty(), Reason); }
	URogue10mTownProgressionComponent* Town = FindTownProgression();
	if (!Town) { return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTown", "NoProgression", "특성 진행 정보를 준비하지 못했습니다.")); }
	if (!Town->CanUpgradeTrait(Trait, Reason, &GetServiceData()))
	{
		return FinishService(false, FText::GetEmpty(), Reason);
	}
	const FRogue10mTraitDefinition Definition = Town->GetUpgradeDefinition(Trait, &GetServiceData());
	return FinishService(Town->TryUpgradeTrait(Trait, &GetServiceData()),
		FText::Format(NSLOCTEXT("Rogue10mTown", "TraitUpgradeCost", "{0} 강화 완료 · 특성 포인트 {1} 사용"), Definition.DisplayName, FText::AsNumber(Definition.PointCost)),
		NSLOCTEXT("Rogue10mTown", "TraitUpgradeFailed", "현재 상태에서 특성을 강화할 수 없습니다."));
}

bool ARogue10mTutorialNPC::CanUseTraitWindow(FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	const ARogue10mCharacter* Character = NearbyCharacter.Get();
	const ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get());
	const URogue10mTownProgressionComponent* Town = FindTownProgression();
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (!bDialogueOpen || !HasAuthority() || !IsValid(Character) || !Character->HasAuthority()
		|| !Controller || Character->GetController() != Controller || Controller->GetPawn() != Character
		|| !IsPlayerAvailable(NearbyCharacter.Get(), InteractionRadius) || !Town || GetNetMode() != NM_Standalone
		|| UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("StartHub")
		|| !Run || Run->GetRunPhase() != ERogue10mRunPhase::WaitingToStart)
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "UnavailableNPC", "마을 안내인과 대화 중이고 살아 있는 상태에서 출발 전에만 이용할 수 있습니다.");
		return false;
	}
	const UGameInstance* Instance = GetGameInstance();
	const URogue10mCharacterProfileSubsystem* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
	const URogue10mProgressionSubsystem* Save = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	const FRogue10mCharacterProfile* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
	if (Selected && (!Selected->IsValid() || !Save || (!Save->IsSaveReady() && !Save->CanRetrySave()) || !Save->IsAttachedTo(*Character)
		|| Save->GetAttachedProfileId() != Selected->ProfileId))
	{
		OutReason = Save && !Save->GetLastError().IsEmpty() ? Save->GetLastError()
			: NSLOCTEXT("Rogue10mTraits", "SaveNotReady", "캐릭터 진행 저장을 준비하지 못했습니다. 저장 오류를 확인해 주세요.");
		return false;
	}
	if (!GetServiceData().IsTuningValid())
	{
		OutReason = NSLOCTEXT("Rogue10mTraits", "InvalidNPCData", "마을 특성 강화 설정을 확인해 주세요.");
		return false;
	}
	return true;
}

bool ARogue10mTutorialNPC::OpenTraitWindow()
{
	FText Reason;
	if (!CanUseTraitWindow(Reason)) { return FinishService(false, FText::GetEmpty(), Reason); }
	ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(NearbyController.Get());
	if (!Controller || !Controller->OpenTraitWindow(this))
	{
		return FinishService(false, FText::GetEmpty(), NSLOCTEXT("Rogue10mTraits", "OpenFailed", "특성 강화 창을 열지 못했습니다."));
	}
	SetServiceInputEnabled(false);
	if (bInputEnabled) { DisableInput(Controller); bInputEnabled = false; }
	if (TutorialWidget) { TutorialWidget->SetVisibility(ESlateVisibility::Collapsed); }
	return true;
}

bool ARogue10mTutorialNPC::ResetTraits()
{
	FText Reason;
	if (!CanUseTraitWindow(Reason)) { return FinishService(false, FText::GetEmpty(), Reason); }
	URogue10mTownProgressionComponent* Town = FindTownProgression();
	const int32 Refund = Town ? Town->GetSpentTraitPoints() : 0;
	return FinishService(Town && Town->TryResetTraits(),
		FText::Format(NSLOCTEXT("Rogue10mTraits", "ResetComplete", "특성 재분배 완료 · {0}포인트 반환"), FText::AsNumber(Refund)),
		NSLOCTEXT("Rogue10mTraits", "ResetFailed", "현재 상태에서는 특성을 재분배할 수 없습니다."));
}

void ARogue10mTutorialNPC::ResumeAfterTraitWindow()
{
	if (!bDialogueOpen || !IsPlayerAvailable(NearbyCharacter.Get(), InteractionRadius)) { return; }
	if (!bInputEnabled && NearbyController.IsValid()) { EnableInput(NearbyController.Get()); bInputEnabled = true; }
	SetServiceInputEnabled(true);
	RefreshDialogueView();
}

bool ARogue10mTutorialNPC::SelectDifficulty(ERogue10mDifficulty Difficulty)
{
	if (!CanUseTownServices()) { return false; }
	URogue10mTownProgressionComponent* Town = FindTownProgression();
	return FinishService(Town && Town->TrySetDifficulty(Difficulty, &GetServiceData()),
		NSLOCTEXT("Rogue10mTown", "DifficultySelected", "다음 탐험의 난이도를 선택했습니다. 포탈 진입 후 적용됩니다."),
		NSLOCTEXT("Rogue10mTown", "DifficultyFailed", "지금은 난이도를 선택할 수 없습니다."));
}

void ARogue10mTutorialNPC::SetServiceInputEnabled(bool bEnabled)
{
	APlayerController* Controller = NearbyController.Get();
	if (bEnabled && Controller && !bServiceInputPushed)
	{
		if (!ServiceInputComponent)
		{
			ServiceInputComponent = NewObject<UInputComponent>(this, TEXT("TownServiceInput"));
			ServiceInputComponent->Priority = InputPriority + 1;
			ServiceInputComponent->RegisterComponent();
			ServiceInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ThisClass::SelectPreviousService).bConsumeInput = true;
			ServiceInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ThisClass::SelectNextService).bConsumeInput = true;
			ServiceInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ExecuteSelectedService).bConsumeInput = true;
			ServiceInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::CloseDialogue).bConsumeInput = true;
		}
		Controller->PushInputComponent(ServiceInputComponent);
		bServiceInputPushed = true;
	}
	else if (!bEnabled && bServiceInputPushed)
	{
		if (Controller && ServiceInputComponent) { Controller->PopInputComponent(ServiceInputComponent); }
		bServiceInputPushed = false;
	}
}

void ARogue10mTutorialNPC::SelectPreviousService()
{
	if (!bDialogueOpen) { return; }
	SelectedServiceIndex = (SelectedServiceIndex + 6) % 7;
	RefreshDialogueView();
}

void ARogue10mTutorialNPC::SelectNextService()
{
	if (!bDialogueOpen) { return; }
	SelectedServiceIndex = (SelectedServiceIndex + 1) % 7;
	RefreshDialogueView();
}

void ARogue10mTutorialNPC::ExecuteSelectedService()
{
	switch (SelectedServiceIndex)
	{
	case 0: PurchasePotion(); break;
	case 1: EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon); break;
	case 2: EnhanceEquippedItem(ERogue10mInventorySlotType::Armor); break;
	case 3: OpenTraitWindow(); break;
	case 4: SelectDifficulty(ERogue10mDifficulty::Easy); break;
	case 5: SelectDifficulty(ERogue10mDifficulty::Normal); break;
	case 6: SelectDifficulty(ERogue10mDifficulty::Hard); break;
	default: break;
	}
}

FText ARogue10mTutorialNPC::BuildServiceText() const
{
	const URogue10mTownServiceDataAsset& Data = GetServiceData();
	const ARogue10mCharacter* Character = NearbyCharacter.Get();
	const URogue10mInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	const URogue10mTownProgressionComponent* Town = FindTownProgression();
	const int32 WeaponLevel = Inventory ? Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::MainWeapon) : 0;
	const int32 ArmorLevel = Inventory ? Inventory->GetEquippedItemEnhancementLevel(ERogue10mInventorySlotType::Armor) : 0;
	const FString DifficultyName = !Town || Town->GetDifficulty() == ERogue10mDifficulty::Normal ? TEXT("보통")
		: Town->GetDifficulty() == ERogue10mDifficulty::Easy ? TEXT("쉬움") : TEXT("어려움");
	const auto CostText = [&Data](int32 Level)
	{
		return Level >= Data.MaximumEnhancementLevel ? FString(TEXT("최대 단계"))
			: FString::Printf(TEXT("%lld 골드"), static_cast<int64>(Data.EnhancementGoldCost) * (static_cast<int64>(Level) + 1));
	};
	TArray<FString> Rows;
	Rows.Add(FString::Printf(TEXT("회복 물약 1개 구매 · %d 골드"), Data.PotionGoldCost));
	Rows.Add(FString::Printf(TEXT("장착 무기 +%d 강화 · %s · 공격 +%g"), WeaponLevel, *CostText(WeaponLevel), Data.EnhancementAttackPerLevel));
	Rows.Add(FString::Printf(TEXT("장착 방어구 +%d 강화 · %s · 방어 +%g"), ArmorLevel, *CostText(ArmorLevel), Data.EnhancementDefensePerLevel));
	Rows.Add(FString::Printf(TEXT("특성 강화 창 열기 · 16종 · 보유 %d포인트"), Town ? Town->GetAvailableTraitPoints() : 0));
	const auto DifficultyRow = [&Rows](const TCHAR* Label, const FRogue10mDifficultyModifiers& Scale)
	{
		Rows.Add(FString::Printf(TEXT("%s · 적 HP ×%g / 피해 ×%g / XP ×%g"), Label, Scale.HealthMultiplier, Scale.DamageMultiplier, Scale.ExperienceMultiplier));
	};
	DifficultyRow(TEXT("쉬움"), Data.Easy);
	DifficultyRow(TEXT("보통"), Data.Normal);
	DifficultyRow(TEXT("어려움"), Data.Hard);
	FString Text = FString::Printf(TEXT("마을 서비스 · %d 골드 · 특성 %d포인트\n다음 탐험: %s\n"), Inventory ? Inventory->GetGold() : 0, Town ? Town->GetAvailableTraitPoints() : 0, *DifficultyName);
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		Text += FString::Printf(TEXT("%s %s\n"), Index == SelectedServiceIndex ? TEXT("▶") : TEXT("  "), *Rows[Index]);
	}
	Text += TEXT("↑↓ 선택 · Enter 이용 · Esc 닫기\n레벨 상승: 특성 +1 · 몬스터 처치: 골드 획득");
	return FText::FromString(Text);
}
