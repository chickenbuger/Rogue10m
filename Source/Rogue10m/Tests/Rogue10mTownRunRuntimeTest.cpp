// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Algo/AllOf.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mDroppedItem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTownServiceDataAsset.h"
#include "Rogue10mTutorialDataAsset.h"
#include "Rogue10mTutorialNPC.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Rogue10mTownRunTest
{
template<typename T> static bool Same(const T& A, const T& B)
{
	return T::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

static bool SafeOverride(const FString& Value, const TCHAR* Prefix, int32 MaxLength)
{
	if (!Value.StartsWith(Prefix) || Value.Len() > MaxLength) { return false; }
	for (TCHAR Character : Value) { if (!FChar::IsAlnum(Character) && Character != TEXT('_')) { return false; } }
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld, bool bInRead)
	{
		World = &InWorld;
		bRead = bInRead;
		FString ProfileSlot, ProgressionPrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueCaptureDepartureDialogue="), DepartureCapture);
		if (!SafeOverride(ProfileSlot, TEXT("Rogue10m_Town_QA_"), 100)
			|| !SafeOverride(ProgressionPrefix, TEXT("Rogue10m_Progression_QA_"), 80))
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=TOWN_RUN_FAILED reason=missing_bounded_QA_save_overrides"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		if (!DepartureCapture.IsEmpty() && (bRead || FPaths::IsRelative(DepartureCapture)
			|| !FPaths::GetExtension(DepartureCapture).Equals(TEXT("png"), ESearchCase::IgnoreCase)
			|| IFileManager::Get().FileExists(*DepartureCapture)))
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=TOWN_RUN_FAILED reason=departure_capture_requires_a_new_absolute_PNG_path_in_write_mode"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/town-run-persistence-20261011"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		Deadline = FPlatformTime::Seconds() + 120.0;
		Watchdog = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (FPlatformTime::Seconds() > Self->Deadline)
			{
				Self->Check(false, TEXT("bounded_runtime_and_portal_travel_timeout"));
				Self->Finish();
				return false;
			}
			return true;
		}), 0.25f);
		LoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([Self = AsShared()](UWorld* NewWorld)
		{
			Self->OnMapLoaded(NewWorld);
		});
		InWorld.GetTimerManager().SetTimer(PrepareTimer, FTimerDelegate::CreateLambda([Self = AsShared()] { Self->Prepare(); }), 3.0f, false);
	}

private:
	void Check(bool bCondition, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bCondition ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bCondition) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("TOWN_RUN FAIL %s"), Reason); }
	}

	bool RefreshCharacterReferences()
	{
		UGameInstance* Instance = World.IsValid() ? World->GetGameInstance() : nullptr;
		Profiles = Instance ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		Persistence = Instance ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		Controller = World.IsValid() ? Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()) : nullptr;
		Character = Controller.IsValid() ? Cast<ARogue10mCharacter>(Controller->GetPawn()) : nullptr;
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Inventory = Character.IsValid() ? Character->GetInventoryComponent() : nullptr;
		Combat = Character.IsValid() ? Character->GetCombatComponent() : nullptr;
		Town = State ? State->GetTownProgressionComponent() : nullptr;
		return Profiles && Persistence && Controller.IsValid() && Character.IsValid() && State && Inventory && Combat && Town;
	}

	void Prepare()
	{
		Check(World.IsValid() && UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub"), TEXT("actual_StartHub_initial_world"));
		RefreshCharacterReferences();
		Check(Profiles && Persistence && Controller.IsValid(), TEXT("QA_profile_subsystems_and_controller_exist"));
		if (Failures) { Finish(); return; }
		if (!bRead)
		{
			Check(Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_QA_profile_slot"));
			FRogue10mCharacterProfile Profile;
			if (Failures || !Profiles->CreateProfile(TEXT("TownRunQa"), FRogue10mCharacterAppearance(), Profile))
			{
				Check(false, TEXT("isolated_travel_profile_created")); Finish(); return;
			}
			ProfileId = Profile.ProfileId;
			Check(Profiles->SelectProfile(ProfileId), TEXT("isolated_travel_profile_selected"));
		}
		else
		{
			const FRogue10mCharacterProfile* Profile = Profiles->GetSelectedProfile();
			Check(Profile && Profile->CharacterName == TEXT("TownRunQa"), TEXT("selected_travel_profile_survives_new_process"));
			if (!Profile) { Finish(); return; }
			ProfileId = Profile->ProfileId;
		}
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Check(RefreshCharacterReferences(), TEXT("selected_character_spawned_with_progression_components"));
		Check(Character.IsValid() && Persistence->IsAttachedTo(*Character) && Persistence->IsSaveReady(), TEXT("selected_character_auto_restore_ready"));
		if (Failures) { Finish(); return; }
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(NPC.IsValid() && NPC->TutorialData && NPC->TutorialData->StartingItem, TEXT("actual_Arin_and_supply_item_exist"));
		if (Failures) { Finish(); return; }
		if (bRead) { VerifyRestart(); return; }
		PrepareTownAndTravel();
	}

	void PrepareTownAndTravel()
	{
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Character->GetCharacterMovement()->StopMovementImmediately();
		Check(NPC->BeginDialogue(Character.Get()), TEXT("actual_Arin_dialogue_opened"));
		if (Failures) { Finish(); return; }
		TownGroundDeadline = FPlatformTime::Seconds() + 10.0;
		WaitForTownGround();
	}

	void WaitForTownGround()
	{
		if (Character.IsValid() && Character->CanChangeWeapon())
		{
			PrepareTownServices();
			return;
		}
		if (!Character.IsValid() || FPlatformTime::Seconds() >= TownGroundDeadline)
		{
			Check(false, TEXT("town_fixture_reaches_grounded_weapon_change_state"));
			Finish();
			return;
		}
		Later(.1f, &FRun::WaitForTownGround);
	}

	bool EquipStarterKnuckle()
	{
		const auto& Containers = Inventory->GetInventoryContainers();
		for (int32 Index = 0; Index < Containers.Num(); ++Index)
		{
			for (const FRogue10mInventoryGridEntry& Entry : Containers[Index].Entries)
			{
				if (Entry.ItemData && Entry.Quantity == 1 && Entry.ItemData->bCanDrop
					&& Entry.ItemData->EquipSlotType == ERogue10mInventorySlotType::MainWeapon
					&& Entry.ItemData->WeaponType == ERogue10mWeaponType::Knuckle)
				{
					const FGuid StarterId = Entry.InstanceId;
					return Inventory->TryEquipGridItem(Index, StarterId);
				}
			}
		}
		return false;
	}

	bool SeedTownTraitFixture()
	{
		const auto Vitality = Town->GetUpgradeDefinition(ERogue10mTownTrait::Vitality, NPC->ServiceData);
		const auto Power = Town->GetUpgradeDefinition(ERogue10mTownTrait::Power, NPC->ServiceData);
		if (!Vitality.IsTuningValid() || !Power.IsTuningValid() || !Persistence->SaveNow()) { return false; }
		const int32 Budget = Vitality.PointCost + Power.PointCost;
		const FString Slot = Persistence->GetSaveSlotForProfile(ProfileId);
		Persistence->DetachCharacter(*Character, false);
		auto* Seed = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Seed || Seed->ProfileId != ProfileId) { return false; }
		// Seed only this disposable profile. Run XP does not earn lobby points before settlement.
		Seed->LobbyEarnedPoints = Budget;
		Seed->HighestRunLevel = FMath::Max(Seed->HighestRunLevel, FMath::Max(Vitality.UnlockLevel, Power.UnlockLevel));
		return UGameplayStatics::SaveGameToSlot(Seed, Slot, 0) && Persistence->AttachCharacter(*Character)
			&& Persistence->IsSaveReady() && Town->GetAvailableTraitPoints() == Budget;
	}

	void PrepareTownServices()
	{
		Check(EquipStarterKnuckle(), TEXT("fixture_equips_existing_droppable_starter_from_fresh_inventory"));
		if (Failures) { Finish(); return; }
		Inventory->AddGold(777);
		State->AddExperience(1500);
		State->RecordSkillUse(TEXT("TownRunQaSkill"), 23);
		State->RecordMonsterDefeat(TEXT("TownRunQaBefore"), 7);
		Check(NPC->ClaimStartingSupplies(), TEXT("profile_supply_claimed_before_portal"));
		RewardId = NPC->TutorialData->RewardId;
		SupplyAsset.Reset(NPC->TutorialData->StartingItem.Get());
		Check(NPC->SelectDifficulty(ERogue10mDifficulty::Hard), TEXT("actual_NPC_Hard_selection_before_portal"));
		Check(SeedTownTraitFixture(), TEXT("isolated_save_restores_exact_two_purchase_lobby_budget_and_unlocks"));
		if (Failures) { Finish(); return; }
		Check(NPC->UpgradeTrait(ERogue10mTownTrait::Vitality) && NPC->UpgradeTrait(ERogue10mTownTrait::Power), TEXT("actual_NPC_traits_purchased_before_portal"));
		Check(NPC->EnhanceEquippedItem(ERogue10mInventorySlotType::MainWeapon), TEXT("actual_NPC_equipped_weapon_enhanced_before_portal"));
		for (URogue10mAttackSkillData* Skill : Combat->GetUnlockedWeaponSkills())
		{
			if (Skill && Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique
				&& Combat->AssignSkillToInputSlot(Skill, ERogue10mAttackInputSlot::Special)) { break; }
		}
		Check(Combat->CaptureCombat().SkillBindings.Num() > 0, TEXT("combat_bindings_present_before_portal"));
		DropAndPickupEnhancedWeapon();
		if (Failures) { Finish(); return; }
		SupplyCountBeforeTravel = CountItem(SupplyAsset.Get());
		NPC->CloseDialogue();
		Expected.Reset(NewObject<URogue10mProgressionSaveGame>());
		CaptureExpected(*Expected);
		Check(Persistence->SaveNow(), TEXT("progression_saved_before_actual_portal_overlap"));
		for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It) { Portal = *It; break; }
		UBoxComponent* Box = Portal.IsValid() ? Portal->FindComponentByClass<UBoxComponent>() : nullptr;
		Check(Box != nullptr, TEXT("actual_StartHub_native_portal_collision_exists"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(Box->GetComponentLocation() + FVector(500.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Check(!Portal->BeginDepartureDialogue(Character.Get()), TEXT("departure_dialogue_rejects_pawn_outside_gate"));
		Character->GetCharacterMovement()->StopMovementImmediately();
		FVector GatePosition = Box->GetComponentLocation();
		if (!DepartureCapture.IsEmpty())
		{
			GatePosition.Y -= 145.0f;
			FHitResult Floor;
			const FCollisionQueryParams Query(SCENE_QUERY_STAT(TownRunDepartureCaptureFloor), false, Character.Get());
			Check(World->LineTraceSingleByChannel(Floor, GatePosition + FVector(0.0f, 0.0f, 600.0f),
				GatePosition - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Query), TEXT("authored_gate_approach_has_visible_floor"));
			if (Failures) { Finish(); return; }
			GatePosition.Z = Floor.ImpactPoint.Z + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f;
			Character->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f), ETeleportType::TeleportPhysics);
			Controller->SetControlRotation(FRotator(-6.0f, 90.0f, 0.0f));
		}
		Character->SetActorLocation(GatePosition, false, nullptr, ETeleportType::TeleportPhysics);
		Check(Box->IsOverlappingActor(Character.Get()), TEXT("real_capsule_overlaps_authored_departure_gate"));
		if (Failures) { Finish(); return; }
		Later(.3f, &FRun::VerifyOverlapAndOpenDeparture);
	}

	void Later(float Delay, void (FRun::*Action)())
	{
		World->GetTimerManager().SetTimer(PrepareTimer, FTimerDelegate::CreateLambda([Self = AsShared(), Action]
		{
			if (!Self->bFinished) { ((*Self).*Action)(); }
		}), Delay, false);
	}

	bool IsWaitingInHub() const
	{
		const ARogue10mGameState* Run = World.IsValid() ? World->GetGameState<ARogue10mGameState>() : nullptr;
		return Run && UGameplayStatics::GetCurrentLevelName(World.Get(), true) == TEXT("StartHub")
			&& Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && !World->URL.HasOption(TEXT("StartRun"));
	}

	void VerifyOverlapAndOpenDeparture()
	{
		Check(IsWaitingInHub() && Portal.IsValid() && !Portal->IsDepartureDialogueOpen(),
			TEXT("overlap_alone_leaves_hub_waiting_and_dialogue_closed"));
		if (Failures) { Finish(); return; }
		PressInteract(&FRun::VerifyDefaultStayChoice);
	}

	void PressInteract(void (FRun::*AfterRelease)())
	{
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Pressed, 1.0f));
		World->GetTimerManager().SetTimer(PrepareTimer, FTimerDelegate::CreateLambda([Self = AsShared(), AfterRelease]
		{
			if (!Self->bFinished && Self->Controller.IsValid() && Self->World.IsValid())
			{
				Self->Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Released, 0.0f));
				Self->Later(.25f, AfterRelease);
			}
		}), .1f, false);
	}

	static bool HasSlateKeyboardFocus()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid();
	}

	static bool PressSlateKey(const FKey& Key)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
		const bool bHandled = Slate.ProcessKeyDownEvent(Event);
		Slate.ProcessKeyUpEvent(Event);
		return bHandled;
	}

	void VerifyDefaultStayChoice()
	{
		Check(Portal.IsValid() && Portal->IsDepartureDialogueOpen() && IsWaitingInHub(),
			TEXT("controller_F_opens_departure_dialogue_without_starting_run"));
		if (Failures) { Finish(); return; }
		if (!DepartureCapture.IsEmpty())
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(DepartureCapture), true);
			FScreenshotRequest::RequestScreenshot(DepartureCapture, true, false);
			DepartureCaptureDeadline = FPlatformTime::Seconds() + 10.0;
			Later(.3f, &FRun::WaitForDepartureCapture);
			return;
		}
		ChooseStay();
	}

	void WaitForDepartureCapture()
	{
		if (!FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*DepartureCapture) > 1024)
		{
			Results += FString::Printf(TEXT("INFO departure_capture=%s\n"), *DepartureCapture);
			UE_LOG(LogRogue10m, Display, TEXT("TOWN_RUN departure capture saved to %s"), *DepartureCapture);
			ChooseStay();
			return;
		}
		if (FPlatformTime::Seconds() >= DepartureCaptureDeadline)
		{
			Check(false, TEXT("requested_departure_dialogue_capture_saved"));
			Finish();
			return;
		}
		Later(.2f, &FRun::WaitForDepartureCapture);
	}

	void ChooseStay()
	{
		Check(Portal.IsValid() && Portal->IsDepartureDialogueOpen(), TEXT("departure_choices_still_available"));
		if (Failures) { Finish(); return; }
		if (HasSlateKeyboardFocus())
		{
			Check(PressSlateKey(EKeys::Enter), TEXT("focused_departure_dialogue_handles_Enter"));
		}
		else
		{
			Results += TEXT("INFO No Slate keyboard focus; using the native stay choice.\n");
			Portal->SelectDepartureChoice(0);
		}
		Check(!Portal->IsDepartureDialogueOpen() && IsWaitingInHub(), TEXT("stay_closes_departure_dialogue_without_travel"));
		if (Failures) { Finish(); return; }
		PressInteract(&FRun::ConfirmDeparture);
	}

	void ConfirmDeparture()
	{
		Check(Portal.IsValid() && Portal->IsDepartureDialogueOpen() && IsWaitingInHub(),
			TEXT("controller_F_reopens_departure_dialogue_after_staying"));
		if (Failures) { Finish(); return; }
		bTravelRequested = true;
		UE_LOG(LogRogue10m, Display, TEXT("TOWN_RUN PORTAL departure confirmed profile=%s"), *ProfileId.ToString());
		if (HasSlateKeyboardFocus())
		{
			Check(PressSlateKey(EKeys::Down) && PressSlateKey(EKeys::Enter), TEXT("focused_departure_dialogue_handles_selection_and_confirm"));
		}
		else
		{
			Results += TEXT("INFO No Slate keyboard focus; using the native departure choice.\n");
			Portal->SelectDepartureChoice(1);
		}
		if (Failures) { Finish(); }
	}

	void DropAndPickupEnhancedWeapon()
	{
		const URogue10mItemDataAsset* WeaponData = Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon);
		Check(WeaponData && WeaponData->bCanDrop, TEXT("actual_equipped_starter_weapon_permits_required_drop_case"));
		if (!WeaponData || !WeaponData->bCanDrop)
		{
			return;
		}
		FGuid WeaponId;
		for (const auto* Slots : { &Inventory->GetLeftEquipmentSlots(), &Inventory->GetRightEquipmentSlots() })
		{
			for (const FRogue10mInventorySlot& Slot : *Slots)
			{
				if (Slot.SlotType == ERogue10mInventorySlotType::MainWeapon) { WeaponId = Slot.EquippedItem.InstanceId; }
			}
		}
		Check(WeaponId.IsValid() && Inventory->TryUnequipItemToFirstAvailableGrid(ERogue10mInventorySlotType::MainWeapon), TEXT("enhanced_weapon_unequipped_for_real_drop"));
		int32 Container = INDEX_NONE;
		FRogue10mInventoryGridEntry Before;
		for (int32 Index = 0; Index < Inventory->GetInventoryContainers().Num(); ++Index)
		{
			for (const FRogue10mInventoryGridEntry& Entry : Inventory->GetInventoryContainers()[Index].Entries)
			{
				if (Entry.InstanceId == WeaponId) { Container = Index; Before = Entry; }
			}
		}
		Check(Container != INDEX_NONE && Inventory->DropGridItem(Container, WeaponId, Character->GetActorLocation() + FVector(500.0f, 0.0f, 100.0f)), TEXT("enhanced_weapon_dropped_through_inventory_public_path"));
		if (Failures) { return; }
		ARogue10mDroppedItem* Dropped = nullptr;
		for (TActorIterator<ARogue10mDroppedItem> It(World.Get()); It; ++It)
		{
			if (It->GetItemStack().InstanceId == WeaponId) { Dropped = *It; break; }
		}
		Check(Dropped && Dropped->GetItemStack().Quantity == Before.Quantity && Dropped->GetItemStack().EnhancementLevel == Before.EnhancementLevel
			&& Dropped->GetItemStack().EnhancementAttackBonus == Before.EnhancementAttackBonus
			&& Dropped->GetItemStack().EnhancementDefenseBonus == Before.EnhancementDefenseBonus, TEXT("actual_dropped_actor_preserves_GUID_quantity_and_enhancement"));
		if (!Dropped) { return; }
		// Stay outside capsule overlap so this explicitly exercises TryPickup exactly once.
		Dropped->SetActorLocation(Character->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Check(Dropped->TryPickup(Character.Get()), TEXT("actual_dropped_actor_pickup_succeeds"));
		Container = INDEX_NONE;
		for (int32 Index = 0; Index < Inventory->GetInventoryContainers().Num(); ++Index)
		{
			for (const FRogue10mInventoryGridEntry& Entry : Inventory->GetInventoryContainers()[Index].Entries)
			{
				if (Entry.InstanceId == WeaponId)
				{
					Container = Index;
					Check(Entry.Quantity == Before.Quantity && Entry.EnhancementLevel == Before.EnhancementLevel && Entry.EnhancementAttackBonus == Before.EnhancementAttackBonus
						&& Entry.EnhancementDefenseBonus == Before.EnhancementDefenseBonus, TEXT("pickup_preserves_original_instance_quantity_and_bonus_values"));
				}
			}
		}
		Check(Container != INDEX_NONE && Inventory->TryEquipGridItem(Container, WeaponId), TEXT("dropped_and_picked_enhanced_weapon_re_equips"));
	}

	void OnMapLoaded(UWorld* NewWorld)
	{
		if (bFinished || !bTravelRequested || !NewWorld || NewWorld->WorldType != EWorldType::Game) { return; }
		World = NewWorld;
		Check(UGameplayStatics::GetCurrentLevelName(NewWorld, true) == TEXT("OpenRunMap"), TEXT("actual_portal_loaded_OpenRunMap"));
		NewWorld->GetTimerManager().SetTimer(PrepareTimer, FTimerDelegate::CreateLambda([Self = AsShared()] { Self->VerifyRun(); }), 3.0f, false);
	}

	void VerifyRun()
	{
		Check(RefreshCharacterReferences(), TEXT("posttravel_selected_Pawn_and_PlayerState_exist"));
		Check(Character.IsValid() && Persistence->IsAttachedTo(*Character) && Persistence->IsSaveReady()
			&& Persistence->GetAttachedProfileId() == ProfileId, TEXT("posttravel_profile_auto_attach_and_restore_ready"));
		if (Failures || !Expected) { Finish(); return; }
		CheckSnapshots(*Expected, TEXT("travel"));
		const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
		Check(Run && Run->GetRunPhase() == ERogue10mRunPhase::Running && Run->GetRemainingRunSeconds() > 0.0f,
			TEXT("portal_StartRun_option_remains_active_after_progression_restore"));
		Check(CountItem(SupplyAsset.Get()) == SupplyCountBeforeTravel && Persistence->HasClaimedReward(RewardId),
			TEXT("portal_does_not_duplicate_starter_supplies_or_profile_receipt"));
		if (Failures) { Finish(); return; }
		VerifyHardMonsterAndRewards();
		Check(Persistence->SaveNow(), TEXT("postkill_progression_saved_for_restart"));
		if (Failures) { Finish(); return; }
		TArray<uint8> Bytes;
		Check(UGameplayStatics::LoadDataFromSlot(Bytes, Persistence->GetSaveSlotForProfile(ProfileId), 0),
			TEXT("postkill_actual_profile_save_bytes_loaded"));
		const auto* Interrupted = Cast<URogue10mProgressionSaveGame>(Bytes.IsEmpty() ? nullptr : UGameplayStatics::LoadGameFromMemory(Bytes));
		Check(Interrupted && Interrupted->ProfileId == ProfileId && Interrupted->RunId == Persistence->GetAttachedRunId()
			&& Interrupted->RunId.IsValid() && Interrupted->bEnteredRun && !Interrupted->bRunCompleted
			&& Interrupted->EntryCurrencies.IsValid() && Interrupted->ClaimedRewards.Contains(RewardId),
			TEXT("postkill_actual_save_retains_entered_run_baseline_and_Arin_receipt"));
		if (Failures) { Finish(); return; }
		CheckSnapshots(*Interrupted, TEXT("postkill_disk"));
		Check(FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Output, TEXT("final-expected.bin"))),
			TEXT("postkill_actual_interrupted_save_preserved_for_independent_restart"));
		Finish();
	}

	void VerifyHardMonsterAndRewards()
	{
		const FTransform Transform(FRotator::ZeroRotator, Character->GetActorLocation() + FVector(600.0f, 0.0f, 100.0f));
		ARogue10mBasicMonster* Monster = World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(), Transform,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Check(Monster != nullptr, TEXT("native_monster_deferred_spawned_in_actual_run"));
		if (!Monster) { return; }
		URogue10mMonsterDataAsset* Data = NewObject<URogue10mMonsterDataAsset>(Monster);
		Data->MonsterId = TEXT("TownRunQaTarget");
		Data->DisplayName = FText::FromString(TEXT("Town Run QA Target"));
		Data->MaxHealth = 80.0f;
		Data->AttackDamage = 12.0f;
		Data->ExperienceReward = 20;
		Data->bDestroyOnDeath = false;
		Data->HealthRegenerationPerSecond = 0.0f;
		Data->StaminaRegenerationPerSecond = 0.0f;
		Data->ManaRegenerationPerSecond = 0.0f;
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Monster->GetClass(), TEXT("MonsterData"));
		Check(Property != nullptr, TEXT("native_monster_authored_DataAsset_property_exists"));
		if (!Property) { Monster->Destroy(); return; }
		Property->SetObjectPropertyValue_InContainer(Monster, Data);
		Monster->AutoPossessAI = EAutoPossessAI::Disabled;
		UGameplayStatics::FinishSpawningActor(Monster, Transform);
		Check(Monster->GetRogueAttributeSet() && FMath::IsNearlyEqual(Monster->GetRogueAttributeSet()->GetMaxHealth(), 120.0f), TEXT("Hard_authored_monster_HP_is_80_times_1_5"));
		Check(FMath::IsNearlyEqual(Monster->GetPatternBaseDamage(), 15.0f), TEXT("Hard_authored_monster_damage_is_12_times_1_25"));
		const int32 PreviousGold = Inventory->GetGold();
		const float PreviousExperience = State->GetCurrentExperience();
		const int32 PreviousLevel = State->GetPlayerLevel();
		const int32 PreviousKills = State->GetMonsterDefeatCount(Data->MonsterId);
		Check(PreviousExperience + 30.0f < State->GetExperienceToNextLevel(), TEXT("reward_sample_avoids_level_threshold_to_measure_exact_XP_delta"));
		const float Applied = UGameplayStatics::ApplyDamage(Monster, 121.0f, Controller.Get(), Character.Get(), UDamageType::StaticClass());
		Check(Applied > 0.0f && Monster->IsDead(), TEXT("actual_ApplyDamage_native_monster_death_path_executed"));
		Check(State->GetPlayerLevel() == PreviousLevel && FMath::IsNearlyEqual(State->GetCurrentExperience(), PreviousExperience + 30.0f), TEXT("Hard_native_kill_XP_reward_is_20_times_1_5"));
		Check(Inventory->GetGold() == PreviousGold + 3 && State->GetMonsterDefeatCount(Data->MonsterId) == PreviousKills + 1, TEXT("native_kill_grants_gold_and_records_monster_defeat_once"));
		const int32 GoldAfter = Inventory->GetGold();
		UGameplayStatics::ApplyDamage(Monster, 121.0f, Controller.Get(), Character.Get(), UDamageType::StaticClass());
		Check(Inventory->GetGold() == GoldAfter && State->GetMonsterDefeatCount(Data->MonsterId) == PreviousKills + 1, TEXT("repeated_damage_on_dead_monster_does_not_duplicate_rewards"));
		Monster->Destroy();
	}

	void VerifyRestart()
	{
		TArray<uint8> Bytes;
		Check(FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Output, TEXT("final-expected.bin"))), TEXT("posttravel_first_process_snapshot_exists"));
		Expected.Reset(Bytes.Num() ? Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr);
		Check(Expected && Expected->ProfileId == ProfileId && Expected->bEnteredRun && !Expected->bRunCompleted,
			TEXT("first_process_finished_with_an_unsettled_entered_run_for_the_same_profile"));
		if (Failures || !Expected) { Finish(); return; }
		const auto* Fresh = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Persistence->GetSaveSlotForProfile(ProfileId), 0));
		Check(Fresh && Fresh->ProfileId == ProfileId && Fresh->RunId.IsValid() && Fresh->RunId != Expected->RunId
			&& Fresh->RunId == Persistence->GetAttachedRunId() && Fresh->LastSettledRunId == Expected->RunId
			&& !Fresh->bEnteredRun && !Fresh->bRunCompleted && !Fresh->bAbandoned && !Fresh->EntryCurrencies.IsValid(),
			TEXT("restart_records_the_interrupted_run_and_creates_a_fresh_waiting_run"));
		if (Failures) { Finish(); return; }
		CheckSnapshots(*Fresh, TEXT("fresh_hub_disk"));
		int32 EntryGold = 0, EntryCrystals = 0, EntryLobby = 0;
	const bool bDiscoveriesRetained = Algo::AllOf(Expected->DiscoveredNodes, [Fresh](FName Node)
	{
		return Fresh->DiscoveredNodes.Contains(Node);
	});
		Check(Expected->EntryCurrencies.TryResolve(Expected->RunId, EntryGold, EntryCrystals, EntryLobby)
			&& Fresh->Inventory.Gold == EntryGold && Fresh->Inventory.Crystals == EntryCrystals && Fresh->LobbyEarnedPoints == EntryLobby
			&& Town->GetLobbyEarnedBudget() == EntryLobby && Fresh->HighestRunLevel == Expected->HighestRunLevel
			&& Same(Fresh->Town, Expected->Town) && bDiscoveriesRetained,
			TEXT("interruption_keeps_entry_currencies_permanent_traits_and_discoveries"));
		Check(Fresh->Growth.Level == 1 && Fresh->Growth.Experience == 0.0f && Fresh->Growth.MonsterDefeats.IsEmpty()
			&& State->GetMonsterDefeatCount(TEXT("TownRunQaTarget")) == 0 && Fresh->RewardDefeatBaseline == 0
			&& Fresh->Growth.WeaponType == ERogue10mWeaponType::Unarmed && Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed
			&& !Inventory->GetEquippedItemData(ERogue10mInventorySlotType::MainWeapon),
			TEXT("fresh_run_clears_growth_recent_kills_and_equipped_gear"));
		const auto* SkillTree = State->GetSkillTreeProgressionComponent();
		Check(Expected->Growth.SkillUses.FindRef(TEXT("TownRunQaSkill")) == 23
			&& State->GetSkillUseCount(TEXT("TownRunQaSkill")) == 0
			&& SkillTree && SkillTree->GetMasteryTotalExperience(ERogue10mSkillTreeTrack::Brawler) == 0
			&& SkillTree->GetSpentSkillTreePoints() == 0 && Same(Fresh->SkillTree, SkillTree->Capture()),
			TEXT("fresh_run_clears_recorded_skill_uses_and_run_skill_progression"));
		const ARogue10mGameState* Run = World->GetGameState<ARogue10mGameState>();
		Check(Run && Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && !World->URL.HasOption(TEXT("StartRun")),
			TEXT("new_process_StartHub_does_not_resume_previous_run_timer"));
		const auto* Supplies = NPC->TutorialData.Get();
		Check(Expected->ClaimedRewards.Contains(Supplies->RewardId) && Fresh->ClaimedRewards.IsEmpty(),
			TEXT("fresh_run_clears_the_interrupted_runs_Arin_receipt"));
		if (Failures) { Finish(); return; }
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		const int32 InitialSupplyCount = CountItem(Supplies->StartingItem);
		Check(NPC->BeginDialogue(Character.Get()) && !NPC->HasClaimedReward(Character.Get()) && NPC->ClaimStartingSupplies()
			&& NPC->HasClaimedReward(Character.Get()) && !NPC->ClaimStartingSupplies()
			&& CountItem(Supplies->StartingItem) == InitialSupplyCount + Supplies->StartingItemQuantity,
			TEXT("Arin_grants_starter_supplies_once_in_the_fresh_run"));
		Finish();
	}

	void CaptureExpected(URogue10mProgressionSaveGame& Target) const
	{
		Target.ProfileId = ProfileId;
		Target.Growth = State->CaptureGrowth();
		Target.Inventory = Inventory->CaptureInventory();
		Target.Combat = Combat->CaptureCombat();
		Target.Town = Town->CaptureProgression();
		Target.ClaimedRewards.Empty();
		if (!RewardId.IsNone()) { Target.ClaimedRewards.Add(RewardId); }
	}

	void CheckSnapshots(const URogue10mProgressionSaveGame& Target, const TCHAR* Phase)
	{
		Check(Same(Target.Growth, State->CaptureGrowth()), *FString::Printf(TEXT("%s_full_growth_snapshot_restored"), Phase));
		Check(Same(Target.Inventory, Inventory->CaptureInventory()), *FString::Printf(TEXT("%s_full_inventory_gold_GUID_and_enhancement_snapshot_restored"), Phase));
		Check(Same(Target.Combat, Combat->CaptureCombat()), *FString::Printf(TEXT("%s_full_combat_unlock_and_binding_snapshot_restored"), Phase));
		Check(Same(Target.Town, Town->CaptureProgression()), *FString::Printf(TEXT("%s_full_trait_bonus_and_Hard_difficulty_snapshot_restored"), Phase));
	}

	int32 CountItem(const URogue10mItemDataAsset* Item) const
	{
		int32 Count = 0;
		for (const FRogue10mInventoryContainer& Bag : Inventory->GetInventoryContainers())
		{
			for (const FRogue10mInventoryGridEntry& Entry : Bag.Entries) { if (Entry.ItemData == Item) { Count += Entry.Quantity; } }
		}
		return Count;
	}

	void Finish()
	{
		if (bFinished) { return; }
		bFinished = true;
		if (NPC.IsValid()) { NPC->CloseDialogue(); }
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadHandle);
		FTSTicker::GetCoreTicker().RemoveTicker(Watchdog);
		Results += FString::Printf(TEXT("mode=%s\nfailures=%d\nprofile=%s\n"), bRead ? TEXT("read") : TEXT("write"), Failures, *ProfileId.ToString());
		if (!FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, bRead ? TEXT("read-results.txt") : TEXT("write-results.txt")))) { ++Failures; }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=TOWN_RUN_%s mode=%s failures=%d output=%s"), Failures ? TEXT("FAILED") : TEXT("PASSED"), bRead ? TEXT("read") : TEXT("write"), Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<ARogue10mRunPortal> Portal;
	TStrongObjectPtr<URogue10mItemDataAsset> SupplyAsset;
	TStrongObjectPtr<URogue10mProgressionSaveGame> Expected;
	URogue10mCharacterProfileSubsystem* Profiles = nullptr;
	URogue10mProgressionSubsystem* Persistence = nullptr;
	ARogue10mPlayerState* State = nullptr;
	URogue10mInventoryComponent* Inventory = nullptr;
	URogue10mCombatComponent* Combat = nullptr;
	URogue10mTownProgressionComponent* Town = nullptr;
	FGuid ProfileId;
	FName RewardId;
	FTimerHandle PrepareTimer;
	FDelegateHandle LoadHandle;
	FTSTicker::FDelegateHandle Watchdog;
	FString Output, Results, DepartureCapture;
	double Deadline = 0.0;
	double TownGroundDeadline = 0.0;
	double DepartureCaptureDeadline = 0.0;
	int32 SupplyCountBeforeTravel = 0;
	int32 Failures = 0;
	bool bTravelRequested = false;
	bool bRead = false;
	bool bFinished = false;
};

static TSharedPtr<FRun> Active;
static void Run(bool bRead)
{
	if (!GEngine || GIsEditor) { return; }
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->WorldType == EWorldType::Game)
		{
			Active = MakeShared<FRun>();
			Active->Start(*World, bRead);
			return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("RESULT=TOWN_RUN_FAILED reason=no_game_world"));
	FPlatformMisc::RequestExitWithStatus(false, 1);
}

static FAutoConsoleCommand WriteCommand(TEXT("Rogue10m.TestTownRunWrite"), TEXT("Tests NPC-selected Hard difficulty, actual portal travel, drop/pickup and native kill rewards with isolated saves."), FConsoleCommandDelegate::CreateLambda([] { Run(false); }));
static FAutoConsoleCommand ReadCommand(TEXT("Rogue10m.TestTownRunRead"), TEXT("Verifies abandoned-run recovery and the fresh hub state in a second isolated game process."), FConsoleCommandDelegate::CreateLambda([] { Run(true); }));
}

#endif
