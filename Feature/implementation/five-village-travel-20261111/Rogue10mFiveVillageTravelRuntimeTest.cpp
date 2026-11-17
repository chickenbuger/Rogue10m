// Isolated source candidate. Copy into Source/Rogue10m/Tests only after build-owner handoff.
// Public profile/portal/abandon/return APIs and native walking; no fabricated victory.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
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
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mFiveVillageGameMode.h"
#include "Rogue10mFiveVillageProgressComponent.h"
#include "Rogue10mFiveVillageRunPortal.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Rogue10mRunPortal.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Rogue10mFiveVillageTravelQA
{
static const TCHAR* Hub = TEXT("/Game/Rogue10m/Maps/StartHub");
static const TCHAR* Field = TEXT("/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110");

static bool SafeLabel(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 20) { return false; }
	for (TCHAR C : Value)
	{
		if (!((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z'))
			|| (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_'))) { return false; }
	}
	return true;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	bool Finished() const { return bFinished; }
	void Start(UWorld& Initial)
	{
		const TCHAR* Args = FCommandLine::Get();
		FString ProfileSlot, Prefix, KeySlot;
		FParse::Value(Args, TEXT("RogueFiveVillageTravelLabel="), Label);
		FParse::Value(Args, TEXT("RogueFiveVillageTravelScenario="), Scenario);
		FParse::Value(Args, TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(Args, TEXT("RogueProgressionSavePrefix="), Prefix);
		FParse::Value(Args, TEXT("RogueSkillKeySaveSlot="), KeySlot);
		const FString Suffix = Label + TEXT("_") + Scenario;
		TArray<FString> Existing;
		IFileManager::Get().FindFiles(Existing, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Prefix + TEXT("_*.sav"))), true, false);
		const FString ExpectedOutput = FPaths::ProjectDir() / TEXT("Feature/doc/evidence/five-village-travel-20261111") / Label / Scenario / TEXT("runtime");
		auto* Profiles = Initial.GetGameInstance() ? Initial.GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		if (!SafeLabel(Label) || (Scenario != TEXT("Direct") && Scenario != TEXT("Revisit"))
			|| ProfileSlot != TEXT("Rogue10m_Town_QA_FiveVillage_") + Suffix
			|| Prefix != TEXT("Rogue10m_Progression_QA_FiveVillage_") + Suffix
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_FiveVillage_") + Suffix
			|| Initial.GetNetMode() != NM_Standalone || !Initial.IsGameWorld()
			|| UWorld::RemovePIEPrefix(Initial.GetOutermost()->GetName()) != Hub
			|| UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0) || UGameplayStatics::DoesSaveGameExist(KeySlot, 0)
			|| !Existing.IsEmpty() || IFileManager::Get().DirectoryExists(*ExpectedOutput)
			|| !Profiles || Profiles->GetProfileCount() != 0 || Profiles->GetSelectedProfile())
		{
			bFinished = true;
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIVE_VILLAGE_TRAVEL_FAILED exact_fresh_QA_namespace_and_StartHub_required"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Output = ExpectedOutput;
		if (!IFileManager::Get().MakeDirectory(*Output, true))
		{
			bFinished = true; UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIVE_VILLAGE_TRAVEL_FAILED output_directory_unavailable"));
			FPlatformMisc::RequestExitWithStatus(false, 1); return;
		}
		Instance = Initial.GetGameInstance(); World = &Initial;
		StartedAt = FPlatformTime::Seconds(); NextAt = StartedAt + 3.0;
		Deadline = StartedAt + (Scenario == TEXT("Direct") ? 180.0 : 450.0);
		Rendered = !FParse::Param(Args, TEXT("nullrhi"));
		Events = TEXT("wall_seconds,stage,map,event,detail\n");
		for (const TCHAR* Name : {TEXT("normal_entry"), TEXT("timer_running"), TEXT("third_person"), TEXT("route_observations"),
			TEXT("settled"), TEXT("saved"), TEXT("returned_hub"), TEXT("profile_preserved")}) { Milestones.Add(Name, false); }
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			return Self->Tick();
		}), 0.0f);
	}

private:
	enum class EStage { Prepare, HubWalk, FieldWait, FieldWalk, RouteObserve, SettlementWait, HubWait };
	static FString Quote(FString Value) { Value.ReplaceInline(TEXT("\""), TEXT("\"\"")); return TEXT("\"") + Value + TEXT("\""); }
	FString Map() const { return World.IsValid() ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString(); }
	void Event(const TCHAR* Name, const FString& Detail = FString())
	{
		Events += FString::Printf(TEXT("%.6f,%d,%s,%s,%s\n"), FPlatformTime::Seconds() - StartedAt,
			static_cast<int32>(Stage), *Quote(Map()), Name, *Quote(Detail));
	}
	bool Check(bool Good, const TCHAR* Name)
	{
		++Checks; auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Name); Row->SetBoolField(TEXT("passed"), Good);
		CheckRows.Add(MakeShared<FJsonValueObject>(Row));
		if (!Good) { ++Failures; Event(TEXT("failed"), Name); UE_LOG(LogRogue10m, Error, TEXT("FIVE_VILLAGE_TRAVEL FAIL %s"), Name); }
		return Good;
	}
	void RefreshWorld()
	{
		UWorld* Found = nullptr;
		if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Context.WorldType == EWorldType::Game && Candidate->GetGameInstance() == Instance.Get()) { Found = Candidate; break; }
		}
		if (Found != World.Get()) { World = Found; Event(TEXT("world_changed")); }
		PC = Found ? Cast<ARogue10mPlayerController>(Found->GetFirstPlayerController()) : nullptr;
		Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
		State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		Run = Found ? Found->GetGameState<ARogue10mGameState>() : nullptr;
		Saves = Instance.IsValid() ? Instance->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
	}
	bool Ready() const { return World.IsValid() && World->HasBegunPlay() && PC.IsValid() && Player.IsValid() && State.IsValid() && Run.IsValid() && Saves.IsValid(); }
	bool Tick()
	{
		if (bFinished) { return false; }
		RefreshWorld(); const double Now = FPlatformTime::Seconds();
		if (Now > Deadline) { Check(false, TEXT("bounded_fixture_watchdog")); Finish(); return false; }
		if (Now < NextAt) { return true; }
		if (Ready() && Now >= NextSampleAt)
		{
			const FVector P = Player->GetActorLocation();
			Event(TEXT("sample"), FString::Printf(TEXT("x=%.2f;y=%.2f;z=%.2f;remaining=%.3f;third=%d"), P.X, P.Y, P.Z, Run->GetRemainingRunSeconds(), Player->IsThirdPersonGameplayActive()));
			NextSampleAt = Now + 0.5;
		}
		switch (Stage)
		{
		case EStage::Prepare: Prepare(); break;
		case EStage::HubWalk:
			if (Map() == Field) { Stage = EStage::FieldWait; StageDeadline = Now + 30.0; NextAt = Now + 0.1; break; }
			if (Map() != Hub) { if (World.IsValid()) { Check(false, TEXT("portal_did_not_travel_to_candidate")); Finish(); } break; }
			if (Ready() && Walk(Waypoints[Waypoint]))
			{
				if (++Waypoint >= Waypoints.Num()) { Stage = EStage::FieldWait; StageDeadline = Now + 30.0; NextAt = Now + 0.1; }
			}
			break;
		case EStage::FieldWait: EnterField(); break;
		case EStage::FieldWalk:
			if (!Ready() || Map() != Field || !Run->IsRunActive()) { Check(false, TEXT("candidate_remains_active_during_route")); Finish(); break; }
			if (Walk(Waypoints[Waypoint]))
			{
				if (++Waypoint >= Waypoints.Num()) { Stage = EStage::RouteObserve; NextAt = Now + 0.35; }
			}
			break;
		case EStage::RouteObserve: ObserveRouteAndAbandon(); break;
		case EStage::SettlementWait: ObserveSettlement(); break;
		case EStage::HubWait: ObserveReturn(); break;
		}
		return !bFinished;
	}
	bool Walk(const FVector& Goal)
	{
		if (!Player.IsValid() || !PC.IsValid()) { return false; }
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement(); const FVector Position = Player->GetActorLocation();
		if (!Movement || Position.ContainsNaN() || Player->IsDead() || PC->IsMoveInputIgnored() || !Player->IsThirdPersonGameplayActive())
		{ Check(false, TEXT("live_third_person_walking_context")); Finish(); return false; }
		const double StepDistance = FVector::Dist2D(PreviousPosition, Position);
		if (PositionWorld == World.Get() && StepDistance >= 500.0)
		{ Check(false, TEXT("same_world_position_jump_invalidates_walking_evidence")); Finish(); return false; }
		if (Stage == EStage::FieldWalk)
		{
			const FVector Velocity = Player->GetVelocity();
			if (!FMath::IsFinite(Movement->MaxWalkSpeed) || !HasAuthoredEquippedWalkContract()
				|| Velocity.ContainsNaN() || Velocity.Size2D() > 650.0)
			{ Check(false, TEXT("field_walk_remains_unboosted_without_root_motion_speedup")); Finish(); return false; }
			MaximumFieldSpeedCmPerSecond = FMath::Max(MaximumFieldSpeedCmPerSecond, Velocity.Size2D());
		}
		if (Stage == EStage::FieldWalk && !bCrossedEntrance && PreviousPosition.Y < 30000.0 && Position.Y >= 30000.0 && FMath::Abs(Position.X) <= 400.0)
		{
			bCrossedEntrance = Movement->IsMovingOnGround();
			BossCrossingActiveSeconds = Run->GetRunDurationSeconds() - Run->GetRemainingRunSeconds();
			BossCrossingWallSeconds = FPlatformTime::Seconds() - FieldReadyAt;
			Event(TEXT("boss_entrance_crossing"), FString::Printf(TEXT("grounded=%d;active=%.3f;wall_after_field_ready=%.6f"), bCrossedEntrance, BossCrossingActiveSeconds, BossCrossingWallSeconds));
		}
		if (PositionWorld == World.Get())
		{
			WalkedDistanceCm += StepDistance;
			if (Stage == EStage::FieldWalk) { FieldWalkedDistanceCm += StepDistance; }
		}
		PositionWorld = World; PreviousPosition = Position;
		const double Distance = FVector::Dist2D(Position, Goal);
		if (Distance <= 65.0 && Movement->IsMovingOnGround()) { Event(TEXT("waypoint_reached"), FString::FromInt(Waypoint)); LastDistance = TNumericLimits<double>::Max(); ProgressAt = FPlatformTime::Seconds(); return true; }
		if (Distance < LastDistance - 10.0) { LastDistance = Distance; ProgressAt = FPlatformTime::Seconds(); }
		if (FPlatformTime::Seconds() - ProgressAt > 12.0) { Check(false, TEXT("walking_waypoint_progress_not_stuck")); Finish(); return false; }
		FVector Direction = Goal - Position; Direction.Z = 0.0; Direction.Normalize();
		PC->SetControlRotation(FRotator(-12.0, Direction.Rotation().Yaw, 0.0));
		Player->AddMovementInput(Direction, 1.0f, false);
		return false;
	}
	void SetRoute(TArray<FVector> Points)
	{
		Waypoints = MoveTemp(Points); Waypoint = 0; ProgressAt = FPlatformTime::Seconds(); LastDistance = TNumericLimits<double>::Max();
		if (Player.IsValid()) { PreviousPosition = Player->GetActorLocation(); PositionWorld = World; }
	}
	// Diagnostic only; no movement, equipment, or gameplay state is changed.

	bool HasAuthoredEquippedWalkContract() const
	{
		const auto Stats = Player->GetCharacterStatSnapshot();
		const auto* Inventory = Player->GetInventoryComponent();
		const auto Gear = Inventory->GetEquipmentStatModifiers();
		const auto* Identity = Player->FindComponentByClass<URogue10mWeaponIdentityComponent>();
		const auto* Data = Player->GetCombatComponent() ? Player->GetCombatComponent()->GetCharacterData() : nullptr;
		TSet<FString> Seen;
		for (const auto* Slots : {&Inventory->GetLeftEquipmentSlots(), &Inventory->GetRightEquipmentSlots()})
		{
			for (const auto& Slot : *Slots)
			{
				if (!Slot.bHasEquippedItem || !Slot.EquippedItem.bOccupied || !Slot.EquippedItem.ItemData) { continue; }
				const auto* Item = Slot.EquippedItem.ItemData.Get();
				const FString Name = Item->GetName();
				if (Seen.Contains(Name) || (Name != TEXT("DA_Item_IronHelmet") && Name != TEXT("DA_Item_LeatherArmor")
					&& Name != TEXT("DA_Item_LeatherBoots") && Name != TEXT("DA_Item_NoviceRing"))) { return false; }
				if (Item->GetPathName() != TEXT("/Game/DataAsset/Item/Starter/") + Name + TEXT(".") + Name
					|| !FMath::IsNearlyEqual(Item->EquipmentStats.MoveSpeedBonus, Name == TEXT("DA_Item_LeatherBoots") ? 20.0f : 0.0f, 0.01f)
					|| !FMath::IsNearlyZero(Item->EquipmentStats.MoveSpeedMultiplierBonus, 0.0001f)) { return false; }
				Seen.Add(Name);
			}
		}
		// Independent authored constants, not a target copied from actual movement.
		return Seen.Num() == 4 && Data && FMath::IsNearlyEqual(Data->WalkSpeed, 600.0f, 0.01f)
			&& FMath::IsNearlyEqual(Stats.Base.MoveSpeed, 600.0f, 0.01f)
			&& FMath::IsNearlyEqual(Gear.MoveSpeedBonus, 20.0f, 0.01f) && FMath::IsNearlyZero(Gear.MoveSpeedMultiplierBonus, 0.0001f)
			&& FMath::IsNearlyEqual(Stats.Equipment.MoveSpeedBonus, 20.0f, 0.01f) && FMath::IsNearlyZero(Stats.Equipment.MoveSpeedMultiplierBonus, 0.0001f)
			&& FMath::IsNearlyEqual(Stats.MoveSpeed, 600.0f + 20.0f, 0.01f)
			&& FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, 600.0f + 20.0f, 0.01f)
			&& Player->IsThirdPersonGameplayActive() && !Player->IsSprinting()
			&& (!Identity || (!Identity->IsIdentityActive() && FMath::IsNearlyEqual(Identity->GetMovementMultiplier(), 1.0f, 0.0001f)));
	}

	void CaptureMovementDiagnostics(bool bHub)
	{
		const auto Stats = Player->GetCharacterStatSnapshot();
		const auto* Inventory = Player->GetInventoryComponent();
		const auto Gear = Inventory->GetEquipmentStatModifiers();
		const auto* Identity = Player->FindComponentByClass<URogue10mWeaponIdentityComponent>();
		const auto* Combat = Player->GetCombatComponent();
		const auto* Data = Combat ? Combat->GetCharacterData() : nullptr;
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("pawn_class"), Player->GetClass()->GetPathName());
		Row->SetStringField(TEXT("game_mode_class"), World->GetAuthGameMode() ? World->GetAuthGameMode()->GetClass()->GetPathName() : TEXT("none"));
		Row->SetStringField(TEXT("character_data"), Data ? Data->GetPathName() : TEXT("native_stat_fallback"));
		Row->SetNumberField(TEXT("authored_data_walk"), Data ? Data->WalkSpeed : -1.0);
		Row->SetNumberField(TEXT("base_walk"), Stats.Base.MoveSpeed);
		Row->SetNumberField(TEXT("base_sprint"), Stats.Base.SprintSpeed);
		Row->SetNumberField(TEXT("gear_bonus"), Gear.MoveSpeedBonus);
		Row->SetNumberField(TEXT("gear_multiplier_bonus"), Gear.MoveSpeedMultiplierBonus);
		Row->SetNumberField(TEXT("combined_bonus"), Stats.Equipment.MoveSpeedBonus);
		Row->SetNumberField(TEXT("combined_multiplier_bonus"), Stats.Equipment.MoveSpeedMultiplierBonus);
		Row->SetNumberField(TEXT("derived_walk"), Stats.MoveSpeed);
		Row->SetNumberField(TEXT("derived_sprint"), Stats.SprintSpeed);
		Row->SetNumberField(TEXT("actual_max_walk"), Player->GetCharacterMovement()->MaxWalkSpeed);
		Row->SetNumberField(TEXT("velocity_2d"), Player->GetVelocity().Size2D());
		Row->SetNumberField(TEXT("identity_multiplier"), Identity ? Identity->GetMovementMultiplier() : 1.0f);
		Row->SetBoolField(TEXT("identity_active"), Identity && Identity->IsIdentityActive());
		Row->SetBoolField(TEXT("sprinting"), Player->IsSprinting());
		Row->SetBoolField(TEXT("third_person"), Player->IsThirdPersonGameplayActive());
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const auto* Slots : {&Inventory->GetLeftEquipmentSlots(), &Inventory->GetRightEquipmentSlots()})
		{
			for (const auto& Slot : *Slots)
			{
				if (!Slot.bHasEquippedItem || !Slot.EquippedItem.bOccupied || !Slot.EquippedItem.ItemData) { continue; }
				auto Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("asset"), Slot.EquippedItem.ItemData->GetPathName());
				Item->SetNumberField(TEXT("move_speed_bonus"), Slot.EquippedItem.ItemData->EquipmentStats.MoveSpeedBonus);
				Item->SetNumberField(TEXT("move_speed_multiplier_bonus"), Slot.EquippedItem.ItemData->EquipmentStats.MoveSpeedMultiplierBonus);
				Items.Add(MakeShared<FJsonValueObject>(Item));
			}
		}
		Row->SetArrayField(TEXT("equipped_items"), Items);
		if (bHub) { HubMovementDiagnostic = Row; } else { CandidateMovementDiagnostic = Row; }
		FString Serialized; auto Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Row, Writer);
		Event(bHub ? TEXT("hub_movement_diagnostic") : TEXT("candidate_movement_diagnostic"), Serialized);
		UE_LOG(LogRogue10m, Display, TEXT("FIVE_VILLAGE_MOVEMENT_DIAGNOSTIC %s %s"), bHub ? TEXT("hub") : TEXT("candidate"), *Serialized);
	}
	void Prepare()
	{
		if (!World.IsValid() || !PC.IsValid() || !Saves.IsValid()) { return; }
		auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		if (!bProfileCreated)
		{
			FRogue10mCharacterProfile Profile;
			if (!Check(Profiles && Profiles->CreateProfile(TEXT("VillageQA"), FRogue10mCharacterAppearance(), Profile)
				&& Profiles->SelectProfile(Profile.ProfileId), TEXT("fresh_profile_created_selected_through_public_API"))) { Finish(); return; }
			ProfileId = Profile.ProfileId; bProfileCreated = true; PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
			NextAt = FPlatformTime::Seconds() + 0.3; return;
		}
		if (!Ready() || !Player->GetCharacterMovement()->IsMovingOnGround()) { return; }
		if (!Check(Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && Saves->IsAttachedTo(*Player) && Saves->IsSaveReady()
			&& Saves->GetAttachedProfileId() == ProfileId, TEXT("normal_hub_avatar_profile_and_waiting_save_contract"))) { Finish(); return; }
		if (!Check(Player->IsThirdPersonGameplayActive(), TEXT("hub_uses_production_third_person"))) { Finish(); return; }
		TArray<ARogue10mFiveVillageRunPortal*> Gates; int32 OriginalCount = 0;
		for (TActorIterator<ARogue10mRunPortal> It(World.Get()); It; ++It)
		{
			if (auto* Gate = Cast<ARogue10mFiveVillageRunPortal>(*It)) { Gates.Add(Gate); }
			else if (It->Tags.Contains(FName(TEXT("Rogue10m.StartVillage")))) { ++OriginalCount; }
		}
		if (!Check(Gates.Num() == 1 && OriginalCount == 1, TEXT("persisted_new_entry_and_original_portal_exist"))) { Finish(); return; }
		const FVector Center = Gates[0]->GetActorLocation(), Axis = Gates[0]->GetActorRightVector();
		EntryRunId = Saves->GetAttachedRunId(); auto* Inventory = Player->GetInventoryComponent(); auto* Town = State->GetTownProgressionComponent();
		if (!Check(EntryRunId.IsValid() && Inventory && Town, TEXT("hub_run_identity_and_currency_components_exist"))) { Finish(); return; }
		BaselineGold = Inventory->GetGold(); BaselineCrystals = Inventory->GetCrystals(); BaselineLobbyEarned = Town->GetLobbyEarnedBudget(); BaselineLobbyAvailable = Saves->GetLobbyAvailablePoints();
		CaptureMovementDiagnostics(true);
		if (!Check(HasAuthoredEquippedWalkContract(), TEXT("hub_authored_equipped_walk_contract_600_plus_boots20"))) { Finish(); return; }
		EntryInventory = Inventory->CaptureInventory();
		SetRoute({FVector(650, -1100, 0), FVector(650, 950, 0), FVector(0, 1800, 0), Center - Axis * 300.0, Center + Axis * 300.0});
		Stage = EStage::HubWalk; Event(TEXT("normal_portal_walk_started"), EntryRunId.ToString(EGuidFormats::Digits));
	}
	void EnterField()
	{
		if (FPlatformTime::Seconds() > StageDeadline) { Check(false, TEXT("candidate_avatar_ready_before_deadline")); Finish(); return; }
		if (!Ready()) { return; }
		if (Map() != Field)
		{
			if (Map() == Hub && FPlatformTime::Seconds() < StageDeadline) { return; }
			Check(false, TEXT("native_portal_overlap_reached_expected_world")); Finish(); return;
		}
		if (!Player->GetCharacterMovement()->IsMovingOnGround() || !Saves->IsAttachedTo(*Player) || !Saves->IsSaveReady()) { return; }
		FieldStartPosition = Player->GetActorLocation();
		if (!Check(FVector::Dist2D(FieldStartPosition, FVector::ZeroVector) <= 250.0,
			TEXT("candidate_starts_at_authored_S_before_any_walking"))) { Finish(); return; }
		auto* Mode = Cast<ARogue10mFiveVillageGameMode>(World->GetAuthGameMode());
		auto* Progress = State->FindComponentByClass<URogue10mFiveVillageProgressComponent>();
		auto* Inventory = Player->GetInventoryComponent();
		ObservedRunDuration = Run->GetRunDurationSeconds();
		if (!Check(Mode && Progress && Run->IsRunActive() && FMath::IsNearlyEqual(ObservedRunDuration, 1200.0f)
			&& Run->GetRemainingRunSeconds() > 1190.0f && Run->GetRemainingRunSeconds() <= 1200.0f,
			TEXT("candidate_mode_component_and_normal_1200_second_run"))) { Finish(); return; }
		if (!Check(Saves->IsAttachedTo(*Player) && Saves->IsSaveReady() && Saves->GetAttachedProfileId() == ProfileId
			&& Saves->GetAttachedRunId() == EntryRunId, TEXT("normal_travel_keeps_profile_and_entry_run_identity"))) { Finish(); return; }
		const auto FieldInventory = Inventory ? Inventory->CaptureInventory() : FRogue10mInventorySnapshot();
		if (!Check(Inventory && FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&EntryInventory, &FieldInventory, 0),
			TEXT("normal_travel_preserves_inventory_instance_identity"))) { Finish(); return; }
		CaptureMovementDiagnostics(false);
		if (!Check(HasAuthoredEquippedWalkContract(), TEXT("candidate_authored_equipped_walk_contract_600_plus_boots20"))) { Finish(); return; }
		bool SameNormal = HubMovementDiagnostic.IsValid() && CandidateMovementDiagnostic.IsValid();
		for (const TCHAR* Key : {TEXT("authored_data_walk"), TEXT("base_walk"), TEXT("base_sprint"), TEXT("gear_bonus"), TEXT("gear_multiplier_bonus"), TEXT("combined_bonus"), TEXT("combined_multiplier_bonus"), TEXT("derived_walk"), TEXT("derived_sprint"), TEXT("actual_max_walk"), TEXT("identity_multiplier")})
		{
			SameNormal &= HubMovementDiagnostic->GetNumberField(Key) == CandidateMovementDiagnostic->GetNumberField(Key);
		}
		SameNormal &= HubMovementDiagnostic->GetStringField(TEXT("pawn_class")) == CandidateMovementDiagnostic->GetStringField(TEXT("pawn_class"))
			&& HubMovementDiagnostic->GetStringField(TEXT("character_data")) == CandidateMovementDiagnostic->GetStringField(TEXT("character_data"));
		if (!Check(SameNormal, TEXT("travel_preserves_hub_authored_equipped_movement_snapshot"))) { Finish(); return; }
		Milestones[TEXT("normal_entry")] = true; Milestones[TEXT("third_person")] = true;
		FieldReadyAt = FPlatformTime::Seconds(); FieldInitialRemaining = Run->GetRemainingRunSeconds();
		if (Scenario == TEXT("Direct")) { SetRoute({FVector(0, 30350, 0)}); }
		else { SetRoute({FVector(-14000,8000,0), FVector(-30000,23000,0), FVector(0,23000,0), FVector(6000,23000,0),
			FVector(0,18500,0), FVector(-14000,8000,0), FVector(0,18500,0), FVector(6000,23000,0), FVector(0,30350,0)}); }
		Stage = EStage::FieldWalk; Event(TEXT("field_route_started"), Scenario);
	}
	void ObserveRouteAndAbandon()
	{
		if (!Ready() || Map() != Field || !Run->IsRunActive()) { Check(false, TEXT("route_observation_has_live_candidate_context")); Finish(); return; }
		auto* Progress = State->FindComponentByClass<URogue10mFiveVillageProgressComponent>();
		if (!Check(Progress && bCrossedEntrance && Progress->HasEnteredBossApproach(), TEXT("grounded_actual_entrance_crossing_and_component_approach_observed"))) { Finish(); return; }
		for (int32 Index = 1; Index <= 5; ++Index) { const FName Id(*FString::Printf(TEXT("V%d"), Index)); Visits.Add(Id.ToString(), Progress->GetVillageVisitCount(Id)); }
		const bool Direct = Scenario == TEXT("Direct");
		const bool RouteGood = Direct ? (Progress->GetVisitedVillageIds().IsEmpty())
			: (Visits[TEXT("V1")] >= 2 && Visits[TEXT("V3")] >= 1 && Visits[TEXT("V4")] >= 2 && Visits[TEXT("V2")] == 0 && Visits[TEXT("V5")] == 0);
		if (!Check(RouteGood, TEXT("direct_bypasses_all_villages_or_revisit_route_preserves_observations"))) { Finish(); return; }
		if (!Check(!Direct || FieldWalkedDistanceCm >= 29500.0, TEXT("direct_crossing_includes_full_native_field_walk_distance"))) { Finish(); return; }
		TimerAdvancedSeconds = FieldInitialRemaining - Run->GetRemainingRunSeconds();
		if (!Check(TimerAdvancedSeconds > 0.0f && (!Direct || (BossCrossingActiveSeconds >= 0.0 && BossCrossingActiveSeconds <= 60.0)),
			TEXT("live_timer_advanced_and_direct_crossing_within_60_active_seconds"))) { Finish(); return; }
		int32 Gold, Crystals, Lobby;
		if (!Check(Saves->TryResolveAbandonCurrencyBalances(Gold, Crystals, Lobby) && Gold == BaselineGold && Crystals == BaselineCrystals
			&& Lobby == BaselineLobbyEarned, TEXT("actual_portal_saved_matching_entry_currency_baseline"))) { Finish(); return; }
		Milestones[TEXT("route_observations")] = true; Milestones[TEXT("timer_running")] = true;
		if (!Check(Run->AbandonRun(), TEXT("normal_public_abandon_request_accepted"))) { Finish(); return; }
		bAbandonAccepted = true; Stage = EStage::SettlementWait; StageDeadline = FPlatformTime::Seconds() + 15.0;
		NextAt = FPlatformTime::Seconds() + 0.1; Event(TEXT("abandon_requested"));
	}
	void ObserveSettlement()
	{
		if (FPlatformTime::Seconds() > StageDeadline) { Check(false, TEXT("abandon_settlement_saved_before_deadline")); Finish(); return; }
		if (!Ready() || Map() != Field) { return; }
		auto* Mode = Cast<ARogue10mGameMode>(World->GetAuthGameMode()); auto* Completion = Mode ? Mode->GetRunCompletionComponent() : nullptr;
		if (!Completion || !Completion->IsSettled() || !Completion->IsProgressionSaved()) { return; }
		const auto Economy = Completion->GetEconomySummary();
		ObservedLobbyAward = Economy.bAvailable ? Economy.LobbyAward : -1;
		if (!Check(Run->GetRunPhase() == ERogue10mRunPhase::Abandoned && Completion->IsAbandoned() && !Completion->IsVictory()
			&& Economy.bAvailable && Economy.bSaved && Economy.bAbandoned && Economy.LobbyAward == 0
			&& Economy.RunGold == BaselineGold && Economy.RunCrystals == BaselineCrystals && Economy.LobbyAvailable == BaselineLobbyAvailable,
			TEXT("normal_abandon_settlement_saved_without_award_and_with_baseline_currency"))) { Finish(); return; }
		const FString Slot = Saves->GetSaveSlotForProfile(ProfileId);
		auto* Snapshot = Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Check(Snapshot && Snapshot->SaveVersion == URogue10mProgressionSaveGame::CurrentVersion && Snapshot->ProfileId == ProfileId
			&& Snapshot->RunId == EntryRunId && Snapshot->LastSettledRunId == EntryRunId && Snapshot->bRunCompleted && Snapshot->bAbandoned
			&& Snapshot->EntryCurrencies.IsValid() && Snapshot->EntryCurrencies.RunId == EntryRunId
			&& Snapshot->EntryCurrencies.Gold == BaselineGold && Snapshot->EntryCurrencies.Crystals == BaselineCrystals
			&& Snapshot->EntryCurrencies.LobbyEarnedPoints == BaselineLobbyEarned,
			TEXT("real_v5_abandon_receipt_read_before_hub_starts_next_run"))) { Finish(); return; }
		SettledRunId = Snapshot->RunId; Milestones[TEXT("settled")] = true; Milestones[TEXT("saved")] = true; bBaselineRestored = true;
		Event(TEXT("settlement_receipt_verified"), SettledRunId.ToString(EGuidFormats::Digits));
		if (!Check(Completion->ReturnToTown(), TEXT("normal_saved_result_return_to_town_accepted"))) { Finish(); return; }
		Stage = EStage::HubWait; StageDeadline = FPlatformTime::Seconds() + 30.0; NextAt = FPlatformTime::Seconds() + 0.1;
	}
	void ObserveReturn()
	{
		if (FPlatformTime::Seconds() > StageDeadline) { Check(false, TEXT("normal_hub_return_before_deadline")); Finish(); return; }
		if (!Ready() || Map() != Hub) { return; }
		auto* Profiles = Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>(); const auto* Profile = Profiles ? Profiles->GetSelectedProfile() : nullptr;
		if (!Saves->IsAttachedTo(*Player) || !Saves->IsSaveReady() || !Profile) { return; }
		ReturnProfileId = Profile->ProfileId; ReturnRunId = Saves->GetAttachedRunId();
		auto* Inventory = Player->GetInventoryComponent(); auto* Town = State->GetTownProgressionComponent();
		if (!Check(ReturnProfileId == ProfileId && Saves->GetAttachedProfileId() == ProfileId && ReturnRunId.IsValid() && ReturnRunId != SettledRunId
			&& Run->GetRunPhase() == ERogue10mRunPhase::WaitingToStart && !World->URL.HasOption(TEXT("StartRun"))
			&& Player->IsThirdPersonGameplayActive() && Inventory && Town && Inventory->GetGold() == BaselineGold
			&& Inventory->GetCrystals() == BaselineCrystals && Town->GetLobbyEarnedBudget() == BaselineLobbyEarned
			&& Saves->GetLobbyAvailablePoints() == BaselineLobbyAvailable,
			TEXT("returned_hub_preserves_profile_baseline_and_creates_new_waiting_run"))) { Finish(); return; }
		Milestones[TEXT("returned_hub")] = true; Milestones[TEXT("profile_preserved")] = true;
		Event(TEXT("normal_return_verified"), ReturnRunId.ToString(EGuidFormats::Digits)); Finish();
	}
	void Finish()
	{
		if (bFinished) { return; } bFinished = true;
		auto Json = MakeShared<FJsonObject>(); Json->SetNumberField(TEXT("schema_version"), 1); Json->SetStringField(TEXT("label"), Label);
		Json->SetStringField(TEXT("scenario"), Scenario); Json->SetNumberField(TEXT("checks"), Checks); Json->SetNumberField(TEXT("failures"), Failures);
		auto Marks = MakeShared<FJsonObject>(); for (const auto& Pair : Milestones) { Marks->SetBoolField(Pair.Key, Pair.Value); }
		Json->SetObjectField(TEXT("milestones"), Marks); Json->SetArrayField(TEXT("check_details"), CheckRows);
		Json->SetBoolField(TEXT("actual_walking"), WalkedDistanceCm > 500.0); Json->SetBoolField(TEXT("physical_input"), false);
		Json->SetBoolField(TEXT("full_run"), false); Json->SetBoolField(TEXT("rendered"), Rendered);
		Json->SetStringField(TEXT("movement_method"), TEXT("AddMovementInput native CharacterMovement"));
		Json->SetStringField(TEXT("outcome"), bAbandonAccepted ? TEXT("Abandoned") : TEXT("NotReached"));
		Json->SetBoolField(TEXT("victory_verified"), false); Json->SetBoolField(TEXT("economic_delta_exercised"), false);
		auto Ids = MakeShared<FJsonObject>(); Ids->SetStringField(TEXT("profile_id"), ProfileId.ToString(EGuidFormats::Digits));
		Ids->SetStringField(TEXT("entry_run_id"), EntryRunId.ToString(EGuidFormats::Digits)); Ids->SetStringField(TEXT("settled_run_id"), SettledRunId.ToString(EGuidFormats::Digits));
		Ids->SetStringField(TEXT("return_profile_id"), ReturnProfileId.ToString(EGuidFormats::Digits)); Ids->SetStringField(TEXT("return_run_id"), ReturnRunId.ToString(EGuidFormats::Digits)); Json->SetObjectField(TEXT("ids"), Ids);
		auto MovementDiagnostics = MakeShared<FJsonObject>();
		if (HubMovementDiagnostic) { MovementDiagnostics->SetObjectField(TEXT("hub"), HubMovementDiagnostic); }
		if (CandidateMovementDiagnostic) { MovementDiagnostics->SetObjectField(TEXT("candidate"), CandidateMovementDiagnostic); }
		Json->SetObjectField(TEXT("movement_diagnostics"), MovementDiagnostics);
		auto Timing = MakeShared<FJsonObject>(); Timing->SetNumberField(TEXT("run_duration_seconds"), ObservedRunDuration); Timing->SetNumberField(TEXT("timer_advanced_seconds"), TimerAdvancedSeconds);
		Timing->SetNumberField(TEXT("boss_crossing_active_seconds"), BossCrossingActiveSeconds); Timing->SetNumberField(TEXT("boss_crossing_wall_after_field_ready_seconds"), BossCrossingWallSeconds);
		Timing->SetNumberField(TEXT("total_wall_seconds"), FPlatformTime::Seconds() - StartedAt); Timing->SetNumberField(TEXT("walked_distance_cm"), WalkedDistanceCm); Json->SetObjectField(TEXT("timing"), Timing);
		Timing->SetNumberField(TEXT("field_walked_distance_cm"), FieldWalkedDistanceCm); Timing->SetNumberField(TEXT("maximum_field_speed_cm_per_second"), MaximumFieldSpeedCmPerSecond);
		Timing->SetNumberField(TEXT("initial_field_x_cm"), FieldStartPosition.X); Timing->SetNumberField(TEXT("initial_field_y_cm"), FieldStartPosition.Y);
		auto Observed = MakeShared<FJsonObject>(); for (int32 Index = 1; Index <= 5; ++Index) { const FString Id = FString::Printf(TEXT("V%d"), Index); Observed->SetNumberField(Id, Visits.FindRef(Id)); } Json->SetObjectField(TEXT("visits"), Observed);
		auto Currency = MakeShared<FJsonObject>(); Currency->SetBoolField(TEXT("baseline_restored"), bBaselineRestored); Currency->SetNumberField(TEXT("lobby_award"), ObservedLobbyAward);
		Currency->SetNumberField(TEXT("baseline_gold"), BaselineGold); Currency->SetNumberField(TEXT("baseline_crystals"), BaselineCrystals);
		Currency->SetNumberField(TEXT("baseline_lobby_earned"), BaselineLobbyEarned); Currency->SetNumberField(TEXT("baseline_lobby_available"), BaselineLobbyAvailable); Json->SetObjectField(TEXT("currency"), Currency);
		const bool EventsOk = FFileHelper::SaveStringToFile(Events, *(Output / TEXT("events.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (!EventsOk) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("FIVE_VILLAGE_TRAVEL event_write_failed")); }
		Json->SetNumberField(TEXT("failures"), Failures); Json->SetStringField(TEXT("status"), Failures ? TEXT("failed") : TEXT("passed"));
		FString Serialized; auto Writer = TJsonWriterFactory<>::Create(&Serialized); const bool SerializedOk = FJsonSerializer::Serialize(Json, Writer);
		const bool ReportOk = SerializedOk && FFileHelper::SaveStringToFile(Serialized, *(Output / TEXT("results.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (!ReportOk) { ++Failures; UE_LOG(LogRogue10m, Error, TEXT("FIVE_VILLAGE_TRAVEL report_write_failed")); }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=FIVE_VILLAGE_TRAVEL_%s label=%s scenario=%s checks=%d failures=%d output=%s"), Failures ? TEXT("FAILED") : TEXT("PASSED"), *Label, *Scenario, Checks, Failures, *Output);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}

	TSharedPtr<FJsonObject> HubMovementDiagnostic, CandidateMovementDiagnostic;
	TWeakObjectPtr<UGameInstance> Instance; TWeakObjectPtr<UWorld> World, PositionWorld;
	TWeakObjectPtr<ARogue10mPlayerController> PC; TWeakObjectPtr<ARogue10mCharacter> Player; TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<ARogue10mGameState> Run; TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;
	FString Label, Scenario, Output, Events; TMap<FString, bool> Milestones; TMap<FString, int32> Visits;
	TArray<TSharedPtr<FJsonValue>> CheckRows; TArray<FVector> Waypoints; FRogue10mInventorySnapshot EntryInventory;
	FGuid ProfileId, EntryRunId, SettledRunId, ReturnProfileId, ReturnRunId; EStage Stage = EStage::Prepare;
	FVector PreviousPosition, FieldStartPosition; double StartedAt = 0, Deadline = 0, StageDeadline = 0, NextAt = 0, NextSampleAt = 0, ProgressAt = 0, LastDistance = TNumericLimits<double>::Max();
	double FieldReadyAt = 0, BossCrossingActiveSeconds = -1, BossCrossingWallSeconds = -1, WalkedDistanceCm = 0, FieldWalkedDistanceCm = 0, MaximumFieldSpeedCmPerSecond = 0;
	float FieldInitialRemaining = 0, TimerAdvancedSeconds = 0, ObservedRunDuration = -1;
	int32 Checks = 0, Failures = 0, Waypoint = 0, BaselineGold = 0, BaselineCrystals = 0, BaselineLobbyEarned = 0, BaselineLobbyAvailable = 0, ObservedLobbyAward = -1;
	bool bFinished = false, bProfileCreated = false, bCrossedEntrance = false, bBaselineRestored = false, bAbandonAccepted = false, Rendered = false;
};

static TSharedPtr<FRun> Active;
static void Begin(UWorld* World)
{
	if (GIsEditor || (Active && !Active->Finished())) { return; }
	if (!World && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				if (World) { UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIVE_VILLAGE_TRAVEL_FAILED ambiguous_game_world")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
				World = Context.World();
			}
		}
	}
	if (!World) { UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIVE_VILLAGE_TRAVEL_FAILED game_world_required")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
	Active = MakeShared<FRun>(); Active->Start(*World);
}
static FAutoConsoleCommandWithWorld Command(TEXT("Rogue10m.TestFiveVillageTravel"),
	TEXT("Fresh bounded QA slots only. Native walking through persisted five-village portal; abandon settlement and return."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Begin));
}
#endif
