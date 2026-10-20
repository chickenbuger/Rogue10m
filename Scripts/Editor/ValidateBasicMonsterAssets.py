"""Read-only reload checks for authored six monsters and StartHub guide."""
import json
from collections import Counter
from pathlib import Path
import sys
import unreal

ROOT = Path('D:/Project/Rogue10m')
sys.path.insert(0, str(ROOT / 'Scripts/Editor'))
from InspectVillageGround import actor_state
from ApplyOpenWorldBasicMonsters import require

EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-monsters-20261005'


def main():
    checks = []

    def check(name, passed, details=None):
        checks.append({'name': name, 'passed': bool(passed), 'details': details})

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    check('idle_editor', not levels.is_in_play_in_editor())
    application = json.loads((EVIDENCE / 'application.json').read_text(encoding='utf-8'))
    inputs = json.loads((EVIDENCE / 'inputs.json').read_text(encoding='utf-8'))
    for species, row in application['species'].items():
        data = require(row['data'])
        profile = require(row['profile'])
        blueprint = require(row['blueprint'])
        cdo = unreal.get_default_object(blueprint.generated_class())
        source = require(inputs['species'][species]['canonical_source']['asset_path'])
        check(species + '_owned_data_profile', data.get_editor_property('attack_pattern_profile') == profile)
        check(species + '_blueprint_uses_owned_data', cdo.get_editor_property('monster_data') == data)
        for field in ('monster_id', 'max_health', 'attack_damage', 'experience_reward', 'walk_speed', 'attack_range'):
            check(species + '_preserved_' + field, data.get_editor_property(field) == source.get_editor_property(field))
        mesh = data.get_editor_property('skeletal_mesh')
        skeleton = mesh.get_editor_property('skeleton')
        for field in ('idle_animation', 'movement_animation', 'attack_animation', 'hit_animation', 'death_animation'):
            animation = profile.get_editor_property(field)
            check(species + '_' + field, isinstance(animation, unreal.AnimSequence)
                  and animation.get_editor_property('skeleton') == skeleton and animation.get_play_length() > .1,
                  animation.get_path_name() if animation else None)
        check(species + '_cdo_preview_model', cdo.get_editor_property('mesh').get_skeletal_mesh_asset() == mesh)
        check(species + '_basic_attack_matches_legacy_range', profile.get_editor_property('attack_range') == data.get_editor_property('attack_range'))
    if world.get_path_name().split('.')[0] == '/Game/Rogue10m/Maps/StartHub':
        actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
        guides = [a for a in actors if isinstance(a, unreal.Rogue10mTutorialNPC)]
        check('one_saved_start_hub_guide', len(guides) == 1)
        if len(guides) == 1:
            guide = guides[0]
            data = guide.get_editor_property('tutorial_data')
            mesh = guide.get_editor_property('guide_mesh')
            check('guide_saved_owned_data', data.get_path_name().startswith('/Game/Rogue10m/World/StartVillage/Tutorial/'))
            check('guide_owned_model_idle_compatible', mesh.get_skeletal_mesh_asset().get_editor_property('skeleton')
                  == data.get_editor_property('guide_idle_animation').get_editor_property('skeleton'))
            check('guide_no_blocking_collision', mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION)
            check('guide_two_real_health_potions', data.get_editor_property('starting_item_quantity') == 2
                  and data.get_editor_property('starting_item').get_path_name().startswith('/Game/DataAsset/Item/Starter/DA_Item_HealthPotion.'))
            check('guide_always_loaded', not guide.get_editor_property('is_spatially_loaded'))
            baseline = json.loads((EVIDENCE / 'npc-before-state.json').read_text(encoding='utf-8'))['actors']
            current = [actor_state(a) for a in actors if a != guide]
            before = Counter(json.dumps(row, sort_keys=True) for row in baseline)
            after = Counter(json.dumps(row, sort_keys=True) for row in current)
            check('all_original_village_actors_preserved', before == after,
                  {'before': len(baseline), 'after_excluding_guide': len(current)})
    else:
        check('start_hub_loaded_for_reload_validation', False, world.get_path_name())
    check('no_dirty_maps', not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    # AI Blackboard may refresh its cached keys while loading BehaviorTree; do not save unrelated packages here.
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    check('no_unsaved_owned_assets', not any('/Rogue10m/World/' in p for p in dirty), dirty)
    report = {'status': 'passed' if all(c['passed'] for c in checks) else 'failed',
              'checks_passed': sum(c['passed'] for c in checks), 'checks_total': len(checks), 'checks': checks}
    (EVIDENCE / 'reload-validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'status': report['status'], 'checks_passed': report['checks_passed'],
                      'checks_total': len(checks), 'failed': [c for c in checks if not c['passed']]}))
    if report['status'] != 'passed':
        raise RuntimeError('Saved monster/NPC reload validation failed')


if __name__ == '__main__':
    main()
