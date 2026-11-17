"""Create two unsaved organic-rock derivatives after an explicit owner call.

Import is inert, including outside Unreal. The source47 materials, actors,
Landscape, camera, lights and exposure are read and compared, never edited.
No assignment, save, cleanup, rollback or automatic retry is performed.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import traceback


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
SPEC = BASE / "materialspec.json"
SPEC_SHA256 = "a2498735c25b9c878c2b2de522702e48119ebf66da7cad04e75e40f753804366"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OLD = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118"
OWN = OLD + "/Organic20261118"
SOURCE = OLD + "/M_CaveSurface"
FLOOR = OLD + "/MI_CaveStoneFloor"
MATERIAL = OWN + "/M_OrganicRock"
INSTANCE = OWN + "/MI_OrganicRock"
MATERIAL_BASELINE_SHA256 = "de2e85d4b23ae4ad014d88bcd0bf54d248722ee854e19f808744e5bf71a77afb"
SOURCE_GRAPH_SHA256 = "8ece65cd367aa9855e7d061cad2501924290afb2ab918115120f628f9c316669"
FROZEN_INPUTS = {
    "Feature/doc/evidence/cave-organic20261118/baseline-live/materials.json": MATERIAL_BASELINE_SHA256,
    "Feature/doc/evidence/cave-organic20261118/baseline-live/cave-actors.json": "fff169e2b9fc5ce1993cd0fa8372c038917fb87ee247179761d578a2a8f11ccb",
    "Feature/doc/evidence/cave-organic20261118/baseline-live/terrain.json": "a9a87d9ed7aa94c1366d0a9d802a262c598e319c56d188bae64a7c43dfe7be2f",
    "Feature/doc/evidence/cave-organic20261118/baseline-live/result.json": "e81bca59acc743213e58880b1a115c78e6a7338768a14238944c727ae3cfd28d",
    "Feature/doc/evidence/caves20261118/baseline-live/baseline-actors.json": "f5390322017e3a501811a105f72686c11e90a8c0ed662484aee43404569eacf5",
    "Feature/doc/evidence/caves20261118/triplanar-spec.json": "0d24c669a60f9c998401afcc7f7c75e74f6fe1f72c499496fda3139e5c47353f",
}
HELPERS = {
    "inspector": "Scripts/Editor/InspectCaveSites20261118.py",
    "reader": "Scripts/Editor/NaturalRoadMaterial20261117.py",
    "api": "Scripts/Editor/NaturalWorldDressing20261117.py",
    "actors": "Scripts/Editor/AuthorNaturalBossThreshold20261118.py",
}
STRUCT_ADDRESS = re.compile(r"(?<=\(0x)[0-9A-Fa-f]+(?=\) \{)")


class OrganicMaterialError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def path(value):
    return value.get_path_name() if value else None


def normalized_addresses(value):
    """Strip only ephemeral addresses in Struct repr, preserving every field."""
    if isinstance(value, dict):
        return {key: normalized_addresses(item) for key, item in value.items()}
    if isinstance(value, list):
        return [normalized_addresses(item) for item in value]
    if isinstance(value, str) and value.startswith("<Struct "):
        return STRUCT_ADDRESS.sub("ADDRESS", value)
    return value


def read_checked(relative, expected_sha256):
    raw = (ROOT / relative).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == expected_sha256, "Frozen input changed: " + relative)
    return json.loads(raw)


def module(name, relative):
    loader = importlib.util.spec_from_file_location(name, ROOT / relative)
    result = importlib.util.module_from_spec(loader)
    loader.loader.exec_module(result)
    return result


def build_material_spec():
    """Read-only pure-Python recipe; no Unreal import or filesystem mutation."""
    inputs = {k: read_checked(k, sha) for k, sha in FROZEN_INPUTS.items()}
    mats = inputs["Feature/doc/evidence/cave-organic20261118/baseline-live/materials.json"]["materials"]
    source = mats[SOURCE + ".M_CaveSurface"]
    tri = inputs["Feature/doc/evidence/caves20261118/triplanar-spec.json"]
    result = inputs["Feature/doc/evidence/cave-organic20261118/baseline-live/result.json"]
    require(len(mats) == 47 and len(source["graph"]["nodes"]) == 43
            and digest(source["graph"]) == SOURCE_GRAPH_SHA256, "Exact source47/43-node graph required")
    require(result["passed"] and result["pid"] == 49456 and result["actors"] == 1408
            and result["dirty_packages"] == [] and not result["mutations"], "Exact clean owner baseline required")
    sums = [name for name, node in source["graph"]["nodes"].items()
            if node["class"] == "MaterialExpressionAdd" and
            {row["node"] for row in node["inputs"]} == {"MaterialExpressionAdd_3", "MaterialExpressionMultiply_5"}]
    require(sums == ["MaterialExpressionAdd_4"] and tri["world_tile_cm"] == 240.0,
            "Exact current texture_sum_xyz triplanar node required")
    return {
        "schema": "Rogue10m.OrganicCaveMaterialSpec.20261118.v1", "map": TARGET,
        "baseline_checkpoint": "3a33ee21", "frozen_inputs": FROZEN_INPUTS,
        "helper_sha256": {name: hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
                          for name, relative in HELPERS.items()},
        "editor_pid": 49456, "expected_dirty_packages": [], "actor_count": 1408,
        "old_material_count": 47, "source": SOURCE, "source_instance": FLOOR,
        "source_graph_sha256": SOURCE_GRAPH_SHA256, "source_nodes": 43,
        "source_sum_node": sums[0], "source_roughness_node": "MaterialExpressionScalarParameter_2",
        "fixed_stone_texture": tri["fixed_stone_texture"], "world_tile_cm": 240.0,
        "source_side_tint_linear_rgba": [0.75, 0.72, 0.65, 1.0],
        "destination_material": MATERIAL, "destination_instance": INSTANCE,
        "new_assets": 2, "new_nodes": 3, "destination_nodes": 46,
        "new_expressions": [
            {"key": "tint", "class": "MaterialExpressionVectorParameter", "parameter_name": "OrganicTint",
             "default_linear_rgba": [0.30, 0.32, 0.28, 1.0]},
            {"key": "tinted_stone", "class": "MaterialExpressionMultiply", "const_a": 0.0, "const_b": 1.0},
            {"key": "neutral_normal", "class": "MaterialExpressionConstant3Vector", "constant_linear_rgba": [0.0, 0.0, 1.0, 1.0]},
        ],
        "links": [
            {"source": "source_sum_node", "output": "", "destination": "tinted_stone", "pin": "A"},
            {"source": "tint", "output": "RGB", "destination": "tinted_stone", "pin": "B"},
        ],
        "outputs_changed": {"MP_BASE_COLOR": "tinted_stone", "MP_NORMAL": "neutral_normal"},
        "instance_values": {"OrganicTint": [0.30, 0.32, 0.28, 1.0], "Roughness": 0.92},
        "preserve_all_source43_nodes_exactly": True, "preserve_other_material_outputs": True,
        "two_sided": False, "used_with_instanced_static_meshes": True,
        "old47_full_material_states_and_disk_files_preserved": True,
        "setter_success_verified_by_getter_not_boolean": True,
        "assets_saved": False, "actors_assigned": False, "lights_or_exposure_changed": False,
        "actual_runtime_render_acceptance_required": True,
        "api_source": "D:/Program Files/UE_5.8/Engine/Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp",
        "api_source_lines": {"create_material_expression": 634, "connect_material_property": 905,
                             "set_material_instance_parent": 1457, "scalar_setter": 1485, "vector_setter": 1573},
    }


def _unsaved_material_state(u, obj, reader, inspector):
    """Same graph/parameter reader as baseline, with explicit unsaved disk state."""
    lib = u.MaterialEditingLibrary
    base, parents, seen = obj, [], set()
    while isinstance(base, u.MaterialInstance):
        require(path(base) not in seen, "Material parent cycle")
        seen.add(path(base))
        base = base.get_editor_property("parent")
        require(base is not None, "Missing material parent")
        parents.append(path(base))
    require(isinstance(base, u.Material), "Concrete Material required")

    def disk(asset):
        package = path(asset).split(".")[0]
        if package not in (MATERIAL, INSTANCE):
            return inspector.disk(asset)
        file = ROOT / "Content" / (package[6:] + ".uasset")
        require(not file.exists(), "New asset unexpectedly saved: " + package)
        return {"package": package, "file": file.relative_to(ROOT).as_posix(), "unsaved": True, "exists": False}

    state = {"path": path(obj), "class": path(obj.get_class()), "disk": disk(obj),
             "parent_chain": parents, "base_material": path(base), "base_disk": disk(base),
             "used_with_instanced_static_meshes": bool(base.get_editor_property("used_with_instanced_static_meshes")),
             "blend_mode": str(base.get_editor_property("blend_mode")),
             "material_domain": str(base.get_editor_property("material_domain")),
             "two_sided": bool(base.get_editor_property("two_sided")),
             "scalar_parameter_names": list(map(str, lib.get_scalar_parameter_names(obj))),
             "vector_parameter_names": list(map(str, lib.get_vector_parameter_names(obj))),
             "texture_parameter_names": list(map(str, lib.get_texture_parameter_names(obj)))}
    if isinstance(obj, u.MaterialInstance):
        state["scalar_values"] = {name: float(lib.get_material_instance_scalar_parameter_value(obj, name))
                                  for name in state["scalar_parameter_names"]}
        state["vector_values"] = {name: [float(getattr(lib.get_material_instance_vector_parameter_value(obj, name), a))
                                         for a in "rgba"] for name in state["vector_parameter_names"]}
        state["texture_values"] = {name: path(lib.get_material_instance_texture_parameter_value(obj, name))
                                   for name in state["texture_parameter_names"]}
    state["graph"] = reader._fingerprint(u, lib, base)
    state["graph_sha256"] = digest(state["graph"])
    state["expression_classes"] = dict(Counter(row["class"] for row in state["graph"]["nodes"].values()))
    return state


def create_organic_cave_materials(*, allow_editor_mutation=False, expected_spec_sha256,
                                 expected_baseline_materials_sha256, expected_editor_pid,
                                 expected_dirty_packages):
    """Explicit idle-editor call. Returns JSON-compatible report; never saves."""
    report = {"schema": "Rogue10m.OrganicCaveMaterials.20261118.v1", "success": False,
              "started_utc": datetime.now(timezone.utc).isoformat(), "mutation_started": False,
              "assets_saved": False, "save_called": False, "created_assets": [],
              "created_actors": [], "created_components": [], "actors_assigned": [],
              "automatic_retry": False, "automatic_rollback": False, "partial_work_deleted": False}
    u = api = None
    try:
        require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
        require(expected_spec_sha256 == SPEC_SHA256 and SPEC_SHA256 != "PENDING_REVIEWED_SPEC_SHA256",
                "Reviewed exact material specification required")
        require(expected_baseline_materials_sha256 == MATERIAL_BASELINE_SHA256,
                "Caller must pin the exact source47 baseline file")
        require(expected_dirty_packages == [], "Material creation requires an explicitly clean snapshot")
        spec = read_checked(SPEC.relative_to(ROOT).as_posix(), SPEC_SHA256)
        require(spec == build_material_spec(), "Material recipe or helper source changed")
        import unreal as u
        api = module("organic_material_component_api", HELPERS["api"])
        inspector = module("organic_material_inspector", HELPERS["inspector"])
        reader = module("organic_material_graph_reader", HELPERS["reader"])
        actors_reader = module("organic_material_actor_reader", HELPERS["actors"])
        api._guard(u)
        require(os.getpid() == expected_editor_pid == spec["editor_pid"], "Exact owner Editor PID required")
        require(api._dirty(u) == [], "Editor must remain clean before material creation")
        inputs = {relative: read_checked(relative, sha) for relative, sha in spec["frozen_inputs"].items()}
        baseline = inputs["Feature/doc/evidence/cave-organic20261118/baseline-live/materials.json"]["materials"]
        terrain = inputs["Feature/doc/evidence/cave-organic20261118/baseline-live/terrain.json"]
        cave = inputs["Feature/doc/evidence/cave-organic20261118/baseline-live/cave-actors.json"]
        old = inputs["Feature/doc/evidence/caves20261118/baseline-live/baseline-actors.json"]
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        terrain_keys = ("component_count", "collision_count", "landscape_guid", "source_layer_guid", "layer_count",
                        "width", "height", "composite_blake3", "original_layer_blake3", "landscape_reports_up_to_date")
        require(native.get("success") and {k: native[k] for k in terrain_keys} == {k: terrain[k] for k in terrain_keys},
                "Exact settled terrain45/hash snapshot required")
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = list(subsystem.get_all_level_actors())
        require(len(actors) == 1408, "Exact baseline1408 actors required")
        before_actors = actors_reader._snapshot_actors(u, actors, api)
        before_components = inspector.persistent_components(u, actors)
        require(normalized_addresses({p: before_actors[p] for p in old["actors"]}) == normalized_addresses(old["actors"])
                and normalized_addresses({p: before_components[p] for p in old["persistent_components"]})
                == normalized_addresses(old["persistent_components"]),
                "Original1404 actor/component state changed")
        require(normalized_addresses({p: before_actors[p] for p in cave["actors"]}) == normalized_addresses(cave["actors"])
                and normalized_addresses({p: before_components[p] for p in cave["components"]})
                == normalized_addresses(cave["components"]),
                "Four cave actor/component states changed since owner baseline")
        before_camera = inspector.camera_defaults(u)
        before_materials = {p: inspector.material_state(u, u.load_asset(p), reader) for p in baseline}
        require(before_materials == baseline and len(before_materials) == 47, "Exact source47 material states required")
        source = u.load_asset(SOURCE)
        floor = u.load_asset(FLOOR)
        lib = u.MaterialEditingLibrary
        require(isinstance(source, u.Material) and isinstance(floor, u.MaterialInstanceConstant), "Exact source types required")
        source_graph = reader._fingerprint(u, lib, source)
        require(digest(source_graph) == SOURCE_GRAPH_SHA256 and len(source_graph["nodes"]) == 43,
                "Exact current43-node source graph required")
        source_nodes = {n.get_name(): n for n in lib.get_material_expressions(source)}
        side = source_nodes["MaterialExpressionConstant3Vector_0"].get_editor_property("constant")
        source_side = [float(getattr(side, a)) for a in "rgba"]
        require(all(abs(a-b) < 1e-6 for a, b in zip(source_side, spec["source_side_tint_linear_rgba"])),
                "Current side constant differs from source recipe")
        require(all(path(source_nodes[name].get_editor_property("texture")) == spec["fixed_stone_texture"]
                    for name in ("MaterialExpressionTextureSample_1", "MaterialExpressionTextureSample_2", "MaterialExpressionTextureSample_3")),
                "Existing triplanar color textures differ")
        require(abs(float(source_nodes["MaterialExpressionConstant_0"].get_editor_property("r")) - 240.) < 1e-6,
                "Existing triplanar world scale differs")
        for name in ("duplicate_asset", "does_asset_exist"):
            require(callable(getattr(u.EditorAssetLibrary, name, None)), "Unavailable asset API: " + name)
        for name in ("create_material_expression", "connect_material_expressions", "connect_material_property", "recompile_material",
                     "set_material_instance_parent", "set_material_instance_vector_parameter_value", "set_material_instance_scalar_parameter_value",
                     "get_material_instance_vector_parameter_value", "get_material_instance_scalar_parameter_value", "update_material_instance"):
            require(callable(getattr(lib, name, None)), "Unavailable material API: " + name)
        for destination in (MATERIAL, INSTANCE):
            file = ROOT / "Content" / (destination[6:] + ".uasset")
            require(not u.EditorAssetLibrary.does_asset_exist(destination) and not file.exists()
                    and u.find_object(None, destination + "." + destination.rsplit("/", 1)[1]) is None,
                    "Destination already exists; inspect any partial result instead of retrying: " + destination)
        require(api._dirty(u) == [], "Preflight unexpectedly dirtied a package")
        report.update(spec_sha256=SPEC_SHA256, baseline_materials_sha256=MATERIAL_BASELINE_SHA256,
                      source47_states_sha256=digest(before_materials), source_graph_sha256=SOURCE_GRAPH_SHA256,
                      source_side_tint_linear_rgba=source_side, dirty_before=[])
        with u.ScopedEditorTransaction("Organic cave continuous triplanar rock materials 20261118"):
            report["mutation_started"] = True
            material = u.EditorAssetLibrary.duplicate_asset(SOURCE, MATERIAL)
            require(isinstance(material, u.Material), "Organic parent duplication failed")
            report["created_assets"].append(MATERIAL)
            material.modify()
            require(reader._fingerprint(u, lib, material) == source_graph, "Duplicate source43 graph differs before edits")
            nodes = {n.get_name(): n for n in lib.get_material_expressions(material)}
            new = {}
            for i, description in enumerate(spec["new_expressions"]):
                node = lib.create_material_expression(material, getattr(u, description["class"]), 2200 + 230*i, 1800)
                require(node is not None, "Expression creation failed: " + description["key"])
                new[description["key"]] = node
                if description["key"] == "tint":
                    node.set_editor_property("parameter_name", description["parameter_name"])
                    rgba = description["default_linear_rgba"]
                    node.set_editor_property("default_value", u.LinearColor(r=rgba[0], g=rgba[1], b=rgba[2], a=rgba[3]))
                elif description["key"] == "neutral_normal":
                    node.set_editor_property("constant", u.LinearColor(r=0., g=0., b=1., a=1.))
                else:
                    node.set_editor_property("const_a", description["const_a"])
                    node.set_editor_property("const_b", description["const_b"])
            aliases = {**new, "source_sum_node": nodes[spec["source_sum_node"]]}
            for link in spec["links"]:
                reader._connect(lib, material, aliases[link["source"]], aliases[link["destination"]], link["pin"], link["output"])
            for prop_name, key in spec["outputs_changed"].items():
                prop = getattr(u.MaterialProperty, prop_name)
                require(lib.connect_material_property(new[key], "", prop), "Material output connection failed: " + prop_name)
                require(lib.get_material_property_input_node(material, prop) == new[key], "Material output readback failed: " + prop_name)
            lib.recompile_material(material)
            instance = u.EditorAssetLibrary.duplicate_asset(FLOOR, INSTANCE)
            require(isinstance(instance, u.MaterialInstanceConstant), "Organic instance duplication failed")
            report["created_assets"].append(INSTANCE)
            instance.modify()
            lib.set_material_instance_parent(instance, material)
            require(instance.get_editor_property("parent") == material, "Organic instance parent readback failed")
            tint = spec["instance_values"]["OrganicTint"]
            scalar_result = lib.set_material_instance_scalar_parameter_value(instance, "Roughness", spec["instance_values"]["Roughness"])
            vector_result = lib.set_material_instance_vector_parameter_value(instance, "OrganicTint",
                u.LinearColor(r=tint[0], g=tint[1], b=tint[2], a=tint[3]))
            lib.update_material_instance(instance)
            actual_tint = lib.get_material_instance_vector_parameter_value(instance, "OrganicTint")
            actual_roughness = float(lib.get_material_instance_scalar_parameter_value(instance, "Roughness"))
            require(all(abs(float(getattr(actual_tint, a))-v) < 1e-6 for a, v in zip("rgba", tint))
                    and abs(actual_roughness-.92) < 1e-6, "Organic instance exact getter verification failed")
            report["setter_receipts"] = {"scalar_return": scalar_result, "vector_return": vector_result,
                                         "getter_verified": True, "Roughness": actual_roughness,
                                         "OrganicTint": [float(getattr(actual_tint, a)) for a in "rgba"]}
        after_graph = reader._fingerprint(u, lib, material)
        require(len(after_graph["nodes"]) == 46 and all(after_graph["nodes"].get(k) == v for k, v in source_graph["nodes"].items()),
                "The original43 duplicate nodes must remain exact and only3 nodes may be added")
        changed_keys = {str(getattr(u.MaterialProperty, name).value) for name in spec["outputs_changed"]}
        require({k: v for k, v in after_graph["outputs"].items() if k not in changed_keys}
                == {k: v for k, v in source_graph["outputs"].items() if k not in changed_keys},
                "A material output besides BaseColor/Normal changed")
        neutral = new["neutral_normal"].get_editor_property("constant")
        require([float(getattr(neutral, a)) for a in "rgba"] == [0., 0., 1., 1.], "Exact neutral tangent normal required")
        require(not material.get_editor_property("two_sided")
                and material.get_editor_property("used_with_instanced_static_meshes") is True,
                "Source two-sided/ISM flags changed")
        after_materials = {p: inspector.material_state(u, u.load_asset(p), reader) for p in baseline}
        require(after_materials == before_materials == baseline, "One of the old47 full states or disk bytes changed")
        require([float(getattr(source_nodes["MaterialExpressionConstant3Vector_0"].get_editor_property("constant"), a))
                 for a in "rgba"] == source_side, "Original side constant changed")
        after_actors = list(subsystem.get_all_level_actors())
        require(normalized_addresses(actors_reader._snapshot_actors(u, after_actors, api)) == normalized_addresses(before_actors)
                and normalized_addresses(inspector.persistent_components(u, after_actors)) == normalized_addresses(before_components)
                and normalized_addresses(inspector.camera_defaults(u)) == normalized_addresses(before_camera),
                "Actor/component/camera/light/exposure state changed")
        final_native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(final_native.get("success") and {k: final_native[k] for k in terrain_keys} == {k: terrain[k] for k in terrain_keys},
                "Terrain changed while creating material derivatives")
        require(set(api._dirty(u)) == {MATERIAL, INSTANCE}, "Only the two new material packages may be dirty")
        states = {path(obj): _unsaved_material_state(u, obj, reader, inspector) for obj in (material, instance)}
        require(states[path(material)]["graph"] == states[path(instance)]["graph"] == after_graph,
                "Parent and instance graph readers differ")
        report.update(success=True, finished_utc=datetime.now(timezone.utc).isoformat(),
                      source47_states_after_sha256=digest(after_materials), old47_preserved=True,
                      source_surface_preserved=True, all1408_actors_preserved=True, terrain_preserved=True,
                      only_struct_repr_process_address_normalized=True,
                      lighting_exposure_camera_preserved=True, dirty_after=api._dirty(u),
                      created_material_states=states, edit_recipe=spec,
                      new_node_names={key: node.get_name() for key, node in new.items()},
                      explicit_node_readbacks={"neutral_normal_linear_rgba": [0., 0., 1., 1.], "world_tile_cm": 240.,
                                              "fixed_stone_texture": spec["fixed_stone_texture"]},
                      material_path=MATERIAL, instance_path=INSTANCE,
                      validation_pending=["actual curved mesh render", "actual camera routes", "bounded save", "independent cold"])
        return report
    except Exception:
        report.update(traceback=traceback.format_exc(), finished_utc=datetime.now(timezone.utc).isoformat())
        if u is not None and api is not None:
            try:
                report["dirty_after"] = api._dirty(u)
            except Exception:
                report["dirty_observation_failed"] = True
        raise OrganicMaterialError(report) from None


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Pure recipe validation; never invokes Unreal")
    parser.add_argument("--write-spec", action="store_true")
    args = parser.parse_args()
    value = build_material_spec()
    if args.write_spec:
        require(not SPEC.exists(), "Preserve existing reviewed material specification")
        with SPEC.open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")
        print(json.dumps({"spec": str(SPEC), "sha256": hashlib.sha256(SPEC.read_bytes()).hexdigest()}))
    else:
        print(json.dumps({"pure_recipe_passed": True, "source47": value["old_material_count"],
                          "source_nodes": value["source_nodes"], "destination_nodes": value["destination_nodes"],
                          "unreal_imported": "unreal" in __import__("sys").modules}))
