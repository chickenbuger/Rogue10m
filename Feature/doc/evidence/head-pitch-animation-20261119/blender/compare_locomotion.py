"""Compare bounded head and neck motion on Unreal's exported player clips."""

import json
import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Quaternion, Vector


ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'skeleton-only' if os.getenv('ROGUE_HEAD_EXPORT_SKELETON_ONLY') == '1' else ROOT
SOURCE = json.loads((OUT / 'unreal-source.json').read_text(encoding='utf-8'))
BONES = ('neck_01', 'head')
PROTECTED = ('root', 'pelvis', 'hand_l', 'hand_r', 'foot_l', 'foot_r')
PRESETS = {
    'existing': (0.15, {'idle': 2.0, 'walk': 2.0, 'jog': 4.0}),
    'moderate': (0.35, {'idle': 5.0, 'walk': 5.0, 'jog': 7.0}),
    'strong': (0.50, {'idle': 6.0, 'walk': 6.0, 'jog': 8.0}),
}
IDENTITY = Quaternion()


def angle(a, b=IDENTITY):
    difference = a.normalized().rotation_difference(b.normalized())
    return math.degrees(2 * math.atan2(math.hypot(difference.x, difference.y, difference.z), abs(difference.w)))


def bounded(raw, weight, limit):
    distance = angle(raw)
    alpha = min(weight, limit / distance) if distance > 1.0e-5 else 0.0
    return IDENTITY.slerp(raw, alpha).normalized()


def source_samples(scene, rig, frames):
    samples = []
    for frame in frames:
        scene.frame_set(frame)
        pose = {name: rig.pose.bones[name].matrix.copy() for name in BONES + PROTECTED if name != 'root'}
        pose['root'] = rig.pose.bones['root'].matrix.copy() if 'root' in rig.pose.bones else rig.matrix_world.copy()
        samples.append(pose)
    return samples


def filtered_residuals(source, rest, follow, weight, limit):
    targets = []
    for sample in source:
        raw = {name: sample[name].to_quaternion() @ rest[name].inverted() for name in BONES}
        targets.append({name: bounded(raw[name], weight, limit) for name in BONES})
    current = {name: IDENTITY.copy() for name in BONES}
    for repeat in range(3):
        result = []
        for frame in targets:
            for name in BONES:
                current[name] = bounded(current[name].slerp(frame[name], follow), 1.0, limit)
            result.append({name: current[name].copy() for name in BONES})
    return result


def use_action(rig, action):
    rig.animation_data.action = action
    slots = [slot for slot in action.slots if slot.target_id_type == 'OBJECT']
    if len(slots) != 1:
        raise RuntimeError(f'Expected one animated object slot in {action.name}: {len(slots)}')
    rig.animation_data.action_slot = slots[0]


def set_rotation_in_armature_space(pose_bone, orientation):
    basis = pose_bone.matrix_basis.to_quaternion()
    current = pose_bone.matrix.to_quaternion()
    result = (basis @ current.inverted() @ orientation).normalized()
    mode = pose_bone.rotation_mode
    if mode == 'QUATERNION':
        if result.dot(pose_bone.rotation_quaternion) < 0.0:
            result.negate()
        pose_bone.rotation_quaternion = result
        return 'rotation_quaternion'
    if mode == 'AXIS_ANGLE':
        raise RuntimeError('FBX imported an unexpected axis-angle rotation mode')
    pose_bone.rotation_euler = result.to_euler(mode, pose_bone.rotation_euler)
    return 'rotation_euler'


def bake(scene, rig, action, frames, rest, residuals, name):
    variant = action.copy()
    variant.name = name
    variant.use_fake_user = True
    use_action(rig, variant)
    for frame, residual in zip(frames, residuals, strict=True):
        scene.frame_set(frame)
        for bone_name in BONES:
            bone = rig.pose.bones[bone_name]
            path = set_rotation_in_armature_space(bone, residual[bone_name] @ rest[bone_name])
            bone.keyframe_insert(data_path=path, frame=frame, group=bone_name)
            bpy.context.view_layer.update()
    return variant


def analyze(raw_samples, poses, rest, fps, forward, unit_scale):
    result = {}
    for name in BONES:
        qs = [pose[name].to_quaternion() for pose in poses]
        from_rest = [angle(q, rest[name]) for q in qs]
        bone_forward = rest[name].inverted() @ forward
        pitches = []
        for q in qs:
            direction = q @ bone_forward
            pitches.append(math.degrees(math.atan2(direction.z, math.hypot(direction.x, direction.y))))
        # Max chord says how much motion survives even when all poses point off neutral.
        result[name] = {
            'peak_from_reference_deg': max(from_rest),
            'animated_rotation_span_deg': max(angle(a, b) for a in qs for b in qs),
            'pitch_min_deg': min(pitches),
            'pitch_max_deg': max(pitches),
            'peak_frame_speed_deg_per_s': max(
                angle(a, b) * fps for a, b in zip(qs, qs[1:])
            ),
            'loop_seam_deg': angle(qs[0], qs[-1]),
        }
    result['protected'] = {
        'max_position_change_cm': max(
            (raw[name].translation - pose[name].translation).length * unit_scale * 100
            for raw, pose in zip(raw_samples, poses, strict=True)
            for name in PROTECTED
        ),
        'max_orientation_change_deg': max(
            angle(raw[name].to_quaternion(), pose[name].to_quaternion())
            for raw, pose in zip(raw_samples, poses, strict=True)
            for name in PROTECTED
        ),
    }
    return result


def setup_camera(scene, rig, frame, forward):
    scene.frame_set(frame)
    head = rig.matrix_world @ rig.pose.bones['head'].head
    foot = rig.matrix_world @ rig.pose.bones['foot_l'].head
    height = abs(head.z - foot.z)
    target = head - Vector((0, 0, height * 0.105))
    data = bpy.data.cameras.new('Fixed right profile')
    camera = bpy.data.objects.new('Fixed right profile', data)
    scene.collection.objects.link(camera)
    side = forward.cross(Vector((0, 0, 1))).normalized()
    camera.location = target + side * height * 3
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    data.type = 'ORTHO'
    data.ortho_scale = height * 0.64
    data.clip_end = max(100, height * 10)
    scene.camera = camera
    return target, height, side


def add_proxy(rig, forward):
    rig.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    head = rig.matrix_world @ rig.data.bones['head'].head_local
    foot = rig.matrix_world @ rig.data.bones['foot_l'].head_local
    height = abs(head.z - foot.z)
    up = Vector((0, 0, 1))
    heading = forward.to_track_quat('Y', 'Z').to_euler()

    def add_shape(name, bone, center, scale, color):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=16)
        shape = bpy.context.object
        shape.name = name
        shape.location = center
        shape.rotation_euler = heading
        shape.scale = Vector(scale) * height
        shape.color = color
        bpy.context.view_layer.update()
        rest_matrix = shape.matrix_world.copy()
        shape.parent = rig
        shape.parent_type = 'BONE'
        shape.parent_bone = bone
        shape.matrix_world = rest_matrix
        for polygon in shape.data.polygons:
            polygon.use_smooth = True

    neck = rig.matrix_world @ rig.data.bones['neck_01'].head_local
    chest = rig.matrix_world @ rig.data.bones['spine_05'].head_local
    add_shape('Shoulder orientation marker', 'spine_05', chest - up * height * 0.035,
              (0.17, 0.073, 0.085), (0.2, 0.38, 0.58, 1.0))
    add_shape('Neck orientation marker', 'neck_01', neck + up * height * 0.025,
              (0.049, 0.046, 0.061), (0.60, 0.47, 0.36, 1.0))
    add_shape('Head orientation marker', 'head', head + up * height * 0.057,
              (0.064, 0.079, 0.086), (0.85, 0.68, 0.51, 1.0))
    add_shape('Forward direction marker', 'head', head + (up * 0.049 + forward * 0.079) * height,
              (0.021, 0.035, 0.021), (0.66, 0.44, 0.28, 1.0))
    rig.data.pose_position = 'POSE'
    bpy.context.view_layer.update()


def render(scene, camera_target, height, side, forward, label, variant):
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.studiolight_rotate_z = 0.65
    scene.display.shading.color_type = 'SINGLE' if SOURCE['preview_mesh'] else 'OBJECT'
    scene.display.shading.single_color = (0.47, 0.64, 0.83)
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.background_type = 'WORLD'
    scene.world = bpy.data.worlds.new('Studio background') if scene.world is None else scene.world
    scene.world.color = (0.14, 0.14, 0.14)
    scene.render.resolution_x = 384
    scene.render.resolution_y = 384
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    for view, direction in {'profile': side, 'front': forward}.items():
        if variant == 'strong' and view == 'front':
            continue
        scene.camera.location = camera_target + direction * height * 3
        scene.camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(OUT / f'{label}-{variant}-{view}.png')
        bpy.ops.render.render(write_still=True)


def main():
    report = {'source': SOURCE, 'filter_speed_per_second': 18.0, 'presets': PRESETS, 'results': {}}
    only_label = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else None
    for label in SOURCE['clips']:
        if only_label and label != only_label:
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(OUT / f'{label}.fbx'))
        scene = bpy.context.scene
        rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
        object_scale = rig.matrix_world.to_scale()
        if not all(0.000001 < value < 10.0 for value in object_scale):
            raise RuntimeError(f'Unexpected rig scale: {object_scale}')
        source_action = rig.animation_data.action
        source_action.name = 'Unreal export'
        source_action.use_fake_user = True
        start, imported_end = map(round, source_action.frame_range)
        fps = scene.render.fps / scene.render.fps_base
        source_duration = SOURCE['clips'][label]['duration_seconds']
        end = start + round(source_duration * fps)
        if imported_end not in (end, end + 1):
            raise RuntimeError(f'{label} duration mismatch between Unreal and Blender')
        frames = range(start, end + 1)
        scene.frame_start, scene.frame_end = start, end
        rest_forward = rig.data.bones['ball_l'].head_local - rig.data.bones['foot_l'].head_local
        world_forward = rig.matrix_world.to_quaternion() @ rest_forward
        world_forward.z = 0
        world_forward.normalize()
        forward = rig.matrix_world.to_quaternion().inverted() @ world_forward
        rest = {name: rig.data.bones[name].matrix_local.to_quaternion() for name in BONES}
        source = source_samples(scene, rig, frames)
        if len(source) < 2:
            raise RuntimeError(f'No sampled motion in {label}')
        follow = 1.0 - math.exp(-18.0 / fps)
        variants = {'raw': source_action}
        results = {'frame_range': [start, end], 'fps': fps,
                   'raw': analyze(source, source, rest, fps, forward, object_scale.x)}
        for variant, (weight, limits) in PRESETS.items():
            residuals = filtered_residuals(source, rest, follow, weight, limits[label])
            variants[variant] = bake(scene, rig, source_action, frames, rest, residuals, variant)
            poses = source_samples(scene, rig, frames)
            measured = analyze(source, poses, rest, fps, forward, object_scale.x)
            if measured['head']['peak_from_reference_deg'] > limits[label] + 0.06:
                raise RuntimeError(f'{label} {variant} bake missed head limit: {measured}')
            if measured['protected']['max_orientation_change_deg'] > 0.08:
                raise RuntimeError(f'{label} {variant} changed a protected orientation: {measured}')
            results[variant] = measured
        report['results'][label] = results
        report_path = OUT / (f'{only_label}-blender-metrics.json' if only_label else 'blender-metrics.json')
        report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

        frame = max(frames, key=lambda i: angle(source[i - start]['head'].to_quaternion(), rest['head']))
        use_action(rig, source_action)
        if not SOURCE['preview_mesh']:
            add_proxy(rig, world_forward)
        target, height, side = setup_camera(scene, rig, frame, world_forward)
        for variant, action in variants.items():
            if label not in ('walk', 'jog') or variant == 'strong':
                continue
            use_action(rig, action)
            scene.frame_set(frame)
            render(scene, target, height, side, world_forward, label, variant)
        use_action(rig, variants['moderate'])
        scene.frame_set(frame)
        bpy.ops.wm.save_as_mainfile(filepath=str(OUT / f'{label}-head-comparison.blend'))


main()
