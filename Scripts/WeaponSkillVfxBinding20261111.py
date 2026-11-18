"""Detached contract for one of the eight unbound weapon VFX pilots.

No Unreal imports, filesystem access, network, setters or activation. A caller
must obtain fresh full reflection, current file pins, reviewed project skills and
an exclusive editor lease separately. The only permitted change is the existing
Cast/positive-damage Impact reference pair plus its shared enable bit. Every
timing, movement, hit interruption, Charge and first-person field stays fixed.
"""
from __future__ import annotations

import argparse
import copy
import json
import math
import re

import WeaponSkillVfxPreflight20261111 as preflight

SCHEMA = "Rogue10m.WeaponSkillVfxBinding.v1"
RECEIPT_SCHEMA = "Rogue10m.WeaponSkillVfxBinding.HashScope.v1"
STAGE_BASE = "/Game/DataAsset/AttackSkill/ClassCombat"
CHARACTER_ASSET = "/Game/DataAsset/Character/DA_Character_Default"
PROFILE_ASSETS = tuple("/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_" + w
                       for w in preflight.WEAPONS)
ALLOWED_FIELDS = frozenset(("enable_attack_effects", "cast_effect", "impact_effect"))
REQUIRED = preflight.REQUIRED | frozenset(("attack_effect",))


def _canonical(value):
    """Type-sensitive detached comparison, including future protected fields."""
    if value is None:
        return ("null",)
    if type(value) is bool:
        return ("bool", value)
    if type(value) is int:
        return ("int", value)
    if type(value) is float:
        if not math.isfinite(value):
            raise ValueError("Nonfinite snapshot/plan value")
        return ("float", value)
    if type(value) is str:
        return ("string", value)
    if type(value) is list:
        return ("list", tuple(_canonical(v) for v in value))
    if type(value) is dict:
        if any(type(k) is not str for k in value):
            raise ValueError("String snapshot/plan keys required")
        return ("dict", tuple(sorted((k, _canonical(v)) for k, v in value.items())))
    raise ValueError("Detached JSON values required")


def _asset(value):
    if type(value) is dict:
        if set(value) != {"refPath"}:
            raise ValueError("Asset mapping must contain only refPath")
        value = value["refPath"]
    if value is None:
        return None
    if type(value) is not str:
        raise ValueError("Plain nullable asset reference required")
    value = value.strip()
    if value.lower() in ("", "none", "null"):
        return None
    return preflight._ref(value)


def _target(weapon, variant):
    source = preflight.skill_path(weapon, variant)
    if (weapon, variant) in preflight.ACTIVE:
        raise ValueError("Existing active pilot is protected")
    return source


def _updates(weapon, variant):
    _target(weapon, variant)
    return {"enable_attack_effects": True,
            "cast_effect": preflight.candidate_path(weapon, variant, "Cast"),
            "impact_effect": preflight.candidate_path(weapon, variant, "Impact")}


def build_plan(weapon, variant, label):
    source = _target(weapon, variant)
    if type(label) is not str or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,23}", label):
        raise ValueError("Fresh simple label of 1..24 characters required")
    return {
        "schema": SCHEMA, "weapon": weapon, "variant": variant, "label": label,
        "source_asset": source,
        "staged_asset": (f"{STAGE_BASE}/{weapon}/VfxBindingCandidates20261111/{label}/"
                         f"DA_Attack_{weapon}_{variant}_Vfx_Candidate"),
        "updates": _updates(weapon, variant),
        "protected_assets": [preflight.skill_path(w, v) for w in preflight.WEAPONS
                             for v in preflight.VARIANTS if (w, v) != (weapon, variant)],
        "profile_assets": list(PROFILE_ASSETS), "character_asset": CHARACTER_ASSET,
        "candidate_assets": {"cast": preflight.candidate_path(weapon, variant, "Cast"),
                             "impact": preflight.candidate_path(weapon, variant, "Impact")},
        "deferred_trajectory_asset": preflight.candidate_path(weapon, variant, "Trajectory"),
        "event_contract": {
            "cast": "Existing cost-accepted cast cue; no new windup/release event or contact claim",
            "impact": "Existing positive-damage feedback; no new damage authority or hit pulse",
            "trajectory_bound": False, "third_person_preserved": True,
            "damage_interruption_preserved": True, "immunity_added": False,
            "charge_and_first_person_preserved": True,
            "legacy_presentation_scope": "Exact existing legacy skill presentation only; does not activate or populate new selectable catalog",
            "selectable_catalog_modified": False,
            "shared_enable": "False-to-true requires explicit three-field scope, null Charge and null legacy AttackEffect",
        },
        "validation_pending": ["fresh full live reflection and file pins", "reviewed project skills",
                               "exclusive editor lease", "saved stage and cold readback",
                               "actual third-person render and hit interruption QA"],
    }


def validate_plan(plan):
    if type(plan) is not dict or plan.get("schema") != SCHEMA:
        raise ValueError("Unexpected weapon VFX binding plan schema")
    expected = build_plan(plan.get("weapon"), plan.get("variant"), plan.get("label"))
    if _canonical(plan) != _canonical(expected):
        raise ValueError("Plan changed outside the single exact three-field pilot")
    return True


def snake_snapshot(native_properties):
    """Return detached snake properties and snake->native names; reject aliases."""
    if type(native_properties) is not dict or not native_properties:
        raise ValueError("Full native reflected property mapping required")
    _canonical(native_properties)
    result, reverse = {}, {}
    for name, value in native_properties.items():
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise ValueError("Invalid native property name")
        stem = name
        if len(stem) > 1 and stem[0] == "b" and stem[1].isupper():
            if type(value) is not bool:
                raise ValueError("Native b-prefixed property must be boolean")
            stem = stem[1:]
        snake = re.sub(r"(?<!^)(?=[A-Z])", "_", stem).lower()
        if snake in result:
            raise ValueError("Native property aliases normalize to the same field: " + snake)
        result[snake] = copy.deepcopy(value)
        reverse[snake] = name
    return result, reverse


def _source(properties):
    if type(properties) is not dict or not REQUIRED.issubset(properties):
        raise ValueError("Complete source snapshot including legacy AttackEffect required")
    _canonical(properties)
    if any(not re.fullmatch(r"[a-z_][a-z0-9_]*", k) for k in properties):
        raise ValueError("Snake-case source properties required")
    for field in ("enable_attack_effects", "use_first_person_effect_overrides"):
        if type(properties[field]) is not bool:
            raise ValueError(field + " must be boolean")
    for field in ("attack_effect_time_dilation", "cast_effect_emission_duration",
                  "impact_effect_emission_duration", "cast_effect_scale", "impact_effect_scale",
                  "first_person_emission_duration_multiplier", "first_person_cast_scale_multiplier",
                  "first_person_impact_scale_multiplier"):
        preflight._positive(properties[field], field)
    for field in ("cast_effect", "impact_effect", "charge_effect", "attack_effect", "attack_montage"):
        _asset(properties[field])
    if _asset(properties["attack_montage"]) is None:
        raise ValueError("Existing attack montage required")


def _hash_scope(candidate_hashes, source_hashes):
    expected_candidates = {preflight.candidate_path(w, v, p) for w in preflight.WEAPONS
                           for v in preflight.VARIANTS for p in preflight.PHASES}
    expected_sources = {preflight._source(w, p) for w in preflight.WEAPONS for p in preflight.PHASES}
    for values, wanted, label in ((candidate_hashes, expected_candidates, "candidate"),
                                  (source_hashes, expected_sources, "source")):
        if type(values) is not dict or set(values) != wanted:
            raise ValueError("Exact thirty-candidate/four-source hash scope required: " + label)
        for row in values.values():
            if type(row) is not dict or set(row) != {"expected_sha256", "actual_sha256", "status"}:
                raise ValueError("Complete current hash evidence required")
            if (row["status"] != "match" or preflight._sha(row["expected_sha256"])
                    != preflight._sha(row["actual_sha256"])):
                raise ValueError("Current candidate/source hash mismatch")


def checked_preflight_row(report, skill):
    """Bundle a detached row with all current 30+4 hash claims, not live proof."""
    _canonical(report)
    if (type(report) is not dict or report.get("schema") != preflight.SCHEMA
            or report.get("status") != "read_only_offline_preflight_no_activation"
            or report.get("candidate_hash_matches") != 30 or report.get("protected_active_skills") != 2
            or report.get("unbound_candidate_skill_count") != 8
            or type(report.get("source_declared_field_count")) is not int
            or report["source_declared_field_count"] < len(REQUIRED)
            or any(report.get(k) is not False for k in ("unreal_or_mcp_executed", "asset_or_gameplay_modified",
                                                       "new_render_performed", "visual_quality_approved"))):
        raise ValueError("Exact read-only preflight report required")
    _hash_scope(report.get("candidate_disk_hashes"), report.get("source_system_disk_hashes"))
    rows = report.get("skills")
    all_skills = {preflight.skill_path(w, v) for w in preflight.WEAPONS for v in preflight.VARIANTS}
    if (type(rows) is not list or len(rows) != 10 or any(type(r) is not dict for r in rows)
            or {r.get("skill") for r in rows} != all_skills):
        raise ValueError("Exactly ten unique preflight skill rows required")
    selected = [r for r in rows if r["skill"] == skill]
    if len(selected) != 1 or selected[0].get("role") != "unbound_candidate_inspection":
        raise ValueError("Only one unbound non-active pilot can be selected")
    result = copy.deepcopy(selected[0])
    result["binding_hash_scope"] = {"schema": RECEIPT_SCHEMA,
        "source_declared_field_count": report["source_declared_field_count"],
        "candidate_disk_hashes": copy.deepcopy(report["candidate_disk_hashes"]),
        "source_system_disk_hashes": copy.deepcopy(report["source_system_disk_hashes"])}
    return result


def _same_number(actual, expected, name):
    actual = preflight._positive(actual, name)
    if not math.isclose(actual, expected, rel_tol=1.e-6, abs_tol=1.e-7):
        raise ValueError("Preflight timing is stale or inconsistent: " + name)


def _timing(properties, row):
    timing = row.get("timing")
    if type(timing) is not dict or set(timing) != {"cast", "impact"}:
        raise ValueError("Both candidate timing budgets required")
    dilation = max(.1, properties["attack_effect_time_dilation"])
    for phase in ("cast", "impact"):
        values = timing[phase]
        if type(values) is not dict or values.get("basis") != "provided_snapshot_only_game_world_seconds":
            raise ValueError("Detached candidate timing basis required")
        life = preflight._positive(values.get("candidate_particle_sim_seconds"), phase + " particle lifetime")
        timer_raw = properties[phase + "_effect_emission_duration"]
        fp_raw = (timer_raw * properties["first_person_emission_duration_multiplier"]
                  if properties["use_first_person_effect_overrides"] else timer_raw)
        expected = {"component_dilation": dilation, "particle_world_seconds_estimate": life / dilation,
                    "destroy_timer_raw_world_seconds": timer_raw,
                    "first_person_destroy_timer_raw_world_seconds": fp_raw,
                    "destroy_timer_minimum_world_seconds": .01,
                    "destroy_timer_world_seconds": max(.01, timer_raw),
                    "first_person_destroy_timer_world_seconds": max(.01, fp_raw)}
        for field, wanted in expected.items():
            _same_number(values.get(field), wanted, phase + ":" + field)
        if (values.get("third_person_particle_truncated_by_destroy_timer") is not False
                or values.get("first_person_particle_truncated_by_destroy_timer") is not False
                or max(.01, timer_raw) + 1.e-6 < life / dilation
                or max(.01, fp_raw) + 1.e-6 < life / dilation):
            raise ValueError("Candidate particle is truncated by existing timer")
        if values.get("system_tail_lifetime_certified") is not False:
            raise ValueError("Offline timing must not certify system tails")


def planned_updates(plan, before_snake, preflight_row):
    validate_plan(plan)
    _source(before_snake)
    _canonical(preflight_row)
    row = preflight_row
    if type(row) is not dict or row.get("skill") != plan["source_asset"]:
        raise ValueError("Preflight belongs to another skill")
    receipt = row.get("binding_hash_scope")
    if type(receipt) is not dict or receipt.get("schema") != RECEIPT_SCHEMA:
        raise ValueError("Use checked_preflight_row with the complete hash report")
    if (set(receipt) != {"schema", "source_declared_field_count", "candidate_disk_hashes", "source_system_disk_hashes"}
            or type(receipt["source_declared_field_count"]) is not int
            or len(before_snake) != receipt["source_declared_field_count"]):
        raise ValueError("Current complete source field count differs from the preflight receipt")
    _hash_scope(receipt.get("candidate_disk_hashes"), receipt.get("source_system_disk_hashes"))
    if (row.get("role") != "unbound_candidate_inspection" or row.get("activation_ready") is not False
            or row.get("proposed_property_updates") != {}
            or row.get("inspection_binding_intent") != {k: v for k, v in plan["updates"].items()
                                                        if k != "enable_attack_effects"}):
        raise ValueError("Preflight row scope or activation status differs")
    trajectory = row.get("trajectory", {})
    if (type(trajectory) is not dict or trajectory.get("asset") != plan["deferred_trajectory_asset"]
            or trajectory.get("binding_allowed") is not False):
        raise ValueError("Trajectory must remain unbound")
    schema = row.get("snapshot_schema", {})
    if (type(schema) is not dict or schema.get("missing_source_declared_fields") != []
            or schema.get("extra_fields") != [] or schema.get("live_reflection_verified") is not False
            or row.get("snapshot_asset_hash_matches_current_disk") is not True):
        raise ValueError("Fresh complete source schema and disk hash required")
    blockers = row.get("blockers")
    if (type(blockers) is not list or any(type(b) is not str for b in blockers)
            or len(blockers) != len(set(blockers))
            or set(blockers) - {"effects_enable_requires_explicit_review"}):
        raise ValueError("Unresolved preflight blocker")
    if before_snake["enable_attack_effects"] is False:
        if _asset(before_snake["charge_effect"]) is not None:
            raise ValueError("Shared enable would activate existing Charge feedback")
        if _asset(before_snake["attack_effect"]) is not None:
            raise ValueError("Shared enable with legacy AttackEffect requires separate review")
        if blockers != ["effects_enable_requires_explicit_review"]:
            raise ValueError("Disabled effects require the explicit enable-review blocker")
    elif blockers:
        raise ValueError("Preflight enable state is stale")
    _timing(before_snake, row)
    return copy.deepcopy(plan["updates"])


def _validated_updates(updates):
    if type(updates) is not dict:
        raise ValueError("Editor updates must be a mapping")
    if not updates:
        return {}
    if set(updates) != ALLOWED_FIELDS or updates["enable_attack_effects"] is not True:
        raise ValueError("Only the exact three binding fields are allowed")
    for weapon in preflight.WEAPONS:
        for variant in preflight.VARIANTS:
            if (weapon, variant) not in preflight.ACTIVE:
                wanted = _updates(weapon, variant)
                if (_asset(updates["cast_effect"]) == wanted["cast_effect"]
                        and _asset(updates["impact_effect"]) == wanted["impact_effect"]):
                    return wanted
    raise ValueError("Candidate reference is protected, cross-skill or outside the binding scope")


def audit_transition(before_snake, after_snake, updates):
    """Audit every reflected field; empty updates audits a protected skill."""
    _source(before_snake)
    if type(after_snake) is not dict or set(before_snake) != set(after_snake):
        raise ValueError("Full reflected property keys changed")
    _canonical(after_snake)
    expected = _validated_updates(updates)
    if expected and before_snake["enable_attack_effects"] is False:
        if _asset(before_snake["charge_effect"]) is not None or _asset(before_snake["attack_effect"]) is not None:
            raise ValueError("Shared enable changes unreviewed Charge or legacy feedback")
    changed = []
    for field, before in before_snake.items():
        after = after_snake[field]
        if field in expected:
            if field == "enable_attack_effects":
                valid = after is True
            else:
                valid = _asset(after) == expected[field]
            if not valid:
                raise ValueError("Planned binding not applied: " + field)
        elif _canonical(before) != _canonical(after):
            raise ValueError("Protected reflected field changed: " + field)
        if _canonical(before) != _canonical(after):
            changed.append(field)
    return {"status": "single_skill_binding_snapshot_verified" if expected else "protected_snapshot_unchanged",
            "changed_fields": sorted(changed), "planned_fields": sorted(expected),
            "protected_fields_checked": len(before_snake) - len(expected), "idempotent_noop": not changed,
            "proof_scope": "provided full snapshots only; no live/disk/gameplay/render approval"}


def main(argv=None):
    """Print a detached plan; this entry point has no live or file-write phase."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--weapon", choices=preflight.WEAPONS, required=True)
    parser.add_argument("--variant", choices=preflight.VARIANTS, required=True)
    parser.add_argument("--label", required=True)
    args = parser.parse_args(argv)
    plan = build_plan(args.weapon, args.variant, args.label)
    validate_plan(plan)
    print(json.dumps({"status": "offline_binding_plan_only", "plan": plan,
                      "ue_or_mcp_executed": False, "asset_modified": False,
                      "gameplay_validated": False, "render_validated": False}, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
