// Opt-in isolated NPC/input QA. Programmatic Controller InputKey, never physical-input/full-run QA.
#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
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
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace Rogue10mNpcInteractionQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
    bool Finished() const { return bFinished; }
    void Start(UWorld& InWorld)
    {
        World=&InWorld; const TCHAR* Args=FCommandLine::Get(); FString Town,Prefix,Keys;
        FParse::Value(Args,TEXT("RogueNpcInteractionLabel="),Label);
        FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town); FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix); FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
        bool bLabel=!Label.IsEmpty() && Label.Len()<=24;
        for (TCHAR C:Label) { bLabel&=(C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_'); }
        TArray<FString> Existing; IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
        if (!bLabel || InWorld.GetNetMode()!=NM_Standalone || Town!=TEXT("Rogue10m_Town_QA_FpNpc_")+Label
            || Prefix!=TEXT("Rogue10m_Progression_QA_FpNpc_")+Label || Keys!=TEXT("Rogue10m_Fighter_QA_FpNpc_")+Label
            || UGameplayStatics::DoesSaveGameExist(Town,0) || UGameplayStatics::DoesSaveGameExist(Keys,0) || !Existing.IsEmpty())
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=NPC_INTERACTION_FAILED reason=exact_fresh_FpNpc_slots_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/npc-interaction-20261029")/Label;
        Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261029_NpcInteraction_")+Label);
        if (IFileManager::Get().DirectoryExists(*Output)||IFileManager::Get().DirectoryExists(*Images))
        { UE_LOG(LogRogue10m,Error,TEXT("RESULT=NPC_INTERACTION_FAILED reason=fresh_evidence_required")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        bRendered=!FParse::Param(Args,TEXT("nullrhi")); bOffscreen=FParse::Param(Args,TEXT("RenderOffScreen"));
        IFileManager::Get().MakeDirectory(*Output,true); if (bRendered) { IFileManager::Get().MakeDirectory(*Images,true); }
        Events=TEXT("frame,stage,world_s,dialogue_open,prompt_visible,settings_visible,selected_skill_uses,stamina,mana,identity,cooldown_s,distance_to_npc_cm,probe_active\n");
        Deadline=FPlatformTime::Seconds()+200.;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds()>Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds()>=Self->NextAt) { auto Work=MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }),.02f);
        Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* W,bool,bool)
        { if (W==Self->World.Get()&&!Self->bFinished) { Self->Check(false,TEXT("world_cleanup_before_completion")); Self->Finish(false); } });
        Later(3.,[this]{Prepare();});
    }
private:
    void Check(bool bOK,const TCHAR* Reason)
    { ++Checks; if (!bOK) { ++Failures; Reasons.Add(Reason); UE_LOG(LogRogue10m,Error,TEXT("NPC_INTERACTION FAIL %s"),Reason); } }
    void Later(double Wait,TFunction<void()> Work) { NextAt=FPlatformTime::Seconds()+Wait; Next=MoveTemp(Work); }
    void Key(FKey K,bool bDown) { if (PC.IsValid()) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,bDown?IE_Pressed:IE_Released,bDown?1.f:0.f)); } }
    void Press(FKey K,TFunction<void()> After)
    { Key(K,true); Later(.08,[this,K,After=MoveTemp(After)]() mutable {Key(K,false); Later(.3,MoveTemp(After));}); }
    void PhotoThen(const TCHAR* Name,TFunction<void()> After)
    {
        if (bRendered)
        {
            const FString File=Images/Name;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float){FScreenshotRequest::RequestScreenshot(File,true,false);return false;}),.1f);
        }
        Later(.85,MoveTemp(After));
    }
    bool PromptVisible() const
    { const auto* W=Npc.IsValid()?Npc->GetTutorialWidget():nullptr;return W&&W->IsInViewport()&&W->IsVisible(); }
    FString ActionText() const
    { const auto* W=Npc.IsValid()?Npc->GetTutorialWidget():nullptr;const auto* Text=W?Cast<UTextBlock>(W->GetWidgetFromName(TEXT("TutorialAction"))):nullptr;return Text?Text->GetText().ToString():FString(); }
    FVector Resources() const
    { const auto* A=Character->GetRogueAttributeSet();return FVector(A->GetStamina(),A->GetMana(),A->GetIdentity()); }
    int32 Uses() const { return Skill.IsValid()?State->GetSkillUseCount(Skill->GetFName()):0; }
    bool Idle() const
    { return Character.IsValid()&&Character->CanChangeWeapon()&&Combat->GetAttackCooldownRemaining()<=.001f&&!Combat->IsComboSequenceActive(World->GetTimeSeconds()); }
    void WaitIdle(TFunction<void()> After)
    {
        IdleDeadline=FPlatformTime::Seconds()+15.; IdleNext=MoveTemp(After); PollIdle();
    }
    void PollIdle()
    {
        if (!Idle()&&FPlatformTime::Seconds()<IdleDeadline) {Later(.1,[this]{PollIdle();});return;}
        Check(Idle(),TEXT("action_cooldown_and_combo_complete_before_next_probe"));
        if (!Idle()) { Finish();return; } auto Work=MoveTemp(IdleNext); Later(.2,MoveTemp(Work));
    }
    void MoveTo(const FVector& At)
    { Character->GetCharacterMovement()->StopMovementImmediately();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(InitialControl); }
    void Prepare()
    {
        PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());auto* GI=World->GetGameInstance();
        auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
        Check(PC.IsValid()&&Profiles&&Saves,TEXT("real_profile_controller_and_progression_available"));if(Failures){Finish();return;}
        Check(Profiles->GetProfileCount()==0,TEXT("fresh_isolated_profile_has_no_characters"));FRogue10mCharacterProfile NewProfile;
        Check(!Failures&&Profiles->CreateProfile(TEXT("NpcQa"),FRogue10mCharacterAppearance(),NewProfile)&&Profiles->SelectProfile(NewProfile.ProfileId),TEXT("normal_QA_profile_created_and_selected"));if(Failures){Finish();return;}
        PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());
        State=Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;Combat=Character.IsValid()?Character->GetCombatComponent():nullptr;Bindings=PC->GetSkillBindingComponent();
        Check(Character.IsValid()&&State.IsValid()&&Combat.IsValid()&&Bindings.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_has_real_pawn_combat_and_bindings"));if(Failures){Finish();return;}
        Saves->DetachCharacter(*Character,false);Check(!Saves->IsAttachedTo(*Character),TEXT("autosave_detached_before_transient_fixture"));
        OriginalActor=Character->GetActorTransform();InitialControl=PC->GetControlRotation();bOriginalDamage=Character->CanBeDamaged();bPrepared=true;Character->SetCanBeDamaged(false);Character->GetRogueAttributeSet()->RestoreVitals();
        FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;Ground=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);
        Check(Ground.IsValid(),TEXT("owned_transient_collision_floor_spawned"));if(Failures){Finish();return;}
        auto* Box=NewObject<UBoxComponent>(Ground.Get(),TEXT("NpcQaFloor"),RF_Transient);Ground->AddInstanceComponent(Box);Ground->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10000,10000,20));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
        const FVector Original=OriginalActor.GetLocation();const double Surface=Original.Z+2000.;Ground->SetActorLocation(FVector(Original.X,Original.Y,Surface-20.));
        Near=FVector(Original.X,Original.Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.);
        Far=Near-Character->GetActorForwardVector()*1200.;MoveTo(Near);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Character->GetCharacterMovement()->bForceNextFloorCheck=true;
        FVector NpcAt=Near+Character->GetActorForwardVector()*200.;NpcAt.Z=Surface;
        Npc=World->SpawnActor<ARogue10mTutorialNPC>(ARogue10mTutorialNPC::StaticClass(),FTransform(FRotator(0,InitialControl.Yaw+180.f,0),NpcAt),P);
        Check(Npc.IsValid(),TEXT("owned_transient_native_NPC_spawned_without_modifying_map_assets"));if(Failures){Finish();return;}
        SampleHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float){if(W==Self->World.Get()){Self->Sample();}});
        Later(.8,[this]{CheckGround();});
    }
    void CheckGround()
    {
        Check(Character->GetCharacterMovement()->IsMovingOnGround()&&Character->GetMovementBaseObject()==Ground->GetRootComponent(),TEXT("actual_pawn_grounded_on_owned_fixture_floor"));
        if(Failures){Finish();return;}
        Stage=TEXT("nearby_prompt");Check(!Npc->IsDialogueOpen()&&PromptVisible()&&ActionText().Contains(TEXT("F"))&&ActionText().Contains(TEXT("대화")),TEXT("actual_proximity_widget_displays_F_dialogue_prompt"));
        Prompt=ActionText();
        PhotoThen(TEXT("01_f_prompt.png"),[this]{Press(EKeys::F,[this]{Opened();});});
    }
    void Opened()
    {
        Check(Npc->IsDialogueOpen(),TEXT("actual_F_InputStack_opens_NPC_dialogue"));
        Check(PromptVisible()&&Npc->GetTutorialWidget()&&!Npc->GetTutorialWidget()->GetDisplayedServices().IsEmpty(),TEXT("real_dialogue_widget_contains_town_services"));
        Stage=TEXT("dialogue_open");PhotoThen(TEXT("02_f_dialogue.png"),[this]
        {
            Press(EKeys::F,[this]
            {
                Check(!Npc->IsDialogueOpen(),TEXT("F_closes_native_NPC_without_starting_item"));
                Press(EKeys::F,[this]
                {
                    Check(Npc->IsDialogueOpen(),TEXT("F_reopens_dialogue_for_service_Escape_probe"));
                    Press(EKeys::Escape,[this]{AfterEscape();});
                });
            });
        });
    }
    void AfterEscape()
    {
        Check(!Npc->IsDialogueOpen()&&!PC->IsSettingsVisible(),TEXT("service_Escape_closes_dialogue_and_is_consumed_before_settings_toggle"));
        Stage=TEXT("settings_blocks_dialogue");PC->ToggleSettings();
        Later(.3,[this]{Press(EKeys::F,[this]
        {
            Check(PC->IsSettingsVisible()&&!Npc->IsDialogueOpen()&&!PromptVisible(),TEXT("blocking_settings_hide_prompt_and_reject_F_dialogue"));
            PC->CloseAllBlockingPanels();MoveTo(Far);Stage=TEXT("out_of_range");Later(.4,[this]{Press(EKeys::F,[this]{Outside();});});
        });});
    }
    void Outside()
    {
        Check(!Npc->IsDialogueOpen()&&!PromptVisible(),TEXT("outside_NPC_range_has_no_prompt_or_F_dialogue"));
        for(auto* Candidate:Combat->GetActiveSkillTreeSkills())
        {
            bool bHasPositiveCost=false;if(Candidate){for(const auto& Cost:Candidate->ResourceCosts){bHasPositiveCost|=FMath::IsFinite(Cost.Cost)&&Cost.Cost>0.f;}}
            if(!Candidate){continue;}
            const bool bAir=Candidate->InputSlot==ERogue10mAttackInputSlot::JumpPrimary||Candidate->InputSlot==ERogue10mAttackInputSlot::JumpSpecial;
            const bool bCharged=Candidate->InputSlot==ERogue10mAttackInputSlot::ChargedPrimary||Candidate->InputSlot==ERogue10mAttackInputSlot::ChargedSpecial;
            auto CandidateRow=MakeShared<FJsonObject>();CandidateRow->SetStringField(TEXT("path"),Candidate->GetPathName());CandidateRow->SetStringField(TEXT("input_slot"),UEnum::GetValueAsString(Candidate->InputSlot));CandidateRow->SetStringField(TEXT("node_type"),UEnum::GetValueAsString(Candidate->SkillTreeNodeType));CandidateRow->SetBoolField(TEXT("has_montage"),Candidate->AttackMontage!=nullptr);CandidateRow->SetBoolField(TEXT("has_positive_cost"),bHasPositiveCost);CandidateRow->SetNumberField(TEXT("authored_cooldown_s"),Candidate->AttackCooldown);
            TArray<TSharedPtr<FJsonValue>> CandidateCosts;for(const auto& Cost:Candidate->ResourceCosts){auto CostRow=MakeShared<FJsonObject>();CostRow->SetStringField(TEXT("resource"),UEnum::GetValueAsString(Cost.ResourceType));CostRow->SetNumberField(TEXT("cost"),Cost.Cost);CandidateCosts.Add(MakeShared<FJsonValueObject>(CostRow));}CandidateRow->SetArrayField(TEXT("costs"),CandidateCosts);Candidates.Add(MakeShared<FJsonValueObject>(CandidateRow));
            // Bound Special is a supported immediate grounded action. The failed B
            // fixture incorrectly required Primary, whose existing attacks are free.
            if(Candidate->AttackMontage&&bHasPositiveCost&&!bAir&&!bCharged&&Candidate->SkillTreeNodeType==ERogue10mSkillTreeNodeType::ActiveTechnique)
            {if(!Skill.IsValid()||Candidate->GetName()==TEXT("DA_Attack_Knuckle_Special")){Skill=Candidate;}}
        }
        Check(Skill.IsValid(),TEXT("existing_costed_active_grounded_noncharged_available_for_F_mapping"));if(!Skill.IsValid()){Finish();return;}
        if(!Combat->IsAttackSkillUnlocked(Skill.Get())) {TestUnlockCount=Combat->UnlockAllActiveTreeForTesting();}
        Check(Combat->IsAttackSkillUnlocked(Skill.Get()),TEXT("selected_existing_skill_unlocked_in_detached_QA_pawn_only"));
        OldBinding=Bindings->GetSkillKey(Skill.Get());FText Error;bFBindingAccepted=Bindings->SetSkillKey(Skill.Get(),EKeys::F,Error);FBindingReason=Error.ToString();
        PositiveKey=EKeys::F;
        if(!bFBindingAccepted)
        {
            Check(Bindings->GetSkillKey(Skill.Get())==OldBinding,TEXT("rejected_F_binding_preserves_previous_key"));
        }
        Check(bFBindingAccepted,TEXT("F_remapping_required_for_this_strict_collision_fixture_no_fallback"));
        Check(Bindings->GetSkillKey(Skill.Get())==PositiveKey,TEXT("actual_binding_matches_positive_control_key"));
        if(Failures){Finish();return;}Key(EKeys::F,false);Key(EKeys::F7,false);
        Later(.5,[this]{WaitIdle([this]{Positive();});});
    }
    void StartProbe(const TCHAR* Name)
    {
        Character->GetRogueAttributeSet()->RestoreVitals();ProbeName=Name;BeforeUses=Uses();BeforeResources=Resources();MinResources=BeforeResources;MaxCooldown=Combat->GetAttackCooldownRemaining();bSawDialogue=false;bSawChargeHeld=false;bObserve=true;
    }
    void SaveProbe()
    {
        bObserve=false;auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),ProbeName);P->SetNumberField(TEXT("uses_before"),BeforeUses);P->SetNumberField(TEXT("uses_after"),Uses());
        P->SetNumberField(TEXT("stamina_before"),BeforeResources.X);P->SetNumberField(TEXT("stamina_min"),MinResources.X);P->SetNumberField(TEXT("mana_before"),BeforeResources.Y);P->SetNumberField(TEXT("mana_min"),MinResources.Y);P->SetNumberField(TEXT("identity_before"),BeforeResources.Z);P->SetNumberField(TEXT("identity_min"),MinResources.Z);P->SetNumberField(TEXT("max_cooldown_s"),MaxCooldown);P->SetBoolField(TEXT("saw_dialogue_open"),bSawDialogue);
        P->SetStringField(TEXT("last_accepted_skill"),GetPathNameSafe(Combat->GetLastAcceptedAttackSkill()));P->SetBoolField(TEXT("saw_charge_held"),bSawChargeHeld);Probes.Add(MakeShared<FJsonValueObject>(P));
    }
    void Positive()
    {
        Stage=TEXT("outside_positive_control");StartProbe(TEXT("outside_binding_positive_control"));
        Press(PositiveKey,[this]
        {
            Check(Uses()==BeforeUses+1&&Combat->GetLastAcceptedAttackSkill()==Skill.Get(),TEXT("outside_NPC_positive_control_key_accepts_exactly_one_real_selected_skill"));
            Check(MaxCooldown>.001,TEXT("positive_control_observer_sees_actual_attack_cooldown"));
            Check(MinResources.X<BeforeResources.X-.001||MinResources.Y<BeforeResources.Y-.001||MinResources.Z<BeforeResources.Z-.001,TEXT("positive_control_observer_sees_actual_costed_skill_resource_spend"));
            Check(!Npc->IsDialogueOpen(),TEXT("outside_positive_control_does_not_open_dialogue"));
            SaveProbe();WaitIdle([this]{MoveTo(Near);Stage=TEXT("bound_F_menu_probe_settle");Later(.5,[this]{MappedMenu();});});
        });
    }
    void MappedMenu()
    {
        Check(PromptVisible()&&!Npc->IsDialogueOpen(),TEXT("NPC_prompt_present_before_bound_F_menu_guard"));
        PC->ToggleSettings();Stage=TEXT("bound_F_blocking_menu");
        Later(.3,[this]
        {
            StartProbe(TEXT("bound_F_menu_must_not_cast_or_talk"));
            Press(EKeys::F,[this]
            {
                Check(PC->IsSettingsVisible()&&!Npc->IsDialogueOpen()&&!PromptVisible(),TEXT("settings_blocks_real_NPC_F_after_active_F_remapping"));
                Check(Uses()==BeforeUses&&MaxCooldown<=.001,TEXT("settings_blocks_Enhanced_F_attack_and_cooldown_after_remapping"));
                Check(!bSawChargeHeld&&!Combat->IsAttackChargeHeld(),TEXT("settings_does_not_start_or_leave_Enhanced_F_charge_held_after_remapping"));
                Check(MinResources.X>=BeforeResources.X-.001&&MinResources.Y>=BeforeResources.Y-.001&&MinResources.Z>=BeforeResources.Z-.001,TEXT("settings_blocks_Enhanced_F_resource_spend_after_remapping"));
                SaveProbe();PC->CloseAllBlockingPanels();Stage=TEXT("NPC_F_collision_probe_settle");Later(.4,[this]{NpcCollision();});
            });
        });
    }
    void NpcCollision()
    {
        Check(!Npc->IsDialogueOpen()&&PromptVisible()&&ActionText().Contains(TEXT("F")),TEXT("NPC_prompt_restored_before_collision_probe"));
        Check(Bindings->GetSkillKey(Skill.Get())==PositiveKey&&Idle(),TEXT("unchanged_binding_and_idle_cooldown_before_NPC_F_probe"));
        Stage=TEXT("NPC_F_collision_probe");StartProbe(TEXT("inside_NPC_F_must_not_cast"));
        Press(EKeys::F,[this]
        {
            bCollisionDetected=Uses()>BeforeUses;
            Check(bSawDialogue&&Npc->IsDialogueOpen(),TEXT("NPC_F_probe_executes_real_legacy_dialogue_path"));
            Check(Uses()==BeforeUses,TEXT("NPC_F_dialogue_does_not_also_execute_bound_active_skill"));
            Check(!bSawChargeHeld&&!Combat->IsAttackChargeHeld(),TEXT("NPC_F_dialogue_does_not_start_or_leave_bound_charge_held"));
            Check(MinResources.X>=BeforeResources.X-.001&&MinResources.Y>=BeforeResources.Y-.001&&MinResources.Z>=BeforeResources.Z-.001,TEXT("NPC_F_dialogue_does_not_spend_combat_resources"));
            Check(MaxCooldown<=.001,TEXT("NPC_F_dialogue_does_not_start_attack_cooldown"));
            SaveProbe();PhotoThen(TEXT("03_f_collision_probe.png"),[this]{Finish();});
        });
    }
    void Sample()
    {
        if(bFinished||!bPrepared||!Character.IsValid()||!Npc.IsValid()){return;}
        const FVector R=Resources();const float CD=Combat->GetAttackCooldownRemaining();
        if(bObserve){MinResources.X=FMath::Min(MinResources.X,R.X);MinResources.Y=FMath::Min(MinResources.Y,R.Y);MinResources.Z=FMath::Min(MinResources.Z,R.Z);MaxCooldown=FMath::Max(MaxCooldown,CD);bSawDialogue|=Npc->IsDialogueOpen();bSawChargeHeld|=Combat->IsAttackChargeHeld();}
        Events+=FString::Printf(TEXT("%d,%s,%.9f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.9f,%.6f,%d\n"),++Frame,*Stage,World->GetTimeSeconds(),Npc->IsDialogueOpen(),PromptVisible(),PC->IsSettingsVisible(),Uses(),R.X,R.Y,R.Z,CD,FVector::Distance(Character->GetActorLocation(),Npc->GetActorLocation()),bObserve);
    }
    void Finish(bool bExit=true)
    {
        if(bFinished){return;}bFinished=true;Next=nullptr;IdleNext=nullptr;bObserve=false;
        FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        Key(EKeys::F,false);Key(EKeys::F7,false);Key(EKeys::Escape,false);
        if(Npc.IsValid()){Npc->CloseDialogue();Npc->Destroy();}
        if(bPrepared&&Character.IsValid())
        {
            if(Saves&&Saves->IsAttachedTo(*Character)){Saves->DetachCharacter(*Character,false);}
            PC->CloseAllBlockingPanels();Combat->CancelBoundSkillInput();Combat->CancelCombatVisuals();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
            Character->SetCanBeDamaged(bOriginalDamage);Character->SetActorTransform(OriginalActor,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(InitialControl);
        }
        if(Ground.IsValid()){Ground->Destroy();}
        Check(FFileHelper::SaveStringToFile(Events,*(Output/TEXT("events.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM),TEXT("raw_state_events_written"));
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("label"),Label);J->SetBoolField(TEXT("rendered"),bRendered);J->SetBoolField(TEXT("offscreen"),bOffscreen);J->SetNumberField(TEXT("checks"),Checks);J->SetNumberField(TEXT("failures"),Failures);J->SetBoolField(TEXT("probe_passed"),Failures==0);
        J->SetStringField(TEXT("input"),TEXT("programmatic_Controller_InputKey_actual_legacy_and_enhanced_stack"));J->SetBoolField(TEXT("physical_input_performed"),false);J->SetBoolField(TEXT("manual_field_run"),false);J->SetBoolField(TEXT("f_binding_accepted"),bFBindingAccepted);J->SetBoolField(TEXT("collision_detected"),bCollisionDetected);J->SetStringField(TEXT("f_binding_reason"),FBindingReason);J->SetStringField(TEXT("positive_control_key"),PositiveKey.ToString());J->SetStringField(TEXT("selected_skill"),GetPathNameSafe(Skill.Get()));J->SetStringField(TEXT("prompt_text"),Prompt);J->SetNumberField(TEXT("test_unlock_count"),TestUnlockCount);J->SetArrayField(TEXT("probes"),Probes);
        J->SetArrayField(TEXT("loaded_candidates"),Candidates);
        J->SetStringField(TEXT("fixture_revision"),TEXT("grounded_noncharged_paid_special_preferred_after_B_primary_only_fixture_failure"));
        J->SetStringField(TEXT("evidence_note"),TEXT("Fresh exact FpNpc slots, normal entry then autosave detach. Owned transient native NPC/default no starting item and elevated collision floor, no map assets saved. F uses actual Controller InputKey/InputStack. Previous B failed before F mapping because the fixture incorrectly required a costed Primary; prior asset audits list free Primaries and B found no eligible Primary. This revision follows grounded noncharged CanUseBoundSkill input slots and prefers existing paid DA_Attack_Knuckle_Special, retaining mandatory actual positive resource spend/cooldown and no extra attack/charge checks. Loaded candidate metadata is recorded; no assets are altered. Existing costed active skill is QA-test-unlocked only if required. F remapping must be accepted; rejection fails explicitly with no alternate-key fallback. PNGs are actual viewport UI, not physical input or normal field completion."));
        TArray<TSharedPtr<FJsonValue>> R;for(const FString& S:Reasons){R.Add(MakeShared<FJsonValueString>(S));}J->SetArrayField(TEXT("failure_reasons"),R);
        FString Json;const auto Writer=TJsonWriterFactory<>::Create(&Json);const bool Saved=FJsonSerializer::Serialize(J,Writer)&&FFileHelper::SaveStringToFile(Json,*(Output/TEXT("results.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);if(!Saved){++Failures;}
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=NPC_INTERACTION_%s checks=%d failures=%d collision_detected=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,bCollisionDetected,*Output);
        if(bExit){FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;TWeakObjectPtr<URogue10mSkillBindingComponent> Bindings;TWeakObjectPtr<URogue10mAttackSkillData> Skill;TWeakObjectPtr<ARogue10mTutorialNPC> Npc;TWeakObjectPtr<AActor> Ground;
    URogue10mProgressionSubsystem* Saves=nullptr;FDelegateHandle SampleHandle,Cleanup;TFunction<void()> Next,IdleNext;TArray<TSharedPtr<FJsonValue>> Probes,Candidates;TArray<FString> Reasons;
    FString Label,Output,Images,Events,Stage=TEXT("setup"),Prompt,FBindingReason,ProbeName;FTransform OriginalActor;FRotator InitialControl;FVector Near,Far,BeforeResources,MinResources;
    FKey OldBinding,PositiveKey=EKeys::F;double Deadline=0,NextAt=0,IdleDeadline=0;float MaxCooldown=0;int32 Checks=0,Failures=0,Frame=0,BeforeUses=0,TestUnlockCount=0;
    bool bFinished=false,bPrepared=false,bRendered=false,bOffscreen=false,bOriginalDamage=true,bObserve=false,bSawDialogue=false,bSawChargeHeld=false,bFBindingAccepted=false,bCollisionDetected=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine||GIsEditor||(Active&&!Active->Finished())){return;}
    for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}}
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=NPC_INTERACTION_FAILED reason=no_standalone_world"));FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestNpcInteraction"),TEXT("Fresh exact FpNpc QA slots only: actual NPC F prompt/dialogue and legacy/Enhanced Input F collision, no physical input or map editing."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
