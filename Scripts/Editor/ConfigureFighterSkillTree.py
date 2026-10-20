"""Connect existing authored fist skills to both fighter profiles without rebaking combat."""

import json
from pathlib import Path
import unreal

PROFILES = (
    "/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed",
    "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle",
)
BASIC_ROOT = "/Game/DataAsset/AttackSkill/BasicBrawler"
BASIC = [f"{BASIC_ROOT}/DA_BasicBrawler_{name}" for name in
         ("LeftJab", "RightJab", "RightStraight", "RightHook")]
MARTIAL = [
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Jab",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight",
    "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03",
    "/Game/DataAsset/AttackSkill/MartialArts/DA_MartialArt_GoldenBody",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_JumpSlam",
    "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave",
]
COMBAT_FIELDS = (
    "input_slot", "damage", "attack_cooldown", "charge_seconds", "resource_costs",
    "hit_mode", "hit_count", "hit_interval", "max_hits_per_target",
    "hit_start_delay_seconds", "attack_montage", "animation_play_rate",
    "prerequisite_skills", "unlock_conditions", "passive_damage_reduction",
)


def require(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value:
        raise RuntimeError(f"Missing existing fighter asset: {path}")
    return value


def fingerprint(skill):
    result = {}
    for name in COMBAT_FIELDS:
        value = skill.get_editor_property(name)
        if name == "resource_costs":
            value = [{"type": str(v.get_editor_property("resource_type")),
                      "cost": v.get_editor_property("cost")} for v in value]
        elif name == "unlock_conditions":
            value = [{"type": str(v.get_editor_property("condition_type")),
                      "skill": (v.get_editor_property("required_skill").get_path_name()
                                if v.get_editor_property("required_skill") else None),
                      "monster": str(v.get_editor_property("required_monster_id")),
                      "name": str(v.get_editor_property("target_display_name")),
                      "count": v.get_editor_property("required_count")} for v in value]
        elif name == "prerequisite_skills":
            value = [v.get_path_name() if v else None for v in value]
        elif name == "attack_montage":
            value = value.get_path_name() if value else None
        elif not isinstance(value, (bool, int, float, str)):
            value = str(value)
        result[name] = value
    return result


def main():
    profiles = [require(path) for path in PROFILES]
    martial = [require(path) for path in MARTIAL]
    basics = [require(path) for path in BASIC]
    skills = martial + basics
    before = {skill.get_path_name(): fingerprint(skill) for skill in skills}
    old_defaults = [str(profile.get_editor_property("default_skill_bindings")) for profile in profiles]
    old_initial = [str(profile.get_editor_property("initially_unlocked_skills")) for profile in profiles]

    # Five entry-tier nodes fit the existing 850px canvas; advanced branches are retained.
    for skill, x in zip(basics, (80.0, 250.0, 600.0, 770.0)):
        skill.set_editor_property("skill_tree_tier", 1)
        skill.set_editor_property("skill_tree_position", unreal.Vector2D(x, 570.0))
    for profile in profiles:
        profile.set_editor_property("skill_tree_skills", skills)
        profile.set_editor_property("display_name", "격투가")
        profile.set_editor_property("description", "맨손 기본기부터 연환권·절기·상시 심법까지 이어지는 격투가 스킬트리. K 창에서 테스트 전체 해금과 스킬별 키 설정을 할 수 있다.")

    for skill in skills:
        if fingerprint(skill) != before[skill.get_path_name()]:
            raise RuntimeError(f"Combat/progression fields changed: {skill.get_path_name()}")
    for index, profile in enumerate(profiles):
        if str(profile.get_editor_property("default_skill_bindings")) != old_defaults[index]:
            raise RuntimeError("Default attack input bindings changed")
        if str(profile.get_editor_property("initially_unlocked_skills")) != old_initial[index]:
            raise RuntimeError("Explicit initial unlock list changed")
        if len(profile.get_editor_property("skill_tree_skills")) != 14:
            raise RuntimeError("Fighter tree must contain 14 unique skills")
    for asset in basics + profiles:
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=True):
            raise RuntimeError(f"Could not save fighter metadata: {asset.get_path_name()}")

    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    evidence = project / "Feature/doc/evidence/fighter-skill-tree-20260930"
    evidence.mkdir(parents=True, exist_ok=True)
    report = {"profiles": PROFILES, "nodes": 14, "active": 13, "passive": 1,
              "combat_before": before, "combat_after": {s.get_path_name(): fingerprint(s) for s in skills},
              "explicit_initial_unlocks_preserved": True, "default_bindings_preserved": True}
    (evidence / "asset-preservation.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("RESULT=FIGHTER_SKILL_TREE_CONFIGURED profiles=2 skills=14 combat_preserved=true")


if __name__ == "__main__":
    main()
