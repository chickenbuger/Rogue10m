"""Pure manifest/audit contract for one Staff Primary01 visual pilot.

No engine imports, filesystem access, asset writes, gameplay execution, or live
connection claim. Other Staff skills and EVERY charge field stay protected.
build_binding_plan(root=None) takes an optional PROJECT filesystem root for
metadata; candidate_root is the fixed, previously prepared MagicMcp01 asset root.
"""
from __future__ import annotations

import copy
import math
import os
from pathlib import PurePosixPath, PureWindowsPath
import re
from collections.abc import Mapping


CANDIDATE_ROOT = "/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01"
SKILL_ROOT = "/Game/DataAsset/AttackSkill/ClassCombat/Staff"
SUFFIXES = ("Primary01", "Primary02", "Primary03", "Special", "Charged")
PILOT_ASSET = SKILL_ROOT + "/DA_Attack_Staff_Primary01"
PROFILE_ASSET = "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Staff"
CHARACTER_ASSET = "/Game/DataAsset/Character/DA_Character_Default"
SCHEMA = "Rogue10m.MagicSkillVfxBinding.v1"

CANDIDATES = {
    "telegraph": CANDIDATE_ROOT + "/NS_MagicTelegraph_Preparation_Candidate",
    "circle": CANDIDATE_ROOT + "/NS_MagicCircle_FloatingCasterSigil_Candidate",
    "release": CANDIDATE_ROOT + "/NS_MagicRelease_DirectionalFork_Candidate",
    "impact": CANDIDATE_ROOT + "/NS_MagicImpact_DiamondStar_Candidate",
}
FLOAT_UPDATES = frozenset(("cast_effect_scale", "impact_effect_scale", "attack_effect_time_dilation",
    "cast_effect_emission_duration", "impact_effect_emission_duration"))
REFERENCE_FIELDS = frozenset(("cast_effect", "impact_effect", "charge_effect", "attack_effect", "attack_montage", "skill_icon"))
POSITIVE_SNAPSHOT_FIELDS = FLOAT_UPDATES | frozenset(("first_person_emission_duration_multiplier", "charge_effect_scale", "first_person_cast_scale_multiplier",
    "first_person_impact_scale_multiplier", "first_person_charge_scale_multiplier"))
REQUIRED_SNAPSHOT_FIELDS = POSITIVE_SNAPSHOT_FIELDS | frozenset(("enable_attack_effects", "cast_effect", "impact_effect",
    "charge_effect", "attack_montage", "animation_play_rate", "hit_start_delay_seconds", "resource_costs", "effect_attach_socket"))


def normalize_object_ref(value):
    """Normalize saved asset package/object paths and nulls; reject subobjects/aliases.

This is an asset-reference normalizer, not a parser for UObject export-text or
generated-class literals. Optional {refPath: ...} MCP references are supported.
"""
    if isinstance(value, Mapping):
        if set(value) != {"refPath"}:
            raise ValueError("Reference mapping must contain only refPath")
        value = value["refPath"]
    if value is None:
        return None
    if not isinstance(value, str):
        raise ValueError("Asset reference must be a string, refPath mapping, or null")
    value = value.strip()
    if value.lower() in ("", "none", "null"):
        return None
    if not re.fullmatch(r"/[A-Za-z_][A-Za-z0-9_]*(?:/[A-Za-z_][A-Za-z0-9_]*)+(?:\.[A-Za-z_][A-Za-z0-9_]*)?", value):
        raise ValueError("Expected a plain absolute Unreal asset package/object path")
    package, separator, name = value.partition(".")
    leaf = package.rsplit("/", 1)[-1]
    if separator and name != leaf:
        raise ValueError("Object name must match its asset package; aliases/subobjects are out of scope")
    return package + "." + leaf


def _project_root(value):
    if value is None:
        return None
    try:
        text = os.fspath(value)
    except TypeError as exception:
        raise ValueError("root means an absolute project filesystem path") from exception
    if not isinstance(text, str) or not text or any(character in text for character in ("\x00", "\n", "\r")):
        raise ValueError("Invalid project filesystem root")
    if text.startswith("/Game/"):
        raise ValueError("root means project filesystem root, not candidate asset root")
    path = PureWindowsPath(text) if "\\" in text or re.match(r"^[A-Za-z]:", text) else PurePosixPath(text)
    if not path.is_absolute() or ".." in path.parts:
        raise ValueError("Project root must be absolute without traversal")
    return str(path)


def _finite(value, label, positive=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(label + " must be a finite real number")
    if positive and value <= 0:
        raise ValueError(label + " must be positive")
    return float(value)


def _pilot_updates():
    return {
        "enable_attack_effects": True,
        "cast_effect": normalize_object_ref(CANDIDATES["telegraph"]),
        "impact_effect": normalize_object_ref(CANDIDATES["impact"]),
        "cast_effect_scale": 1.0,
        "impact_effect_scale": 1.0,
        "attack_effect_time_dilation": 1.0,
        "cast_effect_emission_duration": .35,
        "impact_effect_emission_duration": .27,
    }


def _validated_updates(updates, allow_empty=False):
    if not isinstance(updates, Mapping) or any(not isinstance(key, str) for key in updates):
        raise ValueError("Expected an editor-property mapping")
    if not updates and allow_empty:
        return {}
    wanted = _pilot_updates()
    if set(updates) != set(wanted):
        raise ValueError("Only the exact eight-field Primary01 pilot update is approved")
    result = {}
    for key, expected in wanted.items():
        value = updates[key]
        if key in FLOAT_UPDATES:
            number = _finite(value, key, True)
            if not math.isclose(number, expected, rel_tol=1.e-6, abs_tol=1.e-6):
                raise ValueError("Unapproved visual trial value: " + key)
            result[key] = float(expected)
        elif key == "enable_attack_effects":
            if value is not True:
                raise ValueError("enable_attack_effects must be the boolean True")
            result[key] = True
        else:
            reference = normalize_object_ref(value)
            if reference != expected:
                raise ValueError("Reference is outside the approved telegraph/impact pilot: " + key)
            result[key] = reference
    return result


def _raw_plan(project_root):
    return {
        "schema": SCHEMA,
        "status": "candidate_unconnected",
        "project_root": project_root,
        "candidate_root": CANDIDATE_ROOT,
        "pilot_asset": PILOT_ASSET,
        "profile_asset": PROFILE_ASSET,
        "character_asset": CHARACTER_ASSET,
        "candidate_assets": list(CANDIDATES.values()),
        "targets": [{"asset": SKILL_ROOT + "/DA_Attack_Staff_" + suffix,
                     "role": "pilot" if suffix == "Primary01" else "protected",
                     "updates": _pilot_updates() if suffix == "Primary01" else {}}
                    for suffix in SUFFIXES],
        "deferred": [{"asset": CANDIDATES[kind], "status": "unbound_requires_real_release_event"}
                     for kind in ("circle", "release")],
        "event_contracts": {
            "cast": "Existing cost-accepted CastEffect cue; neither a release/launch notify nor proof of successful montage playback.",
            "impact": "Existing positive-damage feedback at approximate target bounds; not a precise surface contact or new damage authority.",
            "charge": "All charge references and tuning stay unchanged. No charge binding, hold meter, repeated spawn or extended lifetime is added.",
            "release_event_added": False,
            "gameplay_or_input_changes": False,
        },
        "lifetime_contract": {
            "time_basis": "game-world seconds; component custom dilation 1, not guaranteed wall-clock seconds",
            "cast": {"particle_seconds": .30, "system_seconds": .35, "destroy_timer_seconds": .35},
            "impact": {"particle_seconds": .22, "system_seconds": .27, "destroy_timer_seconds": .27},
            "first_person_destroy_formula": "effect_emission_duration * first_person_emission_duration_multiplier",
            "first_person_duration_multiplier": "preserve existing value; first-person development and lifetime validation paused",
            "first_person_lifetime_warning": "Preserved multipliers below 1 may truncate candidate particles; this third-person pilot does not certify first-person lifetime.",
            "first_person_scale_multipliers": "preserve existing values; visual screen coverage remains unvalidated",
            "charge_candidate": "Telegraph is finite .30s (.35s system); it is NOT connected to ChargeEffect and cannot certify sustained hold feedback.",
        },
        "worldspace_warning": "Candidates use world-space particles (bLocalSpace=false). Attached components do not make emitted particles follow animated hands; inspect actual cast/target motion before accepting the visual result.",
        "preconditions": [
            "Freshly validate the four saved candidate assets and existing Staff profile/character/skill references in the editor.",
            "Take complete reflected snapshots of all five targets before any update and audit all five afterward.",
            "Pilot charge reference must remain null if enabling effects or changing the shared effect time dilation; otherwise stop for scoped review.",
            "Preserve sockets, first-person emission duration and scale multipliers, montage/play rate/hit timing, costs, progression and every unplanned field.",
            "Apply no changes to the four protected skills, profile, character asset, candidate systems or materials under this manifest.",
        ],
        "validation_status": "pure_contract_only; no engine execution or live binding/render validation",
    }


def build_binding_plan(root=None):
    """Build a detached plan. root is optional project-filesystem metadata only."""
    plan = _raw_plan(_project_root(root))
    validate_plan(plan)
    return plan


def validate_plan(plan):
    if not isinstance(plan, Mapping) or plan.get("schema") != SCHEMA:
        raise ValueError("Unexpected binding manifest schema")
    expected = _raw_plan(_project_root(plan.get("project_root")))
    if set(plan) != set(expected):
        raise ValueError("Unexpected or missing manifest fields")
    if not isinstance(plan.get("targets"), list) or len(plan["targets"]) != 5:
        raise ValueError("Exactly five Staff targets, one pilot and four protected, are required")
    for actual, wanted in zip(plan["targets"], expected["targets"]):
        if not isinstance(actual, Mapping) or set(actual) != {"asset", "role", "updates"}:
            raise ValueError("Invalid target manifest row")
        if normalize_object_ref(actual["asset"]) != normalize_object_ref(wanted["asset"]) or actual["role"] != wanted["role"]:
            raise ValueError("Target scope or pilot/protected identity differs")
        if wanted["role"] == "pilot":
            _validated_updates(actual["updates"])
        elif not isinstance(actual["updates"], Mapping) or actual["updates"]:
            raise ValueError("Protected targets must have no updates")
    for key in expected.keys() - {"targets"}:
        if _canonical_value(plan[key]) != _canonical_value(expected[key]):
            raise ValueError("Manifest contract differs: " + key)
    return True


def planned_visual_updates(skill):
    """Accept the Primary01 asset path or its exact plan row; reject other skills."""
    if isinstance(skill, Mapping):
        if set(skill) != {"asset", "role", "updates"} or skill.get("role") != "pilot":
            raise ValueError("Only the pilot manifest row can produce updates")
        asset = skill["asset"]
        _validated_updates(skill["updates"])
    else:
        asset = skill
    if normalize_object_ref(asset) != normalize_object_ref(PILOT_ASSET):
        raise ValueError("Only Staff Primary01 is approved for visual updates")
    return copy.deepcopy(_pilot_updates())


def _canonical_value(value):
    """Type-sensitive deep comparison; reject opaque objects and nonfinite data."""
    if value is None:
        return ("null",)
    if isinstance(value, bool):
        return ("bool", value)
    if isinstance(value, int):
        return ("int", value)
    if isinstance(value, float):
        _finite(value, "snapshot/manifest float")
        return ("float", value)
    if isinstance(value, str):
        return ("string", value)
    if isinstance(value, Mapping):
        if any(not isinstance(key, str) for key in value):
            raise ValueError("Snapshot mappings require string keys")
        return ("mapping", tuple(sorted((key, _canonical_value(item)) for key, item in value.items())))
    if isinstance(value, (tuple, list)):
        return ("sequence", tuple(_canonical_value(item) for item in value))
    raise ValueError("Snapshots must contain detached serializable data")


def _field_equal(field, before, after):
    if field in REFERENCE_FIELDS:
        return normalize_object_ref(before) == normalize_object_ref(after)
    return _canonical_value(before) == _canonical_value(after)


def audit_transition(before, after, expected_visual_updates):
    """Audit complete flat reflected snapshots; return evidence or raise ValueError.

An empty expected map audits a protected skill. The eight-field map audits the
pilot. This function does not apply updates or establish live connection status.
"""
    expected = _validated_updates(expected_visual_updates, allow_empty=True)
    if not isinstance(before, Mapping) or not isinstance(after, Mapping):
        raise ValueError("Before/after must be reflected-property mappings")
    if set(before) != set(after) or not REQUIRED_SNAPSHOT_FIELDS.issubset(before):
        raise ValueError("Complete snapshots must retain all property keys and critical protected fields")
    _canonical_value(before)
    _canonical_value(after)
    for snapshot in (before, after):
        if type(snapshot["enable_attack_effects"]) is not bool:
            raise ValueError("enable_attack_effects snapshot must be boolean")
        for field in POSITIVE_SNAPSHOT_FIELDS:
            _finite(snapshot[field], field, True)
        for field in REFERENCE_FIELDS & snapshot.keys():
            normalize_object_ref(snapshot[field])
    # Shared enable/dilation changes could affect a previously configured charge
    # visual even if its pointer is preserved. Reject that unexpected pilot state.
    shared_change = before["enable_attack_effects"] != after["enable_attack_effects"] or not _field_equal(
        "attack_effect_time_dilation", before["attack_effect_time_dilation"], after["attack_effect_time_dilation"])
    if expected and shared_change and normalize_object_ref(before["charge_effect"]) is not None:
        raise ValueError("Pilot has a charge visual affected by shared enable/dilation; scoped review required")
    changed = []
    for field in before:
        equal = _field_equal(field, before[field], after[field])
        if field not in expected:
            if not equal:
                raise ValueError("Unplanned property changed: " + field)
        else:
            wanted = expected[field]
            if field in FLOAT_UPDATES:
                if not math.isclose(_finite(after[field], field, True), wanted, rel_tol=1.e-6, abs_tol=1.e-6):
                    raise ValueError("Planned visual value not applied: " + field)
            elif not _field_equal(field, after[field], wanted):
                raise ValueError("Planned visual value not applied: " + field)
        if not equal:
            changed.append(field)
    return {
        "status": "protected_unchanged_verified" if not expected else "pilot_visual_snapshot_verified",
        "changed_fields": sorted(changed),
        "planned_fields": sorted(expected),
        "protected_fields_checked": len(before) - len(expected),
        "idempotent_noop": not changed,
        "proof_scope": "provided snapshots only; live connection/render state not inferred",
    }
