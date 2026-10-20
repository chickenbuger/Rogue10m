"""Author six opt-in Idle/move/basic-attack sets in the idle OpenRunMap.

Only owned new DataAssets, existing six encounter BPs and the placed boss
are saved. Root serializes invocation after the native editor build.
"""
import json
from pathlib import Path
import unreal

ROOT = Path('D:/Project/Rogue10m')
EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-monsters-20261005'
OWN = '/Game/Rogue10m/World/OpenWorld/Monsters'
MAP = '/Game/Rogue10m/Maps/OpenRunMap'


def require(path):
    value = unreal.load_asset(path)
    if not value:
        raise RuntimeError('Missing asset ' + path)
    return value


def data_asset(path, klass):
    value = unreal.load_asset(path)
    if value:
        if not isinstance(value, klass):
            raise RuntimeError('Unexpected type ' + path)
        return value
    folder, name = path.rsplit('/', 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', klass)
    value = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, klass, factory)
    if not value:
        raise RuntimeError('Could not create ' + path)
    return value


def preview(actor, data, idle):
    mesh = actor.get_editor_property('mesh')
    mesh.modify()
    mesh.set_skeletal_mesh_asset(data.get_editor_property('skeletal_mesh'))
    mesh.set_relative_location(data.get_editor_property('mesh_relative_location'), False, False)
    mesh.set_relative_rotation(data.get_editor_property('mesh_relative_rotation'), False, False)
    mesh.set_relative_scale3d(data.get_editor_property('mesh_relative_scale'))
    mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    animation_data = mesh.get_editor_property('animation_data')
    animation_data.set_editor_property('anim_to_play', idle)
    animation_data.set_editor_property('saved_looping', True)
    animation_data.set_editor_property('saved_playing', True)
    mesh.set_editor_property('animation_data', animation_data)


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Requires idle saved OpenRunMap')
    inputs = json.loads((EVIDENCE / 'inputs.json').read_text(encoding='utf-8'))
    report = {'map': MAP, 'species': {}, 'saved_assets': [], 'actor_packages': []}
    for monster_id, row in inputs['species'].items():
        source = require(row['canonical_source']['asset_path'])
        path = OWN + '/DA_OW_' + monster_id
        data = unreal.load_asset(path)
        if not data:
            data = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name().split('.')[0], path)
        if not isinstance(data, unreal.Rogue10mMonsterDataAsset):
            raise RuntimeError('Invalid duplicate ' + path)
        data.modify()
        profile_path = OWN + '/DA_OW_BasicAttack_' + monster_id
        profile = data_asset(profile_path, unreal.Rogue10mMonsterAttackPatternProfile)
        animations = row['available_animations']

        def clip(token):
            matches = [a for a in animations if token.lower() in a['path'].lower()]
            if len(matches) != 1 or not matches[0]['matches_mesh_skeleton']:
                raise RuntimeError('Ambiguous/incompatible ' + monster_id + ':' + token)
            return require(matches[0]['path'])

        idle, movement, attack, hit, death = [clip(token) for token in ('IdleNormal', 'walk', 'Attack01', 'GetHit', 'Die')]
        duration = float(attack.get_play_length())
        attack_range = float(data.get_editor_property('attack_range'))
        windup = duration * .42
        recovery = duration - windup
        interval = float(data.get_editor_property('attack_interval'))
        properties = {'attack_range': attack_range, 'attack_arc_degrees': 100.0,
                      'attack_half_height': 160.0, 'attack_windup_seconds': windup,
                      'attack_recovery_seconds': recovery,
                      'attack_cooldown_seconds': max(.2, interval - duration),
                      'attack_damage_multiplier': 1.0, 'idle_animation': idle,
                      'movement_animation': movement, 'attack_animation': attack,
                      'hit_animation': hit, 'death_animation': death}
        profile.modify()
        for key, value in properties.items():
            profile.set_editor_property(key, value)
        data.set_editor_property('attack_pattern_profile', profile)
        data.set_editor_property('animation_blueprint_class', None)
        bp = require(row['blueprint'].split('.')[0])
        bp.modify()
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.modify()
        cdo.set_editor_property('monster_data', data)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        preview(cdo, data, idle)
        for value in (profile, data, bp):
            if not unreal.EditorAssetLibrary.save_loaded_asset(value, False):
                raise RuntimeError('Save failed ' + value.get_path_name())
            report['saved_assets'].append(value.get_path_name().split('.')[0])
        report['species'][monster_id] = {'data': path, 'profile': profile_path,
            'blueprint': bp.get_path_name().split('.')[0], 'mesh': data.get_editor_property('skeletal_mesh').get_path_name(),
            'idle': idle.get_path_name(), 'movement': movement.get_path_name(),
            'attack': attack.get_path_name(), 'attack_range_cm': attack_range,
            'attack_duration_seconds': duration, 'windup_seconds': windup,
            'hp': data.get_editor_property('max_health'), 'damage': data.get_editor_property('attack_damage'),
            'experience': data.get_editor_property('experience_reward')}
    bosses = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if a.get_actor_label() == 'OW_NorthernBoss']
    if len(bosses) != 1:
        raise RuntimeError('Expected one authored boss')
    boss = bosses[0]
    boss.modify()
    boss_data = require(report['species']['AbyssOverlordAzathor']['data'])
    boss.set_editor_property('monster_data', boss_data)
    preview(boss, boss_data, require(report['species']['AbyssOverlordAzathor']['idle']))
    report['actor_packages'].append(boss.get_package().get_name())
    if not levels.save_current_level():
        raise RuntimeError('OpenRunMap save failed')
    (EVIDENCE / 'application.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('[BasicMonsters] Saved six owned profiles and six model/animation sets')


if __name__ == '__main__':
    main()
