"""Read-only explanation of root normalization and the contact comparison space."""
import json
import math
from pathlib import Path
import runpy

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT/'Feature/doc/evidence/mouse-head-stable-locomotion-20261013'
API = runpy.run_path(str(ROOT/'Scripts/Editor/PrepareFighterCombatStance.py'), run_name='contact_space_readonly_helpers')


def main():
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', API['require']('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'))
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('extract_root_motion', False)
    rows = []
    for path, contact in [('/Game/Rogue10m/Animation/Combat/FistAuthored/A_Knuckle_Uppercut', .21),
                          ('/Game/Rogue10m/Animation/Combat/FistAuthored/A_Knuckle_ShortBlow', .22)]:
        source = API['require'](path, unreal.AnimSequence)
        samples = []
        for time in [0., contact, source.get_play_length()]:
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, time, options)
            root = API['bone'](pose, 'root')
            hand = API['bone'](pose, 'hand_r')
            relative = API['transform'](unreal.MathLibrary.make_relative_transform(API['native'](hand), API['native'](root)))
            normalized_hand = [hand['p'][i]-root['p'][i] for i in range(3)]
            structural_bones = {name: {'component': API['bone'](pose, name),
                'local': API['bone'](pose, name, unreal.AnimPoseSpaces.LOCAL)}
                for name in ['root', 'pelvis', 'thigh_l', 'thigh_r', 'foot_l', 'foot_r']}
            samples.append({'time': time, 'root_component': root, 'hand_component': hand,
                            'hand_root_relative': relative, 'root_translation_normalized_hand_component_cm': normalized_hand,
                            'root_translation_normalization_displacement_cm': math.dist(root['p'], [0., 0., 0.]),
                            'structural_bones': structural_bones})
        rows.append({'source': source.get_path_name(), 'root_motion': source.get_editor_property('enable_root_motion'),
                     'force_root_lock': source.get_editor_property('force_root_lock'),
                     'root_motion_root_lock': str(source.get_editor_property('root_motion_root_lock')),
                     'samples': samples})
    report = {'read_only': True, 'sources': rows,
              'space_contract': 'The source has a constant root origin contributing to full-body height. New in-place derivatives preserve the first evaluated local root translation and suppress only subsequent travel. Root rotation/scale and every rendered non-root local transform remain source-derived. Contacts compare full hand T/R/S relative to root and component-space height is also retained.'}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE/'safe-contact-spaces.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('RESULT=SAFE_HEAD_CONTACT_SPACES_PASSED')
    return report


if __name__ == '__main__':
    main()
