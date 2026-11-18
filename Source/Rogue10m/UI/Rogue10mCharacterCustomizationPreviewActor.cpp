// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacterCustomizationPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rogue10m.h"
#include "Rogue10mCharacterCustomizationDataAsset.h"

namespace
{
	constexpr float CustomizationPreviewStagingHeight = 75000.0f;
	constexpr float CustomizationPreviewFieldOfView = 30.0f;
	constexpr float CustomizationPreviewFrameMargin = 1.10f;
	constexpr float LobbyPreviewFacingYaw = -90.0f;
	constexpr float PreviewFloorNdc = 0.90f;

	void EvaluatePreviewPose(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->GetSkeletalMeshAsset())
		{
			return;
		}
		// A lobby can be paused before its first component tick. Evaluate this
		// on-demand snapshot without advancing animation time or enabling Tick.
		if (Mesh->GetAnimInstance())
		{
			Mesh->TickAnimation(0.0f, false);
		}
		Mesh->RefreshBoneTransforms();
		Mesh->UpdateComponentToWorld();
		Mesh->UpdateBounds();
		Mesh->MarkRenderDynamicDataDirty();
	}
}

ARogue10mCharacterCustomizationPreviewActor::ARogue10mCharacterCustomizationPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);

	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Preview Root"));
	PreviewRoot->SetupAttachment(SceneRoot);

	AnimationSourceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Animation Source Mesh"));
	AnimationSourceMesh->SetupAttachment(PreviewRoot);
	AnimationSourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimationSourceMesh->SetGenerateOverlapEvents(false);
	AnimationSourceMesh->SetCastShadow(false);
	AnimationSourceMesh->SetReceivesDecals(false);
	AnimationSourceMesh->SetVisibility(false, false);
	AnimationSourceMesh->SetHiddenInGame(true, false);
	AnimationSourceMesh->VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	auto ConfigureMesh = [](USkeletalMeshComponent* Mesh, USceneComponent* Parent)
	{
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetVisibleInSceneCaptureOnly(true);
		Mesh->VisibilityBasedAnimTickOption =
			EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	};

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body Mesh"));
	ConfigureMesh(BodyMesh, AnimationSourceMesh);
	BodyMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);

	HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hair Mesh"));
	ConfigureMesh(HairMesh, BodyMesh);
	FacialMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Facial Mesh"));
	ConfigureMesh(FacialMesh, BodyMesh);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Character Capture"));
	SceneCapture->SetupAttachment(SceneRoot);
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	SceneCapture->FOVAngle = CustomizationPreviewFieldOfView;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->MaxViewDistanceOverride = 2000.0f;
	SceneCapture->ShowFlags.SetAtmosphere(false);
	SceneCapture->ShowFlags.SetFog(false);
	SceneCapture->ShowFlags.SetVolumetricFog(false);
	SceneCapture->ShowFlags.SetMotionBlur(false);
	SceneCapture->ShowFlags.SetBloom(false);

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Key Light"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(190.0f, 150.0f, 230.0f));
	KeyLight->SetIntensity(5000.0f);
	KeyLight->SetLightColor(FLinearColor(1.0f, 0.82f, 0.66f));
	KeyLight->SetAttenuationRadius(750.0f);
	KeyLight->SetCastShadows(false);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill Light"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetRelativeLocation(FVector(100.0f, -230.0f, 130.0f));
	FillLight->SetIntensity(2600.0f);
	FillLight->SetLightColor(FLinearColor(0.38f, 0.58f, 1.0f));
	FillLight->SetAttenuationRadius(700.0f);
	FillLight->SetCastShadows(false);

	RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Rim Light"));
	RimLight->SetupAttachment(SceneRoot);
	RimLight->SetRelativeLocation(FVector(-170.0f, 20.0f, 210.0f));
	RimLight->SetIntensity(3600.0f);
	RimLight->SetLightColor(FLinearColor(0.48f, 0.68f, 1.0f));
	RimLight->SetAttenuationRadius(700.0f);
	RimLight->SetCastShadows(false);

	SceneCapture->ShowOnlyComponent(BodyMesh);
	SceneCapture->ShowOnlyComponent(HairMesh);
	SceneCapture->ShowOnlyComponent(FacialMesh);

	AnimationSourceSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(
		FSoftObjectPath(TEXT(
			"/Game/Characters/Mannequins/Meshes/"
			"SKM_Manny_Simple.SKM_Manny_Simple")));
	AnimationSourceAnimClass = TSoftClassPtr<UAnimInstance>(
		FSoftClassPath(TEXT(
			"/Game/Characters/Mannequins/Anims/Unarmed/"
			"ABP_Unarmed.ABP_Unarmed_C")));
}

bool ARogue10mCharacterCustomizationPreviewActor::InitializePreview(
	URogue10mCharacterCustomizationDataAsset* InCatalog, FIntPoint Resolution)
{
	if (!InCatalog)
	{
		return false;
	}

	USkeletalMesh* SourceMesh = AnimationSourceSkeletalMesh.LoadSynchronous();
	UClass* SourceAnimClass = AnimationSourceAnimClass.LoadSynchronous();
	if (!SourceMesh || !SourceAnimClass)
	{
		UE_LOG(
			LogRogue10m, Error,
			TEXT("Character preview source mesh or AnimBP is missing. Mesh=%s AnimBP=%s"),
			*AnimationSourceSkeletalMesh.ToSoftObjectPath().ToString(),
			*AnimationSourceAnimClass.ToSoftObjectPath().ToString());
		return false;
	}

	Catalog = InCatalog;
	SetActorLocation(FVector(0.0f, 0.0f, CustomizationPreviewStagingHeight));
	AnimationSourceMesh->SetSkeletalMeshAsset(SourceMesh);
	AnimationSourceMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	AnimationSourceMesh->SetAnimInstanceClass(SourceAnimClass);

	RenderTarget = NewObject<UTextureRenderTarget2D>(
		this, TEXT("CharacterCustomizationRenderTarget"), RF_Transient);
	if (!RenderTarget)
	{
		return false;
	}

	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
	RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	RenderTarget->InitAutoFormat(
		FMath::Clamp(Resolution.X, 256, 2048),
		FMath::Clamp(Resolution.Y, 256, 2048));
	RenderTarget->UpdateResourceImmediate(true);
	SceneCapture->TextureTarget = RenderTarget;
	UE_LOG(LogRogue10m, Display,
		TEXT("Character lobby preview initialized. SourceMesh=%s SourceAnimBP=%s Resolution=%dx%d"),
		*GetPathNameSafe(SourceMesh), *GetPathNameSafe(SourceAnimClass),
		RenderTarget->SizeX, RenderTarget->SizeY);
	return true;
}

bool ARogue10mCharacterCustomizationPreviewActor::SetAppearance(
	const FRogue10mCharacterAppearance& Appearance)
{
	if (!Catalog || !Catalog->ApplyAppearance(
		Appearance, BodyMesh, HairMesh, FacialMesh))
	{
		return false;
	}

	const FRogue10mCharacterArchetype* Archetype =
		Catalog->FindArchetype(Appearance.Race, Appearance.Gender);
	UClass* RetargetAnimClass =
		Archetype ? Archetype->RetargetAnimClass.LoadSynchronous() : nullptr;
	if (RetargetAnimClass)
	{
		if (BodyMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}
		if (BodyMesh->GetAnimClass() != RetargetAnimClass)
		{
			BodyMesh->SetAnimInstanceClass(RetargetAnimClass);
		}
	}
	else
	{
		UE_LOG(
			LogRogue10m, Warning,
			TEXT("Character preview Retarget AnimBP is missing; showing reference pose."));
	}

	PreviewRoot->SetRelativeRotation(
		FRotator(0.0f, LobbyPreviewFacingYaw, 0.0f));
	BodyMesh->SetVisibility(true, false);
	HairMesh->SetVisibility(HairMesh->GetSkeletalMeshAsset() != nullptr, false);
	FacialMesh->SetVisibility(FacialMesh->GetSkeletalMeshAsset() != nullptr, false);
	CapturePreview();
	return true;
}

void ARogue10mCharacterCustomizationPreviewActor::SetPreviewActive(bool bActive)
{
	BodyMesh->SetVisibility(bActive, false);
	HairMesh->SetVisibility(bActive && HairMesh->GetSkeletalMeshAsset(), false);
	FacialMesh->SetVisibility(bActive && FacialMesh->GetSkeletalMeshAsset(), false);
	KeyLight->SetVisibility(bActive);
	FillLight->SetVisibility(bActive);
	RimLight->SetVisibility(bActive);
	if (bActive)
	{
		CapturePreview();
	}
}

void ARogue10mCharacterCustomizationPreviewActor::FrameCharacter()
{
	FBox Bounds(ForceInit);
	for (USkeletalMeshComponent* Mesh : { BodyMesh.Get(), HairMesh.Get(), FacialMesh.Get() })
	{
		if (Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->IsVisible())
		{
			Mesh->UpdateBounds();
			Bounds += Mesh->Bounds.GetBox();
		}
	}
	if (!Bounds.IsValid)
	{
		return;
	}

	const FVector Center = GetActorTransform().InverseTransformPosition(Bounds.GetCenter());
	const FVector Extent = Bounds.GetExtent();
	// SceneCapture uses horizontal FOV; a portrait render target has a wider
	// vertical half-angle. Fit both axes and allow for the nearest bound depth.
	const float Aspect = RenderTarget
		? static_cast<float>(RenderTarget->SizeX) / FMath::Max(1, RenderTarget->SizeY)
		: 1.0f;
	const float HalfFov = FMath::DegreesToRadians(SceneCapture->FOVAngle * 0.5f);
	const float HorizontalTangent = FMath::Max(0.01f, FMath::Tan(HalfFov));
	const float VerticalTangent = HorizontalTangent / FMath::Max(0.01f, Aspect);
	const float FitDistance = FMath::Max(
		Extent.Y / HorizontalTangent, Extent.Z / VerticalTangent);
	const float Distance = FMath::Max(
		190.0f, Extent.X + FitDistance * CustomizationPreviewFrameMargin);
	const float NearestDistance = FMath::Max(1.0f, Distance - Extent.X);
	const float FloorHeight = Center.Z - Extent.Z;
	const FVector CameraLocation(Center.X + Distance, Center.Y,
		FloorHeight + NearestDistance * VerticalTangent * PreviewFloorNdc);
	SceneCapture->SetRelativeLocation(CameraLocation);
	// Keep the camera level and anchor the lowest bound near the preview floor,
	// including wider/shorter races, instead of centering them in empty space.
	SceneCapture->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
}

void ARogue10mCharacterCustomizationPreviewActor::CapturePreview()
{
	if (SceneCapture && SceneCapture->TextureTarget)
	{
		// RetargetPoseFromMesh must read a completed source pose. With nullptr
		// tick functions RefreshBoneTransforms evaluates synchronously.
		EvaluatePreviewPose(AnimationSourceMesh);
		EvaluatePreviewPose(BodyMesh);
		BodyMesh->RefreshFollowerComponents();
		FrameCharacter();
		SceneCapture->CaptureScene();
	}
}
