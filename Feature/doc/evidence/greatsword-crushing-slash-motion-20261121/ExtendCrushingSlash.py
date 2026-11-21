"""Advance the lead step and keep the rear sole anchored through CrushingSlash."""
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
for folder in (HERE, HERE.parent/'greatsword-guard-break-motion-20261121', HERE.parent/'greatsword-technique-motion-20261121'):
    sys.path.insert(0, str(folder))
from AuthorGuardBreak import keyed_pose
from AuthorWhirlwind import FPS, angle_degrees, ease, landmarks, local_basis, summarize

INTERVALS = 110
FORWARD = Vector((0.0, -1.0, 0.0))
UP = Vector((0.0, 0.0, 1.0))


def pulse(t, start, arrive, leave, stop):
    return ease((t-start)/(arrive-start)) * ease((stop-t)/(stop-leave))


def lift_step(t, start, stop):
    phase = max(0.0, min(1.0, (t-start)/(stop-start)))
    return math.sin(math.pi*phase)**2


def branch(rig, name):
    node = rig.data.bones[name]
    return {name, *(child.name for child in node.children_recursive)}


def limb_lengths(hip, knee, ankle):
    return (knee-hip).length, (ankle-knee).length


def sword_floor(source):
    right=source['hand_r'].translation.lerp(source['middle_01_r'].translation,.5)
    left=source['hand_l'].translation.lerp(source['middle_01_l'].translation,.5)
    shaft=(right-left).normalized()
    return min((right-30*shaft).z,(right+140*shaft).z)


def lower_for_ankle(hip, knee, ankle, new_hip, new_ankle):
    upper, lower = limb_lengths(hip,knee,ankle)
    max_reach = upper+lower-.75
    offset = new_hip-new_ankle
    horizontal = math.hypot(offset.x,offset.y)
    if horizontal >= max_reach:
        raise RuntimeError('Requested stride exceeds the real thigh and calf lengths.')
    return max(0.0, offset.z-math.sqrt(max_reach**2-horizontal**2))


def leg_pose(source, result, chain, masks, body_delta, offset):
    hip, knee, ankle = (source[name].translation for name in chain)
    new_hip, new_ankle = hip+body_delta, ankle+offset
    upper, lower = limb_lengths(hip,knee,ankle)
    line = new_ankle-new_hip
    distance = line.length
    heading = line.normalized()
    pole = (knee-hip) - heading * (knee-hip).dot(heading)
    if distance >= upper+lower or pole.length < .001:
        raise RuntimeError(f'Unreachable planted foot {chain}: {distance} cm')
    along = (upper**2-lower**2+distance**2)/(2*distance)
    height = math.sqrt(max(0.0, upper**2-along**2))
    new_knee = new_hip+heading*along+pole.normalized()*height

    thigh_rotation = (knee-hip).rotation_difference(new_knee-new_hip)
    calf_rotation = (ankle-knee).rotation_difference(new_ankle-new_knee)
    transforms = (Matrix.Translation(new_hip) @ thigh_rotation.to_matrix().to_4x4() @ Matrix.Translation(-hip),
                  Matrix.Translation(new_knee) @ calf_rotation.to_matrix().to_4x4() @ Matrix.Translation(-knee),
                  Matrix.Translation(offset))
    for group, transform in zip(masks, transforms):
        for name in group:
            result[name] = transform@source[name]


def advanced_pose(source, t, body, limbs, foot_ik):
    drive = pulse(t,.38,.68,.81,1.23)
    lead = pulse(t,.33,.63,.91,1.35)
    if drive == 0.0 and lead == 0.0:
        return source, 0., 0.
    offsets = (FORWARD*(38*lead) + UP*(4.0*lift_step(t,.33,.63)+5.0*lift_step(t,.91,1.35)), Vector((0.,0.,0.)))
    max_lower = max(0., min(16.,sword_floor(source)-8.))
    geometry = [tuple(source[name].translation for name in chain) for chain,_ in limbs]

    def required_lower(forward_cm):
        return max(lower_for_ankle(hip,knee,ankle,hip+FORWARD*forward_cm,ankle+offset)
                   for (hip,knee,ankle),offset in zip(geometry,offsets))

    requested = 26*drive
    if required_lower(requested) > max_lower:
        inside, outside = 0., requested
        for _ in range(16):
            probe = (inside+outside)*.5
            if required_lower(probe) <= max_lower:
                inside = probe
            else:
                outside = probe
        requested = inside
    travel = FORWARD*requested
    lower = max(min(9.*drive,max_lower), required_lower(requested))
    body_delta = travel-UP*lower
    pose = dict(source)
    shifted = Matrix.Translation(body_delta)
    for name in body:
        pose[name] = shifted@source[name]
    for (chain, masks), offset in zip(limbs, offsets):
        leg_pose(source,pose,chain,masks,body_delta,offset)
    for name in foot_ik:
        pose[name] = Matrix.Translation(offsets[0])@source[name]
    return pose, travel.length, lower


def main():
    blend, output = (Path(path).resolve() for path in sys.argv[sys.argv.index('--')+1:])
    output.mkdir(exist_ok=False)
    bpy.ops.wm.open_mainfile(filepath=str(blend))
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type=='ARMATURE')
    if len(rig.pose.bones)!=89 or scene.render.fps!=60 or scene.frame_end-scene.frame_start!=INTERVALS:
        raise RuntimeError('Expected the established 60 Hz Manny source and exact clip clock.')
    original = []
    for frame in range(INTERVALS+1):
        scene.frame_set(frame+1)
        original.append({bone.name:bone.matrix.copy() for bone in rig.pose.bones})

    body = branch(rig,'pelvis')|branch(rig,'ik_hand_root')|branch(rig,'center_of_mass')
    limbs = []
    for side in 'lr':
        names = tuple(f'{name}_{side}' for name in ('thigh','calf','foot'))
        upper, lower, foot = (branch(rig,name) for name in names)
        limbs.append((names,(upper-lower,lower-foot,foot)))
    lead_foot_ik = branch(rig,'ik_foot_l')
    rig.animation_data_clear()
    rig.animation_data_create()
    action = bpy.data.actions.new('Manny GreatSword Crushing Slash - Grounded Lead Step')
    action.use_fake_user = True
    rig.animation_data.action = action
    ready = local_basis(rig,original[0])
    previous = {}
    adjustments = []
    for frame, source in enumerate(original):
        motion, travel, lower = advanced_pose(source,frame/FPS,body,limbs,lead_foot_ik)
        added_floor = keyed_pose(rig, frame+1, local_basis(rig,motion), ready,1.,previous)
        adjustments.append((travel,lower,added_floor))
    for curve in action.fcurves:
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'

    poses = []
    rear_ankle_error = 0.
    length_error = 0.
    palms = []
    blade_clearance = []
    root_turn = 0.
    for frame, source in enumerate(original):
        scene.frame_set(frame+1)
        poses.append(landmarks(rig))
        bones = rig.pose.bones
        rear_ankle_error = max(rear_ankle_error,(bones['foot_r'].head-source['foot_r'].translation).length)
        for names, _ in limbs:
            used = limb_lengths(*(bones[name].head for name in names))
            reference = limb_lengths(*(source[name].translation for name in names))
            length_error = max(length_error, *(abs(a-b) for a,b in zip(used,reference)))
        right = bones['hand_r'].head.lerp(bones['middle_01_r'].head,.5)
        left = bones['hand_l'].head.lerp(bones['middle_01_l'].head,.5)
        span = right-left
        palms.append(span.length)
        blade_clearance.append(min((right-30*span.normalized()).z,(right+140*span.normalized()).z))
        root_turn = max(root_turn,angle_degrees(bones['root'].matrix.to_quaternion(),Matrix.Identity(4).to_quaternion()))
    result = summarize(poses)
    result['shoulder_largest_step_phase'] *= 100/INTERVALS
    result.update({'root_rotation_deg':root_turn, 'palm_span_cm':[min(palms),max(palms)],
                   'maximum_pelvis_advance_cm':max(row[0] for row in adjustments),
                   'maximum_pelvis_lowering_cm':max(row[1] for row in adjustments),
                   'rear_ankle_baseline_difference_cm':rear_ankle_error, 'maximum_leg_segment_length_error_cm':length_error,
                   'two_palms_sword_floor_proxy_cm':min(blade_clearance)})
    (output/'reach-motion.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    if (result['root_travel_cm']>.001 or root_turn>.02 or max(palms)>25 or min(blade_clearance)<3 or
            rear_ankle_error>.025 or length_error>.025 or result['maximum_pelvis_lowering_cm']>16):
        raise RuntimeError(f'Grounded reach correction failed: {result}')

    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(output/'MannyGreatSwordCrushingSlash.fbx'),use_selection=True,
        object_types={'ARMATURE'},add_leaf_bones=False,armature_nodetype='NULL',bake_anim=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_simplify_factor=0,
        bake_anim_force_startend_keying=True,axis_forward='-Y',axis_up='Z')
    scene.frame_set(round(.82*FPS)+1)
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'MannyGreatSwordCrushingSlash.blend'))
    print(f'CRUSHING_REACH_BAKED {json.dumps(result)}')


if __name__=='__main__':
    main()
