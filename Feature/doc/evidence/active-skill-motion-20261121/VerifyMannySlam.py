"""Freshly reopen the editable Blender scene and separately reimport the FBX."""
import bpy
import hashlib
import json
import math
from pathlib import Path
import sys

stage = Path(sys.argv[sys.argv.index("--") + 1])
SAMPLES = (0, 4, 8, 10, 15, 25, 36, 100)
PROTECTED = ("root", "pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r")


def read(path):
    if path.suffix == ".fbx":
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    else:
        bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
    scene = bpy.context.scene
    rig = next(o for o in scene.objects if o.type == "ARMATURE")
    action = rig.animation_data.action
    start, end = tuple(action.frame_range)
    if end - start != 100 or scene.render.fps != 60:
        raise RuntimeError(f"Expected 100 intervals at 60Hz: {action.frame_range}, {scene.render.fps}")
    snapshots = {}
    for key in SAMPLES:
        scene.frame_set(int(start + key))
        snapshots[key] = {bone.name: (tuple(bone.head), tuple(bone.matrix.to_quaternion())) for bone in rig.pose.bones}
    return snapshots


def rotation_delta(a, b):
    norm = math.sqrt(sum(x*x for x in a) * sum(y*y for y in b))
    return math.degrees(2 * math.acos(max(0.0, min(1.0, abs(sum(x*y for x, y in zip(a, b))) / norm))))


blend_path, fbx_path = stage / "MannyMountainSlam.blend", stage / "MannyMountainSlam.fbx"
author, export = read(blend_path), read(fbx_path)
assert set(author[0]) == set(export[0])
frozen = max(math.dist(export[key][bone][0], export[0][bone][0]) for key in SAMPLES for bone in PROTECTED)
frozen_angle = max(rotation_delta(export[key][bone][1], export[0][bone][1]) for key in SAMPLES for bone in PROTECTED)
same = max(math.dist(author[key][bone][0], export[key][bone][0]) for key in SAMPLES for bone in author[0])
arm_angle = max(rotation_delta(export[key]["upperarm_r"][1], export[0]["upperarm_r"][1]) for key in SAMPLES)
if frozen > .01 or frozen_angle > .03 or same > .05 or arm_angle < 20:
    raise RuntimeError(f"Unexpected export diff: {frozen}, {frozen_angle}, {same}, {arm_angle}")
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "fresh-blender.json").write_text(json.dumps({
    "bones": len(author[0]), "fps": 60, "intervals": 100, "sample_indices": SAMPLES,
    "max_protected_bone_drift_cm": frozen, "max_protected_bone_rotation_deg": frozen_angle,
    "max_joint_position_export_error_cm": same, "lead_upperarm_excursion_deg": arm_angle,
    "blend_sha256": digest(blend_path), "fbx_sha256": digest(fbx_path)
}, indent=2) + "\n")
print("RESULT=AERIAL_SLAM_FRESH_BLENDER_PASSED")
