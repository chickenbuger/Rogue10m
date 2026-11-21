"""Author a stationary, upper-body Manny aerial strike in Blender."""
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

DONOR, OUT = (Path(arg) for arg in sys.argv[sys.argv.index("--") + 1:])
OUT.mkdir(exist_ok=False)
FWD = Vector((0, -1, 0))
RIGHT = Vector((1, 0, 0))
UP = Vector((0, 0, 1))
FPS = 60
INTERVALS = 100
CHANGED = {
    "spine_01", "spine_02", "spine_03", "spine_04", "spine_05",
    "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r",
    "lowerarm_l", "lowerarm_r", "hand_l", "hand_r",
}


def ease(a, b, t):
    u = max(0.0, min(1.0, (t - a) / (b - a)))
    return u * u * (3.0 - 2.0 * u)


def rotate(bone, axis, degrees):
    q = bone.matrix.to_quaternion()
    change = Quaternion(axis, math.radians(degrees))
    bone.rotation_quaternion = (bone.rotation_quaternion @ q.inverted() @ change @ q).normalized()
    bpy.context.view_layer.update()


def guide(bone, child, direction, max_degrees):
    if max_degrees <= 0:
        return
    current = (child.head - bone.head).normalized()
    axis = current.cross(direction)
    if axis.length < 1e-5:
        return
    rotate(bone, axis.normalized(), min(max_degrees, math.degrees(current.angle(direction))))


def set_basis(rig, selection):
    for bone in rig.pose.bones:
        bone.matrix_basis = selection[bone.name]
    bpy.context.view_layer.update()


def restore_fbx_root(rig):
    bpy.context.view_layer.objects.active = rig
    if "root" in rig.data.bones:
        return
    bpy.ops.object.mode_set(mode="EDIT")
    bones = rig.data.edit_bones
    root = bones.new("root")
    root.head, root.tail = (0, 0, 0), (0, 10, 0)
    for bone in list(bones):
        if bone != root and bone.parent is None:
            bone.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")


def capture_pose(bones):
    return {name: bones[name].head.copy() for name in ("pelvis", "upperarm_r", "lowerarm_r", "hand_r", "hand_l")}


def form_strike(bones, time, side_amount):
    wind = ease(0.0, .050, time) * (1.0 - ease(.050, .155, time))
    hit = ease(.045, .17, time) * (1.0 - ease(.225, .52, time))
    extension = ease(.040, .165, time)
    if hit + wind <= 0:
        return
    for index, amount in enumerate((.10, .12, .19, .27, .32), start=1):
        spine = bones[f"spine_0{index}"]
        rotate(spine, RIGHT, (16.0 * hit - 3.0 * wind) * amount)
        rotate(spine, UP, (-8.0 * hit + 7.0 * wind) * amount)

    rotate(bones["clavicle_r"], RIGHT, 8.5 * wind + 3.5 * hit)
    upper_target = (FWD * .50 + RIGHT * side_amount + UP * .68).lerp(
        FWD * .74 + RIGHT * .12 - UP * .70, extension)
    fore_target = (FWD * .30 + RIGHT * .12 + UP * .72).lerp(
        FWD * .36 - UP * .92 - RIGHT * .08, extension)
    guide(bones["upperarm_r"], bones["lowerarm_r"], upper_target.normalized(), 43.0 * wind + 52.0 * hit)
    guide(bones["lowerarm_r"], bones["hand_r"], fore_target.normalized(), 38.0 * wind + 56.0 * hit)
    rotate(bones["hand_r"], RIGHT, -5.0 * wind + 13.0 * hit)
    guide(bones["upperarm_l"], bones["lowerarm_l"], FWD + UP * .16 + RIGHT * .37, 9.0 * hit)
    guide(bones["lowerarm_l"], bones["hand_l"], FWD + UP * .60 + RIGHT * .25, 10.0 * hit)


def sample_donor(path, phase):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type == "ARMATURE")
    action = rig.animation_data.action
    start, stop = action.frame_range
    frame = float(start + phase * (stop - start))
    scene.frame_set(math.floor(frame), subframe=frame % 1.0)
    return scene, rig, action


def encode(values):
    return {name: list(value) for name, value in values.items()}


def main():
    _, hook, _ = sample_donor(DONOR / "ground_hook.fbx", .35)
    hook_pose = capture_pose(hook.pose.bones)
    side_amount = min(.36, max(.13, abs(FWD.dot((hook_pose["lowerarm_r"] - hook_pose["upperarm_r"]).normalized())) * .42))
    scene, rig, _ = sample_donor(DONOR / "fall.fbx", .15)
    basis = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
    rig.animation_data_clear()
    restore_fbx_root(rig)
    rig.name = "Armature"
    basis.setdefault("root", Matrix.Identity(4))
    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"
    set_basis(rig, basis)
    original = capture_pose(rig.pose.bones)

    # Bake the actual local delta from the fall donor. Frame zero is the
    # additive reference; all pelvis, leg, root and IK channels stay identical.
    offsets = []
    predicted = []
    for frame in range(INTERVALS + 1):
        time = frame / FPS
        set_basis(rig, basis)
        form_strike(rig.pose.bones, time, side_amount)
        predicted.append({"time": time, **encode(capture_pose(rig.pose.bones))})
        offsets.append({
            name: (rig.pose.bones[name].rotation_quaternion @ basis[name].to_quaternion().inverted()).normalized()
            for name in CHANGED
        })

    rig.animation_data_create()
    action = bpy.data.actions.new("Manny Mountain Slam - Upper Body")
    action.use_fake_user = True
    rig.animation_data.action = action
    scene.frame_start, scene.frame_end, scene.render.fps = 1, INTERVALS + 1, FPS
    for index, delta in enumerate(offsets):
        frame = index + 1
        for bone in rig.pose.bones:
            bone.matrix_basis = Matrix.Identity(4)
            if bone.name in delta:
                rotation = delta[bone.name]
                if index and offsets[index - 1][bone.name].dot(rotation) < 0:
                    rotation.negate()
                bone.rotation_quaternion = rotation
            bone.keyframe_insert(data_path="rotation_quaternion", frame=frame, group=bone.name)
            bone.keyframe_insert(data_path="location", frame=frame, group=bone.name)

    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / "MannyMountainSlam.fbx"), use_selection=True,
                             object_types={"ARMATURE"}, add_leaf_bones=False, armature_nodetype="NULL",
                             bake_anim=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True,
                             axis_forward="-Y", axis_up="Z")
    scene.frame_set(11)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "MannyMountainSlam.blend"))
    (OUT / "blender-motion.json").write_text(json.dumps(
        {"fps": FPS, "intervals": INTERVALS, "changed_bones": sorted(CHANGED),
         "bone_count": len(rig.pose.bones), "hook_pose": encode(hook_pose),
         "fall_reference": encode(original), "modeled_air_poses": predicted}, indent=2) + "\n", encoding="utf-8")
    a = [sample["hand_r"][2] for sample in predicted]
    print(f"AERIAL_SLAM_AUTHORED source bone count={len(rig.pose.bones)} right wrist Z={min(a):.3f}..{max(a):.3f}")


if __name__ == "__main__":
    main()
