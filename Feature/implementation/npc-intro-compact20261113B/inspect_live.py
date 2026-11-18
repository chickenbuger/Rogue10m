"""Read the normal shared Compact editor world; no save, move, or new asset creation."""
import json
from pathlib import Path
import unreal

R = Path('D:/Project/Rogue10m')
E = R / 'Feature/doc/evidence/npc-intro-compact20261113B'
MAP = '/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
assert Path(unreal.Paths.project_dir()).resolve() == R
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name() == MAP + '.WorldLayoutCompact20261113A'
assert not levels.is_in_play_in_editor()

def dirty():
 return {'maps':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
         'content':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
initial = dirty()
assert initial == {'maps':[], 'content':[]}, initial
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
assert descs is not None
records = []
for desc in descs:
 get = desc.get_editor_property
 cls = get('native_class')
 box = get('bounds')
 records.append({'guid':str(get('guid')),'label':str(get('label')),'name':str(get('name')),
  'class':cls.get_path_name() if cls else None,'path':str(get('actor_path')),
  'package':str(get('actor_package')),'spatially_loaded':bool(get('is_spatially_loaded')),
  'editor_only':bool(get('actor_is_editor_only')),'data_layers':[str(x) for x in get('data_layer_assets')],
  'bounds_min':[box.min.x,box.min.y,box.min.z], 'bounds_max':[box.max.x,box.max.y,box.max.z]})
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
loaded = []
for a in actors:
 p = a.get_actor_location()
 loaded.append({'path':a.get_path_name(),'label':a.get_actor_label(),'class':a.get_class().get_path_name(),
                'location':[p.x,p.y,p.z]})
mode = world.get_world_settings().get_editor_property('default_game_mode')
directors = [x for x in records if x['class'] == '/Script/Rogue10m.Rogue10mVillageQuestDirector']
out = {'status':'read_only_live_inventory','world':world.get_path_name(),
 'default_game_mode':mode.get_path_name() if mode else None,'actor_desc_count':len(records),
 'loaded_actor_count':len(loaded),'directors':directors,
 'player_starts':[x for x in records if 'PlayerStart' in str(x['class'])],
 'hub_H':[x for x in records if x['label'].startswith('WL26_H_')],
 'actor_descriptors':records,'loaded_actors':loaded,'initial_dirty':initial,'final_dirty':dirty(),
 'packages_saved':False,'assets_created':False,'metadata_clone_QA':False}
with (E/'live-inventory-A.json').open('x',encoding='utf-8') as f: json.dump(out,f,ensure_ascii=False,indent=2)
assert out['final_dirty'] == initial
print(json.dumps({k:v for k,v in out.items() if k not in ['actor_descriptors','loaded_actors','hub_H']},ensure_ascii=False))
