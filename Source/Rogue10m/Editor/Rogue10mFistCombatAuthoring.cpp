// Dedicated, offline fist animation authoring. Original clips and runtime animation graphs are untouched.
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
#include "TwoBoneIK.h"
#include "Rogue10m.h"

namespace Rogue10mFistCombatAuthoring
{
static const FString Folder(TEXT("/Game/Rogue10m/Animation/Combat/FistAuthored/"));
struct FTrack { TArray<FVector3f> P; TArray<FQuat4f> Q; TArray<FVector3f> S; };
static void Component(const FCompactPose& Pose, const FBoneContainer& Bones, TArray<FTransform>& Out)
{
    Out.SetNum(Bones.GetCompactPoseNumBones());
    for (const FCompactPoseBoneIndex B : Pose.ForEachBoneIndex())
    {
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(B);
        Out[B.GetInt()] = Parent == INDEX_NONE ? Pose[B] : Pose[B] * Out[Parent.GetInt()];
    }
}
static FCompactPoseBoneIndex Bone(const FBoneContainer& Bones, const TCHAR* Name)
{
    return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name)));
}
static void Sample(UAnimSequence& Sequence, double Time, FBoneContainer& Bones, FCompactPose& Pose)
{
    Pose.SetBoneContainer(&Bones); Pose.ResetToRefPose();
    FBlendedCurve Curve; Curve.InitFrom(Bones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Data(Pose, Curve, Attributes);
    Sequence.GetAnimationPose(Data, FAnimExtractContext(Time, false));
}
static double Smooth(double A) { A = FMath::Clamp(A, 0.0, 1.0); return A*A*A*(A*(A*6.0-15.0)+10.0); }
static double SwingAccentMask(double Time, double Contact, double SamplingRate)
{
    // Compressed evaluation may interpolate a contact between two platform-rate
    // keys. Preserve both keys, with one authoring-key margin, so additional swing
    // cannot alter the existing rendered contact. Fade only outside this bracket.
    constexpr double Margin=1.0/120.0, Fade=1.0/60.0;
    const double Lower=FMath::FloorToDouble(Contact*SamplingRate)/SamplingRate-Margin;
    const double Upper=FMath::CeilToDouble(Contact*SamplingRate)/SamplingRate+Margin;
    if (Time<Lower) { return Smooth((Lower-Time)/Fade); }
    if (Time>Upper) { return Smooth((Time-Upper)/Fade); }
    return 0.0;
}
static bool SyncIK(FCompactPose& Pose, const FBoneContainer& Bones)
{
    TArray<FTransform> CS; Component(Pose, Bones, CS);
    for (const TPair<const TCHAR*, const TCHAR*>& Pair : {
        TPair<const TCHAR*, const TCHAR*>(TEXT("ik_hand_gun"), TEXT("hand_r")),
        TPair<const TCHAR*, const TCHAR*>(TEXT("ik_hand_l"), TEXT("hand_l")),
        TPair<const TCHAR*, const TCHAR*>(TEXT("ik_hand_r"), TEXT("hand_r")),
        TPair<const TCHAR*, const TCHAR*>(TEXT("ik_foot_l"), TEXT("foot_l")),
        TPair<const TCHAR*, const TCHAR*>(TEXT("ik_foot_r"), TEXT("foot_r")) })
    {
        const FCompactPoseBoneIndex IK = Bone(Bones, Pair.Key), Hand = Bone(Bones, Pair.Value);
        if (IK == INDEX_NONE || Hand == INDEX_NONE) { return false; }
        const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(IK);
        Pose[IK] = Parent == INDEX_NONE ? CS[Hand.GetInt()] : CS[Hand.GetInt()].GetRelativeTransform(CS[Parent.GetInt()]);
        Pose[IK].NormalizeRotation(); CS[IK.GetInt()] = CS[Hand.GetInt()];
    }
    return true;
}
static void SolveArm(FTransform& Upper, FTransform& Lower, FTransform& Hand, const FVector& Goal, double Sign, double Activity)
{
    // Shared by production baking and the A -> 0 regression probe below.
    const FVector Pole=FMath::Lerp(Lower.GetLocation(),Upper.GetLocation()+FVector(Sign*32.0,8.0,-45.0),Activity);
    AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Pole,Goal,false,1.0,1.0);
}
static bool CheckGuardLimit(const TArray<FTransform>& CS, const FBoneContainer& Bones, double& MaxElbow, double& MaxRotation)
{
    constexpr double Epsilon=1.e-6;
    for (int32 Side=0; Side<2; ++Side)
    {
        const bool Left=Side==0; const double Sign=Left?1.0:-1.0;
        const auto U=Bone(Bones,Left?TEXT("upperarm_l"):TEXT("upperarm_r"));
        const auto L=Bone(Bones,Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"));
        const auto H=Bone(Bones,Left?TEXT("hand_l"):TEXT("hand_r"));
        if (U==INDEX_NONE || L==INDEX_NONE || H==INDEX_NONE) { return false; }
        FTransform Upper=CS[U.GetInt()],Lower=CS[L.GetInt()],Hand=CS[H.GetInt()];
        const FVector Goal=FMath::Lerp(Hand.GetLocation(),Upper.GetLocation()+FVector(-Sign*12.0,51.0,-7.0),Epsilon);
        SolveArm(Upper,Lower,Hand,Goal,Sign,Epsilon);
        MaxElbow=FMath::Max(MaxElbow,FVector::Dist(Lower.GetLocation(),CS[L.GetInt()].GetLocation()));
        MaxRotation=FMath::Max(MaxRotation,FMath::RadiansToDegrees(Upper.GetRotation().AngularDistance(CS[U.GetInt()].GetRotation())));
        MaxRotation=FMath::Max(MaxRotation,FMath::RadiansToDegrees(Lower.GetRotation().AngularDistance(CS[L.GetInt()].GetRotation())));
    }
    return MaxElbow<0.01 && MaxRotation<0.01;
}
static bool Bake(const TArray<FString>& Args, UAnimSequence& Source, USkeletalMesh& Mesh, FString& Report, bool bExpressiveSwing)
{
    // name|style|length|first-contact|contact-spacing|count. All times are montage seconds.
    if (Args.Num() != 6 || Args[0].Contains(TEXT("/")) || Args[0].Contains(TEXT(".."))) { return false; }
    const FString& Name = Args[0]; const FString& Style = Args[1];
    const double Length = FCString::Atod(*Args[2]), First = FCString::Atod(*Args[3]), Spacing = FCString::Atod(*Args[4]);
    const int32 Count = FCString::Atoi(*Args[5]), Frames = FMath::RoundToInt(Length * 120.0);
    UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *(Folder + TEXT("A_") + Name));
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *(Folder + TEXT("AM_") + Name));
    if (!Target || !Montage || Target == &Source || Target->GetSkeleton() != Source.GetSkeleton()
        || Montage->GetSkeleton() != Source.GetSkeleton() || Mesh.GetSkeleton() != Source.GetSkeleton()
        || Count < 1 || Count > 6 || First < 0.05 || First + (Count-1)*Spacing >= Length-0.12
        || Frames < 2 || FMath::Abs(Frames/120.0-Length)>0.0001
        || Montage->SlotAnimTracks.Num()!=1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()!=1) { return false; }
    const FFrameRate SamplingFrameRate=Target->GetSamplingFrameRate();
    if (SamplingFrameRate.Numerator<=0 || SamplingFrameRate.Denominator<=0) { return false; }
    const double SamplingRate=SamplingFrameRate.AsDecimal();
    if (!FMath::IsFinite(SamplingRate) || SamplingRate<=0.0) { return false; }
    const FReferenceSkeleton& Ref = Mesh.GetRefSkeleton();
    TArray<FBoneIndexType> Required;
    for (int32 I=0; I<Ref.GetNum(); ++I) { Required.Add(static_cast<FBoneIndexType>(I)); }
    FBoneContainer Bones; Bones.InitializeTo(Required, UE::Anim::FCurveFilterSettings(), Mesh);
    FMemMark Mark(FMemStack::Get());
    FCompactPose Guard; Sample(Source, 0.0, Bones, Guard);
    TArray<FTransform> GuardCS; Component(Guard, Bones, GuardCS);
    double LimitElbow=0,LimitRotation=0;
    if (!CheckGuardLimit(GuardCS,Bones,LimitElbow,LimitRotation))
    {
        Report+=FString::Printf(TEXT("IK_GUARD_LIMIT_FAILED elbow=%.9f rotation=%.9f\n"),LimitElbow,LimitRotation);
        return false;
    }
    const auto Head = Bone(Bones,TEXT("head")), Spine = Bone(Bones,TEXT("spine_03"));
    if (Head==INDEX_NONE || Spine==INDEX_NONE) { return false; }
    TArray<FTrack> Tracks; Tracks.SetNum(Ref.GetNum());
    double MaxHeadMove=0, MaxHeadAngle=0;
    for (int32 Frame=0; Frame<=Frames; ++Frame)
    {
        FCompactPose Pose; Pose.SetBoneContainer(&Bones); Pose.CopyBonesFrom(Guard);
        const double T=Frame/120.0;
        double Activity[2]={0,0}; double Phase[2]={0,0};
        for (int32 Pulse=0; Pulse<Count; ++Pulse)
        {
            const bool Left=Style==TEXT("left_jab") || (Style==TEXT("flurry") && Pulse%2==0);
            const int32 Side=Left?0:1;
            const double Contact=First+Pulse*Spacing;
            const double AttackDuration=Count>1 ? Spacing*0.82 : FMath::Min(First,Style==TEXT("straight")?0.32:0.28);
            const double RecoverDuration=Count>1 ? Spacing*0.90 : FMath::Min(Length-Contact,Style==TEXT("straight")?0.46:0.35);
            const double PhaseNow=T<Contact ? (T-Contact)/AttackDuration : (T-Contact)/RecoverDuration;
            const double Envelope=PhaseNow<0 ? Smooth(PhaseNow+1.0) : 1.0-Smooth(PhaseNow);
            if (Envelope>Activity[Side]) { Activity[Side]=Envelope; Phase[Side]=PhaseNow; }
        }
        // Small thoracic turn, with a planted pelvis and legs. No root displacement or full-body spin.
        // +Y is forward and the rear/right shoulder is -X: a negative yaw brings
        // that shoulder forward. Preserve the already readable left jab's small turn.
        const double TurnStrength=Style==TEXT("straight")?12.0:
            (Style==TEXT("right_jab") || Style==TEXT("hook") || Style==TEXT("heavy"))?8.0:4.0;
        const double Turn=Style==TEXT("left_jab") ? -4.0*Activity[0] : (Activity[0]-Activity[1])*TurnStrength;
        TArray<FTransform> CS; Component(Pose,Bones,CS);
        const auto SpineParent=Bones.GetParentBoneIndex(Spine);
        const FQuat DesiredSpine=FQuat(FVector::UpVector,FMath::DegreesToRadians(Turn))*CS[Spine.GetInt()].GetRotation();
        Pose[Spine].SetRotation((CS[SpineParent.GetInt()].GetRotation().Inverse()*DesiredSpine).GetNormalized());
        Component(Pose,Bones,CS);
        // Keep the head's source guard orientation: body movement remains visible without a large camera sweep.
        const auto HeadParent=Bones.GetParentBoneIndex(Head);
        Pose[Head].SetRotation((CS[HeadParent.GetInt()].GetRotation().Inverse()*GuardCS[Head.GetInt()].GetRotation()).GetNormalized());
        Component(Pose,Bones,CS);
        // Probe every actual chest pose, including the opposite-hand flurry overlap.
        if (!CheckGuardLimit(CS,Bones,LimitElbow,LimitRotation))
        {
            Report+=FString::Printf(TEXT("IK_CHEST_GUARD_LIMIT_FAILED frame=%d elbow=%.9f rotation=%.9f\n"),Frame,LimitElbow,LimitRotation);
            return false;
        }
        for (int32 Side=0; Side<2; ++Side)
        {
            const bool Left=Side==0; const double Sign=Left?1.0:-1.0, A=Activity[Side];
            if (A<=0.0) { continue; }
            double AccentMask=bExpressiveSwing ? 1.0 : 0.0;
            if (bExpressiveSwing)
            {
                // Both arms must retain the baseline pose around every impact.
                // The opposite fist can already be preparing another flurry hit,
                // and its compressed key also contributes to a full-pose contact audit.
                for (int32 Pulse=0; Pulse<Count; ++Pulse)
                {
                    AccentMask=FMath::Min(AccentMask,SwingAccentMask(T,First+Pulse*Spacing,SamplingRate));
                }
            }
            Component(Pose,Bones,CS);
            const auto U=Bone(Bones,Left?TEXT("upperarm_l"):TEXT("upperarm_r"));
            const auto L=Bone(Bones,Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"));
            const auto H=Bone(Bones,Left?TEXT("hand_l"):TEXT("hand_r"));
            if (U==INDEX_NONE || L==INDEX_NONE || H==INDEX_NONE) { return false; }
            FTransform Upper=CS[U.GetInt()], Lower=CS[L.GetInt()], Hand=CS[H.GetInt()];
            // The other fist can still be turning the chest when this arm reaches A=0.
            // Use this frame's transformed rest hand, never the unturned source CS hand.
            const FVector Shoulder=Upper.GetLocation(), Rest=Hand.GetLocation();
            FVector Offset(-Sign*12.0,51.0,Style==TEXT("right_jab")?6.0:-7.0);
            if (Style==TEXT("straight")) { Offset=FVector(8.0,54.0,8.0); }
            else if (Style==TEXT("body")) { Offset=FVector(12.0,47.0,-26.0); }
            else if (Style==TEXT("hook") || Style==TEXT("flurry"))
            {
                const bool Flurry=Style==TEXT("flurry");
                Offset=FVector(-Sign*(Flurry?16.0:23.0),Flurry?38.0:43.0,Flurry?6.0:7.0);
                // Outside-to-inside travel, passing through the contact pose at phase zero.
                const double Arc=(Flurry?13.0:24.0)+(Flurry?4.0:7.0)*AccentMask;
                Offset.X+=Sign*FMath::Clamp(-Phase[Side],-1.0,1.0)*Arc;
            }
            else if (Style==TEXT("uppercut"))
            {
                Offset=FVector(11.0,36.0,5.0+FMath::Min(Phase[Side],0.0)*(36.0+8.0*AccentMask));
            }
            else if (Style==TEXT("heavy")) { Offset=FVector(10.0,41.0,2.0); }
            FVector Goal=FMath::Lerp(Rest,Shoulder+Offset,A);
            if (Style==TEXT("straight") && Phase[Side]<0.0)
            {
                // A short shoulder-loaded preparation resolves to the exact contact target.
                const double Preparation=4.0*A*(1.0-A);
                Goal+=FVector(Sign*3.0,-5.0,1.0)*Preparation;
            }
            if (bExpressiveSwing)
            {
                // Add contrast around the unchanged impact: load outward/back,
                // then carry the fist a little inward before returning to guard.
                // Protect the actual compressed contact bracket too. The smooth
                // activity and mask converge at guard and protected key boundaries.
                const double Accent=4.0*A*(1.0-A)*AccentMask;
                FVector AccentOffset=FVector::ZeroVector;
                if (Phase[Side]<0.0)
                {
                    if (Style==TEXT("straight")) { AccentOffset=FVector(Sign*2.0,-3.0,1.0); }
                    else if (Style==TEXT("left_jab") || Style==TEXT("right_jab")) { AccentOffset=FVector(Sign*5.0,-7.0,1.0); }
                    else if (Style==TEXT("body")) { AccentOffset=FVector(Sign*5.0,-7.0,2.0); }
                    else if (Style==TEXT("heavy")) { AccentOffset=FVector(Sign*7.0,-7.0,1.0); }
                    else if (Style==TEXT("hook")) { AccentOffset=FVector(Sign*5.0,-4.0,0.0); }
                    else if (Style==TEXT("flurry")) { AccentOffset=FVector(Sign*2.0,-2.0,0.0); }
                    else if (Style==TEXT("uppercut")) { AccentOffset=FVector(Sign*4.0,-4.0,-3.0); }
                }
                else
                {
                    if (Style==TEXT("body")) { AccentOffset=FVector(-Sign*3.0,3.0,-2.0); }
                    else if (Style==TEXT("heavy")) { AccentOffset=FVector(-Sign*6.0,5.0,1.0); }
                    else if (Style==TEXT("hook")) { AccentOffset=FVector(-Sign*5.0,2.0,1.0); }
                    else if (Style==TEXT("flurry")) { AccentOffset=FVector(-Sign*2.0,2.0,1.0); }
                    else if (Style==TEXT("uppercut")) { AccentOffset=FVector(-Sign*4.0,3.0,6.0); }
                    else { AccentOffset=FVector(-Sign*4.0,3.0,1.0); }
                }
                Goal+=AccentOffset*Accent;
            }
            const double Reach=(Lower.GetLocation()-Shoulder).Size()+(Hand.GetLocation()-Lower.GetLocation()).Size();
            Goal=Shoulder+(Goal-Shoulder).GetClampedToMaxSize(Reach*0.94);
            // At A -> 0, the IK plane must converge to the sampled guard plane too.
            // A fixed pole can move the elbow even when the hand target has not moved.
            SolveArm(Upper,Lower,Hand,Goal,Sign,A);
            // Only render-bone rotation is written. The solver cannot stretch bone lengths or scale.
            const auto Parent=Bones.GetParentBoneIndex(U);
            Pose[U].SetRotation((CS[Parent.GetInt()].GetRotation().Inverse()*Upper.GetRotation()).GetNormalized());
            Pose[L].SetRotation((Upper.GetRotation().Inverse()*Lower.GetRotation()).GetNormalized());
            // Preserve the validated closed-fist wrist relative to its forearm.
            Pose[H].SetRotation(Guard[H].GetRotation());
        }
        if (!SyncIK(Pose,Bones)) { return false; }
        Component(Pose,Bones,CS);
        MaxHeadMove=FMath::Max(MaxHeadMove,FVector::Dist(CS[Head.GetInt()].GetLocation(),GuardCS[Head.GetInt()].GetLocation()));
        MaxHeadAngle=FMath::Max(MaxHeadAngle,FMath::RadiansToDegrees(CS[Head.GetInt()].GetRotation().AngularDistance(GuardCS[Head.GetInt()].GetRotation())));
        for (const FCompactPoseBoneIndex B:Pose.ForEachBoneIndex())
        {
            const FTransform& V=Pose[B]; const FString BoneName=Ref.GetBoneName(Bones.MakeMeshPoseIndex(B).GetInt()).ToString();
            if (V.ContainsNaN() || !V.GetRotation().IsNormalized()
                || (!BoneName.StartsWith(TEXT("ik_")) && (!V.GetTranslation().Equals(Guard[B].GetTranslation(),0.0001) || !V.GetScale3D().Equals(Guard[B].GetScale3D(),0.0001)))) { return false; }
            FTrack& Track=Tracks[Bones.MakeMeshPoseIndex(B).GetInt()];
            Track.P.Add(FVector3f(V.GetTranslation())); Track.Q.Add(FQuat4f(V.GetRotation())); Track.S.Add(FVector3f(V.GetScale3D()));
        }
    }
    if (MaxHeadMove>5.0 || MaxHeadAngle>0.01) { return false; }
    Target->Modify(); IAnimationDataController& Controller=Target->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Author stable dedicated fist combat")),false);
    Controller.RemoveAllBoneTracks(false); Controller.SetFrameRate(FFrameRate(120,1),false); Controller.SetNumberOfFrames(FFrameNumber(Frames),false);
    for (int32 I=0; I<Ref.GetNum(); ++I)
    {
        if (!Controller.AddBoneCurve(Ref.GetBoneName(I),false) || !Controller.SetBoneTrackKeys(Ref.GetBoneName(I),Tracks[I].P,Tracks[I].Q,Tracks[I].S,false))
        { Controller.CloseBracket(false); return false; }
    }
    Controller.NotifyPopulated(); Controller.CloseBracket(false);
    Target->bEnableRootMotion=false; Target->bForceRootLock=false; Target->PostEditChange(); Target->MarkPackageDirty();
    if (!bExpressiveSwing)
    {
        Montage->Modify(); auto& Segment=Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
        Segment.SetAnimReference(Target,true); Segment.StartPos=0; Segment.AnimStartTime=0; Segment.AnimEndTime=Length; Segment.AnimPlayRate=1; Segment.LoopingCount=1;
        // Hands-down locomotion must have time to enter the combat guard. Knuckle's
        // first contact is earlier, so its blend completes before the initial damage pulse.
        const bool bKnuckle=Name.StartsWith(TEXT("Knuckle_"));
        const float BlendIn=bKnuckle ? ((Style==TEXT("right_jab") || Style==TEXT("body"))?0.10f:0.12f) : 0.16f;
        Montage->BlendIn.SetBlendTime(BlendIn); Montage->BlendOut.SetBlendTime(0.16f);
        Montage->RefreshCacheData(); Montage->PostEditChange(); Montage->MarkPackageDirty();
    }
    Report+=FString::Printf(TEXT("%s style=%s frames=%d contacts=%d first=%.6f spacing=%.6f head_move_cm=%.6f head_angle_deg=%.6f continuity_elbow_cm=%.9f continuity_rotation_deg=%.9f\n"),*Name,*Style,Frames+1,Count,First,Spacing,MaxHeadMove,MaxHeadAngle,LimitElbow,LimitRotation);
    return true;
}
static void Run(bool bExpressiveSwing)
{
    const FString Work=FPaths::ProjectDir()/(bExpressiveSwing
        ? TEXT("Feature/doc/evidence/fighter-arm-swing-20261005")
        : TEXT("Feature/doc/evidence/fist-combat-20260929"));
    const FString Result=Work/TEXT("native-authoring.txt");
    IFileManager::Get().MakeDirectory(*Work,true);
    FFileHelper::SaveStringToFile(TEXT("RESULT=FIST_COMBAT_AUTHORING_FAILED\n"),*Result);
    UAnimSequence* Source=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingLeftJab_Appearance"));
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    FString Manifest,Report; TArray<FString> Lines;
    if (bExpressiveSwing) { Report+=TEXT("SWING_PROFILE=EXPRESSIVE\n"); }
    if (!Source || !Mesh || !FFileHelper::LoadFileToString(Manifest,*(Work/TEXT("authoring-requests.txt")))) { return; }
    Manifest.ParseIntoArrayLines(Lines,true);
    if (Lines.Num()!=9) { return; }
    for (const FString& Line:Lines)
    {
        TArray<FString> Fields; Line.ParseIntoArray(Fields,TEXT("|"),false);
        if (!Bake(Fields,*Source,*Mesh,Report,bExpressiveSwing))
        { Report+=TEXT("RESULT=FIST_COMBAT_AUTHORING_FAILED\n")+Line; FFileHelper::SaveStringToFile(Report,*Result); UE_LOG(LogRogue10m,Error,TEXT("%s"),*Report); return; }
    }
    Report+=TEXT("RESULT=FIST_COMBAT_AUTHORING_PASSED\n"); FFileHelper::SaveStringToFile(Report,*Result); UE_LOG(LogRogue10m,Display,TEXT("%s"),*Report);
}
static void RunStable() { Run(false); }
static void RunExpressiveSwing() { Run(true); }
static FAutoConsoleCommand Command(TEXT("Rogue10m.AuthorFistCombat"),TEXT("Editor only: bake dedicated stable fist clips from a reviewed manifest."),FConsoleCommandDelegate::CreateStatic(&RunStable));
static FAutoConsoleCommand SwingCommand(TEXT("Rogue10m.AuthorFistSwing"),TEXT("Editor only: opt-in expressive fist swing bake from the reviewed 20261005 manifest."),FConsoleCommandDelegate::CreateStatic(&RunExpressiveSwing));
}
#endif
