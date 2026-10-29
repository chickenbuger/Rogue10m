// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mRunResultWidget.generated.h"

class UButton;
class UTextBlock;
class URogue10mRunCompletionComponent;

/** Event-refreshed terminal run screen. All persistence and travel remain in the run component. */
UCLASS(meta=(DisableNativeTick))
class ROGUE10M_API URogue10mRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Run")
	void RefreshResult();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void BuildTree();
	URogue10mRunCompletionComponent* GetCompletion() const;
	UFUNCTION()
	void HandleReturnToTown();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RewardText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LobbyRewardText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LobbyBalanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RunCurrencyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SaveStatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReturnText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReturnButton;
};
