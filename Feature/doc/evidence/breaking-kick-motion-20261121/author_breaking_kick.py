"""Author a Manny low arc kick on the existing project-authored guard rig."""
from bisect import bisect_right
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector


FPS = 60
INTERVALS = 100
UP = Vector((0, 0, 1))
FORWARD = Vector((0, -1, 0))
RIGHT = Vector((-1, 0, 0))
# Frame, leading foot (forward/right/up cm), pelvis (forward/right/up cm), head-quiet body turn (degrees).
PHASES = (
    (0, (0, 0, 0), (0, 0, 0), 0),
    (4, (22, -8, 11), (1, -2, -3), -6),
    (8, (57, -14, 32), (3, -4, -4), -12),
    (11, (99, -11, 44), (10, -5, -2), -6),
    (13, (119, -1, 46), (17, -5, -1), 3),
    (16, (119, 15, 43), (17, -4, -1), 13),
    (20, (82, 26, 33), (9, -3, -1), 20),
    (25, (34, 22, 17), (3, -3, -2), 13),
    (31, (6, 7, 3), (1, -1, -2), 4),
    (35, (0, 0, 0), (0, 0, 0), 0),
    (100, (0, 0, 0), (0, 0, 0), 0),
)


def damped_sample(frame):
    """Use monotone Hermite slopes to pass through the hand-authored landmarks."""
    if frame == PHASES[-1][0]:
        return (*PHASES[-1][1], *PHASES[-1][2], PHASES[-1][3])
    positions = [key[0] for key in PHASES]
    values = [(*key[1], *key[2], key[3]) for key in PHASES]
    index = bisect_right(positions, frame) - 1
    duration = positions[index + 1] - positions[index]
    u = (frame - positions[index]) / duration
    result = []
    for channel in range(len(values[index])):
        samples = [row[channel] for row in values]
        slopes = [(b - a) / (j - i) for a, b, i, j in zip(samples, samples[1:], positions, positions[1:])]
        tangents = [0.0, *(2 * a * b / (a + b) if a * b > 0.0 else 0.0 for a, b in zip(slopes, slopes[1:])), 0.0]
        result.append((2*u**3-3*u*u+1) * samples[index] + (u**3-2*u*u+u) * duration * tangents[index]
                      + (-2*u**3+3*u*u) * samples[index + 1] + (u**3-u*u) * duration * tangents[index + 1])
    return result


def set_pose_rotation(bone, rotation):
    bone.matrix = Matrix.LocRotScale(bone.head, rotation @ bone.matrix.to_quaternion(), Vector((1, 1, 1)))
    bpy.context.view_layer.update()


def solve_leg(rig, side, goal, bend, foot_rotation):
    """Place a no-stretch foot target and preserve the authored bend plane and segment roll."""
    thigh, calf, foot = [rig.pose.bones[f"{name}_{side}"] for name in ("thigh", "calf", "foot")]
    hip, knee, ankle = thigh.head.copy(), calf.head.copy(), foot.head.copy()
    upper_length, lower_length = (knee - hip).length, (ankle - knee).length
    axis = (goal - hip).normalized()
    distance = min((goal - hip).length, upper_length + lower_length - .15)
    distance = max(abs(upper_length - lower_length) + .15, distance)
    x = (upper_length**2 - lower_length**2 + distance**2) / (2 * distance)
    height = math.sqrt(max(0, upper_length**2 - x*x))
    knee_direction = (bend - axis * bend.dot(axis)).normalized()
    target_knee = hip + axis*x + knee_direction*height
    target_ankle = hip + axis*distance
    set_pose_rotation(thigh, (knee - hip).normalized().rotation_difference((target_knee - hip).normalized()))
    set_pose_rotation(calf, (foot.head - calf.head).normalized().rotation_difference((target_ankle - calf.head).normalized()))
    foot.matrix = Matrix.LocRotScale(foot.head, foot_rotation, Vector((1, 1, 1)))
    bpy.context.view_layer.update()


def positions(rig):
    names = ("root", "pelvis", "head", "hand_r", "hand_l", "thigh_r", "calf_r", "thigh_l", "calf_l", "foot_r", "foot_l", "ball_r", "ball_l")
    return {name: list(rig.pose.bones[name].head) for name in names}


def main():
    baseline, output, helpers = [Path(arg).resolve() for arg in sys.argv[sys.argv.index("--") + 1:]]
    sys.path.insert(0, str(helpers))
    from AuthorWhirlwind import add_fixed_root, evaluated_matrices, import_rig, local_basis

    output.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    rig = import_rig(baseline)
    cm_to_scene = Matrix.Diagonal((.01, .01, .01, 1.0))
    scene_to_cm = cm_to_scene.inverted()
    original = [evaluated_matrices(rig, frame / INTERVALS, scene, scene_to_cm) for frame in range(INTERVALS + 1)]
    ready = original[0]
    rig.animation_data_clear()
    rig.name = "Armature"
    rig.matrix_world = cm_to_scene
    if "root" not in rig.pose.bones:
        add_fixed_root(rig)
    default = local_basis(rig, ready)
    toes = ready["ball_r"].translation - ready["foot_r"].translation
    toes.z = 0
    toes.normalize()
    toe_turn = math.atan2(UP.dot(toes.cross(FORWARD)), toes.dot(FORWARD))
    bends = {}
    for side in ("l", "r"):
        hip, knee, ankle = [ready[f"{name}_{side}"].translation for name in ("thigh", "calf", "foot")]
        axis = (ankle - hip).normalized()
        bends[side] = ((knee - hip) - axis * (knee - hip).dot(axis)).normalized()
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 1, INTERVALS + 1
    rig.animation_data_create()
    action = bpy.data.actions.new("Manny Breaking Kick - Low Right Arc")
    rig.animation_data.action = action
    action.use_fake_user = True
    previous_rotation = {}
    frames = []
    for index in range(INTERVALS + 1):
        scene.frame_set(index + 1)
        for bone in rig.pose.bones:
            bone.rotation_mode = "QUATERNION"
            bone.matrix_basis = default[bone.name]
        bpy.context.view_layer.update()
        fx, fy, fz, px, py, pz, yaw = damped_sample(index)
        if 0 < index < 35:
            pelvis = rig.pose.bones["pelvis"]
            pelvis.matrix = Matrix.LocRotScale(ready["pelvis"].translation + FORWARD*px + RIGHT*py + UP*pz,
                                               Quaternion(UP, math.radians(yaw)) @ ready["pelvis"].to_quaternion(), Vector((1, 1, 1)))
            bpy.context.view_layer.update()
            solve_leg(rig, "l", ready["foot_l"].translation, bends["l"], ready["foot_l"].to_quaternion())
            extension = min(1.0, fz / 44.0)
            kicking_orientation = Quaternion(RIGHT, math.radians(fz * .38)) @ Quaternion(UP, toe_turn * extension) @ ready["foot_r"].to_quaternion()
            bend = bends["r"].lerp((FORWARD + RIGHT * .24).normalized(), extension)
            solve_leg(rig, "r", ready["foot_r"].translation + FORWARD*fx + RIGHT*fy + UP*fz, bend, kicking_orientation)
            # Balance the kick with small guard-arm changes and a steady carried head.
            set_pose_rotation(rig.pose.bones["spine_03"], Quaternion(UP, math.radians(-yaw * .43)))
            set_pose_rotation(rig.pose.bones["upperarm_r"], Quaternion(UP, math.radians(-fz * .28)))
            set_pose_rotation(rig.pose.bones["upperarm_l"], Quaternion(UP, math.radians(fz * .15)))
            head = rig.pose.bones["head"]
            head.matrix = Matrix.LocRotScale(head.head, ready["head"].to_quaternion(), Vector((1, 1, 1)))
            bpy.context.view_layer.update()
        for helper, target in (("ik_foot_l", "foot_l"), ("ik_foot_r", "foot_r")):
            if helper in rig.pose.bones:
                rig.pose.bones[helper].matrix = rig.pose.bones[target].matrix
        bpy.context.view_layer.update()
        frames.append(positions(rig))
        for bone in rig.pose.bones:
            q = bone.rotation_quaternion
            if bone.name in previous_rotation and previous_rotation[bone.name].dot(q) < 0.0:
                q.negate()
            previous_rotation[bone.name] = q.copy()
            bone.keyframe_insert("location", frame=index + 1, group=bone.name)
            bone.keyframe_insert("rotation_quaternion", frame=index + 1, group=bone.name)
    for curve in action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = "LINEAR"
    comparison = {name: [list(row[name].translation) for row in original] for name in frames[0] if name in ready}
    metrics = {"basis_forward": list(FORWARD), "bone_count": len(rig.pose.bones), "frames": frames, "baseline": comparison}
    (output / "blender-poses.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    scene.frame_set(14)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / "MannyBreakingKick.blend"))
    bpy.ops.export_scene.fbx(filepath=str(output / "MannyBreakingKick.fbx"), use_selection=True,
                             object_types={"ARMATURE"}, apply_unit_scale=True, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                             bake_anim_simplify_factor=0.0, axis_forward="-Y", axis_up="Z")


if __name__ == "__main__":
    main()
