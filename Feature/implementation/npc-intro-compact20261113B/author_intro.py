"""Add one new authored DA and one new Director to the backed-up normal candidate only."""
from pathlib import Path
from datetime import datetime, timezone
import importlib.util, json
import unreal

R=Path('D:/Project/Rogue10m'); E=R/'Feature/doc/evidence/npc-intro-compact20261113B'
MAP='/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
DA='/Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B'
PREFIX='/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A/'
OBJECTS='/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A/'
TAG='Rogue10m.Compact.IntroSupply20261113B'
assert Path(unreal.Paths.project_dir()).resolve()==R
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()==MAP+'.WorldLayoutCompact20261113A' and not levels.is_in_play_in_editor()
assert not (E/'authoring-result.json').exists()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
before=json.loads((E/'physical-checkpoint.json').read_text(encoding='utf-8'))
assert before['backup_files']==1419 and before['map']==MAP
before_paths={row['path'] for row in before['files']}
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
assert descs is not None
assert not any(d.get_editor_property('native_class')==unreal.Rogue10mVillageQuestDirector.static_class() for d in descs)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not any(isinstance(a,unreal.Rogue10mVillageQuestDirector) for a in actors.get_all_level_actors())
assert not unreal.EditorAssetLibrary.does_asset_exist(DA)
assert not (R/'Content'/DA.removeprefix('/Game/')).with_suffix('.uasset').exists()

# MCP trace_world returns distance along the supplied vertical ray, not ground Z.
grounds=[]
for tag in ['npc','goal0','goal1']:
 reply=json.loads((E/('trace-'+tag+'.json')).read_text(encoding='utf-8'))
 value=json.loads(reply['result']['content'][0]['text'])['returnValue']
 assert isinstance(value,(int,float)) and 0<value<20000
 grounds.append(20000.0-value)
assert max(grounds)-min(grounds)<1.0
ground=grounds[0]
spec=importlib.util.spec_from_file_location('compact_intro_template',R/'Scripts/Editor/CreateIntroVillageQuestDefinition20261113.py')
template=importlib.util.module_from_spec(spec); spec.loader.exec_module(template)
creation=template.create_intro_definition(destination=DA,supported_world=MAP,execute=True,
 owner_handoff='Root owns normal shared Editor PID 50968; candidate backup/input/save hashes verified',build_verified=True)
assert creation['status']=='definition_saved'
asset=unreal.load_asset(DA)
asset.modify()
asset.set_editor_property('layout_origin_cm',unreal.Vector(1000,42550,ground))
asset.set_editor_property('layout_rotation',unreal.Rotator(0,0,0))
rows=list(asset.get_editor_property('quests'))
assert len(rows)==5 and [bool(q.get_editor_property('enabled')) for q in rows]==[True,False,False,False,False]
first=rows[0]
first.set_editor_property('npc_location',unreal.Vector(0,0,2))
first.set_editor_property('npc_rotation',unreal.Rotator(pitch=0,yaw=-35.5,roll=0))
first.set_editor_property('npc_presentation',unreal.load_asset('/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide'))
first.set_editor_property('objective_locations',[unreal.Vector(-500,400,24.5),unreal.Vector(500,400,24.5)])
asset.set_editor_property('quests',rows)
asset.validate_configuration()
assert asset.get_editor_property('last_validation_passed') is True
assert asset.get_enabled_quest_count()==1
assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()

director=actors.spawn_actor_from_class(unreal.Rogue10mVillageQuestDirector,unreal.Vector(1000,42550,ground))
assert director and director.get_world()==world
director.set_actor_label('CompactIntro_Serin_Director')
director.set_folder_path('WorldLayoutCompact20261113A/Quests/IntroSupply')
director.set_editor_property('quest_definition',asset)
director.set_editor_property('is_spatially_loaded',False)
director.set_editor_property('is_editor_only_actor',False)
director.set_editor_property('tags',[unreal.Name(TAG)])
assert director.get_editor_property('quest_definition')==asset
assert director.get_editor_property('is_spatially_loaded') is False
dirty=list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
names=[p.get_name() for p in dirty]
assert names,'New Director must have dirty packages to persist'
for package in names:
 assert package==MAP or package.startswith((PREFIX,OBJECTS)),names
 if package!=MAP:
  physical='Content/'+package.removeprefix('/Game/')+'.uasset'
  assert physical not in before_paths,'Refuse to serialize any pre-existing external actor/object package: '+physical
with (E/'authoring-save-intent.json').open('x',encoding='utf-8') as f:
 json.dump({'purpose':'new NPC content on normal candidate only','new_definition':DA,'new_director':director.get_path_name(),
            'exact_dirty_packages':names,'original_external_packages_in_save':False},f,indent=2)
assert unreal.EditorLoadingAndSavingUtils.save_packages(dirty,True)
remaining=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]+[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
assert remaining==[],remaining
out={'status':'ADDITIVE_INTRO_DA_AND_DIRECTOR_SAVED_RUNTIME_UNVERIFIED','saved_utc':datetime.now(timezone.utc).isoformat(),
 'world':world.get_path_name(),'data_asset':asset.get_path_name(),'director':director.get_path_name(),
 'saved_packages':names,'data_asset_saved':True,'ground_trace_z_cm':grounds,
 'layout_origin_cm':[1000,42550,ground],'npc_world_cm':[1000,42550,ground+2],
 'objective_world_cm':[[500,42950,ground+24.5],[1500,42950,ground+24.5]],
 'npc_rotation_yaw_degrees':-35.5,'npc_presentation':first.get_editor_property('npc_presentation').get_path_name(),
 'presentation_note':'Existing user-provided Arin mesh/Idle reused as an introductory Serin placeholder; vendor originals unchanged',
 'director_spatially_loaded':False,'director_editor_only':False,'enabled_rows':1,'stable_slots':5,
 'runtime_expected_npcs':1,'runtime_expected_collection_goals':2,'reward_gold':40,'combat_goals':0,
 'intro_native_validation_passed':True,'all_dirty_packages_after':remaining,
 'landscape_crop_performed':False,'map_size_metres':[1800,1800],
 'runtime_verified':False,'cold_reload_verified':False,'denied_metadata_clone_QA':False}
with (E/'authoring-result.json').open('x',encoding='utf-8') as f:json.dump(out,f,ensure_ascii=False,indent=2)
print(json.dumps(out,ensure_ascii=False))
