"""Offline preparation of four finite magic VFX candidates; imports no Unreal APIs.

Values are visual trial assumptions, not skill/charge/damage/targeting rules.
Nothing here creates assets, acquires an editor lease, binds skills, or compiles HLSL.
The coordinator must freshly inspect the owned source and author fresh duplicates.

Usage:
    .venv/Scripts/python.exe Scripts/MagicParticleCandidateSpec20261110.py --self-test
    .venv/Scripts/python.exe Scripts/MagicParticleCandidateSpec20261110.py --print-spec --label Trial01
Import build_spec(label) for a detached JSON-compatible dictionary.
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import re


DESTINATION_BASE = "/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110"
SOURCE_SYSTEM = "/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/NS_KnuckleFlurry_Acceptance"
SOURCE_MATERIAL = "/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/M_KnuckleFlurry_Acceptance"
SPRITE_RENDERER = "/Script/Niagara.NiagaraSpriteRendererProperties"
IDS = ("telegraph", "circle", "release", "impact")

# Custom material inputs: UV = TextureCoordinate float2; Age = ParticleRelativeTime.
# No Core function, textures, world Time, depth reads, lights, decals or extra emitters.
# Each expression is a bounded mask inside ONE stationary camera-facing sprite.
HLSL_MASKS = {
    "telegraph": """float2 p=(UV-.5)*2;
float2 q=abs(p);
float t=saturate(Age);
float side=lerp(.64,.52,smoothstep(0,.8,t));
float hd=(q.y-side)*48;
float vd=(q.x-side)*48;
float h=exp(-hd*hd)*smoothstep(side-.22,side-.18,q.x)*(1-smoothstep(side,side+.04,q.x));
float v=exp(-vd*vd)*smoothstep(side-.22,side-.18,q.y)*(1-smoothstep(side,side+.04,q.y));
float fade=smoothstep(0,.15,t)*(1-smoothstep(.55,1,t));
return saturate(max(h,v)*fade*.6);""",
    "circle": """float2 p=(UV-.5)*2;
float r=length(p);
float a=atan2(p.y,p.x+.000001);
float t=saturate(Age);
float rd=(r-.68)*58;
float ring=exp(-rd*rd);
float ticks=pow(abs(cos(a*3)),64)*smoothstep(.47,.50,r)*(1-smoothstep(.60,.63,r));
float fade=smoothstep(0,.18,t)*(1-smoothstep(.62,1,t));
return saturate((ring*.6+ticks*.45)*fade);""",
    "release": """float2 p=(UV-.5)*2;
float t=saturate(Age);
float taper=max(.2,saturate((.65-p.x)/1.4));
float sd=p.y*18/taper;
float fd=(abs(p.y)-(.62-p.x)*.45)*48;
float shaft=exp(-sd*sd)*smoothstep(-.85,-.70,p.x)*(1-smoothstep(.25,.40,p.x));
float fork=exp(-fd*fd)*smoothstep(.08,.18,p.x)*(1-smoothstep(.60,.66,p.x));
float fade=smoothstep(0,.06,t)*pow(1-t,1.5);
return saturate((shaft*.65+fork*.55)*fade);""",
    "impact": """float2 p=(UV-.5)*2;
float r=length(p);
float a=atan2(p.y,p.x+.000001);
float t=saturate(Age);
float dd=(abs(p.x)+abs(p.y)-lerp(.15,.73,t))*40;
float diamond=exp(-dd*dd);
float rays=pow(abs(cos(a*2)),40)*smoothstep(.16,.20,r)*(1-smoothstep(.68,.74,r))*pow(1-t,2);
float core=exp(-r*r*120)*pow(saturate(1-t*4),2);
float fade=smoothstep(0,.045,t)*pow(1-t,1.3);
return saturate((diamond*.55+rays*.35+core*.5)*fade);""",
}

_TRIALS = (
    ("telegraph", "MagicTelegraph_Preparation", (40.0, 40.0), .30, (.40, .68, .82),
     "Four inward corner brackets with an empty center; short preparation hint, not a charge meter."),
    ("circle", "MagicCircle_FloatingCasterSigil", (64.0, 64.0), .48, (.43, .72, .88),
     "Thin circular rim with six short inner ticks and an empty center; floating caster sigil only, not ground AoE or damage boundary."),
    ("release", "MagicRelease_DirectionalFork", (64.0, 24.0), .16, (.56, .80, .93),
     "A narrow tapered shaft and forked forward stroke along sprite +U; stationary visual flash, not a simulated projectile."),
    ("impact", "MagicImpact_DiamondStar", (44.0, 44.0), .22, (.75, .88, .96),
     "Compact four-point star and expanding hollow diamond; visual reaction candidate without hit or damage authority."),
)


def _number(value, name, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(name + " must be a finite real number")
    if not minimum <= value <= maximum:
        raise ValueError(name + " is outside the trial safety envelope")


def _label(label):
    if not isinstance(label, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,23}", label):
        raise ValueError("Fresh label must start with a letter and contain 1..24 ASCII letters/digits/underscores")
    return label


def build_spec(label):
    """Return detached preparation data. Freshness must be checked by the authoring pipeline."""
    label = _label(label)
    root = DESTINATION_BASE + "/" + label
    effects = []
    for identifier, stem, size, lifetime, tint, description in _TRIALS:
        half_diagonal = math.hypot(*size) * .5
        effects.append({
            "id": identifier,
            "system_name": "NS_" + stem + "_Candidate",
            "material_name": "M_" + stem + "_Candidate",
            "system_path": root + "/NS_" + stem + "_Candidate",
            "material_path": root + "/M_" + stem + "_Candidate",
            "status": "visual_trial_unbuilt_unvalidated",
            "description": description,
            "requires_charge": False,
            "skill_binding": None,
            "emitter_count": 1,
            "emitter_name": "DirectionalBurst",
            "sim_target": "CPUSim",
            "sprite_renderer_count": 1,
            "renderer_class": SPRITE_RENDERER,
            "burst_count": 1,
            "burst_time_world_seconds": 0.0,
            "spawn_rate_per_second": 0.0,
            "system_loop_behavior": "Once",
            "emitter_loop_behavior": "Once",
            "particle_lifetime_world_seconds": lifetime,
            "emitter_duration_world_seconds": round(lifetime + .05, 3),
            "system_duration_world_seconds": round(lifetime + .05, 3),
            "kill_particles_at_lifetime_end": True,
            "custom_time_dilation": 1.0,
            "world_time_meaning": "game-world seconds; follows world time dilation, not guaranteed wall-clock seconds",
            "lifetime_variance_seconds": 0.0,
            "sprite_size_cm": list(size),
            "component_uniform_scale": 1.0,
            "fixed_bounds_half_extent_cm": [round(half_diagonal + 2.0, 3)] * 3,
            "local_space": False,
            "velocity_cm_per_second": [0.0, 0.0, 0.0],
            "facing_mode": "FaceCamera",
            "alignment": "Unaligned",
            "sort_mode": "None",
            "disable_inherited_modules": ["GravityForce", "Drag", "ScaleSpriteSizeBySpeed"],
            "renderer": {"cast_shadows": False, "collision": False, "lights": False, "ribbons": False,
                         "decals": False, "camera_offset_cm": 0.0},
            "material": {"blend_mode": "Additive", "shading_model": "Unlit", "two_sided": True,
                         "depth_test_enabled": True, "scene_depth_sampling": False,
                         "inputs": ["UV", "Age"], "age_source": "ParticleRelativeTime",
                         "tint_linear_rgb": list(tint), "emissive_gain": .90,
                         "mask_hlsl": HLSL_MASKS[identifier]},
            "offline_mask_coverage_limit_fraction": .16,
            "preview_min_camera_distance_cm": 150.0,
            "screen_coverage_verified": False,
        })
    spec = {
        "schema": "Rogue10m.MagicParticleCandidates.v1",
        "label": label,
        "destination_root": root,
        "values_are_visual_trial_assumptions": True,
        "status": "prepared_offline_only",
        "source": {"system": SOURCE_SYSTEM, "material": SOURCE_MATERIAL,
                   "emitter_name": "DirectionalBurst", "emitter_count": 1, "sim_target": "CPUSim",
                   "renderer_class": SPRITE_RENDERER,
                   "provenance": "Existing project-owned WeaponFantasy UV+Age one-sprite derivative; no new external art",
                   "fresh_summary_and_lifecycle_inspection_required": True,
                   "clone_only_to_fresh_destination": True, "modify_source": False},
        "integration": {"skill_bindings": [], "requires_charge": False, "damage_authority": None,
                        "targeting_authority": None, "animation_notify_bindings": [],
                        "ground_projection_authored": False, "camera_changes": False},
        "authoring_checks_pending": [
            "Coordinator-granted exclusive editor slot and fresh source hash/summary/topology/input inspection.",
            "Inspect system AND emitter Once lifecycle, remove inherited recurring spawn, set one burst, equal lifetime min/max and zero velocity.",
            "Use independent material duplicates with only UV/Age mask inputs; confirm no inherited camera offset, size growth or light/collision/ribbon/decal emitters.",
            "Inspect compile/stack issues and cold-read actual emitter/renderer counts, lifetime, world-time dilation=1, bounds and automatic completion.",
            "Rendered floating-caster staging review for shape distinction, dark/bright backgrounds, clipping, depth and screen coverage.",
            "No skill integration or ground/target/damage semantics are authorized by this candidate data.",
        ],
        "effects": effects,
    }
    validate_spec(spec)
    return spec


def validate_source_summary(summary):
    """Topology gate only. A source summary alone does NOT prove finite lifecycle."""
    if not isinstance(summary, dict):
        raise ValueError("Source summary must be a mapping")
    emitters = summary.get("emitters")
    if not isinstance(emitters, list) or len(emitters) != 1:
        raise ValueError("Expected exactly one source emitter")
    emitter = emitters[0]
    if not isinstance(emitter, dict):
        raise ValueError("Emitter summary must be a mapping")
    renderers = emitter.get("rendererClasses")
    if (emitter.get("emitterName") != "DirectionalBurst" or emitter.get("simTarget") != "CPUSim"
            or emitter.get("bEnabled") is not True or not isinstance(renderers, list)
            or len(renderers) != 1 or not isinstance(renderers[0], dict)
            or renderers[0].get("refPath") != SPRITE_RENDERER):
        raise ValueError("Owned finite-template topology changed; inspect before cloning")
    return True


def validate_spec(spec):
    """Reject loops, unbounded dimensions/counts, integration fields, unsafe time and mask drift."""
    if not isinstance(spec, dict) or spec.get("schema") != "Rogue10m.MagicParticleCandidates.v1":
        raise ValueError("Unexpected candidate schema")
    label = _label(spec.get("label"))
    root = DESTINATION_BASE + "/" + label
    if (spec.get("destination_root") != root or spec.get("status") != "prepared_offline_only"
            or spec.get("values_are_visual_trial_assumptions") is not True):
        raise ValueError("Candidate root/status/trial declaration changed")
    source = spec.get("source", {})
    if (not isinstance(source, dict) or source.get("system") != SOURCE_SYSTEM or source.get("material") != SOURCE_MATERIAL
            or source.get("modify_source") is not False or source.get("clone_only_to_fresh_destination") is not True
            or source.get("fresh_summary_and_lifecycle_inspection_required") is not True):
        raise ValueError("Source must be preserved and freshly inspected")
    expected_integration = {"skill_bindings": [], "requires_charge": False, "damage_authority": None,
                            "targeting_authority": None, "animation_notify_bindings": [],
                            "ground_projection_authored": False, "camera_changes": False}
    if spec.get("integration") != expected_integration:
        raise ValueError("Offline candidates cannot establish gameplay integration")
    effects = spec.get("effects")
    if not isinstance(effects, list) or tuple(effect.get("id") for effect in effects if isinstance(effect, dict)) != IDS or len(effects) != 4:
        raise ValueError("Require four distinct ordered telegraph/circle/release/impact candidates")
    paths = set()
    for effect in effects:
        if (effect.get("status") != "visual_trial_unbuilt_unvalidated" or effect.get("requires_charge") is not False
                or effect.get("skill_binding") is not None or effect.get("screen_coverage_verified") is not False):
            raise ValueError("A prototype cannot claim runtime validation, binding or charge rules")
        for kind in ("system", "material"):
            name = effect.get(kind + "_name", "")
            prefix = "NS_" if kind == "system" else "M_"
            path = effect.get(kind + "_path")
            if (not re.fullmatch(prefix + r"[A-Za-z0-9_]+_Candidate", name)
                    or path != root + "/" + name or path in paths):
                raise ValueError("Invalid, shared or duplicate candidate asset path")
            paths.add(path)
        for field in ("emitter_count", "sprite_renderer_count", "burst_count"):
            if type(effect.get(field)) is not int or effect[field] != 1:
                raise ValueError("Exactly one emitter, renderer and burst sprite are permitted")
        if (effect.get("emitter_name") != "DirectionalBurst" or effect.get("sim_target") != "CPUSim"
                or effect.get("renderer_class") != SPRITE_RENDERER or effect.get("system_loop_behavior") != "Once"
                or effect.get("emitter_loop_behavior") != "Once" or effect.get("kill_particles_at_lifetime_end") is not True):
            raise ValueError("Finite CPU sprite lifecycle required")
        for field in ("burst_time_world_seconds", "spawn_rate_per_second", "lifetime_variance_seconds"):
            _number(effect.get(field), field, 0.0, 0.0)
        for field in ("custom_time_dilation", "component_uniform_scale"):
            _number(effect.get(field), field, 1.0, 1.0)
        life = effect.get("particle_lifetime_world_seconds")
        _number(life, "particle lifetime", .08, .60)
        for field in ("emitter_duration_world_seconds", "system_duration_world_seconds"):
            _number(effect.get(field), field, life, .70)
        for field, length in (("sprite_size_cm", 2), ("fixed_bounds_half_extent_cm", 3), ("velocity_cm_per_second", 3)):
            value = effect.get(field)
            if not isinstance(value, list) or len(value) != length:
                raise ValueError("Invalid vector: " + field)
            for component in value:
                _number(component, field, 0.0 if field == "velocity_cm_per_second" else 1.0,
                        0.0 if field == "velocity_cm_per_second" else 72.0)
        required_extent = math.hypot(*effect["sprite_size_cm"]) * .5
        if min(effect["fixed_bounds_half_extent_cm"]) < required_extent:
            raise ValueError("Billboard bounds cannot clip the sprite diagonal")
        if (effect.get("local_space") is not False or effect.get("facing_mode") != "FaceCamera"
                or effect.get("alignment") != "Unaligned" or effect.get("sort_mode") != "None"
                or effect.get("disable_inherited_modules") != ["GravityForce", "Drag", "ScaleSpriteSizeBySpeed"]):
            raise ValueError("Stationary camera-facing preview contract changed")
        if effect.get("renderer") != {"cast_shadows": False, "collision": False, "lights": False,
                                     "ribbons": False, "decals": False, "camera_offset_cm": 0.0}:
            raise ValueError("Unexpected extra renderer or physical behavior")
        _number(effect.get("preview_min_camera_distance_cm"), "staging camera distance", 150.0, 1000.0)
        _number(effect.get("offline_mask_coverage_limit_fraction"), "offline mask fill cap", .01, .20)
        material = effect.get("material")
        if (not isinstance(material, dict) or material.get("blend_mode") != "Additive"
                or material.get("shading_model") != "Unlit" or material.get("two_sided") is not True
                or material.get("depth_test_enabled") is not True or material.get("scene_depth_sampling") is not False
                or material.get("inputs") != ["UV", "Age"] or material.get("age_source") != "ParticleRelativeTime"
                or material.get("mask_hlsl") != HLSL_MASKS[effect["id"]]):
            raise ValueError("Mask source/input/depth contract differs from the checked reference")
        tint = material.get("tint_linear_rgb")
        if not isinstance(tint, list) or len(tint) != 3:
            raise ValueError("RGB tint required")
        for component in tint:
            _number(component, "linear tint", 0.0, 1.0)
        _number(material.get("emissive_gain"), "emissive gain", 0.0, 1.0)
        if re.search(r"\b(?:Time|RealTime|SceneDepth|SceneTextureLookup|Texture2DSample|for|while|discard)\b", material["mask_hlsl"]):
            raise ValueError("Masks require finite normalized Age and texture-free local UV math")
    return True


def _sat(value):
    return min(1.0, max(0.0, value))


def _smooth(low, high, value):
    t = _sat((value - low) / (high - low))
    return t * t * (3.0 - 2.0 * t)


def evaluate_mask(identifier, u, v, age):
    """Numeric reference for the literal HLSL above; this is NOT a shader compiler."""
    if identifier not in IDS:
        raise ValueError("Unknown visual prototype")
    for name, value in (("u", u), ("v", v), ("age", age)):
        _number(value, name, -1.e6, 1.e6)
    x, y, t = (u - .5) * 2.0, (v - .5) * 2.0, _sat(age)
    r, a = math.hypot(x, y), math.atan2(y, x + .000001)
    if identifier == "telegraph":
        side = .64 + (.52 - .64) * _smooth(0, .8, t)
        qx, qy = abs(x), abs(y)
        h = math.exp(-((qy - side) * 48) ** 2) * _smooth(side - .22, side - .18, qx) * (1 - _smooth(side, side + .04, qx))
        vertical = math.exp(-((qx - side) * 48) ** 2) * _smooth(side - .22, side - .18, qy) * (1 - _smooth(side, side + .04, qy))
        return _sat(max(h, vertical) * _smooth(0, .15, t) * (1 - _smooth(.55, 1, t)) * .6)
    if identifier == "circle":
        ring = math.exp(-((r - .68) * 58) ** 2)
        ticks = abs(math.cos(a * 3)) ** 64 * _smooth(.47, .50, r) * (1 - _smooth(.60, .63, r))
        return _sat((ring * .6 + ticks * .45) * _smooth(0, .18, t) * (1 - _smooth(.62, 1, t)))
    if identifier == "release":
        taper = max(.2, _sat((.65 - x) / 1.4))
        shaft = math.exp(-(y * 18 / taper) ** 2) * _smooth(-.85, -.70, x) * (1 - _smooth(.25, .40, x))
        fork = math.exp(-((abs(y) - (.62 - x) * .45) * 48) ** 2) * _smooth(.08, .18, x) * (1 - _smooth(.60, .66, x))
        return _sat((shaft * .65 + fork * .55) * _smooth(0, .06, t) * (1 - t) ** 1.5)
    diamond = math.exp(-((abs(x) + abs(y) - (.15 + (.73 - .15) * t)) * 40) ** 2)
    rays = abs(math.cos(a * 2)) ** 40 * _smooth(.16, .20, r) * (1 - _smooth(.68, .74, r)) * (1 - t) ** 2
    core = math.exp(-r * r * 120) * _sat(1 - t * 4) ** 2
    return _sat((diamond * .55 + rays * .35 + core * .5) * _smooth(0, .045, t) * (1 - t) ** 1.3)


def run_tests():
    """No files, sockets, editor calls, Unreal imports, or external dependencies."""
    checks = 0
    cases = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    def rejected(call, message):
        try:
            call()
        except ValueError:
            check(True, message)
            return
        check(False, message)

    spec = build_spec("Offline01")
    cases += 1
    check(validate_spec(spec), "baseline spec")
    clone = build_spec("Offline01")
    clone["effects"][0]["material"]["tint_linear_rgb"][0] = .1
    check(spec["effects"][0]["material"]["tint_linear_rgb"][0] == .4, "detached specs")
    check(json.loads(json.dumps(spec)) == spec, "JSON round trip")
    for label in ("", "../Outside", "A/B", "A B", "A" * 25, 1, None):
        rejected(lambda label=label: build_spec(label), "invalid label")

    cases += 1
    source = {"emitters": [{"emitterName": "DirectionalBurst", "simTarget": "CPUSim", "bEnabled": True,
                             "rendererClasses": [{"refPath": SPRITE_RENDERER}]}]}
    check(validate_source_summary(source), "source topology")
    for key, value in (("simTarget", "GPUComputeSim"), ("bEnabled", False), ("emitterName", "Other"),
                       ("rendererClasses", [{"refPath": "/Script/Niagara.NiagaraRibbonRendererProperties"}])):
        bad = copy.deepcopy(source)
        bad["emitters"][0][key] = value
        rejected(lambda bad=bad: validate_source_summary(bad), "changed source topology")
    bad = copy.deepcopy(source)
    bad["emitters"].append(copy.deepcopy(bad["emitters"][0]))
    rejected(lambda: validate_source_summary(bad), "extra source emitter")

    cases += 1
    for field, value in (("burst_count", 0), ("burst_count", True), ("emitter_count", 2), ("sprite_renderer_count", 2),
                         ("system_loop_behavior", "Infinite"), ("emitter_loop_behavior", "Infinite"),
                         ("spawn_rate_per_second", 10), ("particle_lifetime_world_seconds", float("inf")),
                         ("particle_lifetime_world_seconds", float("nan")), ("particle_lifetime_world_seconds", -1),
                         ("custom_time_dilation", 3), ("component_uniform_scale", 8), ("requires_charge", True),
                         ("skill_binding", "/Game/DataAsset/AttackSkill/Any"), ("sprite_size_cm", [1000, 1000]),
                         ("fixed_bounds_half_extent_cm", [1, 1, 1]), ("velocity_cm_per_second", [0, 1, 0]),
                         ("system_duration_world_seconds", .01), ("screen_coverage_verified", True)):
        bad = copy.deepcopy(spec)
        bad["effects"][0][field] = value
        rejected(lambda bad=bad: validate_spec(bad), "unsafe effect contract: " + field)
    for field, value in (("mask_hlsl", "return 1;"), ("inputs", ["UV", "Age", "Core"]),
                         ("emissive_gain", 100), ("depth_test_enabled", False), ("tint_linear_rgb", [1, 1, float("nan")])):
        bad = copy.deepcopy(spec)
        bad["effects"][0]["material"][field] = value
        rejected(lambda bad=bad: validate_spec(bad), "unsafe material contract: " + field)
    bad = copy.deepcopy(spec)
    bad["integration"]["damage_authority"] = "ParticleCollision"
    rejected(lambda: validate_spec(bad), "gameplay authority injection")
    bad = copy.deepcopy(spec)
    bad["source"]["modify_source"] = True
    rejected(lambda: validate_spec(bad), "source mutation")

    cases += 1
    grid = [(i / 64, j / 64) for i in range(65) for j in range(65)]
    coverage = {}
    fingerprints = {}
    for effect in spec["effects"]:
        identifier = effect["id"]
        peak_fill = 0.0
        peak_alpha = 0.0
        for age in (.04, .15, .35, .60, .85):
            values = [evaluate_mask(identifier, u, v, age) for u, v in grid]
            check(all(math.isfinite(value) and 0 <= value <= 1 for value in values), "finite bounded mask: " + identifier)
            peak_alpha = max(peak_alpha, max(values))
            peak_fill = max(peak_fill, sum(value > .05 for value in values) / len(values))
        check("exp(-pow" not in HLSL_MASKS[identifier], "Gaussian square uses multiplication, avoiding negative-base HLSL pow: " + identifier)
        check(peak_alpha > .15, "visible geometry exists: " + identifier)
        check(0 < peak_fill <= effect["offline_mask_coverage_limit_fraction"], "sparse UV mask: " + identifier)
        for age in (-1, 0, 1, 2):
            check(all(evaluate_mask(identifier, u, v, age) == 0 for u, v in grid[::37]), "finite zero endpoints: " + identifier)
        check(all(evaluate_mask(identifier, u, v, .35) < 1.e-6 for u, v in ((0, 0), (0, 1), (1, 0), (1, 1))), "clean corners: " + identifier)
        fingerprints[identifier] = [evaluate_mask(identifier, u, v, .35) for u, v in grid]
        coverage[identifier] = round(peak_fill, 6)
    for first_index, first in enumerate(IDS):
        for second in IDS[first_index + 1:]:
            difference = sum(abs(a - b) for a, b in zip(fingerprints[first], fingerprints[second])) / len(grid)
            check(difference > .0025, "distinct masks: " + first + "/" + second)
    check(evaluate_mask("circle", .5, .5, .35) < 1.e-8, "floating sigil empty center")
    check(evaluate_mask("telegraph", .5, .5, .35) < 1.e-8, "preparation brackets empty center")
    rejected(lambda: evaluate_mask("unknown", .5, .5, .5), "invalid visual id")
    rejected(lambda: evaluate_mask("circle", .5, .5, float("nan")), "nonfinite mask input")
    print("RESULT=MAGIC_PARTICLE_SPEC_TESTS_PASSED cases=" + str(cases) + " checks=" + str(checks))
    print("UV_MASK_FILL_FRACTION " + json.dumps(coverage, sort_keys=True))
    print("UNREAL_IMPORTED=false HLSL_COMPILED=false ASSETS_CREATED=false RUNTIME_VALIDATED=false")
    return {"cases": cases, "checks": checks, "uv_mask_peak_fill_fraction": coverage}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--self-test", action="store_true")
    action.add_argument("--print-spec", action="store_true")
    parser.add_argument("--label")
    arguments = parser.parse_args()
    if arguments.print_spec:
        if arguments.label is None:
            parser.error("--print-spec requires an explicit fresh --label")
        print(json.dumps(build_spec(arguments.label), indent=2))
    else:
        run_tests()
