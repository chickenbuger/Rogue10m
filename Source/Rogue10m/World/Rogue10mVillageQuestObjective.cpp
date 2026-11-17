#include "Rogue10mVillageQuestObjective.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Misc/ScopeExit.h"
#include "Rogue10mCharacter.h"
#include "Rogue10m.h"
#include "Rogue10mGameState.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestDirector.h"
#include "Rogue10mVillageQuestModel.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

FName Rogue10mVillageQuestEncounterReceipt(FName QuestId, int32 ObjectiveIndex)
{
	return FName(*FString::Printf(TEXT("Rogue10m.VillageQuest.Encounter.%s.%d"), *QuestId.ToString(), ObjectiveIndex));
}

FName Rogue10mVillageQuestEncounterDeathReceipt(FName QuestId, int32 ObjectiveIndex)
{
	return FName(*FString::Printf(TEXT("Rogue10m.VillageQuest.Death.%s.%d"), *QuestId.ToString(), ObjectiveIndex));
}

ARogue10mVillageQuestObjective::ARogue10mVillageQuestObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	Area = CreateDefaultSubobject<USphereComponent>(TEXT("ObjectiveArea")); SetRootComponent(Area);
	Area->SetSphereRadius(Presentation.RadiusCm); Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore); Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true); Area->SetCanEverAffectNavigation(false);
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(Area);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision); Marker->SetGenerateOverlapEvents(false);
	Marker->SetCanEverAffectNavigation(false); Marker->SetRelativeScale3D(Presentation.MarkerScale);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Mesh.Succeeded()) { DefaultMarkerMesh = Mesh.Object; Marker->SetStaticMesh(DefaultMarkerMesh); }
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ObjectiveLabel")); Label->SetupAttachment(Area);
	Label->SetRelativeLocation(Presentation.LabelOffset); Label->SetRelativeRotation(Presentation.LabelRotation);
	Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center); Label->SetWorldSize(Presentation.LabelWorldSize);
	Label->SetTextRenderColor(Presentation.LabelColor);
}

void ARogue10mVillageQuestObjective::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform); ApplyPresentation();
}

void ARogue10mVillageQuestObjective::ApplyPresentation()
{
	if (!Presentation.IsValid())
	{
		Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogRogue10m, Warning, TEXT("Quest objective %s has invalid presentation settings; collection disabled."), *GetName());
		return;
	}
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Area->SetSphereRadius(Presentation.RadiusCm);
	Marker->SetStaticMesh(Presentation.MarkerMesh ? Presentation.MarkerMesh.Get() : DefaultMarkerMesh.Get());
	Marker->SetRelativeScale3D(Presentation.MarkerScale); Marker->SetVisibility(Presentation.bShowMarker);
	Label->SetRelativeLocation(Presentation.LabelOffset); Label->SetRelativeRotation(Presentation.LabelRotation);
	Label->SetWorldSize(Presentation.LabelWorldSize); Label->SetTextRenderColor(Presentation.LabelColor);
	Label->SetText(ObjectiveLabel); Label->SetVisibility(Presentation.bShowLabel);
}

void ARogue10mVillageQuestObjective::BeginPlay()
{
	Super::BeginPlay(); ApplyPresentation();
	Area->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
	RunState = GetWorld()->GetGameState<ARogue10mGameState>();
	if (RunState.IsValid()) { RunState->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged); }
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::CollectOverlappingPlayers);
}

void ARogue10mVillageQuestObjective::EndPlay(const EEndPlayReason::Type Reason)
{
	if (RunState.IsValid()) { RunState->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRunPhaseChanged); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}

void ARogue10mVillageQuestObjective::HandleRunPhaseChanged(ERogue10mRunPhase Phase)
{
	if (Phase == ERogue10mRunPhase::Running)
	{
		// After every listener has reset its run state, including an already attached quest component.
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::CollectOverlappingPlayers);
	}
}

void ARogue10mVillageQuestObjective::CollectOverlappingPlayers()
{
	TArray<AActor*> Players; Area->GetOverlappingActors(Players, ARogue10mCharacter::StaticClass());
	for (AActor* Player : Players) { Collect(Player); }
}

void ARogue10mVillageQuestObjective::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	Collect(Other);
}

void ARogue10mVillageQuestObjective::Collect(AActor* Actor)
{
	auto* Character = Cast<ARogue10mCharacter>(Actor);
	const int32 QuestIndex = QuestDefinition ? QuestDefinition->FindQuestIndex(QuestId) : INDEX_NONE;
	if (!HasAuthority() || !Presentation.IsValid() || !Character || !QuestDefinition || !QuestDefinition->Quests.IsValidIndex(QuestIndex)
		|| ObjectiveIndex < 0 || ObjectiveIndex > 1
		|| QuestDefinition->Quests[QuestIndex].CombatObjectiveIndex == ObjectiveIndex) { return; }
	if (auto* Quests = URogue10mVillageQuestComponent::FindOrCreate(Character, QuestDefinition))
	{
		if (Quests->ObserveObjective(QuestId, ObjectiveIndex, Character))
		{
			if (auto* Controller = Cast<ARogue10mPlayerController>(Character->GetController()))
			{
				Controller->AddCombatLogMessage(FString::Printf(TEXT("의뢰 목표 확인 · %s (%d/2)"),
					*ObjectiveLabel.ToString(), Quests->GetObjectiveProgress(QuestId)), FLinearColor(.8f, .9f, .4f));
			}
		}
	}
}

ARogue10mVillageQuestMonster::ARogue10mVillageQuestMonster()
{
}

bool ARogue10mVillageQuestMonster::ConfigureQuestMonsterData(URogue10mMonsterDataAsset* Data)
{
	if (HasActorBegunPlay() || !Data || Data->MonsterRank != ERogue10mMonsterRank::Normal) { return false; }
	MonsterData = Data; return true;
}

void ARogue10mVillageQuestMonster::BeginPlay()
{
	if (!QuestDefinition) { QuestDefinition = GetMutableDefault<URogue10mVillageQuestDataAsset>(); }
	EncounterDefinition = QuestDefinition; EncounterQuestId = QuestId; EncounterObjectiveIndex = ObjectiveIndex;
	EncounterRun = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	EncounterRunId = Rogue10mVillageQuestRunId(GetWorld()); bEncounterEnded = false;
	if (EncounterRun.IsValid()) { EncounterRun->OnRunPhaseChanged.AddUniqueDynamic(this, &ThisClass::InvalidateQuestEncounter); }
	Super::BeginPlay();
}

void ARogue10mVillageQuestMonster::InvalidateQuestEncounter(ERogue10mRunPhase)
{
	// Old encounters never acquire a new run's identity, even if a callback keeps them alive.
	bEncounterEnded = true;
}

void ARogue10mVillageQuestMonster::EndPlay(const EEndPlayReason::Type Reason)
{
	bEncounterEnded = true; CurrentQuestDamage = nullptr;
	if (EncounterRun.IsValid()) { EncounterRun->OnRunPhaseChanged.RemoveDynamic(this, &ThisClass::InvalidateQuestEncounter); }
	Super::EndPlay(Reason);
}

bool ARogue10mVillageQuestMonster::ShouldGrantDeathRewards() const
{
	const UWorld* World = GetWorld();
	return !bRecoveryEncounter && bDeathCustodyRecorded && World && !World->bIsTearingDown
		&& EncounterRun.IsValid() && World->GetGameState() == EncounterRun.Get() && EncounterRun->IsRunActive()
		&& EncounterRunId == Rogue10mVillageQuestRunId(World);
}

float ARogue10mVillageQuestMonster::TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
	FQuestDamageContext Context;
	Context.Run = EncounterRun; Context.RunId = EncounterRunId;
	Context.QuestId = EncounterQuestId; Context.ObjectiveIndex = EncounterObjectiveIndex; Context.Definition = EncounterDefinition;
	Context.Director = Cast<ARogue10mVillageQuestDirector>(GetOwner());
	if (Context.Director.IsValid()) { Context.DirectorGeneration = Context.Director->EncounterGeneration; }
	if (HasAuthority() && !IsDead() && !bEncounterEnded && FMath::IsFinite(Amount) && Amount > 0
		&& Rogue10mVillageQuestSameRun(GetWorld(), Context.Run.Get(), Context.RunId))
	{
		Context.Character = EventInstigator ? Cast<ARogue10mCharacter>(EventInstigator->GetPawn()) : nullptr;
		// Attachment may notify observers, so it is deliberately outside the pre-loot death hook.
		Context.Component = URogue10mVillageQuestComponent::FindOrCreate(Context.Character.Get(), Context.Definition.Get());
		if (Context.Component.IsValid()) { Context.CreditEpoch = Context.Component->CombatCredits.GetEpoch(); }
	}
	FQuestDamageContext* Previous = CurrentQuestDamage; CurrentQuestDamage = &Context;
	ON_SCOPE_EXIT { CurrentQuestDamage = bEncounterEnded ? nullptr : Previous; };
	const float Applied = Super::TakeDamage(Amount, Event, EventInstigator, Causer);
	// Locals, not the corpse, own these references. Base Die may already have destroyed this actor.
	if (Context.bObservedDeath)
	{
		if (Context.Component.IsValid()) { Context.Component->FlushCombatDeaths(); }
		if (Context.Director.IsValid())
		{
			Context.Director->RequestCombatRecovery(Context.QuestId, Context.ObjectiveIndex, Context.DirectorGeneration);
		}
	}
	return Applied;
}

void ARogue10mVillageQuestMonster::OnDeathStarted()
{
	Super::OnDeathStarted();
	FQuestDamageContext* Context = CurrentQuestDamage;
	const auto* Definition = EncounterDefinition.Get();
	const int32 Slot = Definition ? Definition->FindQuestIndex(EncounterQuestId) : INDEX_NONE;
	if (!HasAuthority() || !IsDead() || IsCombatTestInstance() || bEncounterEnded || !Context
		|| EncounterObjectiveIndex < 0 || EncounterObjectiveIndex >= Rogue10mVillageQuest::ObjectiveCount
		|| !GetWorld() || GetWorld()->bIsTearingDown || !EncounterRun.IsValid() || GetWorld()->GetGameState() != EncounterRun.Get()
		|| !EncounterRun->IsRunActive() || EncounterRunId != Rogue10mVillageQuestRunId(GetWorld())
		|| Context->Run != EncounterRun || Context->RunId != EncounterRunId
		|| Context->QuestId != EncounterQuestId || Context->ObjectiveIndex != EncounterObjectiveIndex || Context->Definition != EncounterDefinition
		|| !Definition || !Definition->Quests.IsValidIndex(Slot)
		|| Definition->Quests[Slot].CombatObjectiveIndex != EncounterObjectiveIndex) { return; }
	// No callbacks here. Death/first-roll custody is sealed before base loot/XP can re-enter gameplay.
	Context->bObservedDeath = true;
	const FName DeathKey = Rogue10mVillageQuestEncounterDeathReceipt(Context->QuestId, Context->ObjectiveIndex);
	// Also fence a duplicate actor that was created before the first death became visible.
	bRecoveryEncounter |= EncounterRun->Tags.Contains(DeathKey)
		|| EncounterRun->Tags.Contains(Rogue10mVillageQuestEncounterReceipt(Context->QuestId, Context->ObjectiveIndex));
	EncounterRun->Tags.AddUnique(DeathKey);
	bDeathCustodyRecorded = true;
	if (Context->Component.IsValid())
	{
		Context->Component->EnqueueCombatDeath(Context->QuestId, Context->ObjectiveIndex, Context->Character.Get(), Context->CreditEpoch, Context->RunId);
	}
}
