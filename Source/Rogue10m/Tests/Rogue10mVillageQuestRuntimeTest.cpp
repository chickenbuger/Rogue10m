// Isolated rendered QA. Normal hub portal entry, then explicitly reported candidate-only positioning.
#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mFiveVillageGameMode.h"
#include "Rogue10mFiveVillageRunPortal.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mTutorialWidget.h"
#include "Rogue10mVillageQuestComponent.h"
#include "Rogue10mVillageQuestDataAsset.h"
#include "Rogue10mVillageQuestDirector.h"
#include "Rogue10mVillageQuestNPC.h"
#include "Rogue10mVillageQuestObjective.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace Rogue10mVillageQuestRuntimeQA
{
static const TCHAR* Hub = TEXT("/Game/Rogue10m/Maps/StartHub");
static const TCHAR* Field = TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110");

static bool SafeLabel(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 20) { return false; }
	for (TCHAR C : Value)
	{
		if (!((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z'))
			|| (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_'))) { return false; }
	}
	return true;
}

static bool UniqueOption(const TCHAR* Name, FString& Value)
{
	TArray<FString> Tokens, Options; FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
	int32 Count = 0; const FString Prefix = FString(Name) + TEXT("=");
	for (const FString& Option : Options)
	{
		if (Option.StartsWith(Prefix, ESearchCase::IgnoreCase)) { ++Count; Value = Option.Mid(Prefix.Len()); }
	}
	return Count == 1;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	bool Finished() const { return bFinished; }
	void Start(UWorld& Initial)
	{
		FString ProfileSlot, Prefix, KeySlot;
		const bool bOptions = UniqueOption(TEXT("RogueVillageQuestLabel"), Label)
			&& UniqueOption(TEXT("RogueProfileSaveSlot"), ProfileSlot)
			&& UniqueOption(TEXT("RogueProgressionSavePrefix"), Prefix)
			&& UniqueOption(TEXT("RogueSkillKeySaveSlot"), KeySlot);
		const FString ExpectedOutput = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/village-quests-runtime-20261111") / Label / TEXT("runtime");
		TArray<FString> Existing;
		IFileManager::Get().FindFiles(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Prefix + TEXT("_*.sav"))), true, false);
		auto* Profiles = Initial.GetGameInstance() ? Initial.GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		if (!bOptions || !SafeLabel(Label)
			|| ProfileSlot != TEXT("Rogue10m_Town_QA_VillageQuest_") + Label
			|| Prefix != TEXT("Rogue10m_Progression_QA_VillageQuest_") + Label
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_VillageQuest_") + Label
			|| UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0) || UGameplayStatics::DoesSaveGameExist(KeySlot, 0)
			|| !Existing.IsEmpty() || IFileManager::Get().DirectoryExists(*ExpectedOutput)
			|| Initial.GetNetMode() != NM_Standalone || !Initial.IsGameWorld()
			|| UWorld::RemovePIEPrefix(Initial.GetOutermost()->GetName()) != Hub
			|| !Profiles || Profiles->GetProfileCount() != 0 || Profiles->GetSelectedProfile()
			|| FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) || !FSlateApplication::IsInitialized())
		{
			bFinished = true;
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=VILLAGE_QUEST_RUNTIME_FAILED fresh_exact_QA_slots_rendered_StartHub_required"));
			FPlatformMisc::RequestExitWithStatus(false, 1); return;
		}
		Output = ExpectedOutput;
		if (!IFileManager::Get().MakeDirectory(*Output, true))
		{
			bFinished = true; UE_LOG(LogRogue10m, Error, TEXT("RESULT=VILLAGE_QUEST_RUNTIME_FAILED output_unavailable"));
			FPlatformMisc::RequestExitWithStatus(false, 1); return;
		}
		World = &Initial; Instance = Initial.GetGameInstance(); StartedAt = FPlatformTime::Seconds(); Deadline = StartedAt + 220.0;
		Events = TEXT("wall_seconds,map,event,detail\n");
		for (const TCHAR* Name : {TEXT("normal_portal_entry"), TEXT("actor_layout"), TEXT("f_and_slate"), TEXT("interrupted_revisit"),
			TEXT("npc_recreated"), TEXT("director_recreated_without_encounter_respawn"), TEXT("preacceptance"), TEXT("attributed_kill"), TEXT("all_five_claimed"),
			TEXT("duplicate_claim_blocked"), TEXT("input_restored"), TEXT("third_person")}) { Milestones.Add(Name, false); }
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			return Self->Tick();
		}), 0.02f);
		Later(3.0, [this] { Prepare(); });
	}

private:
	static FString Quote(FString Value) { Value.ReplaceInline(TEXT("\""), TEXT("\"\"")); return TEXT("\"") + Value + TEXT("\""); }
	FString Map() const { return World.IsValid() ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString(); }
	void Event(const TCHAR* Name, const FString& Detail = FString())
	{
		Events += FString::Printf(TEXT("%.6f,%s,%s,%s\n"), FPlatformTime::Seconds() - StartedAt, *Quote(Map()), Name, *Quote(Detail));
	}
	bool Check(bool bGood, const TCHAR* Name)
	{
		++Checks; auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Name); Row->SetBoolField(TEXT("passed"), bGood);
		Assertions.Add(MakeShared<FJsonValueObject>(Row));
		if (!bGood) { ++Failures; Event(TEXT("assertion_failed"), Name); UE_LOG(LogRogue10m, Error, TEXT("VILLAGE_QUEST_RUNTIME FAIL %s"), Name); }
		return bGood;
	}
	void Later(double Delay, TFunction<void()> Action) { NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Action); }
	void RefreshWorld()
	{
		UWorld* Found = nullptr;
		if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Context.WorldType == EWorldType::Game && Candidate->GetGameInstance() == Instance.Get())
			{
				if (Found) { Check(false, TEXT("single_owned_game_world")); return; }
				Found = Candidate;
			}
		}
		if (Found != World.Get()) { World = Found; Event(TEXT("world_changed")); }
		PC = Found ? Cast<ARogue10mPlayerController>(Found->GetFirstPlayerController()) : nullptr;
		Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		RunState = Found ? Found->GetGameState<ARogue10mGameState>() : nullptr;
		Inventory = Player.IsValid() ? Player->GetInventoryComponent() : nullptr;
		Quests = State.IsValid() ? State->FindComponentByClass<URogue10mVillageQuestComponent>() : nullptr;
		Saves = Instance.IsValid() ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	}
	bool Ready() const
	{
		return World.IsValid() && World->HasBegunPlay() && PC.IsValid() && Player.IsValid() && State.IsValid()
			&& RunState.IsValid() && Saves.IsValid() && Inventory.IsValid();
	}
	bool LiveCandidate()
	{
		return Check(Ready() && Map() == Field && RunState->IsRunActive() && !Player->IsDead()
			&& Player->IsThirdPersonGameplayActive() && Saves->IsAttachedTo(*Player)
			&& Saves->GetAttachedProfileId() == ProfileId && Saves->GetAttachedRunId() == EntryRunId,
			TEXT("same_live_3P_candidate_profile_and_run"));
	}
	bool Tick()
	{
		if (bFinished) { return false; }
		RefreshWorld();
		if (FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("bounded_fixture_watchdog")); Finish(); return false; }
		if (Failures) { Finish(); return false; }
		if (Milestones.FindRef(TEXT("normal_portal_entry"))
			&& (!Ready() || Map() != Field || !RunState->IsRunActive() || Player->IsDead()
				|| !Player->IsThirdPersonGameplayActive() || !Saves->IsAttachedTo(*Player)
				|| Saves->GetAttachedProfileId() != ProfileId || Saves->GetAttachedRunId() != EntryRunId
				|| (Milestones.FindRef(TEXT("f_and_slate")) && !Quests.IsValid())))
		{
			Check(false, TEXT("live_candidate_context_survives_before_delayed_callback")); Finish(); return false;
		}
		if (bWalkingHub) { WalkHub(); }
		else if (Next && FPlatformTime::Seconds() >= NextAt) { auto Work = MoveTemp(Next); Work(); }
		return !bFinished;
	}
	void Prepare()
	{
		if (!World.IsValid() || !PC.IsValid() || !Saves.IsValid()) { Later(0.1, [this] { Prepare(); }); return; }
		auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		FRogue10mCharacterProfile Profile;
		if (!Check(Profiles && Profiles->CreateProfile(TEXT("VillageQuestQA"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("normal_fresh_profile_created_selected"))) { return; }
		ProfileId = Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Later(0.5, [this] { StartHubWalk(); });
	}
	void StartHubWalk()
	{
		if (!Ready() || !Player->GetCharacterMovement()->IsMovingOnGround()) { Later(0.1, [this] { StartHubWalk(); }); return; }
		if (!Check(Map() == Hub && RunState->GetRunPhase() == ERogue10mRunPhase::WaitingToStart
			&& Saves->IsAttachedTo(*Player) && Saves->IsSaveReady() && Saves->GetAttachedProfileId() == ProfileId
			&& Player->IsThirdPersonGameplayActive(), TEXT("normal_waiting_hub_profile_and_third_person"))) { return; }
		TArray<ARogue10mFiveVillageRunPortal*> Gates; int32 OriginalCount = 0;
		for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It)
		{
			if (auto* Gate = Cast<ARogue10mFiveVillageRunPortal>(*It)) { Gates.Add(Gate); }
			else if (It->Tags.Contains(FName(TEXT("Rogue10m.StartVillage")))) { ++OriginalCount; }
		}
		if (!Check(Gates.Num() == 1 && OriginalCount == 1, TEXT("existing_candidate_and_original_portals_preserved"))) { return; }
		const FVector Center = Gates[0]->GetActorLocation(), Axis = Gates[0]->GetActorRightVector();
		EntryRunId = Saves->GetAttachedRunId();
		if (!Check(EntryRunId.IsValid(), TEXT("normal_hub_run_identity"))) { return; }
		Waypoints = {FVector(650, -1100, 0), FVector(650, 950, 0), FVector(0, 1800, 0), Center - Axis * 300.0, Center + Axis * 300.0};
		Waypoint = 0; bWalkingHub = true; ProgressAt = FPlatformTime::Seconds(); LastDistance = TNumericLimits<double>::Max();
		PreviousHubPosition = Player->GetActorLocation(); Event(TEXT("native_hub_portal_walk_started"));
	}
	void WalkHub()
	{
		if (Map() == Field) { bWalkingHub = false; Later(0.5, [this] { EnterField(); }); return; }
		if (!Ready()) { return; }
		if (!Check(Map() == Hub && !Player->IsDead() && !PC->IsMoveInputIgnored(), TEXT("native_hub_walking_context"))) { return; }
		const FVector Position = Player->GetActorLocation();
		if (!Check(FVector::Dist2D(PreviousHubPosition, Position) < 500.0, TEXT("hub_entry_has_no_fixture_position_jump"))) { return; }
		HubWalkedDistance += FVector::Dist2D(PreviousHubPosition, Position); PreviousHubPosition = Position;
		if (Waypoint >= Waypoints.Num()) { bWalkingHub = false; Later(0.1, [this] { EnterField(); }); return; }
		auto* Movement = Player->GetCharacterMovement(); const FVector Goal = Waypoints[Waypoint];
		const double Distance = FVector::Dist2D(Position, Goal);
		if (Distance <= 65.0 && Movement->IsMovingOnGround())
		{
			++Waypoint; LastDistance = TNumericLimits<double>::Max(); ProgressAt = FPlatformTime::Seconds(); return;
		}
		if (Distance < LastDistance - 10.0) { LastDistance = Distance; ProgressAt = FPlatformTime::Seconds(); }
		if (FPlatformTime::Seconds() - ProgressAt > 12.0) { Check(false, TEXT("normal_hub_walk_not_stuck")); return; }
		FVector Direction = Goal - Position; Direction.Z = 0; Direction.Normalize();
		PC->SetControlRotation(FRotator(-12, Direction.Rotation().Yaw, 0)); Player->AddMovementInput(Direction, 1.0f, false);
	}
	void EnterField()
	{
		if (!Ready() || Map() != Field || !Player->GetCharacterMovement()->IsMovingOnGround() || !Saves->IsSaveReady())
		{
			Later(0.1, [this] { EnterField(); }); return;
		}
		if (!LiveCandidate() || !Check(Cast<ARogue10mFiveVillageGameMode>(World->GetAuthGameMode()) && HubWalkedDistance > 500.0
			&& FMath::IsNearlyEqual(RunState->GetRunDurationSeconds(), 1200.0f), TEXT("normal_native_portal_entry_with_production_run"))) { return; }
		Milestones[TEXT("normal_portal_entry")] = true; FieldInitialRemaining = RunState->GetRemainingRunSeconds();
		int32 Directors = 0, MarkerCount = 0, MonsterCount = 0;
		for (TActorIterator<ARogue10mVillageQuestDirector> It(World.Get()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed()) { ++Directors; ActiveDirector = *It; }
		}
		for (TActorIterator<ARogue10mVillageQuestNPC> It(World.Get()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed()) { Check(!Npcs.Contains(It->QuestId), TEXT("unique_quest_NPC")); Npcs.Add(It->QuestId, *It); }
		}
		for (TActorIterator<ARogue10mVillageQuestObjective> It(World.Get()); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++MarkerCount; } }
		for (TActorIterator<ARogue10mVillageQuestMonster> It(World.Get()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed()) { ++MonsterCount; QuarryMonster = *It; }
		}
		if (!Check(Directors == 1 && Npcs.Num() == 5 && MarkerCount == 9 && MonsterCount == 1,
			TEXT("installed_director_spawns_five_NPC_nine_markers_one_combat_target"))) { return; }
		Milestones[TEXT("actor_layout")] = true; Milestones[TEXT("third_person")] = true;
		NearNpc(TEXT("Q_V1_supply"), [this] { FPress([this] { V1Offer(); }); });
	}
	ARogue10mVillageQuestNPC* GetNpc(FName Id) const
	{
		const auto* Found = Npcs.Find(Id); return Found && Found->IsValid() ? Found->Get() : nullptr;
	}
	bool PlaceCandidate(const FVector& Position)
	{
		if (!LiveCandidate() || Position.ContainsNaN()) { return false; }
		Player->GetCharacterMovement()->StopMovementImmediately();
		const bool bPlaced = Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
		if (bPlaced) { Player->GetCapsuleComponent()->UpdateOverlaps(); ++CandidateTeleports; Event(TEXT("candidate_fixture_teleport"), Position.ToString()); }
		return Check(bPlaced, TEXT("explicit_candidate_only_fixture_positioning"));
	}
	void NearNpc(FName Id, TFunction<void()> After)
	{
		CurrentNpc = GetNpc(Id);
		if (!Check(CurrentNpc.IsValid(), TEXT("requested_live_quest_NPC"))) { return; }
		const FVector Near = CurrentNpc->GetActorLocation() + FVector(180, 0, 95);
		if (!PlaceCandidate(Near)) { return; }
		PC->SetControlRotation((CurrentNpc->GetActorLocation() - Near).Rotation() + FRotator(-12, 0, 0));
		Later(0.6, MoveTemp(After));
	}
	void VisitObjective(FName Id, int32 Goal, TFunction<void()> After)
	{
		ARogue10mVillageQuestObjective* Marker = nullptr;
		for (TActorIterator<ARogue10mVillageQuestObjective> It(World.Get()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->QuestId == Id && It->ObjectiveIndex == Goal) { Marker = *It; break; }
		}
		if (!Check(Marker != nullptr, TEXT("authored_exploration_marker_exists")) || !PlaceCandidate(Marker->GetActorLocation())) { return; }
		const auto* Area = Marker->FindComponentByClass<USphereComponent>();
		if (!Check(Area && Area->IsOverlappingActor(Player.Get()), TEXT("real_capsule_marker_overlap_not_direct_fact_injection"))) { return; }
		Later(0.3, [this, Id, Goal, After = MoveTemp(After)]() mutable
		{
			if (!Check(Quests.IsValid() && Quests->HasObservedObjective(Id, Goal), TEXT("actual_marker_overlap_records_individual_fact"))) { return; }
			After();
		});
	}
	void Key(FKey KeyValue, bool bRepeat = false)
	{
		if (!Check(FSlateApplication::IsInitialized(), TEXT("Slate_key_route_available"))) { return; }
		auto& App = FSlateApplication::Get();
		App.ProcessKeyDownEvent(FKeyEvent(KeyValue, FModifierKeysState(), 0, bRepeat, 0, 0));
		App.ProcessKeyUpEvent(FKeyEvent(KeyValue, FModifierKeysState(), 0, false, 0, 0));
	}
	void FPress(TFunction<void()> After)
	{
		if (!Check(PC.IsValid(), TEXT("controller_F_input_target_live"))) { return; }
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Pressed, 1.0f));
		Later(0.1, [this, After = MoveTemp(After)]() mutable
		{
			if (!Check(PC.IsValid(), TEXT("controller_live_for_F_release"))) { return; }
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Released, 0.0f)); Later(0.35, MoveTemp(After));
		});
	}
	bool Click(const TCHAR* Name)
	{
		auto* Widget = CurrentNpc.IsValid() ? CurrentNpc->GetTutorialWidget() : nullptr;
		auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr; FWidgetPath Path;
		if (!Check(Button && Button->GetIsEnabled() && FSlateApplication::Get().GeneratePathToWidgetUnchecked(Button->TakeWidget(), Path),
			TEXT("real_quest_button_has_Slate_path"))) { return false; }
		const FVector2D Point = Button->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(.5, .5));
		auto& App = FSlateApplication::Get();
		const FWidgetPath Hit = App.LocateWindowUnderMouse(Point, TArray<TSharedRef<SWindow>>{Path.GetWindow()}, false, 0);
		if (!Check(Hit.IsValid() && Hit.ContainsWidget(&Button->TakeWidget().Get()), TEXT("actual_quest_button_hit_test"))) { return false; }
		App.RoutePointerMoveEvent(Hit, FPointerEvent(0, Point, Point, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		App.RoutePointerDownEvent(Hit, FPointerEvent(0, Point, Point, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		App.RoutePointerUpEvent(Hit, FPointerEvent(0, Point, Point, TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState())); return true;
	}
	bool Opened()
	{
		return Check(CurrentNpc.IsValid() && PC.IsValid() && CurrentNpc->IsDialogueOpen()
			&& PC->GetDialogueNPC() == CurrentNpc.Get() && PC->IsNpcDialogueVisible() && CurrentNpc->GetTutorialWidget()
			&& PC->bShowMouseCursor && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored(), TEXT("F_opens_owned_focus_cursor_and_gameplay_guard"));
	}
	bool Restored()
	{
		return Check(PC.IsValid() && (!CurrentNpc.IsValid() || !CurrentNpc->IsDialogueOpen()) && !PC->GetDialogueNPC()
			&& !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor && !PC->IsAnyBlockingWindowVisible(),
			TEXT("dialogue_releases_only_owned_input_and_cursor"));
	}
	void PhotoThen(const TCHAR* Name, TFunction<void()> After)
	{
		if (!Check(!ScreenshotHandle.IsValid() && !FScreenshotRequest::IsScreenshotRequested(), TEXT("exclusive_screenshot_request"))) { return; }
		PendingPhoto = Output / Name; bPhotoProcessed = false;
		ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddSP(AsShared(), &FRun::PhotoProcessed);
		FScreenshotRequest::RequestScreenshot(PendingPhoto, true, false);
		WaitPhoto(FPlatformTime::Seconds() + 10.0, MoveTemp(After));
	}
	void PhotoProcessed() { bPhotoProcessed = true; }
	void WaitPhoto(double Limit, TFunction<void()> After)
	{
		TArray<uint8> Bytes;
		const bool bPng = bPhotoProcessed && !FScreenshotRequest::IsScreenshotRequested()
			&& IFileManager::Get().FileSize(*PendingPhoto) > 1024 && FFileHelper::LoadFileToArray(Bytes, *PendingPhoto)
			&& Bytes.Num() > 8 && Bytes[0] == 137 && Bytes[1] == 80 && Bytes[2] == 78 && Bytes[3] == 71
			&& Bytes[4] == 13 && Bytes[5] == 10 && Bytes[6] == 26 && Bytes[7] == 10;
		if (bPng)
		{
			FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle); ScreenshotHandle.Reset();
			Captures.Add(FPaths::GetCleanFilename(PendingPhoto)); Event(TEXT("processed_PNG_completed"), PendingPhoto);
			Later(0.05, MoveTemp(After)); return;
		}
		if (FPlatformTime::Seconds() > Limit) { Check(false, TEXT("processed_screenshot_and_PNG_complete_before_transition")); return; }
		Later(0.05, [this, Limit, After = MoveTemp(After)]() mutable { WaitPhoto(Limit, MoveTemp(After)); });
	}
	void V1Offer()
	{
		if (!Opened() || !Check(Quests.IsValid() && Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Available,
			TEXT("V1_starts_available"))) { return; }
		auto* Widget = CurrentNpc->GetTutorialWidget(); auto* Extra = Widget->GetWidgetFromName(TEXT("TownService1"));
		if (!Check(Extra && Extra->GetVisibility() == ESlateVisibility::Collapsed, TEXT("unused_legacy_service_buttons_collapsed"))) { return; }
		Key(EKeys::Up); Key(EKeys::Down);
		if (!Check(CurrentNpc->GetSelectedServiceIndex() == 0, TEXT("one_choice_keyboard_wraps_safely"))) { return; }
		PhotoThen(TEXT("01_v1_offer.png"), [this]
		{
			if (!Click(TEXT("TownService0")) || !Check(Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Active,
				TEXT("actual_mouse_choice_accepts_V1"))) { return; }
			Milestones[TEXT("f_and_slate")] = true; const float Before = RunState->GetRemainingRunSeconds();
			Later(1.2, [this, Before]
			{
				if (!Check(RunState.IsValid() && RunState->GetRemainingRunSeconds() < Before && !UGameplayStatics::IsGamePaused(World.Get()),
					TEXT("field_dialogue_keeps_current_run_clock_active"))) { return; }
				Key(EKeys::F); if (!Restored()) { return; }
				VisitObjective(TEXT("Q_V1_supply"), 0, [this]
				{
					NearNpc(TEXT("Q_V2_stolen_grain"), [this]
					{
						if (!Check(Quests->GetObjectiveProgress(TEXT("Q_V1_supply")) == 1
							&& Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Active,
							TEXT("V1_partial_progress_survives_other_village"))) { return; }
						VisitObjective(TEXT("Q_V1_supply"), 1, [this] { RecreateV1Npc(); });
					});
				});
			});
		});
	}
	void RecreateV1Npc()
	{
		auto* Original = GetNpc(TEXT("Q_V1_supply"));
		if (!Check(Original && Quests.IsValid() && Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Ready,
			TEXT("V1_two_actual_objectives_ready_before_revisit"))) { return; }
		const FTransform Transform = Original->GetActorTransform(); TWeakObjectPtr<URogue10mVillageQuestDataAsset> Definition = Original->QuestDefinition;
		TWeakObjectPtr<URogue10mVillageQuestComponent> OriginalProgress = Quests;
		Original->Destroy(); CurrentNpc.Reset(); Npcs.Remove(TEXT("Q_V1_supply"));
		Later(0.2, [this, Transform, Definition, OriginalProgress]
		{
			if (!LiveCandidate() || !Check(Definition.IsValid(), TEXT("quest_definition_survives_NPC_destruction"))) { return; }
			auto* Replacement = World->SpawnActorDeferred<ARogue10mVillageQuestNPC>(ARogue10mVillageQuestNPC::StaticClass(),
				Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Check(Replacement != nullptr, TEXT("same_world_NPC_recreation_succeeded"))) { return; }
			Replacement->QuestId = TEXT("Q_V1_supply"); Replacement->QuestDefinition = Definition.Get(); Replacement->FinishSpawning(Transform);
			RecreatedNpc = Replacement; Npcs.Add(TEXT("Q_V1_supply"), Replacement);
			if (!Check(Quests.IsValid() && OriginalProgress.IsValid() && Quests == OriginalProgress
				&& Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Ready,
				TEXT("PlayerState_progress_survives_same_world_NPC_recreation"))) { return; }
			Milestones[TEXT("npc_recreated")] = true; Milestones[TEXT("interrupted_revisit")] = true;
			NearNpc(TEXT("Q_V1_supply"), [this] { FPress([this]
			{
				if (!Opened()) { return; }
				PhotoThen(TEXT("02_v1_ready_after_revisit.png"), [this] { ReportCurrent(TEXT("Q_V1_supply"), [this] { V1AfterReport(); }); });
			}); });
		});
	}
	void ReportCurrent(FName Id, TFunction<void()> After)
	{
		if (!Opened() || !Check(Quests.IsValid() && Inventory.IsValid() && Quests->GetQuestState(Id) == ERogue10mVillageQuestState::Ready,
			TEXT("objective_complete_before_UI_report"))) { return; }
		const auto* Definition = Quests->GetDefinition(); const int32 Index = Definition ? Definition->FindQuestIndex(Id) : INDEX_NONE;
		if (!Check(Definition && Definition->Quests.IsValidIndex(Index) && !Definition->Quests[Index].RewardItem,
			TEXT("fixture_uses_reviewed_run_gold_reward"))) { return; }
		const int32 Before = Inventory->GetGold(), Award = Definition->Quests[Index].RewardGold;
		Key(EKeys::Enter);
		if (!Check(Quests->GetQuestState(Id) == ERogue10mVillageQuestState::Claimed && Inventory->GetGold() == Before + Award,
			TEXT("Slate_Enter_reports_exactly_one_quest_gold_reward"))) { return; }
		const int32 ClaimedGold = Inventory->GetGold(); Key(EKeys::Enter); Key(EKeys::Enter, true);
		if (!Check(Inventory->GetGold() == ClaimedGold && Quests->GetQuestState(Id) == ERogue10mVillageQuestState::Claimed,
			TEXT("repeat_and_held_Enter_cannot_duplicate_reward"))) { return; }
		++ReportedQuests; Milestones[TEXT("duplicate_claim_blocked")] = true;
		Later(0.2, MoveTemp(After));
	}
	void V1AfterReport()
	{
		PhotoThen(TEXT("03_v1_rewarded.png"), [this]
		{
			auto* OtherVillage = GetNpc(TEXT("Q_V2_stolen_grain"));
			if (!Check(OtherVillage != nullptr, TEXT("other_village_for_distance_close"))
				|| !PlaceCandidate(OtherVillage->GetActorLocation() + FVector(180, 0, 95))) { return; }
			Later(0.5, [this]
			{
				if (!Restored()) { return; }
				NearNpc(TEXT("Q_V1_supply"), [this] { FPress([this]
				{
					if (!Opened() || !Check(Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Claimed,
						TEXT("F_reentry_preserves_claimed_receipt")) || !Click(TEXT("TownCloseDialogue")) || !Restored()) { return; }
					VisitObjective(TEXT("Q_V4_record"), 0, [this] { VisitObjective(TEXT("Q_V4_record"), 1, [this] { V4Preaccepted(); }); });
				}); });
			});
		});
	}
	void V4Preaccepted()
	{
		if (!Check(Quests.IsValid() && Quests->GetQuestState(TEXT("Q_V4_record")) == ERogue10mVillageQuestState::Available
			&& Quests->GetObjectiveProgress(TEXT("Q_V4_record")) == 2, TEXT("V4_local_facts_recorded_before_independent_acceptance"))) { return; }
		NearNpc(TEXT("Q_V4_record"), [this] { FPress([this]
		{
			if (!Opened()) { return; } Key(EKeys::Enter);
			if (!Check(Quests->GetQuestState(TEXT("Q_V4_record")) == ERogue10mVillageQuestState::Ready,
				TEXT("preacceptance_facts_make_V4_ready_after_UI_accept"))) { return; }
			Milestones[TEXT("preacceptance")] = true;
			PhotoThen(TEXT("04_v4_preacceptance_ready.png"), [this] { ReportCurrent(TEXT("Q_V4_record"), [this]
			{
				const int32 StoryGold = Inventory->GetGold();
				if (!Check(Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Found,
					TEXT("real_transport_record_provides_optional_story_fact")) || !Click(TEXT("TownService1"))) { return; }
				if (!Check(Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Analyzed
					&& Quests->GetQuestState(TEXT("Q_V4_record")) == ERogue10mVillageQuestState::Claimed
					&& Inventory->GetGold() == StoryGold && CurrentNpc->GetSelectedServiceIndex() == 0,
					TEXT("Slate_optional_analysis_keeps_paid_primary_receipt_currency_and_visible_selection"))) { return; }
				Key(EKeys::Escape); if (!Restored()) { return; }
				VisitObjective(TEXT("Q_V3_quarry"), 0, [this] { KillQuarryTarget(); });
			}); });
		}); });
	}
	void KillQuarryTarget()
	{
		if (!Check(QuarryMonster.IsValid() && !QuarryMonster->IsDead() && !QuarryMonster->IsCombatTestInstance()
			&& Quests->GetObjectiveProgress(TEXT("Q_V3_quarry")) == 1, TEXT("live_authored_quarry_target_and_only_exploration_fact"))) { return; }
		const FVector Position = QuarryMonster->GetActorLocation() + FVector(450, 0, 0);
		if (!PlaceCandidate(Position)) { return; }
		PC->SetControlRotation((QuarryMonster->GetActorLocation() - Position).Rotation() + FRotator(-12, 0, 0));
		Later(0.2, [this]
		{
			if (!Check(QuarryMonster.IsValid() && Quests->GetObjectiveProgress(TEXT("Q_V3_quarry")) == 1,
				TEXT("combat_proximity_alone_does_not_complete_kill_goal"))) { return; }
			bDamageInjected = true;
			const float Applied = UGameplayStatics::ApplyDamage(QuarryMonster.Get(), 1000000.0f, PC.Get(), Player.Get(), nullptr);
			if (!Check(Applied > 0.0f, TEXT("real_ApplyDamage_uses_player_controller_instigator"))) { return; }
			Later(0.4, [this]
			{
				if (!Check(Quests.IsValid() && Quests->GetObjectiveProgress(TEXT("Q_V3_quarry")) == 2
					&& Quests->GetQuestState(TEXT("Q_V3_quarry")) == ERogue10mVillageQuestState::Available
					&& (!QuarryMonster.IsValid() || QuarryMonster->IsDead()), TEXT("actual_attributed_death_records_preacceptance_combat_fact"))) { return; }
				Milestones[TEXT("attributed_kill")] = true;
				RecreateDirectorAfterKill([this] { NearNpc(TEXT("Q_V3_quarry"), [this] { FPress([this]
				{
					if (!Opened()) { return; } Key(EKeys::Enter);
					PhotoThen(TEXT("05_v3_actual_kill_ready.png"), [this] { ReportCurrent(TEXT("Q_V3_quarry"), [this]
					{
						Key(EKeys::Escape); if (!Restored()) { return; }
						CompleteExploration(TEXT("Q_V2_stolen_grain"), [this]
						{
							CompleteExploration(TEXT("Q_V5_embers"), [this] { FinalChecks(); });
						});
					}); });
				}); }); });
			});
		});
	}
	void RecreateDirectorAfterKill(TFunction<void()> After)
	{
		if (!Check(ActiveDirector.IsValid() && Quests.IsValid(), TEXT("active_director_and_player_progress_before_recreation"))) { return; }
		const FTransform Transform = ActiveDirector->GetActorTransform();
		TWeakObjectPtr<URogue10mVillageQuestDataAsset> Definition = ActiveDirector->QuestDefinition;
		TWeakObjectPtr<URogue10mVillageQuestComponent> OriginalProgress = Quests;
		// The individually recreated V1 NPC is fixture-owned rather than director-owned.
		if (RecreatedNpc.IsValid()) { RecreatedNpc->Destroy(); RecreatedNpc.Reset(); }
		ActiveDirector->Destroy(); ActiveDirector.Reset(); CurrentNpc.Reset(); Npcs.Reset();
		Later(0.2, [this, Transform, Definition, OriginalProgress, After = MoveTemp(After)]() mutable
		{
			if (!LiveCandidate() || !Check(Definition.IsValid() && Quests.IsValid() && OriginalProgress.IsValid() && Quests == OriginalProgress,
				TEXT("player_progress_and_definition_survive_director_unload"))) { return; }
			auto* Replacement = World->SpawnActorDeferred<ARogue10mVillageQuestDirector>(ARogue10mVillageQuestDirector::StaticClass(),
				Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Check(Replacement != nullptr, TEXT("same_run_director_recreation_succeeded"))) { return; }
			Replacement->QuestDefinition = Definition.Get(); Replacement->FinishSpawning(Transform);
			ActiveDirector = Replacement; RecreatedDirector = Replacement;
			Later(0.3, [this, OriginalProgress, After = MoveTemp(After)]() mutable
			{
				int32 Directors = 0, Markers = 0, Monsters = 0;
				for (TActorIterator<ARogue10mVillageQuestDirector> It(World.Get()); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Directors; } }
				for (TActorIterator<ARogue10mVillageQuestNPC> It(World.Get()); It; ++It)
				{
					if (!It->IsActorBeingDestroyed()) { Check(!Npcs.Contains(It->QuestId), TEXT("unique_recreated_quest_NPC")); Npcs.Add(It->QuestId, *It); }
				}
				for (TActorIterator<ARogue10mVillageQuestObjective> It(World.Get()); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Markers; } }
				for (TActorIterator<ARogue10mVillageQuestMonster> It(World.Get()); It; ++It) { if (!It->IsActorBeingDestroyed() && !It->IsDead()) { ++Monsters; } }
				if (!Check(Directors == 1 && Npcs.Num() == 5 && Markers == 9 && Monsters == 0
					&& Quests.IsValid() && OriginalProgress.IsValid() && Quests == OriginalProgress
					&& Quests->GetObjectiveProgress(TEXT("Q_V3_quarry")) == 2
					&& Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Analyzed
					&& Quests->GetQuestState(TEXT("Q_V1_supply")) == ERogue10mVillageQuestState::Claimed
					&& Quests->GetQuestState(TEXT("Q_V4_record")) == ERogue10mVillageQuestState::Claimed,
					TEXT("same_run_director_reload_preserves_receipts_and_does_not_respawn_killed_encounter"))) { return; }
				Milestones[TEXT("director_recreated_without_encounter_respawn")] = true; After();
			});
		});
	}
	void CompleteExploration(FName Id, TFunction<void()> After)
	{
		NearNpc(Id, [this, Id, After = MoveTemp(After)]() mutable { FPress([this, Id, After = MoveTemp(After)]() mutable
		{
			if (!Opened()) { return; } Key(EKeys::Enter);
			if (!Check(Quests->GetQuestState(Id) == ERogue10mVillageQuestState::Active, TEXT("independent_remaining_village_accepts_through_UI"))) { return; }
			Key(EKeys::Escape); if (!Restored()) { return; }
			VisitObjective(Id, 0, [this, Id, After = MoveTemp(After)]() mutable { VisitObjective(Id, 1, [this, Id, After = MoveTemp(After)]() mutable
			{
				NearNpc(Id, [this, Id, After = MoveTemp(After)]() mutable { FPress([this, Id, After = MoveTemp(After)]() mutable
				{
					if (!Opened()) { return; } ReportCurrent(Id, [this, After = MoveTemp(After)]() mutable
					{
						Key(EKeys::Escape); if (Restored()) { After(); }
					});
				}); });
			}); });
		}); });
	}
	void FinalChecks()
	{
		if (!LiveCandidate() || !Restored()) { return; }
		bool bAllClaimed = ReportedQuests == 5;
		for (const TCHAR* Id : {TEXT("Q_V1_supply"), TEXT("Q_V2_stolen_grain"), TEXT("Q_V3_quarry"), TEXT("Q_V4_record"), TEXT("Q_V5_embers")})
		{
			bAllClaimed &= Quests->GetQuestState(FName(Id)) == ERogue10mVillageQuestState::Claimed;
		}
		if (!Check(bAllClaimed && RunState->GetRemainingRunSeconds() < FieldInitialRemaining,
			TEXT("all_five_quests_claimed_in_same_live_run_with_timer_advanced"))) { return; }
		Milestones[TEXT("all_five_claimed")] = true; Milestones[TEXT("input_restored")] = true;
		NearNpc(TEXT("Q_V1_supply"), [this] { FPress([this]
		{
			if (!Opened()) { return; }
			const int32 StoryGold = Inventory->GetGold();
			if (!Click(TEXT("TownService1"))) { return; }
			bool bPrimaryReceiptsKept = true;
			for (const TCHAR* Id : {TEXT("Q_V1_supply"), TEXT("Q_V2_stolen_grain"), TEXT("Q_V3_quarry"), TEXT("Q_V4_record"), TEXT("Q_V5_embers")})
			{
				bPrimaryReceiptsKept &= Quests->GetQuestState(FName(Id)) == ERogue10mVillageQuestState::Claimed;
			}
			if (!Check(Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Reported
				&& bPrimaryReceiptsKept && Inventory->GetGold() == StoryGold && CurrentNpc->GetSelectedServiceIndex() == 0,
				TEXT("Slate_return_report_completes_optional_story_without_extra_payout_or_primary_changes"))) { return; }
			CurrentNpc->ActivateService(1); // Stale removed-row activation must be harmless.
			if (!Check(Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Reported && Inventory->GetGold() == StoryGold,
				TEXT("removed_optional_story_row_cannot_repeat_or_pay_again"))) { return; }
			Key(EKeys::Escape); if (!Restored()) { return; }
			PhotoThen(TEXT("06_all_five_complete_input_restored.png"), [this] { Finish(); });
		}); });
	}
	void Finish()
	{
		if (bFinished) { return; } bFinished = true; Next = nullptr;
		if (ScreenshotHandle.IsValid()) { FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle); ScreenshotHandle.Reset(); }
		if (CurrentNpc.IsValid()) { CurrentNpc->CloseDialogue(); }
		if (RecreatedNpc.IsValid()) { RecreatedNpc->Destroy(); }
		if (RecreatedDirector.IsValid()) { RecreatedDirector->Destroy(); }
		if (Failures == 0)
		{
			for (const auto& Mark : Milestones) { if (!Mark.Value) { ++Failures; } }
			if (Captures.Num() != 6) { ++Failures; }
			for (const TCHAR* Capture : {TEXT("01_v1_offer.png"), TEXT("02_v1_ready_after_revisit.png"), TEXT("03_v1_rewarded.png"),
				TEXT("04_v4_preacceptance_ready.png"), TEXT("05_v3_actual_kill_ready.png"), TEXT("06_all_five_complete_input_restored.png")})
			{
				if (!Captures.Contains(FString(Capture))) { ++Failures; }
			}
		}
		if (!FFileHelper::SaveStringToFile(Events, *(Output / TEXT("events.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { ++Failures; }
		auto Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("label"), Label);
		Json->SetStringField(TEXT("status"), Failures ? TEXT("failed") : TEXT("passed")); Json->SetNumberField(TEXT("checks"), Checks); Json->SetNumberField(TEXT("failures"), Failures);
		Json->SetBoolField(TEXT("physical_input"), false); Json->SetBoolField(TEXT("full_run"), false); Json->SetBoolField(TEXT("facts_injected"), false);
		Json->SetBoolField(TEXT("damage_injected"), bDamageInjected); Json->SetBoolField(TEXT("rendered"), true);
		Json->SetStringField(TEXT("fixture_positioning"), TEXT("teleport_in_candidate_only")); Json->SetNumberField(TEXT("candidate_teleports"), CandidateTeleports);
		Json->SetBoolField(TEXT("normal_portal_entry"), Milestones.FindRef(TEXT("normal_portal_entry")));
		Json->SetBoolField(TEXT("optional_story_completed"), Quests.IsValid()
			&& Quests->GetOptionalClueState() == ERogue10mVillageOptionalClueState::Reported);
		Json->SetNumberField(TEXT("hub_walked_distance_cm"), HubWalkedDistance); Json->SetNumberField(TEXT("reported_quests"), ReportedQuests);
		Json->SetStringField(TEXT("profile_id"), ProfileId.ToString(EGuidFormats::Digits)); Json->SetStringField(TEXT("run_id"), EntryRunId.ToString(EGuidFormats::Digits));
		Json->SetNumberField(TEXT("wall_seconds"), FPlatformTime::Seconds() - StartedAt); Json->SetArrayField(TEXT("assertions"), Assertions);
		auto Marks = MakeShared<FJsonObject>(); for (const auto& Pair : Milestones) { Marks->SetBoolField(Pair.Key, Pair.Value); } Json->SetObjectField(TEXT("milestones"), Marks);
		TArray<TSharedPtr<FJsonValue>> Photos; for (const FString& Capture : Captures) { Photos.Add(MakeShared<FJsonValueString>(Capture)); } Json->SetArrayField(TEXT("captures"), Photos);
		FString Serialized; auto Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Json, Writer) || !FFileHelper::SaveStringToFile(Serialized, *(Output / TEXT("results.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			++Failures; UE_LOG(LogRogue10m, Error, TEXT("VILLAGE_QUEST_RUNTIME report_write_failed"));
		}
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=VILLAGE_QUEST_RUNTIME_%s label=%s checks=%d failures=%d output=%s"), Failures ? TEXT("FAILED") : TEXT("PASSED"), *Label, Checks, Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TWeakObjectPtr<UWorld> World; TWeakObjectPtr<UGameInstance> Instance;
	TWeakObjectPtr<ARogue10mPlayerController> PC; TWeakObjectPtr<ARogue10mCharacter> Player; TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<ARogue10mGameState> RunState; TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;
	TWeakObjectPtr<URogue10mInventoryComponent> Inventory; TWeakObjectPtr<URogue10mVillageQuestComponent> Quests;
	TWeakObjectPtr<ARogue10mVillageQuestNPC> CurrentNpc, RecreatedNpc;
	TWeakObjectPtr<ARogue10mVillageQuestDirector> ActiveDirector, RecreatedDirector;
	TWeakObjectPtr<ARogue10mVillageQuestMonster> QuarryMonster; TMap<FName, TWeakObjectPtr<ARogue10mVillageQuestNPC>> Npcs;
	TMap<FString, bool> Milestones; TArray<TSharedPtr<FJsonValue>> Assertions; TArray<FString> Captures; TArray<FVector> Waypoints;
	TFunction<void()> Next; FDelegateHandle ScreenshotHandle; FString Label, Output, Events, PendingPhoto;
	FGuid ProfileId, EntryRunId; FVector PreviousHubPosition;
	double StartedAt = 0, Deadline = 0, NextAt = 0, LastDistance = 0, ProgressAt = 0, HubWalkedDistance = 0;
	float FieldInitialRemaining = 0; int32 Checks = 0, Failures = 0, Waypoint = 0, CandidateTeleports = 0, ReportedQuests = 0;
	bool bFinished = false, bWalkingHub = false, bPhotoProcessed = false, bDamageInjected = false;
};

static TSharedPtr<FRun> Active;
static void Begin(UWorld* World)
{
	if (GIsEditor || (Active && !Active->Finished())) { return; }
	if (!World && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				if (World) { UE_LOG(LogRogue10m, Error, TEXT("RESULT=VILLAGE_QUEST_RUNTIME_FAILED ambiguous_game_world")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
				World = Context.World();
			}
		}
	}
	if (!World) { UE_LOG(LogRogue10m, Error, TEXT("RESULT=VILLAGE_QUEST_RUNTIME_FAILED game_world_required")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
	Active = MakeShared<FRun>(); Active->Start(*World);
}
static FAutoConsoleCommandWithWorld Command(TEXT("Rogue10m.TestVillageQuests"),
	TEXT("Fresh rendered QA slots only. Normal hub portal entry and candidate quest F/Slate/overlap/attributed-kill lifecycle."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Begin));
}
#endif
