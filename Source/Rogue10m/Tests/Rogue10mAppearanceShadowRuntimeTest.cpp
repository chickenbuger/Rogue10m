// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DirectionalLight.h"
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
#include "Rogue10mAppearanceShadowComponent.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mAppearanceShadowTest
{
static FTransform RawComponentBone(USkeletalMeshComponent* Mesh,FName Name)
{
    const auto Local=Mesh->GetBoneSpaceTransformsView();
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const int32 Index=Mesh->GetBoneIndex(Name);if(!Local.IsValidIndex(Index)){return FTransform::Identity;}
    FTransform Result=Local[Index];
    for(int32 Parent=Ref.GetParentIndex(Index);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent)){Result*=Local[Parent];}
    return Result;
}
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;bCapture=!FParse::Param(FCommandLine::Get(),TEXT("RogueShadowNoCapture"));
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/appearance-shadow"));
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/AppearanceShadow"));
        IFileManager::Get().MakeDirectory(*Output,true);if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Rows=TEXT("frame,third,enabled,active,body_shadow,proxy_shadow,head_hidden,head_screen_x,head_screen_y,max_pose_position_error,max_pose_rotation_error,serial,attack\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if(W==Self->World.Get()){Self->Finish(false);}});
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished()const{return bFinished;}
private:
    void Check(bool Condition,const TCHAR* Message)
    {if(!Condition){++Failures;if(Failures<30){UE_LOG(LogRogue10m,Error,TEXT("SHADOW FAIL frame=%d %s"),FrameIndex,Message);}}}
    void Prepare()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid() || !Controller.IsValid() || !Controller->PlayerCameraManager){Check(false,TEXT("player_missing"));Finish();return;}
        auto* Settings=Character->GetAppearanceCameraComponent();auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();
        if(!Settings || !Body || !Source || !Body->GetSkeletalMeshAsset() || !Character->GetBasicBrawlerComponent())
        {Check(false,TEXT("appearance_missing"));Finish();return;}
        OldInput=Character->InputEnabled();OldDamage=Character->CanBeDamaged();OldControl=Controller->GetControlRotation();
        OldShadowEnabled=Settings->bEnableFullBodyShadow;OldBodyPause=Body->bPauseAnims;OldSourcePause=Source->bPauseAnims;
        bPrepared=true;Character->DisableInput(Controller.Get());Character->SetCanBeDamaged(false);
        Controller->RotationInput=FRotator::ZeroRotator;Character->GetCharacterMovement()->StopMovementImmediately();
        if(Settings->IsThirdPersonInspectionActive()){Settings->ToggleThirdPersonInspection();}
        Settings->bEnableFullBodyShadow=false;Settings->RefreshAppearanceCamera();NativeBodyShadow=Body->CastShadow;Check(NativeBodyShadow,TEXT("native_body_shadow_required_for_visual_baseline"));
        Shadow=Character->FindComponentByClass<URogue10mAppearanceShadowComponent>();
        if(!Shadow.IsValid()){Check(false,TEXT("shadow_component_missing"));Finish();return;}
        bool bFoundLight=false;
        for(TActorIterator<ADirectionalLight> It(World.Get());It;++It)
        {if(It->GetActorForwardVector().Z<-.01f){LightDirection=It->GetActorForwardVector();bFoundLight=true;break;}}
        Check(bFoundLight,TEXT("downward_directional_light_missing"));
        // Face the ground shadow so the repaired head silhouette lies inside the same frozen view.
        const float Yaw=LightDirection.Rotation().Yaw;
        Character->SetActorRotation(FRotator(0,Yaw,0));LockedControl=FRotator(-50.f,Yaw,0);Controller->SetControlRotation(LockedControl);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AppearanceShadowFloor),false,Character.Get());FHitResult Ground;
        const FVector Start=Character->GetActorLocation();
        Check(World->LineTraceSingleByChannel(Ground,Start,Start-FVector(0,0,1000),ECC_Visibility,Query),TEXT("ground_trace_missing"));
        GroundZ=Ground.ImpactPoint.Z;
        Body->bPauseAnims=true;Source->bPauseAnims=true;
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}It->GetCharacterMovement()->StopMovementImmediately();It->GetCharacterMovement()->DisableMovement();}
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();bOwnFixed=true;FApp::SetFixedDeltaTime(1.0/30.0);FApp::SetUseFixedTimeStep(true);
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("SHADOW TIMELINE frozen=[0,120) baseline=[0,30) proxy=[30,60) V=60/80 disable=100 enable=110 jabs=135/138 straight=220/223 hook=290/326 finish=390 light=%s ground=%.3f control=%s"),*LightDirection.ToString(),GroundZ,*LockedControl.ToString());
    }
    void ValidatePose()
    {
        auto* Body=Character->GetMesh();auto* Proxy=Shadow->GetShadowBodyProxy();
        Check(Proxy!=nullptr,TEXT("active_proxy_missing"));if(!Proxy){return;}
        Check(Proxy->bHiddenInGame && Proxy->bCastHiddenShadow && Proxy->CastShadow,TEXT("proxy_not_hidden_shadow_caster"));
        Check(Proxy->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("proxy_collision_enabled"));
        Check(Proxy->GetSkinnedAsset()==Body->GetSkeletalMeshAsset(),TEXT("proxy_mesh_differs"));
        for(const FName Bone:{FName(TEXT("root")),FName(TEXT("pelvis")),FName(TEXT("head")),FName(TEXT("hand_l")),FName(TEXT("hand_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
        {
            const FTransform Expected=RawComponentBone(Body,Bone);
            const FTransform Actual=Proxy->GetBoneTransformByName(Bone,EBoneSpaces::ComponentSpace);
            const double PositionError=FVector::Distance(Expected.GetLocation(),Actual.GetLocation());
            const double RotationError=FMath::RadiansToDegrees(Expected.GetRotation().AngularDistance(Actual.GetRotation()));
            MaxPositionError=FMath::Max(MaxPositionError,PositionError);MaxRotationError=FMath::Max(MaxRotationError,RotationError);
            Check(PositionError<.05 && RotationError<.05,TEXT("proxy_pose_differs_from_raw_body"));
            Check(Actual.GetScale3D().GetMin()>.5f && Actual.GetScale3D().GetMax()<2.f,TEXT("proxy_bone_scale_collapsed"));
            if(Bone==TEXT("head")){Check(Body->IsBoneHiddenByName(Bone),TEXT("first_person_head_not_hidden"));}
        }
        ++PoseSamples;
    }
    void Key(EInputEvent Event)
    {Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,Event,Event==IE_Pressed?1.f:0.f));}
    void Frame()
    {
        if(!Character.IsValid() || !Controller.IsValid() || !Shadow.IsValid()){Check(false,TEXT("player_or_shadow_lost"));Finish();return;}
        auto* Settings=Character->GetAppearanceCameraComponent();auto* Body=Character->GetMesh();auto* Source=Character->GetAnimationPlaybackMesh();
        Check(Controller->GetControlRotation().Equals(LockedControl,.01f),TEXT("physical_input_changed_view"));Controller->RotationInput=FRotator::ZeroRotator;
        const bool Third=Settings->IsThirdPersonInspectionActive();const bool Enabled=Settings->bEnableFullBodyShadow;const bool Active=Shadow->IsShadowActive();
        if(FrameIndex>0)
        {
            Check(Active==(Enabled && !Third),TEXT("shadow_active_policy_incorrect"));
            Check(Body->CastShadow==(Active?false:NativeBodyShadow),TEXT("native_shadow_not_restored"));
            auto* Arms=Character->GetFirstPersonMesh();Check(!Arms || ((!Arms->IsVisible() || Arms->bHiddenInGame) && !Arms->IsComponentTickEnabled()),TEXT("first_person_mesh_reenabled"));
            if(Active){ValidatePose();}else{auto* Proxy=Shadow->GetShadowProxyForSource(Body);Check(!Proxy || !Proxy->CastShadow,TEXT("inactive_proxy_still_casts"));}
            if(FrameIndex>=61 && FrameIndex<=80){Check(Third && !Body->IsBoneHiddenByName(TEXT("head")),TEXT("V_third_person_head_or_mode_incorrect"));++ThirdSamples;}
            if(FrameIndex>=81){Check(!Third,TEXT("V_first_person_not_restored"));}
        }
        const FTransform Head=RawComponentBone(Body,TEXT("head"))*Body->GetComponentTransform();
        const FVector GroundHead=Head.GetLocation()+LightDirection*((GroundZ-Head.GetLocation().Z)/LightDirection.Z);
        FVector2D Screen(-1,-1);Controller->ProjectWorldLocationToScreen(GroundHead,Screen);
        if(FrameIndex==20)
        {
            FrozenHead=Head;FrozenView=Controller->PlayerCameraManager->GetCameraCacheView();BaselineScreen=Screen;
            int32 Width=0,Height=0;Controller->GetViewportSize(Width,Height);
            Check(Screen.X>25 && Screen.X<Width-25 && Screen.Y>25 && Screen.Y<Height-25,TEXT("head_shadow_projection_outside_view"));
            UE_LOG(LogRogue10m,Display,TEXT("SHADOW BASELINE screen=%s world=%s view=%s"),*Screen.ToString(),*GroundHead.ToString(),*FrozenView.Location.ToString());
        }
        if(FrameIndex==50)
        {
            const auto& View=Controller->PlayerCameraManager->GetCameraCacheView();
            Check(Head.Equals(FrozenHead,.001f),TEXT("A_B_body_pose_not_identical"));
            Check(View.Location.Equals(FrozenView.Location,.001f) && View.Rotation.Equals(FrozenView.Rotation,.001f),TEXT("A_B_camera_not_identical"));
            Check(Screen.Equals(BaselineScreen,.01f),TEXT("A_B_head_projection_changed"));
            UE_LOG(LogRogue10m,Display,TEXT("SHADOW AFTER screen=%s world=%s view=%s"),*Screen.ToString(),*GroundHead.ToString(),*View.Location.ToString());
        }
        const auto Snapshot=Character->GetBasicBrawlerComponent()->GetSnapshot();
        if(Active && Snapshot.Phase==ERogue10mBasicBrawlerPhase::Attacking){SeenAttacks.Add(static_cast<int32>(Snapshot.Attack));++AttackPoseSamples;}
        auto* Proxy=Shadow->GetShadowProxyForSource(Body);
        Rows+=FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.9f,%.9f,%d,%d\n"),FrameIndex,Third,Enabled,Active,Body->CastShadow,Proxy?Proxy->CastShadow:false,Body->IsBoneHiddenByName(TEXT("head")),Screen.X,Screen.Y,MaxPositionError,MaxRotationError,Snapshot.AcceptedSerial,static_cast<int32>(Snapshot.Attack));
        if(FrameIndex==30){Settings->bEnableFullBodyShadow=true;}
        if(FrameIndex==60 || FrameIndex==80){Key(IE_Pressed);}if(FrameIndex==61 || FrameIndex==81){Key(IE_Released);}
        if(FrameIndex==100){Settings->bEnableFullBodyShadow=false;}if(FrameIndex==110){Settings->bEnableFullBodyShadow=true;}
        if(FrameIndex==120){Body->bPauseAnims=OldBodyPause;Source->bPauseAnims=OldSourcePause;}
        auto* Combat=Character->GetCombatComponent();
        if(FrameIndex==135 || FrameIndex==138){Combat->HandleAttackPressed(true);Combat->HandleAttackReleased(true);}
        if(FrameIndex==220 || FrameIndex==290){Combat->HandleAttackPressed(false);}if(FrameIndex==223 || FrameIndex==326){Combat->HandleAttackReleased(false);}
        static const TSet<int32> ExtraImages={60,61,79,80,81,99,100,101,109,110,111,130,142,148,157,175,230,240,334,344,360,380};
        if(bCapture && (FrameIndex<60 || ExtraImages.Contains(FrameIndex)))
        {FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false);}
        if(FrameIndex==390)
        {
            Check(ThirdSamples==20 && PoseSamples>200 && AttackPoseSamples>30,TEXT("insufficient_shadow_samples"));
            for(auto Attack:{ERogue10mBasicBrawlerAttack::LeftJab,ERogue10mBasicBrawlerAttack::RightJab,ERogue10mBasicBrawlerAttack::RightStraight,ERogue10mBasicBrawlerAttack::RightHook})
            {Check(SeenAttacks.Contains(static_cast<int32>(Attack)),TEXT("attack_pose_not_sampled"));}
            Check(Snapshot.AcceptedSerial==4,TEXT("not_four_attacks"));Finish();return;
        }
        ++FrameIndex;
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        Check(FFileHelper::SaveStringToFile(Rows,*FPaths::Combine(Output,TEXT("frames.csv"))),TEXT("csv_write_failed"));
        if(bPrepared && Character.IsValid())
        {
            auto* Settings=Character->GetAppearanceCameraComponent();Settings->bEnableFullBodyShadow=OldShadowEnabled;
            if(Settings->IsThirdPersonInspectionActive()){Settings->ToggleThirdPersonInspection();}Settings->RefreshAppearanceCamera();
            Character->GetMesh()->bPauseAnims=OldBodyPause;Character->GetAnimationPlaybackMesh()->bPauseAnims=OldSourcePause;Character->SetCanBeDamaged(OldDamage);
            if(Controller.IsValid()){Controller->SetControlRotation(OldControl);if(OldInput){Character->EnableInput(Controller.Get());}}
        }
        if(bOwnFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        const bool Passed=Failures==0 && FrameIndex==390;
        const FString Result=FString::Printf(TEXT("RESULT=APPEARANCE_SHADOW_%s frames=%d failures=%d pose_samples=%d attack_samples=%d third_samples=%d max_position_error_cm=%.9f max_rotation_error_deg=%.9f baseline_head_screen=%.4f,%.4f\n"),Passed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Failures,PoseSamples,AttackPoseSamples,ThirdSamples,MaxPositionError,MaxRotationError,BaselineScreen.X,BaselineScreen.Y);
        FFileHelper::SaveStringToFile(Result,*FPaths::Combine(Output,TEXT("result.txt")));UE_LOG(LogRogue10m,Display,TEXT("%s"),*Result);
        if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;TWeakObjectPtr<URogue10mAppearanceShadowComponent> Shadow;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Rows;FRotator OldControl,LockedControl;
    FVector LightDirection=FVector(.5,.5,-1).GetSafeNormal();FVector2D BaselineScreen;FTransform FrozenHead;FMinimalViewInfo FrozenView;
    TSet<int32> SeenAttacks;double GroundZ=0,MaxPositionError=0,MaxRotationError=0,OldDelta=1./30.;
    int32 FrameIndex=0,Failures=0,PoseSamples=0,AttackPoseSamples=0,ThirdSamples=0;
    bool bFinished=false,bPrepared=false,bCapture=true,bOwnFixed=false,OldFixed=false,OldInput=false,OldDamage=true,OldShadowEnabled=true,OldBodyPause=false,OldSourcePause=false,NativeBodyShadow=true;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine || GIsEditor || (Active.IsValid() && !Active->IsFinished())){return;}
    for(const FWorldContext& Context:GEngine->GetWorldContexts())
    {if(UWorld* W=Context.World();W && W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestAppearanceShadow"),TEXT("Compares frozen head-shadow off/on, validates raw-pose shadow proxy and V restoration, then four attacks. -RogueShadowNoCapture skips images."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
