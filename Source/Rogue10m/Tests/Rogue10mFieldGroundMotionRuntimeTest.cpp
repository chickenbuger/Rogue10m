// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Animation/AnimNode_StateMachine.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mPlayerController.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace Rogue10mFieldGroundMotion
{
struct FCase
{
	const TCHAR* Name;
	FVector Floor;
	double Yaw;
	bool bJump;
};

// Floor heights are the camera-plan survey of the existing Q approach and ramp.
const FCase Cases[] = {
	{TEXT("approach_jump"), FVector(-9750, 57550, 9600), 180, true},
	{TEXT("downhill_jump"), FVector(-9750, 61050, 8850), 90, true},
	{TEXT("uphill_jump"), FVector(-9750, 61050, 8850), -90, true},
	{TEXT("entrance_run"), FVector(-9750, 57550, 9600), 90, false},
};

const FName Bones[] = {
	TEXT("pelvis"), TEXT("head"),
	TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
	TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r"),
};

FString FullPath(const FString& Path)
{
	FString Result = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Result);
	FPaths::CollapseRelativeDirectories(Result);
	return Result;
}

class FRun : public TSharedFromThis<FRun>
{
public:
	FRun(UWorld& InWorld, FString InOutput, bool bInCapture, bool bInBaseline)
		: World(&InWorld), Output(MoveTemp(InOutput)), bCapture(bInCapture), bBaseline(bInBaseline) {}

	void Start()
	{
		if (bCapture)
		{
			Viewport = World->GetGameViewport();
			if (!Viewport.IsValid()) { Finish(TEXT("the capture requires a game viewport")); return; }
			bPreviousWorldRenderingDisabled = Viewport->bDisableWorldRendering;
			Viewport->bDisableWorldRendering = true;
		}
		Deadline = FPlatformTime::Seconds() + 90;
		TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda(
			[Self = AsShared()](UWorld* TickWorld, ELevelTick, float DeltaTime)
			{
				if (TickWorld == Self->World.Get()) { Self->Tick(DeltaTime); }
			});
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float)
		{
			if (!Self->bFinished && (!Self->World.IsValid() || FPlatformTime::Seconds() > Self->Deadline))
			{
				Self->Finish(FString::Printf(TEXT("timeout case=%s stage=%d"), Cases[Self->CaseIndex].Name, int32(Self->Stage)));
			}
			return !Self->bFinished;
		}), 0.25f);
	}

private:
	enum class EStage { Profile, Stream, Settle, Run };

	void Key(FKey Input, bool bDown) const
	{
		if (Controller.IsValid())
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Input, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	void Tick(float DeltaTime)
	{
		// A software renderer needs to draw only the requested poses. Simulation and animation still tick.
		if (Viewport.IsValid()) { Viewport->bDisableWorldRendering = !FScreenshotRequest::IsScreenshotRequested(); }
		if (Stage == EStage::Profile)
		{
			auto* PC = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
			if (!PC || !PC->GetPawn() || !World->GetGameInstance()) { return; }
			auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
			FRogue10mCharacterProfile Profile;
			if (!Profiles || Profiles->GetProfileCount() != 0)
			{
				Finish(TEXT("field motion QA needs a fresh profile slot"));
				return;
			}
			if (!Profiles->CreateProfile(TEXT("FieldQa"), FRogue10mCharacterAppearance(), Profile)
				|| !Profiles->SelectProfile(Profile.ProfileId))
			{
				Finish(FString::Printf(TEXT("fresh HumanMale profile could not be selected (%s)"), *Profiles->GetLastError().ToString()));
				return;
			}
			PC->EnterSelectedCharacter();
			PC->CloseAllBlockingPanels();
			Controller = PC;
			Pawn = Cast<ARogue10mCharacter>(PC->GetPawn());
			if (!Pawn.IsValid() || !Pawn->IsThirdPersonGameplayActive()
				|| Pawn->GetAnimationPlaybackMesh() == Pawn->GetMesh()
				|| Pawn->GetEquippedWeaponType() != ERogue10mWeaponType::Unarmed)
			{
				Finish(TEXT("possessed profile does not have the live unarmed third-person retarget"));
				return;
			}
			bPreviousDamage = Pawn->CanBeDamaged();
			Pawn->SetCanBeDamaged(false);
			Warp();
			return;
		}
		if (!Pawn.IsValid() || !Controller.IsValid() || Controller->GetPawn() != Pawn.Get())
		{
			Finish(TEXT("possessed pawn changed during a test case"));
			return;
		}
		auto& Movement = *Pawn->GetCharacterMovement();
		const FCase& Sample = Cases[CaseIndex];
		if (Stage == EStage::Stream)
		{
			auto* Streaming = World->GetSubsystem<UWorldPartitionSubsystem>();
			if (!Streaming || !Streaming->IsAllStreamingCompleted()) { return; }
			FHitResult Floor;
			if (!TraceFloor(Sample.Floor, Floor) || FMath::Abs(Floor.ImpactPoint.Z - Sample.Floor.Z) > 30.0) { return; }
			Pawn->SetActorLocation(Floor.ImpactPoint + FVector(0, 0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0),
				false, nullptr, ETeleportType::TeleportPhysics);
			Movement.SetMovementMode(MOVE_Falling);
			Stage = EStage::Settle;
			return;
		}
		if (Stage == EStage::Settle)
		{
			if (!Movement.IsMovingOnGround() || !Movement.CurrentFloor.IsWalkableFloor() || ++Frame < 20) { return; }
			Origin = Pawn->GetActorLocation();
			Frame = 0;
			Stage = EStage::Run;
			Key(EKeys::W, true);
			return;
		}
		if (!FMath::IsNearlyEqual(DeltaTime, 1.f / 60.f, 0.0005f))
		{
			Finish(TEXT("movement samples require -UseFixedTimeStep -FPS=60"));
			return;
		}
		++Frame;
		if (Sample.bJump && Frame == 35) { Key(EKeys::SpaceBar, true); }
		if (Sample.bJump && Frame == 40) { Key(EKeys::SpaceBar, false); }
		const bool bGrounded = Movement.IsMovingOnGround();
		bAscended |= Movement.IsFalling() && Movement.Velocity.Z > 0.f;
		bDescended |= Movement.IsFalling() && Movement.Velocity.Z < 0.f;
		if (bGrounded && bDescended && ContactFrame < 0) { ContactFrame = Frame; }
		if (!RecordPose(Movement)) { return; }
		if (Viewport.IsValid())
		{
			const int32 Phase = ContactFrame >= 0 ? Frame - ContactFrame : -1;
			if (Phase == 0 || Phase == 8 || Phase == 15 || Phase == 25 || (!Sample.bJump && Frame == 100))
			{
				Viewport->bDisableWorldRendering = false;
				const FString Name = FString::Printf(TEXT("%s-%03d.png"), Sample.Name, Frame);
				FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Output) / Name, false, false);
			}
		}
		const bool bDone = Sample.bJump ? ContactFrame >= 0 && Frame - ContactFrame >= 40 : Frame >= 135;
		if (!bDone && Frame < 160) { return; }
		const FVector Delta = Pawn->GetActorLocation() - Origin;
		if (!bDone || Delta.Size2D() < 350.0 || !bGrounded || (Sample.bJump && (!bAscended || !bDescended)))
		{
			Finish(FString::Printf(TEXT("input did not complete %s (travel=%.2f grounded=%d ascent=%d descent=%d contact=%d)"),
				Sample.Name, Delta.Size2D(), bGrounded, bAscended, bDescended, ContactFrame));
			return;
		}
		if ((CaseIndex > 0 && !bSawRamp) || (Sample.bJump && (!bSawForwardLanding || !bSawJogSeed)))
		{
			Finish(FString::Printf(TEXT("%s did not exercise its intended surface and landing states"), Sample.Name));
			return;
		}
		UE_LOG(LogRogue10m, Display, TEXT("Field motion case=%s frames=%d contact=%d travel=%.2f dz=%.2f peak_right_ankle_degrees=%.2f"),
			Sample.Name, Frame, ContactFrame, Delta.Size2D(), Delta.Z, PeakRightAnkleDegrees);
	// This window is after impact, while the trailing right leg swings into the next step.
		if (!bBaseline && Sample.bJump && PeakRightAnkleDegrees > 21.0)
		{
			Finish(FString::Printf(TEXT("%s right ankle exceeded the 21 degree swing limit"), Sample.Name));
			return;
		}
		Key(EKeys::W, false);
		if (CaseIndex + 1 == UE_ARRAY_COUNT(Cases)) { Finish(FString()); return; }
		++CaseIndex;
		Warp();
	}

	void Warp()
	{
		const FCase& Sample = Cases[CaseIndex];
		Deadline = FPlatformTime::Seconds() + 75;
		Stage = EStage::Stream;
		Frame = 0;
		ContactFrame = -1;
		bAscended = bDescended = false;
		bSawRamp = bSawForwardLanding = bSawJogSeed = false;
		PeakRightAnkleDegrees = 0.0;
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->GetCharacterMovement()->DisableMovement();
		const FRotator Look(-12.0, Sample.Yaw, 0);
		Controller->SetControlRotation(Look);
		Pawn->SetActorLocationAndRotation(Sample.Floor + FVector(0, 0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 9.0),
			FRotator(0, Sample.Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
	}

	bool TraceFloor(const FVector& Center, FHitResult& Floor) const
	{
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(FieldGroundMotionFloor), false, Pawn.Get());
		return World->LineTraceSingleByChannel(Floor, Center + FVector(0, 0, 85), Center - FVector(0, 0, 85), ECC_Pawn, Query)
			&& Floor.ImpactNormal.Z > 0.72;
	}

	bool RecordPose(const UCharacterMovementComponent& Movement)
	{
		const auto* Source = Pawn->GetAnimationPlaybackMesh();
		const auto* Body = Pawn->GetMesh();
		const auto* Anim = Source ? Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()) : nullptr;
		const int32 MainIndex = Anim ? Anim->GetStateMachineIndex(TEXT("Main States")) : INDEX_NONE;
		const auto* Main = MainIndex == INDEX_NONE ? nullptr : Anim->GetStateMachineInstance(MainIndex);
		if (!Main || !Body) { Finish(TEXT("missing live Manny or visible rig state")); return false; }
		const int32 Forward = Main->GetStateIndex(TEXT("Forward Run Land"));
		const int32 Stock = Main->GetStateIndex(TEXT("Land"));
		if (Forward == INDEX_NONE || Stock == INDEX_NONE) { Finish(TEXT("missing unarmed landing states")); return false; }
		const FVector PawnPosition = Pawn->GetActorLocation();
		const auto& Floor = Movement.CurrentFloor;
		const bool bWalkable = Floor.IsWalkableFloor();
		const double FloorZ = bWalkable ? Floor.HitResult.ImpactPoint.Z : 0;
		const double NormalZ = bWalkable ? Floor.HitResult.ImpactNormal.Z : 0;
		const float ForwardWeight = Main->GetStateWeight(Forward);
		bSawRamp |= bWalkable && NormalZ < FMath::Cos(FMath::DegreesToRadians(12.0));
		bSawForwardLanding |= ForwardWeight > 0.95f;
		bSawJogSeed |= Anim->GroundLocomotionStartPosition > 0.f;
		const FString State = FString::Printf(TEXT("%s,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%.5f,%.5f,%.5f"),
			Cases[CaseIndex].Name, Frame, ContactFrame, Movement.IsMovingOnGround(), Movement.Velocity.Size2D(),
			Movement.Velocity.Z, PawnPosition.X, PawnPosition.Y, PawnPosition.Z, FloorZ, NormalZ,
			ForwardWeight, Main->GetStateWeight(Stock), Anim->GroundLocomotionStartPosition);
		for (const USkeletalMeshComponent* Mesh : {Source, Body})
		{
			const TCHAR* Rig = Mesh == Source ? TEXT("manny") : TEXT("visible");
			for (const FName Bone : Bones)
			{
				const FTransform Pose = Mesh->GetBoneTransform(Bone, RTS_Component);
				const FTransform Parent = Mesh->GetBoneTransform(Mesh->GetParentBone(Bone), RTS_Component);
				if (Mesh->GetBoneIndex(Bone) == INDEX_NONE || Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized())
				{
					Finish(FString::Printf(TEXT("invalid %s rig bone %s"), Rig, *Bone.ToString()));
					return false;
				}
				const FVector Point = Mesh->GetComponentTransform().TransformPosition(Pose.GetLocation());
				const FQuat Local = Parent.GetRotation().Inverse() * Pose.GetRotation();
				if (Bone == TEXT("foot_r"))
				{
					FQuat& Last = Mesh == Source ? PreviousSourceAnkle : PreviousVisibleAnkle;
					if (ContactFrame >= 0 && Frame - ContactFrame >= 11 && Frame - ContactFrame <= 27)
					{
						const double Degrees = FMath::RadiansToDegrees(Last.AngularDistance(Local));
						PeakRightAnkleDegrees = FMath::Max(PeakRightAnkleDegrees, Degrees);
					}
					Last = Local;
				}
				FHitResult BoneFloor;
				const bool bHasFloor = bWalkable && TraceFloor(FVector(Point.X, Point.Y, FloorZ), BoneFloor);
				Rows += FString::Printf(TEXT("%s,%s,%u,%s,%.5f,%.5f,%.5f,%.8f,%.8f,%.8f,%.8f,%d,%.5f\n"),
					*State, Rig, Mesh->GetBoneTransformRevisionNumber(), *Bone.ToString(), Point.X, Point.Y, Point.Z,
					Local.X, Local.Y, Local.Z, Local.W, bHasFloor, bHasFloor ? Point.Z - BoneFloor.ImpactPoint.Z : 0.0);
			}
		}
		return true;
	}

	void Finish(const FString& Failure)
	{
		if (bFinished) { return; }
		bFinished = true;
		FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
		Key(EKeys::W, false);
		Key(EKeys::SpaceBar, false);
		if (Pawn.IsValid()) { Pawn->SetCanBeDamaged(bPreviousDamage); }
		if (Viewport.IsValid()) { Viewport->bDisableWorldRendering = bPreviousWorldRenderingDisabled; }
		const bool bWritten = FFileHelper::SaveStringToFile(Rows, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (Failure.IsEmpty() && bWritten)
		{
			UE_LOG(LogRogue10m, Display, TEXT("RESULT=FIELD_GROUND_MOTION_RECORDED path=%s"), *Output);
		}
		else
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIELD_GROUND_MOTION_FAILED csv=%d %s"), bWritten, *Failure);
		}
		FPlatformMisc::RequestExitWithStatus(false, Failure.IsEmpty() && bWritten ? 0 : 1);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mCharacter> Pawn;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	FDelegateHandle TickHandle;
	FString Output;
	bool bCapture = false;
	bool bBaseline = false;
	bool bPreviousWorldRenderingDisabled = false;
	FString Rows = TEXT("case,frame,contact_frame,grounded,speed_cm_s,velocity_z,pawn_x,pawn_y,pawn_z,floor_z,floor_normal_z,"
		"forward_land_weight,stock_land_weight,jog_seed,rig,revision,bone,world_x,world_y,world_z,local_qx,local_qy,local_qz,local_qw,has_floor,bone_clearance_cm\n");
	FVector Origin = FVector::ZeroVector;
	FQuat PreviousSourceAnkle = FQuat::Identity;
	FQuat PreviousVisibleAnkle = FQuat::Identity;
	EStage Stage = EStage::Profile;
	double Deadline = 0.0;
	double PeakRightAnkleDegrees = 0.0;
	int32 CaseIndex = 0;
	int32 Frame = 0;
	int32 ContactFrame = -1;
	bool bAscended = false;
	bool bDescended = false;
	bool bSawRamp = false;
	bool bSawForwardLanding = false;
	bool bSawJogSeed = false;
	bool bPreviousDamage = true;
	bool bFinished = false;
};

void Run()
{
	static bool bStarted = false;
	if (bStarted) { return; }
	if (!GEngine || GIsEditor)
	{
		UE_LOG(LogRogue10m, Warning, TEXT("Field ground motion QA requires a standalone game"));
		return;
	}
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	}
	FString UserDir;
	const FString Scratch = FullPath(FPaths::ProjectDir() / TEXT("tmp"));
	if (!World || World->GetNetMode() != NM_Standalone || !World->GetWorldPartition()
		|| UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TEXT("/Game/Rogue10m/Maps/OpenRunMap")
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || FPaths::IsRelative(UserDir)
		|| !FPaths::IsUnderDirectory(FullPath(UserDir), Scratch)
		|| !FPaths::IsUnderDirectory(FullPath(FPaths::ProjectSavedDir()), FullPath(UserDir)))
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIELD_GROUND_MOTION_FAILED requires isolated OpenRunMap standalone under project tmp"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	const FString Output = FullPath(UserDir) / TEXT("field-ground-motion.csv");
	if (IFileManager::Get().FileExists(*Output))
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=FIELD_GROUND_MOTION_FAILED existing report %s"), *Output);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	bStarted = true;
	const bool bCapture = FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("RogueFieldGroundMotionCapture"));
	const bool bBaseline = FParse::Param(FCommandLine::Get(), TEXT("RogueFieldGroundMotionBaseline"));
	MakeShared<FRun>(*World, Output, bCapture, bBaseline)->Start();
}

FAutoConsoleCommand Command(TEXT("Rogue10m.TestFieldGroundMotion"),
	TEXT("Record evaluated unarmed movement on the live OpenRunMap approach and cave ramp."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif
