// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
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
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mAppearanceCameraTest
{
struct FAttackMeasure
{
    int32 MontageSamples=0,DamageSamples=0;
    double MaxBodyHandMotion=0,MaxHitError=0;
    FString MontagePath;
    bool bGeometryMeasured=false,bGeometryIncludesTarget=false;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        bCapture=!FParse::Param(FCommandLine::Get(),TEXT("RogueAppearanceNoCapture"));
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get()) { Self->Finish(false); } });
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("APPEARANCE_CAMERA FAIL frame=%d %s"),FrameIndex,Message); }
    }
    void Press(bool bPrimary) { Character->GetCombatComponent()->HandleAttackPressed(bPrimary); }
    void Release(bool bPrimary) { Character->GetCombatComponent()->HandleAttackReleased(bPrimary); }
    void Click(bool bPrimary) { Press(bPrimary); Release(bPrimary); }
    float Health() const { return Monster.IsValid() ? Monster->GetRogueAttributeSet()->GetHealth() : 0.f; }
    FVector BodyHand(const TCHAR* Bone) const
    {
        return Character->GetMesh()->GetComponentTransform().InverseTransformPosition(Character->GetMesh()->GetSocketLocation(Bone));
    }
    // Independent reconstruction from the visible body's evaluated local bones, before hidden-head scale.
    bool ExpectedCamera(FVector& Location,FQuat& Rotation,FTransform& WorldHead) const
    {
        auto* Mesh=Character->GetMesh(); auto* Asset=Mesh->GetSkeletalMeshAsset();
        auto* Settings=Character->GetAppearanceCameraComponent();
        if (!Asset || !Settings) { return false; }
        const auto& Skeleton=Asset->GetRefSkeleton();
        const int32 Index=Skeleton.FindBoneIndex(Settings->HeadBone);
        const auto Pose=Mesh->GetBoneSpaceTransformsView();
        if (!Pose.IsValidIndex(Index)) { return false; }
        FTransform Animated=Pose[Index],Reference=Skeleton.GetRefBonePose()[Index];
        for (int32 Parent=Skeleton.GetParentIndex(Index);Parent!=INDEX_NONE;Parent=Skeleton.GetParentIndex(Parent))
        {
            if (!Pose.IsValidIndex(Parent)) { return false; }
            Animated*=Pose[Parent]; Reference*=Skeleton.GetRefBonePose()[Parent];
        }
        const FQuat WorldBasis=Mesh->GetComponentQuat();
        const FQuat Motion=(WorldBasis*Animated.GetRotation()*Reference.GetRotation().Inverse()*WorldBasis.Inverse()).GetNormalized();
        WorldHead=Animated*Mesh->GetComponentTransform();
        Location=WorldHead.GetLocation()+(Motion*Character->GetActorQuat()).RotateVector(Settings->EyeOffset);
        Rotation=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
        return !WorldHead.ContainsNaN() && !Location.ContainsNaN() && !Rotation.ContainsNaN();
    }
    bool IndependentTargetOverlap(const URogue10mAttackSkillData& Skill,const FVector& Origin,const FQuat& Rotation)
    {
        const FVector Forward=Rotation.GetForwardVector();
        FVector Center=Origin; FQuat QueryRotation=FQuat::Identity; FCollisionShape Shape;
        bool bSupported=true;
        if (Skill.AttackShape==ERogue10mAttackShape::LinearBox)
        {
            Center=Origin+Forward*(Skill.AttackRange*.5f); QueryRotation=Rotation;
            Shape=FCollisionShape::MakeBox(FVector(Skill.AttackRange*.5f,Skill.BoxHalfWidth,Skill.BoxHalfHeight));
        }
        else if (Skill.AttackShape==ERogue10mAttackShape::Arc) { Shape=FCollisionShape::MakeSphere(Skill.AttackRange); }
        else if (Skill.AttackShape==ERogue10mAttackShape::Circle)
        { Center=Origin+Forward*Skill.CircleForwardOffset; Shape=FCollisionShape::MakeSphere(Skill.AttackRange); }
        else { bSupported=false; }
        Check(bSupported,TEXT("straight_independent_geometry_shape_unsupported"));
        if (!bSupported || !Monster.IsValid()) { return false; }
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic); Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(AppearanceCameraIndependentGeometry),false,Character.Get());
        TArray<FOverlapResult> Results;
        World->OverlapMultiByObjectType(Results,Center,QueryRotation,Objects,Shape,Params);
        bool bTargetOverlaps=false;
        for (const FOverlapResult& Result:Results) { if (Result.GetActor()==Monster.Get()) { bTargetOverlaps=true; } }
        const FVector ToTarget=Monster->GetActorLocation()-Origin;
        const float Dot=FVector::DotProduct(Forward,ToTarget.GetSafeNormal());
        const float ArcThreshold=FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Skill.ArcAngleDegrees,1.f,180.f)*.5f));
        const bool bIncludes=bTargetOverlaps && (Skill.AttackShape!=ERogue10mAttackShape::Arc || Dot>=ArcThreshold);
        UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA INDEPENDENT_GEOMETRY frame=%d skill=%s shape=%d range=%.5f half_width=%.5f half_height=%.5f arc_deg=%.5f circle_offset=%.5f target_view_local=%s forward_dot=%.7f arc_dot_min=%.7f overlap=%d includes_target=%d"),
            FrameIndex,*Skill.GetPathName(),static_cast<int32>(Skill.AttackShape),Skill.AttackRange,Skill.BoxHalfWidth,Skill.BoxHalfHeight,Skill.ArcAngleDegrees,Skill.CircleForwardOffset,*Rotation.UnrotateVector(ToTarget).ToCompactString(),Dot,ArcThreshold,bTargetOverlaps,bIncludes);
        return bIncludes;
    }
    void Prepare()
    {
        Controller=World.IsValid() ? World->GetFirstPlayerController() : nullptr;
        Character=Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_missing")); Finish(); return; }
        auto* Body=Character->GetMesh(); auto* Source=Character->GetAnimationPlaybackMesh();
        auto* Settings=Character->GetAppearanceCameraComponent();
        if (!Body || !Source || !Body->GetAnimInstance() || !Source->GetAnimInstance() || !Settings)
        { Check(false,TEXT("appearance_source_or_camera_missing")); Finish(); return; }
        BodyClass=Body->GetAnimInstance()->GetClass(); SourceClass=Source->GetAnimInstance()->GetClass();
        Check(Settings->bEnableAppearanceCamera && Settings->IsAppearanceCameraActive(),TEXT("appearance_camera_not_default_active"));
        Check(Body!=Character->GetFirstPersonMesh() && Source!=Character->GetFirstPersonMesh(),TEXT("using_first_person_animation_rig"));
        Check(!BodyClass->GetName().Contains(TEXT("FistAnim")) && !SourceClass->GetName().Contains(TEXT("FistAnim")),TEXT("procedural_fist_replaced_body_anim_class"));
        Check(Character->GetEquippedWeaponType()==ERogue10mWeaponType::Unarmed,TEXT("fresh_spawn_not_unarmed"));
        BaseControl=Controller->GetControlRotation(); ExpectedControl=BaseControl;
        Check(FMath::Abs(FRotator::NormalizeAxis(BaseControl.Pitch)+30.f)<.1f,TEXT("default_view_not_minus30"));
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
        Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/AppearanceHeadCamera"));
        if (bCapture) { IFileManager::Get().MakeDirectory(*Output,true); }
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (W==Self->World.Get()) { Self->Frame(); } });
        UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA TIMELINE idle=[0,45) jabs=45/48 aim_pitch=[165,175]:-30_to_0 straight=180/183 charge=[300,345) weapon=442/445 aim_pitch=[435,445]:0_to_-30 lookdown=[455,485) walk=[490,510) jump=515 frames=570 fps=30 body=%s source=%s body_class=%s source_class=%s view=%s capture=%d"),
            *Body->GetSkeletalMeshAsset()->GetPathName(),*Source->GetSkeletalMeshAsset()->GetPathName(),*BodyClass->GetPathName(),*SourceClass->GetPathName(),*BaseControl.ToString(),bCapture);
    }
    void Frame()
    {
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_lost")); Finish(); return; }
        auto* Brawler=Character->GetBasicBrawlerComponent(); auto* Body=Character->GetMesh();
        auto* Source=Character->GetAnimationPlaybackMesh(); auto* Arms=Character->GetFirstPersonMesh();
        auto* Camera=Character->GetFirstPersonCameraComponent(); auto* Settings=Character->GetAppearanceCameraComponent();
        if (!Brawler || !Body || !Source || !Source->GetAnimInstance() || !Settings || !Camera)
        { Check(false,TEXT("runtime_components_lost")); Finish(); return; }
        // Observe the engine-updated cache before this fixture changes input or invokes CalcCamera.
        // This detects camera-ordering lag that an explicit CalcCamera call below would otherwise hide.
        if (FrameIndex>0)
        {
            FVector ExpectedCachedLocation; FQuat ExpectedCachedRotation; FTransform CachedHead;
            if (Controller->PlayerCameraManager && ExpectedCamera(ExpectedCachedLocation,ExpectedCachedRotation,CachedHead))
            {
                const FMinimalViewInfo& CachedView=Controller->PlayerCameraManager->GetCameraCacheView();
                const double PositionError=FVector::Distance(CachedView.Location,ExpectedCachedLocation);
                const double AngleError=FMath::RadiansToDegrees(CachedView.Rotation.Quaternion().AngularDistance(ExpectedCachedRotation));
                MaxCachedPositionError=FMath::Max(MaxCachedPositionError,PositionError);
                MaxCachedAngleError=FMath::Max(MaxCachedAngleError,AngleError);
                ++CachedSamples;
                if (PositionError>=.05 || AngleError>=.02)
                {
                    UE_LOG(LogRogue10m,Error,TEXT("APPEARANCE_CAMERA CACHE_MISMATCH frame=%d pos_error_cm=%.8f angle_error_deg=%.8f cached_position=%s expected_position=%s cached_rotation=%s expected_rotation=%s"),
                        FrameIndex,PositionError,AngleError,*CachedView.Location.ToCompactString(),*ExpectedCachedLocation.ToCompactString(),*CachedView.Rotation.ToCompactString(),*ExpectedCachedRotation.Rotator().ToCompactString());
                }
                Check(PositionError<.05 && AngleError<.02,TEXT("automatic_camera_cache_not_current_appearance_head"));
            }
            else { Check(false,TEXT("automatic_camera_cache_or_head_missing")); }
        }
        if (FrameIndex==30)
        { IdleLeft=BodyHand(TEXT("hand_l")); IdleRight=BodyHand(TEXT("hand_r")); BaseCapsule=Character->GetActorLocation(); }
        if (FrameIndex==45)
        {
            const float Before=Health(); Press(true);
            Check(Brawler->GetSnapshot().AcceptedSerial==1 && Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::LeftJab,TEXT("first_primary_not_left_jab"));
            Check(FMath::IsNearlyEqual(Before,Health(),.01f),TEXT("jab_damaged_before_hit_delay"));
        }
        if (FrameIndex==46) { Release(true); }
        if (FrameIndex==48) { Click(true); }
        if (FrameIndex==110) { Check(Brawler->GetSnapshot().AcceptedSerial==2,TEXT("buffered_primary_not_two_jabs")); }
        // The real head pose bends the view during punches. Deliberately raise the mouse aim
        // toward the unchanged target, rather than widening hit volumes or moving the target.
        if (FrameIndex>=165 && FrameIndex<=175)
        {
            ExpectedControl.Pitch=FMath::Lerp(BaseControl.Pitch,0.f,(FrameIndex-165)/10.f);
            Controller->SetControlRotation(ExpectedControl);
            if (FrameIndex==165 || FrameIndex==175)
            {
                UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA AIM_RAISE frame=%d control_pitch=%.3f target_distance_cm=%.3f"),
                    FrameIndex,ExpectedControl.Pitch,Monster.IsValid() ? FVector::Dist2D(Character->GetActorLocation(),Monster->GetActorLocation()) : -1.f);
            }
        }
        if (FrameIndex==180) { TapHealth=Health(); Press(false); }
        if (FrameIndex==183)
        {
            Release(false);
            Check(Brawler->GetSnapshot().Attack==ERogue10mBasicBrawlerAttack::RightStraight,TEXT("short_secondary_not_straight"));
            Check(FMath::IsNearlyEqual(TapHealth,Health(),.01f),TEXT("straight_hit_on_release"));
        }
        if (FrameIndex==245)
        {
            TapDamage=TapHealth-Health();
            const auto* Straight=Attacks.Find(3);
            Check(Straight && Straight->bGeometryMeasured,TEXT("straight_geometry_not_measured"));
            if (Straight) { Check((TapDamage>0)==Straight->bGeometryIncludesTarget,TEXT("straight_damage_disagrees_with_independent_geometry")); }
        }
        if (FrameIndex==300) { ChargeHealth=Health(); Press(false); }
        if (FrameIndex==340)
        {
            Check(Brawler->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Charging && Brawler->GetSnapshot().ChargeAlpha>.99f,TEXT("charge_not_held"));
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
            HookDamage=ChargeHealth-Health(); Check(HookDamage>0,TEXT("charged_hook_did_not_hit"));
            Check(Brawler->GetSnapshot().AcceptedSerial==4,TEXT("input_not_exactly_four_attacks"));
        }
        if (FrameIndex>=435 && FrameIndex<=445)
        {
            ExpectedControl.Pitch=FMath::Lerp(0.f,BaseControl.Pitch,(FrameIndex-435)/10.f);
            Controller->SetControlRotation(ExpectedControl);
        }
        if (FrameIndex==442) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle); }
        if (FrameIndex==445) { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); }
        if (FrameIndex==455) { ExpectedControl.Pitch=-70; Controller->SetControlRotation(ExpectedControl); }
        if (FrameIndex==485) { ExpectedControl=BaseControl; Controller->SetControlRotation(ExpectedControl); }
        if (FrameIndex>=490 && FrameIndex<510) { Character->AddMovementInput(-Character->GetActorForwardVector(),.45f,true); }
        if (FrameIndex==510) { Character->GetCharacterMovement()->StopMovementImmediately(); }
        if (FrameIndex==515) { JumpStartZ=Character->GetActorLocation().Z; Character->Jump(); }
        if (FrameIndex==521) { Character->StopJumping(); }
        if (FrameIndex>=515) { MaxJumpHeight=FMath::Max(MaxJumpHeight,static_cast<double>(Character->GetActorLocation().Z-JumpStartZ)); }
        Check(Settings->IsAppearanceCameraActive(),TEXT("appearance_camera_inactive"));
        Check(Camera->GetAttachParent()==Body && Camera->GetAttachSocketName()==Settings->HeadBone,TEXT("camera_not_attached_to_appearance_head"));
        Check(!Arms || ((!Arms->IsVisible() || Arms->bHiddenInGame) && !Arms->IsComponentTickEnabled() && Arms->bPauseAnims),TEXT("separate_first_person_rig_not_disabled"));
        Check(Body->IsVisible() && !Body->bHiddenInGame && !Body->bOwnerNoSee,TEXT("appearance_body_hidden"));
        Check(Body->GetAnimInstance() && Body->GetAnimInstance()->GetClass()==BodyClass.Get() && Source->GetAnimInstance()->GetClass()==SourceClass.Get(),TEXT("body_or_source_anim_class_changed"));
        Check(Controller->GetControlRotation().Equals(ExpectedControl,.01f),TEXT("animation_overwrote_control_aim"));
        FMinimalViewInfo View;
        Character->CalcCamera(1.f/30.f,View);
        FVector ExpectedLocation; FQuat ExpectedRotation; FTransform Head;
        if (ExpectedCamera(ExpectedLocation,ExpectedRotation,Head))
        {
            const double PosError=FVector::Distance(View.Location,ExpectedLocation);
            const double RotError=FMath::RadiansToDegrees(View.Rotation.Quaternion().AngularDistance(ExpectedRotation));
            MaxCameraPositionError=FMath::Max(MaxCameraPositionError,PosError); MaxCameraAngleError=FMath::Max(MaxCameraAngleError,RotError);
            Check(PosError<.05 && RotError<.02,TEXT("rendered_view_not_actual_appearance_head"));
            Check(FVector::Distance(Camera->GetComponentLocation(),ExpectedLocation)<.05,TEXT("gameplay_camera_not_actual_appearance_head"));
            if (FrameIndex==30) { BaseHead=Head; BaseView=View.Rotation.Quaternion(); }
            if (FrameIndex>=45 && FrameIndex<=390)
            {
                MaxHeadDegrees=FMath::Max(MaxHeadDegrees,FMath::RadiansToDegrees(BaseHead.GetRotation().AngularDistance(Head.GetRotation())));
                MaxViewDegrees=FMath::Max(MaxViewDegrees,FMath::RadiansToDegrees(BaseView.AngularDistance(View.Rotation.Quaternion())));
                MaxHeadTranslation=FMath::Max(MaxHeadTranslation,FVector::Distance(BaseHead.GetLocation(),Head.GetLocation()));
            }
        }
        else { Check(false,TEXT("appearance_head_pose_unreadable")); }
        const auto S=Brawler->GetSnapshot();
        if (S.Phase==ERogue10mBasicBrawlerPhase::Attacking)
        {
            auto& M=Attacks.FindOrAdd(S.AcceptedSerial);
            const float ScheduledHit=S.AttackDuration*S.HitFraction;
            if (S.Attack==ERogue10mBasicBrawlerAttack::RightStraight && !M.bGeometryMeasured && S.AttackElapsed>=ScheduledHit)
            {
                const auto* Skill=Character->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
                Check(Skill!=nullptr,TEXT("straight_skill_data_missing"));
                if (Skill)
                {
                    M.bGeometryMeasured=true;
                    M.bGeometryIncludesTarget=IndependentTargetOverlap(*Skill,ExpectedLocation,ExpectedRotation);
                }
            }
            if (FMath::Abs(S.AttackElapsed-ScheduledHit)<.07f)
            {
                UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA HIT_VIEW frame=%d serial=%d elapsed=%.6f scheduled=%.6f control=%s actual_view=%s camera_position=%s target_position=%s"),
                    FrameIndex,S.AcceptedSerial,S.AttackElapsed,ScheduledHit,*Controller->GetControlRotation().ToCompactString(),*View.Rotation.ToCompactString(),*View.Location.ToCompactString(),Monster.IsValid() ? *Monster->GetActorLocation().ToCompactString() : TEXT("missing"));
            }
            if (auto* Montage=Source->GetAnimInstance()->GetCurrentActiveMontage())
            {
                ++M.MontageSamples; M.MontagePath=Montage->GetPathName();
                if (S.Attack==ERogue10mBasicBrawlerAttack::LeftJab) { Check(Montage->GetName()==TEXT("AM_BoxingLeftJab_Appearance"),TEXT("left_jab_not_source_montage")); }
                if (S.Attack==ERogue10mBasicBrawlerAttack::RightJab) { Check(Montage->GetName()==TEXT("AM_BoxingRightJab_Appearance"),TEXT("right_jab_not_source_montage")); }
            }
            M.MaxBodyHandMotion=FMath::Max(M.MaxBodyHandMotion,FMath::Max(FVector::Distance(BodyHand(TEXT("hand_l")),IdleLeft),FVector::Distance(BodyHand(TEXT("hand_r")),IdleRight)));
            if (Health()<PreviousHealth-.01f)
            {
                ++M.DamageSamples; const double Hit=S.AttackDuration*S.HitFraction;
                M.MaxHitError=FMath::Max(M.MaxHitError,static_cast<double>(FMath::Abs(S.AttackElapsed-Hit)));
                Check(M.MaxHitError<=1.0/30.0+.012,TEXT("damage_not_at_scheduled_hit"));
                UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA HIT frame=%d serial=%d kind=%d elapsed=%.6f expected=%.6f montage=%s damage=%.3f"),FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Attack),S.AttackElapsed,Hit,*M.MontagePath,PreviousHealth-Health());
            }
        }
        else if (Health()<PreviousHealth-.01f) { Check(false,TEXT("damage_outside_attack")); }
        PreviousHealth=Health();
        if (FrameIndex%15==0)
        {
            UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA SAMPLE frame=%d serial=%d phase=%d view=%s position=%s left=%s right=%s"),FrameIndex,S.AcceptedSerial,static_cast<int32>(S.Phase),*View.Rotation.ToCompactString(),*View.Location.ToCompactString(),*BodyHand(TEXT("hand_l")).ToCompactString(),*BodyHand(TEXT("hand_r")).ToCompactString());
        }
        if (FrameIndex<570 && bCapture) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false); }
        if (FrameIndex==570)
        {
            for (int32 I : {1,2,3,4})
            {
                Check(Attacks.Contains(I),TEXT("attack_not_measured")); if (!Attacks.Contains(I)) { continue; }
                const auto& M=Attacks[I];
                if (I==3)
                {
                    Check(M.bGeometryMeasured && !M.bGeometryIncludesTarget,TEXT("straight_miss_not_supported_by_independent_geometry"));
                    Check(M.DamageSamples==(M.bGeometryIncludesTarget ? 1 : 0),TEXT("straight_hit_count_disagrees_with_geometry"));
                }
                else { Check(M.DamageSamples==1,TEXT("attack_not_exactly_one_damage")); }
                Check(M.MontageSamples>=3,TEXT("attack_not_playing_real_body_montage"));
                Check(M.MaxBodyHandMotion>5,TEXT("visible_body_did_not_animate"));
                UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA ATTACK serial=%d hits=%d montage_samples=%d hand_motion_cm=%.6f hit_error=%.6f montage=%s"),I,M.DamageSamples,M.MontageSamples,M.MaxBodyHandMotion,M.MaxHitError,*M.MontagePath);
            }
            Check(CachedSamples==570,TEXT("automatic_camera_cache_samples_missing"));
            UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA CACHE_METRICS samples=%d max_pos_error_cm=%.8f max_angle_error_deg=%.8f"),CachedSamples,MaxCachedPositionError,MaxCachedAngleError);
            Check(MaxHeadDegrees>.2 && MaxViewDegrees>.2 && MaxHeadTranslation>.2,TEXT("actual_appearance_head_motion_missing"));
            Check(MaxJumpHeight>10,TEXT("jump_motion_missing"));
            Check(FVector::Dist2D(Character->GetActorLocation(),BaseCapsule)>5,TEXT("walking_motion_missing"));
            UE_LOG(LogRogue10m,Display,TEXT("APPEARANCE_CAMERA METRICS pos_error_cm=%.8f angle_error_deg=%.8f head_deg=%.5f view_deg=%.5f head_translation_cm=%.5f jump_cm=%.5f total_damage=%.3f"),MaxCameraPositionError,MaxCameraAngleError,MaxHeadDegrees,MaxViewDegrees,MaxHeadTranslation,MaxJumpHeight,InitialHealth-Health());
            Finish(); return;
        }
        ++FrameIndex;
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; } bFinished=true;
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(PrepareTimer); }
        if (bPrepared && Character.IsValid()) { Character->StopJumping(); Character->SetCanBeDamaged(OldCanDamage); Character->SetEquippedWeaponType(OriginalWeapon); }
        if (bOwnInputIgnore && Controller.IsValid()) { Controller->SetIgnoreLookInput(false); Controller->SetIgnoreMoveInput(false); Controller->SetControlRotation(BaseControl); }
        if (bOwnFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        const bool bPassed=Failures==0 && FrameIndex==570;
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=APPEARANCE_CAMERA_%s frames=%d failures=%d capture=%d"),bPassed ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures,bCapture);
        if (bExit) { FPlatformMisc::RequestExitWithStatus(false,bPassed ? 0 : 1); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mBasicMonster> Monster;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UClass> BodyClass,SourceClass;
    FTimerHandle PrepareTimer;
    FDelegateHandle Tick,Cleanup;
    TMap<int32,FAttackMeasure> Attacks;
    FString Output;
    FRotator BaseControl,ExpectedControl;
    FTransform BaseHead;
    FQuat BaseView;
    FVector BaseCapsule,IdleLeft,IdleRight;
    ERogue10mWeaponType OriginalWeapon=ERogue10mWeaponType::Unarmed;
    float InitialHealth=0,PreviousHealth=0,TapHealth=0,TapDamage=0,ChargeHealth=0,HookDamage=0,JumpStartZ=0;
    double OldDelta=1.0/30.0,MaxHeadDegrees=0,MaxViewDegrees=0,MaxHeadTranslation=0,MaxJumpHeight=0,MaxCameraPositionError=0,MaxCameraAngleError=0,MaxCachedPositionError=0,MaxCachedAngleError=0;
    int32 FrameIndex=0,Failures=0,CachedSamples=0;
    bool bCapture=true,bPrepared=false,bFinished=false,bOwnFixed=false,OldFixed=false,OldCanDamage=true,bOwnInputIgnore=false;
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
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestAppearanceCamera"),
    TEXT("Checks actual Appearance Mesh head camera, disabled separate first-person rig, real combat montages, three timed hits and a geometrically verified straight miss, look-down, walking and jumping. Captures 570 frames; -RogueAppearanceNoCapture skips images."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
