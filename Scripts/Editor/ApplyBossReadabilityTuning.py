"""Guarded new profile variant + one live monster-DA reference; preserve originals.

Run only after native build and AuthorBossBasicWarning.py succeed. A partial attempt
is intentionally not retried automatically: inspect assets and stage journal first.
"""
import hashlib
import json
import math
import re
from pathlib import Path
import unreal

SOURCE = "/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/DA_Azathor_BossPatterns"
VARIANT = SOURCE + "_Readability_1103"
MONSTER = "/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor"
MATERIAL = "/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/M_Azathor_BasicConeWarning_1103"
TIMINGS = {
    "attack_windup_seconds": 1.0, "attack_recovery_seconds": 1.380952,
    "frontal_wave_windup_seconds": 1.6, "frontal_wave_recovery_seconds": .945455,
    "abyss_zone_windup_seconds": 1.9, "abyss_zone_recovery_seconds": 1.085714,
}
EXPECTED = {
    "attack_windup_seconds": .308, "attack_recovery_seconds": .425333,
    "frontal_wave_windup_seconds": 1.1, "frontal_wave_recovery_seconds": .65,
    "abyss_zone_windup_seconds": 1.4, "abyss_zone_recovery_seconds": .8,
}


def canonical(value):
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return str(value)


def profile_property_names(project):
    header = project / "Source/Rogue10m/Enemy/Rogue10mMonsterAttackPatternProfile.h"
    text = header.read_text(encoding="utf-8-sig")
    names = re.findall(r"^\s*(?:float|bool|TSoftObjectPtr<[^>]+>)\s+([A-Za-z0-9_]+)\s*(?:=[^;]*)?;", text, re.MULTILINE)
    result = []
    for name in names:
        if name.startswith("b") and len(name) > 1 and name[1].isupper():
            name = name[1:]
        result.append(re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower())
    if len(result) < 40 or len(set(result)) != len(result):
        raise RuntimeError("Unexpected native profile property inventory; inspect header first")
    return result


def snapshot(obj, names):
    return {name: canonical(obj.get_editor_property(name)) for name in names}


def file_hash(project, asset_path):
    if not asset_path.startswith("/Game/"):
        raise RuntimeError("Only owned project assets allowed")
    path = project / "Content" / (asset_path[len("/Game/"):] + ".uasset")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def same(actual, expected):
    if isinstance(actual, float) and isinstance(expected, (float, int)):
        return math.isfinite(actual) and abs(actual-expected) < .00005
    return actual == expected


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    folder = project / "Feature/doc/evidence/boss-readability-20261103"
    journal = folder / "profile-application-start.json"
    report = folder / "profile-application.json"
    if journal.exists() or report.exists() or unreal.EditorAssetLibrary.does_asset_exist(VARIANT):
        raise RuntimeError("Prior application state exists; inspect assets/journal before any retry")
    source, monster, material = (unreal.load_asset(path) for path in (SOURCE, MONSTER, MATERIAL))
    if not source or not monster or not isinstance(material, unreal.Material):
        raise RuntimeError("Expected source profile, monster DA and authored basic material must load")
    if material.get_editor_property("material_domain") != unreal.MaterialDomain.MD_DEFERRED_DECAL:
        raise RuntimeError("Basic warning must be the new deferred-decal material")
    if canonical(monster.get_editor_property("attack_pattern_profile")) != source.get_path_name():
        raise RuntimeError("Live monster profile differs from inspected source; preserve it and review")
    names = profile_property_names(project)
    before = snapshot(source, names)
    if not before.get("enable_boss_patterns"):
        raise RuntimeError("Inspected source is not enabled for boss patterns")
    for name, value in EXPECTED.items():
        if not same(before.get(name), value):
            raise RuntimeError("Source timing changed since readback: " + name)
    if before.get("require_basic_telegraph") or before.get("basic_telegraph_material") is not None:
        raise RuntimeError("Source already has authored basic-warning settings; do not replace them")
    hp_damage_before = snapshot(monster, ("max_health", "attack_damage"))
    hashes_before = {path: file_hash(project, path) for path in (SOURCE, MATERIAL)}
    folder.mkdir(parents=True, exist_ok=True)
    journal.write_text(json.dumps({"stage": "preconditions_passed_no_asset_mutation_yet", "source": SOURCE,
        "variant": VARIANT, "live_monster": MONSTER, "source_properties": before,
        "preserve_hashes": hashes_before, "monster_HP_damage": hp_damage_before}, ensure_ascii=False, indent=2), encoding="utf-8")

    variant = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, VARIANT)
    if not variant or snapshot(variant, names) != before:
        raise RuntimeError("Variant duplication did not preserve all declared profile properties; no live connection made")
    expected_after = dict(before)
    expected_after.update(TIMINGS)
    expected_after.update({"basic_telegraph_material": material.get_path_name(), "require_basic_telegraph": True})
    for name, value in TIMINGS.items():
        variant.set_editor_property(name, value)
    variant.set_editor_property("basic_telegraph_material", material)
    variant.set_editor_property("require_basic_telegraph", True)
    after = snapshot(variant, names)
    if any(not same(after[name], expected_after[name]) for name in names):
        raise RuntimeError("Variant differs outside the exact allowlist; original live reference is unchanged")
    if snapshot(source, names) != before or snapshot(monster, ("max_health", "attack_damage")) != hp_damage_before:
        raise RuntimeError("Original values changed unexpectedly; do not connect variant")
    if not unreal.EditorAssetLibrary.save_loaded_asset(variant, False):
        raise RuntimeError("Variant save uncertain; inspect before retry; original live reference unchanged")
    if any(file_hash(project, path) != digest for path, digest in hashes_before.items()):
        raise RuntimeError("Original profile or material bytes changed; do not connect variant")

    monster.set_editor_property("attack_pattern_profile", variant)
    if canonical(monster.get_editor_property("attack_pattern_profile")) != variant.get_path_name():
        raise RuntimeError("Live pointer set uncertain; inspect actual state before retry")
    if snapshot(monster, ("max_health", "attack_damage")) != hp_damage_before:
        raise RuntimeError("Live HP/damage changed unexpectedly; inspect before saving")
    if not unreal.EditorAssetLibrary.save_loaded_asset(monster, False):
        raise RuntimeError("Live monster DA save uncertain; read back actual package before retry")
    source_after = snapshot(source, names)
    hashes_after = {path: file_hash(project, path) for path in hashes_before}
    if source_after != before or hashes_after != hashes_before:
        raise RuntimeError("Preserved source/material changed unexpectedly; report not finalized")
    result = {"status": "saved_variant_and_one_live_monster_DA_reference", "source": source.get_path_name(),
        "variant": variant.get_path_name(), "live_monster": monster.get_path_name(),
        "live_profile_readback": canonical(monster.get_editor_property("attack_pattern_profile")),
        "all_declared_profile_properties_before": before, "all_declared_profile_properties_after": after,
        "allowed_changes": list(TIMINGS) + ["basic_telegraph_material", "require_basic_telegraph"],
        "preserved_source_and_material_hashes": hashes_after, "preserved_monster_HP_damage": hp_damage_before,
        "enrage": "Existing basic exception retained; basic 1.0s, wave1.2s, zone1.425s",
        "warning_scope": "Conservative capsule-expanded narrow footprint; physical overlap, selected target, height and LOS remain required",
        "rollback": "Reconnect live monster attack_pattern_profile to original source path; preserve new assets/evidence",
        "runtime_and_rendered_validation": "pending"}
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[BossReadability] " + json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
