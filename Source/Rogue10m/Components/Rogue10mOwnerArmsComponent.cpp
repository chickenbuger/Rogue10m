// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mOwnerArmsComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"
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

USkinnedMeshComponent* URogue10mOwnerArmsComponent::GetOwnerArmsMesh() const
{
 return bFramingActive && IsValid(FramedArmsMesh) ? static_cast<USkinnedMeshComponent*>(FramedArmsMesh.Get()) : OwnerArmsMesh.Get();
}

bool URogue10mOwnerArmsComponent::EnsureFramedProxy()
{
 if (IsValid(FramedArmsMesh)) { return true; }
 ACharacter* Character = Cast<ACharacter>(GetOwner());
 if (!Character || !Character->GetCapsuleComponent()) { return false; }
 FramedArmsMesh = NewObject<UPoseableMeshComponent>(Character, NAME_None, RF_Transient);
 UPoseableMeshComponent* Proxy = FramedArmsMesh.Get();
 Character->AddInstanceComponent(Proxy); Proxy->SetupAttachment(Character->GetCapsuleComponent());
 Proxy->PrimaryComponentTick.bCanEverTick = false; Proxy->SetIsReplicated(false);
 Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision); Proxy->SetGenerateOverlapEvents(false); Proxy->SetCanEverAffectNavigation(false);
 Proxy->SetOnlyOwnerSee(true); Proxy->SetOwnerNoSee(false); Proxy->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
 Proxy->SetCastShadow(false); Proxy->SetCastHiddenShadow(false); Proxy->SetRenderCustomDepth(false); Proxy->SetVisibleInRayTracing(false);
 Proxy->bVisibleInReflectionCaptures=false;Proxy->bVisibleInRealTimeSkyCaptures=false;Proxy->SetReceivesDecals(false);
 Proxy->SetVisibility(false,false);Proxy->SetHiddenInGame(true,false);Proxy->RegisterComponent();Proxy->SetComponentTickEnabled(false);
 return IsValid(Proxy);
}

void URogue10mOwnerArmsComponent::ShowLeaderFallback()
{
 bFramingActive=false;
 if(IsValid(FramedArmsMesh)){FramedArmsMesh->SetVisibility(false,false);FramedArmsMesh->SetHiddenInGame(true,false);}
 if(IsActive()&&IsValid(OwnerArmsMesh)){OwnerArmsMesh->SetVisibility(true,false);OwnerArmsMesh->SetHiddenInGame(false,false);}
}

void URogue10mOwnerArmsComponent::ResetFramingBasis(USkeletalMeshComponent& Body)
{
 FramingBasisBody=&Body;FramingBasisAsset=Body.GetSkeletalMeshAsset();bNeutralCaptured=false;IdleCaptureStart=-1.;
 bFramingChainValid=false;LastFramingRevision=MAX_uint32;
 const auto* Asset=Body.GetSkeletalMeshAsset();if(!Asset)return;
 const auto& Ref=Asset->GetRefSkeleton();TorsoBone=Ref.FindBoneIndex(TEXT("spine_03"));
 UpperBone[0]=Ref.FindBoneIndex(TEXT("upperarm_l"));UpperBone[1]=Ref.FindBoneIndex(TEXT("upperarm_r"));
 LowerBone[0]=Ref.FindBoneIndex(TEXT("lowerarm_l"));LowerBone[1]=Ref.FindBoneIndex(TEXT("lowerarm_r"));
 HandBone[0]=Ref.FindBoneIndex(TEXT("hand_l"));HandBone[1]=Ref.FindBoneIndex(TEXT("hand_r"));
 if(!Ref.IsValidIndex(TorsoBone))return;
 for(int32 I=0;I<2;++I)if(!Ref.IsValidIndex(UpperBone[I])||!Ref.IsValidIndex(LowerBone[I])||!Ref.IsValidIndex(HandBone[I])
  ||Ref.GetParentIndex(LowerBone[I])!=UpperBone[I]||Ref.GetParentIndex(HandBone[I])!=LowerBone[I])return;
 FTransform Torso=Ref.GetRefBonePose()[TorsoBone];
 for(int32 Parent=Ref.GetParentIndex(TorsoBone);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))Torso*=Ref.GetRefBonePose()[Parent];
 ReferenceTorsoInActor=(GetOwner()->GetActorQuat().Inverse()*Body.GetComponentQuat()*Torso.GetRotation()).GetNormalized();
 bFramingChainValid=!ReferenceTorsoInActor.ContainsNaN();
}

bool URogue10mOwnerArmsComponent::UpdatePresentation(const FTransform& View)
{
 auto* Body=AppliedBody.Get();auto* Character=Cast<ARogue10mCharacter>(GetOwner());
 if(!bEnableViewFraming||!IsActive()||!IsValid(Body)||!Character||!GetWorld()||View.ContainsNaN()) { ShowLeaderFallback();return false; }
 if(FramingBasisBody.Get()!=Body||FramingBasisAsset.Get()!=Body->GetSkeletalMeshAsset())ResetFramingBasis(*Body);
 if(!bFramingChainValid){WarnOnce(TEXT("framing_chain"),TEXT("owner-only arm IK requires complete direct upper/lower/hand chains"));ShowLeaderFallback();return false;}
 const FVector BodyScale=Body->GetComponentScale();
 if(BodyScale.ContainsNaN()||BodyScale.X<=UE_SMALL_NUMBER||!FMath::IsNearlyEqual(BodyScale.X,BodyScale.Y,.001)||!FMath::IsNearlyEqual(BodyScale.X,BodyScale.Z,.001))
 {WarnOnce(TEXT("framing_scale"),TEXT("owner-only arm IK requires a positive uniform component scale"));ShowLeaderFallback();return false;}
 const auto& Pose=Body->GetBoneSpaceTransforms();const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
 if(Pose.Num()!=Ref.GetNum()){ShowLeaderFallback();return false;}
 TArray<FTransform> ComponentPose;ComponentPose.SetNumUninitialized(Pose.Num());
 for(int32 I=0;I<Pose.Num();++I){if(Pose[I].ContainsNaN()){ShowLeaderFallback();return false;}const int32 Parent=Ref.GetParentIndex(I);ComponentPose[I]=Parent==INDEX_NONE?Pose[I]:Pose[I]*ComponentPose[Parent];}
 auto* Source=Character->GetAnimationPlaybackMesh();const auto* Anim=Source?Cast<URogue10mFighterCombatStanceAnimInstance>(Source->GetAnimInstance()):nullptr;
 const bool bDodgeAimSkipped=Anim&&Anim->IsViewAimDodgeActive();
 FQuat ResidualInComponent=FQuat::Identity;
 if(Anim&&Anim->bEnableViewAlignedUpperBody&&!bDodgeAimSkipped)
 {
  const FQuat Aim=(Character->GetActorQuat()*FRotator(Anim->UpperBodyAimPitch,Anim->UpperBodyAimYaw,0).Quaternion()*Character->GetActorQuat().Inverse()).GetNormalized();
  const FQuat ResidualWorld=FQuat::Slerp(FQuat::Identity,Aim,1.f-FMath::Clamp(Anim->UpperBodyAimTorsoWeight,0.f,1.f));
  ResidualInComponent=(Body->GetComponentQuat().Inverse()*ResidualWorld*Body->GetComponentQuat()).GetNormalized();
 }
 FVector HandInTorso[2];
 for(int32 I=0;I<2;++I)
 {
  const FVector Shoulder=ComponentPose[UpperBone[I]].GetLocation();
  const FVector Unaimed=Shoulder+ResidualInComponent.Inverse().RotateVector(ComponentPose[HandBone[I]].GetLocation()-Shoulder);
  HandInTorso[I]=ComponentPose[TorsoBone].InverseTransformPosition(Unaimed);
 }
 const auto* Combat=Character->GetCombatComponent();const auto* Movement=Character->GetCharacterMovement();
 const bool bIdle=Movement&&Movement->IsMovingOnGround()&&Movement->Velocity.SizeSquared()<1.f&&!Character->IsDead()&&!Character->IsDodging()&&!bDodgeAimSkipped
  &&Combat&&!Combat->IsWeaponChangeBlocked()&&!Combat->IsAttackChargeHeld()&&!Combat->IsComboSequenceActive(GetWorld()->GetTimeSeconds());
 if(!bNeutralCaptured)
 {
  if(!bIdle){IdleCaptureStart=-1.;ShowLeaderFallback();return false;}
  if(IdleCaptureStart<0.)IdleCaptureStart=GetWorld()->GetTimeSeconds();
  if(GetWorld()->GetTimeSeconds()-IdleCaptureStart<FMath::Clamp(NeutralCaptureDelaySeconds,.05f,1.f)){ShowLeaderFallback();return false;}
  NeutralHandInTorso[0]=HandInTorso[0];NeutralHandInTorso[1]=HandInTorso[1];bNeutralCaptured=true;
 }
 if(ShoulderAnchorView.ContainsNaN()||LeftReadyHandView.ContainsNaN()||RightReadyHandView.ContainsNaN()||LeftElbowHintView.ContainsNaN()||RightElbowHintView.ContainsNaN()||!FMath::IsFinite(AuthoredMotionScale)
  ||!FMath::IsFinite(HandGoalDepthRangeCm.X)||!FMath::IsFinite(HandGoalDepthRangeCm.Y)
  ||!FMath::IsFinite(HandGoalSideRangeCm.X)||!FMath::IsFinite(HandGoalSideRangeCm.Y)
  ||!FMath::IsFinite(HandGoalHeightRangeCm.X)||!FMath::IsFinite(HandGoalHeightRangeCm.Y))
 {ShowLeaderFallback();return false;}
 const FVector Anchor=ShoulderAnchorView.GetClampedToMaxSize(120.);const FVector Ready[2]={LeftReadyHandView.GetClampedToMaxSize(150.),RightReadyHandView.GetClampedToMaxSize(150.)};
 const FVector Elbow[2]={LeftElbowHintView.GetClampedToMaxSize(150.),RightElbowHintView.GetClampedToMaxSize(150.)};const float MotionScale=FMath::Clamp(AuthoredMotionScale,0.f,1.f);
 const auto SortedGoalRange=[](const FVector2D& Range,double Minimum,double Maximum)
 {return FVector2D(FMath::Clamp(FMath::Min(Range.X,Range.Y),Minimum,Maximum),FMath::Clamp(FMath::Max(Range.X,Range.Y),Minimum,Maximum));};
 const FVector2D GoalDepthRange=SortedGoalRange(HandGoalDepthRangeCm,10.,150.);
 const FVector2D GoalSideRange=SortedGoalRange(HandGoalSideRangeCm,0.,80.);
 const FVector2D GoalHeightRange=SortedGoalRange(HandGoalHeightRangeCm,-100.,0.);
 // Material overrides can change while the body pose/view are cached (for example in a paused appearance UI).
 // Keep their synchronization independent of pose copying/IK/render-buffer refresh.
 if(!EnsureFramedProxy()){ShowLeaderFallback();return false;}
 auto* Proxy=FramedArmsMesh.Get();
 if(Proxy->GetSkinnedAsset()!=ResolvedArmsAsset.Get()){Proxy->SetSkinnedAssetAndUpdate(ResolvedArmsAsset.Get());Proxy->EmptyOverrideMaterials();LastFramingRevision=MAX_uint32;}
 if(Proxy->GetNumMaterials()>Body->GetNumMaterials())Proxy->EmptyOverrideMaterials();
 for(int32 I=0;I<Body->GetNumMaterials();++I)if(Proxy->GetMaterial(I)!=Body->GetMaterial(I))Proxy->SetMaterial(I,Body->GetMaterial(I));
 const uint32 Revision=Body->GetBoneTransformRevisionNumber();
 if(bFramingActive&&Revision==LastFramingRevision&&View.Equals(LastFramingView,.000001)&&Body->GetComponentTransform().Equals(LastFramingBody,.000001)
  &&Anchor==LastShoulderAnchor&&Ready[0]==LastReady[0]&&Ready[1]==LastReady[1]&&Elbow[0]==LastElbow[0]&&Elbow[1]==LastElbow[1]&&MotionScale==LastMotionScale
  &&GoalDepthRange==LastGoalDepthRange&&GoalSideRange==LastGoalSideRange&&GoalHeightRange==LastGoalHeightRange)return true;
 if(Proxy->BoneSpaceTransforms.Num()!=Pose.Num()){ShowLeaderFallback();return false;}
 Proxy->BoneSpaceTransforms=Pose;
 const FQuat RigRotation=(View.GetRotation()*ReferenceTorsoInActor*ComponentPose[TorsoBone].GetRotation().Inverse()).GetNormalized();
 const FVector ShoulderCenter=(ComponentPose[UpperBone[0]].GetLocation()+ComponentPose[UpperBone[1]].GetLocation())*.5;
 const FVector RigLocation=View.TransformPosition(Anchor)-RigRotation.RotateVector(ShoulderCenter*BodyScale.X);
 const FTransform Rig(RigRotation,RigLocation,BodyScale);if(Rig.ContainsNaN()){ShowLeaderFallback();return false;}
 Proxy->SetWorldTransform(Rig,false,nullptr,ETeleportType::TeleportPhysics);
 for(int32 I=0;I<2;++I)
 {
  FTransform Upper=ComponentPose[UpperBone[I]],Lower=ComponentPose[LowerBone[I]],Hand=ComponentPose[HandBone[I]];
  const FVector Delta=ReferenceTorsoInActor.RotateVector(HandInTorso[I]-NeutralHandInTorso[I])*BodyScale.X*MotionScale;
  // Retain authored displacement inside the viewing envelope. Clamp goals, never source bones or the animation clock.
  FVector HandGoalView=Ready[I]+Delta;
  HandGoalView.X=FMath::Clamp(HandGoalView.X,GoalDepthRange.X,GoalDepthRange.Y);
  const double SideSign=I==0?-1.:1.;
  HandGoalView.Y=SideSign*FMath::Clamp(SideSign*HandGoalView.Y,GoalSideRange.X,GoalSideRange.Y);
  HandGoalView.Z=FMath::Clamp(HandGoalView.Z,GoalHeightRange.X,GoalHeightRange.Y);
  const FVector Effector=Rig.InverseTransformPosition(View.TransformPosition(HandGoalView));
  const FVector Hint=Rig.InverseTransformPosition(View.TransformPosition(Elbow[I]));
  const double UpperLength=FVector::Dist(Upper.GetLocation(),Lower.GetLocation()),LowerLength=FVector::Dist(Lower.GetLocation(),Hand.GetLocation());
  if(!FMath::IsFinite(UpperLength)||!FMath::IsFinite(LowerLength)||UpperLength<.01||LowerLength<.01){ShowLeaderFallback();return false;}
  const FVector GoalDelta=Effector-Upper.GetLocation();const double Maximum=.98*(UpperLength+LowerLength),Minimum=FMath::Abs(UpperLength-LowerLength)+.001;
  const FVector Goal=Upper.GetLocation()+GoalDelta.GetSafeNormal(UE_SMALL_NUMBER,FVector::ForwardVector)*FMath::Clamp(GoalDelta.Size(),Minimum,Maximum);
  AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Hint,Goal,false,1.,1.);
  const int32 Parent=Ref.GetParentIndex(UpperBone[I]);
  const FTransform UpperLocal=Parent==INDEX_NONE?Upper:Upper.GetRelativeTransform(ComponentPose[Parent]);
  const FTransform LowerLocal=Lower.GetRelativeTransform(Upper);
  Proxy->BoneSpaceTransforms[UpperBone[I]].SetRotation(UpperLocal.GetRotation().GetNormalized());
  Proxy->BoneSpaceTransforms[LowerBone[I]].SetRotation(LowerLocal.GetRotation().GetNormalized());
  // All translations/scales, the hand local pose and every finger remain copied from the evaluated source.
 }
 Proxy->MarkRefreshTransformDirty();Proxy->RefreshBoneTransforms();
 Proxy->SetVisibility(true,false);Proxy->SetHiddenInGame(false,false);OwnerArmsMesh->SetVisibility(false,false);OwnerArmsMesh->SetHiddenInGame(true,false);
 bFramingActive=true;LastFramingRevision=Revision;LastFramingView=View;LastFramingBody=Body->GetComponentTransform();LastShoulderAnchor=Anchor;
 for(int32 I=0;I<2;++I){LastReady[I]=Ready[I];LastElbow[I]=Elbow[I];}LastMotionScale=MotionScale;
 LastGoalDepthRange=GoalDepthRange;LastGoalSideRange=GoalSideRange;LastGoalHeightRange=GoalHeightRange;
 return true;
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
		ShowLeaderFallback();
		Proxy->SetLeaderPoseComponent(Body, true, false);
		AppliedBody = Body;
	}
	// Share the evaluated appearance pose in exactly its world basis. No additional
	// aim, offsets, montage, animation instance, hit pulse or notify runs here.
	Proxy->SetWorldTransform(Body->GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	SyncMaterials(*Body);
	Proxy->SetOnlyOwnerSee(true);
	Proxy->SetOwnerNoSee(false);
	Proxy->SetVisibility(!bFramingActive, false);
	Proxy->SetHiddenInGame(bFramingActive, false);
	if (!bEnableViewFraming) { ShowLeaderFallback(); }
	if (!IsActive()) { Super::Activate(false); }
	return IsOwnerArmsActive();
}

void URogue10mOwnerArmsComponent::Deactivate()
{
	bFramingActive=false;LastFramingRevision=MAX_uint32;
	if(IsValid(FramedArmsMesh)){FramedArmsMesh->SetVisibility(false,false);FramedArmsMesh->SetHiddenInGame(true,false);}
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
	if(IsValid(FramedArmsMesh)){if(AActor* Owner=GetOwner())Owner->RemoveInstanceComponent(FramedArmsMesh);FramedArmsMesh->DestroyComponent();}
	FramedArmsMesh=nullptr;
	OwnerArmsMesh = nullptr;
	ResolvedBodyAsset.Reset();
	ResolvedArmsAsset.Reset();
	Super::EndPlay(EndPlayReason);
}
