// Opt-in isolated presentation QA. Normal profile entry; no stat, HP or production-save fixture.
#if WITH_EDITOR
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunHUD.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace Rogue10mCameraCrosshairQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& InWorld)
    {
        World = &InWorld;
        const TCHAR* Args = FCommandLine::Get();
        FString ProfileSlot, Prefix, KeySlot;
        FParse::Value(Args, TEXT("RogueCameraCrosshairLabel="), Label);
        FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
        FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
        FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), KeySlot);
        FParse::Value(Args, TEXT("ResX="), Width);
        FParse::Value(Args, TEXT("ResY="), Height);
        bWeighted = FParse::Param(Args, TEXT("RogueCameraCrosshairWeighted"));
        bool bValidLabel = !Label.IsEmpty() && Label.Len() <= 24;
        for (TCHAR Ch : Label)
        {
            bValidLabel &= (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('a') && Ch <= TEXT('z'))
                || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
        }
        if (!bValidLabel || ProfileSlot != TEXT("Rogue10m_Town_QA_Crosshair_") + Label
            || Prefix != TEXT("Rogue10m_Progression_QA_Crosshair_") + Label
            || KeySlot != TEXT("Rogue10m_Fighter_QA_Crosshair_") + Label
            || InWorld.GetNetMode() != NM_Standalone || FParse::Param(Args, TEXT("nullrhi"))
            || !((Width == 1280 && Height == 720) || (Width == 1920 && Height == 1080))
            || UWorld::RemovePIEPrefix(InWorld.GetPackage()->GetName()) != TEXT("/Game/Rogue10m/Maps/StartHub"))
        {
            Reject(TEXT("exact_fresh_slots_rendered_supported_resolution_and_StartHub_required")); return;
        }
        Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/camera-crosshair-20261103"), Label);
        Images = FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLog/20261103_CameraCrosshair_") + Label);
        const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
        TArray<FString> Existing;
        IFileManager::Get().FindFiles(Existing, *(SaveDirectory / (Prefix + TEXT("_*.sav"))), true, false);
        if (IFileManager::Get().FileExists(*Output) || IFileManager::Get().DirectoryExists(*Output)
            || IFileManager::Get().FileExists(*Images) || IFileManager::Get().DirectoryExists(*Images)
            || IFileManager::Get().FileExists(*(SaveDirectory / (ProfileSlot + TEXT(".sav"))))
            || IFileManager::Get().FileExists(*(SaveDirectory / (KeySlot + TEXT(".sav")))) || !Existing.IsEmpty())
        {
            Reject(TEXT("refusing_existing_save_or_evidence_namespace")); return;
        }
        if (!IFileManager::Get().MakeDirectory(*Output, true) || !IFileManager::Get().MakeDirectory(*Images, true))
        {
            Reject(TEXT("evidence_directory_creation_failed")); return;
        }
        Heavy = IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy"));
        Check(Heavy != nullptr, TEXT("weighted_CVar_registered"));
        if (Failures) { Finish(); return; }
        OldHeavy = Heavy->GetInt(); bOwnHeavy = true;
        // Session-only, before the selected pawn exists. Never reset a consumed initialization guard.
        Heavy->Set(bWeighted ? 1 : 0, ECVF_SetByConsole);
        Observations = TEXT("screenshot,weighted,width,height,reticle_visible,control_pitch,control_yaw,camera_pitch,camera_yaw,hud_center_x_px,hud_center_y_px,forward_x_px,forward_y_px,local_width,local_height,ui_scale,trace_render_angle_deg,trace_forward_x_px,trace_forward_y_px,trace_center_error_px\n");
        Started = FPlatformTime::Seconds(); Deadline = Started + 75.0;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (!Self->World.IsValid() || FPlatformTime::Seconds() >= Self->Deadline)
            {
                Self->Check(false, TEXT("world_valid_and_wall_watchdog_not_expired")); Self->Finish(); return false;
            }
            if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt)
            {
                auto Work = MoveTemp(Self->Next); Work();
            }
            return !Self->bFinished;
        }), 0.02f);
        Later(3.0, [this] { Prepare(); });
    }
    bool Finished() const { return bFinished; }

private:
    void Reject(const TCHAR* Reason)
    {
        bFinished = true;
        UE_LOG(LogRogue10m, Error, TEXT("RESULT=CAMERA_CROSSHAIR_FAILED reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    void Check(bool bOK, const TCHAR* Reason)
    {
        ++Checks;
        Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("CAMERA_CROSSHAIR FAIL %s"), Reason); }
    }
    bool Continue()
    {
        if (Failures || !World.IsValid() || !PC.IsValid() || !Character.IsValid() || !HUD.IsValid())
        {
            if (!Failures) { Check(false, TEXT("world_player_and_visible_HUD_still_valid")); }
            Finish(); return false;
        }
        return true;
    }
    void Later(double Delay, TFunction<void()> Work)
    {
        NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work);
    }
    void IsolateLook()
    {
        if (PC.IsValid() && !PC->IsLookInputIgnored()) { PC->SetIgnoreLookInput(true); bOwnIgnoreLook = true; }
    }
    void ReleaseLookIsolation()
    {
        if (PC.IsValid() && bOwnIgnoreLook && PC->IsLookInputIgnored()) { PC->SetIgnoreLookInput(false); }
        bOwnIgnoreLook = false;
    }
    void Prepare()
    {
        auto* Instance = World->GetGameInstance();
        Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        Check(Instance && Instance->GetNumLocalPlayers() == 1 && Profiles && Saves && PC.IsValid(), TEXT("single_local_player_and_real_profile_subsystems"));
        if (Failures) { Finish(); return; }
        Check(Profiles->GetProfileCount() == 0, TEXT("isolated_profile_namespace_empty"));
        FRogue10mCharacterAppearance Appearance;
        Appearance.Race = ERogue10mCharacterRace::Human; Appearance.Gender = ERogue10mCharacterGender::Male;
        FRogue10mCharacterProfile Profile;
        Check(!Failures && Profiles->CreateProfile(TEXT("CrosshairQa"), Appearance, Profile)
            && Profiles->SelectProfile(Profile.ProfileId), TEXT("HumanMale_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        ProfileId = Profile.ProfileId;
        BeforeFixtureControl = PC->GetControlRotation();
        // StartHub's temporary default pawn may already have initialized fallback -30.
        // Establish the incoming neutral view BEFORE normal selected-profile spawn;
        // never write control rotation on the selected pawn to manufacture a passing result.
        FRotator Neutral = BeforeFixtureControl; Neutral.Pitch = 0.0; Neutral.Roll = 0.0;
        PC->SetControlRotation(Neutral);
        PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Character = Cast<ARogue10mCharacter>(PC->GetPawn());
        const FString ExpectedClass = TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C");
        Check(Character.IsValid() && Character->GetClass()->GetPathName() == ExpectedClass, TEXT("normal_profile_entry_uses_actual_selected_HumanMale_BP"));
        if (Failures) { Finish(); return; }
        auto* AppearanceCamera = Character->GetAppearanceCameraComponent();
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        Check(AppearanceCamera && AppearanceCamera->IsAppearanceCameraActive() && Presentation,
            TEXT("actual_appearance_head_camera_and_presentation_available"));
        Check(Presentation && !Presentation->HasHandledInitialFullBodyViewPitch(), TEXT("selected_pawn_initial_guard_not_consumed_by_fixture"));
        Check(Saves->IsAttachedTo(*Character) && Saves->GetAttachedProfileId() == ProfileId, TEXT("fresh_QA_profile_attached_by_normal_entry"));
        for (TObjectIterator<URogue10mRunHUD> It; It; ++It)
        {
            if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get() && It->IsInViewport() && It->IsVisible())
            {
                Check(!HUD.IsValid(), TEXT("exactly_one_active_native_RunHUD")); HUD = *It;
            }
        }
        Check(HUD.IsValid(), TEXT("actual_viewport_RunHUD_found"));
        if (!Continue()) { return; }
        IsolateLook();
        Later(1.5, [this] { Initial(); });
    }
    void Observe(const TCHAR* Name, bool bExpectedVisible)
    {
        if (!Continue()) { return; }
        const FRogue10mHudAimCrossLineView View = HUD->GetAimCrossLineView();
        Check(View.bVisible == bExpectedVisible, TEXT("reticle_visibility_matches_current_view_and_UI"));
        int32 ActualWidth = 0, ActualHeight = 0; PC->GetViewportSize(ActualWidth, ActualHeight);
        Check(ActualWidth == Width && ActualHeight == Height, TEXT("actual_viewport_matches_requested_resolution"));
        const FGeometry Geometry = HUD->GetCachedGeometry();
        const FVector2D LocalSize = Geometry.GetLocalSize();
        FVector2D HudPixel, HudViewport;
        USlateBlueprintLibrary::LocalToViewport(World.Get(), Geometry, LocalSize * 0.5, HudPixel, HudViewport);
        FVector2D ForwardPixel = FVector2D::ZeroVector;
        const APlayerCameraManager* Manager = PC->PlayerCameraManager;
        Check(Manager != nullptr, TEXT("render_camera_available"));
        if (!Manager) { return; }
        const bool bProjected = PC->ProjectWorldLocationToScreen(Manager->GetCameraLocation()
            + Manager->GetCameraRotation().Vector() * 1000.0, ForwardPixel, true);
        const FVector2D PixelCenter(ActualWidth * 0.5, ActualHeight * 0.5);
        if (bExpectedVisible)
        {
            Check(LocalSize.X > 0 && LocalSize.Y > 0 && !HudPixel.ContainsNaN()
                && FVector2D::Distance(HudPixel, PixelCenter) <= 1.0, TEXT("native_paint_geometry_center_is_viewport_pixel_center"));
            Check(bProjected && FVector2D::Distance(ForwardPixel, PixelCenter) <= 1.0, TEXT("rendered_camera_forward_projects_at_reticle_center"));
        }
        const auto* TraceCamera = Character->GetFirstPersonCameraComponent();
        const double Angle = TraceCamera ? FMath::RadiansToDegrees(TraceCamera->GetComponentQuat().AngularDistance(Manager->GetCameraRotation().Quaternion())) : -1.0;
        FVector2D TraceForwardPixel = FVector2D::ZeroVector;
        const bool bTraceProjected = TraceCamera && PC->ProjectWorldLocationToScreen(TraceCamera->GetComponentLocation()
            + TraceCamera->GetForwardVector() * 1000.0, TraceForwardPixel, true);
        const double TraceCenterError = FVector2D::Distance(TraceForwardPixel, PixelCenter);
        if (bExpectedVisible)
        {
            // ExecuteAttackHitPulse uses this actual camera component after its normal update.
            // A render-camera self-projection alone would not establish combat aim alignment.
            Check(bTraceProjected && FMath::IsFinite(Angle) && Angle >= 0.0 && Angle <= 0.35
                && TraceCenterError <= 4.0, TEXT("actual_player_attack_camera_forward_agrees_with_center_within_4px_at_rest"));
        }
        const FRotator Control = PC->GetControlRotation(), Render = Manager->GetCameraRotation();
        Observations += FString::Printf(TEXT("%s,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.3f,%.3f,%.6f\n"),
            Name, bWeighted, ActualWidth, ActualHeight, View.bVisible, FRotator::NormalizeAxis(Control.Pitch), FRotator::NormalizeAxis(Control.Yaw),
            FRotator::NormalizeAxis(Render.Pitch), FRotator::NormalizeAxis(Render.Yaw), HudPixel.X, HudPixel.Y, ForwardPixel.X, ForwardPixel.Y,
            LocalSize.X, LocalSize.Y, LocalSize.X > 0 ? ActualWidth / LocalSize.X : 0.0, Angle,
            TraceForwardPixel.X, TraceForwardPixel.Y, TraceCenterError);
    }
    void PhotoThen(const TCHAR* Name, bool bExpectedVisible, TFunction<void()> Work)
    {
        if (!Continue()) { return; }
        const FString File = Images / Name;
        // Permit Slate to paint the new UI state before request; then hold it for a full wall second.
        Later(0.25, [this, Name = FString(Name), File, bExpectedVisible, Work = MoveTemp(Work)]() mutable
        {
            Observe(*Name, bExpectedVisible);
            FScreenshotRequest::RequestScreenshot(File, true, false);
            Later(1.0, MoveTemp(Work));
        });
    }
    void Initial()
    {
        if (!Continue()) { return; }
        const auto* Appearance = Character->GetAppearanceCameraComponent();
        const auto* Presentation = Character->GetFirstPersonPresentationComponent();
        Check(Appearance && FMath::IsNearlyEqual(Appearance->InitialViewPitch, -10.0f, 0.01f), TEXT("selected_BP_effective_normal_initial_pitch_minus10"));
        Check(Appearance && FMath::IsNearlyZero(Appearance->GetWeightedFirstPersonSettings().InitialViewPitch, 0.01f), TEXT("weighted_initial_pitch_still_zero"));
        Check(Appearance && Appearance->IsWeightedFirstPersonActive() == bWeighted, TEXT("requested_presentation_active_before_initial_view"));
        Check(Presentation && Presentation->HasHandledInitialFullBodyViewPitch(), TEXT("normal_controller_tick_consumed_initial_view_once"));
        ExpectedControl = PC->GetControlRotation();
        Check(FMath::Abs(FRotator::NormalizeAxis(ExpectedControl.Pitch) - (bWeighted ? 0.0f : -10.0f)) < 0.1f,
            TEXT("fresh_selected_pawn_initial_control_pitch_correct"));
        if (!Continue()) { return; }
        PhotoThen(TEXT("01_initial_first_person.png"), true, [this] { OrdinaryLook(); });
    }
    void OrdinaryLook()
    {
        if (!Continue()) { return; }
        const FRotator Before = PC->GetControlRotation();
        ReleaseLookIsolation();
        Check(!PC->IsLookInputIgnored(), TEXT("programmatic_ordinary_look_not_blocked"));
        // Synchronous use of the same controller accumulator and virtual rotation update used by look input.
        // No SetControlRotation after selected pawn creation and no initialization-guard reset.
        PC->AddPitchInput(3.0f); PC->AddYawInput(5.0f); PC->UpdateRotation(1.0f / 60.0f);
        ExpectedControl = PC->GetControlRotation();
        // Explicit driver call occurs outside ProcessPlayerInput; prevent the same
        // accumulator from being applied again before the next normal input reset.
        PC->RotationInput = FRotator::ZeroRotator;
        Check(!ExpectedControl.ContainsNaN() && !ExpectedControl.Equals(Before, 0.01f), TEXT("ordinary_AddPitchYawInput_UpdateRotation_changes_aim"));
        IsolateLook();
        Later(0.6, [this]
        {
            if (!Continue()) { return; }
            Check(PC->GetControlRotation().Equals(ExpectedControl, 0.01f), TEXT("subsequent_normal_ticks_preserve_ordinary_look"));
            auto* Presentation = Character->GetFirstPersonPresentationComponent();
            Presentation->ApplyInitialFullBodyViewPitchOnce(*PC.Get()); Presentation->RefreshPresentation();
            Character->GetAppearanceCameraComponent()->RefreshAppearanceCamera();
            Presentation->ApplyInitialFullBodyViewPitchOnce(*PC.Get());
            Check(PC->GetControlRotation().Equals(ExpectedControl, 0.01f), TEXT("repeated_initialization_and_refresh_preserve_ordinary_look"));
            PhotoThen(TEXT("02_after_ordinary_look.png"), true, [this] { Inventory(); });
        });
    }
    void Inventory()
    {
        if (!Continue()) { return; }
        PC->ToggleInventory();
        Check(PC->IsInventoryVisible() && PC->IsAnyBlockingWindowVisible() && UGameplayStatics::IsGamePaused(World.Get()),
            TEXT("real_inventory_open_in_paused_safe_hub"));
        PhotoThen(TEXT("03_inventory_hidden_reticle.png"), false, [this]
        {
            if (!Continue()) { return; }
            PC->ToggleInventory(); IsolateLook();
            Check(!PC->IsAnyBlockingWindowVisible() && !UGameplayStatics::IsGamePaused(World.Get()), TEXT("inventory_close_restores_live_first_person"));
            Later(0.5, [this] { Inspection(); });
        });
    }
    void Inspection()
    {
        if (!Continue()) { return; }
        ReleaseLookIsolation();
        Check(!PC->IsLookInputIgnored() && !PC->IsMoveInputIgnored() && !PC->bShowMouseCursor,
            TEXT("normal_controller_inspection_input_preconditions_met"));
        PC->ToggleInspectionCamera();
        IsolateLook();
        Check(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(), TEXT("real_third_person_inspection_enabled"));
        PhotoThen(TEXT("04_inspection_hidden_reticle.png"), false, [this]
        {
            if (!Continue()) { return; }
            ReleaseLookIsolation();
            PC->ToggleInspectionCamera();
            IsolateLook();
            Check(!Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(), TEXT("inspection_returns_to_first_person"));
            Check(PC->GetControlRotation().Equals(ExpectedControl, 0.01f), TEXT("menu_and_inspection_roundtrip_preserve_ordinary_look"));
            PhotoThen(TEXT("05_returned_first_person.png"), true, [this] { Finish(); });
        });
    }
    void Finish()
    {
        if (bFinished) { return; }
        bFinished = true;
        if (PC.IsValid())
        {
            PC->CloseAllBlockingPanels();
            if (bOwnIgnoreLook) { PC->SetIgnoreLookInput(false); }
        }
        if (bOwnHeavy && Heavy) { Heavy->Set(OldHeavy, ECVF_SetByConsole); }
        Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nrendered=1\nweighted=%d\nwidth=%d\nheight=%d\nwall_seconds=%.3f\nfixture=isolated_StartHub_HumanMale_profile_neutral_control_before_selected_pawn_spawn\npre_fixture_control=%s\nlook_input=programmatic_AddPitchInput_AddYawInput_and_virtual_UpdateRotation\nphysical_input=not_performed_isolated_during_capture\nproduction_saves=never_loaded_or_modified_by_fixture\nstat_and_HP_fixtures=none\ndeath_hide=source_review_only\nvisual_pixels=external_PNG_review_required\nNOTE=Does_not_emulate_L_Menu_travel_or_claim_physical_mouse_playtest._Screenshots_are_actual_native_UMG_paint._Geometry_and_projection_checks_do_not_replace_pixel_review.\n"),
            Checks, Failures, bWeighted, Width, Height, Started > 0 ? FPlatformTime::Seconds() - Started : 0.0, *BeforeFixtureControl.ToString());
        if (!Output.IsEmpty())
        {
            if (!FFileHelper::SaveStringToFile(Observations, *(Output / TEXT("observations.csv")))) { ++Failures; }
            if (!FFileHelper::SaveStringToFile(Results, *(Output / TEXT("results.txt")))) { ++Failures; }
        }
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=CAMERA_CROSSHAIR_%s checks=%d failures=%d output=%s"),
            Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, *Output);
        FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<URogue10mRunHUD> HUD;
    URogue10mCharacterProfileSubsystem* Profiles = nullptr;
    URogue10mProgressionSubsystem* Saves = nullptr;
    IConsoleVariable* Heavy = nullptr;
    TFunction<void()> Next;
    FGuid ProfileId;
    FString Label, Output, Images, Results, Observations;
    FRotator BeforeFixtureControl = FRotator::ZeroRotator, ExpectedControl = FRotator::ZeroRotator;
    double Started = 0.0, Deadline = 0.0, NextAt = 0.0;
    int32 Width = 0, Height = 0, Checks = 0, Failures = 0, OldHeavy = 0;
    bool bFinished = false, bWeighted = false, bOwnHeavy = false, bOwnIgnoreLook = false;
};

static TSharedPtr<FRun> Active;
static void RunCommand()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
        {
            Active = MakeShared<FRun>(); Active->Start(*World); return;
        }
    }
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=CAMERA_CROSSHAIR_FAILED reason=no_game_world"));
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestCameraCrosshair"),
    TEXT("Fresh isolated selected HumanMale: initial normal/weighted view, ordinary look, actual HUD screenshots and menu/inspection visibility."),
    FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
