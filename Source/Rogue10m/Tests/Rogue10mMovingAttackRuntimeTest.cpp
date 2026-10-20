// Editor-only integration fixture; all meshes, ground and state changes are transient.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
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
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mMovingAttackTest
{
static const TCHAR* LowerBones[] = {TEXT("root"), TEXT("pelvis"), TEXT("thigh_l"), TEXT("thigh_r"),
    TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r")};
static const TCHAR* UpperBones[] = {TEXT("spine_01"), TEXT("spine_03"), TEXT("spine_05"), TEXT("upperarm_l"),
    TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r")};

static FString Columns(const FTransform& Transform)
{
    const FVector P=Transform.GetTranslation(), S=Transform.GetScale3D(); const FQuat Q=Transform.GetRotation();
    return FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f"),
        P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
}
static FString Header(const TCHAR* Prefix)
{
    FString Result;
    for(const TCHAR* Axis:{TEXT("tx"),TEXT("ty"),TEXT("tz"),TEXT("qx"),TEXT("qy"),TEXT("qz"),TEXT("qw"),TEXT("sx"),TEXT("sy"),TEXT("sz")})
    { Result+=FString::Printf(TEXT(",%s_%s"),Prefix,Axis); }
    return Result;
}
struct FAcceptance
{
    int32 Skill=INDEX_NONE, Pulses=0, ExpectedPulses=1;
    double Time=0, Delay=0, Interval=0, FirstError=0;
    bool bCancelled=false;
};

class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        FString Slot;
        if(!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot) || !Slot.StartsWith(TEXT("Rogue10m_Fighter_QA_")))
        { Check(false,TEXT("isolated_skill_save_slot_required")); Finish(); return; }
        FString Label=TEXT("runtime"); FParse::Value(FCommandLine::Get(),TEXT("RogueMovingAttackLabel="),Label);
        Label=FPaths::MakeValidFileName(Label); if(Label.IsEmpty()){Label=TEXT("runtime");}
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/moving-attack"),Label);
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/MovingAttack"),Label);
        bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueMovingAttackCapture"));
        if(IFileManager::Get().FileExists(*FPaths::Combine(Output,TEXT("metrics.txt"))))
        { Check(false,TEXT("fresh_evidence_label_required")); Finish(); return; }
        IFileManager::Get().MakeDirectory(*Output,true); if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Frames=TEXT("frame,world_s,stage,alpha,guard_alpha,speed_cm_s,grounded,falling,weapon,accepted_serial,montage,montage_s,montage_weight,actor_step_cm,source_lower_error_deg,source_lower_error_cm,source_upper_error_deg,source_hand_error_cm,appearance_lower_error_deg,appearance_hand_error_cm,source_notifies,baseline_notifies")+Header(TEXT("actor"))+TEXT("\n");
        Poses=TEXT("frame,stage,bone,parent")+Header(TEXT("local"))+Header(TEXT("raw_cs"))+TEXT("\n");
        Curves=TEXT("frame,stage,curve,value\n");
        Damage=TEXT("frame,world_s,skill,health_drop,pulse,elapsed_s,expected_s,error_s,cancelled\n");
        Skeletons=TEXT("stage,mesh,anim_class,bone,parent")+Header(TEXT("reference_local"))+TEXT("\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
            { if(W==Self->World.Get()){Self->Finish(false);} });
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool Good,const TCHAR* Why)
    {
        ++Assertions;
        if(!Good){++Failures; if(Failures<45){UE_LOG(LogRogue10m,Error,TEXT("MOVING_ATTACK FAIL frame=%d %s"),FrameIndex,Why);}}
    }
    void Refill()
    {
        if(auto* Attributes=Character->GetRogueAttributeSet())
        { Attributes->SetStamina(Attributes->GetMaxStamina()); Attributes->SetMana(Attributes->GetMaxMana()); Attributes->SetIdentity(Attributes->GetMaxIdentity()); }
    }
    void Move(float Forward)
    {
        struct FParams{float Right=0.f;float Forward=0.f;} Params; Params.Forward=Forward;
        // DoMove calls AddMovementInput: physics integrates the real pawn on a real collision floor.
        Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&Params);
        Character->GetCharacterAnimationComponent()->SetMovementState(!FMath::IsNearlyZero(Forward),false);
    }
    void Stop()
    { Move(0); Character->GetCharacterMovement()->StopMovementImmediately(); }
    void SynchronizeAnimationClocks()
    {
        // A full-body slot is allowed to stop updating its source pose at rest. Start independent
        // movement cases together, then require continuous agreement throughout each moving attack.
        Source->InitAnim(true); Baseline->InitAnim(true); Body->InitAnim(true); BodyBaseline->InitAnim(true);
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        if(bNeutralHead)
        {
            Stance->bEnableMouseDrivenHead=false;
            if(auto* Reference=Cast<URogue10mFighterCombatStanceAnimInstance>(Baseline->GetAnimInstance())){Reference->bEnableMouseDrivenHead=false;}
        }
        PreviousMontage.Reset(); PreviousMovingFeet.Reset();
    }
    USkeletalMeshComponent* CloneMesh(USkeletalMeshComponent* Original,USceneComponent* Parent,const TCHAR* Name)
    {
        auto* Mesh=NewObject<USkeletalMeshComponent>(Character.Get(),FName(Name),RF_Transient);
        Character->AddInstanceComponent(Mesh); Mesh->SetupAttachment(Parent);
        Mesh->SetRelativeTransform(Original->GetRelativeTransform());
        Mesh->SetSkeletalMeshAsset(Original->GetSkeletalMeshAsset());
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false); Mesh->SetVisibility(false,false); Mesh->SetHiddenInGame(true,false);
        Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->bEnableUpdateRateOptimizations=false; Mesh->SetForcedLOD(1);
        Mesh->PrimaryComponentTick.TickGroup=Original->PrimaryComponentTick.TickGroup;
        Mesh->RegisterComponent(); Mesh->SetAnimInstanceClass(Original->GetAnimInstance()->GetClass());
        return Mesh;
    }
    void MakeGround()
    {
        FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
        Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
        if(!Ground.IsValid()){Check(false,TEXT("transient_ground_spawn_failed"));return;}
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("MovingAttackGround"),RF_Transient);
        Ground->AddInstanceComponent(Box); Ground->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,20));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetGenerateOverlapEvents(false);
        Box->SetCanEverAffectNavigation(false); Box->RegisterComponent();
        const double Surface=OriginalLocation.Z+1000.;
        Ground->SetActorLocation(FVector(OriginalLocation.X,OriginalLocation.Y,Surface-20.));
        Character->SetActorLocation(FVector(OriginalLocation.X,OriginalLocation.Y,
            Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.),false,nullptr,ETeleportType::TeleportPhysics);
    }
    void Prepare()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid() || !Controller.IsValid() || !Character->GetCombatComponent() || !Character->GetBasicBrawlerComponent()
            || !Character->GetCharacterAnimationComponent() || !Character->GetAppearanceCameraComponent() || !Character->GetRogueAttributeSet())
        { Check(false,TEXT("required_player_components_missing")); Finish(); return; }
        Source=Character->GetAnimationPlaybackMesh(); Body=Character->GetMesh();
        if(!Source.IsValid() || !Body.IsValid() || !Source->GetAnimInstance() || !Body->GetAnimInstance()
            || !Source->GetSkeletalMeshAsset() || !Body->GetSkeletalMeshAsset())
        { Check(false,TEXT("required_source_or_appearance_animation_missing")); Finish(); return; }
        Check(Source.Get()!=Body.Get(),TEXT("fixture_requires_actual_source_to_appearance_retarget_path"));
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        Progress=Character->GetPlayerState<ARogue10mPlayerState>();
        if(!Stance.IsValid() || !Progress.IsValid())
        { Check(false,TEXT("native_animation_parent_or_player_state_missing")); Finish(); return; }
        const TCHAR* Paths[]={TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged")};
        for(const TCHAR* Path:Paths)
        {
            auto* Skill=LoadObject<URogue10mAttackSkillData>(nullptr,Path);
            if(!Skill || !Skill->AttackMontage){Check(false,TEXT("required_attack_or_montage_missing"));Finish();return;}
            Skills.Add(Skill); InitialUses.Add(Progress->GetSkillUseCount(Skill->GetFName())); LastUses.Add(InitialUses.Last());
            MovingSamples.Add(0); UpperPeaks.Add(0); MontageSamples.Add(0);
            for(const FSlotAnimationTrack& Track:Skill->AttackMontage->SlotAnimTracks)
            { Check(Track.SlotName==TEXT("DefaultSlot"),TEXT("original_default_slot_not_preserved")); }
        }
        KnuckleChargeRelease=820+FMath::CeilToInt(Skills[8]->ChargeSeconds*30.f)+3;
        Check(KnuckleChargeRelease<905,TEXT("knuckle_charge_exceeds_case_window"));
        if(Failures){Finish();return;}
        bPrepared=true; OldInput=Character->InputEnabled(); OldDamage=Character->CanBeDamaged();
        OldHead=Stance->bEnableMouseDrivenHead;
        bNeutralHead=FParse::Param(FCommandLine::Get(),TEXT("RogueMovingAttackNeutralAim"));
        OriginalLocation=Character->GetActorLocation(); OldControl=Controller->GetControlRotation();
        OldWalkSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;
        OldSourceLOD=Source->GetForcedLOD(); OldBodyLOD=Body->GetForcedLOD();
        OldSourceRate=Source->bEnableUpdateRateOptimizations; OldBodyRate=Body->bEnableUpdateRateOptimizations;
        OldThird=Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive();
        Character->DisableInput(Controller.Get()); Character->SetCanBeDamaged(false); Controller->CloseAllBlockingPanels();
        Character->GetCombatComponent()->CancelCombatVisuals(); Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
        Character->GetCharacterAnimationComponent()->ResetFighterCombatStance(); Stop();
        // Raw authored upper-body comparisons require neutral aim. The separate view-aim
        // fixture verifies tilted production poses against an independent disabled baseline.
        const float RegressionPitch=bNeutralHead ? 0.f : -20.f;
        LockedControl=FRotator(RegressionPitch,Character->GetActorRotation().Yaw,0.f); Controller->SetControlRotation(LockedControl);
        if(OldThird!=bCapture){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
        MakeGround(); if(Failures){Finish();return;}
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget(); if(auto* AI=It->GetController()){AI->UnPossess();}
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
            if(!Target.IsValid())
            {
                Target=*It;
                if(auto* Regen=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()){Regen->ConfigureRegeneration(0,0,0);}
                It->GetRogueAttributeSet()->SetMaxHealth(10000); It->GetRogueAttributeSet()->SetHealth(10000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
                PreviousHealth=It->GetRogueAttributeSet()->GetHealth();
            }
            else{It->SetActorEnableCollision(false);}
        }
        if(!Target.IsValid()){Check(false,TEXT("real_damage_target_missing"));Finish();return;}
        // Both AnimBPs start together and read the SAME movement component and collision ground.
        // No montage is ever played on the independent baseline; attack notifies cannot originate there.
        Baseline=CloneMesh(Source.Get(),Source->GetAttachParent(),TEXT("MovingAttackLocomotionBaseline"));
        Baseline->AddTickPrerequisiteComponent(Source.Get());
        BodyBaseline=CloneMesh(Body.Get(),Baseline.Get(),TEXT("MovingAttackRetargetBaseline"));
        BodyBaseline->AddTickPrerequisiteComponent(Baseline.Get());
        Source->SetForcedLOD(1); Body->SetForcedLOD(1);
        Source->bEnableUpdateRateOptimizations=false; Body->bEnableUpdateRateOptimizations=false;
        Source->AddTickPrerequisiteComponent(Character->GetCharacterMovement());
        Source->InitAnim(true); Baseline->InitAnim(true); Body->InitAnim(true); BodyBaseline->InitAnim(true);
        if(bNeutralHead)
        {
            if(auto* Actual=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance())){Actual->bEnableMouseDrivenHead=false;}
            if(auto* Reference=Cast<URogue10mFighterCombatStanceAnimInstance>(Baseline->GetAnimInstance())){Reference->bEnableMouseDrivenHead=false;}
        }
        Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        if(!Stance.IsValid() || !Baseline->GetAnimInstance() || !BodyBaseline->GetAnimInstance())
        { Check(false,TEXT("paired_animation_initialization_failed")); Finish(); return; }
        for(const TCHAR* Bone:LowerBones)
        { Check(Source->GetBoneIndex(Bone)!=INDEX_NONE && Body->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("required_lower_probe_missing")); }
        for(const TCHAR* Bone:UpperBones){Check(Source->GetBoneIndex(Bone)!=INDEX_NONE,TEXT("required_upper_probe_missing"));}
        RecordSkeleton(Source.Get(),TEXT("source")); RecordSkeleton(Baseline.Get(),TEXT("baseline"));
        RecordSkeleton(Body.Get(),TEXT("appearance")); RecordSkeleton(BodyBaseline.Get(),TEXT("appearance_baseline"));
        Refill(); PreviousLocation=Character->GetActorLocation();
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bOwnFixed=true;
        FApp::SetFixedDeltaTime(1./30.); FApp::SetUseFixedTimeStep(true);
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
            { if(W==Self->World.Get()){Self->Frame();} });
        UE_LOG(LogRogue10m,Display,TEXT("MOVING_ATTACK TIMELINE frames=1100 fps=30 moving_combo=70/74 straight=110/113 hook=140/168 standing=250 stop_mid=330/334 jump_cancel=400/402 dodge=500 knuckle=610,655,705,760,820/%d capture=%d"),KnuckleChargeRelease,bCapture);
    }
    void RecordSkeleton(USkeletalMeshComponent* Mesh,const TCHAR* Stage)
    {
        const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        for(int32 I=0;I<Ref.GetNum();++I)
        { const int32 Parent=Ref.GetParentIndex(I); Skeletons+=FString::Printf(TEXT("%s,%s,%s,%s,%s"),Stage,*Mesh->GetSkeletalMeshAsset()->GetPathName(),*Mesh->GetAnimInstance()->GetClass()->GetPathName(),*Ref.GetBoneName(I).ToString(),Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString())+Columns(Ref.GetRefBonePose()[I])+TEXT("\n"); }
    }
    void RecordPose(USkeletalMeshComponent* Mesh,const TCHAR* Stage,TMap<FName,FTransform>& Raw)
    {
        const auto Local=Mesh->GetBoneSpaceTransformsView(); const FReferenceSkeleton& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        TArray<FTransform> CS; CS.SetNum(Local.Num()); Raw.Reset();
        for(int32 I=0;I<Local.Num();++I)
        {
            const int32 Parent=Ref.GetParentIndex(I); CS[I]=Parent==INDEX_NONE?Local[I]:Local[I]*CS[Parent];
            const FName Bone=Ref.GetBoneName(I); bool Wanted=false;
            for(const TCHAR* Name:LowerBones){Wanted|=Bone==FName(Name);} for(const TCHAR* Name:UpperBones){Wanted|=Bone==FName(Name);}
            if(!Wanted){continue;}
            Check(!Local[I].ContainsNaN() && !CS[I].ContainsNaN(),TEXT("probe_transform_nonfinite"));
            Check(Local[I].GetRotation().IsNormalized(),TEXT("probe_rotation_not_normalized"));
            Check(Local[I].GetScale3D().Equals(FVector::OneVector,.01),TEXT("probe_local_scale_not_unit"));
            Raw.Add(Bone,CS[I]);
            Poses+=FString::Printf(TEXT("%d,%s,%s,%s"),FrameIndex,Stage,*Bone.ToString(),Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString())+Columns(Local[I])+Columns(CS[I])+TEXT("\n");
        }
        TArray<FName> Names; Mesh->GetAnimInstance()->GetAllCurveNames(Names);
        for(FName Name:Names){Curves+=FString::Printf(TEXT("%d,%s,%s,%.9f\n"),FrameIndex,Stage,*Name.ToString(),Mesh->GetAnimInstance()->GetCurveValue(Name));}
    }
    double LowerError(USkeletalMeshComponent* Actual,USkeletalMeshComponent* Control,double& Translation) const
    {
        double Degrees=0; Translation=0;
        const auto A=Actual->GetBoneSpaceTransformsView(),B=Control->GetBoneSpaceTransformsView();
        for(const TCHAR* Name:LowerBones)
        {
            const int32 Index=Actual->GetBoneIndex(Name); if(!A.IsValidIndex(Index) || !B.IsValidIndex(Index)){return 180.;}
            Degrees=FMath::Max(Degrees,FMath::RadiansToDegrees(A[Index].GetRotation().AngularDistance(B[Index].GetRotation())));
            Translation=FMath::Max(Translation,FVector::Distance(A[Index].GetTranslation(),B[Index].GetTranslation()));
        }
        return Degrees;
    }
    double UpperError() const
    {
        double Degrees=0; const auto A=Source->GetBoneSpaceTransformsView(),B=Baseline->GetBoneSpaceTransformsView();
        for(const TCHAR* Name:UpperBones)
        { const int32 Index=Source->GetBoneIndex(Name); if(A.IsValidIndex(Index)&&B.IsValidIndex(Index)){Degrees=FMath::Max(Degrees,FMath::RadiansToDegrees(A[Index].GetRotation().AngularDistance(B[Index].GetRotation())));} }
        return Degrees;
    }
    bool OriginalMontagePoseError(const UAnimMontage* Montage,double Position,double& Degrees,double& Cm) const
    {
        // At slot weight one and alpha zero, the evaluated local render bones must match the
        // original full-body sequence. A stationary jab need not differ from the guard's legs.
        Degrees=0; Cm=0;
        for(const FSlotAnimationTrack& Track:Montage->SlotAnimTracks)
        {
            for(const FAnimSegment& Segment:Track.AnimTrack.AnimSegments)
            {
                const auto* Sequence=Cast<UAnimSequence>(Segment.GetAnimReference());
                if(!Sequence || Position<Segment.StartPos || Position>Segment.StartPos+Segment.GetLength()){continue;}
                const double ClipTime=Segment.ConvertTrackPosToAnimPos(Position);
                const auto Local=Source->GetBoneSpaceTransformsView(); const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();
                auto Compare=[&](const TCHAR* Name)
                {
                    const int32 Actual=Source->GetBoneIndex(Name),Expected=Ref.FindBoneIndex(Name);
                    if(!Local.IsValidIndex(Actual)||Expected==INDEX_NONE){Degrees=180.;return;}
                    FTransform Pose; Sequence->GetBoneTransform(Pose,FSkeletonPoseBoneIndex(Expected),FAnimExtractContext(ClipTime,false),false);
                    Degrees=FMath::Max(Degrees,FMath::RadiansToDegrees(Local[Actual].GetRotation().AngularDistance(Pose.GetRotation())));
                    Cm=FMath::Max(Cm,FVector::Distance(Local[Actual].GetTranslation(),Pose.GetTranslation()));
                };
                for(const TCHAR* Name:LowerBones){Compare(Name);} for(const TCHAR* Name:UpperBones){Compare(Name);}
                return true;
            }
        }
        return false;
    }
    static double HandError(const TMap<FName,FTransform>& Actual,const TMap<FName,FTransform>& Control)
    {
        double Cm=0;
        for(const TCHAR* Name:{TEXT("hand_l"),TEXT("hand_r")})
        { const auto* A=Actual.Find(FName(Name)); const auto* B=Control.Find(FName(Name)); if(A&&B){Cm=FMath::Max(Cm,FVector::Distance(A->GetLocation(),B->GetLocation()));} }
        return Cm;
    }
    void PollAcceptance()
    {
        const double Now=World->GetTimeSeconds();
        for(int32 I=0;I<Skills.Num();++I)
        {
            const auto* Skill=Skills[I].Get(); const int32 Uses=Progress->GetSkillUseCount(Skill->GetFName());
            Check(Uses>=LastUses[I] && Uses-LastUses[I]<=1,TEXT("skill_use_counter_changed_by_more_than_one"));
            if(Uses!=LastUses[I])
            {
                FAcceptance Event; Event.Skill=I; Event.Time=Now;
                const float Speed=FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f);
                Event.Delay=Skill->HitStartDelaySeconds/FMath::Max(.01f,Speed*Skill->AnimationPlayRate);
                Event.Interval=FMath::Max(.01f,Skill->HitInterval/Speed);
                Event.ExpectedPulses=Skill->HitMode==ERogue10mAttackHitMode::Single?1:FMath::Clamp(Skill->HitCount,1,64);
                Accepted.Add(Event);
                Check(Character->GetCombatComponent()->GetLastAcceptedAttackSkill()==Skill,TEXT("accepted_skill_does_not_match_native_exact_skill"));
                UE_LOG(LogRogue10m,Display,TEXT("MOVING_ATTACK ACCEPT frame=%d skill=%s uses=%d delay=%.6f pulses=%d"),FrameIndex,*Skill->GetName(),Uses-InitialUses[I],Event.Delay,Event.ExpectedPulses);
                LastUses[I]=Uses;
            }
        }
    }
    void RecordDamage()
    {
        const double Health=Target->GetRogueAttributeSet()->GetHealth(),Drop=PreviousHealth-Health,Now=World->GetTimeSeconds();
        if(Drop>.01)
        {
            int32 Best=INDEX_NONE; double BestError=DBL_MAX;
            for(int32 I=0;I<Accepted.Num();++I)
            {
                const auto& Event=Accepted[I]; if(Event.bCancelled || Event.Pulses>=Event.ExpectedPulses){continue;}
                const double Error=FMath::Abs(Now-Event.Time-Event.Delay-Event.Pulses*Event.Interval);
                if(Error<BestError){BestError=Error;Best=I;}
            }
            Check(Best!=INDEX_NONE,TEXT("damage_without_uncancelled_accepted_attack"));
            if(Best!=INDEX_NONE)
            {
                auto& Event=Accepted[Best]; const double Elapsed=Now-Event.Time;
                const double Expected=Event.Delay+Event.Pulses*Event.Interval; const double Error=Elapsed-Expected;
                Check(FMath::Abs(Error)<=1./30.+.015,TEXT("actual_damage_pulse_outside_original_timer_schedule"));
                ++Event.Pulses; if(Event.Pulses==1){Event.FirstError=Error;}
                MaxDamageTimingError=FMath::Max(MaxDamageTimingError,FMath::Abs(Error));
                Damage+=FString::Printf(TEXT("%d,%.9f,%s,%.6f,%d,%.9f,%.9f,%.9f,%d\n"),FrameIndex,Now,*Skills[Event.Skill]->GetPathName(),Drop,Event.Pulses,Elapsed,Expected,Error,Event.bCancelled);
            }
        }
        PreviousHealth=Health;
    }
    void Press(bool Primary)
    { Refill(); Character->GetCombatComponent()->HandleAttackPressed(Primary); PollAcceptance(); }
    void Release(bool Primary)
    { Character->GetCombatComponent()->HandleAttackReleased(Primary); PollAcceptance(); }
    const TCHAR* Stage() const
    {
        if(FrameIndex<30)return TEXT("warm_idle"); if(FrameIndex<70)return TEXT("paired_walk_control");
        if(FrameIndex<140)return TEXT("moving_combo_straight"); if(FrameIndex<220)return TEXT("moving_charge_hook");
        if(FrameIndex<300)return TEXT("standing_attack"); if(FrameIndex<380)return TEXT("stop_mid_attack");
        if(FrameIndex<480)return TEXT("jump_cancel"); if(FrameIndex<560)return TEXT("full_body_dodge");
        if(FrameIndex<930)return TEXT("moving_knuckle_five"); if(FrameIndex<990)return TEXT("weapon_cancel");
        return TEXT("blocking_menu_cancel");
    }
    void Frame()
    {
        if(!Character.IsValid() || !Controller.IsValid() || !Source.IsValid() || !Baseline.IsValid() || !BodyBaseline.IsValid() || !Stance.IsValid() || !Target.IsValid())
        { Check(false,TEXT("required_fixture_object_lost")); Finish(); return; }
        Controller->RotationInput=FRotator::ZeroRotator; Controller->SetControlRotation(LockedControl);
        PollAcceptance(); RecordDamage();
        auto* Movement=Character->GetCharacterMovement(); auto* Anim=Source->GetAnimInstance(); auto* Montage=Anim->GetCurrentActiveMontage();
        const float Alpha=Stance->MovingAttackUpperBodyAlpha,Speed=Movement->Velocity.Size2D();
        const bool Grounded=Movement->IsMovingOnGround(); const double Step=FVector::Dist2D(PreviousLocation,Character->GetActorLocation());
        PreviousLocation=Character->GetActorLocation();
        Check(FMath::IsFinite(Alpha)&&Alpha>=0&&Alpha<=1,TEXT("moving_attack_alpha_not_normalized"));
        Check(!Baseline->GetAnimInstance()->GetCurrentActiveMontage(),TEXT("independent_baseline_has_attack_montage"));
        TMap<FName,FTransform> SourcePose,BasePose,BodyPose,BodyBasePose;
        RecordPose(Source.Get(),TEXT("source"),SourcePose); RecordPose(Baseline.Get(),TEXT("baseline"),BasePose);
        RecordPose(Body.Get(),TEXT("appearance"),BodyPose); RecordPose(BodyBaseline.Get(),TEXT("appearance_baseline"),BodyBasePose);
        double LowerCm=0,BodyLowerCm=0; const double LowerDeg=LowerError(Source.Get(),Baseline.Get(),LowerCm);
        const double BodyLowerDeg=LowerError(Body.Get(),BodyBaseline.Get(),BodyLowerCm);
        const double UpperDeg=UpperError(),HandCm=HandError(SourcePose,BasePose),BodyHandCm=HandError(BodyPose,BodyBasePose);
        if(FrameIndex>=55 && FrameIndex<69)
        {
            ++BaselineSamples; Check(Grounded&&Speed>30&&Step>.1,TEXT("paired_control_is_not_actually_walking"));
            Check(LowerDeg<=1.&&LowerCm<=.15,TEXT("baseline_anim_phase_not_synchronized_before_attack"));
            Check(BodyLowerDeg<=2.,TEXT("retarget_baseline_not_synchronized_before_attack"));
            MaxControlLowerDeg=FMath::Max(MaxControlLowerDeg,LowerDeg);
        }
        int32 ActiveSkill=INDEX_NONE;
        for(int32 I=0;I<Skills.Num();++I){if(Montage==Skills[I]->AttackMontage&&Character->GetCombatComponent()->GetLastAcceptedAttackSkill()==Skills[I].Get()){++MontageSamples[I];ActiveSkill=I;break;}}
        if(Alpha>.98 && ActiveSkill!=INDEX_NONE)
        {
            Check(Grounded&&Speed>10&&Step>.1,TEXT("masked_attack_is_not_actual_grounded_movement"));
            Check(LowerDeg<=1.&&LowerCm<=.15,TEXT("moving_attack_replaced_source_locomotion_lower_pose"));
            Check(BodyLowerDeg<=2.&&BodyLowerCm<=.3,TEXT("moving_attack_replaced_retarget_lower_pose"));
            ++MovingSamples[ActiveSkill]; UpperPeaks[ActiveSkill]=FMath::Max(UpperPeaks[ActiveSkill],UpperDeg);
            MaxLowerDeg=FMath::Max(MaxLowerDeg,LowerDeg); MaxLowerCm=FMath::Max(MaxLowerCm,LowerCm);
            MaxBodyLowerDeg=FMath::Max(MaxBodyLowerDeg,BodyLowerDeg); MaxHandCm=FMath::Max(MaxHandCm,HandCm);
            MaxBodyHandCm=FMath::Max(MaxBodyHandCm,BodyHandCm); MovingDistance+=Step;
            for(const TCHAR* Name:{TEXT("foot_l"),TEXT("foot_r")})
            {
                if(const auto* Pose=SourcePose.Find(FName(Name)))
                {
                    if(const auto* Previous=PreviousMovingFeet.Find(FName(Name)))
                    { FootCycleTravel+=FVector::Distance(Previous->GetLocation(),Pose->GetLocation()); }
                    PreviousMovingFeet.Add(FName(Name),*Pose);
                }
            }
        }
        if(FrameIndex>=251&&FrameIndex<266&&Montage==Skills[0]->AttackMontage)
        {
            ++StandingSamples; Check(Alpha<.001&&Speed<.01,TEXT("standing_attack_did_not_keep_original_full_body_pose"));
            StandingLowerPeak=FMath::Max(StandingLowerPeak,LowerDeg);
            const auto* Instance=Anim->GetInstanceForMontage(Montage);
            if(Instance&&Instance->GetWeight()>.999)
            {
                double Degrees=0,Cm=0;
                Check(OriginalMontagePoseError(Montage,Instance->GetPosition(),Degrees,Cm),TEXT("standing_original_sequence_not_sampled"));
                ++StandingOriginalSamples; MaxStandingOriginalDeg=FMath::Max(MaxStandingOriginalDeg,Degrees);
                Check(Degrees<=1.&&Cm<=.15,TEXT("standing_original_full_body_montage_pose_changed"));
            }
        }
        if(FrameIndex>=340&&FrameIndex<352&&Montage==Skills[0]->AttackMontage)
        { ++StoppedSamples; Check(Alpha<.001&&Speed<.01,TEXT("stopping_did_not_restore_full_body_attack")); }
        if(FrameIndex>=403&&FrameIndex<450&&Movement->IsFalling())
        { ++AirSamples; Check(Alpha<.001,TEXT("jump_retained_moving_attack_mask")); MaxJumpHeight=FMath::Max(MaxJumpHeight,Character->GetActorLocation().Z-JumpStartZ); }
        if(FrameIndex>=501&&FrameIndex<518&&Montage)
        { ++DodgeSamples; Check(Alpha<.001,TEXT("dodge_was_masked_to_upper_body")); DodgeLowerPeak=FMath::Max(DodgeLowerPeak,LowerDeg); }
        if(FrameIndex>=142&&FrameIndex<167)
        { ++ChargeSamples; Check(Character->GetBasicBrawlerComponent()->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Charging,TEXT("moving_charge_did_not_remain_held")); }
        const float MontagePosition=Montage?Anim->Montage_GetPosition(Montage):-1.f;
        const auto* MontageInstance=Montage?Anim->GetInstanceForMontage(Montage):nullptr;
        const double MontageWeight=MontageInstance?MontageInstance->GetWeight():0.;
        if(Montage && Montage==PreviousMontage.Get())
        { Check(MontagePosition+UE_SMALL_NUMBER>=PreviousMontagePosition,TEXT("original_montage_time_went_backward")); }
        PreviousMontage=Montage; PreviousMontagePosition=MontagePosition;
        Frames+=FString::Printf(TEXT("%d,%.9f,%s,%.9f,%.9f,%.9f,%d,%d,%d,%d,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d"),
            FrameIndex,World->GetTimeSeconds(),Stage(),Alpha,Stance->FighterCombatStanceAlpha,Speed,Grounded,Movement->IsFalling(),static_cast<int32>(Character->GetEquippedWeaponType()),Character->GetBasicBrawlerComponent()->GetSnapshot().AcceptedSerial,
            Montage?*Montage->GetPathName():TEXT(""),MontagePosition,MontageWeight,Step,LowerDeg,LowerCm,UpperDeg,HandCm,BodyLowerDeg,BodyHandCm,Anim->NotifyQueue.AnimNotifies.Num(),Baseline->GetAnimInstance()->NotifyQueue.AnimNotifies.Num())+Columns(Character->GetActorTransform())+TEXT("\n");

        const bool Walk=(FrameIndex>=30&&FrameIndex<220)||(FrameIndex>=300&&FrameIndex<334)
            ||(FrameIndex>=380&&FrameIndex<450)||(FrameIndex>=480&&FrameIndex<520)||(FrameIndex>=580&&FrameIndex<920)||(FrameIndex>=930&&FrameIndex<960);
        if(FrameIndex==300||FrameIndex==380||FrameIndex==480||FrameIndex==580){SynchronizeAnimationClocks();}
        if(Walk){Move(.35f);}
        if(FrameIndex==220||FrameIndex==334||FrameIndex==450||FrameIndex==520||FrameIndex==920||FrameIndex==960){Stop();}
        if(FrameIndex==70||FrameIndex==74||FrameIndex==250||FrameIndex==330||FrameIndex==400){Press(true);Release(true);}
        if(FrameIndex==110||FrameIndex==140||FrameIndex==995){Press(false);}
        if(FrameIndex==113||FrameIndex==168||FrameIndex==1021){Release(false);}
        if(FrameIndex==402)
        {
            JumpStartZ=Character->GetActorLocation().Z;
            Check(!Accepted.IsEmpty()&&Accepted.Last().Skill==0&&Accepted.Last().Pulses==0,TEXT("jump_case_not_before_jab_hit"));
            if(!Accepted.IsEmpty()){Accepted.Last().bCancelled=true;}
            Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpStart")),nullptr);
        }
        if(FrameIndex==409){Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoJumpEnd")),nullptr);}
        if(FrameIndex==470){Check(!Movement->IsFalling(),TEXT("jump_never_landed"));}
        if(FrameIndex==500){Refill();Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoDodge")),nullptr);}
        if(FrameIndex==560)
        {
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle); auto* Combat=Character->GetCombatComponent();
            for(int32 I=4;I<Skills.Num();++I){Combat->UnlockAttackSkill(Skills[I].Get());Check(Combat->IsAttackSkillUnlocked(Skills[I].Get()),TEXT("knuckle_unlock_failed"));}
            Check(Combat->AssignSkillToInputSlot(Skills[4].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary_assign_failed"));
            Check(Combat->AssignSkillToInputSlot(Skills[7].Get(),ERogue10mAttackInputSlot::Special),TEXT("knuckle_special_assign_failed"));
            Check(Combat->AssignSkillToInputSlot(Skills[8].Get(),ERogue10mAttackInputSlot::ChargedPrimary),TEXT("knuckle_charged_assign_failed"));
        }
        if(FrameIndex==650){Check(Character->GetCombatComponent()->AssignSkillToInputSlot(Skills[5].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary02_assign_failed"));}
        if(FrameIndex==700){Check(Character->GetCombatComponent()->AssignSkillToInputSlot(Skills[6].Get(),ERogue10mAttackInputSlot::Primary),TEXT("knuckle_primary03_assign_failed"));}
        if(FrameIndex==610||FrameIndex==655||FrameIndex==705||FrameIndex==820||FrameIndex==950){Press(true);}
        if(FrameIndex==612||FrameIndex==657||FrameIndex==707||FrameIndex==KnuckleChargeRelease||FrameIndex==956){Release(true);}
        if(FrameIndex==760){Press(false);} if(FrameIndex==762){Release(false);}
        if(FrameIndex==950){CancelUses=LastUses;}
        if(FrameIndex==955){Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);}
        if(FrameIndex==980){Check(LastUses==CancelUses,TEXT("weapon_switch_released_stale_attack"));Check(Alpha<.001,TEXT("weapon_switch_left_mask_active"));}
        if(FrameIndex==1000){Controller->ToggleInventory();Check(Controller->IsInventoryVisible(),TEXT("blocking_menu_fixture_failed"));}
        if(FrameIndex==1020){Controller->CloseAllBlockingPanels();}
        if(FrameIndex==1030){Check(Alpha<.001&&Character->GetBasicBrawlerComponent()->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("blocking_menu_charge_reappeared"));}
        // The query-only dummy follows the moving attacker. Its placement is a fixture, not a movement oracle.
        Target->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*135.f,false,nullptr,ETeleportType::TeleportPhysics);
        Target->SetActorRotation((Character->GetActorLocation()-Target->GetActorLocation()).Rotation());
        static const TSet<int32> CaptureFrames={62,78,120,178,257,333,344,405,508,620,664,715,773,860};
        if(bCapture&&CaptureFrames.Contains(FrameIndex))
        { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false); }
        if(FrameIndex==1100){Complete();Finish();return;} ++FrameIndex;
    }
    void Complete()
    {
        for(int32 I=0;I<Skills.Num();++I)
        {
            const int32 Expected=I==0?4:1;
            Check(LastUses[I]-InitialUses[I]==Expected,TEXT("skill_acceptance_or_combo_count_changed"));
            Check(MovingSamples[I]>=3,TEXT("skill_not_observed_as_moving_masked_attack"));
            Check(UpperPeaks[I]>3.,TEXT("attack_upper_pose_did_not_differ_from_walking_control"));
            Check(MontageSamples[I]>=3,TEXT("original_montage_not_evaluated"));
            UE_LOG(LogRogue10m,Display,TEXT("MOVING_ATTACK SKILL_RESULT skill=%s uses=%d moving_samples=%d upper_peak_deg=%.6f montage_samples=%d"),*Skills[I]->GetName(),LastUses[I]-InitialUses[I],MovingSamples[I],UpperPeaks[I],MontageSamples[I]);
        }
        int32 Cancelled=0;
        for(const auto& Event:Accepted)
        { Check(Event.Pulses==(Event.bCancelled?0:Event.ExpectedPulses),TEXT("actual_health_damage_pulse_count_changed_or_jump_cancel_failed")); Cancelled+=Event.bCancelled?1:0; }
        Check(Cancelled==1,TEXT("jump_cancelled_hit_case_missing"));
        Check(BaselineSamples>=10&&StandingSamples>=3&&StoppedSamples>=2&&AirSamples>=3&&DodgeSamples>=3&&ChargeSamples>=15,TEXT("required_movement_transition_case_not_observed"));
        Check(MovingDistance>100.,TEXT("moving_attack_actual_actor_displacement_not_observed"));
        Check(FootCycleTravel>10.,TEXT("moving_attack_lower_foot_cycle_not_observed"));
        Check(MaxHandCm>3.&&MaxBodyHandCm>3.,TEXT("source_or_visible_retarget_attack_hand_motion_not_observed"));
        Check(MaxJumpHeight>10.,TEXT("actual_jump_height_not_observed"));
        Check(StandingOriginalSamples>=3,TEXT("standing_original_full_body_pose_samples_missing"));
        Check(DodgeLowerPeak>1.,TEXT("dodge_full_body_lower_motion_not_observed"));
        Check(Character->GetBasicBrawlerComponent()->GetSnapshot().Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("brawler_did_not_return_idle"));
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;} bFinished=true;
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        if(!Output.IsEmpty())
        {
            for(const auto& File:TArray<TPair<FString,FString>>{{TEXT("frames.csv"),Frames},{TEXT("poses.csv"),Poses},{TEXT("curves.csv"),Curves},{TEXT("damage.csv"),Damage},{TEXT("skeletons.csv"),Skeletons}})
            { Check(FFileHelper::SaveStringToFile(File.Value,*FPaths::Combine(Output,File.Key)),TEXT("evidence_csv_write_failed")); }
            const FString Metrics=FString::Printf(TEXT("frames=%d\nassertions=%d\nfailures=%d\nbaseline_samples=%d\nstanding_samples=%d\nstanding_original_pose_samples=%d\nmax_standing_original_pose_error_deg=%.9f\nstopped_samples=%d\nair_samples=%d\ndodge_samples=%d\ncharge_samples=%d\nmax_control_lower_error_deg=%.9f\nmax_moving_lower_error_deg=%.9f\nmax_moving_lower_error_cm=%.9f\nmax_retarget_lower_error_deg=%.9f\nsource_hand_divergence_cm=%.9f\nappearance_hand_divergence_cm=%.9f\nmoving_attack_actor_distance_cm=%.9f\nfoot_cycle_travel_cm=%.9f\njump_height_cm=%.9f\nstanding_lower_peak_diagnostic_deg=%.9f\ndodge_lower_peak_deg=%.9f\nmax_actual_damage_timing_error_s=%.9f\nNOTE=The real pawn moves through DoMove/AddMovementInput and CharacterMovement on a transient collision floor. Paired same-owner source/retarget meshes evaluate the same movement and FootIK without attack montages. Animation clocks are reset together at independent movement case boundaries 300/380/480/580; no reset occurs inside a moving attack. Raw local lower-bone agreement proves lower locomotion preservation; upper divergence proves the original montage contributes. Standing alpha0 and full-weight original sequence pose agreement preserve the authored full-body attack, even when the clip has planted legs. Health drops independently validate timer-driven hit pulses, not AnimNotify-driven damage. CSV notify/curve observations are diagnostic. Numeric pose checks do not certify fine mesh deformation.\n"),
                FrameIndex,Assertions,Failures,BaselineSamples,StandingSamples,StandingOriginalSamples,MaxStandingOriginalDeg,StoppedSamples,AirSamples,DodgeSamples,ChargeSamples,MaxControlLowerDeg,MaxLowerDeg,MaxLowerCm,MaxBodyLowerDeg,MaxHandCm,MaxBodyHandCm,MovingDistance,FootCycleTravel,MaxJumpHeight,StandingLowerPeak,DodgeLowerPeak,MaxDamageTimingError);
            Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        }
        if(bPrepared&&Character.IsValid())
        {
            Character->GetCombatComponent()->CancelCombatVisuals(); Stop();
            Character->SetActorLocation(OriginalLocation,false,nullptr,ETeleportType::TeleportPhysics);
            Character->GetCharacterMovement()->MaxWalkSpeed=OldWalkSpeed; Character->SetCanBeDamaged(OldDamage);
            if(Source.IsValid()){Source->SetForcedLOD(OldSourceLOD);Source->bEnableUpdateRateOptimizations=OldSourceRate;}
            if(Source.IsValid()){if(auto* Actual=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance())){Actual->bEnableMouseDrivenHead=OldHead;}}
            if(Body.IsValid()){Body->SetForcedLOD(OldBodyLOD);Body->bEnableUpdateRateOptimizations=OldBodyRate;}
            if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()!=OldThird){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
            if(Controller.IsValid()){Controller->CloseAllBlockingPanels();Controller->SetControlRotation(OldControl);if(OldInput){Character->EnableInput(Controller.Get());}}
        }
        if(BodyBaseline.IsValid()){BodyBaseline->DestroyComponent();} if(Baseline.IsValid()){Baseline->DestroyComponent();}
        if(Ground.IsValid()){Ground->Destroy();}
        if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        const bool Passed=Failures==0&&FrameIndex==1100;
        UE_LOG(LogRogue10m,Display,TEXT("MOVING_ATTACK METRICS lower_deg=%.9f retarget_lower_deg=%.9f actor_cm=%.6f feet_cm=%.6f hand_cm=%.6f appearance_hand_cm=%.6f damage_error_s=%.6f"),MaxLowerDeg,MaxBodyLowerDeg,MovingDistance,FootCycleTravel,MaxHandCm,MaxBodyHandCm,MaxDamageTimingError);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=MOVING_ATTACK_%s frames=%d assertions=%d failures=%d output=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Assertions,Failures,*Output);
        if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ARogue10mCharacter> Character; TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<ARogue10mPlayerState> Progress; TWeakObjectPtr<URogue10mFighterCombatStanceAnimInstance> Stance;
    TWeakObjectPtr<USkeletalMeshComponent> Source,Body,Baseline,BodyBaseline; TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<AActor> Ground; TWeakObjectPtr<UAnimMontage> PreviousMontage;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills; TArray<int32> InitialUses,LastUses,MovingSamples,MontageSamples,CancelUses;
    TArray<double> UpperPeaks; TArray<FAcceptance> Accepted; TMap<FName,FTransform> PreviousMovingFeet;
    FDelegateHandle Tick,Cleanup; FTimerHandle PrepareTimer; FString Output,Images,Frames,Poses,Curves,Damage,Skeletons;
    FRotator OldControl,LockedControl; FVector OriginalLocation,PreviousLocation;
    int32 FrameIndex=0,Assertions=0,Failures=0,BaselineSamples=0,StandingSamples=0,StandingOriginalSamples=0,StoppedSamples=0,AirSamples=0,DodgeSamples=0,ChargeSamples=0,KnuckleChargeRelease=845;
    int32 OldSourceLOD=0,OldBodyLOD=0; float OldWalkSpeed=600,PreviousMontagePosition=0;
    double PreviousHealth=0,MaxControlLowerDeg=0,MaxLowerDeg=0,MaxLowerCm=0,MaxBodyLowerDeg=0,MaxHandCm=0,MaxBodyHandCm=0;
    double MovingDistance=0,FootCycleTravel=0,JumpStartZ=0,MaxJumpHeight=0,StandingLowerPeak=0,MaxStandingOriginalDeg=0,DodgeLowerPeak=0,MaxDamageTimingError=0,OldDelta=1./30.;
    bool bFinished=false,bPrepared=false,bCapture=false,bOwnFixed=false,OldFixed=false,OldInput=false,OldDamage=true,OldThird=false,OldSourceRate=false,OldBodyRate=false,bNeutralHead=false,OldHead=true;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished())){return;}
    for(const FWorldContext& Context:GEngine->GetWorldContexts())
    { if(UWorld* W=Context.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;} }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestMovingAttack"),
    TEXT("Actual moving upper/lower split, paired locomotion/retarget poses, nine attacks, combo/charge, stop/standing/jump/dodge and timer damage. Requires Rogue10m_Fighter_QA_ save slot. -RogueMovingAttackLabel=name -RogueMovingAttackCapture"),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
