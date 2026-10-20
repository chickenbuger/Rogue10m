// Editor-only QA of the three production StoneFist bindings. No asset mutation.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
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
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"

namespace Rogue10mStoneFistAnimationTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* W)
    {
        World=W;
        FString Slot;
        if (!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot) || !Slot.StartsWith(TEXT("Rogue10m_Fighter_QA_")))
        { Finish(false,TEXT("isolated_slot_required")); return; }
        FParse::Value(FCommandLine::Get(),TEXT("RogueStoneFistLabel="),Label);
        Label=FPaths::MakeValidFileName(Label);
        W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()] { Self->Prepare(); }),3.f,false);
    }
private:
    void Check(bool Good,const FString& Why)
    {
        ++Assertions;
        Report+=FString::Printf(TEXT("%s %s\n"),Good?TEXT("PASS"):TEXT("FAIL"),*Why);
        if (!Good) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("STONE_FIST FAIL %s"),*Why); }
    }
    void Key(bool Down)
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F6,Down?IE_Pressed:IE_Released,Down?1.f:0.f));
    }
    int32 Uses() const { return Progress->GetSkillUseCount(Skills[Case]->GetFName()); }
    void Prepare()
    {
        Controller=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if (!Character.IsValid() || !Controller.IsValid()) { Finish(false,TEXT("player_missing")); return; }
        Combat=Character->GetCombatComponent(); Progress=Character->GetPlayerState<ARogue10mPlayerState>();
        Bindings=Controller->GetSkillBindingComponent();
        if (!Combat.IsValid() || !Progress.IsValid() || !Bindings.IsValid() || !Character->GetFirstPersonCameraComponent())
        { Finish(false,TEXT("components_missing")); return; }
        Controller->CloseAllBlockingPanels(); Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
        Character->SetCanBeDamaged(false); Character->GetCharacterMovement()->StopMovementImmediately();
        Controller->SetIgnoreLookInput(true);
        LockedControl=FRotator(-20.f,Character->GetActorRotation().Yaw,0.f);
        CharacterPosition=Character->GetActorLocation();
        if (Character->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive())
        { Character->GetAppearanceCameraComponent()->ToggleThirdPersonInspection(); }
        Combat->UnlockAllActiveTreeForTesting();
        for (const TCHAR* Path : {
            TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Jab"),
            TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight"),
            TEXT("/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave")})
        {
            auto* Skill=LoadObject<URogue10mAttackSkillData>(nullptr,Path);
            if (!Skill || !Skill->AttackMontage || !Combat->IsAttackSkillUnlocked(Skill))
            { Finish(false,TEXT("production_skill_missing")); return; }
            Skills.Add(Skill);
        }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            It->ClearAITarget(); if (auto* AI=It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
            if (!Target.IsValid())
            {
                Target=*It;
                if (auto* Regen=It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
                It->GetRogueAttributeSet()->SetMaxHealth(10000); It->GetRogueAttributeSet()->SetHealth(10000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
                It->SetActorLocation(CharacterPosition+Character->GetActorForwardVector()*135.f,false,nullptr,ETeleportType::TeleportPhysics);
            }
            else { It->SetActorEnableCollision(false); }
        }
        if (!Target.IsValid()) { Finish(false,TEXT("target_missing")); return; }
        OldFixed=FApp::UseFixedTimeStep(); OldDelta=FApp::GetFixedDeltaTime(); bFixed=true;
        FApp::SetFixedDeltaTime(1.0/30.0); FApp::SetUseFixedTimeStep(true);
        Csv=TEXT("frame,skill,accepted,pulse,health_drop,elapsed_s,expected_delay_s,accepted_montage_s,montage,montage_s,weight,rate,control_error_deg,camera_rotation_deg,hand_x,hand_y,hand_z,target_x,target_y,target_z\n");
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float) {
            if (W==Self->World.Get()) { Self->Frame(); }
        });
        BeginCase();
    }
    void BeginCase()
    {
        auto* Skill=Skills[Case].Get();
        FText Error; Check(Bindings->SetSkillKey(Skill,EKeys::F6,Error),TEXT("bind_")+Skill->GetName());
        Before=Uses(); PressFrame=FrameIndex+9;
        const bool Charged=Skill->InputSlot==ERogue10mAttackInputSlot::ChargedSpecial || Skill->InputSlot==ERogue10mAttackInputSlot::ChargedPrimary;
        ReleaseFrame=PressFrame+(Charged?FMath::CeilToInt((Skill->ChargeSeconds+.15f)*30.f):3);
        AcceptedFrame=-1; AcceptedTime=-1; AcceptedPosition=0; DamageCount=0; MontageSamples=0;
        FirstPosition=LastPosition=0; FirstDelayError=0; HitPositionError=0; HitRawPositionError=0;
        Target->GetRogueAttributeSet()->SetHealth(10000); PreviousHealth=10000;
        auto* A=Character->GetRogueAttributeSet();
        A->SetStamina(A->GetMaxStamina()); A->SetMana(A->GetMaxMana()); A->SetIdentity(A->GetMaxIdentity());
    }
    void Frame()
    {
        if (bFinished || !Character.IsValid() || !Target.IsValid()) { return; }
        ++FrameIndex;
        Controller->RotationInput=FRotator::ZeroRotator; Controller->SetControlRotation(LockedControl);
        auto* Skill=Skills[Case].Get();
        if (FrameIndex==PressFrame) { Key(true); }
        if (FrameIndex==ReleaseFrame) { Key(false); }
        const int32 Delta=Uses()-Before;
        const double Now=World->GetTimeSeconds();
        auto* Mesh=Character->GetAnimationPlaybackMesh(); auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
        auto* Instance=Anim?Anim->GetInstanceForMontage(Skill->AttackMontage):nullptr;
        const double Position=Instance?Instance->GetPosition():0;
        const double Weight=Instance?Instance->GetWeight():0;
        if (Delta==1 && AcceptedFrame<0)
        {
            AcceptedFrame=FrameIndex; AcceptedTime=Now; AcceptedPosition=Position;
            Rate=FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f)*Skill->AnimationPlayRate;
            ExpectedDelay=Skill->HitStartDelaySeconds/Rate;
            UE_LOG(LogRogue10m,Display,TEXT("STONE_FIST ACCEPT skill=%s frame=%d rate=%.6f expected_delay=%.6f"),*Skill->GetName(),FrameIndex,Rate,ExpectedDelay);
        }
        if (Delta==1 && Anim && Anim->Montage_IsActive(Skill->AttackMontage) && Weight>0)
        {
            if (MontageSamples==0) { FirstPosition=Position; }
            LastPosition=FMath::Max(LastPosition,Position); ++MontageSamples;
        }
        const double Health=Target->GetRogueAttributeSet()->GetHealth(); const double Drop=PreviousHealth-Health;
        PreviousHealth=Health;
        if (Drop>.01)
        {
            ++DamageCount; Check(AcceptedFrame>=0,TEXT("damage_after_accept_")+Skill->GetName());
            if (DamageCount==1)
            {
                FirstDelayError=Now-AcceptedTime-ExpectedDelay;
                // Both clocks are sampled in OnWorldPostActorTick. The montage has
                // already advanced in the acceptance frame; measure its real start
                // position rather than comparing it to an assumed position zero.
                HitRawPositionError=Position-Skill->HitStartDelaySeconds;
                HitPositionError=(Position-AcceptedPosition)-Skill->HitStartDelaySeconds;
                Check(FMath::Abs(FirstDelayError)<=1.0/30.0+.001,TEXT("first_hit_time_")+Skill->GetName());
                Check(Instance && Weight>0 && FMath::Abs(HitPositionError)<=Rate/30.0+.001,TEXT("hit_montage_pose_time_")+Skill->GetName());
            }
            UE_LOG(LogRogue10m,Display,TEXT("STONE_FIST DAMAGE skill=%s pulse=%d drop=%.6f elapsed=%.6f error=%.6f montage_s=%.6f"),
                *Skill->GetName(),DamageCount,Drop,Now-AcceptedTime,FirstDelayError,Position);
        }
        auto* Camera=Character->GetFirstPersonCameraComponent();
        const double ControlError=FMath::RadiansToDegrees(Controller->GetControlRotation().Quaternion().AngularDistance(LockedControl.Quaternion()));
        const double CameraRotation=FMath::RadiansToDegrees(Camera->GetComponentQuat().AngularDistance(LockedControl.Quaternion()));
        MaxCameraRotation=FMath::Max(MaxCameraRotation,CameraRotation);
        Check(ControlError<.01,TEXT("fixed_aim"));
        Check(CameraRotation<=Character->GetAppearanceCameraComponent()->BrawlerHeadMaxRotationDegrees+.02,TEXT("bounded_existing_camera"));
        Check(FVector::Dist(CharacterPosition,Character->GetActorLocation())<.1,TEXT("in_place_capsule"));
        const FVector Hand=Character->GetMesh()->GetSocketLocation(Case==0?TEXT("hand_l"):TEXT("hand_r"));
        const FVector TargetPosition=Target->GetActorLocation();
        Csv+=FString::Printf(TEXT("%d,%s,%d,%d,%.9f,%.9f,%.9f,%.9f,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),
            FrameIndex,*Skill->GetPathName(),Delta,DamageCount,Drop,AcceptedFrame<0?-1.:Now-AcceptedTime,ExpectedDelay,AcceptedPosition,
            Instance?*Skill->AttackMontage->GetPathName():TEXT(""),Position,Weight,Rate,ControlError,CameraRotation,
            Hand.X,Hand.Y,Hand.Z,TargetPosition.X,TargetPosition.Y,TargetPosition.Z);
        const double Wait=FMath::Max(3.,static_cast<double>(Skill->AttackCooldown))+.5;
        if (AcceptedFrame>=0 && Now-AcceptedTime>Wait)
        {
            Check(Delta==1,TEXT("exactly_one_key_cast_")+Skill->GetName());
            Check(DamageCount==1,TEXT("exactly_one_target_damage_")+Skill->GetName());
            Check(MontageSamples>=2 && LastPosition>FirstPosition+UE_SMALL_NUMBER,TEXT("actual_montage_advances_")+Skill->GetName());
            UE_LOG(LogRogue10m,Display,TEXT("STONE_FIST CASE skill=%s samples=%d first=%.6f last=%.6f damage=%d first_error=%.6f accepted_montage=%.6f raw_pose_error=%.6f elapsed_pose_error=%.6f"),
                *Skill->GetName(),MontageSamples,FirstPosition,LastPosition,DamageCount,FirstDelayError,AcceptedPosition,HitRawPositionError,HitPositionError);
            FText Error; Bindings->ClearSkillKey(Skill,Error); ++Case;
            if (Case==Skills.Num()) { Finish(true,TEXT("three_production_skills_complete")); return; }
            BeginCase();
        }
        if (FrameIndex>PressFrame+300 && AcceptedFrame<0) { Finish(false,TEXT("accept_timeout")); }
    }
    void Finish(bool Good,const FString& Why)
    {
        if (bFinished) { return; } bFinished=true; Check(Good,Why);
        FWorldDelegates::OnWorldPostActorTick.Remove(Tick);
        if (bFixed) { FApp::SetFixedDeltaTime(OldDelta); FApp::SetUseFixedTimeStep(OldFixed); }
        const FString Directory=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/fighter-animation-binding-20261002");
        IFileManager::Get().MakeDirectory(*Directory,true);
        FFileHelper::SaveStringToFile(Csv,*(Directory/(Label+TEXT("-stone.csv"))));
        FFileHelper::SaveStringToFile(Report,*(Directory/(Label+TEXT("-stone-assertions.txt"))));
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=STONE_FIST_ANIMATIONS_%s cases=%d assertions=%d failures=%d camera_deg=%.6f"),
            Failures==0?TEXT("PASSED"):TEXT("FAILED"),Case,Assertions,Failures,MaxCameraRotation);
        FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TWeakObjectPtr<URogue10mSkillBindingComponent> Bindings;
    TWeakObjectPtr<ARogue10mPlayerState> Progress;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;
    FTimerHandle PrepareTimer;
    FDelegateHandle Tick;
    FRotator LockedControl;
    FVector CharacterPosition;
    FString Label=TEXT("stone-followup"),Report,Csv;
    int32 Case=0,FrameIndex=0,PressFrame=0,ReleaseFrame=0,Before=0,AcceptedFrame=-1,DamageCount=0,MontageSamples=0,Assertions=0,Failures=0;
    bool bFinished=false,bFixed=false,OldFixed=false;
    double OldDelta=0,AcceptedTime=-1,AcceptedPosition=0,PreviousHealth=10000,ExpectedDelay=0,Rate=1,FirstPosition=0,LastPosition=0,FirstDelayError=0,HitPositionError=0,HitRawPositionError=0,MaxCameraRotation=0;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (GEngine) for (const FWorldContext& Context:GEngine->GetWorldContexts())
    {
        if (UWorld* W=Context.World();W && (W->WorldType==EWorldType::Game || W->WorldType==EWorldType::PIE))
        { Active=MakeShared<FRun>(); Active->Start(W); return; }
    }
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=STONE_FIST_ANIMATIONS_FAILED no_game_world"));
    FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestStoneFistAnimations"),
    TEXT("Editor QA: three production StoneFist key inputs, advancing montages and fixed-front target damage."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
