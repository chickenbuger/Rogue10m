// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
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
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mFistAnimInstance.h"
#include "Rogue10mGameState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mFistPresentationTest
{
struct FHandBasis
{
    FVector Position = FVector::ZeroVector;
    FVector Forward = FVector::ZeroVector;
    FVector Forearm = FVector::ZeroVector;
    FVector Width = FVector::ZeroVector;
    FVector ThumbRoot = FVector::ZeroVector;
    FVector ThumbUp = FVector::ZeroVector;
    bool bValid = false;
};

struct FFingerMeasure
{
    float PipDegrees = 0.0f;
    float ProximalLength = 0.0f;
    float MiddleLength = 0.0f;
    bool bValid = false;
};

class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld, const TArray<FString>& Args)
    {
        World = InWorld;
        Arguments = Args;
        FTimerHandle Timer;
        InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self=AsShared()] { Self->Prepare(); }), 4.0f, false);
    }
private:
    void Check(bool OK, const TCHAR* Reason)
    {
        if (!OK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("FIST_PRESENTATION FAIL %s"), Reason); }
    }
    void ApplyOrientationTuning()
    {
        auto* Fist = Cast<URogue10mFistAnimInstance>(Character->GetFirstPersonMesh()->GetAnimInstance());
        Check(Fist != nullptr, TEXT("fist_orientation_instance_missing"));
        if (!Fist) { return; }
        for (const FString& Argument : Arguments)
        {
            FString Key, Value;
            if (!Argument.Split(TEXT("="), &Key, &Value)) { continue; }
            if (Key != TEXT("guard_roll") && Key != TEXT("hit_roll") && Key != TEXT("curl_mcp")
                && Key != TEXT("curl_pip") && Key != TEXT("curl_dip")) { continue; }
            const bool bNumeric = Value.IsNumeric();
            const float RequestedAngle = bNumeric ? FCString::Atof(*Value) : 0.0f;
            Check(bNumeric && FMath::IsFinite(RequestedAngle), TEXT("orientation_tuning_value_invalid"));
            if (!bNumeric || !FMath::IsFinite(RequestedAngle)) { continue; }
            if (Key == TEXT("guard_roll")) { Fist->GuardThumbInwardDegrees = FMath::Clamp(RequestedAngle, -60.0f, 90.0f); }
            else if (Key == TEXT("hit_roll")) { Fist->HitThumbInwardDegrees = FMath::Clamp(RequestedAngle, -60.0f, 90.0f); }
            else if (Key == TEXT("curl_mcp")) { Fist->FingerCurlDegrees.X = RequestedAngle; }
            else if (Key == TEXT("curl_pip")) { Fist->FingerCurlDegrees.Y = RequestedAngle; }
            else { Fist->FingerCurlDegrees.Z = RequestedAngle; }
        }
        UE_LOG(LogRogue10m, Display, TEXT("FIST_PRESENTATION ORIENTATION_TUNING guard_roll=%.1f hit_roll=%.1f curl_mcp=%.1f curl_pip=%.1f curl_dip=%.1f"),
            Fist->GuardThumbInwardDegrees, Fist->HitThumbInwardDegrees,
            Fist->FingerCurlDegrees.X, Fist->FingerCurlDegrees.Y, Fist->FingerCurlDegrees.Z);
    }
    void Prepare()
    {
        APlayerController* PC = World->GetFirstPlayerController();
        Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (PC)
        {
            // Own exactly one ignore-stack entry; scripted control rotations still work.
            LookInputController = PC;
            PC->SetIgnoreLookInput(true);
            bOwnLookInputIgnore = true;
        }
        if (!Character.IsValid()) { Check(false, TEXT("character_missing")); Finish(); return; }
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        if (Presentation)
        {
            Presentation->bEnableFistPresentation = false;
            Presentation->RefreshPresentation();
            auto* Camera = Character->GetFirstPersonCameraComponent();
            auto* Mesh = Character->GetFirstPersonMesh();
            OriginalCameraParent = Camera->GetAttachParent();
            OriginalMeshParent = Mesh->GetAttachParent();
            OriginalCameraSocket = Camera->GetAttachSocketName();
            OriginalMeshSocket = Mesh->GetAttachSocketName();
            OriginalCameraTransform = Camera->GetRelativeTransform();
            OriginalMeshTransform = Mesh->GetRelativeTransform();
            OriginalAnimClass = Mesh->GetAnimClass();
            OriginalAnimationMode = Mesh->GetAnimationMode();
            OriginalWorldFOV = Camera->FieldOfView;
            OriginalArmsFOV = Camera->FirstPersonFieldOfView;
            OriginalArmsScale = Camera->FirstPersonScale;
            OriginalFOVEnabled = Camera->bEnableFirstPersonFieldOfView;
            OriginalScaleEnabled = Camera->bEnableFirstPersonScale;
            Presentation->bEnableFistPresentation = true;
            Presentation->RefreshPresentation();
        }
        if (Arguments.Num() == 2 && Arguments[0].IsNumeric() && Arguments[1].IsNumeric() && Presentation)
        {
            Presentation->bEnableFistPresentation = false;
            Presentation->RefreshPresentation();
            Presentation->GuardMeshOffset.X = FCString::Atof(*Arguments[0]);
            Presentation->GuardMeshOffset.Z = FCString::Atof(*Arguments[1]);
            Presentation->bEnableFistPresentation = true;
            Presentation->RefreshPresentation();
        }
        Check(Presentation && Presentation->IsFistPresentationActive(), TEXT("fist_mode_inactive"));
        Check(Character->GetFirstPersonMesh()->GetAnimInstance() != nullptr, TEXT("arms_anim_missing"));
        ApplyOrientationTuning();
        Character->SetCanBeDamaged(false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        PC->SetControlRotation(FRotator(0, PC->GetControlRotation().Yaw, 0));
        if (auto* Attributes=Character->GetRogueAttributeSet())
        {
            Attributes->RestoreVitals();
            Attributes->SetExperience(Attributes->GetExperienceToNextLevel() * 0.42f);
        }
        if (auto* State=World->GetGameState<ARogue10mGameState>()) { State->StartRun(); }
        int32 MonsterIndex=0;
        const FVector Forward=Character->GetActorForwardVector();
        const FVector Right=Character->GetActorRightVector();
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            It->ClearAITarget();
            if (AController* AI=It->GetController()) { AI->UnPossess(); }
            It->SetActorEnableCollision(MonsterIndex==0);
            if (MonsterIndex==0)
            {
                TargetMonster=*It;
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
            }
            It->GetCharacterMovement()->StopMovementImmediately();
            It->GetCharacterMovement()->DisableMovement();
            const FVector Location=Character->GetActorLocation() + Forward * 550.0f + Right * (MonsterIndex++ * 250.0f);
            It->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
        }
        IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/WindowsEditor/BodyMeleePreview")), true);
        OriginalFixed=FApp::UseFixedTimeStep();
        OriginalDelta=FApp::GetFixedDeltaTime();
        FApp::SetFixedDeltaTime(1.0 / 24.0);
        FApp::SetUseFixedTimeStep(true);
        TickHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda(
            [Self=AsShared()](UWorld* TickWorld, ELevelTick, float)
            { if (TickWorld==Self->World.Get()) { Self->Frame(); } });
    }
    FVector HandInCamera(const TCHAR* Bone) const
    {
        return Character->GetFirstPersonCameraComponent()->GetComponentTransform().InverseTransformPosition(
            Character->GetFirstPersonMesh()->GetSocketLocation(Bone));
    }
    FName FingerBone(const TCHAR* Finger, const TCHAR* Side) const
    {
        const auto* Arms = Character->GetFirstPersonMesh();
        for (const TCHAR* Part : { TEXT("01"), TEXT("metacarpal") })
        {
            const FName Bone(*FString::Printf(TEXT("%s_%s_%s"), Finger, Part, Side));
            if (Arms->GetBoneIndex(Bone) != INDEX_NONE) { return Bone; }
        }
        return NAME_None;
    }
    FHandBasis ReadHandBasis(const TCHAR* Side)
    {
        FHandBasis Result;
        const auto* Arms = Character->GetFirstPersonMesh();
        const FName Hand(*FString::Printf(TEXT("hand_%s"), Side));
        const FName Lower(*FString::Printf(TEXT("lowerarm_%s"), Side));
        const FName Middle = FingerBone(TEXT("middle"), Side);
        const FName Index = FingerBone(TEXT("index"), Side);
        const FName Pinky = FingerBone(TEXT("pinky"), Side);
        const FName Thumb = FingerBone(TEXT("thumb"), Side);
        Result.bValid = Arms->GetBoneIndex(Hand) != INDEX_NONE && Arms->GetBoneIndex(Lower) != INDEX_NONE
            && !Middle.IsNone() && !Index.IsNone() && !Pinky.IsNone() && !Thumb.IsNone();
        Check(Result.bValid, TEXT("orientation_fixture_bones_missing"));
        if (!Result.bValid) { return Result; }
        const FTransform Camera = Character->GetFirstPersonCameraComponent()->GetComponentTransform();
        const auto Position = [&](FName Bone)
        {
            return Camera.InverseTransformPosition(Arms->GetSocketLocation(Bone));
        };
        Result.Position = Position(Hand);
        Result.Forward = (Position(Middle) - Result.Position).GetSafeNormal();
        Result.Forearm = (Result.Position - Position(Lower)).GetSafeNormal();
        Result.Width = (Position(Pinky) - Position(Index)).GetSafeNormal();
        Result.ThumbRoot = (Position(Thumb) - Result.Position).GetSafeNormal();
        // Index-to-pinky is logged as requested; the opposite projected axis is thumbward.
        Result.ThumbUp = FVector::VectorPlaneProject(-Result.Width, Result.Forward).GetSafeNormal();
        if (FVector::DotProduct(Result.ThumbUp, Result.ThumbRoot) < 0) { Result.ThumbUp *= -1; }
        Result.bValid = !Result.Forward.IsNearlyZero() && !Result.Forearm.IsNearlyZero()
            && !Result.ThumbUp.IsNearlyZero() && !Result.Width.IsNearlyZero();
        Check(Result.bValid, TEXT("orientation_fixture_basis_degenerate"));
        return Result;
    }
    static float BasisAngle(const FVector& A, const FVector& B)
    {
        return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0)));
    }
    void CheckFingerFlexion(const TCHAR* Stage, bool bSaveGuard)
    {
        const auto* Arms = Character->GetFirstPersonMesh();
        const USkeletalMesh* Mesh = Arms->GetSkeletalMeshAsset();
        Check(Mesh != nullptr, TEXT("finger_reference_mesh_missing"));
        if (!Mesh) { return; }
        const TArray<FTransform>& Reference = Mesh->GetRefSkeleton().GetRefBonePose();
        TArray<FFingerMeasure> Current;
        Current.SetNum(10);
        int32 MetricIndex = 0;
        for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
        {
            const FName Hand(*FString::Printf(TEXT("hand_%s"), Side));
            const FName Middle(*FString::Printf(TEXT("middle_01_%s"), Side));
            const FVector Wrist = Arms->GetSocketLocation(Hand);
            const FVector MiddleBase = Arms->GetSocketLocation(Middle);
            const FVector Palm = (Wrist + MiddleBase) * 0.5;
            const double PalmLength = FMath::Max(1.0, FVector::Distance(Wrist, MiddleBase));
            for (const TCHAR* Finger : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb") })
            {
                FFingerMeasure& Measure = Current[MetricIndex++];
                const FName B1(*FString::Printf(TEXT("%s_01_%s"), Finger, Side));
                const FName B2(*FString::Printf(TEXT("%s_02_%s"), Finger, Side));
                const FName B3(*FString::Printf(TEXT("%s_03_%s"), Finger, Side));
                const int32 I1 = Arms->GetBoneIndex(B1), I2 = Arms->GetBoneIndex(B2), I3 = Arms->GetBoneIndex(B3);
                const bool bBonesValid = Reference.IsValidIndex(I1) && Reference.IsValidIndex(I2) && Reference.IsValidIndex(I3);
                Check(bBonesValid, TEXT("finger_flexion_bones_missing"));
                if (!bBonesValid) { continue; }
                const FVector P1 = Arms->GetSocketLocation(B1), P2 = Arms->GetSocketLocation(B2), P3 = Arms->GetSocketLocation(B3);
                const FVector Proximal = P2 - P1, MiddleSegment = P3 - P2;
                Measure.ProximalLength = Proximal.Size();
                Measure.MiddleLength = MiddleSegment.Size();
                Measure.bValid = Measure.ProximalLength > 0.05f && Measure.MiddleLength > 0.05f;
                Check(Measure.bValid, TEXT("finger_segment_collapsed"));
                if (!Measure.bValid) { continue; }
                Measure.PipDegrees = BasisAngle(Proximal.GetSafeNormal(), MiddleSegment.GetSafeNormal());
                const float McpDegrees = BasisAngle((P1 - Wrist).GetSafeNormal(), Proximal.GetSafeNormal());
                const FQuat LocalDistal = Arms->GetSocketQuaternion(B2).Inverse() * Arms->GetSocketQuaternion(B3);
                const float DistalReferenceDelta = FMath::RadiansToDegrees(LocalDistal.AngularDistance(Reference[I3].GetRotation()));
                const double JointToPalm = FVector::Distance(P3, Palm) / PalmLength;
                const bool bThumb = FCString::Strcmp(Finger, TEXT("thumb")) == 0;
                UE_LOG(LogRogue10m, Display,
                    TEXT("FIST_PRESENTATION FINGER_FLEX stage=%s frame=%d side=%s finger=%s mcp_axis_deg=%.1f pip_deg=%.1f distal_ref_delta_deg=%.1f segment_lengths=%.2f,%.2f joint03_to_palm_normalized=%.3f"),
                    Stage, FrameIndex, Side, Finger, McpDegrees, Measure.PipDegrees, DistalReferenceDelta,
                    Measure.ProximalLength, Measure.MiddleLength, JointToPalm);
                Check(FMath::Abs(Measure.ProximalLength - Reference[I2].GetTranslation().Size()) < 0.25f
                    && FMath::Abs(Measure.MiddleLength - Reference[I3].GetTranslation().Size()) < 0.25f,
                    TEXT("finger_reference_lengths_changed"));
                if (!bThumb)
                {
                    Check(Measure.PipDegrees >= 55.0f && Measure.PipDegrees <= 120.0f, TEXT("finger_not_in_closed_fist_range"));
                    if (GuardFingerMeasures.IsValidIndex(MetricIndex - 1) && GuardFingerMeasures[MetricIndex - 1].bValid)
                    {
                        Check(FMath::Abs(Measure.PipDegrees - GuardFingerMeasures[MetricIndex - 1].PipDegrees) < 8.0f,
                            TEXT("finger_clench_changed_during_punch_or_recovery"));
                    }
                }
                // Thumb proportions differ. Joint-to-palm distance is diagnostic, not a
                // "smaller is better" gate: a clenched pose must still pass skin intersection QA.
            }
        }
        if (bSaveGuard) { GuardFingerMeasures = MoveTemp(Current); }
    }
    void CheckOrientation(const TCHAR* Stage, bool bRequireMirror, bool bSaveGuard = false)
    {
        const FHandBasis Left = ReadHandBasis(TEXT("l"));
        const FHandBasis Right = ReadHandBasis(TEXT("r"));
        if (!Left.bValid || !Right.bValid) { return; }
        for (const bool bLeft : { true, false })
        {
            const FHandBasis& Hand = bLeft ? Left : Right;
            const FHandBasis& Guard = bLeft ? LeftGuardBasis : RightGuardBasis;
            const float ForearmDot = FVector::DotProduct(Hand.Forward, Hand.Forearm);
            const float ThumbChange = Guard.bValid ? BasisAngle(Guard.ThumbUp, Hand.ThumbUp) : 0.0f;
            UE_LOG(LogRogue10m, Display,
                TEXT("FIST_PRESENTATION HAND_BASIS stage=%s frame=%d side=%s hand=%s forward=%s index_to_pinky=%s thumb_root=%s thumb_up=%s forearm_dot=%.3f guard_thumb_delta=%.1f"),
                Stage, FrameIndex, bLeft ? TEXT("left") : TEXT("right"), *Hand.Position.ToString(),
                *Hand.Forward.ToString(), *Hand.Width.ToString(), *Hand.ThumbRoot.ToString(),
                *Hand.ThumbUp.ToString(), ForearmDot, ThumbChange);
            Check(ForearmDot > 0.85f, TEXT("wrist_not_aligned_with_forearm"));
            Check(Hand.Forward.X > 0.15f, TEXT("knuckles_not_facing_forward"));
            Check(Hand.ThumbUp.Z > 0.0f, TEXT("thumb_side_rolled_below_hand"));
            if (Guard.bValid)
            {
                // Allows the intended inward punch turn but rejects a half-turn wrist flip.
                Check(ThumbChange < 65.0f, TEXT("wrist_roll_exceeded_quick_melee_envelope"));
            }
        }
        if (bRequireMirror)
        {
            const FVector MirroredForward(Left.Forward.X, -Left.Forward.Y, Left.Forward.Z);
            const FVector MirroredThumb(Left.ThumbUp.X, -Left.ThumbUp.Y, Left.ThumbUp.Z);
            const float ForwardMirror = FVector::DotProduct(MirroredForward, Right.Forward);
            const float ThumbMirror = FVector::DotProduct(MirroredThumb, Right.ThumbUp);
            UE_LOG(LogRogue10m, Display, TEXT("FIST_PRESENTATION HAND_MIRROR stage=%s forward_dot=%.3f thumb_dot=%.3f"),
                Stage, ForwardMirror, ThumbMirror);
            Check(ForwardMirror > 0.5f && ThumbMirror > 0.5f, TEXT("guard_hand_orientations_not_mirrored"));
        }
        CheckFingerFlexion(Stage, bSaveGuard);
        if (bSaveGuard) { LeftGuardBasis = Left; RightGuardBasis = Right; }
    }
    void BeginViewAudit()
    {
        bViewAudit = true;
        ViewStage = 0;
        ViewFrames = 0;
        ViewBaseRotation = Character->GetController()->GetControlRotation();
        if (const UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Viewport->Viewport)
        {
            OriginalViewportSize = Viewport->Viewport->GetSizeXY();
        }
    }
    void AuditView()
    {
        auto* PC = Cast<APlayerController>(Character->GetController());
        if (!PC) { Check(false, TEXT("view_audit_controller_missing")); Finish(); return; }
        if (ViewStage >= 3)
        {
            // The prior frame has rendered its requested still before the process exits.
            PC->SetControlRotation(ViewBaseRotation);
            if (OriginalViewportSize.X > 0 && OriginalViewportSize.Y > 0)
            {
                GEngine->Exec(World.Get(), *FString::Printf(TEXT("r.SetRes %dx%dw"),
                    OriginalViewportSize.X, OriginalViewportSize.Y));
            }
            UE_LOG(LogRogue10m, Display, TEXT("FIST_PRESENTATION VIEW_AUDIT completed=3"));
            Finish();
            return;
        }
        const float Pitch = ViewStage == 0 ? 25.0f : ViewStage == 1 ? -25.0f : 0.0f;
        if (ViewFrames == 0)
        {
            PC->SetControlRotation(FRotator(Pitch, ViewBaseRotation.Yaw, 0.0f));
            if (ViewStage == 2) { GEngine->Exec(World.Get(), TEXT("r.SetRes 2560x1080w")); }
        }
        if (++ViewFrames < (ViewStage == 2 ? 24 : 8)) { return; }
        const TCHAR* Stage = ViewStage == 0 ? TEXT("guard_pitch_up")
            : ViewStage == 1 ? TEXT("guard_pitch_down") : TEXT("guard_ultrawide");
        CheckOrientation(Stage, true);
        Check(FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Pitch, Pitch)) < 1.0f,
            TEXT("view_audit_pitch_not_applied"));
        Check(Character->GetFirstPersonCameraComponent()->GetAttachParent() == Character->GetCapsuleComponent(),
            TEXT("view_audit_camera_parent_changed"));
        if (ViewStage == 2)
        {
            const UGameViewportClient* Viewport = World->GetGameViewport();
            Check(Viewport && Viewport->Viewport && Viewport->Viewport->GetSizeXY() == FIntPoint(2560, 1080),
                TEXT("view_audit_ultrawide_size_not_applied"));
        }
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
            TEXT("Screenshots/WindowsEditor/BodyMeleePreview"), FString(Stage) + TEXT(".png")), true, false);
        ++ViewStage;
        ViewFrames = 0;
    }
    void AuditCameraControls()
    {
        auto* Presentation=Character->GetFirstPersonPresentationComponent();
        if (!Presentation) { Check(false,TEXT("camera_audit_component_missing")); Finish(); return; }
        ++CameraAuditFrames;
        if (CameraAuditStage==1)
        {
            if (CameraAuditFrames>=36) { CameraAuditStage=2; CameraAuditFrames=0; }
            return;
        }
        if (CameraAuditStage==3)
        {
            if (CameraAuditFrames<12) { return; }
            Check(Presentation->GetAppliedCameraMotionOffset().IsNearlyZero(0.01f)
                && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(0.01f),TEXT("camera_audit_reentry_residual"));
            UE_LOG(LogRogue10m,Display,TEXT("FIST_PRESENTATION CAMERA_CONTROLS_AUDIT active_scale_zero=passed active_weapon_switch=passed active_death_state=passed"));
            bCameraAudit=false;
            if (Arguments.Contains(TEXT("views"))) { BeginViewAudit(); }
            else { Finish(); }
            return;
        }
        if (CameraAuditFrames==1)
        {
            Character->GetCombatComponent()->HandleAttackPressed(true);
            Character->GetCombatComponent()->HandleAttackReleased(true);
        }
        const bool bMoving=Presentation->GetAppliedCameraMotionOffset().Size()>0.05f
            || !Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(0.1f);
        if (!bMoving)
        {
            if (CameraAuditFrames>24) { Check(false,TEXT("camera_controls_audit_never_became_active")); Finish(); }
            return;
        }
        if (CameraAuditStage==0)
        {
            FMinimalViewInfo View;
            View.Location=FVector(12,34,56);
            View.Rotation=FRotator(11,22,3);
            const FMinimalViewInfo Baseline=View;
            const float OriginalScale=Presentation->CameraMotionScale;
            Presentation->CameraMotionScale=0.0f;
            Presentation->ApplyBodyMotionToView(1.0f/24.0f,View);
            Check(View.Location.Equals(Baseline.Location,0.001f) && View.Rotation.Equals(Baseline.Rotation,0.001f)
                && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero() && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),
                TEXT("active_camera_motion_scale_zero_not_respected"));
            Presentation->CameraMotionScale=OriginalScale;
            CameraAuditStage=1;
        }
        else
        {
            auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
            Check(State!=nullptr,TEXT("camera_death_audit_state_missing"));
            if (State)
            {
                FMinimalViewInfo DeathView;
                DeathView.Location=FVector(12,34,56);
                DeathView.Rotation=FRotator(11,22,3);
                const FMinimalViewInfo Baseline=DeathView;
                State->SetCharacterDead(true);
                Presentation->ApplyBodyMotionToView(1.0f/24.0f,DeathView);
                Check(DeathView.Location.Equals(Baseline.Location,0.001f) && DeathView.Rotation.Equals(Baseline.Rotation,0.001f)
                    && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero() && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),
                    TEXT("active_camera_motion_not_cleared_on_death"));
                State->SetCharacterDead(false);
                Presentation->ApplyBodyMotionToView(1.0f/24.0f,DeathView);
                Check(Presentation->GetAppliedCameraMotionOffset().Size()>0.001f
                    || !Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(0.001f),TEXT("weapon_audit_camera_was_not_reactivated"));
            }
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
            Check(!Presentation->IsFistPresentationActive() && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero()
                && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),TEXT("active_camera_motion_not_cleared_on_weapon_change"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
            ApplyOrientationTuning();
            CameraAuditStage=3;
        }
        CameraAuditFrames=0;
    }
    void Frame()
    {
        if (!Character.IsValid()) { Check(false,TEXT("character_lost")); Finish(); return; }
        if (bViewAudit) { AuditView(); return; }
        if (bCameraAudit) { AuditCameraControls(); return; }
        auto* Camera=Character->GetFirstPersonCameraComponent();
        auto* Arms=Character->GetFirstPersonMesh();
        if (bReentryPending)
        {
            if (++ReentryFrames<8) { return; }
            const FVector Left=HandInCamera(TEXT("hand_l"));
            const FVector Right=HandInCamera(TEXT("hand_r"));
            Check(Left.X>20 && Right.X>20 && Left.Z>-30 && Right.Z>-30,TEXT("fist_reentry_pose_missing"));
            CheckOrientation(TEXT("reentry"), true);
            bReentryPending=false;
            bCameraAudit=true;
            CameraAuditFrames=0;
            return;
        }
        if (FrameIndex==8 && TargetMonster.IsValid())
        {
            TargetMonster->SetActorLocation(Camera->GetComponentLocation()+Camera->GetForwardVector()*450.0f-FVector(0,0,64),
                false,nullptr,ETeleportType::TeleportPhysics);
            TargetMonster->SetActorRotation(FRotator(0,Camera->GetComponentRotation().Yaw+180.0f,0));
        }
        if (FrameIndex==12)
        {
            auto* PC=Cast<ARogue10mPlayerController>(Character->GetController());
            Check(PC && PC->FindLookedAtMonster()==TargetMonster.Get(),TEXT("preview_monster_not_targeted"));
            LeftGuard=HandInCamera(TEXT("hand_l"));
            RightGuard=HandInCamera(TEXT("hand_r"));
            GuardCamera=Camera->GetRelativeTransform();
            GuardControlRotation=Character->GetController()->GetControlRotation();
            GuardSpine=Arms->GetSocketQuaternion(TEXT("spine_03"));
            UE_LOG(LogRogue10m, Display, TEXT("FIST_PRESENTATION GUARD left=%s right=%s mesh=%s"),
                *LeftGuard.ToString(), *RightGuard.ToString(), *Arms->GetRelativeLocation().ToString());
            Check(LeftGuard.X>20 && RightGuard.X>20,TEXT("guard_hands_behind_camera"));
            Check(LeftGuard.Y<0 && RightGuard.Y>0,TEXT("guard_hands_not_flanking_view"));
            Check(LeftGuard.Z>-30 && RightGuard.Z>-30,TEXT("guard_hands_below_view"));
            CheckOrientation(TEXT("guard"), true, true);
        }
        if (FrameIndex==24 || FrameIndex==56 || FrameIndex==88)
        {
            const bool bPrimary = true;
            Character->GetCombatComponent()->HandleAttackPressed(bPrimary);
            Character->GetCombatComponent()->HandleAttackReleased(bPrimary);
            ++Attacks;
        }
        if (FrameIndex==28 || FrameIndex==60 || FrameIndex==92)
        {
            const bool bPlaying=Arms->GetAnimInstance() && Arms->GetAnimInstance()->IsAnyMontagePlaying();
            Check(bPlaying,TEXT("requested_punch_montage_not_playing"));
            if (bPlaying) { ++PlayedAttacks; }
            CheckOrientation(TEXT("attack"), false);
        }
        if (FrameIndex>=13)
        {
            Check(Camera->GetAttachParent()==Character->GetCapsuleComponent(),TEXT("camera_not_stable_parent"));
            Check(Camera->GetRelativeTransform().Equals(GuardCamera,0.01f),TEXT("animation_moved_camera"));
            Check(Character->GetController()->GetControlRotation().Equals(GuardControlRotation,0.01f),
                TEXT("body_motion_changed_control_rotation"));
            auto* Presentation = Character->GetFirstPersonPresentationComponent();
            auto* Fist = Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance());
            auto* PC = Cast<APlayerController>(Character->GetController());
            if (Presentation && Fist && PC && PC->PlayerCameraManager)
            {
                const FVector AppliedOffset=Presentation->GetAppliedCameraMotionOffset();
                const FRotator AppliedRotation=Presentation->GetAppliedCameraMotionRotation();
                const float RotationMagnitude=FMath::Max3(FMath::Abs(AppliedRotation.Pitch),
                    FMath::Abs(AppliedRotation.Yaw),FMath::Abs(AppliedRotation.Roll));
                Check(!AppliedOffset.ContainsNaN() && !AppliedRotation.ContainsNaN(),TEXT("body_camera_non_finite"));
                if (Presentation->IsFullBodyPresentationActive())
                {
                    const auto Limit = Presentation->MaxHeadCameraRotation;
                    Check(AppliedOffset.Size() <= Presentation->MaxHeadCameraOffset + .01f
                        && FMath::Abs(AppliedRotation.Pitch) <= Limit.Pitch + .01f
                        && FMath::Abs(AppliedRotation.Yaw) <= Limit.Yaw + .01f
                        && FMath::Abs(AppliedRotation.Roll) <= Limit.Roll + .01f, TEXT("head_camera_exceeded_envelope"));
                }
                else { Check(AppliedOffset.Size()<=2.01f && RotationMagnitude<=2.01f,TEXT("body_camera_exceeded_envelope")); }
                const FRotator ViewDelta=(PC->PlayerCameraManager->GetCameraRotation()-Camera->GetComponentRotation()).GetNormalized();
                const float ActualViewAngle=FMath::Max3(FMath::Abs(ViewDelta.Pitch),FMath::Abs(ViewDelta.Yaw),FMath::Abs(ViewDelta.Roll));
                MaxCameraMotion=FMath::Max(MaxCameraMotion,RotationMagnitude);
                MaxRenderedViewMotion=FMath::Max(MaxRenderedViewMotion,ActualViewAngle);
                MaxCameraOffset=FMath::Max(MaxCameraOffset,static_cast<float>(AppliedOffset.Size()));
                if (FrameIndex>=24 && Fist->IsAnyMontagePlaying())
                {
                    MaxSpineMotion=FMath::Max(MaxSpineMotion,FMath::RadiansToDegrees(GuardSpine.AngularDistance(Arms->GetSocketQuaternion(TEXT("spine_03")))));
                    MaxHandVerticalMotion=FMath::Max(MaxHandVerticalMotion,FMath::Max(
                        FMath::Abs(HandInCamera(TEXT("hand_l")).Z-LeftGuard.Z),FMath::Abs(HandInCamera(TEXT("hand_r")).Z-RightGuard.Z)));
                }
                if (FrameIndex<24) { Check(RotationMagnitude<0.05f && AppliedOffset.Size()<0.05f,TEXT("idle_camera_motion_not_zero")); }
                UE_LOG(LogRogue10m,Display,TEXT("FIST_PRESENTATION BODY_FRAME frame=%d weight=%.3f camera_local=%s camera_rotation=%s rendered_angle=%.3f"),
                    FrameIndex,Fist->GetBodyMotionWeight(),*AppliedOffset.ToString(),*AppliedRotation.ToString(),ActualViewAngle);
            }
            if (FrameIndex>=24 && Arms->GetAnimInstance() && Arms->GetAnimInstance()->IsAnyMontagePlaying())
            {
                ++MontageFrames;
                MaxHandMovement=FMath::Max(MaxHandMovement,FMath::Max(
                    FVector::Distance(RightGuard,HandInCamera(TEXT("hand_r"))),
                    FVector::Distance(LeftGuard,HandInCamera(TEXT("hand_l")))));
            }
        }
        if (FrameIndex<120)
        {
            FScreenshotRequest::RequestScreenshot(
                FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/WindowsEditor/BodyMeleePreview"), FString::Printf(TEXT("frame_%04d.png"),FrameIndex)), true, false);
            ++FrameIndex;
            return;
        }
        CheckOrientation(TEXT("recovered"), true);
        Check(MaxHandMovement>8.0f,TEXT("punch_hands_did_not_move"));
        Check(MontageFrames>0,TEXT("punch_montage_not_played"));
        Check(FVector::Distance(LeftGuard,HandInCamera(TEXT("hand_l")))<15.0f
            && FVector::Distance(RightGuard,HandInCamera(TEXT("hand_r")))<15.0f,TEXT("guard_not_recovered_after_attack"));
        auto* Presentation=Character->GetFirstPersonPresentationComponent();
        Check(MaxSpineMotion>0.5f,TEXT("montage_body_motion_discarded"));
        Check(MaxHandVerticalMotion>0.5f,TEXT("swing_hand_arc_missing"));
        Check(MaxCameraMotion>0.1f && MaxRenderedViewMotion>0.1f,TEXT("body_camera_motion_missing_from_rendered_view"));
        Check(Presentation->GetAppliedCameraMotionOffset().Size()<0.1f
            && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(0.1f),TEXT("body_camera_not_settled_after_attack"));
        UE_LOG(LogRogue10m,Display,TEXT("FIST_PRESENTATION BODY_AUDIT spine_deg=%.3f hand_vertical_cm=%.3f camera_deg=%.3f camera_cm=%.3f rendered_deg=%.3f"),
            MaxSpineMotion,MaxHandVerticalMotion,MaxCameraMotion,MaxCameraOffset,MaxRenderedViewMotion);
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
        Check(Presentation->GetAppliedCameraMotionOffset().IsNearlyZero() && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),
            TEXT("camera_motion_not_cleared_on_weapon_change"));
        Check(!Character->GetFirstPersonPresentationComponent()->IsFistPresentationActive(),TEXT("other_weapon_not_restored"));
        Check(Camera->GetAttachParent()==OriginalCameraParent.Get(),TEXT("original_camera_parent_not_restored"));
        Check(Camera->GetAttachSocketName()==OriginalCameraSocket,TEXT("original_camera_socket_not_restored"));
        Check(Arms->GetAttachParent()==OriginalMeshParent.Get() && Arms->GetAttachSocketName()==OriginalMeshSocket,
            TEXT("original_mesh_attachment_not_restored"));
        Check(Camera->GetRelativeTransform().Equals(OriginalCameraTransform,0.01f)
            && Arms->GetRelativeTransform().Equals(OriginalMeshTransform,0.01f),TEXT("original_transforms_not_restored"));
        Check(Arms->GetAnimClass()==OriginalAnimClass.Get() && Arms->GetAnimationMode()==OriginalAnimationMode,
            TEXT("original_animation_not_restored"));
        Check(FMath::IsNearlyEqual(Camera->FieldOfView,OriginalWorldFOV)
            && FMath::IsNearlyEqual(Camera->FirstPersonFieldOfView,OriginalArmsFOV)
            && FMath::IsNearlyEqual(Camera->FirstPersonScale,OriginalArmsScale)
            && Camera->bEnableFirstPersonFieldOfView==OriginalFOVEnabled
            && Camera->bEnableFirstPersonScale==OriginalScaleEnabled,TEXT("original_projection_not_restored"));
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
        Check(Character->GetFirstPersonPresentationComponent()->IsFistPresentationActive(),TEXT("fist_reentry_failed"));
        ApplyOrientationTuning();
        UE_LOG(LogRogue10m,Display,TEXT("FIST_PRESENTATION MOTION requests=%d played=%d montage_frames=%d hand_travel=%.1f"),
            Attacks,PlayedAttacks,MontageFrames,MaxHandMovement);
        bReentryPending=true;
    }
    void Finish()
    {
        if (bOwnLookInputIgnore && LookInputController.IsValid())
        {
            LookInputController->SetIgnoreLookInput(false);
        }
        bOwnLookInputIgnore = false;
        LookInputController.Reset();
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FApp::SetUseFixedTimeStep(OriginalFixed);
        FApp::SetFixedDeltaTime(OriginalDelta);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIST_PRESENTATION_%s frames=%d failures=%d"),
            Failures==0 && FrameIndex==120 ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures);
        FPlatformMisc::RequestExitWithStatus(false,Failures==0 && FrameIndex==120 ? 0 : 1);
    }
    TWeakObjectPtr<APlayerController> LookInputController;
    bool bOwnLookInputIgnore = false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> TargetMonster;
    TArray<FString> Arguments;
    FDelegateHandle TickHandle;
    FVector LeftGuard,RightGuard;
    FRotator GuardControlRotation=FRotator::ZeroRotator;
    FQuat GuardSpine=FQuat::Identity;
    double MaxSpineMotion=0,MaxHandVerticalMotion=0;
    float MaxCameraMotion=0,MaxCameraOffset=0,MaxRenderedViewMotion=0;
    FHandBasis LeftGuardBasis,RightGuardBasis;
    TArray<FFingerMeasure> GuardFingerMeasures;
    FTransform GuardCamera,OriginalCameraTransform,OriginalMeshTransform;
    TWeakObjectPtr<USceneComponent> OriginalCameraParent,OriginalMeshParent;
    TWeakObjectPtr<UClass> OriginalAnimClass;
    FName OriginalCameraSocket,OriginalMeshSocket;
    EAnimationMode::Type OriginalAnimationMode=EAnimationMode::AnimationBlueprint;
    float OriginalWorldFOV=90,OriginalArmsFOV=70,OriginalArmsScale=0.6f;
    bool OriginalFOVEnabled=false,OriginalScaleEnabled=false;
    int32 FrameIndex=0,Failures=0,MontageFrames=0,Attacks=0,PlayedAttacks=0;
    double MaxHandMovement=0,OriginalDelta=1.0/30.0;
    bool OriginalFixed=false,bReentryPending=false;
    int32 ReentryFrames=0;
    bool bViewAudit=false,bCameraAudit=false;
    int32 CameraAuditStage=0,CameraAuditFrames=0;
    int32 ViewStage=0,ViewFrames=0;
    FRotator ViewBaseRotation=FRotator::ZeroRotator;
    FIntPoint OriginalViewportSize=FIntPoint::ZeroValue;
};
static TSharedPtr<FRun> Active;
static void Run(const TArray<FString>& Args)
{
    if (!GEngine) { return; }
    for (const FWorldContext& Context:GEngine->GetWorldContexts())
    {
        if (UWorld* World=Context.World(); World && World->IsGameWorld())
        {
            Active=MakeShared<FRun>();
            Active->Start(World,Args);
            return;
        }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestFistPresentation"),
    TEXT("Records 120 real game frames at 24fps, checks movement, anatomical wrist basis, camera and weapon restoration, then exits. Writes BodyMeleePreview. Optional views adds pitch +/-25 and 2560x1080 stills. Mesh X Z, guard_roll=N hit_roll=N and curl_mcp=N curl_pip=N curl_dip=N tuning is supported."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
#endif
