// Task-only, uncompiled observation helper. Inert until an isolated QA host calls it.
// No input, world ticking, fixture creation, save, movement, vital, montage or timing mutation.
#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include <cfloat>
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCharacterMotionDataAsset.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "UObject/UnrealType.h"

namespace Rogue10mDodgeDistanceMeasure
{
enum class EHook : uint8 { WorldPreActorTick, WorldPostActorTick, MovementUpdated };
enum class ECase : uint8 { FreeFlat, WallFlat, UnclassifiedLiveWorld };
enum class EContext : uint8 { SyntheticFixture, BasicRunReadOnly };
struct FConfig
{
    FString Label;
    ECase Case = ECase::FreeFlat;
    int32 RequestedHz = 60; // 0 is permitted only for BasicRun: observe actual DT without controlling time.
    EContext Context = EContext::SyntheticFixture;
    bool bClearanceQueries = false;
    bool bMovementUpdatedCallbackBound = false;
    bool bActualHitCallbackBound = false;
    // Only the already-validated BasicRun host supplies these. Never discover arbitrary production profiles.
    bool bBasicRunFreshGatePassed = false;
    TWeakObjectPtr<UGameInstance> BasicRunInstance;
    FGuid BasicRunProfile;
    FString BasicRunDriverOutput;
    TWeakObjectPtr<AActor> TransientFloor;
    TWeakObjectPtr<AActor> TransientWall;
};

/** Pure observer state; the host owns its lifetime, isolation, callbacks, input and output. */
class FProbe
{
public:
    bool Begin(ARogue10mCharacter& InCharacter, const FConfig& InConfig, FString& Error)
    {
        if (bStarted) { Error = TEXT("probe_already_started"); return false; }
        bool LabelValid = !InConfig.Label.IsEmpty() && InConfig.Label.Len() <= 24;
        for (const TCHAR C : InConfig.Label) { LabelValid &= (C < 128 && FChar::IsAlnum(C)) || C == TEXT('_'); }
        UWorld* W = InCharacter.GetWorld();
        const UCharacterMovementComponent* M = InCharacter.GetCharacterMovement();
        const auto* A = InCharacter.GetRogueAttributeSet();
        const auto* Combat = InCharacter.GetCombatComponent();
        const auto* Skill = Combat ? Combat->GetActiveDodgeSkill() : nullptr;
        const auto* Saves = W && W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        const bool Normal = InConfig.Context == EContext::BasicRunReadOnly;
        if (Normal && !ValidBasicRunHost(InConfig, W, Saves, InCharacter))
        { Error = TEXT("requires_explicit_opt_in_exact_fresh_BasicRun_host_instance_profile_and_output"); return false; }
        if (!LabelValid || (Normal ? InConfig.RequestedHz != 0
            : (InConfig.RequestedHz != 30 && InConfig.RequestedHz != 60 && InConfig.RequestedHz != 120))
            || !W || GIsEditor || W->WorldType != EWorldType::Game || W->GetNetMode() != NM_Standalone
            || !InCharacter.HasAuthority() || InCharacter.IsDead() || !InCharacter.CanBeDamaged()
            || !M || !M->IsMovingOnGround() || !A || A->GetHealth() <= 0.f || !Skill || !Saves
            || (!Normal && Saves->IsAttachedTo(InCharacter)) || InCharacter.IsDodging() || InCharacter.GetDodgeCooldownRemaining() > .001f
            || (!Normal && (!InConfig.TransientFloor.IsValid() || InConfig.TransientFloor->GetWorld() != W
                || !InConfig.TransientFloor->HasAnyFlags(RF_Transient)
                || M->CurrentFloor.HitResult.GetActor() != InConfig.TransientFloor.Get()
                || !M->CurrentFloor.IsWalkableFloor() || M->CurrentFloor.HitResult.ImpactNormal.Z < .9999)))
        { Error = TEXT("probe_context_or_live_damageable_grounded_pawn_preconditions_unavailable"); return false; }
        if (Normal && (InConfig.Case != ECase::UnclassifiedLiveWorld || InConfig.bClearanceQueries
            || InConfig.bMovementUpdatedCallbackBound || InConfig.bActualHitCallbackBound))
        { Error = TEXT("minimal_BasicRun_context_uses_world_hooks_only_no_extra_queries_or_dynamic_callbacks"); return false; }
        if (InConfig.Case == ECase::WallFlat && (!InConfig.TransientWall.IsValid()
            || InConfig.TransientWall->GetWorld() != W || !InConfig.TransientWall->HasAnyFlags(RF_Transient)))
        { Error = TEXT("wall_case_requires_exact_owned_transient_wall"); return false; }
        const auto* Traits = InCharacter.GetTraitRuntimeComponent();
        EffectiveCost = Traits ? Traits->GetDodgeStaminaCost(Skill->StaminaCost) : Skill->StaminaCost;
        if (!FMath::IsFinite(Skill->Distance) || Skill->Distance <= 0.f || !FMath::IsFinite(Skill->Duration)
            || Skill->Duration < .05f || !FMath::IsFinite(Skill->Cooldown) || Skill->Cooldown < 0.f
            || !FMath::IsFinite(EffectiveCost) || EffectiveCost < 0.f || A->GetStamina() < EffectiveCost)
        { Error = TEXT("actual_dodge_profile_or_naturally_available_stamina_invalid"); return false; }
        Config = InConfig; Character = &InCharacter; World = W; Dodge = Skill;
        NominalDistance = Skill->Distance; NominalDuration = Skill->Duration; NominalCooldown = Skill->Cooldown;
        ProfileBefore = Editable(*Skill); BeforeStamina = A->GetStamina(); StartedWall = FPlatformTime::Seconds();
        Rows.Add(Header()); bStarted = true;
        return true;
    }

    // Mark immediately before the host's ordinary InputKey request. This does not send input.
    bool MarkRequest(const FVector& ExpectedWorldDirection, FString& Error)
    {
        if (!Alive() || bRequested || bFinished || ExpectedWorldDirection.ContainsNaN()
            || ExpectedWorldDirection.GetSafeNormal2D().IsNearlyZero())
        { Error = TEXT("request_requires_active_probe_unique_request_and_finite_direction"); return false; }
        const auto* CurrentCombat = Character->GetCombatComponent();
        if (!CurrentCombat || CurrentCombat->GetActiveDodgeSkill() != Dodge.Get())
        { Error = TEXT("profile_changed_after_prearm_observation_skipped_no_game_input_block"); return false; }
        const auto* RequestAttributes = Character->GetRogueAttributeSet();
        if (!RequestAttributes || !FMath::IsFinite(RequestAttributes->GetStamina()))
        { Error = TEXT("request_stamina_unavailable_observation_skipped_no_game_input_block"); return false; }
        RequestStamina = RequestAttributes->GetStamina();
        Direction = ExpectedWorldDirection.GetSafeNormal2D(); RequestPosition = Character->GetActorLocation();
        RequestWorld = World->GetTimeSeconds(); RequestWall = FPlatformTime::Seconds(); RequestFrame = GFrameCounter;
        bRequested = true; LastPostPosition = RequestPosition;
        if (Config.bClearanceQueries) { Query(TEXT("request_clearance"), NominalDistance + Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 25.f); }
        return true;
    }

    /** Call from the named world hook, only for this pawn's world. Delta is the actual hook delta. */
    void Sample(EHook Hook, float Delta, const FVector* OldLocation = nullptr, const FVector* OldVelocity = nullptr)
    {
        if (!Alive() || bFinished || !bRequested) { return; }
        if (!FMath::IsFinite(Delta) || Delta <= 0.f || Rows.Num() >= 2048)
        { bInvalid = true; Finish(TEXT("invalid_delta_or_bounded_row_limit")); return; }
        const auto* C = Character.Get(); const auto* M = C->GetCharacterMovement(); const auto* A = C->GetRogueAttributeSet();
        if (!M || !A || !Dodge.IsValid()) { bInvalid = true; Finish(TEXT("required_component_lost")); return; }
        const FVector P = C->GetActorLocation();
        if (P.ContainsNaN() || C->GetVelocity().ContainsNaN()) { bInvalid = true; Finish(TEXT("nonfinite_movement")); return; }
        const bool Active = C->IsDodging();
        if (Hook == EHook::WorldPostActorTick)
        {
            MinDT = FMath::Min(MinDT, static_cast<double>(Delta)); MaxDT = FMath::Max(MaxDT, static_cast<double>(Delta));
            ++PostSamples; DTTotal += Delta;
            if (Config.RequestedHz > 0 && FMath::Abs(Delta - 1. / Config.RequestedHz) > 0.00001) { ++DTMismatches; }
            if (!bEndObserved)
            {
                SampledXYPath += FVector::Dist2D(P, LastPostPosition); LastPostPosition = P;
                if (Active && !bPreviousActive)
                {
                    ++Activations;
                    if (FirstActiveWorld < 0.) { FirstActiveWorld = World->GetTimeSeconds(); FirstActiveFrame = GFrameCounter; FirstActiveDT = Delta; }
                }
                if (!Active && bPreviousActive)
                {
                    bEndObserved = true; EndWorld = World->GetTimeSeconds(); EndFrame = GFrameCounter; EndDT = Delta;
                    EndPosition = P; EndStamina = A->GetStamina(); if (Config.bClearanceQueries) { Query(TEXT("post_end_near_wall"), 5.f); }
                }
                bPreviousActive = Active;
            }
            if (Config.Context == EContext::SyntheticFixture && (M->CurrentFloor.HitResult.GetActor() != Config.TransientFloor.Get()
                || !M->CurrentFloor.IsWalkableFloor() || M->CurrentFloor.HitResult.ImpactNormal.Z < .9999)) { bInvalidFloor = true; }
        }
        if (Hook == EHook::MovementUpdated)
        {
            // Only an actual CharacterMovementUpdated callback may provide these two arguments.
            if (!Config.bMovementUpdatedCallbackBound || !OldLocation || !OldVelocity || OldLocation->ContainsNaN() || OldVelocity->ContainsNaN()) { bInvalid = true; }
            else if (!bEndObserved) { MovementCallbackXYPath += FVector::Dist2D(P, *OldLocation); ++MovementCallbacks; }
        }
        TArray<FString> R;
        Text(R, Hook == EHook::WorldPreActorTick ? TEXT("world_pre_actor") : Hook == EHook::WorldPostActorTick ? TEXT("world_post_actor") : TEXT("movement_post_delegate"));
        Num(R, GFrameCounter); Num(R, World->GetTimeSeconds()); Num(R, FPlatformTime::Seconds() - StartedWall); Num(R, Delta);
        Num(R, Config.RequestedHz); Num(R, FApp::UseFixedTimeStep()); Num(R, FApp::GetFixedDeltaTime()); Num(R, Active);
        Vector(R, P); Vector(R, C->GetVelocity()); Vector(R, M->GetCurrentAcceleration()); Vector(R, C->GetLastMovementInputVector());
        Vector(R, OldLocation ? *OldLocation : FVector(-999999.)); Vector(R, OldVelocity ? *OldVelocity : FVector(-999999.));
        Num(R, M->MovementMode); Num(R, M->GetMaxSpeed()); Num(R, M->MaxWalkSpeed);
        Num(R, M->GroundFriction); Num(R, M->BrakingDecelerationWalking); Num(R, M->BrakingFrictionFactor);
        Num(R, M->bUseSeparateBrakingFriction); Num(R, M->BrakingFriction); Num(R, C->GetDodgeCooldownRemaining()); Num(R, A->GetStamina());
        Text(R, TEXT("dodge_end_timer_handle_not_public_unmeasured"));
        Num(R, M->HasRootMotionSources()); Num(R, M->CurrentRootMotion.HasOverrideVelocity()); Num(R, M->CurrentRootMotion.HasAdditiveVelocity());
        Text(R, TEXT("outside_PerformMovement_anim_root_params_not_certified"));
        Mesh(R, C->GetMesh()); Mesh(R, C->GetAnimationPlaybackMesh()); Mesh(R, C->GetFirstPersonMesh());
        Text(R, GetPathNameSafe(M->CurrentFloor.HitResult.GetActor())); Num(R, M->CurrentFloor.HitResult.ImpactNormal.Z);
        if (R.Num() != HeaderColumns) { bInvalid = true; Finish(TEXT("csv_schema_column_mismatch")); return; }
        Rows.Add(FString::Join(R, TEXT(",")) + TEXT("\n"));
        if (bEndObserved && World->GetTimeSeconds() - EndWorld > .3) { Finish(TEXT("observed_end_plus_300ms")); }
        else if (World->GetTimeSeconds() - RequestWorld > NominalDuration + 1.) { Finish(TEXT("bounded_world_observation_window")); }
    }

    /** Optional: invoke only from a real capsule OnComponentHit callback, never from a query. */
    void ActualCapsuleHit(const FHitResult& Hit)
    {
        if (!Alive() || !bRequested || bFinished || !Config.bActualHitCallbackBound) { return; }
        ++ActualHitCallbacks;
        AppendEvent(FString::Printf(TEXT("actual_capsule_callback,%llu,%.9f,%s,%d,%.9f,%.6f,%.6f,%.6f\n"),
            GFrameCounter, World->GetTimeSeconds(), *Csv(GetPathNameSafe(Hit.GetActor())), Hit.bStartPenetrating,
            Hit.Time, Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z));
    }

    // The host's wall watchdog may call this; it must not kill or mutate the game from this probe.
    void Finish(const FString& InReason)
    {
        if (!bStarted || bFinished) { return; }
        bFinished = true; Reason = InReason;
        bProfileStable = Dodge.IsValid() && Editable(*Dodge) == ProfileBefore;
    }
    bool Finished() const { return bFinished; }
    FString FrameCSV() const { return FString::Join(Rows, TEXT("")); }
    FString EventCSV() const
    { return TEXT("source,frame,world_s,actor,start_penetrating,query_or_hit_time,normal_x,normal_y,normal_z\n") + FString::Join(Events, TEXT("")); }
    FString Summary() const
    {
        const FVector EndDelta = EndPosition - RequestPosition;
        const bool Normal = Config.Context == EContext::BasicRunReadOnly;
        const FString Context = Normal ? TEXT("scope=validated_BasicRun_read_only_observation\n") : TEXT("scope=synthetic_fixture_observation_only\n");
        const FString Hooks = FString::Printf(TEXT("clearance_queries_enabled=%d\nmovement_updated_callback_bound=%d\nactual_hit_callback_bound=%d\ndropped_events=%d\ntarget_dt_comparison_applicable=%d\n"),
            Config.bClearanceQueries, Config.bMovementUpdatedCallbackBound, Config.bActualHitCallbackBound, DroppedEvents, Config.RequestedHz > 0);
        return Context + Hooks + TEXT("dodge_asset=") + GetPathNameSafe(Dodge.Get()) + TEXT("\n") + FString::Printf(TEXT("probe_sends_input=0\nprobe_mutates_game_or_time_or_save=0\nphysical_input=not_performed_by_probe\nnormal_run_completion=not_evaluated\nlabel=%s\ncase=%s\nrequested_hz=%d\nfinished=%d\nreason=%s\nrequest_observed=%d\nactive_transitions=%d\nend_observed=%d\ninvalid_observation=%d\ninvalid_floor=%d\nprofile_unchanged=%d\nnominal_distance_cm=%.9f\nnominal_duration_s=%.9f\nnominal_cooldown_s=%.9f\neffective_stamina_cost=%.9f\nrequest_frame=%llu\nfirst_active_frame=%llu\nend_frame=%llu\nrequest_world_s=%.9f\nfirst_active_world_s=%.9f\nend_observed_world_s=%.9f\nfirst_active_dt=%.9f\nend_dt=%.9f\nendpoint_xy_cm=%.9f\nsigned_progress_cm=%.9f\nsampled_post_tick_xy_path_cm=%.9f\nactual_movement_callback_xy_path_cm=%.9f\nactual_movement_callback_count=%d\nactual_hit_callback_count=%d\nzero_hit_callbacks_does_not_prove_no_collision=1\nstamina_before=%.9f\nstamina_before_scope=prearm_not_request\nstamina_request=%.9f\nstamina_end=%.9f\npost_samples=%d\nactual_dt_min=%.9f\nactual_dt_max=%.9f\nactual_dt_mean=%.9f\ntarget_dt_mismatch_count=%d\ndodge_end_timer_handle=unmeasured\nanim_root_params_outside_PerformMovement=not_certified\nnominal_distance_is_not_asserted_as_realized=1\n"),
            *Config.Label, Config.Case == ECase::FreeFlat ? TEXT("free_flat") : Config.Case == ECase::WallFlat ? TEXT("wall_flat") : TEXT("unclassified_live_world"), Config.RequestedHz,
            bFinished, *Reason, bRequested, Activations, bEndObserved, bInvalid, bInvalidFloor, bProfileStable,
            NominalDistance, NominalDuration, NominalCooldown, EffectiveCost, RequestFrame, FirstActiveFrame, EndFrame,
            RequestWorld, FirstActiveWorld, EndWorld, FirstActiveDT, EndDT,
            bEndObserved ? EndDelta.Size2D() : -1., bEndObserved ? FVector::DotProduct(EndDelta, Direction) : -1.,
            SampledXYPath, Config.bMovementUpdatedCallbackBound ? MovementCallbackXYPath : -1.,
            Config.bMovementUpdatedCallbackBound ? MovementCallbacks : -1, Config.bActualHitCallbackBound ? ActualHitCallbacks : -1, BeforeStamina, RequestStamina, EndStamina,
            PostSamples, PostSamples ? MinDT : -1., PostSamples ? MaxDT : -1., PostSamples ? DTTotal / PostSamples : -1., DTMismatches);
    }

private:
    static bool Single(const TArray<FString>& Options, const TCHAR* Name, FString& Value)
    {
        int32 Count=0; const FString Prefix=FString(Name)+TEXT("=");
        for (const FString& O : Options) if (O.StartsWith(Prefix, ESearchCase::IgnoreCase))
        { ++Count; Value=O.Mid(Prefix.Len()); if (Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) { Value=Value.Mid(1,Value.Len()-2); } }
        return Count==1 && !Value.IsEmpty();
    }
    static bool ValidBasicRunHost(const FConfig& C, const UWorld* W, const URogue10mProgressionSubsystem* Saves, const ARogue10mCharacter& Pawn)
    {
        if (!C.bBasicRunFreshGatePassed || !C.BasicRunInstance.IsValid() || !C.BasicRunProfile.IsValid()
            || !W || W->GetGameInstance()!=C.BasicRunInstance.Get() || !Saves
            || !Saves->IsAttachedTo(Pawn) || Saves->GetAttachedProfileId()!=C.BasicRunProfile) { return false; }
        TArray<FString> Tokens,Options; FCommandLine::Parse(FCommandLine::Get(),Tokens,Options);
        FString Label,Town,Prefix,Keys,Output; int32 OptIn=0;
        for (const FString& O : Options) if (O.Equals(TEXT("RogueBasicRunDodgeObserve"),ESearchCase::IgnoreCase)) { ++OptIn; }
        const FString Expected=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("DevLog/BasicRunPlayQA")/C.Label/TEXT("Driver"));
        return OptIn==1 && Single(Options,TEXT("RogueBasicRunLabel"),Label) && Label==C.Label
            && Single(Options,TEXT("RogueProfileSaveSlot"),Town) && Town==TEXT("Rogue10m_Town_QA_BasicRun_")+Label
            && Single(Options,TEXT("RogueProgressionSavePrefix"),Prefix) && Prefix==TEXT("Rogue10m_Progression_QA_BasicRun_")+Label
            && Single(Options,TEXT("RogueSkillKeySaveSlot"),Keys) && Keys==TEXT("Rogue10m_Fighter_QA_BasicRun_")+Label
            && Single(Options,TEXT("RogueBasicRunOutput"),Output) && FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(Output),Expected)
            && FPaths::IsSamePath(C.BasicRunDriverOutput,Expected);
    }
    void AppendEvent(const FString& Event)
    { if (Events.Num()<128) { Events.Add(Event); } else { ++DroppedEvents; } }
    bool Alive() const { return bStarted && Character.IsValid() && World.IsValid() && Character->GetWorld() == World.Get(); }
    static FString Csv(FString S) { return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")) + TEXT("\""); }
    static void Text(TArray<FString>& R, const FString& S) { R.Add(Csv(S)); }
    static void Num(TArray<FString>& R, double V) { R.Add(FString::Printf(TEXT("%.9f"), V)); }
    static void Vector(TArray<FString>& R, const FVector& V) { Num(R,V.X); Num(R,V.Y); Num(R,V.Z); }
    static FString Editable(const UObject& O)
    {
        FString S;
        for (TFieldIterator<FProperty> It(O.GetClass()); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
            FString V; It->ExportTextItem_Direct(V, It->ContainerPtrToValuePtr<void>(&O), nullptr, nullptr, PPF_None);
            S += It->GetName() + TEXT("=") + V + TEXT("\n");
        }
        return S;
    }
    static void Mesh(TArray<FString>& R, const USkeletalMeshComponent* M)
    {
        const UAnimInstance* Anim = M ? M->GetAnimInstance() : nullptr;
        const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
        Text(R, GetPathNameSafe(M)); Text(R, GetPathNameSafe(M ? M->GetSkeletalMeshAsset() : nullptr));
        Text(R, GetPathNameSafe(Anim)); Text(R, GetPathNameSafe(Montage));
        Num(R, M ? M->IsPlayingRootMotion() : -1); Num(R, Anim ? static_cast<int32>(Anim->RootMotionMode.GetValue()) : -1);
        Num(R, Anim ? Anim->ShouldExtractRootMotion() : -1); Num(R, Montage ? Montage->HasRootMotion() : -1);
        Num(R, Anim && Montage ? Anim->Montage_IsActive(Montage) : -1);
        Num(R, Anim && Montage ? Anim->Montage_GetPosition(Montage) : -1.);
        Num(R, Anim && Montage ? Anim->Montage_GetPlayRate(Montage) : -1.);
    }
    void Query(const TCHAR* Source, float Distance)
    {
        if (!Alive() || Direction.IsNearlyZero()) { return; }
        const auto* C = Character->GetCapsuleComponent(); if (!C) { return; }
        const FVector P = Character->GetActorLocation(); FHitResult Hit;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(DodgeMeasureReadOnlySweep), false, Character.Get());
        const bool Blocking = World->SweepSingleByChannel(Hit,P,P+Direction*Distance,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),Q);
        AppendEvent(FString::Printf(TEXT("query_%s_blocking_%d,%llu,%.9f,%s,%d,%.9f,%.6f,%.6f,%.6f\n"),
            Source, Blocking, GFrameCounter, World->GetTimeSeconds(), *Csv(GetPathNameSafe(Hit.GetActor())),
            Hit.bStartPenetrating, Hit.Time, Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z));
    }
    static FString Header()
    {
        return TEXT("hook,frame,world_s,wall_s,dt,requested_hz,fixed_step_enabled,fixed_step_dt,dodging,x,y,z,vx,vy,vz,ax,ay,az,last_input_x,last_input_y,last_input_z,old_x,old_y,old_z,old_vx,old_vy,old_vz,movement_mode,max_speed,max_walk_speed,ground_friction,braking_deceleration_walking,braking_friction_factor,separate_braking,braking_friction,dodge_cd,stamina,end_timer_scope,has_rm_sources,rm_override,rm_additive,anim_rm_params_scope,body_component,body_asset,body_anim,body_montage,body_is_rm,body_rm_mode,body_should_extract,body_montage_has_rm,body_montage_active,body_montage_s,body_montage_rate,source_component,source_asset,source_anim,source_montage,source_is_rm,source_rm_mode,source_should_extract,source_montage_has_rm,source_montage_active,source_montage_s,source_montage_rate,fp_component,fp_asset,fp_anim,fp_montage,fp_is_rm,fp_rm_mode,fp_should_extract,fp_montage_has_rm,fp_montage_active,fp_montage_s,fp_montage_rate,floor_actor,floor_normal_z\n");
    }
    static constexpr int32 HeaderColumns = 77;
    FConfig Config;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<const URogue10mDodgeSkillDataAsset> Dodge;
    TArray<FString> Rows, Events;
    FString ProfileBefore, Reason;
    FVector Direction=FVector::ZeroVector, RequestPosition=FVector::ZeroVector, LastPostPosition=FVector::ZeroVector, EndPosition=FVector::ZeroVector;
    double StartedWall=0, RequestWall=0, RequestWorld=-1, FirstActiveWorld=-1, EndWorld=-1, FirstActiveDT=0, EndDT=0;
    double SampledXYPath=0, MovementCallbackXYPath=0, MinDT=DBL_MAX, MaxDT=0, DTTotal=0;
    float NominalDistance=0, NominalDuration=0, NominalCooldown=0, EffectiveCost=0, BeforeStamina=0, RequestStamina=-1, EndStamina=-1;
    uint64 RequestFrame=0, FirstActiveFrame=0, EndFrame=0;
    int32 Activations=0, PostSamples=0, DTMismatches=0, MovementCallbacks=0, ActualHitCallbacks=0, DroppedEvents=0;
    bool bStarted=false, bRequested=false, bFinished=false, bPreviousActive=false, bEndObserved=false;
    bool bInvalid=false, bInvalidFloor=false, bProfileStable=false;
};
}
#endif
