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
#include "Engine/Engine.h"
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
#include "Rogue10mAttackSkillData.h"
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
        Instance = Initial.GetGameInstance(); World = &Initial;
        if (!IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true)
            || !WriteNew(TEXT("samples.csv"), TEXT("wall,stage,map,phase,world_time,remaining,x,y,z,speed,hp,max_hp,level,xp,mastery,run_gold,save_ready,target,target_hp,w,a,s,d,shift,lmb,first_kill,sword,learned,boss_reached\n"))
            || !WriteNew(TEXT("movement-diagnostics.csv"), TEXT("wall,stage,map,has_pawn,has_movement,has_attributes,has_capsule,has_mesh,has_anim_instance,movement_mode,custom_movement_mode,max_walk_speed,max_speed,velocity_x,velocity_y,velocity_z,acceleration_x,acceleration_y,acceleration_z,movement_active,movement_tick_enabled,can_jump,jump_current_count,dodging,sprinting,stamina,max_stamina,capsule_radius,capsule_half_height,mesh_root_motion,character_root_motion,active_montage_path,floor_blocking,floor_walkable,floor_line_trace,floor_impact_normal_x,floor_impact_normal_y,floor_impact_normal_z,floor_hit_normal_x,floor_hit_normal_y,floor_hit_normal_z,floor_dist,line_dist,floor_actor_path,floor_component_path\n"))
            || !WriteNew(TEXT("events.csv"), TEXT("wall,event,stage,detail\n"))
            || !WriteNew(TEXT("screenshots.csv"), TEXT("wall,event,file,request_stage,observed_stage,bytes\n"))
            || !WriteNew(TEXT("input-frames.csv"), TEXT("frame,wall,stage,map,dt,world_dt,x,y,z,yaw,pitch,desired_w,actual_w,desired_a,actual_a,desired_s,actual_s,desired_d,actual_d,desired_shift,actual_shift,actual_alt,actual_lmb,last_input_x,last_input_y,move_ignored,look_ignored,paused,streaming_ready,waypoint,remaining\n")))
        { FPlatformMisc::RequestExitWithStatus(false, 3); return; }
        Started = Now(); Deadline = Started + Seconds; StageAt = Started;
        Stage = TEXT("main_menu");
        ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak = TWeakPtr<FDriver>(AsShared())]
        { if (auto Self = Weak.Pin()) { Self->ReleaseAll(); Self->FlushFrames(); Self->Summary(TEXT("process_pre_exit")); } });
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float Delta)
        { return Self->Tick(Delta); }), 0.f);
        Event(TEXT("started"), TEXT("input_only_programmatic; no fixtures; normal 1200-second game rule"));
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
        if (Distance <= Radius) { StopMove(); StuckAt = Now(); LastProgress = Position; return true; }
        if (!StreamingReady()) { StopMove(); return false; }
        const bool Facing = Face(FVector(Goal.X, Goal.Y, Position.Z), Delta);
        if (bFinishing) { return false; }
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
                    const float Radius = Capsule->GetScaledCapsuleRadius();
                    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
                    const FVector Start = Capsule->GetComponentLocation();
                    const FQuat Rotation = Capsule->GetComponentQuat();
                    FHitResult CapsuleHit;
                    World->SweepSingleByChannel(CapsuleHit, Start, Start + Toward * 500.f, Rotation,
                        ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
                    Event(TEXT("stuck_readonly_capsule_pawn_sweep"), FString::Printf(TEXT("blocking=%d start_penetrating=%d penetration_depth=%.3f actor=%s component=%s normal=%s point=%s distance=%.3f start=%s end=%s radius=%.3f half_height=%.3f"),
                        CapsuleHit.bBlockingHit, CapsuleHit.bStartPenetrating, CapsuleHit.PenetrationDepth,
                        *GetPathNameSafe(CapsuleHit.GetActor()), *GetPathNameSafe(CapsuleHit.GetComponent()),
                        *CapsuleHit.ImpactNormal.ToString(), *CapsuleHit.ImpactPoint.ToString(), CapsuleHit.Distance,
                        *Start.ToString(), *(Start + Toward * 500.f).ToString(), Radius, HalfHeight));
                    // Start just above the capsule's current top; this is a line query, not a headroom verdict.
                    const FVector CeilingStart = Start + Capsule->GetUpVector() * (HalfHeight + 1.f);
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
    void Fight(float Delta, bool Boss)
    {
        if (!Target.IsValid() || Target->IsDead()) { Target = Monster(Boss); }
        if (!Target.IsValid()) { StopMove(); return; }
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
                    FVector(-35000,42000,0), FVector(-2000,59000,0), FVector(12000,60000,0), FVector(13450,62100,0),
                    FVector(13800,64000,0), FVector(14700,66000,0), FVector(15000,68000,0) };
                Waypoint = 0; SetStage(TEXT("boss_route"), 400.); Pulse(EKeys::K);
            }
        }
        else if (Stage == TEXT("boss_route"))
        {
            if (PC->IsSkillTreeVisible()) { if (Now() > ActionAt) { Pulse(EKeys::K); ActionAt = Now() + 0.5; } return; }
            if (Waypoint < Route.Num()) { const bool Bridge = Waypoint >= 1 && Waypoint <= 3; if (Walk(Route[Waypoint], Bridge ? 140.f : 200.f, Delta, !Bridge, Bridge)) { ++Waypoint; } }
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
            if (Now() >= FinishAt) { ReleaseAll(); FlushFrames(); Summary(Outcome); bFinished = true; FCoreDelegates::OnPreExit.Remove(ExitHandle); FPlatformMisc::RequestExitWithStatus(false, Outcome.StartsWith(TEXT("returned_")) ? 0 : 2); return false; }
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
                && (!bLearned || !Tree->IsNodeUnlocked(LearnedNode)) && !HasSword();
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
            FScreenshotRequest::RequestScreenshot(PendingShot, true, false);
        }
    }
    void Summary(const FString& Status)
    {
        FString Text = FString::Printf(TEXT("status=%s\noutcome=%s\nstage=%s\nreason=%s\nnormal_menu_ui=%d\nnpc_dialogue=%d\nfield_entered=%d\nsword_overlap_pickup=%d\nfirst_natural_kill_xp=%d\nnormal_UMG_run_learning=%d\nboss_reached=%d\nreturned_reset_observed=%d\nfull_boss_completion=%d\nphysical_input=not_performed\ninput_driver=Controller_InputKey_DoAim_Slate_chars_UMG_delegates\nno_gameplay_fixtures=1\nwall_seconds=%.3f\nterminal_phase=%s\nprofile=%s\nrender_review=not_performed_by_driver\n"),
            *Status,*Outcome,*Stage,*Reason,bMenuUI,bNpcDialogue,bFieldEntered,bSword,bFirstKill,bLearned,bBossReached,bReturned,
            bReturned && bMenuUI && bNpcDialogue && bBossReached && bFirstKill && bLearned && bSword && Natural == TEXT("victory"),Now()-Started,*TerminalPhase,*ProfileId.ToString(EGuidFormats::Digits));
        Text += FString::Printf(TEXT("pending_screenshot=%s\nqueued_screenshot_count=%d\nrequested_or_queued_capture_count=%d\nreset_scope=level_BrawlerXP_selected_learned_node_specific_sword_GUID_profile_phase_only\n"),*PendingShot,ShotQueue.Num(),ShotCount);
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
    FDelegateHandle ExitHandle;
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
