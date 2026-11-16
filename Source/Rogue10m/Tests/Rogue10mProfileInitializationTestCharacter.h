// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Rogue10mCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Rogue10mProfileInitializationTestCharacter.generated.h"

/** Native transient fixture: retain production possession, without a Blueprint or mesh. */
UCLASS(Transient, NotBlueprintable)
class ARogue10mProfileInitializationTestCharacter final : public ARogue10mCharacter
{
	GENERATED_BODY()
public:
	ARogue10mProfileInitializationTestCharacter()
	{
		GetFirstPersonCameraComponent()->SetupAttachment(GetCapsuleComponent());
	}
};
