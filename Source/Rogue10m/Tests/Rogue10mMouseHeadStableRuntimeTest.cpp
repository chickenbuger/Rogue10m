// Editor-only integration proof for mouse-driven heads, bounded camera motion and speed-driven gait.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
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
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace Rogue10mMouseHeadStableRuntimeTest
{
struct FCase
{
    const TCHAR* Name;
    float Pitch=0,Yaw=0,Speed=0,AttackSpeed=1;
    int32 Attack=0; // 1 LMB jab, 2 RMB straight, 3 hold/release hook, 4 actual dodge, 5 actual jump.
    bool bSprint=false,bKnuckle=false,bInspection=false,bYawWrap=false;
};
static const FCase Cases[]={
    {TEXT("idle_mouse_forward"),0,0},
    {TEXT("idle_mouse_up70_yaw90"),70,90},
    {TEXT("idle_mouse_down70_yaw180"),-70,180},
    {TEXT("walk300_mouse_down30"),-30,0,300},
    {TEXT("walk600_mouse_up30"),30,90,600},
    {TEXT("sprint900_mouse_forward"),0,0,900,1,0,true},
    {TEXT("sprint1200_mouse_down30"),-30,90,1200,1,0,true},
    {TEXT("jab_rate_half"),0,0,0,.5f,1},
    {TEXT("jab_rate_one"),0,0,0,1,1},
    {TEXT("jab_rate_two"),0,0,0,2,1},
    {TEXT("jab_rate_three"),0,0,0,3,1},
    {TEXT("sprint900_jab_cap3"),30,0,900,1,1,true},
    {TEXT("walk600_straight_down30"),-30,90,600,1,2},
    {TEXT("right_hook_up30"),30,180,0,1,3},
    {TEXT("knuckle_primary_down30"),-30,0,0,1,1,false,true},
    {TEXT("knuckle_special_forward"),0,90,0,1,2,false,true},
    {TEXT("inspection_head_up70"),70,0,0,1,0,false,false,true},
    {TEXT("inspection_walk600_down30"),-30,90,600,1,0,false,false,true},
    {TEXT("mouse_yaw_wrap179"),15,179,0,1,0,false,false,true,true},
    {TEXT("jump_capsule_world_travel"),0,0,300,1,5},
    {TEXT("safe_forward_dodge_head"),30,0,0,1,4,false,false,true},
    {TEXT("default_idle_down20"),-20,0},
    {TEXT("default_jab_down20"),-20,0,0,1,1},
    {TEXT("inspection_jab_down20"),-20,0,0,1,1,false,false,true}
};
static const TCHAR* LowerBones[]={TEXT("root"),TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("ball_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("ball_r"),TEXT("ik_foot_root"),TEXT("ik_foot_l"),TEXT("ik_foot_r")};
static double Angle(const FQuat& A,const FQuat& B){return FMath::RadiansToDegrees(A.AngularDistance(B));}
static TArray<FTransform> ComponentPose(USkeletalMeshComponent* Mesh,bool Reference=false)
{
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto Local=Mesh->GetBoneSpaceTransformsView();TArray<FTransform> Result;Result.SetNum(Reference?Ref.GetRefBonePose().Num():Local.Num());
    for(int32 I=0;I<Result.Num();++I){const int32 P=Ref.GetParentIndex(I);const FTransform T=Reference?Ref.GetRefBonePose()[I]:Local[I];Result[I]=P==INDEX_NONE?T:T*Result[P];}return Result;
}
static FQuat HeadDelta(USkeletalMeshComponent* Mesh,const TArray<FTransform>& Pose,const TArray<FTransform>& Ref)
{
    const int32 H=Mesh->GetBoneIndex(TEXT("head"));if(!Pose.IsValidIndex(H)||!Ref.IsValidIndex(H)){return FQuat::Identity;}
    const FQuat Q=Mesh->GetComponentQuat();return (Q*Pose[H].GetRotation()*Ref[H].GetRotation().Inverse()*Q.Inverse()).GetNormalized();
}
class FRun:public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;FString Slot,Prefix;
        const auto Valid=[](const FString& V,const TCHAR* P,int32 Max){bool Good=V.StartsWith(P)&&V.Len()<=Max;for(TCHAR C:V){Good&=FChar::IsAlnum(C)||C==TEXT('_');}return Good;};
        if(!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot)||!Valid(Slot,TEXT("Rogue10m_Fighter_QA_"),100)
            ||!FParse::Value(FCommandLine::Get(),TEXT("RogueProgressionSavePrefix="),Prefix)||!Valid(Prefix,TEXT("Rogue10m_Progression_QA_"),80))
        {Check(false,TEXT("strict_isolated_save_paths_required"));Finish();return;}
        FString Label=TEXT("runtime");FParse::Value(FCommandLine::Get(),TEXT("RogueMouseHeadLabel="),Label);Label=FPaths::MakeValidFileName(Label);
        Output=FPaths::ProjectDir()/TEXT("tmp/mouse-head-stable")/Label;Images=FPaths::ProjectSavedDir()/TEXT("Screenshots/WindowsEditor/MouseHeadStable")/Label;
        bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueMouseHeadCapture"));bMovie=FParse::Param(FCommandLine::Get(),TEXT("RogueMouseHeadMovie"));
        bNegative=FParse::Param(FCommandLine::Get(),TEXT("RogueMouseHeadDisabledControl"));CaseFrames=75;
        if(IFileManager::Get().FileExists(*(Output/TEXT("metrics.txt")))){Check(false,TEXT("fresh_evidence_label_required"));Finish();return;}
        IFileManager::Get().MakeDirectory(*Output,true);if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Frames=TEXT("frame,case,case_frame,world_s,control_pitch,control_yaw,speed_cm_s,sprinting,falling,attack_montage,attack_rate,montage_s,ground_play_rate,rotation_cap_deg,position_cap_cm,source_head_mouse_residual_deg,appearance_head_mouse_residual_deg,camera_mouse_residual_deg,camera_local_offset_cm,late_mouse_oracle_error_deg,repeat_camera_error_cm,repeat_camera_error_deg,source_lower_pose_error_deg,source_local_translation_error_cm,source_local_scale_error,source_wrist_finger_error_deg,visible_limb_length_change_percent,applied_head,phase_error_s\n");
        Results=TEXT("case,frames,paired_samples,attack_samples,accepted_uses,requested_attack_speed,observed_montage_rate,first_damage_error_s,damage_pulses,speed_peak_cm_s,ground_rate_peak,foot_travel_cm,air_samples,actor_jump_height_cm,max_source_head_deg,max_appearance_head_deg,max_camera_deg,max_local_offset_cm\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        World->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const{return bFinished;}
private:
    void Check(bool Good,const TCHAR* Reason)
    {++Assertions;if(!Good){++Failures;++Reasons.FindOrAdd(FString(Reason));if(Failures<60){UE_LOG(LogRogue10m,Error,TEXT("MOUSE_HEAD_STABLE FAIL frame=%d case=%d %s"),Frame,CaseIndex,Reason);}}}
    void Input(const TCHAR* Function){Character->ProcessEvent(Character->FindFunctionChecked(FName(Function)),nullptr);}
    void Move(float Forward)
    {struct FParameters{float Right=0,Forward=0;}P;P.Forward=Forward;Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&P);Character->GetCharacterAnimationComponent()->SetMovementState(Forward!=0,Character->IsSprinting());}
    void Refill(){auto* A=Character->GetRogueAttributeSet();A->RestoreVitals();A->SetIdentity(A->GetMaxIdentity());}
    void VerifySafeMotionPolicy()
    {
        auto* Combat=Character->GetCombatComponent();
        for(ERogue10mWeaponType Weapon:{ERogue10mWeaponType::Unarmed,ERogue10mWeaponType::Knuckle})
        {
            Character->SetEquippedWeaponType(Weapon);
            for(auto* Skill:Combat->GetActiveSkillTreeSkills())
            {if(Skill&&Skill->SkillTreeNodeType==ERogue10mSkillTreeNodeType::ActiveTechnique)
             {++AuditedSkills;Check(Skill->AttackMontage&&URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Skill->AttackMontage),TEXT("active_tree_attack_has_missing_or_forbidden_presentation"));}}
        }
        for(auto& Skill:Skills){Check(URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Skill->AttackMontage),TEXT("core_input_attack_presentation_not_safe"));}
        const auto* Motion=Character->GetCharacterAnimationComponent()->GetMotionData();Check(Motion&&Motion->DodgeMontage&&URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Motion->DodgeMontage),TEXT("configured_dodge_has_forbidden_or_missing_presentation"));
        auto* Unsafe=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03"));
        auto* Original=Skills[3].Get();auto* Bad=DuplicateObject<UAnimMontage>(Original->AttackMontage,GetTransientPackage(),TEXT("MouseHead_QA_ForbiddenSegment"));
        if(!Unsafe||!Bad||Bad->SlotAnimTracks.IsEmpty()||Bad->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty())
        {Check(false,TEXT("forbidden_segment_fixture_missing"));return;}
        Bad->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(Unsafe);
        Check(!URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Bad),TEXT("nested_forbidden_sequence_was_not_rejected"));
        Combat->UnlockAttackSkill(Original);Check(Combat->IsSkillInActiveTree(Original)&&Combat->IsAttackSkillUnlocked(Original),TEXT("negative_gate_skill_not_in_real_active_tree"));
        auto* Attributes=Character->GetRogueAttributeSet();Refill();const float Stamina=Attributes->GetStamina(),Mana=Attributes->GetMana(),Identity=Attributes->GetIdentity(),Cooldown=Combat->GetAttackCooldownRemaining();const int32 Uses=Progress->GetSkillUseCount(Original->GetFName());
        Target->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*100,false,nullptr,ETeleportType::TeleportPhysics);
        {TGuardValue<TObjectPtr<UAnimMontage>> Swap(Original->AttackMontage,Bad);Check(!Combat->ActivateTreeSkill(Original),TEXT("forbidden_actual_attack_was_accepted"));}
        Check(FMath::IsNearlyEqual(Attributes->GetStamina(),Stamina)&&FMath::IsNearlyEqual(Attributes->GetMana(),Mana)&&FMath::IsNearlyEqual(Attributes->GetIdentity(),Identity),TEXT("rejected_forbidden_attack_consumed_resources"));
        Check(FMath::IsNearlyEqual(Combat->GetAttackCooldownRemaining(),Cooldown),TEXT("rejected_forbidden_attack_started_cooldown"));Check(Progress->GetSkillUseCount(Original->GetFName())==Uses,TEXT("rejected_forbidden_attack_recorded_skill_use"));
        Check(!Character->PlayCommonMontage(Bad,1),TEXT("direct_common_montage_bypassed_forbidden_gate"));bPolicyChecked=true;
    }
    USkeletalMeshComponent* Clone(USkeletalMeshComponent* Original,USceneComponent* Parent,const TCHAR* Name)
    {
        auto* M=NewObject<USkeletalMeshComponent>(Character.Get(),FName(Name),RF_Transient);Character->AddInstanceComponent(M);M->SetupAttachment(Parent);
        M->SetRelativeTransform(Original->GetRelativeTransform());M->SetSkeletalMeshAsset(Original->GetSkeletalMeshAsset());M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        M->SetGenerateOverlapEvents(false);M->SetCanEverAffectNavigation(false);M->SetVisibility(false,false);M->SetHiddenInGame(true,false);
        M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;M->bSuppressNotifyEventDispatch=true;M->bEnableUpdateRateOptimizations=false;
        M->SetForcedLOD(1);M->PrimaryComponentTick.TickGroup=Original->PrimaryComponentTick.TickGroup;M->RegisterComponent();M->SetAnimInstanceClass(Original->GetAnimInstance()->GetClass());return M;
    }
    void Prepare()
    {
        Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid()||!Character->GetRogueAttributeSet()||!Character->GetAppearanceCameraComponent()||!Character->GetCombatComponent()||!Character->GetCharacterAnimationComponent())
        {Check(false,TEXT("production_pawn_components_missing"));Finish();return;}
        Source=Character->GetAnimationPlaybackMesh();Body=Character->GetMesh();Progress=Character->GetPlayerState<ARogue10mPlayerState>();
        if(!Source.IsValid()||!Body.IsValid()||Source==Body||!Source->GetAnimInstance()||!Body->GetAnimInstance()||!Progress.IsValid())
        {Check(false,TEXT("actual_source_appearance_retarget_missing"));Finish();return;}
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());if(!Stance.IsValid()){Check(false,TEXT("native_common_parent_missing"));Finish();return;}
        OldActor=Character->GetActorTransform();OldControl=Controller->GetControlRotation();OldWeapon=Character->GetEquippedWeaponType();OldThird=Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive();
        OldInput=Character->InputEnabled();OldDamage=Character->CanBeDamaged();OldYaw=Character->bUseControllerRotationYaw;OldSourceLOD=Source->GetForcedLOD();OldBodyLOD=Body->GetForcedLOD();
        OldSourceRate=Source->bEnableUpdateRateOptimizations;OldBodyRate=Body->bEnableUpdateRateOptimizations;OldAim=Stance->bEnableViewAlignedUpperBody;OldHead=Stance->bEnableMouseDrivenHead;
        OldMoveSpeed=Character->GetRogueAttributeSet()->GetMoveSpeed();OldAttackSpeed=Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier();OldMaxWalkSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;
        bPrepared=true;Character->DisableInput(Controller.Get());Character->SetCanBeDamaged(false);Controller->CloseAllBlockingPanels();Character->bUseControllerRotationYaw=false;
        Source->SetForcedLOD(1);Body->SetForcedLOD(1);Source->bEnableUpdateRateOptimizations=false;Body->bEnableUpdateRateOptimizations=false;
        auto* Movement=Character->GetCharacterMovement();const FTickPrerequisite Required(Movement,Movement->PrimaryComponentTick);
        // The production source must evaluate after real movement state changes.
        // Assert that dependency instead of silently repairing it inside the fixture.
        Check(Source->PrimaryComponentTick.GetPrerequisites().Contains(Required),TEXT("production_animation_source_does_not_follow_movement"));
        Baseline=Clone(Source.Get(),Source->GetAttachParent(),TEXT("MouseHeadSourceBaseline"));Baseline->AddTickPrerequisiteComponent(Source.Get());Baseline->AddTickPrerequisiteComponent(Movement);
        BodyBaseline=Clone(Body.Get(),Baseline.Get(),TEXT("MouseHeadAppearanceBaseline"));BodyBaseline->AddTickPrerequisiteComponent(Baseline.Get());SourceReference=ComponentPose(Source.Get(),true);BodyReference=ComponentPose(Body.Get(),true);
        FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);
        if(!Ground.IsValid()){Check(false,TEXT("physics_floor_missing"));Finish();return;}
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("MouseHeadFloor"),RF_Transient);Ground->AddInstanceComponent(Box);Ground->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(20000,20000,20));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
        Ground->SetActorLocation(OldActor.GetLocation()+FVector(0,0,980));FixtureLocation=OldActor.GetLocation()+FVector(0,0,1000+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It){It->ClearAITarget();if(auto* C=It->GetController()){C->UnPossess();}It->SetActorEnableCollision(false);It->GetCharacterMovement()->DisableMovement();}
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;Target=World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),FixtureLocation+FVector(100,0,0),FRotator::ZeroRotator,P);
        if(!Target.IsValid()||!Target->GetRogueAttributeSet()){Check(false,TEXT("actual_damage_target_missing"));Finish();return;}
        Target->ClearAITarget();if(auto* C=Target->GetController()){C->UnPossess();}Target->GetCharacterMovement()->DisableMovement();Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Target->GetRogueAttributeSet()->SetMaxHealth(100000);Target->GetRogueAttributeSet()->SetHealth(100000);
        if(auto* Regen=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()){Regen->ConfigureRegeneration(0,0,0);}
        const TCHAR* Paths[]={TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special")};
        for(const TCHAR* Path:Paths){auto* S=LoadObject<URogue10mAttackSkillData>(nullptr,Path);if(!S||!S->AttackMontage){Check(false,TEXT("required_active_attack_missing"));Finish();return;}Skills.Add(S);}
        VerifySafeMotionPolicy();
        if(bCapture&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))){FAssetCompilingManager::Get().FinishAllCompilation();}
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bFixed=true;FApp::SetFixedDeltaTime(1./30.);FApp::SetUseFixedTimeStep(true);
        Source->InitAnim(true);Baseline->InitAnim(true);Body->InitAnim(true);BodyBaseline->InitAnim(true);Warmup=15;
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
        UE_LOG(LogRogue10m,Display,TEXT("MOUSE_HEAD_STABLE TIMELINE cases=%d frames_per_case=%d movie=%d negative=%d"),UE_ARRAY_COUNT(Cases),CaseFrames,bMovie,bNegative);
    }
    void BeginCase()
    {
        const auto& C=Cases[CaseIndex];auto* Combat=Character->GetCombatComponent();Combat->CancelBoundSkillInput();Combat->CancelCombatVisuals();Move(0);Input(TEXT("DoSprintEnd"));Character->StopJumping();
        auto* Movement=Character->GetCharacterMovement();Movement->StopMovementImmediately();Character->SetActorLocation(FixtureLocation,false,nullptr,ETeleportType::TeleportPhysics);Character->SetActorRotation(FRotator(0,C.Yaw,0));Controller->SetControlRotation(FRotator(C.Pitch,C.Yaw,0));
        Character->SetEquippedWeaponType(C.bKnuckle?ERogue10mWeaponType::Knuckle:ERogue10mWeaponType::Unarmed);Character->GetCharacterAnimationComponent()->ResetFighterCombatStance();
        auto* A=Character->GetRogueAttributeSet();A->SetMoveSpeed(C.Speed>0?C.Speed:300.f);A->SetAttackSpeedMultiplier(C.AttackSpeed);Refill();if(C.bSprint){Input(TEXT("DoSprintStart"));}Movement->MaxWalkSpeed=C.Speed>0?C.Speed:300.f;
        if(C.bKnuckle){for(int32 I=3;I<5;++I){Combat->UnlockAttackSkill(Skills[I].Get());}Check(Combat->AssignSkillToInputSlot(Skills[3].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary_assign_failed"));Check(Combat->AssignSkillToInputSlot(Skills[4].Get(),ERogue10mAttackInputSlot::Special),TEXT("knuckle_special_assign_failed"));}
        auto* Camera=Character->GetAppearanceCameraComponent();if(Camera->IsThirdPersonInspectionActive()){Camera->ToggleThirdPersonInspection();}
        Source->InitAnim(true);Baseline->InitAnim(true);Body->InitAnim(true);BodyBaseline->InitAnim(true);Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        auto* Disabled=Cast<URogue10mFighterCombatStanceAnimInstance>(Baseline->GetAnimInstance());if(!Stance.IsValid()||!Disabled){Check(false,TEXT("paired_native_parent_missing"));Finish();return;}
        Stance->bEnableViewAlignedUpperBody=true;Stance->bEnableMouseDrivenHead=!bNegative;Disabled->bEnableViewAlignedUpperBody=false;Disabled->bEnableMouseDrivenHead=false;
        CaseFrame=0;PairSamples=AttackSamples=AirSamples=DamagePulses=0;LastMirrorFrame=-100;LastMirrored.Reset();AcceptedSkill.Reset();AcceptedMontage.Reset();AcceptedTime=-1;ExpectedRate=ObservedRate=1;FirstDamageError=0;CaseHead=CaseVisibleHead=CaseCamera=CaseOffset=PeakSpeed=PeakGait=FootTravel=JumpHeight=0;bHaveFoot=bHaveMontagePosition=false;
        PreviousHealth=100000;Target->GetRogueAttributeSet()->SetHealth(100000);BeforeUses.Reset();for(auto& S:Skills){BeforeUses.Add(Progress->GetSkillUseCount(S->GetFName()));}
        Target->SetActorLocation(FixtureLocation+Character->GetActorForwardVector()*100,false,nullptr,ETeleportType::TeleportPhysics);
        if(C.Speed>0){Move(1);}UE_LOG(LogRogue10m,Display,TEXT("MOUSE_HEAD_STABLE CASE index=%d name=%s speed=%.1f attack_speed=%.2f"),CaseIndex,C.Name,C.Speed,C.AttackSpeed);
    }
    bool MirrorMontage()
    {
        auto* Actual=Source->GetAnimInstance();auto* Ref=Baseline->GetAnimInstance();auto* M=Actual->GetCurrentActiveMontage();if(M!=LastMirrored.Get())
        {if(M){Ref->Montage_Play(M,Actual->Montage_GetPlayRate(M));Ref->Montage_SetPosition(M,Actual->Montage_GetPosition(M));}LastMirrored=M;LastMirrorFrame=Frame;return false;}return Frame-LastMirrorFrame>7;
    }
    int32 ExpectedSkillIndex() const{const auto& C=Cases[CaseIndex];return C.bKnuckle?(C.Attack==1?3:4):(C.Attack==1?0:(C.Attack==2?1:2));}
    void Attack(bool Press,bool Primary)
    {
        auto* Combat=Character->GetCombatComponent();if(Press){Refill();Combat->HandleAttackPressed(Primary);}else{Combat->HandleAttackReleased(Primary);}MirrorMontage();
        if(const auto* S=Combat->GetLastAcceptedAttackSkill();S&&Progress->GetSkillUseCount(S->GetFName())>BeforeUses[ExpectedSkillIndex()]&&AcceptedTime<0)
        {AcceptedTime=World->GetTimeSeconds();AcceptedSkill=S;AcceptedMontage=Source->GetAnimInstance()->GetCurrentActiveMontage();ExpectedRate=FMath::Max(.1f,S->AnimationPlayRate*Cases[CaseIndex].AttackSpeed);Check(AcceptedMontage.IsValid(),TEXT("accepted_attack_has_no_actual_montage"));}
    }
    double Cap() const
    {
        const auto* Movement=Character->GetCharacterMovement();bool AttackActive=false;auto* Anim=Source->GetAnimInstance();
        for(const FAnimMontageInstance* I:Anim->MontageInstances){if(I&&I->Montage&&I->GetWeight()>UE_SMALL_NUMBER){AttackActive=true;}}
        const auto* Combat=Character->GetCombatComponent();
        const bool Charging=Combat&&Combat->IsAttackChargeHeld();
        const bool Dodging=Stance.IsValid()&&Stance->IsViewAimDodgeActive();
        if(AttackActive||Charging||Dodging||Movement->IsFalling()){return 3.;}return Character->IsSprinting()&&Movement->Velocity.Size2D()>10?4.:2.;
    }
    void Observe()
    {
        const auto& C=Cases[CaseIndex];auto* Camera=Character->GetAppearanceCameraComponent();auto* Movement=Character->GetCharacterMovement();auto* Anim=Source->GetAnimInstance();
        const bool Synced=MirrorMontage();const auto A=Source->GetBoneSpaceTransformsView(),B=Baseline->GetBoneSpaceTransformsView();const auto VA=Body->GetBoneSpaceTransformsView(),VB=BodyBaseline->GetBoneSpaceTransformsView();
        const auto SA=ComponentPose(Source.Get()),SB=ComponentPose(Baseline.Get()),BA=ComponentPose(Body.Get()),BB=ComponentPose(BodyBaseline.Get());
        double Lower=0,Translation=0,Scale=0,Fingers=0,VisibleLength=0,Phase=0;auto* M=Anim->GetCurrentActiveMontage();if(M){Phase=FMath::Abs(Anim->Montage_GetPosition(M)-Baseline->GetAnimInstance()->Montage_GetPosition(M));}
        const auto& Ref=Source->GetSkeletalMeshAsset()->GetRefSkeleton();
        for(int32 I=0;I<A.Num();++I)
        {
            Check(!A[I].ContainsNaN()&&A[I].GetRotation().IsNormalized(),TEXT("source_bone_nonfinite_or_unnormalized"));if(!B.IsValidIndex(I)){Check(false,TEXT("source_baseline_bone_count_changed"));continue;}
            const FString N=Ref.GetBoneName(I).ToString();if(!N.StartsWith(TEXT("ik_hand"))){Translation=FMath::Max(Translation,FVector::Distance(A[I].GetTranslation(),B[I].GetTranslation()));Scale=FMath::Max(Scale,FVector::Distance(A[I].GetScale3D(),B[I].GetScale3D()));}
            if(N.StartsWith(TEXT("index_"))||N.StartsWith(TEXT("middle_"))||N.StartsWith(TEXT("ring_"))||N.StartsWith(TEXT("pinky_"))||N.StartsWith(TEXT("thumb_"))||N==TEXT("hand_l")||N==TEXT("hand_r")){Fingers=FMath::Max(Fingers,Angle(A[I].GetRotation(),B[I].GetRotation()));}
        }
        for(const TCHAR* N:LowerBones){const int32 I=Source->GetBoneIndex(N);if(A.IsValidIndex(I)&&B.IsValidIndex(I)){Lower=FMath::Max(Lower,Angle(A[I].GetRotation(),B[I].GetRotation()));}}
        for(const FTransform& T:VA){Check(!T.ContainsNaN()&&T.GetRotation().IsNormalized(),TEXT("appearance_bone_nonfinite_or_unnormalized"));}
        for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("upperarm_l"),TEXT("lowerarm_l")},{TEXT("lowerarm_l"),TEXT("hand_l")},{TEXT("upperarm_r"),TEXT("lowerarm_r")},{TEXT("lowerarm_r"),TEXT("hand_r")},{TEXT("thigh_l"),TEXT("calf_l")},{TEXT("calf_l"),TEXT("foot_l")},{TEXT("thigh_r"),TEXT("calf_r")},{TEXT("calf_r"),TEXT("foot_r")}})
        {const int32 I=Body->GetBoneIndex(Pair.Key),J=Body->GetBoneIndex(Pair.Value);if(BA.IsValidIndex(I)&&BA.IsValidIndex(J)&&BB.IsValidIndex(I)&&BB.IsValidIndex(J)){const double Base=FVector::Distance(BB[I].GetLocation(),BB[J].GetLocation());VisibleLength=FMath::Max(VisibleLength,FMath::Abs(FVector::Distance(BA[I].GetLocation(),BA[J].GetLocation())-Base)/FMath::Max(.01,Base));}}
        const FQuat Control=Controller->GetControlRotation().Quaternion(),Aim=(Control*Character->GetActorQuat().Inverse()).GetNormalized();
        const FQuat SourceDelta=HeadDelta(Source.Get(),SA,SourceReference),BodyDelta=HeadDelta(Body.Get(),BA,BodyReference);
        const double SourceHead=Angle(SourceDelta,Aim),VisibleHead=Angle(BodyDelta,Aim),Limit=Cap(),PositionLimit=Limit==4?2.:(Limit==3?1.5:1.);
        FTransform View;Check(Camera->ReadCameraTransform(View),TEXT("actual_head_camera_unavailable"));const double CameraResidual=Angle(View.GetRotation(),Control);
        const int32 H=Body->GetBoneIndex(TEXT("head"));const FVector Anchor=Body->GetComponentTransform().TransformPosition(BodyReference[H].GetLocation());
        const FVector Eye=View.GetLocation()-View.GetRotation().RotateVector(Camera->EyeOffset);const double PositionResidual=Body->GetComponentTransform().InverseTransformVector(Eye-Anchor).Size();
        Check(SourceHead<=Limit+.035,TEXT("source_head_exceeds_mouse_residual_quaternion_cap"));Check(VisibleHead<=Limit+.04,TEXT("appearance_head_exceeds_mouse_residual_quaternion_cap"));Check(CameraResidual<=Limit+.035,TEXT("camera_exceeds_mouse_residual_quaternion_cap"));Check(PositionResidual<=PositionLimit+.02,TEXT("camera_body_local_position_offset_exceeds_cap"));
        Check(Stance->bMouseDrivenHeadApplied||bNegative,TEXT("evaluated_head_mouse_snapshot_missing"));
        // Independent late-input oracle reads actual evaluated head relative to its reference.
        // It removes exactly the published aim snapshot and applies the NEW mouse once.
        const FQuat Residual=(BodyDelta*Aim.Inverse()).GetNormalized();
        // A query can observe the current state after the evaluated montage stopped.
        // The camera must apply that state's final quaternion cap to its residual.
        // Derive it here from measured bones instead of calling the production helper.
        const double ResidualAngle=Residual.AngularDistance(FQuat::Identity);
        const double ResidualFraction=ResidualAngle>UE_SMALL_NUMBER?FMath::Min(1.,FMath::DegreesToRadians(Limit)/ResidualAngle):0.;
        const FQuat BoundedLateResidual=FQuat::Slerp(FQuat::Identity,Residual,ResidualFraction).GetNormalized();
        const FRotator OriginalControl=Controller->GetControlRotation(),Late(-33,OriginalControl.Yaw+37,0);Controller->SetControlRotation(Late);FTransform LateView,Repeat;
        Check(Camera->ReadCameraTransform(LateView),TEXT("late_mouse_camera_unavailable"));const double LateError=Angle(LateView.GetRotation(),(BoundedLateResidual*Late.Quaternion()).GetNormalized());
        if(!bNegative){Check(LateError<.025,TEXT("late_mouse_was_stale_or_applied_twice"));}
        double RepeatCm=0,RepeatDeg=0;
        for(int32 I=0;I<4;++I){Check(Camera->ReadCameraTransform(Repeat),TEXT("repeat_camera_query_missing"));RepeatCm=FMath::Max(RepeatCm,FVector::Distance(Repeat.GetLocation(),LateView.GetLocation()));RepeatDeg=FMath::Max(RepeatDeg,Angle(Repeat.GetRotation(),LateView.GetRotation()));}
        if(!Camera->IsThirdPersonInspectionActive()){FMinimalViewInfo POV;Character->CalcCamera(0,POV);RepeatCm=FMath::Max(RepeatCm,FVector::Distance(POV.Location,LateView.GetLocation()));RepeatDeg=FMath::Max(RepeatDeg,Angle(POV.Rotation.Quaternion(),LateView.GetRotation()));}
        Check(RepeatCm<.001&&RepeatDeg<.001,TEXT("same_frame_camera_or_trace_read_advanced_filter"));
        Controller->SetControlRotation(OriginalControl);Camera->UpdateCameraFromAppearance();
        if(CaseIndex==1&&CaseFrame==24)
        {
            // Change the component's world basis without evaluating a new pose.
            // The old mouse basis must travel with it. Derive the expected result
            // from measured local head pose and known control/actor transforms,
            // rather than reusing the camera's published snapshot parameters.
            const FQuat ActorBefore=Character->GetActorQuat(),MeshBefore=Body->GetComponentQuat();
            const FQuat HeadBefore=BA[H].GetRotation();const FRotator ActorRotationBefore=Character->GetActorRotation();
            Character->SetActorRotation(FRotator(ActorRotationBefore.Pitch,ActorRotationBefore.Yaw+80,ActorRotationBefore.Roll));
            const FQuat Transport=(Body->GetComponentQuat()*MeshBefore.Inverse()).GetNormalized();
            const FQuat EvaluatedAim=(OriginalControl.Quaternion()*ActorBefore.Inverse()).GetNormalized();
            const FQuat AuthoredResidual=(BodyDelta*EvaluatedAim.Inverse()).GetNormalized();
            const FRotator NewMouse(70,OriginalControl.Yaw-123,0);Controller->SetControlRotation(NewMouse);
            FTransform TransportedView;Check(Camera->ReadCameraTransform(TransportedView),TEXT("late_component_transport_camera_missing"));
            const FQuat ExpectedTransported=(Transport*AuthoredResidual*Transport.Inverse()*NewMouse.Quaternion()).GetNormalized();
            TransportError=Angle(TransportedView.GetRotation(),ExpectedTransported);
            Check(TransportError<.025,TEXT("late_component_rotation_did_not_transport_evaluated_mouse_basis"));
            const auto StillPose=ComponentPose(Body.Get());Check(Angle(StillPose[H].GetRotation(),HeadBefore)<.0001,TEXT("late_component_query_mutated_head_local_pose"));
            Character->SetActorRotation(ActorRotationBefore);Controller->SetControlRotation(OriginalControl);Camera->UpdateCameraFromAppearance();bTransport=true;
        }
        if(CaseFrame==20&&CaseIndex==0)
        {
            FTransform Before,After;Camera->ReadCameraTransform(Before);const FVector OriginalLocation=Character->GetActorLocation(),Travel(0,0,100);Character->SetActorLocation(OriginalLocation+Travel,false,nullptr,ETeleportType::TeleportPhysics);
            Check(Camera->ReadCameraTransform(After),TEXT("atomic_capsule_camera_missing"));AtomicTravelError=FVector::Distance(After.GetLocation()-Before.GetLocation(),Travel);Check(AtomicTravelError<.02,TEXT("camera_filtered_out_capsule_world_travel"));Character->SetActorLocation(OriginalLocation,false,nullptr,ETeleportType::TeleportPhysics);Camera->UpdateCameraFromAppearance();bAtomic=true;
        }
        if(CaseFrame>=14&&Synced&&Phase<.005)
        {
            ++PairSamples;Check(Lower<.15,TEXT("head_or_aim_changed_lower_body_local_rotation"));Check(Translation<.02&&Scale<.001,TEXT("head_or_aim_changed_render_bone_translation_scale"));Check(Fingers<.15,TEXT("head_or_aim_changed_wrist_finger_rotation"));Check(VisibleLength<.01,TEXT("appearance_limb_length_changed_more_than_one_percent"));
            MaxLower=FMath::Max(MaxLower,Lower);MaxTranslation=FMath::Max(MaxTranslation,Translation);MaxScale=FMath::Max(MaxScale,Scale);MaxFingers=FMath::Max(MaxFingers,Fingers);MaxVisibleLength=FMath::Max(MaxVisibleLength,VisibleLength);
        }
        const double Speed=Movement->Velocity.Size2D();PeakSpeed=FMath::Max(PeakSpeed,Speed);PeakGait=FMath::Max(PeakGait,static_cast<double>(Stance->GroundLocomotionPlayRate));
        if(Movement->IsMovingOnGround()&&C.Speed>0&&CaseFrame>=22&&M==nullptr)
        {
            const double Expected=Speed>600?FMath::Clamp(Speed/600.,1.,3.):1.;Check(FMath::Abs(Stance->GroundLocomotionPlayRate-Expected)<.025,TEXT("ground_play_rate_not_based_on_actual_velocity"));
            // Read the actual compiled runtime node, so an unconnected native float
            // cannot pass merely by publishing a plausible tuning value.
            bool Found=false;const auto* Generated=Cast<UAnimBlueprintGeneratedClass>(Anim->GetClass());
            if(Generated){for(FStructProperty* Property:Generated->GetAnimNodeProperties())
            {if(Property&&Property->Struct==FAnimNode_BlendSpacePlayer::StaticStruct())
                {const auto* Player=Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayer>(Anim);const UBlendSpace* Blend=Player?Player->GetBlendSpace():nullptr;
                 if(Blend&&(Blend->GetName()==TEXT("BS_Unarmed_PalmReference")||Blend->GetName()==TEXT("BS_Idle_Walk_Run")))
                 {Found=true;Check(FMath::Abs(Player->GetPlayRate()-Stance->GroundLocomotionPlayRate)<.025,TEXT("actual_ground_BlendSpace_node_ignores_native_play_rate"));}}
            }}Check(Found,TEXT("compiled_ground_BlendSpace_player_missing"));
        }
        const int32 Foot=Source->GetBoneIndex(TEXT("foot_l"));if(SA.IsValidIndex(Foot)){if(bHaveFoot&&CaseFrame>=14){FootTravel+=FVector::Distance(SA[Foot].GetLocation(),PreviousFoot);}PreviousFoot=SA[Foot].GetLocation();bHaveFoot=true;}
        if(Movement->IsFalling()){++AirSamples;JumpHeight=FMath::Max(JumpHeight,Character->GetActorLocation().Z-FixtureLocation.Z);Check(FMath::Abs(Stance->GroundLocomotionPlayRate-1)<.001,TEXT("airborne_advanced_ground_rate"));}
        if(AcceptedMontage.IsValid()&&Anim->GetInstanceForMontage(AcceptedMontage.Get()))
        {
            const auto* Instance=Anim->GetInstanceForMontage(AcceptedMontage.Get());if(Instance->GetWeight()>UE_SMALL_NUMBER)
            {
                // AnimInstance's public montage getters search only active instances
                // and return zero while this real instance still contributes blend-out.
                ++AttackSamples;ObservedRate=Instance->GetPlayRate();Check(FMath::Abs(ObservedRate-ExpectedRate)<.001,TEXT("accepted_montage_rate_not_attack_speed_snapshot"));
                const double Position=Instance->GetPosition();
                if(bHaveMontagePosition&&Instance->IsActive()&&Position>PreviousMontagePosition&&Position<AcceptedMontage->GetPlayLength()-.05)
                {Check(FMath::Abs((Position-PreviousMontagePosition)*30.-ExpectedRate)<.025,TEXT("actual_montage_clock_ignores_attack_speed"));}
                PreviousMontagePosition=Position;bHaveMontagePosition=true;
            }
        }
        const double Health=Target->GetRogueAttributeSet()->GetHealth();if(PreviousHealth-Health>.01){++DamagePulses;if(DamagePulses==1&&AcceptedTime>=0&&AcceptedSkill.IsValid()){FirstDamageError=World->GetTimeSeconds()-AcceptedTime-AcceptedSkill->HitStartDelaySeconds/ExpectedRate;if(C.Speed==0&&C.Pitch==0&&C.Attack==1){Check(FMath::Abs(FirstDamageError)<=2./30.+.005,TEXT("first_damage_time_does_not_match_attack_rate"));}}}PreviousHealth=Health;
        CaseHead=FMath::Max(CaseHead,SourceHead);CaseVisibleHead=FMath::Max(CaseVisibleHead,VisibleHead);CaseCamera=FMath::Max(CaseCamera,CameraResidual);CaseOffset=FMath::Max(CaseOffset,PositionResidual);MaxLate=FMath::Max(MaxLate,LateError);MaxRepeatCm=FMath::Max(MaxRepeatCm,RepeatCm);MaxRepeatDeg=FMath::Max(MaxRepeatDeg,RepeatDeg);
        Frames+=FString::Printf(TEXT("%d,%s,%d,%.9f,%.6f,%.6f,%.6f,%d,%d,%s,%.6f,%.6f,%.6f,%.3f,%.3f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.9f\n"),Frame,C.Name,CaseFrame,World->GetTimeSeconds(),OriginalControl.Pitch,OriginalControl.Yaw,Speed,Character->IsSprinting(),Movement->IsFalling(),M?*M->GetName():TEXT(""),M?Anim->Montage_GetPlayRate(M):0,M?Anim->Montage_GetPosition(M):0,Stance->GroundLocomotionPlayRate,Limit,PositionLimit,SourceHead,VisibleHead,CameraResidual,PositionResidual,LateError,RepeatCm,RepeatDeg,Lower,Translation,Scale,Fingers,VisibleLength*100,Stance->bMouseDrivenHeadApplied,Phase);
        if(bCapture&&bMovie){FScreenshotRequest::RequestScreenshot(Images/FString::Printf(TEXT("frame_%04d.png"),Frame),true,false);}else if(bCapture&&(CaseFrame==20||CaseFrame==35||CaseFrame==55)){FScreenshotRequest::RequestScreenshot(Images/FString::Printf(TEXT("case_%02d_%s_%02d.png"),CaseIndex,C.Name,CaseFrame),true,false);}
    }
    void CompleteCase()
    {
        const auto& C=Cases[CaseIndex];int32 Uses=0;if(C.Attack>0&&C.Attack<4){const int32 I=ExpectedSkillIndex();Uses=Progress->GetSkillUseCount(Skills[I]->GetFName())-BeforeUses[I];Check(Uses==1,TEXT("expected_input_attack_not_accepted_exactly_once"));Check(AttackSamples>=2,TEXT("accepted_attack_not_visibly_playing"));if(C.Speed==0&&C.Pitch==0&&C.Attack==1){Check(DamagePulses==1,TEXT("real_rate_test_target_not_hit_once"));}}
        Check(PairSamples>=3,TEXT("insufficient_same_phase_lower_body_samples"));if(C.Speed>0&&C.Attack!=5){Check(PeakSpeed>=C.Speed*.65,TEXT("actual_move_input_did_not_reach_requested_speed"));Check(FootTravel>1,TEXT("moving_lower_body_was_frozen"));}
        if(C.Attack==5){Check(AirSamples>=3&&JumpHeight>10,TEXT("actual_jump_and_capsule_motion_not_observed"));}
        Check(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()==C.bInspection,TEXT("actual_V_key_inspection_state_wrong"));
        Results+=FString::Printf(TEXT("%s,%d,%d,%d,%d,%.6f,%.6f,%.9f,%d,%.6f,%.6f,%.6f,%d,%.6f,%.9f,%.9f,%.9f,%.9f\n"),C.Name,CaseFrame,PairSamples,AttackSamples,Uses,C.AttackSpeed,ObservedRate,FirstDamageError,DamagePulses,PeakSpeed,PeakGait,FootTravel,AirSamples,JumpHeight,CaseHead,CaseVisibleHead,CaseCamera,CaseOffset);
        UE_LOG(LogRogue10m,Display,TEXT("MOUSE_HEAD_STABLE CASE_RESULT name=%s source_head=%.6f appearance_head=%.6f camera=%.6f position=%.6f pair=%d uses=%d rate=%.3f speed=%.3f gait=%.3f"),C.Name,CaseHead,CaseVisibleHead,CaseCamera,CaseOffset,PairSamples,Uses,ObservedRate,PeakSpeed,PeakGait);
    }
    void Step()
    {
        if(Warmup>0){--Warmup;if(Warmup==0){Check(bPolicyChecked,TEXT("forbidden_runtime_gate_proof_missing"));Check(Target->GetRogueAttributeSet()->GetHealth()==100000,TEXT("rejected_forbidden_attack_scheduled_damage"));BeginCase();}return;}
        if(!Character.IsValid()||!Source.IsValid()||!Body.IsValid()||!Baseline.IsValid()||!Stance.IsValid()){Check(false,TEXT("runtime_fixture_object_lost"));Finish();return;}
        const auto& C=Cases[CaseIndex];Controller->RotationInput=FRotator::ZeroRotator;if(C.Speed>0){Move(1);}Observe();
        if(C.bInspection&&CaseFrame==4){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Pressed,1.f));}if(C.bInspection&&CaseFrame==5){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Released,0.f));}
        if(C.bYawWrap&&CaseFrame==18){Controller->SetControlRotation(FRotator(C.Pitch,-179,0));}
        if(CaseFrame==12){if(C.Attack==1){Attack(true,true);Attack(false,true);}if(C.Attack==2||C.Attack==3){Attack(true,false);}if(C.Attack==4){Refill();Input(TEXT("DoDodge"));MirrorMontage();}if(C.Attack==5){Input(TEXT("DoJumpStart"));}}
        if(CaseFrame==15&&C.Attack==2){Attack(false,false);}if(CaseFrame==43&&C.Attack==3){Attack(false,false);}if(CaseFrame==17&&C.Attack==5){Input(TEXT("DoJumpEnd"));}
        // Change the live stat after approval. The accepted montage and its first
        // scheduled hit must continue using the original attack-speed snapshot.
        if(CaseFrame==16&&CaseIndex>=7&&CaseIndex<=10&&AcceptedTime>=0){Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(C.AttackSpeed*1.4f);}
        ++Frame;++CaseFrame;if(CaseFrame==CaseFrames){CompleteCase();++CaseIndex;if(CaseIndex==UE_ARRAY_COUNT(Cases)){Check(bAtomic,TEXT("atomic_capsule_translation_proof_missing"));Check(bTransport,TEXT("late_component_basis_transport_proof_missing"));Finish();return;}BeginCase();}
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        if(bPrepared&&Character.IsValid())
        {
            Character->GetCombatComponent()->CancelBoundSkillInput();Character->GetCombatComponent()->CancelCombatVisuals();Move(0);Input(TEXT("DoSprintEnd"));Character->StopJumping();Character->GetCharacterMovement()->StopMovementImmediately();
            Character->bUseControllerRotationYaw=OldYaw;Character->SetEquippedWeaponType(OldWeapon);Character->SetActorTransform(OldActor,false,nullptr,ETeleportType::TeleportPhysics);Character->SetCanBeDamaged(OldDamage);Character->GetRogueAttributeSet()->SetMoveSpeed(OldMoveSpeed);Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(OldAttackSpeed);Character->GetCharacterMovement()->MaxWalkSpeed=OldMaxWalkSpeed;
            if(Source.IsValid()){Source->SetForcedLOD(OldSourceLOD);Source->bEnableUpdateRateOptimizations=OldSourceRate;if(auto* A=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance())){A->bEnableViewAlignedUpperBody=OldAim;A->bEnableMouseDrivenHead=OldHead;}}
            if(Body.IsValid()){Body->SetForcedLOD(OldBodyLOD);Body->bEnableUpdateRateOptimizations=OldBodyRate;}if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()!=OldThird){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}if(Controller.IsValid()){Controller->SetControlRotation(OldControl);if(OldInput){Character->EnableInput(Controller.Get());}}
        }
        if(BodyBaseline.IsValid()){BodyBaseline->DestroyComponent();}if(Baseline.IsValid()){Baseline->DestroyComponent();}if(Ground.IsValid()){Ground->Destroy();}if(Target.IsValid()){Target->Destroy();}if(bFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        if(!Output.IsEmpty())
        {
            Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_write_failed"));Check(FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.csv"))),TEXT("case_evidence_write_failed"));
            FString FailureText;TArray<FString> Keys;Reasons.GetKeys(Keys);Keys.Sort();for(const FString& K:Keys){FailureText+=FString::Printf(TEXT("%s=%d\n"),*K,Reasons[K]);}Check(FFileHelper::SaveStringToFile(FailureText,*(Output/TEXT("failure-reasons.txt"))),TEXT("failure_evidence_write_failed"));
            const FString Metrics=FString::Printf(TEXT("frames=%d\ncases=%d\nexpected_cases=%d\nassertions=%d\nfailures=%d\nnegative_disabled_head=%d\nmax_late_mouse_oracle_error_deg=%.9f\nmax_same_frame_repeat_error_cm=%.9f\nmax_same_frame_repeat_error_deg=%.9f\natomic_capsule_world_translation_error_cm=%.9f\nlate_component_basis_transport_error_deg=%.9f\nmax_paired_source_lowerbody_rotation_error_deg=%.9f\nmax_source_render_local_translation_error_cm=%.9f\nmax_source_render_local_scale_error=%.9f\nmax_source_wrist_finger_local_rotation_error_deg=%.9f\nmax_appearance_limb_length_change_percent=%.9f\nNOTE=Actual selected production pawn, full Appearance retarget mesh, fixed-step ground physics and real attack/move/sprint/jump/dodge/V handlers. Source and visible head quaternions are composed from raw evaluated local bones against skeleton reference, independently of camera helper. Residual total quaternion caps are 2/4/3 degrees; body-local position caps are 1/2/1.5cm. Late input uses actual head delta minus independent control/actor aim plus new mouse, exactly once; same-pose actor yaw +80 and control pitch70 require exact conjugation into the current component basis. Source/baseline share movement prerequisite and unchanged AnimBP, with baseline aim and mouse-head disabled. Source render-bone local T/S, wrist/finger rotations and lower body must match same phase. Appearance checks finite normalized local poses and eight limb segment lengths against its same-phase retarget baseline, without claiming all target local T/S or finger preservation; camera reads and CalcCamera(0) must not advance filters. MoveSpeed and movement max speed are set to identical fixture tuning and actual movement input generates measured velocity. Native rate and actual compiled ground BlendSpace player rate plus visible foot travel are measured; attack speed changes through real attributes and accepted montage clock plus actual target damage are observed. Mesh surface quality requires complementary rendered review.\n"),Frame,CaseIndex,UE_ARRAY_COUNT(Cases),Assertions,Failures,bNegative,MaxLate,MaxRepeatCm,MaxRepeatDeg,AtomicTravelError,TransportError,MaxLower,MaxTranslation,MaxScale,MaxFingers,MaxVisibleLength*100.);
            Check(FFileHelper::SaveStringToFile(Metrics,*(Output/TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        }
        const bool Passed=Failures==0&&CaseIndex==UE_ARRAY_COUNT(Cases);UE_LOG(LogRogue10m,Display,TEXT("RESULT=MOUSE_HEAD_STABLE_%s frames=%d assertions=%d failures=%d output=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),Frame,Assertions,Failures,*Output);if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mPlayerState> Progress;TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<USkeletalMeshComponent> Source,Body,Baseline,BodyBaseline;TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> Stance;TWeakObjectPtr<AActor> Ground;TWeakObjectPtr<UAnimMontage> LastMirrored,AcceptedMontage;TWeakObjectPtr<const URogue10mAttackSkillData> AcceptedSkill;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;TArray<int32> BeforeUses;TArray<FTransform> SourceReference,BodyReference;TMap<FString,int32> Reasons;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Frames,Results;FTransform OldActor;FRotator OldControl;FVector FixtureLocation,PreviousFoot;ERogue10mWeaponType OldWeapon=ERogue10mWeaponType::Unarmed;
    int32 Frame=0,CaseIndex=0,CaseFrame=0,CaseFrames=75,Warmup=0,Assertions=0,Failures=0,PairSamples=0,AttackSamples=0,AirSamples=0,DamagePulses=0,LastMirrorFrame=-100,OldSourceLOD=0,OldBodyLOD=0,AuditedSkills=0;
    double OldDelta=1./30.,OldMoveSpeed=300,OldAttackSpeed=1,OldMaxWalkSpeed=300,AcceptedTime=-1,ExpectedRate=1,ObservedRate=1,FirstDamageError=0,PreviousHealth=100000,PreviousMontagePosition=0,CaseHead=0,CaseVisibleHead=0,CaseCamera=0,CaseOffset=0,PeakSpeed=0,PeakGait=0,FootTravel=0,JumpHeight=0,MaxLate=0,MaxRepeatCm=0,MaxRepeatDeg=0,AtomicTravelError=0,TransportError=0,MaxLower=0,MaxTranslation=0,MaxScale=0,MaxFingers=0,MaxVisibleLength=0;
    bool bFinished=false,bPrepared=false,bCapture=false,bMovie=false,bNegative=false,bFixed=false,bAtomic=false,bTransport=false,bHaveFoot=false,bHaveMontagePosition=false,bPolicyChecked=false,OldFixed=false,OldInput=false,OldDamage=true,OldThird=false,OldAim=true,OldHead=true,OldYaw=true,OldSourceRate=false,OldBodyRate=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished())){return;}for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestMouseHeadStableMotion"),TEXT("Actual mouse-head, bounded camera, walk/run attack-speed and full body integration. Requires strict isolated skill/progression save paths."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
