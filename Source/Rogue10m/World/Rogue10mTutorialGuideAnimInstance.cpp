// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialGuideAnimInstance.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"

namespace
{
	struct FGuidePoseSnapshot
	{
		TArray<FTransform> Transforms;
		TArray<FBoneIndexType> MeshBones;
		FBlendedHeapCurve Curve;
		UE::Anim::FHeapAttributeContainer Attributes;

		void Capture(const FPoseContext& Source)
		{
			Source.Pose.CopyBonesTo(Transforms);
			MeshBones = Source.Pose.GetBoneContainer().GetBoneIndicesArray();
			Curve.CopyFrom(Source.Curve);
			Attributes.CopyFrom(Source.CustomAttributes);
		}

		void Apply(const FPoseContext& Destination, FPoseContext& Target) const
		{
			const auto& RequiredMeshBones = Target.Pose.GetBoneContainer().GetBoneIndicesArray();
			if (MeshBones == RequiredMeshBones)
			{
				Target.Pose.CopyBonesFrom(Transforms);
				Target.CustomAttributes.CopyFrom(Attributes);
			}
			else
			{
				// Mesh indices stay stable when stepping between compact LOD bone sets.
				for (const FCompactPoseBoneIndex Bone : Target.Pose.ForEachBoneIndex())
				{
					const int32 PreviousIndex = MeshBones.IndexOfByKey(RequiredMeshBones[Bone.GetInt()]);
					Target.Pose[Bone] = PreviousIndex == INDEX_NONE ? Destination.Pose[Bone] : Transforms[PreviousIndex];
				}
				// Custom attributes carry compact indices. Use the destination's current layout.
				Target.CustomAttributes.CopyFrom(Destination.CustomAttributes);
			}
			Target.Curve.CopyFrom(Curve);
		}
	};

	class FGuideSequenceNode final : public FAnimNode_Base
	{
	public:
		FGuideSequenceNode() { SequencePlayer.SetLoopAnimation(true); }

		void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			SequencePlayer.Initialize_AnyThread(Context);
			ResetBlend();
		}

		void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			SequencePlayer.CacheBones_AnyThread(Context);
		}

		void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			SequencePlayer.Update_AnyThread(Context);
			Elapsed = FMath::Min(Duration, Elapsed + Context.GetDeltaTime());
		}

		void Evaluate_AnyThread(FPoseContext& Output) override
		{
			SequencePlayer.Evaluate_AnyThread(Output);
			if (bCaptureSource)
			{
				// Capture what was displayed, including any interrupted transition.
				Swap(BlendSource, LastOutput);
				bCaptureSource = false;
				Elapsed = 0.0f;
			}
			if (Elapsed < Duration)
			{
				FPoseContext Source(Output);
				BlendSource.Apply(Output, Source);
				FAnimationPoseData DestinationData(Output);
				const FAnimationPoseData SourceData(Source);
				FAnimationRuntime::BlendTwoPosesTogetherInPlace(DestinationData, SourceData,
					FMath::SmoothStep(0.0f, 1.0f, Elapsed / Duration));
			}
			LastOutput.Capture(Output);
			bHasOutput = true;
		}

		void SetPlayback(UAnimSequence* Sequence, float Rate, float BlendSeconds)
		{
			if (SequencePlayer.GetSequence() != Sequence)
			{
				Duration = SequencePlayer.GetSequence() && bHasOutput ? BlendSeconds : 0.0f;
				bCaptureSource = Duration > 0.0f;
				SequencePlayer.SetSequence(Sequence);
				SequencePlayer.SetAccumulatedTime(0.0f);
			}
			SequencePlayer.SetPlayRate(Rate);
		}

		float GetPlaybackPosition() const { return SequencePlayer.GetAccumulatedTime(); }

	private:
		void ResetBlend()
		{
			// Reinitializing the node may replace the source mesh and its bone indices.
			bHasOutput = false;
			bCaptureSource = false;
			Duration = 0.0f;
			Elapsed = 0.0f;
		}

		FAnimNode_SequencePlayer_Standalone SequencePlayer;
		FGuidePoseSnapshot BlendSource;
		FGuidePoseSnapshot LastOutput;
		float Duration = 0.0f;
		float Elapsed = 0.0f;
		bool bHasOutput = false;
		bool bCaptureSource = false;
	};

	class FTutorialGuideAnimProxy final : public FAnimInstanceProxy
	{
	public:
		explicit FTutorialGuideAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
		FAnimNode_Base* GetCustomRootNode() override { return &GuideSequence; }
		FGuideSequenceNode GuideSequence;
	};
}

void URogue10mTutorialGuideAnimInstance::SetPlayback(UAnimSequence* Sequence, float Rate, float BlendSeconds)
{
	if (PlaybackSequence == Sequence && FMath::IsNearlyEqual(PlaybackRate, Rate)) { return; }

	// Game-thread proxy access completes any in-flight parallel evaluation before
	// replacing the old clip. Keep the evaluated clip alive via the UPROPERTY.
	GetProxyOnGameThread<FTutorialGuideAnimProxy>().GuideSequence.SetPlayback(Sequence, Rate, BlendSeconds);
	PlaybackSequence = Sequence;
	PlaybackRate = Rate;
}

float URogue10mTutorialGuideAnimInstance::GetPlaybackPosition() const
{
	return GetProxyOnGameThread<FTutorialGuideAnimProxy>().GuideSequence.GetPlaybackPosition();
}

FAnimInstanceProxy* URogue10mTutorialGuideAnimInstance::CreateAnimInstanceProxy()
{
	return new FTutorialGuideAnimProxy(this);
}

void URogue10mTutorialGuideAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}
