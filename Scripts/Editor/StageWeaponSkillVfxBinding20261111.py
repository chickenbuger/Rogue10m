"""One-at-a-time remaining weapon Cast/Impact binding via official MCP.

Default prepare is offline. Live phases require the coordinator's exclusive
lease, the exact direct-editor project identity, fresh complete snapshots and
pinned candidate evidence. No editor/build launch, mutation retry or automatic
rollback is performed. This script does not implement hit immunity or timing.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import WeaponSkillVfxBinding20261111 as contract
import WeaponSkillVfxPreflight20261111 as preflight
import BindMagicSkillVfxMCP20261111 as native
import StageStaffVfxReadability20261111 as owner


def all_scope_assets(plan):
    return [plan["source_asset"], *plan["protected_assets"],
            *plan["profile_assets"], plan["character_asset"]]


def material_assets(manifest):
    """Use saved renderer readbacks; never guess unrelated material packages."""
    rows = preflight.validate_manifest(manifest)
    result = set()
    for row in rows.values():
        renderer = row.get("readback", {}).get("renderer", {})
        values = renderer.get("propertyValues")
        if isinstance(values, str):
            values = json.loads(values)
        if not isinstance(values, dict):
            raise RuntimeError("Saved renderer property readback required")
        reference = values.get("Material")
        if not isinstance(reference, dict) or set(reference) != {"refPath"}:
            raise RuntimeError("Explicit saved renderer material reference required")
        path = preflight._ref(reference["refPath"])
        if path is None:
            raise RuntimeError("Explicit saved project material dependency required")
        result.add(path)
    return sorted(result)


def material_dependency_graph(c, roots):
    """Read saved project MIC parent chains without editing any material."""
    result, visiting = {}, set()

    def visit(path):
        if path in visiting:
            raise RuntimeError("Material instance parent cycle")
        if path in result:
            return
        visiting.add(path)
        if c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
            raise RuntimeError("Reviewed material dependency has unsaved changes: " + path)
        klass = c.call(native.A, "get_asset_class", {"asset_path": path})
        if not isinstance(klass, str) or klass.rsplit(".", 1)[-1] not in ("Material", "MaterialInstanceConstant"):
            raise RuntimeError("Reviewed renderer dependency is not a saved material/interface: " + path)
        parent = None
        if klass.rsplit(".", 1)[-1] == "MaterialInstanceConstant":
            ref = {"refPath": native.object_path(path)}
            fields = c.call(native.O, "list_properties", {"instance": ref})
            keys = [k for k in fields if k.lower() == "parent"] if isinstance(fields, dict) else []
            if len(keys) != 1:
                raise RuntimeError("Unique reflected material parent field required")
            values = c.call(native.O, "get_properties", {"instance": ref, "properties": keys})
            if not isinstance(values, dict) or set(values) != set(keys):
                raise RuntimeError("Complete material parent readback required")
            parent = contract._asset(values[keys[0]])
            if parent is None:
                raise RuntimeError("Saved project material instance parent required")
            visit(parent)
        result[path] = {"class": klass, "parent": parent,
                        "saved_sha256": native.file_sha(native.package_file(path))}
        visiting.remove(path)

    for root in roots:
        visit(root)
    return {p: result[p] for p in sorted(result)}


def fingerprints(plan, manifest, material_graph=None):
    assets = set(all_scope_assets(plan)) | set(manifest["created_paths"])
    assets |= set(manifest["source_hashes_before"]) | set(material_assets(manifest))
    if material_graph is not None:
        assets.update(material_graph)
    paths = {native.package_file(p) for p in assets}
    paths.add(ROOT / "Rogue10m.uproject")
    # Another Codex can edit any of these concurrently. Detect it; never restore it.
    for folder, suffixes in (("Source", {".h", ".cpp", ".cs"}),
                             ("Config", {".ini"}), ("Scripts", {".py", ".ps1"})):
        paths.update(p for p in (ROOT / folder).rglob("*")
                     if p.is_file() and p.suffix.lower() in suffixes and "__pycache__" not in p.parts)
    result = {}
    for path in sorted(paths):
        resolved = path.resolve()
        if not resolved.is_relative_to(ROOT.resolve()):
            raise RuntimeError("Protected dependency escapes the physical project")
        result[path.relative_to(ROOT).as_posix()] = native.file_sha(path)
    return result


def expand_inspection_checkpoint(initial, expanded, graph):
    parent_keys = {native.package_file(p).relative_to(ROOT).as_posix() for p in graph}
    if not set(initial).issubset(expanded) or set(expanded) - set(initial) - parent_keys:
        raise RuntimeError("Code/asset inventory changed during material dependency inspection")
    if any(expanded[p] != old for p, old in initial.items()):
        raise RuntimeError("Concurrent file change during dependency inspection")
    for asset, node in graph.items():
        key = native.package_file(asset).relative_to(ROOT).as_posix()
        if expanded[key] != node["saved_sha256"]:
            raise RuntimeError("Material dependency changed during readback")
    return expanded


def assert_hash_transition(before, after, changed_asset=None, saved_sha=None):
    if set(before) != set(after):
        raise RuntimeError("Protected file inventory changed during operation")
    permitted = native.package_file(changed_asset).relative_to(ROOT).as_posix() if changed_asset else None
    for path, old in before.items():
        expected = saved_sha if path == permitted and saved_sha else old
        if after[path] != expected:
            raise RuntimeError("Concurrent/protected file change: " + path)


def assert_mutation_checkpoint(c, plan, manifest, report, material_graph):
    before = report.get("fingerprints_before")
    if not isinstance(before, dict) or not before:
        raise RuntimeError("Pinned pre-mutation file checkpoint required")
    current_graph = material_dependency_graph(c, material_assets(manifest))
    if current_graph != material_graph:
        raise RuntimeError("Material parent chain changed before mutation")
    assert_hash_transition(before, fingerprints(plan, manifest, current_graph))


def inspect(c, plan, manifest, pins):
    contract.validate_plan(plan)
    if c.call(native.E, "IsPIERunning", {}) is not False:
        raise RuntimeError("Stop PIE before inspecting binding candidates")
    listed = c.call(native.S, "ListSkills", {})
    if not isinstance(listed, dict):
        raise RuntimeError("Project skills schema drift")
    skills = c.call(native.S, "GetSkills", {"skillPaths": list(listed)}) if listed else {}
    states, classes, snake, names = {}, {}, {}, {}
    for path in all_scope_assets(plan):
        if c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
            raise RuntimeError("Unsaved source/protected asset: " + path)
        klass = c.call(native.A, "get_asset_class", {"asset_path": path})
        expected = "Rogue10mCharacterDataAsset" if path == plan["character_asset"] else (
            "Rogue10mWeaponSkillProfileDataAsset" if path in plan["profile_assets"] else "Rogue10mAttackSkillData")
        if not isinstance(klass, str) or klass.rsplit(".", 1)[-1] != expected:
            raise RuntimeError("Source/protected asset class mismatch: " + path)
        if path == plan["source_asset"] and c.call(native.A, "can_edit_asset", {"asset_path": path}) is not True:
            raise RuntimeError("Selected skill is locked")
        states[path] = native.all_properties(c, path)
        classes[path] = klass
        if expected == "Rogue10mAttackSkillData":
            snake[path], names[path] = contract.snake_snapshot(states[path])
    catalog_scope = {}
    if snake[plan["source_asset"]].get("skill_tree_node_type") != "ActiveTechnique":
        raise RuntimeError("Selected existing skill must be an active technique")
    for profile in plan["profile_assets"]:
        weapon = profile.rsplit("_", 1)[-1]
        if states[profile].get("weaponType") != weapon:
            raise RuntimeError("Weapon profile identity mismatch")
        if not native.contains_reference(states[plan["character_asset"]].get("weaponSkillProfiles"), profile):
            raise RuntimeError("Default character does not reference protected weapon profile")
        for variant in preflight.VARIANTS:
            if not native.contains_reference(states[profile].get("skillTreeSkills"), preflight.skill_path(weapon, variant)):
                raise RuntimeError("Skill is absent from its weapon tree")
        catalog = states[profile].get("activeTechniqueCatalog")
        if not isinstance(catalog, list):
            raise RuntimeError("Current profile catalog schema missing; compile/read fresh state")
        if catalog and len(catalog) != 10:
            raise RuntimeError("Nonempty explicit technique catalog must retain ten entries")
        catalog_scope[profile] = {
            "explicit_catalog_present": bool(catalog),
            "selected_skill_referenced": native.contains_reference(catalog, plan["source_asset"]),
            "catalog_modified": False,
            "new_catalog_implementation_or_activation_claimed": False,
        }
    material_dependencies = material_assets(manifest)
    material_graph = material_dependency_graph(c, material_dependencies)
    compiles = {}
    for path in plan["candidate_assets"].values():
        if c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
            raise RuntimeError("Candidate Niagara system is unsaved")
        if c.call(native.A, "get_asset_class", {"asset_path": path}) != "NiagaraSystem":
            raise RuntimeError("Candidate is not a Niagara system")
        ref = {"system": {"refPath": native.object_path(path)}}
        state = c.call(native.N, "GetSystemCompileState", ref)
        issues = c.call(native.N, "GetStackIssues", ref)
        if (any(state.get(k, True) for k in ("bIsCompiling", "bIsStale", "bHasErrors"))
                or state.get("aggregateStatus") not in ("UpToDate", "UpToDateWithWarnings")
                or issues.get("numErrors", 1) != 0):
            raise RuntimeError("Selected candidate compile/stack gate failed")
        compiles[path] = {"compile": state, "issues": issues}
    fields = preflight.schema_fields((ROOT / "Source/Rogue10m/Data/Rogue10mAttackSkillData.h").read_text(encoding="utf-8-sig"))
    snapshots = {"schema": preflight.SNAPSHOT_SCHEMA, "skills": {
        path: {"properties": props, "reflected_fields": sorted(props),
               "asset_sha256": native.file_sha(native.package_file(path))}
        for path, props in snake.items()}}
    audit = preflight.audit(manifest, pins, ROOT, fields, snapshots)
    row = contract.checked_preflight_row(audit, plan["source_asset"])
    updates = contract.planned_updates(plan, snake[plan["source_asset"]], row)
    bound_candidates = []
    for w in preflight.WEAPONS:
        for v in preflight.VARIANTS:
            path = preflight.skill_path(w, v)
            props = snake[path]
            if (props.get("enable_attack_effects") is True
                    and native.contains_reference(props.get("cast_effect"), preflight.candidate_path(w, v, "Cast"))
                    and native.contains_reference(props.get("impact_effect"), preflight.candidate_path(w, v, "Impact"))):
                bound_candidates.append(path)
    return {"properties": states, "classes": classes, "source_snake": snake[plan["source_asset"]],
            "source_name_map": names[plan["source_asset"]], "planned_updates": updates,
            "project_skills": skills, "project_skills_sha256": native.digest(skills),
            "candidate_compiles": compiles, "preflight": audit, "catalog_scope": catalog_scope,
            "clean_saved_material_dependencies": material_dependencies,
            "material_parent_graph": material_graph,
            "full_material_function_texture_dependency_graph_verified": False,
            "current_saved_candidate_binding_paths": bound_candidates,
            "candidate_binding_state_proof": "current reflected references only; not runtime/render approval"}


def native_updates(updates, names):
    if not set(updates).issubset(names):
        raise RuntimeError("Native property name mapping is incomplete")
    return {names[k]: ({"refPath": native.object_path(v)} if k in ("cast_effect", "impact_effect") else v)
            for k, v in updates.items()}


def mutate(c, path, before_native, updates, report, require_clean=True):
    if require_clean and c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
        raise RuntimeError("Source became dirty before setter")
    if native.all_properties(c, path) != before_native:
        raise RuntimeError("Source properties changed immediately before setter")
    before, names = contract.snake_snapshot(before_native)
    proposed = copy.deepcopy(before)
    proposed.update(copy.deepcopy(updates))
    contract.audit_transition(before, proposed, updates)
    pending = {k: v for k, v in updates.items() if not native.equivalent(before[k], v)}
    if pending:
        report["mutation_attempted"] = True
        result = c.call(native.O, "set_properties", {"instance": {"refPath": native.object_path(path)},
                        "values": json.dumps(native_updates(pending, names), allow_nan=False)})
        if result is not True:
            raise RuntimeError("Setter failed/uncertain; inspect before retry")
    after_native = native.all_properties(c, path)
    after, _ = contract.snake_snapshot(after_native)
    report["property_audit"] = contract.audit_transition(before, after, updates)
    return after_native


def save(c, path, report):
    report["save_attempted"] = True
    if c.call(native.A, "save_assets", {"asset_paths": [path]}) is not True:
        raise RuntimeError("Scoped save failed/uncertain; inspect before retry")
    if c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
        raise RuntimeError("Scoped asset remains dirty")
    report["asset_saved"] = True
    report["saved_asset_sha256"] = native.file_sha(native.package_file(path))


def audit_other_assets(c, baseline, changed_path=None):
    after = {p: native.all_properties(c, p) for p in baseline["properties"]}
    for path, before in baseline["properties"].items():
        if path != changed_path and after[path] != before:
            raise RuntimeError("Protected reflected asset changed: " + path)
    return after


def stage(c, plan, baseline, manifest, pins, report):
    if inspect(c, plan, manifest, pins) != baseline:
        raise RuntimeError("Stale live source/schema/skills; inspect again")
    candidate = plan["staged_asset"]
    if c.call(native.A, "exists", {"path": candidate}) is not False:
        raise RuntimeError("Fresh unbound candidate destination required")
    assert_mutation_checkpoint(c, plan, manifest, report, baseline["material_parent_graph"])
    report["mutation_attempted"] = True
    result = c.call(native.A, "duplicate", {"path": plan["source_asset"], "new_path": candidate})
    if result is False or result is None or c.call(native.A, "exists", {"path": candidate}) is not True:
        raise RuntimeError("Duplicate failed/uncertain; inspect before retry")
    copied = native.all_properties(c, candidate)
    if copied != baseline["properties"][plan["source_asset"]]:
        raise RuntimeError("Duplicate did not retain full source state")
    assert_mutation_checkpoint(c, plan, manifest, report, baseline["material_parent_graph"])
    after = mutate(c, candidate, copied, baseline["planned_updates"], report, require_clean=False)
    report["properties_after"] = audit_other_assets(c, baseline)
    assert_mutation_checkpoint(c, plan, manifest, report, baseline["material_parent_graph"])
    save(c, candidate, report)
    if native.all_properties(c, candidate) != after:
        raise RuntimeError("Staged candidate changed after scoped save")
    report.update(status="single_unbound_weapon_candidate_saved_review_pending", candidate_properties=after,
                  gameplay_binding_changed=False)


def activate(c, plan, baseline, staged, manifest, pins, report):
    if inspect(c, plan, manifest, pins) != baseline:
        raise RuntimeError("Stale live source/schema/skills; inspect again")
    if (staged.get("plan") != plan or staged.get("live") != baseline
            or staged.get("status") != "single_unbound_weapon_candidate_saved_review_pending"
            or staged.get("asset_saved") is not True):
        raise RuntimeError("Matching successful stage lineage required")
    candidate, source = plan["staged_asset"], plan["source_asset"]
    if (native.file_sha(native.package_file(candidate)) != staged.get("saved_asset_sha256")
            or c.call(native.A, "is_dirty", {"asset_path": candidate}) is not False
            or native.all_properties(c, candidate) != staged.get("candidate_properties")):
        raise RuntimeError("Staged asset changed after review")
    before, _ = contract.snake_snapshot(baseline["properties"][source])
    staged_props, _ = contract.snake_snapshot(staged["candidate_properties"])
    contract.audit_transition(before, staged_props, baseline["planned_updates"])
    report["staged_asset_sha256"] = staged["saved_asset_sha256"]
    assert_mutation_checkpoint(c, plan, manifest, report, baseline["material_parent_graph"])
    after = mutate(c, source, baseline["properties"][source], baseline["planned_updates"], report)
    report["properties_after"] = audit_other_assets(c, baseline, source)
    if after != baseline["properties"][source]:
        assert_mutation_checkpoint(c, plan, manifest, report, baseline["material_parent_graph"])
        save(c, source, report)
    if c.call(native.A, "is_dirty", {"asset_path": source}) is not False or native.all_properties(c, source) != after:
        raise RuntimeError("Activated asset is not a clean exact readback")
    report.update(status="single_weapon_cast_impact_binding_saved_runtime_qa_pending",
                  source_properties_after=after, saved_asset_sha256=native.file_sha(native.package_file(source)),
                  nonvisual_fields_preserved=True, saved_clean_readback_verified=True,
                  vfx_binding_updated=True, gameplay_profile_binding_changed=False)


def verify(c, plan, baseline, applied, manifest, pins, current_hashes, report):
    if (applied.get("phase") != "activate" or applied.get("plan") != plan
            or applied.get("status") != "single_weapon_cast_impact_binding_saved_runtime_qa_pending"
            or applied.get("protected_files_preserved") is not True
            or applied.get("fingerprints_after") != current_hashes):
        raise RuntimeError("Pinned activation/current dependencies required")
    live = inspect(c, plan, manifest, pins)
    if live["project_skills_sha256"] != baseline["project_skills_sha256"] or live["classes"] != baseline["classes"]:
        raise RuntimeError("Project skills/classes changed since inspection")
    if live["properties"] != applied.get("properties_after"):
        raise RuntimeError("Full reflected values differ from activation")
    source, candidate = plan["source_asset"], plan["staged_asset"]
    before, _ = contract.snake_snapshot(baseline["properties"][source])
    after, _ = contract.snake_snapshot(live["properties"][source])
    report["property_audit"] = contract.audit_transition(before, after, baseline["planned_updates"])
    for path in baseline["properties"]:
        if path != source and live["properties"][path] != baseline["properties"][path]:
            raise RuntimeError("Protected asset drift at readback")
    if (c.call(native.A, "is_dirty", {"asset_path": candidate}) is not False
            or native.file_sha(native.package_file(candidate)) != applied.get("staged_asset_sha256")
            or native.all_properties(c, candidate) != live["properties"][source]
            or native.file_sha(native.package_file(source)) != applied.get("saved_asset_sha256")):
        raise RuntimeError("Saved source/candidate artifact drift")
    report.update(status="single_weapon_binding_readback_verified_runtime_qa_pending", live=live,
                  source_properties_after=live["properties"][source], properties_after=live["properties"],
                  staged_asset_sha256=applied["staged_asset_sha256"], saved_asset_sha256=applied["saved_asset_sha256"],
                  nonvisual_fields_preserved=True, saved_clean_readback_verified=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--phase", choices=("prepare", "inspect", "stage", "activate", "verify"), default="prepare")
    p.add_argument("--weapon", choices=preflight.WEAPONS, required=True)
    p.add_argument("--variant", choices=preflight.VARIANTS, required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--evidence-label", required=True)
    p.add_argument("--url", default="http://127.0.0.1:8001/mcp")
    p.add_argument("--lease-granted", action="store_true")
    for key in ("manifest", "capture-evidence", "snapshot", "stage-report", "activation-report"):
        p.add_argument("--" + key, type=Path)
        p.add_argument("--" + key + "-sha256")
    p.add_argument("--require-cold-reload", action="store_true")
    a = p.parse_args()
    plan = contract.build_plan(a.weapon, a.variant, a.label)
    contract.build_plan(a.weapon, a.variant, a.evidence_label)
    if a.phase != "prepare" and not a.lease_granted:
        raise RuntimeError("Coordinator exclusive editor lease required; no calls made")
    out = ROOT / "Feature/doc/evidence/weapon-skill-vfx-binding-20261111" / a.evidence_label
    out.mkdir(parents=True, exist_ok=False)
    report = {"phase": a.phase, "plan": plan, "mutation_attempted": False, "asset_saved": False,
              "mcp_dispatched": False, "editor_or_build_launched": False, "runtime_gameplay_validated": False,
              "visual_quality_approved": False, "cold_reload_validated": False}
    client = None
    try:
        if a.phase == "prepare":
            report["status"] = "single_skill_binding_implementation_prepared_offline"
        else:
            if native.ROOT.resolve() != ROOT.resolve() or owner.ROOT.resolve() != ROOT.resolve():
                raise RuntimeError("MCP/helper imports belong to a different project checkout")
            if not a.manifest or not a.manifest_sha256 or not a.capture_evidence or not a.capture_evidence_sha256:
                raise RuntimeError("Pinned source manifest and candidate capture lineage required")
            manifest = preflight.read_pinned(a.manifest, a.manifest_sha256)
            captures = preflight.read_pinned(a.capture_evidence, a.capture_evidence_sha256)
            pins = preflight.candidate_pins(manifest, captures, ROOT, a.manifest_sha256)
            report["input_hashes"] = {"manifest": a.manifest_sha256.lower(), "captures": a.capture_evidence_sha256.lower()}
            # A CIM/process identity failure stops here; it never grants ownership.
            report["listener_identity"] = owner.listener_identity(a.url)
            client = native.NativeClient(a.url, out / "mcp")
            report["mcp_dispatched"] = True
            report["identity"] = native.project_identity(client)
            initial = fingerprints(plan, manifest)
            report["live"] = inspect(client, plan, manifest, pins)
            graph = report["live"]["material_parent_graph"]
            report["fingerprints_before"] = expand_inspection_checkpoint(initial, fingerprints(plan, manifest, graph), graph)
            if a.phase == "inspect":
                report["status"] = "single_skill_fresh_readonly_snapshot_review_skills_before_stage"
            else:
                if not a.snapshot or not a.snapshot_sha256:
                    raise RuntimeError("Pinned fresh inspect snapshot required")
                snapshot = preflight.read_pinned(a.snapshot, a.snapshot_sha256)
                if (snapshot.get("phase") != "inspect" or snapshot.get("plan") != plan
                        or snapshot.get("status") != "single_skill_fresh_readonly_snapshot_review_skills_before_stage"
                        or snapshot.get("input_hashes") != report["input_hashes"]
                        or (a.phase != "verify" and snapshot.get("fingerprints_before") != report["fingerprints_before"])):
                    raise RuntimeError("Inspect plan/input/files changed")
                report["inspect_snapshot_sha256"] = a.snapshot_sha256.lower()
                baseline = snapshot["live"]
                if a.phase == "stage":
                    stage(client, plan, baseline, manifest, pins, report)
                elif a.phase == "activate":
                    if not a.stage_report or not a.stage_report_sha256:
                        raise RuntimeError("Pinned stage report required")
                    staged = preflight.read_pinned(a.stage_report, a.stage_report_sha256)
                    if (staged.get("inspect_snapshot_sha256") != report["inspect_snapshot_sha256"]
                            or staged.get("fingerprints_after") != report["fingerprints_before"]):
                        raise RuntimeError("Stage lineage/files changed")
                    report["stage_report_sha256"] = a.stage_report_sha256.lower()
                    activate(client, plan, baseline, staged, manifest, pins, report)
                else:
                    if not a.activation_report or not a.activation_report_sha256:
                        raise RuntimeError("Pinned activation report required")
                    applied = preflight.read_pinned(a.activation_report, a.activation_report_sha256)
                    if applied.get("inspect_snapshot_sha256") != report["inspect_snapshot_sha256"]:
                        raise RuntimeError("Activation inspection lineage differs")
                    report["activation_report_sha256"] = a.activation_report_sha256.lower()
                    verify(client, plan, baseline, applied, manifest, pins, report["fingerprints_before"], report)
                    owner.cold_readback(applied, report)
                    if a.require_cold_reload and report["cold_reload_validated"] is not True:
                        raise RuntimeError("New direct editor PID/prior shutdown proof required")
            report["fingerprints_after"] = fingerprints(plan, manifest, report["live"]["material_parent_graph"])
            assert_hash_transition(report["fingerprints_before"], report["fingerprints_after"],
                                   plan["source_asset"] if a.phase == "activate" else None,
                                   report.get("saved_asset_sha256"))
            report["protected_files_preserved"] = True
    except Exception as error:
        report.update(status="stopped_no_automatic_retry", error=str(error),
                      last_action=client.last_action if client else None,
                      uncertain_mutation_outcome_requires_readonly_inspection=report["mutation_attempted"])
        raise
    finally:
        (out / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    print("RESULT=" + report["status"] + " evidence=" + str(out))


if __name__ == "__main__":
    main()
