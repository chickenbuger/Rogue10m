// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mPlayerController.generated.h"

class ARogue10mBasicMonster;
class UInputMappingContext;
class URogue10mCharacterLobbyWidget;
class URogue10mDamageIndicatorWidget;
class URogue10mEquipmentWindowWidget;
class URogue10mInventoryWindowWidget;
class URogue10mMainMenuWidget;
class URogue10mMenuWindowWidget;
class URogue10mSkillTreeWindowWidget;
class URogue10mRunHUD;
class URogue10mItemDataAsset;
class UTexture2D;
class UUserWidget;
class UWorld;

struct FRogue10mRuntimeLogEntry
{
	FString Message;
	FLinearColor Color = FLinearColor::White;
	float ExpireTime = 0.0f;
	TWeakObjectPtr<UTexture2D> ItemIcon;
	int32 Quantity = 0;
};

struct FRogue10mFloatingDamageEntry
{
	TWeakObjectPtr<AActor> TargetActor;
	float DamageAmount = 0.0f;
	float StartTime = 0.0f;
	float ExpireTime = 0.0f;
};

/** 로컬 입력, UMG HUD와 플레이어 UI 상태를 관리합니다. */
UCLASS(abstract, config="Game")
class ROGUE10M_API ARogue10mPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARogue10mPlayerController();
	virtual void UpdateRotation(float DeltaTime) override;

	/** Switches the local gameplay view without moving the head camera used by combat. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Camera")
	void ToggleInspectionCamera();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void ToggleItemWindow();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void ToggleEquipment() { ToggleItemWindow(); }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void ToggleSkillTree();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void ToggleSettings();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|UI")
	void CloseAllBlockingPanels();

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Character Lobby")
	void EnterSelectedCharacter();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Character Lobby")
	bool IsCharacterLobbyVisible() const { return bCharacterLobbyVisible; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Main Menu")
	void OpenCharacterLobbyFromMainMenu();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Main Menu")
	bool IsMainMenuVisible() const { return bMainMenuVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsInventoryVisible() const { return bInventoryVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsItemWindowVisible() const { return bItemWindowVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsEquipmentVisible() const { return bItemWindowVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsSkillTreeVisible() const { return bSkillTreeVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsSettingsVisible() const { return bSettingsVisible; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|UI")
	bool IsAnyBlockingWindowVisible() const;

	/** Promotes a visible menu window to the top of the bounded menu ZOrder stack. */
	void BringMenuWindowToFront(URogue10mMenuWindowWidget* Window);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Settings")
	float GetLookSensitivityX() const { return LookSensitivityX; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Settings")
	float GetLookSensitivityY() const { return LookSensitivityY; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Settings")
	void SetLookSensitivity(float NewSensitivityX, float NewSensitivityY);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Settings")
	int32 GetCurrentFpsLimit() const { return CurrentFpsLimit; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Settings")
	void SetFpsLimit(int32 FpsValue);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void AddCombatLogMessage(const FString& Message, const FLinearColor& Color = FLinearColor::White, float Duration = 4.0f);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void AddItemAcquisitionMessage(const FString& Message, const FLinearColor& Color = FLinearColor::White, float Duration = 3.0f);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Items")
	void AddItemAcquisitionItem(const URogue10mItemDataAsset* ItemData, int32 Quantity, float Duration = 3.0f);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void AddFloatingDamageNumber(AActor* TargetActor, float DamageAmount, bool bCriticalHit = false);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Combat")
	void NotifyPlayerDamaged(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Debug")
	bool ToggleCombatLogVisible();

	UFUNCTION(BlueprintPure, Category="Rogue10m|Debug")
	bool IsCombatLogVisible() const { return bShowCombatLog; }

	UFUNCTION(BlueprintCallable, Category="Rogue10m|Quick Slot")
	bool ActivateQuickSlot(int32 SlotNumber);

	const TArray<FRogue10mRuntimeLogEntry>& GetCombatLogEntries() const { return CombatLogEntries; }
	const TArray<FRogue10mRuntimeLogEntry>& GetItemAcquisitionEntries() const { return ItemAcquisitionEntries; }
	const TArray<FRogue10mFloatingDamageEntry>& GetFloatingDamageEntries() const { return FloatingDamageEntries; }

	ARogue10mBasicMonster* FindLookedAtMonster() const;
	float GetPlayerDamageFeedbackAlpha() const;
	float GetPlayerDamageFeedbackStrength() const { return PlayerDamageFeedbackStrength; }
	FLinearColor GetAimCrossLineColor() const { return AimCrossLineColor; }
	float GetAimCrossLineLength() const { return AimCrossLineLength; }
	float GetAimCrossLineGap() const { return AimCrossLineGap; }
	float GetAimCrossLineThickness() const { return AimCrossLineThickness; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category="Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category="Rogue10m|HUD")
	TSubclassOf<URogue10mRunHUD> RunHUDClass;

	UPROPERTY(EditDefaultsOnly, Category="Rogue10m|HUD|Damage Indicator")
	TSubclassOf<URogue10mDamageIndicatorWidget> DamageIndicatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Damage Indicator", meta=(ClampMin="0.2", ClampMax="3.0"))
	float DamageIndicatorDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Damage Feedback", meta=(ClampMin="0.05", ClampMax="3.0", Units="s"))
	float PlayerDamageFeedbackDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Damage Indicator", meta=(ClampMin="0.0", ClampMax="200.0"))
	float DamageIndicatorHeightOffset = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Damage Indicator|Pool", meta=(ClampMin="0", ClampMax="64"))
	int32 InitialDamageIndicatorPoolSize = 24;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Damage Indicator|Pool", meta=(ClampMin="1", ClampMax="128"))
	int32 MaxDamageIndicatorPoolSize = 48;

	/** RunHUDClass가 비어 있으면 기본 메인 HUD Widget Blueprint를 로드합니다. */
	UPROPERTY(EditAnywhere, Category="Rogue10m|HUD")
	TSoftClassPtr<URogue10mRunHUD> DefaultRunHUDClass;

	UPROPERTY()
	TObjectPtr<URogue10mRunHUD> RunHUD;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character Lobby")
	TSubclassOf<URogue10mCharacterLobbyWidget> CharacterLobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character Lobby")
	TSoftClassPtr<URogue10mCharacterLobbyWidget> DefaultCharacterLobbyWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mCharacterLobbyWidget> CharacterLobbyWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Main Menu")
	TSubclassOf<URogue10mMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Main Menu")
	TSoftClassPtr<URogue10mMainMenuWidget> DefaultMainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Main Menu")
	TSoftObjectPtr<UWorld> GameplayMap;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mMainMenuWidget> MainMenuWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Aim")
	FLinearColor AimCrossLineColor = FLinearColor(0.72f, 0.92f, 1.0f, 0.86f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Aim", meta=(ClampMin="4.0", ClampMax="48.0"))
	float AimCrossLineLength = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Aim", meta=(ClampMin="0.0", ClampMax="32.0"))
	float AimCrossLineGap = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Aim", meta=(ClampMin="1.0", ClampMax="6.0"))
	float AimCrossLineThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSubclassOf<URogue10mInventoryWindowWidget> InventoryWindowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSubclassOf<URogue10mEquipmentWindowWidget> EquipmentWindowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSubclassOf<URogue10mSkillTreeWindowWidget> SkillTreeWindowWidgetClass;

	/** Blueprint에서 클래스가 비어 있을 때 사용할 기본 인벤토리 창입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSoftClassPtr<URogue10mInventoryWindowWidget> DefaultInventoryWindowWidgetClass;

	/** Blueprint에서 클래스가 비어 있을 때 사용할 기본 장비 창입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSoftClassPtr<URogue10mEquipmentWindowWidget> DefaultEquipmentWindowWidgetClass;

	/** Blueprint에서 클래스가 비어 있을 때 사용할 기본 스킬트리 창입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|UI|Menu Windows")
	TSoftClassPtr<URogue10mSkillTreeWindowWidget> DefaultSkillTreeWindowWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mInventoryWindowWidget> InventoryWindowWidget;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mEquipmentWindowWidget> EquipmentWindowWidget;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mSkillTreeWindowWidget> SkillTreeWindowWidget;

private:
	bool ShouldUseTouchControls() const;
	void InitializeMenuWindows();
	void ApplyMenuWindowVisibility();
	URogue10mMenuWindowWidget* GetTopmostOpenMenuWindow() const;
	void InitializeRunHUD();
	TSubclassOf<URogue10mRunHUD> ResolveRunHUDClass();
	void InitializeCharacterLobby();
	void InitializeMainMenu();
	void RefreshInputMode();
	bool IsMenuWorld() const;
	bool TravelToGameplayMap();
	void SetPanelVisible(bool& PanelState, bool bVisible, URogue10mMenuWindowWidget* ActivatedWindow);
	void SetInventoryEquipmentPanelVisible(bool& PanelState, bool bVisible, URogue10mMenuWindowWidget* ActivatedWindow);
	void HandleToggleCombatLog();
	void InitializeDamageIndicatorPool();
	URogue10mDamageIndicatorWidget* AcquireDamageIndicator();
	void ShowDamageIndicatorAtLocation(const FVector& WorldLocation, float DamageAmount, bool bCriticalHit);

	UFUNCTION(Client, Reliable)
	void ClientShowDamageIndicator(FVector WorldLocation, float DamageAmount, bool bCriticalHit);

	static constexpr int32 MenuWindowBaseZOrder = 50;
	TArray<TWeakObjectPtr<URogue10mMenuWindowWidget>> MenuWindowStack;

	bool bInventoryVisible = false;
	bool bMainMenuVisible = false;
	bool bCharacterLobbyVisible = false;
	bool bItemWindowVisible = false;
	bool bSkillTreeVisible = false;
	bool bSettingsVisible = false;
	bool bShowCombatLog = true;
	float LookSensitivityX = 1.0f;
	float LookSensitivityY = 1.0f;
	int32 CurrentFpsLimit = 120;
	float PlayerDamageFeedbackEndTime = 0.0f;
	float PlayerDamageFeedbackStrength = 0.0f;

	TArray<FRogue10mRuntimeLogEntry> CombatLogEntries;
	TArray<FRogue10mRuntimeLogEntry> ItemAcquisitionEntries;
	TArray<FRogue10mFloatingDamageEntry> FloatingDamageEntries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URogue10mDamageIndicatorWidget>> DamageIndicatorPool;
};