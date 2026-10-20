"""Bake actual meter-based surface shaders into four portable UV/PBR atlases.

No reference image is projected onto geometry. Original independent editable
parts remain in a source-only blend; this module joins their already-bound
weights for the runtime mesh and creates real tangent-normal textures.
"""
from pathlib import Path
import hashlib
import json
import math

import bpy

GROUPS = {
    "Face": (2048, {"skin", "lip", "eye_white", "iris", "pupil"}),
    "Hair": (2048, {"hair", "hair_light"}),
    "Cloth": (4096, {"cloth_ivory", "cloth_green", "cloth_green_dark"}),
    "Gear": (2048, {"leather", "leather_dark", "metal_gold", "potion_red", "glass"}),
}
CHANNELS = ("BaseColor", "Roughness", "Metallic", "Normal")


def node(tree, kind, **values):
    item = tree.nodes.new(kind)
    for key, value in values.items():
        setattr(item, key, value)
    return item


def texture_sources(materials):
    """Subtle material-frequency texture in real physical surface coordinates."""
    sources = {}
    for key, material in materials.items():
        tree = material.node_tree
        bsdf = tree.nodes.get("Principled BSDF")
        color = tuple(bsdf.inputs["Base Color"].default_value)
        roughness = float(bsdf.inputs["Roughness"].default_value)
        metallic = float(bsdf.inputs["Metallic"].default_value)
        geometry = node(tree, "ShaderNodeNewGeometry")
        coarse = node(tree, "ShaderNodeTexNoise")
        coarse.inputs["Scale"].default_value = 48 if key.startswith("cloth") else 95
        coarse.inputs["Detail"].default_value = 3
        tree.links.new(geometry.outputs["Position"], coarse.inputs["Vector"])
        ramp = node(tree, "ShaderNodeValToRGB")
        variation = .085 if key.startswith("cloth") or "leather" in key else .035
        for index, amount in ((0, 1-variation), (1, 1+variation)):
            ramp.color_ramp.elements[index].color = tuple(min(1, value*amount) for value in color[:3])+(1,)
        tree.links.new(coarse.outputs["Fac"], ramp.inputs["Fac"])
        color_output = ramp.outputs["Color"]
        if key=="skin":
            tint = node(tree, "ShaderNodeVertexColor", layer_name="ArinSkinTint")
            multiply = node(tree, "ShaderNodeMixRGB", blend_type="MULTIPLY")
            multiply.inputs[0].default_value = 1
            tree.links.new(color_output,multiply.inputs[1])
            tree.links.new(tint.outputs["Color"],multiply.inputs[2])
            color_output=multiply.outputs["Color"]
        tree.links.new(color_output, bsdf.inputs["Base Color"])
        rough = node(tree, "ShaderNodeMath", operation="MULTIPLY_ADD")
        rough.inputs[1].default_value = .09
        rough.inputs[2].default_value = max(.03, roughness-.045)
        tree.links.new(coarse.outputs["Fac"], rough.inputs[0])
        tree.links.new(rough.outputs[0], bsdf.inputs["Roughness"])
        metal = node(tree, "ShaderNodeValue")
        metal.outputs[0].default_value = metallic
        tree.links.new(metal.outputs[0], bsdf.inputs["Metallic"])
        fine = node(tree, "ShaderNodeTexNoise")
        fine.inputs["Scale"].default_value = 1450 if key=="skin" else 800
        fine.inputs["Detail"].default_value = 2
        fine.inputs["Roughness"].default_value = .65
        if key in {"hair","hair_light"}:
            strands=node(tree,"ShaderNodeVectorMath",operation="MULTIPLY")
            strands.inputs[1].default_value=(12,12,.75)
            tree.links.new(geometry.outputs["Position"],strands.inputs[0])
            tree.links.new(strands.outputs["Vector"],fine.inputs["Vector"])
            fine.inputs["Scale"].default_value=120
        else:
            tree.links.new(geometry.outputs["Position"], fine.inputs["Vector"])
        height = fine.outputs["Fac"]
        if key.startswith("cloth"):
            weave = []
            for direction in ("X", "Z"):
                wave = node(tree, "ShaderNodeTexWave", wave_type="BANDS", bands_direction=direction)
                wave.inputs["Scale"].default_value = 360
                wave.inputs["Distortion"].default_value = .4
                wave.inputs["Detail Scale"].default_value = 2
                tree.links.new(geometry.outputs["Position"], wave.inputs["Vector"])
                weave.append(wave.outputs["Fac"])
            multiply = node(tree, "ShaderNodeMath", operation="MULTIPLY")
            tree.links.new(weave[0], multiply.inputs[0])
            tree.links.new(weave[1], multiply.inputs[1])
            height = multiply.outputs[0]
        bump = node(tree, "ShaderNodeBump")
        bump.inputs["Strength"].default_value = .23 if key.startswith("cloth") else .18
        bump.inputs["Distance"].default_value = .00045 if "leather" in key else .00020
        if key in {"eye_white", "iris", "pupil", "glass", "potion_red", "metal_gold"}:
            bump.inputs["Strength"].default_value = .025
        tree.links.new(height, bump.inputs["Height"])
        tree.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        sources[key] = {"BaseColor": color_output, "Roughness": rough.outputs[0],
                        "Metallic": metal.outputs[0], "bsdf": bsdf}
    return sources


def join_group(name, parts, rig, resolution):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = "Arin_" + name
    # Join retains the active armature modifier and merges groups by bone name.
    modifiers = [mod for mod in obj.modifiers if mod.type=="ARMATURE"]
    if len(modifiers)!=1 or modifiers[0].object!=rig:
        raise RuntimeError("Atlas join did not retain the shared armature: "+name)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=20/resolution,
                             margin_method="FRACTION",
                             area_weight=.7, correct_aspect=True, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.data.uv_layers.active.name = "UV_ArinAtlas"
    obj["arin_atlas"] = name
    return obj


def bake_channel(obj, name, channel, resolution, sources, out):
    image = bpy.data.images.new("T_Arin_"+name+"_"+channel, width=resolution, height=resolution, alpha=False)
    image.colorspace_settings.name = "sRGB" if channel=="BaseColor" else "Non-Color"
    for slot in obj.material_slots:
        material = slot.material
        key = material.name.removeprefix("M_Arin_")
        tree = material.node_tree
        target = node(tree, "ShaderNodeTexImage", image=image)
        tree.nodes.active = target
        output = next(item for item in tree.nodes if item.type=="OUTPUT_MATERIAL")
        if channel=="Normal":
            tree.links.new(sources[key]["bsdf"].outputs["BSDF"], output.inputs["Surface"])
        else:
            emission = next((item for item in tree.nodes if item.name=="Arin Bake Emission"), None)
            if emission is None:
                emission = node(tree, "ShaderNodeEmission", name="Arin Bake Emission")
            tree.links.new(sources[key][channel], emission.inputs["Color"])
            tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 1
    scene.render.bake.use_selected_to_active = False
    scene.render.bake.margin = 6
    scene.render.bake.normal_space = "TANGENT"
    scene.render.bake.normal_r, scene.render.bake.normal_g, scene.render.bake.normal_b = "POS_X", "POS_Y", "POS_Z"
    print("ARIN_BAKE "+name+" "+channel, flush=True)
    bpy.ops.object.bake(type="NORMAL" if channel=="Normal" else "EMIT", use_clear=True)
    image.filepath_raw = str(out/(image.name+".png"))
    image.file_format = "PNG"
    image.save()
    return image


def runtime_material(name, images):
    material = bpy.data.materials.new("M_Arin_"+name)
    material.use_nodes = True
    tree = material.node_tree
    bsdf = tree.nodes.get("Principled BSDF")
    if name=="Face":
        bsdf.inputs["Subsurface Weight"].default_value = .02
    for channel, image in images.items():
        sample = node(tree, "ShaderNodeTexImage", image=image, label=channel)
        if channel=="Normal":
            normal = node(tree, "ShaderNodeNormalMap")
            tree.links.new(sample.outputs["Color"], normal.inputs["Color"])
            tree.links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
        else:
            tree.links.new(sample.outputs["Color"], bsdf.inputs["Base Color" if channel=="BaseColor" else channel])
    return material


def build_atlases(objects, rig, materials, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    rig.data.pose_position = "REST"
    bpy.context.view_layer.update()
    for obj in objects:
        if "ArinSkinTint" not in obj.data.color_attributes:
            attribute=obj.data.color_attributes.new(name="ArinSkinTint",type="FLOAT_COLOR",domain="POINT")
            for item in attribute.data:
                item.color=(1,1,1,1)
    sources = texture_sources(materials)
    # Join invalidates removed Object RNA handles. Partition before the first
    # join; do not dereference the original list again after that mutation.
    partitions={name:[obj for obj in objects if obj.data.materials[0].name.removeprefix("M_Arin_") in keys]
                for name,(_,keys) in GROUPS.items()}
    groups, manifest = [], {"method":"unique smart-projected packed UV per atlas, actual Cycles shader/normal bake",
        "normal_convention":"OpenGL +Y tangent-space; Unreal texture flip_green_channel=True",
        "image_projection":False, "groups":{}, "files":{}}
    for name, (resolution, keys) in GROUPS.items():
        parts = partitions[name]
        if not parts:
            raise RuntimeError("Empty atlas group: "+name)
        obj = join_group(name, parts, rig, resolution)
        images = {channel:bake_channel(obj,name,channel,resolution,sources,output) for channel in CHANNELS}
        material = runtime_material(name, images)
        obj.data.materials.clear()
        obj.data.materials.append(material)
        for polygon in obj.data.polygons:
            polygon.material_index = 0
        groups.append(obj)
        manifest["groups"][name] = {"resolution":[resolution,resolution], "mesh":obj.name,
            "material":material.name,"channels":{key:Path(image.filepath_raw).name for key,image in images.items()}}
        for image in images.values():
            path = Path(image.filepath_raw)
            manifest["files"][path.name] = {"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
    rig.data.pose_position = "POSE"
    bpy.context.view_layer.update()
    (output.parent/"surface-manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    return groups, manifest
