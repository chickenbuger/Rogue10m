"""Repair stretched StartHub floor UVs using owned, world-mapped materials."""
import json
from pathlib import Path
import unreal

MAP = '/Game/Rogue10m/Maps/StartHub'
ROOT = '/Game/Rogue10m/World/StartVillage/Materials'
MASTER = ROOT + '/M_StartVillage_GroundDetail'
EVIDENCE = Path('D:/Project/Rogue10m/Feature/doc/evidence/village-ground-20261005')
STONE_LABELS = {'SV_CentralStoneSquare', 'SV_PortalStonePad'} | {
    'SV_' + role + '_ReservedNPCPad' for role in ['Equipment', 'Traits', 'Difficulty', 'Story']}
PROFILES = {
    'grass': ('MI_StartVillage_Grass', 'T_Ground_Grass', 180.0, (0.72, 0.86, 0.65), 0.9),
    'dirt': ('MI_StartVillage_EarthPath', 'T_Ground_Gravel', 90.0, (0.86, 0.78, 0.65), 0.88),
    'stone': ('MI_StartVillage_Cobblestone', 'T_CobbleStone_Rough', 120.0, (0.72, 0.67, 0.56), 0.86),
}


def texture(name):
    value = unreal.load_asset('/Game/StarterContent/Textures/' + name)
    if not isinstance(value, unreal.Texture2D):
        raise RuntimeError('Missing texture ' + name)
    return value


def create_master():
    material = unreal.load_asset(MASTER)
    library = unreal.MaterialEditingLibrary
    if material:
        complete = (isinstance(material, unreal.Material)
                    and {'TileSizeCm', 'Roughness'} <= set(map(str, library.get_scalar_parameter_names(material)))
                    and 'ColorTint' in map(str, library.get_vector_parameter_names(material))
                    and {'BaseColorTexture', 'NormalTexture'} <= set(map(str, library.get_texture_parameter_names(material))))
        if complete:
            errors = list(library.recompile_material(material))
            if errors:
                raise RuntimeError('Owned material compile errors: ' + str(errors))
            return material
        # Only the known four-node, unpublished staging graph may be repaired here.
        asset_file = Path('D:/Project/Rogue10m/Content/Rogue10m/World/StartVillage/Materials/M_StartVillage_GroundDetail.uasset')
        expressions = list(library.get_material_expressions(material))
        staging_classes = {'MaterialExpressionWorldPosition', 'MaterialExpressionComponentMask',
                           'MaterialExpressionScalarParameter', 'MaterialExpressionDivide'}
        if asset_file.exists() or len(expressions) != 4 or {n.get_class().get_name() for n in expressions} != staging_classes:
            raise RuntimeError('Unexpected incomplete owned master; inspect before retrying')
        for expression in expressions:
            library.delete_material_expression(material, expression)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MASTER.rsplit('/', 1)[-1], ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('Cannot create ground master')

    def node(kind, x, y, **properties):
        result = library.create_material_expression(material, kind, x, y)
        for name, value in properties.items():
            result.set_editor_property(name, value)
        return result

    def wire(source, output, target, pin):
        if not library.connect_material_expressions(source, output, target, pin):
            raise RuntimeError('Failed material connection: ' + pin)

    world = node(unreal.MaterialExpressionWorldPosition, -1100, 0)
    size = node(unreal.MaterialExpressionScalarParameter, -900, 190,
                parameter_name='TileSizeCm', default_value=180.0, slider_min=50.0, slider_max=500.0)
    uv = node(unreal.MaterialExpressionDivide, -680, 0)
    wire(world, 'XY', uv, 'A')
    wire(size, '', uv, 'B')
    diffuse = node(unreal.MaterialExpressionTextureSampleParameter2D, -460, -100,
                   parameter_name='BaseColorTexture', texture=texture('T_Ground_Grass_D'),
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = node(unreal.MaterialExpressionTextureSampleParameter2D, -460, 240,
                  parameter_name='NormalTexture', texture=texture('T_Ground_Grass_N'),
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wire(uv, '', diffuse, 'UVs')
    wire(uv, '', normal, 'UVs')
    tint = node(unreal.MaterialExpressionVectorParameter, -220, -350,
                parameter_name='ColorTint', default_value=unreal.LinearColor(0.72, 0.86, 0.65, 1))
    tinted = node(unreal.MaterialExpressionMultiply, -150, -100)
    wire(diffuse, 'RGB', tinted, 'A')
    wire(tint, 'RGB', tinted, 'B')
    macro_uv = node(unreal.MaterialExpressionDivide, -680, 570, const_b=1800.0)
    wire(world, 'XY', macro_uv, 'A')
    macro = node(unreal.MaterialExpressionTextureSample, -460, 570,
                 texture=texture('T_MacroVariation'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    wire(macro_uv, '', macro, 'UVs')
    contrast = node(unreal.MaterialExpressionMultiply, -220, 570, const_b=0.15)
    wire(macro, 'R', contrast, 'A')
    brightness = node(unreal.MaterialExpressionAdd, -20, 570, const_b=0.85)
    wire(contrast, '', brightness, 'A')
    color = node(unreal.MaterialExpressionMultiply, 170, -100)
    wire(tinted, '', color, 'A')
    wire(brightness, '', color, 'B')
    roughness = node(unreal.MaterialExpressionScalarParameter, 180, 200,
                     parameter_name='Roughness', default_value=0.9)
    for expression, output, prop in [(color, '', unreal.MaterialProperty.MP_BASE_COLOR),
                                     (normal, 'RGB', unreal.MaterialProperty.MP_NORMAL),
                                     (roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)]:
        if not library.connect_material_property(expression, output, prop):
            raise RuntimeError('Failed ground output connection')
    library.layout_material_expressions(material)
    errors = list(library.recompile_material(material))
    if errors:
        raise RuntimeError('Material compile errors: ' + str(errors))
    return material


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Requires idle StartHub')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    stone_actors = [a for a in actors if a.get_actor_label() in STONE_LABELS
                    and 'Rogue10m.StartVillage' in map(str, a.get_editor_property('tags'))]
    if len(stone_actors) != 6 or not (EVIDENCE / 'before-state.json').exists() or not (EVIDENCE / 'baseline-content.json').exists():
        raise RuntimeError('Missing preflight actors or preservation checkpoints')
    if not levels.save_current_level():
        raise RuntimeError('Cannot checkpoint StartHub')
    master = create_master()
    library = unreal.MaterialEditingLibrary
    report = {'map': MAP, 'master': MASTER, 'profiles': {}, 'actor_packages': [], 'stone_actors': []}
    for key, (name, prefix, size, tint, roughness) in PROFILES.items():
        instance = unreal.load_asset(ROOT + '/' + name)
        if not instance:
            if key != 'stone':
                raise RuntimeError('Expected existing owned grass/path instance')
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError('Unexpected owned material type')
        instance.modify()
        library.set_material_instance_parent(instance, master)
        library.set_material_instance_texture_parameter_value(instance, 'BaseColorTexture', texture(prefix + '_D'))
        library.set_material_instance_texture_parameter_value(instance, 'NormalTexture', texture(prefix + '_N'))
        library.set_material_instance_scalar_parameter_value(instance, 'TileSizeCm', size)
        library.set_material_instance_scalar_parameter_value(instance, 'Roughness', roughness)
        library.set_material_instance_vector_parameter_value(instance, 'ColorTint', unreal.LinearColor(*tint, 1))
        library.update_material_instance(instance)
        if abs(library.get_material_instance_scalar_parameter_value(instance, 'TileSizeCm') - size) > 0.001:
            raise RuntimeError('Physical tile size readback failed')
        for parameter, expected in [('BaseColorTexture', texture(prefix + '_D')), ('NormalTexture', texture(prefix + '_N'))]:
            if library.get_material_instance_texture_parameter_value(instance, parameter) != expected:
                raise RuntimeError('Texture readback failed')
        if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
            raise RuntimeError('Cannot save ground instance')
        report['profiles'][key] = {'instance': instance.get_path_name(), 'tile_size_cm': size,
                                   'base_color': texture(prefix + '_D').get_path_name(),
                                   'normal': texture(prefix + '_N').get_path_name()}
        if key == 'stone':
            for actor in stone_actors:
                actor.modify()
                component = actor.static_mesh_component
                component.modify()
                component.set_material(0, instance)
                report['stone_actors'].append(actor.get_actor_label())
                report['actor_packages'].append(actor.get_package().get_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(master, only_if_is_dirty=False) or not levels.save_current_level():
        raise RuntimeError('Cannot save master/map')
    report['status'] = 'saved'
    (EVIDENCE / 'application.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
