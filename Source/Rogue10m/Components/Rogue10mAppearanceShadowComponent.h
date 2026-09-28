// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mAppearanceShadowComponent.generated.h"

class UPoseableMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT()
struct FRogue10mAppearanceShadowEntry
{
 GENERATED_BODY()
 TWeakObjectPtr<USkeletalMeshComponent> Source;
 UPROPERTY(Transient)
 TObjectPtr<UPoseableMeshComponent> Proxy;
 TWeakObjectPtr<USkeletalMesh> MappedBodyAsset;
 TWeakObjectPtr<USkeletalMesh> MappedPartAsset;
 TArray<int32> BodyBoneMap;
 TArray<FTransform> PartComponentPose;
 bool bSavedCastShadow = false;
 bool bApplied = false;
 bool bSeen = false;
};

/** Shadow-only copies of the appearance's unhidden local pose; never evaluates animation. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mAppearanceShadowComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URogue10mAppearanceShadowComponent();
 /** Call after the appearance pose is available. Disabled outside the head-hidden owner view. */
 void RefreshShadow(USkeletalMeshComponent* Body, bool bEnable);
 void RestoreShadow();
 UPoseableMeshComponent* GetShadowBodyProxy() const;
 UPoseableMeshComponent* GetShadowProxyForSource(USkeletalMeshComponent* Source) const;
 bool IsShadowActive() const { return bActive; }
protected:
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 FRogue10mAppearanceShadowEntry* FindOrCreateEntry(USkeletalMeshComponent* Source);
 void RestoreEntry(FRogue10mAppearanceShadowEntry& Entry);
 void UpdateMaterials(FRogue10mAppearanceShadowEntry& Entry);
 void UpdatePartPose(FRogue10mAppearanceShadowEntry& Entry, USkeletalMeshComponent& Body);
 void ActivateEntry(FRogue10mAppearanceShadowEntry& Entry);
 UPROPERTY(Transient)
 TArray<FRogue10mAppearanceShadowEntry> Entries;
 TWeakObjectPtr<USkeletalMeshComponent> AppliedBody;
 TArray<FTransform> BodyComponentPose;
 bool bActive = false;
};
