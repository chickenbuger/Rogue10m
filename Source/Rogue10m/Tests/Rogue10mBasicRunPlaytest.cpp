// Draft for a later reviewed work branch. No gameplay fixtures or direct progression mutations.
// Programmatic Controller input / DoAim / actual UMG delegates, NOT physical-input evidence.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "Rogue10mMenuWindowWidgets.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackResourceBudget.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterLobbyWidget.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mHybridSkillTreeWidget.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMainMenuWidget.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mRunResultWidget.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "UnrealClient.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace Rogue10mBasicRunPlaytest
{
static FString Csv(FString Value)
{
    return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")) + TEXT("\"");
}
static bool OneOption(const TArray<FString>& Options, const TCHAR* Name, FString& Value)
{
    int32 Count = 0; const FString Prefix = FString(Name) + TEXT("=");
    for (const FString& Option : Options)
    {
        if (!Option.StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
        ++Count; Value = Option.Mid(Prefix.Len());
        if (Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) { Value = Value.Mid(1, Value.Len() - 2); }
    }
    return Count == 1 && !Value.IsEmpty();
}

class FDriver : public TSharedFromThis<FDriver>
{
public:
    bool Finished() const { return bFinished; }
    void Start(UWorld& Initial)
    {
        TArray<FString> Tokens, Options; FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
        FString TownSlot, Prefix, KeySlot, Requested, Limit;
        bool Valid = OneOption(Options, TEXT("RogueBasicRunLabel"), Label)
            && OneOption(Options, TEXT("RogueBasicRunOutput"), Requested)
            && OneOption(Options, TEXT("RogueBasicRunMaxSeconds"), Limit)
            && OneOption(Options, TEXT("RogueProfileSaveSlot"), TownSlot)
            && OneOption(Options, TEXT("RogueProgressionSavePrefix"), Prefix)
            && OneOption(Options, TEXT("RogueSkillKeySaveSlot"), KeySlot);
        Valid &= Label.Len() >= 1 && Label.Len() <= 24;
        for (TCHAR C : Label) { Valid &= (FChar::IsAlnum(C) && C < 128) || C == TEXT('_'); }
        int32 Seconds = 0; Valid &= LexTryParseString(Seconds, *Limit) && Seconds >= 1300 && Seconds <= 1800;
        Valid &= TownSlot == TEXT("Rogue10m_Town_QA_BasicRun_") + Label
            && Prefix == TEXT("Rogue10m_Progression_QA_BasicRun_") + Label
            && KeySlot == TEXT("Rogue10m_Fighter_QA_BasicRun_") + Label;
        Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("DevLog/BasicRunPlayQA") / Label / TEXT("Driver"));
        Valid &= FPaths::IsSamePath(Output, FPaths::ConvertRelativePathToFull(Requested));
        Valid &= !GIsEditor && Initial.WorldType == EWorldType::Game && Initial.GetNetMode() == NM_Standalone
            && UGameplayStatics::GetCurrentLevelName(&Initial, true) == TEXT("L_Menu")
            && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
        auto* Profiles = Initial.GetGameInstance() ? Initial.GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        Valid &= Profiles && Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile();
        TArray<FString> Existing;
        IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(Prefix + TEXT("_*.sav")), true, false);
        Valid &= Existing.IsEmpty() && !UGameplayStatics::DoesSaveGameExist(TownSlot, 0)
            && !UGameplayStatics::DoesSaveGameExist(KeySlot, 0) && !IFileManager::Get().DirectoryExists(*Output);
        if (!Valid)
        {
            UE_LOG(LogRogue10m, Error, TEXT("BASIC_RUN_REJECTED exact_fresh_three_slots_rendered_L_Menu_required"));
            FPlatformMisc::RequestExitWithStatus(false, 3); return;
        }
        bPreparedBossStrategy = FParse::Param(FCommandLine::Get(), TEXT("RogueBasicRunPreparedBoss"));
        bGearUiEquipment = FParse::Param(FCommandLine::Get(), TEXT("RogueBasicRunGearUI"));
        Instance = Initial.GetGameInstance(); World = &Initial;
        if (!IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true)
            || !WriteNew(TEXT("samples.csv"), TEXT("wall,stage,map,phase,world_time,remaining,x,y,z,speed,hp,max_hp,level,xp,mastery,run_gold,save_ready,target,target_hp,w,a,s,d,shift,lmb,first_kill,sword,learned,boss_reached\n"))
            || !WriteNew(TEXT("movement-diagnostics.csv"), TEXT("wall,stage,map,has_pawn,has_movement,has_attributes,has_capsule,has_mesh,has_anim_instance,movement_mode,custom_movement_mode,max_walk_speed,max_speed,velocity_x,velocity_y,velocity_z,acceleration_x,acceleration_y,acceleration_z,movement_active,movement_tick_enabled,can_jump,jump_current_count,dodging,sprinting,stamina,max_stamina,capsule_radius,capsule_half_height,mesh_root_motion,character_root_motion,active_montage_path,floor_blocking,floor_walkable,floor_line_trace,floor_impact_normal_x,floor_impact_normal_y,floor_impact_normal_z,floor_hit_normal_x,floor_hit_normal_y,floor_hit_normal_z,floor_dist,line_dist,floor_actor_path,floor_component_path\n"))
            || !WriteNew(TEXT("events.csv"), TEXT("wall,event,stage,detail\n"))
            || !WriteNew(TEXT("screenshots.csv"), TEXT("wall,event,file,request_stage,observed_stage,bytes\n"))
            || !WriteNew(TEXT("input-frames.csv"), TEXT("frame,wall,stage,map,dt,world_dt,x,y,z,yaw,pitch,desired_w,actual_w,desired_a,actual_a,desired_s,actual_s,desired_d,actual_d,desired_shift,actual_shift,actual_alt,actual_lmb,last_input_x,last_input_y,move_ignored,look_ignored,paused,streaming_ready,waypoint,remaining\n")))
        { FPlatformMisc::RequestExitWithStatus(false, 3); return; }
        bAnimationEvidence = FParse::Param(FCommandLine::Get(), TEXT("RogueBasicRunAnimationEvidence"));
        if (bAnimationEvidence && (!WriteNew(TEXT("animation-events.csv"), TEXT("event,phase,frame,world_s,wall_s,stage,phase_matches,skill,montage,source_mesh,source_asset,body_asset,target,target_hp,skill_uses,attack_busy,attack_cd,combo,dodging,dodge_cd,montage_s,montage_rate,montage_active,hand_l_x,hand_l_y,hand_l_z,hand_r_x,hand_r_y,hand_r_z\n"))
            || !WriteNew(TEXT("animation-assets.txt"), TEXT("scope=read_only_opportunistic_normal_input_observation\nno_authored_timing_or_gameplay_state_mutations=1\nprocessed_callback_is_not_exact_contact_or_GPU_pixel_time=1\n"))))
        { FPlatformMisc::RequestExitWithStatus(false, 3); return; }
        Started = Now(); Deadline = Started + Seconds; StageAt = Started;
        Stage = TEXT("main_menu");
        ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak = TWeakPtr<FDriver>(AsShared())]
        { if (auto Self = Weak.Pin()) { Self->ReleaseAll(); Self->FlushFrames(); Self->StopAnimationObservation(); Self->Summary(TEXT("process_pre_exit")); } });
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float Delta)
        { return Self->Tick(Delta); }), 0.f);
        if (bAnimationEvidence)
        {
            AnimationTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Weak=TWeakPtr<FDriver>(AsShared())](UWorld* W,ELevelTick,float)
            { if (auto Self=Weak.Pin(); Self && !Self->bFinished && W==Self->World.Get()) { Self->ObserveAnimation(); } });
            AnimationProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Weak=TWeakPtr<FDriver>(AsShared())]
            { if (auto Self=Weak.Pin(); Self && !Self->bFinished) { Self->AnimationScreenshotProcessed(); } });
        }
        Event(TEXT("started"), TEXT("input_only_programmatic; no fixtures; normal 1200-second game rule"));
        if (bPreparedBossStrategy) { Event(TEXT("prepared_strategy"), TEXT("opt_in_state_aware_input_QA; field_walking; actual_reserve; windup_escape; recovery_input; not_human_fairness_evidence")); }
        Shot(TEXT("main_menu")); Summary(TEXT("running"));
    }
private:
    static double Now() { return FPlatformTime::Seconds(); }
    bool WriteNew(const TCHAR* Name, const FString& Text)
    { return FFileHelper::SaveStringToFile(Text, *(Output / Name), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_NoReplaceExisting); }
    void Append(const TCHAR* Name, const FString& Text)
    {
        if (!FFileHelper::SaveStringToFile(Text, *(Output / Name), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append)) { bIoFailed = true; }
    }
    void Event(const TCHAR* Kind, const FString& Detail)
    {
        Append(TEXT("events.csv"), FString::Printf(TEXT("%.3f,%s,%s,%s\n"), Now() - Started, *Csv(Kind), *Csv(Stage), *Csv(Detail)));
        UE_LOG(LogRogue10m, Display, TEXT("BASIC_RUN %s stage=%s %s"), Kind, *Stage, *Detail);
    }
    template<class T> T* VisibleWidget(bool RequireViewport = true) const
    {
        for (TObjectIterator<T> It; It; ++It)
        { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get() && (!RequireViewport || It->IsInViewport()) && It->IsVisible()) { return *It; } }
        return nullptr;
    }
    static bool Displayed(UWidget* Widget)
    {
        if (!Widget || !Widget->GetCachedWidget().IsValid()) { return false; }
        for (UWidget* Cursor = Widget; Cursor; Cursor = Cursor->GetParent())
        { if (!Cursor->IsVisible() || !Cursor->GetIsEnabled()) { return false; } }
        return true;
    }
    bool Click(UUserWidget* Widget, const TCHAR* Name)
    {
        UButton* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
        if (!Displayed(Widget) || !Displayed(Button)) { return false; }
        Event(TEXT("UMG_button_delegate"), Name); Button->OnClicked.Broadcast(); return true;
    }
    void Key(const FKey& Input, bool Down)
    {
        if (!PC.IsValid() || Held.FindRef(Input) == Down) { return; }
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Input, Down ? IE_Pressed : IE_Released, Down ? 1.f : 0.f));
        Held.FindOrAdd(Input) = Down;
        Event(Down ? TEXT("Controller_key_down") : TEXT("Controller_key_up"), Input.ToString());
    }
    void Pulse(const FKey& Input, double Hold = 0.14)
    { if (!Held.FindRef(Input)) { Key(Input, true); Releases.FindOrAdd(Input) = Now() + Hold; } }
    void ReleaseAll()
    {
        TArray<FKey> Keys; Held.GenerateKeyArray(Keys); for (const FKey& K : Keys) { Key(K, false); }
        Releases.Empty();
    }
    void StopMove() { Key(EKeys::W, false); Key(EKeys::A, false); Key(EKeys::S, false); Key(EKeys::D, false); Key(EKeys::LeftShift, false); }
    void SetStage(const TCHAR* Next, double Limit = 180.)
    {
        ReleaseAll(); Stage = Next; StageAt = Now(); StageLimit = Limit; ActionAt = Now() + 0.4;
        StuckAt = Now(); LastProgress = Character.IsValid() ? Character->GetActorLocation() : FVector::ZeroVector; RecoveryCount = 0;
        Event(TEXT("stage"), Next); Shot(Next); Summary(TEXT("running"));
    }
    void Block(const FString& Why)
    {
        if (bFinishing) { return; }
        Reason = Why; Outcome = TEXT("blocked"); Event(TEXT("blocked"), Why); ReleaseAll(); Shot(TEXT("failure"));
        bFinishing = true; FinishAt = Now() + 1.5; Summary(TEXT("blocked"));
    }
    void Refresh()
    {
        UWorld* Found = nullptr;
        if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
        { if (UWorld* W = Context.World(); W && Context.WorldType == EWorldType::Game && W->GetGameInstance() == Instance.Get()) { Found = W; break; } }
        if (Found != World.Get()) { ReleaseAll(); Held.Empty(); bAimSample = false; World = Found; Event(TEXT("world_change"), Found ? Found->GetName() : TEXT("no_world")); }
        PC = Found ? Cast<ARogue10mPlayerController>(Found->GetFirstPlayerController()) : nullptr;
        Character = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
        Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
        Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
        Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
        Run = Found ? Found->GetGameState<ARogue10mGameState>() : nullptr;
        Saves = Instance.IsValid() ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
    }
    FString Map() const { return World.IsValid() ? UGameplayStatics::GetCurrentLevelName(World.Get(), true) : FString(); }
    bool StreamingReady() const
    {
        auto* Streaming = World.IsValid() ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
        return !Streaming || Streaming->IsAllStreamingCompleted();
    }
    bool Face(const FVector& Point, float Delta)
    {
        if (!PC.IsValid() || !Character.IsValid()) { return false; }
        FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye, View);
        const FRotator Control = PC->GetControlRotation();
        if (bAimSample)
        {
            const float ObservedYaw = static_cast<float>(FMath::FindDeltaAngleDegrees(PreviousControl.Yaw, Control.Yaw));
            const float ObservedPitch = static_cast<float>(FMath::FindDeltaAngleDegrees(PreviousControl.Pitch, Control.Pitch));
            if (FMath::Abs(PreviousYawInput) > .001f && FMath::Abs(ObservedYaw) > .01f) { YawInputSign = FMath::Sign(ObservedYaw * PreviousYawInput); }
            if (FMath::Abs(PreviousPitchInput) > .001f && FMath::Abs(ObservedPitch) > .01f) { PitchInputSign = FMath::Sign(ObservedPitch * PreviousPitchInput); }
        }
        const FRotator Goal = (Point - Eye).Rotation();
        const float Yaw = static_cast<float>(FMath::FindDeltaAngleDegrees(View.Yaw, Goal.Yaw));
        const float Pitch = static_cast<float>(FMath::FindDeltaAngleDegrees(View.Pitch, FMath::Clamp(Goal.Pitch, -55.0, 55.0)));
        const float Step = 75.f * FMath::Clamp(Delta, 0.001f, 0.05f);
        // DoAim applies the normal look sensitivity. No control/camera rotation setters.
        PreviousControl = Control;
        PreviousYawInput = FMath::Clamp(Yaw * 0.25f, -Step, Step) * YawInputSign;
        PreviousPitchInput = FMath::Clamp(Pitch * 0.25f, -Step, Step) * PitchInputSign;
        // DoAim is a protected reflected input function; do not broaden product visibility for QA.
        struct FAimInput { float Yaw; float Pitch; } Input { PreviousYawInput, PreviousPitchInput };
        UFunction* AimFunction = Character->FindFunction(TEXT("DoAim"));
        if (!AimFunction || AimFunction->ParmsSize != sizeof(FAimInput)) { Block(TEXT("normal_DoAim_input_function_not_reflected_or_signature_changed")); return false; }
        Character->ProcessEvent(AimFunction, &Input); bAimSample = true;
        return FMath::Abs(Yaw) < 20.f && FMath::Abs(Pitch) < 18.f;
    }
    bool Walk(const FVector& Goal, float Radius, float Delta, bool Sprint = true, bool Narrow = false)
    {
        if (!Character.IsValid() || !PC.IsValid()) { return false; }
        const FVector Position = Character->GetActorLocation(); const float Distance = static_cast<float>(FVector::Dist2D(Position, Goal));
        if (bPreparedBossStrategy && Map() == TEXT("OpenRunMap"))
        {
            // Ordinary walking preserves the sprint budget; no speed or resource setter.
            Sprint = false;
            float DodgeCost = 0.f, Reserve = 0.f; FRogue10mAttackResourceBudget Light;
            if (!PreparedCosts(DodgeCost, Reserve, Light)) { Block(TEXT("prepared_costs_missing_invalid_or_reserve_exceeds_maximum")); return false; }
            const auto* A = Character->GetRogueAttributeSet();
            if (Stage == TEXT("boss_route") && Waypoint == 6 && A->GetStamina() < Reserve)
            {
                StopMove();
                StuckAt = Now(); LastProgress = Position; // Planned rest is not failed locomotion.
                if (!bPreparedReserveWaiting) { Event(TEXT("prepared_reserve_wait"), FString::Printf(TEXT("stamina=%.3f reserve=%.3f; normal_world_regeneration_only"), A->GetStamina(), Reserve)); }
                bPreparedReserveWaiting = true;
                return false; // Existing stage and 20-minute deadlines remain unchanged.
            }
            if (bPreparedReserveWaiting) { Event(TEXT("prepared_reserve_ready"), FString::Printf(TEXT("stamina=%.3f reserve=%.3f"), A->GetStamina(), Reserve)); bPreparedReserveWaiting = false; StuckAt = Now(); LastProgress = Position; }
        }
        if (Distance <= Radius) { StopMove(); StuckAt = Now(); LastProgress = Position; return true; }
        if (!StreamingReady()) { StopMove(); return false; }
        bool Facing = Face(FVector(Goal.X, Goal.Y, Position.Z), Delta);
        if (bFinishing) { return false; }
        if (Narrow)
        {
            FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye, View);
            const float DesiredYaw = (FVector(Goal.X, Goal.Y, Position.Z) - Eye).Rotation().Yaw;
            Facing &= FMath::Abs(FMath::FindDeltaAngleDegrees(View.Yaw, DesiredYaw)) < 5.f;
        }
        Key(EKeys::W, Facing); Key(EKeys::S, false); Key(EKeys::LeftShift, Sprint && Facing && Distance > 900.f);
        if (Now() >= RecoverUntil) { Key(EKeys::A, false); Key(EKeys::D, false); }
        if (FVector::Dist2D(Position, LastProgress) > 80.f) { LastProgress = Position; StuckAt = Now(); }
        if (Facing && Now() - StuckAt > 4.)
        {
            if (++RecoveryCount > 3)
            {
                const auto* Movement = Character->GetCharacterMovement();
                if (Movement)
                {
                    const FHitResult& Floor = Movement->CurrentFloor.HitResult;
                    Event(TEXT("stuck_readonly_floor"), FString::Printf(TEXT("actor=%s component=%s normal=%s walkable=%d goal=%s position=%s"),
                        *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()), *Floor.ImpactNormal.ToString(),
                        Movement->CurrentFloor.IsWalkableFloor(), *Goal.ToString(), *Position.ToString()));
                }
                FHitResult Obstacle; FCollisionQueryParams Query(SCENE_QUERY_STAT(BasicRunStuck), false, Character.Get());
                const FVector Toward = (FVector(Goal.X,Goal.Y,Position.Z)-Position).GetSafeNormal();
                World->LineTraceSingleByChannel(Obstacle,Position,Position+Toward*500.f,ECC_Pawn,Query);
                Event(TEXT("stuck_readonly_forward_hit"), FString::Printf(TEXT("blocking=%d actor=%s component=%s normal=%s point=%s distance=%.3f"),
                    Obstacle.bBlockingHit,*GetNameSafe(Obstacle.GetActor()),*GetNameSafe(Obstacle.GetComponent()),*Obstacle.ImpactNormal.ToString(),*Obstacle.ImpactPoint.ToString(),Obstacle.Distance));
                // Queries only: match the real scaled capsule, without moving it or changing collision responses.
                if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent(); World.IsValid() && Capsule)
                {
                    const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
                    const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
                    const FVector Start = Capsule->GetComponentLocation();
                    const FQuat Rotation = Capsule->GetComponentQuat();
                    FHitResult CapsuleHit;
                    World->SweepSingleByChannel(CapsuleHit, Start, Start + Toward * 500.f, Rotation,
                        ECC_Pawn, FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), Query);
                    Event(TEXT("stuck_readonly_capsule_pawn_sweep"), FString::Printf(TEXT("blocking=%d start_penetrating=%d penetration_depth=%.3f actor=%s component=%s normal=%s point=%s distance=%.3f start=%s end=%s radius=%.3f half_height=%.3f"),
                        CapsuleHit.bBlockingHit, CapsuleHit.bStartPenetrating, CapsuleHit.PenetrationDepth,
                        *GetPathNameSafe(CapsuleHit.GetActor()), *GetPathNameSafe(CapsuleHit.GetComponent()),
                        *CapsuleHit.ImpactNormal.ToString(), *CapsuleHit.ImpactPoint.ToString(), CapsuleHit.Distance,
                        *Start.ToString(), *(Start + Toward * 500.f).ToString(), CapsuleRadius, CapsuleHalfHeight));
                    // Start just above the capsule's current top; this is a line query, not a headroom verdict.
                    const FVector CeilingStart = Start + Capsule->GetUpVector() * (CapsuleHalfHeight + 1.f);
                    const FVector CeilingEnd = CeilingStart + FVector::UpVector * 500.f;
                    FHitResult CeilingHit;
                    World->LineTraceSingleByChannel(CeilingHit, CeilingStart, CeilingEnd, ECC_Pawn, Query);
                    Event(TEXT("stuck_readonly_ceiling_pawn_line"), FString::Printf(TEXT("blocking=%d start_penetrating=%d penetration_depth=%.3f actor=%s component=%s normal=%s point=%s distance=%.3f start=%s end=%s"),
                        CeilingHit.bBlockingHit, CeilingHit.bStartPenetrating, CeilingHit.PenetrationDepth,
                        *GetPathNameSafe(CeilingHit.GetActor()), *GetPathNameSafe(CeilingHit.GetComponent()),
                        *CeilingHit.ImpactNormal.ToString(), *CeilingHit.ImpactPoint.ToString(), CeilingHit.Distance,
                        *CeilingStart.ToString(), *CeilingEnd.ToString()));
                }
                Block(TEXT("waypoint_stuck_after_three_normal_jump_recoveries")); return false;
            }
            Pulse(EKeys::SpaceBar); StuckAt = Now();
            if (!Narrow) { Key(RecoveryCount % 2 ? EKeys::A : EKeys::D, true); RecoverUntil = Now() + 0.3; }
            Event(TEXT("normal_jump_recovery"), Goal.ToString());
        }
        return false;
    }
    ARogue10mBasicMonster* Monster(bool Boss) const
    {
        ARogue10mBasicMonster* Best = nullptr; double BestDistance = TNumericLimits<double>::Max();
        if (!World.IsValid() || !Character.IsValid()) { return nullptr; }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            const auto* Data = It->GetMonsterData(); const auto* A = It->GetRogueAttributeSet();
            if (It->IsDead() || It->IsCombatTestInstance() || !Data || !A || A->GetHealth() <= 0.f) { continue; }
            if (Boss ? Data->MonsterRank != ERogue10mMonsterRank::FinalBoss : !Data->GetName().Contains(TEXT("VerdantSlime"))) { continue; }
            const double D = FVector::DistSquared(It->GetActorLocation(), Character->GetActorLocation());
            if (D < BestDistance) { Best = *It; BestDistance = D; }
        }
        return Best;
    }
    // Opt-in QA strategy. Only const observations plus the existing normal Key/Face path.
    bool PreparedCosts(float& DodgeCost, float& Reserve, FRogue10mAttackResourceBudget& Light) const
    {
        if (!Character.IsValid() || !Combat.IsValid()) { return false; }
        const auto* A = Character->GetRogueAttributeSet();
        const auto* Dodge = Combat->GetActiveDodgeSkill();
        const auto* Primary = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        if (!A || !Dodge || !Primary || !FMath::IsFinite(Dodge->StaminaCost) || Dodge->StaminaCost < 0.f
            || !FMath::IsFinite(Dodge->Distance) || Dodge->Distance <= 0.f || !FMath::IsFinite(Dodge->Duration) || Dodge->Duration < .05f
            || !FMath::IsFinite(Dodge->Cooldown) || Dodge->Cooldown < 0.f || !FMath::IsFinite(Primary->AttackRange) || Primary->AttackRange <= 0.f) { return false; }
        const auto* Traits = Character->GetTraitRuntimeComponent();
        DodgeCost = Traits ? Traits->GetDodgeStaminaCost(Dodge->StaminaCost) : Dodge->StaminaCost;
        // No payment/identity sync. Conservatively cover a possible first follow-up too;
        // BasicBrawler has its own combo latch in addition to Combat's public window.
        Light = FRogue10mAttackResourceBudget::Build(Primary->ResourceCosts, 1.f);
        if (const auto* Combo = Primary->NextComboSkill.Get())
        {
            const auto Followup = FRogue10mAttackResourceBudget::Build(Combo->ResourceCosts, 1.f);
            Light.bValid &= Followup.bValid;
            Light.Health = FMath::Max(Light.Health, Followup.Health); Light.Stamina = FMath::Max(Light.Stamina, Followup.Stamina);
            Light.Mana = FMath::Max(Light.Mana, Followup.Mana); Light.Energy = FMath::Max(Light.Energy, Followup.Energy);
        }
        Reserve = DodgeCost + 2.f * Light.Stamina;
        return Light.bValid && FMath::IsFinite(DodgeCost) && DodgeCost >= 0.f
            && FMath::IsFinite(Reserve) && FMath::IsFinite(A->GetStamina())
            && FMath::IsFinite(A->GetMaxStamina()) && Reserve <= A->GetMaxStamina();
    }
    void PreparedMove(const FVector& Goal)
    {
        FVector Direction = (Goal - Character->GetActorLocation()).GetSafeNormal2D();
        const float Forward = static_cast<float>(FVector::DotProduct(Direction, Character->GetActorForwardVector()));
        const float Right = static_cast<float>(FVector::DotProduct(Direction, Character->GetActorRightVector()));
        Key(EKeys::LeftShift, false);
        Key(EKeys::W, Forward > .25f); Key(EKeys::S, Forward < -.25f);
        Key(EKeys::D, Right > .25f); Key(EKeys::A, Right < -.25f);
    }
    bool PreparedInsideShapeAt(const FVector& Position) const
    {
        const auto* Capsule = Character->GetCapsuleComponent();
        const float R = Capsule->GetScaledCapsuleRadius();
        const FVector Offset = Position - PreparedLockedCenter;
        if (FMath::Abs(Offset.Z) > PreparedHalfHeight + Capsule->GetScaledCapsuleHalfHeight()) { return false; }
        if (PreparedType == ERogue10mMonsterAttackType::AbyssZone) { return Offset.SizeSquared2D() <= FMath::Square(PreparedRange + R + 25.f); }
        if (PreparedType == ERogue10mMonsterAttackType::FrontalWave)
        {
            const FVector Right(-PreparedLockedForward.Y, PreparedLockedForward.X, 0.);
            const float Along = static_cast<float>(FVector::DotProduct(Offset, PreparedLockedForward));
            return Along >= -25.f && Along <= PreparedRange + R + 25.f
                && FMath::Abs(FVector::DotProduct(Offset, Right)) <= PreparedHalfWidth + R + 25.f;
        }
        const float Distance = static_cast<float>(Offset.Size2D());
        const float Padding = FMath::Asin(FMath::Clamp((R + 25.f) / FMath::Max(Distance, R + 25.f), 0.f, 1.f));
        return Distance <= PreparedRange + R + 25.f && FVector::DotProduct(PreparedLockedForward, Offset.GetSafeNormal2D())
            >= FMath::Cos(FMath::DegreesToRadians(PreparedArc * .5f) + Padding);
    }
    bool PreparedGoalSafe(const FVector& Goal, bool RequireOutsideThreat = true) const
    {
        const auto* Capsule = Character->GetCapsuleComponent();
        if (!World.IsValid() || !Capsule || Goal.ContainsNaN()) { return false; }
        const float R = Capsule->GetScaledCapsuleRadius(), HH = Capsule->GetScaledCapsuleHalfHeight();
        const float Margin = R + 50.f;
        if (Goal.X < 9500.f + Margin || Goal.X > 20500.f - Margin
            || Goal.Y < 62500.f + Margin || Goal.Y > 73500.f - Margin
            || (RequireOutsideThreat && PreparedInsideShapeAt(Goal))) { return false; }
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(BasicRunPreparedEscape), false, Character.Get());
        return !World->SweepSingleByChannel(Hit, Capsule->GetComponentLocation(), Goal, FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeCapsule(R, HH), Query);
    }
    bool PreparedLatchWindup(const URogue10mMonsterAttackPatternComponent& Pattern)
    {
        const auto* Profile = Pattern.GetProfile();
        if (!Profile || !Character->GetCapsuleComponent()) { return false; }
        PreparedType = Pattern.GetCurrentAttackType();
        PreparedLockedCenter = Pattern.GetLockedAttackLocation();
        PreparedLockedForward = Pattern.GetLockedAttackForward().GetSafeNormal2D();
        PreparedRange = PreparedType == ERogue10mMonsterAttackType::FrontalWave ? Profile->FrontalWaveRange
            : (PreparedType == ERogue10mMonsterAttackType::AbyssZone ? Profile->AbyssZoneRadius : Profile->AttackRange);
        PreparedHalfWidth = Profile->FrontalWaveHalfWidth;
        PreparedHalfHeight = PreparedType == ERogue10mMonsterAttackType::FrontalWave ? Profile->FrontalWaveHalfHeight
            : (PreparedType == ERogue10mMonsterAttackType::AbyssZone ? Profile->AbyssZoneHalfHeight : Profile->AttackHalfHeight);
        PreparedArc = Profile->AttackArcDegrees;
        if (PreparedType != ERogue10mMonsterAttackType::Basic && PreparedType != ERogue10mMonsterAttackType::FrontalWave
            && PreparedType != ERogue10mMonsterAttackType::AbyssZone) { return false; }
        if (PreparedLockedCenter.ContainsNaN() || PreparedLockedForward.ContainsNaN() || PreparedLockedForward.IsNearlyZero()
            || !FMath::IsFinite(PreparedRange) || PreparedRange <= 0.f || !FMath::IsFinite(PreparedHalfWidth) || PreparedHalfWidth <= 0.f
            || !FMath::IsFinite(PreparedHalfHeight) || PreparedHalfHeight <= 0.f || !FMath::IsFinite(PreparedArc) || PreparedArc < 1.f || PreparedArc > 180.f) { return false; }
        const float R = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const FVector P = Character->GetActorLocation();
        if (!FMath::IsFinite(R) || R <= 0.f) { return false; }
        if (PreparedType == ERogue10mMonsterAttackType::FrontalWave)
        {
            const FVector Right(-PreparedLockedForward.Y, PreparedLockedForward.X, 0.);
            const float Side = FVector::DotProduct(P - PreparedLockedCenter, Right) < 0.f ? -1.f : 1.f;
            const float Along = static_cast<float>(FVector::DotProduct(P - PreparedLockedCenter, PreparedLockedForward));
            PreparedEscapeGoal = PreparedLockedCenter + PreparedLockedForward * Along + Right * Side * (PreparedHalfWidth + R + 60.f);
        }
        else
        {
            FVector Away = (P - PreparedLockedCenter).GetSafeNormal2D();
            if (Away.IsNearlyZero()) { Away = FVector(-PreparedLockedForward.Y, PreparedLockedForward.X, 0.); }
            PreparedEscapeGoal = PreparedLockedCenter + Away * (PreparedRange + R + 60.f);
        }
        PreparedEscapeGoal.Z = P.Z;
        bool SafeGoal = PreparedGoalSafe(PreparedEscapeGoal);
        if (PreparedType == ERogue10mMonsterAttackType::Basic)
        {
            // Compare both expanded-cone sides with radial retreat. A short windup
            // still gives no arrival guarantee: actual input/collision/damage decides.
            const double Half = FMath::DegreesToRadians(static_cast<double>(PreparedArc) * .5);
            const FVector Right(-PreparedLockedForward.Y, PreparedLockedForward.X, 0.);
            for (const double Sign : {-1., 1.})
            {
                const FVector Outward = -PreparedLockedForward * FMath::Sin(Half) + Right * (Sign * FMath::Cos(Half));
                const double Travel = FMath::Max(0., R + 35. - FVector::DotProduct(P - PreparedLockedCenter, Outward));
                FVector Candidate = P + Outward * Travel; Candidate.Z = P.Z;
                if (PreparedGoalSafe(Candidate) && (!SafeGoal || FVector::DistSquared2D(P, Candidate) < FVector::DistSquared2D(P, PreparedEscapeGoal)))
                { PreparedEscapeGoal = Candidate; SafeGoal = true; }
            }
        }
        if (!SafeGoal) { return false; }
        PreparedDirectionFrame = GFrameCounter;
        Event(TEXT("prepared_windup"), FString::Printf(TEXT("count=%d type=%d center=%s forward=%s escape=%s; public_state_QA_not_human_reaction_test"),
            Pattern.GetStartedAttackCount(), static_cast<int32>(PreparedType), *PreparedLockedCenter.ToString(), *PreparedLockedForward.ToString(), *PreparedEscapeGoal.ToString()));
        return true;
    }
    void PreparedFight(float Delta)
    {
        const auto* Pattern = Target.IsValid() ? Target->GetAttackPatternComponent() : nullptr;
        float DodgeCost = 0.f, Reserve = 0.f; FRogue10mAttackResourceBudget Light;
        if (!Pattern || !Pattern->GetProfile() || !PreparedCosts(DodgeCost, Reserve, Light))
        { Block(TEXT("prepared_boss_profile_or_costs_unavailable")); return; }
        if (!FMath::IsFinite(Pattern->GetProfile()->AttackRange) || Pattern->GetProfile()->AttackRange <= 0.f)
        { Block(TEXT("prepared_boss_base_range_invalid")); return; }
        const auto* A = Character->GetRogueAttributeSet();
        if (PreparedDodgePendingAt >= 0.)
        {
            if (Character->IsDodging() || Character->GetDodgeCooldownRemaining() > PreparedDodgeCooldownBefore + .02f)
            {
                ++PreparedDodgeAccepted;
                Event(TEXT("prepared_dodge_accepted"), FString::Printf(TEXT("stamina_before=%.3f after=%.3f velocity=%s"), PreparedDodgeStaminaBefore, A->GetStamina(), *Character->GetVelocity().ToString()));
                PreparedDodgePendingAt = -1.;
            }
            else if (Now() - PreparedDodgePendingAt > .30)
            { ++PreparedDodgeUnobserved; Event(TEXT("prepared_dodge_not_observed"), TEXT("request_not_counted_as_acceptance")); PreparedDodgePendingAt = -1.; }
        }
        const auto Phase = Pattern->GetPatternState();
        const int32 Count = Pattern->GetStartedAttackCount();
        const FVector Aim = Target->GetActorLocation();
        const float Distance = static_cast<float>(FVector::Dist2D(Aim, Character->GetActorLocation()));
        if (Distance < 1800.f && !bBossReached) { bBossReached = true; Event(TEXT("actual_boss_reached"), Target->GetName()); Shot(TEXT("boss_reached")); }
        const bool Facing = Face(Aim, Delta);
        if (bFinishing) { return; }
        Key(EKeys::LeftShift, false);
        if (Phase == ERogue10mMonsterPatternState::Windup)
        {
            if (PreparedLastWindup != Count)
            {
                if (!PreparedLatchWindup(*Pattern)) { Block(TEXT("prepared_windup_geometry_unknown_or_no_safe_arena_goal")); return; }
                PreparedLastWindup = Count;
            }
            if (Pattern->GetCurrentAttackType() != PreparedType || !Pattern->GetLockedAttackLocation().Equals(PreparedLockedCenter, .1)
                || !Pattern->GetLockedAttackForward().GetSafeNormal2D().Equals(PreparedLockedForward, .001))
            { Block(TEXT("prepared_locked_geometry_changed_during_windup")); return; }
            if (!PreparedInsideShapeAt(Character->GetActorLocation())) { StopMove(); return; }
            PreparedMove(PreparedEscapeGoal);
            const FVector Desired = (PreparedEscapeGoal - Character->GetActorLocation()).GetSafeNormal2D();
            const FVector Last = Character->GetLastMovementInputVector().GetSafeNormal2D();
            if (PreparedDodgeAttemptedAttack != Count && GFrameCounter > PreparedDirectionFrame
                && FVector::DotProduct(Desired, Last) > .8 && !Character->IsDodging()
                && Character->GetDodgeCooldownRemaining() <= .001f && A->GetStamina() >= DodgeCost + Light.Stamina)
            {
                PreparedDodgeAttemptedAttack = Count; ++PreparedDodgeRequested;
                PreparedDodgeStaminaBefore = A->GetStamina(); PreparedDodgeCooldownBefore = Character->GetDodgeCooldownRemaining();
                PreparedDodgePendingAt = Now(); Pulse(EKeys::LeftAlt);
                Event(TEXT("prepared_dodge_requested"), FString::Printf(TEXT("count=%d cost=%.3f cached_direction=%s desired=%s"), Count, DodgeCost, *Last.ToString(), *Desired.ToString()));
            }
            return;
        }
        const auto* Skill = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        const float Reach = Skill ? FMath::Clamp(Skill->AttackRange * .7f, 75.f, 240.f) : 100.f;
        if (A->GetStamina() < Reserve)
        {
            const FVector Away = (Character->GetActorLocation() - Aim).GetSafeNormal2D();
            FVector RestGoal = Aim + Away * (Pattern->GetProfile()->AttackRange + 250.f);
            RestGoal.Z = Character->GetActorLocation().Z;
            if (!PreparedGoalSafe(RestGoal, false))
            { StopMove(); Block(TEXT("prepared_resource_retreat_has_no_clear_arena_goal")); return; }
            PreparedMove(RestGoal);
            return; // Ordinary movement/regen; no hidden recovery or changed timeout.
        }
        Key(EKeys::A, false); Key(EKeys::D, false);
        Key(EKeys::W, Facing && Distance > Reach); Key(EKeys::S, Distance < 55.f);
        if (Phase == ERogue10mMonsterPatternState::Recovery && PreparedPunishedAttack != Count && Facing
            && Distance < Reach + 50.f && !Combat->IsWeaponChangeBlocked() && !Combat->IsComboSequenceActive(World->GetTimeSeconds())
            && Combat->GetAttackCooldownRemaining() <= .02f
            && Light.CanPay(*A) && A->GetStamina() - Light.Stamina >= DodgeCost)
        {
            PreparedPunishedAttack = Count; Pulse(EKeys::LeftMouseButton, .12); NextAttack = Now() + .35;
            Event(TEXT("prepared_recovery_attack_input"), FString::Printf(TEXT("count=%d; one_normal_primary_request_not_a_forced_hit"), Count));
        }
    }
    void Fight(float Delta, bool Boss)
    {
        if (!Target.IsValid() || Target->IsDead()) { Target = Monster(Boss); }
        if (!Target.IsValid()) { StopMove(); return; }
        if (Boss && bPreparedBossStrategy) { PreparedFight(Delta); return; }
        const FVector Aim = Target->GetActorLocation(); const float Distance = static_cast<float>(FVector::Dist2D(Aim, Character->GetActorLocation()));
        if (Boss && Distance < 1800.f && !bBossReached) { bBossReached = true; Event(TEXT("actual_boss_reached"), Target->GetName()); Shot(TEXT("boss_reached")); }
        const bool Facing = Face(Aim, Delta);
        if (bFinishing) { return; }
        const auto* Skill = Combat->ResolveAttackSkill(true, false, false);
        const float Reach = Skill ? FMath::Clamp(Skill->AttackRange * 0.7f, 75.f, 240.f) : 100.f;
        Key(EKeys::LeftShift, false); Key(EKeys::W, Facing && Distance > Reach); Key(EKeys::S, Distance < 55.f);
        if (Target->IsPatternAttackActive() && Distance < 350.f && Now() > NextDodge)
        {
            Pulse(EKeys::LeftAlt); Key(EKeys::D, true); RecoverUntil = Now() + 0.4; NextDodge = Now() + 2.0;
            Event(TEXT("normal_dodge_input"), TEXT("LeftAlt; no invulnerability/stat mutation"));
        }
        if (Now() >= RecoverUntil) { Key(EKeys::A, false); Key(EKeys::D, false); }
        if (Facing && Distance < Reach + 50.f && Now() >= NextAttack && !Combat->IsWeaponChangeBlocked() && Combat->GetAttackCooldownRemaining() <= 0.02f)
        { Pulse(EKeys::LeftMouseButton, 0.12); NextAttack = Now() + 0.35; }
    }
    int32 GearEquippedGuidCount() const
    {
        if (!Inventory.IsValid() || !SwordId.IsValid()) { return 0; }
        int32 Count = 0;
        const auto Scan = [&](const TArray<FRogue10mInventorySlot>& Slots)
        {
            for (const auto& Slot : Slots)
            {
                if (Slot.bHasEquippedItem && Slot.EquippedItem.bOccupied && Slot.EquippedItem.InstanceId == SwordId) { ++Count; }
            }
        };
        Scan(Inventory->GetLeftEquipmentSlots()); Scan(Inventory->GetRightEquipmentSlots());
        return Count;
    }
    bool PrepareGearTarget()
    {
        if (!Inventory.IsValid() || !Combat.IsValid() || !State.IsValid() || !SwordId.IsValid()) { return false; }
        int32 Count = 0; const URogue10mItemDataAsset* Item = nullptr;
        const auto& Containers = Inventory->GetInventoryContainers();
        for (int32 Index = 0; Index < Containers.Num(); ++Index)
        {
            for (const auto& Entry : Containers[Index].Entries)
            {
                if (Entry.InstanceId == SwordId) { ++Count; GearContainerIndex = Index; Item = Entry.ItemData; }
            }
        }
        if (Count != 1 || !Item || Item->WeaponType != ERogue10mWeaponType::GreatSword
            || Item->EquipSlotType != ERogue10mInventorySlotType::MainWeapon || GearEquippedGuidCount() != 0) { return false; }
        const auto* CharacterData = Combat->GetCharacterData();
        const URogue10mWeaponSkillProfileDataAsset* Profile = nullptr; int32 Profiles = 0;
        if (CharacterData) for (const auto& Candidate : CharacterData->WeaponSkillProfiles)
        { if (Candidate && Candidate->WeaponType == ERogue10mWeaponType::GreatSword) { Profile = Candidate; ++Profiles; } }
        const auto* Expected = Profile ? Profile->FindDefaultSkill(ERogue10mAttackInputSlot::Primary) : nullptr;
        const auto* Before = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        if (Profiles != 1 || !Expected || !Before || Expected == Before) { return false; }
        GearExpectedItemPath = Item->GetPathName(); GearExpectedPrimaryPath = Expected->GetPathName(); GearBeforePrimaryPath = Before->GetPathName();
        Event(TEXT("gear_ui_target"), FString::Printf(TEXT("guid=%s container=%d item=%s before_primary=%s expected_primary=%s"),
            *SwordId.ToString(EGuidFormats::Digits), GearContainerIndex, *GearExpectedItemPath, *GearBeforePrimaryPath, *GearExpectedPrimaryPath));
        return true;
    }
    bool GearTargetAncestor(const UWidget* Item, const UWidget* Canvas) const
    {
        if (!Item || !Canvas) { return false; }
        for (const UWidget* Cursor = Item; Cursor; Cursor = Cursor->GetParent())
        {
            if (!Cursor->IsVisible() || !Cursor->GetIsEnabled() || Cursor->GetVisibility() == ESlateVisibility::HitTestInvisible) { return false; }
            if (Cursor == Canvas) { return true; }
        }
        return false;
    }
    bool GearGuardReject(const TCHAR* GuardReason, const FString& Detail = FString(), UWidget* Observed = nullptr, UWidget* Window = nullptr)
    {
        if (GearGuardLastReason != GuardReason || Now() - GearGuardLastAt >= 2.)
        {
            GearGuardLastReason = GuardReason; GearGuardLastAt = Now();
            FString Snapshot;
            if (Observed)
            {
                Snapshot += FString::Printf(TEXT(" observed=%s umg_visibility=%s visible=%d enabled=%d cached=%d geometry=%s"),
                    *Observed->GetPathName(), *UEnum::GetValueAsString(Observed->GetVisibility()), Observed->IsVisible(),
                    Observed->GetIsEnabled(), Observed->GetCachedWidget().IsValid(), *Observed->GetCachedGeometry().GetLocalSize().ToString());
                int32 Depth = 0;
                for (UWidget* Parent = Observed; Parent && Depth++ < 12; Parent = Parent->GetParent())
                {
                    Snapshot += FString::Printf(TEXT(" parent[%d]=%s/%s/enabled%d"), Depth, *Parent->GetName(),
                        *UEnum::GetValueAsString(Parent->GetVisibility()), Parent->GetIsEnabled());
                }
                if (FSlateApplication::IsInitialized() && Observed->GetCachedWidget().IsValid())
                {
                    FWidgetPath Path;
                    const bool Found = FSlateApplication::Get().FindPathToWidget(Observed->GetCachedWidget().ToSharedRef(), Path);
                    Snapshot += FString::Printf(TEXT(" diagnostic_find_path=%d path_valid=%d"), Found, Path.IsValid());
                    if (Found && Path.IsValid())
                    {
                        const auto NativeWindow = Path.GetWindow();
                        const FVector2D Center = Observed->GetCachedGeometry().LocalToAbsolute(Observed->GetCachedGeometry().GetLocalSize() * .5f);
                        const FWidgetPath Down = FSlateApplication::Get().LocateWindowUnderMouse(Center, TArray<TSharedRef<SWindow>>{NativeWindow}, false, 0);
                        Snapshot += FString::Printf(TEXT(" window_visible=%d window_minimized=%d diagnostic_down_valid=%d down_has_observed=%d down_has_window_receiver=%d down_widgets=%d"),
                            NativeWindow->IsVisible(), NativeWindow->IsWindowMinimized(), Down.IsValid(),
                            Down.ContainsWidget(Observed->GetCachedWidget().Get()), Window && Window->GetCachedWidget().IsValid() && Down.ContainsWidget(Window->GetCachedWidget().Get()), Down.Widgets.Num());
                    }
                }
            }
            Event(TEXT("gear_ui_guard_rejected"), FString::Printf(TEXT("guard=%s %s%s; diagnostic_query_only_no_pointer_routing"), GuardReason, *Detail, *Snapshot));
        }
        return false;
    }
    bool RouteGearRightClick(URogue10mInventoryWindowWidget& Window)
    {
        if (!FSlateApplication::IsInitialized() || !PC.IsValid() || !PC->IsLocalController()
            || !Instance.IsValid() || Instance->GetNumLocalPlayers() != 1 || !Combat.IsValid()
            || !PC->IsInventoryVisible() || !Window.IsWindowOpen()
            || Window.GetInventoryComponent() != Inventory.Get() || !Displayed(&Window))
        {
            return GearGuardReject(TEXT("route_preconditions"), FString::Printf(TEXT("slate=%d PC=%d local=%d instance=%d local_players=%d combat=%d inventory_flag=%d window_open=%d component_match=%d window_displayed=%d"),
                FSlateApplication::IsInitialized(), PC.IsValid(), PC.IsValid() && PC->IsLocalController(), Instance.IsValid(),
                Instance.IsValid() ? Instance->GetNumLocalPlayers() : -1, Combat.IsValid(), PC.IsValid() && PC->IsInventoryVisible(),
                Window.IsWindowOpen(), Window.GetInventoryComponent() == Inventory.Get(), Displayed(&Window)), &Window, &Window);
        }
        UWidget* Canvas = Window.GetWidgetFromName(TEXT("UI_InventoryItemCanvas"));
        URogue10mInventoryItemWidget* Item = nullptr; URogue10mInventoryItemWidget* RawItem = nullptr; int32 Count = 0, RawCount = 0;
        for (TObjectIterator<URogue10mInventoryItemWidget> It; It; ++It)
        {
            if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()
                && It->GetItemInstanceId() == SwordId && It->GetContainerIndex() == GearContainerIndex) { RawItem = *It; ++RawCount; }
            if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()
                && It->GetItemInstanceId() == SwordId && It->GetContainerIndex() == GearContainerIndex
                && It->IsVisible() && Displayed(*It) && GearTargetAncestor(*It, Canvas))
            { Item = *It; ++Count; }
        }
        if (Count > 1) { GearGuardReject(TEXT("multiple_visible_GUID_tiles"), FString::Printf(TEXT("count=%d raw_count=%d"), Count, RawCount), Item, &Window); Block(TEXT("gear_ui_target_guid_has_multiple_visible_tiles")); return false; }
        if (Count != 1 || !Item || !Canvas)
        {
            return GearGuardReject(TEXT("canvas_or_unique_tile_filter"), FString::Printf(TEXT("canvas=%s canvas_cached=%d count=%d raw_guid_container_count=%d expected_container=%d raw_ancestor_ok=%d"),
                *GetPathNameSafe(Canvas), Canvas && Canvas->GetCachedWidget().IsValid(), Count, RawCount, GearContainerIndex,
                RawItem && GearTargetAncestor(RawItem, Canvas)), RawItem, &Window);
        }
        const FGeometry Geometry = Item->GetCachedGeometry();
        if (Geometry.GetLocalSize().X < 1.f || Geometry.GetLocalSize().Y < 1.f) { return GearGuardReject(TEXT("tile_geometry_zero"), FString(), Item, &Window); }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
        if (!Canvas->GetCachedGeometry().IsUnderLocation(Position) || !Window.GetCachedGeometry().IsUnderLocation(Position))
        { return GearGuardReject(TEXT("tile_center_outside_canvas_or_window"), FString::Printf(TEXT("center=%s canvas_under=%d window_under=%d canvas_size=%s window_size=%s"), *Position.ToString(), Canvas->GetCachedGeometry().IsUnderLocation(Position), Window.GetCachedGeometry().IsUnderLocation(Position), *Canvas->GetCachedGeometry().GetLocalSize().ToString(), *Window.GetCachedGeometry().GetLocalSize().ToString()), Item, &Window); }
        FWidgetPath ItemPath;
        if (!FSlateApplication::Get().FindPathToWidget(Item->GetCachedWidget().ToSharedRef(), ItemPath)
            || !ItemPath.IsValid()) { return GearGuardReject(TEXT("FindPathToWidget_failed"), FString(), Item, &Window); }
        // Respect the current local window's hit-test grid, including overlays and all ancestors.
        FWidgetPath DownPath = FSlateApplication::Get().LocateWindowUnderMouse(Position, TArray<TSharedRef<SWindow>>{ItemPath.GetWindow()}, false, 0);
        if (!DownPath.IsValid() || !DownPath.ContainsWidget(Item->GetCachedWidget().Get())
            || !DownPath.ContainsWidget(Window.GetCachedWidget().Get()))
        { return GearGuardReject(TEXT("native_down_path_or_membership"), FString::Printf(TEXT("window_visible=%d window_minimized=%d down_valid=%d down_has_item=%d down_has_window=%d"), ItemPath.GetWindow()->IsVisible(), ItemPath.GetWindow()->IsWindowMinimized(), DownPath.IsValid(), DownPath.ContainsWidget(Item->GetCachedWidget().Get()), DownPath.ContainsWidget(Window.GetCachedWidget().Get())), Item, &Window); }
        for (int32 Index = 0; Index < DownPath.Widgets.Num(); ++Index)
        {
            const auto& SlateWidget = DownPath.Widgets[Index].Widget;
            const EVisibility Visibility = SlateWidget->GetVisibility();
            if (!SlateWidget->IsEnabled() || !Visibility.IsVisible()
                || (Index + 1 < DownPath.Widgets.Num() && !Visibility.AreChildrenHitTestVisible())
                || (Index + 1 == DownPath.Widgets.Num() && !Visibility.IsHitTestVisible()))
            { return GearGuardReject(TEXT("slate_path_visibility_or_enabled"), FString::Printf(TEXT("index=%d path_count=%d enabled=%d visible=%d children_hit=%d self_hit=%d"), Index, DownPath.Widgets.Num(), SlateWidget->IsEnabled(), Visibility.IsVisible(), Visibility.AreChildrenHitTestVisible(), Visibility.IsHitTestVisible()), Item, &Window); }
        }
        TArray<FWidgetAndPointer> ReleaseWidgets;
        for (int32 Index = 0; Index < DownPath.Widgets.Num(); ++Index)
        {
            const auto& Arranged = DownPath.Widgets[Index];
            ReleaseWidgets.Emplace(Arranged, DownPath.GetVirtualPointerPosition(Index));
            if (Arranged.Widget == Window.GetCachedWidget().ToSharedRef()) { break; }
        }
        const FWidgetPath ReleasePath(MakeArrayView(ReleaseWidgets));
        if (!ReleasePath.IsValid() || !ReleasePath.ContainsWidget(Window.GetCachedWidget().Get())) { return GearGuardReject(TEXT("stable_release_path_invalid"), FString(), Item, &Window); }
        if (!bGearHitPathLogged)
        {
            bGearHitPathLogged = true;
            Event(TEXT("gear_ui_hit_path_verified"), FString::Printf(TEXT("item_visibility=%s window_visible=%d window_minimized=%d path_count=%d leaf_type=%s item_member=%d window_member=%d; actual_hit_path_before_RMB_not_equip_success"),
                *UEnum::GetValueAsString(Item->GetVisibility()), ItemPath.GetWindow()->IsVisible(), ItemPath.GetWindow()->IsWindowMinimized(),
                DownPath.Widgets.Num(), *DownPath.Widgets.Last().Widget->GetTypeAsString(), DownPath.ContainsWidget(Item->GetCachedWidget().Get()), DownPath.ContainsWidget(Window.GetCachedWidget().Get())));
        }
        // Local game Slate routing only. No OS cursor/focus or Controller RMB attack input.
        TSet<FKey> Pressed; Pressed.Add(EKeys::RightMouseButton);
        const FPointerEvent Down(0, Position, Position, Pressed, EKeys::RightMouseButton, 0.f, FModifierKeysState());
        const FReply Reply = FSlateApplication::Get().RoutePointerDownEvent(DownPath, Down);
        // The item may rebuild/disappear synchronously. Release to the stable window path.
        const FPointerEvent Up(0, Position, Position, TSet<FKey>(), EKeys::RightMouseButton, 0.f, FModifierKeysState());
        FSlateApplication::Get().RoutePointerUpEvent(ReleasePath, Up);
        bGearUiClickRouted = true;
        Event(TEXT("gear_ui_right_pointer_routed"), FString::Printf(TEXT("handled=%d guid=%s cooldown_after=%.3f; reply_is_not_equip_success"),
            Reply.IsEventHandled(), *SwordId.ToString(EGuidFormats::Digits), Combat->GetAttackCooldownRemaining()));
        return true;
    }
    bool VerifyGearEquipped()
    {
        if (!Inventory.IsValid() || !State.IsValid() || !Combat.IsValid() || GearEquippedGuidCount() != 1) { return false; }
        bool CorrectSlot = false;
        const auto Scan = [&](const TArray<FRogue10mInventorySlot>& Slots)
        {
            for (const auto& Slot : Slots)
            {
                if (Slot.SlotType == ERogue10mInventorySlotType::MainWeapon && !Slot.bLocked && Slot.bEquipped && Slot.bHasEquippedItem
                    && Slot.EquippedItem.bOccupied && Slot.EquippedItem.InstanceId == SwordId && Slot.EquippedItem.ItemData
                    && Slot.EquippedItem.ItemData->GetPathName() == GearExpectedItemPath) { CorrectSlot = true; }
            }
        };
        Scan(Inventory->GetLeftEquipmentSlots()); Scan(Inventory->GetRightEquipmentSlots());
        const auto* Primary = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
        return CorrectSlot && !HasSword() && State->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
            && Primary && Primary->GetPathName() == GearExpectedPrimaryPath && Primary->GetPathName() != GearBeforePrimaryPath;
    }
    bool HasSword() const
    {
        if (!Inventory.IsValid() || !SwordId.IsValid()) { return false; }
        for (const auto& Container : Inventory->GetInventoryContainers()) for (const auto& Item : Container.Entries)
        { if (Item.InstanceId == SwordId && Item.ItemData && Item.ItemData->WeaponType == ERogue10mWeaponType::GreatSword) { return true; } }
        return false;
    }
    int64 Defeats() const
    {
        int64 Count = 0;
        if (State.IsValid()) { const auto Growth = State->CaptureGrowth(); for (const auto& Row : Growth.MonsterDefeats) { Count += FMath::Max(0, Row.Value); } }
        return Count;
    }
    bool SelectLearnNode(URogue10mHybridSkillTreeWidget& Widget)
    {
        auto* Entry = Widget.FindNodeEntry(TEXT("I.InternalEnergy.Entry"));
        if (!Entry) { return false; }
        URogue10mHybridSkillTreeButton* Found = nullptr; int32 Count = 0;
        for (TObjectIterator<URogue10mHybridSkillTreeButton> It; It; ++It)
        { if (It->GetTypedOuter<URogue10mHybridSkillTreeEntryWidget>() == Entry && Displayed(*It)) { Found = *It; ++Count; } }
        if (Count != 1) { return false; }
        Event(TEXT("UMG_node_button_delegate"), TEXT("I.InternalEnergy.Entry")); Found->OnClicked.Broadcast(); return true;
    }
    void Advance(float Delta)
    {
        if (Stage == TEXT("main_menu"))
        { if (Click(VisibleWidget<URogue10mMainMenuWidget>(), TEXT("UI_StartGameButton"))) { bMenuUI = true; SetStage(TEXT("character_create")); } }
        else if (Stage == TEXT("character_create"))
        {
            auto* Lobby = VisibleWidget<URogue10mCharacterLobbyWidget>();
            auto* Input = Lobby ? Cast<UEditableTextBox>(Lobby->GetWidgetFromName(TEXT("UI_CharacterNameInput"))) : nullptr;
            if (Displayed(Input)) { SetStage(TEXT("character_name")); return; }
            if (Click(Lobby, TEXT("UI_NewCharacterButton"))) { SetStage(TEXT("character_name")); }
        }
        else if (Stage == TEXT("character_name"))
        {
            auto* Lobby = VisibleWidget<URogue10mCharacterLobbyWidget>();
            auto* Input = Lobby ? Cast<UEditableTextBox>(Lobby->GetWidgetFromName(TEXT("UI_CharacterNameInput"))) : nullptr;
            if (Displayed(Input))
            {
                if (!bNameFocused) { Input->SetKeyboardFocus(); bNameFocused = true; ActionAt = Now() + 0.3; return; }
                if (!bNameTyped)
                {
                    if (!FSlateApplication::IsInitialized() || !Input->HasKeyboardFocus()) { Block(TEXT("character_name_Slate_focus_missing")); return; }
                    const FString Name(TEXT("BasicQa"));
                    for (TCHAR C : Name) { FCharacterEvent Typed(C, FModifierKeysState(), 0, false); FSlateApplication::Get().ProcessKeyCharEvent(Typed); }
                    Event(TEXT("Slate_character_events"), TEXT("BasicQa; no profile creation API or text property setter"));
                    bNameTyped = true; ActionAt = Now() + 0.3; return;
                }
                if (Input->GetText().ToString() != TEXT("BasicQa")) { Block(TEXT("character_name_Slate_events_did_not_fill_visible_input")); return; }
                if (Click(Lobby, TEXT("UI_CreateCharacterButton"))) { SetStage(TEXT("character_enter")); }
            }
        }
        else if (Stage == TEXT("character_enter"))
        { if (Click(VisibleWidget<URogue10mCharacterLobbyWidget>(), TEXT("UI_EnterGameButton"))) { SetStage(TEXT("wait_hub")); } }
        else if (Stage == TEXT("wait_hub"))
        {
            if (Map() == TEXT("StartHub") && Character.IsValid() && State.IsValid() && Saves && Saves->IsSaveReady())
            {
                auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>(); const auto* Profile = Profiles ? Profiles->GetSelectedProfile() : nullptr;
                if (!Profile || !Profile->IsValid()) { Block(TEXT("normal_character_UI_did_not_select_profile")); return; }
                ProfileId = Profile->ProfileId; EntryGold = Inventory->GetGold();
                SetStage(TEXT("npc_approach"));
            }
        }
        else if (Stage == TEXT("npc_approach"))
        {
            if (!NPC.IsValid()) for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
            if (NPC.IsValid() && Walk(NPC->GetActorLocation() + FVector(-100.f, -50.f, 0.f), 20.f, Delta, false))
            {
                Event(TEXT("npc_arrived"), FString::Printf(TEXT("radius=%.3f distance=%.3f"), NPC->InteractionRadius, FVector::Distance(NPC->GetActorLocation(), Character->GetActorLocation())));
                Event(TEXT("npc_readonly_state"), FString::Printf(TEXT("npc=%s class=%s location=%s scale=%s pawn_location=%s move_ignored=%d blocking=%d paused=%d widget=%s"),
                    *NPC->GetName(), *NPC->GetClass()->GetPathName(), *NPC->GetActorLocation().ToString(), *NPC->GetActorScale3D().ToString(), *Character->GetActorLocation().ToString(),
                    PC->IsMoveInputIgnored(), PC->IsAnyBlockingWindowVisible(), UGameplayStatics::IsGamePaused(World.Get()), *GetNameSafe(NPC->GetTutorialWidget())));
                TArray<UPrimitiveComponent*> Components; NPC->GetComponents(Components);
                Components.Add(Character->GetCapsuleComponent());
                for (const UPrimitiveComponent* Component : Components)
                {
                    const auto* Sphere = Cast<USphereComponent>(Component);
                    Event(TEXT("npc_readonly_collision"), FString::Printf(TEXT("owner=%s component=%s profile=%s enabled=%d object=%d PawnResponse=%d overlap_events=%d overlapping_pawn=%d sphere_radius=%.3f location=%s"),
                        *GetNameSafe(Component->GetOwner()), *Component->GetName(), *Component->GetCollisionProfileName().ToString(), static_cast<int32>(Component->GetCollisionEnabled()),
                        static_cast<int32>(Component->GetCollisionObjectType()), static_cast<int32>(Component->GetCollisionResponseToChannel(ECC_Pawn)),
                        Component->GetGenerateOverlapEvents(), Component->IsOverlappingActor(Character.Get()), Sphere ? Sphere->GetScaledSphereRadius() : -1.f, *Component->GetComponentLocation().ToString()));
                }

                SetStage(TEXT("npc_open"), 15.);
            }
        }
        else if (Stage == TEXT("npc_open"))
        {
            if (NPC.IsValid() && NPC->IsDialogueOpen()) { bNpcDialogue = true; SetStage(TEXT("npc_supplies"), 15.); }
            else if (NPC.IsValid() && NPC->IsWithinSkillTreeServiceRange(Character.Get()))
            {
                const UUserWidget* Prompt = NPC->GetTutorialWidget();
                if (Prompt && Prompt->IsInViewport() && Prompt->IsVisible() && Now() > ActionAt)
                { Pulse(EKeys::F); ActionAt = Now() + 0.6; }
            }
        }
        else if (Stage == TEXT("npc_supplies"))
        { if (Now() > ActionAt) { SetStage(TEXT("npc_close"), 15.); Pulse(EKeys::F); ActionAt = Now() + 0.5; } }
        else if (Stage == TEXT("npc_close"))
        {
            if (NPC.IsValid() && NPC->IsDialogueOpen() && Now() > ActionAt) { Pulse(EKeys::Escape); ActionAt = Now() + 0.5; }
            else if (NPC.IsValid() && !NPC->IsDialogueOpen()) { Route = { FVector(650, -1100, 0), FVector(650, 950, 0), FVector(0, 1800, 0) }; Waypoint = 0; SetStage(TEXT("hub_route")); }
        }
        else if (Stage == TEXT("hub_route"))
        { if (Waypoint < Route.Num()) { if (Walk(Route[Waypoint], 120.f, Delta, false)) { ++Waypoint; } } else { SetStage(TEXT("portal_walk")); } }
        else if (Stage == TEXT("portal_walk"))
        {
            if (Map() == TEXT("OpenRunMap")) { SetStage(TEXT("field_ready")); return; }
            if (!Portal.IsValid()) for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It) { Portal = *It; break; }
            if (Portal.IsValid()) { Walk(Portal->GetActorLocation(), 15.f, Delta, false); }
        }
        else if (Stage == TEXT("field_ready"))
        {
            if (Character.IsValid() && Run.IsValid() && Run->IsRunActive() && Saves && Saves->IsSaveReady() && StreamingReady())
            {
                if (!FMath::IsNearlyEqual(Run->GetRunDurationSeconds(), 1200.f)) { Block(TEXT("normal_1200_second_rule_missing")); return; }
                bFieldEntered = true; InitialDefeats = Defeats(); SetStage(TEXT("sword_overlap"), 120.);
            }
        }
        else if (Stage == TEXT("sword_overlap"))
        {
            if (!Sword.IsValid() && !SwordId.IsValid()) for (TActorIterator<ARogue10mDroppedItem> It(World.Get()); It; ++It)
            { if (It->ActorHasTag(TEXT("Rogue10m.EntryGreatSword.Pickup"))) { Sword = *It; SwordId = It->GetItemStack().InstanceId; break; } }
            if (HasSword()) { bSword = true; Route = { FVector(-25000, -45000, 0), FVector(-45000, -29000, 0), FVector(-47800, -25000, 0) }; Waypoint = 0; SetStage(TEXT("forest_route"), 240.); }
            else if (Sword.IsValid()) { Walk(Sword->GetActorLocation(), 20.f, Delta, false); }
        }
        else if (Stage == TEXT("forest_route"))
        {
            if (Waypoint < Route.Num()) { if (Walk(Route[Waypoint], 180.f, Delta)) { ++Waypoint; } }
            else { SetStage(TEXT("growth_combat"), 300.); }
        }
        else if (Stage == TEXT("growth_combat"))
        {
            if (Defeats() > InitialDefeats && (State->GetCurrentExperience() > 0 || State->GetPlayerLevel() > 1)) { if (!bFirstKill) { bFirstKill = true; Event(TEXT("natural_kill_XP_observed"), TEXT("credited defeat counter and XP increased; no XP/HP setters")); Shot(TEXT("first_kill")); } }
            if (Tree->GetAvailableGrowthPoints() > 0) { SetStage(TEXT("skill_open"), 30.); Pulse(EKeys::K); }
            else { Fight(Delta, false); }
        }
        else if (Stage == TEXT("skill_open"))
        { if (auto* Widget = VisibleWidget<URogue10mHybridSkillTreeWidget>(false); PC->IsSkillTreeVisible() && Widget && SelectLearnNode(*Widget)) { SetStage(TEXT("skill_learn"), 30.); } }
        else if (Stage == TEXT("skill_learn"))
        {
            auto* Widget = VisibleWidget<URogue10mHybridSkillTreeWidget>(false);
            if (PC->IsSkillTreeVisible() && Widget && Click(Widget, TEXT("HybridTreeLearnPoints"))) { SetStage(TEXT("skill_confirm"), 30.); }
        }
        else if (Stage == TEXT("skill_confirm"))
        {
            if (Tree->IsNodeUnlocked(TEXT("I.InternalEnergy.Entry")))
            {
                bLearned = true; LearnedNode = TEXT("I.InternalEnergy.Entry");
                Route = { FVector(-44000,-4000,0), FVector(-44000,-1100,0), FVector(-44000,5000,0), FVector(-44000,13500,0),
                    FVector(-35000,42000,0), FVector(-2000,59000,0), FVector(11695,59741,0), FVector(12000,60000,0),
                    FVector(14700,62300,0), FVector(15000,62600,0), FVector(15650,64400,0), FVector(15000,68000,0) };
                Waypoint = 0;
                if (bGearUiEquipment) { SetStage(TEXT("gear_close_skill"), 15.); Pulse(EKeys::K); }
                else { SetStage(TEXT("boss_route"), 400.); Pulse(EKeys::K); }
            }
        }
        else if (Stage == TEXT("gear_close_skill"))
        {
            StopMove();
            if (PC->IsSkillTreeVisible()) { if (Now() > ActionAt) { Pulse(EKeys::K); ActionAt = Now() + .5; } return; }
            if (!PrepareGearTarget()) { Block(TEXT("gear_ui_exact_sword_or_distinct_loaded_profile_unavailable")); return; }
            SetStage(TEXT("gear_open_inventory"), 15.); Pulse(EKeys::I);
        }
        else if (Stage == TEXT("gear_open_inventory"))
        {
            auto* Window = VisibleWidget<URogue10mInventoryWindowWidget>();
            if (!Window || !PC->IsInventoryVisible()) { GearGuardReject(TEXT("outer_window_lookup"), FString::Printf(TEXT("window_found=%d inventory_flag=%d"), Window != nullptr, PC->IsInventoryVisible()), Window, Window); return; }
            if (!bGearBeforeQueued) { bGearBeforeQueued = true; Shot(TEXT("gear_ui_before")); ActionAt = Now() + .6; return; }
            if (!PendingShot.IsEmpty() || ShotQueue.Num() > 0) { GearGuardReject(TEXT("outer_screenshot_pending"), FString::Printf(TEXT("pending=%s queued=%d"), *PendingShot, ShotQueue.Num()), Window, Window); return; }
            if (RouteGearRightClick(*Window) && !bFinishing) { SetStage(TEXT("gear_confirm"), 15.); }
        }
        else if (Stage == TEXT("gear_confirm"))
        {
            if (!VerifyGearEquipped()) { return; } // No repeated RMB on a handled/uncertain outcome.
            bGearUiEquipped = true;
            Event(TEXT("gear_ui_equipped_verified"), FString::Printf(TEXT("guid=%s type=GreatSword primary=%s; actual_equipment_and_binding_observed"),
                *SwordId.ToString(EGuidFormats::Digits), *GearExpectedPrimaryPath));
            if (const auto* Charged = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedPrimary))
            { const auto Budget = FRogue10mAttackResourceBudget::Build(Charged->ResourceCosts, 1.f); Event(TEXT("gear_ui_charge_plan_only"), FString::Printf(TEXT("skill=%s hold=%.3f cost_valid=%d stamina=%.3f; no_charge_input_requested"), *Charged->GetPathName(), Charged->ChargeSeconds, Budget.bValid, Budget.Stamina)); }
            Shot(TEXT("gear_ui_after")); SetStage(TEXT("gear_close_inventory"), 15.);
        }
        else if (Stage == TEXT("gear_close_inventory"))
        {
            if (!PendingShot.IsEmpty() || ShotQueue.Num() > 0) { return; }
            if (PC->IsInventoryVisible()) { if (Now() > ActionAt) { Pulse(EKeys::I); ActionAt = Now() + .5; } return; }
            SetStage(TEXT("boss_route"), 400.);
        }
        else if (Stage == TEXT("boss_route"))
        {
            if (PC->IsSkillTreeVisible()) { if (Now() > ActionAt) { Pulse(EKeys::K); ActionAt = Now() + 0.5; } return; }
            if (Waypoint < Route.Num())
            {
                const bool Bridge = Waypoint >= 1 && Waypoint <= 3;
                const bool RampLane = Waypoint >= 6 && Waypoint <= 9;
                if (Walk(Route[Waypoint], RampLane ? 20.f : (Bridge ? 140.f : 200.f), Delta, !Bridge && !RampLane, Bridge || RampLane))
                {
                    Event(TEXT("waypoint_reached"), FString::Printf(TEXT("index=%d point=%s narrow=%d"), Waypoint, *Route[Waypoint].ToString(), Bridge || RampLane));
                    ++Waypoint;
                    if (RampLane) { StopMove(); ActionAt = Now() + 0.5; } // Release keys; allow normal braking before the next turn.
                }
            }
            else { Target = nullptr; SetStage(TEXT("boss_combat"), 1205.); }
        }
        else if (Stage == TEXT("boss_combat")) { Fight(Delta, true); }
        else if (Stage == TEXT("result"))
        {
            auto* Result = PC->GetRunResultWidget();
            if (PC->IsRunResultVisible() && Result && Now() > ActionAt)
            {
                Event(TEXT("result_save_gate"), Saves ? Saves->GetLastError().ToString() : TEXT("no_progression"));
                if (Click(Result, TEXT("RunResultReturnToTown"))) { ++ReturnClicks; ActionAt = Now() + 2.; }
                if (ReturnClicks > 5) { Block(TEXT("result_return_retries_exhausted")); }
            }
        }
    }
    bool Tick(float Delta)
    {
        if (bFinished) { return false; }
        Refresh(); PumpShots();
        RecordInputFrame(Delta);
        TArray<FKey> Released; for (const auto& Pair : Releases) { if (Now() >= Pair.Value) { Key(Pair.Key, false); Released.Add(Pair.Key); } }
        for (const FKey& K : Released) { Releases.Remove(K); }
        if (Now() - LastSample >= 0.25) { Sample(); LastSample = Now(); }
        if (bFinishing)
        {
            if (Now() >= FinishAt) { ReleaseAll(); FlushFrames(); Summary(Outcome); StopAnimationObservation(); bFinished = true; FCoreDelegates::OnPreExit.Remove(ExitHandle); FPlatformMisc::RequestExitWithStatus(false, Outcome.StartsWith(TEXT("returned_")) ? 0 : 2); return false; }
            return true;
        }
        if (bIoFailed) { Block(TEXT("evidence_write_failed")); return true; }
        if (Now() >= Deadline) { Block(TEXT("bounded_wall_watchdog")); return true; }
        if (World.IsValid() && bFieldEntered && Run.IsValid() && Run->IsRunTerminal() && Stage != TEXT("result"))
        {
            const auto Phase = Run->GetRunPhase(); TerminalPhase = StaticEnum<ERogue10mRunPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
            if (Phase == ERogue10mRunPhase::Abandoned) { Block(TEXT("unexpected_abandonment_not_a_natural_death_or_victory")); return true; }
            Natural = Phase == ERogue10mRunPhase::Victory ? TEXT("victory") : (Run->GetRemainingRunSeconds() <= 0.f ? TEXT("timeout") : TEXT("death"));
            Event(TEXT("natural_terminal"), Natural + TEXT(" reached_boss=") + (bBossReached ? TEXT("1") : TEXT("0")));
            SetStage(TEXT("result"), 120.);
        }
        if (Stage == TEXT("result") && Map() == TEXT("StartHub") && Character.IsValid() && State.IsValid() && Saves && Saves->IsSaveReady())
        {
            const bool Reset = Saves->GetAttachedProfileId() == ProfileId && Run.IsValid() && Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart
                && !World->URL.HasOption(TEXT("StartRun")) && State->GetPlayerLevel() == 1 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler) == 0
                && (!bLearned || !Tree->IsNodeUnlocked(LearnedNode)) && !HasSword()
                && (!bGearUiEquipment || (GearEquippedGuidCount() == 0 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::GreatSword) == 0));
            const bool Discovery = !bLearned || Saves->HasDiscoveredNode(LearnedNode);
            if (!Reset || !Discovery) { Block(TEXT("natural_return_did_not_reset_run_or_preserve_discovery")); return true; }
            bReturned = true; Outcome = TEXT("returned_") + Natural; Reason = TEXT("natural terminal and UI return observed; milestone flags remain separate");
            Event(TEXT("returned"), Outcome); ReleaseAll(); Shot(TEXT("returned_hub")); bFinishing = true; FinishAt = Now() + 1.5; Summary(Outcome); return true;
        }
        if (Now() - StageAt > StageLimit) { Block(TEXT("stage_timeout_") + Stage); return true; }
        if (Now() >= ActionAt && World.IsValid() && PC.IsValid()) { Advance(Delta); }
        return true;
    }
    void FlushFrames()
    {
        if (!InputBuffer.IsEmpty()) { Append(TEXT("input-frames.csv"), InputBuffer); InputBuffer.Reset(); }
        LastInputFlush = Now();
    }
    void RecordInputFrame(float Delta)
    {
        if (++InputFrames > 120000) { return; }
        const FVector P = Character.IsValid() ? Character->GetActorLocation() : FVector::ZeroVector;
        const FVector Input = Character.IsValid() ? Character->GetLastMovementInputVector() : FVector::ZeroVector;
        const FRotator Rotation = PC.IsValid() ? PC->GetControlRotation() : FRotator::ZeroRotator;
        const auto Down = [this](const FKey& K) { return PC.IsValid() && PC->IsInputKeyDown(K); };
        InputBuffer += FString::Printf(TEXT("%llu,%.3f,%s,%s,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.4f,%.4f,%d,%d,%d,%d,%d,%.3f\n"),
            static_cast<unsigned long long>(GFrameCounter),Now()-Started,*Csv(Stage),*Csv(Map()),Delta,World.IsValid()?World->GetDeltaSeconds():0.,P.X,P.Y,P.Z,Rotation.Yaw,Rotation.Pitch,
            Held.FindRef(EKeys::W),Down(EKeys::W),Held.FindRef(EKeys::A),Down(EKeys::A),Held.FindRef(EKeys::S),Down(EKeys::S),Held.FindRef(EKeys::D),Down(EKeys::D),Held.FindRef(EKeys::LeftShift),Down(EKeys::LeftShift),Down(EKeys::LeftAlt),Down(EKeys::LeftMouseButton),
            Input.X,Input.Y,PC.IsValid()&&PC->IsMoveInputIgnored(),PC.IsValid()&&PC->IsLookInputIgnored(),World.IsValid()&&UGameplayStatics::IsGamePaused(World.Get()),StreamingReady(),Waypoint,Run.IsValid()?Run->GetRemainingRunSeconds():0.);
        if (Now() - LastInputFlush >= 1.) { FlushFrames(); }
    }
    void SampleMovementDiagnostics()
    {
        const ARogue10mCharacter* Pawn = Character.Get();
        const UCharacterMovementComponent* Movement = Pawn ? Pawn->GetCharacterMovement() : nullptr;
        const auto* Attributes = Pawn ? Pawn->GetRogueAttributeSet() : nullptr;
        const UCapsuleComponent* Capsule = Pawn ? Pawn->GetCapsuleComponent() : nullptr;
        const USkeletalMeshComponent* Mesh = Pawn ? Pawn->GetMesh() : nullptr;
        const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
        const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
        const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
        const FVector Acceleration = Movement ? Movement->GetCurrentAcceleration() : FVector::ZeroVector;
        const FFindFloorResult* Floor = Movement ? &Movement->CurrentFloor : nullptr;
        const FVector ImpactNormal = Floor ? Floor->HitResult.ImpactNormal : FVector::ZeroVector;
        const FVector HitNormal = Floor ? Floor->HitResult.Normal : FVector::ZeroVector;
        TArray<FString> Fields;
        const auto Number = [&Fields](double Value) { Fields.Add(FString::SanitizeFloat(Value)); };
        const auto Integer = [&Fields](int32 Value) { Fields.Add(FString::FromInt(Value)); };
        const auto Vector = [&Number](const FVector& Value) { Number(Value.X); Number(Value.Y); Number(Value.Z); };
        Number(Now() - Started); Fields.Add(Csv(Stage)); Fields.Add(Csv(Map()));
        Integer(Pawn != nullptr); Integer(Movement != nullptr); Integer(Attributes != nullptr);
        Integer(Capsule != nullptr); Integer(Mesh != nullptr); Integer(Anim != nullptr);
        Integer(Movement ? static_cast<int32>(Movement->MovementMode.GetValue()) : -1);
        Integer(Movement ? static_cast<int32>(Movement->CustomMovementMode) : -1);
        Number(Movement ? Movement->MaxWalkSpeed : -1.); Number(Movement ? Movement->GetMaxSpeed() : -1.);
        Vector(Velocity); Vector(Acceleration);
        Integer(Movement ? Movement->IsActive() : -1); Integer(Movement ? Movement->IsComponentTickEnabled() : -1);
        Integer(Pawn ? Pawn->CanJump() : -1); Integer(Pawn ? Pawn->JumpCurrentCount : -1);
        Integer(Pawn ? Pawn->IsDodging() : -1); Integer(Pawn ? Pawn->IsSprinting() : -1);
        Number(Attributes ? Attributes->GetStamina() : -1.); Number(Attributes ? Attributes->GetMaxStamina() : -1.);
        Number(Capsule ? Capsule->GetScaledCapsuleRadius() : -1.); Number(Capsule ? Capsule->GetScaledCapsuleHalfHeight() : -1.);
        Integer(Mesh ? Mesh->IsPlayingRootMotion() : -1); Integer(Pawn ? Pawn->IsPlayingRootMotion() : -1);
        Fields.Add(Csv(Montage ? Montage->GetPathName() : FString()));
        Integer(Floor ? static_cast<int32>(Floor->bBlockingHit) : -1);
        Integer(Floor ? Floor->IsWalkableFloor() : -1); Integer(Floor ? static_cast<int32>(Floor->bLineTrace) : -1);
        Vector(ImpactNormal); Vector(HitNormal);
        Number(Floor ? Floor->FloorDist : -1.); Number(Floor ? Floor->LineDist : -1.);
        Fields.Add(Csv(Floor ? GetPathNameSafe(Floor->HitResult.GetActor()) : FString()));
        Fields.Add(Csv(Floor ? GetPathNameSafe(Floor->HitResult.GetComponent()) : FString()));
        Append(TEXT("movement-diagnostics.csv"), FString::Join(Fields, TEXT(",")) + TEXT("\n"));
    }
    void Sample()
    {
        const auto* A = Character.IsValid() ? Character->GetRogueAttributeSet() : nullptr;
        const FVector P = Character.IsValid() ? Character->GetActorLocation() : FVector::ZeroVector;
        const auto* TA = Target.IsValid() ? Target->GetRogueAttributeSet() : nullptr;
        const FString Phase = Run.IsValid() ? StaticEnum<ERogue10mRunPhase>()->GetNameStringByValue(static_cast<int64>(Run->GetRunPhase())) : FString();
        Append(TEXT("samples.csv"), FString::Printf(TEXT("%.3f,%s,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%d,%d,%d,%s,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
            Now()-Started,*Csv(Stage),*Csv(Map()),*Csv(Phase),World.IsValid()?World->GetTimeSeconds():0.,Run.IsValid()?Run->GetRemainingRunSeconds():0.,
            P.X,P.Y,P.Z,Character.IsValid()?Character->GetVelocity().Size():0.,A?A->GetHealth():0.,A?A->GetMaxHealth():0.,State.IsValid()?State->GetPlayerLevel():0,
            State.IsValid()?State->GetCurrentExperience():0.,Tree.IsValid()?Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler):0,Inventory.IsValid()?Inventory->GetGold():0,
            Saves&&Saves->IsSaveReady(),*Csv(Target.IsValid()?Target->GetName():FString()),TA?TA->GetHealth():0.,Held.FindRef(EKeys::W),Held.FindRef(EKeys::A),Held.FindRef(EKeys::S),Held.FindRef(EKeys::D),Held.FindRef(EKeys::LeftShift),Held.FindRef(EKeys::LeftMouseButton),bFirstKill,bSword,bLearned,bBossReached));
        SampleMovementDiagnostics();
        Summary(bFinishing?Outcome:TEXT("running"));
    }
    // Optional read-only evidence. It never generates or delays a gameplay input.
    void StopAnimationObservation()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(AnimationTickHandle); AnimationTickHandle.Reset();
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(AnimationProcessedHandle); AnimationProcessedHandle.Reset();
    }
    bool AnimationIdle() const
    {
        return Character.IsValid() && Combat.IsValid() && PC.IsValid() && !Character->IsDead()
            && !Character->IsDodging() && !Combat->IsWeaponChangeBlocked() && !Combat->IsAttackChargeHeld()
            && Combat->GetAttackCooldownRemaining() <= .001f && !Combat->IsComboSequenceActive(World->GetTimeSeconds())
            && !PC->IsInputKeyDown(EKeys::LeftMouseButton);
    }
    int32 AnimationUses() const
    { return State.IsValid() && AnimationSkill.IsValid() ? State->GetSkillUseCount(AnimationSkill->GetFName()) : -1; }
    bool AnimationPhaseMatches(const FString& Phase) const
    {
        if (!World.IsValid() || !Character.IsValid() || !Combat.IsValid() || !bAnimationContextCaptured || bAnimationContextInvalidated
            || AnimationProbeWorld.Get()!=World.Get() || AnimationProbePawn.Get()!=Character.Get()) { return false; }
        const auto* Mesh=Character->GetAnimationPlaybackMesh(); const auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
        if (Phase==TEXT("attack_ready")) { return AnimationIdle() && AnimationUses()==AnimationUsesBefore && Target==AnimationTarget; }
        if (Phase==TEXT("attack_first_damage"))
        { const auto* A=AnimationTarget.IsValid()?AnimationTarget->GetRogueAttributeSet():nullptr; return bAnimationDamageSeen && !bAnimationSuperseded && AnimationUses()==AnimationUsesBefore+1 && A && A->GetHealth()<AnimationTargetHPBefore && Combat->IsWeaponChangeBlocked() && Anim && AnimationAttackMontage.IsValid() && Anim->Montage_IsActive(AnimationAttackMontage.Get()); }
        if (Phase==TEXT("attack_recovered"))
        { return bAnimationAccepted && !bAnimationSuperseded && AnimationUses()==AnimationUsesBefore+1 && AnimationIdle() && (!Anim || !AnimationAttackMontage.IsValid() || !Anim->Montage_IsActive(AnimationAttackMontage.Get())); }
        if (Phase==TEXT("dodge_ready")) { return AnimationIdle() && !bAnimationDodgeSeen; }
        if (Phase==TEXT("dodge_active")) { return bAnimationDodgeSeen && !bAnimationDodgeSuperseded && Character->IsDodging(); }
        if (Phase==TEXT("dodge_recovered"))
        { return bAnimationDodgeEnded && !bAnimationDodgeSuperseded && !Character->IsDodging() && !Combat->IsWeaponChangeBlocked() && (!Anim || !AnimationDodgeMontage.IsValid() || !Anim->Montage_IsActive(AnimationDodgeMontage.Get())); }
        return false;
    }
    void AnimationRow(const TCHAR* EventName,const FString& Phase,bool Matches)
    {
        const auto* Mesh=Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr;
        const auto* Body=Character.IsValid()?Character->GetMesh():nullptr;
        const auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
        const UAnimMontage* Montage=Phase.StartsWith(TEXT("dodge"))?AnimationDodgeMontage.Get():AnimationAttackMontage.Get();
        const auto* MI=Anim&&Montage?Anim->GetInstanceForMontage(Montage):nullptr;
        const auto* A=AnimationTarget.IsValid()?AnimationTarget->GetRogueAttributeSet():nullptr;
        const FVector L=Mesh?Mesh->GetSocketLocation(TEXT("hand_l")):FVector::ZeroVector;
        const FVector R=Mesh?Mesh->GetSocketLocation(TEXT("hand_r")):FVector::ZeroVector;
        TArray<FString> F={Csv(EventName),Csv(Phase),LexToString(GFrameCounter),FString::SanitizeFloat(World.IsValid()?World->GetTimeSeconds():-1.),FString::SanitizeFloat(Now()-Started),Csv(Stage),Matches?TEXT("1"):TEXT("0"),
            Csv(GetPathNameSafe(AnimationSkill.Get())),Csv(GetPathNameSafe(Montage)),Csv(GetPathNameSafe(Mesh)),Csv(Mesh?GetPathNameSafe(Mesh->GetSkeletalMeshAsset()):FString()),Csv(Body?GetPathNameSafe(Body->GetSkeletalMeshAsset()):FString()),
            Csv(GetPathNameSafe(AnimationTarget.Get())),FString::SanitizeFloat(A?A->GetHealth():-1.f),LexToString(AnimationUses()),Combat.IsValid()&&Combat->IsWeaponChangeBlocked()?TEXT("1"):TEXT("0"),
            FString::SanitizeFloat(Combat.IsValid()?Combat->GetAttackCooldownRemaining():-1.f),Combat.IsValid()&&World.IsValid()&&Combat->IsComboSequenceActive(World->GetTimeSeconds())?TEXT("1"):TEXT("0"),
            Character.IsValid()&&Character->IsDodging()?TEXT("1"):TEXT("0"),FString::SanitizeFloat(Character.IsValid()?Character->GetDodgeCooldownRemaining():-1.f),FString::SanitizeFloat(Anim&&Montage?Anim->Montage_GetPosition(Montage):-1.f),
            FString::SanitizeFloat(MI?MI->GetPlayRate():-1.f),Anim&&Montage&&Anim->Montage_IsActive(Montage)?TEXT("1"):TEXT("0"),
            FString::SanitizeFloat(L.X),FString::SanitizeFloat(L.Y),FString::SanitizeFloat(L.Z),FString::SanitizeFloat(R.X),FString::SanitizeFloat(R.Y),FString::SanitizeFloat(R.Z)};
        Append(TEXT("animation-events.csv"),FString::Join(F,TEXT(","))+TEXT("\n"));
    }
    bool TryAnimationShot(const TCHAR* Phase)
    {
        // Never steal/overwrite another pending screenshot or alter input cadence for a picture.
        if (!PendingShot.IsEmpty() || !ShotQueue.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) { return false; }
        const int32 Before=ShotCount; Shot(*FString::Printf(TEXT("animation_%s"),Phase));
        if (ShotCount==Before || ShotQueue.IsEmpty()) { return false; }
        AnimationPhaseFiles.Add(ShotQueue.Last().Key,Phase); AnimationPhaseState.Add(Phase,TEXT("queued"));
        AnimationRow(TEXT("queued"),Phase,AnimationPhaseMatches(Phase)); return true;
    }
    void AnimationScreenshotProcessed()
    {
        const FString* Phase=AnimationPhaseFiles.Find(PendingShot); if (!Phase) { return; }
        const bool Matches=AnimationPhaseMatches(*Phase); const bool File=IFileManager::Get().FileSize(*PendingShot)>0;
        AnimationPhaseState.Add(*Phase,Matches&&File?TEXT("processed_phase_and_file_match_decode_pending"):TEXT("processed_phase_or_file_unconfirmed"));
        AnimationRow(TEXT("processed_not_exact_pixel_or_contact_time"),*Phase,Matches);
        // Do not reset PendingShot: the existing queue owns file observation/timeout and stage photos.
    }
    void AnimationMontageAssets(const TCHAR* Kind,const UAnimMontage* Montage)
    {
        Append(TEXT("animation-assets.txt"),FString::Printf(TEXT("%s_montage=%s\n%s_montage_length_s=%.9f\n"),Kind,*GetPathNameSafe(Montage),Kind,Montage?Montage->GetPlayLength():-1.f));
        if (Montage) for (int32 Slot=0;Slot<Montage->SlotAnimTracks.Num();++Slot)
        { const auto& Track=Montage->SlotAnimTracks[Slot]; for (int32 Index=0;Index<Track.AnimTrack.AnimSegments.Num();++Index)
          { const auto& Segment=Track.AnimTrack.AnimSegments[Index]; Append(TEXT("animation-assets.txt"),FString::Printf(TEXT("%s_slot%d_segment%d=%s start=%.9f end=%.9f montage_start=%.9f authored_rate=%.9f\n"),Kind,Slot,Index,*GetPathNameSafe(Segment.GetAnimReference().Get()),Segment.AnimStartTime,Segment.AnimEndTime,Segment.StartPos,Segment.AnimPlayRate)); } }
    }
    void ObserveAnimation()
    {
        if (!bAnimationEvidence || !bFieldEntered || !World.IsValid() || !Character.IsValid() || !Combat.IsValid() || !State.IsValid()) { return; }
        const bool CombatStage=Stage==TEXT("growth_combat") || Stage==TEXT("boss_combat");
        if (!bAnimationContextCaptured && CombatStage) { AnimationProbeWorld=World; AnimationProbePawn=Character; bAnimationContextCaptured=true; }
        if (!bAnimationContextCaptured) { return; }
        if (AnimationProbeWorld.Get()!=World.Get() || AnimationProbePawn.Get()!=Character.Get())
        { if (!bAnimationContextInvalidated) { bAnimationContextInvalidated=true; AnimationRow(TEXT("probe_context_changed_no_cross_world_or_pawn_recovery_claim"),TEXT("context"),false); } return; }
        if (CombatStage && Target.IsValid() && AnimationIdle())
        {
            if (!bAnimationReadyRequested)
            {
                AnimationSkill=Combat->ResolveAttackSkill(true,false,false); AnimationTarget=Target;
                AnimationUsesBefore=AnimationUses(); const auto* A=Target->GetRogueAttributeSet(); AnimationTargetHPBefore=A?A->GetHealth():-1.f;
                if (AnimationSkill.IsValid() && A && TryAnimationShot(TEXT("attack_ready"))) { bAnimationReadyRequested=true; }
            }
            else if (!bAnimationDodgeReadyRequested && TryAnimationShot(TEXT("dodge_ready"))) { bAnimationDodgeReadyRequested=true; }
        }
        if (bAnimationReadyRequested && !bAnimationAccepted && AnimationUses()>AnimationUsesBefore)
        {
            bAnimationAccepted=true; AnimationAttackMontage=AnimationSkill->AttackMontage.Get(); AnimationAcceptedWorld=World->GetTimeSeconds();
            bAnimationSuperseded=AnimationUses()!=AnimationUsesBefore+1 || Combat->GetLastAcceptedAttackSkill()!=AnimationSkill.Get();
            Append(TEXT("animation-assets.txt"),FString::Printf(TEXT("attack_skill=%s\nattack_authored_hit_delay_montage_s=%.9f\nattack_authored_play_rate=%.9f\nattack_authored_cooldown_s=%.9f\nattack_acceptance_observed_world_s=%.9f\nattack_acceptance_is_use_count_plus_actual_skill_observation_not_execution_delegate=1\n"),*AnimationSkill->GetPathName(),AnimationSkill->HitStartDelaySeconds,AnimationSkill->AnimationPlayRate,AnimationSkill->AttackCooldown,AnimationAcceptedWorld));
            AnimationMontageAssets(TEXT("attack"),AnimationAttackMontage.Get()); AnimationRow(TEXT("accepted_observed"),TEXT("attack"),!bAnimationSuperseded);
        }
        if (bAnimationAccepted)
        {
            if (AnimationUses()>AnimationUsesBefore+1 && !bAnimationSuperseded)
            { bAnimationSuperseded=true; AnimationRow(TEXT("next_attack_before_recovery_observation_no_input_delay_added"),TEXT("attack"),false); }
            const auto* A=AnimationTarget.IsValid()?AnimationTarget->GetRogueAttributeSet():nullptr;
            if (!bAnimationSuperseded && !bAnimationDamageSeen && A && A->GetHealth()<AnimationTargetHPBefore)
            {
                bAnimationDamageSeen=true; AnimationRow(TEXT("first_HP_delta_observed_attribution_not_a_damage_delegate"),TEXT("attack_first_damage"),AnimationPhaseMatches(TEXT("attack_first_damage")));
                if (!TryAnimationShot(TEXT("attack_first_damage"))) { AnimationPhaseState.Add(TEXT("attack_first_damage"),TEXT("missed_busy_capture_queue")); }
            }
            if (!bAnimationAttackRecoveryRequested && AnimationPhaseMatches(TEXT("attack_recovered")))
            { bAnimationAttackRecoveryRequested=true; if (!TryAnimationShot(TEXT("attack_recovered"))) { AnimationPhaseState.Add(TEXT("attack_recovered"),TEXT("missed_busy_capture_queue")); } }
        }
        const bool Dodging=Character->IsDodging();
        if (Dodging && !bAnimationDodgeWasActive)
        {
            if (!bAnimationDodgeSeen)
            {
                bAnimationDodgeSeen=true;
                const auto* Data=Combat->GetActiveDodgeSkill(); const auto* Component=Character->GetCharacterAnimationComponent();
                const auto* Motion=Component?Component->GetMotionData():nullptr; AnimationDodgeMontage=Motion?Motion->DodgeMontage.Get():nullptr;
                Append(TEXT("animation-assets.txt"),FString::Printf(TEXT("dodge_skill=%s\ndodge_motion=%s\ndodge_authored_distance_cm=%.9f\ndodge_authored_duration_s=%.9f\ndodge_authored_cooldown_s=%.9f\ndodge_authored_stamina_cost=%.9f\ndodge_distance_is_not_realized_travel=1\n"),*GetPathNameSafe(Data),*GetPathNameSafe(Motion),Data?Data->Distance:-1.f,Data?Data->Duration:-1.f,Data?Data->Cooldown:-1.f,Data?Data->StaminaCost:-1.f));
                AnimationMontageAssets(TEXT("dodge"),AnimationDodgeMontage.Get()); AnimationRow(TEXT("active_observed"),TEXT("dodge_active"),true);
                if (!TryAnimationShot(TEXT("dodge_active"))) { AnimationPhaseState.Add(TEXT("dodge_active"),TEXT("missed_busy_capture_queue")); }
            }
            else { bAnimationDodgeSuperseded=true; }
        }
        if (!Dodging && bAnimationDodgeWasActive && bAnimationDodgeSeen) { bAnimationDodgeEnded=true; AnimationRow(TEXT("movement_end_observed_cooldown_separate"),TEXT("dodge"),true); }
        if (!bAnimationDodgeRecoveryRequested && AnimationPhaseMatches(TEXT("dodge_recovered")))
        { bAnimationDodgeRecoveryRequested=true; if (!TryAnimationShot(TEXT("dodge_recovered"))) { AnimationPhaseState.Add(TEXT("dodge_recovered"),TEXT("missed_busy_capture_queue")); } }
        bAnimationDodgeWasActive=Dodging;
    }
    void Shot(const TCHAR* Name)
    {
        if (ShotCount >= 64) { return; }
        const FString File = Output / TEXT("screenshots") / FString::Printf(TEXT("%02d_%s.png"), ++ShotCount, Name);
        ShotQueue.Add(TPair<FString,FString>(File, Stage));
    }
    void PumpShots()
    {
        if (!PendingShot.IsEmpty())
        {
            const int64 Bytes = IFileManager::Get().FileSize(*PendingShot);
            if (Bytes > 0 || Now() - ShotAt > 8.)
            {
                Append(TEXT("screenshots.csv"), FString::Printf(TEXT("%.3f,%s,%s,%s,%s,%lld\n"),Now()-Started,Bytes>0?TEXT("file_observed_not_visually_reviewed"):TEXT("capture_unconfirmed"),*Csv(PendingShot),*Csv(ShotStage),*Csv(Stage),Bytes));
                PendingShot.Reset();
            }
        }
        if (PendingShot.IsEmpty() && ShotQueue.Num())
        {
            const auto Next = ShotQueue[0]; ShotQueue.RemoveAt(0); PendingShot = Next.Key; ShotStage = Next.Value; ShotAt = Now();
            if (bAnimationEvidence) if (const FString* Phase=AnimationPhaseFiles.Find(PendingShot)) { AnimationRow(TEXT("request"),*Phase,AnimationPhaseMatches(*Phase)); }
            FScreenshotRequest::RequestScreenshot(PendingShot, true, false);
        }
    }
    void Summary(const FString& Status)
    {
        FString Text = FString::Printf(TEXT("status=%s\noutcome=%s\nstage=%s\nreason=%s\nnormal_menu_ui=%d\nnpc_dialogue=%d\nfield_entered=%d\nsword_overlap_pickup=%d\nfirst_natural_kill_xp=%d\nnormal_UMG_run_learning=%d\nboss_reached=%d\nreturned_reset_observed=%d\nfull_boss_completion=%d\nphysical_input=not_performed\ninput_driver=Controller_InputKey_DoAim_Slate_chars_UMG_delegates\nno_gameplay_fixtures=1\nwall_seconds=%.3f\nterminal_phase=%s\nprofile=%s\nrender_review=not_performed_by_driver\n"),
            *Status,*Outcome,*Stage,*Reason,bMenuUI,bNpcDialogue,bFieldEntered,bSword,bFirstKill,bLearned,bBossReached,bReturned,
            bReturned && bMenuUI && bNpcDialogue && bBossReached && bFirstKill && bLearned && bSword && (!bGearUiEquipment || bGearUiEquipped) && Natural == TEXT("victory"),Now()-Started,*TerminalPhase,*ProfileId.ToString(EGuidFormats::Digits));
        Text += FString::Printf(TEXT("gear_ui_requested=%d\ngear_ui_click_routed=%d\ngear_ui_equipped_verified=%d\n"), bGearUiEquipment, bGearUiClickRouted, bGearUiEquipped);
        Text += FString::Printf(TEXT("prepared_boss_strategy=%d\nprepared_dodge_requested=%d\nprepared_dodge_accepted=%d\nprepared_dodge_unobserved=%d\n"), bPreparedBossStrategy, PreparedDodgeRequested, PreparedDodgeAccepted, PreparedDodgeUnobserved);
        Text += FString::Printf(TEXT("pending_screenshot=%s\nqueued_screenshot_count=%d\nrequested_or_queued_capture_count=%d\nreset_scope=level_BrawlerXP_selected_learned_node_specific_sword_GUID_profile_phase_only\n"),*PendingShot,ShotQueue.Num(),ShotCount);
        if (bAnimationEvidence)
        {
            Text+=TEXT("animation_evidence=opportunistic_read_only\nanimation_phase_processed_is_not_exact_contact_or_GPU_time=1\nanimation_PNG_decode=not_performed_by_driver\n");
            for (const TCHAR* Phase:{TEXT("attack_ready"),TEXT("attack_first_damage"),TEXT("attack_recovered"),TEXT("dodge_ready"),TEXT("dodge_active"),TEXT("dodge_recovered")})
            { const FString* PhaseState=AnimationPhaseState.Find(Phase); Text+=FString::Printf(TEXT("animation_%s=%s\n"),Phase,PhaseState?**PhaseState:TEXT("not_captured_no_gameplay_timing_changed")); }
        }
        if (!FFileHelper::SaveStringToFile(Text, *(Output / TEXT("summary.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { bIoFailed = true; }
    }
    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<ARogue10mGameState> Run;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
    TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Tree;
    TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
    TWeakObjectPtr<ARogue10mRunPortal> Portal;
    TWeakObjectPtr<ARogue10mDroppedItem> Sword;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    URogue10mProgressionSubsystem* Saves = nullptr;
    TMap<FKey,bool> Held;
    TMap<FKey,double> Releases;
    TArray<FVector> Route;
    TArray<TPair<FString,FString>> ShotQueue;
    FString Output,Label,Stage,Outcome,Reason,Natural,TerminalPhase,PendingShot,ShotStage,InputBuffer;
    FGuid ProfileId,SwordId;
    FVector LastProgress;
    FRotator PreviousControl;
    bool bGearUiEquipment = false, bGearUiClickRouted = false, bGearUiEquipped = false, bGearBeforeQueued = false;
    int32 GearContainerIndex = INDEX_NONE;
    FString GearGuardLastReason;
    double GearGuardLastAt = 0.;
    bool bGearHitPathLogged = false;
    FString GearExpectedItemPath, GearExpectedPrimaryPath, GearBeforePrimaryPath;
    bool bPreparedBossStrategy = false, bPreparedReserveWaiting = false;
    int32 PreparedLastWindup = -1, PreparedPunishedAttack = -1, PreparedDodgeAttemptedAttack = -1;
    int32 PreparedDodgeRequested = 0, PreparedDodgeAccepted = 0, PreparedDodgeUnobserved = 0;
    uint64 PreparedDirectionFrame = 0;
    double PreparedDodgePendingAt = -1.;
    float PreparedDodgeStaminaBefore = 0.f, PreparedDodgeCooldownBefore = 0.f;
    float PreparedRange = 0.f, PreparedHalfWidth = 0.f, PreparedHalfHeight = 0.f, PreparedArc = 0.f;
    FVector PreparedLockedCenter = FVector::ZeroVector, PreparedLockedForward = FVector::ForwardVector, PreparedEscapeGoal = FVector::ZeroVector;
    ERogue10mMonsterAttackType PreparedType = ERogue10mMonsterAttackType::Basic;
    FDelegateHandle ExitHandle;
    FDelegateHandle AnimationTickHandle,AnimationProcessedHandle;
    TWeakObjectPtr<UWorld> AnimationProbeWorld;
    TWeakObjectPtr<ARogue10mCharacter> AnimationProbePawn;
    bool bAnimationContextCaptured=false,bAnimationContextInvalidated=false;
    TWeakObjectPtr<const URogue10mAttackSkillData> AnimationSkill;
    TWeakObjectPtr<ARogue10mBasicMonster> AnimationTarget;
    TWeakObjectPtr<UAnimMontage> AnimationAttackMontage,AnimationDodgeMontage;
    TMap<FString,FString> AnimationPhaseFiles,AnimationPhaseState;
    int32 AnimationUsesBefore=0;
    float AnimationTargetHPBefore=-1.f;
    double AnimationAcceptedWorld=-1.;
    bool bAnimationEvidence=false,bAnimationReadyRequested=false,bAnimationAccepted=false,bAnimationSuperseded=false,bAnimationDamageSeen=false,bAnimationAttackRecoveryRequested=false;
    bool bAnimationDodgeReadyRequested=false,bAnimationDodgeSeen=false,bAnimationDodgeWasActive=false,bAnimationDodgeEnded=false,bAnimationDodgeSuperseded=false,bAnimationDodgeRecoveryRequested=false;
    double Started=0,Deadline=0,StageAt=0,StageLimit=180,ActionAt=0,LastSample=0,StuckAt=0,RecoverUntil=0,NextAttack=0,NextDodge=0,FinishAt=0,ShotAt=0,LastInputFlush=0;
    int32 Waypoint=0,RecoveryCount=0,ReturnClicks=0,ShotCount=0,EntryGold=0,InputFrames=0;
    int64 InitialDefeats=0;
    FName LearnedNode;
    float PreviousYawInput=0.f,PreviousPitchInput=0.f,YawInputSign=1.f,PitchInputSign=1.f;
    bool bNameFocused=false,bNameTyped=false,bAimSample=false,bMenuUI=false,bFinished=false,bFinishing=false,bIoFailed=false,bFieldEntered=false,bNpcDialogue=false,bSword=false,bFirstKill=false,bLearned=false,bBossReached=false,bReturned=false;
};
static TSharedPtr<FDriver> Active;
static void StartDriver()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    { if (UWorld* World=Context.World(); World && Context.WorldType==EWorldType::Game) { Active=MakeShared<FDriver>(); Active->Start(*World); return; } }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBasicRunPlay"),TEXT("Strict fresh QA input-only run attempt; no fixtures and no physical-input claim."),FConsoleCommandDelegate::CreateStatic(&StartDriver));
}
#endif
