"""Disposable editor-only human boss pose staging. Never saves a map or original asset."""
import unreal,json
from pathlib import Path
PROJECT=Path(unreal.Paths.project_dir()).resolve()
OUT=PROJECT/'DevLog/20261110_HumanoidBossNpc/Boss'
ROOT='/Game/Rogue10m/Characters/Boss/AzathorHuman20261110'
KEY='_human_boss_pose_preview_20261110'
def setup():
 if getattr(unreal,KEY,None): raise RuntimeError('Preview exists; finish before restart')
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 actor=sub.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(1000,-1600,5),unreal.Rotator(pitch=0,yaw=-90,roll=0),transient=True)
 actor.set_actor_label('QA_HumanoidBoss_20261110')
 comp=actor.skeletal_mesh_component
 mesh=unreal.load_asset('/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody')
 comp.set_skeletal_mesh_asset(mesh);comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 comp.set_update_animation_in_editor(True)
 comp.set_component_tick_enabled(True)
 comp.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
 setattr(unreal,KEY,actor)
 OUT.mkdir(parents=True,exist_ok=True)
 bp=unreal.load_asset('/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_FinalBoss')
 cdo=unreal.get_default_object(bp.generated_class())
 cap=cdo.get_editor_property('capsule_component')
 data=cdo.get_monster_data()
 report={'capsule_half_height':cap.get_unscaled_capsule_half_height(),'capsule_radius':cap.get_unscaled_capsule_radius(),'cdo_scale':str(cdo.get_actor_scale3d()),'monster_data':data.get_path_name(),'mesh_relative_location':str(data.get_editor_property('mesh_relative_location')),'materials':[]}
 for entry in mesh.get_editor_property('materials'):
  mat=entry.material_interface
  report['materials'].append({'slot':str(entry.material_slot_name),'material':mat.get_path_name(),'vectors':list(map(str,unreal.MaterialEditingLibrary.get_vector_parameter_names(mat))),'scalars':list(map(str,unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)))})
 (OUT/'preview-inspection.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 pose('Idle',0)
 return report
def pose(name,seconds):
 actor=getattr(unreal,KEY,None)
 if not actor: raise RuntimeError('No preview')
 clip=unreal.load_asset(ROOT+'/A_HumanBoss_'+name)
 comp=actor.skeletal_mesh_component
 comp.play_animation(clip,False);comp.set_play_rate(0);comp.set_position(seconds,False)
 return {'clip':clip.get_path_name(),'time':seconds}
def finish():
 actor=getattr(unreal,KEY,None)
 if actor: unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
 setattr(unreal,KEY,None)

