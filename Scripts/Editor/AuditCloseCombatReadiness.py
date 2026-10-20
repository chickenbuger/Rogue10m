"""Read-only comparison against current authored fist and original spin animations."""
import json
from pathlib import Path
import runpy
import unreal

PROJECT=Path(unreal.Paths.project_dir())
EVIDENCE=PROJECT/'Feature/doc/evidence/close-combat-preparation-20261002'


def main():
    api=runpy.run_path(str(PROJECT/'Scripts/Editor/PrepareCloseCombatPreview.py'),
                      run_name='close_combat_readonly_helpers')
    sample,require=api['sample'],api['require']
    mesh=require(api['MANNY'],unreal.SkeletalMesh)
    refs={
        'current_authored_hook':'/Game/Rogue10m/Animation/Combat/FistAuthored/A_Unarmed_Hook',
        'current_authored_uppercut':'/Game/Rogue10m/Animation/Combat/FistAuthored/A_Knuckle_Uppercut',
        'original_spinning_hook':'/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03',
        'boxing_preview':'/Game/Rogue10m/Animation/Preview/Boxing/A_Boxing_Manny',
        'martelo_preview':'/Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny',
        'dash_sequence':'/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash',
    }
    rows={label:sample(require(path,unreal.AnimSequence),mesh) for label,path in refs.items()}
    defaults=unreal.get_default_object(unreal.Rogue10mAppearanceCameraComponent)
    config={key:api['serial'](defaults.get_editor_property(key)) for key in
            ['stabilize_brawler_head_motion','brawler_head_rotation_weight',
             'brawler_head_max_rotation_degrees','initial_view_pitch','eye_offset']}
    offset=defaults.get_editor_property('eye_offset')
    config['eye_offset']=[offset.x,offset.y,offset.z]
    existing=json.loads((EVIDENCE/'editor-audit.json').read_text(encoding='utf-8'))
    review=[]
    for label,row in existing['clips'].items():
        target=row['manny']
        raw=target['summary']['head']['max_rotation_from_ref_deg']
        cue=target['contact_review_cue']
        review.append({'label':label,
            'max_raw_head_rotation_from_ref_deg':raw,
            'estimated_manny_clip_view_rotation_limit_deg':min(
                raw*config['brawler_head_rotation_weight'],config['brawler_head_max_rotation_degrees']),
            'contact_review_cue':cue,
            'note':'Static Manny clip + compiled native CDO estimate. Not runtime appearance pose or target hit evidence.'})
    dodge=require('/Game/Rogue10m/Animation/Common/AM_Dodge_Roll',unreal.AnimMontage)
    tracks=[]
    for track in dodge.get_editor_property('slot_anim_tracks'):
        segments=track.get_editor_property('anim_track').get_editor_property('anim_segments')
        tracks.append({'slot':str(track.get_editor_property('slot_name')),
                       'sequences':[api['serial'](s.get_editor_property('anim_reference')) for s in segments]})
    report={'native_camera_cdo':config,'references':rows,'clip_camera_estimates':review,
            'existing_dodge_montage':{'path':dodge.get_path_name(),'tracks':tracks},
            'limitations':['No gameplay DA or camera changes; no rendered preview, fixed mouse target or hit volume test.',
                           'Guard block-hit is not a verified parry; 4 evade labels require action classification.']}
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    (EVIDENCE/'readiness-comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    report['references']={key:{k:v for k,v in row.items() if k!='frames'} for key,row in rows.items()}
    (EVIDENCE/'readiness-summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('RESULT=CLOSE_COMBAT_READINESS_PASSED')


if __name__=='__main__':
    try:main()
    finally:unreal.SystemLibrary.quit_editor()
