// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
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
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// This is a diagnostic recorder, not a biological correctness oracle. A capture PASS means
// all four attacks and finite raw poses were recorded; rotation/bend-plane/foot-slip analysis
// and visual comparison remain necessary. Never modify a pose or damp the camera here.
namespace Rogue10mAnimationStabilityTest
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
        World=InWorld; bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueStabilityCapture"));
        bLocomotion=FParse::Param(FCommandLine::Get(),TEXT("RogueStabilityLocomotion")); FrameLimit=bLocomotion?450:330;
        FString Label=TEXT("capture"); FParse::Value(FCommandLine::Get(),TEXT("RogueStabilityLabel="),Label);
        Label=FPaths::MakeValidFileName(Label); if (Label.IsEmpty()) { Label=TEXT("capture"); }
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/animation-stability"),Label);
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/AnimationStability"),Label);
        IFileManager::Get().MakeDirectory(*Output,true); if (bCapture) { IFileManager::Get().MakeDirectory(*Images,true); }
        Poses=TEXT("frame,time_s,stage,bone,parent,hidden,local_length,reference_length,length_ratio,step_cm,step_deg")+TransformHeader(TEXT("local"))+TransformHeader(TEXT("raw_cs"))+TransformHeader(TEXT("rendered_cs"))+TEXT("\n");
        Frames=TEXT("frame,time_s,world_s,phase,attack,serial,attack_elapsed,charge,third,montage,montage_s,clip,clip_s")+
            TransformHeader(TEXT("source_world"))+TransformHeader(TEXT("appearance_world"))+TransformHeader(TEXT("camera"))+TransformHeader(TEXT("view"))+TEXT(",control_pitch,control_yaw,control_roll,capsule_x,capsule_y,capsule_z,is_falling\n");
        Skeletons=TEXT("stage,mesh,anim_class,bone,parent")+TransformHeader(TEXT("reference_local"))+TEXT("\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; if (Failures<30) { UE_LOG(LogRogue10m,Error,TEXT("STABILITY FAIL frame=%d %s"),FrameIndex,Message); } }
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
        OldControl=Controller->GetControlRotation(); Controller->RotationInput=FRotator::ZeroRotator;
        Controller->SetControlRotation(FRotator(-20.f,Character->GetActorRotation().Yaw,0.f));
        OldDamage=Character->CanBeDamaged();Character->SetCanBeDamaged(false);Character->GetCharacterMovement()->StopMovementImmediately();
        if (Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()) { Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection(); }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}
            It->GetCharacterMovement()->StopMovementImmediately();It->GetCharacterMovement()->DisableMovement();
        }
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bOwnFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0);FApp::SetUseFixedTimeStep(true);
        RecordSkeleton(Character->GetAnimationPlaybackMesh(),TEXT("source"));RecordSkeleton(Character->GetMesh(),TEXT("appearance"));
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("STABILITY TIMELINE frames=%d fps=30 V=30/115/125/315 jabs=45/48 straight=135/138 charge=210/246 walk=[330,360) jump=380/386 locomotion=%d capture=%d output=%s"),FrameLimit,bLocomotion,bCapture,*Output);
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
        const FMinimalViewInfo& View=Controller->PlayerCameraManager->GetCameraCacheView();const FRotator Control=Controller->GetControlRotation();
        Frames+=FString::Printf(TEXT("%d,%.9f,%.9f,%d,%d,%d,%.9f,%.9f,%d,%s,%.9f,%s,%.9f"),FrameIndex,FrameIndex/30.0,World->GetTimeSeconds(),
            static_cast<int32>(Snapshot.Phase),static_cast<int32>(Snapshot.Attack),Snapshot.AcceptedSerial,Snapshot.AttackElapsed,Snapshot.ChargeAlpha,
            Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),Montage?*Montage->GetPathName():TEXT(""),MontageTime,*Clip,ClipTime)+
            TransformColumns(Source->GetComponentTransform())+TransformColumns(Body->GetComponentTransform())+
            TransformColumns(Character->GetFirstPersonCameraComponent()->GetComponentTransform())+TransformColumns(FTransform(View.Rotation,View.Location))+
            FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d\n"),Control.Pitch,Control.Yaw,Control.Roll,
                Character->GetActorLocation().X,Character->GetActorLocation().Y,Character->GetActorLocation().Z,Character->GetCharacterMovement()->IsFalling());
        RecordPose(Source,TEXT("source"));RecordPose(Body,TEXT("appearance"));
        if(FrameIndex==30 || FrameIndex==115 || FrameIndex==125 || FrameIndex==315)
        {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Pressed,1.f));}
        if(FrameIndex==31 || FrameIndex==116 || FrameIndex==126 || FrameIndex==316)
        {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Released,0.f));}
        auto* Combat=Character->GetCombatComponent();
        if(FrameIndex==45 || FrameIndex==48){Combat->HandleAttackPressed(true);Combat->HandleAttackReleased(true);}
        if(FrameIndex==135 || FrameIndex==210){Combat->HandleAttackPressed(false);}
        if(FrameIndex==138 || FrameIndex==246){Combat->HandleAttackReleased(false);}
        if(bLocomotion)
        {
            if(FrameIndex==330){WalkStart=Character->GetActorLocation();}
            if(FrameIndex>=330 && FrameIndex<360){Character->AddMovementInput(-Character->GetActorForwardVector(),.35f,true);}
            if(FrameIndex>=330 && FrameIndex<=360){MaxWalkDistance=FMath::Max(MaxWalkDistance,static_cast<double>(FVector::Dist2D(WalkStart,Character->GetActorLocation())));}
            if(FrameIndex==360){Character->GetCharacterMovement()->StopMovementImmediately();}
            if(FrameIndex==380){JumpStartZ=Character->GetActorLocation().Z;Character->Jump();}
            if(FrameIndex==386){Character->StopJumping();}
            if(FrameIndex>=380){MaxJumpHeight=FMath::Max(MaxJumpHeight,static_cast<double>(Character->GetActorLocation().Z-JumpStartZ));}
        }
        if(bCapture && FrameIndex<FrameLimit){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false);}
        if(FrameIndex==FrameLimit)
        {
            for(auto Attack:{ERogue10mBasicBrawlerAttack::LeftJab,ERogue10mBasicBrawlerAttack::RightJab,ERogue10mBasicBrawlerAttack::RightStraight,ERogue10mBasicBrawlerAttack::RightHook})
            {Check(SeenAttacks.Contains(static_cast<int32>(Attack)),TEXT("attack_missing_from_capture"));}
            Check(Snapshot.AcceptedSerial==4,TEXT("not_exactly_four_attacks"));
            if(bLocomotion)
            {
                Check(MaxWalkDistance>5.f,TEXT("locomotion_walk_not_observed"));
                Check(MaxJumpHeight>10.f,TEXT("locomotion_jump_not_observed"));
                Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("locomotion_did_not_land"));
                Check(Snapshot.Phase==ERogue10mBasicBrawlerPhase::Idle,TEXT("locomotion_attack_did_not_return_idle"));
                Check(!Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),TEXT("locomotion_did_not_restore_first_person"));
            }
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
        Check(FFileHelper::SaveStringToFile(Skeletons,*FPaths::Combine(Output,TEXT("skeletons.csv"))),TEXT("skeleton_csv_write_failed"));
        const FString Metrics=FString::Printf(TEXT("frames=%d\nfailures=%d\nmax_step_cm=%.9f\nmax_step_deg=%.9f\nfast_rotation_samples=%d\nlength_outlier_samples=%d\nlocomotion=%d\nwalk_distance_cm=%.9f\njump_height_cm=%.9f\nNOTE: capture PASS does not establish anatomically correct animation. Inspect source/appearance differential, bend planes, foot slip and visuals.\n"),FrameIndex,Failures,MaxStepCm,MaxStepDeg,FastRotationSamples,LengthOutlierSamples,bLocomotion,MaxWalkDistance,MaxJumpHeight);
        Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        if(bPrepared && Character.IsValid())
        {
            Character->SetCanBeDamaged(OldDamage);
            if(Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()){Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection();}
            if(Controller.IsValid()){if(OldInput){Character->EnableInput(Controller.Get());}Controller->SetControlRotation(OldControl);}
        }
        if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        const bool bPassed=Failures==0 && FrameIndex==FrameLimit;
        UE_LOG(LogRogue10m,Display,TEXT("STABILITY METRICS max_step_cm=%.6f max_step_deg=%.6f fast_rotation_samples=%d length_outlier_samples=%d walk_cm=%.6f jump_cm=%.6f"),MaxStepCm,MaxStepDeg,FastRotationSamples,LengthOutlierSamples,MaxWalkDistance,MaxJumpHeight);
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=ANIMATION_STABILITY_CAPTURE_%s frames=%d failures=%d output=%s"),bPassed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Failures,*Output);
        if(bExit){FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Poses,Frames,Skeletons;FRotator OldControl;
    TMap<FString,FTransform> Previous;TSet<int32> SeenAttacks;
    int32 FrameIndex=0,FrameLimit=330,Failures=0,FastRotationSamples=0,LengthOutlierSamples=0;
    FVector WalkStart; double JumpStartZ=0,MaxWalkDistance=0,MaxJumpHeight=0;
    double OldDelta=1.0/30.0,MaxStepCm=0,MaxStepDeg=0;
    bool bFinished=false,bCapture=false,bLocomotion=false,bPrepared=false,OldInput=false,OldDamage=true,bOwnFixed=false,OldFixed=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine || GIsEditor || (Active.IsValid() && !Active->IsFinished())){return;}
    for(const FWorldContext& Context:GEngine->GetWorldContexts())
    {if(UWorld* W=Context.World();W && W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestAnimationStability"),
    TEXT("Records raw source/appearance poses and view for 330 frames. -RogueStabilityLocomotion adds walk/jump through frame 450. -RogueStabilityLabel=before -RogueStabilityCapture enables screenshots."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
