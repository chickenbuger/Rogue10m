// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
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
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mFistAnimInstance.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mBrawlerIdleTransitionTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld, bool bLookDown, bool bReentry, bool bNoCapture)
    {
        World = InWorld; bCapture = !bNoCapture; bDownView = bLookDown; bTestReentry = bReentry; TotalFrames = bReentry ? 510 : 450;
        FTimerHandle Timer;
        InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 4.f, false);
    }
private:
    void Check(bool bCondition, const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("BRAWLER_IDLE FAIL frame=%d %s"), FrameIndex, Message); }
    }
    FVector Hand(const TCHAR* Bone) const
    {
        return Character->GetFirstPersonCameraComponent()->GetComponentTransform().InverseTransformPosition(
            Character->GetFirstPersonMesh()->GetSocketLocation(Bone));
    }
    float Health() const { return Monster.IsValid() ? Monster->GetRogueAttributeSet()->GetHealth() : 0.f; }
    void Press(bool bPrimary) { Character->GetCombatComponent()->HandleAttackPressed(bPrimary); }
    void Release(bool bPrimary) { Character->GetCombatComponent()->HandleAttackReleased(bPrimary); }
    void Click(bool bPrimary) { Press(bPrimary); Release(bPrimary); }
    void Prepare()
    {
        APlayerController* PC = World.IsValid() ? World->GetFirstPlayerController() : nullptr;
        Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        if (!Character.IsValid()) { Check(false, TEXT("character_missing")); Finish(); return; }
        Controller = PC; SavedControl = PC->GetControlRotation(); PC->SetIgnoreLookInput(true); bOwnLookIgnore = true;
        Character->GetCharacterMovement()->StopMovementImmediately();
        PC->SetControlRotation(FRotator(0.f, SavedControl.Yaw, 0.f));
        BaseControl = PC->GetControlRotation();
        BaseCamera = Character->GetFirstPersonCameraComponent()->GetRelativeTransform();
        SavedCanDamage = Character->CanBeDamaged(); Character->SetCanBeDamaged(false);
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed, TEXT("spawn_not_unarmed"));
        Check(Character->GetFirstPersonPresentationComponent()->IsFullBodyPresentationActive(), TEXT("fullbody_not_active"));
        auto* Fist = Cast<URogue10mFistAnimInstance>(Character->GetFirstPersonMesh()->GetAnimInstance());
        Check(Fist && Fist->bEnableRelaxedIdle, TEXT("relaxed_idle_not_enabled_by_default"));
        if (!Fist) { Finish(); return; }
        Character->GetRogueAttributeSet()->RestoreVitals();
        Character->GetRogueAttributeSet()->SetMinDamageRatio(1.f); Character->GetRogueAttributeSet()->SetMaxDamageRatio(1.f);
        Character->GetRogueAttributeSet()->SetCriticalChance(0.f);
        if (auto* Regen = Character->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            It->ClearAITarget();
            if (auto* AI = It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
            if (!Monster.IsValid())
            {
                Monster = *It;
                if (auto* Regen = It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
                It->GetRogueAttributeSet()->SetMaxHealth(2000); It->GetRogueAttributeSet()->SetHealth(2000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                It->SetActorLocation(Character->GetActorLocation() + Character->GetActorForwardVector()*135.f, false, nullptr, ETeleportType::TeleportPhysics);
                It->SetActorRotation((Character->GetActorLocation()-It->GetActorLocation()).Rotation());
            }
            else { It->SetActorEnableCollision(false); }
        }
        Check(Monster.IsValid(), TEXT("target_missing"));
        InitialHealth = PreviousHealth = Health();
        OldFixed = FApp::UseFixedTimeStep(); OldDelta = FApp::GetFixedDeltaTime(); bOwnFixedTime = true;
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        Output = FPaths::Combine(FPaths::ProjectSavedDir(), bDownView ? TEXT("Screenshots/WindowsEditor/BrawlerIdleTransitionLookDown") : (bTestReentry ? TEXT("Screenshots/WindowsEditor/BrawlerIdleTransitionReentry") : TEXT("Screenshots/WindowsEditor/BrawlerIdleTransition")));
        IFileManager::Get().MakeDirectory(*Output, true);
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (W==Self->World.Get()) { Self->Frame(); } });
        UE_LOG(LogRogue10m, Display, TEXT("BRAWLER_IDLE TIMELINE idle=[0,45) jabs=45/48 straight=180/183 charge=[300,345) idle_checks=40/165/285/440 frames=%d fps=30 lookdown=%d reentry=%d"), TotalFrames, bDownView, bTestReentry);
    }
    void CheckRest(URogue10mFistAnimInstance* Fist, const TCHAR* Stage)
    {
        const FVector Left=Hand(TEXT("hand_l")), Right=Hand(TEXT("hand_r"));
        const double LeftError=FVector::Distance(Left,IdleLeft), RightError=FVector::Distance(Right,IdleRight);
        Check(Fist->GetCombatReadyWeight()<.05f, TEXT("combat_weight_did_not_return_to_idle"));
        Check(LeftError<5.0 && RightError<5.0, TEXT("hands_did_not_return_to_relaxed_idle"));
        UE_LOG(LogRogue10m, Display, TEXT("BRAWLER_IDLE REST stage=%s frame=%d weight=%.4f left_error=%.3f right_error=%.3f"),
            Stage,FrameIndex,Fist->GetCombatReadyWeight(),LeftError,RightError);
    }
    void Frame()
    {
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_lost")); Finish(); return; }
        // Weapon changes defer animation initialization/evaluation. Allow real game
        // ticks rather than reading a newly assigned AnimInstance before PreUpdate.
        if (FrameIndex>TotalFrames)
        {
            if (FrameIndex==TotalFrames+1) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); }
            if (FrameIndex==TotalFrames+3)
            {
                const auto* RestoredFist=Cast<URogue10mFistAnimInstance>(Character->GetFirstPersonMesh()->GetAnimInstance());
                Check(RestoredFist && RestoredFist->GetCombatReadyWeight()<.05f,TEXT("weapon_switch_leaked_ready_state"));
                Check(Character->GetFirstPersonPresentationComponent()->IsFullBodyPresentationActive(),TEXT("weapon_switch_lost_fullbody"));
                UE_LOG(LogRogue10m,Display,TEXT("BRAWLER_IDLE WEAPON_RESTORE frame=%d ready=%.5f"),FrameIndex,RestoredFist ? RestoredFist->GetCombatReadyWeight() : -1.f);
                Finish(); return;
            }
            ++FrameIndex; return;
        }
        auto* Mesh=Character->GetFirstPersonMesh();
        auto* Fist=Cast<URogue10mFistAnimInstance>(Mesh->GetAnimInstance());
        auto* Brawler=Character->GetBasicBrawlerComponent();
        if (!Fist || !Brawler) { Check(false,TEXT("animation_or_brawler_lost")); Finish(); return; }
        if (bDownView && (FrameIndex==20 || FrameIndex==35 || FrameIndex==TotalFrames-9))
        {
            BaseControl.Pitch = FrameIndex==35 ? 0.f : -65.f;
            Controller->SetControlRotation(BaseControl);
        }
        if (FrameIndex==40)
        {
            IdleLeft=Hand(TEXT("hand_l")); IdleRight=Hand(TEXT("hand_r"));
            Check(Fist->GetCombatReadyWeight()<.05f,TEXT("initial_pose_not_relaxed"));
            IdleLeftShoulder=Hand(TEXT("upperarm_l")); IdleRightShoulder=Hand(TEXT("upperarm_r"));
            // Check real anatomical lowering, not only the implementation's weight value.
            Check(IdleLeft.Z<IdleLeftShoulder.Z-15 && IdleRight.Z<IdleRightShoulder.Z-15,TEXT("idle_hands_not_below_shoulders"));
            for (int32 Side=0; Side<2; ++Side)
            {
                const FVector Upper=Hand(Side==0 ? TEXT("upperarm_l") : TEXT("upperarm_r"));
                const FVector Lower=Hand(Side==0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
                const FVector Wrist=Hand(Side==0 ? TEXT("hand_l") : TEXT("hand_r"));
                ArmUpperLength[Side]=FVector::Distance(Upper,Lower);
                ArmLowerLength[Side]=FVector::Distance(Lower,Wrist);
            }
            UE_LOG(LogRogue10m, Display, TEXT("BRAWLER_IDLE INITIAL left=%s right=%s"),*IdleLeft.ToString(),*IdleRight.ToString());
        }
        if (FrameIndex==45) { Click(true); Check(Brawler->GetSnapshot().AcceptedSerial==1,TEXT("left_press_not_accepted")); }
        if (FrameIndex==48) { Click(true); }
        if (FrameIndex==95) { Check(Brawler->GetSnapshot().AcceptedSerial==2,TEXT("left_right_combo_not_exactly_two")); }
        if (FrameIndex==165) { CheckRest(Fist,TEXT("after_jabs")); }
        if (FrameIndex==180) { TapStartHealth=Health(); Press(false); }
        if (FrameIndex==183)
        {
            Release(false);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightStraight,TEXT("tap_not_right_straight"));
            Check(FMath::IsNearlyEqual(Health(),TapStartHealth,.01f),TEXT("straight_hit_at_release_before_pose"));
        }
        if (FrameIndex==240) { TapDamage=TapStartHealth-Health(); Check(TapDamage>0,TEXT("straight_no_damage")); }
        if (FrameIndex==285) { CheckRest(Fist,TEXT("after_straight")); }
        if (FrameIndex==300) { HookStartHealth=Health(); Press(false); }
        if (FrameIndex==335)
        {
            Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Charging && Brawler->GetSnapshot().ChargeAlpha>.95f,TEXT("charge_not_held"));
            Check(Fist->GetCombatReadyWeight()>.9f,TEXT("charge_not_combat_ready"));
            Check(FMath::IsNearlyEqual(Health(),HookStartHealth,.01f),TEXT("charge_dealt_damage"));
        }
        if (FrameIndex==345)
        {
            Release(false);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightHook,TEXT("hold_not_right_hook"));
        }
        if (FrameIndex==410) { Check(HookStartHealth-Health()>TapDamage,TEXT("charged_hook_not_stronger")); }
        if ((!bTestReentry && FrameIndex==440) || (bTestReentry && FrameIndex==500)) { CheckRest(Fist,TEXT("final_idle")); }
        const auto Snapshot=Brawler->GetSnapshot();
        const FVector Left=Hand(TEXT("hand_l")), Right=Hand(TEXT("hand_r"));
        const float Ready=Fist->GetCombatReadyWeight();
        if (bTestReentry && !bReentryTriggered && FrameIndex>380 && FrameIndex<440
            && Ready>.2f && Ready<.8f && Ready<PreviousReady)
        {
            bReentryTriggered=true; ReentryFrame=FrameIndex; Click(true);
            Check(Brawler->GetSnapshot().AcceptedSerial==5,TEXT("lowering_reentry_input_not_accepted"));
            UE_LOG(LogRogue10m,Display,TEXT("BRAWLER_IDLE REENTRY frame=%d start_weight=%.4f"),FrameIndex,Ready);
        }
        Check(FMath::IsFinite(Ready) && Ready>=0.f && Ready<=1.f && !Left.ContainsNaN() && !Right.ContainsNaN(),TEXT("invalid_blend_or_hand_pose"));
        // Scripted look input rotates the camera through its existing pawn-control path. Its mount must stay fixed.
        Check(Character->GetFirstPersonCameraComponent()->GetRelativeLocation().Equals(BaseCamera.GetLocation(),.01f)
            && Character->GetFirstPersonCameraComponent()->GetRelativeScale3D().Equals(BaseCamera.GetScale3D(),.001f),TEXT("idle_changed_gameplay_camera_mount"));
        Check(Controller->GetControlRotation().Equals(BaseControl,.01f),TEXT("idle_changed_control_rotation"));
        if (FrameIndex>40 && (!bDownView || FrameIndex<TotalFrames-9))
        {
            const FQuat LeftWrist=Mesh->GetSocketQuaternion(TEXT("hand_l")), RightWrist=Mesh->GetSocketQuaternion(TEXT("hand_r"));
            double WristStep=0, HandStep=0;
            if (bPreviousValid)
            {
                WristStep=FMath::Max(FMath::RadiansToDegrees(LeftWrist.AngularDistance(PreviousLeftWrist)),FMath::RadiansToDegrees(RightWrist.AngularDistance(PreviousRightWrist)));
                HandStep=FMath::Max(FVector::Distance(Left,PreviousLeft),FVector::Distance(Right,PreviousRight));
                MaxWristStepDegrees=FMath::Max(MaxWristStepDegrees,WristStep);
                if (WristStep>35.0) { UE_LOG(LogRogue10m,Display,TEXT("BRAWLER_IDLE WRIST_FAST frame=%d step_deg=%.4f hand_step=%.4f ready=%.4f"),FrameIndex,WristStep,HandStep,Ready); }
                MaxHandStep=FMath::Max(MaxHandStep,FMath::Max(FVector::Distance(Left,PreviousLeft),FVector::Distance(Right,PreviousRight)));
                if (Ready>.01f && Ready<.99f)
                {
                    if (Ready>PreviousReady) { ++RiseIntermediateFrames; }
                    if (Ready<PreviousReady) { ++FallIntermediateFrames; }
                }
            }
            if (bReentryTriggered && FrameIndex>=ReentryFrame && FrameIndex<=ReentryFrame+6)
            {
                UE_LOG(LogRogue10m,Display,TEXT("BRAWLER_IDLE REENTRY_CONTINUITY frame=%d relative_frame=%d ready=%.5f previous_ready=%.5f hand_step=%.4f wrist_step_deg=%.4f left_quat=%s right_quat=%s"),
                    FrameIndex,FrameIndex-ReentryFrame,Ready,PreviousReady,HandStep,WristStep,*LeftWrist.ToString(),*RightWrist.ToString());
            }
            PreviousLeft=Left; PreviousRight=Right; PreviousReady=Ready;
            PreviousLeftWrist=LeftWrist; PreviousRightWrist=RightWrist; bPreviousValid=true;
            for (int32 Side=0; Side<2; ++Side)
            {
                const FVector Upper=Hand(Side==0 ? TEXT("upperarm_l") : TEXT("upperarm_r"));
                const FVector Lower=Hand(Side==0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
                const FVector Wrist=Hand(Side==0 ? TEXT("hand_l") : TEXT("hand_r"));
                const double UpperError=FMath::Abs(FVector::Distance(Upper,Lower)-ArmUpperLength[Side]);
                const double LowerError=FMath::Abs(FVector::Distance(Lower,Wrist)-ArmLowerLength[Side]);
                MaxArmLengthError=FMath::Max(MaxArmLengthError,FMath::Max(UpperError,LowerError));
                Check(UpperError<.15 && LowerError<.15,TEXT("idle_transition_stretched_arm_bones"));
            }
            MaxLeftLift=FMath::Max(MaxLeftLift,Left.Z-IdleLeft.Z); MaxRightLift=FMath::Max(MaxRightLift,Right.Z-IdleRight.Z);
            if (Snapshot.Phase==ERogue10mBasicBrawlerPhase::Attacking) { ObservedAttacks.FindOrAdd(Snapshot.AcceptedSerial)=Snapshot.Attack; }
            if (Health()<PreviousHealth-.01f)
            {
                ++DamageEvents;
                Check(Ready>.99f,TEXT("damage_before_combat_ready"));
                Check(Snapshot.Phase==ERogue10mBasicBrawlerPhase::Attacking,TEXT("damage_outside_attack"));
                const float Error=FMath::Abs(Snapshot.AttackElapsed-Snapshot.AttackDuration*Snapshot.HitFraction);
                MaxHitError=FMath::Max(MaxHitError,Error);
                Check(Error<=.1f,TEXT("damage_drifted_from_attack_apex"));
            }
        }
        PreviousHealth=Health();
        UE_LOG(LogRogue10m, Display, TEXT("BRAWLER_IDLE FRAME frame=%d ready=%.4f serial=%d phase=%d left=%s right=%s health=%.2f"),
            FrameIndex,Ready,Snapshot.AcceptedSerial,static_cast<int32>(Snapshot.Phase),*Left.ToString(),*Right.ToString(),Health());
        if (bCapture && FrameIndex<TotalFrames) { const bool bIdleInspection=bDownView && ((FrameIndex>=20 && FrameIndex<35) || FrameIndex>=TotalFrames-9); FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),!bIdleInspection,false); }
        if (FrameIndex==TotalFrames)
        {
            const int32 ExpectedAttacks=bTestReentry ? 5 : 4;
            Check(Snapshot.AcceptedSerial==ExpectedAttacks && ObservedAttacks.Num()==ExpectedAttacks,TEXT("attack_count_changed"));
            if (bTestReentry) { Check(bReentryTriggered && ObservedAttacks.FindRef(5)==ERogue10mBasicBrawlerAttack::LeftJab,TEXT("lowering_reentry_missing")); }
            Check(ObservedAttacks.FindRef(1)==ERogue10mBasicBrawlerAttack::LeftJab && ObservedAttacks.FindRef(2)==ERogue10mBasicBrawlerAttack::RightJab
                && ObservedAttacks.FindRef(3)==ERogue10mBasicBrawlerAttack::RightStraight && ObservedAttacks.FindRef(4)==ERogue10mBasicBrawlerAttack::RightHook,TEXT("attack_mapping_changed"));
            Check(DamageEvents==ExpectedAttacks,TEXT("not_exactly_one_hit_per_attack"));
            Check(RiseIntermediateFrames>=2 && FallIntermediateFrames>=2,TEXT("idle_combat_blend_snapped"));
            Check(MaxLeftLift>10 && MaxRightLift>10,TEXT("hands_did_not_rise_for_combat"));
            Check(MaxHandStep<20,TEXT("hand_pose_teleported"));
            UE_LOG(LogRogue10m, Display, TEXT("BRAWLER_IDLE METRICS rise_frames=%d fall_frames=%d left_lift=%.3f right_lift=%.3f max_hand_step=%.3f damage_events=%d max_hit_error=%.5f max_wrist_step_deg=%.3f max_arm_length_error=%.6f"),
                RiseIntermediateFrames,FallIntermediateFrames,MaxLeftLift,MaxRightLift,MaxHandStep,DamageEvents,MaxHitError,MaxWristStepDegrees,MaxArmLengthError);
            // Presentation must not retain a half-raised state through another weapon.
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
            Check(!Character->GetBasicBrawlerComponent()->IsBasicBrawlerActive(),TEXT("weapon_switch_kept_brawler_active"));
            ++FrameIndex; return;
        }
        ++FrameIndex;
    }
    void Finish()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        if (Controller.IsValid()) { if (bOwnLookIgnore) { Controller->SetIgnoreLookInput(false); } Controller->SetControlRotation(SavedControl); }
        if (Character.IsValid()) { Character->SetCanBeDamaged(SavedCanDamage); }
        if (bOwnFixedTime) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=BRAWLER_IDLE_TRANSITION_%s frames=%d failures=%d"),Failures==0 && FrameIndex==TotalFrames+3 ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures);
        FPlatformMisc::RequestExitWithStatus(false,Failures==0 && FrameIndex==TotalFrames+3 ? 0 : 1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> Monster;
    TWeakObjectPtr<APlayerController> Controller;
    TMap<int32,ERogue10mBasicBrawlerAttack> ObservedAttacks;
    FDelegateHandle TickHandle;
    FTransform BaseCamera;
    FRotator BaseControl,SavedControl;
    FQuat PreviousLeftWrist=FQuat::Identity,PreviousRightWrist=FQuat::Identity;
    double ArmUpperLength[2]={},ArmLowerLength[2]={};
    FVector IdleLeft,IdleRight,IdleLeftShoulder,IdleRightShoulder,PreviousLeft,PreviousRight;
    FString Output;
    int32 FrameIndex=0,Failures=0,RiseIntermediateFrames=0,FallIntermediateFrames=0,DamageEvents=0,TotalFrames=450,ReentryFrame=-1;
    double OldDelta=1.0/30.0,MaxHandStep=0,MaxLeftLift=0,MaxRightLift=0,MaxWristStepDegrees=0,MaxArmLengthError=0;
    float PreviousReady=0,InitialHealth=0,PreviousHealth=0,TapStartHealth=0,TapDamage=0,HookStartHealth=0,MaxHitError=0;
    bool bCapture=true,bTestReentry=false,bReentryTriggered=false,bDownView=false,bOwnLookIgnore=false,bOwnFixedTime=false,OldFixed=false,SavedCanDamage=true,bPreviousValid=false;
};
static TSharedPtr<FRun> Active;
static void Run(const TArray<FString>& Args)
{
    if (!GEngine || GIsEditor) { UE_LOG(LogRogue10m,Warning,TEXT("BRAWLER_IDLE requires standalone -game, not editor/PIE.")); return; }
    for (const FWorldContext& Context:GEngine->GetWorldContexts())
    {
        if (UWorld* World=Context.World();World && World->IsGameWorld())
        { Active=MakeShared<FRun>(); Active->Start(World,Args.Contains(TEXT("lookdown")),Args.Contains(TEXT("reentry")),Args.Contains(TEXT("nocapture"))); return; }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBrawlerIdleTransition"),
    TEXT("Captures hands-down idle, left/right jabs, short right straight, charged hook and smooth returns; 450 frames at 30fps, optional nocapture skips image saving / lookdown shows idle at -65 without UI / reentry adds a jab while lowering and runs 510 frames; standalone editor build only, then exits."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
#endif
