"""Read-only StartHub ground materials, texture inputs and actor checkpoint."""
import json
from pathlib import Path
import unreal

ROOT = Path('D:/Project/Rogue10m')
EVIDENCE = ROOT / 'Feature/doc/evidence/village-ground-20261005'
MATERIAL_ROOT = '/Game/Rogue10m/World/StartVillage/Materials'


def actor_state(actor):
    row = {'label': actor.get_actor_label(), 'class': actor.get_class().get_name(),
           'location': list(actor.get_actor_location().to_tuple()),
           'rotation': list(actor.get_actor_rotation().to_tuple()),
           'scale': list(actor.get_actor_scale3d().to_tuple())}
    if isinstance(actor, unreal.StaticMeshActor):
        component = actor.static_mesh_component
        row.update(mesh=component.static_mesh.get_path_name() if component.static_mesh else None,
                   collision=str(component.get_collision_profile_name()),
                   materials=[component.get_material(i).get_path_name() if component.get_material(i) else None
                              for i in range(component.get_num_materials())])
    if isinstance(actor, unreal.Rogue10mRunPortal):
        row['portal_target'] = str(actor.get_editor_property('target_level_name'))
    return row


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != '/Game/Rogue10m/Maps/StartHub':
        raise RuntimeError('Requires the idle StartHub editor')
    library = unreal.MaterialEditingLibrary
    report = {'world': world.get_path_name(), 'materials': {}, 'textures': {}, 'actors': []}
    for name in ['MI_StartVillage_Grass', 'MI_StartVillage_EarthPath']:
        material = unreal.load_asset(MATERIAL_ROOT + '/' + name)
        report['materials'][name] = {'parent': material.get_editor_property('parent').get_path_name(),
                                    'scalar_parameters': list(map(str, library.get_scalar_parameter_names(material))),
                                    'vector_parameters': list(map(str, library.get_vector_parameter_names(material)))}
    for name in ['M_Ground_Grass', 'M_Ground_Gravel']:
        material = unreal.load_asset('/Game/StarterContent/Materials/' + name)
        samples = []
        for node in library.get_material_expressions(material):
            if isinstance(node, unreal.MaterialExpressionTextureSample):
                texture = node.get_editor_property('texture')
                samples.append({'expression': node.get_name(), 'texture': texture.get_path_name() if texture else None})
        report['materials'][name] = {'texture_samples': samples}
    paths = ['/Game/StarterContent/Textures/' + name for name in
             ['T_Ground_Grass_D', 'T_Ground_Grass_N', 'T_Ground_Gravel_D', 'T_Ground_Gravel_N',
              'T_CobbleStone_Rough_D', 'T_CobbleStone_Rough_N', 'T_MacroVariation']]
    for path in paths:
        texture = unreal.load_asset(path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError('Missing texture ' + path)
        report['textures'][path] = {'width': texture.blueprint_get_size_x(), 'height': texture.blueprint_get_size_y(),
                                    'lod_bias': texture.get_editor_property('lod_bias'),
                                    'max_texture_size': texture.get_editor_property('max_texture_size'),
                                    'never_stream': texture.get_editor_property('never_stream')}
    report['actors'] = [actor_state(a) for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()]
    destination = EVIDENCE / 'before-state.json'
    if destination.exists():
        raise RuntimeError('Existing baseline must be preserved')
    destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'actors': len(report['actors']), 'materials': report['materials'], 'textures': report['textures']}))


if __name__ == '__main__':
    main()
