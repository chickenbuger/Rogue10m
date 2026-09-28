"""Apply readable hero-shooter combat VFX budgets to the requested ten styles."""

from __future__ import annotations

import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ClassCombatVFXReadabilityProfile as readability
import ConfigureClassCombatAnimationVFX as config


def create_or_load_effect_type():
    path = readability.EFFECT_TYPE_PATH
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        effect_type = unreal.EditorAssetLibrary.load_asset(path)
    else:
        package_path, asset_name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(package_path)
        factory = unreal.NiagaraEffectTypeFactoryNew()
        effect_type = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, package_path, unreal.NiagaraEffectType, factory
        )
    if not isinstance(effect_type, unreal.NiagaraEffectType):
        raise RuntimeError(f"Niagara Effect Type creation failed: {path}")
    effect_type.set_editor_property("allow_culling_for_local_players", False)
    effect_type.set_editor_property(
        "update_frequency", unreal.NiagaraScalabilityUpdateFrequency.SPAWN_ONLY
    )
    effect_type.set_editor_property(
        "cull_reaction", unreal.NiagaraCullReaction.DEACTIVATE_IMMEDIATE
    )
    unreal.EditorAssetLibrary.save_loaded_asset(effect_type, only_if_is_dirty=False)
    return effect_type


def apply_skill_tuning(style_id: str, variant_name: str):
    path = f"{config.ATTACK_ROOT}/{style_id}/DA_Attack_{style_id}_{variant_name}"
    skill = config.require_asset(path)
    tuning = readability.get_skill_tuning(style_id, variant_name)
    properties = {
        key: unreal.Vector(*value) if key == "first_person_effect_offset" else value
        for key, value in tuning.items()
    }
    properties["use_first_person_effect_overrides"] = True
    properties["draw_debug_attack"] = False
    skill.set_editor_properties(properties)
    unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)
    return skill


def assign_effect_type(style_id: str, effect_type):
    effects = []
    for role in ("Cast", "Charge", "Impact"):
        path = f"{config.VFX_ROOT}/{style_id}/NS_{style_id}_{role}"
        effect = config.require_asset(path)
        if not isinstance(effect, unreal.NiagaraSystem):
            raise RuntimeError(f"Not a Niagara System: {path}")
        effect.set_editor_property("effect_type", effect_type)
        unreal.EditorAssetLibrary.set_metadata_tag(
            effect, "Rogue10mVFXReadabilityProfile", "HeroShooterFP_v1"
        )
        unreal.EditorAssetLibrary.save_loaded_asset(effect, only_if_is_dirty=False)
        effects.append(effect)
    return effects


def main():
    effect_type = create_or_load_effect_type()
    skill_count = 0
    niagara_count = 0
    for style_id in readability.TARGET_STYLE_IDS:
        for variant in config.VARIANTS:
            apply_skill_tuning(style_id, variant[0])
            skill_count += 1
        niagara_count += len(assign_effect_type(style_id, effect_type))
    unreal.log(
        "RESULT=READABLE_CLASS_COMBAT_VFX_APPLIED "
        f"styles={len(readability.TARGET_STYLE_IDS)} skills={skill_count} "
        f"niagara={niagara_count} effect_type={readability.EFFECT_TYPE_PATH} "
        "profile=HeroShooterFP_v1"
    )


if __name__ == "__main__":
    main()
