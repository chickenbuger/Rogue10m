"""Author and compare a compact Manny shoulder roll in Blender."""

import json
import math
import os
from pathlib import Path

import bpy
from mathutils import Quaternion, Vector


ROOT = Path(__file__).resolve().parent
OUT = ROOT / os.getenv('ROGUE_SHOULDER_ROLL_LABEL', 'v4')
UP = Vector((0, 0, 1))
FORWARD = Vector((0, -1, 0))
PITCH = UP.cross(FORWARD)
RIGHT = Vector((-1, 0, 0))
INTERVALS = 25
FPS = 60
DEFAULT_CAPSULE_TRAVEL_CM = 250.0
PHASES = (0.0, .10, .22, .34, .47, .58, .70, .82, .93, 1.0)
CURVES = {
    # Align the crouch, shoulder transfer and recovery with actor motion without moving the root.
    'pelvis_pitch': (0, 6, 28, 72, 154, 218, 273, 323, 351, 360),
    'pelvis_drop': (0, 11, 23, 33, 47, 63, 59, 41, 15, 0),
    'pelvis_forward': (0, 5, 15, 16, 12, 0, -12, -12, -7, 0),
    'pelvis_right': (0, 3, 8, 8, 3, -5, -8, -5, -2, 0),
    'diagonal': (0, 8, 17, 27, 41, 35, 10, -9, -6, 0),
    'torso': (0, 12, 24, 37, 62, 61, 46, 24, 7, 0),
    'chin': (0, 6, 19, 42, 61, 57, 39, 18, 4, 0),
    'thigh_l': (0, -21, -42, -69, -103, -112, -99, -49, -13, 0),
    'thigh_r': (0, -10, -27, -52, -85, -103, -54, -52, -22, 0),
    'calf_l': (0, 26, 52, 76, 110, 124, 112, 71, 25, 0),
    'calf_r': (0, 15, 37, 65, 99, 113, 74, 76, 36, 0),
    'foot_l': (0, -4, -18, -28, -32, -31, -16, -22, -6, 0),
    'foot_r': (0, -9, -13, -22, -23, -28, -29, -13, -4, 0),
    'upperarm_l': (0, 18, 37, 63, 81, 90, 84, 45, 19, 0),
    'upperarm_r': (0, 23, 52, 86, 94, 86, 67, 44, 27, 0),
    'forearm_l': (0, 18, 36, 48, 77, 91, 86, 41, 16, 0),
    'forearm_r': (0, 5, 15, 28, 59, 97, 99, 49, 15, 0),
    'arm_tuck': (0, .10, .34, .5, .84, 1, 1, .53, .14, 0),
}
ACTION_BONES = (
    'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05',
    'neck_01', 'neck_02', 'head',
    'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r',
    'hand_l', 'hand_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
    'foot_l', 'foot_r', 'ball_l', 'ball_r',
)


def smoothstep(low, high, value):
    p = min(1.0, max(0.0, (value - low) / (high - low)))
    return p * p * (3.0 - 2.0 * p)


def value(name, t):
    keys = CURVES[name]
    slopes = [(end - start) / (PHASES[i + 1] - PHASES[i])
              for i, (start, end) in enumerate(zip(keys, keys[1:]))]
    tangents = [0.0]
    for a, b in zip(slopes, slopes[1:]):
        tangents.append(2 * a * b / (a + b) if a * b > 0 else 0.0)
    tangents.append(0.0)
    for i in range(len(PHASES) - 1):
        if t <= PHASES[i + 1]:
            span = PHASES[i + 1] - PHASES[i]
            u = (t - PHASES[i]) / span
            return ((2*u**3 - 3*u*u + 1) * keys[i] + (u**3 - 2*u*u + u) * span * tangents[i]
                    + (-2*u**3 + 3*u*u) * keys[i + 1] + (u**3 - u*u) * span * tangents[i + 1])
    return keys[-1]


def rotate(bone, axis, degrees):
    apply_rotation(bone, Quaternion(axis, math.radians(degrees)))


def apply_rotation(bone, change):
    current = bone.matrix.to_quaternion()
    bone.rotation_quaternion = (bone.rotation_quaternion @ current.inverted() @ change @ current).normalized()
    bpy.context.view_layer.update()


def shift_pelvis(pelvis, armature_delta):
    # Translation lives in the rest parent coordinates, independent of animated pitch.
    basis = pelvis.bone.matrix_local.to_quaternion()
    if pelvis.parent:
        basis = pelvis.parent.matrix.to_quaternion() @ pelvis.parent.bone.matrix_local.to_quaternion().inverted() @ basis
    pelvis.location += basis.inverted() @ armature_delta
    bpy.context.view_layer.update()


def reset(rig, neutral):
    for bone in rig.pose.bones:
        bone.matrix_basis = neutral[bone.name]
    bpy.context.view_layer.update()


def original_roll(rig, t):
    tuck = min(1.0, 1.3 * math.sin(math.pi * t))
    angles = {'spine_01': 12, 'spine_02': 12, 'spine_03': 12, 'neck_01': 18}
    for suffix in ('l', 'r'):
        for bone, angle in [('clavicle', -15), ('upperarm', -62), ('lowerarm', 48), ('thigh', -76), ('calf', 112)]:
            angles[f'{bone}_{suffix}'] = angle
    for name, angle in angles.items():
        bone = rig.pose.bones[name]
        bone.rotation_quaternion = bone.rotation_quaternion.slerp(Quaternion(), tuck)
        bpy.context.view_layer.update()
        rotate(bone, PITCH, angle * tuck)
    pelvis = rig.pose.bones['pelvis']
    shift_pelvis(pelvis, -UP * (39.0 * tuck))
    rotate(pelvis, PITCH, 360.0 * smoothstep(.10, .91, t))


def bend_towards(bone, direction, degrees):
    child = bone.children[0]
    current = child.head - bone.head
    axis = current.cross(direction)
    if axis.length > .001:
        rotate(bone, axis.normalized(), min(degrees, math.degrees(current.angle(direction))))


def plant_lead_hand(rig, t, floor):
    strength = smoothstep(.31, .36, t) * (1.0 - smoothstep(.48, .65, t))
    if strength <= 0.0:
        return
    shoulder, elbow, wrist = (rig.pose.bones[bone] for bone in ('upperarm_r', 'lowerarm_r', 'hand_r'))
    wrist_goal = RIGHT * 9.0 + FORWARD * (145.0 - DEFAULT_CAPSULE_TRAVEL_CM * t) + UP * (floor + 5.3)
    wrist_goal = wrist.head.lerp(wrist_goal, strength)
    upper_length = (elbow.head - shoulder.head).length
    lower_length = (wrist.head - elbow.head).length
    reach = wrist_goal - shoulder.head
    extension = min(upper_length + lower_length - .2, max(abs(upper_length - lower_length) + .2, reach.length))
    line = reach.normalized()
    direction = elbow.head - shoulder.head
    elbow_plane = (direction - line * direction.dot(line)).normalized()
    if elbow_plane.length < .1:
        elbow_plane = (RIGHT - line * RIGHT.dot(line)).normalized()
    elbow_along = (upper_length**2 - lower_length**2 + extension**2) / (2*extension)
    elbow_across = math.sqrt(max(0.0, upper_length**2 - elbow_along**2))
    elbow_goal = shoulder.head + line * elbow_along + elbow_plane * elbow_across
    wrist_goal = shoulder.head + line * extension
    apply_rotation(shoulder, (elbow.head - shoulder.head).rotation_difference(elbow_goal - shoulder.head))
    apply_rotation(elbow, (wrist.head - elbow.head).rotation_difference(wrist_goal - elbow.head))


def refined_roll(rig, t, markers, floor):
    bones = rig.pose.bones
    for index, weight in enumerate((.12, .15, .20, .23, .30), start=1):
        rotate(bones[f'spine_0{index}'], PITCH, value('torso', t) * weight)
    rotate(bones['neck_01'], PITCH, .29 * value('chin', t))
    rotate(bones['neck_02'], PITCH, .37 * value('chin', t))
    rotate(bones['head'], PITCH, .34 * value('chin', t))

    # Stagger knees and place the lead elbow forward before the shoulder bears the pose.
    for suffix, side_sign in [('l', -1.0), ('r', 1.0)]:
        hip = bones[f'thigh_{suffix}']
        knee = bones[f'calf_{suffix}']
        rotate(hip, PITCH, value(f'thigh_{suffix}', t))
        rotate(hip, FORWARD, side_sign * value('arm_tuck', t) * 6.0)
        rotate(knee, PITCH, value(f'calf_{suffix}', t))
        rotate(bones[f'foot_{suffix}'], PITCH, value(f'foot_{suffix}', t))
        rotate(bones[f'ball_{suffix}'], PITCH, .22 * value(f'foot_{suffix}', t))

        clavicle = bones[f'clavicle_{suffix}']
        rotate(clavicle, PITCH, -side_sign * value('arm_tuck', t) * 6)
        arm = bones[f'upperarm_{suffix}']
        elbow = bones[f'lowerarm_{suffix}']
        bend_towards(arm, FORWARD * .8 - UP * .45 + RIGHT * side_sign * .13, value(f'upperarm_{suffix}', t))
        across_chest = -RIGHT * side_sign * .6 + FORWARD * .45 + UP * .55
        blend = value('arm_tuck', t)
        approach_floor = FORWARD * .45 - UP * .8
        direction = approach_floor.lerp(across_chest, smoothstep(.29, .47, t))
        bend_towards(elbow, direction, value(f'forearm_{suffix}', t))
        rotate(bones[f'hand_{suffix}'], PITCH, (22.0 - side_sign * 8.0) * blend)

    pelvis = bones['pelvis']
    shift_pelvis(pelvis, FORWARD * value('pelvis_forward', t) + RIGHT * value('pelvis_right', t)
                 - UP * value('pelvis_drop', t))
    rotate(pelvis, PITCH, value('pelvis_pitch', t))
    rotate(pelvis, FORWARD, value('diagonal', t))
    lead_hand_support = smoothstep(.29, .40, t) * (1.0 - smoothstep(.44, .57, t))
    bend_towards(bones['upperarm_r'], FORWARD * .30 - UP * .86 + RIGHT * .17, 30.0 * lead_hand_support)
    bend_towards(bones['lowerarm_r'], FORWARD * .21 - UP * .95, 38.0 * lead_hand_support)
    correction = fit_floor(rig, t, markers, floor)
    plant_lead_hand(rig, t, floor)
    contact_guard = max(0.0, -.4 - min(floor_gaps(rig, markers, floor).values()))
    shift_pelvis(pelvis, UP * contact_guard)
    return correction + contact_guard


def create_floor_markers(rig):
    bones = rig.pose.bones
    def anatomical(name, bone, offset, radius, group):
        original = bones[bone].bone.matrix_local
        return name, bone, original.to_3x3().inverted() @ Vector(offset), radius, group

    markers = [
        anatomical('head', 'head', (0, -2, 8), 9.0, 'head'),
        anatomical('back', 'spine_04', (0, 9, 0), 8.0, 'shoulder_back'),
        anatomical('chest', 'spine_04', (0, -7, 0), 8.0, 'torso'),
        anatomical('hip', 'pelvis', (0, 5, -4), 13.0, 'hip'),
    ]
    for suffix, side in (('l', 1), ('r', -1)):
        markers.extend([
            anatomical(f'shoulder_{suffix}', f'upperarm_{suffix}', (side*2, 1, 0), 8.0, 'shoulder_back'),
            anatomical(f'hand_{suffix}', f'hand_{suffix}', (0, -4, -1), 2.8, 'hand'),
            anatomical(f'knee_{suffix}', f'calf_{suffix}', (0, -2, 0), 5.5, 'knee'),
            anatomical(f'heel_{suffix}', f'foot_{suffix}', (0, 4, -5), 3.8, 'foot'),
            anatomical(f'toe_{suffix}', f'ball_{suffix}', (0, -2, 1.5), 2.7, 'foot'),
        ])
    return markers


def floor_gaps(rig, markers, floor):
    return {name: (rig.pose.bones[bone].matrix @ local).z - radius - floor
            for name, bone, local, radius, _ in markers}


def fit_floor(rig, t, markers, floor):
    # Use existing pose heights until the entry crouch, then keep the support chain near floor.
    gaps = floor_gaps(rig, markers, floor)
    approach = smoothstep(.03, .21, t) * (1.0 - smoothstep(.88, 1.0, t))
    correction = -min(gaps.values()) * approach
    correction = max(correction, -min(gaps.values()) - .4)
    shift_pelvis(rig.pose.bones['pelvis'], UP * correction)
    return correction


def capture(rig, markers, floor, t, correction):
    gaps = floor_gaps(rig, markers, floor)
    pose = rig.pose.bones
    c = pose['head'].head - pose['pelvis'].head
    knees = {side: (pose[f'calf_{side}'].head - pose['pelvis'].head).length for side in ('l','r')}
    closest = min(gaps, key=gaps.get)
    hand = min(gaps['hand_l'], gaps['hand_r'])
    shoulder = min(gaps['back'], gaps['shoulder_l'], gaps['shoulder_r'])
    foot = min(gaps['heel_l'], gaps['heel_r'], gaps['toe_l'], gaps['toe_r'])
    wrist = pose['hand_r'].head + FORWARD * DEFAULT_CAPSULE_TRAVEL_CM * t
    return {'phase': t, 'pelvis_up_cm': pose['pelvis'].head.z - floor, 'floor_correction_cm': correction,
            'nearest_support': closest, 'lowest_gap_cm': gaps[closest], 'head_gap_cm': gaps['head'],
            'hand_gap_cm': hand, 'shoulder_back_gap_cm': shoulder, 'foot_gap_cm': foot,
            'head_pelvis_gap_cm': c.length, 'knee_pelvis_gap_cm': knees, 'floor_gaps_cm': gaps,
            'lead_wrist_at_default_travel_cm': list(wrist)}


def add_root_bone(rig):
    # Unreal FBX's top root became the Blender rig object on import. Make a real bone for export.
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    bones = rig.data.edit_bones
    root = bones.new('root')
    root.head = (0, 0, 0)
    root.tail = (0, 10, 0)
    for bone in list(bones):
        if bone != root and bone.parent is None:
            bone.parent = root
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.name = 'Armature'


def bake(rig, scene, name, neutral, markers, floor, refined):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    previous = {}
    metrics = []
    for frame in range(1, INTERVALS + 2):
        t = (frame - 1) / INTERVALS
        scene.frame_set(frame)
        reset(rig, neutral)
        correction = refined_roll(rig, t, markers, floor) if refined else original_roll(rig, t) or 0.0
        for bone in rig.pose.bones:
            rotation = bone.rotation_quaternion.copy()
            if bone.name in previous and rotation.dot(previous[bone.name]) < 0:
                rotation.negate()
                bone.rotation_quaternion = rotation
            previous[bone.name] = rotation
            bone.keyframe_insert(data_path='rotation_quaternion', frame=frame, group=bone.name)
            bone.keyframe_insert(data_path='location', frame=frame, group=bone.name)
        metrics.append(capture(rig, markers, floor, t, correction))
    return action, metrics


def select_action(rig, action):
    rig.animation_data.action = action
    rig.animation_data.action_slot = action.slots[0]


def save_curve_file(rig, scene, neutral):
    rows = []
    # Blender armature units are centimeters because the imported component scales to .01 m.
    for frame in range(1, INTERVALS + 2):
        scene.frame_set(frame)
        values = {}
        for name in ACTION_BONES:
            bone = rig.pose.bones[name]
            values[name] = {
                'rotation_xyzw': [round(v, 9) for v in (bone.rotation_quaternion.x, bone.rotation_quaternion.y,
                                                        bone.rotation_quaternion.z, bone.rotation_quaternion.w)],
                'location_cm': [round(v, 6) for v in bone.location],
            }
        rows.append({'phase': (frame - 1) / INTERVALS, 'bones': values})
    source = {'basis': 'Blender armature from MannyIdle.fbx, local pose delta from FBX reference bone',
              'length_seconds': INTERVALS / FPS, 'keys': rows}
    (OUT / 'MannyShoulderRoll.blender-curves.json').write_text(json.dumps(source, indent=2) + '\n', encoding='utf-8')


def add_capsule(rig, name, bone, a, b, width, depth, color):
    center = (a + b) / 2
    length = (b - a).length
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=16)
    obj = bpy.context.object
    obj.name = name
    obj.location = rig.matrix_world @ center
    obj.rotation_euler = (b - a).to_track_quat('Z', 'Y').to_euler()
    obj.scale = (.01 * width, .01 * depth, .01 * (length / 2 + width * .25))
    obj.color = color
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    obj.parent, obj.parent_type, obj.parent_bone = rig, 'BONE', bone
    obj.matrix_world = transform


def add_proxy(rig, floor):
    rig.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    bones = rig.pose.bones
    point = lambda name: bones[name].head.copy()
    chest = (.23, .44, .67, 1.0)
    hips = (.18, .29, .43, 1.0)
    skin = (.86, .68, .49, 1.0)
    right = (.84, .45, .25, 1.0)
    left = (.39, .68, .70, 1.0)
    add_capsule(rig, 'hip', 'pelvis', point('pelvis') - UP * 10, point('pelvis') + UP * 12, 16, 12, hips)
    add_capsule(rig, 'lower torso', 'spine_02', point('spine_02') - UP * 8, point('spine_04'), 15, 10, chest)
    add_capsule(rig, 'upper torso', 'spine_04', point('spine_04') - UP * 5, point('neck_01'), 17, 10, chest)
    add_capsule(rig, 'neck', 'neck_01', point('neck_01'), point('head') + UP * 3, 5.1, 5, skin)
    add_capsule(rig, 'head', 'head', point('head') + UP * 1, point('head') + UP * 16, 8.9, 8.6, skin)
    add_capsule(rig, 'head forward', 'head', point('head') + UP * 9 + FORWARD * 6,
                point('head') + UP * 9 + FORWARD * 11, 2.4, 2.5, skin)
    for side, color in [('r', right), ('l', left)]:
        for stem, end, width in [('upperarm', 'lowerarm', 6.5), ('lowerarm', 'hand', 5.0),
                                 ('thigh', 'calf', 8.5), ('calf', 'foot', 5.8),
                                 ('foot', 'ball', 5.1)]:
            name = f'{stem}_{side}'
            add_capsule(rig, name, name, point(name), point(f'{end}_{side}'), width, width, color)
        name = f'hand_{side}'
        add_capsule(rig, name, name, point(name), point(f'middle_01_{side}'), 4.2, 3.2, color)
    rig.data.pose_position = 'POSE'
    bpy.context.view_layer.update()
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, floor * .01))
    bpy.context.object.name = 'Contact plane'
    bpy.context.object.color = (.37, .40, .43, 1)


def render_profiles(scene, rig, actions):
    data = bpy.data.cameras.new('Comparison')
    camera = bpy.data.objects.new('Comparison', data)
    scene.collection.objects.link(camera)
    target = Vector((0, -.06, .82))
    data.type = 'ORTHO'
    data.ortho_scale = 2.25
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'OBJECT'
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.show_shadows = True
    scene.world = bpy.data.worlds.new('Slate')
    scene.world.color = (.2, .22, .25)
    scene.render.resolution_x = scene.render.resolution_y = 300
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    select_action(rig, actions['refined'])
    scene.frame_set(13)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'MannyShoulderRoll.blend'))
    if os.getenv('ROGUE_ROLL_SKIP_RENDER') == '1':
        return
    for label, action in actions.items():
        select_action(rig, action)
        for view, offset in [('side', Vector((-4, 0, .12))), ('three-quarter', Vector((-3, -3, 1.45)))]:
            camera.location = target + offset
            camera.rotation_euler = (-offset).to_track_quat('-Z', 'Y').to_euler()
            for frame in (1, 7, 10, 13, 16, 19, 23, 26):
                scene.frame_set(frame)
                scene.render.filepath = str(OUT / f'{label}-{view}-{frame:02d}.png')
                bpy.ops.render.render(write_still=True)


def main():
    OUT.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(ROOT / 'MannyIdle.fbx'))
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
    scene.frame_set(1)
    neutral = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
    rig.animation_data.action = None
    add_root_bone(rig)
    neutral['root'] = rig.pose.bones['root'].matrix_basis.copy()
    reset(rig, neutral)
    scene.frame_start, scene.frame_end, scene.render.fps = 1, INTERVALS + 1, FPS
    markers = create_floor_markers(rig)
    floor = min(floor_gaps(rig, markers, 0.0).values())
    original, old_metrics = bake(rig, scene, 'Existing Procedural Roll', neutral, markers, floor, False)
    improved, metrics = bake(rig, scene, 'Manny Shoulder Roll', neutral, markers, floor, True)
    select_action(rig, improved)
    save_curve_file(rig, scene, neutral)
    (OUT / 'roll-metrics.json').write_text(json.dumps({
        'fps': FPS, 'intervals': INTERVALS, 'neutral_floor_z_cm': floor,
        'comparison': {'existing': old_metrics, 'refined': metrics},
    }, indent=2) + '\n', encoding='utf-8')

    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'MannyShoulderRoll.fbx'), use_selection=True,
                             object_types={'ARMATURE'}, add_leaf_bones=False, armature_nodetype='NULL',
                             bake_anim=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True,
                             axis_forward='-Y', axis_up='Z')

    add_proxy(rig, floor)
    render_profiles(scene, rig, {'existing': original, 'refined': improved})


main()
