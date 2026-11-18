#!/usr/bin/env python3
"""V2 original organic rock sculpt; source only, no Unreal/Git operations.

Retains V1 as a rejected candidate. Uses its small math/export helpers without
modifying its script, source geometry, or receipts. Run in tmp/apollo-venv.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import trimesh
from skimage.measure import marching_cubes

from OrganicCaveMesh20261118 import ROOT, PLAN, FLOOR_Z, cell_center, segment_distance, rounded_box2, smooth_max, digest, write_json, mesh_audit

SOURCE = ROOT / "Art/World/CavesOrganic20261118/V2"
EVIDENCE = ROOT / "Feature/doc/evidence/cave-organic20261118"
PITCH = 30.0
MAX_TRIANGLES = 126000


class SculptField:
    def __init__(self, site):
        self.site = site
        self.origin = np.asarray([*site["origin_cm"], FLOOR_Z], dtype=np.float64)
        self.phase = 0.67 if site["site_id"] == "Q" else 2.09
        self.edges = [(cell_center(a), cell_center(b)) for a, b in site["graph_edges"]]
        self.cells = set(map(tuple, site["floor_cells"]))
        self.outer_rect = np.asarray(site["roof_rect_cm"], dtype=np.float32) - np.tile(self.origin[:2], 2)
        self.pit_rect = np.asarray(site["pit_rect_cm"], dtype=np.float32) - np.tile(self.origin[:2], 2)
        self.entry = cell_center(site["entry_cell"])
        self.direction = -1 if site["ramp"]["top_y_cm"] < site["ramp"]["bottom_y_cm"] else 1
        end = self.entry.copy()
        end[1] = (self.outer_rect[1] if self.direction < 0 else self.outer_rect[3]) + self.direction * 300
        self.entrance_edge = (self.entry, end)
        self.rooms = []
        for name, (x0, x1, y0, y1) in site["rooms"].items():
            lo, hi = np.asarray([x0 * 500, y0 * 500]), np.asarray([(x1 + 1) * 500, (y1 + 1) * 500])
            self.rooms.append((name, (lo + hi) / 2, (hi - lo) / 2 + 35, 300.0))
        original_barriers = []
        for a, b in site["forced_closed_edges"]:
            a, b = cell_center(a), cell_center(b)
            mid = (a + b) / 2
            normal = (b - a) / np.linalg.norm(b - a)
            tangent = np.asarray([-normal[1], normal[0]])
            original_barriers.append((mid - tangent * 250, mid + tangent * 250))
        self.barriers = []
        for i, (a, b) in enumerate(original_barriers):
            delta = (b - a) / np.linalg.norm(b - a)
            ends = []
            for endpoint, sign in ((a, -1), (b, 1)):
                joins_another = any(np.linalg.norm(endpoint - other_end) < 0.01
                                    for j, pair in enumerate(original_barriers) if j != i for other_end in pair)
                ends.append(endpoint + sign * delta * (0 if joins_another else 140))
            self.barriers.append(tuple(ends))
        # A diagonal cell contact cannot become a new portal when neither
        # orthogonal connecting cell exists. Current Q/Z plans have none, but
        # the generator explicitly guards this case rather than assuming it.
        self.diagonal_guards = []
        for x, y in self.cells:
            for dy in (-1, 1):
                if (x + 1, y + dy) in self.cells and (x + 1, y) not in self.cells and (x, y + dy) not in self.cells:
                    self.diagonal_guards.append((cell_center((x, y)) + cell_center((x + 1, y + dy))) / 2)

    def horizontal(self, x, y, z=232):
        p = self.phase
        shape = np.broadcast_shapes(np.shape(x), np.shape(y), np.shape(z))
        d = np.full(shape, np.inf, dtype=np.float32)
        safe = np.full(shape, np.inf, dtype=np.float32)
        for a, b in self.edges:
            dist = segment_distance(x, y, a, b)
            # At the actual8842 camera plane and below, every ordinary passage
            # has a positive outward radius235..295cm. Aperiodic axial waves
            # vary silhouette without shifting any retained route centerline.
            r = 265 + 20 * np.sin(x / 241 + y / 367 + p) + 10 * np.sin(x / 521 - y / 179 + p * 2)
            d = np.minimum(d, dist - r)
            safe = np.minimum(safe, dist - 235)
        # Warp large rooms as a volume, not as pasted stones on a flat wall.
        #80..110cm motions use unrelated500..900cm full wavelengths.
        wx = 70 * np.sin(y * (2 * np.pi / 823) + x * (2 * np.pi / 1673) + p) + 35 * np.sin(y * (2 * np.pi / 547) - z / 233 + p * 2)
        wy = 70 * np.sin(x * (2 * np.pi / 719) - y * (2 * np.pi / 1823) + p * 1.3) + 35 * np.sin(x * (2 * np.pi / 593) + z / 191 + p * 2.4)
        for _, center, half, radius in self.rooms:
            d = np.minimum(d, rounded_box2(x + wx, y + wy, center, half, radius))
        # Large folds rise from a safe, wider base above the camera plane.
        upper = np.clip((z - 280) / 155, 0, 1)
        upper = upper * upper * (3 - 2 * upper)
        sculpt = (46 * np.sin(x * (2 * np.pi / 617) + y * (2 * np.pi / 911) + z / 173 + p)
                  + 24 * np.sin(x * (2 * np.pi / 373) - y * (2 * np.pi / 577) - z / 117 + p * 1.7))
        micro = 8 * np.sin(x / 67 + y / 113 + z / 71 + p * 2.7)
        d = d + upper * sculpt + upper * micro
        # Guarantee ordinary corridor width through the original camera plane;
        # the seven80cm separators deliberately retain their old210cm side.
        d = np.minimum(d, safe + upper * 55)
        pit = self.pit_rect
        pit_limit = np.maximum.reduce(np.broadcast_arrays(pit[0] - 120 - x, x - pit[2] - 120,
                                                          pit[1] - 120 - y, y - pit[3] - 120))
        d = np.maximum(d, pit_limit)
        entrance_dist = segment_distance(x, y, *self.entrance_edge)
        entrance_r = 274 + 18 * np.sin(y / 219 + p)
        d = np.minimum(d, entrance_dist - entrance_r)
        for a, b in self.barriers:
            # The full80cm solid barrier is fixed through body/camera height.
            # Above it the continuous wall develops a strong broad rock fold.
            barrier_r = 40 + upper * (20 + 16 * np.sin(x / 173 + y / 251 + p))
            d = np.maximum(d, -(segment_distance(x, y, a, b) - barrier_r))
        for center in self.diagonal_guards:
            d = np.maximum(d, -(np.hypot(x - center[0], y - center[1]) - 140))
        return d

    def cavity(self, x, y, z):
        h = self.horizontal(x, y, z)
        p = self.phase
        # Tanh avoids flat clipped maxima/minima. Broad nonparallel folds
        # create an uneven continuous crown across entire large rooms.
        wave = (61 * np.sin(x * (2 * np.pi / 773) + y * (2 * np.pi / 1217) + p)
                + 32 * np.sin(y * (2 * np.pi / 557) - x * (2 * np.pi / 1487) + p * 1.7)
                + 17 * np.sin(x * (2 * np.pi / 389) + y * (2 * np.pi / 683) + p * 2.3))
        full_crown = 9085 + 83 * np.tanh(wave / 65)
        # Around side walls, the upper cavity transitions into an arch shoulder
        # near9000 instead of meeting a flat ceiling at a right angle.
        arch = np.sqrt(np.clip(-h / 205, 0, 1))
        crown = 9005 + (full_crown - 9005) * arch
        inward_from_entry = (y - self.entry[1]) * -self.direction
        entry_weight = np.exp(-np.maximum(inward_from_entry, 0) / 210) * np.exp(-np.abs(x - self.entry[0]) / 320)
        crown = crown * (1 - entry_weight) + (9163 + 4 * np.sin(x / 179 + y / 313)) * entry_weight
        top = smooth_max(h, z - (crown - FLOOR_Z), 110)
        # A continuous new floor8609 lies1cm below preserved legacy8610 route
        # colliders. This closes the rock volume and covers outward alcoves.
        return np.maximum(top, (8609 - FLOOR_Z) - z)

    def __call__(self, x, y, z):
        r = self.outer_rect
        exterior = rounded_box2(x, y, (r[:2] + r[2:]) / 2, (r[2:] - r[:2]) / 2, 150)
        outer = np.maximum(exterior, np.maximum((8490 - FLOOR_Z) - z, z - (9590 - FLOOR_Z)))
        return np.maximum(outer, -self.cavity(x, y, z))


def route_samples(field):
    pts = []
    for a, b in field.edges:
        tangent = (b - a) / np.linalg.norm(b - a)
        side = np.asarray([-tangent[1], tangent[0]])
        for t in np.linspace(0, 1, 21):
            center = a + (b - a) * t
            for lateral in (-120, 0, 120):
                xy = center + side * lateral
                for z in (96, 192, 232):
                    pts.append([*xy, z])
    pts = np.asarray(pts)
    # Floor is deliberately excluded from horizontal radius clearance; the
    # floor support is tested separately and source triangles are audited by QA.
    values = field.horizontal(pts[:, 0], pts[:, 1], pts[:, 2])
    return {"samples": len(pts), "maximum_horizontal_cavity_sdf_cm": float(values.max()),
            "required_route_capsule_radius_cm": 42, "all_samples_have_radius42_margin": bool(values.max() < -42)}


def build(site, output):
    field = SculptField(site)
    r = field.outer_rect
    xs = np.arange(r[0] - PITCH, r[2] + PITCH * 1.5, PITCH, dtype=np.float32)
    ys = np.arange(r[1] - PITCH, r[3] + PITCH * 1.5, PITCH, dtype=np.float32)
    zs = np.arange(8490 - FLOOR_Z - PITCH, 9590 - FLOOR_Z + PITCH * 1.5, PITCH, dtype=np.float32)
    volume = field(xs[:, None, None], ys[None, :, None], zs[None, None, :]).astype(np.float32)
    vertices, faces, _, _ = marching_cubes(volume, 0, spacing=(PITCH,) * 3, allow_degenerate=False)
    vertices += np.asarray([xs[0], ys[0], zs[0]])
    mesh = trimesh.Trimesh(vertices, faces, process=True)
    mesh.fix_normals(multibody=True)
    raw_count = len(mesh.faces)
    if raw_count > MAX_TRIANGLES:
        mesh = mesh.simplify_quadric_decimation(face_count=MAX_TRIANGLES, aggression=3)
    mesh.update_faces(mesh.nondegenerate_faces())
    mesh.merge_vertices(digits_vertex=2)
    mesh.update_faces(mesh.nondegenerate_faces())
    mesh.remove_unreferenced_vertices()
    mesh.fix_normals(multibody=True)
    mesh.vertices = np.round(mesh.vertices, 6)
    audit = mesh_audit(mesh)
    assert audit["watertight"] and audit["winding_consistent"] and audit["all_edges_incidence_two"] and audit["connected_bodies"] == 1, audit
    assert audit["positive_signed_volume"] and audit["degenerate_triangles"] == 0, audit
    routes = route_samples(field)
    assert routes["all_samples_have_radius42_margin"], routes
    face_z = mesh.vertices[mesh.faces][:, :, 2]
    floor_faces = (mesh.face_normals[:, 2] > .9) & np.all(np.abs(face_z + 1) < .02, axis=1)
    # Quadric collapses may move a planar-floor boundary vertex by a hundredth
    # centimeter. Snap the complete authored floor section to its exact plane;
    # do not widen the independent8609cm requirement to accept simplifier drift.
    floor_vertices = np.unique(mesh.faces[floor_faces])
    snapped = mesh.vertices.copy()
    snapped[floor_vertices, 2] = -1.0
    mesh.vertices = snapped
    mesh.fix_normals(multibody=True)
    audit = mesh_audit(mesh)
    assert audit["watertight"] and audit["winding_consistent"] and audit["all_edges_incidence_two"] and audit["connected_bodies"] == 1, audit
    assert audit["positive_signed_volume"] and audit["degenerate_triangles"] == 0, audit
    material_ids = floor_faces.astype(np.uint8)
    assert int(floor_faces.sum()) > 0
    sid = site["site_id"]
    name = f"SM_CaveOrganicSculpt_{sid}_20261118"
    obj = output / (name + ".obj")
    with obj.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# Original Rogue10m V2 continuous rock sculpt. XYZ cm/Z-up, floor localZ=-1.\n")
        stream.write(f"o {name}\n")
        for v in mesh.vertices:
            stream.write("v %.6f %.6f %.6f\n" % tuple(v))
        for n in mesh.vertex_normals:
            stream.write("vn %.7f %.7f %.7f\n" % tuple(n))
        stream.write("s 1\n")
        previous = None
        for face, material_id in zip(mesh.faces + 1, material_ids):
            if material_id != previous:
                stream.write("usemtl " + ("OrganicFloor" if material_id else "OrganicRock") + "\n")
                previous = material_id
            stream.write("f " + " ".join(f"{i}//{i}" for i in face) + "\n")
    indexed = output / (name + ".mesh.json")
    write_json(indexed, {"schema": "Rogue10m.OrganicCaveIndexedMesh.20261118.v2", "site_id": sid,
                         "coordinate_system": "local_XYZ_centimeters_Z_up", "world_pivot_cm": field.origin.tolist(),
                         "vertices_cm": mesh.vertices.tolist(), "triangles": mesh.faces.tolist(),
                         "triangle_material_ids": material_ids.tolist(), "material_slots": ["OrganicRock", "OrganicFloor"]})
    barriers = []
    for a, b in site["forced_closed_edges"]:
        mid = (cell_center(a) + cell_center(b)) / 2
        values = [float(field.horizontal(mid[0], mid[1], z - FLOOR_Z)) for z in (8650, 8842, 9000)]
        barriers.append({"cells": [a, b], "horizontal_cavity_sdf_cm_at_z8650_8842_9000": values, "closed": min(values) >= 40})
    assert all(b["closed"] for b in barriers), barriers
    return {"site_id": sid, "world_pivot_cm": field.origin.tolist(),
            "source_obj": str(obj.relative_to(ROOT)).replace("\\", "/"), "source_obj_sha256": digest(obj),
            "indexed_mesh": str(indexed.relative_to(ROOT)).replace("\\", "/"), "indexed_mesh_sha256": digest(indexed),
            "raw_marching_cubes_triangles": raw_count, "mesh_audit": audit, "field_route_margin": routes,
            "floor_triangles": int(floor_faces.sum()), "floor_world_z_cm": 8609,
            "forced_closed_barriers": barriers, "diagonal_guard_count": len(field.diagonal_guards),
            "source_graph_edges": site["graph_edges"], "source_runtime_routes_ground_cm": site["runtime_routes"],
            "editor_validation_status": "not_imported_not_runtime_validated"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=SOURCE)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8-sig"))
    spec = {"schema": "Rogue10m.OrganicCaveGeometrySpec.20261118.v2", "generator": str(Path(__file__).relative_to(ROOT)),
            "generator_sha256": digest(Path(__file__)), "retained_math_helpers_script": "Scripts/OrganicCaveMesh20261118.py",
            "retained_math_helpers_sha256": digest(ROOT / "Scripts/OrganicCaveMesh20261118.py"), "source_plan_sha256": digest(PLAN),
            "authorship": "Original analytic Rogue10m rock sculpt, no external mesh/texture copied.",
            "previous_candidate": "geometryspec.json; visually rejected before Unreal import",
            "units": "centimeters", "marching_cubes_pitch_cm": PITCH, "maximum_triangles_per_site": MAX_TRIANGLES,
            "preserved_floor_collision_world_z_cm": 8610, "new_continuous_visible_floor_world_z_cm": 8609,
            "preserved_grass_roof_world_top_z_cm": 9602, "new_rock_outer_cap_world_z_cm": 9590,
            "room_domain_warp_max_component_cm": 105, "ordinary_camera_plane_corridor_radius_cm": [235, 295],
            "secret_partition_width_cm": 80, "secret_partition_clearance_exception_cm": 210,
            "maximum_pit_void_expansion_cm": 120, "pit_limit_exception": "existing entrance ramp bore through original roof lip",
            "upper_wall_broad_relief_amplitude_cm": 78, "ceiling_center_world_z_cm": [9002, 9168],
            "protected_landscape_composite_blake3": "67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc",
            "import_contract": {"source_coordinates": "local XYZ centimeters/Z-up", "actor_pivot": "site.world_pivot_cm",
                                "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1], "collision": "UseComplexAsSimple",
                                "material_slots": ["OrganicRock", "OrganicFloor"], "floor_material": "reuse current MI_CaveStoneFloor",
                                "import_materials": False, "import_textures": False, "OBJ": "usemtl groups; intentionally no MTL/texture sidecars"},
            "sites": [build(s, args.output) for s in plan["sites"]],
            "verification_boundary": "Pure source only; actual imported art, capsule/camera and cold validation required."}
    write_json(EVIDENCE / "geometryspec-v2.json", spec)
    print(json.dumps({"geometry_spec": str(EVIDENCE / "geometryspec-v2.json"), "sites": [{"id": s["site_id"], "audit": s["mesh_audit"], "floor_triangles": s["floor_triangles"], "route_margin": s["field_route_margin"]} for s in spec["sites"]]}, indent=2))


if __name__ == "__main__":
    main()
