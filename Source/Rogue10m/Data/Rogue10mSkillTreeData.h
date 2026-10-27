#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mCharacterStats.h"
#include "Rogue10mWeaponTypes.h"
#include "Rogue10mProgressionRequirementTypes.h"
#include "Rogue10mSkillTreeData.generated.h"

class URogue10mAttackSkillData;

UENUM(BlueprintType)
enum class ERogue10mSkillTreeTrack : uint8 { None, Brawler, GreatSword };

/** Progression identities are independent of the legacy HUD resource enum. */
UENUM(BlueprintType)
enum class ERogue10mSkillTreeIdentity : uint8 { None, InternalEnergy, FightingSpirit, Mana };

UENUM(BlueprintType)
enum class ERogue10mHybridSkillTreeNodeType : uint8
{
	Root, Basic, Stat, Technique, Passive, IdentityEntry, IdentityAdvanced, IdentityChoice, Resonance
};

UENUM(BlueprintType)
enum class ERogue10mSkillTreeUnlockRoute : uint8 { Automatic, Mastery, Points, Legacy };

UENUM(BlueprintType)
enum class ERogue10mSkillTreeNodeState : uint8 { Locked, Available, Learned, Active };

USTRUCT(BlueprintType)
struct FRogue10mSkillTreePrerequisiteGroup
{
	GENERATED_BODY()
	/** Every ID in a group is required; one complete group is sufficient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	TArray<FName> NodeIds;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeMasteryRequirement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree")
	ERogue10mSkillTreeTrack Track = ERogue10mSkillTreeTrack::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="1"))
	int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="0"))
	float AttackBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="0"))
	float StaminaBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeChoiceDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FName ChoiceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(MultiLine="true")) FText Description;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeNodeDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FName NodeId;
	/** None denotes a shared identity node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeTrack Track = ERogue10mSkillTreeTrack::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<ERogue10mSkillTreeTrack> AllowedTracks;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mHybridSkillTreeNodeType NodeType = ERogue10mHybridSkillTreeNodeType::Stat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(MultiLine="true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") int32 Ring = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreePrerequisiteGroup> PrerequisiteGroups;
	/** Mandatory AND gate for every acquisition route. Empty preserves the legacy catalog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mProgressionRequirement> RequiredAll;
	/** True only for free roots, basics, mastery rewards and resonance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") bool bAutomaticUnlock = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="0")) int32 PointCost = 0;
	/** Alternatives are OR; level and source-specific stat requirements inside one row are AND. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeMasteryRequirement> MasteryAlternatives;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FRogue10mEquipmentStatModifiers StatReward;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TSoftObjectPtr<URogue10mAttackSkillData> Skill;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<TSoftObjectPtr<URogue10mAttackSkillData>> GrantedSkills;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeIdentity Identity = ERogue10mSkillTreeIdentity::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeChoiceDefinition> Choices;
	/** Resonances may require a particular previously selected nature. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FName RequiredChoiceId;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeMasteryRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeTrack Track = ERogue10mSkillTreeTrack::None;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") int32 TotalExperience = 0;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeInvestment
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FName NodeId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeUnlockRoute Route = ERogue10mSkillTreeUnlockRoute::Automatic;
	/** Historical actual purchase price, independent of later catalog tuning. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") int32 SpentPoints = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") FName ChoiceId;
};

USTRUCT(BlueprintType)
struct FRogue10mSkillTreeSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeMasteryRecord> Masteries;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeInvestment> Investments;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<TSoftObjectPtr<URogue10mAttackSkillData>> LegacyGrantedSkills;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeIdentity ActiveIdentity = ERogue10mSkillTreeIdentity::None;
};

/** Shared content eligibility; town/authority/choice and ledger checks remain command gates. */
USTRUCT(BlueprintType)
struct FRogue10mSkillTreeEligibility
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") FRogue10mProgressionRequirementEvaluation RequiredAll;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bPrerequisitesSatisfied = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bPointsRouteSatisfied = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bMasteryRouteSatisfied = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bAutomaticRouteSatisfied = false;
	bool CanAcquire(ERogue10mSkillTreeUnlockRoute Route) const
	{
		if (!bValid || !RequiredAll.bSatisfied || !bPrerequisitesSatisfied) { return false; }
		switch (Route)
		{
		case ERogue10mSkillTreeUnlockRoute::Points: return bPointsRouteSatisfied;
		case ERogue10mSkillTreeUnlockRoute::Mastery: return bMasteryRouteSatisfied;
		case ERogue10mSkillTreeUnlockRoute::Automatic: return bAutomaticRouteSatisfied;
		default: return false;
		}
	}
};

UCLASS(BlueprintType)
class ROGUE10M_API URogue10mSkillTreeDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	URogue10mSkillTreeDataAsset();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeNodeDefinition> Nodes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree", meta=(ClampMin="2", ClampMax="1000")) int32 MaxMasteryLevel = 20;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float IdentityGaugePerHit = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="1")) float IdentityGaugeThreshold = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float IdentityProcCooldown = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float InternalEnergyBaseBonus = 0.08f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float InternalEnergyStaminaScaling = 0.002f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float FightingSpiritBaseBonus = 0.10f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float FightingSpiritAttackScaling = 0.01f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float ManaBaseBonus = 0.10f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float ManaProcCost = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float IdentityAdvancedGaugeBonus = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float IdentityNatureDamageBonus = 0.15f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="1")) float IdentityRangeMultiplier = 1.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0", ClampMax="1")) float CirculationCostMultiplier = 0.75f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0", ClampMax="1")) float FocusRecoveryMultiplier = 0.75f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Identity", meta=(ClampMin="0")) float ResonanceDamageBonus = 0.10f;
	const FRogue10mSkillTreeNodeDefinition* FindNode(FName NodeId) const;
	TArray<FRogue10mSkillTreeNodeDefinition> GetNodes(ERogue10mSkillTreeTrack Track) const;
	bool IsValidCatalog(FText* OutReason = nullptr) const;
	static ERogue10mSkillTreeTrack ResolveTrack(ERogue10mWeaponType WeaponType);
	bool IsIdentityAllowed(ERogue10mSkillTreeTrack Track, ERogue10mSkillTreeIdentity Identity) const;
	static bool IsNodeInTrack(const FRogue10mSkillTreeNodeDefinition& Node, ERogue10mSkillTreeTrack Track);
	/** Implemented by the project catalog translation unit. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Rogue10m|Skill Tree")
	void BuildDefaultCatalog();
};
