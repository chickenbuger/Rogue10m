// Isolated, opt-in presentation QA. InputKey below is programmatic input, not a physical-input test.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
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
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Rogue10mWeightedFirstPersonSettings.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace Rogue10mWeightedFirstPersonTest
{
static FString EditableSettings(const UObject& Object)
{
    FString Result;
    for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
        FString Value;
        It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(&Object), nullptr, nullptr, PPF_None);
        Result += It->GetName() + TEXT("=") + Value + TEXT("\n");
    }
    return Result;
}
static bool RawHead(USkeletalMeshComponent* Mesh, FTransform& Out)
{
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) { return false; }
    const auto& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto Pose = Mesh->GetBoneSpaceTransformsView();
    const int32 Head = Skeleton.FindBoneIndex(TEXT("head"));
    if (!Pose.IsValidIndex(Head)) { return false; }
    Out = Pose[Head];
    for (int32 P=Skeleton.GetParentIndex(Head); P!=INDEX_NONE; P=Skeleton.GetParentIndex(P))
    { if (!Pose.IsValidIndex(P)) { return false; } Out *= Pose[P]; }
    Out *= Mesh->GetComponentTransform();
    return !Out.ContainsNaN() && Out.GetRotation().IsNormalized();
}
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World=InWorld;
        FString ProfileSlot, Prefix, KeySlot;
        const TCHAR* Args=FCommandLine::Get();
        FParse::Value(Args,TEXT("RogueWeightedFPAttackSpeed="),FixtureAttackSpeed);
        const bool bArgs=FParse::Value(Args,TEXT("RogueWeightedFPLabel="),Label)
            && FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot)
            && FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix)
            && FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
        bool bLabel=!Label.IsEmpty() && Label.Len()<=24;
        for (TCHAR C:Label) { bLabel &= (C>=TEXT('A') && C<=TEXT('Z')) || (C>=TEXT('a') && C<=TEXT('z')) || (C>=TEXT('0') && C<=TEXT('9')) || C==TEXT('_'); }
        if (!bArgs || !bLabel || !FMath::IsFinite(FixtureAttackSpeed) || FixtureAttackSpeed<.5f || FixtureAttackSpeed>3.f || ProfileSlot!=TEXT("Rogue10m_Town_QA_Heavy_")+Label
            || Prefix!=TEXT("Rogue10m_Progression_QA_Heavy_")+Label || KeySlot!=TEXT("Rogue10m_Fighter_QA_Heavy_")+Label)
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEIGHTED_FIRST_PERSON_FAILED reason=exact_fresh_QA_slots_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/weighted-first-person-20261029")/Label;
        Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261029_WeightedFirstPerson_")+Label);
        bRendered=!FParse::Param(Args,TEXT("nullrhi"));
        if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEIGHTED_FIRST_PERSON_FAILED reason=fresh_evidence_label_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        IFileManager::Get().MakeDirectory(*Output,true);
        if (bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
        Frames=TEXT("frame,stage,world_s,active,inspection,dead,speed_cm_s,pitch,yaw,fov,rotation_cap_deg,position_cap_cm,hand_pitch_l,hand_pitch_r,hand_applied,montage_s,montage_rate,target_health\n");
        Heavy=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy"));
        Motion=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyMotionScale"));
        FOV=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyFOV"));
        Check(Heavy && Motion && FOV,TEXT("profile_console_variables_registered"));
        if (Failures) { Finish(); return; }
        OldHeavy=Heavy->GetInt(); OldMotion=Motion->GetFloat(); OldFOV=FOV->GetFloat(); bOwnCVars=true;
        Check(OldHeavy==0 && FMath::IsNearlyEqual(OldMotion,1.f) && FMath::IsNearlyZero(OldFOV),TEXT("fresh_process_profile_is_opt_in_with_default_controls"));
        Heavy->Set(1,ECVF_SetByCode); Motion->Set(1.f,ECVF_SetByCode); FOV->Set(0.f,ECVF_SetByCode);
        Deadline=FPlatformTime::Seconds()+180.;
        Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }),0.02f);
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get() && !Self->bFinished) { Self->Check(false,TEXT("world_cleanup_before_completion")); Self->Finish(false); } });
        Later(3.,[this]{Prepare();});
    }
    bool Finished() const { return bFinished; }
private:
    void Check(bool OK,const TCHAR* Reason)
    { ++Checks; if (!OK) { ++Failures; Results+=FString(TEXT("FAIL "))+Reason+TEXT("\n"); if (Failures<40) { UE_LOG(LogRogue10m,Error,TEXT("WEIGHTED_FIRST_PERSON FAIL %s"),Reason); } } }
    void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
    void Key(FKey Value,bool Down) { if (PC.IsValid()) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value,Down?IE_Pressed:IE_Released,Down?1.f:0.f)); } }
    void Photo(const TCHAR* Name,float Delay=.15f)
    {
        if (!bRendered) { return; }
        const FString Path=Images/Name;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Path](float)
        { FScreenshotRequest::RequestScreenshot(Path,true,false); return false; }),Delay);
    }
    void Prepare()
    {
        PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
        auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
        auto* Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
        Check(PC.IsValid() && Profiles && Saves,TEXT("menu_profile_subsystems_available"));
        if (Failures) { Finish(); return; }
        Check(Profiles->GetProfileCount()==0,TEXT("fresh_isolated_profile_slot"));
        FRogue10mCharacterProfile Profile;
        Check(!Failures && Profiles->CreateProfile(TEXT("WeightedQa"),FRogue10mCharacterAppearance(),Profile) && Profiles->SelectProfile(Profile.ProfileId),TEXT("QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Character=Cast<ARogue10mCharacter>(PC->GetPawn());
        State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
        Check(Character.IsValid() && State.IsValid() && Saves->IsAttachedTo(*Character),TEXT("menu_enter_selected_character_attaches_real_pawn"));
        if (Failures) { Finish(); return; }
        Saves->DetachCharacter(*Character,false);
        Check(!Saves->IsAttachedTo(*Character),TEXT("fixture_detached_before_transient_changes"));
        Appearance=Character->GetAppearanceCameraComponent(); Source=Character->GetAnimationPlaybackMesh(); Body=Character->GetMesh(); Camera=Character->GetFirstPersonCameraComponent();
        Combat=Character->GetCombatComponent(); Inventory=Character->GetInventoryComponent();
        Check(Appearance.IsValid() && Source.IsValid() && Body.IsValid() && Camera.IsValid() && Combat.IsValid() && Inventory.IsValid() && Source!=Body,TEXT("production_appearance_retarget_and_combat_components"));
        if (Failures) { Finish(); return; }
        Check(FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch))<.1f,TEXT("profile_initial_pitch_zero_on_new_pawn_only"));
        InitialActor=Character->GetActorTransform(); InitialControl=PC->GetControlRotation(); InitialWeapon=Character->GetEquippedWeaponType(); InitialDamage=Character->CanBeDamaged();
        InitialAttackSpeed=Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier();
        SourceAsset=Source->GetSkeletalMeshAsset(); BodyAsset=Body->GetSkeletalMeshAsset(); SourceClass=Source->GetAnimClass(); BodyClass=Body->GetAnimClass();
        CameraFOV=Camera->FieldOfView; ArmsFOV=Camera->FirstPersonFieldOfView; ArmsScale=Camera->FirstPersonScale;
        CameraFirstFOV=Camera->bEnableFirstPersonFieldOfView; CameraFirstScale=Camera->bEnableFirstPersonScale;
        AppearanceConfig=EditableSettings(*Appearance); bPrepared=true; Character->SetCanBeDamaged(false);
        Heavy->Set(0,ECVF_SetByCode); FOV->Set(0.f,ECVF_SetByCode);
        SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float DT){if (W==Self->World.Get()) { Self->Sample(DT); }});
        GroundDeadline=FPlatformTime::Seconds()+15.; Later(.5,[this]{WaitGround();});
    }
    void WaitGround()
    {
        if (!Character->CanChangeWeapon() && FPlatformTime::Seconds()<GroundDeadline) { Later(.1,[this]{WaitGround();}); return; }
        Check(Character->CanChangeWeapon(),TEXT("grounded_idle_fixture_ready"));
        if (Failures) { Finish(); return; }
        Knuckle.Reset(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));
        Sword.Reset(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));
        for (auto* D:{Knuckle.Get(),Sword.Get()}) { D->Category=ERogue10mItemCategory::Equipment; D->EquipSlotType=ERogue10mInventorySlotType::MainWeapon; }
        Knuckle->WeaponType=ERogue10mWeaponType::Knuckle; Sword->WeaponType=ERogue10mWeaponType::GreatSword;
        int32 Index=INDEX_NONE;
        Check(Inventory->TryAddGridItem(Knuckle.Get(),1,Index,KnuckleId) && Inventory->TryAddGridItem(Sword.Get(),1,Index,SwordId),TEXT("QA_owned_transient_inventory_items_added"));
        Check(Equip(KnuckleId),TEXT("owned_knuckle_equipped"));
        Later(.8,[this]{StartProfile();});
    }
    bool Equip(FGuid Id)
    {
        const auto& Containers=Inventory->GetInventoryContainers();
        for (int32 I=0;I<Containers.Num();++I) { for (const auto& Entry:Containers[I].Entries) { if (Entry.InstanceId==Id) { return Inventory->TryEquipGridItem(I,Id); } } }
        return false;
    }
    void SettingsRanges(const FRogue10mWeightedFirstPersonSettings& S)
    {
        for (TFieldIterator<FFloatProperty> It(FRogue10mWeightedFirstPersonSettings::StaticStruct()); It; ++It)
        {
            const float V=It->GetPropertyValue_InContainer(&S);
            Check(FMath::IsFinite(V) && V>=FCString::Atof(*It->GetMetaData(TEXT("ClampMin")))-.0001f && V<=FCString::Atof(*It->GetMetaData(TEXT("ClampMax")))+.0001f,TEXT("sanitized_settings_are_finite_and_inside_declared_ranges"));
        }
    }
    void StartProfile()
    {
        ValidateView(false); SettingsRanges(Appearance->GetWeightedFirstPersonSettings());
        auto Dirty=Appearance->WeightedFirstPersonSettings;
        for (TFieldIterator<FFloatProperty> It(FRogue10mWeightedFirstPersonSettings::StaticStruct()); It; ++It) { It->SetPropertyValue_InContainer(&Dirty,std::numeric_limits<float>::quiet_NaN()); }
        SettingsRanges(Dirty.Sanitized());
        for (float Extreme:{-1.e20f,1.e20f})
        { for (TFieldIterator<FFloatProperty> It(FRogue10mWeightedFirstPersonSettings::StaticStruct()); It; ++It) { It->SetPropertyValue_InContainer(&Dirty,Extreme); } SettingsRanges(Dirty.Sanitized()); }
        Heavy->Set(1,ECVF_SetByCode);
        Motion->Set(-4.f,ECVF_SetByCode); const auto Zero=Appearance->GetWeightedFirstPersonSettings();
        Check(Zero.HeadMotionScale==0 && Zero.HandMotionScale==0,TEXT("motion_scale_lower_bound_and_zero_intensity"));
        Motion->Set(4.f,ECVF_SetByCode); SettingsRanges(Appearance->GetWeightedFirstPersonSettings()); Motion->Set(1.f,ECVF_SetByCode);
        FOV->Set(999.f,ECVF_SetByCode); FMinimalViewInfo High; Character->CalcCamera(0,High);
        Check(FMath::IsNearlyEqual(High.FOV,110.f),TEXT("FOV_console_upper_bound"));
        FOV->Set(1.f,ECVF_SetByCode); FMinimalViewInfo Low; Character->CalcCamera(0,Low);
        Check(FMath::IsNearlyEqual(Low.FOV,70.f),TEXT("FOV_console_lower_bound")); FOV->Set(0.f,ECVF_SetByCode);
        DoAimComparison(); Stage=TEXT("idle"); Later(.7,[this]
        { ValidateView(true); Check(Appearance->GetHeadMotionSettings().MaxRotationDegrees<=.751f && Appearance->GetHeadMotionSettings().MaxPositionOffsetCm<=.351f,TEXT("weighted_walking_head_budget")); Photo(TEXT("01_weighted_idle.png")); Later(.4,[this]{Walk();}); });
    }
    bool InvokeReflectedAim()
    {
        UFunction* Function=Character->FindFunction(TEXT("DoAim"));
        Check(Function!=nullptr,TEXT("actual_DoAim_UFUNCTION_is_reflected"));
        if (!Function) { return false; }
        auto* Yaw=FindFProperty<FFloatProperty>(Function,TEXT("Yaw"));
        auto* Pitch=FindFProperty<FFloatProperty>(Function,TEXT("Pitch"));
        const bool Valid=Yaw && Pitch && Yaw->HasAnyPropertyFlags(CPF_Parm) && Pitch->HasAnyPropertyFlags(CPF_Parm)
            && !Yaw->HasAnyPropertyFlags(CPF_ReturnParm) && !Pitch->HasAnyPropertyFlags(CPF_ReturnParm);
        Check(Valid,TEXT("DoAim_reflection_exposes_float_Yaw_and_Pitch_input_parameters"));
        if (!Valid) { return false; }
        FStructOnScope Params(Function);
        Yaw->SetPropertyValue_InContainer(Params.GetStructMemory(),7.f);
        Pitch->SetPropertyValue_InContainer(Params.GetStructMemory(),-4.f);
        Character->ProcessEvent(Function,Params.GetStructMemory()); return true;
    }
    void DoAimComparison()
    {
        const FRotator Before=PC->RotationInput;
        PC->RotationInput=FRotator::ZeroRotator; Heavy->Set(0,ECVF_SetByCode); const bool A=InvokeReflectedAim(); const FRotator Off=PC->RotationInput;
        PC->RotationInput=FRotator::ZeroRotator; Heavy->Set(1,ECVF_SetByCode); const bool B=InvokeReflectedAim(); const FRotator On=PC->RotationInput;
        Check(A && B && !On.ContainsNaN() && !Off.ContainsNaN() && FMath::Abs(On.Yaw)>UE_SMALL_NUMBER && FMath::Abs(On.Pitch)>UE_SMALL_NUMBER
            && On.Equals(Off,.0001f),TEXT("real_DoAim_yaw_pitch_accumulation_unchanged_by_profile"));
        Results+=FString::Printf(TEXT("aim_reflection_off=%d\naim_reflection_on=%d\naim_off_accumulated_yaw=%.9f\naim_off_accumulated_pitch=%.9f\naim_on_accumulated_yaw=%.9f\naim_on_accumulated_pitch=%.9f\n"),A,B,Off.Yaw,Off.Pitch,On.Yaw,On.Pitch);
        PC->RotationInput=Before;
    }
    void ValidateView(bool Weighted)
    {
        FMinimalViewInfo View; Character->CalcCamera(0,View); FTransform Trace;
        if (Character->IsDead())
        {
            // Death restores the original camera attachment/projection. The live appearance
            // trace and its mouse-aim snapshot are deliberately unavailable until re-entry.
            Check(!Weighted && !Appearance->IsWeightedFirstPersonActive(),TEXT("dead_boundary_disables_weighted_profile"));
            Check(!Appearance->IsAppearanceCameraActive() && !Appearance->ReadCameraTransform(Trace),TEXT("dead_boundary_restores_camera_and_releases_appearance_trace"));
            Check(!View.Location.ContainsNaN() && !View.Rotation.ContainsNaN() && FMath::IsFinite(View.FOV),TEXT("restored_dead_boundary_POV_is_finite"));
            Check(AppearanceConfig==EditableSettings(*Appearance),TEXT("dead_boundary_preserves_authored_appearance_settings"));
            return;
        }
        Check(Appearance->ReadCameraTransform(Trace),TEXT("combat_camera_transform_available"));
        const bool Inspection=Appearance->IsThirdPersonInspectionActive();
        Check(Appearance->IsWeightedFirstPersonActive()==Weighted,TEXT("profile_activation_matches_boundary"));
        Check(FMath::IsFinite(View.FOV) && FMath::IsNearlyEqual(View.FOV,Weighted?86.f:CameraFOV,.01f),TEXT("rendered_only_world_FOV_policy"));
        if (!Inspection)
        {
            Check(View.Location.Equals(Trace.GetLocation(),.001f) && View.Rotation.Equals(Trace.Rotator(),.001f)
                && Camera->GetComponentLocation().Equals(Trace.GetLocation(),.001f),TEXT("rendered_origin_rotation_match_combat_trace_camera"));
            const auto PoseBefore=Source->GetBoneSpaceTransformsView(); TArray<FTransform> Pose; Pose.Append(PoseBefore.GetData(),PoseBefore.Num());
            for (int32 I=0;I<4;++I) { FMinimalViewInfo Again; Character->CalcCamera(0,Again); Check(Again.Location.Equals(View.Location,.001f) && Again.Rotation.Equals(View.Rotation,.001f),TEXT("same_frame_CalcCamera_does_not_advance_motion")); }
            const auto After=Source->GetBoneSpaceTransformsView(); bool Same=Pose.Num()==After.Num();
            for (int32 I=0;Same && I<Pose.Num();++I) { Same &= Pose[I].Equals(After[I],.0001f); }
            Check(Same,TEXT("camera_queries_do_not_mutate_evaluated_source_pose"));
            const FRotator Control=PC->GetControlRotation(),Late(-33.f,Control.Yaw+37.f,0);
            const FQuat Residual=(Trace.GetRotation()*Control.Quaternion().Inverse()).GetNormalized();
            PC->SetControlRotation(Late); FTransform LateView; const bool Read=Appearance->ReadCameraTransform(LateView);
            Check(Read && FMath::RadiansToDegrees(LateView.GetRotation().AngularDistance((Residual*Late.Quaternion()).GetNormalized()))<.03,TEXT("late_mouse_is_applied_once_without_input_filter"));
            PC->SetControlRotation(Control); Appearance->UpdateCameraFromAppearance();
        }
        else { Check(FVector::Distance(View.Location,Trace.GetLocation())>10.f,TEXT("inspection_keeps_independent_combat_origin_and_skips_profile_FOV")); }
        Check(FMath::IsNearlyEqual(Camera->FieldOfView,CameraFOV) && FMath::IsNearlyEqual(Camera->FirstPersonFieldOfView,ArmsFOV) && FMath::IsNearlyEqual(Camera->FirstPersonScale,ArmsScale)
            && Camera->bEnableFirstPersonFieldOfView==CameraFirstFOV && Camera->bEnableFirstPersonScale==CameraFirstScale,TEXT("camera_projection_UPROPERTY_values_unchanged"));
        if (const auto* Anim=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()); Anim && Anim->bMouseDrivenHeadApplied
            && !PC->IsAnyBlockingWindowVisible() && !Inspection && !Character->IsDead())
        {
            const FQuat ExpectedAim=(PC->GetControlRotation().Quaternion()*Character->GetActorQuat().Inverse()).GetNormalized();
            Check(FMath::RadiansToDegrees(Anim->MouseDrivenHeadWorldAim.AngularDistance(ExpectedAim))<.1
                && FMath::RadiansToDegrees(Anim->MouseDrivenHeadSourceRotation.AngularDistance(Source->GetComponentQuat()))<.1,TEXT("evaluated_mouse_aim_and_source_basis_match_current_avatar"));
        }
    }
    void Walk()
    {
        Stage=TEXT("walk"); MoveOrigin=Character->GetActorLocation(); Key(EKeys::W,true);
        Later(.4,[this]
        {
            WalkCm=FVector::Dist2D(MoveOrigin,Character->GetActorLocation()); Check(WalkCm>5,TEXT("programmatic_W_InputKey_moves_actual_pawn")); ValidateView(true); Photo(TEXT("02_weighted_walk.png"));
            Stage=TEXT("sprint"); Key(EKeys::LeftShift,true);
            Later(.4,[this]
            { Check(Character->IsSprinting(),TEXT("programmatic_shift_enters_actual_sprint")); Check(Appearance->GetHeadMotionSettings().MaxRotationDegrees<=1.251f && Appearance->GetHeadMotionSettings().MaxPositionOffsetCm<=.651f,TEXT("weighted_running_head_budget")); Photo(TEXT("03_weighted_sprint.png")); Later(.25,[this]{Key(EKeys::W,false); Key(EKeys::LeftShift,false); Character->GetCharacterMovement()->StopMovementImmediately(); Later(.7,[this]{Attack();});}); });
        });
    }
    void Attack()
    {
        Stage=TEXT("attack"); Skill=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        Check(Skill.IsValid() && Skill->AttackMontage,TEXT("authored_knuckle_primary_and_montage_available"));
        if (Failures) { Finish(); return; }
        Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(FixtureAttackSpeed);
        SkillConfig=EditableSettings(*Skill); ExpectedRate=FMath::Max(.01f,FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f)*Skill->AnimationPlayRate);
        UClass* Class=LoadClass<ARogue10mBasicMonster>(nullptr,TEXT("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_ThornWarden.BP_OW_ThornWarden_C"));
        const FTransform At(Character->GetActorRotation(),Character->GetActorLocation()+Character->GetActorForwardVector()*135.f);
        Target=Class?World->SpawnActorDeferred<ARogue10mBasicMonster>(Class,At,PC.Get(),Character.Get(),ESpawnActorCollisionHandlingMethod::AlwaysSpawn):nullptr;
        Check(Target.IsValid(),TEXT("marked_authored_damage_target_spawned")); if (Failures) { Finish(); return; }
        Target->MarkCombatTestInstance(); UGameplayStatics::FinishSpawningActor(Target.Get(),At);
        if (auto* AI=Cast<ARogue10mMonsterAIController>(Target->GetController())) { AI->StopMonsterAI(); }
        Target->GetCharacterMovement()->DisableMovement(); Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Target->GetRogueAttributeSet()->SetMaxHealth(10000); Target->GetRogueAttributeSet()->SetHealth(10000);
        if (auto* Regen=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
        Character->GetRogueAttributeSet()->RestoreVitals();
        Later(.7,[this]{UsesBefore=State->GetSkillUseCount(Skill->GetFName()); bObserveAttack=true; Key(EKeys::LeftMouseButton,true); Later(.1,[this]{Key(EKeys::LeftMouseButton,false); GroundDeadline=FPlatformTime::Seconds()+15.; Later(.15,[this]{WaitAttack();});});});
    }
    void WaitAttack()
    {
        if ((!Character->CanChangeWeapon() || Combat->GetAttackCooldownRemaining()>.01f) && FPlatformTime::Seconds()<GroundDeadline) { Later(.1,[this]{WaitAttack();}); return; }
        bObserveAttack=false;
        Check(State->GetSkillUseCount(Skill->GetFName())==UsesBefore+1,TEXT("programmatic_LMB_accepts_exactly_one_real_knuckle_attack"));
        Damage=10000-Target->GetRogueAttributeSet()->GetHealth();
        Check(Damage>0 && AcceptedAt>=0 && FirstDamageAt>=0,TEXT("actual_knuckle_attack_hits_authored_target"));
        const bool ValidIntervals=FMath::IsFinite(AcceptedObservationDelta) && AcceptedObservationDelta>=0.
            && FMath::IsFinite(FirstDamageFrameDelta) && FirstDamageFrameDelta>=0.
            && FMath::IsFinite(PreviousDamageFrameDelta) && PreviousDamageFrameDelta>=0.;
        Check(ValidIntervals,TEXT("attack_timing_observation_intervals_are_finite_and_nonnegative"));
        FirstDamageTimingError=FirstDamageAt-AcceptedAt-Skill->HitStartDelaySeconds/ExpectedRate;
        // TimerManager and synchronous damage run before this post-actor observer.
        // Only the crossing damage frame quantizes a late observation; approval
        // observation quantization affects the early bound, not the late bound.
        AllowedEarlyTimingError=-(AcceptedObservationDelta+.001);
        AllowedLateTimingError=FirstDamageFrameDelta+.001;
        Check(MontageSamples>=2 && ValidIntervals && FMath::IsFinite(FirstDamageTimingError)
            && FirstDamageTimingError>=AllowedEarlyTimingError && FirstDamageTimingError<=AllowedLateTimingError,
            TEXT("authored_montage_rate_and_first_hit_timing_preserved"));
        Check(SkillConfig==EditableSettings(*Skill),TEXT("attack_timing_playrate_cooldown_resource_data_unchanged"));
        // The close target belongs to the hit probe only. Keep later viewport evidence unobstructed.
        Target->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*450.f
            +Character->GetActorRightVector()*100.f,false,nullptr,ETeleportType::TeleportPhysics);
        Stage=TEXT("attack_recovered"); ValidateView(true); Photo(TEXT("04_weighted_attack_recovered.png"));
        Later(.6,[this]{MenuBoundary();});
    }
    void MenuBoundary()
    {
        Stage=TEXT("menu"); PC->ToggleSettings();
        Check(PC->IsSettingsVisible() && PC->IsAnyBlockingWindowVisible(),TEXT("settings_menu_opens_on_real_controller"));
        const int32 Uses=State->GetSkillUseCount(Skill->GetFName()); Key(EKeys::LeftMouseButton,true);
        Later(.3,[this,Uses]
        {
            Key(EKeys::LeftMouseButton,false);
            Check(State->GetSkillUseCount(Skill->GetFName())==Uses,TEXT("menu_blocks_programmatic_attack_input"));
            ValidateView(true); Photo(TEXT("05_settings_boundary.png"));
            Later(.6,[this]
            {
                PC->ToggleSettings(); Check(!PC->IsAnyBlockingWindowVisible(),TEXT("menu_closes_without_stale_blocking_state"));
                Later(.4,[this]{Inspection();});
            });
        });
    }
    void Inspection()
    {
        Stage=TEXT("inspection"); Key(EKeys::V,true); Key(EKeys::V,false);
        Later(.4,[this]
        {
            Check(Appearance->IsThirdPersonInspectionActive(),TEXT("real_V_InputKey_enters_inspection"));
            ValidateView(false); Photo(TEXT("06_inspection_profile_skipped.png"));
            Later(.6,[this]{Key(EKeys::V,true); Key(EKeys::V,false); Later(.4,[this]{WeaponBoundary();});});
        });
    }
    void WeaponBoundary()
    {
        Check(!Appearance->IsThirdPersonInspectionActive(),TEXT("V_returns_to_first_person"));
        State->SetCharacterDead(true); ValidateView(false); State->SetCharacterDead(false); ValidateView(true);
        Stage=TEXT("weapon"); Check(Equip(SwordId),TEXT("actual_inventory_switch_to_owned_greatsword"));
        Later(.7,[this]
        {
            ValidateView(true); Photo(TEXT("07_greatsword_boundary.png"));
            Later(.6,[this]
            {
                Check(Equip(KnuckleId),TEXT("actual_inventory_returns_to_owned_knuckle"));
                Later(.8,[this]{RestoreProfile();});
            });
        });
    }
    void RestoreProfile()
    {
        Stage=TEXT("restored"); Heavy->Set(0,ECVF_SetByCode); Motion->Set(1.f,ECVF_SetByCode); FOV->Set(0.f,ECVF_SetByCode);
        Later(.8,[this]
        {
            ValidateView(false);
            Check(AppearanceConfig==EditableSettings(*Appearance),TEXT("profile_toggles_preserve_all_authored_appearance_settings"));
            Check(Source->GetSkeletalMeshAsset()==SourceAsset.Get() && Body->GetSkeletalMeshAsset()==BodyAsset.Get() && Source->GetAnimClass()==SourceClass.Get() && Body->GetAnimClass()==BodyClass.Get(),TEXT("original_source_retarget_assets_and_animation_classes_retained"));
            if (auto* Anim=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance())) { Check(!Anim->bWeightedHandMotionApplied && Anim->WeightedHandPitch.IsNearlyZero(.001f),TEXT("weighted_hand_snapshot_clears_after_profile_off")); }
            Check(HandSamples>0,TEXT("weighted_hand_pose_was_evaluated_on_actual_source"));
            Photo(TEXT("08_original_profile_restored.png")); Later(.7,[this]{Finish();});
        });
    }
    void Sample(float DeltaSeconds)
    {
        if (bFinished || !Character.IsValid() || !Source.IsValid() || !Body.IsValid()) { return; }
        ++Frame;
        const double ObservedWorldTime=World->GetTimeSeconds();
        const double SampleDelta=LastSampleWorldTime>=0.?ObservedWorldTime-LastSampleWorldTime:static_cast<double>(DeltaSeconds);
        auto* Anim=Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance());
        FTransform SourceHead,BodyHead;
        Check(RawHead(Source.Get(),SourceHead) && RawHead(Body.Get(),BodyHead),TEXT("actual_source_and_visible_appearance_head_are_finite_normalized"));
        Check(Anim && !Anim->MouseDrivenHeadWorldAim.ContainsNaN() && Anim->MouseDrivenHeadWorldAim.IsNormalized()
            && !Anim->MouseDrivenHeadSourceRotation.ContainsNaN() && Anim->MouseDrivenHeadSourceRotation.IsNormalized(),TEXT("evaluated_source_mouse_head_snapshot_is_valid"));
        const bool WantAim=!PC->IsAnyBlockingWindowVisible() && !Character->IsDead() && !Appearance->IsThirdPersonInspectionActive();
        if (WantAim && bPreviousAimExpected && Frame>4) { Check(Anim && Anim->bMouseDrivenHeadApplied,TEXT("active_gameplay_source_publishes_evaluated_mouse_aim")); }
        bPreviousAimExpected=WantAim;
        if (Anim && Anim->bWeightedHandMotionApplied) { ++HandSamples; Check(!Anim->WeightedHandPitch.ContainsNaN() && FMath::Abs(Anim->WeightedHandPitch.X)<=21.1f && FMath::Abs(Anim->WeightedHandPitch.Y)<=21.1f,TEXT("evaluated_weighted_hand_pitch_is_finite_and_bounded")); }
        float Position=-1,Rate=0;
        if (bObserveAttack && Skill.IsValid())
        {
            if (AcceptedAt<0 && State->GetSkillUseCount(Skill->GetFName())>UsesBefore)
            {
                AcceptedAt=ObservedWorldTime; AcceptedObservationDelta=SampleDelta;
                Photo(TEXT("09_actual_knuckle_attack.png"),0.f);
                Check(Appearance->GetHeadMotionSettings().MaxRotationDegrees<=1.001f && Appearance->GetHeadMotionSettings().MaxPositionOffsetCm<=.451f,TEXT("weighted_attacking_head_budget"));
            }
            if (Source->GetAnimInstance()) { if (const auto* Instance=Source->GetAnimInstance()->GetInstanceForMontage(Skill->AttackMontage)) { if (Instance->GetWeight()>UE_SMALL_NUMBER) { ++MontageSamples; Position=Instance->GetPosition(); Rate=Instance->GetPlayRate(); Check(FMath::Abs(Rate-ExpectedRate)<.001f,TEXT("real_attack_montage_playrate_is_authored_rate")); } } }
            if (Target.IsValid() && FirstDamageAt<0 && Target->GetRogueAttributeSet()->GetHealth()<9999.99f)
            {
                FirstDamageAt=ObservedWorldTime; FirstDamageFrameDelta=SampleDelta; PreviousDamageFrameDelta=PreviousSampleDelta;
                ContactHandPitch=Anim?Anim->WeightedHandPitch:FVector2D::ZeroVector;
                Check(Anim && ContactHandPitch.IsNearlyZero(.05f),TEXT("actual_first_damage_frame_keeps_worker_published_hand_offset_zero"));
            }
        }
        FMinimalViewInfo View; Character->CalcCamera(0,View); const auto Budget=Appearance->GetHeadMotionSettings(); const FRotator Control=PC->GetControlRotation();
        Check(FMath::IsFinite(DeltaSeconds) && FMath::IsFinite(Budget.MaxRotationDegrees) && FMath::IsFinite(Budget.MaxPositionOffsetCm),TEXT("runtime_head_budget_is_finite"));
        Frames+=FString::Printf(TEXT("%d,%s,%.9f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f\n"),Frame,*Stage,World->GetTimeSeconds(),Appearance->IsWeightedFirstPersonActive(),Appearance->IsThirdPersonInspectionActive(),Character->IsDead(),Character->GetVelocity().Size2D(),Control.Pitch,Control.Yaw,View.FOV,Budget.MaxRotationDegrees,Budget.MaxPositionOffsetCm,Anim?Anim->WeightedHandPitch.X:0.,Anim?Anim->WeightedHandPitch.Y:0.,Anim?Anim->bWeightedHandMotionApplied:false,Position,Rate,Target.IsValid()?Target->GetRogueAttributeSet()->GetHealth():0.f);
        LastSampleWorldTime=ObservedWorldTime; PreviousSampleDelta=SampleDelta;
    }
    void Finish(bool Exit=true)
    {
        if (bFinished) { return; } bFinished=true; Next=nullptr;
        FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        Key(EKeys::W,false); Key(EKeys::LeftShift,false); Key(EKeys::LeftMouseButton,false); Key(EKeys::V,false);
        if (bPrepared && Character.IsValid())
        {
            State->SetCharacterDead(false); PC->CloseAllBlockingPanels(); Combat->CancelBoundSkillInput(); Combat->CancelCombatVisuals();
            if (Appearance->IsThirdPersonInspectionActive()) { Appearance->ToggleThirdPersonInspection(); }
            Character->GetCharacterMovement()->StopMovementImmediately(); Character->SetEquippedWeaponType(InitialWeapon);
            Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(InitialAttackSpeed);
            Character->SetCanBeDamaged(InitialDamage); Character->SetActorTransform(InitialActor,false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(InitialControl);
        }
        if (Target.IsValid()) { Target->Destroy(); }
        if (bOwnCVars) { Heavy->Set(OldHeavy,ECVF_SetByCode); Motion->Set(OldMotion,ECVF_SetByCode); FOV->Set(OldFOV,ECVF_SetByCode); }
        Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))),TEXT("frame_evidence_saved"));
        Results+=FString::Printf(TEXT("first_damage_allowed_early_s=%.9f\nfirst_damage_allowed_late_s=%.9f\naccepted_observation_delta_s=%.9f\nfirst_damage_frame_delta_s=%.9f\nprevious_damage_frame_delta_s=%.9f\n"),AllowedEarlyTimingError,AllowedLateTimingError,AcceptedObservationDelta,FirstDamageFrameDelta,PreviousDamageFrameDelta);
        Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nframes=%d\nrendered=%d\nwalk_cm=%.6f\nactual_target_damage=%.6f\nattack_speed_fixture=%.6f\nattack_montage_samples=%d\nweighted_hand_samples=%d\ncontact_hand_pitch_l=%.9f\ncontact_hand_pitch_r=%.9f\nfirst_damage_error_s=%.9f\ninput=programmatic_InputKey_and_DoAim\nNOTE=Fresh QA profile, progression detached before transient items; authored attack data and clock are not edited. Attack-speed attribute is explicit fixture tuning and restored. AI-disabled marked target; reversible death flag, not full death/run/balance validation. Viewport PNGs require visual readability review.\n"),Checks,Failures,Frame,bRendered,WalkCm,Damage,FixtureAttackSpeed,MontageSamples,HandSamples,ContactHandPitch.X,ContactHandPitch.Y,FirstDamageTimingError);
        if (!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt")))) { ++Failures; }
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=WEIGHTED_FIRST_PERSON_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);
        if (Exit) { FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f); }
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
    TWeakObjectPtr<USkeletalMeshComponent> Source,Body;
    TWeakObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<USkeletalMesh> SourceAsset,BodyAsset;
    TWeakObjectPtr<UClass> SourceClass,BodyClass;
    TWeakObjectPtr<URogue10mAttackSkillData> Skill;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TStrongObjectPtr<URogue10mItemDataAsset> Knuckle,Sword;
    FGuid KnuckleId,SwordId;
    FTSTicker::FDelegateHandle Ticker;
    FDelegateHandle SampleHandle,Cleanup;
    TFunction<void()> Next;
    IConsoleVariable* Heavy=nullptr; IConsoleVariable* Motion=nullptr; IConsoleVariable* FOV=nullptr;
    FString Label,Output,Images,Results,Frames,AppearanceConfig,SkillConfig,Stage=TEXT("baseline");
    FTransform InitialActor;
    FRotator InitialControl;
    FVector MoveOrigin;
    FVector2D ContactHandPitch=FVector2D::ZeroVector;
    ERogue10mWeaponType InitialWeapon=ERogue10mWeaponType::Knuckle;
    double NextAt=0,Deadline=0,GroundDeadline=0,AcceptedAt=-1,FirstDamageAt=-1;
    double LastSampleWorldTime=-1,PreviousSampleDelta=0,AcceptedObservationDelta=-1,FirstDamageFrameDelta=-1,PreviousDamageFrameDelta=-1;
    double FirstDamageTimingError=0,AllowedEarlyTimingError=0,AllowedLateTimingError=0;
    float OldMotion=1,OldFOV=0,CameraFOV=90,ArmsFOV=80,ArmsScale=.6f,ExpectedRate=1,WalkCm=0,Damage=0,InitialAttackSpeed=1,FixtureAttackSpeed=1;
    int32 OldHeavy=0,Checks=0,Failures=0,Frame=0,UsesBefore=0,MontageSamples=0,HandSamples=0;
    bool bFinished=false,bRendered=false,bPrepared=false,bOwnCVars=false,bObserveAttack=false,bPreviousAimExpected=false,InitialDamage=true,CameraFirstFOV=false,CameraFirstScale=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine || GIsEditor || (Active.IsValid() && !Active->Finished())) { return; }
    for (const auto& Context:GEngine->GetWorldContexts()) { if (UWorld* W=Context.World(); W && W->WorldType==EWorldType::Game) { Active=MakeShared<FRun>(); Active->Start(W); return; } }
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=WEIGHTED_FIRST_PERSON_FAILED reason=no_standalone_game_world")); FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestWeightedFirstPerson"),TEXT("Fresh Heavy QA slots only: optional weighted presentation, real programmatic InputKey/DoAim, authored knuckle hit, camera/trace, menu/inspection/death/weapon restoration. -RogueWeightedFPLabel=label. NullRHI supported."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
