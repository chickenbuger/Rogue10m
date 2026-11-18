"""Build one unsaved, project-owned feathered-road material explicitly.

Import is inert. The editor owner calls build_material(allow_editor_mutation=True)
after loading the exact idle Crop editor world. Returns (material, report).
No main hook, level loading, save, actor assignment, source edit or cleanup.
Failure preserves the partial duplicate and raises MaterialBuildError.report.

UE5.8 DeferredDecal.usf:153-164 maps TexCoord=(localZ, localY), normalized
to 0..1; therefore G is road width for the existing projector orientation.
Opacity = RoadOpacity * (1 - smoothstep(.75 + .075*N, 1, abs(2*G-1))).
N is one-level absolute-world Fast Gradient 3D noise. The center 6m stays
brown inside the unchanged 8m projector footprint; only the outer .7..1m
on each side exposes the existing Landscape. No length/end fade is added.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
SOURCE = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
MATERIAL = OWN + "/M_RoadFeather"
SCALARS = {"WorldUVScale": 0.002, "RoadOpacity": 0.9, "RoadRoughness": 0.9}
ORIGINAL_CLASSES = Counter({"MaterialExpressionWorldPosition": 1,
    "MaterialExpressionComponentMask": 1, "MaterialExpressionScalarParameter": 3,
    "MaterialExpressionMultiply": 2, "MaterialExpressionTextureSample": 1,
    "MaterialExpressionVectorParameter": 1})

SOURCE_NOTES = (
    {"title": "Crafting the world of Tsushima", "url": "https://blog.playstation.com/2020/07/09/crafting-the-world-of-tsushima/",
     "summary_ko": "자연의 색과 지역별 제한된 식생, 인공물과 자연의 질서, 환경의 시선 유도에서 제작 원칙을 참고한다. 이 후보에는 마을별 색 정체성과 입구 프레이밍, 평평한 광장 주변의 자연 경계로 적용한다. 바람·동물 안내 시스템을 구현했다는 뜻은 아니다."},
    {"title": "Horizon Forbidden West: An authentic world", "url": "https://blog.playstation.com/2021/11/22/horizon-forbidden-west-an-authentic-world/",
     "summary_ko": "정착지의 재료와 소품을 주민의 문화·목적에 맞추고 멀리서도 지역을 구별하는 원칙을 참고한다. H는 목재·청록, M은 장터·황토, T는 석재·청색을 제안하며 도로 옆 선택 소품 이야기3곳에 적용한다. 다른 게임의 진행·보상 장치를 복제하지 않고 비급 확정 보상도 추가하지 않는다."},
)


class MaterialBuildError(RuntimeError):
    """A preserved failure; callers may retain report without retry or cleanup."""
    def __init__(self, report, partial_material=None):
        self.report = report
        self.partial_material = partial_material
        super().__init__(json.dumps(report, ensure_ascii=False))


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _path(value):
    return value.get_path_name() if value else None


def _dirty(u):
    lib = u.EditorLoadingAndSavingUtils
    return sorted({p.get_path_name() for p in list(lib.get_dirty_map_packages())
                   + list(lib.get_dirty_content_packages())})


def _guard(u):
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    _require(Path(u.Paths.project_dir()).resolve() == ROOT.resolve(), "Wrong project")
    _require(str(u.SystemLibrary.get_engine_version()).startswith("5.8."), "This helper is pinned to UE5.8")
    _require(world and world.get_path_name().split(".")[0] == TARGET
             and not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor(),
             "Exact idle Crop editor world required; no map is switched")
    return world


def _disk(package):
    _require(package.startswith("/Game/"), "Project-owned package required")
    return ROOT / "Content" / (package[6:] + ".uasset")


def _inputs(lib, material, node):
    names = list(map(str, lib.get_material_expression_input_names(node)))
    sources = list(lib.get_inputs_for_material_expression(material, node))
    _require(len(names) == len(sources), "Input names/values differ: " + node.get_name())
    return [{"pin": name, "node": value.get_name() if value else None,
             "output": str(lib.get_input_node_output_name_for_material_expression(node, value)) if value else ""}
            for name, value in zip(names, sources)]


def _properties(node):
    kind = node.get_class().get_name()
    if kind == "MaterialExpressionScalarParameter":
        return {"parameter_name": str(node.get_editor_property("parameter_name")),
                "default_value": float(node.get_editor_property("default_value"))}
    if kind == "MaterialExpressionVectorParameter":
        v = node.get_editor_property("default_value")
        return {"parameter_name": str(node.get_editor_property("parameter_name")),
                "default_value": [float(v.r), float(v.g), float(v.b), float(v.a)]}
    if kind == "MaterialExpressionTextureSample":
        return {"texture": _path(node.get_editor_property("texture")),
                "sampler_type": str(node.get_editor_property("sampler_type")),
                "sampler_source": str(node.get_editor_property("sampler_source"))}
    if kind == "MaterialExpressionComponentMask":
        return {k: bool(node.get_editor_property(k)) for k in ("r", "g", "b", "a")}
    if kind in ("MaterialExpressionMultiply", "MaterialExpressionSubtract", "MaterialExpressionAdd"):
        return {k: float(node.get_editor_property(k)) for k in ("const_a", "const_b")}
    if kind == "MaterialExpressionWorldPosition":
        return {"world_position_shader_offset": str(node.get_editor_property("world_position_shader_offset"))}
    if kind == "MaterialExpressionTextureCoordinate":
        return {"coordinate_index": int(node.get_editor_property("coordinate_index")),
                "u_tiling": float(node.get_editor_property("u_tiling")),
                "v_tiling": float(node.get_editor_property("v_tiling"))}
    if kind == "MaterialExpressionNoise":
        return {"noise_function": str(node.get_editor_property("noise_function")),
                "world_position_origin_type": str(node.get_editor_property("world_position_origin_type")),
                "scale": float(node.get_editor_property("scale")), "levels": int(node.get_editor_property("levels")),
                "output_min": float(node.get_editor_property("output_min")),
                "output_max": float(node.get_editor_property("output_max")),
                "turbulence": bool(node.get_editor_property("turbulence"))}
    if kind == "MaterialExpressionSmoothStep":
        return {key: float(node.get_editor_property(key)) for key in ("const_min", "const_max", "const_value")}
    return {}


def _outputs(u, lib, material):
    # SceneTypes.h UE5.8 values that Material.cpp::GetExpressionInputDescription
    # handles. Never query deprecated/pseudo values: the library dereferences
    # that native input without a null check. Only exposed enum entries are read.
    valid = set(range(34)) - {3, 4, 13, 14}
    result = {}
    for name in dir(u.MaterialProperty):
        if not name.startswith("MP_"):
            continue
        prop = getattr(u.MaterialProperty, name)
        if not isinstance(prop, u.MaterialProperty) or prop.value not in valid:
            continue
        key = str(prop.value)
        if key in result:
            continue
        node = lib.get_material_property_input_node(material, prop)
        result[key] = {"node": node.get_name() if node else None,
                       "output": str(lib.get_material_property_input_node_output_name(material, prop)) if node else ""}
    _require(all(str(p.value) in result for p in (u.MaterialProperty.MP_BASE_COLOR,
             u.MaterialProperty.MP_OPACITY, u.MaterialProperty.MP_ROUGHNESS)), "Required material outputs unavailable")
    return result


def _fingerprint(u, lib, material):
    nodes = list(lib.get_material_expressions(material))
    _require(len({n.get_name() for n in nodes}) == len(nodes), "Duplicate expression names")
    return {"domain": str(material.get_editor_property("material_domain")),
            "blend": str(material.get_editor_property("blend_mode")),
            "nodes": {n.get_name(): {"class": n.get_class().get_name(), "properties": _properties(n),
                      "inputs": _inputs(lib, material, n)} for n in nodes},
            "outputs": _outputs(u, lib, material)}


def _require_inputs(lib, material, node, links):
    """Check all public pin slots, including those expected to be unwired."""
    actual = _inputs(lib, material, node)
    wanted = [{"pin": row["pin"], "node": None, "output": ""} for row in actual]
    names = [row["pin"] for row in actual]
    for pin, source in links.items():
        index = names.index(pin) if pin else 0
        wanted[index]["node"] = source.get_name()
        wanted[index]["output"] = str(list(lib.get_material_expression_output_names(source))[0])
    _require(actual == wanted, "Exact input slots differ: " + node.get_name())


def _source_contract(u, lib, material):
    _require(isinstance(material, u.Material), "Source must be a Material")
    _require(material.get_editor_property("material_domain") == u.MaterialDomain.MD_DEFERRED_DECAL
             and material.get_editor_property("blend_mode") == u.BlendMode.BLEND_TRANSLUCENT,
             "Source domain/blend differs")
    nodes = list(lib.get_material_expressions(material))
    _require(len(nodes) == 9 and Counter(n.get_class().get_name() for n in nodes) == ORIGINAL_CLASSES,
             "Source must contain exactly the existing nine-node graph")
    scalars = {str(n.get_editor_property("parameter_name")): n for n in nodes
               if isinstance(n, u.MaterialExpressionScalarParameter)}
    _require(set(scalars) == set(SCALARS) and all(abs(float(scalars[k].get_editor_property("default_value"))-v) < 1e-6
             for k, v in SCALARS.items()), "Source scalar defaults differ")
    _require(lib.get_material_property_input_node(material, u.MaterialProperty.MP_OPACITY) == scalars["RoadOpacity"]
             and lib.get_material_property_input_node(material, u.MaterialProperty.MP_ROUGHNESS) == scalars["RoadRoughness"],
             "Source opacity/roughness outputs differ")
    world = next(n for n in nodes if isinstance(n, u.MaterialExpressionWorldPosition))
    mask = next(n for n in nodes if isinstance(n, u.MaterialExpressionComponentMask))
    texture = next(n for n in nodes if isinstance(n, u.MaterialExpressionTextureSample))
    tint = next(n for n in nodes if isinstance(n, u.MaterialExpressionVectorParameter))
    base = lib.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR)
    multiplies = [n for n in nodes if isinstance(n, u.MaterialExpressionMultiply)]
    _require(base in multiplies, "Source BaseColor must be the existing tint multiply")
    uv = next(n for n in multiplies if n != base)
    _require([bool(mask.get_editor_property(k)) for k in ("r", "g", "b", "a")] == [True, True, False, False]
             and _path(texture.get_editor_property("texture")) == "/Game/StarterContent/Textures/T_Ground_Gravel_D.T_Ground_Gravel_D"
             and str(tint.get_editor_property("parameter_name")) == "RoadTint", "Source appearance/WorldXY mask differs")
    tint_value = tint.get_editor_property("default_value")
    _require(all(abs(a-b) < 1e-6 for a, b in zip((tint_value.r, tint_value.g, tint_value.b, tint_value.a),
                 (0.86, 0.78, 0.65, 1.0))), "Source RoadTint differs")
    for node, links in ((world, {}), (mask, {"": world}), (scalars["WorldUVScale"], {}),
                        (uv, {"A": mask, "B": scalars["WorldUVScale"]}), (texture, {"UVs": uv}),
                        (tint, {}), (base, {"A": texture, "B": tint}),
                        (scalars["RoadOpacity"], {}), (scalars["RoadRoughness"], {})):
        _require_inputs(lib, material, node, links)
    snapshot = _fingerprint(u, lib, material)
    active = {key for key, value in snapshot["outputs"].items() if value["node"] is not None}
    _require(active == {str(p.value) for p in (u.MaterialProperty.MP_BASE_COLOR,
             u.MaterialProperty.MP_OPACITY, u.MaterialProperty.MP_ROUGHNESS)}, "Source has unexpected active material outputs")
    return scalars, snapshot


def _connect(lib, material, source, destination, pin="", output=""):
    _require(lib.connect_material_expressions(source, output, destination, pin), "Connection failed: " + pin)
    names = list(map(str, lib.get_material_expression_input_names(destination)))
    index = names.index(pin) if pin else 0
    values = list(lib.get_inputs_for_material_expression(material, destination))
    _require(values[index] == source, "Connection node readback failed: " + pin)
    expected = output or str(list(lib.get_material_expression_output_names(source))[0])
    _require(str(lib.get_input_node_output_name_for_material_expression(destination, source)) == expected,
             "Connection output pin readback failed: " + pin)


def build_material(allow_editor_mutation=False):
    """Return (new Material, JSON-compatible report); never save or assign it."""
    _require(allow_editor_mutation is True, "The editor owner must explicitly opt into material creation")
    import unreal as u
    lib = u.MaterialEditingLibrary
    material = None
    report = {"status": "started", "passed": False, "source": SOURCE, "destination": MATERIAL,
              "started_utc": datetime.now(timezone.utc).isoformat(), "step": "guards",
              "created_nodes": [], "packages_saved": [], "actors_assigned": [],
              "source_modified": False, "terrain_modified": False, "collision_modified": False,
              "mutation_flags_describe_builder_scope_only": True, "source_preservation_verified": False,
              "automatic_rollback": False, "partial_assets_deleted": False,
              "render_settle_pending": True, "visual_validation_passed": False,
              "platform_limit": "Fast Gradient 3D is for the current Win64 target; Mobile unsupported; GPU cost unmeasured",
              "source_notes": list(SOURCE_NOTES)}
    try:
        world = _guard(u)
        _require(not u.EditorAssetLibrary.does_asset_exist(MATERIAL)
                 and not _disk(MATERIAL).exists() and u.find_object(None, MATERIAL + ".M_RoadFeather") is None,
                 "Destination exists; inspect partial work instead of retrying")
        _require(u.EditorAssetLibrary.does_asset_exist(SOURCE) and _disk(SOURCE).is_file(), "Saved source material unavailable")
        dirty_before = _dirty(u)
        _require(SOURCE not in dirty_before, "Source material is unsaved")
        source = u.load_asset(SOURCE)
        _, source_before = _source_contract(u, lib, source)
        source_hash = hashlib.sha256(_disk(SOURCE).read_bytes()).hexdigest()
        report.update(dirty_before=dirty_before, source_before=source_before, source_sha256=source_hash)
        report["step"] = "duplicate source"
        material = u.EditorAssetLibrary.duplicate_asset(SOURCE, MATERIAL)
        _require(isinstance(material, u.Material) and _path(material) == MATERIAL + ".M_RoadFeather", "Unexpected duplicate destination")
        scalars, original = _source_contract(u, lib, material)
        _require(original == source_before, "Duplicate did not preserve the original public graph")
        definitions = (
            ("coord", u.MaterialExpressionTextureCoordinate, {"coordinate_index": 0, "u_tiling": 1.0, "v_tiling": 1.0}),
            ("width", u.MaterialExpressionComponentMask, {"r": False, "g": True, "b": False, "a": False}),
            ("twice", u.MaterialExpressionMultiply, {"const_b": 2.0}),
            ("center", u.MaterialExpressionSubtract, {"const_b": 1.0}),
            ("distance", u.MaterialExpressionAbs, {}),
            ("noise", u.MaterialExpressionNoise, {"noise_function": u.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX3D,
                "scale": 1.0/300.0, "levels": 1, "output_min": 0.0, "output_max": 1.0, "turbulence": False}),
            ("noise_width", u.MaterialExpressionMultiply, {"const_b": 0.075}),
            ("core", u.MaterialExpressionAdd, {"const_b": 0.75}),
            ("falloff", u.MaterialExpressionSmoothStep, {"const_max": 1.0}),
            ("side", u.MaterialExpressionOneMinus, {}),
            ("opacity", u.MaterialExpressionMultiply, {}),
        )
        created = {}
        for index, (name, cls, values) in enumerate(definitions):
            report["step"] = "create/configure " + name
            node = lib.create_material_expression(material, cls, -1200 + (index % 6)*230, 650 + (index//6)*250)
            _require(node is not None, "Expression creation failed: " + name)
            created[name] = node
            report["created_nodes"].append({"role": name, "path": _path(node)})
            for key, value in values.items():
                node.set_editor_property(key, value)
                observed = node.get_editor_property(key)
                _require(abs(float(observed)-float(value)) < 1e-6 if isinstance(value, float)
                         else observed == value, "Expression property readback failed: " + name + "/" + key)
        noise = created["noise"]
        _require(noise.get_editor_property("world_position_origin_type") == u.PositionOrigin.ABSOLUTE
                 and all(value is None for value in lib.get_inputs_for_material_expression(material, noise)),
                 "Noise must use its unconnected absolute-world position default")
        links = (("coord", "width", ""), ("width", "twice", "A"), ("twice", "center", "A"),
                 ("center", "distance", ""), ("noise", "noise_width", "A"), ("noise_width", "core", "A"),
                 ("core", "falloff", "Min"), ("distance", "falloff", "Value"), ("falloff", "side", ""),
                 ("side", "opacity", "B"))
        report["step"] = "connect width/noise opacity mask"
        for left, right, pin in links:
            _connect(lib, material, created[left], created[right], pin)
        _connect(lib, material, scalars["RoadOpacity"], created["opacity"], "A")
        _require(lib.connect_material_property(created["opacity"], "", u.MaterialProperty.MP_OPACITY), "Opacity output connection failed")
        report["step"] = "compile duplicate"
        errors = [str(error) for error in lib.recompile_material(material)]
        report["compile_errors"] = errors
        _require(errors == [], "Duplicate shader compile failed: " + json.dumps(errors))
        after = _fingerprint(u, lib, material)
        expected_classes = ORIGINAL_CLASSES + Counter(cls.__name__ for _, cls, _ in definitions)
        _require(len(after["nodes"]) == 20 and Counter(row["class"] for row in after["nodes"].values()) == expected_classes,
                 "Expected exactly 20 expressions")
        _require(all(after["nodes"].get(name) == value for name, value in original["nodes"].items()),
                 "An original node/property/input changed")
        opacity_key = str(u.MaterialProperty.MP_OPACITY.value)
        expected_outputs = dict(original["outputs"])
        expected_outputs[opacity_key] = {"node": created["opacity"].get_name(),
            "output": str(list(lib.get_material_expression_output_names(created["opacity"]))[0])}
        _require(after["outputs"] == expected_outputs, "A material output other than Opacity changed")
        exact_inputs = {name: {} for name in created}
        for left, right, pin in links:
            exact_inputs[right][pin] = created[left]
        exact_inputs["opacity"]["A"] = scalars["RoadOpacity"]
        for name, node in created.items():
            _require_inputs(lib, material, node, exact_inputs[name])
        for name, _, values in definitions:
            for key, value in values.items():
                observed = created[name].get_editor_property(key)
                _require(abs(float(observed)-float(value)) < 1e-6 if isinstance(value, float)
                         else observed == value, "Post-compile property readback failed: " + name + "/" + key)
        _require(_fingerprint(u, lib, source) == source_before
                 and hashlib.sha256(_disk(SOURCE).read_bytes()).hexdigest() == source_hash,
                 "Original source changed")
        _require(_guard(u) == world and set(_dirty(u))-set(dirty_before) <= {MATERIAL}, "Unexpected editor/dirty package change")
        _require(not _disk(MATERIAL).exists(), "Unexpected material disk save")
        report.update(status="unsaved_material_compiled_requires_render_settle_and_visual_review", passed=True,
                      step="complete", original_node_count=9, added_node_count=11, final_node_count=20,
                      scalar_defaults=dict(SCALARS), formula="RoadOpacity*(1-smoothstep(.75+.075*N,1,abs(2*TexCoord.G-1)))",
                      footprint_width_cm=800.0, guaranteed_full_opacity_core_width_cm=600.0,
                      source_after=source_before, source_preservation_verified=True,
                      material_after=after, dirty_after=_dirty(u),
                      finished_utc=datetime.now(timezone.utc).isoformat())
        return material, report
    except Exception as error:
        report.update(status="failed_partial_material_preserved_no_retry", error=repr(error),
                      traceback=traceback.format_exc(), dirty_after=_dirty(u),
                      finished_utc=datetime.now(timezone.utc).isoformat())
        raise MaterialBuildError(report, material) from error
