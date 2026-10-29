// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mProgressionSaveGame.generated.h"

/** Runtime presentation values contain no hard asset reference in the save. */
USTRUCT()
struct FRogue10mSavedItemStack
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FRogue10mItemStack Values;
	UPROPERTY(SaveGame) TSoftObjectPtr<URogue10mItemDataAsset> ItemAsset;
};

USTRUCT()
struct FRogue10mSavedEquipmentSlot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FRogue10mInventorySlot Values;
	UPROPERTY(SaveGame) TSoftObjectPtr<URogue10mItemDataAsset> ItemAsset;
};

USTRUCT()
struct FRogue10mSavedGridEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FRogue10mInventoryGridEntry Values;
	UPROPERTY(SaveGame) TSoftObjectPtr<URogue10mItemDataAsset> ItemAsset;
};

USTRUCT()
struct FRogue10mSavedInventoryContainer
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName ContainerId;
	UPROPERTY(SaveGame) FText DisplayName;
	UPROPERTY(SaveGame) FIntPoint GridSize = FIntPoint(10, 10);
	UPROPERTY(SaveGame) TSoftObjectPtr<URogue10mItemDataAsset> BagAsset;
	UPROPERTY(SaveGame) TArray<FRogue10mSavedGridEntry> Entries;
};

USTRUCT()
struct FRogue10mInventorySnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TArray<FRogue10mSavedEquipmentSlot> LeftEquipment;
	UPROPERTY(SaveGame) TArray<FRogue10mSavedEquipmentSlot> RightEquipment;
	UPROPERTY(SaveGame) TArray<FRogue10mSavedItemStack> ItemSlots;
	UPROPERTY(SaveGame) TArray<FRogue10mSavedInventoryContainer> Containers;
	UPROPERTY(SaveGame) TArray<int32> ConsumableQuickSlots;
	UPROPERTY(SaveGame) int32 Gold = 0;
	UPROPERTY(SaveGame) int32 Crystals = 0;
};

USTRUCT()
struct FRogue10mGrowthSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Level = 1;
	UPROPERTY(SaveGame) float Experience = 0.0f;
	UPROPERTY(SaveGame) float ExperienceToNextLevel = 100.0f;
	UPROPERTY(SaveGame) ERogue10mWeaponType WeaponType = ERogue10mWeaponType::Unarmed;
	UPROPERTY(SaveGame) TMap<FName, int32> SkillUses;
	UPROPERTY(SaveGame) TMap<FName, int32> MonsterDefeats;
};

USTRUCT()
struct FRogue10mCombatSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TArray<TSoftObjectPtr<URogue10mAttackSkillData>> UnlockedSkills;
	UPROPERTY(SaveGame) TMap<ERogue10mAttackInputSlot, TSoftObjectPtr<URogue10mAttackSkillData>> SkillBindings;
};

UCLASS()
class ROGUE10M_API URogue10mProgressionSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	static constexpr int32 CurrentVersion = 4;
	/** Lobby currency is earned independently of the current run level. Spent value is Town's ledger. */
	UPROPERTY(SaveGame) int32 LobbyEarnedPoints = 0;
	UPROPERTY(SaveGame) int32 HighestRunLevel = 1;
	UPROPERTY(SaveGame) int64 RewardDefeatBaseline = 0;
	/** Informational discovery only; never used as learned skills or acquisition requirements. */
	UPROPERTY(SaveGame) TSet<FName> DiscoveredNodes;
	UPROPERTY(SaveGame) FGuid RunId;
	UPROPERTY(SaveGame) FGuid LastSettledRunId;
	UPROPERTY(SaveGame) bool bRunCompleted = false;
	UPROPERTY(SaveGame) int32 SaveVersion = CurrentVersion;
	UPROPERTY(SaveGame) FGuid ProfileId;
	UPROPERTY(SaveGame) FRogue10mGrowthSnapshot Growth;
	UPROPERTY(SaveGame) FRogue10mInventorySnapshot Inventory;
	UPROPERTY(SaveGame) FRogue10mCombatSnapshot Combat;
	UPROPERTY(SaveGame) FRogue10mTownProgressionSnapshot Town;
	UPROPERTY(SaveGame) FRogue10mSkillTreeSnapshot SkillTree;
	UPROPERTY(SaveGame) TSet<FName> ClaimedRewards;
};
