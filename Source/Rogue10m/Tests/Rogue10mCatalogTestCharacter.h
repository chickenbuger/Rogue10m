#pragma once

#include "Rogue10mCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Rogue10mCatalogTestCharacter.generated.h"

/** Content-independent fixture; no Blueprint, appearance assets, or gameplay initialization. */
UCLASS(Transient, NotBlueprintable)
class ARogue10mCatalogTestCharacter final : public ARogue10mCharacter
{
	GENERATED_BODY()

public:
	ARogue10mCatalogTestCharacter()
	{
		// The fixture has no skeletal mesh or head socket; preserve the camera without skeletal queries.
		GetFirstPersonCameraComponent()->SetupAttachment(GetCapsuleComponent());
	}
	virtual void PossessedBy(AController* NewController) override { ACharacter::PossessedBy(NewController); }
};
