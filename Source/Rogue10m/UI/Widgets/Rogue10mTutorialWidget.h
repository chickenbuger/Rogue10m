// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rogue10mTutorialWidget.generated.h"

class UTextBlock;

/** 게임 입력을 가져가지 않는 작은 대화와 보급 안내 카드입니다. */
UCLASS(Blueprintable)
class ROGUE10M_API URogue10mTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDialogueView(const FText& Name, const FText& Role,
		const FText& Dialogue, const FText& Status, const FText& Action);
	void SetServiceView(const FText& Services);

	UFUNCTION(BlueprintPure, Category="Rogue10m|Tutorial")
	FText GetDisplayedDialogue() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Town")
	FText GetDisplayedServices() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void EnsureNativeWidget();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ServicesText;
};
