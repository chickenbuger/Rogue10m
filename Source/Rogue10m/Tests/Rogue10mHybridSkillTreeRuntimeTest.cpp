#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/InputKeySelector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Slate/UMGDragDropOp.h"
#include "Rogue10mHudWidgetParts.h"
#include "Rogue10mMainHUDWidget.h"
#include "Rogue10mSkillDragDropOperation.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
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
#include "Rogue10mHybridSkillTreeWidget.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTutorialNPC.h"
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
		IFileManager::Get().MakeDirectory(*Output, true);
		Later(3.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->Done) { return false; }
			if (!Self->World.IsValid()) { Self->Check(false, TEXT("world_lost")); Self->Finish(); return false; }
			if (FPlatformTime::Seconds() >= Self->NextTime && Self->Next) { auto F = MoveTemp(Self->Next); F(); }
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
	void Later(double Seconds, TFunction<void()> F) { NextTime = FPlatformTime::Seconds() + Seconds; Next = MoveTemp(F); }
	bool HitPath(UWidget* Target, FWidgetPath& OutPath, FVector2D& Point, const FVector2D& NormalizedPoint = FVector2D(0.5, 0.5))
	{
		FWidgetPath FocusPath;
		if (!Target || !Target->GetIsEnabled()
			|| !FSlateApplication::Get().GeneratePathToWidgetUnchecked(Target->TakeWidget(), FocusPath))
		{ Check(false, TEXT("enabled_real_widget_path")); return false; }
		Point = Target->GetCachedGeometry().GetAbsolutePositionAtCoordinates(NormalizedPoint);
		OutPath = FSlateApplication::Get().LocateWindowUnderMouse(Point,
			TArray<TSharedRef<SWindow>>{FocusPath.GetWindow()}, false, 0);
		const bool Hit = OutPath.IsValid() && OutPath.ContainsWidget(&Target->TakeWidget().Get());
		if (!Hit) { UE_LOG(LogRogue10m, Warning, TEXT("HYBRID_RUNTIME widget outside hit path: %s point=%s size=%s"),
			*Target->GetName(), *Point.ToString(), *Target->GetCachedGeometry().GetLocalSize().ToString()); }
		Check(Hit, TEXT("real_mouse_hit_reaches_widget"));
		return Hit;
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
	URogue10mQuickSlotWidget* FindHUDSlot(bool bDodge)
	{
		for (TObjectIterator<URogue10mQuickSlotWidget> It; It; ++It)
		{
			const auto View = It->GetQuickSlotView();
			auto* MainHUD = It->GetOwningMainHUD();
			if (It->GetWorld() != World.Get() || It->GetOwningPlayer() != PC.Get()
				|| View.bDodgeSlot != bDodge || (!bDodge && View.InputSlot != ERogue10mAttackInputSlot::Primary)
				|| It->GetCachedGeometry().GetLocalSize().GetMin() <= 1.0
				|| !MainHUD || !MainHUD->IsInViewport() || MainHUD->GetOwningPlayer() != PC.Get()) { continue; }
			// Equipment preview widgets retain geometry even while hidden. Require
			// the actual displayed HUD as a Slate ancestor, not merely a matching view.
			FWidgetPath Path;
			if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(It->TakeWidget(), Path)
				|| !Path.ContainsWidget(&MainHUD->TakeWidget().Get())) { continue; }
			UE_LOG(LogRogue10m, Display, TEXT("HYBRID_RUNTIME actual HUD drop target: %s owner=%s size=%s"),
				*It->GetPathName(), *MainHUD->GetName(), *It->GetCachedGeometry().GetLocalSize().ToString());
			return *It;
		}
		return nullptr;
	}
	void CheckDragPresentationRestored(ESlateVisibility BeforeVisibility, float BeforeOpacity)
	{
		Check(Widget.IsValid() && !Widget->IsSkillDragInProgress() && Widget->GetVisibility() == BeforeVisibility
			&& FMath::IsNearlyEqual(Widget->GetRenderOpacity(), BeforeOpacity) && PC->IsAnyBlockingWindowVisible(),
			TEXT("input_drop_or_cancel_restores_menu_visibility_opacity_and_open_state"));
	}
	void DragNodeToHUD(FName NodeId, bool bDodge, bool bExpectDrag, TFunction<void()> Continuation, bool bCloseMenu = false)
	{
		auto* Entry = Widget.IsValid() ? Widget->FindNodeEntry(NodeId) : nullptr;
		FWidgetPath SourcePath; FVector2D Start;
		if (!HitPath(Entry, SourcePath, Start)) { Later(0.15, MoveTemp(Continuation)); return; }
		auto* Target = bExpectDrag ? FindHUDSlot(bDodge) : nullptr;
		if (bExpectDrag)
		{
			Check(Target != nullptr, TEXT("input_real_HUD_target_exists"));
			if (!Target) { Later(0.15, MoveTemp(Continuation)); return; }
		}
		const auto Before = Player->GetCombatComponent()->CaptureCombat().SkillBindings;
		const auto BeforeHUD = Target ? Target->GetQuickSlotView() : FRogue10mHudQuickSlotView();
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
		Later(0.15, [this, Target = TWeakObjectPtr<URogue10mQuickSlotWidget>(Target), bDodge, bCloseMenu,
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
			if (!bDodge)
			{
				Shot(InputTrack == ETrack::Brawler ? TEXT("input-brawler-drag-active.png")
					: bRead ? TEXT("input-greatsword-read-drag-active.png") : TEXT("input-greatsword-drag-active-720p.png"));
			}
			// Leave a real frame for the requested active-drag screenshot before release.
			Later(0.15, [this, Target, bDodge, Before, BeforeHUD, BeforeVisibility, BeforeOpacity,
				SourcePath, Start, Threshold, Continuation = MoveTemp(Continuation)]() mutable
			{
				FWidgetPath TargetPath; FVector2D End;
				const bool TargetHit = HitPath(Target.Get(), TargetPath, End);
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
				Later(0.15, [this, Target, TargetHit, bDodge, Before, BeforeHUD, BeforeVisibility,
					BeforeOpacity, Continuation = MoveTemp(Continuation)]() mutable
				{
					CheckDragPresentationRestored(BeforeVisibility, BeforeOpacity);
					Check(Widget->GetSelectedNodeId() == InputNode, TEXT("input_drag_end_preserves_selected_technique"));
					if (TargetHit && Target.IsValid())
					{
						if (!bDodge)
						{
							Check(Player->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) == InputSkill.Get()
								&& Target->GetQuickSlotView().SkillData == InputSkill.Get(), TEXT("input_real_HUD_drop_equips_learned_technique"));
						}
						else
						{
							Check(Before.OrderIndependentCompareEqual(Player->GetCombatComponent()->CaptureCombat().SkillBindings)
								&& Target->GetQuickSlotView().SkillData == BeforeHUD.SkillData,
								TEXT("input_E_dodge_drop_rejected_without_binding_change"));
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
		InputKey = Track == ETrack::Brawler ? EKeys::F7 : EKeys::F8;
		const auto* Node = Tree->FindNode(InputNode);
		InputSkill = Node ? Node->Skill.LoadSynchronous() : nullptr;
		Check(InputSkill.IsValid() && Tree->IsNodeUnlocked(InputNode)
			&& Tree->GetActiveTrack() == Track, TEXT("input_fixture_uses_real_learned_current_track_technique"));
		if (!InputSkill.IsValid()) { Finish(); return; }
		if (!PC->IsAnyBlockingWindowVisible()) { PC->ToggleSkillTree(); }
		Click(Track == ETrack::Brawler ? TEXT("HybridTreeFighter") : TEXT("HybridTreeGreatSword"));
		Later(0.35, [this]
		{
			const FName IdentityNode = InputTrack == ETrack::Brawler ? FName(TEXT("I.InternalEnergy.Entry")) : FName(TEXT("I.Mana.Entry"));
			const FName ResonanceNode = InputTrack == ETrack::Brawler ? FName(TEXT("F.Resonance.Release")) : FName(TEXT("G.Resonance.Spread"));
			const auto Diamond = Widget->GetNodeOuterFramePoints(IdentityNode);
			const auto Ring = Widget->GetNodeOuterFramePoints(ResonanceNode);
			Check(Widget->GetNodeFrameShape(IdentityNode) == TEXT("IdentityDiamond") && Diamond.Num() == 5
				&& Diamond[0].Equals(Diamond.Last()) && Widget->GetNodeFrameOutlineCount(IdentityNode) == 1,
				TEXT("input_actual_paint_geometry_has_closed_identity_diamond"));
			Check(Widget->GetNodeFrameShape(ResonanceNode) == TEXT("ResonanceDoubleRing") && Ring.Num() == 49
				&& Ring[0].Equals(Ring.Last()) && Widget->GetNodeFrameOutlineCount(ResonanceNode) == 2,
				TEXT("input_actual_paint_geometry_has_double_resonance_outline"));
			// The visible outer frames extend beyond the old central 44-unit button.
			// Route real clicks at their upper edge; cached centers would miss this regression.
			ClickWidget(Widget->FindNodeEntry(IdentityNode), FVector2D(0.5, 0.06));
			Check(Widget->GetSelectedNodeId() == IdentityNode, TEXT("input_identity_outer_diamond_pointer_click_selects_node"));
			ClickWidget(Widget->FindNodeEntry(ResonanceNode), FVector2D(0.5, 0.06));
			Check(Widget->GetSelectedNodeId() == ResonanceNode, TEXT("input_resonance_outer_ring_pointer_click_selects_node"));
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
		});
	}
	void InputKeys()
	{
		auto* Keys = PC->GetSkillBindingComponent();
		Check(Widget->GetSelectedNodeId() == InputNode, TEXT("input_drag_selects_technique_for_key_controls"));
		SelectKeyByInput(InputKey);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_selector_actual_key_persists_binding"));
		SelectKeyByInput(EKeys::Escape);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == InputKey && PC->IsAnyBlockingWindowVisible(),
			TEXT("input_Esc_cancels_selection_preserves_key_and_menu"));
		Check(Rogue10mInput::DodgeKey() == EKeys::LeftAlt, TEXT("input_contract_independently_requires_LeftAlt_dodge"));
		SelectKeyByInput(EKeys::W);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_reserved_movement_key_rejected_preserves_binding"));
		FText DodgeReason;
		Check(Keys && !Keys->SetSkillKey(InputSkill.Get(), Rogue10mInput::DodgeKey(), DodgeReason)
			&& Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_LeftAlt_dodge_cannot_be_assigned_as_active_skill"));
		SelectKeyByInput(EKeys::E);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == EKeys::E, TEXT("input_freed_E_selected_through_real_key_selector"));
		Click(TEXT("HybridTreeClearKey"));
		Check(Keys && !Keys->GetSkillKey(InputSkill.Get()).IsValid(), TEXT("input_real_clear_button_removes_binding"));
		SelectKeyByInput(InputKey);
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_reassign_after_clear_through_actual_key_event"));
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
		Check(Keys && Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_saved_F_key_fixture_is_retained_before_temporary_E_probe"));
		Check(Keys && Keys->SetSkillKey(InputSkill.Get(), EKeys::E, Reason), TEXT("input_freed_E_binding_accepted_for_actual_cast_no_fallback"));
		if (Failures) { Finish(); return; }
		GameKey(EKeys::E, false);
		// Mapping rebuild ignores pressed keys until released; give the real input stack two ticks.
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
				Check(Keys && Keys->SetSkillKey(InputSkill.Get(), InputKey, Reason)
					&& Keys->GetSkillKey(InputSkill.Get()) == InputKey, TEXT("input_original_F7_F8_QA_binding_restored_for_existing_disk_roundtrip"));
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
		Player = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Tree = State.IsValid() ? State->GetSkillTreeProgressionComponent() : nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Tree.IsValid() && NPC.IsValid() && Player.IsValid(), TEXT("player_tree_and_town_npc_exist"));
		if (Failures) { Finish(); return; }
		Check(Player->GetCombatComponent()->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) != nullptr,
			TEXT("new_profile_keeps_playable_primary_binding"));
		// Keep the real, authored Knuckle starter inventory/character/combat domains aligned.
		const auto* StarterItem = Player->GetInventoryComponent()->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		Check(StarterItem && StarterItem->WeaponType == ERogue10mWeaponType::Knuckle
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::Knuckle
			&& Tree->GetActiveTrack() == ETrack::Brawler, TEXT("fighter_fixture_retains_real_knuckle_starter_across_all_domains"));
		Check(Tree->GetMasteryLevel(ETrack::Brawler) == 1 && Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5,
			TEXT("initial_M1_five_basic_skills"));
		ExerciseTemporaryBindings();
		Player->SetCanBeDamaged(false);
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 5; Growth.Experience = 0;
		Check(State->RestoreGrowth(Growth) && State->GetPlayerLevel() == 5
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
		const int32 BeforeEntryPoints = Tree->GetAvailableGrowthPoints();
		const bool bEntryLearned = Tree->TryUnlockNode(TEXT("I.InternalEnergy.Entry"), ERoute::Points, NAME_None);
		const auto EntryState = Tree->Capture();
		const auto* Entry = EntryState.Investments.FindByPredicate([](const auto& Row) { return Row.NodeId == TEXT("I.InternalEnergy.Entry"); });
		Check(bEntryLearned && BeforeEntryPoints == 4 && Tree->GetAvailableGrowthPoints() == 3
			&& Tree->GetAvailableGrowthPoints() == BeforeEntryPoints - 1 && Tree->GetSpentSkillTreePoints() == 1
			&& Entry && Entry->Route == ERoute::Points && Entry->SpentPoints == 1 && Entry->ChoiceId.IsNone(),
			TEXT("inner_entry_debits_exactly_one_run_point_four_to_three"));
		Check(!Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
			&& Tree->GetAvailableGrowthPoints() == 3 && Tree->GetSpentSkillTreePoints() == 1, TEXT("brawler_mana_purchase_rejected_without_spending"));
		if (Failures) { Finish(); return; }
		const int32 BeforeAdvancedPoints = Tree->GetAvailableGrowthPoints();
		const bool bAdvancedLearned = Tree->TryUnlockNode(TEXT("I.InternalEnergy.Advanced"), ERoute::Points, NAME_None);
		const auto AdvancedState = Tree->Capture();
		const auto* Advanced = AdvancedState.Investments.FindByPredicate([](const auto& Row) { return Row.NodeId == TEXT("I.InternalEnergy.Advanced"); });
		Check(bAdvancedLearned && BeforeAdvancedPoints == 3 && Tree->GetAvailableGrowthPoints() == 1
			&& Tree->GetAvailableGrowthPoints() == BeforeAdvancedPoints - 2 && Tree->GetSpentSkillTreePoints() == 3
			&& Advanced && Advanced->Route == ERoute::Points && Advanced->SpentPoints == 2 && Tree->IsIdentityDeepened(EIdentity::InternalEnergy),
			TEXT("paid_inner_deepening_debits_exactly_two_run_points_three_to_one"));
		if (Failures) { Finish(); return; }
		const int32 BeforeRefundPoints = Tree->GetAvailableGrowthPoints();
		const int32 BeforeRefundMastery = Tree->GetMasteryTotalExperience(ETrack::Brawler);
		const bool bAdvancedRefunded = Tree->RefundNode(TEXT("I.InternalEnergy.Advanced"));
		const auto RefundedState = Tree->Capture();
		const auto* KeptEntry = RefundedState.Investments.FindByPredicate([](const auto& Row) { return Row.NodeId == TEXT("I.InternalEnergy.Entry"); });
		Check(bAdvancedRefunded && BeforeRefundPoints == 1 && Tree->GetAvailableGrowthPoints() == 3
			&& Tree->GetAvailableGrowthPoints() == BeforeRefundPoints + 2 && Tree->GetSpentSkillTreePoints() == 1
			&& KeptEntry && KeptEntry->Route == ERoute::Points && KeptEntry->SpentPoints == 1 && KeptEntry->ChoiceId.IsNone()
			&& Tree->IsIdentityLearned(EIdentity::InternalEnergy) && !Tree->IsIdentityDeepened(EIdentity::InternalEnergy)
			&& !Tree->IsNodeUnlocked(TEXT("I.InternalEnergy.Advanced")) && Tree->GetMasteryTotalExperience(ETrack::Brawler) == BeforeRefundMastery,
			TEXT("paid_inner_deepening_refund_returns_exact_two_points_one_to_three_keeps_entry_and_mastery"));
		const auto TownAfterRunInvestments = Town->CaptureProgression();
		Check(FRogue10mTownProgressionSnapshot::StaticStruct()->CompareScriptStruct(&TownBeforeRejectedPurchase, &TownAfterRunInvestments, 0)
			&& Town->GetLobbyEarnedBudget() == 0 && Town->GetAvailableTraitPoints() == 0 && Town->GetSpentTraitPoints() == 0
			&& Persistence->GetLobbyAvailablePoints() == 0, TEXT("run_learning_and_refund_do_not_touch_zero_lobby_budget_or_town_ledger"));
		if (Failures) { Finish(); return; }
		PC->ToggleSkillTree();
		for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
		{ if (It->GetWorld() == World && It->GetOwningPlayer() == PC) { Widget = *It; break; } }
		Check(Widget.IsValid() && Widget->GetVisibleNodeCount() > 25 && PC->IsAnyBlockingWindowVisible(),
			TEXT("real_K_window_native_radial_tree"));
		if (Failures) { Finish(); return; }
		Widget->HandleAction(100, TEXT("I.InternalEnergy.Advanced"));
		Later(0.7, [this]
		{
			const int32 BeforeFreeDeepening = Tree->GetAvailableGrowthPoints();
			Click(TEXT("HybridTreeLearnMastery"));
			const auto FreeState = Tree->Capture();
			const auto* FreeAdvanced = FreeState.Investments.FindByPredicate([](const auto& Row) { return Row.NodeId == TEXT("I.InternalEnergy.Advanced"); });
			Check(BeforeFreeDeepening == 3 && Tree->IsIdentityDeepened(EIdentity::InternalEnergy)
				&& Tree->GetSpentSkillTreePoints() == 1 && Tree->GetAvailableGrowthPoints() == BeforeFreeDeepening
				&& FreeAdvanced && FreeAdvanced->Route == ERoute::Mastery && FreeAdvanced->SpentPoints == 0,
				TEXT("real_UI_free_mastery_deepening_keeps_exact_three_run_points"));
			Widget->HandleAction(100, TEXT("I.InternalEnergy.Choice"));
			Widget->HandleAction(6, NAME_None, TEXT("InternalEnergy.Release"));
			Later(0.35, [this]
			{
				const int32 BeforeChoicePoints = Tree->GetAvailableGrowthPoints();
				Click(TEXT("HybridTreeLearnPoints"));
				Check(BeforeChoicePoints == 3 && Tree->GetSelectedProperty(EIdentity::InternalEnergy) == TEXT("InternalEnergy.Release")
					&& Tree->GetAvailableGrowthPoints() == BeforeChoicePoints - 2 && Tree->GetAvailableGrowthPoints() == 1
					&& Tree->GetSpentSkillTreePoints() == 3 && State->GetTownProgressionComponent()->GetAvailableTraitPoints() == 0,
					TEXT("UI_property_debits_exact_two_run_points_three_to_one_lobby_stays_zero"));
				if (bInputQA)
				{
					Shot(TEXT("brawler-1080p.png"));
					BeginInputChecks(ETrack::Brawler, [this] { PC->ToggleSkillTree(); Later(0.35, [this] { GreatSword(); }); });
					return;
				}
				Widget->HandleAction(100, TEXT("F.DiamondStrike"));
				Check(Widget->SetSelectedSkillKey(EKeys::F7), TEXT("learned_technique_key_assignment"));
				Widget->HandleAction(100, TEXT("F.Ultimate"));
				Check(!Widget->SetSelectedSkillKey(EKeys::F8), TEXT("locked_ultimate_key_rejected"));
				Shot(TEXT("brawler-1080p.png"));
				Later(0.6, [this] { GreatSword(); });
			});
		});
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
		Check(State->GetPlayerLevel() == 15 && State->GetPlayerLevel() == Saved->Growth.Level
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::GreatSword, TEXT("read_growth_LV15_and_greatsword_combat_restored"));
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
		Check(Tree->GetSpentSkillTreePoints() == 8 && Tree->GetAvailableGrowthPoints() == 6
			&& State->GetPlayerLevel() - 1 == Tree->GetSpentSkillTreePoints() + Tree->GetAvailableGrowthPoints(),
			TEXT("read_LV15_run_budget_fourteen_equals_exact_eight_spent_plus_six_available"));
		Check(Tree->GetActiveIdentity() == EIdentity::Mana && Tree->GetSelectedProperty(EIdentity::Mana) == TEXT("Mana.Spread")
			&& Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana), TEXT("read_active_mana_spread_and_ultimate_resonance_restored"));
		auto* Combat = Player->GetCombatComponent();
		Check(Combat && Combat->GetActiveSkillTreeSkills().Num() == 10 && Combat->GetUnlockedWeaponSkills().Num() == 10
			&& Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary), TEXT("read_ten_greatsword_attacks_and_playable_primary_restored"));
		const auto* DiamondNode = Tree->FindNode(TEXT("F.DiamondStrike"));
		auto* Diamond = DiamondNode ? DiamondNode->Skill.LoadSynchronous() : nullptr;
		auto* Keys = PC->GetSkillBindingComponent();
		Check(Keys && Diamond && Keys->GetSkillKey(Diamond) == EKeys::F7, TEXT("read_F_DiamondStrike_F7_key_restored_from_isolated_key_save"));
		const auto* IdentityCombat = Player->GetSkillTreeCombatComponent();
		Check(IdentityCombat && IdentityCombat->GetGauge() == 0.0f && IdentityCombat->GetCooldownRemaining() == 0.0f,
			TEXT("read_transient_identity_gauge_and_cooldown_start_empty"));
		if (bInputQA)
		{
			const auto* Node = Tree->FindNode(TEXT("G.CrushingSlash"));
			auto* Crushing = Node ? Node->Skill.LoadSynchronous() : nullptr;
			Check(Keys && Crushing && Keys->GetSkillKey(Crushing) == EKeys::F8
				&& Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) == Crushing,
				TEXT("read_real_HUD_drop_and_F8_key_restored_from_disk"));
			PC->ToggleSkillTree();
			for (TObjectIterator<URogue10mHybridSkillTreeWidget> It; It; ++It)
			{ if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get()) { Widget = *It; break; } }
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
		auto* Ultimate = Tree->FindNode(TEXT("F.Ultimate"))->Skill.LoadSynchronous();
		Combat->UnlockAllActiveTreeForTesting();
		Check(Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Primary),
			TEXT("temporary_test_skill_can_be_previewed"));
		const auto Preview = Combat->CaptureCombat();
		Check(Preview.SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary
			&& !Preview.UnlockedSkills.Contains(TSoftObjectPtr<URogue10mAttackSkillData>(Ultimate))
			&& Tree->GetMasteryTotalExperience(ETrack::Brawler) == 0,
			TEXT("test_preview_preserves_normal_primary_without_unlock_or_XP_pollution"));
		Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Special);
		Check(Combat->CaptureCombat().SkillBindings.FindRef(ERogue10mAttackInputSlot::Primary).Get() == Primary,
			TEXT("moving_test_preview_preserves_normal_primary"));
		Combat->UnassignSkillFromInputSlot(ERogue10mAttackInputSlot::Primary);
		Combat->AssignSkillToInputSlot(Ultimate, ERogue10mAttackInputSlot::Primary);
		Check(!Combat->CaptureCombat().SkillBindings.Contains(ERogue10mAttackInputSlot::Primary),
			TEXT("explicitly_empty_slot_remains_empty_under_test_preview"));
		Check(Combat->RestoreCombat(Baseline) && Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary) == Primary
			&& !Combat->IsAttackSkillUnlocked(Ultimate), TEXT("restore_clears_test_unlocks_and_restores_normal_loadout"));
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
	void GreatSword()
	{
		Widget->HandleAction(2);
		const auto* BrowsingMainItem = Player->GetInventoryComponent()->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		Check(BrowsingMainItem && BrowsingMainItem->WeaponType == ERogue10mWeaponType::Knuckle
			&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::Knuckle
			&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::Knuckle,
			TEXT("browsing_greatsword_keeps_real_knuckle_inventory_character_and_combat"));
		Shot(TEXT("greatsword-locked-1080p.png"));
		Later(0.6, [this]
		{
			PC->CloseAllBlockingPanels();
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
				Check(Inventory->TryEquipGridItem(ContainerIndex, SwordInstance), TEXT("actual_inventory_equip_commits_greatsword_fixture"));
			}
			Check(Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon) == Sword
				&& Player->GetEquippedWeaponType() == ERogue10mWeaponType::GreatSword
				&& Player->GetCombatComponent()->GetActiveSkillTreeWeaponType() == ERogue10mWeaponType::GreatSword
				&& Tree->GetActiveTrack() == ETrack::GreatSword, TEXT("greatsword_inventory_character_combat_and_mastery_track_are_consistent"));
			if (Failures) { Finish(); return; }
			Check(Player->GetCombatComponent()->GetActiveSkillTreeSkills().Num() == 10
				&& Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 5, TEXT("greatsword_five_basics_five_locked_techniques"));
			Check(Tree->GetMasteryLevel(ETrack::GreatSword) == 1 && Tree->GetMasteryLevel(ETrack::Brawler) == 12,
				TEXT("separate_mastery_tracks"));
			Check(Tree->GetActiveIdentity() == EIdentity::None, TEXT("inner_energy_inactive_with_greatsword"));
			FRogue10mGrowthSnapshot Growth = State->CaptureGrowth(); Growth.Level = 15;
			State->RestoreGrowth(Growth);
			auto Snapshot = Tree->Capture(); FRogue10mSkillTreeMasteryRecord Row; Row.Track = ETrack::GreatSword; Row.TotalExperience = 1045;
			Snapshot.Masteries.Add(Row);
			Check(Tree->Restore(Snapshot), TEXT("M20_greatsword_fixture_restore_and_auto_rewards"));
			Check(Player->GetCombatComponent()->GetUnlockedWeaponSkills().Num() == 10, TEXT("all_five_greatsword_core_attacks_granted"));
			Check(Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread"))
				&& Tree->TrySetActiveIdentity(EIdentity::Mana), TEXT("greatsword_mana_hybrid_investment_and_activation"));
			Check(Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana), TEXT("core_and_property_both_required_for_resonance"));
			const int32 BeforeRefund = Tree->GetSpentSkillTreePoints();
			Check(Tree->RefundNode(TEXT("I.Mana.Advanced")) && Tree->IsIdentityLearned(EIdentity::Mana)
				&& !Tree->IsIdentityDeepened(EIdentity::Mana) && Tree->GetSelectedProperty(EIdentity::Mana).IsNone()
				&& !Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana)
				&& Tree->GetSpentSkillTreePoints() == BeforeRefund - 4, TEXT("advanced_refund_keeps_entry_removes_choice_and_resonance_returns_actual_cost"));
			Check(!Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Mastery, NAME_None),
				TEXT("mana_deepening_remains_points_only_at_M20"));
			Check(Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
				&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread")),
				TEXT("refunded_mana_deepening_and_choice_can_be_repurchased"));
			IdentityAndSave();
			PC->ToggleSkillTree(); Widget->HandleAction(2); Widget->HandleAction(100, TEXT("G.Ultimate"));
			PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false);
			Later(0.7, [this]
			{
				Shot(TEXT("greatsword-720p.png"));
				Later(0.6, [this]
				{
					if (bInputQA) { BeginInputChecks(ETrack::GreatSword, [this] { Finish(); }); }
					else { Finish(); }
				});
			});
		});
	}
	void IdentityAndSave()
	{
		auto* CombatIdentity = Player->GetSkillTreeCombatComponent();
		auto* Ultimate = Tree->FindNode(TEXT("G.Ultimate"))->Skill.LoadSynchronous();
		for (int32 I = 0; I < 4; ++I) { CombatIdentity->NotifyConfirmedAttack(ETrack::GreatSword); }
		State->GetRogueAttributeSet()->SetMana(0);
		Check(!CombatIdentity->AcceptAttack(*Ultimate, false).bTriggered && CombatIdentity->GetGauge() == 100,
			TEXT("mana_proc_requires_MP_preserves_gauge_when_short"));
		State->GetRogueAttributeSet()->SetMana(100);
		auto Effect = CombatIdentity->AcceptAttack(*Ultimate, false);
		Check(Effect.bTriggered && Effect.DamageMultiplier > 1.1f && Effect.RangeMultiplier > 1
			&& State->GetRogueAttributeSet()->GetMana() == 95, TEXT("mana_core_resonance_consumes_MP_and_enlarges_actual_attack"));
		const float Cooldown = CombatIdentity->GetCooldownRemaining();
		Check(Tree->TrySetActiveIdentity(EIdentity::None) && Tree->TrySetActiveIdentity(EIdentity::Mana)
			&& CombatIdentity->GetGauge() == 0 && CombatIdentity->GetCooldownRemaining() >= Cooldown - .1f,
			TEXT("identity_switch_cannot_refill_or_reset_cooldown"));
		auto* Persistence = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		Check(Persistence->SaveNow(), TEXT("actual_isolated_profile_save"));
		auto* Saved = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(
			Persistence->GetSaveSlotForProfile(Persistence->GetAttachedProfileId()), 0));
		Check(Saved && URogue10mProgressionSaveGame::CurrentVersion == 5 && Saved->SaveVersion == 5
			&& Saved->RunId.IsValid() && Saved->RunId == Persistence->GetAttachedRunId()
			&& Saved->LobbyEarnedPoints == 0 && Saved->Town.TraitInvestments.IsEmpty()
			&& Saved->SkillTree.Masteries.Num() == 2 && Saved->SkillTree.ActiveIdentity == EIdentity::Mana,
			TEXT("disk_current_v5_roundtrip_preserves_run_mastery_identity_and_zero_lobby_ledger"));
		const float Health = State->GetRogueAttributeSet()->GetHealth();
		Check(Tree->Respec(EIdentity::Mana) && Tree->GetMasteryLevel(ETrack::GreatSword) == 20
			&& !Tree->IsIdentityLearned(EIdentity::Mana) && !Tree->HasResonance(ETrack::GreatSword, EIdentity::Mana)
			&& State->GetRogueAttributeSet()->GetHealth() <= Health, TEXT("respec_removes_descendants_preserves_mastery_no_healing"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(1500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Check(!Tree->CanChangeInvestments() && !Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None),
			TEXT("out_of_NPC_range_rejects_purchase"));
		Player->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Check(Tree->TryUnlockNode(TEXT("I.Mana.Entry"), ERoute::Points, NAME_None)
			&& Tree->TryUnlockNode(TEXT("I.Mana.Advanced"), ERoute::Points, NAME_None)
			&& Tree->TryUnlockNode(TEXT("I.Mana.Choice"), ERoute::Points, TEXT("Mana.Spread")),
			TEXT("mana_can_be_rebuilt_after_refund_for_final_UI"));
	}
	void Finish()
	{
		if (Done) { return; }
		FWorldDelegates::OnWorldPostActorTick.Remove(EProbeHandle);
		if (PC.IsValid()) { GameKey(EKeys::E, false); }
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
			Check(Persistence && Persistence->SaveNow(), TEXT("write_final_rebuilt_identity_state_saved_before_exit"));
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
	ETrack InputTrack = ETrack::None;
	FName InputNode;
	FDelegateHandle EProbeHandle;
	bool bSawDodgeDuringE = false;
	FKey InputKey;
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
