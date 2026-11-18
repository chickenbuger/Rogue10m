// Editor-only narrow graph authoring. The Python caller validates and saves the single affected AnimBP.
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

namespace Rogue10mWeaponAnimationAuthoring
{
static const TCHAR* AssetPath = TEXT("/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed");

static FName RateName;
static FName PropertyName;

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

static bool IsBound(UAnimGraphNode_Base* Player)
{
    UEdGraphPin* Rate = FindPin(Player, PropertyName, EGPD_Input);
    UEdGraphPin* Output = Rate && Rate->LinkedTo.Num() == 1 ? Rate->LinkedTo[0] : nullptr;
    const UK2Node_VariableGet* Getter = Output ? Cast<UK2Node_VariableGet>(Output->GetOwningNode()) : nullptr;
    return Getter && Getter->VariableReference.GetMemberName() == RateName
        && Output->PinName == RateName && Output->LinkedTo.Num() == 1 && Output->LinkedTo[0] == Rate;
}

static bool Configure(bool bApply, bool bStride, TSharedPtr<FJsonObject> Report, FString& Error)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Requires editor without PIE/SIE"); return false; }
    UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, AssetPath);
    if (!Blueprint || Blueprint->ParentClass != URogue10mFighterCombatStanceAnimInstance::StaticClass())
    { Error = TEXT("Unexpected common AnimBP parent"); return false; }
    RateName = bStride ? FName(TEXT("GroundLocomotionBlendSpace")) : FName(TEXT("FighterCombatStanceAnimation"));
    PropertyName = bStride ? FName(TEXT("BlendSpace")) : FName(TEXT("Sequence"));
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    UAnimGraphNode_Base* Player = nullptr;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* Candidate=Cast<UAnimGraphNode_Base>(Node);
            bool Selected=bStride && Graph->GetName()==TEXT("Walk / Run") && Cast<UAnimGraphNode_BlendSpacePlayer>(Candidate);
            if (!bStride && Cast<UAnimGraphNode_SequencePlayer>(Candidate))
            {
                // Identify the guard by its actual Alpha->FighterCombatStanceAlpha and B pose edge,
                // not every SequencePlayer in the locomotion/jump graph.
                for (UEdGraphPin* Output : Candidate->Pins)
                {
                    if (!Output || Output->Direction!=EGPD_Output) { continue; }
                    for (UEdGraphPin* Linked : Output->LinkedTo)
                    {
                        auto* Blend=Linked ? Cast<UAnimGraphNode_TwoWayBlend>(Linked->GetOwningNode()) : nullptr;
                        UEdGraphPin* Alpha=Blend ? FindPin(Blend,TEXT("Alpha"),EGPD_Input) : nullptr;
                        auto* Getter=Alpha && Alpha->LinkedTo.Num()==1 ? Cast<UK2Node_VariableGet>(Alpha->LinkedTo[0]->GetOwningNode()) : nullptr;
                        if (Blend && Linked->PinName==TEXT("B") && Getter && Getter->VariableReference.GetMemberName()==TEXT("FighterCombatStanceAlpha")) { Selected=true; }
                    }
                }
            }
            if (Selected)
            { if (Player) { Error=TEXT("Ambiguous grounded guard/BlendSpace player"); return false; } Player=Candidate; }
        }
    }
    if (!Player || (bStride && Player->GetGraph()->GetName() != TEXT("Walk / Run")))
    { Error = TEXT("Expected unique stance sequence or ground BlendSpace player"); return false; }
    Report->SetStringField(TEXT("player"), Player->GetPathName());
    Report->SetBoolField(TEXT("bound_before"), IsBound(Player));
    Report->SetBoolField(TEXT("saved"), false);
    if (IsBound(Player)) { Report->SetBoolField(TEXT("changed"), false); return true; }
    if (!bApply) { Error = TEXT("Native grounded play-rate property is not connected"); return false; }
    const int32 OptionalIndex = Player->ShowPinForProperties.IndexOfByPredicate([](const FOptionalPinFromProperty& Pin)
    { return Pin.PropertyName == PropertyName; });
    if (OptionalIndex == INDEX_NONE || !Player->ShowPinForProperties[OptionalIndex].bCanToggleVisibility)
    { Error = TEXT("No exposable PlayRate property"); return false; }
    if (UEdGraphPin* Existing = FindPin(Player, PropertyName, EGPD_Input))
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
    Getter->NodeComment = TEXT("Weapon-selected stance / directional locomotion");
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
    UEdGraphPin* Rate = FindPin(Player, PropertyName, EGPD_Input);
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

static bool SafeMontage(const UAnimMontage* Montage, const UAnimSequence* Sequence)
{
    return Montage && Sequence && Montage->GetSkeleton() && Montage->GetSkeleton()==Sequence->GetSkeleton()
        && Montage->SlotAnimTracks.Num()==1 && Montage->SlotAnimTracks[0].SlotName==FName(TEXT("DefaultSlot"))
        && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()==1
        && Montage->CompositeSections.Num()==1 && Montage->CompositeSections[0].SectionName==FName(TEXT("Default"))
        && Montage->CompositeSections[0].GetTime()==0.f && Montage->CompositeSections[0].NextSectionName.IsNone()
        && Montage->CompositeSections[0].MetaData.IsEmpty() && Montage->Notifies.IsEmpty();
}

static void Run()
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    bool Passed = true;
    struct FRequest { UAnimMontage* Montage; UAnimSequence* Sequence; float Start; float End; };
    TArray<FRequest> Parsed;
    TSet<FString> Expected;
    for (const TCHAR* Name : {TEXT("Primary01"),TEXT("Primary02"),TEXT("Primary03"),TEXT("Special"),TEXT("Charged")})
    { Expected.Add(FString(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/AM_GreatSword_"))+Name); }
    const FString Folder = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/sword-knuckle-20261110");
    FString Requests;
    if (Passed && FFileHelper::LoadFileToString(Requests, *(Folder / TEXT("montage-requests.txt"))))
    {
        TArray<FString> Lines; Requests.ParseIntoArrayLines(Lines);
        if (Lines.Num()!=5) { Passed=false; Error=TEXT("Expected five montage requests"); }
        for (const FString& Line : Lines)
        {
            if (!Passed) { break; }
            TArray<FString> Fields; Line.ParseIntoArray(Fields, TEXT("|"), false);
            if ((Fields.Num()!=2 && Fields.Num()!=4) || !Fields[0].StartsWith(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/")) || !Fields[1].StartsWith(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/"))) { Passed=false; Error=TEXT("Invalid montage request"); break; }
            UAnimMontage* Montage=LoadObject<UAnimMontage>(nullptr,*Fields[0]); UAnimSequence* Sequence=LoadObject<UAnimSequence>(nullptr,*Fields[1]);
            if (!Expected.Remove(Fields[0]) || !SafeMontage(Montage,Sequence)) { Passed=false; Error=TEXT("Unsafe or duplicate montage skeleton/slot/section/notify contract: ")+Fields[0]; break; }
            float SourceStart = 0.f, SourceEnd = Sequence->GetPlayLength();
            if (Fields.Num()==4 && (!FDefaultValueHelper::ParseFloat(Fields[2],SourceStart) || !FDefaultValueHelper::ParseFloat(Fields[3],SourceEnd))) { Passed=false; Error=TEXT("Malformed montage source cut"); break; }
            if (!FMath::IsFinite(SourceStart) || !FMath::IsFinite(SourceEnd) || SourceStart<0.f || SourceEnd<=SourceStart || SourceEnd>Sequence->GetPlayLength()+.0001f) { Passed=false; Error=TEXT("Invalid montage source cut"); break; }
            Parsed.Add({Montage,Sequence,SourceStart,SourceEnd});
        }
    }
    else if (Passed) { Passed=false; Error=TEXT("Missing montage requests"); }
    Report->SetBoolField(TEXT("montage_preflight_passed"),Passed);
    Report->SetNumberField(TEXT("preflight_montage_count"),Parsed.Num());
    if (Passed) { Passed=Configure(true,false,Report,Error) && Configure(true,true,Report,Error); }
    if (Passed)
    {
        for (const FRequest& Request : Parsed)
        {
            UAnimMontage* Montage=Request.Montage; UAnimSequence* Sequence=Request.Sequence;
            const float SourceStart=Request.Start, SourceEnd=Request.End;
            Montage->Modify(); FAnimSegment& Segment=Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
            Segment.SetAnimReference(Sequence,true); Segment.StartPos=0; Segment.AnimStartTime=SourceStart; Segment.AnimEndTime=SourceEnd; Segment.AnimPlayRate=1; Segment.LoopingCount=1;
            Montage->SetCompositeLength(Montage->CalculateSequenceLength());
            Montage->RefreshCacheData(); Montage->PostEditChange(); Montage->MarkPackageDirty();
            if(!SafeMontage(Montage,Sequence) || !FMath::IsNearlyEqual(Montage->GetPlayLength(),SourceEnd-SourceStart,.0001f)){Passed=false;Error=TEXT("Montage post-authoring contract/length failed");break;}
        }
    }
    if (Passed)
    {
        UBlendSpace* Stride=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/BS_GreatSword_Ground"));
        FString StrideText;
        if (!Stride || Stride->GetNumberOfBlendSamples()!=27 || !FFileHelper::LoadFileToString(StrideText,*(Folder/TEXT("stride-requests.txt")))) { Passed=false; Error=TEXT("Missing weapon stride requests or expected 27 samples (17 unique clips)"); }
        else
        {
            TMap<FString,FString> Mapping; TArray<FString> Lines; StrideText.ParseIntoArrayLines(Lines);
            for(const FString& Line:Lines){TArray<FString> Fields;Line.ParseIntoArray(Fields,TEXT("|"));if(Fields.Num()==2){Mapping.Add(Fields[0],Fields[1]);}}
            Stride->Modify();
            for(int32 I=0;I<Stride->GetNumberOfBlendSamples();++I)
            {
                const auto& Sample=Stride->GetBlendSample(I);const FString* Target=Sample.Animation?Mapping.Find(Sample.Animation->GetPathName()):nullptr;
                UAnimSequence* Clip=Target?LoadObject<UAnimSequence>(nullptr,**Target):nullptr;
                if(!Clip || Clip->GetSkeleton()!=Stride->GetSkeleton() || !Target->StartsWith(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/")) || !Stride->ReplaceSampleAnimation(I,Clip)){Passed=false;Error=TEXT("Weapon stride sample replacement failed");break;}
            }
            if(Passed){Stride->ValidateSampleData();Stride->ResampleData();Stride->PostEditChange();Stride->MarkPackageDirty();}
        }
    }
    TArray<TSharedPtr<FJsonValue>> MontageRows, SampleRows;
    for (const FRequest& Request : Parsed)
    {
        const UAnimMontage* Montage=Request.Montage;
        auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("montage"),Montage->GetPathName());
        Row->SetStringField(TEXT("requested_source"),Request.Sequence->GetPathName());
        Row->SetNumberField(TEXT("requested_start"),Request.Start); Row->SetNumberField(TEXT("requested_end"),Request.End);
        Row->SetNumberField(TEXT("length"),Montage->GetPlayLength());
        Row->SetNumberField(TEXT("notify_count"),Montage->Notifies.Num());
        Row->SetBoolField(TEXT("contract_valid"),SafeMontage(Montage,Request.Sequence));
        TArray<TSharedPtr<FJsonValue>> Sections;
        for (const FCompositeSection& Section : Montage->CompositeSections)
        {
            auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("name"),Section.SectionName.ToString());
            Item->SetNumberField(TEXT("time"),Section.GetTime()); Item->SetStringField(TEXT("next"),Section.NextSectionName.ToString());
            Item->SetNumberField(TEXT("metadata_count"),Section.MetaData.Num()); Sections.Add(MakeShared<FJsonValueObject>(Item));
        }
        Row->SetArrayField(TEXT("sections"),Sections);
        if (Montage->SlotAnimTracks.Num()==1 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()==1)
        {
            const FAnimSegment& Segment=Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
            Row->SetStringField(TEXT("slot"),Montage->SlotAnimTracks[0].SlotName.ToString());
            Row->SetStringField(TEXT("source"),Segment.GetAnimReference()?Segment.GetAnimReference()->GetPathName():TEXT("None"));
            Row->SetNumberField(TEXT("start"),Segment.AnimStartTime); Row->SetNumberField(TEXT("end"),Segment.AnimEndTime);
            Row->SetNumberField(TEXT("start_pos"),Segment.StartPos); Row->SetNumberField(TEXT("play_rate"),Segment.AnimPlayRate);
            Row->SetNumberField(TEXT("loop_count"),Segment.LoopingCount);
        }
        MontageRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (const UBlendSpace* Stride=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/BS_GreatSword_Ground")))
    {
        for(int32 I=0;I<Stride->GetNumberOfBlendSamples();++I)
        {
            const auto& Sample=Stride->GetBlendSample(I); auto Row=MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("index"),I); Row->SetStringField(TEXT("animation"),Sample.Animation?Sample.Animation->GetPathName():TEXT("None"));
            Row->SetNumberField(TEXT("x"),Sample.SampleValue.X); Row->SetNumberField(TEXT("y"),Sample.SampleValue.Y); Row->SetNumberField(TEXT("z"),Sample.SampleValue.Z);
            SampleRows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Report->SetArrayField(TEXT("montages"),MontageRows); Report->SetArrayField(TEXT("stride_samples"),SampleRows);
    Report->SetNumberField(TEXT("stride_sample_count"),SampleRows.Num());
    Report->SetBoolField(TEXT("passed"), Passed); Report->SetStringField(TEXT("error"),Error);
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); IFileManager::Get().MakeDirectory(*Folder,true);
    FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("weapon-animation-native.json")));
    UE_LOG(LogRogue10m,Display,TEXT("RESULT=WEAPON_ANIMATION_AUTHORING_%s %s"),Passed?TEXT("PASSED"):TEXT("FAILED"),*Error);
}
static void RefreshSwordPreview()
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { UE_LOG(LogRogue10m,Error,TEXT("Sword preview refresh requires editor without PIE/SIE")); return; }
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if (!World) { UE_LOG(LogRogue10m,Error,TEXT("Sword preview editor world missing")); return; }
    ASkeletalMeshActor* Found=nullptr;
    for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel()!=TEXT("QA_SwordPreview_20261110")) { continue; }
        if (Found || !It->HasAnyFlags(RF_Transient))
        { UE_LOG(LogRogue10m,Error,TEXT("Sword preview target must be unique and transient")); return; }
        Found=*It;
    }
    USkeletalMeshComponent* Mesh=Found?Found->GetSkeletalMeshComponent():nullptr;
    if (!Mesh || Mesh->GetAnimationMode()!=EAnimationMode::AnimationSingleNode)
    { UE_LOG(LogRogue10m,Error,TEXT("Sword preview target missing or not single-node")); return; }
    // Preview-only child: never evaluate arbitrary actor components or runtime bodies.
    USkeletalMeshComponent* PreviewBody=nullptr;
    TArray<USkeletalMeshComponent*> PreviewComponents;
    Found->GetComponents<USkeletalMeshComponent>(PreviewComponents);
    for (USkeletalMeshComponent* Candidate : PreviewComponents)
    {
        if (!Candidate || Candidate->GetFName()!=FName(TEXT("QAWeaponPreviewBody"))) { continue; }
        if (PreviewBody || Candidate==Mesh || Candidate->GetOwner()!=Found || Candidate->GetAttachParent()!=Mesh)
        { UE_LOG(LogRogue10m,Error,TEXT("Weapon preview body must be unique, same-owner and attached directly to source")); return; }
        PreviewBody=Candidate;
    }
    Mesh->TickAnimation(0.f,false);
    Mesh->RefreshBoneTransforms();
    Mesh->UpdateComponentToWorld();
    Mesh->MarkRenderDynamicDataDirty();
    Mesh->UpdateChildTransforms();
    if (PreviewBody)
    {
        PreviewBody->TickAnimation(0.f,false);
        PreviewBody->RefreshBoneTransforms();
        PreviewBody->UpdateComponentToWorld();
        PreviewBody->MarkRenderDynamicDataDirty();
        PreviewBody->UpdateChildTransforms();
    }
    UE_LOG(LogRogue10m,Display,TEXT("RESULT=SWORD_PREVIEW_REFRESHED"));
}
static FAutoConsoleCommand PreviewCommand(TEXT("Rogue10m.RefreshSwordPreview"),TEXT("Refresh the unique transient sword preview single-node pose without saving assets."),FConsoleCommandDelegate::CreateStatic(&RefreshSwordPreview));

static void ImportAlternatives()
{
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source/SK_MixamoXBot"));
    bool Passed=GIsEditor && Mesh;
    // AssetImportTask saved the mesh but did not save its generated Skeleton in the first
    // candidate import. Rebuild that task-local dependency from the mesh reference bones.
    if(Passed && !Mesh->GetSkeleton())
    {
        const FString SkeletonPath=TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source/SK_MixamoXBot_Skeleton");
        UPackage* Package=CreatePackage(*SkeletonPath);
        USkeleton* Skeleton=NewObject<USkeleton>(Package,TEXT("SK_MixamoXBot_Skeleton"),RF_Public|RF_Standalone);
        Passed=Skeleton && Skeleton->MergeAllBonesToBoneTree(Mesh,false);
        if(Passed){Skeleton->SetPreviewMesh(Mesh);Mesh->SetSkeleton(Skeleton);Skeleton->MarkPackageDirty();Mesh->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Skeleton);}
    }
    Passed=Passed && Mesh->GetSkeleton();
    const TCHAR* SourceNames[]={TEXT("Idle"),TEXT("Walk"),TEXT("Run"),TEXT("Primary01"),TEXT("Primary02"),TEXT("Primary03"),TEXT("Special"),TEXT("Charged")};
    int32 Rebound=0;
    for(const TCHAR* Name:SourceNames)
    {
        if(!Passed){break;}
        const FString Path=FString(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source/A_Source_"))+Name;
        UAnimSequence* Clip=LoadObject<UAnimSequence>(nullptr,*Path);
        if(!Clip){Passed=false;break;}
        if(Clip->GetSkeleton()!=Mesh->GetSkeleton())
        {
            Clip->SetSkeleton(Mesh->GetSkeleton());Clip->GetController().NotifyPopulated();Clip->PostEditChange();Clip->MarkPackageDirty();++Rebound;
        }
    }
    UE_LOG(LogRogue10m,Display,TEXT("Weapon source skeleton bindings rebound: %d"),Rebound);
    if(FParse::Param(FCommandLine::Get(),TEXT("RogueWeaponRepairSkeletonOnly")))
    { UE_LOG(LogRogue10m,Display,TEXT("RESULT=WEAPON_SOURCE_REPAIR_%s"),Passed?TEXT("PASSED"):TEXT("FAILED")); return; }
    const TCHAR* Names[]={TEXT("A_Source_AltSlash04"),TEXT("A_Source_AltSlash05")};
    const TCHAR* Files[]={TEXT("great sword slash (4).fbx"),TEXT("great sword slash (5).fbx")};
    for(int32 I=0;Passed && I<2;++I)
    {
        const FString Path=FString(TEXT("/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source/"))+Names[I];
        if(FindObject<UAnimSequence>(nullptr,*(Path+TEXT(".")+Names[I])) || LoadObject<UAnimSequence>(nullptr,*Path,nullptr,LOAD_NoWarn)){continue;}
        UPackage* Package=CreatePackage(*Path);
        UFbxAnimSequenceImportData* Data=NewObject<UFbxAnimSequenceImportData>();
        Data->bUseDefaultSampleRate=true;Data->bImportBoneTracks=true;Data->AnimationLength=FBXALIT_ExportedTime;
        const FString Filename=FString(TEXT("C:/Users/PC/Downloads/Rogue10m_Mixamo_GreatSword_20261110/"))+Files[I];
        UAnimSequence* Clip=UEditorEngine::ImportFbxAnimation(Mesh->GetSkeleton(),Package,Data,*Filename,Names[I],false);
        if(!Clip){Passed=false;break;}
        FAssetRegistryModule::AssetCreated(Clip);Clip->MarkPackageDirty();
    }
    UE_LOG(LogRogue10m,Display,TEXT("RESULT=WEAPON_SOURCE_IMPORT_%s"),Passed?TEXT("PASSED"):TEXT("FAILED"));
}
static FAutoConsoleCommand ImportCommand(TEXT("Rogue10m.ImportGreatSwordAlternatives"),TEXT("Import two task-local licensed animation-only FBX candidates through the official native animation importer. Caller saves."),FConsoleCommandDelegate::CreateStatic(&ImportAlternatives));
static FAutoConsoleCommand Command(TEXT("Rogue10m.ConfigureWeaponAnimations"),TEXT("Bind native weapon idle/stride and scoped greatsword montage segments. Caller validates and saves."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
