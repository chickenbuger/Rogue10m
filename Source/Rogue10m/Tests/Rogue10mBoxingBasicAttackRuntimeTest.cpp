// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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

namespace Rogue10mBoxingBasicAttackTest
{
struct FAttackMeasure
{
    ERogue10mBasicBrawlerAttack Kind=ERogue10mBasicBrawlerAttack::None;
    int32 DamageSamples=0,SourceSamples=0;
    double MaxWeight=0,MaxHitError=0,MinPeakError=1000;
    double LeftXAtPeak=0,RightXAtPeak=0;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        bCapture=!FParse::Param(FCommandLine::Get(),TEXT("RogueBoxingBasicNoCapture"));
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get()) { Self->Finish(false); } });
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("BOXING_BASIC FAIL frame=%d %s"),FrameIndex,Message); }
    }
    void Press(bool bPrimary) { Character->GetCombatComponent()->HandleAttackPressed(bPrimary); }
    void Release(bool bPrimary) { Character->GetCombatComponent()->HandleAttackReleased(bPrimary); }
    void Click(bool bPrimary) { Press(bPrimary); Release(bPrimary); }
    float Health() const { return Monster.IsValid() ? Monster->GetRogueAttributeSet()->GetHealth() : 0.f; }
    FVector Hand(const TCHAR* Bone) const
    {
        return Character->GetFirstPersonCameraComponent()->GetComponentTransform().InverseTransformPosition(
            Character->GetFirstPersonMesh()->GetSocketLocation(Bone));
    }
    void Prepare()
    {
        Controller=World.IsValid() ? World->GetFirstPlayerController() : nullptr;
        Character=Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_missing")); Finish(); return; }
        auto* Presentation=Character->GetFirstPersonPresentationComponent();
        auto* Fist=Cast<URogue10mFistAnimInstance>(Character->GetFirstPersonMesh()->GetAnimInstance());
        Check(Presentation && Presentation->IsFullBodyPresentationActive(),TEXT("fullbody_not_active"));
        Check(Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed,TEXT("fresh_spawn_not_unarmed"));
        Check(Fist && Fist->bEnableSourceBoxingBasicAttacks && Fist->LoadedSourceBoxingAnimation,TEXT("source_boxing_not_loaded_by_default"));
        if (!Fist || !Fist->LoadedSourceBoxingAnimation) { Finish(); return; }
        SourceLength=Fist->LoadedSourceBoxingAnimation->GetPlayLength();
        Check(FMath::IsNearlyEqual(SourceLength,52.f/30.f,.01f),TEXT("source_clip_length_not_original_boxing"));
        Check(Fist->LoadedSourceBoxingAnimation->GetPathName()==TEXT("/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny.A_BoxingJab_Manny"),TEXT("wrong_source_clip"));
        Check(Fist->bEnableRelaxedIdle,TEXT("relaxed_idle_disabled"));
        BaseControl=Controller->GetControlRotation();
        Check(FMath::Abs(FRotator::NormalizeAxis(BaseControl.Pitch)+30.f)<.1f,TEXT("default_view_not_minus30"));
        BaseCamera=Character->GetFirstPersonCameraComponent()->GetRelativeTransform();
        OriginalWeapon=Character->GetEquippedWeaponType(); OldCanDamage=Character->CanBeDamaged(); bPrepared=true;
        Character->SetCanBeDamaged(false); Character->GetCharacterMovement()->StopMovementImmediately();
        Controller->SetIgnoreLookInput(true); Controller->SetIgnoreMoveInput(true); bOwnInputIgnore=true;
        Character->GetRogueAttributeSet()->RestoreVitals();
        Character->GetRogueAttributeSet()->SetMinDamageRatio(1.f); Character->GetRogueAttributeSet()->SetMaxDamageRatio(1.f);
        Character->GetRogueAttributeSet()->SetCriticalChance(0.f);
        if (auto* Regen=Character->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget(); if (auto* AI=It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
            if (!Monster.IsValid())
            {
                Monster=*It;
                if (auto* Regen=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
                It->GetRogueAttributeSet()->SetMaxHealth(10000); It->GetRogueAttributeSet()->SetHealth(10000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
                It->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*135.f,false,nullptr,ETeleportType::TeleportPhysics);
                It->SetActorRotation((Character->GetActorLocation()-It->GetActorLocation()).Rotation());
            }
            else { It->SetActorEnableCollision(false); }
        }
        Check(Monster.IsValid(),TEXT("damage_target_missing"));
        InitialHealth=PreviousHealth=Health();
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bOwnFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/BoxingBasicAttack"));
        if (bCapture) { IFileManager::Get().MakeDirectory(*Output,true); }
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (W==Self->World.Get()) { Self->Frame(); } });
        UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC TIMELINE idle=[0,45) jabs=45/48 straight=180/183 charge=[300,345) rest=165/285/440 weapon=442/445 frames=450 fps=30 source=%s source_length=%.6f view=%s capture=%d"),
            *Fist->LoadedSourceBoxingAnimation->GetPathName(),SourceLength,*BaseControl.ToString(),bCapture);
    }
    void CheckRest(URogue10mFistAnimInstance* Fist)
    {
        Check(Fist->GetCombatReadyWeight()<.05f,TEXT("combat_ready_did_not_return_to_idle"));
        Check(Fist->GetSourceBoxingWeight()<.001f,TEXT("source_pose_remained_in_idle"));
        Check(FVector::Distance(Hand(TEXT("hand_l")),IdleLeft)<5 && FVector::Distance(Hand(TEXT("hand_r")),IdleRight)<5,TEXT("hands_did_not_return_to_relaxed_idle"));
        UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC REST frame=%d ready=%.5f source=%.5f"),FrameIndex,Fist->GetCombatReadyWeight(),Fist->GetSourceBoxingWeight());
    }
    void Frame()
    {
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_lost")); Finish(); return; }
        auto* Brawler=Character->GetBasicBrawlerComponent();
        auto* Mesh=Character->GetFirstPersonMesh();
        auto* Camera=Character->GetFirstPersonCameraComponent();
        auto* Fist=Cast<URogue10mFistAnimInstance>(Mesh->GetAnimInstance());
        if (!Brawler || !Fist) { Check(false,TEXT("runtime_component_or_fist_instance_missing")); Finish(); return; }
        if (FrameIndex==30)
        {
            BaseCapsule=Character->GetActorLocation(); IdleLeft=Hand(TEXT("hand_l")); IdleRight=Hand(TEXT("hand_r"));
            BaseHead=Mesh->GetSocketQuaternion(TEXT("head"));
            Check(Fist->GetSourceBoxingWeight()<.001f,TEXT("source_pose_active_before_input"));
        }
        if (FrameIndex==45)
        {
            const float Before=Health(); Press(true);
            Check(Brawler->GetSnapshot().AcceptedSerial==1 && Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::LeftJab,TEXT("first_primary_not_left_jab"));
            Check(FMath::IsNearlyEqual(Before,Health(),.01f),TEXT("jab_damaged_before_hit_delay"));
        }
        if (FrameIndex==46) { Release(true); }
        if (FrameIndex==48) { Click(true); }
        if (FrameIndex==110) { Check(Brawler->GetSnapshot().AcceptedSerial==2,TEXT("buffered_primary_not_exactly_two_jabs")); }
        if (FrameIndex==165 || FrameIndex==285 || FrameIndex==440) { CheckRest(Fist); }
        if (FrameIndex==180) { TapHealth=Health(); Press(false); }
        if (FrameIndex==183)
        {
            Release(false);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightStraight,TEXT("short_secondary_not_straight"));
            Check(FMath::IsNearlyEqual(TapHealth,Health(),.01f),TEXT("straight_hit_on_release"));
        }
        if (FrameIndex==245) { TapDamage=TapHealth-Health(); Check(TapDamage>0,TEXT("straight_did_not_hit")); }
        if (FrameIndex==300) { ChargeHealth=Health(); Press(false); }
        if (FrameIndex==340)
        {
            const auto S=Brawler->GetSnapshot();
            Check(S.Phase==ERogue10mBasicBrawlerPhase::Charging && S.ChargeAlpha>.99f,TEXT("full_charge_not_held"));
            Check(FMath::IsNearlyEqual(ChargeHealth,Health(),.01f),TEXT("charge_fired_before_release"));
        }
        if (FrameIndex==345)
        {
            Release(false);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightHook,TEXT("held_secondary_not_hook"));
            Check(FMath::IsNearlyEqual(ChargeHealth,Health(),.01f),TEXT("hook_hit_on_release"));
        }
        if (FrameIndex==390)
        {
            HookDamage=ChargeHealth-Health(); Check(HookDamage>TapDamage,TEXT("charged_hook_not_stronger"));
            Check(Brawler->GetSnapshot().AcceptedSerial==4,TEXT("input_sequence_not_exactly_four_attacks"));
        }
        if (FrameIndex==442) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle); }
        if (FrameIndex==444) { Check(Fist->GetSourceBoxingWeight()<.001f && !Brawler->IsBasicBrawlerActive(),TEXT("weapon_switch_kept_source_boxing_active")); }
        if (FrameIndex==445) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); }
        if (FrameIndex==449) { Check(Fist->GetSourceBoxingWeight()<.001f && Brawler->IsBasicBrawlerActive(),TEXT("weapon_return_replayed_source_boxing")); }
        const auto S=Brawler->GetSnapshot();
        const float SourceTime=Fist->GetSourceBoxingTime(), SourceWeight=Fist->GetSourceBoxingWeight();
        Check(FMath::IsFinite(SourceTime) && FMath::IsFinite(SourceWeight),TEXT("nonfinite_source_state"));
        Check(Controller->GetControlRotation().Equals(BaseControl,.01f),TEXT("animation_changed_mouse_aim"));
        if (FrameIndex>30 && FrameIndex<442)
        {
            Check(Camera->GetRelativeTransform().Equals(BaseCamera,.01f),TEXT("animation_changed_gameplay_camera_mount"));
            Check(FVector::Distance(Character->GetActorLocation(),BaseCapsule)<.1f,TEXT("source_root_motion_moved_capsule"));
            Check(!Mesh->GetSocketTransform(TEXT("pelvis")).ContainsNaN() && !Mesh->GetSocketTransform(TEXT("head")).ContainsNaN(),TEXT("nonfinite_body_pose"));
            const FVector L=Hand(TEXT("hand_l")),R=Hand(TEXT("hand_r"));
            Check(!L.ContainsNaN() && !R.ContainsNaN(),TEXT("nonfinite_hand_pose"));
            if (bPreviousHands)
            {
                const double Step=FMath::Max(FVector::Distance(L,PreviousLeft),FVector::Distance(R,PreviousRight));
                if (Step>MaxHandStep)
                {
                    MaxHandStep=Step;
                    UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC HAND_STEP frame=%d serial=%d phase=%d source=%.6f weight=%.6f max_cm=%.6f left=%s right=%s"),
                        FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Phase),SourceTime,SourceWeight,MaxHandStep,*L.ToCompactString(),*R.ToCompactString());
                }
            }
            PreviousLeft=L; PreviousRight=R; bPreviousHands=true;
            if (FrameIndex>=45 && FrameIndex<=100 && FrameIndex%5==0)
            {
                const auto* Presentation=Character->GetFirstPersonPresentationComponent();
                const FVector Offset=Presentation ? Presentation->GetAppliedCameraMotionOffset() : FVector::ZeroVector;
                const FRotator Rotation=Presentation ? Presentation->GetAppliedCameraMotionRotation() : FRotator::ZeroRotator;
                UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC SOURCE_SAMPLE frame=%d serial=%d phase=%d elapsed=%.6f source=%.6f weight=%.6f mirrored=%d left=%s right=%s head_offset=%s head_rotation=%s"),
                    FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Phase),S.AttackElapsed,SourceTime,SourceWeight,Fist->IsSourceBoxingMirrored(),
                    *L.ToCompactString(),*R.ToCompactString(),*Offset.ToCompactString(),*Rotation.ToCompactString());
            }
            MaxHeadDegrees=FMath::Max(MaxHeadDegrees,FMath::RadiansToDegrees(BaseHead.AngularDistance(Mesh->GetSocketQuaternion(TEXT("head")))));
            if (Controller->PlayerCameraManager)
            {
                const auto RView=(Controller->PlayerCameraManager->GetCameraRotation()-Camera->GetComponentRotation()).GetNormalized();
                Check(!RView.ContainsNaN(),TEXT("nonfinite_rendered_head_camera"));
                MaxViewDegrees=FMath::Max(MaxViewDegrees,FMath::Max3(FMath::Abs(RView.Pitch),FMath::Abs(RView.Yaw),FMath::Abs(RView.Roll)));
            }
        }
        if (S.Phase==ERogue10mBasicBrawlerPhase::Attacking)
        {
            auto& M=Attacks.FindOrAdd(S.AcceptedSerial); M.Kind=S.Attack;
            const bool bJab=S.Attack==ERogue10mBasicBrawlerAttack::LeftJab || S.Attack==ERogue10mBasicBrawlerAttack::RightJab;
            M.MaxWeight=FMath::Max(M.MaxWeight,static_cast<double>(SourceWeight));
            if (bJab && SourceWeight>.01f)
            {
                ++M.SourceSamples;
                Check(Fist->IsSourceBoxingMirrored()==(S.Attack==ERogue10mBasicBrawlerAttack::RightJab),TEXT("jab_source_mirror_side_incorrect"));
                Check(SourceTime>=0 && SourceTime<=SourceLength+.001f,TEXT("source_sample_outside_original_clip"));
                const float Hit=S.AttackDuration*S.HitFraction;
                const float Expected=S.AttackElapsed<=Hit ? S.AttackElapsed/FMath::Max(Hit,.001f)*SourcePeak
                    : SourcePeak+(SourceLength-SourcePeak)*(S.AttackElapsed-Hit)/FMath::Max(S.AttackDuration-Hit,.001f);
                const float MaxRate=FMath::Max(SourcePeak/FMath::Max(Hit,.001f),(SourceLength-SourcePeak)/FMath::Max(S.AttackDuration-Hit,.001f));
                Check(FMath::Abs(SourceTime-Expected)<=MaxRate/30.f+.025f,TEXT("source_time_not_mapped_to_gameplay_hit"));
                const double PeakError=FMath::Abs(SourceTime-SourcePeak);
                if (PeakError<M.MinPeakError)
                {
                    M.MinPeakError=PeakError;
                    M.LeftXAtPeak=Hand(TEXT("hand_l")).X; M.RightXAtPeak=Hand(TEXT("hand_r")).X;
                }
            }
            else if (!bJab) { Check(SourceWeight<.001f,TEXT("secondary_attack_replaced_by_boxing_jab")); }
            if (Health()<PreviousHealth-.01f)
            {
                ++M.DamageSamples;
                const double Hit=S.AttackDuration*S.HitFraction;
                const double Error=FMath::Abs(S.AttackElapsed-Hit); M.MaxHitError=FMath::Max(M.MaxHitError,Error);
                Check(Error<=1.0/30.0+.012,TEXT("damage_did_not_match_scheduled_hit"));
                if (bJab)
                {
                    Check(SourceWeight>.8f,TEXT("damage_not_during_source_boxing_pose"));
                    const double SourceTolerance=FMath::Max(SourcePeak/FMath::Max(Hit,.001),(SourceLength-SourcePeak)/FMath::Max(S.AttackDuration-Hit,.001))/30.0+.025;
                    Check(FMath::Abs(SourceTime-SourcePeak)<=SourceTolerance,TEXT("damage_not_near_original_source_peak"));
                }
                UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC HIT frame=%d serial=%d kind=%d elapsed=%.6f expected=%.6f error=%.6f source=%.6f weight=%.6f mirrored=%d damage=%.3f"),
                    FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Attack),S.AttackElapsed,Hit,Error,SourceTime,SourceWeight,Fist->IsSourceBoxingMirrored(),PreviousHealth-Health());
            }
        }
        else if (Health()<PreviousHealth-.01f) { Check(false,TEXT("damage_outside_accepted_attack")); }
        if (S.Phase==ERogue10mBasicBrawlerPhase::Charging) { Check(SourceWeight<.001f,TEXT("charge_replaced_by_boxing_jab")); }
        PreviousHealth=Health();
        if (FrameIndex<450 && bCapture) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false); }
        if (FrameIndex==450)
        {
            for (int32 I : {1,2,3,4})
            {
                Check(Attacks.Contains(I),TEXT("accepted_attack_was_not_measured"));
                if (!Attacks.Contains(I)) { continue; }
                const auto& M=Attacks[I];
                Check(M.DamageSamples==1,TEXT("attack_did_not_damage_exactly_once"));
                if (I<=2)
                {
                    Check(M.SourceSamples>=3 && M.MaxWeight>.95,TEXT("primary_did_not_use_original_source"));
                    Check(I==1 ? M.LeftXAtPeak>M.RightXAtPeak+5 : M.RightXAtPeak>M.LeftXAtPeak+5,TEXT("source_jab_wrong_physical_hand_forward_at_peak"));
                }
                else { Check(M.SourceSamples==0 && M.MaxWeight<.001,TEXT("secondary_used_primary_source")); }
                UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC ATTACK serial=%d kind=%d hit_samples=%d source_samples=%d max_weight=%.6f peak_error=%.6f hit_error=%.6f left_peak_x=%.4f right_peak_x=%.4f"),I,static_cast<int32>(M.Kind),M.DamageSamples,M.SourceSamples,M.MaxWeight,M.MinPeakError,M.MaxHitError,M.LeftXAtPeak,M.RightXAtPeak);
            }
            Check(MaxHeadDegrees>.2 && MaxViewDegrees>.05,TEXT("fullbody_head_and_camera_response_missing"));
            UE_LOG(LogRogue10m,Display,TEXT("BOXING_BASIC METRICS max_hand_step_cm=%.4f head_deg=%.4f rendered_deg=%.4f tap_damage=%.3f hook_damage=%.3f total_damage=%.3f"),MaxHandStep,MaxHeadDegrees,MaxViewDegrees,TapDamage,HookDamage,InitialHealth-Health());
            Finish(); return;
        }
        ++FrameIndex;
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; } bFinished=true;
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(PrepareTimer); }
        if (bPrepared && Character.IsValid()) { Character->SetCanBeDamaged(OldCanDamage); Character->SetEquippedWeaponType(OriginalWeapon); }
        if (bOwnInputIgnore && Controller.IsValid()) { Controller->SetIgnoreLookInput(false); Controller->SetIgnoreMoveInput(false); }
        if (bOwnFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        const bool bPassed=Failures==0 && FrameIndex==450;
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=BOXING_BASIC_ATTACK_%s frames=%d failures=%d capture=%d"),bPassed ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures,bCapture);
        if (bExit) { FPlatformMisc::RequestExitWithStatus(false,bPassed ? 0 : 1); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> Monster;
    TWeakObjectPtr<APlayerController> Controller;
    FTimerHandle PrepareTimer;
    FDelegateHandle Tick,Cleanup;
    TMap<int32,FAttackMeasure> Attacks;
    FString Output;
    FRotator BaseControl;
    FTransform BaseCamera;
    FQuat BaseHead;
    FVector BaseCapsule,IdleLeft,IdleRight,PreviousLeft,PreviousRight;
    ERogue10mWeaponType OriginalWeapon=ERogue10mWeaponType::Unarmed;
    static constexpr float SourcePeak=13.f/30.f;
    float SourceLength=0,InitialHealth=0,PreviousHealth=0,TapHealth=0,TapDamage=0,ChargeHealth=0,HookDamage=0;
    double OldDelta=1.0/30.0,MaxHandStep=0,MaxHeadDegrees=0,MaxViewDegrees=0;
    int32 FrameIndex=0,Failures=0;
    bool bCapture=true,bPrepared=false,bFinished=false,bOwnFixed=false,OldFixed=false,OldCanDamage=true,bOwnInputIgnore=false,bPreviousHands=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine || GIsEditor || (Active.IsValid() && !Active->IsFinished())) { return; }
    for (const FWorldContext& C:GEngine->GetWorldContexts())
    {
        if (UWorld* W=C.World();W && W->WorldType==EWorldType::Game)
        { Active=MakeShared<FRun>(); Active->Start(W); return; }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBoxingBasicAttack"),
    TEXT("Verifies source Boxing jabs through real combat input, mirrored second jab, timed damage, secondary fallback and default head view; captures 450 frames with HUD then exits. -RogueBoxingBasicNoCapture skips images."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
