"""Correct river foam on an owned material instance without editing the asset pack."""
import json
from pathlib import Path
import unreal

MAP = '/Game/Rogue10m/Maps/OpenRunMap'
MATERIAL = '/Game/Rogue10m/World/OpenWorld/Materials/MI_OpenWorldRiver'
EVIDENCE = Path('D:/Project/Rogue10m/Feature/doc/evidence/open-world-20261005')


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Requires the idle OpenRunMap editor world')
    if not levels.save_current_level():
        raise RuntimeError('Cannot checkpoint the map')
    parent = unreal.load_asset('/Game/Stylized_Village/Assets/VFX/Water/Material/MI_Water')
    if not parent:
        raise RuntimeError('Missing source water material')
    material = unreal.load_asset(MATERIAL)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_OpenWorldRiver', MATERIAL.rsplit('/', 1)[0],
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if not isinstance(material, unreal.MaterialInstanceConstant):
        raise RuntimeError('Unexpected owned material type')
    library = unreal.MaterialEditingLibrary
    library.set_material_instance_parent(material, parent)
    library.set_material_instance_scalar_parameter_value(material, 'Foam Intensity', 0.0)
    library.set_material_instance_vector_parameter_value(
        material, 'Color1', unreal.LinearColor(0.025, 0.09, 0.18, 1))
    library.set_material_instance_vector_parameter_value(
        material, 'Color2', unreal.LinearColor(0.04, 0.30, 0.35, 1))
    library.update_material_instance(material)
    if library.get_material_instance_scalar_parameter_value(material, 'Foam Intensity') != 0:
        raise RuntimeError('Foam override readback failed')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rivers = [actor for actor in actors.get_all_level_actors()
              if actor.get_actor_label().startswith('OW_River_')
              and 'Rogue10m.OpenWorld' in map(str, actor.get_editor_property('tags'))]
    if len(rivers) != 17:
        raise RuntimeError('Expected 15 river panels and 2 rounded-end panels')
    for actor in rivers:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component or component.get_collision_profile_name() != 'NoCollision':
            raise RuntimeError('Unexpected river component')
        # SetMaterial changes render state but does not dirty an existing OFPA package.
        actor.modify()
        component.modify()
        component.set_material(0, material)
        if component.get_material(0) != material:
            raise RuntimeError('River material assignment failed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError('Cannot save owned water material')
    if not levels.save_current_level():
        raise RuntimeError('Cannot save river assignments')
    report = {'status': 'saved', 'material': MATERIAL, 'parent': parent.get_path_name(),
              'river_panels': len(rivers), 'foam_intensity': 0,
              'collision_changed': False, 'source_material_changed': False}
    (EVIDENCE / 'water.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('[OpenWorld] Saved owned river material and 17 panel assignments')


if __name__ == '__main__':
    main()
