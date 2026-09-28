// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "Misc/MemStack.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
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

// Standalone-only visual fixture: the retargeted source sequence directly replaces the
// procedural pose temporarily. One-to-one head-follow tuning is local to this fixture and restored.
namespace Rogue10mBoxingDirectPreview
{
bool EvaluateSourceHead(const UAnimSequence& Sequence, const FBoneContainer& Bones, float Time, FTransform& OutHead)
{
    if (!Bones.IsValid() || !FMath::IsFinite(Time) || Sequence.IsValidAdditive()) { return false; }
    const int32 MeshHead = Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("head"));
    if (MeshHead == INDEX_NONE) { return false; }
    const FCompactPoseBoneIndex Head = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshHead));
    if (Head == INDEX_NONE) { return false; }
    FMemMark Mark(FMemStack::Get());
    FCompactPose Pose; Pose.SetBoneContainer(&Bones); Pose.ResetToRefPose();
    FBlendedCurve Curve; Curve.InitFrom(Bones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Data(Pose, Curve, Attributes);
    Sequence.GetAnimationPose(Data, FAnimExtractContext(FMath::Clamp(Time, 0.f, Sequence.GetPlayLength()), false));
    FCSPose<FCompactPose> ComponentPose; ComponentPose.InitPose(Pose);
    OutHead = ComponentPose.GetComponentSpaceTransform(Head);
    return !OutHead.ContainsNaN() && OutHead.GetRotation().IsNormalized();
}
struct FMonsterState
{
    TWeakObjectPtr<ARogue10mBasicMonster> Monster;
    TWeakObjectPtr<AController> Controller;
    FTransform Transform;
    EMovementMode Mode = MOVE_Walking;
    uint8 CustomMode = 0;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World = InWorld;
        OldFixed = FApp::UseFixedTimeStep(); OldDelta = FApp::GetFixedDeltaTime();
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([Self = AsShared()](UWorld* W, bool, bool)
        { if (W == Self->World.Get()) { Self->Finish(false); } });
        InWorld->GetTimerManager().SetTimer(PrepareTimer,
            FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 4.f, false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bOK, const TCHAR* Reason)
    {
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("BOXING_DIRECT FAIL frame=%d %s"), FrameIndex, Reason); }
    }
    void Prepare()
    {
        if (!World.IsValid()) { Check(false, TEXT("world_missing")); Finish(); return; }
        PC = World->GetFirstPlayerController();
        Character = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (!Character.IsValid() || !PC.IsValid()) { Check(false, TEXT("player_missing")); Finish(); return; }
        auto* Mesh = Character->GetFirstPersonMesh();
        auto* Camera = Character->GetFirstPersonCameraComponent();
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        Check(Mesh && Camera && Presentation && Character->GetBasicBrawlerComponent()
            && PC->PlayerCameraManager && Presentation->IsFullBodyPresentationActive(), TEXT("full_body_components_missing"));
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed, TEXT("fresh_player_not_unarmed"));
        if (Failures) { Finish(); return; }
        OriginalMesh = Mesh->GetSkeletalMeshAsset(); OriginalClass.Reset(Mesh->GetAnimClass());
        OriginalMode = Mesh->GetAnimationMode(); MeshTransform = Mesh->GetRelativeTransform();
        CameraTransform = Camera->GetRelativeTransform(); ExpectedCameraLocation = CameraTransform.GetLocation(); OriginalControl = PC->GetControlRotation();
        OriginalFOV = Camera->FieldOfView; bOldCanBeDamaged = Character->CanBeDamaged();
        InitialSerial = Character->GetBasicBrawlerComponent()->GetSnapshot().AcceptedSerial;
        Check(OriginalMesh.IsValid() && Cast<URogue10mFistAnimInstance>(Mesh->GetAnimInstance())
            && OriginalMode == EAnimationMode::AnimationBlueprint, TEXT("native_guard_missing"));
        Check(Mesh->GetAttachParent() == Character->GetCapsuleComponent()
            && Mesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None
            && !Camera->bEnableFirstPersonFieldOfView && !Camera->bEnableFirstPersonScale, TEXT("full_body_projection_incorrect"));
        Sequence.Reset(LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Rogue10m/Animation/Preview/Boxing/A_Boxing_Manny.A_Boxing_Manny")));
        Check(Sequence.IsValid(), TEXT("direct_retarget_sequence_missing"));
        if (Failures) { Finish(); return; }
        Check(Sequence->GetSkeleton() == OriginalMesh->GetSkeleton(), TEXT("retarget_skeleton_mismatch"));
        Duration = Sequence->GetPlayLength();
        Check(FMath::IsFinite(Duration) && FMath::Abs(Duration - 52.f / 30.f) < .02f, TEXT("active_fbx_stack_duration_incorrect"));
        if (Failures) { Finish(); return; }
        bLookDownPreview = FParse::Param(FCommandLine::Get(), TEXT("RogueBoxingLookDown"));
        bForwardViewPreview = FParse::Param(FCommandLine::Get(), TEXT("RogueBoxingForwardView"));
        ExpectedControl = FRotator(bForwardViewPreview ? -30.f : bLookDownPreview ? -40.f : 0.f, OriginalControl.Yaw, 0.f);
        PC->SetControlRotation(ExpectedControl); PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true);
        bOwnInputIgnore = true; bPrepared = true;
        OldHeadRotationScale = Presentation->HeadCameraRotationScale; OldHeadLimits = Presentation->MaxHeadCameraRotation;
        OldHeadTranslationScale = Presentation->HeadCameraTranslationScale; OldHeadSpeed = Presentation->HeadCameraFollowSpeed;
        OldHeadOffsetLimit = Presentation->MaxHeadCameraOffset; OldMotionScale = Presentation->CameraMotionScale;
        bOldHeadFollow = Presentation->bEnableHeadCameraFollow;
        Presentation->bEnableHeadCameraFollow = true; Presentation->CameraMotionScale = 1.f;
        Presentation->HeadCameraRotationScale = FRotator(1.f, 1.f, 1.f); Presentation->HeadCameraTranslationScale = 1.f;
        Presentation->HeadCameraFollowSpeed = 0.f; Presentation->MaxHeadCameraOffset = 80.f;
        Presentation->MaxHeadCameraRotation = FRotator(85.f, 180.f, 60.f); Presentation->ResetCameraMotion();
        bHeadOverride = true;
        Character->SetCanBeDamaged(false); Character->GetCharacterMovement()->StopMovementImmediately();
        PlayerTransform = Character->GetActorTransform();
        bool bPlacedTarget = false;
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            FMonsterState& Saved = Monsters.AddDefaulted_GetRef();
            Saved.Monster = *It; Saved.Controller = It->GetController(); Saved.Transform = It->GetActorTransform();
            Saved.Mode = It->GetCharacterMovement()->MovementMode; Saved.CustomMode = It->GetCharacterMovement()->CustomMovementMode;
            if (Saved.Controller.IsValid()) { Saved.Controller->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
            if (!bPlacedTarget)
            {
                It->SetActorLocation(Character->GetActorLocation() + FRotator(0.f, ExpectedControl.Yaw, 0.f).Vector() * 240.f, false, nullptr, ETeleportType::TeleportPhysics);
                It->SetActorRotation((Character->GetActorLocation() - It->GetActorLocation()).Rotation()); bPlacedTarget = true;
            }
        }
        Check(bPlacedTarget, TEXT("preview_target_missing"));
        FastStart = 30; FastEnd = FastStart + FMath::RoundToInt(Duration * 30.f) + 1;
        SlowStart = FastEnd + 30; SlowEnd = SlowStart + FMath::RoundToInt(Duration * 60.f) + 1;
        NativeStart = SlowEnd + 30; TotalFrames = NativeStart + 30;
        FApp::SetFixedDeltaTime(1.0 / 30.0); FApp::SetUseFixedTimeStep(true);
        OutputFolder = FPaths::Combine(FPaths::ProjectSavedDir(), bForwardViewPreview ? TEXT("Screenshots/WindowsEditor/BoxingDirectPreviewForward30")
            : bLookDownPreview ? TEXT("Screenshots/WindowsEditor/BoxingDirectPreviewLookDown40") : TEXT("Screenshots/WindowsEditor/BoxingDirectPreview"));
        IFileManager::Get().MakeDirectory(*OutputFolder, true);
        PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"));
        Mesh->PlayAnimation(Sequence.Get(), false); Mesh->SetPlayRate(0.f);
        if (auto* Single = Mesh->GetSingleNodeInstance()) { Single->SetPlaying(false); }
        bSequenceActive = true;
        Mesh->SetPosition(0.f, false); Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
        Check(Mesh->GetSingleNodeInstance() && EvaluateSourceHead(*Sequence.Get(), Mesh->GetSingleNodeInstance()->GetRequiredBones(), 0.f, ReferenceHead),
            TEXT("unhidden_source_reference_head_missing"));
        if (Failures) { Finish(); return; }
        // Calibrate the fixture eye from the actual source guard, not the taller procedural guard.
        ExpectedCameraLocation = MeshTransform.TransformPosition(ReferenceHead.GetLocation()) + EyeLever;
        Camera->SetRelativeLocation(ExpectedCameraLocation);
        Presentation->ResetCameraMotion();
        UE_LOG(LogRogue10m, Display, TEXT("BOXING_DIRECT EYE reference_head=%s camera_body=%s eye_lever=%s"),
            *ReferenceHead.GetLocation().ToCompactString(), *ExpectedCameraLocation.ToCompactString(), *EyeLever.ToCompactString());
        PC->PlayerCameraManager->UpdateCamera(1.f / 30.f);
        Check(Mesh->GetSingleNodeInstance() != nullptr, TEXT("single_node_missing"));
        UE_LOG(LogRogue10m, Display, TEXT("BOXING_DIRECT ASSET sequence=%s duration=%.6f fast=[%d,%d) slow=[%d,%d) frames=%d view=%s camera=unfiltered_head_follow gain=1 translation=1 limits=85/180/60 offset_limit=80 end_hold=30 native_guard=30 transition=unblended source=retargeted_original"),
            *Sequence->GetPathName(), Duration, FastStart, FastEnd, SlowStart, SlowEnd, TotalFrames, bForwardViewPreview ? TEXT("forward_30_no_ui") : bLookDownPreview ? TEXT("look_down_40_no_ui") : TEXT("forward"));
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self = AsShared()](UWorld* W, ELevelTick, float)
        { if (W == Self->World.Get()) { Self->Frame(); } });
    }
    void RestoreAnimation(bool bEvaluate)
    {
        if (!bSequenceActive || !Character.IsValid()) { return; }
        auto* Mesh = Character->GetFirstPersonMesh();
        Mesh->SetAnimInstanceClass(OriginalClass.Get()); Mesh->SetAnimationMode(OriginalMode);
        if (bEvaluate) { Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms(); }
        bSequenceActive = false;
        Check(Mesh->GetAnimClass() == OriginalClass.Get() && Mesh->GetAnimationMode() == OriginalMode
            && Cast<URogue10mFistAnimInstance>(Mesh->GetAnimInstance()), TEXT("native_guard_not_restored"));
    }
    void Frame()
    {
        if (!Character.IsValid() || !PC.IsValid()) { Check(false, TEXT("player_lost")); Finish(); return; }
        if (FrameIndex >= TotalFrames) { Finish(); return; }
        auto* Mesh = Character->GetFirstPersonMesh(); auto* Camera = Character->GetFirstPersonCameraComponent();
        if (FrameIndex == NativeStart) { RestoreAnimation(true); RestoreHeadCamera(); }
        float SampleTime = -1.f;
        if (bSequenceActive)
        {
            SampleTime = 0.f;
            if (FrameIndex >= FastStart && FrameIndex < FastEnd) { SampleTime = FMath::Min(Duration, (FrameIndex - FastStart) / 30.f); }
            if (FrameIndex >= SlowStart && FrameIndex < SlowEnd) { SampleTime = FMath::Min(Duration, (FrameIndex - SlowStart) / 60.f); }
            if (FrameIndex >= SlowEnd) { SampleTime = Duration; }
            Mesh->SetPosition(SampleTime, false); Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
            Check(Mesh->GetSingleNodeInstance() && FMath::IsNearlyEqual(Mesh->GetPosition(), SampleTime, .001f), TEXT("source_time_not_applied"));
        }
        // Evaluate final POV after the exact source sample, avoiding the normal one-frame
        // post-tick sampling lag. Speed zero makes the extra update independent of smoothing.
        PC->PlayerCameraManager->UpdateCamera(1.f / 30.f);
        Check(Mesh->GetSkeletalMeshAsset() == OriginalMesh.Get() && Mesh->GetRelativeTransform().Equals(MeshTransform, .001f), TEXT("mesh_configuration_changed"));
        Check(Mesh->bOnlyOwnerSee && !Mesh->bOwnerNoSee && Character->GetMesh()->bOwnerNoSee
            && Mesh->IsBoneHiddenByName(TEXT("neck_01")) && !Mesh->IsBoneHiddenByName(TEXT("pelvis"))
            && !Mesh->IsBoneHiddenByName(TEXT("foot_l")) && !Mesh->IsBoneHiddenByName(TEXT("foot_r")), TEXT("full_body_visibility_changed"));
        Check(Character->GetActorTransform().Equals(PlayerTransform, .01f), TEXT("source_motion_moved_gameplay_capsule"));
        Check(Camera->GetRelativeLocation().Equals(ExpectedCameraLocation, .001f)
            && FMath::IsNearlyEqual(Camera->FieldOfView, OriginalFOV), TEXT("gameplay_camera_configuration_changed"));
        Check(PC->GetControlRotation().Equals(ExpectedControl, .001f), TEXT("source_motion_changed_control_rotation"));
        Check(Character->GetBasicBrawlerComponent()->GetSnapshot().AcceptedSerial == InitialSerial, TEXT("source_preview_triggered_attack"));
        for (const FName Bone : {FName(TEXT("hand_l")), FName(TEXT("hand_r")), FName(TEXT("foot_l")), FName(TEXT("foot_r")), FName(TEXT("pelvis"))})
        { Check(Mesh->GetBoneIndex(Bone) != INDEX_NONE && !Mesh->GetSocketTransform(Bone).ContainsNaN(), TEXT("nonfinite_or_missing_bone")); }
        const auto BonePosition = [Mesh](const TCHAR* Name) { return Mesh->GetSocketTransform(FName(Name), RTS_Component).GetLocation(); };
        const FVector Left = BonePosition(TEXT("hand_l")), Right = BonePosition(TEXT("hand_r"));
        const FVector LeftFoot = BonePosition(TEXT("foot_l")), RightFoot = BonePosition(TEXT("foot_r"));
        const FVector LeftReach = Left - BonePosition(TEXT("upperarm_l"));
        const FVector RightReach = Right - BonePosition(TEXT("upperarm_r"));
        if (FrameIndex == 0)
        { StartLeft = Left; StartRight = Right; StartLeftFoot = LeftFoot; StartRightFoot = RightFoot; StartLeftReach = LeftReach; StartRightReach = RightReach; }
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        const FVector Offset = Presentation->GetAppliedCameraMotionOffset(); const FRotator Rotation = Presentation->GetAppliedCameraMotionRotation();
        Check(!Offset.ContainsNaN() && !Rotation.ContainsNaN(), TEXT("head_camera_nonfinite"));
        if (bSequenceActive)
        {
            FTransform CurrentHead;
            Check(EvaluateSourceHead(*Sequence.Get(), Mesh->GetSingleNodeInstance()->GetRequiredBones(), SampleTime, CurrentHead), TEXT("source_head_evaluation_failed"));
            const FQuat MeshToBody = MeshTransform.GetRotation();
            const FQuat SourceDelta = (MeshToBody * (CurrentHead.GetRotation() * ReferenceHead.GetRotation().Inverse()) * MeshToBody.Inverse()).GetNormalized();
            const FVector ExpectedOffset = MeshToBody.RotateVector(CurrentHead.GetLocation() - ReferenceHead.GetLocation())
                + SourceDelta.RotateVector(EyeLever) - EyeLever;
            const double OffsetError = FVector::Distance(Offset, ExpectedOffset);
            const double RotationError = FMath::RadiansToDegrees(Rotation.Quaternion().AngularDistance(SourceDelta));
            MaxHeadOffsetError = FMath::Max(MaxHeadOffsetError, OffsetError);
            MaxHeadRotationError = FMath::Max(MaxHeadRotationError, RotationError);
            Check(OffsetError < .05 && RotationError < .05, TEXT("head_camera_does_not_match_raw_source_sample"));
            MaxLeftTravel = FMath::Max(MaxLeftTravel, FVector::Distance(Left, StartLeft));
            MaxRightTravel = FMath::Max(MaxRightTravel, FVector::Distance(Right, StartRight));
            MaxLeftFoot = FMath::Max(MaxLeftFoot, FVector::Distance(LeftFoot, StartLeftFoot));
            MaxRightFoot = FMath::Max(MaxRightFoot, FVector::Distance(RightFoot, StartRightFoot));
            MaxLeftReach = FMath::Max(MaxLeftReach, FVector::Distance(LeftReach, StartLeftReach));
            MaxRightReach = FMath::Max(MaxRightReach, FVector::Distance(RightReach, StartRightReach));
            MaxHeadOffset = FMath::Max(MaxHeadOffset, Offset.Size()); MaxHeadYaw = FMath::Max(MaxHeadYaw, FMath::Abs(Rotation.Yaw));
            MaxHeadPitch = FMath::Max(MaxHeadPitch, FMath::Abs(Rotation.Pitch));
            MaxHeadRoll = FMath::Max(MaxHeadRoll, FMath::Abs(Rotation.Roll));
            if (Offset.Size() >= 79.99 || FMath::Abs(Rotation.Pitch) >= 84.99 || FMath::Abs(Rotation.Roll) >= 59.99) { ++CameraLimitFrames; }
            const FRotator Rendered = (PC->PlayerCameraManager->GetCameraRotation() - Camera->GetComponentRotation()).GetNormalized();
            MaxRenderedRotation = FMath::Max(MaxRenderedRotation, FMath::Max(FMath::Abs(Rendered.Yaw), FMath::Abs(Rendered.Pitch)));
        }
        if (FrameIndex % 10 == 0 || FrameIndex == FastEnd - 1 || FrameIndex == SlowEnd - 1)
        {
            UE_LOG(LogRogue10m, Display, TEXT("BOXING_DIRECT POSE frame=%d time=%.5f left=%s right=%s left_foot=%s right_foot=%s camera_offset=%s camera_rotation=%s"),
                FrameIndex, SampleTime, *Left.ToCompactString(), *Right.ToCompactString(), *LeftFoot.ToCompactString(), *RightFoot.ToCompactString(),
                *Offset.ToCompactString(), *Rotation.ToCompactString());
        }
        const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
            ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
        Check(Size == FIntPoint(1280, 720), TEXT("capture_resolution_invalid"));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(OutputFolder, FString::Printf(TEXT("frame_%04d.png"), FrameIndex)), !(bLookDownPreview || bForwardViewPreview), false);
        ++FrameIndex;
    }
    void RestoreHeadCamera()
    {
        if (!bHeadOverride || !Character.IsValid()) { return; }
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        auto* Camera = Character->GetFirstPersonCameraComponent();
        Camera->SetRelativeLocation(CameraTransform.GetLocation()); ExpectedCameraLocation = CameraTransform.GetLocation();
        Presentation->HeadCameraRotationScale = OldHeadRotationScale; Presentation->MaxHeadCameraRotation = OldHeadLimits;
        Presentation->HeadCameraTranslationScale = OldHeadTranslationScale; Presentation->HeadCameraFollowSpeed = OldHeadSpeed;
        Presentation->MaxHeadCameraOffset = OldHeadOffsetLimit; Presentation->CameraMotionScale = OldMotionScale;
        Presentation->bEnableHeadCameraFollow = bOldHeadFollow; bHeadOverride = false;
        Check(Camera->GetRelativeLocation().Equals(CameraTransform.GetLocation(), .001f)
            && Presentation->HeadCameraRotationScale.Equals(OldHeadRotationScale)
            && Presentation->MaxHeadCameraRotation.Equals(OldHeadLimits)
            && FMath::IsNearlyEqual(Presentation->HeadCameraTranslationScale, OldHeadTranslationScale)
            && FMath::IsNearlyEqual(Presentation->HeadCameraFollowSpeed, OldHeadSpeed)
            && FMath::IsNearlyEqual(Presentation->MaxHeadCameraOffset, OldHeadOffsetLimit)
            && FMath::IsNearlyEqual(Presentation->CameraMotionScale, OldMotionScale)
            && Presentation->bEnableHeadCameraFollow == bOldHeadFollow, TEXT("head_tuning_or_eye_not_restored"));
        Presentation->ResetCameraMotion();
    }
    void Finish(bool bRequestExit = true)
    {
        if (bFinished) { return; } bFinished = true;
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle); FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(PrepareTimer); }
        RestoreAnimation(bRequestExit);
        if (bPrepared && Character.IsValid())
        {
            Character->SetCanBeDamaged(bOldCanBeDamaged);
            Check(MaxLeftTravel > 20.f && MaxLeftFoot > 10.f, TEXT("source_step_and_left_hand_not_visible"));
            Check(MaxLeftReach > 12.f && MaxLeftReach > MaxRightReach * 1.2, TEXT("left_strike_dominance_not_preserved"));
            Check(MaxHeadOffset > 2.f && MaxRenderedRotation > 1.f, TEXT("source_head_camera_not_following"));
            Check(!bSequenceActive && Character->GetFirstPersonMesh()->GetAnimClass() == OriginalClass.Get(), TEXT("final_guard_missing"));
            Check(CameraLimitFrames == 0, TEXT("direct_head_camera_clipped_by_safety_limit"));
            RestoreHeadCamera();
        }
        if (bOwnInputIgnore && PC.IsValid())
        {
            PC->SetControlRotation(OriginalControl); PC->SetIgnoreLookInput(false); PC->SetIgnoreMoveInput(false);
            if (Character.IsValid())
            { FMinimalViewInfo RestoredView; Character->GetFirstPersonCameraComponent()->GetCameraView(0.f, RestoredView); }
        }
        bOwnInputIgnore = false;
        if (bRequestExit)
        {
            for (FMonsterState& Saved : Monsters)
            {
                if (!Saved.Monster.IsValid()) { continue; }
                Saved.Monster->SetActorTransform(Saved.Transform, false, nullptr, ETeleportType::TeleportPhysics);
                Saved.Monster->GetCharacterMovement()->SetMovementMode(Saved.Mode, Saved.CustomMode);
                if (Saved.Controller.IsValid()) { Saved.Controller->Possess(Saved.Monster.Get()); }
            }
        }
        FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta);
        UE_LOG(LogRogue10m, Display, TEXT("BOXING_DIRECT METRICS hand_l=%.3f hand_r=%.3f shoulder_relative_l=%.3f shoulder_relative_r=%.3f foot_l=%.3f foot_r=%.3f head_offset=%.3f head_yaw=%.3f head_pitch=%.3f head_roll=%.3f rendered_rotation=%.3f limit_frames=%d raw_offset_error=%.6f raw_rotation_error=%.6f duration=%.6f"),
            MaxLeftTravel, MaxRightTravel, MaxLeftReach, MaxRightReach, MaxLeftFoot, MaxRightFoot, MaxHeadOffset, MaxHeadYaw, MaxHeadPitch, MaxHeadRoll, MaxRenderedRotation, CameraLimitFrames, MaxHeadOffsetError, MaxHeadRotationError, Duration);
        const bool bPassed = Failures == 0 && bPrepared && FrameIndex == TotalFrames;
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=BOXING_DIRECT_PREVIEW_%s frames=%d failures=%d"), bPassed ? TEXT("PASSED") : TEXT("FAILED"), FrameIndex, Failures);
        if (bRequestExit) { FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<APlayerController> PC;
    TWeakObjectPtr<USkeletalMesh> OriginalMesh;
    TStrongObjectPtr<UClass> OriginalClass;
    TStrongObjectPtr<UAnimSequence> Sequence;
    TArray<FMonsterState> Monsters;
    FTransform MeshTransform, CameraTransform, PlayerTransform, ReferenceHead;
    FVector ExpectedCameraLocation = FVector::ZeroVector;
    const FVector EyeLever = FVector(12.f, 0.f, 8.f);
    FRotator OriginalControl, ExpectedControl, OldHeadRotationScale, OldHeadLimits;
    EAnimationMode::Type OriginalMode = EAnimationMode::AnimationBlueprint;
    FDelegateHandle TickHandle, CleanupHandle;
    FTimerHandle PrepareTimer;
    FString OutputFolder;
    FVector StartLeft, StartRight, StartLeftFoot, StartRightFoot, StartLeftReach, StartRightReach;
    int32 FrameIndex = 0, Failures = 0, InitialSerial = 0, FastStart = 0, FastEnd = 0, SlowStart = 0, SlowEnd = 0, NativeStart = 0, TotalFrames = 0, CameraLimitFrames = 0;
    float Duration = 0.f, OriginalFOV = 90.f;
    float OldHeadTranslationScale = 1.f, OldHeadSpeed = 24.f, OldHeadOffsetLimit = 65.f, OldMotionScale = 1.f;
    bool bOldHeadFollow = true, bHeadOverride = false, bLookDownPreview = false, bForwardViewPreview = false;
    double OldDelta = 1.0 / 30.0, MaxLeftTravel = 0, MaxRightTravel = 0, MaxLeftFoot = 0, MaxRightFoot = 0;
    double MaxLeftReach = 0, MaxRightReach = 0, MaxHeadOffset = 0, MaxHeadYaw = 0, MaxHeadPitch = 0, MaxHeadRoll = 0, MaxRenderedRotation = 0, MaxHeadOffsetError = 0, MaxHeadRotationError = 0;
    bool OldFixed = false, bOldCanBeDamaged = true, bOwnInputIgnore = false, bPrepared = false, bSequenceActive = false, bFinished = false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (GIsEditor) { UE_LOG(LogRogue10m, Warning, TEXT("BOXING_DIRECT requires standalone -game; editor and PIE are unsupported.")); return; }
    if (!GEngine || (Active.IsValid() && !Active->IsFinished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* W = Context.World(); W && W->WorldType == EWorldType::Game)
        { Active = MakeShared<FRun>(); Active->Start(W); return; }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.PreviewBoxingDirect"),
    TEXT("Captures retargeted Boxing.fbx on the actual first-person body at 1x/0.5x, restores native animation, then exits. Editor-only standalone fixture."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
