// Project-owned arms derivatives. Never saves the vendor mesh, skeleton or materials.
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rogue10m.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"

namespace Rogue10mOwnerArmsAuthoring
{
struct FAllowedPair
{
    const TCHAR* Source;
    const TCHAR* Target;
    const TCHAR* ShortName;
};

static const FAllowedPair AllowedPairs[] = {
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Hu_M"), TEXT("SK_OwnerArms_Hu_M")},
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Human/Female/SK_Hu_F_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Hu_F"), TEXT("SK_OwnerArms_Hu_F")},
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Dwarf/Male/SK_Dw_M_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Dw_M"), TEXT("SK_OwnerArms_Dw_M")},
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Dwarf/Female/SK_Dw_F_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Dw_F"), TEXT("SK_OwnerArms_Dw_F")},
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Orc/Male/SK_Or_M_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Or_M"), TEXT("SK_OwnerArms_Or_M")},
    {TEXT("/Game/StylizedCharacter/Meshes/Character/Orc/Female/SK_Or_F_FullBody"), TEXT("/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_Or_F"), TEXT("SK_OwnerArms_Or_F")}
};

static FString ObjectPath(const FString& Package)
{
    return Package + TEXT(".") + FPaths::GetCleanFilename(Package);
}

static bool IsArmBone(const FReferenceSkeleton& Skeleton, int32 Bone)
{
    while (Bone != INDEX_NONE)
    {
        const FName Name = Skeleton.GetBoneName(Bone);
        if (Name == TEXT("upperarm_l") || Name == TEXT("upperarm_r")) { return true; }
        Bone = Skeleton.GetParentIndex(Bone);
    }
    return false;
}

static bool SameReferenceSkeleton(const FReferenceSkeleton& A, const FReferenceSkeleton& B)
{
    if (A.GetNum() != B.GetNum()) { return false; }
    for (int32 Bone = 0; Bone < A.GetNum(); ++Bone)
    {
        if (A.GetBoneName(Bone) != B.GetBoneName(Bone)
            || A.GetParentIndex(Bone) != B.GetParentIndex(Bone)
            || !A.GetRefBonePose()[Bone].Equals(B.GetRefBonePose()[Bone])) { return false; }
    }
    return true;
}

static void Create(const TArray<FString>& Args)
{
    const FAllowedPair* Pair = nullptr;
    if (Args.Num() == 2)
    {
        for (const FAllowedPair& Candidate : AllowedPairs)
        {
            const bool SourceMatches = Args[0] == Candidate.Source || Args[0] == ObjectPath(Candidate.Source);
            const bool TargetMatches = Args[1] == Candidate.Target || Args[1] == ObjectPath(Candidate.Target);
            if (SourceMatches && TargetMatches) { Pair = &Candidate; break; }
        }
    }
    if (!Pair)
    {
        UE_LOG(LogRogue10m, Error, TEXT("OWNER_ARMS_FAILED: expected exactly one whitelisted source/target pair"));
        return;
    }
    const FString ResultPath = FPaths::ProjectDir() / TEXT("tmp/owner-arms") / (FString(Pair->ShortName) + TEXT(".txt"));
    if (IFileManager::Get().FileExists(*ResultPath))
    {
        UE_LOG(LogRogue10m, Error, TEXT("OWNER_ARMS_FAILED: preserve the existing report before a diagnosed new attempt: %s"), *ResultPath);
        return;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
    FString Report = FString::Printf(TEXT("source=%s\ntarget=%s\n"), Pair->Source, Pair->Target);
    auto Fail = [&Report, &ResultPath](const TCHAR* Reason)
    {
        Report += FString(TEXT("OWNER_ARMS_FAILED reason=")) + Reason + TEXT("\n");
        UE_LOG(LogRogue10m, Error, TEXT("%s"), *Report);
        FFileHelper::SaveStringToFile(Report, *ResultPath);
    };
    if (FPackageName::DoesPackageExist(Pair->Target))
    {
        Fail(TEXT("target_already_saved_refusing_to_mutate_existing_asset")); return;
    }
    USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, *ObjectPath(Pair->Source));
    USkeletalMesh* Target = LoadObject<USkeletalMesh>(nullptr, *ObjectPath(Pair->Target));
    if (!Source || !Target || Source == Target || Source->GetSkeleton() != Target->GetSkeleton())
    {
        Fail(TEXT("expected_separate_duplicate_with_same_skeleton")); return;
    }
    USkinnedAsset* Assets[] = {Source, Target};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const FReferenceSkeleton& Ref = Source->GetRefSkeleton();
    if (!SameReferenceSkeleton(Ref, Target->GetRefSkeleton()))
    {
        Fail(TEXT("duplicate_reference_skeleton_mismatch_before_edit")); return;
    }
    for (const FName Bone : {FName(TEXT("upperarm_l")), FName(TEXT("upperarm_r")), FName(TEXT("head")), FName(TEXT("spine_03"))})
    {
        if (Ref.FindBoneIndex(Bone) == INDEX_NONE) { Fail(TEXT("required_source_bone_missing")); return; }
    }

    // Clone every LOD before changing the destination. The source UObject is read only.
    TArray<FMeshDescription> Descriptions;
    Descriptions.SetNum(Source->GetLODNum());
    if (Descriptions.IsEmpty()) { Fail(TEXT("no_source_lods")); return; }
    for (int32 LOD = 0; LOD < Descriptions.Num(); ++LOD)
    {
        FMeshDescription& Mesh = Descriptions[LOD];
        // Generated LODs may lack import descriptions. Retain their reduction settings
        // and rebuild them from a filtered LOD0 description, matching the existing helper.
        if (!Source->CloneMeshDescription(LOD, Mesh) && !Source->CloneMeshDescription(0, Mesh))
        {
            Fail(TEXT("source_mesh_description_unavailable")); return;
        }
        FSkeletalMeshAttributes Attributes(Mesh);
        const auto Weights = Attributes.GetVertexSkinWeights();
        const auto BoneNames = Attributes.GetBoneNames();
        if (!Weights.IsValid() || !BoneNames.IsValid()) { Fail(TEXT("source_skin_attributes_unavailable")); return; }
        TSet<int32> ArmBones;
        for (const FBoneID Bone : Attributes.Bones().GetElementIDs())
        {
            if (IsArmBone(Ref, Ref.FindBoneIndex(BoneNames[Bone]))) { ArmBones.Add(Bone.GetValue()); }
        }
        TSet<FVertexID> ArmVertices;
        for (const FVertexID Vertex : Mesh.Vertices().GetElementIDs())
        {
            float ArmWeight = 0.0f;
            for (const auto Weight : Weights.Get(Vertex))
            {
                if (ArmBones.Contains(Weight.GetBoneIndex())) { ArmWeight += Weight.GetWeight(); }
            }
            if (ArmWeight >= 0.5f) { ArmVertices.Add(Vertex); }
        }
        // DeleteTriangles determines orphan groups from their triangle count. A
        // source group can still contain a zero-triangle polygon, so that path
        // can assert while deleting the group. Delete complete polygons instead:
        // DeletePolygons determines orphan groups from their polygon count.
        // Keep zero-triangle source polygons: DeletePolygon_Internal reserves
        // TriangleCount - 1 and cannot safely process those empty polygons.
        TArray<FPolygonID> RemovedPolygons;
        const int32 Before = Mesh.Triangles().Num();
        int32 RemovedTriangles = 0;
        int32 EmptyPolygons = 0;
        for (const FPolygonID Polygon : Mesh.Polygons().GetElementIDs())
        {
            const TArrayView<const FTriangleID> Triangles = Mesh.GetPolygonTriangles(Polygon);
            if (Triangles.IsEmpty()) { ++EmptyPolygons; continue; }
            bool bRemovePolygon = false;
            for (const FTriangleID Triangle : Triangles)
            {
                for (const FVertexID Vertex : Mesh.GetTriangleVertices(Triangle))
                {
                    if (!ArmVertices.Contains(Vertex)) { bRemovePolygon = true; break; }
                }
                if (bRemovePolygon) { break; }
            }
            if (bRemovePolygon)
            {
                RemovedPolygons.Add(Polygon);
                RemovedTriangles += Triangles.Num();
            }
        }
        Report += FString::Printf(TEXT("LOD%d source_polygons=%d empty_source_polygons=%d removed_polygons=%d\n"),
            LOD, Mesh.Polygons().Num(), EmptyPolygons, RemovedPolygons.Num());
        UE_LOG(LogRogue10m, Display, TEXT("OWNER_ARMS_TOPOLOGY source=%s LOD=%d polygons=%d empty_polygons=%d removed_polygons=%d"),
            Pair->Source, LOD, Mesh.Polygons().Num(), EmptyPolygons, RemovedPolygons.Num());
        if (RemovedPolygons.IsEmpty() || RemovedTriangles >= Before) { Fail(TEXT("filter_must_remove_body_and_retain_arms")); return; }
        Mesh.DeletePolygons(RemovedPolygons);
        if (Mesh.Triangles().Num() != Before - RemovedTriangles)
        {
            Fail(TEXT("polygon_filter_triangle_count_mismatch")); return;
        }
        for (const FTriangleID Triangle : Mesh.Triangles().GetElementIDs())
        {
            for (const FVertexID Vertex : Mesh.GetTriangleVertices(Triangle))
            {
                if (!ArmVertices.Contains(Vertex)) { Fail(TEXT("polygon_filter_retained_non_arm_triangle")); return; }
            }
        }
        FElementIDRemappings Remappings;
        Mesh.Compact(Remappings);
        Report += FString::Printf(TEXT("LOD%d source_triangles=%d arm_triangles=%d removed=%d bones=%d\n"),
            LOD, Before, Mesh.Triangles().Num(), RemovedTriangles, Attributes.Bones().Num());
    }

    Target->Modify();
    Target->PreEditChange(nullptr);
    Target->SetNumSourceModels(Descriptions.Num());
    for (int32 LOD = 0; LOD < Descriptions.Num(); ++LOD)
    {
        Target->CreateMeshDescription(LOD, MoveTemp(Descriptions[LOD]));
        USkeletalMesh::FCommitMeshDescriptionParams Params;
        Params.bForceUpdate = true;
        if (!Target->CommitMeshDescription(LOD, Params)) { Fail(TEXT("target_mesh_description_commit_failed")); return; }
    }
    Target->PostEditChange();
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    if (Source->GetSkeleton() != Target->GetSkeleton()
        || !SameReferenceSkeleton(Ref, Target->GetRefSkeleton())
        || Source->GetMaterials().Num() != Target->GetMaterials().Num())
    {
        Fail(TEXT("skeleton_or_material_slot_count_changed")); return;
    }
    for (int32 Slot = 0; Slot < Source->GetMaterials().Num(); ++Slot)
    {
        const FSkeletalMaterial& A = Source->GetMaterials()[Slot];
        const FSkeletalMaterial& B = Target->GetMaterials()[Slot];
        if (A.MaterialInterface != B.MaterialInterface || A.MaterialSlotName != B.MaterialSlotName)
        {
            Fail(TEXT("material_interface_or_slot_name_changed")); return;
        }
    }
    const FSkeletalMeshRenderData* Render = Target->GetResourceForRendering();
    if (!Render || Render->LODRenderData.Num() != Source->GetLODNum()) { Fail(TEXT("built_lod_count_changed")); return; }
    for (int32 LOD = 0; LOD < Render->LODRenderData.Num(); ++LOD)
    {
        const FSkeletalMeshLODRenderData& Data = Render->LODRenderData[LOD];
        const FSkinWeightVertexBuffer& Weights = Data.SkinWeightVertexBuffer;
        int32 NonArmVertices = 0;
        int32 InvalidInfluences = 0;
        for (const FSkelMeshRenderSection& Section : Data.RenderSections)
        {
            for (uint32 Vertex = Section.BaseVertexIndex; Vertex < Section.BaseVertexIndex + Section.NumVertices; ++Vertex)
            {
                uint32 Total = 0, Arm = 0;
                for (uint32 Influence = 0; Influence < Weights.GetMaxBoneInfluences(); ++Influence)
                {
                    const uint16 Weight = Weights.GetBoneWeight(Vertex, Influence);
                    if (!Weight) { continue; }
                    const uint32 LocalBone = Weights.GetBoneIndex(Vertex, Influence);
                    Total += Weight;
                    if (!Section.BoneMap.IsValidIndex(LocalBone)) { ++InvalidInfluences; continue; }
                    if (IsArmBone(Ref, Section.BoneMap[LocalBone])) { Arm += Weight; }
                }
                // Same byte-quantization allowance as the existing Manny authoring helper.
                if (!Total || static_cast<float>(Arm) / Total < 0.49f) { ++NonArmVertices; }
            }
        }
        Report += FString::Printf(TEXT("LOD%d render_vertices=%d non_arm_vertices=%d invalid_influences=%d\n"),
            LOD, Data.GetNumVertices(), NonArmVertices, InvalidInfluences);
        if (!Data.GetNumVertices() || NonArmVertices > 0 || InvalidInfluences > 0)
        {
            Fail(TEXT("built_geometry_is_not_valid_arms_only")); return;
        }
    }
    Report += FString::Printf(TEXT("OWNER_ARMS_PASSED skeleton_bones=%d material_slots=%d asset=%s saved_by_helper=0\n"),
        Ref.GetNum(), Target->GetMaterials().Num(), Pair->Target);
    if (!FFileHelper::SaveStringToFile(Report, *ResultPath))
    {
        UE_LOG(LogRogue10m, Error, TEXT("OWNER_ARMS_FAILED: could not persist result %s"), *ResultPath); return;
    }
    UE_LOG(LogRogue10m, Display, TEXT("%s"), *Report);
}

static FAutoConsoleCommand CreateCommand(
    TEXT("Rogue10m.CreateOwnerArms"),
    TEXT("Editor only: filter one fresh, whitelisted StylizedCharacter duplicate to arms geometry; no asset save. Args: SourcePath TargetPath."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&Create));
}
#endif
