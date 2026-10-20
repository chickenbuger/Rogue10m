"""Run existing real dialogue/supply checks plus the new authored appearance."""
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / "Scripts/Editor"))
import ValidateTutorialNPC as validator

validator.REPORT = ROOT / "Feature/doc/evidence/arin-blender-20261007/tutorial-runtime.json"
original_prepare = validator.prepare_fixture
original_dialogue = validator.begin_dialogue_checks


def prepare_fixture(world, pawn, npc):
    original_prepare(world, pawn, npc)
    mesh = npc.get_editor_property("guide_mesh")
    asset = mesh.get_skeletal_mesh_asset()
    animation = mesh.get_editor_property("animation_data").get_editor_property("anim_to_play")
    validator.check("authored_arin_mesh_in_real_pie", asset.get_path_name().split(".")[0]
                    == "/Game/Rogue10m/Characters/Arin/Blender/SK_Arin")
    validator.check("authored_arin_idle_in_real_pie", animation.get_path_name().split(".")[0]
                    == "/Game/Rogue10m/Characters/Arin/Blender/ArinIdle")
    validator.check("authored_mesh_idle_same_skeleton_in_pie",
                    asset.get_editor_property("skeleton") == animation.get_editor_property("skeleton"))
    validator.check("authored_arin_idle_two_seconds_in_pie", abs(animation.get_play_length()-2)<.0001)
    validator.check("authored_arin_humanoid_and_fbx_root_in_pie", mesh.get_num_bones()==24
                    and str(mesh.get_bone_name(0))=="Arin_Rig", count=mesh.get_num_bones())
    validator.state["arin_head_rotation"] = list(mesh.get_socket_rotation("head").to_tuple())
    validator.state["arin_actor_location"] = list(npc.get_actor_location().to_tuple())


def begin_dialogue_checks(world, pawn, npc):
    mesh = npc.get_editor_property("guide_mesh")
    rotation = list(mesh.get_socket_rotation("head").to_tuple())
    delta = max(abs(a-b) for a,b in zip(rotation,validator.state["arin_head_rotation"]))
    validator.check("authored_idle_really_moves_head_in_pie", delta>.0001,
                    head_rotation_delta_degrees=delta)
    validator.check("authored_idle_keeps_actor_stationary_in_pie",
                    list(npc.get_actor_location().to_tuple())==validator.state["arin_actor_location"])
    original_dialogue(world, pawn, npc)


validator.prepare_fixture = prepare_fixture
validator.begin_dialogue_checks = begin_dialogue_checks
validator.main()
