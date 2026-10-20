// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Integrated gameplay test: the actual combat input policy, movement/jump delegates,
// source AnimBP and retargeted appearance are observed together. Raw bone data is
// reconstructed before hidden-head rendering scales; numeric checks supplement visual review.
namespace Rogue10mFighterCombatStanceTest
{
static FString Columns(const FTransform& T)
{
    const FVector P=T.GetTranslation(),S=T.GetScale3D();const FQuat Q=T.GetRotation();
    return FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f"),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
}
static FString Header(const TCHAR* Prefix)
{
    FString R;for(const TCHAR* A:{TEXT("tx"),TEXT("ty"),TEXT("tz"),TEXT("qx"),TEXT("qy"),TEXT("qz"),TEXT("qw"),TEXT("sx"),TEXT("sy"),TEXT("sz")})
    {R+=FString::Printf(TEXT(",%s_%s"),Prefix,A);}return R;
}
static bool Wanted(const FString& N)
{
    static const TSet<FString> Main={TEXT("root"),TEXT("pelvis"),TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05"),TEXT("neck_01"),TEXT("neck_02"),TEXT("head"),
        TEXT("clavicle_l"),TEXT("clavicle_r"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),TEXT("ball_l"),TEXT("ball_r")};
    return Main.Contains(N)||N.Contains(TEXT("thumb"))||N.Contains(TEXT("index"))||N.Contains(TEXT("middle"))||N.Contains(TEXT("ring"))||N.Contains(TEXT("pinky"))||N.Contains(TEXT("twist"));
}
class FRun:public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;bCaptureAll=FParse::Param(FCommandLine::Get(),TEXT("RogueFighterStanceCaptureAll"));
        FString Label=TEXT("runtime");FParse::Value(FCommandLine::Get(),TEXT("RogueFighterStanceLabel="),Label);Label=FPaths::MakeValidFileName(Label);if(Label.IsEmpty()){Label=TEXT("runtime");}
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/fighter-combat-stance"),Label);
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/FighterCombatStance"),Label);
        IFileManager::Get().MakeDirectory(*Output,true);if(bCaptureAll){IFileManager::Get().MakeDirectory(*Images,true);}
        Frames=TEXT("frame,time_s,world_s,stage,alpha,requested,phase,attack,serial,charge,weapon,third,moving,falling,montage,montage_s,stance_sequence")+Header(TEXT("source_world"))+Header(TEXT("appearance_world"))+Header(TEXT("camera"))+Header(TEXT("view"))+TEXT(",control_pitch,control_yaw,control_roll\n");
        Poses=TEXT("frame,time_s,stage,bone,parent,hidden,length_ratio,step_cm,step_deg")+Header(TEXT("local"))+Header(TEXT("raw_cs"))+TEXT("\n");
        Skeletons=TEXT("stage,mesh,anim_class,bone,parent")+Header(TEXT("reference_local"))+TEXT("\n");
        CameraRows=TEXT("frame,third,motion_degrees,position_error_cm,rotation_error_deg\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished()const{return bFinished;}
private:
    void Check(bool Condition,const TCHAR* Message)
    {if(!Condition){++Failures;if(Failures<35){UE_LOG(LogRogue10m,Error,TEXT("FIGHTER_STANCE FAIL frame=%d %s"),FrameIndex,Message);}}}
    void Prepare()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid()||!Controller.IsValid()||!Controller->PlayerCameraManager||!Character->GetAppearanceCameraComponent()||!Character->GetCharacterAnimationComponent()||!Character->GetBasicBrawlerComponent()||!Character->GetCombatComponent()||!Character->GetMesh()->GetAnimInstance()||!Character->GetAnimationPlaybackMesh()->GetAnimInstance())
        {Check(false,TEXT("required_player_components_missing"));Finish();return;}
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Character->GetAnimationPlaybackMesh()->GetAnimInstance());
        Check(Stance.IsValid()&&Stance->FighterCombatStanceAnimation,TEXT("source_not_fighter_stance_parent_or_sequence_missing"));
        auto* State=Character->GetPlayerState<ARogue10mPlayerState>();if(!State){Check(false,TEXT("player_state_missing"));Finish();return;}
        const TCHAR* Paths[]={TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01")};
        for(const TCHAR* Path:Paths){auto* Skill=LoadObject<URogue10mAttackSkillData>(nullptr,Path);if(!Skill||!Skill->AttackMontage){Check(false,TEXT("required_attack_skill_missing"));Finish();return;}Skills.Add(Skill);InitialUses.Add(State->GetSkillUseCount(Skill->GetFName()));MontageSamples.Add(0);}
        if(Failures){Finish();return;}
        OldInput=Character->InputEnabled();Character->DisableInput(Controller.Get());OldDamage=Character->CanBeDamaged();Character->SetCanBeDamaged(false);
        OldControl=Controller->GetControlRotation();LockedControl=FRotator(-20.f,Character->GetActorRotation().Yaw,0.f);Controller->RotationInput=FRotator::ZeroRotator;Controller->SetControlRotation(LockedControl);bPrepared=true;
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);Character->GetCharacterAnimationComponent()->ResetFighterCombatStance();Character->GetCharacterMovement()->StopMovementImmediately();
        if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It){It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}It->GetCharacterMovement()->StopMovementImmediately();It->GetCharacterMovement()->DisableMovement();}
        Refill();RecordSkeleton(Character->GetAnimationPlaybackMesh(),TEXT("source"));RecordSkeleton(Character->GetMesh(),TEXT("appearance"));
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bOwnFixed=true;FApp::SetFixedDeltaTime(1.0/30.0);FApp::SetUseFixedTimeStep(true);
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("FIGHTER_STANCE TIMELINE frames=640 fps=30 jabs=35/38 tap=95/98 charge=150:276 walk=180:200 jump=326:332 V=105/130 knuckle=448/455/457 unsupported=510:530 menu=550:570 dodge=600 capture=%d"),bCaptureAll);
    }
    void Refill(){if(auto* A=Character->GetRogueAttributeSet()){A->SetStamina(A->GetMaxStamina());A->SetMana(A->GetMaxMana());A->SetIdentity(A->GetMaxIdentity());}}
    void Press(bool Primary){Refill();Character->GetCombatComponent()->HandleAttackPressed(Primary);}
    void Release(bool Primary){Character->GetCombatComponent()->HandleAttackReleased(Primary);}
    void Move(float Forward)
    {struct FParams{float Right=0.f;float Forward=0.f;}Params;Params.Forward=Forward;Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&Params);Character->GetCharacterAnimationComponent()->SetMovementState(!FMath::IsNearlyZero(Forward),false);}
    void V(EInputEvent Event){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,Event,Event==IE_Pressed?1.f:0.f));}
    void RecordSkeleton(USkeletalMeshComponent* Mesh,const TCHAR* Stage)
    {
        if(!Mesh||!Mesh->GetSkeletalMeshAsset()){Check(false,TEXT("skeleton_missing"));return;}
        const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        for(int32 I=0;I<Ref.GetNum();++I){const int32 P=Ref.GetParentIndex(I);Skeletons+=FString::Printf(TEXT("%s,%s,%s,%s,%s"),Stage,*Mesh->GetSkeletalMeshAsset()->GetPathName(),*Mesh->GetAnimInstance()->GetClass()->GetPathName(),*Ref.GetBoneName(I).ToString(),P==INDEX_NONE?TEXT(""):*Ref.GetBoneName(P).ToString())+Columns(Ref.GetRefBonePose()[I])+TEXT("\n");}
    }
    void RecordPose(USkeletalMeshComponent* Mesh,const TCHAR* Stage)
    {
        const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();const auto Local=Mesh->GetBoneSpaceTransformsView();TArray<FTransform> CS;CS.SetNum(Local.Num());
        static const TSet<FString> Fixed={TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r")};
        for(int32 I=0;I<Local.Num();++I)
        {
            const int32 Parent=Ref.GetParentIndex(I);CS[I]=Parent==INDEX_NONE?Local[I]:Local[I]*CS[Parent];const FString Bone=Ref.GetBoneName(I).ToString();if(!Wanted(Bone)){continue;}
            const FTransform& L=Local[I];const FTransform& C=CS[I];Check(!L.ContainsNaN()&&!C.ContainsNaN(),TEXT("raw_bone_nonfinite"));Check(L.GetRotation().IsNormalized(),TEXT("raw_rotation_not_normalized"));Check(L.GetScale3D().Equals(FVector::OneVector,.01),TEXT("raw_bone_scale_not_unit"));
            const double RefLength=Ref.GetRefBonePose()[I].GetTranslation().Length();const double Ratio=RefLength>.01?L.GetTranslation().Length()/RefLength:1.;if(Fixed.Contains(Bone)&&RefLength>.01){Check(FMath::Abs(Ratio-1.)<=.01,TEXT("limb_segment_length_changed_over_one_percent"));MaxLimbRatioError=FMath::Max(MaxLimbRatioError,FMath::Abs(Ratio-1.));}
            const FString Key=FString(Stage)+TEXT("/")+Bone;double Cm=0,Deg=0;if(const FTransform* Last=Previous.Find(Key)){Cm=FVector::Distance(Last->GetLocation(),C.GetLocation());Deg=FMath::RadiansToDegrees(Last->GetRotation().AngularDistance(C.GetRotation()));}
            Previous.Add(Key,C);MaxStepCm=FMath::Max(MaxStepCm,Cm);MaxStepDeg=FMath::Max(MaxStepDeg,Deg);
            Poses+=FString::Printf(TEXT("%d,%.9f,%s,%s,%s,%d,%.9f,%.9f,%.9f"),FrameIndex,FrameIndex/30.,Stage,*Bone,Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString(),Mesh->IsBoneHiddenByName(Ref.GetBoneName(I)),Ratio,Cm,Deg)+Columns(L)+Columns(C)+TEXT("\n");
        }
    }
    double HandHeight(const TCHAR* Stage)const
    {const FTransform* L=Previous.Find(FString(Stage)+TEXT("/hand_l"));const FTransform* R=Previous.Find(FString(Stage)+TEXT("/hand_r"));return L&&R?.5*(L->GetLocation().Z+R->GetLocation().Z):0.;}
    void SampleGrounding(bool Guard)
    {
        const TCHAR* Stages[]={TEXT("source"),TEXT("appearance")};const TCHAR* Feet[]={TEXT("foot_l"),TEXT("foot_r")};
        USkeletalMeshComponent* Meshes[]={Character->GetAnimationPlaybackMesh(),Character->GetMesh()};
        if(FrameIndex==6){RelaxedActorTransform=Character->GetActorTransform();for(int32 S=0;S<2;++S){RelaxedMeshTransform[S]=Meshes[S]->GetComponentTransform();}}
        if(Guard)
        {
            MaxStationaryTranslationErrorCm=FMath::Max(MaxStationaryTranslationErrorCm,FVector::Distance(RelaxedActorTransform.GetLocation(),Character->GetActorLocation()));
            MaxStationaryRotationErrorDegrees=FMath::Max(MaxStationaryRotationErrorDegrees,FMath::RadiansToDegrees(RelaxedActorTransform.GetRotation().AngularDistance(Character->GetActorQuat())));
            for(int32 S=0;S<2;++S){MaxStationaryTranslationErrorCm=FMath::Max(MaxStationaryTranslationErrorCm,FVector::Distance(RelaxedMeshTransform[S].GetLocation(),Meshes[S]->GetComponentLocation()));MaxStationaryRotationErrorDegrees=FMath::Max(MaxStationaryRotationErrorDegrees,FMath::RadiansToDegrees(RelaxedMeshTransform[S].GetRotation().AngularDistance(Meshes[S]->GetComponentQuat())));}
        }
        for(int32 S=0;S<2;++S)
        {
            for(int32 F=0;F<2;++F){if(const FTransform* Foot=Previous.Find(FString(Stages[S])+TEXT("/")+Feet[F])){const double Z=(*Foot*Meshes[S]->GetComponentTransform()).GetLocation().Z;if(Guard){GuardFootZ[S][F]+=Z;}else{RelaxedFootZ[S][F]+=Z;}}}
            if(const FTransform* Root=Previous.Find(FString(Stages[S])+TEXT("/root")))
            {
                const FTransform RootWorld=*Root*Meshes[S]->GetComponentTransform();
                if(!Guard){RelaxedRootZ[S]+=RootWorld.GetLocation().Z;if(FrameIndex==6){RelaxedRootQ[S]=RootWorld.GetRotation();}}
                else{GuardRootZ[S]+=RootWorld.GetLocation().Z;MaxRootTurnDegrees=FMath::Max(MaxRootTurnDegrees,FMath::RadiansToDegrees(RelaxedRootQ[S].AngularDistance(RootWorld.GetRotation())));}
            }
        }
    }
    void CameraCheck()
    {
        auto* Settings=Character->GetAppearanceCameraComponent();auto* Camera=Character->GetFirstPersonCameraComponent();auto* Body=Character->GetMesh();const bool Third=Settings->IsThirdPersonInspectionActive();
        auto* Arms=Character->GetFirstPersonMesh();Check(!Arms||((!Arms->IsVisible()||Arms->bHiddenInGame)&&!Arms->IsComponentTickEnabled()),TEXT("legacy_first_person_mesh_enabled"));
        const FTransform* Head=Previous.Find(TEXT("appearance/head"));if(!Head){Check(false,TEXT("raw_appearance_head_missing"));return;}
        const FReferenceSkeleton& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();const int32 H=Body->GetBoneIndex(Settings->HeadBone);if(H==INDEX_NONE){Check(false,TEXT("head_bone_missing"));return;}
        FQuat RefQ=Ref.GetRefBonePose()[H].GetRotation();for(int32 P=Ref.GetParentIndex(H);P!=INDEX_NONE;P=Ref.GetParentIndex(P)){RefQ=Ref.GetRefBonePose()[P].GetRotation()*RefQ;}
        const FQuat MeshQ=Body->GetComponentQuat();FQuat Raw=(MeshQ*(Head->GetRotation()*RefQ.Inverse())*MeshQ.Inverse()).GetNormalized();if(Raw.W<0){Raw=Raw*-1.;}
        FVector Axis;double Angle=0;Raw.ToAxisAndAngle(Axis,Angle);const bool Stabilized=Settings->IsBrawlerViewStabilized();const double Weight=Stabilized?FMath::Clamp(Settings->BrawlerHeadRotationWeight,0.f,1.f):1.;const double Limit=FMath::DegreesToRadians(Stabilized?FMath::Clamp(Settings->BrawlerHeadMaxRotationDegrees,0.f,180.f):180.f);Angle=FMath::Min(Angle*Weight,Limit);
        const FQuat Motion=Angle>UE_SMALL_NUMBER?FQuat(Axis.GetSafeNormal(),Angle):FQuat::Identity;const FTransform WorldHead=*Head*Body->GetComponentTransform();const FVector ExpectedP=WorldHead.GetLocation()+(Motion*Character->GetActorQuat()).RotateVector(Settings->EyeOffset);const FQuat ExpectedQ=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
        const double PositionError=FVector::Distance(Camera->GetComponentLocation(),ExpectedP);const double RotationError=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(ExpectedQ));const double MotionDegrees=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(Controller->GetControlRotation().Quaternion()));
        Check(PositionError<.05&&RotationError<.02,TEXT("head_camera_transform_changed_by_stance_or_V"));if(Stabilized){MaxCameraDegrees=FMath::Max(MaxCameraDegrees,MotionDegrees);Check(MotionDegrees<=Settings->BrawlerHeadMaxRotationDegrees+.02,TEXT("head_camera_exceeds_eight_degree_cap"));}
        if(!Third){++FirstSamples;const auto& View=Controller->PlayerCameraManager->GetCameraCacheView();Check(FVector::Distance(View.Location,ExpectedP)<.05&&FMath::RadiansToDegrees(View.Rotation.Quaternion().AngularDistance(ExpectedQ))<.02,TEXT("first_person_cache_not_head_camera"));}else{++ThirdSamples;Check(FVector::Distance(Controller->PlayerCameraManager->GetCameraCacheView().Location,Camera->GetComponentLocation())>30.,TEXT("V_view_not_separate_from_gameplay_camera"));}
        CameraRows+=FString::Printf(TEXT("%d,%d,%.9f,%.9f,%.9f\n"),FrameIndex,Third,MotionDegrees,PositionError,RotationError);
    }
    const TCHAR* Stage()const
    {
        if(FrameIndex<35)return TEXT("normal_idle");if(FrameIndex<66)return TEXT("jab_pair");if(FrameIndex<95)return TEXT("unarmed_guard");if(FrameIndex<150)return TEXT("straight_and_V");if(FrameIndex<180)return TEXT("charge");if(FrameIndex<200)return TEXT("charge_walk");if(FrameIndex<264)return TEXT("charge_stand");if(FrameIndex<276)return TEXT("long_charge");if(FrameIndex<326)return TEXT("hook");if(FrameIndex<367)return TEXT("jump_from_guard");if(FrameIndex<390)return TEXT("landed_guard");if(FrameIndex<420)return TEXT("hold_then_lower");if(FrameIndex<448)return TEXT("returned_idle");if(FrameIndex<510)return TEXT("knuckle");if(FrameIndex<530)return TEXT("unsupported_weapon");if(FrameIndex<550)return TEXT("charge_before_menu");if(FrameIndex<570)return TEXT("blocking_menu");if(FrameIndex<600)return TEXT("menu_cancelled");return TEXT("dodge_guard");
    }
    void Frame()
    {
        if(!Character.IsValid()||!Controller.IsValid()||!Stance.IsValid()){Check(false,TEXT("player_or_stance_instance_lost"));Finish();return;}
        Controller->RotationInput=FRotator::ZeroRotator;Check(Controller->GetControlRotation().Equals(LockedControl,.01),TEXT("test_aim_changed"));
        auto* Source=Character->GetAnimationPlaybackMesh();auto* Body=Character->GetMesh();auto* Animation=Character->GetCharacterAnimationComponent();auto* Combat=Character->GetCombatComponent();const auto Snapshot=Character->GetBasicBrawlerComponent()->GetSnapshot();const float Alpha=Stance->FighterCombatStanceAlpha;const bool Requested=Animation->IsFighterCombatStanceRequested();auto* Montage=Source->GetAnimInstance()->GetCurrentActiveMontage();
        Check(FMath::IsFinite(Alpha)&&Alpha>=0&&Alpha<=1,TEXT("stance_alpha_nonfinite_or_outside_unit_range"));
        const auto& View=Controller->PlayerCameraManager->GetCameraCacheView();const FRotator Control=Controller->GetControlRotation();
        Frames+=FString::Printf(TEXT("%d,%.9f,%.9f,%s,%.9f,%d,%d,%d,%d,%.9f,%d,%d,%d,%d,%s,%.9f,%s"),FrameIndex,FrameIndex/30.,World->GetTimeSeconds(),Stage(),Alpha,Requested,static_cast<int32>(Snapshot.Phase),static_cast<int32>(Snapshot.Attack),Snapshot.AcceptedSerial,Snapshot.ChargeAlpha,static_cast<int32>(Character->GetEquippedWeaponType()),Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),Animation->HasMovementInput(),Character->GetCharacterMovement()->IsFalling(),Montage?*Montage->GetPathName():TEXT(""),Montage?Source->GetAnimInstance()->Montage_GetPosition(Montage):-1.,Stance->FighterCombatStanceAnimation?*Stance->FighterCombatStanceAnimation->GetPathName():TEXT(""))+Columns(Source->GetComponentTransform())+Columns(Body->GetComponentTransform())+Columns(Character->GetFirstPersonCameraComponent()->GetComponentTransform())+Columns(FTransform(View.Rotation,View.Location))+FString::Printf(TEXT(",%.9f,%.9f,%.9f\n"),Control.Pitch,Control.Yaw,Control.Roll);
        RecordPose(Source,TEXT("source"));RecordPose(Body,TEXT("appearance"));if(FrameIndex>0){CameraCheck();}
        for(int32 I=0;I<Skills.Num();++I){if(Montage&&Montage==Skills[I]->AttackMontage){++MontageSamples[I];}}
        if(FrameIndex>=6&&FrameIndex<=24){Check(Alpha<.001&&!Requested,TEXT("fresh_idle_not_relaxed"));RelaxedSource+=HandHeight(TEXT("source"));RelaxedBody+=HandHeight(TEXT("appearance"));SampleGrounding(false);++RelaxedSamples;}
        if(FrameIndex>=66&&FrameIndex<=90&&!Montage){Check(Alpha>.98&&Requested,TEXT("accepted_jab_did_not_hold_combat_guard"));GuardSource+=HandHeight(TEXT("source"));GuardBody+=HandHeight(TEXT("appearance"));SampleGrounding(true);++GuardSamples;}
        // Walking starts at 180; the configured 0.45s lower blend needs 14 ticks.
        // Observe its completed state after that duration rather than during it.
        if(FrameIndex>=195&&FrameIndex<200){Check(Alpha<.01,TEXT("full_body_guard_did_not_yield_to_walk"));++WalkSamples;}
        if(FrameIndex>=211&&FrameIndex<=217){Check(Alpha>.98&&Requested,TEXT("standing_did_not_restore_charge_guard"));++RestoredSamples;}
        if(FrameIndex>=341&&FrameIndex<=356&&Character->GetCharacterMovement()->IsFalling()){Check(Alpha<.01,TEXT("full_body_guard_did_not_yield_to_jump"));++JumpSamples;}
        if(FrameIndex>=372&&FrameIndex<=386&&Character->GetCharacterMovement()->IsMovingOnGround()){Check(Alpha>.98&&Requested,TEXT("landing_before_hold_expiry_did_not_restore_guard"));++LandedGuardSamples;}
        if(FrameIndex>=264&&FrameIndex<=273){Check(Snapshot.Phase==ERogue10mBasicBrawlerPhase::Charging&&Alpha>.98&&Requested,TEXT("charge_longer_than_hold_lost_guard"));++LongChargeSamples;}
        if(FrameIndex>=420&&FrameIndex<=440){Check(!Requested&&Alpha<.001&&!Montage,TEXT("guard_did_not_expire_to_normal_idle"));++ExpiredSamples;}
        if(FrameIndex>=480&&FrameIndex<=500&&!Montage){Check(Alpha>.98&&Requested,TEXT("knuckle_attack_did_not_hold_guard"));++KnuckleGuardSamples;}
        if(FrameIndex>=512&&FrameIndex<=529){Check(!Requested,TEXT("unsupported_weapon_did_not_clear_request"));if(FrameIndex>=526){Check(Alpha<.001,TEXT("unsupported_weapon_did_not_lower_guard"));}}
        if(FrameIndex>=552&&FrameIndex<=569){Check(Controller->IsAnyBlockingWindowVisible()&&!Requested,TEXT("blocking_menu_did_not_clear_request"));if(FrameIndex>=566){Check(Alpha<.001,TEXT("blocking_menu_did_not_lower_guard"));}}
        if(FrameIndex>=579&&FrameIndex<=595){Check(!Requested&&Alpha<.001&&Snapshot.Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("menu_charge_reappeared_after_close"));}
        if(FrameIndex>=625&&FrameIndex<=638&&!Montage){Check(Alpha>.98&&Requested,TEXT("dodge_did_not_restore_guard"));++DodgeGuardSamples;}
        if(FrameIndex==35||FrameIndex==38){Press(true);Release(true);}if(FrameIndex==95||FrameIndex==150||FrameIndex==536){Press(false);}if(FrameIndex==98||FrameIndex==276||FrameIndex==571){Release(false);}
        if(FrameIndex==105||FrameIndex==130){V(IE_Pressed);}if(FrameIndex==106||FrameIndex==131){V(IE_Released);}
        if(FrameIndex==180){WalkStart=Character->GetActorLocation();}if(FrameIndex>=180&&FrameIndex<200){Move(-.25f);MaxWalkCm=FMath::Max(MaxWalkCm,static_cast<double>(FVector::Dist2D(WalkStart,Character->GetActorLocation())));}if(FrameIndex==200){Move(0);Character->GetCharacterMovement()->StopMovementImmediately();}
        if(FrameIndex==326){JumpStartZ=Character->GetActorLocation().Z;Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpStart")),nullptr);}if(FrameIndex==332){Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpEnd")),nullptr);}if(FrameIndex>=326&&FrameIndex<396){MaxJumpCm=FMath::Max(MaxJumpCm,static_cast<double>(Character->GetActorLocation().Z-JumpStartZ));}
        if(FrameIndex==448){Check(Snapshot.AcceptedSerial==4,TEXT("unarmed_jab_jab_straight_hook_not_all_accepted"));Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);Combat->UnlockAttackSkill(Skills[4].Get());Check(Combat->AssignSkillToInputSlot(Skills[4].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary_assignment_failed"));}
        if(FrameIndex==455){Press(true);}if(FrameIndex==457){Release(true);}
        if(FrameIndex==510){Character->SetEquippedWeaponType(ERogue10mWeaponType::Dagger);}if(FrameIndex==530){Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);}
        if(FrameIndex==550){Controller->ToggleInventory();Check(Controller->IsInventoryVisible(),TEXT("inventory_fixture_did_not_open"));}if(FrameIndex==570){Controller->CloseAllBlockingPanels();}
        if(FrameIndex==600){Refill();Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoDodge")),nullptr);}
        if(bCaptureAll&&FrameIndex<640){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false);}
        if(FrameIndex==640){CompleteChecks();Finish();return;}++FrameIndex;
    }
    void CompleteChecks()
    {
        auto* State=Character->GetPlayerState<ARogue10mPlayerState>();for(int32 I=0;I<Skills.Num();++I){const int32 Uses=State?State->GetSkillUseCount(Skills[I]->GetFName())-InitialUses[I]:-1;Check(Uses==1,TEXT("skill_not_accepted_exactly_once_or_menu_release_attacked"));Check(MontageSamples[I]>=3,TEXT("accepted_skill_montage_not_evaluated"));UE_LOG(LogRogue10m,Display,TEXT("FIGHTER_STANCE SKILL_RESULT skill=%s uses=%d montage_samples=%d"),*Skills[I]->GetName(),Uses,MontageSamples[I]);}
        Check(RelaxedSamples>=10&&GuardSamples>=10,TEXT("relaxed_and_guard_sampling_insufficient"));SourceRaise=GuardSamples&&RelaxedSamples?GuardSource/GuardSamples-RelaxedSource/RelaxedSamples:0.;BodyRaise=GuardSamples&&RelaxedSamples?GuardBody/GuardSamples-RelaxedBody/RelaxedSamples:0.;Check(SourceRaise>10&&BodyRaise>10,TEXT("guard_did_not_visibly_raise_both_meshes_hands"));
        if(RelaxedSamples&&GuardSamples)
        {
            for(int32 S=0;S<2;++S){for(int32 F=0;F<2;++F){MaxFootHeightDeltaCm=FMath::Max(MaxFootHeightDeltaCm,FMath::Abs(GuardFootZ[S][F]/GuardSamples-RelaxedFootZ[S][F]/RelaxedSamples));}MaxRootHeightDeltaCm=FMath::Max(MaxRootHeightDeltaCm,FMath::Abs(GuardRootZ[S]/GuardSamples-RelaxedRootZ[S]/RelaxedSamples));}
            // Imported root helpers can carry an offset cancelled by the pelvis.
            // Ground contact and component/capsule transforms are the invariants;
            // root height/yaw remain diagnostic data, not anatomical assertions.
            Check(MaxFootHeightDeltaCm<=3.,TEXT("standing_guard_feet_changed_ground_height_over_three_cm"));
            Check(MaxStationaryTranslationErrorCm<.05&&MaxStationaryRotationErrorDegrees<.05,TEXT("standing_guard_displaced_mesh_component_or_capsule"));
        }
        Check(WalkSamples>=5&&RestoredSamples>=5&&JumpSamples>=5&&LandedGuardSamples>=5&&LongChargeSamples>=5&&ExpiredSamples>=10&&KnuckleGuardSamples>=5&&DodgeGuardSamples>=5,TEXT("required_transition_windows_not_observed"));Check(MaxWalkCm>5&&MaxJumpCm>10,TEXT("actual_walk_or_jump_not_observed"));Check(FirstSamples>500&&ThirdSamples>15,TEXT("both_view_modes_not_observed"));Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("jump_never_landed"));
    }
    void Finish(bool Exit=true)
    {
        if(bFinished)return;bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        for(const auto& File:TArray<TPair<FString,FString>>{{TEXT("frames.csv"),Frames},{TEXT("poses.csv"),Poses},{TEXT("camera.csv"),CameraRows},{TEXT("skeletons.csv"),Skeletons}}){Check(FFileHelper::SaveStringToFile(File.Value,*FPaths::Combine(Output,File.Key)),TEXT("evidence_write_failed"));}
        const FString Metrics=FString::Printf(TEXT("frames=%d\nfailures=%d\nsource_hand_raise_cm=%.9f\nappearance_hand_raise_cm=%.9f\nmax_camera_motion_deg=%.9f\nmax_limb_ratio_error=%.9f\nmax_bone_step_cm=%.9f\nmax_bone_step_deg=%.9f\nwalk_cm=%.9f\njump_cm=%.9f\nmax_foot_ground_height_delta_cm=%.9f\nmax_root_height_delta_cm=%.9f\nmax_root_turn_degrees=%.9f\nrelaxed_samples=%d\nguard_samples=%d\nwalk_samples=%d\nrestored_samples=%d\njump_samples=%d\nlong_charge_samples=%d\nexpired_samples=%d\nknuckle_guard_samples=%d\ndodge_guard_samples=%d\nfirst_person_samples=%d\nthird_person_samples=%d\nNOTE=Gameplay input policy and protected movement/dodge entry points are invoked in an isolated pawn; V travels through PlayerController InputKey. Numeric bone checks do not certify fine mesh deformation.\n"),FrameIndex,Failures,SourceRaise,BodyRaise,MaxCameraDegrees,MaxLimbRatioError,MaxStepCm,MaxStepDeg,MaxWalkCm,MaxJumpCm,MaxFootHeightDeltaCm,MaxRootHeightDeltaCm,MaxRootTurnDegrees,RelaxedSamples,GuardSamples,WalkSamples,RestoredSamples,JumpSamples,LongChargeSamples,ExpiredSamples,KnuckleGuardSamples,DodgeGuardSamples,FirstSamples,ThirdSamples);
        Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        if(bPrepared&&Character.IsValid()){Character->SetCanBeDamaged(OldDamage);if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}if(Controller.IsValid()){Controller->CloseAllBlockingPanels();Controller->SetControlRotation(OldControl);if(OldInput){Character->EnableInput(Controller.Get());}}}
        if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}const bool Passed=Failures==0&&FrameIndex==640;
        UE_LOG(LogRogue10m,Display,TEXT("FIGHTER_STANCE METRICS source_raise=%.6f appearance_raise=%.6f camera_degrees=%.6f limb_error=%.9f walk=%.6f jump=%.6f"),SourceRaise,BodyRaise,MaxCameraDegrees,MaxLimbRatioError,MaxWalkCm,MaxJumpCm);
        UE_LOG(LogRogue10m,Display,TEXT("FIGHTER_STANCE GROUND foot_height_delta_cm=%.9f stationary_component_translation_error_cm=%.9f stationary_component_rotation_error_deg=%.9f root_height_delta_diagnostic_cm=%.9f root_turn_diagnostic_deg=%.9f landed_guard_samples=%d"),MaxFootHeightDeltaCm,MaxStationaryTranslationErrorCm,MaxStationaryRotationErrorDegrees,MaxRootHeightDeltaCm,MaxRootTurnDegrees,LandedGuardSamples);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIGHTER_COMBAT_STANCE_RUNTIME_%s frames=%d failures=%d output=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Failures,*Output);if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> Stance;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;TArray<int32> InitialUses,MontageSamples;TMap<FString,FTransform> Previous;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Frames,Poses,Skeletons,CameraRows;FRotator OldControl,LockedControl;
    int32 FrameIndex=0,Failures=0,RelaxedSamples=0,GuardSamples=0,WalkSamples=0,RestoredSamples=0,JumpSamples=0,LandedGuardSamples=0,LongChargeSamples=0,ExpiredSamples=0,KnuckleGuardSamples=0,DodgeGuardSamples=0,FirstSamples=0,ThirdSamples=0;
    double RelaxedSource=0,RelaxedBody=0,GuardSource=0,GuardBody=0,SourceRaise=0,BodyRaise=0,MaxCameraDegrees=0,MaxLimbRatioError=0,MaxStepCm=0,MaxStepDeg=0,MaxWalkCm=0,MaxJumpCm=0,JumpStartZ=0,OldDelta=1./30.;FVector WalkStart=FVector::ZeroVector;
    double RelaxedFootZ[2][2]={{0,0},{0,0}},GuardFootZ[2][2]={{0,0},{0,0}},RelaxedRootZ[2]={0,0},GuardRootZ[2]={0,0};FQuat RelaxedRootQ[2]={FQuat::Identity,FQuat::Identity};double MaxFootHeightDeltaCm=0,MaxRootHeightDeltaCm=0,MaxRootTurnDegrees=0;
    FTransform RelaxedActorTransform,RelaxedMeshTransform[2];double MaxStationaryTranslationErrorCm=0,MaxStationaryRotationErrorDegrees=0;
    bool bFinished=false,bPrepared=false,bCaptureAll=false,bOwnFixed=false,OldFixed=false,OldInput=false,OldDamage=true;
};
static TSharedPtr<FRun> Active;
static void Run()
{if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished()))return;for(const FWorldContext& Context:GEngine->GetWorldContexts()){if(UWorld* W=Context.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestFighterCombatStance"),TEXT("Integrated relaxed/combat guard, >3s charge, locomotion, V, Knuckle, weapon/menu cancellation and dodge. -RogueFighterStanceLabel=name -RogueFighterStanceCaptureAll"),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
