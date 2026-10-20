// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Rogue10mAppearanceShadowComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mStylizedCharacter.h"
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
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Real combat-input regression. Test-only skill unlocks/resources never modify Data Assets.
// Pose/camera assertions do not replace visual review or actual hit-geometry testing.
namespace Rogue10mFistCombatTest
{
static FString TransformColumns(const FTransform& T)
{
    const FVector P=T.GetTranslation(), S=T.GetScale3D(); const FQuat Q=T.GetRotation();
    return FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f"),
        P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
}
static FString TransformHeader(const TCHAR* Prefix)
{
    FString Result;
    for (const TCHAR* Axis : {TEXT("tx"),TEXT("ty"),TEXT("tz"),TEXT("qx"),TEXT("qy"),TEXT("qz"),TEXT("qw"),TEXT("sx"),TEXT("sy"),TEXT("sz")})
    { Result+=FString::Printf(TEXT(",%s_%s"),Prefix,Axis); }
    return Result;
}
static bool WantedBone(const FString& Name)
{
    static const TSet<FString> Main={TEXT("root"),TEXT("pelvis"),TEXT("head"),TEXT("neck_01"),TEXT("neck_02"),
        TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05"),
        TEXT("clavicle_l"),TEXT("clavicle_r"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),
        TEXT("hand_l"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),TEXT("ball_l"),TEXT("ball_r")};
    return Main.Contains(Name) || Name.Contains(TEXT("twist")) || Name.Contains(TEXT("thumb")) || Name.Contains(TEXT("index")) ||
        Name.Contains(TEXT("middle")) || Name.Contains(TEXT("ring")) || Name.Contains(TEXT("pinky"));
}
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld; bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueFistCapture"));
        bLocomotion=true; FrameLimit=840; bFirstPersonOnly=FParse::Param(FCommandLine::Get(),TEXT("RogueFistFirstPersonOnly")); bCaptureAll=FParse::Param(FCommandLine::Get(),TEXT("RogueFistCaptureAll")); bCaptureKeyframes=FParse::Param(FCommandLine::Get(),TEXT("RogueFistCaptureKeyframes")); bCapture|=bCaptureAll || bCaptureKeyframes; bDamageTarget=FParse::Param(FCommandLine::Get(),TEXT("RogueFistDamageTarget"));
        FString Label=TEXT("capture"); FParse::Value(FCommandLine::Get(),TEXT("RogueFistLabel="),Label);
        Label=FPaths::MakeValidFileName(Label); if (Label.IsEmpty()) { Label=TEXT("capture"); }
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/fist-combat"),Label);
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/FistCombat"),Label);
        IFileManager::Get().MakeDirectory(*Output,true); if (bCapture) { IFileManager::Get().MakeDirectory(*Images,true); }
        Poses=TEXT("frame,time_s,stage,bone,parent,hidden,local_length,reference_length,length_ratio,step_cm,step_deg")+TransformHeader(TEXT("local"))+TransformHeader(TEXT("raw_cs"))+TransformHeader(TEXT("rendered_cs"))+TEXT("\n");
        Frames=TEXT("frame,time_s,world_s,phase,attack,serial,attack_elapsed,charge,third,montage,montage_s,clip,clip_s")+
            TransformHeader(TEXT("source_world"))+TransformHeader(TEXT("appearance_world"))+TransformHeader(TEXT("camera"))+TransformHeader(TEXT("view"))+TEXT(",control_pitch,control_yaw,control_roll,capsule_x,capsule_y,capsule_z,is_falling,weapon,active_skill\n");
        CameraRows=TEXT("frame,raw_head_degrees,view_motion_degrees,expected_motion_degrees,position_error_cm,rotation_error_deg,third,shadow_position_error_cm,shadow_rotation_error_deg,head_residual_degrees,head_limit_degrees,position_limit_cm,head_anchor_offset_cm\n");
        SkillRows=TEXT("frame,time_s,skill,use_count_delta,montage,rate,expected_first_pulse_s,hit_count,hit_interval_s,note\n");
        DamageRows=TEXT("frame,time_s,world_s,skill,health,damage,pulse,elapsed_since_accept,expected_first_delay,first_delay_error,target_x,target_y,target_z\n");
        Skeletons=TEXT("stage,mesh,anim_class,bone,parent")+TransformHeader(TEXT("reference_local"))+TEXT("\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Bootstrap();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; if (Failures<30) { UE_LOG(LogRogue10m,Error,TEXT("FIST_COMBAT FAIL frame=%d %s"),FrameIndex,Message); } }
    }
    void Bootstrap()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        FString ClassPath;
        if(!FParse::Value(FCommandLine::Get(),TEXT("RogueFistCharacterClass="),ClassPath)){Prepare();return;}
        UClass* Class=LoadClass<ARogue10mCharacter>(nullptr,*ClassPath);
        if(!Character.IsValid() || !Controller.IsValid() || !Class || !Class->IsChildOf(ARogue10mCharacter::StaticClass()))
        {Check(false,TEXT("character_override_invalid"));Finish();return;}
        OriginalPawn=Character;
        const FTransform SpawnTransform=Character->GetActorTransform();
        OriginalPawn->SetActorEnableCollision(false);
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARogue10mCharacter* Spawned=World->SpawnActor<ARogue10mCharacter>(Class,SpawnTransform,Params);
        if(!Spawned){Check(false,TEXT("character_override_spawn_failed"));Finish();return;}
        Controller->Possess(Spawned);Controller->SetViewTarget(Spawned);Character=Spawned;
        if(auto* Stylized=Cast<ARogue10mStylizedCharacter>(Spawned))
        {
            FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("Fist combat fixture");
            Profile.Appearance.Race=Stylized->GetAppearanceRace();Profile.Appearance.Gender=Stylized->GetAppearanceGender();
            Check(Stylized->ApplyCharacterProfile(Profile),TEXT("matching_race_profile_apply_failed"));
            if(Failures){Finish();return;}
        }
        OriginalPawn->GetAppearanceCameraComponent()->RefreshAppearanceCamera();OriginalPawn->SetActorHiddenInGame(true);
        UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT OVERRIDE class=%s spawned=%s warmup_seconds=2"),*ClassPath,*Spawned->GetName());
        World->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),2.f,false);
    }
    bool LoadSkills()
    {
        const TCHAR* Paths[]={
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged")};
        auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
        if(!State){Check(false,TEXT("player_state_missing"));return false;}
        for(const TCHAR* Path:Paths)
        {
            auto* Skill=LoadObject<URogue10mAttackSkillData>(nullptr,Path);
            if(!Skill || !Skill->AttackMontage){Check(false,TEXT("required_skill_or_montage_missing"));return false;}
            Skills.Add(Skill);InitialUses.Add(State->GetSkillUseCount(Skill->GetFName()));LastUses.Add(InitialUses.Last());MontageSamples.Add(0);DamageCounts.Add(0);AcceptanceTimes.Add(-1.0);ExpectedDelays.Add(0.0);FirstDamageErrors.Add(0.0);
        }
        ChargedRelease=550+FMath::CeilToInt(Skills[8]->ChargeSeconds*30.f)+3;
        Check(ChargedRelease<610,TEXT("charge_duration_outside_test_window"));
        return Failures==0;
    }
    void RefillResources()
    {
        if(auto* A=Character->GetRogueAttributeSet())
        {A->SetStamina(A->GetMaxStamina());A->SetMana(A->GetMaxMana());A->SetIdentity(A->GetMaxIdentity());}
    }
    void PrepareKnuckle()
    {
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
        auto* Combat=Character->GetCombatComponent();
        for(int32 I=4;I<Skills.Num();++I){Combat->UnlockAttackSkill(Skills[I].Get());Check(Combat->IsAttackSkillUnlocked(Skills[I].Get()),TEXT("test_skill_unlock_failed"));}
        Check(Combat->AssignSkillToInputSlot(Skills[4].Get(),ERogue10mAttackInputSlot::Primary),TEXT("primary01_assign_failed"));
        Check(Combat->AssignSkillToInputSlot(Skills[7].Get(),ERogue10mAttackInputSlot::Special),TEXT("special_assign_failed"));
        Check(Combat->AssignSkillToInputSlot(Skills[8].Get(),ERogue10mAttackInputSlot::ChargedPrimary),TEXT("charged_assign_failed"));
        RefillResources();
    }
    void RecordSkills(UAnimMontage* Montage)
    {
        auto* State=Character->GetPlayerState<ARogue10mPlayerState>();
        for(int32 I=0;I<Skills.Num();++I)
        {
            const auto* Skill=Skills[I].Get();const int32 Uses=State->GetSkillUseCount(Skill->GetFName());
            if(Montage && Montage==Skill->AttackMontage){++MontageSamples[I];}
            if(Uses!=LastUses[I])
            {
                const float Speed=Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier();
                const float Rate=FMath::Max(.01f,FMath::Clamp(Speed,.1f,5.f)*Skill->AnimationPlayRate);
                SkillRows+=FString::Printf(TEXT("%d,%.9f,%s,%d,%s,%.9f,%.9f,%d,%.9f,expected_schedule_not_actual_damage\n"),
                    FrameIndex,FrameIndex/30.0,*Skill->GetPathName(),Uses-InitialUses[I],*Skill->AttackMontage->GetPathName(),Rate,
                    Skill->HitStartDelaySeconds/Rate,Skill->HitCount,Skill->HitInterval/FMath::Max(.01f,Speed));
                UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT ACCEPT frame=%d skill=%s count=%d montage=%s first_delay=%.6f pulses=%d interval=%.6f"),
                    FrameIndex,*Skill->GetName(),Uses-InitialUses[I],*Skill->AttackMontage->GetName(),Skill->HitStartDelaySeconds/Rate,Skill->HitCount,Skill->HitInterval/FMath::Max(.01f,Speed));
                if(bDamageTarget)
                {DamageSkillIndex=I;AcceptanceTimes[I]=World->GetTimeSeconds();ExpectedDelays[I]=Skill->HitStartDelaySeconds/Rate;}
                LastUses[I]=Uses;
            }
        }
    }
    void RecordDamage()
    {
        if(!bDamageTarget || !DamageMonster.IsValid()){return;}
        const double Current=DamageMonster->GetRogueAttributeSet()->GetHealth();
        const double Drop=PreviousHealth-Current;const double Now=World->GetTimeSeconds();
        const bool HasSkill=Skills.IsValidIndex(DamageSkillIndex);
        const double Elapsed=HasSkill?Now-AcceptanceTimes[DamageSkillIndex]:-1.0;
        double Error=0.0;
        if(Drop>.01)
        {
            Check(HasSkill,TEXT("damage_before_any_accepted_skill"));
            if(HasSkill)
            {
                ++DamageCounts[DamageSkillIndex];
                if(DamageCounts[DamageSkillIndex]==1)
                {
                    Error=Elapsed-ExpectedDelays[DamageSkillIndex];FirstDamageErrors[DamageSkillIndex]=Error;
                    // Acceptance is observed on the next post-actor tick. One frame plus 10ms
                    // permits this sampling offset and timer quantization, not arbitrary delay.
                    Check(FMath::Abs(Error)<=1.0/30.0+.01,TEXT("first_damage_outside_expected_delay_tolerance"));
                }
                UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT DAMAGE frame=%d skill=%s pulse=%d health=%.3f drop=%.3f elapsed=%.6f first_expected=%.6f first_error=%.6f"),
                    FrameIndex,*Skills[DamageSkillIndex]->GetName(),DamageCounts[DamageSkillIndex],Current,Drop,Elapsed,ExpectedDelays[DamageSkillIndex],Error);
            }
        }
        const FVector P=DamageMonster->GetActorLocation();
        DamageRows+=FString::Printf(TEXT("%d,%.9f,%.9f,%s,%.6f,%.6f,%d,%.9f,%.9f,%.9f,%.6f,%.6f,%.6f\n"),
            FrameIndex,FrameIndex/30.0,Now,HasSkill?*Skills[DamageSkillIndex]->GetPathName():TEXT(""),Current,FMath::Max(0.0,Drop),
            HasSkill?DamageCounts[DamageSkillIndex]:0,Elapsed,HasSkill?ExpectedDelays[DamageSkillIndex]:0.0,Error,P.X,P.Y,P.Z);
        PreviousHealth=Current;
    }
    void CheckCameraAndShadow()
    {
        auto* Body=Character->GetMesh();auto* Settings=Character->GetAppearanceCameraComponent();auto* Camera=Character->GetFirstPersonCameraComponent();
        auto* Arms=Character->GetFirstPersonMesh();
        Check(!Arms || ((!Arms->IsVisible() || Arms->bHiddenInGame) && !Arms->IsComponentTickEnabled()),TEXT("first_person_mesh_enabled"));
        const FTransform* HeadCS=Previous.Find(TEXT("appearance/head"));if(!HeadCS){Check(false,TEXT("recorded_head_missing"));return;}
        const FReferenceSkeleton& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
        const int32 HeadIndex=Body->GetBoneIndex(Settings->HeadBone);
        if(HeadIndex==INDEX_NONE){Check(false,TEXT("head_bone_missing"));return;}
        FTransform RefHead=Ref.GetRefBonePose()[HeadIndex];
        for(int32 Parent=Ref.GetParentIndex(HeadIndex);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent)){RefHead*=Ref.GetRefBonePose()[Parent];}
        const FQuat MeshQ=Body->GetComponentQuat();
        const FQuat Raw=(MeshQ*(HeadCS->GetRotation()*RefHead.GetRotation().Inverse())*MeshQ.Inverse()).GetNormalized();
        auto* Source=Character->GetAnimationPlaybackMesh();
        auto* Animation=Source?Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()):nullptr;
        Check(Settings->bStabilizeAllHeadMotion && Animation && Animation->bMouseDrivenHeadApplied,TEXT("mouse_separated_head_mode_missing"));
        if(!Animation || !Animation->bMouseDrivenHeadApplied){return;}
        Check(Animation->MouseDrivenHeadWorldAim.IsNormalized() && Animation->MouseDrivenHeadSourceRotation.IsNormalized(),TEXT("evaluated_head_basis_not_normalized"));
        // RecordPose has completed both evaluations. Transport their evaluated mouse basis
        // if the actor/component moved later; never remove a later controller sample.
        const FQuat Transport=(Source->GetComponentQuat()*Animation->MouseDrivenHeadSourceRotation.Inverse()).GetNormalized();
        const FQuat EvaluatedAim=(Transport*Animation->MouseDrivenHeadWorldAim*Transport.Inverse()).GetNormalized();
        FQuat Residual=(Raw*EvaluatedAim.Inverse()).GetNormalized();
        if(Residual.W<0){Residual=Residual*-1.;}
        FVector Axis;double Angle=0;Residual.ToAxisAndAngle(Axis,Angle);
        const auto* Combat=Character->GetCombatComponent();
        const URogue10mAttackSkillData* Accepted=Combat?Combat->GetLastAcceptedAttackSkill():nullptr;
        const UAnimMontage* Attack=Accepted?Accepted->AttackMontage.Get():nullptr;
        const FAnimMontageInstance* AttackInstance=Attack?Animation->GetInstanceForMontage(Attack):nullptr;
        const UAnimMontage* Dodge=CameraDodgeMontage.Get();
        const FAnimMontageInstance* DodgeInstance=Dodge?Animation->GetInstanceForMontage(Dodge):nullptr;
        const bool Attacking=Attack && (Animation->GetCurrentActiveMontage()==Attack || (AttackInstance && AttackInstance->GetWeight()>UE_SMALL_NUMBER));
        const bool Dodging=DodgeInstance && (DodgeInstance->IsActive() || DodgeInstance->GetWeight()>UE_SMALL_NUMBER);
        const UCharacterMovementComponent* Movement=Character->GetCharacterMovement();
        const bool Busy=Attacking || (Combat && Combat->IsAttackChargeHeld()) || Dodging || Movement->IsFalling();
        const bool Running=Character->IsSprinting() && Movement->Velocity.Size2D()>10.f;
        const double MaxDegrees=Busy?FMath::Clamp(Settings->AttackingHeadMaxRotationDegrees,0.f,3.f):
            (Running?FMath::Clamp(Settings->RunningHeadMaxRotationDegrees,0.f,4.f):FMath::Clamp(Settings->WalkingHeadMaxRotationDegrees,0.f,2.f));
        const double PositionLimit=FMath::Clamp(Busy?Settings->AttackingHeadMaxPositionOffset:
            (Running?Settings->RunningHeadMaxPositionOffset:Settings->WalkingHeadMaxPositionOffset),0.f,5.f);
        const double ExpectedAngle=FMath::Min(Angle,FMath::DegreesToRadians(MaxDegrees));
        const FQuat Motion=ExpectedAngle>UE_SMALL_NUMBER?FQuat(Axis.GetSafeNormal(),ExpectedAngle):FQuat::Identity;
        const FQuat ExpectedRotation=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
        // The animated local head offset is bounded and filtered, so equality with the
        // reference anchor is not expected. Remove the final-view eye offset to test its cap.
        const FVector EyePivot=Camera->GetComponentLocation()-Camera->GetComponentQuat().RotateVector(Settings->EyeOffset);
        const double PositionOffset=(Body->GetComponentTransform().InverseTransformPosition(EyePivot)-RefHead.GetLocation()).Size();
        const double PosError=FMath::Max(0.0,PositionOffset-PositionLimit);
        const double RotError=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(ExpectedRotation));
        const double ViewMotion=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(Controller->GetControlRotation().Quaternion()));
        MaxCameraPositionError=FMath::Max(MaxCameraPositionError,PosError);MaxCameraRotationError=FMath::Max(MaxCameraRotationError,RotError);MaxViewMotion=FMath::Max(MaxViewMotion,ViewMotion);
        Check(PosError<.05 && RotError<.02,TEXT("camera_not_independent_mouse_separated_bounded_head"));
        Check(FMath::RadiansToDegrees(Angle)<=MaxDegrees+.03,TEXT("appearance_head_residual_exceeds_state_limit"));
        Check(ViewMotion<=MaxDegrees+.02,TEXT("camera_exceeds_state_head_rotation_limit"));
        const bool Third=Settings->IsThirdPersonInspectionActive();
        if(!Third)
        {
            ++FirstSamples;const FMinimalViewInfo& View=Controller->PlayerCameraManager->GetCameraCacheView();
            const FVector ViewPivot=View.Location-View.Rotation.Quaternion().RotateVector(Settings->EyeOffset);
            const double ViewOffset=(Body->GetComponentTransform().InverseTransformPosition(ViewPivot)-RefHead.GetLocation()).Size();
            Check(ViewOffset<=PositionLimit+.05 && FVector::Distance(View.Location,Camera->GetComponentLocation())<.05 &&
                FMath::RadiansToDegrees(View.Rotation.Quaternion().AngularDistance(ExpectedRotation))<.02,TEXT("render_cache_not_stabilized_head"));
        }else{++ThirdSamples;}
        double ShadowPosition=0,ShadowRotation=0;
        if(!Third && Settings->bEnableFullBodyShadow)
        {
            auto* Shadow=Character->FindComponentByClass<URogue10mAppearanceShadowComponent>();
            auto* Proxy=Shadow?Shadow->GetShadowBodyProxy():nullptr;Check(Proxy && Shadow->IsShadowActive(),TEXT("shadow_proxy_missing"));
            if(Proxy)for(const TCHAR* Bone:{TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")})
            {
                const FTransform* Expected=Previous.Find(FString(TEXT("appearance/"))+Bone);
                if(!Expected){Check(false,TEXT("shadow_expected_bone_missing"));continue;}
                const FTransform Actual=Proxy->GetBoneTransformByName(Bone,EBoneSpaces::ComponentSpace);
                ShadowPosition=FMath::Max(ShadowPosition,FVector::Distance(Expected->GetLocation(),Actual.GetLocation()));
                ShadowRotation=FMath::Max(ShadowRotation,FMath::RadiansToDegrees(Expected->GetRotation().AngularDistance(Actual.GetRotation())));
                Check(Actual.GetScale3D().GetMin()>.99,TEXT("shadow_bone_scale_invalid"));
            }
            Check(ShadowPosition<.05 && ShadowRotation<.05,TEXT("shadow_pose_not_raw_appearance"));
        }
        CameraRows+=FString::Printf(TEXT("%d,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),FrameIndex,
            FMath::RadiansToDegrees(Raw.AngularDistance(FQuat::Identity)),ViewMotion,FMath::RadiansToDegrees(ExpectedAngle),PosError,RotError,Third,ShadowPosition,ShadowRotation,
            FMath::RadiansToDegrees(Angle),MaxDegrees,PositionLimit,PositionOffset);
    }
    void Prepare()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if (!Character.IsValid() || !Controller.IsValid() || !Controller->PlayerCameraManager || !Character->GetAppearanceCameraComponent() ||
            !Character->GetBasicBrawlerComponent() || !Character->GetCombatComponent() || !Character->GetAnimationPlaybackMesh() ||
            !Character->GetAnimationPlaybackMesh()->GetAnimInstance() || !Character->GetMesh()->GetAnimInstance())
        {Check(false,TEXT("required_player_components_missing"));Finish();return;}
        OldInput=Character->InputEnabled(); Character->DisableInput(Controller.Get()); bPrepared=true;
        const URogue10mCharacterAnimationComponent* CharacterAnimation=Character->GetCharacterAnimationComponent();
        const URogue10mCharacterMotionDataAsset* MotionData=CharacterAnimation?CharacterAnimation->GetMotionData():nullptr;
        CameraDodgeMontage=MotionData?MotionData->DodgeMontage.Get():nullptr;
        OldControl=Controller->GetControlRotation(); Controller->RotationInput=FRotator::ZeroRotator;
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); if(!LoadSkills()){Finish();return;}
        LockedControl=FRotator(-20.f,Character->GetActorRotation().Yaw,0.f); Controller->SetControlRotation(LockedControl);
        OldDamage=Character->CanBeDamaged();Character->SetCanBeDamaged(false);Character->GetCharacterMovement()->StopMovementImmediately();
        if (Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()) { Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection(); }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}
            It->GetCharacterMovement()->StopMovementImmediately();It->GetCharacterMovement()->DisableMovement();
            if(bDamageTarget)
            {
                if(!DamageMonster.IsValid())
                {
                    DamageMonster=*It;
                    if(auto* Regen=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()){Regen->ConfigureRegeneration(0,0,0);}
                    It->GetRogueAttributeSet()->SetMaxHealth(10000);It->GetRogueAttributeSet()->SetHealth(10000);
                    It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                    It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
                    It->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*135.f,false,nullptr,ETeleportType::TeleportPhysics);
                    It->SetActorRotation((Character->GetActorLocation()-It->GetActorLocation()).Rotation());
                    InitialHealth=PreviousHealth=It->GetRogueAttributeSet()->GetHealth();
                }
                else{It->SetActorEnableCollision(false);}
            }
        }
        if(bDamageTarget){Check(DamageMonster.IsValid(),TEXT("damage_target_missing"));}
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bOwnFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0);FApp::SetUseFixedTimeStep(true);
        RecordSkeleton(Character->GetAnimationPlaybackMesh(),TEXT("source"));RecordSkeleton(Character->GetMesh(),TEXT("appearance"));
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT TIMELINE frames=840 fps=30 unarmed=45/48,135/138,210/246 knuckle=330/332,395/397,455/457,510/512,550/%d switch_cancel=620/625/626 V=20/95/310/495 walk=700:730 jump=755/761 class=%s capture=%d all=%d keyframes=%d first_person_only=%d"),ChargedRelease,*Character->GetClass()->GetPathName(),bCapture,bCaptureAll,bCaptureKeyframes,bFirstPersonOnly);
    }
    void RecordSkeleton(USkeletalMeshComponent* Mesh,const TCHAR* Stage)
    {
        if (!Mesh || !Mesh->GetSkeletalMeshAsset()) {Check(false,TEXT("mesh_asset_missing"));return;}
        const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        for (int32 I=0;I<Ref.GetNum();++I)
        {
            const int32 Parent=Ref.GetParentIndex(I);
            Skeletons+=FString::Printf(TEXT("%s,%s,%s,%s,%s"),Stage,*Mesh->GetSkeletalMeshAsset()->GetPathName(),*Mesh->GetAnimInstance()->GetClass()->GetPathName(),
                *Ref.GetBoneName(I).ToString(),Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString())+TransformColumns(Ref.GetRefBonePose()[I])+TEXT("\n");
        }
        for (const TCHAR* Bone:{TEXT("pelvis"),TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")})
        {Check(Mesh->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("required_sample_bone_missing"));}
    }
    void RecordPose(USkeletalMeshComponent* Mesh,const TCHAR* Stage)
    {
        const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        const auto Local=Mesh->GetBoneSpaceTransformsView(); const auto& Rendered=Mesh->GetComponentSpaceTransforms();
        TArray<FTransform> RawCS;RawCS.SetNum(Local.Num());
        for (int32 I=0;I<Local.Num();++I)
        {
            const int32 Parent=Ref.GetParentIndex(I);
            RawCS[I]=Parent==INDEX_NONE?Local[I]:Local[I]*RawCS[Parent];
            const FString Bone=Ref.GetBoneName(I).ToString();if(!WantedBone(Bone)){continue;}
            const FTransform& L=Local[I];const FTransform& C=RawCS[I];
            Check(!L.ContainsNaN() && !C.ContainsNaN(),TEXT("raw_pose_nonfinite"));
            Check(L.GetRotation().IsNormalized(),TEXT("raw_rotation_not_normalized"));
            const FVector Scale=L.GetScale3D();Check(Scale.GetMin()>.001 && Scale.GetMax()<100.f,TEXT("raw_scale_collapsed_or_extreme"));
            const double Length=L.GetTranslation().Length(),RefLength=Ref.GetRefBonePose()[I].GetTranslation().Length();
            const double Ratio=RefLength>.01?Length/RefLength:1.0;
            static const TSet<FString> FixedLength={TEXT("spine_01"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r")};
            if(FixedLength.Contains(Bone) && RefLength>.01){Check(FMath::Abs(Ratio-1.0)<=.01,TEXT("anatomical_segment_length_changed_over_one_percent"));}
            Check(Scale.Equals(FVector::OneVector,.01),TEXT("raw_bone_scale_not_unit"));
            const FString Key=FString(Stage)+TEXT("/")+Bone;double StepCm=0,StepDeg=0;
            if(const FTransform* Prev=Previous.Find(Key))
            {StepCm=FVector::Distance(Prev->GetLocation(),C.GetLocation());StepDeg=FMath::RadiansToDegrees(Prev->GetRotation().AngularDistance(C.GetRotation()));}
            Previous.Add(Key,C);MaxStepCm=FMath::Max(MaxStepCm,StepCm);MaxStepDeg=FMath::Max(MaxStepDeg,StepDeg);
            // These are review flags, not automatic anatomical failures: axes, authored stretch,
            // strike speed, and visibility scaling require stage-aware interpretation.
            if(StepDeg>45.f){++FastRotationSamples;}if(RefLength>.01 && (Ratio<.5 || Ratio>1.5)){++LengthOutlierSamples;}
            Poses+=FString::Printf(TEXT("%d,%.9f,%s,%s,%s,%d,%.9f,%.9f,%.9f,%.9f,%.9f"),FrameIndex,FrameIndex/30.0,Stage,*Bone,
                Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString(),Mesh->IsBoneHiddenByName(Ref.GetBoneName(I)),Length,RefLength,Ratio,StepCm,StepDeg)+
                TransformColumns(L)+TransformColumns(C)+TransformColumns(Rendered.IsValidIndex(I)?Rendered[I]:FTransform::Identity)+TEXT("\n");
        }
    }
    void Frame()
    {
        if(!Character.IsValid() || !Controller.IsValid()){Check(false,TEXT("player_lost"));Finish();return;}
        auto* Source=Character->GetAnimationPlaybackMesh();auto* Body=Character->GetMesh();auto* Anim=Source->GetAnimInstance();
        Check(Controller->GetControlRotation().Equals(LockedControl,.01f),TEXT("physical_input_changed_control"));Controller->RotationInput=FRotator::ZeroRotator;
        const auto Snapshot=Character->GetBasicBrawlerComponent()->GetSnapshot();
        auto* Montage=Anim->GetCurrentActiveMontage();const float MontageTime=Montage?Anim->Montage_GetPosition(Montage):-1.f;
        FString Clip;float ClipTime=-1.f;
        if(Montage)
        {
            for(const FSlotAnimationTrack& Track:Montage->SlotAnimTracks)
            {for(const FAnimSegment& Segment:Track.AnimTrack.AnimSegments)
                {if(Segment.GetAnimReference() && MontageTime>=Segment.StartPos && MontageTime<=Segment.StartPos+Segment.GetLength())
                    {Clip=Segment.GetAnimReference()->GetPathName();ClipTime=Segment.ConvertTrackPosToAnimPos(MontageTime);break;}}
                if(!Clip.IsEmpty()){break;}}
            SeenAttacks.Add(static_cast<int32>(Snapshot.Attack));
        }
        RecordSkills(Montage);RecordDamage();FString ActiveSkillPath;
        if(Montage)for(const auto& Skill:Skills){if(Skill->AttackMontage==Montage){ActiveSkillPath=Skill->GetPathName();break;}}
        const FMinimalViewInfo& View=Controller->PlayerCameraManager->GetCameraCacheView();const FRotator Control=Controller->GetControlRotation();
        Frames+=FString::Printf(TEXT("%d,%.9f,%.9f,%d,%d,%d,%.9f,%.9f,%d,%s,%.9f,%s,%.9f"),FrameIndex,FrameIndex/30.0,World->GetTimeSeconds(),
            static_cast<int32>(Snapshot.Phase),static_cast<int32>(Snapshot.Attack),Snapshot.AcceptedSerial,Snapshot.AttackElapsed,Snapshot.ChargeAlpha,
            Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),Montage?*Montage->GetPathName():TEXT(""),MontageTime,*Clip,ClipTime)+
            TransformColumns(Source->GetComponentTransform())+TransformColumns(Body->GetComponentTransform())+
            TransformColumns(Character->GetFirstPersonCameraComponent()->GetComponentTransform())+TransformColumns(FTransform(View.Rotation,View.Location))+
            FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d,%s\n"),Control.Pitch,Control.Yaw,Control.Roll,
                Character->GetActorLocation().X,Character->GetActorLocation().Y,Character->GetActorLocation().Z,Character->GetCharacterMovement()->IsFalling(),static_cast<int32>(Character->GetEquippedWeaponType()),*ActiveSkillPath);
        RecordPose(Source,TEXT("source"));RecordPose(Body,TEXT("appearance"));
        if(FrameIndex>0){CheckCameraAndShadow();}
        if(!bFirstPersonOnly && (FrameIndex==20 || FrameIndex==95 || FrameIndex==310 || FrameIndex==495))
        {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Pressed,1.f));}
        if(!bFirstPersonOnly && (FrameIndex==21 || FrameIndex==96 || FrameIndex==311 || FrameIndex==496))
        {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Released,0.f));}
        auto* Combat=Character->GetCombatComponent();
        if(FrameIndex==45 || FrameIndex==48){RefillResources();Combat->HandleAttackPressed(true);Combat->HandleAttackReleased(true);}
        if(FrameIndex==135 || FrameIndex==210){RefillResources();Combat->HandleAttackPressed(false);}
        if(FrameIndex==138 || FrameIndex==246){Combat->HandleAttackReleased(false);}
        if(FrameIndex==300){Check(Snapshot.AcceptedSerial==4,TEXT("unarmed_four_attacks_not_accepted"));PrepareKnuckle();}
        if(FrameIndex==390){Check(Combat->AssignSkillToInputSlot(Skills[5].Get(),ERogue10mAttackInputSlot::Primary),TEXT("primary02_assign_failed"));}
        if(FrameIndex==450){Check(Combat->AssignSkillToInputSlot(Skills[6].Get(),ERogue10mAttackInputSlot::Primary),TEXT("primary03_assign_failed"));}
        if(FrameIndex==330 || FrameIndex==395 || FrameIndex==455 || FrameIndex==550){RefillResources();Combat->HandleAttackPressed(true);}
        if(FrameIndex==332 || FrameIndex==397 || FrameIndex==457 || FrameIndex==ChargedRelease){Combat->HandleAttackReleased(true);}
        if(FrameIndex==510){RefillResources();Combat->HandleAttackPressed(false);}if(FrameIndex==512){Combat->HandleAttackReleased(false);}
        if(FrameIndex==620){CancelUses=LastUses;Combat->HandleAttackPressed(true);}
        if(FrameIndex==625){Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);}
        if(FrameIndex==626){Combat->HandleAttackReleased(true);}
        if(FrameIndex==655)
        {
            for(int32 I=0;I<LastUses.Num();++I){Check(LastUses[I]==CancelUses[I],TEXT("weapon_switch_released_stale_attack"));}
            Check(Snapshot.Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("weapon_switch_not_idle"));
            Check(!Montage,TEXT("weapon_switch_stale_montage"));
        }
        if(FrameIndex==700){WalkStart=Character->GetActorLocation();}
        if(FrameIndex>=700 && FrameIndex<730){Character->AddMovementInput(-Character->GetActorForwardVector(),.35f,true);}
        if(FrameIndex>=700 && FrameIndex<=730){MaxWalkDistance=FMath::Max(MaxWalkDistance,static_cast<double>(FVector::Dist2D(WalkStart,Character->GetActorLocation())));}
        if(FrameIndex==730){Character->GetCharacterMovement()->StopMovementImmediately();}
        if(FrameIndex==755){JumpStartZ=Character->GetActorLocation().Z;Character->Jump();}
        if(FrameIndex==761){Character->StopJumping();}
        if(FrameIndex>=755){MaxJumpHeight=FMath::Max(MaxJumpHeight,static_cast<double>(Character->GetActorLocation().Z-JumpStartZ));}
        static const TSet<int32> CaptureKeyframes={0,46,57,68,145,156,262,345,407,464,474,485,522,590,712,762,790};
        if(bCapture && FrameIndex<FrameLimit && (bCaptureAll || (bCaptureKeyframes ? CaptureKeyframes.Contains(FrameIndex) : FrameIndex%3==0)))
        {FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false);}
        if(FrameIndex==FrameLimit)
        {
            for(int32 I=0;I<Skills.Num();++I)
            {
                Check(LastUses[I]-InitialUses[I]==1,TEXT("skill_not_accepted_exactly_once"));
                Check(MontageSamples[I]>=3,TEXT("skill_montage_not_sampled"));
                if(bDamageTarget)
                {
                    const int32 Expected=I==6?6:1;
                    Check(DamageCounts[I]==Expected,TEXT("actual_damage_pulse_count_mismatch"));
                    UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT DAMAGE_RESULT skill=%s actual=%d expected=%d first_error=%.6f"),*Skills[I]->GetName(),DamageCounts[I],Expected,FirstDamageErrors[I]);
                }
                UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT SKILL_RESULT skill=%s uses=%d montage_samples=%d"),*Skills[I]->GetName(),LastUses[I]-InitialUses[I],MontageSamples[I]);
            }
            Check(MaxWalkDistance>5.f,TEXT("locomotion_walk_not_observed"));Check(MaxJumpHeight>10.f,TEXT("locomotion_jump_not_observed"));
            Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("locomotion_did_not_land"));
            Check(Snapshot.Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("attack_did_not_return_idle"));
            Check(bFirstPersonOnly ? (FirstSamples>800 && ThirdSamples==0) : (FirstSamples>100 && ThirdSamples>100),bFirstPersonOnly ? TEXT("first_person_only_sampling_failed") : TEXT("both_view_modes_not_sampled"));
            Finish();return;
        }
        ++FrameIndex;
    }
    void Finish(bool bExit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        Check(FFileHelper::SaveStringToFile(Poses,*FPaths::Combine(Output,TEXT("poses.csv"))),TEXT("pose_csv_write_failed"));
        Check(FFileHelper::SaveStringToFile(Frames,*FPaths::Combine(Output,TEXT("frames.csv"))),TEXT("frame_csv_write_failed"));
        Check(FFileHelper::SaveStringToFile(CameraRows,*FPaths::Combine(Output,TEXT("camera.csv"))),TEXT("camera_csv_write_failed"));
        Check(FFileHelper::SaveStringToFile(SkillRows,*FPaths::Combine(Output,TEXT("skills.csv"))),TEXT("skills_csv_write_failed"));
        Check(FFileHelper::SaveStringToFile(Skeletons,*FPaths::Combine(Output,TEXT("skeletons.csv"))),TEXT("skeleton_csv_write_failed"));
        if(bDamageTarget){Check(FFileHelper::SaveStringToFile(DamageRows,*FPaths::Combine(Output,TEXT("damage.csv"))),TEXT("damage_csv_write_failed"));}
        const FString Metrics=FString::Printf(TEXT("frames=%d\nfailures=%d\nmax_step_cm=%.9f\nmax_step_deg=%.9f\nfast_rotation_samples=%d\nlength_outlier_samples=%d\nlocomotion=%d\nwalk_distance_cm=%.9f\njump_height_cm=%.9f\nNOTE: validates nine real skill inputs, structural bone lengths and independently composed mouse-separated state-bounded head camera; camera position_error_cm is bound excess, not distance from an exactly fixed anchor; skills.csv is an expected pulse schedule; optional RogueFistDamageTarget independently records target health in damage.csv.\n"),FrameIndex,Failures,MaxStepCm,MaxStepDeg,FastRotationSamples,LengthOutlierSamples,bLocomotion,MaxWalkDistance,MaxJumpHeight);
        Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        if(bPrepared && Character.IsValid())
        {
            Character->SetCanBeDamaged(OldDamage);
            if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
            if(Controller.IsValid()){if(OldInput){Character->EnableInput(Controller.Get());}Controller->SetControlRotation(OldControl);}
        }
        if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT CAMERA first_samples=%d third_samples=%d max_position_error_cm=%.9f max_rotation_error_deg=%.9f max_motion_deg=%.9f"),FirstSamples,ThirdSamples,MaxCameraPositionError,MaxCameraRotationError,MaxViewMotion);
        const bool bPassed=Failures==0 && FrameIndex==FrameLimit;
        UE_LOG(LogRogue10m,Display,TEXT("FIST_COMBAT METRICS max_step_cm=%.6f max_step_deg=%.6f fast_rotation_samples=%d length_outlier_samples=%d walk_cm=%.6f jump_cm=%.6f"),MaxStepCm,MaxStepDeg,FastRotationSamples,LengthOutlierSamples,MaxWalkDistance,MaxJumpHeight);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIST_COMBAT_%s frames=%d failures=%d output=%s"),bPassed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Failures,*Output);
        if(bExit){FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mCharacter> OriginalPawn;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Poses,Frames,Skeletons,CameraRows,SkillRows,DamageRows;FRotator OldControl,LockedControl;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;TArray<int32> InitialUses,LastUses,MontageSamples,CancelUses;
    TWeakObjectPtr<ARogue10mBasicMonster> DamageMonster;
    TWeakObjectPtr<UAnimMontage> CameraDodgeMontage;
    TArray<int32> DamageCounts;TArray<double> AcceptanceTimes,ExpectedDelays,FirstDamageErrors;
    int32 DamageSkillIndex=INDEX_NONE;double InitialHealth=0,PreviousHealth=0;
    int32 ChargedRelease=575,FirstSamples=0,ThirdSamples=0;double MaxCameraPositionError=0,MaxCameraRotationError=0,MaxViewMotion=0;
    TMap<FString,FTransform> Previous;TSet<int32> SeenAttacks;
    int32 FrameIndex=0,FrameLimit=330,Failures=0,FastRotationSamples=0,LengthOutlierSamples=0;
    FVector WalkStart; double JumpStartZ=0,MaxWalkDistance=0,MaxJumpHeight=0;
    double OldDelta=1.0/30.0,MaxStepCm=0,MaxStepDeg=0;
    bool bFinished=false,bCapture=false,bCaptureAll=false,bCaptureKeyframes=false,bFirstPersonOnly=false,bDamageTarget=false,bLocomotion=false,bPrepared=false,OldInput=false,OldDamage=true,bOwnFixed=false,OldFixed=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine || GIsEditor || (Active.IsValid() && !Active->IsFinished())){return;}
    for(const FWorldContext& Context:GEngine->GetWorldContexts())
    {if(UWorld* W=Context.World();W && W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestFistCombat"),
    TEXT("Real input Unarmed4/Knuckle5, bounded head camera, bones/shadow, V, switch cancellation and locomotion. -RogueFistLabel=name -RogueFistCaptureAll -RogueFistCaptureKeyframes -RogueFistDamageTarget -RogueFistFirstPersonOnly -RogueFistCharacterClass=/Game/..._C"),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
