"""Reopen the comparison scenes and verify all non-head rotation curves."""

import hashlib
import json
from pathlib import Path

import bpy


OUT = Path(__file__).resolve().parent / 'skeleton-only'
ALLOWED = {f'pose.bones["{bone}"].rotation_quaternion' for bone in ('neck_01', 'head')}


def curves(action):
    strip = action.layers[0].strips[0]
    return {(curve.data_path, curve.array_index): curve for curve in strip.channelbag(action.slots[0]).fcurves}


def keys(curve):
    return [(tuple(key.co), tuple(key.handle_left), tuple(key.handle_right), key.interpolation,
             key.handle_left_type, key.handle_right_type) for key in curve.keyframe_points]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    evidence = {}
    for label in ('idle', 'walk', 'jog'):
        source_path = OUT / f'{label}.fbx'
        blend_path = OUT / f'{label}-head-comparison.blend'
        bpy.ops.wm.open_mainfile(filepath=str(blend_path))
        rig = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
        if rig.animation_data.action.name != 'moderate':
            raise RuntimeError(f'{label}: approved variant not loaded by default')
        original = curves(bpy.data.actions['Unreal export'])
        record = {'input_fbx_sha256': digest(source_path), 'comparison_blend_sha256': digest(blend_path),
                  'bone_count': len(rig.pose.bones), 'variants': {}}
        for name in ('existing', 'moderate', 'strong'):
            comparison = curves(bpy.data.actions[name])
            if comparison.keys() != original.keys():
                raise RuntimeError(f'{label} {name}: unexpected curve layout')
            changed = {curve for curve in original if keys(original[curve]) != keys(comparison[curve])}
            fields = {path for path, _ in changed}
            if fields != ALLOWED:
                raise RuntimeError(f'{label} {name}: unexpected curve changes {fields}')
            record['variants'][name] = {
                'changed_rotation_curves': len(changed),
                'unchanged_curves': len(original) - len(changed),
            }
        evidence[label] = record
    (OUT / 'fresh-reopen.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')


main()
