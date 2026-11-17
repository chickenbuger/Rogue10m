"""Detached Staff Primary01 third-person presentation trial; no Unreal imports.

Only four opt-in presentation properties may change. The existing finite Niagara
systems, attack montage/rate, damage timing, all shared VFX tuning and every
first-person property stay unchanged. Trial offsets require actual socket-basis
and default-camera review; this module makes no visual-quality claim.
"""
from __future__ import annotations

import copy
import json
import math
import re

from MagicSkillVfxBinding20261111 import (
    CANDIDATES, PILOT_ASSET, build_binding_plan, normalize_object_ref,
)

SCHEMA = "Rogue10m.StaffVfxReadability.v1"
BASE = "/Game/DataAsset/AttackSkill/ClassCombat/Staff/ReadabilityCandidates20261111"
UPDATES = {
    "thirdPersonCastEffectOffset": {"x": 25.0, "y": 0.0, "z": 20.0},
    "thirdPersonCastEffectRotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
    "bUseThirdPersonImpactBoundsSurface": True,
    "thirdPersonImpactEffectSurfaceOffset": 4.0,
}
BASELINE_PLACEMENT = {
    "thirdPersonCastEffectOffset": {"x": 0.0, "y": 0.0, "z": 0.0},
    "thirdPersonCastEffectRotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
    "bUseThirdPersonImpactBoundsSurface": False,
    "thirdPersonImpactEffectSurfaceOffset": 0.0,
}
REQUIRED = frozenset(UPDATES) | frozenset((
    "bEnableAttackEffects", "castEffect", "impactEffect", "attackMontage",
    "animationPlayRate", "hitStartDelaySeconds", "resourceCosts", "chargeEffect",
    "castEffectScale", "impactEffectScale", "castEffectEmissionDuration",
    "impactEffectEmissionDuration", "attackEffectTimeDilation",
    "firstPersonEmissionDurationMultiplier", "firstPersonCastScaleMultiplier",
    "firstPersonImpactScaleMultiplier", "firstPersonChargeScaleMultiplier",
    "firstPersonEffectOffset", "firstPersonEffectRotation", "effectAttachSocket",
))


def build_plan(label):
    if not isinstance(label, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,23}", label):
        raise ValueError("Fresh 1..24-character simple label required")
    return {
        "schema": SCHEMA,
        "status": "prepared_offline_unvalidated_trial",
        "source_asset": PILOT_ASSET,
        "staged_asset": BASE + "/" + label + "/DA_Attack_Staff_Primary01_Readability_Candidate",
        "updates": copy.deepcopy(UPDATES),
        "protected_assets": [r["asset"] for r in build_binding_plan()["targets"]
                             if r["asset"] != PILOT_ASSET],
        "reuse": {"cast": CANDIDATES["telegraph"], "impact": CANDIDATES["impact"],
                  "animation": "Preserve the exact currently bound AttackMontage and AnimationPlayRate"},
        "timing_contract": {
            "hit_start_delay": "preserve exact value; observed baseline is 0 seconds",
            "cast_event": "existing cost-accepted cast event; no new windup or release event",
            "impact_event": "existing positive-damage event; no added or delayed damage",
            "scale_duration_dilation": "preserve all shared and first-person values",
            "particle_lifetimes": "existing finite .30s cast/.22s impact; destroy timers alone cannot extend particles",
        },
        "trial_limits": [
            "Cast offset (25,0,20)cm is socket-local; hand_r basis and weapon-tip alignment are unverified.",
            "Impact uses the caster-facing XY bounds surface plus 4cm; it is approximate feedback, not physics contact.",
            "No material depth-test bypass, new light, particle count, montage, notify or original Niagara edit.",
            "Stage is an unbound DataAsset duplicate; no profile or character binding is changed.",
            "Activation only copies these four properties; old eight-field binding report lineage is not reused.",
        ],
        "validation_pending": ["C++ reflected build", "live schema and project skills", "saved candidate stage/readback",
                               "cold load", "default third-person captures at multiple ages", "camera occlusion comparison",
                               "motion/contact/recovery evidence", "physical input/full-run gameplay"],
    }


def validate_plan(plan):
    if not isinstance(plan, dict) or plan.get("schema") != SCHEMA:
        raise ValueError("Unexpected readability plan schema")
    path = plan.get("staged_asset", "")
    label = path.removeprefix(BASE + "/").split("/", 1)[0]
    if plan != build_plan(label):
        raise ValueError("Readability plan changed outside the four-property trial")
    return True


def _finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _equivalent(actual, expected):
    if isinstance(expected, dict):
        return (isinstance(actual, dict) and set(actual) == set(expected)
                and all(_equivalent(actual[k], v) for k, v in expected.items()))
    if _finite(expected):
        return _finite(actual) and math.isclose(actual, expected, abs_tol=1.e-5, rel_tol=1.e-6)
    return type(actual) is type(expected) and actual == expected


def validate_source(properties):
    if not isinstance(properties, dict) or not REQUIRED.issubset(properties):
        raise ValueError("Complete reflected snapshot/new presentation schema required")
    json.dumps(properties, allow_nan=False)
    if properties["bEnableAttackEffects"] is not True:
        raise ValueError("Existing Staff visual pilot must already be enabled")
    for field, kind in (("castEffect", "telegraph"), ("impactEffect", "impact")):
        if normalize_object_ref(properties[field]) != normalize_object_ref(CANDIDATES[kind]):
            raise ValueError("Existing reviewed MagicMcp01 binding required: " + field)
    # These are baseline prerequisites, never values dispatched by this recipe.
    baseline = {"castEffectScale": 1.0, "impactEffectScale": 1.0,
                "castEffectEmissionDuration": .35, "impactEffectEmissionDuration": .27,
                "attackEffectTimeDilation": 1.0}
    for field, wanted in baseline.items():
        if not _equivalent(properties[field], wanted):
            raise ValueError("Shared visual tuning drift; inspect before trial: " + field)
    if not normalize_object_ref(properties["attackMontage"]):
        raise ValueError("Existing Staff attack montage required")
    return True


def audit_transition(before, after, planned=True):
    """Check every reflected property, including unknown future gameplay fields."""
    validate_source(before)
    if not isinstance(after, dict) or set(before) != set(after):
        raise ValueError("Full reflected property keys changed")
    json.dumps(after, allow_nan=False)
    changed = []
    for field in before:
        if planned and field in UPDATES:
            if not _equivalent(after[field], UPDATES[field]):
                raise ValueError("Trial field not applied: " + field)
        elif type(before[field]) is not type(after[field]) or before[field] != after[field]:
            raise ValueError("Protected reflected field changed: " + field)
        if before[field] != after[field]:
            changed.append(field)
    return {"status": "four_property_trial_verified" if planned else "protected_snapshot_unchanged",
            "changed_fields": sorted(changed), "protected_fields_checked": len(before) - (4 if planned else 0),
            "proof_scope": "provided property snapshots only; no rendered/gameplay verification"}


def validate_runtime_profile(properties, candidate):
    """Keep baseline/candidate captures distinguishable after reconnect/restart."""
    if type(candidate) is not bool:
        raise ValueError("Runtime profile selector must be boolean")
    validate_source(properties)
    expected = UPDATES if candidate else BASELINE_PLACEMENT
    for field, wanted in expected.items():
        if not _equivalent(properties[field], wanted):
            raise ValueError("Wrong runtime presentation profile: " + field)
    return "readability_candidate" if candidate else "baseline_zero_placement"


def audit_baseline_capture_reports(applied, readback):
    """Detached report audit only; does not establish live or disk state."""
    plan = build_binding_plan()
    expected = {row["asset"] for row in plan["targets"]} | {
        plan["profile_asset"], plan["character_asset"]}
    after = applied.get("properties_after")
    current = readback.get("live", {}).get("properties")
    if not isinstance(after, dict) or not isinstance(current, dict):
        raise ValueError("Baseline full-property reports required")
    if set(after) != expected or set(current) != expected or after != current:
        raise ValueError("Baseline complete property scope/readback differs")
    if applied.get("nonvisual_fields_preserved") is not True:
        raise ValueError("Baseline nonvisual preservation audit required")
    return validate_runtime_profile(current[PILOT_ASSET], False)
