#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"
#include "Rogue10mAttackSkillData.h"
#include "TimerManager.h"
#include "Rogue10mCombatTestComponent.generated.h"

class ARogue10mBasicMonster;
class URogue10mCombatTestWidget;
class UInputComponent;
class AController;
class ULineBatchComponent;
class UPrimitiveComponent;
class UGameViewportClient;
enum class ERogue10mRunPhase : uint8;

UENUM(BlueprintType)
enum class ERogue10mCombatTestAction : uint8
{
	TogglePanel, Spawn, Clear, Heal, ToggleAI, Basic, FrontalWave, AbyssZone, Enrage, Reset, Invulnerable, ToggleSpatialDebug, ToggleCollisionReference
};

USTRUCT(BlueprintType)
struct FRogue10mCombatTestCreature
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText Name;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<ARogue10mBasicMonster> MonsterClass;
};

/** Read-only capture of the actual executed overlap or a later locked-target pulse. */
struct FRogue10mAttackDebugQuery
{
    FVector Origin=FVector::ZeroVector, Forward=FVector::ForwardVector, Center=FVector::ZeroVector, Extent=FVector::ZeroVector;
    FQuat Rotation=FQuat::Identity;
    ERogue10mAttackShape Shape=ERogue10mAttackShape::LinearBox;
    float Radius=0.f, ArcAngleDegrees=0.f;
    uint32 ExecutionId=0;
    int32 Pulse=0;
    bool bLockedTargetReuse=false;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> OverlapComponents;
    TArray<TWeakObjectPtr<AActor>> EligibleTargets;
};

/** Local development tools. Never enabled in Shipping or on a remote client. */
UCLASS()
class ROGUE10M_API URogue10mCombatTestComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mCombatTestComponent();
	void InitializeUI();
	void BindInput(UInputComponent* Input);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test")
	void TogglePanel();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test")
	void SetPanelOpen(bool bOpen);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test")
	bool ExecuteAction(ERogue10mCombatTestAction Action);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test")
	void SelectCreature(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test") bool SetSpatialDebugEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test") bool IsSpatialDebugEnabled() const;
    UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test") FText GetSpatialDebugSummary() const;
    UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat Test") bool SetCollisionReferenceEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test") bool IsCollisionReferenceEnabled() const;
    int32 GetLiveSkeletalShapeCount() const { return LiveSkeletalShapeCount; }
    int32 GetUnresolvedColliderCount() const { return UnresolvedColliderCount; }
    bool TryGetFirstLiveSkeletalShapeCenter(FVector& OutCenter) const;
    void RecordAttackQuery(const FRogue10mAttackDebugQuery& Query);
    void RecordAcceptedTarget(AActor& Actor, uint32 ExecutionId, int32 Pulse, bool bLockedReuse);
    const FRogue10mAttackDebugQuery& GetLastAttackQuery() const { return LastAttackQuery; }
    int32 GetRecordedQueryCount() const { return RecordedQueryCount; }
    int32 GetAcceptedDebugTargetCount() const { return AcceptedTargets.Num(); }
    int32 GetUnsupportedColliderCount() const { return UnsupportedColliderCount; }
    int32 GetSupportedColliderCount() const { return SupportedColliderCount; }
    int32 GetSpatialDebugLineCount() const;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test")
	bool IsPanelOpen() const { return bPanelOpen; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test")
	ARogue10mBasicMonster* GetTestTarget() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test")
	int32 GetTestMonsterCount() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Combat Test")
	FText GetStatus() const { return Status; }
	URogue10mCombatTestWidget* GetWidget() const { return Widget; }
	const TArray<FRogue10mCombatTestCreature>& GetCreatures() const { return Creatures; }
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat Test")
	TArray<FRogue10mCombatTestCreature> Creatures;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Combat Test", meta=(ClampMin="250", ClampMax="1000", Units="cm"))
	float SpawnDistance = 600.0f;
private:
	bool IsAvailable() const;
    void RefreshSpatialDebug();
    bool DrawQueryCollider(UPrimitiveComponent& Primitive, const FLinearColor& Color);
    UFUNCTION() void HandleSpatialRunPhaseChanged(ERogue10mRunPhase Phase);
    UPROPERTY(Transient) TObjectPtr<ULineBatchComponent> SpatialLines;
    TWeakObjectPtr<UGameViewportClient> CollisionReferenceViewport;
    FVector FirstLiveSkeletalShapeCenter=FVector::ZeroVector;
    FRogue10mAttackDebugQuery LastAttackQuery;
    TArray<TWeakObjectPtr<AActor>> AcceptedTargets;
    FTimerHandle SpatialDebugTimer;
    float LastQueryTime=-1.f;
    int32 RecordedQueryCount=0, SupportedColliderCount=0, UnsupportedColliderCount=0, LiveSkeletalShapeCount=0, UnresolvedColliderCount=0;
    bool bSpatialDebugEnabled=false, bOwnCollisionReference=false, bSavedViewportCollision=false, bHasFirstLiveSkeletalShape=false;

	bool SpawnMonster();
	bool FindPlacement(UClass* MonsterClass, float Distance, AActor* IgnoredActor, FTransform& OutTransform) const;
	bool StartPattern(ERogue10mMonsterAttackType Type);
	void ClearMonsters();
	void SetStatus(const FString& Message);
	void RefreshView();
	void RestoreDamageFlag();
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	UPROPERTY(Transient)
	TObjectPtr<URogue10mCombatTestWidget> Widget;
	TArray<TWeakObjectPtr<ARogue10mBasicMonster>> TestMonsters;
	TArray<TWeakObjectPtr<AController>> TestControllers;
	TWeakObjectPtr<ARogue10mBasicMonster> Target;
	TWeakObjectPtr<APawn> ProtectedPawn;
	FText Status;
	int32 SelectedIndex = 5;
	bool bPanelOpen = false;
	bool bOwnsPause = false;
	bool bTargetAI = false;
	bool bSavedCanBeDamaged = true;
};
