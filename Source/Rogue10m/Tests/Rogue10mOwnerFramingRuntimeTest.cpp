// Editor-only evidence, exact fresh QA profiles only. No physical-input/full-run claim.
#if WITH_EDITOR
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "RawIndexBuffer.h"
#include "Rogue10m.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mAppearanceShadowComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mOwnerArmsComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mProgressionSubsystem.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace Rogue10mOwnerFramingQA
{
class FRun:public TSharedFromThis<FRun>
{
public:
 bool Finished()const{return bFinished;}
 void Start(UWorld& W)
 {
  World=&W;const TCHAR* Args=FCommandLine::Get();FString Town,Prefix,Keys;
  FParse::Value(Args,TEXT("RogueOwnerFramingLabel="),Label);FParse::Value(Args,TEXT("RogueOwnerFramingMode="),Mode);
  FParse::Value(Args,TEXT("RogueProfileSaveSlot="),Town);FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix);FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),Keys);
  bool Good=!Label.IsEmpty()&&Label.Len()<=24;for(TCHAR C:Label)Good&=FChar::IsAlnum(C)||C==TEXT('_');
  TArray<FString> Existing;IFileManager::Get().FindFiles(Existing,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Prefix+TEXT("_*.sav"))),true,false);
  if(!Good||W.GetNetMode()!=NM_Standalone||(Mode!=TEXT("Baseline")&&Mode!=TEXT("Candidate"))||Town!=TEXT("Rogue10m_Town_QA_OwnerFraming_")+Label||Prefix!=TEXT("Rogue10m_Progression_QA_OwnerFraming_")+Label||Keys!=TEXT("Rogue10m_Fighter_QA_OwnerFraming_")+Label||UGameplayStatics::DoesSaveGameExist(Town,0)||UGameplayStatics::DoesSaveGameExist(Keys,0)||!Existing.IsEmpty())
  {UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_FRAMING_FAILED exact_fresh_namespace_required"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Output=FPaths::ProjectDir()/TEXT("Feature/doc/evidence/owner-framing-20261031")/Label;Images=FPaths::ProjectDir()/TEXT("DevLog")/(TEXT("20261031_OwnerFraming_")+Label);
  bRendered=!FParse::Param(Args,TEXT("nullrhi"));
  if(IFileManager::Get().DirectoryExists(*Output)||IFileManager::Get().DirectoryExists(*Images)){UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_FRAMING_FAILED evidence_exists"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  IFileManager::Get().MakeDirectory(*Output,true);if(bRendered)IFileManager::Get().MakeDirectory(*Images,true);
  Captures=TEXT("name,event,frame,world_s,scene,visibility_mode,pitch,body_owner_hidden,proxy_view_hidden,body_cast_shadow,proxy_visible,dodging,action_busy,attack_cooldown,camera_x,camera_y,camera_z,camera_pitch,camera_yaw,hand_l_x,hand_l_y,hand_l_z,hand_r_x,hand_r_y,hand_r_z,framing_active,proxy_class,proxy_mesh\n");
  Vertices=TEXT("capture,frame,world_s,lod,section,vertex,world_x,world_y,world_z,view_depth,view_right,view_up,screen_u,screen_v,near_cm,upperarm_weight\n");
  Geometry=TEXT("capture,frame,world_s,vertices,triangles,near_cm,min_depth,max_depth,vertices_behind_camera,vertices_before_near,vertices_in_frustum,triangles_cross_near,upperarm_triangles_cross_near,boundary_edges,boundary_edges_cross_near,boundary_edges_in_frustum,upperarm_boundary_edges_in_frustum,hand_l_depth,hand_r_depth\n");
  Deadline=FPlatformTime::Seconds()+180.;FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float){if(Self->bFinished)return false;if(FPlatformTime::Seconds()>Self->Deadline){Self->Check(false,TEXT("watchdog"));Self->Finish();return false;}if(Self->Next&&FPlatformTime::Seconds()>=Self->NextAt){auto F=MoveTemp(Self->Next);F();}return !Self->bFinished;}),.01f);
  Cleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Self=AsShared()](UWorld* X,bool,bool){if(X==Self->World.Get()&&!Self->bFinished){Self->Check(false,TEXT("world_cleanup_before_finish"));Self->Finish(false);}});
  Later(3.,[this]{Prepare();});
 }
private:
 struct FImage { FString Name;int32 Width=0,Height=0;TArray<FColor> Pixels; };
 struct FShadow { TWeakObjectPtr<UPrimitiveComponent> Component;bool Cast=false; };
 void Check(bool OK,const TCHAR* Why){++Checks;if(!OK){++Failures;Results+=FString(TEXT("FAIL "))+Why+TEXT("\n");UE_LOG(LogRogue10m,Error,TEXT("OWNER_FRAMING FAIL %s"),Why);}}
 void Later(double Delay,TFunction<void()> F){NextAt=FPlatformTime::Seconds()+Delay;Next=MoveTemp(F);}
 void Key(FKey K,bool Down){if(PC.IsValid())PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
 static bool SamePose(const TArray<FTransform>& A,const TArray<FTransform>& B){if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].Equals(B[I],.000001f))return false;return true;}
 bool ViewExcluded() const{return PC.IsValid()&&Proxy.IsValid()&&PC->HiddenPrimitiveComponents.Contains(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));}
 FString CaptureRow(const FString& Name,const TCHAR* Event)
 {
  const FMinimalViewInfo& V=PC->PlayerCameraManager->GetCameraCacheView();const FVector L=Body->GetSocketLocation(TEXT("hand_l")),R=Body->GetSocketLocation(TEXT("hand_r"));
  return FString::Printf(TEXT("%s,%s,%llu,%.9f,%d,%d,%.6f,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%s,%s\n"),*Name,Event,GFrameCounter,World->GetTimeSeconds(),Scene,VisibilityMode,FRotator::NormalizeAxis(PC->GetControlRotation().Pitch),Body->bOwnerNoSee,ViewExcluded(),Body->CastShadow,Proxy->IsVisible()&&!Proxy->bHiddenInGame,Character->IsDodging(),Combat->IsWeaponChangeBlocked(),Combat->GetAttackCooldownRemaining(),V.Location.X,V.Location.Y,V.Location.Z,V.Rotation.Pitch,V.Rotation.Yaw,L.X,L.Y,L.Z,R.X,R.Y,R.Z,Arms->IsViewFramingActive(),*Proxy->GetClass()->GetPathName(),*GetPathNameSafe(Proxy->GetSkinnedAsset()));
 }
 void RefreshActualProxy()
 {
  if(!Appearance.IsValid()||!PC.IsValid())return;
  USkinnedMeshComponent* Actual=Appearance->GetOwnerArmsMesh();
  if(Actual!=Proxy.Get())
  {
   if(Proxy.IsValid()&&!InitialHidden.Contains(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get())))PC->HiddenPrimitiveComponents.Remove(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));
   Proxy=Actual;
   if(Proxy.IsValid()&&!Shadows.ContainsByPredicate([Actual](const FShadow& S){return S.Component.Get()==Actual;}))Shadows.Add({Actual,Actual->CastShadow!=0});
  }
  if(Proxy.IsValid())
  {
   if(VisibilityMode==1)PC->HiddenPrimitiveComponents.AddUnique(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));
   else if(!InitialHidden.Contains(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get())))PC->HiddenPrimitiveComponents.Remove(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));
  }
 }
 void Prepare()
 {
  PC=Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());auto* GI=World->GetGameInstance();auto* Profiles=GI?GI->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;Saves=GI?GI->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
  Check(PC.IsValid()&&Profiles&&Saves.IsValid(),TEXT("real_controller_profile_progression"));if(Failures){Finish();return;}
  FRogue10mCharacterProfile P;Check(Profiles->GetProfileCount()==0&&Profiles->CreateProfile(TEXT("OwnerFrameQa"),FRogue10mCharacterAppearance(),P)&&Profiles->SelectProfile(P.ProfileId),TEXT("fresh_normal_profile_create_select"));if(Failures){Finish();return;}
  PC->EnterSelectedCharacter();PC->CloseAllBlockingPanels();Character=Cast<ARogue10mCharacter>(PC->GetPawn());Check(Character.IsValid()&&Saves->IsAttachedTo(*Character),TEXT("normal_entry_has_attached_pawn"));if(Failures){Finish();return;}
  Saves->DetachCharacter(*Character,false);Check(!Saves->IsAttachedTo(*Character),TEXT("autosave_detached_before_fixture"));
  Appearance=Character->GetAppearanceCameraComponent();Body=Character->GetMesh();Source=Character->GetAnimationPlaybackMesh();Combat=Character->GetCombatComponent();
  Check(Appearance.IsValid()&&Body.IsValid()&&Source.IsValid()&&Combat.IsValid()&&PC->PlayerCameraManager&&Character->GetRogueAttributeSet(),TEXT("actual_components_available"));if(Failures){Finish();return;}
  InitialTransform=Character->GetActorTransform();InitialControl=PC->GetControlRotation();InitialEnable=Appearance->bEnableOwnerArms;InitialHidden=PC->HiddenPrimitiveComponents;bInitialDamage=Character->CanBeDamaged();bPrepared=true;
  Check(!InitialEnable&&!GetDefault<URogue10mAppearanceCameraComponent>()->bEnableOwnerArms,TEXT("product_owner_view_remains_default_OFF"));if(Failures){Finish();return;}
  Appearance->bEnableOwnerArms=true;Appearance->RefreshAppearanceCamera();Arms=Character->FindComponentByClass<URogue10mOwnerArmsComponent>();Proxy=Appearance->GetOwnerArmsMesh();
  Check(Arms.IsValid()&&Proxy.IsValid()&&Body->bOwnerNoSee&&Appearance->IsOwnerArmsViewActive(),TEXT("QA_instance_only_owner_arms_active_world_body_hidden_to_owner"));if(Failures){Finish();return;}
  if(auto* F=FindFProperty<FBoolProperty>(Arms->GetClass(),TEXT("bEnableViewFraming"))){FramingProperty=F;OldFraming=F->GetPropertyValue_InContainer(Arms.Get());F->SetPropertyValue_InContainer(Arms.Get(),Mode==TEXT("Candidate"));Appearance->RefreshAppearanceCamera();}
  else Check(Mode==TEXT("Baseline"),TEXT("Candidate_requires_explicit_presentation_framing_property"));
  CandidateReadyBy=FPlatformTime::Seconds()+10.;RefreshActualProxy();
  TArray<UPrimitiveComponent*> Parts;Character->GetComponents<UPrimitiveComponent>(Parts);for(auto* Part:Parts)Shadows.Add({Part,Part->CastShadow != 0});
  InitialHidden=PC->HiddenPrimitiveComponents;bInitialDamage=Character->CanBeDamaged();Character->SetCanBeDamaged(false);
  Heavy=IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue10m.FPHeavy"));if(Heavy){OldHeavy=Heavy->GetInt();Heavy->Set(0,ECVF_SetByConsole);}
  CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Self=AsShared()](int32 W,int32 H,const TArray<FColor>& Pixels){if(Self->bFinished||Self->Pending.IsEmpty())return;Self->Captures+=Self->CaptureRow(Self->Pending,TEXT("captured"));Self->Check(Self->Mode!=TEXT("Candidate")||Self->Arms->IsViewFramingActive(),TEXT("Candidate_actual_capture_uses_active_framing_not_leader_fallback"));Self->Check(Self->Appearance->GetOwnerArmsMesh()==Self->Proxy.Get(),TEXT("actual_capture_proxy_matches_current_owner_view_getter"));Self->Check(Self->Body->bOwnerNoSee,TEXT("world_body_stays_owner_hidden_in_all_diagnostic_images"));Self->Check(Self->ViewExcluded()==(Self->VisibilityMode==1),TEXT("actual_capture_view_exclusion_matches_requested_diagnostic"));if(Self->Scene==3)Self->Check(Self->Character->IsDodging(),TEXT("free_dodge_photo_is_captured_during_real_active_dodge"));auto& Image=Self->ImageData.AddDefaulted_GetRef();Image.Name=Self->Pending;Image.Width=W;Image.Height=H;Image.Pixels=Pixels;Self->Pending.Empty();++Self->Completed;Self->Later(.25,[Self]{Self->NextCase();});});
  PostTick=FWorldDelegates::OnWorldPostActorTick.AddLambda([Self=AsShared()](UWorld* X,ELevelTick,float){if(X==Self->World.Get())Self->Observe();});
  WaitDeadline=FPlatformTime::Seconds()+12.;Later(.7,[this]{MakeFloor();});
 }
 void MakeFloor()
 {
  RefreshActualProxy();if(Mode==TEXT("Candidate")&&!Arms->IsViewFramingActive()&&FPlatformTime::Seconds()<CandidateReadyBy){Later(.1,[this]{MakeFloor();});return;}
  Check(Mode!=TEXT("Candidate")||Arms->IsViewFramingActive(),TEXT("Candidate_framing_activates_before_bounded_fixture_start"));
  if(!Character->CanChangeWeapon()&&FPlatformTime::Seconds()<WaitDeadline){Later(.1,[this]{MakeFloor();});return;}
  Check(!Failures&&Character->CanChangeWeapon(),TEXT("real_idle_entry_ready"));if(Failures){Finish();return;}
  Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));Check(Cube.IsValid(),TEXT("existing_engine_cube_available"));if(Failures){Finish();return;}
  Floor=SpawnBox(TEXT("OwnerFramingFloor"),FVector(10000,10000,20),FVector(200,200,.4f));if(Failures){Finish();return;}
  Surface=InitialTransform.GetLocation().Z+1000.;Floor->SetActorLocation(InitialTransform.GetLocation()+FVector(0,0,Surface-20.-InitialTransform.GetLocation().Z));
  Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorLocation(FVector(InitialTransform.GetLocation().X,InitialTransform.GetLocation().Y,Surface+Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.),false,nullptr,ETeleportType::TeleportPhysics);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Later(.5,[this]{Check(Character->GetCharacterMovement()->IsMovingOnGround()&&Character->GetMovementBaseObject()==Floor->GetRootComponent(),TEXT("fixture_uses_real_floor_collision"));if(Failures){Finish();return;}Fixture=Character->GetActorTransform();Scene=0;VisibilityMode=0;BeginCase();});
 }
 AActor* SpawnBox(const TCHAR* Name,FVector Extent,FVector VisualScale)
 {
  FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;auto* Actor=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);auto* Box=Actor?NewObject<UBoxComponent>(Actor,Name,RF_Transient):nullptr;Check(Box!=nullptr,TEXT("transient_collision_box_created"));if(!Box)return Actor;
  Actor->AddInstanceComponent(Box);Actor->SetRootComponent(Box);Box->SetBoxExtent(Extent);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
  auto* Visual=NewObject<UStaticMeshComponent>(Actor,NAME_None,RF_Transient);Actor->AddInstanceComponent(Visual);Visual->SetupAttachment(Box);Visual->SetStaticMesh(Cube.Get());Visual->SetRelativeScale3D(VisualScale);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->RegisterComponent();return Actor;
 }
 void ApplyVisibility()
 {
  RefreshActualProxy();Check(Proxy.IsValid(),TEXT("actual_owner_proxy_available_for_visibility_diagnostic"));if(!Proxy.IsValid())return;const auto SP=Source->GetBoneSpaceTransforms(),BP=Body->GetBoneSpaceTransforms();FMinimalViewInfo Before;Character->CalcCamera(0,Before);RefreshActualProxy();Check(Proxy.IsValid(),TEXT("actual_owner_proxy_available_after_camera_update"));if(!Proxy.IsValid())return;const float CD=Combat->GetAttackCooldownRemaining();const double Clock=World->GetTimeSeconds();auto* Anim=Source->GetAnimInstance();
  if(VisibilityMode==1)PC->HiddenPrimitiveComponents.AddUnique(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));else PC->HiddenPrimitiveComponents.Remove(TWeakObjectPtr<UPrimitiveComponent>(Proxy.Get()));
  for(const auto& S:Shadows)if(S.Component.IsValid())S.Component->SetCastShadow(VisibilityMode==2?false:S.Cast);
  FMinimalViewInfo After;Character->CalcCamera(0,After);Check(Appearance->GetOwnerArmsMesh()==Proxy.Get(),TEXT("same_frame_diagnostic_uses_same_current_proxy_after_camera_refresh"));TSet<FPrimitiveComponentId> Hidden;PC->BuildHiddenComponentList(After.Location,Hidden);
  Check(Hidden.Contains(Proxy->GetPrimitiveSceneId())==(VisibilityMode==1),TEXT("official_hidden_component_list_has_exact_proxy_exclusion"));
  Check(SamePose(SP,Source->GetBoneSpaceTransforms())&&SamePose(BP,Body->GetBoneSpaceTransforms())&&World->GetTimeSeconds()==Clock&&Combat->GetAttackCooldownRemaining()==CD&&Source->GetAnimInstance()==Anim,TEXT("same_frame_visibility_change_preserves_source_body_clock_animation_and_attack_cooldown"));
  Check(Before.Location.Equals(After.Location,.000001f)&&Before.Rotation.Equals(After.Rotation,.000001f)&&Before.FOV==After.FOV,TEXT("same_frame_visibility_change_preserves_gameplay_camera"));
  Check(Body->bOwnerNoSee&&Appearance->IsOwnerArmsViewActive(),TEXT("diagnostic_does_not_restore_world_body_to_owner"));
 }
 void BeginCase()
 {
  if(Failures){Finish();return;}if(Wall.IsValid()){Wall->Destroy();Wall=nullptr;}
  Character->GetCharacterMovement()->StopMovementImmediately();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorTransform(Fixture,false,nullptr,ETeleportType::TeleportPhysics);Character->GetRogueAttributeSet()->RestoreVitals();PC->SetControlRotation(FRotator(Scene==0?0.f:-85.f,InitialControl.Yaw,0));
  if(Scene==2||Scene==4){Wall=SpawnBox(TEXT("OwnerFramingWall"),FVector(10,500,500),FVector(.2f,10,10));if(Failures||!Wall.IsValid()){Finish();return;}Wall->SetActorRotation(Character->GetActorRotation());Wall->SetActorLocation(Character->GetActorLocation()+Character->GetActorForwardVector()*(Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+18.f));}
  ApplyVisibility();WaitDeadline=FPlatformTime::Seconds()+10.;Later(.5,[this]{WaitReady();});
 }
 void WaitReady()
 {
  if((Character->IsDodging()||Character->GetDodgeCooldownRemaining()>.001f||!Character->CanChangeWeapon())&&FPlatformTime::Seconds()<WaitDeadline){Later(.05,[this]{WaitReady();});return;}
  Check(!Character->IsDodging()&&Character->GetDodgeCooldownRemaining()<=.001f&&Character->CanChangeWeapon(),TEXT("real_dodge_and_action_recovered_before_diagnostic_case"));if(Failures){Finish();return;}
  ApplyVisibility();ReadyAfter=FPlatformTime::Seconds()+.2;
  if(Scene>=3){bDodgeRequested=true;bSawDodge=false;Key(Rogue10mInput::DodgeKey(),true);bDodgeKeyDown=true;ReleaseDodgeKeyAt=FPlatformTime::Seconds()+.025;}
  else bWantCapture=true;
 }
 void Observe()
 {
  if(bFinished||!bPrepared||!Character.IsValid())return;
  if(bDodgeKeyDown&&FPlatformTime::Seconds()>=ReleaseDodgeKeyAt){Key(Rogue10mInput::DodgeKey(),false);bDodgeKeyDown=false;}
  RefreshActualProxy();if(!Proxy.IsValid()){if(FPlatformTime::Seconds()>CandidateReadyBy){Check(false,TEXT("actual_owner_proxy_missing_after_bounded_wait"));Finish();}return;}
  if(bDodgeRequested){if(Character->IsDodging()){bSawDodge=true;if(Scene==3)bWantCapture=true;}else if(Scene==4&&bSawDodge){bWantCapture=true;}else if(FPlatformTime::Seconds()>WaitDeadline){Check(false,TEXT("actual_E_dodge_did_not_reach_requested_phase"));Finish();return;}}
  if(!bWantCapture||!Pending.IsEmpty()||(Scene<3&&FPlatformTime::Seconds()<ReadyAfter))return;
  if(Mode==TEXT("Candidate")&&!Arms->IsViewFramingActive()){if(FPlatformTime::Seconds()>WaitDeadline){Check(false,TEXT("Candidate_capture_wait_expired_without_active_framing"));Finish();}return;}
  Check(Mode!=TEXT("Candidate")||Arms->IsViewFramingActive(),TEXT("Candidate_geometry_and_photo_request_use_active_framing"));if(Failures){Finish();return;}bWantCapture=false;bDodgeRequested=false;ApplyVisibility();const FString Name=FString::Printf(TEXT("%02d_scene%d_mode%d.png"),Requested+1,Scene,VisibilityMode);
  AuditGeometry(Name);if(Failures){Finish();return;}++Requested;
  if(bRendered){Pending=Name;Captures+=CaptureRow(Name,TEXT("request"));FScreenshotRequest::RequestScreenshot(Images/Name,true,false);}else Later(.25,[this]{NextCase();});
 }
 void NextCase(){if(++VisibilityMode==3){VisibilityMode=0;++Scene;}if(Scene==5){Finish();return;}BeginCase();}
 void AuditGeometry(const FString& Name)
 {
  const FMinimalViewInfo V=PC->PlayerCameraManager->GetCameraCacheView();const auto* Mesh=Cast<USkeletalMesh>(Proxy->GetSkinnedAsset());auto* Render=Mesh?Mesh->GetResourceForRendering():nullptr;Check(Render&&Render->LODRenderData.IsValidIndex(0),TEXT("actual_proxy_LOD0_render_data_available"));if(!Render||!Render->LODRenderData.IsValidIndex(0))return;
  const auto& LOD=Render->LODRenderData[0];auto* Weights=Proxy->GetSkinWeightBuffer(0);const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
  const bool Readable=Weights&&LOD.StaticVertexBuffers.PositionVertexBuffer.GetVertexData()&&Weights->GetDataVertexBuffer()&&Weights->GetDataVertexBuffer()->GetWeightData()&&(!Weights->GetVariableBonesPerVertex()||(Weights->GetLookupVertexBuffer()&&Weights->GetLookupVertexBuffer()->GetLookupData()))&&Indices&&Indices->GetResourceDataSize()>=Indices->Num()*LOD.MultiSizeIndexContainer.GetDataTypeSize();
  Check(Readable,TEXT("actual_CPU_position_skinweight_and_triangle_buffers_readable"));if(!Readable)return;
  TArray<FMatrix44f> Matrices;Proxy->GetCurrentRefToLocalMatrices(Matrices,0);Check(!Matrices.IsEmpty(),TEXT("actual_current_reference_to_local_skinning_matrices_available"));if(Matrices.IsEmpty())return;
  const int32 Count=LOD.GetNumVertices();TArray<FVector> Projected;Projected.SetNum(Count);TArray<double> Upper;Upper.Init(0.,Count);TArray<int32> Sections;Sections.Init(INDEX_NONE,Count);
  for(int32 S=0;S<LOD.RenderSections.Num();++S){const auto& Section=LOD.RenderSections[S];for(uint32 I=Section.BaseVertexIndex;I<Section.BaseVertexIndex+Section.NumVertices;++I){Sections[I]=S;uint64 Total=0,Top=0;for(uint32 K=0;K<Weights->GetMaxBoneInfluences();++K){const uint16 W=Weights->GetBoneWeight(I,K);if(!W)continue;const uint32 B=Weights->GetBoneIndex(I,K);Check(Section.BoneMap.IsValidIndex(B),TEXT("skin_influence_bone_map_valid"));if(!Section.BoneMap.IsValidIndex(B))return;const int32 Global=Section.BoneMap[B];Check(Mesh->GetRefSkeleton().IsValidIndex(Global),TEXT("skin_influence_reference_bone_valid"));if(!Mesh->GetRefSkeleton().IsValidIndex(Global))return;Total+=W;if(Mesh->GetRefSkeleton().GetBoneName(Global).ToString().Contains(TEXT("upperarm")))Top+=W;}Upper[I]=Total?double(Top)/Total:0.;}}
  const double Near=V.GetFinalPerspectiveNearClipPlane();const double TanH=FMath::Tan(FMath::DegreesToRadians(V.FOV*.5));const FIntPoint Size=World->GetGameViewport()->Viewport->GetSizeXY();const double TanV=TanH*double(Size.Y)/FMath::Max(1,Size.X);const bool ProjectionValid=Size.X>0&&Size.Y>0&&FMath::IsFinite(Near)&&Near>0&&FMath::IsFinite(TanH)&&TanH>0&&FMath::IsFinite(TanV)&&TanV>0;Check(ProjectionValid,TEXT("actual_viewport_size_near_and_perspective_projection_are_valid"));if(!ProjectionValid)return;double Min=UE_BIG_NUMBER,Max=-UE_BIG_NUMBER;int32 Behind=0,Before=0,Inside=0;
  for(int32 I=0;I<Count;++I){const FVector Local(USkinnedMeshComponent::GetSkinnedVertexPosition(Proxy.Get(),I,LOD,*Weights,Matrices));const FVector WorldP=Proxy->GetComponentTransform().TransformPosition(Local);const FVector P=V.Rotation.Quaternion().UnrotateVector(WorldP-V.Location);Check(!P.ContainsNaN(),TEXT("actual_skinned_vertex_world_view_is_finite"));if(P.ContainsNaN())return;Projected[I]=P;Min=FMath::Min(Min,P.X);Max=FMath::Max(Max,P.X);Behind+=P.X<=0.;Before+=P.X<Near;const bool In=P.X>=Near&&FMath::Abs(P.Y)<=P.X*TanH&&FMath::Abs(P.Z)<=P.X*TanV;Inside+=In;const double U=P.X>UE_SMALL_NUMBER?.5+P.Y/(2.*P.X*TanH):-999.;const double SV=P.X>UE_SMALL_NUMBER?.5-P.Z/(2.*P.X*TanV):-999.;Vertices+=FString::Printf(TEXT("%s,%llu,%.9f,0,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),*Name,GFrameCounter,World->GetTimeSeconds(),Sections[I],I,WorldP.X,WorldP.Y,WorldP.Z,P.X,P.Y,P.Z,U,SV,Near,Upper[I]);}
  TMap<uint64,int32> Edges;int32 Cross=0,UpperCross=0;auto AddEdge=[&](uint32 A,uint32 B){const uint64 K=(uint64(FMath::Min(A,B))<<32)|FMath::Max(A,B);++Edges.FindOrAdd(K);};
  Check(Indices->Num()%3==0,TEXT("triangle_index_count_multiple_of_three"));for(int32 I=0;I+2<Indices->Num();I+=3){const uint32 A=Indices->Get(I),B=Indices->Get(I+1),C=Indices->Get(I+2);Check(A<uint32(Count)&&B<uint32(Count)&&C<uint32(Count),TEXT("triangle_indices_inside_actual_proxy_vertex_buffer"));if(A>=uint32(Count)||B>=uint32(Count)||C>=uint32(Count))return;const bool Intersects=FMath::Min3(Projected[A].X,Projected[B].X,Projected[C].X)<=Near&&FMath::Max3(Projected[A].X,Projected[B].X,Projected[C].X)>=Near;Cross+=Intersects;UpperCross+=Intersects&&FMath::Max3(Upper[A],Upper[B],Upper[C])>=.5;AddEdge(A,B);AddEdge(B,C);AddEdge(C,A);}
  int32 Boundaries=0,BoundaryNear=0,BoundaryInside=0,UpperBoundaryInside=0;for(const auto& E:Edges){if(E.Value!=1)continue;++Boundaries;const uint32 A=E.Key>>32,B=uint32(E.Key);const FVector Mid=(Projected[A]+Projected[B])*.5;BoundaryNear+=(Projected[A].X-Near)*(Projected[B].X-Near)<=0.;const bool In=Mid.X>=Near&&FMath::Abs(Mid.Y)<=Mid.X*TanH&&FMath::Abs(Mid.Z)<=Mid.X*TanV;BoundaryInside+=In;UpperBoundaryInside+=In&&FMath::Max(Upper[A],Upper[B])>=.5;}
  auto HandDepth=[&](const TCHAR* Bone){return V.Rotation.Quaternion().UnrotateVector(Proxy->GetSocketLocation(Bone)-V.Location).X;};
  Geometry+=FString::Printf(TEXT("%s,%llu,%.9f,%d,%d,%.9f,%.9f,%.9f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f\n"),*Name,GFrameCounter,World->GetTimeSeconds(),Count,Indices->Num()/3,Near,Min,Max,Behind,Before,Inside,Cross,UpperCross,Boundaries,BoundaryNear,BoundaryInside,UpperBoundaryInside,HandDepth(TEXT("hand_l")),HandDepth(TEXT("hand_r")));
 }
 void Finish(bool Exit=true)
 {
  if(bFinished)return;bFinished=true;Next=nullptr;FWorldDelegates::OnWorldPostActorTick.Remove(PostTick);FWorldDelegates::OnWorldCleanup.Remove(Cleanup);UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);Key(Rogue10mInput::DodgeKey(),false);
  if(bPrepared&&Character.IsValid()){PC->HiddenPrimitiveComponents=InitialHidden;for(const auto& S:Shadows)if(S.Component.IsValid())S.Component->SetCastShadow(S.Cast);if(FramingProperty&&Arms.IsValid())FramingProperty->SetPropertyValue_InContainer(Arms.Get(),OldFraming);Appearance->bEnableOwnerArms=InitialEnable;Appearance->RefreshAppearanceCamera();Check(!Appearance->IsOwnerArmsViewActive()&&!GetDefault<URogue10mAppearanceCameraComponent>()->bEnableOwnerArms,TEXT("QA_optin_restored_product_default_OFF"));Character->SetCanBeDamaged(bInitialDamage);Character->GetCharacterMovement()->StopMovementImmediately();Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));Character->SetActorTransform(InitialTransform,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(InitialControl);}
  if(Floor.IsValid())Floor->Destroy();if(Wall.IsValid())Wall->Destroy();if(Heavy)Heavy->Set(OldHeavy,ECVF_SetByConsole);
  if(bRendered)Check(Requested==15&&Completed==15&&Pending.IsEmpty(),TEXT("all_fifteen_actual_diagnostic_captures_completed"));
  for(const auto& Image:ImageData){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Image.Width,Image.Height,Image.Pixels,Bytes);Check(FFileHelper::SaveArrayToFile(Bytes,*(Images/Image.Name)),TEXT("actual_viewport_PNG_saved"));}
  Check(FFileHelper::SaveStringToFile(Vertices,*(Output/TEXT("vertices.csv"))),TEXT("actual_CPU_skinned_vertices_written"));Check(FFileHelper::SaveStringToFile(Geometry,*(Output/TEXT("geometry.csv"))),TEXT("actual_triangle_and_boundary_projection_written"));Check(FFileHelper::SaveStringToFile(Captures,*(Output/TEXT("captures.csv"))),TEXT("actual_request_capture_metadata_written"));
  Results+=FString::Printf(TEXT("dodge_key=%s\n"),*Rogue10mInput::DodgeKey().ToString());
  Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nmode=%s\nrendered=%d\nrequested_captures=%d\ncompleted_captures=%d\ninput=programmatic_Controller_InputKey\nphysical_input=not_performed\nmanual_field_completion=not_performed\ngeometry_scope=actual_CPU_skinned_LOD0_vertices_and_indices_not_socket_only\nvisibility_modes=0_proxy_and_world_shadows_ON_1_proxy_view_excluded_world_shadows_ON_2_proxy_ON_owned_world_shadows_OFF\nscene_names=0_neutral_1_minus85_2_minus85_nearwall_3_minus85_active_E_4_minus85_wall_E_recovered\npose_policy=no_animation_or_world_time_freeze_no_asset_edit_each_capture_has_real_time_and_hand_pose\nprojection_scope=conventional_world_camera_FOV_1280x720_not_material_opacity_or_TAA_pixel_occlusion\nboundary_scope=render_index_boundaries_include_material_UV_seams_not_all_are_derivative_cut_surfaces\nvisual_acceptance=requires_real_image_review_not_numeric_PASS\n"),Checks,Failures,*Mode,bRendered,Requested,Completed);
  Check(FFileHelper::SaveStringToFile(Results,*(Output/TEXT("results.txt"))),TEXT("results_written"));UE_LOG(LogRogue10m,Display,TEXT("RESULT=OWNER_FRAMING_%s checks=%d failures=%d output=%s"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,*Output);if(Exit)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),.5f);
 }
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ARogue10mPlayerController> PC;TWeakObjectPtr<ARogue10mCharacter> Character;TWeakObjectPtr<URogue10mAppearanceCameraComponent> Appearance;TWeakObjectPtr<URogue10mOwnerArmsComponent> Arms;TWeakObjectPtr<USkeletalMeshComponent> Body,Source;TWeakObjectPtr<USkinnedMeshComponent> Proxy;TWeakObjectPtr<URogue10mCombatComponent> Combat;TWeakObjectPtr<URogue10mProgressionSubsystem> Saves;TWeakObjectPtr<AActor> Floor,Wall;TWeakObjectPtr<UStaticMesh> Cube;
 TArray<FShadow> Shadows;TArray<TWeakObjectPtr<UPrimitiveComponent>> InitialHidden;TArray<FImage> ImageData;FString Label,Mode,Output,Images,Pending,Results,Captures,Vertices,Geometry;FTransform InitialTransform,Fixture;FRotator InitialControl;FDelegateHandle PostTick,Cleanup,CaptureHandle;TFunction<void()> Next;IConsoleVariable* Heavy=nullptr;FBoolProperty* FramingProperty=nullptr;
 double Deadline=0,NextAt=0,WaitDeadline=0,ReadyAfter=0,Surface=0,ReleaseDodgeKeyAt=0,CandidateReadyBy=0;int32 Scene=0,VisibilityMode=0,Checks=0,Failures=0,Requested=0,Completed=0,OldHeavy=0;bool bPrepared=false,bFinished=false,bRendered=false,bInitialDamage=true,InitialEnable=false,bWantCapture=false,bDodgeRequested=false,bSawDodge=false,bDodgeKeyDown=false,OldFraming=false;
};
static TSharedPtr<FRun> Active;
static void Run(){if(!GEngine||GIsEditor||(Active&&!Active->Finished()))return;for(const FWorldContext& C:GEngine->GetWorldContexts())if(UWorld* W=C.World();W&&C.WorldType==EWorldType::Game){Active=MakeShared<FRun>();Active->Start(*W);return;}UE_LOG(LogRogue10m,Error,TEXT("RESULT=OWNER_FRAMING_FAILED no_game_world"));FPlatformMisc::RequestExitWithStatus(false,1);}
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestOwnerFraming"),TEXT("Exact fresh OwnerFraming QA profiles. Real proxy/shadow/CPU skin projection evidence, not visual acceptance."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
