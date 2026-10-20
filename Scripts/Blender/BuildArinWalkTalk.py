"""Author Arin-specific in-place walk and conversation gestures, preserve v3.

Mixamo's public Walking/Talking previews inform timing/gesture; no downloaded
Mixamo motion data is embedded. Analytic leg IK is baked to the existing bones.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Art/NPC/Arin/ReferenceRefined/Arin.blend'
SOURCE_SHA = '56cc68fda9a32ec932a83d57a20dbb8897ef51933922c609027a542e2ad01f74'
OUT = ROOT / 'Art/NPC/Arin/Animation'
EVIDENCE = ROOT / 'Feature/doc/evidence/arin-walk-talk-20261009'
sys.path.insert(0, str(Path(__file__).parent))
import BuildArin as common


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def reset(rig):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        bone.rotation_quaternion = (1, 0, 0, 0)
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    for side, sign in (('l', 1), ('r', -1)):
        common.world_rotation(rig, 'upperarm_' + side, (0, 1, 0), sign * 68)
        common.world_rotation(rig, 'lowerarm_' + side, (0, 1, 0), sign * 7)
        common.world_rotation(rig, 'hand_' + side, (0, 0, 1), sign * 4)
    bpy.context.view_layer.update()


def rotate_world(rig, name, axis, degrees):
    bone = rig.pose.bones[name]
    rotation = Quaternion(Vector(axis), math.radians(degrees)) @ bone.matrix.to_quaternion()
    bone.matrix = Matrix.Translation(bone.head) @ rotation.to_matrix().to_4x4()
    bpy.context.view_layer.update()


def segment_matrix(rig, name, head, tail):
    rest = rig.data.bones[name]
    rotation = (rest.tail_local - rest.head_local).rotation_difference(tail - head) @ rest.matrix_local.to_quaternion()
    return Matrix.Translation(head) @ rotation.to_matrix().to_4x4()


def solve_leg(rig, side, ankle, pitch):
    upper = rig.pose.bones['thigh_' + side]
    lower = rig.pose.bones['calf_' + side]
    hip = upper.head.copy()
    delta = ankle - hip
    distance = delta.length
    length1 = rig.data.bones[upper.name].length
    length2 = rig.data.bones[lower.name].length
    if distance > length1 + length2 - .0001:
        raise RuntimeError('Unreachable foot target: ' + side + ' ' + str(distance))
    axis = delta.normalized()
    pole = Vector((0, -1, 0))
    bend = (pole - axis * pole.dot(axis)).normalized()
    along = (length1 * length1 - length2 * length2 + distance * distance) / (2 * distance)
    height = math.sqrt(max(0, length1 * length1 - along * along))
    knee = hip + axis * along + bend * height
    upper.matrix = segment_matrix(rig, upper.name, hip, knee)
    bpy.context.view_layer.update()
    lower.matrix = segment_matrix(rig, lower.name, knee, ankle)
    bpy.context.view_layer.update()
    foot = rig.pose.bones['foot_' + side]
    rotation = Quaternion(Vector((1, 0, 0)), math.radians(pitch)) @ rig.data.bones[foot.name].matrix_local.to_quaternion()
    foot.matrix = Matrix.Translation(ankle) @ rotation.to_matrix().to_4x4()
    bpy.context.view_layer.update()
    if (upper.tail-lower.head).length > .0001 or (lower.tail-foot.head).length > .0001:
        raise RuntimeError('Disconnected IK joints: '+side)


def foot_vertices(mesh, side):
    group = mesh.vertex_groups['foot_' + side].index
    return [v.co.copy() for v in mesh.data.vertices if v.co.z < .035 and
            any(g.group == group and g.weight > .90 for g in v.groups)]


def gait_foot(phase):
    # 50% stance + opposite-leg stance: constant 1.10m/s virtual ground speed.
    # Hermite swing keeps foot position AND velocity continuous at touchdown.
    if phase < .5:
        u = phase * 2
        y = -.33 + .66 * u
        pitch = -8 * max(0., 1 - u / .20) + 15 * max(0., (u - .65) / .35)
        lift = 0.
        planted = True
    else:
        u = (phase - .5) * 2
        h00, h10 = 2*u**3-3*u**2+1, u**3-2*u**2+u
        h01, h11 = -2*u**3+3*u**2, u**3-u**2
        y = h00 * .33 + h10 * .66 + h01 * -.33 + h11 * .66
        lift = .115 * math.sin(math.pi * u)**2
        ease = u*u*(3-2*u)
        pitch = 15*(1-ease)-8*ease
        planted = False
    return y, lift, pitch, planted


def walk_pose(rig, mesh, soles, t):
    reset(rig)
    # Pelvis is lowest at double contact, higher over the supporting leg.
    pelvis_world = Vector((.008*math.sin(math.tau*t), 0., -.058-.017*math.cos(2*math.tau*t)))
    rig.pose.bones['pelvis'].location = rig.data.bones['pelvis'].matrix_local.to_quaternion().inverted() @ pelvis_world
    common.world_rotation(rig, 'spine_02', (0, 0, 1), 2*math.sin(math.tau*t))
    common.world_rotation(rig, 'spine_03', (1, 0, 0), -1.5)
    common.world_rotation(rig, 'head', (0, 0, 1), -.6*math.sin(math.tau*t))
    bpy.context.view_layer.update()
    for side, sign in (('l', 1), ('r', -1)):
        phase = (t + (0 if side == 'l' else .5)) % 1.
        y, lift, pitch, planted = gait_foot(phase)
        rest_ankle = rig.data.bones['foot_' + side].head_local
        rocker = Quaternion(Vector((1, 0, 0)), math.radians(pitch))
        ground_offset = -min((rocker @ (v - rest_ankle)).z for v in soles[side])
        ankle = Vector((sign*.155, rest_ankle.y+y, ground_offset+lift+.001))
        solve_leg(rig, side, ankle, pitch)
        # Contralateral arm swing; hands stay outside the satchel corridor.
        arm_wave = math.cos(math.tau * phase)
        rotate_world(rig, 'upperarm_' + side, (1, 0, 0), 15*arm_wave)
        rotate_world(rig, 'lowerarm_' + side, (1, 0, 0), -7)


def talk_pose(rig, mesh, soles, t):
    reset(rig)
    left = max(0., math.sin(math.tau*t))**2
    right = max(0., -math.sin(math.tau*t))**2
    for side, sign, gesture in (('l', 1, left), ('r', -1, right)):
        common.world_rotation(rig, 'upperarm_' + side, (0, 1, 0), sign*(68-20*gesture))
        bpy.context.view_layer.update()
        rotate_world(rig, 'upperarm_' + side, (1, 0, 0), -12*gesture)
        rotate_world(rig, 'lowerarm_' + side, (1, 0, 0), -53*gesture)
        rotate_world(rig, 'hand_' + side, (0, 0, 1), sign*10*gesture)
    envelope = math.sin(math.pi*t)**2
    common.world_rotation(rig, 'spine_02', (0, 0, 1), 1.8*math.sin(math.tau*t))
    common.world_rotation(rig, 'spine_03', (1, 0, 0), 1.1*math.sin(2*math.tau*t))
    common.world_rotation(rig, 'head', (0, 0, 1), 2.0*math.sin(math.tau*t))
    # Compound yaw + nod, without erasing the nod quaternion.
    rotate_world(rig, 'head', (1, 0, 0), 2.2*envelope*math.sin(4*math.pi*t))
    bpy.context.view_layer.update()


def author(rig, mesh, soles, name, frames, pose):
    scene = bpy.context.scene
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    scene.frame_start, scene.frame_end = 1, frames + 1
    for frame in range(1, frames+2):
        scene.frame_set(frame)
        t = ((frame-1) % frames) / frames
        pose(rig, mesh, soles, t)
        for bone in rig.pose.bones:
            bone.keyframe_insert('rotation_quaternion', frame=frame, group=bone.name)
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('scale', frame=frame, group=bone.name)
    scene.frame_set(1)
    bpy.context.view_layer.update()
    return action


def probe(mesh, rig, action, soles):
    scene = bpy.context.scene
    rig.animation_data.action = action
    rows = []
    rest = np.array([tuple(v.co) for v in mesh.data.vertices])
    edges = np.array([tuple(e.vertices) for e in mesh.data.edges])
    lengths = np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]], axis=1)
    indices = {s:[v.index for v in mesh.data.vertices if v.co.z<.035 and
                 any(g.group==mesh.vertex_groups['foot_'+s].index and g.weight>.90 for g in v.groups)] for s in ('l','r')}
    first = None
    root_first = None
    for frame in range(1, int(action.frame_range[1])+1):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        ev = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        data = ev.to_mesh()
        flat = np.empty(len(data.vertices)*3)
        data.vertices.foreach_get('co', flat)
        points = flat.reshape((-1,3))
        stretch = np.linalg.norm(points[edges[:,0]]-points[edges[:,1]], axis=1) / np.maximum(lengths, 1e-8)
        if first is None:
            first = points.copy()
            root_first = rig.pose.bones['root'].matrix.copy()
        root_delta = max(abs(a-b) for a,b in zip(sum((list(row) for row in root_first),[]), sum((list(row) for row in rig.pose.bones['root'].matrix),[])))
        row = {'frame':frame,'finite':bool(np.isfinite(points).all()),'edges_over4':int((stretch>4).sum()),
            'max_edge_stretch':float(stretch.max()),'root_matrix_delta':root_delta,
            'sole_min_z_m':{s:float(points[indices[s],2].min()) for s in indices},
            'loop_endpoint_delta_m':float(np.linalg.norm(points-first,axis=1).max()) if frame==int(action.frame_range[1]) else None}
        if 'Walk' in action.name:
            t=((frame-1)%36)/36
            row['stance']={s:gait_foot((t+(0 if s=='l' else .5))%1)[3] for s in indices}
        rows.append(row)
        ev.to_mesh_clear()
    return {'action':action.name,'frames':list(map(float,action.frame_range)),'duration_seconds':(action.frame_range[1]-action.frame_range[0])/30,'samples':rows}


def export_animation(rig, action, frames, name):
    scene = bpy.context.scene
    rig.animation_data.action = action
    scene.frame_start, scene.frame_end = 1, frames+1
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path = OUT / name
    bpy.ops.export_scene.fbx(filepath=str(path), object_types={'ARMATURE'}, use_selection=True,
        add_leaf_bones=False, axis_forward='-Z', axis_up='Y', apply_unit_scale=True,
        bake_space_transform=False, bake_anim=True, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
        bake_anim_step=1, bake_anim_simplify_factor=0)
    return path


def render_samples(rig, actions):
    scene = bpy.context.scene
    camera = scene.camera
    scene.cycles.samples = 16
    scene.render.resolution_x,scene.render.resolution_y = 600,760
    out = EVIDENCE/'candidate'
    common.EVIDENCE = out
    out.mkdir(parents=True,exist_ok=True)
    records=[]
    for key, action, frames in actions:
        rig.animation_data.action = action
        for frame in ([1,7,10,16,19,25,28,34] if key=='walk' else [1,16,24,46,68,76]):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            for view,pos in [('oblique',(2.6,-4,1.45)),('profile',(4,0,.95))] if key=='walk' else [('front',(0,-4,.95)),('oblique',(2.6,-4,1.45))]:
                name=key+'-'+str(frame).zfill(3)+'-'+view
                common.render(camera,name,pos,(0,0,.85),1.95)
                records.append({'action':action.name,'frame':frame,'view':view,'file':str(out/(name+'.png')),'sha256':sha(out/(name+'.png'))})
    (EVIDENCE/'candidate-render-manifest.json').write_text(json.dumps(records,indent=2),encoding='utf-8')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--probe-only',action='store_true')
    parser.add_argument('--skip-render',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    if sha(SOURCE)!=SOURCE_SHA:
        raise RuntimeError('Accepted v3 source differs')
    OUT.mkdir(parents=True,exist_ok=True)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    rig=bpy.data.objects['Arin_Rig']
    mesh=bpy.data.objects['Arin_Adventurer']
    idle=rig.animation_data.action
    scene=bpy.context.scene
    scene.render.fps=30
    soles={side:foot_vertices(mesh,side) for side in ('l','r')}
    walk=author(rig,mesh,soles,'Arin_Walk',36,walk_pose)
    talk=author(rig,mesh,soles,'Arin_Talk',90,talk_pose)
    actions=[('walk',walk,36),('talk',talk,90)]
    probes=[probe(mesh,rig,action,soles) for key,action,frames in actions]
    (EVIDENCE/'source-motion-probe.json').write_text(json.dumps(probes,indent=2),encoding='utf-8')
    if args.probe_only:
        print('ARIN_MOTION_PROBE_COMPLETE')
        return
    paths=[export_animation(rig,action,frames,'A_Arin_'+key.title()+'.fbx') for key,action,frames in actions]
    rig.animation_data.action=idle
    scene.frame_start,scene.frame_end=1,91
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Arin_Motions.blend'))
    paths.append(OUT/'Arin_Motions.blend')
    report={'status':'exported_requires_independent_review','source':str(SOURCE),'source_sha256':SOURCE_SHA,
        'source_preserved':sha(SOURCE)==SOURCE_SHA,'source_script_sha256':sha(Path(__file__)),
        'blender_version':bpy.app.version_string,'mesh_or_material_edit':False,'bones':[b.name for b in rig.data.bones],
        'walk':{'action':'Arin_Walk','frames':[1,37],'seconds':1.2,'reference_speed_cm_s':110,'in_place':True},
        'talk':{'action':'Arin_Talk','frames':[1,91],'seconds':3,'scope':'body/head conversation gestures, no lip sync'},
        'reference':'Mixamo public Walking/Feminine Walk Forward and Talking/General Conversation previews; own baked keyframes, no downloaded mocap',
        'files':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in paths}}
    (EVIDENCE/'motion-build.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if not args.skip_render:
        render_samples(rig,actions)
    print(json.dumps({'status':report['status'],'files':report['files']}),flush=True)


if __name__=='__main__':
    main()
