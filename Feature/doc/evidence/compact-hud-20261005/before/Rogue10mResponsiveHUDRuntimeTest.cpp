// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mGameState.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/Rogue10mBottomHUDWidget.h"
#include "Widgets/Rogue10mMainHUDWidget.h"

namespace Rogue10mResponsiveHUDTest
{
struct FCase { int32 Width; int32 Height; ERogue10mWeaponType Weapon; };
static const FCase Cases[] = {
    {1280, 720, ERogue10mWeaponType::Knuckle},
    {1920, 1080, ERogue10mWeaponType::GreatSword},
    {2560, 1440, ERogue10mWeaponType::Staff},
    {3440, 1440, ERogue10mWeaponType::Dagger},
    {800, 900, ERogue10mWeaponType::Knuckle},
    {1920, 1080, ERogue10mWeaponType::Knuckle},
    {1024, 768, ERogue10mWeaponType::Knuckle}
};

struct FBounds
{
    FVector2D Min = FVector2D::ZeroVector;
    FVector2D Max = FVector2D::ZeroVector;
    FVector2D Size() const { return Max - Min; }
    bool Contains(const FBounds& B, double Tolerance = 1.0) const
    {
        return B.Min.X >= Min.X - Tolerance && B.Min.Y >= Min.Y - Tolerance
            && B.Max.X <= Max.X + Tolerance && B.Max.Y <= Max.Y + Tolerance;
    }
};

class FRun : public TSharedFromThis<FRun>
{
public:
    void Start(UWorld* InWorld)
    {
        World = InWorld;
        if (ARogue10mGameState* State = InWorld->GetGameState<ARogue10mGameState>()) { State->StartRun(); }
        OriginalApplicationScale = GetDefault<UUserInterfaceSettings>()->ApplicationScale;
        Schedule(4.0f, [this]() { Next(); });
    }
private:
    void Schedule(float Delay, TFunction<void()> Callback)
    {
        if (!World.IsValid()) { Finish(); return; }
        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle,
            FTimerDelegate::CreateLambda([Self = AsShared(), Callback = MoveTemp(Callback)]()
            { if (Self->World.IsValid()) { Callback(); } }),
            Delay, false);
    }
    void Check(bool Condition, const FString& Reason)
    {
        if (!Condition)
        {
            ++Failures;
            UE_LOG(LogRogue10m, Error, TEXT("REFERENCE_METAL_HUD FAIL case=%d reason=%s"), Index, *Reason);
        }
    }
    FBounds Bounds(UWidget* Widget, const FGeometry& Root)
    {
        if (!Widget) { Check(false, TEXT("missing_widget")); return {}; }
        const FGeometry& G = Widget->GetCachedGeometry();
        FBounds B;
        B.Min = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
        B.Max = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
        Check(B.Size().X > 0.0 && B.Size().Y > 0.0, Widget->GetName() + TEXT("_empty_geometry"));
        return B;
    }
    void Next()
    {
        if (Index >= UE_ARRAY_COUNT(Cases)) { Finish(); return; }
        const FCase& C = Cases[Index];
        GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = OriginalApplicationScale * (Index == 4 ? 1.25f : 1.0f);
        Check(World->GetFirstPlayerController() && Cast<ARogue10mCharacter>(World->GetFirstPlayerController()->GetPawn()), TEXT("missing_test_character"));
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(PC->GetPawn()))
            {
                if (ARogue10mPlayerState* State = Character->GetPlayerState<ARogue10mPlayerState>())
                {
                    // Explicit fixture: weapon-driven theme plus enabled/disabled mana.
                    // These values belong only to this disposable test process.
                    State->SetIdentityType(Index == 5 ? ERogue10mIdentityType::None : Index == 3 ? ERogue10mIdentityType::Focus : ERogue10mIdentityType::Vigor);
                    State->SetManaEnabled(Index == 2);
                }
                if (URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet())
                {
                    Attributes->RestoreVitals();
                    Attributes->SetExperience(Attributes->GetExperienceToNextLevel() * 0.42f);
                    Attributes->SetIdentity(Attributes->GetMaxIdentity() * (Index == 0 ? 0.0f : Index == 1 ? 0.72f : Index == 2 ? 1.0f : 0.35f));
                }
                Character->SetEquippedWeaponType(C.Weapon);
                Character->SetCanBeDamaged(false);
                Character->GetCharacterMovement()->StopMovementImmediately();
                Character->GetCharacterMovement()->DisableMovement();
            }
        }
        GEngine->Exec(World.Get(), *FString::Printf(TEXT("r.SetRes %dx%dw"), C.Width, C.Height));
        Schedule(2.5f, [this]() { PrepareTarget(); });
    }
    void PrepareTarget()
    {
        ARogue10mPlayerController* PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        if (!PC || !PC->PlayerCameraManager)
        {
            Check(false, TEXT("monster_fixture_camera_missing"));
            Finish();
            return;
        }
        for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
        {
            if (!TargetMonster.IsValid() && !It->IsDead()) { TargetMonster = *It; }
            It->ClearAITarget();
            if (AController* AI = It->GetController()) { AI->UnPossess(); }
            It->GetCharacterMovement()->StopMovementImmediately();
            It->GetCharacterMovement()->DisableMovement();
            It->SetActorEnableCollision(false);
        }
        if (!TargetMonster.IsValid())
        {
            Check(false, TEXT("actual_monster_fixture_missing"));
            Finish();
            return;
        }
        // All fixture changes are restricted to this disposable game process.
        // Exercise the production camera trace instead of injecting a synthetic HUD view.
        const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
        const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
        TargetMonster->SetActorLocation(CameraLocation + CameraRotation.Vector() * 300.0f,
            false, nullptr, ETeleportType::TeleportPhysics);
        TargetMonster->SetActorRotation(FRotator(0.0f, CameraRotation.Yaw + 180.0f, 0.0f));
        TargetMonster->SetActorEnableCollision(true);
        TargetMonster->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        TargetMonster->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        if (URogue10mAttributeSet* Attributes = TargetMonster->GetRogueAttributeSet())
        {
            Attributes->SetHealth(Attributes->GetMaxHealth() * (Index == 6 ? 0.35f : 0.72f));
        }
        Check(PC->FindLookedAtMonster() == TargetMonster.Get(), TEXT("actual_monster_trace_fixture_missed"));
        Schedule(0.4f, [this]() { Inspect(); });
    }
    void Inspect()
    {
        URogue10mMainHUDWidget* HUD = nullptr;
        for (TObjectIterator<URogue10mMainHUDWidget> It; It; ++It)
        {
            if (It->GetWorld() == World.Get() && It->IsInViewport()) { HUD = *It; break; }
        }
        Check(HUD != nullptr, TEXT("main_hud_not_on_player_screen"));
        if (!HUD) { Finish(); return; }
        if (Index == 0)
        {
            UClass* LineClass = LoadClass<URogue10mLogLineWidget>(nullptr,
                TEXT("/Game/Widget/Parts/ReferenceMetal/WBP_ReferenceMetalLogLine.WBP_ReferenceMetalLogLine_C"));
            URogue10mLogLineWidget* Line = LineClass
                ? CreateWidget<URogue10mLogLineWidget>(World->GetFirstPlayerController(), LineClass) : nullptr;
            Check(Line != nullptr, TEXT("log_line_fixture_missing"));
            if (Line)
            {
                FRogue10mHudLogEntryView Entry;
                Entry.Message = FText::FromString(TEXT("HUD QA Item"));
                Entry.Quantity = 10;
                Entry.bItemAcquisition = true;
                Line->SetLogEntryView(Entry);
                UTextBlock* Message = Cast<UTextBlock>(Line->GetWidgetFromName(TEXT("UI_MessageText")));
                Check(Message && Message->GetText().ToString().Contains(TEXT("10"))
                    && Line->GetToolTipText().ToString().Contains(TEXT("10")), TEXT("acquisition_quantity_missing"));
                Entry.bItemAcquisition = false;
                Line->SetLogEntryView(Entry);
                Check(Message && Message->GetText().EqualTo(Entry.Message), TEXT("system_message_changed"));
            }
        }
        HUD->RefreshBoundWidgetData();
        auto* Bottom = Cast<URogue10mBottomHUDWidget>(HUD->GetWidgetFromName(TEXT("BottomHUDWidget")));
        Check(Bottom != nullptr, TEXT("bottom_hud_missing"));
        if (!Bottom) { Finish(); return; }
        const ERogue10mBottomHUDTheme ExpectedTheme = Index == 1 ? ERogue10mBottomHUDTheme::Warrior : Index == 2 ? ERogue10mBottomHUDTheme::Wizard : Index == 3 ? ERogue10mBottomHUDTheme::Rogue : ERogue10mBottomHUDTheme::MartialArtist;
        Check(Bottom->GetResolvedTheme() == ExpectedTheme, TEXT("weapon_theme_did_not_change"));
        const FGeometry& Root = HUD->GetCachedGeometry();
        const FVector2D RootSize = Root.GetLocalSize();
        const FIntPoint Pixels = World->GetGameViewport()->Viewport->GetSizeXY();
        const FCase& C = Cases[Index];
        Check(Pixels == FIntPoint(C.Width, C.Height), TEXT("requested_viewport_size_not_applied"));
        FBounds Viewport;
        Viewport.Max = RootSize;
        Check(HUD->GetWidgetFromName(TEXT("SystemLogPanelWidget")) == nullptr, TEXT("legacy_log_placeholder_retained"));
        Check(HUD->GetWidgetFromName(TEXT("ItemAcquisitionFeedWidget")) == nullptr, TEXT("legacy_feed_retained"));
        UTextBlock* Timer = Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("UI_RunTimerText")));
        Check(Timer && !Timer->GetText().IsEmpty(), TEXT("run_timer_not_bound"));
        const FBounds TimerBounds = Bounds(Timer, Root);
        Check(Viewport.Contains(TimerBounds) && FMath::Abs(RootSize.X - TimerBounds.Max.X - 24.0) < 1.5,
            TEXT("timer_not_right_anchored"));
        for (const TCHAR* Name : {TEXT("SystemLogContainer"), TEXT("ItemAcquisitionContainer"), TEXT("MonsterInfoWidget")})
        {
            UWidget* W = HUD->GetWidgetFromName(Name);
            Check(W != nullptr, FString(Name) + TEXT("_missing"));
            if (W && W->IsVisible()) { Check(Viewport.Contains(Bounds(W, Root)), FString(Name) + TEXT("_outside_viewport")); }
        }
        auto* Monster = Cast<URogue10mMonsterInfoWidget>(HUD->GetWidgetFromName(TEXT("MonsterInfoWidget")));
        ARogue10mPlayerController* PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
        Check(PC && TargetMonster.IsValid() && PC->FindLookedAtMonster() == TargetMonster.Get(),
            TEXT("live_monster_trace_lost"));
        Check(Monster && Monster->IsVisible() && Monster->GetMonsterInfoView().bHasMonster,
            TEXT("target_information_not_visible"));
        if (Monster && Monster->IsVisible())
        {
            const FBounds Target = Bounds(Monster, Root);
            Check(Viewport.Contains(Target) && FMath::Abs(Target.Size().X - 560.0) < 1.5
                && FMath::Abs(Target.Size().Y - 68.0) < 1.5 && FMath::Abs(Target.Min.Y - 24.0) < 1.5
                && FMath::Abs((Target.Min.X + Target.Max.X) * 0.5 - RootSize.X * 0.5) < 1.5,
                TEXT("monster_reference_framing_incorrect"));
            UProgressBar* Health = Cast<UProgressBar>(Monster->GetWidgetFromName(TEXT("UI_MonsterHealthBar")));
            const FBounds HealthBounds = Bounds(Health, Root);
            Check(Target.Contains(HealthBounds) && FMath::Abs(HealthBounds.Min.X - Target.Min.X - 14.0) < 1.5
                && FMath::Abs(HealthBounds.Min.Y - Target.Min.Y - 32.0) < 1.5
                && FMath::Abs(HealthBounds.Size().X - 532.0) < 1.5
                && FMath::Abs(HealthBounds.Size().Y - 12.0) < 1.5, TEXT("monster_health_bar_geometry_incorrect"));
            const FRogue10mHudMonsterInfoView View = Monster->GetMonsterInfoView();
            Check(Health && FMath::IsNearlyEqual(Health->GetPercent(), View.Health.Normalized, 0.001f),
                TEXT("monster_health_fill_not_live"));
            UTextBlock* Name = Cast<UTextBlock>(Monster->GetWidgetFromName(TEXT("UI_MonsterNameText")));
            Check(TargetMonster.IsValid() && View.Name.EqualTo(TargetMonster->GetMonsterDisplayName())
                && Name && !Name->GetText().IsEmpty(), TEXT("monster_name_not_live"));
            Check(Name && Bounds(Name, Root).Max.X <= TimerBounds.Min.X + 1.0,
                TEXT("monster_name_overlaps_timer"));
            const URogue10mAttributeSet* TargetAttributes = TargetMonster.IsValid()
                ? TargetMonster->GetRogueAttributeSet() : nullptr;
            Check(TargetAttributes && FMath::IsNearlyEqual(View.Health.Current, TargetAttributes->GetHealth(), 0.01f),
                TEXT("monster_health_data_did_not_reach_hud"));
        }
        UWidget* CoreWidget = Bottom->GetWidgetFromName(TEXT("UI_CombatCanvas"));
        const FBounds Core = Bounds(CoreWidget, Root);
        Check(Viewport.Contains(Core), TEXT("combat_core_outside_viewport"));
        Check(FMath::Abs((Core.Min.X + Core.Max.X) * 0.5 - RootSize.X * 0.5) < 1.5,
            TEXT("combat_core_not_centered"));
        Check(FMath::IsNearlyEqual(Core.Size().X / FMath::Max(1.0, Core.Size().Y), 1200.0 / 208.0, 0.03),
            TEXT("combat_core_aspect_distorted"));
        Check(Core.Size().X <= 1201.0 && Core.Size().Y <= 209.0, TEXT("combat_core_upscaled"));
        Check(FMath::Abs(RootSize.Y - Core.Max.Y - 56.0) < 1.5, TEXT("bottom_margin_changed"));

        if (Index == 4) { Check(Core.Size().X < 1199.0, TEXT("downscale_branch_not_exercised")); }
        UWidget* Resource = Bottom->GetWidgetFromName(Index == 2 ? TEXT("ManaBarWidget") : TEXT("StaminaBarWidget"));
        Check(Resource && Resource->IsVisible(), TEXT("active_resource_hidden"));
        Check(Core.Contains(Bounds(Resource, Root)), TEXT("resource_outside_core"));
        TArray<FBounds> Groups;
        for (const TCHAR* Name : {TEXT("HealthBarWidget"), TEXT("IdentityWidget"),
                                  TEXT("SkillSlotContainer"), TEXT("ItemSlotContainer")})
        {
            UWidget* W = Bottom->GetWidgetFromName(Name);
            const FBounds B = Bounds(W, Root);
            Check(Core.Contains(B), FString(Name) + TEXT("_outside_core"));
            Groups.Add(B);
        }
        Check(Groups[2].Max.X <= Groups[1].Min.X + 1.0
            && Groups[1].Max.X <= Groups[3].Min.X + 1.0, TEXT("skill_identity_item_overlap"));
        Check(FMath::IsNearlyEqual(Groups[1].Size().X, Groups[1].Size().Y, 1.0),
            TEXT("identity_not_square"));

        for (const TCHAR* Name : {TEXT("SkillSlotContainer"), TEXT("ItemSlotContainer")})
        {
            UPanelWidget* Group = Cast<UPanelWidget>(Bottom->GetWidgetFromName(Name));
            if (!Group) { Check(false, TEXT("slot_container_missing")); continue; }
            const bool bSkills = FCString::Strcmp(Name, TEXT("SkillSlotContainer")) == 0;
            Check(Group->GetChildrenCount() == (bSkills ? 5 : 4), TEXT("runtime_slot_count_changed"));
            const FBounds GroupBounds = Bounds(Group, Root);
            double LastRight = GroupBounds.Min.X;
            FVector2D FirstSize = FVector2D::ZeroVector;
            for (int32 ChildIndex = 0; ChildIndex < Group->GetChildrenCount(); ++ChildIndex)
            {
                UWidget* Child = Group->GetChildAt(ChildIndex);
                const FBounds B = Bounds(Child, Root);
                Check(GroupBounds.Contains(B), TEXT("slot_clipped_by_container"));
                Check(B.Min.X >= LastRight - 0.5, TEXT("slot_overlap"));
                if (ChildIndex == 0) { FirstSize = B.Size(); }
                else { Check(B.Size().Equals(FirstSize, 0.5), TEXT("unequal_slot_sizes")); }
                LastRight = B.Max.X;
                auto* Slot = Cast<URogue10mQuickSlotWidget>(Child);
                Check(Slot != nullptr, TEXT("wrong_slot_class"));
                if (Slot)
                {
                    const FRogue10mHudQuickSlotView V = Slot->GetQuickSlotView();
                    if (bSkills && ChildIndex == 4)
                    {
                        Check(V.bDodgeSlot && V.InputText.ToString() == TEXT("E"), TEXT("dodge_input_changed"));
                    }
                    if (bSkills) { Check(V.SkillIcon != nullptr, TEXT("skill_art_missing")); }
                    if (!bSkills) { Check(!V.bUnlocked, TEXT("placeholder_item_became_usable")); }
                }
            }
        }
        if (auto* Identity = Cast<URogue10mIdentityWidget>(Bottom->GetWidgetFromName(TEXT("IdentityWidget"))))
        {
            const auto View = Identity->GetIdentityView();
            const UTextBlock* Percent = Cast<UTextBlock>(Identity->GetWidgetFromName(TEXT("UI_IdentityPercentText")));
            const FString Expected = View.bHasIdentityResource
                ? FString::Printf(TEXT("%.0f%%"), FMath::Clamp(View.Normalized, 0.0f, 1.0f) * 100.0f)
                : TEXT("--");
            Check(Percent && Percent->GetText().ToString() == Expected, TEXT("identity_percent_not_live"));
            Check(View.bHasIdentityResource == (Index != 5), TEXT("identity_resource_fixture_wrong"));
            UWidget* Fallback = Identity->GetWidgetFromName(TEXT("UI_IdentityFallbackIcon"));
            UWidget* ActualIcon = Identity->GetWidgetFromName(TEXT("UI_IdentityIcon"));
            Check(Fallback && ActualIcon, TEXT("identity_emblem_widgets_missing"));
            Check(Fallback && ActualIcon && (View.IconTexture.IsValid() ? ActualIcon->IsVisible() && !Fallback->IsVisible()
                : Fallback->IsVisible()), TEXT("identity_emblem_blank_or_overlapped"));
        }
        auto* Progression = Cast<URogue10mProgressionWidget>(Bottom->GetWidgetFromName(TEXT("ProgressionWidget")));
        const FBounds XP = Bounds(Progression, Root);
        Check(Viewport.Contains(XP), TEXT("xp_outside_viewport"));
        Check(FMath::Abs(XP.Min.X - 24.0) < 1.5 && FMath::Abs(RootSize.X - XP.Max.X - 24.0) < 1.5
            && FMath::Abs(XP.Size().Y - 10.0) < 1.5 && FMath::Abs(RootSize.Y - XP.Max.Y - 16.0) < 1.5,
            TEXT("xp_reference_framing_incorrect"));
        if (Progression)
        {
            auto* XPBar = Cast<UProgressBar>(Progression->GetWidgetFromName(TEXT("UI_ExperienceBar")));
            const FBounds Fill = Bounds(XPBar, Root);
            Check(XP.Contains(Fill) && FMath::Abs(Fill.Size().Y - 4.0) < 1.0
                && FMath::Abs(Fill.Min.X - XP.Min.X - 6.0) < 1.0
                && FMath::Abs(XP.Max.X - Fill.Max.X - 6.0) < 1.0
                && FMath::Abs(Fill.Min.Y - XP.Min.Y - 3.0) < 1.0, TEXT("xp_inner_fill_geometry_incorrect"));
            Check(XPBar && FMath::IsNearlyEqual(XPBar->GetPercent(),
                HUD->GetProgressionView().ExperienceNormalized, 0.001f), TEXT("xp_fill_not_live"));
        }
        UE_LOG(LogRogue10m, Display,
            TEXT("REFERENCE_METAL_HUD CASE=%d viewport=%dx%d logical=%.1fx%.1f core=%.1f,%.1f %.1fx%.1f identity=%.1fx%.1f failures=%d"),
            Index, Pixels.X, Pixels.Y, RootSize.X, RootSize.Y, Core.Min.X, Core.Min.Y,
            Core.Size().X, Core.Size().Y, Groups[1].Size().X, Groups[1].Size().Y, Failures);
        FScreenshotRequest::RequestScreenshot(
            FString::Printf(TEXT("ReferenceMetalHUD_%02d_%dx%d.png"), Index, C.Width, C.Height), true, false);
        Schedule(1.5f, [this]() { ++Index; Next(); });
    }
    void Finish()
    {
        GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = OriginalApplicationScale;
        UE_LOG(LogRogue10m, Display, TEXT("RESULT=REFERENCE_METAL_HUD_%s cases=%d failures=%d"),
            Failures == 0 && Index == UE_ARRAY_COUNT(Cases) ? TEXT("PASSED") : TEXT("FAILED"), Index, Failures);
        FPlatformMisc::RequestExitWithStatus(false,
            Failures == 0 && Index == UE_ARRAY_COUNT(Cases) ? 0 : 1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mBasicMonster> TargetMonster;
    int32 Index = 0;
    int32 Failures = 0;
    float OriginalApplicationScale = 1.0f;
};
static TSharedPtr<FRun> ActiveRun;
static void Run()
{
    if (!GEngine) { return; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
        {
            ActiveRun = MakeShared<FRun>();
            ActiveRun->Start(World);
            return;
        }
    }
    UE_LOG(LogRogue10m, Error, TEXT("RESULT=REFERENCE_METAL_HUD_FAILED reason=no_game_world"));
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestReferenceMetalHUD"),
    TEXT("Resizes the live game, verifies actual HUD geometry and bindings, captures seven cases, then exits."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
