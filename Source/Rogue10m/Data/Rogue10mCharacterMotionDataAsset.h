// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mCharacterMotionDataAsset.generated.h"

class UAnimMontage;
class UNiagaraSystem;

/** 종족과 무관하게 사용하는 기본 이동 모션과 Niagara Cue 모음입니다. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mCharacterMotionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Identity")
	FName MotionSetId = TEXT("CommonUnarmed");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

	/** 애니메이션 단독 프리뷰에서는 끄고, 이동 파티클 확인 시에만 켭니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	bool bEnableMotionEffects = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> WalkFootstepEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> RunFootstepEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> JumpEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> DoubleJumpEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> LandEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	TObjectPtr<UNiagaraSystem> DodgeEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Footstep", meta=(ClampMin="0.05", Units="s"))
	float WalkFootstepInterval = 0.68f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Footstep", meta=(ClampMin="0.05", Units="s"))
	float RunFootstepInterval = 0.38f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Footstep", meta=(ClampMin="0.0", Units="cm/s"))
	float MinimumFootstepSpeed = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Footstep", meta=(ClampMin="0.0", Units="cm"))
	float FootstepLateralOffset = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|Footstep", meta=(ClampMin="0.0", Units="cm"))
	float FootstepRearOffset = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float WalkEffectScale = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float RunEffectScale = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float JumpEffectScale = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float DoubleJumpEffectScale = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float LandEffectScale = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01"))
	float DodgeEffectScale = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float WalkEffectEmissionDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float RunEffectEmissionDuration = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float JumpEffectEmissionDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float DoubleJumpEffectEmissionDuration = 0.14f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float LandEffectEmissionDuration = 0.14f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.01", Units="s"))
	float DodgeEffectEmissionDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX", meta=(ClampMin="0.1"))
	float MotionEffectTimeDilation = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Motion|VFX")
	FVector GroundEffectOffset = FVector(0.0f, 0.0f, 2.0f);
};
