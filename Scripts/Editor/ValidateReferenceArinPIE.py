"""Actual integrated NPC dialogue/supplies plus v3 scale/PBR/LOD/Idle checks."""
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
import ApplyReferenceArin as application
import ValidateTutorialNPC as validator
validator = importlib.reload(validator)

validator.REPORT=ROOT/"Feature/doc/evidence/arin-reference-refinement-20261009/tutorial-runtime.json"
original_prepare=validator.prepare_fixture
original_dialogue=validator.begin_dialogue_checks


def prepare_fixture(world,pawn,npc):
    original_prepare(world,pawn,npc)
    component=npc.get_editor_property("guide_mesh")
    asset=component.get_skeletal_mesh_asset()
    animation=component.get_editor_property("animation_data").get_editor_property("anim_to_play")
    validator.check("reference_mesh_in_real_pie",asset.get_path_name().split(".")[0]=="/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin")
    validator.check("reference_idle_in_real_pie",animation.get_path_name().split(".")[0]=="/Game/Rogue10m/Characters/Arin/RefinedV3/ArinIdle")
    validator.check("reference_same_skeleton_in_pie",asset.get_editor_property("skeleton")==animation.get_editor_property("skeleton"))
    validator.check("reference_two_second_idle_in_pie",abs(animation.get_play_length()-2)<.0001)
    bone_names=[str(component.get_bone_name(index)) for index in range(component.get_num_bones())]
    bone_contract=application._validate_bone_names(bone_names)
    validator.check("reference_exact_authored_bones_in_pie",True,
                    bone_names=bone_names,authored_bone_count=bone_contract["bone_count"])
    height=component.get_socket_location("head").z-component.get_world_location().z
    expected_head_cm=bone_contract["head_joint_z_m"]*100
    validator.check("reference_visible_physical_head_height_in_pie",abs(height-expected_head_cm)<=3,
                    observed_cm=height,expected_rest_cm=expected_head_cm)
    materials=list(asset.get_editor_property("materials"))
    validator.check("reference_single_real_pbr_slot_in_pie",len(materials)==1 and all(
        slot.get_editor_property("material_interface").get_path_name().startswith("/Game/Rogue10m/Characters/Arin/RefinedV3/Materials/") for slot in materials))
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    validator.check("reference_three_persisted_lods_in_pie",subsystem.get_lod_count(asset)==3)
    validator.state["arin_head_rotation"]=list(component.get_socket_rotation("head").to_tuple())
    validator.state["arin_actor_location"]=list(npc.get_actor_location().to_tuple())


def begin_dialogue_checks(world,pawn,npc):
    component=npc.get_editor_property("guide_mesh")
    rotation=list(component.get_socket_rotation("head").to_tuple())
    delta=max(abs(a-b) for a,b in zip(rotation,validator.state["arin_head_rotation"]))
    validator.check("reference_idle_really_moves_head_in_pie",delta>.0001,delta_degrees=delta)
    validator.check("reference_idle_stationary_actor_in_pie",list(npc.get_actor_location().to_tuple())==validator.state["arin_actor_location"])
    original_dialogue(world,pawn,npc)


validator.prepare_fixture=prepare_fixture
validator.begin_dialogue_checks=begin_dialogue_checks
validator.main()
