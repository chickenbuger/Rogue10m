"""Verify the actual Maya-compatible FBX skeleton in a fresh Blender process."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector


folder, helper_path = [Path(arg).resolve() for arg in sys.argv[sys.argv.index('--') + 1:]]
sys.path.insert(0, str(helper_path))
from AuthorWhirlwind import evaluated_matrices, import_rig

bpy.ops.wm.read_factory_settings(use_empty=True)
source = folder / 'MannyShoulderBash.fbx'
rig = import_rig(source)
scene = bpy.context.scene
reference = json.loads((folder / 'shoulder-blender.json').read_text(encoding='utf-8'))
intervals = reference['intervals']
matrix = Matrix.Diagonal((100, 100, 100, 1))
rows = [evaluated_matrices(rig, index / intervals, scene, matrix) for index in range(intervals + 1)]
ready = rows[0]
error = max(math.dist(expected, rows[int(index)][name].translation)
            for index, pose in reference['pose_landmarks'].items() for name, expected in pose.items())
limbs = [(f'{a}_{side}', f'{b}_{side}') for side in ('l', 'r')
         for a, b in (('thigh','calf'), ('calf','foot'), ('upperarm','lowerarm'), ('lowerarm','hand'))]
root = max(math.dist(row['root'].translation, ready['root'].translation) for row in rows)
support = max(math.dist(row['ball_l'].translation, ready['ball_l'].translation) for row in rows)
limb_error = max(abs(math.dist(row[a].translation, row[b].translation) - math.dist(ready[a].translation, ready[b].translation))
                 for a,b in limbs for row in rows)
contact = rows[13]
lead = min((contact['upperarm_r'].translation - contact[hand].translation).dot(Vector((0,-1,0)))
           for hand in ('hand_l','hand_r'))
report = {'fbx_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'bone_count': len(rig.pose.bones),
          'intervals': intervals, 'landmark_error_cm': error, 'root_travel_cm': root,
          'support_toe_travel_cm': support, 'limb_length_error_cm': limb_error, 'shoulder_lead_cm': lead}
if len(rig.pose.bones) != 89 or max(error, root, support, limb_error) > .08 or lead <= 2:
    raise RuntimeError(report)
(folder / 'fresh-reimport.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('RESULT=SHOULDER_FBX_FRESH_IMPORT_PASSED', report)
