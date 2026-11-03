// Opt-in standalone editor-build fixture. Programmatic input; no physical-input or full-run claim.
#if WITH_EDITOR
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunHUD.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace Rogue10mBossReadabilityQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& W)
    {
        World = &W;
        FString Town, Prefix, Keys;
        const TCHAR* Args = FCommandLine::Get();
        FParse::Value(Args, TEXT("RogueBossReadabilityLabel="), Label);
        FParse::Value(Args, TEXT("RogueProfileSaveSlot="), Town);
        FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
        FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), Keys);
        bool Good = !Label.IsEmpty() && Label.Len() <= 24;
        for (TCHAR C : Label) Good &= FChar::IsAlnum(C) || C == TEXT('_');
        TArray<FString> Existing;
        IFileManager::Get().FindFiles(Existing, *(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))), true, false);
        if (!Good || W.GetNetMode() != NM_Standalone
            || Town != TEXT("Rogue10m_Town_QA_BossReadability_")+Label
            || Prefix != TEXT("Rogue10m_Progression_QA_BossReadability_")+Label
            || Keys != TEXT("Rogue10m_Fighter_QA_BossReadability_")+Label
            || UGameplayStatics::DoesSaveGameExist(Town, 0) || UGameplayStatics::DoesSaveGameExist(Keys, 0) || !Existing.IsEmpty())
        { UE_LOG(LogRogue10m, Error, TEXT("RESULT=BOSS_READABILITY_FAILED fresh_exact_QA_namespace_required")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        Output = FPaths::ProjectDir()/TEXT("Feature/doc/evidence/boss-readability-20261103")/Label;
        Images = FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261103_BossReadability_")+Label);
        bRendered = !FParse::Param(Args, TEXT("nullrhi"));
        if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
        { UE_LOG(LogRogue10m, Error, TEXT("RESULT=BOSS_READABILITY_FAILED fresh_evidence_required")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        IFileManager::Get().MakeDirectory(*Output, true);
        if (bRendered) IFileManager::Get().MakeDirectory(*Images, true);
        Frames = TEXT("frame,world_s,dt,case,elapsed_s,state,warning,health,hit_count,player_x,player_y,player_z,speed,dodging,dodge_cd,inside_narrow,boss_move_cm,locked_move_cm,locked_forward_error,windup_remaining\n");
        Captures = TEXT("name,event,frame,world_s,case,elapsed_s,state,health,hud_visible,hud_tracked_boss,hud_remaining,hud_duration,hud_progress\n");
        HudFrames = TEXT("frame,world_s,case,elapsed_s,pattern_state,locked_local,tracked_boss,hud_visible,hud_remaining,hud_duration,hud_progress,blocking_ui,inspection\n");
        Deadline = FPlatformTime::Seconds()+240.0;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) return false;
            if (FPlatformTime::Seconds() > Self->Deadline) { Self->Check(false, TEXT("watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt) { auto F=MoveTemp(Self->Next); F(); }
            return !Self->bFinished;
        }), .01f);
        Cleanup = FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X, bool, bool)
        { if (X == Self->World.Get() && !Self->bFinished) { Self->Check(false, TEXT("unexpected_world_cleanup")); Self->Finish(false); } });
        Later(3.0, [this] { Prepare(); });
    }
private:
    struct FPhoto { FString Name; int32 Width=0, Height=0; TArray<FColor> Pixels; };
    void Check(bool OK, const TCHAR* Why)
    { ++Checks; Results += FString(OK ? TEXT("PASS ") : TEXT("FAIL "))+Why+TEXT("\n"); if (!OK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("BOSS_READABILITY FAIL %s"), Why); } }
    void Later(double Delay, TFunction<void()> F) { NextAt=FPlatformTime::Seconds()+Delay; Next=MoveTemp(F); }
    void Key(FKey K, bool Down) { if (PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, Down?IE_Pressed:IE_Released, Down?1.f:0.f)); }
    void StopInput()
    { Key(EKeys::S, false); Key(EKeys::D, false); Key(Rogue10mInput::DodgeKey(), false); }
    void Photo(const TCHAR* Suffix)
    {
        if (!bRendered) return;
        Check(PendingPhoto.IsEmpty(), TEXT("previous_capture_finished"));
        if (!PendingPhoto.IsEmpty()) return;
        PendingPhoto=CaseName+TEXT("_")+Suffix+TEXT(".png"); ++Requested;
        CaptureRow(TEXT("request")); FScreenshotRequest::RequestScreenshot(Images/PendingPhoto, true, false);
    }
    void CaptureRow(const TCHAR* Event)
    {
        const auto V=HUD.IsValid()?HUD->GetBossWindupView():FRogue10mHudBossWindupView();
        const bool Tracked=HUD.IsValid() && HUD->GetTrackedMonsterForHUD()==Boss.Get();
        Captures += FString::Printf(TEXT("%s,%s,%llu,%.9f,%s,%.9f,%d,%.3f,%d,%d,%.6f,%.6f,%.6f\n"), *PendingPhoto, Event, GFrameCounter,
            World->GetTimeSeconds(), *CaseName, World->GetTimeSeconds()-StartTime,
            Pattern.IsValid()?int32(Pattern->GetPatternState()):-1, Player.IsValid()?Player->GetRogueAttributeSet()->GetHealth():-1.f,
            V.bVisible,Tracked,V.RemainingSeconds,V.TotalSeconds,V.ProgressAlpha);
        const bool Expected=PendingPhoto.EndsWith(TEXT("_ready.png"));
        Check(V.bVisible==Expected,TEXT("actual_capture_HUD_visibility_matches_ready_or_resolved_phase"));
        if (Expected) CheckHudWindup(TEXT("capture"));
        if (FString(Event)==TEXT("captured") && Case==0 && Expected && !bHudVisibilityProbed) RunHudVisibilityProbes();
    }
    void CheckHudWindup(const TCHAR* Where)
    {
        if (!HUD.IsValid() || !Pattern.IsValid() || !Player.IsValid()) { Check(false,TEXT("actual_owned_viewport_HUD_available")); return; }
        const auto V=HUD->GetBossWindupView();
        const auto Shape=Pattern->GetLockedAttackShape();
        Check(HUD->GetTrackedMonsterForHUD()==Boss.Get(),TEXT("warning_reuses_actual_displayed_boss_target_without_seeding"));
        Check(V.bVisible && Pattern->IsWindingAgainst(*Player.Get()) && Shape.bWindup,TEXT("actual_local_target_windup_HUD_is_visible"));
        Check(V.AttackType==Type && !V.AttackName.IsEmpty(),TEXT("HUD_pattern_name_and_type_match_actual_attack"));
        Check(FMath::IsFinite(V.RemainingSeconds) && FMath::IsFinite(V.TotalSeconds) && V.TotalSeconds>0.f
            && FMath::Abs(V.RemainingSeconds-Shape.WindupRemaining)<0.035f
            && FMath::Abs(V.TotalSeconds-Shape.WindupDuration)<0.001f
            && FMath::Abs(V.TotalSeconds-ExpectedLead)<0.035f,TEXT("HUD_seconds_use_actual_locked_duration_and_remaining_time"));
        const float ExpectedProgress=Shape.WindupDuration>0.f?FMath::Clamp(1.f-Shape.WindupRemaining/Shape.WindupDuration,0.f,1.f):0.f;
        Check(FMath::IsFinite(V.ProgressAlpha) && FMath::Abs(V.ProgressAlpha-ExpectedProgress)<0.001f,TEXT("HUD_progress_matches_actual_windup_fraction"));
        Results+=FString::Printf(TEXT("HUD %s %s visible=%d tracked=%d type=%d remaining=%.6f total=%.6f progress=%.6f\n"),
            *CaseName,Where,V.bVisible,HUD->GetTrackedMonsterForHUD()==Boss.Get(),int32(V.AttackType),V.RemainingSeconds,V.TotalSeconds,V.ProgressAlpha);
    }
    void RunHudVisibilityProbes()
    {
        bHudVisibilityProbed=true;
        Check(HUD.IsValid() && PC.IsValid() && Player.IsValid() && Pattern.IsValid()
            && Pattern->IsWindingAgainst(*Player.Get()) && !UGameplayStatics::IsGamePaused(World.Get()),
            TEXT("HUD_visibility_probes_use_same_real_active_local_windup"));
        if (Failures) return;
        const FRotator Control=PC->GetControlRotation();
        const auto* Appearance=Player->GetAppearanceCameraComponent();
        const float Remaining=Pattern->GetLockedAttackShape().WindupRemaining;
        bHudProbeOwnsPause=PC->SetPause(true);
        Check(bHudProbeOwnsPause && UGameplayStatics::IsGamePaused(World.Get()),TEXT("HUD_probe_owns_scoped_world_pause"));
        PC->ToggleInventory();
        Check(PC->IsInventoryVisible() && PC->IsAnyBlockingWindowVisible() && !HUD->GetBossWindupView().bVisible,
            TEXT("actual_inventory_hides_first_person_boss_warning"));
        PC->ToggleInventory();
        Check(!PC->IsAnyBlockingWindowVisible(),TEXT("HUD_probe_closes_actual_inventory"));
        if (bHudProbeOwnsPause) PC->SetPause(false);
        bHudProbeOwnsPause=false;
        PC->ToggleInspectionCamera();
        Check(Appearance && Appearance->IsThirdPersonInspectionActive() && !HUD->GetBossWindupView().bVisible,
            TEXT("actual_controller_inspection_hides_first_person_boss_warning"));
        if (Appearance && Appearance->IsThirdPersonInspectionActive()) PC->ToggleInspectionCamera();
        Check(Appearance && !Appearance->IsThirdPersonInspectionActive() && HUD->GetBossWindupView().bVisible
            && PC->GetControlRotation().Equals(Control,.01f)
            && FMath::Abs(Pattern->GetLockedAttackShape().WindupRemaining-Remaining)<.001f,
            TEXT("same_windup_warning_returns_without_aim_change_or_world_time_advance"));
        CheckHudWindup(TEXT("after_visibility_probes"));
        ++HudVisibilityProbeCount;
    }
    void Prepare()
    {
        PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        auto* GI=World->GetGameInstance();
        auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
        Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
        Check(PC.IsValid() && Profiles && Saves.IsValid(), TEXT("actual_controller_profile_progression"));
        if (Failures) { Finish(); return; }
        FRogue10mCharacterProfile P;
        Check(Profiles->GetProfileCount()==0 && Profiles->CreateProfile(TEXT("BossReadQA"), FRogue10mCharacterAppearance(), P)
            && Profiles->SelectProfile(P.ProfileId), TEXT("fresh_profile_created_selected"));
        if (Failures) { Finish(); return; }
        PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Player=Cast<ARogue10mCharacter>(PC->GetPawn()); Tools=PC->GetCombatTestComponent();
        Check(Player.IsValid() && Tools.IsValid() && Player->GetRogueAttributeSet(), TEXT("actual_pawn_and_test_tools"));
        if (Failures) { Finish(); return; }
        for (TObjectIterator<URogue10mRunHUD> It;It;++It)
            if (It->GetWorld()==World.Get() && It->GetOwningPlayer()==PC.Get() && It->IsInViewport() && It->IsVisible())
            { Check(!HUD.IsValid(),TEXT("exactly_one_actual_owned_viewport_RunHUD")); HUD=*It; }
        Check(HUD.IsValid(),TEXT("actual_RunHUD_found_without_creating_or_seeding_target"));
        if (Failures) { Finish(); return; }
        Saves->DetachCharacter(*Player, false);
        Check(!Saves->IsAttachedTo(*Player), TEXT("transient_fixture_not_saved_as_progression"));
        Initial=Player->GetActorTransform(); InitialControl=PC->GetControlRotation(); bOwnFixture=true;
        FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
        Ground=World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
        auto* Box=Ground.IsValid()?NewObject<UBoxComponent>(Ground.Get(), TEXT("BossReadabilityFloor"), RF_Transient):nullptr;
        Check(Box!=nullptr, TEXT("transient_collision_floor")); if (Failures) { Finish(); return; }
        Ground->AddInstanceComponent(Box); Ground->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,20));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetCanEverAffectNavigation(false); Box->RegisterComponent();
        Surface=Initial.GetLocation().Z+1500.0;
        Ground->SetActorLocation(FVector(Initial.GetLocation().X, Initial.GetLocation().Y, Surface-20.0));
        auto* Visible=NewObject<UStaticMeshComponent>(Ground.Get(), TEXT("VisibleOwnedFloor"), RF_Transient);
        Ground->AddInstanceComponent(Visible); Visible->SetupAttachment(Box);
        Visible->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Visible->SetRelativeScale3D(FVector(200,200,.4)); Visible->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visible->RegisterComponent();
        Player->SetActorLocation(FVector(Initial.GetLocation().X, Initial.GetLocation().Y, Surface+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2), false, nullptr, ETeleportType::TeleportPhysics);
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling); Player->GetCharacterMovement()->bForceNextFloorCheck=true;
        CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 W, int32 H, const TArray<FColor>& Pixels)
        {
            if (Self->bFinished || Self->PendingPhoto.IsEmpty()) return;
            Self->CaptureRow(TEXT("captured")); auto& P=Self->Photos.AddDefaulted_GetRef();
            P.Name=Self->PendingPhoto; P.Width=W; P.Height=H; P.Pixels=Pixels; Self->PendingPhoto.Empty(); ++Self->Completed;
        });
        SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W, ELevelTick, float DT)
        { if (W==Self->World.Get()) Self->Sample(DT); });
        Later(.8, [this]
        {
            Check(Player->GetCharacterMovement()->IsMovingOnGround() && Player->GetMovementBaseObject()==Ground->GetRootComponent(), TEXT("actual_player_grounded_on_owned_floor"));
            Fixture=Player->GetActorTransform(); if (Failures) Finish(); else { BroadphaseCornerProbes(); BeginCase(); }
        });
    }
    void BroadphaseCornerProbes()
    {
        // Reproduce the actual raw-box broadphase against an isolated capsule, not just the narrow predicate.
        const float R=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HH=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        for (const float Yaw : {0.f, 73.f})
        {
            const FVector F=FRotator(0,Yaw,0).Vector(), Right(-F.Y,F.X,0);
            FRogue10mMonsterAttackShape Shape; Shape.AttackType=ERogue10mMonsterAttackType::FrontalWave;
            Shape.Location=Fixture.GetLocation()+FVector(0,2500,0); Shape.Forward=F;
            Shape.Range=850; Shape.HalfWidth=180; Shape.HalfHeight=160;
            const FVector2D Points[]={{850+R-2,0},{850+R-2,180+R-2},{850+R*.5f,180+R*.5f}};
            for (int32 Index=0;Index<3;++Index)
            {
                FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
                AActor* Probe=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
                auto* Capsule=Probe?NewObject<UCapsuleComponent>(Probe,TEXT("RawOverlapCapsule"),RF_Transient):nullptr;
                Check(Capsule!=nullptr,TEXT("isolated_actual_capsule_overlap_probe")); if (!Capsule) { if(Probe)Probe->Destroy(); continue; }
                Probe->AddInstanceComponent(Capsule); Probe->SetRootComponent(Capsule); Capsule->SetCapsuleSize(R,HH);
                Capsule->SetCollisionObjectType(ECC_Pawn); Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Capsule->SetCollisionResponseToAllChannels(ECR_Block); Capsule->RegisterComponent();
                const FVector Center=Shape.Location+F*Points[Index].X+Right*Points[Index].Y;
                Probe->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);
                FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
                FCollisionQueryParams Query(SCENE_QUERY_STAT(BossReadabilityRawOverlap),false);
                TArray<FOverlapResult> Overlaps;
                World->OverlapMultiByObjectType(Overlaps,Shape.Location+F*425,FRotator(0,Yaw,0).Quaternion(),Objects,
                    FCollisionShape::MakeBox(FVector(425,180,160)),Query);
                bool Raw=false; for (const auto& Hit:Overlaps) Raw|=Hit.GetActor()==Probe;
                const bool Narrow=Rogue10mMonsterAttackGeometry::ContainsCapsule(Shape,Center,R,HH);
                Check(Narrow,TEXT("expanded_narrow_shape_contains_probe"));
                Check(Raw==(Index!=1),Index==1?TEXT("actual_broadphase_excludes_rounded_far_side_corner"):TEXT("actual_broadphase_includes_face_or_rounded_inset"));
                Results+=FString::Printf(TEXT("BROADPHASE yaw=%.3f point=%d radius=%.6f narrow=%d raw=%d final_candidate=%d\n"),Yaw,Index,R,Narrow,Raw,Narrow&&Raw);
                Probe->Destroy();
            }
        }
    }
    void BeginCase()
    {
        if (Case>=18) { Finish(); return; }
        StopInput(); bTrial=false; bHudLeadChecked=false;
        Tools->SetPanelOpen(false); Tools->ExecuteAction(ERogue10mCombatTestAction::Clear);
        Player->SetActorTransform(Fixture, false, nullptr, ETeleportType::TeleportPhysics);
        Player->GetCharacterMovement()->StopMovementImmediately(); PC->SetControlRotation(FRotator(0, Fixture.Rotator().Yaw, 0));
        Player->GetRogueAttributeSet()->SetMaxHealth(50000); Player->GetRogueAttributeSet()->RestoreVitals();
        Type=ERogue10mMonsterAttackType((Case/3)%3); Mode=Case%3; bEnraged=Case>=9;
        CaseName=FString::Printf(TEXT("%s_%s_%s"), Type==ERogue10mMonsterAttackType::Basic?TEXT("basic"):Type==ERogue10mMonsterAttackType::FrontalWave?TEXT("instant_wave"):TEXT("zone"), bEnraged?TEXT("enraged"):TEXT("normal"), Mode==0?TEXT("stand"):Mode==1?TEXT("walk"):TEXT("walk_alt"));
        Tools->SelectCreature(5); Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn), TEXT("real_boss_test_instance_spawned"));
        Boss=Tools->GetTestTarget(); Pattern=Boss.IsValid()?Boss->GetAttackPatternComponent():nullptr;
        Check(Boss.IsValid() && Boss->IsCombatTestInstance() && Pattern.IsValid() && Pattern->GetProfile(), TEXT("reward_free_real_boss_profile"));
        if (Failures) { Finish(); return; }
        const auto* LiveData=LoadObject<URogue10mMonsterDataAsset>(nullptr,TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor.DA_OW_AbyssOverlordAzathor"));
        Check(LiveData && Boss->GetMonsterData()==LiveData && Pattern->GetProfile()==LiveData->AttackPatternProfile,
            TEXT("fixture_resolves_actual_live_monster_DA_profile_not_old_hardcoded_profile"));
        if (Failures) { Finish(); return; }
        if (Case==0) { MissingRequiredBasicMaterialProbes(); if (Failures) { Finish(); return; } }
        if (bEnraged) Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Enrage), TEXT("existing_HP40_enrage_fixture"));
        Later(.3, [this] { StartCase(); });
    }
    void MissingRequiredBasicMaterialProbes()
    {
        auto* Original=Pattern->GetProfile();
        auto* Missing=DuplicateObject<URogue10mMonsterAttackPatternProfile>(Original,GetTransientPackage());
        Check(Missing!=nullptr,TEXT("transient_configuration_failure_profile_copy")); if (!Missing) return;
        Missing->BasicTelegraphMaterial.Reset(); Missing->bRequireBasicTelegraph=true;
        Pattern->ConfigureProfile(Missing);
        const int32 Started=Pattern->GetStartedAttackCount();
        Check(!Pattern->TryStartCombatTestAttack(Player.Get(),ERogue10mMonsterAttackType::Basic)
            && !Pattern->IsAttackActive() && Pattern->GetStartedAttackCount()==Started,
            TEXT("explicit_basic_action_fails_closed_without_starting_special"));
        Check(Pattern->TryStartAttack(Player.Get()) && Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::FrontalWave,
            TEXT("missing_basic_material_skips_first_melee_slot_to_valid_wave"));
        Pattern->CancelAttack();
        Check(Pattern->TryStartAttack(Player.Get()) && Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::AbyssZone,
            TEXT("missing_basic_material_advances_past_selected_wave_to_valid_zone"));
        Pattern->CancelAttack();
        Check(!Pattern->TryStartAttack(Player.Get()) && !Pattern->IsAttackActive(),
            TEXT("missing_basic_and_cooling_specials_do_not_start_invisible_basic"));
        Missing->FrontalWaveTelegraphMaterial.Reset(); Pattern->ConfigureProfile(Missing);
        Check(Pattern->TryStartAttack(Player.Get()) && Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::AbyssZone,
            TEXT("missing_basic_and_wave_materials_keep_valid_zone_available"));
        Pattern->CancelAttack();
        Missing->FrontalWaveTelegraphMaterial=Original->FrontalWaveTelegraphMaterial; Missing->AbyssZoneTelegraphMaterial.Reset();
        Pattern->ConfigureProfile(Missing);
        Check(Pattern->TryStartAttack(Player.Get()) && Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::FrontalWave,
            TEXT("missing_basic_and_zone_materials_keep_valid_wave_available"));
        Pattern->CancelAttack();
        Pattern->ConfigureProfile(Original);
        Check(Pattern->GetProfile()==Original && !Pattern->IsAttackActive(),TEXT("live_profile_restored_after_transient_missing_material_probes"));
    }
    void StartCase()
    {
        auto Action=Type==ERogue10mMonsterAttackType::Basic?ERogue10mCombatTestAction::Basic:
            Type==ERogue10mMonsterAttackType::FrontalWave?ERogue10mCombatTestAction::FrontalWave:ERogue10mCombatTestAction::AbyssZone;
        Check(Tools->ExecuteAction(Action), TEXT("production_warning_damage_pipeline_started"));
        if (Failures) { Finish(); return; }
        Locked=Pattern->GetLockedAttackShape(); BossStart=Boss->GetActorLocation(); StartLocation=Player->GetActorLocation();
        StartTime=World->GetTimeSeconds(); Health=Player->GetRogueAttributeSet()->GetHealth(); Hits=Pattern->GetSuccessfulHitCount();
        bMoved=bAlt=bSawDodge=bContact=bRecoveryPhoto=false; MaxBossMove=0; ObservedLead=-1; bTrial=true;
        Check(Pattern->IsTelegraphVisible(), TEXT("warning_exists_from_attack_start"));
        Check(Rogue10mMonsterAttackGeometry::ContainsCapsule(Locked, Player->GetActorLocation(), Locked.TargetCapsuleRadius, Locked.TargetCapsuleHalfHeight), TEXT("initial_center_inside_narrow_shape"));
        ExpectedLead=Type==ERogue10mMonsterAttackType::Basic?1.0:Type==ERogue10mMonsterAttackType::FrontalWave?1.6:1.9;
        if (bEnraged && Type!=ERogue10mMonsterAttackType::Basic) ExpectedLead*=.75;
        Check(FMath::Abs(Locked.WindupRemaining-ExpectedLead)<.035, TEXT("actual_profile_matches_tuning_candidate"));
        Results+=FString::Printf(TEXT("CASE %s profile=%s authored_lead=%.9f initial_warning=1\n"), *CaseName, *Pattern->GetProfile()->GetPathName(), ExpectedLead);
    }
    void Sample(float DT)
    {
        if (!bTrial || bFinished || !Pattern.IsValid() || !Boss.IsValid() || !Player.IsValid()) return;
        const double Elapsed=World->GetTimeSeconds()-StartTime;
        const auto S=Pattern->GetLockedAttackShape();
        const float Move=FVector::Dist(BossStart, Boss->GetActorLocation()); MaxBossMove=FMath::Max(MaxBossMove, Move);
        const auto* Capsule=Player->GetCapsuleComponent();
        const bool Inside=Rogue10mMonsterAttackGeometry::ContainsCapsule(S, Player->GetActorLocation(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        const FVector P=Player->GetActorLocation();
        Frames+=FString::Printf(TEXT("%llu,%.9f,%.9f,%s,%.9f,%d,%d,%.3f,%d,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%d,%.6f,%.6f,%.9f,%.6f\n"),
            GFrameCounter, World->GetTimeSeconds(), DT, *CaseName, Elapsed, int32(S.State), Pattern->IsTelegraphVisible(), Player->GetRogueAttributeSet()->GetHealth(), Pattern->GetSuccessfulHitCount(), P.X,P.Y,P.Z,
            Player->GetVelocity().Size2D(), Player->IsDodging(), Player->GetDodgeCooldownRemaining(), Inside, Move, FVector::Dist(Locked.Location,S.Location), 1.0-FVector::DotProduct(Locked.Forward,S.Forward), S.WindupRemaining);
        const auto Hud=HUD.IsValid()?HUD->GetBossWindupView():FRogue10mHudBossWindupView();
        const auto* Appearance=Player->GetAppearanceCameraComponent();
        HudFrames+=FString::Printf(TEXT("%llu,%.9f,%s,%.9f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%d,%d\n"),
            GFrameCounter,World->GetTimeSeconds(),*CaseName,Elapsed,int32(S.State),Pattern->IsWindingAgainst(*Player.Get()),
            HUD.IsValid() && HUD->GetTrackedMonsterForHUD()==Boss.Get(),Hud.bVisible,Hud.RemainingSeconds,Hud.TotalSeconds,Hud.ProgressAlpha,
            PC->IsAnyBlockingWindowVisible(),Appearance && Appearance->IsThirdPersonInspectionActive());
        if (!bHudLeadChecked && Elapsed>=.1 && S.bWindup)
        {
            bHudLeadChecked=true; CheckHudWindup(TEXT("before_reaction_input")); ++HudCasesChecked;
            if (!bRendered && Case==0 && !bHudVisibilityProbed) RunHudVisibilityProbes();
        }
        bSawDodge|=Player->IsDodging();
        if (Mode==0 && Elapsed>=.1 && !bMoved) { bMoved=true; Photo(TEXT("ready")); }
        if (Mode!=0 && Elapsed>=.25 && !bMoved) { bMoved=true; Key(Type==ERogue10mMonsterAttackType::FrontalWave?EKeys::D:EKeys::S, true); }
        if (Mode==2 && Elapsed>=.32 && !bAlt) { bAlt=true; Key(Rogue10mInput::DodgeKey(), true); Key(Rogue10mInput::DodgeKey(), false); }
        if (!bContact && S.State==ERogue10mMonsterPatternState::Recovery)
        {
            bContact=true; ObservedLead=Elapsed; StopInput();
            Check(FMath::Abs(ObservedLead-ExpectedLead)<=FMath::Max(.09, double(DT)*2), TEXT("observed_activation_matches_authored_lead_with_frame_tolerance"));
            Check(!Pattern->IsTelegraphVisible(), TEXT("warning_removed_at_resolution"));
            Check(HUD.IsValid() && !HUD->GetBossWindupView().bVisible,TEXT("HUD_warning_hidden_immediately_at_resolution"));
            const bool Damaged=Pattern->GetSuccessfulHitCount()>Hits && Player->GetRogueAttributeSet()->GetHealth()<Health;
            Check(Mode==0?Damaged:!Damaged, Mode==0?TEXT("stationary_control_hit"):TEXT("delayed_normal_input_avoids_damage"));
            if (Mode==2) Check(bSawDodge, TEXT("configured_Alt_input_accepted_actual_dodge"));
            if (Mode!=0) Check(FVector::Dist2D(StartLocation,P)>5, TEXT("avoidance_uses_actual_player_translation"));
            if (Mode==0) Photo(TEXT("resolution"));
        }
        if (Mode==0 && bContact && !bRecoveryPhoto && Elapsed>=ObservedLead+.30)
        { bRecoveryPhoto=true; Photo(TEXT("recovery")); }
        if (bContact && S.State==ERogue10mMonsterPatternState::Idle)
        {
            Check(MaxBossMove<5, TEXT("boss_position_frozen_through_windup_recovery"));
            Check(bHudLeadChecked && HUD.IsValid() && !HUD->GetBossWindupView().bVisible,TEXT("case_observed_visible_windup_and_hidden_idle_HUD"));
            Check(FVector::Dist(Locked.Location,S.Location)<.1 && FVector::DotProduct(Locked.Forward,S.Forward)>.99999, TEXT("warning_geometry_not_retargeted_during_attack"));
            Results+=FString::Printf(TEXT("OBSERVED %s lead=%.9f health_drop=%.3f travel_cm=%.6f boss_move_cm=%.6f dodge_seen=%d\n"), *CaseName, ObservedLead, Health-Player->GetRogueAttributeSet()->GetHealth(), FVector::Dist2D(StartLocation,P), MaxBossMove, bSawDodge);
            bTrial=false; ++Case; Later(.45, [this] { BeginCase(); });
        }
        if (Elapsed>8 && bTrial) { Check(false, TEXT("attack_case_timeout")); bTrial=false; Finish(); }
    }
    void Finish(bool Exit=true)
    {
        if (bFinished) return; bFinished=true; bTrial=false; Next=nullptr; StopInput();
        FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle); FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);
        if (PC.IsValid())
        {
            PC->CloseAllBlockingPanels();
            if (bHudProbeOwnsPause) PC->SetPause(false);
            bHudProbeOwnsPause=false;
            if (Player.IsValid() && Player->GetAppearanceCameraComponent() && Player->GetAppearanceCameraComponent()->IsThirdPersonInspectionActive()) PC->ToggleInspectionCamera();
        }
        if (Tools.IsValid()) { Tools->SetPanelOpen(false); Tools->ExecuteAction(ERogue10mCombatTestAction::Clear); }
        if (bOwnFixture && Player.IsValid()) { Player->SetActorTransform(Initial, false, nullptr, ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
        if (bOwnFixture && PC.IsValid()) PC->SetControlRotation(InitialControl);
        if (Ground.IsValid()) Ground->Destroy();
        if (bRendered) Check(Requested==Completed && PendingPhoto.IsEmpty(), TEXT("all_actual_viewport_captures_completed"));
        for (const auto& P:Photos) { TArray64<uint8> Bytes; FImageUtils::PNGCompressImageArray(P.Width,P.Height,P.Pixels,Bytes); Check(FFileHelper::SaveArrayToFile(Bytes,*(Images/P.Name)), TEXT("actual_pixels_saved_as_PNG")); }
        Photos.Empty();
        if (!Output.IsEmpty())
        {
            Check(FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))), TEXT("frames_written_at_finish_no_live_append"));
            Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))), TEXT("capture_metadata_written"));
            Check(FFileHelper::SaveStringToFile(HudFrames,*(Output/TEXT("hud-frames.csv"))),TEXT("actual_HUD_frame_data_written"));
            Check(HudCasesChecked==18 && HudVisibilityProbeCount==1,TEXT("all_18_cases_and_one_menu_inspection_probe_completed"));
            Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\ncases_completed=%d\nrendered=%d\ninput=programmatic_Controller_InputKey\nreaction_delay_s=.25\nAlt_s=.32_after_warning_with_movement_key_held\nstandalone_owned_floor_and_reward_free_boss_only=1\nno_teleport_during_attack=1\nHP_damage_cooldown_assets_unchanged=1\nhud_windup_cases_checked=%d\nhud_menu_inspection_probes=%d\nhud_target_source=normal_HP_widget_refresh_not_seeded\nhud_pixels=requires_actual_NativePaint_PNG_review\nhud_death_and_wrong_locked_actor=source_review_only_no_extra_HP_or_actor_fixture\nphysical_input_and_full_field_run=not_performed\ngeometry=expanded_narrow_shape_plus_actual_overlap_selected_target_height_and_LOS_gates\nwave=instant_locked_rectangle_no_travelling_projectile\nvisual_acceptance=requires_actual_ready_resolution_recovery_PNG_review\n"), Checks,Failures,Case,bRendered,HudCasesChecked,HudVisibilityProbeCount);
            if (!FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt")))) ++Failures;
        }
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=BOSS_READABILITY_%s failures=%d cases=%d"), Failures?TEXT("FAILED"):TEXT("PASSED"), Failures, Case);
        if (Exit) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float) { FPlatformMisc::RequestExitWithStatus(false,Code); return false; }), .3f);
    }
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Player; TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
    TWeakObjectPtr<ARogue10mBasicMonster> Boss; TWeakObjectPtr<URogue10mMonsterAttackPatternComponent> Pattern;
    TWeakObjectPtr<URogue10mRunHUD> HUD;
    TWeakObjectPtr<URogue10mProgressionSubsystem> Saves; TWeakObjectPtr<AActor> Ground;
    FDelegateHandle CaptureHandle,SampleHandle,Cleanup; TFunction<void()> Next; TArray<FPhoto> Photos;
    FString Label,Output,Images,Results,Frames,Captures,HudFrames,CaseName,PendingPhoto;
    FTransform Initial,Fixture; FRotator InitialControl; FRogue10mMonsterAttackShape Locked;
    FVector BossStart,StartLocation; ERogue10mMonsterAttackType Type=ERogue10mMonsterAttackType::Basic;
    double Deadline=0,NextAt=0,Surface=0,StartTime=0,ExpectedLead=0,ObservedLead=-1;
    float Health=0,MaxBossMove=0; int32 Case=0,Mode=0,Hits=0,Checks=0,Failures=0,Requested=0,Completed=0,HudCasesChecked=0,HudVisibilityProbeCount=0;
    bool bHudLeadChecked=false,bHudVisibilityProbed=false,bHudProbeOwnsPause=false;
    bool bFinished=false,bRendered=false,bTrial=false,bEnraged=false,bMoved=false,bAlt=false,bSawDodge=false,bContact=false,bRecoveryPhoto=false,bOwnFixture=false;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestBossReadability"), TEXT("Fresh exact BossReadability QA saves only: real delayed walk/Alt avoidance, timing and viewport captures."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (!GEngine || GIsEditor || Active) return;
        for (const FWorldContext& C:GEngine->GetWorldContexts()) if (UWorld* W=C.World(); W && C.WorldType==EWorldType::Game)
        { Active=MakeShared<FRun>(); Active->Start(*W); return; }
    }));
}
#endif
