"""Validate the inherited runtime character and retargeting asset structure."""

import unreal


CATALOG_PATH = (
    "/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog"
)
PLAYER_BLUEPRINT_PATH = (
    "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
)
SOURCE_MESH_PATH = (
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
)
SOURCE_ANIM_PATH = (
    "/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed"
)


def fail(message):
    raise RuntimeError(message)


def path_of(value):
    if value is None:
        return ""
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def find_anim_graph(blueprint):
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        if graph.get_name() == "AnimGraph":
            return graph
    fail(f"AnimGraph 누락: {blueprint.get_path_name()}")


def main():
    catalog = unreal.load_asset(CATALOG_PATH)
    if not catalog:
        fail(f"카탈로그 누락: {CATALOG_PATH}")

    parent_class = unreal.EditorAssetLibrary.load_blueprint_class(
        PLAYER_BLUEPRINT_PATH
    )
    if not parent_class:
        fail(f"플레이어 Blueprint class 누락: {PLAYER_BLUEPRINT_PATH}")
    parent_cdo = unreal.get_default_object(parent_class)
    if not isinstance(parent_cdo, unreal.Rogue10mStylizedCharacter):
        fail("BP_FirstPersonCharacter가 Rogue10mStylizedCharacter를 상속하지 않습니다.")

    if SOURCE_MESH_PATH not in path_of(
        parent_cdo.get_editor_property("animation_source_skeletal_mesh")
    ):
        fail("AnimationSourceSkeletalMesh 기본값이 Manny가 아닙니다.")
    if SOURCE_ANIM_PATH not in path_of(
        parent_cdo.get_editor_property("animation_source_anim_class")
    ):
        fail("AnimationSourceAnimClass 기본값이 ABP_Common_Unarmed가 아닙니다.")
    if CATALOG_PATH not in path_of(
        parent_cdo.get_editor_property("customization_catalog")
    ):
        fail("BP_FirstPersonCharacter의 CustomizationCatalog가 비어 있습니다.")

    source_component = parent_cdo.get_editor_property("animation_source_mesh")
    body_component = parent_cdo.get_editor_property("mesh")
    hair_component = parent_cdo.get_editor_property("appearance_hair_mesh")
    facial_component = parent_cdo.get_editor_property("appearance_facial_mesh")
    if body_component.get_attach_parent() != source_component:
        fail("Character Mesh가 AnimationSourceMesh의 자식이 아닙니다.")
    if hair_component.get_attach_parent() != body_component:
        fail("AppearanceHairMesh가 Character Mesh의 자식이 아닙니다.")
    if facial_component.get_attach_parent() != body_component:
        fail("AppearanceFacialMesh가 Character Mesh의 자식이 아닙니다.")
    archetypes = list(catalog.get_editor_property("archetypes"))
    if len(archetypes) != 6:
        fail(f"Archetype 수량 불일치: {len(archetypes)}")

    pairs = set()
    for entry in archetypes:
        race = entry.get_editor_property("race")
        gender = entry.get_editor_property("gender")
        pair = (str(race), str(gender))
        if pair in pairs:
            fail(f"중복 Archetype: {pair}")
        pairs.add(pair)

        body = entry.get_editor_property("body_mesh")
        character_class = entry.get_editor_property("character_class")
        retargeter = entry.get_editor_property("retargeter")
        anim_class = entry.get_editor_property("retarget_anim_class")
        if not all((body, character_class, retargeter, anim_class)):
            fail(f"상속/리타기팅 참조 누락: {pair}")

        character_cdo = unreal.get_default_object(character_class)
        if not unreal.MathLibrary.class_is_child_of(
            character_cdo.get_class(), parent_class
        ):
            fail(f"종족 Character가 공통 Player Blueprint를 상속하지 않습니다: {pair}")
        if character_cdo.get_editor_property("appearance_race") != race:
            fail(f"Character Race 기본값 불일치: {pair}")
        if character_cdo.get_editor_property("appearance_gender") != gender:
            fail(f"Character Gender 기본값 불일치: {pair}")
        if path_of(body) not in path_of(
            character_cdo.get_editor_property("appearance_body_mesh")
        ):
            fail(f"Character Body 기본값 불일치: {pair}")
        if path_of(anim_class) not in path_of(
            character_cdo.get_editor_property("appearance_anim_class")
        ):
            fail(f"Character AnimClass 기본값 불일치: {pair}")

        retarget_controller = unreal.IKRetargeterController.get_controller(
            retargeter
        )
        target_rig = retarget_controller.get_ik_rig(
            unreal.RetargetSourceOrTarget.TARGET
        )
        target_rig_controller = unreal.IKRigController.get_controller(
            target_rig
        )
        for chain in target_rig_controller.get_retarget_chains():
            chain_name = chain.get_editor_property("chain_name")
            start = target_rig_controller.get_retarget_chain_start_bone(
                chain_name
            )
            end = target_rig_controller.get_retarget_chain_end_bone(
                chain_name
            )
            if str(start) == "None" or str(end) == "None":
                fail(f"유효하지 않은 Target IK 체인: {pair}.{chain_name}")

        anim_blueprint_path = path_of(anim_class).split("_C")[0]
        anim_blueprint = unreal.load_asset(anim_blueprint_path)
        if not anim_blueprint:
            fail(f"Retarget AnimBP 로드 실패: {pair}")
        if anim_blueprint.get_editor_property(
            "target_skeleton"
        ) != body.get_editor_property("skeleton"):
            fail(f"Retarget AnimBP Skeleton 불일치: {pair}")

        graph = find_anim_graph(anim_blueprint)
        graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        retarget_nodes = [
            node
            for node in graph_editor.list_all_nodes()
            if isinstance(node, unreal.AnimGraphNode_RetargetPoseFromMesh)
        ]
        if len(retarget_nodes) != 1:
            fail(f"Retarget Pose From Mesh 노드 수량 불일치: {pair}")
        runtime_node = retarget_nodes[0].get_editor_property("node")
        if runtime_node.get_editor_property(
            "ik_retargeter_asset"
        ) != retargeter:
            fail(f"Retarget AnimBP의 Retargeter 참조 불일치: {pair}")

        unreal.log(
            f"[Rogue10mInheritedCharacterValidation] OK {pair} "
            f"class={path_of(character_class)}"
        )

    unreal.log(
        "[Rogue10mInheritedCharacterValidation] PASSED: "
        "공통 부모 + 6 상속 Character + 6 Retarget AnimBP"
    )


if __name__ == "__main__":
    main()
