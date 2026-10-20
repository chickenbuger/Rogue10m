// Transient integration fixture: production input, source AnimBP and visible retarget mesh.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetCompilingManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
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
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mViewAlignedUpperBodyTest
{
// Paired meshes share the actual pawn, collision floor, controller and locomotion inputs.
// The baseline independently evaluates the unchanged graph with aim disabled. Montages
// are mirrored only when production combat accepts one; no pose is forced or overwritten.
struct FCase
{
    const TCHAR* Name;
    float Pitch, Yaw;
    int32 Attack; // 0 idle, 1 LMB, 2 RMB tap, 3 RMB hold, 4 two LMBs, 5 dodge.
    bool bGuard, bMoving, bKnuckle, bInspection;
    float ResidualYaw=0;
};
static const FCase Cases[] = {
    {TEXT("neutral_idle"),0,0,0,false,false,false,false},
    {TEXT("idle_up30_yaw0"),30,0,0,false,false,false,false},
    {TEXT("idle_down30_yaw90"),-30,90,0,false,false,false,false},
    {TEXT("guard_up60_yaw180"),60,180,0,true,false,false,false},
    {TEXT("guard_down60_yaw270"),-60,-90,0,true,false,false,false},
    {TEXT("left_right_jabs_up30"),30,0,4,false,false,false,false},
    {TEXT("left_jab_down30_yaw90"),-30,90,1,false,false,false,false},
    {TEXT("straight_up60_yaw180"),60,180,2,false,false,false,false},
    {TEXT("hook_down60_yaw270"),-60,-90,3,false,false,false,false},
    {TEXT("moving_jab_up30_yaw90"),30,90,1,false,true,false,false},
    {TEXT("moving_knuckle_down30_yaw180"),-30,180,1,false,true,true,false},
    {TEXT("knuckle_special_up60_yaw270"),60,-90,2,false,false,true,false},
    {TEXT("clamp_up85"),85,0,0,true,false,false,false},
    {TEXT("clamp_down85"),-85,0,0,true,false,false,false},
    {TEXT("inspection_jab_up30_yaw90"),30,90,1,false,false,false,true},
    {TEXT("inspection_hook_down30_yaw0"),-30,0,3,false,false,false,true},
    {TEXT("residual_yaw_plus30"),30,90,0,true,false,false,true,30},
    {TEXT("residual_yaw_minus30"),-30,180,0,true,false,false,true,-30},
    {TEXT("residual_yaw_clamp_plus60"),30,-90,0,true,false,false,true,60},
    {TEXT("residual_yaw_clamp_minus60"),-30,0,0,true,false,false,true,-60},
    {TEXT("guard_up60_dodge"),60,0,5,true,false,false,true}
};
static constexpr int32 FramesPerCase=75;
static const TCHAR* LowerBones[]={TEXT("root"),TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),
    TEXT("ball_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("ball_r"),TEXT("ik_foot_root"),TEXT("ik_foot_l"),TEXT("ik_foot_r")};
static double Angle(const FQuat& A,const FQuat& B)
{return FMath::RadiansToDegrees(A.AngularDistance(B));}
static TArray<FTransform> ComponentPose(USkeletalMeshComponent* Mesh)
{
    const auto Local=Mesh->GetBoneSpaceTransformsView();const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    TArray<FTransform> Result;Result.SetNum(Local.Num());
    for(int32 I=0;I<Local.Num();++I){const int32 P=Ref.GetParentIndex(I);Result[I]=P==INDEX_NONE?Local[I]:Local[I]*Result[P];}
    return Result;
}
class FRun:public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;FString Slot,ProgressionPrefix;
        const auto ValidToken=[](const FString& Value,const TCHAR* Prefix,int32 Limit)
        {bool Valid=Value.StartsWith(Prefix)&&Value.Len()<=Limit;for(TCHAR C:Value){Valid&=FChar::IsAlnum(C)||C==TEXT('_');}return Valid;};
        if(!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot)||!ValidToken(Slot,TEXT("Rogue10m_Fighter_QA_"),100)
            ||!FParse::Value(FCommandLine::Get(),TEXT("RogueProgressionSavePrefix="),ProgressionPrefix)||!ValidToken(ProgressionPrefix,TEXT("Rogue10m_Progression_QA_"),80))
        {Check(false,TEXT("isolated_save_slot_required"));Finish();return;}
        FString Label=TEXT("runtime");FParse::Value(FCommandLine::Get(),TEXT("RogueUpperBodyAimLabel="),Label);
        Label=FPaths::MakeValidFileName(Label);Output=FPaths::ProjectDir()/TEXT("tmp/view-aligned-upper-body")/Label;
        Images=FPaths::ProjectSavedDir()/TEXT("Screenshots/WindowsEditor/ViewAlignedUpperBody")/Label;
        bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueUpperBodyAimCapture"));
        bNegative=FParse::Param(FCommandLine::Get(),TEXT("RogueUpperBodyAimDisabledControl"));
        bPairedControl=FParse::Param(FCommandLine::Get(),TEXT("RogueUpperBodyAimPairedBaselineControl"));
        if(IFileManager::Get().FileExists(*(Output/TEXT("metrics.txt")))){Check(false,TEXT("fresh_label_required"));Finish();return;}
        IFileManager::Get().MakeDirectory(*Output,true);if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Csv=TEXT("frame,case,case_frame,control_pitch,control_yaw,applied_pitch,applied_yaw,moving_alpha,guard_alpha,montage,phase_error_s,lower_error_deg,lower_error_cm,local_translation_error_cm,local_scale_error,finger_wrist_error_deg,source_head_error_deg,appearance_head_error_deg,head_displacement_cm,camera_control_error_deg,left_arm_aim_error_deg,right_arm_aim_error_deg,ik_hand_error_cm,appearance_hand_divergence_cm,dodge_active,head_frame_step_cm,head_component_step_cm,target_pitch_error_deg,target_yaw_error_deg\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const{return bFinished;}
private:
    void Check(bool Good,const TCHAR* Reason)
    {++Assertions;if(!Good){++Failures;++FailureReasons.FindOrAdd(FString(Reason));if(Failures<50){UE_LOG(LogRogue10m,Error,TEXT("UPPER_BODY_AIM FAIL frame=%d case=%d %s"),Frame,CaseIndex,Reason);}}}
    USkeletalMeshComponent* Clone(USkeletalMeshComponent* Original,USceneComponent* Parent,const TCHAR* Name)
    {
        auto* M=NewObject<USkeletalMeshComponent>(Character.Get(),FName(Name),RF_Transient);Character->AddInstanceComponent(M);M->SetupAttachment(Parent);
        M->SetRelativeTransform(Original->GetRelativeTransform());M->SetSkeletalMeshAsset(Original->GetSkeletalMeshAsset());
        M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetGenerateOverlapEvents(false);M->SetCanEverAffectNavigation(false);
        M->SetVisibility(false,false);M->SetHiddenInGame(true,false);M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        M->bSuppressNotifyEventDispatch=true;
        M->bEnableUpdateRateOptimizations=false;M->SetForcedLOD(1);M->PrimaryComponentTick.TickGroup=Original->PrimaryComponentTick.TickGroup;
        M->RegisterComponent();M->SetAnimInstanceClass(Original->GetAnimInstance()->GetClass());return M;
    }
    void Move(float Forward)
    {
        struct FParams{float Right=0,Forward=0;} P;P.Forward=Forward;
        Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&P);
        Character->GetCharacterAnimationComponent()->SetMovementState(Forward!=0,false);
    }
    void Refill()
    {auto* A=Character->GetRogueAttributeSet();A->SetStamina(A->GetMaxStamina());A->SetMana(A->GetMaxMana());A->SetIdentity(A->GetMaxIdentity());}
    void Prepare()
    {
        Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid()||!Character->GetCombatComponent()||!Character->GetCharacterAnimationComponent()||!Character->GetAppearanceCameraComponent()||!Character->GetRogueAttributeSet())
        {Check(false,TEXT("required_production_pawn_missing"));Finish();return;}
        Source=Character->GetAnimationPlaybackMesh();Body=Character->GetMesh();Progress=Character->GetPlayerState<ARogue10mPlayerState>();
        if(!Source.IsValid()||!Body.IsValid()||Source==Body||!Source->GetAnimInstance()||!Body->GetAnimInstance()||!Progress.IsValid())
        {Check(false,TEXT("source_to_appearance_retarget_required"));Finish();return;}
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());if(!Stance.IsValid()){Check(false,TEXT("native_parent_missing"));Finish();return;}
        OldActor=Character->GetActorTransform();OldControl=Controller->GetControlRotation();OldWeapon=Character->GetEquippedWeaponType();OldThird=Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive();
        OldInput=Character->InputEnabled();OldDamage=Character->CanBeDamaged();OldControllerYaw=Character->bUseControllerRotationYaw;OldSourceLOD=Source->GetForcedLOD();OldBodyLOD=Body->GetForcedLOD();
        OldSourceRate=Source->bEnableUpdateRateOptimizations;OldBodyRate=Body->bEnableUpdateRateOptimizations;OldAim=Stance->bEnableViewAlignedUpperBody;bPrepared=true;
        Character->DisableInput(Controller.Get());Character->SetCanBeDamaged(false);Controller->CloseAllBlockingPanels();
        Source->SetForcedLOD(1);Body->SetForcedLOD(1);Source->bEnableUpdateRateOptimizations=false;Body->bEnableUpdateRateOptimizations=false;
        auto* Movement=Character->GetCharacterMovement();const FTickPrerequisite MovementPrerequisite(Movement,Movement->PrimaryComponentTick);
        bAddedSourceMovementPrerequisite=!Source->PrimaryComponentTick.GetPrerequisites().Contains(MovementPrerequisite);
        Source->AddTickPrerequisiteComponent(Movement);
        Baseline=Clone(Source.Get(),Source->GetAttachParent(),TEXT("UpperBodyAimSourceBaseline"));Baseline->AddTickPrerequisiteComponent(Source.Get());
        Baseline->AddTickPrerequisiteComponent(Movement);
        BodyBaseline=Clone(Body.Get(),Baseline.Get(),TEXT("UpperBodyAimAppearanceBaseline"));BodyBaseline->AddTickPrerequisiteComponent(Baseline.Get());
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
        if(!Ground.IsValid()){Check(false,TEXT("collision_floor_missing"));Finish();return;}
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("UpperBodyAimGround"),RF_Transient);Ground->AddInstanceComponent(Box);Ground->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10000,10000,20));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
        Ground->SetActorLocation(OldActor.GetLocation()+FVector(0,0,980));FixtureLocation=OldActor.GetLocation()+FVector(0,0,1000+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
        const TCHAR* Paths[]={TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special")};
        for(const TCHAR* Path:Paths){auto* S=LoadObject<URogue10mAttackSkillData>(nullptr,Path);if(!S||!S->AttackMontage){Check(false,TEXT("knuckle_skill_missing"));Finish();return;}KnuckleSkills.Add(S);}
        const TCHAR* FistPaths[]={TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook")};
        for(const TCHAR* Path:FistPaths){auto* S=LoadObject<URogue10mAttackSkillData>(nullptr,Path);if(!S||!S->AttackMontage){Check(false,TEXT("fist_skill_missing"));Finish();return;}FistSkills.Add(S);}
        const auto* MotionData=Character->GetCharacterAnimationComponent()->GetMotionData();ConfiguredDodge=MotionData?MotionData->DodgeMontage.Get():nullptr;
        Check(ConfiguredDodge.IsValid(),TEXT("configured_dodge_montage_missing"));
        if(bCapture&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))){FAssetCompilingManager::Get().FinishAllCompilation();}
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bOwnFixed=true;FApp::SetFixedDeltaTime(1./30.);FApp::SetUseFixedTimeStep(true);
        // Registering a component and changing the global timestep halfway through
        // an existing frame must not become the first paired observation. Let both
        // meshes complete fixed-step ticks, then restart both graphs together.
        Source->InitAnim(true);Baseline->InitAnim(true);Body->InitAnim(true);BodyBaseline->InitAnim(true);WarmupFrames=15;
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Step();}});
        UE_LOG(LogRogue10m,Display,TEXT("UPPER_BODY_AIM TIMELINE cases=%d frames_per_case=%d capture=%d disabled_negative=%d"),UE_ARRAY_COUNT(Cases),FramesPerCase,bCapture,bNegative);
    }
    void BeginCase()
    {
        const auto& C=Cases[CaseIndex];auto* Combat=Character->GetCombatComponent();Combat->CancelCombatVisuals();Move(0);Character->GetCharacterMovement()->StopMovementImmediately();
        Character->bUseControllerRotationYaw=C.ResidualYaw==0?OldControllerYaw:false;
        Character->SetActorLocation(FixtureLocation,false,nullptr,ETeleportType::TeleportPhysics);Character->SetActorRotation(FRotator(0,C.Yaw,0));Controller->SetControlRotation(FRotator(C.Pitch,C.Yaw+C.ResidualYaw,0));
        Character->SetEquippedWeaponType(C.bKnuckle?ERogue10mWeaponType::Knuckle:ERogue10mWeaponType::Unarmed);Character->GetCharacterAnimationComponent()->ResetFighterCombatStance();
        if(C.bKnuckle){for(auto& S:KnuckleSkills){Combat->UnlockAttackSkill(S.Get());}Check(Combat->AssignSkillToInputSlot(KnuckleSkills[0].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary_assign_failed"));Check(Combat->AssignSkillToInputSlot(KnuckleSkills[1].Get(),ERogue10mAttackInputSlot::Special),TEXT("knuckle_special_assign_failed"));}
        auto* Camera=Character->GetAppearanceCameraComponent();if(Camera->IsThirdPersonInspectionActive()){Camera->ToggleThirdPersonInspection();}
        Source->InitAnim(true);Baseline->InitAnim(true);Body->InitAnim(true);BodyBaseline->InitAnim(true);Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        auto* Control=Cast<URogue10mFighterCombatStanceAnimInstance>(Baseline->GetAnimInstance());
        Check(Stance.IsValid()&&Control,TEXT("paired_native_parent_missing"));if(!Stance.IsValid()||!Control){Finish();return;}
        Stance->bEnableViewAlignedUpperBody=!(bNegative||bPairedControl);Control->bEnableViewAlignedUpperBody=false;
        if(C.bGuard){Character->GetCharacterAnimationComponent()->RequestFighterCombatStance(3.f);}
        CaseFrame=0;LastMirrored.Reset();LastMirrorFrame=-100;CaseAimSamples=0;CaseAttackSamples=0;CaseAppearancePeak=0;CaseDodgeSamples=0;CaseDodgeReturnSamples=0;AimReturnFrame=0;bHavePreviousHead=false;
        InitialUses.Reset();for(auto& S:FistSkills){InitialUses.Add(Progress->GetSkillUseCount(S->GetFName()));}for(auto& S:KnuckleSkills){InitialUses.Add(Progress->GetSkillUseCount(S->GetFName()));}
        SecondJabFrame=12+FMath::Max(1,FMath::CeilToInt(FistSkills[0]->ComboWindowOpenSeconds/FMath::Max(.1f,Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier())*30.f)+1);
        Check(SecondJabFrame<50,TEXT("left_jab_combo_window_exceeds_fixture"));
        UE_LOG(LogRogue10m,Display,TEXT("UPPER_BODY_AIM CASE index=%d name=%s pitch=%.1f yaw=%.1f"),CaseIndex,C.Name,C.Pitch,C.Yaw);
    }
    bool MirrorMontage()
    {
        auto* Actual=Source->GetAnimInstance();auto* Ref=Baseline->GetAnimInstance();auto* M=Actual->GetCurrentActiveMontage();
        if(M!=LastMirrored.Get())
        {
            if(M){Ref->Montage_Play(M,Actual->Montage_GetPlayRate(M));Ref->Montage_SetPosition(M,Actual->Montage_GetPosition(M));}
            // A montage becoming inactive may still have its authored blend-out
            // weight. The mirrored clip already owns the same automatic blend-out;
            // an extra Stop here would replace it with a different interpolation.
            LastMirrored=M;LastMirrorFrame=Frame;return false;
        }
        return Frame-LastMirrorFrame>7;
    }
    void Press(bool Primary)
    {
        Refill();Character->GetCombatComponent()->HandleAttackPressed(Primary);
        MirrorMontage();
    }
    void Release(bool Primary){Character->GetCombatComponent()->HandleAttackReleased(Primary);MirrorMontage();}
    void Observe()
    {
        const auto& C=Cases[CaseIndex];const bool Synced=MirrorMontage();const auto A=Source->GetBoneSpaceTransformsView(),B=Baseline->GetBoneSpaceTransformsView();
        const auto SA=ComponentPose(Source.Get()),SB=ComponentPose(Baseline.Get()),VA=ComponentPose(Body.Get()),VB=ComponentPose(BodyBaseline.Get());
        const auto& Ref=Source->GetSkeletalMeshAsset()->GetRefSkeleton();double LowerDeg=0,LowerCm=0,Translation=0,Scale=0,Fingers=0,VisibleFingers=0,Head=0,VisibleHead=0,HeadTravel=0,CameraDelta=0,ArmL=0,ArmR=0,IK=0,VisibleHand=0,LimbFraction=0;
        auto* ActualAnim=Source->GetAnimInstance();auto* M=ActualAnim->GetCurrentActiveMontage();const double Phase=M?FMath::Abs(ActualAnim->Montage_GetPosition(M)-Baseline->GetAnimInstance()->Montage_GetPosition(M)):0;
        for(int32 I=0;I<A.Num();++I)
        {
            Check(!A[I].ContainsNaN()&&A[I].GetRotation().IsNormalized(),TEXT("nonfinite_or_unnormalized_source_bone"));
            if(!B.IsValidIndex(I)){Check(false,TEXT("paired_bone_count_changed"));continue;}
            const FString Name=Ref.GetBoneName(I).ToString();if(!Name.StartsWith(TEXT("ik_hand"))){Translation=FMath::Max(Translation,FVector::Distance(A[I].GetTranslation(),B[I].GetTranslation()));Scale=FMath::Max(Scale,FVector::Distance(A[I].GetScale3D(),B[I].GetScale3D()));}
            if(Name.StartsWith(TEXT("index_"))||Name.StartsWith(TEXT("middle_"))||Name.StartsWith(TEXT("ring_"))||Name.StartsWith(TEXT("pinky_"))||Name.StartsWith(TEXT("thumb_"))||Name==TEXT("hand_l")||Name==TEXT("hand_r"))
            {Fingers=FMath::Max(Fingers,Angle(A[I].GetRotation(),B[I].GetRotation()));}
        }
        for(const TCHAR* Name:LowerBones){const int32 I=Source->GetBoneIndex(Name);if(A.IsValidIndex(I)&&B.IsValidIndex(I)){LowerDeg=FMath::Max(LowerDeg,Angle(A[I].GetRotation(),B[I].GetRotation()));LowerCm=FMath::Max(LowerCm,FVector::Distance(A[I].GetLocation(),B[I].GetLocation()));}}
        const auto VL=Body->GetBoneSpaceTransformsView(),VBL=BodyBaseline->GetBoneSpaceTransformsView();const auto& BodyRef=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
        for(int32 I=0;I<VL.Num();++I)
        {
            Check(!VL[I].ContainsNaN()&&VL[I].GetRotation().IsNormalized(),TEXT("nonfinite_or_unnormalized_visible_appearance_bone"));
            if(!VBL.IsValidIndex(I)){Check(false,TEXT("visible_baseline_bone_count_changed"));continue;}
            const FString Name=BodyRef.GetBoneName(I).ToString();
            if(Name.StartsWith(TEXT("index_"))||Name.StartsWith(TEXT("middle_"))||Name.StartsWith(TEXT("ring_"))||Name.StartsWith(TEXT("pinky_"))||Name.StartsWith(TEXT("thumb_")))
            {VisibleFingers=FMath::Max(VisibleFingers,Angle(VL[I].GetRotation(),VBL[I].GetRotation()));}
        }
        for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("upperarm_l"),TEXT("lowerarm_l")},{TEXT("lowerarm_l"),TEXT("hand_l")},{TEXT("upperarm_r"),TEXT("lowerarm_r")},{TEXT("lowerarm_r"),TEXT("hand_r")},{TEXT("thigh_l"),TEXT("calf_l")},{TEXT("calf_l"),TEXT("foot_l")},{TEXT("thigh_r"),TEXT("calf_r")},{TEXT("calf_r"),TEXT("foot_r")}})
        {
            const int32 I=Body->GetBoneIndex(Pair.Key),J=Body->GetBoneIndex(Pair.Value);Check(VA.IsValidIndex(I)&&VA.IsValidIndex(J)&&VB.IsValidIndex(I)&&VB.IsValidIndex(J),TEXT("visible_arm_or_leg_probe_missing"));
            if(VA.IsValidIndex(I)&&VA.IsValidIndex(J)&&VB.IsValidIndex(I)&&VB.IsValidIndex(J))
            {const double Original=FVector::Distance(VB[I].GetLocation(),VB[J].GetLocation()),Actual=FVector::Distance(VA[I].GetLocation(),VA[J].GetLocation());Check(Original>.01,TEXT("visible_limb_baseline_length_degenerate"));LimbFraction=FMath::Max(LimbFraction,FMath::Abs(Actual-Original)/FMath::Max(.01,Original));}
        }
        const int32 H=Source->GetBoneIndex(TEXT("head")),VH=Body->GetBoneIndex(TEXT("head"));
        bool bDodgeActive=false;double HeadStep=0,HeadComponentStep=0;
        const UAnimMontage* Dodge=ConfiguredDodge.Get();
        for(const FAnimMontageInstance* Instance:ActualAnim->MontageInstances){if(Instance&&Dodge&&Instance->Montage==Dodge&&(Instance->IsActive()||Instance->GetWeight()>UE_SMALL_NUMBER)){bDodgeActive=true;}}
        // Target convergence starts after the actual dodge blend-out ends. The
        // independent pose pair may be warming up when the active montage becomes
        // null, so its comparison gate must not skip this transition clock.
        if(bDodgeActive){AimReturnFrame=CaseFrame;}
        if(SA.IsValidIndex(H)&&SB.IsValidIndex(H)){Head=Angle(SA[H].GetRotation(),SB[H].GetRotation());HeadTravel=FVector::Distance(SA[H].GetLocation(),SB[H].GetLocation());}
        if(VA.IsValidIndex(VH)&&VB.IsValidIndex(VH)){VisibleHead=Angle(VA[VH].GetRotation(),VB[VH].GetRotation());const FVector HeadWorld=Body->GetComponentTransform().TransformPosition(VA[VH].GetLocation());if(bHavePreviousHead){HeadStep=FVector::Distance(HeadWorld,PreviousHeadWorld);HeadComponentStep=FVector::Distance(VA[VH].GetLocation(),PreviousHeadComponent);}PreviousHeadWorld=HeadWorld;PreviousHeadComponent=VA[VH].GetLocation();bHavePreviousHead=true;if(C.Attack==5&&CaseFrame>=10){MaxDodgeHeadStep=FMath::Max(MaxDodgeHeadStep,HeadStep);MaxDodgeHeadComponentStep=FMath::Max(MaxDodgeHeadComponentStep,HeadComponentStep);}}
        const FVector Facing=Source->GetComponentTransform().InverseTransformVectorNoScale(Character->GetActorForwardVector()).GetSafeNormal();
        const float DesiredPitch=bPairedControl?0:FMath::Clamp(C.Pitch,-Stance->UpperBodyAimMaxPitch,Stance->UpperBodyAimMaxPitch),DesiredYaw=bPairedControl?0:FMath::Clamp(C.ResidualYaw,-Stance->UpperBodyAimMaxYaw,Stance->UpperBodyAimMaxYaw);
        const double TargetPitchError=FMath::Abs(Stance->UpperBodyAimPitch-DesiredPitch),TargetYawError=FMath::Abs(Stance->UpperBodyAimYaw-DesiredYaw);
        const float GeometryPitch=bNegative?DesiredPitch:Stance->UpperBodyAimPitch,GeometryYaw=bNegative?DesiredYaw:Stance->UpperBodyAimYaw;
        const FVector Desired=Source->GetComponentTransform().InverseTransformVectorNoScale(FRotator(GeometryPitch,C.Yaw+GeometryYaw,0).Vector()).GetSafeNormal();
        auto ArmError=[&](const TCHAR* Name){const int32 I=Source->GetBoneIndex(Name);if(!SA.IsValidIndex(I)||!SB.IsValidIndex(I)){return 180.;}const FQuat Delta=(SA[I].GetRotation()*SB[I].GetRotation().Inverse()).GetNormalized();return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Delta.RotateVector(Facing),Desired),-1.,1.)));};
        ArmL=ArmError(TEXT("upperarm_l"));ArmR=ArmError(TEXT("upperarm_r"));
        for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("ik_hand_l"),TEXT("hand_l")},{TEXT("ik_hand_r"),TEXT("hand_r")}})
        {const int32 I=Source->GetBoneIndex(Pair.Key),J=Source->GetBoneIndex(Pair.Value);if(SA.IsValidIndex(I)&&SA.IsValidIndex(J)){IK=FMath::Max(IK,FVector::Distance(SA[I].GetLocation(),SA[J].GetLocation()));}}
        for(const TCHAR* Name:{TEXT("hand_l"),TEXT("hand_r")}){const int32 I=Body->GetBoneIndex(Name);if(VA.IsValidIndex(I)&&VB.IsValidIndex(I)){VisibleHand=FMath::Max(VisibleHand,FVector::Distance(VA[I].GetLocation(),VB[I].GetLocation()));}}
        FTransform Camera;Check(Character->GetAppearanceCameraComponent()->ReadCameraTransform(Camera),TEXT("appearance_head_camera_unavailable"));CameraDelta=Angle(Camera.GetRotation(),Controller->GetControlRotation().Quaternion());
        Check(CameraDelta<=8.02,TEXT("head_camera_added_more_than_bounded_motion_to_mouse_aim"));
        if(CaseFrame>=12&&Synced&&Phase<.005)
        {
            ++CaseAimSamples;Check(LowerDeg<.15&&LowerCm<.02,TEXT("aim_changed_paired_lower_body_pose"));Check(Translation<.02&&Scale<.001,TEXT("aim_changed_render_local_translation_or_scale"));
            Check(Fingers<.15,TEXT("aim_changed_wrist_or_finger_local_rotation"));Check(Head<.15,TEXT("aim_changed_head_component_rotation"));Check(VisibleHead<3.,TEXT("retarget_added_head_rotation_to_aim"));
            Check(VisibleFingers<.3,TEXT("visible_finger_local_rotation_changed_with_aim"));Check(LimbFraction<.01,TEXT("visible_arm_or_leg_length_changed_more_than_one_percent"));
            if(bDodgeActive){++CaseDodgeSamples;Check(FMath::Abs(Stance->UpperBodyAimPitch)<.01&&FMath::Abs(Stance->UpperBodyAimYaw)<.01,TEXT("authored_full_body_dodge_retained_view_aim"));}
            else{
                if(CaseFrame-AimReturnFrame>=12){Check(TargetPitchError<1.,TEXT("applied_pitch_wrong_sign_or_clamp"));Check(TargetYawError<1.,TEXT("applied_residual_yaw_wrong_sign_or_clamp"));}
                Check(ArmL<3.&&ArmR<3.,TEXT("upper_arm_direction_does_not_follow_world_mouse_aim"));
                if(C.Attack==5&&CaseFrame>52){++CaseDodgeReturnSamples;}
            }
            // Neutral aim deliberately bypasses the proxy; original idle IK helpers
            // need not coincide with FK. Every nonzero aimed output must resync them.
            if((FMath::Abs(Stance->UpperBodyAimPitch)>UE_SMALL_NUMBER||FMath::Abs(Stance->UpperBodyAimYaw)>UE_SMALL_NUMBER)&&!bDodgeActive){Check(IK<.02,TEXT("FK_IK_hands_diverged"));}
            for(const TCHAR* Name:LowerBones){const int32 I=Body->GetBoneIndex(Name);if(VL.IsValidIndex(I)&&VBL.IsValidIndex(I)){const double Degrees=Angle(VL[I].GetRotation(),VBL[I].GetRotation());MaxVisibleLowerDeg=FMath::Max(MaxVisibleLowerDeg,Degrees);Check(Degrees<2.&&FVector::Distance(VL[I].GetLocation(),VBL[I].GetLocation())<.3,TEXT("visible_retarget_lowerbody_changed_with_aim"));}}
            Check(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()==C.bInspection,TEXT("actual_V_key_inspection_state_did_not_match_case"));
            // Independent camera rotation oracle uses the DISABLED visible pose's
            // authored head motion. View aim must not be folded into head motion again.
            const auto& VRef=BodyBaseline->GetSkeletalMeshAsset()->GetRefSkeleton();FTransform RefHead=VRef.GetRefBonePose()[VH];
            for(int32 P=VRef.GetParentIndex(VH);P!=INDEX_NONE;P=VRef.GetParentIndex(P)){RefHead*=VRef.GetRefBonePose()[P];}
            const FQuat MeshQ=BodyBaseline->GetComponentQuat();FQuat Motion=(MeshQ*VB[VH].GetRotation()*RefHead.GetRotation().Inverse()*MeshQ.Inverse()).GetNormalized();if(Motion.W<0){Motion=Motion*(-1.);}
            FVector Axis;double Radians=0;Motion.ToAxisAndAngle(Axis,Radians);auto* CameraSettings=Character->GetAppearanceCameraComponent();
            Radians=FMath::Min(Radians*CameraSettings->BrawlerHeadRotationWeight,FMath::DegreesToRadians(static_cast<double>(CameraSettings->BrawlerHeadMaxRotationDegrees)));
            Motion=Radians>UE_SMALL_NUMBER?FQuat(Axis.GetSafeNormal(),Radians):FQuat::Identity;const FQuat ExpectedCamera=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
            Check(Angle(ExpectedCamera,Camera.GetRotation())<.6,TEXT("actual_camera_contains_aim_motion_missing_from_authored_head"));
            MaxCameraOracleError=FMath::Max(MaxCameraOracleError,Angle(ExpectedCamera,Camera.GetRotation()));MaxVisibleHeadDeg=FMath::Max(MaxVisibleHeadDeg,VisibleHead);MaxVisibleFingerDeg=FMath::Max(MaxVisibleFingerDeg,VisibleFingers);MaxVisibleLimbFraction=FMath::Max(MaxVisibleLimbFraction,LimbFraction);
            if(M){++CaseAttackSamples;}CaseAppearancePeak=FMath::Max(CaseAppearancePeak,VisibleHand);
            MaxLowerDeg=FMath::Max(MaxLowerDeg,LowerDeg);MaxLocalTranslation=FMath::Max(MaxLocalTranslation,Translation);MaxHeadDeg=FMath::Max(MaxHeadDeg,Head);MaxHeadTravel=FMath::Max(MaxHeadTravel,HeadTravel);if(!bDodgeActive){MaxArmError=FMath::Max(MaxArmError,FMath::Max(ArmL,ArmR));if(FMath::Abs(Stance->UpperBodyAimPitch)>UE_SMALL_NUMBER||FMath::Abs(Stance->UpperBodyAimYaw)>UE_SMALL_NUMBER){MaxIKError=FMath::Max(MaxIKError,IK);}}
        }
        Csv+=FString::Printf(TEXT("%d,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.9f,%.9f,%.9f,%.9f\n"),Frame,C.Name,CaseFrame,C.Pitch,C.Yaw+C.ResidualYaw,Stance->UpperBodyAimPitch,Stance->UpperBodyAimYaw,Stance->MovingAttackUpperBodyAlpha,Stance->FighterCombatStanceAlpha,M?*M->GetName():TEXT(""),Phase,LowerDeg,LowerCm,Translation,Scale,Fingers,Head,VisibleHead,HeadTravel,CameraDelta,ArmL,ArmR,IK,VisibleHand,bDodgeActive,HeadStep,HeadComponentStep,TargetPitchError,TargetYawError);
        if(bCapture&&FParse::Param(FCommandLine::Get(),TEXT("RogueUpperBodyAimMovie"))){FScreenshotRequest::RequestScreenshot(Images/FString::Printf(TEXT("frame_%04d.png"),Frame),true,false);}
        else if(bCapture&&(CaseFrame==10||CaseFrame==22||CaseFrame==48)){FScreenshotRequest::RequestScreenshot(Images/FString::Printf(TEXT("case_%02d_%s_%02d.png"),CaseIndex,C.Name,CaseFrame),true,false);}
    }
    void Step()
    {
        if(WarmupFrames>0){--WarmupFrames;if(WarmupFrames==0){BeginCase();}return;}
        if(!Character.IsValid()||!Source.IsValid()||!Baseline.IsValid()||!Stance.IsValid()){Check(false,TEXT("runtime_fixture_object_lost"));Finish();return;}
        const auto& C=Cases[CaseIndex];Controller->RotationInput=FRotator::ZeroRotator;Controller->SetControlRotation(FRotator(C.Pitch,C.Yaw+C.ResidualYaw,0));
        if(C.bMoving){Move(.35f);}Observe();
        if(C.bInspection&&CaseFrame==4){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Pressed,1.f));}
        if(C.bInspection&&CaseFrame==5){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Released,0.f));}
        if(CaseFrame==12){if(C.Attack==1||C.Attack==4){Press(true);Release(true);}if(C.Attack==2||C.Attack==3){Press(false);}}
        if(CaseFrame==12&&C.Attack==5){Refill();Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoDodge")),nullptr);MirrorMontage();}
        if(CaseFrame==SecondJabFrame&&C.Attack==4){Press(true);Release(true);}
        if(CaseFrame==15&&C.Attack==2){Release(false);}if(CaseFrame==43&&C.Attack==3){Release(false);}
        ++Frame;++CaseFrame;if(CaseFrame==FramesPerCase)
        {
            Check(CaseAimSamples>=20,TEXT("insufficient_independent_same_pose_samples"));if(C.Attack){Check(CaseAttackSamples>=3,TEXT("actual_input_did_not_produce_attack_montage"));}
            if(C.Attack>0&&C.Attack<5){const int32 Expected=C.bKnuckle?(C.Attack==1?4:5):(C.Attack==1||C.Attack==4?0:(C.Attack==2?2:3));const auto* S=Expected<4?FistSkills[Expected].Get():KnuckleSkills[Expected-4].Get();Check(Progress->GetSkillUseCount(S->GetFName())-InitialUses[Expected]==1,TEXT("expected_real_input_skill_not_accepted_exactly_once"));if(C.Attack==4){Check(Progress->GetSkillUseCount(FistSkills[1]->GetFName())-InitialUses[1]==1,TEXT("second_LMB_did_not_accept_right_jab"));}}
            if(C.Attack==5){Check(CaseDodgeSamples>=3&&CaseDodgeReturnSamples>=3,TEXT("actual_dodge_or_return_to_aim_not_observed"));}
            if(FMath::Abs(C.Pitch)>1&&!bPairedControl){Check(CaseAppearancePeak>1.,TEXT("visible_appearance_hands_did_not_follow_aim"));}
            UE_LOG(LogRogue10m,Display,TEXT("UPPER_BODY_AIM CASE_RESULT name=%s aim_samples=%d montage_samples=%d appearance_hand_divergence_cm=%.6f"),C.Name,CaseAimSamples,CaseAttackSamples,CaseAppearancePeak);
            ++CaseIndex;if(CaseIndex==UE_ARRAY_COUNT(Cases)){Finish();return;}BeginCase();
        }
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        if(!Output.IsEmpty())
        {
            Check(FFileHelper::SaveStringToFile(Csv,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_write_failed"));
            const FString Metrics=FString::Printf(TEXT("frames=%d\ncases=%d\nassertions=%d\nfailures=%d\nnegative_control_disabled=%d\nmax_lower_rotation_error_deg=%.9f\nmax_render_local_translation_error_cm=%.9f\nmax_source_head_rotation_error_deg=%.9f\nmax_aim_head_displacement_cm=%.9f\nmax_upperarm_world_aim_error_deg=%.9f\nmax_high_pitch_dodge_head_frame_step_cm=%.9f\nNOTE=Production pawn controller and actual Combat input are used. Same-owner source and retarget baselines evaluate the original AnimBP with aim disabled. Comparisons require matching montage phase after blend warmup. Lowerbody, local render translations/scales, finger and wrist rotations must agree. Independent world-space upper-arm direction validates pitch sign across four world yaws; head rotation and bounded camera view exclude aim being applied twice. FK/IK hand positions must agree. Numeric pose checks do not certify fine mesh deformation; rendered captures complement them.\n"),Frame,CaseIndex,Assertions,Failures,bNegative,MaxLowerDeg,MaxLocalTranslation,MaxHeadDeg,MaxHeadTravel,MaxArmError,MaxDodgeHeadStep);
            Check(FFileHelper::SaveStringToFile(Metrics,*(Output/TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
            const FString VisibleMetrics=FString::Printf(TEXT("max_visible_lower_rotation_error_deg=%.9f\nmax_visible_head_rotation_error_deg=%.9f\nmax_visible_finger_local_rotation_error_deg=%.9f\nmax_visible_arm_leg_length_change_percent=%.9f\nmax_disabled_head_camera_oracle_error_deg=%.9f\nmax_aimed_FK_IK_hand_error_cm=%.9f\n"),MaxVisibleLowerDeg,MaxVisibleHeadDeg,MaxVisibleFingerDeg,MaxVisibleLimbFraction*100.,MaxCameraOracleError,MaxIKError);
            Check(FFileHelper::SaveStringToFile(VisibleMetrics,*(Output/TEXT("appearance-metrics.txt"))),TEXT("visible_metrics_write_failed"));
            FString Reasons;TArray<FString> Keys;FailureReasons.GetKeys(Keys);Keys.Sort();for(const FString& Reason:Keys){Reasons+=FString::Printf(TEXT("%s=%d\n"),*Reason,FailureReasons[Reason]);}
            Check(FFileHelper::SaveStringToFile(Reasons,*(Output/TEXT("failure-reasons.txt"))),TEXT("failure_reasons_write_failed"));
            const FString TransitionMetrics=FString::Printf(TEXT("paired_baseline_control=%d\nmax_high_pitch_dodge_head_component_frame_step_cm=%.9f\n"),bPairedControl,MaxDodgeHeadComponentStep);
            Check(FFileHelper::SaveStringToFile(TransitionMetrics,*(Output/TEXT("transition-metrics.txt"))),TEXT("transition_metrics_write_failed"));
        }
        if(bPrepared&&Character.IsValid())
        {
            Character->GetCombatComponent()->CancelCombatVisuals();Move(0);Character->GetCharacterMovement()->StopMovementImmediately();Character->bUseControllerRotationYaw=OldControllerYaw;Character->SetEquippedWeaponType(OldWeapon);Character->SetActorTransform(OldActor,false,nullptr,ETeleportType::TeleportPhysics);Character->SetCanBeDamaged(OldDamage);
            if(Source.IsValid()){Source->SetForcedLOD(OldSourceLOD);Source->bEnableUpdateRateOptimizations=OldSourceRate;if(bAddedSourceMovementPrerequisite){Source->RemoveTickPrerequisiteComponent(Character->GetCharacterMovement());}if(auto* A=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance())){A->bEnableViewAlignedUpperBody=OldAim;}}
            if(Body.IsValid()){Body->SetForcedLOD(OldBodyLOD);Body->bEnableUpdateRateOptimizations=OldBodyRate;}if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()!=OldThird){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
            if(Controller.IsValid()){Controller->SetControlRotation(OldControl);if(OldInput){Character->EnableInput(Controller.Get());}}
        }
        if(BodyBaseline.IsValid()){BodyBaseline->DestroyComponent();}if(Baseline.IsValid()){Baseline->DestroyComponent();}if(Ground.IsValid()){Ground->Destroy();}if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        const bool Passed=Failures==0&&CaseIndex==UE_ARRAY_COUNT(Cases);UE_LOG(LogRogue10m,Display,TEXT("RESULT=VIEW_ALIGNED_UPPER_BODY_%s frames=%d assertions=%d failures=%d output=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),Frame,Assertions,Failures,*Output);if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ARogue10mPlayerState> Progress;
    TWeakObjectPtr<USkeletalMeshComponent> Source,Body,Baseline,BodyBaseline;TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> Stance;TWeakObjectPtr<AActor> Ground;TWeakObjectPtr<UAnimMontage> LastMirrored,ConfiguredDodge;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> KnuckleSkills,FistSkills;TArray<int32> InitialUses;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Csv;FTransform OldActor;FRotator OldControl;FVector FixtureLocation,PreviousHeadWorld,PreviousHeadComponent;TMap<FString,int32> FailureReasons;
    ERogue10mWeaponType OldWeapon=ERogue10mWeaponType::Unarmed;int32 Frame=0,CaseIndex=0,CaseFrame=0,Assertions=0,Failures=0,CaseAimSamples=0,CaseAttackSamples=0,CaseDodgeSamples=0,CaseDodgeReturnSamples=0,SecondJabFrame=20,LastMirrorFrame=-100,AimReturnFrame=0,WarmupFrames=0,OldSourceLOD=0,OldBodyLOD=0;
    double CaseAppearancePeak=0,MaxLowerDeg=0,MaxLocalTranslation=0,MaxHeadDeg=0,MaxHeadTravel=0,MaxArmError=0,MaxDodgeHeadStep=0,MaxDodgeHeadComponentStep=0,MaxVisibleLowerDeg=0,MaxVisibleHeadDeg=0,MaxVisibleFingerDeg=0,MaxVisibleLimbFraction=0,MaxCameraOracleError=0,MaxIKError=0,OldDelta=1./30.;
    bool bFinished=false,bPrepared=false,bCapture=false,bNegative=false,bPairedControl=false,bOwnFixed=false,bHavePreviousHead=false,bAddedSourceMovementPrerequisite=false,OldFixed=false,OldInput=false,OldDamage=true,OldThird=false,OldAim=true,OldControllerYaw=true,OldSourceRate=false,OldBodyRate=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished())){return;}
    for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestViewAlignedUpperBody"),TEXT("Production source/Appearance aim integration; exact disabled graph baseline, pitch sign/world yaw/limits, actual LMB/RMB and movement/V. Requires isolated Rogue10m_Fighter_QA_ slot."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
