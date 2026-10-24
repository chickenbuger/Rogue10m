#pragma once

#include "CoreMinimal.h"
#include "MapMaker/Rogue10mMapMakerModel.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class UStaticMesh;

/** A World Partition editing workspace, separate from runtime streaming cell identity. */
class SRogue10mMapMaker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRogue10mMapMaker) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRogue10mMapMaker() override;

private:
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Content) const;
	TSharedRef<SWidget> MakeGrid();
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& Tooltip, TFunction<FReply()> Action);
	FReply RunAction(TFunction<bool(FString&)> Action);
	void Refresh();
	void OnMapChanged(uint32 Flags);
	void OnPIEChanged(bool bSimulating);
	bool CanEdit() const;
	FText GetCellText(FIntPoint Coordinate) const;
	FSlateColor GetCellColor(FIntPoint Coordinate) const;
	FText GetSelectionText() const;
	FReply UseContentBrowserMesh();
	FReply OpenNativeWorldPartition();
	FReply OpenLandscape();

	Rogue10mMapMaker::FModel Model;
	TArray<Rogue10mMapMaker::FCell> Cells;
	FIntPoint SelectedCell = FIntPoint(0, -3);
	TWeakObjectPtr<UStaticMesh> PaletteMesh;
	TSharedPtr<SEditableTextBox> LabelFilter;
	FString WorldDescription;
	FString Message;
	bool bLastActionSucceeded = true;
	double HeightOffsetCm = 0.0;
	double YawDegrees = 0.0;
	double UniformScale = 1.0;
};
