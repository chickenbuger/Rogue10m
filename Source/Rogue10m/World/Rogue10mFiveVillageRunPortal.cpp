// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mFiveVillageRunPortal.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ARogue10mFiveVillageRunPortal::ARogue10mFiveVillageRunPortal()
{
	TargetLevelName = FName(TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"));
	bStartRunTimerOnTravel = true;
	AdditionalTravelOptions = TEXT("game=/Script/Rogue10m.Rogue10mFiveVillageGameMode");

	LeftPillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Left Pillar"));
	RightPillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Right Pillar"));
	LeftPillar->SetupAttachment(SceneRoot);
	RightPillar->SetupAttachment(SceneRoot);
	LeftPillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightPillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftPillar->SetRelativeLocation(FVector(-110.0, 0.0, 0.0));
	RightPillar->SetRelativeLocation(FVector(110.0, 0.0, 0.0));
	LeftPillar->SetRelativeScale3D(FVector(0.2, 0.3, 3.6));
	RightPillar->SetRelativeScale3D(FVector(0.2, 0.3, 3.6));
	PortalMesh->SetRelativeLocation(FVector(0.0, 0.0, 170.0));
	PortalMesh->SetRelativeScale3D(FVector(2.4, 0.3, 0.2));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		PortalMesh->SetStaticMesh(Cube.Object);
		LeftPillar->SetStaticMesh(Cube.Object);
		RightPillar->SetStaticMesh(Cube.Object);
	}
	RouteLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Route Label"));
	RouteLabel->SetupAttachment(SceneRoot);
	RouteLabel->SetRelativeLocation(FVector(0.0, 0.0, 225.0));
	RouteLabel->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	RouteLabel->SetHorizontalAlignment(EHTA_Center);
	RouteLabel->SetWorldSize(32.0f);
	RouteLabel->SetText(FText::FromString(TEXT("FIVE VILLAGES\nROUTE TEST")));
}
