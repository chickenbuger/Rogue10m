// Editor-only narrow graph authoring. The Python caller validates and saves the single affected AnimBP.
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

namespace Rogue10mGroundLocomotionAuthoring
{
static const TCHAR* AssetPath = TEXT("/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed");
static const TCHAR* BlendPath = TEXT("/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference.BS_Unarmed_PalmReference");
static const FName RateName(TEXT("GroundLocomotionPlayRate"));

static UEdGraphPin* FindPin(UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction)
{
    UEdGraphPin* Found = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinName == Name && Pin->Direction == Direction)
        { if (Found) { return nullptr; } Found = Pin; }
    }
    return Found;
}

static bool IsBound(UAnimGraphNode_BlendSpacePlayer* Player)
{
    UEdGraphPin* Rate = FindPin(Player, TEXT("PlayRate"), EGPD_Input);
    UEdGraphPin* Output = Rate && Rate->LinkedTo.Num() == 1 ? Rate->LinkedTo[0] : nullptr;
    const UK2Node_VariableGet* Getter = Output ? Cast<UK2Node_VariableGet>(Output->GetOwningNode()) : nullptr;
    return Getter && Getter->VariableReference.GetMemberName() == RateName
        && Output->PinName == RateName && Output->LinkedTo.Num() == 1 && Output->LinkedTo[0] == Rate;
}

static bool Configure(bool bApply, TSharedPtr<FJsonObject> Report, FString& Error)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Requires editor without PIE/SIE"); return false; }
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, AssetPath);
    if (!Blueprint || Blueprint->ParentClass != URogue10mFighterCombatStanceAnimInstance::StaticClass())
    { Error = TEXT("Unexpected common AnimBP parent"); return false; }
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    UAnimGraphNode_BlendSpacePlayer* Player = nullptr;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* Candidate = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
            { if (Player) { Error = TEXT("Ambiguous grounded BlendSpace players"); return false; } Player = Candidate; }
        }
    }
    if (!Player || Player->GetGraph()->GetName() != TEXT("Walk / Run")
        || !Player->Node.GetBlendSpace() || Player->Node.GetBlendSpace()->GetPathName() != BlendPath)
    { Error = TEXT("Expected isolated PalmReference Walk / Run player"); return false; }
    Report->SetStringField(TEXT("player"), Player->GetPathName());
    Report->SetBoolField(TEXT("bound_before"), IsBound(Player));
    Report->SetBoolField(TEXT("saved"), false);
    if (IsBound(Player)) { Report->SetBoolField(TEXT("changed"), false); return true; }
    if (!bApply) { Error = TEXT("Native grounded play-rate property is not connected"); return false; }
    const int32 OptionalIndex = Player->ShowPinForProperties.IndexOfByPredicate([](const FOptionalPinFromProperty& Pin)
    { return Pin.PropertyName == TEXT("PlayRate"); });
    if (OptionalIndex == INDEX_NONE || !Player->ShowPinForProperties[OptionalIndex].bCanToggleVisibility)
    { Error = TEXT("No exposable PlayRate property"); return false; }
    if (UEdGraphPin* Existing = FindPin(Player, TEXT("PlayRate"), EGPD_Input))
    { if (Existing->LinkedTo.Num() != 0) { Error = TEXT("Unexpected existing PlayRate source"); return false; } }
    const bool bWasShown = Player->ShowPinForProperties[OptionalIndex].bShowPin;
    const bool bWasDirty = Blueprint->GetOutermost()->IsDirty();
    const int32 PreviousCount = Player->GetGraph()->Nodes.Num();
    Blueprint->Modify(); Player->Modify(); Player->GetGraph()->Modify();
    Player->SetPinVisibility(true, OptionalIndex);
    FGraphNodeCreator<UK2Node_VariableGet> Creator(*Player->GetGraph());
    UK2Node_VariableGet* Getter = Creator.CreateNode(false);
    Getter->VariableReference.SetSelfMember(RateName);
    Getter->NodePosX = Player->NodePosX - 280; Getter->NodePosY = Player->NodePosY + 180;
    Getter->NodeComment = TEXT("Grounded cadence above authored 600 cm/s");
    Creator.Finalize();
    auto Rollback = [&]()
    {
        FBlueprintEditorUtils::RemoveNode(Blueprint, Getter, true);
        Player->SetPinVisibility(bWasShown, OptionalIndex);
        FCompilerResultsLog RollbackLog;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &RollbackLog);
        Blueprint->GetOutermost()->SetDirtyFlag(bWasDirty);
        Report->SetBoolField(TEXT("rolled_back"), true);
        Report->SetNumberField(TEXT("rollback_compile_errors"), RollbackLog.NumErrors);
    };
    UEdGraphPin* Rate = FindPin(Player, TEXT("PlayRate"), EGPD_Input);
    UEdGraphPin* Output = FindPin(Getter, RateName, EGPD_Output);
    if (!Rate || !Output || !Player->GetSchema()->TryCreateConnection(Output, Rate))
    { Error = TEXT("Unable to connect native grounded rate"); Rollback(); return false; }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
    Report->SetNumberField(TEXT("compile_errors"), Log.NumErrors);
    Report->SetNumberField(TEXT("compile_warnings"), Log.NumWarnings);
    if (Log.NumErrors || Blueprint->Status == BS_Error || !IsBound(Player)
        || Player->GetGraph()->Nodes.Num() != PreviousCount + 1)
    { Error = TEXT("Grounded rate compile/binding/node preservation failed"); Rollback(); return false; }
    Report->SetBoolField(TEXT("changed"), true);
    Report->SetBoolField(TEXT("bound_after"), true);
    Report->SetStringField(TEXT("getter"), Getter->GetName());
    return true;
}

static void Run(bool bApply)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("apply"), bApply);
    Report->SetStringField(TEXT("asset"), AssetPath);
    FString Error;
    const bool bPassed = Configure(bApply, Report, Error);
    Report->SetStringField(TEXT("result"), bPassed ? TEXT("PASSED") : TEXT("FAILED"));
    Report->SetStringField(TEXT("error"), Error);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Folder = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/mouse-head-stable-locomotion-20261013");
    IFileManager::Get().MakeDirectory(*Folder, true);
    const bool bWritten = FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("ground-rate-native.json")));
    if (bPassed && bWritten) { UE_LOG(LogRogue10m, Display, TEXT("RESULT=GROUND_LOCOMOTION_RATE_PASSED apply=%d"), bApply); }
    else { UE_LOG(LogRogue10m, Error, TEXT("RESULT=GROUND_LOCOMOTION_RATE_FAILED: %s"), *Error); }
}
static void Apply() { Run(true); }
static void Audit() { Run(false); }
static FAutoConsoleCommand ApplyCommand(TEXT("Rogue10m.ConfigureGroundLocomotionRate"),
    TEXT("Editor only: connect grounded play rate in the single existing Walk / Run graph, compile, never save."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
static FAutoConsoleCommand AuditCommand(TEXT("Rogue10m.AuditGroundLocomotionRate"),
    TEXT("Editor only: inspect the native grounded rate binding, never modify, compile or save."),
    FConsoleCommandDelegate::CreateStatic(&Audit));
}
#endif
