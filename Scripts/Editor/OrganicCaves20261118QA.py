"""Independent organic-cave QA primitives. Import is inert and Unreal-free.

The old cases are immutable inputs, not a claim that replaced cube walls remain
visible/collidable. Explicit callers supply the new shell actor/component paths
and observed source-triangle contact proof. This module never edits or saves.
"""
from __future__ import annotations

from collections import Counter, defaultdict
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys
import traceback

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT / "Feature/doc/evidence/caves20261118"
PLAN_SHA256 = "39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
VALIDATOR_SHA256 = "58b6d50a67b52822000f8a778b0ae120096405264056155cea745774720106fd"
OBSERVER_SHA256 = "806d2bca47505dc9da5f99dd8ec4d92c952155e5f74231177fb0a601962d47ae"
COLD_SHA256 = "cd58b38cddbade972ca4bd7c787deccf6bb7ef92ad6d0aecc8da48d0327937f6"
CASES_SHA256 = "2d0bdc07e7d4eca5a49a5df87d9ad8ff1b3eb3982913fc69d83fa5cf21baecab"
VECTORS_SHA256 = "fde56356bf26d7acad43015455ca09a8faab47c6f7b00598606d90038d3820c4"
ROUTE_SHA256 = "3aaec11cdfc1676699571aa4fdd9db52090e6da5b06fa79fa4b3281866268890"
EXPECTED_KINDS = {"route_clear": 1752, "floor_support": 112, "ceiling": 112,
                  "ramp_support": 10, "wall": 20, "surface_roof": 10}
NATIVE_WINDING_ADAPTER = {"source_convention": "Artist CCW with outward mathematical cross-product normals",
    "native_convention": "UE CW; TriangleNormal=(V2-V0) cross (V1-V0)",
    "triangle_index_mapping": [0, 2, 1], "positions_changed": False,
    "authored_normals_changed": False, "source_artwork_changed": False}
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
BASELINE_PINS = {
    "cave-actors.json": "fff169e2b9fc5ce1993cd0fa8372c038917fb87ee247179761d578a2a8f11ccb",
    "materials.json": "de2e85d4b23ae4ad014d88bcd0bf54d248722ee854e19f808744e5bf71a77afb",
    "result.json": "e81bca59acc743213e58880b1a115c78e6a7338768a14238944c727ae3cfd28d",
    "terrain.json": "a9a87d9ed7aa94c1366d0a9d802a262c598e319c56d188bae64a7c43dfe7be2f",
}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
        separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def pinned_module(name, relative, expected):
    file = ROOT / relative
    require(hashlib.sha256(file.read_bytes()).hexdigest() == expected,
            "Frozen predecessor changed: " + relative)
    spec = importlib.util.spec_from_file_location(name, file)
    module = importlib.util.module_from_spec(spec)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = old
    return module


def frozen_contract():
    """Return the exact original 2016 queries and 276 runtime waypoints."""
    require(hashlib.sha256((OLD / "plan.json").read_bytes()).hexdigest() == PLAN_SHA256,
            "Frozen cave plan changed")
    plan = json.loads((OLD / "plan.json").read_text(encoding="utf-8-sig"))
    validator = pinned_module("organic_predecessor_cases", "Scripts/Editor/ValidateCaves20261118.py", VALIDATOR_SHA256)
    observer = pinned_module("organic_predecessor_camera", "Scripts/Editor/ObserveCaveTraversalCamera20261118.py", OBSERVER_SHA256)
    cases, coverage = validator.build_probe_cases(plan)
    routes, length = observer.route_plan(plan)
    vectors = [{k: case[k] for k in ("id", "kind", "site", "start", "end", "radius")}
               | {"half_height_cm": 96.0} for case in cases]
    require(len(cases) == 2016 and digest(cases) == CASES_SHA256
            and digest(vectors) == VECTORS_SHA256 and dict(Counter(c["kind"] for c in cases)) == EXPECTED_KINDS,
            "Original query case identity/vector/radius/halfheight changed")
    require(len(routes) == 276 and digest(routes) == ROUTE_SHA256
            and [i for i, row in enumerate(routes) if row["capture"]] == [10, 12, 77, 149, 151, 219],
            "Original runtime waypoints or six capture points changed")
    return {"plan": plan, "cases": cases, "coverage": coverage, "routes": routes,
            "route_length_cm": length, "case_sha256": CASES_SHA256,
            "query_vectors_sha256": VECTORS_SHA256, "route_sha256": ROUTE_SHA256}


def _sub(a, b):
    return [x-y for x, y in zip(a, b)]


def _dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def _cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def native_ue_triangles(artist_triangles):
    """Explicit approved artist-CCW to native-UE-CW adapter; no source edits."""
    return [[a, c, b] for a, b, c in artist_triangles]


def _point_triangle_distance(point, a, b, c):
    """Independent closest-point regions; all inputs are centimetres."""
    ab, ac, ap = _sub(b, a), _sub(c, a), _sub(point, a)
    d1, d2 = _dot(ab, ap), _dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        return math.dist(point, a)
    bp = _sub(point, b)
    d3, d4 = _dot(ab, bp), _dot(ac, bp)
    if d3 >= 0 and d4 <= d3:
        return math.dist(point, b)
    vc = d1*d4-d3*d2
    if vc <= 0 and d1 >= 0 and d3 <= 0:
        t = d1/(d1-d3)
        return math.dist(point, [a[i]+t*ab[i] for i in range(3)])
    cp = _sub(point, c)
    d5, d6 = _dot(ab, cp), _dot(ac, cp)
    if d6 >= 0 and d5 <= d6:
        return math.dist(point, c)
    vb = d5*d2-d1*d6
    if vb <= 0 and d2 >= 0 and d6 <= 0:
        t = d2/(d2-d6)
        return math.dist(point, [a[i]+t*ac[i] for i in range(3)])
    va = d3*d6-d5*d4
    if va <= 0 and d4-d3 >= 0 and d5-d6 >= 0:
        t = (d4-d3)/((d4-d3)+(d5-d6))
        return math.dist(point, [b[i]+t*(c[i]-b[i]) for i in range(3)])
    denominator = va+vb+vc
    require(abs(denominator) > 1e-12, "Degenerate source triangle")
    v, w = vb/denominator, vc/denominator
    return math.dist(point, [a[i]+v*ab[i]+w*ac[i] for i in range(3)])


def make_source_contact_check(meshes_by_site):
    """Create a bounded spatial-indexed independent triangle contact witness.

    Each site needs vertices_cm, triangles and pivot_world_cm. Input geometry
    should be hash-pinned separately and the imported asset geometry compared
    to it; this callback alone cannot prove an import used the intended file.
    """
    require(set(meshes_by_site) == {"Q", "Z"}, "Two source shell meshes required")
    data = {}
    cell = 80.0
    for site, mesh in meshes_by_site.items():
        vertices, triangles, pivot = mesh["vertices_cm"], mesh["triangles"], mesh["pivot_world_cm"]
        require(len(pivot) == 3 and all(math.isfinite(x) for x in pivot), "Finite source pivot required")
        buckets = defaultdict(list)
        for index, tri in enumerate(triangles):
            points = [vertices[v] for v in tri]
            ranges = [range(math.floor((min(p[axis] for p in points)-3)/cell),
                            math.floor((max(p[axis] for p in points)+3)/cell)+1) for axis in range(3)]
            require(math.prod(len(r) for r in ranges) <= 1000, "Source triangle unexpectedly spans a large spatial index")
            for x in ranges[0]:
                for y in ranges[1]:
                    for z in ranges[2]:
                        buckets[(x, y, z)].append(index)
        data[site] = (mesh, buckets)

    def check(site, world_point, observed_normal, kind):
        mesh, buckets = data[site]
        point = _sub(world_point, mesh["pivot_world_cm"])
        candidates = buckets.get(tuple(math.floor(x/cell) for x in point), ())
        nearest = None
        matched = None
        for index in candidates:
            a, b, c = [mesh["vertices_cm"][i] for i in mesh["triangles"][index]]
            distance = _point_triangle_distance(point, a, b, c)
            face = _cross(_sub(b, a), _sub(c, a))
            length = math.sqrt(_dot(face, face))
            if length <= 1e-12:
                continue
            normal_dot = _dot([x/length for x in face], observed_normal)
            row = {"triangle_index": index, "surface_distance_cm": distance, "oriented_face_normal_dot": normal_dot}
            if nearest is None or (distance, -normal_dot) < (nearest["surface_distance_cm"], -nearest["oriented_face_normal_dot"]):
                nearest = row
            if distance <= 3.0 and normal_dot >= .99 and (matched is None or distance < matched["surface_distance_cm"]):
                matched = row
        return {"passed": matched is not None,
                "kind": kind, "nearest_source_triangle": nearest, "candidates": len(candidates),
                "matched_oriented_source_triangle": matched,
                "source": "Independent closest point on indexed source triangles in centimetres"}
    return check


def audit_indexed_shell(vertices, triangles):
    """Audit raw welded source indices, without trusting generator statistics.

    A closed rock-solid boundary may have an open walking entrance while its
    rock volume remains closed. Require exactly two oppositely wound faces per
    edge and one connected triangle component; this is not self-intersection QA.
    """
    require(vertices and triangles, "Empty shell")
    require(all(len(v) == 3 and all(math.isfinite(float(x)) for x in v) for v in vertices),
            "Nonfinite or malformed vertex")
    require(all(len(t) == 3 and all(isinstance(i, int) and 0 <= i < len(vertices) for i in t)
                and len(set(t)) == 3 for t in triangles), "Invalid triangle indices")
    edges = defaultdict(list)
    signed_volume = 0.0
    minimum_area = float("inf")
    canonical_faces = set()
    duplicate_faces = []
    degenerates = []
    for index, tri in enumerate(triangles):
        a, b, c = [vertices[i] for i in tri]
        normal = _cross(_sub(b, a), _sub(c, a))
        area = math.sqrt(_dot(normal, normal)) / 2
        minimum_area = min(minimum_area, area)
        if area <= 1e-6:
            degenerates.append(index)
        signed_volume += _dot(a, _cross(b, c)) / 6
        key = tuple(sorted(tri))
        if key in canonical_faces:
            duplicate_faces.append(index)
        canonical_faces.add(key)
        for u, v in zip(tri, (*tri[1:], tri[0])):
            edges[tuple(sorted((u, v)))].append((index, u < v))
    boundary = [e for e, rows in edges.items() if len(rows) == 1]
    nonmanifold = [e for e, rows in edges.items() if len(rows) > 2]
    inconsistent = [e for e, rows in edges.items() if len(rows) == 2 and rows[0][1] == rows[1][1]]
    adjacency = defaultdict(list)
    for rows in edges.values():
        if len(rows) == 2:
            a, b = rows[0][0], rows[1][0]
            adjacency[a].append(b)
            adjacency[b].append(a)
    unseen = set(range(len(triangles)))
    components = []
    while unseen:
        todo = [unseen.pop()]
        size = 0
        while todo:
            index = todo.pop()
            size += 1
            for neighbour in adjacency[index]:
                if neighbour in unseen:
                    unseen.remove(neighbour)
                    todo.append(neighbour)
        components.append(size)
    used = set(i for t in triangles for i in t)
    checks = {"closed_two_faces_per_edge": not boundary and not nonmanifold,
              "consistent_face_winding": not inconsistent,
              "no_degenerate_faces": not degenerates, "no_duplicate_faces": not duplicate_faces,
              "one_connected_surface": len(components) == 1, "no_orphan_vertices": len(used) == len(vertices),
              "finite_nonzero_signed_volume": math.isfinite(signed_volume) and abs(signed_volume) > 1}
    return {"passed": all(checks.values()), "checks": checks, "vertices": len(vertices),
            "triangles": len(triangles), "edges": len(edges), "component_triangle_counts": components,
            "boundary_edges": len(boundary), "nonmanifold_edges": len(nonmanifold),
            "inconsistent_edges": len(inconsistent), "degenerate_faces": degenerates[:20],
            "duplicate_faces": duplicate_faces[:20], "minimum_face_area_cm2": minimum_area,
            "signed_volume_cm3": signed_volume,
            "limitations": ["No global triangle self-intersection certification; actual collision and rendered interior QA are required."]}


def evaluate_capsule_results(contract, results, shell_targets, source_contact_check):
    """Judge unchanged queries; intentional hit-target changes are explicit.

    `source_contact_check(site, impact, normal, kind)` must independently prove
    actual wall/ceiling impact lies on the imported shell's source triangles.
    It returns a JSON record with passed=true, not an unchecked boolean. Exact
    floor/ramp/grass-roof heights retain the previous 3cm/0.71 tolerances.
    """
    cases = contract["cases"]
    require(len(results) == len(cases) == 2016, "All original queries required")
    require(set(shell_targets) == {"Q", "Z"}, "Explicit Q/Z shell targets required")
    sites = {s["site_id"]: s for s in contract["plan"]["sites"]}
    rows = []
    for case, result in zip(cases, results):
        require(result.get("success") and result.get("read_only") and result.get("dirty_package_names_unchanged"),
                "Native query is not proven read-only: " + case["id"])
        require(result.get("trace_complex") is False and result.get("channel") == "ECC_Pawn"
                and result.get("radius_cm") == case["radius"] and result.get("half_height_cm") == 96.0
                and result.get("start_cm") == case["start"] and result.get("end_cm") == case["end"],
                "Observed query vectors/radius/halfheight/channel differ: " + case["id"])
        hit = result.get("hit") or {}
        passed = result.get("blocking_hit") is case["expected_blocking"]
        contact = None
        if case["expected_blocking"]:
            passed = passed and hit.get("start_penetrating") is False
            impact, normal = hit.get("impact_point_cm", []), hit.get("impact_normal", [])
            passed = passed and len(impact) == len(normal) == 3 and all(math.isfinite(x) for x in impact + normal)
            if case["kind"] in {"wall", "ceiling"}:
                target = shell_targets[case["site"]]
                passed = passed and hit.get("actor") == target["actor_path"] and hit.get("component") == target["component_path"]
                if len(impact) == len(normal) == 3:
                    contact = source_contact_check(case["site"], impact, normal, case["kind"])
                    passed = passed and isinstance(contact, dict) and contact.get("passed") is True
                    if case["kind"] == "ceiling":
                        passed = passed and normal[2] < 0
            else:
                site = sites[case["site"]]
                expected_z = case.get("impact_z")
                if case["kind"] == "ramp_support" and len(impact) == 3:
                    r = site["ramp"]
                    expected_z = 9600.0-1000.0*(impact[1]-r["top_y_cm"])/(r["bottom_y_cm"]-r["top_y_cm"])
                passed = passed and hit.get("actor") == shell_targets[case["site"]]["preserved_structure_actor_path"]
                passed = passed and len(impact) == 3 and abs(impact[2]-expected_z) <= 3.0 and normal[2] >= .71
        rows.append({"case_id": case["id"], "kind": case["kind"], "passed": bool(passed), "source_contact": contact})
    return {"passed": all(r["passed"] for r in rows), "case_count": len(rows),
            "query_vectors_sha256": VECTORS_SHA256, "intentional_expectation_changes":
            ["20 wall hits target continuous shell", "112 ceiling contacts target curved source shell; old flat9200 assumption retired"],
            "failed_cases": [r["case_id"] for r in rows if not r["passed"]], "results": rows}


def public_camera_sphere(u, world, start, end, *, actors_to_ignore=(), trace_complex=True):
    """Actual world Camera-channel geometry, never hidden source-plan OBBs."""
    require(len(start) == len(end) == 3 and all(math.isfinite(x) for x in list(start)+list(end)),
            "Finite world trace required")
    hit = u.SystemLibrary.sphere_trace_single(world, u.Vector(*start), u.Vector(*end), 14.0,
        u.TraceTypeQuery.TRACE_TYPE_QUERY2, trace_complex, list(actors_to_ignore), u.DrawDebugTrace.NONE, True)
    fields = hit.to_tuple() if hit is not None else None
    if not fields:
        return {"blocking_hit": False, "radius_cm": 14.0, "trace_complex": trace_complex}
    require(len(fields) >= 11, "Unexpected public HitResult tuple schema")
    vector = lambda value: [float(value.x), float(value.y), float(value.z)]
    return {"blocking_hit": bool(fields[0]), "start_penetrating": bool(fields[1]), "fraction": float(fields[2]),
            "distance_cm": float(fields[3]), "location_cm": vector(fields[4]), "impact_point_cm": vector(fields[5]),
            "normal": vector(fields[6]), "impact_normal": vector(fields[7]),
            "actor_path": fields[9].get_path_name() if fields[9] else None,
            "component_path": fields[10].get_path_name() if fields[10] else None,
            "radius_cm": 14.0, "trace_complex": trace_complex, "channel": "ECC_Camera"}


def audit_original_camera_arc():
    """Re-run all1440 original failure-pivot rays against actual new geometry.

    Test both the spring arm's simple query and independent complex query.
    This is a finite fixed-pivot regression, not an all-position camera claim.
    """
    import unreal as u
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    require(not editor.get_game_world() and editor.get_editor_world().get_path_name().split(".")[0] == TARGET,
            "Exact idle candidate editor required for fixed camera arc")
    old_failure = OLD / "runtime-camera-final/result.json"
    require(hashlib.sha256(old_failure.read_bytes()).hexdigest() == "71a7e0959a7f5bba1e49afaf65cb18fa0cb0d9900b3d8485f05b454dd6c028c1",
            "Retained original camera failure sample changed")
    sample = json.loads(old_failure.read_text(encoding="utf-8-sig"))["worst_camera_sample"]
    dirty = lambda: sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
    before = dirty()
    pivot = sample["pivot_cm"]
    delta = _sub(sample["unfixed_camera_cm"], pivot)
    yaw = math.radians(sample["camera_rotation_pitch_yaw_roll"][1])
    c, s = math.cos(yaw), math.sin(yaw)
    local = [c*delta[0]+s*delta[1], -s*delta[0]+c*delta[1], delta[2]]
    desired_distance = math.sqrt(_dot(delta, delta))
    rows = []
    for index in range(1440):
        angle = index*.25
        radians = math.radians(angle)
        c, s = math.cos(radians), math.sin(radians)
        end = [pivot[0]+c*local[0]-s*local[1], pivot[1]+s*local[0]+c*local[1], pivot[2]+local[2]]
        simple = public_camera_sphere(u, editor.get_editor_world(), pivot, end, trace_complex=False)
        complex_hit = public_camera_sphere(u, editor.get_editor_world(), pivot, end, trace_complex=True)
        simple_distance = simple["distance_cm"] if simple["blocking_hit"] else desired_distance
        complex_distance = complex_hit["distance_cm"] if complex_hit["blocking_hit"] else desired_distance
        clear = all(not hit.get("start_penetrating", False) and distance >= 140 and desired_distance-distance <= 180
                    for hit, distance in ((simple, simple_distance), (complex_hit, complex_distance)))
        parity = simple["blocking_hit"] == complex_hit["blocking_hit"] and abs(simple_distance-complex_distance) <= .5
        rows.append({"yaw_degrees": angle, "passed": clear and parity, "default_simple": simple,
                     "independent_complex": complex_hit, "simple_distance_cm": simple_distance,
                     "complex_distance_cm": complex_distance, "simple_complex_distance_parity": parity})
    after = dirty()
    return {"schema": "Rogue10m.OrganicCaveActualCameraArc.20261118.v1", "read_only": True,
            "passed": all(r["passed"] for r in rows) and before == after, "angles": 1440,
            "actual_sphere_sweeps": 2880, "angular_step_degrees": .25, "probe_radius_cm": 14,
            "pivot_cm": pivot, "unfixed_distance_cm": desired_distance,
            "minimum_simple_camera_distance_cm": min(r["simple_distance_cm"] for r in rows),
            "minimum_complex_camera_distance_cm": min(r["complex_distance_cm"] for r in rows),
            "failed_angles": [r["yaw_degrees"] for r in rows if not r["passed"]],
            "dirty_before": before, "dirty_after": after, "cases": rows, "assets_saved": False,
            "camera_or_pawn_setters": False, "runtime_traversal_certified": False,
            "limitation": "One original fixed failure pivot; no global all-positions guarantee."}


def extract_static_mesh_geometry(u, mesh):
    """Read a source mesh into a transient DynamicMesh; never copy back/save."""
    dynamic = u.DynamicMesh()
    copied = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dynamic,
        u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
    require(isinstance(copied, tuple) and len(copied) == 2 and copied[0] == dynamic
            and copied[1] == u.GeometryScriptOutcomePins.SUCCESS, "Public static mesh source extraction failed")
    vertex_result = u.GeometryScript_MeshQueries.get_all_vertex_positions(dynamic, False)
    triangle_result = u.GeometryScript_MeshQueries.get_all_triangle_indices(dynamic, False)
    require(len(vertex_result) == len(triangle_result) == 3
            and vertex_result[0] == triangle_result[0] == dynamic
            and vertex_result[2] is False and triangle_result[2] is False,
            "Contiguous public source vertex/triangle lists required")
    vertex_array = u.GeometryScript_List.convert_vector_list_to_array(vertex_result[1])
    triangle_array = u.GeometryScript_List.convert_triangle_list_to_array(triangle_result[1])
    material_library = u.GeometryScript_Materials
    id_getters = [name for name in dir(material_library) if name.replace("_", "").lower() == "getalltrianglematerialids"]
    require(len(id_getters) == 1 and callable(getattr(material_library, id_getters[0])),
            "One public GetAllTriangleMaterialIDs Python binding required")
    material_result = getattr(material_library, id_getters[0])(dynamic)
    require(len(material_result) == 3 and material_result[0] == dynamic and material_result[2] is True,
            "Actual imported per-triangle section/material IDs required")
    material_ids = list(map(int, u.GeometryScript_List.convert_index_list_to_array(material_result[1])))
    vertices = [[float(v.x), float(v.y), float(v.z)] for v in vertex_array]
    triangles = [[int(v.x), int(v.y), int(v.z)] for v in triangle_array]
    require(dynamic.get_triangle_count() == len(triangles) == len(material_ids), "Copied triangle/material list counts differ")
    return vertices, triangles, material_ids


def compare_oriented_triangle_geometry(expected_vertices, expected_triangles, vertices, triangles,
                                      expected_material_ids=None, material_ids=None):
    """Exact oriented triangle/material multiset in MeshDescription float32.

    Source JSON retains six-decimal positions. UMeshDescription stores vertex
    positions as FVector3f, so raw-double lexical sorting can pair unrelated
    triangles when nominally tied coordinates round together. Normalize BOTH
    meshes to the installed storage precision before finding cyclic keys;
    retain the original coordinates for the separate0.05cm error measurement.
    Reversing a triangle, losing a face, or changing its material still fails.
    """
    require((expected_material_ids is None and material_ids is None)
            or (len(expected_material_ids) == len(expected_triangles) and len(material_ids) == len(triangles)),
            "Corresponding source/import triangle material IDs required")
    def f32_point(value):
        require(len(value) == 3 and all(math.isfinite(float(x)) for x in value), "Finite three-component mesh position required")
        return tuple(0.0 if x == 0 else x for x in struct.unpack("<fff", struct.pack("<fff", *value)))
    def buckets(vs, ts, ids):
        result = defaultdict(list)
        minimum_area = float("inf")
        degenerate = []
        normalized = [f32_point(value) for value in vs]
        for index, tri in enumerate(ts):
            require(len(tri) == 3 and all(isinstance(i, int) and 0 <= i < len(vs) for i in tri), "Valid mesh triangle indices required")
            points = tuple(normalized[i] for i in tri)
            raw = tuple(tuple(vs[i]) for i in tri)
            cycles = (points, points[1:]+points[:1], points[2:]+points[:2])
            shift = min(range(3), key=lambda i: cycles[i])
            key = (cycles[shift], ids[index] if ids is not None else 0)
            result[key].append(raw[shift:]+raw[:shift])
            a, b, c = points
            face = _cross(_sub(b, a), _sub(c, a))
            area = math.sqrt(_dot(face, face))/2
            minimum_area = min(minimum_area, area)
            if area <= 1e-6:
                degenerate.append(index)
        return result, minimum_area, degenerate
    expected, expected_area, expected_degenerate = buckets(expected_vertices, expected_triangles, expected_material_ids)
    actual, actual_area, actual_degenerate = buckets(vertices, triangles, material_ids)
    expected_counts = Counter({key: len(rows) for key, rows in expected.items()})
    actual_counts = Counter({key: len(rows) for key, rows in actual.items()})
    missing, added = expected_counts-actual_counts, actual_counts-expected_counts
    exact = not missing and not added and len(expected_triangles) == len(triangles)
    maximum, over_tolerance = 0.0, []
    for key in expected.keys() & actual.keys():
        for a, b in zip(sorted(expected[key]), sorted(actual[key])):
            error = max(math.dist(p, q) for p, q in zip(a, b))
            maximum = max(maximum, error)
            if error > .05:
                over_tolerance.append(key)
    def key_digest(counts):
        value = hashlib.sha256()
        for (points, material), count in sorted(counts.items()):
            value.update(struct.pack("<9fii", *(x for point in points for x in point), int(material), count))
        return value.hexdigest()
    return {"passed": bool(expected_triangles) and exact and not over_tolerance and not actual_degenerate and not expected_degenerate,
            "expected_triangles": len(expected_triangles), "actual_triangles": len(triangles),
            "position_comparison_precision": "Both meshes explicitly converted to FVector3f/IEEE754float32 before cyclic triangle keys",
            "exact_oriented_triangle_material_multiset_equal": exact,
            "expected_oriented_multiset_sha256": key_digest(expected_counts), "actual_oriented_multiset_sha256": key_digest(actual_counts),
            "missing_triangle_multiplicity": sum(missing.values()), "unexpected_triangle_multiplicity": sum(added.values()),
            "missing_keys_first20": [{"points": key[0], "material_id": key[1], "count": count} for key, count in list(missing.items())[:20]],
            "unexpected_keys_first20": [{"points": key[0], "material_id": key[1], "count": count} for key, count in list(added.items())[:20]],
            "maximum_corresponding_original_position_distance_cm": maximum,
            "position_error_over_tolerance_count": len(over_tolerance), "tolerance_cm": .05,
            "source_float32_minimum_face_area_cm2": expected_area, "actual_float32_minimum_face_area_cm2": actual_area,
            "source_float32_degenerate_faces_first20": expected_degenerate[:20],
            "actual_float32_degenerate_faces_first20": actual_degenerate[:20],
            "orientation_preserved": exact, "triangle_material_ids_compared": expected_material_ids is not None}


def validate_organic_material_recipe(u, material_path, floor_path, baseline_materials, reader):
    """Independent readback of the reviewed original43+3-node derivative."""
    prefix = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118"
    require(floor_path == prefix+"/MI_CaveStoneFloor.MI_CaveStoneFloor", "Preserved original floor material required")
    require(material_path == prefix+"/Organic20261118/MI_OrganicRock.MI_OrganicRock", "Scoped expected organic rock instance required")
    instance = u.load_asset(material_path)
    require(isinstance(instance, u.MaterialInstanceConstant), "Organic rock material instance required")
    parent = instance.get_editor_property("parent")
    require(isinstance(parent, u.Material) and parent.get_path_name() == prefix+"/Organic20261118/M_OrganicRock.M_OrganicRock",
            "Exact new organic parent required")
    lib = u.MaterialEditingLibrary
    before = baseline_materials[prefix+"/M_CaveSurface.M_CaveSurface"]["graph"]
    after = reader._fingerprint(u, lib, parent)
    require(len(before["nodes"]) == 43 and len(after["nodes"]) == 46
            and all(after["nodes"].get(k) == v for k, v in before["nodes"].items()), "Original43 derivative nodes changed")
    nodes = {n.get_name(): n for n in lib.get_material_expressions(parent)}
    additions = {key: nodes[key] for key in after["nodes"] if key not in before["nodes"]}
    by_class = {node.get_class().get_name(): node for node in additions.values()}
    require(len(additions) == len(by_class) == 3 and set(by_class) == {"MaterialExpressionVectorParameter",
            "MaterialExpressionMultiply", "MaterialExpressionConstant3Vector"}, "Exactly three reviewed rock expressions required")
    tint, multiply, normal = (by_class[k] for k in ("MaterialExpressionVectorParameter", "MaterialExpressionMultiply", "MaterialExpressionConstant3Vector"))
    require(str(tint.get_editor_property("parameter_name")) == "OrganicTint", "Exact OrganicTint parameter required")
    expected_tint = [.30, .32, .28, 1.0]
    tint_value = tint.get_editor_property("default_value")
    normal_value = normal.get_editor_property("constant")
    require(all(abs(float(getattr(tint_value, k))-v) <= 1e-6 for k, v in zip("rgba", expected_tint))
            and all(abs(float(getattr(normal_value, k))-v) <= 1e-6 for k, v in zip("rgba", [0, 0, 1, 1]))
            and abs(float(multiply.get_editor_property("const_a"))) <= 1e-6
            and abs(float(multiply.get_editor_property("const_b"))-1) <= 1e-6, "Reviewed new expression constants differ")
    reader._require_inputs(lib, parent, multiply, {"A": nodes["MaterialExpressionAdd_4"], "B": tint})
    reader._require_inputs(lib, parent, tint, {})
    reader._require_inputs(lib, parent, normal, {})
    require(lib.get_material_property_input_node(parent, u.MaterialProperty.MP_BASE_COLOR) == multiply
            and lib.get_material_property_input_node(parent, u.MaterialProperty.MP_NORMAL) == normal,
            "New BaseColor and neutral Normal output connections differ")
    changed = {str(p.value) for p in (u.MaterialProperty.MP_BASE_COLOR, u.MaterialProperty.MP_NORMAL)}
    require({k: v for k, v in before["outputs"].items() if k not in changed}
            == {k: v for k, v in after["outputs"].items() if k not in changed}
            and not parent.get_editor_property("two_sided") and parent.get_editor_property("used_with_instanced_static_meshes"),
            "Other material outputs or original parent rendering flags changed")
    actual_tint = lib.get_material_instance_vector_parameter_value(instance, "OrganicTint")
    roughness = float(lib.get_material_instance_scalar_parameter_value(instance, "Roughness"))
    require(all(abs(float(getattr(actual_tint, k))-v) <= 1e-6 for k, v in zip("rgba", expected_tint))
            and abs(roughness-.92) <= 1e-6, "Exact OrganicTint and Roughness instance values differ")
    return {"passed": True, "parent": parent.get_path_name(), "instance": material_path, "source_nodes": 43,
            "new_nodes": 3, "actual_nodes": 46, "graph_sha256": digest(after),
            "world_triplanar_cm": 240, "neutral_tangent_normal": [0, 0, 1], "roughness": roughness}


def validate_organic_caves(config, require_saved_clean=False):
    """Full read-only live/cold regression; no save, spawn, selection or PIE.

    config = {geometry_spec_sha256, expected_mesh_paths:{Q,Z}, expected_material_path, expected_floor_material_path}.
    The owner must pin/review this explicit config for the owned cold process.
    """
    report = {"schema": "Rogue10m.OrganicCaveValidation.20261118.v1", "passed": False,
              "checks": {}, "diagnostics": [], "assets_saved": False, "read_only": True,
              "config": config, "config_sha256": digest(config)}
    report["native_winding_adapter"] = dict(NATIVE_WINDING_ADAPTER)
    def check(name, passed, details=None):
        report["checks"][name] = bool(passed)
        if not passed:
            report["diagnostics"].append({"check": name, "details": details})
    try:
        require(set(config) == {"geometry_spec_sha256", "expected_mesh_paths", "expected_material_path", "expected_floor_material_path"}
                and set(config["expected_mesh_paths"]) == {"Q", "Z"}, "Exact explicit validation config required")
        baseline = {}
        for name, expected in BASELINE_PINS.items():
            file = BASE / "baseline-live" / name
            require(hashlib.sha256(file.read_bytes()).hexdigest() == expected, "Immutable new baseline changed: " + name)
            baseline[name] = json.loads(file.read_text(encoding="utf-8-sig"))
        file = OLD / "baseline-live/baseline-actors.json"
        require(hashlib.sha256(file.read_bytes()).hexdigest() == "f5390322017e3a501811a105f72686c11e90a8c0ed662484aee43404569eacf5",
                "Immutable original1404 actor baseline changed")
        old_baseline = json.loads(file.read_text(encoding="utf-8-sig"))
        geometry_file = BASE / "geometryspec-v2.json"
        require(hashlib.sha256(geometry_file.read_bytes()).hexdigest() == config["geometry_spec_sha256"],
                "Explicit reviewed geometry spec changed")
        geometry = json.loads(geometry_file.read_text(encoding="utf-8-sig"))
        require(geometry["schema"] == "Rogue10m.OrganicCaveGeometrySpec.20261118.v2"
                and geometry["source_plan_sha256"] == PLAN_SHA256 and len(geometry["sites"]) == 2,
                "Exact two-site original cave plan geometry required")
        frozen = frozen_contract()
        import unreal as u
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        require(Path(u.Paths.project_dir()).resolve() == ROOT and str(u.SystemLibrary.get_engine_version()).startswith("5.8.")
                and editor.get_editor_world().get_path_name().split(".")[0] == TARGET and not editor.get_game_world(),
                "Exact idle candidate UE5.8 editor required")
        api = pinned_module("organic_dressing_api", "Scripts/Editor/NaturalWorldDressing20261117.py",
            "10bfccf425157cd1676b2c75a0347e815b3426dfa77815ec5c98edb7cdaaf280")
        actor_reader = pinned_module("organic_actor_reader", "Scripts/Editor/AuthorNaturalBossThreshold20261118.py",
            "62fc86c81de9b997fc741cc73510321164a35a4bc94cb1e44242cd663a9f43cc")
        inspector = pinned_module("organic_site_inspector", "Scripts/Editor/InspectCaveSites20261118.py",
            "55c01c8ab8ec453ac2454242a1db3998c6518120eeb225828e87741854fa5ace")
        material_reader = pinned_module("organic_material_reader", "Scripts/Editor/NaturalRoadMaterial20261117.py",
            "66ebc1c58cd9abddf14739c7624e302b09466a4b2702d5ba63fb98daedf7fb03")
        predecessor = pinned_module("organic_previous_validator", "Scripts/Editor/ValidateCaves20261118.py", VALIDATOR_SHA256)
        dirty_before = api._dirty(u)
        report["dirty_packages_before"] = dirty_before
        if require_saved_clean:
            require(not dirty_before, "Cold saved-clean candidate required")
        actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
        by_path = {a.get_path_name(): a for a in actors}
        old_paths = set(old_baseline["actors"])
        cave_paths = set(baseline["cave-actors.json"]["actors"])
        require(len(actors) == len(by_path) == 1408 and set(by_path) == old_paths | cave_paths,
                "Exact original1404 plus existingfour cave actors required")
        check("exact1408_actor_identities_preserved_no_new_actors", True)
        old_actors = [by_path[p] for p in sorted(old_paths)]
        normalize = predecessor.normalized_addresses
        check("all_original1404_actor_and_instance_snapshots_preserved", normalize(actor_reader._snapshot_actors(u, old_actors, api))
              == normalize(old_baseline["actors"]))
        check("all_original1404_persistent_components_preserved", normalize(inspector.persistent_components(u, old_actors))
              == normalize(old_baseline["persistent_components"]))
        caves = [by_path[p] for p in sorted(cave_paths)]
        labels = {a.get_actor_label(): a for a in caves}
        expected_caves = copy.deepcopy(baseline["cave-actors.json"]["actors"])
        expected_components = copy.deepcopy(baseline["cave-actors.json"]["components"])
        retired_paths = set()
        retired_structure_paths = set()
        hidden_floor_paths = set()
        new_paths = set()
        ramp_paths = set()
        targets = {}
        for site in ("Q", "Z"):
            structure, dressing = labels["CV18_"+site+"_Structure"], labels["CV18_"+site+"_Dressing"]
            retired_paths.add(structure.get_path_name()+".CV18_"+site+"_ISM_01")
            retired_structure_paths.add(structure.get_path_name()+".CV18_"+site+"_ISM_01")
            hidden_floor_paths.add(structure.get_path_name()+".CV18_"+site+"_ISM_00")
            retired_paths.add(dressing.get_path_name()+"."+site+"_CaveDressingISM_00")
            component_path = dressing.get_path_name()+".Organic_"+site+"_Shell"
            new_paths.add(component_path)
            ramp_paths.add(dressing.get_path_name()+".Organic_"+site+"_RampVisual")
            targets[site] = {"actor_path": dressing.get_path_name(), "component_path": component_path,
                             "preserved_structure_actor_path": structure.get_path_name()}
        for actor in expected_caves.values():
            for row in actor["primitives"]:
                if row["path"] in retired_paths:
                    row.update(visible=False, hidden_in_game=True, collision_enabled=str(u.CollisionEnabled.NO_COLLISION))
                    if row["path"] in retired_structure_paths:
                        # FBodyInstance::SetCollisionEnabled invalidates the
                        # original named BlockAll profile for this exact edit.
                        # Dressing bins already used Custom; preserve them.
                        row["collision_profile"] = "Custom"
                elif row["path"] in hidden_floor_paths:
                    row.update(visible=False, hidden_in_game=True)
        for rows in expected_components.values():
            for row in rows:
                if row["path"] in retired_paths | hidden_floor_paths:
                    row.update(visible=False, hidden_in_game=True)
        actual_caves = actor_reader._snapshot_actors(u, caves, api)
        actual_components = inspector.persistent_components(u, caves)
        observed_new = set()
        allowed_new_paths = new_paths | ramp_paths
        for actor in actual_caves.values():
            observed_new.update(row["path"] for row in actor["primitives"] if row["path"] in allowed_new_paths)
            actor["primitives"] = [row for row in actor["primitives"] if row["path"] not in allowed_new_paths]
        for path in actual_components:
            actual_components[path] = [row for row in actual_components[path] if row["path"] not in allowed_new_paths]
        check("exactly_two_shell_and_two_ramp_visual_components_on_existing_Dressing_actors", observed_new == allowed_new_paths)
        check("four_retired_bins_collision_off_two_floor_bins_hidden_only_other_scene_fields_exact",
              normalize(actual_caves) == normalize(expected_caves))
        check("floor_physics_grassroof_timber_props_lights_other_components_exact", normalize(actual_components) == normalize(expected_components))
        # Preserve all complete exposure properties and native camera defaults,
        # beyond the small actor/component snapshot fields.
        visibility_receipt = json.loads((OLD / "author-live/visibility.json").read_text(encoding="utf-8-sig"))
        visibility_spec = json.loads((OLD / "visibility-spec.json").read_text(encoding="utf-8-sig"))
        report["exposure"] = predecessor.validate_local_visibility(u, actors, labels, visibility_receipt, visibility_spec, inspector)
        check("full_global_EV11_5_local_EV3_bounds_and_camera_defaults_preserved", True)
        material_failures = []
        for path, expected in baseline["materials.json"]["materials"].items():
            if inspector.material_state(u, u.load_asset(path), material_reader) != expected:
                material_failures.append(path)
        check("all47_preexisting_material_graphs_parameters_parentage_files_preserved",
              len(baseline["materials.json"]["materials"]) == 47 and not material_failures, material_failures)
        report["new_material_recipe"] = validate_organic_material_recipe(u, config["expected_material_path"],
            config["expected_floor_material_path"], baseline["materials.json"]["materials"], material_reader)
        check("new_rock_derivative_exact_original43_plus3_recipe_and_floor_material_preserved", True)
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
        old_observer = pinned_module("organic_height_contract", "Scripts/Editor/ObserveCaveTraversalCamera20261118.py", OBSERVER_SHA256)
        height_values, height_guard = old_observer.reviewed_height_contract(frozen["plan"])
        require(native.get("success") and native.get("read_only") and native.get("dirty_package_names_unchanged"),
                "Read-only terrain inspection failed")
        check("all179488_height_samples_bit_exact_and_original_layer_45collision45render_preserved",
              native.get("composite_heights_uint16") == height_values and len(height_values) == 179488
              and native.get("composite_blake3") == old_observer.COLLAR_NATIVE_BLAKE3
              and native.get("original_layer_blake3") == old_observer.ORIGINAL_LAYER
              and native.get("component_count") == native.get("collision_count") == 45
              and native.get("layer_count") == 2 and native.get("landscape_reports_up_to_date"))
        report["height_guard"] = height_guard
        components = {c.get_path_name(): c for a in caves for c in a.get_components_by_class(u.StaticMeshComponent)}
        for site in frozen["plan"]["sites"]:
            sid = site["site_id"]
            ramp_path = targets[sid]["actor_path"]+".Organic_"+sid+"_RampVisual"
            ramp = components[ramp_path]
            planned = [r for r in site["components"] if r["label"] == sid+"_Ramp"]
            require(len(planned) == 1, "One original ramp transform required")
            planned = planned[0]
            transform = ramp.get_world_transform()
            p, y, r = planned["rotation_pitch_yaw_roll_deg"]
            expected_q = u.Rotator(pitch=p, yaw=y, roll=r).quaternion()
            actual_q = [getattr(transform.rotation, a) for a in "xyzw"]
            wanted_q = [getattr(expected_q, a) for a in "xyzw"]
            require(ramp.get_editor_property("static_mesh").get_path_name() == "/Engine/BasicShapes/Cube.Cube"
                    and ramp.get_material(0).get_path_name() == config["expected_floor_material_path"]
                    and ramp.get_num_materials() == 1 and ramp.get_editor_property("visible")
                    and not ramp.get_editor_property("hidden_in_game") and not ramp.is_component_tick_enabled()
                    and not ramp.get_editor_property("can_ever_affect_navigation")
                    and ramp.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
                    and ramp.get_editor_property("mobility") == u.ComponentMobility.STATIC
                    and predecessor.near(predecessor.xyz(transform.translation), planned["location_cm"], .01)
                    and predecessor.near(predecessor.xyz(transform.scale3d), [n/100 for n in planned["dimensions_cm"]], 1e-5)
                    and (predecessor.near(actual_q, wanted_q, 1e-6) or predecessor.near(actual_q, [-q for q in wanted_q], 1e-6)),
                    "New render-only ramp differs from exact original collision ramp transform")
        check("two_ramp_visuals_reproduce_original_hidden_ramp_exactly_without_extra_collision", True)
        mesh_sources = {}
        report["source_topology"], report["imported_geometry"] = {}, {}
        for site in geometry["sites"]:
            sid = site["site_id"]
            require(sid in {"Q", "Z"} and sid not in mesh_sources, "Two unique Q/Z source sites required")
            source_file = (ROOT / site["indexed_mesh"]).resolve()
            require(source_file.is_relative_to(ROOT / "Art/World/CavesOrganic20261118")
                    and hashlib.sha256(source_file.read_bytes()).hexdigest() == site["indexed_mesh_sha256"],
                    "Exact reviewed source indexed mesh required")
            source = json.loads(source_file.read_text(encoding="utf-8-sig"))
            source["pivot_world_cm"] = source["world_pivot_cm"]
            mesh_sources[sid] = source
            topology = audit_indexed_shell(source["vertices_cm"], source["triangles"])
            report["source_topology"][sid] = topology
            check(sid+"_one_closed_oriented_manifold_source_shell", topology["passed"])
            require(len(source["triangle_material_ids"]) == len(source["triangles"])
                    and set(source["triangle_material_ids"]) == {0, 1}, "Exact source wall/floor triangle assignments required")
            floor_heights, floor_normals = [], []
            for tri, material_id in zip(source["triangles"], source["triangle_material_ids"]):
                if material_id != 1:
                    continue
                a, b, c = [source["vertices_cm"][i] for i in tri]
                floor_heights.extend(p[2]+source["world_pivot_cm"][2] for p in (a, b, c))
                normal = _cross(_sub(b, a), _sub(c, a))
                floor_normals.append(normal[2]/math.sqrt(_dot(normal, normal)))
            check(sid+"_visible_floor_section_upward_horizontal8609_one_cm_below_original_physics",
                  bool(floor_heights) and max(abs(z-8609.0) for z in floor_heights) <= .01
                  and min(floor_normals) >= .9999,
                  {"z_min_max_cm": [min(floor_heights), max(floor_heights)], "min_normal_z": min(floor_normals)})
            component = components[targets[sid]["component_path"]]
            mesh = component.get_editor_property("static_mesh")
            require(mesh is not None and mesh.get_path_name() == config["expected_mesh_paths"][sid], "Explicit expected imported mesh differs")
            require(not isinstance(component, u.InstancedStaticMeshComponent)
                    and component.get_editor_property("visible") and not component.get_editor_property("hidden_in_game")
                    and not component.is_component_tick_enabled() and component.get_editor_property("cast_shadow")
                    and component.get_editor_property("mobility") == u.ComponentMobility.STATIC
                    and component.get_material(0).get_path_name() == config["expected_material_path"]
                    and component.get_num_materials() == 2
                    and component.get_material(1).get_path_name() == config["expected_floor_material_path"]
                    and component.get_collision_enabled() == u.CollisionEnabled.QUERY_AND_PHYSICS
                    and all(component.get_collision_response_to_channel(ch) == u.CollisionResponseType.ECR_BLOCK for ch in
                            (u.CollisionChannel.ECC_PAWN, u.CollisionChannel.ECC_CAMERA, u.CollisionChannel.ECC_VISIBILITY))
                    and u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_collision_complexity(mesh)
                    == u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE, "Exact visible static query+physics complex-as-simple shell contract differs")
            transform = component.get_world_transform()
            require(predecessor.near(predecessor.xyz(transform.translation), source["world_pivot_cm"], .01)
                    and predecessor.near(predecessor.xyz(transform.scale3d), [1, 1, 1], 1e-6)
                    and predecessor.near([transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w], [0, 0, 0, 1], 1e-6),
                    "Imported shell world pivot/rotation/scale differs")
            vertices, triangles, material_ids = extract_static_mesh_geometry(u, mesh)
            require(set(source["triangle_material_ids"]) == {0, 1}, "Source wall/floor triangle assignments required")
            comparison = compare_oriented_triangle_geometry(source["vertices_cm"], native_ue_triangles(source["triangles"]), vertices, triangles,
                source["triangle_material_ids"], material_ids)
            comparison["native_winding_adapter"] = dict(NATIVE_WINDING_ADAPTER)
            report["imported_geometry"][sid] = comparison
            check(sid+"_actual_imported_oriented_triangles_match_source_no_axis_or_degenerate_loss", comparison["passed"])
        contact_check = make_source_contact_check(mesh_sources)
        results = []
        for case in frozen["cases"]:
            result = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.probe_pawn_capsule(
                u.Vector(*case["start"]), u.Vector(*case["end"]), case["radius"], 96.0))
            native_dirty = result.get("dirty_packages")
            require(isinstance(native_dirty, list) and len(native_dirty) == len(dirty_before)
                    and set(native_dirty) == set(dirty_before), "Native query dirty package identities changed or missing")
            result.pop("dirty_packages", None)
            results.append(result)
        report["capsule_results"] = results
        report["capsule_acceptance"] = evaluate_capsule_results(frozen, results, targets, contact_check)
        report["capsule_query_count"] = len(results)
        report["shell_targets"] = targets
        check("all2016_original_capsule_vectors_radii_and_expected_outcomes_pass",
              report["capsule_acceptance"]["passed"], report["capsule_acceptance"]["failed_cases"])
        after = api._dirty(u)
        report["dirty_packages_after"] = after
        check("dirty_package_identities_unchanged", set(after) == set(dirty_before) and len(after) == len(dirty_before))
        if require_saved_clean:
            check("saved_clean_after_independent_reload", not after)
        report["passed"] = all(report["checks"].values()) and not report["diagnostics"]
    except Exception:
        report["traceback"] = traceback.format_exc()
        report["diagnostics"].append({"check": "validation_exception", "detail": report["traceback"]})
        if "api" in locals() and "dirty_before" in locals():
            after = api._dirty(u)
            report["dirty_packages_after"] = after
            check("dirty_package_identities_unchanged", set(after) == set(dirty_before) and len(after) == len(dirty_before))
    report["limitations"] = ["Finite original Pawn sweeps and source/import triangle proof do not certify runtime camera; separate276-point normal-game camera walk and six reviewed captures are required.",
        "No physical keyboard, full combat, navigation, whole-project or Shipping acceptance."]
    return report
