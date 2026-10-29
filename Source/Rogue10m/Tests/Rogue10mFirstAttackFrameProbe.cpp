// Opt-in, fresh-profile QA only. Measures process-first-use, never a cleared-cache benchmark.
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerInputDebugging.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
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
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace Rogue10mFirstAttackFramesQA
{
enum class EEvent : uint8 { BeginFrame, EndFrame, WorldPre, WorldPost, InputRequest, InputRelease, Flush, ViewportInput };
enum class EStage : uint8 { Setup, Settle, QuietBeforeInput, Pressed, Observe, BetweenTrials, Complete };
static const TCHAR* Name(EEvent E)
{
    switch (E) { case EEvent::BeginFrame:return TEXT("begin_frame"); case EEvent::EndFrame:return TEXT("end_frame");
    case EEvent::WorldPre:return TEXT("world_pre"); case EEvent::WorldPost:return TEXT("world_post");
    case EEvent::InputRequest:return TEXT("input_request"); case EEvent::InputRelease:return TEXT("input_release"); case EEvent::ViewportInput:return TEXT("viewport_input"); default:return TEXT("input_flushed"); }
}
static FString EditableSettings(const UObject& Object)
{
    FString Result;
    for (TFieldIterator<FProperty> It(Object.GetClass());It;++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
        FString Value; It->ExportTextItem_Direct(Value,It->ContainerPtrToValuePtr<void>(&Object),nullptr,nullptr,PPF_None);
        Result+=It->GetName()+TEXT("=")+Value+TEXT("\n");
    }
    return Result;
}
struct FRow
{
    uint64 Frame=0, Flushes=0, ViewportEvents=0;
    EEvent Event=EEvent::WorldPost;
    EStage Stage=EStage::Setup;
    int32 Trial=INDEX_NONE, Uses=0;
    double Wall=0, World=0, BeginGap=0, BeginIntervalStart=0, PostGap=0, ControlDeviation=0;
    FName ViewportKey=NAME_None; int32 ViewportEvent=INDEX_NONE; float ViewportAmount=0, ControlPitch=0, ControlYaw=0, ControlRoll=0;
    float WorldDT=0, Health=0, Montage=-1, Rate=0;
    bool bLMB=false, bBlocking=false, bWeighted=false, bInspection=false, bViewportSimulated=false, bBeginIntervalContained=false;
};
struct FTrial
{
    int32 UsesBefore=0, UsesAfter=0, MontageSamples=0, FrameSamples=0, CompleteIntervals=0, BoundaryIntervals=0, WallHitches50ms=0;
    int32 ViewportNonSimulated=0, ViewportLMBReleases=0, ControlDeviationSamples=0;
    uint64 FlushStart=0, FlushEnd=0, ViewportStart=0, ViewportEnd=0, RequestFrame=0, AcceptFrame=0, DamageFrame=0;
    FRotator ControlBaseline=FRotator::ZeroRotator;
    double WindowStart=0, WindowEnd=0, RequestWall=-1, RequestWorld=-1, InputCallSeconds=0;
    double AcceptWall=-1, AcceptWorld=-1, DamageWall=-1, DamageWorld=-1;
    double AcceptDelta=-1, DamageDelta=-1, PreviousDamageDelta=-1, TimingError=0, AllowedEarly=0, AllowedLate=0;
    double MaxBeginGap=0, MaxBoundaryBeginGap=0, MaxWorldDT=0, Damage=0, MaxControlDeviation=0;
    bool bContaminated=false, bComplete=false, bRateCorrect=true, bProfileCorrect=true, bControlInvalid=false;
};
class FRun : public TSharedFromThis<FRun>
{
public:
    bool Finished() const { return bFinished; }
    void Start(UWorld& InWorld)
    {
        World=&InWorld; const TCHAR* Args=FCommandLine::Get(); FString Town,Prefix,Keys;
        FParse::Value(Args,TEXT("RogueFirstAttackLabel="),Label); FParse::Value(Args,TEXT("RogueFirstAttackProfile="),Profile);
        FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town); FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix); FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
        bool bLabel=!Label.IsEmpty() && Label.Len()<=24;
        for (TCHAR C:Label) { bLabel&=(C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_'); }
        TArray<FString> Existing; IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
        if (!bLabel || (Profile!=TEXT("Original") && Profile!=TEXT("Weighted")) || InWorld.GetNetMode()!=NM_Standalone
            || Town!=TEXT("Rogue10m_Town_QA_FpAttack_")+Label || Prefix!=TEXT("Rogue10m_Progression_QA_FpAttack_")+Label || Keys!=TEXT("Rogue10m_Fighter_QA_FpAttack_")+Label
            || UGameplayStatics::DoesSaveGameExist(Town,0) || UGameplayStatics::DoesSaveGameExist(Keys,0) || !Existing.IsEmpty())
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_ATTACK_FRAMES_FAILED reason=exact_fresh_profile_namespace_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/first-attack-frames-20261029")/Label;
        if (IFileManager::Get().DirectoryExists(*Output)) { UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_ATTACK_FRAMES_FAILED reason=fresh_evidence_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        bWeighted=Profile==TEXT("Weighted"); bRendered=!FParse::Param(Args,TEXT("nullrhi")); bOffscreen=FParse::Param(Args,TEXT("RenderOffScreen"));
        IFileManager::Get().MakeDirectory(*Output,true); Rows.Reserve(20000); Deadline=FPlatformTime::Seconds()+150.;
        Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }),.01f);
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool){if (W==Self->World.Get()&&!Self->bFinished) { Self->Check(false,TEXT("world_cleanup_before_completion")); Self->Finish(false); }});
        Later(3.,[this]{Prepare();});
    }
private:
    void Check(bool bOK,const TCHAR* Reason)
    {
        ++Checks;
        // Buffer failures too: no QA logging/disk writes in an attack observation window.
        if (!bOK) { ++Failures; FailureReasons.Add(Reason); }
    }
    void Later(double Delay,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(Work); }
    void Key(bool bDown) { if (PC.IsValid()) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,bDown?IE_Pressed:IE_Released,bDown?1.f:0.f)); } }
    void Prepare()
    {
        PC=World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* GI=World.IsValid()?World->GetGameInstance():nullptr; auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
        Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
        Check(PC.IsValid() && Profiles && Saves,TEXT("normal_profile_controller_and_progression_available")); if (Failures) { Finish(); return; }
        Check(Profiles->GetProfileCount()==0,TEXT("fresh_QA_profile_has_no_existing_characters")); FRogue10mCharacterProfile NewProfile;
        Check(!Failures && Profiles->CreateProfile(TEXT("AttackQa"),FRogue10mCharacterAppearance(),NewProfile) && Profiles->SelectProfile(NewProfile.ProfileId),TEXT("new_QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; } PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Character=Cast<ARogue10mCharacter>(PC->GetPawn()); State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
        Check(Character.IsValid() && State.IsValid() && Saves->IsAttachedTo(*Character),TEXT("normal_entry_has_real_attached_player_pawn")); if (Failures) { Finish(); return; }
        Saves->DetachCharacter(*Character,false); Check(!Saves->IsAttachedTo(*Character),TEXT("autosave_detached_before_all_transient_QA_changes"));
        Combat=Character->GetCombatComponent(); Source=Character->GetAnimationPlaybackMesh(); Appearance=Character->GetAppearanceCameraComponent();
        Check(Combat.IsValid() && Source.IsValid() && Source->GetAnimInstance() && Appearance.IsValid() && Character->GetRogueAttributeSet()
            && Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle,TEXT("authored_starting_knuckle_and_actual_source_animation_available"));
        if (Failures) { Finish(); return; }
        Skill=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        Check(Skill.IsValid() && Skill->AttackMontage && State->GetSkillUseCount(Skill->GetFName())==0 && !Combat->GetLastAcceptedAttackSkill(),TEXT("authored_primary_is_first_unplayed_attack_of_this_process"));
        if (Failures) { Finish(); return; }
        SkillFields=EditableSettings(*Skill); InitialTransform=Character->GetActorTransform(); InitialControl=PC->GetControlRotation(); bInitialDamage=Character->CanBeDamaged();
        InitialAttackSpeed=Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(); bPrepared=true; Character->SetCanBeDamaged(false);
        Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(1.f); Character->GetRogueAttributeSet()->RestoreVitals();
        ExpectedRate=FMath::Max(.01f,FMath::Clamp(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),.1f,5.f)*Skill->AnimationPlayRate);
        ExpectedDelay=Skill->HitStartDelaySeconds/ExpectedRate;
        Check(FMath::IsFinite(ExpectedRate) && ExpectedRate>0. && FMath::IsFinite(ExpectedDelay) && ExpectedDelay>=0.,TEXT("authored_rate_and_first_hit_delay_are_finite_and_nonnegative"));
        Heavy=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy")); Motion=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyMotionScale")); Fov=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavyFOV"));
        Check(Heavy && Motion && Fov,TEXT("presentation_controls_registered")); if (Failures) { Finish(); return; }
        OldHeavy=Heavy->GetInt(); OldMotion=Motion->GetFloat(); OldFov=Fov->GetFloat(); bOwnCVars=true;
        PC->ToggleSettings(); Check(PC->RestoreOriginalFirstPersonPresentation(),TEXT("profile_restored_through_real_settings_API"));
        if (bWeighted) { Check(PC->SetWeightedFirstPersonEnabled(true) && PC->SetWeightedFirstPersonMotionScale(1.f),TEXT("weighted_profile_enabled_through_real_settings_API")); }
        PC->CloseAllBlockingPanels();
        Check(Appearance->IsWeightedFirstPersonActive()==bWeighted,TEXT("requested_profile_is_actual_runtime_profile"));
#if UE_PLAYER_INPUT_INCLUDE_DEBUG
        FlushHandle=UE::Input::FPlayerInputDebugging::OnPlayerInputFlushed.AddLambda([Self=AsShared()](const UPlayerInput* Input)
        { if (Self->PC.IsValid() && Input==Self->PC->PlayerInput) { ++Self->Flushes; Self->Record(EEvent::Flush,0.f); } });
        bFlushHook=true;
#endif
        Check(bFlushHook,TEXT("official_engine_input_flush_integrity_hook_available"));
        Viewport=World->GetGameViewport();
        Check(Viewport.IsValid(),TEXT("own_world_viewport_available_for_input_integrity_observer"));
        if (Viewport.IsValid())
        {
            ViewportHandle=Viewport->OnInputKey().AddLambda([Self=AsShared()](const FInputKeyEventArgs& Args)
            {
                // Direct PC->InputKey fixture events bypass this viewport delegate. Do not
                // filter releases/repeats/other keys or heal them by re-injecting held input.
                if (!Self->bWindow || Self->bFinished) { return; }
                ++Self->ViewportEvents; auto& T=Self->Trials[Self->TrialIndex];
                T.ViewportNonSimulated+=Args.IsSimulatedInput()?0:1;
                T.ViewportLMBReleases+=(Args.Key==EKeys::LeftMouseButton && Args.Event==IE_Released)?1:0;
                Self->Record(EEvent::ViewportInput,0.f,&Args);
            });
            bViewportObserver=true;
        }
        BeginHandle=FCoreDelegates::OnBeginFrame.AddLambda([Self=AsShared()]{Self->Record(EEvent::BeginFrame,0.f);});
        EndHandle=FCoreDelegates::OnEndFrame.AddLambda([Self=AsShared()]{Self->Record(EEvent::EndFrame,0.f);});
        PreHandle=FWorldDelegates::OnWorldPreActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float DT){if (W==Self->World.Get()) { Self->Record(EEvent::WorldPre,DT); }});
        PostHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float DT){if (W==Self->World.Get()) { Self->Record(EEvent::WorldPost,DT); }});
        Stage=EStage::Settle; GroundDeadline=FPlatformTime::Seconds()+15.; Later(.7,[this]{WaitGround();});
    }
    void WaitGround()
    {
        if (!Character->CanChangeWeapon() && FPlatformTime::Seconds()<GroundDeadline) { Later(.1,[this]{WaitGround();}); return; }
        Check(Character->CanChangeWeapon() && !PC->IsAnyBlockingWindowVisible(),TEXT("real_pawn_is_grounded_idle_after_menu_transition"));
        UClass* Class=LoadClass<ARogue10mBasicMonster>(nullptr,TEXT("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_ThornWarden.BP_OW_ThornWarden_C"));
        const FTransform At(Character->GetActorRotation(),Character->GetActorLocation()+Character->GetActorForwardVector()*135.f);
        Target=Class?World->SpawnActorDeferred<ARogue10mBasicMonster>(Class,At,PC.Get(),Character.Get(),ESpawnActorCollisionHandlingMethod::AlwaysSpawn):nullptr;
        Check(Target.IsValid(),TEXT("authored_marked_target_spawned_before_measurement")); if (Failures) { Finish(); return; }
        Target->MarkCombatTestInstance(); UGameplayStatics::FinishSpawningActor(Target.Get(),At);
        if (auto* AI=Cast<ARogue10mMonsterAIController>(Target->GetController())) { AI->StopMonsterAI(); }
        Target->GetCharacterMovement()->DisableMovement(); Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Target->GetRogueAttributeSet()->SetMaxHealth(10000.f); Target->GetRogueAttributeSet()->SetHealth(10000.f);
        if (auto* Regen=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
        Later(.7,[this]{BeginTrial(0);});
    }
    bool ReadyForSameSkill() const
    {
        return Character.IsValid() && Combat.IsValid() && Character->CanChangeWeapon() && !PC->IsAnyBlockingWindowVisible()
            && Combat->GetAttackCooldownRemaining()<=.001f && !Combat->IsComboSequenceActive(World->GetTimeSeconds());
    }
    void BeginTrial(int32 Index)
    {
        Check(ReadyForSameSkill() && Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)==Skill.Get(),TEXT("trial_starts_after_action_cooldown_and_combo_with_same_authored_skill"));
        Check(Appearance->IsWeightedFirstPersonActive()==bWeighted && !Appearance->IsThirdPersonInspectionActive(),TEXT("requested_first_person_profile_is_active_at_each_trial_start"));
        Check(EditableSettings(*Skill)==SkillFields && FMath::IsNearlyEqual(Character->GetRogueAttributeSet()->GetAttackSpeedMultiplier(),1.f),TEXT("authored_data_and_speed_one_unchanged_before_trial"));
        if (Failures) { Finish(); return; }
        Character->GetRogueAttributeSet()->RestoreVitals(); Character->GetCharacterMovement()->StopMovementImmediately(); PC->SetControlRotation(InitialControl);
        Target->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*135.f,false,nullptr,ETeleportType::TeleportPhysics);
        Target->GetRogueAttributeSet()->SetHealth(10000.f);
        TrialIndex=Index; auto& T=Trials[Index]; T.UsesBefore=State->GetSkillUseCount(Skill->GetFName()); T.FlushStart=Flushes; T.ViewportStart=ViewportEvents; T.ControlBaseline=PC->GetControlRotation(); T.WindowStart=FPlatformTime::Seconds();
        Stage=EStage::QuietBeforeInput; bWindow=true;
        Later(.5,[this]
        {
            auto& T=Trials[TrialIndex]; T.RequestWall=FPlatformTime::Seconds(); T.RequestWorld=World->GetTimeSeconds(); T.RequestFrame=GFrameCounter;
            Stage=EStage::Pressed; Record(EEvent::InputRequest,0.f); const double Before=FPlatformTime::Seconds(); Key(true); T.InputCallSeconds=FPlatformTime::Seconds()-Before;
            TrialDeadline=FPlatformTime::Seconds()+15.;
            Later(.1,[this]{Key(false); Record(EEvent::InputRelease,0.f); Stage=EStage::Observe; Later(.05,[this]{WaitTrial();});});
        });
    }
    void WaitTrial()
    {
        auto& T=Trials[TrialIndex]; const double Now=FPlatformTime::Seconds();
        if ((T.DamageWall<0. || Now<T.DamageWall+1. || !ReadyForSameSkill()) && Now<TrialDeadline) { Later(.05,[this]{WaitTrial();}); return; }
        T.WindowEnd=Now; T.FlushEnd=Flushes; T.ViewportEnd=ViewportEvents; T.bContaminated=T.FlushEnd!=T.FlushStart || T.ViewportEnd!=T.ViewportStart || T.ControlDeviationSamples>0 || T.bControlInvalid; T.UsesAfter=State->GetSkillUseCount(Skill->GetFName());
        T.Damage=10000.-Target->GetRogueAttributeSet()->GetHealth();
        Check(T.UsesAfter==T.UsesBefore+1 && Combat->GetLastAcceptedAttackSkill()==Skill.Get(),TEXT("exactly_one_real_accepted_attack_uses_same_authored_primary"));
        Check(T.AcceptWorld>=0. && T.DamageWorld>=0. && T.Damage>0.,TEXT("accepted_attack_deals_real_target_damage"));
        const bool bIntervals=FMath::IsFinite(T.AcceptDelta) && T.AcceptDelta>=0. && FMath::IsFinite(T.DamageDelta) && T.DamageDelta>=0.;
        T.TimingError=T.DamageWorld-T.AcceptWorld-ExpectedDelay; T.AllowedEarly=-(T.AcceptDelta+.001); T.AllowedLate=T.DamageDelta+.001;
        Check(bIntervals && T.MontageSamples>=2 && T.bRateCorrect && FMath::IsFinite(T.TimingError) && T.TimingError>=T.AllowedEarly && T.TimingError<=T.AllowedLate,TEXT("unchanged_authored_montage_rate_and_first_hit_time_oracle"));
        Check(EditableSettings(*Skill)==SkillFields && ReadyForSameSkill(),TEXT("trial_finishes_after_full_recovery_without_authored_data_change"));
        Check(T.bProfileCorrect && Appearance->IsWeightedFirstPersonActive()==bWeighted && !Appearance->IsThirdPersonInspectionActive(),TEXT("actual_requested_first_person_profile_held_throughout_observed_world_frames"));
        Check(T.FrameSamples>0 && T.CompleteIntervals>0 && T.CompleteIntervals+T.BoundaryIntervals==T.FrameSamples && FMath::IsFinite(T.MaxBeginGap) && T.MaxBeginGap>=0.,TEXT("finite_fully_contained_frame_intervals_exist_without_performance_threshold"));
        T.bComplete=true; bWindow=false; Stage=EStage::BetweenTrials;
        // Preserve real clocks: the repeat starts >2s after the first fully recovered trial.
        if (TrialIndex==0 && !Failures) { Later(2.1,[this]{BeginTrial(1);}); } else { Finish(); }
    }
    void Record(EEvent Event,float DT,const FInputKeyEventArgs* Input=nullptr)
    {
        if (bFinished || !bPrepared || !Character.IsValid() || !State.IsValid() || !Skill.IsValid() || Rows.Num()>=20000) { return; }
        const double Now=FPlatformTime::Seconds(),WorldNow=World->GetTimeSeconds();
        FRow R; R.Frame=GFrameCounter; R.Event=Event; R.Stage=Stage; R.Trial=bWindow?TrialIndex:INDEX_NONE; R.Wall=Now; R.World=WorldNow; R.WorldDT=DT; R.Flushes=Flushes; R.ViewportEvents=ViewportEvents;
        if (Input) { R.ViewportKey=Input->Key.GetFName(); R.ViewportEvent=static_cast<int32>(Input->Event); R.ViewportAmount=Input->AmountDepressed; R.bViewportSimulated=Input->IsSimulatedInput(); }
        const FRotator Control=PC->GetControlRotation(); R.ControlPitch=Control.Pitch; R.ControlYaw=Control.Yaw; R.ControlRoll=Control.Roll;
        if (bWindow) { R.ControlDeviation=Control.ContainsNaN()?std::numeric_limits<double>::quiet_NaN():FMath::RadiansToDegrees(Control.Quaternion().AngularDistance(Trials[TrialIndex].ControlBaseline.Quaternion())); }
        R.Uses=State->GetSkillUseCount(Skill->GetFName()); R.Health=Target.IsValid()?Target->GetRogueAttributeSet()->GetHealth():0.f; R.bLMB=PC->IsInputKeyDown(EKeys::LeftMouseButton); R.bBlocking=PC->IsAnyBlockingWindowVisible();
        R.bWeighted=Appearance.IsValid() && Appearance->IsWeightedFirstPersonActive(); R.bInspection=Appearance.IsValid() && Appearance->IsThirdPersonInspectionActive();
        if (Event==EEvent::BeginFrame) { R.BeginIntervalStart=LastBegin; R.BeginGap=LastBegin>0.?Now-LastBegin:0.; LastBegin=Now; R.bBeginIntervalContained=bWindow && R.BeginIntervalStart>=Trials[TrialIndex].WindowStart; }
        if (Event==EEvent::WorldPost) { R.PostGap=LastPostWorld>=0.?WorldNow-LastPostWorld:DT; LastPostWorld=WorldNow; }
        if (Source.IsValid() && Source->GetAnimInstance())
        {
            if (const auto* M=Source->GetAnimInstance()->GetInstanceForMontage(Skill->AttackMontage); M && M->GetWeight()>UE_SMALL_NUMBER) { R.Montage=M->GetPosition(); R.Rate=M->GetPlayRate(); }
        }
        if (bWindow)
        {
            auto& T=Trials[TrialIndex];
            if (Event==EEvent::BeginFrame)
            {
                ++T.FrameSamples;
                if (R.bBeginIntervalContained) { ++T.CompleteIntervals; T.MaxBeginGap=FMath::Max(T.MaxBeginGap,R.BeginGap); T.WallHitches50ms+=R.BeginGap>.05?1:0; }
                else { ++T.BoundaryIntervals; T.MaxBoundaryBeginGap=FMath::Max(T.MaxBoundaryBeginGap,R.BeginGap); }
            }
            if (Event==EEvent::WorldPost)
            {
                T.bProfileCorrect&=R.bWeighted==bWeighted && !R.bInspection && !R.bBlocking;
                if (!FMath::IsFinite(R.ControlDeviation)) { T.bControlInvalid=true; }
                else { T.MaxControlDeviation=FMath::Max(T.MaxControlDeviation,R.ControlDeviation); T.ControlDeviationSamples+=R.ControlDeviation>.01?1:0; }
            }
            if (Event==EEvent::WorldPre || Event==EEvent::WorldPost) { T.MaxWorldDT=FMath::Max(T.MaxWorldDT,static_cast<double>(DT)); }
            if (Event==EEvent::WorldPost && T.RequestWall>=0.)
            {
                if (T.AcceptWorld<0. && R.Uses>T.UsesBefore) { T.AcceptWorld=WorldNow; T.AcceptWall=Now; T.AcceptFrame=GFrameCounter; T.AcceptDelta=R.PostGap; }
                if (R.Montage>=0.) { ++T.MontageSamples; T.bRateCorrect&=FMath::Abs(R.Rate-ExpectedRate)<.001f; }
                if (T.DamageWorld<0. && R.Health<9999.99f) { T.DamageWorld=WorldNow; T.DamageWall=Now; T.DamageFrame=GFrameCounter; T.DamageDelta=R.PostGap; T.PreviousDamageDelta=PreviousPostGap; }
            }
        }
        if (Event==EEvent::WorldPost) { PreviousPostGap=R.PostGap; }
        Rows.Add(R);
    }
    void Finish(bool bExit=true)
    {
        if (bFinished) { return; }
        if (bWindow && TrialIndex>=0 && TrialIndex<2)
        {
            auto& T=Trials[TrialIndex]; T.WindowEnd=FPlatformTime::Seconds(); T.FlushEnd=Flushes; T.ViewportEnd=ViewportEvents; T.bContaminated=T.FlushEnd!=T.FlushStart || T.ViewportEnd!=T.ViewportStart || T.ControlDeviationSamples>0 || T.bControlInvalid;
        }
        bFinished=true; Next=nullptr; bWindow=false; Stage=EStage::Complete;
        FCoreDelegates::OnBeginFrame.Remove(BeginHandle); FCoreDelegates::OnEndFrame.Remove(EndHandle);
        FWorldDelegates::OnWorldPreActorTick.Remove(PreHandle); FWorldDelegates::OnWorldPostActorTick.Remove(PostHandle); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        if (Viewport.IsValid()) { Viewport->OnInputKey().Remove(ViewportHandle); }
#if UE_PLAYER_INPUT_INCLUDE_DEBUG
        UE::Input::FPlayerInputDebugging::OnPlayerInputFlushed.Remove(FlushHandle);
#endif
        Key(false);
        if (bPrepared && Character.IsValid())
        {
            if (Saves && Saves->IsAttachedTo(*Character)) { Saves->DetachCharacter(*Character,false); }
            Combat->CancelBoundSkillInput(); Combat->CancelCombatVisuals(); Character->SetCanBeDamaged(bInitialDamage);
            Character->GetRogueAttributeSet()->SetAttackSpeedMultiplier(InitialAttackSpeed); PC->SetControlRotation(InitialControl); Character->SetActorTransform(InitialTransform,false,nullptr,ETeleportType::TeleportPhysics);
        }
        if (Target.IsValid()) { Target->Destroy(); }
        if (bOwnCVars) { Heavy->Set(OldHeavy,ECVF_SetByConsole); Motion->Set(OldMotion,ECVF_SetByConsole); Fov->Set(OldFov,ECVF_SetByConsole); }
        Check(Rows.Num()<20000,TEXT("raw_observations_not_truncated_by_memory_guard"));
        FString Csv=TEXT("frame,event,stage,trial,wall_s,world_s,world_dt_s,begin_frame_gap_s,post_world_gap_s,skill_use_count,target_health,montage_s,montage_rate,lmb_down,blocking_ui,input_flush_count,weighted_active,inspection,viewport_event_count,viewport_key,viewport_input_event,viewport_amount,viewport_simulated,control_pitch,control_yaw,control_roll,control_deviation_deg,begin_interval_start_wall_s,begin_interval_fully_in_window\n"); Csv.Reserve(Rows.Num()*180);
        for (const auto& R:Rows) { Csv+=FString::Printf(TEXT("%llu,%s,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.6f,%.6f,%.6f,%d,%d,%llu,%d,%d,%llu,%s,%d,%.9f,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%d\n"),R.Frame,Name(R.Event),static_cast<int32>(R.Stage),R.Trial,R.Wall,R.World,R.WorldDT,R.BeginGap,R.PostGap,R.Uses,R.Health,R.Montage,R.Rate,R.bLMB,R.bBlocking,R.Flushes,R.bWeighted,R.bInspection,R.ViewportEvents,*R.ViewportKey.ToString(),R.ViewportEvent,R.ViewportAmount,R.bViewportSimulated,R.ControlPitch,R.ControlYaw,R.ControlRoll,R.ControlDeviation,R.BeginIntervalStart,R.bBeginIntervalContained); }
        Check(FFileHelper::SaveStringToFile(Csv,*(Output/TEXT("frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM),TEXT("raw_frame_CSV_written_after_all_observation_windows"));
        auto Json=MakeShared<FJsonObject>(); Json->SetNumberField(TEXT("probe_schema_version"),2); Json->SetStringField(TEXT("label"),Label); Json->SetStringField(TEXT("profile"),Profile); Json->SetBoolField(TEXT("rendered"),bRendered); Json->SetBoolField(TEXT("offscreen"),bOffscreen);
        Json->SetStringField(TEXT("measurement_kind"),TEXT("fresh_process_first_use_and_same_skill_repeat_not_cleared_cache")); Json->SetBoolField(TEXT("cache_cleared"),false); Json->SetBoolField(TEXT("physical_input_performed"),false); Json->SetBoolField(TEXT("manual_field_run"),false);
        Json->SetStringField(TEXT("input"),TEXT("programmatic_Controller_InputKey_actual_authored_primary")); Json->SetStringField(TEXT("performance_assertions"),TEXT("none; 50ms is an observation category"));
        Json->SetStringField(TEXT("IO_scope"),TEXT("No QA screenshot, file write, hash polling or stack symbolization from quiet start through damage+1s/recovery; existing engine/game logging remains enabled."));
        Json->SetStringField(TEXT("window_scope"),TEXT("Each trial includes 0.5s before input through first damage+1s and actual full action/cooldown/combo recovery; maximum gaps cover this entire window. Only BeginFrame intervals fully contained in the window contribute to max/hitch summary; boundary interval count/max and all raw rows remain available. Raw event/frame rows permit narrower analysis."));
        Json->SetStringField(TEXT("residency_limit"),TEXT("Normal profile and target loaded before window; attack assets may already reside in process/OS/DDC/driver caches. No cache reset or pre-attack asset warmup."));
        Json->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601()); Json->SetStringField(TEXT("skill"),Skill.IsValid()?Skill->GetPathName():TEXT("unavailable")); Json->SetNumberField(TEXT("expected_montage_rate"),ExpectedRate); Json->SetNumberField(TEXT("expected_first_hit_delay_s"),ExpectedDelay);
        Json->SetBoolField(TEXT("official_flush_hook_available"),bFlushHook); Json->SetBoolField(TEXT("viewport_input_observer_available"),bViewportObserver);
        Json->SetStringField(TEXT("contamination_contract"),TEXT("official_flush_or_any_viewport_key_event_or_control_rotation_deviation_gt_0p01deg")); Json->SetNumberField(TEXT("control_deviation_classification_tolerance_deg"),.01);
        Json->SetNumberField(TEXT("viewport_key_events_total_in_windows"),ViewportEvents); Json->SetNumberField(TEXT("input_flush_count_total"),Flushes); Json->SetNumberField(TEXT("row_count"),Rows.Num());
        TArray<TSharedPtr<FJsonValue>> TrialValues; bool bContaminated=false;
        for (int32 I=0;I<2;++I)
        {
            const auto& T=Trials[I]; auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("trial"),I); J->SetStringField(TEXT("kind"),I==0?TEXT("first_use"):TEXT("same_skill_repeat")); J->SetBoolField(TEXT("complete"),T.bComplete); J->SetBoolField(TEXT("input_contaminated"),T.bContaminated); bContaminated|=T.bContaminated;
            J->SetNumberField(TEXT("window_start_wall_s"),T.WindowStart); J->SetNumberField(TEXT("window_end_wall_s"),T.WindowEnd); J->SetNumberField(TEXT("request_wall_s"),T.RequestWall); J->SetNumberField(TEXT("request_world_s"),T.RequestWorld); J->SetNumberField(TEXT("request_frame"),T.RequestFrame); J->SetNumberField(TEXT("input_call_wall_s"),T.InputCallSeconds);
            J->SetNumberField(TEXT("accepted_wall_s"),T.AcceptWall); J->SetNumberField(TEXT("accepted_world_s"),T.AcceptWorld); J->SetNumberField(TEXT("accepted_frame"),T.AcceptFrame); J->SetNumberField(TEXT("first_damage_wall_s"),T.DamageWall); J->SetNumberField(TEXT("first_damage_world_s"),T.DamageWorld); J->SetNumberField(TEXT("first_damage_frame"),T.DamageFrame);
            J->SetNumberField(TEXT("accepted_observation_delta_s"),T.AcceptDelta); J->SetNumberField(TEXT("first_damage_frame_delta_s"),T.DamageDelta); J->SetNumberField(TEXT("previous_damage_delta_s"),T.PreviousDamageDelta); J->SetNumberField(TEXT("first_damage_error_s"),T.TimingError); J->SetNumberField(TEXT("allowed_early_s"),T.AllowedEarly); J->SetNumberField(TEXT("allowed_late_s"),T.AllowedLate);
            J->SetNumberField(TEXT("uses_before"),T.UsesBefore); J->SetNumberField(TEXT("uses_after"),T.UsesAfter); J->SetNumberField(TEXT("target_damage"),T.Damage); J->SetNumberField(TEXT("montage_samples"),T.MontageSamples); J->SetBoolField(TEXT("montage_rate_correct"),T.bRateCorrect);
            J->SetBoolField(TEXT("requested_profile_held"),T.bProfileCorrect);
            J->SetNumberField(TEXT("viewport_event_count_start"),T.ViewportStart); J->SetNumberField(TEXT("viewport_event_count_end"),T.ViewportEnd); J->SetNumberField(TEXT("viewport_key_events"),T.ViewportEnd-T.ViewportStart); J->SetNumberField(TEXT("viewport_non_simulated_key_events"),T.ViewportNonSimulated); J->SetNumberField(TEXT("viewport_lmb_release_events"),T.ViewportLMBReleases);
            J->SetNumberField(TEXT("control_baseline_pitch"),T.ControlBaseline.Pitch); J->SetNumberField(TEXT("control_baseline_yaw"),T.ControlBaseline.Yaw); J->SetNumberField(TEXT("control_baseline_roll"),T.ControlBaseline.Roll); J->SetNumberField(TEXT("max_control_rotation_deviation_deg"),T.MaxControlDeviation); J->SetNumberField(TEXT("control_rotation_deviation_samples"),T.ControlDeviationSamples); J->SetBoolField(TEXT("control_rotation_invalid"),T.bControlInvalid);
            TArray<TSharedPtr<FJsonValue>> ContaminationReasons; if(T.FlushEnd!=T.FlushStart){ContaminationReasons.Add(MakeShared<FJsonValueString>(TEXT("official_input_flush")));} if(T.ViewportEnd!=T.ViewportStart){ContaminationReasons.Add(MakeShared<FJsonValueString>(TEXT("viewport_key_event_during_direct_PC_input_window")));} if(T.ControlDeviationSamples>0 || T.bControlInvalid){ContaminationReasons.Add(MakeShared<FJsonValueString>(TEXT("control_rotation_changed_or_invalid")));} J->SetArrayField(TEXT("contamination_reasons"),ContaminationReasons);
            J->SetNumberField(TEXT("complete_begin_frame_intervals"),T.CompleteIntervals); J->SetNumberField(TEXT("boundary_begin_frame_intervals"),T.BoundaryIntervals); J->SetNumberField(TEXT("boundary_max_begin_frame_gap_ms"),T.MaxBoundaryBeginGap*1000.);
            J->SetNumberField(TEXT("flush_count_start"),T.FlushStart); J->SetNumberField(TEXT("flush_count_end"),T.FlushEnd); J->SetNumberField(TEXT("begin_frame_samples"),T.FrameSamples); J->SetNumberField(TEXT("max_begin_frame_gap_ms"),T.MaxBeginGap*1000.); J->SetNumberField(TEXT("wall_gaps_over_50ms"),T.WallHitches50ms); J->SetNumberField(TEXT("max_world_dt_ms"),T.MaxWorldDT*1000.); TrialValues.Add(MakeShared<FJsonValueObject>(J));
        }
        Json->SetArrayField(TEXT("trials"),TrialValues); Json->SetNumberField(TEXT("checks"),Checks); Json->SetNumberField(TEXT("failures"),Failures); Json->SetBoolField(TEXT("input_contaminated"),bContaminated); Json->SetBoolField(TEXT("functional_passed"),Failures==0); Json->SetBoolField(TEXT("probe_passed"),Failures==0&&!bContaminated);
        TArray<TSharedPtr<FJsonValue>> Reasons; for (const FString& R:FailureReasons) { Reasons.Add(MakeShared<FJsonValueString>(R)); } Json->SetArrayField(TEXT("failure_reasons"),Reasons);
        FString Serialized; const auto Writer=TJsonWriterFactory<>::Create(&Serialized); const bool bSaved=FJsonSerializer::Serialize(Json,Writer) && FFileHelper::SaveStringToFile(Serialized,*(Output/TEXT("results.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        if (!bSaved) { ++Failures; }
        const TCHAR* Status=Failures?TEXT("FAILED"):bContaminated?TEXT("CONTAMINATED"):TEXT("PASSED");
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIRST_ATTACK_FRAMES_%s checks=%d failures=%d input_contaminated=%d output=%s"),Status,Checks,Failures,bContaminated,*Output);
        if (bExit) { FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:bContaminated?2:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f); }
    }
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<UGameViewportClient> Viewport; TWeakObjectPtr<ARogue10mPlayerController> PC; TWeakObjectPtr<ARogue10mCharacter> Character; TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<URogue10mCombatComponent> Combat; TWeakObjectPtr<USkeletalMeshComponent> Source; TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;
    TWeakObjectPtr<URogue10mAttackSkillData> Skill; TWeakObjectPtr<ARogue10mBasicMonster> Target; URogue10mProgressionSubsystem* Saves=nullptr;
    FDelegateHandle BeginHandle,EndHandle,PreHandle,PostHandle,FlushHandle,ViewportHandle,Cleanup; FTSTicker::FDelegateHandle Ticker; TFunction<void()> Next;
    TArray<FRow> Rows; TArray<FString> FailureReasons; FTrial Trials[2]; FString Label,Profile,Output,SkillFields;
    FTransform InitialTransform; FRotator InitialControl; IConsoleVariable* Heavy=nullptr; IConsoleVariable* Motion=nullptr; IConsoleVariable* Fov=nullptr;
    EStage Stage=EStage::Setup; int32 Checks=0,Failures=0,TrialIndex=INDEX_NONE,OldHeavy=0; uint64 Flushes=0,ViewportEvents=0;
    double Deadline=0,NextAt=0,GroundDeadline=0,TrialDeadline=0,LastBegin=0,LastPostWorld=-1,PreviousPostGap=0,ExpectedRate=1,ExpectedDelay=0;
    float InitialAttackSpeed=1,OldMotion=1,OldFov=0; bool bFinished=false,bPrepared=false,bOwnCVars=false,bWindow=false,bWeighted=false,bRendered=false,bOffscreen=false,bInitialDamage=true,bFlushHook=false,bViewportObserver=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context:GEngine->GetWorldContexts()) { if (UWorld* W=Context.World(); W && Context.WorldType==EWorldType::Game) { Active=MakeShared<FRun>(); Active->Start(*W); return; } }
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIRST_ATTACK_FRAMES_FAILED reason=no_standalone_game_world")); FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.MeasureFirstAttackFrames"),TEXT("Fresh FpAttack QA profiles only: process-first authored knuckle primary and same-skill repeat, Begin/EndFrame wall and world deltas; never a cleared-cache or physical-input benchmark."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
