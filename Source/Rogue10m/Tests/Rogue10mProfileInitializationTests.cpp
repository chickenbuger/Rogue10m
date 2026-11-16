// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProfileInitializationTestCharacter.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mProfileInitializationTests
{
class FMemoryHandle final : public IFileHandle
{
	TSharedRef<TArray<uint8>> Bytes;
	int64 Position = 0;
	bool bWritable;
public:
	FMemoryHandle(TSharedRef<TArray<uint8>> InBytes, bool bInWritable) : Bytes(InBytes), bWritable(bInWritable) {}
	virtual int64 Tell() override { return Position; }
	virtual int64 Size() override { return Bytes->Num(); }
	virtual bool Seek(int64 Offset) override { if (Offset < 0 || Offset > Bytes->Num()) { return false; } Position = Offset; return true; }
	virtual bool SeekFromEnd(int64 Offset = 0) override { return Seek(Bytes->Num() + Offset); }
	virtual bool ReadAt(uint8* Dest, int64 Count, int64 Offset) override
	{
		if (Count < 0 || Offset < 0 || Offset > Bytes->Num() || Count > Bytes->Num() - Offset) { return false; }
		if (Count) { FMemory::Memcpy(Dest, Bytes->GetData() + Offset, Count); } return true;
	}
	virtual bool Read(uint8* Dest, int64 Count) override { if (!ReadAt(Dest, Count, Position)) { return false; } Position += Count; return true; }
	virtual bool Write(const uint8* Src, int64 Count) override
	{
		if (!bWritable || Count < 0 || Position > MAX_int32 - Count) { return false; }
		Bytes->SetNum(FMath::Max<int64>(Bytes->Num(), Position + Count));
		if (Count) { FMemory::Memcpy(Bytes->GetData() + Position, Src, Count); } Position += Count; return true;
	}
	virtual bool Flush(bool = false) override { return true; }
	virtual bool Truncate(int64 NewSize) override { if (!bWritable || NewSize < 0 || NewSize > MAX_int32) { return false; } Bytes->SetNum(NewSize); Position = FMath::Min(Position, NewSize); return true; }
};

/** Game-thread scoped exact-slot overlay. All other ordinary files forward unchanged.
 * Unknown SaveGames accesses fail closed so a fixture bug cannot touch a production slot.
 * No existing world, subsystem private field or SaveGame property is inspected or edited.
 */
class FMemorySaves final : public IPlatformFile
{
	IPlatformFile& Lower;
	FString OriginalCommandLine = FCommandLine::Get();
	FString SaveRoot = Normalize(*(FPaths::ProjectSavedDir() / TEXT("SaveGames")));
	TSet<FString> Allowed;
	TMap<FString, TSharedPtr<TArray<uint8>>> Files;
	static FString Normalize(const TCHAR* Path) { FString Result = FPaths::ConvertRelativePathToFull(Path); FPaths::NormalizeFilename(Result); FPaths::CollapseRelativeDirectories(Result); return Result.ToLower(); }
	bool IsSave(const TCHAR* Path) const { return Normalize(Path).StartsWith(SaveRoot + TEXT("/")); }
	bool IsAllowed(const TCHAR* Path) const { return Allowed.Contains(Normalize(Path)); }
	bool IsRoot(const TCHAR* Path) const { return Normalize(Path) == SaveRoot; }
	bool Reject(const TCHAR* Path) { if (IsSave(Path) && !IsAllowed(Path)) { ++UnexpectedSaveAccesses; return true; } return false; }
public:
	FString ProfileSlot, KeySlot, ProgressionPrefix;
	int32 MemoryWrites = 0, UnexpectedSaveAccesses = 0;
	FMemorySaves() : Lower(FPlatformFileManager::Get().GetPlatformFile())
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ProfileSlot = TEXT("Rogue10m_Town_QA_ProfileInit_") + Suffix;
		KeySlot = TEXT("Rogue10m_Fighter_QA_ProfileInit_") + Suffix;
		ProgressionPrefix = TEXT("Rogue10m_Progression_QA_ProfileInit_") + Suffix;
		Allow(ProfileSlot); Allow(KeySlot);
		// Put scoped switches first: FParse::Value consumes the first occurrence.
		FCommandLine::Set(*(FString::Printf(TEXT("-RogueProfileSaveSlot=%s -RogueSkillKeySaveSlot=%s -RogueProgressionSavePrefix=%s "), *ProfileSlot, *KeySlot, *ProgressionPrefix) + OriginalCommandLine));
		FPlatformFileManager::Get().SetPlatformFile(*this);
	}
	~FMemorySaves() { FPlatformFileManager::Get().SetPlatformFile(Lower); FCommandLine::Set(*OriginalCommandLine); }
	void Allow(const FString& Slot) { Allowed.Add(Normalize(*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"))))); }
	void AllowProgression(const FGuid& Id) { const FString Base = ProgressionPrefix + TEXT("_") + Id.ToString(EGuidFormats::Digits); Allow(Base); Allow(Base + TEXT("_v4")); Allow(Base + TEXT("_v5")); }
	virtual bool Initialize(IPlatformFile*, const TCHAR*) override { return true; }
	virtual IPlatformFile* GetLowerLevel() override { return &Lower; }
	virtual void SetLowerLevel(IPlatformFile*) override {}
	virtual const TCHAR* GetName() const override { return TEXT("Rogue10mProfileMemorySaves"); }
	virtual bool FileExists(const TCHAR* P) override { if (Reject(P)) { return false; } return IsAllowed(P) ? Files.Contains(Normalize(P)) : Lower.FileExists(P); }
	virtual int64 FileSize(const TCHAR* P) override { if (Reject(P)) { return -1; } const auto* B = Files.Find(Normalize(P)); return IsAllowed(P) ? (B ? (*B)->Num() : -1) : Lower.FileSize(P); }
	virtual bool DeleteFile(const TCHAR* P) override { if (Reject(P)) { return false; } return IsAllowed(P) ? Files.Remove(Normalize(P)) > 0 : Lower.DeleteFile(P); }
	virtual bool IsReadOnly(const TCHAR* P) override { return IsSave(P) ? false : Lower.IsReadOnly(P); }
	virtual bool SetReadOnly(const TCHAR* P, bool Value) override { if (Reject(P)) { return false; } return IsAllowed(P) || Lower.SetReadOnly(P, Value); }
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{ if (Reject(To) || Reject(From)) { return false; } if (IsSave(To) || IsSave(From)) { return false; } return Lower.MoveFile(To, From); }
	virtual FDateTime GetTimeStamp(const TCHAR* P) override { return IsSave(P) ? FDateTime::MinValue() : Lower.GetTimeStamp(P); }
	virtual FDateTime GetAccessTimeStamp(const TCHAR* P) override { return IsSave(P) ? FDateTime::MinValue() : Lower.GetAccessTimeStamp(P); }
	virtual void SetTimeStamp(const TCHAR* P, FDateTime Value) override { if (!IsSave(P)) { Lower.SetTimeStamp(P, Value); } }
	virtual FString GetFilenameOnDisk(const TCHAR* P) override { return IsSave(P) ? FString(P) : Lower.GetFilenameOnDisk(P); }
	virtual IFileHandle* OpenRead(const TCHAR* P, bool Write = false) override
	{ if (Reject(P)) { return nullptr; } const auto* B = Files.Find(Normalize(P)); return IsAllowed(P) ? (B ? new FMemoryHandle(B->ToSharedRef(), false) : nullptr) : Lower.OpenRead(P, Write); }
	virtual IFileHandle* OpenWrite(const TCHAR* P, bool Append = false, bool Read = false) override
	{
		if (Reject(P)) { return nullptr; } if (!IsAllowed(P)) { return Lower.OpenWrite(P, Append, Read); }
		auto& B = Files.FindOrAdd(Normalize(P)); if (!B) { B = MakeShared<TArray<uint8>>(); } if (!Append) { B->Reset(); }
		++MemoryWrites; auto* Handle = new FMemoryHandle(B.ToSharedRef(), true); if (Append) { Handle->SeekFromEnd(); } return Handle;
	}
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* P, bool Write = false) override { return IsSave(P) ? nullptr : Lower.OpenAsyncRead(P, Write); }
	virtual bool DirectoryExists(const TCHAR* P) override { return IsRoot(P) || (!IsSave(P) && Lower.DirectoryExists(P)); }
	virtual bool CreateDirectory(const TCHAR* P) override { return IsRoot(P) || (!IsSave(P) && Lower.CreateDirectory(P)); }
	virtual bool DeleteDirectory(const TCHAR* P) override { return !IsRoot(P) && !IsSave(P) && Lower.DeleteDirectory(P); }
	virtual FFileStatData GetStatData(const TCHAR* P) override
	{ if (IsRoot(P)) { return FFileStatData(FDateTime(), FDateTime(), FDateTime(), -1, true, false); } if (IsSave(P)) { const int64 Size = FileSize(P); return Size < 0 ? FFileStatData() : FFileStatData(FDateTime(), FDateTime(), FDateTime(), Size, false, false); } return Lower.GetStatData(P); }
	virtual bool IterateDirectory(const TCHAR* P, FDirectoryVisitor& Visitor) override { return IsRoot(P) || (!IsSave(P) && Lower.IterateDirectory(P, Visitor)); }
	virtual bool IterateDirectoryStat(const TCHAR* P, FDirectoryStatVisitor& Visitor) override { return IsRoot(P) || (!IsSave(P) && Lower.IterateDirectoryStat(P, Visitor)); }
};

struct FFixture
{
	FMemorySaves Saves;
	TStrongObjectPtr<UGameInstance> Instance;
	UWorld* World = nullptr;
	APlayerController* Controller = nullptr;
	URogue10mCharacterProfileSubsystem* Profiles = nullptr;
	URogue10mProgressionSubsystem* Progression = nullptr;
	explicit FFixture()
	{
		Instance.Reset(NewObject<UGameInstance>(GEngine));
		Instance->InitializeStandalone(MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ProfileInitialization")));
		World = Instance->GetWorld();
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		if (!World->SetGameMode(URL)) { return; }
		World->GetAuthGameMode()->PlayerStateClass = ARogue10mPlayerState::StaticClass();
		World->InitializeActorsForPlay(URL);
		Controller = World->SpawnActor<APlayerController>();
		if (Controller) { Controller->SetAsLocalPlayerController(); }
		Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		Progression = Instance->GetSubsystem<URogue10mProgressionSubsystem>();
	}
	~FFixture() { Cleanup(); }
	void Cleanup()
	{
		if (World) { World->DestroyWorld(false); }
		if (Instance.IsValid()) { Instance->Shutdown(); }
		if (World) { GEngine->DestroyWorldContext(World); }
		World = nullptr; Instance.Reset();
	}
	ARogue10mProfileInitializationTestCharacter* Spawn()
	{
		FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARogue10mProfileInitializationTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mProfileInitializationTest,
	"Rogue10m.CharacterProfile.PossessionInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRogue10mProfileInitializationTest::RunTest(const FString&)
{
	using namespace Rogue10mProfileInitializationTests;
	if (!TestTrue(TEXT("Synchronous isolated fixture runs unattended on game thread"), IsInGameThread() && FApp::IsUnattended()) || !GEngine) { return false; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.OwningGameInstance)
		{ AddError(TEXT("Profile fixture must run in an isolated process without an existing GameInstance")); return false; }
	}
	IPlatformFile* OriginalFiles = &FPlatformFileManager::Get().GetPlatformFile();
	const FString OriginalArgs = FCommandLine::Get();
	{
		FFixture F;
		if (!TestNotNull(TEXT("Transient controller"), F.Controller) || !TestNotNull(TEXT("Native profile subsystem"), F.Profiles)
			|| !TestNotNull(TEXT("Native progression subsystem"), F.Progression)) { return false; }
		TestEqual(TEXT("Memory-only profile fixture starts empty"), F.Profiles->GetProfileCount(), 0);
		auto* Default = F.Spawn(); if (!TestNotNull(TEXT("Default native pawn"), Default)) { return false; }
		F.Controller->Possess(Default);
		TestEqual(TEXT("No selection preserves default identity"), Default->GetCharacterDisplayName().ToString(), GetDefault<ARogue10mPlayerState>()->GetCharacterDisplayName().ToString());
		FRogue10mCharacterProfile A;
		if (!TestTrue(TEXT("Public normal profile creation"), F.Profiles->CreateProfile(TEXT("ProfileAlpha"), {}, A))) { return false; }
		F.Saves.AllowProgression(A.ProfileId);
		auto* First = F.Spawn(); if (!TestNotNull(TEXT("Selected native pawn"), First)) { return false; }
		F.Controller->Possess(First); // Actual virtual dispatch, without direct profile application.
		TestEqual(TEXT("Normal initial possession applies selected name"), First->GetCharacterDisplayName().ToString(), A.CharacterName);
		TestTrue(TEXT("Pawn receives the controller-owned native PlayerState"), First->GetPlayerState() == F.Controller->PlayerState && First->GetPlayerState<ARogue10mPlayerState>());
		First->DispatchBeginPlay();
		TestTrue(TEXT("Normal BeginPlay attaches selected profile GUID"), F.Progression->IsAttachedTo(*First) && F.Progression->GetAttachedProfileId() == A.ProfileId);
		F.Progression->DetachCharacter(*First, false); // End this isolated session before selecting another profile.
		FRogue10mCharacterProfile B;
		if (!TestTrue(TEXT("Create a second normal profile"), F.Profiles->CreateProfile(TEXT("ProfileBeta"), {}, B))) { return false; }
		F.Saves.AllowProgression(B.ProfileId);
		auto* Second = F.Spawn(); if (!TestNotNull(TEXT("Replacement native pawn"), Second)) { return false; }
		TestTrue(TEXT("Manual spawn's pre-possession profile application still succeeds"), Second->ApplyCharacterProfile(B));
		F.Controller->Possess(Second);
		TestEqual(TEXT("Replacement possession updates existing PlayerState identity"), Second->GetCharacterDisplayName().ToString(), B.CharacterName);
		Second->DispatchBeginPlay();
		TestTrue(TEXT("Replacement progression keeps the selected GUID"), F.Progression->IsAttachedTo(*Second) && F.Progression->GetAttachedProfileId() == B.ProfileId);
		AddExpectedError(TEXT("유효하지 않은 캐릭터 프로필"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Invalid profile remains rejected"), Second->ApplyCharacterProfile(FRogue10mCharacterProfile()));
		TestEqual(TEXT("Rejected profile cannot overwrite identity"), Second->GetCharacterDisplayName().ToString(), B.CharacterName);
		F.Cleanup(); // Include EndPlay/subsystem shutdown in the no-production-save assertion.
		TestTrue(TEXT("Normal serialization was exercised entirely in memory"), F.Saves.MemoryWrites > 0);
		TestEqual(TEXT("No unexpected or production SaveGame path was accessed"), F.Saves.UnexpectedSaveAccesses, 0);
	}
	TestTrue(TEXT("Platform file restored after fixture"), &FPlatformFileManager::Get().GetPlatformFile() == OriginalFiles);
	TestEqual(TEXT("Original command line restored after fixture"), FString(FCommandLine::Get()), OriginalArgs);
	return true;
}

#endif
