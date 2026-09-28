"""Validate the Knuckle martial-arts skill tree data and Widget Blueprints."""

import unreal


PROFILE_PATH = "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle"
WINDOW_PATH = "/Game/Widget/Component/SkillTree/WBP_SkillTreeWindow"
ENTRY_PATH = "/Game/Widget/Component/SkillTree/WBP_SkillTreeEntry"
FRAME_PATH = "/Game/UI/MartialArts/T_UI_MartialArtsSkillTreeFrame"


def require_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing asset: {path}")
    return asset


def check(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def widget_names(path: str):
    widget = require_asset(path)
    tool = unreal.get_default_object(unreal.UMGToolSet)
    tree = tool.call_method("GetWidgets", (widget,))
    return {entry.widget.get_name() for entry in tree.widgets if entry.widget}


def main() -> None:
    require_asset(FRAME_PATH)
    profile = require_asset(PROFILE_PATH)
    skills = list(profile.get_editor_property("skill_tree_skills"))
    initial = list(profile.get_editor_property("initially_unlocked_skills"))
    check(len(skills) == 10, f"Knuckle tree must contain 10 skills, got {len(skills)}")
    check(len(initial) == 1, f"Knuckle tree must start with one skill, got {len(initial)}")
    check(initial[0] == skills[0], "The initial skill must be the first skill-tree node")
    tiers = {skill.get_editor_property("skill_tree_tier") for skill in skills}
    check(tiers == {1, 2, 3, 4}, f"Expected four martial-arts tiers, got {tiers}")
    names = {str(skill.get_editor_property("skill_name")) for skill in skills}
    for required_name in ("연환권", "철산고", "백열난무", "금강불괴", "무극패왕권"):
        check(required_name in names, f"Missing martial art: {required_name}")
    passives = [
        skill for skill in skills
        if "PASSIVE" in str(skill.get_editor_property("skill_tree_node_type")).upper()
    ]
    check(len(passives) == 1, f"Expected one passive technique, got {len(passives)}")
    check(abs(passives[0].get_editor_property("passive_damage_reduction") - 0.15) < 0.001,
          "Golden Body must provide 15% damage reduction")
    for skill in skills[1:]:
        prerequisites = list(skill.get_editor_property("prerequisite_skills"))
        conditions = list(skill.get_editor_property("unlock_conditions"))
        check(prerequisites, f"{skill.get_name()} has no prerequisite")
        check(conditions, f"{skill.get_name()} has no training/defeat condition")

    window_names = widget_names(WINDOW_PATH)
    entry_names = widget_names(ENTRY_PATH)
    for name in {
        "UI_SkillListContainer", "UI_SelectedSkillNameText", "UI_SelectedSkillConditionsText",
        "UI_MasteryProgressBar", "UI_MartialArtsBackgroundImage",
    }:
        check(name in window_names, f"Skill-tree window is missing {name}")
    for name in {"UI_SkillIconImage", "UI_SkillTierText", "UI_SkillProgressBar"}:
        check(name in entry_names, f"Skill-tree entry is missing {name}")

    unreal.log(
        "RESULT=MARTIAL_ARTIST_SKILL_TREE_VALIDATED skills=10 tiers=4 initial=1 "
        "passive_damage_reduction=0.15 widgets=2"
    )


if __name__ == "__main__":
    main()
