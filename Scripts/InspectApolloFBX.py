"""Inspect an exported FBX in Blender and render its actual front and side.

Run with Blender, not ordinary Python::

    blender.exe --background --python Scripts/InspectApolloFBX.py -- \
        --input path/to/model.fbx --out-dir Feature/doc/evidence/apollo-fbx

This script imports only the supplied FBX. It does not create a rig, alter the
model, export replacement meshes, or touch Unreal assets. A technical import
pass does not mean that the model passes Apollo's upstream quality checks.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import traceback

import bpy
from mathutils import Vector


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--resolution", type=int, default=768)
    parser.add_argument("--no-render", action="store_true")
    parser.add_argument("--geometry-preview", action="store_true",
                        help="Use a labeled clay display override to inspect untextured geometry")
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parsed = parser.parse_args(arguments)
    parsed.input = parsed.input.resolve()
    parsed.out_dir = parsed.out_dir.resolve()
    workspace = Path(__file__).resolve().parents[1]
    if not parsed.out_dir.is_relative_to(workspace):
        parser.error("--out-dir must stay within the Rogue10m workspace")
    if not parsed.input.is_file() or parsed.input.suffix.lower() != ".fbx":
        parser.error("--input must be an existing .fbx file")
    if not 128 <= parsed.resolution <= 2048:
        parser.error("--resolution must be between 128 and 2048")
    return parsed


def rounded_vector(vector: Vector) -> list[float]:
    return [round(float(value), 7) for value in vector]


def inspect_mesh(obj: bpy.types.Object) -> dict:
    mesh = obj.data
    mesh.calc_loop_triangles()
    modifiers = [modifier for modifier in obj.modifiers if modifier.type == "ARMATURE"]
    bone_names = {
        bone.name
        for modifier in modifiers
        if modifier.object is not None and modifier.object.type == "ARMATURE"
        for bone in modifier.object.data.bones
    }
    bone_group_ids = {group.index for group in obj.vertex_groups if group.name in bone_names}
    weighted_vertices = sum(
        any(group.group in bone_group_ids and group.weight > 0.0 for group in vertex.groups)
        for vertex in mesh.vertices
    )
    return {
        "name": obj.name,
        "vertices": len(mesh.vertices),
        "polygons": len(mesh.polygons),
        "triangles": len(mesh.loop_triangles),
        "uv_layers": [layer.name for layer in mesh.uv_layers],
        "material_slots": [slot.material.name if slot.material else None for slot in obj.material_slots],
        "dimensions_blender_units": rounded_vector(obj.dimensions),
        "location": rounded_vector(obj.location),
        "rotation_euler": rounded_vector(obj.rotation_euler),
        "scale": rounded_vector(obj.scale),
        "armature_modifier_targets": [modifier.object.name if modifier.object else None for modifier in modifiers],
        "vertex_groups": [group.name for group in obj.vertex_groups],
        "vertices_weighted_to_existing_bones": weighted_vertices,
        "weighted_vertex_fraction": round(weighted_vertices / len(mesh.vertices), 6) if mesh.vertices else 0.0,
        "hidden_for_render": bool(obj.hide_render),
    }


def inspect_material(material: bpy.types.Material) -> dict:
    image_nodes = []
    if material.use_nodes and material.node_tree:
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE":
                image = node.image
                image_nodes.append({
                    "node": node.name,
                    "image": image.name if image else None,
                    "size": list(image.size) if image else None,
                    "packed": bool(image and image.packed_file),
                    "filepath": image.filepath if image else None,
                    "external_file_exists": bool(image and Path(bpy.path.abspath(image.filepath)).is_file()),
                })
    return {"name": material.name, "use_nodes": bool(material.use_nodes), "image_nodes": image_nodes}


def world_bounds(mesh_objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for obj in mesh_objects for corner in obj.bound_box]
    if not points or not all(math.isfinite(value) for point in points for value in point):
        raise ValueError("The FBX contains no finite mesh bounds")
    minimum = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
    if max(maximum - minimum) <= 0.0:
        raise ValueError("The FBX mesh bounds have zero extent")
    return minimum, maximum


def add_area_light(name: str, location: Vector, center: Vector, energy: float, size: float) -> None:
    data = bpy.data.lights.new(name, type="AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (center - location).to_track_quat("-Z", "Y").to_euler()


def render_views(args: argparse.Namespace, minimum: Vector, maximum: Vector) -> list[dict]:
    scene = bpy.context.scene
    center = (minimum + maximum) / 2.0
    dimensions = maximum - minimum
    extent = max(dimensions)
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = args.resolution
    scene.render.resolution_y = args.resolution
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.use_file_extension = True
    scene.view_settings.view_transform = "Standard"
    geometry_preview = getattr(args, "geometry_preview", False)
    if geometry_preview:
        scene.render.engine = "BLENDER_WORKBENCH"
        shading = scene.display.shading
        shading.light = "STUDIO"
        shading.studio_light = "paint.sl"
        shading.color_type = "SINGLE"
        shading.single_color = (0.32, 0.38, 0.45)
        shading.show_shadows = True
        shading.show_cavity = True
        shading.cavity_type = "BOTH"
    world = bpy.data.worlds.new("InspectionWhiteWorld")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.65
    add_area_light("InspectionFrontKey", center + Vector((-1.4, -2.3, 2.0)) * extent,
                   center, 700.0 * extent * extent, extent * 1.8)
    add_area_light("InspectionSideFill", center + Vector((2.3, 0.8, 1.0)) * extent,
                   center, 500.0 * extent * extent, extent * 1.8)
    camera_data = bpy.data.cameras.new("InspectionCamera")
    camera = bpy.data.objects.new("InspectionCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = extent * 1.18
    camera_data.clip_start = max(extent * 0.001, 0.000001)
    camera_data.clip_end = extent * 20.0
    views = [
        ("front", Vector((0.0, -1.0, 0.0)), "-Y"),
        ("side", Vector((1.0, 0.0, 0.0)), "+X"),
        ("oblique", Vector((1.0, -1.6, 0.35)).normalized(), "oblique"),
    ]
    rendered = []
    for label, direction, axis in views:
        camera.location = center + direction * extent * 3.0
        camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
        path = args.out_dir / f"fbx-{label}.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Blender did not write {label} render")
        rendered.append({"view": label, "camera_axis": axis, "projection": "orthographic", "path": str(path),
                         "geometry_only_display_override": geometry_preview})
    return rendered


def main() -> None:
    args = parse_arguments()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report = {
        "status": "running",
        "source_fbx": str(args.input),
        "source_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "source_size_bytes": args.input.stat().st_size,
        "blender_version": bpy.app.version_string,
        "scope": "actual FBX re-import and renders; no geometry repair, rigging, or Unreal modification",
        "axes": {"up": "+Z", "front_camera": "-Y", "side_camera": "+X"},
        "quality_acceptance": "not evaluated; preserve Apollo asset state and upstream QA separately",
    }
    report_path = args.out_dir / "fbx-inspection.json"
    try:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        result = bpy.ops.import_scene.fbx(filepath=str(args.input))
        if "FINISHED" not in result:
            raise RuntimeError(f"FBX importer returned {result}")
        bpy.context.view_layer.update()
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
        minimum, maximum = world_bounds(meshes)
        dimensions = maximum - minimum
        report.update({
            "meshes": [inspect_mesh(obj) for obj in meshes],
            "mesh_count": len(meshes),
            "total_triangles": sum(len(obj.data.loop_triangles) for obj in meshes),
            "materials": [inspect_material(material) for material in bpy.data.materials],
            "world_bounds_min": rounded_vector(minimum),
            "world_bounds_max": rounded_vector(maximum),
            "world_dimensions_blender_units": rounded_vector(dimensions),
            "scene_unit_scale_length": bpy.context.scene.unit_settings.scale_length,
            "front_depth_to_height_ratio": round(dimensions.y / dimensions.z, 7) if dimensions.z > 0 else None,
            "armatures": [{
                "name": obj.name,
                "bone_count": len(obj.data.bones),
                "bones": [bone.name for bone in obj.data.bones],
            } for obj in armatures],
            "armature_count": len(armatures),
            "actions": [{
                "name": action.name,
                "frame_range": [float(value) for value in action.frame_range],
            } for action in bpy.data.actions],
            "action_count": len(bpy.data.actions),
        })
        warnings = []
        if not armatures:
            warnings.append("No armature: this FBX is not a rigged skeletal NPC")
        if not bpy.data.actions:
            warnings.append("No imported animation actions")
        if dimensions.z > 0 and dimensions.y / dimensions.z < 0.15:
            warnings.append("Depth is less than 15% of height; inspect the side render for a flat or extruded 2.5D result")
        if any(not mesh["uv_layers"] for mesh in report["meshes"]):
            warnings.append("One or more meshes have no UV layer")
        report["warnings"] = warnings
        if report["total_triangles"] <= 0:
            raise ValueError("The FBX contains no mesh triangles")
        # Save the FBX-only metrics before adding inspection cameras and lights.
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        report["renders"] = [] if args.no_render else render_views(args, minimum, maximum)
        report["status"] = "passed_reimport"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        raise
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("APOLLO_FBX_INSPECTION " + json.dumps({
        "status": report["status"],
        "mesh_count": report["mesh_count"],
        "triangles": report["total_triangles"],
        "armatures": report["armature_count"],
        "actions": report["action_count"],
        "report": str(report_path),
    }))


if __name__ == "__main__":
    main()
