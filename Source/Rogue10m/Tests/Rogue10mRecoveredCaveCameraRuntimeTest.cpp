// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace Rogue10mRecoveredCaveCamera
{
constexpr const TCHAR* OpenRun = TEXT("/Game/Rogue10m/Maps/OpenRunMap");
constexpr double MinimumCameraDistance = 140.0;
constexpr double MaximumPullIn = 180.0;
constexpr double MaxFloorError = 24.0;

struct FSample
{
	FString Id;
	FVector Floor = FVector::ZeroVector;
	TArray<double> YawOffsets;
	double Yaw = 0.0;
	double Pitch = -12.0;
};

FString FullPath(const FString& Path)
{
	FString Result = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Result);
	FPaths::CollapseRelativeDirectories(Result);
	return Result;
}

bool IsCaveSample(const FVector& Floor)
{
	const bool bTraining = Floor.X >= -16000 && Floor.X <= -3000 && Floor.Y >= 53500 && Floor.Y <= 71000;
	const bool bWorkshop = Floor.X >= 49600 && Floor.X <= 62000 && Floor.Y >= 45000 && Floor.Y <= 61200;
	return !Floor.ContainsNaN() && Floor.Z >= 8400 && Floor.Z <= 9850 && (bTraining || bWorkshop);
}

bool ReadVector(const FJsonObject& Entry, FVector& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	return Entry.TryGetArrayField(TEXT("floor_cm"), Values) && Values->Num() == 3
		&& (*Values)[0]->TryGetNumber(Out.X) && (*Values)[1]->TryGetNumber(Out.Y) && (*Values)[2]->TryGetNumber(Out.Z)
		&& IsCaveSample(Out);
}

bool ReadPlan(const FString& Path, TArray<FSample>& Out)
{
	const int64 Bytes = IFileManager::Get().FileSize(*Path);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (Bytes <= 0 || Bytes > 128 * 1024 || !FFileHelper::LoadFileToString(Text, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root
		|| !Root->TryGetArrayField(TEXT("samples"), Entries) || Entries->IsEmpty() || Entries->Num() > 80)
	{
		return false;
	}
	TSet<FString> Ids;
	for (const auto& Value : *Entries)
	{
		if (Value->Type != EJson::Object) { return false; }
		const FJsonObject& Entry = *Value->AsObject();
		FSample& Sample = Out.AddDefaulted_GetRef();
		if (!Entry.TryGetStringField(TEXT("id"), Sample.Id) || Sample.Id.IsEmpty() || Sample.Id.Len() > 72
			|| Ids.Contains(Sample.Id) || !ReadVector(Entry, Sample.Floor)
			|| !Entry.TryGetNumberField(TEXT("yaw_deg"), Sample.Yaw) || !FMath::IsFinite(Sample.Yaw))
		{
			return false;
		}
		Ids.Add(Sample.Id);
		if (Entry.HasField(TEXT("pitch_deg")) && (!Entry.TryGetNumberField(TEXT("pitch_deg"), Sample.Pitch)
			|| !FMath::IsFinite(Sample.Pitch) || FMath::Abs(Sample.Pitch) > 65.0)) { return false; }
		if (Entry.HasField(TEXT("yaw_offsets_deg")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Offsets = nullptr;
			if (!Entry.TryGetArrayField(TEXT("yaw_offsets_deg"), Offsets) || Offsets->IsEmpty() || Offsets->Num() > 9) { return false; }
			for (const auto& Offset : *Offsets)
			{
				double Angle = 0.0;
				if (!Offset->TryGetNumber(Angle) || !FMath::IsFinite(Angle) || FMath::Abs(Angle) > 180.0) { return false; }
				Sample.YawOffsets.Add(Angle);
			}
		}
		else { Sample.YawOffsets.Add(0.0); }
	}
	return true;
}

TArray<TSharedPtr<FJsonValue>> JsonVector(const FVector& Vector)
{
	return {MakeShared<FJsonValueNumber>(Vector.X), MakeShared<FJsonValueNumber>(Vector.Y), MakeShared<FJsonValueNumber>(Vector.Z)};
}

class FRun : public TSharedFromThis<FRun>
{
public:
	FRun(UWorld& InWorld, TArray<FSample>&& InSamples, FString InOutput)
		: World(&InWorld), Samples(MoveTemp(InSamples)), Output(MoveTemp(InOutput)) {}

	void Start()
	{
		for (const auto& Sample : Samples) { TotalViews += Sample.YawOffsets.Num(); }
		Deadline = FPlatformTime::Seconds() + 35;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self = AsShared()](float) { return Self->Tick(); }), 0.05f);
	}

private:
	enum class EStage { Pawn, Ground, Camera };

	bool Tick()
	{
		if (!World.IsValid()) { return Finish(TEXT("The standalone test world ended before all views were measured")); }
		const double Now = FPlatformTime::Seconds();
		if (Now > Deadline)
		{
			const TCHAR* StageName = Stage == EStage::Pawn ? TEXT("default pawn") : Stage == EStage::Ground ? TEXT("streaming and floor") : TEXT("grounded camera stabilization");
			return Finish(FString::Printf(TEXT("Timed out waiting for %s at %s"), StageName, *Samples[SampleIndex].Id));
		}
		if (Stage == EStage::Pawn)
		{
			APlayerController* PC = World->GetFirstPlayerController();
			ARogue10mCharacter* Candidate = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
			if (!Candidate || !Candidate->IsLocallyControlled() || !PC->PlayerCameraManager) { return true; }
			USpringArmComponent* Arm = Candidate->GetThirdPersonCameraBoom();
			TInlineComponentArray<UCameraComponent*> Cameras(Candidate);
			UCameraComponent* Camera = nullptr;
			for (UCameraComponent* Component : Cameras)
			{
				if (Component->GetAttachParent() == Arm && Component->IsActive()) { Camera = Component; break; }
			}
			if (!Candidate->IsThirdPersonGameplayActive() || !Arm || !Camera || !Arm->bDoCollisionTest
				|| Arm->ProbeChannel != ECC_Camera || !FMath::IsNearlyEqual(Arm->ProbeSize, 14.0f, 0.1f)
				|| !FMath::IsNearlyEqual(Arm->TargetArmLength, 320.0f, 0.1f))
			{
				return Finish(TEXT("The active pawn does not use the expected default third-person camera"));
			}
			Controller = PC;
			Pawn = Candidate;
			Boom = Arm;
			DefaultCamera = Camera;
			bPreviousDamage = Candidate->CanBeDamaged();
			Candidate->SetCanBeDamaged(false);
			return Warp(Now);
		}
		if (!Pawn.IsValid() || !Controller.IsValid() || Controller->GetPawn() != Pawn.Get() || !Boom.IsValid() || !DefaultCamera.IsValid())
		{
			return Finish(TEXT("The default possessed pawn or camera changed during verification"));
		}
		if (Now < ReadyAt) { return true; }
		return Stage == EStage::Ground ? Land(Now) : MeasureStableCamera(Now);
	}

	bool Warp(double Now)
	{
		const FSample& Sample = Samples[SampleIndex];
		Stage = EStage::Ground;
		Deadline = Now + 16;
		ReadyAt = Now + 0.45;
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->GetCharacterMovement()->DisableMovement();
		const FVector Placement = Sample.Floor + FVector(0, 0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 6.0);
		if (!Pawn->SetActorLocation(Placement, false, nullptr, ETeleportType::TeleportPhysics))
		{
			return Finish(FString::Printf(TEXT("Could not place the test pawn at %s"), *Sample.Id));
		}
		return true;
	}

	bool Land(double Now)
	{
		auto* Streaming = World->GetSubsystem<UWorldPartitionSubsystem>();
		if (!Streaming || !Streaming->IsAllStreamingCompleted()) { return true; }
		const FSample& Sample = Samples[SampleIndex];
		FCollisionQueryParams Query(SCENE_QUERY_STAT(RecoveredCaveFloor), false, Pawn.Get());
		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor, Sample.Floor + FVector(0, 0, 80), Sample.Floor - FVector(0, 0, 60), ECC_Pawn, Query)
			|| FMath::Abs(Floor.ImpactPoint.Z - Sample.Floor.Z) > MaxFloorError || Floor.ImpactNormal.Z < 0.72)
		{
			return true;
		}
		const auto* Capsule = Pawn->GetCapsuleComponent();
		const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FVector Placement = Floor.ImpactPoint + FVector(0, 0, HalfHeight + 5.0);
		const FCollisionShape Body = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 1.0f, HalfHeight - 1.0);
		if (World->OverlapBlockingTestByChannel(Placement, FQuat::Identity, ECC_Pawn, Body, Query))
		{
			return Finish(FString::Printf(TEXT("A blocking surface overlaps the native player capsule at %s"), *Sample.Id));
		}
		ObservedFloor = Floor.ImpactPoint;
		Pawn->SetActorLocation(Placement, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Stage = EStage::Camera;
		SetView(Now);
		return true;
	}

	void SetView(double Now)
	{
		const FSample& Sample = Samples[SampleIndex];
		DesiredView = FRotator(Sample.Pitch, FRotator::NormalizeAxis(Sample.Yaw + Sample.YawOffsets[ViewIndex]), 0);
		Controller->SetControlRotation(DesiredView);
		StableFrames = 0;
		ReadyAt = Now + 0.3;
		Deadline = Now + 12;
	}

	bool MeasureStableCamera(double Now)
	{
		APlayerCameraManager* PCM = Controller->PlayerCameraManager;
		if (!PCM || Controller->GetViewTarget() != Pawn.Get() || !DefaultCamera->IsActive()
			|| !Pawn->GetCharacterMovement()->IsMovingOnGround() || !PCM->GetCameraRotation().Equals(DesiredView, 1.0))
		{
			StableFrames = 0;
			return true;
		}
		const FVector Camera = PCM->GetCameraLocation();
		if (Camera.ContainsNaN() || !Camera.Equals(DefaultCamera->GetComponentLocation(), 2.0)) { StableFrames = 0; return true; }
		StableFrames = Camera.Equals(PreviousCamera, 0.5) ? StableFrames + 1 : 0;
		PreviousCamera = Camera;
		if (StableFrames < 3) { return true; }

		const FSample& Sample = Samples[SampleIndex];
		const FVector Pivot = Boom->GetComponentLocation() + Boom->TargetOffset;
		const FVector Unfixed = Boom->GetUnfixedCameraPosition();
		const double Distance = FVector::Distance(Pivot, Camera);
		const double PullIn = FMath::Max(0.0, FVector::Distance(Pivot, Unfixed) - Distance);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(RecoveredCaveCamera), true, Pawn.Get());
		FHitResult CameraHit;
		const bool bHit = World->SweepSingleByChannel(CameraHit, Pivot, Camera, FQuat::Identity, ECC_Camera,
			FCollisionShape::MakeSphere(Boom->ProbeSize), Query);
		const bool bBlocked = bHit && (CameraHit.bStartPenetrating || CameraHit.Distance + 0.5 < Distance);
		const bool bViewPassed = !Pivot.ContainsNaN() && !Unfixed.ContainsNaN()
			&& Distance >= MinimumCameraDistance && PullIn <= MaximumPullIn && !bBlocked;
		MinimumObservedDistance = FMath::Min(MinimumObservedDistance, Distance);
		MaximumObservedPullIn = FMath::Max(MaximumObservedPullIn, PullIn);
		if (!bViewPassed) { ++Failures; }

		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("id"), Sample.Id);
		Row->SetBoolField(TEXT("passed"), bViewPassed);
		Row->SetArrayField(TEXT("observed_floor_cm"), JsonVector(ObservedFloor));
		Row->SetArrayField(TEXT("pawn_cm"), JsonVector(Pawn->GetActorLocation()));
		Row->SetArrayField(TEXT("pivot_cm"), JsonVector(Pivot));
		Row->SetArrayField(TEXT("camera_cm"), JsonVector(Camera));
		Row->SetArrayField(TEXT("unfixed_camera_cm"), JsonVector(Unfixed));
		Row->SetNumberField(TEXT("pitch_deg"), DesiredView.Pitch);
		Row->SetNumberField(TEXT("yaw_deg"), DesiredView.Yaw);
		Row->SetNumberField(TEXT("yaw_offset_deg"), Sample.YawOffsets[ViewIndex]);
		Row->SetNumberField(TEXT("camera_distance_cm"), Distance);
		Row->SetNumberField(TEXT("camera_pull_in_cm"), PullIn);
		Row->SetNumberField(TEXT("fov_deg"), PCM->GetFOVAngle());
		Row->SetBoolField(TEXT("spring_arm_collision_adjusted"), Boom->IsCollisionFixApplied());
		Row->SetBoolField(TEXT("complex_camera_path_blocked"), bBlocked);
		if (bHit)
		{
			Row->SetStringField(TEXT("camera_hit_actor"), GetNameSafe(CameraHit.GetActor()));
			Row->SetNumberField(TEXT("camera_hit_distance_cm"), CameraHit.Distance);
		}
		Views.Add(MakeShared<FJsonValueObject>(Row));
		if (++ViewIndex < Sample.YawOffsets.Num()) { SetView(Now); return true; }
		ViewIndex = 0;
		if (++SampleIndex < Samples.Num()) { return Warp(Now); }
		return Finish(FString());
	}

	bool Finish(const FString& Error)
	{
		if (Pawn.IsValid())
		{
			Pawn->SetCanBeDamaged(bPreviousDamage);
			if (Stage == EStage::Ground) { Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
		}
		const bool bPassed = Error.IsEmpty() && Failures == 0 && Views.Num() == TotalViews;
		const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("passed"), bPassed);
		Report->SetStringField(TEXT("map"), OpenRun);
		Report->SetStringField(TEXT("method"), TEXT("rendered standalone, default possessed camera, grounded viewpoints placed by QA fixture"));
		Report->SetNumberField(TEXT("view_count"), Views.Num());
		Report->SetNumberField(TEXT("planned_view_count"), TotalViews);
		Report->SetNumberField(TEXT("failure_count"), Failures + (Error.IsEmpty() ? 0 : 1));
		Report->SetStringField(TEXT("error"), Error);
		if (!Views.IsEmpty())
		{
			Report->SetNumberField(TEXT("minimum_camera_distance_cm"), MinimumObservedDistance);
			Report->SetNumberField(TEXT("maximum_camera_pull_in_cm"), MaximumObservedPullIn);
		}
		Report->SetArrayField(TEXT("views"), Views);
		FString Text;
		const bool bWritten = FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text))
			&& FFileHelper::SaveStringToFile(Text, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (bWritten && bPassed)
		{
			UE_LOG(LogRogue10m, Display, TEXT("RESULT=RECOVERED_CAVE_CAMERA_PASSED views=%d minimum=%.2f maximum_pull_in=%.2f"),
				Views.Num(), MinimumObservedDistance, MaximumObservedPullIn);
		}
		else
		{
			UE_LOG(LogRogue10m, Error, TEXT("RESULT=RECOVERED_CAVE_CAMERA_FAILED views=%d/%d failed=%d output_written=%d %s"),
				Views.Num(), TotalViews, Failures, bWritten, *Error);
		}
		FPlatformMisc::RequestExitWithStatus(false, bWritten && bPassed ? 0 : 1);
		return false;
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<ARogue10mCharacter> Pawn;
	TWeakObjectPtr<USpringArmComponent> Boom;
	TWeakObjectPtr<UCameraComponent> DefaultCamera;
	TArray<FSample> Samples;
	TArray<TSharedPtr<FJsonValue>> Views;
	FString Output;
	FVector ObservedFloor = FVector::ZeroVector;
	FVector PreviousCamera = FVector::ZeroVector;
	FRotator DesiredView = FRotator::ZeroRotator;
	EStage Stage = EStage::Pawn;
	double Deadline = 0.0;
	double ReadyAt = 0.0;
	double MinimumObservedDistance = TNumericLimits<double>::Max();
	double MaximumObservedPullIn = 0.0;
	int32 SampleIndex = 0;
	int32 ViewIndex = 0;
	int32 StableFrames = 0;
	int32 Failures = 0;
	int32 TotalViews = 0;
	bool bPreviousDamage = true;
};

void Run()
{
	if (!GEngine || GIsEditor || !FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		UE_LOG(LogRogue10m, Error, TEXT("Recovered cave camera verification requires a rendered standalone game"));
		return;
	}
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	}
	FString PlanArgument, UserDirArgument;
	const FString Scratch = FullPath(FPaths::ProjectDir() / TEXT("tmp"));
	TArray<FSample> Samples;
	if (!World || World->GetNetMode() != NM_Standalone || !World->GetWorldPartition()
		|| UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != OpenRun
		|| !FParse::Value(FCommandLine::Get(), TEXT("RogueRecoveredCaveCameraPlan="), PlanArgument)
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirArgument)
		|| FPaths::IsRelative(PlanArgument) || FPaths::IsRelative(UserDirArgument))
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=RECOVERED_CAVE_CAMERA_FAILED required OpenRunMap and absolute scratch plan/UserDir"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	const FString Plan = FullPath(PlanArgument);
	const FString UserDir = FullPath(UserDirArgument);
	const FString Output = FPaths::GetPath(Plan) / (FPaths::GetBaseFilename(Plan) + TEXT(".results.json"));
	if (!FPaths::IsUnderDirectory(Plan, Scratch) || !FPaths::IsUnderDirectory(UserDir, Scratch)
		|| !FPaths::IsUnderDirectory(FullPath(FPaths::ProjectSavedDir()), UserDir)
		|| IFileManager::Get().FileExists(*Output) || !ReadPlan(Plan, Samples))
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=RECOVERED_CAVE_CAMERA_FAILED invalid scratch plan, UserDir or existing result"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	static bool bStarted = false;
	if (bStarted) { return; }
	bStarted = true;
	MakeShared<FRun>(*World, MoveTemp(Samples), Output)->Start();
}

FAutoConsoleCommand Command(TEXT("Rogue10m.TestRecoveredCaveCamera"),
	TEXT("Verify the default camera at recovered OpenRunMap cave viewpoints in an isolated standalone game."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif
