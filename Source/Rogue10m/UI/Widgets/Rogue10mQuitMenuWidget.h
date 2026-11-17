// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mQuitMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class UWidget;

/** Shared quit action and confirmation for the front end, settings, and run results. */
UCLASS(meta=(DisableNativeTick))
class ROGUE10M_API URogue10mQuitMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetMenuContextVisible(bool bVisible);
	void SetConfirmationOpen(bool bOpen, const FText& Policy = FText::GetEmpty());
	void ShowError(const FText& Error);
	void FocusCancel();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	void BuildTree();
	UFUNCTION() void HandleOpen();
	UFUNCTION() void HandleCancel();
	UFUNCTION() void HandleConfirm();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UButton> MenuButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> Shade;
	UPROPERTY(Transient) TObjectPtr<UWidget> CardContent;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PolicyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ErrorText;
	bool bContextVisible = false;
	bool bConfirmationOpen = false;
	bool bSubmitting = false;
};
