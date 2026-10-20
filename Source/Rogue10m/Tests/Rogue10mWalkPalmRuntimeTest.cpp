// Editor-only integration evidence. Original/corrected clip and lower-body isolation are audited offline.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mStylizedCharacter.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace Rogue10mWalkPalmTest
{
static FString Columns(const FTransform& T)
{const FVector P=T.GetLocation(),S=T.GetScale3D();const FQuat Q=T.GetRotation();return FString::Printf(TEXT(",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f"),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);}
static FString Header(const TCHAR* P)
{FString R;for(const TCHAR* A:{TEXT("tx"),TEXT("ty"),TEXT("tz"),TEXT("qx"),TEXT("qy"),TEXT("qz"),TEXT("qw"),TEXT("sx"),TEXT("sy"),TEXT("sz")}){R+=FString::Printf(TEXT(",%s_%s"),P,A);}return R;}
static bool WorldBone(USkeletalMeshComponent* Mesh,FName Name,FTransform& Out,bool Reference=false)
{
    if(!Mesh||!Mesh->GetSkeletalMeshAsset()){return false;}const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto Local=Mesh->GetBoneSpaceTransformsView();const int32 I=Ref.FindBoneIndex(Name);if(I==INDEX_NONE||(!Reference&&!Local.IsValidIndex(I))){return false;}
    Out=Reference?Ref.GetRefBonePose()[I]:Local[I];
    for(int32 P=Ref.GetParentIndex(I);P!=INDEX_NONE;P=Ref.GetParentIndex(P)){if(!Reference&&!Local.IsValidIndex(P)){return false;}Out*=Reference?Ref.GetRefBonePose()[P]:Local[P];}
    Out*=Mesh->GetComponentTransform();return !Out.ContainsNaN();
}
struct FPalm {FVector W,M,I,P,Thumb,Flex,T,B,N;};
static bool ReadPalm(USkeletalMeshComponent* Mesh,int32 Side,FPalm& O,bool Reference=false)
{
    const TCHAR* S=Side==0?TEXT("l"):TEXT("r");FTransform H,M,I,P,Thumb,Flex;
    if(!WorldBone(Mesh,*FString::Printf(TEXT("hand_%s"),S),H,Reference)||!WorldBone(Mesh,*FString::Printf(TEXT("middle_01_%s"),S),M,Reference)
        ||!WorldBone(Mesh,*FString::Printf(TEXT("index_01_%s"),S),I,Reference)||!WorldBone(Mesh,*FString::Printf(TEXT("pinky_01_%s"),S),P,Reference)
        ||!WorldBone(Mesh,*FString::Printf(TEXT("thumb_01_%s"),S),Thumb,Reference)||!WorldBone(Mesh,*FString::Printf(TEXT("middle_02_%s"),S),Flex,Reference)){return false;}
    O.W=H.GetLocation();O.M=M.GetLocation();O.I=I.GetLocation();O.P=P.GetLocation();O.Thumb=Thumb.GetLocation();O.Flex=Flex.GetLocation();
    O.T=(O.M-O.W).GetSafeNormal();const FVector Across=O.I-O.P;O.B=(Across-O.T*FVector::DotProduct(Across,O.T)).GetSafeNormal();O.N=FVector::CrossProduct(O.T,O.B).GetSafeNormal();
    return O.T.SizeSquared()>.99&&O.B.SizeSquared()>.99&&O.N.SizeSquared()>.99;
}
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* W)
    {
        World=W;FString Slot;if(!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot)||!Slot.StartsWith(TEXT("Rogue10m_Fighter_QA_"))){Check(false,TEXT("isolated_save_slot_required"));Finish();return;}
        FString Label=TEXT("runtime");FParse::Value(FCommandLine::Get(),TEXT("RogueWalkPalmLabel="),Label);Label=FPaths::MakeValidFileName(Label);
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/walk-palm"),Label);Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/WalkPalm"),Label);
        bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueWalkPalmCapture"));bBaseline=FParse::Param(FCommandLine::Get(),TEXT("RogueWalkPalmBaseline"));
        if(IFileManager::Get().FileExists(*FPaths::Combine(Output,TEXT("metrics.txt")))){Check(false,TEXT("fresh_label_required"));Finish();return;}
        IFileManager::Get().MakeDirectory(*Output,true);if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Palms=TEXT("frame,world_s,phase,stable,stage,side,sign,speed,normal_forward,normal_right,normal_up,normal_outward,thumb_palmar_offset_cm,finger_flex_cm,wx,wy,wz,mx,my,mz,ix,iy,iz,px,py,pz,nx,ny,nz,distal_x,distal_y,distal_z,actor_yaw\n");
        Frames=TEXT("frame,world_s,phase,speed_cm_s,requested_speed_cm_s,actual_step_cm,grounded,montage,montage_time,guard_alpha,moving_attack_alpha,view_fixture,eye_z,control_pitch,camera_error_cm")+Header(TEXT("actor"))+Header(TEXT("camera"))+TEXT("\n");
        Poses=TEXT("frame,phase,stage,bone,parent")+Header(TEXT("local"))+Header(TEXT("raw_world"))+TEXT("\n");
        Players=TEXT("frame,phase,node,node_weight,blendspace,input_x,input_y,input_z,filtered_x,filtered_y,filtered_z,asset_time,asset_length,note\n");
        Signs=TEXT("stage,mesh,side,fixed_sign,reference_thumb_palmar_offset_cm,reference_flex_cm\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()){Self->Finish(false);}});
        W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Bootstrap();}),5.f,false);
    }
    bool IsFinished()const{return bFinished;}
private:
    void Check(bool Good,const TCHAR* Why){++Assertions;if(!Good){++Failures;if(Failures<40){UE_LOG(LogRogue10m,Error,TEXT("WALK_PALM FAIL frame=%d %s"),FrameIndex,Why);}}}
    void Bootstrap()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid()){Check(false,TEXT("player_missing"));Finish();return;}
        FString Path;if(!FParse::Value(FCommandLine::Get(),TEXT("RogueWalkPalmCharacterClass="),Path)){Prepare();return;}
        UClass* Class=LoadClass<ARogue10mCharacter>(nullptr,*Path);if(!Class){Check(false,TEXT("appearance_override_class_missing"));Finish();return;}
        OriginalPawn=Character;OriginalPawn->SetActorEnableCollision(false);FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn=World->SpawnActor<ARogue10mCharacter>(Class,OriginalPawn->GetActorTransform(),Params);if(!Pawn){Check(false,TEXT("appearance_spawn_failed"));Finish();return;}
        Controller->Possess(Pawn);Controller->SetViewTarget(Pawn);Character=Pawn;
        if(auto* Stylized=Cast<ARogue10mStylizedCharacter>(Pawn))
        {FRogue10mCharacterProfile Profile;Profile.ProfileId=FGuid::NewGuid();Profile.CharacterName=TEXT("Walk palm QA");Profile.Appearance.Race=Stylized->GetAppearanceRace();Profile.Appearance.Gender=Stylized->GetAppearanceGender();Check(Stylized->ApplyCharacterProfile(Profile),TEXT("matching_profile_apply_failed"));}
        OriginalPawn->SetActorHiddenInGame(true);
        World->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),2.f,false);
    }
    void Move(float Scale)
    {
        struct FParams{float Right=0,Forward=0;}P;P.Forward=Scale;Character->ProcessEvent(Character->FindFunctionChecked(TEXT("DoMove")),&P);
        Character->GetCharacterAnimationComponent()->SetMovementState(!FMath::IsNearlyZero(Scale),false);
    }
    void Stop(){Move(0);Character->GetCharacterMovement()->StopMovementImmediately();}
    void MakeGround()
    {
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
        if(!Ground.IsValid()){Check(false,TEXT("transient_ground_spawn_failed"));return;}
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("WalkPalmGround"),RF_Transient);Ground->AddInstanceComponent(Box);Ground->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10000,10000,20));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
        const double Surface=OldActor.GetLocation().Z+1000.;Ground->SetActorLocation(FVector(OldActor.GetLocation().X,OldActor.GetLocation().Y,Surface-20.));
        Character->SetActorLocation(FVector(OldActor.GetLocation().X,OldActor.GetLocation().Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.),false,nullptr,ETeleportType::TeleportPhysics);
        PreviousLocation=Character->GetActorLocation();
    }
    void Prepare()
    {
        auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();auto* Settings=Character->GetAppearanceCameraComponent();
        if(!Body||!Source||Body==Source||!Body->GetAnimInstance()||!Source->GetAnimInstance()||!Settings||!Controller->PlayerCameraManager)
        {Check(false,TEXT("actual_source_retarget_camera_required"));Finish();return;}
        OldActor=Character->GetActorTransform();PreviousLocation=Character->GetActorLocation();OldControl=Controller->GetControlRotation();OldSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;
        OldDamage=Character->CanBeDamaged();OldThird=Settings->IsThirdPersonInspectionActive();OldEye=Settings->EyeOffset;OldWeapon=Character->GetEquippedWeaponType();bPrepared=true;
        Check(FMath::Abs(FRotator::NormalizeAxis(OldControl.Pitch)+20)<.1,TEXT("initial_pitch_not_preserved_minus20"));Check(OldEye.Equals(FVector(8,0,-3),.01),TEXT("seven_cm_lowered_camera_not_preserved"));
        if(OldThird){Settings->ToggleThirdPersonInspection();}Character->SetCanBeDamaged(false);Controller->CloseAllBlockingPanels();Controller->SetIgnoreLookInput(true);bIgnored=true;
        OldInput=Character->InputEnabled();Character->DisableInput(Controller.Get());
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);Stop();
        MakeGround();
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It){It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}It->GetCharacterMovement()->DisableMovement();It->SetActorEnableCollision(false);}
        for(int32 Stage=0;Stage<2;++Stage)
        {
            auto* Mesh=Stage==0?Source:Body;
            for(int32 Side=0;Side<2;++Side)
            {
                FPalm Ref;if(!ReadPalm(Mesh,Side,Ref,true)){Check(false,TEXT("reference_palm_landmarks_missing_or_degenerate"));Finish();return;}
                const double Thumb=FVector::DotProduct(Ref.Thumb-Ref.M,Ref.N);
                // Fixed source signs independently established from Manny anatomy; body signs use its own rest geometry once.
                FixedSign[Stage][Side]=Stage==0?(Side==0?-1.:1.):(Thumb>=0?1.:-1.);
                const double Palmar=Thumb*FixedSign[Stage][Side],Flex=FVector::DotProduct(Ref.Flex-Ref.M,Ref.N)*FixedSign[Stage][Side];
                Check(Palmar>.02,TEXT("fixed_sign_anatomical_thumb_reference_proof_failed"));
                Signs+=FString::Printf(TEXT("%s,%s,%s,%.0f,%.9f,%.9f\n"),Stage==0?TEXT("source"):TEXT("body"),*Mesh->GetSkeletalMeshAsset()->GetPathName(),Side==0?TEXT("left"):TEXT("right"),FixedSign[Stage][Side],Palmar,Flex);
            }
        }
        if(bCapture&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))){FAssetCompilingManager::Get().FinishAllCompilation();Check(FAssetCompilingManager::Get().GetNumRemainingAssets()==0,TEXT("capture_compilation_incomplete"));}
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./30.);bFixed=true;
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("WALK_PALM START class=%s map=%s baseline=%d body=%s source=%s phases=idle,walk110,walk210,run600,yaw90walk210,moving_jab,stop_idle no_pose_reset=1 ground=transient_floor_1000cm_above_StartHub"),*Character->GetClass()->GetPathName(),*World->GetMapName(),bBaseline,*Body->GetSkeletalMeshAsset()->GetPathName(),*Source->GetSkeletalMeshAsset()->GetPathName());
    }
    const TCHAR* Phase()const
    {if(FrameIndex<30){return TEXT("idle");}if(FrameIndex<120){return TEXT("walk110");}if(FrameIndex<210){return TEXT("walk210");}if(FrameIndex<270){return TEXT("run600");}if(FrameIndex<300){return TEXT("stopped");}if(FrameIndex<400){return TEXT("yaw90walk210");}if(FrameIndex<440){return TEXT("moving_jab");}return TEXT("stop_idle");}
    bool Stable()const{return (FrameIndex>=55&&FrameIndex<115)||(FrameIndex>=145&&FrameIndex<205)||(FrameIndex>=235&&FrameIndex<265)||(FrameIndex>=325&&FrameIndex<395);}
    void Probe(USkeletalMeshComponent* Mesh,int32 Stage)
    {
        const TCHAR* Name=Stage==0?TEXT("source"):TEXT("body");
        for(int32 Side=0;Side<2;++Side)
        {
            FPalm P;if(!ReadPalm(Mesh,Side,P)){Check(false,TEXT("runtime_palm_landmarks_degenerate"));continue;}P.N*=FixedSign[Stage][Side];
            const double Forward=FVector::DotProduct(P.N,Character->GetActorForwardVector()),Right=FVector::DotProduct(P.N,Character->GetActorRightVector()),Up=FVector::DotProduct(P.N,Character->GetActorUpVector()),Outward=Right*(Side==0?-1.:1.);
            const double Thumb=FVector::DotProduct(P.Thumb-P.M,P.N),Flex=FVector::DotProduct(P.Flex-P.M,P.N);
            Check(FMath::Abs(P.N.Size()-1)<.001&&!P.N.ContainsNaN(),TEXT("signed_palm_normal_nonfinite_or_not_unit"));
            if(Stable())
            {
                ++StableSamples[Stage][Side];SumForward[Stage][Side]+=Forward;SumOutward[Stage][Side]+=Outward;
                Check(Thumb>-.5,TEXT("fixed_palm_sign_does_not_match_runtime_thumb_anatomy"));
                // A permissive anti-flip envelope, independent of the new roll. Exact FBX roll matching and
                // changed-bone isolation belong to the frozen offline clip audit, not this runtime aggregate.
                if(!bBaseline){Check(Outward<.6,TEXT("stable_walk_palm_flipped_to_face_outward"));}
            }
            Palms+=FString::Printf(TEXT("%d,%.9f,%s,%d,%s,%s,%.0f,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),FrameIndex,World->GetTimeSeconds(),Phase(),Stable(),Name,Side==0?TEXT("left"):TEXT("right"),FixedSign[Stage][Side],Character->GetCharacterMovement()->Velocity.Size2D(),Forward,Right,Up,Outward,Thumb,Flex,P.W.X,P.W.Y,P.W.Z,P.M.X,P.M.Y,P.M.Z,P.I.X,P.I.Y,P.I.Z,P.P.X,P.P.Y,P.P.Z,P.N.X,P.N.Y,P.N.Z,P.T.X,P.T.Y,P.T.Z,Character->GetActorRotation().Yaw);
        }
        const auto Local=Mesh->GetBoneSpaceTransformsView();const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        for(int32 I=0;I<Local.Num();++I)
        {
            const FString N=Ref.GetBoneName(I).ToString();
            if(!(N==TEXT("root")||N==TEXT("pelvis")||N==TEXT("head")||N.StartsWith(TEXT("spine"))||N.StartsWith(TEXT("hand"))||N.StartsWith(TEXT("upperarm"))||N.StartsWith(TEXT("lowerarm"))||N.StartsWith(TEXT("clavicle"))||N.StartsWith(TEXT("thigh"))||N.StartsWith(TEXT("calf"))||N.StartsWith(TEXT("foot"))||N.StartsWith(TEXT("ball"))||N.StartsWith(TEXT("ik_"))||N.StartsWith(TEXT("middle"))||N.StartsWith(TEXT("index"))||N.StartsWith(TEXT("pinky"))||N.StartsWith(TEXT("thumb")))){continue;}
            FTransform W;if(!WorldBone(Mesh,Ref.GetBoneName(I),W)){Check(false,TEXT("raw_parent_composed_pose_missing"));continue;}
            Check(!Local[I].ContainsNaN()&&Local[I].GetRotation().IsNormalized(),TEXT("evaluated_local_bone_invalid"));
            const int32 Parent=Ref.GetParentIndex(I);Poses+=FString::Printf(TEXT("%d,%s,%s,%s,%s"),FrameIndex,Phase(),Name,*N,Parent==INDEX_NONE?TEXT(""):*Ref.GetBoneName(Parent).ToString())+Columns(Local[I])+Columns(W)+TEXT("\n");
        }
    }
    void RecordPlayers()
    {
        auto* Anim=Character->GetAnimationPlaybackMesh()->GetAnimInstance();
        for(TFieldIterator<FStructProperty> I(Anim->GetClass());I;++I)
        {
            if(!I->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct())){continue;}
            const auto* Node=I->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(Anim);auto* BS=Node->GetBlendSpace();if(!BS){continue;}
            const FVector P=Node->GetPosition(),F=Node->GetFilteredPosition();const double Weight=Node->GetCachedBlendWeight();
            Players+=FString::Printf(TEXT("%d,%s,%s,%.9f,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,highest_cached_sample_is_private_api\n"),FrameIndex,Phase(),*I->GetName(),Weight,*BS->GetPathName(),P.X,P.Y,P.Z,F.X,F.Y,F.Z,Node->GetCurrentAssetTime(),Node->GetCurrentAssetLength());
            if(Stable()&&Weight>.5){++ActiveBlendSpaceSamples;}
        }
    }
    double CameraCheck(bool CheckCache)
    {
        auto* Settings=Character->GetAppearanceCameraComponent();FTransform Head;auto* Mesh=Character->GetMesh();
        if(!WorldBone(Mesh,Settings->HeadBone,Head)){Check(false,TEXT("camera_raw_head_missing"));return 1000;}
        const auto& R=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();const int32 H=R.FindBoneIndex(Settings->HeadBone);FTransform Ref=R.GetRefBonePose()[H];
        for(int32 P=R.GetParentIndex(H);P!=INDEX_NONE;P=R.GetParentIndex(P)){Ref*=R.GetRefBonePose()[P];}
        const FQuat MQ=Mesh->GetComponentQuat();FQuat Motion=(Head.GetRotation()*Ref.GetRotation().Inverse()*MQ.Inverse()).GetNormalized();
        if(Settings->IsBrawlerViewStabilized())
        {if(Motion.W<0){Motion=Motion*(-1.);}FVector Axis;double Angle=0;Motion.ToAxisAndAngle(Axis,Angle);Angle=FMath::Min(Angle*Settings->BrawlerHeadRotationWeight,FMath::DegreesToRadians(static_cast<double>(Settings->BrawlerHeadMaxRotationDegrees)));Motion=Angle>UE_SMALL_NUMBER?FQuat(Axis.GetSafeNormal(),Angle):FQuat::Identity;}
        const FVector Expected=Head.GetLocation()+(Motion*Character->GetActorQuat()).RotateVector(Settings->EyeOffset);const FQuat Q=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
        const auto* Camera=Character->GetFirstPersonCameraComponent();const double Error=FVector::Distance(Camera->GetComponentLocation(),Expected);
        Check(Error<.05&&FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(Q))<.02,TEXT("walk_camera_not_independent_animated_head"));
        if(CheckCache){const auto V=Controller->PlayerCameraManager->GetCameraCacheView();Check(FVector::Distance(V.Location,Expected)<.05&&FMath::RadiansToDegrees(V.Rotation.Quaternion().AngularDistance(Q))<.02,TEXT("automatic_walk_firstperson_cache_not_current"));}
        return Error;
    }
    void Capture(const TCHAR* Label,bool Front,int32 HandSide=INDEX_NONE)
    {
        if(!bCapture){return;}if(Front)
        {
            if(!CaptureCamera.IsValid()){CaptureCamera=World->SpawnActor<ACameraActor>();}if(!CaptureCamera.IsValid()){Check(false,TEXT("front_camera_spawn_failed"));return;}
            auto* Settings=Character->GetAppearanceCameraComponent();if(!Settings->IsThirdPersonInspectionActive()){Settings->ToggleThirdPersonInspection();}
            FVector Target=Character->GetActorLocation()+Character->GetActorUpVector()*15;
            FVector Position=Target+Character->GetActorForwardVector()*260+Character->GetActorUpVector()*50;float Fov=60;
            if(HandSide!=INDEX_NONE)
            {
                FPalm Palm;if(!ReadPalm(Character->GetMesh(),HandSide,Palm)){Check(false,TEXT("hand_closeup_actual_pose_missing"));return;}
                Target=(Palm.W+Palm.M)*.5;
                Position=Target+Character->GetActorForwardVector()*65+Character->GetActorRightVector()*(HandSide==0?-30.:30.)+Character->GetActorUpVector()*15;Fov=40;
            }
            CaptureCamera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());CaptureCamera->GetCameraComponent()->SetFieldOfView(Fov);
            Controller->SetViewTarget(CaptureCamera.Get());Controller->PlayerCameraManager->UpdateCamera(0);bCaptureView=true;
        }
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString(Label)+TEXT(".png")),true,false);
    }
    void Frame()
    {
        if(!Character.IsValid()||!Controller.IsValid()){Check(false,TEXT("runtime_player_lost"));Finish();return;}
        const bool RestoringView=bCaptureView;
        if(bCaptureView){Controller->SetViewTarget(Character.Get());auto* S=Character->GetAppearanceCameraComponent();if(S->IsThirdPersonInspectionActive()){S->ToggleThirdPersonInspection();}FMinimalViewInfo V;Character->CalcCamera(0,V);bCaptureView=false;}
        auto* Movement=Character->GetCharacterMovement();auto* Source=Character->GetAnimationPlaybackMesh();auto* Anim=Source->GetAnimInstance();auto* Stance=Cast<URogue10mFighterCombatStanceAnimInstance>(Anim);
        const double Speed=Movement->Velocity.Size2D(),Step=FVector::Dist2D(PreviousLocation,Character->GetActorLocation());Travel+=Step;PreviousLocation=Character->GetActorLocation();
        if(Stable()){Check(Movement->IsMovingOnGround()&&Speed>30&&Step>.1,TEXT("stable_gait_is_not_real_grounded_actor_movement"));if(FrameIndex<120){++Speed110Samples;}else if(FrameIndex<210){++Speed210Samples;}else if(FrameIndex<270){++Speed600Samples;}else{++YawSamples;}Check(FMath::Abs(Speed-Movement->MaxWalkSpeed)<2,TEXT("stable_actual_speed_does_not_match_requested_phase"));}
        const double CameraError=CameraCheck(!RestoringView);MaxCameraError=FMath::Max(MaxCameraError,CameraError);
        Probe(Source,0);Probe(Character->GetMesh(),1);RecordPlayers();auto* Montage=Anim->GetCurrentActiveMontage();
        if(FrameIndex>=409&&FrameIndex<430&&Montage){++MovingJabSamples;Check(Stance&&Stance->MovingAttackUpperBodyAlpha>.98&&Movement->IsMovingOnGround(),TEXT("moving_jab_upper_body_policy_changed"));}
        Frames+=FString::Printf(TEXT("%d,%.9f,%s,%.6f,%.6f,%.6f,%d,%s,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.9f"),FrameIndex,World->GetTimeSeconds(),Phase(),Speed,Movement->MaxWalkSpeed,Step,Movement->IsMovingOnGround(),Montage?*Montage->GetPathName():TEXT(""),Montage?Anim->Montage_GetPosition(Montage):0,Stance?Stance->FighterCombatStanceAlpha:0,Stance?Stance->MovingAttackUpperBodyAlpha:0,RestoringView,Character->GetAppearanceCameraComponent()->EyeOffset.Z,Controller->GetControlRotation().Pitch,CameraError)+Columns(Character->GetActorTransform())+Columns(Character->GetFirstPersonCameraComponent()->GetComponentTransform())+TEXT("\n");
        if(FrameIndex==25){Capture(TEXT("idle_firstperson"),false);}if(FrameIndex==90){Capture(TEXT("walk110_front"),true);}if(FrameIndex==180){Capture(TEXT("walk210_front"),true);}if(FrameIndex==185){Capture(TEXT("walk210_left_hand_close"),true,0);}if(FrameIndex==190){Capture(TEXT("walk210_right_hand_close"),true,1);}if(FrameIndex==250){Capture(TEXT("run600_front"),true);}if(FrameIndex==360){Capture(TEXT("yaw90_walk210_front"),true);}if(FrameIndex==416){Capture(TEXT("moving_jab_front"),true);}if(FrameIndex==630){Capture(TEXT("stop_idle_firstperson"),false);}
        if(FrameIndex==30||FrameIndex==120||FrameIndex==210||FrameIndex==300)
        {
            Movement->MaxWalkSpeed=FrameIndex==30?110:FrameIndex==210?600:210;
            FRotator R=OldActor.Rotator();if(FrameIndex==210){R.Yaw+=180;}if(FrameIndex==300){R.Yaw+=90;}Character->SetActorRotation(R);FRotator C=OldControl;C.Yaw=R.Yaw;Controller->SetControlRotation(C);
        }
        if((FrameIndex>=30&&FrameIndex<270)||(FrameIndex>=300&&FrameIndex<440)){Move(1);}
        if(FrameIndex==270||FrameIndex==440){Stop();}
        if(FrameIndex==405){auto* A=Character->GetRogueAttributeSet();A->SetStamina(A->GetMaxStamina());Character->GetCombatComponent()->HandleAttackPressed(true);Character->GetCombatComponent()->HandleAttackReleased(true);}
        if(FrameIndex==650)
        {
            Check(Travel>500&&Speed110Samples>=50&&Speed210Samples>=50&&Speed600Samples>=25&&YawSamples>=60,TEXT("required_actual_walk_run_yaw_phases_missing"));Check(ActiveBlendSpaceSamples>30,TEXT("active_blendspace_player_not_observed"));Check(MovingJabSamples>=3,TEXT("moving_jab_transition_not_observed"));Check(Speed<.01&&Character->GetBasicBrawlerComponent()->GetSnapshot().AcceptedSerial==1,TEXT("stopped_or_exactly_one_actual_jab_contract_failed"));
            for(int32 S=0;S<2;++S){for(int32 H=0;H<2;++H){Check(StableSamples[S][H]>150,TEXT("palm_stable_samples_missing"));}}
            bComplete=true;Finish();return;
        }
        ++FrameIndex;
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        if(bPrepared&&Character.IsValid())
        {Stop();Character->GetCombatComponent()->CancelCombatVisuals();Character->GetCharacterMovement()->MaxWalkSpeed=OldSpeed;Character->SetCanBeDamaged(OldDamage);Character->SetEquippedWeaponType(OldWeapon);Character->SetActorTransform(OldActor,false,nullptr,ETeleportType::TeleportPhysics);auto* S=Character->GetAppearanceCameraComponent();S->EyeOffset=OldEye;if(S->IsThirdPersonInspectionActive()!=OldThird){S->ToggleThirdPersonInspection();}if(Controller.IsValid()){Controller->SetViewTarget(Character.Get());Controller->SetControlRotation(OldControl);if(bIgnored){Controller->SetIgnoreLookInput(false);}if(OldInput){Character->EnableInput(Controller.Get());}}}
        if(CaptureCamera.IsValid()){CaptureCamera->Destroy();}if(Ground.IsValid()){Ground->Destroy();}if(OriginalPawn.IsValid()&&Controller.IsValid()){Controller->Possess(OriginalPawn.Get());Controller->SetViewTarget(OriginalPawn.Get());OriginalPawn->SetActorEnableCollision(true);OriginalPawn->SetActorHiddenInGame(false);if(Character.IsValid()&&Character!=OriginalPawn){Character->Destroy();}}
        if(bFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        if(!Output.IsEmpty())
        {
            for(const auto& File:TArray<TPair<FString,FString>>{{TEXT("palms.csv"),Palms},{TEXT("frames.csv"),Frames},{TEXT("poses.csv"),Poses},{TEXT("blendspaces.csv"),Players},{TEXT("palm-signs.csv"),Signs}}){Check(FFileHelper::SaveStringToFile(File.Value,*FPaths::Combine(Output,File.Key)),TEXT("csv_write_failed"));}
            FString Metrics=FString::Printf(TEXT("frames=%d\nassertions=%d\nfailures=%d\nbaseline_report_mode=%d\ntravel_cm=%.9f\nwalk110_samples=%d\nwalk210_samples=%d\nrun600_samples=%d\nyaw90_samples=%d\nmoving_jab_samples=%d\nactive_blendspace_samples=%d\nmax_camera_error_cm=%.9f\n"),FrameIndex,Assertions,Failures,bBaseline,Travel,Speed110Samples,Speed210Samples,Speed600Samples,YawSamples,MovingJabSamples,ActiveBlendSpaceSamples,MaxCameraError);
            for(int32 S=0;S<2;++S){for(int32 H=0;H<2;++H){const int32 N=FMath::Max(1,StableSamples[S][H]);Metrics+=FString::Printf(TEXT("%s_%s_mean_forward=%.9f\n%s_%s_mean_outward=%.9f\n"),S==0?TEXT("source"):TEXT("body"),H==0?TEXT("left"):TEXT("right"),SumForward[S][H]/N,S==0?TEXT("source"):TEXT("body"),H==0?TEXT("left"):TEXT("right"),SumOutward[S][H]/N);}}
            Metrics+=TEXT("NOTE=Actual CharacterMovement on a transient collision floor 1000cm above StartHub, fixed-step observation with no animation/pose reset. Character physical input is disabled; DoMove/AddMovementInput remains active. Source signs L=-1/R=+1; body signs calibrated ONCE from its own reference thumb offset. Normals use wrist/middle/index/pinky base landmarks, independently parent-composed into world then actor axes. Baseline skips only the new anti-outward-flip direction predicate. Exact FBX roll matching, unchanged idle/lower-body/FootIK and modified-track isolation are separate frozen offline audits. Actual BlendSpace node weight/input/filter/time are logged; highest cached sample API is private. Front capture briefly uses a separate camera and third-person visibility, restoring gameplay view on the next observation frame. Two close hand cameras use the actual retargeted wrist/middle midpoint, 65cm in front plus 30cm outward and 15cm upward, FOV40. Existing moving-attack/camera-forward suites cover full regression matrix.\n");
            Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        }
        const bool Passed=bComplete&&Failures==0;UE_LOG(LogRogue10m,Display,TEXT("RESULT=WALK_PALM_%s frames=%d assertions=%d failures=%d output=%s images=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Assertions,Failures,*Output,*Images);if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character,OriginalPawn;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<ACameraActor> CaptureCamera;TWeakObjectPtr<AActor> Ground;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FTransform OldActor;FRotator OldControl;FVector OldEye,PreviousLocation;ERogue10mWeaponType OldWeapon=ERogue10mWeaponType::Unarmed;
    FString Output,Images,Palms,Frames,Poses,Players,Signs;int32 FrameIndex=0,Assertions=0,Failures=0,Speed110Samples=0,Speed210Samples=0,Speed600Samples=0,YawSamples=0,MovingJabSamples=0,ActiveBlendSpaceSamples=0,StableSamples[2][2]={{0,0},{0,0}};
    double FixedSign[2][2]={{-1,1},{-1,1}},SumForward[2][2]={{0,0},{0,0}},SumOutward[2][2]={{0,0},{0,0}},Travel=0,MaxCameraError=0,OldDelta=1./30.;float OldSpeed=600;
    bool bFinished=false,bComplete=false,bPrepared=false,bCapture=false,bBaseline=false,bIgnored=false,bFixed=false,OldFixed=false,OldDamage=true,OldThird=false,OldInput=false,bCaptureView=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished())){return;}for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestWalkPalm"),TEXT("Actual walk110/210, run600 and yaw90 palm geometry, six appearance class overrides, walk/idle head camera and moving jab. Requires isolated Fighter QA slot; -RogueWalkPalmBaseline -RogueWalkPalmCapture -RogueWalkPalmCharacterClass=path -RogueWalkPalmLabel=name"),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
