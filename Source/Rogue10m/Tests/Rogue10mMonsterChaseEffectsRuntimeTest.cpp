#if WITH_EDITOR
#include "AI/Rogue10mMonsterAIController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
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
#include "Rogue10mHumanoidAnimInstance.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mMonsterAttackPatternProfile.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "UnrealClient.h"

namespace Rogue10mMonsterChaseEffectsQA
{
static const FName SporelingSkirtBone(TEXT("mushroom_skirt08"));
static const FName SporelingHeadBone(TEXT("mushroom_head"));

struct FOrdinaryGaitSpecies
{
	const TCHAR* Id;
	int32 CombatTestIndex;
	const TCHAR* DetailBone;
};

static constexpr FOrdinaryGaitSpecies OrdinaryGaitSpecies[] = {
	{TEXT("VerdantSlime"), 0, TEXT("Crown")},
	{TEXT("CrimsonSlime"), 2, TEXT("Crown")},
	{TEXT("IronShell"), 3, TEXT("HornMidTip03")}
};
static constexpr const TCHAR* DeathSpecies[] = {TEXT("VerdantSlime"), TEXT("Sporeling"), TEXT("CrimsonSlime"),
	TEXT("IronShell"), TEXT("ThornWarden"), TEXT("AbyssOverlordAzathor")};
static constexpr const TCHAR* DeathStances[] = {TEXT("idle"), TEXT("walk"), TEXT("windup")};

struct FSporelingWalkMeasure
{
	double StartSkirtStep = 0.0;
	double StopSkirtStep = 0.0;
	double HeadStep = 0.0;
	double PeakSpeed = 0.0;
	double Travel = 0.0;
	int32 PairedFrames = 0;
	int32 FirstDamageFrame = 0;
	int32 RecoveryFrame = 0;
	float Damage = 0.0f;
	bool bSawWalk = false;
};

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
		bThornWalk = FParse::Param(FCommandLine::Get(), TEXT("RogueThornWardenWalkQA"));
		bSporelingWalk = FParse::Param(FCommandLine::Get(), TEXT("RogueSporelingWalkQA"));
		bSporelingSavedProfile = FParse::Param(FCommandLine::Get(), TEXT("RogueSporelingSavedProfile"));
		bDeathMotion = FParse::Param(FCommandLine::Get(), TEXT("RogueMonsterDeathMotionQA"));
		bDeathRender = bDeathMotion && FParse::Param(FCommandLine::Get(), TEXT("RogueMonsterDeathRender"));
		FString OrdinaryId;
		if (FParse::Value(FCommandLine::Get(), TEXT("RogueOrdinaryGaitQA="), OrdinaryId))
		{
			for (const auto& Case : OrdinaryGaitSpecies)
			{
				if (OrdinaryId == Case.Id) { OrdinaryGait = &Case; break; }
			}
			if (!OrdinaryGait || bThornWalk || bSporelingWalk)
			{ Check(false, TEXT("ordinary_gait_requires_one_deployed_species")); Finish(); return; }
			Species = OrdinaryGait->CombatTestIndex;
		}
		bOrdinarySavedProfile = FParse::Param(FCommandLine::Get(), TEXT("RogueOrdinaryGaitSaved"));
		if (bOrdinarySavedProfile && !OrdinaryGait)
		{ Check(false, TEXT("ordinary_gait_saved_profile_requires_species")); Finish(); return; }
		if ((bThornWalk && bSporelingWalk) || (bSporelingSavedProfile && !bSporelingWalk)
			|| (bDeathMotion && (bThornWalk || bSporelingWalk || OrdinaryGait)))
		{ Check(false, TEXT("incompatible_monster_walk_qa_modes")); Finish(); return; }
		if (bThornWalk) { Species = 4; }
		if (bSporelingWalk) { Species = 1; }
		if (bDeathRender) { Species = 4; DeathStance = 1; }
		Deadline = FPlatformTime::Seconds() + (bThornWalk || bSporelingWalk || OrdinaryGait || bDeathMotion ? 1.0 : 7.0);
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
		Stage = 1; Deadline = FPlatformTime::Seconds()+(bThornWalk || bSporelingWalk || OrdinaryGait || bDeathMotion ? .5 : 4);
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
		if (bDeathMotion) { PrepareDeathMotion(); return; }
		if (OrdinaryGait) { PrepareOrdinaryGait(); return; }
		if (bThornWalk || bSporelingWalk)
		{
			auto* Pattern = Monster->GetAttackPatternComponent();
			const auto* Live = LoadObject<URogue10mMonsterDataAsset>(nullptr, bSporelingWalk
				? TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_Sporeling.DA_OW_Sporeling")
				: TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_ThornWarden.DA_OW_ThornWarden"));
			ThornProfile = Pattern ? Pattern->GetProfile() : nullptr;
			const bool bUsesLiveProfile = Live && Monster->GetMonsterData() == Live && ThornProfile.IsValid()
				&& ThornProfile.Get() == Live->AttackPatternProfile;
			if (bSporelingWalk)
			{
				Check(bUsesLiveProfile && !ThornProfile->bUseHumanoidAnimInstance
					&& ThornProfile->bBlendLegacyLocomotion == bSporelingSavedProfile
					&& ThornProfile->GetPathName() == TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_BasicAttack_Sporeling.DA_OW_BasicAttack_Sporeling")
					&& Monster->GetMesh()->GetSkeletalMeshAsset() == Live->SkeletalMesh.Get(),
					TEXT("real_Sporeling_profile_and_mesh_match_requested_saved_state"));
				Check(FMath::IsNearlyEqual(World->GetDeltaSeconds(), 1.0f / 60.0f, 0.0001f),
					TEXT("Sporeling_pose_samples_use_fixed_60hz"));
				const auto* Mesh = Monster->GetMesh();
				Check(Mesh->GetBoneIndex(SporelingSkirtBone) != INDEX_NONE && Mesh->GetBoneIndex(SporelingHeadBone) != INDEX_NONE,
					TEXT("live_Sporeling_skirt_and_head_bones_exist"));
				if (bUsesLiveProfile)
				{
					Results += FString::Printf(TEXT("profile=%s\nmesh=%s\nidle=%s\nwalk=%s\nattack=%s\nhit=%s\ndeath=%s\n"),
						*ThornProfile->GetPathName(), *GetPathNameSafe(Mesh->GetSkeletalMeshAsset()),
						*ThornProfile->IdleAnimation.ToString(), *ThornProfile->MovementAnimation.ToString(),
						*ThornProfile->AttackAnimation.ToString(), *ThornProfile->HitAnimation.ToString(),
						*ThornProfile->DeathAnimation.ToString());
				}
			}
			else
			{
				Check(bUsesLiveProfile && ThornProfile->bBlendLegacyLocomotion, TEXT("real_ThornWarden_profile_uses_blended_ground_graph"));
			}
			if (Failures) { Finish(); return; }
			ThornOrigin = Monster->GetActorTransform();
			ThornDirection = (Monster->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			if (bSporelingWalk)
			{
				SporelingFrames = TEXT("case,frame,world_seconds,revision,paired,input_for_next_frame,speed_cm_s,travel_cm,native_graph,skirt_x,skirt_y,skirt_z,skirt_step_cm,head_x,head_y,head_z,head_step_cm\n");
				BeginSporelingWalk(false);
				Deadline = FPlatformTime::Seconds() + 35.0;
			}
			else
			{
				ThornFrames = TEXT("case,frame,elapsed_s,speed_cm_s,right_hand_step_cm,native_graph\n");
				BeginGroundWalk(false);
			}
			Stage = 11;
			ThornHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* InWorld, ELevelTick, float)
			{
				if (InWorld != Self->World.Get()) { return; }
				if (Self->bSporelingWalk) { Self->SampleSporelingWalk(); }
				else { Self->SampleThornWalk(); }
			});
			return;
		}
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
	void PrepareDeathMotion()
	{
		const FString Name = FString::Printf(TEXT("DA_OW_%s"), DeathSpecies[Species]);
		const auto* Data = Monster->GetMonsterData();
		const auto* Pattern = Monster->GetAttackPatternComponent();
		const auto* Profile = Pattern ? Pattern->GetProfile() : nullptr;
		Check(Data && Data->GetName() == Name && Profile && Profile == Data->AttackPatternProfile
			&& Monster->GetMesh()->GetSkeletalMeshAsset() == Data->SkeletalMesh.Get(), TEXT("death_deployed_profile_mesh_and_class"));
		Check(FMath::IsNearlyEqual(World->GetDeltaSeconds(), 1.f/60.f, .0001f), TEXT("death_fixed_60hz"));
		if (Failures) { Finish(); return; }
		DeathClip = Profile->DeathAnimation.LoadSynchronous();
		Check(DeathClip.IsValid(), TEXT("death_original_sequence_available"));
		if (Failures) { Finish(); return; }
		DeathFrame = 0;
		DeathLife = 0.f;
		DeathOutgoingPose.Reset();
		bSawFinalDeathClip = false;
		ThornDirection = (Monster->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		Monster->SetActorRotation(ThornDirection.Rotation());
		Monster->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Monster->GetMesh()->bEnableUpdateRateOptimizations = false;
		Monster->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (bDeathRender)
		{
			Check(Camera.IsValid(), TEXT("death_side_camera_available"));
			if (Failures) { Finish(); return; }
			Camera->GetCameraComponent()->SetFieldOfView(45.f);
			PC->SetViewTarget(Camera.Get());
			const auto& Bounds = Monster->GetMesh()->Bounds;
			DeathViewHeight = Bounds.Origin.Z - Monster->GetActorLocation().Z;
			const double Distance = FMath::Max(350.0, static_cast<double>(Bounds.SphereRadius) * 3.4);
			DeathViewOffset = Monster->GetActorRotation().RotateVector(FVector(0.5, -1.0, 0.28) * Distance);
		}
		if (!ThornHandle.IsValid())
		{
			DeathRows = TEXT("species,stance,relative_frame,world_frame,revision,death_asset,death_time_s,bone,x_cm,y_cm,z_cm,qx,qy,qz,qw,sx,sy,sz\n");
			ThornHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* TickWorld, ELevelTick, float)
			{
				if (TickWorld == Self->World.Get() && !Self->bFinished && Self->Stage == 12) { Self->SampleDeathMotion(); }
			});
		}
		Stage = 12;
		Deadline = FPlatformTime::Seconds() + (bDeathRender ? 125.0 : 55.0);
	}

	void RecordDeathPose(int32 RelativeFrame)
	{
		const auto* Mesh = Monster->GetMesh();
		auto* Instance = Mesh->GetAnimInstance();
		auto* Single = Mesh->GetSingleNodeInstance();
		const UAnimMontage* Montage = Instance ? Instance->GetCurrentActiveMontage() : nullptr;
		const UAnimationAsset* Active = Single ? Single->GetCurrentAsset() : Montage ? Montage->GetFirstAnimReference() : nullptr;
		const float Position = Single ? Single->GetCurrentTime() : Montage ? Instance->Montage_GetPosition(Montage) : -1.f;
		bSawFinalDeathClip |= RelativeFrame > 0 && Active == DeathClip.Get() && Position > 0.f;
		if (RelativeFrame < -4 || RelativeFrame > 12) { return; }
		double SynchronousStep = 0.0;
		double SynchronousTurn = 0.0;
		for (const FName Bone : {FName(TEXT("Body")), FName(TEXT("Crown")), FName(TEXT("Shell")), FName(TEXT("HornMidTip03")),
			FName(TEXT("mushroom_root")), FName(TEXT("mushroom_head")), FName(TEXT("mushroom_skirt08")), FName(TEXT("cactus_rightHandTip")),
			FName(TEXT("cactus_root")), FName(TEXT("cactus_head")), FName(TEXT("pelvis")), FName(TEXT("head")), FName(TEXT("hand_r")), FName(TEXT("foot_l"))})
		{
			if (Mesh->GetBoneIndex(Bone) == INDEX_NONE) { continue; }
			const FTransform Pose = Mesh->GetSocketTransform(Bone, RTS_Component);
			if (Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized()) { Check(false, TEXT("death_recorded_finite_pose")); return; }
			if (RelativeFrame == -1) { DeathOutgoingPose.Add(Bone, Pose); }
			if (RelativeFrame == 0)
			{
				const FTransform& Before = DeathOutgoingPose.FindChecked(Bone);
				SynchronousStep = FMath::Max(SynchronousStep, FVector::Dist(Before.GetLocation(), Pose.GetLocation()));
				SynchronousTurn = FMath::Max(SynchronousTurn, FMath::RadiansToDegrees(Before.GetRotation().AngularDistance(Pose.GetRotation())));
			}
			const FVector P = Pose.GetLocation(), S = Pose.GetScale3D();
			const FQuat Q = Pose.GetRotation();
			DeathRows += FString::Printf(TEXT("%s,%s,%d,%llu,%u,%s,%.6f,%s,%.6f,%.6f,%.6f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f\n"),
				DeathSpecies[Species], bDeathRender ? (bDeathStockCapture ? TEXT("walk_stock") : TEXT("walk_blended")) : DeathStances[DeathStance],
				RelativeFrame, GFrameCounter, Mesh->GetBoneTransformRevisionNumber(),
				*GetNameSafe(Active), Position, *Bone.ToString(), P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z);
		}
		if (RelativeFrame == 0)
		{
			Check(!DeathOutgoingPose.IsEmpty() && SynchronousStep <= .15 && SynchronousTurn <= .15,
				TEXT("lethal_damage_keeps_last_displayed_pose_during_class_switch"));
		}
	}

	void SampleDeathMotion()
	{
		constexpr int32 LethalFrame = 32;
		const int32 RelativeFrame = ++DeathFrame - LethalFrame;
		if (!Monster.IsValid())
		{
			Check(bSawFinalDeathClip && FMath::Abs(RelativeFrame / 60.f - DeathLife) <= .05f,
				TEXT("death_original_clip_observed_and_corpse_expires_on_schedule"));
			Results += FString::Printf(TEXT("death_case=%s/%s life_s=%.6f final_frame=%d\n"),
				DeathSpecies[Species], DeathStances[DeathStance], DeathLife, RelativeFrame);
			if (Failures) { Finish(); return; }
			if (bDeathRender)
			{
				if (bDeathStockCapture) { bDeathStockCapture = false; Spawn(); return; }
				Check(DeathCaptures.Num() == 6, TEXT("death_matched_three_capture_frames_per_pose"));
				for (const FString& Capture : DeathCaptures)
				{
					Check(IFileManager::Get().FileSize(*Capture) > 1024, TEXT("death_comparison_capture_written"));
				}
				Finish(); return;
			}
			if (++DeathStance == UE_ARRAY_COUNT(DeathStances)) { DeathStance = 0; ++Species; }
			if (Species == UE_ARRAY_COUNT(DeathSpecies)) { Finish(); return; }
			Spawn();
			return;
		}
		auto* Pattern = Monster->GetAttackPatternComponent();
		if (DeathStance == 1 && RelativeFrame < 0) { Monster->AddMovementInput(ThornDirection, 1.f, true); }
		if (DeathStance == 2 && RelativeFrame == -6)
		{
			Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Basic), TEXT("death_actual_basic_attack_started"));
		}
		if (RelativeFrame == 0)
		{
			const double Speed = Monster->GetVelocity().Size2D();
			Check(DeathStance == 2 ? Pattern->IsAttackActive() && Pattern->GetSuccessfulHitCount() == 0
				: DeathStance == 1 ? Speed > 20.0 && !Pattern->IsAttackActive() : Speed < 1.0 && !Pattern->IsAttackActive(),
				TEXT("death_exact_idle_walk_or_windup_precondition"));
			RecordDeathPose(-1);
			const auto* State = Player->GetPlayerState<ARogue10mPlayerState>();
			const float Experience = State->GetCurrentExperience();
			const int32 Gold = Player->GetInventoryComponent()->GetGold();
			const float LifeBeforeHit = Monster->GetRogueAttributeSet()->GetHealth();
			if (bDeathRender && bDeathStockCapture) { Pattern->UseStockDeathForAudit(); }
			const float Accepted = UGameplayStatics::ApplyDamage(Monster.Get(), LifeBeforeHit + 100.f, PC.Get(), Player.Get(), nullptr);
			Check(Accepted > 0.0f && Monster.IsValid() && Monster->IsDead() && !Pattern->IsAttackActive()
				&& !Monster->GetActorEnableCollision() && Monster->GetCharacterMovement()->MovementMode == MOVE_None,
				TEXT("actual_lethal_damage_stops_attack_movement_and_collision"));
			Check(State->GetCurrentExperience() == Experience && Player->GetInventoryComponent()->GetGold() == Gold,
				TEXT("isolated_field_clone_does_not_grant_rewards"));
			if (Failures) { Finish(); return; }
			DeathLife = Monster->GetLifeSpan();
			Check(FMath::IsNearlyEqual(DeathLife, DeathClip->GetPlayLength() + .05f, .05f), TEXT("death_lifespan_matches_original_clip"));
		}
		if (RelativeFrame > 0 && (Pattern->IsAttackActive() || Pattern->GetSuccessfulHitCount() != 0))
		{ Check(false, TEXT("dead_enemy_never_hits_during_corpse_presentation")); Finish(); return; }
		if (RelativeFrame >= -4 && RelativeFrame != -1) { RecordDeathPose(RelativeFrame); }
		if (bDeathRender) { UpdateDeathCapture(RelativeFrame); }
	}

	void UpdateDeathCapture(int32 RelativeFrame)
	{
		const FVector Focus = Monster->GetActorLocation() + FVector(0, 0, DeathViewHeight);
		Camera->SetActorLocationAndRotation(Focus + DeathViewOffset, (-DeathViewOffset).Rotation());
		const TCHAR* Phase = RelativeFrame == -1 ? TEXT("before") : RelativeFrame == 1 ? TEXT("first") : RelativeFrame == 10 ? TEXT("follow-through") : nullptr;
		if (Phase)
		{
			const FString Name = FString::Printf(TEXT("%s-%s.png"), bDeathStockCapture ? TEXT("stock") : TEXT("blended"), Phase);
			const FString Path = FPaths::ConvertRelativePathToFull(Output / Name);
			DeathCaptures.Add(Path);
			FScreenshotRequest::RequestScreenshot(Path, false, false);
		}
	}
	void BeginGroundWalk(bool bBlend)
	{
		const bool bUseSavedProfile = bBlend && (OrdinaryGait ? bOrdinarySavedProfile : (!bSporelingWalk || bSporelingSavedProfile));
		auto* Profile = bUseSavedProfile ? ThornProfile.Get()
			: DuplicateObject<URogue10mMonsterAttackPatternProfile>(ThornProfile.Get(), GetTransientPackage());
		if (!bUseSavedProfile) { Profile->bBlendLegacyLocomotion = bBlend; }
		Monster->GetAttackPatternComponent()->ConfigureProfile(Profile);
		Monster->SetActorTransform(ThornOrigin, false, nullptr, ETeleportType::TeleportPhysics);
		Monster->SetActorRotation(ThornDirection.Rotation());
		Monster->GetCharacterMovement()->StopMovementImmediately();
		Monster->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Monster->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		bThornBlend = bBlend;
		ThornStart = World->GetTimeSeconds();
		PeakThornStep = 0.0;
		PeakThornSpeed = 0.0;
		bThornAnimated = false;
	}
	void SampleThornWalk()
	{
		if (!Monster.IsValid() || !ThornProfile.IsValid()) { Check(false, TEXT("thorn_walk_fixture_lost_actor_or_profile")); Finish(); return; }
		auto* Pattern = Monster->GetAttackPatternComponent();
		auto* Mesh = Monster->GetMesh();
		if (bObserveThornRecovery)
		{
			if (Pattern->IsAttackActive()) { return; }
			if (!Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance()) && World->GetTimeSeconds() < ThornStart + 1.0) { return; }
			Check(Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance()) != nullptr, TEXT("legacy_attack_recovers_into_blended_ground_graph"));
			Finish();
			return;
		}

		const double Elapsed = World->GetTimeSeconds() - ThornStart;
		const double Speed = Monster->GetVelocity().Size2D();
		const bool bMoving = Elapsed >= .40 && Elapsed < 1.03;
		if (bMoving) { Monster->AddMovementInput(ThornDirection, 1.0f, true); }
		const FName Bone(TEXT("cactus_rightHandTip"));
		if (Mesh->GetBoneIndex(Bone) == INDEX_NONE) { Check(false, TEXT("live_right_hand_tip_exists")); Finish(); return; }
		const FVector Hand = Mesh->GetSocketTransform(Bone, RTS_Component).GetLocation();
		const double Step = FVector::Dist(Hand, PreviousThornHand);
		const bool bNative = Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance()) != nullptr;
		if (Elapsed >= .40 && Elapsed < 1.28)
		{
			PeakThornStep = FMath::Max(PeakThornStep, Step);
			PeakThornSpeed = FMath::Max(PeakThornSpeed, Speed);
			bThornAnimated |= Speed > 100.0 && bNative == bThornBlend;
			ThornFrames += FString::Printf(TEXT("%s,%llu,%.6f,%.3f,%.4f,%d\n"),
				bThornBlend ? TEXT("blended") : TEXT("legacy"), GFrameCounter, Elapsed, Speed, Step, bNative);
		}
		PreviousThornHand = Hand;
		if (Elapsed < 1.45) { return; }
		Results += FString::Printf(TEXT("thorn_%s_peak_pose_step_cm=%.4f speed_cm_s=%.3f\n"),
			bThornBlend ? TEXT("blended") : TEXT("legacy"), PeakThornStep, PeakThornSpeed);
		Check(bThornAnimated && PeakThornSpeed >= 0.95 * Monster->GetMonsterData()->WalkSpeed,
			TEXT("actual_ThornWarden_moves_at_profile_speed_in_expected_pose_graph"));
		if (!bThornBlend && !Failures)
		{
			LegacyThornStep = PeakThornStep;
			BeginGroundWalk(true);
			return;
		}
		Check(PeakThornStep > 0.5 && LegacyThornStep >= 20.0 && PeakThornStep < LegacyThornStep * 0.6,
			TEXT("blended_start_and_stop_reduce_visible_hand_tip_discontinuity"));
		if (Failures) { Finish(); return; }
		const auto* Clip = ThornProfile->AttackAnimation.LoadSynchronous();
		Check(Tools->ExecuteAction(ERogue10mCombatTestAction::Basic)
			&& Mesh->GetSingleNodeInstance() && Mesh->GetSingleNodeInstance()->GetCurrentAsset() == Clip
			&& FMath::IsNearlyEqual(Pattern->GetLockedAttackShape().WindupDuration, ThornProfile->AttackWindupSeconds),
			TEXT("blended_locomotion_still_uses_existing_legacy_attack_clip_and_windup"));
		if (Failures) { Finish(); return; }
		ThornStart = World->GetTimeSeconds();
		bObserveThornRecovery = true;
	}
	void BeginSporelingWalk(bool bBlend)
	{
		BeginGroundWalk(bBlend);
		Monster->GetMesh()->bEnableUpdateRateOptimizations = false;
		SporelingFrame = 0;
		SporelingAttackFrame = 0;
		SporelingMeasure = {};
	}
	void PrepareOrdinaryGait()
	{
		const FString LivePath = FString::Printf(TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_%s.DA_OW_%s"),
			OrdinaryGait->Id, OrdinaryGait->Id);
		const auto* Live = LoadObject<URogue10mMonsterDataAsset>(nullptr, *LivePath);
		auto* Mesh = Monster->GetMesh();
		auto* Pattern = Monster->GetAttackPatternComponent();
		ThornProfile = Pattern ? Pattern->GetProfile() : nullptr;
		const FString ExpectedProfile = FString::Printf(TEXT("/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_BasicAttack_%s.DA_OW_BasicAttack_%s"),
			OrdinaryGait->Id, OrdinaryGait->Id);
		Check(Live && Monster->GetMonsterData() == Live && ThornProfile.IsValid() && ThornProfile.Get() == Live->AttackPatternProfile
			&& ThornProfile->GetPathName() == ExpectedProfile && Mesh->GetSkeletalMeshAsset() == Live->SkeletalMesh.Get()
			&& !ThornProfile->bUseHumanoidAnimInstance && ThornProfile->bBlendLegacyLocomotion == bOrdinarySavedProfile,
			TEXT("actual_OpenRun_species_profile_and_mesh_loaded"));
		Check(FMath::IsNearlyEqual(World->GetDeltaSeconds(), 1.0f / 60.0f, 0.0001f), TEXT("ordinary_gait_uses_fixed_60hz"));
		if (Failures) { Finish(); return; }
		const auto* Idle = ThornProfile->IdleAnimation.LoadSynchronous();
		const auto* Walk = ThornProfile->MovementAnimation.LoadSynchronous();
		const auto* Hit = ThornProfile->HitAnimation.LoadSynchronous();
		const auto* Skeleton = Mesh->GetSkeletalMeshAsset()->GetSkeleton();
		Check(Idle && Walk && Hit && Idle->GetSkeleton() == Skeleton && Walk->GetSkeleton() == Skeleton && Hit->GetSkeleton() == Skeleton,
			TEXT("ground_and_hit_sequences_use_live_skeleton"));
		OrdinaryBones = {TEXT("Body"), TEXT("Head"), TEXT("Spine01"), FName(OrdinaryGait->DetailBone)};
		if (Species == 3) { OrdinaryBones.Add(TEXT("Shell")); }
		else { OrdinaryBones.Append({TEXT("Side_L"), TEXT("Side_R")}); }
		for (FName Bone : OrdinaryBones)
		{
			Check(Mesh->GetBoneIndex(Bone) != INDEX_NONE, *FString::Printf(TEXT("ordinary_gait_pose_bone_%s_exists"), *Bone.ToString()));
		}
		if (Failures) { Finish(); return; }
		Results += FString::Printf(TEXT("species=%s\nprofile=%s\nmesh=%s\nidle=%s\nwalk=%s\nattack=%s\nhit=%s\n"),
			OrdinaryGait->Id, *ExpectedProfile, *GetPathNameSafe(Mesh->GetSkeletalMeshAsset()), *GetPathNameSafe(Idle), *GetPathNameSafe(Walk),
			*ThornProfile->AttackAnimation.ToString(), *GetPathNameSafe(Hit));
		ThornOrigin = Monster->GetActorTransform();
		ThornDirection = (Monster->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		OrdinaryFrames = TEXT("case,frame,paired,input_for_next_frame,speed_cm_s,travel_cm,native_graph,gait_time_s,bone,x_cm,y_cm,z_cm,qx,qy,qz,qw,sx,sy,sz\n");
		BeginOrdinaryGait(false);
		Deadline = FPlatformTime::Seconds() + 45.0;
		Stage = 11;
		ThornHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* InWorld, ELevelTick, float)
		{
			if (InWorld == Self->World.Get() && !Self->bFinished) { Self->SampleOrdinaryGait(); }
		});
	}

	void BeginOrdinaryGait(bool bBlend)
	{
		BeginSporelingWalk(bBlend);
		OrdinaryHitFrame = 0;
		if (auto* SingleNode = Monster->GetMesh()->GetSingleNodeInstance()) { SingleNode->SetPosition(0.f, false); }
	}

	void SampleOrdinaryGait()
	{
		if (!Monster.IsValid() || !ThornProfile.IsValid() || !Player.IsValid())
		{ Check(false, TEXT("ordinary_gait_fixture_retains_actor_and_profile")); Finish(); return; }
		if (OrdinaryHitFrame > 0) { ObserveOrdinaryHit(); return; }
		if (SporelingAttackFrame > 0) { ObservePairedGaitAttack(); return; }
		constexpr int32 MoveStart = 26, MoveEnd = 128, LastFrame = 170;
		++SporelingFrame;
		auto* Mesh = Monster->GetMesh();
		const uint32 Revision = Mesh->GetBoneTransformRevisionNumber();
		const bool bPaired = SporelingFrame > 1 && Revision != PreviousSporelingRevision;
		const bool bMovementInput = SporelingFrame >= MoveStart && SporelingFrame < MoveEnd;
		const auto* Anim = Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance());
		auto* SingleNode = Mesh->GetSingleNodeInstance();
		const float GaitTime = Anim ? Anim->GetMovementPlaybackTime()
			: SingleNode && SingleNode->GetCurrentAsset() == ThornProfile->MovementAnimation.Get() ? SingleNode->GetCurrentTime() : -1.f;
		const double Speed = Monster->GetVelocity().Size2D();
		SporelingMeasure.Travel = FVector::Dist2D(ThornOrigin.GetLocation(), Monster->GetActorLocation());
		SporelingMeasure.PeakSpeed = FMath::Max(SporelingMeasure.PeakSpeed, Speed);
		SporelingMeasure.PairedFrames += bPaired ? 1 : 0;
		SporelingMeasure.bSawWalk |= Speed >= 0.95 * Monster->GetMonsterData()->WalkSpeed && HasPairedGaitGroundPose(true);
		const TCHAR* Label = PairedGaitCase();
		for (FName Bone : OrdinaryBones)
		{
			const FTransform Pose = Mesh->GetSocketTransform(Bone, RTS_Component);
			if (Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized())
			{ Check(false, TEXT("ordinary_gait_body_pose_is_finite")); Finish(); return; }
			const FVector P = Pose.GetLocation(), S = Pose.GetScale3D();
			const FQuat Q = Pose.GetRotation();
			OrdinaryFrames += FString::Printf(TEXT("%s,%d,%d,%d,%.4f,%.4f,%d,%.6f,%s,%.6f,%.6f,%.6f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f\n"),
				Label, SporelingFrame, bPaired, bMovementInput, Speed, SporelingMeasure.Travel, Anim != nullptr, GaitTime, *Bone.ToString(),
				P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z);
		}
		PreviousSporelingRevision = Revision;
		if (bMovementInput) { Monster->AddMovementInput(ThornDirection, 1.0f, true); }
		if (SporelingFrame != LastFrame) { return; }
		Check(SporelingMeasure.PairedFrames >= LastFrame - 2 && SporelingMeasure.bSawWalk && Speed < 1.0 && HasPairedGaitGroundPose(false),
			TEXT("ordinary_gait_pose_pairs_include_full_speed_and_settled_idle"));
		if (bThornBlend)
		{
			Check(FMath::IsNearlyEqual(SporelingMeasure.PeakSpeed, LegacySporelingMeasure.PeakSpeed, 0.1)
				&& FMath::IsNearlyEqual(SporelingMeasure.Travel, LegacySporelingMeasure.Travel, 1.0),
				TEXT("ordinary_gait_keeps_legacy_speed_and_travel"));
		}
		Results += FString::Printf(TEXT("%s speed=%.4f travel=%.4f pose_pairs=%d\n"), Label,
			SporelingMeasure.PeakSpeed, SporelingMeasure.Travel, SporelingMeasure.PairedFrames);
		if (Failures) { Finish(); return; }
		StartPairedGaitAttack();
	}

	void ObserveOrdinaryHit()
	{
		if (!HasPairedGaitGroundPose(false) && OrdinaryHitFrame < 250) { ++OrdinaryHitFrame; return; }
		Check(HasPairedGaitGroundPose(false), TEXT("ordinary_hit_recovers_into_expected_ground_graph"));
		Results += FString::Printf(TEXT("%s hit_recovery_frame=%d\n"), bThornBlend ? TEXT("blend") : TEXT("legacy"), OrdinaryHitFrame);
		if (!bThornBlend && !Failures)
		{
			LegacyOrdinaryHitFrame = OrdinaryHitFrame;
			LegacySporelingMeasure = SporelingMeasure;
			BeginOrdinaryGait(true);
			return;
		}
		Check(FMath::Abs(OrdinaryHitFrame - LegacyOrdinaryHitFrame) <= 1, TEXT("ordinary_hit_recovery_matches_legacy"));
		Finish();
	}
	const TCHAR* PairedGaitCase() const
	{
		if (!bThornBlend) { return TEXT("legacy"); }
		return (bSporelingSavedProfile || bOrdinarySavedProfile) ? TEXT("saved_blend") : TEXT("transient_blend");
	}
	bool HasPairedGaitGroundPose(bool bMoving) const
	{
		const auto* Mesh = Monster->GetMesh();
		const auto* Idle = ThornProfile->IdleAnimation.Get();
		const auto* Walk = ThornProfile->MovementAnimation.Get();
		if (!Idle || !Walk) { return false; }
		if (bThornBlend)
		{
			const auto* Anim = Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance());
			return Anim && Anim->GetIdleSequence() == Idle && Anim->GetMovementSequence() == Walk;
		}
		auto* SingleNode = Mesh->GetSingleNodeInstance();
		return SingleNode && SingleNode->GetCurrentAsset() == (bMoving ? Walk : Idle);
	}
	void SampleSporelingWalk()
	{
		if (!Monster.IsValid() || !ThornProfile.IsValid() || !Player.IsValid())
		{ Check(false, TEXT("Sporeling_walk_fixture_lost_actor_or_profile")); Finish(); return; }
		if (SporelingAttackFrame > 0) { ObservePairedGaitAttack(); return; }

		constexpr int32 MoveStart = 24;
		constexpr int32 MoveEnd = 62;
		constexpr int32 LastFrame = 90;
		++SporelingFrame;
		auto* Mesh = Monster->GetMesh();
		const uint32 Revision = Mesh->GetBoneTransformRevisionNumber();
		const bool bPaired = SporelingFrame > 1 && Revision != PreviousSporelingRevision;
		const bool bMovementInput = SporelingFrame >= MoveStart && SporelingFrame < MoveEnd;
		const FVector Skirt = Mesh->GetSocketTransform(SporelingSkirtBone, RTS_Component).GetLocation();
		const FVector Head = Mesh->GetSocketTransform(SporelingHeadBone, RTS_Component).GetLocation();
		if (Skirt.ContainsNaN() || Head.ContainsNaN()) { Check(false, TEXT("Sporeling_valid_bone_positions")); Finish(); return; }
		const double SkirtStep = bPaired ? FVector::Dist(Skirt, PreviousSporelingSkirt) : 0.0;
		const double HeadStep = bPaired ? FVector::Dist(Head, PreviousSporelingHead) : 0.0;
		const double Speed = Monster->GetVelocity().Size2D();
		SporelingMeasure.Travel = FVector::Dist2D(ThornOrigin.GetLocation(), Monster->GetActorLocation());
		SporelingMeasure.PeakSpeed = FMath::Max(SporelingMeasure.PeakSpeed, Speed);
		SporelingMeasure.PairedFrames += bPaired ? 1 : 0;
		SporelingMeasure.bSawWalk |= Speed >= 0.95 * Monster->GetMonsterData()->WalkSpeed && HasPairedGaitGroundPose(true);
		if (SporelingFrame > MoveStart && SporelingFrame <= MoveStart + 18)
		{ SporelingMeasure.StartSkirtStep = FMath::Max(SporelingMeasure.StartSkirtStep, SkirtStep); }
		if (SporelingFrame > MoveEnd && SporelingFrame <= MoveEnd + 18)
		{ SporelingMeasure.StopSkirtStep = FMath::Max(SporelingMeasure.StopSkirtStep, SkirtStep); }
		if (SporelingFrame > MoveStart)
		{ SporelingMeasure.HeadStep = FMath::Max(SporelingMeasure.HeadStep, HeadStep); }
		const bool bNative = Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance()) != nullptr;
		SporelingFrames += FString::Printf(TEXT("%s,%d,%.6f,%u,%d,%d,%.4f,%.4f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n"),
			PairedGaitCase(), SporelingFrame, World->GetTimeSeconds(), Revision, bPaired, bMovementInput, Speed,
			SporelingMeasure.Travel, bNative, Skirt.X, Skirt.Y, Skirt.Z, SkirtStep, Head.X, Head.Y, Head.Z, HeadStep);
		PreviousSporelingRevision = Revision;
		PreviousSporelingSkirt = Skirt;
		PreviousSporelingHead = Head;
		if (bMovementInput) { Monster->AddMovementInput(ThornDirection, 1.0f, true); }
		if (SporelingFrame != LastFrame) { return; }

		Results += FString::Printf(TEXT("sporeling_%s start_skirt_cm=%.4f stop_skirt_cm=%.4f head_cm=%.4f speed_cm_s=%.4f travel_cm=%.4f paired_frames=%d\n"),
			PairedGaitCase(), SporelingMeasure.StartSkirtStep, SporelingMeasure.StopSkirtStep, SporelingMeasure.HeadStep,
			SporelingMeasure.PeakSpeed, SporelingMeasure.Travel, SporelingMeasure.PairedFrames);
		Check(SporelingMeasure.PairedFrames >= LastFrame - 2 && SporelingMeasure.bSawWalk && Speed < 1.0 && HasPairedGaitGroundPose(false),
			TEXT("Sporeling_fresh_pose_pairs_cover_settled_walk_and_idle"));
		if (!bThornBlend)
		{
			Check(SporelingMeasure.StartSkirtStep >= 15.0, TEXT("legacy_gait_reproduces_large_component_space_skirt_step"));
		}
		else
		{
			Check(FMath::IsNearlyEqual(SporelingMeasure.PeakSpeed, LegacySporelingMeasure.PeakSpeed, 0.1)
				&& FMath::IsNearlyEqual(SporelingMeasure.Travel, LegacySporelingMeasure.Travel, 1.0),
				TEXT("Sporeling_matches_legacy_ground_speed_and_travel"));
			Check(SporelingMeasure.StartSkirtStep > 0.1 && SporelingMeasure.StartSkirtStep < LegacySporelingMeasure.StartSkirtStep * 0.6
				&& SporelingMeasure.StopSkirtStep <= FMath::Max(3.0, LegacySporelingMeasure.StopSkirtStep * 1.15)
				&& SporelingMeasure.HeadStep <= FMath::Max(3.0, LegacySporelingMeasure.HeadStep * 1.15),
				TEXT("Sporeling_ground_blend_reduces_start_pop_without_larger_stop_or_head_step"));
		}
		if (Failures) { Finish(); return; }
		StartPairedGaitAttack();
	}
	void StartPairedGaitAttack()
	{
		auto* AbilitySystem = Player->GetAbilitySystemComponent();
		if (!AbilitySystem)
		{ Check(false, TEXT("gait_attack_has_live_player_health_delegate")); Finish(); return; }
		SporelingHealthObserver = AbilitySystem->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute())
			.AddLambda([this](const FOnAttributeChangeData& Change)
			{
				SporelingMeasure.Damage += FMath::Max(0.0f, Change.OldValue - Change.NewValue);
			});
		const auto* Pattern = Monster->GetAttackPatternComponent();
		SporelingHitsBefore = Pattern->GetSuccessfulHitCount();
		const auto* Clip = ThornProfile->AttackAnimation.LoadSynchronous();
		const bool bStarted = Clip && Tools->ExecuteAction(ERogue10mCombatTestAction::Basic);
		auto* Playing = Monster->GetMesh()->GetSingleNodeInstance();
		Check(bStarted && Playing && Playing->GetCurrentAsset() == Clip
			&& FMath::IsNearlyEqual(Pattern->GetLockedAttackShape().WindupDuration, ThornProfile->AttackWindupSeconds),
			OrdinaryGait ? TEXT("ordinary_basic_uses_existing_single_node_clip_and_windup")
				: TEXT("Sporeling_basic_uses_existing_single_node_clip_and_windup"));
		if (Failures) { Finish(); return; }
		SporelingAttackFrame = 1;
	}
	void ObservePairedGaitAttack()
	{
		const auto* Pattern = Monster->GetAttackPatternComponent();
		const int32 Hits = Pattern->GetSuccessfulHitCount() - SporelingHitsBefore;
		if (Hits > 0 && SporelingMeasure.FirstDamageFrame == 0)
		{
			SporelingMeasure.FirstDamageFrame = SporelingAttackFrame;
		}
		if (Pattern->IsAttackActive() && SporelingAttackFrame < 420) { ++SporelingAttackFrame; return; }
		RemovePairedGaitHealthObserver();
		SporelingMeasure.RecoveryFrame = SporelingAttackFrame;
		Results += FString::Printf(TEXT("%s_%s attack_first_damage_frame=%d recovery_frame=%d damage=%.4f\n"),
			OrdinaryGait ? OrdinaryGait->Id : TEXT("sporeling"), PairedGaitCase(), SporelingMeasure.FirstDamageFrame,
			SporelingMeasure.RecoveryFrame, SporelingMeasure.Damage);
		const int32 WindupFrames = FMath::RoundToInt(ThornProfile->AttackWindupSeconds * 60.0f);
		Check(!Pattern->IsAttackActive() && Hits == 1 && SporelingMeasure.Damage > 0.0f
			&& FMath::Abs(SporelingMeasure.FirstDamageFrame - WindupFrames) <= 1 && HasPairedGaitGroundPose(false),
			OrdinaryGait ? TEXT("ordinary_same_attack_lands_once_then_recovers_into_ground_pose")
				: TEXT("Sporeling_same_attack_lands_once_then_recovers_into_ground_pose"));
		if (OrdinaryGait)
		{
			if (bThornBlend)
			{
				Check(FMath::Abs(SporelingMeasure.FirstDamageFrame - LegacySporelingMeasure.FirstDamageFrame) <= 1
					&& FMath::Abs(SporelingMeasure.RecoveryFrame - LegacySporelingMeasure.RecoveryFrame) <= 1
					&& FMath::IsNearlyEqual(SporelingMeasure.Damage, LegacySporelingMeasure.Damage, 0.01f),
					TEXT("ordinary_gait_keeps_basic_damage_timing_and_recovery"));
			}
			if (Failures) { Finish(); return; }
			const double Applied = UGameplayStatics::ApplyDamage(Monster.Get(), 1.f, PC.Get(), Player.Get(), nullptr);
			auto* HitPlayer = Monster->GetMesh()->GetSingleNodeInstance();
			Check(Applied > 0.0 && !Monster->IsDead() && HitPlayer && HitPlayer->GetCurrentAsset() == ThornProfile->HitAnimation.Get(),
				TEXT("ordinary_hit_plays_existing_hit_clip_after_nonlethal_damage"));
			if (Failures) { Finish(); return; }
			OrdinaryHitFrame = 1;
			return;
		}
		if (!bThornBlend && !Failures)
		{
			LegacySporelingMeasure = SporelingMeasure;
			BeginSporelingWalk(true);
			return;
		}
		Check(FMath::Abs(SporelingMeasure.FirstDamageFrame - LegacySporelingMeasure.FirstDamageFrame) <= 1
			&& FMath::Abs(SporelingMeasure.RecoveryFrame - LegacySporelingMeasure.RecoveryFrame) <= 1
			&& FMath::IsNearlyEqual(SporelingMeasure.Damage, LegacySporelingMeasure.Damage, 0.01f),
			TEXT("Sporeling_damage_and_attack_recovery_match_legacy"));
		Finish();
	}
	void RemovePairedGaitHealthObserver()
	{
		if (!SporelingHealthObserver.IsValid()) { return; }
		if (Player.IsValid())
		{
			if (auto* AbilitySystem = Player->GetAbilitySystemComponent())
			{
				AbilitySystem->GetGameplayAttributeValueChangeDelegate(URogue10mAttributeSet::GetHealthAttribute())
					.Remove(SporelingHealthObserver);
			}
		}
		SporelingHealthObserver.Reset();
	}
	bool Tick()
	{
		if (bFinished) return false;
		if (Stage == 12)
		{
			if (FPlatformTime::Seconds() < Deadline) { return true; }
			Check(false, TEXT("death_motion_watchdog")); Finish(); return false;
		}
		if (Stage == 11 && (bSporelingWalk || OrdinaryGait))
		{
			if (FPlatformTime::Seconds() < Deadline) { return true; }
			Check(false, TEXT("paired_gait_pose_fixture_watchdog"));
			Finish();
			return false;
		}
		if (Stage==3 && Monster.IsValid() && !bCapturePending
			&& Monster->GetAttackPatternComponent()->GetSuccessfulHitCount()>0
			&& Monster->GetAttackPatternComponent()->GetActiveEffectCount()>0)
		{
			bCapturePending=true; Deadline=FPlatformTime::Seconds()+.1;
		}
		// Observe a real burst as it appears, rather than sampling before a tuned windup ends.
		if ((Stage==7 || Stage==9) && Monster.IsValid())
		{
			auto* Pattern=Monster->GetAttackPatternComponent();
			if (Pattern->GetAttackEffectCount()>AttackBefore && Pattern->GetActiveEffectCount()>0)
				Deadline=FPlatformTime::Seconds();
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
				SetBossView(); Stage=7; ArmBossFXDeadline();
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
			SetBossView(); Stage=9; ArmBossFXDeadline();
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
	void ArmBossFXDeadline()
	{
		auto* Pattern=Monster->GetAttackPatternComponent();
		const float Windup=Pattern->GetLockedAttackShape().WindupDuration;
		Check(FMath::IsFinite(Windup) && Windup>0.f && Windup<=30.f,TEXT("boss_windup_has_bounded_timeout"));
		// Timers run on game time; allow limited wall-clock slack, but never wait forever.
		const double Wait=FMath::IsFinite(Windup) ? FMath::Clamp(static_cast<double>(Windup),0.0,30.0)+5.0 : 5.0;
		Deadline=FPlatformTime::Seconds()+Wait;
		UE_LOG(LogRogue10m,Display,TEXT("MONSTER_FX_WAIT stage=%d windup=%.3f timeout=%.3f attacks_before=%d"),Stage,Windup,Wait,AttackBefore);
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
		if (bFinished) { return; }
		bFinished=true;
		FWorldDelegates::OnWorldPostActorTick.Remove(ThornHandle);
		RemovePairedGaitHealthObserver();
		if (Tools.IsValid()) Tools->ExecuteAction(ERogue10mCombatTestAction::Clear);
		if (bThornWalk)
		{
			Check(FFileHelper::SaveStringToFile(ThornFrames,*FPaths::Combine(Output,TEXT("thorn-walk-frames.csv"))),
				TEXT("thorn_walk_pose_samples_written"));
		}
		if (bSporelingWalk)
		{
			Check(FFileHelper::SaveStringToFile(SporelingFrames,*FPaths::Combine(Output,TEXT("sporeling-walk-frames.csv"))),
				TEXT("Sporeling_walk_pose_samples_written"));
		}
		if (OrdinaryGait)
		{
			Check(FFileHelper::SaveStringToFile(OrdinaryFrames, *FPaths::Combine(Output, TEXT("ordinary-gait-frames.csv"))),
				TEXT("ordinary_gait_pose_samples_written"));
		}
		if (bDeathMotion)
		{
			Check(FFileHelper::SaveStringToFile(DeathRows, *FPaths::Combine(Output, TEXT("death-motion-frames.csv"))),
				TEXT("death_motion_pose_samples_written"));
		}
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
	TWeakObjectPtr<URogue10mMonsterAttackPatternProfile> ThornProfile;
	FTransform ThornOrigin;
	FVector ThornDirection = FVector::ZeroVector, PreviousThornHand = FVector::ZeroVector;
	FDelegateHandle ThornHandle;
	FString ThornFrames;
	double ThornStart = 0.0, PeakThornStep = 0.0, PeakThornSpeed = 0.0, LegacyThornStep = 0.0;
	bool bThornWalk = false, bThornBlend = false, bThornAnimated = false, bObserveThornRecovery = false;
	FSporelingWalkMeasure SporelingMeasure, LegacySporelingMeasure;
	FString SporelingFrames;
	FVector PreviousSporelingSkirt = FVector::ZeroVector, PreviousSporelingHead = FVector::ZeroVector;
	FDelegateHandle SporelingHealthObserver;
	uint32 PreviousSporelingRevision = 0;
	int32 SporelingFrame = 0, SporelingAttackFrame = 0, SporelingHitsBefore = 0;
	bool bSporelingWalk = false, bSporelingSavedProfile = false;
	const FOrdinaryGaitSpecies* OrdinaryGait = nullptr;
	TArray<FName> OrdinaryBones;
	FString OrdinaryFrames;
	int32 OrdinaryHitFrame = 0, LegacyOrdinaryHitFrame = 0;
	bool bOrdinarySavedProfile = false;
	bool bDeathMotion = false, bSawFinalDeathClip = false, bDeathRender = false, bDeathStockCapture = true;
	int32 DeathStance = 0, DeathFrame = 0;
	float DeathLife = 0.f;
	TWeakObjectPtr<UAnimSequence> DeathClip;
	TMap<FName,FTransform> DeathOutgoingPose;
	TArray<FString> DeathCaptures;
	FVector DeathViewOffset = FVector::ZeroVector;
	double DeathViewHeight = 0.0;
	FString DeathRows;
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
