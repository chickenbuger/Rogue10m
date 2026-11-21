// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mWeaponRetargetAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mFighterCombatStanceAnimInstance.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "TwoBoneIK.h"

namespace
{
FVector ReferenceGripMarker(const UAnimSequence& Sequence, const FReferenceSkeleton& Reference, FName Name, float Time)
{
	FTransform Transform = FTransform::Identity;
	const FAnimExtractContext Context(Time, false);
	for (int32 Index = Reference.FindBoneIndex(Name); Index != INDEX_NONE; Index = Reference.GetParentIndex(Index))
	{
		FTransform Local = Reference.GetRefBonePose()[Index];
		Sequence.GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), Context, false);
		Transform *= Local;
	}
	return Transform.GetLocation();
}

FCompactPoseBoneIndex CompactBone(const FBoneContainer& Bones, FName Name)
{
	return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(Name)));
}

double LowestBladeProjection(const FBox& Bounds, const FQuat& Rotation, const FVector& Scale, const FVector& Axis)
{
	const FVector LocalAxis = Rotation.UnrotateVector(Axis) * Scale;
	return FVector::DotProduct(Bounds.GetCenter(), LocalAxis) - FVector::DotProduct(Bounds.GetExtent(), LocalAxis.GetAbs());
}

struct FGripArm
{
	FCompactPoseBoneIndex Shoulder = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Elbow = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Wrist = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Middle = FCompactPoseBoneIndex(INDEX_NONE);

	bool IsValid() const { return Shoulder != INDEX_NONE && Elbow != INDEX_NONE && Wrist != INDEX_NONE && Middle != INDEX_NONE; }
	FVector Palm(FCSPose<FCompactPose>& Pose) const
	{
		return (Pose.GetComponentSpaceTransform(Wrist).GetLocation() + Pose.GetComponentSpaceTransform(Middle).GetLocation()) * .5;
	}

	bool Solve(FCSPose<FCompactPose>& Pose, const FVector& PalmTarget, const FVector& GripOffset, double Reserve) const
	{
		FTransform Upper = Pose.GetComponentSpaceTransform(Shoulder);
		FTransform Lower = Pose.GetComponentSpaceTransform(Elbow);
		FTransform Hand = Pose.GetComponentSpaceTransform(Wrist);
		const FVector Root = Upper.GetLocation();
		const FVector PalmOffset = Palm(Pose) - Hand.GetLocation();
		const FQuat WristTurn = FQuat::FindBetweenNormals(PalmOffset.GetSafeNormal(), GripOffset.GetSafeNormal());
		const FQuat WristRotation = WristTurn * Hand.GetRotation();
		const FVector Target = PalmTarget - GripOffset;
		if (Target.ContainsNaN()) { return false; }
		const double UpperLength = FVector::Distance(Root, Lower.GetLocation());
		const double LowerLength = FVector::Distance(Lower.GetLocation(), Hand.GetLocation());
		const double Reach = FVector::Distance(Root, Target);
		if (Reach < FMath::Abs(UpperLength - LowerLength) + Reserve - .005
			|| Reach > UpperLength + LowerLength - Reserve + .005) { return false; }
		const FVector Axis = (Hand.GetLocation() - Root).GetSafeNormal();
		const FVector TargetAxis = (Target - Root).GetSafeNormal();
		const FVector ElbowPole = FVector::VectorPlaneProject(Lower.GetLocation() - Root, Axis);
		if (Axis.IsNearlyZero() || TargetAxis.IsNearlyZero() || ElbowPole.IsNearlyZero()) { return false; }
		const FVector Pole = Root + FQuat::FindBetweenNormals(Axis, TargetAxis).RotateVector(ElbowPole);
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1.0, 1.0);
		Hand.SetRotation(WristRotation);
		if (Upper.ContainsNaN() || Lower.ContainsNaN() || Hand.ContainsNaN()) { return false; }
		Upper.NormalizeRotation();
		Lower.NormalizeRotation();
		Hand.NormalizeRotation();
		const FBoneTransform Adjustments[] = { {Shoulder, Upper}, {Elbow, Lower}, {Wrist, Hand} };
		Pose.LocalBlendCSBoneTransforms(Adjustments, 1.0f);
		return true;
	}
};

class FWeaponRetargetProxy final : public FAnimInstanceProxy
{
public:
	explicit FWeaponRetargetProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	void SetPreviousPose(TArray<FTransform>&& Pose, TBitArray<>&& Bones, float Seconds)
	{
		PreviousLocalPose = MoveTemp(Pose);
		PreviousBoneMask = MoveTemp(Bones);
		Duration = Seconds;
		Elapsed = 0.0f;
		OldLegWeight = 1.0f;
	}

	void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto* Character = Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner());
		const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		CaptureRollFloor(Instance, Character);
		if (PreviousLocalPose.IsEmpty()) { return; }
		Elapsed += FMath::Max(0.0f, DeltaSeconds);
		if (Elapsed >= Duration || !Movement || !Movement->IsMovingOnGround() || Character->IsDodging() || Character->IsDead())
		{
			PreviousLocalPose.Reset();
			return;
		}
		// The first evaluation can happen before the new graph has live foot motion.
		// Preserve the last complete pose and ease the legs back into the live stride.
		const float StanceWeight = FMath::GetMappedRangeValueClamped(FVector2f(10.0f, 120.0f), FVector2f(1.0f, 0.0f), Movement->Velocity.Size2D());
		const float LegBlend = 1.0f - FMath::Exp(-Elapsed / FMath::Min(Duration, 0.055f));
		OldLegWeight = FMath::Lerp(1.0f, StanceWeight, LegBlend);
	}

	void PreEvaluateAnimation(UAnimInstance* Instance) override
	{
		FAnimInstanceProxy::PreEvaluateAnimation(Instance);
		DwarfGripWeight = 0.0f;
		FrontWall = FPlane(ForceInit);
		const auto* Character = Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner());
		if (!Character || Character->GetMesh() != Instance->GetSkelMeshComponent() || Character->IsDodging() || Character->IsDead()) { return; }
		const auto* SourceMesh = Character->GetAnimationPlaybackMesh();
		const auto* Source = SourceMesh ? Cast<URogue10mFighterCombatStanceAnimInstance>(SourceMesh->GetAnimInstance()) : nullptr;
		if (!Source) { return; }
		const FRogue10mDwarfEarthSplitterSample Recovery = Source->GetEarthSplitterWallRecoverySample();
		const bool bWallRecovery = Recovery.Weight > UE_SMALL_NUMBER;
		if (!bWallRecovery && !URogue10mWeaponRetargetAnimInstance::UsesVisibleEarthSplitter(*Character)) { return; }
		const FRogue10mDwarfEarthSplitterSample Sample = bWallRecovery ? Recovery : Source->GetDwarfEarthSplitterSample();
		if (Sample.Weight <= UE_SMALL_NUMBER) { return; }
		const auto* Clip = Source->GetEarthSplitterReference(Sample.bRelease);
		const auto* Visual = Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
		const auto* SwordComponent = Visual ? Visual->GetRightWeaponMesh() : nullptr;
		const UStaticMesh* Sword = SwordComponent ? SwordComponent->GetStaticMesh() : nullptr;
		const auto* Movement = Character->GetCharacterMovement();
		if (!Clip || !Clip->GetSkeleton() || !Sword || !Movement || !Movement->IsMovingOnGround()) { return; }
		const auto* Capsule = Character->GetCapsuleComponent();
		DwarfGripFloor = FMath::Max(Movement->CurrentFloor.HitResult.ImpactPoint.Z,
			Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight());
		DwarfGripBounds = Sword->GetBoundingBox();
		DwarfGripMeshWorld = Character->GetMesh()->GetComponentTransform();
		if (DwarfGripMeshWorld.GetScale3D().X <= UE_SMALL_NUMBER
			|| !DwarfGripMeshWorld.GetScale3D().AllComponentsEqual(UE_KINDA_SMALL_NUMBER)) { return; }
		DwarfGripSwordScale = SwordComponent->GetComponentScale();
		DwarfGripForward = Character->GetActorForwardVector();
		DwarfGripRight = Character->GetActorRightVector();
		FrontWall = Sample.FrontWall;
		const FVector WallNormal(FrontWall.X, FrontWall.Y, FrontWall.Z);
		if (!WallNormal.IsNearlyZero())
		{
			// Use the same compact guard before charging and when the attack finishes.
			// The raised hilt folds back towards the chest after the damage frame.
			const float Raised = Sample.bRelease
				? 1.0f - FMath::SmoothStep(.78f, 1.51f, Sample.Time)
				: FMath::SmoothStep(0.0f, .65f, Sample.Time);
			const float Retract = Sample.bRelease
				? FMath::SmoothStep(.10f, .60f, Sample.Time) * (1.0f - FMath::SmoothStep(.91f, 1.52f, Sample.Time))
				: 0.0f;
			FVector AlongWall = FVector::CrossProduct(WallNormal, FVector::UpVector).GetSafeNormal();
			if (FVector::DotProduct(AlongWall, DwarfGripRight) < 0.0) { AlongWall *= -1.0; }
			const FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, WallNormal).GetSafeNormal();
			const double Lean = FMath::DegreesToRadians(4.0 + 10.0 * Raised - 6.0 * Retract);
			const FVector BladeAxis = WallUp * FMath::Cos(Lean) + AlongWall * FMath::Sin(Lean);
			const FVector PalmFacing = FVector::VectorPlaneProject(DwarfGripForward, BladeAxis).GetSafeNormal();
			DwarfGripAuthoredRotation = FRotationMatrix::MakeFromZY(BladeAxis, PalmFacing).ToQuat();
			DwarfGripAuthoredFingers[0] = FVector::RightVector;
			DwarfGripAuthoredFingers[1] = FVector(-.7, .7, 0.0);
			DwarfGripSpan = 20.0;
			FrontHiltOffset = FVector(10.0 + 6.0 * Raised - 11.0 * Retract,
				5.0 + 7.0 * Raised + 4.0 * Retract, -13.0 + 8.0 * Raised - 7.0 * Retract);
		}
		else
		{
			const auto& Reference = Clip->GetSkeleton()->GetReferenceSkeleton();
			const auto MarkerWorld = [&](FName Bone)
			{
				return SourceMesh->GetComponentTransform().TransformPosition(ReferenceGripMarker(*Clip, Reference, Bone, Sample.Time));
			};
			const FVector RightWrist = MarkerWorld(TEXT("hand_r"));
			const FVector LeftWrist = MarkerWorld(TEXT("hand_l"));
			const FVector RightFinger = MarkerWorld(TEXT("middle_01_r")) - RightWrist;
			const FVector LeftFinger = MarkerWorld(TEXT("middle_01_l")) - LeftWrist;
			const FVector Right = RightWrist + RightFinger * .5;
			const FVector Left = LeftWrist + LeftFinger * .5;
			FTransform AuthoredGrip;
			if (!URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Right, Left, RightFinger, AuthoredGrip)) { return; }
			DwarfGripAuthoredRotation = AuthoredGrip.GetRotation();
			DwarfGripAuthoredFingers[0] = DwarfGripAuthoredRotation.UnrotateVector(RightFinger);
			DwarfGripAuthoredFingers[1] = DwarfGripAuthoredRotation.UnrotateVector(LeftFinger);
			DwarfGripSpan = FMath::Clamp((Right - Left).Size() * .82, 17.0, 22.0);
			DwarfGripHeight = Right.Z + 6.0;
		}
		DwarfGripWeight = Sample.Weight;
		if (Sample.bRelease && !bWallRecovery)
		{
			DwarfGripWeight *= 1.0f - FMath::SmoothStep(.15f, .73f, Sample.Time);
		}
	}

	bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* Root) override
	{
		if (!Root || Root != GetRootNode()) { return false; }
		EvaluateAnimationNode_WithRoot(Output, Root);
		SolveRollPalmSupport(Output.Pose);
		if (PreviousLocalPose.IsEmpty())
		{
			ApplyDwarfEarthGrip(Output);
			return true;
		}

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const auto Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(TEXT("spine_01"))));
		const float Weight = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Elapsed / Duration);
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
			if (!PreviousBoneMask.IsValidIndex(MeshIndex) || !PreviousBoneMask[MeshIndex]) { continue; }
			const bool bUpperBody = Spine != INDEX_NONE && (Bone == Spine || Bones.BoneIsChildOf(Bone, Spine));
			Output.Pose[Bone].BlendWith(PreviousLocalPose[MeshIndex], Weight * (bUpperBody ? 1.0f : OldLegWeight));
		}
		return true;
	}

private:
	void ApplyDwarfEarthGrip(FPoseContext& Output) const
	{
		if (DwarfGripWeight <= UE_SMALL_NUMBER) { return; }
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FGripArm Right {CompactBone(Bones, TEXT("upperarm_r")), CompactBone(Bones, TEXT("lowerarm_r")),
			CompactBone(Bones, TEXT("hand_r")), CompactBone(Bones, TEXT("middle_01_r"))};
		const FGripArm Left {CompactBone(Bones, TEXT("upperarm_l")), CompactBone(Bones, TEXT("lowerarm_l")),
			CompactBone(Bones, TEXT("hand_l")), CompactBone(Bones, TEXT("middle_01_l"))};
		if (!Right.IsValid() || !Left.IsValid()) { return; }
		FCSPose<FCompactPose> Corrected;
		Corrected.InitPose(Output.Pose);
		const auto WorldJoint = [&](FCompactPoseBoneIndex Bone)
		{
			return DwarfGripMeshWorld.TransformPosition(Corrected.GetComponentSpaceTransform(Bone).GetLocation());
		};
		const FGripArm Arms[] = {Right, Left};
		const FVector StockPalms[] = {DwarfGripMeshWorld.TransformPosition(Right.Palm(Corrected)), DwarfGripMeshWorld.TransformPosition(Left.Palm(Corrected))};
		const FVector StockFingerOffsets[] = {StockPalms[0] - WorldJoint(Right.Wrist), StockPalms[1] - WorldJoint(Left.Wrist)};
		FTransform StockGrip;
		if (!URogue10mWeaponVisualComponent::BuildSwordPalmGrip(StockPalms[0], StockPalms[1], StockFingerOffsets[0], StockGrip)) { return; }
		const FVector StockAxis = StockGrip.GetUnitAxis(EAxis::Z);
		const FVector AuthoredAxis = DwarfGripAuthoredRotation.GetAxisZ();
		const double Alpha = DwarfGripWeight;
		const double StockYaw = FMath::Atan2(StockAxis.Y, StockAxis.X);
		const double AuthoredYaw = FMath::Atan2(AuthoredAxis.Y, AuthoredAxis.X);
		const double Yaw = StockYaw + Alpha * FMath::FindDeltaAngleRadians(StockYaw, AuthoredYaw);
		const double Elevation = FMath::Lerp(FMath::Asin(StockAxis.Z), FMath::Asin(AuthoredAxis.Z), Alpha);
		const FVector Axis(FMath::Cos(Elevation) * FMath::Cos(Yaw), FMath::Cos(Elevation) * FMath::Sin(Yaw), FMath::Sin(Elevation));
		const FVector RollReference = FQuat::Slerp(StockGrip.GetRotation(), DwarfGripAuthoredRotation, Alpha).GetAxisY();
		const FVector GripY = FVector::VectorPlaneProject(RollReference, Axis).GetSafeNormal();
		if (GripY.IsNearlyZero()) { return; }
		const FQuat Rotation = FRotationMatrix::MakeFromZY(Axis, GripY).ToQuat();
		const double Span = FMath::Lerp(FVector::Distance(StockPalms[0], StockPalms[1]), DwarfGripSpan, Alpha);
		const FVector PalmOffsets[] = {Axis * (Span * .5), Axis * (-Span * .5)};
		const FVector Shoulders[] = {WorldJoint(Right.Shoulder), WorldJoint(Left.Shoulder)};
		FVector AuthoredRight = (Shoulders[0] + Shoulders[1]) * .5 + DwarfGripForward * 22.0 + DwarfGripRight * 8.0;
		AuthoredRight.Z = DwarfGripHeight;
		const FVector WallNormal(FrontWall.X, FrontWall.Y, FrontWall.Z);
		const bool bHasFrontWall = !WallNormal.IsNearlyZero();
		if (bHasFrontWall)
		{
			AuthoredRight = (Shoulders[0] + Shoulders[1]) * .5 + DwarfGripForward * FrontHiltOffset.X
				+ DwarfGripRight * FrontHiltOffset.Y + FVector::UpVector * FrontHiltOffset.Z;
		}
		FVector Center = FMath::Lerp((StockPalms[0] + StockPalms[1]) * .5, AuthoredRight - AuthoredAxis * (DwarfGripSpan * .5), Alpha);
		FVector FingerOffsets[2], SphereCenters[2];
		double SphereRadii[2];
		// Preserve the finger-facing motion in grip space while moving to the verified right/left hold angles.
		const double FingerAxial[] = {.17, .30};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector Begin = StockGrip.InverseTransformVectorNoScale(StockFingerOffsets[Index]).GetSafeNormal();
			const FVector AuthoredAcross = DwarfGripAuthoredFingers[Index].GetSafeNormal2D();
			if (Begin.IsNearlyZero() || AuthoredAcross.IsNearlyZero()) { return; }
			const double Axial = FingerAxial[Index];
			const FVector End = AuthoredAcross * FMath::Sqrt(1.0 - Axial * Axial) + FVector::UpVector * Axial;
			const FVector BlendedFinger = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Begin, End), Alpha).RotateVector(Begin);
			FingerOffsets[Index] = Rotation.RotateVector(BlendedFinger) * StockFingerOffsets[Index].Size();
			SphereCenters[Index] = Shoulders[Index] - PalmOffsets[Index] + FingerOffsets[Index];
			const FVector Elbow = WorldJoint(Arms[Index].Elbow);
			SphereRadii[Index] = FVector::Distance(Elbow, Shoulders[Index]) + FVector::Distance(Elbow, WorldJoint(Arms[Index].Wrist)) - 3.0 * Alpha;
		}
		const double MinimumHeight = DwarfGripFloor + 2.0 * Alpha - PalmOffsets[0].Z
			- DwarfGripBounds.TransformBy(FTransform(Rotation, FVector::ZeroVector, DwarfGripSwordScale)).Min.Z;
		const double WallClearance = 2.0;
		const double MinimumWallProjection = bHasFrontWall
			? FrontWall.W + WallClearance - FVector::DotProduct(PalmOffsets[0], WallNormal)
				- LowestBladeProjection(DwarfGripBounds, Rotation, DwarfGripSwordScale, WallNormal)
			: 0.0;
		// Project the rigid hilt into both arm reaches and clear of the floor and any frontal wall.
		FVector Residue[4] = {};
		for (int32 Pass = 0; Pass < 40; ++Pass)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FVector Request = Center + Residue[Index];
				Center = SphereCenters[Index] + (Request - SphereCenters[Index]).GetClampedToMaxSize(SphereRadii[Index]);
				Residue[Index] = Request - Center;
			}
			const FVector Request = Center + Residue[2];
			Center = FVector(Request.X, Request.Y, FMath::Max(Request.Z, MinimumHeight));
			Residue[2] = Request - Center;
			if (bHasFrontWall)
			{
				const FVector WallRequest = Center + Residue[3];
				Center = WallRequest + WallNormal * FMath::Max(0.0, MinimumWallProjection - FVector::DotProduct(WallRequest, WallNormal));
				Residue[3] = WallRequest - Center;
			}
		}
		const double Reserve = 3.0 * Alpha / DwarfGripMeshWorld.GetScale3D().X;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector PalmGoal = DwarfGripMeshWorld.InverseTransformPosition(Center + PalmOffsets[Index]);
			if (!Arms[Index].Solve(Corrected, PalmGoal, DwarfGripMeshWorld.InverseTransformVector(FingerOffsets[Index]), Reserve)
				|| FVector::Distance(Arms[Index].Palm(Corrected), PalmGoal) > .10) { return; }
		}
		const FVector FinalRight = DwarfGripMeshWorld.TransformPosition(Right.Palm(Corrected));
		const FVector FinalLeft = DwarfGripMeshWorld.TransformPosition(Left.Palm(Corrected));
		FTransform Grip;
		if (!URogue10mWeaponVisualComponent::BuildSwordPalmGrip(FinalRight, FinalLeft, FinalRight - WorldJoint(Right.Wrist), Grip)) { return; }
		Grip.SetScale3D(DwarfGripSwordScale);
		if (DwarfGripBounds.TransformBy(Grip).Min.Z < DwarfGripFloor - .005) { return; }
		if (bHasFrontWall && FrontWall.PlaneDot(FinalRight)
			+ LowestBladeProjection(DwarfGripBounds, Grip.GetRotation(), DwarfGripSwordScale, WallNormal) < WallClearance - .005) { return; }
		FPoseContext Mixed(Output);
		Mixed.Pose.CopyBonesFrom(Output.Pose);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Corrected, Mixed.Pose);
		for (const FCompactPoseBoneIndex Bone : {Right.Shoulder, Right.Elbow, Right.Wrist, Left.Shoulder, Left.Elbow, Left.Wrist})
		{
			Output.Pose[Bone] = Mixed.Pose[Bone];
			Output.Pose[Bone].NormalizeRotation();
		}
	}

	void CaptureRollFloor(const UAnimInstance* Instance, const ARogue10mCharacter* Character)
	{
		bHasRollFloor = false;
		if (!CastChecked<URogue10mWeaponRetargetAnimInstance>(Instance)->bEnableRollPalmSupport) { return; }
#if WITH_EDITOR
		if (!URogue10mWeaponVisualComponent::IsRollSwordFloorCorrectionEnabled()) { return; }
#endif
		const auto* Mesh = Instance->GetSkelMeshComponent();
		float Progress = 0.0f;
		FVector Direction;
		if (!Mesh || !Character || !Character->IsThirdPersonGameplayActive()
			|| Character->GetEquippedWeaponType() != ERogue10mWeaponType::GreatSword
			|| !Character->GetRollPresentation(Progress, Direction)
			|| !URogue10mWeaponVisualComponent::GetRollSwordFloorPlane(*Character, RollFloorPoint, RollFloorNormal)) { return; }

		RollMeshToWorld = Mesh->GetComponentTransform();
		bHasRollFloor = true;
	}

	void SolveRollPalmSupport(FCompactPose& Pose) const
	{
		if (!bHasRollFloor) { return; }
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const auto FindBone = [&Bones](FName Name)
		{
			return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(Name)));
		};
		const FCompactPoseBoneIndex UpperIndex = FindBone(TEXT("upperarm_r"));
		const FCompactPoseBoneIndex LowerIndex = FindBone(TEXT("lowerarm_r"));
		const FCompactPoseBoneIndex HandIndex = FindBone(TEXT("hand_r"));
		const FCompactPoseBoneIndex FingerIndex = FindBone(TEXT("middle_01_r"));
		if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE || FingerIndex == INDEX_NONE) { return; }

		FCSPose<FCompactPose> ComponentPose;
		ComponentPose.InitPose(Pose);
		FTransform Upper = ComponentPose.GetComponentSpaceTransform(UpperIndex);
		FTransform Lower = ComponentPose.GetComponentSpaceTransform(LowerIndex);
		FTransform Hand = ComponentPose.GetComponentSpaceTransform(HandIndex);
		const FVector Palm = (Hand.GetLocation() + ComponentPose.GetComponentSpaceTransform(FingerIndex).GetLocation()) * 0.5;
		const double Height = FVector::DotProduct(RollMeshToWorld.TransformPosition(Palm) - RollFloorPoint, RollFloorNormal);
		// The near-horizontal GreatSword extends 5.62 cm below its grip at the authored carry angle.
		constexpr double CarryPalmHeightCm = 6.25;
		if (Height >= CarryPalmHeightCm) { return; }

		const FVector Shoulder = Upper.GetLocation();
		const FVector ArmAxis = (Hand.GetLocation() - Shoulder).GetSafeNormal();
		const FVector ElbowPole = FVector::VectorPlaneProject(Lower.GetLocation() - Shoulder, ArmAxis);
		const FVector Target = Hand.GetLocation()
			+ RollMeshToWorld.InverseTransformVector(RollFloorNormal * (CarryPalmHeightCm - Height));
		const FVector TargetAxis = (Target - Shoulder).GetSafeNormal();
		if (ArmAxis.IsNearlyZero() || ElbowPole.IsNearlyZero() || TargetAxis.IsNearlyZero()) { return; }

		// Carry the current bend plane to the raised wrist target, preserving both segment lengths and wrist orientation.
		const FVector Pole = Shoulder + FQuat::FindBetweenNormals(ArmAxis, TargetAxis).RotateVector(ElbowPole);
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1.0, 1.0);
		const FBoneTransform Adjustments[] = { {UpperIndex, Upper}, {LowerIndex, Lower}, {HandIndex, Hand} };
		ComponentPose.LocalBlendCSBoneTransforms(Adjustments, 1.0f);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(ComponentPose, Pose);
		Pose.NormalizeRotations();
	}

	TArray<FTransform> PreviousLocalPose;
	TBitArray<> PreviousBoneMask;
	float Duration = 0.0f;
	float Elapsed = 0.0f;
	float OldLegWeight = 1.0f;
	FTransform RollMeshToWorld;
	FVector RollFloorPoint = FVector::ZeroVector;
	FVector RollFloorNormal = FVector::UpVector;
	bool bHasRollFloor = false;
	FTransform DwarfGripMeshWorld;
	FBox DwarfGripBounds;
	FQuat DwarfGripAuthoredRotation = FQuat::Identity;
	FVector DwarfGripAuthoredFingers[2] = {FVector::RightVector, FVector::RightVector};
	FVector DwarfGripForward = FVector::ForwardVector;
	FVector DwarfGripRight = FVector::RightVector;
	FVector DwarfGripSwordScale = FVector::OneVector;
	double DwarfGripHeight = 0.0;
	double DwarfGripFloor = 0.0;
	double DwarfGripSpan = 0.0;
	float DwarfGripWeight = 0.0f;
	FPlane FrontWall = FPlane(ForceInit);
	FVector FrontHiltOffset = FVector::ZeroVector;
};
}

bool URogue10mWeaponRetargetAnimInstance::UsesVisibleEarthSplitter(const ARogue10mCharacter& Character)
{
	const auto* Body = Character.GetMesh();
	const auto* Retarget = Body ? Cast<URogue10mWeaponRetargetAnimInstance>(Body->GetAnimInstance()) : nullptr;
	if (!Retarget) { return false; }
	const auto Appearance = Character.GetCharacterAppearance();
	if (Appearance.Race == ERogue10mCharacterRace::Dwarf) { return true; }
	const auto* Mesh = Body->GetSkeletalMeshAsset();
	return Retarget->bEnableOrcEarthSplitterPresentation && Mesh && Mesh->GetFName() == TEXT("SK_Or_M_FullBody")
		&& Appearance.Race == ERogue10mCharacterRace::Orc && Appearance.Gender == ERogue10mCharacterGender::Male;
}

void URogue10mWeaponRetargetAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	auto* Character = Cast<ARogue10mCharacter>(TryGetPawnOwner());
	auto* Visual = Character ? Character->FindComponentByClass<URogue10mWeaponVisualComponent>() : nullptr;
	TArray<FTransform> PreviousPose;
	TBitArray<> PreviousBones;
	if (Visual && EquipmentBlendSeconds > UE_SMALL_NUMBER)
	{
		Visual->ConsumeEquipmentRetargetPose(*this, PreviousPose, PreviousBones);
	}
	GetProxyOnGameThread<FWeaponRetargetProxy>().SetPreviousPose(MoveTemp(PreviousPose), MoveTemp(PreviousBones), EquipmentBlendSeconds);
}

FAnimInstanceProxy* URogue10mWeaponRetargetAnimInstance::CreateAnimInstanceProxy()
{
	return new FWeaponRetargetProxy(this);
}

void URogue10mWeaponRetargetAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}
