// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputKeySelector.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/Rogue10mMenuWindowWidgets.h"

namespace Rogue10mFighterSkillTreeTest
{
struct FStep { float Wait; TFunction<void()> Action; };
class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld, bool bPersistenceOnly)
    {
        World = InWorld;
        bPersistence = bPersistenceOnly;
        FString Slot;
        bool bValidSlot = FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), Slot)
            && Slot.StartsWith(TEXT("Rogue10m_Fighter_QA_")) && Slot.Len() <= 100;
        for (TCHAR C : Slot) { bValidSlot &= FChar::IsAlnum(C) || C == TCHAR('_'); }
        if (!bValidSlot)
        {
            Check(false, TEXT("isolated_save_slot_required")); Finish(); return;
        }
        InWorld->GetTimerManager().SetTimer(Timer,
            FTimerDelegate::CreateLambda([Self=AsShared()] { Self->Prepare(); }), 3.0f, false);
    }
private:
    void Check(bool bCondition, const FString& Reason)
    {
        ++Assertions;
        if (!bCondition) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("FIGHTER_SKILL_TREE FAIL %s"), *Reason); }
        else { UE_LOG(LogRogue10m, Display, TEXT("FIGHTER_SKILL_TREE PASS %s"), *Reason); }
        Report += FString::Printf(TEXT("%s %s\n"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Reason);
    }
    void Add(float Wait, TFunction<void()> Action) { Steps.Add({Wait, MoveTemp(Action)}); }
    void Key(FKey KeyValue, bool bDown)
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(KeyValue,
            bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
    }
    int32 Uses(URogue10mAttackSkillData* Skill) const { return Progress->GetSkillUseCount(Skill->GetFName()); }
    void Refill()
    {
        auto* A = Character->GetRogueAttributeSet();
        A->SetStamina(A->GetMaxStamina()); A->SetMana(A->GetMaxMana()); A->SetIdentity(A->GetMaxIdentity());
    }
    void SelectKeyThroughSlate(FKey KeyValue)
    {
        auto* Selector = Cast<UInputKeySelector>(Window->GetWidgetFromName(TEXT("UI_SkillKeySelector")));
        if (!Selector) { Check(false,TEXT("live_key_selector_missing")); return; }
        TSharedRef<SWidget> Slate = Selector->TakeWidget();
        FSlateApplication::Get().SetKeyboardFocus(Slate);
        const FKeyEvent Enter(EKeys::Enter,FModifierKeysState(),0,false,0,0);
        Slate->OnKeyDown(Selector->GetCachedGeometry(),Enter);
        Slate->OnKeyUp(Selector->GetCachedGeometry(),Enter);
        Check(Selector->GetIsSelectingKey(),TEXT("slate_key_selector_enters_capture"));
        Slate->OnKeyUp(Selector->GetCachedGeometry(),FKeyEvent(KeyValue,FModifierKeysState(),0,false,0,0));
        Check(!Selector->GetIsSelectingKey(),TEXT("slate_key_selector_exits_capture"));
    }
    void ObserveAcceptedMontage()
    {
        if (!bObserveCast || !Active.IsValid() || Uses(Active.Get()) != Before+1) { return; }
        auto* Mesh=Character->GetAnimationPlaybackMesh();
        auto* Anim=Mesh ? Mesh->GetAnimInstance() : nullptr;
        if (!Anim || !Anim->Montage_IsActive(Active->AttackMontage)) { return; }
        auto* Instance=Anim->GetActiveInstanceForMontage(Active->AttackMontage);
        if (!Instance || Instance->GetWeight()<=0) { return; }
        const float Position=Anim->Montage_GetPosition(Active->AttackMontage);
        if (MontageSamples==0) { FirstMontagePosition=Position; }
        LastMontagePosition=FMath::Max(LastMontagePosition,Position); ++MontageSamples;
    }
    void Prepare()
    {
        Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        Character = Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
        Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
        Progress = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
        Bindings = Controller.IsValid() ? Controller->GetSkillBindingComponent() : nullptr;
        if (!Character.IsValid() || !Combat.IsValid() || !Progress.IsValid() || !Bindings.IsValid())
        { Check(false, TEXT("runtime_player_state_missing")); Finish(); return; }
        Character->SetCanBeDamaged(false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        GroundPosition = Character->GetActorLocation();
        Controller->CloseAllBlockingPanels();
        Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
        Controller->SetControlRotation(FRotator(-20.0f, Character->GetActorRotation().Yaw, 0.0f));
        for (URogue10mAttackSkillData* Skill : Combat->GetActiveSkillTreeSkills())
        {
            Skills.Add(Skill);
            if (Skill->GetName().Contains(TEXT("LeftJab"))) { Jab = Skill; }
            if (Skill->GetName().Contains(TEXT("RightHook"))) { Hook = Skill; }
            if (Skill->GetName().Contains(TEXT("Primary02"))) { Locked = Skill; }
            if (Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique) { Passive = Skill; }
        }
        if (!Jab.IsValid() || !Hook.IsValid() || !Locked.IsValid() || !Passive.IsValid())
        { Check(false, TEXT("required_fighter_nodes_missing")); Finish(); return; }
        OldFixed = FApp::UseFixedTimeStep(); OldDelta = FApp::GetFixedDeltaTime();
        FApp::SetFixedDeltaTime(1.0 / 30.0); FApp::SetUseFixedTimeStep(true); bFixed = true;
        MontageTick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* W,ELevelTick,float) {
            if (W==Self->World.Get()) { Self->ObserveAcceptedMontage(); }
        });
        if (bPersistence)
        {
            Check(Bindings->GetSkillKey(Jab.Get()) == EKeys::F7, TEXT("saved_key_restored_in_new_process"));
            Before = Uses(Jab.Get()); Refill();
            Add(0.4f, [this] { Key(EKeys::F7, true); });
            Add(0.3f, [this] { Key(EKeys::F7, false); });
            Add(0.8f, [this] { Check(Uses(Jab.Get()) == Before + 1, TEXT("restored_key_executes_skill")); });
            Advance(); return;
        }
        Controller->ToggleSkillTree();
        for (TObjectIterator<URogue10mSkillTreeWindowWidget> It; It; ++It)
        { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == Controller.Get()) { Window = *It; break; } }
        if (!Window.IsValid()) { Check(false, TEXT("live_skill_window_missing")); Finish(); return; }
        Check(Skills.Num() == 14, TEXT("unarmed_tree_fourteen_nodes"));
        Check(Combat->GetUnlockedWeaponSkills().Num() == 5, TEXT("initial_five_basic_nodes"));
        auto* Canvas = Cast<UCanvasPanel>(Window->GetWidgetFromName(TEXT("UI_SkillListContainer")));
        int32 NodeCount = 0;
        if (Canvas) { for (UWidget* Child : Canvas->GetAllChildren()) { NodeCount += Child->IsA<URogue10mSkillTreeEntryWidget>() ? 1 : 0; } }
        Check(NodeCount == 14, TEXT("live_umg_fourteen_entries"));
        Check(!Combat->IsAttackSkillUnlocked(Locked.Get()), TEXT("advanced_skill_initially_locked"));
        Window->SelectSkill(Locked.Get());
        Check(!Window->SetSelectedSkillKey(EKeys::F6), TEXT("locked_skill_key_rejected"));
        const int32 Training = Uses(Jab.Get());
        auto* Unlock = Cast<UButton>(Window->GetWidgetFromName(TEXT("UI_UnlockAllSkillsButton")));
        Check(Unlock && Unlock->GetIsEnabled(), TEXT("test_unlock_button_available"));
        if (Unlock) { Unlock->OnClicked.Broadcast(); }
        Check(Combat->GetUnlockedWeaponSkills().Num() == 14, TEXT("button_unlocks_all_fourteen"));
        Check(Uses(Jab.Get()) == Training, TEXT("test_unlock_preserves_training_counts"));
        Check(Combat->UnlockAllActiveTreeForTesting() == 0, TEXT("test_unlock_idempotent"));
        Check(FMath::IsNearlyEqual(Combat->GetUnlockedPassiveDamageReduction(), 0.15f), TEXT("passive_applies_after_unlock"));
        Window->SelectSkill(Passive.Get());
        Check(!Window->SetSelectedSkillKey(EKeys::F6), TEXT("passive_key_rejected"));
        Window->SelectSkill(Jab.Get());
        Check(Window->SetSelectedSkillKey(EKeys::F6), TEXT("selected_skill_key_set"));
        for (FKey Reserved : { EKeys::W, EKeys::One, EKeys::K, EKeys::LeftMouseButton })
        { Check(!Window->SetSelectedSkillKey(Reserved), TEXT("reserved_key_rejected_") + Reserved.ToString()); }
        Window->SelectSkill(Locked.Get());
        Check(!Window->SetSelectedSkillKey(EKeys::F6), TEXT("duplicate_key_rejected"));
        Check(Bindings->GetSkillKey(Jab.Get()) == EKeys::F6, TEXT("rejected_binding_preserves_original"));
        Window->SelectSkill(Jab.Get());
        Check(Window->SetSelectedSkillKey(EKeys::F7), TEXT("key_rebind_succeeds"));
        SelectKeyThroughSlate(EKeys::F6);
        Check(Bindings->GetSkillKey(Jab.Get())==EKeys::F6,TEXT("slate_selected_key_reaches_saved_binding"));
        SelectKeyThroughSlate(EKeys::Escape);
        Check(Bindings->GetSkillKey(Jab.Get())==EKeys::F6,TEXT("slate_escape_preserves_binding"));
        SelectKeyThroughSlate(EKeys::W);
        Check(Bindings->GetSkillKey(Jab.Get())==EKeys::F6,TEXT("slate_reserved_key_restores_previous"));
        SelectKeyThroughSlate(EKeys::F7);
        Check(Bindings->GetSkillKey(Jab.Get())==EKeys::F7,TEXT("slate_new_key_saved"));
        FText SameFrameError;
        Check(Bindings->SetSkillKey(Locked.Get(),EKeys::F9,SameFrameError)
            && Bindings->ClearSkillKey(Locked.Get(),SameFrameError),TEXT("same_frame_second_binding_edit"));
        FScreenshotRequest::RequestScreenshot(TEXT("FighterSkillTree"), true, false);

        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            It->ClearAITarget(); if (AController* AI = It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->DisableMovement();
            if (!Target.IsValid())
            {
                Target = *It;
                if (auto* Regen = It->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
                It->GetRogueAttributeSet()->SetMaxHealth(10000); It->GetRogueAttributeSet()->SetHealth(10000);
                It->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                It->SetActorLocation(Character->GetActorLocation() + Character->GetActorForwardVector()*135.0f);
            }
            else { It->SetActorEnableCollision(false); }
        }
        Check(Target.IsValid(), TEXT("actual_damage_target_available"));
        Before = Uses(Jab.Get());
        Add(0.5f, [this] { Key(EKeys::F7, true); });
        Add(0.2f, [this] { Key(EKeys::F7, false); Check(Uses(Jab.Get()) == Before, TEXT("menu_blocks_bound_skill")); Controller->CloseAllBlockingPanels(); });
        Add(0.5f, [this] { Key(EKeys::F6, true); });
        Add(0.2f, [this] { Key(EKeys::F6, false); Check(Uses(Jab.Get()) == Before, TEXT("old_key_no_longer_executes")); Refill(); Key(EKeys::F7, true); });
        Add(0.2f, [this] { Key(EKeys::F7, false); Check(Uses(Jab.Get()) == Before+1, TEXT("new_key_executes_once"));
            Check(!Combat->ActivateTreeSkill(Jab.Get()), TEXT("shared_cooldown_blocks_second_cast")); });
        Add(2.0f, [this] { Check(Target.IsValid() && Target->GetRogueAttributeSet()->GetHealth() < 10000, TEXT("key_cast_causes_actual_health_loss"));
            FText Error; Check(Bindings->ClearSkillKey(Jab.Get(), Error), TEXT("key_clear_succeeds")); Before=Uses(Jab.Get()); });
        Add(0.3f, [this] { Key(EKeys::F7, true); });
        Add(0.2f, [this] { Key(EKeys::F7, false); Check(Uses(Jab.Get())==Before, TEXT("cleared_key_no_cast"));
            FText Error; Check(Bindings->SetSkillKey(Jab.Get(),EKeys::F7,Error), TEXT("test_key_restored"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Dagger); });
        Add(0.5f, [this] { Key(EKeys::F7,true); });
        Add(0.2f, [this] { Key(EKeys::F7,false); Check(Uses(Jab.Get())==Before,TEXT("inactive_weapon_key_no_cast"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
            Check(Combat->GetActiveSkillTreeSkills().Num()==14,TEXT("knuckle_tree_fourteen_nodes"));
            Check(Bindings->GetSkillKey(Jab.Get())==EKeys::F7,TEXT("fighter_key_retained_on_weapon_switch"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed);
            FText Error; Check(Bindings->SetSkillKey(Hook.Get(),EKeys::F8,Error),TEXT("charged_skill_key_set")); Before=Uses(Hook.Get()); });
        Add(0.4f,[this] { Key(EKeys::F8,true); });
        Add(0.06f,[this] { Key(EKeys::F8,false); });
        Add(0.2f,[this] { Check(Uses(Hook.Get())==Before,TEXT("short_hold_rejected")); Refill(); Key(EKeys::F8,true); });
        Add(Hook->ChargeSeconds+0.15f,[this] { Check(Uses(Hook.Get())==Before,TEXT("charge_does_not_cast_before_release")); Key(EKeys::F8,false); });
        Add(0.25f,[this] { Check(Uses(Hook.Get())==Before+1,TEXT("full_hold_release_casts")); });
        Add(3.0f,[this] { Before=Uses(Hook.Get()); Refill(); Key(EKeys::F8,true); });
        Add(0.15f,[this] { Controller->ToggleSkillTree(); });
        Add(Hook->ChargeSeconds+0.15f,[this] { Controller->CloseAllBlockingPanels(); Key(EKeys::F8,false); });
        Add(0.25f,[this] { Check(Uses(Hook.Get())==Before,TEXT("menu_open_cancels_held_charge")); Refill(); Key(EKeys::F8,true); });
        Add(0.15f,[this] { Character->SetEquippedWeaponType(ERogue10mWeaponType::Dagger); });
        Add(0.15f,[this] { Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); });
        Add(Hook->ChargeSeconds+0.15f,[this] { Key(EKeys::F8,false); });
        Add(0.25f,[this] { Check(Uses(Hook.Get())==Before,TEXT("weapon_switch_cancels_held_charge")); Refill(); Key(EKeys::F8,true); });
        Add(0.15f,[this] { Controller->SetPause(true); Controller->SetPause(false); });
        Add(Hook->ChargeSeconds+0.15f,[this] { Key(EKeys::F8,false); });
        Add(0.25f,[this] { Check(Uses(Hook.Get())==Before,TEXT("pause_cancels_held_charge")); Refill(); Key(EKeys::F8,true); });
        Add(0.15f,[this] { FText Error; Check(Bindings->SetSkillKey(Hook.Get(),EKeys::F9,Error),TEXT("held_key_remap_succeeds")); });
        Add(Hook->ChargeSeconds+0.15f,[this] { Key(EKeys::F8,false); });
        Add(0.25f,[this] { Check(Uses(Hook.Get())==Before,TEXT("remap_cancels_old_held_charge")); FText Error; Bindings->ClearSkillKey(Hook.Get(),Error); });

        // Execute every active node through the actual Enhanced Input key mapping.
        for (TWeakObjectPtr<URogue10mAttackSkillData> WeakSkill : Skills)
        {
            if (WeakSkill->SkillTreeNodeType != ERogue10mSkillTreeNodeType::ActiveTechnique) { continue; }
            Add(0.2f,[this,WeakSkill] {
                FText Error; Bindings->ClearSkillKey(Jab.Get(),Error);
                Refill(); auto* Skill=WeakSkill.Get(); Active=Skill; Before=Uses(Skill);
                bObserveCast=false; MontageSamples=0; FirstMontagePosition=LastMontagePosition=0;
                Check(Bindings->SetSkillKey(Skill,EKeys::F6,Error),TEXT("bind_each_")+Skill->GetName());
                if (Skill->InputSlot==ERogue10mAttackInputSlot::JumpPrimary || Skill->InputSlot==ERogue10mAttackInputSlot::JumpSpecial)
                {
                    Check(!Combat->ActivateTreeSkill(Skill),TEXT("ground_jump_skill_rejected"));
                    Character->SetActorLocation(Character->GetActorLocation()+FVector(0,0,250));
                    Character->GetCharacterMovement()->GravityScale=0;
                    Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                }
                else { Check(!Character->GetCharacterMovement()->IsFalling(),TEXT("ground_fixture_")+Skill->GetName()); }
            });
            Add(0.3f,[this] { bObserveCast=true; Key(EKeys::F6,true); });
            const bool bCharged=WeakSkill->InputSlot==ERogue10mAttackInputSlot::ChargedPrimary || WeakSkill->InputSlot==ERogue10mAttackInputSlot::ChargedSpecial;
            Add(bCharged ? WeakSkill->ChargeSeconds+0.15f : 0.2f,[this] { Key(EKeys::F6,false); });
            Add(0.25f,[this] {
                Check(Uses(Active.Get())==Before+1,TEXT("actual_key_executes_")+Active->GetName());
                // Observe only after the accepted input and before blend-out. A short
                // valid montage can have completed active playback at this later UI step.
                Check(MontageSamples>=2,TEXT("actual_montage_")+Active->GetName());
                Check(LastMontagePosition>FirstMontagePosition+UE_SMALL_NUMBER,
                      TEXT("actual_montage_advances_")+Active->GetName());
                UE_LOG(LogRogue10m,Display,TEXT("FIGHTER_MONTAGE_OBSERVED skill=%s samples=%d first=%.6f last=%.6f length=%.6f"),
                    *Active->GetName(),MontageSamples,FirstMontagePosition,LastMontagePosition,Active->AttackMontage->GetPlayLength());
                bObserveCast=false; ++Executed;
            });
            Add(FMath::Max(4.0f,WeakSkill->AttackCooldown+1.0f),[this] {
                FText Error; Bindings->ClearSkillKey(Active.Get(),Error);
                if (Character->GetCharacterMovement()->IsFalling())
                {
                    Character->SetActorLocation(GroundPosition,false,nullptr,ETeleportType::TeleportPhysics);
                    Character->GetCharacterMovement()->StopMovementImmediately();
                    Character->GetCharacterMovement()->GravityScale=1;
                    Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                }
            });
        }
        Add(0.2f,[this] {
            FText Error; Check(Bindings->SetSkillKey(Jab.Get(),EKeys::F7,Error),TEXT("persistence_key_saved"));
            Character->SetEquippedWeaponType(ERogue10mWeaponType::Unarmed); Controller->ToggleSkillTree(); Window->SelectSkill(Jab.Get());
            Check(Executed==13,TEXT("all_thirteen_active_skills_exercised"));
        });
        Advance();
    }
    void Advance()
    {
        if (!World.IsValid() || !Controller.IsValid() || !Character.IsValid())
        { Check(false,TEXT("runtime_lost")); Finish(); return; }
        if (Index>=Steps.Num()) { Finish(); return; }
        FStep& Step=Steps[Index++];
        World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self=AsShared(),Action=MoveTemp(Step.Action)]() mutable {
            Action(); Self->Advance();
        }),FMath::Max(0.01f,Step.Wait),false);
    }
    void Finish()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(MontageTick);
        if (World.IsValid()) { World->GetTimerManager().ClearTimer(Timer); }
        if (bFixed) { FApp::SetUseFixedTimeStep(OldFixed); FApp::SetFixedDeltaTime(OldDelta); }
        const FString Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("Feature/doc/evidence/fighter-skill-tree-20260930"));
        IFileManager::Get().MakeDirectory(*Output,true);
        FFileHelper::SaveStringToFile(Report,*FPaths::Combine(Output,bPersistence?TEXT("persistence-assertions.txt"):TEXT("runtime-assertions.txt")));
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=FIGHTER_SKILL_TREE_%s mode=%s assertions=%d active_executed=%d failures=%d"),
            Failures==0?TEXT("PASSED"):TEXT("FAILED"),bPersistence?TEXT("persistence"):TEXT("full"),Assertions,Executed,Failures);
        FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<ARogue10mPlayerState> Progress;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TWeakObjectPtr<URogue10mSkillBindingComponent> Bindings;
    TWeakObjectPtr<URogue10mSkillTreeWindowWidget> Window;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<URogue10mAttackSkillData> Jab, Hook, Locked, Passive, Active;
    TArray<TWeakObjectPtr<URogue10mAttackSkillData>> Skills;
    TArray<FStep> Steps;
    FTimerHandle Timer;
    FString Report;
    int32 Failures=0, Assertions=0, Index=0, Before=0, Executed=0;
    bool bFixed=false, OldFixed=false, bPersistence=false;
    double OldDelta=0;
    FVector GroundPosition=FVector::ZeroVector;
    FDelegateHandle MontageTick;
    bool bObserveCast=false;
    int32 MontageSamples=0;
    float FirstMontagePosition=0,LastMontagePosition=0;
};
static void Run(bool bPersistence)
{
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World=Context.World();
        if (World && (World->WorldType==EWorldType::Game || World->WorldType==EWorldType::PIE))
        { MakeShared<FRun>()->Start(World,bPersistence); return; }
    }
    UE_LOG(LogRogue10m,Error,TEXT("RESULT=FIGHTER_SKILL_TREE_FAILED no_game_world"));
    FPlatformMisc::RequestExitWithStatus(false,1);
}
static FAutoConsoleCommand FullCommand(TEXT("Rogue10m.TestFighterSkillTree"),TEXT("Checks live fighter UI, unlocks, bindings and actual key casts."),
    FConsoleCommandDelegate::CreateLambda([] { Run(false); }));
static FAutoConsoleCommand PersistenceCommand(TEXT("Rogue10m.TestFighterSkillKeyPersistence"),TEXT("Checks fighter key restore in a new game process."),
    FConsoleCommandDelegate::CreateLambda([] { Run(true); }));
}
#endif
