"""Read-only audit of the actual Unarmed and progression-driven Knuckle animation contracts."""
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir()) / 'Feature/doc/evidence/fist-combat-20260929/baseline-assets.json'
PROFILES = ['/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed',
            '/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle',
            '/Game/DataAsset/SkillProfile/DA_SkillProfile_StoneFist']
FIELDS = ['skill_name','input_slot','attack_montage','animation_play_rate','hit_start_delay_seconds',
          'attack_cooldown','charge_seconds','hit_count','hit_interval','max_hits_per_target','damage',
          'attack_range','attack_shape','enable_combo','combo_window_open_seconds','combo_window_close_seconds',
          'next_combo_skill','prerequisite_skills','unlock_conditions','effect_attach_socket']


def serial(value):
    if value is None or isinstance(value,(str,int,float,bool)): return value
    if isinstance(value,unreal.Object): return value.get_path_name()
    if isinstance(value,(list,tuple,unreal.Array)): return [serial(x) for x in value]
    try: return {str(k):serial(v) for k,v in value.items()}
    except (AttributeError,TypeError): return str(value)


def main():
    profiles=[]
    candidates=set()
    for path in PROFILES:
        profile=unreal.load_asset(path)
        if not profile: continue
        row={'path':path}
        for name in ['weapon_type','uses_martial_arts_progression','default_skill_bindings','initially_unlocked_skills','skill_tree_skills']:
            try:
                value=profile.get_editor_property(name);row[name]=serial(value)
                values=list(value.values()) if name=='default_skill_bindings' else list(value) if name.endswith('skills') else []
                candidates.update(v.get_path_name() for v in values if isinstance(v,unreal.Rogue10mAttackSkillData))
            except Exception as exc: row[name]={'error':str(exc)}
        profiles.append(row)
    for folder in ['/Game/DataAsset/AttackSkill/BasicBrawler','/Game/DataAsset/AttackSkill/ClassCombat/Knuckle','/Game/DataAsset/AttackSkill/StoneFist']:
        candidates.update(unreal.EditorAssetLibrary.list_assets(folder,recursive=True))
    skills=[]
    for path in sorted(candidates):
        asset=unreal.load_asset(path)
        if not isinstance(asset,unreal.Rogue10mAttackSkillData): continue
        row={'path':asset.get_path_name()}
        for name in FIELDS: row[name]=serial(asset.get_editor_property(name))
        montage=asset.get_editor_property('attack_montage')
        if montage:
            row['montage_length']=montage.get_play_length()
            row['effective_duration']=montage.get_play_length()/asset.get_editor_property('animation_play_rate')
            for prop in ['blend_in','blend_out','slot_anim_tracks']:
                try:
                    value=montage.get_editor_property(prop)
                    row[prop]=value.get_editor_property('blend_time') if prop.startswith('blend_') else serial(value)
                except Exception as exc: row[prop]={'error':str(exc)}
        skills.append(row)
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps({'profiles':profiles,'skills':skills},ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('RESULT=FIST_COMBAT_BASELINE_AUDIT_PASSED '+str(OUT))


if __name__=='__main__':
    try: main()
    finally: unreal.SystemLibrary.quit_editor()
