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
class UBorder;
class UCanvasPanel;
class UComboBoxString;
class UDragDropOperation;
class UHorizontalBox;
class UImage;
class UInputKeySelector;
class UOverlaySlot;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;

UENUM(BlueprintType)
enum class ERogue10mSkillTreeTab : uint8
{
	Common,
	Knuckle,
	Sword,
	Magic
};

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
		ERogue10mSkillTreeNodeState State, bool bSelected, bool bCurrentTrack, float DiameterOverride = 0.0f);
	FName GetNodeId() const { return NodeId; }
	bool CanDragSkill() const { return bCanDrag; }
	URogue10mAttackSkillData* GetDragSkill() const { return Skill.Get(); }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
private:
	void BuildEntry();
	TWeakObjectPtr<URogue10mHybridSkillTreeWidget> OwnerWidget;
	FName NodeId;
	bool bCanDrag = false;
	bool bHasOuterFrame = false;
	bool bPaintSwordGlyph = false;
	ERogue10mCommonSkill CommonGlyph = ERogue10mCommonSkill::None;
	FLinearColor GlyphTint;
	UPROPERTY(Transient) TObjectPtr<URogue10mAttackSkillData> Skill;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BodyBox;
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

/** The current tab's paths are painted behind real UMG node controls. */
UCLASS()
class ROGUE10M_API URogue10mHybridSkillTreeGraphWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetData(URogue10mHybridSkillTreeWidget* Owner, URogue10mSkillTreeProgressionComponent* Progression,
		ERogue10mSkillTreeTab InTab, ERogue10mSkillTreeTrack ContextTrack, FName SelectedNode);
	int32 PaintConnections(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 LayerId) const;
	URogue10mHybridSkillTreeEntryWidget* FindNodeEntry(FName NodeId) const;
	FName GetNodeFrameShape(FName NodeId) const;
	int32 GetNodeFrameOutlineCount(FName NodeId) const;
	TArray<FVector2f> GetNodeOuterFramePoints(FName NodeId) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void BuildGraph();
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Progression;
	ERogue10mSkillTreeTab Tab = ERogue10mSkillTreeTab::Common;
	FName Selected;
	TArray<FRogue10mSkillTreeNodeDefinition> Nodes;
	TArray<ERogue10mSkillTreeNodeState> States;
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
	void BeginSkillDrag(UDragDropOperation* Operation);
	bool IsSkillDragInProgress() const { return bSkillDragPresentation; }
	void HandleAction(int32 Action, FName NodeId = NAME_None, FName ChoiceId = NAME_None);
	void SelectAttackSkill(URogue10mAttackSkillData* Skill);
	bool SetSelectedSkillKey(FKey Key);
	bool ClearSelectedSkillKey();
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void BrowseTrack(ERogue10mSkillTreeTrack Track);
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void BrowseTab(ERogue10mSkillTreeTab Tab);
	void BrowseCommonIdentity();
	bool IsShowingCommonIdentity() const { return bShowingCommonIdentity; }
	UFUNCTION(BlueprintCallable, Category="Rogue10m|Skill Tree")
	void SelectNode(FName NodeId);
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	FName GetSelectedNodeId() const { return SelectedNodeId; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	ERogue10mSkillTreeTrack GetDisplayedTrack() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	ERogue10mSkillTreeTab GetDisplayedTab() const { return DisplayedTab; }
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	TArray<ERogue10mSkillTreeTab> GetTabOrder() const;
	UFUNCTION(BlueprintPure, Category="Rogue10m|Skill Tree")
	int32 GetVisibleNodeCount() const;
	TArray<FRogue10mSkillTreeNodeDefinition> GetDisplayNodes() const;
	URogue10mAttackSkillData* GetSelectedAttackSkill() const { return GetSelectedSkill(); }
	/** Read-only presentation queries share the geometry used by the painted frame. */
	URogue10mHybridSkillTreeEntryWidget* FindNodeEntry(FName NodeId) const;
	FName GetNodeFrameShape(FName NodeId) const;
	int32 GetNodeFrameOutlineCount(FName NodeId) const;
	TArray<FVector2f> GetNodeOuterFramePoints(FName NodeId) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void BuildTree();
	void ApplyCommonChrome(bool bCommonWeb);
	void BindContext();
	void RefreshEquipmentTab();
	void BrowseTabView(ERogue10mSkillTreeTab Tab, bool bShowCommonIdentity);
	void SetDisplayedTab(ERogue10mSkillTreeTab Tab, bool bShowCommonIdentity = false);
	URogue10mHybridSkillTreeButton* GetTabButton(ERogue10mSkillTreeTab Tab) const;
	bool IsDisplayingActiveTrack() const;
	void RefreshDetails();
	void ShowMessage(const FText& Message, bool bError = false);
	void RefreshKeyControls();
	void CancelOwnedSkillDrag();
	void RestoreSkillDragPresentation();
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
	UFUNCTION() void HandleSkillDragEnded(UDragDropOperation* Operation);
	TWeakObjectPtr<ARogue10mPlayerState> BoundState;
	TWeakObjectPtr<URogue10mSkillTreeProgressionComponent> Progression;
	TWeakObjectPtr<URogue10mTownProgressionComponent> BoundTown;
	TWeakObjectPtr<URogue10mCombatComponent> BoundCombat;
	TWeakObjectPtr<URogue10mSkillBindingComponent> BoundBindings;
	ERogue10mSkillTreeTab DisplayedTab = ERogue10mSkillTreeTab::Common;
	TOptional<ERogue10mWeaponType> EquippedWeapon;
	FName SelectedNodeId;
	FName PendingChoiceId;
	TWeakObjectPtr<URogue10mAttackSkillData> SelectedBasicSkill;
	TWeakObjectPtr<UDragDropOperation> ActiveSkillDrag;
	ESlateVisibility VisibilityBeforeSkillDrag = ESlateVisibility::SelfHitTestInvisible;
	float OpacityBeforeSkillDrag = 1.0f;
	bool bSkillDragPresentation = false;
	bool bSelectEquippedTabOnRefresh = false;
	bool bUpdatingKeys = false;
	bool bApplyingAction = false;
	bool bRefundConfirmation = false;
	bool bWindowActive = false;
	bool bShowingCommonIdentity = false;
	TOptional<bool> bCommonChromeEnabled;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeGraphWidget> Graph;
	UPROPERTY(Transient) TObjectPtr<USizeBox> WindowSize;
	UPROPERTY(Transient) TObjectPtr<USizeBox> LeftColumnSize;
	UPROPERTY(Transient) TObjectPtr<USizeBox> GraphViewportSize;
	UPROPERTY(Transient) TObjectPtr<UBorder> WindowRim;
	UPROPERTY(Transient) TObjectPtr<UBorder> WindowInterior;
	UPROPERTY(Transient) TObjectPtr<UOverlaySlot> DetailPanelSlot;
	UPROPERTY(Transient) TObjectPtr<UOverlaySlot> GraphPanelSlot;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> CommonChromeWidgets;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TabBar;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TreeBody;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MasterySection;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MagicPlaceholder;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> CommonTab;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> FighterTab;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> GreatSwordTab;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> MagicTab;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeButton> CommonViewToggle;
	UPROPERTY(Transient) TObjectPtr<URogue10mHybridSkillTreeEntryWidget> CommonDetailIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CommonGrowthText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailHeading;
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
