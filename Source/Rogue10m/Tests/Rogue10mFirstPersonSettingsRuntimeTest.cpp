// Opt-in isolated QA: actual native UMG delegates and programmatic input, never physical-input/full-run QA.
#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/CheckBox.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Slider.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Scene.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAbandonMenuWidget.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerFeedbackComponent.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace Rogue10mFirstPersonSettingsQA
{
static FString EditableSettings(const UObject& Object)
{
    FString Result;
    for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
        FString Value;
        It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(&Object), nullptr, nullptr, PPF_None);
        Result += It->GetName() + TEXT("=") + Value + TEXT("\n");
    }
    return Result;
}
struct FSegment
{
    bool bWeighted=false;
    TArray<double> WallDeltas,WorldDeltas;
    FVector StartLocation=FVector::ZeroVector;
    double MinSpeed=std::numeric_limits<double>::max(),MaxSpeed=0.,NetDistance=0.;
    float ExpectedSprintSpeed=0.f;
    int32 GroundedSamples=0;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& InWorld)
    {
        World=&InWorld;
        const TCHAR* Args=FCommandLine::Get();
        FString ProfileSlot,Prefix,KeySlot;
        FParse::Value(Args,TEXT("RogueFirstPersonSettingsLabel="),Label);
        FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot);
        FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);
        FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
        bool bValidLabel=!Label.IsEmpty() && Label.Len()<=24;
        for (TCHAR Ch:Label) { bValidLabel &= (Ch>=TEXT('A') && Ch<=TEXT('Z')) || (Ch>=TEXT('a') && Ch<=TEXT('z')) || (Ch>=TEXT('0') && Ch<=TEXT('9')) || Ch==TEXT('_'); }
        TArray<FString> ExistingProgression;
        IFileManager::Get().FindFiles(ExistingProgression,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
        if (!bValidLabel || InWorld.GetNetMode()!=NM_Standalone
            || ProfileSlot!=TEXT("Rogue10m_Town_QA_FpUi_")+Label
            || Prefix!=TEXT("Rogue10m_Progression_QA_FpUi_")+Label
            || KeySlot!=TEXT("Rogue10m_Fighter_QA_FpUi_")+Label
            || UGameplayStatics::DoesSaveGameExist(ProfileSlot,0) || UGameplayStatics::DoesSaveGameExist(KeySlot,0) || !ExistingProgression.IsEmpty())
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_PERSON_SETTINGS_FAILED reason=exact_fresh_QA_slots_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/first-person-settings-20261029")/Label;
        Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261029_FirstPersonSettings_")+Label);
        bRendered=!FParse::Param(Args,TEXT("nullrhi"));
        if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_PERSON_SETTINGS_FAILED reason=fresh_evidence_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        IFileManager::Get().MakeDirectory(*Output,true);
        if (bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
        Frames=TEXT("frame,stage,segment,profile,weighted,active,run_phase,world_s,world_delta_s,frame_wall_delta_s,speed_cm_s,sprinting,fov,pp_priority,pp_blend,pp_motion_blur,pp_fringe,pp_override_blur,pp_override_fringe,fixture,actor_x_cm,actor_y_cm,actor_z_cm,movement_mode,max_walk_speed_cm_s,acceleration_cm_s2,blocking_ui,move_ignored,on_ground,w_key_down,shift_key_down\n");
        Metrics=TEXT("segment,profile,phase,samples,wall_median_ms,wall_p95_ms,wall_max_ms,wall_hitches_gt50ms,world_median_ms,world_p95_ms,world_max_ms,world_hitches_gt50ms,min_speed_cm_s,max_speed_cm_s,net_xy_cm,expected_sprint_speed_cm_s,grounded_samples\n");
        Heavy=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy"));
        Motion=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyMotionScale"));
        FOV=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyFOV"));
        Check(Heavy && Motion && FOV,TEXT("presentation_console_variables_registered"));
        if (Failures) { Finish(); return; }
        OldHeavy=Heavy->GetInt(); OldMotion=Motion->GetFloat(); OldFOV=FOV->GetFloat(); bOwnCVars=true;
        Check(OldHeavy==0 && OldMotion==1.f && OldFOV==0.f,TEXT("fresh_process_uses_original_first_person_and_default_controls"));
        Deadline=FPlatformTime::Seconds()+210.;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }),.02f);
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get() && !Self->bFinished) { Self->Check(false,TEXT("world_cleanup_before_completion")); Self->Finish(false); } });
        Later(3.,[this]{Prepare();});
    }
    bool Finished() const { return bFinished; }
private:
    void Check(bool bOK,const TCHAR* Why)
    { ++Checks; if (!bOK) { ++Failures; Results+=FString(TEXT("FAIL "))+Why+TEXT("\n"); if (Failures<40) { UE_LOG(LogRogue10m,Error,TEXT("FIRST_PERSON_SETTINGS FAIL %s"),Why); } } }
    void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
    void Key(FKey Value,bool bDown)
    { if (PC.IsValid()) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value,bDown?IE_Pressed:IE_Released,bDown?1.f:0.f)); } }
    void MenuKey(FKey Value)
    {
        if (PC->IsSettingsVisible() && bRendered && FSlateApplication::IsInitialized())
        {
            const FKeyEvent Event(Value,FModifierKeysState(),0,false,0,0);
            FSlateApplication::Get().ProcessKeyDownEvent(Event); FSlateApplication::Get().ProcessKeyUpEvent(Event);
        }
        else { Key(Value,true); Key(Value,false); }
    }
    void PhotoThen(const TCHAR* Name,TFunction<void()> Work)
    {
        if (bRendered)
        {
            const FString File=Images/Name;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float)
            { FScreenshotRequest::RequestScreenshot(File,true,false); return false; }),.1f);
        }
        // The captured state remains unchanged for >0.6s after the asynchronous request.
        Later(.85,MoveTemp(Work));
    }
    template<typename T> T* Control(const TCHAR* Name) const
    { return Menu.IsValid()?Cast<T>(Menu->GetWidgetFromName(FName(Name))):nullptr; }
    bool ControlsReady()
    {
        Menu=PC->GetAbandonMenuWidget();
        Check(Menu.IsValid() && Control<UCheckBox>(TEXT("WeightedFirstPersonCheck")) && Control<USlider>(TEXT("WeightedFirstPersonMotionSlider"))
            && Control<UButton>(TEXT("WeightedFirstPersonRestoreButton")),TEXT("native_settings_tree_contains_all_three_controls"));
        return Failures==0;
    }
    void Toggle(bool bEnabled)
    {
        auto* Widget=Control<UCheckBox>(TEXT("WeightedFirstPersonCheck"));
        Check(Widget && Widget->GetIsEnabled(),TEXT("actual_checkbox_is_enabled"));
        if (Widget) { Widget->SetIsChecked(bEnabled); Widget->OnCheckStateChanged.Broadcast(bEnabled); }
        Check(PC->IsWeightedFirstPersonEnabled()==bEnabled && Heavy->GetInt()==(bEnabled?1:0),TEXT("actual_checkbox_delegate_applies_requested_session_profile"));
    }
    void Slide(float Value)
    {
        auto* Widget=Control<USlider>(TEXT("WeightedFirstPersonMotionSlider"));
        Check(Widget && Widget->GetIsEnabled(),TEXT("actual_motion_slider_is_enabled"));
        if (Widget) { Widget->SetValue(Value); Widget->OnValueChanged.Broadcast(Value); }
        Check(FMath::IsNearlyEqual(PC->GetWeightedFirstPersonMotionScale(),Value,.0001f),TEXT("actual_slider_delegate_applies_session_intensity"));
    }
    void Sync(bool bEnabled,float Scale)
    {
        auto* CheckBox=Control<UCheckBox>(TEXT("WeightedFirstPersonCheck")); auto* Slider=Control<USlider>(TEXT("WeightedFirstPersonMotionSlider"));
        Check(CheckBox && Slider && CheckBox->IsChecked()==bEnabled && FMath::IsNearlyEqual(Slider->GetValue(),Scale,.0001f),TEXT("native_controls_resynchronize_with_current_session_values"));
    }
    void Prepare()
    {
        PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
        Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
        Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
        RunState=World.IsValid()?World->GetGameState<ARogue10mGameState>():nullptr;
        Check(PC.IsValid() && Profiles && Saves && RunState.IsValid(),TEXT("real_profile_progression_and_controller_available"));
        if (Failures) { Finish(); return; }
        Check(Profiles->GetProfileCount()==0,TEXT("fresh_isolated_profile_slot"));
        FRogue10mCharacterProfile Profile;
        Check(!Failures && Profiles->CreateProfile(TEXT("FpUiQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        ProfileId=Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Character=Cast<ARogue10mCharacter>(PC->GetPawn());
        Check(Character.IsValid() && Saves->IsAttachedTo(*Character),TEXT("normal_character_entry_attaches_real_pawn"));
        if (Failures) { Finish(); return; }
        Saves->DetachCharacter(*Character,false);
        Check(!Saves->IsAttachedTo(*Character),TEXT("QA_pawn_detached_before_transient_presentation_checks"));
        Appearance=Character->GetAppearanceCameraComponent(); Source=Character->GetAnimationPlaybackMesh(); Body=Character->GetMesh(); Camera=Character->GetFirstPersonCameraComponent();
        Feedback=Character->FindComponentByClass<URogue10mPlayerFeedbackComponent>();
        Check(Appearance.IsValid() && Source.IsValid() && Body.IsValid() && Source!=Body && Camera.IsValid() && Feedback.IsValid(),TEXT("production_appearance_source_camera_and_feedback_components_valid"));
        if (Failures) { Finish(); return; }
        auto* Property=FindFProperty<FObjectPropertyBase>(Feedback->GetClass(),TEXT("EffectPostProcess"));
        PostProcess=Property?Cast<UPostProcessComponent>(Property->GetObjectPropertyValue_InContainer(Feedback.Get())):nullptr;
        TArray<UPostProcessComponent*> Components; Character->GetComponents(Components);
        Check(PostProcess.IsValid() && Components.Contains(PostProcess.Get()) && PostProcess->GetOwner()==Character.Get()
            && PostProcess->Priority==50.f && PostProcess->Settings.bOverride_MotionBlurAmount && PostProcess->Settings.bOverride_SceneFringeIntensity,TEXT("reflected_player_feedback_PP_is_actual_owned_priority50_component"));
        if (Failures) { Finish(); return; }
        InitialActor=Character->GetActorTransform(); InitialControl=PC->GetControlRotation(); bInitialDamage=Character->CanBeDamaged();
        AppearanceConfig=EditableSettings(*Appearance); FeedbackConfig=EditableSettings(*Feedback);
        SourceAsset=Source->GetSkeletalMeshAsset(); BodyAsset=Body->GetSkeletalMeshAsset(); SourceClass=Source->GetAnimClass(); BodyClass=Body->GetAnimClass();
        OriginalFOV=Camera->FieldOfView; OriginalArmsFOV=Camera->FirstPersonFieldOfView; OriginalArmsScale=Camera->FirstPersonScale;
        bOriginalFirstFOV=Camera->bEnableFirstPersonFieldOfView; bOriginalFirstScale=Camera->bEnableFirstPersonScale;
        bPrepared=true; Character->SetCanBeDamaged(false);
        SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float DT)
        { if (W==Self->World.Get()) { Self->Sample(DT); } });
        Check(!PC->SetWeightedFirstPersonEnabled(true) && !PC->SetWeightedFirstPersonMotionScale(.2f) && !PC->RestoreOriginalFirstPersonPresentation()
            && Heavy->GetInt()==0 && Motion->GetFloat()==1.f && FOV->GetFloat()==0.f,TEXT("all_three_setters_reject_closed_settings_atomically"));
        GroundDeadline=FPlatformTime::Seconds()+15.; Later(.5,[this]{WaitGround();});
    }
    void WaitGround()
    {
        if (!Character->CanChangeWeapon() && FPlatformTime::Seconds()<GroundDeadline) { Later(.1,[this]{WaitGround();}); return; }
        Check(Character->CanChangeWeapon(),TEXT("grounded_idle_fixture_ready"));
        if (Failures) { Finish(); return; }
        HubGroundedActor=Character->GetActorTransform();
        MenuKey(EKeys::O); Later(.3,[this]
        {
            Check(PC->IsSettingsVisible(),TEXT("programmatic_O_opens_native_settings_without_console"));
            if (!ControlsReady()) { Finish(); return; }
            Sync(false,1.f); Stage=TEXT("ui_off");
            PhotoThen(TEXT("01_settings_off.png"),[this]{Toggle(true); Slide(1.f); Stage=TEXT("ui_on"); PhotoThen(TEXT("02_settings_on.png"),[this]{Intensity();});});
        });
    }
    void Intensity()
    {
        Slide(0.f); Sync(true,0.f); Stage=TEXT("ui_zero");
        const auto Zero=Appearance->GetWeightedFirstPersonSettings();
        Check(Zero.HeadMotionScale==0.f && Zero.HandMotionScale==0.f,TEXT("zero_intensity_disables_added_head_and_hand_motion"));
        PhotoThen(TEXT("03_settings_intensity_zero.png"),[this]
        {
            Slide(.4f); Sync(true,.4f); Stage=TEXT("ui_point4");
            PhotoThen(TEXT("04_settings_intensity_point4.png"),[this]{ClampAndReopen();});
        });
    }
    void ClampAndReopen()
    {
        Check(PC->SetWeightedFirstPersonMotionScale(-5.f) && Motion->GetFloat()==0.f,TEXT("finite_negative_intensity_clamps_to_zero"));
        Check(PC->SetWeightedFirstPersonMotionScale(5.f) && Motion->GetFloat()==1.f,TEXT("finite_excess_intensity_clamps_to_one"));
        const int32 BeforeHeavy=Heavy->GetInt(); const float BeforeMotion=Motion->GetFloat(),BeforeFOV=FOV->GetFloat();
        Check(!PC->SetWeightedFirstPersonMotionScale(std::numeric_limits<float>::quiet_NaN())
            && Heavy->GetInt()==BeforeHeavy && Motion->GetFloat()==BeforeMotion && FOV->GetFloat()==BeforeFOV,TEXT("NaN_intensity_rejected_without_any_partial_setting_change"));
        Slide(1.f); Toggle(false); Toggle(true); Slide(.4f);
        MenuKey(EKeys::Escape); Later(.3,[this]
        {
            Check(!PC->IsSettingsVisible() && !PC->IsAnyBlockingWindowVisible(),TEXT("programmatic_Escape_closes_native_settings"));
            Check(PC->IsWeightedFirstPersonEnabled() && FMath::IsNearlyEqual(Motion->GetFloat(),.4f),TEXT("closing_settings_preserves_session_values"));
            MenuKey(EKeys::F10); Later(.3,[this]
            {
                Check(PC->IsSettingsVisible(),TEXT("programmatic_F10_reopens_native_settings"));
                if (!ControlsReady()) { Finish(); return; } Sync(true,.4f); Restore();
            });
        });
    }
    void CheckAuthoredFields()
    {
        Check(AppearanceConfig==EditableSettings(*Appearance) && FeedbackConfig==EditableSettings(*Feedback),TEXT("presentation_controls_preserve_all_authored_appearance_and_feedback_fields"));
        Check(Camera->FieldOfView==OriginalFOV && Camera->FirstPersonFieldOfView==OriginalArmsFOV && Camera->FirstPersonScale==OriginalArmsScale
            && Camera->bEnableFirstPersonFieldOfView==bOriginalFirstFOV && Camera->bEnableFirstPersonScale==bOriginalFirstScale,TEXT("presentation_controls_preserve_all_original_camera_projection_fields"));
        Check(Source->GetSkeletalMeshAsset()==SourceAsset.Get() && Body->GetSkeletalMeshAsset()==BodyAsset.Get()
            && Source->GetAnimClass()==SourceClass.Get() && Body->GetAnimClass()==BodyClass.Get(),TEXT("presentation_controls_preserve_source_retarget_meshes_and_animation_classes"));
    }
    void Restore()
    {
        // Seed only the QA-owned console override; restore must reset exactly the three presentation controls.
        FOV->Set(97.f,ECVF_SetByConsole);
        Check(FOV->GetFloat()==97.f,TEXT("QA_FOV_override_seed_applied_before_restore_probe"));
        auto* Unrelated=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")); const float MaxFPS=Unrelated?Unrelated->GetFloat():0.f;
        auto* Button=Control<UButton>(TEXT("WeightedFirstPersonRestoreButton"));
        Check(Button && Button->GetIsEnabled(),TEXT("actual_restore_button_available")); if (Button) { Button->OnClicked.Broadcast(); }
        Check(Heavy->GetInt()==0 && Motion->GetFloat()==1.f && FOV->GetFloat()==0.f,TEXT("actual_restore_delegate_resets_only_three_original_presentation_controls"));
        Check(!Unrelated || Unrelated->GetFloat()==MaxFPS,TEXT("restore_preserves_unrelated_frame_cap"));
        Sync(false,1.f); CheckAuthoredFields(); Stage=TEXT("ui_restored");
        PhotoThen(TEXT("05_settings_original_restored.png"),[this]{Modal();});
    }
    void Modal()
    {
        PC->CloseAllBlockingPanels();
        // Reattach only the new QA profile for the real entry-checkpoint/confirmation guard, then detach again.
        Check(Saves->AttachCharacter(*Character) && Saves->SaveRunEntryCurrencyBaseline(),TEXT("new_QA_profile_has_real_owned_entry_checkpoint_for_modal_guard"));
        RunState->StartRunWithDuration(1200.f); PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget();
        auto* Abandon=Control<UButton>(TEXT("AbandonMenuButton"));
        Check(Abandon && Abandon->GetIsEnabled(),TEXT("actual_abandon_action_enabled_for_owned_QA_checkpoint")); if (Abandon) { Abandon->OnClicked.Broadcast(); }
        Check(Menu.IsValid() && Menu->IsConfirmationOpen() && RunState->IsRunActive(),TEXT("actual_abandon_delegate_opens_modal_without_confirming_run_end"));
        auto* CheckBox=Control<UCheckBox>(TEXT("WeightedFirstPersonCheck")); auto* Slider=Control<USlider>(TEXT("WeightedFirstPersonMotionSlider")); auto* RestoreButton=Control<UButton>(TEXT("WeightedFirstPersonRestoreButton"));
        Check(CheckBox && Slider && RestoreButton && !CheckBox->GetIsEnabled() && !Slider->GetIsEnabled() && !RestoreButton->GetIsEnabled(),TEXT("all_three_presentation_controls_disabled_behind_abandon_modal"));
        const auto* Card=Control<UBorder>(TEXT("FirstPersonPresentationCard"));
        Check(Card && Card->GetVisibility()==ESlateVisibility::Collapsed,TEXT("presentation_card_is_hidden_behind_abandon_modal"));
        const int32 H=Heavy->GetInt(); const float M=Motion->GetFloat(),F=FOV->GetFloat();
        Check(!PC->SetWeightedFirstPersonEnabled(true) && !PC->SetWeightedFirstPersonMotionScale(.1f) && !PC->RestoreOriginalFirstPersonPresentation(),TEXT("all_three_setters_reject_abandon_confirmation_context"));
        if (CheckBox) { CheckBox->OnCheckStateChanged.Broadcast(true); } if (Slider) { Slider->OnValueChanged.Broadcast(.1f); } if (RestoreButton) { RestoreButton->OnClicked.Broadcast(); }
        Check(Heavy->GetInt()==H && Motion->GetFloat()==M && FOV->GetFloat()==F && Menu->IsConfirmationOpen() && RunState->IsRunActive(),TEXT("forced_UMG_delegates_cannot_change_presentation_or_confirm_abandonment_behind_modal"));
        Stage=TEXT("modal"); PhotoThen(TEXT("06_settings_abandon_modal.png"),[this]
        {
            auto* Cancel=Control<UButton>(TEXT("AbandonCancelButton")); Check(Cancel && Cancel->GetIsEnabled(),TEXT("actual_modal_cancel_button_available")); if (Cancel) { Cancel->OnClicked.Broadcast(); }
            Check(!Menu->IsConfirmationOpen() && PC->IsSettingsVisible() && RunState->IsRunActive(),TEXT("cancel_restores_settings_without_ending_QA_run"));
            Sync(false,1.f); PC->CloseAllBlockingPanels(); Saves->DetachCharacter(*Character,false);
            Check(!Saves->IsAttachedTo(*Character),TEXT("fixture_detached_again_before_locomotion_observations"));
            if (Failures) { Finish(); return; } PrepareLocomotionFixture();
        });
    }
    void PrepareLocomotionFixture()
    {
        // Match the existing MovingAttack/MouseHead QA's transient collision floor. Only the
        // first six UI screenshots depict the unmodified Hub; the last two and ABBA use this fixture.
        Stop();
        FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
        Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
        Check(Ground.IsValid(),TEXT("QA_owned_transient_locomotion_ground_spawned"));
        if (Failures) { Finish(); return; }
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("FirstPersonSettingsQAGround"),RF_Transient);
        Ground->AddInstanceComponent(Box); Ground->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,20));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetGenerateOverlapEvents(false);
        Box->SetCanEverAffectNavigation(false); Box->RegisterComponent();
        const FVector Hub=HubGroundedActor.GetLocation(); const double Surface=Hub.Z+1000.;
        Ground->SetActorLocation(FVector(Hub.X,Hub.Y,Surface-20.));
        FTransform At=HubGroundedActor;
        At.SetLocation(FVector(Hub.X,Hub.Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.));
        Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr)); Character->SetActorTransform(At,false,nullptr,ETeleportType::TeleportPhysics);
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Character->GetCharacterMovement()->bForceNextFloorCheck=true;
        Stage=TEXT("elevated_fixture_settle"); GroundDeadline=FPlatformTime::Seconds()+8.;
        Later(.4,[this]{WaitLocomotionGround();});
    }
    void WaitLocomotionGround()
    {
        auto* Movement=Character->GetCharacterMovement();
        if (!Movement->IsMovingOnGround() && FPlatformTime::Seconds()<GroundDeadline) { Later(.1,[this]{WaitLocomotionGround();}); return; }
        Check(Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
            && Movement->CurrentFloor.HitResult.GetComponent()==Ground->GetRootComponent()
            && Character->GetMovementBaseObject()==Ground->GetRootComponent(),TEXT("actual_pawn_lands_on_its_owned_elevated_collision_floor"));
        if (Failures) { Finish(); return; }
        LocomotionActor=Character->GetActorTransform();
        Results+=FString::Printf(TEXT("locomotion_fixture=QA_owned_transient_collision_floor\nlocomotion_fixture_location=%s\nlocomotion_fixture_ground_surface_z_cm=%.6f\nui_screenshots_01_to_06=actual_hub\nsprint_screenshots_07_to_08=elevated_collision_fixture\n"),*LocomotionActor.GetLocation().ToString(),Ground->GetActorLocation().Z+20.);
        Warm(false);
    }
    void Stop()
    {
        Key(EKeys::W,false); Key(EKeys::LeftShift,false);
        if (Character.IsValid()) { Character->GetCharacterMovement()->StopMovementImmediately(); }
    }
    void ConfigureLocomotion(bool bWeighted)
    {
        Stop(); PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget(); Toggle(true); Slide(1.f); Toggle(bWeighted); PC->CloseAllBlockingPanels();
        Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr)); Character->SetActorTransform(LocomotionActor,false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(InitialControl);
        Character->GetRogueAttributeSet()->RestoreVitals();
    }
    void Warm(bool bWeighted)
    {
        ConfigureLocomotion(bWeighted); Stage=TEXT("settle_input_mode_after_menu");
        // SetInputMode schedules Slate focus/capture operations. Inject a fresh press only
        // after that transition, rather than in the same frame as closing the settings UI.
        Later(.3,[this,bWeighted]
        {
            Stage=bWeighted?TEXT("warm_on_walk"):TEXT("warm_off_walk"); Key(EKeys::W,true);
            Later(.7,[this,bWeighted]
            {
                Stage=bWeighted?TEXT("warm_on_sprint"):TEXT("warm_off_sprint"); Key(EKeys::LeftShift,true);
                Later(.8,[this,bWeighted]{Stop(); if (!bWeighted) { Warm(true); } else { BeginSegment(0); }});
            });
        });
    }
    void ValidateView(bool bWeighted)
    {
        FMinimalViewInfo View; Character->CalcCamera(0,View); FTransform Trace;
        Check(Appearance->IsAppearanceCameraActive() && !Appearance->IsThirdPersonInspectionActive() && Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("original_first_person_camera_remains_active_with_optional_weighted_profile"));
        Check(Appearance->ReadCameraTransform(Trace) && View.Location.Equals(Trace.GetLocation(),.001f)
            && View.Rotation.Equals(Trace.Rotator(),.001f) && Camera->GetComponentLocation().Equals(Trace.GetLocation(),.001f),TEXT("actual_camera_origin_and_rotation_match_combat_trace"));
        Check(FMath::IsFinite(View.FOV) && FMath::IsNearlyEqual(View.FOV,bWeighted?86.f:OriginalFOV,.01f),TEXT("weighted86_or_original_world_FOV_matches_profile"));
    }
    void BeginSegment(int32 Index)
    {
        const bool bWeighted=Index==1 || Index==2;
        ConfigureLocomotion(bWeighted); Stage=TEXT("settle_input_mode_after_menu");
        Later(.3,[this,Index,bWeighted]
        {
        Stage=TEXT("settle_walk"); MoveOrigin=Character->GetActorLocation(); Key(EKeys::W,true);
        Later(.5,[this,Index,bWeighted]
        {
            Check(FVector::Dist2D(MoveOrigin,Character->GetActorLocation())>5.f,TEXT("programmatic_W_moves_actual_pawn"));
            ValidateView(bWeighted); Key(EKeys::LeftShift,true); Stage=TEXT("settle_sprint");
            Later(.4,[this,Index,bWeighted]
            {
                Check(Character->IsSprinting(),TEXT("programmatic_shift_enters_actual_sprint")); ValidateView(bWeighted);
                Check(Character->GetVelocity().Size2D()>100.f && Character->GetCharacterMovement()->IsMovingOnGround(),TEXT("actual_sprint_is_moving_on_collision_ground_before_measurement"));
                Segments[Index].bWeighted=bWeighted; Stage=TEXT("settle_sprint_capture_hold");
                if (bRendered && (Index==0 || Index==1))
                {
                    const FString File=Images/(Index==0?TEXT("07_original_sprint.png"):TEXT("08_weighted_sprint.png"));
                    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float)
                    { FScreenshotRequest::RequestScreenshot(File,true,false); return false; }),.2f);
                }
                // Every segment has the same .85s capture hold. PNG requests/writes precede the
                // observation window; the actual captured state is held >0.6s after each request.
                Later(.85,[this,Index,bWeighted]
                {
                    Check(Character->IsSprinting() && Character->GetVelocity().Size2D()>100.f
                        && Character->GetCharacterMovement()->IsMovingOnGround(),TEXT("actual_sprint_remains_moving_after_equal_capture_hold"));
                    ValidateView(bWeighted); Segment=Index; Segments[Index].StartLocation=Character->GetActorLocation();
                    Segments[Index].ExpectedSprintSpeed=Character->GetCharacterMovement()->MaxWalkSpeed; Stage=TEXT("measured_sprint");
                    Later(1.5,[this,Index]{EndSegment(Index);});
                });
            });
        });
        });
    }
    void EndSegment(int32 Index)
    {
        Segment=INDEX_NONE;
        Check(!Segments[Index].WallDeltas.IsEmpty(),TEXT("each_warmed_ABBA_segment_contains_frame_observations"));
        Check(Character->IsSprinting(),TEXT("sprint_state_held_through_measurement")); ValidateView(Segments[Index].bWeighted);
        Check(PC->IsInputKeyDown(EKeys::W) && PC->IsInputKeyDown(EKeys::LeftShift),TEXT("simulated_W_and_shift_remain_held_through_measurement"));
        auto& Observation=Segments[Index]; Observation.NetDistance=FVector::Dist2D(Observation.StartLocation,Character->GetActorLocation());
        Check(Observation.MinSpeed>100. && Observation.MaxSpeed>=Observation.ExpectedSprintSpeed*.8f && Observation.NetDistance>100.
            && Observation.GroundedSamples==Observation.WallDeltas.Num(),TEXT("every_sprint_segment_has_real_sustained_ground_speed_and_net_displacement"));
        const auto& Settings=PostProcess->Settings;
        Check(PostProcess->Priority==50.f && PostProcess->BlendWeight==1.f && Settings.bOverride_MotionBlurAmount && Settings.bOverride_SceneFringeIntensity
            && FMath::IsNearlyEqual(Settings.MotionBlurAmount,.35f,.00001f) && FMath::IsNearlyEqual(Settings.SceneFringeIntensity,.6f,.00001f),TEXT("actual_sprint_feedback_PP_keeps_authored_priority50_blur035_fringe06"));
        if (Index==0) { SprintSettings=Settings; SprintBlend=PostProcess->BlendWeight; SprintPriority=PostProcess->Priority; }
        else { Check(FPostProcessSettings::StaticStruct()->CompareScriptStruct(&SprintSettings,&Settings,0)
            && PostProcess->BlendWeight==SprintBlend && PostProcess->Priority==SprintPriority,TEXT("complete_owned_sprint_PP_settings_are_identical_OFF_ON_ON_OFF")); }
        Results+=FString::Printf(TEXT("segment_%d_profile=%s\nsegment_%d_pp_blur=%.9f\nsegment_%d_pp_fringe=%.9f\nsegment_%d_fov_expected=%.3f\n"),Index,Segments[Index].bWeighted?TEXT("ON"):TEXT("OFF"),Index,Settings.MotionBlurAmount,Index,Settings.SceneFringeIntensity,Index,Segments[Index].bWeighted?86.f:OriginalFOV);
        Results+=FString::Printf(TEXT("segment_%d_min_speed_cm_s=%.6f\nsegment_%d_max_speed_cm_s=%.6f\nsegment_%d_net_xy_cm=%.6f\nsegment_%d_expected_sprint_speed_cm_s=%.6f\nsegment_%d_grounded_samples=%d\n"),Index,Observation.MinSpeed,Index,Observation.MaxSpeed,Index,Observation.NetDistance,Index,Observation.ExpectedSprintSpeed,Index,Observation.GroundedSamples);
        Stop(); Stage=TEXT("between_segments");
        if (Index<3) { Later(.35,[this,Index]{BeginSegment(Index+1);}); }
        else { PC->ToggleSettings(); Menu=PC->GetAbandonMenuWidget(); auto* Button=Control<UButton>(TEXT("WeightedFirstPersonRestoreButton")); if (Button) { Button->OnClicked.Broadcast(); } PC->CloseAllBlockingPanels(); Later(.7,[this]{ValidateView(false); CheckAuthoredFields(); ResultGuard();}); }
    }
    void ResultGuard()
    {
        // Synthetic result-panel boundary only: no death, settlement, abandonment or travel is requested.
        PC->ShowRunResult();
        Check(PC->IsRunResultVisible() && !PC->IsSettingsVisible(),TEXT("synthetic_result_panel_closes_settings_for_guard_probe"));
        const int32 H=Heavy->GetInt(); const float M=Motion->GetFloat(),F=FOV->GetFloat();
        Check(!PC->SetWeightedFirstPersonEnabled(true) && !PC->SetWeightedFirstPersonMotionScale(.2f) && !PC->RestoreOriginalFirstPersonPresentation(),TEXT("all_three_setters_reject_result_panel_context"));
        PC->ToggleSettings();
        Check(PC->IsRunResultVisible() && !PC->IsSettingsVisible() && Heavy->GetInt()==H && Motion->GetFloat()==M && FOV->GetFloat()==F,TEXT("result_panel_blocks_settings_reopen_without_partial_presentation_change"));
        Finish();
    }
    void Sample(float WorldDelta)
    {
        if (bFinished || !bPrepared || !Character.IsValid() || !PostProcess.IsValid()) { return; }
        const double Now=FPlatformTime::Seconds(); const double WallDelta=LastWall>0.?Now-LastWall:0.; LastWall=Now;
        FMinimalViewInfo View; Character->CalcCamera(0,View); const auto& S=PostProcess->Settings;
        const auto* Movement=Character->GetCharacterMovement(); const FVector Location=Character->GetActorLocation(); const double Speed=Character->GetVelocity().Size2D();
        if (Segment!=INDEX_NONE)
        {
            const bool bFinite=FMath::IsFinite(WallDelta) && WallDelta>=0. && FMath::IsFinite(WorldDelta) && WorldDelta>=0. && FMath::IsFinite(Speed);
            Check(bFinite,TEXT("frame_observation_deltas_are_finite_nonnegative"));
            if (bFinite)
            {
                auto& Observation=Segments[Segment]; Observation.WallDeltas.Add(WallDelta); Observation.WorldDeltas.Add(WorldDelta);
                Observation.MinSpeed=FMath::Min(Observation.MinSpeed,Speed); Observation.MaxSpeed=FMath::Max(Observation.MaxSpeed,Speed);
                Observation.GroundedSamples+=Movement->IsMovingOnGround()?1:0;
            }
        }
        Frames+=FString::Printf(TEXT("%d,%s,%d,%s,%d,%d,%d,%.9f,%.9f,%.9f,%.6f,%d,%.6f,%.6f,%.6f,%.9f,%.9f,%d,%d,%s,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%d,%d,%d,%d,%d\n"),++Frame,*Stage,Segment,*ProfileId.ToString(EGuidFormats::Digits),PC->IsWeightedFirstPersonEnabled(),Appearance->IsWeightedFirstPersonActive(),static_cast<int32>(RunState->GetRunPhase()),World->GetTimeSeconds(),WorldDelta,WallDelta,Speed,Character->IsSprinting(),View.FOV,PostProcess->Priority,PostProcess->BlendWeight,S.MotionBlurAmount,S.SceneFringeIntensity,S.bOverride_MotionBlurAmount,S.bOverride_SceneFringeIntensity,Ground.IsValid()?TEXT("elevated_collision"):TEXT("actual_hub"),Location.X,Location.Y,Location.Z,static_cast<int32>(Movement->MovementMode),Movement->MaxWalkSpeed,Movement->GetCurrentAcceleration().Size(),PC->IsAnyBlockingWindowVisible(),PC->IsMoveInputIgnored(),Movement->IsMovingOnGround(),PC->IsInputKeyDown(EKeys::W),PC->IsInputKeyDown(EKeys::LeftShift));
    }
    static FString Statistics(TArray<double> Values)
    {
        if (Values.IsEmpty()) { return TEXT("0,0,0,0"); }
        Values.Sort(); const int32 N=Values.Num(); int32 Hitches=0; for (double V:Values) { Hitches+=V>.05?1:0; }
        const double Median=(N%2)?Values[N/2]:(Values[N/2-1]+Values[N/2])*.5;
        return FString::Printf(TEXT("%.9f,%.9f,%.9f,%d"),Median*1000.,Values[FMath::Clamp(FMath::CeilToInt(.95*N)-1,0,N-1)]*1000.,Values.Last()*1000.,Hitches);
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; } bFinished=true; Next=nullptr;
        FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        Stop();
        if (bPrepared && Character.IsValid())
        {
            if (Saves && Saves->IsAttachedTo(*Character)) { Saves->DetachCharacter(*Character,false); }
            if (PC.IsValid()) { PC->CloseAllBlockingPanels(); PC->SetControlRotation(InitialControl); }
            Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr)); Character->SetCanBeDamaged(bInitialDamage); Character->SetActorTransform(InitialActor,false,nullptr,ETeleportType::TeleportPhysics);
        }
        if (Ground.IsValid()) { Ground->Destroy(); }
        if (bOwnCVars) { Heavy->Set(OldHeavy,ECVF_SetByConsole); Motion->Set(OldMotion,ECVF_SetByConsole); FOV->Set(OldFOV,ECVF_SetByConsole); }
        for (int32 I=0;I<4;++I) { const auto& O=Segments[I]; Metrics+=FString::Printf(TEXT("%d,%s,warmed_sprint,%d,%s,%s,%.6f,%.6f,%.6f,%.6f,%d\n"),I,O.bWeighted?TEXT("ON"):TEXT("OFF"),O.WallDeltas.Num(),*Statistics(O.WallDeltas),*Statistics(O.WorldDeltas),O.WallDeltas.IsEmpty()?0.:O.MinSpeed,O.MaxSpeed,O.NetDistance,O.ExpectedSprintSpeed,O.GroundedSamples); }
        Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_written"));
        Check(FFileHelper::SaveStringToFile(Metrics,*(Output/TEXT("metrics.csv"))),TEXT("observation_metrics_written"));
        Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nframes=%d\nrendered=%d\nprofile_id=%s\ninput=programmatic_InputKey_and_actual_UMG_delegates\nmenu_input=controller_InputKey_and_rendered_Slate_keys\nmanual_play=not_performed\nperformance_assertions=none\nmetrics_scope=same_process_warmed_locomotion_OFF_ON_ON_OFF_1p5s_each_on_elevated_transient_collision_floor\ncapture_hold_s=0.85_equal_all_four_segments_before_measurement\npost_menu_input_settle_s=0.3_before_fresh_W_press\nframe_wall_delta=PlatformTime_seconds_between_world_post_actor_tick_observations\nworld_delta=OnWorldPostActorTick_game_tick_seconds\nresult_guard=synthetic_result_panel_only_no_settlement_or_travel\ncold_first_attack_hitch=not_measured_use_separate_existing_rendered_attack_evidence\nNOTE=Fresh QA-owned profile; transient presentation tests detached from autosave. Modal-only checkpoint temporarily reattached then detached. No physical input, manual full run, or cold attack hitch comparison. ABBA numbers on a QA-owned elevated collision floor are observations under renderer/frame-cap/save-hash-monitor conditions, without performance causality or improvement claims. Viewport PNG readability requires visual review.\n"),Checks,Failures,Frame,bRendered,*ProfileId.ToString(EGuidFormats::Digits));
        if (!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt")))) { ++Failures; }
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIRST_PERSON_SETTINGS_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);
        if (bExit) { FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AActor> Ground;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mGameState> RunState;
    TWeakObjectPtr<URogue10mAbandonMenuWidget> Menu;
    TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;
    TWeakObjectPtr<URogue10mPlayerFeedbackComponent> Feedback;
    TWeakObjectPtr<UPostProcessComponent> PostProcess;
    TWeakObjectPtr<USkeletalMeshComponent> Source,Body;
    TWeakObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<UObject> SourceAsset,BodyAsset;
    TWeakObjectPtr<UClass> SourceClass,BodyClass;
    URogue10mCharacterProfileSubsystem* Profiles=nullptr;
    URogue10mProgressionSubsystem* Saves=nullptr;
    IConsoleVariable* Heavy=nullptr; IConsoleVariable* Motion=nullptr; IConsoleVariable* FOV=nullptr;
    FDelegateHandle SampleHandle,Cleanup;
    TFunction<void()> Next;
    FSegment Segments[4];
    FPostProcessSettings SprintSettings;
    FGuid ProfileId;
    FTransform InitialActor,HubGroundedActor,LocomotionActor;
    FRotator InitialControl;
    FVector MoveOrigin;
    FString Label,Output,Images,Frames,Metrics,Results,AppearanceConfig,FeedbackConfig,Stage=TEXT("initial");
    double NextAt=0,Deadline=0,GroundDeadline=0,LastWall=0;
    float OldMotion=1,OldFOV=0,OriginalFOV=90,OriginalArmsFOV=80,OriginalArmsScale=.6f,SprintBlend=0,SprintPriority=0;
    int32 OldHeavy=0,Checks=0,Failures=0,Frame=0,Segment=INDEX_NONE;
    bool bFinished=false,bRendered=false,bOwnCVars=false,bPrepared=false,bInitialDamage=true,bOriginalFirstFOV=false,bOriginalFirstScale=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context:GEngine->GetWorldContexts())
    { if (UWorld* W=Context.World(); W && Context.WorldType==EWorldType::Game) { Active=MakeShared<FRun>(); Active->Start(*W); return; } }
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_PERSON_SETTINGS_FAILED reason=no_standalone_game_world")); FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestFirstPersonSettings"),TEXT("Fresh FpUi QA slots only. Native settings UMG delegates, original restore, modal guards, camera/trace and sprint PP equality; warmed ABBA frame observations without performance assertions."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
