"""Enable the blended ground graph on the verified ThornWarden profile."""

import unreal

ROOT = "/Game/Rogue10m/World/OpenWorld/Monsters/"
CACTUS = "/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/"
actor = unreal.load_asset(ROOT + "DA_OW_ThornWarden")
if actor is None:
    raise RuntimeError("Missing active ThornWarden dataset")
profile = actor.get_editor_property("attack_pattern_profile")
if (profile is None or profile.get_path_name().split(".")[0] != ROOT + "DA_OW_BasicAttack_ThornWarden"
        or profile.get_editor_property("use_humanoid_anim_instance")):
    raise RuntimeError("ThornWarden profile changed since the pose audit")
mesh = actor.get_editor_property("skeletal_mesh")
if mesh is None:
    raise RuntimeError("Missing ThornWarden mesh")
skeleton = mesh.get_editor_property("skeleton")
for property_name, expected in (("idle_animation", "Cactus_IdleNormal_ANIM"),
                                ("movement_animation", "Cactus_WalkFWD_ANIM")):
    clip = profile.get_editor_property(property_name)
    if (clip is None or clip.get_path_name().split(".")[0] != CACTUS + expected
            or clip.get_editor_property("skeleton") != skeleton):
        raise RuntimeError("Unexpected ThornWarden " + property_name)

profile.set_editor_property("blend_legacy_locomotion", True)
if not unreal.EditorAssetLibrary.save_loaded_asset(profile, True):
    raise RuntimeError("Failed to save ThornWarden locomotion profile")
print("RESULT=THORN_WARDEN_LOCOMOTION_BLEND_ENABLED")
