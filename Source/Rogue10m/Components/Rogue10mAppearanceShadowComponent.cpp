// Copyright Epic Games, Inc. All Rights Reserved.
#include "Components/Rogue10mAppearanceShadowComponent.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

URogue10mAppearanceShadowComponent::URogue10mAppearanceShadowComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
}

FRogue10mAppearanceShadowEntry* URogue10mAppearanceShadowComponent::FindOrCreateEntry(USkeletalMeshComponent* Source)
{
 FRogue10mAppearanceShadowEntry* Entry = Entries.FindByPredicate(
  [Source](const FRogue10mAppearanceShadowEntry& Item) { return Item.Source.Get() == Source; });
 if (!Entry)
 {
  AActor* Owner = GetOwner();
  if (!Owner) return nullptr;
  Entry = &Entries.AddDefaulted_GetRef();
  Entry->Source = Source;
  Entry->Proxy = NewObject<UPoseableMeshComponent>(Owner, NAME_None, RF_Transient);
  UPoseableMeshComponent* Proxy = Entry->Proxy;
  Owner->AddInstanceComponent(Proxy);
  Proxy->SetupAttachment(Source);
  Proxy->SetRelativeTransform(FTransform::Identity);
  Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Proxy->SetGenerateOverlapEvents(false);
  Proxy->SetCanEverAffectNavigation(false);
  Proxy->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
  Proxy->SetOwnerNoSee(false);
  Proxy->SetOnlyOwnerSee(false);
  Proxy->SetVisibility(true);
  Proxy->SetHiddenInGame(true);
  Proxy->SetCastShadow(false);
  Proxy->SetCastHiddenShadow(true);
  Proxy->SetRenderInMainPass(false);
  Proxy->SetRenderInDepthPass(false);
  Proxy->SetRenderCustomDepth(false);
  Proxy->SetVisibleInRayTracing(false);
  Proxy->SetReceivesDecals(false);
  Proxy->PrimaryComponentTick.bCanEverTick = false;
  Proxy->RegisterComponent();
  Proxy->SetComponentTickEnabled(false);
 }
 if (Entry->Proxy->GetSkinnedAsset() != Source->GetSkeletalMeshAsset())
 {
  // Force the poseable component to rebuild RequiredBones even when the new
  // appearance has the same bone count as the previous skeletal asset.
  Entry->Proxy->BoneSpaceTransforms.Reset();
  Entry->Proxy->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
  Entry->MappedPartAsset.Reset();
 }
 Entry->bSeen = true;
 UpdateMaterials(*Entry);
 return Entry;
}

void URogue10mAppearanceShadowComponent::UpdateMaterials(FRogue10mAppearanceShadowEntry& Entry)
{
 USkeletalMeshComponent* Source = Entry.Source.Get();
 UPoseableMeshComponent* Proxy = Entry.Proxy;
 // Clear overrides left by a previous appearance with more material slots.
 if (Proxy->GetNumMaterials() > Source->GetNumMaterials()) Proxy->EmptyOverrideMaterials();
 for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index)
 {
  if (Proxy->GetMaterial(Index) != Source->GetMaterial(Index))
   Proxy->SetMaterial(Index, Source->GetMaterial(Index));
 }
}

void URogue10mAppearanceShadowComponent::ActivateEntry(FRogue10mAppearanceShadowEntry& Entry)
{
 if (!Entry.bApplied)
 {
  Entry.bSavedCastShadow = Entry.Source->CastShadow;
  Entry.bApplied = true;
 }
 Entry.Source->SetCastShadow(false);
 Entry.Proxy->SetCastShadow(Entry.bSavedCastShadow);
}

void URogue10mAppearanceShadowComponent::RestoreEntry(FRogue10mAppearanceShadowEntry& Entry)
{
 if (Entry.bApplied)
 {
  if (USkeletalMeshComponent* Source = Entry.Source.Get()) Source->SetCastShadow(Entry.bSavedCastShadow);
  Entry.bApplied = false;
 }
 if (Entry.Proxy) Entry.Proxy->SetCastShadow(false);
}

void URogue10mAppearanceShadowComponent::UpdatePartPose(FRogue10mAppearanceShadowEntry& Entry, USkeletalMeshComponent& Body)
{
 USkeletalMesh* BodyAsset = Body.GetSkeletalMeshAsset();
 USkeletalMesh* PartAsset = Entry.Source->GetSkeletalMeshAsset();
 const FReferenceSkeleton& PartRef = PartAsset->GetRefSkeleton();
 if (Entry.MappedBodyAsset.Get() != BodyAsset || Entry.MappedPartAsset.Get() != PartAsset)
 {
  Entry.BodyBoneMap.SetNum(PartRef.GetNum());
  for (int32 Index = 0; Index < PartRef.GetNum(); ++Index)
   Entry.BodyBoneMap[Index] = BodyAsset->GetRefSkeleton().FindBoneIndex(PartRef.GetBoneName(Index));
  Entry.MappedBodyAsset = BodyAsset;
  Entry.MappedPartAsset = PartAsset;
 }
 TArray<FTransform>& LocalPose = Entry.Proxy->BoneSpaceTransforms;
 LocalPose.SetNum(PartRef.GetNum());
 Entry.PartComponentPose.SetNum(PartRef.GetNum());
 // Leader-pose rendering consumes the body's component-space pose in the part's
 // component frame. Rebuild locals through the part hierarchy, retaining unmapped
 // accessory bones in their own reference pose (no body proportions imposed there).
 for (int32 Index = 0; Index < PartRef.GetNum(); ++Index)
 {
  const int32 Parent = PartRef.GetParentIndex(Index);
  const int32 BodyIndex = Entry.BodyBoneMap[Index];
  if (BodyComponentPose.IsValidIndex(BodyIndex))
  {
   Entry.PartComponentPose[Index] = BodyComponentPose[BodyIndex];
   LocalPose[Index] = Parent == INDEX_NONE ? Entry.PartComponentPose[Index]
    : Entry.PartComponentPose[Index].GetRelativeTransform(Entry.PartComponentPose[Parent]);
  }
  else
  {
   LocalPose[Index] = PartRef.GetRefBonePose()[Index];
   Entry.PartComponentPose[Index] = Parent == INDEX_NONE ? LocalPose[Index]
    : LocalPose[Index] * Entry.PartComponentPose[Parent];
  }
  LocalPose[Index].NormalizeRotation();
 }
 Entry.Proxy->RefreshBoneTransforms();
 ActivateEntry(Entry);
}

void URogue10mAppearanceShadowComponent::RefreshShadow(USkeletalMeshComponent* Body, bool bEnable)
{
 if (!bEnable || !IsValid(Body) || !Body->GetSkeletalMeshAsset())
 {
  RestoreShadow();
  return;
 }
 if (AppliedBody.Get() != Body)
 {
  RestoreShadow();
  AppliedBody = Body;
 }
 const FReferenceSkeleton& BodyRef = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
 // This accessor completes pending evaluation. The local pose is unmodified by
 // HideBone's zero scale in the rendered component-space transforms.
 const auto RawPose = Body->GetBoneSpaceTransformsView();
 if (RawPose.Num() != BodyRef.GetNum() || RawPose.IsEmpty())
 {
  RestoreShadow();
  return;
 }
 for (const FTransform& Bone : RawPose)
 {
  if (Bone.ContainsNaN()) { RestoreShadow(); return; }
 }
 for (FRogue10mAppearanceShadowEntry& Entry : Entries) Entry.bSeen = false;
 FRogue10mAppearanceShadowEntry* BodyEntry = FindOrCreateEntry(Body);
 if (!BodyEntry) { RestoreShadow(); return; }
 BodyEntry->Proxy->BoneSpaceTransforms.Reset(RawPose.Num());
 BodyEntry->Proxy->BoneSpaceTransforms.Append(RawPose.GetData(), RawPose.Num());
 BodyEntry->Proxy->RefreshBoneTransforms();
 ActivateEntry(*BodyEntry);
 bActive = true;

 BodyComponentPose.SetNum(RawPose.Num());
 for (int32 Index = 0; Index < RawPose.Num(); ++Index)
 {
  const int32 Parent = BodyRef.GetParentIndex(Index);
  BodyComponentPose[Index] = Parent == INDEX_NONE ? RawPose[Index] : RawPose[Index] * BodyComponentPose[Parent];
 }
 // Snapshot children before proxy creation attaches more children to a source.
 TArray<USkeletalMeshComponent*, TInlineAllocator<4>> Parts;
 for (USceneComponent* Child : Body->GetAttachChildren())
 {
  USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Child);
  if (Part && Part->LeaderPoseComponent.Get() == Body && Part->GetSkeletalMeshAsset()
   && Part->IsVisible() && !Part->bHiddenInGame) Parts.Add(Part);
 }
 for (USkeletalMeshComponent* Part : Parts)
 {
  if (FRogue10mAppearanceShadowEntry* Entry = FindOrCreateEntry(Part)) UpdatePartPose(*Entry, *Body);
 }
 for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
 {
  FRogue10mAppearanceShadowEntry& Entry = Entries[Index];
  if (Entry.bSeen) continue;
  RestoreEntry(Entry);
  if (Entry.Proxy)
  {
   if (AActor* Owner = GetOwner()) Owner->RemoveInstanceComponent(Entry.Proxy);
   Entry.Proxy->DestroyComponent();
  }
  Entries.RemoveAtSwap(Index);
 }
}

void URogue10mAppearanceShadowComponent::RestoreShadow()
{
 for (FRogue10mAppearanceShadowEntry& Entry : Entries) RestoreEntry(Entry);
 bActive = false;
 AppliedBody.Reset();
}

UPoseableMeshComponent* URogue10mAppearanceShadowComponent::GetShadowBodyProxy() const
{
 return GetShadowProxyForSource(AppliedBody.Get());
}

UPoseableMeshComponent* URogue10mAppearanceShadowComponent::GetShadowProxyForSource(USkeletalMeshComponent* Source) const
{
 if (!Source) return nullptr;
 const FRogue10mAppearanceShadowEntry* Entry = Entries.FindByPredicate(
  [Source](const FRogue10mAppearanceShadowEntry& Item) { return Item.Source.Get() == Source; });
 return Entry ? Entry->Proxy.Get() : nullptr;
}

void URogue10mAppearanceShadowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 RestoreShadow();
 for (FRogue10mAppearanceShadowEntry& Entry : Entries)
 {
  if (Entry.Proxy)
  {
   if (AActor* Owner = GetOwner()) Owner->RemoveInstanceComponent(Entry.Proxy);
   Entry.Proxy->DestroyComponent();
  }
 }
 Entries.Reset();
 Super::EndPlay(EndPlayReason);
}
