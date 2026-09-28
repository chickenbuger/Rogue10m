// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
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

namespace Rogue10mThirdPersonInspectionTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        bCapture=!FParse::Param(FCommandLine::Get(),TEXT("RogueInspectionNoCapture"));
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get()) { Self->Finish(false); } });
        InWorld->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),4.f,false);
    }
    bool IsFinished() const { return bFinished; }
private:
    void Check(bool bCondition,const TCHAR* Message)
    {
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("INSPECTION FAIL frame=%d %s"),FrameIndex,Message); }
    }
    void Key(EInputEvent Event)
    {
        // This enters PlayerInput and its bound IE_Pressed delegate on the following input tick.
        const bool bAccepted=Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,Event,Event==IE_Pressed ? 1.f : 0.f));
        UE_LOG(LogRogue10m,Display,TEXT("INSPECTION KEY frame=%d event=%d accepted=%d menu=%d"),FrameIndex,static_cast<int32>(Event),bAccepted,Controller->IsInventoryVisible());
    }
    FVector Pivot() const
    {
        return Character->GetCapsuleComponent()->GetComponentLocation()+FVector(0,0,Character->GetAppearanceCameraComponent()->InspectionPivotHeight);
    }
    void Prepare()
    {
        Controller=World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Character=Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
        if (!Character.IsValid() || !Controller.IsValid() || !Controller->PlayerCameraManager)
        { Check(false,TEXT("player_or_camera_manager_missing")); Finish(); return; }
        auto* Settings=Character->GetAppearanceCameraComponent(); auto* Body=Character->GetMesh(); auto* Source=Character->GetAnimationPlaybackMesh();
        if (!Settings || !Body || !Body->GetAnimInstance() || !Source || !Source->GetAnimInstance())
        { Check(false,TEXT("appearance_components_missing")); Finish(); return; }
        Check(Settings->IsAppearanceCameraActive() && !Settings->IsThirdPersonInspectionActive(),TEXT("fresh_default_not_first_person"));
        Check(!Controller->IsInventoryVisible() && !Controller->IsLookInputIgnored() && !Controller->IsMoveInputIgnored() && !Controller->bShowMouseCursor,TEXT("initial_input_blocked"));
        const FRotator ObservedControl=Controller->GetControlRotation();
        // Isolate only this test pawn from the user's live mouse/movement input. PlayerController
        // input stays enabled, so V still travels through InputKey and its real bound delegate.
        OldPawnInputEnabled=Character->InputEnabled(); Character->DisableInput(Controller.Get()); bOwnPawnInput=true;
        Controller->RotationInput=FRotator::ZeroRotator;
        BaseControl=FRotator(-30.f,Character->GetActorRotation().Yaw,0.f);
        Controller->SetControlRotation(BaseControl);
        OldCanDamage=Character->CanBeDamaged(); bPrepared=true;
        UE_LOG(LogRogue10m,Display,TEXT("INSPECTION INPUT_ISOLATION observed_control=%s test_control=%s pawn_input=%d pc_input=%d ignored_look=%d ignored_move=%d"),
            *ObservedControl.ToString(),*BaseControl.ToString(),Character->InputEnabled(),Controller->InputEnabled(),Controller->IsLookInputIgnored(),Controller->IsMoveInputIgnored());
        BodyClass=Body->GetAnimInstance()->GetClass(); SourceClass=Source->GetAnimInstance()->GetClass();
        for (USceneComponent* Child:Body->GetAttachChildren())
        {
            auto* Part=Cast<USkeletalMeshComponent>(Child);
            if (Part && Part!=Character->GetFirstPersonMesh()) { PartOwnerNoSee.Add(Part,Part->bOwnerNoSee); }
        }
        Character->SetCanBeDamaged(false); Character->GetCharacterMovement()->StopMovementImmediately();
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget(); if (auto* AI=It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
        }
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bOwnFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/ThirdPersonInspection"));
        if (bCapture) { IFileManager::Get().MakeDirectory(*Output,true); }
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float)
        { if (W==Self->World.Get()) { Self->Frame(); } });
        UE_LOG(LogRogue10m,Display,TEXT("INSPECTION TIMELINE V=30/120/170/195/220/250 left_right_jab=55/58 obstacle=[100,110) inventory=[140,150) blocked_V=145 frames=280 fps=30 capture=%d cosmetics=%d control=%s"),bCapture,PartOwnerNoSee.Num(),*BaseControl.ToString());
    }
    void SpawnCameraObstacle()
    {
        Obstacle=World->SpawnActor<AActor>();
        if (!Obstacle.IsValid()) { Check(false,TEXT("obstacle_spawn_failed")); return; }
        auto* Box=NewObject<UBoxComponent>(Obstacle.Get());
        Obstacle->AddInstanceComponent(Box); Obstacle->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10,100,100));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);
        Box->RegisterComponent();
        Obstacle->SetActorLocationAndRotation(Pivot()-BaseControl.Vector()*150.f,BaseControl);
        UE_LOG(LogRogue10m,Display,TEXT("INSPECTION OBSTACLE inserted pivot=%s center=%s"),*Pivot().ToCompactString(),*Obstacle->GetActorLocation().ToCompactString());
    }
    void CheckAutomaticView(bool bExpectedThird)
    {
        auto* Settings=Character->GetAppearanceCameraComponent(); auto* Body=Character->GetMesh();
        auto* Camera=Character->GetFirstPersonCameraComponent(); auto* Arms=Character->GetFirstPersonMesh();
        Check(Settings->IsThirdPersonInspectionActive()==bExpectedThird,TEXT("V_mode_not_expected"));
        Check(!Arms || ((!Arms->IsVisible() || Arms->bHiddenInGame) && !Arms->IsComponentTickEnabled()),TEXT("separate_first_person_mesh_enabled"));
        Check(Body->IsVisible() && !Body->bHiddenInGame && !Body->bOwnerNoSee,TEXT("appearance_mesh_hidden"));
        Check(Body->IsBoneHiddenByName(Settings->HeadBone)!=bExpectedThird,TEXT("head_visibility_not_matched_to_mode"));
        for (const auto& Entry:PartOwnerNoSee)
        { if (auto* Part=Entry.Key.Get()) { Check(Part->bOwnerNoSee==(bExpectedThird ? false : Entry.Value),TEXT("cosmetic_owner_visibility_not_restored")); } }
        Check(Body->GetAnimInstance() && Body->GetAnimInstance()->GetClass()==BodyClass.Get(),TEXT("appearance_anim_class_changed"));
        Check(Character->GetAnimationPlaybackMesh()->GetAnimInstance()->GetClass()==SourceClass.Get(),TEXT("source_anim_class_changed"));
        Check(!Character->InputEnabled() && Controller->InputEnabled(),TEXT("test_input_isolation_not_preserved"));
        Check(Controller->GetControlRotation().Equals(BaseControl,.01f),TEXT("inspection_changed_control_aim"));
        FTransform Head; FQuat Motion;
        if (!Settings->ReadAnimatedHead(Head,Motion)) { Check(false,TEXT("raw_head_missing")); return; }
        const FVector HeadLocation=Head.GetLocation()+(Motion*Character->GetActorQuat()).RotateVector(Settings->EyeOffset);
        const FQuat HeadRotation=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();
        const double PositionError=FVector::Distance(Camera->GetComponentLocation(),HeadLocation);
        const double AngleError=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(HeadRotation));
        MaxHeadPositionError=FMath::Max(MaxHeadPositionError,PositionError); MaxHeadAngleError=FMath::Max(MaxHeadAngleError,AngleError);
        Check(PositionError<.05 && AngleError<.02,TEXT("inspection_moved_gameplay_head_camera"));
        // Observe the engine's automatic rendered cache; do not call CalcCamera to repair it.
        const FMinimalViewInfo& View=Controller->PlayerCameraManager->GetCameraCacheView();
        if (bExpectedThird)
        {
            ++ThirdSamples; const double Distance=FVector::Distance(View.Location,Pivot());
            Check(FVector::DotProduct(View.Location-Pivot(),BaseControl.Vector())<-30.f,TEXT("inspection_view_not_behind_player"));
            Check(Distance<=Settings->InspectionDistance+.1f,TEXT("inspection_view_beyond_distance"));
            Check(View.Rotation.Equals(BaseControl,.02f),TEXT("inspection_view_not_control_orientation"));
            Check(FVector::Distance(View.Location,Camera->GetComponentLocation())>30.f,TEXT("inspection_render_camera_not_separate_from_head_trace"));
            if (FrameIndex>=101 && FrameIndex<=110)
            {
                ++ObstacleSamples; Check(Distance<145.f,TEXT("camera_passed_through_blocking_obstacle"));
                MaxObstacleDistance=FMath::Max(MaxObstacleDistance,Distance);
            }
            if (FrameIndex==99) { BeforeObstacleDistance=Distance; }
            if (FrameIndex==112)
            { Check(Distance>BeforeObstacleDistance-1.f,TEXT("camera_did_not_recover_after_obstacle_removed")); }
        }
        else
        {
            ++FirstSamples;
            Check(FVector::Distance(View.Location,HeadLocation)<.05 && FMath::RadiansToDegrees(View.Rotation.Quaternion().AngularDistance(HeadRotation))<.02,TEXT("first_person_cache_not_restored_to_head"));
        }
        if (FrameIndex%15==0)
        { UE_LOG(LogRogue10m,Display,TEXT("INSPECTION SAMPLE frame=%d third=%d rendered=%s head_camera=%s head_hidden=%d"),FrameIndex,bExpectedThird,*View.Location.ToCompactString(),*Camera->GetComponentLocation().ToCompactString(),Body->IsBoneHiddenByName(Settings->HeadBone)); }
    }
    void Frame()
    {
        if (!Character.IsValid() || !Controller.IsValid()) { Check(false,TEXT("player_lost")); Finish(); return; }
        const bool bExpectedThird=(FrameIndex>=31 && FrameIndex<=120) || (FrameIndex>=171 && FrameIndex<=195) || (FrameIndex>=221 && FrameIndex<=250);
        if (FrameIndex>0) { CheckAutomaticView(bExpectedThird); }
        if (FrameIndex==30 || FrameIndex==120 || FrameIndex==170 || FrameIndex==195 || FrameIndex==220 || FrameIndex==250 || FrameIndex==145) { Key(IE_Pressed); }
        if (FrameIndex==31 || FrameIndex==121 || FrameIndex==171 || FrameIndex==196 || FrameIndex==221 || FrameIndex==251 || FrameIndex==146) { Key(IE_Released); }
        if (FrameIndex==55 || FrameIndex==58)
        { Character->GetCombatComponent()->HandleAttackPressed(true); Character->GetCombatComponent()->HandleAttackReleased(true); }
        const auto Snapshot=Character->GetBasicBrawlerComponent()->GetSnapshot();
        if (bExpectedThird && Snapshot.Phase==ERogue10mBasicBrawlerPhase::Attacking)
        {
            // The gameplay attack timer includes recovery after the montage leaves the active map.
            // Count only actual montage samples; a wrong active clip still fails, and each side must have >=3 samples.
            auto* Montage=Character->GetAnimationPlaybackMesh()->GetAnimInstance()->GetCurrentActiveMontage();
            if (!Montage) { ++RecoveryWithoutMontageSamples; }
            if (Montage && Snapshot.Attack==ERogue10mBasicBrawlerAttack::LeftJab)
            { Check(Montage->GetName()==TEXT("AM_BoxingLeftJab_Appearance"),TEXT("inspection_left_jab_not_original_montage")); ++LeftMontageSamples; }
            if (Montage && Snapshot.Attack==ERogue10mBasicBrawlerAttack::RightJab)
            { Check(Montage->GetName()==TEXT("AM_BoxingRightJab_Appearance"),TEXT("inspection_right_jab_not_original_montage")); ++RightMontageSamples; }
        }
        if (FrameIndex==100) { SpawnCameraObstacle(); }
        if (FrameIndex==110 && Obstacle.IsValid()) { Obstacle->Destroy(); Obstacle.Reset(); }
        if (FrameIndex==140) { Controller->ToggleInventory(); Check(Controller->IsInventoryVisible(),TEXT("inventory_did_not_open")); }
        if (FrameIndex==148)
        {
            Check(Controller->IsInventoryVisible(),TEXT("inventory_closed_during_V_test"));
            Check(!Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),TEXT("V_not_blocked_by_inventory"));
            Controller->ToggleInspectionCamera();
            Check(!Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive(),TEXT("direct_handler_not_blocked_by_inventory"));
        }
        if (FrameIndex==150) { Controller->ToggleInventory(); Check(!Controller->IsInventoryVisible(),TEXT("inventory_did_not_close")); }
        if (FrameIndex<280 && bCapture) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("frame_%04d.png"),FrameIndex)),true,false); }
        if (FrameIndex==280)
        {
            Check(FirstSamples>100 && ThirdSamples>100,TEXT("both_camera_modes_not_measured"));
            Check(ObstacleSamples==10 && BeforeObstacleDistance>200,TEXT("obstacle_contraction_not_meaningfully_tested"));
            Check(LeftMontageSamples>=3 && RightMontageSamples>=3 && Snapshot.AcceptedSerial==2,TEXT("inspection_jab_pair_not_complete"));
            UE_LOG(LogRogue10m,Display,TEXT("INSPECTION METRICS first_samples=%d third_samples=%d left_montage=%d right_montage=%d obstacle_samples=%d recovery_without_montage=%d open_distance=%.5f blocked_max_distance=%.5f head_error_cm=%.8f head_error_deg=%.8f"),FirstSamples,ThirdSamples,LeftMontageSamples,RightMontageSamples,ObstacleSamples,RecoveryWithoutMontageSamples,BeforeObstacleDistance,MaxObstacleDistance,MaxHeadPositionError,MaxHeadAngleError);
            Finish(); return;
        }
        ++FrameIndex;
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; } bFinished=true;
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(PrepareTimer); }
        if (Obstacle.IsValid()) { Obstacle->Destroy(); }
        if (bPrepared && Character.IsValid())
        {
            Character->SetCanBeDamaged(OldCanDamage);
            auto* Settings=Character->GetAppearanceCameraComponent();
            if (Settings && Settings->IsThirdPersonInspectionActive()) { Settings->ToggleThirdPersonInspection(); }
        }
        if (bOwnPawnInput && Character.IsValid() && Controller.IsValid())
        {
            if (OldPawnInputEnabled) { Character->EnableInput(Controller.Get()); }
            else { Character->DisableInput(Controller.Get()); }
        }
        if (Controller.IsValid() && Controller->IsInventoryVisible()) { Controller->ToggleInventory(); }
        if (bOwnFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        const bool bPassed=Failures==0 && FrameIndex==280;
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=THIRD_PERSON_INSPECTION_%s frames=%d failures=%d capture=%d"),bPassed ? TEXT("PASSED") : TEXT("FAILED"),FrameIndex,Failures,bCapture);
        if (bExit) { FPlatformMisc::RequestExitWithStatus(false,bPassed ? 0 : 1); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<AActor> Obstacle;
    TWeakObjectPtr<UClass> BodyClass,SourceClass;
    TMap<TWeakObjectPtr<USkeletalMeshComponent>,bool> PartOwnerNoSee;
    FTimerHandle PrepareTimer;
    FDelegateHandle Tick,Cleanup;
    FString Output;
    FRotator BaseControl;
    double OldDelta=1.0/30.0,MaxHeadPositionError=0,MaxHeadAngleError=0,BeforeObstacleDistance=0,MaxObstacleDistance=0;
    int32 FrameIndex=0,Failures=0,FirstSamples=0,ThirdSamples=0,LeftMontageSamples=0,RightMontageSamples=0,ObstacleSamples=0,RecoveryWithoutMontageSamples=0;
    bool bCapture=true,bPrepared=false,bFinished=false,bOwnFixed=false,OldFixed=false,OldCanDamage=true,bOwnPawnInput=false,OldPawnInputEnabled=false;
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
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestThirdPersonInspection"),
    TEXT("Injects V through PlayerController input to verify third-person inspection, original body jabs, first-person restoration, menu rejection and camera collision; captures 280 frames. -RogueInspectionNoCapture skips images."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
