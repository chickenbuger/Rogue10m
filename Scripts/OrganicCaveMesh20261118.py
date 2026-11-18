#!/usr/bin/env python3
"""Author original, continuous Q/Z cave rock shells from the retained route plan.

This program writes source geometry only; it never opens Unreal, changes a map,
imports assets, or edits the retained cave recipes. Coordinates are centimeters.
Run with tmp/apollo-venv/Scripts/python.exe (NumPy/SciPy/scikit-image/trimesh).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import trimesh
from skimage.measure import marching_cubes


ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "Feature/doc/evidence/caves20261118/plan.json"
SOURCE = ROOT / "Art/World/CavesOrganic20261118"
EVIDENCE = ROOT / "Feature/doc/evidence/cave-organic20261118"
PITCH = 40.0
FLOOR_Z = 8610.0
MAX_TRIANGLES = 90000


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")


def cell_center(cell):
    return np.asarray([(cell[0] + 0.5) * 500, (cell[1] + 0.5) * 500], dtype=np.float32)


def segment_distance(x, y, a, b):
    dx, dy = b - a
    denominator = dx * dx + dy * dy
    if denominator == 0:
        return np.hypot(x - a[0], y - a[1])
    t = np.clip(((x - a[0]) * dx + (y - a[1]) * dy) / denominator, 0, 1)
    return np.hypot(x - a[0] - t * dx, y - a[1] - t * dy)


def rounded_box2(x, y, center, half, radius):
    qx = np.abs(x - center[0]) - (half[0] - radius)
    qy = np.abs(y - center[1]) - (half[1] - radius)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - radius


def smooth_max(a, b, k):
    h = np.maximum(k - np.abs(a - b), 0) / k
    return np.maximum(a, b) + h * h * k * 0.25


class CaveField:
    """A route-preserving cavity subtracted from one closed rock body."""

    def __init__(self, site):
        self.site = site
        self.origin = np.asarray([*site["origin_cm"], FLOOR_Z], dtype=np.float64)
        self.phase = 0.67 if site["site_id"] == "Q" else 2.09
        self.edges = [(cell_center(a), cell_center(b)) for a, b in site["graph_edges"]]
        # The legacy roof lip is 500 cm outside the pit. Extend the entry bore
        # through this lip, not merely to the first square floor center.
        entry = cell_center(site["entry_cell"])
        rect = np.asarray(site["roof_rect_cm"], dtype=np.float32) - np.tile(self.origin[:2], 2)
        self.outer_rect = rect
        direction = -1 if site["ramp"]["top_y_cm"] < site["ramp"]["bottom_y_cm"] else 1
        self.entry_direction = direction
        end = entry.copy()
        end[1] = (rect[1] if direction < 0 else rect[3]) + direction * 300
        self.edges.append((entry, end))
        self.rooms = []
        for name, (x0, x1, y0, y1) in site["rooms"].items():
            lo, hi = np.asarray([x0 * 500, y0 * 500]), np.asarray([(x1 + 1) * 500, (y1 + 1) * 500])
            # The maximum displacement is 14 cm; outer room limit 230+14
            # leaves at least 6 cm of the existing floor beneath the wall.
            self.rooms.append((name, (lo + hi) / 2, (hi - lo) / 2 - 20, 220.0))
        self.barriers = []
        for c0, c1 in site["forced_closed_edges"]:
            a, b = cell_center(c0), cell_center(c1)
            mid = (a + b) / 2
            normal = (b - a) / np.linalg.norm(b - a)
            tangent = np.asarray([-normal[1], normal[0]])
            # Overlap adjacent barrier segments; rounded ends cannot create a
            # diagonal bypass at the end of a preserved secret-room partition.
            self.barriers.append((mid - tangent * 270, mid + tangent * 270))

    def horizontal(self, x, y):
        shape = np.broadcast_shapes(np.shape(x), np.shape(y))
        d = np.full(shape, np.inf, dtype=np.float32)
        safe = d.copy()
        for a, b in self.edges:
            dist = segment_distance(x, y, a, b)
            d = np.minimum(d, dist - 230.0)
            safe = np.minimum(safe, dist - 210.0)
        for _, center, half, radius in self.rooms:
            # half is total-room half size minus20; therefore room side inset
            #20, plus roughness bounded14 leaves six centimeters of floor.
            d = np.minimum(d, rounded_box2(x, y, center, half, radius))
        return d, safe

    def roughness(self, x, y, z):
        p = self.phase
        # Aperiodic-looking, deterministic continuous strata. No 500 cm grid
        # frequency, detached stones, repeated spheres or displacement seams.
        return (7.0 * np.sin(x / 173 + y / 439 + z / 137 + p)
                + 4.5 * np.sin(x / 347 - y / 229 + z / 213 + p * 2)
                + 2.5 * np.sin(x / 83 + y / 137 - z / 179 + p * 3))

    def cavity(self, x, y, z):
        horizontal, safe = self.horizontal(x, y)
        d = np.minimum(horizontal + self.roughness(x, y, z), safe)
        for a, b in self.barriers:
            # A bounded, gently curving thickness stays outside the guaranteed
            #210 cm centerline radius, including the Camera-height plane.
            barrier = segment_distance(x, y, a, b) - (31 + 3 * np.sin(y / 271 + x / 353 + self.phase))
            d = np.maximum(d, -barrier)
        # Vault spring line near9000; continuous wall-to-roof fillet gives a
        # curved shoulder. The crown undulates independently through rooms.
        crown = (9044 + 74 * (1 - np.exp(np.minimum(horizontal, 0) / 145))
                 + 22 * np.sin(x / 383 + y / 661 + self.phase)
                 + 15 * np.sin(y / 277 - x / 719 + self.phase * 1.7)
                 + 9 * np.sin(x / 131 + y / 199 + self.phase * 2.4))
        # The entrance roof receives the highest practical vault to preserve
        # clearance over the unchanged ramp where it enters the roof lip.
        entry_y = cell_center(self.site["entry_cell"])[1]
        entrance_weight = np.exp(-np.maximum((y - entry_y) * -self.entry_direction, 0) / 230)
        entrance_weight = entrance_weight * np.exp(-np.abs(x - cell_center(self.site["entry_cell"])[0]) / 420)
        crown = crown * (1 - entrance_weight) + (9164 + 3 * np.sin(x / 179 + y / 313)) * entrance_weight
        crown = np.minimum(crown, 9169.0) - FLOOR_Z
        return smooth_max(d, z - crown, 180.0)

    def __call__(self, x, y, z):
        rect = self.outer_rect
        center = (rect[:2] + rect[2:]) / 2
        half = (rect[2:] - rect[:2]) / 2
        exterior = rounded_box2(x, y, center, half, 120)
        # Outer cap ends below the existing grass cube; the closed bottom
        # ring is buried below all existing walkable floor surfaces.
        outer = np.maximum(exterior, np.maximum((8490 - FLOOR_Z) - z, z - (9590 - FLOOR_Z)))
        return np.maximum(outer, -self.cavity(x, y, z))


def sample_routes(field):
    distances = []
    count = 0
    for a, b in field.edges[:-1]:
        delta = b - a
        tangent = delta / np.linalg.norm(delta)
        side = np.asarray([-tangent[1], tangent[0]])
        for t in np.linspace(0, 1, 21):
            center = a + (b - a) * t
            for lateral in (-120, 0, 120):
                point = center + lateral * side
                # Original large route capsule: radius42, halfheight96.
                for z_world in (8610 + 96, 8842, 8610 + 192):
                    d = float(field.cavity(point[0], point[1], z_world - FLOOR_Z))
                    distances.append(d)
                    count += 1
    return {"samples": count, "maximum_cavity_sdf_cm": max(distances),
            "required_route_capsule_radius_cm": 42, "all_samples_have_radius42_margin": max(distances) < -42}


def mesh_audit(mesh):
    unique, counts = np.unique(np.sort(mesh.edges, axis=1), axis=0, return_counts=True)
    return {"vertices": len(mesh.vertices), "triangles": len(mesh.faces),
            "watertight": bool(mesh.is_watertight), "winding_consistent": bool(mesh.is_winding_consistent),
            "connected_bodies": int(mesh.body_count), "positive_signed_volume": bool(mesh.volume > 0),
            "volume_cm3": float(mesh.volume), "all_edges_incidence_two": bool(np.all(counts == 2)),
            "degenerate_triangles": int(np.count_nonzero(mesh.area_faces < 1e-5)),
            "bounds_local_cm": mesh.bounds.tolist()}


def build_site(site, output, evidence):
    field = CaveField(site)
    rect = field.outer_rect
    xs = np.arange(rect[0] - PITCH, rect[2] + PITCH * 1.5, PITCH, dtype=np.float32)
    ys = np.arange(rect[1] - PITCH, rect[3] + PITCH * 1.5, PITCH, dtype=np.float32)
    zs = np.arange(8490 - FLOOR_Z - PITCH, 9590 - FLOOR_Z + PITCH * 1.5, PITCH, dtype=np.float32)
    values = field(xs[:, None, None], ys[None, :, None], zs[None, None, :]).astype(np.float32)
    verts, faces, _, _ = marching_cubes(values, 0, spacing=(PITCH,) * 3, allow_degenerate=False)
    verts += np.asarray([xs[0], ys[0], zs[0]])
    mesh = trimesh.Trimesh(verts, faces, process=True)
    mesh.fix_normals(multibody=True)
    initial_count = len(mesh.faces)
    if len(mesh.faces) > MAX_TRIANGLES:
        mesh = mesh.simplify_quadric_decimation(face_count=MAX_TRIANGLES, aggression=3)
        # The quadric backend can retain zero-area repeated-index triangles
        # after a collapse. Remove only these null faces, then weld the exact
        # coincident positions and require the full manifold audit again.
        mesh.update_faces(mesh.nondegenerate_faces())
        mesh.merge_vertices(digits_vertex=2)
        mesh.update_faces(mesh.nondegenerate_faces())
        mesh.remove_unreferenced_vertices()
        mesh.fix_normals(multibody=True)
    # Audit the exact six-decimal coordinates delivered to the importer.
    mesh.vertices = np.round(mesh.vertices, 6)
    audit = mesh_audit(mesh)
    assert audit["watertight"] and audit["winding_consistent"] and audit["connected_bodies"] == 1, audit
    assert audit["all_edges_incidence_two"] and not audit["degenerate_triangles"] and audit["positive_signed_volume"], audit
    routes = sample_routes(field)
    assert routes["all_samples_have_radius42_margin"], routes
    sid = site["site_id"]
    name = f"SM_CaveOrganic_{sid}_20261118"
    obj = output / (name + ".obj")
    with obj.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# Original Rogue10m continuous cave shell, 2026-11-18. Local XYZ centimeters, Z up.\n")
        stream.write(f"# World actor pivot XYZ cm: {','.join(str(float(v)) for v in field.origin)}\n")
        stream.write(f"o {name}\n")
        for v in mesh.vertices:
            stream.write("v %.6f %.6f %.6f\n" % tuple(v))
        # Smooth normals are shared at the connected rock surface. Material
        # uses world triplanar sampling, so this source intentionally has no UV.
        for n in mesh.vertex_normals:
            stream.write("vn %.7f %.7f %.7f\n" % tuple(n))
        stream.write("s 1\n")
        for face in mesh.faces + 1:
            stream.write("f " + " ".join(f"{i}//{i}" for i in face) + "\n")
    indexed = output / (name + ".mesh.json")
    write_json(indexed, {"schema": "Rogue10m.OrganicCaveIndexedMesh.20261118.v1", "site_id": sid,
                         "coordinate_system": "local_XYZ_centimeters_Z_up", "world_pivot_cm": field.origin.tolist(),
                         "vertices_cm": np.round(mesh.vertices, 6).tolist(), "triangles": mesh.faces.tolist()})
    # Explicit graph invariants and barrier centers are input for independent
    # mesh-triangle QA. A numeric field result is not an Unreal collision pass.
    barriers = []
    for a, b in site["forced_closed_edges"]:
        mid = (cell_center(a) + cell_center(b)) / 2
        samples = [float(field.cavity(mid[0], mid[1], z - FLOOR_Z)) for z in (8650, 8842, 9000)]
        barriers.append({"cells": [a, b], "midpoint_world_cm": [*(mid + field.origin[:2]).tolist(), 8842],
                         "cavity_sdf_cm_at_z8650_8842_9000": samples, "closed": min(samples) > 0})
    assert all(b["closed"] for b in barriers)
    return {"site_id": sid, "world_pivot_cm": field.origin.tolist(),
            "source_obj": str(obj.relative_to(ROOT)).replace("\\", "/"), "source_obj_sha256": digest(obj),
            "indexed_mesh": str(indexed.relative_to(ROOT)).replace("\\", "/"), "indexed_mesh_sha256": digest(indexed),
            "raw_marching_cubes_triangles": initial_count, "mesh_audit": audit, "field_route_margin": routes,
            "forced_closed_barriers": barriers, "source_graph_edges": site["graph_edges"],
            "source_runtime_routes_ground_cm": site["runtime_routes"],
            "entrance": {"roof_lip_y_world_cm": float(field.origin[1] + (rect[1] if field.entry_direction < 0 else rect[3])),
                         "ramp_x_world_cm": site["ramp"]["x_cm"], "bore_extends_beyond_outer_rock_cm": 300,
                         "entry_crown_target_world_z_cm": [9161, 9167]},
            "collision_requirement": "UseComplexAsSimple; never convex-hull this concave connected cave",
            "editor_validation_status": "not_imported_not_runtime_validated"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=SOURCE)
    parser.add_argument("--evidence", type=Path, default=EVIDENCE)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.evidence.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8-sig"))
    spec = {"schema": "Rogue10m.OrganicCaveGeometrySpec.20261118.v1", "generator": str(Path(__file__).relative_to(ROOT)),
            "generator_sha256": digest(Path(__file__)), "source_plan_sha256": digest(PLAN),
            "authorship": "Original analytic geometry by Rogue10m; no external mesh/texture copied.",
            "material_reuse": "Root-owned Unreal integration reuses the current project-owned world-triplanar Stone surface.",
            "units": "centimeters", "marching_cubes_pitch_cm": PITCH, "maximum_triangles_per_site": MAX_TRIANGLES,
            "preserved_floor_walk_z_cm": FLOOR_Z, "preserved_grass_roof_top_z_cm": 9602,
            "protected_landscape_composite_blake3": "67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc",
            "import_contract": {"OBJ": "XYZ cm/Z-up; verify importer axis readback before placement", "rotation_degrees": [0, 0, 0],
                                "scale": [1, 1, 1], "place_actor_at": "site.world_pivot_cm", "material_uv_required": False,
                                "static_mesh_collision": "UseComplexAsSimple", "preserve_imported_normals": True},
            "sites": [build_site(site, args.output, args.evidence) for site in plan["sites"]],
            "verification_boundary": "Pure source geometry only; requires actual Unreal import, capsule/camera QA, visual review and cold reload."}
    target = args.evidence / "geometryspec.json"
    write_json(target, spec)
    print(json.dumps({"geometry_spec": str(target), "sites": [{"id": s["site_id"], "audit": s["mesh_audit"], "routes": s["field_route_margin"]} for s in spec["sites"]]}, indent=2))


if __name__ == "__main__":
    main()
