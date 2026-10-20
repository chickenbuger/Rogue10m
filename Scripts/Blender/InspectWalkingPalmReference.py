"""Read-only FBX walking reference inspection in a fresh background Blender.

The import exists only in this isolated process. Source FBX bytes are hashed
before and after inspection. Palm axes are derived from finger joint geometry,
never assigned from an unverified hand bone local axis. A geometric palm plane
does not establish its palmar side unless finger flexion or mesh supports it.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

try:
    import bpy
except ModuleNotFoundError:
    bpy = None
if bpy is not None:
    from mathutils import Vector


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def vec(value):
    return [float(x) for x in value]


def canonical(value):
    """FBX reference import: Blender -Y forward, -X right, +Z up."""
    return [-float(value.y), -float(value.x), float(value.z)]


def palm_semantics(bones, side):
    prefix = 'mixamorig:' + side
    point = lambda name: Vector(bones[prefix + name]['head_world'])
    wrist = point('Hand')
    middle = point('HandMiddle1')
    index = point('HandIndex1')
    pinky = point('HandPinky1')
    thumb = point('HandThumb1')
    finger_long = (middle - wrist).normalized()
    radial = (index - pinky).normalized()
    normal = finger_long.cross(radial).normalized()
    if (thumb - middle).dot(normal) < 0:
        normal.negate()
    flexion = [100 * (point('Hand' + finger + '4') -
                     point('Hand' + finger + '1')).dot(normal)
               for finger in ('Index', 'Middle', 'Ring', 'Pinky')]
    inward = Vector((-1 if side == 'Left' else 1, 0, 0))
    return {
        'palm_normal_world': vec(normal),
        'palm_normal_forward_right_up': canonical(normal),
        'finger_long_world': vec(finger_long),
        'finger_long_forward_right_up': canonical(finger_long),
        'radial_index_side_world': vec(radial),
        'radial_index_side_forward_right_up': canonical(radial),
        'palm_inward_dot': normal.dot(inward),
        'thumb_palmar_displacement_cm': 100 * (thumb - middle).dot(normal),
        'finger_flexion_palmar_displacement_cm': flexion,
        'wrist_position_forward_right_up_cm_relative_hips':
            [100 * value for value in canonical(wrist - Vector(bones['mixamorig:Hips']['head_world']))],
        'forearm_long_forward_right_up': canonical((wrist - point('ForeArm')).normalized()),
    }


def material(name, color):
    data = bpy.data.materials.new(name)
    data.diffuse_color = (*color, 1)
    return data


def cylinder(name, start, end, radius, mat):
    direction = end - start
    if direction.length < 1e-5:
        return None
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius,
                                        depth=direction.length,
                                        location=(start + end) / 2)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
    obj.data.materials.append(mat)
    return obj


def render_skeleton(rig, frames, directory):
    """Diagnostic skeleton + semantic palm planes, not invented FBX skin."""
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 720
    scene.render.resolution_y = 920
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.background_type = 'WORLD'
    scene.world.color = (0.045, 0.055, 0.065)
    scene.view_settings.view_transform = 'Standard'
    mats = {'body': material('Diagnostic skeleton', (.52, .56, .6)),
            'Left': material('Left hand', (.17, .6, .8)),
            'Right': material('Right hand', (.77, .48, .15)),
            'palm': material('Palmar normal', (.93, .18, .1))}
    camera_data = bpy.data.cameras.new('DiagnosticCamera')
    camera = bpy.data.objects.new('DiagnosticCamera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = 2.2
    camera_data.lens = 45
    outputs = []
    generated = []
    for phase, frame in frames:
        for obj in generated:
            bpy.data.objects.remove(obj, do_unlink=True)
        generated.clear()
        scene.frame_set(math.floor(frame), subframe=frame % 1)
        evaluated = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        world = evaluated.matrix_world.copy()
        hips = world @ evaluated.pose.bones['mixamorig:Hips'].head
        shift = Vector((-hips.x, -hips.y, 0))
        # Freeze evaluated geometry before mesh operators can invalidate it.
        bones = {bone.name: {'head_world': vec(world @ bone.head), 'tail_world': vec(world @ bone.tail)}
                 for bone in evaluated.pose.bones}
        for name, positions in bones.items():
            start, end = Vector(positions['head_world']) + shift, Vector(positions['tail_world']) + shift
            side = 'Left' if 'LeftHand' in name else 'Right' if 'RightHand' in name else 'body'
            radius = .0032 if 'Hand' in name and ('1' in name or '2' in name or '3' in name or '4' in name) else .009
            obj = cylinder(name, start, end, radius, mats[side])
            if obj:
                generated.append(obj)
        for side in ('Left', 'Right'):
            metrics = palm_semantics(bones, side)
            prefix = 'mixamorig:' + side
            wrist = Vector(bones[prefix + 'Hand']['head_world']) + shift
            middle = Vector(bones[prefix + 'HandMiddle1']['head_world']) + shift
            normal = Vector(metrics['palm_normal_world'])
            center = (wrist + middle) / 2
            obj = cylinder(side + ' semantic palm disc', center - normal * .002,
                           center + normal * .002, .033, mats[side])
            if obj:
                generated.append(obj)
            obj = cylinder(side + ' palmar normal arrow', center, center + normal * .105,
                           .006, mats['palm'])
            if obj:
                generated.append(obj)
        views = [('front', Vector((.12, -4, 1.15)), Vector((0, 0, 1.0)), 2.7),
                 ('three-quarter', Vector((-3, -4, 1.45)), Vector((0, 0, 1.0)), 2.7)]
        for side in ('Left', 'Right'):
            wrist = Vector(bones['mixamorig:' + side + 'Hand']['head_world']) + shift
            middle = Vector(bones['mixamorig:' + side + 'HandMiddle1']['head_world']) + shift
            center = (wrist + middle) / 2
            offset = Vector((-.27 if side == 'Left' else .27, -.5, .05))
            views.append((side.lower() + '-hand-detail', center + offset, center, .35))
        for view, location, target, scale in views:
            camera_data.ortho_scale = scale
            camera.location = location
            camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
            filename = directory / ('phase-' + str(round(phase * 100)).zfill(3) + '-' + view + '.png')
            scene.render.filepath = str(filename)
            bpy.context.view_layer.update()
            bpy.ops.render.render(write_still=True)
            outputs.append({'phase': phase, 'frame': frame, 'view': view,
                            'file': str(filename), 'sha256': sha256(filename)})
    return outputs


def assemble_contact_sheet(folder):
    """Run with project Python/Pillow after the isolated Blender render."""
    from PIL import Image, ImageDraw, ImageFont
    path = folder / 'reference-palms.json'
    report = json.loads(path.read_text(encoding='utf-8'))
    if not report['source_preserved'] or sha256(Path(report['source'])) != report['sha256_before']:
        raise RuntimeError('Reference source has changed since the recorded inspection')
    for render in report['rigs'][0]['renders']:
        if sha256(Path(render['file'])) != render['sha256']:
            raise RuntimeError('A diagnostic render changed after inspection')
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 19)
    small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 15)
    board = Image.new('RGB', (1440, 1096), (24, 28, 34))
    draw = ImageDraw.Draw(board)
    draw.text((20, 14), 'Walking.fbx: geometry-derived palm facing across one gait cycle', font=font, fill='white')
    draw.text((20, 43), 'Animation skeleton only. Blue=left hand, gold=right hand, red=palmar normal. +F forward / +R right / +U up.',
              font=small, fill=(193, 205, 218))
    for row, side in enumerate(('Left', 'Right')):
        for column, phase in enumerate((0, .25, .5, .75)):
            x, y = column * 360, 84 + row * 506
            sample = min(report['rigs'][0]['samples'], key=lambda sample: abs(sample['phase'] - phase))
            palms = sample['palms'][side]
            filename = folder / ('phase-' + str(round(phase * 100)).zfill(3) + '-' + side.lower() + '-hand-detail.png')
            with Image.open(filename) as original:
                preview = original.convert('RGB').resize((360, 460))
            board.paste(preview, (x, y + 46))
            color = (110, 209, 249) if side == 'Left' else (236, 184, 102)
            draw.text((x + 10, y + 2), side + ' / cycle ' + str(round(phase * 100)) + '%', font=font, fill=color)
            normal = palms['palm_normal_forward_right_up']
            draw.text((x + 10, y + 27), 'normal (F/R/U) ' + ' / '.join(format(value, '+.2f') for value in normal), font=small, fill='white')
    target = folder / 'reference-hand-contact-sheet.png'
    board.save(target)
    report['contact_sheet'] = {'file': str(target), 'sha256': sha256(target)}
    path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('WALKING_PALM_REFERENCE_ASSEMBLED', str(target), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--assemble', action='store_true', help='Use ordinary project Python/Pillow to assemble the final render contact sheet')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    args.output = args.output.resolve()
    if args.assemble:
        assemble_contact_sheet(args.output.parent)
        return
    if bpy is None:
        raise RuntimeError('FBX inspection requires Blender, --assemble uses project Python/Pillow')
    if not bpy.app.background:
        raise RuntimeError('Only a fresh background Blender process is permitted')
    source = args.input.resolve()
    digest = sha256(source)
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.import_scene.fbx(filepath=str(source), use_anim=True,
                             automatic_bone_orientation=False)
    rigs = [obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE']
    report = {'source': str(source), 'sha256_before': digest,
              'blender_version': bpy.app.version_string,
              'fps': bpy.context.scene.render.fps / bpy.context.scene.render.fps_base,
              'objects': [{'name': obj.name, 'type': obj.type,
                           'matrix_world': [vec(row) for row in obj.matrix_world],
                           'vertices': len(obj.data.vertices) if obj.type == 'MESH' else None}
                          for obj in bpy.context.scene.objects],
              'actions': [{'name': action.name, 'frame_range': vec(action.frame_range)}
                          for action in bpy.data.actions], 'rigs': []}
    for rig in rigs:
        record = {'name': rig.name, 'rest_bones': {}, 'samples': []}
        for bone in rig.data.bones:
            record['rest_bones'][bone.name] = {
                'parent': bone.parent.name if bone.parent else None,
                'head_world': vec(rig.matrix_world @ bone.head_local),
                'tail_world': vec(rig.matrix_world @ bone.tail_local),
                'matrix_local': [vec(row) for row in bone.matrix_local]}
        active = rig.animation_data.action if rig.animation_data else None
        record['active_action'] = active.name if active else None
        start, end = active.frame_range if active else (1, 1)
        frames = sorted(set([round(start + (end - start) * index / 116, 6)
                             for index in range(117)]))
        for frame in frames:
            bpy.context.scene.frame_set(math.floor(frame), subframe=frame % 1)
            evaluated = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
            poses = {}
            for bone in evaluated.pose.bones:
                poses[bone.name] = {
                    'head_world': vec(evaluated.matrix_world @ bone.head),
                    'tail_world': vec(evaluated.matrix_world @ bone.tail),
                    'rotation_world_wxyz': vec((evaluated.matrix_world @ bone.matrix).to_quaternion()),
                    'matrix_basis_quaternion_wxyz': vec(bone.matrix_basis.to_quaternion())}
            semantics = {side: palm_semantics(poses, side) for side in ('Left', 'Right')}
            record['samples'].append({'frame': frame, 'phase': (frame - start) / (end - start),
                                      'bones': poses, 'palms': semantics})
        record['semantic_axes'] = {'forward_blender_world': [0, -1, 0],
                                   'right_blender_world': [-1, 0, 0],
                                   'up_blender_world': [0, 0, 1]}
        first_hips = Vector(record['samples'][0]['bones']['mixamorig:Hips']['head_world'])
        last_hips = Vector(record['samples'][-1]['bones']['mixamorig:Hips']['head_world'])
        record['semantic_axes_evidence'] = {
            'hips_first_to_last_m': vec(last_hips - first_hips),
            'rest_right_shoulder_position_m': record['rest_bones']['mixamorig:RightArm']['head_world'],
            'rest_left_shoulder_position_m': record['rest_bones']['mixamorig:LeftArm']['head_world'],
            'root_translation_cm': 100 * (last_hips - first_hips).length,
            'heading_is_negative_blender_y':
                (last_hips - first_hips).normalized().dot(Vector((0, -1, 0))) > .99}
        record['semantic_method'] = {
            'plane': 'normalize(cross(Middle1-wrist, Index1-Pinky1))',
            'sign': 'Toward Thumb1 displacement from metacarpal plane; independently corroborated by actual flexion of all 4 non-thumb fingers.',
            'limitation': 'FBX contains animation skeleton only, no skin mesh; render is a skeleton diagnostic.'}
        record['rest_palms'] = {side: palm_semantics(record['rest_bones'], side)
                                for side in ('Left', 'Right')}
        for side in ('Left', 'Right'):
            hand = rig.data.bones['mixamorig:' + side + 'Hand']
            inverse = (rig.matrix_world @ hand.matrix_local).to_3x3().normalized().inverted()
            record['rest_palms'][side]['semantic_vectors_in_verified_hand_bone_local_space'] = {
                key: vec((inverse @ Vector(record['rest_palms'][side][key])).normalized())
                for key in ('palm_normal_world', 'finger_long_world', 'radial_index_side_world')}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if args.render:
            record['renders'] = render_skeleton(rig, [(phase, start + (end - start) * phase)
                                               for phase in (0, .25, .5, .75)], args.output.parent)
        report['rigs'].append(record)
    report['sha256_after'] = sha256(source)
    report['source_preserved'] = digest == report['sha256_after']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    compact = {key: report[key] for key in ('source', 'sha256_before', 'sha256_after',
                                          'source_preserved', 'blender_version', 'fps',
                                          'objects', 'actions')}
    compact['rigs'] = [{key: record[key] for key in ('name', 'active_action', 'semantic_axes', 'semantic_axes_evidence',
                        'semantic_method', 'rest_palms')} |
                       {'samples': [{'frame': sample['frame'], 'phase': sample['phase'],
                                     'palms': sample['palms']} for sample in record['samples']],
                        'renders': record.get('renders', [])}
                      for record in report['rigs']]
    for record in compact['rigs']:
        record['summary'] = {}
        for side in ('Left', 'Right'):
            palms = [sample['palms'][side] for sample in record['samples']]
            record['summary'][side] = {
                'palm_inward_dot_min_max': [min(p['palm_inward_dot'] for p in palms),
                                            max(p['palm_inward_dot'] for p in palms)],
                'palm_forward_dot_min_max': [min(p['palm_normal_forward_right_up'][0] for p in palms),
                                             max(p['palm_normal_forward_right_up'][0] for p in palms)],
                'palm_up_dot_min_max': [min(p['palm_normal_forward_right_up'][2] for p in palms),
                                        max(p['palm_normal_forward_right_up'][2] for p in palms)],
                'minimum_individual_finger_flexion_palmar_displacement_cm': min(
                    value for p in palms for value in p['finger_flexion_palmar_displacement_cm']),
                'minimum_thumb_palmar_displacement_cm': min(p['thumb_palmar_displacement_cm'] for p in palms)}
    (args.output.parent / 'reference-palms.json').write_text(json.dumps(compact, indent=2), encoding='utf-8')
    print('WALKING_PALM_REFERENCE_INSPECTED', str(args.output), flush=True)
    if not report['source_preserved']:
        raise RuntimeError('Source FBX bytes changed')


if __name__ == '__main__':
    main()
