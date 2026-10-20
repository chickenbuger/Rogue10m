"""Rig an actual Apollo multiview hull in Blender and export an idle NPC draft.

Blender invocation: --background --python-exit-code 2 --python this.py --
--input actual-hull.glb --front front.png --side side.png --back back.png
--out-dir Feature/doc/evidence/arin-apollo-20261005/rig

No substitute geometry, automatic heat weights, or Unreal assets are created.
The geometric weights and image projections are deliberately a simple draft;
the report and actual renders must be reviewed before game integration.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import traceback

import bmesh
import bpy
from mathutils import Quaternion, Vector
import numpy as np


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "front", "side", "back", "out-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-front", choices=("+Z", "-Z"), default="+Z")
    parser.add_argument("--side-facing", choices=("left", "right"), default="left")
    parser.add_argument("--resolution", type=int, default=768)
    parsed = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    for name in ("input", "front", "side", "back", "out_dir"):
        setattr(parsed, name, getattr(parsed, name).resolve())
    if not parsed.out_dir.is_relative_to(Path(__file__).resolve().parents[1]):
        parser.error("Output must remain within the Rogue10m workspace")
    if parsed.input.suffix.lower() != ".glb" or not parsed.input.is_file():
        parser.error("Input must be an actual existing GLB hull")
    if any(not getattr(parsed, name).is_file() for name in ("front", "side", "back")):
        parser.error("All three reference PNGs must exist")
    if not 128 <= parsed.resolution <= 2048:
        parser.error("Resolution must be between 128 and 2048")
    return parsed


def vector_values(value) -> list[float]:
    return [round(float(component), 7) for component in value]


def coordinates(mesh: bpy.types.Mesh) -> np.ndarray:
    values = np.empty(len(mesh.vertices) * 3, dtype=np.float64)
    mesh.vertices.foreach_get("co", values)
    return values.reshape((-1, 3))


def bounds(points: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    if not len(points) or not np.isfinite(points).all():
        raise ValueError("Hull vertices are empty or non-finite")
    return points.min(axis=0), points.max(axis=0)


def normalize_hull(args: argparse.Namespace) -> tuple[bpy.types.Object, dict]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(args.input))
    if any(obj.type == "ARMATURE" for obj in bpy.context.scene.objects):
        raise ValueError("This script accepts a static hull, not an already rigged character")
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise ValueError("GLB import produced no mesh")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = "SK_Arin_Apollo"
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    original = coordinates(obj.data)
    original_min, original_max = bounds(original)
    height = float(original_max[2] - original_min[2])
    if height <= 0:
        raise ValueError("Hull has no upright height after glTF coordinate conversion")
    transform_scale = 1.7 / height
    center = (original_min + original_max) / 2.0
    normalized = (original - np.array([center[0], center[1], original_min[2]])) * transform_scale
    if args.source_front == "-Z":
        normalized[:, 1] *= -1.0
    obj.data.vertices.foreach_set("co", normalized.reshape(-1))
    obj.data.update()
    mesh_edit = bmesh.new()
    mesh_edit.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(mesh_edit, faces=list(mesh_edit.faces))
    nonmanifold = sum(not edge.is_manifold for edge in mesh_edit.edges)
    edges = len(mesh_edit.edges)
    mesh_edit.to_mesh(obj.data)
    mesh_edit.free()
    minimum, maximum = bounds(coordinates(obj.data))
    dimensions = maximum - minimum
    if dimensions[1] / dimensions[2] < 0.12:
        raise ValueError("Hull is too thin to be accepted as a volumetric NPC")
    if nonmanifold / max(edges, 1) > 0.005:
        raise ValueError(f"Hull has too many non-manifold edges: {nonmanifold}/{edges}")
    if not 1.1 <= dimensions[0] <= 2.3:
        raise ValueError("Hull width does not match a 170 cm adult T-pose")
    return obj, {
        "original_bounds": [vector_values(original_min), vector_values(original_max)],
        "scale_to_170cm": transform_scale,
        "normalized_bounds": [vector_values(minimum), vector_values(maximum)],
        "dimensions_m": vector_values(dimensions),
        "nonmanifold_edges": nonmanifold,
        "edge_count": edges,
        "source_front": args.source_front,
        "blender_front": "-Y",
    }


def reference_texture(path: Path, label: str) -> tuple[bpy.types.Material, dict]:
    image = bpy.data.images.load(str(path), check_existing=False)
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    rgba = pixels.reshape((height, width, 4))  # Blender pixel rows grow from bottom to top.
    if rgba[:, :, 3].min() < 0.98:
        mask = rgba[:, :, 3] > 0.5
    else:
        border = np.concatenate((rgba[0, :, :3], rgba[-1, :, :3], rgba[:, 0, :3], rgba[:, -1, :3]))
        background = np.median(border, axis=0)
        mask = np.linalg.norm(rgba[:, :, :3] - background, axis=2) > 0.1
    rows, columns = np.nonzero(mask)
    if len(rows) < width * height * 0.01:
        raise ValueError(f"Reference {label} has no detectable subject")
    crop = [columns.min() / width, (columns.max() + 1) / width,
            rows.min() / height, (rows.max() + 1) / height]
    image.pack()
    material = bpy.data.materials.new("M_Arin_" + label)
    material.use_nodes = True
    tree = material.node_tree
    shader = tree.nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = 0.88
    shader.inputs["Metallic"].default_value = 0.0
    texture = tree.nodes.new("ShaderNodeTexImage")
    texture.image = image
    texture.interpolation = "Linear"
    texture.extension = "EXTEND"
    tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    return material, {"label": label, "path": str(path), "size": [width, height], "crop_uv": crop}


def project_references(obj: bpy.types.Object, args: argparse.Namespace) -> list[dict]:
    materials_and_refs = [reference_texture(getattr(args, label), label) for label in ("front", "side", "back")]
    obj.data.materials.clear()
    for material, _ in materials_and_refs:
        obj.data.materials.append(material)
    points = coordinates(obj.data)
    minimum, maximum = bounds(points)
    span = maximum - minimum
    uv = obj.data.uv_layers.new(name="ArinReferenceProjection")
    for polygon in obj.data.polygons:
        normal = polygon.normal
        if abs(normal.x) > abs(normal.y) * 1.15:
            material_index = 1
        else:
            material_index = 0 if normal.y <= 0 else 2
        polygon.material_index = material_index
        crop = materials_and_refs[material_index][1]["crop_uv"]
        for loop_index in polygon.loop_indices:
            point = points[obj.data.loops[loop_index].vertex_index]
            if material_index == 1:
                horizontal = (point[1] - minimum[1]) / span[1]
                if args.side_facing == "right":
                    horizontal = 1.0 - horizontal
            else:
                horizontal = (point[0] - minimum[0]) / span[0]
                if material_index == 2:
                    horizontal = 1.0 - horizontal
            vertical = (point[2] - minimum[2]) / span[2]
            uv.data[loop_index].uv = (crop[0] + horizontal * (crop[1] - crop[0]),
                                      crop[2] + vertical * (crop[3] - crop[2]))
    return [reference for _, reference in materials_and_refs]


def create_skeleton(obj: bpy.types.Object) -> tuple[bpy.types.Object, dict]:
    points = coordinates(obj.data)
    height = 1.7
    outer_arm = points[np.abs(points[:, 0]) > height * 0.30]
    if len(outer_arm) < 50:
        raise ValueError("Hull lacks separated T-pose arm geometry")
    arm_z = float(np.median(outer_arm[:, 2]))
    if not height * 0.68 <= arm_z <= height * 0.86:
        raise ValueError("Outer limb height does not resemble an adult T-pose")
    shoulder = height * 0.105
    arm_tip = float(np.quantile(np.abs(outer_arm[:, 0]), 0.99))
    hand_length = height * 0.052
    wrist = arm_tip - hand_length
    elbow = shoulder + (wrist - shoulder) * 0.5
    foot_points = points[(points[:, 2] < height * 0.075) & (points[:, 0] > 0)]
    foot_x = float(np.median(foot_points[:, 0])) if len(foot_points) else height * 0.09
    foot_x = float(np.clip(foot_x, height * 0.06, height * 0.14))
    definitions = [
        ("root", None, (0, 0, 0), (0, 0, height * 0.08)),
        ("hips", "root", (0, 0, height * 0.49), (0, 0, height * 0.55)),
        ("spine", "hips", (0, 0, height * 0.55), (0, 0, height * 0.65)),
        ("chest", "spine", (0, 0, height * 0.65), (0, 0, arm_z + height * 0.015)),
        ("neck", "chest", (0, 0, arm_z + height * 0.015), (0, 0, height * 0.86)),
        ("head", "neck", (0, 0, height * 0.86), (0, 0, height * 0.99)),
    ]
    for suffix, sign in (("L", 1), ("R", -1)):
        definitions += [
            ("clavicle_" + suffix, "chest", (0, 0, arm_z), (sign * shoulder, 0, arm_z)),
            ("upper_arm_" + suffix, "clavicle_" + suffix, (sign * shoulder, 0, arm_z), (sign * elbow, 0, arm_z)),
            ("lower_arm_" + suffix, "upper_arm_" + suffix, (sign * elbow, 0, arm_z), (sign * wrist, 0, arm_z)),
            ("hand_" + suffix, "lower_arm_" + suffix, (sign * wrist, 0, arm_z), (sign * arm_tip, 0, arm_z)),
            ("upper_leg_" + suffix, "hips", (sign * foot_x, 0, height * 0.49), (sign * foot_x, 0, height * 0.25)),
            ("lower_leg_" + suffix, "upper_leg_" + suffix, (sign * foot_x, 0, height * 0.25), (sign * foot_x, 0, height * 0.055)),
            ("foot_" + suffix, "lower_leg_" + suffix, (sign * foot_x, 0, height * 0.055), (sign * foot_x, -height * 0.10, height * 0.025)),
        ]
    armature_data = bpy.data.armatures.new("ArinHumanoidSkeleton")
    armature = bpy.data.objects.new("ArinArmature", armature_data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, start, end in definitions:
        bone = armature_data.edit_bones.new(name)
        bone.head, bone.tail = start, end
        if parent:
            bone.parent = armature_data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.parent = armature
    modifier = obj.modifiers.new("ArinSemanticSkin", "ARMATURE")
    modifier.object = armature
    modifier.use_deform_preserve_volume = False
    return armature, {"arm_height": arm_z, "shoulder_x": shoulder, "elbow_x": elbow,
                      "wrist_x": wrist, "hand_tip_x": arm_tip, "foot_x": foot_x,
                      "bone_count": len(definitions), "definitions": definitions}


def segment_distance(point: np.ndarray, start, end) -> float:
    start, end = np.asarray(start, dtype=float), np.asarray(end, dtype=float)
    segment = end - start
    amount = float(np.clip(np.dot(point - start, segment) / np.dot(segment, segment), 0.0, 1.0))
    return float(np.linalg.norm(point - start - amount * segment))


def chain_weights(point: np.ndarray, chain: list[str], definitions: dict) -> dict[str, float]:
    distances = [(segment_distance(point, definitions[name][0], definitions[name][1]), name) for name in chain]
    # Blend at most two neighboring semantic segments, with a finite 12 mm radius.
    selected = sorted(distances)[:2]
    weights = {name: 1.0 / (distance * distance + 0.012 ** 2) for distance, name in selected}
    total = sum(weights.values())
    return {name: weight / total for name, weight in weights.items()}


def apply_semantic_weights(obj: bpy.types.Object, skeleton: dict) -> dict:
    groups = {name: obj.vertex_groups.new(name=name) for name, _, _, _ in skeleton["definitions"]}
    segments = {name: (start, end) for name, _, start, end in skeleton["definitions"]}
    counts = {name: 0 for name in groups}
    arm_z, shoulder = skeleton["arm_height"], skeleton["shoulder_x"]
    sums = []
    for vertex in obj.data.vertices:
        point = np.array(vertex.co)
        abs_x, z = abs(point[0]), point[2]
        suffix = "L" if point[0] >= 0 else "R"
        if abs_x > shoulder * 0.73 and abs(z - arm_z) < 1.7 * 0.068:
            arm = chain_weights(point, ["clavicle_" + suffix, "upper_arm_" + suffix,
                                        "lower_arm_" + suffix, "hand_" + suffix], segments)
            blend = float(np.clip((abs_x - shoulder * 0.73) / (shoulder * 0.6), 0.0, 1.0))
            weights = {name: weight * blend for name, weight in arm.items()}
            weights["chest"] = weights.get("chest", 0.0) + 1.0 - blend
        elif z < 1.7 * 0.47 and abs_x < 1.7 * 0.25:
            # Rear cape pixels below the pelvis are anchored to the hips, not the knees.
            if point[1] > 1.7 * 0.075 and z > 1.7 * 0.27:
                weights = {"hips": 1.0}
            else:
                weights = chain_weights(point, ["hips", "upper_leg_" + suffix,
                                               "lower_leg_" + suffix, "foot_" + suffix], segments)
        else:
            weights = chain_weights(point, ["hips", "spine", "chest", "neck", "head"], segments)
        positive = {name: weight for name, weight in weights.items() if weight > 0.0001}
        total = sum(positive.values())
        if total <= 0 or not math.isfinite(total):
            raise ValueError(f"Vertex {vertex.index} has invalid semantic weights")
        for name, value in positive.items():
            groups[name].add([vertex.index], value / total, "REPLACE")
            counts[name] += 1
        sums.append(sum(value / total for value in positive.values()))
    if any(counts[name] == 0 for name in ("head", "upper_arm_L", "upper_arm_R", "lower_leg_L", "lower_leg_R")):
        raise ValueError("Hull anatomy cannot populate the required head and limb groups")
    return {"method": "explicit anatomical regions and nearest semantic bone segments; no heat weights",
            "vertices": len(obj.data.vertices), "weight_sum_min": min(sums), "weight_sum_max": max(sums),
            "vertices_per_group": counts}


def world_rotation(bone: bpy.types.PoseBone, axis, angle: float) -> Quaternion:
    rest = bone.bone.matrix_local.to_quaternion()
    return rest.inverted() @ Quaternion(Vector(axis), angle) @ rest


def set_pose(armature: bpy.types.Object, degrees: float, breath: float = 0.0) -> None:
    for bone in armature.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = Quaternion()
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)
    for suffix, sign in (("L", 1), ("R", -1)):
        bone = armature.pose.bones["upper_arm_" + suffix]
        bone.rotation_quaternion = world_rotation(bone, (0, 1, 0), math.radians(degrees) * sign)
    chest = armature.pose.bones["chest"]
    chest.rotation_quaternion = world_rotation(chest, (1, 0, 0), math.radians(0.45) * breath)
    head = armature.pose.bones["head"]
    head.rotation_quaternion = world_rotation(head, (0, 0, 1), math.radians(0.7) * breath)
    armature.pose.bones["hips"].location.z = 0.001 * breath
    bpy.context.view_layer.update()


def evaluated_points(obj: bpy.types.Object) -> np.ndarray:
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    try:
        return coordinates(mesh).copy()
    finally:
        evaluated.to_mesh_clear()


def deformation_check(obj: bpy.types.Object, original: np.ndarray) -> dict:
    posed = evaluated_points(obj)
    edge_indices = np.array([edge.vertices[:] for edge in obj.data.edges], dtype=int)
    rest_lengths = np.linalg.norm(original[edge_indices[:, 0]] - original[edge_indices[:, 1]], axis=1)
    lengths = np.linalg.norm(posed[edge_indices[:, 0]] - posed[edge_indices[:, 1]], axis=1)
    valid = rest_lengths > 0.000001
    stretch = lengths[valid] / rest_lengths[valid]
    obj.data.calc_loop_triangles()
    triangles = np.array([triangle.vertices[:] for triangle in obj.data.loop_triangles], dtype=int)
    old = original[triangles]
    new = posed[triangles]
    old_area = np.linalg.norm(np.cross(old[:, 1] - old[:, 0], old[:, 2] - old[:, 0]), axis=1)
    new_area = np.linalg.norm(np.cross(new[:, 1] - new[:, 0], new[:, 2] - new[:, 0]), axis=1)
    valid_faces = old_area > 0.00000001
    collapse_fraction = float(np.mean(new_area[valid_faces] / old_area[valid_faces] < 0.10))
    report = {"finite": bool(np.isfinite(posed).all()), "edge_stretch_p99": float(np.quantile(stretch, 0.99)),
              "edge_stretch_max": float(stretch.max()), "collapsed_triangle_fraction": collapse_fraction,
              "posed_bounds": [vector_values(value) for value in bounds(posed)],
              "limits": {"edge_stretch_p99": 1.75, "edge_stretch_max": 4.0, "collapsed_triangle_fraction": 0.003},
              "self_intersections": "not automatically evaluated; inspect the actual front, side, and oblique renders"}
    report["passed"] = report["finite"] and report["edge_stretch_p99"] <= 1.75 and report["edge_stretch_max"] <= 4.0 and collapse_fraction <= 0.003
    return report


def make_idle(armature: bpy.types.Object, degrees: float) -> None:
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.frame_start, scene.frame_end = 1, 61
    action = bpy.data.actions.new("A_Arin_Idle")
    armature.animation_data_create()
    armature.animation_data.action = action
    for frame in (1, 16, 31, 46, 61):
        scene.frame_set(frame)
        set_pose(armature, degrees, math.sin((frame - 1) * math.tau / 60.0))
        for bone in armature.pose.bones:
            bone.keyframe_insert(data_path="rotation_quaternion", frame=frame, group=bone.name)
            bone.keyframe_insert(data_path="location", frame=frame, group=bone.name)
    scene.frame_set(16)
    bpy.context.view_layer.update()


def export_files(obj: bpy.types.Object, armature: bpy.types.Object, args: argparse.Namespace) -> dict:
    common = {"use_selection": True, "add_leaf_bones": False, "axis_forward": "-Z", "axis_up": "Y",
              "apply_unit_scale": True, "bake_space_transform": False, "path_mode": "COPY", "embed_textures": True}
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    mesh_fbx = args.out_dir / "SK_Arin_Apollo.fbx"
    bpy.ops.export_scene.fbx(filepath=str(mesh_fbx), object_types={"MESH", "ARMATURE"}, bake_anim=False, **common)
    obj.select_set(False)
    idle_fbx = args.out_dir / "A_Arin_Idle.fbx"
    bpy.ops.export_scene.fbx(filepath=str(idle_fbx), object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                             bake_anim_force_startend_keying=True, bake_anim_step=1.0, **common)
    result = {}
    for label, path in (("skeletal_mesh", mesh_fbx), ("idle_animation", idle_fbx)):
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"FBX export missing: {label}")
        result[label] = {"path": str(path), "size_bytes": path.stat().st_size,
                         "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    return result


def render_idle(obj: bpy.types.Object, args: argparse.Namespace) -> list[dict]:
    # Use the already reviewed FBX inspection renderer with the actual posed mesh.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from InspectApolloFBX import render_views

    minimum, maximum = bounds(evaluated_points(obj))
    renders = render_views(args, Vector(minimum), Vector(maximum))
    for render in renders:
        path = Path(render["path"])
        renamed = path.with_name(path.name.replace("fbx-", "idle-"))
        path.replace(renamed)
        render["path"] = str(renamed)
        render["animation_frame"] = bpy.context.scene.frame_current
    return renders


def main() -> None:
    args = arguments()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.out_dir / "arin-rig-report.json"
    report = {"status": "running", "source_glb": str(args.input),
              "source_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
              "blender_version": bpy.app.version_string, "target_height_m": 1.7,
              "known_limitations": ["Silhouette-derived geometry and approximate planar color projection",
                                    "Simple 20-bone rig; no facial or finger animation",
                                    "Generated views are reference hints, not scans",
                                    "Automatic deformation checks do not replace visual review"],
              "game_integration": "not performed; actual renders and FBX re-import must be reviewed"}
    try:
        obj, normalization = normalize_hull(args)
        report["normalization"] = normalization
        report["texture_references"] = project_references(obj, args)
        armature, skeleton = create_skeleton(obj)
        report["skeleton"] = skeleton
        report["weights"] = apply_semantic_weights(obj, skeleton)
        original = coordinates(obj.data)
        report["neutral_pose_attempts"] = []
        accepted = None
        for degrees in (65.0, 55.0):
            set_pose(armature, degrees)
            check = deformation_check(obj, original)
            report["neutral_pose_attempts"].append({"arm_lowering_degrees": degrees, **check})
            if check["passed"]:
                accepted = degrees
                break
        if accepted is None:
            # Preserve actual failure renders without exporting an unusable rig.
            report["failure_renders"] = render_idle(obj, args)
            raise ValueError("No arm-down pose passed the deformation checks; no game FBX exported")
        report["accepted_arm_lowering_degrees"] = accepted
        make_idle(armature, accepted)
        report["idle_extreme_deformation"] = deformation_check(obj, original)
        if not report["idle_extreme_deformation"]["passed"]:
            raise ValueError("Idle deformation failed")
        report["idle"] = {"action": "A_Arin_Idle", "fps": 30, "frame_start": 1, "frame_end": 61,
                          "duration_seconds": 2.0, "stationary": True}
        report["renders"] = render_idle(obj, args)
        report["files"] = export_files(obj, armature, args)
        bpy.ops.wm.save_as_mainfile(filepath=str(args.out_dir / "Arin_Apollo_Rig.blend"))
        report["status"] = "exported_draft_requires_visual_review"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        raise
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("ARIN_RIG_RESULT " + json.dumps({"status": report["status"], "report": str(report_path)}))


if __name__ == "__main__":
    main()
