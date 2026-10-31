// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mOwnerArmsComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/** Project-owned arms geometry derived from an unchanged full appearance asset. */
USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mOwnerArmsVariant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TSoftObjectPtr<USkeletalMesh> ArmsMesh;
};

/** Owner-only geometry follower. Never owns animation, gameplay, body visibility or a camera. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mOwnerArmsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mOwnerArmsComponent();

	/** Body must belong to this locally controlled character. False leaves the proxy hidden. */
	bool Refresh(USkeletalMeshComponent* Body, bool bEnable);
	virtual void Deactivate() override;

	UFUNCTION(BlueprintPure, Category="Rogue10m|Owner Arms")
	USkeletalMeshComponent* GetOwnerArmsMesh() const { return OwnerArmsMesh.Get(); }

	UFUNCTION(BlueprintPure, Category="Rogue10m|Owner Arms")
	bool IsOwnerArmsActive() const { return IsActive() && OwnerArmsMesh != nullptr && AppliedBody.IsValid(); }

	/** Explicit soft references retain the derivatives for cooking and future appearance variants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Owner Arms")
	TArray<FRogue10mOwnerArmsVariant> AssetVariants;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	USkeletalMesh* ResolveArmsMesh(USkeletalMesh& BodyAsset);
	bool HasMatchingReferencePose(const USkeletalMesh& BodyAsset, const USkeletalMesh& ArmsAsset) const;
	bool EnsureProxy();
	void SyncMaterials(const USkeletalMeshComponent& Body);
	void WarnOnce(const FString& Key, const FString& Reason);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OwnerArmsMesh;

	TWeakObjectPtr<USkeletalMeshComponent> AppliedBody;
	TWeakObjectPtr<USkeletalMesh> ResolvedBodyAsset;
	TWeakObjectPtr<USkeletalMesh> ResolvedArmsAsset;
	FSoftObjectPath ResolvedArmsPath;
	TSet<FString> ReportedWarnings;
	bool bResolutionAttempted = false;
};
