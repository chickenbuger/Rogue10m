// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

// A synthetic head-turn fixture, explicitly separate from the original Martelo clip.
// Only transient animation data is generated; no package or original asset is saved.
namespace Rogue10mHeadCameraTest
{
static void Execute(TWeakObjectPtr<UWorld> WeakWorld)
{
    UWorld* World = WeakWorld.Get();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    ARogue10mCharacter* Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
    auto* Arms = Character ? Character->GetFirstPersonMesh() : nullptr;
    auto* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
    auto* Presentation = Character ? Character->GetFirstPersonPresentationComponent() : nullptr;
    int32 Failures = 0;
    const auto Check = [&](bool bOK, const TCHAR* Reason)
    {
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("HEAD_CAMERA FAIL %s"), Reason); }
    };
    Check(PC && PC->PlayerCameraManager && Arms && Camera && Presentation
        && Presentation->IsFullBodyPresentationActive() && Arms->GetSkeletalMeshAsset(), TEXT("full_body_player_required"));
    if (Failures) { FPlatformMisc::RequestExitWithStatus(false, 1); return; }

    const FRotator OriginalControl = PC->GetControlRotation();
    const float OriginalMotionScale = Presentation->CameraMotionScale;
    const FRotator OriginalHeadGain = Presentation->HeadCameraRotationScale;
    const FRotator OriginalHeadLimit = Presentation->MaxHeadCameraRotation;
    const FVector OriginalCameraLocation = Camera->GetRelativeLocation();
    const EAnimationMode::Type OriginalMode = Arms->GetAnimationMode();
    TStrongObjectPtr<UClass> OriginalClass(Arms->GetAnimClass());
    TStrongObjectPtr<UAnimSequence> Sequence(NewObject<UAnimSequence>(GetTransientPackage(), NAME_None, RF_Transient));
    Sequence->SetSkeleton(Arms->GetSkeletalMeshAsset()->GetSkeleton());
    auto& Controller = Sequence->GetController();
    Controller.InitializeModel();
    Check(Sequence->CreateAnimation(Arms->GetSkeletalMeshAsset()), TEXT("transient_reference_pose_failed"));
    Controller.OpenBracket(FText::FromString(TEXT("Synthetic head-camera yaw wrap fixture")), false);
    Controller.SetFrameRate(FFrameRate(60, 1), false);
    Controller.SetNumberOfFrames(FFrameNumber(240), false);
    TArray<FVector3f> Positions, Scales;
    TArray<FQuat4f> Rotations;
    const FTransform Root = Arms->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose()[0];
    for (int32 Key = 0; Key <= 240; ++Key)
    {
        // Include a rear-facing dwell and cross +180/-180 without reversing direction.
        const float Yaw = Key < 30 ? 0.f : Key < 90 ? (Key - 30) * 3.f
            : Key < 120 ? 180.f : Key < 150 ? 180.f + (Key - 120) * (20.f / 30.f)
            : Key < 210 ? 200.f + (Key - 150) * (160.f / 60.f) : 360.f;
        Positions.Add(FVector3f(Root.GetLocation()));
        Scales.Add(FVector3f(Root.GetScale3D()));
        Rotations.Add(FQuat4f(FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * Root.GetRotation()));
    }
    Check(Controller.SetBoneTrackKeys(Arms->GetBoneName(0), Positions, Rotations, Scales, false), TEXT("transient_root_track_failed"));
    Controller.NotifyPopulated();
    Controller.CloseBracket(false);
    Sequence->bEnableRootMotion = false;
    Sequence->bForceRootLock = false;
    Arms->PlayAnimation(Sequence.Get(), false);
    Arms->SetPlayRate(0.f);
    if (auto* Single = Arms->GetSingleNodeInstance()) { Single->SetPlaying(false); }
    Check(Arms->GetSingleNodeInstance() != nullptr, TEXT("single_node_required"));

    double MaxStep = 0.0, MinRearDot = 1.0;
    int32 Samples = 0;
    if (!Failures)
    {
        for (const float Pitch : { 0.f, -70.f, 35.f })
        {
            const FRotator Input(Pitch, OriginalControl.Yaw, 0.f);
            PC->SetControlRotation(Input);
            Presentation->ResetCameraMotion();
            FQuat Previous = FQuat::Identity;
            double CaseRearDot = 1.0;
            double SignedTurn = 0.0;
            double PreviousYaw = 0.0;
            for (int32 Frame = 0; Frame <= 240; ++Frame)
            {
                Arms->SetPosition(Frame / 60.f, false);
                Arms->TickAnimation(0.f, false);
                Arms->RefreshBoneTransforms();
                PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
                const FRotator View = PC->PlayerCameraManager->GetCameraRotation();
                const FQuat Q = View.Quaternion();
                Check(!View.ContainsNaN() && !PC->PlayerCameraManager->GetCameraLocation().ContainsNaN(), TEXT("nonfinite_final_pov"));
                Check(PC->GetControlRotation().Equals(Input, .001f), TEXT("head_follow_changed_control_rotation"));
                Check(Camera->GetRelativeLocation().Equals(OriginalCameraLocation, .001f), TEXT("head_follow_moved_camera_component"));
                Check(Arms->IsBoneHiddenByName(TEXT("neck_01")), TEXT("head_visibility_changed"));
                const FVector FlatView = FVector(View.Vector().X, View.Vector().Y, 0).GetSafeNormal();
                const FVector FlatInput = FVector(Input.Vector().X, Input.Vector().Y, 0).GetSafeNormal();
                const double Dot = FVector::DotProduct(FlatView, FlatInput);
                if (Frame >= 105 && Frame <= 119) { CaseRearDot = FMath::Min(CaseRearDot, Dot); }
                const double Yaw = FRotator::NormalizeAxis(View.Yaw - Input.Yaw);
                if (Frame > 0)
                {
                    MaxStep = FMath::Max(MaxStep, FMath::RadiansToDegrees(Previous.AngularDistance(Q)));
                    SignedTurn += FMath::FindDeltaAngleDegrees(PreviousYaw, Yaw);
                }
                Previous = Q;
                PreviousYaw = Yaw;
                ++Samples;
            }
            MinRearDot = FMath::Min(MinRearDot, CaseRearDot);
            Check(CaseRearDot < -.98, TEXT("head_turn_did_not_face_rear"));
            Check(SignedTurn > 350.0 && SignedTurn < 370.0, TEXT("yaw_wrap_reversed_or_lost_full_turn"));
            Check(FMath::Abs(FRotator::NormalizeAxis(PC->PlayerCameraManager->GetCameraRotation().Yaw - Input.Yaw)) < 1.0,
                TEXT("full_turn_did_not_return_to_forward"));
            UE_LOG(LogRogue10m, Display, TEXT("HEAD_CAMERA CASE pitch=%.1f rear_dot=%.6f continuous_yaw=%.3f"), Pitch, CaseRearDot, SignedTurn);
        }
        Check(MaxStep < 9.0, TEXT("yaw_wrap_caused_camera_snap"));
        struct FBoundedCase { float Strength; float YawGain; float YawLimit; };
        const FBoundedCase BoundedCases[] = { { .5f, 1.f, 180.f }, { 1.f, .5f, 180.f }, { 1.f, 1.f, 90.f } };
        for (const FBoundedCase& Case : BoundedCases)
        {
            Presentation->CameraMotionScale = Case.Strength;
            Presentation->HeadCameraRotationScale.Yaw = Case.YawGain;
            Presentation->MaxHeadCameraRotation.Yaw = Case.YawLimit;
            PC->SetControlRotation(FRotator(0.f, OriginalControl.Yaw, 0.f));
            Presentation->ResetCameraMotion();
            Arms->PlayAnimation(Sequence.Get(), false);
            Arms->SetPlayRate(0.f);
            if (auto* Single = Arms->GetSingleNodeInstance()) { Single->SetPlaying(false); }
            FQuat Previous = FQuat::Identity;
            double CaseStep = 0.0;
            double PreviousYaw = 0.0;
            double MinimumYawStep = 0.0;
            for (int32 Frame = 0; Frame <= 240; ++Frame)
            {
                Arms->SetPosition(Frame / 60.f, false);
                Arms->TickAnimation(0.f, false);
                Arms->RefreshBoneTransforms();
                PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
                const FRotator View = PC->PlayerCameraManager->GetCameraRotation();
                const double Yaw = FRotator::NormalizeAxis(View.Yaw - OriginalControl.Yaw);
                if (Frame > 0)
                {
                    CaseStep = FMath::Max(CaseStep, FMath::RadiansToDegrees(Previous.AngularDistance(View.Quaternion())));
                    MinimumYawStep = FMath::Min(MinimumYawStep, FMath::FindDeltaAngleDegrees(PreviousYaw, Yaw));
                }
                Check(FMath::Abs(Yaw) <= Case.YawLimit * Case.Strength + .1, TEXT("reduced_head_rotation_exceeded_limit"));
                Previous = View.Quaternion();
                PreviousYaw = Yaw;
                ++Samples;
            }
            Check(CaseStep < 9.0, TEXT("reduced_gain_yaw_wrap_caused_camera_snap"));
            Check(MinimumYawStep > -.1, TEXT("reduced_gain_yaw_wrap_reversed_direction"));
            // A changed source must settle to the actual native guard; no explicit reset is used.
            Arms->SetAnimInstanceClass(OriginalClass.Get());
            Arms->SetAnimationMode(OriginalMode);
            for (int32 Frame = 0; Frame < 90; ++Frame)
            {
                Arms->TickAnimation(1.f / 60.f, false);
                Arms->RefreshBoneTransforms();
                PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
            }
            Check(Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(.1f)
                && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero(.1f), TEXT("source_change_did_not_settle_to_guard"));
            UE_LOG(LogRogue10m, Display, TEXT("HEAD_CAMERA BOUNDED strength=%.2f yaw_gain=%.2f yaw_limit=%.1f max_step=%.3f min_yaw_step=%.4f"),
                Case.Strength, Case.YawGain, Case.YawLimit, CaseStep, MinimumYawStep);
        }
        Presentation->HeadCameraRotationScale = OriginalHeadGain;
        Presentation->MaxHeadCameraRotation = OriginalHeadLimit;
        Arms->PlayAnimation(Sequence.Get(), false);
        Arms->SetPlayRate(0.f);
        if (auto* Single = Arms->GetSingleNodeInstance()) { Single->SetPlaying(false); }
        Presentation->CameraMotionScale = 0.f;
        Arms->SetPosition(1.8f, false);
        Arms->TickAnimation(0.f, false);
        Arms->RefreshBoneTransforms();
        PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
        Check(PC->PlayerCameraManager->GetCameraRotation().Equals(PC->GetControlRotation(), .01f), TEXT("zero_strength_did_not_disable_head_follow"));
        Presentation->CameraMotionScale = OriginalMotionScale;
    }
    Arms->SetAnimInstanceClass(OriginalClass.Get());
    Arms->SetAnimationMode(OriginalMode);
    Arms->TickAnimation(0.f, false);
    Arms->RefreshBoneTransforms();
    Presentation->ResetCameraMotion();
    PC->SetControlRotation(OriginalControl);
    FMinimalViewInfo RestoredView;
    Camera->GetCameraView(0.f, RestoredView);
    Check(Arms->GetAnimClass() == OriginalClass.Get() && Arms->GetAnimationMode() == OriginalMode, TEXT("animation_mode_not_restored"));
    UE_LOG(LogRogue10m, Display, TEXT("HEAD_CAMERA METRICS synthetic_only=1 samples=%d max_angular_step=%.3f min_rear_dot=%.6f"), Samples, MaxStep, MinRearDot);
    UE_LOG(LogRogue10m, Display, TEXT("RESULT=HEAD_CAMERA_%s failures=%d"), Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
    FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
}
static void Run()
{
    if (GIsEditor || !GEngine) { UE_LOG(LogRogue10m, Warning, TEXT("HEAD_CAMERA requires standalone -game, not PIE/editor.")); return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
        {
            FTimerHandle Timer;
            World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World)] { Execute(WeakWorld); }), 4.f, false);
            return;
        }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestHeadCamera"),
    TEXT("Validates head camera rear view and yaw wrap using a transient synthetic turn sequence; standalone editor build only."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
