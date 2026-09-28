"""Create and connect the shared player animation/VFX assets through Unreal Editor APIs."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import unreal


ANIMATION_ROOT = "/Game/Rogue10m/Animation/Common"
VFX_ROOT = "/Game/Rogue10m/VFX/Character"
MOVEMENT_VFX_ROOT = f"{VFX_ROOT}/Movement"
ATTACK_VFX_ROOT = f"{VFX_ROOT}/Combat/Unarmed"
MOTION_DATA_ROOT = "/Game/DataAsset/Character/Animation"
MOTION_DATA_PATH = f"{MOTION_DATA_ROOT}/DA_CommonCharacterMotion"
COMMON_ANIM_BP_PATH = f"{ANIMATION_ROOT}/ABP_Common_Unarmed"
SOURCE_ANIM_BP_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
MOTION_DUST_CASCADE_SOURCE = "/Game/Rogue10m/VFX/Character/Source/P_MotionDustSource"
MOTION_DUST_NIAGARA_SOURCE = "/Game/Rogue10m/VFX/Character/Source/NS_MotionDustPoof_Source"
CASCADE_CONVERTER_PYTHON = Path(
    unreal.Paths.engine_plugins_dir()
) / "FX" / "CascadeToNiagaraConverter" / "Content" / "Python"

ANIMATION_SOURCES = {
    "AM_Dodge_Roll": "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash",
    "AM_Punch_01": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01",
    "AM_Punch_02": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02",
    "AM_Punch_03": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03",
    "AM_Punch_Charged": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack",
}

NIAGARA_SOURCES = {
    "NS_Motion_WalkDust": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Motion_RunDust": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Motion_JumpBurst": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Motion_DoubleJumpBurst": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Motion_LandBurst": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Motion_DodgeTrail": MOTION_DUST_NIAGARA_SOURCE,
    "NS_Punch_Swing_01": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_04",
    "NS_Punch_Swing_02": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_05",
    "NS_Punch_Swing_03": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_06",
    "NS_Punch_Charge": "/Game/AdvancedPortalsSystemVFX/VFX/Vortex/NS_Portal_Vortex_1",
    "NS_Punch_ChargedRelease": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_06",
    "NS_Punch_Impact": "/Game/AdvancedPortalsSystemVFX/VFX/Sparks/NS_Sparks_03_Collision",
}

MOTION_NIAGARA_NAMES = {
    "NS_Motion_WalkDust",
    "NS_Motion_RunDust",
    "NS_Motion_JumpBurst",
    "NS_Motion_DoubleJumpBurst",
    "NS_Motion_LandBurst",
    "NS_Motion_DodgeTrail",
}
SOURCE_METADATA_TAG = "Rogue10mMotionVFXSource"

ATTACK_VFX_TUNING = {
    "DA_Attack_Unarmed_Primary": (0.22, 0.10),
    "DA_Attack_Unarmed_Primary_Combo2": (0.24, 0.11),
    "DA_Attack_Unarmed_Primary_Combo3": (0.26, 0.12),
    "DA_Attack_Unarmed_Special": (0.26, 0.12),
    "DA_Attack_Unarmed_ChargedPrimary": (0.32, 0.16),
    "DA_Attack_Unarmed_ChargedSpecial": (0.32, 0.16),
    "DA_Attack_Unarmed_JumpPrimary": (0.24, 0.11),
    "DA_Attack_Unarmed_JumpSpecial": (0.32, 0.16),
}

ATTACK_ROOT = "/Game/DataAsset/AttackSkill/Unarmed"
ATTACK_SETTINGS = {
    "DA_Attack_Unarmed_Primary": ("AM_Punch_01", "NS_Punch_Swing_01", None, True, "DA_Attack_Unarmed_Primary_Combo2"),
    "DA_Attack_Unarmed_Primary_Combo2": ("AM_Punch_02", "NS_Punch_Swing_02", None, True, "DA_Attack_Unarmed_Primary_Combo3"),
    "DA_Attack_Unarmed_Primary_Combo3": ("AM_Punch_03", "NS_Punch_Swing_03", None, False, None),
    "DA_Attack_Unarmed_Special": ("AM_Punch_03", "NS_Punch_Swing_03", None, False, None),
    "DA_Attack_Unarmed_ChargedPrimary": ("AM_Punch_Charged", "NS_Punch_ChargedRelease", "NS_Punch_Charge", False, None),
    "DA_Attack_Unarmed_ChargedSpecial": ("AM_Punch_Charged", "NS_Punch_ChargedRelease", "NS_Punch_Charge", False, None),
    "DA_Attack_Unarmed_JumpPrimary": ("AM_Punch_02", "NS_Punch_Swing_02", None, False, None),
    "DA_Attack_Unarmed_JumpSpecial": ("AM_Punch_Charged", "NS_Punch_ChargedRelease", None, False, None),
}

CHARACTER_BLUEPRINTS = (
    "/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_HumanFemaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_DwarfMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_DwarfFemaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_OrcMaleCharacter",
    "/Game/Character/Customization/Characters/BP_Rogue10m_OrcFemaleCharacter",
)


def require_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


def duplicate_if_missing(source_path: str, target_path: str):
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        return require_asset(target_path)
    require_asset(source_path)
    asset = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not asset:
        raise RuntimeError(f"Asset duplication failed: {source_path} -> {target_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def duplicate_motion_effect(asset_name: str, source_path: str):
    target_path = f"{MOVEMENT_VFX_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        existing = require_asset(target_path)
        current_source = unreal.EditorAssetLibrary.get_metadata_tag(
            existing, SOURCE_METADATA_TAG
        )
        if current_source == source_path:
            return existing
        if not unreal.EditorAssetLibrary.delete_asset(target_path):
            raise RuntimeError(f"Old motion Niagara deletion failed: {target_path}")

    effect = duplicate_if_missing(source_path, target_path)
    unreal.EditorAssetLibrary.set_metadata_tag(
        effect, SOURCE_METADATA_TAG, source_path
    )
    unreal.EditorAssetLibrary.save_loaded_asset(effect, only_if_is_dirty=False)
    return effect


def effect_target_path(asset_name: str) -> str:
    root = MOVEMENT_VFX_ROOT if asset_name in MOTION_NIAGARA_NAMES else ATTACK_VFX_ROOT
    return f"{root}/{asset_name}"


def organize_existing_effects():
    for asset_name in NIAGARA_SOURCES:
        legacy_path = f"{VFX_ROOT}/{asset_name}"
        target_path = effect_target_path(asset_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(legacy_path):
            continue
        if unreal.EditorAssetLibrary.does_asset_exist(target_path):
            continue
        if not unreal.EditorAssetLibrary.rename_asset(legacy_path, target_path):
            raise RuntimeError(f"Niagara organization failed: {legacy_path} -> {target_path}")

def fixup_effect_redirectors():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    redirectors = []
    for asset_data in registry.get_assets_by_path(
        unreal.Name(VFX_ROOT), recursive=True
    ):
        if str(asset_data.asset_class_path.asset_name) == "ObjectRedirector":
            redirector = asset_data.get_asset()
            if redirector:
                redirectors.append(redirector)
    if redirectors:
        unreal.AssetToolsHelpers.get_asset_tools().fixup_referencers(redirectors)

def remove_unreferenced_legacy_effects():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions()
    for asset_name in NIAGARA_SOURCES:
        legacy_path = f"{VFX_ROOT}/{asset_name}"
        target_path = effect_target_path(asset_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(legacy_path):
            continue
        legacy_data = unreal.EditorAssetLibrary.find_asset_data(legacy_path)
        if str(legacy_data.asset_class_path.asset_name) == "ObjectRedirector":
            continue
        target = require_asset(target_path)
        legacy = require_asset(legacy_path)
        if not isinstance(target, unreal.NiagaraSystem) or not isinstance(legacy, unreal.NiagaraSystem):
            raise RuntimeError(f"Unexpected legacy VFX asset type: {legacy_path}")
        referencers = registry.get_referencers(legacy_path, options)
        if referencers:
            raise RuntimeError(
                f"Legacy VFX still has referencers: {legacy_path} -> "
                + ", ".join(str(item) for item in referencers)
            )
        editor_asset_subsystem = unreal.get_editor_subsystem(
            unreal.EditorAssetSubsystem
        )
        if not editor_asset_subsystem.delete_loaded_assets([legacy]):
            raise RuntimeError(f"Legacy VFX deletion failed: {legacy_path}")

def ensure_motion_dust_niagara_source():
    if unreal.EditorAssetLibrary.does_asset_exist(MOTION_DUST_NIAGARA_SOURCE):
        return require_asset(MOTION_DUST_NIAGARA_SOURCE)

    source_directory = MOTION_DUST_CASCADE_SOURCE.rsplit("/", 1)[0]
    unreal.EditorAssetLibrary.make_directory(source_directory)
    if not unreal.EditorAssetLibrary.does_asset_exist(MOTION_DUST_CASCADE_SOURCE):
        duplicate_if_missing(
            "/Game/StarterContent/Particles/P_Smoke",
            MOTION_DUST_CASCADE_SOURCE,
        )

    converter_path = str(CASCADE_CONVERTER_PYTHON)
    if converter_path not in sys.path:
        sys.path.insert(0, converter_path)
    import CascadeToNiagaraConverter

    cascade_source = require_asset(MOTION_DUST_CASCADE_SOURCE)
    conversion_results = unreal.ConvertCascadeToNiagaraResults()
    CascadeToNiagaraConverter.convert_cascade_to_niagara(
        cascade_source, conversion_results
    )
    converted_path = f"{MOTION_DUST_CASCADE_SOURCE}_Converted"
    if not unreal.EditorAssetLibrary.does_asset_exist(converted_path):
        raise RuntimeError(f"Converted Niagara source is missing: {converted_path}")
    if not unreal.EditorAssetLibrary.rename_asset(
        converted_path, MOTION_DUST_NIAGARA_SOURCE
    ):
        raise RuntimeError(
            f"Converted Niagara source rename failed: {converted_path}"
        )
    source = require_asset(MOTION_DUST_NIAGARA_SOURCE)
    unreal.EditorAssetLibrary.set_metadata_tag(
        source, SOURCE_METADATA_TAG, "/Game/StarterContent/Particles/P_Smoke"
    )
    unreal.EditorAssetLibrary.save_loaded_asset(source, only_if_is_dirty=False)
    return source


def create_montage(asset_name: str, source_path: str):
    target_path = f"{ANIMATION_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        return require_asset(target_path)

    source_animation = require_asset(source_path)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", source_animation)
    skeleton = source_animation.get_editor_property("skeleton")
    if skeleton:
        factory.set_editor_property("target_skeleton", skeleton)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ANIMATION_ROOT, unreal.AnimMontage, factory
    )
    if not montage:
        raise RuntimeError(f"AnimMontage creation failed: {target_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
    return montage


def create_motion_data(montages: dict[str, object], effects: dict[str, object]):
    motion_data = None
    if unreal.EditorAssetLibrary.does_asset_exist(MOTION_DATA_PATH):
        motion_data = require_asset(MOTION_DATA_PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property(
            "data_asset_class", unreal.Rogue10mCharacterMotionDataAsset
        )
        motion_data = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_CommonCharacterMotion",
            MOTION_DATA_ROOT,
            unreal.Rogue10mCharacterMotionDataAsset,
            factory,
        )
    if not motion_data:
        raise RuntimeError(f"Motion Data Asset creation failed: {MOTION_DATA_PATH}")

    motion_data.set_editor_properties(
        {
            "motion_set_id": "CommonUnarmed",
            "dodge_montage": montages["AM_Dodge_Roll"],
            "enable_motion_effects": False,
            "walk_footstep_effect": effects["NS_Motion_WalkDust"],
            "run_footstep_effect": effects["NS_Motion_RunDust"],
            "jump_effect": effects["NS_Motion_JumpBurst"],
            "double_jump_effect": effects["NS_Motion_DoubleJumpBurst"],
            "land_effect": effects["NS_Motion_LandBurst"],
            "dodge_effect": effects["NS_Motion_DodgeTrail"],
            "walk_footstep_interval": 0.68,
            "run_footstep_interval": 0.38,
            "minimum_footstep_speed": 120.0,
            "footstep_lateral_offset": 8.0,
            "footstep_rear_offset": 10.0,
            "walk_effect_scale": 0.12,
            "run_effect_scale": 0.16,
            "jump_effect_scale": 0.18,
            "double_jump_effect_scale": 0.20,
            "land_effect_scale": 0.20,
            "dodge_effect_scale": 0.16,
            "walk_effect_emission_duration": 0.08,
            "run_effect_emission_duration": 0.10,
            "jump_effect_emission_duration": 0.12,
            "double_jump_effect_emission_duration": 0.14,
            "land_effect_emission_duration": 0.14,
            "dodge_effect_emission_duration": 0.12,
            "motion_effect_time_dilation": 3.0,
            "ground_effect_offset": unreal.Vector(0.0, 0.0, 2.0),
        }
    )
    unreal.EditorAssetLibrary.save_loaded_asset(motion_data, only_if_is_dirty=False)
    return motion_data


def configure_attack_assets(montages: dict[str, object], effects: dict[str, object]):
    impact = effects["NS_Punch_Impact"]
    for asset_name, setting in ATTACK_SETTINGS.items():
        montage_name, cast_name, charge_name, enable_combo, next_name = setting
        cast_scale, cast_duration = ATTACK_VFX_TUNING[asset_name]
        skill = require_asset(f"{ATTACK_ROOT}/{asset_name}")
        next_skill = require_asset(f"{ATTACK_ROOT}/{next_name}") if next_name else None
        skill.set_editor_properties(
            {
                "attack_montage": montages[montage_name],
                "enable_attack_effects": False,
                "cast_effect": effects[cast_name],
                "charge_effect": effects[charge_name] if charge_name else None,
                "impact_effect": impact,
                "effect_attach_socket": "hand_r",
                "cast_effect_scale": cast_scale,
                "charge_effect_scale": 0.18,
                "impact_effect_scale": 0.28,
                "cast_effect_emission_duration": cast_duration,
                "impact_effect_emission_duration": 0.14,
                "attack_effect_time_dilation": 2.0,
                "enable_combo": enable_combo,
                "combo_window_open_seconds": 0.15,
                "combo_window_close_seconds": 0.55,
                "combo_input_slot": unreal.Rogue10mAttackInputSlot.PRIMARY,
                "next_combo_skill": next_skill,
            }
        )
        unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)


def configure_common_anim_blueprint():
    common_bp = duplicate_if_missing(SOURCE_ANIM_BP_PATH, COMMON_ANIM_BP_PATH)
    # Keep locomotion ground IK before the attack slot when this generator is rerun.
    # Importing the helper does not invoke its standalone editor-exit entry point.
    helper_path = Path(unreal.Paths.project_dir()) / "Scripts/Editor/ConfigureBrawlerSourceFootIKOrder.py"
    spec = importlib.util.spec_from_file_location("rogue10m_source_foot_ik_order", helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    helper.main(apply=True)
    common_class = common_bp.generated_class()
    if not common_class:
        raise RuntimeError(f"Common AnimBP generated class is missing: {COMMON_ANIM_BP_PATH}")

    for blueprint_path in CHARACTER_BLUEPRINTS:
        blueprint = require_asset(blueprint_path)
        generated_class = blueprint.generated_class()
        if not generated_class:
            raise RuntimeError(f"Character Blueprint class is missing: {blueprint_path}")
        cdo = unreal.get_default_object(generated_class)
        cdo.set_editor_property("animation_source_anim_class", common_class)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(common_bp, only_if_is_dirty=False)
    return common_bp


def configure_jump_profiles():
    unarmed = require_asset("/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed")
    stone_fist = require_asset("/Game/DataAsset/SkillProfile/DA_SkillProfile_StoneFist")
    unarmed.set_editor_property("max_jump_count", 1)
    stone_fist.set_editor_property("max_jump_count", 2)
    unreal.EditorAssetLibrary.save_loaded_asset(unarmed, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(stone_fist, only_if_is_dirty=False)


def main():
    for directory in (
        ANIMATION_ROOT, VFX_ROOT, MOVEMENT_VFX_ROOT, ATTACK_VFX_ROOT, MOTION_DATA_ROOT
    ):
        unreal.EditorAssetLibrary.make_directory(directory)

    ensure_motion_dust_niagara_source()
    organize_existing_effects()

    montages = {
        name: create_montage(name, source)
        for name, source in ANIMATION_SOURCES.items()
    }
    effects = {
        name: (
            duplicate_motion_effect(name, source)
            if name in MOTION_NIAGARA_NAMES
            else duplicate_if_missing(source, effect_target_path(name))
        )
        for name, source in NIAGARA_SOURCES.items()
    }
    create_motion_data(montages, effects)
    configure_attack_assets(montages, effects)
    configure_common_anim_blueprint()
    configure_jump_profiles()
    remove_unreferenced_legacy_effects()
    fixup_effect_redirectors()
    unreal.log("RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED")


if __name__ == "__main__":
    main()
