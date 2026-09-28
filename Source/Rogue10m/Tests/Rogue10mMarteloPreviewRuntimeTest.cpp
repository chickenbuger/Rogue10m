// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mFistAnimInstance.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"

// An editor-only visual fixture. No combat binding, cooldown, damage, or saved
// character configuration is changed. The sequence plays on the actual full-body first-person mesh.
namespace Rogue10mMarteloPreview
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World = InWorld;
        OldFixed = FApp::UseFixedTimeStep();
        OldDelta = FApp::GetFixedDeltaTime();
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([Self = AsShared()](UWorld* W, bool, bool)
        { if (W == Self->World.Get()) { Self->Finish(false); } });
        FTimerHandle Timer;
        InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 4.f, false);
    }
    bool IsFinished() const { return bFinished; }

private:
    void Check(bool bOK, const TCHAR* Reason)
    {
        if (!bOK)
        {
            ++Failures;
            UE_LOG(LogRogue10m, Error, TEXT("MARTELO_PREVIEW FAIL frame=%d %s"), FrameIndex, Reason);
        }
    }
    void Prepare()
    {
        if (!World.IsValid()) { Check(false, TEXT("world_missing")); Finish(); return; }
        APlayerController* PC = World->GetFirstPlayerController();
        Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (!Character.IsValid() || !PC) { Check(false, TEXT("player_missing")); Finish(); return; }
        auto* Arms = Character->GetFirstPersonMesh();
        auto* Camera = Character->GetFirstPersonCameraComponent();
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        auto* Brawler = Character->GetBasicBrawlerComponent();
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed, TEXT("fresh_spawn_not_unarmed"));
        Check(Arms && Camera && Presentation && Presentation->IsFistPresentationActive() && Brawler, TEXT("first_person_components_missing"));
        if (Failures) { Finish(); return; }
        // Exercise the actual player-camera clamp before recording the fixture
        // baseline; temporary mode changes must not become part of that baseline.
        Check(PC->PlayerCameraManager != nullptr, TEXT("player_camera_manager_missing"));
        if (Failures) { Finish(); return; }
        const bool bOriginalFullBody = Presentation->bShowFullBody;
        FRotator FullBodyView(-89.f, PC->GetControlRotation().Yaw, 0.f);
        FRotator ViewDelta = FRotator::ZeroRotator;
        PC->PlayerCameraManager->ProcessViewRotation(0.f, FullBodyView, ViewDelta);
        Check(Presentation->IsFullBodyPresentationActive()
            && FMath::IsNearlyEqual(FRotator::NormalizeAxis(FullBodyView.Pitch), -85.f, .001f), TEXT("full_body_pitch_limit_not_85_degrees"));
        Presentation->bShowFullBody = false;
        Presentation->RefreshPresentation();
        FRotator ArmsOnlyView(-89.f, PC->GetControlRotation().Yaw, 0.f);
        ViewDelta = FRotator::ZeroRotator;
        PC->PlayerCameraManager->ProcessViewRotation(0.f, ArmsOnlyView, ViewDelta);
        Check(!Presentation->IsFullBodyPresentationActive()
            && FMath::IsNearlyEqual(FRotator::NormalizeAxis(ArmsOnlyView.Pitch), -70.f, .001f), TEXT("arms_only_pitch_limit_not_preserved"));
        Presentation->bShowFullBody = bOriginalFullBody;
        Presentation->RefreshPresentation();
        FRotator RestoredBodyView(-89.f, PC->GetControlRotation().Yaw, 0.f);
        ViewDelta = FRotator::ZeroRotator;
        PC->PlayerCameraManager->ProcessViewRotation(0.f, RestoredBodyView, ViewDelta);
        Check(Presentation->IsFullBodyPresentationActive()
            && FMath::IsNearlyEqual(FRotator::NormalizeAxis(RestoredBodyView.Pitch), -85.f, .001f), TEXT("full_body_not_restored_after_pitch_limit_check"));
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW PITCH_LIMIT full_body=%.3f arms_only=%.3f mode_restored=%d"),
            FRotator::NormalizeAxis(FullBodyView.Pitch), FRotator::NormalizeAxis(ArmsOnlyView.Pitch), Presentation->IsFullBodyPresentationActive());
        if (Failures) { Finish(); return; }
        OriginalMesh = Arms->GetSkeletalMeshAsset();
        OriginalAnimClass.Reset(Arms->GetAnimClass());
        OriginalMode = Arms->GetAnimationMode();
        OriginalArmsTransform = Arms->GetRelativeTransform();
        RestoreCameraTransform = Camera->GetRelativeTransform();
        FVector FixtureCameraLocation = RestoreCameraTransform.GetLocation();
        float CameraX = 0.f, CameraZ = 0.f;
        if (FParse::Value(FCommandLine::Get(), TEXT("FullBodyCameraX="), CameraX))
        {
            Check(FMath::IsFinite(CameraX) && FMath::Abs(CameraX) <= 40.f, TEXT("invalid_fixture_camera_x"));
            FixtureCameraLocation.X = CameraX;
            bFixtureCameraOverride = true;
        }
        if (FParse::Value(FCommandLine::Get(), TEXT("FullBodyCameraZ="), CameraZ))
        {
            Check(FMath::IsFinite(CameraZ) && CameraZ >= 40.f && CameraZ <= 100.f, TEXT("invalid_fixture_camera_z"));
            FixtureCameraLocation.Z = CameraZ;
            bFixtureCameraOverride = true;
        }
        if (Failures) { Finish(); return; }
        if (bFixtureCameraOverride) { Camera->SetRelativeLocation(FixtureCameraLocation); }
        float SpineShare = 0.f;
        if (FParse::Value(FCommandLine::Get(), TEXT("FullBodySpineShare="), SpineShare))
        {
            Check(FMath::IsFinite(SpineShare) && SpineShare >= 0.f && SpineShare <= .5f, TEXT("invalid_fixture_spine_share"));
            if (Failures) { Finish(); return; }
            OriginalSpineShare = Presentation->FullBodySpinePitchShare;
            Presentation->FullBodySpinePitchShare = SpineShare;
            bFixtureSpineOverride = true;
        }
        float AimLimit = 0.f;
        if (FParse::Value(FCommandLine::Get(), TEXT("FullBodyAimLimit="), AimLimit))
        {
            Check(FMath::IsFinite(AimLimit) && AimLimit >= 0.f && AimLimit <= 85.f, TEXT("invalid_fixture_aim_limit"));
            if (Failures) { Finish(); return; }
            OriginalAimLimit = Presentation->FullBodyAimPitchLimit;
            Presentation->FullBodyAimPitchLimit = AimLimit;
            bFixtureAimOverride = true;
        }
        bHeadCameraCapture = FParse::Param(FCommandLine::Get(), TEXT("HeadCameraCapture"));
        bCaptureUI = !FParse::Param(FCommandLine::Get(), TEXT("MarteloNoUI"));
        OriginalCameraTransform = Camera->GetRelativeTransform();
        OriginalFOV = Camera->FieldOfView;
        OriginalArmsFOV = Camera->FirstPersonFieldOfView; OriginalArmsScale = Camera->FirstPersonScale;
        bOriginalFirstPersonFOV = Camera->bEnableFirstPersonFieldOfView; bOriginalFirstPersonScale = Camera->bEnableFirstPersonScale;
        OriginalControl = PC->GetControlRotation();
        bOriginalCanBeDamaged = Character->CanBeDamaged();
        InitialSerial = Brawler->GetSnapshot().AcceptedSerial;
        Check(OriginalMesh.IsValid() && OriginalMesh->GetPathName() == TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("expected_full_body_mesh_missing"));
        Check(Presentation->IsFullBodyPresentationActive() && Arms->GetAttachParent() == Character->GetCapsuleComponent(), TEXT("full_body_capsule_attachment_missing"));
        Check(Arms->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None
            && !Camera->bEnableFirstPersonFieldOfView && !Camera->bEnableFirstPersonScale, TEXT("full_body_world_projection_not_enabled"));
        Check(Arms->IsBoneHiddenByName(TEXT("neck_01")) && !Arms->IsBoneHiddenByName(TEXT("pelvis"))
            && !Arms->IsBoneHiddenByName(TEXT("thigh_l")) && !Arms->IsBoneHiddenByName(TEXT("thigh_r"))
            && !Arms->IsBoneHiddenByName(TEXT("foot_l")) && !Arms->IsBoneHiddenByName(TEXT("foot_r")), TEXT("full_body_bone_visibility_incorrect"));
        Check(OriginalMode == EAnimationMode::AnimationBlueprint && Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()), TEXT("expected_fist_animation_missing"));
        Sequence.Reset(LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny.A_Martelo2_Manny")));
        Check(Sequence.IsValid(), TEXT("retarget_sequence_missing"));
        if (Failures) { Finish(); return; }
        Check(Sequence->GetSkeleton() == OriginalMesh->GetSkeleton(), TEXT("sequence_skeleton_does_not_match_full_body"));
        Duration = Sequence->GetPlayLength();
        Check(FMath::IsFinite(Duration) && Duration > .5f && Duration < 5.f, TEXT("invalid_sequence_duration"));
        if (Failures) { Finish(); return; }

        bDiagnosticWorldRender = FParse::Param(FCommandLine::Get(), TEXT("MarteloWorldRender"));
        bDiagnosticShowHead = FParse::Param(FCommandLine::Get(), TEXT("MarteloShowHead"));
        OriginalPrimitiveType = Arms->FirstPersonPrimitiveType;
        if (bDiagnosticWorldRender)
        {
            Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
            Camera->bEnableFirstPersonFieldOfView = false;
            Camera->bEnableFirstPersonScale = false;
        }
        if (bDiagnosticShowHead) { Arms->UnHideBoneByName(TEXT("neck_01")); }
        LookController = PC;
        PC->SetIgnoreLookInput(true);
        PC->SetIgnoreMoveInput(true);
        bOwnInputIgnore = true;
        bPrepared = true;
        Character->SetCanBeDamaged(false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        ExpectedControl = FRotator(0, OriginalControl.Yaw, 0);
        PC->SetControlRotation(ExpectedControl);
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            It->ClearAITarget();
            if (auto* AI = It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately();
            It->GetCharacterMovement()->DisableMovement();
            if (!Target.IsValid())
            {
                Target = *It;
                // Keep the stationary preview target beyond the kick arc so its head does not occlude the foot.
                It->SetActorLocation(Character->GetActorLocation() + ExpectedControl.Vector() * 210.f, false, nullptr, ETeleportType::TeleportPhysics);
                It->SetActorRotation((Character->GetActorLocation() - It->GetActorLocation()).Rotation());
            }
        }
        Check(Target.IsValid(), TEXT("preview_target_missing"));
        FastFrames = FMath::Max(2, FMath::CeilToInt(Duration * 30.f) + 1);
        SlowFrames = FMath::Max(2, FMath::CeilToInt(Duration * 60.f) + 1);
        FastStart = 90;
        FastEnd = FastStart + FastFrames;
        SlowStart = FastEnd + 30;
        SlowEnd = SlowStart + SlowFrames;
        TotalFrames = SlowEnd + (bHeadCameraCapture ? 180 : 30);
        FApp::SetFixedDeltaTime(1.0 / 30.0);
        FApp::SetUseFixedTimeStep(true);
        OutputFolder = FPaths::Combine(FPaths::ProjectSavedDir(), bHeadCameraCapture ? TEXT("Screenshots/WindowsEditor/HeadCameraPreview") : TEXT("Screenshots/WindowsEditor/MarteloFullBodyPreview"));
        IFileManager::Get().MakeDirectory(*OutputFolder, true);
        PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"));
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW ASSET sequence=%s skeleton=%s mesh=%s duration=%.6f fast=[%d,%d) slow=[%d,%d) frames=%d camera=intentional_pitch_sweep look_down=[30,80) hold_pitch=-85 source_pose_transitions=unblended"),
            *Sequence->GetPathName(), *GetPathNameSafe(Sequence->GetSkeleton()), *OriginalMesh->GetPathName(), Duration,
            FastStart, FastEnd, SlowStart, SlowEnd, TotalFrames);
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW FIXTURE camera_location=%s capture_ui=%d camera_override=%d"),
            *Camera->GetRelativeLocation().ToCompactString(), bCaptureUI, bFixtureCameraOverride);
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW DIAGNOSTIC world_render=%d show_head=%d aim_override=%d aim_limit=%.3f"),
            bDiagnosticWorldRender, bDiagnosticShowHead, bFixtureAimOverride, Presentation->FullBodyAimPitchLimit);
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self = AsShared()](UWorld* W, ELevelTick, float)
        { if (W == Self->World.Get()) { Self->Frame(); } });
    }
    void BeginSequence()
    {
        auto* Arms = Character->GetFirstPersonMesh();
        Arms->PlayAnimation(Sequence.Get(), false);
        Arms->SetPlayRate(0.f);
        if (auto* Single = Arms->GetSingleNodeInstance()) { Single->SetPlaying(false); }
        Check(Arms->GetSingleNodeInstance() != nullptr, TEXT("single_node_instance_missing"));
        bSequenceActive = true;
    }
    void RestoreAnimation(bool bEvaluatePose = true)
    {
        if (!Character.IsValid() || !bSequenceActive) { return; }
        auto* Arms = Character->GetFirstPersonMesh();
        Arms->SetAnimInstanceClass(OriginalAnimClass.Get());
        Arms->SetAnimationMode(OriginalMode);
        if (bEvaluatePose)
        {
            Arms->TickAnimation(0.f, false);
            Arms->RefreshBoneTransforms();
        }
        bSequenceActive = false;
        Check(Arms->GetAnimClass() == OriginalAnimClass.Get() && Arms->GetAnimationMode() == OriginalMode
            && Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()), TEXT("fist_animation_not_restored"));
    }
    void Frame()
    {
        if (!Character.IsValid() || !LookController.IsValid()) { Check(false, TEXT("player_lost")); Finish(); return; }
        auto* Arms = Character->GetFirstPersonMesh();
        auto* Camera = Character->GetFirstPersonCameraComponent();
        if (FrameIndex == TotalFrames) { Finish(); return; }
        // Use the same control-rotation path as mouse look. Refreshing the camera
        // through GetCameraView makes the deliberate pitch visible this frame.
        float DesiredPitch = 0.f;
        if (FrameIndex >= 30 && FrameIndex < 45) { DesiredPitch = FMath::Lerp(0.f, -85.f, (FrameIndex - 29) / 15.f); }
        else if (FrameIndex >= 45 && FrameIndex < 65) { DesiredPitch = -85.f; }
        else if (FrameIndex >= 65 && FrameIndex < 80) { DesiredPitch = FMath::Lerp(-85.f, 0.f, (FrameIndex - 64) / 15.f); }
        ExpectedControl.Pitch = DesiredPitch;
        LookController->SetControlRotation(ExpectedControl);
        FMinimalViewInfo CameraView;
        Camera->GetCameraView(0.f, CameraView);
        if (bHeadCameraCapture && FrameIndex >= SlowEnd + 30)
        {
            const int32 MoveFrame = FrameIndex - SlowEnd - 30;
            if (MoveFrame == 90) { Character->Jump(); }
            if (MoveFrame == 91) { Character->StopJumping(); }
            if (MoveFrame < 105)
            {
                // Actual movement input drives CharacterMovement and the animation's gait.
                const float Direction = (MoveFrame / 35) % 2 == 0 ? 1.f : -1.f;
                Character->AddMovementInput(ExpectedControl.RotateVector(FVector::RightVector), Direction, true);
            }
            else { Character->GetCharacterMovement()->StopMovementImmediately(); }
        }
        if (FrameIndex == 0) { FixedBodyTransform = Arms->GetComponentTransform(); }
        if (FrameIndex >= 45 && FrameIndex < 65) { ++LookDownFrames; }
        if (FrameIndex == FastStart || FrameIndex == SlowStart) { BeginSequence(); }
        if (FrameIndex == FastEnd || FrameIndex == SlowEnd) { RestoreAnimation(); }
        float SampleTime = -1.f;
        if (bSequenceActive)
        {
            const bool bSlow = FrameIndex >= SlowStart;
            const int32 LocalFrame = FrameIndex - (bSlow ? SlowStart : FastStart);
            SampleTime = FMath::Min(Duration, LocalFrame / (bSlow ? 60.f : 30.f));
            Arms->SetPosition(SampleTime, false);
            Arms->TickAnimation(0.f, false);
            Arms->RefreshBoneTransforms();
            Check(Arms->GetSingleNodeInstance() && FMath::IsNearlyEqual(Arms->GetPosition(), SampleTime, .001f), TEXT("sequence_time_not_applied"));
        }
        Check(Arms->GetSkeletalMeshAsset() == OriginalMesh.Get() && Arms->GetRelativeTransform().Equals(OriginalArmsTransform, .001f), TEXT("mesh_or_viewmodel_transform_changed"));
        Check(Arms->bOnlyOwnerSee && !Arms->bOwnerNoSee && Character->GetMesh()->bOwnerNoSee && Arms->FirstPersonPrimitiveType == (bDiagnosticWorldRender ? EFirstPersonPrimitiveType::None : OriginalPrimitiveType), TEXT("first_person_ownership_visibility_changed"));
        Check(Arms->GetAttachParent() == Character->GetCapsuleComponent() && (bHeadCameraCapture && FrameIndex >= SlowEnd + 30 || Arms->GetComponentTransform().Equals(FixedBodyTransform, .001f)), TEXT("body_followed_camera_pitch_or_moved"));
        Check(Arms->IsBoneHiddenByName(TEXT("neck_01")) == !bDiagnosticShowHead && !Arms->IsBoneHiddenByName(TEXT("pelvis"))
            && !Arms->IsBoneHiddenByName(TEXT("foot_l")) && !Arms->IsBoneHiddenByName(TEXT("foot_r")), TEXT("head_or_legs_visibility_changed"));
        Check(Camera->GetRelativeLocation().Equals(OriginalCameraTransform.GetLocation(), .001f)
            && Camera->GetRelativeScale3D().Equals(OriginalCameraTransform.GetScale3D(), .001f)
            && FMath::IsNearlyEqual(Camera->FieldOfView, OriginalFOV), TEXT("gameplay_camera_position_scale_or_fov_changed"));
        Check(Camera->GetComponentRotation().Equals(ExpectedControl, .001f), TEXT("camera_did_not_follow_deliberate_look_pitch"));
        Check(FMath::IsNearlyEqual(Camera->FirstPersonFieldOfView, OriginalArmsFOV) && FMath::IsNearlyEqual(Camera->FirstPersonScale, OriginalArmsScale)
            && Camera->bEnableFirstPersonFieldOfView == (bOriginalFirstPersonFOV && !bDiagnosticWorldRender)
            && Camera->bEnableFirstPersonScale == (bOriginalFirstPersonScale && !bDiagnosticWorldRender), TEXT("first_person_fov_or_scale_changed"));
        Check(LookController->GetControlRotation().Equals(ExpectedControl, .001f), TEXT("aim_changed"));
        Check(Character->GetBasicBrawlerComponent()->GetSnapshot().AcceptedSerial == InitialSerial, TEXT("preview_executed_combat_attack"));
        const FTransform CameraTransform = Camera->GetComponentTransform();
        const FVector Left = CameraTransform.InverseTransformPosition(Arms->GetSocketLocation(TEXT("hand_l")));
        const FVector Right = CameraTransform.InverseTransformPosition(Arms->GetSocketLocation(TEXT("hand_r")));
        for (FName Bone : {FName(TEXT("hand_l")), FName(TEXT("hand_r")), FName(TEXT("head")), FName(TEXT("foot_l")), FName(TEXT("foot_r"))})
        {
            Check(Arms->GetBoneIndex(Bone) != INDEX_NONE && !Arms->GetSocketTransform(Bone).ContainsNaN(), TEXT("invalid_bone_pose"));
        }
        const FVector LeftFoot = Arms->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation();
        const FVector RightFoot = Arms->GetSocketTransform(TEXT("foot_r"), RTS_Component).GetLocation();
        if (FrameIndex == FastStart)
        {
            StartLeft = Left; StartRight = Right;
            StartLeftFoot = LeftFoot; StartRightFoot = RightFoot;
        }
        if (bSequenceActive)
        {
            MaxLeftTravel = FMath::Max(MaxLeftTravel, FVector::Distance(Left, StartLeft));
            MaxRightTravel = FMath::Max(MaxRightTravel, FVector::Distance(Right, StartRight));
            MaxLeftFootTravel = FMath::Max(MaxLeftFootTravel, FVector::Distance(LeftFoot, StartLeftFoot));
            MaxRightFootTravel = FMath::Max(MaxRightFootTravel, FVector::Distance(RightFoot, StartRightFoot));
        }
        if (FrameIndex % 10 == 0 || FrameIndex == FastEnd - 1 || FrameIndex == SlowEnd - 1)
        {
            UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW POSE frame=%d time=%.5f look_pitch=%.3f hand_l=%s hand_r=%s head=%s foot_l=%s foot_r=%s"),
                FrameIndex, SampleTime, ExpectedControl.Pitch, *Left.ToCompactString(), *Right.ToCompactString(),
                *Arms->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation().ToCompactString(),
                *Arms->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation().ToCompactString(),
                *Arms->GetSocketTransform(TEXT("foot_r"), RTS_Component).GetLocation().ToCompactString());
        }
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        const FVector CameraOffset = Presentation->GetAppliedCameraMotionOffset();
        const FRotator CameraRotation = Presentation->GetAppliedCameraMotionRotation();
        Check(!CameraOffset.ContainsNaN() && !CameraRotation.ContainsNaN(), TEXT("head_camera_nonfinite"));
        if (bSequenceActive)
        {
            MaxHeadOffset = FMath::Max(MaxHeadOffset, CameraOffset.Size());
            MaxHeadYaw = FMath::Max(MaxHeadYaw, FMath::Abs(CameraRotation.Yaw));
            const APlayerCameraManager* Manager = LookController->PlayerCameraManager;
            const auto Delta = (Manager->GetCameraRotation() - Camera->GetComponentRotation()).GetNormalized();
            MaxRenderedHeadTurn = FMath::Max(MaxRenderedHeadTurn, FMath::Abs(Delta.Yaw));
        }
        if (bHeadCameraCapture && FrameIndex >= SlowEnd + 40 && FrameIndex < SlowEnd + 135)
        {
            if (const auto* Fist = Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()))
            {
                if (Fist->GetLocomotionPresentationWeight() > .1f) { ++GaitFrames; }
                MinGaitZ = FMath::Min(MinGaitZ, CameraOffset.Z);
                MaxGaitZ = FMath::Max(MaxGaitZ, CameraOffset.Z);
            }
        }
        if (FrameIndex % 10 == 0)
        {
            UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW HEAD_CAMERA frame=%d offset=%s rotation=%s speed=%.3f"),
                FrameIndex, *CameraOffset.ToCompactString(), *CameraRotation.ToCompactString(), Character->GetVelocity().Size2D());
        }
        const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
            ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
        Check(Size == FIntPoint(1280, 720), TEXT("capture_resolution_invalid"));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(OutputFolder, FString::Printf(TEXT("frame_%04d.png"), FrameIndex)), bCaptureUI, false);
        ++FrameIndex;
    }
    void Finish(bool bRequestProcessExit = true)
    {
        if (bFinished) { return; }
        bFinished = true;
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        RestoreAnimation(bRequestProcessExit);
        if (bPrepared && Character.IsValid())
        {
            auto* Arms = Character->GetFirstPersonMesh();
            auto* Camera = Character->GetFirstPersonCameraComponent();
            Check(Arms->GetSkeletalMeshAsset() == OriginalMesh.Get() && Arms->GetRelativeTransform().Equals(OriginalArmsTransform, .001f), TEXT("final_mesh_configuration_changed"));
            Check(Camera->GetRelativeLocation().Equals(OriginalCameraTransform.GetLocation(), .001f)
                && Camera->GetRelativeScale3D().Equals(OriginalCameraTransform.GetScale3D(), .001f)
                && FMath::IsNearlyEqual(Camera->FieldOfView, OriginalFOV), TEXT("final_camera_configuration_changed"));
            Check(Arms->GetAnimClass() == OriginalAnimClass.Get() && Arms->GetAnimationMode() == OriginalMode, TEXT("final_animation_configuration_changed"));
            Character->SetCanBeDamaged(bOriginalCanBeDamaged);
            Check(MaxLeftTravel > 5.f && MaxRightTravel > 5.f, TEXT("retarget_did_not_move_both_hands"));
            Check(MaxLeftFootTravel > 60.f && MaxLeftFootTravel > MaxRightFootTravel * 2.f, TEXT("left_kicking_leg_not_preserved"));
            Check(LookDownFrames == 20, TEXT("look_down_body_preview_missing"));
            Check(MaxHeadOffset > 2.f && MaxHeadYaw > 5.f && MaxRenderedHeadTurn > 5.f, TEXT("sequence_head_camera_not_following"));
            if (bHeadCameraCapture)
            {
                Check(GaitFrames > 30 && MaxGaitZ - MinGaitZ > .5, TEXT("moving_body_and_camera_gait_missing"));
                Check(Character->GetFirstPersonPresentationComponent()->GetAppliedCameraMotionOffset().Size() < .25,
                    TEXT("head_camera_did_not_settle_after_stop"));
            }
            Character->GetCharacterMovement()->StopMovementImmediately();
        }
        if (bOwnInputIgnore && LookController.IsValid())
        {
            LookController->SetControlRotation(OriginalControl);
            Check(LookController->GetControlRotation().Equals(OriginalControl, .001f), TEXT("original_aim_not_restored"));
            if (Character.IsValid())
            {
                FMinimalViewInfo RestoredView;
                Character->GetFirstPersonCameraComponent()->GetCameraView(0.f, RestoredView);
                Check(Character->GetFirstPersonCameraComponent()->GetComponentRotation().Equals(OriginalControl, .001f), TEXT("original_camera_rotation_not_restored"));
            }
            LookController->SetIgnoreLookInput(false);
            LookController->SetIgnoreMoveInput(false);
        }
        bOwnInputIgnore = false;
        if (bFixtureCameraOverride && Character.IsValid())
        {
            auto* Camera = Character->GetFirstPersonCameraComponent();
            Camera->SetRelativeTransform(RestoreCameraTransform);
            Check(Camera->GetRelativeTransform().Equals(RestoreCameraTransform, .001f), TEXT("fixture_camera_override_not_restored"));
        }
        bFixtureCameraOverride = false;
        if (bFixtureSpineOverride && Character.IsValid())
        {
            auto* Presentation = Character->GetFirstPersonPresentationComponent();
            Presentation->FullBodySpinePitchShare = OriginalSpineShare;
            Check(FMath::IsNearlyEqual(Presentation->FullBodySpinePitchShare, OriginalSpineShare), TEXT("fixture_spine_override_not_restored"));
        }
        bFixtureSpineOverride = false;
        if (bFixtureAimOverride && Character.IsValid())
        {
            Character->GetFirstPersonPresentationComponent()->FullBodyAimPitchLimit = OriginalAimLimit;
        }
        bFixtureAimOverride = false;
        if ((bDiagnosticWorldRender || bDiagnosticShowHead) && Character.IsValid())
        {
            auto* Arms = Character->GetFirstPersonMesh();
            auto* Camera = Character->GetFirstPersonCameraComponent();
            Arms->SetFirstPersonPrimitiveType(OriginalPrimitiveType);
            Camera->bEnableFirstPersonFieldOfView = bOriginalFirstPersonFOV;
            Camera->bEnableFirstPersonScale = bOriginalFirstPersonScale;
            if (bDiagnosticShowHead) { Arms->HideBoneByName(TEXT("neck_01"), EPhysBodyOp::PBO_None); }
            Check(Arms->FirstPersonPrimitiveType == OriginalPrimitiveType && Arms->IsBoneHiddenByName(TEXT("neck_01")), TEXT("diagnostic_render_or_head_state_not_restored"));
        }
        bDiagnosticWorldRender = false; bDiagnosticShowHead = false;
        FApp::SetUseFixedTimeStep(OldFixed);
        FApp::SetFixedDeltaTime(OldDelta);
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW METRICS hand_l_travel=%.3f hand_r_travel=%.3f foot_l_travel=%.3f foot_r_travel=%.3f duration=%.6f look_down_frames=%d body=capsule_stable"), MaxLeftTravel, MaxRightTravel, MaxLeftFootTravel, MaxRightFootTravel, Duration, LookDownFrames);
        UE_LOG(LogRogue10m, Display, TEXT("MARTELO_PREVIEW HEAD_METRICS max_offset=%.3f max_yaw=%.3f rendered_yaw=%.3f gait_frames=%d gait_z_range=%.3f"),
            MaxHeadOffset, MaxHeadYaw, MaxRenderedHeadTurn, GaitFrames, MaxGaitZ - MinGaitZ);
        const bool bPassed = Failures == 0 && bPrepared && FrameIndex == TotalFrames;
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=MARTELO_PREVIEW_%s frames=%d failures=%d"), bPassed ? TEXT("PASSED") : TEXT("FAILED"), FrameIndex, Failures);
        if (bRequestProcessExit) { FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1); }
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<APlayerController> LookController;
    TWeakObjectPtr<USkeletalMesh> OriginalMesh;
    TStrongObjectPtr<UClass> OriginalAnimClass;
    TStrongObjectPtr<UAnimSequence> Sequence;
    EAnimationMode::Type OriginalMode = EAnimationMode::AnimationBlueprint;
    FTransform OriginalArmsTransform, OriginalCameraTransform, FixedBodyTransform, RestoreCameraTransform;
    FRotator OriginalControl, ExpectedControl;
    FVector StartLeft = FVector::ZeroVector, StartRight = FVector::ZeroVector;
    FVector StartLeftFoot = FVector::ZeroVector, StartRightFoot = FVector::ZeroVector;
    FString OutputFolder;
    FDelegateHandle TickHandle, CleanupHandle;
    int32 FrameIndex = 0, Failures = 0, InitialSerial = 0, FastFrames = 0, SlowFrames = 0, LookDownFrames = 0;
    int32 FastStart = 0, FastEnd = 0, SlowStart = 0, SlowEnd = 0, TotalFrames = 0;
    float Duration = 0.f, OriginalFOV = 90.f, OriginalArmsFOV = 80.f, OriginalArmsScale = .6f;
    bool bHeadCameraCapture = false;
    double MaxHeadOffset = 0, MaxHeadYaw = 0, MaxRenderedHeadTurn = 0, MinGaitZ = 1.e6, MaxGaitZ = -1.e6;
    int32 GaitFrames = 0;
    bool bOriginalFirstPersonFOV = false, bOriginalFirstPersonScale = false;
    bool bFixtureCameraOverride = false, bFixtureSpineOverride = false, bCaptureUI = true;
    float OriginalSpineShare = .25f, OriginalAimLimit = 75.f;
    bool bFixtureAimOverride = false, bDiagnosticWorldRender = false, bDiagnosticShowHead = false;
    EFirstPersonPrimitiveType OriginalPrimitiveType = EFirstPersonPrimitiveType::None;
    double OldDelta = 1.0 / 30.0, MaxLeftTravel = 0.0, MaxRightTravel = 0.0, MaxLeftFootTravel = 0.0, MaxRightFootTravel = 0.0;
    bool OldFixed = false, bOwnInputIgnore = false, bSequenceActive = false, bPrepared = false, bFinished = false, bOriginalCanBeDamaged = true;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (GIsEditor)
    {
        UE_LOG(LogRogue10m, Warning, TEXT("MARTELO_PREVIEW requires a standalone -game process; editor and PIE are not supported."));
        return;
    }
    if (!GEngine || (Active.IsValid() && !Active->IsFinished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* W = Context.World(); W && W->WorldType == EWorldType::Game)
        {
            Active = MakeShared<FRun>();
            Active->Start(W);
            return;
        }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.PreviewMartelo"),
    TEXT("Shows the full first-person body while looking down, then Martelo at 1x and 0.5x; captures frames and restores animation and aim, then exits. Editor only."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
