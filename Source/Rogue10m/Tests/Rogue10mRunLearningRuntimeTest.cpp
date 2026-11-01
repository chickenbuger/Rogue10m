// Isolated programmatic integration, not physical-input or full-field-play evidence.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mCombatTestWidget.h"
#include "Rogue10mMainHUDWidget.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mHybridSkillTreeWidget.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mOpenWorldBootstrap.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Rogue10mRunLearningQA
{
using ERoute = ERogue10mSkillTreeUnlockRoute;
using EIdentity = ERogue10mSkillTreeIdentity;
static const FName FirstNode(TEXT("I.InternalEnergy.Entry"));
static const FName SecondNode(TEXT("I.FightingSpirit.Entry"));

class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& InWorld)
    {
        World = &InWorld;
        FString ProfileSlot, Prefix, KeySlot;
        const TCHAR* Args = FCommandLine::Get();
        FParse::Value(Args, TEXT("RogueRunLearningLabel="), Label);
        FParse::Value(Args, TEXT("RogueRunLearningMode="), Mode);
        FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
        FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
        FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), KeySlot);
        bool bLabel = !Label.IsEmpty() && Label.Len() <= 24;
        for (TCHAR Ch : Label) { bLabel &= FChar::IsAlnum(Ch) || Ch == TCHAR('_'); }
        if (!bLabel || ProfileSlot != TEXT("Rogue10m_Town_QA_Learning_") + Label
            || Prefix != TEXT("Rogue10m_Progression_QA_Learning_") + Label
            || KeySlot != TEXT("Rogue10m_Fighter_QA_Learning_") + Label
            || (Mode != TEXT("Normal") && Mode != TEXT("Abandon") && Mode != TEXT("Defeat"))
            || InWorld.GetNetMode() != NM_Standalone)
        {
            UE_LOG(LogRogue10m, Error, TEXT("RESULT=RUN_LEARNING_FAILED reason=exact_fresh_QA_overrides_required"));
            FPlatformMisc::RequestExitWithStatus(false, 1); return;
        }
        Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/run-learning-20261101"), Label);
        Images = FPaths::Combine(FPaths::ProjectDir(), TEXT("DevLog/20261101_RunLearning_") + Label);
        TArray<FString> ExistingProgression;
        IFileManager::Get().FindFiles(ExistingProgression, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Prefix + TEXT("_*.sav")), true, false);
        if (!ExistingProgression.IsEmpty() || IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images)
            || UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0) || UGameplayStatics::DoesSaveGameExist(KeySlot, 0))
        {
            UE_LOG(LogRogue10m, Error, TEXT("RESULT=RUN_LEARNING_FAILED reason=fresh_QA_label_required"));
            FPlatformMisc::RequestExitWithStatus(false, 1); return;
        }
        IFileManager::Get().MakeDirectory(*Output, true);
        bRendered = !FParse::Param(Args, TEXT("nullrhi"));
        if (bRendered) { IFileManager::Get().MakeDirectory(*Images, true); }
        Deadline = FPlatformTime::Seconds() + 240.0;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds() > Self->Deadline) { Self->Check(false, TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt) { auto Work = MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }), 0.02f);
        Later(4.0, [this] { Prepare(); });
    }
    bool Finished() const { return bFinished; }
private:
    void Check(bool bOK, const TCHAR* Why)
    {
        ++Checks; Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Why);
        if (!bOK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("RUN_LEARNING FAIL %s"), Why); }
    }
    void Later(double Delay, TFunction<void()> Work) { NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work); }
    static bool SameLearningLedger(const FRogue10mSkillTreeSnapshot& A, const FRogue10mSkillTreeSnapshot& B)
    {
        if (A.ActiveIdentity != B.ActiveIdentity || A.Investments.Num() != B.Investments.Num()
            || A.Masteries.Num() != B.Masteries.Num() || A.LegacyGrantedSkills.Num() != B.LegacyGrantedSkills.Num()) { return false; }
        for (int32 I = 0; I < A.Investments.Num(); ++I)
        {
            const auto& L = A.Investments[I]; const auto& R = B.Investments[I];
            if (L.NodeId != R.NodeId || L.Route != R.Route || L.SpentPoints != R.SpentPoints || L.ChoiceId != R.ChoiceId) { return false; }
        }
        for (int32 I = 0; I < A.Masteries.Num(); ++I)
        { if (A.Masteries[I].Track != B.Masteries[I].Track || A.Masteries[I].TotalExperience != B.Masteries[I].TotalExperience) { return false; } }
        for (int32 I = 0; I < A.LegacyGrantedSkills.Num(); ++I)
        { if (A.LegacyGrantedSkills[I].ToSoftObjectPath() != B.LegacyGrantedSkills[I].ToSoftObjectPath()) { return false; } }
        return true;
    }
    void Photo(const TCHAR* Name)
    {
        if (!bRendered) { return; }
        const FString File = FPaths::Combine(Images, Name);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([File](float)
        { FScreenshotRequest::RequestScreenshot(File, true, false); return false; }), 0.2f);
    }
    void Key(FKey InputKey, EInputEvent Event)
    {
        if (PC.IsValid()) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(InputKey, Event, Event == IE_Released ? 0.f : 1.f)); }
    }
    bool Refresh()
    {
        auto* GI = World.IsValid() ? World->GetGameInstance() : nullptr;
        Profiles = GI ? GI->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        Saves = GI ? GI->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        PC = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Character = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
        State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
        Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
        Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
        Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
        Town = State ? State->GetTownProgressionComponent() : nullptr;
        Run = World.IsValid() ? World->GetGameState<ARogue10mGameState>() : nullptr;
        auto* GM = World.IsValid() ? World->GetAuthGameMode<ARogue10mGameMode>() : nullptr;
        Completion = GM ? GM->GetRunCompletionComponent() : nullptr;
        return Profiles && Saves && PC.IsValid() && Character.IsValid() && State && Inventory && Combat && Tree && Town && Run.IsValid() && Completion.IsValid();
    }
    void Prepare()
    {
        Check(World.IsValid() && UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub"), TEXT("starts_in_actual_StartHub"));
        auto* GI = World.IsValid() ? World->GetGameInstance() : nullptr;
        Profiles = GI ? GI->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        PC = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Check(Profiles && PC.IsValid() && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_slot"));
        if (Failures) { Finish(); return; }
        FRogue10mCharacterProfile Profile;
        Check(Profiles->CreateProfile(TEXT("LearningQa"), FRogue10mCharacterAppearance(), Profile) && Profiles->SelectProfile(Profile.ProfileId), TEXT("new_QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        ProfileId = Profile.ProfileId; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Check(Refresh() && Saves->IsSaveReady() && Saves->IsAttachedTo(*Character), TEXT("selected_owned_pawn_is_save_ready"));
        if (Failures) { Finish(); return; }
        Check(!Tree->CanLearnDuringRun(), TEXT("town_is_not_run_learning_context"));
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle, TEXT("starts_with_knuckle_combat_track"));
        EntryGold = Inventory->GetGold(); EntryCrystals = Inventory->GetCrystals(); EntryLobby = Saves->GetLobbyAvailablePoints();
        Check(!Tree->IsNodeUnlocked(FirstNode) && !Tree->IsNodeUnlocked(SecondNode), TEXT("identities_start_unlearned"));
        if (Failures) { Finish(); return; }
        ARogue10mRunPortal* Portal = nullptr;
        for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It) { Portal = *It; break; }
        UBoxComponent* Box = Portal ? Portal->FindComponentByClass<UBoxComponent>() : nullptr;
        Check(Box != nullptr, TEXT("authored_hub_portal_collision_available"));
        if (Failures) { Finish(); return; }
        OldWorld = World;
        Character->SetActorLocation(Box->GetComponentLocation() + FVector(500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
        Character->SetActorLocation(Box->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
        Later(0.2, [this] { WaitForWorld(TEXT("OpenRunMap"), [this] { BeginField(); }); });
    }
    void WaitForWorld(const TCHAR* Name, TFunction<void()> Ready)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* Candidate = Context.World();
            if (!Candidate || Candidate == OldWorld.Get() || Context.WorldType != EWorldType::Game
                || !Candidate->HasBegunPlay() || !Candidate->GetFirstPlayerController()
                || UGameplayStatics::GetCurrentLevelName(Candidate, true) != Name) { continue; }
            World = Candidate; Later(3.0, MoveTemp(Ready)); return;
        }
        const FString Expected(Name);
        Later(0.2, [this, Expected, Ready = MoveTemp(Ready)]() mutable { WaitForWorld(*Expected, MoveTemp(Ready)); });
    }
    void VerifyFieldTimerPresentation()
    {
        if (!bRendered) { return; }
        URogue10mMainHUDWidget* HUD = nullptr;
        URogue10mCombatTestWidget* Tools = nullptr;
        for (TObjectIterator<URogue10mMainHUDWidget> It; It; ++It)
        { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()) { HUD = *It; break; } }
        for (TObjectIterator<URogue10mCombatTestWidget> It; It; ++It)
        { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()) { Tools = *It; break; } }
        UTextBlock* Timer = HUD ? Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("UI_RunTimerText"))) : nullptr;
        UWidget* Hint = Tools ? Tools->GetWidgetFromName(TEXT("CombatTestHint")) : nullptr;
        Check(HUD && HUD->GetRunTimerView().bVisible && Timer && Timer->IsVisible() && !Timer->GetText().IsEmpty(), TEXT("actual_running_field_timer_is_visible_and_bound"));
        Check(Hint && Hint->IsVisible(), TEXT("debug_hint_exists_for_real_overlap_regression"));
        if (!Timer || !Hint) { return; }
        const FGeometry& TG = Timer->GetCachedGeometry();
        const FGeometry& HG = Hint->GetCachedGeometry();
        const FVector2D TMin = TG.LocalToAbsolute(FVector2D::ZeroVector), TMax = TG.LocalToAbsolute(TG.GetLocalSize());
        const FVector2D HMin = HG.LocalToAbsolute(FVector2D::ZeroVector), HMax = HG.LocalToAbsolute(HG.GetLocalSize());
        const bool bValid = TMax.X > TMin.X && TMax.Y > TMin.Y && HMax.X > HMin.X && HMax.Y > HMin.Y;
        const bool bOverlap = TMin.X < HMax.X && HMin.X < TMax.X && TMin.Y < HMax.Y && HMin.Y < TMax.Y;
        Check(bValid && !bOverlap, TEXT("live_timer_is_not_occluded_by_F6_debug_button"));
    }
    void BeginField()
    {
        Check(Refresh() && Run->IsRunActive() && Saves->IsSaveReady() && Saves->GetAttachedProfileId() == ProfileId, TEXT("portal_restores_owned_profile_and_starts_actual_field_run"));
        if (Failures) { Finish(); return; }
        Check(FMath::IsNearlyEqual(Run->GetRunDurationSeconds(), 1200.0f), TEXT("normal_field_retains_twenty_minute_run_limit"));
        VerifyFieldTimerPresentation();
        PC->CloseAllBlockingPanels(); Character->GetCharacterMovement()->StopMovementImmediately();
        Check(!Character->IsDead() && Tree->CanLearnDuringRun(), TEXT("living_attached_character_can_learn_during_run"));
        Check(!Tree->CanChangeInvestments() && !Town->CanChangeTraits(), TEXT("run_learning_does_not_enable_town_edits"));
        State->AddExperience(500); Inventory->AddGold(77);
        Check(Tree->GetAvailableGrowthPoints() >= 2, TEXT("programmatic_XP_fixture_provides_two_purchase_points"));
        Check(Saves->SaveNow(), TEXT("QA_run_state_saved_before_learning"));
        if (Failures) { Finish(); return; }
        Slot = Saves->GetSaveSlotForProfile(ProfileId);
        VerifySaveFailureLearningGate();
        if (Failures) { Finish(); return; }
        EntrySwordDeadline = FPlatformTime::Seconds() + 90.0;
        Later(0.2, [this] { WaitForEntryGreatSword(); });
    }
    int32 CountEntryGreatSwordDrops(ARogue10mDroppedItem** OutDrop = nullptr) const
    {
        int32 Count = 0;
        if (OutDrop) { *OutDrop = nullptr; }
        if (!World.IsValid()) { return Count; }
        for (TActorIterator<ARogue10mDroppedItem> It(World.Get()); It; ++It)
        {
            if (!It->IsActorBeingDestroyed() && It->ActorHasTag(TEXT("Rogue10m.EntryGreatSword.Pickup")))
            { ++Count; if (OutDrop) { *OutDrop = *It; } }
        }
        return Count;
    }
    void WaitForEntryGreatSword()
    {
        if (!EntryBootstrap.IsValid())
        {
            for (TActorIterator<ARogue10mOpenWorldBootstrap> It(World.Get()); It; ++It) { EntryBootstrap = *It; break; }
        }
        ARogue10mDroppedItem* Drop = nullptr;
        const int32 DropCount = CountEntryGreatSwordDrops(&Drop);
        if ((!EntryBootstrap.IsValid() || !EntryBootstrap->HasPlacedEntryGreatSword() || DropCount == 0)
            && FPlatformTime::Seconds() < EntrySwordDeadline)
        { Later(0.2, [this] { WaitForEntryGreatSword(); }); return; }
        Check(EntryBootstrap.IsValid() && EntryBootstrap->HasPlacedEntryGreatSword(), TEXT("authored_bootstrap_places_entry_sword_after_streaming"));
        Check(DropCount == 1 && Drop && Drop->GetWorld() == World.Get(), TEXT("same_world_has_exactly_one_tagged_entry_sword_pickup"));
        const auto* Settings = World->GetWorldSettings();
        Check(Settings && Settings->ActorHasTag(TEXT("Rogue10m.EntryGreatSword.Placed")), TEXT("world_once_ledger_records_entry_sword_placement"));
        if (Failures) { Finish(); return; }
        EntrySword = Drop;
        const auto& Stack = Drop->GetItemStack();
        Check(Stack.bOccupied && Stack.InstanceId.IsValid() && Stack.Quantity == 1 && Stack.ItemData
            && Stack.WeaponType == ERogue10mWeaponType::GreatSword
            && Stack.ItemData->WeaponType == ERogue10mWeaponType::GreatSword
            && Stack.ItemData->GetPathName() == TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"), TEXT("runtime_drop_uses_authored_sword_metadata_and_valid_instance_GUID"));
        Check(Stack.ItemData && Stack.Category == Stack.ItemData->Category && Stack.Category == ERogue10mItemCategory::Equipment
            && Stack.EquipSlotType == Stack.ItemData->EquipSlotType && Stack.DisplayName.EqualTo(Stack.ItemData->DisplayName)
            && Stack.Description.EqualTo(Stack.ItemData->Description) && Stack.ItemColor.Equals(Stack.ItemData->InventoryTint)
            && Stack.EnhancementLevel == 0 && FMath::IsNearlyZero(Stack.EnhancementAttackBonus)
            && FMath::IsNearlyZero(Stack.EnhancementDefenseBonus) && !Stack.bLocked, TEXT("fixed_drop_preserves_item_classification_display_and_fresh_enhancement_metadata"));
        EntrySwordId = Stack.InstanceId;
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        DuplicateBootstrap = World->SpawnActor<ARogue10mOpenWorldBootstrap>(ARogue10mOpenWorldBootstrap::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
        Check(DuplicateBootstrap.IsValid(), TEXT("duplicate_bootstrap_fixture_created_in_same_world"));
        // Leave the duplicate alive until hard travel; its nav registration shares the pawn.
        Photo(TEXT("00_entry_sword_before_pickup.png"));
        if (Failures) { Finish(); return; }
        Later(2.0, [this]
        {
            Check(CountEntryGreatSwordDrops() == 1 && DuplicateBootstrap.IsValid() && DuplicateBootstrap->HasPlacedEntryGreatSword(), TEXT("duplicate_bootstrap_does_not_duplicate_existing_pickup"));
            if (Failures) { Finish(); return; }
            PickupEntryGreatSword();
        });
    }
    int32 InventoryEntrySwordCount() const
    {
        int32 Count = 0;
        for (const auto& Container : Inventory->GetInventoryContainers())
        {
            for (const auto& Entry : Container.Entries)
            {
                if (Entry.InstanceId == EntrySwordId && Entry.ItemData
                    && Entry.ItemData->WeaponType == ERogue10mWeaponType::GreatSword && Entry.Quantity == 1) { ++Count; }
            }
        }
        return Count;
    }
    void PickupEntryGreatSword()
    {
        Check(EntrySword.IsValid() && Character.IsValid() && !Character->IsDead(), TEXT("living_character_and_entry_pickup_remain_available"));
        if (Failures) { Finish(); return; }
        PrePickupLocation = Character->GetActorLocation();
        const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // 200cm lateral offset avoids the pickup mesh overlap; within TryPickup's 250cm range.
        Character->SetActorLocation(EntrySword->GetActorLocation() + FVector(200.f, 0.f, HalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
        Check(EntrySword->TryPickup(Character.Get()), TEXT("existing_programmatic_TryPickup_accepts_real_entry_sword"));
        Check(InventoryEntrySwordCount() == 1, TEXT("picked_up_sword_keeps_exact_instance_GUID_in_inventory"));
        Character->SetActorLocation(PrePickupLocation, false, nullptr, ETeleportType::TeleportPhysics);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle, TEXT("entry_sword_pickup_does_not_replace_knuckle_equipment"));
        Check(Saves->SaveNow(), TEXT("picked_up_entry_sword_snapshot_saved_before_profile_guard_fixtures"));
        if (Failures) { Finish(); return; }
        Later(2.0, [this]
        {
            Check(CountEntryGreatSwordDrops() == 0 && InventoryEntrySwordCount() == 1
                && EntryBootstrap.IsValid() && EntryBootstrap->HasPlacedEntryGreatSword()
                && DuplicateBootstrap.IsValid() && DuplicateBootstrap->HasPlacedEntryGreatSword(), TEXT("picked_up_sword_does_not_respawn_after_duplicate_bootstrap_retry"));
            Check(Character->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle, TEXT("learning_attack_fixture_still_uses_knuckle_track"));
            if (Failures) { Finish(); return; }
            ContinueLearningChecks();
        });
    }
    void ContinueLearningChecks()
    {
        const int32 OwnedPoints = Tree->GetAvailableGrowthPoints();
        const int32 OwnedCount = Tree->Capture().Investments.Num();
        Saves->DetachCharacter(*Character, false);
        Check(!Tree->CanLearnDuringRun() && !Tree->TryUnlockNode(FirstNode, ERoute::Points, NAME_None), TEXT("detached_profile_cannot_learn_in_running_world"));
        Check(Tree->GetAvailableGrowthPoints() == OwnedPoints && Tree->Capture().Investments.Num() == OwnedCount, TEXT("detached_learning_rejection_preserves_run_ledger"));
        Check(Saves->AttachCharacter(*Character) && Tree->CanLearnDuringRun(), TEXT("reattaching_saved_owned_profile_restores_learning_access"));
        FRogue10mCharacterProfile OtherProfile;
        Check(Profiles->CreateProfile(TEXT("LearningAlt"), FRogue10mCharacterAppearance(), OtherProfile)
            && Profiles->SelectProfile(OtherProfile.ProfileId), TEXT("second_QA_profile_selection_fixture_available"));
        Check(!Tree->CanLearnDuringRun() && !Tree->TryUnlockNode(FirstNode, ERoute::Points, NAME_None), TEXT("selected_profile_mismatch_cannot_modify_attached_run"));
        Check(Profiles->SelectProfile(ProfileId) && Tree->CanLearnDuringRun(), TEXT("returning_to_owned_selection_restores_learning_access"));
        if (Failures) { Finish(); return; }
        // A transient catalog copy adds a mandatory AND only for the rejected command.
        // It is restored synchronously before any timer/autosave can observe it.
        const auto Original = Tree->Catalog;
        TStrongObjectPtr<URogue10mSkillTreeDataAsset> Candidate(DuplicateObject(const_cast<URogue10mSkillTreeDataAsset*>(Tree->GetCatalog()), GetTransientPackage()));
        auto* Guarded = Candidate.IsValid() ? Candidate->Nodes.FindByPredicate([](const auto& N) { return N.NodeId == SecondNode; }) : nullptr;
        Check(Guarded != nullptr, TEXT("transient_required_all_fixture_has_existing_node"));
        if (!Guarded) { Finish(); return; }
        FRogue10mProgressionRequirement Required; Required.Kind = ERogue10mProgressionRequirementKind::DisciplineRankAtLeast; Required.Id = TEXT("Discipline.Mage"); Required.Minimum = 2;
        Guarded->RequiredAll.Add(Required); Tree->Catalog = Candidate.Get();
        const int32 Gold = Inventory->GetGold(), Points = Tree->GetAvailableGrowthPoints(), Count = Tree->Capture().Investments.Num();
        const auto Eligibility = Tree->EvaluateNodeEligibility(SecondNode);
        Check(Candidate->IsValidCatalog() && Eligibility.RequiredAll.bValid && !Eligibility.RequiredAll.bSatisfied,
            TEXT("required_all_fixture_is_valid_and_fails_only_its_mandatory_condition"));
        FText Reason;
        Check(!Tree->CanUnlockNode(SecondNode, ERoute::Points, NAME_None, Reason) && !Reason.IsEmpty(), TEXT("RequiredAll_blocks_points_despite_sufficient_budget"));
        Check(!Tree->TryUnlockNode(SecondNode, ERoute::Points, NAME_None), TEXT("RequiredAll_rejected_purchase_has_no_commit"));
        Check(!Tree->TryUnlockNode(SecondNode, ERoute::Mastery, NAME_None), TEXT("RequiredAll_cannot_be_bypassed_by_other_manual_route"));
        Check(Gold == Inventory->GetGold() && Points == Tree->GetAvailableGrowthPoints() && Count == Tree->Capture().Investments.Num(), TEXT("required_all_rejection_preserves_gold_points_and_ledger"));
        Tree->Catalog = Original;
        if (Failures) { Finish(); return; }
        Later(0.5, [this] { LearnDuringAttack(); });
    }
    void VerifySaveFailureLearningGate()
    {
        // Only the exact profile slot created after the fresh-label guard is eligible.
        const FString ExpectedSlot = TEXT("Rogue10m_Progression_QA_Learning_") + Label + TEXT("_") + ProfileId.ToString(EGuidFormats::Digits) + TEXT("_v5");
        Check(ProfileId.IsValid() && Saves->GetAttachedProfileId() == ProfileId && Saves->IsAttachedTo(*Character)
            && Slot == ExpectedSlot && UGameplayStatics::DoesSaveGameExist(ExpectedSlot, 0), TEXT("write_failure_fixture_targets_exact_owned_QA_profile_slot"));
        if (Failures) { return; }
        ReadOnlyFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), ExpectedSlot + TEXT(".sav"));
        TArray<uint8> Before, After;
        Check(FFileHelper::LoadFileToArray(Before, *ReadOnlyFile), TEXT("owned_QA_save_readable_before_write_failure"));
        const int32 Points = Tree->GetAvailableGrowthPoints(), Gold = Inventory->GetGold(), Count = Tree->Capture().Investments.Num();
        const auto BeforeLedger = Tree->Capture();
        Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile, true), TEXT("owned_QA_readonly_write_failure_fixture_enabled"));
        if (Failures) { return; }
        Check(!Saves->SaveNow() && !Saves->IsSaveReady() && Saves->CanRetrySave(), TEXT("failed_write_keeps_owned_session_retryable_but_not_save_ready"));
        FText Reason;
        Check(!Tree->CanLearnDuringRun() && !Tree->CanUnlockNode(SecondNode, ERoute::Points, NAME_None, Reason)
            && !Tree->TryUnlockNode(SecondNode, ERoute::Points, NAME_None), TEXT("failed_write_disables_run_learning_and_purchase"));
        Check(Points == Tree->GetAvailableGrowthPoints() && Gold == Inventory->GetGold()
            && Count == Tree->Capture().Investments.Num() && SameLearningLedger(BeforeLedger, Tree->Capture())
            && !Tree->IsNodeUnlocked(SecondNode), TEXT("save_blocked_purchase_preserves_points_gold_and_ledger"));
        Check(FFileHelper::LoadFileToArray(After, *ReadOnlyFile) && Before == After, TEXT("failed_write_preserves_prior_owned_QA_disk_bytes"));
        Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile, false), TEXT("owned_QA_readonly_fixture_released"));
        if (Failures) { return; }
        ReadOnlyFile.Reset();
        Check(Saves->GetAttachedProfileId() == ProfileId && Saves->SaveNow() && Saves->IsSaveReady()
            && Tree->CanLearnDuringRun(), TEXT("same_profile_successful_save_retry_reenables_run_learning"));
    }
    void LearnDuringAttack()
    {
        Key(EKeys::LeftMouseButton, IE_Pressed);
        // PlayerInput consumes simulated keys on a game tick, as with the existing combat QA.
        Later(0.15, [this]
        {
            Key(EKeys::LeftMouseButton, IE_Released);
            Later(0.04, [this] { VerifyAttackAndLearn(); });
        });
    }
    void VerifyAttackAndLearn()
    {
        const int32 Points = Tree->GetAvailableGrowthPoints(), Gold = Inventory->GetGold();
        Check(Combat->IsWeaponChangeBlocked() && Combat->GetLastAcceptedAttackSkill() != nullptr, TEXT("accepted_programmatic_primary_input_owns_active_attack"));
        if (Failures) { Finish(); return; }
        const auto* AcceptedSkill = Combat->GetLastAcceptedAttackSkill();
        const auto* Attributes = Character->GetRogueAttributeSet();
        Check(Attributes != nullptr, TEXT("accepted_attack_has_authoritative_attributes"));
        if (Failures) { Finish(); return; }
        const float PaidStamina = Attributes->GetStamina(), PaidMana = Attributes->GetMana();
        Check(Tree->CanLearnDuringRun() && Tree->TryUnlockNode(FirstNode, ERoute::Points, NAME_None), TEXT("first_identity_learning_is_allowed_during_active_attack"));
        Check(Tree->GetActiveIdentity() == EIdentity::InternalEnergy, TEXT("first_learned_identity_retains_automatic_activation"));
        Check(Combat->GetLastAcceptedAttackSkill() == AcceptedSkill && Combat->IsWeaponChangeBlocked(), TEXT("learning_does_not_replace_or_cancel_the_accepted_attack"));
        Check(FMath::IsNearlyEqual(PaidStamina, Attributes->GetStamina()) && FMath::IsNearlyEqual(PaidMana, Attributes->GetMana()), TEXT("first_identity_learning_does_not_charge_attack_resources_again"));
        Check(Tree->GetAvailableGrowthPoints() == Points - 1 && Inventory->GetGold() == Gold, TEXT("first_identity_consumes_one_point_and_no_gold"));
        const int32 Count = Tree->Capture().Investments.Num();
        Check(!Tree->TryUnlockNode(FirstNode, ERoute::Points, NAME_None) && Tree->GetAvailableGrowthPoints() == Points - 1
            && Tree->Capture().Investments.Num() == Count && Inventory->GetGold() == Gold, TEXT("duplicate_learning_is_noop_for_points_gold_and_ledger"));
        Later(1.5, [this] { OpenActualKWindow(); });
    }
    void OpenActualKWindow()
    {
        Key(EKeys::K, IE_Pressed); Key(EKeys::K, IE_Released);
        Later(0.5, [this]
        {
            for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
            { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()) { Widget = *It; break; } }
            Check(PC->IsSkillTreeVisible() && Widget.IsValid(), TEXT("simulated_K_opens_actual_native_tree"));
            Check(!UGameplayStatics::IsGamePaused(this->World.Get()) && Run->IsRunActive(), TEXT("field_tree_keeps_world_and_run_active"));
            if (Failures) { Finish(); return; }
            Widget->SelectNode(SecondNode); Widget->RefreshFromData();
            auto* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("HybridTreeLearnPoints")));
            Check(Button && Button->GetIsEnabled(), TEXT("actual_points_learning_button_enabled_in_field"));
            if (Failures) { Finish(); return; }
            const int32 Points = Tree->GetAvailableGrowthPoints(), Gold = Inventory->GetGold();
            Button->OnClicked.Broadcast();
            Check(Tree->IsNodeUnlocked(SecondNode) && Tree->GetAvailableGrowthPoints() == Points - 1 && Inventory->GetGold() == Gold, TEXT("actual_UMG_points_delegate_learns_second_identity_once"));
            Check(Tree->GetActiveIdentity() == EIdentity::InternalEnergy, TEXT("second_identity_does_not_replace_first_automatically"));
            FText Reason;
            const int32 RemainingPoints = Tree->GetAvailableGrowthPoints();
            Check(!Tree->CanUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Points, NAME_None, Reason)
                && !Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Points, NAME_None)
                && Tree->GetAvailableGrowthPoints() == RemainingPoints, TEXT("insufficient_points_reject_purchase_without_partial_spending"));
            Check(!Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Mastery, NAME_None), TEXT("insufficient_mastery_rejects_free_route"));
            // Existing mastery progression is a programmatic fixture, not confirmed player hits.
            for (int32 Index = 0; Index < 400; ++Index) { Tree->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::Brawler); }
            Check(Tree->GetMasteryLevel(ERogue10mSkillTreeTrack::Brawler) >= 12
                && Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Mastery, NAME_None)
                && Tree->GetAvailableGrowthPoints() == RemainingPoints, TEXT("satisfied_mastery_manual_route_learns_in_running_field_without_points"));
            Check(!Tree->RefundNode(FirstNode) && !Tree->Respec(), TEXT("field_refund_and_respec_remain_rejected"));
            Check(!Tree->TrySetActiveIdentity(EIdentity::FightingSpirit), TEXT("field_manual_identity_switch_remains_rejected"));
            Check(Tree->GetActiveIdentity() == EIdentity::InternalEnergy, TEXT("rejected_manual_switch_preserves_active_identity"));
            Check(!Town->TryUpgradeTrait(ERogue10mTownTrait::Vitality), TEXT("run_learning_does_not_enable_lobby_trait_purchase"));
            Check(Saves->GetLobbyAvailablePoints() == EntryLobby, TEXT("run_learning_never_debits_or_credits_lobby_currency"));
            Check(Saves->SaveNow() && Saves->HasDiscoveredNode(FirstNode) && Saves->HasDiscoveredNode(SecondNode), TEXT("learned_nodes_are_saved_as_independent_discovery"));
            Photo(TEXT("01_field_learning.png"));
            Later(0.7, [this] { EndRun(); });
        });
    }
    void EndRun()
    {
        PC->CloseAllBlockingPanels();
        if (Mode == TEXT("Abandon")) { Check(PC->RequestAbandonCurrentRun(), TEXT("abandon_API_accepts_checkpointed_actual_field_run")); }
        else
        {
            // Normal is a settlement fixture, not a boss-combat victory claim.
            if (Mode == TEXT("Defeat")) { Character->Die(); }
            else { Run->FinishRun(true); }
        }
        Later(0.5, [this]
        {
            Check(Run->IsRunTerminal() && Completion->IsSettled() && PC->IsRunResultVisible(), TEXT("terminal_phase_presents_actual_result_widget"));
            Check(!Tree->CanLearnDuringRun() && !Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Points, NAME_None), TEXT("terminal_result_disallows_new_learning"));
            Check(Completion->IsProgressionSaved(), TEXT("terminal_snapshot_is_durable_before_return"));
            Photo(TEXT("02_learning_result.png"));
            if (Failures) { Finish(); return; }
            Later(0.6, [this]
            {
                OldWorld = World;
                Check(Completion->ReturnToTown(), TEXT("result_component_enters_actual_town_travel"));
                if (Failures) { Finish(); return; }
                Later(0.2, [this] { WaitForWorld(TEXT("StartHub"), [this] { VerifyTown(); }); });
            });
        });
    }
    void VerifyTown()
    {
        PC = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (PC.IsValid()) { PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels(); }
        Check(Refresh() && Saves->IsSaveReady() && Saves->GetAttachedProfileId() == ProfileId, TEXT("same_profile_restores_on_real_town_return"));
        if (Failures) { Finish(); return; }
        Check(Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && !World->URL.HasOption(TEXT("StartRun")), TEXT("town_waits_for_new_portal_entry"));
        Check(!Tree->CanLearnDuringRun() && !PC->IsRunResultVisible() && !Character->IsDead(), TEXT("new_town_pawn_has_no_stale_run_learning_or_result_context"));
        Check(State->GetPlayerLevel() == 1 && State->GetCurrentExperience() == 0, TEXT("run_XP_and_level_reset"));
        Check(!Tree->IsNodeUnlocked(FirstNode) && !Tree->IsNodeUnlocked(SecondNode)
            && !Tree->IsNodeUnlocked(TEXT("I.InternalEnergy.Advanced")) && Tree->GetActiveIdentity() == EIdentity::None, TEXT("run_learned_identities_and_active_identity_reset"));
        Check(Tree->GetAvailableGrowthPoints() == 0 && Tree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler) == 0, TEXT("run_points_and_mastery_reset_with_learning"));
        Check(Saves->HasDiscoveredNode(FirstNode) && Saves->HasDiscoveredNode(SecondNode), TEXT("permanent_discovery_survives_without_regranting_run_learning"));
        Check(InventoryEntrySwordCount() == 0, TEXT("run_acquired_entry_sword_GUID_does_not_survive_completed_run"));
        Check(Saves->HasDiscoveredNode(TEXT("I.InternalEnergy.Advanced")), TEXT("mastery_route_discovery_survives_run_reset"));
        Check(Inventory->GetGold() == EntryGold && Inventory->GetCrystals() == EntryCrystals, TEXT("run_currency_returns_to_entry_or_starter_baseline"));
        if (Mode == TEXT("Abandon")) { Check(Saves->GetLobbyAvailablePoints() == EntryLobby, TEXT("abandonment_mints_no_lobby_reward")); }
        else { Check(Saves->GetLobbyAvailablePoints() == EntryLobby + (Mode == TEXT("Normal") ? 10 : 0), TEXT("only_existing_terminal_rule_changes_lobby_budget")); }
        Check(Town->GetSpentTraitPoints() == 0, TEXT("run_learning_does_not_create_permanent_town_investments"));
        Photo(TEXT("03_learning_reset_town.png")); Later(0.7, [this] { Finish(); });
    }
    void Finish()
    {
        if (bFinished) { return; } bFinished = true;
        // Assigned only after exact fresh-slot/profile ownership validation above.
        if (!ReadOnlyFile.IsEmpty())
        {
            Check(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnlyFile, false), TEXT("owned_QA_readonly_fixture_released_on_finish"));
            ReadOnlyFile.Reset();
        }
        Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nphysical_input=not_performed\nfull_field_play=not_performed\nXP_and_terminal_phase=programmatic_fixtures\n"), Checks, Failures, *Mode, bRendered);
        if (!FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, TEXT("results.txt")))) { ++Failures; }
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=RUN_LEARNING_%s checks=%d failures=%d output=%s"), Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, *Output);
        FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
    }
    TWeakObjectPtr<UWorld> World, OldWorld;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mGameState> Run;
    TWeakObjectPtr<URogue10mRunCompletionComponent> Completion;
    TWeakObjectPtr<URogue10mHybridSkillTreeWidget> Widget;
    TWeakObjectPtr<ARogue10mOpenWorldBootstrap> EntryBootstrap, DuplicateBootstrap;
    TWeakObjectPtr<ARogue10mDroppedItem> EntrySword;
    URogue10mCharacterProfileSubsystem* Profiles = nullptr;
    URogue10mProgressionSubsystem* Saves = nullptr;
    ARogue10mPlayerState* State = nullptr;
    URogue10mInventoryComponent* Inventory = nullptr;
    URogue10mCombatComponent* Combat = nullptr;
    URogue10mSkillTreeProgressionComponent* Tree = nullptr;
    URogue10mTownProgressionComponent* Town = nullptr;
    FGuid ProfileId, EntrySwordId;
    FVector PrePickupLocation = FVector::ZeroVector;
    FString Label, Mode, Output, Images, Slot, Results, ReadOnlyFile;
    TFunction<void()> Next;
    double Deadline = 0, NextAt = 0, EntrySwordDeadline = 0;
    int32 Checks = 0, Failures = 0, EntryGold = 0, EntryCrystals = 0, EntryLobby = 0;
    bool bFinished = false, bRendered = false;
};
static TSharedPtr<FRun> Active;
static void RunCommand()
{
    if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* World = Context.World(); World && Context.WorldType == EWorldType::Game)
        { Active = MakeShared<FRun>(); Active->Start(*World); return; }
    }
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=RUN_LEARNING_FAILED reason=no_game_world"));
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestRunLearning"), TEXT("Fresh QA profile, real portal, programmatic attack/UMG learning and real town return."), FConsoleCommandDelegate::CreateStatic(&RunCommand));
}
#endif
