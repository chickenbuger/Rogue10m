"""Create the Knuckle wuxia skill-tree data and its generated UI frame texture."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
FRAME_SOURCE = PROJECT_ROOT / "Content" / "UI" / "MartialArts" / "Source" / "T_UI_MartialArtsSkillTreeFrame.png"
FRAME_DESTINATION = "/Game/UI/MartialArts"
FRAME_ASSET_PATH = f"{FRAME_DESTINATION}/T_UI_MartialArtsSkillTreeFrame"
PROFILE_PATH = "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle"
PASSIVE_ROOT = "/Game/DataAsset/AttackSkill/MartialArts"
PASSIVE_PATH = f"{PASSIVE_ROOT}/DA_MartialArt_GoldenBody"


SKILL_PATHS = {
    "chain": "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01",
    "collapse": "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary02",
    "hundred": "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03",
    "kick": "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special",
    "shoulder": "/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged",
    "cloud": "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Jab",
    "diamond": "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight",
    "ultimate": "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave",
    "mountain": "/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_JumpSlam",
}


ICON_PATHS = {
    "chain": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_Jab",
    "collapse": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_Straight",
    "kick": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_JumpSlam",
    "shoulder": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_ChargedShockwave",
    "cloud": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_Dodge",
    "diamond": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_Straight",
    "hundred": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_Jab",
    "golden": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_DoubleJump",
    "mountain": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_JumpSlam",
    "ultimate": "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_ChargedShockwave",
}


MONSTER_PATHS = {
    "iron_disciple": "/Game/DataAsset/Monster/Normal/DA_Monster_Normal_DuneShell",
    "iron_captain": "/Game/DataAsset/Monster/Normal/DA_Monster_Normal_IronShell",
    "desert_bandit": "/Game/DataAsset/Monster/Normal/DA_Monster_Normal_CactusRaider",
    "thorn_master": "/Game/DataAsset/Monster/MidBoss/DA_Monster_MidBoss_ThornWarden",
    "poison_matriarch": "/Game/DataAsset/Monster/MidBoss/DA_Monster_MidBoss_SporeMatriarch",
    "abyss_lord": "/Game/DataAsset/Monster/FinalBoss/DA_Monster_FinalBoss_AbyssOverlordAzathor",
}


def log(message: str) -> None:
    unreal.log(f"[MartialArtistSkillTree] {message}")


def require_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


def enum_value(enum_type, name: str):
    for candidate in (name, name.upper(), name.title().replace("_", "")):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    raise RuntimeError(f"Enum value not found: {enum_type}.{name}")


def create_or_load_data_asset(path: str, data_asset_class):
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing
    package_path, asset_name = path.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(package_path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", data_asset_class)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, data_asset_class, factory
    )


def import_frame_texture():
    if not FRAME_SOURCE.exists():
        raise RuntimeError(f"Generated frame source is missing: {FRAME_SOURCE}")
    unreal.EditorAssetLibrary.make_directory(FRAME_DESTINATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(FRAME_SOURCE))
    task.set_editor_property("destination_path", FRAME_DESTINATION)
    task.set_editor_property("destination_name", "T_UI_MartialArtsSkillTreeFrame")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = require_asset(FRAME_ASSET_PATH)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def skill_condition(skill, count: int, display_name: str):
    condition = unreal.Rogue10mSkillUnlockCondition()
    condition.set_editor_property(
        "condition_type",
        enum_value(unreal.Rogue10mSkillUnlockConditionType, "SKILL_USE_COUNT"),
    )
    condition.set_editor_property("required_skill", skill)
    condition.set_editor_property("target_display_name", display_name)
    condition.set_editor_property("required_count", count)
    return condition


def monster_condition(monster, count: int, display_name: str):
    condition = unreal.Rogue10mSkillUnlockCondition()
    condition.set_editor_property(
        "condition_type",
        enum_value(unreal.Rogue10mSkillUnlockConditionType, "MONSTER_DEFEAT_COUNT"),
    )
    condition.set_editor_property("required_monster_id", monster.get_editor_property("monster_id"))
    condition.set_editor_property("target_display_name", display_name)
    condition.set_editor_property("required_count", count)
    return condition


def configure_skill(skill, *, name, description, tier, position, prerequisites=(), conditions=(),
                    icon=None, reward="", passive_reduction=0.0):
    passive = passive_reduction > 0.0
    skill.set_editor_properties(
        {
            "skill_name": name,
            "skill_description": description,
            "skill_tree_node_type": enum_value(
                unreal.Rogue10mSkillTreeNodeType,
                "PASSIVE_TECHNIQUE" if passive else "ACTIVE_TECHNIQUE",
            ),
            "skill_tree_tier": tier,
            "skill_tree_position": unreal.Vector2D(position[0], position[1]),
            "prerequisite_skills": list(prerequisites),
            "unlock_conditions": list(conditions),
            "unlock_reward_text": reward,
            "passive_damage_reduction": passive_reduction,
            "skill_icon": icon,
            "icon_tint": unreal.LinearColor(0.72, 0.94, 0.72, 1.0),
        }
    )
    unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)


def main() -> None:
    import_frame_texture()
    skills = {key: require_asset(path) for key, path in SKILL_PATHS.items()}
    icons = {key: require_asset(path) for key, path in ICON_PATHS.items()}
    monsters = {key: require_asset(path) for key, path in MONSTER_PATHS.items()}

    attack_class = unreal.load_class(None, "/Script/Rogue10m.Rogue10mAttackSkillData")
    if not attack_class:
        raise RuntimeError("Rogue10mAttackSkillData is not loaded. Build and restart the Editor first.")
    golden_body = create_or_load_data_asset(PASSIVE_PATH, attack_class)
    skills["golden"] = golden_body

    configure_skill(
        skills["chain"], name="연환권", tier=1, position=(425, 570), icon=icons["chain"],
        description="호흡을 짧게 끊으며 좌우 권을 연달아 내지르는 권사의 입문 초식.",
        reward="권사 기본 공격으로 즉시 사용할 수 있다.",
    )
    configure_skill(
        skills["collapse"], name="붕권", tier=2, position=(165, 420), icon=icons["collapse"],
        description="한 치의 거리에서 전신의 경력을 폭발시켜 적의 중심을 무너뜨린다.",
        prerequisites=[skills["chain"]],
        conditions=[skill_condition(skills["chain"], 20, "연환권")],
        reward="해금 후 공격 슬롯에 장착 가능 · 단일 대상 경력 집중",
    )
    configure_skill(
        skills["kick"], name="파진각", tier=2, position=(425, 420), icon=icons["kick"],
        description="낮게 회전하며 진형의 하단을 쓸어 다수의 적을 흐트러뜨리는 각법.",
        prerequisites=[skills["chain"]],
        conditions=[monster_condition(monsters["iron_disciple"], 5, "철갑사")],
        reward="해금 후 공격 슬롯에 장착 가능 · 부채꼴 제압",
    )
    configure_skill(
        skills["shoulder"], name="철산고", tier=2, position=(685, 420), icon=icons["shoulder"],
        description="등과 어깨에 모은 경력을 한순간에 쏟아내 장갑째 밀어내는 근접 절초.",
        prerequisites=[skills["chain"]],
        conditions=[
            skill_condition(skills["chain"], 50, "연환권"),
            monster_condition(monsters["iron_captain"], 1, "철갑대장"),
        ],
        reward="해금 후 공격 슬롯에 장착 가능 · 높은 충격력",
    )
    configure_skill(
        skills["cloud"], name="유운수", tier=3, position=(135, 255), icon=icons["cloud"],
        description="흐르는 구름처럼 힘을 흘려보내고 빈틈으로 손끝을 되돌리는 변화수.",
        prerequisites=[skills["collapse"]],
        conditions=[skill_condition(skills["collapse"], 30, "붕권")],
        reward="해금 후 공격 슬롯에 장착 가능 · 빠른 연계 시동",
    )
    configure_skill(
        skills["diamond"], name="금강쇄", tier=3, position=(345, 255), icon=icons["diamond"],
        description="두 주먹을 쇠사슬처럼 엮어 상대의 방어 틈을 끊어내는 강권.",
        prerequisites=[skills["kick"]],
        conditions=[monster_condition(monsters["desert_bandit"], 8, "사막마적")],
        reward="해금 후 공격 슬롯에 장착 가능 · 방어선 분쇄",
    )
    skills["hundred"].set_editor_property(
        "hit_mode", enum_value(unreal.Rogue10mAttackHitMode, "CONTINUOUS")
    )
    skills["hundred"].set_editor_property("hit_count", 6)
    skills["hundred"].set_editor_property("hit_interval", 0.075)
    skills["hundred"].set_editor_property("max_hits_per_target", 6)
    configure_skill(
        skills["hundred"], name="백열난무", tier=3, position=(625, 255), icon=icons["hundred"],
        description="내공을 양팔에 순환시켜 눈앞을 백 번의 권영으로 뒤덮는 연속 절기.",
        prerequisites=[skills["shoulder"]],
        conditions=[skill_condition(skills["shoulder"], 40, "철산고")],
        reward="해금 후 공격 슬롯에 장착 가능 · 6연속 타격",
    )
    configure_skill(
        golden_body, name="금강불괴", tier=4, position=(150, 90), icon=icons["golden"],
        description="호흡과 기혈을 단련해 살갗을 금강처럼 굳히는 권사의 호신 심법.",
        prerequisites=[skills["cloud"], skills["diamond"]],
        conditions=[monster_condition(monsters["thorn_master"], 1, "가시문주")],
        reward="해금 즉시 받는 피해 15% 감소 · 슬롯 장착 불필요",
        passive_reduction=0.15,
    )
    configure_skill(
        skills["mountain"], name="태산압정", tier=4, position=(425, 90), icon=icons["mountain"],
        description="공중에서 기세를 모아 태산처럼 낙하하며 주변의 적을 짓누른다.",
        prerequisites=[skills["diamond"]],
        conditions=[monster_condition(monsters["poison_matriarch"], 1, "독무노파")],
        reward="해금 후 공격 슬롯에 장착 가능 · 착지 원형 충격",
    )
    configure_skill(
        skills["ultimate"], name="무극패왕권", tier=4, position=(700, 90), icon=icons["ultimate"],
        description="세 갈래 권맥을 하나로 합쳐 무극의 경력을 전방에 폭발시키는 문파 최종 오의.",
        prerequisites=[skills["hundred"], golden_body, skills["mountain"]],
        conditions=[
            skill_condition(skills["hundred"], 100, "백열난무"),
            monster_condition(monsters["abyss_lord"], 1, "심연마군"),
        ],
        reward="권사 최종 오의 · 넓은 전방 권압과 높은 피해",
    )

    profile = require_asset(PROFILE_PATH)
    slot = unreal.Rogue10mAttackInputSlot
    ordered_skills = [
        skills["chain"], skills["collapse"], skills["kick"], skills["shoulder"],
        skills["cloud"], skills["diamond"], skills["hundred"], golden_body,
        skills["mountain"], skills["ultimate"],
    ]
    profile.set_editor_properties(
        {
            "display_name": "권사",
            "description": "수련 횟수와 강적 토벌로 경맥을 열어 입문 초식부터 문파 최종 오의까지 전수받는 무협식 권법 트리.",
            "default_skill_bindings": {
                enum_value(slot, "PRIMARY"): skills["chain"],
            },
            "skill_tree_skills": ordered_skills,
            "initially_unlocked_skills": [skills["chain"]],
        }
    )
    unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("RESULT=MARTIAL_ARTIST_SKILL_TREE_CREATED skills=10 tiers=4 initial=1 passive=1")


if __name__ == "__main__":
    main()
