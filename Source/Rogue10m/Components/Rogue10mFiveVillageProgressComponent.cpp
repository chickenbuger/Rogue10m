#include "Rogue10mFiveVillageProgressComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Rogue10m.h"
#include "Rogue10mFiveVillageRouteDataAsset.h"
#include "Rogue10mGameState.h"

namespace Rogue10mFiveVillageProgress
{
	bool IsCandidateWorld(const UWorld& World)
	{
		return World.IsGameWorld() && UWorld::RemovePIEPrefix(World.GetOutermost()->GetName())
			== TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110");
	}
}

URogue10mFiveVillageProgressComponent::URogue10mFiveVillageProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URogue10mFiveVillageProgressComponent::InitializeDefinition(URogue10mFiveVillageRouteDataAsset* Definition)
{
	ActiveRoute = Definition ? Definition : NewObject<URogue10mFiveVillageRouteDataAsset>(this);
	FString Reason;
	bDefinitionValid = ActiveRoute && ActiveRoute->ValidateDefinition(Reason);
	if (!bDefinitionValid)
	{
		UE_LOG(LogRogue10m, Error, TEXT("Five-village observations disabled: %s"), *Reason);
	}
	Progress.Reset();
	return bDefinitionValid;
}

bool URogue10mFiveVillageProgressComponent::Configure(URogue10mFiveVillageRouteDataAsset* Definition)
{
	if (HasBegunPlay())
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Five-village route cannot be reconfigured after BeginPlay."));
		return false;
	}
	return InitializeDefinition(Definition);
}

void URogue10mFiveVillageProgressComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	APlayerState* State = Cast<APlayerState>(GetOwner());
	if (!World || !Rogue10mFiveVillageProgress::IsCandidateWorld(*World) || !State || !State->HasAuthority())
	{
		return;
	}
	if (!ActiveRoute && !InitializeDefinition(nullptr))
	{
		return;
	}
	if (!bDefinitionValid || !FMath::IsFinite(ObservationIntervalSeconds) || ObservationIntervalSeconds < 0.05f)
	{
		return;
	}
	OwnerState = State;
	BindRunState();
	World->GetTimerManager().SetTimer(ObservationTimer, this,
		&URogue10mFiveVillageProgressComponent::ObserveProgress, ObservationIntervalSeconds, true);
	ObserveProgress();
}

void URogue10mFiveVillageProgressComponent::BindRunState()
{
	UWorld* World = GetWorld();
	ARogue10mGameState* Current = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
	if (Current == RunState.Get())
	{
		return;
	}
	if (RunState.IsValid())
	{
		RunState->OnRunPhaseChanged.RemoveDynamic(this, &URogue10mFiveVillageProgressComponent::HandleRunPhaseChanged);
	}
	RunState = Current;
	if (Current)
	{
		Current->OnRunPhaseChanged.AddUniqueDynamic(this, &URogue10mFiveVillageProgressComponent::HandleRunPhaseChanged);
		HandleRunPhaseChanged(Current->GetRunPhase());
	}
}

void URogue10mFiveVillageProgressComponent::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (Phase == ERogue10mRunPhase::Running)
	{
		Progress.Reset();
		bObservedRunning = true;
		OnRouteProgressChanged.Broadcast();
	}
	else
	{
		bObservedRunning = false;
		if (Progress.ObserveVillage(-1))
		{
			OnRouteProgressChanged.Broadcast();
		}
	}
}

void URogue10mFiveVillageProgressComponent::ObserveProgress()
{
	if (!OwnerState.IsValid() || !bDefinitionValid || !ActiveRoute)
	{
		return;
	}
	if (!RunState.IsValid())
	{
		BindRunState();
	}
	if (!RunState.IsValid() || !RunState->IsRunActive())
	{
		if (Progress.ObserveVillage(-1)) { OnRouteProgressChanged.Broadcast(); }
		return;
	}
	if (!bObservedRunning)
	{
		HandleRunPhaseChanged(ERogue10mRunPhase::Running);
	}
	ObservedPawn = OwnerState->GetPawn();
	if (!ObservedPawn.IsValid())
	{
		return; // A brief unpossessed interval is not a physical leave/revisit.
	}
	const FVector Location = ObservedPawn->GetActorLocation();
	if (Location.ContainsNaN()) { return; }
	int32 VillageIndex = INDEX_NONE;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < ActiveRoute->Villages.Num(); ++Index)
	{
		const FRogue10mFiveVillageLocation& Village = ActiveRoute->Villages[Index];
		const double DistanceSquared = FVector::DistSquared2D(Location, Village.VillageCenter);
		if (FMath::Abs(Location.Z - Village.VillageCenter.Z) <= Village.HeightToleranceCm
			&& DistanceSquared <= FMath::Square(static_cast<double>(Village.RadiusCm))
			&& DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			VillageIndex = Index;
		}
	}
	bool bChanged = Progress.ObserveVillage(VillageIndex);
	if (FMath::Abs(Location.Z - ActiveRoute->BossApproachLocation.Z) <= ActiveRoute->BossHeightToleranceCm
		&& FVector::DistSquared2D(Location, ActiveRoute->BossApproachLocation)
			<= FMath::Square(static_cast<double>(ActiveRoute->BossApproachRadiusCm)))
	{
		const double ActiveSeconds = static_cast<double>(RunState->GetRunDurationSeconds())
			- static_cast<double>(RunState->GetRemainingRunSeconds());
		bChanged = Progress.ObserveBossApproach(ActiveSeconds) || bChanged;
	}
	if (bChanged) { OnRouteProgressChanged.Broadcast(); }
}

TArray<FName> URogue10mFiveVillageProgressComponent::GetVisitedVillageIds() const
{
	TArray<FName> Result;
	if (bDefinitionValid && ActiveRoute)
	{
		for (int32 Index = 0; Index < ActiveRoute->Villages.Num(); ++Index)
		{
			if (Progress.GetVisitCount(Index) > 0) { Result.Add(ActiveRoute->Villages[Index].VillageId); }
		}
	}
	return Result;
}

int32 URogue10mFiveVillageProgressComponent::GetVillageVisitCount(FName VillageId) const
{
	if (bDefinitionValid && ActiveRoute)
	{
		for (int32 Index = 0; Index < ActiveRoute->Villages.Num(); ++Index)
		{
			if (ActiveRoute->Villages[Index].VillageId == VillageId) { return Progress.GetVisitCount(Index); }
		}
	}
	return 0;
}

FName URogue10mFiveVillageProgressComponent::GetCurrentVillageId() const
{
	const int32 Index = Progress.GetCurrentVillageIndex();
	return bDefinitionValid && ActiveRoute && ActiveRoute->Villages.IsValidIndex(Index)
		? ActiveRoute->Villages[Index].VillageId : NAME_None;
}

int32 URogue10mFiveVillageProgressComponent::GetTotalRevisitCount() const
{
	return static_cast<int32>(FMath::Min<std::uint32_t>(Progress.GetRevisitCount(), MAX_int32));
}

bool URogue10mFiveVillageProgressComponent::HasEnteredBossApproach() const
{
	return Progress.HasEnteredBossApproach();
}

float URogue10mFiveVillageProgressComponent::GetFirstBossApproachActiveSeconds() const
{
	return static_cast<float>(Progress.GetFirstBossApproachActiveSeconds());
}

void URogue10mFiveVillageProgressComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ObservationTimer); }
	if (RunState.IsValid())
	{
		RunState->OnRunPhaseChanged.RemoveDynamic(this, &URogue10mFiveVillageProgressComponent::HandleRunPhaseChanged);
	}
	ObservedPawn.Reset();
	OwnerState.Reset();
	RunState.Reset();
	Super::EndPlay(EndPlayReason);
}
