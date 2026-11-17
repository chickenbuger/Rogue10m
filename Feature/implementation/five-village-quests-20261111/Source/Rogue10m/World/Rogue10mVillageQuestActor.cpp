#include "Rogue10mVillageQuestActor.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "UObject/ConstructorHelpers.h"

ARogue10mVillageQuestActor::ARogue10mVillageQuestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Current production is local single-player; no client reward RPC.
	InputPriority = 8;
	bBlockInput = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("QuestMarker"));
	Marker->SetupAttachment(SceneRoot);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetGenerateOverlapEvents(false);
	Marker->SetCanEverAffectNavigation(false);
	Marker->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Marker->SetStaticMesh(Cube.Object); }
	Prompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("QuestPrompt"));
	Prompt->SetupAttachment(SceneRoot);
	Prompt->SetRelativeLocation(FVector(0.0, 0.0, 180.0));
	Prompt->SetWorldSize(24.0f);
	Prompt->SetHorizontalAlignment(EHTA_Center);
	Prompt->SetTextRenderColor(FColor(245, 222, 125));
	Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARogue10mVillageQuestActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Marker->SetRelativeScale3D(Role == ERogue10mVillageQuestActorRole::Board
		? FVector(0.18, 1.1, 1.4) : FVector(0.75, 0.75, 0.65));
	Prompt->SetAbsolute(false, true, true);
	RefreshPrompt(nullptr);
}

URogue10mVillageQuestDataAsset* ARogue10mVillageQuestActor::GetDefinition() const
{
	return QuestDefinition ? QuestDefinition.Get() : GetMutableDefault<URogue10mVillageQuestDataAsset>();
}

void ARogue10mVillageQuestActor::BeginPlay()
{
	Super::BeginPlay();
	// A timer replaces actor Tick; outside the candidate it only shows an inert label.
	GetWorldTimerManager().SetTimer(ProximityTimer, this, &ThisClass::RefreshNearbyPlayer, 0.2f, true);
	RefreshNearbyPlayer();
}

void ARogue10mVillageQuestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ProximityTimer);
	SetInteractionInput(nullptr);
	NearbyCharacter.Reset();
	Super::EndPlay(EndPlayReason);
}

bool ARogue10mVillageQuestActor::CanInteract(const ARogue10mCharacter* Character) const
{
	const UWorld* World = GetWorld();
	const ARogue10mGameState* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	const ARogue10mPlayerController* Controller = IsValid(Character)
		? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	if (!HasAuthority() || !World || !IsValid(Character) || Character->GetWorld() != World
		|| Character->IsDead() || !Controller || !Controller->IsLocalController()
		|| Controller->GetPawn() != Character || Controller->IsAnyBlockingWindowVisible()
		|| !Run || !Run->IsRunActive() || World->IsPaused()
		|| UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
			!= TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110")
		|| !FMath::IsFinite(InteractionRadius) || InteractionRadius < 100.0f || InteractionRadius > 400.0f
		|| FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(InteractionRadius))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VillageQuestInteraction), false, Character);
	Params.AddIgnoredActor(this);
	// Prevent interaction through a wall without requiring a gameplay camera ray hit.
	const FVector Eye = Character->GetPawnViewLocation();
	return !World->LineTraceSingleByChannel(Hit, Eye, GetActorLocation() + FVector(0, 0, 65),
		ECC_Visibility, Params);
}

void ARogue10mVillageQuestActor::SetInteractionInput(APlayerController* Controller)
{
	if (InputController.Get() == Controller) { return; }
	if (APlayerController* Previous = InputController.Get()) { DisableInput(Previous); }
	InputController = Controller;
	if (!Controller) { return; }
	EnableInput(Controller);
	if (InputComponent && !bInputBound)
	{
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::HandleInteract).bConsumeInput = true;
		bInputBound = true;
	}
}

void ARogue10mVillageQuestActor::RefreshNearbyPlayer()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	ARogue10mCharacter* Character = Controller ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
	if (!CanInteract(Character)) { Character = nullptr; }
	NearbyCharacter = Character;
	SetInteractionInput(Character ? Controller : nullptr);
	RefreshPrompt(Character);
}

void ARogue10mVillageQuestActor::HandleInteract()
{
	Interact(NearbyCharacter.Get());
}

bool ARogue10mVillageQuestActor::Interact(ARogue10mCharacter* Character)
{
	if (!CanInteract(Character)) { return false; }
	URogue10mVillageQuestDataAsset* Definition = GetDefinition();
	URogue10mVillageQuestComponent* Quests = URogue10mVillageQuestComponent::GetOrCreate(Character);
	if (!Definition || !Quests || !Quests->RegisterQuest(Definition)) { return false; }
	bool bSucceeded = false;
	if (Role == ERogue10mVillageQuestActorRole::Recovery)
	{
		bSucceeded = Quests->RecordObjective(Definition->QuestId, ObjectiveId);
	}
	else
	{
		const ERogue10mVillageQuestStatus Status = Quests->GetQuestStatus(Definition->QuestId);
		if (Status == ERogue10mVillageQuestStatus::Unaccepted)
		{
			bSucceeded = Quests->AcceptQuest(Definition->QuestId);
		}
		else if (Status == ERogue10mVillageQuestStatus::Ready)
		{
			bSucceeded = Quests->TryClaimReward(Definition->QuestId);
			if (!bSucceeded && Quests->GetQuestStatus(Definition->QuestId) == ERogue10mVillageQuestStatus::Ready)
			{
				LastFailure = NSLOCTEXT("VillageQuest", "RewardRetry", "보상을 받을 수 없습니다. 가방 공간을 확인하고 다시 보고하세요.");
			}
		}
	}
	if (bSucceeded) { LastFailure = FText::GetEmpty(); }
	RefreshPrompt(Character);
	return bSucceeded;
}

void ARogue10mVillageQuestActor::RefreshPrompt(ARogue10mCharacter* Character)
{
	if (!Prompt) { return; }
	URogue10mVillageQuestDataAsset* Definition = GetDefinition();
	FString Text = Role == ERogue10mVillageQuestActorRole::Board
		? FString::Printf(TEXT("선택 의뢰 · %s\n%s"),
			Definition ? *Definition->Title.ToString() : TEXT("분실 보급품"),
			Definition ? *Definition->Description.ToString() : TEXT("V3의 보급품을 회수해 V1에 보고"))
		: TEXT("분실 보급품\nV1 선택 의뢰 · 수락 전에도 회수 가능");
	if (IsValid(Character) && Definition)
	{
		URogue10mVillageQuestComponent* Quests = URogue10mVillageQuestComponent::GetOrCreate(Character);
		if (Quests && Quests->RegisterQuest(Definition))
		{
			const ERogue10mVillageQuestStatus Status = Quests->GetQuestStatus(Definition->QuestId);
			if (Status == ERogue10mVillageQuestStatus::Claimed || Status == ERogue10mVillageQuestStatus::Unaccepted)
			{
				LastFailure = FText::GetEmpty();
			}
			if (Role == ERogue10mVillageQuestActorRole::Recovery)
			{
				Text += Quests->HasObjective(Definition->QuestId, ObjectiveId)
					? TEXT("\n회수 완료 · V1에서 수락/보고") : TEXT("\n[F] 회수 · 선택 사항");
			}
			else
			{
				switch (Status)
				{
				case ERogue10mVillageQuestStatus::Unaccepted:
					Text += FString::Printf(TEXT("\n[F] 수락 · 보상 아이템 %d개"), Definition->RewardQuantity); break;
				case ERogue10mVillageQuestStatus::Accepted: Text += TEXT("\n진행 중 · 회수 후 돌아와 보고"); break;
				case ERogue10mVillageQuestStatus::Ready: Text += TEXT("\n[F] 보고하고 보상 받기"); break;
				case ERogue10mVillageQuestStatus::Claiming: Text += TEXT("\n보상 처리 중"); break;
				case ERogue10mVillageQuestStatus::Claimed: Text += TEXT("\n완료 · 이번 런 보상 수령"); break;
				}
			}
		}
		if (!LastFailure.IsEmpty()) { Text += TEXT("\n") + LastFailure.ToString(); }
		const FVector Facing = Character->GetActorLocation() - Prompt->GetComponentLocation();
		Prompt->SetWorldRotation(FRotator(0.0, Facing.Rotation().Yaw, 0.0));
	}
	Prompt->SetText(FText::FromString(Text));
}
