"""Build the editable Arin humanoid through the live upstream Blender MCP."""
import hashlib
import importlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector, Quaternion

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import ArinBody
import ArinHead
importlib.reload(ArinBody)
importlib.reload(ArinHead)

OUT = ROOT / "Art/NPC/Arin"
EVIDENCE = ROOT / "Feature/doc/evidence/arin-blender-20261007"


def materials():
    palette = {
        "skin": ((.65, .36, .22), .62, 0),
        "hair": ((.105, .037, .020), .48, 0),
        "hair_light": ((.19, .077, .035), .55, 0),
        "eye_white": ((.91, .86, .75), .31, 0),
        "iris": ((.13, .24, .105), .33, 0),
        "pupil": ((.006, .010, .007), .3, 0),
        "lip": ((.36, .12, .085), .64, 0),
        "cloth_ivory": ((.70, .64, .45), .84, 0),
        "cloth_green": ((.14, .26, .14), .86, 0),
        "cloth_green_dark": ((.060, .115, .068), .83, 0),
        "leather": ((.21, .088, .037), .71, 0),
        "leather_dark": ((.085, .038, .021), .74, 0),
        "metal_gold": ((.63, .38, .10), .39, .65),
        "potion_red": ((.42, .023, .018), .27, .05),
        "glass": ((.40, .035, .025), .24, .05),
    }
    result = {}
    for key, (rgb, roughness, metallic) in palette.items():
        material = bpy.data.materials.new("M_Arin_" + key)
        material.diffuse_color = (*rgb, 1)
        material.use_nodes = True
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*rgb, 1)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
        if key == "skin":
            bsdf.inputs["Subsurface Weight"].default_value = .035
        result[key] = material
    return result


def create_rig():
    data = bpy.data.armatures.new("SKEL_Arin")
    rig = bpy.data.objects.new("Arin_Rig", data)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    defs = [
        ("root", (0, 0, 0), (0, 0, .12), None),
        ("pelvis", (0, 0, .94), (0, 0, 1.02), "root"),
        ("spine_01", (0, 0, 1.02), (0, 0, 1.13), "pelvis"),
        ("spine_02", (0, 0, 1.13), (0, 0, 1.24), "spine_01"),
        ("spine_03", (0, 0, 1.24), (0, 0, 1.37), "spine_02"),
        ("neck", (0, 0, 1.37), (0, 0, 1.445), "spine_03"),
        ("head", (0, 0, 1.445), (0, 0, 1.68), "neck"),
    ]
    for side, s in (("l", 1), ("r", -1)):
        defs += [
            ("clavicle_"+side, (s*.02, 0, 1.32), (s*.16, 0, 1.32), "spine_03"),
            ("upperarm_"+side, (s*.16, 0, 1.32), (s*.43, 0, 1.32), "clavicle_"+side),
            ("lowerarm_"+side, (s*.43, 0, 1.32), (s*.65, 0, 1.32), "upperarm_"+side),
            ("hand_"+side, (s*.65, 0, 1.32), (s*.79, 0, 1.32), "lowerarm_"+side),
            ("thigh_"+side, (s*.085, 0, .97), (s*.085, 0, .55), "pelvis"),
            ("calf_"+side, (s*.085, 0, .55), (s*.085, 0, .09), "thigh_"+side),
            ("foot_"+side, (s*.085, 0, .09), (s*.085, -.11, .05), "calf_"+side),
            ("ball_"+side, (s*.085, -.11, .05), (s*.085, -.17, .05), "foot_"+side),
        ]
    for name, head, tail, parent in defs:
        bone = data.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        if parent:
            bone.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.show_in_front = True
    data.display_type = "STICK"
    rig["arin_model"] = "volumetric procedural stylized humanoid"
    return rig


def blend_axis(value, stops):
    if value <= stops[0][0]:
        return {stops[0][1]: 1.0}
    for (a, bone_a), (b, bone_b) in zip(stops, stops[1:]):
        if value <= b:
            if bone_a == bone_b:
                return {bone_a: 1.0}
            t = (value - a) / (b - a)
            return {bone_a: 1-t, bone_b: t}
    return {stops[-1][1]: 1.0}


def bind(objects, rig):
    for obj in objects:
        groups = {b.name: obj.vertex_groups.new(name=b.name) for b in rig.data.bones}
        region, rigid = obj.get("arin_region"), obj.get("arin_bone")
        for vertex in obj.data.vertices:
            p = obj.matrix_world @ vertex.co
            if rigid:
                weights = {rigid: 1}
            elif region in ("arm_l", "arm_r"):
                side = region[-1]
                weights = blend_axis(abs(p.x), [(.17, "clavicle_"+side), (.27, "upperarm_"+side),
                    (.40, "upperarm_"+side), (.46, "lowerarm_"+side), (.61, "lowerarm_"+side), (.67,"hand_"+side)])
            elif region in ("leg_l", "leg_r"):
                side = region[-1]
                weights = blend_axis(p.z, [(.13,"foot_"+side), (.23,"calf_"+side), (.51,"calf_"+side),
                    (.60,"thigh_"+side), (.91,"thigh_"+side), (1.015,"pelvis")])
            elif region in ("hand_l", "hand_r"):
                weights = {region:1}
            elif region == "neck":
                weights = blend_axis(p.z, [(1.36,"spine_03"),(1.40,"neck"),(1.455,"head")])
            else:
                weights = blend_axis(p.z, [(.98,"pelvis"),(1.075,"spine_01"),
                    (1.185,"spine_02"),(1.29,"spine_03")])
            for bone, weight in weights.items():
                if weight > .000001:
                    groups[bone].add([vertex.index], weight, "REPLACE")
            if abs(sum(weights.values())-1)>1e-6:
                raise RuntimeError("Unnormalized source weights: "+obj.name)
        mod = obj.modifiers.new("Arin Skin", "ARMATURE")
        mod.object = rig
        mod.use_deform_preserve_volume = True
        obj.parent = rig


def world_rotation(rig, bone, axis, angle):
    pose = rig.pose.bones[bone]
    pose.rotation_mode = "QUATERNION"
    local_axis = rig.data.bones[bone].matrix_local.to_quaternion().inverted() @ Vector(axis)
    pose.rotation_quaternion = Quaternion(local_axis, math.radians(angle))


def idle(rig):
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.frame_start, scene.frame_end = 1, 61
    rig.animation_data_create()
    rig.animation_data.action = bpy.data.actions.new("Arin_Idle")
    for frame in range(1,62):
        scene.frame_set(frame)
        wave = math.sin((frame-1)*math.tau/60)
        for pose in rig.pose.bones:
            pose.rotation_mode = "QUATERNION"
            pose.rotation_quaternion = (1,0,0,0)
            pose.location = (0,0,0)
        for side,s in (("l",1),("r",-1)):
            world_rotation(rig, "upperarm_"+side, (0,1,0), s*(68+.5*wave))
            world_rotation(rig, "lowerarm_"+side, (0,1,0), s*(7+.3*wave))
            world_rotation(rig, "hand_"+side, (0,0,1), s*4)
        world_rotation(rig,"spine_02",(1,0,0), .32*wave)
        world_rotation(rig,"spine_03",(1,0,0), -.18*wave)
        world_rotation(rig,"head",(0,0,1), .8*wave)
        for pose in rig.pose.bones:
            pose.keyframe_insert("rotation_quaternion", frame=frame, group=pose.name)
            pose.keyframe_insert("location", frame=frame, group=pose.name)
    scene.frame_set(1)
    bpy.context.view_layer.update()


def studio():
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = 900, 1100
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.world.color = (.18,.18,.18)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.16,.18,.19,1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    scene.view_settings.view_transform = "AgX"
    for name,location,power,size in [("Key",(-2,-3,4),430,3),("Fill",(2,-2,2.5),220,3),("Rim",(1,2,3),420,2)]:
        data = bpy.data.lights.new(name,"AREA")
        data.energy, data.shape, data.size = power,"DISK",size
        obj = bpy.data.objects.new(name,data)
        bpy.context.collection.objects.link(obj)
        obj.location = location
        obj.rotation_euler = (Vector((0,0,1))-obj.location).to_track_quat("-Z","Y").to_euler()
    floor_mat = bpy.data.materials.new("Studio Ground")
    floor_mat.diffuse_color = (.10,.12,.13,1)
    bpy.ops.mesh.primitive_plane_add(size=200)
    ground = bpy.context.object
    ground.name="Studio Ground"
    ground.data.materials.append(floor_mat)
    data = bpy.data.cameras.new("Arin Camera")
    camera = bpy.data.objects.new("Arin Camera",data)
    bpy.context.collection.objects.link(camera)
    data.type, data.ortho_scale = "ORTHO",1.95
    scene.camera=camera
    return camera


def render(camera, name, position, target=(0,0,.87), scale=1.95):
    camera.location = position
    camera.rotation_euler = (Vector(target)-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=scale
    bpy.context.scene.render.filepath = str(EVIDENCE/(name+".png"))
    bpy.ops.render.render(write_still=True)


def export(objects, rig):
    common = dict(use_selection=True, add_leaf_bones=False, axis_forward="-Z", axis_up="Y",
                  apply_unit_scale=True, bake_space_transform=False, path_mode="COPY", embed_textures=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects+[rig]:
        obj.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(OUT/"Arin.fbx"), object_types={"MESH","ARMATURE"}, bake_anim=True,
        bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_step=1, bake_anim_simplify_factor=0, **common)
    bpy.ops.export_scene.fbx(filepath=str(OUT/"SK_Arin.fbx"), object_types={"MESH","ARMATURE"}, bake_anim=False, **common)
    for obj in objects:
        obj.select_set(False)
    bpy.ops.export_scene.fbx(filepath=str(OUT/"A_Arin_Idle.fbx"), object_types={"ARMATURE"}, bake_anim=True,
        bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_step=1, bake_anim_simplify_factor=0, **common)
    for obj in objects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(OUT/"Arin.glb"), export_format="GLB", use_selection=True,
        export_animations=True, export_frame_range=True, export_force_sampling=True)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    # Own instance only. Preserve preferences and the running MCP addon.
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj,do_unlink=True)
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)
    for container in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for data in list(container):
            if data.users == 0:
                container.remove(data)
    for collection in list(bpy.data.collections):
        if collection.name.startswith("Arin Editable Parts"):
            bpy.data.collections.remove(collection)
    scene=bpy.context.scene
    scene.unit_settings.system="METRIC"
    scene.unit_settings.scale_length=1
    model_collection=bpy.data.collections.new("Arin Editable Parts")
    scene.collection.children.link(model_collection)
    mats=materials()
    objects=ArinBody.build_body(mats,model_collection)+ArinHead.build_head(mats,model_collection)
    triangle_count=sum(sum(len(p.vertices)-2 for p in obj.data.polygons) for obj in objects)
    if triangle_count>55000:
        raise RuntimeError("Model exceeds scoped triangle budget")
    rig=create_rig()
    bind(objects,rig)
    idle(rig)
    export(objects,rig)
    camera=studio()
    for name,pos in [("arin-front",(0,-4,.95)),("arin-side",(4,0,.95)),("arin-oblique",(2.6,-4,1.45)),("arin-back",(0,4,.95))]:
        render(camera,name,pos)
    render(camera,"arin-face",(.35,-2,1.60),(0,0,1.555),.38)
    # Save editable meshes in the same scene as the rig and studio.
    camera.location=(2.6,-4,1.45)
    camera.rotation_euler=(Vector((0,0,.87))-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=1.95
    bpy.context.scene.frame_set(1)
    for area in bpy.context.screen.areas:
        if area.type=="VIEW_3D":
            area.spaces.active.region_3d.view_perspective="CAMERA"
            area.spaces.active.shading.type="MATERIAL"
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"Arin.blend"))
    report={"status":"exported_requires_roundtrip_and_visual_review", "blender_version":bpy.app.version_string,
        "mesh_parts":len(objects),"triangles":triangle_count,"bones":len(rig.data.bones),
        "idle":{"action":"Arin_Idle","frames":[1,61],"fps":30,"seconds":2},
        "geometry":"volumetric modeled parts; no reference image projection", "files":{}}
    for path in OUT.iterdir():
        if path.suffix in (".fbx",".glb",".blend"):
            report["files"][path.name]={"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
    (EVIDENCE/"model-build.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    return report


if __name__=="__main__":
    result=main()
