#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/Image.h"
#include "Components/InputKeySelector.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Slate/UMGDragDropOp.h"
#include "Rogue10mCombatHudAWidget.h"
#include "Rogue10mMainHUDWidget.h"
#include "Rogue10mSkillDragDropOperation.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Children.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mHybridSkillTreeWidget.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mLootTableDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

/** Exercises the real damage adapter with explicit executions, not a synthetic XP counter. */
struct FRogue10mHybridCombatTestAccess
{
	static bool Hit(URogue10mCombatComponent& Combat, const URogue10mAttackSkillData& Skill,
		FRogue10mActiveAttackExecution& Execution, AActor& Target)
	{
		return Combat.ApplyAttackDamage(Skill, Execution, Target, FVector::ForwardVector, 1);
	}
};

namespace Rogue10mHybridRuntime
{
using ETrack = ERogue10mSkillTreeTrack;
using EIdentity = ERogue10mSkillTreeIdentity;
using ERoute = ERogue10mSkillTreeUnlockRoute;
using ETab = ERogue10mSkillTreeTab;
constexpr auto TestLoadoutSlot = ERogue10mAttackInputSlot::QSkill;
constexpr auto IncompatibleLoadoutSlot = ERogue10mAttackInputSlot::MouseChord;
static bool SafeFlag(const TCHAR* Key, const TCHAR* Prefix)
{
	FString Value;
	if (!FParse::Value(FCommandLine::Get(), Key, Value) || !Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
	for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
	return true;
}
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		bRead = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridRead"));
		bInputQA = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridInputQA"));
		bCommonQA = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridCommonQA"));
		bCommonPresentationQA = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridCommonPresentationQA"));
		bRollOnlyQA = FParse::Param(FCommandLine::Get(), TEXT("RogueHybridRollQA"));
		if (!SafeFlag(TEXT("RogueProfileSaveSlot="), TEXT("Rogue10m_Town_QA_"))
			|| !SafeFlag(TEXT("RogueProgressionSavePrefix="), TEXT("Rogue10m_Progression_QA_"))
			|| !SafeFlag(TEXT("RogueSkillKeySaveSlot="), TEXT("Rogue10m_Fighter_QA_")))
		{
			Check(false, TEXT("all_three_isolated_save_flags_required")); Finish(); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), bInputQA
			? TEXT("Feature/doc/evidence/hybrid-tree-input-polish-20261020")
			: TEXT("Feature/doc/evidence/hybrid-skill-tree-runtime-20261020"));
		// Optional isolated evidence packet. Older wrappers only varied save/log names;
		// their fixed result/PNG directory must not be reused for this Alt input QA.
		FString EvidenceLabel;
		if (FParse::Value(FCommandLine::Get(), TEXT("RogueHybridEvidenceLabel="), EvidenceLabel))
		{
			bool bSafeLabel = !EvidenceLabel.IsEmpty() && EvidenceLabel.Len() <= 24;
			for (TCHAR C : EvidenceLabel) { bSafeLabel &= FChar::IsAlnum(C) || C == TEXT('_'); }
			Output = FPaths::Combine(Output, EvidenceLabel, bRead ? TEXT("read") : TEXT("runtime"));
			if (!bSafeLabel || IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().FileExists(*Output))
			{
				UE_LOG(LogRogue10m, Error, TEXT("HYBRID INPUT FAILED: fresh, safe evidence label required."));
				FPlatformMisc::RequestExitWithStatus(false, 1); return;
			}
		}
		if (bRollOnlyQA && FParse::Value(FCommandLine::Get(), TEXT("RogueRollAuditDirection="), RollAuditDirection))
		{
			if (RollAuditDirection == TEXT("Right")) { RollMoveKey = EKeys::D; }
			else if (RollAuditDirection == TEXT("Back")) { RollMoveKey = EKeys::S; }
			else if (RollAuditDirection != TEXT("Forward"))
			{
				Check(false, TEXT("known_directional_roll_audit_case")); Finish(); return;
			}
		}
		IFileManager::Get().MakeDirectory(*Output, true);
		Later(bRollOnlyQA ? 0.15 : 3.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->Done) { return false; }
			if (!Self->World.IsValid()) { Self->Check(false, TEXT("world_lost")); Self->Finish(); return false; }
			if (Self->FixtureTime() >= Self->NextTime && Self->Next) { auto F = MoveTemp(Self->Next); F(); }
			return !Self->Done;
		}), 0.03f);
	}
private:
	void Check(bool Passed, const TCHAR* Message)
	{
		Report += FString::Printf(TEXT("%s %s\n"), Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
		UE_LOG(LogRogue10m, Display, TEXT("HYBRID_%s %s %s"), bRead ? TEXT("READ") : TEXT("RUNTIME"), Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
		if (!Passed) { ++Failures; }
	}
	double FixtureTime() const { return bRollOnlyQA ? World->GetTimeSeconds() : FPlatformTime::Seconds(); }
	void Later(double Seconds, TFunction<void()> F) { NextTime = FixtureTime() + Seconds; Next = MoveTemp(F); }
	bool HitPath(const TSharedPtr<SWidget>& Target, FWidgetPath& OutPath, FVector2D& Point,
		const FVector2D& NormalizedPoint = FVector2D(0.5, 0.5))
	{
		FWidgetPath FocusPath;
		if (!Target || !Target->IsEnabled()
			|| !FSlateApplication::Get().GeneratePathToWidgetUnchecked(Target.ToSharedRef(), FocusPath))
		{ Check(false, TEXT("enabled_real_widget_path")); return false; }
		const FGeometry& Geometry = Target->GetTickSpaceGeometry();
		Point = Geometry.GetAbsolutePositionAtCoordinates(NormalizedPoint);
		OutPath = FSlateApplication::Get().LocateWindowUnderMouse(Point,
			TArray<TSharedRef<SWindow>>{FocusPath.GetWindow()}, false, 0);
		const bool Hit = OutPath.IsValid() && OutPath.ContainsWidget(Target.Get());
		if (!Hit) { UE_LOG(LogRogue10m, Warning, TEXT("HYBRID_RUNTIME widget outside hit path: %s point=%s size=%s"),
			*Target->GetTypeAsString(), *Point.ToString(), *Geometry.GetLocalSize().ToString()); }
		Check(Hit, TEXT("real_mouse_hit_reaches_widget"));
		return Hit;
	}
	bool HitPath(UWidget* Target, FWidgetPath& OutPath, FVector2D& Point,
		const FVector2D& NormalizedPoint = FVector2D(0.5, 0.5))
	{
		return HitPath(Target ? Target->GetCachedWidget() : nullptr, OutPath, Point, NormalizedPoint);
	}
	bool ClickWidget(UWidget* Target, const FVector2D& NormalizedPoint = FVector2D(0.5, 0.5))
	{
		FWidgetPath Path; FVector2D Point;
		if (!HitPath(Target, Path, Point, NormalizedPoint)) { return false; }
		auto& Slate = FSlateApplication::Get();
		Slate.RoutePointerMoveEvent(Path,
			FPointerEvent(0, Point, Point, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		Slate.RoutePointerDownEvent(Path, FPointerEvent(0, Point, Point,
			TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		Slate.RoutePointerUpEvent(Path, FPointerEvent(0, Point, Point,
			TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		return true;
	}
	bool Click(const TCHAR* Name)
	{
		return ClickWidget(Widget.IsValid() ? Widget->GetWidgetFromName(Name) : nullptr);
	}
	URogue10mCombatHudAWidget* FindCombatHUD() const
	{
		for (TObjectIterator<URogue10mMainHUDWidget> It; It; ++It)
		{
			if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get() && It->IsInViewport())
			{
				if (URogue10mCombatHudAWidget* HUD = It->GetCombatHudA()) { return HUD; }
			}
		}
		return nullptr;
	}
	TSharedPtr<SWidget> FindHUDSlot(const URogue10mCombatHudAWidget* HUD, ERogue10mAttackInputSlot Slot) const
	{
		const TSharedPtr<SWidget> Root = HUD ? HUD->GetCachedWidget() : nullptr;
		if (!Root) { return nullptr; }
		const int32 Index = HUD->GetDisplayedSkills().IndexOfByPredicate([Slot](const FRogue10mHudQuickSlotView& View)
		{
			return View.InputSlot == Slot;
		});
		// Layout A's first Slate children are the skill hit zones, in displayed slot order.
		FChildren* HitZones = Root->GetChildren();
		if (!HitZones || Index == INDEX_NONE || Index >= HitZones->Num()) { return nullptr; }
		const TSharedRef<SWidget> Target = HitZones->GetChildAt(Index);
		const FGeometry& Geometry = Target->GetTickSpaceGeometry();
		if (Geometry.GetLocalSize().GetMin() <= 1.0) { return nullptr; }
		UE_LOG(LogRogue10m, Display, TEXT("HYBRID_RUNTIME actual Layout A drop target: %s slot=%d size=%s"),
			*Target->GetTypeAsString(), static_cast<int32>(Slot), *Geometry.GetLocalSize().ToString());
		return Target;
	}
	static const FRogue10mHudQuickSlotView* FindHUDSlotView(
		const URogue10mCombatHudAWidget* HUD, ERogue10mAttackInputSlot Slot)
	{
		return HUD ? HUD->GetDisplayedSkills().FindByPredicate([Slot](const FRogue10mHudQuickSlotView& View)
		{
			return View.InputSlot == Slot;
		}) : nullptr;
	}
	void CheckDragPresentationRestored(ESlateVisibility BeforeVisibility, float BeforeOpacity)
	{
		Check(Widget.IsValid() && !Widget->IsSkillDragInProgress() && Widget->GetVisibility() == BeforeVisibility
			&& FMath::IsNearlyEqual(Widget->GetRenderOpacity(), BeforeOpacity) && PC->IsAnyBlockingWindowVisible(),
			TEXT("input_drop_or_cancel_restores_menu_visibility_opacity_and_open_state"));
	}
	void DragNodeToHUD(FName NodeId, bool bIncompatibleSlot, bool bExpectDrag, TFunction<void()> Continuation, bool bCloseMenu = false)
	{
		auto* Entry = Widget.IsValid() ? Widget->FindNodeEntry(NodeId) : nullptr;
		FWidgetPath SourcePath; FVector2D Start;
		if (!HitPath(Entry, SourcePath, Start)) { Later(0.15, MoveTemp(Continuation)); return; }
		const auto TargetSlot = bIncompatibleSlot ? IncompatibleLoadoutSlot : TestLoadoutSlot;
		URogue10mCombatHudAWidget* HUD = bExpectDrag ? FindCombatHUD() : nullptr;
		const TSharedPtr<SWidget> Target = FindHUDSlot(HUD, TargetSlot);
		if (bExpectDrag)
		{
			Check(Target != nullptr, TEXT("input_real_HUD_target_exists"));
			if (!Target) { Later(0.15, MoveTemp(Continuation)); return; }
		}
		const auto Before = Player->GetCombatComponent()->CaptureCombat().SkillBindings;
		const FRogue10mHudQuickSlotView* DisplayedSlot = FindHUDSlotView(HUD, TargetSlot);
		const FRogue10mHudQuickSlotView BeforeHUD = DisplayedSlot ? *DisplayedSlot : FRogue10mHudQuickSlotView();
		const auto BeforeVisibility = Widget->GetVisibility();
		const float BeforeOpacity = Widget->GetRenderOpacity();
		auto& Slate = FSlateApplication::Get();
		Check(!Slate.IsDragDropping(), TEXT("input_drag_starts_without_stale_operation"));
		Slate.RoutePointerMoveEvent(SourcePath, FPointerEvent(0, Start, Start,
			TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		Slate.RoutePointerDownEvent(SourcePath, FPointerEvent(0, Start, Start,
			TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		const FVector2D Threshold = Start + FVector2D(32, 0);
		auto MovePath = Slate.LocateWindowUnderMouse(Threshold,
			TArray<TSharedRef<SWindow>>{SourcePath.GetWindow()}, false, 0);
		Slate.RoutePointerMoveEvent(MovePath, FPointerEvent(0, Threshold, Start,
			TSet<FKey>{EKeys::LeftMouseButton}, EKeys::Invalid, 0, FModifierKeysState()), false);
		const auto ActiveDrag = Slate.GetDragDroppingContent();
		const auto UMGDrag = ActiveDrag.IsValid() && ActiveDrag->IsOfType<FUMGDragDropOp>()
			? StaticCastSharedPtr<FUMGDragDropOp>(ActiveDrag) : TSharedPtr<FUMGDragDropOp>();
		const auto* Payload = UMGDrag ? Cast<URogue10mSkillDragDropOperation>(UMGDrag->GetOperation()) : nullptr;
		Check(bExpectDrag ? Payload && Payload->bUnlocked && Payload->SkillData == InputSkill.Get()
			: !ActiveDrag.IsValid(), bExpectDrag ? TEXT("input_real_drag_contains_learned_skill_payload")
			: TEXT("input_locked_node_cannot_start_drag"));
		if (!bExpectDrag)
		{
			// A locked node must not expose the HUD or start a drop operation.
			Slate.RoutePointerUpEvent(SourcePath, FPointerEvent(0, Start, Threshold,
				TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
			Check(!Slate.IsDragDropping() && Before.OrderIndependentCompareEqual(
				Player->GetCombatComponent()->CaptureCombat().SkillBindings), TEXT("input_locked_drag_does_not_change_HUD"));
			if (Slate.IsDragDropping()) { Slate.CancelDragDrop(); }
			CheckDragPresentationRestored(BeforeVisibility, BeforeOpacity);
			Later(0.15, MoveTemp(Continuation));
			return;
		}
		// Visibility changes take effect in Slate's next paint/hit-test grid. Do
		// not synthesize the drop in the same frame as drag detection.
		Later(0.15, [this, Target = TWeakPtr<SWidget>(Target), HUD = TWeakObjectPtr<URogue10mCombatHudAWidget>(HUD),
			TargetSlot, bIncompatibleSlot, bCloseMenu,
			Before, BeforeHUD, BeforeVisibility, BeforeOpacity, SourcePath, Start, Threshold,
			Continuation = MoveTemp(Continuation)]() mutable
		{
			Check(Widget.IsValid() && Widget->IsSkillDragInProgress()
				&& Widget->GetVisibility() == ESlateVisibility::HitTestInvisible
				&& Widget->GetRenderOpacity() < BeforeOpacity, TEXT("input_active_drag_dims_menu_and_exposes_HUD_hit_testing"));
			if (bCloseMenu)
			{
				// Deliberately exercise the controller state transition, not a physical K event.
				// The product must cancel its owned Slate operation before any pointer release.
				PC->ToggleSkillTree();
				auto& App = FSlateApplication::Get();
				Check(!PC->IsAnyBlockingWindowVisible() && !App.IsDragDropping() && !Widget->IsSkillDragInProgress()
					&& Widget->GetVisibility() == BeforeVisibility && FMath::IsNearlyEqual(Widget->GetRenderOpacity(), BeforeOpacity),
					TEXT("input_controller_menu_close_cancels_owned_Slate_drag_and_restores_presentation"));
				Check(Before.OrderIndependentCompareEqual(Player->GetCombatComponent()->CaptureCombat().SkillBindings),
					TEXT("input_menu_close_during_drag_keeps_loadout_unchanged"));
				const auto ReleasePath = App.LocateWindowUnderMouse(Threshold,
					TArray<TSharedRef<SWindow>>{SourcePath.GetWindow()}, false, 0);
				App.RoutePointerUpEvent(ReleasePath, FPointerEvent(0, Threshold, Threshold,
					TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
				Later(0.15, [this, BeforeVisibility, BeforeOpacity, Continuation = MoveTemp(Continuation)]() mutable
				{
					PC->ToggleSkillTree();
					Later(0.15, [this, BeforeVisibility, BeforeOpacity, Continuation = MoveTemp(Continuation)]() mutable
					{
						CheckDragPresentationRestored(BeforeVisibility, BeforeOpacity);
						Check(Widget->GetSelectedNodeId() == InputNode, TEXT("input_reopen_after_cancel_preserves_selected_technique"));
						if (Continuation) { Continuation(); }
					});
				});
				return;
			}
			if (!bIncompatibleSlot)
			{
				Shot(InputTrack == ETrack::Brawler ? TEXT("input-brawler-drag-active.png")
					: bRead ? TEXT("input-greatsword-read-drag-active.png") : TEXT("input-greatsword-drag-active-720p.png"));
			}
			// Leave a real frame for the requested active-drag screenshot before release.
			Later(0.15, [this, Target, HUD, TargetSlot, bIncompatibleSlot, Before, BeforeHUD, BeforeVisibility, BeforeOpacity,
				SourcePath, Start, Threshold, Continuation = MoveTemp(Continuation)]() mutable
			{
				FWidgetPath TargetPath; FVector2D End;
				const bool TargetHit = HitPath(Target.Pin(), TargetPath, End);
				auto& App = FSlateApplication::Get();
				if (TargetHit)
				{
					App.RoutePointerMoveEvent(TargetPath, FPointerEvent(0, End, Threshold,
						TSet<FKey>{EKeys::LeftMouseButton}, EKeys::Invalid, 0, FModifierKeysState()), false);
					App.RoutePointerUpEvent(TargetPath, FPointerEvent(0, End, End,
						TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
				}
				else { App.RoutePointerUpEvent(SourcePath, FPointerEvent(0, Start, Threshold,
					TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState())); }
				Check(!App.IsDragDropping(), TEXT("input_pointer_release_finishes_drag"));
				if (App.IsDragDropping()) { App.CancelDragDrop(); }
				Later(0.15, [this, HUD, TargetSlot, TargetHit, bIncompatibleSlot, Before, BeforeHUD, BeforeVisibility,
					BeforeOpacity, Continuation = MoveTemp(Continuation)]() mutable
				{
					CheckDragPresentationRestored(BeforeVisibility, BeforeOpacity);
					Check(Widget->GetSelectedNodeId() == InputNode, TEXT("input_drag_end_preserves_selected_technique"));
					if (TargetHit)
					{
						const FRogue10mHudQuickSlotView* View = FindHUDSlotView(HUD.Get(), TargetSlot);
						if (!bIncompatibleSlot)
						{
							Check(Player->GetCombatComponent()->GetEquippedSkill(TestLoadoutSlot) == InputSkill.Get()
								&& View && View->SkillData == InputSkill.Get(), TEXT("input_real_HUD_drop_equips_learned_technique"));
						}
						else
						{
							Check(Before.OrderIndependentCompareEqual(Player->GetCombatComponent()->CaptureCombat().SkillBindings)
								&& View && View->SkillData == BeforeHUD.SkillData,
								TEXT("input_mouse_chord_slot_rejects_common_skill_without_binding_change"));
						}
					}
					if (Continuation) { Continuation(); }
				});
			});
		});
	}
	UInputKeySelector* Selector() const
	{
		return Widget.IsValid() ? Cast<UInputKeySelector>(Widget->GetWidgetFromName(TEXT("HybridTreeKeySelector"))) : nullptr;
	}
	bool SelectKeyByInput(FKey Key)
	{
		auto* Control = Selector();
		if (!ClickWidget(Control)) { return false; }
		Check(Control->GetIsSelectingKey(), TEXT("input_pointer_click_enters_key_capture"));
		if (!Control->GetIsSelectingKey()) { return false; }
		auto& Slate = FSlateApplication::Get();
		Slate.ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
		Slate.ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
		Check(!Control->GetIsSelectingKey(), TEXT("input_actual_key_event_exits_capture"));
		return !Control->GetIsSelectingKey();
	}
	void GameKey(FKey Key, bool bDown)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
			bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
	}
	void BeginInputChecks(ETrack Track, TFunction<void()> Continuation)
	{
		InputTrack = Track; InputContinuation = MoveTemp(Continuation);
		InputNode = Track == ETrack::Brawler ? FName(TEXT("F.DiamondStrike")) : FName(TEXT("G.CrushingSlash"));
		const auto* Node = Tree->FindNode(InputNode);
		InputSkill = Node ? Node->Skill.LoadSynchronous() : nullptr;
		Check(InputSkill.IsValid() && Tree->IsNodeUnlocked(InputNode)
			&& Tree->GetActiveTrack() == Track, TEXT("input_fixture_uses_real_learned_current_track_technique"));
		if (!InputSkill.IsValid()) { Finish(); return; }
		Check(InputSkill->IsAllowedInLoadoutSlot(TestLoadoutSlot) && !InputSkill->IsAllowedInLoadoutSlot(IncompatibleLoadoutSlot),
			TEXT("input_fixture_accepts_Q_and_rejects_mouse_chord"));
		if (!PC->IsAnyBlockingWindowVisible()) { PC->ToggleSkillTree(); }
		Click(TEXT("HybridTreeCommon"));
		Later(0.35, [this]
		{
			Check(Widget->GetDisplayedTab() == ETab::Common && Widget->FindNodeEntry(TEXT("C.Foundation"))
				&& !Widget->GetWidgetFromName(TEXT("HybridTreeCommonViewToggle")), TEXT("input_common_uses_single_web_panel"));
			Click(InputTrack == ETrack::Brawler ? TEXT("HybridTreeFighter") : TEXT("HybridTreeGreatSword"));
			Later(0.35, [this] { VerifyWeaponTabInput(); });
		});
	}
	void VerifyWeaponTabInput()
	{
		if (InputTrack == ETrack::Brawler)
		{
			const FName Foundation(TEXT("F.Basic.LeftJab"));
			const auto* Node = Tree->FindNode(Foundation);
			const auto Frame = Widget->GetNodeOuterFramePoints(Foundation);
			Check(Widget->GetNodeFrameShape(Foundation) == TEXT("KnuckleMedallion") && Frame.Num() == 49
				&& Frame[0].Equals(Frame.Last()), TEXT("input_knuckle_foundation_has_closed_medallion"));
			ClickWidget(Widget->FindNodeEntry(Foundation), FVector2D(0.5, 0.06));
			Check(Node && Widget->GetSelectedNodeId() == Foundation && Widget->GetSelectedAttackSkill() == Node->Skill.LoadSynchronous(),
				TEXT("input_knuckle_medallion_edge_selects_its_actual_foundation_skill"));
		}
		else
		{
			const FName Resonance(TEXT("G.Resonance.Spread"));
			const auto Ring = Widget->GetNodeOuterFramePoints(Resonance);
			Check(Widget->GetNodeFrameShape(Resonance) == TEXT("ResonanceDoubleRing") && Ring.Num() == 49
				&& Ring[0].Equals(Ring.Last()) && Widget->GetNodeFrameOutlineCount(Resonance) == 2,
				TEXT("input_actual_paint_geometry_has_double_resonance_outline"));
			ClickWidget(Widget->FindNodeEntry(Resonance), FVector2D(0.5, 0.06));
			Check(Widget->GetSelectedNodeId() == Resonance, TEXT("input_resonance_outer_ring_pointer_click_selects_node"));
		}
		TFunction<void()> LearnedDrops = [this]
		{
			DragNodeToHUD(InputNode, true, true, [this]
			{
				DragNodeToHUD(InputNode, false, true, [this]
				{
					DragNodeToHUD(InputNode, false, true, [this] { InputKeys(); }, true);
				});
			});
		};
		if (InputTrack == ETrack::Brawler) { DragNodeToHUD(TEXT("F.Ultimate"), false, false, MoveTemp(LearnedDrops)); }
		else { LearnedDrops(); }
	}
	void InputKeys()
	{
		auto* Keys = PC->GetSkillBindingComponent();
		Check(Widget->GetSelectedNodeId() == InputNode, TEXT("input_drag_selects_technique_for_key_controls"));
		SelectKeyByInput(EKeys::Q);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_selector_actual_key_persists_binding"));
		SelectKeyByInput(EKeys::Escape);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q && PC->IsAnyBlockingWindowVisible(),
			TEXT("input_Esc_cancels_selection_preserves_key_and_menu"));
		Check(Rogue10mInput::DodgeKey() == EKeys::LeftControl, TEXT("input_contract_independently_requires_LeftCtrl_dodge"));
		SelectKeyByInput(EKeys::W);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_reserved_movement_key_rejected_preserves_binding"));
		FText DodgeReason;
		Check(Keys && !Keys->SetSkillKey(InputSkill.Get(), Rogue10mInput::DodgeKey(), DodgeReason)
			&& Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_LeftCtrl_dodge_cannot_be_assigned_as_active_skill"));
		SelectKeyByInput(EKeys::E);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::E, TEXT("input_freed_E_selected_through_real_key_selector"));
		Click(TEXT("HybridTreeClearKey"));
		Check(Keys && !Keys->GetSkillKey(InputSkill.Get()).IsValid(), TEXT("input_real_clear_button_removes_binding"));
		SelectKeyByInput(EKeys::Q);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_reassign_after_clear_through_actual_key_event"));
		Shot(InputTrack == ETrack::Brawler ? TEXT("input-brawler-drag-key-1080p.png") : TEXT("input-greatsword-drag-key.png"));
		Later(0.15, [this]
		{
			Click(TEXT("HybridTreeClose"));
			Check(!PC->IsAnyBlockingWindowVisible() && !UGameplayStatics::IsGamePaused(World.Get())
				&& Selector() && !Selector()->GetIsSelectingKey(), TEXT("input_real_close_restores_gameplay_without_key_capture"));
			Later(0.6, [this] { InputAttack(); });
		});
	}
	void InputAttack()
	{
		auto* Keys = PC->GetSkillBindingComponent();
		FText Reason;
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_Q_slot_is_retained_before_temporary_E_probe"));
		Check(Keys && Keys->SetSkillKey(InputSkill.Get(), EKeys::E, Reason), TEXT("input_freed_E_binding_accepted_for_actual_cast_no_fallback"));
		if (Failures) { Finish(); return; }
		GameKey(EKeys::E, false);
		// Let the fixed input mapping process the release before the next press.
		Later(0.2, [this] { InputAttackUsingE(); });
	}
	void InputAttackUsingE()
	{
		// A hitless real cast validates binding/input/presentation without changing the exact saved mastery fixture.
		Player->GetCharacterMovement()->StopMovementImmediately();
		auto* Attributes = State->GetRogueAttributeSet();
		Attributes->SetStamina(Attributes->GetMaxStamina()); Attributes->SetMana(Attributes->GetMaxMana());
		InputUseBefore = State->GetSkillUseCount(InputSkill->GetFName());
		InputStaminaBefore = Attributes->GetStamina();
		Check(Player->GetCombatComponent()->GetAttackCooldownRemaining() <= 0,
			TEXT("input_cast_starts_after_shared_cooldown"));
		bSawDodgeDuringE = false;
		FWorldDelegates::OnWorldPostActorTick.Remove(EProbeHandle);
		EProbeHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self = AsShared()](UWorld* InWorld, ELevelTick, float)
		{
			if (InWorld == Self->World.Get() && Self->Player.IsValid()) { Self->bSawDodgeDuringE |= Self->Player->IsDodging(); }
		});
		GameKey(EKeys::E, true);
		Later(0.15, [this]
		{
			GameKey(EKeys::E, false);
			auto* Combat = Player->GetCombatComponent();
			Check(State->GetSkillUseCount(InputSkill->GetFName()) == InputUseBefore + 1
				&& Combat->GetLastAcceptedAttackSkill() == InputSkill.Get() && Combat->GetAttackCooldownRemaining() > 0
				&& State->GetRogueAttributeSet()->GetStamina() <= InputStaminaBefore,
				TEXT("input_bound_key_executes_exact_skill_use_and_cooldown_once"));
			Check(!Player->IsDodging() && Player->GetDodgeCooldownRemaining() <= 0.0f,
				TEXT("input_E_skill_cast_does_not_start_dodge_or_dodge_cooldown"));
			InputCompletedTracks.Add(InputTrack);
			Shot(InputTrack == ETrack::Brawler ? TEXT("input-brawler-key-cast.png") : TEXT("input-greatsword-key-cast.png"));
			Later(1.5, [this]
			{
				const bool bExactlyOnce = State->GetSkillUseCount(InputSkill->GetFName()) == InputUseBefore + 1;
				Check(bExactlyOnce, TEXT("input_key_release_does_not_duplicate_cast"));
				FWorldDelegates::OnWorldPostActorTick.Remove(EProbeHandle);
				Check(!bSawDodgeDuringE, TEXT("input_E_positive_cast_has_zero_dodge_in_all_observed_actor_tick_frames"));
				auto* Keys = PC->GetSkillBindingComponent(); FText Reason;
				Check(Keys && Keys->SetSkillKey(InputSkill.Get(), EKeys::Q, Reason)
					&& Keys->GetSkillKey(InputSkill.Get()) == EKeys::Q, TEXT("input_Q_slot_restored_for_disk_roundtrip"));
				auto Continuation = MoveTemp(InputContinuation); if (Continuation) { Continuation(); }
			});
		});
	}
	void Shot(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output, Name), true, false); }
	void Prepare()
	{
		PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		if (bRead) { PrepareRead(Profiles); return; }
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("HybridTreeQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("new_profile_created"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Later(bRollOnlyQA ? 0.22 : 0.6, [this]
		{
			if (bRollOnlyQA) { PrepareRollOnlyFixture(); }
			else { PrepareKnuckleFixture(); }
		});
	}
	void PrepareRollOnlyFixture()
	{
		Player = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Tree.IsValid() && NPC.IsValid() && Player.IsValid(), TEXT("roll_only_player_tree_and_town_npc_exist"));
		if (Failures) { Finish(); return; }

		auto* Inventory = Player->GetInventoryComponent();
		auto* Sword = LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
		int32 Grid = INDEX_NONE;
		FGuid Item;
		Check(Inventory && Sword && Player->CanChangeWeapon() && Inventory->TryAddGridItem(Sword, 1, Grid, Item)
			&& Inventory->TryEquipGridItem(Grid, Item) && Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword,
			TEXT("roll_only_equips_real_sword_through_inventory"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		auto Growth = State->CaptureGrowth();
		Growth.Level = 8;
		Check(State->RestoreGrowth(Growth), TEXT("roll_only_restores_growth_in_isolated_profile"));
		for (const TCHAR* Id : {TEXT("C.EvadeRecovery"), TEXT("C.Roll"), TEXT("C.EvadeDistance"), TEXT("C.RollCooldown")})
		{
			Check(Tree->TryUnlockNode(Id, ERoute::Points, NAME_None), Id);
		}
		Check(Tree->IsCommonSkillLearned(ERogue10mCommonSkill::Roll), TEXT("roll_only_learns_common_roll"));
		if (Failures) { Finish(); return; }
		Later(0.22, [this] { VerifyCommonRoll(18); });
	}
	void PrepareKnuckleFixture()
	{
		Player = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Tree.IsValid() && NPC.IsValid() && Player.IsValid(), TEXT("player_tree_and_town_npc_exist"));
		if (Failures) { Finish(); return; }
		auto* Inventory = Player->GetInventoryComponent();
		const auto* StarterItem = Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		if (!StarterItem || StarterItem->WeaponType != ERogue10mWeaponType::Knuckle)
		{
			auto* Knuckle = LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_StarterKnuckle.DA_Item_StarterKnuckle"));
			int32 ContainerIndex = INDEX_NONE;
			FGuid InstanceId;
			Check(Knuckle && Knuckle->WeaponType == ERogue10mWeaponType::Knuckle
				&& Inventory->TryAddGridItem(Knuckle, 1, ContainerIndex, InstanceId)
				&& Inventory->TryEquipGridItem(ContainerIndex, InstanceId),
				TEXT("default_unarmed_fixture_equips_real_knuckle_through_inventory"));
			StarterItem = Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		}
		Check(Player->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) != nullptr,
			TEXT("new_profile_keeps_playable_primary_binding"));
		Check(StarterItem && StarterItem->WeaponType == ERogue10mWeaponType::Knuckle
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::Knuckle
			&& Tree->GetActiveTrack() == ETrack::Brawler, TEXT("fighter_fixture_retains_real_knuckle_starter_across_all_domains"));
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 1 && Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5,
			TEXT("initial_M1_five_basic_skills"));
		ExerciseTemporaryBindings();
		Player->SetCanBeDamaged(false);
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 4; Growth.Experience = 0;
		Check(State->RestoreGrowth(Growth), TEXT("level_four_growth_fixture"));
		State->AddExperience(FMath::CeilToInt(State->GetExperienceToNextLevel()));
		Check(State->GetPlayerLevel() == 5
			&& Tree->GetAvailableGrowthPoints() == 4 && Tree->GetSpentSkillTreePoints() == 0,
			TEXT("level5_exactly_four_run_points_with_empty_paid_tree_ledger"));
		Check(Tree->CanChangeInvestments(), TEXT("near_npc_allows_tree_investment"));
		ExerciseDamage();
		auto* Town = State->GetTownProgressionComponent();
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Town && Persistence && Town->UsesLobbyBudget() && Town->CanChangeTraits()
			&& Town->GetLobbyEarnedBudget() == 0 && Town->GetAvailableTraitPoints() == 0
			&& Town->GetSpentTraitPoints() == 0 && Town->GetRunSharedTraitSpent() == 0
			&& Persistence->GetLobbyAvailablePoints() == 0 && Tree->GetAvailableGrowthPoints() == 4,
			TEXT("attached_fresh_profile_has_four_run_points_and_zero_independent_lobby_budget"));
		if (Failures) { Finish(); return; }
		const auto PowerDefinition = Town->GetUpgradeDefinition(ERogue10mTownTrait::Power);
		Check(PowerDefinition.IsTuningValid() && PowerDefinition.PointCost == 1
			&& PowerDefinition.UnlockLevel <= Town->GetLobbyUnlockLevel()
			&& Town->GetTraitLevel(ERogue10mTownTrait::Power) < PowerDefinition.MaxRank,
			TEXT("power_purchase_fixture_satisfies_nonbudget_prerequisites"));
		const auto TownBeforeRejectedPurchase = Town->CaptureProgression();
		const auto TreeBeforeRejectedPurchase = Tree->Capture();
		const int32 GoldBeforeRejectedPurchase = Player->GetInventoryComponent()->GetGold();
		const int32 CrystalsBeforeRejectedPurchase = Player->GetInventoryComponent()->GetCrystals();
		FText TraitReason;
		Check(!Town->CanUpgradeTrait(ERogue10mTownTrait::Power, TraitReason) && !TraitReason.IsEmpty()
			&& !Town->TryUpgradeTrait(ERogue10mTownTrait::Power),
			TEXT("run_level_points_cannot_fund_lobby_power_purchase"));
		const auto TownAfterRejectedPurchase = Town->CaptureProgression();
		const auto TreeAfterRejectedPurchase = Tree->Capture();
		Check(FRogue10mTownProgressionSnapshot::StaticStruct()->CompareScriptStruct(&TownBeforeRejectedPurchase, &TownAfterRejectedPurchase, 0)
			&& FRogue10mSkillTreeSnapshot::StaticStruct()->CompareScriptStruct(&TreeBeforeRejectedPurchase, &TreeAfterRejectedPurchase, 0)
			&& Tree->GetAvailableGrowthPoints() == 4 && Tree->GetSpentSkillTreePoints() == 0
			&& Town->GetLobbyEarnedBudget() == 0 && Town->GetAvailableTraitPoints() == 0 && Town->GetSpentTraitPoints() == 0
			&& Persistence->GetLobbyAvailablePoints() == 0
			&& Player->GetInventoryComponent()->GetGold() == GoldBeforeRejectedPurchase
			&& Player->GetInventoryComponent()->GetCrystals() == CrystalsBeforeRejectedPurchase,
			TEXT("rejected_lobby_purchase_preserves_full_run_and_town_ledgers_and_all_currency"));
		if (Failures) { Finish(); return; }
		for (int32 I = 0; I < 385 && Tree->GetMasteryTotalExperience(ETrack::Brawler) < 385; ++I) { Tree->NotifyConfirmedAttack(ETrack::Brawler); }
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 12 && Tree->IsNodeUnlocked(TEXT("F.DiamondStrike")), TEXT("actual_M12_auto_technique_unlock"));
		Check(Tree->GetIdentityRank(EIdentity::InternalEnergy) == 4
			&& Tree->GetIdentityRank(EIdentity::FightingSpirit) == 4
			&& Tree->GetIdentityRank(EIdentity::Mana) == 4, TEXT("level_five_grows_all_three_identity_ranks"));
		Check(!Tree->TryUnlockNode(TEXT("I.InternalEnergy.Entry"), ERoute::Points, NAME_None)
			&& !Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Mastery, NAME_None)
			&& !Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
			&& Tree->GetAvailableGrowthPoints() == 4 && Tree->GetSpentSkillTreePoints() == 0,
			TEXT("legacy_identity_unlocks_reject_points_and_mastery_without_spending"));
		UseIdentityTonic(EIdentity::InternalEnergy);
		UseIdentityTonic(EIdentity::FightingSpirit);
		UseIdentityTonic(EIdentity::Mana, 2);
		Check(Tree->GetIdentityRank(EIdentity::InternalEnergy) == 5
			&& Tree->GetIdentityRank(EIdentity::FightingSpirit) == 5
			&& Tree->GetIdentityRank(EIdentity::Mana) == 6
			&& Tree->GetAvailableGrowthPoints() == 4 && Tree->GetSpentSkillTreePoints() == 0,
			TEXT("identity_tonics_stack_by_type_without_spending_growth_points"));
		const auto TownAfterRunInvestments = Town->CaptureProgression();
		Check(FRogue10mTownProgressionSnapshot::StaticStruct()->CompareScriptStruct(&TownBeforeRejectedPurchase, &TownAfterRunInvestments, 0)
			&& Town->GetLobbyEarnedBudget() == 0 && Town->GetAvailableTraitPoints() == 0 && Town->GetSpentTraitPoints() == 0
			&& Persistence->GetLobbyAvailablePoints() == 0, TEXT("run_learning_and_refund_do_not_touch_zero_lobby_budget_or_town_ledger"));
		if (Failures) { Finish(); return; }
		PC->ToggleSkillTree();
		for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
		{ if (It->GetWorld() == World && It->GetOwningPlayer() == PC) { Widget = *It; break; } }
		Check(Widget.IsValid() && PC->IsAnyBlockingWindowVisible(), TEXT("real_K_window_native_tabbed_tree"));
		if (Failures) { Finish(); return; }
		Check(Widget->GetDisplayedTab() == ETab::Knuckle
			&& Widget->GetTabOrder() == TArray<ETab>{ETab::Knuckle, ETab::Common, ETab::Sword, ETab::Magic}
			&& Widget->GetVisibleNodeCount() == 17 && Widget->FindNodeEntry(TEXT("F.DiamondStrike"))
			&& Widget->FindNodeEntry(TEXT("F.Ultimate")) && !Widget->FindNodeEntry(TEXT("F.Basic"))
			&& !Widget->FindNodeEntry(TEXT("F.Resonance.Release"))
			&& !Widget->FindNodeEntry(TEXT("G.Ultimate")) && !Widget->FindNodeEntry(TEXT("I.InternalEnergy.Entry")),
			TEXT("equipped_knuckle_opens_first_with_exact_seventeen_visible_training_nodes"));
		for (const TCHAR* Foundation : {TEXT("F.Basic.LeftJab"), TEXT("F.Basic.RightJab"),
			TEXT("F.Basic.RightStraight"), TEXT("F.Basic.RightHook")})
		{
			const auto* FoundationEntry = Widget->FindNodeEntry(Foundation);
			const auto Slot = FName(Foundation) == TEXT("F.Basic.RightHook") ? ERogue10mAttackInputSlot::ChargedSpecial : TestLoadoutSlot;
			Check(FoundationEntry && Tree->IsNodeUnlocked(Foundation) && FoundationEntry->CanDragSkill()
				&& Player->GetCombatComponent()->CanEquipSkillInSlot(FoundationEntry->GetDragSkill(), Slot), Foundation);
		}
		Later(0.35, [this]
		{
			Click(TEXT("HybridTreeMagic"));
			Check(Widget->GetDisplayedTab() == ETab::Magic && Widget->GetVisibleNodeCount() == 0
				&& Widget->GetTabOrder() == TArray<ETab>{ETab::Knuckle, ETab::Common, ETab::Sword, ETab::Magic},
				TEXT("manual_magic_browse_has_no_nodes_and_keeps_equipment_order"));
			Shot(TEXT("magic-placeholder-1080p.png"));
			Later(0.35, [this] { VerifyCommonTab(); });
		});
	}
	void VerifyCommonTab()
	{
		PC->ToggleSkillTree();
		PC->ToggleSkillTree();
		Check(Widget->GetDisplayedTab() == ETab::Knuckle, TEXT("reopening_after_browsing_selects_equipped_tab"));
		Widget->BrowseTab(ETab::Common);
		Check(Widget->GetDisplayedTab() == ETab::Common && Widget->GetVisibleNodeCount() == 16
			&& Widget->FindNodeEntry(TEXT("C.Foundation")) && Widget->FindNodeEntry(TEXT("C.Roll"))
			&& !Widget->FindNodeEntry(TEXT("I.InternalEnergy.Entry")), TEXT("common_tab_opens_the_sixteen_node_mobility_web"));
		Check(!Widget->GetWidgetFromName(TEXT("HybridTreeCommonViewToggle"))
			&& !Widget->FindNodeEntry(TEXT("I.Mana.Entry")), TEXT("common_has_no_legacy_identity_page_or_button"));
		Tree->NotifyWeaponChanged();
		Check(Widget->GetDisplayedTab() == ETab::Common, TEXT("unchanged_equipment_refresh_preserves_manual_tab"));
		Widget->BrowseTab(ETab::Knuckle);
		Widget->SelectNode(TEXT("F.DiamondStrike"));
		Later(0.35, [this]
		{
			VerifyWeaponPresentation(TEXT("F.DiamondStrike"), TEXT("HybridTreeFighter"));
			Shot(TEXT("knuckle-1080p.png"));
			Later(0.4, [this]
			{
				PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
				Later(0.7, [this]
				{
					Shot(TEXT("knuckle-720p.png"));
					Later(0.4, [this]
					{
						PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
						Later(0.7, [this] { VerifyKnuckleInput(); });
					});
				});
			});
		});
	}
	void VerifyKnuckleInput()
	{
		if (bInputQA)
		{
			BeginInputChecks(ETrack::Brawler, [this] { Later(0.35, [this] { GreatSword(); }); });
			return;
		}
		Check(Widget->SetSelectedSkillKey(EKeys::Q), TEXT("learned_technique_key_assignment"));
		Widget->SelectNode(TEXT("F.Ultimate"));
		Check(!Widget->SetSelectedSkillKey(EKeys::Q), TEXT("locked_ultimate_key_rejected"));
		Later(0.6, [this] { GreatSword(); });
	}
	void UseIdentityTonic(EIdentity Identity, int32 Quantity = 1)
	{
		const TCHAR* ItemName = Identity == EIdentity::InternalEnergy ? TEXT("DA_Item_InternalEnergyElixir")
			: Identity == EIdentity::FightingSpirit ? TEXT("DA_Item_FightingSpiritElixir") : TEXT("DA_Item_ManaElixir");
		const FString Path = FString::Printf(TEXT("/Game/DataAsset/Item/Starter/%s.%s"), ItemName, ItemName);
		auto* Item = LoadObject<URogue10mItemDataAsset>(nullptr, *Path);
		const auto* Crimson = LoadObject<URogue10mMonsterDataAsset>(nullptr,
			TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_CrimsonSlime.DA_OW_CrimsonSlime"));
		Check(Item && Crimson && Crimson->LootTable && Crimson->LootTable->Entries.Num() == 3
			&& Crimson->LootTable->Entries.ContainsByPredicate([Item](const FRogue10mLootEntry& Entry)
			{
				return Entry.ItemData == Item && FMath::IsNearlyEqual(Entry.DropChance, 0.25f)
					&& Entry.MinQuantity == 1 && Entry.MaxQuantity == 1;
			}), TEXT("live_crimson_slime_profile_references_typed_identity_tonic"));
		auto* Inventory = Player->GetInventoryComponent();
		int32 ContainerIndex = INDEX_NONE;
		FGuid InstanceId;
		Check(Item && Inventory->TryAddGridItem(Item, Quantity, ContainerIndex, InstanceId), TEXT("identity_tonic_asset_reaches_inventory"));
		if (!InstanceId.IsValid()) { return; }
		const int32 PreviousRank = Tree->GetIdentityRank(Identity);
		const auto* Attributes = State->GetRogueAttributeSet();
		const float PreviousMana = Attributes->GetMana();
		const float PreviousMaxMana = Attributes->GetMaxMana();
		for (int32 Used = 0; Used < Quantity; ++Used)
		{
			Check(Inventory->TryUseGridConsumable(ContainerIndex, InstanceId)
				&& Tree->GetIdentityRank(Identity) == PreviousRank + Used + 1,
				TEXT("identity_tonic_consumes_one_and_adds_one_rank"));
		}
		Check(!Inventory->TryUseGridConsumable(ContainerIndex, InstanceId)
			&& Tree->GetIdentityRank(Identity) == PreviousRank + Quantity,
			TEXT("depleted_identity_tonic_cannot_apply_twice"));
		if (Identity == EIdentity::Mana)
		{
			const float ExpectedMaxMana = PreviousMaxMana + Quantity * Tree->GetCatalog()->ManaCapacityPerRank;
			Check(FMath::IsNearlyEqual(Attributes->GetMaxMana(), ExpectedMaxMana)
				&& FMath::IsNearlyEqual(Attributes->GetMana(), PreviousMana),
				TEXT("mana_tonic_increases_live_capacity_without_refilling_current_mana"));
		}
	}
	void PrepareRead(URogue10mCharacterProfileSubsystem* Profiles)
	{
		// Never create, select, migrate or inspect an unrelated profile in this phase.
		// The three command-line overrides were validated before reaching this point.
		const auto* Selected = Profiles ? Profiles->GetSelectedProfile() : nullptr;
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 1
			&& Selected && Selected->CharacterName == TEXT("HybridTreeQa"), TEXT("read_existing_selected_isolated_HybridTreeQa_profile"));
		if (Failures) { Finish(); return; }
		ReadProfileId = Selected->ProfileId;
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		const FString Slot = Persistence ? Persistence->GetSaveSlotForProfile(ReadProfileId) : FString();
		Check(Persistence && Slot.StartsWith(TEXT("Rogue10m_Progression_QA_"))
			&& UGameplayStatics::DoesSaveGameExist(Slot, 0), TEXT("read_requires_existing_QA_progression_file"));
		if (Failures) { Finish(); return; }
		// Exercise the normal spawn/Possess/AttachCharacter route. No direct snapshot
		// restoration or manufactured mastery/trait state is used by the read phase.
		PC->EnterSelectedCharacter();
		PC->CloseAllBlockingPanels();
		Later(0.6, [this] { VerifyRead(); });
	}
	void VerifyRead()
	{
		Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Player.IsValid() && State.IsValid() && Tree.IsValid() && Persistence
			&& Persistence->IsAttachedTo(*Player) && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == ReadProfileId, TEXT("read_normal_AttachCharacter_completed_all_restore_domains"));
		if (Failures) { Finish(); return; }
		const FString Slot = Persistence->GetSaveSlotForProfile(ReadProfileId);
		Check(Slot.StartsWith(TEXT("Rogue10m_Progression_QA_")), TEXT("read_disk_comparison_stays_in_QA_namespace"));
		if (Failures) { Finish(); return; }
		const auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		Check(Saved && Saved->ProfileId == ReadProfileId && URogue10mProgressionSaveGame::CurrentVersion == 5
			&& Saved->SaveVersion == 5 && Saved->RunId.IsValid() && Saved->RunId == Persistence->GetAttachedRunId()
			&& Saved->LobbyEarnedPoints == 0 && Saved->Town.TraitInvestments.IsEmpty(),
			TEXT("read_current_v5_same_profile_valid_run_identity_and_zero_independent_lobby_budget"));
		if (Failures) { Finish(); return; }
		Check(State->GetPlayerLevel() == (bCommonQA ? 29 : 15) && State->GetPlayerLevel() == Saved->Growth.Level
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::GreatSword, TEXT("read_expected_growth_and_greatsword_combat_restored"));
		const auto Inventory = Player->GetInventoryComponent()->CaptureInventory();
		Check(Inventory.Gold == Saved->Inventory.Gold && Inventory.Crystals == Saved->Inventory.Crystals
			&& Inventory.LeftEquipment.Num() == Saved->Inventory.LeftEquipment.Num()
			&& Inventory.RightEquipment.Num() == Saved->Inventory.RightEquipment.Num()
			&& Inventory.Containers.Num() == Saved->Inventory.Containers.Num(), TEXT("read_inventory_currency_and_equipment_container_domains_restored"));
		const auto* ActualMain = Inventory.LeftEquipment.FindByPredicate([](const auto& Row) { return Row.Values.SlotType == ERogue10mInventorySlotType::MainWeapon; });
		const auto* SavedMain = Saved->Inventory.LeftEquipment.FindByPredicate([](const auto& Row) { return Row.Values.SlotType == ERogue10mInventorySlotType::MainWeapon; });
		const auto* RestoredSword = Player->GetInventoryComponent()->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		Check(ActualMain && SavedMain && RestoredSword && ActualMain->Values.bHasEquippedItem && SavedMain->Values.bHasEquippedItem
			&& RestoredSword->WeaponType == ERogue10mWeaponType::GreatSword
			&& ActualMain->Values.EquippedItem.WeaponType == ERogue10mWeaponType::GreatSword
			&& SavedMain->Values.EquippedItem.WeaponType == ERogue10mWeaponType::GreatSword
			&& ActualMain->Values.EquippedItem.InstanceId.IsValid()
			&& ActualMain->Values.EquippedItem.InstanceId == SavedMain->Values.EquippedItem.InstanceId
			&& ActualMain->ItemAsset.ToSoftObjectPath() == SavedMain->ItemAsset.ToSoftObjectPath()
			&& SavedMain->ItemAsset.ToString() == RestoredSword->GetPathName(),
			TEXT("read_restores_exact_saved_greatsword_item_instance_without_adding_another"));
		const auto* Town = State->GetTownProgressionComponent();
		Check(Town && Town->UsesLobbyBudget() && Town->GetTraitLevel(ERogue10mTownTrait::Power) == 0
			&& Town->GetSpentTraitPoints() == 0 && Town->GetAvailableTraitPoints() == 0 && Town->GetRunSharedTraitSpent() == 0
			&& Saved->LobbyEarnedPoints == 0 && Saved->Town.PowerLevel == 0 && Saved->Town.TraitInvestments.IsEmpty(),
			TEXT("read_zero_lobby_budget_and_empty_town_ledger_restore_without_converting_run_points"));
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 12 && Tree->GetMasteryTotalExperience(ETrack::Brawler) == 385
			&& Tree->GetMasteryLevel(ETrack::GreatSword) == 20 && Tree->GetMasteryTotalExperience(ETrack::GreatSword) == 1045,
			TEXT("read_brawler_M12_and_greatsword_M20_with_exact_XP_restored"));
		Check(Tree->GetSpentSkillTreePoints() == (bCommonQA ? 20 : 5) && Tree->GetAvailableGrowthPoints() == (bCommonQA ? 8 : 9)
			&& State->GetPlayerLevel() - 1 == Tree->GetSpentSkillTreePoints() + Tree->GetAvailableGrowthPoints(),
			TEXT("read_budget_restores_sword_and_common_investments"));
		if (bCommonQA)
		{
			for (const auto& Node : Tree->GetCatalog()->Nodes)
			{
				if (Node.CommonSkill != ERogue10mCommonSkill::None)
				{
					Check(Tree->IsCommonSkillLearned(Node.CommonSkill), *Node.NodeId.ToString());
				}
			}
			Check(Player->JumpMaxCount >= 2, TEXT("read_common_double_jump_reapplied_to_equipped_greatsword"));
		}
		const int32 LevelRank = State->GetPlayerLevel() - 1;
		Check(Tree->GetActiveIdentity() == EIdentity::Mana
			&& Tree->GetIdentityRank(EIdentity::InternalEnergy) == LevelRank + 1
			&& Tree->GetIdentityRank(EIdentity::FightingSpirit) == LevelRank + 1
			&& Tree->GetIdentityRank(EIdentity::Mana) == LevelRank + 2
			&& Saved->SkillTree.IdentityConsumableRanks.FindRef(EIdentity::Mana) == 2
			&& Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana), TEXT("read_tonic_growth_and_ultimate_resonance_restored"));
		auto* Combat = Player->GetCombatComponent();
		Check(Combat && Combat->GetActiveSkillTreeSkills().Num() == 10 && Combat->GetUnlockedWeaponSkills().Num() == 10
			&& Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary), TEXT("read_ten_greatsword_attacks_and_playable_primary_restored"));
		const auto* DiamondNode = Tree->FindNode(TEXT("F.DiamondStrike"));
		auto* Diamond = DiamondNode ? DiamondNode->Skill.LoadSynchronous() : nullptr;
		auto* Keys = PC->GetSkillBindingComponent();
		const auto IsKnuckle = [](const FRogue10mSavedWeaponSkillPreset& Preset)
		{
			return Preset.WeaponType == ERogue10mWeaponType::Knuckle;
		};
		const auto RestoredCombat = Combat ? Combat->CaptureCombat() : FRogue10mCombatSnapshot();
		const auto* RestoredKnuckle = RestoredCombat.WeaponPresets.FindByPredicate(IsKnuckle);
		const auto* SavedKnuckle = Saved->Combat.WeaponPresets.FindByPredicate(IsKnuckle);
		Check(Diamond && RestoredKnuckle && SavedKnuckle && RestoredKnuckle->Slots.FindRef(TestLoadoutSlot).Get() == Diamond
			&& SavedKnuckle->Slots.FindRef(TestLoadoutSlot).Get() == Diamond, TEXT("read_inactive_knuckle_Q_slot_restored_from_durable_preset"));
		const auto* IdentityCombat = Player->GetSkillTreeCombatComponent();
		Check(IdentityCombat && IdentityCombat->GetGauge() == 0.0f && IdentityCombat->GetCooldownRemaining() == 0.0f,
			TEXT("read_transient_identity_gauge_and_cooldown_start_empty"));
		Check(Persistence->HasDiscoveredNode(TEXT("G.EarthSplitter"))
			&& Persistence->HasDiscoveredNode(TEXT("G.Whirlwind"))
			&& Persistence->HasDiscoveredNode(TEXT("F.DiamondStrike"))
			&& !Persistence->HasDiscoveredNode(TEXT("F.Ultimate")),
			TEXT("cold_profile_restores_actual_discoveries_without_revealing_unlearned_techniques"));
		if (bInputQA)
		{
			const auto* Node = Tree->FindNode(TEXT("G.CrushingSlash"));
			auto* Crushing = Node ? Node->Skill.LoadSynchronous() : nullptr;
			Check(Keys && Combat && Crushing && Keys->GetSkillKey(Crushing) == EKeys::Q
				&& Combat->GetEquippedSkill(TestLoadoutSlot) == Crushing
				&& Saved->Combat.SkillBindings.FindRef(TestLoadoutSlot).Get() == Crushing,
				TEXT("read_active_sword_Q_slot_restored_from_disk"));
			PC->ToggleSkillTree();
			for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
			{ if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()) { Widget = *It; break; } }
			if (Widget.IsValid())
			{
				Widget->SelectNode(TEXT("G.EarthSplitter"));
				VerifySelectedDisclosure(TEXT("G.EarthSplitter"), false, false);
				Widget->SelectNode(TEXT("F.Ultimate"));
				VerifySelectedDisclosure(TEXT("F.Ultimate"), true, true);
			}
			Later(0.4, [this] { BeginInputChecks(ETrack::GreatSword, [this] { Finish(); }); });
			return;
		}
		Finish();
	}
	void ExerciseTemporaryBindings()
	{
		auto* Combat = Player->GetCombatComponent();
		const auto Baseline = Combat->CaptureCombat();
		auto* Primary = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		auto* Technique = Tree->FindNode(TEXT("F.DiamondStrike"))->Skill.LoadSynchronous();
		Combat->UnlockAllActiveTreeForTesting();
		Check(Combat->AssignSkillToInputSlot(Technique, TestLoadoutSlot),
			TEXT("temporary_test_skill_can_be_previewed"));
		const auto Preview = Combat->CaptureCombat();
		Check(Preview.SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary
			&& !Preview.SkillBindings.Contains(TestLoadoutSlot)
			&& !Preview.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Technique))
			&& Tree->GetMasteryTotalExperience(ETrack::Brawler) == 0,
			TEXT("test_preview_preserves_normal_primary_without_slot_unlock_or_XP_pollution"));
		Check(Combat->AssignSkillToInputSlot(Technique, ERogue10mAttackInputSlot::RSkill)
			&& !Combat->GetEquippedSkill(TestLoadoutSlot)
			&& Combat->CaptureCombat().SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary,
			TEXT("moving_test_preview_preserves_normal_primary"));
		Combat->UnassignSkillFromInputSlot(ERogue10mAttackInputSlot::RSkill);
		Combat->AssignSkillToInputSlot(Technique, ERogue10mAttackInputSlot::RSkill);
		Check(!Combat->CaptureCombat().SkillBindings.Contains(ERogue10mAttackInputSlot::RSkill),
			TEXT("explicitly_empty_slot_remains_empty_under_test_preview"));
		Check(Combat->RestoreCombat(Baseline) && Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) == Primary
			&& !Combat->IsAttackSkillUnlocked(Technique), TEXT("restore_clears_test_unlocks_and_restores_normal_loadout"));
	}
	ARogue10mBasicMonster* Monster(bool Training = false)
	{
		auto* Result = World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),
			FTransform(Player->GetActorLocation() + FVector(0, 600, 0)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		auto* Data = NewObject<URogue10mMonsterDataAsset>(Result); Data->MaxHealth = 10000; Data->ExperienceReward = 1;
		Data->bDestroyOnDeath = false; Data->DetectionRange = 0; Data->AttackDamage = 0;
		auto* Property = FindFProperty<FObjectPropertyBase>(Result->GetClass(), TEXT("MonsterData"));
		Property->SetObjectPropertyValue_InContainer(Result, Data);
		if (Training) { Result->MarkCombatTestInstance(); }
		Result->FinishSpawning(FTransform(Result->GetActorLocation()));
		if (auto* AI = Result->GetController()) { AI->UnPossess(); }
		Result->ClearAITarget();
		return Result;
	}
	void ExerciseDamage()
	{
		auto* A = Monster(); auto* B = Monster(); auto* Dummy = Monster(true);
		auto* Skill = NewObject<URogue10mAttackSkillData>(Player.Get());
		Skill->Damage = 1; Skill->HitMode = ERogue10mAttackHitMode::MultiHit; Skill->HitCount = 3;
		FRogue10mActiveAttackExecution Execution; Execution.ExecutionId = 9001; Execution.MasteryTrack = ETrack::Brawler;
		auto& Combat = *Player->GetCombatComponent();
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *A);
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *A);
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Execution, *B);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("three_hits_two_targets_one_mastery_XP"));
		FRogue10mActiveAttackExecution DummyExecution; DummyExecution.ExecutionId = 9002; DummyExecution.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, DummyExecution, *Dummy);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("combat_training_target_no_XP"));
		A->SetCanBeDamaged(false);
		FRogue10mActiveAttackExecution Immune; Immune.ExecutionId = 9003; Immune.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Immune, *A);
		Check(Tree->GetMasteryTotalExperience(ETrack::Brawler) == 1, TEXT("zero_applied_damage_no_XP"));
		B->GetRogueAttributeSet()->SetHealth(1);
		FRogue10mActiveAttackExecution Killing; Killing.ExecutionId = 9004; Killing.MasteryTrack = ETrack::Brawler;
		FRogue10mHybridCombatTestAccess::Hit(Combat, *Skill, Killing, *B);
		Check(B->IsDead() && Tree->GetMasteryTotalExperience(ETrack::Brawler) == 2, TEXT("killing_hit_awards_XP_once"));
		A->Destroy(); B->Destroy(); Dummy->Destroy();
	}
	void GreatSword(int32 Attempts = 25)
	{
		if (!Player->CanChangeWeapon() && Attempts > 0)
		{
			// The skill window pauses the hub. Let the previous attack finish before opening it.
			if (PC->IsSkillTreeVisible()) { PC->ToggleSkillTree(); }
			Later(0.15, [this, Attempts] { GreatSword(Attempts - 1); });
			return;
		}
		if (!PC->IsSkillTreeVisible()) { PC->ToggleSkillTree(); }
		Widget->HandleAction(2);
		const auto* BrowsingMainItem = Player->GetInventoryComponent()->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		Check(BrowsingMainItem && BrowsingMainItem->WeaponType == ERogue10mWeaponType::Knuckle
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::Knuckle,
			TEXT("browsing_greatsword_keeps_real_knuckle_inventory_character_and_combat"));
		Shot(TEXT("greatsword-locked-1080p.png"));
		Later(0.6, [this]
		{
			Widget->BrowseTab(ETab::Magic);
			auto* Inventory = Player->GetInventoryComponent();
			auto* Sword = LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_IronLongsword.DA_Item_IronLongsword"));
			Check(Inventory && Sword && Sword->WeaponType == ERogue10mWeaponType::GreatSword
				&& Sword->EquipSlotType == ERogue10mInventorySlotType::MainWeapon, TEXT("existing_greatsword_item_asset_is_valid_main_weapon_fixture"));
			if (Failures) { Finish(); return; }
			// Runtime fixture only. The read phase never adds/equips a weapon; it must restore the saved instance.
			if (Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon) != Sword)
			{
				Check(Player->CanChangeWeapon(), TEXT("actual_greatsword_fixture_waited_for_legal_weapon_change_state"));
				int32 ContainerIndex = INDEX_NONE; FGuid SwordInstance;
				Check(Inventory->TryAddGridItem(Sword, 1, ContainerIndex, SwordInstance) && SwordInstance.IsValid(), TEXT("one_existing_greatsword_QA_instance_added_through_inventory_API"));
				if (Failures) { Finish(); return; }
				Check(Inventory->TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType::MainWeapon)
					&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
					&& Widget->GetDisplayedTab() == ETab::Common
					&& Widget->GetTabOrder() == TArray<ETab>{ETab::Common, ETab::Knuckle, ETab::Sword, ETab::Magic},
					TEXT("unequipping_while_open_restores_common_default_and_base_order"));
				Check(Inventory->TryEquipGridItem(ContainerIndex, SwordInstance), TEXT("actual_inventory_equip_commits_greatsword_fixture"));
			}
			Check(Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon) == Sword
				&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
				&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::GreatSword
				&& Tree->GetActiveTrack() == ETrack::GreatSword, TEXT("greatsword_inventory_character_combat_and_mastery_track_are_consistent"));
			Check(Widget->GetDisplayedTab() == ETab::Sword
				&& Widget->GetTabOrder() == TArray<ETab>{ETab::Sword, ETab::Common, ETab::Knuckle, ETab::Magic}
				&& Widget->GetVisibleNodeCount() == 20 && Widget->FindNodeEntry(TEXT("G.Ultimate"))
				&& !Widget->FindNodeEntry(TEXT("F.Ultimate")),
				TEXT("equipping_greatsword_while_open_selects_and_moves_sword_first"));
			if (Failures) { Finish(); return; }
			VerifySwordGraphFrames();
			Check(Player->GetCombatComponent()->GetActiveSkillTreeSkills().Num() == 10
				&& Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5, TEXT("greatsword_five_basics_five_locked_techniques"));
			Check(Tree->GetMasteryLevel(ETrack::GreatSword) == 1 && Tree->GetMasteryLevel(ETrack::Brawler) == 12,
				TEXT("separate_mastery_tracks"));
			Check(Tree->GetActiveIdentity() == EIdentity::Mana, TEXT("sword_automatically_uses_its_strongest_compatible_identity"));
			FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 15;
			State->RestoreGrowth(Growth);
			auto Snapshot = Tree->Capture(); FRogue10mSkillTreeMasteryRecord Row; Row.Track = ETrack::GreatSword; Row.TotalExperience = 1045;
			Snapshot.Masteries.Add(Row);
			Check(Tree->Restore(Snapshot), TEXT("M20_greatsword_fixture_restore_and_auto_rewards"));
			Check(Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5,
				TEXT("M20_does_not_auto_grant_the_five_investment_techniques"));
			const int32 BeforeSwordPoints = Tree->GetAvailableGrowthPoints();
			Check(!Tree->TryUnlockNode(TEXT("G.EarthSplitter"), ERoute::Points, NAME_None)
				&& !Tree->TryUnlockNode(TEXT("G.CrushingSlash"), ERoute::Mastery, NAME_None)
				&& Tree->GetAvailableGrowthPoints() == BeforeSwordPoints,
				TEXT("sword_rejects_disconnected_purchase_and_free_mastery_without_spending"));
			Widget->SelectNode(TEXT("G.EarthSplitter"));
			Check(Tree->TryUnlockNode(TEXT("G.Whirlwind"), ERoute::Points, NAME_None),
				TEXT("sword_learns_first_side_prerequisite"));
			VerifySwordRouteCard(false);
			Check(!Tree->TryUnlockNode(TEXT("G.EarthSplitter"), ERoute::Points, NAME_None)
				&& Tree->GetAvailableGrowthPoints() == BeforeSwordPoints - 1,
				TEXT("sword_side_route_requires_both_prerequisites_without_extra_spend"));
			Check(Tree->TryUnlockNode(TEXT("G.GuardBreak"), ERoute::Points, NAME_None)
				&& !Tree->IsNodeUnlocked(TEXT("G.CrushingSlash")), TEXT("sword_completes_side_route_without_the_main_branch"));
			VerifySwordRouteCard(true);
			Widget->HandleAction(3);
			Check(Tree->IsNodeUnlocked(TEXT("G.EarthSplitter")), TEXT("available_mystery_can_be_learned_through_skill_tree_action"));
			VerifySelectedDisclosure(TEXT("G.EarthSplitter"), false, false);
			const auto SideRoute = Tree->Capture();
			Check(Tree->Respec(EIdentity::None) && Tree->TryUnlockNode(TEXT("G.Whirlwind"), ERoute::Points, NAME_None),
				TEXT("sword_discovery_fixture_keeps_one_learned_branch_after_respec"));
			VerifySelectedDisclosure(TEXT("G.EarthSplitter"), false, true);
			VerifyNodeDisclosure(TEXT("G.Whirlwind"), false, false, TEXT("T_WeaponSkillMedallions"));
			VerifyNodeDisclosure(TEXT("G.Ultimate"), true, true, TEXT("T_WeaponSkillMedallions"));
			auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
			Check(Persistence && Persistence->HasDiscoveredNode(TEXT("G.EarthSplitter"))
				&& !Tree->IsNodeUnlocked(TEXT("G.EarthSplitter")) && Persistence->SaveNow(),
				TEXT("discovery_is_recorded_before_save_even_when_skill_is_removed_immediately"));
			if (Failures) { Finish(); return; }
			const auto* SavedDiscovery = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(
				Persistence->GetSaveSlotForProfile(Persistence->GetAttachedProfileId()), 0));
			Check(SavedDiscovery && SavedDiscovery->DiscoveredNodes.Contains(TEXT("G.EarthSplitter"))
				&& !SavedDiscovery->SkillTree.Investments.ContainsByPredicate([](const auto& Row)
				{
					return Row.NodeId == TEXT("G.EarthSplitter");
				}), TEXT("serialized_history_keeps_a_discovered_skill_without_its_current_investment"));
			CaptureSwordDiscoveryStates(SideRoute, BeforeSwordPoints);
		});
	}
	void CaptureSwordDiscoveryStates(const FRogue10mSkillTreeSnapshot& SideRoute, int32 BeforeSwordPoints)
	{
		Shot(TEXT("greatsword-discovery-states-1080p.png"));
		Later(0.4, [this, SideRoute, BeforeSwordPoints]
		{
			PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
			Later(0.7, [this, SideRoute, BeforeSwordPoints]
			{
				Shot(TEXT("greatsword-discovery-states-720p.png"));
				Later(0.4, [this, SideRoute, BeforeSwordPoints]
				{
					PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
					Later(0.7, [this, SideRoute, BeforeSwordPoints] { CompleteSwordInvestments(SideRoute, BeforeSwordPoints); });
				});
			});
		});
	}
	void CompleteSwordInvestments(const FRogue10mSkillTreeSnapshot& SideRoute, int32 BeforeSwordPoints)
	{
		Check(Tree->Restore(SideRoute) && Tree->TryUnlockNode(TEXT("G.Ultimate"), ERoute::Points, NAME_None),
			TEXT("sword_web_can_reach_earth_splitter_and_ultimate_via_side_branches"));
		Widget->HandleAction(100, TEXT("G.CrushingSlash"));
		Widget->HandleAction(3);
		Check(!Tree->TryUnlockNode(TEXT("G.Whirlwind"), ERoute::Points, NAME_None)
			&& Tree->IsNodeUnlocked(TEXT("G.CrushingSlash"))
			&& Tree->GetAvailableGrowthPoints() == BeforeSwordPoints - 5
			&& Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 10,
			TEXT("sword_widget_purchase_and_other_paths_grant_all_attacks_for_exactly_five_points"));
		const auto InvestedSword = Tree->Capture();
		Check(Tree->IsNodeUnlocked(TEXT("G.Power.2")) && Tree->Respec(EIdentity::None)
			&& Tree->GetAvailableGrowthPoints() == 14 && !Tree->IsNodeUnlocked(TEXT("G.Power.2"))
			&& Tree->IsNodeUnlocked(TEXT("G.Power.1")) && Tree->GetMasteryLevel(ETrack::GreatSword) == 20
			&& Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5,
			TEXT("full_respec_refunds_paid_sword_paths_and_prunes_dependents_without_losing_mastery"));
		Check(Tree->Restore(InvestedSword), TEXT("paid_sword_paths_restore_before_identity_checks"));
		Check(Tree->GetIdentityRank(EIdentity::Mana) == 16
			&& Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana),
			TEXT("sword_ultimate_and_mana_rank_activate_resonance_without_a_purchase"));
		if (Failures) { Finish(); return; }
		IdentityAndSave();
		CaptureSwordPanel();
	}
	void VerifySwordGraphFrames()
	{
		for (const TCHAR* Technique : {TEXT("G.CrushingSlash"), TEXT("G.Whirlwind"), TEXT("G.GuardBreak"),
			TEXT("G.EarthSplitter"), TEXT("G.Ultimate")})
		{
			const FName NodeId(Technique);
			const auto Frame = Widget->GetNodeOuterFramePoints(NodeId);
			Check(Widget->FindNodeEntry(NodeId) && Widget->GetNodeFrameShape(NodeId) == TEXT("SwordMedallion")
				&& Widget->GetNodeFrameOutlineCount(NodeId) == 3 && Frame.Num() == 49 && Frame[0].Equals(Frame.Last()),
				*FString::Printf(TEXT("sword_paid_technique_has_closed_medallion_%s"), Technique));
		}
		Check(Widget->GetNodeFrameShape(TEXT("G.Power.1")) == TEXT("SwordSocket")
			&& Widget->GetNodeFrameOutlineCount(TEXT("G.Power.1")) == 2,
			TEXT("sword_mastery_stat_uses_smaller_socket_frame"));
	}
	void VerifySwordRouteCard(bool bSideReady)
	{
		const auto* Card = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("HybridTreeDetailRequirements")));
		const auto* LearnButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("HybridTreeLearnPoints")));
		const FString Expected = FString(TEXT("선행 경로 · 아래 중 하나\n미완료 · 공격력 +2 (숙련 M8)\n"))
			+ (bSideReady ? TEXT("완료 · 회전참 + 파쇄참") : TEXT("미완료 · 회전참 + 미발견"));
		Check(Card && Card->GetVisibility() != ESlateVisibility::Collapsed
			&& Card->GetText().ToString().StartsWith(Expected)
			&& LearnButton && LearnButton->GetIsEnabled() == bSideReady,
			bSideReady ? TEXT("rendered_sword_card_shows_ready_alternative_and_missing_main_route")
				: TEXT("rendered_sword_card_masks_never_learned_alternative_and_keeps_it_missing"));
		VerifySelectedDisclosure(TEXT("G.EarthSplitter"), true, true);
	}
	void CaptureSwordPanel()
	{
		Widget->SelectNode(TEXT("G.EarthSplitter"));
		Later(0.35, [this]
		{
			VerifyWeaponPresentation(TEXT("G.EarthSplitter"), TEXT("HybridTreeGreatSword"));
			Shot(TEXT("greatsword-learned-1080p.png"));
			Later(0.35, [this] { CaptureSwordPanelAt720p(); });
		});
	}
	void CaptureSwordPanelAt720p()
	{
		PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
		Later(0.7, [this]
		{
			Shot(TEXT("greatsword-720p.png"));
			Later(0.6, [this]
			{
				if (bInputQA) { BeginInputChecks(ETrack::GreatSword, [this] { CompleteRuntime(); }); }
				else { CompleteRuntime(); }
			});
		});
	}
	void CompleteRuntime()
	{
		if (!bCommonQA || Failures) { Finish(); return; }
		PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
		if (!PC->IsSkillTreeVisible()) { PC->ToggleSkillTree(); }
		Widget->BrowseTab(ETab::Common);
		auto Growth = State->CaptureGrowth();
		Growth.Level = 29;
		Check(State->RestoreGrowth(Growth) && Tree->GetAvailableGrowthPoints() == 23,
			TEXT("common_fixture_keeps_all_twenty_three_points_after_sword_investments"));
		const auto* Movement = Player->GetCharacterMovement();
		CommonBaseWalkSpeed = Movement->MaxWalkSpeed;
		CommonBaseAcceleration = Movement->MaxAcceleration;
		CommonBaseAirControl = Movement->AirControl;
		CommonBaseAirBraking = Movement->BrakingDecelerationFalling;
		Check(Player->JumpMaxCount == 1, TEXT("common_fixture_greatsword_has_one_jump_before_training"));
		Later(0.7, [this]
		{
			ClickWidget(Widget->FindNodeEntry(TEXT("C.MoveSpeed")));
			Check(Widget->GetSelectedNodeId() == TEXT("C.MoveSpeed"), TEXT("common_move_speed_node_hit_target"));
			VerifySelectedDisclosure(TEXT("C.MoveSpeed"), true, true);
			Later(0.25, [this] { InvestCommonWeb(); });
		});
	}
	void InvestCommonWeb()
	{
		Click(TEXT("HybridTreeLearnPoints"));
		Check(Tree->GetAvailableGrowthPoints() == 22 && Tree->IsCommonSkillLearned(ERogue10mCommonSkill::MoveSpeed)
			&& Player->GetCharacterMovement()->MaxWalkSpeed > CommonBaseWalkSpeed, TEXT("common_real_UI_purchase_spends_one_point_and_applies_speed"));
		VerifySelectedDisclosure(TEXT("C.MoveSpeed"), false, false);
		Check(!Tree->TryUnlockNode(TEXT("C.RollCooldown"), ERoute::Points, NAME_None) && Tree->GetAvailableGrowthPoints() == 22,
			TEXT("locked_common_skill_cannot_spend_points"));
		for (const TCHAR* Id : {TEXT("C.GroundTurning"), TEXT("C.AirMomentum"), TEXT("C.DoubleJump")})
		{
			Check(Tree->TryUnlockNode(Id, ERoute::Points, NAME_None), Id);
		}
		Check(Player->JumpMaxCount >= 2 && !Tree->IsCommonSkillLearned(ERogue10mCommonSkill::AirControl)
			&& Player->GetCharacterMovement()->BrakingDecelerationFalling < CommonBaseAirBraking
			&& Tree->GetAvailableGrowthPoints() == 19, TEXT("common_cross_path_grants_two_jumps_on_greatsword_without_air_control"));
		Widget->SelectNode(TEXT("C.DoubleJump"));
		Later(0.35, [this]
		{
			Shot(TEXT("common-1080p.png"));
			Later(0.35, [this]
			{
				PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
				Later(0.7, [this]
				{
					Shot(TEXT("common-720p.png"));
					Later(0.35, [this]
					{
						if (bCommonPresentationQA) { VerifyCommonPresentation(); }
						ExerciseCommonMovement();
					});
				});
			});
		});
	}
	bool HasMedallion(const UUserWidget* Entry, FName Atlas) const
	{
		bool bHasMedallion = false;
		if (!Entry || !Entry->WidgetTree) { return false; }
		Entry->WidgetTree->ForEachWidget([&bHasMedallion, Atlas](UWidget* Child)
		{
			const auto* Icon = Cast<UImage>(Child);
			bHasMedallion |= Icon && GetFNameSafe(Icon->GetBrush().GetResourceObject()) == Atlas
				&& static_cast<FBox2f>(Icon->GetBrush().GetUVRegion()).bIsValid && Icon->IsVisible();
		});
		return bHasMedallion;
	}
	void VerifyNodeDisclosure(FName NodeId, bool bMystery, bool bLocked, FName Atlas)
	{
		const auto* Entry = Widget->FindNodeEntry(NodeId);
		const auto* Glyph = Entry && Entry->WidgetTree
			? Cast<UTextBlock>(Entry->WidgetTree->FindWidget(TEXT("HybridTreeMysteryGlyph"))) : nullptr;
		const auto* Definition = Tree->FindNode(NodeId);
		const FString Tooltip = Entry ? Entry->GetToolTipText().ToString() : FString();
		FString ExpectedLabel = bMystery || !Definition ? TEXT("미발견") : Definition->DisplayName.ToString();
		if (Definition && Definition->Track == ETrack::GreatSword && Definition->PointCost > 0 && bLocked)
		{
			ExpectedLabel += FString::Printf(TEXT(" · %dP"), Definition->PointCost);
		}
		Check(Entry && Definition && Entry->IsMystery() == bMystery && Entry->HasLockBadge() == bLocked
			&& Widget->GetNodeLabel(NodeId).ToString() == ExpectedLabel
			&& HasMedallion(Entry, Atlas) == !bMystery
			&& (!bMystery || (Glyph && Glyph->IsVisible() && Glyph->GetText().ToString() == TEXT("?")
				&& !Tooltip.Contains(Definition->DisplayName.ToString())
				&& (Definition->Description.IsEmpty() || !Tooltip.Contains(Definition->Description.ToString()))))
			&& (!bLocked || !Entry->CanDragSkill()),
			*FString::Printf(TEXT("node_disclosure_matches_icon_label_tooltip_and_drag_%s"), *NodeId.ToString()));
	}
	void VerifySelectedDisclosure(FName NodeId, bool bMystery, bool bLocked)
	{
		const auto* Definition = Tree->FindNode(NodeId);
		if (!Definition)
		{
			Check(false, TEXT("disclosure_fixture_node_exists"));
			return;
		}
		const FName Atlas = Definition->CommonSkill == ERogue10mCommonSkill::None
			? TEXT("T_WeaponSkillMedallions") : TEXT("T_CommonSkillMedallions");
		VerifyNodeDisclosure(NodeId, bMystery, bLocked, Atlas);
		const auto* Name = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("HybridTreeDetailName")));
		const auto* Description = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("HybridTreeDetailDescription")));
		const auto* Portrait = Cast<URogue10mHybridSkillTreeEntryWidget>(Widget->GetWidgetFromName(TEXT("HybridTreeCommonDetailIcon")));
		const auto* Skill = Widget->GetSelectedAttackSkill();
		const FString ExpectedName = bMystery ? TEXT("미발견") : Definition->DisplayName.ToString();
		Check(Widget->GetSelectedNodeId() == NodeId && Name && Description && Portrait
			&& Name->GetText().ToString() == ExpectedName
			&& Portrait->IsMystery() == bMystery && Portrait->HasLockBadge() == bLocked
			&& HasMedallion(Portrait, Atlas) == !bMystery
			&& (!bMystery || (!Skill && Description->GetText().ToString() == TEXT("처음 습득하면 기술의 이름과 효과가 공개됩니다.")))
			&& (bMystery || !Skill || Definition->Track != ETrack::GreatSword
				|| Description->GetText().ToString().Contains(Skill->GetAllowedSlotText().ToString())),
			*FString::Printf(TEXT("selected_card_exposes_only_discovered_skill_details_%s"), *NodeId.ToString()));
	}
	void VerifyWeaponPresentation(FName HighlightedNode, FName ActiveTab)
	{
		const FName WeaponAtlas(TEXT("T_WeaponSkillMedallions"));
		for (const auto& Node : Widget->GetDisplayNodes())
		{
			VerifyNodeDisclosure(Node.NodeId, Widget->IsMysteryNode(Node), !Tree->IsNodeUnlocked(Node.NodeId), WeaponAtlas);
		}
		Check(HasMedallion(Widget->FindNodeEntry(HighlightedNode), WeaponAtlas)
			&& HasMedallion(Cast<UUserWidget>(Widget->GetWidgetFromName(TEXT("HybridTreeCommonDetailIcon"))), WeaponAtlas),
			TEXT("weapon_graph_and_detail_use_bronze_medallion_atlas"));
		const auto* Active = Cast<UButton>(Widget->GetWidgetFromName(ActiveTab));
		const auto* Common = Cast<UButton>(Widget->GetWidgetFromName(TEXT("HybridTreeCommon")));
		Check(Active && Common && Active->GetStyle().Normal.DrawAs == ESlateBrushDrawType::RoundedBox
			&& Active->GetStyle().Normal.TintColor.GetSpecifiedColor() != Common->GetStyle().Normal.TintColor.GetSpecifiedColor(),
			TEXT("weapon_active_tab_uses_shared_gold_tab_fill"));
	}
	void VerifyCommonPresentation()
	{
		const FName CommonAtlas(TEXT("T_CommonSkillMedallions"));
		for (const auto& Node : Tree->GetCatalog()->Nodes)
		{
			if (Node.CommonSkill != ERogue10mCommonSkill::None)
			{
				VerifyNodeDisclosure(Node.NodeId, Widget->IsMysteryNode(Node), !Tree->IsNodeUnlocked(Node.NodeId), CommonAtlas);
			}
		}
		Check(HasMedallion(Cast<UUserWidget>(Widget->GetWidgetFromName(TEXT("HybridTreeCommonDetailIcon"))), CommonAtlas)
			&& Widget->GetSelectedNodeId() == TEXT("C.DoubleJump"), TEXT("common_detail_medallion_shows_selected_double_jump"));
		const auto* Active = Cast<UButton>(Widget->GetWidgetFromName(TEXT("HybridTreeCommon")));
		const auto* Inactive = Cast<UButton>(Widget->GetWidgetFromName(TEXT("HybridTreeGreatSword")));
		Check(Active && Inactive && Active->GetStyle().Normal.DrawAs == ESlateBrushDrawType::RoundedBox
			&& Active->GetStyle().Normal.TintColor.GetSpecifiedColor() != Inactive->GetStyle().Normal.TintColor.GetSpecifiedColor(),
			TEXT("common_active_tab_has_its_own_visible_fill"));
	}
	void ExerciseCommonMovement()
	{
		for (const TCHAR* Id : {TEXT("C.DoubleJumpUpgrade"), TEXT("C.JumpHeight"), TEXT("C.AirControl"), TEXT("C.LandingStability"),
			TEXT("C.Roll"), TEXT("C.RollCooldown"), TEXT("C.EvadeDistance"), TEXT("C.EvadeRecovery"),
			TEXT("C.EvadeHaste"), TEXT("C.GroundAcceleration"), TEXT("C.MoveSpeedUpgrade")})
		{
			Check(Tree->TryUnlockNode(Id, ERoute::Points, NAME_None), Id);
		}
		Check(Tree->GetAvailableGrowthPoints() == 8 && Player->GetCharacterMovement()->MaxAcceleration > CommonBaseAcceleration
			&& Player->GetCharacterMovement()->AirControl > CommonBaseAirControl, TEXT("common_full_web_spends_fifteen_points_and_applies_acceleration_and_air_control"));
		if (bCommonPresentationQA) { Finish(); return; }
		PC->CloseAllBlockingPanels();
		WaitForCommonWeaponSwap(25);
	}
	void WaitForCommonWeaponSwap(int32 Attempts)
	{
		if (!Player->CanChangeWeapon() && Attempts > 0)
		{
			Later(0.15, [this, Attempts] { WaitForCommonWeaponSwap(Attempts - 1); });
			return;
		}
		Check(Player->CanChangeWeapon(), TEXT("common_swap_fixture_waits_for_attack_recovery"));
		auto* Inventory = Player->GetInventoryComponent();
		const auto Equipped = Inventory->CaptureInventory();
		Check(Inventory->TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType::MainWeapon)
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed && Player->JumpMaxCount >= 2
			&& Tree->IsCommonSkillLearned(ERogue10mCommonSkill::Roll), TEXT("common_unequipping_keeps_jump_and_roll_available"));
		Check(Inventory->RestoreInventory(Equipped) && Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
			&& Player->GetCharacterMovement()->MaxWalkSpeed > CommonBaseWalkSpeed, TEXT("common_greatsword_re_equip_retains_movement_passives"));
		Later(0.5, [this] { VerifyCommonJump(); });
	}
	void VerifyCommonJump()
	{
		auto* Movement = Player->GetCharacterMovement();
		if (!Movement->IsMovingOnGround()) { Check(false, TEXT("common_fixture_requires_ground_for_jump")); Finish(); return; }
		GameKey(EKeys::SpaceBar, true);
		Later(0.26, [this]
		{
			GameKey(EKeys::SpaceBar, false);
			Check(Player->GetCharacterMovement()->IsFalling() && Player->JumpCurrentCount == 1, TEXT("common_first_space_press_leaves_ground"));
			Later(0.08, [this]
			{
				const double BeforeAirJumpVelocity = Player->GetVelocity().Z;
				GameKey(EKeys::SpaceBar, true);
				Later(0.12, [this, BeforeAirJumpVelocity]
				{
					GameKey(EKeys::SpaceBar, false);
					Check(Player->JumpCurrentCount == 2 && Player->GetVelocity().Z > BeforeAirJumpVelocity,
						TEXT("common_second_space_input_launches_the_equipped_sword_pawn_in_air"));
				Later(0.8, [this] { VerifyCommonRoll(18); });
				});
			});
		});
	}
	void VerifyCommonRoll(int32 Attempts)
	{
		if (!Player->GetCharacterMovement()->IsMovingOnGround() && Attempts > 0)
		{
			Later(0.12, [this, Attempts] { VerifyCommonRoll(Attempts - 1); });
			return;
		}
		Check(Player->GetCharacterMovement()->IsMovingOnGround(), TEXT("common_roll_starts_after_landing"));
		BeginRollMotionEvidence();
		Later(0.12, [this] { VerifyCommonRollInput(); });
	}
	FVector GetRollAuditDirection() const
	{
		if (RollMoveKey == EKeys::D) { return Player->GetActorRightVector().GetSafeNormal2D(); }
		if (RollMoveKey == EKeys::S) { return -Player->GetActorForwardVector().GetSafeNormal2D(); }
		return Player->GetActorForwardVector().GetSafeNormal2D();
	}
	void VerifyCommonRollInput()
	{
		const double PelvisBefore = LogCommonRollPelvis(TEXT("ready"));
		RollPelvisBefore = PelvisBefore;
		RecordRollMotion(TEXT("ready"), 0.0f);
		Shot(TEXT("roll-ready.png"));
		if (RollMoveKey.IsValid()) { GameKey(RollMoveKey, true); }
		Later(RollMoveKey.IsValid() ? 0.10 : 0.12, [this, PelvisBefore]
		{
			const FVector Origin = Player->GetActorLocation();
			if (!RollAuditDirection.IsEmpty())
			{
				RollExpectedDirection = GetRollAuditDirection();
				Player->GetCharacterMovement()->StopMovementImmediately();
			}
			GameKey(Rogue10mInput::DodgeKey(), true);
			Later(RollMoveKey.IsValid() ? 0.05 : 0.12, [this, Origin, PelvisBefore]
			{
				if (RollMoveKey.IsValid()) { GameKey(RollMoveKey, false); }
				GameKey(Rogue10mInput::DodgeKey(), false);
				Later(0.75, [this, Origin, PelvisBefore]
				{
					Check(bRollTuckCaptured, TEXT("common_roll_observed_a_tucked_simulation_frame"));
					const double Travel = FVector::Dist2D(Origin, Player->GetActorLocation());
					Check(!Player->IsRolling() && !Player->IsDodging() && Travel > 100.0 && Travel < 370.0,
						TEXT("common_roll_travels_plausible_distance_and_finishes"));
					UE_LOG(LogRogue10m, Display, TEXT("COMMON_ROLL travel_cm=%.2f"), Travel);
					if (!RollAuditDirection.IsEmpty())
					{
						const FVector Displacement = Player->GetActorLocation() - Origin;
						const double Along = FVector::DotProduct(Displacement, RollExpectedDirection);
						const double Alignment = FVector::DotProduct(RollObservedDirection, RollExpectedDirection);
						Check(Alignment > 0.98 && Along >= Travel * 0.95, TEXT("actual_ctrl_roll_matches_held_direction"));
						Report += FString::Printf(TEXT("roll_direction=%s alignment=%.6f travel_cm=%.4f along_cm=%.4f expected=(%.4f,%.4f) actual=(%.4f,%.4f)\n"),
							*RollAuditDirection, Alignment, Travel, Along, RollExpectedDirection.X, RollExpectedDirection.Y,
							RollObservedDirection.X, RollObservedDirection.Y);
					}
					const double PelvisAfter = LogCommonRollPelvis(TEXT("recovered"));
					Check(FMath::Abs(PelvisBefore - PelvisAfter) < 15.0, TEXT("common_roll_restores_the_standing_pose"));
					RecordRollMotion(TEXT("recovered"), 1.0f);
					Shot(TEXT("roll-recovered.png"));
					Later(0.35, [this] { Finish(); });
				});
			});
		});
	}
	void BeginRollMotionEvidence()
	{
		const auto* Source = Player->GetAnimationPlaybackMesh();
		const auto* Anim = Source ? Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()) : nullptr;
		const auto* Sequence = Anim ? Anim->GetLearnedRollAnimation() : nullptr;
		Check(Sequence && Sequence->GetPlayLength() > 0.0f, TEXT("common_roll_has_loaded_authored_sequence"));
		if (Sequence)
		{
			Report += FString::Printf(TEXT("roll_sequence=%s length_s=%.6f\n"), *Sequence->GetPathName(), Sequence->GetPlayLength());
		}
		RollMotionRows = TEXT("phase,frame,world_s,progress,body_revision,capsule_x_cm,capsule_y_cm,capsule_z_cm,capsule_bottom_z_cm,walkable_floor,floor_z_cm,sword_bounds_clearance_cm,bone,bone_x_cm,bone_y_cm,bone_z_cm,bone_qx,bone_qy,bone_qz,bone_qw\n");
		RollMotionHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self = AsShared()](UWorld* InWorld, ELevelTick, float)
		{
			float Progress = 0.0f;
			FVector Direction;
			if (InWorld != Self->World.Get() || !Self->Player.IsValid() || Self->RollMotionFrames >= 120
				|| !Self->Player->GetRollPresentation(Progress, Direction)) { return; }
			++Self->RollMotionFrames;
			Self->RollObservedDirection = Direction.GetSafeNormal2D();
			Self->RollMotionMin = FMath::Min(Self->RollMotionMin, Progress);
			Self->RollMotionMax = FMath::Max(Self->RollMotionMax, Progress);
			Self->RecordRollMotion(TEXT("active"), Progress);
			if (Self->RollPhotoCamera.IsValid() && !Self->RollAuditDirection.IsEmpty())
			{
				if (!Self->bRollReleaseEntryCaptured && Progress >= 0.50f)
				{
					Self->bRollReleaseEntryCaptured = true;
					Self->Shot(TEXT("roll-release-entry.png"));
				}
				if (!Self->bRollReleaseExitCaptured && Progress >= 0.60f)
				{
					Self->bRollReleaseExitCaptured = true;
					Self->Shot(TEXT("roll-release-exit.png"));
				}
			}
			if (!Self->bRollTuckCaptured && Progress >= 0.54f && Progress <= 0.70f)
			{
				Self->CaptureRollTuck();
			}
		});
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) { return; }
		const FVector TravelDirection = GetRollAuditDirection();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, TravelDirection);
		const FVector Target = Player->GetActorLocation() + TravelDirection * 140.0 - FVector::UpVector * 24.0;
		const FVector Position = Target + Side * 385.0
			- TravelDirection * 120.0 + FVector::UpVector * 45.0;
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		RollPhotoCamera = World->SpawnActor<ACameraActor>(Position, (Target - Position).Rotation(), Params);
		Check(RollPhotoCamera.IsValid(), TEXT("common_roll_fixed_side_capture_camera"));
		if (RollPhotoCamera.IsValid())
		{
			RollPreviousViewTarget = PC->GetViewTarget();
			PC->SetViewTarget(RollPhotoCamera.Get());
		}
	}
	void CaptureRollTuck()
	{
		bRollTuckCaptured = true;
		const auto* Source = Player->GetAnimationPlaybackMesh();
		const auto* Anim = Source ? Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()) : nullptr;
		Check(Anim && Anim->WasLearnedRollPoseApplied(), TEXT("common_roll_tuck_uses_the_authored_pose"));
		Check(Player->GetDodgeCooldownRemaining() > 0.0f, TEXT("common_dodge_key_starts_timed_grounded_roll_with_cooldown"));
		const auto* AttackNode = Tree->FindNode(TEXT("G.CrushingSlash"));
		auto* Attack = bRollOnlyQA ? Player->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary)
			: AttackNode ? AttackNode->Skill.LoadSynchronous() : nullptr;
		Check(Attack && !Player->GetCombatComponent()->ActivateTreeSkill(Attack), TEXT("common_roll_blocks_attack_acceptance_until_recovery"));
		Check(RollPelvisBefore - LogCommonRollPelvis(TEXT("active")) > 15.0,
			TEXT("common_roll_lowers_rendered_pelvis_into_tuck"));
		Shot(TEXT("roll-active.png"));
	}
	void RecordRollMotion(const TCHAR* Phase, float Progress)
	{
		const auto* Mesh = Player->GetMesh();
		const auto* Capsule = Player->GetCapsuleComponent();
		if (!Mesh || !Capsule) { bRollMotionValid = false; return; }
		const FVector Origin = Capsule->GetComponentLocation();
		const auto& Floor = Player->GetCharacterMovement()->CurrentFloor;
		const double FloorZ = Floor.IsWalkableFloor() ? Floor.HitResult.ImpactPoint.Z : 0.0;
		const auto* Visual = Player->FindComponentByClass<URogue10mWeaponVisualComponent>();
		const auto* Sword = Visual ? Visual->GetRightWeaponMesh() : nullptr;
		const UStaticMesh* SwordAsset = Sword ? Sword->GetStaticMesh().Get() : nullptr;
		double SwordClearance = 0.0;
		if (SwordAsset && Floor.IsWalkableFloor())
		{
			const FBox Bounds = SwordAsset->GetBoundingBox().TransformBy(Sword->GetComponentTransform());
			SwordClearance = Bounds.Min.Z - FloorZ;
			MinRollSwordClearance = FMath::Min(MinRollSwordClearance, SwordClearance);
		}
		else
		{
			bRollMotionValid = false;
		}
		const FString Measure = FString::Printf(TEXT("%s,%llu,%.6f,%.6f,%u,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%.4f"),
			Phase, GFrameCounter, World->GetTimeSeconds(), Progress, Mesh->GetBoneTransformRevisionNumber(),
			Origin.X, Origin.Y, Origin.Z, Origin.Z - Capsule->GetScaledCapsuleHalfHeight(), Floor.IsWalkableFloor(), FloorZ, SwordClearance);
		static const FName Bones[] = { TEXT("pelvis"), TEXT("head"), TEXT("clavicle_l"), TEXT("clavicle_r"),
			TEXT("hand_l"), TEXT("hand_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r") };
		static const FName ExtraBones[] = { TEXT("thigh_l"), TEXT("thigh_r"), TEXT("ball_l"), TEXT("ball_r"),
			TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("neck_01") };
		bRollMotionValid &= !Origin.ContainsNaN() && FMath::IsFinite(FloorZ) && FMath::IsFinite(Progress);
		const auto RecordBone = [&](const USkeletalMeshComponent* SampleMesh, FName Bone, const TCHAR* Prefix)
		{
			if (!SampleMesh || SampleMesh->GetBoneIndex(Bone) == INDEX_NONE) { bRollMotionValid = false; return; }
			const FTransform Pose = SampleMesh->GetBoneTransform(Bone, RTS_World);
			const FVector Location = Pose.GetLocation();
			const FQuat Rotation = Pose.GetRotation();
			bRollMotionValid &= !Pose.ContainsNaN() && Rotation.IsNormalized();
			RollMotionRows += FString::Printf(TEXT("%s,%s%s,%.4f,%.4f,%.4f,%.7f,%.7f,%.7f,%.7f\n"),
				*Measure, Prefix, *Bone.ToString(), Location.X, Location.Y, Location.Z, Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
		};
		for (const FName Bone : Bones) { RecordBone(Mesh, Bone, TEXT("")); }
		if (!RollAuditDirection.IsEmpty())
		{
			const auto* Source = Player->GetAnimationPlaybackMesh();
			for (const FName Bone : Bones) { RecordBone(Source, Bone, TEXT("source.")); }
			for (const FName Bone : ExtraBones)
			{
				RecordBone(Mesh, Bone, TEXT(""));
				RecordBone(Source, Bone, TEXT("source."));
			}
		}
	}
	double LogCommonRollPelvis(const TCHAR* Stage)
	{
		const auto* Mesh = Player->GetMesh();
		Check(Mesh && Mesh->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE && Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE,
			TEXT("common_roll_visible_body_has_sampled_bones"));
		if (!Mesh) { return 0.0; }
		const FVector Origin = Player->GetActorLocation();
		const FVector Pelvis = Mesh->GetBoneLocation(TEXT("pelvis")) - Origin;
		const FVector Hand = Mesh->GetBoneLocation(TEXT("hand_r")) - Origin;
		UE_LOG(LogRogue10m, Display, TEXT("COMMON_ROLL %s t=%.3f capsule=%s pelvis=%s right_hand=%s"), Stage,
			World->GetTimeSeconds(), *Origin.ToCompactString(), *Pelvis.ToCompactString(), *Hand.ToCompactString());
		return Pelvis.Z;
	}
	void IdentityAndSave()
	{
		auto* CombatIdentity = Player->GetSkillTreeCombatComponent();
		auto* Ultimate = Tree->FindNode(TEXT("G.Ultimate"))->Skill.LoadSynchronous();
		for (int32 I = 0; I < 4; ++I) { CombatIdentity->NotifyConfirmedAttack(ETrack::GreatSword); }
		State->GetRogueAttributeSet()->SetMana(0);
		Check(!CombatIdentity->AcceptAttack(*Ultimate, false).bTriggered && CombatIdentity->GetGauge() == 100,
			TEXT("mana_proc_requires_MP_preserves_gauge_when_short"));
		auto* Attributes = State->GetRogueAttributeSet();
		Attributes->SetMana(Attributes->GetMaxMana());
		const float ManaBeforeProc = Attributes->GetMana();
		const auto Effect = CombatIdentity->AcceptAttack(*Ultimate, false);
		const auto* Catalog = Tree->GetCatalog();
		const float ExpectedDamage = 1.0f + Catalog->ManaBaseBonus + 16.0f * Catalog->IdentityDamageBonusPerRank
			+ Tree->GetResonanceDamageBonus(ETrack::GreatSword, EIdentity::Mana);
		Check(Effect.bTriggered && FMath::IsNearlyEqual(Effect.DamageMultiplier, ExpectedDamage, UE_KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(Attributes->GetMana(), ManaBeforeProc - Catalog->ManaProcCost),
			TEXT("mana_rank_and_both_resonances_increase_damage_and_consume_MP"));
		const float Cooldown = CombatIdentity->GetCooldownRemaining();
		Check(CombatIdentity->GetGauge() == 0 && Cooldown > 0
			&& !CombatIdentity->AcceptAttack(*Ultimate, false).bTriggered
			&& CombatIdentity->GetCooldownRemaining() >= Cooldown - .1f,
			TEXT("identity_proc_drains_gauge_and_respects_shared_cooldown"));
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Persistence->SaveNow(), TEXT("actual_isolated_profile_save"));
		Check(Persistence->HasDiscoveredNode(TEXT("G.Resonance.Pursuit"))
			&& Persistence->HasDiscoveredNode(TEXT("G.Resonance.Spread")),
			TEXT("derived_ultimate_resonances_are_discovered_on_checkpoint"));
		auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(
			Persistence->GetSaveSlotForProfile(Persistence->GetAttachedProfileId()), 0));
		Check(Saved && URogue10mProgressionSaveGame::CurrentVersion == 5 && Saved->SaveVersion == 5
			&& Saved->RunId.IsValid() && Saved->RunId == Persistence->GetAttachedRunId()
			&& Saved->LobbyEarnedPoints == 0 && Saved->Town.TraitInvestments.IsEmpty()
			&& Saved->SkillTree.Masteries.Num() == 2 && Saved->SkillTree.IdentityConsumableRanks.FindRef(EIdentity::Mana) == 2,
			TEXT("disk_current_v5_roundtrip_preserves_run_mastery_identity_and_zero_lobby_ledger"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(1500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Check(!Tree->CanChangeInvestments() && !Tree->TryUnlockNode(TEXT("C.MoveSpeed"), ERoute::Points, NAME_None),
			TEXT("out_of_NPC_range_rejects_skill_investment"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
	}
	void Finish()
	{
		if (Done) { return; }
		FWorldDelegates::OnWorldPostActorTick.Remove(EProbeHandle);
		FWorldDelegates::OnWorldPostActorTick.Remove(RollMotionHandle);
		if (PC.IsValid()) { GameKey(EKeys::E, false); }
		if (!RollMotionRows.IsEmpty())
		{
			if (PC.IsValid()) { GameKey(Rogue10mInput::DodgeKey(), false); }
			if (RollPhotoCamera.IsValid())
			{
				for (const TCHAR* Name : { TEXT("roll-ready.png"), TEXT("roll-active.png"), TEXT("roll-recovered.png") })
				{
					Check(IFileManager::Get().FileSize(*FPaths::Combine(Output, Name)) > 1024, TEXT("common_roll_profile_capture_written"));
				}
				if (!RollAuditDirection.IsEmpty())
				{
					for (const TCHAR* Name : {TEXT("roll-release-entry.png"), TEXT("roll-release-exit.png")})
					{
						Check(IFileManager::Get().FileSize(*FPaths::Combine(Output, Name)) > 1024, TEXT("directional_roll_release_capture_written"));
					}
				}
				if (PC.IsValid() && RollPreviousViewTarget.IsValid()) { PC->SetViewTarget(RollPreviousViewTarget.Get()); }
				RollPhotoCamera->Destroy();
			}
			Report += FString::Printf(TEXT("roll_samples=%d progress_min=%.4f progress_max=%.4f\n"),
				RollMotionFrames, RollMotionMin, RollMotionMax);
			Report += FString::Printf(TEXT("sword_bounds_min_clearance_cm=%.3f\n"), MinRollSwordClearance);
			Check(bRollMotionValid, TEXT("common_roll_visible_bone_transforms_finite_and_normalized"));
			Check(RollMotionFrames >= 3 && RollMotionMin < 0.35f && RollMotionMax > 0.65f, TEXT("common_roll_motion_sampled_across_active_phases"));
			Check(MinRollSwordClearance >= -2.0, TEXT("common_roll_keeps_equipped_sword_above_floor"));
			Check(FFileHelper::SaveStringToFile(RollMotionRows, *FPaths::Combine(Output, TEXT("roll-motion.csv"))), TEXT("common_roll_motion_csv_saved"));
		}
		if (bInputQA)
		{
			Check(InputCompletedTracks.Contains(ETrack::GreatSword) && (bRead || InputCompletedTracks.Contains(ETrack::Brawler)),
				TEXT("input_required_tracks_completed_real_UI_and_gameplay_routes"));
			if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping()) { FSlateApplication::Get().CancelDragDrop(); }
			if (Widget.IsValid()) { Widget->CancelKeySelection(); }
		}
		if (!bRead && State.IsValid() && Tree.IsValid())
		{
			auto* Persistence = World.IsValid() ? World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
			Check(Persistence && Persistence->SaveNow(), TEXT("write_final_skill_tree_state_before_exit"));
		}
		Done = true;
		const TCHAR* Phase = bRead ? TEXT("READ") : TEXT("RUNTIME");
		if (bInputQA)
		{
			Report += FString::Printf(TEXT("RESULT=HYBRID_INPUT_%s_%s failures=%d\n"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
			UE_LOG(LogRogue10m, Display, TEXT("RESULT=HYBRID_INPUT_%s_%s failures=%d"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		}
		Report += FString::Printf(TEXT("RESULT=HYBRID_%s_%s failures=%d\n"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		if (!Output.IsEmpty()) { FFileHelper::SaveStringToFile(Report, *FPaths::Combine(Output, bRead ? TEXT("runtime-read-results.txt") : TEXT("runtime-results.txt"))); }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=HYBRID_%s_%s failures=%d"), Phase, Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Tree;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> Widget;
	TFunction<void()> Next;
	double NextTime = 0;
	int32 Failures = 0;
	bool Done = false;
	bool bRead = false;
	bool bInputQA = false;
	bool bCommonQA = false;
	bool bCommonPresentationQA = false;
	bool bRollOnlyQA = false;
	float CommonBaseWalkSpeed = 0.0f;
	float CommonBaseAcceleration = 0.0f;
	float CommonBaseAirControl = 0.0f;
	float CommonBaseAirBraking = 0.0f;
	ETrack InputTrack = ETrack::None;
	FName InputNode;
	FDelegateHandle EProbeHandle;
	FDelegateHandle RollMotionHandle;
	TWeakObjectPtr<ACameraActor> RollPhotoCamera;
	TWeakObjectPtr<AActor> RollPreviousViewTarget;
	FString RollMotionRows;
	FString RollAuditDirection;
	FKey RollMoveKey;
	FVector RollExpectedDirection = FVector::ZeroVector;
	FVector RollObservedDirection = FVector::ZeroVector;
	double RollPelvisBefore = 0.0;
	double MinRollSwordClearance = TNumericLimits<double>::Max();
	int32 RollMotionFrames = 0;
	float RollMotionMin = 1.0f;
	float RollMotionMax = 0.0f;
	bool bRollMotionValid = true;
	bool bRollTuckCaptured = false;
	bool bRollReleaseEntryCaptured = false;
	bool bRollReleaseExitCaptured = false;
	bool bSawDodgeDuringE = false;
	TWeakObjectPtr<URogue10mAttackSkillData> InputSkill;
	TSet<ETrack> InputCompletedTracks;
	TFunction<void()> InputContinuation;
	int32 InputUseBefore = 0;
	float InputStaminaBefore = 0;
	FGuid ReadProfileId;
	FString Output, Report;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestHybridSkillTree"),
	TEXT("Runs real skill-tree UI, damage, identity and isolated persistence checks."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor || Active) { return; }
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (auto* World = Context.World(); World && World->WorldType == EWorldType::Game)
			{ Active = MakeShared<FRun>(); Active->Start(*World); return; }
		}
	}));
}
#endif
