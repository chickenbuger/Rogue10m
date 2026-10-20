// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitCatalogDataAsset.h"
#include "Rogue10mTraitWidget.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "Rendering/SlateRenderer.h"
#include "UnrealClient.h"

// Executable-only integration check. Both isolated save overrides are required
// before this fixture creates a character or purchases/refunds a trait.
namespace Rogue10mTraitUIQA
{
static bool SafeOverride(const FString& Value, const TCHAR* Prefix)
{
	if (!Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
	for (TCHAR Character : Value) { if (!FChar::IsAlnum(Character) && Character != TEXT('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString ProfileSlot, SavePrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), SavePrefix);
		if (!SafeOverride(ProfileSlot, TEXT("Rogue10m_Town_QA_"))
			|| !SafeOverride(SavePrefix, TEXT("Rogue10m_Progression_QA_")))
		{
			Check(false, TEXT("isolated_save_flags_required")); Finish(); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/trait16-runtime-20261011"), ProfileSlot);
		ExpectedSavePrefix = SavePrefix;
		IFileManager::Get().MakeDirectory(*Output, true);
		Later(4.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (!Self->World.IsValid()) { Self->Check(false, TEXT("world_lost")); Self->Finish(); return false; }
			if (FPlatformTime::Seconds() >= Self->NextTime && Self->Next)
			{
				auto Callback = MoveTemp(Self->Next); Callback();
			}
			return !Self->bFinished;
		}), 0.03f);
	}
private:
	void Check(bool Condition, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), Reason);
		UE_LOG(LogRogue10m, Display, TEXT("TRAIT_UI_CHECK %s %s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!Condition) { ++Failures; }
	}
	void Later(double Delay, TFunction<void()> Callback) { NextTime = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Callback); }
	void Key(FKey InputKey, bool Repeat=false)
	{
		FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(InputKey, FModifierKeysState(), 0, Repeat, 0, 0));
		FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0));
	}
	bool MouseClick(const TCHAR* Name)
	{
		UButton* Button = Widget.IsValid() ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
		FWidgetPath FocusPath;
		if (!Button || !Button->GetIsEnabled()
			|| !FSlateApplication::Get().GeneratePathToWidgetUnchecked(Button->TakeWidget(), FocusPath))
		{
			Check(false, TEXT("real_button_path_missing")); return false;
		}
		const FVector2D Point = Button->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5,0.5));
		FWidgetPath PointerPath = FSlateApplication::Get().LocateWindowUnderMouse(Point,
			TArray<TSharedRef<SWindow>>{FocusPath.GetWindow()}, false, 0);
		const bool bHit = PointerPath.IsValid() && PointerPath.ContainsWidget(&Button->TakeWidget().Get());
		Check(bHit, TEXT("mouse_hit_test_reaches_real_trait_button"));
		if (!bHit) { return false; }
		FSlateApplication::Get().RoutePointerMoveEvent(PointerPath,
			FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::Invalid,0,FModifierKeysState()),false);
		FSlateApplication::Get().RoutePointerDownEvent(PointerPath,
			FPointerEvent(0,Point,Point,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
		FSlateApplication::Get().RoutePointerUpEvent(PointerPath,
			FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState()));
		return true;
	}
	void Prepare()
	{
		PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		URogue10mCharacterProfileSubsystem* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("TraitUiQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("test_character_created_and_selected"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Player = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Town = State.IsValid() ? State->GetTownProgressionComponent() : nullptr;
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Player.IsValid() && State.IsValid() && Town.IsValid() && NPC.IsValid(), TEXT("real_player_state_town_and_NPC"));
		if (Failures) { Finish(); return; }
		FRogue10mGrowthSnapshot Growth = State->CaptureGrowth();
		Growth.Level = 10; Growth.Experience = 0;
		Check(State->RestoreGrowth(Growth), TEXT("level10_fixture_has_nine_trait_points"));
		Player->SetActorLocation(NPC->GetActorLocation()+FVector(100,0,100),false,nullptr,ETeleportType::TeleportPhysics);
		Check(NPC->BeginDialogue(Player.Get()), TEXT("actual_NPC_dialogue_opens"));
		const FString Services = NPC->GetTutorialWidget() ? NPC->GetTutorialWidget()->GetDisplayedServices().ToString() : FString();
		Check(Services.Contains(TEXT("특성 강화 창")) && !Services.Contains(TEXT("활력 ")) && !Services.Contains(TEXT("힘 ")),
			TEXT("one_traits_entry_replaces_two_legacy_service_rows"));
		Check(NPC->OpenTraitWindow(), TEXT("NPC_opens_standalone_trait_window"));
		Widget = PC->GetTraitWidget();
		Check(Widget.IsValid() && Widget->GetVisibleTraitCount()==16 && PC->IsAnyBlockingWindowVisible()
			&& PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor,
			TEXT("sixteen_cards_and_gameplay_input_blocking"));
		if (Failures) { Finish(); return; }
		URogue10mBasicBrawlerComponent* Basic = Player->GetBasicBrawlerComponent();
		const int32 AttackSerial = Basic ? Basic->GetSnapshot().AcceptedSerial : 0;
		const float Stamina = State->GetRogueAttributeSet()->GetStamina();
		for (const TCHAR* FunctionName : {TEXT("DoPrimaryAttackPressed"),TEXT("DoPrimaryAttackReleased"),TEXT("DoDodge")})
		{
			UFunction* Function = Player->FindFunction(FunctionName);
			Check(Function != nullptr, TEXT("reflected_gameplay_input_function_exists"));
			if (Function) { Player->ProcessEvent(Function,nullptr); }
		}
		struct FMoveParameters { float Right; float Forward; } MoveParameters{1.0f,1.0f};
		UFunction* MoveFunction = Player->FindFunction(TEXT("DoMove"));
		Check(MoveFunction != nullptr,TEXT("reflected_movement_function_exists"));
		if (MoveFunction) { Player->ProcessEvent(MoveFunction,&MoveParameters); }
		Check(Basic && Basic->GetSnapshot().AcceptedSerial == AttackSerial
			&& FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetStamina(),Stamina)
			&& Player->GetPendingMovementInputVector().IsNearlyZero(), TEXT("real_attack_dodge_and_movement_are_blocked"));
		PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"),false);
		Later(1.0,[this]
		{
			CheckRenderedCards(TEXT("traits-1080p.png"),[this] { Inputs(); });
		});
	}
	void CheckRenderedCards(const FString& Screenshot, TFunction<void()> Continuation)
	{
		UScrollBox* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("TraitListScroll")));
		Check(Scroll != nullptr,TEXT("trait_scroll_viewport_exists_for_rendered_card_checks"));
		if (!Scroll) { Finish(); return; }
		RenderedTraitIds.Reset();
		Scroll->ScrollToStart();
		Later(0.35,[this,Screenshot,Continuation=MoveTemp(Continuation)]() mutable
		{
			Geometry(*Screenshot);
			CheckCardsInsideViewport();
			CastChecked<UScrollBox>(Widget->GetWidgetFromName(TEXT("TraitListScroll")))->ScrollToEnd();
			Later(0.35,[this,Screenshot,Continuation=MoveTemp(Continuation)]() mutable
			{
				CheckCardsInsideViewport();
				Check(RenderedTraitIds.Num()==16,TEXT("all_sixteen_cards_checked_after_actual_visible_render"));
				const FString BottomScreenshot=Screenshot.Replace(TEXT("traits-"),TEXT("traits-bottom-"));
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,BottomScreenshot),true,false);
				// Give the bottom screenshot a rendered frame before restoring the list.
				Later(0.35,[this,Continuation=MoveTemp(Continuation)]() mutable
				{
					CastChecked<UScrollBox>(Widget->GetWidgetFromName(TEXT("TraitListScroll")))->ScrollToStart();
					Later(0.35,MoveTemp(Continuation));
				});
			});
		});
	}
	void Geometry(const TCHAR* Screenshot)
	{
		const FGeometry& Root = Widget->GetCachedGeometry();
		for (const TCHAR* Name : {TEXT("TraitDesignSize"),TEXT("TraitCloseButton"),TEXT("TraitListScroll"),TEXT("TraitResetButton"),TEXT("TraitUpgradeButton")})
		{
			const UWidget* Child = Widget->GetWidgetFromName(Name);
			if (!Child) { Check(false, TEXT("layout_widget_missing")); continue; }
			const FGeometry& ChildGeometry = Child->GetCachedGeometry();
			const FVector2D A = Root.AbsoluteToLocal(ChildGeometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D B = Root.AbsoluteToLocal(ChildGeometry.LocalToAbsolute(ChildGeometry.GetLocalSize()));
			Check(ChildGeometry.GetLocalSize().X>0 && ChildGeometry.GetLocalSize().Y>0 && A.X>=-1 && A.Y>=-1
				&& B.X<=Root.GetLocalSize().X+1 && B.Y<=Root.GetLocalSize().Y+1, TEXT("trait_controls_inside_actual_viewport"));
		}
		Check(LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Traits/T_TraitCatalogAtlas.T_TraitCatalogAtlas")) != nullptr,
			TEXT("imported_trait_icon_atlas_resolves"));
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,Screenshot),true,false);
	}
	void CheckCardsInsideViewport()
	{
		const FGeometry& ScrollGeometry=Widget->GetWidgetFromName(TEXT("TraitListScroll"))->GetCachedGeometry();
		int32 PaintedCards=0;
		for (const FRogue10mTraitCardView& Card : Widget->GetVisibleTraitCards())
		{
			const FGeometry& CardGeometry = Card.Frame->GetCachedGeometry();
			const FVector2D CardA=ScrollGeometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D CardB=ScrollGeometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize()));
			// Slate culls offscreen descendants. Check them only after scrolling the
			// entire card into view and waiting for paint; require all 16 IDs below.
			if (CardA.X < -1 || CardA.Y < -1 || CardB.X > ScrollGeometry.GetLocalSize().X+1
				|| CardB.Y > ScrollGeometry.GetLocalSize().Y+1) { continue; }
			++PaintedCards;
			RenderedTraitIds.Add(Card.Trait);
			const UTextBlock* Fields[] = {Card.Name.Get(),Card.Rank.Get(),Card.Effect.Get(),Card.State.Get()};
			const TCHAR* FieldNames[] = {TEXT("Name"),TEXT("Rank"),TEXT("Effect"),TEXT("State")};
			for (int32 FieldIndex=0;FieldIndex<UE_ARRAY_COUNT(Fields);++FieldIndex)
			{
				const UTextBlock* Text = Fields[FieldIndex];
				if (Text->GetText().IsEmpty()) { continue; }
				const FGeometry& TextGeometry = Text->GetCachedGeometry();
				const FVector2D A = CardGeometry.AbsoluteToLocal(TextGeometry.LocalToAbsolute(FVector2D::ZeroVector));
				const FVector2D B = CardGeometry.AbsoluteToLocal(TextGeometry.LocalToAbsolute(TextGeometry.GetLocalSize()));
				Check(CardGeometry.GetLocalSize().X>0 && CardGeometry.GetLocalSize().Y>0 && A.X>=-1 && A.Y>=-1
					&& B.X<=CardGeometry.GetLocalSize().X+1 && B.Y<=CardGeometry.GetLocalSize().Y+1,
					*FString::Printf(TEXT("trait_%d_%s_text_inside_card"),static_cast<int32>(Card.Trait),FieldNames[FieldIndex]));
			}
			const FVector2D KoreanGlyphSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(
				FText::FromString(TEXT("가")),Card.Effect->GetFont());
			Check(Card.Effect->GetCachedGeometry().GetLocalSize().Y <= KoreanGlyphSize.Y * 2.0 + 2.0,
				*FString::Printf(TEXT("trait_%d_effect_uses_at_most_two_lines"),static_cast<int32>(Card.Trait)));
		}
		Check(PaintedCards>=4,TEXT("scrolled_position_has_fully_rendered_cards"));
	}
	void SelectedCardVisible()
	{
		const UWidget* Scroll = Widget->GetWidgetFromName(TEXT("TraitListScroll"));
		Check(Scroll != nullptr,TEXT("trait_scroll_viewport_exists"));
		if (!Scroll) { return; }
		const FGeometry& ScrollGeometry = Scroll->GetCachedGeometry();
		for (const FRogue10mTraitCardView& Card : Widget->GetVisibleTraitCards())
		{
			if (Card.Trait != Widget->GetSelectedTrait()) { continue; }
			const FGeometry& CardGeometry = Card.Frame->GetCachedGeometry();
			const FVector2D A = ScrollGeometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D B = ScrollGeometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize()));
			Check(A.X>=-1 && A.Y>=-1 && B.X<=ScrollGeometry.GetLocalSize().X+1 && B.Y<=ScrollGeometry.GetLocalSize().Y+1,
				TEXT("keyboard_selected_bottom_card_is_inside_scroll_viewport"));
			return;
		}
		Check(false,TEXT("keyboard_selected_card_exists"));
	}
	void ModalGeometry()
	{
		const FGeometry& Root = Widget->GetCachedGeometry();
		const UWidget* Backdrop = Widget->GetWidgetFromName(TEXT("TraitResetBackdrop"));
		const UWidget* Dialog = Widget->GetWidgetFromName(TEXT("TraitResetDialogFrame"));
		Check(Backdrop && Dialog,TEXT("refund_backdrop_and_dialog_exist"));
		if (!Backdrop || !Dialog) { return; }
		const FGeometry& BackdropGeometry = Backdrop->GetCachedGeometry();
		const FVector2D DimA = Root.AbsoluteToLocal(BackdropGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D DimB = Root.AbsoluteToLocal(BackdropGeometry.LocalToAbsolute(BackdropGeometry.GetLocalSize()));
		Check(DimA.IsNearlyZero(1.0) && DimB.Equals(Root.GetLocalSize(),1.0),TEXT("refund_dim_covers_entire_viewport"));
		const FGeometry& DialogGeometry = Dialog->GetCachedGeometry();
		const FVector2D A = Root.AbsoluteToLocal(DialogGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D B = Root.AbsoluteToLocal(DialogGeometry.LocalToAbsolute(DialogGeometry.GetLocalSize()));
		Check(((A+B)*0.5).Equals(Root.GetLocalSize()*0.5,1.0) && A.X>=0 && A.Y>=0
			&& B.X<=Root.GetLocalSize().X+1 && B.Y<=Root.GetLocalSize().Y+1,TEXT("refund_dialog_is_centered_and_inside_viewport"));
	}
	void Inputs()
	{
		MouseClick(TEXT("TraitFilter3"));
		Check(Widget->GetVisibleTraitCount()==4, TEXT("actual_mouse_filter_shows_four_mobility_traits"));
		Key(EKeys::One);
		Widget->SelectTrait(ERogue10mTownTrait::CriticalChance);
		Key(EKeys::Down);
		Check(Widget->GetSelectedTrait()==ERogue10mTownTrait::CriticalDamage, TEXT("Slate_down_selects_next_card"));
		Key(EKeys::Up);
		const float Before = State->GetRogueAttributeSet()->GetCriticalChance();
		Key(EKeys::Enter);
		Check(Town->GetTraitLevel(ERogue10mTownTrait::CriticalChance)==1 && Widget->GetDisplayedAvailablePoints()==8
			&& FMath::IsNearlyEqual(State->GetRogueAttributeSet()->GetCriticalChance(),Before+0.02f),
			TEXT("Slate_enter_buys_selected_trait_with_actual_effect_and_cost"));
		Key(EKeys::Enter,true);
		Check(Town->GetTraitLevel(ERogue10mTownTrait::CriticalChance)==1, TEXT("held_enter_does_not_repeat_purchase"));
		Widget->SelectTrait(ERogue10mTownTrait::LastStand);
		Key(EKeys::Enter);
		Check(Town->GetTraitLevel(ERogue10mTownTrait::LastStand)==0 && Widget->GetDisplayedAvailablePoints()==8,
			TEXT("level20_lock_rejects_purchase_without_spending"));
		Widget->SelectTrait(ERogue10mTownTrait::CriticalChance);
		Key(EKeys::Enter); Key(EKeys::Enter); Key(EKeys::Enter);
		Check(Town->GetTraitLevel(ERogue10mTownTrait::CriticalChance)==3 && Widget->GetDisplayedAvailablePoints()==6,
			TEXT("trait_cap_rejects_extra_purchase_without_spending"));
		Key(EKeys::R);
		Check(Widget->IsResetConfirmationVisible(), TEXT("R_opens_refund_confirmation"));
		Key(EKeys::Escape);
		Check(!Widget->IsResetConfirmationVisible() && Town->GetSpentTraitPoints()==3 && PC->IsTraitWindowVisible(),
			TEXT("Escape_cancels_refund_and_keeps_trait_window"));
		PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"),false);
		Later(1.0,[this]
		{
			CheckRenderedCards(TEXT("traits-720p.png"),[this] { SaveFailureAndRetry(); });
		});
	}
	void SaveFailureAndRetry()
	{
		URogue10mProgressionSubsystem* Save = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
		const FString SaveSlot = Save ? Save->GetSaveSlotForProfile(Save->GetAttachedProfileId()) : FString();
		Check(Save && SaveSlot.StartsWith(ExpectedSavePrefix), TEXT("write_failure_target_is_isolated_QA_save"));
		if (Failures) { Finish(); return; }
		SaveFile = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),SaveSlot+TEXT(".sav"));
		Check(FFileHelper::LoadFileToArray(BeforeSaveBytes,*SaveFile) && FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,true),
			TEXT("isolated_saved_file_write_failure_fixture_created"));
		bSaveReadOnly = true;
		if (Failures) { Finish(); return; }
		Widget->SelectTrait(ERogue10mTownTrait::Defense);
		Key(EKeys::Enter);
		Check(Town->GetTraitLevel(ERogue10mTownTrait::Defense)==1 && !Save->IsSaveReady() && Save->CanRetrySave()
			&& Widget->GetDisplayedStatus().ToString().Contains(TEXT("현재 세션"))
			&& Widget->GetDisplayedStatus().ToString().Contains(TEXT("저장 재시도")),
			TEXT("failed_write_keeps_session_investment_and_reports_retry_needed"));
		Key(EKeys::Down); Key(EKeys::Down);
		Later(0.35,[this]
		{
			SelectedCardVisible();
			URogue10mProgressionSubsystem* Save = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
			UButton* Retry = Cast<UButton>(Widget->GetWidgetFromName(TEXT("TraitSaveRetryButton")));
			UButton* Upgrade = Cast<UButton>(Widget->GetWidgetFromName(TEXT("TraitUpgradeButton")));
			UButton* Reset = Cast<UButton>(Widget->GetWidgetFromName(TEXT("TraitResetButton")));
			TArray<uint8> AfterBytes;
			Check(PC->IsTraitWindowVisible() && Retry && Retry->GetIsEnabled()
				&& Upgrade && !Upgrade->GetIsEnabled() && Reset && !Reset->GetIsEnabled(),
				TEXT("NPC_timer_keeps_retry_window_but_blocks_further_mutations"));
			Check(FFileHelper::LoadFileToArray(AfterBytes,*SaveFile) && BeforeSaveBytes==AfterBytes,
				TEXT("failed_UI_save_preserves_last_valid_disk_bytes"));
			bSaveReadOnly=!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false);
			Check(!bSaveReadOnly,TEXT("isolated_QA_save_write_protection_restored"));
			MouseClick(TEXT("TraitSaveRetryButton"));
			Check(Save && Save->IsSaveReady() && Widget->GetDisplayedStatus().ToString().Contains(TEXT("저장 완료")),
				TEXT("actual_retry_button_recovers_write_failure_and_reports_saved"));
			ConfirmRefund();
		});
	}
	void ConfirmRefund()
	{
		Key(EKeys::R);
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,TEXT("traits-refund-720p.png")),true,false);
		Later(0.7,[this]
		{
			ModalGeometry();
			const int32 Level = State->GetPlayerLevel();
			const FRogue10mInventorySnapshot Inventory = Player->GetInventoryComponent()->CaptureInventory();
			Key(EKeys::Enter);
			Check(!Widget->IsResetConfirmationVisible() && Town->GetSpentTraitPoints()==0 && Widget->GetDisplayedAvailablePoints()==9,
				TEXT("confirmed_refund_returns_all_spent_points"));
			const FRogue10mInventorySnapshot AfterInventory = Player->GetInventoryComponent()->CaptureInventory();
			Check(State->GetPlayerLevel()==Level && FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(
				&Inventory,&AfterInventory,0),TEXT("refund_preserves_level_and_items"));
			Check(Widget->GetDisplayedStatus().ToString().Contains(TEXT("저장 완료")), TEXT("trait_change_reports_actual_save_success"));
			PC->SetIgnoreMoveInput(true); // A separate owner adds a block while our window is open.
			Key(EKeys::Escape);
			Check(!PC->IsTraitWindowVisible() && !PC->IsAnyBlockingWindowVisible() && !PC->bShowMouseCursor
				&& PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored(), TEXT("close_restores_game_input_and_preserves_external_ignore"));
			PC->SetIgnoreMoveInput(false);
			NPC->BeginDialogue(Player.Get());
			Check(NPC->OpenTraitWindow(), TEXT("trait_window_can_reopen_after_close"));
			Player->SetActorLocation(NPC->GetActorLocation()+FVector(1200,0,100),false,nullptr,ETeleportType::TeleportPhysics);
			Later(0.35,[this]
			{
				Check(!PC->IsTraitWindowVisible() && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored(),
					TEXT("leaving_NPC_range_closes_and_restores_input"));
				Finish();
			});
		});
	}
	void Finish()
	{
		if (bFinished) { return; }
		bFinished = true;
		if (bSaveReadOnly && !SaveFile.IsEmpty())
		{
			bSaveReadOnly=!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SaveFile,false);
			Check(!bSaveReadOnly,TEXT("QA_write_protection_restored_on_finish"));
		}
		if (PC.IsValid()) { PC->CloseTraitWindow(); }
		if (!Output.IsEmpty())
		{
			for (const TCHAR* Name : {TEXT("traits-1080p.png"),TEXT("traits-720p.png"),TEXT("traits-refund-720p.png")})
			{
				Check(IFileManager::Get().FileSize(*FPaths::Combine(Output,Name))>24,
					*FString::Printf(TEXT("actual_UMG_%s_written"),Name));
			}
			Results += FString::Printf(TEXT("RESULT=TRAIT_UI_%s failures=%d\n"),Failures?TEXT("FAILED"):TEXT("PASSED"),Failures);
			FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("ui-results.txt")));
		}
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=TRAIT_UI_%s failures=%d"),Failures?TEXT("FAILED"):TEXT("PASSED"),Failures);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mTownProgressionComponent> Town;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<URogue10mTraitWidget> Widget;
	TFunction<void()> Next;
	double NextTime = 0;
	FString Output,Results,ExpectedSavePrefix,SaveFile;
	TArray<uint8> BeforeSaveBytes;
	TSet<ERogue10mTownTrait> RenderedTraitIds;
	int32 Failures = 0;
	bool bFinished = false;
	bool bSaveReadOnly = false;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestTraitUI"),
	TEXT("Tests real trait buttons, keyboard, 720p/1080p layout, refund and input restoration with isolated saves."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor || Active) { return; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->WorldType==EWorldType::Game)
			{
				Active=MakeShared<FRun>(); Active->Start(*World); return;
			}
		}
	}));
}
#endif
