// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mFistAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mFirstPersonPresentationComponent.h"

namespace
{
	const FName FistSlot(TEXT("DefaultSlot"));

	float FiniteClamp(float Value, float Min, float Max, float Fallback)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Fallback;
	}

	/** Small first-person adaptations, not claimed measurements of the source FBX. */
	struct FBrawlerStanceTuning
	{
		FVector LeadGuard = FVector::ZeroVector;
		FVector RearGuard = FVector::ZeroVector;
		float ElbowTuck = 8.0f;
		float PelvisYawShare = 0.40f;
		float FootHalfDepth = 4.0f;
		float HipCompression = 0.7f;
		float TorsoTurnScale = 1.0f;
		float PivotScale = 1.0f;
	};

	FBrawlerStanceTuning MakeStanceTuning(ERogue10mBrawlerStance Style)
	{
		FBrawlerStanceTuning Result;
		switch (Style)
		{
		case ERogue10mBrawlerStance::LongGuard:
			Result.LeadGuard = FVector(0.0f, 3.0f, 1.0f);
			Result.RearGuard = FVector(0.0f, -1.0f, 2.0f);
			Result.ElbowTuck = 5.0f;
			Result.PelvisYawShare = 0.28f;
			Result.FootHalfDepth = 3.0f;
			Result.HipCompression = 0.3f;
			Result.TorsoTurnScale = 0.85f;
			Result.PivotScale = 0.75f;
			break;
		case ERogue10mBrawlerStance::RootedMartial:
			Result.LeadGuard = FVector(1.0f, -1.0f, -1.0f);
			Result.RearGuard = FVector(1.0f, -1.0f, 0.0f);
			Result.ElbowTuck = 9.0f;
			Result.PelvisYawShare = 0.52f;
			Result.FootHalfDepth = 6.0f;
			Result.HipCompression = 1.1f;
			Result.TorsoTurnScale = 1.15f;
			Result.PivotScale = 1.2f;
			break;
		default:
			break;
		}
		return Result;
	}

	FCompactPoseBoneIndex FindBone(const FCompactPose& Pose, FName Name)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const int32 Index = Bones.GetReferenceSkeleton().FindBoneIndex(Name);
		return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
			: Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
	}


	struct FHandAnatomicalBasis
	{
		FVector Forward = FVector::ForwardVector;
		FVector Thumb = FVector::RightVector;
		bool bValid = false;
	};

	FHandAnatomicalBasis ReadHandBasis(FCSPose<FCompactPose>& Pose, bool bLeft)
	{
		FHandAnatomicalBasis Result;
		const FCompactPose& LocalPose = Pose.GetPose();
		const FCompactPoseBoneIndex Hand = FindBone(LocalPose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
		const FCompactPoseBoneIndex Middle = FindBone(LocalPose, bLeft ? TEXT("middle_01_l") : TEXT("middle_01_r"));
		const FCompactPoseBoneIndex Index = FindBone(LocalPose, bLeft ? TEXT("index_01_l") : TEXT("index_01_r"));
		const FCompactPoseBoneIndex Pinky = FindBone(LocalPose, bLeft ? TEXT("pinky_01_l") : TEXT("pinky_01_r"));
		const FCompactPoseBoneIndex Thumb = FindBone(LocalPose, bLeft ? TEXT("thumb_01_l") : TEXT("thumb_01_r"));
		if (Hand == INDEX_NONE || Middle == INDEX_NONE || Index == INDEX_NONE
			|| Pinky == INDEX_NONE || Thumb == INDEX_NONE)
		{
			return Result;
		}
		const FTransform Wrist = Pose.GetComponentSpaceTransform(Hand);
		const FVector ForwardCS = (Pose.GetComponentSpaceTransform(Middle).GetLocation() - Wrist.GetLocation()).GetSafeNormal();
		FVector ThumbCS = Pose.GetComponentSpaceTransform(Index).GetLocation()
			- Pose.GetComponentSpaceTransform(Pinky).GetLocation();
		ThumbCS = FVector::VectorPlaneProject(ThumbCS, ForwardCS).GetSafeNormal();
		const FVector ThumbRootCS = Pose.GetComponentSpaceTransform(Thumb).GetLocation() - Wrist.GetLocation();
		if (FVector::DotProduct(ThumbCS, ThumbRootCS) < 0.0)
		{
			ThumbCS *= -1.0;
		}
		if (!ForwardCS.IsNearlyZero() && !ThumbCS.IsNearlyZero())
		{
			Result.Forward = Wrist.InverseTransformVectorNoScale(ForwardCS).GetSafeNormal();
			Result.Thumb = Wrist.InverseTransformVectorNoScale(ThumbCS).GetSafeNormal();
			Result.bValid = true;
		}
		return Result;
	}


	/** Cache a closed hand using measured chain directions, not assumed local bone axes. */
	void CloseFingerPose(FCompactPose& Pose, float KnuckleDegrees, float MiddleDegrees, float TipDegrees)
	{
		FCSPose<FCompactPose> Source;
		Source.InitPose(Pose);
		TArray<FBoneTransform, TInlineAllocator<30>> Changes;
		const TCHAR* Fingers[] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
		for (bool bLeft : { true, false })
		{
			const TCHAR* Suffix = bLeft ? TEXT("l") : TEXT("r");
			const FCompactPoseBoneIndex HandIndex = FindBone(Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
			const FHandAnatomicalBasis Basis = ReadHandBasis(Source, bLeft);
			if (HandIndex == INDEX_NONE || !Basis.bValid)
			{
				continue;
			}
			const FTransform Wrist = Source.GetComponentSpaceTransform(HandIndex);
			const FVector Forward = Wrist.TransformVectorNoScale(Basis.Forward).GetSafeNormal();
			const FVector ThumbSide = Wrist.TransformVectorNoScale(Basis.Thumb).GetSafeNormal();
			// Mirrored anatomical radial axes have opposite cross-product handedness.
			const FVector Palm = FVector::CrossProduct(Forward, ThumbSide).GetSafeNormal() * (bLeft ? -1.0f : 1.0f);
			const float A1 = FMath::DegreesToRadians(KnuckleDegrees);
			const float A2 = FMath::DegreesToRadians(KnuckleDegrees + MiddleDegrees);
			const float A3 = FMath::DegreesToRadians(KnuckleDegrees + MiddleDegrees + TipDegrees);
			const FVector D1 = Forward * FMath::Cos(A1) + Palm * FMath::Sin(A1);
			const FVector D2 = Forward * FMath::Cos(A2) + Palm * FMath::Sin(A2);
			const FVector D3 = Forward * FMath::Cos(A3) + Palm * FMath::Sin(A3);
			FVector IndexMiddle = FVector::ZeroVector;
			bool bHaveIndex = false;
			for (const TCHAR* Finger : Fingers)
			{
				const FCompactPoseBoneIndex I1 = FindBone(Pose, FName(*FString::Printf(TEXT("%s_01_%s"), Finger, Suffix)));
				const FCompactPoseBoneIndex I2 = FindBone(Pose, FName(*FString::Printf(TEXT("%s_02_%s"), Finger, Suffix)));
				const FCompactPoseBoneIndex I3 = FindBone(Pose, FName(*FString::Printf(TEXT("%s_03_%s"), Finger, Suffix)));
				if (I1 == INDEX_NONE || I2 == INDEX_NONE || I3 == INDEX_NONE)
				{
					continue;
				}
				FTransform B1 = Source.GetComponentSpaceTransform(I1);
				FTransform B2 = Source.GetComponentSpaceTransform(I2);
				FTransform B3 = Source.GetComponentSpaceTransform(I3);
				const FVector OldD1 = B2.GetLocation() - B1.GetLocation();
				const FVector OldD2 = B3.GetLocation() - B2.GetLocation();
				// Carry the measured parent-child longitudinal axis into the terminal phalanx.
				const FVector OldD3 = B3.TransformVectorNoScale(B2.InverseTransformVectorNoScale(OldD2)).GetSafeNormal();
				B1.SetRotation(FQuat::FindBetweenNormals(OldD1.GetSafeNormal(), D1) * B1.GetRotation());
				B2.SetRotation(FQuat::FindBetweenNormals(OldD2.GetSafeNormal(), D2) * B2.GetRotation());
				B3.SetRotation(FQuat::FindBetweenNormals(OldD3, D3) * B3.GetRotation());
				B2.SetLocation(B1.GetLocation() + D1 * OldD1.Size());
				B3.SetLocation(B2.GetLocation() + D2 * OldD2.Size());
				B1.NormalizeRotation();
				B2.NormalizeRotation();
				B3.NormalizeRotation();
				Changes.Emplace(I1, B1);
				Changes.Emplace(I2, B2);
				Changes.Emplace(I3, B3);
				if (FCString::Strcmp(Finger, TEXT("index")) == 0)
				{
					IndexMiddle = B2.GetLocation();
					bHaveIndex = true;
				}
			}
			// Lay the thumb outside the folded index, with clearance instead of burying it in the palm.
			const FCompactPoseBoneIndex T1 = FindBone(Pose, bLeft ? TEXT("thumb_01_l") : TEXT("thumb_01_r"));
			const FCompactPoseBoneIndex T2 = FindBone(Pose, bLeft ? TEXT("thumb_02_l") : TEXT("thumb_02_r"));
			const FCompactPoseBoneIndex T3 = FindBone(Pose, bLeft ? TEXT("thumb_03_l") : TEXT("thumb_03_r"));
			if (bHaveIndex && T1 != INDEX_NONE && T2 != INDEX_NONE && T3 != INDEX_NONE)
			{
				FTransform B1 = Source.GetComponentSpaceTransform(T1);
				FTransform B2 = Source.GetComponentSpaceTransform(T2);
				FTransform B3 = Source.GetComponentSpaceTransform(T3);

				const FVector ThumbTarget = IndexMiddle + Palm * 1.5f;
				const FVector ThumbJoint = B1.GetLocation() + ThumbSide * 4.0f + Palm * 3.0f;
				const FQuat OldMiddleRotation = B2.GetRotation();
				AnimationCore::SolveTwoBoneIK(B1, B2, B3, ThumbJoint, ThumbTarget, false, 1.0, 1.0);
				B3.SetRotation(B2.GetRotation() * OldMiddleRotation.Inverse() * B3.GetRotation());
				B1.NormalizeRotation();
				B2.NormalizeRotation();
				B3.NormalizeRotation();
				Changes.Emplace(T1, B1);
				Changes.Emplace(T2, B2);
				Changes.Emplace(T3, B3);
			}
		}
		if (!Changes.IsEmpty())
		{
			Changes.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
			Source.LocalBlendCSBoneTransforms(Changes, 1.0f);
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Source, Pose);
			Pose.NormalizeRotations();
		}
	}

	/** Aim the knuckles along the forearm; keep the thumb up/inward on both mirrored hands. */
	void OrientAnatomicalFist(FTransform& Lower, FTransform& Hand, const FQuat& WristLocalRotation,
		const FHandAnatomicalBasis& Basis, float Side, float InwardDegrees)
	{
		const FQuat BaselineHandRotation = Lower.GetRotation() * WristLocalRotation;
		Hand.SetRotation(BaselineHandRotation);
		if (!Basis.bValid)
		{
			return;
		}
		const FVector Forward = (Hand.GetLocation() - Lower.GetLocation()).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return;
		}
		const float Lean = FMath::DegreesToRadians(InwardDegrees);
		const FVector RequestedThumb(-Side * FMath::Sin(Lean), 0.0f, FMath::Cos(Lean));
		FVector TargetThumb = FVector::VectorPlaneProject(RequestedThumb, Forward).GetSafeNormal();
		if (TargetThumb.IsNearlyZero())
		{
			TargetThumb = FVector::VectorPlaneProject(FVector(-Side, 0.0, 0.0), Forward).GetSafeNormal();
		}
		// Source axes are measured from the actual skeleton, so left/right local bone axes may differ.
		const FQuat SourceBasis = FRotationMatrix::MakeFromXY(Basis.Forward, Basis.Thumb).ToQuat();
		const FQuat TargetBasis = FRotationMatrix::MakeFromXY(Forward, TargetThumb).ToQuat();
		const FQuat TargetHandRotation = TargetBasis * SourceBasis.Inverse();

		// Carry pronation on the whole forearm instead of twisting only the wrist skin.
		// The signed angle is derived from the measured thumb direction, never a fixed 180-degree roll.
		const FVector BaselineThumb = FVector::VectorPlaneProject(
			BaselineHandRotation.RotateVector(Basis.Thumb), Forward).GetSafeNormal();
		if (!BaselineThumb.IsNearlyZero())
		{
			const double SignedTwist = FMath::Atan2(
				FVector::DotProduct(Forward, FVector::CrossProduct(BaselineThumb, TargetThumb)),
				FVector::DotProduct(BaselineThumb, TargetThumb));
			Lower.SetRotation(FQuat(Forward, SignedTwist) * Lower.GetRotation());
		}
		Hand.SetRotation(TargetHandRotation);
	}


	/** Apply absolute anatomical body orientations while preserving each local bone translation. */
	FTransform ApplyBasicBodyPose(FCompactPose& Pose, FCSPose<FCompactPose>& BasePose,
		const FQuat& MeshToCamera, const FRotator& BodyRotation, const FRotator& HeadRotation,
		const FVector& BodyOffset, float PelvisYawShare = 0.08f)
	{
		const FName BodyNames[] = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
			TEXT("spine_04"), TEXT("spine_05"), TEXT("neck_01"), TEXT("neck_02"), TEXT("head") };
		const float Weights[] = { 0.08f, 0.25f, 0.45f, 0.65f, 0.85f, 1.0f, 0.55f, 0.8f, 1.0f };
		FCompactPoseBoneIndex Indices[UE_ARRAY_COUNT(BodyNames)];
		for (int32 I = 0; I < UE_ARRAY_COUNT(BodyNames); ++I) { Indices[I] = FindBone(Pose, BodyNames[I]); }
		TArray<FTransform, TInlineAllocator<256>> ComponentTransforms;
		ComponentTransforms.SetNum(Pose.GetNumBones());
		FTransform HeadResult = FTransform::Identity;
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
			const bool bHasParent = Parent != INDEX_NONE;
			const FTransform ParentTransform = bHasParent ? ComponentTransforms[Parent.GetInt()] : FTransform::Identity;
			FTransform Component = Pose[Bone] * ParentTransform;
			for (int32 I = 0; I < UE_ARRAY_COUNT(BodyNames); ++I)
			{
				if (Indices[I] == INDEX_NONE || Bone != Indices[I]) { continue; }
				FRotator CameraRotation = (I >= 6 ? HeadRotation : BodyRotation) * Weights[I];
				if (I == 0) { CameraRotation.Yaw = BodyRotation.Yaw * PelvisYawShare; }
				const FQuat Delta = MeshToCamera.Inverse() * CameraRotation.Quaternion() * MeshToCamera;
				Component.SetRotation(Delta * BasePose.GetComponentSpaceTransform(Bone).GetRotation());
				if (I == 0) { Component.AddToTranslation(MeshToCamera.Inverse().RotateVector(BodyOffset)); }
				Pose[Bone] = Component.GetRelativeTransform(ParentTransform);
				Pose[Bone].NormalizeRotation();
				break;
			}
			ComponentTransforms[Bone.GetInt()] = Component;
			if (Bone == Indices[8]) { HeadResult = Component; }
		}
		return HeadResult;
	}

	float SmoothPhase(float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** A mostly linear raise distributes the long hip-to-guard travel over its full time window. */
	float ReadyTravelPhase(float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return FMath::Lerp(T, SmoothPhase(T), 0.25f);
	}

	float StrikeDrive(float Progress, float Peak, float Acceleration, float Recovery)
	{
		return Progress < Peak ? SmoothPhase((Progress - Peak + Acceleration) / Acceleration)
			: 1.0f - SmoothPhase((Progress - Peak) / Recovery);
	}

	FVector RecoveryArc(const FVector& Apex, const FVector& Guard, float Progress, float Arc)
	{
		const float T = SmoothPhase(Progress);
		const float U = 1.0f - T;
		// Retract first, then fold down/outside the outgoing path instead of reversing the same line.
		const FVector First = Apex + FVector(Arc * 0.4f, -14.0f, -2.0f);
		const FVector Second = Guard + FVector(Arc, -5.0f, -Arc * 0.7f);
		return Apex * (U * U * U) + First * (3.0f * U * U * T)
			+ Second * (3.0f * U * T * T) + Guard * (T * T * T);
	}

	/** Game-thread asset/config snapshot; pose evaluation uses only immutable data. */
	struct FFistAnimProxy final : FAnimInstanceProxy
	{
		explicit FFistAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

		virtual void Initialize(UAnimInstance* Instance) override
		{
			FAnimInstanceProxy::Initialize(Instance);
			RegisterSlotNodeWithAnimInstance(FistSlot);
		}

		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const URogue10mFistAnimInstance* Fist = CastChecked<URogue10mFistAnimInstance>(Instance);
			Idle = Fist->LoadedIdleAnimation;
			BodyStrength = FiniteClamp(Fist->BodyMotionStrength, 0.0f, 1.0f, 0.0f);
			MeshToCamera = GetComponentRelativeTransform().GetRotation();
			if (FistPose != Fist->LoadedFistAnimation) { bFingerCacheValid = false; }
			FistPose = Fist->LoadedFistAnimation;
			if (!CurlDegrees.Equals(Fist->FingerCurlDegrees)) { bFingerCacheValid = false; }
			CurlDegrees = Fist->FingerCurlDegrees;
			GuardTarget = Fist->GuardHandTarget;
			ExtendedTarget = Fist->ExtendedHandTarget;
			PunchTravel = FMath::Max(10.0f, Fist->SourcePunchTravel);
			GuardThumbInward = FMath::Clamp(Fist->GuardThumbInwardDegrees, -60.0f, 90.0f);
			HitThumbInward = FMath::Clamp(Fist->HitThumbInwardDegrees, -60.0f, 90.0f);

			const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(Instance->TryGetPawnOwner());
			// Cache owner state on the game thread; evaluation never reads UObjects.
			FullBodyAimPitch = 0.0f;
			FullBodySpineShare = 0.0f;
			FullBodyTargetOffset = FVector::ZeroVector;
			FullBodyGuardPullback = 0.0f;
			FullBodyGuardInward = 0.0f;
			const URogue10mFirstPersonPresentationComponent* Presentation = Character
				? Character->GetFirstPersonPresentationComponent() : nullptr;
			if (Presentation && Presentation->IsFullBodyPresentationActive()
				&& Instance->GetSkelMeshComponent() == Character->GetFirstPersonMesh()
				&& FMath::IsFinite(Presentation->FullBodyAimPitchLimit)
				&& FMath::IsFinite(Presentation->FullBodySpinePitchShare))
			{
				const float Pitch = FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch - Character->GetActorRotation().Pitch);
				const float Limit = FMath::Clamp(Presentation->FullBodyAimPitchLimit, 0.0f, 85.0f);
				if (FMath::IsFinite(Pitch)) { FullBodyAimPitch = FMath::Clamp(Pitch, -Limit, Limit); }
				FullBodySpineShare = FMath::Clamp(Presentation->FullBodySpinePitchShare, 0.0f, 0.5f);
				if (FMath::IsFinite(Presentation->FullBodyGuardPullback))
				{
					FullBodyGuardPullback = FMath::Clamp(Presentation->FullBodyGuardPullback, 0.0f, 25.0f);
				}
				if (FMath::IsFinite(Presentation->FullBodyGuardInward))
				{
					FullBodyGuardInward = FMath::Clamp(Presentation->FullBodyGuardInward, 0.0f, 15.0f);
				}
				if (!Presentation->FullBodyHandTargetOffset.ContainsNaN())
				{
					FullBodyTargetOffset = Presentation->FullBodyHandTargetOffset.GetClampedToMaxSize(40.0);
				}
			}
			const bool bFullBodyOwner = Presentation && Presentation->IsFullBodyPresentationActive()
				&& Instance->GetSkelMeshComponent() == Character->GetFirstPersonMesh() && !Character->IsDead();
			GroundSpeed = 0.0f;
			GaitStrength = FMath::IsFinite(Fist->LocomotionMotionStrength)
				? FMath::Clamp(Fist->LocomotionMotionStrength, 0.0f, 1.0f) : 0.0f;
			GaitReferenceSpeed = FMath::IsFinite(Fist->LocomotionReferenceSpeed)
				? FMath::Clamp(Fist->LocomotionReferenceSpeed, 100.0f, 1200.0f) : 600.0f;
			GaitStrideLength = FMath::IsFinite(Fist->LocomotionStrideLength)
				? FMath::Clamp(Fist->LocomotionStrideLength, 150.0f, 600.0f) : 340.0f;
			GaitVerticalAmplitude = FMath::IsFinite(Fist->LocomotionVerticalAmplitude)
				? FMath::Clamp(Fist->LocomotionVerticalAmplitude, 0.0f, 4.0f) : 0.0f;
			if (bFullBodyOwner && Character->GetCharacterMovement())
			{
				const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
				if (Movement->IsMovingOnGround() && !Movement->Velocity.ContainsNaN())
				{
					GroundSpeed = FMath::Clamp(static_cast<float>(Movement->Velocity.Size2D()), 0.0f, 1800.0f);
				}
			}
			// Deactivation must not carry a residual full-body gait into the legacy arms mesh.
			if (!bFullBodyOwner) { GaitWeight = 0.0f; GaitPhase = 0.0f; }
			const bool bNextBasic = Character && !Character->IsDead()
				&& Character->GetEquippedWeaponType() == ERogue10mWeaponType::Unarmed;
			if (bNextBasic != bBasicPose)
			{
				bBasicHandsInitialized = false;
				LastBasicAttackSerial = 0;
				bWasBasicCharging = false;
				ChargePresentationAge = 0.0f;
				ComboPrepareWeight = 0.0f;
				CurrentHookElbowWeight = AttackStartHookElbowWeight = ChargeStartHookElbowWeight = 0.0f;
				SmoothedBodyRotation = FRotator::ZeroRotator;
				SmoothedHeadRotation = FRotator::ZeroRotator;
				SmoothedBodyOffset = FVector::ZeroVector;
				AirInertia = 0.0f;
				TakeoffAge = LandingAge = 1.0f;
				bPreviouslyFalling = false;
				PreviousVerticalSpeed = 0.0f;
			}
			bBasicPose = bNextBasic;
			const bool bNextRelaxed = bFullBodyOwner && bBasicPose && Fist->bEnableRelaxedIdle;
			if (bNextRelaxed != bRelaxedIdle)
			{
				CombatReadyProgress = bNextRelaxed ? 0.0f : 1.0f;
				ReadyIdleAge = 100.0f;
				bBasicHandsInitialized = false;
			}
			bRelaxedIdle = bNextRelaxed;
			ReadyRaiseSeconds = FiniteClamp(Fist->CombatReadyRaiseSeconds, 0.08f, 0.3f, 0.16f);
			ReadyHoldSeconds = FiniteClamp(Fist->CombatReadyHoldSeconds, 0.0f, 3.0f, 0.9f);
			ReadyLowerSeconds = FiniteClamp(Fist->CombatReadyLowerSeconds, 0.2f, 1.0f, 0.45f);
			StanceTuning = MakeStanceTuning(Fist->BasicCombatStance);
			StanceWeight = bFullBodyOwner && bBasicPose && BodyStrength > KINDA_SMALL_NUMBER
				? FiniteClamp(Fist->BasicStanceStrength, 0.0f, 1.0f, 0.0f) : 0.0f;
			LeadFootStep = FiniteClamp(Fist->BasicLeadFootStep, 0.0f, 10.0f, 0.0f);
			SupportFootPivot = FiniteClamp(Fist->BasicSupportFootPivot, 0.0f, 20.0f, 0.0f);
			SupportHeelLift = FiniteClamp(Fist->BasicSupportHeelLift, 0.0f, 3.0f, 0.0f);
			BasicSnapshot = FRogue10mBasicBrawlerSnapshot();
			if (bBasicPose && Character->GetBasicBrawlerComponent())
			{
				BasicSnapshot = Character->GetBasicBrawlerComponent()->GetSnapshot();
			}
			// Validate/load on the game thread. Parallel evaluation only samples this retained sequence.
			const UAnimSequence* NextSource = Fist->LoadedSourceBoxingAnimation;
			const USkeletalMesh* MeshAsset = Instance->GetSkelMeshComponent()
				? Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset() : nullptr;
			bSourceBoxingEligible = Fist->bEnableSourceBoxingBasicAttacks && bFullBodyOwner && bBasicPose
				&& NextSource && MeshAsset && NextSource->GetSkeleton() == MeshAsset->GetSkeleton()
				&& !NextSource->IsValidAdditive() && FMath::IsFinite(NextSource->GetPlayLength())
				&& NextSource->GetPlayLength() > SourceBoxingHitSeconds + 0.01f;
			if (SourceBoxing != NextSource || !bSourceBoxingEligible)
			{
				bSourceBoxingCacheValid = false;
				PreviousSourcePose.Reset();
				SourceTransitionPose.Reset();
				bPreviousSourcePoseActive = false;
			}
			SourceBoxing = NextSource;
			SourceBoxingLength = bSourceBoxingEligible ? NextSource->GetPlayLength() : 0.0f;
			JabTarget = Fist->BasicJabTarget;
			StraightTarget = Fist->BasicStraightTarget;
			StraightBodyAdvance = FMath::Clamp(Fist->BasicStraightBodyAdvance, 0.0f, 12.0f);
			HookGuardOffset = Fist->BasicHookGuardOffset;
			HookTarget = Fist->BasicHookTarget;
			HookLoadedTarget = Fist->BasicHookLoadedTarget;
			HookArcControl = Fist->BasicHookArcControl;
			HookBodyYaw = FMath::Clamp(Fist->BasicHookBodyYaw, 0.0f, 35.0f);
			HookThumbInward = FMath::Clamp(Fist->BasicHookThumbInwardDegrees, -30.0f, 60.0f);
			UppercutTarget = Fist->BasicUppercutTarget;
			// Keep the authored fist paths in the shifted full-body eye frame. Offset
			// absolute endpoints/control points together; relative recovery offsets stay unchanged.
			GuardTarget += FullBodyTargetOffset;
			// The basic unarmed reach clamp needs a compact guard before a strike.
			// Its load/recovery/combination paths share this endpoint; apex targets stay forward.
			// Equipped knuckles use their authored montage guard without this retraction.
			if (bBasicPose)
			{
				GuardTarget.Y -= FullBodyGuardPullback;
				if (FullBodyGuardInward > 0.0f)
				{
					GuardTarget.X = FMath::Max(0.0, GuardTarget.X - FullBodyGuardInward);
				}
			}
			ExtendedTarget += FullBodyTargetOffset;
			JabTarget += FullBodyTargetOffset;
			StraightTarget += FullBodyTargetOffset;
			HookTarget += FullBodyTargetOffset;
			HookLoadedTarget += FullBodyTargetOffset;
			HookArcControl += FullBodyTargetOffset;
			UppercutTarget += FullBodyTargetOffset;
			ChargeHandOffset = Fist->BasicChargeHandOffset;
			ChargeBodyYaw = FMath::Clamp(Fist->BasicChargeBodyYaw, 0.0f, 30.0f);
			JumpStrength = FMath::Clamp(Fist->JumpPresentationStrength, 0.0f, 1.0f);
			StrikeAcceleration = FMath::Clamp(Fist->BasicStrikeAccelerationFraction, 0.08f, 0.22f);
			RecoveryFraction = FMath::Clamp(Fist->BasicRecoveryFraction, 0.20f, 0.45f);
			BodyLead = FMath::Clamp(Fist->BasicBodyLeadFraction, 0.0f, 0.08f);
			HeadLag = FMath::Clamp(Fist->BasicHeadLagFraction, 0.0f, 0.08f);
			HandRecoveryArc = FMath::Clamp(Fist->BasicRecoveryArc, 0.0f, 10.0f);
			bFalling = false;
			VerticalSpeed = 0.0f;
			if (bBasicPose && Character->GetCharacterMovement())
			{
				bFalling = Character->GetCharacterMovement()->IsFalling();
				VerticalSpeed = Character->GetCharacterMovement()->Velocity.Z;
				if (bFalling && !bPreviouslyFalling && VerticalSpeed > 80.0f) { TakeoffAge = 0.0f; }
				if (!bFalling && bPreviouslyFalling)
				{
					LandingAge = 0.0f;
					LandingStrength = FMath::Clamp(-PreviousVerticalSpeed / 650.0f, 0.15f, 1.0f);
				}
				bPreviouslyFalling = bFalling;
				PreviousVerticalSpeed = VerticalSpeed;
			}

		}

		virtual void Update(float DeltaSeconds) override
		{
			GetSlotWeight(FistSlot, SlotWeight, SourceWeight, TotalWeight);
			UpdateSlotNodeWeight(FistSlot, SlotWeight, 1.0f);
			EvaluationDeltaSeconds = FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0f;
			IdleTime += EvaluationDeltaSeconds;
			SourcePoseClock += EvaluationDeltaSeconds;
			if (bRelaxedIdle)
			{
				const bool bActiveCombat = BasicSnapshot.bActive && (BasicSnapshot.Phase != ERogue10mBasicBrawlerPhase::Idle
					|| BasicSnapshot.bComboBuffered);
				ReadyIdleAge = bActiveCombat ? 0.0f : ReadyIdleAge + EvaluationDeltaSeconds;
				const bool bRaise = bActiveCombat || ReadyIdleAge < ReadyHoldSeconds;
				const float Change = EvaluationDeltaSeconds / (bRaise ? ReadyRaiseSeconds : ReadyLowerSeconds);
				CombatReadyProgress = FMath::Clamp(CombatReadyProgress + (bRaise ? Change : -Change), 0.0f, 1.0f);
				if (BasicSnapshot.bActive && BasicSnapshot.Phase == ERogue10mBasicBrawlerPhase::Attacking)
				{
					// Presentation cannot postpone the already scheduled hit. Finish raising before its apex.
					const float BeforeHit = FMath::Max(0.001f, BasicSnapshot.AttackDuration * BasicSnapshot.HitFraction * 0.8f);
					CombatReadyProgress = FMath::Max(CombatReadyProgress, FMath::Clamp(BasicSnapshot.AttackElapsed / BeforeHit, 0.0f, 1.0f));
				}
				ReadyWeight = SmoothPhase(CombatReadyProgress);
			}
			else { CombatReadyProgress = ReadyWeight = 1.0f; ReadyIdleAge = 100.0f; }
			const float SupportTarget = !bFalling ? 1.0f - FMath::Clamp((GroundSpeed - 5.0f) / 60.0f, 0.0f, 1.0f) : 0.0f;
			SupportingFeetWeight = StanceWeight > KINDA_SMALL_NUMBER
				? FMath::Lerp(SupportingFeetWeight, SupportTarget, 1.0f - FMath::Exp(-18.0f * FMath::Min(EvaluationDeltaSeconds, 0.1f))) : 0.0f;
			const float GaitDeltaSeconds = FMath::Min(EvaluationDeltaSeconds, 0.1f);
			const float GaitTarget = GroundSpeed > 5.0f
				? FMath::Clamp(GroundSpeed / GaitReferenceSpeed, 0.0f, 1.0f) * GaitStrength : 0.0f;
			GaitWeight = FMath::Lerp(GaitWeight, GaitTarget, 1.0f - FMath::Exp(-12.0f * GaitDeltaSeconds));
			if (GaitWeight < 0.0001f && GaitTarget == 0.0f) { GaitWeight = 0.0f; }
			if (GroundSpeed > 5.0f)
			{
				GaitPhase = FMath::Fmod(GaitPhase + GroundSpeed / GaitStrideLength * (2.0f * PI) * GaitDeltaSeconds, 2.0f * PI);
			}
			TakeoffAge += EvaluationDeltaSeconds;
			LandingAge += EvaluationDeltaSeconds;
			const float AirTarget = bBasicPose && bFalling ? FMath::Clamp(-VerticalSpeed / 400.0f, -1.0f, 1.0f) : 0.0f;
			AirInertia = FMath::FInterpTo(AirInertia, AirTarget, EvaluationDeltaSeconds, 10.0f);
			if (SlotWeight <= ZERO_ANIMWEIGHT_THRESH || BodyStrength <= 0.0f)
			{ SnapshotOffset = FVector::ZeroVector; SnapshotRotation = FRotator::ZeroRotator; SnapshotWeight = 0.0f; }
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			SnapshotOffset = FVector::ZeroVector;
			SnapshotRotation = FRotator::ZeroRotator;
			SnapshotWeight = 0.0f;
			SnapshotHeadOffset = FVector::ZeroVector;
			SnapshotHeadRotation = FRotator::ZeroRotator;
			SnapshotJumpWeight = 0.0f;
			SnapshotSourceBoxingWeight = SnapshotSourceBoxingTime = 0.0f;
			bSnapshotSourceBoxingMirrored = false;
			Output.ResetToRefPose();
			if (!Idle)
			{
				return true;
			}
			FAnimationPoseData IdleData(Output);
			Idle->GetAnimationPose(IdleData,
				FAnimExtractContext(FMath::Fmod(IdleTime, FMath::Max(0.01f, Idle->GetPlayLength())), false));

			// Preserve actual montage evaluation and slot weights. Only the first-person spatial
			// representation is adapted below; the gameplay playback mesh is completely untouched.
			FPoseContext Animated(Output);
			if (SlotWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				FAnimationPoseData AnimatedData(Animated);
				SlotEvaluatePose(FistSlot, IdleData, SourceWeight, AnimatedData, SlotWeight, TotalWeight);
			}
			else
			{
				Animated.Pose.CopyBonesFrom(Output.Pose);
			}

			// The hand pose is static: evaluate/curl it only after a source, LOD, or curl-setting change.
			if (!bFingerCacheValid || CachedBoneSerial != Output.Pose.GetBoneContainer().GetSerialNumber())
			{
				FPoseContext ClosedFist(Output);
				ClosedFist.Pose.CopyBonesFrom(Output.Pose);
				if (FistPose)
				{
					FAnimationPoseData FistData(ClosedFist);
					FistPose->GetAnimationPose(FistData, FAnimExtractContext(0.30, false));
				}
				CloseFingerPose(ClosedFist.Pose, CurlDegrees.X, CurlDegrees.Y, CurlDegrees.Z);
				FCSPose<FCompactPose> SourceHandPose;
				SourceHandPose.InitPose(ClosedFist.Pose);
				HandBases[0] = ReadHandBasis(SourceHandPose, true);
				HandBases[1] = ReadHandBasis(SourceHandPose, false);
				FingerBones.Reset();
				FingerTransforms.Reset();
				for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
				{
					const FString Name = Output.Pose.GetBoneContainer().GetReferenceSkeleton().GetBoneName(
						Output.Pose.GetBoneContainer().MakeMeshPoseIndex(Index).GetInt()).ToString();
					if (Name.StartsWith(TEXT("thumb_")) || Name.StartsWith(TEXT("index_"))
						|| Name.StartsWith(TEXT("middle_")) || Name.StartsWith(TEXT("ring_"))
						|| Name.StartsWith(TEXT("pinky_")))
					{
						FingerBones.Add(Index);
						FingerTransforms.Add(ClosedFist.Pose[Index]);
					}
				}
				for (const bool bLeft : { true, false })
				{
					const FCompactPoseBoneIndex Hand = FindBone(Output.Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
					WristLocalRotations[bLeft ? 0 : 1] = Hand != INDEX_NONE ? ClosedFist.Pose[Hand].GetRotation() : FQuat::Identity;
				}
				CachedBoneSerial = Output.Pose.GetBoneContainer().GetSerialNumber();
				bFingerCacheValid = true;
			}
			for (int32 I = 0; I < FingerBones.Num(); ++I)
			{
				FTransform BlendedFinger;
				BlendedFinger.Blend(Output.Pose[FingerBones[I]], FingerTransforms[I], bRelaxedIdle ? ReadyWeight : 1.0f);
				Output.Pose[FingerBones[I]] = BlendedFinger;
			}


			// SlotEvaluatePose already blended the montage; never apply SlotWeight a second time.
			FCSPose<FCompactPose> OriginalIdleCS;
			OriginalIdleCS.InitPose(Output.Pose);
			FCSPose<FCompactPose> AnimatedCS;
			AnimatedCS.InitPose(Animated.Pose);

			if (bBasicPose)
			{
				EvaluateBasicPose(Output, OriginalIdleCS);
				ApplySourceBoxingPose(Output);
				ApplyLocomotionPose(Output.Pose);
				CaptureHeadDelta(Output.Pose, OriginalIdleCS);
				ApplyFullBodyAim(Output.Pose);
				return true;
			}

			FCompactPoseBoneIndex MotionBone = FindBone(Output.Pose, TEXT("spine_05"));
			if (MotionBone == INDEX_NONE) { MotionBone = FindBone(Output.Pose, TEXT("spine_03")); }
			if (MotionBone != INDEX_NONE && SlotWeight > ZERO_ANIMWEIGHT_THRESH && BodyStrength > 0.0f)
			{
				const FTransform IdleSpine = OriginalIdleCS.GetComponentSpaceTransform(MotionBone);
				const FTransform AnimatedSpine = AnimatedCS.GetComponentSpaceTransform(MotionBone);
				const FVector CameraDelta = MeshToCamera.RotateVector(AnimatedSpine.GetLocation() - IdleSpine.GetLocation());
				SnapshotOffset = (CameraDelta * (0.12f * BodyStrength)).GetClampedToMaxSize(2.0);
				const FQuat SpineDelta = AnimatedSpine.GetRotation() * IdleSpine.GetRotation().Inverse();
				const FRotator CameraRotation = (MeshToCamera * SpineDelta * MeshToCamera.Inverse()).Rotator().GetNormalized();
				SnapshotRotation = FRotator(
					FMath::Clamp(CameraRotation.Pitch * 0.10f * BodyStrength, -1.5, 1.5),
					FMath::Clamp(CameraRotation.Yaw * 0.10f * BodyStrength, -1.5, 1.5),
					FMath::Clamp(CameraRotation.Roll * 0.08f * BodyStrength, -0.8, 0.8));
				SnapshotWeight = FMath::Clamp(SlotWeight, 0.0f, 1.0f);
			}

			// Restore a bounded portion of the real pelvis/spine/clavicle motion in this FP mesh.
			// Apply parent-sorted CS changes, then rebuild the arm CS pose before solving IK.
			TArray<FBoneTransform, TInlineAllocator<8>> BodyChanges;
			const FName BodyBones[] = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
				TEXT("spine_04"), TEXT("spine_05"), TEXT("clavicle_l"), TEXT("clavicle_r") };
			if (SlotWeight > ZERO_ANIMWEIGHT_THRESH && BodyStrength > 0.0f)
			{
				for (const FName Name : BodyBones)
				{
					const FCompactPoseBoneIndex Index = FindBone(Output.Pose, Name);
					if (Index == INDEX_NONE) { continue; }
					const FTransform Base = OriginalIdleCS.GetComponentSpaceTransform(Index);
					const FTransform Motion = AnimatedCS.GetComponentSpaceTransform(Index);
					FTransform Adapted = Base;
					FQuat RotationDelta = Motion.GetRotation() * Base.GetRotation().Inverse();
					if (RotationDelta.W < 0.0) { RotationDelta = RotationDelta * -1.0; }
					RotationDelta.Normalize();
					FVector Axis;
					double Angle = 0.0;
					RotationDelta.ToAxisAndAngle(Axis, Angle);
					const double Limit = FMath::DegreesToRadians(Name == TEXT("pelvis") ? 6.0 : 10.0);
					const double AdaptedAngle = FMath::Clamp(Angle * 0.35 * BodyStrength, 0.0, Limit);
					Adapted.SetRotation(FQuat(Axis, AdaptedAngle) * Base.GetRotation());
					Adapted.SetLocation(Base.GetLocation()
						+ ((Motion.GetLocation() - Base.GetLocation()) * (0.25f * BodyStrength)).GetClampedToMaxSize(4.0));
					Adapted.NormalizeRotation();
					BodyChanges.Emplace(Index, Adapted);
				}
			}
			if (!BodyChanges.IsEmpty())
			{
				BodyChanges.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
				FCSPose<FCompactPose> BodyCS;
				BodyCS.InitPose(Output.Pose);
				BodyCS.LocalBlendCSBoneTransforms(BodyChanges, 1.0f);
				FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(BodyCS, Output.Pose);
			}
			FCSPose<FCompactPose> IdleCS;
			IdleCS.InitPose(Output.Pose);

			FVector HandMotion[2] = { FVector::ZeroVector, FVector::ZeroVector };
			float HandExtension[2] = { 0.0f, 0.0f };
			for (const bool bLeft : { true, false })
			{
				const int32 SideIndex = bLeft ? 0 : 1;
				const FCompactPoseBoneIndex Hand = FindBone(Output.Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
				if (Hand != INDEX_NONE)
				{
					HandMotion[SideIndex] = AnimatedCS.GetComponentSpaceTransform(Hand).GetLocation()
						- OriginalIdleCS.GetComponentSpaceTransform(Hand).GetLocation();
					HandExtension[SideIndex] = FMath::Clamp(static_cast<float>(HandMotion[SideIndex].Y / PunchTravel), 0.0f, 1.0f);
				}
			}

			TArray<FBoneTransform, TInlineAllocator<6>> Changes;
			for (const bool bLeft : { true, false })
			{
				const FCompactPoseBoneIndex UpperIndex = FindBone(Output.Pose, bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
				const FCompactPoseBoneIndex LowerIndex = FindBone(Output.Pose, bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
				const FCompactPoseBoneIndex HandIndex = FindBone(Output.Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
				if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE)
				{
					continue;
				}
				FTransform Upper = IdleCS.GetComponentSpaceTransform(UpperIndex);
				FTransform Lower = IdleCS.GetComponentSpaceTransform(LowerIndex);
				FTransform Hand = IdleCS.GetComponentSpaceTransform(HandIndex);
				const int32 SideIndex = bLeft ? 0 : 1;
				const float Extension = HandExtension[SideIndex];
				const FVector RestHand = OriginalIdleCS.GetComponentSpaceTransform(HandIndex).GetLocation();
				const float Side = RestHand.X >= 0.0 ? 1.0f : -1.0f;
				const FVector Motion = HandMotion[SideIndex];
				FVector Target = FMath::Lerp(GuardTarget, ExtendedTarget, Extension);
				Target.X *= Side;
				Target.X += FMath::Clamp(Motion.X * 0.22, -8.0, 8.0) * BodyStrength;
				Target.Z += FMath::Clamp(Motion.Z * 0.14, -6.0, 6.0) * BodyStrength;
				Target.Y += FMath::Clamp(Motion.Y * 0.20, -7.0, 0.0) * BodyStrength;
				Target.Y -= FMath::Max(0.0f, HandExtension[1 - SideIndex] - Extension) * 3.0f * BodyStrength;
				// Elbows remain below and outside the hands, giving a raised boxing guard.
				const FVector ElbowTarget = FVector(Side * 55.0f, 12.0f, 113.0f) + FullBodyTargetOffset;
				// Preserve the sampled wrist bend relative to the forearm, not its old world orientation.
				AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, ElbowTarget, Target, false, 1.0, 1.0);
				if (Output.Pose.GetParentBoneIndex(HandIndex) == LowerIndex)
				{
					OrientAnatomicalFist(Lower, Hand, WristLocalRotations[bLeft ? 0 : 1],
						HandBases[bLeft ? 0 : 1], Side, FMath::Lerp(GuardThumbInward, HitThumbInward, Extension));
				}
				Upper.NormalizeRotation();
				Lower.NormalizeRotation();
				Hand.NormalizeRotation();
				Changes.Emplace(UpperIndex, Upper);
				Changes.Emplace(LowerIndex, Lower);
				Changes.Emplace(HandIndex, Hand);
			}
			Changes.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
			if (!Changes.IsEmpty())
			{
				IdleCS.LocalBlendCSBoneTransforms(Changes, 1.0f);
			}
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(IdleCS, Output.Pose);
			Output.Pose.NormalizeRotations();
			ApplyLocomotionPose(Output.Pose);
			CaptureHeadDelta(Output.Pose, OriginalIdleCS);
			ApplyFullBodyAim(Output.Pose);
			return true;
		}


		/** Pose-space gait keeps the head, shoulders and guard in the same moving frame.
		 * Actor/capsule motion already supplies travel and jump height; only a small torso
		 * compression/sway is added here. Feet remain in their existing authored pose. */
		/** Cache the source-composited local pose before locomotion/aim. A buffered combo can
		 * replace an attack before its natural end, so endpoint fades alone cannot preserve continuity. */
		void ApplySourceBoxingPose(FPoseContext& Output)
		{
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			if (!bSourceBoxingEligible || PreviousSourcePose.Num() != Bones.GetCompactPoseNumBones()
				|| PreviousSourcePoseBoneSerial != Bones.GetSerialNumber())
			{
				PreviousSourcePose.Reset();
				SourceTransitionPose.Reset();
				bPreviousSourcePoseActive = false;
			}
			if (!bSourceBoxingEligible) { return; }
			const bool bAttackChanged = PreviousSourcePoseAttackSerial != BasicSnapshot.AcceptedSerial
				|| PreviousSourcePosePhase != BasicSnapshot.Phase;
			if (bAttackChanged && bPreviousSourcePoseActive && !PreviousSourcePose.IsEmpty())
			{
				SourceTransitionPose = PreviousSourcePose;
				SourceTransitionStartClock = SourcePoseClock;
				SourceTransitionSeconds = 0.1f;
				if (BasicSnapshot.Phase == ERogue10mBasicBrawlerPhase::Attacking
					&& FMath::IsFinite(BasicSnapshot.AttackDuration) && FMath::IsFinite(BasicSnapshot.HitFraction))
				{
					// Complete the transition before the newly scheduled hit, even after a long frame.
					const float RemainingToHit = BasicSnapshot.AttackDuration * BasicSnapshot.HitFraction - BasicSnapshot.AttackElapsed;
					SourceTransitionSeconds = FMath::Clamp(RemainingToHit * 0.75f, 0.0f, 0.1f);
				}
			}
			BuildSourceBoxingPose(Output);
			bool bTransitionActive = false;
			if (SourceTransitionPose.Num() == Bones.GetCompactPoseNumBones())
			{
				const float Alpha = SourceTransitionSeconds > SMALL_NUMBER
					? SmoothPhase((SourcePoseClock - SourceTransitionStartClock) / SourceTransitionSeconds) : 1.0f;
				bTransitionActive = Alpha < 1.0f;
				for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
				{
					FTransform Blended;
					Blended.Blend(SourceTransitionPose[Bone.GetInt()], Output.Pose[Bone], Alpha);
					Blended.NormalizeRotation();
					Output.Pose[Bone] = Blended;
				}
				if (!bTransitionActive) { SourceTransitionPose.Reset(); }
			}
			PreviousSourcePose.SetNumUninitialized(Bones.GetCompactPoseNumBones());
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				PreviousSourcePose[Bone.GetInt()] = Output.Pose[Bone];
			}
			PreviousSourcePoseBoneSerial = Bones.GetSerialNumber();
			PreviousSourcePoseAttackSerial = BasicSnapshot.AcceptedSerial;
			PreviousSourcePosePhase = BasicSnapshot.Phase;
			bPreviousSourcePoseActive = SnapshotSourceBoxingWeight > KINDA_SMALL_NUMBER || bTransitionActive;
		}

		/** Source playback is presentation only: the existing snapshot remains the sole attack clock. */
		void BuildSourceBoxingPose(FPoseContext& Output)
		{
			const bool bJab = BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::LeftJab
				|| BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightJab;
			if (!bSourceBoxingEligible || !SourceBoxing || !bJab || !BasicSnapshot.bActive
				|| BasicSnapshot.Phase != ERogue10mBasicBrawlerPhase::Attacking
				|| BasicSnapshot.AcceptedSerial <= 0 || !FMath::IsFinite(BasicSnapshot.AttackElapsed)
				|| !FMath::IsFinite(BasicSnapshot.AttackDuration) || BasicSnapshot.AttackDuration <= SMALL_NUMBER
				|| !FMath::IsFinite(BasicSnapshot.HitFraction)) { return; }
			const float Progress = FMath::Clamp(BasicSnapshot.AttackElapsed / BasicSnapshot.AttackDuration, 0.0f, 1.0f);
			const float HitFraction = FMath::Clamp(BasicSnapshot.HitFraction, 0.001f, 0.999f);
			const float SampleTime = Progress <= HitFraction
				? SourceBoxingHitSeconds * (Progress / HitFraction)
				: FMath::Lerp(SourceBoxingHitSeconds, SourceBoxingLength, (Progress - HitFraction) / (1.0f - HitFraction));
			// Natural attack endpoints fade to the native pose; early combo changes crossfade the cached pose above.
			const float BlendWeight = SmoothPhase(Progress / (HitFraction * 0.75f))
				* SmoothPhase((1.0f - Progress) / FMath::Min(0.30f, (1.0f - HitFraction) * 0.75f));
			const bool bMirror = BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightJab;
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			if (!bSourceBoxingCacheValid || SourceBoxingBoneSerial != Bones.GetSerialNumber())
			{
				SourceMirrorBones.SetNum(Bones.GetCompactPoseNumBones());
				SourceReferenceRotations.SetNumUninitialized(Bones.GetCompactPoseNumBones());
				for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
				{
					const FString Name = Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt()).ToString();
					FString MirrorName = Name;
					if (Name.EndsWith(TEXT("_l"))) { MirrorName = Name.LeftChop(2) + TEXT("_r"); }
					else if (Name.EndsWith(TEXT("_r"))) { MirrorName = Name.LeftChop(2) + TEXT("_l"); }
					SourceMirrorBones[Bone.GetInt()] = FindBone(Output.Pose, FName(*MirrorName));
					const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
					SourceReferenceRotations[Bone] = Parent == INDEX_NONE ? Bones.GetRefPoseTransform(Bone).GetRotation()
						: SourceReferenceRotations[Parent] * Bones.GetRefPoseTransform(Bone).GetRotation();
				}
				FPoseContext StartPose(Output);
				StartPose.ResetToRefPose();
				FAnimationPoseData StartData(StartPose);
				SourceBoxing->GetAnimationPose(StartData, FAnimExtractContext(0.0, false));
				SourceRootOrigin = StartPose.Pose[FCompactPoseBoneIndex(0)].GetTranslation();
				SourceBoxingBoneSerial = Bones.GetSerialNumber();
				bSourceBoxingCacheValid = true;
			}
			FPoseContext Source(Output);
			Source.ResetToRefPose();
			FAnimationPoseData SourceData(Source);
			SourceBoxing->GetAnimationPose(SourceData, FAnimExtractContext(SampleTime, false));
			if (bMirror)
			{
				// Manny faces +Y in mesh space. X is the left/right reflection plane; the engine
				// corrects differing left/right reference axes instead of swapping local transforms.
				FAnimationRuntime::MirrorPose(Source.Pose, EAxis::X, SourceMirrorBones, SourceReferenceRotations);
			}
			const FCompactPoseBoneIndex Root(0);
			const FVector Origin = bMirror ? FAnimationRuntime::MirrorVector(SourceRootOrigin, EAxis::X) : SourceRootOrigin;
			Source.Pose[Root].SetTranslation(Source.Pose[Root].GetTranslation() - Origin + Output.Pose[Root].GetTranslation());
			for (const FCompactPoseBoneIndex Bone : Source.Pose.ForEachBoneIndex())
			{
				if (Source.Pose[Bone].ContainsNaN()) { return; }
			}
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				FTransform Blended;
				Blended.Blend(Output.Pose[Bone], Source.Pose[Bone], BlendWeight);
				Blended.NormalizeRotation();
				Output.Pose[Bone] = Blended;
			}
			SnapshotSourceBoxingTime = SampleTime;
			SnapshotSourceBoxingWeight = BlendWeight;
			bSnapshotSourceBoxingMirrored = bMirror;
		}

		void ApplyLocomotionPose(FCompactPose& Pose) const
		{
			if (GaitWeight <= KINDA_SMALL_NUMBER) { return; }
			const FCompactPoseBoneIndex Spine = FindBone(Pose, TEXT("spine_01"));
			if (Spine == INDEX_NONE) { return; }
			FCSPose<FCompactPose> CurrentCS;
			CurrentCS.InitPose(Pose);
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Spine);
			const FTransform ParentCS = Parent != INDEX_NONE ? CurrentCS.GetComponentSpaceTransform(Parent) : FTransform::Identity;
			FTransform SpineCS = CurrentCS.GetComponentSpaceTransform(Spine);
			const float Sway = FMath::Sin(GaitPhase);
			const float Step = FMath::Sin(GaitPhase * 2.0f);
			const FVector Offset = FVector(0.25f * Step, 0.65f * Sway,
				-GaitVerticalAmplitude * FMath::Cos(GaitPhase * 2.0f)) * GaitWeight;
			const FRotator Rotation(0.35f * Step * GaitWeight, 0.0f, 0.55f * Sway * GaitWeight);
			SpineCS.AddToTranslation(MeshToCamera.Inverse().RotateVector(Offset));
			SpineCS.SetRotation((MeshToCamera.Inverse() * Rotation.Quaternion() * MeshToCamera) * SpineCS.GetRotation());
			Pose[Spine] = SpineCS.GetRelativeTransform(ParentCS);
			Pose[Spine].NormalizeRotation();
		}

		/** Snapshot before mouse aim and before the renderer hides neck/head bones. */
		void CaptureHeadDelta(FCompactPose& Pose, FCSPose<FCompactPose>& BaseCS)
		{
			const FCompactPoseBoneIndex Head = FindBone(Pose, TEXT("head"));
			if (Head == INDEX_NONE) { return; }
			FCSPose<FCompactPose> EvaluatedCS;
			EvaluatedCS.InitPose(Pose);
			const FTransform Base = BaseCS.GetComponentSpaceTransform(Head);
			const FTransform Evaluated = EvaluatedCS.GetComponentSpaceTransform(Head);
			SnapshotHeadReferenceLocation = Base.GetLocation();
			SnapshotHeadOffset = MeshToCamera.RotateVector(Evaluated.GetLocation() - Base.GetLocation());
			const FQuat Delta = Evaluated.GetRotation() * Base.GetRotation().Inverse();
			SnapshotHeadRotation = (MeshToCamera * Delta * MeshToCamera.Inverse()).Rotator().GetNormalized();
			if (SnapshotHeadOffset.ContainsNaN() || SnapshotHeadRotation.ContainsNaN())
			{
				SnapshotHeadOffset = FVector::ZeroVector;
				SnapshotHeadRotation = FRotator::ZeroRotator;
			}
		}

		void ApplyFullBodyAim(FCompactPose& Pose) const
		{
			const float AimPitch = FullBodyAimPitch * (bRelaxedIdle ? ReadyWeight : 1.0f);
			if (FMath::IsNearlyZero(AimPitch)) { return; }
			const FCompactPoseBoneIndex Spine = FindBone(Pose, TEXT("spine_01"));
			const FCompactPoseBoneIndex LeftShoulder = FindBone(Pose, TEXT("clavicle_l"));
			const FCompactPoseBoneIndex RightShoulder = FindBone(Pose, TEXT("clavicle_r"));
			if (Spine == INDEX_NONE || LeftShoulder == INDEX_NONE || RightShoulder == INDEX_NONE) { return; }
			TArray<FTransform, TInlineAllocator<256>> ComponentTransforms;
			ComponentTransforms.SetNum(Pose.GetNumBones());
			// Apply after fist IK and after recording animation-driven camera motion.
			// Only spine/shoulder descendants follow aim; pelvis and legs keep their pose.
			for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
			{
				const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
				const FTransform ParentTransform = Parent != INDEX_NONE ? ComponentTransforms[Parent.GetInt()] : FTransform::Identity;
				FTransform Component = Pose[Bone] * ParentTransform;
				if (Bone == Spine || Bone == LeftShoulder || Bone == RightShoulder)
				{
					const float Share = Bone == Spine ? FullBodySpineShare : 1.0f - FullBodySpineShare;
					const FQuat Delta = MeshToCamera.Inverse() * FRotator(AimPitch * Share, 0.0f, 0.0f).Quaternion() * MeshToCamera;
					Component.SetRotation((Delta * Component.GetRotation()).GetNormalized());
					Pose[Bone] = Component.GetRelativeTransform(ParentTransform);
					Pose[Bone].NormalizeRotation();
				}
				ComponentTransforms[Bone.GetInt()] = Component;
			}
		}

		/** Transfer the hips over supported legs instead of translating both feet with the pelvis.
		 * This is bounded pose-space footwork, not terrain IK or root motion. */
		void ApplySupportingFeet(FCompactPose& Pose, FCSPose<FCompactPose>& BaseCS,
			bool bAttacking, bool bJab, float Drive) const
		{
			if (StanceWeight <= KINDA_SMALL_NUMBER || SupportingFeetWeight <= KINDA_SMALL_NUMBER
				|| (bRelaxedIdle && ReadyWeight <= KINDA_SMALL_NUMBER)) { return; }
			FCSPose<FCompactPose> CurrentCS;
			CurrentCS.InitPose(Pose);
			TArray<FBoneTransform, TInlineAllocator<6>> Changes;
			const float Weight = StanceWeight * BodyStrength;
			const bool bLeadJab = bAttacking && bJab && BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::LeftJab;
			const float Transfer = bAttacking ? Drive * Weight : 0.0f;
			for (bool bLeft : { true, false })
			{
				const FCompactPoseBoneIndex UpperIndex = FindBone(Pose, bLeft ? TEXT("thigh_l") : TEXT("thigh_r"));
				const FCompactPoseBoneIndex LowerIndex = FindBone(Pose, bLeft ? TEXT("calf_l") : TEXT("calf_r"));
				const FCompactPoseBoneIndex FootIndex = FindBone(Pose, bLeft ? TEXT("foot_l") : TEXT("foot_r"));
				if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || FootIndex == INDEX_NONE) { continue; }
				FTransform Upper = CurrentCS.GetComponentSpaceTransform(UpperIndex);
				FTransform Lower = CurrentCS.GetComponentSpaceTransform(LowerIndex);
				FTransform Foot = CurrentCS.GetComponentSpaceTransform(FootIndex);
				const FTransform BaseFoot = BaseCS.GetComponentSpaceTransform(FootIndex);
				const float Step = bLeft && bLeadJab ? LeadFootStep * Transfer : 0.0f;
				const FVector FootOffset((bLeft ? 1.0f : -1.0f) * StanceTuning.FootHalfDepth * Weight + Step,
					0.0f, bLeft ? 0.0f : SupportHeelLift * StanceTuning.PivotScale * Transfer);
				const FVector Target = BaseFoot.GetLocation() + MeshToCamera.Inverse().RotateVector(FootOffset);
				const FVector KneePole = BaseCS.GetComponentSpaceTransform(LowerIndex).GetLocation()
					+ MeshToCamera.Inverse().RotateVector(FVector(25.0f, 0.0f, 0.0f));
				// No stretching: preserve the source skeleton's limb lengths.
				AnimationCore::SolveTwoBoneIK(Upper, Lower, Foot, KneePole, Target, false, 1.0, 1.0);
				const float Pivot = bLeft ? 0.0f : -SupportFootPivot * StanceTuning.PivotScale * Transfer;
				const FQuat FootRotation = MeshToCamera.Inverse() * FRotator(0.0f, Pivot, 0.0f).Quaternion() * MeshToCamera;
				Foot.SetRotation((FootRotation * BaseFoot.GetRotation()).GetNormalized());
				Upper.NormalizeRotation(); Lower.NormalizeRotation(); Foot.NormalizeRotation();
				Changes.Emplace(UpperIndex, Upper); Changes.Emplace(LowerIndex, Lower); Changes.Emplace(FootIndex, Foot);
			}
			Changes.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
			if (!Changes.IsEmpty()) { CurrentCS.LocalBlendCSBoneTransforms(Changes, SupportingFeetWeight * StanceWeight * (bRelaxedIdle ? ReadyWeight : 1.0f)); }
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CurrentCS, Pose);
			Pose.NormalizeRotations();
		}

		void EvaluateBasicPose(FPoseContext& Output, FCSPose<FCompactPose>& BaseCS)
		{
			const bool bCharging = BasicSnapshot.bActive && BasicSnapshot.Phase == ERogue10mBasicBrawlerPhase::Charging;
			const bool bAttacking = BasicSnapshot.bActive && BasicSnapshot.Phase == ERogue10mBasicBrawlerPhase::Attacking
				&& BasicSnapshot.AcceptedSerial > 0 && BasicSnapshot.AttackDuration > SMALL_NUMBER;
			const float Progress = bAttacking ? FMath::Clamp(BasicSnapshot.AttackElapsed / BasicSnapshot.AttackDuration, 0.0f, 1.0f) : 0.0f;
			const float Charge = FMath::Clamp(bCharging ? BasicSnapshot.ChargeAlpha : BasicSnapshot.ReleasedChargeAlpha, 0.0f, 1.0f);
			const bool bUppercut = bAttacking && BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightUppercut;
			const bool bHook = bAttacking && BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightHook;
			const bool bJab = bAttacking && (BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::LeftJab
				|| BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightJab);
			// Gameplay supplies the actual rate-adjusted hit time; animation reaches its apex on that frame.
			const float Peak = FMath::Clamp(BasicSnapshot.HitFraction, 0.001f, 0.999f);
			const float AttackAcceleration = FMath::Min(StrikeAcceleration, Peak * 0.95f);
			const float LaunchStart = Peak - AttackAcceleration;
			const float Extension = bAttacking ? StrikeDrive(Progress, Peak, AttackAcceleration, RecoveryFraction) : 0.0f;
			const float BodyDrive = bAttacking ? StrikeDrive(Progress, FMath::Max(0.001f, Peak - BodyLead), AttackAcceleration, RecoveryFraction) : 0.0f;
			const float HeadDrive = bAttacking ? StrikeDrive(Progress, Peak + HeadLag, AttackAcceleration, RecoveryFraction) : 0.0f;
			const FVector GuardHands[2] = {
				GuardTarget + FVector(0.0f, 1.0f, 1.0f) + StanceTuning.LeadGuard * StanceWeight,
				GuardTarget + FVector(1.0f, -0.5f, 0.0f) + StanceTuning.RearGuard * StanceWeight };
			FVector Targets[2] = { GuardHands[0], GuardHands[1] };
			float HandExtension[2] = { 0.0f, 0.0f };
			FRotator DesiredBody = FRotator::ZeroRotator;
			FRotator DesiredHead = FRotator::ZeroRotator;
			FVector DesiredOffset = FVector::ZeroVector;
			FVector IdleHands[2] = { GuardHands[0], GuardHands[1] };
			for (int32 I = 0; I < 2; ++I)
			{
				const FCompactPoseBoneIndex Hand = FindBone(Output.Pose, I == 0 ? TEXT("hand_l") : TEXT("hand_r"));
				if (Hand != INDEX_NONE)
				{
					IdleHands[I] = BaseCS.GetComponentSpaceTransform(Hand).GetLocation();
					BasicHandSides[I] = IdleHands[I].X >= 0.0 ? 1.0f : -1.0f;
					IdleHands[I].X *= BasicHandSides[I];
				}
			}
			const auto CaptureHands = [this, &GuardHands, &IdleHands](FVector (&Captured)[2])
			{
				for (int32 I = 0; I < 2; ++I)
				{
					Captured[I] = bBasicHandsInitialized
						? CurrentBasicHandTargets[I]
						: (bRelaxedIdle ? IdleHands[I] : GuardHands[I]);
					// Retain a hook that crossed the centreline when another input interrupts recovery.
					if (bBasicHandsInitialized) { Captured[I].X *= BasicHandSides[I]; }
				}
			};
			if (bCharging && !bWasBasicCharging)
			{
				CaptureHands(ChargeStartHandTargets);
				ChargeStartHookElbowWeight = CurrentHookElbowWeight;
				ChargePresentationAge = 0.0f;
			}
			if (bAttacking && LastBasicAttackSerial != BasicSnapshot.AcceptedSerial)
			{
				CaptureHands(AttackStartHandTargets);
				AttackStartHookElbowWeight = CurrentHookElbowWeight;
				bAttackRaisedFromIdle = bRelaxedIdle && ReadyWeight < 0.999f;
				LastBasicAttackSerial = BasicSnapshot.AcceptedSerial;
			}
			if (bCharging)
			{
				ChargePresentationAge += EvaluationDeltaSeconds;
				const float Enter = bRelaxedIdle ? ReadyTravelPhase(ChargePresentationAge / 0.20f) : SmoothPhase(ChargePresentationAge / 0.16f);
				const float Load = SmoothPhase(Charge);
				// A short press barely loads the straight. Continued holding coils the right shoulder for a hook.
				const FVector ChargeTarget = FMath::Lerp(GuardHands[1] + FVector(1.0f, -2.0f, 0.0f), HookLoadedTarget, Load);
				Targets[1] = FMath::Lerp(ChargeStartHandTargets[1], ChargeTarget, Enter);
				Targets[0] = FMath::Lerp(ChargeStartHandTargets[0], GuardHands[0] + FVector(-1.0f, -1.5f, 1.0f) * Load, Enter);
				DesiredBody = FRotator(-2.0f * Load, ChargeBodyYaw * (0.20f + 0.80f * Load), 1.0f * Load) * Enter;
				DesiredHead = FRotator(-1.2f * Load, 3.0f * Load, 0.35f * Load) * Enter;
				DesiredOffset = FVector(-0.6f * Load, 0.5f * Load, -1.2f * Load) * Enter;
			}
			else if (bAttacking)
			{
				const int32 ActiveHand = BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::LeftJab ? 0 : 1;
				const int32 GuardHand = 1 - ActiveHand;
				FVector Loaded = GuardHands[ActiveHand] + FVector(1.5f, bJab ? -2.5f : -5.0f, -1.0f);
				FVector Apex = bJab ? JabTarget : StraightTarget;
				if (bHook)
				{
					Loaded = FMath::Lerp(GuardHands[ActiveHand], HookLoadedTarget, 0.6f + 0.4f * Charge);
					Apex = HookTarget;
				}
				if (bUppercut)
				{
					Loaded = GuardHands[ActiveHand] + ChargeHandOffset * (0.55f + 0.45f * Charge);
					Apex = UppercutTarget + FVector(-1.0f, 1.0f, 2.0f) * Charge;
				}
				if (Progress < LaunchStart)
				{
					Targets[ActiveHand] = FMath::Lerp(AttackStartHandTargets[ActiveHand], Loaded, SmoothPhase(Progress / LaunchStart));
				}
				else if (Progress <= Peak)
				{
					Targets[ActiveHand] = FMath::Lerp(Loaded, Apex, Extension);
					if (bHook)
					{
						const float U = 1.0f - Extension;
						Targets[ActiveHand] = Loaded * (U * U) + HookArcControl * (2.0f * U * Extension)
							+ Apex * (Extension * Extension);
					}
					// The uppercut scoops inward and forward before rising; its apex remains bounded.
					if (bUppercut)
					{
						const float Scoop = FMath::Sin(PI * (Progress - LaunchStart) / AttackAcceleration);
						Targets[ActiveHand] += FVector(-2.0f, 3.0f, -2.0f) * Scoop;
					}
				}
				else
				{
					if (bHook)
					{
						// Follow through across the centreline before folding back outside the outgoing sweep.
						const float T = SmoothPhase((Progress - Peak) / RecoveryFraction);
						const float U = 1.0f - T;
						const FVector FollowThrough = Apex + FVector(-6.0f, -2.0f, 0.0f);
						const FVector Fold = GuardHands[ActiveHand] + FVector(10.0f, -6.0f, 3.0f);
						Targets[ActiveHand] = Apex * (U * U * U) + FollowThrough * (3.0f * U * U * T)
							+ Fold * (3.0f * U * T * T) + GuardHands[ActiveHand] * (T * T * T);
					}
					else
					{
						Targets[ActiveHand] = RecoveryArc(Apex, GuardHands[ActiveHand], (Progress - Peak) / RecoveryFraction, HandRecoveryArc);
					}
				}
				if (bAttackRaisedFromIdle && Progress <= Peak)
				{
					// A relaxed fist has a much longer route than a ready guard. Use the whole
					// existing pre-hit window, rather than finishing a late raise then teleporting to the apex.
					Targets[ActiveHand] = FMath::Lerp(AttackStartHandTargets[ActiveHand], Apex, ReadyTravelPhase(Progress / Peak));
				}
				const FVector CounterGuardOffset = (bHook ? HookGuardOffset
					: FVector(-1.0f, ActiveHand == 0 ? -2.0f : -3.0f, 1.5f))
					+ FVector(0.0f, -1.0f, 1.0f) * StanceWeight;
				const FVector CounterGuard = GuardHands[GuardHand] + CounterGuardOffset * BodyDrive;
				Targets[GuardHand] = FMath::Lerp(AttackStartHandTargets[GuardHand], CounterGuard,
					bAttackRaisedFromIdle ? ReadyTravelPhase(Progress / Peak) : SmoothPhase(Progress / 0.14f));
				HandExtension[ActiveHand] = Extension;
				// Shoulder transfer leads the fist; the head follows later with a smaller rotation to preserve aim.
				const float Prepare = Progress < LaunchStart ? FMath::Sin(PI * Progress / LaunchStart) : 0.0f;
				const float BodyTransfer = BodyDrive - 0.16f * Prepare;
				if (bJab)
				{
					const float JabSide = ActiveHand == 0 ? 1.0f : -1.0f;
					DesiredBody = FRotator(-1.0f, 8.0f * JabSide, -1.0f * JabSide) * BodyTransfer;
					DesiredHead = FRotator(-1.4f, 2.0f * JabSide, -0.4f * JabSide) * HeadDrive;
					// The source's step-in left straight transfers the centre before the fist.
					// Keep that intent bounded in pose space; gameplay/capsule displacement is unchanged.
					DesiredOffset = FVector(0.8f + (ActiveHand == 0 ? 2.0f : 1.0f) * StanceWeight,
						-0.6f * JabSide, 0.1f) * BodyTransfer;
				}
				else if (bHook)
				{
					const float LoadedYaw = ChargeBodyYaw * (0.20f + 0.80f * Charge);
					const float Unwind = Progress < Peak - BodyLead
						? FMath::Lerp(LoadedYaw, -HookBodyYaw, BodyDrive) : -HookBodyYaw * BodyDrive;
					DesiredBody = FRotator(-1.0f * BodyDrive, Unwind, 2.0f * BodyDrive);
					DesiredHead = FRotator(-0.8f, -2.4f, 0.8f) * HeadDrive;
					DesiredOffset = FVector(1.3f, 1.0f, -0.15f) * BodyDrive;
				}
				else if (!bUppercut)
				{
					DesiredBody = FRotator(-1.8f, -16.0f, 1.5f) * BodyTransfer;
					DesiredHead = FRotator(-1.2f, -1.8f, 0.5f) * HeadDrive;
					DesiredOffset = FVector(StraightBodyAdvance, 0.8f, -0.2f) * BodyTransfer;
				}
				else
				{
					const float LoadedYaw = ChargeBodyYaw * (0.45f + 0.55f * Charge);
					const float Unwind = Progress < Peak - BodyLead
						? FMath::Lerp(LoadedYaw, -12.0f - 4.0f * Charge, BodyDrive)
						: (-12.0f - 4.0f * Charge) * BodyDrive;
					DesiredBody = FRotator(4.0f * BodyDrive, Unwind, -1.5f * BodyDrive);
					DesiredHead = FRotator((2.0f + Charge) * HeadDrive, -2.0f * HeadDrive, -0.5f * HeadDrive);
					DesiredOffset = FVector(1.0f, -0.6f, 1.6f) * BodyDrive;
				}
			}
			// Only an accepted buffered input prepares the following jab; idle returns to the normal guard.
			const bool bPreparingJab = bAttacking && BasicSnapshot.bComboBuffered
				&& (!bAttackRaisedFromIdle || Progress >= Peak)
				&& (BasicSnapshot.NextAttack == ERogue10mBasicBrawlerAttack::LeftJab
					|| BasicSnapshot.NextAttack == ERogue10mBasicBrawlerAttack::RightJab);
			const float PrepareAlpha = 1.0f - FMath::Exp(-24.0f * EvaluationDeltaSeconds);
			ComboPrepareWeight = FMath::Lerp(ComboPrepareWeight, bPreparingJab ? 1.0f : 0.0f, PrepareAlpha);
			if (bPreparingJab)
			{
				const int32 NextHand = BasicSnapshot.NextAttack == ERogue10mBasicBrawlerAttack::LeftJab ? 0 : 1;
				const FVector NextLoaded = GuardHands[NextHand] + FVector(1.5f, -2.5f, -1.0f);
				Targets[NextHand] = FMath::Lerp(Targets[NextHand], NextLoaded, ComboPrepareWeight);
			}
			// Capture the elbow pole as well as the fist. Releasing just below full charge must not
			// drop a coiled elbow in one frame when the accepted attack is a straight.
			if (bCharging)
			{
				CurrentHookElbowWeight = FMath::Lerp(ChargeStartHookElbowWeight, SmoothPhase(Charge) * 0.5f,
					SmoothPhase(ChargePresentationAge / 0.16f));
			}
			else if (bAttacking)
			{
				const float AttackPole = bHook ? (Progress <= Peak ? FMath::Lerp(0.5f, 1.0f, Extension) : Extension) : 0.0f;
				CurrentHookElbowWeight = FMath::Lerp(AttackStartHookElbowWeight, AttackPole,
					SmoothPhase(Progress / FMath::Min(LaunchStart, 0.12f)));
			}
			else
			{
				CurrentHookElbowWeight *= FMath::Exp(-28.0f * EvaluationDeltaSeconds);
			}
			bWasBasicCharging = bCharging;

			const float Takeoff = TakeoffAge < 0.26f ? FMath::Sin(PI * TakeoffAge / 0.26f) : 0.0f;
			const float Landing = LandingAge < 0.32f ? FMath::Sin(PI * LandingAge / 0.32f) * LandingStrength : 0.0f;
			const float JumpHandZ = (-3.0f * Takeoff + 3.0f * AirInertia - 5.0f * Landing) * JumpStrength;
			for (FVector& Target : Targets)
			{
				Target.Z += JumpHandZ;
				Target.Y -= (1.5f * FMath::Abs(AirInertia) + 2.0f * Landing) * JumpStrength;
			}
			DesiredOffset.Z += (-0.5f * Takeoff + 0.8f * AirInertia - 1.8f * Landing) * JumpStrength;
			DesiredHead.Pitch += (-0.8f * Takeoff + 0.6f * AirInertia - 2.0f * Landing) * JumpStrength;
			DesiredBody.Pitch += (-1.0f * Takeoff - 3.0f * Landing) * JumpStrength;
			SnapshotJumpWeight = FMath::Clamp(Takeoff + Landing + FMath::Abs(AirInertia), 0.0f, 1.0f) * JumpStrength;

			DesiredBody.Yaw *= FMath::Lerp(1.0f, StanceTuning.TorsoTurnScale, StanceWeight);
			if (!bFalling)
			{
				DesiredOffset.Z -= StanceTuning.HipCompression * StanceWeight * BodyDrive;
			}
			const float BodyAlpha = 1.0f - FMath::Exp(-22.0f * EvaluationDeltaSeconds);
			SmoothedBodyRotation = FMath::Lerp(SmoothedBodyRotation, DesiredBody * BodyStrength, BodyAlpha);
			SmoothedHeadRotation = FMath::Lerp(SmoothedHeadRotation, DesiredHead * BodyStrength, BodyAlpha);
			SmoothedBodyOffset = FMath::Lerp(SmoothedBodyOffset, DesiredOffset * BodyStrength, BodyAlpha);
			if (BodyStrength <= KINDA_SMALL_NUMBER)
			{
				SmoothedBodyRotation = SmoothedHeadRotation = FRotator::ZeroRotator;
				SmoothedBodyOffset = FVector::ZeroVector;
			}
			// Settle to exact zero so the camera consumer's idle gate does not retain numerical noise.
			if (SmoothedBodyRotation.IsNearlyZero(0.001)) { SmoothedBodyRotation = FRotator::ZeroRotator; }
			if (SmoothedHeadRotation.IsNearlyZero(0.001)) { SmoothedHeadRotation = FRotator::ZeroRotator; }
			if (SmoothedBodyOffset.IsNearlyZero(0.001)) { SmoothedBodyOffset = FVector::ZeroVector; }
			const FTransform EvaluatedHead = ApplyBasicBodyPose(Output.Pose, BaseCS, MeshToCamera,
				SmoothedBodyRotation, SmoothedHeadRotation, SmoothedBodyOffset,
				FMath::Lerp(0.08f, StanceTuning.PelvisYawShare, StanceWeight));
			ApplySupportingFeet(Output.Pose, BaseCS, bAttacking, bJab, BodyDrive);
			const FCompactPoseBoneIndex HeadIndex = FindBone(Output.Pose, TEXT("head"));
			if (HeadIndex != INDEX_NONE && BodyStrength > KINDA_SMALL_NUMBER)
			{
				const FTransform BaseHead = BaseCS.GetComponentSpaceTransform(HeadIndex);
				SnapshotHeadOffset = MeshToCamera.RotateVector(EvaluatedHead.GetLocation() - BaseHead.GetLocation());
				const FQuat HeadDelta = EvaluatedHead.GetRotation() * BaseHead.GetRotation().Inverse();
				SnapshotHeadRotation = (MeshToCamera * HeadDelta * MeshToCamera.Inverse()).Rotator().GetNormalized();
				SnapshotOffset = (SnapshotHeadOffset * 0.20f).GetClampedToMaxSize(2.0);
				SnapshotRotation = FRotator(
					FMath::Clamp(SnapshotHeadRotation.Pitch * 0.65, -1.5, 1.5),
					FMath::Clamp(SnapshotHeadRotation.Yaw * 0.65, -1.5, 1.5),
					FMath::Clamp(SnapshotHeadRotation.Roll * 0.5, -0.8, 0.8));
				if (!SnapshotOffset.IsNearlyZero(0.0001) || !SnapshotRotation.IsNearlyZero(0.0001)) { SnapshotWeight = 1.0f; }
			}

			FCSPose<FCompactPose> BodyCS;
			BodyCS.InitPose(Output.Pose);
			TArray<FBoneTransform, TInlineAllocator<6>> Changes;
			const float HandAlpha = 1.0f - FMath::Exp(-28.0f * EvaluationDeltaSeconds);
			for (const bool bLeft : { true, false })
			{
				const int32 I = bLeft ? 0 : 1;
				const FCompactPoseBoneIndex UpperIndex = FindBone(Output.Pose, bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
				const FCompactPoseBoneIndex LowerIndex = FindBone(Output.Pose, bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
				const FCompactPoseBoneIndex HandIndex = FindBone(Output.Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
				if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE) { continue; }
				const float Side = BaseCS.GetComponentSpaceTransform(HandIndex).GetLocation().X >= 0.0 ? 1.0f : -1.0f;
				BasicHandSides[I] = Side;
				Targets[I].X *= Side;
				const FVector IdleTarget = BaseCS.GetComponentSpaceTransform(HandIndex).GetLocation();
				if (bRelaxedIdle && !bAttacking && !bCharging) { Targets[I] = FMath::Lerp(IdleTarget, Targets[I], ReadyWeight); }
				if (!bBasicHandsInitialized)
				{
					CurrentBasicHandTargets[I] = bRelaxedIdle ? IdleTarget
						: FVector(GuardHands[I].X * Side, GuardHands[I].Y, GuardHands[I].Z);
				}
				// The authored path includes its own continuous anticipation/recovery. Filtering it again
				// would delay full extension beyond the actual damage frame. Cancellation/charge still blend.
				CurrentBasicHandTargets[I] = FMath::Lerp(CurrentBasicHandTargets[I], Targets[I], bAttacking ? 1.0f : HandAlpha);
				FTransform Upper = BodyCS.GetComponentSpaceTransform(UpperIndex);
				FTransform Lower = BodyCS.GetComponentSpaceTransform(LowerIndex);
				FTransform Hand = BodyCS.GetComponentSpaceTransform(HandIndex);
				const bool bUppercutHand = !bLeft && bAttacking && BasicSnapshot.Attack == ERogue10mBasicBrawlerAttack::RightUppercut;
				const float ElbowRise = bUppercutHand ? Extension : 0.0f;
				FVector ElbowTarget = FMath::Lerp(FVector(Side * 55.0f, 12.0f, 113.0f),
					FVector(Side * 43.0f, 20.0f, 125.0f), ElbowRise);
				if (!bLeft)
				{
					ElbowTarget = FMath::Lerp(ElbowTarget, FVector(Side * 59.0f, 32.0f, 142.0f), CurrentHookElbowWeight);
				}
				// Compact elbows protect the ribs; the punching elbow opens with extension.
				// The hook keeps its horizontal pole rather than becoming a downward hammer.
				const float GuardTuck = StanceTuning.ElbowTuck * StanceWeight * (1.0f - HandExtension[I]);
				ElbowTarget.X -= Side * GuardTuck * (bLeft ? 1.0f : 1.0f - CurrentHookElbowWeight);
				ElbowTarget += FullBodyTargetOffset;
				if (bRelaxedIdle) { ElbowTarget = FMath::Lerp(Lower.GetLocation(), ElbowTarget, ReadyWeight); }
				// Keep a bend at full extension; a locked elbow magnifies the source skin's corrective-bone limits.
				const double ReachLimit = ((Lower.GetLocation() - Upper.GetLocation()).Size()
					+ (Hand.GetLocation() - Lower.GetLocation()).Size()) * (bRelaxedIdle ? FMath::Lerp(1.0, 0.94, static_cast<double>(ReadyWeight)) : 0.94);
				const FVector ReachTarget = Upper.GetLocation()
					+ (CurrentBasicHandTargets[I] - Upper.GetLocation()).GetClampedToMaxSize(ReachLimit);
				const FQuat IdleHandRotation = Hand.GetRotation();
				AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, ElbowTarget, ReachTarget, false, 1.0, 1.0);
				const FQuat SolvedForearmRotation = Lower.GetRotation();
				if (Output.Pose.GetParentBoneIndex(HandIndex) == LowerIndex)
				{
					OrientAnatomicalFist(Lower, Hand, WristLocalRotations[I], HandBases[I], Side,
						FMath::Lerp(GuardThumbInward, (!bLeft && bHook) ? HookThumbInward : HitThumbInward, HandExtension[I]));
				}
				if (bRelaxedIdle)
				{
					Lower.SetRotation(FQuat::Slerp(SolvedForearmRotation, Lower.GetRotation(), ReadyWeight).GetNormalized());
					Hand.SetRotation(FQuat::Slerp(IdleHandRotation, Hand.GetRotation(), ReadyWeight).GetNormalized());
					// Capture the actual bounded endpoint once, without a second whole-arm readiness blend.
					CurrentBasicHandTargets[I] = Hand.GetLocation();
				}
				Upper.NormalizeRotation(); Lower.NormalizeRotation(); Hand.NormalizeRotation();
				Changes.Emplace(UpperIndex, Upper); Changes.Emplace(LowerIndex, Lower); Changes.Emplace(HandIndex, Hand);
			}
			bBasicHandsInitialized = true;
			Changes.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
			if (!Changes.IsEmpty()) { BodyCS.LocalBlendCSBoneTransforms(Changes, 1.0f); }
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(BodyCS, Output.Pose);
			Output.Pose.NormalizeRotations();
		}

		// Borrowed from the anim instance's UPROPERTY strong references, copied in PreUpdate.
		const UAnimSequence* Idle = nullptr;
		const UAnimSequence* FistPose = nullptr;
		const UAnimSequence* SourceBoxing = nullptr;
		static constexpr float SourceBoxingHitSeconds = 13.0f / 30.0f;
		bool bSourceBoxingEligible = false;
		bool bSourceBoxingCacheValid = false;
		uint16 SourceBoxingBoneSerial = 0;
		float SourceBoxingLength = 0.0f;
		float SourcePoseClock = 0.0f;
		float SourceTransitionStartClock = 0.0f;
		float SourceTransitionSeconds = 0.1f;
		TArray<FTransform> PreviousSourcePose;
		TArray<FTransform> SourceTransitionPose;
		uint16 PreviousSourcePoseBoneSerial = 0;
		int32 PreviousSourcePoseAttackSerial = 0;
		ERogue10mBasicBrawlerPhase PreviousSourcePosePhase = ERogue10mBasicBrawlerPhase::Idle;
		bool bPreviousSourcePoseActive = false;
		TArray<FCompactPoseBoneIndex> SourceMirrorBones;
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> SourceReferenceRotations;
		FVector SourceRootOrigin = FVector::ZeroVector;
		float SnapshotSourceBoxingWeight = 0.0f;
		float SnapshotSourceBoxingTime = 0.0f;
		bool bSnapshotSourceBoxingMirrored = false;
		FVector GuardTarget;
		FVector ExtendedTarget;
		float PunchTravel = 32.0f;
		float BodyStrength = 0.65f;
		FQuat MeshToCamera = FQuat::Identity;
		float GroundSpeed = 0.0f;
		float GaitStrength = 1.0f;
		float GaitReferenceSpeed = 600.0f;
		float GaitStrideLength = 340.0f;
		float GaitVerticalAmplitude = 1.8f;
		float GaitWeight = 0.0f;
		float GaitPhase = 0.0f;
		float FullBodyAimPitch = 0.0f;
		float FullBodySpineShare = 0.25f;
		FVector FullBodyTargetOffset = FVector::ZeroVector;
		float FullBodyGuardPullback = 0.0f;
		float FullBodyGuardInward = 0.0f;
		FVector SnapshotOffset = FVector::ZeroVector;
		FRotator SnapshotRotation = FRotator::ZeroRotator;
		float SnapshotWeight = 0.0f;
		float GuardThumbInward = 15.0f;
		float HitThumbInward = 35.0f;
		FHandAnatomicalBasis HandBases[2];
		FQuat WristLocalRotations[2];
		FVector CurlDegrees = FVector(100.0f, 100.0f, 75.0f);
		float SlotWeight = 0.0f;
		float SourceWeight = 1.0f;
		float TotalWeight = 0.0f;
		float IdleTime = 0.0f;
		TArray<FCompactPoseBoneIndex> FingerBones;
		TArray<FTransform> FingerTransforms;
		uint16 CachedBoneSerial = 0;
		bool bFingerCacheValid = false;

		FBrawlerStanceTuning StanceTuning;
		float StanceWeight = 0.0f;
		float SupportingFeetWeight = 0.0f;
		float LeadFootStep = 6.0f;
		float SupportFootPivot = 12.0f;
		float SupportHeelLift = 2.0f;
		bool bRelaxedIdle = false;
		float CombatReadyProgress = 0.0f;
		float ReadyWeight = 0.0f;
		float ReadyIdleAge = 100.0f;
		float ReadyRaiseSeconds = 0.16f;
		float ReadyHoldSeconds = 0.9f;
		float ReadyLowerSeconds = 0.45f;
		FRogue10mBasicBrawlerSnapshot BasicSnapshot;
		bool bBasicPose = false;
		bool bBasicHandsInitialized = false;
		bool bFalling = false;
		bool bPreviouslyFalling = false;
		float VerticalSpeed = 0.0f;
		float PreviousVerticalSpeed = 0.0f;
		float AirInertia = 0.0f;
		float TakeoffAge = 1.0f;
		float LandingAge = 1.0f;
		float LandingStrength = 0.0f;
		float JumpStrength = 1.0f;
		float ChargeBodyYaw = 18.0f;
		float StrikeAcceleration = 0.14f;
		float RecoveryFraction = 0.30f;
		float BodyLead = 0.04f;
		float HeadLag = 0.035f;
		float HandRecoveryArc = 5.0f;
		int32 LastBasicAttackSerial = 0;
		bool bWasBasicCharging = false;
		bool bAttackRaisedFromIdle = false;
		float ChargePresentationAge = 0.0f;
		float ComboPrepareWeight = 0.0f;
		float CurrentHookElbowWeight = 0.0f;
		float AttackStartHookElbowWeight = 0.0f;
		float ChargeStartHookElbowWeight = 0.0f;
		FVector AttackStartHandTargets[2] = { FVector::ZeroVector, FVector::ZeroVector };
		FVector ChargeStartHandTargets[2] = { FVector::ZeroVector, FVector::ZeroVector };
		float EvaluationDeltaSeconds = 0.0f;
		FVector JabTarget = FVector::ZeroVector;
		FVector StraightTarget = FVector::ZeroVector;
		float StraightBodyAdvance = 7.0f;
		FVector HookGuardOffset = FVector::ZeroVector;
		FVector HookTarget = FVector::ZeroVector;
		FVector HookLoadedTarget = FVector::ZeroVector;
		FVector HookArcControl = FVector::ZeroVector;
		float HookBodyYaw = 24.0f;
		float HookThumbInward = 12.0f;
		float BasicHandSides[2] = { 1.0f, 1.0f };
		FVector UppercutTarget = FVector::ZeroVector;
		FVector ChargeHandOffset = FVector::ZeroVector;
		FVector CurrentBasicHandTargets[2] = { FVector::ZeroVector, FVector::ZeroVector };
		FRotator SmoothedBodyRotation = FRotator::ZeroRotator;
		FRotator SmoothedHeadRotation = FRotator::ZeroRotator;
		FVector SmoothedBodyOffset = FVector::ZeroVector;
		FRotator SnapshotHeadRotation = FRotator::ZeroRotator;
		FVector SnapshotHeadOffset = FVector::ZeroVector;
		FVector SnapshotHeadReferenceLocation = FVector::ZeroVector;
		float SnapshotJumpWeight = 0.0f;

	};
}

URogue10mFistAnimInstance::URogue10mFistAnimInstance()
{
	SourceBoxingAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny.A_BoxingJab_Manny")));
	IdleAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")));
	FistAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT(
		"/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01")));
}

void URogue10mFistAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	BodyMotionOffset = FVector::ZeroVector;
	BodyMotionRotation = FRotator::ZeroRotator;
	BodyMotionWeight = 0.0f;
	HeadPoseDeltaOffset = FVector::ZeroVector;
	HeadPoseReferenceLocation = FVector::ZeroVector;
	HeadPoseDeltaRotation = FRotator::ZeroRotator;
	JumpPresentationWeight = 0.0f;
	LocomotionPresentationWeight = 0.0f;
	CombatReadyWeight = 0.0f;
	LoadedIdleAnimation = IdleAnimation.LoadSynchronous();
	LoadedFistAnimation = FistAnimation.LoadSynchronous();
	LoadedSourceBoxingAnimation = SourceBoxingAnimation.LoadSynchronous();
	SourceBoxingWeight = SourceBoxingTime = 0.0f;
	bSourceBoxingMirrored = false;
}


void URogue10mFistAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	// All parallel evaluation work has completed before this game-thread callback.
	const FFistAnimProxy& Proxy = GetProxyOnGameThread<FFistAnimProxy>();
	BodyMotionOffset = Proxy.SnapshotOffset;
	BodyMotionRotation = Proxy.SnapshotRotation;
	BodyMotionWeight = Proxy.SnapshotWeight;
	HeadPoseDeltaOffset = Proxy.SnapshotHeadOffset;
	HeadPoseReferenceLocation = Proxy.SnapshotHeadReferenceLocation;
	HeadPoseDeltaRotation = Proxy.SnapshotHeadRotation;
	JumpPresentationWeight = Proxy.SnapshotJumpWeight;
	LocomotionPresentationWeight = Proxy.GaitWeight;
	CombatReadyWeight = Proxy.ReadyWeight;
	SourceBoxingWeight = Proxy.SnapshotSourceBoxingWeight;
	SourceBoxingTime = Proxy.SnapshotSourceBoxingTime;
	bSourceBoxingMirrored = Proxy.bSnapshotSourceBoxingMirrored;
}

FAnimInstanceProxy* URogue10mFistAnimInstance::CreateAnimInstanceProxy()
{
	return new FFistAnimProxy(this);
}

void URogue10mFistAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
