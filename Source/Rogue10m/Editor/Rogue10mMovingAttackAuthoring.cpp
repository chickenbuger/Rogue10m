// Editor graph authoring only. The caller validates and saves the resulting AnimBP.
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
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
#include "Misc/ScopeExit.h"
#include "Rogue10m.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace Rogue10mMovingAttackAuthoring
{
static const TCHAR* AssetPath = TEXT("/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed");
static const TCHAR* ParentPath = TEXT("/Script/Rogue10m.Rogue10mFighterCombatStanceAnimInstance");
static const FName AlphaName(TEXT("MovingAttackUpperBodyAlpha"));
static const FString Prefix(TEXT("Rogue10m MovingAttack "));
static const FString LocomotionCacheName(TEXT("FighterMovingAttackLocomotion"));
static const FString MontageCacheName(TEXT("FighterMovingAttackMontage"));

struct FEndpoint
{
    UEdGraphNode* Node = nullptr;
    FName Name;
    EEdGraphPinDirection Direction = EGPD_Input;
};
struct FEdge { FEndpoint A, B; };

static UEdGraphPin* Pin(UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction)
{
    UEdGraphPin* Result = nullptr;
    if (!Node) { return nullptr; }
    for (UEdGraphPin* Candidate : Node->Pins)
    {
        if (Candidate && Candidate->PinName == Name && Candidate->Direction == Direction)
        {
            if (Result) { return nullptr; }
            Result = Candidate;
        }
    }
    return Result;
}
static UEdGraphPin* Resolve(const FEndpoint& Value) { return Pin(Value.Node, Value.Name, Value.Direction); }
static FEndpoint Endpoint(UEdGraphPin* Value)
{
    return { Value->GetOwningNode(), Value->PinName, Value->Direction };
}
static FString EndpointKey(const FEndpoint& Value)
{
    return Value.Node->NodeGuid.ToString() + TEXT(":") + Value.Name.ToString()
        + FString::FromInt(static_cast<int32>(Value.Direction));
}
static FString EdgeKey(const FEdge& Value)
{
    const FString A = EndpointKey(Value.A), B = EndpointKey(Value.B);
    return A < B ? A + TEXT("|") + B : B + TEXT("|") + A;
}
static TArray<FEdge> AllEdges(UEdGraph* Graph)
{
    TArray<FEdge> Result;
    TSet<FString> Seen;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        for (UEdGraphPin* Value : Node->Pins)
        {
            for (UEdGraphPin* Other : Value->LinkedTo)
            {
                const FEdge Edge{ Endpoint(Value), Endpoint(Other) };
                const FString Key = EdgeKey(Edge);
                if (!Seen.Contains(Key)) { Seen.Add(Key); Result.Add(Edge); }
            }
        }
    }
    return Result;
}
static TSet<FString> EdgeSet(const TArray<FEdge>& Edges)
{
    TSet<FString> Result;
    for (const FEdge& Value : Edges) { Result.Add(EdgeKey(Value)); }
    return Result;
}
static bool SameEdges(const TSet<FString>& A, const TSet<FString>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (const FString& Value : A) { if (!B.Contains(Value)) { return false; } }
    return true;
}
static bool Linked(UEdGraphPin* Output, UEdGraphPin* Input)
{
    return Output && Input && Input->LinkedTo.Num() == 1 && Input->LinkedTo[0] == Output
        && Output->LinkedTo.Contains(Input);
}
static UEdGraphPin* OnlyLink(UEdGraphPin* Value)
{
    return Value && Value->LinkedTo.Num() == 1 ? Value->LinkedTo[0] : nullptr;
}
template <class T> static T* Unique(UEdGraph* Graph)
{
    T* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (T* Candidate = Cast<T>(Node)) { if (Result) { return nullptr; } Result = Candidate; }
    }
    return Result;
}
static FString RuntimeText(UEdGraphNode* Node)
{
    const FProperty* Property = FindFProperty<FProperty>(Node->GetClass(),
        Cast<UAnimGraphNode_TwoWayBlend>(Node) ? TEXT("BlendNode") : TEXT("Node"));
    FString Result;
    if (Property) { Property->ExportText_InContainer(0, Result, Node, nullptr, Node, PPF_None); }
    return Result;
}
static UAnimGraphNode_TwoWayBlend* GuardBlend(UEdGraph* Graph)
{
    UAnimGraphNode_TwoWayBlend* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        auto* Blend = Cast<UAnimGraphNode_TwoWayBlend>(Node);
        UEdGraphPin* Source = Blend ? OnlyLink(Pin(Blend, TEXT("Alpha"), EGPD_Input)) : nullptr;
        auto* Getter = Source ? Cast<UK2Node_VariableGet>(Source->GetOwningNode()) : nullptr;
        if (Getter && Getter->VariableReference.GetMemberName() == TEXT("FighterCombatStanceAlpha"))
        { if (Result) { return nullptr; } Result = Blend; }
    }
    return Result;
}
static TSharedPtr<FJsonObject> GraphReport(UEdGraph* Graph)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Nodes, Edges;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Node->GetName());
        Row->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        Row->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
        Row->SetStringField(TEXT("comment"), Node->NodeComment);
        Row->SetStringField(TEXT("runtime"), RuntimeText(Node));
        if (auto* Save = Cast<UAnimGraphNode_SaveCachedPose>(Node))
        { Row->SetStringField(TEXT("cache_name"), Save->CacheName); }
        if (auto* Use = Cast<UAnimGraphNode_UseCachedPose>(Node))
        {
            Row->SetStringField(TEXT("save_cached_pose_node"),
                Use->SaveCachedPoseNode.IsValid() ? Use->SaveCachedPoseNode->GetName() : TEXT(""));
        }
        if (auto* Getter = Cast<UK2Node_VariableGet>(Node))
        { Row->SetStringField(TEXT("variable"), Getter->VariableReference.GetMemberName().ToString()); }
        Nodes.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (const FEdge& Value : AllEdges(Graph))
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("a_node"), Value.A.Node->GetName());
        Row->SetStringField(TEXT("a_pin"), Value.A.Name.ToString());
        Row->SetNumberField(TEXT("a_direction"), Value.A.Direction);
        Row->SetStringField(TEXT("b_node"), Value.B.Node->GetName());
        Row->SetStringField(TEXT("b_pin"), Value.B.Name.ToString());
        Row->SetNumberField(TEXT("b_direction"), Value.B.Direction);
        Edges.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("nodes"), Nodes);
    Result->SetArrayField(TEXT("edges"), Edges);
    return Result;
}
static bool CheckMask(UAnimBlueprint* Blueprint, FString& Error)
{
    if (!Blueprint->TargetSkeleton) { Error = TEXT("Missing target skeleton"); return false; }
    FInputBlendPose Filter;
    Filter.BranchFilters.Add({ TEXT("spine_01"), 1 });
    Filter.BranchFilters.Add({ TEXT("ik_hand_root"), 1 });
    TArray<FInputBlendPose> Filters{ Filter };
    TArray<FPerBoneBlendWeight> Weights;
    FAnimationRuntime::CreateMaskWeights(Weights, Filters, Blueprint->TargetSkeleton);
    const FReferenceSkeleton& Ref = Blueprint->TargetSkeleton->GetReferenceSkeleton();
    const TCHAR* Lower[] = { TEXT("root"), TEXT("pelvis"), TEXT("thigh_l"), TEXT("calf_l"),
        TEXT("foot_l"), TEXT("ball_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
        TEXT("ball_r"), TEXT("ik_foot_root"), TEXT("ik_foot_l"), TEXT("ik_foot_r") };
    const TCHAR* Upper[] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("hand_l"), TEXT("clavicle_r"), TEXT("hand_r"),
        TEXT("ik_hand_root"), TEXT("ik_hand_gun"), TEXT("ik_hand_l"), TEXT("ik_hand_r") };
    for (const TCHAR* Name : Lower)
    {
        const int32 Index = Ref.FindBoneIndex(Name);
        if (!Weights.IsValidIndex(Index) || Weights[Index].BlendWeight != 0.0f)
        { Error = FString(TEXT("Lower-body mask is not zero: ")) + Name; return false; }
    }
    for (const TCHAR* Name : Upper)
    {
        const int32 Index = Ref.FindBoneIndex(Name);
        if (!Weights.IsValidIndex(Index) || Weights[Index].BlendWeight != 1.0f)
        { Error = FString(TEXT("Upper-body mask is not one: ")) + Name; return false; }
    }
    return true;
}
static bool Compile(UAnimBlueprint* Blueprint, TSharedPtr<FJsonObject> Report, const TCHAR* Field)
{
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
    const bool bSuccess = Log.NumErrors == 0 && Blueprint->Status != BS_Error;
    Report->SetBoolField(Field, bSuccess);
    Report->SetNumberField(FString(Field) + TEXT("_errors"), Log.NumErrors);
    Report->SetNumberField(FString(Field) + TEXT("_warnings"), Log.NumWarnings);
    return bSuccess;
}
template <class T> static T* AddNode(UEdGraph* Graph, const TCHAR* Role, int32 X, int32 Y,
    TArray<UEdGraphNode*>& Added, TFunctionRef<void(T*)> Setup)
{
    FGraphNodeCreator<T> Creator(*Graph);
    T* Node = Creator.CreateNode(false);
    Node->NodePosX = X; Node->NodePosY = Y;
    Node->NodeComment = Prefix + Role;
    Node->bCommentBubbleVisible = true;
    Setup(Node);
    Creator.Finalize();
    Added.Add(Node);
    return Node;
}
template <class T> static T* RoleNode(UEdGraph* Graph, const TCHAR* Role)
{
    T* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->NodeComment == Prefix + Role)
        { if (Result || !Cast<T>(Node)) { return nullptr; } Result = Cast<T>(Node); }
    }
    return Result;
}
static bool ValidateApplied(UEdGraph* Graph, UAnimGraphNode_TwoWayBlend* Guard,
    UAnimGraphNode_Slot* Slot, UAnimGraphNode_Root* Root, FString& Error)
{
    auto* Loco = RoleNode<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("LocomotionCache"));
    auto* ToSlot = RoleNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("LocomotionToSlot"));
    auto* Base = RoleNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("LocomotionBase"));
    auto* Montage = RoleNode<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("MontageCache"));
    auto* Upper = RoleNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("MontageUpperBody"));
    auto* Full = RoleNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("MontageFullBody"));
    auto* Layer = RoleNode<UAnimGraphNode_LayeredBoneBlend>(Graph, TEXT("UpperBodyLayer"));
    auto* Select = RoleNode<UAnimGraphNode_TwoWayBlend>(Graph, TEXT("UpperBodySelector"));
    auto* Getter = RoleNode<UK2Node_VariableGet>(Graph, TEXT("UpperBodyAlpha"));
    int32 Count = 0;
    for (UEdGraphNode* Node : Graph->Nodes) { Count += Node->NodeComment.StartsWith(Prefix) ? 1 : 0; }
    if (Count != 9 || !Loco || !ToSlot || !Base || !Montage || !Upper || !Full || !Layer || !Select || !Getter)
    { Error = TEXT("Partial or ambiguous moving-attack nodes"); return false; }
    if (Loco->CacheName != LocomotionCacheName || Montage->CacheName != MontageCacheName
        || ToSlot->SaveCachedPoseNode.Get() != Loco || Base->SaveCachedPoseNode.Get() != Loco
        || Upper->SaveCachedPoseNode.Get() != Montage || Full->SaveCachedPoseNode.Get() != Montage
        || Getter->VariableReference.GetMemberName() != AlphaName)
    { Error = TEXT("Unexpected cache or alpha binding"); return false; }
    const FAnimNode_LayeredBoneBlend& Node = Layer->Node;
    if (Node.BlendMode != ELayeredBoneBlendMode::BranchFilter || Node.BlendPoses.Num() != 1
        || Node.BlendWeights.Num() != 1 || Node.BlendWeights[0] != 1.0f || Node.LayerSetup.Num() != 1
        || Node.LayerSetup[0].BranchFilters.Num() != 2 || !Node.bMeshSpaceRotationBlend
        || Node.bRootSpaceRotationBlend || Node.bMeshSpaceScaleBlend || !Node.bBlendRootMotionBasedOnRootBone)
    { Error = TEXT("Unexpected layered blend configuration"); return false; }
    const auto& Branches = Node.LayerSetup[0].BranchFilters;
    if (Branches[0].BoneName != TEXT("spine_01") || Branches[0].BlendDepth != 1
        || Branches[1].BoneName != TEXT("ik_hand_root") || Branches[1].BlendDepth != 1)
    { Error = TEXT("Unexpected upper-body branches"); return false; }
    const TArray<TPair<UEdGraphPin*, UEdGraphPin*>> Links = {
        { Pin(Guard, TEXT("Pose"), EGPD_Output), Pin(Loco, TEXT("Pose"), EGPD_Input) },
        { Pin(ToSlot, TEXT("Pose"), EGPD_Output), Pin(Slot, TEXT("Source"), EGPD_Input) },
        { Pin(Base, TEXT("Pose"), EGPD_Output), Pin(Layer, TEXT("BasePose"), EGPD_Input) },
        { Pin(Slot, TEXT("Pose"), EGPD_Output), Pin(Montage, TEXT("Pose"), EGPD_Input) },
        { Pin(Upper, TEXT("Pose"), EGPD_Output), Pin(Layer, TEXT("BlendPoses_0"), EGPD_Input) },
        { Pin(Full, TEXT("Pose"), EGPD_Output), Pin(Select, TEXT("A"), EGPD_Input) },
        { Pin(Layer, TEXT("Pose"), EGPD_Output), Pin(Select, TEXT("B"), EGPD_Input) },
        { Pin(Getter, AlphaName, EGPD_Output), Pin(Select, TEXT("Alpha"), EGPD_Input) },
        { Pin(Select, TEXT("Pose"), EGPD_Output), Pin(Root, TEXT("Result"), EGPD_Input) } };
    for (const auto& Pair : Links)
    {
        if (!Linked(Pair.Key, Pair.Value) || Pair.Key->LinkedTo.Num() != 1)
        { Error = TEXT("Unexpected moving-attack pose edge"); return false; }
    }
    return true;
}
static bool Apply(TSharedPtr<FJsonObject> Report, FString& Error)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Requires editor with no PIE/SIE world"); return false; }
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, AssetPath);
    if (!Blueprint || !Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != ParentPath
        || !FindFProperty<FFloatProperty>(Blueprint->ParentClass, AlphaName))
    { Error = TEXT("Expected source AnimBP native parent/float alpha unavailable; build editor first"); return false; }
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Candidate : Blueprint->UbergraphPages)
    { if (Candidate && Candidate->GetFName() == TEXT("AnimGraph")) { if (Graph) { Error = TEXT("Ambiguous AnimGraph"); return false; } Graph = Candidate; } }
    // Animation graphs are generally function graphs, not Ubergraph pages.
    for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
    { if (Candidate && Candidate->GetFName() == TEXT("AnimGraph")) { if (Graph) { Error = TEXT("Ambiguous AnimGraph"); return false; } Graph = Candidate; } }
    if (!Graph || !Graph->GetSchema()) { Error = TEXT("Missing AnimGraph or schema"); return false; }
    auto* Slot = Unique<UAnimGraphNode_Slot>(Graph);
    auto* Root = Unique<UAnimGraphNode_Root>(Graph);
    auto* Guard = GuardBlend(Graph);
    UEdGraphNode* Rig = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->GetClass()->GetFName() == TEXT("AnimGraphNode_ControlRig"))
        { if (Rig) { Error = TEXT("Ambiguous locomotion rig"); return false; } Rig = Node; }
    }
    UEdGraphPin* GuardAlpha = Guard ? OnlyLink(Pin(Guard, TEXT("Alpha"), EGPD_Input)) : nullptr;
    UEdGraphPin* GuardB = Guard ? OnlyLink(Pin(Guard, TEXT("B"), EGPD_Input)) : nullptr;
    if (!Slot || !Root || !Guard || !Rig || Slot->Node.SlotName != TEXT("DefaultSlot")
        || !GuardAlpha || !GuardB || !Cast<UAnimGraphNode_SequencePlayer>(GuardB->GetOwningNode())
        || !RuntimeText(Rig).Contains(TEXT("CR_Mannequin_FootIK"))
        || !Linked(Pin(Rig, TEXT("Pose"), EGPD_Output), Pin(Guard, TEXT("A"), EGPD_Input)))
    { Error = TEXT("Expected FootIK/standing guard/DefaultSlot graph unavailable"); return false; }
    if (!CheckMask(Blueprint, Error)) { return false; }
    Report->SetObjectField(TEXT("before"), GraphReport(Graph));
    Report->SetBoolField(TEXT("package_dirty_before"), Blueprint->GetOutermost()->IsDirty());
    bool bAnyApplied = false;
    for (UEdGraphNode* Node : Graph->Nodes) { bAnyApplied |= Node->NodeComment.StartsWith(Prefix); }
    if (bAnyApplied)
    {
        if (!ValidateApplied(Graph, Guard, Slot, Root, Error)) { return false; }
        const TSet<FString> AppliedEdges = EdgeSet(AllEdges(Graph));
        if (!Compile(Blueprint, Report, TEXT("compile_passed"))
            || !SameEdges(AppliedEdges, EdgeSet(AllEdges(Graph)))
            || !ValidateApplied(Graph, Guard, Slot, Root, Error))
        { if (Error.IsEmpty()) { Error = TEXT("Idempotent graph compile/audit failed"); } return false; }
        Report->SetBoolField(TEXT("idempotent"), true);
        Report->SetBoolField(TEXT("changed"), false);
        Report->SetBoolField(TEXT("original_nodes_and_settings_preserved"), true);
        Report->SetNumberField(TEXT("added_node_count"), 0);
        Report->SetObjectField(TEXT("after"), GraphReport(Graph));
        return true;
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Save = Cast<UAnimGraphNode_SaveCachedPose>(Node))
        { if (Save->CacheName == LocomotionCacheName || Save->CacheName == MontageCacheName)
            { Error = TEXT("Reserved moving-attack cache already exists"); return false; } }
    }
    UEdGraphPin* GuardOut = Pin(Guard, TEXT("Pose"), EGPD_Output);
    UEdGraphPin* SlotIn = Pin(Slot, TEXT("Source"), EGPD_Input);
    UEdGraphPin* SlotOut = Pin(Slot, TEXT("Pose"), EGPD_Output);
    UEdGraphPin* RootIn = Pin(Root, TEXT("Result"), EGPD_Input);
    if (!Linked(GuardOut, SlotIn) || GuardOut->LinkedTo.Num() != 1
        || !Linked(SlotOut, RootIn) || SlotOut->LinkedTo.Num() != 1)
    { Error = TEXT("Refusing unexpected original guard/slot/root wiring"); return false; }
    const TArray<FEdge> OriginalEdges = AllEdges(Graph);
    const TSet<FString> BeforeEdges = EdgeSet(OriginalEdges);
    TArray<UEdGraphNode*> OriginalNodes;
    for (UEdGraphNode* Node : Graph->Nodes) { OriginalNodes.Add(Node); }
    const FString SlotText = RuntimeText(Slot), RigText = RuntimeText(Rig), GuardText = RuntimeText(Guard);
    const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();
    TArray<UEdGraphNode*> Added;
    FScopedTransaction Transaction(NSLOCTEXT("Rogue10m", "MovingAttackGraph", "Layer moving attacks over locomotion"));
    Blueprint->Modify(); Graph->Modify(); Slot->Modify(); Guard->Modify(); Root->Modify();
    auto Rollback = [&]()
    {
        for (UEdGraphNode* Node : Added) { FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true); }
        for (UEdGraphNode* Node : OriginalNodes)
        { for (UEdGraphPin* Value : Node->Pins) { Value->BreakAllPinLinks(false); } }
        bool bRestored = true;
        for (const FEdge& Value : OriginalEdges)
        {
            UEdGraphPin* A = Resolve(Value.A); UEdGraphPin* B = Resolve(Value.B);
            bRestored &= A && B && Graph->GetSchema()->TryCreateConnection(A, B);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        bRestored &= Compile(Blueprint, Report, TEXT("rollback_compile_passed"));
        bRestored &= SameEdges(BeforeEdges, EdgeSet(AllEdges(Graph)));
        Report->SetBoolField(TEXT("rolled_back"), bRestored);
        if (bRestored) { Blueprint->GetOutermost()->SetDirtyFlag(bDirtyBefore); }
        Transaction.Cancel();
    };
    auto* Loco = AddNode<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("LocomotionCache"), 600, -180, Added,
        [](auto* Node) { Node->CacheName = LocomotionCacheName; });
    auto* ToSlot = AddNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("LocomotionToSlot"), 820, -180, Added,
        [Loco](auto* Node) { Node->SaveCachedPoseNode = Loco; });
    auto* Base = AddNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("LocomotionBase"), 820, 260, Added,
        [Loco](auto* Node) { Node->SaveCachedPoseNode = Loco; });
    auto* Montage = AddNode<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("MontageCache"), 1050, -180, Added,
        [](auto* Node) { Node->CacheName = MontageCacheName; });
    auto* Upper = AddNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("MontageUpperBody"), 1050, 350, Added,
        [Montage](auto* Node) { Node->SaveCachedPoseNode = Montage; });
    auto* Full = AddNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("MontageFullBody"), 1350, -180, Added,
        [Montage](auto* Node) { Node->SaveCachedPoseNode = Montage; });
    auto* Layer = AddNode<UAnimGraphNode_LayeredBoneBlend>(Graph, TEXT("UpperBodyLayer"), 1350, 260, Added,
        [](auto* Node)
        {
            Node->Node.BlendMode = ELayeredBoneBlendMode::BranchFilter;
            Node->Node.LayerSetup[0].BranchFilters = { { TEXT("spine_01"), 1 }, { TEXT("ik_hand_root"), 1 } };
            Node->Node.bMeshSpaceRotationBlend = true;
            Node->Node.bRootSpaceRotationBlend = false;
            Node->Node.bMeshSpaceScaleBlend = false;
            Node->Node.bBlendRootMotionBasedOnRootBone = true;
            Node->Node.InvalidatePerBoneBlendWeights();
        });
    auto* Select = AddNode<UAnimGraphNode_TwoWayBlend>(Graph, TEXT("UpperBodySelector"), 1650, 0, Added,
        [](auto*) {});
    auto* Getter = AddNode<UK2Node_VariableGet>(Graph, TEXT("UpperBodyAlpha"), 1350, 530, Added,
        [](auto* Node) { Node->VariableReference.SetSelfMember(AlphaName); });
    const TArray<TPair<UEdGraphPin*, UEdGraphPin*>> Links = {
        { GuardOut, Pin(Loco, TEXT("Pose"), EGPD_Input) },
        { Pin(ToSlot, TEXT("Pose"), EGPD_Output), SlotIn },
        { Pin(Base, TEXT("Pose"), EGPD_Output), Pin(Layer, TEXT("BasePose"), EGPD_Input) },
        { SlotOut, Pin(Montage, TEXT("Pose"), EGPD_Input) },
        { Pin(Upper, TEXT("Pose"), EGPD_Output), Pin(Layer, TEXT("BlendPoses_0"), EGPD_Input) },
        { Pin(Full, TEXT("Pose"), EGPD_Output), Pin(Select, TEXT("A"), EGPD_Input) },
        { Pin(Layer, TEXT("Pose"), EGPD_Output), Pin(Select, TEXT("B"), EGPD_Input) },
        { Pin(Getter, AlphaName, EGPD_Output), Pin(Select, TEXT("Alpha"), EGPD_Input) },
        { Pin(Select, TEXT("Pose"), EGPD_Output), RootIn } };
    TSet<FString> Expected = BeforeEdges;
    Expected.Remove(EdgeKey({ Endpoint(GuardOut), Endpoint(SlotIn) }));
    Expected.Remove(EdgeKey({ Endpoint(SlotOut), Endpoint(RootIn) }));
    GuardOut->BreakLinkTo(SlotIn); SlotOut->BreakLinkTo(RootIn);
    for (const auto& Pair : Links)
    {
        if (!Pair.Key || !Pair.Value || !Graph->GetSchema()->TryCreateConnection(Pair.Key, Pair.Value))
        { Error = TEXT("Unable to create required pose/alpha connection"); Rollback(); return false; }
        Expected.Add(EdgeKey({ Endpoint(Pair.Key), Endpoint(Pair.Value) }));
    }
    auto Preserved = [&]()
    {
        if (Graph->Nodes.Num() != OriginalNodes.Num() + Added.Num() || Added.Num() != 9
            || RuntimeText(Slot) != SlotText || RuntimeText(Rig) != RigText || RuntimeText(Guard) != GuardText)
        { return false; }
        for (UEdGraphNode* Node : OriginalNodes) { if (!Graph->Nodes.Contains(Node)) { return false; } }
        return SameEdges(Expected, EdgeSet(AllEdges(Graph)));
    };
    if (!Preserved() || !ValidateApplied(Graph, Guard, Slot, Root, Error))
    { if (Error.IsEmpty()) { Error = TEXT("Original nodes/settings or unrelated edges changed"); } Rollback(); return false; }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (!Compile(Blueprint, Report, TEXT("compile_passed")) || !Preserved()
        || !ValidateApplied(Graph, Guard, Slot, Root, Error))
    { if (Error.IsEmpty()) { Error = TEXT("Graph compile or post-compile preservation failed"); } Rollback(); return false; }
    Report->SetBoolField(TEXT("changed"), true);
    Report->SetBoolField(TEXT("idempotent"), false);
    Report->SetBoolField(TEXT("original_nodes_and_settings_preserved"), true);
    Report->SetNumberField(TEXT("added_node_count"), Added.Num());
    Report->SetObjectField(TEXT("after"), GraphReport(Graph));
    return true;
}
static void Run()
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("asset"), AssetPath);
    Report->SetBoolField(TEXT("saved"), false);
    Report->SetBoolField(TEXT("changed"), false);
    FString Error;
    const bool bSuccess = Apply(Report, Error);
    const FString Marker = bSuccess ? TEXT("RESULT=MOVING_ATTACK_UPPER_BODY_AUTHORING_PASSED")
        : TEXT("RESULT=MOVING_ATTACK_UPPER_BODY_AUTHORING_FAILED");
    Report->SetStringField(TEXT("result"), bSuccess ? TEXT("PASSED") : TEXT("FAILED"));
    Report->SetStringField(TEXT("marker"), Marker);
    Report->SetStringField(TEXT("error"), Error);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Folder = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/moving-upper-body-attack-20261009");
    IFileManager::Get().MakeDirectory(*Folder, true);
    const bool bReportWritten = FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("native-authoring.json")));
    if (bSuccess && bReportWritten) { UE_LOG(LogRogue10m, Display, TEXT("%s (compiled, not saved)"), *Marker); }
    else { UE_LOG(LogRogue10m, Error, TEXT("%s: %s; report_written=%d"), *Marker, *Error, bReportWritten); }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.AuthorMovingAttackUpperBody"),
    TEXT("Editor only: preserve locomotion below spine_01 during selected moving attacks; compile but never save."),
    FConsoleCommandDelegate::CreateStatic(&Run));

// The Python reflection layer does not expose this weak editor-node reference.
// Export the actual native cache identities without compiling or modifying the graph.
static bool Audit(TSharedPtr<FJsonObject> Report, FString& Error)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Requires editor with no PIE/SIE world"); return false; }
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, AssetPath);
    if (!Blueprint || !Blueprint->ParentClass || Blueprint->ParentClass->GetPathName() != ParentPath
        || !FindFProperty<FFloatProperty>(Blueprint->ParentClass, AlphaName))
    { Error = TEXT("Expected source AnimBP native parent/float alpha unavailable"); return false; }
    const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();
    Report->SetBoolField(TEXT("package_dirty_before"), bDirtyBefore);
    ON_SCOPE_EXIT { Report->SetBoolField(TEXT("package_dirty_after"), Blueprint->GetOutermost()->IsDirty()); };
    Report->SetStringField(TEXT("parent"), Blueprint->ParentClass->GetPathName());
    UEdGraph* Graph = nullptr;
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph* Candidate : AllGraphs)
    {
        if (Candidate && Candidate->GetFName() == TEXT("AnimGraph"))
        { if (Graph) { Error = TEXT("Ambiguous AnimGraph"); return false; } Graph = Candidate; }
    }
    if (!Graph) { Error = TEXT("Missing AnimGraph"); return false; }
    auto* Slot = Unique<UAnimGraphNode_Slot>(Graph);
    auto* Root = Unique<UAnimGraphNode_Root>(Graph);
    auto* Guard = GuardBlend(Graph);
    UEdGraphNode* Rig = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->GetClass()->GetFName() == TEXT("AnimGraphNode_ControlRig"))
        { if (Rig) { Error = TEXT("Ambiguous locomotion rig"); return false; } Rig = Node; }
    }
    UEdGraphPin* GuardB = Guard ? OnlyLink(Pin(Guard, TEXT("B"), EGPD_Input)) : nullptr;
    auto* Player = GuardB ? Cast<UAnimGraphNode_SequencePlayer>(GuardB->GetOwningNode()) : nullptr;
    if (!Slot || !Root || !Guard || !Rig || Slot->Node.SlotName != TEXT("DefaultSlot")
        || !Player || !RuntimeText(Player).Contains(TEXT(
            "/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle.A_Fighter_CombatIdle"))
        || !RuntimeText(Rig).Contains(TEXT("CR_Mannequin_FootIK"))
        || !Linked(Pin(Rig, TEXT("Pose"), EGPD_Output), Pin(Guard, TEXT("A"), EGPD_Input)))
    { Error = TEXT("Expected FootIK/standing guard/DefaultSlot contract unavailable"); return false; }
    if (!CheckMask(Blueprint, Error) || !ValidateApplied(Graph, Guard, Slot, Root, Error)) { return false; }
    Report->SetObjectField(TEXT("graph"), GraphReport(Graph));
    Report->SetBoolField(TEXT("foot_ik_before_guard_before_slot"), true);
    Report->SetBoolField(TEXT("native_cache_identities_verified"), true);
    Report->SetBoolField(TEXT("upper_and_lower_mask_weights_verified"), true);
    const bool bDirtyAfter = Blueprint->GetOutermost()->IsDirty();
    Report->SetBoolField(TEXT("package_dirty_after"), bDirtyAfter);
    if (bDirtyAfter != bDirtyBefore)
    { Error = TEXT("Read-only audit unexpectedly changed package dirtiness"); return false; }
    return true;
}
static void RunAudit()
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("asset"), AssetPath);
    Report->SetBoolField(TEXT("read_only"), true);
    Report->SetBoolField(TEXT("compiled"), false);
    Report->SetBoolField(TEXT("changed"), false);
    Report->SetBoolField(TEXT("saved"), false);
    FString Error;
    const bool bSuccess = Audit(Report, Error);
    const FString Marker = bSuccess ? TEXT("RESULT=MOVING_ATTACK_UPPER_BODY_AUDIT_PASSED")
        : TEXT("RESULT=MOVING_ATTACK_UPPER_BODY_AUDIT_FAILED");
    Report->SetStringField(TEXT("result"), bSuccess ? TEXT("PASSED") : TEXT("FAILED"));
    Report->SetStringField(TEXT("marker"), Marker);
    Report->SetStringField(TEXT("error"), Error);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Folder = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/moving-upper-body-attack-20261009");
    IFileManager::Get().MakeDirectory(*Folder, true);
    const bool bReportWritten = FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("native-audit.json")));
    if (bSuccess && bReportWritten) { UE_LOG(LogRogue10m, Display, TEXT("%s (read only)"), *Marker); }
    else { UE_LOG(LogRogue10m, Error, TEXT("%s: %s; report_written=%d"), *Marker, *Error, bReportWritten); }
}
static FAutoConsoleCommand AuditCommand(TEXT("Rogue10m.AuditMovingAttackUpperBody"),
    TEXT("Editor only: audit native moving-attack cache references, masks and FootIK order; never compile, modify or save."),
    FConsoleCommandDelegate::CreateStatic(&RunAudit));
}
#endif
