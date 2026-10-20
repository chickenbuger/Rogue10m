"""Roundtrip the actual Arin FBX and inspect its rig, Idle and rendered shape.

Run in the owned Blender instance, through Blender MCP or Blender --python.
Only inspection cameras/lights are added after import; geometry, weights and
animation are never repaired. The active scene is replaced by the FBX import.
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


ROOT = Path(__file__).resolve().parents[2]
REQUIRED_BONES = {
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "neck", "head",
    *{part + "_" + side for side in ("l", "r") for part in (
        "clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball")},
}
HEIGHT_M = 1.7
HEIGHT_TOLERANCE_M = 0.002
WEIGHT_TOLERANCE = 0.001
ROOT_TOLERANCE = 0.00001
LOOP_TOLERANCE = 0.0001
MOTION_MIN_M = 0.00001


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "Art/NPC/Arin/Arin.fbx")
    parser.add_argument("--out-dir", type=Path,
                        default=ROOT / "Feature/doc/evidence/arin-blender-20261007/roundtrip")
    parser.add_argument("--resolution", type=int, default=1024)
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = parser.parse_args(arguments)
    args.input = args.input.resolve()
    args.out_dir = args.out_dir.resolve()
    if not args.input.is_relative_to(ROOT) or not args.out_dir.is_relative_to(ROOT):
        parser.error("Input and inspection output must stay within the Rogue10m workspace")
    if args.input.suffix.lower() != ".fbx":
        parser.error("Input must be an FBX file")
    if not 256 <= args.resolution <= 2048:
        parser.error("Resolution must be between 256 and 2048")
    return args


def vector_values(vector) -> list[float]:
    return [round(float(value), 9) for value in vector]


def matrix_values(matrix) -> list[list[float]]:
    values = [[float(value) for value in row] for row in matrix]
    if any(not math.isfinite(value) for row in values for value in row):
        raise ValueError("Imported animation produced a nonfinite bone/object matrix")
    return values


def matrix_delta(first, second) -> float:
    return max(abs(first[row][column] - second[row][column])
               for row in range(4) for column in range(4))


def action_curves(action: bpy.types.Action) -> list:
    """Read legacy and Blender 4.4+ layered action curves without modifying them."""
    curves = []
    seen = set()
    for layer in getattr(action, "layers", ()):
        for strip in layer.strips:
            for channelbag in getattr(strip, "channelbags", ()):
                for curve in channelbag.fcurves:
                    identifier = curve.as_pointer()
                    if identifier not in seen:
                        seen.add(identifier)
                        curves.append(curve)
    # Legacy actions expose fcurves directly. Layered actions may reject this
    # access, which is why the layered paths are enumerated first.
    try:
        for curve in action.fcurves:
            identifier = curve.as_pointer()
            if identifier not in seen:
                seen.add(identifier)
                curves.append(curve)
    except (AttributeError, RuntimeError):
        pass
    return curves


def inspect_action(action: bpy.types.Action) -> dict:
    curves = action_curves(action)
    frames = sorted({round(float(point.co.x), 6)
                     for curve in curves for point in curve.keyframe_points})
    start, end = (float(value) for value in action.frame_range)
    return {
        "name": action.name,
        "frame_range": [start, end],
        "frame_interval": end - start,
        "inclusive_frame_count": int(round(end - start)) + 1,
        "curve_count": len(curves),
        "keyframe_count": sum(len(curve.keyframe_points) for curve in curves),
        "distinct_keyframe_count": len(frames),
        "distinct_keyframes": frames,
        "animated_paths": sorted({curve.data_path for curve in curves}),
    }


def inspect_mesh(obj: bpy.types.Object, expected_rig: bpy.types.Object | None) -> dict:
    mesh = obj.data
    mesh.calc_loop_triangles()
    modifiers = [modifier for modifier in obj.modifiers if modifier.type == "ARMATURE"]
    targets = [modifier.object for modifier in modifiers if modifier.object is not None]
    deform_bones = {
        bone.name for target in targets if target.type == "ARMATURE"
        for bone in target.data.bones if bone.use_deform
    }
    group_names = {group.index: group.name for group in obj.vertex_groups}
    bone_group_ids = {index for index, name in group_names.items() if name in deform_bones}
    sums = []
    missing, abnormal, invalid, unknown = [], [], [], []
    max_influences = 0
    for vertex in mesh.vertices:
        weights = [float(group.weight) for group in vertex.groups if group.group in bone_group_ids]
        positive = [weight for weight in weights if weight > 0]
        total = sum(positive)
        sums.append(total)
        max_influences = max(max_influences, len(positive))
        if not positive:
            missing.append(vertex.index)
        if abs(total - 1.0) > WEIGHT_TOLERANCE:
            abnormal.append(vertex.index)
        if any(not math.isfinite(weight) or weight < 0 or weight > 1.0 + WEIGHT_TOLERANCE
               for weight in weights):
            invalid.append(vertex.index)
        if any(group.weight > 0 and group.group not in bone_group_ids for group in vertex.groups):
            unknown.append(vertex.index)
    finite_vertices = all(math.isfinite(value) for vertex in mesh.vertices for value in vertex.co)
    return {
        "name": obj.name,
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.loop_triangles),
        "finite_vertices": finite_vertices,
        "armature_modifier_targets": [target.name for target in targets],
        "uses_expected_rig": expected_rig is not None and expected_rig in targets,
        "all_armature_modifiers_enabled": all(modifier.show_viewport and modifier.show_render
                                                for modifier in modifiers),
        "deform_bone_groups": sorted(name for name in group_names.values() if name in deform_bones),
        "weighted_vertices": len(mesh.vertices) - len(missing),
        "unweighted_vertices": len(missing),
        "unweighted_examples": missing[:12],
        "unnormalized_vertices": len(abnormal),
        "unnormalized_examples": abnormal[:12],
        "invalid_weight_vertices": len(invalid),
        "weights_outside_deform_bones_vertices": len(unknown),
        "weight_sum_min": min(sums) if sums else None,
        "weight_sum_max": max(sums) if sums else None,
        "max_bone_influences": max_influences,
        "material_slots": [slot.material.name if slot.material else None for slot in obj.material_slots],
        "uv_layers": [layer.name for layer in mesh.uv_layers],
        "hidden_for_render": bool(obj.hide_render),
    }


def evaluated_geometry(meshes: list[bpy.types.Object], samples_only=False) -> dict:
    graph = bpy.context.evaluated_depsgraph_get()
    coordinates = {}
    for obj in meshes:
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            vertices = mesh.vertices
            indices = range(len(vertices))
            if samples_only and vertices:
                step = max(1, len(vertices) // 64)
                indices = sorted(set(range(0, len(vertices), step)) | {len(vertices) - 1})
            points = [tuple(evaluated.matrix_world @ vertices[index].co) for index in indices]
            if any(not math.isfinite(value) for point in points for value in point):
                raise ValueError("Imported animation produced nonfinite mesh vertices: " + obj.name)
            coordinates[obj.name] = points
        finally:
            evaluated.to_mesh_clear()
    return coordinates


def geometry_bounds(coordinates: dict) -> tuple[Vector, Vector]:
    points = [point for values in coordinates.values() for point in values]
    if not points or any(not math.isfinite(value) for point in points for value in point):
        raise ValueError("Imported mesh has missing or nonfinite evaluated vertices")
    minimum = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
    return minimum, maximum


def geometry_delta(first: dict, second: dict) -> float:
    if first.keys() != second.keys():
        raise ValueError("Evaluated mesh objects changed during Idle")
    maximum = 0.0
    for name, values in first.items():
        if len(values) != len(second[name]):
            raise ValueError("Evaluated vertex count changed during Idle: " + name)
        for before, after in zip(values, second[name]):
            maximum = max(maximum, (Vector(before) - Vector(after)).length)
    return maximum


def inspect_motion(rig: bpy.types.Object, meshes: list[bpy.types.Object]) -> dict:
    scene = bpy.context.scene
    scene.frame_set(1)
    bpy.context.view_layer.update()
    reference_armature = matrix_values(rig.matrix_world)
    reference_bones = {bone.name: matrix_values(rig.matrix_world @ bone.matrix)
                       for bone in rig.pose.bones}
    reference_geometry = evaluated_geometry(meshes, samples_only=True)
    bone_motion = {name: 0.0 for name in reference_bones}
    root_delta, armature_delta, geometry_motion = 0.0, 0.0, 0.0
    end_bone_delta, end_geometry_delta = 0.0, 0.0
    frames = []
    for frame in range(1, 62):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        armature_delta = max(armature_delta, matrix_delta(
            reference_armature, matrix_values(rig.matrix_world)))
        largest_bone_delta = 0.0
        for bone in rig.pose.bones:
            current = matrix_values(rig.matrix_world @ bone.matrix)
            delta = matrix_delta(reference_bones[bone.name], current)
            bone_motion[bone.name] = max(bone_motion[bone.name], delta)
            largest_bone_delta = max(largest_bone_delta, delta)
            if bone.name == "root":
                root_delta = max(root_delta, delta)
        geometry = evaluated_geometry(meshes, samples_only=True)
        mesh_delta = geometry_delta(reference_geometry, geometry)
        geometry_motion = max(geometry_motion, mesh_delta)
        frames.append({"frame": frame, "bone_matrix_delta_max": largest_bone_delta,
                       "sampled_vertex_displacement_m": mesh_delta})
        if frame == 61:
            end_bone_delta = largest_bone_delta
            end_geometry_delta = mesh_delta
    scene.frame_set(1)
    bpy.context.view_layer.update()
    return {
        "sampled_frames": frames,
        "bone_matrix_delta_by_name": bone_motion,
        "moving_nonroot_bones": [name for name, delta in bone_motion.items()
                                  if name != "root" and delta > ROOT_TOLERANCE],
        "root_world_matrix_delta_max": root_delta,
        "armature_world_matrix_delta_max": armature_delta,
        "mesh_sample_displacement_max_m": geometry_motion,
        "loop_endpoint_bone_matrix_delta_max": end_bone_delta,
        "loop_endpoint_vertex_displacement_max_m": end_geometry_delta,
    }


def add_light(name: str, location: Vector, center: Vector, energy: float, size: float) -> None:
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.shape, data.size = energy, "DISK", size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (center - location).to_track_quat("-Z", "Y").to_euler()


def render_views(args: argparse.Namespace, meshes: list[bpy.types.Object]) -> list[dict]:
    scene = bpy.context.scene
    scene.frame_set(1)
    minimum, maximum = geometry_bounds(evaluated_geometry(meshes))
    center, extent = (minimum + maximum) / 2, max(maximum - minimum)
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = args.resolution
    scene.render.resolution_y = args.resolution
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.use_file_extension = True
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    world = bpy.data.worlds.new("ArinRoundtripWorld")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (.16, .18, .19, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    add_light("RoundtripKey", center + Vector((-1.4, -2.4, 2.0)) * extent,
              center, 480 * extent ** 2, 1.5 * extent)
    add_light("RoundtripFill", center + Vector((2, -1, 1.2)) * extent,
              center, 230 * extent ** 2, 1.7 * extent)
    add_light("RoundtripRim", center + Vector((.3, 1.8, 1.5)) * extent,
              center, 300 * extent ** 2, extent)
    camera_data = bpy.data.cameras.new("ArinRoundtripCamera")
    camera = bpy.data.objects.new("ArinRoundtripCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type, camera_data.ortho_scale = "ORTHO", extent * 1.13
    camera_data.clip_start, camera_data.clip_end = .001, extent * 20
    renders = []
    for label, direction in (
        ("front", Vector((0, -1, 0))),
        ("side", Vector((1, 0, 0))),
        ("oblique", Vector((1, -1.7, .25)).normalized()),
    ):
        camera.location = center + direction * extent * 3
        camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
        path = args.out_dir / ("fbx-" + label + ".png")
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError("Actual roundtrip render missing: " + label)
        renders.append({"view": label, "path": str(path), "frame": 1,
                        "projection": "orthographic", "material_override": False})
    return renders


def main() -> dict:
    args = parse_arguments()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.out_dir / "validation.json"
    report = {
        "status": "running", "blender_version": bpy.app.version_string,
        "source_fbx": str(args.input),
        "scope": "actual combined FBX import; inspect original geometry, skinning and Idle",
        "axes": {"up": "+Z", "front": "-Y", "units": "metres"},
        "thresholds": {"height_m": HEIGHT_M, "height_tolerance_m": HEIGHT_TOLERANCE_M,
                       "weight_sum_tolerance": WEIGHT_TOLERANCE,
                       "root_matrix_tolerance": ROOT_TOLERANCE,
                       "loop_endpoint_tolerance": LOOP_TOLERANCE,
                       "mesh_motion_min_m": MOTION_MIN_M},
        "checks": {}, "renders": [],
    }

    def save_report():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    try:
        if not args.input.is_file():
            raise FileNotFoundError("Combined Arin FBX does not exist: " + str(args.input))
        report["source_sha256"] = hashlib.sha256(args.input.read_bytes()).hexdigest()
        report["source_size_bytes"] = args.input.stat().st_size
        # Keep the MCP addon/preferences alive; reset only the owned scene.
        for obj in list(bpy.data.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        for action in list(bpy.data.actions):
            bpy.data.actions.remove(action)
        scene = bpy.context.scene
        scene.unit_settings.system = "METRIC"
        scene.unit_settings.scale_length = 1.0
        imported = bpy.ops.import_scene.fbx(filepath=str(args.input), use_anim=True,
                                          use_custom_normals=True, anim_offset=1.0)
        if "FINISHED" not in imported:
            raise RuntimeError("FBX importer did not finish: " + str(imported))
        bpy.context.view_layer.update()
        meshes = [obj for obj in scene.objects if obj.type == "MESH"]
        rigs = [obj for obj in scene.objects if obj.type == "ARMATURE"]
        rig = rigs[0] if len(rigs) == 1 else None
        scene.frame_set(1)
        minimum, maximum = geometry_bounds(evaluated_geometry(meshes))
        dimensions = maximum - minimum
        report.update({
            "mesh_count": len(meshes), "armature_count": len(rigs),
            "bounds_min_m": vector_values(minimum), "bounds_max_m": vector_values(maximum),
            "dimensions_m": vector_values(dimensions),
            "height_m": float(dimensions.z),
            "fps": scene.render.fps / scene.render.fps_base,
            "meshes": [inspect_mesh(obj, rig) for obj in meshes],
            "armatures": [{"name": obj.name, "bone_count": len(obj.data.bones),
                           "bones": [bone.name for bone in obj.data.bones],
                           "deform_bones": [bone.name for bone in obj.data.bones if bone.use_deform],
                           "bone_parents": {bone.name: bone.parent.name if bone.parent else None
                                            for bone in obj.data.bones}}
                          for obj in rigs],
            "actions": [inspect_action(action) for action in bpy.data.actions],
        })
        report["total_vertices"] = sum(item["vertices"] for item in report["meshes"])
        report["total_triangles"] = sum(item["triangles"] for item in report["meshes"])
        checks = report["checks"]
        checks["actual_nonempty_mesh"] = bool(meshes) and report["total_triangles"] > 0
        checks["height_170cm"] = abs(dimensions.z - HEIGHT_M) <= HEIGHT_TOLERANCE_M
        checks["feet_at_ground"] = abs(minimum.z) <= HEIGHT_TOLERANCE_M
        checks["single_armature"] = len(rigs) == 1
        present_bones = set(rig.data.bones.keys()) if rig is not None else set()
        report["missing_required_bones"] = sorted(REQUIRED_BONES - present_bones)
        checks["required_humanoid_bones"] = not report["missing_required_bones"]
        checks["root_has_no_parent"] = rig is not None and "root" in rig.data.bones and rig.data.bones["root"].parent is None
        checks["finite_mesh_vertices"] = bool(meshes) and all(item["finite_vertices"] for item in report["meshes"])
        checks["every_mesh_bound_to_rig"] = bool(meshes) and all(
            item["uses_expected_rig"] and item["all_armature_modifiers_enabled"] for item in report["meshes"])
        checks["all_vertices_weighted"] = bool(meshes) and all(
            item["vertices"] > 0 and item["unweighted_vertices"] == 0 for item in report["meshes"])
        checks["normalized_deform_weights"] = bool(meshes) and all(
            item["unnormalized_vertices"] == 0 and item["invalid_weight_vertices"] == 0
            and item["weights_outside_deform_bones_vertices"] == 0 for item in report["meshes"])
        active_action = rig.animation_data.action if rig is not None and rig.animation_data else None
        report["active_armature_action"] = active_action.name if active_action else None
        idle = next((item for item in report["actions"] if active_action and item["name"] == active_action.name), None)
        checks["active_arin_idle"] = bool(idle and "Arin_Idle" in idle["name"])
        checks["idle_frames_1_to_61"] = bool(idle and abs(idle["frame_range"][0] - 1) <= .001
                                             and abs(idle["frame_range"][1] - 61) <= .001)
        checks["idle_30fps"] = abs(report["fps"] - 30) < .001
        if rig is not None and "root" in rig.pose.bones and active_action is not None:
            report["motion"] = inspect_motion(rig, meshes)
            motion = report["motion"]
            checks["idle_changes_actual_bones"] = bool(motion["moving_nonroot_bones"])
            checks["idle_deforms_actual_mesh"] = motion["mesh_sample_displacement_max_m"] > MOTION_MIN_M
            checks["stationary_root"] = motion["root_world_matrix_delta_max"] <= ROOT_TOLERANCE
            checks["stationary_armature"] = motion["armature_world_matrix_delta_max"] <= ROOT_TOLERANCE
            checks["idle_loops"] = (motion["loop_endpoint_bone_matrix_delta_max"] <= LOOP_TOLERANCE
                                    and motion["loop_endpoint_vertex_displacement_max_m"] <= LOOP_TOLERANCE)
        else:
            for key in ("idle_changes_actual_bones", "idle_deforms_actual_mesh", "stationary_root",
                        "stationary_armature", "idle_loops"):
                checks[key] = False
        report["failed_checks"] = [name for name, passed in checks.items() if not passed]
        save_report()
        # Preserve actual failure views too. Technical checks never replace the
        # separate visual review of anatomy, hands, outfit and deformation.
        report["renders"] = render_views(args, meshes)
        report["status"] = "passed_roundtrip" if not report["failed_checks"] else "failed_roundtrip"
        save_report()
        if report["failed_checks"]:
            raise ValueError("Arin FBX roundtrip failed: " + ", ".join(report["failed_checks"]))
    except Exception as error:
        report["status"] = "failed_roundtrip"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        save_report()
        raise
    summary = {"status": report["status"], "report": str(report_path),
               "height_m": report["height_m"], "mesh_count": report["mesh_count"],
               "triangles": report["total_triangles"], "armature_count": report["armature_count"],
               "active_armature_action": report["active_armature_action"],
               "failed_checks": report["failed_checks"], "renders": report["renders"]}
    print("ARIN_FBX_VALIDATION " + json.dumps(summary, ensure_ascii=False))
    return summary


if __name__ == "__main__":
    result = main()
