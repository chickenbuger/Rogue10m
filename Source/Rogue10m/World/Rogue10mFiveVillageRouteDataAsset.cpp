#include "Rogue10mFiveVillageRouteDataAsset.h"

#include "Rogue10mFiveVillageProgressModel.h"

URogue10mFiveVillageRouteDataAsset::URogue10mFiveVillageRouteDataAsset()
{
	auto AddVillage = [this](const TCHAR* Id, const FVector& Center)
	{
		FRogue10mFiveVillageLocation Location;
		Location.VillageId = FName(Id);
		Location.VillageCenter = Center;
		Villages.Add(Location);
	};
	AddVillage(TEXT("V1"), FVector(-14000.0, 8000.0, 0.0));
	AddVillage(TEXT("V2"), FVector(14000.0, 8000.0, 0.0));
	AddVillage(TEXT("V3"), FVector(-30000.0, 23000.0, 0.0));
	AddVillage(TEXT("V4"), FVector(6000.0, 23000.0, 0.0));
	AddVillage(TEXT("V5"), FVector(30000.0, 23000.0, 0.0));
}

bool URogue10mFiveVillageRouteDataAsset::ValidateDefinition(FString& OutReason) const
{
	OutReason.Reset();
	if (Villages.Num() != Rogue10mFiveVillage::FProgressModel::VillageCount)
	{
		OutReason = TEXT("The five-village route requires exactly five definitions.");
		return false;
	}
	TSet<FName> Seen;
	for (const FRogue10mFiveVillageLocation& Village : Villages)
	{
		if (Village.VillageId.IsNone() || Seen.Contains(Village.VillageId)
			|| Village.VillageCenter.ContainsNaN() || !FMath::IsFinite(Village.RadiusCm) || Village.RadiusCm <= 0.0f
			|| !FMath::IsFinite(Village.HeightToleranceCm) || Village.HeightToleranceCm < 0.0f)
		{
			OutReason = TEXT("Village IDs must be unique; positions and positive radii must be finite.");
			return false;
		}
		Seen.Add(Village.VillageId);
	}
	if (BossApproachLocation.ContainsNaN() || !FMath::IsFinite(BossApproachRadiusCm) || BossApproachRadiusCm <= 0.0f
		|| !FMath::IsFinite(BossHeightToleranceCm) || BossHeightToleranceCm < 0.0f)
	{
		OutReason = TEXT("Boss approach position/radius/tolerance must be finite and valid.");
		return false;
	}
	return true;
}
