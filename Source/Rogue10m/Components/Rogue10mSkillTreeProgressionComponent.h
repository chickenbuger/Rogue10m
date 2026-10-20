#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mSkillTreeProgressionComponent.generated.h"

class ARogue10mPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mSkillTreeProgressionChanged);

/** PlayerState owns the durable ledger. Combat retains execution and input ownership. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mSkillTreeProgressionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mSkillTreeProgressionComponent();
	/** Reflected default reference also keeps the authored catalog in cooked asset dependencies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Skill Tree") TSoftObjectPtr<URogue10mSkillTreeDataAsset> Catalog;
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Skill Tree") FRogue10mSkillTreeProgressionChanged OnChanged;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") const URogue10mSkillTreeDataAsset* GetCatalog() const;
	/** The profile format uses one project catalog; absent assets retain the native bootstrap definitions. */
	static const URogue10mSkillTreeDataAsset* GetDefaultCatalog();
	const FRogue10mSkillTreeNodeDefinition* FindNode(FName NodeId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") TArray<FRogue10mSkillTreeNodeDefinition> GetNodes(ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeTrack GetActiveTrack() const;
	static ERogue10mSkillTreeTrack ResolveTrack(ERogue10mWeaponType WeaponType) { return URogue10mSkillTreeDataAsset::ResolveTrack(WeaponType); }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetMasteryLevel(ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetMasteryExperience(ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetMasteryTotalExperience(ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetMasteryExperienceToNextLevel(ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetAvailableGrowthPoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") int32 GetSpentSkillTreePoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") bool IsNodeUnlocked(FName NodeId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeNodeState GetNodeState(FName NodeId, ERogue10mSkillTreeTrack Track) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") FName GetSelectedChoice(FName NodeId) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") FText GetNodeRequirementsText(FName NodeId) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool CanUnlockNode(FName NodeId, ERogue10mSkillTreeUnlockRoute Route, FName ChoiceId, FText& OutReason) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool TryUnlockNode(FName NodeId, ERogue10mSkillTreeUnlockRoute Route, FName ChoiceId);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeIdentity GetActiveIdentity() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") ERogue10mSkillTreeIdentity GetRememberedIdentity() const { return State.ActiveIdentity; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") FName GetActiveIdentityChoice() const;
	ERogue10mSkillTreeIdentity GetActiveIdentity(ERogue10mSkillTreeTrack Track) const;
	FName GetSelectedProperty(ERogue10mSkillTreeIdentity Identity) const;
	bool IsIdentityDeepened(ERogue10mSkillTreeIdentity Identity) const;
	bool HasResonance(ERogue10mSkillTreeTrack Track, ERogue10mSkillTreeIdentity Identity) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") bool IsIdentityLearned(ERogue10mSkillTreeIdentity Identity) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool TrySetActiveIdentity(ERogue10mSkillTreeIdentity Identity);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool TrySetActiveChoice(FName NodeId, FName ChoiceId);
	/** Removes this identity and descendants, or all paid/identity investments when None. Free mastery remains. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool Respec(ERogue10mSkillTreeIdentity Identity = ERogue10mSkillTreeIdentity::None);
	/** Refund one identity stage and every dependent stage/resonance that loses all valid prerequisite groups. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree") bool RefundNode(FName NodeId);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") bool CanChangeInvestments() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") FRogue10mEquipmentStatModifiers GetMasteryStatModifiers(ERogue10mWeaponType WeaponType) const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree") FRogue10mEquipmentStatModifiers GetTrackMasteryStatModifiers(ERogue10mSkillTreeTrack Track) const;
	FRogue10mEquipmentStatModifiers GetMasteryStatModifiers(ERogue10mSkillTreeTrack Track) const { return GetTrackMasteryStatModifiers(Track); }
	bool IsSkillManaged(const URogue10mAttackSkillData* Skill) const;
	bool IsSkillGranted(const URogue10mAttackSkillData* Skill) const;
	TArray<URogue10mAttackSkillData*> GetGrantedSkills(ERogue10mSkillTreeTrack Track) const;
	/** Caller confirms real living enemy damage and deduplicates all targets/pulses for its execution. */
	void NotifyConfirmedAttack(ERogue10mSkillTreeTrack Track);
	void RefreshAutomaticUnlocks();
	void NotifyWeaponChanged();
	void NotifySharedPointsChanged() { OnChanged.Broadcast(); }
	FRogue10mSkillTreeSnapshot Capture() const { return State; }
	bool Restore(const FRogue10mSkillTreeSnapshot& Snapshot, bool bNotify = true);
	static bool Validate(const FRogue10mSkillTreeSnapshot& Snapshot, int32 EarnedPoints, int32 TraitSpentPoints, FText* OutReason = nullptr, const URogue10mSkillTreeDataAsset* InCatalog = nullptr);
	static FRogue10mSkillTreeSnapshot MigrateLegacy(const TArray<TSoftObjectPtr<URogue10mAttackSkillData>>& Skills, const URogue10mSkillTreeDataAsset* InCatalog = nullptr);
private:
	friend class URogue10mProgressionSubsystem;
	/** The persistence transaction supplies its already validated target trait domain. */
	bool RestoreForTransaction(const FRogue10mSkillTreeSnapshot& Snapshot, bool bNotify, int32 TraitSpentPoints);
	UPROPERTY() FRogue10mSkillTreeSnapshot State;
	const ARogue10mPlayerState* GetPlayerState() const;
	bool PrerequisitesComplete(const FRogue10mSkillTreeNodeDefinition& Node) const;
	bool MasteryRequirementsComplete(const FRogue10mSkillTreeNodeDefinition& Node) const;
	void NotifyChanged(bool bIdentityChanged = false);
	bool bRefreshing = false;
};
