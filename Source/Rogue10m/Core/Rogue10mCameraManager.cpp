// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCameraManager.h"

#include "GameFramework/PlayerController.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"

ARogue10mCameraManager::ARogue10mCameraManager()
{
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ARogue10mCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ApplyCameraModifiers(DeltaTime, InOutPOV);

	ARogue10mCharacter* Character = PCOwner ? Cast<ARogue10mCharacter>(PCOwner->GetPawn()) : nullptr;
	if (Character && Character->GetAppearanceCameraComponent())
 { Character->GetAppearanceCameraComponent()->RefreshAppearanceCamera(); }
	URogue10mFirstPersonPresentationComponent* Presentation = Character
		? Character->GetFirstPersonPresentationComponent() : nullptr;
	if (LastFistPresentation.Get() != Presentation)
	{
		if (LastFistPresentation.IsValid()) { LastFistPresentation->ResetCameraMotion(); }
		LastFistPresentation = Presentation;
	}
	if (!Presentation || (Character && Character->GetAppearanceCameraComponent()
  && Character->GetAppearanceCameraComponent()->IsAppearanceCameraActive()))
	{
		return;
	}

	// A transition evaluates two view targets. Do not filter twice or affect a cutscene/debug view.
	static const FName DefaultStyle(TEXT("Default"));
	static const FName FirstPersonStyle(TEXT("FirstPerson"));
	const bool bFirstPersonStyle = CameraStyle.IsNone() || CameraStyle == DefaultStyle || CameraStyle == FirstPersonStyle;
	if (ViewTarget.Target != Character || PendingViewTarget.Target || !bFirstPersonStyle)
	{
		Presentation->ResetCameraMotion();
		return;
	}
	Presentation->ApplyBodyMotionToView(DeltaTime, InOutPOV);
}

void ARogue10mCameraManager::LimitViewPitch(FRotator& ViewRotation, float InViewPitchMin, float InViewPitchMax)
{
	const ARogue10mCharacter* Character = PCOwner ? Cast<ARogue10mCharacter>(PCOwner->GetPawn()) : nullptr;
	const URogue10mFirstPersonPresentationComponent* Presentation = Character
		? Character->GetFirstPersonPresentationComponent() : nullptr;
	static const FName DefaultStyle(TEXT("Default"));
	static const FName FirstPersonStyle(TEXT("FirstPerson"));
	const bool bFirstPersonStyle = CameraStyle.IsNone() || CameraStyle == DefaultStyle || CameraStyle == FirstPersonStyle;
	if (Character && Character->IsLocallyControlled() && ((Presentation && Presentation->IsFullBodyPresentationActive())
   || (Character->GetAppearanceCameraComponent() && Character->GetAppearanceCameraComponent()->IsAppearanceCameraActive()))
		&& ViewTarget.Target == Character && !PendingViewTarget.Target && bFirstPersonStyle && FMath::IsFinite(FullBodyViewPitchMin))
	{
		InViewPitchMin = FMath::Min(InViewPitchMin, FMath::Clamp(FullBodyViewPitchMin, -89.0f, -70.0f));
	}
	Super::LimitViewPitch(ViewRotation, InViewPitchMin, InViewPitchMax);
}
