// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rogue10mCharacterCustomizationTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mCharacterCustomizationPreviewActor.generated.h"

class UAnimInstance;
class UMaterialInterface;
class UPointLightComponent;
class URogue10mCharacterCustomizationDataAsset;
class USceneCaptureComponent2D;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

UCLASS(NotBlueprintable, NotPlaceable, Transient)
class ROGUE10M_API ARogue10mCharacterCustomizationPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ARogue10mCharacterCustomizationPreviewActor();

	bool InitializePreview(
		URogue10mCharacterCustomizationDataAsset* InCatalog,
		FIntPoint Resolution);
	bool SetAppearance(const FRogue10mCharacterAppearance& Appearance);
	void SetPreviewActive(bool bActive);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

private:
	void FrameCharacter();
	void CapturePreview();

#if WITH_EDITOR
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void OnPreviewMaterialCompiled(UMaterialInterface* CompiledMaterial);
	void StopWatchingPreviewMaterials();

	FDelegateHandle PreviewMaterialCompilationHandle;
	bool bMaterialRecaptureQueued = false;
#endif

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USceneComponent> PreviewRoot;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USkeletalMeshComponent> AnimationSourceMesh;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USkeletalMeshComponent> HairMesh;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USkeletalMeshComponent> FacialMesh;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category="Rogue10m|Character Customization Preview")
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY(Transient)
	TObjectPtr<URogue10mCharacterCustomizationDataAsset> Catalog;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> AnimationSourceSkeletalMesh;

	UPROPERTY()
	TSoftClassPtr<UAnimInstance> AnimationSourceAnimClass;
};
