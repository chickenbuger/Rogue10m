#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "Rogue10mSkillBindingComponent.generated.h"

class APawn;
class UInputComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class URogue10mAttackSkillData;
class URogue10mCombatComponent;

UCLASS()
class ROGUE10M_API URogue10mSkillBindingsSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) TMap<FString, FKey> SkillKeys;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogue10mSkillBindingsChanged);

/** Local single-key shortcuts. The combat component remains the authority for execution. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mSkillBindingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogue10mSkillBindingComponent();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Keys")
	bool SetSkillKey(URogue10mAttackSkillData* Skill, FKey Key, FText& OutError);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Keys")
	FKey GetSkillKey(const URogue10mAttackSkillData* Skill) const;
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Keys")
	bool ClearSkillKey(URogue10mAttackSkillData* Skill, FText& OutError);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Keys")
	void RefreshBindings();
	void InitializeBindings(UInputComponent* Input);
	void CancelHeldInput();
	UPROPERTY(BlueprintAssignable, Category="Rogue10m|Skill Keys")
	FRogue10mSkillBindingsChanged OnSkillBindingsChanged;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	URogue10mCombatComponent* GetCombat() const;
	bool IsUsableKey(FKey Key) const;
	bool IsReservedKey(FKey Key) const;
	bool IsAllowedSkill(const URogue10mAttackSkillData* Skill) const;
	bool StoreKeys(const TMap<FString,FKey>& Keys, FText& OutError);
	FString GetSaveSlot() const;
	void RemoveOwnBindings();
	void SkillPressed(URogue10mAttackSkillData* Skill);
	void SkillReleased(URogue10mAttackSkillData* Skill);
	void SkillCanceled(URogue10mAttackSkillData* Skill);
	UFUNCTION() void HandlePawnChanged(APawn* Previous, APawn* Current);
	UFUNCTION() void ScheduleRefresh();
	UFUNCTION() void HandleContextChanged(const UInputMappingContext* Context);
	UPROPERTY(Transient) TObjectPtr<URogue10mSkillBindingsSaveGame> Settings;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundSubsystem;
	TWeakObjectPtr<URogue10mCombatComponent> BoundCombat;
	TArray<uint32> BindingHandles;
	FString CurrentLayout;
	FTimerHandle RefreshTimer;
	bool bLoaded = false;
	bool bRefreshing = false;
};
