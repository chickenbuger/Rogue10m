// Opt-in rendered lobby QA. All actions use visible UMG controls; no physical-input claim.
#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterCustomizationPreviewActor.h"
#include "Rogue10mCharacterLobbyWidget.h"
#include "Rogue10mCharacterProfileSaveGame.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mMainMenuWidget.h"
#include "Rogue10mMenuGameMode.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace Rogue10mCharacterLobbyStyleQA
{
static const FColor HumanDefaultSkin(0xC8, 0x90, 0x70);
static const FColor DwarfDefaultSkin(0xAE, 0x75, 0x51);
static const FColor OrcDefaultSkin(0x74, 0x92, 0x4C);
static const FColor DwarfSwatchSkin(0xFF, 0xDF, 0xC3);
static const FColor HumanCustomSkin(0x7B, 0x4C, 0x38);
static const FName SkinParameterName(TEXT("RogueSkinColor"));
static const FName BodySlotName(TEXT("M_Body"));
static const FName HeadSlotName(TEXT("M_Head"));

static FString Csv(FString Value)
{
	return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")) + TEXT("\"");
}

static bool OneOption(const TArray<FString>& Options, const TCHAR* Name, FString& Value)
{
	int32 Count = 0;
	const FString Prefix = FString(Name) + TEXT("=");
	for (const FString& Option : Options)
	{
		if (!Option.StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
		++Count; Value = Option.Mid(Prefix.Len());
		if (Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) { Value = Value.Mid(1, Value.Len() - 2); }
	}
	return Count == 1 && !Value.IsEmpty();
}

class FRun : public TSharedFromThis<FRun>
{
public:
	bool Finished() const { return bFinished; }
	void Resume() { bHoldReleased = true; }
	void Start(UWorld& Initial)
	{
		World = &Initial;
		TArray<FString> Tokens, Options;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
		FString Requested;
		bool bValid = FParse::Param(FCommandLine::Get(), TEXT("RogueCharacterLobbyStyleQA"))
			&& OneOption(Options, TEXT("RogueCharacterLobbyStyleLabel"), Label)
			&& OneOption(Options, TEXT("RogueCharacterLobbyStyleOutput"), Requested)
			&& OneOption(Options, TEXT("RogueProfileSaveSlot"), TownSlot)
			&& OneOption(Options, TEXT("RogueProgressionSavePrefix"), Prefix)
			&& OneOption(Options, TEXT("RogueSkillKeySaveSlot"), KeySlot);
		bValid &= Label.Len() >= 1 && Label.Len() <= 24;
		for (TCHAR C : Label) { bValid &= C < 128 && (FChar::IsAlnum(C) || C == TEXT('_')); }
		bValid &= TownSlot == TEXT("Rogue10m_Town_QA_LobbyStyle_") + Label
			&& Prefix == TEXT("Rogue10m_Progression_QA_LobbyStyle_") + Label
			&& KeySlot == TEXT("Rogue10m_Fighter_QA_LobbyStyle_") + Label;
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tmp/CharacterLobbyStyleQA") / Label);
		bValid &= !Requested.IsEmpty() && FPaths::IsSamePath(Output, FPaths::ConvertRelativePathToFull(Requested))
			&& !IFileManager::Get().DirectoryExists(*Output) && !IFileManager::Get().FileExists(*Output);
		Profiles = Initial.GetGameInstance() ? Initial.GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		bValid &= !GIsEditor && Initial.WorldType == EWorldType::Game && Initial.GetNetMode() == NM_Standalone
			&& UWorld::RemovePIEPrefix(Initial.GetOutermost()->GetName()) == TEXT("/Game/Rogue10m/Maps/StartHub")
			&& Initial.GetAuthGameMode<ARogue10mMenuGameMode>() && Profiles.IsValid()
			&& Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile()
			&& GEngine && GEngine->GameViewport && FSlateApplication::IsInitialized()
			&& !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
		TArray<FString> Existing;
		IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(Prefix + TEXT("_*.sav")), true, false);
		bValid &= Existing.IsEmpty() && !UGameplayStatics::DoesSaveGameExist(TownSlot, 0) && !UGameplayStatics::DoesSaveGameExist(KeySlot, 0);
		if (!bValid)
		{
			bFinished = true;
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=CHARACTER_LOBBY_STYLE_REJECTED exact_fresh_namespaces_output_rendered_editor_game_StartHub_profile0_required"));
			FPlatformMisc::RequestExitWithStatus(false, 3); return;
		}
		// Snapshot pre-existing saves in memory only. Test code never restores or deletes saves.
		IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), TEXT("*.sav"), true, false);
		for (const FString& File : Existing)
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *File))
			{
				bFinished = true;
				UE_LOG(LogRogue10m, Error, TEXT("RESULT=CHARACTER_LOBBY_STYLE_REJECTED existing_save_snapshot_unreadable"));
				FPlatformMisc::RequestExitWithStatus(false, 3); return;
			}
			OriginalSaves.Add(File, MoveTemp(Bytes));
		}
		if (!IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true))
		{
			bFinished = true;
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=CHARACTER_LOBBY_STYLE_REJECTED output_creation_failed"));
			FPlatformMisc::RequestExitWithStatus(false, 3); return;
		}
		Started = StepStarted = FPlatformTime::Seconds(); NextAt = Started + 1.5;
		bHold = FParse::Param(FCommandLine::Get(), TEXT("RogueCharacterLobbyStyleHold"));
		ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak = TWeakPtr<FRun>(AsShared())]
		{
			if (const auto Self = Weak.Pin(); Self && !Self->bFinished)
			{
				Self->Check(false, TEXT("external_exit_before_driver_completion")); Self->Finish(false);
			}
		});
		BuildSteps();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float) { return Self->Tick(); }), 0.04f);
		UE_LOG(LogRogue10m, Display, TEXT("CHARACTER_LOBBY_STYLE_STARTED label=%s scope=programmatic_rendered_UMG physical_input=not_performed output=%s"), *Label, *Output);
	}

private:
	struct FStep { FString Name; TFunction<bool()> Work; double Delay; };
	void Add(const TCHAR* Name, TFunction<bool()> Work, double Delay = 1.0)
	{
		Steps.Add(FStep{FString(Name), MoveTemp(Work), Delay});
	}
	void Check(bool Condition, const FString& Name)
	{
		++Checks; if (!Condition) { ++Failures; }
		Results += FString::Printf(TEXT("%s %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Name);
		CsvChecks += FString::Printf(TEXT("%.3f,%s,%s,%s\n"), FPlatformTime::Seconds() - Started,
			*Csv(Step < Steps.Num() ? Steps[Step].Name : TEXT("finish")), Condition ? TEXT("PASS") : TEXT("FAIL"), *Csv(Name));
		UE_LOG(LogRogue10m, Display, TEXT("CHARACTER_LOBBY_STYLE_CHECK %s %s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Name);
	}
	template<class T> T* VisibleWidget() const
	{
		for (TObjectIterator<T> It; It; ++It)
		{
			if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get() && It->IsInViewport() && It->IsVisible()) { return *It; }
		}
		return nullptr;
	}
	static bool Shown(const UWidget* Widget)
	{
		if (!Widget || !Widget->GetCachedWidget().IsValid()) { return false; }
		for (const UWidget* Cursor = Widget; Cursor; Cursor = Cursor->GetParent()) { if (!Cursor->IsVisible()) { return false; } }
		return true;
	}
	UWidget* Node(const FString& Name) const
	{
		return Lobby.IsValid() ? Lobby->GetWidgetFromName(FName(*Name)) : nullptr;
	}
	bool ReadyLobby() const { return Lobby.IsValid() && PC.IsValid() && PC->IsCharacterLobbyVisible(); }
	bool Creation() const { return Shown(Node(TEXT("UI_CreationPanel"))); }
	bool Click(UUserWidget* Widget, const TCHAR* Name)
	{
		auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(FName(Name))) : nullptr;
		bool bReady = Shown(Button);
		for (const UWidget* Cursor = Button; Cursor; Cursor = Cursor->GetParent()) { bReady &= Cursor->GetIsEnabled(); }
		Check(bReady, FString(TEXT("visible_enabled_programmatic_button_")) + Name);
		if (bReady) { Button->OnClicked.Broadcast(); }
		return bReady;
	}
	bool ClickLobby(const TCHAR* Name) { return Click(Lobby.Get(), Name); }
	void Photo(const TCHAR* Name)
	{
		const FString File = Output / TEXT("screenshots") / Name;
		Check(!FScreenshotRequest::IsScreenshotRequested(), FString(TEXT("no_pending_screenshot_")) + Name);
		if (FScreenshotRequest::IsScreenshotRequested()) { return; }
		Photos.Add(File);
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogRogue10m, Display, TEXT("CHARACTER_LOBBY_STYLE_SCREENSHOT requested_not_visually_reviewed=%s"), *File);
	}
	void Draft(const TCHAR* RaceButton, const TCHAR* GenderButton, const TCHAR* Name)
	{
		ClickLobby(RaceButton); ClickLobby(GenderButton);
		auto* Input = Cast<UEditableTextBox>(Node(TEXT("UI_CharacterNameInput")));
		Check(Shown(Input), TEXT("creation_name_input_visible"));
		if (Input) { Input->SetText(FText::FromString(Name)); }
	}
	void CheckHit(UWidget* Widget, const FString& Name)
	{
		if (!Shown(Widget)) { Check(false, Name + TEXT("_visible_for_hit_test")); return; }
		const FVector2D Center = Widget->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5));
		const FWidgetPath Hit = FSlateApplication::Get().LocateWindowUnderMouse(Center, FSlateApplication::Get().GetInteractiveTopLevelWindows());
		Check(Hit.ContainsWidget(Widget->GetCachedWidget().Get()), Name + TEXT("_native_Slate_center_hit_path"));
	}
	const USkeletalMeshComponent* FindPreviewBody(const FString& ImageName) const
	{
		const UImage* Image = Cast<UImage>(Node(ImageName));
		if (!World.IsValid() || !Shown(Image))
		{
			return nullptr;
		}
		UObject* Brush = Image->GetBrush().GetResourceObject();
		const UMaterialInstanceDynamic* BrushMaterial = Cast<UMaterialInstanceDynamic>(Brush);
		UTexture* PreviewTexture = Cast<UTexture>(Brush);
		if (BrushMaterial)
		{
			BrushMaterial->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("PreviewTexture")), PreviewTexture);
		}
		for (TActorIterator<ARogue10mCharacterCustomizationPreviewActor> Actor(World.Get()); PreviewTexture && Actor; ++Actor)
		{
			if (Actor->GetRenderTarget() != PreviewTexture)
			{
				continue;
			}
			for (const USkeletalMeshComponent* Mesh : TInlineComponentArray<USkeletalMeshComponent*>(*Actor))
			{
				if (Mesh->IsVisible() && Mesh->GetMaterialIndex(BodySlotName) != INDEX_NONE)
				{
					return Mesh;
				}
			}
		}
		return nullptr;
	}
	void CheckMeshSkin(const USkeletalMeshComponent* Mesh, FColor Expected, const FString& Name)
	{
		const FLinearColor LinearExpected = FLinearColor::FromSRGBColor(Expected);
		for (const FName Slot : {HeadSlotName, BodySlotName})
		{
			const int32 Index = Mesh ? Mesh->GetMaterialIndex(Slot) : INDEX_NONE;
			const auto* Instance = Index != INDEX_NONE ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Index)) : nullptr;
			FLinearColor Actual;
			const bool bHasColor = Instance && Instance->GetVectorParameterValue(FHashedMaterialParameterInfo(SkinParameterName), Actual);
			Check(bHasColor && Actual.Equals(LinearExpected, 0.0001f), Name + TEXT("_") + Slot.ToString() + TEXT("_skin_color"));
		}
	}
	void CheckDraftSkin(FColor Expected, const FString& Name)
	{
		CheckMeshSkin(FindPreviewBody(TEXT("UI_CharacterPreviewImage")), Expected, Name);
	}
	void CheckSlotSkin(int32 SlotNumber, FColor Expected, const FString& Name)
	{
		const FString ImageName = FString::Printf(TEXT("UI_Slot%dPreviewImage"), SlotNumber);
		CheckMeshSkin(FindPreviewBody(ImageName), Expected, Name);
	}
	void CommitSkinHex(const TCHAR* Value)
	{
		auto* Input = Cast<UEditableTextBox>(Node(TEXT("SkinColorHexInput")));
		const bool bReady = Shown(Input) && Input->GetIsEnabled();
		Check(bReady, TEXT("skin_hex_visible_and_enabled"));
		if (bReady)
		{
			const FText Text = FText::FromString(Value);
			Input->SetText(Text);
			Input->OnTextCommitted.Broadcast(Text, ETextCommit::OnEnter);
		}
	}
	void CheckSavedSkin(const FGuid& ProfileId, TOptional<FColor> Expected, const FString& Name)
	{
		const auto* Saved = Cast<URogue10mCharacterProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(TownSlot, 0));
		const auto* Profile = Saved ? Saved->Profiles.FindByPredicate([&ProfileId](const FRogue10mCharacterProfile& Candidate)
		{
			return Candidate.ProfileId == ProfileId;
		}) : nullptr;
		Check(Profile && (Expected.IsSet()
			? Profile->Appearance.bUseCustomSkinColor && Profile->Appearance.CustomSkinColor == Expected.GetValue()
			: !Profile->Appearance.bUseCustomSkinColor && Profile->Appearance.SkinToneIndex == 0), Name);
	}
	void CheckSlots(int32 Count, int32 Selected)
	{
		Check(ReadyLobby() && !Creation() && Profiles.IsValid() && Profiles->GetProfileCount() == Count, TEXT("selection_mode_profile_count"));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FString Stem = FString::Printf(TEXT("UI_Slot%d"), Index + 1);
			const auto* Name = Cast<UTextBlock>(Node(Stem + TEXT("Text")));
			const auto* Detail = Cast<UTextBlock>(Node(Stem + TEXT("DetailText")));
			const UWidget* Badge = Node(Stem + TEXT("SelectedBadge"));
			auto* Empty = Cast<UButton>(Node(Stem + TEXT("EmptyButton")));
			const auto* Preview = Cast<UImage>(Node(Stem + TEXT("PreviewImage")));
			Check(Name && Detail && Badge && Empty && Preview, Stem + TEXT("_styled_nodes_present"));
			if (!Name || !Detail || !Badge || !Empty || !Preview) { continue; }
			const bool bOccupied = Index < Count;
			Check(Shown(Badge) == (bOccupied && Index == Selected), Stem + TEXT("_selected_badge_matches_slot"));
			Check(Shown(Empty) == !bOccupied && Shown(Preview) == bOccupied, Stem + TEXT("_empty_button_and_preview_visibility"));
			if (bOccupied && Profiles.IsValid() && Profiles->GetProfiles().IsValidIndex(Index))
			{
				const FRogue10mCharacterProfile& Profile = Profiles->GetProfiles()[Index];
				Check(Name->GetText().ToString() == Profile.CharacterName && !Detail->GetText().IsEmpty(), Stem + TEXT("_split_name_and_detail"));
				const FLinearColor Tint = Preview->GetColorAndOpacity();
				Check(Index == Selected ? Tint.Equals(FLinearColor::White, 0.02f) : Tint.R < 0.99f, Stem + TEXT("_selection_preview_tint"));
				CheckHit(Node(Stem + TEXT("Button")), Stem + TEXT("_nameplate"));
			}
			else
			{
				Check(!Name->GetText().IsEmpty() && Detail->GetText().ToString().Contains(TEXT("캐릭터 생성")), Stem + TEXT("_empty_slot_copy"));
				CheckHit(Empty, Stem + TEXT("_empty_create"));
			}
			if (const auto* Slot = Cast<UCanvasPanelSlot>(Preview->Slot))
			{
				Check(Slot->GetSize().Equals(FVector2D(360.0, 600.0), 0.1), Stem + TEXT("_preview_360x600"));
			}
			else { Check(false, Stem + TEXT("_preview_canvas_slot")); }
		}
		const UWidget* New = Node(TEXT("UI_NewCharacterButton"));
		Check(New && (Count < 3 ? Shown(New) && New->GetIsEnabled() : !Shown(New) || !New->GetIsEnabled()), TEXT("new_character_availability_matches_free_slots"));
		Check(Shown(Node(TEXT("UI_SelectionHintText"))), TEXT("selection_hint_visible_outside_creation"));
		if (Count > 0)
		{
			CheckHit(Node(TEXT("UI_EnterGameButton")), TEXT("bottom_enter"));
			CheckHit(Node(TEXT("UI_DeleteCharacterButton")), TEXT("bottom_delete"));
		}
		if (Count < 3) { CheckHit(Node(TEXT("UI_NewCharacterButton")), TEXT("bottom_new_character")); }
	}
	void BuildSteps()
	{
		Add(TEXT("main_menu_start"), [this]
		{
			auto* Main = VisibleWidget<URogue10mMainMenuWidget>(); if (!PC.IsValid() || !Main) { return false; }
			return Click(Main, TEXT("UI_StartGameButton"));
		}, 1.5);
		Add(TEXT("initial_empty_state"), [this]
		{
			if (!ReadyLobby()) { return false; }
			Check(Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile(), TEXT("fresh_profile0_initial_state_preserved"));
			if (!Creation()) { CheckSlots(0, INDEX_NONE); }
			Photo(TEXT("01_empty_initial.png")); return true;
		}, 1.5);
		Add(TEXT("open_initial_creation_if_needed"), [this]
		{
			if (!ReadyLobby()) { return false; }
			if (Creation()) { return true; }
			return ClickLobby(TEXT("UI_Slot1EmptyButton"));
		});
		Add(TEXT("empty_cancel_guard"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			Check(!Shown(Node(TEXT("UI_SelectionHintText"))), TEXT("selection_hint_hidden_during_creation"));
			ClickLobby(TEXT("UI_CancelCreationButton"));
			Check(Creation() && Profiles->GetProfileCount() == 0, TEXT("cancel_without_first_profile_remains_creation")); return true;
		});
		Add(TEXT("draft_dwarf_male"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			Draft(TEXT("UI_DwarfRaceButton"), TEXT("UI_MaleGenderButton"), TEXT("시안검수01")); return true;
		}, 1.5);
		Add(TEXT("skin_default_and_controls"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			CheckHit(Node(TEXT("SkinColorSwatch_0")), TEXT("skin_swatch"));
			CheckHit(Node(TEXT("SkinColorHexInput")), TEXT("skin_hex_input"));
			CheckHit(Node(TEXT("SkinColorResetButton")), TEXT("skin_reset"));
			CheckDraftSkin(DwarfDefaultSkin, TEXT("draft_dwarf_male_natural"));
			Photo(TEXT("01a_creation_dwarf_default_skin.png"));
			return true;
		}, 1.5);
		Add(TEXT("natural_tones_all_archetypes"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			ClickLobby(TEXT("UI_FemaleGenderButton"));
			CheckDraftSkin(DwarfDefaultSkin, TEXT("draft_dwarf_female_natural"));
			ClickLobby(TEXT("UI_HumanRaceButton"));
			CheckDraftSkin(HumanDefaultSkin, TEXT("draft_human_female_natural"));
			ClickLobby(TEXT("UI_MaleGenderButton"));
			CheckDraftSkin(HumanDefaultSkin, TEXT("draft_human_male_natural"));
			ClickLobby(TEXT("UI_OrcRaceButton"));
			CheckDraftSkin(OrcDefaultSkin, TEXT("draft_orc_male_natural"));
			ClickLobby(TEXT("UI_FemaleGenderButton"));
			CheckDraftSkin(OrcDefaultSkin, TEXT("draft_orc_female_natural"));
			ClickLobby(TEXT("UI_DwarfRaceButton"));
			ClickLobby(TEXT("UI_MaleGenderButton"));
			return true;
		});
		Add(TEXT("preset_reset_and_hex_guard"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			ClickLobby(TEXT("SkinColorSwatch_0"));
			CheckDraftSkin(DwarfSwatchSkin, TEXT("draft_dwarf_swatch"));
			CommitSkinHex(TEXT("#NOTHEX"));
			CheckDraftSkin(DwarfSwatchSkin, TEXT("draft_rejects_invalid_skin_hex"));
			ClickLobby(TEXT("UI_CreateCharacterButton"));
			Check(Creation() && Profiles->GetProfileCount() == 0, TEXT("invalid_skin_hex_blocks_character_creation"));
			ClickLobby(TEXT("SkinColorResetButton"));
			CheckDraftSkin(DwarfDefaultSkin, TEXT("draft_reset_uses_race_default"));
			ClickLobby(TEXT("SkinColorSwatch_0"));
			CheckDraftSkin(DwarfSwatchSkin, TEXT("draft_dwarf_reapplies_swatch"));
			return true;
		}, 1.5);
		Add(TEXT("creation_capture"), [this] { Photo(TEXT("02_creation_dwarf.png")); return true; }, 1.5);
		Add(TEXT("create_profile1"), [this] { return ReadyLobby() && Creation() && ClickLobby(TEXT("UI_CreateCharacterButton")); }, 1.5);
		Add(TEXT("profile1_and_begin_cancel"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			CheckSlots(1, 0);
			if (Profiles->GetProfileCount() == 1)
			{
				const auto& P = Profiles->GetProfiles()[0];
				Check(P.CharacterName == TEXT("시안검수01") && P.Appearance.Race == ERogue10mCharacterRace::Dwarf && P.Appearance.Gender == ERogue10mCharacterGender::Male, TEXT("profile1_actual_UI_dwarf_male"));
				FirstProfile = P.ProfileId;
				CheckSavedSkin(FirstProfile, DwarfSwatchSkin, TEXT("profile1_swatch_opaque_srgb_on_disk"));
			}
			CheckSlotSkin(1, DwarfSwatchSkin, TEXT("profile1_swatch_in_selection_preview"));
			return ClickLobby(TEXT("UI_NewCharacterButton"));
		});
		Add(TEXT("cancel_existing_profile"), [this] { return ReadyLobby() && Creation() && ClickLobby(TEXT("UI_CancelCreationButton")); });
		Add(TEXT("cancel_preserved_profile1"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			Check(Profiles->GetProfileCount() == 1 && Profiles->FindProfile(FirstProfile), TEXT("creation_cancel_preserves_existing_profile"));
			CheckSlotSkin(1, DwarfSwatchSkin, TEXT("profile1_swatch_after_return_to_selection"));
			return ClickLobby(TEXT("UI_NewCharacterButton"));
		});
		Add(TEXT("draft_human_female"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			Draft(TEXT("UI_HumanRaceButton"), TEXT("UI_FemaleGenderButton"), TEXT("시안검수02")); return true;
		});
		Add(TEXT("draft_human_exact_skin_hex"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			CheckDraftSkin(HumanDefaultSkin, TEXT("new_draft_does_not_inherit_previous_custom_skin"));
			CommitSkinHex(TEXT("7b4c38"));
			CheckDraftSkin(HumanCustomSkin, TEXT("draft_human_hex_srgb"));
			return true;
		}, 1.5);
		Add(TEXT("draft_skin_slider_sync"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			auto* Slider = Cast<USlider>(Node(TEXT("SkinColorValueSlider")));
			const auto* Input = Cast<UEditableTextBox>(Node(TEXT("SkinColorHexInput")));
			CheckHit(Slider, TEXT("skin_value_slider"));
			if (Slider && Input)
			{
				const float NewValue = 0.75f;
				Slider->SetValue(NewValue);
				const FColor SliderColor = FColor::FromHex(Input->GetText().ToString());
				Check(SliderColor.A == 255 && SliderColor != HumanCustomSkin, TEXT("skin_value_slider_updates_hex_field"));
				CheckDraftSkin(SliderColor, TEXT("skin_value_slider_updates_both_slots"));
			}
			else
			{
				Check(false, TEXT("skin_slider_and_hex_input_available"));
			}
			CommitSkinHex(TEXT("#7B4C38"));
			CheckDraftSkin(HumanCustomSkin, TEXT("draft_human_hex_with_hash_srgb"));
			return true;
		}, 1.5);
		Add(TEXT("human_hex_capture"), [this] { Photo(TEXT("02a_creation_human_custom_skin.png")); return true; });
		Add(TEXT("create_profile2"), [this] { return ReadyLobby() && Creation() && ClickLobby(TEXT("UI_CreateCharacterButton")); }, 1.5);
		Add(TEXT("two_profiles_plus_empty"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			CheckSlots(2, 1);
			if (Profiles->GetProfileCount() == 2)
			{
				const auto& P = Profiles->GetProfiles()[1];
				Check(P.CharacterName == TEXT("시안검수02") && P.Appearance.Race == ERogue10mCharacterRace::Human && P.Appearance.Gender == ERogue10mCharacterGender::Female, TEXT("profile2_actual_UI_human_female"));
				SecondProfile = P.ProfileId;
				CheckSavedSkin(SecondProfile, HumanCustomSkin, TEXT("profile2_exact_hex_opaque_srgb_on_disk"));
			}
			CheckSlotSkin(1, DwarfSwatchSkin, TEXT("profile1_skin_unaffected_by_profile2"));
			CheckSlotSkin(2, HumanCustomSkin, TEXT("profile2_hex_in_selection_preview"));
			Photo(TEXT("03_two_profiles_plus_empty.png")); return true;
		}, 1.5);
		Add(TEXT("select_slot1"), [this] { return ReadyLobby() && !Creation() && ClickLobby(TEXT("UI_Slot1Button")); });
		Add(TEXT("slot1_selected_capture"), [this] { CheckSlots(2, 0); Photo(TEXT("04_slot1_selected.png")); return true; }, 1.5);
		Add(TEXT("select_slot2"), [this] { return ReadyLobby() && !Creation() && ClickLobby(TEXT("UI_Slot2Button")); });
		Add(TEXT("slot2_selected_capture"), [this] { CheckSlots(2, 1); Photo(TEXT("05_slot2_selected.png")); return true; }, 1.5);
		Add(TEXT("begin_profile3"), [this] { return ReadyLobby() && !Creation() && ClickLobby(TEXT("UI_Slot3EmptyButton")); });
		Add(TEXT("draft_orc_male"), [this]
		{
			if (!ReadyLobby() || !Creation()) { return false; }
			Draft(TEXT("UI_OrcRaceButton"), TEXT("UI_MaleGenderButton"), TEXT("시안검수03"));
			ClickLobby(TEXT("UI_BodyDetailNextButton"));
			CheckDraftSkin(OrcDefaultSkin, TEXT("draft_orc_tattoo_preserves_head_and_body_skin"));
			return true;
		});
		Add(TEXT("create_profile3"), [this] { return ReadyLobby() && Creation() && ClickLobby(TEXT("UI_CreateCharacterButton")); }, 1.5);
		Add(TEXT("full_three_slots"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			CheckSlots(3, 2);
			if (Profiles->GetProfileCount() == 3)
			{
				const auto& P = Profiles->GetProfiles()[2];
				Check(P.CharacterName == TEXT("시안검수03") && P.Appearance.Race == ERogue10mCharacterRace::Orc && P.Appearance.Gender == ERogue10mCharacterGender::Male, TEXT("profile3_actual_UI_orc_male"));
				ThirdProfile = P.ProfileId;
				CheckSavedSkin(ThirdProfile, {}, TEXT("profile3_no_custom_uses_legacy_skin_index_on_disk"));
			}
			CheckSlotSkin(3, OrcDefaultSkin, TEXT("profile3_default_orc_tattoo_selection"));
			Photo(TEXT("06_full_three_slots.png")); return true;
		}, 1.5);
		Add(TEXT("reload_menu_world"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			UGameplayStatics::OpenLevel(World.Get(), FName(TEXT("/Game/Rogue10m/Maps/StartHub")), true,
				TEXT("game=/Script/Rogue10m.Rogue10mMenuGameMode"));
			return true;
		}, 2.0);
		Add(TEXT("reopen_character_lobby"), [this]
		{
			auto* Main = VisibleWidget<URogue10mMainMenuWidget>();
			return PC.IsValid() && PC->IsMainMenuVisible() && Main && Click(Main, TEXT("UI_StartGameButton"));
		}, 1.5);
		Add(TEXT("reopened_lobby_skin_restored"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			CheckSlots(3, 2);
			CheckSlotSkin(1, DwarfSwatchSkin, TEXT("reopened_dwarf_swatch"));
			CheckSlotSkin(2, HumanCustomSkin, TEXT("reopened_human_hex"));
			CheckSlotSkin(3, OrcDefaultSkin, TEXT("reopened_orc_default"));
			return true;
		});
		Add(TEXT("delete_first_confirmation"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			ClickLobby(TEXT("UI_DeleteCharacterButton"));
			Check(Profiles->GetProfileCount() == 3 && Profiles->FindProfile(ThirdProfile), TEXT("delete_first_click_does_not_remove_profile")); return true;
		});
		Add(TEXT("delete_second_confirmation"), [this] { return ReadyLobby() && !Creation() && ClickLobby(TEXT("UI_DeleteCharacterButton")); }, 1.5);
		Add(TEXT("deleted_slot_reopens_creation"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			Check(Profiles->GetProfileCount() == 2 && !Profiles->FindProfile(ThirdProfile) && Profiles->FindProfile(FirstProfile) && Profiles->FindProfile(SecondProfile), TEXT("delete_second_click_removes_only_selected_third_profile"));
			return ClickLobby(TEXT("UI_Slot3EmptyButton"));
		});
		Add(TEXT("empty_slot_creation_cancel"), [this] { return ReadyLobby() && Creation() && ClickLobby(TEXT("UI_CancelCreationButton")); });
		Add(TEXT("select_final_slot2"), [this]
		{
			if (!ReadyLobby() || Creation()) { return false; }
			Check(Profiles->GetProfileCount() == 2, TEXT("empty_slot_creation_cancel_creates_no_profile"));
			return ClickLobby(TEXT("UI_Slot2Button"));
		});
		Add(TEXT("ready_to_enter_capture"), [this] { CheckSlots(2, 1); Photo(TEXT("07_ready_to_enter.png")); return true; }, 1.5);
		Add(TEXT("optional_handoff_hold"), [this]
		{
			if (!bHold || bHoldReleased || !PC.IsValid() || !PC->IsCharacterLobbyVisible()) { return true; }
			if (!bHoldAnnounced)
			{
				bHoldAnnounced = true;
				FFileHelper::SaveStringToFile(TEXT("state=held_before_enter\nphysical_input=not_performed\nresume=Rogue10m.ResumeCharacterLobbyStyleQA_or_enter_game\n"), *(Output / TEXT("hold.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
				UE_LOG(LogRogue10m, Display, TEXT("CHARACTER_LOBBY_STYLE_HOLD ready_for_external_review; enter_game_or_Rogue10m.ResumeCharacterLobbyStyleQA; total_watchdog150s"));
			}
			return false;
		});
		Add(TEXT("enter_selected_profile"), [this]
		{
			if (PC.IsValid() && !PC->IsCharacterLobbyVisible()) { return true; }
			return ReadyLobby() && !Creation() && ClickLobby(TEXT("UI_EnterGameButton"));
		}, 2.0);
		Add(TEXT("entered_StartHub"), [this]
		{
			if (!World.IsValid() || !PC.IsValid() || PC->IsCharacterLobbyVisible() || PC->IsMainMenuVisible()) { return false; }
			auto* Character = Cast<ARogue10mCharacter>(PC->GetPawn());
			auto* Save = World->GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>();
			if (!Character || !Save || !Save->IsSaveReady()) { return false; }
			const auto* Profile = Profiles.IsValid() ? Profiles->GetSelectedProfile() : nullptr;
			Check(UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub"), TEXT("entered_actual_StartHub_gameplay"));
			Check(Profile && Profile->ProfileId == SecondProfile && Character->GetCharacterDisplayName().ToString() == TEXT("시안검수02"), TEXT("entered_selected_profile_name_and_GUID_applied"));
			Check(Character->GetCharacterAppearance().Race == ERogue10mCharacterRace::Human && Character->GetCharacterAppearance().Gender == ERogue10mCharacterGender::Female, TEXT("entered_selected_human_female_appearance"));
			CheckMeshSkin(Character->GetMesh(), HumanCustomSkin, TEXT("selected_skin_in_actual_StartHub_pawn"));
			Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !UGameplayStatics::IsGamePaused(World.Get()), TEXT("gameplay_move_look_and_pause_restored"));
			Check(Save->GetAttachedProfileId() == SecondProfile && Save->IsAttachedTo(*Character), TEXT("progression_attached_to_actual_selected_profile"));
			Check(UGameplayStatics::DoesSaveGameExist(TownSlot, 0) && Save->GetSaveSlotForProfile(SecondProfile).StartsWith(Prefix + TEXT("_")), TEXT("QA_profile_save_and_progression_namespace_used"));
			Photo(TEXT("08_entered_StartHub.png")); return true;
		}, 2.0);
	}
	bool Tick()
	{
		if (bFinished) { return false; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* Current = Context.World(); Current && Context.WorldType == EWorldType::Game && Current->GetNetMode() == NM_Standalone)
			{
				World = Current; PC = Cast<ARogue10mPlayerController>(Current->GetFirstPlayerController());
				Profiles = Current->GetGameInstance() ? Current->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
				break;
			}
		}
		Lobby = VisibleWidget<URogue10mCharacterLobbyWidget>();
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 150.0)
		{
			Check(false, TEXT("total_150_second_watchdog")); Finish(); return false;
		}
		if (Now < NextAt) { return true; }
		if (Step >= Steps.Num()) { Finish(); return false; }
		if (Steps[Step].Work())
		{
			if (Failures) { Finish(); return false; }
			NextAt = Now + Steps[Step].Delay; ++Step; StepStarted = Now;
		}
		else if (Now - StepStarted > (Steps[Step].Name == TEXT("optional_handoff_hold") ? 150.0 : 18.0))
		{
			Check(false, TEXT("stage_timeout_") + Steps[Step].Name); Finish(); return false;
		}
		return !bFinished;
	}
	void Finish(bool bRequestExit = true)
	{
		if (bFinished) { return; }
		bFinished = true;
		FCoreDelegates::OnPreExit.Remove(ExitHandle);
		bool bOriginalsUnchanged = true;
		for (const auto& Pair : OriginalSaves)
		{
			TArray<uint8> Current;
			bOriginalsUnchanged &= FFileHelper::LoadFileToArray(Current, *Pair.Key) && Current == Pair.Value;
		}
		Check(bOriginalsUnchanged, TEXT("all_preexisting_save_bytes_preserved"));
		TArray<FString> CurrentSaves;
		IFileManager::Get().FindFilesRecursive(CurrentSaves, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), TEXT("*.sav"), true, false);
		bool bOnlyOwnedNewSaves = true;
		for (const FString& File : CurrentSaves)
		{
			if (OriginalSaves.Contains(File)) { continue; }
			const FString Base = FPaths::GetBaseFilename(File);
			bOnlyOwnedNewSaves &= Base == TownSlot || Base == KeySlot || Base.StartsWith(Prefix + TEXT("_"));
		}
		Check(bOnlyOwnedNewSaves, TEXT("all_new_save_files_are_owned_QA_namespaces"));
		for (const FString& PhotoFile : Photos) { Check(IFileManager::Get().FileSize(*PhotoFile) > 0, TEXT("screenshot_file_written_") + FPaths::GetCleanFilename(PhotoFile)); }
		const FString Summary = FString::Printf(TEXT("checks=%d\nfailures=%d\nstate=%s\nelapsed_seconds=%.3f\nscope=programmatic_rendered_UMG\nphysical_input=not_performed\nhit_test=native_Slate_path_at_widget_centers\npreexisting_save_count=%d\npreexisting_save_bytes_preserved=%d\nnew_saves_owned_QA_namespaces=%d\nQA_files=retained_no_delete_or_restore\nscreenshots=%d\n"),
			Checks, Failures, Failures ? TEXT("failed") : TEXT("passed"), FPlatformTime::Seconds() - Started, OriginalSaves.Num(), bOriginalsUnchanged, bOnlyOwnedNewSaves, Photos.Num());
		const bool bWritten = FFileHelper::SaveStringToFile(Results, *(Output / TEXT("checks.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			&& FFileHelper::SaveStringToFile(CsvChecks, *(Output / TEXT("checks.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			&& FFileHelper::SaveStringToFile(Summary, *(Output / TEXT("summary.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (!bWritten) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("CHARACTER_LOBBY_STYLE_EVIDENCE_WRITE_FAILED")); }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=CHARACTER_LOBBY_STYLE_%s checks=%d failures=%d physical_input=not_performed hit_test=native_Slate_path_at_widget_centers output=%s"), Failures ? TEXT("FAILED") : TEXT("PASSED"), Checks, Failures, *Output);
		if (bRequestExit) { FPlatformMisc::RequestExitWithStatus(false, Failures ? 2 : 0); }
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<URogue10mCharacterLobbyWidget> Lobby;
	TWeakObjectPtr<URogue10mCharacterProfileSubsystem> Profiles;
	TMap<FString, TArray<uint8>> OriginalSaves;
	TArray<FStep> Steps;
	TArray<FString> Photos;
	FGuid FirstProfile, SecondProfile, ThirdProfile;
	FString Label, Output, TownSlot, Prefix, KeySlot, Results;
	FString CsvChecks = TEXT("elapsed_seconds,stage,result,check\n");
	FDelegateHandle ExitHandle;
	double Started = 0.0, NextAt = 0.0, StepStarted = 0.0;
	int32 Step = 0, Checks = 0, Failures = 0;
	bool bFinished = false, bHold = false, bHoldReleased = false, bHoldAnnounced = false;
};

static TSharedPtr<FRun> Active;
static void Resume() { if (Active && !Active->Finished()) { Active->Resume(); } }
static FAutoConsoleCommand ResumeCommand(TEXT("Rogue10m.ResumeCharacterLobbyStyleQA"), TEXT("Resume the explicitly opted-in isolated lobby style driver after its review hold."), FConsoleCommandDelegate::CreateStatic(&Resume));

static void Schedule()
{
	static bool bScheduled = false;
	if (bScheduled || GIsEditor || !FParse::Param(FCommandLine::Get(), TEXT("RogueCharacterLobbyStyleQA"))) { return; }
	bScheduled = true;
	const double Deadline = FPlatformTime::Seconds() + 60.0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline](float)
	{
		if (GEngine && GEngine->GameViewport)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (UWorld* World = Context.World(); World && Context.WorldType == EWorldType::Game && World->HasBegunPlay() && World->GetFirstPlayerController())
				{
					Active = MakeShared<FRun>(); Active->Start(*World); return false;
				}
			}
		}
		if (FPlatformTime::Seconds() < Deadline) { return true; }
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=CHARACTER_LOBBY_STYLE_REJECTED no_rendered_game_world_after_opt_in"));
		FPlatformMisc::RequestExitWithStatus(false, 3); return false;
	}));
}

static FDelayedAutoRegisterHelper AutoStart(EDelayedRegisterRunPhase::EndOfEngineInit, &Schedule, true);
}
#endif
