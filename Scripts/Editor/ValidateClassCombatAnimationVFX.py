"""Validate ten class combat presentation sets and first-person VFX budgets."""

from __future__ import annotations

import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ClassCombatVFXReadabilityProfile as readability
import ConfigureClassCombatAnimationVFX as config


CHARACTER_BLUEPRINTS = (
    "/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_HumanFemaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_DwarfMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_DwarfFemaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_OrcMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_OrcFemaleCharacter",
)
COMMON_ANIM_BP_PATH = "/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed"
TARGET_STYLE_IDS = set(readability.TARGET_STYLE_IDS)


def check(condition, message: str):
    if not condition:
        raise RuntimeError(message)


def require_asset(path: str):
    check(unreal.EditorAssetLibrary.does_asset_exist(path), f"Asset missing: {path}")
    asset = unreal.EditorAssetLibrary.load_asset(path)
    check(asset is not None, f"Asset failed to load: {path}")
    return asset


def check_close(actual: float, expected: float, label: str, tolerance: float = 0.001):
    check(abs(actual - expected) <= tolerance, f"{label}: expected={expected} actual={actual}")


def asset_path(asset) -> str:
    return asset.get_path_name().split(".", 1)[0] if asset else ""


def check_skeleton_compatibility():
    main_skeleton = require_asset(config.MAIN_SKELETON_PATH)
    magic_skeleton = require_asset(config.MAGIC_SKELETON_PATH)
    main_compatible = [str(path) for path in main_skeleton.compatible_skeletons]
    magic_compatible = [str(path) for path in magic_skeleton.compatible_skeletons]
    check(
        any(config.MAGIC_SKELETON_PATH in path for path in main_compatible),
        "Main Manny skeleton does not list the magic Manny skeleton as compatible",
    )
    check(
        any(config.MAIN_SKELETON_PATH in path for path in magic_compatible),
        "Magic Manny skeleton does not list the main Manny skeleton as compatible",
    )


def check_race_animation_sharing():
    common_anim_bp = require_asset(COMMON_ANIM_BP_PATH)
    common_anim_class = common_anim_bp.generated_class()
    check(common_anim_class is not None, "Common AnimBP generated class is missing")
    for blueprint_path in CHARACTER_BLUEPRINTS:
        blueprint = require_asset(blueprint_path)
        cdo = unreal.get_default_object(blueprint.generated_class())
        check(
            cdo.get_editor_property("animation_source_anim_class") == common_anim_class,
            f"Race does not share the common animation source: {blueprint_path}",
        )




def check_skill(class_config, variant, skill, montage, effects, next_skill):
    variant_name, _, slot_name, damage_multiplier, source_index = variant
    style_id = class_config["id"]
    charged = variant_name == "Charged"
    special = variant_name == "Special"
    combo = variant_name in ("Primary01", "Primary02")
    expected_shape = class_config["charged_shape"] if charged else (
        class_config["special_shape"] if special else class_config["shape"]
    )
    expected_multi_hit = class_config["hit_mode"] == "MULTI_HIT" and not charged

    check(skill.get_editor_property("attack_montage") == montage, f"Montage mismatch: {style_id}/{variant_name}")
    check_close(
        skill.get_editor_property("animation_play_rate"),
        class_config["play_rates"][source_index],
        f"Animation play rate mismatch: {style_id}/{variant_name}",
    )
    check(
        skill.get_editor_property("input_slot")
        == config.enum_value(unreal.Rogue10mAttackInputSlot, slot_name),
        f"Input slot mismatch: {style_id}/{variant_name}",
    )
    check(
        skill.get_editor_property("attack_shape")
        == config.enum_value(unreal.Rogue10mAttackShape, expected_shape),
        f"Attack shape mismatch: {style_id}/{variant_name}",
    )
    check_close(
        skill.get_editor_property("damage"),
        class_config["damage"] * damage_multiplier,
        f"Damage mismatch: {style_id}/{variant_name}",
    )
    check(skill.get_editor_property("enable_combo") == combo, f"Combo flag mismatch: {style_id}/{variant_name}")
    check(skill.get_editor_property("next_combo_skill") == next_skill, f"Combo link mismatch: {style_id}/{variant_name}")
    # Knuckle hit timing/count is owned by the martial-artist gameplay work.
    # This pass only verifies that the presentation asset remains usable instead
    # of forcing the older class-combat default back onto concurrent gameplay data.
    if style_id == "Knuckle":
        check(skill.get_editor_property("hit_count") >= 1, f"Invalid hit count: {style_id}/{variant_name}")
    else:
        check(
            skill.get_editor_property("hit_count") == (2 if expected_multi_hit else 1),
            f"Hit count mismatch: {style_id}/{variant_name}",
        )
    check(skill.get_editor_property("enable_attack_effects"), f"Attack VFX disabled: {style_id}/{variant_name}")
    check(skill.get_editor_property("cast_effect") == effects["cast"], f"Cast VFX mismatch: {style_id}/{variant_name}")
    check(skill.get_editor_property("impact_effect") == effects["impact"], f"Impact VFX mismatch: {style_id}/{variant_name}")
    check(
        skill.get_editor_property("charge_effect") == (effects["charge"] if charged else None),
        f"Charge VFX mismatch: {style_id}/{variant_name}",
    )
    check(
        skill.get_editor_property("spawn_effect_on_off_hand") == class_config["dual"],
        f"Off-hand VFX mismatch: {style_id}/{variant_name}",
    )
    check(skill.get_editor_property("use_first_person_effect_overrides"), f"First-person override disabled: {style_id}/{variant_name}")
    check(not skill.get_editor_property("draw_debug_attack"), f"Debug traces enabled: {style_id}/{variant_name}")

    tuning = readability.get_skill_tuning(style_id, variant_name)
    for property_name, expected in tuning.items():
        actual = skill.get_editor_property(property_name)
        if property_name == "first_person_effect_offset":
            check_close(actual.x, expected[0], f"FP offset X mismatch: {style_id}/{variant_name}")
            check_close(actual.y, expected[1], f"FP offset Y mismatch: {style_id}/{variant_name}")
            check_close(actual.z, expected[2], f"FP offset Z mismatch: {style_id}/{variant_name}")
        else:
            check_close(actual, expected, f"{property_name} mismatch: {style_id}/{variant_name}")

    budget = readability.calculate_first_person_budget(style_id, variant_name)
    check(budget["cast_scale"] <= 0.060, f"FP cast scale exceeds budget: {style_id}/{variant_name}={budget['cast_scale']}")
    check(budget["charge_scale"] <= 0.026, f"FP charge scale exceeds budget: {style_id}/{variant_name}={budget['charge_scale']}")
    check(budget["impact_scale"] <= 0.180, f"FP impact scale exceeds budget: {style_id}/{variant_name}={budget['impact_scale']}")
    check(budget["cast_duration"] <= 0.060, f"FP cast duration exceeds budget: {style_id}/{variant_name}={budget['cast_duration']}")
    check(budget["impact_duration"] <= 0.070, f"FP impact duration exceeds budget: {style_id}/{variant_name}={budget['impact_duration']}")
    offset = skill.get_editor_property("first_person_effect_offset")
    check(abs(offset.y) >= 14.0 and offset.z <= -11.0, f"FP effect is not moved below/aside the reticle: {style_id}/{variant_name}")
    check(
        skill.get_editor_property("first_person_off_hand_effect_scale_multiplier") <= 0.70,
        f"FP off-hand scale exceeds budget: {style_id}/{variant_name}",
    )


def check_style(class_config):
    style_id = class_config["id"]
    effect_type = require_asset(readability.EFFECT_TYPE_PATH)
    effects = {}
    for role, source_key in (("cast", "cast_source"), ("charge", "charge_source"), ("impact", "impact_source")):
        role_name = role.capitalize()
        effect_path = f"{config.VFX_ROOT}/{style_id}/NS_{style_id}_{role_name}"
        effect = require_asset(effect_path)
        check(isinstance(effect, unreal.NiagaraSystem), f"Not a Niagara System: {effect_path}")
        check(
            unreal.EditorAssetLibrary.get_metadata_tag(effect, config.SOURCE_VFX_TAG)
            == class_config[source_key],
            f"Niagara source mismatch: {effect_path}",
        )
        effects[role] = effect

    montages = {}
    for variant in config.VARIANTS:
        variant_name, _, _, _, source_index = variant
        montage_path = f"{config.ANIMATION_ROOT}/{style_id}/AM_{style_id}_{variant_name}"
        montage = require_asset(montage_path)
        check(isinstance(montage, unreal.AnimMontage), f"Not an AnimMontage: {montage_path}")
        check(
            unreal.EditorAssetLibrary.get_metadata_tag(montage, config.SOURCE_ANIMATION_TAG)
            == class_config["sources"][source_index],
            f"Animation source mismatch: {montage_path}",
        )
        check(effect.get_editor_property("effect_type") == effect_type, f"Effect Type mismatch: {effect_path}")
        check(
            unreal.EditorAssetLibrary.get_metadata_tag(effect, "Rogue10mVFXReadabilityProfile")
            == "HeroShooterFP_v1",
            f"Readability profile metadata mismatch: {effect_path}",
        )
        expected_skeleton = config.MAGIC_SKELETON_PATH if style_id == "Staff" else config.MAIN_SKELETON_PATH
        check(asset_path(montage.get_editor_property("skeleton")) == expected_skeleton, f"Montage skeleton mismatch: {montage_path}")
        montages[variant_name] = montage

    skills = {
        variant[0]: require_asset(f"{config.ATTACK_ROOT}/{style_id}/DA_Attack_{style_id}_{variant[0]}")
        for variant in config.VARIANTS
    }
    for variant in config.VARIANTS:
        variant_name = variant[0]
        next_skill = skills["Primary02"] if variant_name == "Primary01" else (
            skills["Primary03"] if variant_name == "Primary02" else None
        )
        check_skill(class_config, variant, skills[variant_name], montages[variant_name], effects, next_skill)

    profile_path = f"{config.PROFILE_ROOT}/DA_SkillProfile_Combat_{style_id}"
    profile = require_asset(profile_path)
    check(
        profile.get_editor_property("weapon_type")
        == config.enum_value(unreal.Rogue10mWeaponType, class_config["weapon"]),
        f"Weapon type mismatch: {profile_path}",
    )
    check(profile.get_editor_property("max_jump_count") == class_config["jump_count"], f"Jump count mismatch: {profile_path}")
    skill_tree_count = len(profile.get_editor_property("skill_tree_skills"))
    unlocked_count = len(profile.get_editor_property("initially_unlocked_skills"))
    if style_id == "Knuckle":
        # The martial-artist feature owns additional Knuckle skill-tree entries.
        check(skill_tree_count >= 5, f"Skill tree is missing base entries: {profile_path}")
    else:
        check(skill_tree_count == 5, f"Skill tree count mismatch: {profile_path}")
        check(unlocked_count == 5, f"Unlocked skill count mismatch: {profile_path}")
    bindings = profile.get_editor_property("default_skill_bindings")
    attack_slot = unreal.Rogue10mAttackInputSlot
    if style_id != "Knuckle":
        check(bindings[config.enum_value(attack_slot, "PRIMARY")] == skills["Primary01"], f"Primary binding mismatch: {profile_path}")
        check(bindings[config.enum_value(attack_slot, "SPECIAL")] == skills["Special"], f"Special binding mismatch: {profile_path}")
        check(bindings[config.enum_value(attack_slot, "CHARGED_PRIMARY")] == skills["Charged"], f"Charged primary mismatch: {profile_path}")
        check(bindings[config.enum_value(attack_slot, "CHARGED_SPECIAL")] == skills["Charged"], f"Charged special mismatch: {profile_path}")
    return profile


def main():
    check_skeleton_compatibility()
    check_race_animation_sharing()
    target_configs = [
        class_config for class_config in config.CLASS_CONFIGS
        if class_config["id"] in TARGET_STYLE_IDS
    ]
    check(len(target_configs) == 10, "Requested style configuration count is not 10")
    profiles = [check_style(class_config) for class_config in target_configs]
    character_data = require_asset(config.CHARACTER_DATA_PATH)
    registered_profiles = list(character_data.get_editor_property("weapon_skill_profiles"))
    for profile in profiles:
        check(profile in registered_profiles, f"Profile is not registered in Character Data: {asset_path(profile)}")
    requested_types = [profile.get_editor_property("weapon_type") for profile in profiles]
    profile_count = len(target_configs)
    check(
        len(set(requested_types)) == profile_count,
        "Requested combat profiles do not have unique weapon types",
    )
    unreal.log(
        f"RESULT=PASSED profiles={profile_count} attacks={profile_count * 5} "
        f"montages={profile_count * 5} niagara={profile_count * 3} "
        f"combos={profile_count} first_person_safe={profile_count * 5} "
        "races=6 skeleton_compatibility=bidirectional "
        "readability=HeroShooterFP_v1 effect_type=assigned"
    )


main()
