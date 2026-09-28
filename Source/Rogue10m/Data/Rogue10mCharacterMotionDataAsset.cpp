// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterMotionDataAsset.h"

FPrimaryAssetId URogue10mCharacterMotionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(
		TEXT("Rogue10mCharacterMotion"), MotionSetId.IsNone() ? GetFName() : MotionSetId);
}
