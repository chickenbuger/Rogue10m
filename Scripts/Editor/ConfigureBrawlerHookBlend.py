"""Create a dedicated hook montage with only blend-in 0.25 -> 0.10 seconds.
Default preview; apply with -Rogue10mApplyHookBlend. Shared montage/clip stay unchanged.
"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal

PROJECT = Path(unreal.Paths.project_dir())
SOURCE = '/Game/Rogue10m/Animation/Common/AM_Punch_03'
CLIP = '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03'
TARGET = '/Game/Rogue10m/Animation/Combat/Boxing/AM_BrawlerRightHook_Stable'
SKILL = '/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook'
OUTPUT = PROJECT / 'Feature/doc/evidence/animation-stability-20260928/hook-blend.json'
TAG = 'Rogue10m.StabilityHookSource'
MANDATORY = ['animation_play_rate', 'hit_start_delay_seconds', 'attack_cooldown', 'damage', 'attack_range',
             'enable_combo', 'combo_window_open_seconds', 'combo_window_close_seconds', 'next_combo_skill',
             'hit_count', 'hit_interval', 'max_hits_per_target', 'charge_seconds', 'input_slot', 'resource_costs']
MONTAGE_FIELDS = ['slot_anim_tracks', 'blend_out', 'blend_out_trigger_time', 'enable_auto_blend_out',
                  'rate_scale', 'skeleton', 'sync_group']


def require(path):
    asset = unreal.load_asset(path)
    if not asset: raise RuntimeError('Missing asset: ' + path)
    return asset


def file(path):
    return PROJECT / ('Content/' + path.removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(file(path).read_bytes()).hexdigest()


def serialize(value):
    if value is None or isinstance(value,(str,bool,int,float)): return value
    if isinstance(value,unreal.Object): return value.get_path_name()
    if hasattr(value,'export_text'): return value.export_text()
    if hasattr(value,'items'): return sorted([[serialize(k),serialize(v)] for k,v in value.items()], key=lambda item: str(item[0]))
    try: return [serialize(item) for item in value]
    except TypeError: return str(value)


def snapshot(asset):
    out = {}
    for name in sorted(set(dir(asset)) | set(MANDATORY)):
        if name.startswith('_') or name == 'attack_montage': continue
        try: out[name] = serialize(asset.get_editor_property(name))
        except Exception:
            if name in MANDATORY: raise
    if not all(name in out for name in MANDATORY): raise RuntimeError('Required gameplay properties unavailable')
    return out


def backup(path):
    src = file(path)
    dst = PROJECT / 'tmp/animation-stability/hook-blend-before' / src.name
    dst.parent.mkdir(parents=True,exist_ok=True)
    digest = sha(path)
    if dst.exists():
        if hashlib.sha256(dst.read_bytes()).hexdigest() != digest: raise RuntimeError('Existing backup differs: ' + str(dst))
    else: shutil.copy2(src,dst)
    return {'path':str(dst),'sha256':digest}


def main(apply=False):
    source, skill = require(SOURCE), require(SKILL)
    original = skill.get_editor_property('attack_montage')
    if original not in (source,unreal.load_asset(TARGET)):
        raise RuntimeError('Hook currently references an unexpected montage')
    before = snapshot(skill)
    hashes = {path:sha(path) for path in [SOURCE,CLIP]}
    source_blend = source.get_editor_property('blend_in')
    if abs(source_blend.get_editor_property('blend_time')-.25)>1e-5:
        raise RuntimeError('Unexpected shared source blend-in')
    original_fields = {name:serialize(source.get_editor_property(name)) for name in MONTAGE_FIELDS}
    report = {'apply':apply,'source':SOURCE,'target':TARGET,'skill':SKILL,'source_hashes_before':hashes,
              'skill_montage_before':original.get_path_name(),'gameplay_before':before,
              'source_montage_fields':original_fields,'source_blend_in':source_blend.export_text(),
              'target_blend_seconds':.10,'saved':False}
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    if not apply:
        OUTPUT.with_name('hook-blend-preview.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
        unreal.log('HOOK_BLEND_PREVIEW_PASS')
        return
    report['backups'] = {path:backup(path) for path in [SOURCE,CLIP,SKILL]}
    target = unreal.load_asset(TARGET)
    if target:
        if unreal.EditorAssetLibrary.get_metadata_tag(target,TAG) != SOURCE:
            raise RuntimeError('Existing target was not created by this tool')
    else:
        target = unreal.EditorAssetLibrary.duplicate_asset(SOURCE,TARGET)
        if not target: raise RuntimeError('Montage duplication failed')
    try:
        blend = target.get_editor_property('blend_in')
        # Preserve blend option/custom curve. Only time changes.
        blend.set_editor_property('blend_time',.10)
        target.set_editor_property('blend_in',blend)
        if abs(target.get_play_length()-source.get_play_length())>1e-6: raise RuntimeError('Duration changed')
        for name, value in original_fields.items():
            if serialize(target.get_editor_property(name)) != value: raise RuntimeError('Montage field changed: '+name)
        for name in ['blend_option','custom_curve']:
            if target.get_editor_property('blend_in').get_editor_property(name) != source_blend.get_editor_property(name):
                raise RuntimeError('Blend shape changed: '+name)
        unreal.EditorAssetLibrary.set_metadata_tag(target,TAG,SOURCE)
        unreal.EditorAssetLibrary.set_metadata_tag(target,'Rogue10m.StabilityHookChange','BlendInOnly=.10; same original clip, speed, hit timing, charge, damage and blend-out')
        if not unreal.EditorAssetLibrary.save_loaded_asset(target,only_if_is_dirty=False): raise RuntimeError('Dedicated montage save failed')
        skill.set_editor_property('attack_montage',target)
        if snapshot(skill) != before: raise RuntimeError('Gameplay fields changed')
        if {path:sha(path) for path in hashes} != hashes: raise RuntimeError('Shared source changed')
        if not unreal.EditorAssetLibrary.save_loaded_asset(skill,only_if_is_dirty=False): raise RuntimeError('Hook skill save failed')
        report['saved'] = True
        report['gameplay_after'] = snapshot(skill)
        report['gameplay_unchanged'] = report['gameplay_after'] == before
        report['skill_montage_after'] = skill.get_editor_property('attack_montage').get_path_name()
        report['target_blend_in'] = target.get_editor_property('blend_in').export_text()
        report['target_duration'] = target.get_play_length()
        report['source_hashes_after'] = {path:sha(path) for path in hashes}
        report['target_sha256'] = sha(TARGET)
        report['skill_sha256'] = sha(SKILL)
        if report['source_hashes_after'] != hashes: raise RuntimeError('Shared source changed after save')
    except Exception as error:
        report['error'] = str(error)
        if not report['saved']: skill.set_editor_property('attack_montage',original)
        OUTPUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
        raise
    OUTPUT.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    unreal.log('HOOK_BLEND_APPLY_PASS')


if __name__ == '__main__':
    try: main('-Rogue10mApplyHookBlend' in unreal.SystemLibrary.get_command_line())
    finally: unreal.SystemLibrary.quit_editor()
