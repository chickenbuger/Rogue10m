"""Compare read-only FBX SDK samples with the baked UE Boxing pose report.
Run after PreviewBoxingRetarget.py. Requires NumPy, never modifies assets.
"""
import argparse
import json
from pathlib import Path
import numpy as np


def measure(positions, bones, up, handedness):
    hip, left_arm, right_arm, left_hand, right_hand = bones
    left = positions[left_arm][0] - positions[right_arm][0]
    left -= up * np.dot(left, up)
    left /= np.linalg.norm(left)
    forward = np.cross(left, up) * handedness
    result = {'left_axis': left.tolist(), 'forward_axis': forward.tolist()}
    for side, hand, arm in [('left', left_hand, left_arm), ('right', right_hand, right_arm)]:
        values = (positions[hand] - positions[arm]) @ forward
        result[side] = {'initial_forward_cm': float(values[0]), 'peak_forward_cm': float(values.max()),
                        'excursion_cm': float(np.ptp(values)), 'peak_frame': int(values.argmax()), 'values_cm': values.tolist()}
    result['pelvis_forward_cm'] = ((positions[hip] - positions[hip][0]) @ forward).tolist()
    result['pelvis_vertical_cm'] = ((positions[hip] - positions[hip][0]) @ up).tolist()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('Feature/doc/evidence/boxing-reference-20260922/boxing-source-motion.json'))
    parser.add_argument('--target', type=Path, default=Path('Feature/doc/evidence/boxing-direct-head-20260922/retarget-poses.json'))
    args = parser.parse_args()
    source_data = json.loads(args.source.read_text(encoding='utf-8-sig'))
    target_data = json.loads(args.target.read_text(encoding='utf-8-sig'))
    assert source_data['stack'] == 'mixamo.com' and len(source_data['frames']) == 53
    assert target_data['frame_count'] == 53 and abs(target_data['duration'] - 52/30) < .001
    names = [bone['name'].split(':')[-1] for bone in source_data['bones']]
    source_array = np.array([frame['positions'] for frame in source_data['frames']])
    source_names = ['Hips', 'LeftArm', 'RightArm', 'LeftHand', 'RightHand']
    source_feet = {name: source_array[:, names.index(name)] for name in ['LeftFoot', 'RightFoot', 'Hips']}
    target_names = ['pelvis', 'upperarm_l', 'upperarm_r', 'hand_l', 'hand_r']
    source = measure({name: source_array[:, names.index(name)] for name in source_names}, source_names, np.array([0., 1., 0.]), 1)
    target = measure({name: np.array([row['bones'][name]['position'] for row in target_data['samples']]) for name in target_names}, target_names, np.array([0., 0., 1.]), -1)
    source_forward = np.array(source['forward_axis'])
    target_forward = np.array(target['forward_axis'])
    feet = {}
    for side, source_name, target_name in [('left', 'LeftFoot', 'foot_l'), ('right', 'RightFoot', 'foot_r')]:
        source_world = source_feet[source_name] @ source_forward
        target_positions = np.array([row['bones'][target_name]['position'] for row in target_data['samples']])
        target_pelvis = np.array([row['bones']['pelvis']['position'] for row in target_data['samples']])
        target_world = target_positions @ target_forward
        source_relative = (source_feet[source_name] - source_feet['Hips']) @ source_forward
        target_relative = (target_positions - target_pelvis) @ target_forward
        correlation = float(np.corrcoef(source_world - source_world[0], target_world - target_world[0])[0, 1])
        relative_correlation = float(np.corrcoef(source_relative, target_relative)[0, 1])
        assert correlation > .98 and relative_correlation > .95
        feet[side] = {'source_initial_pelvis_relative_forward_cm': float(source_relative[0]),
                      'target_initial_pelvis_relative_forward_cm': float(target_relative[0]),
                      'source_max_forward_travel_cm': float((source_world - source_world[0]).max()),
                      'target_max_forward_travel_cm': float((target_world - target_world[0]).max()),
                      'forward_correlation': correlation, 'pelvis_relative_forward_correlation': relative_correlation}
    assert feet['left']['target_initial_pelvis_relative_forward_cm'] > feet['right']['target_initial_pelvis_relative_forward_cm']
    assert feet['left']['target_max_forward_travel_cm'] > feet['right']['target_max_forward_travel_cm']
    checks = {}
    for side in ['left', 'right']:
        checks[side + '_forward_correlation'] = float(np.corrcoef(source[side]['values_cm'], target[side]['values_cm'])[0, 1])
        checks[side + '_peak_frame_difference'] = target[side]['peak_frame'] - source[side]['peak_frame']
        assert checks[side + '_forward_correlation'] > .95
        assert abs(checks[side + '_peak_frame_difference']) <= 1
    for axis in ['forward', 'vertical']:
        checks['pelvis_' + axis + '_correlation'] = float(np.corrcoef(source['pelvis_' + axis + '_cm'], target['pelvis_' + axis + '_cm'])[0, 1])
        assert checks['pelvis_' + axis + '_correlation'] > .99
    checks['left_dominance_ratio'] = target['left']['excursion_cm'] / target['right']['excursion_cm']
    assert checks['left_dominance_ratio'] > 5
    assert target['left']['peak_forward_cm'] > 35 and target['left']['initial_forward_cm'] > 0
    assert min(target['pelvis_vertical_cm']) < -5
    assert max(target['pelvis_forward_cm']) > 30
    report = {'source': source, 'target': target, 'checks': checks, 'feet': feet,
              'mapping': 'Initial shoulder-relative forward: Maya Y-up right-handed source, UE Z-up left-handed Manny. Matching anatomical lateral ordering needs opposite cross-product sign.',
              'result': 'BOXING_SOURCE_TARGET_COMPARISON_PASSED'}
    args.target.with_name('source-target-comparison.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(report['result'], json.dumps(checks))


if __name__ == '__main__':
    main()
