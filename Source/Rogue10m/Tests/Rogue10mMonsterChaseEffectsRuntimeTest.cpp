#if WITH_EDITOR
#include "AI/Rogue10mMonsterAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatTestComponent.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mPlayerController.h"
#include "UnrealClient.h"

namespace Rogue10mMonsterChaseEffectsQA
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld* InWorld)
	{
		World = InWorld;
		FString Slot, Progression;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), Slot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), Progression);
		if (!Slot.StartsWith(TEXT("Rogue10m_Town_QA_MonsterFX_")) || !Progression.StartsWith(TEXT("Rogue10m_Progression_QA_")))
		{ FPlatformMisc::RequestExitWithStatus(false, 2); return; }
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/monster-effects-chase-20261011"), Slot);
		IFileManager::Get().MakeDirectory(*Output, true);
		Deadline = FPlatformTime::Seconds() + 7.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float) { return Self->Tick(); }), 0.05f);
	}
private:
	void Check(bool OK, const TCHAR* Name)
	{
		Results += FString::Printf(TEXT("%s species=%d %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), Species, Name);
		UE_LOG(LogRogue10m, Display, TEXT("MONSTER_FX_CHECK %s species=%d %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Species, Name);
		if (!OK) ++Failures;
	}
	bool Ground(FVector XY, float HalfHeight, FVector& Out)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterChaseQA), false, Player.Get());
		if (Monster.IsValid()) Params.AddIgnoredActor(Monster.Get());
		if (!World->LineTraceSingleByChannel(Hit, XY+FVector(0,0,1500), XY-FVector(0,0,2000), ECC_WorldStatic, Params)
			|| Hit.ImpactNormal.Z < 0.7f) return false;
		Out = Hit.ImpactPoint + FVector(0,0,HalfHeight+3); return true;
	}
	void Setup()
	{
		PC = Cast<ARogue10mPlayerController>(UGameplayStatics::GetPlayerController(World.Get(),0));
		auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		FRogue10mCharacterProfile Profile;
		Check(PC.IsValid() && Profiles && Profiles->CreateProfile(TEXT("MonsterFX"),FRogue10mCharacterAppearance(),Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("isolated_profile_created"));
		if (Failures) { Finish(); return; }
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Player = Cast<ARogue10mCharacter>(PC->GetPawn()); Tools = PC->GetCombatTestComponent();
		Check(Player.IsValid() && Tools.IsValid(),TEXT("actual_player_test_ui"));
		if (Failures) { Finish(); return; }
		Player->GetRogueAttributeSet()->SetMaxHealth(100000); Player->GetRogueAttributeSet()->RestoreVitals();
		Origin = Player->GetActorLocation();
		Camera = World->SpawnActor<ACameraActor>();
		for (TActorIterator<ARogue10mBasicMonster> It(World.Get()); It; ++It)
			if (auto* ExistingAI = Cast<ARogue10mMonsterAIController>(It->GetController())) ExistingAI->StopMonsterAI();
		Stage = 1; Deadline = FPlatformTime::Seconds()+4;
	}
	void Spawn()
	{
		Tools->ExecuteAction(ERogue10mCombatTestAction::Clear);
		Player->SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
		Player->GetCharacterMovement()->StopMovementImmediately();
		Tools->SelectCreature(Species);
		Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Spawn),TEXT("species_spawned"));
		Monster = Tools->GetTestTarget();
		if (!Monster.IsValid()) { Finish(); return; }
		AI = Cast<ARogue10mMonsterAIController>(Monster->GetController());
		FVector SpawnAt;
		SpawnAt=Monster->GetActorLocation();
		Check(FVector::Dist2D(Origin,SpawnAt)>400,TEXT("chase_starts_outside_melee_range_on_UI_validated_ground"));
		Monster->SetActorRotation((Player->GetActorLocation()-SpawnAt).Rotation());
		PC->SetControlRotation((SpawnAt-Player->GetActorLocation()).Rotation());
		ChaseStart = Monster->GetActorLocation();
		Check(AI.IsValid() && !AI->IsMonsterAIEnabled(),TEXT("test_spawn_AI_stopped"));
		Health = Player->GetRogueAttributeSet()->GetHealth();
		bCapturePending=false;
		Tools->ExecuteAction(ERogue10mCombatTestAction::ToggleAI);
		Stage=2; Deadline=FPlatformTime::Seconds()+2;
	}
	bool Tick()
	{
		if (bFinished) return false;
		if (Stage==3 && Monster.IsValid() && !bCapturePending
			&& Monster->GetAttackPatternComponent()->GetSuccessfulHitCount()>0
			&& Monster->GetAttackPatternComponent()->GetActiveEffectCount()>0)
		{
			bCapturePending=true; Deadline=FPlatformTime::Seconds()+.1;
		}
		if (FPlatformTime::Seconds()<Deadline) return true;
		if (Stage==0) Setup();
		else if (Stage==1) Spawn();
		else if (Stage==2)
		{
			Check(AI->GetCombatTarget()==Player.Get(),TEXT("natural_sight_detects_without_damage_stimulus"));
			UE_LOG(LogRogue10m,Display,TEXT("MONSTER_FX_AI species=%d enabled=%d LOS=%d range=%.0f distance=%.0f position=%s"),Species,
				AI->IsMonsterAIEnabled(),AI->LineOfSightTo(Player.Get()),Monster->GetDetectionRange(),FVector::Dist2D(Monster->GetActorLocation(),Player->GetActorLocation()),*Monster->GetActorLocation().ToString());
			Check(FVector::Dist2D(ChaseStart,Monster->GetActorLocation())>20,TEXT("navigation_chase_moves_monster"));
			FVector MovedAt;
			Check(Ground(Origin+FVector(-250,250,0),Player->GetSimpleCollisionHalfHeight(),MovedAt),TEXT("moving_player_ground"));
			Player->SetActorLocation(MovedAt,false,nullptr,ETeleportType::TeleportPhysics);
			PC->SetControlRotation((Monster->GetActorLocation()-MovedAt).Rotation());
			if (Camera.IsValid())
			{
				const FVector ViewAt=MovedAt+FVector(-450,-450,280);
				Camera->SetActorLocation(ViewAt);
				Camera->SetActorRotation(((MovedAt+Monster->GetActorLocation())*.5-ViewAt).Rotation());
				PC->SetViewTarget(Camera.Get());
			}
			Stage=3; Deadline=FPlatformTime::Seconds()+8;
		}
		else if (Stage==3)
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			Check(Pattern->GetStartedAttackCount()>0,TEXT("chases_moved_player_then_attacks"));
			Check(Pattern->GetSuccessfulHitCount()>0 && Player->GetRogueAttributeSet()->GetHealth()<Health,TEXT("actual_melee_damage"));
			Check(Pattern->GetAttackEffectCount()>0 && Pattern->GetImpactEffectCount()>0,TEXT("attack_and_real_hit_Niagara_spawned"));
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,FString::Printf(TEXT("species-%d.png"),Species)),true,false);
			Stage=4; Deadline=FPlatformTime::Seconds()+.3;
		}
		else if (Stage==4)
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			AI->StopMonsterAI();
			Check(!AI->IsMonsterAIEnabled() && !AI->GetCombatTarget() && Pattern->GetActiveEffectCount()==0,TEXT("stop_clears_target_attack_effects"));
			AI->NotifyDamageStimulus(Player.Get(),1);
			Check(!AI->GetCombatTarget(),TEXT("stopped_AI_ignores_stimulus"));
			AI->InitializeMonsterAI();
			Stage=5; Deadline=FPlatformTime::Seconds()+2;
		}
		else if (Stage==5)
		{
			Check(AI->GetCombatTarget()==Player.Get(),TEXT("resume_reacquires_already_visible_player"));
			Player->SetActorLocation(Origin+FVector(-5000,0,0),false,nullptr,ETeleportType::TeleportPhysics);
			AI->ExecuteBehaviorDecision();
			Check(!AI->GetCombatTarget(),TEXT("leash_drops_distant_player"));
			AI->NotifyDamageStimulus(Player.Get(),1);
			Check(!AI->GetCombatTarget(),TEXT("damage_does_not_reacquire_outside_leash"));
			AI->StopMonsterAI();
			Player->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics);
			AttackBefore=Monster->GetAttackPatternComponent()->GetAttackEffectCount();
			ImpactBefore=Monster->GetAttackPatternComponent()->GetImpactEffectCount();
			Tools->ExecuteAction(ERogue10mCombatTestAction::Invulnerable);
			Health=Player->GetRogueAttributeSet()->GetHealth();
			Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Basic),TEXT("manual_attack_starts_while_AI_stopped"));
			Stage=6; Deadline=FPlatformTime::Seconds()+2;
		}
		else if (Stage==6)
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			Check(Pattern->GetAttackEffectCount()>AttackBefore && Pattern->GetImpactEffectCount()==ImpactBefore
				&& Player->GetRogueAttributeSet()->GetHealth()>=Health,TEXT("invulnerability_has_attack_FX_without_fake_impact_FX"));
			Check(Pattern->GetActiveEffectCount()==0,TEXT("finite_burst_releases_without_cancel"));
			Tools->ExecuteAction(ERogue10mCombatTestAction::Invulnerable);
			if (Species<5) { ++Species; Stage=1; Deadline=FPlatformTime::Seconds()+1; }
			else
			{
				AttackBefore=Pattern->GetAttackEffectCount();
				Check(Tools->ExecuteAction(ERogue10mCombatTestAction::FrontalWave),TEXT("boss_wave_starts"));
				SetBossView(); Stage=7; Deadline=FPlatformTime::Seconds()+1.3;
			}
		}
		else if (Stage==7)
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			Check(Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::FrontalWave
				&& Pattern->GetAttackEffectCount()>AttackBefore && Pattern->GetActiveEffectCount()>0,TEXT("boss_frontal_wave_resolves_with_visible_FX"));
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,TEXT("azathor-wave.png")),true,false);
			Stage=8; Deadline=FPlatformTime::Seconds()+3;
		}
		else if (Stage==8)
		{
			Check(Monster->GetAttackPatternComponent()->GetActiveEffectCount()==0,TEXT("boss_wave_FX_finishes"));
			AttackBefore=Monster->GetAttackPatternComponent()->GetAttackEffectCount();
			Check(Tools->ExecuteAction(ERogue10mCombatTestAction::AbyssZone),TEXT("boss_zone_starts"));
			SetBossView(); Stage=9; Deadline=FPlatformTime::Seconds()+1.6;
		}
		else if (Stage==9)
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			Check(Pattern->GetCurrentAttackType()==ERogue10mMonsterAttackType::AbyssZone
				&& Pattern->GetAttackEffectCount()>AttackBefore && Pattern->GetActiveEffectCount()>0,TEXT("boss_abyss_zone_resolves_with_visible_FX"));
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output,TEXT("azathor-zone.png")),true,false);
			Stage=10; Deadline=FPlatformTime::Seconds()+2.5;
		}
		else if (Stage==10)
		{
			Check(Monster->GetAttackPatternComponent()->GetActiveEffectCount()==0,TEXT("boss_zone_FX_finishes"));
			Finish();
		}
		return !bFinished;
	}
	void SetBossView()
	{
		if (!Camera.IsValid()) return;
		const FVector Mid=(Player->GetActorLocation()+Monster->GetActorLocation())*.5;
		const FVector ViewAt=Mid+FVector(-650,-500,400);
		Camera->SetActorLocation(ViewAt); Camera->SetActorRotation((Mid-ViewAt).Rotation()); PC->SetViewTarget(Camera.Get());
	}
	void Finish()
	{
		bFinished=true;
		if (Tools.IsValid()) Tools->ExecuteAction(ERogue10mCombatTestAction::Clear);
		FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=MONSTER_FX_%s failures=%d"),Failures ? TEXT("FAILED") : TEXT("PASSED"),Failures);
		FPlatformMisc::RequestExitWithStatus(false,Failures ? 1 : 0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Player;
	TWeakObjectPtr<URogue10mCombatTestComponent> Tools;
	TWeakObjectPtr<ARogue10mBasicMonster> Monster;
	TWeakObjectPtr<ARogue10mMonsterAIController> AI;
	TWeakObjectPtr<ACameraActor> Camera;
	FString Output,Results;
	FVector Origin,ChaseStart;
	double Deadline=0;
	float Health=0;
	int32 Stage=0,Species=0,Failures=0;
	int32 AttackBefore=0,ImpactBefore=0;
	bool bFinished=false;
	bool bCapturePending=false;
};
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestMonsterChaseEffects"),TEXT("Isolated standalone monster sight/chase/effects QA"),FConsoleCommandDelegate::CreateLambda([]
{
	if (!GEngine || GIsEditor) return;
	for (const auto& Context:GEngine->GetWorldContexts())
		if (Context.World() && Context.World()->WorldType==EWorldType::Game) { MakeShared<FRun>()->Start(Context.World()); break; }
}));
}
#endif
