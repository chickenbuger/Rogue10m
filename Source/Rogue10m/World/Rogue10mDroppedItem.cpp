// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mDroppedItem.h"

#include "Rogue10mItemDataAsset.h"
#include "Rogue10mCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARogue10mDroppedItem::ARogue10mDroppedItem()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(SceneRoot);
	ItemMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.16f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ItemMesh->SetStaticMesh(CubeMesh.Object);
	}

	ItemNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemNameText"));
	ItemNameText->SetupAttachment(SceneRoot);
	ItemNameText->SetRelativeLocation(FVector(0.0f, 0.0f, 44.0f));
	ItemNameText->SetHorizontalAlignment(EHTA_Center);
	ItemNameText->SetTextRenderColor(FColor(235, 245, 255));
	ItemNameText->SetWorldSize(18.0f);
	ItemNameText->SetText(FText::FromString(TEXT("드랍 아이템")));
}

void ARogue10mDroppedItem::InitializeDroppedItem(const FRogue10mItemStack& NewItemStack)
{
	ItemStack = NewItemStack;

	if (ItemMesh)
	{
		ItemMesh->SetCustomPrimitiveDataFloat(0, ItemStack.ItemColor.R);
		if (ItemStack.ItemData && !ItemStack.ItemData->DroppedWorldMesh.IsNull())
		{
			ItemMesh->SetStaticMesh(ItemStack.ItemData->DroppedWorldMesh.LoadSynchronous());
		}
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(ItemStack.DisplayName.IsEmpty() ? FText::FromString(TEXT("이름 없는 아이템")) : ItemStack.DisplayName);
	}
}

void ARogue10mDroppedItem::BeginPlay()
{
	Super::BeginPlay();
	ItemMesh->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandlePickupOverlap);
	ItemMesh->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandlePickupEndOverlap);
}

void ARogue10mDroppedItem::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(OtherActor);
	if (!TryPickup(Character)) { WatchPickupRetry(Character); }
}

void ARogue10mDroppedItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPickupRetry();
	Super::EndPlay(EndPlayReason);
}

void ARogue10mDroppedItem::WatchPickupRetry(ARogue10mCharacter* Character)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !Character || Character->IsDead()
		|| !ItemMesh || !ItemMesh->IsOverlappingActor(Character)) { return; }
	URogue10mInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory) { return; }
	if (PickupRetryCharacter.Get() == Character && PickupRetryInventory.Get() == Inventory) { return; }
	ClearPickupRetry();
	PickupRetryCharacter = Character;
	PickupRetryInventory = Inventory;
	Inventory->OnInventoryGridChanged.AddUniqueDynamic(this, &ThisClass::HandlePickupInventoryChanged);
}

void ARogue10mDroppedItem::HandlePickupEndOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (OtherActor == PickupRetryCharacter.Get() && (!ItemMesh || !ItemMesh->IsOverlappingActor(OtherActor)))
	{ ClearPickupRetry(); }
}

void ARogue10mDroppedItem::HandlePickupInventoryChanged()
{
	UWorld* World = GetWorld();
	if (bPickupInProgress || IsActorBeingDestroyed() || !World || PickupRetryTimer.IsValid()) { return; }
	// Inventory events can occur inside a larger drop/equipment transaction. Wait
	// until it finishes before using the resulting capacity, and coalesce events.
	PickupRetryTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RetryOverlappingPickup);
}

void ARogue10mDroppedItem::RetryOverlappingPickup()
{
	PickupRetryTimer.Invalidate();
	ARogue10mCharacter* Character = PickupRetryCharacter.Get();
	URogue10mInventoryComponent* Inventory = PickupRetryInventory.Get();
	if (!HasAuthority() || IsActorBeingDestroyed() || !Character || Character->IsDead() || !Inventory
		|| Character->GetInventoryComponent() != Inventory || !ItemMesh || !ItemMesh->IsOverlappingActor(Character)
		|| FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(250.0f))
	{ ClearPickupRetry(); return; }
	TryPickup(Character);
}

void ARogue10mDroppedItem::ClearPickupRetry()
{
	if (URogue10mInventoryComponent* Inventory = PickupRetryInventory.Get())
	{ Inventory->OnInventoryGridChanged.RemoveDynamic(this, &ThisClass::HandlePickupInventoryChanged); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(PickupRetryTimer); }
	PickupRetryTimer.Invalidate();
	PickupRetryCharacter.Reset();
	PickupRetryInventory.Reset();
}

bool ARogue10mDroppedItem::TryPickup(ARogue10mCharacter* Character)
{
	if (bPickupInProgress || IsActorBeingDestroyed() || !HasAuthority() || !Character || Character->IsDead()
		|| FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(250.0f))
	{
		return false;
	}
	URogue10mInventoryComponent* Inventory = Character->GetInventoryComponent();
	TGuardValue<bool> PickupGuard(bPickupInProgress, true);
	int32 ContainerIndex;
	FGuid InstanceId;
	if (!Inventory || !Inventory->TryAddItemStack(ItemStack, ContainerIndex, InstanceId)) { return false; }
	ClearPickupRetry();
	Destroy();
	return true;
}
