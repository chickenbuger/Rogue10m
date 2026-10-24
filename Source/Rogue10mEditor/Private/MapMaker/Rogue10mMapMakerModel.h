#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMesh;
class UWorld;
class UWorldPartitionEditorLoaderAdapter;

namespace Rogue10mMapMaker
{
inline constexpr double CellSizeCm = 25600.0;
inline constexpr double WorldHalfSizeCm = 90000.0;
inline constexpr int32 MinCell = -4;
inline constexpr int32 MaxCell = 3;

struct FCell
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	FBox Bounds = FBox(ForceInit);
	int32 LoadedEnvironmentActors = 0;
	bool bLoadedByTool = false;
};

bool TryGetCell(const FVector& Position, FIntPoint& OutCoordinate);
FBox GetCellBounds(FIntPoint Coordinate);
bool IsEditableEnvironmentActor(const AActor* Actor);

/** Editor-only state. Weak handles never keep a previous map alive. No runtime streaming changes. */
class FModel
{
public:
	~FModel();
	UWorld* GetWorld(FString& OutError) const;
	FString DescribeWorld() const;
	TArray<FCell> Inspect(FString& OutError) const;
	bool LoadCell(FIntPoint Coordinate, FString& OutMessage);
	void ReleaseCells();
	bool IsCellLoaded(FIntPoint Coordinate) const;
	bool FocusCell(FIntPoint Coordinate, FString& OutMessage) const;
	bool SelectCell(FIntPoint Coordinate, const FString& LabelFilter, FString& OutMessage) const;
	bool PlaceMesh(UStaticMesh* Mesh, FIntPoint Coordinate, double HeightOffsetCm, double YawDegrees, double UniformScale, FString& OutMessage);
	bool MoveSelectionToCell(FIntPoint Coordinate, FString& OutMessage);
	bool SaveMap(FString& OutMessage) const;

private:
	TWeakObjectPtr<UWorld> LoadedWorld;
	TMap<FIntPoint, TWeakObjectPtr<UWorldPartitionEditorLoaderAdapter>> Loaders;
};
}
