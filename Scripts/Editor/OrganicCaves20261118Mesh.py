"""Native GeometryScript creation of NEW organic-cave mesh assets only.

Import is inert and Unreal-free. No saves, actors, maps, existing asset writes,
or source geometry edits. The caller owns log boundaries and later persistence.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = (ROOT / "Art/World/CavesOrganic20261118/V2").resolve()
NEW_ASSET_ROOT = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118/Organic20261118/"
APPROVED_MESH_PATHS = {sid: NEW_ASSET_ROOT + f"SM_CaveOrganic_{sid}.SM_CaveOrganic_{sid}" for sid in ("Q", "Z")}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _qa():
    path = ROOT / "Scripts/Editor/OrganicCaves20261118QA.py"
    spec = importlib.util.spec_from_file_location("organic_mesh_creation_independent_qa", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module, _sha(path)


def _checked_source(relative, expected_hash):
    path = (ROOT / relative).resolve()
    require(path.is_relative_to(SOURCE_ROOT), "Only V2 organic source geometry may be read")
    require(_sha(path) == expected_hash, "Source file hash differs: " + relative)
    return path


def read_source(source_site_spec):
    """Load hash-pinned indexed geometry and matching authored OBJ normals."""
    site = source_site_spec
    require(site.get("site_id") in ("Q", "Z"), "Only the retained Q/Z source sites are allowed")
    indexed = _checked_source(site["indexed_mesh"], site["indexed_mesh_sha256"])
    obj = _checked_source(site["source_obj"], site["source_obj_sha256"])
    source = json.loads(indexed.read_text(encoding="utf-8"))
    require(source["site_id"] == site["site_id"], "Indexed site differs")
    require(source["world_pivot_cm"] == site["world_pivot_cm"], "Indexed actor pivot differs")
    normals = []
    with obj.open("r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("vn "):
                normals.append(list(map(float, line.split()[1:4])))
    vertices, triangles, materials = source["vertices_cm"], source["triangles"], source["triangle_material_ids"]
    require(len(normals) == len(vertices), "OBJ normal count must match indexed vertices")
    require(len(materials) == len(triangles) and set(materials) == {0, 1}, "Exactly rock/floor triangle materials required")
    require(all(len(v) == 3 and all(math.isfinite(float(x)) for x in v) for v in vertices + normals), "Invalid source vectors")
    require(all(abs(math.sqrt(sum(x*x for x in n)) - 1) < 1e-4 for n in normals), "Authored source normals must be normalized")
    source["normals"] = normals
    return source


def _extract_dynamic(u, dynamic):
    vertex_result = u.GeometryScript_MeshQueries.get_all_vertex_positions(dynamic, False)
    triangle_result = u.GeometryScript_MeshQueries.get_all_triangle_indices(dynamic, False)
    require(len(vertex_result) == len(triangle_result) == 3 and vertex_result[0] == triangle_result[0] == dynamic,
            "Dynamic mesh public query return shape differs")
    require(vertex_result[2] is False and triangle_result[2] is False, "Dynamic mesh contains vertex/triangle ID gaps")
    vertices = [[float(v.x), float(v.y), float(v.z)] for v in u.GeometryScript_List.convert_vector_list_to_array(vertex_result[1])]
    triangles = [[int(v.x), int(v.y), int(v.z)] for v in u.GeometryScript_List.convert_triangle_list_to_array(triangle_result[1])]
    getters = [name for name in dir(u.GeometryScript_Materials) if name.replace("_", "").lower() == "getalltrianglematerialids"]
    require(len(getters) == 1, "One public triangle-material query binding required")
    result = getattr(u.GeometryScript_Materials, getters[0])(dynamic)
    require(len(result) == 3 and result[0] == dynamic and result[2] is True, "Dynamic mesh material IDs unavailable")
    ids = list(map(int, u.GeometryScript_List.convert_index_list_to_array(result[1])))
    require(len(triangles) == len(ids) == dynamic.get_triangle_count(), "Dynamic mesh triangle/material count differs")
    return vertices, triangles, ids


def native_triangles(source):
    """Adapt immutable artist CCW triangles to Unreal's left-hand front faces.

    Keep vertex positions, source outward shading normals and material IDs.
    Installed GeometryCore/VectorUtil.h NormalDirection uses (V2-V0)x(V1-V0),
    and the actual EngineCube has negative conventional signed volume.
    """
    return [[a, c, b] for a, b, c in source["triangles"]]


def _compare(qa, source, actual, *, native=True):
    triangles = native_triangles(source) if native else source["triangles"]
    result = qa.compare_oriented_triangle_geometry(source["vertices_cm"], triangles, actual[0], actual[1],
                expected_material_ids=source["triangle_material_ids"], material_ids=actual[2])
    require(result["passed"], "Oriented triangle positions/material IDs differ: " + json.dumps(result))
    result["expected_orientation"] = "Unreal native [a,c,b] adapter" if native else "Known wrong native artist [a,b,c] before repair"
    return result


def prepare_organic_dynamic_mesh(u, source_site_spec):
    """Build and verify a transient mesh. Never create or save an asset."""
    source = read_source(source_site_spec)
    qa, qa_hash = _qa()
    buffers = u.GeometryScriptSimpleMeshBuffers()
    buffers.vertices = [u.Vector(*v) for v in source["vertices_cm"]]
    buffers.normals = [u.Vector(*v) for v in source["normals"]]
    buffers.triangles = [u.IntVector(*v) for v in native_triangles(source)]
    buffers.tri_group_i_ds = list(map(int, source["triangle_material_ids"]))
    # Supply one valid UV channel even if the optional box projection binding
    # is absent. Current materials use world projection, not source UVs.
    buffers.uv0 = [u.Vector2D(v[0] / 240, v[1] / 240) for v in source["vertices_cm"]]
    dynamic = u.DynamicMesh()
    appended = u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers, 0)
    require(isinstance(appended, tuple) and appended[0] == dynamic, "AppendBuffers return differs")
    require(dynamic.get_triangle_count() == len(source["triangles"]), "AppendBuffers omitted source triangles")
    material_result = u.GeometryScript_Materials.set_polygroup_material_id(dynamic, u.GeometryScriptGroupLayer(), 1, 1)
    require(isinstance(material_result, tuple) and material_result == (dynamic, True), "Floor polygroup was not assigned material1")
    before_uv = _compare(qa, source, _extract_dynamic(u, dynamic))
    uv_applied = False
    uv_library = getattr(u, "GeometryScript_UVs", None)
    projection = getattr(uv_library, "set_mesh_u_vs_from_box_projection", None) if uv_library else None
    if callable(projection):
        box = u.Transform(location=u.Vector(0, 0, 0), rotation=u.Rotator(0, 0, 0), scale=u.Vector(240, 240, 240))
        projected = projection(dynamic, 0, box, u.GeometryScriptMeshSelection())
        require(projected == dynamic, "Box UV projection returned a different mesh")
        uv_applied = True
    after_uv = _compare(qa, source, _extract_dynamic(u, dynamic))
    return dynamic, source, {"qa_script_sha256": qa_hash, "before_uv_comparison": before_uv,
                             "box_uv_projection_applied": uv_applied, "after_uv_comparison": after_uv,
                             "native_winding_adapter": "[a,b,c] -> [a,c,b]; authored outward normals unchanged"}


def create_organic_cave_mesh(u, source_site_spec, mesh_path, rock_mi, floor_mi, *, allow_editor_mutation=False):
    """Create one NEW unsaved asset, verify it, return ``(mesh, report)``.

    Caller supplies resolved material objects and an exact new package path.
    On an exception after creation, retain the unsaved partial asset for root
    diagnosis; never delete, overwrite, save, or retry an ambiguous mutation.
    """
    require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
    require(isinstance(mesh_path, str) and mesh_path.startswith(NEW_ASSET_ROOT) and "." not in mesh_path,
            "Only an explicit new organic package path is allowed")
    require(mesh_path == APPROVED_MESH_PATHS.get(source_site_spec.get("site_id"), "").split(".")[0],
            "Creation path must match the exact approved new source-site mesh")
    require(rock_mi is not None and floor_mi is not None, "Resolved rock/floor materials required")
    require(not u.EditorAssetLibrary.does_asset_exist(mesh_path), "Refusing to edit existing mesh asset: " + mesh_path)
    object_path = mesh_path + "." + mesh_path.rsplit("/", 1)[1]
    require(u.find_object(None, object_path) is None, "Refusing to reuse a loaded/unsaved asset: " + object_path)
    dynamic, source, transient_report = prepare_organic_dynamic_mesh(u, source_site_spec)
    qa, qa_hash = _qa()
    require(qa_hash == transient_report["qa_script_sha256"], "Independent QA script changed during transient preparation")
    options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.enable_collision = True
    options.collision_mode = u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    options.enable_nanite = False
    options.enable_recompute_normals = False
    options.enable_recompute_tangents = False
    options.use_original_vertex_order = False
    created = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, mesh_path, options)
    require(isinstance(created, tuple) and len(created) == 2 and created[0] is not None
            and created[1] == u.GeometryScriptOutcomePins.SUCCESS, "Native mesh creation failed; do not retry blindly")
    mesh = created[0]
    require(mesh.get_path_name() == object_path, "Native creator returned an unexpected asset path")
    report = finish_organic_cave_mesh(u, mesh, source_site_spec, rock_mi, floor_mi,
                                     allow_editor_mutation=True)
    report["transient"] = transient_report
    report["resumed_new_unsaved_asset"] = False
    return mesh, report


def _require_new_unsaved_mesh(u, mesh, source):
    sid = source["site_id"]
    object_path = APPROVED_MESH_PATHS[sid]
    require(mesh is not None and isinstance(mesh, u.StaticMesh) and mesh.get_path_name() == object_path,
            "Only the exact approved new organic Q/Z mesh may be finished")
    require(u.find_object(None, object_path) == mesh, "Finish target must be the caller's existing loaded new mesh")
    package_path = object_path.split(".")[0]
    disk_path = ROOT / "Content" / (package_path.removeprefix("/Game/") + ".uasset")
    require(not disk_path.exists(), "Refusing to finish an asset already saved on disk")
    require(package_path in {p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()},
            "Finish target must remain the explicitly unsaved dirty new package")
    return object_path


def finish_organic_cave_mesh(u, mesh, source_site_spec, rock_mi, floor_mi, *, allow_editor_mutation=False):
    """Explicitly finish one exact approved NEW unsaved native-adapted asset.

    This does not create, delete, overwrite geometry, load another mesh, or
    save. The create entrypoint uses this implementation after creation. The
    exact native-adapted source comparison passes before any finish mutation.
    """
    require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required for finish")
    source = read_source(source_site_spec)
    object_path = _require_new_unsaved_mesh(u, mesh, source)
    require(rock_mi is not None and floor_mi is not None, "Resolved rock/floor materials required")
    qa, qa_hash = _qa()
    # Preserve the exact oriented float32-key/material multiset comparator from
    # independent QA. Never retry a failed asset creation to recover this step.
    initial_comparison = _compare(qa, source, qa.extract_static_mesh_geometry(u, mesh))
    # Installed CreateStaticMeshUtil.cpp guarantees an extra material slot for
    # each MeshDescription PolygonGroup, even with default NumMaterialSlots1.
    require(len(mesh.get_editor_property("static_materials")) >= 2, "Creator did not provide both polygon-group slots")
    mesh.set_material(0, rock_mi)
    mesh.set_material(1, floor_mi)
    subsystem = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
    settings = subsystem.get_lod_build_settings(mesh, 0)
    settings.set_editor_property("remove_degenerates", False)
    settings.set_editor_property("recompute_normals", False)
    settings.set_editor_property("recompute_tangents", False)
    settings.set_editor_property("generate_lightmap_u_vs", False)
    subsystem.set_lod_build_settings(mesh, 0, settings)
    actual_settings = subsystem.get_lod_build_settings(mesh, 0)
    for name in ("remove_degenerates", "recompute_normals", "recompute_tangents", "generate_lightmap_u_vs"):
        require(actual_settings.get_editor_property(name) is False, "New mesh build setting differs: " + name)
    after_build_comparison = _compare(qa, source, qa.extract_static_mesh_geometry(u, mesh))
    require(mesh.get_material(0) == rock_mi and mesh.get_material(1) == floor_mi, "New mesh material slot readback differs")
    require(subsystem.get_collision_complexity(mesh) == u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
            "Concave cave collision must remain UseComplexAsSimple")
    return {"schema": "Rogue10m.OrganicCaveNativeMeshCreation.20261118.v2", "site_id": source["site_id"],
                  "mesh_path": object_path, "source_obj_sha256": source_site_spec["source_obj_sha256"],
                  "source_indexed_sha256": source_site_spec["indexed_mesh_sha256"], "qa_script_sha256": qa_hash,
                  "initial_asset_comparison": initial_comparison, "after_build_settings_comparison": after_build_comparison,
                  "material_paths": [rock_mi.get_path_name(), floor_mi.get_path_name()], "material_slot_count": len(mesh.get_editor_property("static_materials")),
                  "remove_degenerates": False, "recompute_normals": False, "recompute_tangents": False,
                  "generate_lightmap_uvs": False, "nanite_requested": False, "collision": "UseComplexAsSimple",
                  "saved": False, "actors_changed": False, "existing_assets_changed": False,
                  "resumed_new_unsaved_asset": True,
                  "native_winding_adapter": "[a,b,c] -> [a,c,b]; authored outward normals unchanged"}


def repair_organic_cave_mesh(u, mesh, source_site_spec, rock_mi, floor_mi, *, allow_editor_mutation=False):
    """Repair the two known unsaved wrong-winding assets, explicitly once.

    Requires exact old artist-order geometry/materials before mutation. Copies
    adapted native triangles into that same new object through the public API,
    rebuilds and independently verifies. Never creates/deletes assets or edits
    an asset already saved on disk; never edits actors or artist source bytes.
    """
    require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required for winding repair")
    source = read_source(source_site_spec)
    _require_new_unsaved_mesh(u, mesh, source)
    qa, qa_hash = _qa()
    before = _compare(qa, source, qa.extract_static_mesh_geometry(u, mesh), native=False)
    dynamic, prepared_source, transient_report = prepare_organic_dynamic_mesh(u, source_site_spec)
    require(qa_hash == transient_report["qa_script_sha256"], "Independent QA changed during repair preparation")
    require(prepared_source["vertices_cm"] == source["vertices_cm"], "Source changed during repair preparation")
    subsystem = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
    existing_settings = subsystem.get_lod_build_settings(mesh, 0)
    require(existing_settings.get_editor_property("generate_lightmap_u_vs") is False,
            "Repair requires the reviewed existing no-lightmap-UV build setting")
    options = u.GeometryScriptCopyMeshToAssetOptions()
    options.enable_recompute_normals = False
    options.enable_recompute_tangents = False
    options.enable_remove_degenerates = False
    options.use_original_vertex_order = False
    options.use_build_scale = False
    options.replace_materials = False
    options.clean_assigned_materials = False
    options.apply_nanite_settings = False
    options.emit_transaction = True
    options.defer_mesh_post_edit_change = False
    lod = u.GeometryScriptMeshWriteLOD()
    lod.lod_index = 0
    lod.write_hi_res_source = False
    copied = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dynamic, mesh, options, lod, True)
    require(isinstance(copied, tuple) and len(copied) == 2 and copied[0] == dynamic
            and copied[1] == u.GeometryScriptOutcomePins.SUCCESS, "Native winding repair failed; do not retry blindly")
    report = finish_organic_cave_mesh(u, mesh, source_site_spec, rock_mi, floor_mi, allow_editor_mutation=True)
    report["schema"] = "Rogue10m.OrganicCaveNativeWindingRepair.20261118.v1"
    report["wrong_winding_before_repair"] = before
    report["repair_transient"] = transient_report
    report["geometry_replaced_on_same_unsaved_new_object"] = True
    return report
