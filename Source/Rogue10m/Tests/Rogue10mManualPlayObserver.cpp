// Editor-build, opt-in observation of a fresh QA session driven by external input.
// No input injection, profile selection, gameplay commands, save requests or exit requests.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "UnrealClient.h"

namespace Rogue10mManualPlayObservation
{
constexpr double SampleInterval = 0.25;
constexpr int32 MaximumScreenshots = 128;
constexpr int32 MaximumInputFrames = 216000;

static FString Csv(const FString& Value)
{
    return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")) + TEXT("\"");
}

static FString Flag(bool Value) { return Value ? TEXT("1") : TEXT("0"); }
static FString Number(double Value) { return FString::SanitizeFloat(Value, 6); }
static FString Guid(const FGuid& Value) { return Value.IsValid() ? Value.ToString(EGuidFormats::Digits) : FString(); }

static bool SingleOption(const TArray<FString>& Options, const TCHAR* Key, FString& Out)
{
    const FString Prefix = FString(Key) + TEXT("=");
    int32 Count = 0;
    for (const FString& Option : Options)
    {
        if (Option.StartsWith(Prefix, ESearchCase::IgnoreCase))
        {
            ++Count;
            Out = Option.Mid(Prefix.Len());
            if (Out.Len() >= 2 && Out.StartsWith(TEXT("\"")) && Out.EndsWith(TEXT("\""))) { Out = Out.Mid(1, Out.Len() - 2); }
        }
    }
    return Count == 1 && !Out.IsEmpty();
}

class FObserver : public TSharedFromThis<FObserver>
{
public:
    bool IsFinished() const { return bFinished; }

    bool Start(UWorld* InitialWorld, FOutputDevice& Ar)
    {
        TArray<FString> Tokens, Options;
        FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
        FString Town, Progression, Keys, RequestedOutput, Timeout;
        bool bValid = SingleOption(Options, TEXT("RogueManualPlayLabel"), Label)
            && SingleOption(Options, TEXT("RogueManualPlayOutput"), RequestedOutput)
            && SingleOption(Options, TEXT("RogueProfileSaveSlot"), Town)
            && SingleOption(Options, TEXT("RogueProgressionSavePrefix"), Progression)
            && SingleOption(Options, TEXT("RogueSkillKeySaveSlot"), Keys)
            && SingleOption(Options, TEXT("RogueManualPlayTimeoutSeconds"), Timeout);
        bValid &= !Label.IsEmpty() && Label.Len() <= 24;
        for (TCHAR C : Label) { bValid &= (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_'); }
        int32 Seconds = 0;
        bValid &= LexTryParseString(Seconds, *Timeout) && Seconds >= 1 && Seconds <= 1800;
        bValid &= Town == TEXT("Rogue10m_Town_QA_Manual_") + Label
            && Progression == TEXT("Rogue10m_Progression_QA_Manual_") + Label
            && Keys == TEXT("Rogue10m_Fighter_QA_Manual_") + Label;
        Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("DevLog/ManualPlayQA") / Label / TEXT("Observation"));
        FString ActualOutput = FPaths::ConvertRelativePathToFull(RequestedOutput);
        FPaths::NormalizeDirectoryName(Output);
        FPaths::NormalizeDirectoryName(ActualOutput);
        bValid &= FPaths::IsSamePath(Output, ActualOutput);
        bValid &= InitialWorld && !GIsEditor && InitialWorld->WorldType == EWorldType::Game
            && InitialWorld->GetNetMode() == NM_Standalone
            && UGameplayStatics::GetCurrentLevelName(InitialWorld, true) == TEXT("L_Menu")
            && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))
            && !FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));
        const UGameInstance* Instance = InitialWorld ? InitialWorld->GetGameInstance() : nullptr;
        const auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        const auto* PC = InitialWorld ? Cast<ARogue10mPlayerController>(InitialWorld->GetFirstPlayerController()) : nullptr;
        bValid &= Profiles && Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile()
            && PC && PC->IsMainMenuVisible();
        TArray<FString> Existing;
        IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(Progression + TEXT("_*.sav")), true, false);
        bValid &= !UGameplayStatics::DoesSaveGameExist(Town, 0) && !UGameplayStatics::DoesSaveGameExist(Keys, 0)
            && Existing.IsEmpty() && !IFileManager::Get().DirectoryExists(*Output) && !IFileManager::Get().FileExists(*Output);
        if (!bValid)
        {
            Ar.Logf(TEXT("Manual observer rejected: exact fresh QA_Manual label/three slots/output, visible standalone L_Menu and untouched profile UI required. No gameplay state was changed; game remains running."));
            UE_LOG(LogRogue10m, Error, TEXT("MANUAL_OBSERVER_REJECTED exact_fresh_visible_menu_namespace_required"));
            return false;
        }
        if (!IFileManager::Get().MakeDirectory(*Output, true)
            || !IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true))
        {
            Ar.Logf(TEXT("Manual observer could not create fresh evidence output. Game remains running."));
            return false;
        }
        CsvPath = Output / TEXT("samples.csv");
        ShotPath = Output / TEXT("screenshots.csv");
        InputPath = Output / TEXT("input-frames.csv");
        const FString Header = TEXT("sample,utc,wall_seconds,frame_counter,core_delta_seconds,world_delta_seconds,map,phase,run_id,selected_profile,world_seconds,run_remaining_seconds,paused,pawn_present,x,y,z,speed,health,max_health,player_level,current_xp,weapon,run_gold,run_crystals,lobby_available,save_ready,save_retry_available,save_attached,attached_profile,attached_run,save_error,settled,victory,abandoned,settlement_saved,travel_started,result_visible,main_menu_visible,character_lobby_visible,inventory_visible,skill_tree_visible,settings_visible,npc_dialogue_open,game_key_w,game_key_a,game_key_s,game_key_d,game_key_shift,game_key_f,game_key_e,game_key_lmb\n");
        const FString ShotHeader = TEXT("utc,event,image,trigger_state,request_state,world_state_when_logged,bytes,note\n");
        const FString InputHeader = TEXT("frame_counter,utc,wall_seconds,world_seconds,core_delta_seconds,world_delta_seconds,map,paused,pawn_present,x,y,z,speed,move_ignored,blocking_window,last_input_x,last_input_y,game_key_w,game_key_a,game_key_s,game_key_d,game_key_shift,w_just_pressed,w_just_released,a_just_pressed,a_just_released,s_just_pressed,s_just_released,d_just_pressed,d_just_released,game_key_f,game_key_e,game_key_lmb\n");
        if (!WriteNew(CsvPath, Header) || !WriteNew(ShotPath, ShotHeader) || !WriteNew(InputPath, InputHeader))
        {
            Ar.Logf(TEXT("Manual observer could not initialize evidence files. Existing files were not overwritten. Game remains running."));
            return false;
        }
        GameInstance = InitialWorld->GetGameInstance();
        Started = FPlatformTime::Seconds();
        Deadline = Started + Seconds;
        NextSample = Started + SampleInterval;
        bFinished = false;
        const TWeakPtr<FObserver> Weak = AsShared();
        ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak]
        {
            if (auto Self = Weak.Pin()) { Self->Stop(TEXT("process_pre_exit")); }
        });
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaSeconds)
        {
            const auto Self = Weak.Pin();
            return Self && Self->Tick(DeltaSeconds);
        }), 0.f);
        Record(InitialWorld);
        WriteSummary(TEXT("observing"));
        Ar.Logf(TEXT("Read-only manual observer started: %s. It never generates input or verifies physical input/full-run success."), *Output);
        UE_LOG(LogRogue10m, Display, TEXT("MANUAL_OBSERVER_STARTED output=%s limit_seconds=%d"), *Output, Seconds);
        return true;
    }

    void Stop(const TCHAR* Reason, bool bRemoveTicker = true)
    {
        if (bFinished) { return; }
        bFinished = true;
        if (bRemoveTicker && TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); }
        TickerHandle.Reset();
        FCoreDelegates::OnPreExit.Remove(ExitHandle);
        ExitHandle.Reset();
        if (!Pending.File.IsEmpty())
        {
            ShotEvent(TEXT("unconfirmed_on_observer_stop"), Pending, TEXT("No screenshot request was cancelled; any late capture is not confirmed by this summary."));
            ++ShotsUnconfirmed;
        }
        for (const FShot& Shot : Queue)
        {
            ShotEvent(TEXT("not_requested_on_observer_stop"), Shot, TEXT("Observer stopped; game remains running."));
            ++ShotsUnconfirmed;
        }
        Queue.Reset();
        FlushInputFrames();
        WriteSummary(Reason);
        UE_LOG(LogRogue10m, Display, TEXT("MANUAL_OBSERVER_STOPPED reason=%s samples=%d output=%s game_exit_not_requested"), Reason, Samples, *Output);
    }

private:
    struct FShot
    {
        FString File, TriggerState, RequestState;
        double RequestedAt = 0.;
    };

    bool WriteNew(const FString& Path, const FString& Text) const
    {
        return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_NoReplaceExisting);
    }
    bool Append(const FString& Path, const FString& Text)
    {
        if (FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append)) { return true; }
        bIoFailed = true;
        return false;
    }
    UWorld* FindWorld() const
    {
        if (!GEngine || !GameInstance.IsValid()) { return nullptr; }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (World && World->WorldType == EWorldType::Game && World->GetGameInstance() == GameInstance.Get()) { return World; }
        }
        return nullptr;
    }
    FString WorldState(UWorld* World) const
    {
        const auto* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
        const auto* Instance = GameInstance.Get();
        const auto* Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        return (World ? UGameplayStatics::GetCurrentLevelName(World, true) : TEXT("<travel>"))
            + TEXT("|") + (Run ? UEnum::GetValueAsString(Run->GetRunPhase()) : TEXT("none"))
            + TEXT("|") + (Saves ? Guid(Saves->GetAttachedRunId()) : FString());
    }
    void FlushInputFrames()
    {
        if (InputBuffer.IsEmpty()) { return; }
        if (Append(InputPath, InputBuffer)) { InputFramesFlushed += BufferedInputFrames; }
        InputBuffer.Reset();
        BufferedInputFrames = 0;
    }
    void RecordInputFrame(UWorld* World)
    {
        if (LastInputFrame == GFrameCounter || InputFrames >= MaximumInputFrames) { return; }
        LastInputFrame = GFrameCounter;
        const auto* PC = World ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        const auto* Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        const FVector Position = Character ? Character->GetActorLocation() : FVector::ZeroVector;
        const FVector LastInput = Character ? Character->GetLastMovementInputVector() : FVector::ZeroVector;
        TArray<FString> Values = {
            LexToString(GFrameCounter), FDateTime::UtcNow().ToIso8601(), Number(FPlatformTime::Seconds() - Started), World ? Number(World->GetTimeSeconds()) : FString(),
            Number(LastTickerDelta), World ? Number(World->GetDeltaSeconds()) : FString(), World ? UGameplayStatics::GetCurrentLevelName(World, true) : TEXT("<travel>"),
            World ? Flag(UGameplayStatics::IsGamePaused(World)) : FString(), Flag(Character != nullptr),
            Character ? Number(Position.X) : FString(), Character ? Number(Position.Y) : FString(), Character ? Number(Position.Z) : FString(),
            Character ? Number(Character->GetVelocity().Size()) : FString(), PC ? Flag(PC->IsMoveInputIgnored()) : FString(), PC ? Flag(PC->IsAnyBlockingWindowVisible()) : FString(),
            Character ? Number(LastInput.X) : FString(), Character ? Number(LastInput.Y) : FString(),
            PC ? Flag(PC->IsInputKeyDown(EKeys::W)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::A)) : FString(),
            PC ? Flag(PC->IsInputKeyDown(EKeys::S)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::D)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::LeftShift)) : FString(),
            PC ? Flag(PC->WasInputKeyJustPressed(EKeys::W)) : FString(), PC ? Flag(PC->WasInputKeyJustReleased(EKeys::W)) : FString(),
            PC ? Flag(PC->WasInputKeyJustPressed(EKeys::A)) : FString(), PC ? Flag(PC->WasInputKeyJustReleased(EKeys::A)) : FString(),
            PC ? Flag(PC->WasInputKeyJustPressed(EKeys::S)) : FString(), PC ? Flag(PC->WasInputKeyJustReleased(EKeys::S)) : FString(),
            PC ? Flag(PC->WasInputKeyJustPressed(EKeys::D)) : FString(), PC ? Flag(PC->WasInputKeyJustReleased(EKeys::D)) : FString(),
            PC ? Flag(PC->IsInputKeyDown(EKeys::F)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::E)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::LeftMouseButton)) : FString()
        };
        for (FString& Value : Values) { Value = Csv(Value); }
        InputBuffer += FString::Join(Values, TEXT(",")) + TEXT("\n");
        ++InputFrames;
        ++BufferedInputFrames;
        // Bounded per-frame observation, flushed in small batches rather than
        // opening a file every frame. A hard termination can lose the last batch.
        if (InputFrames >= MaximumInputFrames || FPlatformTime::Seconds() >= NextInputFlush || BufferedInputFrames >= 256)
        {
            FlushInputFrames();
            NextInputFlush = FPlatformTime::Seconds() + SampleInterval;
        }
    }
    bool Tick(float DeltaSeconds)
    {
        if (bFinished) { return false; }
        const double Now = FPlatformTime::Seconds();
        if (Now >= Deadline) { Stop(TEXT("observation_time_limit_game_continues"), false); return false; }
        if (bIoFailed) { Stop(TEXT("evidence_io_error_game_continues"), false); return false; }
        LastTickerDelta = DeltaSeconds;
        UWorld* CurrentWorld = FindWorld();
        RecordInputFrame(CurrentWorld);
        if (Now >= NextSample)
        {
            Record(CurrentWorld);
            NextSample = Now + SampleInterval;
        }
        UpdateScreenshots();
        return !bFinished;
    }
    void Record(UWorld* World)
    {
        const auto* PC = World ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        const auto* Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        const auto* Player = Character ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
        const auto* Town = Player ? Player->GetTownProgressionComponent() : nullptr;
        const auto* Attributes = Character ? Character->GetRogueAttributeSet() : nullptr;
        const auto* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
        const auto* Instance = GameInstance.Get();
        const auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
        const auto* Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        const auto* Run = World ? World->GetGameState<ARogue10mGameState>() : nullptr;
        const auto* Mode = World ? World->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
        const auto* Completion = Mode ? Mode->GetRunCompletionComponent() : nullptr;
        const FString Map = World ? UGameplayStatics::GetCurrentLevelName(World, true) : TEXT("<travel>");
        const FString Phase = Run ? UEnum::GetValueAsString(Run->GetRunPhase()) : TEXT("none");
        const FString RunId = Saves ? Guid(Saves->GetAttachedRunId()) : FString();
        CurrentState = Map + TEXT("|") + Phase + TEXT("|") + RunId;
        bool bDialogue = false;
        if (World)
        {
            for (TActorIterator<ARogue10mTutorialNPC> It(World); It; ++It) { bDialogue |= It->IsDialogueOpen(); }
        }
        const FVector Position = Character ? Character->GetActorLocation() : FVector::ZeroVector;
        TArray<FString> Values = {
            FString::FromInt(++Samples), FDateTime::UtcNow().ToIso8601(), Number(FPlatformTime::Seconds() - Started), LexToString(GFrameCounter), Number(LastTickerDelta),
            World ? Number(World->GetDeltaSeconds()) : FString(), Map, Phase, RunId,
            Selected ? Guid(Selected->ProfileId) : FString(), World ? Number(World->GetTimeSeconds()) : FString(), Run ? Number(Run->GetRemainingRunSeconds()) : FString(),
            World ? Flag(UGameplayStatics::IsGamePaused(World)) : FString(), Flag(Character != nullptr),
            Character ? Number(Position.X) : FString(), Character ? Number(Position.Y) : FString(), Character ? Number(Position.Z) : FString(),
            Character ? Number(Character->GetVelocity().Size()) : FString(), Attributes ? Number(Attributes->GetHealth()) : FString(), Attributes ? Number(Attributes->GetMaxHealth()) : FString(),
            Player ? FString::FromInt(Player->GetPlayerLevel()) : FString(), Player ? Number(Player->GetCurrentExperience()) : FString(),
            Character ? UEnum::GetValueAsString(Character->GetEquippedWeaponType()) : FString(),
            Inventory ? FString::FromInt(Inventory->GetGold()) : FString(), Inventory ? FString::FromInt(Inventory->GetCrystals()) : FString(),
            // Read the already-attached budget only: subsystem GetLobbyAvailablePoints
            // can lazily load/create a snapshot and change LastError, so is not observational.
            Town && Town->UsesLobbyBudget() ? FString::FromInt(Town->GetAvailableTraitPoints()) : FString(), Saves ? Flag(Saves->IsSaveReady()) : FString(), Saves ? Flag(Saves->CanRetrySave()) : FString(),
            Saves && Character ? Flag(Saves->IsAttachedTo(*Character)) : FString(), Saves ? Guid(Saves->GetAttachedProfileId()) : FString(), RunId,
            Saves ? Saves->GetLastError().ToString() : FString(), Completion ? Flag(Completion->IsSettled()) : FString(), Completion ? Flag(Completion->IsVictory()) : FString(),
            Completion ? Flag(Completion->IsAbandoned()) : FString(), Completion ? Flag(Completion->IsProgressionSaved()) : FString(), Completion ? Flag(Completion->IsTravelStarted()) : FString(),
            PC ? Flag(PC->IsRunResultVisible()) : FString(), PC ? Flag(PC->IsMainMenuVisible()) : FString(), PC ? Flag(PC->IsCharacterLobbyVisible()) : FString(),
            PC ? Flag(PC->IsInventoryVisible()) : FString(), PC ? Flag(PC->IsSkillTreeVisible()) : FString(), PC ? Flag(PC->IsSettingsVisible()) : FString(),
            World ? Flag(bDialogue) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::W)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::A)) : FString(),
            PC ? Flag(PC->IsInputKeyDown(EKeys::S)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::D)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::LeftShift)) : FString(),
            PC ? Flag(PC->IsInputKeyDown(EKeys::F)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::E)) : FString(), PC ? Flag(PC->IsInputKeyDown(EKeys::LeftMouseButton)) : FString()
        };
        for (FString& Value : Values) { Value = Csv(Value); }
        Append(CsvPath, FString::Join(Values, TEXT(",")) + TEXT("\n"));
        LastMap = Map;
        LastPhase = Phase;
        if (World && CurrentState != PreviousState)
        {
            ++Transitions;
            PreviousState = CurrentState;
            if (!SeenStates.Contains(CurrentState) && ShotsQueued < MaximumScreenshots)
            {
                SeenStates.Add(CurrentState);
                FShot Shot;
                Shot.TriggerState = CurrentState;
                Shot.File = Output / TEXT("screenshots") / FString::Printf(TEXT("%03d_transition.png"), ++ShotsQueued);
                Queue.Add(Shot);
                ShotEvent(TEXT("queued"), Shot, TEXT("Map/phase/run identity observation; no gameplay outcome or physical-input verdict."));
            }
            WriteSummary(TEXT("observing"));
        }
    }
    void ShotEvent(const TCHAR* Event, const FShot& Shot, const TCHAR* Note)
    {
        TArray<FString> Values = { FDateTime::UtcNow().ToIso8601(), Event, FPaths::GetCleanFilename(Shot.File), Shot.TriggerState, Shot.RequestState, WorldState(FindWorld()),
            LexToString(IFileManager::Get().FileSize(*Shot.File)), Note };
        for (FString& Value : Values) { Value = Csv(Value); }
        Append(ShotPath, FString::Join(Values, TEXT(",")) + TEXT("\n"));
    }
    void UpdateScreenshots()
    {
        const double Now = FPlatformTime::Seconds();
        if (!Pending.File.IsEmpty())
        {
            if (!FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*Pending.File) > 0)
            {
                ShotEvent(TEXT("file_observed"), Pending, TEXT("Engine screenshot file exists; independent image decoding/visual review still required."));
                ++ShotsObserved;
                Pending = FShot();
            }
            else if (Now - Pending.RequestedAt >= 10.)
            {
                ShotEvent(TEXT("unconfirmed_timeout"), Pending, TEXT("Capture not confirmed in 10 wall seconds; global screenshot request was not reset."));
                ++ShotsUnconfirmed;
                Pending = FShot();
            }
            return;
        }
        UWorld* CurrentWorld = FindWorld();
        if (Queue.IsEmpty() || FScreenshotRequest::IsScreenshotRequested() || !GEngine || !GEngine->GameViewport || !CurrentWorld) { return; }
        const FString RequestState = WorldState(CurrentWorld);
        FShot Shot = Queue[0];
        Queue.RemoveAt(0);
        if (Shot.TriggerState != RequestState)
        {
            ShotEvent(TEXT("skipped_state_changed_before_request"), Shot, TEXT("Never label a later map/phase screenshot as the earlier transition."));
            ++ShotsUnconfirmed;
            return;
        }
        if (IFileManager::Get().FileExists(*Shot.File))
        {
            ShotEvent(TEXT("skipped_existing_filename"), Shot, TEXT("Existing screenshot preserved; no overwrite requested."));
            ++ShotsUnconfirmed;
            return;
        }
        Pending = MoveTemp(Shot);
        Pending.RequestState = RequestState;
        Pending.RequestedAt = Now;
        ShotEvent(TEXT("requested"), Pending, TEXT("UI included, restricted to game viewport; no desktop or other-application capture."));
        FScreenshotRequest::RequestScreenshot(Pending.File, true, false, false, FIntRect(), true);
    }
    void WriteSummary(const TCHAR* Reason)
    {
        const FString Text = FString::Printf(TEXT("observer_status=%s\nlabel=%s\nsamples=%d\ninput_frames_sampled=%d\ninput_frames_flushed=%d\ninput_frame_cap=%d\ninput_frame_cap_reached=%d\ntransitions=%d\nscreenshot_requests_queued=%d\nscreenshot_files_observed=%d\nscreenshot_unconfirmed_or_skipped=%d\nlast_map=%s\nlast_phase=%s\nevidence_io_error=%d\nphysical_input=not_assessed\nnormal_full_run=not_assessed\nNOTE=Read-only runtime observation. Combine with separately recorded external OS input and UI evidence. No input/profile selection/spawn/teleport/reward/save/gameplay state change or game exit is performed. Core-ticker wall timeout stops observation only; a stalled process cannot be sampled. Input-frame samples read game key states/just-pressed/released once per core-ticker engine frame and are not proof of OS or EnhancedInput event delivery. Input frames flush every 0.25 wall seconds or 256 rows; abrupt termination can lose the last batch. Screenshots and logging add measurement overhead.\n"),
            Reason, *Label, Samples, InputFrames, InputFramesFlushed, MaximumInputFrames, InputFrames >= MaximumInputFrames, Transitions, ShotsQueued, ShotsObserved, ShotsUnconfirmed, *LastMap, *LastPhase, bIoFailed);
        if (!FFileHelper::SaveStringToFile(Text, *(Output / TEXT("summary.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { bIoFailed = true; }
    }

    TWeakObjectPtr<UGameInstance> GameInstance;
    FTSTicker::FDelegateHandle TickerHandle;
    FDelegateHandle ExitHandle;
    TArray<FShot> Queue;
    TSet<FString> SeenStates;
    FShot Pending;
    FString Label, Output, CsvPath, ShotPath, InputPath, InputBuffer, CurrentState, PreviousState, LastMap, LastPhase;
    double Started = 0., Deadline = 0., NextSample = 0., NextInputFlush = 0.;
    float LastTickerDelta = 0.f;
    uint64 LastInputFrame = MAX_uint64;
    int32 Samples = 0, Transitions = 0, ShotsQueued = 0, ShotsObserved = 0, ShotsUnconfirmed = 0, InputFrames = 0, InputFramesFlushed = 0, BufferedInputFrames = 0;
    bool bFinished = true, bIoFailed = false;
};

static TSharedPtr<FObserver> Active;
static void Observe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
    if (!Args.IsEmpty() || (Active && !Active->IsFinished())) { Ar.Logf(TEXT("Manual observer already active, or unexpected command arguments. No change made.")); return; }
    auto Candidate = MakeShared<FObserver>();
    if (Candidate->Start(World, Ar)) { Active = MoveTemp(Candidate); }
}
static void Stop(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
    if (!Args.IsEmpty()) { Ar.Logf(TEXT("StopManualPlayObservation accepts no arguments.")); return; }
    if (Active && !Active->IsFinished()) { Active->Stop(TEXT("explicit_observer_stop_game_continues")); }
    Ar.Logf(TEXT("Manual observation stopped. No gameplay command or game exit was requested."));
}
static FAutoConsoleCommandWithWorldArgsAndOutputDevice ObserveCommand(TEXT("Rogue10m.ObserveManualPlay"),
    TEXT("Read-only fresh visible QA_Manual L_Menu observation; never drives or passes physical input."),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Observe));
static FAutoConsoleCommandWithWorldArgsAndOutputDevice StopCommand(TEXT("Rogue10m.StopManualPlayObservation"),
    TEXT("Stop only the read-only observer, preserving the running game and append-only samples."),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Stop));
}
#endif
