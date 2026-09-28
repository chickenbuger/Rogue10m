"""Create inherited race/gender pawns and Manny-to-Stylized retarget assets."""

import unreal

CATALOG_PATH = (
    "/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog"
)
PLAYER_BASE_PATH = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
SOURCE_RIG_PATH = (
    "/Game/Polyart/SharedResources/Characters/AnimationStuff/"
    "Retargeting/IK_Mannequin"
)
SOURCE_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SOURCE_ANIM_PATH = (
    "/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed"
)
RETARGET_FOLDER = "/Game/Character/Customization/Retargeting"
CHARACTER_FOLDER = "/Game/Character/Customization/Characters"

ARCHETYPES = (
    {
        "race": "HUMAN",
        "gender": "MALE",
        "code": "Hu_M",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Human/Male/"
            "SK_Hu_M_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Hu_M",
        "character": "BP_Rogue10m_HumanMaleCharacter",
    },
    {
        "race": "HUMAN",
        "gender": "FEMALE",
        "code": "Hu_F",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Human/Female/"
            "SK_Hu_F_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Hu_F",
        "character": "BP_Rogue10m_HumanFemaleCharacter",
    },
    {
        "race": "DWARF",
        "gender": "MALE",
        "code": "Dw_M",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Dwarf/Male/"
            "SK_Dw_M_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Dw_M",
        "character": "BP_Rogue10m_DwarfMaleCharacter",
    },
    {
        "race": "DWARF",
        "gender": "FEMALE",
        "code": "Dw_F",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Dwarf/Female/"
            "SK_Dw_F_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Dw_F",
        "character": "BP_Rogue10m_DwarfFemaleCharacter",
    },
    {
        "race": "ORC",
        "gender": "MALE",
        "code": "Or_M",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Orc/Male/"
            "SK_Or_M_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Or_M",
        "character": "BP_Rogue10m_OrcMaleCharacter",
    },
    {
        "race": "ORC",
        "gender": "FEMALE",
        "code": "Or_F",
        "body": (
            "/Game/StylizedCharacter/Meshes/Character/Orc/Female/"
            "SK_Or_F_FullBody"
        ),
        "rig": "/Game/StylizedCharacter/Rigs/IK_Or_F",
        "character": "BP_Rogue10m_OrcFemaleCharacter",
    },
)


def log(message):
    unreal.log(f"[Rogue10mInheritedCharacterAssets] {message}")


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"필수 에셋을 찾을 수 없습니다: {path}")
    return asset


def require_blueprint_class(path):
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not generated_class:
        raise RuntimeError(f"Blueprint GeneratedClass 로드 실패: {path}")
    return generated_class


def enum_value(enum_type, name):
    if hasattr(enum_type, name):
        return getattr(enum_type, name)
    title_name = name.title()
    if hasattr(enum_type, title_name):
        return getattr(enum_type, title_name)
    raise RuntimeError(f"Enum 값을 찾을 수 없습니다: {enum_type}.{name}")


def create_or_load_target_rig(definition):
    asset_name = f"IK_Target_{definition['code']}"
    asset_path = f"{RETARGET_FOLDER}/{asset_name}"
    target_rig = unreal.load_asset(asset_path)
    if not target_rig:
        if not unreal.EditorAssetLibrary.duplicate_asset(
            definition["rig"], asset_path
        ):
            raise RuntimeError(f"Target IK Rig 복제 실패: {asset_path}")
        target_rig = require_asset(asset_path)

    controller = unreal.IKRigController.get_controller(target_rig)
    for chain in list(controller.get_retarget_chains()):
        chain_name = chain.get_editor_property("chain_name")
        start_bone = controller.get_retarget_chain_start_bone(chain_name)
        end_bone = controller.get_retarget_chain_end_bone(chain_name)
        if str(start_bone) == "None" or str(end_bone) == "None":
            controller.remove_retarget_chain(chain_name)
            log(
                f"유효하지 않은 Target IK 체인 제거: "
                f"{asset_name}.{chain_name}"
            )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        target_rig, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Target IK Rig 저장 실패: {asset_path}")
    return target_rig

def configure_pose_preserving_retargeter(controller):
    """Keep source body motion while preserving target limb proportions.

    UE default setup may select the rig's pelvis as its Root Motion root.
    The post-FK operation then overwrites the pelvis without moving retargeted children.
    Live A/B also showed target FBIK adding foot rotation spikes; FK is the default
    until a separately validated body-specific IK profile is introduced.
    """
    source_side = unreal.RetargetSourceOrTarget.SOURCE
    target_side = unreal.RetargetSourceOrTarget.TARGET
    rig_controllers = {
        side: unreal.IKRigController.get_controller(controller.get_ik_rig(side))
        for side in (source_side, target_side)
    }
    roots = {}
    for side, rig in rig_controllers.items():
        root_chains = [c.get_editor_property('chain_name') for c in rig.get_retarget_chains()
                       if str(c.get_editor_property('chain_name')).lower() == 'root']
        if len(root_chains) != 1:
            raise RuntimeError('Expected one real root chain on each retarget rig')
        roots[side] = rig.get_retarget_chain_start_bone(root_chains[0])
        if str(roots[side]).lower() != 'root':
            raise RuntimeError('Root Motion must target the actual root bone, not pelvis')
    root_ops = []
    ik_ops = []
    for index in range(controller.get_num_retarget_ops()):
        op = controller.get_op_controller(index)
        if isinstance(op, unreal.IKRetargetRootMotionController): root_ops.append((index, op))
        if isinstance(op, unreal.IKRetargetRunIKRigController): ik_ops.append(index)
    if len(root_ops) != 1 or len(ik_ops) != 1:
        raise RuntimeError('Expected one Root Motion and one Run IK Rig operation')
    root_index, root = root_ops[0]
    root.set_source_root_bone(roots[source_side])
    root.set_target_root_bone(roots[target_side])
    controller.set_retarget_op_enabled(root_index, True)
    controller.set_retarget_op_enabled(ik_ops[0], False)
    target_chains = {str(c.get_editor_property('chain_name'))
                     for c in rig_controllers[target_side].get_retarget_chains()}
    cleared = []
    for chain in ('Tail', 'Cape', 'Tabard_Back', 'Tabard_Front'):
        if chain in target_chains:
            # There are no matching source accessory chains; fuzzy Spine/HandRootIK
            # mappings drive cloth/tail bones with unrelated body animation.
            controller.set_source_chain('None', chain)
            cleared.append(chain)
    return {'source_root':str(root.get_source_root_bone()),
            'target_root':str(root.get_target_root_bone()),
            'target_pelvis':str(root.get_settings().get_editor_property('target_pelvis').get_editor_property('bone_name')),
            'run_ik_enabled':controller.get_retarget_op_enabled(ik_ops[0]),
            'unmapped_accessories':cleared}

def create_or_load_retargeter(definition):
    asset_name = f"IKR_Manny_To_{definition['code']}"
    asset_path = f"{RETARGET_FOLDER}/{asset_name}"
    retargeter = unreal.load_asset(asset_path)
    if not retargeter:
        retargeter = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            RETARGET_FOLDER,
            unreal.IKRetargeter,
            unreal.IKRetargetFactory(),
        )
    if not retargeter:
        raise RuntimeError(f"IK Retargeter 생성 실패: {asset_path}")

    controller = unreal.IKRetargeterController.get_controller(retargeter)
    source_rig = require_asset(SOURCE_RIG_PATH)
    target_rig = create_or_load_target_rig(definition)
    source_mesh = require_asset(SOURCE_MESH_PATH)
    target_mesh = require_asset(definition["body"])
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    controller.set_preview_mesh(
        unreal.RetargetSourceOrTarget.SOURCE, source_mesh
    )
    controller.set_preview_mesh(
        unreal.RetargetSourceOrTarget.TARGET, target_mesh
    )
    controller.remove_all_ops()
    controller.add_default_ops()
    controller.auto_map_chains(
        unreal.AutoMapChainType.EXACT, True
    )
    controller.auto_map_chains(
        unreal.AutoMapChainType.FUZZY, False
    )
    configure_pose_preserving_retargeter(controller)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        retargeter, only_if_is_dirty=False
    ):
        raise RuntimeError(f"IK Retargeter 저장 실패: {asset_path}")
    log(f"Retargeter 갱신: {asset_path}")
    return retargeter


def find_anim_graph(blueprint):
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        if graph.get_name() == "AnimGraph":
            return graph
    raise RuntimeError(f"AnimGraph를 찾을 수 없습니다: {blueprint.get_path_name()}")


def find_retarget_action(graph_editor):
    candidates = [
        name
        for name in graph_editor.list_available_nodes([])
        if (
            "메시에서포즈리타깃" in name
            or "RetargetPoseFromMesh" in name.replace(" ", "")
            or "RetargetPosefromMesh" in name.replace(" ", "")
        )
    ]
    if not candidates:
        raise RuntimeError("Retarget Pose From Mesh 노드 액션을 찾지 못했습니다.")
    return candidates[0]


def create_or_load_anim_blueprint(definition, retargeter):
    asset_name = f"ABP_Retarget_{definition['code']}"
    asset_path = f"{RETARGET_FOLDER}/{asset_name}"
    body_mesh = require_asset(definition["body"])
    target_skeleton = body_mesh.get_editor_property("skeleton")
    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property("target_skeleton", target_skeleton)
        factory.set_editor_property("preview_skeletal_mesh", body_mesh)
        factory.set_editor_property("parent_class", unreal.AnimInstance)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            RETARGET_FOLDER,
            unreal.AnimBlueprint,
            factory,
        )
    if not blueprint:
        raise RuntimeError(f"Retarget AnimBP 생성 실패: {asset_path}")
    if blueprint.get_editor_property("target_skeleton") != target_skeleton:
        raise RuntimeError(f"Retarget AnimBP Skeleton 불일치: {asset_path}")

    graph = find_anim_graph(blueprint)
    graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    nodes = list(graph_editor.list_all_nodes())
    roots = [
        node for node in nodes
        if isinstance(node, unreal.AnimGraphNode_Root)
    ]
    if len(roots) != 1:
        raise RuntimeError(f"AnimGraph Root 수량 불일치: {asset_path}")
    graph_editor.remove_nodes([node for node in nodes if node not in roots])

    retarget_node = graph_editor.create_node_from_name(
        find_retarget_action(graph_editor),
        unreal.Vector2D(-320.0, 0.0),
        [],
        unreal.AnimGraphNode_RetargetPoseFromMesh,
    )
    if not isinstance(
        retarget_node, unreal.AnimGraphNode_RetargetPoseFromMesh
    ):
        raise RuntimeError(f"Retarget Pose 노드 생성 실패: {asset_path}")

    runtime_node = retarget_node.get_editor_property("node")
    runtime_node.set_editor_property("ik_retargeter_asset", retargeter)
    retarget_node.set_editor_property("node", runtime_node)

    output_pins = [
        pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(retarget_node)
        if unreal.BlueprintGraphPinLibrary.get_pin_direction(pin)
        == unreal.EdGraphPinDirection.EGPD_OUTPUT
    ]
    input_pins = [
        pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(roots[0])
        if unreal.BlueprintGraphPinLibrary.get_pin_direction(pin)
        == unreal.EdGraphPinDirection.EGPD_INPUT
    ]
    if len(output_pins) != 1 or len(input_pins) != 1:
        raise RuntimeError(f"Retarget AnimGraph Pose pin 불일치: {asset_path}")
    if not unreal.BlueprintGraphPinLibrary.try_create_connection(
        output_pins[0], input_pins[0]
    ):
        raise RuntimeError(f"Retarget Pose → Root 연결 실패: {asset_path}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    errors = graph_editor.list_nodes_with_errors()
    if errors:
        raise RuntimeError(
            f"Retarget AnimBP compile node error: "
            f"{[node.get_name() for node in errors]}"
        )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Retarget AnimBP 저장 실패: {asset_path}")
    log(f"Retarget AnimBP 갱신: {asset_path}")
    return blueprint


def configure_player_base():
    blueprint = require_asset(PLAYER_BASE_PATH)
    expected_parent = unreal.Rogue10mStylizedCharacter
    current_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
        blueprint
    )
    if current_parent != expected_parent:
        unreal.BlueprintEditorLibrary.reparent_blueprint(
            blueprint, expected_parent
        )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    generated_class = require_blueprint_class(PLAYER_BASE_PATH)
    cdo = unreal.get_default_object(generated_class)
    cdo.set_editor_property(
        "customization_catalog", require_asset(CATALOG_PATH)
    )
    cdo.set_editor_property(
        "animation_source_skeletal_mesh", require_asset(SOURCE_MESH_PATH)
    )
    cdo.set_editor_property(
        "animation_source_anim_class",
        require_blueprint_class(SOURCE_ANIM_PATH),
    )
    cdo.get_editor_property("mesh").set_relative_transform(
        unreal.Transform(), False, True
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError("BP_FirstPersonCharacter 재부모화 저장 실패")
    log("BP_FirstPersonCharacter → Rogue10mStylizedCharacter 재부모화")
    return require_blueprint_class(PLAYER_BASE_PATH)


def create_or_load_character_blueprint(
    definition, parent_class, anim_blueprint
):
    asset_path = f"{CHARACTER_FOLDER}/{definition['character']}"
    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            definition["character"],
            CHARACTER_FOLDER,
            unreal.Blueprint,
            factory,
        )
    if not blueprint:
        raise RuntimeError(f"상속 Character Blueprint 생성 실패: {asset_path}")
    if (
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
        != parent_class
    ):
        unreal.BlueprintEditorLibrary.reparent_blueprint(
            blueprint, parent_class
        )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    generated_class = require_blueprint_class(asset_path)
    cdo = unreal.get_default_object(generated_class)
    race = enum_value(unreal.Rogue10mCharacterRace, definition["race"])
    gender = enum_value(
        unreal.Rogue10mCharacterGender, definition["gender"]
    )
    cdo.set_editor_property("appearance_race", race)
    cdo.set_editor_property("appearance_gender", gender)
    cdo.set_editor_property(
        "appearance_body_mesh", require_asset(definition["body"])
    )
    cdo.set_editor_property(
        "appearance_anim_class",
        unreal.BlueprintEditorLibrary.generated_class(anim_blueprint),
    )
    cdo.set_editor_property(
        "customization_catalog", require_asset(CATALOG_PATH)
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"상속 Character Blueprint 저장 실패: {asset_path}")
    log(f"상속 Character 갱신: {asset_path}")
    return require_blueprint_class(asset_path)


def update_catalog(generated):
    catalog = require_asset(CATALOG_PATH)
    archetypes = list(catalog.get_editor_property("archetypes"))
    if len(archetypes) != len(ARCHETYPES):
        raise RuntimeError(
            f"카탈로그 Archetype 수량 불일치: {len(archetypes)}"
        )

    race_enum = unreal.Rogue10mCharacterRace
    gender_enum = unreal.Rogue10mCharacterGender
    by_pair = {
        (
            enum_value(race_enum, definition["race"]),
            enum_value(gender_enum, definition["gender"]),
        ): generated[definition["code"]]
        for definition in ARCHETYPES
    }
    for entry in archetypes:
        pair = (
            entry.get_editor_property("race"),
            entry.get_editor_property("gender"),
        )
        if pair not in by_pair:
            raise RuntimeError(f"카탈로그에 알 수 없는 Archetype이 있습니다: {pair}")
        generated_data = by_pair[pair]
        entry.set_editor_property(
            "character_class", generated_data["character_class"]
        )
        entry.set_editor_property(
            "retargeter", generated_data["retargeter"]
        )
        entry.set_editor_property(
            "retarget_anim_class", generated_data["anim_class"]
        )
    catalog.set_editor_property("archetypes", archetypes)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        catalog, only_if_is_dirty=False
    ):
        raise RuntimeError("상속 Character 참조 카탈로그 저장 실패")
    log("카탈로그 CharacterClass·Retargeter·AnimClass 연결 완료")


def main():
    unreal.EditorAssetLibrary.make_directory(RETARGET_FOLDER)
    unreal.EditorAssetLibrary.make_directory(CHARACTER_FOLDER)

    generated = {}
    for definition in ARCHETYPES:
        retargeter = create_or_load_retargeter(definition)
        anim_blueprint = create_or_load_anim_blueprint(
            definition, retargeter
        )
        generated[definition["code"]] = {
            "retargeter": retargeter,
            "anim_blueprint": anim_blueprint,
            "anim_class": unreal.BlueprintEditorLibrary.generated_class(
                anim_blueprint
            ),
        }

    parent_class = configure_player_base()
    for definition in ARCHETYPES:
        data = generated[definition["code"]]
        data["character_class"] = create_or_load_character_blueprint(
            definition, parent_class, data["anim_blueprint"]
        )

    update_catalog(generated)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("상속 기반 Character 에셋 생성 완료")


if __name__ == "__main__":
    main()
