"""Own the five ISM shader variants without saving or changing source assets.

Import is inert. create_derivatives() is for fresh dressing before any ISM
assignment. repair_material_bindings() is pinned to the diagnosed unsaved
22-component candidate: duplicate five materials, retarget just those ISMs,
then reload only the five source packages from their unchanged disk files.
Neither function saves packages, changes geometry, or invokes arbitrary code.
Partial failures are preserved in MaterialDerivativeError.report; never retry.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117/author-live"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
SOURCE_ROOT = "/Game/Rogue10m/World/StartVillage/Materials/"
SUFFIXES = ("DarkTimber", "WarmStone", "Banner_Teal", "Banner_Ochre", "Banner_Blue")
MAPPING = {SOURCE_ROOT + "M_StartVillage_" + suffix: OWN + "/M_Natural_" + suffix for suffix in SUFFIXES}
TAG = "Rogue10m.NaturalWorldSetDressing20261117"
SCOPE = "Rogue10m.NaturalBoundary20261117"
SCHEMA = "Rogue10m.NaturalDressingMaterialDerivatives20261117.v1"


class MaterialDerivativeError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def _require(value, message):
    if not value:
        raise RuntimeError(message)


def _path(value):
    return value.get_path_name() if value else None


def _package(value):
    return _path(value).split(".")[0] if value else None


def _file(package):
    _require(package in MAPPING, "Only the exact five source files may be read")
    return ROOT / "Content" / (package[6:] + ".uasset")


def _sha(package):
    return hashlib.sha256(_file(package).read_bytes()).hexdigest()


def _dirty(u):
    return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                   + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def _owned(package):
    return (package == TARGET or package.startswith(OWN + "/")
            or package.startswith("/Game/__ExternalActors__/" + TARGET[6:] + "/")
            or package.startswith("/Game/__ExternalObjects__/" + TARGET[6:] + "/"))


def _guard(u):
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    _require(Path(u.Paths.project_dir()).resolve() == ROOT.resolve()
             and str(u.SystemLibrary.get_engine_version()).startswith("5.8.")
             and world and _package(world) == TARGET
             and not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor(),
             "Exact UE5.8 idle candidate editor required; no world is switched")
    return world


def _reader():
    spec = importlib.util.spec_from_file_location("natural_material_graph_reader",
        ROOT / "Scripts/Editor/NaturalRoadMaterial20261117.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _fingerprint(u, reader, material):
    """Use the same public graph traversal for source, clone and disk reload."""
    lib = u.MaterialEditingLibrary
    value = reader._fingerprint(u, lib, material)
    known = {"MaterialExpressionScalarParameter", "MaterialExpressionVectorParameter",
             "MaterialExpressionTextureSample", "MaterialExpressionComponentMask",
             "MaterialExpressionMultiply", "MaterialExpressionSubtract", "MaterialExpressionAdd",
             "MaterialExpressionWorldPosition", "MaterialExpressionTextureCoordinate",
             "MaterialExpressionNoise", "MaterialExpressionSmoothStep", "MaterialExpressionAbs",
             "MaterialExpressionOneMinus", "MaterialExpressionConstant", "MaterialExpressionConstant2Vector",
             "MaterialExpressionConstant3Vector", "MaterialExpressionConstant4Vector"}
    nodes = {n.get_name(): n for n in lib.get_material_expressions(material)}
    for name, row in value["nodes"].items():
        kind = row["class"]
        _require(kind in known, "Unreviewed source expression class before duplication: " + kind)
        node = nodes[name]
        if kind == "MaterialExpressionConstant":
            row["properties"] = {"r": float(node.get_editor_property("r"))}
        elif kind == "MaterialExpressionConstant2Vector":
            row["properties"] = {key: float(node.get_editor_property(key)) for key in ("r", "g")}
        elif kind in ("MaterialExpressionConstant3Vector", "MaterialExpressionConstant4Vector"):
            color = node.get_editor_property("constant")
            row["properties"] = {"constant": [float(getattr(color, key)) for key in "rgba"]}
    value["appearance"] = {key: str(material.get_editor_property(key))
        for key in ("shading_model", "two_sided", "use_material_attributes", "opacity_mask_clip_value")}
    return value


def inspect_api_bindings():
    """Read-only binding/property audit callable before any repair mutation."""
    import unreal as u
    _guard(u)
    before = _dirty(u)
    specs = ((u.EditorAssetLibrary, ("duplicate_asset", "does_asset_exist")),
             (u.EditorLoadingAndSavingUtils, ("reload_packages", "get_dirty_map_packages", "get_dirty_content_packages")),
             (u.MaterialEditingLibrary, ("get_material_expressions", "get_material_expression_input_names",
                 "get_inputs_for_material_expression", "get_input_node_output_name_for_material_expression",
                 "get_material_property_input_node", "get_material_property_input_node_output_name", "recompile_material")),
             (u.InstancedStaticMeshComponent, ("set_material", "get_material", "get_num_materials", "get_instance_count")))
    report = {"read_only": True, "methods": {}, "source_properties": []}
    for cls, names in specs:
        found = {name: callable(getattr(cls, name, None)) for name in names}
        report["methods"][cls.__name__] = found
        _require(all(found.values()), "Required public Python material/reload binding unavailable: " + cls.__name__)
    _require(hasattr(u.ReloadPackagesInteractionMode, "ASSUME_POSITIVE"), "Exact noninteractive reload enum unavailable")
    for package in MAPPING:
        material = u.load_asset(package)
        _require(isinstance(material, u.Material), "Source is not an exact Material: " + package)
        report["source_properties"].append({"source_package": package,
            "used_with_instanced_static_meshes": bool(material.get_editor_property("used_with_instanced_static_meshes"))})
    _require(before == _dirty(u), "Read-only binding audit changed dirty packages")
    report["dirty_set_unchanged"] = True
    return report


def _start(allow_editor_mutation, operation):
    _require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
    import unreal as u
    world = _guard(u)
    report = {"schema": SCHEMA, "success": False, "operation": operation, "map": TARGET,
        "started_utc": datetime.now(timezone.utc).isoformat(), "assets_saved": False,
        "source_assets_saved": False, "geometry_mutated": False, "materials": [], "assignments": [],
        "reload": None, "dirty_before": _dirty(u), "packages": []}
    report["api_binding_preflight"] = inspect_api_bindings()
    return u, world, report


def _clone_five(u, report, *, allow_diagnosed_dirty):
    reader = _reader()
    dirty = set(_dirty(u))
    foreign = {name for name in dirty if not _owned(name)}
    _require(foreign == (set(MAPPING) if allow_diagnosed_dirty else set()),
             "Unexpected foreign dirty packages before derivative creation")
    originals = {}
    # Validate all destinations, source graphs and file hashes before any duplicate.
    for source, destination in MAPPING.items():
        _require(not u.EditorAssetLibrary.does_asset_exist(destination)
                 and not (ROOT / "Content" / (destination[6:] + ".uasset")).exists(),
                 "Derivative already exists; no overwrite or retry: " + destination)
        material = u.load_asset(source)
        _require(isinstance(material, u.Material) and _package(material) == source, "Source identity differs")
        row = {"source_package": source, "source_object": _path(material),
               "destination_package": destination, "destination_object": None,
               "source_file": _file(source).relative_to(ROOT).as_posix(),
               "source_sha256_before": _sha(source),
               "source_used_with_instanced_static_meshes_before":
                   bool(material.get_editor_property("used_with_instanced_static_meshes")),
               "source_graph_before": _fingerprint(u, reader, material)}
        if allow_diagnosed_dirty:
            _require(row["source_used_with_instanced_static_meshes_before"], "Diagnosed source ISM usage flag is absent")
        report["materials"].append(row)
        originals[source] = material
    owned = {}
    for row in report["materials"]:
        source, destination = row["source_package"], row["destination_package"]
        clone = u.EditorAssetLibrary.duplicate_asset(source, destination)
        _require(isinstance(clone, u.Material) and _package(clone) == destination, "Unexpected material duplicate")
        row["destination_object"] = _path(clone)
        report["packages"].append(destination)
        _require(_fingerprint(u, reader, clone) == row["source_graph_before"], "Clone changed the source graph")
        clone.set_editor_property("used_with_instanced_static_meshes", True)
        _require(clone.get_editor_property("used_with_instanced_static_meshes") is True, "Clone ISM usage flag did not persist")
        errors = list(map(str, u.MaterialEditingLibrary.recompile_material(clone)))
        row["clone_compile_errors"] = errors
        _require(not errors, "Owned derivative shader compilation failed")
        row["clone_graph_after"] = _fingerprint(u, reader, clone)
        _require(row["clone_graph_after"] == row["source_graph_before"], "Usage-only derivative changed graph/appearance")
        _require(_sha(source) == row["source_sha256_before"], "Source disk changed during duplicate/compile")
        owned[source] = clone
    return originals, owned, reader


def create_derivatives(*, allow_editor_mutation=False):
    """For future fresh authoring: return (source-package -> owned Material, report)."""
    report = {"schema": SCHEMA, "operation": "create_before_ism_assignment", "success": False}
    try:
        u, _, report = _start(allow_editor_mutation, "create_before_ism_assignment")
        _, owned, reader = _clone_five(u, report, allow_diagnosed_dirty=False)
        for row in report["materials"]:
            source = row["source_package"]
            original = u.load_asset(source)
            row["source_sha256_after"] = _sha(source)
            row["source_graph_after_reload"] = _fingerprint(u, reader, original)
            row["source_used_with_instanced_static_meshes_after_reload"] = bool(original.get_editor_property("used_with_instanced_static_meshes"))
            row["source_dirty_after_reload"] = source in _dirty(u)
            _require(row["source_sha256_after"] == row["source_sha256_before"]
                     and row["source_graph_after_reload"] == row["source_graph_before"]
                     and not row["source_dirty_after_reload"], "Source changed during fresh derivative preparation")
        report.update(success=True, dirty_after=_dirty(u), finished_utc=datetime.now(timezone.utc).isoformat())
        return owned, report
    except Exception:
        report["traceback"] = traceback.format_exc()
        raise MaterialDerivativeError(report) from None


def repair_material_bindings(*, allow_editor_mutation=False):
    """Retarget exactly22 current owned ISMs, then revert only five automatic flags."""
    report = {"schema": SCHEMA, "operation": "repair_diagnosed_unsaved_ism_usage", "success": False}
    try:
        u, world, report = _start(allow_editor_mutation, "repair_diagnosed_unsaved_ism_usage")
        authored = json.loads((EVIDENCE / "author-content.json").read_text(encoding="utf-8"))
        failed_save = json.loads((EVIDENCE / "save-result.json").read_text(encoding="utf-8"))
        _require(failed_save.get("save_called") is False and failed_save.get("success") is False,
                 "Expected native save refusal before any source write")
        _require(not (set(authored["dressing"]["dirty_before"]) & set(MAPPING)),
                 "Sources were already dirty before this dressing; do not discard other work")
        observed = json.loads((EVIDENCE / "source-material-inspection.json").read_text(encoding="utf-8"))
        source_inspection = {row["package"]: row for row in observed["materials"]}
        _require(observed.get("read_only") is True and set(source_inspection) == set(MAPPING)
                 and all(_sha(source) == source_inspection[source]["sha256"] for source in MAPPING),
                 "Source files changed since the independent read-only diagnosis")
        groups = authored["dressing"]["groups"]
        _require(len(groups) == 10 and sum(len(g["components"]) for g in groups) == 22,
                 "Exact recorded ten groups/22 ISMs required")
        actors = {a.get_path_name(): a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
        components = {}
        for group in groups:
            actor = actors[group["actor_path"]]
            _require(actor.get_actor_label() == group["actor_label"] and TAG in map(str, actor.tags)
                     and SCOPE in map(str, actor.tags), "Owned group identity differs")
            actual = {c.get_path_name(): c for c in actor.get_components_by_class(u.InstancedStaticMeshComponent)}
            _require(set(actual) == {c["path"] for c in group["components"]}, "Unexpected added/removed ISM component")
            for recorded in group["components"]:
                component = actual[recorded["path"]]
                source = recorded["material"].split(".")[0]
                _require(source in MAPPING and _package(component.get_material(0)) == source
                         and component.get_num_materials() == 1
                         and component.get_instance_count() == len(recorded["instances"]), "ISM binding/count changed")
                components[_path(component)] = (actor, component, source)
        _require(len(components) == 22 and sum(c.get_instance_count() for _, c, _ in components.values()) == 708,
                 "Exact22 ISMs/708 instances required")
        for component in u.ObjectIterator(u.InstancedStaticMeshComponent):
            if any(_package(component.get_material(i)) in MAPPING for i in range(component.get_num_materials())):
                _require(_path(component) in components, "Another loaded ISM still needs the original material: " + _path(component))
        originals, owned, reader = _clone_five(u, report, allow_diagnosed_dirty=True)
        with u.ScopedEditorTransaction("Own natural-world instanced-material shader variants"):
            for component_path, (actor, component, source) in components.items():
                component.modify()
                component.set_material(0, owned[source])
                _require(component.get_material(0) == owned[source], "Owned material assignment readback differs")
                report["assignments"].append({"actor_path": _path(actor), "component_path": component_path,
                    "slot": 0, "source_object": _path(originals[source]), "destination_object": _path(owned[source]),
                    "instance_count": component.get_instance_count()})
        for component in u.ObjectIterator(u.InstancedStaticMeshComponent):
            _require(not any(_package(component.get_material(i)) in MAPPING for i in range(component.get_num_materials())),
                     "A loaded ISM retains an original material; source reload is blocked")
        source_packages = [originals[name].get_package() for name in MAPPING]
        _require({p.get_name() for p in source_packages} == set(MAPPING), "Source reload package scope differs")
        # Release material UObject wrappers before native replacement. Existing
        # ordinary mesh references are remapped by UE to the same source paths.
        originals.clear()
        value = u.EditorLoadingAndSavingUtils.reload_packages(source_packages,
                    interaction_mode=u.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
        _require(isinstance(value, tuple) and len(value) == 2, "Unexpected reload result binding")
        any_reloaded, error_message = value
        report["reload"] = {"packages": list(MAPPING), "interaction_mode": "ASSUME_POSITIVE",
                            "any_packages_reloaded": bool(any_reloaded), "error_message": str(error_message)}
        _require(any_reloaded is True and not str(error_message).strip(), "Exact source package reload failed")
        _guard(u)
        for row in report["materials"]:
            source = row["source_package"]
            original = u.load_asset(source)
            row["source_sha256_after"] = _sha(source)
            row["source_graph_after_reload"] = _fingerprint(u, reader, original)
            row["source_used_with_instanced_static_meshes_after_reload"] = bool(original.get_editor_property("used_with_instanced_static_meshes"))
            row["source_dirty_after_reload"] = source in _dirty(u)
            _require(row["source_sha256_after"] == row["source_sha256_before"]
                     and row["source_graph_after_reload"] == row["source_graph_before"]
                     and row["source_used_with_instanced_static_meshes_after_reload"] is False
                     and not row["source_dirty_after_reload"], "Original disk/graph/clean state not restored")
        for assignment in report["assignments"]:
            component = components[assignment["component_path"]][1]
            _require(_path(component.get_material(0)) == assignment["destination_object"], "Reload changed owned ISM binding")
        report["dirty_after"] = _dirty(u)
        _require(all(_owned(name) for name in report["dirty_after"]), "Foreign dirty package remains after source reload")
        report.update(success=True, finished_utc=datetime.now(timezone.utc).isoformat(),
                      validation_pending=["settled shaders/render", "independent actor and material QA", "bounded save and cold reload"])
        return report
    except Exception:
        report["traceback"] = traceback.format_exc()
        raise MaterialDerivativeError(report) from None
