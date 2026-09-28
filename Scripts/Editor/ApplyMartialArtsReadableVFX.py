"""Connect StoneFist presentation assets to the readable Knuckle VFX profile."""

from __future__ import annotations

import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ClassCombatVFXReadabilityProfile as readability
import ConfigureClassCombatAnimationVFX as config


MARTIAL_ART_PRESENTATION = (
    ("Jab", "Primary01", "Primary01", False),
    ("Straight", "Primary03", "Primary03", False),
    ("ChargedShockwave", "Charged", "Charged", True),
    ("JumpSlam", "Special", "Special", True),
)


def require(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing asset: {path}")
    return asset


def apply_presentation(skill_name: str, variant_name: str, montage_name: str, dual_hand: bool):
    skill_path = f"/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_{skill_name}"
    skill = require(skill_path)
    montage = require(
        f"/Game/Rogue10m/Animation/Combat/Knuckle/AM_Knuckle_{montage_name}"
    )
    cast_effect = require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Cast"
    )
    charge_effect = require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Charge"
    )
    impact_effect = require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Impact"
    )

    tuning = readability.get_skill_tuning("Knuckle", variant_name)
    properties = {
        key: unreal.Vector(*value) if key == "first_person_effect_offset" else value
        for key, value in tuning.items()
    }
    properties.update(
        {
            "attack_montage": montage,
            "enable_attack_effects": True,
            "cast_effect": cast_effect,
            "charge_effect": charge_effect if variant_name == "Charged" else None,
            "impact_effect": impact_effect,
            "effect_attach_socket": "hand_r",
            "spawn_effect_on_off_hand": dual_hand,
            "off_hand_effect_attach_socket": "hand_l",
            "use_first_person_effect_overrides": True,
            "draw_debug_attack": False,
        }
    )
    skill.set_editor_properties(properties)
    unreal.EditorAssetLibrary.set_metadata_tag(
        skill, "Rogue10mVFXReadabilityProfile", "HeroShooterFP_v1"
    )
    unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)
    return skill


def main():
    for mapping in MARTIAL_ART_PRESENTATION:
        apply_presentation(*mapping)
    unreal.log(
        "RESULT=MARTIAL_ART_READABLE_VFX_APPLIED "
        f"skills={len(MARTIAL_ART_PRESENTATION)} style=Knuckle "
        "gameplay_fields=preserved profile=HeroShooterFP_v1"
    )


if __name__ == "__main__":
    main()
