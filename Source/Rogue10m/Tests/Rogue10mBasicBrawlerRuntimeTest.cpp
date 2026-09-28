// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include <limits>
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mFistAnimInstance.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mBasicBrawlerTest
{
struct FAttackMeasure
{
    ERogue10mBasicBrawlerAttack Kind=ERogue10mBasicBrawlerAttack::None;
    double LeftTravel=0,RightTravel=0,LeftTravelAtRightPeak=0,MinRightZ=1.e6,MaxRightZ=-1.e6,FirstRightZ=0,FirstTime=-1,PeakTime=0,HeadDegrees=0,ViewDegrees=0;
    double MinRightX=1.e6,MaxRightX=-1.e6,MinRightY=1.e6,MaxRightY=-1.e6,MaxHitTimeError=0;
    int32 DamageSamples=0;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld, bool bSmoothCapture, FString InStance)
    {
        World=InWorld; CaptureStride=bSmoothCapture ? 1 : 2;
        StanceLabel = MoveTemp(InStance);
        Stance = StanceLabel == TEXT("longguard") ? ERogue10mBrawlerStance::LongGuard
            : StanceLabel == TEXT("rooted") ? ERogue10mBrawlerStance::RootedMartial : ERogue10mBrawlerStance::CompactBoxing;
        CaptureFolder = StanceLabel.IsEmpty() ? TEXT("Screenshots/WindowsEditor/BasicBrawlerPreview")
            : FString(TEXT("Screenshots/WindowsEditor/BoxingStances/")) + StanceLabel;
        FTimerHandle Handle;
        InWorld->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.0f,false);
    }
private:
    void Check(bool bOK,const TCHAR* What)
    {
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("BASIC_BRAWLER FAIL frame=%d %s"),FrameIndex,What); }
    }
    FVector Hand(const TCHAR* Bone) const
    {
        return Character->GetFirstPersonCameraComponent()->GetComponentTransform().InverseTransformPosition(
            Character->GetFirstPersonMesh()->GetSocketLocation(Bone));
    }
    void LogArmChains(const TCHAR* Stage, int32 Serial) const
    {
        for (const bool bLeft : {true,false})
        {
            const FVector Upper=Hand(bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
            const FVector Lower=Hand(bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
            const FVector Wrist=Hand(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
            UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER ARM_CHAIN stage=%s frame=%d serial=%d side=%s upper=%s lower=%s hand=%s upper_length=%.3f lower_length=%.3f"),
                Stage,FrameIndex,Serial,bLeft ? TEXT("left") : TEXT("right"),*Upper.ToString(),*Lower.ToString(),*Wrist.ToString(),
                FVector::Distance(Upper,Lower),FVector::Distance(Lower,Wrist));
        }
    }
    void Press(bool bPrimary) { Character->GetCombatComponent()->HandleAttackPressed(bPrimary); }
    void Release(bool bPrimary) { Character->GetCombatComponent()->HandleAttackReleased(bPrimary); }
    void Click(bool bPrimary) { Press(bPrimary); Release(bPrimary); }
    float Health() const { return Monster.IsValid() ? Monster->GetRogueAttributeSet()->GetHealth() : 0; }
    void Prepare()
    {
        auto* PC=World->GetFirstPlayerController();
        Character=PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (PC)
        {
            // Own exactly one ignore-stack entry; scripted control rotations still work.
            LookInputController = PC;
            PC->SetIgnoreLookInput(true);
            bOwnLookInputIgnore = true;
        }
        if (!Character.IsValid()) { Check(false,TEXT("character_missing")); Finish(); return; }
        Check(Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed,TEXT("fresh_spawn_is_not_basic_unarmed"));
        auto* Brawler=Character->GetBasicBrawlerComponent();
        Check(Brawler && Brawler->IsBasicBrawlerActive(),TEXT("basic_brawler_not_active_on_spawn"));
        if (!Brawler) { Finish(); return; }
        auto* Combat=Character->GetCombatComponent();
        const auto* Primary=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        const auto* Special=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
        const auto* Charged=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedSpecial);
        Check(Primary && Special && Charged,TEXT("default_jab_straight_or_hook_asset_missing"));
        Check(Combat->IsAttackSkillUnlocked(Primary) && Combat->IsAttackSkillUnlocked(Special)
            && Combat->IsAttackSkillUnlocked(Charged),TEXT("basic_inputs_locked_on_spawn"));
        Character->SetCanBeDamaged(false);
        Character->GetRogueAttributeSet()->RestoreVitals();
        Character->GetRogueAttributeSet()->SetMinDamageRatio(1.0f);
        Character->GetRogueAttributeSet()->SetMaxDamageRatio(1.0f);
        Character->GetRogueAttributeSet()->SetCriticalChance(0.0f);
        if (auto* Regen=Character->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
        Character->GetCharacterMovement()->StopMovementImmediately();
        PC->SetControlRotation(FRotator(0,PC->GetControlRotation().Yaw,0));
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget();
            if (auto* AI=It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately();
            It->GetCharacterMovement()->DisableMovement();
            if (!Monster.IsValid())
            {
                Monster=*It;
                if (auto* Regen=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
                It->GetRogueAttributeSet()->SetMaxHealth(2000);
                It->GetRogueAttributeSet()->SetHealth(2000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
                It->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*135.0f,false,nullptr,ETeleportType::TeleportPhysics);
                It->SetActorRotation((Character->GetActorLocation()-It->GetActorLocation()).Rotation());
            }
            else { It->SetActorEnableCollision(false); }
        }
        Check(Monster.IsValid(),TEXT("target_missing"));
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime();
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),CaptureFolder),true);
        TickHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (W==Self->World.Get()) {Self->Frame();} });
    }
    void Frame()
    {
        if (!Character.IsValid()) { Check(false,TEXT("character_lost")); Finish(); return; }
        auto* Brawler=Character->GetBasicBrawlerComponent();
        auto* Camera=Character->GetFirstPersonCameraComponent();
        auto* Arms=Character->GetFirstPersonMesh();
        auto* PC=Cast<APlayerController>(Character->GetController());
        if (auto* Fist = Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()))
        {
            Fist->BasicCombatStance = Stance;
            // This fixture measures established guard-relative attack trajectories.
            // Default hands-down transitions are covered separately by TestBrawlerIdleTransition.
            Fist->bEnableRelaxedIdle = false;
        }
        if (FrameIndex==12)
        {
            LeftGuard=Hand(TEXT("hand_l")); RightGuard=Hand(TEXT("hand_r"));
            GuardCamera=Camera->GetRelativeTransform(); GuardControl=PC->GetControlRotation();
            GuardHead=Arms->GetSocketQuaternion(TEXT("head")); GuardSpine=Arms->GetSocketQuaternion(TEXT("spine_03"));
            GroundZ=Character->GetActorLocation().Z; InitialHealth=Health(); PreviousHealth=InitialHealth;
            LogArmChains(TEXT("guard"),0);
            for (int32 I=0; I<2; ++I)
            {
                const FName Thigh = I==0 ? TEXT("thigh_l") : TEXT("thigh_r");
                const FName Calf = I==0 ? TEXT("calf_l") : TEXT("calf_r");
                const FName Foot = I==0 ? TEXT("foot_l") : TEXT("foot_r");
                LegUpper[I] = FVector::Distance(Arms->GetSocketLocation(Thigh),Arms->GetSocketLocation(Calf));
                LegLower[I] = FVector::Distance(Arms->GetSocketLocation(Calf),Arms->GetSocketLocation(Foot));
            }
            UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER STANCE name=%s left=%s right=%s"),
                *StanceLabel, *LeftGuard.ToCompactString(), *RightGuard.ToCompactString());
        }
        if (FrameIndex > 12 && FrameIndex < 200)
        {
            for (int32 I=0; I<2; ++I)
            {
                const FName Thigh = I==0 ? TEXT("thigh_l") : TEXT("thigh_r");
                const FName Calf = I==0 ? TEXT("calf_l") : TEXT("calf_r");
                const FName Foot = I==0 ? TEXT("foot_l") : TEXT("foot_r");
                const double Upper = FVector::Distance(Arms->GetSocketLocation(Thigh),Arms->GetSocketLocation(Calf));
                const double Lower = FVector::Distance(Arms->GetSocketLocation(Calf),Arms->GetSocketLocation(Foot));
                Check(FMath::Abs(Upper-LegUpper[I])<.1 && FMath::Abs(Lower-LegLower[I])<.1, TEXT("stance_stretched_leg_bones"));
            }
        }
        if (FrameIndex==20)
        {
            const float BeforeHit=Health();
            Press(true);
            Check(FMath::IsNearlyEqual(Health(),BeforeHit,0.01f),TEXT("left_jab_hit_before_motion"));
            Check(Brawler->GetSnapshot().AcceptedSerial==1 && Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::LeftJab,
                TEXT("left_jab_did_not_execute_on_press"));
        }
        if (FrameIndex==21) { Release(true); Check(Brawler->GetSnapshot().AcceptedSerial==1,TEXT("primary_release_duplicated_attack")); }
        if (FrameIndex==23 || FrameIndex==24) { Click(true); }
        if (FrameIndex==60)
        {
            Check(Brawler->GetSnapshot().AcceptedSerial==2,TEXT("left_right_buffer_did_not_produce_exactly_two_attacks"));
            Check(Health()<InitialHealth,TEXT("jabs_did_not_damage_target"));
        }
        if (FrameIndex==75) { TapHealth=Health(); Press(false); }
        if (FrameIndex==78)
        {
            Release(false);
            Check(FMath::IsNearlyEqual(Health(),TapHealth,0.01f),TEXT("tap_straight_hit_on_release"));
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightStraight,TEXT("short_release_did_not_select_right_straight"));
        }
        if (FrameIndex==112) { TapDamage=TapHealth-Health(); Check(TapDamage>0,TEXT("tap_straight_did_not_hit")); }
        if (FrameIndex==120) { ChargeHealth=Health(); ChargeStartSerial=Brawler->GetSnapshot().AcceptedSerial; Press(false); }
        if (FrameIndex==150)
        {
            const auto S=Brawler->GetSnapshot();
            Check(S.Phase==ERogue10mBasicBrawlerPhase::Charging && S.ChargeAlpha>.95f,TEXT("full_charge_not_held"));
            Check(S.AcceptedSerial==ChargeStartSerial && FMath::Abs(Health()-ChargeHealth)<0.1f,TEXT("charge_dealt_damage_or_auto_fired"));
            const FQuat C=Camera->GetComponentQuat();
            ChargeYaw=(C.Inverse()*(Arms->GetSocketQuaternion(TEXT("spine_03"))*GuardSpine.Inverse())*C).Rotator().Yaw;
            Check(ChargeYaw>3.0,TEXT("charge_body_did_not_turn_right"));
        }
        if (FrameIndex==156)
        {
            Release(false);
            Check(FMath::IsNearlyEqual(Health(),ChargeHealth,0.01f),TEXT("charged_hook_hit_on_release"));
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightHook,TEXT("hold_release_did_not_select_right_hook"));
        }
        if (FrameIndex==180)
        {
            ChargedDamage=ChargeHealth-Health();
            Check(ChargedDamage>TapDamage,TEXT("charged_hook_not_stronger_than_tap"));
            Check(Brawler->GetSnapshot().AcceptedSerial==4,TEXT("straight_and_hook_not_exactly_once_each"));
        }
        if (FrameIndex==185) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpStart")),nullptr); }
        if (FrameIndex==190) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpEnd")),nullptr); }
        if (FrameIndex==200) { Check(Character->GetCharacterMovement()->IsFalling(),TEXT("airborne_input_fixture_not_falling")); Click(true); Check(Brawler->GetSnapshot().AcceptedSerial==4,TEXT("airborne_primary_consumed_attack")); }
        if (FrameIndex==235)
        {
            Check(bSawJump && MaxJumpHeight>25,TEXT("space_jump_did_not_leave_ground"));
            Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("jump_did_not_land"));
        }
        if (FrameIndex==240) { BeforeCancelSerial=Brawler->GetSnapshot().AcceptedSerial; Press(false); }
        if (FrameIndex==246) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpStart")),nullptr); }
        if (FrameIndex==250) { Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpEnd")),nullptr); }
        if (FrameIndex==290)
        {
            Release(false);
            Check(Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial,TEXT("jump_cancel_released_a_hook"));
            Check(Brawler->GetSnapshot().Phase!=ERogue10mBasicBrawlerPhase::Charging,TEXT("jump_failed_to_cancel_charge"));
        }
        if (FrameIndex==300) { Press(false); }
        if (FrameIndex==308)
        {
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
            Check(!Brawler->IsBasicBrawlerActive(),TEXT("other_weapon_kept_brawler_active"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); Release(false);
            Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("weapon_cancel_kept_charge_state"));
        }
        if (FrameIndex==320)
        {
            // WITH_EDITOR test-only, in-memory fixture: production right punches intentionally cost zero.
            // No package is dirtied or saved; restore on both normal completion and early Finish().
            CostFixture=Character->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
            Check(CostFixture.IsValid(),TEXT("paid_straight_fixture_missing"));
            if (CostFixture.IsValid())
            {
                OriginalResourceCosts=CostFixture->ResourceCosts;
                CostFixture->ResourceCosts={FRogue10mAttackResourceCost(ERogue10mAttackResourceType::Stamina,10.0f)};
            }
            Character->GetRogueAttributeSet()->SetStamina(0); Press(false);
            Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("unaffordable_straight_started_charging"));
        }
        if (FrameIndex==345) { Release(false); }
        if (FrameIndex==350)
        {
            Check(Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial,TEXT("no_stamina_straight_was_accepted"));
            RestoreCostFixture();
            Character->GetRogueAttributeSet()->RestoreVitals();
        }
        if (FrameIndex==365)
        {
            HealthBeforeCancelledHit=Health();
            Press(true);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::LeftJab
                && Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial+1,TEXT("expired_combo_did_not_restart_left"));
        }
        if (FrameIndex==366)
        {
            Release(true);
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
            Click(true); Press(false); Release(false);
        }
        if (FrameIndex==390)
        {
            Check(FMath::IsNearlyEqual(Health(),HealthBeforeCancelledHit,0.01f),TEXT("weapon_cancelled_jab_dealt_delayed_damage"));
            Check(Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial+1,TEXT("cooldown_rejected_input_advanced_sequence"));
            Release(false); Release(false);
            Check(Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial+1,TEXT("repeated_release_attacked"));
        }
        if (FrameIndex==410) { Press(false); }
        if (FrameIndex>=411 && FrameIndex<=420) { struct { float Right=1; float Forward=0; } Move; Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&Move); }
        if (FrameIndex==422) { Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Charging,TEXT("moving_cancelled_charge")); }
        if (FrameIndex==425) { CastChecked<ARogue10mPlayerController>(PC)->ToggleInventory(); }
        if (FrameIndex==430)
        {
            Check(CastChecked<ARogue10mPlayerController>(PC)->IsAnyBlockingWindowVisible(),TEXT("ui_cancel_fixture_not_open"));
            Release(false);
            Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle
                && Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial+1,TEXT("blocking_ui_failed_to_cancel_charge"));
            CastChecked<ARogue10mPlayerController>(PC)->CloseAllBlockingPanels();
        }
        if (FrameIndex==450) { Press(false); }
        if (FrameIndex==460)
        {
            auto* Presentation=Character->GetFirstPersonPresentationComponent();
            Check(Presentation!=nullptr,TEXT("camera_audit_component_missing"));
            if (Presentation)
            {
                const FVector OldOffset=Presentation->GetAppliedCameraMotionOffset();
                const FRotator OldRotation=Presentation->GetAppliedCameraMotionRotation();
                FMinimalViewInfo View; View.Location=FVector(10,20,30); View.Rotation=FRotator(4,5,6);
                const FMinimalViewInfo BaseView=View;
                Presentation->ApplyBodyMotionToView(0,View);
                Check(Presentation->GetAppliedCameraMotionOffset().Equals(OldOffset,0.001)
                    && Presentation->GetAppliedCameraMotionRotation().Equals(OldRotation,0.001),TEXT("zero_dt_advanced_camera_state"));
                const float OriginalScale=Presentation->CameraMotionScale;
                Presentation->CameraMotionScale=0; View=BaseView;
                Presentation->ApplyBodyMotionToView(1.0f/30.0f,View);
                Check(View.Location.Equals(BaseView.Location,0.001) && View.Rotation.Equals(BaseView.Rotation,0.001)
                    && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero()
                    && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),TEXT("basic_camera_scale_zero_failed"));
                Presentation->CameraMotionScale=OriginalScale; View=BaseView;
                Presentation->ApplyBodyMotionToView(1.0f,View);
                Check(!View.Location.ContainsNaN() && !View.Rotation.ContainsNaN()
                    && FVector::Distance(View.Location,BaseView.Location) <= (Presentation->IsFullBodyPresentationActive() ? Presentation->MaxHeadCameraOffset + .01f : 3.01f),TEXT("camera_hitch_unbounded"));
                View=BaseView;
                Presentation->ApplyBodyMotionToView(std::numeric_limits<float>::quiet_NaN(),View);
                Check(View.Location.Equals(BaseView.Location,0.001) && View.Rotation.Equals(BaseView.Rotation,0.001)
                    && Presentation->GetAppliedCameraMotionOffset().IsNearlyZero()
                    && Presentation->GetAppliedCameraMotionRotation().IsNearlyZero(),TEXT("nonfinite_dt_camera_not_reset"));
                UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER CAMERA_AUDIT zero_dt scale_zero hitch nonfinite_dt checked"));
            }
        }
        if (FrameIndex==470) { Character->Die(); }
        if (FrameIndex==472)
        {
            Release(false); Click(true);
            Check(Character->IsDead() && Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle,
                TEXT("death_did_not_cancel_basic_charge"));
            Check(Brawler->GetSnapshot().AcceptedSerial==BeforeCancelSerial+1,TEXT("death_or_weapon_cancel_accepted_an_attack"));
        }
        const auto S=Brawler->GetSnapshot();
        if (FrameIndex>=13 && FrameIndex<300)
        {
            const FVector CurrentLeft=Hand(TEXT("hand_l")), CurrentRight=Hand(TEXT("hand_r"));
            Check(!CurrentLeft.ContainsNaN() && !CurrentRight.ContainsNaN(),TEXT("nonfinite_hand_pose"));
            if (bHasPreviousHands)
            {
                MaxHandStep=FMath::Max(MaxHandStep,FMath::Max(FVector::Distance(CurrentLeft,PreviousLeft),FVector::Distance(CurrentRight,PreviousRight)));
            }
            PreviousLeft=CurrentLeft; PreviousRight=CurrentRight; bHasPreviousHands=true;
            if (!bLoggedAimFailure && (!Camera->GetRelativeTransform().Equals(GuardCamera,0.01f)
                || !PC->GetControlRotation().Equals(GuardControl,0.01f)))
            {
                bLoggedAimFailure = true;
                const FTransform CurrentCamera = Camera->GetRelativeTransform();
                const FRotator ControlDelta = (PC->GetControlRotation() - GuardControl).GetNormalized();
                const FRotator CameraDelta = (CurrentCamera.Rotator() - GuardCamera.Rotator()).GetNormalized();
                UE_LOG(LogRogue10m, Error, TEXT("BASIC_BRAWLER FIRST_AIM_FAILURE frame=%d guard_control=%s current_control=%s control_delta=%s guard_camera=%s current_camera=%s camera_rotation_delta=%s camera_location_delta=%s look_ignored=%d"),
                    FrameIndex, *GuardControl.ToString(), *PC->GetControlRotation().ToString(), *ControlDelta.ToString(),
                    *GuardCamera.ToString(), *CurrentCamera.ToString(), *CameraDelta.ToString(),
                    *(CurrentCamera.GetLocation() - GuardCamera.GetLocation()).ToString(), PC->IsLookInputIgnored());
            }
            Check(Camera->GetRelativeTransform().Equals(GuardCamera,0.01f),TEXT("presentation_changed_gameplay_camera"));
            Check(PC->GetControlRotation().Equals(GuardControl,0.01f),TEXT("presentation_changed_aim_input"));
            if (S.Phase==ERogue10mBasicBrawlerPhase::Attacking)
            {
                auto& M=Attacks.FindOrAdd(S.AcceptedSerial); M.Kind=S.Attack;
                M.LeftTravel=FMath::Max(M.LeftTravel,FVector::Distance(LeftGuard,Hand(TEXT("hand_l"))));
                const double RightNow=FVector::Distance(RightGuard,Hand(TEXT("hand_r")));
                // Compare simultaneous hands at the second punch's peak, excluding the previous jab's recovery.
                if (RightNow>M.RightTravel) { M.RightTravel=RightNow; M.LeftTravelAtRightPeak=FVector::Distance(LeftGuard,Hand(TEXT("hand_l"))); }
                if (M.FirstTime<0) { M.FirstTime=S.AttackElapsed; M.FirstRightZ=Hand(TEXT("hand_r")).Z; }
                if (Hand(TEXT("hand_r")).Z>M.MaxRightZ) { M.PeakTime=S.AttackElapsed; }
                M.HeadDegrees=FMath::Max(M.HeadDegrees,FMath::RadiansToDegrees(GuardHead.AngularDistance(Arms->GetSocketQuaternion(TEXT("head")))));
                if (PC->PlayerCameraManager)
                {
                    const auto D=(PC->PlayerCameraManager->GetCameraRotation()-Camera->GetComponentRotation()).GetNormalized();
                    M.ViewDegrees=FMath::Max(M.ViewDegrees,FMath::Max3(FMath::Abs(D.Pitch),FMath::Abs(D.Yaw),FMath::Abs(D.Roll)));
                }
                // Camera-local axes separate forward extension (X), horizontal sweep (Y), and lift (Z).
                M.MinRightX=FMath::Min(M.MinRightX,CurrentRight.X); M.MaxRightX=FMath::Max(M.MaxRightX,CurrentRight.X);
                M.MinRightY=FMath::Min(M.MinRightY,CurrentRight.Y); M.MaxRightY=FMath::Max(M.MaxRightY,CurrentRight.Y);
                M.MinRightZ=FMath::Min(M.MinRightZ,CurrentRight.Z); M.MaxRightZ=FMath::Max(M.MaxRightZ,CurrentRight.Z);
                if (Health()<PreviousHealth-0.01f)
                {
                    ++M.DamageSamples;
                    LogArmChains(TEXT("damage"),S.AcceptedSerial);
                    const double ExpectedHit=S.AttackDuration*S.HitFraction;
                    const double Error=FMath::Abs(S.AttackElapsed-ExpectedHit);
                    M.MaxHitTimeError=FMath::Max(M.MaxHitTimeError,Error);
                    Check(Error<=0.10,TEXT("actual_damage_not_aligned_with_authored_apex"));
                    UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER HIT serial=%d kind=%d elapsed=%.4f expected=%.4f error=%.4f"),
                        S.AcceptedSerial,static_cast<int32>(S.Attack),S.AttackElapsed,ExpectedHit,Error);
                }
            }
            PreviousHealth=Health();
            MaxHeadDegrees=FMath::Max(MaxHeadDegrees,FMath::RadiansToDegrees(GuardHead.AngularDistance(Arms->GetSocketQuaternion(TEXT("head")))));
            if (auto* Fist=Cast<URogue10mFistAnimInstance>(Arms->GetAnimInstance()))
            {
                const FRotator Motion=Fist->GetBodyMotionRotation();
                if (FrameIndex>=185 && FrameIndex<235)
                {
                    const bool bFalling=Character->GetCharacterMovement()->IsFalling();
                    if (bWasFalling && !bFalling) { ++LandingCount; }
                    bWasFalling=bFalling;
                    if (bFalling) { JumpAirResponse=FMath::Max(JumpAirResponse,static_cast<double>(Fist->GetJumpPresentationWeight())); }
                    else if (bSawJump) { JumpLandingResponse=FMath::Max(JumpLandingResponse,static_cast<double>(Fist->GetJumpPresentationWeight())); }
                }
                MaxHeadCameraSignal=FMath::Max(MaxHeadCameraSignal,FMath::Max3(FMath::Abs(Motion.Pitch),FMath::Abs(Motion.Yaw),FMath::Abs(Motion.Roll)));
            }
            if (PC->PlayerCameraManager)
            {
                const auto Delta=(PC->PlayerCameraManager->GetCameraRotation()-Camera->GetComponentRotation()).GetNormalized();
                MaxViewDegrees=FMath::Max(MaxViewDegrees,FMath::Max3(FMath::Abs(Delta.Pitch),FMath::Abs(Delta.Yaw),FMath::Abs(Delta.Roll)));
            }
            if (FrameIndex>=185 && FrameIndex<235)
            {
                bSawJump|=Character->GetCharacterMovement()->IsFalling();
                MaxJumpHeight=FMath::Max(MaxJumpHeight,Character->GetActorLocation().Z-GroundZ);
            }
            UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER FRAME n=%d serial=%d phase=%d attack=%d charge=%.3f elapsed=%.3f left=%s right=%s hp=%.1f"),
                FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Phase),static_cast<int32>(S.Attack),S.ChargeAlpha,S.AttackElapsed,
                *Hand(TEXT("hand_l")).ToString(),*Hand(TEXT("hand_r")).ToString(),Health());
        }
        if (FrameIndex<300 && FrameIndex%CaptureStride==0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),CaptureFolder,
                FString::Printf(TEXT("frame_%04d.png"),FrameIndex/CaptureStride)),true,false);
        }
        if (FrameIndex==480)
        {
            Check(Attacks.Contains(1) && Attacks.Contains(2) && Attacks.Contains(3) && Attacks.Contains(4),TEXT("missing_rendered_attacks"));
            if (Attacks.Contains(1)) { const auto& M=Attacks[1]; Check(M.Kind==ERogue10mBasicBrawlerAttack::LeftJab && M.LeftTravel>M.RightTravel+2 && M.HeadDegrees>.2 && M.ViewDegrees>.05,TEXT("first_attack_not_visually_left")); }
            if (Attacks.Contains(2)) { const auto& M=Attacks[2]; Check(M.Kind==ERogue10mBasicBrawlerAttack::RightJab && M.RightTravel>M.LeftTravelAtRightPeak+2 && M.HeadDegrees>.2 && M.ViewDegrees>.05,TEXT("second_attack_not_visually_right")); }
            if (Attacks.Contains(3))
            {
                const auto& M=Attacks[3];
                Check(M.Kind==ERogue10mBasicBrawlerAttack::RightStraight && M.MaxRightX-RightGuard.X>8
                    && M.MaxRightX-M.MinRightX>M.MaxRightZ-M.MinRightZ,TEXT("right_straight_forward_extension_missing"));
            }
            if (Attacks.Contains(4))
            {
                const auto& M=Attacks[4];
                const double Lateral=M.MaxRightY-M.MinRightY, Vertical=M.MaxRightZ-M.MinRightZ;
                Check(M.Kind==ERogue10mBasicBrawlerAttack::RightHook && Lateral>15 && Lateral>Vertical*1.3,
                    TEXT("right_hook_horizontal_sweep_missing"));
                if (Attacks.Contains(3))
                {
                    const auto& Straight=Attacks[3];
                    Check(Lateral>Straight.MaxRightY-Straight.MinRightY+5,TEXT("hook_path_not_distinct_from_straight"));
                }
            }
            for (int32 I : {1,2,3,4})
            {
                if (Attacks.Contains(I)) { Check(Attacks[I].DamageSamples==1,TEXT("punch_did_not_have_exactly_one_timed_damage_sample")); }
            }
            Check(JumpAirResponse>.05 && JumpLandingResponse>.05 && LandingCount==1,TEXT("jump_air_or_single_landing_response_missing"));
            Check(MaxHeadDegrees>.2 && MaxHeadCameraSignal>.05 && MaxViewDegrees>.05,TEXT("head_and_camera_response_missing"));
            for (const auto& Pair : Attacks)
            {
                const auto& M=Pair.Value;
                UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER ATTACK serial=%d kind=%d left=%.2f right=%.2f forward_x_span=%.2f lateral_y_span=%.2f vertical_z_span=%.2f hit_samples=%d hit_error=%.4f"),
                    Pair.Key,static_cast<int32>(M.Kind),M.LeftTravel,M.RightTravel,M.MaxRightX-M.MinRightX,
                    M.MaxRightY-M.MinRightY,M.MaxRightZ-M.MinRightZ,M.DamageSamples,M.MaxHitTimeError);
            }
            UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER METRICS tap_damage=%.1f charged_damage=%.1f charge_yaw=%.2f jump_height=%.2f head_deg=%.2f source_camera_deg=%.2f rendered_deg=%.2f air_response=%.2f landing_response=%.2f landings=%d"),TapDamage,ChargedDamage,ChargeYaw,MaxJumpHeight,MaxHeadDegrees,MaxHeadCameraSignal,MaxViewDegrees,JumpAirResponse,JumpLandingResponse,LandingCount);
            Check(MaxHandStep<20.0,TEXT("hand_pose_teleported_between_frames"));
            UE_LOG(LogRogue10m,Display,TEXT("BASIC_BRAWLER CONTINUITY max_hand_step_cm=%.3f capture_fps=%d"),MaxHandStep,30/CaptureStride);
            Finish(); return;
        }
        ++FrameIndex;
    }
    void RestoreCostFixture()
    {
        if (CostFixture.IsValid()) { CostFixture->ResourceCosts=OriginalResourceCosts; }
        CostFixture.Reset(); OriginalResourceCosts.Reset();
    }
    void Finish()
    {
        if (bOwnLookInputIgnore && LookInputController.IsValid())
        {
            LookInputController->SetIgnoreLookInput(false);
        }
        bOwnLookInputIgnore = false;
        LookInputController.Reset();
        RestoreCostFixture();
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=BASIC_BRAWLER_%s frames=%d failures=%d"),Failures==0 && FrameIndex==480 ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures);
        FPlatformMisc::RequestExitWithStatus(false,Failures==0 && FrameIndex==480 ? 0 : 1);
    }
    TWeakObjectPtr<URogue10mAttackSkillData> CostFixture;
    TArray<FRogue10mAttackResourceCost> OriginalResourceCosts;
    TWeakObjectPtr<APlayerController> LookInputController;
    bool bOwnLookInputIgnore = false;
    bool bLoggedAimFailure = false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> Monster;
    FDelegateHandle TickHandle;
    TMap<int32,FAttackMeasure> Attacks;
    FVector LeftGuard,RightGuard,PreviousLeft,PreviousRight;
    bool bHasPreviousHands=false;
    double MaxHandStep=0;
    FTransform GuardCamera;
    FRotator GuardControl;
    FQuat GuardHead,GuardSpine;
    int32 CaptureStride=2;
    FString StanceLabel, CaptureFolder;
    ERogue10mBrawlerStance Stance = ERogue10mBrawlerStance::CompactBoxing;
    double LegUpper[2] = {}, LegLower[2] = {};
    int32 FrameIndex=0,Failures=0,ChargeStartSerial=0,BeforeCancelSerial=0;
    bool OldFixed=false,bSawJump=false,bWasFalling=false;
    int32 LandingCount=0;
    double JumpAirResponse=0,JumpLandingResponse=0;
    double OldDelta=1.0/30.0,GroundZ=0,MaxJumpHeight=0,MaxHeadDegrees=0,MaxHeadCameraSignal=0,MaxViewDegrees=0,ChargeYaw=0;
    float PreviousHealth=0,InitialHealth=0,TapHealth=0,TapDamage=0,ChargeHealth=0,ChargedDamage=0,HealthBeforeCancelledHit=0;
};
static TSharedPtr<FRun> Active;
static void Run(const TArray<FString>& Args)
{
    if (!GEngine) { return; }
    for (const FWorldContext& C:GEngine->GetWorldContexts())
    { if (UWorld* W=C.World();W && W->IsGameWorld()) { Active=MakeShared<FRun>(); Active->Start(W,Args.Contains(TEXT("smooth")), Args.Contains(TEXT("longguard")) ? TEXT("longguard")
        : Args.Contains(TEXT("rooted")) ? TEXT("rooted") : Args.Contains(TEXT("boxing")) ? TEXT("boxing") : TEXT("")); return; } }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBasicBrawler"),TEXT("Verifies fresh basic class left/right jabs, tap right straight, charged right hook, head camera, jump and interruption; records 150 frames (smooth: 300 at 30fps); optional boxing/longguard/rooted stance, then exits."),FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
#endif
