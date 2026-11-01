// Normal UI/input round trip on a byte-identical clone of one closed, settled QA profile.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAbandonMenuWidget.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterLobbyWidget.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mGameState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMainMenuWidget.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mRunResultWidget.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitWidget.h"
#include "Rogue10mTutorialNPC.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace Rogue10mLobbyUpgradeRoundTripQA
{
static FString Csv(FString S) { return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")) + TEXT("\""); }
static bool SafeLabel(const FString& S)
{
	if (S.IsEmpty() || S.Len() > 20) { return false; }
	for (TCHAR C : S) { if (!((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_'))) { return false; } }
	return true;
}
static int32 Rank(const FRogue10mTownProgressionSnapshot& S)
{
	const auto* Row = S.TraitInvestments.FindByPredicate([](const auto& R) { return R.Trait == ERogue10mTownTrait::Power; });
	return Row ? Row->Rank : 0;
}
static int32 Spent(const FRogue10mTownProgressionSnapshot& S)
{
	int64 Sum = 0; for (const auto& Row : S.TraitInvestments) { Sum += Row.SpentPoints; }
	return static_cast<int32>(FMath::Clamp<int64>(Sum, 0, MAX_int32));
}
class FRun : public TSharedFromThis<FRun>
{
public:
	bool Finished() const { return bFinished; }
	void Start(UWorld& Initial)
	{
		Instance = Initial.GetGameInstance(); World = &Initial;
		FString Id;
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyUpgradeLabel="), Label);
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyUpgradeSourceLabel="), SourceLabel);
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyUpgradeExpectedProfile="), Id);
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyUpgradeOutput="), Output);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), TownSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), Prefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), KeySlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueLobbyUpgradeMaxSeconds="), MaxSeconds);
		FString Expected = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("DevLog/LobbyUpgradeRoundTripQA") / Label / TEXT("Driver"));
		const bool HasOutput = !Output.IsEmpty(); Output = HasOutput ? FPaths::ConvertRelativePathToFull(Output) : FString();
		FPaths::NormalizeDirectoryName(Expected); FPaths::NormalizeDirectoryName(Output);
		FPaths::CollapseRelativeDirectories(Expected); FPaths::CollapseRelativeDirectories(Output);
		auto* Profiles = Instance.IsValid() ? Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		bool Valid = SafeLabel(Label) && (SourceLabel == TEXT("BasicRun1101H") || SourceLabel == TEXT("BasicRun1101I"))
			&& FGuid::ParseExact(Id, EGuidFormats::Digits, Profile) && Profile.IsValid() && HasOutput && Output.Equals(Expected, ESearchCase::IgnoreCase)
			&& TownSlot == TEXT("Rogue10m_Town_QA_LobbyUpgrade_") + Label
			&& Prefix == TEXT("Rogue10m_Progression_QA_LobbyUpgrade_") + Label && KeySlot == TEXT("Rogue10m_Fighter_QA_LobbyUpgrade_") + Label
			&& TownSlot.Len() <= 80 && Prefix.Len() <= 80 && KeySlot.Len() <= 80 && MaxSeconds >= 120 && MaxSeconds <= 600
			&& Profiles && Profiles->GetProfileCount() == 1 && Profiles->GetProfiles()[0].ProfileId == Profile
			&& UGameplayStatics::DoesSaveGameExist(TownSlot, 0) && Initial.GetNetMode() == NM_Standalone
			&& UGameplayStatics::GetCurrentLevelName(&Initial, true) == TEXT("L_Menu")
			&& !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && GEngine && GEngine->GameViewport
			&& !IFileManager::Get().DirectoryExists(*Output) && ValidateCloneManifest();
		const auto* Disk = Valid ? LoadDisk() : nullptr;
		Valid &= Disk && Disk->ProfileId == Profile && Disk->SaveVersion == URogue10mProgressionSaveGame::CurrentVersion
			&& !Disk->bEnteredRun && !Disk->bRunCompleted;
		if (!Valid)
		{
			UE_LOG(LogRogue10m, Error, TEXT("LOBBY_UPGRADE_REJECTED verified_clone_fresh_output_profile1_normal_L_Menu_required"));
			bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 2); return;
		}
		Discovery = Disk->DiscoveredNodes;
		if (Discovery.IsEmpty()) { bFinished = true; UE_LOG(LogRogue10m, Error, TEXT("LOBBY_UPGRADE_REJECTED source_Discovery_empty")); FPlatformMisc::RequestExitWithStatus(false, 2); return; }
		if (!IFileManager::Get().MakeDirectory(*(Output / TEXT("screenshots")), true)) { bFinished = true; FPlatformMisc::RequestExitWithStatus(false, 2); return; }
		Started = Now(); StageAt = Started;
		ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak = TWeakPtr<FRun>(AsShared())]
		{ if (const auto S = Weak.Pin(); S && !S->bFinished) { S->Block(TEXT("external_exit_before_roundtrip"), false); } });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float Dt) { return Self->Tick(Dt); }), 0.f);
		Event(TEXT("start"), TEXT("verified_clone_seed_only; Controller_InputKey_DoAim_visible_UMG_delegates; no_physical_input"));
	}
private:
	int32 PortalWaypoint = 0;
	static double Now() { return FPlatformTime::Seconds(); }
	FString Map() const { return World.IsValid() ? UGameplayStatics::GetCurrentLevelName(World.Get(), true) : FString(); }
	const URogue10mProgressionSaveGame* LoadDisk() const
	{
		return Cast<URogue10mProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Prefix + TEXT("_") + Profile.ToString(EGuidFormats::Digits) + TEXT("_v5"), 0));
	}
	bool ValidateCloneManifest() const
	{
		FString Text; TSharedPtr<FJsonObject> Json;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::GetPath(Output) / TEXT("clone-manifest.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid()) { return false; }
		const auto Match = [&Json](const TCHAR* Key, const FString& Expected)
		{ FString Value; return Json->TryGetStringField(Key, Value) && Value.Equals(Expected, ESearchCase::CaseSensitive); };
		bool Verified = false; FString ManifestOutput;
		if (!Json->TryGetStringField(TEXT("DriverOutput"), ManifestOutput)) { return false; }
		ManifestOutput = FPaths::ConvertRelativePathToFull(ManifestOutput); FPaths::NormalizeDirectoryName(ManifestOutput); FPaths::CollapseRelativeDirectories(ManifestOutput);
		return Match(TEXT("Schema"), TEXT("Rogue10m.LobbyUpgradeClone.v1")) && Match(TEXT("Label"), Label)
			&& Match(TEXT("SourceLabel"), SourceLabel) && Match(TEXT("ExpectedProfile"), Profile.ToString(EGuidFormats::Digits))
			&& Match(TEXT("ProfileSlot"), TownSlot) && Match(TEXT("ProgressionPrefix"), Prefix) && Match(TEXT("KeySlot"), KeySlot)
			&& Json->TryGetBoolField(TEXT("CloneVerified"), Verified) && Verified && ManifestOutput.Equals(Output, ESearchCase::IgnoreCase);
	}
	void Event(const TCHAR* Kind, const FString& Detail)
	{
		Events += FString::Printf(TEXT("%.3f,%s,%s,%s\n"), Started ? Now() - Started : 0., *Csv(Stage), *Csv(Kind), *Csv(Detail));
		UE_LOG(LogRogue10m, Display, TEXT("LOBBY_UPGRADE %s stage=%s detail=%s"), Kind, *Stage, *Detail);
	}
	bool Check(bool Valid, const TCHAR* Name)
	{
		++Checks; if (!Valid) { ++Failures; }
		Event(Valid ? TEXT("PASS") : TEXT("FAIL"), Name); return Valid;
	}
	void Key(const FKey& K, bool Down)
	{
		if (!PC.IsValid() || Held.FindRef(K) == Down) { return; }
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(K, Down ? IE_Pressed : IE_Released, Down ? 1.f : 0.f)); Held.FindOrAdd(K) = Down;
	}
	void Pulse(const FKey& K) { if (!Held.FindRef(K)) { Key(K, true); Releases.FindOrAdd(K) = Now() + .14; } }
	void ReleaseAll() { TArray<FKey> Keys; Held.GenerateKeyArray(Keys); for (const auto& K : Keys) { Key(K, false); } Releases.Empty(); }
	void StopMove() { for (const FKey K : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftShift}) { Key(K, false); } }
	void SetStage(const TCHAR* Next, double Limit = 30.)
	{
		ReleaseAll(); Stage = Next; StageAt = Now(); StageLimit = Limit; ActionAt = Now() + .2; StuckAt = Now();
		if (Player.IsValid()) { LastPosition = Player->GetActorLocation(); } Event(TEXT("stage"), Stage);
	}
	void Refresh()
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (UWorld* W = C.World(); W && W->WorldType == EWorldType::Game && W->GetGameInstance() == Instance.Get())
			{
				auto* NextPC = Cast<ARogue10mPlayerController>(W->GetFirstPlayerController());
				if (PC.Get() != NextPC) { ReleaseAll(); Held.Empty(); Releases.Empty(); }
				World = W; PC = NextPC; Player = PC.IsValid() ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
				State = Player.IsValid() ? Player->GetPlayerState<ARogue10mPlayerState>() : nullptr;
				Town = State.IsValid() ? State->GetTownProgressionComponent() : nullptr;
				Inventory = Player.IsValid() ? Player->GetInventoryComponent() : nullptr;
				Save = Instance->GetSubsystem<URogue10mProgressionSubsystem>(); return;
			}
		}
	}
	template <typename T> T* Visible() const
	{
		for (TObjectIterator<T> It; It; ++It) { if (It->GetWorld() == World.Get() && It->GetOwningPlayer() == PC.Get() && It->IsInViewport() && It->IsVisible()) { return *It; } }
		return nullptr;
	}
	static bool Displayed(UWidget* W)
	{
		if (!W || !W->GetCachedWidget().IsValid()) { return false; }
		for (UWidget* C = W; C; C = C->GetParent()) { if (!C->IsVisible() || !C->GetIsEnabled()) { return false; } }
		return true;
	}
	bool Click(UUserWidget* W, const TCHAR* Name)
	{
		auto* B = W ? Cast<UButton>(W->GetWidgetFromName(Name)) : nullptr;
		if (!Displayed(W) || !Displayed(B)) { return false; }
		Event(TEXT("visible_UMG_delegate"), Name); B->OnClicked.Broadcast(); return true;
	}
	bool Walk(const FVector& Goal, float Radius, float Dt)
	{
		if (!Player.IsValid() || !PC.IsValid()) { return false; }
		const FVector P = Player->GetActorLocation();
		if (FVector::Dist2D(P, Goal) <= Radius) { StopMove(); return true; }
		const FRotator Want = (FVector(Goal.X, Goal.Y, P.Z) - P).Rotation();
		const FRotator Current = PC->GetControlRotation();
		const float Yaw = static_cast<float>(FMath::FindDeltaAngleDegrees(Current.Yaw, Want.Yaw));
		const float Pitch = static_cast<float>(FMath::FindDeltaAngleDegrees(Current.Pitch, 0.));
		struct FAim { float Yaw; float Pitch; } Aim { FMath::Clamp(Yaw, -80.f * Dt, 80.f * Dt), FMath::Clamp(Pitch, -60.f * Dt, 60.f * Dt) };
		UFunction* Fn = Player->FindFunction(TEXT("DoAim"));
		if (!Fn || Fn->ParmsSize != sizeof(Aim)) { Block(TEXT("normal_Character_DoAim_API_missing")); return false; }
		Player->ProcessEvent(Fn, &Aim);
		Key(EKeys::W, FMath::Abs(Yaw) < 15.f); Key(EKeys::A, false); Key(EKeys::S, false); Key(EKeys::D, false); Key(EKeys::LeftShift, false);
		if (FVector::Dist2D(P, LastPosition) > 60.f) { LastPosition = P; StuckAt = Now(); }
		if (Now() - StuckAt > 12.) { Block(TEXT("normal_walk_stuck_no_teleport_fallback")); }
		return false;
	}
	bool Attached() const { return Player.IsValid() && Save.IsValid() && Save->IsAttachedTo(*Player) && Save->GetAttachedProfileId() == Profile && Save->IsSaveReady(); }
	bool Discovered() const { if (!Save.IsValid()) { return false; } for (const FName Id : Discovery) { if (!Save->HasDiscoveredNode(Id)) { return false; } } return true; }
	void Shot(const TCHAR* Name)
	{
		const FString File = Output / TEXT("screenshots") / Name; Photos.Add(File);
		FScreenshotRequest::RequestScreenshot(File, true, false); Event(TEXT("screenshot_requested_not_visually_reviewed"), File);
	}
	bool Tick(float Dt)
	{
		if (bFinished) { return false; }
		Refresh();
		TArray<FKey> Up; for (const auto& Pair : Releases) { if (Now() >= Pair.Value) { Up.Add(Pair.Key); } }
		for (const auto& K : Up) { Key(K, false); Releases.Remove(K); }
		if (Now() - Started > MaxSeconds || Now() - StageAt > StageLimit) { Block(TEXT("bounded_timeout_") + Stage); return false; }
		if (!World.IsValid() || !PC.IsValid()) { return true; }
		if (Now() - LastSample > .5)
		{
			Samples += FString::Printf(TEXT("%.3f,%s,%s,%d,%d,%d,%d,%d,%d\n"), Now() - Started, *Csv(Stage), *Csv(Map()), State.IsValid() ? State->GetPlayerLevel() : -1,
				Town.IsValid() ? Town->GetTraitLevel(ERogue10mTownTrait::Power) : -1, Town.IsValid() ? Town->GetAvailableTraitPoints() : -1,
				Inventory.IsValid() ? Inventory->GetGold() : -1, Inventory.IsValid() ? Inventory->GetCrystals() : -1, Attached()); LastSample = Now();
		}
		if (Stage == TEXT("main_menu"))
		{ if (Click(Visible<URogue10mMainMenuWidget>(), TEXT("UI_StartGameButton"))) { SetStage(TEXT("select_existing")); } }
		else if (Stage == TEXT("select_existing"))
		{
			if (auto* W = Visible<URogue10mCharacterLobbyWidget>(); W && Now() > ActionAt && Click(W, TEXT("UI_Slot1Button"))) { SetStage(TEXT("enter_existing")); }
		}
		else if (Stage == TEXT("enter_existing"))
		{
			// The slot button selects the widget's local row; Enter performs subsystem selection.
			if (Click(Visible<URogue10mCharacterLobbyWidget>(), TEXT("UI_EnterGameButton"))) { bMenu = true; SetStage(TEXT("hub_ready"), 70.); }
		}
		else if (Stage == TEXT("hub_ready"))
		{
			if (Map() == TEXT("StartHub") && Attached() && Town.IsValid() && Inventory.IsValid())
			{
				if (!Check(State->GetPlayerLevel() == 1 && Discovered(), TEXT("settled_clone_starts_LV1_and_retains_source_Discovery"))) { Block(TEXT("clone_not_clean_settled_Hub_state")); return false; }
				for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
				if (!NPC.IsValid()) { Block(TEXT("actual_map_NPC_missing_no_spawn_fallback")); return false; } SetStage(TEXT("npc_walk"), 40.);
			}
		}
		else if (Stage == TEXT("npc_walk"))
		{
			if (NPC.IsValid() && Walk(NPC->GetActorLocation() + FVector(-100., -50., 0.), 20.f, Dt)) { SetStage(TEXT("npc_open")); }
		}
		else if (Stage == TEXT("npc_open"))
		{
			if (NPC.IsValid() && NPC->IsDialogueOpen()) { ServiceDowns = 0; SetStage(TEXT("npc_service")); }
			else if (Now() > ActionAt) { Pulse(EKeys::F); ActionAt = Now() + 1.; }
		}
		else if (Stage == TEXT("npc_service"))
		{
			if (Now() > ActionAt)
			{
				if (ServiceDowns < 3) { Pulse(EKeys::Down); ++ServiceDowns; ActionAt = Now() + .3; }
				else { SetStage(TEXT("trait_select")); Pulse(EKeys::Enter); } // Keep the down event held across a normal input frame.
			}
		}
		else if (Stage == TEXT("trait_select"))
		{
			auto* W = PC->GetTraitWidget();
			if (PC->IsTraitWindowVisible() && Displayed(W) && NPC.IsValid() && PC->GetTraitServiceNPC() == NPC.Get())
			{
				for (const auto& Card : W->GetVisibleTraitCards())
				{
					if (Card.Trait == ERogue10mTownTrait::Power && Displayed(Card.Button))
					{
						Event(TEXT("visible_UMG_delegate"), TEXT("Power_card")); Card.Button->OnClicked.Broadcast(); bNpc = true; SetStage(TEXT("trait_preview")); break;
					}
				}
			}
		}
		else if (Stage == TEXT("trait_preview"))
		{
			if (Now() > ActionAt && !bPreviewShot) { Shot(TEXT("01_before_actual_purchase.png")); bPreviewShot = true; ActionAt = Now() + .85; }
			else if (bPreviewShot && Now() > ActionAt) { SetStage(TEXT("trait_buy")); }
		}
		else if (Stage == TEXT("trait_buy"))
		{
			auto* W = PC->GetTraitWidget();
			if (Displayed(W) && W->GetSelectedTrait() == ERogue10mTownTrait::Power && Now() > ActionAt && Attached())
			{
				const auto Def = Town->GetUpgradeDefinition(ERogue10mTownTrait::Power, NPC->GetTraitServiceData());
				BeforeRank = Town->GetTraitLevel(ERogue10mTownTrait::Power); BeforeBudget = Town->GetAvailableTraitPoints(); BeforeSpent = Town->GetSpentTraitPoints();
				Cost = Def.PointCost; BeforeBonus = Town->GetTraitBonus(ERogue10mTownTrait::Power); BonusDelta = Def.ValuePerRank;
				BeforeGold = Inventory->GetGold(); BeforeCrystals = Inventory->GetCrystals();
				const auto* A = Player->GetRogueAttributeSet(); BeforeAttackPower = A ? A->GetAttackPower() : -1.f;
				if (BeforeRank >= Def.MaxRank || Cost <= 0 || BeforeBudget < Cost) { Block(TEXT("actual_settled_clone_cannot_buy_Power_max_or_budget_no_seed_fallback")); return false; }
				if (Click(W, TEXT("TraitUpgradeButton"))) { SetStage(TEXT("purchase_saved")); }
			}
		}
		else if (Stage == TEXT("purchase_saved"))
		{
			if (Town.IsValid() && Town->GetTraitLevel(ERogue10mTownTrait::Power) == BeforeRank + 1)
			{
				bUpgrade = Check(Town->GetAvailableTraitPoints() == BeforeBudget - Cost && Town->GetSpentTraitPoints() == BeforeSpent + Cost
					&& Inventory->GetGold() == BeforeGold && Inventory->GetCrystals() == BeforeCrystals
					&& FMath::IsNearlyEqual(Town->GetTraitBonus(ERogue10mTownTrait::Power), BeforeBonus + BonusDelta), TEXT("actual_UI_purchase_exact_rank_cost_bonus_run_currency_unchanged"));
				if (!bUpgrade) { Block(TEXT("actual_purchase_ledger_mismatch_no_rebuy")); return false; }
				if (Attached())
				{
					const auto* Disk = LoadDisk(); bSaved = Check(Disk && Disk->ProfileId == Profile && Rank(Disk->Town) == BeforeRank + 1
						&& Disk->LobbyEarnedPoints - Spent(Disk->Town) == BeforeBudget - Cost
						&& Disk->Inventory.Gold == BeforeGold && Disk->Inventory.Crystals == BeforeCrystals, TEXT("read_only_disk_snapshot_confirms_UI_saved_purchase"));
					if (!bSaved) { Block(TEXT("save_ready_but_disk_purchase_mismatch")); return false; }
					AfterBonus = Town->GetTraitBonus(ERogue10mTownTrait::Power);
					const auto* A = Player->GetRogueAttributeSet(); AfterAttackPower = A ? A->GetAttackPower() : -1.f;
					if (!Check(BeforeAttackPower > 0.f && FMath::IsNearlyEqual(AfterAttackPower, BeforeAttackPower + BonusDelta, .05f), TEXT("actual_Power_upgrade_applies_attack_stat_delta"))) { Block(TEXT("purchased_rank_saved_but_attack_stat_delta_mismatch")); return false; }
					Shot(TEXT("02_after_saved_purchase.png")); SetStage(TEXT("trait_close"));
				}
				else if (Save.IsValid() && !Save->GetLastError().IsEmpty() && Now() > ActionAt)
				{
					if (++SaveRetries > 2 || !Click(PC->GetTraitWidget(), TEXT("TraitSaveRetryButton"))) { Block(TEXT("actual_save_failure_retry_unavailable_no_rebuy")); return false; } ActionAt = Now() + 1.;
				}
			}
		}
		else if (Stage == TEXT("trait_close"))
		{ if (!PC->IsTraitWindowVisible()) { SetStage(TEXT("npc_close")); } else { Click(PC->GetTraitWidget(), TEXT("TraitCloseButton")); } }
		else if (Stage == TEXT("npc_close"))
		{
			if (!NPC.IsValid() || !NPC->IsDialogueOpen()) { EntryGold = Inventory->GetGold(); EntryCrystals = Inventory->GetCrystals(); SetStage(TEXT("portal_walk"), 70.); }
			else if (Now() > ActionAt) { Pulse(EKeys::Escape); ActionAt = Now() + .7; }
		}
		else if (Stage == TEXT("portal_walk"))
		{
			if (Map() == TEXT("OpenRunMap")) { SetStage(TEXT("next_run"), 70.); }
			else
			{
				// The direct NPC-to-portal line crosses the central well. Use the proven H/I walking route.
				const FVector Via[] = { FVector(650., -1100., 0.), FVector(650., 950., 0.), FVector(0., 1800., 0.), FVector(0., 4270., 0.) };
				if (PortalWaypoint < UE_ARRAY_COUNT(Via) && Walk(Via[PortalWaypoint], PortalWaypoint == 3 ? 20.f : 140.f, Dt)) { ++PortalWaypoint; }
			}
		}
		else if (Stage == TEXT("next_run"))
		{
			const auto* Run = World->GetGameState<ARogue10mGameState>();
			if (Map() == TEXT("OpenRunMap") && Attached() && Run && Run->IsRunActive())
			{
				const auto* A = Player->GetRogueAttributeSet();
				bNext = Check(State->GetPlayerLevel() == 1 && Town->GetTraitLevel(ERogue10mTownTrait::Power) == BeforeRank + 1
					&& FMath::IsNearlyEqual(Town->GetTraitBonus(ERogue10mTownTrait::Power), AfterBonus)
					&& A && AfterAttackPower > 0.f && FMath::IsNearlyEqual(A->GetAttackPower(), AfterAttackPower, .05f)
					&& Town->GetAvailableTraitPoints() == BeforeBudget - Cost && Discovered(), TEXT("next_run_LV1_Power_rank_bonus_attack_stat_budget_Discovery_preserved"));
				if (!bNext) { Block(TEXT("next_run_upgrade_or_reset_mismatch")); return false; }
				Shot(TEXT("03_next_run.png")); SetStage(TEXT("settings"));
			}
		}
		else if (Stage == TEXT("settings"))
		{
			if (auto* W = Visible<URogue10mAbandonMenuWidget>(); W && Click(W, TEXT("AbandonMenuButton"))) { SetStage(TEXT("abandon_confirm")); }
			else if (Now() > ActionAt) { Pulse(EKeys::O); ActionAt = Now() + 1.; }
		}
		else if (Stage == TEXT("abandon_confirm"))
		{ if (Click(Visible<URogue10mAbandonMenuWidget>(), TEXT("AbandonConfirmButton"))) { SetStage(TEXT("result"), 40.); } }
		else if (Stage == TEXT("result"))
		{
			const auto* Run = World->GetGameState<ARogue10mGameState>();
			if (PC->IsRunResultVisible() && Run)
			{
				bAbandon = Check(Run->GetRunPhase() == ERogue10mRunPhase::Abandoned, TEXT("actual_UI_confirmation_ended_as_Abandoned"));
				if (!bAbandon) { Block(TEXT("unexpected_terminal_kind_not_abandon")); return false; }
				if (Now() > ActionAt && Click(PC->GetRunResultWidget(), TEXT("RunResultReturnToTown"))) { SetStage(TEXT("returned_hub"), 70.); }
			}
		}
		else if (Stage == TEXT("returned_hub"))
		{
			if (Map() == TEXT("StartHub") && Attached())
			{
				bReturned = Check(State->GetPlayerLevel() == 1 && Town->GetTraitLevel(ERogue10mTownTrait::Power) == BeforeRank + 1
					&& Town->GetAvailableTraitPoints() == BeforeBudget - Cost && Inventory->GetGold() == EntryGold && Inventory->GetCrystals() == EntryCrystals,
					TEXT("abandon_UI_return_keeps_lobby_upgrade_and_entry_currency_baseline"));
				bDiscovery = Check(Discovered(), TEXT("source_Discovery_preserved_after_roundtrip")); Shot(TEXT("04_returned_hub.png"));
				SetStage(TEXT("capture_wait")); ActionAt = Now() + 1.;
			}
		}
		else if (Stage == TEXT("capture_wait") && Now() > ActionAt) { Finish(true); return false; }
		return !bFinished;
	}
	void Block(const FString& Why, bool Exit = true) { if (!bFinished) { ++Failures; Reason = Why; Event(TEXT("blocked"), Why); Finish(false, Exit); } }
	void Finish(bool Complete, bool Exit = true)
	{
		if (bFinished) { return; } bFinished = true; ReleaseAll(); FCoreDelegates::OnPreExit.Remove(ExitHandle);
		for (const auto& File : Photos) { Check(IFileManager::Get().FileSize(*File) > 0, TEXT("screenshot_file_exists_decode_visual_review_separate")); }
		const bool Milestones = bMenu && bNpc && bUpgrade && bSaved && bNext && bAbandon && bReturned && bDiscovery;
		if (Complete && !Milestones) { ++Failures; Reason = TEXT("completion_milestones_missing"); }
		const FString Summary = FString::Printf(TEXT("outcome=%s\nfailures=%d\nchecks=%d\nreason=%s\nscope=normal_UI_upgrade_roundtrip_programmatic\nphysical_input=not_performed\nprofile=%s\nsource_label=%s\nnormal_menu_ui=%d\nactual_NPC_service_ui=%d\nnormal_power_upgrade=%d\npurchase_saved=%d\nnext_run_reset_and_trait=%d\nabandon_UI=%d\nreturned_hub=%d\ndiscovery_preserved=%d\nno_state_injection=1\n"), Complete && Milestones && !Failures ? TEXT("completed") : TEXT("blocked"), Failures, Checks, *Reason, *Profile.ToString(EGuidFormats::Digits), *SourceLabel, bMenu, bNpc, bUpgrade, bSaved, bNext, bAbandon, bReturned, bDiscovery);
		const bool Written = FFileHelper::SaveStringToFile(Events, *(Output / TEXT("events.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			&& FFileHelper::SaveStringToFile(Samples, *(Output / TEXT("samples.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			&& FFileHelper::SaveStringToFile(Summary, *(Output / TEXT("summary.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (!Written) { ++Failures; }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=LOBBY_UPGRADE_%s failures=%d scope=normal_UI_upgrade_roundtrip_programmatic output=%s"), Complete && Milestones && !Failures ? TEXT("PASSED") : TEXT("FAILED"), Failures, *Output);
		if (Exit) { FPlatformMisc::RequestExitWithStatus(false, Complete && Milestones && !Failures ? 0 : 2); }
	}
	TWeakObjectPtr<UGameInstance> Instance;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mTownProgressionComponent> Town;
	TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	TWeakObjectPtr<URogue10mProgressionSubsystem> Save;
	FGuid Profile;
	TSet<FName> Discovery;
	TMap<FKey, bool> Held;
	TMap<FKey, double> Releases;
	TArray<FString> Photos;
	FString Label, SourceLabel, Output, TownSlot, Prefix, KeySlot, Reason, Stage = TEXT("main_menu");
	FString Events = TEXT("wall,stage,event,detail\n"), Samples = TEXT("wall,stage,map,level,Power_rank,lobby_available,run_gold,crystals,save_ready\n");
	FVector LastPosition = FVector::ZeroVector;
	double Started = 0, StageAt = 0, StageLimit = 30, ActionAt = 0, StuckAt = 0, LastSample = 0;
	int32 MaxSeconds = 300, Checks = 0, Failures = 0, ServiceDowns = 0, SaveRetries = 0, BeforeRank = 0, BeforeBudget = 0, BeforeSpent = 0, Cost = 0, BeforeGold = 0, BeforeCrystals = 0, EntryGold = 0, EntryCrystals = 0;
	float BeforeBonus = 0, BonusDelta = 0, AfterBonus = 0, BeforeAttackPower = 0, AfterAttackPower = 0;
	bool bMenu = false, bNpc = false, bUpgrade = false, bSaved = false, bNext = false, bAbandon = false, bReturned = false, bDiscovery = false, bPreviewShot = false, bFinished = false;
	FDelegateHandle ExitHandle;
};
static TSharedPtr<FRun> Active;
static void Run()
{
	if (!GEngine || GIsEditor || (Active && !Active->Finished())) { return; }
	for (const FWorldContext& C : GEngine->GetWorldContexts()) { if (UWorld* W = C.World(); W && W->WorldType == EWorldType::Game) { Active = MakeShared<FRun>(); Active->Start(*W); return; } }
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestLobbyUpgradeRoundTrip"), TEXT("Verified byte-identical closed H/I QA clone only. Normal UI purchase/next-run/abandon return; no state injection or physical-input claim."), FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
