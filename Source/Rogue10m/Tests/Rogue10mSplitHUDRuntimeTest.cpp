// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

// Full integration QA uses the separately managed progression snapshots.
// A HUD-only checkout must still build and report this prerequisite clearly.
#if __has_include("Rogue10mProgressionSubsystem.h") && __has_include("Rogue10mProgressionSaveGame.h")

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/Rogue10mBottomHUDWidget.h"
#include "Widgets/Rogue10mMainHUDWidget.h"

// Rendering integration fixture: a fresh QA profile is attached through the real
// controller, then detached without saving before any weapon/resource samples.
// Acquisition samples call the presentation API; no inventory item is granted.
namespace Rogue10mSplitHUDQA
{
struct FCase
{
    int32 Width;
    int32 Height;
    ERogue10mWeaponType Weapon;
    ERogue10mIdentityType Identity;
    ERogue10mBottomHUDTheme Theme;
    bool bManaEnabled;
    float ApplicationScale;
};
static const FCase Cases[] = {
    {1280, 720, ERogue10mWeaponType::Knuckle, ERogue10mIdentityType::StoneFist, ERogue10mBottomHUDTheme::MartialArtist, false, 1.0f},
    {1920, 1080, ERogue10mWeaponType::GreatSword, ERogue10mIdentityType::Rage, ERogue10mBottomHUDTheme::Warrior, false, 1.0f},
    {2560, 1440, ERogue10mWeaponType::Staff, ERogue10mIdentityType::Mana, ERogue10mBottomHUDTheme::Wizard, true, 1.0f},
    {1920, 800, ERogue10mWeaponType::Dagger, ERogue10mIdentityType::Focus, ERogue10mBottomHUDTheme::Rogue, false, 1.0f},
    {800, 600, ERogue10mWeaponType::Knuckle, ERogue10mIdentityType::StoneFist, ERogue10mBottomHUDTheme::MartialArtist, false, 1.25f},
    {1920, 1080, ERogue10mWeaponType::Staff, ERogue10mIdentityType::Mana, ERogue10mBottomHUDTheme::Wizard, false, 1.0f},
    {1920, 1080, ERogue10mWeaponType::Unarmed, ERogue10mIdentityType::None, ERogue10mBottomHUDTheme::MartialArtist, false, 1.0f}
};
struct FBounds
{
    FVector2D Min = FVector2D::ZeroVector;
    FVector2D Max = FVector2D::ZeroVector;
    FVector2D Size() const { return Max - Min; }
    bool Contains(const FBounds& Other, double Tolerance = 1.5) const
    {
        return Other.Min.X >= Min.X - Tolerance && Other.Min.Y >= Min.Y - Tolerance
            && Other.Max.X <= Max.X + Tolerance && Other.Max.Y <= Max.Y + Tolerance;
    }
    bool Overlaps(const FBounds& Other, double Tolerance = 0.5) const
    {
        return Min.X < Other.Max.X - Tolerance && Max.X > Other.Min.X + Tolerance
            && Min.Y < Other.Max.Y - Tolerance && Max.Y > Other.Min.Y + Tolerance;
    }
};
struct FPreservedSave { FString Filename; TArray<uint8> Bytes; };
static bool SafeSlot(const FString& Slot, const TCHAR* Prefix)
{
    if (!Slot.StartsWith(Prefix) || Slot.Len() > 80) { return false; }
    for (TCHAR Character : Slot) { if (!FChar::IsAlnum(Character) && Character != TEXT('_')) { return false; } }
    return true;
}
template <typename T> static bool Same(const T& A, const T& B)
{
    return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld& InWorld)
    {
        World = &InWorld;
        OriginalApplicationScale = GetDefault<UUserInterfaceSettings>()->ApplicationScale;
        FString ProfileSlot, SkillSlot;
        FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
        FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), SavePrefix);
        FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), SkillSlot);
        if (!SafeSlot(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
            || !SafeSlot(SavePrefix, TEXT("Rogue10m_Progression_QA_"))
            || !SafeSlot(SkillSlot, TEXT("Rogue10m_Fighter_QA_")))
        {
            Check(false, TEXT("three_isolated_save_flags_required")); Finish(); return;
        }
        Evidence = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
            TEXT("Feature/doc/evidence/split-hud-20261014"), ProfileSlot));
        Check(IFileManager::Get().MakeDirectory(*Evidence, true), TEXT("evidence_directory_available"));
        const FString SaveRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(SaveRoot, TEXT("*.sav")), true, false);
        for (const FString& File : Files)
        {
            FPreservedSave& Saved = PreservedSaves.AddDefaulted_GetRef();
            Saved.Filename = FPaths::Combine(SaveRoot, File);
            Check(FFileHelper::LoadFileToArray(Saved.Bytes, *Saved.Filename), TEXT("existing_save_bytes_read_before_fixture"));
        }
        if (Failures) { Finish(); return; }
        Schedule(4.0f, [this]() { Prepare(); });
    }
private:
    void Schedule(float Delay, TFunction<void()> Callback)
    {
        if (!World.IsValid()) { Check(false, TEXT("disposable_world_lost")); Finish(); return; }
        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle,
            FTimerDelegate::CreateLambda([Self = AsShared(), Callback = MoveTemp(Callback)]()
            { if (!Self->bFinished && Self->World.IsValid()) { Callback(); } }), Delay, false);
    }
    void Check(bool Condition, const FString& Reason)
    {
        ++Assertions;
        if (!Condition) { ++Failures; }
        Report += FString::Printf(TEXT("%s case=%d %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), Index, *Reason);
        UE_LOG(LogRogue10m, Display, TEXT("SPLIT_COMBAT_HUD %s case=%d %s"),
            Condition ? TEXT("PASS") : TEXT("FAIL"), Index, *Reason);
    }
    FBounds Bounds(UWidget* Widget, const FGeometry& Root)
    {
        if (!Widget) { Check(false, TEXT("required_rendered_widget_missing")); return {}; }
        const FGeometry& Geometry = Widget->GetCachedGeometry();
        FBounds Result;
        Result.Min = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
        Result.Max = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
        Check(Result.Size().X > 0.0 && Result.Size().Y > 0.0, Widget->GetName() + TEXT("_paint_geometry_nonempty"));
        return Result;
    }
    URogue10mMainHUDWidget* FindHUD() const
    {
        for (TObjectIterator<URogue10mMainHUDWidget> It; It; ++It)
        {
            if (It->GetWorld() == World.Get() && It->IsInViewport()) { return *It; }
        }
        return nullptr;
    }
    void Prepare()
    {
        Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
        Check(Controller.IsValid() && Profiles && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_session"));
        if (Failures) { Finish(); return; }
        FRogue10mCharacterProfile Profile;
        Check(Profiles->CreateProfile(TEXT("SplitHudQa"), FRogue10mCharacterAppearance(), Profile)
            && Profiles->SelectProfile(Profile.ProfileId), TEXT("actual_QA_profile_created_and_selected"));
        if (Failures) { Finish(); return; }
        Controller->EnterSelectedCharacter();
        Controller->CloseAllBlockingPanels();
        Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
        State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
        Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
        auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
        Check(Character.IsValid() && State.IsValid() && Combat.IsValid() && Character->GetInventoryComponent()
            && Persistence && Persistence->IsSaveReady() && Persistence->IsAttachedTo(*Character)
            && Persistence->GetAttachedProfileId() == Profile.ProfileId
            && Persistence->GetSaveSlotForProfile(Profile.ProfileId).StartsWith(SavePrefix + TEXT("_")),
            TEXT("production_profile_attach_uses_isolated_GUID_and_slot"));
        if (Failures) { Finish(); return; }
        FPreservedSave& QaBaseline = PreservedSaves.AddDefaulted_GetRef();
        QaBaseline.Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"),
            Persistence->GetSaveSlotForProfile(Profile.ProfileId) + TEXT(".sav"));
        Check(FFileHelper::LoadFileToArray(QaBaseline.Bytes, *QaBaseline.Filename), TEXT("actual_attached_QA_progression_baseline_captured"));
        Persistence->DetachCharacter(*Character, false);
        Check(!Persistence->IsAttachedTo(*Character) && !Persistence->CanRetrySave(), TEXT("samples_detached_before_runtime_mutations"));
        OriginalInventory = Character->GetInventoryComponent()->CaptureInventory();
        OriginalCombat = Combat->CaptureCombat();
        OriginalGrowth = State->CaptureGrowth();
        OriginalIdentityType = State->GetIdentityType();
        bOriginalManaEnabled = State->IsManaEnabled();
        bHaveDomainBaseline = true;
        Character->SetCanBeDamaged(false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement();
        if (auto* Regeneration = Character->FindComponentByClass<URogue10mVitalRegenerationComponent>())
        {
            Regeneration->ConfigureRegeneration(0.0f, 0.0f, 0.0f);
        }
        for (const TCHAR* Name : {TEXT("NoviceRing"), TEXT("LeatherBoots"), TEXT("LeatherArmor")})
        {
            const FString Path = FString::Printf(TEXT("/Game/DataAsset/Item/Starter/DA_Item_%s.DA_Item_%s"), Name, Name);
            URogue10mItemDataAsset* Item = LoadObject<URogue10mItemDataAsset>(nullptr, *Path);
            Check(Item && Item->InventoryIcon.LoadSynchronous(), FString(Name) + TEXT("_real_acquisition_icon_available"));
            Items.Add(Item);
        }
        if (ARogue10mGameState* RunState = World->GetGameState<ARogue10mGameState>()) { RunState->StartRun(); }
        Next();
    }
    void SetResources(bool bUpdated)
    {
        URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
        Check(Attributes != nullptr, TEXT("actual_GAS_resource_attributes_exist"));
        if (!Attributes) { return; }
        Attributes->SetHealth(Attributes->GetMaxHealth() * (bUpdated ? 0.42f : 0.76f));
        Attributes->SetStamina(Attributes->GetMaxStamina() * (bUpdated ? 0.83f : 0.58f));
        Attributes->SetMana(Attributes->GetMaxMana() * (bUpdated ? 0.64f : 0.43f));
        Attributes->SetExperience(Attributes->GetExperienceToNextLevel() * (bUpdated ? 0.71f : 0.27f));
        const float IdentityFraction = bUpdated ? (Index == 2 ? 1.0f : 0.72f) : (Index == 0 ? 0.0f : 0.37f);
        Attributes->SetIdentity(Attributes->GetMaxIdentity() * IdentityFraction);
    }
    void Next()
    {
        if (Index >= UE_ARRAY_COUNT(Cases)) { Finish(); return; }
        const FCase& Case = Cases[Index];
        GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = Case.ApplicationScale;
        Character->SetEquippedWeaponType(Case.Weapon);
        State->SetIdentityType(Case.Identity);
        State->SetManaEnabled(Case.bManaEnabled);
        SetResources(false);
        GEngine->Exec(World.Get(), *FString::Printf(TEXT("r.SetRes %dx%dw"), Case.Width, Case.Height));
        Schedule(2.5f, [this]() { PrepareData(); });
    }
    void PrepareData()
    {
        URogue10mMainHUDWidget* HUD = FindHUD();
        Check(HUD != nullptr, TEXT("production_MainHUD_on_player_screen"));
        if (!HUD) { Finish(); return; }
        HUD->RefreshBoundWidgetData();
        InitialHealth = HUD->GetHealthView().Current;
        InitialIdentity = HUD->GetIdentityView().Normalized;
        InspectResources(HUD, false);
        SetResources(true);
        for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
        {
            if (Items[ItemIndex].IsValid()) { Controller->AddItemAcquisitionItem(Items[ItemIndex].Get(), ItemIndex + 1, 2.0f); }
        }
        Controller->AddCombatLogMessage(TEXT("Split HUD QA 로그"), FLinearColor::White, 2.0f);
        HUD->RefreshBoundWidgetData();
        Schedule(0.25f, [this]() { Inspect(); });
    }
    void InspectVital(URogue10mBottomHUDWidget* Bottom, const TCHAR* Name,
        const FRogue10mHudVitalView& Expected)
    {
        auto* Vital = Cast<URogue10mVitalBarWidget>(Bottom->GetWidgetFromName(Name));
        Check(Vital && Vital->IsVisible(), FString(Name) + TEXT("_rendered"));
        if (!Vital) { return; }
        const auto Actual = Vital->GetVitalView();
        Check(FMath::IsNearlyEqual(Actual.Current, Expected.Current, 0.01f)
            && FMath::IsNearlyEqual(Actual.Max, Expected.Max, 0.01f)
            && FMath::IsNearlyEqual(Actual.Normalized, Expected.Normalized, 0.001f), FString(Name) + TEXT("_actual_getter_matches_live_GAS"));
        auto* Bar = Cast<UProgressBar>(Vital->GetWidgetFromName(TEXT("UI_ValueProgressBar")));
        auto* Value = Cast<UTextBlock>(Vital->GetWidgetFromName(TEXT("UI_ValueText")));
        Check(Bar && FMath::IsNearlyEqual(Bar->GetPercent(), Expected.Normalized, 0.001f), FString(Name) + TEXT("_rendered_fill_matches_live_getter"));
        Check(Value && Value->GetText().ToString().Contains(Expected.ValueText.ToString()), FString(Name) + TEXT("_rendered_current_and_max_numbers_live"));
    }
    void InspectResources(URogue10mMainHUDWidget* HUD, bool bUpdated)
    {
        auto* Bottom = Cast<URogue10mBottomHUDWidget>(HUD->GetWidgetFromName(TEXT("BottomHUDWidget")));
        Check(Bottom && Bottom->GetResolvedTheme() == Cases[Index].Theme, TEXT("live_weapon_resolves_expected_class_theme"));
        if (!Bottom) { return; }
        const URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
        Check(Attributes && FMath::IsNearlyEqual(HUD->GetHealthView().Current, Attributes->GetHealth(), 0.01f)
            && FMath::IsNearlyEqual(HUD->GetStaminaView().Current, Attributes->GetStamina(), 0.01f), TEXT("MainHUD_getters_read_actual_player_resources"));
        InspectVital(Bottom, TEXT("HealthBarWidget"), HUD->GetHealthView());
        const bool bUseMana = Cases[Index].Theme == ERogue10mBottomHUDTheme::Wizard && Cases[Index].bManaEnabled;
        InspectVital(Bottom, bUseMana ? TEXT("ManaBarWidget") : TEXT("StaminaBarWidget"), bUseMana ? HUD->GetManaView() : HUD->GetStaminaView());
        UWidget* Inactive = Bottom->GetWidgetFromName(bUseMana ? TEXT("StaminaBarWidget") : TEXT("ManaBarWidget"));
        Check(Inactive && !Inactive->IsVisible(), TEXT("mana_replacement_has_one_active_resource_row"));
        if (bUseMana) { Check(Attributes && FMath::IsNearlyEqual(HUD->GetManaView().Current, Attributes->GetMana(), 0.01f), TEXT("mana_getter_reads_actual_mana")); }
        if (Index == 5) { Check(!HUD->GetManaView().bVisible, TEXT("wizard_without_mana_falls_back_to_stamina")); }
        auto* Progression = Cast<URogue10mProgressionWidget>(Bottom->GetWidgetFromName(TEXT("ProgressionWidget")));
        auto* XP = Progression ? Cast<UProgressBar>(Progression->GetWidgetFromName(TEXT("UI_ExperienceBar"))) : nullptr;
        const auto Progress = HUD->GetProgressionView();
        Check(Progression && FMath::IsNearlyEqual(Progression->GetProgressionView().CurrentExperience, State->GetCurrentExperience(), 0.01f)
            && XP && FMath::IsNearlyEqual(XP->GetPercent(), Progress.ExperienceNormalized, 0.001f), TEXT("experience_fill_reads_independent_actual_growth"));
        auto* Level = Cast<UTextBlock>(Bottom->GetWidgetFromName(TEXT("UI_LevelText")));
        Check(Level && Level->GetText().ToString().Contains(FString::FromInt(State->GetPlayerLevel())), TEXT("level_label_reads_actual_player_level"));
        auto* Identity = Cast<URogue10mIdentityWidget>(Bottom->GetWidgetFromName(TEXT("IdentityWidget")));
        const auto Expected = HUD->GetIdentityView();
        Check(Identity && Expected.WeaponType == Cases[Index].Weapon && Expected.IdentityType == Cases[Index].Identity
            && Expected.bHasIdentityResource == (Cases[Index].Identity != ERogue10mIdentityType::None), TEXT("identity_getter_reads_weapon_and_resource_type"));
        if (Identity)
        {
            const auto Actual = Identity->GetIdentityView();
            Check(Actual.WeaponType == Expected.WeaponType && Actual.IdentityType == Expected.IdentityType
                && FMath::IsNearlyEqual(Actual.Current, Expected.Current, 0.01f)
                && FMath::IsNearlyEqual(Actual.Normalized, State->GetIdentityNormalized(), 0.001f), TEXT("identity_widget_consumes_actual_player_state_getter"));
            auto* Percent = Cast<UTextBlock>(Identity->GetWidgetFromName(TEXT("UI_IdentityPercentText")));
            const FString ExpectedText = Expected.bHasIdentityResource ? FString::Printf(TEXT("%.0f%%"), Expected.Normalized * 100.0f) : TEXT("--");
            Check(Percent && Percent->GetText().ToString() == ExpectedText, TEXT("identity_percent_or_no_resource_label_live"));
        }
        if (bUpdated)
        {
            Check(!FMath::IsNearlyEqual(HUD->GetHealthView().Current, InitialHealth, 0.01f), TEXT("second_resource_sample_updates_cached_health"));
            if (Expected.bHasIdentityResource) { Check(!FMath::IsNearlyEqual(Expected.Normalized, InitialIdentity, 0.001f), TEXT("second_resource_sample_updates_identity_fill")); }
        }
    }
    void InspectSlots(URogue10mMainHUDWidget* HUD, URogue10mBottomHUDWidget* Bottom,
        bool bSkills, const FBounds& Right, const FGeometry& Root)
    {
        auto* Container = Cast<UPanelWidget>(Bottom->GetWidgetFromName(bSkills ? TEXT("SkillSlotContainer") : TEXT("ItemSlotContainer")));
        const auto Views = bSkills ? HUD->GetSkillQuickSlotViews() : HUD->GetItemQuickSlotViews();
        Check(Container && Container->GetChildrenCount() == (bSkills ? 5 : 4)
            && Views.Num() == (bSkills ? 5 : 4), TEXT("production_skill5_item4_slot_counts_preserved"));
        if (!Container) { return; }
        const FBounds Group = Bounds(Container, Root);
        Check(Right.Contains(Group), TEXT("actual_quick_slot_group_inside_right_panel"));
        double PreviousRight = Group.Min.X;
        for (int32 ChildIndex = 0; ChildIndex < Container->GetChildrenCount(); ++ChildIndex)
        {
            auto* Slot = Cast<URogue10mQuickSlotWidget>(Container->GetChildAt(ChildIndex));
            Check(Slot && Views.IsValidIndex(ChildIndex), TEXT("real_quick_slot_and_view_exist"));
            if (!Slot || !Views.IsValidIndex(ChildIndex)) { continue; }
            const FBounds SlotBounds = Bounds(Slot, Root);
            Check(Group.Contains(SlotBounds) && SlotBounds.Min.X >= PreviousRight - 0.5, TEXT("actual_slots_not_clipped_or_overlapped"));
            PreviousRight = SlotBounds.Max.X;
            const auto Actual = Slot->GetQuickSlotView();
            const auto Expected = Views[ChildIndex];
            Check(Actual.bUnlocked == Expected.bUnlocked && Actual.SkillData == Expected.SkillData
                && Actual.InputSlot == Expected.InputSlot && Actual.bDodgeSlot == Expected.bDodgeSlot
                && Actual.InputText.EqualTo(Expected.InputText), TEXT("slot_getter_preserves_production_bindings"));
            auto* Key = Cast<UTextBlock>(Slot->GetWidgetFromName(TEXT("UI_KeyText")));
            if (bSkills) { Check(Key && SlotBounds.Contains(Bounds(Key, Root)), TEXT("real_skill_input_label_inside_slot")); }
            else { Check(!Key || !Key->IsVisible() || Key->GetText().IsEmpty(), TEXT("item_placeholders_do_not_invent_skill_input_keys")); }
            if (bSkills && Key)
            {
                FString Display = Key->GetText().ToString();
                Display.ReplaceInline(TEXT("\n"), TEXT("")); Display.ReplaceInline(TEXT("\r"), TEXT("")); Display.ReplaceInline(TEXT(" "), TEXT(""));
                const TCHAR* Keys[] = {TEXT("LMB"), TEXT("RMB"), TEXT("HOLDLMB"), TEXT("HOLDRMB"), TEXT("E")};
                Check(Display == Keys[ChildIndex], TEXT("actual_five_skill_keys_LMB_RMB_HOLDLMB_HOLDRMB_E"));
                Check(Key->GetCachedGeometry().GetLocalSize().Y + 1.0 >= Key->GetDesiredSize().Y
                    && Key->GetCachedGeometry().GetLocalSize().X + 1.0 >= Key->GetDesiredSize().X,
                    TEXT("input_label_desired_width_and_height_not_clipped"));
                if (ChildIndex < 4)
                {
                    const ERogue10mAttackInputSlot InputSlots[] = {ERogue10mAttackInputSlot::Primary, ERogue10mAttackInputSlot::Special,
                        ERogue10mAttackInputSlot::ChargedPrimary, ERogue10mAttackInputSlot::ChargedSpecial};
                    Check(!Actual.bDodgeSlot && Actual.InputSlot == InputSlots[ChildIndex]
                        && Actual.SkillData == Combat->GetEquippedSkill(InputSlots[ChildIndex]), TEXT("displayed_skill_slot_matches_actual_combat_binding"));
                }
                else { Check(Actual.bDodgeSlot && Expected.InputText.ToString() == TEXT("E"), TEXT("fifth_slot_remains_actual_E_dodge")); }
                auto* Icon = Cast<UImage>(Slot->GetWidgetFromName(TEXT("UI_IconImage")));
                if (Expected.SkillIcon)
                {
                    Check(Actual.SkillIcon == Expected.SkillIcon && Icon && Icon->GetBrush().GetResourceObject() == Expected.SkillIcon.Get(),
                        TEXT("real_skill_icon_not_replaced_by_sample_art"));
                }
            }
            auto* Lock = Slot->GetWidgetFromName(TEXT("UI_LockGlyph"));
            Check(Lock && Lock->IsVisible() == !Actual.bUnlocked, TEXT("real_slot_lock_visibility_matches_binding"));
        }
    }
    void InspectFeed(URogue10mMainHUDWidget* HUD, const FBounds& Viewport,
        const FBounds& Left, const FBounds& Right, const FGeometry& Root)
    {
        auto* Feed = Cast<UPanelWidget>(HUD->GetWidgetFromName(TEXT("ItemAcquisitionContainer")));
        const auto Views = HUD->GetItemAcquisitionEntries();
        Check(Feed && Feed->IsVisible() && Feed->GetChildrenCount() == 3 && Views.Num() == 3, TEXT("actual_snapshot_acquisition_feed_has_three_rows"));
        if (!Feed) { return; }
        const FBounds FeedBounds = Bounds(Feed, Root);
        Check(Viewport.Contains(FeedBounds) && !FeedBounds.Overlaps(Left) && !FeedBounds.Overlaps(Right), TEXT("acquisition_feed_inside_viewport_and_clear_of_both_panels"));
        double PreviousBottom = FeedBounds.Min.Y;
        for (int32 ChildIndex = 0; ChildIndex < Feed->GetChildrenCount(); ++ChildIndex)
        {
            auto* Line = Cast<URogue10mLogLineWidget>(Feed->GetChildAt(ChildIndex));
            Check(Line && Views.IsValidIndex(ChildIndex), TEXT("real_acquisition_line_exists"));
            if (!Line || !Views.IsValidIndex(ChildIndex)) { continue; }
            const auto Expected = Views[ChildIndex];
            auto* Name = Cast<UTextBlock>(Line->GetWidgetFromName(TEXT("UI_ItemNameText")));
            auto* Quantity = Cast<UTextBlock>(Line->GetWidgetFromName(TEXT("UI_ItemCountText")));
            auto* Icon = Cast<UImage>(Line->GetWidgetFromName(TEXT("UI_ItemIconImage")));
            Check(Name && Name->GetText().EqualTo(Expected.Message)
                && Quantity && Quantity->GetText().ToString().Contains(FString::FromInt(Expected.Quantity))
                && Icon && Icon->GetBrush().GetResourceObject() == Expected.ItemIcon.Get(), TEXT("acquisition_name_quantity_icon_are_actual_snapshot"));
            const FBounds Card = Bounds(Line->GetWidgetFromName(TEXT("UI_AcquisitionCard")), Root);
            Check(FeedBounds.Contains(Card) && Viewport.Contains(Card) && Card.Min.Y >= PreviousBottom - 0.5
                && !Card.Overlaps(Left) && !Card.Overlaps(Right), TEXT("acquisition_cards_not_clipped_or_overlapping_controls"));
            PreviousBottom = Card.Max.Y;
        }
        Check(Same(OriginalInventory, Character->GetInventoryComponent()->CaptureInventory()), TEXT("sample_feed_does_not_grant_or_mutate_inventory"));
    }
    void Inspect()
    {
        URogue10mMainHUDWidget* HUD = FindHUD();
        if (!HUD) { Check(false, TEXT("MainHUD_lost")); Finish(); return; }
        HUD->RefreshBoundWidgetData();
        InspectResources(HUD, true);
        auto* Bottom = Cast<URogue10mBottomHUDWidget>(HUD->GetWidgetFromName(TEXT("BottomHUDWidget")));
        Check(Bottom && Bottom->IsSplitCombatLayout(), TEXT("applied_asset_uses_split_combat_layout"));
        if (!Bottom || !World->GetGameViewport() || !World->GetGameViewport()->Viewport) { Finish(); return; }
        const FGeometry& Root = HUD->GetCachedGeometry();
        const FVector2D RootSize = Root.GetLocalSize();
        const FIntPoint Pixels = World->GetGameViewport()->Viewport->GetSizeXY();
        Check(Pixels == FIntPoint(Cases[Index].Width, Cases[Index].Height), TEXT("actual_render_viewport_matches_requested_pixels"));
        Check(FMath::IsNearlyEqual(GetDefault<UUserInterfaceSettings>()->ApplicationScale, Cases[Index].ApplicationScale, 0.001f),
            TEXT("requested_ApplicationScale_applied_once"));
        FBounds Viewport; Viewport.Max = RootSize;
        const FBounds Left = Bounds(Bottom->GetWidgetFromName(TEXT("UI_LeftCombatDesignSize")), Root);
        const FBounds Right = Bounds(Bottom->GetWidgetFromName(TEXT("UI_RightCombatDesignSize")), Root);
        const FBox2D ExpectedLeft = Bottom->GetResponsiveLeftCombatBounds(RootSize);
        const FBox2D ExpectedRight = Bottom->GetResponsiveRightCombatBounds(RootSize);
        Check(Left.Min.Equals(ExpectedLeft.Min, 1.5) && Left.Max.Equals(ExpectedLeft.Max, 1.5)
            && Right.Min.Equals(ExpectedRight.Min, 1.5) && Right.Max.Equals(ExpectedRight.Max, 1.5), TEXT("runtime_layout_bounds_match_actual_painted_panels"));
        Check(Viewport.Contains(Left) && Viewport.Contains(Right), TEXT("both_combat_panels_inside_actual_viewport"));
        Check(FMath::Abs(Left.Min.X - 24.0) < 1.5 && FMath::Abs(RootSize.X - Right.Max.X - 24.0) < 1.5,
            TEXT("left_and_right_panels_keep_logical_side_margins"));
        Check(FMath::Abs(RootSize.Y - Left.Max.Y - 24.0) < 1.5 && FMath::Abs(RootSize.Y - Right.Max.Y - 24.0) < 1.5,
            TEXT("both_panels_keep_logical_bottom_margins"));
        Check(Right.Min.X - Left.Max.X >= 94.5 && Left.Max.X < RootSize.X * 0.5
            && Right.Min.X > RootSize.X * 0.5, TEXT("center_playfield_gap_at_least_96_and_split_sides_preserved"));
        const double LeftScale = Left.Size().X / 560.0;
        const double RightScale = Right.Size().X / 600.0;
        Check(FMath::Abs(LeftScale - RightScale) < 0.003 && LeftScale <= 1.001
            && FMath::Abs(Left.Size().Y - 180.0 * LeftScale) < 1.5
            && FMath::Abs(Right.Size().Y - 270.0 * RightScale) < 1.5, TEXT("shared_down_only_scale_preserves_both_panel_aspects"));
        if (Index == 4) { Check(LeftScale < 0.999, TEXT("800x600_ApplicationScale125_exercises_responsive_downscale")); }
        for (const TCHAR* Name : {TEXT("HealthBarWidget"), Cases[Index].bManaEnabled ? TEXT("ManaBarWidget") : TEXT("StaminaBarWidget"),
                                  TEXT("ProgressionWidget"), TEXT("UI_LevelText")})
        {
            UWidget* Part = Bottom->GetWidgetFromName(Name);
            Check(Part && Part->IsVisible() && Left.Contains(Bounds(Part, Root)), FString(Name) + TEXT("_inside_left_panel"));
        }
        auto* Identity = Cast<URogue10mIdentityWidget>(Bottom->GetWidgetFromName(TEXT("IdentityWidget")));
        Check(Identity && Right.Contains(Bounds(Identity, Root)), TEXT("actual_identity_inside_right_panel"));
        if (Identity)
        {
            const FBounds IdentityBounds = Bounds(Identity, Root);
            Check(FMath::Abs(IdentityBounds.Size().X - IdentityBounds.Size().Y) < 1.5,
                TEXT("identity_image_and_circular_gauge_not_distorted"));
            auto* Icon = Cast<UImage>(Identity->GetWidgetFromName(TEXT("UI_IdentityIcon")));
            auto* Fallback = Cast<UImage>(Identity->GetWidgetFromName(TEXT("UI_IdentityFallbackIcon")));
            const auto Displayed = Identity->GetDisplayedIconTexture();
            Check(Icon && Fallback, TEXT("actual_identity_image_and_fallback_widgets_exist"));
            if (Displayed.IsValid())
            {
                Check(Icon && Fallback && Icon->IsVisible() && !Fallback->IsVisible()
                    && Icon->GetBrush().GetResourceObject() == Displayed.Get()
                    && Right.Contains(Bounds(Icon, Root)), TEXT("identity_rendered_image_matches_actual_displayed_texture_getter"));
            }
            else
            {
                Check(Fallback && Fallback->IsVisible() && Icon && !Icon->IsVisible()
                    && Right.Contains(Bounds(Fallback, Root)), TEXT("unconfigured_identity_icon_has_visible_fallback"));
            }
            const bool bFist = Cases[Index].Weapon == ERogue10mWeaponType::Unarmed || Cases[Index].Weapon == ERogue10mWeaponType::Knuckle;
            const bool bSword = Cases[Index].Weapon == ERogue10mWeaponType::GreatSword;
            if (bFist || bSword)
            {
                const FString ExpectedPath = bFist ? TEXT("/Game/UI/HUD/Split/T_Identity_Unarmed.T_Identity_Unarmed")
                    : TEXT("/Game/UI/HUD/Split/T_Identity_GreatSword.T_Identity_GreatSword");
                Check(Displayed.IsValid() && Displayed.ToSoftObjectPath().ToString() == ExpectedPath
                    && Icon && Icon->GetBrush().GetResourceObject() == Displayed.Get(), TEXT("approved_class_identity_art_is_bound_and_rendered"));
                ApprovedIdentityTexturePaths.Add(Cases[Index].Theme, Displayed.ToSoftObjectPath().ToString());
            }
            else { Check(Displayed == Identity->GetIdentityView().IconTexture, TEXT("other_class_identity_uses_its_existing_resource_icon")); }
            auto* Name = Cast<UTextBlock>(Identity->GetWidgetFromName(TEXT("UI_WeaponNameText")));
            Check(Name && !Name->GetText().IsEmpty() && Right.Contains(Bounds(Name, Root)), TEXT("actual_identity_weapon_name_not_blank_or_clipped"));
        }
        InspectSlots(HUD, Bottom, true, Right, Root);
        InspectSlots(HUD, Bottom, false, Right, Root);
        UWidget* Skills = Bottom->GetWidgetFromName(TEXT("SkillSlotContainer"));
        UWidget* ItemSlots = Bottom->GetWidgetFromName(TEXT("ItemSlotContainer"));
        if (Skills && ItemSlots && Identity)
        {
            const FBounds SkillsBounds = Bounds(Skills, Root), ItemsBounds = Bounds(ItemSlots, Root), IdentityBounds = Bounds(Identity, Root);
            Check(!SkillsBounds.Overlaps(ItemsBounds) && !SkillsBounds.Overlaps(IdentityBounds)
                && !ItemsBounds.Overlaps(IdentityBounds), TEXT("right_skill_item_identity_groups_do_not_overlap"));
        }
        InspectFeed(HUD, Viewport, Left, Right, Root);
        const FString Filename = FString::Printf(TEXT("SplitCombatHUD_%02d_%dx%d.png"), Index, Pixels.X, Pixels.Y);
        ScreenshotPaths.Add(FPaths::Combine(Evidence, Filename));
        FScreenshotRequest::RequestScreenshot(ScreenshotPaths.Last(), true, false);
        UE_LOG(LogRogue10m, Display, TEXT("SPLIT_COMBAT_HUD CASE=%d pixels=%dx%d logical=%.1fx%.1f scale=%.3f gap=%.1f capture=%s"),
            Index, Pixels.X, Pixels.Y, RootSize.X, RootSize.Y, LeftScale, Right.Min.X - Left.Max.X, *ScreenshotPaths.Last());
        Schedule(2.2f, [this]() { InspectExpiredFeed(); ++Index; Next(); });
    }
    void InspectExpiredFeed()
    {
        URogue10mMainHUDWidget* HUD = FindHUD();
        if (!HUD) { Check(false, TEXT("HUD_lost_after_capture")); return; }
        HUD->RefreshBoundWidgetData();
        auto* Feed = Cast<UPanelWidget>(HUD->GetWidgetFromName(TEXT("ItemAcquisitionContainer")));
        Check(HUD->GetItemAcquisitionEntries().IsEmpty() && Feed && Feed->GetChildrenCount() == 0 && !Feed->IsVisible(),
            TEXT("expired_acquisition_snapshot_rows_removed"));
        Check(!ScreenshotPaths.IsEmpty() && IFileManager::Get().FileSize(*ScreenshotPaths.Last()) > 24,
            TEXT("actual_game_render_capture_saved"));
    }
    void Finish()
    {
        if (bFinished) { return; }
        bFinished = true;
        GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = OriginalApplicationScale;
        if (bHaveDomainBaseline && Character.IsValid() && Combat.IsValid())
        {
            // Switching the sample weapon activates real weapon profiles. Restore the
            // captured domains in this detached disposable session before comparing.
            Character->SetEquippedWeaponType(OriginalGrowth.WeaponType);
            Check(State->RestoreGrowth(OriginalGrowth) && Combat->RestoreCombat(OriginalCombat), TEXT("weapon_samples_restore_captured_growth_and_combat_snapshots"));
            State->SetIdentityType(OriginalIdentityType);
            State->SetManaEnabled(bOriginalManaEnabled);
            Check(Same(OriginalInventory, Character->GetInventoryComponent()->CaptureInventory())
                && Same(OriginalCombat, Combat->CaptureCombat()) && Same(OriginalGrowth, State->CaptureGrowth()),
                TEXT("render_samples_preserve_captured_inventory_growth_and_skill_progression"));
        }
        if (Index == UE_ARRAY_COUNT(Cases))
        {
            TSet<FString> UniquePaths;
            for (const auto& Pair : ApprovedIdentityTexturePaths) { UniquePaths.Add(Pair.Value); }
            Check(ApprovedIdentityTexturePaths.Num() == 2 && UniquePaths.Num() == 2, TEXT("approved_unarmed_and_greatsword_images_are_distinct"));
        }
        for (const FPreservedSave& Saved : PreservedSaves)
        {
            TArray<uint8> Current;
            Check(FFileHelper::LoadFileToArray(Current, *Saved.Filename) && Current == Saved.Bytes, TEXT("pre_existing_save_remains_byte_identical"));
        }
        const bool bPassed = Failures == 0 && Index == UE_ARRAY_COUNT(Cases);
        Report += FString::Printf(TEXT("RESULT=SPLIT_COMBAT_HUD_%s cases=%d assertions=%d failures=%d\n"),
            bPassed ? TEXT("PASSED") : TEXT("FAILED"), Index, Assertions, Failures);
        const bool bReportSaved = !Evidence.IsEmpty()
            && FFileHelper::SaveStringToFile(Report, *FPaths::Combine(Evidence, TEXT("runtime-contract.txt")));
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=SPLIT_COMBAT_HUD_%s cases=%d assertions=%d failures=%d report=%d"),
            bPassed && bReportSaved ? TEXT("PASSED") : TEXT("FAILED"), Index, Assertions, Failures, bReportSaved);
        FPlatformMisc::RequestExitWithStatus(false, bPassed && bReportSaved ? 0 : 1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> Controller;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<ARogue10mPlayerState> State;
    TWeakObjectPtr<URogue10mCombatComponent> Combat;
    TArray<TWeakObjectPtr<URogue10mItemDataAsset>> Items;
    TMap<ERogue10mBottomHUDTheme, FString> ApprovedIdentityTexturePaths;
    TArray<FPreservedSave> PreservedSaves;
    TArray<FString> ScreenshotPaths;
    FRogue10mInventorySnapshot OriginalInventory;
    FRogue10mCombatSnapshot OriginalCombat;
    FRogue10mGrowthSnapshot OriginalGrowth;
    ERogue10mIdentityType OriginalIdentityType = ERogue10mIdentityType::None;
    FString SavePrefix, Evidence, Report;
    int32 Index = 0, Failures = 0, Assertions = 0;
    float OriginalApplicationScale = 1.0f, InitialHealth = 0.0f, InitialIdentity = 0.0f;
    bool bHaveDomainBaseline = false, bFinished = false, bOriginalManaEnabled = false;
};
static TSharedPtr<FRun> ActiveRun;
static void Run()
{
    if (GIsEditor || !GEngine || ActiveRun.IsValid())
    {
        UE_LOG(LogRogue10m, Error, TEXT("RESULT=SPLIT_COMBAT_HUD_FAILED reason=fresh_disposable_game_process_required"));
        return;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (World && World->WorldType == EWorldType::Game && World->GetNetMode() == NM_Standalone)
        {
            ActiveRun = MakeShared<FRun>(); ActiveRun->Start(*World); return;
        }
    }
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=SPLIT_COMBAT_HUD_FAILED reason=standalone_game_world_required"));
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestSplitCombatHUD"),
    TEXT("Checks the applied split HUD through live getters and paint geometry in seven isolated viewport/resource cases, captures PNGs and exits."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#else
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Rogue10m.h"

namespace Rogue10mSplitHUDQA
{
static void ReportMissingProgression()
{
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=SPLIT_COMBAT_HUD_FAILED reason=progression_integration_prerequisite_missing"));
    if (GIsEditor || !GEngine) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (World && World->WorldType == EWorldType::Game && World->GetNetMode() == NM_Standalone)
        {
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return;
        }
    }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestSplitCombatHUD"),
    TEXT("Requires the separate progression integration to verify player snapshot preservation."),
    FConsoleCommandDelegate::CreateStatic(&ReportMissingProgression));
}
#endif
#endif
