"""Inert independent render-LOD triangle/section proof for organic cave meshes.

The main QA also checks source MeshDescription geometry. This complementary
read-only test extracts actual RenderData LOD0 and compares its full oriented
float32 triangle/material multiset; counts alone do not establish equality.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"


def validate_render_geometry(config, *, expected_qa_sha256):
    """Return JSON only. Creates transient DynamicMeshes; never saves assets."""
    report = {"schema": "Rogue10m.OrganicCaveRenderLODValidation.20261118.v1", "passed": False,
              "read_only": True, "assets_saved": False, "sites": {}, "config": config}
    try:
        qa_file = ROOT / "Scripts/Editor/OrganicCaves20261118QA.py"
        if hashlib.sha256(qa_file.read_bytes()).hexdigest() != expected_qa_sha256:
            raise RuntimeError("Caller-pinned independent geometry comparator changed")
        spec = importlib.util.spec_from_file_location("organic_render_geometry_comparator", qa_file)
        qa = importlib.util.module_from_spec(spec)
        old = sys.dont_write_bytecode
        sys.dont_write_bytecode = True
        try:
            spec.loader.exec_module(qa)
        finally:
            sys.dont_write_bytecode = old
        require = qa.require
        import unreal as u
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        require(not editor.get_game_world() and editor.get_editor_world().get_path_name().split(".")[0] == TARGET,
                "Exact idle candidate editor required")
        dirty = lambda: sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
        before = dirty()
        report["dirty_before"] = before
        geometry_file = BASE / "geometryspec-v2.json"
        require(hashlib.sha256(geometry_file.read_bytes()).hexdigest() == config["geometry_spec_sha256"], "Reviewed V2 geometry changed")
        geometry = json.loads(geometry_file.read_text(encoding="utf-8-sig"))
        for site in geometry["sites"]:
            sid = site["site_id"]
            mesh = u.find_object(None, config["expected_mesh_paths"][sid])
            require(isinstance(mesh, u.StaticMesh), "Expected mesh must already exist and be loaded")
            source_file = ROOT / site["indexed_mesh"]
            require(hashlib.sha256(source_file.read_bytes()).hexdigest() == site["indexed_mesh_sha256"], "Reviewed indexed mesh changed")
            source = json.loads(source_file.read_text(encoding="utf-8-sig"))
            requested = u.GeometryScriptMeshReadLOD()
            requested.set_editor_property("lod_type", u.GeometryScriptLODType.RENDER_DATA)
            requested.set_editor_property("lod_index", 0)
            availability_getters = [name for name in dir(u.GeometryScript_AssetUtils)
                if name.replace("_", "").lower() == "checkstaticmeshhasavailablelod"]
            require(len(availability_getters) == 1, "One public RenderData availability getter required")
            availability = getattr(u.GeometryScript_AssetUtils, availability_getters[0])(mesh, requested)
            require(availability == u.GeometryScriptSearchOutcomePins.FOUND, "Actual RenderData LOD0 unavailable")
            dynamic = u.DynamicMesh()
            copied = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dynamic,
                u.GeometryScriptCopyMeshFromAssetOptions(), requested)
            require(len(copied) == 2 and copied[0] == dynamic and copied[1] == u.GeometryScriptOutcomePins.SUCCESS,
                    "Public RenderData LOD0 extraction failed")
            vertex_result = u.GeometryScript_MeshQueries.get_all_vertex_positions(dynamic, False)
            triangle_result = u.GeometryScript_MeshQueries.get_all_triangle_indices(dynamic, False)
            require(len(vertex_result) == len(triangle_result) == 3 and vertex_result[0] == triangle_result[0] == dynamic
                    and vertex_result[2] is False and triangle_result[2] is False, "Contiguous actual RenderData lists required")
            vertices = [[float(v.x), float(v.y), float(v.z)] for v in u.GeometryScript_List.convert_vector_list_to_array(vertex_result[1])]
            triangles = [[int(v.x), int(v.y), int(v.z)] for v in u.GeometryScript_List.convert_triangle_list_to_array(triangle_result[1])]
            getters = [name for name in dir(u.GeometryScript_Materials) if name.replace("_", "").lower() == "getalltrianglematerialids"]
            require(len(getters) == 1, "One public render triangle material-ID getter required")
            result = getattr(u.GeometryScript_Materials, getters[0])(dynamic)
            require(len(result) == 3 and result[0] == dynamic and result[2] is True, "Actual render section IDs unavailable")
            ids = list(map(int, u.GeometryScript_List.convert_index_list_to_array(result[1])))
            comparison = qa.compare_oriented_triangle_geometry(source["vertices_cm"], qa.native_ue_triangles(source["triangles"]), vertices, triangles,
                source["triangle_material_ids"], ids)
            comparison["native_winding_adapter"] = dict(qa.NATIVE_WINDING_ADAPTER)
            render_count = int(mesh.get_num_triangles(0))
            settings = u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_lod_build_settings(mesh, 0)
            flags = {name: settings.get_editor_property(name) for name in
                     ("remove_degenerates", "recompute_normals", "recompute_tangents", "generate_lightmap_u_vs")}
            report["sites"][sid] = {"passed": comparison["passed"] and render_count == len(triangles) == len(source["triangles"])
                and all(value is False for value in flags.values()), "mesh": mesh.get_path_name(),
                "requested_LOD_type": "RenderData", "requested_LOD_index": 0,
                "availability_public_enum": str(availability),
                "native_render_triangle_count": render_count, "extracted_triangles": len(triangles),
                "source_triangles": len(source["triangles"]), "build_flags": flags, "comparison": comparison}
        after = dirty()
        report["dirty_after"] = after
        report["passed"] = set(report["sites"]) == {"Q", "Z"} and all(row["passed"] for row in report["sites"].values()) and before == after
    except Exception:
        report["traceback"] = traceback.format_exc()
        if "dirty" in locals():
            report["dirty_after"] = dirty()
    return report
