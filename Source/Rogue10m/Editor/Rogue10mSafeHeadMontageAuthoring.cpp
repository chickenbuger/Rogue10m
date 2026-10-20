// Editor-only reference substitution in new montage derivatives; never saves or alters originals.
#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

namespace Rogue10mSafeHeadMontageAuthoring
{
static const FString TargetFolder(TEXT("/Game/Rogue10m/Animation/Combat/HeadSafe/"));

static FString JsonText(const TSharedPtr<FJsonObject>& Object)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
    return Text;
}

static FString ObjectState(const UObject* Object, const UAnimMontage* Montage)
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
    return Text.Replace(*Montage->GetPathName(), TEXT("<Montage>"), ESearchCase::CaseSensitive);
}

static TSharedPtr<FJsonObject> SectionState(const UAnimMontage* Montage)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FCompositeSection& Section : Montage->CompositeSections)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Section.SectionName.ToString());
        Row->SetStringField(TEXT("next"), Section.NextSectionName.ToString());
        Row->SetNumberField(TEXT("time"), Section.GetTime());
        Row->SetNumberField(TEXT("link_method"), static_cast<int32>(Section.GetLinkMethod()));
        Row->SetNumberField(TEXT("slot"), Section.GetSlotIndex());
        Row->SetNumberField(TEXT("segment"), Section.GetSegmentIndex());
        TArray<TSharedPtr<FJsonValue>> Metadata;
        for (const auto& Item : Section.MetaData)
        { Metadata.Add(MakeShared<FJsonValueString>(ObjectState(Item.Get(), Montage))); }
        Row->SetArrayField(TEXT("metadata"), Metadata);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("sections"), Rows);
    return Result;
}

static TSharedPtr<FJsonObject> NotifyState(const UAnimMontage* Montage)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Rows;
    const UAnimSequenceBase* Sequence = Montage->SlotAnimTracks.IsEmpty()
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty() ? nullptr
        : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference().Get();
    for (const FAnimNotifyEvent& Notify : Montage->Notifies)
    {
        FString Text;
        FAnimNotifyEvent::StaticStruct()->ExportText(Text, &Notify, nullptr, nullptr, PPF_None, nullptr);
        Text = Text.Replace(*Montage->GetPathName(), TEXT("<Montage>"), ESearchCase::CaseSensitive);
        if (Sequence) { Text = Text.Replace(*Sequence->GetPathName(), TEXT("<Sequence>"), ESearchCase::CaseSensitive); }
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("event"), Text);
        Row->SetStringField(TEXT("notify_object"), ObjectState(Notify.Notify.Get(), Montage));
        Row->SetStringField(TEXT("notify_state_object"), ObjectState(Notify.NotifyStateClass.Get(), Montage));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("notifies"), Rows);
    return Result;
}

static void Run()
{
    auto Report = MakeShared<FJsonObject>();
    bool bPassed = GIsEditor && GEditor && !GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor;
    FString Error;
    const FString Folder = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/mouse-head-stable-locomotion-20261013");
    TArray<FString> Lines;
    bPassed = bPassed && FFileHelper::LoadFileToStringArray(Lines, *(Folder / TEXT("safe-montage-requests.txt")))
        && Lines.Num() > 0 && Lines.Num() < 80;
    TArray<TSharedPtr<FJsonValue>> Results;
    for (const FString& Line : Lines)
    {
        if (!bPassed) { break; }
        TArray<FString> Parts; Line.ParseIntoArray(Parts, TEXT("|"), false);
        if (Parts.Num() != 3 || !Parts[0].StartsWith(TEXT("/Game/Rogue10m/Animation/"))
            || !Parts[1].StartsWith(TargetFolder) || !Parts[2].StartsWith(TargetFolder) || Parts[0] == Parts[1])
        { Error = TEXT("Invalid scoped montage request"); bPassed = false; break; }
        UAnimMontage* Original = LoadObject<UAnimMontage>(nullptr, *Parts[0]);
        UAnimMontage* Target = LoadObject<UAnimMontage>(nullptr, *Parts[1]);
        UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Parts[2]);
        if (!Original || !Target || !Sequence || Original == Target || Original->GetSkeleton() != Sequence->GetSkeleton()
            || Target->SlotAnimTracks.Num() != 1 || Target->SlotAnimTracks[0].SlotName != TEXT("DefaultSlot")
            || Target->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1)
        { Error = TEXT("Unexpected montage derivative, skeleton or slot"); bPassed = false; break; }
        const bool bDodge = Target->GetName() == TEXT("AM_Safe_Dodge_Step");
        const float Length = Sequence->GetPlayLength();
        if ((!bDodge && FMath::Abs(Length - Original->GetPlayLength()) > 0.0001f)
            || (bDodge && (FMath::Abs(Length - 0.2f) > 0.0001f || !Original->Notifies.IsEmpty()
                || Original->CompositeSections.Num() != 1 || Original->CompositeSections[0].GetTime() != 0.0f)))
        { Error = TEXT("Unexpected original length/notify/section contract"); bPassed = false; break; }
        const auto OriginalSections = SectionState(Original);
        const auto OriginalNotifies = NotifyState(Original);
        if (JsonText(OriginalSections) != JsonText(SectionState(Target))
            || JsonText(OriginalNotifies) != JsonText(NotifyState(Target)))
        { Error = TEXT("Cloned montage did not preserve original sections/notifies"); bPassed = false; break; }
        Target->Modify();
        FAnimSegment& Segment = Target->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
        Segment.SetAnimReference(Sequence, false);
        Segment.StartPos = 0.0f; Segment.AnimStartTime = 0.0f; Segment.AnimEndTime = Length;
        Segment.AnimPlayRate = 1.0f; Segment.LoopingCount = 1;
        if (bDodge)
        {
            Target->SetCompositeLength(Length);
            Target->BlendIn.SetBlendTime(0.04f); Target->BlendOut.SetBlendTime(0.08f);
        }
        Target->RefreshCacheData(); Target->PostEditChange(); Target->MarkPackageDirty();
        bPassed = FMath::Abs(Target->GetPlayLength() - Length) < 0.0001f
            && URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Target);
        if (!bPassed) { Error = TEXT("Resulting montage is unsafe or has wrong length"); break; }
        const auto TargetSections = SectionState(Target);
        const auto TargetNotifies = NotifyState(Target);
        const bool bSectionsPreserved = JsonText(OriginalSections) == JsonText(TargetSections);
        const bool bNotifiesPreserved = JsonText(OriginalNotifies) == JsonText(TargetNotifies);
        if (!bSectionsPreserved || !bNotifiesPreserved)
        { Error = TEXT("Safe substitution changed authored sections or notify events"); bPassed = false; break; }
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("original"), Original->GetPathName());
        Row->SetStringField(TEXT("target"), Target->GetPathName());
        Row->SetStringField(TEXT("sequence"), Sequence->GetPathName());
        Row->SetNumberField(TEXT("length"), Length);
        Row->SetBoolField(TEXT("safe"), true);
        Row->SetBoolField(TEXT("sections_preserved"), bSectionsPreserved);
        Row->SetBoolField(TEXT("notifies_preserved"), bNotifiesPreserved);
        Row->SetObjectField(TEXT("sections_before"), OriginalSections);
        Row->SetObjectField(TEXT("sections_after"), TargetSections);
        Row->SetObjectField(TEXT("notifies_before"), OriginalNotifies);
        Row->SetObjectField(TEXT("notifies_after"), TargetNotifies);
        Results.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetStringField(TEXT("result"), bPassed ? TEXT("PASSED") : TEXT("FAILED"));
    Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("saved"), false);
    Report->SetArrayField(TEXT("montages"), Results);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const bool bWritten = FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("safe-montage-native.json")));
    if (bPassed && bWritten) { UE_LOG(LogRogue10m, Display, TEXT("RESULT=SAFE_HEAD_MONTAGES_PASSED count=%d"), Results.Num()); }
    else { UE_LOG(LogRogue10m, Error, TEXT("RESULT=SAFE_HEAD_MONTAGES_FAILED: %s"), *Error); }
}
static FAutoConsoleCommand ApplyCommand(TEXT("Rogue10m.ConfigureSafeHeadMontages"),
    TEXT("Editor only: connect scoped new safe montage/sequence derivatives from the prepared request file, never save."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
