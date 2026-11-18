// Opt-in query/pulse fixture; no physical-input, cost acceptance, balance or full-run claim.
#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
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
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// Narrow editor-only friendship. The fixture executes production pulse/query/hit
// code with a transient skill; it does not unlock skills or change any attributes.
struct FRogue10mSpatialDebugTestAccess
{
    static void Start(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill)
    { Combat.StartAttackHitSequence(Skill); }
    static void Cancel(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData* Skill)
    {
        TArray<uint32> OwnedExecutions;
        for (const auto& Pair : Combat.ActiveAttackExecutions)
            if (Pair.Value.SkillData.Get() == Skill) { OwnedExecutions.Add(Pair.Key); }
        for (uint32 Id : OwnedExecutions) { Combat.FinishAttackHitSequence(Id); }
    }
};

namespace Rogue10mSpatialDebugQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& InWorld)
    {
        World = &InWorld;
        const TCHAR* Args = FCommandLine::Get();
        FString Slot, Prefix, Keys;
        FParse::Value(Args, TEXT("RogueSpatialDebugLabel="), Label);
        FParse::Value(Args, TEXT("RogueProfileSaveSlot="), Slot);
        FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
        FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), Keys);
        bool bValidLabel = !Label.IsEmpty() && Label.Len() <= 24;
        for (TCHAR Ch : Label)
        {
            bValidLabel &= (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('a') && Ch <= TEXT('z'))
                || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
        }
        if (!bValidLabel || Slot != TEXT("Rogue10m_Town_QA_Spatial_") + Label
            || Prefix != TEXT("Rogue10m_Progression_QA_Spatial_") + Label || Keys != TEXT("Rogue10m_Fighter_QA_Spatial_") + Label
            || InWorld.GetNetMode() != NM_Standalone
            || UWorld::RemovePIEPrefix(InWorld.GetPackage()->GetName()) != TEXT("/Game/Rogue10m/Maps/StartHub"))
        { Reject(TEXT("exact_isolated_slots_and_StartHub_required")); return; }
        Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/combat-spatial-debug-20261103"), Label);
        Images = FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLog/20261103_SpatialDebug_") + Label);
        const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
        TArray<FString> Existing;
        IFileManager::Get().FindFiles(Existing, *(SaveDirectory / (Prefix + TEXT("_*.sav"))), true, false);
        if (IFileManager::Get().FileExists(*Output) || IFileManager::Get().DirectoryExists(*Output)
            || IFileManager::Get().FileExists(*Images) || IFileManager::Get().DirectoryExists(*Images)
            || IFileManager::Get().FileExists(*(SaveDirectory / (Slot + TEXT(".sav"))))
            || IFileManager::Get().FileExists(*(SaveDirectory / (Keys + TEXT(".sav")))) || !Existing.IsEmpty())
        { Reject(TEXT("fresh_save_and_evidence_namespace_required")); return; }
        bRendered = !FParse::Param(Args, TEXT("nullrhi"));
        if (!IFileManager::Get().MakeDirectory(*Output, true) || (bRendered && !IFileManager::Get().MakeDirectory(*Images, true)))
        { Reject(TEXT("evidence_directory_creation_failed")); return; }
        Measurements = TEXT("stage,paused,spatial,owned_lines,supported,live_skeletal,unresolved,unsupported,queries,execution,pulse,shape,locked_reuse,raw_components,eligible,accepted,reference_requested,viewport_collision,live_center_valid,live_center_x,live_center_y,live_center_z,target_health\n");
        Started = FPlatformTime::Seconds(); Deadline = Started + 80.0;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (!Self->World.IsValid() || FPlatformTime::Seconds() >= Self->Deadline)
            { Self->Check(false, TEXT("world_and_wall_watchdog_valid")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt)
            { auto Work = MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }), 0.02f);
        Later(3.0, [this] { Prepare(); });
    }
    bool Finished() const { return bFinished; }

private:
    void Reject(const TCHAR* Reason)
    {
        bFinished = true;
        UE_LOG(LogRogue10m, Error, TEXT("RESULT=SPATIAL_DEBUG_FAILED reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    void Check(bool bOK, const TCHAR* Reason)
    {
        ++Checks;
        Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("SPATIAL_DEBUG FAIL %s"), Reason); }
    }
    bool Continue()
    {
        if (Failures || !World.IsValid() || !PC.IsValid() || !Player.IsValid() || !Tools.IsValid() || !Combat.IsValid())
        {
            if (!Failures) { Check(false, TEXT("real_player_tools_and_combat_still_valid")); }
            Finish(); return false;
        }
        return true;
    }
    void Later(double Delay, TFunction<void()> Work)
    { NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work); }
    void WaitForQueries(int32 Expected, double Until, TFunction<void()> Work)
    {
        if (!Continue()) { return; }
        if (Tools->GetRecordedQueryCount() >= Expected) { Work(); return; }
        if (FPlatformTime::Seconds() >= Until)
        { Check(false, TEXT("actual_timer_pulse_arrives_within_bounded_wall_watchdog")); Finish(); return; }
        Later(0.05, [this, Expected, Until, Work = MoveTemp(Work)]() mutable
        { WaitForQueries(Expected, Until, MoveTemp(Work)); });
    }
    UGameViewportClient* Viewport() const
    { return World.IsValid() ? World->GetGameViewport() : nullptr; }
    int32 SharedDebugLines() const
    {
        const UWorld* W = World.Get();
        const auto Count = [](const ULineBatchComponent* Lines)
        { return Lines ? Lines->BatchedLines.Num() + Lines->BatchedPoints.Num() : 0; };
        if (!W) { return 0; }
        int32 Total = 0;
        for (const auto Type : {UWorld::ELineBatcherType::World, UWorld::ELineBatcherType::WorldPersistent,
            UWorld::ELineBatcherType::Foreground, UWorld::ELineBatcherType::ForegroundPersistent})
        { Total += Count(W->GetLineBatcher(Type)); }
        return Total;
    }
    float TargetHealth() const
    { return Target.IsValid() && Target->GetRogueAttributeSet() ? Target->GetRogueAttributeSet()->GetHealth() : -1.0f; }
    void RecordViewGeometry(const TCHAR* Stage)
    {
        if (!Target.IsValid() || !Player.IsValid() || !PC.IsValid()) return;
        const auto* Mesh=Target->GetMesh();
        const auto* AttackCamera=Player->GetFirstPersonCameraComponent();
        const APlayerCameraManager* RenderCamera=PC->PlayerCameraManager;
        const auto* Capsule=Target->GetCapsuleComponent();
        if (!Mesh || !AttackCamera || !RenderCamera || !Capsule) return;
        const FVector RenderLocation=RenderCamera->GetCameraLocation();
        const bool bInsideRenderBounds=Mesh->Bounds.GetBox().IsInsideOrOn(RenderLocation);
        Results+=FString::Printf(TEXT("VIEW %s actor_to_attack_camera_cm=%.3f mesh_bounds_origin=%s mesh_bounds_extent=%s render_camera=%s attack_camera=%s camera_inside_mesh_bounds_aabb=%d capsule_radius_cm=%.3f capsule_half_height_cm=%.3f mesh_visible_flag=%d actor_hidden=%d inspection=%d\n"),
            Stage,FVector::Distance(Target->GetActorLocation(),AttackCamera->GetComponentLocation()),
            *Mesh->Bounds.Origin.ToString(),*Mesh->Bounds.BoxExtent.ToString(),*RenderLocation.ToString(),*AttackCamera->GetComponentLocation().ToString(),
            bInsideRenderBounds,Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight(),Mesh->IsVisible(),Target->IsHidden(),
            Player->GetAppearanceCameraComponent() && Player->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive());
        // Mesh AABB containment is a framing diagnostic, not proof of being inside its surface or a hitbox.
    }
    void ConfigureOwnedTargetRegenerationFixture()
    {
        auto* Regen=Target.IsValid() && Target->IsCombatTestInstance()
            ? Target->FindComponentByClass<URogue10mVitalRegenerationComponent>() : nullptr;
        const FFloatProperty* Health=Regen?FindFProperty<FFloatProperty>(Regen->GetClass(),TEXT("HealthRegenerationPerSecond")):nullptr;
        const FFloatProperty* Stamina=Regen?FindFProperty<FFloatProperty>(Regen->GetClass(),TEXT("StaminaRegenerationPerSecond")):nullptr;
        const FFloatProperty* Mana=Regen?FindFProperty<FFloatProperty>(Regen->GetClass(),TEXT("ManaRegenerationPerSecond")):nullptr;
        const FFloatProperty* Interval=Regen?FindFProperty<FFloatProperty>(Regen->GetClass(),TEXT("RegenerationInterval")):nullptr;
        Check(Regen && Health && Stamina && Mana && Interval,TEXT("owned_test_target_actual_regeneration_properties_available"));
        if (Failures) return;
        Results+=FString::Printf(TEXT("owned_target_regen_before_health_per_s=%.9f\nowned_target_regen_before_stamina_per_s=%.9f\nowned_target_regen_before_mana_per_s=%.9f\nowned_target_regen_interval_s=%.9f\n"),
            Health->GetPropertyValue_InContainer(Regen),Stamina->GetPropertyValue_InContainer(Regen),
            Mana->GetPropertyValue_InContainer(Regen),Interval->GetPropertyValue_InContainer(Regen));
        const float UnchangedHealth=TargetHealth();
        Regen->ConfigureRegeneration(0.f,0.f,0.f);
        Check(Health->GetPropertyValue_InContainer(Regen)==0.f && Stamina->GetPropertyValue_InContainer(Regen)==0.f
            && Mana->GetPropertyValue_InContainer(Regen)==0.f && TargetHealth()==UnchangedHealth,
            TEXT("owned_test_target_regeneration_disabled_without_HP_write"));
        // Only this reward-disabled, fixture-owned target changes. Cleanup destroys it;
        // component EndPlay clears the timer. Never alter player, map monsters or assets.
    }
    void Sample(const TCHAR* Stage)
    {
        RecordViewGeometry(Stage);
        const auto& Q = Tools->GetLastAttackQuery();
        FVector Center = FVector::ZeroVector;
        const bool bCenter = Tools->TryGetFirstLiveSkeletalShapeCenter(Center);
        Measurements += FString::Printf(TEXT("%s,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.6f\n"),
            Stage, UGameplayStatics::IsGamePaused(World.Get()), Tools->IsSpatialDebugEnabled(), Tools->GetSpatialDebugLineCount(),
            Tools->GetSupportedColliderCount(), Tools->GetLiveSkeletalShapeCount(), Tools->GetUnresolvedColliderCount(), Tools->GetUnsupportedColliderCount(),
            Tools->GetRecordedQueryCount(), Q.ExecutionId, Q.Pulse, static_cast<int32>(Q.Shape), Q.bLockedTargetReuse,
            Q.OverlapComponents.Num(), Q.EligibleTargets.Num(), Tools->GetAcceptedDebugTargetCount(), Tools->IsCollisionReferenceEnabled(),
            Viewport() && Viewport()->EngineShowFlags.Collision, bCenter, Center.X, Center.Y, Center.Z, TargetHealth());
    }
    void PauseOwned()
    {
        Check(!UGameplayStatics::IsGamePaused(World.Get()), TEXT("QA_pause_starts_from_unpaused_world"));
        bOwnsPause = PC->SetPause(true);
        Check(bOwnsPause && UGameplayStatics::IsGamePaused(World.Get()), TEXT("QA_owned_pause_acquired"));
    }
    void ResumeOwned()
    {
        if (bOwnsPause && PC.IsValid()) { PC->SetPause(false); }
        bOwnsPause = false;
    }
    void PhotoThen(const TCHAR* Name, TFunction<void()> Work)
    {
        if (!Continue()) { return; }
        if (!UGameplayStatics::IsGamePaused(World.Get())) { PauseOwned(); }
        Sample(Name);
        const FString File = Images / Name;
        Later(0.2, [this, File, Work = MoveTemp(Work)]() mutable
        {
            if (bRendered) { FScreenshotRequest::RequestScreenshot(File, true, false); }
            Later(0.8, [this, Work = MoveTemp(Work)]() mutable { ResumeOwned(); Work(); });
        });
    }
    void Prepare()
    {
        PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        auto* Instance = World->GetGameInstance();
        auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        Check(PC.IsValid() && Profiles && Saves && Profiles->GetProfileCount() == 0 && Viewport(), TEXT("fresh_isolated_profile_and_real_local_viewport"));
        if (Failures) { Finish(); return; }
        OriginalCollisionFlag = Viewport()->EngineShowFlags.Collision; bOwnViewportFixture = true;
        FRogue10mCharacterProfile Profile;
        Check(Profiles->CreateProfile(TEXT("SpatialQa"), FRogue10mCharacterAppearance(), Profile)
            && Profiles->SelectProfile(Profile.ProfileId), TEXT("QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Player = Cast<ARogue10mCharacter>(PC->GetPawn()); Tools = PC->GetCombatTestComponent();
        Combat = Player.IsValid() ? Player->GetCombatComponent() : nullptr;
        Check(Player.IsValid() && Tools.IsValid() && Combat.IsValid() && Saves->IsAttachedTo(*Player), TEXT("normal_QA_profile_entry_attaches_actual_player"));
        if (!Continue()) { return; }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
            if (!It->IsCombatTestInstance() && It->GetRogueAttributeSet()) { Originals.Add(*It, It->GetRogueAttributeSet()->GetHealth()); }
        Check(!Tools->IsSpatialDebugEnabled() && !Tools->IsCollisionReferenceEnabled() && Tools->GetSpatialDebugLineCount() == 0,
            TEXT("ordinary_gameplay_debug_and_reference_requests_default_OFF"));
        Check(!Tools->SetSpatialDebugEnabled(true) && Tools->GetSpatialDebugLineCount() == 0,
            TEXT("spatial_debug_refuses_no_live_owned_test_monster"));
        Tools->SelectCreature(5);
        Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn), TEXT("normal_F6_action_spawns_real_test_boss"));
        Target = Tools->GetTestTarget();
        Check(Target.IsValid() && Target->IsCombatTestInstance() && !Target->IsDead(), TEXT("owned_reward_disabled_live_test_target"));
        if (!Continue() || !Target.IsValid()) { Finish(); return; }
        Target->GetCharacterMovement()->DisableMovement();
        ConfigureOwnedTargetRegenerationFixture();
        if (!Continue()) { return; }
        Later(0.8, [this]
        {
            if (!Continue()) { return; }
            Check(Tools->SetSpatialDebugEnabled(true),TEXT("wide_spawn_reference_uses_actual_live_colliders"));
            PhotoThen(TEXT("00_spawned_target_wide.png"),[this]
            {
                if (!Continue()) { return; }
                Tools->SetSpatialDebugEnabled(false);
                PlaceTarget(); OffShot();
            });
        });
    }
    void PlaceTarget()
    {
        if (auto* Appearance = Player->GetAppearanceCameraComponent()) { Appearance->UpdateCameraFromAppearance(); }
        const auto* Camera = Player->GetFirstPersonCameraComponent();
        Target->SetActorLocation(Camera->GetComponentLocation() + Camera->GetForwardVector() * 150.0, false, nullptr, ETeleportType::TeleportPhysics);
    }
    void MakeSkill(ERogue10mAttackShape Shape, ERogue10mAttackHitMode Mode = ERogue10mAttackHitMode::Single)
    {
        if (Skill.IsValid()) { FRogue10mSpatialDebugTestAccess::Cancel(*Combat.Get(), Skill.Get()); }
        Skill.Reset(NewObject<URogue10mAttackSkillData>(GetTransientPackage(), NAME_None, RF_Transient));
        Skill->SkillName = FText::FromString(TEXT("Spatial query QA fixture"));
        Skill->AttackShape = Shape; Skill->HitMode = Mode;
        Skill->HitCount = Mode == ERogue10mAttackHitMode::Single ? 1 : 2;
        Skill->HitInterval = 0.25f; Skill->HitStartDelaySeconds = 0.0f;
        Skill->AttackRange = 320.0f; Skill->AttackTraceRadius = 80.0f;
        Skill->BoxHalfWidth = 80.0f; Skill->BoxHalfHeight = 100.0f;
        Skill->CircleForwardOffset = 150.0f; Skill->ArcAngleDegrees = 60.0f;
        Skill->Damage = 1.0f; Skill->MinDamageRatioMultiplier = Skill->MaxDamageRatioMultiplier = 0.001f;
        Skill->CriticalChanceBonus = -1.0f; Skill->bEnableAttackEffects = false;
        Skill->bDrawDebugAttack = true; // Regression guard: asset flag alone must not draw during ordinary play.
    }
    void OffShot()
    {
        if (!Continue()) { return; }
        MakeSkill(ERogue10mAttackShape::LinearBox);
        const int32 BeforeLines = SharedDebugLines();
        const float OffShotHealth = TargetHealth();
        FRogue10mSpatialDebugTestAccess::Start(*Combat.Get(), *Skill.Get());
        Check(TargetHealth() < OffShotHealth, TEXT("OFF_fixture_executes_actual_query_and_applied_damage"));
        Check(Tools->GetRecordedQueryCount() == 0 && Tools->GetSpatialDebugLineCount() == 0 && SharedDebugLines() == BeforeLines,
            TEXT("asset_debug_true_cannot_emit_or_record_ordinary_play_debug"));
        Check(Tools->SetSpatialDebugEnabled(true), TEXT("explicit_spatial_enable_with_live_owned_target"));
        Later(0.2, [this]
        {
            if (!Continue()) { return; }
            Check(Tools->GetSupportedColliderCount() > 0 && Tools->GetSpatialDebugLineCount() > 0, TEXT("actual_query_enabled_supported_colliders_draw_in_owned_buffer"));
            LiveShapeFixture();
        });
    }
    void RestoreOwnedMeshFreeze()
    {
        if (bOwnMeshFreeze)
        {
            if (auto* Mesh=FrozenMesh.Get())
            {
                Mesh->bPauseAnims=bSavedMeshPauseAnims;
                Mesh->SetComponentTickEnabled(bSavedMeshTickEnabled);
            }
        }
        bOwnMeshFreeze=false; FrozenMesh.Reset();
    }
    void RecordSceneShapeState(const TCHAR* Stage,USkeletalMeshComponent& Mesh,const FVector& ShapeCenter)
    {
        FTransform BodyPose=FTransform::Identity;
        int32 BodyIndex=INDEX_NONE;
        for (int32 Index=0;Index<Mesh.Bodies.Num();++Index)
        {
            auto* Body=Mesh.Bodies[Index];
            if (!Body) continue;
            const auto& Actor=Body->GetActorReferenceWithWelding();
            if (!FPhysicsInterface::IsValid(Actor)) continue;
            if (FPhysicsCommand::ExecuteRead(Actor,[&](const FPhysicsActorHandle& LockedActor)
                { BodyPose=FPhysicsInterface::GetGlobalPose_AssumesLocked(LockedActor); }))
            { BodyIndex=Index; break; }
        }
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic); Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SpatialPoseFixtureSceneProbe),false,Player.Get());
        TArray<FOverlapResult> Overlaps;
        World->OverlapMultiByObjectType(Overlaps,ShapeCenter,FQuat::Identity,Objects,FCollisionShape::MakeSphere(1.f),Query);
        const bool bMeshOverlap=Overlaps.ContainsByPredicate([&Mesh](const FOverlapResult& Hit){return Hit.GetComponent()==&Mesh;});
        Results+=FString::Printf(TEXT("SCENE_POSE %s frame=%llu paused=%d actor=%s mesh=%s shape_center=%s first_body_index=%d first_body_pose=%s native_probe_components=%d native_probe_hits_owned_mesh=%d parallel_running=%d pose_bones=%d\n"),
            Stage,static_cast<uint64>(GFrameCounter),UGameplayStatics::IsGamePaused(World.Get()),
            *Target->GetActorTransform().ToHumanReadableString(),*Mesh.GetComponentTransform().ToHumanReadableString(),*ShapeCenter.ToString(),
            BodyIndex,*BodyPose.ToHumanReadableString(),Overlaps.Num(),bMeshOverlap,Mesh.IsRunningParallelEvaluation(),Mesh.GetNumComponentSpaceTransforms());
        UE_LOG(LogRogue10m,Display,TEXT("SPATIAL_SCENE_POSE stage=%s actor=%s mesh=%s shape=%s first_body=%d body=%s native_owned_mesh=%d parallel=%d"),
            Stage,*Target->GetActorLocation().ToString(),*Mesh.GetComponentLocation().ToString(),*ShapeCenter.ToString(),BodyIndex,*BodyPose.GetLocation().ToString(),bMeshOverlap,Mesh.IsRunningParallelEvaluation());
        Check(BodyIndex!=INDEX_NONE && bMeshOverlap,TEXT("sampled_live_skeletal_center_agrees_with_independent_native_scene_overlap"));
    }
    void LiveShapeFixture()
    {
        if (!Continue()) { return; }
        PauseOwned();
        auto* Mesh=Target.IsValid()?Target->GetMesh():nullptr;
        Check(Mesh && Target->IsCombatTestInstance() && !Mesh->IsAnySimulatingPhysics(),
            TEXT("pose_fixture_owns_a_live_kinematic_test_mesh"));
        if (!Mesh || Failures) { Finish(); return; }
        FrozenMesh=Mesh; bSavedMeshPauseAnims=Mesh->bPauseAnims; bSavedMeshTickEnabled=Mesh->IsComponentTickEnabled(); bOwnMeshFreeze=true;
        const bool bWasParallel=Mesh->IsRunningParallelEvaluation();
        // World pause does not drain a previously dispatched animation evaluation.
        // Freeze only this owned test mesh, complete its pending task, then explicitly
        // synchronize the resulting current pose into live physics without deferral.
        Mesh->bPauseAnims=true; Mesh->SetComponentTickEnabled(false);
        const bool bDrained=Mesh->HandleExistingParallelEvaluationTask(true,true);
        const TArray<FTransform> FrozenPose=Mesh->GetComponentSpaceTransforms();
        Check(!Mesh->IsRunningParallelEvaluation() && !FrozenPose.IsEmpty(),TEXT("owned_mesh_parallel_pose_completed_before_scene_baseline"));
        Results+=FString::Printf(TEXT("owned_mesh_pose_fixture_before_parallel=%d task_drained=%d bones=%d original_pause_anims=%d original_tick=%d sync=UpdateKinematicBonesToAnim_TeleportPhysics_DisallowDeferral\n"),
            bWasParallel,bDrained,FrozenPose.Num(),bSavedMeshPauseAnims,bSavedMeshTickEnabled);
        if (Failures) { RestoreOwnedMeshFreeze(); Finish(); return; }
        const auto SyncFrozenPose=[&]
        {
            Mesh->UpdateKinematicBonesToAnim(FrozenPose,ETeleportType::TeleportPhysics,false,EAllowKinematicDeferral::DisallowDeferral);
            Tools->SetSpatialDebugEnabled(true); // Fresh live-physics capture, never a stale TimerManager sample.
        };
        SyncFrozenPose();
        FVector Before=FVector::ZeroVector,After=FVector::ZeroVector,Restored=FVector::ZeroVector;
        if (Tools->GetLiveSkeletalShapeCount()>0 && Tools->TryGetFirstLiveSkeletalShapeCenter(Before))
        {
            const FTransform ActorBefore=Target->GetActorTransform();
            const FTransform MeshBefore=Mesh->GetComponentTransform();
            RecordSceneShapeState(TEXT("baseline_synced"),*Mesh,Before);
            const FVector Delta(0,40,0);
            Target->SetActorLocation(ActorBefore.GetLocation()+Delta,false,nullptr,ETeleportType::TeleportPhysics);
            SyncFrozenPose();
            bLiveSkeletalExercised=Tools->TryGetFirstLiveSkeletalShapeCenter(After);
            RecordSceneShapeState(TEXT("translated_synced"),*Mesh,After);
            Check((Target->GetActorLocation()-ActorBefore.GetLocation()).Equals(Delta,0.01)
                && (Mesh->GetComponentLocation()-MeshBefore.GetLocation()).Equals(Delta,0.01),
                TEXT("owned_actor_and_mesh_move_exactly_the_requested_translation"));
            bool bPoseUnchanged=FrozenPose.Num()==Mesh->GetComponentSpaceTransforms().Num();
            if (bPoseUnchanged)
                for (int32 Index=0;Index<FrozenPose.Num();++Index)
                    if (!FrozenPose[Index].Equals(Mesh->GetComponentSpaceTransforms()[Index],0.0001f)) { bPoseUnchanged=false; break; }
            Check(bPoseUnchanged && !Mesh->IsRunningParallelEvaluation(),TEXT("paused_owned_mesh_component_pose_stays_identical_during_translation"));
            Results+=FString::Printf(TEXT("LIVE_SHAPE_TRANSLATION before=%s after=%s measured_delta=%s expected_delta=%s delta_error_cm=%.9f\n"),
                *Before.ToString(),*After.ToString(),*(After-Before).ToString(),*Delta.ToString(),FVector::Distance(After-Before,Delta));
            Check(bLiveSkeletalExercised && (After-Before).Equals(Delta,1.0),TEXT("live_skeletal_scene_shape_follows_owned_actor_transform_while_pose_paused"));
            Target->SetActorTransform(ActorBefore,false,nullptr,ETeleportType::TeleportPhysics);
            SyncFrozenPose();
            const bool bRestored=Tools->TryGetFirstLiveSkeletalShapeCenter(Restored);
            RecordSceneShapeState(TEXT("restored_synced"),*Mesh,Restored);
            Check(bRestored && Restored.Equals(Before,0.1),TEXT("owned_scene_shape_restores_to_original_physics_location"));
        }
        else { Results+=TEXT("UNEXERCISED live_skeletal_scene_shape_absent_on_selected_asset_reported_separately\n"); }
        RestoreOwnedMeshFreeze();
        Check(Mesh->bPauseAnims==bSavedMeshPauseAnims && Mesh->IsComponentTickEnabled()==bSavedMeshTickEnabled,
            TEXT("owned_mesh_pause_and_tick_flags_restored"));
        PhotoThen(TEXT("01_live_colliders.png"),[this]{RunSingle(ERogue10mAttackShape::LinearBox);});
    }
    void VerifyCaptured(ERogue10mAttackShape Shape, bool bLocked)
    {
        const auto& Q = Tools->GetLastAttackQuery();
        Check(Q.ExecutionId > 0 && Q.Pulse > 0 && Q.bLockedTargetReuse == bLocked, TEXT("snapshot_is_actual_executed_pulse_and_lock_kind"));
        Check(Q.EligibleTargets.ContainsByPredicate([this](const auto& Weak) { return Weak.Get() == Target.Get(); })
            && Tools->GetAcceptedDebugTargetCount() > 0, TEXT("owned_target_eligible_and_green_only_after_actual_applied_damage"));
        if (bLocked)
        {
            Check(Q.OverlapComponents.IsEmpty() && Q.Pulse == 2, TEXT("later_MultiHit_snapshot_has_no_new_raw_overlap_geometry")); return;
        }
        Check(Q.Shape == Shape && Q.OverlapComponents.ContainsByPredicate([this](const auto& Weak)
            { return Weak.IsValid() && Weak->GetOwner() == Target.Get(); }), TEXT("snapshot_keeps_real_overlapped_hit_component"));
        if (Shape == ERogue10mAttackShape::LinearBox)
        {
            Check(Q.Center.Equals(Q.Origin + Q.Forward * 160.0, 0.1) && Q.Extent.Equals(FVector(160, 80, 100), 0.1), TEXT("linear_capture_matches_actual_oriented_query_box"));
        }
        else if (Shape == ERogue10mAttackShape::Circle)
        {
            Check(Q.Center.Equals(Q.Origin + Q.Forward * 150.0, 0.1) && FMath::IsNearlyEqual(Q.Radius, 320.0f, 0.1f), TEXT("circle_capture_uses_actual_sphere_and_forward_offset"));
        }
        else if (Shape == ERogue10mAttackShape::Arc)
        {
            Check(Q.Center.Equals(Q.Origin, 0.1) && FMath::IsNearlyEqual(Q.Radius, 320.0f, 0.1f)
                && FMath::IsNearlyEqual(Q.ArcAngleDegrees, 60.0f, 0.1f), TEXT("arc_capture_distinguishes_sphere_query_from_actor_angle_filter"));
        }
        else
        {
            Check(Q.Pulse == 2 && Q.Center.Equals(Q.Origin + Q.Forward * 240.0, 0.1) && Q.Extent.Equals(FVector(80), 0.1),
                TEXT("projectile_second_pulse_is_actual_incremental_segment_not_whole_range"));
        }
    }
    void RunSingle(ERogue10mAttackShape Shape)
    {
        if (!Continue()) { return; }
        PlaceTarget(); MakeSkill(Shape);
        const int32 SingleQueries = Tools->GetRecordedQueryCount(); const float SingleHealth = TargetHealth();
        FRogue10mSpatialDebugTestAccess::Start(*Combat.Get(), *Skill.Get());
        Check(Tools->GetRecordedQueryCount() == SingleQueries + 1 && TargetHealth() < SingleHealth, TEXT("real_single_pulse_records_query_and_applied_damage_once"));
        VerifyCaptured(Shape, false);
        const TCHAR* Name = Shape == ERogue10mAttackShape::LinearBox ? TEXT("02_linear_query_hit.png")
            : Shape == ERogue10mAttackShape::Circle ? TEXT("03_circle_query_hit.png") : TEXT("04_arc_sphere_query_and_filter.png");
        PhotoThen(Name, [this, Shape]
        {
            if (Shape == ERogue10mAttackShape::LinearBox) { InspectionReference(); }
            else if (Shape == ERogue10mAttackShape::Circle) { RunSingle(ERogue10mAttackShape::Arc); }
            else { Projectile(); }
        });
    }
    void InspectionReference()
    {
        if (!Continue()) { return; }
        const auto* Appearance=Player->GetAppearanceCameraComponent();
        Check(Appearance && !Appearance->IsThirdPersonInspectionActive() && !UGameplayStatics::IsGamePaused(World.Get())
            && !PC->IsAnyBlockingWindowVisible() && !PC->IsLookInputIgnored() && !PC->IsMoveInputIgnored(),
            TEXT("actual_inspection_reference_starts_from_normal_unpaused_first_person"));
        if (!Continue()) return;
        InspectionControl=PC->GetControlRotation();
        InspectionQueryCount=Tools->GetRecordedQueryCount();
        PC->ToggleInspectionCamera();
        Check(Appearance->IsThirdPersonInspectionActive(),TEXT("actual_controller_inspection_reference_enabled"));
        Later(0.2,[this]
        {
            PhotoThen(TEXT("02b_actual_query_inspection_reference.png"),[this]
            {
                if (!Continue()) return;
                PC->ToggleInspectionCamera();
                Check(!Player->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()
                    && PC->GetControlRotation().Equals(InspectionControl,0.01f)
                    && Tools->GetRecordedQueryCount()==InspectionQueryCount,
                    TEXT("inspection_reference_restores_first_person_aim_without_new_query"));
                if (!Continue()) return;
                RunSingle(ERogue10mAttackShape::Circle);
            });
        });
    }
    void Projectile()
    {
        if (!Continue()) { return; }
        PlaceTarget(); MakeSkill(ERogue10mAttackShape::Projectile, ERogue10mAttackHitMode::Continuous);
        BeforeQueries = Tools->GetRecordedQueryCount(); BeforeHealth = TargetHealth();
        FRogue10mSpatialDebugTestAccess::Start(*Combat.Get(), *Skill.Get());
        WaitForQueries(BeforeQueries + 2, FPlatformTime::Seconds() + 6.0, [this]
        {
            if (!Continue()) { return; }
            Check(Tools->GetRecordedQueryCount() == BeforeQueries + 2 && TargetHealth() < BeforeHealth, TEXT("projectile_two_actual_timer_pulses_recorded"));
            VerifyCaptured(ERogue10mAttackShape::Projectile, false);
            PhotoThen(TEXT("05_projectile_segment.png"), [this] { LockedReuse(); });
        });
    }
    void LockedReuse()
    {
        if (!Continue()) { return; }
        PlaceTarget(); MakeSkill(ERogue10mAttackShape::LinearBox, ERogue10mAttackHitMode::MultiHit);
        BeforeQueries = Tools->GetRecordedQueryCount();
        FRogue10mSpatialDebugTestAccess::Start(*Combat.Get(), *Skill.Get()); VerifyCaptured(ERogue10mAttackShape::LinearBox, false);
        BeforeHealth = TargetHealth(); Target->SetActorLocation(Target->GetActorLocation() + FVector(2000, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
        WaitForQueries(BeforeQueries + 2, FPlatformTime::Seconds() + 6.0, [this]
        {
            if (!Continue()) { return; }
            const int32 ActualQueries=Tools->GetRecordedQueryCount();
            const float ActualHealth=TargetHealth();
            const auto& Captured=Tools->GetLastAttackQuery();
            Results+=FString::Printf(TEXT("MULTIHIT expected_records=%d actual_records=%d health_before_second=%.9f health_after_second=%.9f execution=%u pulse=%d locked=%d accepted=%d\n"),
                BeforeQueries+2,ActualQueries,BeforeHealth,ActualHealth,Captured.ExecutionId,Captured.Pulse,Captured.bLockedTargetReuse,Tools->GetAcceptedDebugTargetCount());
            UE_LOG(LogRogue10m,Display,TEXT("SPATIAL_MULTIHIT expected_records=%d actual_records=%d before_health=%.9f after_health=%.9f pulse=%d locked=%d accepted=%d"),
                BeforeQueries+2,ActualQueries,BeforeHealth,ActualHealth,Captured.Pulse,Captured.bLockedTargetReuse,Tools->GetAcceptedDebugTargetCount());
            Check(ActualQueries == BeforeQueries + 2, TEXT("actual_MultiHit_exactly_two_pulse_records"));
            Check(ActualHealth < BeforeHealth, TEXT("actual_MultiHit_second_locked_pulse_decreases_HP_with_owned_regeneration_disabled"));
            VerifyCaptured(ERogue10mAttackShape::LinearBox, true);
            PhotoThen(TEXT("06_locked_reuse_no_new_query.png"), [this] { Reference(); });
        });
    }
    void Reference()
    {
        if (!Continue()) { return; }
        PlaceTarget(); Tools->SetSpatialDebugEnabled(true);
        const int32 Queries = Tools->GetRecordedQueryCount(); const uint32 Execution = Tools->GetLastAttackQuery().ExecutionId;
        const auto* Capsule = Target->GetCapsuleComponent();
        const ECollisionEnabled::Type Enabled = Capsule->GetCollisionEnabled(); const ECollisionChannel Object = Capsule->GetCollisionObjectType();
        Check(Tools->SetCollisionReferenceEnabled(true) && Tools->IsCollisionReferenceEnabled() && Viewport()->EngineShowFlags.Collision,
            TEXT("authored_reference_sets_only_this_local_viewport_flag"));
        Check(Tools->GetRecordedQueryCount() == Queries && Tools->GetLastAttackQuery().ExecutionId == Execution
            && Capsule->GetCollisionEnabled() == Enabled && Capsule->GetCollisionObjectType() == Object,
            TEXT("reference_toggle_does_not_execute_query_or_mutate_physical_collision"));
        PhotoThen(TEXT("07_authored_reference_on.png"), [this]
        {
            if (!Continue()) { return; }
            Check(Tools->SetCollisionReferenceEnabled(false) && !Tools->IsCollisionReferenceEnabled()
                && static_cast<bool>(Viewport()->EngineShowFlags.Collision) == OriginalCollisionFlag,
                TEXT("reference_OFF_restores_preexisting_viewport_flag"));
            // Separate explicit viewport fixture: an already-enabled flag must survive OFF.
            Viewport()->EngineShowFlags.SetCollision(true);
            Tools->SetCollisionReferenceEnabled(true); Tools->SetCollisionReferenceEnabled(false);
            Check(Viewport()->EngineShowFlags.Collision, TEXT("reference_preserves_an_already_enabled_local_viewport_flag"));
            Viewport()->EngineShowFlags.SetCollision(OriginalCollisionFlag);
            Tools->SetCollisionReferenceEnabled(true);
            DisablePaused();
        });
    }
    void DisablePaused()
    {
        if (!Continue()) { return; }
        PauseOwned();
        Check(Tools->SetSpatialDebugEnabled(false) && !Tools->IsSpatialDebugEnabled() && Tools->GetSpatialDebugLineCount() == 0
            && Tools->GetRecordedQueryCount() == 0 && !Tools->IsCollisionReferenceEnabled()
            && static_cast<bool>(Viewport()->EngineShowFlags.Collision) == OriginalCollisionFlag,
            TEXT("OFF_while_paused_immediately_flushes_owned_lines_snapshots_and_reference"));
        PhotoThen(TEXT("08_debug_disabled_paused.png"), [this]
        {
            if (!Continue()) { return; }
            Check(Tools->SetSpatialDebugEnabled(true), TEXT("explicit_debug_can_reenable_for_live_owned_target"));
            PauseOwned();
            Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Clear) && Tools->GetTestMonsterCount() == 0
                && Tools->GetSpatialDebugLineCount() == 0 && !Tools->IsSpatialDebugEnabled(),
                TEXT("clear_while_paused_deletes_only_owned_targets_and_immediately_flushes_debug"));
            PhotoThen(TEXT("09_all_test_targets_cleared.png"), [this] { Finish(); });
        });
    }
    void Finish()
    {
        if (bFinished) { return; }
        bFinished = true;
        if (Combat.IsValid() && Skill.IsValid()) { FRogue10mSpatialDebugTestAccess::Cancel(*Combat.Get(), Skill.Get()); }
        RestoreOwnedMeshFreeze();
        if (Tools.IsValid()) { Tools->SetSpatialDebugEnabled(false); Tools->SetCollisionReferenceEnabled(false); Tools->ExecuteAction(ERogue10mCombatTestAction::Clear); }
        ResumeOwned();
        if (PC.IsValid() && Player.IsValid() && Player->GetAppearanceCameraComponent()
            && Player->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()) PC->ToggleInspectionCamera();
        if (bOwnViewportFixture && Viewport()) { Viewport()->EngineShowFlags.SetCollision(OriginalCollisionFlag); }
        for (const auto& Pair : Originals)
        {
            Check(Pair.Key.IsValid() && Pair.Key->GetRogueAttributeSet()
                && FMath::IsNearlyEqual(Pair.Key->GetRogueAttributeSet()->GetHealth(), Pair.Value, 0.0001f),
                TEXT("original_map_monster_not_destroyed_or_damaged_by_query_fixture"));
        }
        Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nrendered=%d\nlive_skeletal_exercised=%d\nwall_seconds=%.3f\nfixture=isolated_StartHub_profile_transient_lowdamage_skill_and_owned_F6_target_position\nentrypoint=real_StartAttackHitSequence_ExecuteAttackHitPulse_GatherAttackTargets_ApplyAttackDamage\ninput_and_cost_acceptance=not_tested\nstat_and_HP_seeding=none\nowned_target_regeneration_fixture=all_rates_zero_no_HP_write\nowned_mesh_pose_fixture=temporary_pause_tick_off_parallel_completion_and_immediate_live_kinematic_sync\nwide_reference_fixture=actual_F6_spawn_and_actual_inspection_no_camera_setting_override\nrewards=disabled_by_F6_test_instance\nphysical_input=not_performed\nvisual_pixels=external_review_required\nNOTE=Owned_target_movement_is_disabled_and_target_is_placed_at_observed_attack_camera_forward150cm._MultiHit_target_moves_outside_range_after_first_pulse._No_asset_or_production_save_edits._Authored_viewport_reference_is_not_actual_filtered_scene_query_geometry.\n"),
            Checks, Failures, bRendered, bLiveSkeletalExercised, Started > 0 ? FPlatformTime::Seconds() - Started : 0.0);
        if (!Output.IsEmpty())
        {
            if (!FFileHelper::SaveStringToFile(Measurements, *(Output / TEXT("measurements.csv")))) { ++Failures; }
            if (!FFileHelper::SaveStringToFile(Results, *(Output / TEXT("results.txt")))) { ++Failures; }
        }
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=SPATIAL_DEBUG_%s checks=%d failures=%d live_skeletal_exercised=%d output=%s"),
            Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, bLiveSkeletalExercised, *Output);
        FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Player;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
    TMap<TWeakObjectPtr<ARogue10mBasicMonster>, float> Originals;
    TStrongObjectPtr<URogue10mAttackSkillData> Skill;
    TWeakObjectPtr<USkeletalMeshComponent> FrozenMesh;
    bool bOwnMeshFreeze=false,bSavedMeshPauseAnims=false,bSavedMeshTickEnabled=false;
    URogue10mProgressionSubsystem* Saves = nullptr;
    TFunction<void()> Next;
    FString Label, Output, Images, Results, Measurements;
    double Started = 0.0, Deadline = 0.0, NextAt = 0.0;
    FRotator InspectionControl=FRotator::ZeroRotator;
    int32 InspectionQueryCount=0;
    float BeforeHealth = 0.0f;
    int32 Checks = 0, Failures = 0, BeforeQueries = 0;
    bool bFinished = false, bRendered = false, bOwnsPause = false, bLiveSkeletalExercised = false;
    bool OriginalCollisionFlag = false, bOwnViewportFixture = false;
};

static TSharedPtr<FRun> Active;
static void RunCommand()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
        { Active = MakeShared<FRun>(); Active->Start(*World); return; }
    }
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=SPATIAL_DEBUG_FAILED reason=no_game_world"));
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestSpatialDebug"),
    TEXT("Fresh isolated query/pulse fixture: actual shape capture, accepted hits, locked reuse, live collider transform and paused cleanup."),
    FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
