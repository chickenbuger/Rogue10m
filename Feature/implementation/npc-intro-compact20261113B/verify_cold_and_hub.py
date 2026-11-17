"""Verify saved NPC references in a fresh normal Editor, then read existing Hub portals without saving."""
from pathlib import Path
from datetime import datetime, timezone
import importlib.util,json,os,unreal
R=Path('D:/Project/Rogue10m');E=R/'Feature/doc/evidence/npc-intro-compact20261113B'
launch=json.loads((E/'editor-cold-owner-launch.json').read_text(encoding='utf-8'))
assert os.getpid()==launch['owner_pid'] and os.getpid()!=launch['previous_editor_pid']
assert Path(unreal.Paths.project_dir()).resolve()==R
spec=importlib.util.spec_from_file_location('intro_contract_cold',R/'Feature/implementation/npc-intro-compact20261113B/intro_contract.py')
contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
actual=contract.capture()
before=json.loads((E/'serialized-before-cold.json').read_text(encoding='utf-8'))
assert actual['serialized']==before['serialized'],'Saved NPC content/reference mismatch; do not alter either evidence'
actual.update({'status':'SAVED_NPC_CONTRACT_IDENTICAL_AFTER_ACTUAL_COLD_RESTART',
 'verified_utc':datetime.now(timezone.utc).isoformat(),'owned_editor_pid':os.getpid(),
 'previous_editor_pid':launch['previous_editor_pid'],'actual_cold_process':True,'serialized_equal':True,
 'runtime_quest_progress_persistence_tested':False,'metadata_clone_QA':False})
with (E/'cold-reload-result.json').open('x',encoding='utf-8') as f:json.dump(actual,f,ensure_ascii=False,indent=2)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Rogue10m/Maps/StartHub')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()=='/Game/Rogue10m/Maps/StartHub.StartHub'
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
portal_descs=[d for d in descs if 'RunPortal' in str(d.get_editor_property('native_class'))]
unreal.WorldPartitionBlueprintLibrary.load_actors([d.get_editor_property('guid') for d in portal_descs])
portals=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.Rogue10mRunPortal):
  pos=a.get_actor_location()
  portals.append({'path':a.get_path_name(),'label':a.get_actor_label(),'class':a.get_class().get_path_name(),
   'target_level_name':str(a.get_editor_property('target_level_name')),
   'start_run_timer_on_travel':a.get_editor_property('start_run_timer_on_travel'),
   'additional_travel_options':a.get_editor_property('additional_travel_options'),
   'location_cm':[pos.x,pos.y,pos.z]})
dirty={'maps':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
       'content':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
out={'status':'NORMAL_HUB_PORTALS_READ_ONLY','world':world.get_path_name(),'portals':portals,
 'candidate_portal_count':sum(p['target_level_name']==contract.MAP for p in portals),
 'dirty':dirty,'hub_actor_or_map_saved':False,'owner_pid':os.getpid()}
with (E/'hub-portals-live.json').open('x',encoding='utf-8') as f:json.dump(out,f,ensure_ascii=False,indent=2)
assert dirty=={'maps':[],'content':[]},dirty
with (E/'editor-C-normal-exit-request.json').open('x',encoding='utf-8') as f:json.dump({'owner_pid':os.getpid(),'dirty':dirty,'method':'public SystemLibrary.quit_editor','metadata_clone_QA':False},f,indent=2)
print(json.dumps({'cold_reload':actual['status'],'validation_before':actual['transient_validation_before'],
 'validation_after':actual['validation_after'],'hub_portals':out},ensure_ascii=False))
unreal.SystemLibrary.quit_editor()
