"""Load only the H authoring context through public WP APIs; no assets changed or saved."""
import json
from pathlib import Path
import unreal
R = Path('D:/Project/Rogue10m'); E = R/'Feature/doc/evidence/npc-intro-compact20261113B'
MAP = '/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert Path(unreal.Paths.project_dir()).resolve()==R and world.get_path_name().split('.')[0]==MAP
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
box = unreal.Box(min=unreal.Vector(-4000,37550,-10000),max=unreal.Vector(6000,47550,30000))
descs = unreal.WorldPartitionBlueprintLibrary.get_intersecting_actor_descs(box)
assert descs is not None
unreal.WorldPartitionBlueprintLibrary.load_actors([d.get_editor_property('guid') for d in descs])
mode=world.get_world_settings().get_editor_property('default_game_mode')
default=unreal.get_default_object(mode) if mode else None
contracts={}
if default:
 for key in ['game_state_class','player_controller_class','default_pawn_class']:
  value=default.get_editor_property(key); contracts[key]=value.get_path_name() if value else None
presentation=unreal.load_asset('/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide')
assert presentation
mesh=presentation.get_editor_property('guide_skeletal_mesh')
idle=presentation.get_editor_property('guide_idle_animation')
assert mesh and idle
out={'status':'hub_context_loaded_no_save','world':world.get_path_name(),
 'loaded_descriptor_count':len(descs),'selected_packages':[str(d.get_editor_property('actor_package')) for d in descs],
 'mode_class':mode.get_path_name() if mode else None,'mode_contracts':contracts,
 'presentation':presentation.get_path_name(),'mesh':mesh.get_path_name(),'idle':idle.get_path_name(),
 'mesh_skeleton':mesh.get_editor_property('skeleton').get_path_name(),
 'idle_skeleton':idle.get_editor_property('skeleton').get_path_name(),
 'dirty_maps':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
 'dirty_content':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
with (E/'hub-context-A.json').open('x',encoding='utf-8') as f:json.dump(out,f,ensure_ascii=False,indent=2)
assert not out['dirty_maps'] and not out['dirty_content']
assert out['mesh_skeleton']==out['idle_skeleton']
print(json.dumps({k:v for k,v in out.items() if k!='selected_packages'},ensure_ascii=False))
