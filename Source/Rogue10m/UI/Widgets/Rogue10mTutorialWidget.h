// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Rogue10mTutorialWidget.generated.h"
class ARogue10mTutorialNPC;
class ARogue10mRunPortal;
class UBorder;
class UCanvasPanelSlot;
class UScaleBoxSlot;
class UTextBlock;
class UVerticalBox;
class URogue10mTutorialWidget;
UCLASS()
class ROGUE10M_API URogue10mTutorialActionButton : public UButton
{
    GENERATED_BODY()
public:
    URogue10mTutorialActionButton(const FObjectInitializer& ObjectInitializer);
    void Configure(URogue10mTutorialWidget* Owner, int32 InAction);
private:
    UFUNCTION() void HandleClicked();
    TWeakObjectPtr<URogue10mTutorialWidget> OwnerWidget;
    int32 Action = INDEX_NONE;
};
/** Proximity prompt becomes a real mouse and keyboard service card while talking. */
UCLASS(Blueprintable)
class ROGUE10M_API URogue10mTutorialWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetDialogueView(const FText& Name,const FText& Role,const FText& Dialogue,const FText& Status,const FText& Action);
    void SetServiceView(const FText& Services);
    void SetServiceChoices(ARogue10mTutorialNPC* NPC,const TArray<FText>& Labels,int32 Selected,bool bCanClaim);
    void SetDepartureChoices(ARogue10mRunPortal* Portal, bool bOpen, int32 Selected);
    void HandleAction(int32 Action);
    UFUNCTION(BlueprintPure,Category="Rogue10m|Tutorial") FText GetDisplayedDialogue() const;
    UFUNCTION(BlueprintPure,Category="Rogue10m|Town") FText GetDisplayedServices() const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& KeyEvent) override;
private:
    void EnsureNativeWidget();
    void UpdateChoiceRows(const TArray<FText>& Labels, int32 Selected, bool bOpen, bool bCanClaim, bool bShowClose);
    URogue10mTutorialActionButton* AddAction(UVerticalBox* Rows,FName Name,int32 Action,UTextBlock*& Label);
    TWeakObjectPtr<ARogue10mTutorialNPC> ServiceNPC;
    TWeakObjectPtr<ARogue10mRunPortal> DeparturePortal;
    FText FullServices;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RoleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DialogueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ServicesText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ChoiceRows;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> CardSlot;
    UPROPERTY(Transient) TObjectPtr<UScaleBoxSlot> CardContentSlot;
    UPROPERTY(Transient) TObjectPtr<UBorder> CardFrame;
    UPROPERTY(Transient) TArray<TObjectPtr<URogue10mTutorialActionButton>> ServiceButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ServiceLabels;
    UPROPERTY(Transient) TObjectPtr<URogue10mTutorialActionButton> SupplyButton;
    UPROPERTY(Transient) TObjectPtr<URogue10mTutorialActionButton> CloseButton;
};
