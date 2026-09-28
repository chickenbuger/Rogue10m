// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Rogue10mHudWidgetParts.h"
#include "Rogue10mBottomHUDWidget.generated.h"

class UImage;
class UPanelWidget;
class UScaleBox;
class USizeBox;
class UTextBlock;

UENUM(BlueprintType)
enum class ERogue10mBottomHUDTheme : uint8
{
	Auto UMETA(DisplayName="자동"),
	Wizard UMETA(DisplayName="마법사"),
	Warrior UMETA(DisplayName="전사"),
	MartialArtist UMETA(DisplayName="권사"),
	Rogue UMETA(DisplayName="도적")
};

/**
 * 체력, 직업 자원, 아이덴티티, 스킬, 아이템, 경험치를 하나로 조립하는 하단 HUD입니다.
 * Main HUD는 이 위젯 하나만 소유하고 View 데이터를 전달합니다.
 */
UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mBottomHUDWidget : public URogue10mHudPartWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;

	void InitializeBottomHUD(URogue10mMainHUDWidget* InOwningMainHUD);

	void SetFrequentViews(
		const FRogue10mHudVitalView& HealthView,
		const FRogue10mHudVitalView& StaminaView,
		const FRogue10mHudVitalView& ManaView,
		const FRogue10mHudProgressionView& ProgressionView,
		const FRogue10mHudIdentityView& IdentityView);

	void SetSlowViews(
		const TArray<FRogue10mHudQuickSlotView>& SkillViews,
		const TArray<FRogue10mHudQuickSlotView>& ItemViews,
		TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass);

	UFUNCTION(BlueprintCallable, Category="Rogue10m|HUD|Bottom")
	void SetThemeOverride(ERogue10mBottomHUDTheme InThemeOverride);

	UFUNCTION(BlueprintPure, Category="Rogue10m|HUD|Bottom")
	ERogue10mBottomHUDTheme GetResolvedTheme() const { return ResolvedTheme; }

	/** Height reserved by Main HUD in Slate units; viewport DPI is applied by UMG. */
	float GetResponsiveLayoutHeight() const;

protected:
	virtual FText GetPrototypeDesignTitle() const override;
	virtual FText GetPrototypeDesignDescription() const override;
	virtual FVector2D GetPrototypeDesignSize() const override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Layout")
	TObjectPtr<UScaleBox> UI_CombatScaleBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Layout")
	TObjectPtr<USizeBox> UI_CombatDesignSize;

	/** Combat art and controls share one design space without horizontal distortion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Layout", meta=(ClampMin="1.0"))
	FVector2D CombatDesignSize = FVector2D(1200.0f, 208.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Layout", meta=(ClampMin="0.0"))
	float CombatSideMargin = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Layout", meta=(ClampMin="4.0"))
	float CombatBottomMargin = 56.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Layout", meta=(ClampMin="0.0"))
	float QuickSlotSpacing = 10.0f;

	/** Empty keeps the slot class supplied by Main HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Bottom")
	TSubclassOf<URogue10mQuickSlotWidget> BottomQuickSlotWidgetClass;

	/** Presentation-only fallback for skill slots whose gameplay view has no icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|HUD|Bottom")
	TArray<TObjectPtr<UTexture2D>> DefaultSkillIcons;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UTextBlock> UI_LevelText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mVitalBarWidget> HealthBarWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mVitalBarWidget> StaminaBarWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mVitalBarWidget> ManaBarWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mVitalBarWidget> IdentityBarWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mProgressionWidget> ProgressionWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<URogue10mIdentityWidget> IdentityWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UPanelWidget> SkillSlotContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UPanelWidget> ItemSlotContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UImage> UI_WizardThemeFrame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UImage> UI_WarriorThemeFrame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UImage> UI_MartialArtistThemeFrame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UImage> UI_RogueThemeFrame;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UTextBlock> UI_ThemeNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|HUD|Bottom")
	TObjectPtr<UTextBlock> UI_ResourceTypeText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|HUD|Bottom")
	ERogue10mBottomHUDTheme ThemeOverride = ERogue10mBottomHUDTheme::Auto;

private:
	void ApplyResponsiveLayout();
	void ApplyQuickSlotLayout(UPanelWidget* Container);
	ERogue10mBottomHUDTheme ResolveTheme(const FRogue10mHudIdentityView& IdentityView) const;
	void RefreshResolvedTheme();
	void ApplyThemePresentation();
	void AssignOwningMainHUDToChildren(URogue10mMainHUDWidget* InOwningMainHUD);
	void RefreshQuickSlotContainer(
		UPanelWidget* Container,
		const TArray<FRogue10mHudQuickSlotView>& Views,
		TSubclassOf<URogue10mQuickSlotWidget> InQuickSlotWidgetClass);

	UPROPERTY(Transient)
	FRogue10mHudIdentityView CachedIdentityView;

	UPROPERTY(Transient)
	ERogue10mBottomHUDTheme ResolvedTheme = ERogue10mBottomHUDTheme::MartialArtist;
};
