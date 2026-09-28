"""Apply the visually reviewed first-person VFX budget to requested combat styles."""

from __future__ import annotations

import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ConfigureClassCombatAnimationVFX as config


TARGET_STYLE_IDS = {
    "Dagger", "Shuriken", "DualDaggers", "LongSword", "GreatSword",
    "DualBlades", "Shield", "SwordBuckler", "Staff", "Knuckle",
}


def main():
    tuned_count = 0
    for class_config in config.CLASS_CONFIGS:
        style_id = class_config["id"]
        if style_id not in TARGET_STYLE_IDS:
            continue
        for variant in config.VARIANTS:
            variant_name = variant[0]
            path = f"{config.ATTACK_ROOT}/{style_id}/DA_Attack_{style_id}_{variant_name}"
            skill = config.require_asset(path)
            skill.set_editor_properties(
                {
                    "first_person_effect_offset": unreal.Vector(*class_config["fp_offset"]),
                    "first_person_charge_scale_multiplier": 0.12,
                }
            )
            unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=True)
            tuned_count += 1
    if tuned_count != 50:
        raise RuntimeError(f"Expected 50 requested attack skills, tuned {tuned_count}")
    unreal.log(
        "RESULT=FIRST_PERSON_CLASS_COMBAT_VFX_TUNED "
        "styles=10 attacks=50 charge_multiplier=0.12 dual_offsets=reviewed"
    )


main()
