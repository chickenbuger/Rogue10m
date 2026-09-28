#include <fbxsdk.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
void walk(FbxNode* n,std::vector<FbxNode*>& b,int& meshes) {
 if(n->GetMesh())++meshes;
 if(n->GetSkeleton())b.push_back(n);
 for(int i=0;i<n->GetChildCount();++i)walk(n->GetChild(i),b,meshes);
}
std::string esc(const char* s){std::string r;for(;*s;++s){if(*s=='"'||*s=='\\')r+='\\';r+=*s;}return r;}
int main(int argc,char** argv){
 if(argc<3 || argc>4)return 2;
 int selected=argc==4?std::stoi(argv[3]):0;
 auto m=FbxManager::Create();auto io=FbxIOSettings::Create(m,IOSROOT);m->SetIOSettings(io);
 auto imp=FbxImporter::Create(m,""); if(!imp->Initialize(argv[1],-1,io)){std::cerr<<imp->GetStatus().GetErrorString();return 3;}
 auto scene=FbxScene::Create(m,"inspect");if(!imp->Import(scene))return 4;imp->Destroy();
 FbxAxisSystem::MayaYUp.DeepConvertScene(scene);
 std::vector<FbxNode*> bones;int meshes=0;walk(scene->GetRootNode(),bones,meshes);
 for(int i=0;i<scene->GetSrcObjectCount<FbxAnimStack>();++i)std::cout<<"stack["<<i<<"]="<<scene->GetSrcObject<FbxAnimStack>(i)->GetName()<<std::endl;
 auto stack=scene->GetSrcObject<FbxAnimStack>(selected);if(!stack)return 5;scene->SetCurrentAnimationStack(stack);
 auto span=stack->GetLocalTimeSpan();double start=span.GetStart().GetSecondDouble(),end=span.GetStop().GetSecondDouble();
 std::ofstream o(argv[2]);o<<"{\"mesh_count\":"<<meshes<<",\"stack_count\":"<<scene->GetSrcObjectCount<FbxAnimStack>()<<",\"stack\":\""<<esc(stack->GetName())<<"\",\"start\":"<<start<<",\"end\":"<<end<<",\"unit_cm\":"<<scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor()<<",\"axis\":\"MayaYUp\",\"bones\":[";
 for(int i=0;i<(int)bones.size();++i){int p=-1;for(int j=0;j<(int)bones.size();++j)if(bones[i]->GetParent()==bones[j])p=j;if(i)o<<",";o<<"{\"name\":\""<<esc(bones[i]->GetName())<<"\",\"parent\":"<<p<<"}";}
 o<<"],\"frames\":[";
 int count=(int)((end-start)*30+0.5)+1;
 for(int f=0;f<count;++f){double t=start+f/30.0;FbxTime time;time.SetSecondDouble(t);if(f)o<<",";o<<"{\"time\":"<<t<<",\"positions\":[";for(int i=0;i<(int)bones.size();++i){if(i)o<<",";auto p=bones[i]->EvaluateGlobalTransform(time).GetT();o<<"["<<p[0]<<","<<p[1]<<","<<p[2]<<"]";}o<<"]}";}
 o<<"]}";o.close();std::cout<<"bones="<<bones.size()<<" meshes="<<meshes<<" frames="<<count<<" seconds="<<end-start<<std::endl;m->Destroy();
}
