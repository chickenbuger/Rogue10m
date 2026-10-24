#include "MapMaker/SRogue10mMapMaker.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Engine/StaticMesh.h"
#include "Framework/Docking/TabManager.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Rogue10mMapMaker"

void SRogue10mMapMaker::Construct(const FArguments& InArgs)
{
	FEditorDelegates::MapChange.AddSP(this, &SRogue10mMapMaker::OnMapChanged);
	FEditorDelegates::PostUndoRedo.AddSP(this, &SRogue10mMapMaker::Refresh);
	FEditorDelegates::BeginPIE.AddSP(this, &SRogue10mMapMaker::OnPIEChanged);
	FEditorDelegates::ShutdownPIE.AddSP(this, &SRogue10mMapMaker::OnPIEChanged);
	Message = TEXT("Open OpenRunMap, choose a work area, then Load + focus. No map is saved automatically.");
	Refresh();

	const TSharedRef<SWrapBox> AreaActions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
	AreaActions->AddSlot()[MakeButton(LOCTEXT("LoadFocus", "Load + focus"),
		LOCTEXT("LoadFocusTip", "Load this work area through native World Partition and focus the viewport."), [this]
		{
			return RunAction([this](FString& Out)
			{
				if (!Model.LoadCell(SelectedCell, Out)) { return false; }
				return Model.FocusCell(SelectedCell, Out);
			});
		})];
	AreaActions->AddSlot()[MakeButton(LOCTEXT("Focus", "Focus"), LOCTEXT("FocusTip", "Frame the selected work area."), [this]
		{ return RunAction([this](FString& Out) { return Model.FocusCell(SelectedCell, Out); }); })];
	AreaActions->AddSlot()[MakeButton(LOCTEXT("Release", "Release tool areas"),
		LOCTEXT("ReleaseTip", "Release only regions loaded by this panel. Other editor regions remain loaded."), [this]
		{
			Model.ReleaseCells();
			Message = TEXT("Released this panel's regions. Other editor loaders are unchanged.");
			bLastActionSucceeded = true;
			Refresh();
			return FReply::Handled();
		})];

	const TSharedRef<SVerticalBox> SelectionContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[SAssignNew(LabelFilter, SEditableTextBox)
			.HintText(LOCTEXT("FilterHint", "Filter environment actor labels (optional)"))]
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f))
			+ SWrapBox::Slot()[MakeButton(LOCTEXT("SelectEnvironment", "Select environment in area"),
				LOCTEXT("SelectTip", "Select loaded, project-owned static mesh environment actors matching the label filter."), [this]
				{
					return RunAction([this](FString& Out)
					{ return Model.SelectCell(SelectedCell, LabelFilter->GetText().ToString(), Out); });
				})]
			+ SWrapBox::Slot()[MakeButton(LOCTEXT("MoveSelection", "Move selection here"),
				LOCTEXT("MoveTip", "Move the selected environment group to this area's center while preserving relative offsets and height. Undo is supported."), [this]
				{ return RunAction([this](FString& Out) { return Model.MoveSelectionToCell(SelectedCell, Out); }); })]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Protected", "Loaded actors only. Terrain, gameplay systems, boundary actors and the boss floor are protected. Edit selected props in the standard Details panel; Ctrl+Z / Ctrl+Y undo and redo."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())];

	const TSharedRef<SVerticalBox> PlacementContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[MakeButton(LOCTEXT("UseAsset", "Use selected mesh"),
				LOCTEXT("UseAssetTip", "Choose exactly one Static Mesh in the Content Browser, then click here."),
				[this] { return UseContentBrowserMesh(); })]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8, 0, 0, 0)
			[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
				{ return PaletteMesh.IsValid() ? FText::FromString(PaletteMesh->GetName()) : LOCTEXT("NoMesh", "No mesh selected"); })]];

	const auto AddNumber = [&PlacementContent](const FText& Label, double* Value, double Min, double Max, double Delta)
	{
		PlacementContent->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(SBox).WidthOverride(112)[SNew(STextBlock).Text(Label)]]
			+ SHorizontalBox::Slot().FillWidth(1)
			[SNew(SSpinBox<double>).MinValue(Min).MaxValue(Max).Delta(Delta)
				.Value_Lambda([Value] { return *Value; })
				.OnValueChanged_Lambda([Value](double InValue) { *Value = InValue; })]];
	};
	AddNumber(LOCTEXT("HeightOffset", "Height offset (cm)"), &HeightOffsetCm, -10000.0, 10000.0, 10.0);
	AddNumber(LOCTEXT("Yaw", "Rotation (degrees)"), &YawDegrees, -360.0, 360.0, 15.0);
	AddNumber(LOCTEXT("Scale", "Uniform scale"), &UniformScale, 0.01, 100.0, 0.1);
	PlacementContent->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
	[SNew(SButton).ButtonStyle(FAppStyle::Get(), "PrimaryButton")
		.Text(LOCTEXT("PlaceMesh", "Place mesh at area center"))
		.ToolTipText(LOCTEXT("PlaceTip", "Place the chosen mesh on loaded Landscape collision at the selected area's center, with the specified offset, rotation and scale."))
		.IsEnabled_Lambda([this] { return CanEdit() && PaletteMesh.IsValid(); })
		.OnClicked_Lambda([this]
		{
			return RunAction([this](FString& Out)
			{ return Model.PlaceMesh(PaletteMesh.Get(), SelectedCell, HeightOffsetCm, YawDegrees, UniformScale, Out); });
		})];

	ChildSlot
	[SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[SNew(SScrollBox)
			+ SScrollBox::Slot()
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Text(LOCTEXT("Title", "WORLD MAP MAKER"))
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)
				[SNew(STextBlock).Text(LOCTEXT("Subtitle", "OpenRunMap  /  World Partition workspace"))
					.ColorAndOpacity(FLinearColor(0.42f, 0.72f, 0.77f))]
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(WorldDescription); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[MakeSection(LOCTEXT("WorkArea", "01   WORK AREA"), SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).AutoWrapText(true)
						.Text(LOCTEXT("GridLegend", "NORTH (+Y)  |  256m editing grid  |  cyan = tool-loaded\nNumbers count loaded environment actors. Work areas are not runtime cell IDs."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 8)[MakeGrid()]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[SNew(STextBlock).AutoWrapText(true).Text(this, &SRogue10mMapMaker::GetSelectionText)]
					+ SVerticalBox::Slot().AutoHeight()[AreaActions])]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[MakeSection(LOCTEXT("SelectionSection", "02   EDIT ENVIRONMENT"), SelectionContent)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[MakeSection(LOCTEXT("PlacementSection", "03   PLACE FROM CONTENT BROWSER"), PlacementContent)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[MakeSection(LOCTEXT("NativeTools", "04   TERRAIN & STREAMING"),
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
					+ SWrapBox::Slot()[MakeButton(LOCTEXT("Landscape", "Landscape tools"),
						LOCTEXT("LandscapeTip", "Switch to Unreal's native Landscape editing mode."), [this] { return OpenLandscape(); })]
					+ SWrapBox::Slot()[MakeButton(LOCTEXT("WP", "World Partition"),
						LOCTEXT("WPTip", "Open Unreal's native World Partition editor for advanced region management."), [this] { return OpenNativeWorldPartition(); })])]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
				[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
					+ SWrapBox::Slot()[SNew(SButton).Text(LOCTEXT("Refresh", "Refresh"))
						.OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })]
					+ SWrapBox::Slot()[MakeButton(LOCTEXT("SaveMap", "Save map + external actors"),
						LOCTEXT("SaveTip", "Save all changes in the current OpenRunMap and its external actor packages. This includes edits made outside this panel. Other maps and content are not saved."), [this]
						{ return RunAction([this](FString& Out) { return Model.SaveMap(Out); }); })]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[SNew(SBorder).Padding(8).BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Message); })
						.ColorAndOpacity_Lambda([this]
						{ return FSlateColor(bLastActionSucceeded ? FLinearColor(0.65f, 0.82f, 0.72f) : FLinearColor(1.0f, 0.55f, 0.35f)); })]]
			]]];
}

SRogue10mMapMaker::~SRogue10mMapMaker()
{
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::PostUndoRedo.RemoveAll(this);
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::ShutdownPIE.RemoveAll(this);
	Model.ReleaseCells();
}

TSharedRef<SWidget> SRogue10mMapMaker::MakeSection(const FText& Title, const TSharedRef<SWidget>& Content) const
{
	return SNew(SBorder).Padding(10).BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
		[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("SmallFontBold"))]
			+ SVerticalBox::Slot().AutoHeight()[Content]];
}

TSharedRef<SWidget> SRogue10mMapMaker::MakeGrid()
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(1.5f));
	for (int32 Y = Rogue10mMapMaker::MaxCell; Y >= Rogue10mMapMaker::MinCell; --Y)
	{
		for (int32 X = Rogue10mMapMaker::MinCell; X <= Rogue10mMapMaker::MaxCell; ++X)
		{
			const FIntPoint Coordinate(X, Y);
			Grid->AddSlot(X - Rogue10mMapMaker::MinCell, Rogue10mMapMaker::MaxCell - Y)
			[SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center).ContentPadding(FMargin(2))
				.ButtonColorAndOpacity(this, &SRogue10mMapMaker::GetCellColor, Coordinate)
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Work area (%d, %d). Click to choose; Load + focus loads it."), X, Y)))
				.OnClicked_Lambda([this, Coordinate] { SelectedCell = Coordinate; return FReply::Handled(); })
				[SNew(SBox).MinDesiredWidth(34).HeightOverride(38)
					[SNew(STextBlock).Justification(ETextJustify::Center)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(this, &SRogue10mMapMaker::GetCellText, Coordinate)]]];
		}
	}
	return Grid;
}

TSharedRef<SWidget> SRogue10mMapMaker::MakeButton(const FText& Label, const FText& Tooltip, TFunction<FReply()> Action)
{
	return SNew(SButton).Text(Label).ToolTipText(Tooltip)
		.IsEnabled(this, &SRogue10mMapMaker::CanEdit)
		.OnClicked_Lambda([Action = MoveTemp(Action)] { return Action(); });
}

FReply SRogue10mMapMaker::RunAction(TFunction<bool(FString&)> Action)
{
	bLastActionSucceeded = Action(Message);
	Refresh();
	return FReply::Handled();
}

void SRogue10mMapMaker::Refresh()
{
	WorldDescription = Model.DescribeWorld();
	FString Error;
	Cells = Model.Inspect(Error);
}

void SRogue10mMapMaker::OnMapChanged(uint32 Flags)
{
	if (Flags & (MapChangeEventFlags::NewMap | MapChangeEventFlags::WorldTornDown))
	{
		Model.ReleaseCells();
		Message = TEXT("Map changed. Tool-owned regions were released; choose a work area again.");
		bLastActionSucceeded = true;
	}
	Refresh();
}

void SRogue10mMapMaker::OnPIEChanged(bool bSimulating)
{
	Refresh();
}

bool SRogue10mMapMaker::CanEdit() const
{
	FString Error;
	return Model.GetWorld(Error) != nullptr;
}

FText SRogue10mMapMaker::GetCellText(FIntPoint Coordinate) const
{
	const Rogue10mMapMaker::FCell* Cell = Cells.FindByPredicate([Coordinate](const auto& Entry)
		{ return Entry.Coordinate == Coordinate; });
	return FText::FromString(FString::Printf(TEXT("%d,%d\n%d"), Coordinate.X, Coordinate.Y,
		Cell ? Cell->LoadedEnvironmentActors : 0));
}

FSlateColor SRogue10mMapMaker::GetCellColor(FIntPoint Coordinate) const
{
	if (Coordinate == SelectedCell)
	{
		return FLinearColor(0.80f, 0.56f, 0.19f);
	}
	return Model.IsCellLoaded(Coordinate) ? FLinearColor(0.12f, 0.48f, 0.58f) : FLinearColor(0.18f, 0.21f, 0.24f);
}

FText SRogue10mMapMaker::GetSelectionText() const
{
	const FBox Bounds = Rogue10mMapMaker::GetCellBounds(SelectedCell);
	return FText::FromString(FString::Printf(TEXT("Selected (%d, %d)  |  X %.0f to %.0fm  /  Y %.0f to %.0fm%s"),
		SelectedCell.X, SelectedCell.Y, Bounds.Min.X / 100.0, Bounds.Max.X / 100.0,
		Bounds.Min.Y / 100.0, Bounds.Max.Y / 100.0,
		Model.IsCellLoaded(SelectedCell) ? TEXT("  |  loaded by tool") : TEXT("")));
}

FReply SRogue10mMapMaker::UseContentBrowserMesh()
{
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Assets);
	if (Assets.Num() != 1 || !Assets[0].IsInstanceOf<UStaticMesh>())
	{
		bLastActionSucceeded = false;
		Message = TEXT("Select exactly one Static Mesh asset in the Content Browser, then click Use selected mesh.");
		return FReply::Handled();
	}
	PaletteMesh = Cast<UStaticMesh>(Assets[0].GetAsset());
	bLastActionSucceeded = PaletteMesh.IsValid();
	Message = PaletteMesh.IsValid() ? FString::Printf(TEXT("Palette: %s. The source asset will be reused."), *PaletteMesh->GetPathName())
		: TEXT("The selected mesh could not be loaded.");
	return FReply::Handled();
}

FReply SRogue10mMapMaker::OpenNativeWorldPartition()
{
	TSharedPtr<FTabManager> Manager = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorTabManager();
	if (Manager.IsValid() && Manager->TryInvokeTab(FName(TEXT("WorldBrowserPartitionEditor"))))
	{
		Message = TEXT("Opened native World Partition. Its editor regions are independent from this panel.");
		bLastActionSucceeded = true;
	}
	else
	{
		Message = TEXT("World Partition tab is unavailable. Use Window > World Partition > World Partition Editor.");
		bLastActionSucceeded = false;
	}
	return FReply::Handled();
}

FReply SRogue10mMapMaker::OpenLandscape()
{
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
	Message = TEXT("Landscape mode is active. Use its native sculpt and paint tools; save when finished.");
	bLastActionSucceeded = true;
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
