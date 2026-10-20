#if WITH_EDITOR
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mCombatTestWidget.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "UnrealClient.h"

namespace Rogue10mCombatTestUIQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString Slot, Prefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), Slot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), Prefix);
		if (!Slot.StartsWith(TEXT("Rogue10m_Town_QA_CombatUI_")) || !Prefix.StartsWith(TEXT("Rogue10m_Progression_QA_")))
		{ Check(false, TEXT("isolated_save_flags_required")); Finish(); return; }
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/monster-test-ui-20261011"), Slot);
		IFileManager::Get().MakeDirectory(*Output, true);
		Later(6.0f, [this] { Prepare(); });
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if (Self->bFinished) return false;
			if (FPlatformTime::Seconds() >= Self->NextTime && Self->Next)
			{
				auto Callback = MoveTemp(Self->Next); Callback();
			}
			return !Self->bFinished;
		}), 0.03f);
	}
private:
	void Check(bool bOK, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
		UE_LOG(LogRogue10m, Display, TEXT("COMBAT_UI_CHECK %s %s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bOK) ++Failures;
	}
	void Later(double Delay, TFunction<void()> Callback) { NextTime = FPlatformTime::Seconds() + Delay; Next = MoveTemp(Callback); }
	void Key(FKey Key)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1));
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0));
	}
	void Click(const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Tools->GetWidget()->GetWidgetFromName(Name));
		Check(Button && Button->GetIsEnabled(), Name);
		if (Button && Button->GetIsEnabled()) Button->OnClicked.Broadcast();
	}
	void Prepare()
	{
		PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		Check(PC.IsValid() && Profiles && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_profile"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("CombatUI"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("test_profile_selected"));
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Player = Cast<ARogue10mCharacter>(PC->GetPawn()); Tools = PC->GetCombatTestComponent();
		Check(Player.IsValid() && Tools.IsValid() && Tools->GetWidget(), TEXT("actual_player_and_native_test_widget"));
		if (Failures) { Finish(); return; }
		for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
			if (!It->IsCombatTestInstance()) { Original = *It; break; }
		Check(Original.IsValid(), TEXT("original_map_monster_exists"));
		if (Original.IsValid()) Check(!Original->GetAttackPatternComponent()->TryStartCombatTestAttack(Player.Get(),ERogue10mMonsterAttackType::FrontalWave),
			TEXT("debug_attack_rejected_on_map_monster"));
		Player->GetRogueAttributeSet()->SetMaxHealth(5000); Player->GetRogueAttributeSet()->RestoreVitals();
		Key(EKeys::F6);
		Later(0.7, [this]
		{
			Check(Tools->IsPanelOpen() && UGameplayStatics::IsGamePaused(World.Get()) && PC->bShowMouseCursor
				&& PC->IsAnyBlockingWindowVisible(), TEXT("F6_opens_panel_pauses_and_enables_cursor"));
			SpawnSix();
		});
	}
	void SpawnSix()
	{
		auto* Picker = Cast<UComboBoxString>(Tools->GetWidget()->GetWidgetFromName(TEXT("CombatTestCreature")));
		Check(Picker && Picker->GetOptionCount() == 6, TEXT("six_real_species_in_picker"));
		if (Failures) { Finish(); return; }
		for (int32 Index=0; Index<6; ++Index)
		{
			Picker->SetSelectedIndex(Index); Click(TEXT("CombatTestSpawn"));
			auto* Monster = Tools->GetTestTarget();
			Check(Monster && Monster->IsCombatTestInstance() && Monster->GetMonsterData()
				&& Monster->GetAttackPatternComponent()->GetProfile(), TEXT("spawn_real_profile_without_rewards"));
			if (Monster)
			{
				Species.Add(Monster->GetMonsterData()->MonsterId);
				Check(FVector::Dist2D(Player->GetActorLocation(), Monster->GetActorLocation()) < 1001,
					TEXT("spawn_within_ten_metres"));
				Check(Monster->GetController() && !Monster->IsPatternAttackActive(), TEXT("spawn_ai_stopped"));
			}
		}
		Check(Species.Num()==6 && Tools->GetTestMonsterCount()==6, TEXT("six_distinct_species_spawned"));
		Check(!Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn) && Tools->GetTestMonsterCount()==6, TEXT("spawn_limit_enforced"));
		Boss = Tools->GetTestTarget();
		if (Failures || !Boss.IsValid()) { Finish(); return; }
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,TEXT("combat-test-ui.png")),true,false);
		Later(0.8,[this] { GeometryAndWave(); });
	}
	void GeometryAndWave()
	{
		auto* Widget = Tools->GetWidget();
		const FGeometry& Root = Widget->GetCachedGeometry();
		for (const TCHAR* Name : {TEXT("CombatTestSpawn"),TEXT("CombatTestWave"),TEXT("CombatTestZone"),TEXT("CombatTestClear"),TEXT("CombatTestClose")})
		{
			const UWidget* Child = Widget->GetWidgetFromName(Name);
			const FGeometry& G = Child->GetCachedGeometry();
			const FVector2D A = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D B = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
			Check(G.GetLocalSize().X>0 && G.GetLocalSize().Y>0 && A.X>=0 && A.Y>=0
				&& B.X <= Root.GetLocalSize().X+1 && B.Y<=Root.GetLocalSize().Y+1, TEXT("button_fully_inside_viewport"));
		}
		Check(IFileManager::Get().FileSize(*FPaths::Combine(Output,TEXT("combat-test-ui.png")))>24, TEXT("actual_UI_screenshot_written"));
		Player->GetRogueAttributeSet()->SetHealth(4000);
		auto* HealButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("CombatTestHeal")));
		FWidgetPath HealPath;
		const bool bPathFound = FSlateApplication::Get().GeneratePathToWidgetUnchecked(HealButton->TakeWidget(),HealPath);
		Check(bPathFound,TEXT("real_Slate_button_path_found"));
		if (bPathFound)
		{
			const FVector2D Point = HealButton->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f,0.5f));
			// Pointer paths must come from the hit-test grid. A generated keyboard/focus path
			// has no virtual pointer coordinates and cannot be routed as a mouse path in UE5.8.
			FWidgetPath PointerPath = FSlateApplication::Get().LocateWindowUnderMouse(Point,TArray<TSharedRef<SWindow>>{HealPath.GetWindow()},false,0);
			const bool bHit = PointerPath.IsValid() && PointerPath.ContainsWidget(&HealButton->TakeWidget().Get());
			Check(bHit,TEXT("mouse_hit_test_locates_heal_button"));
			if (bHit)
			{
				FSlateApplication::Get().RoutePointerMoveEvent(PointerPath,FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::Invalid,0,FModifierKeysState()),false);
				FSlateApplication::Get().RoutePointerDownEvent(PointerPath,FPointerEvent(0,Point,Point,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
				FSlateApplication::Get().RoutePointerUpEvent(PointerPath,FPointerEvent(0,Point,Point,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState()));
				Check(Player->GetRogueAttributeSet()->GetHealth()==5000,TEXT("Slate_mouse_click_executes_actual_heal_button"));
			}
		}
		Health = Player->GetRogueAttributeSet()->GetHealth();
		Click(TEXT("CombatTestWave"));
		Check(!Tools->IsPanelOpen() && !UGameplayStatics::IsGamePaused(World.Get()) && !PC->bShowMouseCursor
			&& !PC->IsAnyBlockingWindowVisible(), TEXT("attack_closes_panel_and_restores_gameplay_input"));
		Check(Boss->GetAttackPatternComponent()->GetCurrentAttackType()==ERogue10mMonsterAttackType::FrontalWave
			&& Boss->GetAttackPatternComponent()->IsTelegraphVisible(), TEXT("wave_button_starts_actual_warning"));
		Later(2.8, [this] { Zone(); });
	}
	void Zone()
	{
		Check(Player->GetRogueAttributeSet()->GetHealth()<Health && Boss->GetAttackPatternComponent()->GetSuccessfulHitCount()>0,
			TEXT("wave_resolves_actual_damage"));
		Key(EKeys::F6); Later(0.25,[this] { ZoneInputs(); });
	}
	void ZoneInputs()
	{
		Check(Tools->IsPanelOpen(),TEXT("F6_reopens_during_combat"));
		Click(TEXT("CombatTestHeal"));
		Check(Player->GetRogueAttributeSet()->GetHealth()==Player->GetRogueAttributeSet()->GetMaxHealth(),TEXT("heal_restores_player"));
		Click(TEXT("CombatTestImmunity"));
		const float ImmuneHealth = Player->GetRogueAttributeSet()->GetHealth();
		Check(!Player->CanBeDamaged() && UGameplayStatics::ApplyDamage(Player.Get(),100, nullptr, Boss.Get(), UDamageType::StaticClass())==0
			&& Player->GetRogueAttributeSet()->GetHealth()==ImmuneHealth, TEXT("immunity_blocks_actual_damage"));
		Click(TEXT("CombatTestImmunity")); Check(Player->CanBeDamaged(),TEXT("immunity_toggle_restores_damage_flag"));
		Click(TEXT("CombatTestEnrage"));
		Check(Boss->GetAttackPatternComponent()->IsEnraged() && Boss->GetRogueAttributeSet()->GetHealth()/Boss->GetRogueAttributeSet()->GetMaxHealth() < 0.41f,
			TEXT("HP40_enters_actual_enrage"));
		Click(TEXT("CombatTestZone"));
		Check(Boss->GetAttackPatternComponent()->GetCurrentAttackType()==ERogue10mMonsterAttackType::AbyssZone
			&& Boss->GetAttackPatternComponent()->IsTelegraphVisible(), TEXT("enraged_zone_button_starts_warning"));
		Later(2.4,[this] { ResetAndAI(); });
	}
	void ResetAndAI()
	{
		Check(Player->GetRogueAttributeSet()->GetHealth()<4999 && Boss->GetAttackPatternComponent()->GetSuccessfulHitCount()>1,
			TEXT("zone_resolves_actual_damage"));
		Key(EKeys::F6); Later(0.25,[this] { ResetInputs(); });
	}
	void ResetInputs()
	{
		Click(TEXT("CombatTestReset")); Boss = Tools->GetTestTarget();
		Check(Boss.IsValid() && !Boss->GetAttackPatternComponent()->IsEnraged()
			&& Boss->GetRogueAttributeSet()->GetHealth()==Boss->GetRogueAttributeSet()->GetMaxHealth(), TEXT("reset_fresh_full_health_boss"));
		Health = Player->GetRogueAttributeSet()->GetHealth();
		Click(TEXT("CombatTestBasic"));
		Check(Boss->GetAttackPatternComponent()->GetCurrentAttackType()==ERogue10mMonsterAttackType::Basic
			&& Boss->IsPatternAttackActive(),TEXT("basic_button_starts_actual_attack"));
		Later(1.5,[this]
		{
			Check(Player->GetRogueAttributeSet()->GetHealth()<Health,TEXT("basic_attack_resolves_damage"));
			Key(EKeys::F6); Later(0.25,[this]
			{
				AIStartedCount = Boss->GetAttackPatternComponent()->GetStartedAttackCount();
				Click(TEXT("CombatTestAI"));
				Check(!Tools->IsPanelOpen() && !UGameplayStatics::IsGamePaused(World.Get()), TEXT("AI_start_returns_to_game"));
				Later(4.0,[this] { RewardAndCleanup(); });
			});
		});
	}
	void RewardAndCleanup()
	{
		Check(Boss->GetAttackPatternComponent()->GetStartedAttackCount()>AIStartedCount,
			TEXT("actual_AI_acquires_player_and_engages"));
		Key(EKeys::F6); Later(0.25,[this] { CleanupInputs(); });
	}
	void CleanupInputs()
	{
		Click(TEXT("CombatTestAI"));
		Check(!Boss->IsPatternAttackActive(),TEXT("AI_stop_cancels_attack"));
		auto* State = PC->GetPlayerState<ARogue10mPlayerState>();
		const float Experience = State->GetRogueAttributeSet()->GetExperience();
		const int32 Gold = Player->GetInventoryComponent()->GetGold();
		const FName ID = Boss->GetMonsterData()->MonsterId;
		const int32 Defeats = State->GetMonsterDefeatCount(ID);
		UGameplayStatics::ApplyDamage(Boss.Get(),10000000,PC.Get(),Player.Get(),UDamageType::StaticClass());
		Check(Boss->IsDead() && State->GetRogueAttributeSet()->GetExperience()==Experience
			&& Player->GetInventoryComponent()->GetGold()==Gold && State->GetMonsterDefeatCount(ID)==Defeats,
			TEXT("test_kill_grants_no_EXP_gold_or_defeat_progress"));
		Click(TEXT("CombatTestClear"));
		Check(Tools->GetTestMonsterCount()==0 && !Tools->GetTestTarget() && Original.IsValid(), TEXT("cleanup_preserves_map_monster"));
		Key(EKeys::F6);
		Later(0.25,[this]
		{
			Check(!Tools->IsPanelOpen() && !UGameplayStatics::IsGamePaused(World.Get()) && !PC->bShowMouseCursor,
				TEXT("F6_closes_paused_panel"));
			Key(EKeys::F6); Later(0.25,[this]
			{
				Check(Tools->IsPanelOpen(),TEXT("panel_reopened_for_ESC"));
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
				Check(!Tools->IsPanelOpen() && !UGameplayStatics::IsGamePaused(World.Get()),TEXT("Slate_ESC_closes_panel"));
				Tools->SetPanelOpen(true); PC->ToggleInventory();
				Check(!Tools->IsPanelOpen() && !UGameplayStatics::IsGamePaused(World.Get()),TEXT("other_menu_releases_test_pause"));
				PC->CloseAllBlockingPanels();
				SafetyAndLifecycle();
			});
		});
	}
	void SafetyAndLifecycle()
	{
		const FVector Location = Player->GetActorLocation();
		Player->SetActorLocation(Location+FVector(0,0,5000),false,nullptr,ETeleportType::TeleportPhysics);
		Check(!Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn) && Tools->GetTestMonsterCount()==0,
			TEXT("no_spawn_without_safe_ground"));
		Player->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
		PC->SetPause(true); Tools->SetPanelOpen(true); Tools->SetPanelOpen(false);
		Check(UGameplayStatics::IsGamePaused(World.Get()),TEXT("preexisting_pause_is_preserved")); PC->SetPause(false);
		Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn);
		Tools->ExecuteAction(ERogue10mCombatTestAction::Invulnerable);
		Check(!Player->CanBeDamaged() && Tools->GetTestMonsterCount()==1,TEXT("lifecycle_fixture_active"));
		PC->UnPossess();
		Check(Player->CanBeDamaged() && Tools->GetTestMonsterCount()==0 && !Tools->GetTestTarget(),
			TEXT("pawn_change_restores_immunity_and_clears_test_actors"));
		PC->Possess(Player.Get());
		// Ordinary combat still pays its normal rewards; only the spawned test instances opt out.
		if (Original.IsValid())
		{
			auto* State = PC->GetPlayerState<ARogue10mPlayerState>();
			const float Experience = State->GetCurrentExperience();
			const float Level = State->GetRogueAttributeSet()->GetPlayerLevel();
			const int32 Gold = Player->GetInventoryComponent()->GetGold();
			const FName ID = Original->GetMonsterData()->MonsterId;
			const int32 Defeats = State->GetMonsterDefeatCount(ID);
			UGameplayStatics::ApplyDamage(Original.Get(),10000000,PC.Get(),Player.Get(),UDamageType::StaticClass());
			Check(Original->IsDead() && Player->GetInventoryComponent()->GetGold()>Gold
				&& State->GetMonsterDefeatCount(ID)==Defeats+1 && (State->GetCurrentExperience()>Experience
				|| State->GetRogueAttributeSet()->GetPlayerLevel()>Level), TEXT("ordinary_monster_rewards_unchanged"));
		}
		Finish();
	}
	void Finish()
	{
		bFinished = true;
		if (Tools.IsValid()) { Tools->SetPanelOpen(false); Tools->ExecuteAction(ERogue10mCombatTestAction::Clear); }
		if (!Output.IsEmpty()) FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")));
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=COMBAT_UI_%s failures=%d"),Failures?TEXT("FAILED"):TEXT("PASSED"),Failures);
		FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
	TWeakObjectPtr<ARogue10mBasicMonster> Boss,Original;
	TSet<FName> Species;
	TFunction<void()> Next;
	double NextTime = 0;
	float Health = 0;
	FString Output,Results;
	int32 Failures = 0;
	int32 AIStartedCount = 0;
	bool bFinished = false;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestCombatUI"), TEXT("Tests the real development panel and combat in an isolated save profile."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor || Active) return;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (UWorld* W=Context.World(); W && W->WorldType==EWorldType::Game)
			{ Active=MakeShared<FRun>(); Active->Start(*W); return; }
	}));
}
#endif
