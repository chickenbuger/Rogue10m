// Controlled standalone collision/persistence fixture. No authored asset mutation.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mLootTableDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterLootComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Rogue10mMonsterLootRuntime
{
static bool OneOption(const TArray<FString>& Options, const TCHAR* Name, FString& Value)
{
    int32 Count = 0; const FString Prefix = FString(Name) + TEXT("=");
    for (const FString& Option : Options)
    {
        if (!Option.StartsWith(Prefix, ESearchCase::IgnoreCase)) { continue; }
        ++Count; Value = Option.Mid(Prefix.Len());
        if (Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) { Value = Value.Mid(1, Value.Len() - 2); }
    }
    return Count == 1 && !Value.IsEmpty();
}

struct FGroundWarningCapture : FOutputDevice
{
    int32 Count = 0;
    virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (Verbosity == ELogVerbosity::Warning && Category == FName(TEXT("LogRogue10m"))
            && FString(Text).Contains(TEXT("no supported ground was found"))) { ++Count; }
    }
};

class FRun : public TSharedFromThis<FRun>
{
public:
    bool Finished() const { return bFinished; }
    void Start(UWorld& InWorld, bool bInRead)
    {
        World = &InWorld; bRead = bInRead;
        TArray<FString> Tokens, Options; FCommandLine::Parse(FCommandLine::Get(), Tokens, Options);
        FString Town, Keys;
        bool Valid = OneOption(Options, TEXT("RogueMonsterLootLabel"), Label)
            && OneOption(Options, TEXT("RogueProfileSaveSlot"), Town)
            && OneOption(Options, TEXT("RogueProgressionSavePrefix"), Prefix)
            && OneOption(Options, TEXT("RogueSkillKeySaveSlot"), Keys);
        Valid &= Label.Len() >= 1 && Label.Len() <= 24;
        for (TCHAR C : Label) { Valid &= (FChar::IsAlnum(C) && C < 128) || C == TEXT('_'); }
        Valid &= Town == TEXT("Rogue10m_Town_QA_MonsterLoot_") + Label
            && Prefix == TEXT("Rogue10m_Progression_QA_MonsterLoot_") + Label
            && Keys == TEXT("Rogue10m_Fighter_QA_MonsterLoot_") + Label;
        Valid &= !GIsEditor && InWorld.WorldType == EWorldType::Game && InWorld.GetNetMode() == NM_Standalone
            && UGameplayStatics::GetCurrentLevelName(&InWorld, true) == TEXT("StartHub");
        Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("DevLog/MonsterLootQA") / Label);
        auto* Instance = InWorld.GetGameInstance();
        Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
        Saves = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
        Valid &= Profiles && Saves;
        if (!bRead)
        {
            TArray<FString> Existing;
            IFileManager::Get().FindFilesRecursive(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames")), *(Prefix + TEXT("_*.sav")), true, false);
            Valid &= Profiles && Profiles->GetProfileCount() == 0 && !Profiles->GetSelectedProfile()
                && Existing.IsEmpty() && !UGameplayStatics::DoesSaveGameExist(Town, 0)
                && !UGameplayStatics::DoesSaveGameExist(Keys, 0) && !IFileManager::Get().DirectoryExists(*Output);
        }
        else
        {
            Valid &= UGameplayStatics::DoesSaveGameExist(Town, 0)
                && IFileManager::Get().FileExists(*(Output / TEXT("expected.bin")))
                && !IFileManager::Get().FileExists(*(Output / TEXT("read-results.txt")));
        }
        if (!Valid)
        {
            UE_LOG(LogRogue10m, Error, TEXT("RESULT=MONSTER_LOOT_RUNTIME_REJECTED exact_three_QA_namespaces_fresh_write_or_existing_read_StartHub_required"));
            FPlatformMisc::RequestExitWithStatus(false, 3); return;
        }
        IFileManager::Get().MakeDirectory(*Output, true);
        Deadline = FPlatformTime::Seconds() + 90.0;
        Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
        {
            if (Self->bFinished) { return false; }
            if (FPlatformTime::Seconds() > Self->Deadline) { Self->Check(false, TEXT("bounded_watchdog")); Self->Finish(); return false; }
            if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt) { auto Work = MoveTemp(Self->Next); Work(); }
            return !Self->bFinished;
        }), 0.02f);
        Later(2.0, [this]{ Prepare(); });
    }
private:
    void Check(bool OK, const TCHAR* Why)
    {
        ++Checks; Results += FString::Printf(TEXT("%s %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), Why);
        if (!OK) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("MONSTER_LOOT_RUNTIME FAIL %s"), Why); }
    }
    void Later(double Delay, TFunction<void()> Work) { NextAt = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Work); }
    const FRogue10mInventoryGridEntry* FindOwned(FGuid Id) const
    {
        if (!Inventory.IsValid()) { return nullptr; }
        for (const auto& Container : Inventory->GetInventoryContainers())
        { for (const auto& Entry : Container.Entries) { if (Entry.InstanceId == Id) { return &Entry; } } }
        return nullptr;
    }
    const FRogue10mSavedGridEntry* FindSaved(const URogue10mProgressionSaveGame* Saved, FGuid Id) const
    {
        if (!Saved) { return nullptr; }
        for (const auto& Container : Saved->Inventory.Containers)
        { for (const auto& Entry : Container.Entries) { if (Entry.Values.InstanceId == Id) { return &Entry; } } }
        return nullptr;
    }
    bool CorrectSaved(const URogue10mProgressionSaveGame* Saved) const
    {
        const auto* E = FindSaved(Saved, LootId);
        return Saved && Saved->ProfileId == ProfileId && E && E->Values.Quantity == 1
            && E->ItemAsset.ToSoftObjectPath() == FSoftObjectPath(Potion.Get());
    }
    void Prepare()
    {
        PC = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Check(PC.IsValid() && Profiles && Saves, TEXT("actual_controller_and_profile_subsystems"));
        if (Failures) { Finish(); return; }
        if (!bRead)
        {
            FRogue10mCharacterProfile Profile;
            Check(Profiles->CreateProfile(TEXT("LootQaHero"), FRogue10mCharacterAppearance(), Profile)
                && Profiles->SelectProfile(Profile.ProfileId), TEXT("fresh_QA_profile_created_and_selected"));
            ProfileId = Profile.ProfileId;
        }
        else
        {
            const auto* Profile = Profiles->GetSelectedProfile();
            Check(Profile && Profile->CharacterName == TEXT("LootQaHero"), TEXT("selected_QA_profile_survives_process_restart"));
            if (Profile) { ProfileId = Profile->ProfileId; }
        }
        if (Failures) { Finish(); return; }
        PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
        Character = Cast<ARogue10mCharacter>(PC->GetPawn());
        Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
        Check(Character.IsValid() && Inventory.IsValid() && Saves->IsAttachedTo(*Character) && Saves->IsSaveReady(), TEXT("actual_selected_pawn_attached_and_save_ready"));
        Potion.Reset(LoadObject<URogue10mItemDataAsset>(nullptr, TEXT("/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.DA_Item_HealthPotion")));
        Check(Potion.IsValid() && Potion->MaxStackSize >= 1, TEXT("real_persistent_potion_definition_loaded"));
        auto* LiveSlime = LoadObject<URogue10mMonsterDataAsset>(nullptr, TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_VerdantSlime.DA_OW_VerdantSlime"));
        const auto* LiveTable = LiveSlime ? LiveSlime->LootTable.Get() : nullptr;
        const bool Example = LiveTable && LiveTable->Entries.Num() == 1
            && LiveTable->Entries[0].ItemData == Potion.Get() && FMath::IsNearlyEqual(LiveTable->Entries[0].DropChance,0.25f)
            && LiveTable->Entries[0].MinQuantity == 1 && LiveTable->Entries[0].MaxQuantity == 1
            && !LiveTable->BalanceNotes.IsEmpty();
        Check(Example, TEXT("live_world_slime_has_explicit_provisional_25pct_one_potion_table"));
        if (Failures) { Finish(); return; }
        SaveSlot = Saves->GetSaveSlotForProfile(ProfileId);
        Check(SaveSlot.StartsWith(Prefix + TEXT("_")), TEXT("progression_destination_is_exact_QA_namespace"));
        if (bRead) { Read(); return; }
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement(); Character->SetCanBeDamaged(false);
        Later(0.3, [this]{ CreateDrop(); });
    }
    ARogue10mBasicMonster* SpawnFixture(const FVector& Where)
    {
        auto* Source = LoadObject<URogue10mMonsterDataAsset>(nullptr, TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_VerdantSlime.DA_OW_VerdantSlime"));
        if (!Source) { return nullptr; }
        auto* Data = DuplicateObject<URogue10mMonsterDataAsset>(Source, GetTransientPackage());
        auto* Table = NewObject<URogue10mLootTableDataAsset>();
        FRogue10mLootEntry Entry; Entry.ItemData = Potion.Get(); Entry.DropChance = 1.0f; Table->Entries.Add(Entry);
        Data->LootTable = Table; Data->MaxHealth = 100.0f; Data->ExperienceReward = 0; Data->bDestroyOnDeath = false;
        Data->HealthRegenerationPerSecond = Data->StaminaRegenerationPerSecond = Data->ManaRegenerationPerSecond = 0.0f;
        Data->AttackPatternProfile = nullptr;
        FActorSpawnParameters Params; Params.OverrideLevel = World->PersistentLevel; Params.bDeferConstruction = true;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FTransform Transform(FRotator::ZeroRotator, Where);
        auto* Monster = World->SpawnActor<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(), Transform, Params);
        if (!Monster) { return nullptr; }
        auto* Property = FindFProperty<FObjectPropertyBase>(Monster->GetClass(), TEXT("MonsterData"));
        if (!Property) { Monster->Destroy(); return nullptr; }
        Property->SetObjectPropertyValue_InContainer(Monster, Data);
        Monster->AutoPossessAI = EAutoPossessAI::Disabled;
        Monster->GetAbilitySystemComponent()->AddAttributeSetSubobject(Monster->GetRogueAttributeSet());
        Monster->FinishSpawning(Transform);
        return Monster;
    }
    void CreateDrop()
    {
        Check(Saves->SaveNow(), TEXT("baseline_QA_inventory_saved_before_death"));
        FHitResult Ground; const FVector Probe = Character->GetActorLocation() + Character->GetActorForwardVector() * 600.0f + FVector(0,0,300);
        const bool Supported = World->LineTraceSingleByObjectType(Ground, Probe, Probe - FVector(0,0,5000), FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(LootRuntimeGround), false, Character.Get()));
        Check(Supported && Ground.bBlockingHit && Ground.ImpactNormal.Z >= 0.65f, TEXT("actual_hub_supporting_ground_found"));
        if (Failures) { Finish(); return; }
        GroundPoint = Ground.ImpactPoint;
        Target = SpawnFixture(GroundPoint + FVector(0,0,100));
        Check(Target.IsValid() && !Target->IsCombatTestInstance(), TEXT("QA_normal_monster_clones_live_slime_without_mutating_asset"));
        if (Failures) { Finish(); return; }
        Target->TakeDamage(1000.0f, FDamageEvent(), PC.Get(), Character.Get());
        Check(Target->IsDead() && Target->GetMonsterLootComponent()->GetCommittedDropCount() == 1, TEXT("actual_lethal_damage_commits_one_drop"));
        for (TActorIterator<ARogue10mDroppedItem> It(World.Get()); It; ++It)
        { if (It->GetOwner() == Target.Get() && !It->IsActorBeingDestroyed()) { Pickup = *It; break; } }
        Check(Pickup.IsValid(), TEXT("death_created_real_world_pickup_away_from_player"));
        if (Failures) { Finish(); return; }
        LootId = Pickup->GetItemStack().InstanceId;
        Check(LootId.IsValid() && Pickup->GetItemStack().Quantity == 1 && !FindOwned(LootId), TEXT("uncollected_instance_identity_recorded"));
        const FVector Destination = Pickup->GetActorLocation() + FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-35.0f);
        // Collision overlap is the only acquisition trigger. No direct TryPickup call is used.
        Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
        Character->GetCapsuleComponent()->UpdateOverlaps();
        Later(0.25, [this]{ VerifyOverlap(); });
    }
    void VerifyOverlap()
    {
        const auto* Entry = FindOwned(LootId);
        Check(Entry && Entry->Quantity == 1 && Entry->ItemData == Potion.Get(), TEXT("actual_collision_auto_overlap_added_exact_GUID_and_quantity"));
        Check(!Pickup.IsValid() || Pickup->IsActorBeingDestroyed(), TEXT("successful_auto_overlap_consumed_world_actor"));
        Target->TakeDamage(1000.0f, FDamageEvent(), PC.Get(), Character.Get());
        Check(Target->GetMonsterLootComponent()->GetCommittedDropCount() == 1, TEXT("repeated_lethal_damage_does_not_duplicate"));
        if (Failures) { Finish(); return; }
        Later(1.6, [this]{ VerifyPersistence(); });
    }
    void VerifyPersistence()
    {
        TStrongObjectPtr<URogue10mProgressionSaveGame> AutoSaved(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)));
        Check(CorrectSaved(AutoSaved.Get()), TEXT("inventory_change_debounce_wrote_GUID_quantity_and_asset_to_disk"));
        Check(Saves->SaveNow(), TEXT("explicit_SaveNow_succeeds_after_debounce"));
        TStrongObjectPtr<URogue10mProgressionSaveGame> Disk(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)));
        Check(CorrectSaved(Disk.Get()), TEXT("fresh_disk_deserialization_preserves_collected_instance"));
        if (Failures) { Finish(); return; }
        Saves->DetachCharacter(*Character,false);
        bool Removed = false;
        for (int32 C=0;C<Inventory->GetInventoryContainers().Num();++C)
        { if (FindOwned(LootId)) { FRogue10mInventoryGridEntry Out; Removed |= Inventory->RemoveGridItem(C,LootId,Out); } }
        Check(Removed && !FindOwned(LootId), TEXT("detached_memory_copy_removed_without_disk_write"));
        Check(Saves->AttachCharacter(*Character) && Saves->IsSaveReady(), TEXT("public_attach_reloads_real_disk_snapshot"));
        const auto* Restored = FindOwned(LootId);
        Check(Restored && Restored->Quantity == 1 && Restored->ItemData == Potion.Get(), TEXT("inventory_restores_disk_GUID_quantity_and_asset"));
        TArray<uint8> Bytes;
        Check(UGameplayStatics::SaveGameToMemory(Disk.Get(),Bytes) && FFileHelper::SaveArrayToFile(Bytes,*(Output/TEXT("expected.bin"))), TEXT("cross_process_expected_snapshot_written"));
        Check(FFileHelper::SaveStringToFile(FString::Printf(TEXT("profile=%s\nloot=%s\nquantity=1\nitem=%s\nslot=%s\n"),*ProfileId.ToString(),*LootId.ToString(),*Potion->GetPathName(),*SaveSlot),*(Output/TEXT("identity.txt"))), TEXT("cross_process_identity_receipt_written"));
        NoGround();
    }
    void NoGround()
    {
        auto* Unsupported = SpawnFixture(GroundPoint + FVector(0,0,100000));
        Check(Unsupported != nullptr, TEXT("unsupported_ground_fixture_spawned"));
        if (!Unsupported) { Finish(); return; }
        FGroundWarningCapture Capture; if (GLog) { GLog->AddOutputDevice(&Capture); }
        Unsupported->TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr);
        if (GLog) { GLog->RemoveOutputDevice(&Capture); }
        auto* Loot = Unsupported->GetMonsterLootComponent();
        Check(Capture.Count == 1 && Unsupported->IsDead() && Loot->HasProcessedDeathLoot() && Loot->GetCommittedDropCount() == 0, TEXT("no_supported_ground_emits_one_warning_and_consumes_roll_without_drop"));
        Unsupported->SetActorLocation(GroundPoint+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
        Check(Loot->SpawnDeathLoot(Unsupported->GetMonsterData()->LootTable.Get()) == 0 && Loot->GetCommittedDropCount() == 0, TEXT("failed_ground_drop_is_not_rerolled_after_corpse_moves"));
        Finish();
    }
    void Read()
    {
        TArray<uint8> Bytes;
        Check(FFileHelper::LoadFileToArray(Bytes,*(Output/TEXT("expected.bin"))), TEXT("prior_process_expected_snapshot_loaded"));
        TStrongObjectPtr<URogue10mProgressionSaveGame> Expected(Bytes.Num()?Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)):nullptr);
        Check(Expected.IsValid() && Expected->ProfileId == ProfileId, TEXT("cross_process_profile_identity_matches"));
        if (Failures) { Finish(); return; }
        FString IdText; FFileHelper::LoadFileToString(IdText,*(Output/TEXT("identity.txt")));
        TArray<FString> Lines; IdText.ParseIntoArrayLines(Lines);
        for (const auto& Line : Lines) { if (Line.StartsWith(TEXT("loot="))) { FGuid::Parse(Line.Mid(5),LootId); } }
        const auto* Entry = FindOwned(LootId);
        TStrongObjectPtr<URogue10mProgressionSaveGame> Disk(Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)));
        Check(LootId.IsValid() && CorrectSaved(Expected.Get()) && CorrectSaved(Disk.Get()) && Entry && Entry->Quantity == 1 && Entry->ItemData == Potion.Get(), TEXT("new_process_restores_exact_collected_GUID_quantity_and_asset"));
        Finish();
    }
    void Finish()
    {
        if (bFinished) { return; } bFinished=true;
        Results += FString::Printf(TEXT("checks=%d\nfailures=%d\nNOTE=Controlled StartHub normal-monster fixture; cloned live slime data, guaranteed transient table, zero XP, no AI, real potion and collision overlap. Not normal combat/balance proof. Uncollected world drops are not serialized. SpawnActor failure not forced.\n"),Checks,Failures);
        if (!FFileHelper::SaveStringToFile(Results,*(Output/(bRead?TEXT("read-results.txt"):TEXT("write-results.txt")))))
        { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("MONSTER_LOOT_RUNTIME failed to write final report")); }
        UE_LOG(LogRogue10m,Display,TEXT("RESULT=MONSTER_LOOT_RUNTIME_%s mode=%s checks=%d failures=%d"),Failures?TEXT("FAILED"):TEXT("PASSED"),bRead?TEXT("read"):TEXT("write"),Checks,Failures);
        FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogue10mPlayerController> PC;
    TWeakObjectPtr<ARogue10mCharacter> Character;
    TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
    TWeakObjectPtr<ARogue10mBasicMonster> Target;
    TWeakObjectPtr<ARogue10mDroppedItem> Pickup;
    TStrongObjectPtr<URogue10mItemDataAsset> Potion;
    URogue10mCharacterProfileSubsystem* Profiles=nullptr;
    URogue10mProgressionSubsystem* Saves=nullptr;
    FGuid ProfileId,LootId;
    FVector GroundPoint=FVector::ZeroVector;
    FString Label,Prefix,Output,SaveSlot,Results;
    TFunction<void()> Next;
    FTSTicker::FDelegateHandle Tick;
    double Deadline=0,NextAt=0;
    int32 Checks=0,Failures=0;
    bool bRead=false,bFinished=false;
};
static TSharedPtr<FRun> Active;
static void Run(bool Read)
{
    if (!GEngine || GIsEditor || (Active.IsValid() && !Active->Finished())) { return; }
    for (const auto& Context:GEngine->GetWorldContexts())
    { if (auto* W=Context.World();W&&W->WorldType==EWorldType::Game) { Active=MakeShared<FRun>();Active->Start(*W,Read);return; } }
}
static FAutoConsoleCommand WriteCommand(TEXT("Rogue10m.TestMonsterLootWrite"),TEXT("Controlled real overlap and debounce/disk loot QA; exact fresh MonsterLoot QA namespaces required."),FConsoleCommandDelegate::CreateLambda([]{Run(false);}));
static FAutoConsoleCommand ReadCommand(TEXT("Rogue10m.TestMonsterLootRead"),TEXT("Second-process verification of isolated monster loot GUID/quantity."),FConsoleCommandDelegate::CreateLambda([]{Run(true);}));
}
#endif
