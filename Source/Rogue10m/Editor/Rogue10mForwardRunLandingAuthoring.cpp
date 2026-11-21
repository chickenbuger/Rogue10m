// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateAliasNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rogue10m.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"

namespace Rogue10mForwardRunLandingAuthoring
{
static constexpr TCHAR BlueprintPath[] = TEXT("/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed");
static constexpr TCHAR SequencePath[] = TEXT("/Game/Rogue10m/Animation/Common/Airborne/A_CommonForwardRunLanding_L");
static const FName MovingGate(GET_MEMBER_NAME_CHECKED(URogue10mFighterCombatStanceAnimInstance, bUseForwardRunLanding));
static const FName SeedProperty(GET_MEMBER_NAME_CHECKED(URogue10mFighterCombatStanceAnimInstance, GroundLocomotionStartPosition));

template <typename T>
T* FindUniqueNode(const UEdGraph& Graph)
{
	T* Found = nullptr;
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (T* Match = Cast<T>(Node))
		{
			if (Found) { return nullptr; }
			Found = Match;
		}
	}
	return Found;
}

UAnimStateNodeBase* FindState(const UEdGraph& Machine, const FString& Name)
{
	for (UEdGraphNode* Node : Machine.Nodes)
	{
		if (UAnimStateNodeBase* State = Cast<UAnimStateNodeBase>(Node); State && State->GetStateName() == Name)
		{
			return State;
		}
	}
	return nullptr;
}

UAnimStateTransitionNode* ConnectStates(UEdGraph& Machine, UAnimStateNodeBase& From, UAnimStateNodeBase& To)
{
	TSet<const UEdGraphNode*> Previous;
	for (const UEdGraphNode* Node : Machine.Nodes) { Previous.Add(Node); }
	if (!Machine.GetSchema()->TryCreateConnection(From.GetOutputPin(), To.GetInputPin())) { return nullptr; }
	for (UEdGraphNode* Node : Machine.Nodes)
	{
		if (auto* Transition = Cast<UAnimStateTransitionNode>(Node); Transition && !Previous.Contains(Transition))
		{
			return Transition->GetPreviousState() == &From && Transition->GetNextState() == &To ? Transition : nullptr;
		}
	}
	return nullptr;
}

UK2Node_VariableGet* AddGetter(UEdGraph& Graph, FName Variable, int32 X, int32 Y)
{
	FGraphNodeCreator<UK2Node_VariableGet> Creator(Graph);
	UK2Node_VariableGet* Getter = Creator.CreateNode(false);
	Getter->VariableReference.SetSelfMember(Variable);
	Getter->NodePosX = X;
	Getter->NodePosY = Y;
	Creator.Finalize();
	return Getter;
}

bool ConnectRule(UAnimStateTransitionNode& Edge, FName Variable, bool bNegate = false)
{
	UEdGraph* Graph = Edge.GetBoundGraph();
	UAnimGraphNode_TransitionResult* Result = Graph ? FindUniqueNode<UAnimGraphNode_TransitionResult>(*Graph) : nullptr;
	UEdGraphPin* ResultPin = Result ? Result->FindPin(TEXT("bCanEnterTransition"), EGPD_Input) : nullptr;
	if (!ResultPin) { return false; }
	UK2Node_VariableGet* Getter = AddGetter(*Graph, Variable, -350, 0);
	UEdGraphPin* Output = Getter->FindPin(Variable, EGPD_Output);
	if (!Output) { return false; }
	if (bNegate)
	{
		const UFunction* Not = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool));
		if (!Not) { return false; }
		FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
		UK2Node_CallFunction* Negate = Creator.CreateNode(false);
		Negate->SetFromFunction(Not);
		Negate->NodePosX = -160;
		Creator.Finalize();
		UEdGraphPin* Input = Negate->FindPin(TEXT("A"), EGPD_Input);
		if (!Input || !Graph->GetSchema()->TryCreateConnection(Output, Input)) { return false; }
		Output = Negate->FindPin(TEXT("ReturnValue"), EGPD_Output);
	}
	return Output && Graph->GetSchema()->TryCreateConnection(Output, ResultPin);
}

bool AddPlayer(UAnimStateNode& State, UAnimSequence& Sequence)
{
	UEdGraph* Graph = State.GetBoundGraph();
	UAnimGraphNode_StateResult* Result = State.GetResultNodeInsideState();
	if (!Graph || !Result) { return false; }

	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);
	UAnimGraphNode_SequencePlayer* Player = Creator.CreateNode(false);
	Player->Node.SetSequence(&Sequence);
	Player->Node.SetLoopAnimation(false);
	Player->Node.SetGroupName(TEXT("Locomotion"));
	Player->Node.SetGroupRole(EAnimGroupRole::ExclusiveAlwaysLeader);
	Player->Node.SetGroupMethod(EAnimSyncMethod::SyncGroup);
	Player->Node.SetOverridePositionWhenJoiningSyncGroupAsLeader(true);
	Player->NodePosX = -330;
	Creator.Finalize();
	UEdGraphPin* Pose = Player->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* Destination = Result->FindPin(TEXT("Result"), EGPD_Input);
	return Pose && Destination && Graph->GetSchema()->TryCreateConnection(Pose, Destination);
}

bool BindSeed(UAnimGraphNode_BlendSpacePlayer& Player)
{
	const int32 Index = Player.ShowPinForProperties.IndexOfByPredicate([](const FOptionalPinFromProperty& Pin)
	{
		return Pin.PropertyName == TEXT("StartPosition") && Pin.bCanToggleVisibility;
	});
	if (Index == INDEX_NONE || Player.ShowPinForProperties[Index].bShowPin) { return false; }
	Player.SetPinVisibility(true, Index);
	UEdGraph* Graph = Player.GetGraph();
	UK2Node_VariableGet* Getter = AddGetter(*Graph, SeedProperty, Player.NodePosX - 300, Player.NodePosY + 280);
	UEdGraphPin* Source = Getter->FindPin(SeedProperty, EGPD_Output);
	UEdGraphPin* Target = Player.FindPin(TEXT("StartPosition"), EGPD_Input);
	return Source && Target && Graph->GetSchema()->TryCreateConnection(Source, Target);
}

bool Configure(UAnimBlueprint& Blueprint, UAnimSequence& Sequence)
{
	TArray<UEdGraph*> Graphs;
	Blueprint.GetAllGraphs(Graphs);
	UAnimationStateMachineGraph* Machine = nullptr;
	UAnimGraphNode_BlendSpacePlayer* Blend = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetName() == TEXT("Main States")) { Machine = Cast<UAnimationStateMachineGraph>(Graph); }
		if (Graph->GetName() == TEXT("Walk / Run")) { Blend = FindUniqueNode<UAnimGraphNode_BlendSpacePlayer>(*Graph); }
	}
	if (!Machine || !Blend || FindState(*Machine, TEXT("Forward Run Land"))) { return false; }
	UAnimStateNodeBase* GroundContact = FindState(*Machine, TEXT("To Land"));
	UAnimStateNodeBase* StockLand = FindState(*Machine, TEXT("Land"));
	UAnimStateAliasNode* Takeoff = Cast<UAnimStateAliasNode>(FindState(*Machine, TEXT("To Falling")));
	UAnimStateNodeBase* Locomotion = FindState(*Machine, TEXT("Locomotion"));
	if (!GroundContact || !StockLand || !Takeoff || !Locomotion) { return false; }
	const UAnimStateTransitionNode* StockEntry = nullptr;
	for (UEdGraphNode* Node : Machine->Nodes)
	{
		const auto* Edge = Cast<UAnimStateTransitionNode>(Node);
		if (Edge && Edge->GetPreviousState() == GroundContact && Edge->GetNextState() == StockLand) { StockEntry = Edge; }
	}
	const FAnimSyncMarker* Right = Sequence.AuthoredSyncMarkers.FindByPredicate([](const FAnimSyncMarker& Marker)
	{
		return Marker.MarkerName == TEXT("R");
	});
	if (!StockEntry || !Right || Right->Time <= 0.1f || Sequence.GetPlayLength() < Right->Time) { return false; }

	Blueprint.Modify();
	Machine->Modify();
	FGraphNodeCreator<UAnimStateNode> Creator(*Machine);
	UAnimStateNode* Running = Creator.CreateNode(false);
	Running->NodePosX = StockLand->NodePosX + 330;
	Running->NodePosY = StockLand->NodePosY + 150;
	Creator.Finalize();
	Running->OnRenameNode(TEXT("Forward Run Land"));
	if (!Takeoff->bGlobalAlias)
	{
		Takeoff->Modify();
		Takeoff->GetAliasedStates().Add(Running);
	}
	if (!AddPlayer(*Running, Sequence) || !BindSeed(*Blend)) { return false; }

	UAnimStateTransitionNode* Entry = ConnectStates(*Machine, *GroundContact, *Running);
	UAnimStateTransitionNode* Cancel = ConnectStates(*Machine, *Running, *Locomotion);
	UAnimStateTransitionNode* Handoff = ConnectStates(*Machine, *Running, *Locomotion);
	if (!Entry || !Cancel || !Handoff) { return false; }
	Entry->PriorityOrder = StockEntry->PriorityOrder - 1;
	Entry->CrossfadeDuration = StockEntry->CrossfadeDuration;
	Cancel->PriorityOrder = 1;
	Cancel->CrossfadeDuration = 0.1f;
	Handoff->PriorityOrder = 2;
	Handoff->CrossfadeDuration = 0.1f;
	Handoff->bAutomaticRuleBasedOnSequencePlayerInState = true;
	Handoff->AutomaticRuleTriggerTime = Sequence.GetPlayLength() - (Right->Time - Handoff->CrossfadeDuration);
	return ConnectRule(*Entry, MovingGate) && ConnectRule(*Cancel, MovingGate, true);
}

void Author()
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, SequencePath);
	if (!GIsEditor || !GEditor || GEditor->PlayWorld || !Blueprint || !Sequence
		|| Blueprint->ParentClass != URogue10mFighterCombatStanceAnimInstance::StaticClass()
		|| !Configure(*Blueprint, *Sequence))
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=FORWARD_RUN_LANDING_AUTHOR_FAILED graph configuration"));
		return;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Compiler;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Compiler);
	if (Compiler.NumErrors || Blueprint->Status == BS_Error)
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=FORWARD_RUN_LANDING_AUTHOR_FAILED blueprint compile errors=%d"), Compiler.NumErrors);
		return;
	}
	UE_LOG(LogRogue10m, Display, TEXT("RESULT=FORWARD_RUN_LANDING_AUTHOR_PASSED warnings=%d"), Compiler.NumWarnings);
}

FAutoConsoleCommand Command(TEXT("Rogue10m.AuthorForwardRunLanding"), TEXT("Configure and compile the forward landing branch only. Save is performed by the editor caller."),
	FConsoleCommandDelegate::CreateStatic(&Author));
}
#endif
