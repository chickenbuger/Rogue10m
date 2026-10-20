#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"
#include "Rogue10mCombatTestComponent.generated.h"

class ARogue10mBasicMonster;
class URogue10mCombatTestWidget;
class UInputComponent;
class AController;

UENUM(BlueprintType)
enum class ERogue10mCombatTestAction : uint8
{
	TogglePanel, Spawn, Clear, Heal, ToggleAI, Basic, FrontalWave, AbyssZone, Enrage, Reset, Invulnerable
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
