#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Framework/Commands/InputChord.h"
#include "Rogue10mSkillTreeData.h"
#include "Rogue10mHybridSkillTreeWidget.generated.h"

class ARogue10mPlayerState;
class URogue10mAttackSkillData;
class URogue10mCombatComponent;
class URogue10mHybridSkillTreeWidget;
class URogue10mHybridSkillTreeGraphWidget;
class URogue10mSkillBindingComponent;
class URogue10mSkillTreeProgressionComponent;
class URogue10mTownProgressionComponent;
class UCanvasPanel;
class UComboBoxString;
class UImage;
class UInputKeySelector;
class UProgressBar;
class UTextBlock;

/** Action context belongs to the control, never to an unstable visible array index. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(URogue10mHybridSkillTreeWidget* InOwner, int32 InAction, FName InNodeId = NAME_None, FName InChoiceId = NAME_None);
private:
	UFUNCTION() void Clicked();
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> OwnerWidget;
	int32 Action = 0;
	FName NodeId;
	FName ChoiceId;
};

/** Native node keeps the existing unlocked-attack drag operation and keyboard button. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(URogue10mHybridSkillTreeWidget* Owner, const FRogue10mSkillTreeNodeDefinition& Definition,
		ERogue10mSkillTreeNodeState State, bool bSelected, bool bCurrentTrack);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
private:
	void BuildEntry();
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> OwnerWidget;
	FName NodeId;
	bool bCanDrag = false;
	UPROPERTY(Transient) TObjectPtr<URogue10mAttackSkillData> Skill;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> NodeButton;
	UPROPERTY(Transient) TObjectPtr<UImage> Icon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Symbol;
};

/** A separate lower canvas layer keeps painted edges below node labels and buttons. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeConnectionsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetGraph(URogue10mHybridSkillTreeGraphWidget* InGraph);
protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	TWeakObjectPtr<URogue10mHybridSkillTreeGraphWidget> Graph;
};

/** Ring and dependency geometry is painted behind real UMG node controls. No Tick. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeGraphWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetData(URogue10mHybridSkillTreeWidget* Owner, URogue10mSkillTreeProgressionComponent* Progression,
		ERogue10mSkillTreeTrack Track, FName SelectedNode);
	int32 PaintConnections(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 LayerId) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void BuildGraph();
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> OwnerWidget;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Progression;
	ERogue10mSkillTreeTrack Track = ERogue10mSkillTreeTrack::Brawler;
	FName Selected;
	TArray<FRogue10mSkillTreeNodeDefinition> Nodes;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TArray<TObjectPtr<URogue10mHybridSkillTreeEntryWidget>> Entries;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
};

/** The existing K-window hosts this native view; Blueprint assets remain configuration. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeTree();
	void SetWindowActive(bool bActive);
	void ReleaseContext();
	void RefreshFromData();
	void CancelKeySelection();
	void HandleAction(int32 Action, FName NodeId = NAME_None, FName ChoiceId = NAME_None);
	void SelectAttackSkill(URogue10mAttackSkillData* Skill);
	bool SetSelectedSkillKey(FKey Key);
	bool ClearSelectedSkillKey();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void BrowseTrack(ERogue10mSkillTreeTrack Track);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void SelectNode(FName NodeId);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	FName GetSelectedNodeId() const { return SelectedNodeId; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	ERogue10mSkillTreeTrack GetDisplayedTrack() const { return DisplayedTrack; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	int32 GetVisibleNodeCount() const;
	URogue10mAttackSkillData* GetSelectedAttackSkill() const { return GetSelectedSkill(); }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void BuildTree();
	void BindContext();
	void RefreshDetails();
	void ShowMessage(const FText& Message, bool bError = false);
	void RefreshKeyControls();
	URogue10mCombatComponent* GetCombat() const;
	URogue10mSkillBindingComponent* GetBindings() const;
	URogue10mAttackSkillData* GetSelectedSkill() const;
	UTextBlock* MakeText(FName Name, const FText& Text, int32 Size, FLinearColor Color);
	URogue10mHybridSkillTreeButton* MakeButton(FName Name, const FText& Label, int32 Action,
		FName NodeId = NAME_None, FName ChoiceId = NAME_None);
	UFUNCTION() void HandleProgressionChanged();
	UFUNCTION() void HandleKeySelected(FInputChord Key);
	UFUNCTION() void HandleBindingsChanged();
	UFUNCTION() void HandleBasicSkillSelected(FString Selection, ESelectInfo::Type SelectionType);
	TWeakObjectPtr<ARogue10mPlayerState> BoundState;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Progression;
	TWeakObjectPtr<URogue10mTownProgressionComponent> BoundTown;
	TWeakObjectPtr<URogue10mCombatComponent> BoundCombat;
	TWeakObjectPtr<URogue10mSkillBindingComponent> BoundBindings;
	ERogue10mSkillTreeTrack DisplayedTrack = ERogue10mSkillTreeTrack::Brawler;
	FName SelectedNodeId;
	FName PendingChoiceId;
	TWeakObjectPtr<URogue10mAttackSkillData> SelectedBasicSkill;
	bool bHasChosenTrack = false;
	bool bUpdatingKeys = false;
	bool bApplyingAction = false;
	bool bRefundConfirmation = false;
	bool bWindowActive = false;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeGraphWidget> Graph;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> FighterTab;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> GreatSwordTab;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GrowthText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MasteryText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> MasteryBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MasteryStatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> IdentityText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailState;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailDescription;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailRequirements;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCost;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BindingText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LegendText;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> BasicSkillPicker;
	UPROPERTY(Transient) TObjectPtr<UInputKeySelector> KeySelector;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> ClearKeyButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> PointsButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> MasteryButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> ActivateButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> ChoiceAButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> ChoiceBButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> RefundButton;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> RefundCancelButton;
};
