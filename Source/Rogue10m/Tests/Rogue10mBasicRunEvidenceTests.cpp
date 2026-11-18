#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mBasicRunSharedEvidenceTest,
    "Rogue10m.BasicRunEvidence.SharedReaderAppend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mBasicRunSharedEvidenceTest::RunTest(const FString& Parameters)
{
    // Exercise real native handles, not a model of the logger. Retain only an owned temp artifact.
    const FString Directory = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("Rogue10mEvidenceIO"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (!TestTrue(TEXT("Create unique owned temp directory"), IFileManager::Get().MakeDirectory(*Directory, true))) { return false; }
    const FString File = Directory / TEXT("shared-evidence.csv");
    FString Expected = TEXT("sample\n");
    if (!TestTrue(TEXT("Create fresh native evidence"), FFileHelper::SaveStringToFile(Expected, *File,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_NoReplaceExisting))) { return false; }

    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    TUniquePtr<IFileHandle> Reader(Platform.OpenRead(*File, true));
    if (!TestTrue(TEXT("Open a real observer handle allowing writes"), Reader.IsValid())) { return false; }
    int32 SuccessfulAppends = 0;
    for (int32 Index = 1; Index <= 100; ++Index)
    {
        const FString Row = FString::Printf(TEXT("%03d\n"), Index);
        if (FFileHelper::SaveStringToFile(Row, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead))
        { ++SuccessfulAppends; Expected += Row; }
        else { AddError(FString::Printf(TEXT("Native append failed while observer open at row %d"), Index)); break; }
    }
    TestEqual(TEXT("Every native append succeeds while the observer remains open"), SuccessfulAppends, 100);
    Reader.Reset();
    FString Actual;
    TestTrue(TEXT("Read completed native evidence"), FFileHelper::LoadFileToString(Actual, *File));
    TestEqual(TEXT("No native row loss, truncation or duplicate"), Actual, Expected);
    TestEqual(TEXT("Final row was persisted"), Actual.Right(4), FString(TEXT("100\n")));
    AddInfo(FString::Printf(TEXT("Owned temporary evidence retained: %s"), *File));
    return !HasAnyErrors();
}
#endif
