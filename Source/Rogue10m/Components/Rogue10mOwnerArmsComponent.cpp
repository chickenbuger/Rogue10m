// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mOwnerArmsComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Rogue10m.h"

URogue10mOwnerArmsComponent::URogue10mOwnerArmsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
	SetIsReplicatedByDefault(false);

	struct FRacePath { const TCHAR* Directory; const TCHAR* Code; };
	struct FGenderPath { const TCHAR* Directory; const TCHAR* Code; };
	for (const FRacePath Race : {FRacePath{TEXT("Human"), TEXT("Hu")},
		FRacePath{TEXT("Dwarf"), TEXT("Dw")}, FRacePath{TEXT("Orc"), TEXT("Or")}})
	{
		for (const FGenderPath Gender : {FGenderPath{TEXT("Male"), TEXT("M")},
			FGenderPath{TEXT("Female"), TEXT("F")}})
		{
			const FString BodyName = FString::Printf(TEXT("SK_%s_%s_FullBody"), Race.Code, Gender.Code);
			const FString ArmsName = FString::Printf(TEXT("SK_OwnerArms_%s_%s"), Race.Code, Gender.Code);
			auto& Variant = AssetVariants.AddDefaulted_GetRef();
			Variant.BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(FString::Printf(
				TEXT("/Game/StylizedCharacter/Meshes/Character/%s/%s/%s.%s"),
				Race.Directory, Gender.Directory, *BodyName, *BodyName)));
			Variant.ArmsMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(FString::Printf(
				TEXT("/Game/Rogue10m/Character/OwnerArms/%s.%s"), *ArmsName, *ArmsName)));
		}
	}
}

void URogue10mOwnerArmsComponent::WarnOnce(const FString& Key, const FString& Reason)
{
	if (!ReportedWarnings.Contains(Key))
	{
		ReportedWarnings.Add(Key);
		UE_LOG(LogRogue10m, Warning, TEXT("Owner arms unavailable: %s"), *Reason);
	}
}

bool URogue10mOwnerArmsComponent::HasMatchingReferencePose(
	const USkeletalMesh& BodyAsset, const USkeletalMesh& ArmsAsset) const
{
	if (&BodyAsset == &ArmsAsset || !BodyAsset.GetSkeleton()
		|| BodyAsset.GetSkeleton() != ArmsAsset.GetSkeleton()) { return false; }
	const FReferenceSkeleton& BodyRef = BodyAsset.GetRefSkeleton();
	const FReferenceSkeleton& ArmsRef = ArmsAsset.GetRefSkeleton();
	if (BodyRef.GetNum() == 0 || BodyRef.GetNum() != ArmsRef.GetNum()) { return false; }
	for (int32 Index = 0; Index < BodyRef.GetNum(); ++Index)
	{
		if (BodyRef.GetBoneName(Index) != ArmsRef.GetBoneName(Index)
			|| BodyRef.GetParentIndex(Index) != ArmsRef.GetParentIndex(Index)
			|| BodyRef.GetRefBonePose()[Index].ContainsNaN() || ArmsRef.GetRefBonePose()[Index].ContainsNaN()
			|| !BodyRef.GetRefBonePose()[Index].Equals(ArmsRef.GetRefBonePose()[Index])) { return false; }
	}
	return BodyAsset.GetMaterials().Num() == ArmsAsset.GetMaterials().Num();
}

USkeletalMesh* URogue10mOwnerArmsComponent::ResolveArmsMesh(USkeletalMesh& BodyAsset)
{
	const FSoftObjectPath BodyPath(&BodyAsset);
	const FRogue10mOwnerArmsVariant* Variant = AssetVariants.FindByPredicate(
		[&BodyPath](const FRogue10mOwnerArmsVariant& Item)
		{ return Item.BodyMesh.ToSoftObjectPath() == BodyPath; });
	if (!Variant || Variant->ArmsMesh.IsNull())
	{
		WarnOnce(BodyPath.ToString(), FString::Printf(TEXT("no variant for %s"), *BodyPath.ToString()));
		return nullptr;
	}
	const FSoftObjectPath ArmsPath = Variant->ArmsMesh.ToSoftObjectPath();
	if (bResolutionAttempted && ResolvedBodyAsset.Get() == &BodyAsset && ResolvedArmsPath == ArmsPath)
	{
		return ResolvedArmsAsset.Get();
	}
	// Resolve and validate once per asset pair, rather than synchronously loading or
	// comparing an entire reference skeleton on each camera/trace query.
	bResolutionAttempted = true;
	ResolvedBodyAsset = &BodyAsset;
	ResolvedArmsPath = ArmsPath;
	ResolvedArmsAsset.Reset();
	USkeletalMesh* ArmsAsset = Variant->ArmsMesh.LoadSynchronous();
	if (!ArmsAsset || !HasMatchingReferencePose(BodyAsset, *ArmsAsset))
	{
		WarnOnce(BodyPath.ToString() + TEXT("->") + ArmsPath.ToString(),
			FString::Printf(TEXT("missing or incompatible derivative %s for %s"),
				*ArmsPath.ToString(), *BodyPath.ToString()));
		return nullptr;
	}
	ResolvedArmsAsset = ArmsAsset;
	return ArmsAsset;
}

bool URogue10mOwnerArmsComponent::EnsureProxy()
{
	if (IsValid(OwnerArmsMesh)) { return true; }
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetCapsuleComponent()) { return false; }
	OwnerArmsMesh = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
	USkeletalMeshComponent* Proxy = OwnerArmsMesh.Get();
	Character->AddInstanceComponent(Proxy);
	// Do not join the body's direct cosmetic children: inspection code enumerates
	// those parts, and hiding the world body must not recursively hide these arms.
	Proxy->SetupAttachment(Character->GetCapsuleComponent());
	Proxy->PrimaryComponentTick.bCanEverTick = false;
	Proxy->SetIsReplicated(false);
	Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Proxy->SetGenerateOverlapEvents(false);
	Proxy->SetCanEverAffectNavigation(false);
	Proxy->SetOnlyOwnerSee(true);
	Proxy->SetOwnerNoSee(false);
	Proxy->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
	Proxy->SetCastShadow(false);
	Proxy->SetCastHiddenShadow(false);
	Proxy->SetRenderCustomDepth(false);
	Proxy->SetVisibleInRayTracing(false);
	Proxy->bVisibleInReflectionCaptures = false;
	Proxy->bVisibleInRealTimeSkyCaptures = false;
	Proxy->SetReceivesDecals(false);
	Proxy->bUseBoundsFromLeaderPoseComponent = true;
	Proxy->SetVisibility(false, false);
	Proxy->SetHiddenInGame(true, false);
	Proxy->RegisterComponent();
	Proxy->SetComponentTickEnabled(false);
	return IsValid(Proxy);
}

void URogue10mOwnerArmsComponent::SyncMaterials(const USkeletalMeshComponent& Body)
{
	USkeletalMeshComponent* Proxy = OwnerArmsMesh.Get();
	if (Proxy->GetNumMaterials() > Body.GetNumMaterials()) { Proxy->EmptyOverrideMaterials(); }
	for (int32 Index = 0; Index < Body.GetNumMaterials(); ++Index)
	{
		if (Proxy->GetMaterial(Index) != Body.GetMaterial(Index))
		{
			Proxy->SetMaterial(Index, Body.GetMaterial(Index));
		}
	}
}

bool URogue10mOwnerArmsComponent::Refresh(USkeletalMeshComponent* Body, bool bEnable)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMesh* BodyAsset = IsValid(Body) ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!bEnable || !Character || !Character->IsLocallyControlled() || !BodyAsset
		|| Body->GetOwner() != Character || Body->GetComponentTransform().ContainsNaN())
	{
		Deactivate();
		return false;
	}
	USkeletalMesh* ArmsAsset = ResolveArmsMesh(*BodyAsset);
	if (!ArmsAsset || !EnsureProxy())
	{
		Deactivate();
		return false;
	}
	USkeletalMeshComponent* Proxy = OwnerArmsMesh.Get();
	const bool bChangedAsset = Proxy->GetSkeletalMeshAsset() != ArmsAsset;
	if (bChangedAsset)
	{
		Proxy->SetLeaderPoseComponent(nullptr, true, false);
		Proxy->SetSkeletalMeshAsset(ArmsAsset);
		Proxy->EmptyOverrideMaterials();
	}
	if (bChangedAsset || AppliedBody.Get() != Body)
	{
		Proxy->SetLeaderPoseComponent(Body, true, false);
		AppliedBody = Body;
	}
	// Share the evaluated appearance pose in exactly its world basis. No additional
	// aim, offsets, montage, animation instance, hit pulse or notify runs here.
	Proxy->SetWorldTransform(Body->GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	SyncMaterials(*Body);
	Proxy->SetOnlyOwnerSee(true);
	Proxy->SetOwnerNoSee(false);
	Proxy->SetVisibility(true, false);
	Proxy->SetHiddenInGame(false, false);
	if (!IsActive()) { Super::Activate(false); }
	return IsOwnerArmsActive();
}

void URogue10mOwnerArmsComponent::Deactivate()
{
	if (IsValid(OwnerArmsMesh))
	{
		OwnerArmsMesh->SetVisibility(false, false);
		OwnerArmsMesh->SetHiddenInGame(true, false);
		OwnerArmsMesh->SetLeaderPoseComponent(nullptr, true, false);
	}
	AppliedBody.Reset();
	if (IsActive()) { Super::Deactivate(); }
}

void URogue10mOwnerArmsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();
	if (IsValid(OwnerArmsMesh))
	{
		if (AActor* Owner = GetOwner()) { Owner->RemoveInstanceComponent(OwnerArmsMesh); }
		OwnerArmsMesh->DestroyComponent();
	}
	OwnerArmsMesh = nullptr;
	ResolvedBodyAsset.Reset();
	ResolvedArmsAsset.Reset();
	Super::EndPlay(EndPlayReason);
}
