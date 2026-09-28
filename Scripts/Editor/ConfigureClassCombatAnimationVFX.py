"""Create eleven weapon-combat animation, skill-profile, and first-person-safe VFX sets."""

from __future__ import annotations

import unreal

import ClassCombatVFXReadabilityProfile as readability

ANIMATION_ROOT = "/Game/Rogue10m/Animation/Combat"
VFX_ROOT = "/Game/Rogue10m/VFX/Character/Combat"
ATTACK_ROOT = "/Game/DataAsset/AttackSkill/ClassCombat"
PROFILE_ROOT = "/Game/DataAsset/SkillProfile"
CHARACTER_DATA_PATH = "/Game/DataAsset/Character/DA_Character_Default"
DEFAULT_DODGE_PATH = "/Game/DataAsset/DodgeSkill/DA_Dodge_Unarmed"
FIST_DODGE_PATH = "/Game/DataAsset/DodgeSkill/DA_Dodge_StoneFist"
MAIN_SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
MAGIC_SKELETON_PATH = "/Game/CombatMagicAnims/Demo/Mannequins/Meshes/SK_Mannequin"
SOURCE_ANIMATION_TAG = "Rogue10mCombatSourceAnimation"
SOURCE_VFX_TAG = "Rogue10mCombatVFXSource"

UNARMED = "/Game/Characters/Mannequins/Anims/Unarmed/Attack"
PISTOL = "/Game/Characters/Mannequins/Anims/Pistol"
RIFLE = "/Game/Characters/Mannequins/Anims/Rifle"
MAGIC = "/Game/CombatMagicAnims/Animations"


CLASS_CONFIGS = (
    {
        "id": "Dagger",
        "display": "단검 도적",
        "weapon": "DAGGER",
        "sources": (
            f"{UNARMED}/MM_Attack_01", f"{UNARMED}/MM_Attack_02",
            f"{UNARMED}/MM_Attack_03", f"{UNARMED}/MM_Attack_02",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.42, 1.48, 1.38, 1.34, 1.08),
        "shape": "LINEAR_BOX", "special_shape": "ARC", "charged_shape": "LINEAR_BOX",
        "hit_mode": "SINGLE", "range": 155.0, "width": 34.0, "arc": 72.0,
        "damage": 10.0, "dual": False, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_1",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (0.18, 0.95, 0.82, 1.0), "fp_offset": (0.0, 13.0, -10.0),
    },
    {
        "id": "Shuriken",
        "display": "표창 도적",
        "weapon": "SHURIKEN",
        "sources": (f"{PISTOL}/MM_Pistol_Fire",) * 5,
        "play_rates": (1.35, 1.48, 1.58, 1.25, 0.92),
        "shape": "PROJECTILE", "special_shape": "PROJECTILE", "charged_shape": "PROJECTILE",
        "hit_mode": "SINGLE", "range": 1050.0, "width": 20.0, "arc": 30.0,
        "damage": 9.0, "dual": False, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_05",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_3",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_02_Collision",
        "tint": (0.28, 0.74, 1.0, 1.0), "fp_offset": (4.0, 15.0, -11.0),
    },
    {
        "id": "Bow",
        "display": "활 도적",
        "weapon": "BOW",
        "sources": (f"{RIFLE}/MM_Rifle_Fire",) * 5,
        "play_rates": (1.02, 1.08, 1.16, 0.92, 0.72),
        "shape": "PROJECTILE", "special_shape": "PROJECTILE", "charged_shape": "PROJECTILE",
        "hit_mode": "SINGLE", "range": 1350.0, "width": 24.0, "arc": 30.0,
        "damage": 12.0, "dual": False, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_7",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (0.42, 0.9, 0.34, 1.0), "fp_offset": (0.0, 15.0, -12.0),
    },
    {
        "id": "DualDaggers",
        "display": "쌍단검 도적",
        "weapon": "DUAL_DAGGERS",
        "sources": (
            f"{UNARMED}/MM_Attack_02", f"{UNARMED}/MM_Attack_01",
            f"{UNARMED}/MM_Attack_03", f"{UNARMED}/MM_Attack_02",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.55, 1.62, 1.48, 1.46, 1.14),
        "shape": "ARC", "special_shape": "ARC", "charged_shape": "LINEAR_BOX",
        "hit_mode": "MULTI_HIT", "range": 145.0, "width": 38.0, "arc": 92.0,
        "damage": 8.0, "dual": True, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_06",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_2",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (0.12, 0.9, 0.72, 1.0), "fp_offset": (0.0, 22.0, -18.0),
    },
    {
        "id": "LongSword",
        "display": "장검 전사",
        "weapon": "LONG_SWORD",
        "sources": (
            f"{UNARMED}/MM_Attack_03", f"{UNARMED}/MM_Attack_01",
            f"{UNARMED}/MM_Attack_02", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.08, 1.12, 1.02, 0.96, 0.82),
        "shape": "ARC", "special_shape": "LINEAR_BOX", "charged_shape": "ARC",
        "hit_mode": "SINGLE", "range": 235.0, "width": 54.0, "arc": 105.0,
        "damage": 15.0, "dual": False, "socket": "hand_r", "jump_count": 1,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_4",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
        "tint": (0.64, 0.82, 1.0, 1.0), "fp_offset": (0.0, 15.0, -12.0),
    },
    {
        "id": "GreatSword",
        "display": "대검 전사",
        "weapon": "GREAT_SWORD",
        "sources": (
            f"{UNARMED}/MM_ChargedAttack", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_ChargedAttack", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (0.78, 0.84, 0.72, 0.76, 0.62),
        "shape": "ARC", "special_shape": "CIRCLE", "charged_shape": "ARC",
        "hit_mode": "SINGLE", "range": 285.0, "width": 82.0, "arc": 138.0,
        "damage": 23.0, "dual": False, "socket": "hand_r", "jump_count": 1,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_06",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_6",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (1.0, 0.46, 0.12, 1.0), "fp_offset": (0.0, 18.0, -15.0),
    },
    {
        "id": "DualBlades",
        "display": "쌍검 전사",
        "weapon": "DUAL_BLADES",
        "sources": (
            f"{UNARMED}/MM_Attack_02", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_Attack_01", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.28, 1.34, 1.22, 1.18, 0.94),
        "shape": "ARC", "special_shape": "ARC", "charged_shape": "ARC",
        "hit_mode": "MULTI_HIT", "range": 215.0, "width": 56.0, "arc": 120.0,
        "damage": 12.0, "dual": True, "socket": "hand_r", "jump_count": 1,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_05",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_5",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (0.78, 0.9, 1.0, 1.0), "fp_offset": (0.0, 24.0, -18.0),
    },
    {
        "id": "Shield",
        "display": "방패 전사",
        "weapon": "SHIELD",
        "sources": (
            "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash",
            f"{UNARMED}/MM_Attack_01", f"{UNARMED}/MM_Attack_03",
            "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.0, 1.04, 0.94, 0.86, 0.72),
        "shape": "LINEAR_BOX", "special_shape": "CIRCLE", "charged_shape": "LINEAR_BOX",
        "hit_mode": "SINGLE", "range": 165.0, "width": 76.0, "arc": 90.0,
        "damage": 13.0, "dual": False, "socket": "hand_l", "jump_count": 1,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_05",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_8",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (1.0, 0.72, 0.2, 1.0), "fp_offset": (0.0, -16.0, -13.0),
    },
    {
        "id": "SwordBuckler",
        "display": "한손검 작은방패 전사",
        "weapon": "SWORD_BUCKLER",
        "sources": (
            f"{UNARMED}/MM_Attack_01", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_Attack_02", "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.12, 1.16, 1.05, 0.96, 0.82),
        "shape": "ARC", "special_shape": "LINEAR_BOX", "charged_shape": "ARC",
        "hit_mode": "SINGLE", "range": 205.0, "width": 52.0, "arc": 98.0,
        "damage": 14.0, "dual": True, "socket": "hand_r", "jump_count": 1,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_7",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
        "tint": (0.62, 0.82, 1.0, 1.0), "fp_offset": (0.0, 14.0, -12.0),
    },
    {
        "id": "Staff",
        "display": "마법사",
        "weapon": "STAFF",
        "sources": (
            f"{MAGIC}/AS_StaffStrike", f"{MAGIC}/AS_StaffShot",
            f"{MAGIC}/AS_MagicStrike2", f"{MAGIC}/AS_ManaCastShot",
            f"{MAGIC}/AS_SpellAndCastFireball",
        ),
        "play_rates": (1.08, 1.06, 1.0, 0.94, 0.78),
        "shape": "PROJECTILE", "special_shape": "CIRCLE", "charged_shape": "PROJECTILE",
        "hit_mode": "SINGLE", "range": 1150.0, "width": 42.0, "arc": 80.0,
        "damage": 17.0, "dual": False, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_9",
        "charge_source": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_10",
        "impact_source": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_06",
        "tint": (0.54, 0.3, 1.0, 1.0), "fp_offset": (8.0, 18.0, -16.0),
    },
    {
        "id": "Knuckle",
        "display": "권사",
        "weapon": "KNUCKLE",
        "sources": (
            f"{UNARMED}/MM_Attack_01", f"{UNARMED}/MM_Attack_02",
            f"{UNARMED}/MM_Attack_03", f"{UNARMED}/MM_Attack_03",
            f"{UNARMED}/MM_ChargedAttack",
        ),
        "play_rates": (1.34, 1.4, 1.24, 1.14, 0.9),
        "shape": "LINEAR_BOX", "special_shape": "ARC", "charged_shape": "LINEAR_BOX",
        "hit_mode": "SINGLE", "range": 165.0, "width": 44.0, "arc": 88.0,
        "damage": 12.0, "dual": True, "socket": "hand_r", "jump_count": 2,
        "cast_source": "/Game/Rogue10m/VFX/Character/Combat/Unarmed/NS_Punch_Swing_01",
        "charge_source": "/Game/Rogue10m/VFX/Character/Combat/Unarmed/NS_Punch_Charge",
        "impact_source": "/Game/Rogue10m/VFX/Character/Combat/Unarmed/NS_Punch_Impact",
        "tint": (1.0, 0.72, 0.18, 1.0), "fp_offset": (0.0, 22.0, -17.0),
    },
)

VARIANTS = (
    ("Primary01", "1타", "PRIMARY", 0.80, 0),
    ("Primary02", "2타", "PRIMARY", 0.95, 1),
    ("Primary03", "3타", "PRIMARY", 1.15, 2),
    ("Special", "특수", "SPECIAL", 1.45, 3),
    ("Charged", "차징", "CHARGED_PRIMARY", 2.10, 4),
)


def enum_value(enum_type, member_name: str):
    value = getattr(enum_type, member_name, None)
    if value is None:
        raise RuntimeError(f"Enum member missing: {enum_type}.{member_name}")
    return value


def require_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset missing: {path}")
    return asset


def create_or_load_data_asset(path: str, data_asset_class):
    asset = (
        unreal.EditorAssetLibrary.load_asset(path)
        if unreal.EditorAssetLibrary.does_asset_exist(path)
        else None
    )
    if asset:
        return asset
    package_path, asset_name = path.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(package_path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", data_asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, data_asset_class, factory
    )
    if not asset:
        raise RuntimeError(f"Data Asset creation failed: {path}")
    return asset


def ensure_skeleton_compatibility():
    main_skeleton = require_asset(MAIN_SKELETON_PATH)
    magic_skeleton = require_asset(MAGIC_SKELETON_PATH)
    main_skeleton.add_compatible_skeleton(magic_skeleton)
    magic_skeleton.add_compatible_skeleton(main_skeleton)
    unreal.EditorAssetLibrary.save_loaded_asset(main_skeleton, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(magic_skeleton, only_if_is_dirty=False)


def create_montage(style_id: str, variant_name: str, source_path: str):
    directory = f"{ANIMATION_ROOT}/{style_id}"
    asset_name = f"AM_{style_id}_{variant_name}"
    target_path = f"{directory}/{asset_name}"
    montage = (
        unreal.EditorAssetLibrary.load_asset(target_path)
        if unreal.EditorAssetLibrary.does_asset_exist(target_path)
        else None
    )
    if montage:
        return montage
    source_animation = require_asset(source_path)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", source_animation)
    skeleton = source_animation.get_editor_property("skeleton")
    if skeleton:
        factory.set_editor_property("target_skeleton", skeleton)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, directory, unreal.AnimMontage, factory
    )
    if not montage:
        raise RuntimeError(f"AnimMontage creation failed: {target_path}")
    unreal.EditorAssetLibrary.set_metadata_tag(montage, SOURCE_ANIMATION_TAG, source_path)
    unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
    return montage


def duplicate_effect(style_id: str, role: str, source_path: str):
    directory = f"{VFX_ROOT}/{style_id}"
    asset_name = f"NS_{style_id}_{role}"
    target_path = f"{directory}/{asset_name}"
    effect = (
        unreal.EditorAssetLibrary.load_asset(target_path)
        if unreal.EditorAssetLibrary.does_asset_exist(target_path)
        else None
    )
    if effect:
        return effect
    require_asset(source_path)
    effect = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not isinstance(effect, unreal.NiagaraSystem):
        raise RuntimeError(f"Niagara duplication failed: {source_path} -> {target_path}")
    unreal.EditorAssetLibrary.set_metadata_tag(effect, SOURCE_VFX_TAG, source_path)
    unreal.EditorAssetLibrary.save_loaded_asset(effect, only_if_is_dirty=False)
    return effect


def make_resource_cost(resource_name: str, amount: float):
    cost = unreal.Rogue10mAttackResourceCost()
    cost.set_editor_property(
        "resource_type", enum_value(unreal.Rogue10mAttackResourceType, resource_name)
    )
    cost.set_editor_property("cost", amount)
    return cost


def configure_attack(config, variant, montage, effects, next_skill):
    variant_name, label, slot_name, damage_multiplier, source_index = variant
    style_id = config["id"]
    path = f"{ATTACK_ROOT}/{style_id}/DA_Attack_{style_id}_{variant_name}"
    skill = create_or_load_data_asset(path, unreal.Rogue10mAttackSkillData)
    charged = variant_name == "Charged"
    special = variant_name == "Special"
    combo = variant_name in ("Primary01", "Primary02")
    shape_name = config["charged_shape"] if charged else (
        config["special_shape"] if special else config["shape"]
    )
    multi_hit = config["hit_mode"] == "MULTI_HIT" and not charged
    resource_name = "MANA" if config["id"] == "Staff" else "STAMINA"
    resource_costs = []
    if special:
        resource_costs = [make_resource_cost(resource_name, 7.0)]
    elif charged:
        resource_costs = [make_resource_cost(resource_name, 12.0)]

    if readability.has_profile(style_id):
        effect_tuning = readability.get_skill_tuning(style_id, variant_name)
    else:
        cast_scale = 0.20
        if style_id in ("GreatSword", "Staff"):
            cast_scale = 0.28
        elif config["dual"]:
            cast_scale = 0.17
        effect_tuning = {
            "cast_effect_scale": cast_scale,
            "charge_effect_scale": 0.20 if style_id != "Staff" else 0.26,
            "impact_effect_scale": 0.28 if style_id != "GreatSword" else 0.34,
            "cast_effect_emission_duration": 0.09 if not charged else 0.12,
            "impact_effect_emission_duration": 0.12,
            "attack_effect_time_dilation": 2.6,
            "first_person_cast_scale_multiplier": 0.34,
            "first_person_charge_scale_multiplier": 0.12,
            "first_person_impact_scale_multiplier": 0.68,
            "first_person_emission_duration_multiplier": 0.68,
            "first_person_off_hand_effect_scale_multiplier": 0.75,
            "first_person_effect_offset": config["fp_offset"],
        }

    skill.set_editor_properties(
        {
            "skill_name": f"{config['display']} {label}",
            "skill_description": (
                f"{config['display']} 전용 {label} 모션. 공통 Manny 원본을 사용해 모든 종족이 같은 동작을 재생한다."
            ),
            "input_slot": enum_value(unreal.Rogue10mAttackInputSlot, slot_name),
            "gameplay_ability_class": unreal.load_class(
                None, "/Script/Rogue10m.Rogue10mGameplayAbility_Attack"
            ),
            "damage": config["damage"] * damage_multiplier,
            "attack_shape": enum_value(unreal.Rogue10mAttackShape, shape_name),
            "hit_mode": enum_value(
                unreal.Rogue10mAttackHitMode, "MULTI_HIT" if multi_hit else "SINGLE"
            ),
            "hit_count": 2 if multi_hit else 1,
            "hit_interval": 0.07 if multi_hit else 0.08,
            "max_hits_per_target": 2 if multi_hit else 1,
            "attack_range": config["range"] * (1.12 if charged else 1.0),
            "attack_trace_radius": config["width"] * 0.5,
            "box_half_width": config["width"],
            "box_half_height": 62.0,
            "arc_angle_degrees": config["arc"],
            "circle_forward_offset": 55.0 if shape_name == "CIRCLE" else 0.0,
            "attack_cooldown": 0.32 if combo else (0.78 if special else (1.05 if charged else 0.48)),
            "charge_seconds": 0.72 if charged else 0.65,
            "resource_costs": resource_costs,
            "enable_combo": combo,
            "combo_window_open_seconds": 0.09 if config["id"] in ("Dagger", "DualDaggers") else 0.13,
            "combo_window_close_seconds": 0.40 if config["id"] in ("Dagger", "DualDaggers") else 0.56,
            "combo_input_slot": enum_value(unreal.Rogue10mAttackInputSlot, "PRIMARY"),
            "next_combo_skill": next_skill,
            "attack_montage": montage,
            "animation_play_rate": config["play_rates"][source_index],
            "enable_attack_effects": True,
            "cast_effect": effects["cast"],
            "charge_effect": effects["charge"] if charged else None,
            "impact_effect": effects["impact"],
            "effect_attach_socket": config["socket"],
            "spawn_effect_on_off_hand": config["dual"],
            "off_hand_effect_attach_socket": "hand_l",
            "cast_effect_scale": effect_tuning["cast_effect_scale"],
            "charge_effect_scale": effect_tuning["charge_effect_scale"],
            "impact_effect_scale": effect_tuning["impact_effect_scale"],
            "cast_effect_emission_duration": effect_tuning["cast_effect_emission_duration"],
            "impact_effect_emission_duration": effect_tuning["impact_effect_emission_duration"],
            "attack_effect_time_dilation": effect_tuning["attack_effect_time_dilation"],
            "use_first_person_effect_overrides": True,
            "first_person_effect_offset": unreal.Vector(*effect_tuning["first_person_effect_offset"]),
            "first_person_effect_rotation": unreal.Rotator(0.0, 0.0, 0.0),
            "first_person_cast_scale_multiplier": effect_tuning["first_person_cast_scale_multiplier"],
            "first_person_charge_scale_multiplier": effect_tuning["first_person_charge_scale_multiplier"],
            "first_person_impact_scale_multiplier": effect_tuning["first_person_impact_scale_multiplier"],
            "first_person_emission_duration_multiplier": effect_tuning["first_person_emission_duration_multiplier"],
            "first_person_off_hand_effect_scale_multiplier": effect_tuning["first_person_off_hand_effect_scale_multiplier"],
            "icon_label": label,
            "icon_tint": unreal.LinearColor(*config["tint"]),
            "draw_debug_attack": False,
        }
    )
    unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)
    return skill


def create_style_assets(config):
    style_id = config["id"]
    for directory in (
        f"{ANIMATION_ROOT}/{style_id}",
        f"{VFX_ROOT}/{style_id}",
        f"{ATTACK_ROOT}/{style_id}",
    ):
        unreal.EditorAssetLibrary.make_directory(directory)

    effects = {
        "cast": duplicate_effect(style_id, "Cast", config["cast_source"]),
        "charge": duplicate_effect(style_id, "Charge", config["charge_source"]),
        "impact": duplicate_effect(style_id, "Impact", config["impact_source"]),
    }
    montages = {
        variant[0]: create_montage(style_id, variant[0], config["sources"][variant[4]])
        for variant in VARIANTS
    }

    primary_3 = configure_attack(config, VARIANTS[2], montages["Primary03"], effects, None)
    primary_2 = configure_attack(config, VARIANTS[1], montages["Primary02"], effects, primary_3)
    primary_1 = configure_attack(config, VARIANTS[0], montages["Primary01"], effects, primary_2)
    special = configure_attack(config, VARIANTS[3], montages["Special"], effects, None)
    charged = configure_attack(config, VARIANTS[4], montages["Charged"], effects, None)
    return {
        "primary_1": primary_1,
        "primary_2": primary_2,
        "primary_3": primary_3,
        "special": special,
        "charged": charged,
    }


def create_profile(config, skills):
    style_id = config["id"]
    profile_path = f"{PROFILE_ROOT}/DA_SkillProfile_Combat_{style_id}"
    profile = create_or_load_data_asset(
        profile_path, unreal.Rogue10mWeaponSkillProfileDataAsset
    )
    slot = unreal.Rogue10mAttackInputSlot
    bindings = {
        enum_value(slot, "PRIMARY"): skills["primary_1"],
        enum_value(slot, "SPECIAL"): skills["special"],
        enum_value(slot, "CHARGED_PRIMARY"): skills["charged"],
        enum_value(slot, "CHARGED_SPECIAL"): skills["charged"],
    }
    skill_list = [
        skills["primary_1"], skills["primary_2"], skills["primary_3"],
        skills["special"], skills["charged"],
    ]
    profile.set_editor_properties(
        {
            "profile_id": f"Combat{style_id}",
            "display_name": config["display"],
            "description": (
                "3연계 기본 공격, 단일 특수 공격, 차징 공격과 1인칭 안전 Niagara를 포함한 전투 프로필."
            ),
            "weapon_type": enum_value(unreal.Rogue10mWeaponType, config["weapon"]),
            "default_skill_bindings": bindings,
            "skill_tree_skills": skill_list,
            "initially_unlocked_skills": skill_list,
            "default_dodge_skill": require_asset(
                FIST_DODGE_PATH if style_id == "Knuckle" else DEFAULT_DODGE_PATH
            ),
            "max_jump_count": config["jump_count"],
            "extra_jump_display_name": "추가 도약" if config["jump_count"] > 1 else "",
            "extra_jump_description": (
                "공중에서 한 번 더 도약한다." if config["jump_count"] > 1 else ""
            ),
        }
    )
    unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    return profile


def register_profiles(profiles):
    character_data = require_asset(CHARACTER_DATA_PATH)
    replaced_weapon_types = {
        profile.get_editor_property("weapon_type") for profile in profiles
    }
    retained = [
        profile
        for profile in character_data.get_editor_property("weapon_skill_profiles")
        if profile and profile.get_editor_property("weapon_type") not in replaced_weapon_types
    ]
    character_data.set_editor_property("weapon_skill_profiles", retained + profiles)
    unreal.EditorAssetLibrary.save_loaded_asset(character_data, only_if_is_dirty=False)


def main():
    for directory in (ANIMATION_ROOT, VFX_ROOT, ATTACK_ROOT, PROFILE_ROOT):
        unreal.EditorAssetLibrary.make_directory(directory)
    ensure_skeleton_compatibility()
    profiles = []
    for config in CLASS_CONFIGS:
        skills = create_style_assets(config)
        profiles.append(create_profile(config, skills))
    register_profiles(profiles)
    profile_count = len(CLASS_CONFIGS)
    unreal.log(
        "RESULT=CLASS_COMBAT_PRESENTATION_CONFIGURED "
        f"profiles={profile_count} attacks={profile_count * 5} "
        f"montages={profile_count * 5} niagara={profile_count * 3} "
        f"fp_safe={profile_count}"
    )


if __name__ == "__main__":
    main()
