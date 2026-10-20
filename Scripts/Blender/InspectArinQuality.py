"""Read-only technical inspection for authored and freshly imported characters.

Run with Blender in a NEW process for --input, for example:
  blender --background --factory-startup --python InspectArinQuality.py -- \
    --input character.fbx --output metrics.json --prefix Arin \
    --frames auto --contract quality-contract.json

With --current-scene this script does not clear, import, save, or repair anything.
It temporarily samples existing animation frames and restores the original frame.
The report always requires a separate visual review: metrics do not prove good
anatomy, garment continuity, UV non-overlap, clipping, or deformation quality.
"""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Vector


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--input", type=Path)
    source.add_argument("--current-scene", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--prefix", default="")
    parser.add_argument("--objects-json", type=Path,
                        help="JSON array of exact mesh names; overrides --prefix")
    parser.add_argument("--frames", default="auto",
                        help="auto samples active action endpoints and quarter points; otherwise CSV frame numbers")
    parser.add_argument("--area-epsilon", type=float, default=1e-12)
    parser.add_argument("--edge-epsilon", type=float, default=1e-8)
    parser.add_argument("--weight-tolerance", type=float, default=.001)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    args.frames = None if args.frames == "auto" else sorted(set(float(frame) for frame in args.frames.split(",")))
    if args.frames is not None and not 1 <= len(args.frames) <= 12:
        parser.error("Specify between one and twelve distinct frames")
    if args.frames is not None and not all(math.isfinite(frame) for frame in args.frames):
        parser.error("Frame numbers must be finite")
    if args.area_epsilon <= 0 or args.edge_epsilon <= 0 or args.weight_tolerance <= 0:
        parser.error("Inspection tolerances must be positive")
    if args.input and not bpy.app.background:
        parser.error("--input is restricted to a fresh background Blender process; use --current-scene for live MCP")
    return args


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_asset(path):
    suffix = path.suffix.lower()
    if suffix == ".blend":
        bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
        return
    # A background command starts a separate process. Its default cube is not
    # an asset, so remove the factory scene's objects before the fresh import.
    for obj in list(bpy.context.scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    if suffix == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path), use_anim=True)
    elif suffix in {".glb", ".gltf"}:
        bpy.ops.import_scene.gltf(filepath=str(path))
    elif suffix == ".obj":
        bpy.ops.wm.obj_import(filepath=str(path))
    else:
        raise ValueError("Supported input formats: .blend, .fbx, .glb, .gltf, .obj")


def floats(values):
    return [float(value) for value in values]


def finite(values):
    return all(math.isfinite(float(value)) for value in values)


def flatten_matrix(matrix):
    return [float(value) for row in matrix for value in row]


def topology(mesh, matrix, args):
    mesh.calc_loop_triangles()
    adjacency = [[] for _ in mesh.vertices]
    uses = Counter()
    invalid_vertices = sum(not finite(vertex.co) for vertex in mesh.vertices)
    world = [matrix @ vertex.co for vertex in mesh.vertices]
    degenerate_edges = 0
    for edge in mesh.edges:
        a, b = edge.vertices
        adjacency[a].append(b)
        adjacency[b].append(a)
        if (world[a] - world[b]).length <= args.edge_epsilon:
            degenerate_edges += 1
    degenerate_triangles = 0
    for face in mesh.polygons:
        for edge in face.edge_keys:
            uses[tuple(sorted(edge))] += 1
    for triangle in mesh.loop_triangles:
        a, b, c = (world[index] for index in triangle.vertices)
        if (b - a).cross(c - a).length * .5 <= args.area_epsilon:
            degenerate_triangles += 1
    remaining = set(range(len(mesh.vertices)))
    components = []
    while remaining:
        stack, count = [remaining.pop()], 0
        while stack:
            vertex = stack.pop()
            count += 1
            for neighbour in adjacency[vertex]:
                if neighbour in remaining:
                    remaining.remove(neighbour)
                    stack.append(neighbour)
        components.append(count)
    faces = len(mesh.polygons)
    return {
        "vertices": len(mesh.vertices), "edges": len(mesh.edges),
        "polygons": faces, "triangles": len(mesh.loop_triangles),
        "quad_ratio": sum(len(face.vertices) == 4 for face in mesh.polygons) / max(1, faces),
        "triangle_polygon_ratio": sum(len(face.vertices) == 3 for face in mesh.polygons) / max(1, faces),
        "ngons": sum(len(face.vertices) > 4 for face in mesh.polygons),
        "smooth_polygon_ratio": sum(face.use_smooth for face in mesh.polygons) / max(1, faces),
        "invalid_vertices": invalid_vertices,
        "zero_length_edges": degenerate_edges,
        "zero_area_triangles": degenerate_triangles,
        "loose_vertices": sum(not neighbours for neighbours in adjacency),
        "loose_edges": sum(uses[tuple(sorted(edge.vertices))] == 0 for edge in mesh.edges),
        "boundary_edges": sum(uses[tuple(sorted(edge.vertices))] == 1 for edge in mesh.edges),
        "non_manifold_edges": sum(uses[tuple(sorted(edge.vertices))] > 2 for edge in mesh.edges),
        "connected_components": len(components),
        "component_vertex_counts": sorted(components, reverse=True),
    }


def uv_metrics(mesh):
    layers = []
    for layer in mesh.uv_layers:
        invalid = sum(not finite(loop.uv) for loop in layer.data)
        outside = sum(any(float(value) < -1e-6 or float(value) > 1 + 1e-6 for value in loop.uv)
                      for loop in layer.data)
        areas = []
        for face in mesh.polygons:
            points = [layer.data[index].uv for index in face.loop_indices]
            area = abs(sum(a.x * b.y - b.x * a.y for a, b in zip(points, points[1:] + points[:1]))) * .5
            areas.append(area)
        layers.append({
            "name": layer.name, "invalid_loops": invalid,
            "outside_0_1_loops": outside,
            "non_degenerate_face_fraction": sum(area > 1e-12 for area in areas) / max(1, len(areas)),
            "summed_polygon_area": sum(areas),
            "overlap_check": "not_performed; summed area is not unique atlas coverage",
        })
    return layers


def upstream_nodes(socket):
    found, queue = {}, [link.from_node for link in socket.links]
    while queue:
        node = queue.pop()
        if node.as_pointer() in found:
            continue
        found[node.as_pointer()] = node
        for node_input in node.inputs:
            queue.extend(link.from_node for link in node_input.links)
    return list(found.values())


def material_metrics(material):
    report = {"name": material.name, "use_nodes": bool(material.use_nodes), "images": [], "pbr_inputs": {}}
    if not material.use_nodes or not material.node_tree:
        return report
    images, missing_nodes = {}, []
    for node in material.node_tree.nodes:
        if node.type != "TEX_IMAGE":
            continue
        if node.image is None:
            missing_nodes.append(node.name)
            continue
        image = node.image
        path = Path(bpy.path.abspath(image.filepath, library=image.library)) if image.filepath else None
        images[image.name] = {
            "name": image.name, "source": image.source,
            "filepath": str(path) if path else None,
            "file_exists": bool(path and path.is_file()),
            "packed": bool(image.packed_file or getattr(image, "packed_files", ())),
            "dimensions": list(image.size), "colorspace": image.colorspace_settings.name,
        }
    report["images"] = list(images.values())
    report["empty_image_nodes"] = missing_nodes
    for node in material.node_tree.nodes:
        if node.type != "BSDF_PRINCIPLED":
            continue
        for name in ("Base Color", "Roughness", "Metallic", "Normal", "Alpha"):
            socket = node.inputs.get(name)
            if socket is None:
                continue
            nodes = upstream_nodes(socket)
            report["pbr_inputs"][name] = {
                "linked": bool(socket.is_linked),
                "images": sorted({item.image.name for item in nodes if item.type == "TEX_IMAGE" and item.image}),
                "procedural_nodes_requires_bake": sorted({item.type for item in nodes
                    if item.type in {"TEX_NOISE", "TEX_VORONOI", "TEX_WAVE", "TEX_MAGIC", "TEX_MUSGRAVE", "TEX_CHECKER", "TEX_GRADIENT"}}),
            }
    return report


def skin_metrics(obj, tolerance):
    targets = [modifier.object for modifier in obj.modifiers
               if modifier.type == "ARMATURE" and modifier.object]
    deform = {bone.name for rig in targets for bone in rig.data.bones if bone.use_deform}
    names = {group.index: group.name for group in obj.vertex_groups}
    totals, influences, unweighted, unnormalized, invalid, unknown = [], [], [], [], [], []
    for vertex in obj.data.vertices:
        weights = [float(group.weight) for group in vertex.groups if names.get(group.group) in deform]
        positive = [weight for weight in weights if weight > 1e-8]
        total = sum(positive)
        totals.append(total)
        influences.append(len(positive))
        if not positive:
            unweighted.append(vertex.index)
        if abs(total - 1) > tolerance:
            unnormalized.append(vertex.index)
        if any(not math.isfinite(weight) or weight < 0 or weight > 1 + tolerance for weight in weights):
            invalid.append(vertex.index)
        if any(group.weight > 1e-8 and names.get(group.group) not in deform for group in vertex.groups):
            unknown.append(vertex.index)
    return {
        "rigs": [rig.name for rig in targets],
        "unweighted_vertices": len(unweighted), "unweighted_examples": unweighted[:12],
        "unnormalized_vertices": len(unnormalized), "invalid_weight_vertices": len(invalid),
        "non_deform_group_vertices": len(unknown), "max_influences": max(influences, default=0),
        "weight_sum_min": min(totals, default=None), "weight_sum_max": max(totals, default=None),
    }


def action_curves(action):
    curves = {}
    for layer in getattr(action, "layers", ()):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", ()):
                for curve in bag.fcurves:
                    curves[curve.as_pointer()] = curve
    try:
        for curve in action.fcurves:
            curves[curve.as_pointer()] = curve
    except (AttributeError, RuntimeError):
        pass
    return list(curves.values())


def frame_geometry(objects):
    graph = bpy.context.evaluated_depsgraph_get()
    positions, hashes = {}, {}
    for obj in objects:
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            local = [vertex.co.copy() for vertex in mesh.vertices]
            positions[obj.name] = [evaluated.matrix_world @ point for point in local]
            digest = hashlib.sha256()
            for point in local:
                digest.update(struct.pack("<fff", *point))
            hashes[obj.name] = digest.hexdigest()
        finally:
            evaluated.to_mesh_clear()
    return positions, hashes


def max_displacement(first, second):
    if len(first) != len(second):
        return None
    return max(((a - b).length for a, b in zip(first, second)), default=0)


def inspect(args):
    scene = bpy.context.scene
    contract_document = json.loads(args.contract.read_text(encoding="utf-8-sig")) if args.contract else {}
    # The proposal includes a nested reusable numerical contract; callers may
    # also pass a direct contract object.
    contract = dict(contract_document.get("inspector_contract", contract_document))
    source_suffix = args.input.suffix.lower() if args.input else Path(bpy.data.filepath).suffix.lower()
    contract.update(contract_document.get("inspector_format_overrides", {}).get(source_suffix, {}))
    wanted = set(json.loads(args.objects_json.read_text(encoding="utf-8-sig"))) if args.objects_json else None
    objects = [obj for obj in scene.objects if obj.type == "MESH" and
               (obj.name in wanted if wanted is not None else obj.name.startswith(args.prefix))]
    rigs = sorted({modifier.object for obj in objects for modifier in obj.modifiers
                   if modifier.type == "ARMATURE" and modifier.object}, key=lambda obj: obj.name)
    materials = {}
    meshes = []
    graph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        authored = topology(obj.data, obj.matrix_world, args)
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            evaluated_metrics = topology(mesh, evaluated.matrix_world, args)
        finally:
            evaluated.to_mesh_clear()
        used_indices = sorted({face.material_index for face in obj.data.polygons})
        missing_faces = sum(face.material_index >= len(obj.material_slots) or
                            obj.material_slots[face.material_index].material is None
                            for face in obj.data.polygons)
        used_names = []
        for index in used_indices:
            if index < len(obj.material_slots) and obj.material_slots[index].material:
                material = obj.material_slots[index].material
                materials[material.name] = material
                used_names.append(material.name)
        meshes.append({
            "name": obj.name, "authored": authored, "evaluated": evaluated_metrics,
            "finite_object_matrix": finite(flatten_matrix(obj.matrix_world)),
            "scale": floats(obj.scale), "negative_scale": obj.matrix_world.determinant() < 0,
            "region": obj.get("arin_region"), "rigid_bone": obj.get("arin_bone"),
            "used_materials": used_names, "missing_material_faces": missing_faces,
            "uv_layers": uv_metrics(obj.data), "skin": skin_metrics(obj, args.weight_tolerance),
            "modifiers": [{"name": modifier.name, "type": modifier.type,
                           "viewport": modifier.show_viewport, "render": modifier.show_render}
                          for modifier in obj.modifiers],
        })
    fps = scene.render.fps / scene.render.fps_base
    used_actions = {rig.animation_data.action for rig in rigs if rig.animation_data and rig.animation_data.action}
    actions = [{"name": action.name, "frame_range": floats(action.frame_range),
                "duration_seconds": (action.frame_range[1] - action.frame_range[0]) / fps,
                "curves": len(action_curves(action)),
                "keyframes": sum(len(curve.keyframe_points) for curve in action_curves(action))}
               for action in sorted(used_actions, key=lambda action: action.name)]
    if args.frames is None:
        if actions:
            start, end = actions[0]["frame_range"]
            # Imported clips can start on a fractional frame. Rounding changes
            # the first phase while the last sample may clamp to the endpoint,
            # which falsely reports a discontinuous loop for a valid export.
            sample_frames = sorted({start + (end - start) * fraction
                                    for fraction in (0, .25, .5, .75, 1)})
        else:
            sample_frames = [float(scene.frame_current) + scene.frame_subframe]
    else:
        sample_frames = args.frames
    frames, first_positions, last_positions, first_root = [], None, None, None
    for frame in sample_frames:
        whole_frame = math.floor(frame)
        scene.frame_set(whole_frame, subframe=frame - whole_frame)
        bpy.context.view_layer.update()
        positions, hashes = frame_geometry(objects)
        flat = [point for points in positions.values() for point in points]
        minimum = [min((point[axis] for point in flat), default=0) for axis in range(3)]
        maximum = [max((point[axis] for point in flat), default=0) for axis in range(3)]
        roots = {rig.name + "/" + bone.name: flatten_matrix(rig.matrix_world @ bone.matrix)
                 for rig in rigs for bone in rig.pose.bones if bone.parent is None}
        if first_positions is None:
            first_positions, first_root = positions, roots
        displacements = {name: max_displacement(first_positions[name], points)
                         for name, points in positions.items()}
        frames.append({"frame": frame, "bounds_min": minimum, "bounds_max": maximum,
                       "dimensions": [high - low for low, high in zip(minimum, maximum)],
                       "local_geometry_sha256": hashes,
                       "displacement_from_first_frame_m": displacements,
                       "root_matrix_delta_from_first": max((abs(value - first_root[name][index])
                           for name, matrix in roots.items() for index, value in enumerate(matrix)), default=0),
                       "finite_root_matrices": all(finite(matrix) for matrix in roots.values())})
        last_positions = positions
    loop_displacements = {name: max_displacement(points, last_positions[name])
                          for name, points in (first_positions or {}).items()}
    report = {
        "schema_version": 1, "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "blender_version": bpy.app.version_string, "process_id": os.getpid(),
        "background_process": bool(bpy.app.background), "source": str(args.input) if args.input else bpy.data.filepath,
        "source_sha256": sha256(args.input) if args.input else None,
        "selection": {"prefix": args.prefix, "exact_names": sorted(wanted) if wanted else None},
        "coordinate_units": "Blender world units; expected_height_m assumes source units are meters",
        "scene_unit_scale": scene.unit_settings.scale_length,
        "sampled_frames": sample_frames, "fps": fps, "meshes": meshes,
        "rigs": [{"name": rig.name, "bones": [bone.name for bone in rig.data.bones],
                   "roots": [bone.name for bone in rig.data.bones if bone.parent is None],
                   "deform_bone_count": sum(bone.use_deform for bone in rig.data.bones)} for rig in rigs],
        "materials": [material_metrics(material) for material in materials.values()],
        "actions": actions, "motion": frames,
        "loop_endpoint_displacement_m": loop_displacements,
        "totals": {"mesh_objects": len(objects), "materials": len(materials), "armatures": len(rigs),
                   "authored_triangles": sum(mesh["authored"]["triangles"] for mesh in meshes),
                   "evaluated_triangles": sum(mesh["evaluated"]["triangles"] for mesh in meshes),
                   "authored_vertices": sum(mesh["authored"]["vertices"] for mesh in meshes)},
        "visual_review_required": True,
        "not_measured": ["UV overlap/unique texel coverage", "bone-joint surface continuity",
                         "intersections and clipping", "facial likeness and anatomy", "Unreal runtime rendering"],
    }
    findings = []
    def finding(kind, check, message, objects=None):
        findings.append({"severity": kind, "check": check, "message": message, "objects": objects or []})
    if not objects or not any(mesh["authored"]["vertices"] for mesh in meshes):
        finding("gate", "nonempty_mesh", "No selected non-empty mesh exists")
    for mesh in meshes:
        name = mesh["name"]
        if not mesh["finite_object_matrix"] or mesh["evaluated"]["invalid_vertices"]:
            finding("gate", "finite_coordinates", "Invalid evaluated vertex/object coordinates", [name])
        if mesh["evaluated"]["zero_area_triangles"] or mesh["evaluated"]["zero_length_edges"]:
            finding("gate" if contract.get("require_zero_degenerates") else "warning", "degenerates",
                    "Zero-area triangles or zero-length edges require region inspection", [name])
        if mesh["missing_material_faces"]:
            finding("gate", "material_assignment", "Some polygons have no assigned material", [name])
        if mesh["negative_scale"]:
            finding("warning", "negative_scale", "Negative world transform may invert winding", [name])
        if contract.get("require_uvs") and (not mesh["uv_layers"] or
                mesh["uv_layers"][0]["invalid_loops"] or
                mesh["uv_layers"][0]["non_degenerate_face_fraction"] < contract.get("min_uv_face_fraction", .995)):
            finding("gate", "usable_uvs", "Missing/invalid/degenerate primary UVs", [name])
        if contract.get("require_skinning"):
            skin = mesh["skin"]
            if not skin["rigs"] or skin["unweighted_vertices"] or skin["unnormalized_vertices"] or skin["invalid_weight_vertices"]:
                finding("gate", "deform_weights", "Missing, invalid, or unnormalized deform-bone weights", [name])
            if skin["max_influences"] > contract.get("maximum_bone_influences", 4):
                finding("gate", "influence_budget", "Per-vertex bone influence count exceeds contract", [name])
        if name in contract.get("require_closed_meshes", []) and (mesh["evaluated"]["boundary_edges"] or mesh["evaluated"]["non_manifold_edges"]):
            finding("gate", "declared_closed_mesh", "A specifically declared closed surface is open/non-manifold", [name])
        if mesh["authored"]["ngons"] and mesh["skin"]["max_influences"] > 1:
            finding("warning", "deforming_ngons", "Deforming authored surfaces contain n-gons", [name])
    totals = report["totals"]
    if totals["evaluated_triangles"] > contract.get("max_triangles", float("inf")):
        finding("gate", "triangle_budget", "Evaluated selected triangle count exceeds contract")
    if totals["materials"] > contract.get("max_materials", float("inf")):
        finding("gate", "material_budget", "Used material count exceeds contract; section merging needs explicit validation")
    if contract.get("require_skinning") and totals["armatures"] != 1:
        finding("gate", "single_armature", "Expected one shared deform armature")
    bones = {bone for rig in report["rigs"] for bone in rig["bones"]}
    missing_bones = sorted(set(contract.get("required_bones", [])) - bones)
    if missing_bones:
        finding("gate", "required_bones", "Missing required bones: " + ", ".join(missing_bones))
    action_filter = contract.get("require_action_name_contains")
    matched_actions = [action for action in actions if not action_filter or action_filter.lower() in action["name"].lower()]
    if action_filter and not matched_actions:
        finding("gate", "required_action", "Required active action did not survive import")
    if contract.get("expected_idle_seconds") is not None and matched_actions and any(
            abs(action["duration_seconds"] - contract["expected_idle_seconds"]) > .05 for action in matched_actions):
        finding("gate", "idle_duration", "Active Idle duration differs from contract")
    if frames and contract.get("expected_height_m") is not None:
        height = frames[0]["dimensions"][2]
        if abs(height - contract["expected_height_m"]) > contract.get("height_tolerance_m", .01):
            finding("gate", "known_height", "First sampled frame height differs from known character dimension")
    if contract.get("require_stationary_root") and any(frame["root_matrix_delta_from_first"] > 1e-5 for frame in frames):
        finding("gate", "stationary_root", "Root matrix changes across sampled Idle frames")
    if contract.get("require_loop") and any(value is None or value > .0001 for value in loop_displacements.values()):
        finding("gate", "loop_endpoints", "Evaluated loop endpoint geometry differs")
    if contract.get("require_deformation") and not any(len({frame["local_geometry_sha256"][obj.name] for frame in frames}) > 1 for obj in objects):
        finding("gate", "sampled_deformation", "No selected evaluated local mesh changes across sampled frames")
    for material in report["materials"]:
        if material.get("empty_image_nodes"):
            finding("warning", "empty_image_nodes", "Image texture nodes have no image", [material["name"]])
        for image in material["images"]:
            if not image["file_exists"] and not image["packed"]:
                finding("gate", "missing_texture", "Image texture has neither an external file nor packed data", [image["name"]])
            if contract.get("require_external_texture_files") and not image["file_exists"]:
                finding("gate", "unreal_texture_files", "FBX handoff requires an existing external texture file", [image["name"]])
        for channel in contract.get("required_texture_channels", []):
            if not material["pbr_inputs"].get(channel, {}).get("images"):
                finding("gate", "baked_pbr_channel", "Missing image-driven " + channel, [material["name"]])
        if contract.get("require_baked_pbr") and any(channel["procedural_nodes_requires_bake"] for channel in material["pbr_inputs"].values()):
            finding("gate", "procedural_bake", "Procedural material generators remain in runtime PBR inputs", [material["name"]])
    report["findings"] = findings
    report["technical_status"] = "failed" if any(item["severity"] == "gate" for item in findings) else "passed_requires_visual_review"
    report["contract"] = contract
    return report


def main():
    args = arguments()
    args.output = args.output.resolve()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.input:
        args.input = args.input.resolve()
        load_asset(args.input)
    original_frame = bpy.context.scene.frame_current
    original_subframe = bpy.context.scene.frame_subframe
    try:
        report = inspect(args)
    finally:
        bpy.context.scene.frame_set(original_frame, subframe=original_subframe)
        bpy.context.view_layer.update()
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "technical_status": report["technical_status"],
                      "totals": report["totals"], "visual_review_required": True}, ensure_ascii=False))
    return report


if __name__ == "__main__":
    result = main()
