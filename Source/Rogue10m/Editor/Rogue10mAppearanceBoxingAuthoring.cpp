// Copyright Epic Games, Inc. All Rights Reserved.
// Bake the approved source clip for the ordinary locomotion AnimBP montage slots.
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MemStack.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"

namespace Rogue10mAppearanceBoxingAuthoring
{
static const TCHAR* Root = TEXT("/Game/Rogue10m/Animation/Combat/Boxing/");
struct FTrack { TArray<FVector3f> Positions; TArray<FQuat4f> Rotations; TArray<FVector3f> Scales; };
static void BuildComponentPose(const FCompactPose& Pose, const FBoneContainer& Bones, TArray<FTransform>& OutPose)
{
    OutPose.SetNum(Bones.GetCompactPoseNumBones());
    for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
    {
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
        OutPose[Bone.GetInt()] = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone] * OutPose[Parent.GetInt()];
    }
}

// The mirrored arms and the preserved stance no longer share the source hand IK tracks.
// Bake IK goals from the FINAL pose, resolving parents before children (gun -> hands).
static bool SyncHandIK(FCompactPose& Pose, const FBoneContainer& Bones)
{
    const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
    TArray<FCompactPoseBoneIndex> TargetHands;
    TargetHands.Init(FCompactPoseBoneIndex(INDEX_NONE), Bones.GetCompactPoseNumBones());
    for (const TPair<FName, FName>& Pair : {
        TPair<FName, FName>(TEXT("ik_hand_gun"), TEXT("hand_r")),
        TPair<FName, FName>(TEXT("ik_hand_l"), TEXT("hand_l")),
        TPair<FName, FName>(TEXT("ik_hand_r"), TEXT("hand_r")) })
    {
        const int32 IKIndex = Ref.FindBoneIndex(Pair.Key);
        const int32 HandIndex = Ref.FindBoneIndex(Pair.Value);
        if (IKIndex == INDEX_NONE || HandIndex == INDEX_NONE) { return false; }
        const FCompactPoseBoneIndex IK = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(IKIndex));
        const FCompactPoseBoneIndex Hand = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandIndex));
        if (IK == INDEX_NONE || Hand == INDEX_NONE) { return false; }
        TargetHands[IK.GetInt()] = Hand;
    }
    TArray<FTransform> ComponentPose;
    ComponentPose.SetNum(Bones.GetCompactPoseNumBones());
    for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
    {
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
        ComponentPose[Bone.GetInt()] = Parent == INDEX_NONE ? Pose[Bone]
            : Pose[Bone] * ComponentPose[Parent.GetInt()];
    }
    for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
    {
        const FCompactPoseBoneIndex Target = TargetHands[Bone.GetInt()];
        if (Target == INDEX_NONE) { continue; }
        const FTransform Desired = ComponentPose[Target.GetInt()];
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
        Pose[Bone] = Parent == INDEX_NONE ? Desired : Desired.GetRelativeTransform(ComponentPose[Parent.GetInt()]);
        Pose[Bone].NormalizeRotation();
        ComponentPose[Bone.GetInt()] = Desired;
    }
    return true;
}

static bool Bake(const TCHAR* Side, bool bMirror, UAnimSequence& Source, USkeletalMesh& Mesh, FString& Report)
{
    const FString SideName(Side);
    UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *(FString(Root) + TEXT("A_Boxing") + SideName + TEXT("Jab_Appearance")));
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *(FString(Root) + TEXT("AM_Boxing") + SideName + TEXT("Jab_Appearance")));
    URogue10mAttackSkillData* Skill = LoadObject<URogue10mAttackSkillData>(nullptr,
        *(TEXT("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_") + SideName + TEXT("Jab")));
    if (!Target || !Montage || !Skill || Target == &Source || Montage->GetSkeleton() != Source.GetSkeleton()
        || Target->GetSkeleton() != Source.GetSkeleton() || Mesh.GetSkeleton() != Source.GetSkeleton()) { return false; }
    const float Length = Montage->GetPlayLength();
    const float HitTime = Skill->HitStartDelaySeconds;
    const int32 Frames = FMath::RoundToInt(Length * 120.0f);
    if (Frames < 2 || HitTime <= 0 || HitTime >= Length || FMath::Abs(Frames / 120.0f - Length) > 0.0001f
        || Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1) { return false; }
    const FReferenceSkeleton& Ref = Mesh.GetRefSkeleton();
    TArray<FBoneIndexType> Required;
    for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone) { Required.Add(static_cast<FBoneIndexType>(Bone)); }
    FBoneContainer Bones;
    Bones.InitializeTo(Required, UE::Anim::FCurveFilterSettings(), Mesh);
    TArray<FCompactPoseBoneIndex> MirrorBones;
    TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> RefRotations;
    MirrorBones.SetNum(Bones.GetCompactPoseNumBones());
    RefRotations.SetNumUninitialized(Bones.GetCompactPoseNumBones());
    for (int32 Index = 0; Index < Bones.GetCompactPoseNumBones(); ++Index)
    {
        const FCompactPoseBoneIndex Bone(Index);
        FString Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt()).ToString();
        if (Name.EndsWith(TEXT("_l"))) { Name = Name.LeftChop(2) + TEXT("_r"); }
        else if (Name.EndsWith(TEXT("_r"))) { Name = Name.LeftChop(2) + TEXT("_l"); }
        const int32 MirrorIndex = Ref.FindBoneIndex(*Name);
        MirrorBones[Index] = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MirrorIndex));
        if (MirrorBones[Index] == INDEX_NONE) { return false; }
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
        RefRotations[Bone] = Parent == INDEX_NONE ? Bones.GetRefPoseTransform(Bone).GetRotation()
            : RefRotations[Parent] * Bones.GetRefPoseTransform(Bone).GetRotation();
    }
    TArray<FTrack> Tracks;
    Tracks.SetNum(Ref.GetNum());
    TArray<uint8> PreserveSourceBone;
    TArray<uint8> AlignCenterBone;
    TArray<FTransform> UnmirroredPose;
    TArray<FQuat> CenterStartOffsets;
    PreserveSourceBone.SetNumZeroed(Bones.GetCompactPoseNumBones());
    AlignCenterBone.SetNumZeroed(Bones.GetCompactPoseNumBones());
    UnmirroredPose.SetNum(Bones.GetCompactPoseNumBones());
    CenterStartOffsets.Init(FQuat::Identity, Bones.GetCompactPoseNumBones());
    FQuat ChestStartAlignment = FQuat::Identity;
    const int32 SpineRoot = Ref.FindBoneIndex(TEXT("spine_01"));
    if (SpineRoot == INDEX_NONE) { return false; }
    const int32 ClavicleIndex = Ref.FindBoneIndex(TEXT("clavicle_l"));
    const int32 ChestIndex = ClavicleIndex == INDEX_NONE ? INDEX_NONE : Ref.GetParentIndex(ClavicleIndex);
    if (ChestIndex == INDEX_NONE) { return false; }
    const FCompactPoseBoneIndex SpineBone = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SpineRoot));
    const FCompactPoseBoneIndex ChestBone = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ChestIndex));
    const FCompactPoseBoneIndex PelvisBone = Bones.GetParentBoneIndex(SpineBone);
    if (SpineBone == INDEX_NONE || ChestBone == INDEX_NONE || PelvisBone == INDEX_NONE) { return false; }
    for (int32 Index = 0; Index < Bones.GetCompactPoseNumBones(); ++Index)
    {
        const int32 MeshIndex = Bones.MakeMeshPoseIndex(FCompactPoseBoneIndex(Index)).GetInt();
        bool bUpperBody = false;
        for (int32 Ancestor = MeshIndex; Ancestor != INDEX_NONE; Ancestor = Ref.GetParentIndex(Ancestor))
        {
            if (Ancestor == SpineRoot) { bUpperBody = true; break; }
        }
        // Root, pelvis, both legs and foot IK retain the stepping stance. Hand IK is synchronized below.
        PreserveSourceBone[Index] = !bUpperBody;
        const FString Name = Ref.GetBoneName(MeshIndex).ToString();
        AlignCenterBone[Index] = bUpperBody && (Name.StartsWith(TEXT("spine_"))
            || Name.StartsWith(TEXT("neck_")) || Name == TEXT("head"));
    }
    // The left jab retains every source transform. The right jab mirrors the arms and
    // torso motion, with the source lower body and a shared central starting pose.
    for (int32 Frame = 0; Frame <= Frames; ++Frame)
    {
        const float Time = Frame / 120.0f;
        constexpr float SourceHit = 13.0f / 30.0f;
        const float SourceTime = Time <= HitTime ? SourceHit * Time / HitTime
            : FMath::Lerp(SourceHit, Source.GetPlayLength(), (Time - HitTime) / (Length - HitTime));
        FMemMark Mark(FMemStack::Get());
        FCompactPose Pose; Pose.SetBoneContainer(&Bones); Pose.ResetToRefPose();
        FBlendedCurve Curve; Curve.InitFrom(Bones);
        UE::Anim::FStackAttributeContainer Attributes;
        FAnimationPoseData Data(Pose, Curve, Attributes);
        Source.GetAnimationPose(Data, FAnimExtractContext(FMath::Clamp(SourceTime, 0.f, Source.GetPlayLength()), false));
        if (bMirror)
        {
            for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
            {
                UnmirroredPose[Bone.GetInt()] = Pose[Bone];
            }
            TArray<FTransform> SourceComponentPose;
            BuildComponentPose(Pose, Bones, SourceComponentPose);
            FAnimationRuntime::MirrorPose(Pose, EAxis::X, MirrorBones, RefRotations);
            TArray<FTransform> MirroredComponentPose;
            BuildComponentPose(Pose, Bones, MirroredComponentPose);
            if (Frame == 0)
            {
                ChestStartAlignment = (SourceComponentPose[ChestBone.GetInt()].GetRotation()
                    * MirroredComponentPose[ChestBone.GetInt()].GetRotation().Inverse()).GetNormalized();
            }
            for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
            {
                const int32 Index = Bone.GetInt();
                if (PreserveSourceBone[Index])
                {
                    Pose[Bone] = UnmirroredPose[Index];
                }
                else if (AlignCenterBone[Index])
                {
                    // A full mirrored guard changes the pelvis heading by ~70 degrees.
                    // Rebase each central local rotation at frame zero so mirroring an arm
                    // does not flip the stance or inject that heading change at the waist.
                    if (Frame == 0)
                    {
                        CenterStartOffsets[Index] = (UnmirroredPose[Index].GetRotation()
                            * Pose[Bone].GetRotation().Inverse()).GetNormalized();
                    }
                    Pose[Bone].SetRotation((CenterStartOffsets[Index] * Pose[Bone].GetRotation()).GetNormalized());
                    Pose[Bone].SetTranslation(UnmirroredPose[Index].GetTranslation());
                    Pose[Bone].SetScale3D(UnmirroredPose[Index].GetScale3D());
                }
            }
            // Preserving the pelvis also preserves its animated turn. Local-only mirroring
            // would add that turn a second time and send the right fist sideways. Preserve
            // the mirrored CHEST motion in component space, aligned once to the source guard.
            // The arms then retain the original mirrored shoulder-relative trajectory.
            TArray<FTransform> RebasedComponentPose;
            BuildComponentPose(Pose, Bones, RebasedComponentPose);
            const FQuat DesiredChest = (ChestStartAlignment
                * MirroredComponentPose[ChestBone.GetInt()].GetRotation()).GetNormalized();
            const FQuat Correction = (DesiredChest
                * RebasedComponentPose[ChestBone.GetInt()].GetRotation().Inverse()).GetNormalized();
            const FQuat DesiredSpine = (Correction
                * RebasedComponentPose[SpineBone.GetInt()].GetRotation()).GetNormalized();
            Pose[SpineBone].SetRotation((SourceComponentPose[PelvisBone.GetInt()].GetRotation().Inverse()
                * DesiredSpine).GetNormalized());
            if (!SyncHandIK(Pose, Bones)) { return false; }
        }
        for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
        {
            const FTransform& Transform = Pose[Bone];
            if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized()) { return false; }
            FTrack& Track = Tracks[Bones.MakeMeshPoseIndex(Bone).GetInt()];
            Track.Positions.Add(FVector3f(Transform.GetTranslation()));
            Track.Rotations.Add(FQuat4f(Transform.GetRotation()));
            Track.Scales.Add(FVector3f(Transform.GetScale3D()));
        }
    }
    Target->Modify();
    IAnimationDataController& Controller = Target->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Bake original Boxing for Appearance Mesh")), false);
    Controller.RemoveAllBoneTracks(false);
    Controller.SetFrameRate(FFrameRate(120, 1), false);
    Controller.SetNumberOfFrames(FFrameNumber(Frames), false);
    for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
    {
        const FName Name = Ref.GetBoneName(Bone);
        if (!Controller.AddBoneCurve(Name, false)
            || !Controller.SetBoneTrackKeys(Name, Tracks[Bone].Positions, Tracks[Bone].Rotations, Tracks[Bone].Scales, false))
        { Controller.CloseBracket(false); return false; }
    }
    Controller.NotifyPopulated();
    Controller.CloseBracket(false);
    Target->bEnableRootMotion = false;
    Target->bForceRootLock = false;
    Target->PostEditChange();
    Target->MarkPackageDirty();
    Montage->Modify();
    FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
    Segment.SetAnimReference(Target, true);
    Segment.StartPos = 0; Segment.AnimStartTime = 0; Segment.AnimEndTime = Length;
    Segment.AnimPlayRate = 1; Segment.LoopingCount = 1;
    Montage->RefreshCacheData();
    Montage->PostEditChange();
    Montage->MarkPackageDirty();
    if (FMath::Abs(Target->GetPlayLength() - Length) > 0.0001f || FMath::Abs(Montage->GetPlayLength() - Length) > 0.0001f) { return false; }
    Report += FString::Printf(TEXT("%s length=%.9f hit=%.9f rate=%.6f frames=%d bones=%d mirror=%d slot=%s stance=source_lower_body torso=shared_start_mirrored_motion\n"),
        Side, Length, HitTime, Skill->AnimationPlayRate, Frames + 1, Ref.GetNum(), bMirror,
        *Montage->SlotAnimTracks[0].SlotName.ToString());
    return true;
}
static void Run()
{
    const FString Result = FPaths::ProjectDir() / TEXT("tmp/appearance-head/boxing-authoring.txt");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Result), true);
    FFileHelper::SaveStringToFile(TEXT("RESULT=APPEARANCE_BOXING_AUTHORING_FAILED\n"), *Result);
    UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny"));
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    FString Report;
    if (!Source || !Mesh || Source->IsValidAdditive() || !Bake(TEXT("Left"), false, *Source, *Mesh, Report)
        || !Bake(TEXT("Right"), true, *Source, *Mesh, Report))
    { UE_LOG(LogRogue10m, Error, TEXT("APPEARANCE_BOXING_AUTHORING_FAILED %s"), *Report); return; }
    Report += TEXT("RESULT=APPEARANCE_BOXING_AUTHORING_PASSED\n");
    FFileHelper::SaveStringToFile(Report, *Result);
    UE_LOG(LogRogue10m, Display, TEXT("%s"), *Report);
}
static FAutoConsoleCommand Command(TEXT("Rogue10m.AuthorAppearanceBoxing"),
    TEXT("Editor only: bake already duplicated Boxing sequences and montage segments."), FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
