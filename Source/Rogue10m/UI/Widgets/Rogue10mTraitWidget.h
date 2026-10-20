// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTraitWidget.generated.h"

class ARogue10mTutorialNPC;
class ARogue10mPlayerState;
class URogue10mTownProgressionComponent;
class UBorder;
class UImage;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UTexture2D;
class UScrollBox;
class URogue10mTraitWidget;

/** A button keeps its action context; no array index is used as a trait identity. */
UCLASS()
class ROGUE10M_API URogue10mTraitActionButton : public UButton
{
	GENERATED_BODY()
public:
	URogue10mTraitActionButton(const FObjectInitializer& ObjectInitializer);
	void Configure(URogue10mTraitWidget* Owner, int32 InAction, ERogue10mTownTrait InTrait);
private:
	UFUNCTION() void HandleClicked();
	TWeakObjectPtr<URogue10mTraitWidget> OwnerWidget;
	int32 Action = 0;
	ERogue10mTownTrait Trait = ERogue10mTownTrait::Vitality;
};

USTRUCT()
struct FRogue10mTraitCardView
{
	GENERATED_BODY()
	ERogue10mTownTrait Trait = ERogue10mTownTrait::Vitality;
	UPROPERTY(Transient) TObjectPtr<URogue10mTraitActionButton> Button;
	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Name;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Rank;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Effect;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> State;
};

/** Standalone, event-driven trait service. Layout uses real UMG text and controls. */
UCLASS(Blueprintable)
class ROGUE10M_API URogue10mTraitWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	URogue10mTraitWidget(const FObjectInitializer& ObjectInitializer);
	void InitializeTraits(URogue10mTownProgressionComponent* Town, ARogue10mTutorialNPC* NPC);
	void ReleaseContext();
	void HandleAction(int32 Action, ERogue10mTownTrait Trait);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void SelectTrait(ERogue10mTownTrait Trait);
	/** -1 = all; 0..3 = Combat, Survival, Mobility, Exploration. */
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void SetCategoryFilter(int32 Category);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void RequestUpgrade();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void RequestReset();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void ConfirmReset();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void CancelReset();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void RequestClose();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void RetrySave();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Traits") void RefreshFromData();
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") int32 GetVisibleTraitCount() const { return VisibleTraits.Num(); }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") int32 GetDisplayedAvailablePoints() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") bool IsResetConfirmationVisible() const { return bResetConfirmation; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") ERogue10mTownTrait GetSelectedTrait() const { return SelectedTrait; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") FText GetDisplayedCurrentEffect() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") FText GetDisplayedNextEffect() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Traits") FText GetDisplayedStatus() const;
	const TArray<FRogue10mTraitCardView>& GetVisibleTraitCards() const { return Cards; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Traits|Appearance")
	TSoftObjectPtr<UTexture2D> TraitIconAtlasAsset;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
private:
	void EnsureNativeWidget();
	void RebuildCards();
	void RefreshDetail();
	void MoveSelection(int32 Horizontal, int32 Vertical);
	void SetMessage(const FText& Message, bool bError);
	void RefreshSaveMessage(const FText& Success);
	bool CanInteract(FText& OutReason) const;
	const URogue10mTownServiceDataAsset* GetServiceData() const;
	UTextBlock* MakeText(FName Name, int32 Size, FLinearColor Color);
	URogue10mTraitActionButton* MakeButton(FName Name, const FText& Label, int32 Action,
		ERogue10mTownTrait Trait = ERogue10mTownTrait::Vitality);
	UImage* MakeIcon(ERogue10mTownTrait Trait, float Size);
	void SetIconBrush(UImage* Icon, ERogue10mTownTrait Trait, float Size) const;
	FText FormatEffect(ERogue10mTownTrait Trait, float Bonus, bool bNext) const;
	UFUNCTION() void HandleProgressionChanged();
	TWeakObjectPtr<URogue10mTownProgressionComponent> TownComponent;
	TWeakObjectPtr<ARogue10mTutorialNPC> ServiceNPC;
	TWeakObjectPtr<ARogue10mPlayerState> BoundPlayerState;
	ERogue10mTownTrait SelectedTrait = ERogue10mTownTrait::CriticalChance;
	int32 CategoryFilter = -1;
	bool bResetConfirmation = false;
	TArray<ERogue10mTownTrait> VisibleTraits;
	UPROPERTY(Transient) TArray<FRogue10mTraitCardView> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<URogue10mTraitActionButton>> FilterButtons;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> IconAtlas;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> CardGrid;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> CardScroll;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> IdentityText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PointsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCategory;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailDescription;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailRank;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrentEffect;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NextEffect;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCondition;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RemainingText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<URogue10mTraitActionButton> SaveRetryButton;
	UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
	UPROPERTY(Transient) TObjectPtr<URogue10mTraitActionButton> UpgradeButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mTraitActionButton> ResetButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> ResetOverlay;
	UPROPERTY(Transient) TObjectPtr<UWidget> MainContent;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResetSummary;
	UPROPERTY(Transient) TObjectPtr<URogue10mTraitActionButton> ConfirmResetButton;
};
