#include <fbxsdk.h>
#include <iostream>
#include <vector>
#include <string>

static void Bones(FbxNode* Node, std::vector<FbxNode*>& Out)
{
    if (Node->GetSkeleton()) Out.push_back(Node);
    for (int i = 0; i < Node->GetChildCount(); ++i) Bones(Node->GetChild(i), Out);
}
int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    FbxManager* Manager = FbxManager::Create();
    FbxIOSettings* Settings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(Settings);
    FbxImporter* Importer = FbxImporter::Create(Manager, "");
    if (!Importer->Initialize(argv[1], -1, Settings)) return 3;
    FbxScene* Scene = FbxScene::Create(Manager, "MarteloSourcePreview");
    if (!Importer->Import(Scene)) return 4;
    Importer->Destroy();
    std::vector<FbxNode*> Skeleton; Bones(Scene->GetRootNode(), Skeleton);
    if (Skeleton.size() != 65 || Scene->GetSrcObjectCount<FbxAnimStack>() != 1) return 5;
    // Strip only Mixamo's namespace in this derived file. Source transforms and all curves stay intact.
    for (FbxNode* Bone : Skeleton)
    {
        std::string Name = Bone->GetName(); size_t Colon = Name.rfind(':');
        if (Colon != std::string::npos) Bone->SetName(Name.substr(Colon + 1).c_str());
    }
    FbxNode* MeshNode = FbxNode::Create(Scene, "MarteloSourceBindProxy");
    FbxMesh* Mesh = FbxMesh::Create(Scene, "MarteloSourceBindProxy");
    MeshNode->SetNodeAttribute(Mesh); Scene->GetRootNode()->AddChild(MeshNode);
    Mesh->InitControlPoints(static_cast<int>(Skeleton.size()) * 3);
    FbxSkin* Skin = FbxSkin::Create(Scene, "MarteloSourceSkin");
    FbxPose* BindPose = FbxPose::Create(Scene, "MarteloSourceBindPose"); BindPose->SetIsBindPose(true);
    FbxAMatrix Identity; Identity.SetIdentity();
    BindPose->Add(MeshNode, FbxMatrix(Identity));
    for (int i = 0; i < static_cast<int>(Skeleton.size()); ++i)
    {
        FbxNode* Bone = Skeleton[i];
        FbxAMatrix Bind = Bone->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE);
        FbxVector4 P = Bind.GetT();
        Mesh->SetControlPointAt(P + FbxVector4(-0.2, 0, 0), i * 3);
        Mesh->SetControlPointAt(P + FbxVector4(0.2, 0, 0), i * 3 + 1);
        Mesh->SetControlPointAt(P + FbxVector4(0, 0.2, 0), i * 3 + 2);
        Mesh->BeginPolygon(); Mesh->AddPolygon(i * 3); Mesh->AddPolygon(i * 3 + 1); Mesh->AddPolygon(i * 3 + 2); Mesh->EndPolygon();
        FbxCluster* Cluster = FbxCluster::Create(Scene, Bone->GetName());
        Cluster->SetLink(Bone); Cluster->SetLinkMode(FbxCluster::eNormalize);
        Cluster->SetTransformMatrix(Identity); Cluster->SetTransformLinkMatrix(Bind);
        for (int j = 0; j < 3; ++j) Cluster->AddControlPointIndex(i * 3 + j, 1.0);
        Skin->AddCluster(Cluster); BindPose->Add(Bone, FbxMatrix(Bind));
    }
    Mesh->AddDeformer(Skin); Scene->AddPose(BindPose);
    FbxGeometryConverter Converter(Manager); Converter.ComputeEdgeSmoothingFromNormals(Mesh);
    FbxExporter* Exporter = FbxExporter::Create(Manager, "");
    if (!Exporter->Initialize(argv[2], -1, Settings)) return 6;
    if (!Exporter->Export(Scene)) { std::cerr << Exporter->GetStatus().GetErrorString(); return 7; }
    Exporter->Destroy(); Manager->Destroy();
    std::cout << "MARTElO_SOURCE_READY bones=65 triangles=65 source_curves_preserved=1" << std::endl;
    return 0;
}
