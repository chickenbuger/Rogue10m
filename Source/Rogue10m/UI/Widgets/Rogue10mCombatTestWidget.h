#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mCombatTestWidget.generated.h"

class UComboBoxString;
class UTextBlock;
class UWidget;

UCLASS()
class ROGUE10M_API URogue10mCombatTestButton : public UButton
{
	GENERATED_BODY()
public:
	URogue10mCombatTestButton();
	void Configure(URogue10mCombatTestComponent* InTools, ERogue10mCombatTestAction InAction);
private:
	UFUNCTION()
	void Clicked();
	TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
	ERogue10mCombatTestAction Action = ERogue10mCombatTestAction::TogglePanel;
};

UCLASS()
class ROGUE10M_API URogue10mCombatTestWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeTools(URogue10mCombatTestComponent* InTools);
	void RefreshView(bool bOpen, const FText& Status, const FText& TargetSummary, bool bHasTarget, bool bBoss, bool bImmune);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	void BuildTree();
	URogue10mCombatTestButton* AddAction(class UPanelWidget* Parent, const FName Name, const TCHAR* Label,
		ERogue10mCombatTestAction Action);
	UFUNCTION()
	void CreatureSelected(FString Selection, ESelectInfo::Type SelectionType);
	TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
	UPROPERTY(Transient) TObjectPtr<UWidget> Panel;
	UPROPERTY(Transient) TObjectPtr<UWidget> Hint;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> CreaturePicker;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetText;
	UPROPERTY(Transient) TObjectPtr<UButton> ImmunityButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> TargetButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> BossButtons;
};
