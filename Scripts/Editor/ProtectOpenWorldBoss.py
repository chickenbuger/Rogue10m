"""Keep the persistent boss grounded before distant Landscape cells load."""
import unreal

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if levels.is_in_play_in_editor():
    raise RuntimeError('PIE must be idle')
if not levels.load_level('/Game/Rogue10m/Maps/OpenRunMap'):
    raise RuntimeError('Map load failed')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matches=[a for a in actors.get_all_level_actors() if a.get_actor_label()=='OW_PersistentBossGround']
if not matches:
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(15000,68000,9550),unreal.Rotator())
    actor.set_actor_label('OW_PersistentBossGround')
    actor.set_folder_path('OpenWorld/Gameplay')
    actor.set_editor_property('tags',[unreal.Name('Rogue10m.OpenWorld')])
    actor.set_editor_property('is_spatially_loaded',False)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    actor.static_mesh_component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(110,110,1))
    actor.set_actor_hidden_in_game(True)
if not levels.save_current_level():
    raise RuntimeError('Boss collision checkpoint save failed')
print('Persistent boss collision plane saved; top Z9600 matches the authored plateau.')
