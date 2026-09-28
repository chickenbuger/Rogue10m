// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mInitialHeadFramingTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{ Self->Prepare(); }),4.f,false);
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (Self->World.Get()==W) { Self->Finish(false); } });
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("INITIAL_HEAD_FRAMING FAIL frame=%d %s"),FrameIndex,Message); }
    }
    void Prepare()
    {
        PC=World.IsValid() ? World->GetFirstPlayerController() : nullptr;
        Character=PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (!Character.IsValid() || !PC.IsValid()) { Check(false,TEXT("player_missing")); Finish(); return; }
        auto* Presentation=Character->GetFirstPersonPresentationComponent();
        auto* Camera=Character->GetFirstPersonCameraComponent();
        if (!Presentation || !Camera) { Check(false,TEXT("presentation_or_camera_missing")); Finish(); return; }
        // Observe the fresh spawn before the fixture writes any view angle.
        OriginalControl=PC->GetControlRotation(); OriginalWeapon=Character->GetEquippedWeaponType();
        OldCanDamage=Character->CanBeDamaged(); bPrepared=true;
        Check(Presentation->bEnableInitialFullBodyViewPitch && FMath::IsNearlyEqual(Presentation->InitialFullBodyViewPitch,-30.f,.01f),TEXT("initial_view_defaults_incorrect"));
        Check(Presentation->IsFullBodyPresentationActive() && Presentation->HasHandledInitialFullBodyViewPitch(),TEXT("fresh_initialization_not_handled"));
        Check(FMath::Abs(FRotator::NormalizeAxis(OriginalControl.Pitch)+30.f)<.1f,TEXT("fresh_spawn_control_pitch_not_minus30"));
        Check(FMath::Abs(FRotator::NormalizeAxis(Camera->GetComponentRotation().Pitch)+30.f)<.1f,TEXT("fresh_spawn_camera_pitch_not_minus30"));
        ExpectedControl=OriginalControl;
        UE_LOG(LogRogue10m,Display,TEXT("INITIAL_HEAD_FRAMING FRESH control=%s camera=%s rendered=%s"),
            *OriginalControl.ToString(),*Camera->GetComponentRotation().ToString(),PC->PlayerCameraManager ? *PC->PlayerCameraManager->GetCameraRotation().ToString() : TEXT("missing"));
        Character->SetCanBeDamaged(false); Character->GetCharacterMovement()->StopMovementImmediately();
        PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true); bOwnInputIgnore=true;
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bOwnFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/InitialHeadFraming"));
        IFileManager::Get().MakeDirectory(*Output,true);
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (Self->World.Get()==W) { Self->Frame(); } });
    }
    void Frame()
    {
        if (!Character.IsValid() || !PC.IsValid()) { Check(false,TEXT("player_lost")); Finish(); return; }
        auto* Presentation=Character->GetFirstPersonPresentationComponent();
        if (!Presentation) { Check(false,TEXT("presentation_lost")); Finish(); return; }
        if (FrameIndex==15)
        {
            // A deterministic explicit look result, equivalent to a mouse-authored
            // control orientation. Physical mouse input remains isolated from QA.
            ExpectedControl=FRotator(-12.f,FRotator::NormalizeAxis(OriginalControl.Yaw+17.f),0.f);
            PC->SetControlRotation(ExpectedControl);
            UE_LOG(LogRogue10m,Display,TEXT("INITIAL_HEAD_FRAMING EXPLICIT_LOOK expected=%s"),*ExpectedControl.ToString());
        }
        if (FrameIndex==20 || FrameIndex==60)
        {
            Presentation->ApplyInitialFullBodyViewPitchOnce(*PC.Get());
            Check(PC->GetControlRotation().Equals(ExpectedControl,.01f),TEXT("reinitialize_overwrote_explicit_look"));
        }
        if (FrameIndex==25 || FrameIndex==65)
        {
            Presentation->RefreshPresentation();
            Presentation->ApplyInitialFullBodyViewPitchOnce(*PC.Get());
            Check(PC->GetControlRotation().Equals(ExpectedControl,.01f),TEXT("refresh_overwrote_explicit_look"));
        }
        if (FrameIndex==30)
        {
            BeforeInputControl=ExpectedControl;
            PC->SetIgnoreLookInput(false);
            Check(!PC->IsLookInputIgnored(),TEXT("look_input_fixture_still_blocked"));
            PC->AddPitchInput(3.f); PC->AddYawInput(2.f);
            // This callback runs after the controller tick. Consume the accumulated
            // input through the actual virtual UpdateRotation now: restoring our
            // ignore flag first would make next tick's PostProcessInput clear it.
            PC->UpdateRotation(1.f/30.f);
            ExpectedControl=PC->GetControlRotation();
            Check(!ExpectedControl.ContainsNaN() && !ExpectedControl.Equals(BeforeInputControl,.01f),TEXT("real_rotation_input_did_not_change_view"));
            PC->SetIgnoreLookInput(true);
            UE_LOG(LogRogue10m,Display,TEXT("INITIAL_HEAD_FRAMING INPUT_CONSUMED phase=explicit_UpdateRotation_after_PostActorTick before=%s after=%s"),*BeforeInputControl.ToString(),*ExpectedControl.ToString());
        }
        if (FrameIndex==31)
        {
            const FRotator Actual=PC->GetControlRotation();
            Check(!Actual.ContainsNaN() && Actual.Equals(ExpectedControl,.01f),TEXT("next_tick_did_not_preserve_consumed_input"));
            UE_LOG(LogRogue10m,Display,TEXT("INITIAL_HEAD_FRAMING INPUT_NEXT_TICK expected=%s actual=%s"),*ExpectedControl.ToString(),*Actual.ToString());
        }
        if (FrameIndex==40) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff); }
        if (FrameIndex==43) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); }
        Check(PC->GetControlRotation().Equals(ExpectedControl,.01f),TEXT("tick_or_weapon_reset_manual_view"));
        if (FrameIndex==47)
        {
            Check(Presentation->IsFullBodyPresentationActive(),TEXT("weapon_return_lost_fullbody"));
            Check(Presentation->HasHandledInitialFullBodyViewPitch(),TEXT("weapon_return_lost_initialization_guard"));
            Check(FMath::Abs(FRotator::NormalizeAxis(Character->GetFirstPersonCameraComponent()->GetComponentRotation().Pitch-ExpectedControl.Pitch))<.1f,TEXT("camera_did_not_follow_explicit_pitch"));
        }
        if (FrameIndex==32 || FrameIndex==47 || FrameIndex==70)
        {
            const FVector CameraForward=Character->GetFirstPersonCameraComponent()->GetForwardVector();
            Check(FVector::DotProduct(CameraForward,ExpectedControl.Vector())>.99999,TEXT("gameplay_camera_forward_detached_from_control"));
        }
        if (FrameIndex<15 || FrameIndex==30 || FrameIndex==70)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false);
        }
        if (FrameIndex==90) { Finish(); return; }
        ++FrameIndex;
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; } bFinished=true;
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(PrepareTimer); }
        if (bPrepared && Character.IsValid())
        {
            Character->SetEquippedWeaponType(OriginalWeapon); Character->SetCanBeDamaged(OldCanDamage);
        }
        if (bPrepared && PC.IsValid())
        {
            PC->SetControlRotation(OriginalControl);
            if (bOwnInputIgnore) { PC->SetIgnoreLookInput(false); PC->SetIgnoreMoveInput(false); }
        }
        if (bOwnFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        const bool bPassed=Failures==0 && FrameIndex==90;
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=INITIAL_HEAD_FRAMING_%s frames=%d failures=%d"),bPassed ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures);
        if (bExit) { FPlatformMisc::RequestExitWithStatus(false,bPassed ? 0 : 1); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<APlayerController> PC;
    FTimerHandle PrepareTimer;
    FDelegateHandle Tick,Cleanup;
    FRotator OriginalControl,ExpectedControl,BeforeInputControl;
    ERogue10mWeaponType OriginalWeapon=ERogue10mWeaponType::Unarmed;
    FString Output;
    double OldDelta=1.0/30.0;
    int32 FrameIndex=0,Failures=0;
    bool OldFixed=false,OldCanDamage=true,bOwnFixed=false,bPrepared=false,bOwnInputIgnore=false,bFinished=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine || GIsEditor || (Active.IsValid() && !Active->IsFinished())) { return; }
    for (const FWorldContext& Context:GEngine->GetWorldContexts())
    {
        if (UWorld* World=Context.World();World && World->WorldType==EWorldType::Game)
        { Active=MakeShared<FRun>(); Active->Start(World); return; }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestInitialHeadFraming"),
    TEXT("Verifies fresh -30-degree framing once, preserves explicit look through updates/refresh/weapon changes; standalone editor build, 90 frames then exits."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
