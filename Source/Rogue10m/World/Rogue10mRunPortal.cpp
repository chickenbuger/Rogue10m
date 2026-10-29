// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mRunPortal.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mTutorialSupplySubsystem.h"
#include "Rogue10mProgressionSubsystem.h"

ARogue10mRunPortal::ARogue10mRunPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);

	PortalCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Portal Collision"));
	PortalCollision->SetupAttachment(SceneRoot);
	PortalCollision->SetBoxExtent(FVector(120.0f, 80.0f, 180.0f));
	PortalCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PortalCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PortalCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PortalCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Portal Mesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARogue10mRunPortal::BeginPlay()
{
	Super::BeginPlay();
	PortalCollision->OnComponentBeginOverlap.AddDynamic(this, &ARogue10mRunPortal::HandlePortalBeginOverlap);
}

void ARogue10mRunPortal::HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bTravelStarted || TargetLevelName.IsNone() || !Cast<ARogue10mCharacter>(OtherActor))
	{
		return;
	}

	bTravelStarted = true;
	if (const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(OtherActor))
	{
		if (UGameInstance* Instance = GetGameInstance())
		{
			if (URogue10mProgressionSubsystem* Progression = Instance->GetSubsystem<URogue10mProgressionSubsystem>())
			{
				if (Progression->IsAttachedTo(*Character)
					&& !(bStartRunTimerOnTravel ? Progression->SaveRunEntryCurrencyBaseline() : Progression->SaveNow()))
				{
					bTravelStarted = false;
					UE_LOG(LogRogue10m, Error, TEXT("Portal travel stopped: character progression could not be saved."));
					return;
				}
			}
			if (URogue10mTutorialSupplySubsystem* Supplies = Instance->GetSubsystem<URogue10mTutorialSupplySubsystem>())
			{
				if (const URogue10mInventoryComponent* Inventory = Character->GetInventoryComponent())
				{
					Supplies->CaptureSuppliesForTravel(*Inventory);
				}
			}
		}
	}
	UE_LOG(LogRogue10m, Log, TEXT("Run portal opened level '%s' with options '%s'."), *TargetLevelName.ToString(), *BuildTravelOptions());
	UGameplayStatics::OpenLevel(this, TargetLevelName, true, BuildTravelOptions());
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
