"""Enable floor support on the two existing Dwarf body retarget graphs."""

import json

import unreal


RETARGET_FOLDER = '/Game/Character/Customization/Retargeting'
PROFILES = ('Dw_M', 'Dw_F')


def describe_graph(blueprint):
    skeleton = blueprint.get_editor_property('target_skeleton')
    graphs = unreal.BlueprintEditorLibrary.list_graphs(blueprint)
    anim_graph = next(graph for graph in graphs if graph.get_name() == 'AnimGraph')
    nodes = unreal.BlueprintGraphEditor.get_graph_editor(anim_graph).list_all_nodes()
    retarget_nodes = [node for node in nodes if isinstance(node, unreal.AnimGraphNode_RetargetPoseFromMesh)]
    if not skeleton or len(retarget_nodes) != 1:
        raise RuntimeError(f'Unexpected retarget graph in {blueprint.get_path_name()}')
    retargeter = retarget_nodes[0].get_editor_property('node').get_editor_property('ik_retargeter_asset')
    if not retargeter:
        raise RuntimeError(f'Missing retargeter in {blueprint.get_path_name()}')
    return {
        'skeleton': skeleton.get_path_name(),
        'retargeter': retargeter.get_path_name(),
        'graph_nodes': sorted(node.get_class().get_name() for node in nodes),
    }


def main():
    native = unreal.Rogue10mWeaponRetargetAnimInstance
    to_save = []
    for code in PROFILES:
        blueprint = unreal.load_asset(f'{RETARGET_FOLDER}/ABP_Retarget_{code}')
        if not isinstance(blueprint, unreal.AnimBlueprint):
            raise RuntimeError(f'The existing Dwarf retarget graph is required for {code}')
        before = describe_graph(blueprint)
        expected_retargeter = f'{RETARGET_FOLDER}/IKR_Manny_To_{code}.IKR_Manny_To_{code}'
        if before['retargeter'] != expected_retargeter or not before['skeleton'].endswith(f'.SKEL_{code}'):
            raise RuntimeError(f'Active Dwarf model binding differs from the verified graph for {code}')

        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, native.static_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        generated_class = blueprint.generated_class()
        if not generated_class or not isinstance(unreal.get_default_object(generated_class), native):
            raise RuntimeError(f'Could not compile the roll support parent in {code}')
        if before != describe_graph(blueprint):
            raise RuntimeError(f'Retarget graph, skeleton, or binding changed in {code}')
        to_save.append((blueprint, before))

    for blueprint, before in to_save:
        if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
            raise RuntimeError(f'Could not save {blueprint.get_path_name()}')
        print(f'DWARF_ROLL_RETARGET {blueprint.get_path_name()} {json.dumps(before, sort_keys=True)}')
    print('RESULT=DWARF_ROLL_RETARGET_CONFIGURED')


if __name__ == '__main__':
    main()
