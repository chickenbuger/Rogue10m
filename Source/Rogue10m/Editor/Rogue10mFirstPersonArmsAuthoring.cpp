// Copyright Epic Games, Inc. All Rights Reserved.
// Editor-only asset authoring. The runtime retains the complete animation skeleton.
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rogue10m.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"

namespace Rogue10mFirstPersonArmsAuthoring
{
static constexpr TCHAR SourcePath[] = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
static constexpr TCHAR TargetPath[] = TEXT("/Game/Rogue10m/Character/SK_FirstPersonArms.SK_FirstPersonArms");

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

static void Create()
{
	const FString ResultPath = FPaths::ProjectDir() / TEXT("tmp/arms-only/asset-result.txt");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
	FFileHelper::SaveStringToFile(TEXT("FIRST_PERSON_ARMS_FAILED: authoring did not finish\n"), *ResultPath);
	USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, SourcePath);
	USkeletalMesh* Target = LoadObject<USkeletalMesh>(nullptr, TargetPath);
	if (!Source || !Target || Source == Target || Source->GetSkeleton() != Target->GetSkeleton())
	{
		UE_LOG(LogRogue10m, Error, TEXT("FIRST_PERSON_ARMS_FAILED: expected a separate duplicate with the source skeleton"));
		return;
	}
	USkinnedAsset* Assets[] = { Source, Target };
	FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
	const FReferenceSkeleton& Ref = Source->GetRefSkeleton();
	for (const FName Bone : { FName(TEXT("upperarm_l")), FName(TEXT("upperarm_r")), FName(TEXT("head")), FName(TEXT("spine_03")) })
	{
		if (Ref.FindBoneIndex(Bone) == INDEX_NONE) { return; }
	}
	// Work on independent descriptions before touching the destination asset.
	TArray<FMeshDescription> Descriptions;
	Descriptions.SetNum(Source->GetLODNum());
	FString Report;
	for (int32 LOD = 0; LOD < Descriptions.Num(); ++LOD)
	{
		FMeshDescription& Mesh = Descriptions[LOD];
		// A generated LOD has no imported description. Seed it from LOD 0, then let
		// its existing reduction settings rebuild it from the filtered geometry.
		if (!Source->CloneMeshDescription(LOD, Mesh) && !Source->CloneMeshDescription(0, Mesh)) { return; }
		FSkeletalMeshAttributes Attributes(Mesh);
		const auto Weights = Attributes.GetVertexSkinWeights();
		const auto BoneNames = Attributes.GetBoneNames();
		if (!Weights.IsValid() || !BoneNames.IsValid()) { return; }
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
		TArray<FTriangleID> Removed;
		const int32 Before = Mesh.Triangles().Num();
		for (const FTriangleID Triangle : Mesh.Triangles().GetElementIDs())
		{
			for (const FVertexID Vertex : Mesh.GetTriangleVertices(Triangle))
			{
				if (!ArmVertices.Contains(Vertex)) { Removed.Add(Triangle); break; }
			}
		}
		if (Removed.IsEmpty() || Removed.Num() >= Before) { return; }
		Mesh.DeleteTriangles(Removed);
		FElementIDRemappings Remappings;
		Mesh.Compact(Remappings);
		Report += FString::Printf(TEXT("LOD%d source_triangles=%d arm_triangles=%d removed=%d bones=%d\n"), LOD, Before, Mesh.Triangles().Num(), Removed.Num(), Attributes.Bones().Num());
	}
	Target->Modify();
	Target->PreEditChange(nullptr);
	Target->SetNumSourceModels(Descriptions.Num());
	for (int32 LOD = 0; LOD < Descriptions.Num(); ++LOD)
	{
		Target->CreateMeshDescription(LOD, MoveTemp(Descriptions[LOD]));
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bForceUpdate = true;
		if (!Target->CommitMeshDescription(LOD, Params)) { return; }
	}
	Target->PostEditChange();
	FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
	const FReferenceSkeleton& ResultRef = Target->GetRefSkeleton();
	if (Ref.GetNum() != ResultRef.GetNum() || Source->GetMaterials().Num() != Target->GetMaterials().Num()) { return; }
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		if (Ref.GetBoneName(Bone) != ResultRef.GetBoneName(Bone)
			|| Ref.GetParentIndex(Bone) != ResultRef.GetParentIndex(Bone)
			|| !Ref.GetRefBonePose()[Bone].Equals(ResultRef.GetRefBonePose()[Bone])) { return; }
	}
	for (int32 Slot = 0; Slot < Source->GetMaterials().Num(); ++Slot)
	{
		const FSkeletalMaterial& A = Source->GetMaterials()[Slot];
		const FSkeletalMaterial& B = Target->GetMaterials()[Slot];
		if (A.MaterialInterface != B.MaterialInterface || A.MaterialSlotName != B.MaterialSlotName) { return; }
	}
	// Inspect built LODs, not just source geometry: stale generated body LODs must
	// never reappear when mesh LOD selection changes at runtime.
	const FSkeletalMeshRenderData* Render = Target->GetResourceForRendering();
	if (!Render || Render->LODRenderData.Num() != Source->GetLODNum()) { return; }
	for (int32 LOD = 0; LOD < Render->LODRenderData.Num(); ++LOD)
	{
		const FSkeletalMeshLODRenderData& Data = Render->LODRenderData[LOD];
		const FSkinWeightVertexBuffer& Weights = Data.SkinWeightVertexBuffer;
		int32 NonArmVertices = 0;
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
					if (Section.BoneMap.IsValidIndex(LocalBone) && IsArmBone(Ref, Section.BoneMap[LocalBone])) { Arm += Weight; }
				}
				// Small allowance for byte quantization in built skin weights.
				if (!Total || static_cast<float>(Arm) / Total < 0.49f) { ++NonArmVertices; }
			}
		}
		Report += FString::Printf(TEXT("LOD%d render_vertices=%d non_arm_vertices=%d\n"), LOD, Data.GetNumVertices(), NonArmVertices);
		if (NonArmVertices > 0)
		{
			UE_LOG(LogRogue10m, Error, TEXT("FIRST_PERSON_ARMS_FAILED: %s"), *Report);
			FFileHelper::SaveStringToFile(TEXT("FIRST_PERSON_ARMS_FAILED\n") + Report, *ResultPath);
			return;
		}
	}
	Report += FString::Printf(TEXT("FIRST_PERSON_ARMS_PASSED skeleton_bones=%d material_slots=%d asset=%s\n"), Ref.GetNum(), Target->GetMaterials().Num(), TargetPath);
	UE_LOG(LogRogue10m, Log, TEXT("%s"), *Report);
	FFileHelper::SaveStringToFile(Report, *ResultPath);
}

static FAutoConsoleCommand CreateCommand(
	TEXT("Rogue10m.CreateFirstPersonArms"),
	TEXT("Editor only: strip body geometry from the dedicated duplicated first-person arms asset; retain all bones/materials."),
	FConsoleCommandDelegate::CreateStatic(&Create));
}
#endif
