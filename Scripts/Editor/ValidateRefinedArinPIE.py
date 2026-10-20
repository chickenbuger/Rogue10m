"""Actual integrated NPC dialogue/supplies plus v2 scale/PBR/LOD/Idle checks."""
from pathlib import Path
import sys
import importlib
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/"Scripts/Editor"))
previous = getattr(unreal,"_rogue10m_tutorial_npc_validator",None)
if previous and not previous.get("finished"):
    raise RuntimeError("An NPC validator is already running; inspect its result before starting another")
if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Requires idle StartHub editor before starting the owned PIE verification")
import ValidateTutorialNPC as validator
validator = importlib.reload(validator)

validator.REPORT=ROOT/"Feature/doc/evidence/arin-quality-20261007/tutorial-runtime.json"
original_prepare=validator.prepare_fixture
original_dialogue=validator.begin_dialogue_checks


def prepare_fixture(world,pawn,npc):
    original_prepare(world,pawn,npc)
    component=npc.get_editor_property("guide_mesh")
    asset=component.get_skeletal_mesh_asset()
    animation=component.get_editor_property("animation_data").get_editor_property("anim_to_play")
    validator.check("refined_mesh_in_real_pie",asset.get_path_name().split(".")[0]=="/Game/Rogue10m/Characters/Arin/RefinedV2/SK_Arin")
    validator.check("refined_idle_in_real_pie",animation.get_path_name().split(".")[0]=="/Game/Rogue10m/Characters/Arin/RefinedV2/ArinIdle")
    validator.check("refined_same_skeleton_in_pie",asset.get_editor_property("skeleton")==animation.get_editor_property("skeleton"))
    validator.check("refined_two_second_idle_in_pie",abs(animation.get_play_length()-2)<.0001)
    validator.check("refined_real_humanoid_bones_in_pie",component.get_num_bones()==24 and str(component.get_bone_name(0))=="Arin_Rig")
    height=component.get_socket_location("head").z-npc.get_actor_location().z
    validator.check("refined_visible_physical_head_height_in_pie",140<=height<=149,observed_cm=height)
    materials=list(asset.get_editor_property("materials"))
    validator.check("refined_four_real_pbr_slots_in_pie",len(materials)==4 and all(
        slot.get_editor_property("material_interface").get_path_name().startswith("/Game/Rogue10m/Characters/Arin/RefinedV2/Materials/") for slot in materials))
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    validator.check("refined_three_persisted_lods_in_pie",subsystem.get_lod_count(asset)==3)
    validator.state["arin_head_rotation"]=list(component.get_socket_rotation("head").to_tuple())
    validator.state["arin_actor_location"]=list(npc.get_actor_location().to_tuple())


def begin_dialogue_checks(world,pawn,npc):
    component=npc.get_editor_property("guide_mesh")
    rotation=list(component.get_socket_rotation("head").to_tuple())
    delta=max(abs(a-b) for a,b in zip(rotation,validator.state["arin_head_rotation"]))
    validator.check("refined_idle_really_moves_head_in_pie",delta>.0001,delta_degrees=delta)
    validator.check("refined_idle_stationary_actor_in_pie",list(npc.get_actor_location().to_tuple())==validator.state["arin_actor_location"])
    original_dialogue(world,pawn,npc)


validator.prepare_fixture=prepare_fixture
validator.begin_dialogue_checks=begin_dialogue_checks
validator.main()
