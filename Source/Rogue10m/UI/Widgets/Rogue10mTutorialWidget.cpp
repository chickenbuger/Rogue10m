// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Rogue10mTutorialNPC.h"
URogue10mTutorialActionButton::URogue10mTutorialActionButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) { InitIsFocusable(false); }
void URogue10mTutorialActionButton::Configure(URogue10mTutorialWidget* Owner,int32 InAction)
{ OwnerWidget=Owner; Action=InAction; OnClicked.AddUniqueDynamic(this,&ThisClass::HandleClicked); }
void URogue10mTutorialActionButton::HandleClicked()
{ if(auto* Owner=OwnerWidget.Get()){Owner->HandleAction(Action);} }
TSharedRef<SWidget> URogue10mTutorialWidget::RebuildWidget()
{ EnsureNativeWidget();return Super::RebuildWidget(); }
void URogue10mTutorialWidget::NativeConstruct()
{ Super::NativeConstruct();SetIsFocusable(true);EnsureNativeWidget();SetVisibility(ESlateVisibility::HitTestInvisible); }
URogue10mTutorialActionButton* URogue10mTutorialWidget::AddAction(UVerticalBox* Rows,FName Name,int32 Action,UTextBlock*& Label)
{
    auto* Button=WidgetTree->ConstructWidget<URogue10mTutorialActionButton>(URogue10mTutorialActionButton::StaticClass(),Name);
    Button->Configure(this,Action);Button->SetBackgroundColor(FLinearColor(.12f,.18f,.15f,1));
    Rows->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,2));
    Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*(Name.ToString()+TEXT("Label"))));
    auto Font=Label->GetFont();Font.Size=15;Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor(.93f,.95f,.91f)));
    Label->SetAutoWrapText(true);Label->SetWrapTextAt(470.f);Label->SetMargin(FMargin(10,5));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);Button->AddChild(Label);return Button;
}
void URogue10mTutorialWidget::EnsureNativeWidget()
{
    if(NameText||!WidgetTree){return;}
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("TutorialCanvas"));
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Canvas;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("TutorialScale"));
    Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* CanvasSlot=Canvas->AddChildToCanvas(Scale);CanvasSlot->SetAnchors(FAnchors(.52f,.04f,.98f,.94f));CanvasSlot->SetOffsets(FMargin(0));
    auto* Size=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("TutorialSize"));Size->SetWidthOverride(550);Scale->AddChild(Size);
    if(auto* ScaleSlot=Cast<UScaleBoxSlot>(Size->Slot)){ScaleSlot->SetHorizontalAlignment(HAlign_Right);ScaleSlot->SetVerticalAlignment(VAlign_Top);}
    auto* Border=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("TutorialCard"));
    Border->SetBrushColor(FLinearColor(.028f,.040f,.035f,.97f));Border->SetPadding(FMargin(16,12));Size->AddChild(Border);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("TutorialRows"));Border->AddChild(Rows);
    auto AddText=[this,Rows](FName Name,int32 FontSize,FLinearColor Color,float Gap)
    {
        auto* Text=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=Text->GetFont();Font.Size=FontSize;Text->SetFont(Font);Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetAutoWrapText(true);Text->SetWrapTextAt(510);Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0,0,0,Gap));return Text;
    };
    NameText=AddText(TEXT("GuideName"),23,FLinearColor(.96f,.83f,.53f),1);
    RoleText=AddText(TEXT("GuideRole"),14,FLinearColor(.66f,.78f,.70f),7);
    DialogueText=AddText(TEXT("TutorialDialogue"),17,FLinearColor(.91f,.93f,.89f),7);
    ServicesText=AddText(TEXT("TownServices"),14,FLinearColor(.91f,.93f,.89f),5);ServicesText->SetVisibility(ESlateVisibility::Collapsed);
    ChoiceRows=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("TownChoices"));
    Rows->AddChildToVerticalBox(ChoiceRows);ChoiceRows->SetVisibility(ESlateVisibility::Collapsed);
    for(int32 Index=0;Index<7;++Index)
    {
        UTextBlock* Label=nullptr;
        ServiceButtons.Add(AddAction(ChoiceRows,FName(*FString::Printf(TEXT("TownService%d"),Index)),Index,Label));ServiceLabels.Add(Label);
    }
    UTextBlock* Label=nullptr;
    SupplyButton=AddAction(ChoiceRows,TEXT("TownClaimSupplies"),7,Label);Label->SetText(NSLOCTEXT("Rogue10mTutorial","SupplyButton","출발 보급품 받기"));
    CloseButton=AddAction(ChoiceRows,TEXT("TownCloseDialogue"),8,Label);Label->SetText(NSLOCTEXT("Rogue10mTutorial","CloseButton","대화 닫기"));
    StatusText=AddText(TEXT("TutorialStatus"),15,FLinearColor(.69f,.83f,.72f),5);
    ActionText=AddText(TEXT("TutorialAction"),14,FLinearColor(.96f,.83f,.53f),0);
}
void URogue10mTutorialWidget::SetServiceView(const FText& Services)
{
    EnsureNativeWidget();FullServices=Services;if(!ServicesText){return;}
    // Keep full inspection text; draw a compact summary plus actual buttons.
    FString Summary=Services.ToString();int32 First=INDEX_NONE;
    if(Summary.FindChar(TEXT('\n'),First))
    {
        const int32 Second=Summary.Find(TEXT("\n"),ESearchCase::CaseSensitive,ESearchDir::FromStart,First+1);
        if(Second!=INDEX_NONE){Summary.LeftInline(Second);}
    }
    ServicesText->SetText(FText::FromString(Summary));ServicesText->SetVisibility(Services.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
}
void URogue10mTutorialWidget::SetServiceChoices(ARogue10mTutorialNPC* NPC,const TArray<FText>& Labels,int32 Selected,bool bCanClaim)
{
    EnsureNativeWidget();ServiceNPC=NPC;const bool bOpen=IsValid(NPC)&&NPC->IsDialogueOpen();
    ChoiceRows->SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    for(int32 Index=0;Index<ServiceButtons.Num();++Index)
    {
        ServiceButtons[Index]->SetVisibility(Labels.IsValidIndex(Index)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        ServiceLabels[Index]->SetText(Labels.IsValidIndex(Index)?Labels[Index]:FText::GetEmpty());
        ServiceButtons[Index]->SetBackgroundColor(Index==Selected?FLinearColor(.34f,.31f,.12f,1):FLinearColor(.12f,.18f,.15f,1));
    }
    SupplyButton->SetVisibility(bCanClaim?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible);
}
void URogue10mTutorialWidget::SetDialogueView(const FText& Name,const FText& Role,const FText& Dialogue,const FText& Status,const FText& Action)
{
    EnsureNativeWidget();if(!NameText){return;}
    NameText->SetText(Name);RoleText->SetText(Role);DialogueText->SetText(Dialogue);StatusText->SetText(Status);ActionText->SetText(Action);
    StatusText->SetVisibility(Status.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
}
void URogue10mTutorialWidget::HandleAction(int32 Action)
{
    auto* NPC=ServiceNPC.Get();if(!NPC||!NPC->IsDialogueOpen()){return;}
    if(Action>=0&&Action<7){NPC->ActivateService(Action);}else if(Action==7){NPC->ClaimStartingSupplies();}else if(Action==8){NPC->CloseDialogue();}
    if(IsInViewport()&&NPC->IsDialogueOpen()&&GetVisibility()!=ESlateVisibility::Collapsed&&GetOwningPlayer()){SetUserFocus(GetOwningPlayer());}
}
FReply URogue10mTutorialWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& KeyEvent)
{
    auto* NPC=ServiceNPC.Get();if(!NPC||!NPC->IsDialogueOpen()){return Super::NativeOnPreviewKeyDown(Geometry,KeyEvent);}
    const FKey Key=KeyEvent.GetKey();
    if(Key==EKeys::Up||Key==EKeys::Down){NPC->NavigateService(Key==EKeys::Up?-1:1);return FReply::Handled();}
    if(Key==EKeys::Enter||Key==EKeys::SpaceBar){if(!KeyEvent.IsRepeat()){NPC->ActivateService(NPC->GetSelectedServiceIndex());}return FReply::Handled();}
    if(Key==EKeys::F){if(!KeyEvent.IsRepeat()){NPC->Interact();}return FReply::Handled();}
    if(Key==EKeys::Escape){if(!KeyEvent.IsRepeat()){NPC->CloseDialogue();}return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(Geometry,KeyEvent);
}
FText URogue10mTutorialWidget::GetDisplayedDialogue()const{return DialogueText?DialogueText->GetText():FText::GetEmpty();}
FText URogue10mTutorialWidget::GetDisplayedServices()const{return FullServices;}
