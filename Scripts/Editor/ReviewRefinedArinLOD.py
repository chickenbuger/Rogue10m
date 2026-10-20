"""Temporary component-only LOD review. Never modify/save a package."""
import unreal


def component():
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0]!='/Game/Rogue10m/Maps/StartHub':
        raise RuntimeError('Requires idle StartHub editor')
    guides=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if a.get_actor_label()=='SV_TutorialGuide_Arin']
    if len(guides)!=1:
        raise RuntimeError('Expected exactly one existing Arin guide')
    mesh=guides[0].get_editor_property('guide_mesh')
    if mesh.get_skeletal_mesh_asset().get_path_name().split('.')[0]!='/Game/Rogue10m/Characters/Arin/RefinedV2/SK_Arin':
        raise RuntimeError('LOD review requires the actual refined mesh')
    return mesh


def inspect():
    mesh=component()
    performance=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    return {'forced_value':mesh.get_forced_lod(),'predicted_lod_index':mesh.get_predicted_lod_level(),
            'update_animation_in_editor':bool(mesh.get_editor_property('update_animation_in_editor')),
            'background_throttle':bool(performance.get_editor_property('bThrottleCPUWhenNotForeground')),
            'mesh':mesh.get_skeletal_mesh_asset().get_path_name(),'asset_saved':False}


def request(value,update_animation=None,background_throttle=None):
    if value not in (0,1,2,3):
        raise ValueError('0=automatic, 1/2/3=LOD0/1/2')
    mesh=component()
    previous=mesh.get_forced_lod()
    previous_update=bool(mesh.get_editor_property('update_animation_in_editor'))
    performance=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    previous_throttle=bool(performance.get_editor_property('bThrottleCPUWhenNotForeground'))
    if background_throttle is not None:
        # Temporary runtime CDO setting only. Never SaveConfig or save assets.
        performance.set_editor_property('bThrottleCPUWhenNotForeground',bool(background_throttle))
    if update_animation is not None:
        mesh.set_update_animation_in_editor(bool(update_animation))
    mesh.set_forced_lod(value)
    result=inspect()
    result['previous_forced_value']=previous
    result['previous_update_animation_in_editor']=previous_update
    result['previous_background_throttle']=previous_throttle
    return result
