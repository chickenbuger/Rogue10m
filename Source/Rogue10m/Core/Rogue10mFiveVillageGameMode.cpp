// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mFiveVillageGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Rogue10m.h"
#include "Rogue10mFiveVillageProgressComponent.h"
#include "Rogue10mFiveVillageRouteDataAsset.h"

ARogue10mFiveVillageGameMode::ARogue10mFiveVillageGameMode()
{
	// StartRun=1 must come through the normal hub entry baseline/supply contract.
	bStartRunOnStartPlay = false;
	bUsePrototypeRunDuration = false;
}

bool ARogue10mFiveVillageGameMode::IsCandidateWorld() const
{
	const UWorld* World = GetWorld();
	return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
		== TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110");
}

void ARogue10mFiveVillageGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!IsCandidateWorld() || !NewPlayer)
	{
		return;
	}
	APlayerState* State = NewPlayer->GetPlayerState<APlayerState>();
	if (!State || State->FindComponentByClass<URogue10mFiveVillageProgressComponent>())
	{
		return;
	}
	URogue10mFiveVillageRouteDataAsset* Definition = nullptr;
	if (!RouteDefinition.IsNull())
	{
		Definition = RouteDefinition.LoadSynchronous();
		if (!Definition)
		{
			UE_LOG(LogRogue10m, Error, TEXT("Configured five-village route asset could not be loaded."));
			return;
		}
	}
	URogue10mFiveVillageProgressComponent* Progress = NewObject<URogue10mFiveVillageProgressComponent>(
		State, TEXT("FiveVillageProgress"), RF_Transient);
	if (!Progress)
	{
		UE_LOG(LogRogue10m, Error, TEXT("Could not create five-village PlayerState progression."));
		return;
	}
	if (!Progress->Configure(Definition))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Five-village route definition is invalid; progression was not registered."));
		return;
	}
	State->AddInstanceComponent(Progress);
	Progress->RegisterComponent();
}

URogue10mFiveVillageProgressComponent* ARogue10mFiveVillageGameMode::GetRouteProgress(
	APlayerController* Controller) const
{
	if (!IsCandidateWorld() || !Controller || Controller->GetWorld() != GetWorld())
	{
		return nullptr;
	}
	const APlayerState* State = Controller->GetPlayerState<APlayerState>();
	return State ? State->FindComponentByClass<URogue10mFiveVillageProgressComponent>() : nullptr;
}
