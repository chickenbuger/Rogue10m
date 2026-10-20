// Editor-only first-person integration probe. No authored pose, animation clock or asset is modified.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
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
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mCameraForwardTest
{
struct FCase
{
    TWeakObjectPtr<URogue10mAttackSkillData> Skill;
    ERogue10mWeaponType Weapon=ERogue10mWeaponType::Unarmed;
    bool bCore=false,bRotated=false;
};
// Read the evaluated raw local pose and compose every parent, then the component world transform.
// In particular, the hidden source's +Y forward and -90 degree component yaw are not world +X.
static bool BoneWorld(USkeletalMeshComponent* Mesh,FName Bone,FTransform& Result)
{
    if(!Mesh || !Mesh->GetSkeletalMeshAsset()){return false;}
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto Pose=Mesh->GetBoneSpaceTransformsView(); const int32 Index=Ref.FindBoneIndex(Bone);
    if(!Pose.IsValidIndex(Index)){return false;} Result=Pose[Index];
    for(int32 P=Ref.GetParentIndex(Index);P!=INDEX_NONE;P=Ref.GetParentIndex(P))
    {if(!Pose.IsValidIndex(P)){return false;} Result*=Pose[P];}
    Result*=Mesh->GetComponentTransform(); return !Result.ContainsNaN();
}
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* W)
    {
        World=W; FString Slot;
        if(!FParse::Value(FCommandLine::Get(),TEXT("RogueSkillKeySaveSlot="),Slot)||!Slot.StartsWith(TEXT("Rogue10m_Fighter_QA_")))
        {Check(false,TEXT("isolated_save_slot_required"));Finish();return;}
        FString Label=TEXT("runtime"); FParse::Value(FCommandLine::Get(),TEXT("RogueCameraForwardLabel="),Label);
        Label=FPaths::MakeValidFileName(Label); if(Label.IsEmpty()){Label=TEXT("runtime");}
        Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/camera-forward"),Label);
        Images=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/WindowsEditor/CameraForward"),Label);
        bCapture=FParse::Param(FCommandLine::Get(),TEXT("RogueCameraForwardCapture"));
        if(IFileManager::Get().FileExists(*FPaths::Combine(Output,TEXT("metrics.txt"))))
        {Check(false,TEXT("fresh_label_required"));Finish();return;}
        IFileManager::Get().MakeDirectory(*Output,true); if(bCapture){IFileManager::Get().MakeDirectory(*Images,true);}
        Frames=TEXT("frame,world_s,case,skill,phase,actor_x,actor_y,actor_z,actor_yaw,control_pitch,control_yaw,camera_x,camera_y,camera_z,camera_pitch,camera_yaw,eye_z,montage_s,montage_weight,elapsed_s,front_health,rear_health,front_geometry,rear_geometry,falling\n");
        Probes=TEXT("frame,case,skill,contact,stage,bone,world_x,world_y,world_z,origin_x,origin_y,origin_z,actor_forward_cm,actor_right_cm,actor_up_cm,azimuth_deg,idle_forward_extension_cm,camera_forward_cm,camera_right_cm,camera_up_cm,actor_forward_x,actor_forward_y,actor_forward_z,contact_pulse,active_strike_hand\n");
        Cases=TEXT("case,skill,weapon,core_nine,rotated,shape,input_slot,hit_count,delay_s,cooldown_s,montage,style_policy\n");
        Results=TEXT("case,skill,uses_delta,montage_samples,montage_first_s,montage_last_s,contacts,front_pulses,rear_pulses,front_geometry,rear_geometry,body_forward_extension_cm,body_contact_forward_cm,body_contact_azimuth_deg,source_forward_extension_cm,source_contact_forward_cm,first_damage_error_s\n");
        Atomic=TEXT("frame,kind,eye_z,head_x,head_y,head_z,camera_x,camera_y,camera_z,pitch,yaw,delta_cm,expected_delta_cm,error_cm,rotation_error_deg,note\n");
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()){Self->Finish(false);}});
        W->GetTimerManager().SetTimer(PrepareTimer,FTimerDelegate::CreateLambda([Self=AsShared()]{Self->Prepare();}),5.f,false);
    }
    bool IsFinished() const{return bFinished;}
private:
    void Check(bool Good,const TCHAR* Why)
    {++Assertions;if(!Good){++Failures;if(Failures<60){UE_LOG(LogRogue10m,Error,TEXT("CAMERA_FORWARD FAIL frame=%d case=%d %s"),FrameIndex,CaseIndex,Why);}}}
    void Refill()
    {auto* A=Character->GetRogueAttributeSet();A->SetStamina(A->GetMaxStamina());A->SetMana(A->GetMaxMana());A->SetIdentity(A->GetMaxIdentity());}
    bool IsJump(const URogue10mAttackSkillData* S) const
    {return S->InputSlot==ERogue10mAttackInputSlot::JumpPrimary||S->InputSlot==ERogue10mAttackInputSlot::JumpSpecial;}
    bool IsCharge(const URogue10mAttackSkillData* S) const
    {return S->InputSlot==ERogue10mAttackInputSlot::ChargedPrimary||S->InputSlot==ERogue10mAttackInputSlot::ChargedSpecial;}
    bool StrictPunch(const FCase& C) const
    {
        const FString Clip=C.Skill->AttackMontage->GetName();
        return C.bCore&&C.Skill->AttackShape==ERogue10mAttackShape::LinearBox&&!Clip.Contains(TEXT("Hook"))&&!Clip.Contains(TEXT("Uppercut"));
    }
    FName ActiveStrikeHand(const FCase& C,int32 Pulse) const
    {
        if(C.bCore&&C.Skill->GetName().Contains(TEXT("LeftJab"))){return TEXT("hand_l");}
        if(C.bCore&&C.Skill->AttackMontage->GetName().Contains(TEXT("HookFlurry")))
        {return Pulse%2==0?FName(TEXT("hand_l")):FName(TEXT("hand_r"));}
        // This derivative's Niagara attach socket remains right, but its authored counter strikes with the left hand.
        if(C.Skill->AttackMontage->GetName().Contains(TEXT("FlowingCloud_LeftCounter"))){return TEXT("hand_l");}
        return C.bCore?FName(TEXT("hand_r")):C.Skill->EffectAttachSocket;
    }
    bool ExpectedCamera(FVector& P,FQuat& Q,FTransform& Head,FQuat* OutMotion=nullptr) const
    {
        auto* Body=Character->GetMesh();auto* Settings=Character->GetAppearanceCameraComponent();
        if(!BoneWorld(Body,Settings->HeadBone,Head)){return false;}
        const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();const int32 H=Ref.FindBoneIndex(Settings->HeadBone);
        if(H==INDEX_NONE){return false;} FTransform Reference=Ref.GetRefBonePose()[H];
        for(int32 I=Ref.GetParentIndex(H);I!=INDEX_NONE;I=Ref.GetParentIndex(I)){Reference*=Ref.GetRefBonePose()[I];}
        const FQuat MeshQ=Body->GetComponentQuat();
        const FQuat HeadCS=MeshQ.Inverse()*Head.GetRotation();
        FQuat Motion=(MeshQ*HeadCS*Reference.GetRotation().Inverse()*MeshQ.Inverse()).GetNormalized();
        if(Settings->IsBrawlerViewStabilized())
        {
            if(Motion.W<0){Motion=Motion*(-1.);}
            FVector Axis;double Angle=0;Motion.ToAxisAndAngle(Axis,Angle);
            const double Limit=FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(Settings->BrawlerHeadMaxRotationDegrees),0.,180.));
            const double Weighted=FMath::Min(Angle*FMath::Clamp(static_cast<double>(Settings->BrawlerHeadRotationWeight),0.,1.),Limit);
            Motion=Weighted>UE_SMALL_NUMBER?FQuat(Axis.GetSafeNormal(),Weighted):FQuat::Identity;
        }
        P=Head.GetLocation()+(Motion*Character->GetActorQuat()).RotateVector(Settings->EyeOffset);
        Q=(Motion*Controller->GetControlRotation().Quaternion()).GetNormalized();if(OutMotion){*OutMotion=Motion;}
        return !P.ContainsNaN()&&!Q.ContainsNaN();
    }
    void AddCase(URogue10mAttackSkillData* Skill,ERogue10mWeaponType Weapon,bool Core)
    {
        if(!Skill){Check(false,TEXT("required_skill_missing"));return;}
        if(Skill->SkillTreeNodeType!=ERogue10mSkillTreeNodeType::ActiveTechnique){++PassiveNodes;return;}
        for(const FCase& Existing:Queue){if(Existing.Skill==Skill){return;}}
        Check(Skill->AttackMontage!=nullptr,TEXT("attack_montage_missing"));if(!Skill->AttackMontage){return;}
        Queue.Add({Skill,Weapon,Core,false});
    }
    void Prepare()
    {
        Controller=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        Character=Controller.IsValid()?Cast<ARogue10mCharacter>(Controller->GetPawn()):nullptr;
        if(!Character.IsValid()||!Character->GetPlayerState<ARogue10mPlayerState>()||!Character->GetRogueAttributeSet())
        {Check(false,TEXT("runtime_player_missing"));Finish();return;}
        auto* Settings=Character->GetAppearanceCameraComponent();auto* Source=Character->GetAnimationPlaybackMesh();auto* Body=Character->GetMesh();
        if(!Settings||!Source||!Body||!Source->GetAnimInstance()||!Body->GetAnimInstance())
        {Check(false,TEXT("evaluated_source_body_camera_missing"));Finish();return;}
        Check(Source!=Body,TEXT("actual_retarget_body_required"));
        OldControl=Controller->GetControlRotation();OldActor=Character->GetActorTransform();OldWeapon=Character->GetEquippedWeaponType();
        OldEye=Settings->EyeOffset;OldDamage=Character->CanBeDamaged();OldThird=Settings->IsThirdPersonInspectionActive();bPrepared=true;
        Check(FMath::Abs(FRotator::NormalizeAxis(OldControl.Pitch)-Settings->InitialViewPitch)<.1f,TEXT("fresh_default_initial_pitch_not_applied"));
        Check(FMath::IsNearlyEqual(Settings->InitialViewPitch,-20.f,.01f),TEXT("initial_pitch_changed_from_minus20"));
        Check(Settings->EyeOffset.Equals(FVector(8,0,-3),.01f),TEXT("default_camera_offset_not_lowered_seven_cm"));
        Check(Settings->IsAppearanceCameraActive(),TEXT("default_appearance_camera_not_active"));
        if(OldThird){Settings->ToggleThirdPersonInspection();}
        Controller->CloseAllBlockingPanels();Controller->SetIgnoreLookInput(true);Controller->SetIgnoreMoveInput(true);bIgnored=true;
        Character->SetCanBeDamaged(false);Character->GetCharacterMovement()->StopMovementImmediately();
        const TCHAR* Paths[]={TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab"),
            TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight"),TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02"),
            TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special"),TEXT("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged")};
        for(int32 I=0;I<9;++I){AddCase(LoadObject<URogue10mAttackSkillData>(nullptr,Paths[I]),I<4?ERogue10mWeaponType::Unarmed:ERogue10mWeaponType::Knuckle,true);}
        for(ERogue10mWeaponType Weapon:{ERogue10mWeaponType::Unarmed,ERogue10mWeaponType::Knuckle})
        {Character->SetEquippedWeaponType(Weapon);for(auto* Skill:Character->GetCombatComponent()->GetActiveSkillTreeSkills()){AddCase(Skill,Weapon,false);}}
        UniqueCases=Queue.Num();Check(UniqueCases>9,TEXT("fighter_derivative_scope_missing"));
        // Repeated real yaw cases demonstrate that signed projections use actor axes, not a hardcoded world direction.
        if(Queue.Num()>=3){FCase Jab=Queue[0],Straight=Queue[2];Jab.bRotated=Straight.bRotated=true;Queue.Add(Jab);Queue.Add(Straight);}
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
        UClass* MonsterClass=ARogue10mBasicMonster::StaticClass();
        for(TActorIterator<ARogue10mBasicMonster> It(World.Get());It;++It)
        {
            MonsterClass=It->GetClass();It->ClearAITarget();if(auto* AI=It->GetController()){AI->UnPossess();}
            It->GetCharacterMovement()->DisableMovement();It->SetActorEnableCollision(false);
        }
        for(int32 I=0;I<2;++I)
        {
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Dummy=World->SpawnActor<ARogue10mBasicMonster>(MonsterClass,OldActor.GetLocation()+OldActor.GetRotation().GetForwardVector()*(I==0?135.f:-135.f),FRotator::ZeroRotator,Params);
            if(!Dummy){Check(false,TEXT("actual_monster_dummy_spawn_failed"));Finish();return;}
            Targets[I]=Dummy;Dummy->ClearAITarget();if(auto* AI=Dummy->GetController()){AI->UnPossess();}
            Dummy->GetCharacterMovement()->DisableMovement();Dummy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Dummy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
            Dummy->GetRogueAttributeSet()->SetMaxHealth(100000);Dummy->GetRogueAttributeSet()->SetHealth(100000);
            if(auto* Regen=Dummy->FindComponentByClass<URogue10mVitalRegenerationComponent>()){Regen->ConfigureRegeneration(0,0,0);}
        }
        for(int32 I=0;I<Queue.Num();++I)
        {
            const FCase& C=Queue[I];const auto* S=C.Skill.Get();
            Cases+=FString::Printf(TEXT("%d,%s,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%s,%s\n"),I,*S->GetPathName(),static_cast<int32>(C.Weapon),C.bCore,C.bRotated,static_cast<int32>(S->AttackShape),static_cast<int32>(S->InputSlot),S->HitCount,S->HitStartDelaySeconds,S->AttackCooldown,*S->AttackMontage->GetPathName(),StrictPunch(C)?TEXT("strict_forward_extension"):TEXT("style_direction_report_only"));
        }
        // Complete the editor asset jobs once for reviewable rendered evidence, after all fixture loads.
        // This runs before the observation tick/fixed-step clock; it does not force or reset an animation pose.
        if(bCapture&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi")))
        {
            auto& Compilation=FAssetCompilingManager::Get();
            UE_LOG(LogRogue10m,Display,TEXT("CAMERA_FORWARD CAPTURE_ASSETS begin_remaining=%d"),Compilation.GetNumRemainingAssets());
            Compilation.FinishAllCompilation();
            const int32 Remaining=Compilation.GetNumRemainingAssets();
            UE_LOG(LogRogue10m,Display,TEXT("CAMERA_FORWARD CAPTURE_ASSETS finished_remaining=%d"),Remaining);
            Check(Remaining==0,TEXT("capture_asset_compilation_incomplete"));
        }
        OldFixed=FApp::UseFixedTimeStep();OldDelta=FApp::GetFixedDeltaTime();FApp::SetFixedDeltaTime(1./30.);FApp::SetUseFixedTimeStep(true);bFixed=true;
        Tick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Frame();}});
        UE_LOG(LogRogue10m,Display,TEXT("CAMERA_FORWARD START map=%s unique_attacks=%d cases=%d passive_observations=%d body=%s source=%s initial_pitch=%.3f eye=%s"),*World->GetMapName(),UniqueCases,Queue.Num(),PassiveNodes,*Body->GetSkeletalMeshAsset()->GetPathName(),*Source->GetSkeletalMeshAsset()->GetPathName(),OldControl.Pitch,*OldEye.ToCompactString());
    }
    void AtomicHeight()
    {
        auto* Settings=Character->GetAppearanceCameraComponent();FVector Expected;FQuat Q,Motion;FTransform Before,After;
        Check(ExpectedCamera(Expected,Q,Before,&Motion),TEXT("atomic_head_oracle_missing"));
        Settings->EyeOffset.Z=4;FMinimalViewInfo Old;Character->CalcCamera(0,Old);
        Settings->EyeOffset.Z=-3;FMinimalViewInfo Current;Character->CalcCamera(0,Current);
        Settings->EyeOffset=OldEye;Character->CalcCamera(0,Current);
        Check(BoneWorld(Character->GetMesh(),Settings->HeadBone,After),TEXT("atomic_head_after_missing"));
        const FVector ExpectedDelta=(Motion*Character->GetActorQuat()).RotateVector(FVector(0,0,-7));const FVector Actual=Current.Location-Old.Location;
        const double Error=FVector::Distance(Actual,ExpectedDelta),Angle=FMath::RadiansToDegrees(Current.Rotation.Quaternion().AngularDistance(Old.Rotation.Quaternion()));
        Check(Error<.001&&FMath::Abs(Actual.Size()-7.)<.001,TEXT("same_pose_camera_height_delta_not_seven_cm"));
        Check(Angle<.0001&&Before.Equals(After,.000001),TEXT("atomic_camera_comparison_changed_pose_or_rotation"));
        AtomicError=Error;AtomicDelta=Actual.Size();bAtomic=true;
        for(int32 I=0;I<2;++I)
        {const auto& V=I==0?Old:Current;Atomic+=FString::Printf(TEXT("%d,atomic,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,7,%.9f,%.9f,same_evaluated_pose_no_tick\n"),FrameIndex,I==0?4:-3,Before.GetLocation().X,Before.GetLocation().Y,Before.GetLocation().Z,V.Location.X,V.Location.Y,V.Location.Z,V.Rotation.Pitch,V.Rotation.Yaw,Actual.Size(),Error,Angle);}
    }
    void IdleShot(bool Old)
    {
        auto* Settings=Character->GetAppearanceCameraComponent();Settings->EyeOffset.Z=Old?4:-3;
        Controller->PlayerCameraManager->UpdateCamera(0);const auto View=Controller->PlayerCameraManager->GetCameraCacheView();FTransform Head;
        BoneWorld(Character->GetMesh(),Settings->HeadBone,Head);
        Atomic+=FString::Printf(TEXT("%d,render_idle,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,0,0,0,0,natural_idle_different_frame\n"),FrameIndex,Old?4:-3,Head.GetLocation().X,Head.GetLocation().Y,Head.GetLocation().Z,View.Location.X,View.Location.Y,View.Location.Z,View.Rotation.Pitch,View.Rotation.Yaw);
        // Cache the old rendered POV for this frame; restore the actual component setting immediately.
        Settings->EyeOffset=OldEye;FMinimalViewInfo Restore;Character->CalcCamera(0,Restore);
        if(bCapture){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,Old?TEXT("idle_old_eye4.png"):TEXT("idle_current_eye_minus3.png")),true,false);}
    }
    void BeginCase()
    {
        const FCase& C=Queue[CaseIndex];auto* S=C.Skill.Get();auto* Combat=Character->GetCombatComponent();
        Character->SetEquippedWeaponType(C.Weapon);Combat->UnlockAllActiveTreeForTesting();Combat->UnlockAttackSkill(S);Refill();
        Check(Combat->IsSkillInActiveTree(S),TEXT("scoped_attack_not_in_current_weapon_tree"));
        // Current selected Fighter data has >=2 targets and no locked MultiHit mode.
        // Reject an unsupported future data change instead of treating target-cap/locked damage as a direction failure.
        Check(S->MaxTargetsPerHit>=2&&S->HitMode!=ERogue10mAttackHitMode::MultiHit,TEXT("geometry_oracle_requires_current_fighter_target_and_hit_mode_contract"));
        FRotator Actor=OldActor.Rotator();if(C.bRotated){Actor.Yaw+=90;}Character->SetActorRotation(Actor);
        FRotator Control=OldControl;Control.Yaw=Actor.Yaw;Controller->SetControlRotation(Control);
        Character->GetCharacterMovement()->StopMovementImmediately();
        const FVector Ground=Character->GetActorLocation();
        for(int32 I=0;I<2;++I)
        {Targets[I]->SetActorLocation(Ground+Character->GetActorForwardVector()*(I==0?135.f:-135.f),false,nullptr,ETeleportType::TeleportPhysics);Targets[I]->GetRogueAttributeSet()->SetHealth(100000);PreviousHealth[I]=100000;Pulses[I]=0;AnyGeometry[I]=false;PreviousGeometry[I]=false;}
        BeforeUses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(S->GetFName());CaseStart=World->GetTimeSeconds();AcceptedTime=-1;
        bPressed=bReleased=bJumped=bShot=false;MontageSamples=Contacts=0;FirstMontage=LastMontage=0;MaxExtension[0]=MaxExtension[1]=0;
        MaxForward[0]=MaxForward[1]=-DBL_MAX;BestAngle=180;FirstDamageError=0;IdleForward.Empty();
        const double Speed=FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f);
        Rate=FMath::Max(.01,Speed*S->AnimationPlayRate);Delay=S->HitStartDelaySeconds/Rate;Interval=S->HitInterval/Speed;
        const int32 ExpectedPulses=S->HitMode==ERogue10mAttackHitMode::Single?1:FMath::Clamp(S->HitCount,1,64);
        CaseDuration=FMath::Max(3.,.5+(IsCharge(S)?S->ChargeSeconds+.1:0)+FMath::Max(static_cast<double>(S->AttackCooldown),FMath::Max(S->AttackMontage->GetPlayLength()/Rate,Delay+(ExpectedPulses-1)*Interval))+1.);
        UE_LOG(LogRogue10m,Display,TEXT("CAMERA_FORWARD CASE index=%d skill=%s yaw=%.3f duration=%.3f charged=%d jump=%d"),CaseIndex,*S->GetPathName(),Actor.Yaw,CaseDuration,IsCharge(S),IsJump(S));
    }
    bool Geometry(const URogue10mAttackSkillData& S,int32 TargetIndex,const FVector& Origin,const FQuat& Rotation,int32 Pulse)
    {
        FVector Center=Origin;FQuat Query=FQuat::Identity;FCollisionShape Shape;
        const FVector Forward=Rotation.GetForwardVector();
        if(S.AttackShape==ERogue10mAttackShape::LinearBox)
        {Center+=Forward*S.AttackRange*.5;Query=Rotation;Shape=FCollisionShape::MakeBox(FVector(S.AttackRange*.5,S.BoxHalfWidth,S.BoxHalfHeight));}
        else if(S.AttackShape==ERogue10mAttackShape::Projectile)
        {const int32 Count=S.HitMode==ERogue10mAttackHitMode::Single?1:FMath::Clamp(S.HitCount,1,64);const double Segment=S.AttackRange/Count;Center+=Forward*(Pulse+.5)*Segment;Query=Rotation;Shape=FCollisionShape::MakeBox(FVector(Segment*.5,S.AttackTraceRadius,S.AttackTraceRadius));}
        else if(S.AttackShape==ERogue10mAttackShape::Arc){Shape=FCollisionShape::MakeSphere(S.AttackRange);}
        else{Center+=Forward*S.CircleForwardOffset;Shape=FCollisionShape::MakeSphere(S.AttackRange);}
        FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraForwardIndependent),false,Character.Get());TArray<FOverlapResult> Found;
        World->OverlapMultiByObjectType(Found,Center,Query,Objects,Shape,Params);bool Included=false;
        for(const auto& Item:Found){if(Item.GetActor()==Targets[TargetIndex].Get()){Included=true;}}
        if(S.AttackShape==ERogue10mAttackShape::Arc)
        {const double Dot=FVector::DotProduct(Forward,(Targets[TargetIndex]->GetActorLocation()-Origin).GetSafeNormal());Included&=Dot>=FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(S.ArcAngleDegrees,1.f,180.f)*.5));}
        return Included;
    }
    void Probe(USkeletalMeshComponent* Mesh,int32 Stage,const TCHAR* StageName,bool Contact,int32 Pulse,const FVector& CameraP,const FQuat& CameraQ)
    {
        for(const TCHAR* Bone:{TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")})
        {
            const FString Name=Bone;const FName Reference=*FString::Printf(TEXT("%s_%s"),Name.StartsWith(TEXT("hand"))?TEXT("upperarm"):TEXT("thigh"),Name.EndsWith(TEXT("_l"))?TEXT("l"):TEXT("r"));
            FTransform Point,Origin;if(!BoneWorld(Mesh,Bone,Point)||!BoneWorld(Mesh,Reference,Origin)){Check(false,TEXT("evaluated_probe_bone_missing"));continue;}
            const FVector V=Point.GetLocation()-Origin.GetLocation(),Fwd=Character->GetActorForwardVector();
            const double F=FVector::DotProduct(V,Fwd),R=FVector::DotProduct(V,Character->GetActorRightVector()),U=FVector::DotProduct(V,Character->GetActorUpVector());
            const double Az=FMath::RadiansToDegrees(FMath::Atan2(R,F));const FString Key=FString(StageName)+TEXT("/")+Name;
            if(!bPressed){IdleForward.Add(Key,F);}const double Extension=F-IdleForward.FindRef(Key);const FVector Cam=CameraQ.UnrotateVector(Point.GetLocation()-CameraP);
            const bool ActiveHand=FName(Bone)==ActiveStrikeHand(Queue[CaseIndex],Pulse);
            if(Contact&&ActiveHand)
            {MaxExtension[Stage]=FMath::Max(MaxExtension[Stage],Extension);MaxForward[Stage]=FMath::Max(MaxForward[Stage],F);if(Stage==1&&Extension>3){BestAngle=FMath::Min(BestAngle,FMath::Abs(Az));}}
            Probes+=FString::Printf(TEXT("%d,%d,%s,%d,%s,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d\n"),FrameIndex,CaseIndex,*Queue[CaseIndex].Skill->GetPathName(),Contact,StageName,Bone,Point.GetLocation().X,Point.GetLocation().Y,Point.GetLocation().Z,Origin.GetLocation().X,Origin.GetLocation().Y,Origin.GetLocation().Z,F,R,U,Az,Extension,Cam.X,Cam.Y,Cam.Z,Fwd.X,Fwd.Y,Fwd.Z,Pulse,ActiveHand);
        }
    }
    void CompleteCase()
    {
        const FCase& C=Queue[CaseIndex];const auto* S=C.Skill.Get();const int32 Delta=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(S->GetFName())-BeforeUses;
        Check(Delta==1,TEXT("scoped_bound_input_not_accepted_exactly_once"));Check(MontageSamples>=2&&LastMontage>FirstMontage+.001,TEXT("authored_montage_not_evaluated_and_advanced"));Check(Contacts>0,TEXT("attack_contact_window_not_observed"));
        for(int32 I=0;I<2;++I){Check((Pulses[I]>0)==AnyGeometry[I],TEXT("actual_front_or_rear_damage_disagrees_with_geometry"));}
        if(C.bCore)
        {
            const int32 Expected=S->HitMode==ERogue10mAttackHitMode::Single?1:FMath::Clamp(S->HitCount,1,64);
            Check(Pulses[0]==Expected,TEXT("core_attack_front_target_pulses_missing"));
            if(S->AttackShape!=ERogue10mAttackShape::Circle){Check(Pulses[1]==0,TEXT("forward_core_attack_damaged_rear_target"));}
        }
        if(StrictPunch(C))
        {Check(MaxExtension[0]>3&&MaxExtension[1]>3,TEXT("source_or_retarget_punch_did_not_extend_forward_at_contact"));Check(MaxForward[1]>0&&BestAngle<75,TEXT("retarget_punch_contact_not_in_actor_forward_sector"));}
        Results+=FString::Printf(TEXT("%d,%s,%d,%d,%.9f,%.9f,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),CaseIndex,*S->GetPathName(),Delta,MontageSamples,FirstMontage,LastMontage,Contacts,Pulses[0],Pulses[1],AnyGeometry[0],AnyGeometry[1],MaxExtension[1],MaxForward[1],BestAngle,MaxExtension[0],MaxForward[0],FirstDamageError);
        UE_LOG(LogRogue10m,Display,TEXT("CAMERA_FORWARD CASE_RESULT index=%d skill=%s uses=%d samples=%d front=%d rear=%d body_forward_extension=%.6f azimuth=%.6f"),CaseIndex,*S->GetName(),Delta,MontageSamples,Pulses[0],Pulses[1],MaxExtension[1],BestAngle);
        ++CompletedCases;
    }
    void Frame()
    {
        if(!Character.IsValid()||!Controller.IsValid()||!Targets[0].IsValid()||!Targets[1].IsValid())
        {Check(false,TEXT("fixture_actor_lost"));Finish();return;}
        auto* Settings=Character->GetAppearanceCameraComponent();Check(!Settings->IsThirdPersonInspectionActive(),TEXT("capture_not_first_person"));
        FVector CP;FQuat CQ;FTransform Head;if(!ExpectedCamera(CP,CQ,Head)){Check(false,TEXT("independent_camera_oracle_missing"));Finish();return;}
        const auto Cached=Controller->PlayerCameraManager->GetCameraCacheView();
        const double PE=FVector::Distance(Cached.Location,CP),RE=FMath::RadiansToDegrees(Cached.Rotation.Quaternion().AngularDistance(CQ));
        Check(PE<.05&&RE<.02,TEXT("automatic_cached_firstperson_camera_not_current_body_head"));MaxCameraError=FMath::Max(MaxCameraError,PE);
        Check(FMath::Abs(FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch)+20)<.01,TEXT("fixture_default_pitch_changed"));
        if(FrameIndex==29){AtomicHeight();}if(FrameIndex==30){IdleShot(true);}if(FrameIndex==31){IdleShot(false);}
        if(FrameIndex==60){CaseIndex=0;BeginCase();}
        if(CaseIndex>=0)
        {
            const auto* S=Queue[CaseIndex].Skill.Get();auto* Combat=Character->GetCombatComponent();const double Now=World->GetTimeSeconds(),Elapsed=Now-CaseStart;
            if(IsJump(S)&&!bJumped&&Elapsed>=.2){Character->Jump();bJumped=true;}
            if(IsJump(S)&&bJumped&&Elapsed>=.3){Character->StopJumping();}
            const int32 Uses=Character->GetPlayerState<ARogue10mPlayerState>()->GetSkillUseCount(S->GetFName());
            if(AcceptedTime<0&&Uses>BeforeUses)
            {AcceptedTime=Now;Check(Combat->GetLastAcceptedAttackSkill()==S,TEXT("accepted_skill_identity_mismatch"));}
            const double AttackElapsed=AcceptedTime<0?-1:Now-AcceptedTime;bool Contact=false;int32 Pulse=0;
            const int32 HitCount=S->HitMode==ERogue10mAttackHitMode::Single?1:FMath::Clamp(S->HitCount,1,64);
            if(AcceptedTime>=0)
            {double Closest=DBL_MAX;for(int32 I=0;I<HitCount;++I){const double D=FMath::Abs(AttackElapsed-(Delay+I*Interval));if(D<=2./30.&&D<Closest){Contact=true;Pulse=I;Closest=D;}}}
            if(Contact){++Contacts;}
            auto* Anim=Character->GetAnimationPlaybackMesh()->GetAnimInstance();const auto* Instance=Anim->GetActiveInstanceForMontage(S->AttackMontage);
            const double Position=Anim->Montage_GetPosition(S->AttackMontage),Weight=Instance?Instance->GetWeight():0;
            if(AcceptedTime>=0&&Weight>0){if(MontageSamples==0){FirstMontage=Position;}LastMontage=FMath::Max(LastMontage,Position);++MontageSamples;}
            bool Included[2]={false,false};
            for(int32 I=0;I<2;++I)
            {
                Included[I]=Geometry(*S,I,CP,CQ,Pulse);if(Contact){AnyGeometry[I]|=Included[I];}
                const double Health=Targets[I]->GetRogueAttributeSet()->GetHealth(),Drop=PreviousHealth[I]-Health;
                if(Drop>.01)
                {
                    ++Pulses[I];++DamagePulses;Check(AcceptedTime>=0,TEXT("damage_without_accepted_skill"));
                    Check(Included[I]||PreviousGeometry[I],TEXT("actual_damage_target_outside_independent_current_or_previous_geometry"));
                    if(I==0&&Pulses[I]==1){FirstDamageError=AttackElapsed-Delay;Check(FMath::Abs(FirstDamageError)<=1./30.+.015,TEXT("first_actual_damage_outside_timer_tolerance"));}
                }
                PreviousHealth[I]=Health;PreviousGeometry[I]=Included[I];
            }
            Probe(Character->GetAnimationPlaybackMesh(),0,TEXT("source"),Contact,Pulse,CP,CQ);Probe(Character->GetMesh(),1,TEXT("retarget_body"),Contact,Pulse,CP,CQ);
            const FVector AP=Character->GetActorLocation();
            Frames+=FString::Printf(TEXT("%d,%.9f,%d,%s,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%d,%d,%d\n"),FrameIndex,Now,CaseIndex,*S->GetPathName(),Contact?TEXT("contact"):TEXT("motion"),AP.X,AP.Y,AP.Z,Character->GetActorRotation().Yaw,Controller->GetControlRotation().Pitch,Controller->GetControlRotation().Yaw,CP.X,CP.Y,CP.Z,CQ.Rotator().Pitch,CQ.Rotator().Yaw,Settings->EyeOffset.Z,Position,Weight,AttackElapsed,PreviousHealth[0],PreviousHealth[1],Included[0],Included[1],Character->GetCharacterMovement()->IsFalling());
            if(Contact&&!bShot&&Weight>.1)
            {bShot=true;if(bCapture){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Images,FString::Printf(TEXT("contact_%02d_%s%s.png"),CaseIndex,*S->GetName(),Queue[CaseIndex].bRotated?TEXT("_yaw90"):TEXT(""))),true,false);}}
            if(!bPressed&&Elapsed>=.4)
            {Refill();Combat->HandleBoundSkillPressed(Queue[CaseIndex].Skill.Get());bPressed=true;}
            if(bPressed&&!bReleased&&Elapsed>=.4+(IsCharge(S)?S->ChargeSeconds+.1:.1))
            {Combat->HandleBoundSkillReleased(Queue[CaseIndex].Skill.Get());bReleased=true;}
            if(Elapsed>=CaseDuration)
            {
                CompleteCase();++CaseIndex;
                if(CaseIndex==Queue.Num()){Check(bAtomic,TEXT("atomic_height_comparison_missing"));bComplete=true;Finish();return;}
                Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("real_jump_case_did_not_land"));BeginCase();
            }
        }
        ++FrameIndex;if(FrameIndex>12000){Check(false,TEXT("bounded_fixture_timeout"));Finish();}
    }
    void Finish(bool Exit=true)
    {
        if(bFinished){return;}bFinished=true;FWorldDelegates::OnWorldPostActorTick.Remove(Tick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if(World.IsValid()){World->GetTimerManager().ClearTimer(PrepareTimer);}
        if(bPrepared&&Character.IsValid())
        {
            Character->GetCombatComponent()->CancelBoundSkillInput();Character->GetCombatComponent()->CancelCombatVisuals();Character->StopJumping();
            Character->SetCanBeDamaged(OldDamage);Character->SetEquippedWeaponType(OldWeapon);Character->SetActorTransform(OldActor,false,nullptr,ETeleportType::TeleportPhysics);
            auto* Settings=Character->GetAppearanceCameraComponent();Settings->EyeOffset=OldEye;
            if(Settings->IsThirdPersonInspectionActive()!=OldThird){Settings->ToggleThirdPersonInspection();}
            if(Controller.IsValid()){Controller->SetControlRotation(OldControl);if(bIgnored){Controller->SetIgnoreLookInput(false);Controller->SetIgnoreMoveInput(false);}}
        }
        for(auto& T:Targets){if(T.IsValid()){T->Destroy();}}
        if(bFixed){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);}
        if(!Output.IsEmpty())
        {
            for(const auto& File:TArray<TPair<FString,FString>>{{TEXT("frames.csv"),Frames},{TEXT("probes.csv"),Probes},{TEXT("cases.csv"),Cases},{TEXT("results.csv"),Results},{TEXT("camera-height.csv"),Atomic}})
            {Check(FFileHelper::SaveStringToFile(File.Value,*FPaths::Combine(Output,File.Key)),TEXT("csv_write_failed"));}
            const FString Metrics=FString::Printf(TEXT("frames=%d\nassertions=%d\nfailures=%d\nunique_attack_skills=%d\ncompleted_cases=%d\nexpected_cases=%d\nactual_health_pulses=%d\natomic_camera_delta_cm=%.9f\natomic_camera_error_cm=%.9f\nmax_cached_camera_error_cm=%.9f\nNOTE=Fresh first-person pitch is observed before writes. Idle old-eye4/current-eye-3 renders retain natural animation at different recorded frames; same-pose atomic CalcCamera comparison proves the 7cm delta. Source and visible retarget bones are independently composed through parent local transforms and component world transform. Core straight punches require forward extension; hooks/kicks/area styles report contact directions. Actual ground physics and bound combat handlers are used; no synthetic airborne mode, pose or animation clock reset. Circle damage is compared to geometry and may legally include rear targets.\n"),FrameIndex,Assertions,Failures,UniqueCases,CompletedCases,Queue.Num(),DamagePulses,AtomicDelta,AtomicError,MaxCameraError);
            Check(FFileHelper::SaveStringToFile(Metrics,*FPaths::Combine(Output,TEXT("metrics.txt"))),TEXT("metrics_write_failed"));
        }
        const bool Passed=Failures==0&&bComplete;
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=CAMERA_FORWARD_%s frames=%d assertions=%d failures=%d cases=%d/%d output=%s images=%s"),Passed?TEXT("PASSED"):TEXT("FAILED"),FrameIndex,Assertions,Failures,CompletedCases,Queue.Num(),*Output,*Images);
        if(Exit){FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<ARogue10mBasicMonster> Targets[2];TArray<FCase> Queue;TMap<FString,double> IdleForward;
    FDelegateHandle Tick,Cleanup;FTimerHandle PrepareTimer;FString Output,Images,Frames,Probes,Cases,Results,Atomic;
    FRotator OldControl;FTransform OldActor;FVector OldEye;ERogue10mWeaponType OldWeapon=ERogue10mWeaponType::Unarmed;
    int32 Assertions=0,Failures=0,FrameIndex=0,CaseIndex=-1,UniqueCases=0,CompletedCases=0,PassiveNodes=0,BeforeUses=0,MontageSamples=0,Contacts=0,DamagePulses=0,Pulses[2]={0,0};
    double OldDelta=1./30.,CaseStart=0,AcceptedTime=-1,CaseDuration=4,Rate=1,Delay=0,Interval=.08,FirstMontage=0,LastMontage=0,FirstDamageError=0;
    double PreviousHealth[2]={0,0},MaxExtension[2]={0,0},MaxForward[2]={0,0},BestAngle=180,AtomicDelta=0,AtomicError=0,MaxCameraError=0;
    bool AnyGeometry[2]={false,false},PreviousGeometry[2]={false,false};
    bool bFinished=false,bPrepared=false,bCapture=false,bIgnored=false,bFixed=false,OldFixed=false,OldDamage=true,OldThird=false,bPressed=false,bReleased=false,bJumped=false,bShot=false,bAtomic=false,bComplete=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine||GIsEditor||(Active.IsValid()&&!Active->IsFinished())){return;}
    for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&W->WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(W);return;}}
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestCameraForward"),TEXT("Actual first-person lowered camera, core nine + active Fighter derivative attacks, raw source/retarget signed contact probes and real front/rear damage. -RogueCameraForwardLabel=name -RogueCameraForwardCapture; requires isolated Fighter QA save slot."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
