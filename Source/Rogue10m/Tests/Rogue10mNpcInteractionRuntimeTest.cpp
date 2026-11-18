// Opt-in isolated NPC/input QA. Programmatic Controller InputKey, never physical-input/full-run QA.
#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
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
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

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
    int32 TotalUses() const { int32 Total=0;TSet<FName> Seen;for(const auto* S:Combat->GetActiveSkillTreeSkills()){if(S&&!Seen.Contains(S->GetFName())){Seen.Add(S->GetFName());Total+=State->GetSkillUseCount(S->GetFName());}}return Total; }
    bool Idle() const
    { return Character.IsValid()&&Character->CanChangeWeapon()&&Combat->GetAttackCooldownRemaining()<=.001f&&!Combat->IsComboSequenceActive(World->GetTimeSeconds()); }
    void WaitIdle(TFunction<void()> After)
    {
        IdleDeadline=FPlatformTime::Seconds()+15.; bIdleOneSecondRecorded=false; IdleNext=MoveTemp(After); RecordIdleDiagnostic(TEXT("start")); PollIdle();
    }
    void PollIdle()
    {
        if(!bIdleOneSecondRecorded&&FPlatformTime::Seconds()>=IdleDeadline-14.) {bIdleOneSecondRecorded=true;RecordIdleDiagnostic(TEXT("after_one_second"));}
        if (!Idle()&&FPlatformTime::Seconds()<IdleDeadline) {Later(.1,[this]{PollIdle();});return;}
        if(!Idle()){RecordIdleDiagnostic(TEXT("timeout"));}
        Check(Idle(),TEXT("action_cooldown_and_combo_complete_before_next_probe"));
        if (!Idle()) { Finish();return; } auto Work=MoveTemp(IdleNext); Later(.2,MoveTemp(Work));
    }
    void RecordIdleDiagnostic(const TCHAR* Phase)
    {
        const auto* Movement=Character.IsValid()?Character->GetCharacterMovement():nullptr;
        const double Now=World.IsValid()?World->GetTimeSeconds():-1.;
        FString Diagnostic=FString::Printf(TEXT("phase=%s stage=%s world_s=%.9f movement_mode=%d grounded=%d can_change_weapon=%d weapon_change_blocked=%d combo_active=%d cooldown_s=%.9f charge_held=%d dead=%d\n"),
            Phase,*Stage,Now,Movement?static_cast<int32>(Movement->MovementMode):-1,Movement&&Movement->IsMovingOnGround(),
            Character.IsValid()&&Character->CanChangeWeapon(),Combat.IsValid()&&Combat->IsWeaponChangeBlocked(),
            Combat.IsValid()&&Combat->IsComboSequenceActive(Now),Combat.IsValid()?Combat->GetAttackCooldownRemaining():-1.f,
            Combat.IsValid()&&Combat->IsAttackChargeHeld(),Character.IsValid()&&Character->IsDead());
        auto AddMesh=[&Diagnostic](const TCHAR* Role,USkeletalMeshComponent* Mesh)
        {
            UAnimInstance* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
            UAnimMontage* Montage=Anim?Anim->GetCurrentActiveMontage():nullptr;
            Diagnostic+=FString::Printf(TEXT("role=%s mesh=%s anim=%s active_montage=%s position=%.9f play_rate=%.9f length=%.9f section=%s any_montage_playing=%d montage_is_playing=%d pause_anims=%d tick_enabled=%d visible=%d hidden_in_game=%d visibility_tick_option=%d\n"),
                Role,*GetPathNameSafe(Mesh),*GetPathNameSafe(Anim),*GetPathNameSafe(Montage),
                Anim&&Montage?Anim->Montage_GetPosition(Montage):-1.f,Anim&&Montage?Anim->Montage_GetPlayRate(Montage):0.f,
                Montage?Montage->GetPlayLength():0.f,Anim&&Montage?*Anim->Montage_GetCurrentSection(Montage).ToString():TEXT("None"),
                Anim&&Anim->IsAnyMontagePlaying(),Anim&&Montage&&Anim->Montage_IsPlaying(Montage),
                Mesh&&Mesh->bPauseAnims,Mesh&&Mesh->IsComponentTickEnabled(),Mesh&&Mesh->IsVisible(),Mesh&&Mesh->bHiddenInGame,
                Mesh?static_cast<int32>(Mesh->VisibilityBasedAnimTickOption):-1);
        };
        AddMesh(TEXT("playback"),Character.IsValid()?Character->GetAnimationPlaybackMesh():nullptr);
        AddMesh(TEXT("first_person"),Character.IsValid()?Character->GetFirstPersonMesh():nullptr);
        UE_LOG(LogRogue10m,Warning,TEXT("NPC_INTERACTION_IDLE_DIAGNOSTIC\n%s"),*Diagnostic);
        if(!FFileHelper::SaveStringToFile(Diagnostic,*(Output/TEXT("idle-diagnostics.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))
        {UE_LOG(LogRogue10m,Warning,TEXT("NPC_INTERACTION idle-diagnostics.txt could not be appended"));}
        if(FCString::Strcmp(Phase,TEXT("timeout"))==0)
        {
            if(!FFileHelper::SaveStringToFile(Diagnostic,*(Output/TEXT("idle-timeout.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            {UE_LOG(LogRogue10m,Warning,TEXT("NPC_INTERACTION idle-timeout.txt could not be written"));}
        }
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
        // Preserve the existing 39-check input/collision suite below. First exercise
        // a fresh outside -> physical overlap shell -> service-range approach.
        Later(.8,[this]{BeginOverlapEdge();});
    }
    void BeginOverlapEdge()
    {
        Detection=Npc->FindComponentByClass<USphereComponent>();
        Stage=TEXT("overlap_edge_outside_far");MoveTo(Far);
        Later(.4,[this]
        {
            bEdgeFarReleased=Detection.IsValid()&&!Detection->IsOverlappingActor(Character.Get())
                &&!Npc->GetTutorialWidget()&&!Npc->IsDialogueOpen();
            if(!bEdgeFarReleased){Check(false,TEXT("overlap_edge_outside_far_releases_proximity_context"));Finish();return;}
            const float Radius=Detection->GetScaledSphereRadius();
            const float CapsuleRadius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
            // A sphere touches the capsule before actor centers enter its radius.
            // Keep the character's normal grounded Z; change only this detached QA fixture.
            EdgeShell=Detection->GetComponentLocation()-Character->GetActorForwardVector()*(Radius+CapsuleRadius*.5f);
            EdgeShell.Z=Near.Z;Stage=TEXT("overlap_edge_shell_hold");MoveTo(EdgeShell);
            Later(.4,[this]{CheckOverlapShell();});
        });
    }
    void CheckOverlapShell()
    {
        EdgeRadius=Detection->GetScaledSphereRadius();
        EdgeCenterDistance=FVector::Distance(Character->GetActorLocation(),Npc->GetActorLocation());
        bEdgePhysicalOverlap=Detection->IsOverlappingActor(Character.Get());
        Check(bEdgeFarReleased&&bEdgePhysicalOverlap&&EdgeCenterDistance>EdgeRadius
            &&!Npc->IsWithinSkillTreeServiceRange(Character.Get()),
            TEXT("overlap_edge_capsule_overlaps_while_actor_center_is_outside_detection_radius"));
        EdgeWidget=Npc->GetTutorialWidget();
        Check(EdgeWidget.IsValid()&&!PromptVisible()&&!Npc->IsDialogueOpen(),
            TEXT("overlap_edge_outer_shell_retains_context_without_widening_service_prompt"));
        if(Failures){Finish();return;}
        Press(EKeys::F,[this]
        {
            Check(!Npc->IsDialogueOpen()&&!PromptVisible(),
                TEXT("overlap_edge_outer_shell_F_still_rejects_dialogue_outside_service_range"));
            if(Failures){Finish();return;}
            // Both endpoints remain in the same physical overlap. No second
            // BeginOverlap or direct BeginDialogue call should be needed.
            Stage=TEXT("overlap_edge_move_inside_service");MoveTo(Near);
            Later(.4,[this]
            {
                bEdgeSameContextRestored=Detection->IsOverlappingActor(Character.Get())
                    &&Npc->GetTutorialWidget()==EdgeWidget.Get()
                    &&Npc->IsWithinSkillTreeServiceRange(Character.Get())
                    &&PromptVisible()&&ActionText().Contains(TEXT("F"))&&!Npc->IsDialogueOpen();
                Check(bEdgeSameContextRestored,
                    TEXT("overlap_edge_same_context_recovers_F_prompt_on_moving_inside_service_range"));
                if(Failures){Finish();return;}
                Stage=TEXT("overlap_edge_complete_exit");MoveTo(Far);
                Later(.4,[this]
                {
                    bEdgeReleasedAgain=!Detection->IsOverlappingActor(Character.Get())&&!Npc->GetTutorialWidget()&&!Npc->IsDialogueOpen();
                    Stage=TEXT("overlap_edge_reenter");MoveTo(Near);
                    Later(.4,[this]
                    {
                        int32 VisibleWidgets=0;
                        for(TObjectIterator<URogue10mTutorialWidget> It;It;++It)
                        {if(It->GetWorld()==World.Get()&&It->GetOwningPlayer()==PC.Get()&&It->IsInViewport()&&It->IsVisible()){++VisibleWidgets;}}
                        bEdgeSingleWidget=bEdgeReleasedAgain&&Detection->IsOverlappingActor(Character.Get())
                            &&VisibleWidgets==1&&PromptVisible()&&Npc->GetTutorialWidget()!=EdgeWidget.Get();
                        Press(EKeys::F,[this]
                        {
                            bEdgeSingleFOpened=Npc->IsDialogueOpen();
                            Press(EKeys::F,[this]
                            {
                                Check(bEdgeSingleWidget&&bEdgeSingleFOpened&&!Npc->IsDialogueOpen()&&PromptVisible(),
                                    TEXT("overlap_edge_full_exit_reentry_has_one_widget_and_single_F_open_close"));
                                if(Failures){Finish();return;}
                                CheckGround(); // The existing 39-check suite continues unchanged.
                            });
                        });
                    });
                });
            });
        });
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
        auto* Inventory=Character->GetInventoryComponent();int32 Bag=INDEX_NONE;FGuid Id;
        if(Inventory){for(int32 I=0;I<Inventory->GetInventoryContainers().Num()&&Bag==INDEX_NONE;++I)
        {for(const auto& Entry:Inventory->GetInventoryContainers()[I].Entries)
        {if(Entry.ItemData&&Entry.ItemData->WeaponType==ERogue10mWeaponType::Knuckle&&Entry.ItemData->EquipSlotType==ERogue10mInventorySlotType::MainWeapon){Bag=I;Id=Entry.InstanceId;break;}}}}
        Check(Inventory&&Bag!=INDEX_NONE&&Inventory->TryEquipGridItem(Bag,Id)&&Character->GetEquippedWeaponType()==ERogue10mWeaponType::Knuckle,TEXT("QA_existing_knuckle_equipped_through_real_inventory"));
        if(Failures){Finish();return;}
        TestUnlockCount=Combat->UnlockAllActiveTreeForTesting();
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
            if(Candidate->AttackMontage&&bHasPositiveCost&&!bAir&&!bCharged&&!Candidate->RequiresLoadoutCharge()&&Candidate->AttackCooldown>.001f&&Combat->CanEquipSkillInSlot(Candidate,ERogue10mAttackInputSlot::QSkill)&&Candidate->SkillTreeNodeType==ERogue10mSkillTreeNodeType::ActiveTechnique)
            {if(!Skill.IsValid()||Candidate->GetName()==TEXT("DA_Attack_Knuckle_Special")){Skill=Candidate;}}
        }
        Check(Skill.IsValid(),TEXT("existing_costed_active_grounded_noncharged_available_for_common_Q_mapping"));if(!Skill.IsValid()){Finish();return;}
        if(!Combat->IsAttackSkillUnlocked(Skill.Get())) {TestUnlockCount=Combat->UnlockAllActiveTreeForTesting();}
        Check(Combat->IsAttackSkillUnlocked(Skill.Get()),TEXT("selected_existing_skill_unlocked_in_detached_QA_pawn_only"));
        FText Error;bQBindingAccepted=Bindings->SetSkillKey(Skill.Get(),EKeys::Q,Error);PositiveKey=EKeys::Q;
        Check(bQBindingAccepted&&Bindings->GetSkillKey(Skill.Get())==PositiveKey&&Combat->GetEquippedSkill(ERogue10mAttackInputSlot::QSkill)==Skill.Get(),TEXT("actual_common_Q_binding_accepts_selected_paid_skill"));
        OldBinding=Bindings->GetSkillKey(Skill.Get());
        const auto BeforeRejectedBinding=Combat->CaptureCombat();
        bFBindingAccepted=Bindings->SetSkillKey(Skill.Get(),EKeys::F,Error);FBindingReason=Error.ToString();
        bFBindingPreserved=!bFBindingAccepted&&Bindings->GetSkillKey(Skill.Get())==OldBinding;
        const auto AfterRejectedBinding=Combat->CaptureCombat();
        bFBindingPreserved=bFBindingPreserved&&FRogue10mCombatSnapshot::StaticStruct()->CompareScriptStruct(&BeforeRejectedBinding,&AfterRejectedBinding,0);
        Check(!bFBindingAccepted&&!FBindingReason.IsEmpty(),TEXT("reserved_F_binding_rejected_by_current_slot_policy"));
        Check(bFBindingPreserved,TEXT("rejected_F_binding_preserves_complete_existing_Q_loadout"));
        if(Failures){Finish();return;}Key(EKeys::F,false);Key(EKeys::Q,false);
        Later(.5,[this]{WaitIdle([this]{FixedKeys(0);});});
    }
    void StartProbe(const TCHAR* Name)
    {
        Character->GetRogueAttributeSet()->RestoreVitals();ProbeStartWorld=World->GetTimeSeconds();BeforeAcceptedSkill=GetPathNameSafe(Combat->GetLastAcceptedAttackSkill());SprintRate=0;SprintInterval=0;SprintDuration=0;SprintAllowance=0;ProbeName=Name;BeforeUses=Uses();BeforeTotalUses=TotalUses();BeforeResources=Resources();MinResources=BeforeResources;MaxCooldown=Combat->GetAttackCooldownRemaining();bSawDialogue=false;bSawChargeHeld=false;bPlaybackObserved=false;bFrozenFpMontageObserved=false;bBusyChangeViolation=false;bObserve=true;
    }
    void SaveProbe()
    {
        bObserve=false;auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),ProbeName);P->SetNumberField(TEXT("uses_before"),BeforeUses);P->SetNumberField(TEXT("uses_after"),Uses());P->SetNumberField(TEXT("total_uses_before"),BeforeTotalUses);P->SetNumberField(TEXT("total_uses_after"),TotalUses());
        P->SetNumberField(TEXT("stamina_before"),BeforeResources.X);P->SetNumberField(TEXT("stamina_min"),MinResources.X);P->SetNumberField(TEXT("mana_before"),BeforeResources.Y);P->SetNumberField(TEXT("mana_min"),MinResources.Y);P->SetNumberField(TEXT("identity_before"),BeforeResources.Z);P->SetNumberField(TEXT("identity_min"),MinResources.Z);P->SetNumberField(TEXT("max_cooldown_s"),MaxCooldown);P->SetBoolField(TEXT("saw_dialogue_open"),bSawDialogue);
        P->SetStringField(TEXT("accepted_skill_before"),BeforeAcceptedSkill);P->SetNumberField(TEXT("sprint_cost_per_second"),SprintRate);P->SetNumberField(TEXT("sprint_tick_interval_s"),SprintInterval);P->SetNumberField(TEXT("sprint_observed_duration_s"),SprintDuration);P->SetNumberField(TEXT("allowed_sprint_stamina_drain"),SprintAllowance);P->SetStringField(TEXT("last_accepted_skill"),GetPathNameSafe(Combat->GetLastAcceptedAttackSkill()));P->SetBoolField(TEXT("saw_charge_held"),bSawChargeHeld);P->SetBoolField(TEXT("selected_body_montage_observed"),bPlaybackObserved);P->SetBoolField(TEXT("inactive_FP_montage_observed"),bFrozenFpMontageObserved);P->SetBoolField(TEXT("busy_weapon_change_violation"),bBusyChangeViolation);Probes.Add(MakeShared<FJsonValueObject>(P));
    }
    FKey FixedKey(int32 Index) const { return Index==0?EKeys::Q:Index==1?EKeys::E:EKeys::R; }
    void FixedKeys(int32 Index)
    {
        if(Index==3)
        {
            FText Error;Check(Bindings->SetSkillKey(Skill.Get(),EKeys::Q,Error)&&Bindings->GetSkillKey(Skill.Get())==EKeys::Q,TEXT("Q_restored_after_fixed_slot_matrix"));
            if(Failures){Finish();return;}Later(.2,[this]{WaitIdle([this]{Positive();});});return;
        }
        const FKey K=FixedKey(Index);FText Error;
        Check(Bindings->SetSkillKey(Skill.Get(),K,Error)&&Bindings->GetSkillKey(Skill.Get())==K,*FString::Printf(TEXT("fixed_%s_slot_assigned"),*K.ToString()));
        if(Failures){Finish();return;}
        Later(.2,[this,Index,K]
        {
            WaitIdle([this,Index,K]
            {
                Key(EKeys::LeftShift,true);const int32 ModifierFrame=Frame;
                Later(.15,[this,Index,K,ModifierFrame]
                {
                    Check(Frame>ModifierFrame&&PC->IsInputKeyDown(EKeys::LeftShift),TEXT("LeftShift_observed_down_across_world_frame"));
                    Stage=TEXT("fixed_modifier_guard");StartProbe(*FString::Printf(TEXT("fixed_%s_LeftShift_blocked"),*K.ToString()));
                    Press(K,[this,Index,K]
                    {
                        Check(PC->IsInputKeyDown(EKeys::LeftShift)&&NoCombatWithSprint(),TEXT("LeftShift_blocks_fixed_slot_combat"));SaveProbe();Key(EKeys::LeftShift,false);
                        Later(.15,[this,Index,K]
                        {
                            Check(!PC->IsInputKeyDown(EKeys::LeftShift),TEXT("LeftShift_released_before_bare_positive_control"));
                            Stage=TEXT("fixed_bare_positive");StartProbe(*FString::Printf(TEXT("fixed_%s_bare_positive"),*K.ToString()));
                            Press(K,[this,Index]
                            {
                                Check(Uses()==BeforeUses+1&&TotalUses()==BeforeTotalUses+1&&Combat->GetLastAcceptedAttackSkill()==Skill.Get()
                                    &&MaxCooldown>.001f&&(MinResources.X<BeforeResources.X-.001||MinResources.Y<BeforeResources.Y-.001||MinResources.Z<BeforeResources.Z-.001),
                                    TEXT("bare_fixed_slot_after_modifier_executes_exactly_one_paid_skill"));
                                Check(Character->IsThirdPersonGameplayActive()&&bPlaybackObserved&&!bFrozenFpMontageObserved,
                                    TEXT("fixed_slot_plays_selected_body_montage_without_inactive_FP_montage"));
                                Check(!bBusyChangeViolation,TEXT("body_or_combat_busy_rejects_weapon_change"));
                                SaveProbe();if(Failures){Finish();return;}
                                WaitIdle([this,Index]
                                {
                                    const auto* Body=Character->GetAnimationPlaybackMesh();const auto* BodyAnim=Body?Body->GetAnimInstance():nullptr;
                                    const auto* FP=Character->GetFirstPersonMesh();const auto* FPAnim=FP?FP->GetAnimInstance():nullptr;
                                    const bool Recovered=Character->CanChangeWeapon()&&!Combat->IsWeaponChangeBlocked()
                                        &&(!BodyAnim||!BodyAnim->IsAnyMontagePlaying())&&(!FPAnim||!FPAnim->IsAnyMontagePlaying());
                                    Check(Recovered,TEXT("fixed_slot_body_and_combat_finish_and_weapon_change_recovers"));
                                    if(!Probes.IsEmpty()){Probes.Last()->AsObject()->SetBoolField(TEXT("body_finished_weapon_change_recovered"),Recovered);}
                                    if(Failures){Finish();return;}FixedKeys(Index+1);
                                });
                            });
                        });
                    });
                });
            });
        });
    }
    void MenuKeys(int32 Index)
    {
        if(Index==3)
        {
            FText Error;Check(Bindings->SetSkillKey(Skill.Get(),EKeys::Q,Error)&&Bindings->GetSkillKey(Skill.Get())==EKeys::Q,TEXT("Q_restored_after_menu_release_matrix"));
            if(Failures){Finish();return;}Later(.3,[this]{MappedMenu();});return;
        }
        const FKey K=FixedKey(Index);FText Error;
        Check(Bindings->SetSkillKey(Skill.Get(),K,Error)&&Bindings->GetSkillKey(Skill.Get())==K,TEXT("menu_matrix_fixed_slot_assigned"));
        PC->ToggleSettings();Later(.2,[this,Index,K]
        {
            Stage=TEXT("fixed_menu_block");StartProbe(*FString::Printf(TEXT("fixed_%s_menu_blocked"),*K.ToString()));
            Press(K,[this,Index,K]
            {
                Check(PC->IsSettingsVisible()&&NoCombat(),TEXT("menu_blocks_fixed_slot_press_and_release"));SaveProbe();
                StartProbe(*FString::Printf(TEXT("fixed_%s_menu_close_release_only"),*K.ToString()));Key(K,true);
                Later(.15,[this,Index,K]
                {
                    Check(PC->IsSettingsVisible()&&NoCombat(),TEXT("menu_held_fixed_key_does_not_cast"));PC->CloseAllBlockingPanels();
                    Later(.15,[this,Index,K]
                    {
                        Key(K,false);Later(.3,[this,Index]
                        {
                            Check(!PC->IsAnyBlockingWindowVisible()&&NoCombat(),TEXT("menu_close_then_release_only_does_not_execute_skill"));SaveProbe();
                            if(Failures){Finish();return;}MenuKeys(Index+1);
                        });
                    });
                });
            });
        });
    }
    void Positive()
    {
        Stage=TEXT("outside_positive_control");StartProbe(TEXT("outside_binding_positive_control"));
        Press(PositiveKey,[this]
        {
            Check(Uses()==BeforeUses+1&&TotalUses()==BeforeTotalUses+1&&Combat->GetLastAcceptedAttackSkill()==Skill.Get(),TEXT("outside_NPC_positive_control_key_accepts_exactly_one_real_selected_skill"));
            Check(MaxCooldown>.001,TEXT("positive_control_observer_sees_actual_attack_cooldown"));
            Check(MinResources.X<BeforeResources.X-.001||MinResources.Y<BeforeResources.Y-.001||MinResources.Z<BeforeResources.Z-.001,TEXT("positive_control_observer_sees_actual_costed_skill_resource_spend"));
            Check(!Npc->IsDialogueOpen(),TEXT("outside_positive_control_does_not_open_dialogue"));
            SaveProbe();WaitIdle([this]{MoveTo(Near);Stage=TEXT("common_Q_menu_probe_settle");Later(.5,[this]{MenuKeys(0);});});
        });
    }
    bool NoCombatWithSprint()
    {
        const auto* RateProperty=FindFProperty<FFloatProperty>(Character->GetClass(),TEXT("SprintStaminaCostPerSecond"));
        const auto* IntervalProperty=FindFProperty<FFloatProperty>(Character->GetClass(),TEXT("SprintStaminaDrainInterval"));
        if(!RateProperty||!IntervalProperty){return false;}
        SprintRate=RateProperty->GetPropertyValue_InContainer(Character.Get());
        SprintInterval=IntervalProperty->GetPropertyValue_InContainer(Character.Get());
        SprintDuration=World->GetTimeSeconds()-ProbeStartWorld;
        if(!FMath::IsFinite(SprintRate)||SprintRate<0||!FMath::IsFinite(SprintInterval)||SprintInterval<=0
            ||!FMath::IsFinite(SprintDuration)||SprintDuration<0){return false;}
        // An already-running sprint timer may tick immediately after StartProbe.
        // Permit only elapsed-time drain plus one authored timer tick, never an attack cost.
        SprintAllowance=SprintRate*(SprintDuration+SprintInterval);
        const FVector Now=Resources();
        return Uses()==BeforeUses&&TotalUses()==BeforeTotalUses
            &&GetPathNameSafe(Combat->GetLastAcceptedAttackSkill())==BeforeAcceptedSkill
            &&MaxCooldown<=.001f&&Combat->GetAttackCooldownRemaining()<=.001f
            &&!bSawChargeHeld&&!Combat->IsAttackChargeHeld()
            &&Now.X>=BeforeResources.X-SprintAllowance-.001&&MinResources.X>=BeforeResources.X-SprintAllowance-.001
            &&Now.Y>=BeforeResources.Y-.001&&Now.Z>=BeforeResources.Z-.001
            &&MinResources.Y>=BeforeResources.Y-.001&&MinResources.Z>=BeforeResources.Z-.001;
    }
    bool NoCombat() const
    {
        const FVector Now=Resources();
        return Uses()==BeforeUses&&TotalUses()==BeforeTotalUses&&MaxCooldown<=.001f&&Combat->GetAttackCooldownRemaining()<=.001f
            &&!bSawChargeHeld&&!Combat->IsAttackChargeHeld()&&Now.X>=BeforeResources.X-.001&&Now.Y>=BeforeResources.Y-.001&&Now.Z>=BeforeResources.Z-.001
            &&MinResources.X>=BeforeResources.X-.001&&MinResources.Y>=BeforeResources.Y-.001&&MinResources.Z>=BeforeResources.Z-.001;
    }
    void MappedMenu()
    {
        Check(PromptVisible()&&!Npc->IsDialogueOpen(),TEXT("NPC_prompt_present_before_common_Q_menu_guard"));
        PC->ToggleSettings();Stage=TEXT("common_Q_blocking_menu");
        Later(.3,[this]
        {
            StartProbe(TEXT("common_Q_menu_must_not_cast"));
            Press(EKeys::Q,[this]
            {
                Check(PC->IsSettingsVisible()&&!Npc->IsDialogueOpen()&&!PromptVisible()&&NoCombat(),TEXT("settings_blocks_real_common_Q_without_resource_cooldown_or_skill_use"));
                SaveProbe();StartProbe(TEXT("reserved_F_menu_must_not_cast_or_talk"));
                Press(EKeys::F,[this]
                {
                    Check(PC->IsSettingsVisible()&&!Npc->IsDialogueOpen()&&!PromptVisible()&&NoCombat(),TEXT("settings_blocks_reserved_F_dialogue_and_combat"));
                    SaveProbe();PC->CloseAllBlockingPanels();Stage=TEXT("NPC_F_dialogue_probe_settle");Later(.4,[this]{NpcCollision();});
                });
            });
        });
    }
    void DialogueQ()
    {
        Stage=TEXT("common_Q_dialogue_guard");StartProbe(TEXT("common_Q_dialogue_must_not_cast"));
        Press(EKeys::Q,[this]
        {
            Check(Npc->IsDialogueOpen()&&PC->IsNpcDialogueVisible()&&NoCombat(),TEXT("dialogue_blocks_real_common_Q_without_resource_cooldown_or_skill_use"));
            SaveProbe();StartProbe(TEXT("reserved_F_closes_without_attack"));
            Press(EKeys::F,[this]
            {
                Check(!Npc->IsDialogueOpen()&&NoCombat(),TEXT("reserved_F_closes_dialogue_without_extra_attack"));
                SaveProbe();PhotoThen(TEXT("03_f_collision_probe.png"),[this]{Finish();});
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
            Check(Uses()==BeforeUses,TEXT("NPC_F_dialogue_does_not_execute_selected_Q_skill"));
            Check(!bSawChargeHeld&&!Combat->IsAttackChargeHeld(),TEXT("NPC_F_dialogue_does_not_start_or_leave_bound_charge_held"));
            Check(MinResources.X>=BeforeResources.X-.001&&MinResources.Y>=BeforeResources.Y-.001&&MinResources.Z>=BeforeResources.Z-.001,TEXT("NPC_F_dialogue_does_not_spend_combat_resources"));
            Check(MaxCooldown<=.001,TEXT("NPC_F_dialogue_does_not_start_attack_cooldown"));
            Check(NoCombat(),TEXT("reserved_F_opens_dialogue_without_any_extra_skill"));SaveProbe();Later(.3,[this]{DialogueQ();});
        });
    }
    void Sample()
    {
        if(bFinished||!bPrepared||!Character.IsValid()||!Npc.IsValid()){return;}
        const FVector R=Resources();const float CD=Combat->GetAttackCooldownRemaining();
        if(bObserve){MinResources.X=FMath::Min(MinResources.X,R.X);MinResources.Y=FMath::Min(MinResources.Y,R.Y);MinResources.Z=FMath::Min(MinResources.Z,R.Z);MaxCooldown=FMath::Max(MaxCooldown,CD);bSawDialogue|=Npc->IsDialogueOpen();bSawChargeHeld|=Combat->IsAttackChargeHeld();}
        if(bObserve&&Character->IsThirdPersonGameplayActive())
        {
            const auto* Body=Character->GetAnimationPlaybackMesh();const auto* BodyAnim=Body?Body->GetAnimInstance():nullptr;
            const auto* FP=Character->GetFirstPersonMesh();const auto* FPAnim=FP?FP->GetAnimInstance():nullptr;
            bPlaybackObserved|=BodyAnim&&Skill.IsValid()&&BodyAnim->GetCurrentActiveMontage()==Skill->AttackMontage&&BodyAnim->IsAnyMontagePlaying();
            bFrozenFpMontageObserved|=FP&&FP!=Body&&(FP->bPauseAnims||!FP->IsComponentTickEnabled())&&FPAnim&&FPAnim->IsAnyMontagePlaying();
            bBusyChangeViolation|=((BodyAnim&&BodyAnim->IsAnyMontagePlaying())||Combat->IsWeaponChangeBlocked())&&Character->CanChangeWeapon();
        }
        Events+=FString::Printf(TEXT("%d,%s,%.9f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.9f,%.6f,%d\n"),++Frame,*Stage,World->GetTimeSeconds(),Npc->IsDialogueOpen(),PromptVisible(),PC->IsSettingsVisible(),Uses(),R.X,R.Y,R.Z,CD,FVector::Distance(Character->GetActorLocation(),Npc->GetActorLocation()),bObserve);
    }
    void Finish(bool bExit=true)
    {
        if(bFinished){return;}bFinished=true;Next=nullptr;IdleNext=nullptr;bObserve=false;
        FWorldDelegates::OnWorldPostActorTick.Remove(SampleHandle);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
        Key(EKeys::F,false);Key(EKeys::Q,false);Key(EKeys::E,false);Key(EKeys::R,false);Key(EKeys::LeftShift,false);Key(EKeys::Escape,false);
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
        J->SetStringField(TEXT("input"),TEXT("programmatic_Controller_InputKey_actual_legacy_and_enhanced_stack"));J->SetBoolField(TEXT("physical_input_performed"),false);J->SetBoolField(TEXT("manual_field_run"),false);J->SetBoolField(TEXT("f_binding_accepted"),bFBindingAccepted);J->SetBoolField(TEXT("q_binding_accepted"),bQBindingAccepted);J->SetBoolField(TEXT("f_rejection_preserved_bindings"),bFBindingPreserved);J->SetBoolField(TEXT("collision_detected"),bCollisionDetected);J->SetStringField(TEXT("f_binding_reason"),FBindingReason);J->SetStringField(TEXT("positive_control_key"),PositiveKey.ToString());J->SetStringField(TEXT("selected_skill"),GetPathNameSafe(Skill.Get()));J->SetStringField(TEXT("prompt_text"),Prompt);J->SetNumberField(TEXT("test_unlock_count"),TestUnlockCount);J->SetArrayField(TEXT("probes"),Probes);
        J->SetArrayField(TEXT("loaded_candidates"),Candidates);
        J->SetStringField(TEXT("overlap_edge_sequence"),TEXT("outside_far_to_capsule_overlap_shell_to_same_overlap_service_range_then_full_exit_reentry"));
        J->SetNumberField(TEXT("overlap_edge_radius_cm"),EdgeRadius);J->SetNumberField(TEXT("overlap_edge_shell_actor_distance_cm"),EdgeCenterDistance);
        J->SetBoolField(TEXT("overlap_edge_physical_overlap"),bEdgePhysicalOverlap);J->SetBoolField(TEXT("overlap_edge_same_context_restored"),bEdgeSameContextRestored);
        J->SetBoolField(TEXT("overlap_edge_single_widget_after_reentry"),bEdgeSingleWidget);J->SetBoolField(TEXT("overlap_edge_single_F_open"),bEdgeSingleFOpened);
        J->SetStringField(TEXT("fixture_revision"),TEXT("equipped_knuckle_reserved_F_paid_QER_LeftShift_sprint_bound_20261111"));
        J->SetStringField(TEXT("evidence_note"),TEXT("Fresh exact FpNpc slots; normal entry then autosave detach. Existing starter knuckle explicitly equipped via inventory, existing active tree QA unlocked without asset edits. Common Q eligible noncharged paid skill executes exactly once with observed resource spend and cooldown outside NPC range. F binding rejection must preserve the full existing loadout. Real Q is blocked by settings and dialogue; reserved F only opens/closes dialogue with no extra attack. Original overlap boundary probes retained. Controller InputKey is programmatic, not physical input or normal field completion; no first-person validation. Q/E/R bare execution, LeftShift guards and menu-close release-only guards are tested. LeftShift-only probes permit actual reflected sprint drain rate times observed world duration plus one authored tick; skill uses, accepted skill, cooldown, charge, mana and identity remain guarded. Other seven modifier keys and charged-skill cancellation are not validated by this fixture."));
        TArray<TSharedPtr<FJsonValue>> R;for(const FString& S:Reasons){R.Add(MakeShared<FJsonValueString>(S));}J->SetArrayField(TEXT("failure_reasons"),R);
        FString Json;const auto Writer=TJsonWriterFactory<>::Create(&Json);const bool Saved=FJsonSerializer::Serialize(J,Writer)&&FFileHelper::SaveStringToFile(Json,*(Output/TEXT("results.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);if(!Saved){++Failures;}
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=NPC_INTERACTION_%s checks=%d failures=%d collision_detected=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,bCollisionDetected,*Output);
        if(bExit){FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);}
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;TWeakObjectPtr<URogue10mSkillBindingComponent> Bindings;TWeakObjectPtr<URogue10mAttackSkillData> Skill;TWeakObjectPtr<ARogue10mTutorialNPC> Npc;TWeakObjectPtr<AActor> Ground;
    TWeakObjectPtr<USphereComponent> Detection;TWeakObjectPtr<URogue10mTutorialWidget> EdgeWidget;FVector EdgeShell;double EdgeRadius=0,EdgeCenterDistance=0;
    bool bEdgeFarReleased=false,bEdgePhysicalOverlap=false,bEdgeSameContextRestored=false,bEdgeReleasedAgain=false,bEdgeSingleWidget=false,bEdgeSingleFOpened=false;
    URogue10mProgressionSubsystem* Saves=nullptr;FDelegateHandle SampleHandle,Cleanup;TFunction<void()> Next,IdleNext;TArray<TSharedPtr<FJsonValue>> Probes,Candidates;TArray<FString> Reasons;
    FString BeforeAcceptedSkill;double ProbeStartWorld=0,SprintRate=0,SprintInterval=0,SprintDuration=0,SprintAllowance=0;
    FString Label,Output,Images,Events,Stage=TEXT("setup"),Prompt,FBindingReason,ProbeName;FTransform OriginalActor;FRotator InitialControl;FVector Near,Far,BeforeResources,MinResources;
    FKey OldBinding,PositiveKey=EKeys::Q;double Deadline=0,NextAt=0,IdleDeadline=0;float MaxCooldown=0;int32 Checks=0,Failures=0,Frame=0,BeforeUses=0,BeforeTotalUses=0,TestUnlockCount=0;
    bool bPlaybackObserved=false,bFrozenFpMontageObserved=false,bBusyChangeViolation=false;
    bool bIdleOneSecondRecorded=false;
    bool bFinished=false,bPrepared=false,bRendered=false,bOffscreen=false,bOriginalDamage=true,bObserve=false,bSawDialogue=false,bSawChargeHeld=false,bFBindingAccepted=false,bQBindingAccepted=false,bFBindingPreserved=false,bCollisionDetected=false;
};
static TSharedPtr<FRun> Active;
static void Run()
{
    if(!GEngine||GIsEditor||(Active&&!Active->Finished())){return;}
    for(const FWorldContext& C:GEngine->GetWorldContexts()){if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}}
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=NPC_INTERACTION_FAILED reason=no_standalone_world"));FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestNpcInteraction"),TEXT("Fresh exact FpNpc QA slots only: actual NPC F prompt/dialogue and reserved F dialogue and actual common Q combat guards, no physical input or map editing."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
