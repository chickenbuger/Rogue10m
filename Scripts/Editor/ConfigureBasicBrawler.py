"""Configure Unarmed left/right jabs, tap right straight and charged right hook in UE 5.8.

Only four dedicated BasicBrawler skill assets and the Unarmed profile are saved.
Existing uppercut, Knuckle/StoneFist assets, original attack data, and shared montages are read-only.
"""
from __future__ import annotations

import unreal

ROOT = "/Game/DataAsset/AttackSkill/BasicBrawler"
PROFILE = "/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed"
CHARACTER = "/Game/DataAsset/Character/DA_Character_Default"
SOURCE_ROOT = "/Game/DataAsset/AttackSkill/Unarmed"
MONTAGE_ROOT = "/Game/Rogue10m/Animation/Common"
DEFINITIONS = {
    "LeftJab": ("DA_Attack_Unarmed_Primary", "AM_Punch_01", "왼손 잽", "PRIMARY"),
    "RightJab": ("DA_Attack_Unarmed_Primary_Combo2", "AM_Punch_02", "오른손 잽", "PRIMARY"),
    "RightStraight": ("DA_Attack_Unarmed_Special", "AM_Punch_02", "오른손 스트레이트", "SPECIAL"),
    "RightHook": ("DA_Attack_Unarmed_ChargedSpecial", "AM_Punch_03", "차지 오른손 훅", "CHARGED_SPECIAL"),
}
PLAY_RATES = {"LeftJab": 1.5, "RightJab": 1.5, "RightStraight": 1.0, "RightHook": 1.5}
BALANCE_FIELDS = (
    "damage", "attack_cooldown", "resource_costs", "attack_range",
    "min_damage_ratio_multiplier", "max_damage_ratio_multiplier", "charge_seconds",
    "critical_chance_bonus", "critical_damage_multiplier_bonus", "attack_shape", "hit_mode",
    "hit_count", "hit_interval", "max_targets_per_hit", "max_hits_per_target",
    "attack_trace_radius", "box_half_width", "box_half_height", "arc_angle_degrees", "circle_forward_offset",
)


def hit_fraction(name):
    return 0.38 if name == "RightHook" else 0.34


def require(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value:
        raise RuntimeError(f"Missing required asset: {path}")
    return value


def slot(name):
    return getattr(unreal.Rogue10mAttackInputSlot, name)


def path_for(name):
    return f"{ROOT}/DA_BasicBrawler_{name}"


def unique_assets(values):
    result = []
    seen = set()
    for value in values:
        if value and value.get_path_name() not in seen:
            result.append(value)
            seen.add(value.get_path_name())
    return result


def validate():
    profile = require(PROFILE)
    character = require(CHARACTER)
    if character.get_editor_property("default_weapon_type") != unreal.Rogue10mWeaponType.UNARMED:
        raise RuntimeError("Default character must spawn Unarmed.")
    skills = {name: require(path_for(name)) for name in DEFINITIONS}
    bindings = dict(profile.get_editor_property("default_skill_bindings"))
    expected = {
        slot("PRIMARY"): skills["LeftJab"],
        slot("SPECIAL"): skills["RightStraight"],
        slot("CHARGED_SPECIAL"): skills["RightHook"],
    }
    for key, value in expected.items():
        if bindings.get(key) != value:
            raise RuntimeError(f"Wrong BasicBrawler binding: {key}")
    if slot("CHARGED_PRIMARY") in bindings:
        raise RuntimeError("LMB must not expose the old hold/charged-primary binding.")
    if skills["LeftJab"].get_editor_property("next_combo_skill") != skills["RightJab"]:
        raise RuntimeError("Left jab must lead only to right jab.")
    for name in ("RightJab", "RightStraight", "RightHook"):
        if skills[name].get_editor_property("enable_combo") or skills[name].get_editor_property("next_combo_skill"):
            raise RuntimeError(f"Only the left jab may open a combo: {name}")
    unlocked = list(profile.get_editor_property("initially_unlocked_skills"))
    for skill in skills.values():
        if skill not in unlocked:
            raise RuntimeError(f"Basic attack must be initially unlocked: {skill.get_path_name()}")
    for name, (source_name, montage_name, _, _) in DEFINITIONS.items():
        skill = skills[name]
        source = require(f"{SOURCE_ROOT}/{source_name}")
        expected_rate = PLAY_RATES[name]
        if abs(skill.get_editor_property("animation_play_rate") - expected_rate) > 0.001:
            raise RuntimeError(f"Wrong basic attack playback tempo: {name}")
        expected_delay = skill.get_editor_property("attack_montage").get_play_length() * hit_fraction(name)
        if abs(skill.get_editor_property("hit_start_delay_seconds") - expected_delay) > 0.001:
            raise RuntimeError(f"Hit must match the basic fist pose peak: {name}")
        if skill.get_editor_property("draw_debug_attack"):
            raise RuntimeError(f"Basic attack debug shapes must be disabled: {name}")
        if skill.get_editor_property("attack_montage") != require(f"{MONTAGE_ROOT}/{montage_name}"):
            raise RuntimeError(f"Wrong existing montage: {name}")
        for field in BALANCE_FIELDS:
            if skill.get_editor_property(field) != source.get_editor_property(field):
                raise RuntimeError(f"Existing balance must remain unchanged: {name}/{field}")
    if skills["RightHook"].get_editor_property("charge_seconds") <= 0.0:
        raise RuntimeError("Charge duration must be positive.")
    unreal.log("RESULT=BASIC_BRAWLER_ASSETS_PASSED skills=4 chain=left_jab_right_jab rmb=tap_straight_or_charged_hook")


def main():
    profile = require(PROFILE)
    require(CHARACTER)
    skills = {}
    for name, (source_name, montage_name, label, input_name) in DEFINITIONS.items():
        target = path_for(name)
        skill = unreal.EditorAssetLibrary.load_asset(target)
        if not skill:
            skill = unreal.EditorAssetLibrary.duplicate_asset(f"{SOURCE_ROOT}/{source_name}", target)
        if not skill:
            raise RuntimeError(f"Could not create {target}")
        # Refresh the dedicated skill from its matching source, including an older
        # RightStraight asset that previously served as the LMB combo finisher.
        source = require(f"{SOURCE_ROOT}/{source_name}")
        skill.set_editor_properties({field: source.get_editor_property(field) for field in BALANCE_FIELDS})
        skill.set_editor_properties({
            "skill_name": label,
            "icon_label": "잽" if name in ("LeftJab", "RightJab") else ("훅" if name == "RightHook" else "직"),
            "effect_attach_socket": "hand_l" if name == "LeftJab" else "hand_r",
            "skill_description": "기본 직업 전용 왼손-오른손 2연타" if name in ("LeftJab", "RightJab")
                else "우클릭을 짧게 눌렀다 놓으면 오른손 스트레이트, 충분히 모았다 놓으면 오른손 훅.",
            "input_slot": slot(input_name),
            "attack_montage": require(f"{MONTAGE_ROOT}/{montage_name}"),
            "animation_play_rate": PLAY_RATES[name],
            "enable_combo": name == "LeftJab",
            "draw_debug_attack": False,
            # Match the native pose's normalized extension/apex to each existing montage.
            "hit_start_delay_seconds": require(f"{MONTAGE_ROOT}/{montage_name}").get_play_length()
                * hit_fraction(name),
            "next_combo_skill": None,
            "prerequisite_skills": [],
            "unlock_conditions": [],
            "gameplay_ability_class": unreal.load_class(None, "/Script/Rogue10m.Rogue10mGameplayAbility_Attack"),
        })
        skills[name] = skill
    skills["LeftJab"].set_editor_properties({
        "next_combo_skill": skills["RightJab"],
        "combo_window_open_seconds": 0.40,
        "combo_window_close_seconds": 0.55,
        "combo_input_slot": slot("PRIMARY"),
    })
    bindings = dict(profile.get_editor_property("default_skill_bindings"))
    bindings[slot("PRIMARY")] = skills["LeftJab"]
    bindings[slot("SPECIAL")] = skills["RightStraight"]
    bindings[slot("CHARGED_SPECIAL")] = skills["RightHook"]
    bindings.pop(slot("CHARGED_PRIMARY"), None)
    profile.set_editor_property("default_skill_bindings", bindings)
    profile.set_editor_property("initially_unlocked_skills", unique_assets(
        list(profile.get_editor_property("initially_unlocked_skills")) + list(skills.values())))
    profile.set_editor_property("skill_tree_skills", unique_assets(
        list(profile.get_editor_property("skill_tree_skills")) + list(skills.values())))
    for asset in list(skills.values()) + [profile]:
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Save failed: {asset.get_path_name()}")
    validate()


if __name__ == "__main__":
    main()
