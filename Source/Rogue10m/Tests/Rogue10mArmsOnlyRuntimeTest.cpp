// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mFistAnimInstance.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mArmsOnlyTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World = InWorld;
        OldFixed = FApp::UseFixedTimeStep(); OldDelta = FApp::GetFixedDeltaTime();
        FTimerHandle Timer;
        InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 4.f, false);
    }
private:
    void Check(bool bOK, const TCHAR* Reason)
    {
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("ARMS_ONLY FAIL frame=%d %s"), FrameIndex, Reason); }
    }
    void Pitch(float Value)
    {
        auto* PC = World->GetFirstPlayerController();
        // The normal view keeps its production pitch limit. Only the explicit -89
        // stress view widens this local fixture, then the next case restores it.
        if (CameraManager.IsValid())
        {
            CameraManager->ViewPitchMin = FMath::IsNearlyEqual(Value, -89.0f) ? FMath::Min(OriginalPitchMin, -89.0f) : OriginalPitchMin;
        }
        ExpectedPitch = Value;
        PC->SetControlRotation(FRotator(Value, PC->GetControlRotation().Yaw, 0));
    }
    void Capture(const TCHAR* Name, int32 Width = 1280)
    {
        const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
            ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
        Check(Size.X == Width && Size.Y == 720, TEXT("unexpected_capture_resolution"));
        if (CameraManager.IsValid() && ExpectedPitch >= OriginalPitchMin)
        {
            Check(FMath::IsNearlyEqual(CameraManager->ViewPitchMin, OriginalPitchMin), TEXT("production_pitch_limit_not_restored_after_stress_view"));
        }
        const double ActualPitch = World->GetFirstPlayerController()->GetControlRotation().GetNormalized().Pitch;
        Check(FMath::Abs(FMath::FindDeltaAngleDegrees(ExpectedPitch, ActualPitch)) < .25,
            TEXT("capture_control_pitch_did_not_match_requested_view"));
        UE_LOG(LogRogue10m, Display, TEXT("ARMS_ONLY CAPTURE %s size=%dx%d mesh=%s pitch=%.1f"),
            Name, Size.X, Size.Y, *GetPathNameSafe(Character->GetFirstPersonMesh()->GetSkeletalMeshAsset()),
            World->GetFirstPlayerController()->GetControlRotation().Pitch);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(OutputFolder, FString(Name) + TEXT(".png")), true, false);
    }
    void CheckActive(const TCHAR* Stage)
    {
        auto* Arms = Character->GetFirstPersonMesh();
        auto* Mesh = Arms->GetSkeletalMeshAsset();
        Check(Character->GetFirstPersonPresentationComponent()->IsFistPresentationActive(), TEXT("presentation_inactive"));
        Check(Mesh && Mesh->GetPathName() == TEXT("/Game/Rogue10m/Character/SK_FirstPersonArms.SK_FirstPersonArms"), TEXT("arms_only_asset_not_active"));
        Check(Arms->bOnlyOwnerSee && !Arms->bOwnerNoSee && Arms->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson,
            TEXT("arms_owner_or_firstperson_render_flags_invalid"));
        Check(Character->GetMesh()->bOwnerNoSee, TEXT("world_body_visible_to_owner"));
        Check(Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()) != nullptr, TEXT("fist_animation_missing"));
        if (Mesh && OriginalMesh.IsValid())
        {
            const auto& A = Mesh->GetRefSkeleton(); const auto& B = OriginalMesh->GetRefSkeleton();
            bool bSame = Mesh->GetSkeleton() == OriginalMesh->GetSkeleton() && A.GetNum() == B.GetNum();
            for (int32 I = 0; bSame && I < A.GetNum(); ++I)
            {
                bSame = A.GetBoneName(I) == B.GetBoneName(I) && A.GetParentIndex(I) == B.GetParentIndex(I)
                    && A.GetRefBonePose()[I].Equals(B.GetRefBonePose()[I]);
            }
            Check(bSame, TEXT("full_reference_skeleton_changed"));
        }
        for (FName Bone : { FName(TEXT("head")), FName(TEXT("spine_03")), FName(TEXT("hand_l")), FName(TEXT("hand_r")) })
        {
            Check(Arms->GetBoneIndex(Bone) != INDEX_NONE && !Arms->GetSocketTransform(Bone).ContainsNaN(), TEXT("head_spine_or_hand_pose_missing"));
        }
        UE_LOG(LogRogue10m, Display, TEXT("ARMS_ONLY ACTIVE stage=%s mesh=%s"), Stage, *GetPathNameSafe(Mesh));
    }
    void CheckRestored()
    {
        auto* Arms = Character->GetFirstPersonMesh();
        Check(Arms->GetSkeletalMeshAsset() == OriginalMesh.Get(), TEXT("original_mesh_not_restored"));
        Check(Arms->OverrideMaterials.Num() == OriginalMaterials.Num(), TEXT("material_override_count_not_restored"));
        for (int32 I = 0; I < FMath::Min(Arms->OverrideMaterials.Num(), OriginalMaterials.Num()); ++I)
        {
            Check(Arms->OverrideMaterials[I] == OriginalMaterials[I].Get(), TEXT("material_override_not_restored"));
        }
        Check(Arms->bOnlyOwnerSee == bOriginalOnlyOwner && Arms->bOwnerNoSee == bOriginalOwnerNoSee
            && Character->GetMesh()->bOwnerNoSee == bOriginalBodyOwnerNoSee, TEXT("original_owner_visibility_not_restored"));
        Check(Arms->GetRelativeTransform().Equals(OriginalTransform), TEXT("original_mesh_transform_not_restored"));
        Check(Arms->GetAnimClass() == OriginalAnimClass.Get() && static_cast<uint8>(Arms->GetAnimationMode()) == OriginalAnimationMode,
            TEXT("original_animation_configuration_not_restored"));
        Check(static_cast<uint8>(Arms->FirstPersonPrimitiveType) == OriginalPrimitiveType, TEXT("original_firstperson_render_type_not_restored"));
        const auto& BoneVisibility = Arms->GetBoneVisibilityStates();
        Check(BoneVisibility.Num() == OriginalBoneVisibility.Num(), TEXT("original_bone_visibility_count_not_restored"));
        for (int32 I = 0; I < FMath::Min(BoneVisibility.Num(), OriginalBoneVisibility.Num()); ++I)
        {
            Check((BoneVisibility[I] == BVS_ExplicitlyHidden) == (OriginalBoneVisibility[I] == BVS_ExplicitlyHidden),
                TEXT("original_explicit_bone_visibility_not_restored"));
        }
    }
    void Prepare()
    {
        auto* PC = World->GetFirstPlayerController();
        Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (PC)
        {
            // Own exactly one ignore-stack entry; scripted control rotations still work.
            LookInputController = PC;
            PC->SetIgnoreLookInput(true);
            bOwnLookInputIgnore = true;
        }
        if (!Character.IsValid()) { Check(false, TEXT("player_character_missing")); Finish(); return; }
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed, TEXT("spawn_not_unarmed"));
        CameraManager = PC->PlayerCameraManager;
        if (CameraManager.IsValid())
        {
            OriginalPitchMin = CameraManager->ViewPitchMin;
            UE_LOG(LogRogue10m, Display, TEXT("ARMS_ONLY VIEW_LIMIT production_min_pitch=%.3f stress_min_pitch=-89"), OriginalPitchMin);
        }
        else { Check(false, TEXT("camera_manager_missing")); Finish(); return; }
        auto* Presentation = Character->GetFirstPersonPresentationComponent();
        auto* Arms = Character->GetFirstPersonMesh();
        if (!Presentation || !Arms || !Character->GetBasicBrawlerComponent()) { Check(false, TEXT("required_component_missing")); Finish(); return; }
        // Capture the real configured mesh through the same production restore path.
        Presentation->bEnableFistPresentation = false; Presentation->RefreshPresentation();
        // Explicit legacy opt-in; full body is now the production default.
        Presentation->bShowFullBody = false;
        OriginalMesh = Arms->GetSkeletalMeshAsset(); OriginalTransform = Arms->GetRelativeTransform();
        OriginalAnimClass = Arms->GetAnimClass(); OriginalAnimationMode = static_cast<uint8>(Arms->GetAnimationMode());
        OriginalPrimitiveType = static_cast<uint8>(Arms->FirstPersonPrimitiveType);
        OriginalBoneVisibility = Arms->GetBoneVisibilityStates();
        for (const auto& Material : Arms->OverrideMaterials) { OriginalMaterials.Add(Material.Get()); }
        bOriginalOnlyOwner = Arms->bOnlyOwnerSee; bOriginalOwnerNoSee = Arms->bOwnerNoSee;
        bOriginalBodyOwnerNoSee = Character->GetMesh()->bOwnerNoSee;
        Check(OriginalMesh.IsValid(), TEXT("original_mesh_missing"));
        // Independent baseline: a broken restore must not establish its own expected result.
        const auto* ClassDefaults = Character->GetClass()->GetDefaultObject<ARogue10mCharacter>();
        const auto* DefaultArms = ClassDefaults ? ClassDefaults->GetFirstPersonMesh() : nullptr;
        Check(DefaultArms && OriginalMesh.Get() == DefaultArms->GetSkeletalMeshAsset(),
            TEXT("restored_source_mesh_does_not_match_character_class_defaults"));
        Presentation->bEnableFistPresentation = true; Presentation->RefreshPresentation();
        CheckActive(TEXT("spawn"));
        Character->SetCanBeDamaged(false); Character->GetRogueAttributeSet()->RestoreVitals();
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            It->ClearAITarget(); if (auto* AI = It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
        }
        GroundZ = Character->GetActorLocation().Z;
        FApp::SetFixedDeltaTime(1.0 / 30.0); FApp::SetUseFixedTimeStep(true);
        OutputFolder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/WindowsEditor/ArmsOnlyPreview"));
        IFileManager::Get().MakeDirectory(*OutputFolder, true);
        PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"));
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self = AsShared()](UWorld* W, ELevelTick, float)
        { if (W == Self->World.Get()) { Self->Frame(); } });
    }
    void Frame()
    {
        if (!Character.IsValid()) { Check(false, TEXT("player_lost")); Finish(); return; }
        auto* Combat = Character->GetCombatComponent();
        auto* Brawler = Character->GetBasicBrawlerComponent();
        if (FrameIndex == 10 || FrameIndex == 310) { Pitch(-89); }
        if (FrameIndex == 30 || FrameIndex == 330 || FrameIndex == 90) { Pitch(-70); }
        if (FrameIndex == 50 || FrameIndex == 350) { Pitch(0); }
        if (FrameIndex == 70 || FrameIndex == 370) { Pitch(70); }
        if (FrameIndex == 20) { Capture(TEXT("16x9_pitch_minus89")); }
        if (FrameIndex == 40) { Capture(TEXT("16x9_pitch_minus70")); }
        if (FrameIndex == 60) { Capture(TEXT("16x9_pitch_zero")); }
        if (FrameIndex == 80) { Capture(TEXT("16x9_pitch_plus70")); }
        if (FrameIndex == 90)
        {
            StartSerial = Brawler->GetSnapshot().AcceptedSerial;
            Combat->HandleAttackPressed(true);
            Check(Brawler->GetSnapshot().Attack == ERogue10mBasicBrawlerAttack::LeftJab, TEXT("downward_left_attack_not_accepted"));
        }
        if (FrameIndex == 91) { Combat->HandleAttackReleased(true); }
        if (FrameIndex == 98) { Combat->HandleAttackPressed(true); Combat->HandleAttackReleased(true); }
        if (FrameIndex == 97) { Capture(TEXT("downward_left_jab")); }
        if (FrameIndex == 109)
        {
            Check(Brawler->GetSnapshot().Attack == ERogue10mBasicBrawlerAttack::RightJab, TEXT("downward_second_attack_not_right_jab"));
            Capture(TEXT("downward_right_jab"));
        }
        if (FrameIndex == 130) { Check(Brawler->GetSnapshot().AcceptedSerial == StartSerial + 2, TEXT("downward_combo_not_executed")); }
        if (FrameIndex == 140) { Combat->HandleAttackPressed(false); }
        if (FrameIndex == 168)
        {
            Check(Brawler->GetSnapshot().Phase == ERogue10mBasicBrawlerPhase::Charging && Brawler->GetSnapshot().ChargeAlpha > .95f,
                TEXT("downward_charge_not_held"));
            Capture(TEXT("downward_charged_guard"));
        }
        if (FrameIndex == 170) { Combat->HandleAttackReleased(false); }
        if (FrameIndex == 183)
        {
            Check(Brawler->GetSnapshot().Attack == ERogue10mBasicBrawlerAttack::RightHook, TEXT("downward_charged_attack_not_right_hook"));
            Capture(TEXT("downward_right_hook"));
        }
        if (FrameIndex == 208) { Check(Brawler->GetSnapshot().AcceptedSerial == StartSerial + 3, TEXT("downward_hook_not_executed")); }
        if (FrameIndex == 210) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpStart")), nullptr); }
        if (FrameIndex == 215) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpEnd")), nullptr); }
        if (FrameIndex == 220) { Capture(TEXT("downward_jump")); }
        if (FrameIndex >= 90 && FrameIndex < 210)
        {
            if (const auto* Fist = Cast<URogue10mFistAnimInstance>(Character->GetFirstPersonMesh()->GetAnimInstance()))
            {
                const FRotator R = Fist->GetBodyMotionRotation();
                Check(!R.ContainsNaN() && !Fist->GetHeadPoseDeltaOffset().ContainsNaN(), TEXT("nonfinite_head_motion"));
                MaxHeadSignal = FMath::Max(MaxHeadSignal, FMath::Max3(FMath::Abs(R.Pitch), FMath::Abs(R.Yaw), FMath::Abs(R.Roll)));
            }
        }
        if (FrameIndex >= 92 && FrameIndex < 210)
        {
            const FTransform CameraTransform = Character->GetFirstPersonCameraComponent()->GetComponentTransform();
            const FVector Left = CameraTransform.InverseTransformPosition(Character->GetFirstPersonMesh()->GetSocketLocation(TEXT("hand_l")));
            const FVector Right = CameraTransform.InverseTransformPosition(Character->GetFirstPersonMesh()->GetSocketLocation(TEXT("hand_r")));
            Check(!Left.ContainsNaN() && !Right.ContainsNaN() && Left.Size() < 500 && Right.Size() < 500,
                TEXT("downward_hand_position_nonfinite_or_out_of_bounds"));
            if (bHavePreviousHands)
            {
                MaxHandStep = FMath::Max(MaxHandStep, FMath::Max(FVector::Dist(Left, PreviousLeft), FVector::Dist(Right, PreviousRight)));
            }
            PreviousLeft = Left; PreviousRight = Right; bHavePreviousHands = true;
        }
        if (FrameIndex >= 210 && FrameIndex < 260)
        {
            bSawJump |= Character->GetCharacterMovement()->IsFalling();
            MaxJumpHeight = FMath::Max(MaxJumpHeight, Character->GetActorLocation().Z - GroundZ);
        }
        if (FrameIndex == 260)
        {
            Check(bSawJump && MaxJumpHeight > 25, TEXT("downward_jump_missing"));
            Check(MaxHeadSignal > .05, TEXT("head_camera_source_motion_missing"));
            Check(MaxHandStep < 20.0, TEXT("downward_hand_pose_teleported_between_frames"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff); CheckRestored();
        }
        if (FrameIndex == 270) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle); CheckActive(TEXT("knuckle")); }
        if (FrameIndex == 280) { Capture(TEXT("downward_knuckle")); }
        if (FrameIndex == 290) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); CheckActive(TEXT("unarmed_return")); }
        if (FrameIndex == 300) { World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes 1720x720w")); }
        if (FrameIndex == 320) { Capture(TEXT("ultrawide_pitch_minus89"), 1720); }
        if (FrameIndex == 340) { Capture(TEXT("ultrawide_pitch_minus70"), 1720); }
        if (FrameIndex == 360) { Capture(TEXT("ultrawide_pitch_zero"), 1720); }
        if (FrameIndex == 380) { Capture(TEXT("ultrawide_pitch_plus70"), 1720); }
        if (FrameIndex == 390) { Finish(); return; }
        ++FrameIndex;
    }
    void Finish()
    {
        if (bOwnLookInputIgnore && LookInputController.IsValid())
        {
            LookInputController->SetIgnoreLookInput(false);
        }
        bOwnLookInputIgnore = false;
        LookInputController.Reset();
        if (CameraManager.IsValid()) { CameraManager->ViewPitchMin = OriginalPitchMin; }
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta);
        UE_LOG(LogRogue10m, Display, TEXT("ARMS_ONLY METRICS head_signal=%.3f jump_height=%.3f max_hand_step=%.3f"), MaxHeadSignal, MaxJumpHeight, MaxHandStep);
        const bool bPassed = Failures == 0 && FrameIndex == 390;
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=ARMS_ONLY_%s frames=%d failures=%d"), bPassed ? TEXT("PASSED") : TEXT("FAILED"), FrameIndex, Failures);
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
    }
    TWeakObjectPtr<APlayerController> LookInputController;
    bool bOwnLookInputIgnore = false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<APlayerCameraManager> CameraManager;
    float OriginalPitchMin = -70.0f;
    TWeakObjectPtr<USkeletalMesh> OriginalMesh;
    TWeakObjectPtr<UClass> OriginalAnimClass;
    TArray<uint8> OriginalBoneVisibility;
    uint8 OriginalAnimationMode = 0, OriginalPrimitiveType = 0;
    TArray<TWeakObjectPtr<UMaterialInterface>> OriginalMaterials;
    FTransform OriginalTransform;
    FVector PreviousLeft = FVector::ZeroVector, PreviousRight = FVector::ZeroVector;
    bool bHavePreviousHands = false;
    double ExpectedPitch = 0, MaxHandStep = 0;
    FString OutputFolder;
    FDelegateHandle TickHandle;
    int32 FrameIndex = 0, Failures = 0, StartSerial = 0;
    bool OldFixed = false, bSawJump = false, bOriginalOnlyOwner = false, bOriginalOwnerNoSee = false, bOriginalBodyOwnerNoSee = false;
    double OldDelta = 1.0 / 30.0, GroundZ = 0, MaxJumpHeight = 0, MaxHeadSignal = 0;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* W = Context.World(); W && W->IsGameWorld()) { Active = MakeShared<FRun>(); Active->Start(W); return; }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestArmsOnly"),
    TEXT("Checks arms-only mesh, downward attacks/jump, restoration, and 16:9/ultrawide views; saves 14 screenshots then exits."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
