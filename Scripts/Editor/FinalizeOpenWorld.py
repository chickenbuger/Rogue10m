"""Finalize only OpenRunMap streaming settings and the existing hub portal."""
import json
from pathlib import Path
import unreal

MAP='/Game/Rogue10m/Maps/OpenRunMap'
HUB='/Game/Rogue10m/Maps/StartHub'
EVIDENCE=Path('D:/Project/Rogue10m/Feature/doc/evidence/open-world-20261005')

def main():
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if levels.is_in_play_in_editor():
        raise RuntimeError('End PIE before finalizing')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split('.')[0]!=MAP:
        raise RuntimeError('Load OpenRunMap before finalizing')
    report={'map':MAP,'streaming_grids':[]}
    result=json.loads(unreal.Rogue10mOpenWorldAuthoring.inspect_open_world_terrain())
    runtime=unreal.load_object(None,result['runtime_hash_path'])
    if runtime.get_class().get_name()!='WorldPartitionRuntimeHashSet':
        raise RuntimeError('Unexpected runtime hash class')
    partitions=runtime.get_editor_property('RuntimePartitions')
    for entry in partitions:
        layer=entry.get_editor_property('MainLayer')
        layer.set_editor_property('CellSize',25600)
        layer.set_editor_property('LoadingRange',70000)
        report['streaming_grids'].append({'class':layer.get_class().get_name(),
            'cell_size_cm':int(layer.get_editor_property('CellSize')),
            'loading_range_cm':int(layer.get_editor_property('LoadingRange'))})
    if not report['streaming_grids']:
        raise RuntimeError('No runtime streaming grid')
    # Add rounded-end water coverage; surrounding terrain occludes the edges.
    mesh=unreal.load_asset('/Game/Stylized_Village/Assets/VFX/Water/Mesh/SM_WaterMesh')
    material=unreal.load_asset('/Game/Stylized_Village/Assets/VFX/Water/Material/MI_Water')
    for side in (-1,1):
        label='OW_River_End_%d'%side
        if any(a.get_actor_label()==label for a in actors.get_all_level_actors()):
            continue
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(side*76500,5000,2600),unreal.Rotator())
        a.set_actor_label(label)
        a.set_folder_path('OpenWorld/River')
        a.set_editor_property('tags',[unreal.Name('Rogue10m.OpenWorld')])
        a.set_editor_property('is_spatially_loaded',True)
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_collision_profile_name('NoCollision')
        a.static_mesh_component.set_material(0,material)
        a.set_actor_scale3d(unreal.Vector(4,6,1))
    if not levels.save_current_level():
        raise RuntimeError('OpenRunMap save failed')
    if not levels.load_level(HUB):
        raise RuntimeError('Cannot load StartHub')
    portals=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.Rogue10mRunPortal)
             and 'Rogue10m.StartVillage' in map(str,a.get_editor_property('tags'))]
    if len(portals)!=1:
        raise RuntimeError('Expected one owned hub portal')
    portal=portals[0]
    report['portal']={'old_target':str(portal.get_editor_property('target_level_name')),
                      'location':[portal.get_actor_location().x,portal.get_actor_location().y,portal.get_actor_location().z]}
    portal.set_editor_property('target_level_name',unreal.Name(MAP))
    portal.set_editor_property('start_run_timer_on_travel',True)
    portal.set_actor_label('SV_RunPortal_ToOpenWorld')
    report['portal']['target']=str(portal.get_editor_property('target_level_name'))
    report['portal']['starts_run']=bool(portal.get_editor_property('start_run_timer_on_travel'))
    if report['portal']['target']!=MAP or not report['portal']['starts_run']:
        raise RuntimeError('Portal readback failed')
    if not levels.save_current_level():
        raise RuntimeError('Hub portal save failed')
    report['status']='saved'
    (EVIDENCE/'connection.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('[OpenWorld] Saved streaming grid and StartHub→OpenRunMap portal')

if __name__=='__main__':
    main()
