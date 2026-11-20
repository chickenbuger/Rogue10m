// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Input/HittestGrid.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mItemDragDropOperation.h"
#include "Rogue10mMenuWindowWidgets.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Slate/UMGDragDropOp.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace Rogue10mInventoryUIQA
{
const FIntPoint SwordDestination(0, 5);
const FIntPoint GuardianOrigin(2, 0);
const FIntPoint GridSize(10, 10);
const TCHAR* PhotoName = TEXT("inventory-comparison.png");

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		const TCHAR* Args = FCommandLine::Get();
		FString Label, ProfileSlot, ProgressionPrefix, SkillSlot;
		FParse::Value(Args, TEXT("RogueInventoryQALabel="), Label);
		FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), SkillSlot);
		bRead = FParse::Param(Args, TEXT("RogueInventoryQARead"));
		bool bSafeLabel = !Label.IsEmpty() && Label.Len() <= 24;
		for (const TCHAR Ch : Label)
		{
			bSafeLabel &= (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'))
				|| (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
		}
		if (!bSafeLabel || ProfileSlot != TEXT("Rogue10m_Town_QA_InventoryUI_") + Label
			|| ProgressionPrefix != TEXT("Rogue10m_Progression_QA_InventoryUI_") + Label
			|| SkillSlot != TEXT("Rogue10m_Fighter_QA_InventoryUI_") + Label
			|| InWorld.GetNetMode() != NM_Standalone || FParse::Param(Args, TEXT("nullrhi"))
			|| InWorld.GetPackage()->GetName() != TEXT("/Game/Rogue10m/Maps/StartHub"))
		{
			UE_LOG(LogRogue10m, Error, TEXT("INVENTORY_UI_QA requires rendered standalone StartHub and isolated save flags."));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tmp/inventory-ui-qa")
			/ Label / (bRead ? TEXT("reload") : TEXT("play")));
		if (IFileManager::Get().DirectoryExists(*Output) || !IFileManager::Get().MakeDirectory(*Output, true))
		{
			UE_LOG(LogRogue10m, Error, TEXT("INVENTORY_UI_QA requires a fresh evidence directory: %s"), *Output);
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
		TArray<FString> ProgressionFiles;
		IFileManager::Get().FindFiles(ProgressionFiles, *(SaveDir / (ProgressionPrefix + TEXT("_*.sav"))), true, false);
		const bool bProfileExists = IFileManager::Get().FileExists(*(SaveDir / (ProfileSlot + TEXT(".sav"))));
		const bool bSkillSlotExists = IFileManager::Get().FileExists(*(SaveDir / (SkillSlot + TEXT(".sav"))));
		if (!Check(bRead ? bProfileExists && ProgressionFiles.Num() == 1
			: !bProfileExists && !bSkillSlotExists && ProgressionFiles.IsEmpty(), TEXT("isolated_expected_save_namespace")))
		{
			Finish();
			return;
		}
		Deadline = FPlatformTime::Seconds() + 60.0;
		Later(3.0, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			const double Now = FPlatformTime::Seconds();
			if (!Self->World.IsValid() || Now >= Self->Deadline)
			{
				Self->Check(false, TEXT("fixture_world_and_watchdog"));
				Self->Finish();
			}
			else if (Self->Next && Now >= Self->NextAt)
			{
				auto Work = MoveTemp(Self->Next);
				Work();
			}
			return !Self->bFinished;
		}), 0.03f);
	}

private:
	bool Check(bool bPass, const TCHAR* Reason)
	{
		++Checks;
		Failures += bPass ? 0 : 1;
		Results += FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bPass)
		{
			UE_LOG(LogRogue10m, Error, TEXT("INVENTORY_UI_QA FAIL %s"), Reason);
		}
		return bPass;
	}

	void Later(double Delay, TFunction<void()> Work)
	{
		NextAt = FPlatformTime::Seconds() + Delay;
		Next = MoveTemp(Work);
	}

	void Press(const FKey& Key) const
	{
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
		const TWeakObjectPtr<ARogue10mPlayerController> InputController = Controller;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([InputController, Key](float)
		{
			if (ARogue10mPlayerController* PC = InputController.Get())
			{
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
			}
			return false;
		}), 0.1f);
	}

	template <class WidgetType>
	WidgetType* VisibleWidget() const
	{
		for (TObjectIterator<WidgetType> It; It; ++It)
		{
			if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == Controller.Get()
				&& It->IsInViewport() && It->IsVisible())
			{
				return *It;
			}
		}
		return nullptr;
	}

	const FRogue10mInventoryGridEntry* Entry(FGuid Id) const
	{
		return Inventory->GetInventoryContainers()[0].Entries.FindByPredicate(
			[Id](const FRogue10mInventoryGridEntry& Record) { return Record.InstanceId == Id; });
	}

	URogue10mInventoryItemWidget* Tile(FGuid Id) const
	{
		for (UWidget* Child : ItemCanvas->GetAllChildren())
		{
			auto* Item = Cast<URogue10mInventoryItemWidget>(Child);
			if (Item && Item->GetItemInstanceId() == Id)
			{
				return Item;
			}
		}
		return nullptr;
	}

	const URogue10mItemDataAsset* LoadItem(const TCHAR* Asset) const
	{
		return LoadObject<URogue10mItemDataAsset>(nullptr,
			*FString::Printf(TEXT("/Game/DataAsset/Item/Starter/%s.%s"), Asset, Asset));
	}

	bool AddItem(const TCHAR* Asset, int32 Quantity, FIntPoint Position, FGuid& Id)
	{
		int32 Container = INDEX_NONE;
		const auto* Data = LoadItem(Asset);
		return Data && Inventory->TryAddGridItem(Data, Quantity, Container, Id)
			&& Container == 0 && Inventory->TryMoveGridItem(Container, Id, Container, Position);
	}

	void Prepare()
	{
		auto* Instance = World->GetGameInstance();
		auto* Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Progression = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		if (!Check(Profiles && Progression.IsValid() && Controller.IsValid(), TEXT("production_game_and_save_subsystems")))
		{
			Finish();
			return;
		}
		if (!bRead)
		{
			FRogue10mCharacterProfile Profile;
			if (!Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile_slot_is_empty")))
			{
				Finish();
				return;
			}
			if (!Check(Profiles->CreateProfile(TEXT("InventoryQA"), FRogue10mCharacterAppearance(), Profile),
				TEXT("fresh_isolated_profile_created")))
			{
				Results += TEXT("profile_error=") + Profiles->GetLastError().ToString() + TEXT("\n");
				Finish();
				return;
			}
		}
		const auto* Selected = Profiles->GetSelectedProfile();
		if (!Check(Profiles->GetProfileCount() == 1 && Selected, TEXT("exactly_one_selected_QA_profile")))
		{
			Finish();
			return;
		}
		const FGuid ProfileId = Selected->ProfileId;
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		if (!Check(Inventory.IsValid() && Progression->IsSaveReady() && Progression->GetAttachedProfileId() == ProfileId
			&& Inventory->GetInventoryContainers().Num() == 1
			&& Inventory->GetInventoryContainers()[0].GridSize == GridSize, TEXT("saved_character_with_single_10_by_10_grid")))
		{
			Finish();
			return;
		}
		if (bRead)
		{
			VerifyReload();
			return;
		}
		Seed();
		if (Failures)
		{
			Finish();
			return;
		}
		if (const URogue10mAppearanceCameraComponent* Camera = Character->GetAppearanceCameraComponent();
			Camera && Camera->IsThirdPersonInspectionActive())
		{
			Controller->ToggleInspectionCamera();
		}
		Later(3.5, [this] { Press(EKeys::I); Later(0.5, [this] { VerifyLayout(); }); });
	}

	void Seed()
	{
		const auto Head = ERogue10mInventorySlotType::Head;
		const auto* IronHelmet = LoadItem(TEXT("DA_Item_IronHelmet"));
		if (IronHelmet && Inventory->GetEquippedItemData(Head) != IronHelmet)
		{
			int32 Container = INDEX_NONE;
			FGuid Iron;
			Check(Inventory->TryAddGridItem(IronHelmet, 1, Container, Iron)
				&& Inventory->TryEquipGridItem(Container, Iron), TEXT("wear_iron_helmet_for_comparison"));
		}
		Check(IronHelmet && Inventory->GetEquippedItemData(Head) == IronHelmet, TEXT("iron_helmet_initially_equipped"));

		const TArray<FRogue10mInventoryGridEntry> StartingEntries = Inventory->GetInventoryContainers()[0].Entries;
		for (const auto& Existing : StartingEntries)
		{
			FRogue10mInventoryGridEntry Removed;
			Check(Inventory->RemoveGridItem(0, Existing.InstanceId, Removed), TEXT("remove_automatic_QA_starter_grid_entry"));
		}
		Check(AddItem(TEXT("DA_Item_IronLongsword"), 1, FIntPoint(0, 0), Sword), TEXT("seed_concept_sword"));
		Check(AddItem(TEXT("DA_Item_GuardianHelmet"), 1, GuardianOrigin, Guardian), TEXT("seed_concept_guardian_helmet"));
		Check(AddItem(TEXT("DA_Item_LeatherArmor"), 1, FIntPoint(5, 0), Armor), TEXT("seed_concept_armor"));
		Check(AddItem(TEXT("DA_Item_HealthPotion"), 4, FIntPoint(8, 0), Potion), TEXT("seed_concept_potion_stack"));
		Check(AddItem(TEXT("DA_Item_NoviceRing"), 1, FIntPoint(9, 0), Ring), TEXT("seed_concept_ring"));
		Check(AddItem(TEXT("DA_Item_LeatherBoots"), 1, FIntPoint(3, 3), Boots), TEXT("seed_concept_boots"));
		const int32 GoldDelta = 120 - Inventory->GetGold();
		if (GoldDelta > 0) { Inventory->AddGold(GoldDelta); }
		else if (GoldDelta < 0) { Check(Inventory->TrySpendGold(-GoldDelta), TEXT("set_concept_gold")); }
		Check(Inventory->GetGold() == 120 && Inventory->GetInventoryContainers()[0].Entries.Num() == 6,
			TEXT("concept_currency_and_six_items"));
		Results += FString::Printf(TEXT("seed_bag_weight=%.1f maximum=%.1f\n"),
			Inventory->GetTotalInventoryWeight(), Inventory->GetMaxCarryWeight());
	}

	void VerifyLayout()
	{
		Window = VisibleWidget<URogue10mInventoryWindowWidget>();
		if (!Check(Controller->IsInventoryVisible() && !Controller->IsItemWindowVisible() && Window.IsValid()
			&& Window->IsWindowOpen() && Window->GetInventoryComponent() == Inventory.Get(),
			TEXT("I_opens_actual_inventory_widget_without_equipment")))
		{
			Results += FString::Printf(TEXT("inventory_visible=%d equipment_visible=%d widget_found=%d widget_open=%d inventory_matches=%d paused=%d\n"),
				Controller->IsInventoryVisible(), Controller->IsItemWindowVisible(), Window.IsValid(),
				Window.IsValid() && Window->IsWindowOpen(),
				Window.IsValid() && Window->GetInventoryComponent() == Inventory.Get(), World->IsPaused());
			Finish();
			return;
		}
		auto* Grid = Cast<UUniformGridPanel>(Window->GetWidgetFromName(TEXT("UI_InventoryGrid")));
		ItemCanvas = Cast<UCanvasPanel>(Window->GetWidgetFromName(TEXT("UI_InventoryItemCanvas")));
		if (!Check(Grid && ItemCanvas.IsValid() && Grid->GetChildrenCount() == 100
			&& ItemCanvas->GetChildrenCount() == 6, TEXT("visible_100_cells_and_six_item_widgets")))
		{
			Finish();
			return;
		}
		const FGeometry CanvasGeometry = ItemCanvas->GetCachedGeometry();
		const FGeometry GridGeometry = Grid->GetCachedGeometry();
		const FVector2D Size = CanvasGeometry.GetLocalSize();
		Check(Size.X > 100 && FMath::IsNearlyEqual(Size.X, Size.Y, 1.0)
			&& Size.Equals(GridGeometry.GetLocalSize(), 1.0)
			&& CanvasGeometry.GetAbsolutePosition().Equals(GridGeometry.GetAbsolutePosition(), 1.0),
			TEXT("cell_and_item_layers_share_square_geometry"));
		const FVector2D CellSize = Size / 10.0;
		bool bGridCellsFillAndAlign = true;
		for (int32 Index = 0; Index < Grid->GetChildrenCount(); ++Index)
		{
			const auto* Cell = Cast<URogue10mInventoryCellWidget>(Grid->GetChildAt(Index));
			const auto* Frame = Cell ? Cell->GetWidgetFromName(TEXT("UI_InventoryCellFrame")) : nullptr;
			const FVector2D Position(Index % 10, Index / 10);
			bGridCellsFillAndAlign &= Frame
				&& Frame->GetCachedGeometry().GetLocalSize().Equals(CellSize, 1.0)
				&& Frame->GetCachedGeometry().GetAbsolutePosition().Equals(
					GridGeometry.LocalToAbsolute(Position * CellSize), 1.0);
		}
		Check(bGridCellsFillAndAlign, TEXT("all_100_painted_grid_cell_frames_fill_and_align_with_their_slots"));
		const auto* Gold = Cast<UTextBlock>(Window->GetWidgetFromName(TEXT("UI_InventoryMoneyText")));
		const auto* Weight = Cast<UTextBlock>(Window->GetWidgetFromName(TEXT("UI_InventoryWeightText")));
		Check(Gold && Weight && Gold->GetText().ToString() == TEXT("골드 120")
			&& Weight->GetText().ToString() == TEXT("무게 12.8 / 100.0 kg"),
			TEXT("footer_displays_the_current_concept_gold_and_weight"));
		CheckFootprint(Sword, FIntPoint(1, 3));
		CheckFootprint(Guardian, FIntPoint(2, 2));
		CheckFootprint(Armor, FIntPoint(2, 3));
		CheckFootprint(Potion, FIntPoint(1, 1));
		CheckFootprint(Ring, FIntPoint(1, 1));
		CheckFootprint(Boots, FIntPoint(2, 2));
		auto* HelmetTile = Tile(Guardian);
		if (!Check(HelmetTile != nullptr, TEXT("guardian_tile_available_for_comparison"))
			|| !MovePointer(HelmetTile, Point(HelmetTile)))
		{
			FScreenshotRequest::RequestScreenshot(Output / TEXT("failed-hover-layout.png"), true, false);
			Later(0.5, [this] { Finish(); });
			return;
		}
		Later(0.85, [this]
		{
			VerifyTooltip();
			FScreenshotRequest::RequestScreenshot(Output / PhotoName, true, false);
			Later(0.5, [this] { CheckFirstDrop(); });
		});
	}

	void VerifyTooltip()
	{
		auto* Root = Cast<UCanvasPanel>(Window->GetRootWidget());
		URogue10mInventoryItemTooltipWidget* Tooltip = nullptr;
		if (Root)
		{
			for (UWidget* Child : Root->GetAllChildren())
			{
				if (auto* Candidate = Cast<URogue10mInventoryItemTooltipWidget>(Child); Candidate && Candidate->IsVisible())
				{
					Tooltip = Candidate;
					break;
				}
			}
		}
		if (!Check(Tooltip && Tooltip->GetCachedGeometry().GetLocalSize().GetMin() > 1.0,
			TEXT("actual_hover_renders_comparison_widget_in_game_viewport")))
		{
			return;
		}
		const auto Text = [Tooltip](const TCHAR* Name)
		{
			const auto* Block = Cast<UTextBlock>(Tooltip->GetWidgetFromName(Name));
			return Block ? Block->GetText().ToString() : FString();
		};
		const auto* Comparison = Tooltip->GetWidgetFromName(TEXT("UI_EquippedItemPanel"));
		Check(Text(TEXT("UI_ItemNameText")) == TEXT("수호자의 투구")
			&& Text(TEXT("UI_ItemKindText")) == TEXT("에픽 · 투구")
			&& Text(TEXT("UI_ItemActionHintText")) == TEXT("우클릭 장착"),
			TEXT("hover_shows_exact_guardian_name_rarity_and_action"));
		Check(Comparison && Comparison->IsVisible() && Text(TEXT("UI_EquippedItemNameText")) == TEXT("철제 투구"),
			TEXT("hover_compares_actual_equipped_iron_helmet"));
		const auto HasStats = [Tooltip](const TCHAR* Name, const TCHAR* Defense, const TCHAR* Health)
		{
			const auto* Stats = Cast<UPanelWidget>(Tooltip->GetWidgetFromName(Name));
			const auto* DefenseRow = Stats ? Cast<UTextBlock>(Stats->GetChildAt(0)) : nullptr;
			const auto* HealthRow = Stats ? Cast<UTextBlock>(Stats->GetChildAt(1)) : nullptr;
			return DefenseRow && HealthRow && DefenseRow->GetText().ToString() == Defense
				&& HealthRow->GetText().ToString() == Health;
		};
		Check(HasStats(TEXT("UI_ItemStatsContainer"), TEXT("방어력 +10"), TEXT("최대 체력 +20")),
			TEXT("guardian_column_displays_plus_10_defense_and_plus_20_health"));
		Check(HasStats(TEXT("UI_EquippedItemStatsContainer"), TEXT("방어력 +5"), TEXT("최대 체력 +10")),
			TEXT("iron_column_displays_plus_5_defense_and_plus_10_health"));
		const FGeometry RootGeometry = Root->GetCachedGeometry();
		const FGeometry TooltipGeometry = Tooltip->GetCachedGeometry();
		Check(RootGeometry.IsUnderLocation(TooltipGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.01)))
			&& RootGeometry.IsUnderLocation(TooltipGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.99))),
			TEXT("comparison_cards_fit_inside_game_viewport"));
	}

	void CheckFootprint(FGuid Id, FIntPoint Footprint)
	{
		const auto* Record = Entry(Id);
		const auto* Item = Tile(Id);
		const FGeometry CanvasGeometry = ItemCanvas->GetCachedGeometry();
		const FVector2D Cell = CanvasGeometry.GetLocalSize() / 10.0;
		Check(Record && Item && Record->ItemData->GetClampedInventorySize() == Footprint
			&& Item->GetCachedGeometry().GetLocalSize().Equals(FVector2D(Footprint) * Cell, 1.0)
			&& Item->GetCachedGeometry().GetAbsolutePosition().Equals(
				CanvasGeometry.LocalToAbsolute(FVector2D(Record->Position) * Cell), 1.0),
			TEXT("item_data_and_rendered_footprint_are_aligned_to_cells"));
		const auto* Background = Item ? Item->GetWidgetFromName(TEXT("UI_InventoryItemRarityBackground")) : nullptr;
		Check(Background && Background->GetCachedGeometry().GetLocalSize().Equals(FVector2D(Footprint) * Cell, 1.0)
			&& Background->GetCachedGeometry().GetAbsolutePosition().Equals(Item->GetCachedGeometry().GetAbsolutePosition(), 1.0),
			TEXT("painted_item_background_fills_its_complete_footprint"));
	}

	FVector2D GridPoint(FIntPoint Position) const
	{
		const FGeometry Geometry = ItemCanvas->GetCachedGeometry();
		return Geometry.LocalToAbsolute((FVector2D(Position) + FVector2D(0.5)) * Geometry.GetLocalSize() / 10.0);
	}

	static FVector2D Point(const UWidget* Widget, FVector2D Normalized = FVector2D(0.5))
	{
		return Widget->GetCachedGeometry().GetAbsolutePositionAtCoordinates(Normalized);
	}

	static FWidgetPath HitInWindow(const TSharedRef<SWindow>& Window, FVector2D Position)
	{
		// The test drives the painted window directly because the launcher keeps
		// its native window hidden. Use the same hit grid that Slate uses for a mouse.
		return FWidgetPath(Window->GetHittestGrid().GetBubblePath(Position, 0.0f, false, 0));
	}

	bool HitPath(UWidget* Receiver, FVector2D Position, FWidgetPath& Path)
	{
		FWidgetPath ReceiverPath;
		auto& Slate = FSlateApplication::Get();
		if (Receiver && Slate.GeneratePathToWidgetUnchecked(Receiver->TakeWidget(), ReceiverPath) && ReceiverPath.IsValid())
		{
			Path = HitInWindow(ReceiverPath.GetWindow(), Position);
		}
		const bool bHit = Receiver && Path.IsValid() && Path.ContainsWidget(&Receiver->TakeWidget().Get());
		if (!bHit)
		{
			Results += FString::Printf(TEXT("hit_receiver_path=%d hit_grid_path=%d position=%s\n"),
				ReceiverPath.IsValid(), Path.IsValid(), *Position.ToString());
			Results += FString::Printf(TEXT("receiver_widgets=%s\nresolved_widgets=%s\n"),
				*ReceiverPath.ToString(), *Path.ToString());
		}
		return Check(bHit, TEXT("Slate_pointer_uses_actual_hit_path"));
	}

	bool MovePointer(UWidget* Receiver, FVector2D Position)
	{
		FWidgetPath Path;
		if (!HitPath(Receiver, Position, Path)) { return false; }
		FSlateApplication::Get().RoutePointerMoveEvent(Path,
			FPointerEvent(0, Position, Position, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		return true;
	}

	bool Click(UWidget* Receiver, FVector2D Position, const FKey& Key)
	{
		FWidgetPath Path;
		auto& Slate = FSlateApplication::Get();
		if (!HitPath(Receiver, Position, Path))
		{
			return false;
		}
		Slate.RoutePointerMoveEvent(Path,
			FPointerEvent(0, Position, Position, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		Slate.RoutePointerDownEvent(Path,
			FPointerEvent(0, Position, Position, TSet<FKey>{Key}, Key, 0, FModifierKeysState()));
		Slate.RoutePointerUpEvent(Path,
			FPointerEvent(0, Position, Position, TSet<FKey>{}, Key, 0, FModifierKeysState()));
		return true;
	}

	void CheckFirstDrop()
	{
		Check(IFileManager::Get().FileSize(*(Output / PhotoName)) > 0, TEXT("rendered_inventory_screenshot_produced"));
		DragSword(GuardianOrigin, false, [this]
		{
			Check(Entry(Sword) && Entry(Sword)->Position == FIntPoint(0, 0), TEXT("overlap_drop_preserves_sword_position"));
			DragSword(SwordDestination, true, [this]
			{
				Check(Entry(Sword) && Entry(Sword)->Position == SwordDestination, TEXT("valid_drop_moves_same_sword_GUID"));
				EquipGuardian();
			});
		});
	}

	void DragSword(FIntPoint Target, bool bCanPlace, TFunction<void()> NextStep)
	{
		const auto* SwordRecord = Entry(Sword);
		if (!Check(SwordRecord != nullptr, TEXT("drag_source_sword_still_in_bag"))) { Finish(); return; }
		auto* Item = Tile(Sword);
		FWidgetPath Source;
		const FVector2D StartPoint = GridPoint(SwordRecord->Position);
		if (!HitPath(Item, StartPoint, Source)) { Finish(); return; }
		auto& Slate = FSlateApplication::Get();
		const TSet<FKey> Pressed{EKeys::LeftMouseButton};
		Slate.RoutePointerMoveEvent(Source, FPointerEvent(0, StartPoint, StartPoint,
			TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState()), false);
		Slate.RoutePointerDownEvent(Source, FPointerEvent(0, StartPoint, StartPoint,
			Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		const FVector2D Threshold = StartPoint + FVector2D(16, 0);
		const FWidgetPath ThresholdPath = HitInWindow(Source.GetWindow(), Threshold);
		Slate.RoutePointerMoveEvent(ThresholdPath, FPointerEvent(0, Threshold, StartPoint,
			Pressed, EKeys::Invalid, 0, FModifierKeysState()), false);
		const auto ActiveDrag = Slate.GetDragDroppingContent();
		const auto UMGDrag = ActiveDrag && ActiveDrag->IsOfType<FUMGDragDropOp>()
			? StaticCastSharedPtr<FUMGDragDropOp>(ActiveDrag) : TSharedPtr<FUMGDragDropOp>();
		auto* Payload = UMGDrag ? Cast<URogue10mItemDragDropOperation>(UMGDrag->GetOperation()) : nullptr;
		if (!Check(Payload && Payload->InstanceId == Sword, TEXT("Slate_drag_has_exact_item_GUID")))
		{
			Finish();
			return;
		}
		Later(0.18, [this, Target, bCanPlace, Threshold, Payload = TWeakObjectPtr<URogue10mItemDragDropOperation>(Payload),
			NextStep = MoveTemp(NextStep)]() mutable
		{
			const FVector2D End = GridPoint(Target);
			FWidgetPath TargetPath;
			if (!HitPath(Window.Get(), End, TargetPath)) { Finish(); return; }
			auto& App = FSlateApplication::Get();
			App.RoutePointerMoveEvent(TargetPath, FPointerEvent(0, End, Threshold, TSet<FKey>{EKeys::LeftMouseButton},
				EKeys::Invalid, 0, FModifierKeysState()), false);
			Check(Payload.IsValid() && Payload->PreviewGridPosition == Target && Payload->bPreviewCanPlace == bCanPlace,
				bCanPlace ? TEXT("valid_grid_drag_preview") : TEXT("blocked_grid_drag_preview"));
			App.RoutePointerUpEvent(TargetPath, FPointerEvent(0, End, End, TSet<FKey>{},
				EKeys::LeftMouseButton, 0, FModifierKeysState()));
			Check(!App.IsDragDropping(), TEXT("drop_ends_native_drag"));
			Later(0.2, MoveTemp(NextStep));
		});
	}

	void EquipGuardian()
	{
		auto* Item = Tile(Guardian);
		if (!Check(Item != nullptr, TEXT("guardian_tile_available_for_equip"))
			|| !Click(Item, Point(Item), EKeys::RightMouseButton)) { Finish(); return; }
		const auto Head = ERogue10mInventorySlotType::Head;
		const auto* Iron = Inventory->GetInventoryContainers()[0].Entries.FindByPredicate(
			[&](const FRogue10mInventoryGridEntry& Data) { return Data.Position == GuardianOrigin; });
		Check(Inventory->GetEquippedItemData(Head) == LoadItem(TEXT("DA_Item_GuardianHelmet"))
			&& !Entry(Guardian) && Iron && Iron->ItemData == LoadItem(TEXT("DA_Item_IronHelmet")),
			TEXT("RMB_equips_guardian_and_returns_iron_to_original_slot"));
		Press(EKeys::P);
		Later(0.25, [this]
		{
			Check(Controller->IsItemWindowVisible() && Controller->IsInventoryVisible(), TEXT("P_and_I_open_together"));
			Press(EKeys::I);
			Later(0.25, [this]
			{
				Check(Controller->IsItemWindowVisible() && !Controller->IsInventoryVisible(), TEXT("closing_I_preserves_P"));
				UnequipGuardian();
			});
		});
	}

	void UnequipGuardian()
	{
		auto* Equipment = VisibleWidget<URogue10mEquipmentWindowWidget>();
		auto* HeadFrame = Equipment ? Equipment->GetWidgetFromName(TEXT("UI_HeadSlotFrame")) : nullptr;
		if (!Check(HeadFrame && Equipment->GetInventoryComponent() == Inventory.Get(),
			TEXT("P_has_actual_equipment_head_slot_for_the_current_character"))
			|| !Click(Equipment, Point(HeadFrame), EKeys::RightMouseButton))
		{
			Finish();
			return;
		}
		Later(0.2, [this]
		{
			auto* Menu = VisibleWidget<URogue10mEquipmentSlotActionWidget>();
			auto* Button = Menu ? Menu->GetWidgetFromName(TEXT("UI_UnequipButton")) : nullptr;
			if (!Check(Button != nullptr, TEXT("RMB_on_equipped_slot_opens_unequip_action"))
				|| !Click(Button, Point(Button), EKeys::LeftMouseButton))
			{
				Finish();
				return;
			}
			Check(!Inventory->GetEquippedItemData(ERogue10mInventorySlotType::Head) && Entry(Guardian),
				TEXT("unequip_action_returns_same_guardian_GUID_to_bag"));
			Press(EKeys::I);
			Later(0.25, [this]
			{
				Press(EKeys::P);
				Later(0.25, [this]
				{
					Check(Controller->IsInventoryVisible() && !Controller->IsItemWindowVisible(), TEXT("closing_P_preserves_I"));
					UsePotion();
				});
			});
		});
	}

	void UsePotion()
	{
		auto* Attributes = Character->GetRogueAttributeSet();
		auto* Item = Tile(Potion);
		if (!Check(Attributes && Item && Entry(Potion), TEXT("potion_fixture_and_attributes_are_available")))
		{
			Finish();
			return;
		}
		Attributes->SetHealth(Attributes->GetMaxHealth());
		Click(Item, Point(Item), EKeys::RightMouseButton);
		Check(Entry(Potion) && Entry(Potion)->Quantity == 4, TEXT("full_health_RMB_keeps_potion_stack"));
		const float Injured = FMath::Max(1.0f, Attributes->GetMaxHealth() - 40.0f);
		Attributes->SetHealth(Injured);
		Click(Item, Point(Item), EKeys::RightMouseButton);
		Check(Entry(Potion) && Entry(Potion)->Quantity == 3 && Attributes->GetHealth() > Injured,
			TEXT("injured_RMB_heals_and_consumes_exactly_one"));
		Press(EKeys::I);
		Later(0.25, [this]
		{
			Check(!Controller->IsAnyBlockingWindowVisible(), TEXT("I_closes_last_menu"));
			Check(Progression->SaveNow(), TEXT("normal_progression_saves_modified_layout"));
			Finish();
		});
	}

	void VerifyReload()
	{
		const auto& Entries = Inventory->GetInventoryContainers()[0].Entries;
		const auto* SwordData = LoadItem(TEXT("DA_Item_IronLongsword"));
		const auto* PotionData = LoadItem(TEXT("DA_Item_HealthPotion"));
		const auto* GuardianData = LoadItem(TEXT("DA_Item_GuardianHelmet"));
		const auto* SavedSword = Entries.FindByPredicate([SwordData](const auto& Row) { return Row.ItemData == SwordData; });
		const auto* SavedPotion = Entries.FindByPredicate([PotionData](const auto& Row) { return Row.ItemData == PotionData; });
		Check(SavedSword && SavedSword->Position == SwordDestination, TEXT("fresh_process_restores_sword_cell"));
		Check(SavedPotion && SavedPotion->Quantity == 3, TEXT("fresh_process_restores_consumed_stack"));
		Check(!Inventory->GetEquippedItemData(ERogue10mInventorySlotType::Head)
			&& Entries.ContainsByPredicate([GuardianData](const auto& Row) { return Row.ItemData == GuardianData; }),
			TEXT("fresh_process_restores_unequipped_helmet"));
		Check(Inventory->GetGold() == 120, TEXT("fresh_process_restores_gold"));
		Finish();
	}

	void Finish()
	{
		if (bFinished) { return; }
		bFinished = true;
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
		{
			FSlateApplication::Get().CancelDragDrop();
		}
		if (Controller.IsValid()) { Controller->CloseAllBlockingPanels(); }
		Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nphysical_input=not_performed\nscope=rendered_local_Slate_and_isolated_profile\n"),
			Checks, Failures);
		const bool bReportWritten = FFileHelper::SaveStringToFile(Results, *(Output / TEXT("results.txt")));
		const bool bPass = Failures == 0 && bReportWritten;
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=INVENTORY_UI_%s_%s checks=%d failures=%d report_saved=%d"),
			bRead ? TEXT("RELOAD") : TEXT("PLAY"), bPass ? TEXT("PASSED") : TEXT("FAILED"),
			Checks, Failures, bReportWritten);
		FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
	TWeakObjectPtr<URogue10mProgressionSubsystem> Progression;
	TWeakObjectPtr<URogue10mInventoryWindowWidget> Window;
	TWeakObjectPtr<UCanvasPanel> ItemCanvas;
	FGuid Sword, Guardian, Armor, Potion, Ring, Boots;
	FString Output, Results;
	TFunction<void()> Next;
	double Deadline = 0.0, NextAt = 0.0;
	int32 Checks = 0, Failures = 0;
	bool bRead = false, bFinished = false;
};

static void Run()
{
	if (GEngine && !GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
			{
				MakeShared<FRun>()->Start(*World);
				return;
			}
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("INVENTORY_UI_QA requires a standalone game world."));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}

static FAutoConsoleCommand Command(TEXT("Rogue10m.TestInventoryUI"),
	TEXT("Verify rendered native inventory layout, mouse input and isolated save state."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif
