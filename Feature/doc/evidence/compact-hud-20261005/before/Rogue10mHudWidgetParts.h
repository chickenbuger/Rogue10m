// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mRunHUD.h"
#include "Rogue10mHudWidgetParts.generated.h"

class URogue10mMainHUDWidget;
class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mHudPartWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD")
	void SetOwningMainHUD(URogue10mMainHUDWidget* InOwningMainHUD);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD")
	URogue10mMainHUDWidget* GetOwningMainHUD() const { return OwningMainHUD.Get(); }

protected:
	/** Designer가 비어 있을 때 CanvasPanel 기반의 간단한 파트 미리보기 구조를 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|HUD|Design")
	bool bCreatePrototypeDesignScaffold = false;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD", meta=(DisplayName="Owning Main HUD Changed"))
	void BP_OnOwningMainHUDChanged();

	virtual FText GetPrototypeDesignTitle() const;
	virtual FText GetPrototypeDesignDescription() const;
	virtual FVector2D GetPrototypeDesignSize() const;

private:
	void EnsurePrototypeDesignScaffold();

	UPROPERTY(Transient)
	TObjectPtr<URogue10mMainHUDWidget> OwningMainHUD;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mVitalBarWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Vitals")
	void SetVitalView(FText InLabel, const FRogue10mHudVitalView& InVitalView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Vitals")
	FText GetVitalLabel() const { return VitalLabel; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Vitals")
	FRogue10mHudVitalView GetVitalView() const { return VitalView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Vitals")
	TObjectPtr<UProgressBar> UI_ValueProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Vitals")
	TObjectPtr<UTextBlock> UI_ValueText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Vitals")
	bool bShowVitalLabel = false;

	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Vitals", meta=(DisplayName="Vital View Changed"))
	void BP_OnVitalViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Vitals", meta=(AllowPrivateAccess="true"))
	FText VitalLabel;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Vitals", meta=(AllowPrivateAccess="true"))
	FRogue10mHudVitalView VitalView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mProgressionWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Progression")
	void SetProgressionView(const FRogue10mHudProgressionView& InProgressionView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Progression")
	FRogue10mHudProgressionView GetProgressionView() const { return ProgressionView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Progression")
	TObjectPtr<UProgressBar> UI_ExperienceBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Progression")
	TObjectPtr<UTextBlock> UI_ExperienceText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Progression")
	TObjectPtr<UTextBlock> UI_LevelText;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Progression", meta=(DisplayName="Progression View Changed"))
	void BP_OnProgressionViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Progression", meta=(AllowPrivateAccess="true"))
	FRogue10mHudProgressionView ProgressionView;

	bool bHasProgressionView = false;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mIdentityWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Identity")
	void SetIdentityView(const FRogue10mHudIdentityView& InIdentityView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Identity")
	FRogue10mHudIdentityView GetIdentityView() const { return IdentityView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Identity")
	TObjectPtr<UImage> UI_IdentityIcon;

	/** Decorative emblem while a configured gameplay icon is missing or still streaming. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Identity")
	TObjectPtr<UImage> UI_IdentityFallbackIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Identity")
	TObjectPtr<UTextBlock> UI_MasteryText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Identity")
	TObjectPtr<UTextBlock> UI_IdentityPercentText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Identity")
	bool bUseCircularResourceGauge = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Identity", meta=(ClampMin="1.0", ClampMax="20.0"))
	float CircularGaugeThickness = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Identity")
	FLinearColor CircularGaugeColor = FLinearColor(0.14f, 0.76f, 0.48f, 1.0f);

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Identity", meta=(DisplayName="Identity View Changed"))
	void BP_OnIdentityViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Identity", meta=(AllowPrivateAccess="true"))
	FRogue10mHudIdentityView IdentityView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mMonsterInfoWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Monster")
	void SetMonsterInfoView(const FRogue10mHudMonsterInfoView& InMonsterInfoView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Monster")
	FRogue10mHudMonsterInfoView GetMonsterInfoView() const { return MonsterInfoView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Monster")
	TObjectPtr<UTextBlock> UI_MonsterNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Monster")
	TObjectPtr<UProgressBar> UI_MonsterHealthBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Monster")
	TObjectPtr<UTextBlock> UI_MonsterHealthText;


	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Monster", meta=(DisplayName="Monster Info View Changed"))
	void BP_OnMonsterInfoViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Monster", meta=(AllowPrivateAccess="true"))
	FRogue10mHudMonsterInfoView MonsterInfoView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mQuickSlotWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Quick Slot")
	void SetQuickSlotView(const FRogue10mHudQuickSlotView& InQuickSlotView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Quick Slot")
	FRogue10mHudQuickSlotView GetQuickSlotView() const { return QuickSlotView; }

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	/** Compact combat layouts can shorten the known mouse input labels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Quick Slot")
	bool bUseCompactInputLabels = false;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UTextBlock> UI_LockedText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UTextBlock> UI_KeyText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UTextBlock> UI_CooldownText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UImage> UI_CooldownShade;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UImage> UI_IconImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Quick Slot")
	TObjectPtr<UBorder> UI_SlotFrame;
	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Quick Slot", meta=(DisplayName="Quick Slot View Changed"))
	void BP_OnQuickSlotViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Quick Slot", meta=(AllowPrivateAccess="true"))
	FRogue10mHudQuickSlotView QuickSlotView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mSkillSlotPanelWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	void SetSkillSlotViews(const TArray<FRogue10mHudQuickSlotView>& Views,
		TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass);

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Skill Slots")
	TObjectPtr<UBorder> UI_SkillSlotFrame1;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Skill Slots")
	TObjectPtr<UBorder> UI_SkillSlotFrame2;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Skill Slots")
	TObjectPtr<UBorder> UI_SkillSlotFrame3;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Skill Slots")
	TObjectPtr<UBorder> UI_SkillSlotFrame4;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Skill Slots")
	TObjectPtr<UBorder> UI_SkillSlotFrame5;

private:
	void RefreshSlot(UBorder* Frame, int32 Index, const TArray<FRogue10mHudQuickSlotView>& Views,
		TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass);
};
UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mLogLineWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Log")
	void SetLogEntryView(const FRogue10mHudLogEntryView& InLogEntryView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Log")
	FRogue10mHudLogEntryView GetLogEntryView() const { return LogEntryView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Log")
	TObjectPtr<UTextBlock> UI_MessageText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Log")
	TObjectPtr<UImage> UI_ItemIconImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Log")
	TObjectPtr<UTextBlock> UI_ItemCountText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Log")
	TObjectPtr<UTextBlock> UI_ItemNameText;


	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Log", meta=(DisplayName="Log Entry View Changed"))
	void BP_OnLogEntryViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Log", meta=(AllowPrivateAccess="true"))
	FRogue10mHudLogEntryView LogEntryView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mMinimapMarkerWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Minimap")
	void SetMinimapMarkerView(const FRogue10mHudMinimapMarkerView& InMinimapMarkerView);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Minimap")
	FRogue10mHudMinimapMarkerView GetMinimapMarkerView() const { return MinimapMarkerView; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Minimap", meta=(DisplayName="Minimap Marker View Changed"))
	void BP_OnMinimapMarkerViewChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Minimap", meta=(AllowPrivateAccess="true"))
	FRogue10mHudMinimapMarkerView MinimapMarkerView;
};

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mShortcutHintWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Shortcut")
	void SetShortcutHint(FText InKeyText, FText InLabelText, bool bInActive);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Shortcut")
	FText GetKeyText() const { return KeyText; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Shortcut")
	FText GetLabelText() const { return LabelText; }

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Shortcut")
	bool IsActive() const { return bActive; }

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Rogue10m|HUD|Shortcut", meta=(DisplayName="Shortcut Hint Changed"))
	void BP_OnShortcutHintChanged();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Shortcut", meta=(AllowPrivateAccess="true"))
	FText KeyText;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Shortcut", meta=(AllowPrivateAccess="true"))
	FText LabelText;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Rogue10m|HUD|Shortcut", meta=(AllowPrivateAccess="true"))
	bool bActive = false;
};
