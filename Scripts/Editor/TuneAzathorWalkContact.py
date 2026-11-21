"""Set the measured stance speed on the active Garret boss profile."""

import unreal

BOSS = "/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor"
EXPECTED_PROFILE = "/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/DA_HumanBoss_AttackProfile"
EXPECTED_WALK = "/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/A_HumanBoss_Walk"

boss = unreal.load_asset(BOSS)
if boss is None:
    raise RuntimeError("Missing live Azathor data")
profile = boss.get_editor_property("attack_pattern_profile")
if profile is None or profile.get_path_name().split(".")[0] != EXPECTED_PROFILE:
    raise RuntimeError("The live boss now uses a different animation profile")
walk = profile.get_editor_property("movement_animation")
scale = boss.get_editor_property("mesh_relative_scale")
if (walk is None or walk.get_path_name().split(".")[0] != EXPECTED_WALK
        or scale.to_tuple() != (1.0, 1.0, 1.0)
        or not walk.get_editor_property("force_root_lock")
        or walk.get_editor_property("enable_root_motion")):
    raise RuntimeError("Azathor walk no longer matches the measured in-place clip")

profile.set_editor_property("authored_walk_speed", 300.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(profile, True):
    raise RuntimeError("Unable to save the tuned boss profile")
print("RESULT=AZATHOR_WALK_CONTACT_TUNED authored_speed_cm_s=300.0")
