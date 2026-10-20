// Editor-only read access to the modern animation data model; never modifies or saves an asset.
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace Rogue10mWalkPalmAuthoring
{
static const TCHAR* BlendPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run");
static const TCHAR* TargetFolder = TEXT("/Game/Rogue10m/Animation/Locomotion/PalmReference/");
static const FName Exported[] = { TEXT("hand_l"), TEXT("hand_r"), TEXT("ik_hand_gun"), TEXT("ik_hand_l"), TEXT("ik_hand_r") };
static TSharedPtr<FJsonValue> J(double V) { return MakeShared<FJsonValueNumber>(V); }
static TSharedPtr<FJsonValue> J(const FString& V) { return MakeShared<FJsonValueString>(V); }
static FString Canonical(FString Text, const UAnimSequence* Sequence)
{ return Text.Replace(*Sequence->GetPathName(), TEXT("<Sequence>"), ESearchCase::CaseSensitive); }
template<class T> static FString StructText(const T& Value, const UAnimSequence* Sequence)
{
    FString Text;
    T::StaticStruct()->ExportText(Text, &Value, nullptr, nullptr, PPF_None, nullptr);
    return Canonical(Text, Sequence);
}
static FString ObjectText(const UObject* Object, const UAnimSequence* Sequence)
{
    if (!Object) { return TEXT("None"); }
    FString Text = Object->GetClass()->GetPathName();
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) { continue; }
        FString Value;
        It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(Object), nullptr, const_cast<UObject*>(Object), PPF_None);
        Text += TEXT("\n") + It->GetName() + TEXT("=") + Value;
    }
    return Canonical(Text, Sequence);
}
static FString Metadata(UAnimSequence* Sequence, const IAnimationDataModel* Model)
{
    FString Text = Sequence->GetSkeleton()->GetPathName() + StructText(Model->GetCurveData(), Sequence);
    for (const auto& Attribute : Model->GetAttributes()) { Text += StructText(Attribute, Sequence); }
    for (const auto& Notify : Sequence->Notifies)
    { Text += StructText(Notify, Sequence) + ObjectText(Notify.Notify.Get(), Sequence) + ObjectText(Notify.NotifyStateClass.Get(), Sequence); }
    for (const auto& Marker : Sequence->AuthoredSyncMarkers) { Text += StructText(Marker, Sequence); }
    for (const TCHAR* Name : { TEXT("RateScale"), TEXT("bLoop"), TEXT("Interpolation"), TEXT("bEnableRootMotion"), TEXT("RootMotionRootLock"),
        TEXT("bForceRootLock"), TEXT("bUseNormalizedRootMotionScale"), TEXT("bRootMotionSettingsCopiedFromMontage"),
        TEXT("AdditiveAnimType"), TEXT("RefPoseType"), TEXT("RefFrameIndex"), TEXT("RefPoseSeq"), TEXT("RetargetSource"),
        TEXT("RetargetSourceAsset"), TEXT("RetargetSourceAssetReferencePose"), TEXT("BoneCompressionSettings"),
        TEXT("CurveCompressionSettings"), TEXT("VariableFrameStrippingSettings"), TEXT("AnimNotifyTracks") })
    {
        FProperty* Property = FindFProperty<FProperty>(Sequence->GetClass(), Name);
        if (!Property) { return TEXT("<Missing metadata property>") + FString(Name); }
        FString Value;
        Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Sequence), nullptr, Sequence, PPF_None);
        Text += TEXT("\n") + FString(Name) + TEXT("=") + Canonical(Value, Sequence);
    }
    return Text;
}
static bool Sources(TArray<UAnimSequence*>& Sequences, UBlendSpace*& Blend, FString& Error)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Requires editor without PIE/SIE"); return false; }
    Blend = LoadObject<UBlendSpace>(nullptr, BlendPath);
    if (!Blend || Blend->GetBlendSamples().Num() != 27) { Error = TEXT("Expected original 27-sample BlendSpace"); return false; }
    for (const FBlendSample& Sample : Blend->GetBlendSamples())
    {
        UAnimSequence* Sequence = Sample.Animation;
        if (!Sequence) { Error = TEXT("Missing original sample animation"); return false; }
        if (Sequence->GetFName() == TEXT("MM_Idle")) { continue; }
        const FString Path = Sequence->GetPathName();
        if (!Path.StartsWith(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/"))
            && !Path.StartsWith(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/")))
        { Error = TEXT("Unexpected moving sample: ") + Path; return false; }
        Sequences.AddUnique(Sequence);
    }
    if (Sequences.Num() != 16) { Error = TEXT("Expected 16 unique original moving sequences"); return false; }
    Sequences.Sort([](const UAnimSequence& A, const UAnimSequence& B) { return A.GetPathName() < B.GetPathName(); });
    return true;
}
static TSharedRef<FJsonObject> SequenceInfo(UAnimSequence* Sequence, const IAnimationDataModel* Model)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("asset"), Sequence->GetPathName());
    Row->SetStringField(TEXT("target"), FString(TargetFolder) + Sequence->GetName() + TEXT("_PalmReference"));
    Row->SetStringField(TEXT("model_class"), Sequence->GetDataModelInterface().GetObject()->GetClass()->GetPathName());
    Row->SetNumberField(TEXT("length"), Model->GetPlayLength());
    Row->SetNumberField(TEXT("frames"), Model->GetNumberOfFrames());
    Row->SetNumberField(TEXT("keys"), Model->GetNumberOfKeys());
    auto Rate = MakeShared<FJsonObject>();
    Rate->SetNumberField(TEXT("numerator"), Model->GetFrameRate().Numerator);
    Rate->SetNumberField(TEXT("denominator"), Model->GetFrameRate().Denominator);
    Row->SetObjectField(TEXT("frame_rate"), Rate);
    TArray<FName> Names; Model->GetBoneTrackNames(Names);
    TArray<TSharedPtr<FJsonValue>> JsonNames;
    for (FName Name : Names) { JsonNames.Add(J(Name.ToString())); }
    Row->SetArrayField(TEXT("track_names"), JsonNames);
    Row->SetNumberField(TEXT("float_curves"), Model->GetNumberOfFloatCurves());
    Row->SetNumberField(TEXT("transform_curves"), Model->GetNumberOfTransformCurves());
    Row->SetNumberField(TEXT("attributes"), Model->GetNumberOfAttributes());
    Row->SetNumberField(TEXT("notifies"), Sequence->Notifies.Num());
    Row->SetNumberField(TEXT("sync_markers"), Sequence->AuthoredSyncMarkers.Num());
    return Row;
}
static bool ReadKeys(UAnimSequence* Sequence, FName Bone, TArray<FTransform>& Keys, FString& Error)
{
    IAnimationDataModel* Model = Sequence->GetDataModel();
    if (!Model || !Model->IsValidBoneTrackName(Bone)) { Error = TEXT("Missing native bone track: ") + Bone.ToString(); return false; }
    Model->GetBoneTrackTransforms(Bone, Keys);
    if (Keys.Num() != Model->GetNumberOfKeys() || Keys.Num() < 2 || Keys.Num() > 10000)
    { Error = TEXT("Invalid original model key count: ") + Bone.ToString(); return false; }
    for (const FTransform& Key : Keys)
    { if (Key.ContainsNaN() || !Key.GetRotation().IsNormalized()) { Error = TEXT("Invalid native transform: ") + Bone.ToString(); return false; } }
    return true;
}
static bool Export(UAnimSequence* Source, TSharedRef<FJsonObject> Row, FString& Error)
{
    auto Tracks = MakeShared<FJsonObject>();
    for (FName Bone : Exported)
    {
        TArray<FTransform> Keys; if (!ReadKeys(Source, Bone, Keys, Error)) { return false; }
        TArray<TSharedPtr<FJsonValue>> P, Q, S;
        for (const FTransform& Key : Keys)
        {
            const FVector V = Key.GetTranslation(), Scale = Key.GetScale3D(); const FQuat R = Key.GetRotation();
            P.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ J(V.X), J(V.Y), J(V.Z) }));
            Q.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ J(R.X), J(R.Y), J(R.Z), J(R.W) }));
            S.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ J(Scale.X), J(Scale.Y), J(Scale.Z) }));
        }
        auto Track = MakeShared<FJsonObject>(); Track->SetArrayField(TEXT("p"), P); Track->SetArrayField(TEXT("q"), Q); Track->SetArrayField(TEXT("s"), S);
        Tracks->SetObjectField(Bone.ToString(), Track);
    }
    Row->SetObjectField(TEXT("tracks"), Tracks); return true;
}
static bool Audit(UAnimSequence* Source, TSharedRef<FJsonObject> Row, FString& Error)
{
    UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *(FString(TargetFolder) + Source->GetName() + TEXT("_PalmReference")));
    IAnimationDataModel* A = Source->GetDataModel(); IAnimationDataModel* B = Target ? Target->GetDataModel() : nullptr;
    if (!A || !B || !Source->GetSkeleton() || !Target->GetSkeleton()) { Error = TEXT("Missing source/target skeleton or data model"); return false; }
    const bool Dirty = Target->GetOutermost()->IsDirty(); TArray<FName> NamesA, NamesB; A->GetBoneTrackNames(NamesA); B->GetBoneTrackNames(NamesB);
    const FString MetaA = Metadata(Source, A), MetaB = Metadata(Target, B);
    const bool Preserved = A->GetFrameRate() == B->GetFrameRate() && A->GetNumberOfFrames() == B->GetNumberOfFrames()
        && A->GetNumberOfKeys() == B->GetNumberOfKeys() && FMath::Abs(A->GetPlayLength() - B->GetPlayLength()) < 1.e-8
        && NamesA == NamesB && MetaA == MetaB && !MetaA.Contains(TEXT("<Missing metadata property>"));
    Row->SetBoolField(TEXT("metadata_and_track_order_preserved"), Preserved);
    if (!Preserved) { Error = TEXT("Metadata, timing, skeleton, curves, notifies, markers, attributes or track order changed"); return false; }
    double MaxP = 0, MaxS = 0, MaxR = 0, MaxEditedR = 0; int64 Count = 0;
    for (FName Bone : NamesA)
    {
        TArray<FTransform> KA, KB; if (!ReadKeys(Source, Bone, KA, Error) || !ReadKeys(Target, Bone, KB, Error)) { return false; }
        const bool AllowR = Bone == TEXT("hand_l") || Bone == TEXT("hand_r");
        for (int32 I = 0; I < KA.Num(); ++I)
        {
            const double P = (KA[I].GetTranslation() - KB[I].GetTranslation()).GetAbsMax();
            const double S = (KA[I].GetScale3D() - KB[I].GetScale3D()).GetAbsMax();
            const double R = FMath::RadiansToDegrees(KA[I].GetRotation().AngularDistance(KB[I].GetRotation()));
            MaxP = FMath::Max(MaxP, P); MaxS = FMath::Max(MaxS, S);
            if (!AllowR) { MaxR = FMath::Max(MaxR, R); } else { MaxEditedR = FMath::Max(MaxEditedR, R); }
            ++Count;
        }
    }
    Row->SetNumberField(TEXT("native_transform_pairs"), Count); Row->SetNumberField(TEXT("protected_position_max_cm"), MaxP);
    Row->SetNumberField(TEXT("scale_max_error"), MaxS); Row->SetNumberField(TEXT("protected_rotation_max_deg"), MaxR);
    Row->SetNumberField(TEXT("allowed_rotation_max_deg"), MaxEditedR);
    Row->SetBoolField(TEXT("target_package_dirty_before"), Dirty); Row->SetBoolField(TEXT("target_package_dirty_after"), Target->GetOutermost()->IsDirty());
    if (MaxP > 1.e-5 || MaxS > 1.e-5 || MaxR >= .001 || Dirty != Target->GetOutermost()->IsDirty())
    { Error = TEXT("Protected native key channels or package dirtiness changed"); return false; }
    return true;
}
static void Run(bool bAudit)
{
    auto Report = MakeShared<FJsonObject>(); FString Error; TArray<UAnimSequence*> Sequences; UBlendSpace* Blend = nullptr;
    bool Passed = Sources(Sequences, Blend, Error); const bool BlendDirty = Blend && Blend->GetOutermost()->IsDirty();
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (UAnimSequence* Source : Sequences)
    {
        if (!Passed) { break; } const bool Dirty = Source->GetOutermost()->IsDirty(); IAnimationDataModel* Model = Source->GetDataModel();
        if (!Model || !Source->GetSkeleton()) { Passed = false; Error = TEXT("Missing original source model/skeleton"); break; }
        auto Row = SequenceInfo(Source, Model); Passed = bAudit ? Audit(Source, Row, Error) : Export(Source, Row, Error);
        Row->SetBoolField(TEXT("source_package_dirty_before"), Dirty); Row->SetBoolField(TEXT("source_package_dirty_after"), Source->GetOutermost()->IsDirty());
        if (Dirty != Source->GetOutermost()->IsDirty()) { Passed = false; Error = TEXT("Source package dirtiness changed"); }
        Row->SetStringField(TEXT("result"), Passed ? TEXT("PASSED") : TEXT("FAILED")); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (Blend && BlendDirty != Blend->GetOutermost()->IsDirty()) { Passed = false; Error = TEXT("Original BlendSpace dirtiness changed"); }
    Report->SetBoolField(TEXT("read_only"), true); Report->SetBoolField(TEXT("saved_assets"), false);
    Report->SetBoolField(TEXT("only_hand_rotations_allowed"), bAudit);
    Report->SetStringField(TEXT("blendspace"), BlendPath); Report->SetArrayField(TEXT("sequences"), Rows);
    Report->SetStringField(TEXT("result"), Passed ? TEXT("PASSED") : TEXT("FAILED")); Report->SetStringField(TEXT("error"), Error);
    FString Json; const bool Serialized = FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Folder = FPaths::ProjectDir() / TEXT("tmp/walking-palm-reference-20261009"); IFileManager::Get().MakeDirectory(*Folder, true);
    const bool Written = Serialized && FFileHelper::SaveStringToFile(Json, *(Folder / (bAudit ? TEXT("native-key-audit.json") : TEXT("source-keys.json"))));
    const FString Marker = FString(TEXT("RESULT=WALK_PALM_")) + (bAudit ? TEXT("KEY_AUDIT_") : TEXT("KEY_EXPORT_")) + (Passed && Written ? TEXT("PASSED") : TEXT("FAILED"));
    if (Passed && Written) { UE_LOG(LogRogue10m, Display, TEXT("%s sequences=%d (read only)"), *Marker, Rows.Num()); }
    else { UE_LOG(LogRogue10m, Error, TEXT("%s: %s; written=%d"), *Marker, *Error, Written); }
}
static void RunExport() { Run(false); }
static void RunAudit() { Run(true); }
static FAutoConsoleCommand ExportCommand(TEXT("Rogue10m.ExportWalkPalmKeys"), TEXT("Editor only: export modern original hand/IK keys; never edit/save assets."), FConsoleCommandDelegate::CreateStatic(&RunExport));
static FAutoConsoleCommand AuditCommand(TEXT("Rogue10m.AuditWalkPalmKeys"), TEXT("Editor only: compare every original/corrected locomotion key and metadata; never edit/save assets."), FConsoleCommandDelegate::CreateStatic(&RunAudit));
}
#endif
