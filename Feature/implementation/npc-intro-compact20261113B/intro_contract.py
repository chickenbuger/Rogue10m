"""Read one authored intro asset and its owning Director in the normal shared editor."""
import unreal
MAP='/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
DA='/Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B'

def vector(value): return [value.x,value.y,value.z]
def rotation(value): return [value.pitch,value.yaw,value.roll]
def path(value): return value.get_path_name() if value else None

def capture():
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world.get_path_name()==MAP+'.WorldLayoutCompact20261113A'
 assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
 asset=unreal.load_asset(DA); assert asset
 transient_before=bool(asset.get_editor_property('last_validation_passed'))
 asset.validate_configuration(); assert asset.get_editor_property('last_validation_passed') is True
 values=[]
 for row in asset.get_editor_property('quests'):
  get=row.get_editor_property
  presentation=get('objective_presentation')
  values.append({'enabled':bool(get('enabled')),'quest_id':str(get('quest_id')),'village_id':str(get('village_id')),
   'village_name':str(get('village_name')),'npc_name':str(get('npc_name')),'title':str(get('title')),
   'description':str(get('description')),'in_progress_text':str(get('progress_text')),
   'ready_text':str(get('ready_text')),'completed_text':str(get('completed_text')),
   'npc_location':vector(get('npc_location')),'npc_rotation':rotation(get('npc_rotation')),
   'npc_presentation':path(get('npc_presentation')),'npc_interaction_radius_cm':get('npc_interaction_radius_cm'),
   'npc_detection_radius_cm':get('npc_detection_radius_cm'),'objective_labels':[str(x) for x in get('objective_labels')],
   'objective_locations':[vector(x) for x in get('objective_locations')],
   'goal_radius_cm':presentation.get_editor_property('radius_cm'),
   'goal_marker_mesh':path(presentation.get_editor_property('marker_mesh')),
   'goal_marker_scale':vector(presentation.get_editor_property('marker_scale')),
   'goal_show_marker':presentation.get_editor_property('show_marker'),
   'goal_show_label':presentation.get_editor_property('show_label'),
   'combat_index':get('combat_objective_index'),'reward_gold':get('reward_gold'),
   'reward_item':path(get('reward_item')),'reward_quantity':get('reward_quantity'),
   'clue_objective_index':get('supply_clue_objective_index'),
   'offers_clue_analysis':get('offers_clue_analysis'),'accepts_clue_report':get('accepts_clue_report')})
 directors=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if isinstance(a,unreal.Rogue10mVillageQuestDirector)]
 assert len(directors)==1
 director=directors[0]
 assert director.get_editor_property('quest_definition')==asset
 descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
 selected=[d for d in descs if str(d.get_editor_property('actor_path'))==director.get_path_name()]
 assert len(selected)==1
 desc=selected[0]
 dirty={'maps':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
        'content':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
 assert dirty=={'maps':[],'content':[]},dirty
 serialized={'world':world.get_path_name(),'data_asset':asset.get_path_name(),
  'supported_world':path(asset.get_editor_property('supported_world')),
  'layout_origin_cm':vector(asset.get_editor_property('layout_origin_cm')),
  'layout_rotation':rotation(asset.get_editor_property('layout_rotation')),'rows':values,
  'director':director.get_path_name(),'director_label':director.get_actor_label(),
  'director_definition':path(director.get_editor_property('quest_definition')),
  'director_spatially_loaded':director.get_editor_property('is_spatially_loaded'),
  'director_editor_only':director.get_editor_property('is_editor_only_actor'),
  'director_data_layers':[str(x) for x in desc.get_editor_property('data_layer_assets')],
  'director_package':str(desc.get_editor_property('actor_package')),
  'mode':path(world.get_world_settings().get_editor_property('default_game_mode'))}
 assert [q['enabled'] for q in values]==[True,False,False,False,False]
 assert serialized['director_spatially_loaded'] is False and not serialized['director_data_layers']
 return {'serialized':serialized,'transient_validation_before':transient_before,
         'validation_after':True,'dirty':dirty,'runtime_quest_progress_checked':False}
