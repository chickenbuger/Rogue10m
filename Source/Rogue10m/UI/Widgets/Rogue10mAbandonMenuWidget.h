// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mAbandonMenuWidget.generated.h"

class ARogue10mPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UScaleBox;
class UTextBlock;

/** Adds a settings action and a modal confirmation without replacing authored settings content. */
UCLASS(meta=(DisableNativeTick))
class ROGUE10M_API URogue10mAbandonMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMenuOpen(bool bOpen);
	void SetConfirmationOpen(bool bOpen);
	bool IsConfirmationOpen() const { return bConfirmationOpen; }
	void RefreshAvailability();
	void ShowError(const FText& Error);
	void FocusConfirmationCancel();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void BuildTree();
	ARogue10mPlayerController* GetRogueController() const;
	UFUNCTION() void HandleOpenConfirmation();
	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleCancel();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> Footer;
	UPROPERTY(Transient) TObjectPtr<UBorder> ConfirmationShade;
	UPROPERTY(Transient) TObjectPtr<UScaleBox> ConfirmationContent;
	UPROPERTY(Transient) TObjectPtr<UButton> MenuButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReasonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ErrorText;
	bool bMenuOpen = false;
	bool bConfirmationOpen = false;
	bool bHandlingPress = false;
};
