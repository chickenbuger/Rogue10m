"""Author unconnected magic VFX candidates through official local Unreal MCP.

Coordinator owns the editor and must grant --lease-granted. Each label/phase has
one fresh evidence directory. No mutation retry, resume, source edits, DataAsset
changes, actors, EffectTypes, gameplay integration, or rendered-quality claim.
Niagara accepts independently prepared materials after fresh exact readback.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import time
import traceback

from InspectWeaponFantasyVFX20261110 import Client, ROOT, object_path, ref

sys.path.insert(0, str(ROOT / "Scripts"))
from MagicParticleCandidateSpec20261110 import build_spec, validate_source_summary

ASSET = "editor_toolset.toolsets.asset.AssetTools"
MATERIAL = "editor_toolset.toolsets.material.MaterialTools"
OBJECT = "editor_toolset.toolsets.object.ObjectTools"
NIAGARA = "NiagaraToolsets.NiagaraToolset_System"
EDITOR = "EditorToolset.EditorAppToolset"
BASE = "/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110"
SOURCE_PREFIX = "/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/"


def decoded(value):
    for _ in range(5):
        if isinstance(value, str):
            try:
                value = json.loads(value)
                continue
            except ValueError:
                return value
        if isinstance(value, dict) and "returnValue" in value:
            value = value["returnValue"]
            continue
        return value
    raise RuntimeError("Unexpectedly nested MCP result")


def references(value):
    value = decoded(value)
    if isinstance(value, dict):
        result = {value["refPath"]} if isinstance(value.get("refPath"), str) else set()
        for item in value.values():
            result.update(references(item))
        return result
    if isinstance(value, list):
        return set().union(*(references(item) for item in value)) if value else set()
    return {value} if isinstance(value, str) and value.startswith("/Game/") else set()


def close(a, b, tolerance=1.e-4):
    return not isinstance(a, bool) and isinstance(a, (int, float)) and math.isfinite(a) and abs(a - b) <= tolerance


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def properties_blob(value):
    value = decoded(value)
    require(isinstance(value, dict) and "propertyValues" in value, "Missing native propertyValues readback")
    result = decoded(value["propertyValues"])
    require(isinstance(result, dict), "Native propertyValues is not a JSON mapping")
    return result


def module_inputs(value):
    value = decoded(value)
    require(isinstance(value, dict) and isinstance(value.get("inputs"), list), "Missing module inputs")
    return {item["name"]: decoded(item.get("value", {}).get("value", {})) for item in value["inputs"]}


def scalar(inputs, name):
    require(name in inputs and isinstance(inputs[name], dict), "Missing stack input " + name)
    return inputs[name].get("value")


class API:
    def __init__(self, url, out, root):
        self.client = Client(url, out)
        self.root = root
        self.out = out
        self.index = 0
        self.mutations = []

    def call(self, toolset, tool, arguments, label, mutation=False):
        self.index += 1
        label = f"{self.index:03d}-" + re.sub(r"[^A-Za-z0-9_-]", "-", label)
        require(not (self.out / (label + ".json")).exists(), "Evidence overwrite refused")
        entry = {"toolset": toolset, "tool": tool, "label": label, "outcome": "attempted"}
        if mutation:
            self.mutations.append(entry)
        try:
            answer = decoded(self.client.tool("call_tool", {
                "toolset_name": toolset, "tool_name": tool, "arguments": arguments}, label))
        except Exception:
            if mutation:
                entry["outcome"] = "failed_or_uncertain_inspect_before_any_retry"
            raise
        if mutation:
            entry["outcome"] = "response_received_requires_readback"
        require(answer is not False or not mutation, "MCP mutation returned false: " + tool)
        return answer

    def exists(self, path):
        answer = decoded(self.call(ASSET, "exists", {"path": path}, "exists-" + path.rsplit("/", 1)[-1]))
        if isinstance(answer, dict) and "exists" in answer:
            answer = answer["exists"]
        require(type(answer) is bool, "Unrecognized exists result; inspect evidence")
        return answer

    def own(self, path):
        require(path.startswith(self.root + "/") and ":" not in path and ".." not in path,
                "Only this label's candidate packages may be saved or tagged")

    def save(self, paths):
        for path in paths:
            self.own(path)
        return self.call(ASSET, "save_assets", {"asset_paths": paths}, "save-candidate-packages", True)

    def tag(self, path, effect, spec_hash):
        self.own(path)
        return self.call(ASSET, "update_metadata_tags", {"asset_path": path, "set_tags": {
            "Rogue10m.Source": SOURCE_PREFIX + ("M_KnuckleFlurry_Acceptance" if path.rsplit("/", 1)[-1].startswith("M_") else "NS_KnuckleFlurry_Acceptance"),
            "Rogue10m.MagicCandidateRole": effect["id"], "Rogue10m.CandidateSpecSHA256": spec_hash,
            "Rogue10m.Status": "Unconnected visual trial; rendered/runtime review pending"}}, "metadata-" + effect["id"], True)

    def obj(self, instance, names, label):
        value = decoded(self.call(OBJECT, "get_properties", {"instance": {"refPath": instance}, "properties": names}, label))
        require(isinstance(value, dict), "Object readback must be a JSON mapping")
        return value

    def schema(self, instance, names, label):
        value = decoded(self.call(OBJECT, "list_properties", {"instance": {"refPath": instance}}, label))
        require(isinstance(value, dict) and all(name in value for name in names), "Fresh object schema differs; stop before edits")

    def niagara(self, tool, args, label, mutation=False):
        return self.call(NIAGARA, tool, args, label, mutation)

    def set_input(self, path, stage, module, name, kind, value, label):
        self.own(path)
        emitter = "" if stage.startswith("System") else "DirectionalBurst"
        return self.niagara("SetStackInputData", {"stackInputRef": ref(path, emitter, stage, module, names=[name]),
            "inputData": {"struct": {"refPath": kind}, "value": value}}, label, True)


def inspect_material(api, effect, expected=False):
    path = effect["material_path"]
    material_ref = {"refPath": object_path(path)}
    expressions = api.call(MATERIAL, "get_expressions", {"material_or_function": material_ref}, effect["id"] + "-expressions")
    expression_refs = references(expressions)
    require(len(expression_refs) == 7 and all(p.startswith(object_path(path) + ":") for p in expression_refs),
            "Source-derived graph topology differs from seven-node inspected material")
    opacity = api.call(MATERIAL, "get_property_input", {"material": material_ref, "material_property": "MP_Opacity"}, effect["id"] + "-opacity-wire")
    customs = {p for p in expression_refs if ":MaterialExpressionCustom_" in p}
    tints = {p for p in expression_refs if ":MaterialExpressionVectorParameter_" in p}
    require(len(customs) == 1 and len(tints) == 1 and customs.issubset(references(opacity)), "Opacity must use the clone's sole Custom mask")
    custom, tint = next(iter(customs)), next(iter(tints))
    api.schema(custom, ("code", "outputType", "inputs", "description"), effect["id"] + "-custom-schema")
    api.schema(tint, ("defaultValue", "parameterName", "group"), effect["id"] + "-tint-schema")
    custom_data = api.obj(custom, ["code", "outputType", "inputs", "description"], effect["id"] + "-custom-values")
    tint_data = api.obj(tint, ["defaultValue", "parameterName", "group"], effect["id"] + "-tint-values")
    require(custom_data.get("outputType") == "CMOT_Float1" and [i.get("inputName") for i in custom_data.get("inputs", [])] == ["UV", "Age"],
            "Custom must retain float1 and exactly UV/Age")
    require(tint_data.get("parameterName") == "CueTint", "Unexpected vector parameter")
    wiring = references(api.call(MATERIAL, "get_expression_inputs", {"material_or_function": material_ref,
        "expression": {"refPath": custom}}, effect["id"] + "-custom-wiring"))
    require(any(":MaterialExpressionTextureCoordinate_" in p for p in wiring)
            and any(":MaterialExpressionParticleRelativeTime_" in p for p in wiring)
            and wiring.issubset(expression_refs), "UV/Age connections not established")
    emissive = references(api.call(MATERIAL, "get_property_input", {"material": material_ref,
        "material_property": "MP_EmissiveColor"}, effect["id"] + "-emissive-wire"))
    multiply = {p for p in emissive if ":MaterialExpressionMultiply_" in p}
    require(len(multiply) == 1, "Emissive must preserve tint/particle-color multiply")
    multiply_wires = references(api.call(MATERIAL, "get_expression_inputs", {"material_or_function": material_ref,
        "expression": {"refPath": next(iter(multiply))}}, effect["id"] + "-emissive-inputs"))
    require(tint in multiply_wires and any(":MaterialExpressionParticleColor_" in p for p in multiply_wires)
            and multiply_wires.issubset(expression_refs), "Tint/particle RGB wiring differs")
    flags = api.obj(object_path(path), ["blendMode", "shadingModel", "twoSided", "bDisableDepthTest"], effect["id"] + "-material-flags")
    require(flags.get("blendMode") == "BLEND_Additive" and flags.get("shadingModel") == "MSM_Unlit"
            and flags.get("twoSided") is True and flags.get("bDisableDepthTest") is False, "Material render flags differ")
    if expected:
        require(custom_data["code"].replace("\r\n", "\n").strip() == effect["material"]["mask_hlsl"].strip(), "Mask code differs from specification")
        color = tint_data.get("defaultValue", {})
        wanted = dict(zip(("r", "g", "b"), [c * effect["material"]["emissive_gain"] for c in effect["material"]["tint_linear_rgb"]]))
        require(all(close(color.get(k), v, 1.e-5) for k, v in wanted.items()) and close(color.get("a"), 1), "Candidate tint/gain differs")
    return {"asset": path, "custom": custom, "tint": tint, "custom_values": custom_data,
            "tint_values": tint_data, "flags": flags, "custom_wiring": sorted(wiring), "emissive_wiring": sorted(multiply_wires)}


def materials(api, spec, spec_hash):
    for effect in spec["effects"]:
        api.own(effect["material_path"])
        api.call(ASSET, "duplicate", {"path": spec["source"]["material"], "new_path": effect["material_path"]}, effect["id"] + "-duplicate-material", True)
        require(api.exists(effect["material_path"]), "Material duplicate missing; do not retry")
        nodes = inspect_material(api, effect)
        api.call(OBJECT, "set_properties", {"instance": {"refPath": nodes["custom"]}, "values": json.dumps({
            "code": effect["material"]["mask_hlsl"], "description": "Finite floating magic " + effect["id"] + " visual trial; UV+Age"})}, effect["id"] + "-set-custom", True)
        color = dict(zip(("r", "g", "b"), [v * effect["material"]["emissive_gain"] for v in effect["material"]["tint_linear_rgb"]]))
        color["a"] = 1.0
        api.call(OBJECT, "set_properties", {"instance": {"refPath": nodes["tint"]},
            "values": json.dumps({"defaultValue": color})}, effect["id"] + "-set-tint", True)
        api.call(MATERIAL, "recompile", {"material_or_function": {"refPath": object_path(effect["material_path"])}}, effect["id"] + "-recompile", True)
        inspect_material(api, effect, True)
        api.tag(effect["material_path"], effect, spec_hash)
        api.save([effect["material_path"]])
    return [inspect_material(api, effect, True) for effect in spec["effects"]]


def compile_and_issues(api, path, label):
    opened = references(api.call(EDITOR, "GetOpenAssets", {}, label + "-open-assets"))
    if path not in opened and object_path(path) not in opened:
        api.call(EDITOR, "OpenEditorForAsset", {"assetPath": path}, label + "-open-editor", True)
    state = None
    for attempt in range(20):
        state = api.niagara("GetSystemCompileState", {"system": {"refPath": object_path(path)}}, label + f"-compile-{attempt}")
        require(isinstance(state, dict), "Unrecognized Niagara compile-state response")
        if not state.get("bIsCompiling") and not state.get("bIsStale"):
            break
        time.sleep(.5)  # Read polling only; no mutation is retried.
    require(isinstance(state, dict) and not state.get("bIsCompiling") and not state.get("bIsStale")
            and not state.get("bHasErrors") and state.get("aggregateStatus") in ("UpToDate", "UpToDateWithWarnings"), "Niagara compile not ready/error-free; inspect before proceeding")
    issues = api.niagara("GetStackIssues", {"system": {"refPath": object_path(path)}}, label + "-stack-issues")
    require(isinstance(issues, dict) and issues.get("numErrors", 1) == 0, "Niagara stack errors or unavailable stack review")
    return state, issues


def inspect_niagara(api, effect):
    path, label = effect["system_path"], effect["id"]
    summary = api.niagara("GetSystemSummary", {"system": {"refPath": object_path(path)}}, label + "-summary")
    validate_source_summary(summary)
    topology = api.niagara("GetEmitterTopology", {"emitterRef": ref(path, "DirectionalBurst")}, label + "-topology")
    expected = {"emitterSpawnScript": [], "emitterUpdateScript": [("EmitterState", True), ("SpawnBurst_Instantaneous", True)],
        "particleSpawnScript": [("InitializeParticle", True), ("AddVelocityInCone", True)],
        "particleUpdateScript": [("ParticleState", True), ("GravityForce", False), ("Drag", False),
            ("ScaleColor", True), ("SolveForcesAndVelocity", True), ("ScaleSpriteSizeBySpeed", False)]}
    for stage, modules in expected.items():
        require([(m.get("moduleName"), m.get("enabled")) for m in topology.get(stage, {}).get("modules", [])] == modules,
                "Unexpected modules, recurring spawn or behavior: " + stage)
    rows = api.niagara("GetEmitterInputValues", {"emitterRef": ref(path, "DirectionalBurst")}, label + "-inputs")
    require(isinstance(rows, list), "Expected native emitter input array")
    inputs = {row["moduleName"]: module_inputs(row) for row in rows}
    initialize, burst = inputs.get("InitializeParticle", {}), inputs.get("SpawnBurst_Instantaneous", {})
    for name in ("Lifetime Min", "Lifetime Max"):
        require(close(scalar(initialize, name), effect["particle_lifetime_world_seconds"]), "Lifetime mismatch")
    for name in ("Sprite Size Min", "Sprite Size Max"):
        size = initialize.get(name, {})
        require(close(size.get("x"), effect["sprite_size_cm"][0]) and close(size.get("y"), effect["sprite_size_cm"][1]), "Sprite size mismatch")
    require(all(close(initialize.get("Color", {}).get(k), 1) for k in ("r", "g", "b", "a")), "Particle color must remain identity white")
    require(scalar(burst, "Spawn Count") == 1 and close(scalar(burst, "Spawn Time"), 0), "Exactly one initial burst required")
    require(close(scalar(inputs.get("AddVelocityInCone", {}), "Velocity Strength"), 0), "Stationary candidate required")
    require(bool(scalar(inputs.get("ParticleState", {}), "Kill Particles When Lifetime Has Elapsed")), "Particle lifetime kill must be enabled")
    require(inputs.get("ParticleState", {}).get("DeltaTime", {}).get("linkedVariable", {}).get("name") == "Engine.DeltaTime",
            "Particle lifetime must follow world delta time")
    emitter_state = inputs.get("EmitterState", {})
    require(emitter_state.get("Life Cycle Mode", {}).get("displayName") == "System", "Expected effective emitter lifecycle inherited from System")
    system_state = module_inputs(api.niagara("GetModuleInputValues", {"moduleRef": ref(path, "", "SystemUpdateScript", "SystemState")}, label + "-system-state"))
    require(system_state.get("Loop Behavior", {}).get("displayName") == "Once"
            and system_state.get("Inactive Response", {}).get("displayName", "").startswith("Complete")
            and close(scalar(system_state, "Loop Duration"), effect["system_duration_world_seconds"])
            and not bool(scalar(system_state, "UseLoopDelay")), "Effective finite system lifecycle differs")
    emitter = properties_blob(api.niagara("GetEmitterData", {"emitterRef": ref(path, "DirectionalBurst")}, label + "-emitter-data"))
    require(emitter.get("bLocalSpace") is False and emitter.get("SimTarget") == "CPUSim", "World-space CPU emitter required")
    system = properties_blob(api.niagara("GetSystemData", {"system": {"refPath": object_path(path)}}, label + "-system-data"))
    bounds = system.get("FixedBounds", {})
    require(system.get("bFixedBounds") is True and bounds.get("isValid") is True, "Valid fixed bounds required")
    for axis, extent in zip(("x", "y", "z"), effect["fixed_bounds_half_extent_cm"]):
        require(close(bounds.get("min", {}).get(axis), -extent) and close(bounds.get("max", {}).get(axis), extent), "Fixed bounds mismatch")
    renderer = properties_blob(api.niagara("GetRendererData", {"rendererRef": ref(path, "DirectionalBurst", index=0)}, label + "-renderer"))
    require(renderer.get("Material", {}).get("refPath") == object_path(effect["material_path"])
            and renderer.get("FacingMode") == "FaceCamera" and renderer.get("Alignment") == "Unaligned"
            and renderer.get("SortMode") == "None" and renderer.get("bCastShadows") is False, "Renderer material/facing/flags differ")
    state, issues = compile_and_issues(api, path, label)
    return {"asset": path, "summary": summary, "topology": topology, "inputs": rows, "system_state": system_state,
            "system": system, "emitter": emitter, "renderer": renderer, "compile": state, "issues": issues,
            "effective_lifecycle": "System Once, emitter inherits System, particle lifetime kill enabled"}


def niagara(api, spec, spec_hash):
    # Accept coordinator-authored saved materials without a local materials-phase report.
    for effect in spec["effects"]:
        inspect_material(api, effect, True)
    for effect in spec["effects"]:
        path, label = effect["system_path"], effect["id"]
        created = api.niagara("CreateNiagaraSystem", {"assetName": effect["system_name"], "assetPath": spec["destination_root"],
            "templateSystem": {"refPath": object_path(spec["source"]["system"])}}, label + "-create-system", True)
        require(isinstance(created, dict) and created.get("refPath") == object_path(path), "Unexpected system creation result; do not retry")
        api.set_input(path, "EmitterUpdateScript", "SpawnBurst_Instantaneous", "Spawn Count", "/Script/Niagara.NiagaraInt32", {"value": 1}, label + "-burst-count")
        for name, kind, value in (
            ("Color", "/Script/CoreUObject.LinearColor", {"r": 1., "g": 1., "b": 1., "a": 1.}),
            ("Lifetime Min", "/Script/Niagara.NiagaraFloat", {"value": effect["particle_lifetime_world_seconds"]}),
            ("Lifetime Max", "/Script/Niagara.NiagaraFloat", {"value": effect["particle_lifetime_world_seconds"]}),
            ("Sprite Size Min", "/Script/CoreUObject.Vector2f", dict(zip(("x", "y"), effect["sprite_size_cm"]))),
            ("Sprite Size Max", "/Script/CoreUObject.Vector2f", dict(zip(("x", "y"), effect["sprite_size_cm"]))),
        ):
            api.set_input(path, "ParticleSpawnScript", "InitializeParticle", name, kind, value, label + "-" + name)
        api.set_input(path, "ParticleSpawnScript", "AddVelocityInCone", "Velocity Strength", "/Script/Niagara.NiagaraFloat", {"value": 0.}, label + "-velocity-zero")
        api.set_input(path, "SystemUpdateScript", "SystemState", "Loop Duration", "/Script/Niagara.NiagaraFloat",
                      {"value": effect["system_duration_world_seconds"]}, label + "-system-duration")
        for module in effect["disable_inherited_modules"]:
            api.niagara("SetModuleEnabled", {"moduleRef": ref(path, "DirectionalBurst", "ParticleUpdateScript", module), "bEnabled": False}, label + "-disable-" + module, True)
        api.niagara("SetEmitterData", {"emitter": ref(path, "DirectionalBurst"), "emitterData": {"propertyValues": json.dumps({"bLocalSpace": False})}}, label + "-world-space", True)
        extents = effect["fixed_bounds_half_extent_cm"]
        api.niagara("SetSystemData", {"system": {"refPath": object_path(path)}, "systemData": {"propertyValues": json.dumps({"bFixedBounds": True,
            "FixedBounds": {"min": dict(zip(("x", "y", "z"), [-v for v in extents])), "max": dict(zip(("x", "y", "z"), extents)), "isValid": True}})}}, label + "-fixed-bounds", True)
        api.niagara("SetRendererData", {"renderer": ref(path, "DirectionalBurst", index=0), "rendererData": {"propertyValues": json.dumps({
            "Material": {"refPath": object_path(effect["material_path"])}, "Alignment": "Unaligned", "FacingMode": "FaceCamera", "SortMode": "None", "bCastShadows": False})}}, label + "-renderer-set", True)
        inspect_niagara(api, effect)
        api.tag(path, effect, spec_hash)
        api.save([path])
    return [inspect_niagara(api, effect) for effect in spec["effects"]]


def source_files(spec):
    content = (ROOT / "Content").resolve()
    require(content == ROOT.resolve() / "Content", "Redirected/shared Content directory refused")
    result = []
    for key in ("system", "material"):
        path = spec["source"][key]
        require(path.startswith(SOURCE_PREFIX) and ":" not in path and ".." not in path, "Unexpected source prefix")
        stem = content / path.removeprefix("/Game/")
        require(stem.resolve().is_relative_to(content) and stem.with_suffix(".uasset").is_file(), "Saved physical own source package required")
        result.extend(p for suffix in (".uasset", ".uexp", ".ubulk", ".uptnl") if (p := stem.with_suffix(suffix)).is_file())
    return result


def hashes(files):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None for path in files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8001/mcp")
    parser.add_argument("--lease-granted", action="store_true")
    parser.add_argument("--label", required=True)
    parser.add_argument("--phase", required=True, choices=("materials", "niagara", "verify"))
    args = parser.parse_args()
    require(args.lease_granted, "Coordinator-granted exclusive editor lease is required")
    spec = build_spec(args.label)
    require(spec["destination_root"] == BASE + "/" + args.label, "Candidate destination differs")
    destination = ROOT / "Content" / spec["destination_root"].removeprefix("/Game/")
    require(destination.resolve().is_relative_to((ROOT / "Content").resolve()), "Redirected destination refused")
    out = ROOT / "Feature/doc/evidence/magic-particle-candidates-20261110" / args.label / ("author-" + args.phase)
    require(not out.exists() and out.resolve().is_relative_to(ROOT.resolve()), "Fresh phase evidence directory required")
    files = source_files(spec)
    before = hashes(files)
    spec_hash = hashlib.sha256(json.dumps(spec, sort_keys=True).encode()).hexdigest()
    out.mkdir(parents=True, exist_ok=False)
    report = {"phase": args.phase, "label": args.label, "status": "started", "candidate_unconnected": True,
        "render_validated": False, "physical_input_tested": False, "gameplay_or_data_assets_changed": False,
        "cold_reload_performed": False, "original_hashes_before": before, "spec_sha256": spec_hash,
        "component_dilation_scale_requirement": "1.0 when previewed; assets alone cannot set future component overrides",
        "asset_root": spec["destination_root"], "actual_states": []}
    (out / "phase-start.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    api = None
    failure = None
    try:
        api = API(args.url, out, spec["destination_root"])
        api.client.tool("list_toolsets", {}, "000-toolsets")
        for index, toolset in enumerate((ASSET, MATERIAL, OBJECT, NIAGARA, EDITOR)):
            decoded(api.client.tool("describe_toolset", {"toolset_name": toolset}, f"000-schema-{index}"))
        require(api.exists(spec["source"]["system"]) and api.exists(spec["source"]["material"]), "Source assets unavailable")
        source = api.niagara("GetSystemSummary", {"system": {"refPath": object_path(spec["source"]["system"])}}, "source-summary")
        validate_source_summary(source)
        material_paths = [effect["material_path"] for effect in spec["effects"]]
        system_paths = [effect["system_path"] for effect in spec["effects"]]
        if args.phase == "materials":
            require(not any(api.exists(path) for path in material_paths + system_paths), "Destination assets exist; no overwrite/resume")
            report["actual_states"] = materials(api, spec, spec_hash)
        elif args.phase == "niagara":
            require(all(api.exists(path) for path in material_paths) and not any(api.exists(path) for path in system_paths), "Saved verified materials and fresh system destinations required")
            require(all((ROOT / "Content" / path.removeprefix("/Game/")).with_suffix(".uasset").is_file() for path in material_paths), "Materials must be saved before Niagara phase")
            report["actual_states"] = niagara(api, spec, spec_hash)
        else:
            require(all(api.exists(path) for path in material_paths + system_paths), "All eight candidate assets must exist")
            require(all((ROOT / "Content" / path.removeprefix("/Game/")).with_suffix(".uasset").is_file() for path in material_paths + system_paths), "Candidate package has not been saved")
            report["actual_states"] = [{"material": inspect_material(api, effect, True), "niagara": inspect_niagara(api, effect)} for effect in spec["effects"]]
        report["status"] = "saved_candidate_readback_passed_unconnected" if args.phase != "verify" else "saved_candidate_live_readback_passed_unconnected"
    except Exception as exception:
        failure = exception
        report["status"] = "failed_or_partial_inspect_evidence_no_blind_retry"
        report["failure"] = traceback.format_exc()
    try:
        after = hashes(files)
        report["original_hashes_after"] = after
        report["originals_preserved"] = after == before
        if after != before:
            failure = failure or RuntimeError("Original source packages changed")
            report["status"] = "failed_original_preservation_check"
    except Exception:
        report["originals_preserved"] = False
        report["source_hash_recheck_error"] = traceback.format_exc()
        failure = failure or RuntimeError("Original hashes could not be rechecked")
    report["mutation_calls"] = api.mutations if api else []
    with (out / "summary.json").open("x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2)
    if failure:
        raise failure
    print("RESULT=MAGIC_PARTICLE_" + args.phase.upper() + "_READBACK_PASSED candidate_unconnected=true render_validated=false evidence=" + str(out), flush=True)


if __name__ == "__main__":
    main()
