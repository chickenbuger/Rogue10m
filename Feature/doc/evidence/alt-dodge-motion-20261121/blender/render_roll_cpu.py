"""Render identical Blender proxy poses with CPU Cycles for the comparison."""

import os
from pathlib import Path

import bpy
from mathutils import Vector


OUT = Path(__file__).resolve().parent / os.getenv('ROGUE_SHOULDER_ROLL_LABEL', 'v4')


def main():
    bpy.ops.wm.open_mainfile(filepath=str(OUT / 'MannyShoulderRoll.blend'))
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
    camera = scene.camera
    target = Vector((0, -.06, .82))
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 7
    scene.cycles.max_bounces = 1
    scene.cycles.diffuse_bounces = 1
    scene.cycles.glossy_bounces = 1
    scene.render.resolution_x = scene.render.resolution_y = 270
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 6
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (.36, .42, .5, 1.0)
    background.inputs['Strength'].default_value = .38

    material = bpy.data.materials.new('Proxy color')
    material.use_nodes = True
    nodes = material.node_tree.nodes
    shader = nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value = .62
    material.node_tree.links.new(nodes.new('ShaderNodeObjectInfo').outputs['Color'], shader.inputs['Base Color'])
    for obj in scene.objects:
        if obj.type == 'MESH':
            obj.data.materials.clear()
            obj.data.materials.append(material)
    light_data = bpy.data.lights.new('Softbox', 'AREA')
    light = bpy.data.objects.new('Softbox', light_data)
    scene.collection.objects.link(light)
    light.location = Vector((-2.5, -3, 5))
    light.rotation_euler = (target - light.location).to_track_quat('-Z', 'Y').to_euler()
    light_data.energy = 360
    light_data.shape = 'DISK'
    light_data.size = 4.0
    for label, action_name in [('existing', 'Existing Procedural Roll'), ('refined', 'Manny Shoulder Roll')]:
        action = bpy.data.actions[action_name]
        rig.animation_data.action = action
        rig.animation_data.action_slot = action.slots[0]
        for view, offset, frames in [('side', Vector((-4, 0, .12)), (7, 10, 13, 16, 19)),
                                     ('three-quarter', Vector((-3, -3, 1.45)), (13,))]:
            camera.location = target + offset
            camera.rotation_euler = (-offset).to_track_quat('-Z', 'Y').to_euler()
            for frame in frames:
                scene.frame_set(frame)
                scene.render.filepath = str(OUT / f'{label}-{view}-{frame:02d}.png')
                bpy.ops.render.render(write_still=True)


main()
