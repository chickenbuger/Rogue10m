"""Read-only validation of saved StartHub ground materials and actor preservation."""
import json
from collections import Counter
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from InspectVillageGround import actor_state
from ImproveVillageGround import EVIDENCE, MAP, MASTER, ROOT, PROFILES, STONE_LABELS


def main():
    checks = []

    def check(name, condition, detail=None):
        checks.append({'name': name, 'passed': bool(condition), 'detail': detail})

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Requires idle StartHub after map reload')
    library = unreal.MaterialEditingLibrary
    master = unreal.load_asset(MASTER)
    check('master_material_type', isinstance(master, unreal.Material))
    check('master_scalar_parameters', {'TileSizeCm', 'Roughness'} <= set(map(str, library.get_scalar_parameter_names(master))))
    check('master_vector_parameter', 'ColorTint' in map(str, library.get_vector_parameter_names(master)))
    check('master_texture_parameters', {'BaseColorTexture', 'NormalTexture'} <= set(map(str, library.get_texture_parameter_names(master))))
    profiles, textures = {}, {}
    for key, (name, prefix, size, tint, roughness) in PROFILES.items():
        instance = unreal.load_asset(ROOT + '/' + name)
        check(key + '_instance_type', isinstance(instance, unreal.MaterialInstanceConstant))
        parent = instance.get_editor_property('parent')
        actual_size = library.get_material_instance_scalar_parameter_value(instance, 'TileSizeCm')
        actual_roughness = library.get_material_instance_scalar_parameter_value(instance, 'Roughness')
        actual_tint = library.get_material_instance_vector_parameter_value(instance, 'ColorTint')
        check(key + '_parent', parent == master, parent.get_path_name())
        check(key + '_physical_tile_size', abs(actual_size - size) < 0.001, actual_size)
        check(key + '_roughness', abs(actual_roughness - roughness) < 0.001, actual_roughness)
        check(key + '_tint', all(abs(getattr(actual_tint, c) - v) < 0.001
                                for c, v in zip(('r', 'g', 'b', 'a'), (*tint, 1.0))))
        profiles[key] = {'instance': instance.get_path_name(), 'tile_size_cm': actual_size,
                         'parent': parent.get_path_name()}
        for parameter, suffix in [('BaseColorTexture', '_D'), ('NormalTexture', '_N')]:
            path = '/Game/StarterContent/Textures/' + prefix + suffix
            texture = library.get_material_instance_texture_parameter_value(instance, parameter)
            check(key + '_' + parameter, isinstance(texture, unreal.Texture2D) and texture.get_path_name().split('.')[0] == path)
            width, height = texture.blueprint_get_size_x(), texture.blueprint_get_size_y()
            textures[path] = {'settled_runtime_width': width, 'settled_runtime_height': height,
                              'lod_bias': texture.get_editor_property('lod_bias'),
                              'max_texture_size': texture.get_editor_property('max_texture_size')}
            check(key + '_' + parameter + '_settled_size', min(width, height) >= 1024, [width, height])
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    current = [actor_state(actor) for actor in actors]
    baseline = json.loads((EVIDENCE / 'before-state.json').read_text(encoding='utf-8'))['actors']
    check('actor_count_preserved', len(current) == len(baseline), {'before': len(baseline), 'after': len(current)})

    def signature(row):
        row = dict(row)
        if row['label'] in STONE_LABELS:
            row.pop('materials', None)
        return json.dumps(row, sort_keys=True)

    previous_counts = Counter(signature(row) for row in baseline)
    current_counts = Counter(signature(row) for row in current)
    difference = {'missing': list((previous_counts - current_counts).elements()),
                  'unexpected': list((current_counts - previous_counts).elements())}
    check('all_actor_transforms_meshes_collision_portals_preserved', previous_counts == current_counts, difference)
    stone = ROOT + '/MI_StartVillage_Cobblestone.MI_StartVillage_Cobblestone'
    for label in sorted(STONE_LABELS):
        found = [row for row in current if row['label'] == label]
        check(label + '_saved_material_slot', len(found) == 1 and found[0].get('materials') == [stone])
    for portal in (a for a in actors if isinstance(a, unreal.Rogue10mRunPortal)):
        check('open_world_portal_target', str(portal.get_editor_property('target_level_name')) == '/Game/Rogue10m/Maps/OpenRunMap')
        check('open_world_portal_starts_run', bool(portal.get_editor_property('start_run_timer_on_travel')))
    dirty_maps = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    dirty_content = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    check('no_unsaved_map_packages', not dirty_maps, dirty_maps)
    check('no_unsaved_content_packages', not dirty_content, dirty_content)
    report = {'status': 'passed' if all(c['passed'] for c in checks) else 'failed',
              'map': MAP, 'checks_passed': sum(c['passed'] for c in checks), 'checks_total': len(checks),
              'actor_count': len(current), 'profiles': profiles, 'textures': textures, 'checks': checks,
              'validation_scope': 'idle editor after map unload/reload; no runtime gameplay changes'}
    (EVIDENCE / 'reload-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'status': report['status'], 'checks_passed': report['checks_passed'],
                      'checks_total': len(checks), 'actors': len(current),
                      'failed': [c for c in checks if not c['passed']]}))
    if report['status'] != 'passed':
        raise RuntimeError('Village ground reload validation failed')


if __name__ == '__main__':
    main()
