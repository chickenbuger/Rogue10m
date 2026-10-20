"""Author the hybrid tree catalog and five project-owned greatsword techniques.

Run only in the Unreal Editor after building/restarting the module. Existing
attack assets, default inputs, initial unlocks, animations and VFX are preserved.
The caller owns Editor execution; importing this module has no side effects.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


CATALOG_PATH = "/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog"
GREAT_PROFILE_PATH = "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_GreatSword"
GREAT_SOURCE_ROOT = "/Game/DataAsset/AttackSkill/ClassCombat/GreatSword"
DERIVED_ROOT = "/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword"
SOURCE_TAG = "Rogue10mHybridSource"
AUTHOR_TAG = "Rogue10mHybridAuthoringVersion"
AUTHOR_VERSION = "1"
EXPECTED_CATALOG_NODES = 53

# Damage is an explicit additive skill base; character AttackPower still enters
# the existing RollDamage path. Ranges, costs and cooldowns are new tuned values.
# Montage, playback rate, hit delay, hit pulses and Niagara remain source-owned.
TECHNIQUES = (
    {"id": "CrushingSlash", "name": "분쇄참", "source": "Primary03", "mastery": 5,
     "damage": 31.0, "cooldown": 1.10, "range": 285.0, "stamina": 8.0,
     "slot": "SPECIAL", "description": "대검의 기존3타 내려베기를 독립 강타로 운용한다."},
    {"id": "Whirlwind", "name": "회전참", "source": "Special", "mastery": 6,
     "damage": 29.0, "cooldown": 1.05, "range": 300.0, "stamina": 10.0,
     "slot": "SPECIAL", "description": "기존 대검 특수 동작과 원형 판정을 재사용한 연계용 광역 공격."},
    {"id": "GuardBreak", "name": "파쇄참", "source": "Primary02", "mastery": 7,
     "damage": 27.0, "cooldown": 0.95, "range": 285.0, "stamina": 7.0,
     "slot": "SPECIAL", "description": "기존 대검2타 동작을 사용하는 짧은 독립 공격. 가드 성공이나 반격 판정을 새로 부여하지 않는다."},
    {"id": "EarthSplitter", "name": "대지파쇄", "source": "Charged", "mastery": 12,
     "damage": 55.0, "cooldown": 1.65, "range": 330.0, "stamina": 16.0,
     "slot": "CHARGED_PRIMARY", "description": "기존 대검 차징 동작과 타격 시점을 유지한 중량 핵심기."},
    {"id": "Ultimate", "name": "패왕참", "source": "Charged", "mastery": 20,
     "damage": 72.0, "cooldown": 2.40, "range": 360.0, "stamina": 24.0,
     "slot": "CHARGED_PRIMARY", "description": "세 수련 경로 중 하나를 완성하면 전수하는 대검 최종기. 기존 차징 동작과 소요 시간을 재사용한다."},
)

FIGHTER_SKILL_PATHS = (
    "/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab",
    "/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightJab",
    "/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightStraight",
    "/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_RightHook",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Jab",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_JumpSlam",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave",
    "/Game/DataAsset/AttackSkill/MartialArts/DA_MartialArt_GoldenBody",
)
GREAT_BASE_PATHS = tuple(
    f"{GREAT_SOURCE_ROOT}/DA_Attack_GreatSword_{variant}"
    for variant in ("Primary01", "Primary02", "Primary03", "Special", "Charged")
)
PROTECTED_PROFILES = (
    "/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed",
    "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle",
    GREAT_PROFILE_PATH,
)
PRESERVED_SKILL_FIELDS = (
    "skill_name", "skill_description", "input_slot", "skill_tree_node_type",
    "damage", "attack_cooldown", "charge_seconds", "resource_costs",
    "attack_shape", "attack_range", "attack_trace_radius", "arc_angle_degrees",
    "box_half_width", "box_half_height", "circle_forward_offset",
    "hit_mode", "hit_count", "hit_interval", "max_targets_per_hit", "max_hits_per_target",
    "hit_start_delay_seconds", "attack_montage", "animation_play_rate",
    "enable_combo", "next_combo_skill", "prerequisite_skills", "unlock_conditions",
    "passive_damage_reduction", "cast_effect", "charge_effect", "impact_effect",
)
PRESERVED_PRESENTATION_FIELDS = (
    "attack_montage", "animation_play_rate", "charge_seconds", "hit_start_delay_seconds",
    "hit_mode", "hit_count", "hit_interval", "max_targets_per_hit", "max_hits_per_target",
    "attack_shape", "attack_trace_radius", "arc_angle_degrees", "box_half_width",
    "box_half_height", "circle_forward_offset",
    "cast_effect", "charge_effect", "impact_effect", "effect_attach_socket",
    "first_person_effect_offset", "use_first_person_effect_overrides",
    "first_person_effect_rotation", "first_person_cast_scale_multiplier",
    "first_person_charge_scale_multiplier", "first_person_impact_scale_multiplier",
    "first_person_emission_duration_multiplier", "first_person_off_hand_effect_scale_multiplier",
    "cast_effect_scale", "charge_effect_scale", "impact_effect_scale",
    "cast_effect_emission_duration", "impact_effect_emission_duration",
    "attack_effect_time_dilation", "spawn_effect_on_off_hand", "off_hand_effect_attach_socket",
)
DERIVED_TUNING_FIELDS = (
    "skill_name", "skill_description", "input_slot", "skill_tree_node_type",
    "damage", "attack_cooldown", "attack_range", "resource_costs", "enable_combo",
    "next_combo_skill", "prerequisite_skills", "unlock_conditions",
    "passive_damage_reduction", "unlock_reward_text", "icon_label", "draw_debug_attack",
)


def require(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required existing asset missing: {path}")
    return asset


def existing_asset(path):
    return (unreal.EditorAssetLibrary.load_asset(path)
            if unreal.EditorAssetLibrary.does_asset_exist(path) else None)


def enum_value(enum_type, name):
    value = getattr(enum_type, name, None)
    if value is None:
        raise RuntimeError(f"Missing enum member: {enum_type}.{name}")
    return value


def plain(value):
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "items"):
        return {str(key): plain(item) for key, item in value.items()}
    if isinstance(value, (list, tuple, unreal.Array)):
        return [plain(item) for item in value]
    if hasattr(value, "export_text"):
        return value.export_text()
    # Unreal structs export stable field text; retained only as evidence, not
    # evaluated or used to create shell or Python commands.
    return str(value)


def fields(asset, names):
    return {name: plain(asset.get_editor_property(name)) for name in names}


def default_contract(profile):
    return fields(profile, ("default_skill_bindings", "initially_unlocked_skills",
                            "default_dodge_skill", "max_jump_count"))


def destination(row):
    return f"{DERIVED_ROOT}/DA_Hybrid_GreatSword_{row['id']}"


def require_properties(asset, names):
    """Fail on stale reflection before duplicating or changing any asset."""
    for name in sorted(set(names)):
        try:
            asset.get_editor_property(name)
        except Exception as error:
            raise RuntimeError(
                f"Reflected property unavailable: {asset.get_path_name()}::{name}. "
                "Build and restart the Editor before authoring."
            ) from error


def reflection_preflight(assets, profiles):
    all_skill_fields = PRESERVED_SKILL_FIELDS + PRESERVED_PRESENTATION_FIELDS + DERIVED_TUNING_FIELDS
    for asset in assets.values():
        require_properties(asset, all_skill_fields)
    for profile in profiles.values():
        require_properties(profile, ("skill_tree_skills", "default_skill_bindings",
                                     "initially_unlocked_skills", "default_dodge_skill", "max_jump_count"))
    catalog_default = unreal.get_default_object(unreal.Rogue10mSkillTreeDataAsset)
    require_properties(catalog_default, ("nodes",))
    if not callable(getattr(catalog_default, "build_default_catalog", None)):
        raise RuntimeError("BuildDefaultCatalog is not exposed; build and restart the Editor.")
    for node in catalog_default.get_editor_property("nodes"):
        for name in ("node_id", "prerequisite_groups"):
            node.get_editor_property(name)
        for group in node.get_editor_property("prerequisite_groups"):
            group.get_editor_property("node_ids")
    cost = unreal.Rogue10mAttackResourceCost()
    cost.get_editor_property("resource_type")
    cost.get_editor_property("cost")
    for member in {row["slot"] for row in TECHNIQUES}:
        enum_value(unreal.Rogue10mAttackInputSlot, member)
    enum_value(unreal.Rogue10mAttackResourceType, "STAMINA")
    enum_value(unreal.Rogue10mSkillTreeNodeType, "ACTIVE_TECHNIQUE")


def preflight():
    if not hasattr(unreal, "Rogue10mSkillTreeDataAsset"):
        raise RuntimeError("Build the hybrid-tree module and restart the Editor before authoring.")
    assets = {path: require(path) for path in FIGHTER_SKILL_PATHS + GREAT_BASE_PATHS}
    profiles = {path: require(path) for path in PROTECTED_PROFILES}
    reflection_preflight(assets, profiles)
    current_tree = {
        skill.get_path_name()
        for skill in profiles[GREAT_PROFILE_PATH].get_editor_property("skill_tree_skills")
        if skill is not None
    }
    allowed_tree = set(GREAT_BASE_PATHS).union(destination(row) for row in TECHNIQUES)
    # get_path_name() includes '.AssetName'; the declared package paths do not.
    package_tree = {path.split(".", 1)[0] for path in current_tree}
    if not set(GREAT_BASE_PATHS).issubset(package_tree) or not package_tree.issubset(allowed_tree):
        raise RuntimeError("Unexpected existing greatsword tree; review before authoring.")
    for row in TECHNIQUES:
        source = assets[f"{GREAT_SOURCE_ROOT}/DA_Attack_GreatSword_{row['source']}"]
        if source.get_editor_property("attack_montage") is None:
            raise RuntimeError(f"Source montage missing: {source.get_path_name()}")
        existing = existing_asset(destination(row))
        if existing is not None:
            tag = unreal.EditorAssetLibrary.get_metadata_tag(existing, SOURCE_TAG)
            if tag != source.get_path_name():
                raise RuntimeError(f"Refusing to replace an unrelated destination: {destination(row)}")
    existing_catalog = existing_asset(CATALOG_PATH)
    if existing_catalog is not None:
        if not isinstance(existing_catalog, unreal.Rogue10mSkillTreeDataAsset):
            raise RuntimeError("Catalog path already contains an unrelated asset type.")
        if unreal.EditorAssetLibrary.get_metadata_tag(existing_catalog, AUTHOR_TAG) != AUTHOR_VERSION:
            raise RuntimeError("Refusing to overwrite an unrecognized catalog at the authoring path.")
    return assets, profiles


def make_technique(row, source):
    path = destination(row)
    skill = existing_asset(path)
    if skill is None:
        unreal.EditorAssetLibrary.make_directory(DERIVED_ROOT)
        if not unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), path):
            raise RuntimeError(f"Could not duplicate the existing skill: {path}")
        skill = require(path)
    source_presentation = fields(source, PRESERVED_PRESENTATION_FIELDS)
    # Reuse current source presentation even when this idempotent authoring pass
    # follows a later camera/hit-timing fix. Never rebake or alter the animation.
    for field in PRESERVED_PRESENTATION_FIELDS:
        skill.set_editor_property(field, source.get_editor_property(field))
    cost = unreal.Rogue10mAttackResourceCost()
    cost.set_editor_properties({
        "resource_type": enum_value(unreal.Rogue10mAttackResourceType, "STAMINA"),
        "cost": row["stamina"],
    })
    skill.set_editor_properties({
        "skill_name": row["name"],
        "skill_description": row["description"] + f" 숙련M{row['mastery']}·선행 전수로 해금.",
        "input_slot": enum_value(unreal.Rogue10mAttackInputSlot, row["slot"]),
        "skill_tree_node_type": enum_value(unreal.Rogue10mSkillTreeNodeType, "ACTIVE_TECHNIQUE"),
        "damage": row["damage"],
        "attack_cooldown": row["cooldown"],
        "attack_range": row["range"],
        "resource_costs": [cost],
        "enable_combo": False,
        "next_combo_skill": None,
        "prerequisite_skills": [],
        "unlock_conditions": [],
        "passive_damage_reduction": 0.0,
        "unlock_reward_text": "혼합 스킬트리 숙련 전수 · 기존 대검 동작 재사용",
        "icon_label": row["name"],
        "draw_debug_attack": False,
    })
    unreal.EditorAssetLibrary.set_metadata_tag(skill, SOURCE_TAG, source.get_path_name())
    unreal.EditorAssetLibrary.set_metadata_tag(skill, AUTHOR_TAG, AUTHOR_VERSION)
    authored_presentation = fields(skill, PRESERVED_PRESENTATION_FIELDS)
    if authored_presentation != source_presentation:
        changed = {name: [source_presentation[name], authored_presentation[name]]
                   for name in source_presentation
                   if source_presentation[name] != authored_presentation[name]}
        raise RuntimeError(f"Source animation/hit timing/presentation changed: {path}: {changed}")
    return skill


def make_catalog():
    catalog = existing_asset(CATALOG_PATH)
    if catalog is None:
        package, name = CATALOG_PATH.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(package)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.Rogue10mSkillTreeDataAsset)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, package, unreal.Rogue10mSkillTreeDataAsset, factory)
    if catalog is None:
        raise RuntimeError("Could not create the hybrid catalog Data Asset.")
    catalog.build_default_catalog()
    unreal.EditorAssetLibrary.set_metadata_tag(catalog, AUTHOR_TAG, AUTHOR_VERSION)
    nodes = list(catalog.get_editor_property("nodes"))
    ids = [str(node.get_editor_property("node_id")) for node in nodes]
    if len(nodes) != EXPECTED_CATALOG_NODES or len(set(ids)) != len(nodes):
        raise RuntimeError(f"Unexpected catalog size or duplicate NodeId: {len(nodes)}")
    expected = {"F.Basic", "F.Ultimate", "G.Basic", "G.Ultimate"}
    expected.update(f"I.{identity}.{stage}" for identity in
                    ("InternalEnergy", "FightingSpirit", "Mana")
                    for stage in ("Entry", "Advanced", "Choice"))
    if not expected.issubset(set(ids)):
        raise RuntimeError("Required shared identity or class nodes missing.")
    for node in nodes:
        for group in node.get_editor_property("prerequisite_groups"):
            parents = [str(parent) for parent in group.get_editor_property("node_ids")]
            if not parents or not set(parents).issubset(set(ids)):
                raise RuntimeError(f"Invalid catalog prerequisite group: {parents}")
    return catalog, ids


def main():
    assets, profiles = preflight()
    before = {path: fields(skill, PRESERVED_SKILL_FIELDS) for path, skill in assets.items()}
    contracts = {path: default_contract(profile) for path, profile in profiles.items()}
    previous_great_tree = list(profiles[GREAT_PROFILE_PATH].get_editor_property("skill_tree_skills"))
    derived = []
    for row in TECHNIQUES:
        source = assets[f"{GREAT_SOURCE_ROOT}/DA_Attack_GreatSword_{row['source']}"]
        derived.append(make_technique(row, source))
    catalog, ids = make_catalog()
    great = profiles[GREAT_PROFILE_PATH]
    next_tree = list(previous_great_tree)
    for skill in derived:
        if skill not in next_tree:
            next_tree.append(skill)
    great.set_editor_property("skill_tree_skills", next_tree)
    if {skill.get_path_name().split(".", 1)[0] for skill in next_tree} != set(GREAT_BASE_PATHS).union(
            destination(row) for row in TECHNIQUES):
        raise RuntimeError("Unexpected existing greatsword skills; review before changing the profile.")
    after = {path: fields(skill, PRESERVED_SKILL_FIELDS) for path, skill in assets.items()}
    if before != after:
        raise RuntimeError("An existing fighter or greatsword source skill changed.")
    if any(default_contract(profiles[path]) != contract for path, contract in contracts.items()):
        raise RuntimeError("A default binding, initial unlock, dodge or jump contract changed.")
    for asset in derived + [catalog, great]:
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save scoped hybrid-tree asset: {asset.get_path_name()}")
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    evidence = project / "Feature" / "doc" / "evidence" / "hybrid-skill-tree-runtime-20261020"
    evidence.mkdir(parents=True, exist_ok=True)
    report = {
        "catalog": catalog.get_path_name(), "catalog_nodes": len(ids), "node_ids": ids,
        "great_profile": great.get_path_name(), "great_tree_skills": len(next_tree),
        "original_attacks_preserved": before == after,
        "default_inputs_initial_unlocks_dodge_jump_preserved": True,
        "source_skill_fields_before": before, "source_skill_fields_after": after,
        "derived_tuning": list(TECHNIQUES),
        "derived": {skill.get_path_name(): fields(skill, PRESERVED_SKILL_FIELDS)
                    for skill in derived},
        "source_ownership_scope": "Existing project-owned assets reused; no new Fab import or ownership claim.",
    }
    (evidence / "asset-authoring.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("RESULT=HYBRID_TREE_ASSETS_CREATED catalog_nodes=53 great_skills=10 "
               "derived=5 source_attacks_preserved=true defaults_preserved=true")


if __name__ == "__main__":
    main()
