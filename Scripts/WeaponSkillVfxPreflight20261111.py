"""Read-only preflight for the thirty saved GreatSword/Knuckle VFX candidates.

No Unreal imports, MCP, build, asset writes or activation. Only the eight skills
outside the two existing active pilots receive a Cast/Impact inspection intent.
Trajectory is always deferred: a single afterglow card is not a ribbon/contact
event. Even a complete offline pass requires fresh live schema and render review.

Usage (the project and all input evidence are read-only):
  python -B Scripts/WeaponSkillVfxPreflight20261111.py --project-root D:/Project/Rogue10m
    --manifest <SkillParticlesA/manifest.json> --manifest-sha256 <raw-file-sha256>
    --capture-evidence <SkillParticlesReviewG/captures.json> --capture-sha256 <sha256>
    [--snapshots <snapshot.json> --snapshots-sha256 <sha256>]
    --output <fresh-output-outside-project-root.json>

Snapshot schema: {"schema":"Rogue10m.WeaponVfxSourceSnapshots.v1", "skills":{
  "<exact skill package>":{"properties":{<all snake_case reflected fields>},
  "reflected_fields":[<same keys>], "asset_sha256":<export-time asset hash or null>}}}.
Provided snapshots and source-declaration coverage are not live reflection proof.
Missing provenance/schema/fields are blocked, never synthesized from defaults.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re

SCHEMA = "Rogue10m.WeaponSkillVfxPreflight.v1"
SNAPSHOT_SCHEMA = "Rogue10m.WeaponVfxSourceSnapshots.v1"
BASE = "/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110"
CANDIDATE_BASE = BASE + "/SkillCandidates/SkillParticlesA"
WEAPONS = ("GreatSword", "Knuckle")
VARIANTS = ("Primary01", "Primary02", "Primary03", "Special", "Charged")
PHASES = ("Cast", "Trajectory", "Impact")
ACTIVE = frozenset((("GreatSword", "Charged"), ("Knuckle", "Primary03")))
REQUIRED = frozenset((
    "enable_attack_effects", "cast_effect", "impact_effect", "charge_effect",
    "cast_effect_scale", "impact_effect_scale", "charge_effect_scale",
    "cast_effect_emission_duration", "impact_effect_emission_duration",
    "attack_effect_time_dilation", "use_first_person_effect_overrides",
    "first_person_emission_duration_multiplier", "first_person_cast_scale_multiplier",
    "first_person_impact_scale_multiplier", "first_person_charge_scale_multiplier",
    "first_person_effect_offset", "first_person_effect_rotation", "effect_attach_socket",
    "damage", "hit_count", "hit_start_delay_seconds", "hit_interval", "attack_montage",
    "animation_play_rate", "resource_costs", "input_slot", "attack_shape", "hit_mode",
))


def _json(value):
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError("Nonfinite JSON number")
    if isinstance(value, dict):
        if any(not isinstance(k, str) for k in value):
            raise ValueError("String JSON keys required")
        for item in value.values():
            _json(item)
    elif isinstance(value, list):
        for item in value:
            _json(item)
    elif value is not None and not isinstance(value, (str, bool, int, float)):
        raise ValueError("Detached JSON data required")


def _positive(value, name):
    try:
        valid = (not isinstance(value, bool) and isinstance(value, (int, float))
                 and math.isfinite(value) and value > 0)
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(name + " must be a finite positive real")
    return float(value)


def _sha(value):
    if not isinstance(value, str) or not re.fullmatch(r"[a-fA-F0-9]{64}", value):
        raise ValueError("SHA256 must contain exactly 64 hex characters")
    return value.lower()


def skill_path(weapon, variant):
    if weapon not in WEAPONS or variant not in VARIANTS:
        raise ValueError("Skill is outside the exact ten-skill scope")
    return f"/Game/DataAsset/AttackSkill/ClassCombat/{weapon}/DA_Attack_{weapon}_{variant}"


def candidate_path(weapon, variant, phase):
    skill_path(weapon, variant)
    if phase not in PHASES:
        raise ValueError("Unexpected VFX phase")
    return f"{CANDIDATE_BASE}/NS_{weapon}_{variant}_{phase}_Candidate"


def package_file(project_root, asset):
    if not isinstance(asset, str) or not re.fullmatch(r"/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+", asset):
        raise ValueError("Plain absolute Unreal package path required")
    root = Path(project_root).resolve()
    content = (root / "Content").resolve()
    if not content.is_relative_to(root):
        raise ValueError("Content escapes the specified physical project")
    target = (content / (asset[6:] + ".uasset")).resolve()
    if not target.is_relative_to(content):
        raise ValueError("Resolved package escapes Content")
    return target


def _source(weapon, phase):
    identity = "GreatSwordHeavy" if weapon == "GreatSword" else "KnuckleFlurry"
    suffix = "ConfirmedImpact" if phase == "Impact" else "Acceptance"
    return BASE + "/NS_" + identity + "_" + suffix


def validate_manifest(manifest):
    _json(manifest)
    if (not isinstance(manifest, dict)
            or manifest.get("status") != "30_candidates_saved_compile_checked_rendered_preview_pending"
            or manifest.get("activated") is not False or manifest.get("gameplay_modified") is not False
            or manifest.get("continuous_ribbon_implemented") is not False):
        raise ValueError("Successful unactivated thirty-candidate authoring manifest required")
    rows = manifest.get("candidates")
    if not isinstance(rows, list) or len(rows) != 30:
        raise ValueError("Exactly thirty candidate records required")
    source_pins = manifest.get("source_hashes_before")
    expected_sources = {_source(w, p) for w in WEAPONS for p in PHASES}
    if (not isinstance(source_pins, dict) or set(source_pins) != expected_sources
            or manifest.get("source_hashes_after") != source_pins
            or manifest.get("sources_preserved") is not True):
        raise ValueError("Exact four preserved source-system hash pins required")
    for value in source_pins.values():
        _sha(value)
    expected = {(w, v, p) for w in WEAPONS for v in VARIANTS for p in PHASES}
    seen = set()
    result = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("Candidate record must be a mapping")
        key = (row.get("weapon"), row.get("skill"), row.get("phase"))
        if key not in expected or key in seen:
            raise ValueError("Duplicate or out-of-scope candidate identity")
        seen.add(key)
        w, v, p = key
        asset = candidate_path(w, v, p)
        if (row.get("asset") != asset or row.get("name") != asset.rsplit("/", 1)[1]
                or row.get("existing_skill_asset") != skill_path(w, v)
                or row.get("source") != _source(w, p) or row.get("saved") is not True
                or row.get("source_sha256") != source_pins[_source(w, p)]
                or row.get("activated") is not False or row.get("presentation_only") is not True
                or type(row.get("local_space")) is not bool):
            raise ValueError("Candidate identity/source/space or saved-state drift")
        _positive(row.get("lifetime_sim_seconds"), "candidate lifetime")
        compile_state, issues = row.get("compile", {}), row.get("issues", {})
        if (compile_state.get("aggregateStatus") not in ("UpToDate", "UpToDateWithWarnings")
                or any(compile_state.get(k) is not False for k in ("bIsCompiling", "bIsStale", "bHasErrors"))
                or type(issues.get("numErrors")) is not int or issues["numErrors"] != 0):
            raise ValueError("Historical compile/stack gate failed")
        result[key] = row
    created = manifest.get("created_paths")
    if (not isinstance(created, list) or len(created) != 30 or len(set(created)) != 30
            or set(created) != {r["asset"] for r in rows}):
        raise ValueError("Created package scope differs from the thirty records")
    return result


def candidate_pins(manifest, captures, project_root, manifest_file_sha256):
    """Extract only the thirty candidate pins; do not approve the other 26 files."""
    rows = validate_manifest(manifest)
    _json(captures)
    if not isinstance(captures, dict):
        raise ValueError("Capture evidence must be a mapping")
    manifest_file_sha256 = _sha(manifest_file_sha256)
    before, after = captures.get("protected_sha256_before"), captures.get("protected_sha256_after")
    wanted = {r["asset"] for r in rows.values()}
    selected = captures.get("selected_assets")
    if (captures.get("manifest_sha256") != manifest_file_sha256 or not isinstance(before, dict)
            or before != after or captures.get("protected_assets_unchanged") is not True
            or captures.get("manifest_candidate_count") != 30
            or not isinstance(selected, list) or not all(isinstance(x, str) for x in selected)
            or len(selected) != 30 or set(selected) != wanted):
        raise ValueError("Pinned full thirty-candidate capture lineage required")
    root = Path(project_root).resolve()
    normalized = {}
    for raw_path, digest in before.items():
        path = Path(raw_path)
        if not path.is_absolute() or ".." in path.parts or not path.resolve().is_relative_to(root):
            raise ValueError("Historical evidence path escapes the read-only project")
        path = path.resolve()
        if path in normalized:
            raise ValueError("Ambiguous duplicate evidence file")
        normalized[path] = _sha(digest)
    if any(package_file(root, asset) not in normalized for asset in wanted):
        raise ValueError("Historical candidate package SHA pin is missing")
    return {asset: normalized[package_file(root, asset)] for asset in sorted(wanted)}


def schema_fields(header):
    """Source-declared UPROPERTY coverage, not live UHT/reflection validation."""
    if not isinstance(header, str) or "class ROGUE10M_API URogue10mAttackSkillData" not in header:
        raise ValueError("AttackSkillData source declaration required")
    header = header.split("class ROGUE10M_API URogue10mAttackSkillData", 1)[1]
    fields, waiting = set(), False
    for line in header.splitlines():
        if "UPROPERTY(" in line:
            waiting = True
        elif waiting and ";" in line:
            match = re.search(r"\b([A-Za-z_]\w*)\s*(?:=[^;]*)?;\s*$", line)
            if not match:
                raise ValueError("Unsupported reflected source declaration")
            native = match.group(1)
            if len(native) > 1 and native[0] == "b" and native[1].isupper():
                native = native[1:]
            fields.add(re.sub(r"(?<!^)(?=[A-Z])", "_", native).lower())
            waiting = False
    if not REQUIRED.issubset(fields):
        raise ValueError("Source declaration lacks critical protected properties")
    return fields


def _ref(value):
    # Native saved-property exports can retain a refPath envelope. Keep the
    # representation in the snapshot; normalize only this detached comparison.
    if type(value) is dict:
        if set(value) != {"refPath"}:
            raise ValueError("Asset mapping must contain only refPath")
        value = value["refPath"]
    if value is None:
        return None
    if type(value) is not str:
        raise ValueError("Expected a plain nullable asset reference")
    value = value.strip()
    if value.lower() in ("", "none", "null"):
        return None
    if not re.fullmatch(r"/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)?", value):
        raise ValueError("Expected a plain nullable asset reference")
    package, _, leaf = value.partition(".")
    if leaf and leaf != package.rsplit("/", 1)[1]:
        raise ValueError("Asset object/package leaf mismatch")
    return package


def audit(manifest, pins, project_root, source_fields, snapshots=None):
    rows = validate_manifest(manifest)
    expected_assets = {r["asset"] for r in rows.values()}
    if not isinstance(pins, dict) or set(pins) != expected_assets:
        raise ValueError("Exact thirty candidate SHA pins required")
    pins = {p: _sha(h) for p, h in pins.items()}
    if not isinstance(source_fields, set) or not REQUIRED.issubset(source_fields):
        raise ValueError("Complete source-declaration field set required")
    snapshots = {"schema": SNAPSHOT_SCHEMA, "skills": {}} if snapshots is None else snapshots
    _json(snapshots)
    if (not isinstance(snapshots, dict) or snapshots.get("schema") != SNAPSHOT_SCHEMA
            or not isinstance(snapshots.get("skills"), dict)
            or set(snapshots["skills"]) - {skill_path(w, v) for w in WEAPONS for v in VARIANTS}):
        raise ValueError("Unexpected/out-of-scope source snapshot schema")
    disk = {}
    for asset, wanted in pins.items():
        path = package_file(project_root, asset)
        actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        disk[asset] = {"expected_sha256": wanted, "actual_sha256": actual,
                       "status": "match" if actual == wanted else "missing" if actual is None else "mismatch"}
    source_disk = {}
    for asset, wanted in manifest["source_hashes_before"].items():
        path = package_file(project_root, asset)
        actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        source_disk[asset] = {"expected_sha256": wanted, "actual_sha256": actual,
                              "status": "match" if actual == wanted else "missing" if actual is None else "mismatch"}
    skills = []
    for w in WEAPONS:
        for v in VARIANTS:
            path = skill_path(w, v)
            protected = (w, v) in ACTIVE
            row = {"skill": path, "role": "protected_existing_active_pilot" if protected else "unbound_candidate_inspection",
                   "activation_ready": False, "proposed_property_updates": {},
                   "trajectory": {"asset": candidate_path(w, v, "Trajectory"), "binding_allowed": False,
                                  "reason": "Single afterglow card; no continuous ribbon or authoritative contact event"},
                   "blockers": [], "timing": {}}
            if not protected:
                row["inspection_binding_intent"] = {"cast_effect": candidate_path(w, v, "Cast"),
                                                    "impact_effect": candidate_path(w, v, "Impact")}
            snap = snapshots["skills"].get(path)
            props = snap.get("properties") if isinstance(snap, dict) else None
            if props is None:
                row["blockers"].append("missing_source_snapshot")
            elif not isinstance(props, dict):
                raise ValueError("Snapshot properties must be a mapping")
            else:
                declared = snap.get("reflected_fields")
                if (not isinstance(declared, list) or not all(isinstance(x, str) for x in declared)
                        or len(declared) != len(set(declared)) or set(declared) != set(props)):
                    raise ValueError("Snapshot reflected-field metadata must match all property keys")
                missing = sorted(source_fields - set(props))
                extra = sorted(set(props) - source_fields)
                row["snapshot_schema"] = {"missing_source_declared_fields": missing, "extra_fields": extra,
                                          "live_reflection_verified": False}
                if missing or extra:
                    row["blockers"].append("source_snapshot_schema_incomplete_or_changed")
                supplied_hash = snap.get("asset_sha256")
                current_path = package_file(project_root, path)
                current_hash = hashlib.sha256(current_path.read_bytes()).hexdigest() if current_path.is_file() else None
                hash_match = supplied_hash is not None and _sha(supplied_hash) == current_hash
                row["snapshot_asset_hash_matches_current_disk"] = hash_match
                if not hash_match:
                    row["blockers"].append("snapshot_asset_hash_missing_or_stale")
                for boolean in ("enable_attack_effects", "use_first_person_effect_overrides"):
                    if boolean in props and type(props[boolean]) is not bool:
                        raise ValueError(boolean + " must be an actual boolean")
                if props.get("enable_attack_effects") is not True:
                    row["blockers"].append("effects_enable_requires_explicit_review")
                    if "charge_effect" not in props or _ref(props["charge_effect"]) is not None:
                        row["blockers"].append("shared_enable_may_affect_charge_feedback")
                if not REQUIRED.issubset(props):
                    row["blockers"].append("missing_critical_timing_or_gameplay_fields")
                else:
                    dilation = max(.1, _positive(props["attack_effect_time_dilation"], "effect dilation"))
                    fp_multiplier = _positive(props["first_person_emission_duration_multiplier"], "FP emission multiplier")
                    for phase, field in (("Cast", "cast"), ("Impact", "impact")):
                        life = rows[(w, v, phase)]["lifetime_sim_seconds"]
                        timer_raw = _positive(props[field + "_effect_emission_duration"], field + " emission")
                        _positive(props[field + "_effect_scale"], field + " scale")
                        _positive(props["first_person_" + field + "_scale_multiplier"], field + " FP scale")
                        duration = life / dilation
                        timer = max(.01, timer_raw)
                        fp_timer_raw = timer_raw * fp_multiplier if props["use_first_person_effect_overrides"] else timer_raw
                        fp_timer = max(.01, fp_timer_raw)
                        third_truncated = timer + 1.e-6 < duration
                        fp_truncated = fp_timer + 1.e-6 < duration
                        row["timing"][field] = {"basis": "provided_snapshot_only_game_world_seconds",
                            "candidate_local_space": rows[(w, v, phase)]["local_space"],
                            "candidate_particle_sim_seconds": life, "component_dilation": dilation,
                            "particle_world_seconds_estimate": duration,
                            "destroy_timer_raw_world_seconds": timer_raw,
                            "first_person_destroy_timer_raw_world_seconds": fp_timer_raw,
                            "destroy_timer_minimum_world_seconds": .01,
                            "destroy_timer_world_seconds": timer,
                            "first_person_destroy_timer_world_seconds": fp_timer,
                            "third_person_particle_truncated_by_destroy_timer": third_truncated,
                            "first_person_particle_truncated_by_destroy_timer": fp_truncated,
                            "system_tail_lifetime_certified": False}
                        if third_truncated:
                            row["blockers"].append(field + "_third_person_timer_truncates_candidate")
                        if fp_truncated:
                            row["blockers"].append(field + "_first_person_timer_truncates_candidate")
            for phase in ("Cast", "Impact"):
                asset = candidate_path(w, v, phase)
                if disk[asset]["status"] != "match":
                    row["blockers"].append(phase.lower() + "_candidate_hash_" + disk[asset]["status"])
                source = _source(w, phase)
                if source_disk[source]["status"] != "match":
                    row["blockers"].append(phase.lower() + "_source_hash_" + source_disk[source]["status"])
            row["eligible_for_scoped_live_inspection"] = not protected and not row["blockers"]
            row["status"] = "protected_no_updates" if protected else "offline_conditions_pass_live_schema_render_pending" if not row["blockers"] else "blocked_or_missing_evidence"
            skills.append(row)
    return {"schema": SCHEMA, "status": "read_only_offline_preflight_no_activation",
            "candidate_disk_hashes": disk, "candidate_hash_matches": sum(r["status"] == "match" for r in disk.values()),
            "source_system_disk_hashes": source_disk,
            "skills": skills, "source_declared_field_count": len(source_fields), "protected_active_skills": 2,
            "unbound_candidate_skill_count": 8, "unreal_or_mcp_executed": False,
            "asset_or_gameplay_modified": False, "new_render_performed": False, "visual_quality_approved": False,
            "proof_limits": ["Only thirty candidate hashes are compared; the other historical protected files are not approved by this audit.",
                             "Source-declaration and supplied-snapshot coverage are not current live reflection proof.",
                             "Particle lifetime arithmetic excludes system tails, renderer visibility, hand motion and camera occlusion.",
                             "All timing/gameplay/Charge/first-person fields remain unchanged; this tool produces no setter updates."]}


def read_pinned(path, digest):
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != _sha(digest):
        raise ValueError("Pinned JSON file SHA mismatch")
    return json.loads(data.decode("utf-8-sig"), parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON constant")))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--project-root", required=True, type=Path)
    p.add_argument("--manifest", required=True, type=Path)
    p.add_argument("--manifest-sha256", required=True)
    p.add_argument("--capture-evidence", required=True, type=Path)
    p.add_argument("--capture-sha256", required=True)
    p.add_argument("--snapshots", type=Path)
    p.add_argument("--snapshots-sha256")
    p.add_argument("--output", required=True, type=Path)
    a = p.parse_args()
    root, output = a.project_root.resolve(), a.output.resolve()
    if output.exists() or output.is_relative_to(root):
        raise ValueError("Fresh output outside the read-only project root required")
    manifest = read_pinned(a.manifest, a.manifest_sha256)
    captures = read_pinned(a.capture_evidence, a.capture_sha256)
    if bool(a.snapshots) != bool(a.snapshots_sha256):
        raise ValueError("Snapshot path and SHA256 must be supplied together")
    snapshots = read_pinned(a.snapshots, a.snapshots_sha256) if a.snapshots else None
    header = root / "Source/Rogue10m/Data/Rogue10mAttackSkillData.h"
    header_bytes = header.read_bytes()
    report = audit(manifest, candidate_pins(manifest, captures, root, a.manifest_sha256), root,
                   schema_fields(header_bytes.decode("utf-8-sig")), snapshots)
    report["inputs"] = {"manifest_sha256": a.manifest_sha256.lower(), "capture_sha256": a.capture_sha256.lower(),
                        "snapshots_sha256": a.snapshots_sha256.lower() if a.snapshots_sha256 else None,
                        "source_header_sha256": hashlib.sha256(header_bytes).hexdigest()}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
    print("READ_ONLY_PREFLIGHT candidate_hash_matches=" + str(report["candidate_hash_matches"]) + "/30 activation_ready=0 new_render=0")


if __name__ == "__main__":
    main()
