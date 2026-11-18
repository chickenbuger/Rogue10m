// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mCharacterLobbyWidget.generated.h"

class ARogue10mCharacterCustomizationPreviewActor;
class ARogue10mPlayerController;
class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPanelWidget;
class URogue10mCharacterCustomizationDataAsset;
class URogue10mCharacterProfileSubsystem;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class ROGUE10M_API URogue10mCharacterLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeCharacterLobby(ARogue10mPlayerController* InController);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Character Lobby")
	TObjectPtr<UImage> UI_LobbyBackgroundImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Character Lobby")
	TObjectPtr<UBorder> UI_LobbyBackdrop;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Character Lobby")
	TObjectPtr<UBorder> UI_BackgroundShade;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Rogue10m|Character Lobby")
	TObjectPtr<UBorder> UI_BottomBar;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UImage> UI_CharacterPreviewImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UPanelWidget> UI_SelectionStagePanel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UImage> UI_Slot1PreviewImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UImage> UI_Slot2PreviewImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UImage> UI_Slot3PreviewImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UPanelWidget> UI_CreationPanel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UEditableTextBox> UI_CharacterNameInput;



	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_Slot1Button;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_Slot2Button;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_Slot3Button;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_Slot1Text;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_Slot2Text;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_Slot3Text;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_NewCharacterButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_DeleteCharacterButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_EnterGameButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_CreateCharacterButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_CancelCreationButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_RandomizeButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HumanRaceButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_DwarfRaceButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_OrcRaceButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_MaleGenderButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_FemaleGenderButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HeadPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HeadNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_HeadValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_SkinPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_SkinNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_SkinValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HairPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HairNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_HairValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HairColorPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_HairColorNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_HairColorValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_FacialPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_FacialNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_FacialValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_EyePreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_EyeNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_EyeValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_BodyDetailPreviousButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UButton> UI_BodyDetailNextButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Rogue10m|Character Lobby")
	TObjectPtr<UTextBlock> UI_BodyDetailValueText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character Lobby")
	TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset> CustomizationCatalog =
		TSoftObjectPtr<URogue10mCharacterCustomizationDataAsset>(
			FSoftObjectPath(TEXT("/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Character Lobby")
	TSoftObjectPtr<UMaterialInterface> PreviewTransparencyMaterial =
		TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT(
				"/Game/Material/UI/M_CharacterPreviewTransparent.M_CharacterPreviewTransparent")));

private:
	void RefreshAll();
	void RefreshSlotButtons();
	void RefreshMode();
	void RefreshDraft();
	void RefreshPreview(const FRogue10mCharacterAppearance& Appearance);
	void RefreshStagePreviews();
	bool TryGetPreviewSlotAtScreenPosition(
		const FVector2D& ScreenPosition, int32& OutSlotIndex) const;
	void SelectSlot(int32 SlotIndex);
	void EnterSlotCharacter(int32 SlotIndex);
	void BeginCreation();
	void EndCreation();
	void CycleRequired(int32& Value, int32 Count, int32 Delta);
	void CycleOptional(int32& Value, int32 Count, int32 Delta);
	const struct FRogue10mCharacterArchetype* GetDraftArchetype() const;
	void SetStatus(const FText& Status, bool bError = false);
	void ClearDeleteConfirmation();

	UFUNCTION() void HandleSlot1Clicked();
	UFUNCTION() void HandleSlot2Clicked();
	UFUNCTION() void HandleSlot3Clicked();
	UFUNCTION() void HandleNewCharacterClicked();
	UFUNCTION() void HandleDeleteCharacterClicked();
	UFUNCTION() void HandleEnterGameClicked();
	UFUNCTION() void HandleCreateCharacterClicked();
	UFUNCTION() void HandleCancelCreationClicked();
	UFUNCTION() void HandleRandomizeClicked();
	UFUNCTION() void HandleHumanRaceClicked();
	UFUNCTION() void HandleDwarfRaceClicked();
	UFUNCTION() void HandleOrcRaceClicked();
	UFUNCTION() void HandleMaleGenderClicked();
	UFUNCTION() void HandleFemaleGenderClicked();
	UFUNCTION() void HandleHeadPreviousClicked();
	UFUNCTION() void HandleHeadNextClicked();
	UFUNCTION() void HandleSkinPreviousClicked();
	UFUNCTION() void HandleSkinNextClicked();
	UFUNCTION() void HandleHairPreviousClicked();
	UFUNCTION() void HandleHairNextClicked();
	UFUNCTION() void HandleHairColorPreviousClicked();
	UFUNCTION() void HandleHairColorNextClicked();
	UFUNCTION() void HandleFacialPreviousClicked();
	UFUNCTION() void HandleFacialNextClicked();
	UFUNCTION() void HandleEyePreviousClicked();
	UFUNCTION() void HandleEyeNextClicked();
	UFUNCTION() void HandleBodyDetailPreviousClicked();
	UFUNCTION() void HandleBodyDetailNextClicked();

	UPROPERTY(Transient)
	TObjectPtr<ARogue10mPlayerController> RogueController;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mCharacterProfileSubsystem> ProfileSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mCharacterCustomizationDataAsset> Catalog;

	UPROPERTY(Transient)
	TObjectPtr<ARogue10mCharacterCustomizationPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ARogue10mCharacterCustomizationPreviewActor>> SlotPreviewActors;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DraftPreviewMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SlotPreviewMaterials;

	TArray<FGuid> SlotProfileIds;
	FGuid SelectedProfileId;
	FGuid PendingDeleteProfileId;
	FRogue10mCharacterAppearance DraftAppearance;
	bool bCreationMode = false;

	int32 LastClickedSlotIndex = INDEX_NONE;
	double LastSlotClickTime = -1.0;

	static constexpr double CharacterEnterDoubleClickSeconds = 0.35;
};
